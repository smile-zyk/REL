// =============================================================================
//  REL -- Operation pipeline (derive + operate)
// =============================================================================

#include "operation_pipeline.h"
#include "data_series.h"
#include "data_array.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace rel {

// =========================================================================
//  RowBroadcastPlan::Compute
// =========================================================================

RowBroadcastPlan RowBroadcastPlan::Compute(const std::vector<Index>& sizes) {
    RowBroadcastPlan plan;
    Index r = 1;
    for (size_t i = 0; i < sizes.size(); ++i) {
        Index s = sizes[i];
        if (s == 1) continue;
        if (r == 1) { r = s; continue; }
        if (s != r)
            throw std::invalid_argument(
                "broadcast size mismatch (" + std::to_string(r) +
                " vs " + std::to_string(s) + ") at index " +
                std::to_string(i));
    }
    plan.result_size = r;
    for (size_t i = 0; i < sizes.size(); ++i)
        plan.broadcast.push_back(sizes[i] == 1 && r > 1);
    return plan;
}

// =========================================================================
//  ShapeBroadcastPlan::Make
// =========================================================================

ShapeBroadcastPlan ShapeBroadcastPlan::Make(const std::vector<DataShape>& operand_shapes,
                     const DataShape& result) {
    ShapeBroadcastPlan sp;
    DataKind rk = result.kind();
    sp.result_shape    = result;
    sp.result_elements = (rk == DataKind::kScalar) ? 1
                       : (rk == DataKind::kVector) ? result[0]
                       : result[0] * result[1];

    if (rk == DataKind::kMatrix)
        sp.result_cols = result[1];
    else if (rk == DataKind::kVector)
        sp.result_cols = result[0];
    else
        sp.result_cols = 1;

    for (size_t i = 0; i < operand_shapes.size(); ++i) {
        DataKind k = operand_shapes[i].kind();
        const auto& s = operand_shapes[i];

        OperandBroadcastShapeInfo op;
        op.elements = (k == DataKind::kScalar) ? 1
                    : (k == DataKind::kVector) ? s[0]
                    : s[0] * s[1];

        bool bc_row = false, bc_col = false;
        Index cols = 1;

        if (k == DataKind::kScalar) {
            bc_row = true;
            bc_col = true;
            cols   = 1;

        } else if (k == DataKind::kVector) {
            Index w = s[0];
            if (rk == DataKind::kVector) {
                bc_col = (w == 1 && sp.result_cols > 1);
                cols   = w;
            } else /* result is Matrix */ {
                bc_row = true;
                bc_col = (w == 1 && result[1] > 1);
                cols   = w;
            }

        } else /* kMatrix */ {
            cols   = s[1];
            bc_row = (s[0] == 1 && result[0] > 1);
            bc_col = (s[1] == 1 && result[1] > 1);
        }

        op.cols = cols;
        op.broadcast_row = bc_row;
        op.broadcast_col = bc_col;
        sp.ops.push_back(op);
    }

    return sp;
}

// =========================================================================
//  ShapeBroadcastPlan::MapFlatIndex
// =========================================================================

Index ShapeBroadcastPlan::MapFlatIndex(Index result_flat, int k) const {
    const OperandBroadcastShapeInfo& op = ops[static_cast<size_t>(k)];
    if (op.elements == 1) return 0;

    Index row = 0, col = result_flat;
    if (result_cols > 1 && result_elements != result_cols) {
        row = result_flat / result_cols;
        col = result_flat % result_cols;
    }

    Index r = op.broadcast_row ? 0 : row;
    Index c = op.broadcast_col ? 0 : col;
    return r * op.cols + c;
}

// =========================================================================
//  Operate
// =========================================================================

Value Operate(const std::vector<Value>& operands, const OpTraits& traits) {
    if (traits.arity != Arity::kVariadic) {
        Index n = static_cast<Index>(operands.size());
        if (n != traits.arity) {
            throw std::invalid_argument(
                std::string("arity mismatch: expected ") +
                std::to_string(traits.arity) + " operand(s), got " +
                std::to_string(n));
        }
    }

    std::vector<Value> canonical_ops;
    canonical_ops.reserve(operands.size());
    for (size_t i = 0; i < operands.size(); ++i)
        canonical_ops.push_back(operands[i].canonicalized());

    std::vector<DataShape> operand_shapes;
    std::vector<Index>     row_counts;
    std::vector<DataType>  dtypes;
    std::vector<Unit>      units;

    for (size_t i = 0; i < canonical_ops.size(); ++i) {
        operand_shapes.push_back(canonical_ops[i].data_shape());
        row_counts.push_back(canonical_ops[i].rows());
        dtypes.push_back(canonical_ops[i].data_type());
        units.push_back(canonical_ops[i].unit());
    }

    DataShape shape = traits.derive_shape(operand_shapes);
    Index     rows  = traits.derive_rows(row_counts);
    DataType  dtype = traits.derive_dtype(dtypes);
    Unit      unit  = traits.derive_unit(units);

    ExecContextInfo info;
    info.rows  = rows;
    info.shape = shape;
    info.dtype = dtype;
    info.unit  = unit;

    return traits.execute(info, canonical_ops);
}

