// Evaluator sweep/matrix tests -> uses eval for concise test setup.

#include "evaluator.h"
#include "rel.h"

#include "expr.h"
#include "data_array.h"
#include "data_series.h"
#include "measurement.h"

#include <gtest/gtest.h>

#include <string>

using rel::Eval;

// =========================================================================
//  SweepExpr -> [expr_list] -> Independent DataArray
// =========================================================================

TEST(SweepExprTest, ThreeScalars)
{
    rel::Value v = Eval("[1.0, 2.0, 3.0]");
    EXPECT_TRUE(v.is_data_array());
    auto& da = v.as_data_array();
    EXPECT_EQ(da.data_kind(), xdataset::DataArrayKind::kIndependent);
    EXPECT_EQ(da.data().size(), 3u);
    EXPECT_EQ(da.data().data_kind(), xdataset::DataKind::kScalar);
}

TEST(SweepExprTest, SingleScalar)
{
    rel::Value v = Eval("[42.0]");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 1u);
}

TEST(SweepExprTest, EmptySweep)
{
    // Empty sweep now returns default Measurement (empty handled before OperationSweep).
    rel::SweepExpr expr(1, 1, std::vector<rel::ExprPtr>{});
    rel::Environment env;
    rel::Evaluator e(env);
    rel::Value v = e.Evaluate(expr);
    EXPECT_TRUE(v.is_measurement());
}

TEST(SweepExprTest, MixedTypesIntegerAndUnit)
{
    rel::Value v = Eval("[{1, 2MHz}, {3, 4}]");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
}

TEST(SweepExprTest, IntegerSweep)
{
    rel::Value v = Eval("[1, 2, 3]");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 3u);
    EXPECT_EQ(v.as_data_array().data().data_type(), xdataset::DataType::kInteger);
}

TEST(SweepExprTest, MixedScalarAndVectorInsideSweep)
{
    // [{1,2},{3,4}] -> two MatrixExpr producing Vector(2) inside Sweep
    rel::Value v = Eval("[{1,2}, {3,4}]");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
}

// =========================================================================
//  MatrixExpr -> {expr_list}
// =========================================================================

TEST(MatrixExprTest, ThreeScalarsPromotedToVector)
{
    rel::Value v = Eval("{1.0, 2.0, 3.0}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
    auto vec = v.as_measurement().as_vector<double>();
    EXPECT_EQ(vec.size(), 3);
    EXPECT_DOUBLE_EQ(vec[0], 1.0);
}

TEST(MatrixExprTest, SingleScalarStaysScalar)
{
    // OperationHorzcat with single scalar stays Scalar.
    rel::Value v = Eval("{5.0}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kScalar);
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 5.0);
}

TEST(MatrixExprTest, TwoScalarsPromotedToVector)
{
    rel::Value v = Eval("{10.0, 20.0}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
    auto vec = v.as_measurement().as_vector<double>();
    EXPECT_EQ(vec.size(), 2);
    EXPECT_DOUBLE_EQ(vec[0], 10.0);
    EXPECT_DOUBLE_EQ(vec[1], 20.0);
}

TEST(MatrixExprTest, IntegerScalarsPromotedToVector)
{
    rel::Value v = Eval("{1, 2, 3}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
}

TEST(MatrixExprTest, EmptyMatrixReturnsDefaultMeasurement)
{
    rel::MatrixExpr expr(1, 1, std::vector<rel::ExprPtr>{});
    rel::Environment env;
    rel::Evaluator e(env);
    rel::Value v = e.Evaluate(expr);
    EXPECT_TRUE(v.is_measurement());
}

TEST(MatrixExprTest, MixedUnitAndNoUnit)
{
    // Regression: 1 (Integer, no unit) + 2MHz (Integer, with unit)
    rel::Value v = Eval("{1, 2MHz}");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kVector);
}

TEST(MatrixExprTest, WithStringThrows)
{
    EXPECT_THROW(Eval("{1, 2, \"hello\"}"), std::exception);
}

// =========================================================================
//  MatrixExpr horizontal concat vs vertical stack
// =========================================================================

TEST(MatrixExprTest, NestedScalarsStackToColumn)
{
    // {{1},{2}}: the outer {} sees two row blocks -> Matrix(2, 1).
    rel::Value v = Eval("{{1},{2}}");
    ASSERT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kMatrix);
    const auto m = v.as_measurement().as_matrix<int>();
    EXPECT_EQ(m.rows(), 2);
    EXPECT_EQ(m.cols(), 1);
    EXPECT_EQ(m(0, 0), 1);
    EXPECT_EQ(m(1, 0), 2);
}

TEST(MatrixExprTest, NestedRowBlocksToMatrix)
{
    // {{1,2},{3,4}}: two 1x2 row blocks -> Matrix(2, 2).
    rel::Value v = Eval("{{1,2},{3,4}}");
    ASSERT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kMatrix);
    const auto m = v.as_measurement().as_matrix<int>();
    EXPECT_EQ(m.rows(), 2);
    EXPECT_EQ(m.cols(), 2);
    EXPECT_EQ(m(0, 0), 1);
    EXPECT_EQ(m(0, 1), 2);
    EXPECT_EQ(m(1, 0), 3);
    EXPECT_EQ(m(1, 1), 4);
}

