// =============================================================================
//  REL -- Math functions implemented via the Operate pipeline
// =============================================================================

#include "operation/math_functions.h"
#include "operation/pipeline.h"
#include "operation/exec_helpers.h"
#include "data_series.h"
#include "data_array.h"

#include <cmath>
#include <complex>
#include <stdexcept>
#include <vector>

namespace rel {
namespace operation {

using namespace xdataset;

// =========================================================================
//  Element-wise math ops (double path �?used for Integer and Real)
// =========================================================================

static inline double op_sin_d  (double x) { return std::sin(x); }
static inline double op_cos_d  (double x) { return std::cos(x); }
static inline double op_tan_d  (double x) { return std::tan(x); }
static inline double op_asin_d (double x) { return std::asin(x); }
static inline double op_acos_d (double x) { return std::acos(x); }
static inline double op_atan_d (double x) { return std::atan(x); }
static inline double op_sinh_d (double x) { return std::sinh(x); }
static inline double op_cosh_d (double x) { return std::cosh(x); }
static inline double op_tanh_d (double x) { return std::tanh(x); }
static inline double op_asinh_d(double x) { return std::asinh(x); }
static inline double op_acosh_d(double x) { return std::acosh(x); }
static inline double op_atanh_d(double x) { return std::atanh(x); }
static inline double op_log_d  (double x) { return std::log(x); }
static inline double op_log10_d(double x) { return std::log10(x); }
static inline double op_exp_d  (double x) { return std::exp(x); }
static inline double op_sqrt_d (double x) { return std::sqrt(x); }
static inline double op_sqr_d  (double x) { return x * x; }
static inline double op_abs_d  (double x) { return std::abs(x); }
static inline double op_sgn_d  (double x) { return (x > 0.0) ? 1.0 : (x < 0.0) ? -1.0 : 0.0; }
static inline double op_real_d (double x) { return x; }
static inline double op_imag_d (double)   { return 0.0; }
static inline double op_conj_d (double x) { return x; }
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static inline double op_phase_d(double x) { return (x >= 0.0) ? 0.0 : M_PI; }

// =========================================================================
//  Element-wise math ops (complex path)
// =========================================================================

using C = std::complex<double>;

static inline C op_sin_c  (C x) { return std::sin(x); }
static inline C op_cos_c  (C x) { return std::cos(x); }
static inline C op_tan_c  (C x) { return std::tan(x); }
static inline C op_asin_c (C x) { return std::asin(x); }
static inline C op_acos_c (C x) { return std::acos(x); }
static inline C op_atan_c (C x) { return std::atan(x); }
static inline C op_sinh_c (C x) { return std::sinh(x); }
static inline C op_cosh_c (C x) { return std::cosh(x); }
static inline C op_tanh_c (C x) { return std::tanh(x); }
static inline C op_asinh_c(C x) { return std::asinh(x); }
static inline C op_acosh_c(C x) { return std::acosh(x); }
static inline C op_atanh_c(C x) { return std::atanh(x); }
static inline C op_log_c  (C x) { return std::log(x); }
static inline C op_log10_c(C x) { return std::log10(x); }
static inline C op_exp_c  (C x) { return std::exp(x); }
static inline C op_sqrt_c (C x) { return std::sqrt(x); }
static inline C op_sqr_c  (C x) { return x * x; }
static inline double op_abs_c (C x) { return std::abs(x); }
static inline C op_sgn_c (C x) {
    double n = std::abs(x);
    return (n == 0.0) ? C(0.0, 0.0) : x / n;
}
static inline double op_real_c (C x) { return std::real(x); }
static inline double op_imag_c (C x) { return std::imag(x); }
static inline C op_conj_c(C x) { return std::conj(x); }
static inline double op_phase_c(C x) { return std::arg(x); }

// =========================================================================
//  Derive callbacks for unary math
// =========================================================================

static DataType DeriveDtypeMath(const std::vector<DataType>& dtypes) {
    DataType dt = dtypes[0];
    if (dt == DataType::kBoolean || dt == DataType::kInteger)
        return DataType::kReal;
    if (dt == DataType::kReal || dt == DataType::kComplex)
        return dt;
    throw std::runtime_error("math function: unsupported type");
}

// DeriveDtypeMathRealDown: complex→real, real→real, int→real
static DataType DeriveDtypeMathRealDown(const std::vector<DataType>& dtypes) {
    DataType dt = dtypes[0];
    if (dt == DataType::kBoolean || dt == DataType::kInteger)
        return DataType::kReal;
    if (dt == DataType::kReal || dt == DataType::kComplex)
        return DataType::kReal;
    throw std::runtime_error("math function: unsupported type");
}

// DeriveDtypeMathConj: complex→complex, real→real, int→int
static DataType DeriveDtypeMathConj(const std::vector<DataType>& dtypes) {
    DataType dt = dtypes[0];
    if (dt == DataType::kBoolean) return DataType::kInteger;
    return dt;
}

static Unit DeriveUnitFirst(const std::vector<Unit>& units) {
    return units[0];
}

// =========================================================================
//  Execute callbacks �?standard math (dtype dispatch: Real �?Complex)
// =========================================================================

Value ExecuteSin(const ExecContextInfo& info,
                  const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_sin_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_sin_c);
        default:
            throw std::invalid_argument("sin: unsupported dtype");
    }
}

