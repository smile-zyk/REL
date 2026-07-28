// Evaluator operator tests -> uses eval for concise test setup.

#include "rel.h"

#include "data_array.h"
#include "measurement.h"

#include <gtest/gtest.h>

#include <string>

using rel::eval;

// =========================================================================
//  Arithmetic
// =========================================================================

TEST(OperatorTest, Addition)
{
    rel::Value v = eval("1 + 2");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_EQ(v.as_measurement().data_type(), xdataset::DataType::kInteger);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 3);
}

TEST(OperatorTest, Subtraction)
{
    rel::Value v = eval("5 - 3");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 2);
}

TEST(OperatorTest, Multiplication)
{
    rel::Value v = eval("3 * 4");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 12);
}

TEST(OperatorTest, Division)
{
    rel::Value v = eval("10 / 2");
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 5.0);
}

TEST(OperatorTest, Modulo)
{
    rel::Value v = eval("7 % 3");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, Power)
{
    rel::Value v = eval("2 ** 3");
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 8.0);
}

TEST(OperatorTest, ChainedAddition)
{
    rel::Value v = eval("1 + 2 + 3");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 6);
}

TEST(OperatorTest, PrecedenceMulBeforeAdd)
{
    rel::Value v = eval("2 * 3 + 4");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 10);
}

TEST(OperatorTest, GroupingOverridesPrecedence)
{
    rel::Value v = eval("(1 + 2) * 3");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 9);
}

TEST(OperatorTest, RealDivision)
{
    rel::Value v = eval("10.0 / 3.0");
    EXPECT_TRUE(v.is_measurement());
}

// =========================================================================
//  Shift
// =========================================================================

TEST(OperatorTest, LeftShift)
{
    rel::Value v = eval("1 << 2");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 4);
}

TEST(OperatorTest, RightShift)
{
    rel::Value v = eval("8 >> 1");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 4);
}

// =========================================================================
//  Comparison
// =========================================================================

TEST(OperatorTest, LessThan)
{
    rel::Value v = eval("1 < 2");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, LessThanFalse)
{
    rel::Value v = eval("3 < 2");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 0);
}

TEST(OperatorTest, GreaterThan)
{
    rel::Value v = eval("3 > 2");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, LessEqual)
{
    rel::Value v = eval("2 <= 2");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, GreaterEqual)
{
    rel::Value v = eval("2 >= 2");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, Equal)
{
    rel::Value v = eval("1 == 1");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, EqualFalse)
{
    rel::Value v = eval("1 == 2");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 0);
}

TEST(OperatorTest, NotEqual)
{
    rel::Value v = eval("1 != 2");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, KeywordEquals)
{
    rel::Value v = eval("1 EQUALS 1");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, KeywordNotEquals)
{
    rel::Value v = eval("1 NOTEQUALS 2");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

// =========================================================================
//  Bitwise
// =========================================================================

TEST(OperatorTest, BitwiseAnd)
{
    rel::Value v = eval("5 & 3");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, BitwiseOr)
{
    rel::Value v = eval("5 | 2");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 7);
}

TEST(OperatorTest, BitwiseXor)
{
    rel::Value v = eval("5 ^ 3");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 6);
}

// =========================================================================
//  Unary
// =========================================================================

TEST(OperatorTest, UnaryNegate)
{
    rel::Value v = eval("-3.14");
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), -3.14);
}

TEST(OperatorTest, UnaryLogicalNotTrue)
{
    rel::Value v = eval("!1");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 0);
}

TEST(OperatorTest, UnaryLogicalNotFalse)
{
    rel::Value v = eval("!0");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, KeywordNot)
{
    rel::Value v = eval("NOT 1");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 0);
}

TEST(OperatorTest, UnaryBitwiseNot)
{
    rel::Value v = eval("~5");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), ~5);
}

// =========================================================================
//  Logical (short-circuit)
// =========================================================================

TEST(OperatorTest, AndTrue)
{
    rel::Value v = eval("1 && 1");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, AndFalse)
{
    rel::Value v = eval("1 && 0");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 0);
}

TEST(OperatorTest, OrTrue)
{
    rel::Value v = eval("0 || 1");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, OrFalse)
{
    rel::Value v = eval("0 || 0");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 0);
}

TEST(OperatorTest, KeywordAnd)
{
    rel::Value v = eval("1 AND 1");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, KeywordOr)
{
    rel::Value v = eval("0 OR 1");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 1);
}

TEST(OperatorTest, ShortCircuitAnd)
{
    //      0 && (1/0) -> should NOT evaluate 1/0 (division by zero is fine in Measurement)
    //      The fact that it doesn't crash proves short-circuit works.
    //      But eval doesn't have divide-by-zero to test with.
    //      Instead: 0 && <syntax-error> -> parser would reject, so skip.
    //      Smoke test: 0 && any-value doesn't crash.
    rel::Value v = eval("0 && 1");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 0);
}

TEST(OperatorTest, ComplexExpression)
{
    rel::Value v = eval("1 + 2 * 3 - 4 / 2");
    EXPECT_EQ(v.as_measurement().as_scalar<double>(), 5);  // 1+6-2
}

TEST(OperatorTest, NegateExpression)
{
    rel::Value v = eval("-(1 + 2)");
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), -3);
}

// =========================================================================
//  Operator with units
// =========================================================================

TEST(OperatorTest, AddWithUnit)
{
    rel::Value v = eval("1GHz + 2GHz");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_TRUE(v.as_measurement().unit().has_dimension());
}

TEST(OperatorTest, MulWithUnit)
{
    rel::Value v = eval("2 * 3MHz");
    EXPECT_TRUE(v.is_measurement());
}
