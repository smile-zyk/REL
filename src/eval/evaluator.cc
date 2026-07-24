#include "evaluator.h"

#include "measurement.h"
#include "unit.h"

#include <complex>
#include <cstdlib>
#include <string>

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
    // Imaginary: strip trailing 'i', parse the numeric part
    std::string num = lexeme;
    if (!num.empty() && num.back() == 'i')
        num.pop_back();

    if (num.empty())
        return 0.0;

    char* end = nullptr;

    // Hex / octal: integer-only, use strtol
    if (radix == 16 || radix == 8)
        return static_cast<double>(std::strtol(num.c_str(), &end, radix));

    // Decimal integer or real
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
//  visit_number (fully implemented)
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

    // Plain number, no suffix.
    if (expr.kind == NumberKind::Integer)
        result_ = Value::integer(static_cast<int>(base_val));
    else if (expr.kind == NumberKind::Imaginary)
        result_ = Value(xdataset::Measurement(std::complex<double>(0.0, base_val)));
    else
        result_ = Value::real(base_val);
}

// =========================================================================
//  visit_string (fully implemented)
// =========================================================================

void Evaluator::visit_string(const StringExpr& expr)
{
    result_ = Value::string_value(expr.value);
}

// =========================================================================
//  Stubs — return null, implemented in later steps
// =========================================================================

void Evaluator::visit_reference(const ReferenceExpr& /*expr*/)
{
    result_ = Value::null_value();
}

void Evaluator::visit_unary(const UnaryExpr& /*expr*/)
{
    result_ = Value::null_value();
}

void Evaluator::visit_binary(const BinaryExpr& /*expr*/)
{
    result_ = Value::null_value();
}

void Evaluator::visit_logical(const LogicalExpr& /*expr*/)
{
    result_ = Value::null_value();
}

void Evaluator::visit_conditional(const ConditionalExpr& /*expr*/)
{
    result_ = Value::null_value();
}

void Evaluator::visit_if(const IfExpr& /*expr*/)
{
    result_ = Value::null_value();
}

void Evaluator::visit_call(const CallExpr& /*expr*/)
{
    result_ = Value::null_value();
}

void Evaluator::visit_index(const IndexExpr& /*expr*/)
{
    result_ = Value::null_value();
}

void Evaluator::visit_grouping(const GroupingExpr& /*expr*/)
{
    result_ = Value::null_value();
}

void Evaluator::visit_sweep(const SweepExpr& /*expr*/)
{
    result_ = Value::null_value();
}

void Evaluator::visit_matrix(const MatrixExpr& /*expr*/)
{
    result_ = Value::null_value();
}

void Evaluator::visit_range(const RangeExpr& /*expr*/)
{
    result_ = Value::null_value();
}

void Evaluator::visit_null_range(const NullRangeExpr& /*expr*/)
{
    result_ = Value::null_value();
}

} // namespace rel
