#pragma once

#include "function.h"
#include "value.h"

namespace rel {
namespace builtin {

// =============================================================================
//  Builtin operation C++ API
// =============================================================================

/// datasets() -- list every registered Dataset name.
Value Datasets();

/// default_dataset() -- current default Dataset name.
Value DefaultDataset();

/// variables() -- return empty list.
Value Variables();

/// what(x) -- inspect a Value: Dependency, Kind, Dimension, Data Shape, Data Type, Unit.
Value What(const Value& v);

/// indep(da, selector = Integer(1)) -- extract an independent variable from a DataArray.
/// selector: Integer = 1-based index, String = independent variable name.
Value Indep(const Value& da, const Value& selector);

/// permute(data, permute_vector) -- reorder the independent dimensions of a
/// DataArray.  The result is always a Dependent DataArray.
///   - permute_vector: an Integer scalar or vector Measurement, listing the
///     desired result order innermost-first (1 = innermost, N = outermost).  Identity is {1,2,...,N}; the default
///     (omitted / empty) reverses to {N,...,1}.  A shorter vector is padded
///     by appending the missing outer dims in their natural order.
Value Permute(const Value& data, const Value& permute_vector);

/// vs(dependent, independent, indepName = "") -- attach coordinates to a
/// value column.  Builds a new Dependent DataArray whose value column is
/// `dependent`'s data and whose coordinate columns come entirely from
/// `independent` (unified via the Value array view; a Measurement is a
/// single promoted row):
///   - Dependent independent: its independent columns.
///   - Independent independent: ALL its columns (the self series acts as the
///     innermost coordinate column, named indepName / its source name / "x").
/// All coordinate columns must be scalar data, and the implied grid
/// (cell count) must equal the dependent's row count.
Value Vs(const Value& dependent, const Value& independent,
         const Value& indepName);

/// plot_vs(dependent, independent) -- plot `dependent` against `independent`
/// as the innermost (X) axis.
///   - If `independent` carries source provenance (a direct reference such as
///     `block.xxx`) naming one of the dependent's independents, the
///     dimensions are permuted so that variable becomes the innermost axis.
///   - Else if `independent` has the same size as the OUTERMOST independent
///     dimension, that dimension's data is replaced by `independent` and the
///     axes are reversed (empty permutation), so the replaced outermost
///     dimension becomes the innermost (plot) axis (e.g. plot_vs(dbS11,
///     CvalH) with CvalH = Cval/2).
///   - Else if it has the same size as the dependent's row count, it is
///     attached as a new 1-D coordinate (vs semantics).
///   - Otherwise an error is raised.
Value PlotVs(const Value& dependent, const Value& independent);

/// output(da, variable_name = String("data")) -- write DataFrame to "<name>.csv".
/// Returns the absolute file path as a String Measurement.
Value Output(const Value& da, const Value& variable_name);

// =============================================================================
//  Library factory
// =============================================================================

/// Build the "builtin" function library.
FunctionLibrary MakeLibrary();

}  // namespace builtin
}  // namespace rel