Value ExecuteCos(const ExecContextInfo& info,
                  const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_cos_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_cos_c);
        default:
            throw std::invalid_argument("cos: unsupported dtype");
    }
}

Value ExecuteTan(const ExecContextInfo& info,
                  const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_tan_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_tan_c);
        default:
            throw std::invalid_argument("tan: unsupported dtype");
    }
}

Value ExecuteAsin(const ExecContextInfo& info,
                   const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_asin_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_asin_c);
        default:
            throw std::invalid_argument("asin: unsupported dtype");
    }
}

Value ExecuteAcos(const ExecContextInfo& info,
                   const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_acos_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_acos_c);
        default:
            throw std::invalid_argument("acos: unsupported dtype");
    }
}

Value ExecuteAtan(const ExecContextInfo& info,
                   const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_atan_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_atan_c);
        default:
            throw std::invalid_argument("atan: unsupported dtype");
    }
}

Value ExecuteSinh(const ExecContextInfo& info,
                   const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_sinh_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_sinh_c);
        default:
            throw std::invalid_argument("sinh: unsupported dtype");
    }
}

Value ExecuteCosh(const ExecContextInfo& info,
                   const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_cosh_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_cosh_c);
        default:
            throw std::invalid_argument("cosh: unsupported dtype");
    }
}

Value ExecuteTanh(const ExecContextInfo& info,
                   const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_tanh_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_tanh_c);
        default:
            throw std::invalid_argument("tanh: unsupported dtype");
    }
}

Value ExecuteAsinh(const ExecContextInfo& info,
                    const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_asinh_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_asinh_c);
        default:
            throw std::invalid_argument("asinh: unsupported dtype");
    }
}

Value ExecuteAcosh(const ExecContextInfo& info,
                    const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_acosh_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_acosh_c);
        default:
            throw std::invalid_argument("acosh: unsupported dtype");
    }
}

Value ExecuteAtanh(const ExecContextInfo& info,
                    const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_atanh_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_atanh_c);
        default:
            throw std::invalid_argument("atanh: unsupported dtype");
    }
}

Value ExecuteLog(const ExecContextInfo& info,
                  const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_log_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_log_c);
        default:
            throw std::invalid_argument("log: unsupported dtype");
    }
}

Value ExecuteLog10(const ExecContextInfo& info,
                    const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_log10_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_log10_c);
        default:
            throw std::invalid_argument("log10: unsupported dtype");
    }
}

Value ExecuteExp(const ExecContextInfo& info,
                  const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_exp_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_exp_c);
        default:
            throw std::invalid_argument("exp: unsupported dtype");
    }
}

Value ExecuteSqrt(const ExecContextInfo& info,
                   const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_sqrt_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_sqrt_c);
        default:
            throw std::invalid_argument("sqrt: unsupported dtype");
    }
}

Value ExecuteSqr(const ExecContextInfo& info,
                  const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_sqr_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_sqr_c);
        default:
            throw std::invalid_argument("sqr: unsupported dtype");
    }
}

