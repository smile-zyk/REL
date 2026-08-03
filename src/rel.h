#pragma once

#include "eval/function.h"  // FunctionParam / NativeFunction
#include "value.h"          // xdataset::Value

#include <sstream>
#include <string>
#include <vector>

namespace rel {

class Environment;

/// Convenience alias — REL uses xdataset::Value directly.
using Value = xdataset::Value;

/// Opaque handle to a loaded function plugin (see LoadFunctionPlugin).
struct LoadedPlugin;

/// Parse and evaluate a single REL expression from a source string.
/// When `env` is nullptr (the default), a temporary Environment is used.
/// Otherwise the given Environment is used (with its variables, datasets,
/// and built-in constants).
/// Throws std::runtime_error on parse or evaluation failure.
Value Eval(const std::string& source, Environment* env = nullptr);

/// Populate `env` with REL's built-in numeric constants (PI, e, etc.).
void InitBuiltinConstants(Environment& env);

/// Register REL's builtin functions on `env` (runtime introspection, e.g.
/// print_datasets / print_dataset / print_variables).
/// The registered functions hold a reference to `env`, so `env` must stay in
/// place (not be moved) for as long as the functions remain registered.
void InitBuiltinFunctions(Environment& env);

/// Register a custom function on `env`.
///
/// Parameters may carry default values (see FunctionParam); call sites may
/// omit any parameter slot, and omitted slots are filled with the declared
/// default.  Defaults do not have to be trailing.
void RegisterFunction(Environment& env,
                      std::string name,
                      std::vector<FunctionParam> params,
                      NativeFunction impl);

/// Load a function plugin (DLL / .so / .dylib) and register its functions
/// on `env`.  Returns an opaque handle, or nullptr on failure.
LoadedPlugin* LoadFunctionPlugin(Environment& env, const std::string& path);

/// Unload a plugin previously returned by LoadFunctionPlugin.
/// Unregisters the functions the plugin registered (so `env` no longer
/// references code inside the plugin), then releases the library.
/// The handle must be unloaded before its Environment is destroyed.
void UnloadFunctionPlugin(LoadedPlugin* plugin);

// =========================================================================
//  Value formatting helpers
// =========================================================================
//
//  Shared by REL's builtins (e.g. what(x)) and by hosts that want the same
//  text rendering of xdataset values.  All are inline so they are usable
//  from any translation unit without a rel_core link.

/// Render a MultiDimensionSpec as "[1, 2, [1, 2, 3]]" — regular dimensions
/// print their size, ragged dimensions nest their sizes.
inline std::string FormatDimensionSpec(const xdataset::MultiDimensionSpec& spec)
{
    std::ostringstream oss;
    oss << '[';
    const std::vector<xdataset::DimensionSpec>& dims = spec.dims();
    for (std::size_t i = 0; i < dims.size(); ++i)
    {
        if (i > 0) oss << ", ";
        const xdataset::DimensionSpec& d = dims[i];
        if (d.is_regular())
        {
            oss << d.regular_size();
        }
        else
        {
            oss << '[';
            const std::vector<std::size_t>& sizes = d.ragged_sizes();
            for (std::size_t j = 0; j < sizes.size(); ++j)
            {
                if (j > 0) oss << ", ";
                oss << sizes[j];
            }
            oss << ']';
        }
    }
    oss << ']';
    return oss.str();
}

/// Render a cell shape as "Scalar" / "Vector(w)" / "Matrix(r, c)".
inline std::string FormatDataShape(xdataset::DataKind kind,
                                   const xdataset::DataShape& shape)
{
    std::ostringstream oss;
    switch (kind)
    {
        case xdataset::DataKind::kScalar:
            oss << "Scalar";
            break;
        case xdataset::DataKind::kVector:
            oss << "Vector(" << shape[0] << ")";
            break;
        case xdataset::DataKind::kMatrix:
            oss << "Matrix(" << shape[0] << ", " << shape[1] << ")";
            break;
    }
    return oss.str();
}

/// Render a cell data type as Integer / Double / Complex / String / Boolean.
inline std::string FormatDataType(xdataset::DataType type)
{
    switch (type)
    {
        case xdataset::DataType::kInteger: return "Integer";
        case xdataset::DataType::kReal:    return "Double";
        case xdataset::DataType::kComplex: return "Complex";
        case xdataset::DataType::kString:  return "String";
        case xdataset::DataType::kBoolean: return "Boolean";
    }
    return "Unknown";
}

} // namespace rel
