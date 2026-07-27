// Evaluator sweep/matrix tests — uses eval_string for concise test setup.

#include "eval/evaluator.h"

#include "ast/expr.h"
#include "data_array.h"
#include "data_series.h"
#include "measurement.h"

#include <gtest/gtest.h>

#include <string>

using rel::eval_string;

// =========================================================================
//  SweepExpr — [expr_list] → Independent DataArray
// =========================================================================

TEST(SweepExprTest, ThreeScalars)
{
    rel::Value v = eval_string("[1.0, 2.0, 3.0]");
    EXPECT_TRUE(v.is_data_array());
    auto& da = v.as_data_array();
    EXPECT_EQ(da.data_kind(), xdataset::DataArrayKind::kIndependent);
    EXPECT_EQ(da.data().size(), 3u);
    EXPECT_EQ(da.data().data_kind(), xdataset::DataKind::kScalar);
}

TEST(SweepExprTest, SingleScalar)
{
    rel::Value v = eval_string("[42.0]");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 1u);
}

TEST(SweepExprTest, EmptySweep)
{
    // Parser rejects empty [], so construct AST manually.
    rel::SweepExpr expr(1, 1, std::vector<rel::ExprPtr>{});
    rel::Environment env;
    rel::Evaluator e(env);
    rel::Value v = e.evaluate(expr);
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 0u);
}

TEST(SweepExprTest, MixedTypesIntegerAndUnit)
{
    // Regression: 2MHz is Integer with unit, plain scalars are Real.
    // Combine must handle Integer→Real promotion without boost::bad_get.
    rel::Value v = eval_string("[{1, 2MHz}, {3, 4}]");
    EXPECT_TRUE(v.is_data_array());
    // Should produce a 2-row, 2-col (Vector) DataArray without crash.
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
}

TEST(SweepExprTest, IntegerSweep)
{
    rel::Value v = eval_string("[1, 2, 3]");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 3u);
    EXPECT_EQ(v.as_data_array().data().data_type(), xdataset::DataType::kInteger);
}

TEST(SweepExprTest, MixedScalarAndVectorInsideSweep)
{
    // [{1,2},{3,4}] — two MatrixExpr producing Vector(2) inside Sweep
    rel::Value v = eval_string("[{1,2}, {3,4}]");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
}

// =========================================================================
//  MatrixExpr — {expr_list}
// =========================================================================

TEST(MatrixExprTest, ThreeScalarsPromotedToVector)
{
    rel::Value v = eval_string("{1.0, 2.0, 3.0}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
    auto vec = v.as_measurement().as_vector<double>();
    EXPECT_EQ(vec.size(), 3);
    EXPECT_DOUBLE_EQ(vec[0], 1.0);
}

TEST(MatrixExprTest, SingleScalarStaysScalar)
{
    rel::Value v = eval_string("{5.0}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kScalar);
}

TEST(MatrixExprTest, TwoScalarsPromotedToVector)
{
    rel::Value v = eval_string("{10.0, 20.0}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
    auto vec = v.as_measurement().as_vector<double>();
    EXPECT_EQ(vec.size(), 2);
    EXPECT_DOUBLE_EQ(vec[0], 10.0);
    EXPECT_DOUBLE_EQ(vec[1], 20.0);
}

TEST(MatrixExprTest, IntegerScalarsPromotedToVector)
{
    rel::Value v = eval_string("{1, 2, 3}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
}

TEST(MatrixExprTest, EmptyMatrixReturnsNull)
{
    rel::MatrixExpr expr(1, 1, std::vector<rel::ExprPtr>{});
    rel::Environment env;
    rel::Evaluator e(env);
    rel::Value v = e.evaluate(expr);
    EXPECT_TRUE(v.is_null());
}

TEST(MatrixExprTest, MixedUnitAndNoUnit)
{
    // Regression: 1 (Integer, no unit) + 2MHz (Integer, with unit)
    rel::Value v = eval_string("{1, 2MHz}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
}

TEST(MatrixExprTest, WithStringThrows)
{
    EXPECT_THROW(eval_string("{1, 2, \"hello\"}"), std::exception);
}

// =========================================================================
//  Combined: SweepExpr inside MatrixExpr
// =========================================================================

TEST(SweepMatrixTest, SingleSweepInMatrix)
{
    rel::Value v = eval_string("{[1.0, 2.0, 3.0]}");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 3u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kScalar);
}

TEST(SweepMatrixTest, TwoSweepsConcat)
{
    rel::Value v = eval_string("{[1.0, 2.0], [3.0, 4.0]}");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
}

TEST(SweepMatrixTest, SweepAndScalarConcatWithBroadcast)
{
    // [1,2,3] (3-row DA) + 42 (1-row M broadcast) → 3 rows Vector(2)
    rel::Value v = eval_string("{[1.0, 2.0, 3.0], 42.0}");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 3u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
}

// =========================================================================
//  Combined: MatrixExpr inside SweepExpr
// =========================================================================

TEST(SweepMatrixTest, MatrixInsideSweep)
{
    rel::Value v = eval_string("[{1.0, 2.0, 3.0}]");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 1u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
}

TEST(SweepMatrixTest, MultipleMatricesInsideSweep)
{
    rel::Value v = eval_string("[{1.0, 2.0}, {3.0, 4.0}]");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
}

// =========================================================================
//  Regression: to_string() must not crash
// =========================================================================

TEST(RegressionTest, SweepToString)
{
    rel::Value v = eval_string("[1, 2, 3]");
    std::string s = v.to_string();
    EXPECT_FALSE(s.empty());
}

TEST(RegressionTest, MatrixToString)
{
    rel::Value v = eval_string("{1, 2, 3}");
    std::string s = v.to_string();
    EXPECT_FALSE(s.empty());
}

TEST(RegressionTest, NestedSweepMatrixToString)
{
    // [{1,2},{3,4}] was crashing because Combine produced Dependent DataArray
    rel::Value v = eval_string("[{1,2}, {3,4}]");
    std::string s = v.to_string();
    EXPECT_FALSE(s.empty());
}

TEST(RegressionTest, SweepWithUnitsToString)
{
    // Mixed units crash regression
    rel::Value v = eval_string("[{1,2MHz}, {3,4}]");
    std::string s = v.to_string();
    EXPECT_FALSE(s.empty());
}
