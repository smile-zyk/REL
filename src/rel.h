#pragma once

#include "eval/function.h"  // FunctionParam / NativeFunction
#include "value.h"          // xdataset::Value

#include <string>
#include <vector>

namespace rel {

class Environment;

/// Convenience alias — REL uses xdataset::Value directly.
using Value = xdataset::Value;

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

} // namespace rel
