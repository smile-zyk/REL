#include "evaluator.h"

#include "data_array.h"
#include "data_series.h"
#include "measurement.h"
#include "parser/parser.h"
#include "scanner/scanner.h"
#include "unit.h"

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
    , result_(Value::Null())
{}

// =========================================================================
//  Entry point
// =========================================================================

Value Evaluator::Evaluate(const Expr& expr)
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
    result_ = Value::Null();
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
        result_ = Value::Integer(static_cast<int>(base_val));
    else if (expr.kind == NumberKind::Imaginary)
        result_ = Value(xdataset::Measurement(std::complex<double>(0.0, base_val)));
    else
        result_ = Value::Real(base_val);
}

// =========================================================================
//  visit_string
// =========================================================================

void Evaluator::visit_string(const StringExpr& expr)
{
    result_ = Value::String(expr.value);
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
        Value v = Evaluate(*item);
        if (v.is_null()) { result_ = Value::Null(); return; }
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
        result_ = Value(std::make_shared<DataArray>(Combine(rows)));
    }
}

// =========================================================================
//  visit_matrix — {expr_list}, Measurement or DataArray
// =========================================================================

void Evaluator::visit_matrix(const MatrixExpr& expr)
{
    using namespace xdataset;

    if (expr.items.empty()) { result_ = Value::Null(); return; }

    // Evaluate all items once.
    std::vector<Value> item_vals;
    bool has_data_array = false;
    for (const auto& item : expr.items)
    {
        Value v = Evaluate(*item);
        if (v.is_null()) { result_ = Value::Null(); return; }
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
//  apply_unary — dispatch to xdataset unary operators
// =========================================================================

Value Evaluator::apply_unary(TokenType op, const Value& operand)
{
    using namespace xdataset;

    if (operand.is_measurement())
    {
        const Measurement& m = operand.as_measurement();
        switch (op)
        {
            case TokenType::OP_SUB:  return Value(-m);
            case TokenType::OP_LNOT:
            case TokenType::KW_NOT:  return Value(!m);
            case TokenType::OP_BNOT: return Value(~m);
            default: return Value::Null();
        }
    }
    else if (operand.is_data_array())
    {
        const DataArray& da = operand.as_data_array();
        switch (op)
        {
            case TokenType::OP_SUB:  return Value(std::make_shared<DataArray>(-da));
            case TokenType::OP_LNOT:
            case TokenType::KW_NOT:  return Value(std::make_shared<DataArray>(!da));
            case TokenType::OP_BNOT: return Value(std::make_shared<DataArray>(~da));
            default: return Value::Null();
        }
    }
    return Value::Null();
}

// =========================================================================
//  apply_binary — dispatch to xdataset binary operators
// =========================================================================

#define REL_BINARY_OP(OpToken, OpFunc)                                         \
    case TokenType::OpToken:                                                   \
        if (lhs.is_measurement() && rhs.is_measurement())                      \
            return Value(OpFunc(lhs.as_measurement(), rhs.as_measurement())); \
        if (lhs.is_data_array() && rhs.is_measurement())                       \
            return Value(std::make_shared<DataArray>(                          \
                OpFunc(lhs.as_data_array(), rhs.as_measurement())));           \
        if (lhs.is_measurement() && rhs.is_data_array())                       \
            return Value(std::make_shared<DataArray>(                          \
                OpFunc(lhs.as_measurement(), rhs.as_data_array())));           \
        return Value(std::make_shared<DataArray>(                              \
            OpFunc(lhs.as_data_array(), rhs.as_data_array())));

Value Evaluator::apply_binary(TokenType op, const Value& lhs, const Value& rhs)
{
    using namespace xdataset;

    switch (op)
    {
        REL_BINARY_OP(OP_ADD,  operator+ )
        REL_BINARY_OP(OP_SUB,  operator- )
        REL_BINARY_OP(OP_MUL,  operator* )
        REL_BINARY_OP(OP_DIV,  operator/ )
        REL_BINARY_OP(OP_MOD,  operator% )
        REL_BINARY_OP(OP_SHL,  operator<<)
        REL_BINARY_OP(OP_SHR,  operator>>)
        REL_BINARY_OP(OP_LT,   operator< )
        REL_BINARY_OP(OP_LE,   operator<=)
        REL_BINARY_OP(OP_GT,   operator> )
        REL_BINARY_OP(OP_GE,   operator>=)
        REL_BINARY_OP(OP_EQ,   operator==)
        REL_BINARY_OP(OP_NE,   operator!=)
        REL_BINARY_OP(KW_EQUALS,    operator==)
        REL_BINARY_OP(KW_NOTEQUALS, operator!=)
        REL_BINARY_OP(OP_BAND,  operator& )
        REL_BINARY_OP(OP_BXOR,  operator^ )
        REL_BINARY_OP(OP_BOR,   operator| )
        REL_BINARY_OP(OP_POW,   pow)
        default:
            return Value::Null();
    }
}

#undef REL_BINARY_OP

// =========================================================================
//  apply_logical — short-circuit &&/AND/||/OR
// =========================================================================

// Helpers for logical truthiness: null or zero → false, everything else → true.
static bool is_truthy(const xdataset::Measurement& m)
{
    if (m.data_type() == xdataset::DataType::kString) return true;
    if (m.data_kind() != xdataset::DataKind::kScalar)  return true;
    if (m.data_type() == xdataset::DataType::kInteger)  return m.as_scalar<int>() != 0;
    if (m.data_type() == xdataset::DataType::kReal)     return m.as_scalar<double>() != 0.0;
    return true;  // Complex: always truthy
}

static bool is_truthy(const xdataset::DataArray& /*da*/) { return true; }

static bool is_truthy(const Value& v)
{
    if (v.is_null())  return false;
    if (v.is_data_array()) return true;
    return is_truthy(v.as_measurement());
}

Value Evaluator::apply_logical(TokenType op, const LogicalExpr& expr)
{
    Value lhs = Evaluate(*expr.left);
    if (lhs.is_null()) return Value::Null();

    bool is_and = (op == TokenType::OP_LAND || op == TokenType::KW_AND);
    bool lhsT = is_truthy(lhs);

    if (is_and && !lhsT)
        return Value::BooleanValue(false);
    if (!is_and && lhsT)
        return Value::BooleanValue(true);

    Value rhs = Evaluate(*expr.right);
    if (rhs.is_null()) return Value::Null();
    bool rhsT = is_truthy(rhs);

    return Value::BooleanValue(rhsT);
}

// =========================================================================
//  visit_unary
// =========================================================================

void Evaluator::visit_unary(const UnaryExpr& expr)
{
    Value operand = Evaluate(*expr.operand);
    if (operand.is_null()) { result_ = Value::Null(); return; }
    result_ = apply_unary(expr.op, operand);
}

// =========================================================================
//  visit_binary
// =========================================================================

void Evaluator::visit_binary(const BinaryExpr& expr)
{
    Value lhs = Evaluate(*expr.left);
    Value rhs = Evaluate(*expr.right);
    if (lhs.is_null() || rhs.is_null()) { result_ = Value::Null(); return; }
    result_ = apply_binary(expr.op, lhs, rhs);
}

// =========================================================================
//  visit_logical — short-circuit semantics
// =========================================================================

void Evaluator::visit_logical(const LogicalExpr& expr)
{
    result_ = apply_logical(expr.op, expr);
}

// =========================================================================
//  visit_grouping
// =========================================================================

void Evaluator::visit_grouping(const GroupingExpr& expr)
{
    result_ = Evaluate(*expr.inner);
}

// =========================================================================
//  visit_reference — resolve identifier / dotted path via Environment
// =========================================================================

void Evaluator::visit_reference(const ReferenceExpr& expr)
{
    try
    {
        result_ = env_.ResolveReference(expr.segments);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            std::string("line ") + std::to_string(expr.line) +
            ", column " + std::to_string(expr.column) + ": " + e.what());
    }
}

// =========================================================================
//  Stubs
// =========================================================================

void Evaluator::visit_conditional(const ConditionalExpr&) { result_ = Value::Null(); }
void Evaluator::visit_if(const IfExpr&)                 { result_ = Value::Null(); }
void Evaluator::visit_call(const CallExpr&)             { result_ = Value::Null(); }
void Evaluator::visit_index(const IndexExpr&)           { result_ = Value::Null(); }
void Evaluator::visit_range(const RangeExpr&)           { result_ = Value::Null(); }
void Evaluator::visit_null_range(const NullRangeExpr&)  { result_ = Value::Null(); }

} // namespace rel