// =========================================================================
//  Execute callbacks �?abs (output is always Real)
// =========================================================================

Value ExecuteAbs(const ExecContextInfo& info,
                  const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops,
                static_cast<double(*)(double)>(std::abs));
        case DataType::kComplex: {
            // abs(complex) �?real; do a manual unary loop with type change
            bool is_meas = ops[0].is_measurement();
            DataShape op_shape = ops[0].data_shape();
            ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make({op_shape}, info.shape);

            auto in = ops[0].flat_data<C>();
            const C* ptr = in.ptr;
            Index stride = in.stride;

            auto out_ds = std::unique_ptr<DataSeries>(
                new DataSeries(DataType::kReal, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));
            double* out = out_ds->mutable_contiguous_data<double>();

            Index out_stride = shape_plan.result_elements;
            for (Index i = 0; i < info.rows; ++i) {
                Index i_off = i * stride;
                Index o_off = i * out_stride;
                for (Index j = 0; j < shape_plan.result_elements; ++j)
                    out[o_off + j] = std::abs(ptr[i_off + j]);
            }

            if (is_meas)
                return Value(out_ds->measurement_at(0));
            {
                const DataArray& src = ops[0].as_data_array();
                auto da = std::make_shared<DataArray>(src.clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
        }
        default:
            throw std::invalid_argument("abs: unsupported dtype");
    }
}

// =========================================================================
//  Execute callbacks �?sgn
// =========================================================================

Value ExecuteSgn(const ExecContextInfo& info,
                  const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_sgn_d);
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_sgn_c);
        default:
            throw std::invalid_argument("sgn: unsupported dtype");
    }
}

// =========================================================================
//  Execute callbacks �?real / imag (output always Real)
// =========================================================================

Value ExecuteReal(const ExecContextInfo& info,
                   const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_real_d);
        case DataType::kComplex: {
            bool is_meas = ops[0].is_measurement();
            DataShape op_shape = ops[0].data_shape();
            ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make({op_shape}, info.shape);

            auto in = ops[0].flat_data<C>();
            const C* ptr = in.ptr;
            Index stride = in.stride;

            auto out_ds = std::unique_ptr<DataSeries>(
                new DataSeries(DataType::kReal, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));
            double* out = out_ds->mutable_contiguous_data<double>();

            Index out_stride = shape_plan.result_elements;
            for (Index i = 0; i < info.rows; ++i) {
                Index i_off = i * stride;
                Index o_off = i * out_stride;
                for (Index j = 0; j < shape_plan.result_elements; ++j)
                    out[o_off + j] = std::real(ptr[i_off + j]);
            }

            if (is_meas)
                return Value(out_ds->measurement_at(0));
            {
                const DataArray& src = ops[0].as_data_array();
                auto da = std::make_shared<DataArray>(src.clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
        }
        default:
            throw std::invalid_argument("real: unsupported dtype");
    }
}

Value ExecuteImag(const ExecContextInfo& info,
                   const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_imag_d);
        case DataType::kComplex: {
            bool is_meas = ops[0].is_measurement();
            DataShape op_shape = ops[0].data_shape();
            ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make({op_shape}, info.shape);

            auto in = ops[0].flat_data<C>();
            const C* ptr = in.ptr;
            Index stride = in.stride;

            auto out_ds = std::unique_ptr<DataSeries>(
                new DataSeries(DataType::kReal, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));
            double* out = out_ds->mutable_contiguous_data<double>();

            Index out_stride = shape_plan.result_elements;
            for (Index i = 0; i < info.rows; ++i) {
                Index i_off = i * stride;
                Index o_off = i * out_stride;
                for (Index j = 0; j < shape_plan.result_elements; ++j)
                    out[o_off + j] = std::imag(ptr[i_off + j]);
            }

            if (is_meas)
                return Value(out_ds->measurement_at(0));
            {
                const DataArray& src = ops[0].as_data_array();
                auto da = std::make_shared<DataArray>(src.clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
        }
        default:
            throw std::invalid_argument("imag: unsupported dtype");
    }
}

// =========================================================================
//  Execute callbacks �?conj
// =========================================================================

