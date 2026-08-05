// Math library: element-wise unary math functions.
//
// Each function accepts either:
//   - a DataArray  -> mapped row-by-row through DataArray::transform, or
//   - a Measurement -> mapped directly.
//
// Every cell is mapped element-wise through Measurement::transform, which
// preserves the cell shape.  The cell's data type is dispatched:
//   - Integer -> converted to double; the result is Real
//   - Real    -> the result is Real
//   - Complex -> the result is Complex
// String and Boolean cells have no meaningful math mapping and are rejected.

#include "eval/builtin/math_library.h"

#include <cmath>
#include <complex>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace rel
{
    namespace
    {
        // =================================================================
        //  Unary measurement mappers  (Measurement -> Measurement)
        // =================================================================
        //
        //  Each xxx_measurement() applies one math function to every scalar
        //  element of a Measurement.  A switch on the cell's data type picks
        //  the matching typed callback; Measurement::transform preserves the
        //  cell's shape and infers the output dtype from the callback's
        //  return type.

        // ---- sin ----------------------------------------------------------

        static xdataset::Measurement sin_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::sin(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::sin(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::sin(x); });
                default:
                    throw std::runtime_error("sin: argument must be Integer, Real, or Complex");
            }
        }

        // ---- cos ----------------------------------------------------------

        static xdataset::Measurement cos_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::cos(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::cos(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::cos(x); });
                default:
                    throw std::runtime_error("cos: argument must be Integer, Real, or Complex");
            }
        }

        // ---- tan ----------------------------------------------------------

        static xdataset::Measurement tan_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::tan(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::tan(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::tan(x); });
                default:
                    throw std::runtime_error("tan: argument must be Integer, Real, or Complex");
            }
        }

        // ---- asin ---------------------------------------------------------

        static xdataset::Measurement asin_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::asin(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::asin(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::asin(x); });
                default:
                    throw std::runtime_error("asin: argument must be Integer, Real, or Complex");
            }
        }

        // ---- acos ---------------------------------------------------------

        static xdataset::Measurement acos_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::acos(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::acos(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::acos(x); });
                default:
                    throw std::runtime_error("acos: argument must be Integer, Real, or Complex");
            }
        }

        // ---- atan ---------------------------------------------------------

        static xdataset::Measurement atan_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::atan(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::atan(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::atan(x); });
                default:
                    throw std::runtime_error("atan: argument must be Integer, Real, or Complex");
            }
        }

        // ---- sinh ---------------------------------------------------------

        static xdataset::Measurement sinh_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::sinh(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::sinh(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::sinh(x); });
                default:
                    throw std::runtime_error("sinh: argument must be Integer, Real, or Complex");
            }
        }

        // ---- cosh ---------------------------------------------------------

        static xdataset::Measurement cosh_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::cosh(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::cosh(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::cosh(x); });
                default:
                    throw std::runtime_error("cosh: argument must be Integer, Real, or Complex");
            }
        }

        // ---- tanh ---------------------------------------------------------

        static xdataset::Measurement tanh_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::tanh(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::tanh(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::tanh(x); });
                default:
                    throw std::runtime_error("tanh: argument must be Integer, Real, or Complex");
            }
        }

        // ---- asinh --------------------------------------------------------

        static xdataset::Measurement asinh_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::asinh(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::asinh(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::asinh(x); });
                default:
                    throw std::runtime_error("asinh: argument must be Integer, Real, or Complex");
            }
        }

        // ---- acosh --------------------------------------------------------

        static xdataset::Measurement acosh_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::acosh(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::acosh(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::acosh(x); });
                default:
                    throw std::runtime_error("acosh: argument must be Integer, Real, or Complex");
            }
        }

        // ---- atanh --------------------------------------------------------

        static xdataset::Measurement atanh_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::atanh(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::atanh(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::atanh(x); });
                default:
                    throw std::runtime_error("atanh: argument must be Integer, Real, or Complex");
            }
        }

        // ---- log / ln -----------------------------------------------------

        static xdataset::Measurement log_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::log(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::log(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::log(x); });
                default:
                    throw std::runtime_error("log: argument must be Integer, Real, or Complex");
            }
        }

        // ---- log10 --------------------------------------------------------

        static xdataset::Measurement log10_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::log10(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::log10(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::log10(x); });
                default:
                    throw std::runtime_error("log10: argument must be Integer, Real, or Complex");
            }
        }

        // ---- exp ----------------------------------------------------------

        static xdataset::Measurement exp_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::exp(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::exp(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::exp(x); });
                default:
                    throw std::runtime_error("exp: argument must be Integer, Real, or Complex");
            }
        }

        // ---- sqrt ---------------------------------------------------------

        static xdataset::Measurement sqrt_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::sqrt(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::sqrt(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::sqrt(x); });
                default:
                    throw std::runtime_error("sqrt: argument must be Integer, Real, or Complex");
            }
        }

        // ---- sqr (square) -------------------------------------------------

        static xdataset::Measurement sqr_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { double d = static_cast<double>(x); return d * d; });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return x * x; });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return x * x; });
                default:
                    throw std::runtime_error("sqr: argument must be Integer, Real, or Complex");
            }
        }

        // ---- abs ----------------------------------------------------------

        static xdataset::Measurement abs_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) { return std::abs(static_cast<double>(x)); });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) { return std::abs(x); });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::abs(x); });
                default:
                    throw std::runtime_error("abs: argument must be Integer, Real, or Complex");
            }
        }

        // ---- sgn (sign extraction) ----------------------------------------

        static xdataset::Measurement sgn_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) -> double {
                        return (x > 0) ? 1.0 : (x < 0) ? -1.0 : 0.0;
                    });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) -> double {
                        return (x > 0.0) ? 1.0 : (x < 0.0) ? -1.0 : 0.0;
                    });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) -> std::complex<double> {
                        double n = std::abs(x);
                        return (n == 0.0) ? std::complex<double>(0.0, 0.0) : x / n;
                    });
                default:
                    throw std::runtime_error("sgn: argument must be Integer, Real, or Complex");
            }
        }

        // ---- real / re ----------------------------------------------------

        static xdataset::Measurement real_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) -> double { return static_cast<double>(x); });
                case xdataset::DataType::kReal:
                    return m;  // identity
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) -> double { return std::real(x); });
                default:
                    throw std::runtime_error("real: argument must be Integer, Real, or Complex");
            }
        }

        // ---- imag / im ----------------------------------------------------

        static xdataset::Measurement imag_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int) -> double { return 0.0; });
                case xdataset::DataType::kReal:
                    return m.transform([](double) -> double { return 0.0; });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) -> double { return std::imag(x); });
                default:
                    throw std::runtime_error("imag: argument must be Integer, Real, or Complex");
            }
        }

        // ---- conj / conjg ------------------------------------------------

        static xdataset::Measurement conj_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                case xdataset::DataType::kReal:
                    return m;  // identity for real numbers
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) { return std::conj(x); });
                default:
                    throw std::runtime_error("conj: argument must be Integer, Real, or Complex");
            }
        }

        // ---- mag (same as abs) --------------------------------------------

        static xdataset::Measurement mag_measurement(const xdataset::Measurement& m)
        {
            return abs_measurement(m);
        }

        // ---- phase --------------------------------------------------------

        static xdataset::Measurement phase_measurement(const xdataset::Measurement& m)
        {
            switch (m.data_type())
            {
                case xdataset::DataType::kInteger:
                    return m.transform([](int x) -> double { return (x >= 0) ? 0.0 : M_PI; });
                case xdataset::DataType::kReal:
                    return m.transform([](double x) -> double { return (x >= 0.0) ? 0.0 : M_PI; });
                case xdataset::DataType::kComplex:
                    return m.transform([](const std::complex<double>& x) -> double { return std::arg(x); });
                default:
                    throw std::runtime_error("phase: argument must be Integer, Real, or Complex");
            }
        }

        // =================================================================
        //  Function builder
        // =================================================================

        /// Build a unary function named `name` that maps its argument through
        /// `map`: DataArrays are mapped row-by-row via DataArray::transform;
        /// bare Measurements are mapped directly.
        static Function make_unary_math(const char* name,
                                        xdataset::Measurement (*map)(const xdataset::Measurement&))
        {
            return Function(
                name,
                std::vector<FunctionParam>{ Param("x") },
                [name, map](const std::vector<xdataset::Value>& args) -> xdataset::Value {
                    const xdataset::Value& v = args[0];

                    if (v.is_data_array())
                    {
                        xdataset::DataArray out = v.as_data_array().transform(map);
                        return xdataset::Value(out);
                    }

                    if (v.is_measurement())
                        return xdataset::Value(map(v.as_measurement()));

                    throw std::runtime_error(std::string(name) +
                                             ": argument must be a DataArray or Measurement");
                });
        }

    } // namespace

    // =====================================================================
    //  Library assembly
    // =====================================================================

    FunctionLibrary MakeMathLibrary()
    {
        FunctionLibrary lib("math");

        // Trigonometric
        lib.Add(make_unary_math("sin",   sin_measurement));
        lib.Add(make_unary_math("cos",   cos_measurement));
        lib.Add(make_unary_math("tan",   tan_measurement));

        // Inverse trigonometric
        lib.Add(make_unary_math("asin",  asin_measurement));
        lib.Add(make_unary_math("acos",  acos_measurement));
        lib.Add(make_unary_math("atan",  atan_measurement));

        // Hyperbolic
        lib.Add(make_unary_math("sinh",  sinh_measurement));
        lib.Add(make_unary_math("cosh",  cosh_measurement));
        lib.Add(make_unary_math("tanh",  tanh_measurement));

        // Inverse hyperbolic
        lib.Add(make_unary_math("asinh", asinh_measurement));
        lib.Add(make_unary_math("acosh", acosh_measurement));
        lib.Add(make_unary_math("atanh", atanh_measurement));

        // Logarithms & exponential
        lib.Add(make_unary_math("log",   log_measurement));
        lib.Add(make_unary_math("ln",    log_measurement));
        lib.Add(make_unary_math("log10", log10_measurement));
        lib.Add(make_unary_math("exp",   exp_measurement));

        // Power
        lib.Add(make_unary_math("sqrt",  sqrt_measurement));
        lib.Add(make_unary_math("sqr",   sqr_measurement));

        // Absolute & sign
        lib.Add(make_unary_math("abs",   abs_measurement));
        lib.Add(make_unary_math("sgn",   sgn_measurement));

        // Complex number operations
        lib.Add(make_unary_math("real",  real_measurement));
        lib.Add(make_unary_math("re",    real_measurement));
        lib.Add(make_unary_math("imag",  imag_measurement));
        lib.Add(make_unary_math("im",    imag_measurement));
        lib.Add(make_unary_math("conj",  conj_measurement));
        lib.Add(make_unary_math("conjg", conj_measurement));
        lib.Add(make_unary_math("mag",   mag_measurement));
        lib.Add(make_unary_math("phase", phase_measurement));

        return lib;
    }

} // namespace rel
