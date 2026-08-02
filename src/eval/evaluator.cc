#include "evaluator.h"

#include "multi_index_selector.h"
#include "operation.h"
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
    , result_(xdataset::Value())
{}

// =========================================================================
//  Entry point
// =========================================================================

xdataset::Value Evaluator::Evaluate(const Expr& expr)
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
//  visit_boolean
// =========================================================================

void Evaluator::visit_boolean(const BooleanExpr& expr)
{
    result_ = xdataset::Value(xdataset::Measurement::Boolean(expr.value));
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
            result_ = xdataset::Value(xdataset::Measurement(static_cast<int>(base_val), std::move(u)));
        else if (expr.kind == NumberKind::Imaginary)
            result_ = xdataset::Value(xdataset::Measurement(
                std::complex<double>(0.0, base_val), std::move(u)));
        else
            result_ = xdataset::Value(xdataset::Measurement(base_val, std::move(u)));
        return;
    }

    if (expr.kind == NumberKind::Integer)
        result_ = xdataset::Value(xdataset::Measurement::Integer(static_cast<int>(base_val)));
    else if (expr.kind == NumberKind::Imaginary)
        result_ = xdataset::Value(xdataset::Measurement(std::complex<double>(0.0, base_val)));
    else
        result_ = xdataset::Value(xdataset::Measurement::Real(base_val));
}

// =========================================================================
//  visit_string
// =========================================================================

void Evaluator::visit_string(const StringExpr& expr)
{
    result_ = xdataset::Value(xdataset::Measurement::String(expr.value));
}

// =========================================================================
//  apply_unary — delegate to xdataset::Value operators
// =========================================================================

xdataset::Value Evaluator::apply_unary(TokenType op, const xdataset::Value& operand)
{
    switch (op)
    {
        case TokenType::OP_SUB:  return -operand;
        case TokenType::OP_LNOT:
        case TokenType::KW_NOT:  return !operand;
        case TokenType::OP_BNOT: return ~operand;
        default: return xdataset::Value();
    }
}

// =========================================================================
//  apply_binary — delegate to xdataset::Value operators
// =========================================================================

xdataset::Value Evaluator::apply_binary(TokenType op, const xdataset::Value& lhs, const xdataset::Value& rhs)
{
    switch (op)
    {
        case TokenType::OP_ADD:  return lhs + rhs;
        case TokenType::OP_SUB:  return lhs - rhs;
        case TokenType::OP_MUL:  return lhs * rhs;
        case TokenType::OP_DIV:  return lhs / rhs;
        case TokenType::OP_MOD:  return lhs % rhs;
        case TokenType::OP_SHL:  return lhs << rhs;
        case TokenType::OP_SHR:  return lhs >> rhs;
        case TokenType::OP_LT:   return lhs < rhs;
        case TokenType::OP_LE:   return lhs <= rhs;
        case TokenType::OP_GT:   return lhs > rhs;
        case TokenType::OP_GE:   return lhs >= rhs;
        case TokenType::OP_EQ:
        case TokenType::KW_EQUALS:    return lhs == rhs;
        case TokenType::OP_NE:
        case TokenType::KW_NOTEQUALS: return lhs != rhs;
        case TokenType::OP_BAND: return lhs & rhs;
        case TokenType::OP_BXOR: return lhs ^ rhs;
        case TokenType::OP_BOR:  return lhs | rhs;
        case TokenType::OP_POW:  return xdataset::pow(lhs, rhs);
        default: return xdataset::Value();
    }
}

// =========================================================================
//  apply_logical — delegate to xdataset::Value operators
// =========================================================================

xdataset::Value Evaluator::apply_logical(TokenType op, const LogicalExpr& expr)
{
    xdataset::Value lhs = Evaluate(*expr.left);
    xdataset::Value rhs = Evaluate(*expr.right);

    if (op == TokenType::OP_LAND || op == TokenType::KW_AND)
        return lhs && rhs;
    else
        return lhs || rhs;
}

// =========================================================================
//  visit_unary
// =========================================================================

void Evaluator::visit_unary(const UnaryExpr& expr)
{
    xdataset::Value operand = Evaluate(*expr.operand);
    result_ = apply_unary(expr.op, operand);
}

// =========================================================================
//  visit_binary
// =========================================================================

