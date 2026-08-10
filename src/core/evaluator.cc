#include "evaluator.h"

#include "multi_index_selector.h"
#include "operation/operator.h"
#include "unit.h"

#include <complex>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace rel {

// =========================================================================
//  Construction
// =========================================================================

Evaluator::Evaluator(Environment& env)
    : env_(env)
    , result_(rel::Value())
{}

// =========================================================================
//  Entry point
// =========================================================================

rel::Value Evaluator::Evaluate(const Expr& expr)
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
    result_ = rel::Value::Boolean(expr.value);
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
            result_ = rel::Value::Integer(static_cast<int>(base_val), std::move(u));
        else if (expr.kind == NumberKind::Imaginary)
            result_ = rel::Value::Complex(std::complex<double>(0.0, base_val), std::move(u));
        else
            result_ = rel::Value::Real(base_val, std::move(u));
        return;
    }

    if (expr.kind == NumberKind::Integer)
        result_ = rel::Value::Integer(static_cast<int>(base_val));
    else if (expr.kind == NumberKind::Imaginary)
        result_ = rel::Value::Complex(std::complex<double>(0.0, base_val));
    else
        result_ = rel::Value::Real(base_val);
}

// =========================================================================
//  visit_string
// =========================================================================

void Evaluator::visit_string(const StringExpr& expr)
{
    result_ = rel::Value::String(expr.value);
}

// =========================================================================
//  apply_unary — delegate to rel::Value operators
// =========================================================================

rel::Value Evaluator::apply_unary(TokenType op, const rel::Value& operand)
{
    switch (op)
    {
        case TokenType::OP_SUB:  return -operand;
        case TokenType::OP_LNOT:
        case TokenType::KW_NOT:  return !operand;
        case TokenType::OP_BNOT: return ~operand;
        default: return rel::Value();
    }
}

// =========================================================================
//  apply_binary — delegate to rel::Value operators
// =========================================================================

rel::Value Evaluator::apply_binary(TokenType op, const rel::Value& lhs, const rel::Value& rhs)
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
        case TokenType::OP_POW:  return pow(lhs, rhs);
        default: return rel::Value();
    }
}

// =========================================================================
//  apply_logical — delegate to rel::Value operators
// =========================================================================

rel::Value Evaluator::apply_logical(TokenType op, const LogicalExpr& expr)
{
    rel::Value lhs = Evaluate(*expr.left);
    rel::Value rhs = Evaluate(*expr.right);

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
    rel::Value operand = Evaluate(*expr.operand);
    result_ = apply_unary(expr.op, operand);
}

// =========================================================================
//  visit_binary
// =========================================================================

void Evaluator::visit_binary(const BinaryExpr& expr)
{
    rel::Value lhs = Evaluate(*expr.left);
    rel::Value rhs = Evaluate(*expr.right);
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
        result_ = ResolveReference(expr.segments);
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(
            std::string("line ") + std::to_string(expr.line) +
            ", column " + std::to_string(expr.column) + ": " + e.what());
    }
}

// =========================================================================
//  ResolveReference — AST-aware reference resolution (formerly in Environment)
// =========================================================================