TEST(MatrixExprTest, ConcatMatricesHorizontally)
{
    // S = 2x2; {S,S} -> Matrix(2, 4).
    rel::Environment env;
    env.Define("S", rel::Eval("{{1,2},{3,4}}", &env));
    rel::Value v = rel::Eval("{S,S}", &env);
    ASSERT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kMatrix);
    const auto m = v.as_measurement().as_matrix<int>();
    EXPECT_EQ(m.rows(), 2);
    EXPECT_EQ(m.cols(), 4);
    EXPECT_EQ(m(0, 0), 1);
    EXPECT_EQ(m(0, 2), 1);
    EXPECT_EQ(m(1, 1), 4);
    EXPECT_EQ(m(1, 3), 4);
}

TEST(MatrixExprTest, StackMatricesVertically)
{
    // S = 2x2; {{S},{S}} -> Matrix(4, 2).
    rel::Environment env;
    env.Define("S", rel::Eval("{{1,2},{3,4}}", &env));
    rel::Value v = rel::Eval("{{S},{S}}", &env);
    ASSERT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_kind(), xdataset::DataKind::kMatrix);
    const auto m = v.as_measurement().as_matrix<int>();
    EXPECT_EQ(m.rows(), 4);
    EXPECT_EQ(m.cols(), 2);
    EXPECT_EQ(m(0, 0), 1);
    EXPECT_EQ(m(1, 0), 3);   // S itself is row-major {{1,2},{3,4}}
    EXPECT_EQ(m(2, 0), 1);   // second S
    EXPECT_EQ(m(3, 1), 4);
}

TEST(MatrixExprTest, ConcatVectorsHorizontally)
{
    // v = {1,2}; {v,v} -> Vector(4).
    rel::Environment env;
    env.Define("v", rel::Eval("{1,2}", &env));
    rel::Value r = rel::Eval("{v,v}", &env);
    ASSERT_TRUE(r.is_measurement());
    EXPECT_EQ(r.as_measurement().data_kind(), xdataset::DataKind::kVector);
    const auto vec = r.as_measurement().as_vector<int>();
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(vec(0), 1);
    EXPECT_EQ(vec(1), 2);
    EXPECT_EQ(vec(2), 1);
    EXPECT_EQ(vec(3), 2);
}

TEST(MatrixExprTest, StackDataArrayBlocksKeepsRows)
{
    // S is a 2-row DataArray of 2x2 matrix cells.
    // {{S},{S}} stacks the CELLS vertically per data row:
    //   row 0 -> 4x2, row 1 -> 4x2, data rows stay 2.
    rel::Environment env;
    env.Define("S",
               rel::Eval("[{{1,2},{3,4}}, {{5,6},{7,8}}]", &env));
    rel::Value v = rel::Eval("{{S},{S}}", &env);
    ASSERT_TRUE(v.is_data_array());
    const xdataset::DataArray& da = v.as_data_array();
    EXPECT_EQ(da.data().size(), 2u);                 // data rows preserved
    EXPECT_EQ(da.data().data_shape()[0], 4);         // cell grew 2x2 -> 4x2
    EXPECT_EQ(da.data().data_shape()[1], 2);

    // Row 0 cell: S itself ({{1,2},{3,4}}) stacked on itself.
    const auto m0 = da.data().matrix_at<int>(0);
    EXPECT_EQ(m0(0, 0), 1);
    EXPECT_EQ(m0(0, 1), 2);
    EXPECT_EQ(m0(1, 0), 3);
    EXPECT_EQ(m0(1, 1), 4);
    EXPECT_EQ(m0(2, 0), 1);
    EXPECT_EQ(m0(3, 1), 4);

    // Row 1 cell: {{5,6},{7,8}} stacked on itself.
    const auto m1 = da.data().matrix_at<int>(1);
    EXPECT_EQ(m1(0, 0), 5);
    EXPECT_EQ(m1(0, 1), 6);
    EXPECT_EQ(m1(1, 0), 7);
    EXPECT_EQ(m1(1, 1), 8);
    EXPECT_EQ(m1(2, 0), 5);
    EXPECT_EQ(m1(3, 1), 8);
}

