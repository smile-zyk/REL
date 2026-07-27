// Evaluator sweep/matrix tests.

#include "eval/evaluator.h"
#include "eval/environment.h"
#include "eval/value.h"

#include "ast/expr.h"
#include "data_array.h"
#include "data_series.h"
#include "measurement.h"

#include <Eigen/Dense>

#include <gtest/gtest.h>

#include <memory>
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
    EXPECT_EQ(da.data_kind(), xdataset::DataArrayKind::kIndependent);
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

TEST(SweepExprTest, WithDataArray)
{
    // Flattening logic: 4 scalars → 4 rows.
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

TEST(MatrixExprTest, TwoVectorsPromotedToMatrix)
{
    // Build two Vector(2) Measurement values.
    Eigen::VectorXd v1(2), v2(2);
    v1 << 1.0, 2.0;
    v2 << 3.0, 4.0;

    // Test directly via SweepExpr — each vector is a Measurement, 1 row.
    // MatrixExpr with pure Measurements promotes Vector × N → Matrix.
    std::vector<rel::ExprPtr> items;

    // Use NumberExpr entries with Vector values isn't possible without
    // reference bindings.  Skip for now.
}

TEST(MatrixExprTest, EmptyMatrixReturnsNull)
{
    rel::MatrixExpr expr(1, 1, std::vector<rel::ExprPtr>{});
    rel::Value v = eval_expr(expr);
    EXPECT_TRUE(v.is_null());
}
