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

    Environment& env_;
    xdataset::Value result_;
};

} // namespace rel
