#ifndef REL_RUNTIME_OPERATION_PIPELINE_H
#define REL_RUNTIME_OPERATION_PIPELINE_H

#include "rel_runtime_api.h"
#include "value.h"

#include <vector>

namespace rel {
namespace operation {

using namespace xdataset;

// =========================================================================
//  Broadcast plans
// =========================================================================

struct RowBroadcastPlan {
    Index              result_size;
    std::vector<bool>  broadcast;

    static RowBroadcastPlan Compute(const std::vector<Index>& sizes);
};

struct OperandBroadcastShapeInfo {
    Index elements;
    Index cols;
    bool  broadcast_row;
    bool  broadcast_col;
};

struct ShapeBroadcastPlan {
    DataShape          result_shape;
    Index              result_elements;
    Index              result_cols;
    std::vector<OperandBroadcastShapeInfo> ops;

    static ShapeBroadcastPlan Make(const std::vector<DataShape>& operand_shapes,
                                    const DataShape& result);
    Index MapFlatIndex(Index result_flat, int k) const;
};

// =========================================================================
//  Execution context & traits
// =========================================================================

struct ExecContextInfo {
    Index              rows;
    DataShape          shape;
    DataType           dtype;
    Unit               unit;
};

template <typename T>
using ElemOp = T (*)(T, T);

template <typename T>
using UnaryOp = T (*)(T);

typedef DataShape (*DeriveShapeFunc)(const std::vector<DataShape>& operand_shapes);
typedef DataType (*DeriveDtypeFunc)(const std::vector<DataType>& dtypes);
typedef Unit     (*DeriveUnitFunc)(const std::vector<Unit>& units);
typedef Index    (*DeriveRowsFunc)(const std::vector<Index>& rows);
typedef Value    (*ExecuteFunc)(const ExecContextInfo& info,
                                const std::vector<Value>& ops);

enum Arity : Index {
    kVariadic = -1
};

struct OpTraits {
    Index           arity;
    DeriveShapeFunc derive_shape;
    DeriveRowsFunc  derive_rows;
    DeriveDtypeFunc derive_dtype;
    DeriveUnitFunc  derive_unit;
    ExecuteFunc     execute;
};

// =========================================================================
//  Operate -- the core pipeline entry point
// =========================================================================

REL_RUNTIME_API Value Operate(const std::vector<Value>& operands,
                              const OpTraits& traits);

// =========================================================================
//  Generic Derive callbacks (used by many operators)
// =========================================================================

REL_RUNTIME_API DataShape DeriveShapeBroadcast(const std::vector<DataShape>& operand_shapes);
REL_RUNTIME_API Index    DeriveRowsBroadcast(const std::vector<Index>& rows);

REL_RUNTIME_API DataType DeriveDtypePromote(const std::vector<DataType>& dtypes);
REL_RUNTIME_API Unit     DeriveUnitSameDim(const std::vector<Unit>& units);

}  // namespace operation
}  // namespace rel

#endif  // REL_RUNTIME_OPERATION_PIPELINE_H