Value ExecuteConj(const ExecContextInfo& info,
                   const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kInteger:
            return ExecUnaryT<int>(info, ops, [](int x) { return x; });
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, [](double x) { return x; });
        case DataType::kComplex:
            return ExecUnaryT<C>(info, ops, op_conj_c);
        default:
            throw std::invalid_argument("conj: unsupported dtype");
    }
}

// =========================================================================
//  Execute callbacks �?phase
// =========================================================================

Value ExecutePhase(const ExecContextInfo& info,
                    const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecUnaryT<double>(info, ops, op_phase_d);
        case DataType::kComplex: {
            bool is_meas = ops[0].is_measurement();
            DataShape op_shape = ops[0].data_shape();
            ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make({op_shape}, info.shape);

            auto in = ops[0].flat_data<C>();
            const C* ptr = in.ptr;
            Index stride = in.stride;

            auto out_ds = std::unique_ptr<DataSeries>(
                new DataSeries(DataType::kReal, info.shape));
            out_ds->set_unit(info.unit);
            out_ds->resize(static_cast<std::size_t>(info.rows));
            double* out = out_ds->mutable_contiguous_data<double>();

            Index out_stride = shape_plan.result_elements;
            for (Index i = 0; i < info.rows; ++i) {
                Index i_off = i * stride;
                Index o_off = i * out_stride;
                for (Index j = 0; j < shape_plan.result_elements; ++j)
                    out[o_off + j] = std::arg(ptr[i_off + j]);
            }

            if (is_meas)
                return Value(out_ds->measurement_at(0));
            {
                const DataArray& src = ops[0].as_data_array();
                auto da = std::make_shared<DataArray>(src.clone());
                da->set_data(std::move(*out_ds));
                return Value(da);
            }
        }
        default:
            throw std::invalid_argument("phase: unsupported dtype");
    }
}

// =========================================================================
//  OpTraits definitions
// =========================================================================

const OpTraits kOpSin = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteSin
};

const OpTraits kOpCos = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteCos
};

const OpTraits kOpTan = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteTan
};

const OpTraits kOpAsin = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteAsin
};

const OpTraits kOpAcos = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteAcos
};

const OpTraits kOpAtan = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteAtan
};

const OpTraits kOpSinh = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteSinh
};

const OpTraits kOpCosh = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteCosh
};

const OpTraits kOpTanh = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteTanh
};

const OpTraits kOpAsinh = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteAsinh
};

const OpTraits kOpAcosh = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteAcosh
};

const OpTraits kOpAtanh = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteAtanh
};

const OpTraits kOpLog = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteLog
};

const OpTraits kOpLog10 = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteLog10
};

const OpTraits kOpExp = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteExp
};

const OpTraits kOpSqrt = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteSqrt
};

const OpTraits kOpSqr = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteSqr
};

const OpTraits kOpSgn = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteSgn
};

const OpTraits kOpAbs = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMath, DeriveUnitFirst, ExecuteAbs
};

// real / imag �?complex→real downcast
const OpTraits kOpReal = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMathRealDown, DeriveUnitFirst, ExecuteReal
};

const OpTraits kOpImag = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMathRealDown, DeriveUnitFirst, ExecuteImag
};

const OpTraits kOpPhase = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMathRealDown, DeriveUnitFirst, ExecutePhase
};

// conj �?complex→complex, real→real, int→int (type-preserving)
const OpTraits kOpConj = {
    1, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeMathConj, DeriveUnitFirst, ExecuteConj
};

// =========================================================================
//  Binary math �?element ops
// =========================================================================

static inline double op_atan2_d(double y, double x) { return std::atan2(y, x); }

static inline double op_root_d(double x, double n)  { return std::pow(x, 1.0 / n); }
static inline C      op_root_c(C x, C n)             { return std::pow(x, C(1.0) / n); }

// =========================================================================
//  Binary math �?derive callbacks
// =========================================================================

static DataType DeriveDtypeAtan2(const std::vector<DataType>& dtypes) {
    for (size_t i = 0; i < dtypes.size(); ++i) {
        DataType dt = dtypes[i];
        if (dt == DataType::kBoolean) continue;
        if (dt == DataType::kInteger) continue;
        if (dt == DataType::kReal)    continue;
        throw std::runtime_error("atan2: arguments must be Integer, Real, or Boolean");
    }
    return DataType::kReal;
}