rel::Value Evaluator::ResolveReference(
    const std::vector<RefSegment>& segments) const
{
    if (segments.empty())
        return rel::Value();

    // ---- 1 segment: variable lookup ---------------------------------
    if (segments.size() == 1)
    {
        const std::string& name = segments[0].name;

        // 1a) User variable / builtin constant
        const rel::Value* c = env_.LookupVariableOrConstant(name);
        if (c)
            return *c;

        // 1b) Unique lookup in default dataset
        xdataset::Dataset* ds = Environment::DefaultDataset();
        if (ds && ds->HasUniqueDataArray(name))
        {
            return rel::Value(ds->GetDataArray(name));
        }

        throw std::runtime_error(
            "undefined identifier '" + name + "'");
    }

    // ---- ≥2 segments DDot: dataset..variable (may contain dots) ----
    if (segments.size() >= 2 && segments[1].sep == RefSeparator::DDot)
    {
        xdataset::Dataset* ds = Environment::FindDataset(segments[0].name);
        if (!ds)
        {
            throw std::runtime_error(
                "unknown Dataset '" + segments[0].name + "'");
        }

        // Join segments[1..end] with '.' as the variable name.
        //  e.g. ds..SRC1.i  →  var = "SRC1.i"
        std::ostringstream var_name;
        for (std::size_t i = 1; i < segments.size(); ++i)
        {
            if (i > 1) var_name << ".";
            var_name << segments[i].name;
        }

        return rel::Value(ds->GetDataArray(var_name.str()));
    }

    // ---- ≥2 segments Dot: path navigation ---------------------------
    {
        // Determine which Dataset to use.
        xdataset::Dataset* ds = Environment::DefaultDataset();
        std::size_t start = 0;

        xdataset::Dataset* explicit_ds =
            Environment::FindDataset(segments[0].name);
        if (explicit_ds)
        {
            ds = explicit_ds;
            start = 1;
        }

        if (!ds)
        {
            throw std::runtime_error(
                "no default Dataset set; cannot resolve '" +
                segments[0].name + "'");
        }

        if (segments.size() < start + 2)
        {
            throw std::runtime_error(
                "reference needs at least block.variable after path");
        }

        // Try each split point: segments[start..k-1] → block path,
        // segments[k..end] → variable name (joined with '.').
        // k = n-1: original behaviour (single-segment var, e.g. SP.Vout).
        // k = n-2: fallback for dotted vars (e.g. SP.SRC1.i).
        for (std::size_t k = segments.size() - 1;
             k >= segments.size() - 2 && k >= start + 1; --k)
        {
            // Build block path (segments[start..k-1] joined with '/')
            std::ostringstream block_path;
            for (std::size_t i = start; i < k; ++i)
            {
                if (i > start) block_path << "/";
                block_path << segments[i].name;
            }

            if (!ds->IsLeaf(block_path.str()))
                continue;

            // Build variable name (segments[k..end] joined with '.')
            std::ostringstream var_name;
            for (std::size_t i = k; i < segments.size(); ++i)
            {
                if (i > k) var_name << ".";
                var_name << segments[i].name;
            }

            try
            {
                return rel::Value(
                    ds->GetDataArray(block_path.str(), var_name.str()));
            }
            catch (const std::invalid_argument&) { /* try next split */ }
        }

        // All interpretations exhausted — report failure with the original
        // dotted form the user wrote.
        std::ostringstream err;
        err << "cannot resolve '";
        for (std::size_t i = start; i < segments.size(); ++i)
        {
            if (i > start) err << ".";
            err << segments[i].name;
        }
        err << "'";
        throw std::runtime_error(err.str());
    }
}

// =========================================================================
//  visit_conditional — ternary ?:
// =========================================================================

void Evaluator::visit_conditional(const ConditionalExpr& expr)
{
    rel::Value cond   = Evaluate(*expr.condition);
    rel::Value then_v = Evaluate(*expr.then_branch);
    rel::Value else_v = Evaluate(*expr.else_branch);
    result_ = rel::operation::OperationConditional(cond, then_v, else_v);
}

// =========================================================================
//  visit_if — if(…)then…elseif(…)then…else…
// =========================================================================

void Evaluator::visit_if(const IfExpr& expr)
{
    std::vector<rel::Value> operands;
    operands.reserve(expr.branches.size() * 2 + 1);

    for (const auto& br : expr.branches)
    {
        operands.push_back(Evaluate(*br.condition));
        operands.push_back(Evaluate(*br.value));
    }
    operands.push_back(Evaluate(*expr.else_value));

    result_ = rel::operation::OperationIf(operands);
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
    rel::Value v = eval.Evaluate(arg);
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
                         std::vector<rel::Value>& out)
{
    // Check if all three sub-expressions are Integer-valued.
    auto is_int = [&](const rel::ExprPtr& e) {
        if (!e) return true;
        rel::Value v = eval.Evaluate(*e);
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
            ? rel::Value::Integer(static_cast<int>(x))
            : rel::Value::Real(x);
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
                        std::vector<rel::Value>& out)
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
    std::vector<rel::Value> items;
    items.reserve(expr.items.size());
    for (const auto& item : expr.items)
        expand_item(*this, item, items);

    if (items.empty())
    {
        result_ = rel::Value();
        return;
    }

    result_ = rel::operation::OperationSweep(items);
}

// =========================================================================
//  visit_matrix — {expr_list}, expands RangeExpr items, uses OperationMatrix
// =========================================================================

void Evaluator::visit_matrix(const MatrixExpr& expr)
{
    std::vector<rel::Value> items;
    items.reserve(expr.items.size());

    // Nested matrices (e.g. {{1},{2}}) must not unwrap inner single-element
    // braces, otherwise the outer matrix receives scalars instead of vectors
    // and collapses {{1},{2}} into {1,2}.
    bool was_inside = inside_matrix_;
    inside_matrix_ = true;
    for (const auto& item : expr.items)
        expand_item(*this, item, items);
    inside_matrix_ = was_inside;

    if (items.empty())
    {
        result_ = rel::Value();
        return;
    }

    // Only unwrap a single-element brace at the outermost level.
    // {1} → 1, but inside {{1},{2}} each inner {1} stays as a 1-vector.
    if (items.size() == 1 && !was_inside)
    {
        result_ = items[0];
        return;
    }

    result_ = rel::operation::OperationMatrix(items);
}

