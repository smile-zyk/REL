#pragma once

#include "eval/function.h"

namespace rel
{
    class Environment;

    /// Build the "builtin" library — runtime introspection and DataArray
    /// utilities:
    ///
    ///   - datasets()              list every registered Dataset
    ///   - default_dataset()       name of the current default Dataset
    ///   - variables()             list every registered variable
    ///   - what(x)                 inspect a Value (kind / dimension / shape / type)
    ///   - indep(da, selector=1)   extract an independent variable from a DataArray
    ///   - min(da) / max(da)       reduce along the innermost dimension
    ///   - output(da, name="data") write a DataArray's DataFrame view to CSV
    ///
    /// The introspection functions hold a reference to `env`, so `env` must
    /// stay in place (not be moved) for as long as the functions remain
    /// registered.
    FunctionLibrary MakeBuiltinLibrary(Environment& env);
}