static DataType DeriveDtypeRoot(const std::vector<DataType>& dtypes) {
    DataType res = DataType::kReal;
    for (size_t i = 0; i < dtypes.size(); ++i) {
        DataType dt = dtypes[i];
        if (dt == DataType::kBoolean) dt = DataType::kInteger;
        if (dt == DataType::kComplex) { res = DataType::kComplex; continue; }
        if (dt == DataType::kReal)    continue;
        if (dt == DataType::kInteger) continue;
        throw std::runtime_error("root: unsupported type");
    }
    return res;
}

static Unit DeriveUnitAtan2(const std::vector<Unit>& units) {
    return DeriveUnitSameDim({units[0], units[1]});
}

static Unit DeriveUnitRoot(const std::vector<Unit>& units) {
    return DeriveUnitFirst({units[0]});
}

// =========================================================================
//  Binary math �?execute callbacks
// =========================================================================

Value ExecuteAtan2(const ExecContextInfo& info,
                    const std::vector<Value>& ops) {
    return ExecBinaryArithT<double>(info, ops, op_atan2_d);
}

Value ExecuteRoot(const ExecContextInfo& info,
                   const std::vector<Value>& ops) {
    switch (info.dtype) {
        case DataType::kReal:
            return ExecBinaryArithT<double>(info, ops, op_root_d);
        case DataType::kComplex:
            return ExecBinaryArithT<C>(info, ops, op_root_c);
        default:
            throw std::invalid_argument("root: unsupported dtype");
    }
}

// =========================================================================
//  Binary math �?OpTraits
// =========================================================================

const OpTraits kOpAtan2 = {
    2, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeAtan2, DeriveUnitAtan2, ExecuteAtan2
};

const OpTraits kOpRoot = {
    2, DeriveShapeBroadcast, DeriveRowsBroadcast,
    DeriveDtypeRoot, DeriveUnitRoot, ExecuteRoot
};

// =========================================================================
//  Public API wrappers
// =========================================================================

Value OperationSin (const Value& v) { return Operate({v}, kOpSin); }
Value OperationCos (const Value& v) { return Operate({v}, kOpCos); }
Value OperationTan (const Value& v) { return Operate({v}, kOpTan); }
Value OperationAsin(const Value& v) { return Operate({v}, kOpAsin); }
Value OperationAcos(const Value& v) { return Operate({v}, kOpAcos); }
Value OperationAtan(const Value& v) { return Operate({v}, kOpAtan); }
Value OperationSinh(const Value& v) { return Operate({v}, kOpSinh); }
Value OperationCosh(const Value& v) { return Operate({v}, kOpCosh); }
Value OperationTanh(const Value& v) { return Operate({v}, kOpTanh); }
Value OperationAsinh(const Value& v) { return Operate({v}, kOpAsinh); }
Value OperationAcosh(const Value& v) { return Operate({v}, kOpAcosh); }
Value OperationAtanh(const Value& v) { return Operate({v}, kOpAtanh); }
Value OperationLog  (const Value& v) { return Operate({v}, kOpLog); }
Value OperationLog10(const Value& v) { return Operate({v}, kOpLog10); }
Value OperationExp  (const Value& v) { return Operate({v}, kOpExp); }
Value OperationSqrt (const Value& v) { return Operate({v}, kOpSqrt); }
Value OperationSqr  (const Value& v) { return Operate({v}, kOpSqr); }
Value OperationAbs  (const Value& v) { return Operate({v}, kOpAbs); }
Value OperationSgn  (const Value& v) { return Operate({v}, kOpSgn); }
Value OperationReal (const Value& v) { return Operate({v}, kOpReal); }
Value OperationImag (const Value& v) { return Operate({v}, kOpImag); }
Value OperationConj (const Value& v) { return Operate({v}, kOpConj); }
Value OperationPhase(const Value& v) { return Operate({v}, kOpPhase); }

Value OperationAtan2(const Value& y, const Value& x) { return Operate({y, x}, kOpAtan2); }
Value OperationRoot (const Value& x, const Value& n) { return Operate({x, n}, kOpRoot); }

}  // namespace operation
}  // namespace rel