// =========================================================================
//  visit_call — function call or matrix index a(i, j, ...)
// =========================================================================

namespace
{
    /// Render a callee for error messages: identifier references print as
    /// their dotted name, everything else as a generic description.
    std::string callee_name(const rel::ExprPtr& callee)
    {
        if (const rel::ReferenceExpr* ref =
                dynamic_cast<const rel::ReferenceExpr*>(callee.get()))
        {
            std::string name;
            for (std::size_t i = 0; i < ref->segments.size(); ++i)
            {
                if (i > 0) name += (ref->segments[i].sep == rel::RefSeparator::DDot) ? ".." : ".";
                name += ref->segments[i].name;
            }
            return name;
        }
        return "expression";
    }
} // namespace

void Evaluator::visit_call(const CallExpr& expr)
{
    // Function call and matrix index share the `a(...)` syntax; try the
    // function-call interpretation first, then fall back to indexing.
    if (try_function_call(expr))
        return;

    result_ = eval_matrix_index(expr);
}

// =========================================================================
//  try_function_call — dispatch to a registered custom function
// =========================================================================
//
//  A call is a function call when the callee is a single-segment identifier
//  that exists in the environment's function registry.  Everything else
//  (multi-segment paths, other expressions) is left to eval_matrix_index.

bool Evaluator::try_function_call(const CallExpr& expr)
{
    const ReferenceExpr* ref =
        dynamic_cast<const ReferenceExpr*>(expr.callee.get());
    if (!ref || ref->segments.size() != 1)
        return false;

    const Function* fn = Environment::FindFunction(ref->segments[0].name);
    if (!fn)
        return false;

    // Work on a stack copy: the implementation may register more functions
    // while running, and rehashing the registry would invalidate the
    // pointer we hold.
    Function fn_copy = *fn;
    result_ = invoke_function(fn_copy, expr);
    return true;
}

// =========================================================================
//  eval_matrix_index — matrix / DataArray indexing a(i, j)
// =========================================================================

rel::Value Evaluator::eval_matrix_index(const CallExpr& expr)
{
    rel::Value obj = Evaluate(*expr.callee);

    bool is_matrix_index =
        obj.is_data_array() ||
        (obj.is_measurement() &&
         obj.as_measurement().data_kind() != xdataset::DataKind::kScalar);

    if (!is_matrix_index)
    {
        throw std::runtime_error(
            std::string("'") + callee_name(expr.callee) +
            "' is not a registered function or matrix");
    }

    std::vector<xdataset::MultiIndexSelector> selectors;
    selectors.reserve(expr.args.size());
    for (const auto& arg : expr.args)
        selectors.push_back(make_selector(*this, arg, /*one_based=*/true));

    if (obj.is_measurement())
        return rel::Value(obj.as_measurement().at(selectors));
    return rel::Value(obj.as_data_array().at(selectors));
}

// =========================================================================
//  invoke_function — resolve call-site slots, then call the implementation
// =========================================================================
//
//  Slot resolution lives here (not in Function): explicit arguments
//  are evaluated, omitted slots are filled with the declared defaults via
//  Function::HasDefault / DefaultValue, and the fully-resolved list
//  is handed to Function::Invoke.

rel::Value Evaluator::invoke_function(const Function& fn,
                                           const CallExpr& expr)
{
    const std::size_t provided = expr.args.size();
    if (provided > fn.arity())
    {
        std::ostringstream oss;
        oss << "function '" << fn.name() << "' expects at most " << fn.arity()
            << " argument(s), got " << provided;
        throw std::runtime_error(oss.str());
    }

    Function::ArgMap user_args;
    for (std::size_t i = 0; i < fn.arity(); ++i)
    {
        if (i < provided && expr.args[i])
            user_args[fn.params()[i].name] = Evaluate(*expr.args[i]);
    }

    return fn.Invoke(user_args);
}

// =========================================================================
//  visit_index — sweep index a[i, j, ...] via DataArray::select()
// =========================================================================

void Evaluator::visit_index(const IndexExpr& expr)
{
    rel::Value obj = Evaluate(*expr.object);

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
        da.data().size() == 1)
    {
        result_ = rel::Value(da.data().measurement_at(0));
    }
    else
    {
        result_ = rel::Value(std::move(da));
    }
}

// =========================================================================
//  visit_range / visit_null_range — dead code; these nodes are consumed
//  inside make_selector / expand_item above, never reached as top-level.
// =========================================================================

void Evaluator::visit_range(const RangeExpr&)           {}
void Evaluator::visit_null_range(const NullRangeExpr&)  {}

} // namespace rel
