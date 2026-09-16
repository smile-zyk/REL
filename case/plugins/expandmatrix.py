# =============================================================================
#  expandmatrix.py — REL Python plugin (loaded via test_env.json "python_plugins")
#
#  Registers two variants of the same operation:
#    expandmatrix(X)     -> rel builtin version (units preserved)
#    expandmatrix_np(X)  -> pure numpy version  (units discarded)
#
#  Both stack every row of X vertically into one matrix.
#
#  Port of the AEL helper:
#
#      defun expandmatrix(X)
#      {
#        decl a=sweep_size(X);
#        decl b;
#        decl newX=X[0];
#        for ( b=0; b<a-1; b++)
#         {
#          newX={{newX},{X[b+1]}};
#          }
#          return newX;
#       }
#
#  Semantics: X's rows are concatenated TOP-TO-BOTTOM (vertical stack), which
#  in REL is the `{{A},{B}}` block form — exposed as the builtin `vertcat`
#  (MATLAB [A; B] / vertcat).  Each row of X becomes a block row of the result:
#
#      X = [ {{1,2},{3,4}}, {{5,6},{7,8}} ]   (2 rows of 2x2 cells)
#      expandmatrix(X) -> 4x2 matrix:
#          1 2
#          3 4
#          5 6
#          7 8
#
# =============================================================================
#  Variant 1: expandmatrix -- pure rel, units preserved
# =============================================================================
#
#  Implementation notes:
#   - Uses only the builtin rel function `vertcat` plus the rel Value API —
#     no numpy round-trip, so units are preserved automatically.
#   - `rel.eval` runs against a FRESH Environment and cannot see the argument
#     X, so the loop is driven from Python over the exported rows.
#   - X.data() is Value::data(), i.e. as_data_array_view().data(): a bare
#     Measurement is lazily promoted to a 1-row DataArray, so this works for
#     both Measurement and DataArray inputs.
#   - X.rows is Value::rows() == data().size(), which is the same count that
#     sweep_size() reports (dimension_spec().compute_cell_count()).  Using it
#     avoids converting a scalar Measurement back into a Python number.
#   - X.data()[i] yields the i-th row as a Measurement, which is exactly the
#     "block" that vertcat stacks.
# =============================================================================

import numpy as np
import rel


def expandmatrix(args):
    X = args["X"]

    # Number of rows (== sweep_size(X)), read straight off the Value.
    n = X.rows

    if n == 0:
        return X

    # newX = X[0]; then newX = {{newX},{X[b+1]}} for each remaining row.
    series = X.data()
    newX = series[0]
    for b in range(n - 1):
        newX = rel.vertcat(newX, series[b + 1])

    return newX


# =============================================================================
#  Variant 2: expandmatrix_np -- pure numpy, units DISCARDED
# =============================================================================
#
#  Units are dropped on purpose: the numeric round-trip through numpy is
#  unitless, so the result carries no unit.  Use expandmatrix() when the unit
#  must be preserved.
#
#  Shape conventions (see make_series_buffer in xdataset_bindings.cc): the
#  first buffer axis is always the row count, so np.asarray(Value) gives
#      scalar cells -> (N,)
#      vector cells -> (N, w)
#      matrix cells -> (N, r, c)
#  and the vertical stack is just a reshape along that first axis.
# =============================================================================

def expandmatrix_np(args):
    X = args["X"]

    # Zero-copy export (numeric data uses the buffer protocol).
    arr = np.asarray(X)

    if arr.ndim == 0:
        # A single Measurement (already one cell) -- nothing to stack.
        return X

    n_rows = arr.shape[0]
    if n_rows == 0:
        return X

    # Vertical stack: concatenate the rows along the cell's row axis.
    #   scalar cells (N,)      -> (N, 1)  column, matching {{x0},{x1},...}
    #   vector cells (N, w)    -> (N, w)  already one row per entry
    #   matrix cells (N, r, c) -> (N*r, c)
    if arr.ndim == 1:
        stacked = arr.reshape(n_rows, 1)
    elif arr.ndim == 2:
        stacked = arr
    else:
        stacked = arr.reshape(n_rows * arr.shape[1], arr.shape[2])

    # Re-import as ONE matrix cell.  from_array() reads the first buffer axis
    # as the row count, so a 2-d (R, C) array would become R separate vector
    # rows -- the stack must be wrapped as (1, R, C) to land as a single
    # R x C matrix cell.
    r, c = stacked.shape
    cell = np.ascontiguousarray(stacked.reshape(1, r, c))

    # Returning the DataSeries directly is enough: the plugin shim's
    # from_python() sees a buffer, rebuilds it via dataseries_from_buffer(),
    # and -- because it holds exactly one entry -- unwraps it to
    # Value(ds.measurement_at(0)), i.e. the same single Measurement.
    return rel.DataSeries.from_array(cell)


rel.register_function("expandmatrix", [
    rel.Param("X"),
], expandmatrix)

rel.register_function("expandmatrix_np", [
    rel.Param("X"),
], expandmatrix_np)
