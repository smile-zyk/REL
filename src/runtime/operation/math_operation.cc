// =============================================================================
//  REL -- Math functions implemented via the Operate pipeline
// =============================================================================

#include "operation/math_operation.h"
#include "data_array.h"
#include "data_series.h"
#include "operation/operation_helpers.h"
#include "operation/pipeline.h"

#include <cmath>
#include <complex>
#include <stdexcept>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace rel
{
    namespace operation
    {

        using namespace xdataset;

        // =========================================================================
        //  Element-wise math ops
        // =========================================================================

        using C = std::complex<double>;

        namespace
        {

            template <typename T>
            static inline T op_sin(T x)
            {
                return std::sin(x);
            }
            template <typename T>
            static inline T op_cos(T x)
            {
                return std::cos(x);
            }
            template <typename T>
            static inline T op_tan(T x)
            {
                return std::tan(x);
            }
            template <typename T>
            static inline T op_cot(T x)
            {
                return T(1.0) / std::tan(x);
            }
            template <typename T>
            static inline T op_asin(T x)
            {
                return std::asin(x);
            }
            template <typename T>
            static inline T op_acos(T x)
            {
                return std::acos(x);
            }
            template <typename T>
            static inline T op_atan(T x)
            {
                return std::atan(x);
            }
            template <typename T>
            static inline T op_acot(T x)
            {
                return std::atan(T(1.0) / x);
            }
            template <typename T>
            static inline T op_sinh(T x)
            {
                return std::sinh(x);
            }
            template <typename T>
            static inline T op_cosh(T x)
            {
                return std::cosh(x);
            }
            template <typename T>
            static inline T op_tanh(T x)
            {
                return std::tanh(x);
            }
            template <typename T>
            static inline T op_coth(T x)
            {
                return T(1.0) / std::tanh(x);
            }
            template <typename T>
            static inline T op_asinh(T x)
            {
                return std::asinh(x);
            }
            template <typename T>
            static inline T op_acosh(T x)
            {
                return std::acosh(x);
            }
            template <typename T>
            static inline T op_atanh(T x)
            {
                return std::atanh(x);
            }
            template <typename T>
            static inline T op_acoth(T x)
            {
                return std::atanh(T(1.0) / x);
            }
            template <typename T>
            static inline T op_log(T x)
            {
                return std::log(x);
            }
            template <typename T>
            static inline T op_log10(T x)
            {
                return std::log10(x);
            }
            template <typename T>
            static inline T op_exp(T x)
            {
                return std::exp(x);
            }
            template <typename T>
            static inline T op_sqrt(T x)
            {
                return std::sqrt(x);
            }
            template <typename T>
            static inline T op_sqr(T x)
            {
                return x * x;
            }

            template <typename T>
            static inline int op_ceil(T x)
            {
                return static_cast<int>(std::ceil(x));
            }
            template <typename T>
            static inline int op_floor(T x)
            {
                return static_cast<int>(std::floor(x));
            }
            template <typename T>
            static inline int op_round(T x)
            {
                return static_cast<int>(std::round(x));
            }

            template <typename T>
            static inline T op_deg(T x)
            {
                return x * T(180.0 / M_PI);
            }
            template <typename T>
            static inline T op_rad(T x)
            {
                return x * T(M_PI / 180.0);
            }

            // --- T -> double ------------------------------------------------------
            template <typename T>
            static inline double op_abs(T x)
            {
                return std::abs(x);
            }
            template <typename T>
            static inline double op_real(T x)
            {
                return std::real(x);
            }
            template <typename T>
            static inline double op_imag(T x)
            {
                return std::imag(x);
            }
            template <typename T>
            static inline double op_phase(T x);
            template <>
            inline double op_phase<double>(double x)
            {
                return (x >= 0.0) ? 0.0 : M_PI;
            }
            template <>
            inline double op_phase<C>(C x)
            {
                return std::arg(x);
            }

            template <typename T>
            static inline T op_conj(T x)
            {
                return std::conj(x);
            }

            template <typename T>
            static inline T op_sgn(T x);
            template <>
            inline double op_sgn<double>(double x)
            {
                return (x > 0.0) ? 1.0 : (x < 0.0) ? -1.0 : 0.0;
            }
            template <>
            inline C op_sgn<C>(C x)
            {
                double n = std::abs(x);
                return (n == 0.0) ? C(0.0, 0.0) : x / n;
            }

            // --- int / float ------------------------------------------------
            template <typename T>
            static inline int op_int(T x)
            {
                return static_cast<int>(x);
            }
            template <typename T>
            static inline double op_float(T x)
            {
                return static_cast<double>(x);
            }

            // --- sinc / step ------------------------------------------------------
            template <typename T>
            static inline T op_sinc(T x)
            {
                return (std::abs(x) < T(1e-15)) ? T(1.0) : std::sin(x) / x;
            }
            template <>
            inline C op_sinc<C>(C x)
            {
                double a = std::abs(x);
                return (a < 1e-15) ? C(1.0, 0.0) : std::sin(x) / x;
            }
            template <typename T>
            static inline double op_step(T x)
            {
                double r = std::real(x);
                return (r > 0.0) ? 1.0 : (r < 0.0) ? 0.0 : 0.5;
            }
            template <>
            inline double op_step<C>(C x)
            {
                return (std::real(x) > 0.0) ? 1.0 : (std::real(x) < 0.0) ? 0.0 : 0.5;
            }

            // --- db / dbm / dbmtow / wtodbm ---------------------------------------
            template <typename T>
            static inline double op_dbmtow(T x)
            {
                return std::pow(10.0, std::real(x) / 10.0) * 0.001;
            }
            template <typename T>
            static inline double op_wtodbm_real(T x)
            {
                return 10.0 * std::log10(std::real(x) * 1000.0);
            }
            static inline C op_wtodbm_c(C x)
            {
                return C(10.0 * std::log10(std::abs(x) * 1000.0), std::arg(x));
            }

            static inline double op_db(double r_abs, double z1, double z2)
            {
                double z_out = (z2 * z2) / z2;
                double z_in = (z1 * z1) / z1;
                return 20.0 * std::log10(r_abs) - 10.0 * std::log10(z_out / z_in);
            }
            static inline double op_dbm(double v_abs, double z)
            {
                return 20.0 * std::log10(v_abs) - 10.0 * std::log10(z / 50.0) + 10.0;
            }

            // --- binary helpers ---------------------------------------------------
            static inline double op_atan2_d(double y, double x)
            {
                return std::atan2(y, x);
            }
            template <typename T>
            static inline T op_root(T x, T n)
            {
                return std::pow(x, T(1.0) / n);
            }
            template <typename T>
            static inline T op_max2(T a, T b)
            {
                return a > b ? a : b;
            }
            template <typename T>
            static inline T op_min2(T a, T b)
            {
                return a < b ? a : b;
            }
            template <>
            inline C op_max2<C>(C a, C b)
            {
                return std::abs(a) > std::abs(b) ? a : b;
            }
            template <>
            inline C op_min2<C>(C a, C b)
            {
                return std::abs(a) < std::abs(b) ? a : b;
            }

        } // anonymous namespace

        // =========================================================================
        //  Derive callbacks
        // =========================================================================

        static DataType DeriveDtypeMath(const std::vector<DataType>& dtypes)
        {
            DataType dt = dtypes[0];
            if (dt == DataType::kBoolean || dt == DataType::kInteger)
                return DataType::kReal;
            if (dt == DataType::kReal || dt == DataType::kComplex)
                return dt;
            throw std::runtime_error("math function: unsupported type");
        }
        static DataType DeriveDtypeMathRealDown(const std::vector<DataType>& dtypes)
        {
            DataType dt = dtypes[0];
            if (dt == DataType::kBoolean || dt == DataType::kInteger)
                return DataType::kReal;
            if (dt == DataType::kReal || dt == DataType::kComplex)
                return DataType::kReal;
            throw std::runtime_error("math function: unsupported type");
        }
        static DataType DeriveDtypeMathConj(const std::vector<DataType>& dtypes)
        {
            DataType dt = dtypes[0];
            if (dt == DataType::kBoolean)
                return DataType::kInteger;
            return dt;
        }

        static DataType DeriveDtypeAtan2(const std::vector<DataType>& dtypes)
        {
            for (size_t i = 0; i < dtypes.size(); ++i)
            {
                DataType dt = dtypes[i];
                if (dt == DataType::kBoolean)
                    continue;
                if (dt == DataType::kInteger)
                    continue;
                if (dt == DataType::kReal)
                    continue;
                throw std::runtime_error("atan2: arguments must be Integer, Real, or Boolean");
            }
            return DataType::kReal;
        }
        static DataType DeriveDtypeRoot(const std::vector<DataType>& dtypes)
        {
            DataType res = DataType::kReal;
            for (size_t i = 0; i < dtypes.size(); ++i)
            {
                DataType dt = dtypes[i];
                if (dt == DataType::kBoolean)
                    dt = DataType::kInteger;
                if (dt == DataType::kComplex)
                {
                    res = DataType::kComplex;
                    continue;
                }
                if (dt == DataType::kReal)
                    continue;
                if (dt == DataType::kInteger)
                    continue;
                throw std::runtime_error("root: unsupported type");
            }
            return res;
        }

        // =========================================================================
        //  Execute callbacks
        // =========================================================================

        Value ExecuteSin(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sin<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sin<C>);
                default: throw std::invalid_argument("Sin: unsupported dtype");
            }
        }

        Value ExecuteCos(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_cos<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_cos<C>);
                default: throw std::invalid_argument("Cos: unsupported dtype");
            }
        }
        Value ExecuteTan(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_tan<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_tan<C>);
                default: throw std::invalid_argument("Tan: unsupported dtype");
            }
        }
        Value ExecuteCot(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_cot<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_cot<C>);
                default: throw std::invalid_argument("Cot: unsupported dtype");
            }
        }
        Value ExecuteAsin(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_asin<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_asin<C>);
                default: throw std::invalid_argument("Asin: unsupported dtype");
            }
        }
        Value ExecuteAcos(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_acos<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_acos<C>);
                default: throw std::invalid_argument("Acos: unsupported dtype");
            }
        }
        Value ExecuteAtan(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_atan<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_atan<C>);
                default: throw std::invalid_argument("Atan: unsupported dtype");
            }
        }
        Value ExecuteAcot(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_acot<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_acot<C>);
                default: throw std::invalid_argument("Acot: unsupported dtype");
            }
        }
        Value ExecuteSinh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sinh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sinh<C>);
                default: throw std::invalid_argument("Sinh: unsupported dtype");
            }
        }
        Value ExecuteCosh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_cosh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_cosh<C>);
                default: throw std::invalid_argument("Cosh: unsupported dtype");
            }
        }
        Value ExecuteTanh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_tanh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_tanh<C>);
                default: throw std::invalid_argument("Tanh: unsupported dtype");
            }
        }
        Value ExecuteCoth(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_coth<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_coth<C>);
                default: throw std::invalid_argument("Coth: unsupported dtype");
            }
        }
        Value ExecuteAsinh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_asinh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_asinh<C>);
                default: throw std::invalid_argument("Asinh: unsupported dtype");
            }
        }
        Value ExecuteAcosh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_acosh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_acosh<C>);
                default: throw std::invalid_argument("Acosh: unsupported dtype");
            }
        }
        Value ExecuteAtanh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_atanh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_atanh<C>);
                default: throw std::invalid_argument("Atanh: unsupported dtype");
            }
        }
        Value ExecuteAcoth(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_acoth<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_acoth<C>);
                default: throw std::invalid_argument("Acoth: unsupported dtype");
            }
        }
        Value ExecuteLog(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_log<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_log<C>);
                default: throw std::invalid_argument("Log: unsupported dtype");
            }
        }
        Value ExecuteLog10(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_log10<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_log10<C>);
                default: throw std::invalid_argument("Log10: unsupported dtype");
            }
        }
        Value ExecuteExp(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_exp<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_exp<C>);
                default: throw std::invalid_argument("Exp: unsupported dtype");
            }
        }
        Value ExecuteSqrt(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sqrt<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sqrt<C>);
                default: throw std::invalid_argument("Sqrt: unsupported dtype");
            }
        }
        Value ExecuteSqr(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sqr<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sqr<C>);
                default: throw std::invalid_argument("Sqr: unsupported dtype");
            }
        }
        Value ExecuteSgn(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sgn<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sgn<C>);
                default: throw std::invalid_argument("Sgn: unsupported dtype");
            }
        }
        Value ExecuteCeil(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal: return ExecUnaryCT<double, int>(info, ops, op_ceil<double>);
                default: throw std::invalid_argument("Ceil: unsupported dtype");
            }
        }
        Value ExecuteFloor(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal: return ExecUnaryCT<double, int>(info, ops, op_floor<double>);
                default: throw std::invalid_argument("Floor: unsupported dtype");
            }
        }
        Value ExecuteRound(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal: return ExecUnaryCT<double, int>(info, ops, op_round<double>);
                default: throw std::invalid_argument("Round: unsupported dtype");
            }
        }
        Value ExecuteDeg(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_deg<double>);
                default: throw std::invalid_argument("Deg: unsupported dtype");
            }
        }
        Value ExecuteRad(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_rad<double>);
                default: throw std::invalid_argument("Rad: unsupported dtype");
            }
        }

        Value ExecuteStep(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal:
                    return ExecUnaryCT<double, double>(info, ops, op_step<double>);
                case DataType::kComplex: return ExecUnaryCT<C, double>(info, ops, op_step<C>);
                default: throw std::invalid_argument("Step: unsupported dtype");
            }
        }

        Value ExecuteAbs(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_abs<double>);
                case DataType::kComplex: return ExecUnaryCT<C, double>(info, ops, op_abs<C>);
                default: throw std::invalid_argument("abs: unsupported dtype");
            }
        }
        Value ExecuteReal(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_real<double>);
                case DataType::kComplex: return ExecUnaryCT<C, double>(info, ops, op_real<C>);
                default: throw std::invalid_argument("real: unsupported dtype");
            }
        }
        Value ExecuteImag(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_imag<double>);
                case DataType::kComplex: return ExecUnaryCT<C, double>(info, ops, op_imag<C>);
                default: throw std::invalid_argument("imag: unsupported dtype");
            }
        }
        Value ExecutePhase(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_phase<double>);
                case DataType::kComplex: return ExecUnaryCT<C, double>(info, ops, op_phase<C>);
                default: throw std::invalid_argument("phase: unsupported dtype");
            }
        }
        Value ExecuteConj(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kInteger: return ExecUnaryT<int>(info, ops, [](int x) { return x; });
                case DataType::kReal:
                    return ExecUnaryT<double>(info, ops, [](double x) { return x; });
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_conj<C>);
                default: throw std::invalid_argument("conj: unsupported dtype");
            }
        }
        Value ExecuteInt(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal: return ExecUnaryCT<double, int>(info, ops, op_int<double>);
                default: throw std::invalid_argument("int: unsupported dtype");
            }
        }
        Value ExecuteFloat(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger: return ExecUnaryCT<int, double>(info, ops, op_float<int>);
                case DataType::kReal:
                    return ExecUnaryT<double>(info, ops, [](double x) { return x; });
                default: throw std::invalid_argument("float: unsupported dtype");
            }
        }
        Value ExecuteSinc(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal:
                    return ExecUnaryCT<double, double>(info, ops, op_sinc<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sinc<C>);
                default: throw std::invalid_argument("sinc: unsupported dtype");
            }
        }
        Value ExecuteDbmtow(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecUnaryCT<double, double>(info, ops, op_dbmtow<double>);
        }
        Value ExecuteWtodbm(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (ops[0].data_type())
            {
                case DataType::kBoolean:
                case DataType::kInteger:
                case DataType::kReal:
                    return ExecUnaryCT<double, double>(info, ops, op_wtodbm_real<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_wtodbm_c);
                default: throw std::invalid_argument("wtodbm: unsupported dtype");
            }
        }

        // multi-arg callbacks
        Value ExecuteDb(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            auto v_flat = ops[0].template flat_data<double>();
            auto z1_flat = ops[1].template flat_data<double>();
            auto z2_flat = ops[2].template flat_data<double>();
            const double* v_ptr = v_flat.ptr;
            const double* z1_ptr = z1_flat.ptr;
            const double* z2_ptr = z2_flat.ptr;
            Index v_stride = v_flat.stride;
            Index z1_stride = z1_flat.stride;
            Index z2_stride = z2_flat.stride;

            ShapeBroadcastPlan sp = ShapeBroadcastPlan::Make(
                {ops[0].data_shape(), ops[1].data_shape(), ops[2].data_shape()}, info.shape);
            RowBroadcastPlan rp =
                RowBroadcastPlan::Compute({ops[0].rows(), ops[1].rows(), ops[2].rows()});

            auto out_ds = std::unique_ptr<DataSeries>(new DataSeries(DataType::kReal, info.shape));
            out_ds->set_unit(Unit());
            out_ds->resize(static_cast<std::size_t>(info.rows));
            double* out = out_ds->mutable_contiguous_data<double>();

            Index os = sp.result_elements;
            for (Index i = 0; i < info.rows; ++i)
            {
                Index v_off = (rp.broadcast[0] ? 0 : i) * v_stride;
                Index z1_off = (rp.broadcast[1] ? 0 : i) * z1_stride;
                Index z2_off = (rp.broadcast[2] ? 0 : i) * z2_stride;
                Index o_off = i * os;
                for (Index j = 0; j < os; ++j)
                {
                    out[o_off + j] = op_db(std::abs(v_ptr[v_off + sp.MapFlatIndex(j, 0)]),
                                           z1_ptr[z1_off + sp.MapFlatIndex(j, 1)],
                                           z2_ptr[z2_off + sp.MapFlatIndex(j, 2)]);
                }
            }
            if (ops[0].is_measurement() && ops[1].is_measurement() && ops[2].is_measurement())
                return Value(out_ds->measurement_at(0));
            const DataArray* src = &ops[0].as_data_array();
            auto da = std::make_shared<DataArray>(src->clone());
            da->set_data(std::move(*out_ds));
            return Value(da);
        }
        Value ExecuteDbm(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            auto l_flat = ops[0].template flat_data<double>();
            auto r_flat = ops[1].template flat_data<double>();
            const double* l_ptr = l_flat.ptr;
            const double* r_ptr = r_flat.ptr;
            Index ls = l_flat.stride;
            Index rs = r_flat.stride;

            ShapeBroadcastPlan sp =
                ShapeBroadcastPlan::Make({ops[0].data_shape(), ops[1].data_shape()}, info.shape);
            RowBroadcastPlan rp = RowBroadcastPlan::Compute({ops[0].rows(), ops[1].rows()});

            auto out_ds = std::unique_ptr<DataSeries>(new DataSeries(DataType::kReal, info.shape));
            out_ds->set_unit(Unit());
            out_ds->resize(static_cast<std::size_t>(info.rows));
            double* out = out_ds->mutable_contiguous_data<double>();

            Index os = sp.result_elements;
            for (Index i = 0; i < info.rows; ++i)
            {
                Index lo = (rp.broadcast[0] ? 0 : i) * ls;
                Index ro = (rp.broadcast[1] ? 0 : i) * rs;
                Index oo = i * os;
                for (Index j = 0; j < os; ++j)
                {
                    out[oo + j] = op_dbm(std::abs(l_ptr[lo + sp.MapFlatIndex(j, 0)]),
                                         r_ptr[ro + sp.MapFlatIndex(j, 1)]);
                }
            }
            if (ops[0].is_measurement() && ops[1].is_measurement())
                return Value(out_ds->measurement_at(0));
            const DataArray* src = &ops[0].as_data_array();
            auto da = std::make_shared<DataArray>(src->clone());
            da->set_data(std::move(*out_ds));
            return Value(da);
        }

        // binary
        Value ExecuteAtan2(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            return ExecBinaryArithT<double>(info, ops, op_atan2_d);
        }
        Value ExecuteRoot(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecBinaryArithT<double>(info, ops, op_root<double>);
                case DataType::kComplex: return ExecBinaryArithT<C>(info, ops, op_root<C>);
                default: throw std::invalid_argument("root: unsupported dtype");
            }
        }
        Value ExecuteMax2(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecBinaryArithT<double>(info, ops, op_max2<double>);
                case DataType::kInteger: return ExecBinaryArithT<int>(info, ops, op_max2<int>);
                case DataType::kComplex: return ExecBinaryArithT<C>(info, ops, op_max2<C>);
                default: throw std::invalid_argument("max2: unsupported dtype");
            }
        }
        Value ExecuteMin2(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecBinaryArithT<double>(info, ops, op_min2<double>);
                case DataType::kInteger: return ExecBinaryArithT<int>(info, ops, op_min2<int>);
                case DataType::kComplex: return ExecBinaryArithT<C>(info, ops, op_min2<C>);
                default: throw std::invalid_argument("min2: unsupported dtype");
            }
        }

        // =========================================================================
        //  OpTraits definitions
        // =========================================================================

        const OpTraits kOpSin = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeMath,
                                 DeriveUnitFirst,
                                 ExecuteSin};
        const OpTraits kOpCos = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeMath,
                                 DeriveUnitFirst,
                                 ExecuteCos};
        const OpTraits kOpTan = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeMath,
                                 DeriveUnitFirst,
                                 ExecuteTan};
        const OpTraits kOpCot = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeMath,
                                 DeriveUnitFirst,
                                 ExecuteCot};
        const OpTraits kOpAsin = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMath,
                                  DeriveUnitFirst,
                                  ExecuteAsin};
        const OpTraits kOpAcos = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMath,
                                  DeriveUnitFirst,
                                  ExecuteAcos};
        const OpTraits kOpAtan = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMath,
                                  DeriveUnitFirst,
                                  ExecuteAtan};
        const OpTraits kOpAcot = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMath,
                                  DeriveUnitFirst,
                                  ExecuteAcot};
        const OpTraits kOpSinh = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMath,
                                  DeriveUnitFirst,
                                  ExecuteSinh};
        const OpTraits kOpCosh = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMath,
                                  DeriveUnitFirst,
                                  ExecuteCosh};
        const OpTraits kOpTanh = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMath,
                                  DeriveUnitFirst,
                                  ExecuteTanh};
        const OpTraits kOpCoth = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMath,
                                  DeriveUnitFirst,
                                  ExecuteCoth};
        const OpTraits kOpAsinh = {1,
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeMath,
                                   DeriveUnitFirst,
                                   ExecuteAsinh};
        const OpTraits kOpAcosh = {1,
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeMath,
                                   DeriveUnitFirst,
                                   ExecuteAcosh};
        const OpTraits kOpAtanh = {1,
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeMath,
                                   DeriveUnitFirst,
                                   ExecuteAtanh};
        const OpTraits kOpAcoth = {1,
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeMath,
                                   DeriveUnitFirst,
                                   ExecuteAcoth};
        const OpTraits kOpLog = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeMath,
                                 DeriveUnitFirst,
                                 ExecuteLog};
        const OpTraits kOpLog10 = {1,
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeMath,
                                   DeriveUnitFirst,
                                   ExecuteLog10};
        const OpTraits kOpExp = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeMath,
                                 DeriveUnitFirst,
                                 ExecuteExp};
        const OpTraits kOpSqrt = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMath,
                                  DeriveUnitFirst,
                                  ExecuteSqrt};
        const OpTraits kOpSqr = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeMath,
                                 DeriveUnitFirst,
                                 ExecuteSqr};
        const OpTraits kOpSgn = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeMath,
                                 DeriveUnitFirst,
                                 ExecuteSgn};
        const OpTraits kOpCeil = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeForceInt,
                                  DeriveUnitDimless,
                                  ExecuteCeil};
        const OpTraits kOpFloor = {1,
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeForceInt,
                                   DeriveUnitDimless,
                                   ExecuteFloor};
        const OpTraits kOpRound = {1,
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeForceInt,
                                   DeriveUnitDimless,
                                   ExecuteRound};
        const OpTraits kOpDeg = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeMath,
                                 DeriveUnitFirst,
                                 ExecuteDeg};
        const OpTraits kOpRad = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeMath,
                                 DeriveUnitFirst,
                                 ExecuteRad};

        const OpTraits kOpAbs = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeMath,
                                 DeriveUnitFirst,
                                 ExecuteAbs};
        const OpTraits kOpReal = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMathRealDown,
                                  DeriveUnitFirst,
                                  ExecuteReal};
        const OpTraits kOpImag = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMathRealDown,
                                  DeriveUnitFirst,
                                  ExecuteImag};
        const OpTraits kOpPhase = {1,
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeMathRealDown,
                                   DeriveUnitFirst,
                                   ExecutePhase};
        const OpTraits kOpConj = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMathConj,
                                  DeriveUnitFirst,
                                  ExecuteConj};

        const OpTraits kOpInt = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeForceInt,
                                 DeriveUnitDimless,
                                 ExecuteInt};
        const OpTraits kOpFloat = {1,
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeForceReal,
                                   DeriveUnitFirst,
                                   ExecuteFloat};
        const OpTraits kOpSinc = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMath,
                                  DeriveUnitFirst,
                                  ExecuteSinc};
        const OpTraits kOpStep = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeForceReal,
                                  DeriveUnitDimless,
                                  ExecuteStep};

        const OpTraits kOpDbmtow = {1,
                                    DeriveShapeBroadcast,
                                    DeriveRowsBroadcast,
                                    DeriveDtypeForceReal,
                                    DeriveUnitDimless,
                                    ExecuteDbmtow};
        const OpTraits kOpWtodbm = {1,
                                    DeriveShapeBroadcast,
                                    DeriveRowsBroadcast,
                                    DeriveDtypeComplexOrReal,
                                    DeriveUnitDimless,
                                    ExecuteWtodbm};
        const OpTraits kOpDb = {3,
                                DeriveShapeBroadcast,
                                DeriveRowsBroadcast,
                                DeriveDtypeForceReal,
                                DeriveUnitForceDimless,
                                ExecuteDb};
        const OpTraits kOpDbm = {2,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeForceReal,
                                 DeriveUnitForceDimless,
                                 ExecuteDbm};

        const OpTraits kOpAtan2 = {2,
                                   DeriveShapeBroadcast,
                                   DeriveRowsBroadcast,
                                   DeriveDtypeAtan2,
                                   DeriveUnitSameDim,
                                   ExecuteAtan2};
        const OpTraits kOpRoot = {2,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeRoot,
                                  DeriveUnitFirst,
                                  ExecuteRoot};
        const OpTraits kOpMax2 = {2,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromote,
                                  DeriveUnitSameDim,
                                  ExecuteMax2};
        const OpTraits kOpMin2 = {2,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypePromote,
                                  DeriveUnitSameDim,
                                  ExecuteMin2};

        // =========================================================================
        //  Public API wrappers
        // =========================================================================

        Value OperationSin(const Value& v)
        {
            return Operate({v}, kOpSin);
        }
        Value OperationCos(const Value& v)
        {
            return Operate({v}, kOpCos);
        }
        Value OperationTan(const Value& v)
        {
            return Operate({v}, kOpTan);
        }
        Value OperationCot(const Value& v)
        {
            return Operate({v}, kOpCot);
        }
        Value OperationAsin(const Value& v)
        {
            return Operate({v}, kOpAsin);
        }
        Value OperationAcos(const Value& v)
        {
            return Operate({v}, kOpAcos);
        }
        Value OperationAtan(const Value& v)
        {
            return Operate({v}, kOpAtan);
        }
        Value OperationAcot(const Value& v)
        {
            return Operate({v}, kOpAcot);
        }
        Value OperationSinh(const Value& v)
        {
            return Operate({v}, kOpSinh);
        }
        Value OperationCosh(const Value& v)
        {
            return Operate({v}, kOpCosh);
        }
        Value OperationTanh(const Value& v)
        {
            return Operate({v}, kOpTanh);
        }
        Value OperationCoth(const Value& v)
        {
            return Operate({v}, kOpCoth);
        }
        Value OperationAsinh(const Value& v)
        {
            return Operate({v}, kOpAsinh);
        }
        Value OperationAcosh(const Value& v)
        {
            return Operate({v}, kOpAcosh);
        }
        Value OperationAtanh(const Value& v)
        {
            return Operate({v}, kOpAtanh);
        }
        Value OperationAcoth(const Value& v)
        {
            return Operate({v}, kOpAcoth);
        }
        Value OperationLog(const Value& v)
        {
            return Operate({v}, kOpLog);
        }
        Value OperationLog10(const Value& v)
        {
            return Operate({v}, kOpLog10);
        }
        Value OperationExp(const Value& v)
        {
            return Operate({v}, kOpExp);
        }
        Value OperationSqrt(const Value& v)
        {
            return Operate({v}, kOpSqrt);
        }
        Value OperationSqr(const Value& v)
        {
            return Operate({v}, kOpSqr);
        }
        Value OperationCeil(const Value& v)
        {
            return Operate({v}, kOpCeil);
        }
        Value OperationFloor(const Value& v)
        {
            return Operate({v}, kOpFloor);
        }
        Value OperationRound(const Value& v)
        {
            return Operate({v}, kOpRound);
        }
        Value OperationDeg(const Value& v)
        {
            return Operate({v}, kOpDeg);
        }
        Value OperationRad(const Value& v)
        {
            return Operate({v}, kOpRad);
        }
        Value OperationAbs(const Value& v)
        {
            return Operate({v}, kOpAbs);
        }
        Value OperationSgn(const Value& v)
        {
            return Operate({v}, kOpSgn);
        }
        Value OperationReal(const Value& v)
        {
            return Operate({v}, kOpReal);
        }
        Value OperationImag(const Value& v)
        {
            return Operate({v}, kOpImag);
        }
        Value OperationConj(const Value& v)
        {
            return Operate({v}, kOpConj);
        }
        Value OperationPhase(const Value& v)
        {
            return Operate({v}, kOpPhase);
        }
        Value OperationInt(const Value& v)
        {
            return Operate({v}, kOpInt);
        }
        Value OperationFloat(const Value& v)
        {
            return Operate({v}, kOpFloat);
        }
        Value OperationSinc(const Value& v)
        {
            return Operate({v}, kOpSinc);
        }
        Value OperationStep(const Value& v)
        {
            return Operate({v}, kOpStep);
        }
        Value OperationDbmtow(const Value& v)
        {
            return Operate({v}, kOpDbmtow);
        }
        Value OperationWtodbm(const Value& v)
        {
            return Operate({v}, kOpWtodbm);
        }

        Value OperationAtan2(const Value& a, const Value& b)
        {
            return Operate({a, b}, kOpAtan2);
        }
        Value OperationRoot(const Value& a, const Value& b)
        {
            return Operate({a, b}, kOpRoot);
        }
        Value OperationMax2(const Value& a, const Value& b)
        {
            return Operate({a, b}, kOpMax2);
        }
        Value OperationMin2(const Value& a, const Value& b)
        {
            return Operate({a, b}, kOpMin2);
        }
        Value OperationDb(const Value& a, const Value& b, const Value& c)
        {
            return Operate({a, b, c}, kOpDb);
        }
        Value OperationDbm(const Value& v, const Value& z)
        {
            return Operate({v, z}, kOpDbm);
        }

    } // namespace operation
} // namespace rel
