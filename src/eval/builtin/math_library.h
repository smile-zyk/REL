#pragma once

#include "eval/function.h"

namespace rel
{
    /// Build the "math" library — element-wise unary math functions applied
    /// via the xdataset transform interface:
    ///
    ///   - sin(x), cos(x), tan(x)
    ///   - log(x) / ln(x)   (natural logarithm)
    ///   - log10(x)
    ///
    /// Each function accepts a DataArray (mapped row-by-row through
    /// DataArray::transform) or a Measurement.  Scalar, vector, and matrix
    /// cells are mapped element-wise through Measurement::transform, which
    /// preserves shape and unit.  The output dtype follows the input: Real /
    /// Integer cells map to Real, Complex cells map to Complex.  String and
    /// Boolean cells are rejected.
    FunctionLibrary MakeMathLibrary();
}