// =========================================================================
//  DeriveShapeBroadcast
// =========================================================================

DataShape DeriveShapeBroadcast(const std::vector<DataShape>& operand_shapes) {
    DataKind res_kind = DataKind::kScalar;
    for (size_t i = 0; i < operand_shapes.size(); ++i) {
        DataKind k = operand_shapes[i].kind();
        if (k == DataKind::kMatrix)
            res_kind = DataKind::kMatrix;
        else if (k == DataKind::kVector && res_kind != DataKind::kMatrix)
            res_kind = DataKind::kVector;
    }

    if (res_kind == DataKind::kScalar)
        return DataShape::Scalar();

    if (res_kind == DataKind::kVector) {
        Index w = 1;
        for (size_t i = 0; i < operand_shapes.size(); ++i) {
            if (operand_shapes[i].kind() == DataKind::kScalar) continue;
            Index sw = operand_shapes[i][0];
            if (sw == 1) continue;
            if (w == 1) { w = sw; continue; }
            if (sw != w)
                throw std::invalid_argument(
                    "vector width mismatch (" + std::to_string(w) +
                    " vs " + std::to_string(sw) + ")");
        }
        return DataShape::Vector(w);
    }

    Index r = 1, c = 1;
    for (size_t i = 0; i < operand_shapes.size(); ++i) {
        Index op_r = 1, op_c = 1;
        DataKind k = operand_shapes[i].kind();
        const auto& s = operand_shapes[i];

        if      (k == DataKind::kScalar) { op_r = 1; op_c = 1; }
        else if (k == DataKind::kVector) { op_r = 1; op_c = s[0]; }
        else /* kMatrix */               { op_r = s[0]; op_c = s[1]; }

        if (op_r == 1) { /* broadcast */ }
        else if (r == 1) { r = op_r; }
        else if (op_r != r)
            throw std::invalid_argument(
                "row dim mismatch (" + std::to_string(r) +
                " vs " + std::to_string(op_r) + ")");

        if (op_c == 1) { /* broadcast */ }
        else if (c == 1) { c = op_c; }
        else if (op_c != c)
            throw std::invalid_argument(
                "col dim mismatch (" + std::to_string(c) +
                " vs " + std::to_string(op_c) + ")");
    }
    return DataShape::Matrix(r, c);
}

// =========================================================================
//  DeriveRowsBroadcast
// =========================================================================

Index DeriveRowsBroadcast(const std::vector<Index>& rows) {
    return RowBroadcastPlan::Compute(rows).result_size;
}

// =========================================================================
//  DeriveDtypePromote
// =========================================================================

DataType DeriveDtypePromote(const std::vector<DataType>& dtypes) {
    DataType res = DataType::kInteger;
    for (size_t i = 0; i < dtypes.size(); ++i) {
        DataType dt = dtypes[i];
        if (dt == DataType::kString)
            throw std::invalid_argument("arithmetic: string operand not allowed");
        if (dt == DataType::kBoolean) dt = DataType::kInteger;
        if (dt == DataType::kComplex)
            res = DataType::kComplex;
        else if (dt == DataType::kReal && res != DataType::kComplex)
            res = DataType::kReal;
    }
    return res;
}

// =========================================================================
//  DeriveUnitSameDim
// =========================================================================

Unit DeriveUnitSameDim(const std::vector<Unit>& units) {
    if (units.empty()) return Unit();
    Unit res = units[0];
    for (size_t i = 1; i < units.size(); ++i) {
        if (res.same_dimension(units[i])) continue;
        if (!res.has_dimension()) { res = units[i]; continue; }
        if (!units[i].has_dimension()) continue;
        throw std::invalid_argument("unit dimension mismatch");
    }
    return res;
}

}  // namespace rel
