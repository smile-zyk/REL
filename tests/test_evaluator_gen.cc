// Evaluator sweep/matrix tests.

#include "eval/evaluator.h"
#include "eval/environment.h"
#include "eval/value.h"

#include "ast/expr.h"
#include "data_array.h"
#include "data_series.h"
#include "measurement.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    rel::Value eval_expr(const rel::Expr& expr)
    {
        rel::Environment env;
        rel::Evaluator e(env);
        return e.evaluate(expr);
    }

    rel::ExprPtr num(double v)
    {
        return rel::ExprPtr(new rel::NumberExpr(1, 1, rel::NumberKind::Real,
                             std::to_string(v), 10, ""));
    }
} // namespace

// =========================================================================
//  SweepExpr — [expr_list] → Independent DataArray
// =========================================================================

TEST(SweepExprTest, ThreeScalars)
{
    std::vector<rel::ExprPtr> items;
    items.push_back(num(1.0));
    items.push_back(num(2.0));
    items.push_back(num(3.0));
    rel::SweepExpr expr(1, 1, std::move(items));

    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_data_array());
    xdataset::DataArray& da = v.as_data_array();
    EXPECT_EQ(da.data().size(), 3u);
    EXPECT_EQ(da.data().data_kind(), xdataset::DataKind::kScalar);
}

TEST(SweepExprTest, SingleScalar)
{
    std::vector<rel::ExprPtr> items;
    items.push_back(num(42.0));
    rel::SweepExpr expr(1, 1, std::move(items));

    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 1u);
}

TEST(SweepExprTest, FourScalars)
{
    std::vector<rel::ExprPtr> items;
    items.push_back(num(1.0));
    items.push_back(num(2.0));
    items.push_back(num(3.0));
    items.push_back(num(4.0));
    rel::SweepExpr expr(1, 1, std::move(items));

    rel::Value v = eval_expr(expr);
    EXPECT_EQ(v.as_data_array().data().size(), 4u);
}

TEST(SweepExprTest, EmptySweep)
{
    std::vector<rel::ExprPtr> items;
    rel::SweepExpr expr(1, 1, std::move(items));
    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 0u);
}

// =========================================================================
//  MatrixExpr — {expr_list}, pure Measurement → Measurement
// =========================================================================

TEST(MatrixExprTest, ThreeScalarsPromotedToVector)
{
    std::vector<rel::ExprPtr> items;
    items.push_back(num(1.0));
    items.push_back(num(2.0));
    items.push_back(num(3.0));
    rel::MatrixExpr expr(1, 1, std::move(items));

    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
    auto vec = v.as_measurement().as_vector<double>();
    EXPECT_EQ(vec.size(), 3);
    EXPECT_DOUBLE_EQ(vec[0], 1.0);
    EXPECT_DOUBLE_EQ(vec[1], 2.0);
    EXPECT_DOUBLE_EQ(vec[2], 3.0);
}

TEST(MatrixExprTest, SingleScalarStaysScalar)
{
    std::vector<rel::ExprPtr> items;
    items.push_back(num(5.0));
    rel::MatrixExpr expr(1, 1, std::move(items));

    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kScalar);
}

TEST(MatrixExprTest, TwoScalarsPromotedToVector)
{
    std::vector<rel::ExprPtr> items;
    items.push_back(num(10.0));
    items.push_back(num(20.0));
    rel::MatrixExpr expr(1, 1, std::move(items));

    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
    auto vec = v.as_measurement().as_vector<double>();
    EXPECT_EQ(vec.size(), 2);
    EXPECT_DOUBLE_EQ(vec[0], 10.0);
    EXPECT_DOUBLE_EQ(vec[1], 20.0);
}

TEST(MatrixExprTest, EmptyMatrixReturnsNull)
{
    rel::MatrixExpr expr(1, 1, std::vector<rel::ExprPtr>{});
    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_null());
}

// =========================================================================
//  Combined: SweepExpr inside MatrixExpr
// =========================================================================

TEST(SweepMatrixTest, SingleSweepInMatrix)
{
    // {[1,2,3]} — SweepExpr (3-row DA) inside MatrixExpr → DataArray Concat
    std::vector<rel::ExprPtr> inner;
    inner.push_back(num(1.0));
    inner.push_back(num(2.0));
    inner.push_back(num(3.0));

    std::vector<rel::ExprPtr> outer;
    outer.push_back(rel::ExprPtr(new rel::SweepExpr(1, 1, std::move(inner))));
    rel::MatrixExpr expr(1, 1, std::move(outer));

    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 3u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kScalar);
}

TEST(SweepMatrixTest, TwoSweepsConcat)
{
    // {[1,2], [3,4]} → Concat two 2-row DAs → 2 rows of Vector(2)
    std::vector<rel::ExprPtr> s1;
    s1.push_back(num(1.0));
    s1.push_back(num(2.0));

    std::vector<rel::ExprPtr> s2;
    s2.push_back(num(3.0));
    s2.push_back(num(4.0));

    std::vector<rel::ExprPtr> outer;
    outer.push_back(rel::ExprPtr(new rel::SweepExpr(1, 1, std::move(s1))));
    outer.push_back(rel::ExprPtr(new rel::SweepExpr(1, 1, std::move(s2))));
    rel::MatrixExpr expr(1, 1, std::move(outer));

    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
}

TEST(SweepMatrixTest, SweepAndScalarConcatWithBroadcast)
{
    // {[1,2,3], 42} → DA(3 rows) + M(1 row broadcast) → 3 rows of Vector(2)
    std::vector<rel::ExprPtr> inner;
    inner.push_back(num(1.0));
    inner.push_back(num(2.0));
    inner.push_back(num(3.0));

    std::vector<rel::ExprPtr> outer;
    outer.push_back(rel::ExprPtr(new rel::SweepExpr(1, 1, std::move(inner))));
    outer.push_back(num(42.0));
    rel::MatrixExpr expr(1, 1, std::move(outer));

    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 3u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
}

// =========================================================================
//  Combined: MatrixExpr inside SweepExpr
// =========================================================================

TEST(SweepMatrixTest, MatrixInsideSweep)
{
    // [{1,2,3}] → MatrixExpr produces Vector(3) Measurement → 1 Vector row
    std::vector<rel::ExprPtr> mat;
    mat.push_back(num(1.0));
    mat.push_back(num(2.0));
    mat.push_back(num(3.0));

    std::vector<rel::ExprPtr> outer;
    outer.push_back(rel::ExprPtr(new rel::MatrixExpr(1, 1, std::move(mat))));
    rel::SweepExpr expr(1, 1, std::move(outer));

    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 1u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
}

TEST(SweepMatrixTest, MultipleMatricesInsideSweep)
{
    // [{1,2}, {3,4}] → two Vector(2) Meas → 2 Vector rows
    std::vector<rel::ExprPtr> m1;
    m1.push_back(num(1.0));
    m1.push_back(num(2.0));

    std::vector<rel::ExprPtr> m2;
    m2.push_back(num(3.0));
    m2.push_back(num(4.0));

    std::vector<rel::ExprPtr> outer;
    outer.push_back(rel::ExprPtr(new rel::MatrixExpr(1, 1, std::move(m1))));
    outer.push_back(rel::ExprPtr(new rel::MatrixExpr(1, 1, std::move(m2))));
    rel::SweepExpr expr(1, 1, std::move(outer));

    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
}
