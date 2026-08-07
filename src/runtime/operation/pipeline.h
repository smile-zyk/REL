#ifndef REL_RUNTIME_OPERATION_PIPELINE_H
#define REL_RUNTIME_OPERATION_PIPELINE_H

#include "rel_runtime_api.h"
#include "value.h"

#include <vector>

namespace rel {
namespace operation {

// =========================================================================
//  Broadcast plans
// =========================================================================

struct RowBroadcastPlan {
    xdataset::Index              result_size;
    std::vector<bool>  broadcast;

    static RowBroadcastPlan Compute(const std::vector<xdataset::Index>& sizes);
};

struct OperandBroadcastShapeInfo {
    xdataset::Index elements;
    xdataset::Index cols;
    bool  broadcast_row;
    bool  broadcast_col;
};

struct ShapeBroadcastPlan {
    xdataset::DataShape          result_shape;
    xdataset::Index              result_elements;
    xdataset::Index              result_cols;
    std::vector<OperandBroadcastShapeInfo> ops;

    static ShapeBroadcastPlan Make(const std::vector<xdataset::DataShape>& operand_shapes,
                                    const xdataset::DataShape& result);
    xdataset::Index MapFlatIndex(xdataset::Index result_flat, int k) const;
};

// =========================================================================
//  Execution context & traits
// =========================================================================

struct ExecContextInfo {
    xdataset::Index              rows;
    xdataset::DataShape          shape;
    xdataset::DataType           dtype;
    xdataset::Unit               unit;
};

template <typename T>
using ElemOp = T (*)(T, T);

template <typename T>
using UnaryOp = T (*)(T);

typedef xdataset::DataShape (*DeriveShapeFunc)(const std::vector<xdataset::DataShape>& operand_shapes);
typedef xdataset::DataType (*DeriveDtypeFunc)(const std::vector<xdataset::DataType>& dtypes);
typedef xdataset::Unit     (*DeriveUnitFunc)(const std::vector<xdataset::Unit>& units);
typedef xdataset::Index    (*DeriveRowsFunc)(const std::vector<xdataset::Index>& rows);
typedef Value    (*ExecuteFunc)(const ExecContextInfo& info,
                                const std::vector<Value>& ops);

enum Arity : xdataset::Index {
    kVariadic = -1
};

struct OpTraits {
    xdataset::Index           arity;
    DeriveShapeFunc derive_shape;
    DeriveRowsFunc  derive_rows;
    DeriveDtypeFunc derive_dtype;
    DeriveUnitFunc  derive_unit;
    ExecuteFunc     execute;
};

// =========================================================================
//  Operate -- the core pipeline entry point
// =========================================================================

Value Operate(const std::vector<Value>& operands,
              const OpTraits& traits);

// =========================================================================
//  Generic Derive callbacks (used by many operators)
// =========================================================================

xdataset::DataShape DeriveShapeBroadcast(const std::vector<xdataset::DataShape>& operand_shapes);
xdataset::Index    DeriveRowsBroadcast(const std::vector<xdataset::Index>& rows);

xdataset::DataType DeriveDtypePromote(const std::vector<xdataset::DataType>& dtypes);
xdataset::Unit     DeriveUnitSameDim(const std::vector<xdataset::Unit>& units);

// Additional reusable derive callbacks
xdataset::Unit DeriveUnitFirst(const std::vector<xdataset::Unit>& units);
xdataset::Unit DeriveUnitMul(const std::vector<xdataset::Unit>& units);
xdataset::Unit DeriveUnitDiv(const std::vector<xdataset::Unit>& units);
xdataset::Unit DeriveUnitDimless(const std::vector<xdataset::Unit>& units);

}  // namespace operation
}  // namespace rel

#endif  // REL_RUNTIME_OPERATION_PIPELINE_H
