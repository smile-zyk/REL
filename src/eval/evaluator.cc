#include "evaluator.h"

#include "data_array.h"
#include "data_series.h"
#include "measurement.h"
#include "unit.h"

#include <complex>
#include <cstdlib>
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

    // Combine handles: dtype promotion, scalar broadcast, shape
    // validation, and Independent DataArray construction.
    // Empty sweep: Combine throws, so handle it explicitly.
    if (rows.empty())
    {
        DataSeries ds(DataKind::kScalar, DataType::kReal, {});
        result_ = Value(std::make_shared<DataArray>(DataArray::CreateIndependent(ds)));
    }
    else
    {
        // Combine yields a Dependent DataArray (no multi_dimension_spec).
        // Re-wrap as Independent so GetOrCreateDataFrame() works.
        DataArray combined = Combine(rows);
        DataSeries ds = combined.data();
        result_ = Value(std::make_shared<DataArray>(DataArray::CreateIndependent(ds)));
    }
}

// =========================================================================
//  visit_matrix — {expr_list}, Measurement or DataArray
// =========================================================================

void Evaluator::visit_matrix(const MatrixExpr& expr)
{
    using namespace xdataset;

    if (expr.items.empty()) { result_ = Value::null_value(); return; }

    // Evaluate all items once.
    std::vector<Value> item_vals;
    bool has_data_array = false;
    for (const auto& item : expr.items)
    {
        Value v = evaluate(*item);
        if (v.is_null()) { result_ = Value::null_value(); return; }
        if (v.is_data_array()) has_data_array = true;
        item_vals.push_back(v);
    }

    if (!has_data_array)
    {
        // Pure Measurement: single item returns as-is.
        if (item_vals.size() == 1)
        {
            result_ = item_vals[0];
            return;
        }

        std::vector<Measurement> values;
        for (auto& v : item_vals)
            values.push_back(v.as_measurement());
        result_ = Value(Concat(values));
        return;
    }

    // DataArray path: single DataArray or Measurement returns as-is.
    if (item_vals.size() == 1)
    {
        result_ = item_vals[0];
        return;
    }

    // Mixed or pure DataArray: promote Measurements to 1-row DataArrays.
    std::vector<DataArray> values;
    for (auto& v : item_vals)
    {
        if (v.is_measurement())
        {
            const Measurement& m = v.as_measurement();
            DataSeries ds(m.data_kind(), m.data_type(), m.shape());
            ds.set_unit(m.unit());
            ds.append(m);
            values.push_back(DataArray::CreateIndependent(ds));
        }
        else
        {
            values.push_back(v.as_data_array());
        }
    }

    result_ = Value(std::make_shared<DataArray>(Concat(values)));
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
