// Builtin function libraries — registration entry point.
//
// All builtin function implementations live in src/eval/builtin/:
//   - builtin_library.cc  -> "builtin" library (datasets, default_dataset,
//                            variables, what, indep, min, max, output).
//   - math_library.cc     -> "math" library (sin, cos, tan, log, ln, log10).
//
// This translation unit only wires them onto an Environment.

#include "rel.h"

#include "eval/builtin/builtin_library.h"
#include "eval/builtin/math_library.h"
#include "eval/environment.h"

namespace rel
{
    void InitBuiltinFunctions(Environment& env)
    {
        env.RegisterLibrary(MakeBuiltinLibrary(env));
        env.RegisterLibrary(MakeMathLibrary());
    }

} // namespace rel
