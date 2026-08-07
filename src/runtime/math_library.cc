// Math library: element-wise unary math functions.
//
// Implemented via the Operate pipeline (operation/pipeline.h /
// operation/math_functions.h).  Each function delegates to
// rel::operation::OperationXxx, which runs through the full
// derive + broadcast + execute flow.
//
// Type promotion: Boolean �?Integer (0/1), Integer �?Real promotion
// is handled by DeriveDtypeMath in the pipeline.

#include "function.h"
#include "operation/math_functions.h"
#include "operation/operator.h"

#include <string>
#include <vector>

namespace rel
{
    namespace
    {
        static Function make_unary_fn(const char* name,
                                       rel::Value (*fn)(const rel::Value&))
        {
            return Function(
                name,
                std::vector<FunctionParam>{ Param("x") },
                [name, fn](const rel::Function::ArgMap& args) -> rel::Value {
                    return fn(args.at("x"));
                });
        }

        static Function make_binary_fn(const char* name,
                                        rel::Value (*fn)(const rel::Value&, const rel::Value&))
        {
            return Function(
                name,
                std::vector<FunctionParam>{ Param("x"), Param("y") },
                [name, fn](const rel::Function::ArgMap& args) -> rel::Value {
                    return fn(args.at("x"), args.at("y"));
                });
        }

    } // namespace

    FunctionLibrary kMathLibrary = [] {
        using namespace rel::operation;

        FunctionLibrary lib("math");

        // Trigonometric
        lib.Add(make_unary_fn("sin",   OperationSin));
        lib.Add(make_unary_fn("cos",   OperationCos));
        lib.Add(make_unary_fn("tan",   OperationTan));

        // Inverse trigonometric
        lib.Add(make_unary_fn("asin",  OperationAsin));
        lib.Add(make_unary_fn("acos",  OperationAcos));
        lib.Add(make_unary_fn("atan",  OperationAtan));

        // Hyperbolic
        lib.Add(make_unary_fn("sinh",  OperationSinh));
        lib.Add(make_unary_fn("cosh",  OperationCosh));
        lib.Add(make_unary_fn("tanh",  OperationTanh));

        // Inverse hyperbolic
        lib.Add(make_unary_fn("asinh", OperationAsinh));
        lib.Add(make_unary_fn("acosh", OperationAcosh));
        lib.Add(make_unary_fn("atanh", OperationAtanh));

        // Logarithms & exponential
        lib.Add(make_unary_fn("log",   OperationLog));
        lib.Add(make_unary_fn("ln",    OperationLog));
        lib.Add(make_unary_fn("log10", OperationLog10));
        lib.Add(make_unary_fn("exp",   OperationExp));

        // Power
        lib.Add(make_unary_fn("sqrt",  OperationSqrt));
        lib.Add(make_unary_fn("sqr",   OperationSqr));

        // Absolute & sign
        lib.Add(make_unary_fn("abs",   OperationAbs));
        lib.Add(make_unary_fn("sgn",   OperationSgn));

        // Complex number operations
        lib.Add(make_unary_fn("real",  OperationReal));
        lib.Add(make_unary_fn("re",    OperationReal));
        lib.Add(make_unary_fn("imag",  OperationImag));
        lib.Add(make_unary_fn("im",    OperationImag));
        lib.Add(make_unary_fn("conj",  OperationConj));
        lib.Add(make_unary_fn("conjg", OperationConj));
        lib.Add(make_unary_fn("mag",   OperationAbs));
        lib.Add(make_unary_fn("phase", OperationPhase));

        // Binary math
        lib.Add(make_binary_fn("pow",   OperationPow));
        lib.Add(make_binary_fn("atan2", OperationAtan2));
        lib.Add(make_binary_fn("root",  OperationRoot));

        return lib;
    }();

}  // namespace rel
