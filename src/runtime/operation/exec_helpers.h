// =============================================================================
//  REL -- Internal execution helpers (shared by operator.cc & math_functions.cc)
// =============================================================================
//
//  This is a private header -- NOT part of the public API.  It contains
//  the template functions used by both operator.cc and math_functions.cc
//  to run element-wise unary and binary loops over flat data buffers.

#ifndef REL_RUNTIME_OPERATION_EXEC_HELPERS_H
#define REL_RUNTIME_OPERATION_EXEC_HELPERS_H

#include "operation/pipeline.h"
#include "data_series.h"
#include "data_array.h"

#include <memory>
#include <vector>

namespace rel {
namespace operation {

using namespace xdataset;

// =========================================================================
//  ExecBinaryLoop -- core flat-buffer loop for binary ops
// =========================================================================
//
//  Row-level and cell-level broadcast are driven by the two plans.

template <typename T, typename Out = T>
inline void ExecBinaryLoop(Index rows,
                            const RowBroadcastPlan& row_plan,
                            const ShapeBroadcastPlan& shape_plan,
                            const T* l_ptr, Index l_stride,
                            const T* r_ptr, Index r_stride,
                            Out* out,
                            Out (*elem_op)(T, T))
{
    Index out_stride = shape_plan.result_elements;

    for (Index i = 0; i < rows; ++i) {
        Index l_row_off = (row_plan.broadcast[0] ? 0 : i) * l_stride;
        Index r_row_off = (row_plan.broadcast[1] ? 0 : i) * r_stride;
        Index o_off     = i * out_stride;

        for (Index j = 0; j < shape_plan.result_elements; ++j) {
            Index lj = shape_plan.MapFlatIndex(j, 0);
            Index rj = shape_plan.MapFlatIndex(j, 1);
            out[o_off + j] = elem_op(
                l_ptr[l_row_off + lj],
                r_ptr[r_row_off + rj]);
        }
    }
}

// =========================================================================
//  ExecUnaryLoop -- core flat-buffer loop for single operand
// =========================================================================

template <typename T>
inline void ExecUnaryLoop(Index rows,
                           const ShapeBroadcastPlan& shape_plan,
                           const T* ptr, Index stride,
                           T* out,
                           UnaryOp<T> op)
{
    Index out_stride = shape_plan.result_elements;

    for (Index i = 0; i < rows; ++i) {
        Index i_off = i * stride;
        Index o_off = i * out_stride;

        for (Index j = 0; j < shape_plan.result_elements; ++j) {
            out[o_off + j] = op(ptr[i_off + j]);
        }
    }
}

// =========================================================================
//  Output helpers
// =========================================================================

/// Reconstruct a Measurement from a flat typed buffer and a DataShape.
template <typename T>
inline Measurement MakeMeasFromFlat(const T* data,
                                     const DataShape& shape,
                                     const Unit& unit) {
    DataKind dk = shape.kind();

    if (dk == DataKind::kScalar)
        return Measurement::Scalar(data[0], unit);

    if (dk == DataKind::kVector) {
        Index w = shape[0];
        Eigen::Matrix<T, 1, Eigen::Dynamic> v(w);
        for (Index i = 0; i < w; ++i)
            v(i) = data[i];
        return Measurement::Vector(v, unit);
    }

    // Matrix
    Index r = shape[0];
    Index c = shape[1];
    Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> m(r, c);
    for (Index i = 0; i < r; ++i)
        for (Index j = 0; j < c; ++j)
            m(i, j) = data[i * c + j];
    return Measurement::Matrix(m, unit);
}

/// Binary ops: when the output is a DataArray, choose which operand's
/// metadata (MultiDimensionSpec, DataArrayKind) to inherit.
inline const DataArray* SelectOutputSource(bool l_meas, bool r_meas,
                                            const std::vector<Value>& ops) {
    if (!l_meas && !r_meas) return &ops[0].as_data_array();
    if (l_meas && !r_meas) return &ops[1].as_data_array();
    if (!l_meas && r_meas) return &ops[0].as_data_array();
    return nullptr;
}

// =========================================================================
//  ExecBinaryArithT -- binary arithmetic entry point
// =========================================================================
//
//  Extract operand metadata, compute broadcast plans, flatten inputs,
//  allocate output, run the unified loop, and convert back to Value.

template <typename T>
inline Value ExecBinaryArithT(const ExecContextInfo& info,
                               const std::vector<Value>& ops,
                               ElemOp<T> elem_op)
{
    bool l_meas = ops[0].is_measurement();
    bool r_meas = ops[1].is_measurement();

    DataShape l_shape = ops[0].data_shape();
    DataShape r_shape = ops[1].data_shape();
    std::vector<DataShape> op_shapes = {l_shape, r_shape};

    Index l_rows = ops[0].rows();
    Index r_rows = ops[1].rows();
    std::vector<Index> row_counts = {l_rows, r_rows};

    ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make(op_shapes, info.shape);
    RowBroadcastPlan   row_plan   = RowBroadcastPlan::Compute(row_counts);

    auto l_in    = ops[0].flat_data<T>();
    auto r_in    = ops[1].flat_data<T>();
    const T* l_ptr    = l_in.ptr;
    const T* r_ptr    = r_in.ptr;
    Index    l_stride = l_in.stride;
    Index    r_stride = r_in.stride;

    const DataArray* out_src = SelectOutputSource(l_meas, r_meas, ops);

    auto out_ds = std::unique_ptr<DataSeries>(
        new DataSeries(DataTypeOf<T>::tag, info.shape));
    out_ds->set_unit(info.unit);
    out_ds->resize(static_cast<std::size_t>(info.rows));
    T* out = out_ds->mutable_contiguous_data<T>();

    ExecBinaryLoop(info.rows, row_plan, shape_plan,
                   l_ptr, l_stride, r_ptr, r_stride, out, elem_op);

    if (l_meas && r_meas) {
        return Value(MakeMeasFromFlat(out, info.shape, info.unit));
    } else {
        auto da = std::make_shared<DataArray>(out_src->clone());
        da->set_data(std::move(*out_ds));
        return Value(da);
    }
}

// =========================================================================
//  ExecUnaryT -- unary entry point (reuses flat_data, output helpers)
// =========================================================================

template <typename T>
inline Value ExecUnaryT(const ExecContextInfo& info,
                         const std::vector<Value>& ops,
                         UnaryOp<T> op)
{
    bool is_meas = ops[0].is_measurement();

    DataShape op_shape = ops[0].data_shape();
    ShapeBroadcastPlan shape_plan = ShapeBroadcastPlan::Make({op_shape}, info.shape);

    auto in = ops[0].flat_data<T>();
    const T* ptr    = in.ptr;
    Index    stride = in.stride;

    auto out_ds = std::unique_ptr<DataSeries>(
        new DataSeries(DataTypeOf<T>::tag, info.shape));
    out_ds->set_unit(info.unit);
    out_ds->resize(static_cast<std::size_t>(info.rows));
    T* out = out_ds->mutable_contiguous_data<T>();

    ExecUnaryLoop(info.rows, shape_plan, ptr, stride, out, op);

    if (is_meas) {
        return Value(MakeMeasFromFlat(out, info.shape, info.unit));
    } else {
        const DataArray& src = ops[0].as_data_array();
        auto da = std::make_shared<DataArray>(src.clone());
        da->set_data(std::move(*out_ds));
        return Value(da);
    }
}

}  // namespace operation
}  // namespace rel

#endif  // REL_RUNTIME_OPERATION_EXEC_HELPERS_H
