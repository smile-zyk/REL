#ifndef REL_RUNTIME_OPERATION_H
#define REL_RUNTIME_OPERATION_H

#include "rel_runtime_api.h"
#include "value.h"

#include <vector>

namespace rel {

// Bring all xdataset types into rel namespace for convenient unqualified use.
using namespace xdataset;

// =========================================================================
//  Value / Measurement / DataArray operators
// =========================================================================
//
//  These operators used to live inside xdataset (measurement.cc /
//  data_array.cc) and were removed from there so that xdataset stays a pure
//  storage library.  They are re-implemented here, in namespace rel, so
//  that argument-dependent lookup keeps working for expressions like
//  `m1 + m2` (m1/m2 are xdataset::Measurement) or `da1 * da2`.  Every
//  operator delegates to the corresponding OperationXxx kernel below.

// -- Value: arithmetic / comparison / logical / bitwise / shift / unary / pow
REL_RUNTIME_API Value operator+(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator-(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator*(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator/(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator%(const Value& lhs, const Value& rhs);

REL_RUNTIME_API Value operator==(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator!=(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator<(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator>(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator<=(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator>=(const Value& lhs, const Value& rhs);

REL_RUNTIME_API Value operator&&(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator||(const Value& lhs, const Value& rhs);

REL_RUNTIME_API Value operator&(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator|(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator^(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator<<(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value operator>>(const Value& lhs, const Value& rhs);

REL_RUNTIME_API Value operator-(const Value& v);
REL_RUNTIME_API Value operator!(const Value& v);
REL_RUNTIME_API Value operator~(const Value& v);

REL_RUNTIME_API Value pow(const Value& base, const Value& exponent);

// -- Measurement: unary
REL_RUNTIME_API Measurement operator-(const Measurement& v);
REL_RUNTIME_API Measurement operator!(const Measurement& v);
REL_RUNTIME_API Measurement operator~(const Measurement& v);

// -- Measurement: arithmetic / comparison / logical / bitwise / shift / mod / pow
REL_RUNTIME_API Measurement operator+(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator-(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator*(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator/(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator%(const Measurement& lhs, const Measurement& rhs);

REL_RUNTIME_API Measurement operator==(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator!=(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator<(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator>(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator<=(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator>=(const Measurement& lhs, const Measurement& rhs);

REL_RUNTIME_API Measurement operator&&(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator||(const Measurement& lhs, const Measurement& rhs);

REL_RUNTIME_API Measurement operator&(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator|(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator^(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator<<(const Measurement& lhs, const Measurement& rhs);
REL_RUNTIME_API Measurement operator>>(const Measurement& lhs, const Measurement& rhs);

REL_RUNTIME_API Measurement pow(const Measurement& base, const Measurement& exponent);

// -- DataArray: arithmetic (AA, AM, MA)
REL_RUNTIME_API DataArray operator+(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator-(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator*(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator/(const DataArray& lhs, const DataArray& rhs);

REL_RUNTIME_API DataArray operator+(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator-(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator*(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator/(const DataArray& lhs, const Measurement& rhs);

REL_RUNTIME_API DataArray operator+(const Measurement& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator-(const Measurement& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator*(const Measurement& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator/(const Measurement& lhs, const DataArray& rhs);

// -- DataArray: comparison (AA, AM, MA)
REL_RUNTIME_API DataArray operator==(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator!=(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator<(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator>(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator<=(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator>=(const DataArray& lhs, const DataArray& rhs);

REL_RUNTIME_API DataArray operator==(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator!=(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator<(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator>(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator<=(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator>=(const DataArray& lhs, const Measurement& rhs);

REL_RUNTIME_API DataArray operator==(const Measurement& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator!=(const Measurement& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator<(const Measurement& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator>(const Measurement& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator<=(const Measurement& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator>=(const Measurement& lhs, const DataArray& rhs);

// -- DataArray: logical (AA, AM, MA)
REL_RUNTIME_API DataArray operator&&(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator||(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator&&(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator||(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator&&(const Measurement& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator||(const Measurement& lhs, const DataArray& rhs);

// -- DataArray: bitwise (AA, AM, MA)
REL_RUNTIME_API DataArray operator&(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator|(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator^(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator&(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator|(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator^(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator&(const Measurement& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator|(const Measurement& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator^(const Measurement& lhs, const DataArray& rhs);

// -- DataArray: shift (AA, AM, MA)
REL_RUNTIME_API DataArray operator<<(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator>>(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator<<(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator>>(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator<<(const Measurement& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator>>(const Measurement& lhs, const DataArray& rhs);

// -- DataArray: modulo (AA, AM, MA)
REL_RUNTIME_API DataArray operator%(const DataArray& lhs, const DataArray& rhs);
REL_RUNTIME_API DataArray operator%(const DataArray& lhs, const Measurement& rhs);
REL_RUNTIME_API DataArray operator%(const Measurement& lhs, const DataArray& rhs);

// -- DataArray: unary
REL_RUNTIME_API DataArray operator-(const DataArray& v);
REL_RUNTIME_API DataArray operator!(const DataArray& v);
REL_RUNTIME_API DataArray operator~(const DataArray& v);

// -- DataArray: pow
REL_RUNTIME_API DataArray pow(const DataArray& base, const DataArray& exponent);
REL_RUNTIME_API DataArray pow(const DataArray& base, const Measurement& exponent);
REL_RUNTIME_API DataArray pow(const Measurement& base, const DataArray& exponent);

// =========================================================================
//  Operation kernels (delegate targets for the operators above)
// =========================================================================

// =========================================================================
// Binary arithmetic
// =========================================================================

REL_RUNTIME_API Value OperationAdd(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationSub(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationMul(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationDiv(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationMod(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationPow(const Value& lhs, const Value& rhs);

// =========================================================================
// Unary
// =========================================================================

REL_RUNTIME_API Value OperationNegate(const Value& v);
REL_RUNTIME_API Value OperationNot(const Value& v);
REL_RUNTIME_API Value OperationBitNot(const Value& v);

// =========================================================================
// Comparison (result is Integer 0/1, dimensionless)
// =========================================================================

REL_RUNTIME_API Value OperationEq(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationNeq(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationLt(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationGt(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationLe(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationGe(const Value& lhs, const Value& rhs);

// =========================================================================
// Bitwise (Integer only, dimensionless)
// =========================================================================

REL_RUNTIME_API Value OperationBitAnd(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationBitOr(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationBitXor(const Value& lhs, const Value& rhs);

// =========================================================================
// Shift (Integer only)
// =========================================================================

REL_RUNTIME_API Value OperationShl(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationShr(const Value& lhs, const Value& rhs);

// =========================================================================
// Logical (result is Integer 0/1, dimensionless)
// =========================================================================

REL_RUNTIME_API Value OperationAnd(const Value& lhs, const Value& rhs);
REL_RUNTIME_API Value OperationOr(const Value& lhs, const Value& rhs);

// =========================================================================
// Ternary
// =========================================================================

/// Conditional(condition, true_value, false_value) �?ternary operator.
/// condition is evaluated as logical (non-zero �?1, zero �?0).
/// For each element, if condition is 1 the result is taken from true_value,
/// otherwise from false_value.  Supports row broadcast and shape broadcast.
REL_RUNTIME_API Value OperationConditional(const Value& condition,
                                         const Value& true_value,
                                         const Value& false_value);
/// If(cond0, val0, cond1, val1, ..., cond_{n-1}, val_{n-1}, else_val)
/// �?multi-branch if/elseif/else.  Takes 2n+1 operands (n >= 1).
/// For each element, the first branch whose condition is non-zero provides
/// the result; if no condition matches, the final else_val is used.
/// This generalizes Conditional to an arbitrary number of branches.
REL_RUNTIME_API Value OperationIf(const std::vector<Value>& operands);
// =========================================================================
// Variadic generators
// =========================================================================

/// Matrix {} �?stack operands with row broadcast.
REL_RUNTIME_API Value OperationMatrix(const std::vector<Value>& operands);

/// Sweep [] �?collect operands into a DataArray (one row per operand).
REL_RUNTIME_API Value OperationSweep(const std::vector<Value>& operands);

}  // namespace rel

#endif  // REL_RUNTIME_OPERATION_H
