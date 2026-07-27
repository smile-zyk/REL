#include "evaluator.h"

#include "data_array.h"
#include "data_series.h"
#include "measurement.h"
#include "unit.h"

#include <Eigen/Dense>

#include <complex>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

namespace rel {

// =========================================================================
//  Construction
// =========================================================================

Evaluator::Evaluator(Environment& env)
    : env_(env)
    , result_(Value::null_value())
{}

// =========================================================================
//  Entry point
// =========================================================================

Value Evaluator::evaluate(const Expr& expr)
{
    expr.accept(*this);
    return std::move(result_);
}

// =========================================================================
//  parse_base — convert lexeme according to radix
// =========================================================================

double Evaluator::parse_base(const std::string& lexeme, int radix)
{
    std::string num = lexeme;
    if (!num.empty() && num.back() == 'i')
        num.pop_back();

    if (num.empty())
        return 0.0;

    char* end = nullptr;
    if (radix == 16 || radix == 8)
        return static_cast<double>(std::strtol(num.c_str(), &end, radix));

    return std::strtod(num.c_str(), &end);
}

// =========================================================================
//  visit_null
// =========================================================================

void Evaluator::visit_null(const NullExpr& /*expr*/)
{
    result_ = Value::null_value();
}

// =========================================================================
//  visit_number
// =========================================================================

void Evaluator::visit_number(const NumberExpr& expr)
{
    double base_val = parse_base(expr.base_lexeme, expr.radix);
    const std::string& sfx = expr.suffix;

    if (!sfx.empty())
    {
        xdataset::Unit u = xdataset::Unit::parse(sfx);
        if (expr.kind == NumberKind::Integer)
            result_ = Value(xdataset::Measurement(
                static_cast<int>(base_val), std::move(u)));
        else if (expr.kind == NumberKind::Imaginary)
            result_ = Value(xdataset::Measurement(
                std::complex<double>(0.0, base_val), std::move(u)));
        else
            result_ = Value(xdataset::Measurement(base_val, std::move(u)));
        return;
    }

    if (expr.kind == NumberKind::Integer)
        result_ = Value::integer(static_cast<int>(base_val));
    else if (expr.kind == NumberKind::Imaginary)
        result_ = Value(xdataset::Measurement(std::complex<double>(0.0, base_val)));
    else
        result_ = Value::real(base_val);
}

// =========================================================================
//  visit_string
// =========================================================================

void Evaluator::visit_string(const StringExpr& expr)
{
    result_ = Value::string_value(expr.value);
}

// =========================================================================
//  visit_sweep — [expr_list], always produces Independent DataArray
// =========================================================================

void Evaluator::visit_sweep(const SweepExpr& expr)
{
    using namespace xdataset;

    if (expr.items.empty())
    {
        DataSeries ds(DataKind::kScalar, DataType::kReal, {});
        result_ = Value(std::make_shared<DataArray>(DataArray::CreateIndependent(ds)));
        return;
    }

    // Evaluate and flatten all items to measurements.
    std::vector<Measurement> rows;
    for (const auto& item : expr.items)
    {
        Value v = evaluate(*item);
        if (v.is_null()) { result_ = Value::null_value(); return; }
        if (v.is_measurement())
        {
            rows.push_back(v.as_measurement());
        }
        else
        {
            const DataArray& da = v.as_data_array();
            const DataSeries& src = da.data();
            for (std::size_t i = 0; i < src.size(); ++i)
                rows.push_back(src.measurement_at(i));
        }
    }

    if (rows.empty()) { result_ = Value::null_value(); return; }

    // Validate uniformity.
    const DataKind  kind  = rows[0].data_kind();
    const DataType  dtype = rows[0].data_type();
    const std::vector<Index>& shape = rows[0].shape();
    for (std::size_t i = 1; i < rows.size(); ++i)
    {
        if (rows[i].data_kind() != kind || rows[i].data_type() != dtype ||
            rows[i].shape() != shape)
            throw std::runtime_error("sweep: all items must have same kind/dtype/shape");
    }

    DataSeries ds(kind, dtype, shape);
    ds.resize(0);
    for (auto& m : rows)
        ds.append(m);

    result_ = Value(std::make_shared<DataArray>(DataArray::CreateIndependent(ds)));
}

// =========================================================================
//  visit_matrix — {expr_list}, Measurement or DataArray
// =========================================================================

void Evaluator::visit_matrix(const MatrixExpr& expr)
{
    using namespace xdataset;

    if (expr.items.empty()) { result_ = Value::null_value(); return; }

    struct ItemInfo {
        Measurement first_row;
        std::size_t row_count;
        bool        is_data_array;
        Value       value;
    };
    std::vector<ItemInfo> infos;

    for (const auto& item : expr.items)
    {
        Value v = evaluate(*item);
        if (v.is_null()) { result_ = Value::null_value(); return; }

        ItemInfo info;
        info.value = v;
        if (v.is_measurement())
        {
            info.first_row     = v.as_measurement();
            info.row_count     = 1;
            info.is_data_array = false;
        }
        else
        {
            const DataArray& da = v.as_data_array();
            info.first_row     = da.data().measurement_at(0);
            info.row_count     = da.data().size();
            info.is_data_array = true;
        }
        infos.push_back(info);
    }

    // Validate uniformity.
    const DataKind  kind  = infos[0].first_row.data_kind();
    const DataType  dtype = infos[0].first_row.data_type();
    const std::vector<Index>& shape = infos[0].first_row.shape();
    std::size_t max_rows = 0;
    for (const auto& info : infos)
    {
        if (info.first_row.data_kind() != kind ||
            info.first_row.data_type() != dtype ||
            info.first_row.shape()     != shape)
            throw std::runtime_error("matrix: all items must have same kind/dtype/shape");
        if (info.row_count > max_rows) max_rows = info.row_count;
    }
    for (const auto& info : infos)
    {
        if (info.row_count != 1 && info.row_count != max_rows)
            throw std::runtime_error("matrix: row counts must be 1 or equal");
    }

    bool has_data_array = false;
    for (const auto& info : infos)
        if (info.is_data_array) { has_data_array = true; break; }

    if (!has_data_array)
    {
        // Pure Measurement: shape promotion only when N > 1.
        // Single item: return as-is.
        if (infos.size() == 1)
        {
            result_ = Value(infos[0].first_row);
            return;
        }

        if (kind == DataKind::kScalar)
        {
            // Scalar × N → Vector(N).  Handle Integer vs Real dtype.
            Eigen::VectorXd vec(static_cast<Eigen::Index>(infos.size()));
            for (std::size_t i = 0; i < infos.size(); ++i)
            {
                if (dtype == DataType::kInteger)
                    vec[static_cast<Eigen::Index>(i)] = static_cast<double>(
                        infos[i].first_row.as_scalar<int>());
                else
                    vec[static_cast<Eigen::Index>(i)] = infos[i].first_row.as_scalar<double>();
            }
            result_ = Value(Measurement::Vector(vec));
            if (infos[0].first_row.unit().has_dimension())
                result_.as_measurement().set_unit(infos[0].first_row.unit());
        }
        else if (kind == DataKind::kVector)
        {
            // Vector(w) × N → Matrix(N, w)
            Index w = shape[0];
            Index n = static_cast<Index>(infos.size());
            Eigen::MatrixXd mat(n, w);
            for (Index i = 0; i < n; ++i)
            {
                auto v = infos[static_cast<std::size_t>(i)].first_row.as_vector<double>();
                mat.row(i) = v;
            }
            result_ = Value(Measurement::Matrix(mat));
            if (infos[0].first_row.unit().has_dimension())
                result_.as_measurement().set_unit(infos[0].first_row.unit());
        }
        else
        {
            throw std::runtime_error("matrix: cannot combine Matrix items (max rank is 2)");
        }
        return;
    }

    // DataArray path.
    DataSeries ds(kind, dtype, shape);
    ds.resize(0);
    for (const auto& info : infos)
    {
        if (info.is_data_array)
        {
            const DataArray& da = info.value.as_data_array();
            const DataSeries& src = da.data();
            for (std::size_t i = 0; i < src.size(); ++i)
                ds.append(src.measurement_at(i));
        }
        else
        {
            for (std::size_t i = 0; i < max_rows; ++i)
                ds.append(info.first_row);
        }
    }
    result_ = Value(std::make_shared<DataArray>(DataArray::CreateIndependent(ds)));
}

// =========================================================================
//  Stubs
// =========================================================================

void Evaluator::visit_reference(const ReferenceExpr&)  { result_ = Value::null_value(); }
void Evaluator::visit_unary(const UnaryExpr&)           { result_ = Value::null_value(); }
void Evaluator::visit_binary(const BinaryExpr&)         { result_ = Value::null_value(); }
void Evaluator::visit_logical(const LogicalExpr&)       { result_ = Value::null_value(); }
void Evaluator::visit_conditional(const ConditionalExpr&) { result_ = Value::null_value(); }
void Evaluator::visit_if(const IfExpr&)                 { result_ = Value::null_value(); }
void Evaluator::visit_call(const CallExpr&)             { result_ = Value::null_value(); }
void Evaluator::visit_index(const IndexExpr&)           { result_ = Value::null_value(); }
void Evaluator::visit_grouping(const GroupingExpr&)     { result_ = Value::null_value(); }
void Evaluator::visit_range(const RangeExpr&)           { result_ = Value::null_value(); }
void Evaluator::visit_null_range(const NullRangeExpr&)  { result_ = Value::null_value(); }

} // namespace rel