TEST(MatrixExprTest, ConcatDataArrayBlocksKeepsRows)
{
    // S 2-row DataArray of 2x2 cells; {S,S} -> 2 rows of 2x4 cells.
    rel::Environment env;
    env.Define("S",
               rel::Eval("[{{1,2},{3,4}}, {{5,6},{7,8}}]", &env));
    rel::Value v = rel::Eval("{S,S}", &env);
    ASSERT_TRUE(v.is_data_array());
    const xdataset::DataArray& da = v.as_data_array();
    EXPECT_EQ(da.data().size(), 2u);
    EXPECT_EQ(da.data().data_shape()[0], 2);
    EXPECT_EQ(da.data().data_shape()[1], 4);

    const auto m0 = da.data().matrix_at<int>(0);
    EXPECT_EQ(m0(0, 0), 1);
    EXPECT_EQ(m0(0, 2), 1);
    EXPECT_EQ(m0(1, 1), 4);
    EXPECT_EQ(m0(1, 3), 4);

    const auto m1 = da.data().matrix_at<int>(1);
    EXPECT_EQ(m1(0, 0), 5);
    EXPECT_EQ(m1(0, 2), 5);
    EXPECT_EQ(m1(1, 1), 8);
    EXPECT_EQ(m1(1, 3), 8);
}

// =========================================================================
//  Combined: SweepExpr inside MatrixExpr
// =========================================================================

TEST(SweepMatrixTest, SingleSweepInMatrix)
{
    // Matrix wrapping a single sweep stays as-is.
    rel::Value v = Eval("{[1.0, 2.0, 3.0]}");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 3u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kScalar);
}

TEST(SweepMatrixTest, TwoSweepsConcat)
{
    rel::Value v = Eval("{[1.0, 2.0], [3.0, 4.0]}");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
}

TEST(SweepMatrixTest, SweepAndScalarConcatWithBroadcast)
{
    // [1,2,3] (3-row DA) + 42 (1-row M broadcast) -> 3 rows Vector(2)
    rel::Value v = Eval("{[1.0, 2.0, 3.0], 42.0}");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 3u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
}

// =========================================================================
//  Combined: MatrixExpr inside SweepExpr
// =========================================================================

TEST(SweepMatrixTest, MatrixInsideSweep)
{
    rel::Value v = Eval("[{1.0, 2.0, 3.0}]");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 1u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
}

TEST(SweepMatrixTest, MultipleMatricesInsideSweep)
{
    rel::Value v = Eval("[{1.0, 2.0}, {3.0, 4.0}]");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
}

// =========================================================================
//  Regression: Format must not crash
// =========================================================================

TEST(RegressionTest, SweepFormat)
{
    rel::Value v = Eval("[1, 2, 3]");
    std::string s = v.Format();
    EXPECT_FALSE(s.empty());
}

TEST(RegressionTest, MatrixFormat)
{
    rel::Value v = Eval("{1, 2, 3}");
    std::string s = v.Format();
    EXPECT_FALSE(s.empty());
}

TEST(RegressionTest, NestedSweepMatrixFormat)
{
    // [{1,2},{3,4}] was crashing because Combine produced Dependent DataArray
    rel::Value v = Eval("[{1,2}, {3,4}]");
    std::string s = v.Format();
    EXPECT_FALSE(s.empty());
}

TEST(RegressionTest, SweepWithUnitsFormat)
{
    rel::Value v = Eval("[{1,2MHz}, {3,4}]");
    std::string s = v.Format();
    EXPECT_FALSE(s.empty());
}

TEST(RegressionTest, MatrixWithSweepAndScalarConcat)
{
    rel::Value v = Eval("{[1,2GHz], 3}");
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
    EXPECT_EQ(v.as_data_array().data().data_kind(), xdataset::DataKind::kVector);
    std::string s = v.Format();
    EXPECT_FALSE(s.empty());
}
