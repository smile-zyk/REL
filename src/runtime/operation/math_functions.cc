// =============================================================================
//  REL -- Math functions implemented via the Operate pipeline
// =============================================================================

#include "operation/math_functions.h"
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
        //  Element-wise math ops (T -> T, works for double and complex)
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

            // T -> double (abs, real, imag, phase)
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

            // T -> T (conj)
            template <typename T>
            static inline T op_conj(T x)
            {
                return std::conj(x);
            }

            // T -> T (sgn)
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

        } // anonymous namespace

        // =========================================================================
        //  Derive callbacks for unary math
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

        // DeriveDtypeMathRealDown: complex→real, real→real, int→real
        static DataType DeriveDtypeMathRealDown(const std::vector<DataType>& dtypes)
        {
            DataType dt = dtypes[0];
            if (dt == DataType::kBoolean || dt == DataType::kInteger)
                return DataType::kReal;
            if (dt == DataType::kReal || dt == DataType::kComplex)
                return DataType::kReal;
            throw std::runtime_error("math function: unsupported type");
        }

        // DeriveDtypeMathConj: complex→complex, real→real, int→int
        static DataType DeriveDtypeMathConj(const std::vector<DataType>& dtypes)
        {
            DataType dt = dtypes[0];
            if (dt == DataType::kBoolean)
                return DataType::kInteger;
            return dt;
        }

        // =========================================================================
        //  Execute callbacks -- standard math (Real / Complex dispatch)
        // =========================================================================

        Value ExecuteSin(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sin<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sin<C>);
                default: throw std::invalid_argument("sin: unsupported dtype");
            }
        }
        Value ExecuteCos(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_cos<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_cos<C>);
                default: throw std::invalid_argument("cos: unsupported dtype");
            }
        }
        Value ExecuteTan(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_tan<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_tan<C>);
                default: throw std::invalid_argument("tan: unsupported dtype");
            }
        }
        Value ExecuteAsin(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_asin<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_asin<C>);
                default: throw std::invalid_argument("asin: unsupported dtype");
            }
        }
        Value ExecuteAcos(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_acos<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_acos<C>);
                default: throw std::invalid_argument("acos: unsupported dtype");
            }
        }
        Value ExecuteAtan(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_atan<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_atan<C>);
                default: throw std::invalid_argument("atan: unsupported dtype");
            }
        }
        Value ExecuteSinh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sinh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sinh<C>);
                default: throw std::invalid_argument("sinh: unsupported dtype");
            }
        }
        Value ExecuteCosh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_cosh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_cosh<C>);
                default: throw std::invalid_argument("cosh: unsupported dtype");
            }
        }
        Value ExecuteTanh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_tanh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_tanh<C>);
                default: throw std::invalid_argument("tanh: unsupported dtype");
            }
        }
        Value ExecuteAsinh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_asinh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_asinh<C>);
                default: throw std::invalid_argument("asinh: unsupported dtype");
            }
        }
        Value ExecuteAcosh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_acosh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_acosh<C>);
                default: throw std::invalid_argument("acosh: unsupported dtype");
            }
        }
        Value ExecuteAtanh(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_atanh<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_atanh<C>);
                default: throw std::invalid_argument("atanh: unsupported dtype");
            }
        }
        Value ExecuteLog(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_log<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_log<C>);
                default: throw std::invalid_argument("log: unsupported dtype");
            }
        }
        Value ExecuteLog10(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_log10<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_log10<C>);
                default: throw std::invalid_argument("log10: unsupported dtype");
            }
        }
        Value ExecuteExp(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_exp<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_exp<C>);
                default: throw std::invalid_argument("exp: unsupported dtype");
            }
        }
        Value ExecuteSqrt(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sqrt<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sqrt<C>);
                default: throw std::invalid_argument("sqrt: unsupported dtype");
            }
        }
        Value ExecuteSqr(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sqr<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sqr<C>);
                default: throw std::invalid_argument("sqr: unsupported dtype");
            }
        }

        Value ExecuteSgn(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_sgn<double>);
                case DataType::kComplex: return ExecUnaryT<C>(info, ops, op_sgn<C>);
                default: throw std::invalid_argument("sgn: unsupported dtype");
            }
        }

        Value ExecuteAbs(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_abs<double>);
                case DataType::kComplex: return ExecUnaryCT<C, double>(info, ops, op_abs<C>);
                default: throw std::invalid_argument("abs: unsupported dtype");
            }
        }

        Value ExecuteReal(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_real<double>);
                case DataType::kComplex: return ExecUnaryCT<C, double>(info, ops, op_real<C>);
                default: throw std::invalid_argument("real: unsupported dtype");
            }
        }

        Value ExecuteImag(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_imag<double>);
                case DataType::kComplex: return ExecUnaryCT<C, double>(info, ops, op_imag<C>);
                default: throw std::invalid_argument("imag: unsupported dtype");
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

        Value ExecutePhase(const ExecContextInfo& info, const std::vector<Value>& ops)
        {
            switch (info.dtype)
            {
                case DataType::kReal: return ExecUnaryT<double>(info, ops, op_phase<double>);
                case DataType::kComplex: return ExecUnaryCT<C, double>(info, ops, op_phase<C>);
                default: throw std::invalid_argument("phase: unsupported dtype");
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

        const OpTraits kOpAbs = {1,
                                 DeriveShapeBroadcast,
                                 DeriveRowsBroadcast,
                                 DeriveDtypeMath,
                                 DeriveUnitFirst,
                                 ExecuteAbs};

        // real / imag �?complex→real downcast
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

        // conj �?complex→complex, real→real, int→int (type-preserving)
        const OpTraits kOpConj = {1,
                                  DeriveShapeBroadcast,
                                  DeriveRowsBroadcast,
                                  DeriveDtypeMathConj,
                                  DeriveUnitFirst,
                                  ExecuteConj};

        // =========================================================================
        //  Binary math �?element ops
        // =========================================================================

        static inline double op_atan2_d(double y, double x)
        {
            return std::atan2(y, x);
        }

        template <typename T>
        static inline T op_root(T x, T n)
        {
            return std::pow(x, T(1.0) / n);
        }

        // =========================================================================
        //  Binary math �?derive callbacks
        // =========================================================================

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
        //  Binary math �?execute callbacks
        // =========================================================================

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

        // =========================================================================
        //  Binary math �?OpTraits
        // =========================================================================

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

        Value OperationAtan2(const Value& y, const Value& x)
        {
            return Operate({y, x}, kOpAtan2);
        }
        Value OperationRoot(const Value& x, const Value& n)
        {
            return Operate({x, n}, kOpRoot);
        }

    } // namespace operation
} // namespace rel
