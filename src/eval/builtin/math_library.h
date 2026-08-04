#pragma once

#include "eval/function.h"

namespace rel
{
    /// Build the "math" library — element-wise unary math functions applied
    /// via the xdataset transform interface.
    ///
    ///   Trigonometric:      sin, cos, tan
    ///   Inverse trig:       asin, acos, atan
    ///   Hyperbolic:         sinh, cosh, tanh
    ///   Inverse hyperbolic: asinh, acosh, atanh
    ///   Logarithms:         log / ln  (natural),  log10
    ///   Exponential:        exp
    ///   Power:              sqrt, sqr (square)
    ///   Other:              abs, sgn
    ///   Complex:            real / re, imag / im, conj / conjg, mag, phase
    ///
    /// Each function accepts a DataArray (mapped row-by-row through
    /// DataArray::transform) or a Measurement.  Scalar, vector, and matrix
    /// cells are mapped element-wise through Measurement::transform, which
    /// preserves shape.  Integer -> Real promotion; String/Boolean rejected.
    FunctionLibrary MakeMathLibrary();
}
