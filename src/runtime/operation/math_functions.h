#ifndef REL_RUNTIME_OPERATION_MATH_FUNCTIONS_H
#define REL_RUNTIME_OPERATION_MATH_FUNCTIONS_H

#include "rel_runtime_api.h"
#include "value.h"

namespace rel {
namespace operation {

// =========================================================================
//  Math function operation wrappers (public API, called from math_library)
// =========================================================================

// Trigonometric
REL_RUNTIME_API Value OperationSin (const Value& v);
REL_RUNTIME_API Value OperationCos (const Value& v);
REL_RUNTIME_API Value OperationTan (const Value& v);
// Inverse trigonometric
REL_RUNTIME_API Value OperationAsin(const Value& v);
REL_RUNTIME_API Value OperationAcos(const Value& v);
REL_RUNTIME_API Value OperationAtan(const Value& v);
// Hyperbolic
REL_RUNTIME_API Value OperationSinh(const Value& v);
REL_RUNTIME_API Value OperationCosh(const Value& v);
REL_RUNTIME_API Value OperationTanh(const Value& v);
// Inverse hyperbolic
REL_RUNTIME_API Value OperationAsinh(const Value& v);
REL_RUNTIME_API Value OperationAcosh(const Value& v);
REL_RUNTIME_API Value OperationAtanh(const Value& v);
// Log / exp
REL_RUNTIME_API Value OperationLog  (const Value& v);
REL_RUNTIME_API Value OperationLog10(const Value& v);
REL_RUNTIME_API Value OperationExp  (const Value& v);
// Power
REL_RUNTIME_API Value OperationSqrt(const Value& v);
REL_RUNTIME_API Value OperationSqr (const Value& v);
// Absolute / sign
REL_RUNTIME_API Value OperationAbs(const Value& v);
REL_RUNTIME_API Value OperationSgn(const Value& v);
// Complex
REL_RUNTIME_API Value OperationReal (const Value& v);
REL_RUNTIME_API Value OperationImag (const Value& v);
REL_RUNTIME_API Value OperationConj (const Value& v);
REL_RUNTIME_API Value OperationPhase(const Value& v);

// Binary math
REL_RUNTIME_API Value OperationAtan2(const Value& y, const Value& x);
REL_RUNTIME_API Value OperationRoot (const Value& x, const Value& n);

}  // namespace operation
}  // namespace rel

#endif  // REL_RUNTIME_OPERATION_MATH_FUNCTIONS_H
