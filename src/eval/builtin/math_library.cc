// Math library: element-wise unary math functions.
//
// Each function accepts either:
//   - a DataArray  -> mapped row-by-row through DataArray::transform, or
//   - a Measurement -> mapped directly.
//
// Every cell is mapped element-wise through Measurement::transform, which
// preserves the cell shape and unit.  The cell's data type is dispatched
// with an explicit switch:
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

namespace rel
{
    namespace
    {
        // ---- per-function measurement mappers -----------------------------
        //
        //  Each xxx_measurement() applies one C math function to every scalar
        //  element of a Measurement.  A switch on the cell's data type picks
        //  the matching typed callback; Measurement::transform keeps the
        //  cell's shape and unit and produces the output dtype from the
        //  callback's return type.

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

        // ---- function builder ---------------------------------------------

        /// Build a function named `name` that maps its argument through `map`:
        /// DataArrays are mapped row-by-row via DataArray::transform; bare
        /// Measurements are mapped directly.
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
                        // Map every row through the xdataset transform interface.
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

    FunctionLibrary MakeMathLibrary()
    {
        FunctionLibrary lib("math");
        lib.Add(make_unary_math("sin", sin_measurement));
        lib.Add(make_unary_math("cos", cos_measurement));
        lib.Add(make_unary_math("tan", tan_measurement));
        lib.Add(make_unary_math("log", log_measurement));
        lib.Add(make_unary_math("ln", log_measurement));
        lib.Add(make_unary_math("log10", log10_measurement));
        return lib;
    }

} // namespace rel
