#pragma once

#include "value.h"  // xdataset::Value

#include <string>

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

} // namespace rel
