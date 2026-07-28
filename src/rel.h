#pragma once

#include "eval/value.h"

#include <string>

namespace rel {

/// Parse and evaluate a single REL expression from a source string.
/// Uses a temporary Environment (no Dataset, no built-in constants).
/// Throws std::runtime_error on parse or evaluation failure.
Value eval(const std::string& source);

} // namespace rel