void Evaluator::visit_binary(const BinaryExpr& expr)
{
    xdataset::Value lhs = Evaluate(*expr.left);
    xdataset::Value rhs = Evaluate(*expr.right);
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
//  visit_conditional — ternary ?:
// =========================================================================

void Evaluator::visit_conditional(const ConditionalExpr& expr)
{
    xdataset::Value cond   = Evaluate(*expr.condition);
    xdataset::Value then_v = Evaluate(*expr.then_branch);
    xdataset::Value else_v = Evaluate(*expr.else_branch);
    result_ = xdataset::OperationConditional(cond, then_v, else_v);
}

// =========================================================================
//  visit_if — if(…)then…elseif(…)then…else…
// =========================================================================

void Evaluator::visit_if(const IfExpr& expr)
{
    std::vector<xdataset::Value> operands;
    operands.reserve(expr.branches.size() * 2 + 1);

    for (const auto& br : expr.branches)
    {
        operands.push_back(Evaluate(*br.condition));
        operands.push_back(Evaluate(*br.value));
    }
    operands.push_back(Evaluate(*expr.else_value));

    result_ = xdataset::OperationIf(operands);
}

// =========================================================================
//  Sequence helpers
// =========================================================================
//  RangeExpr / NullRangeExpr only appear inside:
//    a(i, j)  — matrix index (via visit_call)
//    a[i, j]  — sweep index (via visit_index)
//    [items]  — sweep generator
//    {items}  — matrix generator
//  They are never top-level expressions; visit_range / visit_null_range
//  remain dead code (parser guarantees this).

// ---- shared: evaluate a sub-expression into a scalar double ----

static double eval_scalar_num(rel::Evaluator& eval, const rel::Expr& arg)
{
    xdataset::Value v = eval.Evaluate(arg);
    if (!v.is_measurement() || !v.is_scalar())
        throw std::runtime_error("range/index operand must be a scalar number");
    const auto& m = v.as_measurement();
    if (m.data_type() == xdataset::DataType::kInteger)
        return static_cast<double>(m.as_scalar<int>());
    if (m.data_type() == xdataset::DataType::kReal)
        return m.as_scalar<double>();
    throw std::runtime_error("range/index operand must be numeric");
}

// ---- index selectors: AST → MultiIndexSelector (used by visit_call / visit_index) ----
//    matrix index a(i, j): 1-based → 0-based
//    sweep  index a[i, j]: 0-based (no conversion)

static xdataset::MultiIndexSelector
make_selector(rel::Evaluator& eval, const rel::ExprPtr& arg, bool one_based)
{
    if (dynamic_cast<const rel::NullRangeExpr*>(arg.get()))
        return xdataset::MultiIndexSelector::Any();

    if (auto* r = dynamic_cast<const rel::RangeExpr*>(arg.get()))
    {
        double start = eval_scalar_num(eval, *r->start);
        double step  = r->step ? eval_scalar_num(eval, *r->step) : 1.0;
        double stop  = eval_scalar_num(eval, *r->stop);

        if (step == 0.0)
            throw std::runtime_error("range step cannot be zero");
        if ((step > 0 && start > stop) || (step < 0 && start < stop))
            throw std::runtime_error("range direction mismatch");

        if (one_based) { start -= 1; stop -= 1; }

        if (start < 0)
            throw std::runtime_error("index out of range (negative)");

        std::vector<xdataset::Index> indices;
        if (step > 0)
            for (double v = start; v <= stop + 1e-12; v += step)
                indices.push_back(static_cast<xdataset::Index>(v));
        else
            for (double v = start; v >= stop - 1e-12; v += step)
                indices.push_back(static_cast<xdataset::Index>(v));

        return xdataset::MultiIndexSelector::In(indices);
    }

    double idx = eval_scalar_num(eval, *arg);
    if (one_based) idx -= 1;
    if (idx < 0)
        throw std::runtime_error("index out of range (negative)");
    return xdataset::MultiIndexSelector::Equal(
        static_cast<xdataset::Index>(idx));
}

// ---- sequence expansion: RangeExpr → Value items (used by visit_sweep / visit_matrix) ----
//    start/step/stop can be Integer or Real; output preserves the type.

static void expand_range(rel::Evaluator& eval,
                         const rel::RangeExpr& r,
                         std::vector<xdataset::Value>& out)
{
    // Check if all three sub-expressions are Integer-valued.
    auto is_int = [&](const rel::ExprPtr& e) {
        if (!e) return true;
        xdataset::Value v = eval.Evaluate(*e);
        return v.is_measurement() && v.is_scalar() &&
               v.as_measurement().data_type() == xdataset::DataType::kInteger;
    };

    double start = eval_scalar_num(eval, *r.start);
    double step  = r.step ? eval_scalar_num(eval, *r.step) : 1.0;
    double stop  = eval_scalar_num(eval, *r.stop);

    if (step == 0.0)
        throw std::runtime_error("range step cannot be zero");
    if ((step > 0 && start > stop) || (step < 0 && start < stop))
        throw std::runtime_error("range direction mismatch");

    bool all_int = is_int(r.start) && is_int(r.step) && is_int(r.stop);

    auto make_val = [all_int](double x) {
        return all_int
            ? xdataset::Value(xdataset::Measurement::Integer(static_cast<int>(x)))
            : xdataset::Value(xdataset::Measurement::Real(x));
    };

    if (step > 0)
        for (double v = start; v <= stop + 1e-12; v += step)
            out.push_back(make_val(v));
    else
        for (double v = start; v >= stop - 1e-12; v += step)
            out.push_back(make_val(v));
}

/// Expand one item: RangeExpr → multiple Values; everything else → single Value.
static void expand_item(rel::Evaluator& eval,
                        const rel::ExprPtr& item,
                        std::vector<xdataset::Value>& out)
{
    if (auto* r = dynamic_cast<const rel::RangeExpr*>(item.get()))
    {
        expand_range(eval, *r, out);
        return;
    }
    // NullRangeExpr is illegal inside [] / {}
    if (dynamic_cast<const rel::NullRangeExpr*>(item.get()))
        throw std::runtime_error("bare '::' is not allowed inside a generator");

    out.push_back(eval.Evaluate(*item));
}

// =========================================================================
//  visit_sweep — [expr_list], expands RangeExpr items, uses OperationSweep
// =========================================================================

void Evaluator::visit_sweep(const SweepExpr& expr)
{
    std::vector<xdataset::Value> items;
    items.reserve(expr.items.size());
    for (const auto& item : expr.items)
        expand_item(*this, item, items);

    if (items.empty())
    {
        result_ = xdataset::Value();
        return;
    }

    result_ = xdataset::OperationSweep(items);
}

// =========================================================================
//  visit_matrix — {expr_list}, expands RangeExpr items, uses OperationMatrix
// =========================================================================

void Evaluator::visit_matrix(const MatrixExpr& expr)
{
    std::vector<xdataset::Value> items;
    items.reserve(expr.items.size());
    for (const auto& item : expr.items)
        expand_item(*this, item, items);

    if (items.empty())
    {
        result_ = xdataset::Value();
        return;
    }

    result_ = xdataset::OperationMatrix(items);
}

// =========================================================================
//  visit_call — function call or matrix index a(i, j, ...)
// =========================================================================

void Evaluator::visit_call(const CallExpr& expr)
{
    xdataset::Value obj = Evaluate(*expr.callee);

    // Matrix index: non-scalar Measurement or DataArray → at()
    bool is_matrix_index =
        obj.is_data_array() ||
        (obj.is_measurement() &&
         obj.as_measurement().data_kind() != xdataset::DataKind::kScalar);

    if (is_matrix_index)
    {
        std::vector<xdataset::MultiIndexSelector> selectors;
        selectors.reserve(expr.args.size());
        for (const auto& arg : expr.args)
            selectors.push_back(make_selector(*this, arg, /*one_based=*/true));

        if (obj.is_measurement())
            result_ = xdataset::Value(obj.as_measurement().at(selectors));
        else
            result_ = xdataset::Value(obj.as_data_array().at(selectors));

        return;
    }

    // TODO: function call
    throw std::runtime_error("function calls are not yet implemented");
}

// =========================================================================
//  visit_index — sweep index a[i, j, ...] via DataArray::select()
// =========================================================================

void Evaluator::visit_index(const IndexExpr& expr)
{
    xdataset::Value obj = Evaluate(*expr.object);

    if (!obj.is_data_array())
        throw std::runtime_error("[] indexing requires a DataArray");

    std::vector<xdataset::MultiIndexSelector> selectors;
    selectors.reserve(expr.indices.size());
    for (const auto& idx : expr.indices)
        selectors.push_back(make_selector(*this, idx, /*one_based=*/false));

    xdataset::DataArray da = obj.as_data_array().select(selectors);

    // Unwrap single-row, single-cell Independent DataArray → Measurement.
    if (da.data_kind() == xdataset::DataArrayKind::kIndependent &&
        da.datas().size() == 1 &&
        da.data().size() == 1 &&
        da.data().element_count() == 1)
    {
        result_ = xdataset::Value(da.data().measurement_at(0));
    }
    else
    {
        result_ = xdataset::Value(std::move(da));
    }
}

// =========================================================================
//  visit_range / visit_null_range — dead code; these nodes are consumed
//  inside make_selector / expand_item above, never reached as top-level.
// =========================================================================

void Evaluator::visit_range(const RangeExpr&)           {}
void Evaluator::visit_null_range(const NullRangeExpr&)  {}

} // namespace rel
