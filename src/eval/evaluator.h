#pragma once

#include "ast/expr.h"
#include "eval/environment.h"
#include "eval/value.h"

#include <string>

namespace rel {

// =========================================================================
//  Evaluator — ExprVisitor that walks the AST and produces a Value
// =========================================================================
//
//  Usage:
//    Evaluator eval(env);
//    Value result = eval.evaluate(expr);
//
//  The Evaluator holds a reference to the Environment for variable lookup.
//  It is stateless beyond that — each call to evaluate() resets internal
//  state.

class Evaluator : public ExprVisitor
{
public:
    explicit Evaluator(Environment& env);

    /// Top-level entry point.
    Value evaluate(const Expr& expr);

    // ---- ExprVisitor interface ----
    void visit_null(const NullExpr& expr) override;
    void visit_number(const NumberExpr& expr) override;
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

    /// Apply a unary operator to a Value.
    Value apply_unary(TokenType op, const Value& operand);

    /// Apply a binary operator to two Values.
    Value apply_binary(TokenType op, const Value& lhs, const Value& rhs);

    /// Apply a short-circuit logical operator.
    Value apply_logical(TokenType op, const LogicalExpr& expr);

    Environment& env_;
    Value result_;
};

// =========================================================================
} // namespace rel
