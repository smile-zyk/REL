#pragma once

#include "ast/expr.h"
#include "eval/environment.h"
#include "value.h"

#include <string>

namespace rel {

// =========================================================================
//  Evaluator — ExprVisitor that walks the AST and produces an xdataset::Value
// =========================================================================
//
//  Usage:
//    Evaluator eval(env);
//    xdataset::Value result = eval.evaluate(expr);
//
//  Arithmetic, comparison, etc. delegate to xdataset::Value operators.

class Evaluator : public ExprVisitor
{
public:
    explicit Evaluator(Environment& env);

    /// Top-level entry point.
    xdataset::Value Evaluate(const Expr& expr);

    // ---- ExprVisitor interface ----
    void visit_number(const NumberExpr& expr) override;
    void visit_boolean(const BooleanExpr& expr) override;
    void visit_string(const StringExpr& expr) override;
    void visit_reference(const ReferenceExpr& expr) override;
    void visit_unary(const UnaryExpr& expr) override;
    void visit_binary(const BinaryExpr& expr) override;
    void visit_logical(const LogicalExpr& expr) override;
    void visit_conditional(const ConditionalExpr& expr) override;
    void visit_if(const IfExpr& expr) override;
    void visit_call(const CallExpr& expr) override;
    void visit_index(const IndexExpr& expr) override;
    void visit_grouping(const GroupingExpr& expr) override;
    void visit_sweep(const SweepExpr& expr) override;
    void visit_matrix(const MatrixExpr& expr) override;
    void visit_range(const RangeExpr& expr) override;
    void visit_null_range(const NullRangeExpr& expr) override;

private:
    /// Parse base_lexeme according to radix into a double.
    static double parse_base(const std::string& lexeme, int radix);

    /// Apply a unary operator.
    xdataset::Value apply_unary(TokenType op, const xdataset::Value& operand);

    /// Apply a binary operator (delegates to xdataset::Value operators).
    xdataset::Value apply_binary(TokenType op, const xdataset::Value& lhs, const xdataset::Value& rhs);

    /// Apply a short-circuit logical operator.
    xdataset::Value apply_logical(TokenType op, const LogicalExpr& expr);

    /// Resolve a registered-function call site: evaluate the explicit
    /// arguments, fill omitted parameter slots with the declared defaults
    /// (via Function::HasDefault/DefaultValue), then invoke the
    /// implementation with the fully-resolved argument list.
    xdataset::Value invoke_function(const Function& fn, const CallExpr& expr);

    /// Try to handle `expr` as a call to a registered function.
    /// Returns true when the callee is a single-segment identifier that is
    /// registered in the environment (and the call has been evaluated into
    /// result_); returns false otherwise.
    bool try_function_call(const CallExpr& expr);

    /// Handle `expr` as matrix / DataArray indexing `a(i, j)` via at().
    /// Throws std::runtime_error when the callee is neither a matrix-like
    /// value nor a registered function.
    xdataset::Value eval_matrix_index(const CallExpr& expr);

    Environment& env_;
    xdataset::Value result_;

    /// When inside a visit_matrix call, prevent inner single-element
    /// braces from unwrapping, so that nested matrices (e.g.
    /// {{1},{2}}) stack correctly instead of collapsing to {1,2}.
    bool inside_matrix_ = false;
};

} // namespace rel
