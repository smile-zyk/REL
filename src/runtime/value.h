#pragma once

#include "rel_runtime_api.h"

#include <boost/variant.hpp>

#include <memory>
#include <string>
#include <vector>
#include "data_array.h"
#include "measurement.h"
#include "xdataset_predefine.h"
#include "unit.h"

namespace rel {
// Bring all xdataset types into rel namespace -- Value is the core bridge
// between rel and xdataset, and its API uses xdataset types extensively.
using namespace xdataset;
// =========================================================================
//  Value �?unified value type for Measurement and DataArray
// =========================================================================
//
//  Value is a two-way variant:
//    Measurement                 �?scalar / vector / matrix + unit (by value)
//    shared_ptr<DataArray>       �?named variable with coordinate axes
//
//  Measurement is stored by value (~64 bytes on the stack); DataArray is
//  stored via shared_ptr to avoid deep copies when the same array is
//  returned through multiple evaluation paths.
//
//  Calling data() on a Measurement-backed Value auto-converts it to an
//  Independent DataArray so that the unified mutation API works seamlessly.

class REL_RUNTIME_API Value
{
public:
    // =====================================================================
    //  FlatData<T> — typed flat pointer + optional owning storage
    // =====================================================================
    //
    //  Returned by flat_data<T>().  Replaces the old FlatInput<T> pattern
    //  in operation.cc.  Measurement-backed Values are auto-converted to a
    //  single-row DataSeries; DataArray-backed Values borrow the underlying
    //  DataSeries when the dtype already matches, or copy+promote otherwise.

    template <typename T>
    struct FlatData {
        std::unique_ptr<DataSeries> owner;  // owns memory when conversion needed
        const T*                    ptr;    // contiguous T data pointer
        Index                       stride; // T-elements per logical row
    };

public:
    // ---- construction --------------------------------------------------

    /// Default: Measurement Integer 0
    Value();

    /// Implicit from Measurement.
    Value(Measurement m);  // NOLINT(runtime/explicit)

    /// Implicit from DataArray (wraps in shared_ptr).
    Value(const DataArray& da);  // NOLINT(runtime/explicit)

    /// Implicit from DataArray shared_ptr.
    Value(std::shared_ptr<DataArray> da);  // NOLINT(runtime/explicit)

    // Copy / move: compiler-generated is fine (variant + shared_ptr are
    // both deep-copyable / movable).
    Value(const Value&) = default;
    Value& operator=(const Value&) = default;
    Value(Value&&) = default;
    Value& operator=(Value&&) = default;

    // ---- type queries --------------------------------------------------

    /// True when this Value holds a Measurement.
    bool is_measurement() const;

    /// True when this Value holds a DataArray.
    bool is_data_array() const;

    // ---- accessors (throw boost::bad_get on type mismatch) -------------

    Measurement& as_measurement();
    const Measurement& as_measurement() const;

    DataArray& as_data_array();
    const DataArray& as_data_array() const;

    // ---- unified metadata ----------------------------------------------

    DataKind  data_kind() const;
    DataType  data_type() const;
    DataShape data_shape() const;
    const Unit& unit() const;
    Index     rows() const;         // Measurement = 1, DataArray = data().size()
    Index     element_count() const;

    // ---- unified inspection (replaces is_measurement/is_data_array branching) -

    /// Ordered names of independent variables.  Empty for Measurement.
    std::vector<std::string> indep_names() const;

    /// True when this Value holds a Dependent DataArray.
    /// Always false for Measurement.
    bool is_dependent() const;

    /// Multi-dimension spec.  Measurement returns a single regular dim of
    /// size 1; DataArray delegates to multi_dimension_spec().
    MultiDimensionSpec dimension_spec() const;

    // ---- convenience queries -------------------------------------------

    bool is_scalar() const { return data_kind() == DataKind::kScalar; }
    bool is_vector() const { return data_kind() == DataKind::kVector; }
    bool is_matrix() const { return data_kind() == DataKind::kMatrix; }

    // ---- canonicalization ----------------------------------------------

    /// Return a canonicalized copy (multiplier absorbed, unit = base SI).
    /// Measurement: delegates to Measurement::canonicalized().
    /// DataArray: canonicalizes kSelf DataSeries, preserves indep dims.
    Value canonicalized() const;

    /// True when already canonical (no-op for canonicalized()).
    bool is_canonicalized() const;

    // ---- flat data access ----------------------------------------------

    /// Acquire typed flat data from this Value.
    /// Measurement: converts to a single-row DataSeries in the target type.
    /// DataArray: borrows the underlying DataSeries when dtype matches;
    ///            copies + promotes otherwise.
    template <typename T>
    FlatData<T> flat_data() const;

    // ---- data / indep_data access --------------------------------------

    /// Return a copy of self data.  Measurement: creates a 1-row DataSeries
    /// from the Measurement.  DataArray: returns a copy of the underlying series.
    DataSeries data() const;

    /// Independent variable data by 1-based index (innermost-first).
    /// Measurement-backed: returns a single-row DataSeries with int 0
    /// (the index of the single value).
    /// DataArray-backed: delegates to DataArray::indep_data().
    DataSeries indep_data(Index index) const;

    /// Independent variable data by name.
    /// Measurement-backed: throws runtime_error.
    /// DataArray-backed: delegates to DataArray::indep_data().
    DataSeries indep_data(const std::string& name) const;

    /// Extract an independent variable as a Value (Independent DataArray).
    /// Indep index is 1-based, innermost-first (1 = innermost).
    /// Measurement-backed: throws runtime_error.
    /// DataArray-backed: delegates to DataArray::indep().
    Value indep(Index index = 1) const;

    /// Extract an independent variable by name.
    Value indep(const std::string& name) const;

    // ---- leaf / group iteration ---------------------------------------

    /// Visit groups at a given independent dimension level.
    /// Indep index is 1-based, innermost-first.
    /// Measurement-backed: single group spanning the single row when index==1.
    /// DataArray-backed: delegates to DataArray::for_each_indep_group().
    void for_each_indep_group(
        Index indep_index,
        const MultiDimensionSpec::DimGroupVisitor& visitor) const;

    /// Visit every leaf row in row-major order.
    /// Measurement-backed: single leaf row.
    /// DataArray-backed: delegates to DataArray::for_each_leaf_row().
    void for_each_leaf_row(
        const MultiDimensionSpec::LeafRowVisitor& visitor) const;

    /// Visit leaf rows in [start_flat_row, end_flat_row).
    void for_each_leaf_row(
        const MultiDimensionSpec::LeafRowVisitor& visitor,
        Index start_flat_row, Index end_flat_row) const;

    // ---- setters (mutate in place) -------------------------------------

    /// Replace the entire Value with a Measurement.
    void set_data(Measurement value);

    /// Replace self data in place.  Measurement-backed Values are
    /// auto-converted first.  Invalidates the DataFrame cache.
    void set_data(DataSeries new_self);

    /// Replace the value at a specific row in the self data series.
    /// Measurement-backed Values are auto-converted first.
    void set_data(Index row, Measurement value);

    /// Replace the last independent data series wholesale.
    void set_indep_data(DataSeries new_series);

    /// Replace an independent data series wholesale by index.
    void set_indep_data(Index indep_index, DataSeries new_series);

    /// Replace an independent data series wholesale by name.
    void set_indep_data(const std::string& indep_name, DataSeries new_series);

    /// Replace the value at a specific row in an independent data series by index.
    void set_indep_data(Index indep_index, Index row, Measurement value);

    /// Replace the value at a specific row in an independent data series by name.
    void set_indep_data(const std::string& indep_name, Index row, Measurement value);

    /// Return a deep copy of this Value.
    Value clone() const;

    // ---- formatting ----------------------------------------------------

    /// Human-readable string.
    /// When `name` is empty: Measurement renders inline (e.g. "3.14 GHz"),
    /// DataArray renders as DataFrame with a default header.
    /// When `name` is given: Measurement is wrapped in a named DataFrame;
    /// DataArray uses the name as its header.  `max_rows` caps output rows
    /// (0 = no limit).
    std::string Format(const std::string& name = "data", int max_rows = 32) const;

    // ---- convenience factories -----------------------------------------

    /// @{
    /// Scalar factories with optional unit (default: dimensionless).
    /// Boolean and String values cannot carry a physical unit, so their
    /// factories take no unit.
    static Value Boolean(bool b);
    static Value String(const std::string& s);
    static Value Complex(std::complex<double> v, const Unit& u = Unit());
    static Value Real(double v, const Unit& u = Unit());
    static Value Integer(int v, const Unit& u = Unit());
    /// @}

    /// @{
    /// Vector factories (1-d).
    static Value Vector(const VecXd& v, const Unit& u = Unit());
    static Value Vector(const VecXi& v, const Unit& u = Unit());
    static Value Vector(const VecXcd& v, const Unit& u = Unit());
    static Value Vector(const VecXs& v);
    /// @}

    /// @{
    /// Matrix factories (2-d).
    static Value Matrix(const MatXd& m, const Unit& u = Unit());
    static Value Matrix(const MatXi& m, const Unit& u = Unit());
    static Value Matrix(const MatXcd& m, const Unit& u = Unit());
    static Value Matrix(const MatXs& m);
    /// @}

    /// @{
    /// Independent DataArray factories: build a single-column (scalar) DataArray
    /// Value directly from a flat std::vector.  Each entry becomes one row.
    static Value ArrayReal(const std::vector<double>& v, const Unit& u = Unit());
    static Value ArrayInteger(const std::vector<int>& v, const Unit& u = Unit());
    static Value ArrayComplex(const std::vector<std::complex<double>>& v,
                              const Unit& u = Unit());
    static Value ArrayString(const std::vector<std::string>& v);
    /// @}

    /// @{
    /// Independent DataArray factories with vector cells: each element of
    /// `rows` becomes one row (a 1-d vector cell).  All rows must share the
    /// same width.
    static Value ArrayVector(const std::vector<VecXd>& rows, const Unit& u = Unit());
    static Value ArrayVector(const std::vector<VecXi>& rows, const Unit& u = Unit());
    static Value ArrayVector(const std::vector<VecXcd>& rows, const Unit& u = Unit());
    static Value ArrayVector(const std::vector<VecXs>& rows);
    /// @}

    /// @{
    /// Independent DataArray factories with matrix cells: each element of
    /// `rows` becomes one row (a 2-d matrix cell).  All rows must share the
    /// same shape.
    static Value ArrayMatrix(const std::vector<MatXd>& rows, const Unit& u = Unit());
    static Value ArrayMatrix(const std::vector<MatXi>& rows, const Unit& u = Unit());
    static Value ArrayMatrix(const std::vector<MatXcd>& rows, const Unit& u = Unit());
    static Value ArrayMatrix(const std::vector<MatXs>& rows);
    /// @}

    // ---- operators ------------------------------------------------------

    /// @{
    /// Binary operators (delegate to rel::operation::OperationXxx kernels).
    Value operator+(const Value& rhs) const;
    Value operator-(const Value& rhs) const;
    Value operator*(const Value& rhs) const;
    Value operator/(const Value& rhs) const;
    Value operator%(const Value& rhs) const;

    Value operator==(const Value& rhs) const;
    Value operator!=(const Value& rhs) const;
    Value operator<(const Value& rhs) const;
    Value operator>(const Value& rhs) const;
    Value operator<=(const Value& rhs) const;
    Value operator>=(const Value& rhs) const;

    Value operator&&(const Value& rhs) const;
    Value operator||(const Value& rhs) const;

    Value operator&(const Value& rhs) const;
    Value operator|(const Value& rhs) const;
    Value operator^(const Value& rhs) const;
    Value operator<<(const Value& rhs) const;
    Value operator>>(const Value& rhs) const;

    /// Unary operators.
    Value operator-() const;
    Value operator!() const;
    Value operator~() const;

    /// Exponentiation (delegates to OperationPow).
    Value pow(const Value& exponent) const;
    /// @}

private:
    typedef boost::variant<
        Measurement,
        std::shared_ptr<DataArray>
    > Storage;
    Storage storage_;
};

}  // namespace rel

// =========================================================================
//  Value::flat_data<T>() — template implementation
// =========================================================================
//
//  Defined here (in header) because it is a template that must be visible
//  to all translation units using it.

namespace rel {

template <typename T>
Value::FlatData<T> Value::flat_data() const {
    if (is_measurement()) {
        FlatData<T> fd;
        const Measurement& m = as_measurement();

        fd.owner = std::unique_ptr<DataSeries>(
            new DataSeries(m.data_type(), m.shape()));
        fd.owner->append(m);

        DataType target = DataTypeOf<T>::tag;
        if (fd.owner->data_type() != target) {
            fd.owner = std::unique_ptr<DataSeries>(
                new DataSeries(fd.owner->promoted_data_type(target)));
        }
        fd.ptr    = fd.owner->template contiguous_data<T>();
        fd.stride = static_cast<Index>(fd.owner->element_count());
        return fd;
    }

    // DataArray path
    FlatData<T> fd;
    const DataSeries& src = as_data_array().data();
    if (src.data_type() == DataTypeOf<T>::tag) {
        // borrow directly — no copy
        fd.ptr    = src.contiguous_data<T>();
        fd.stride = static_cast<Index>(src.element_count());
    } else {
        DataType target = DataTypeOf<T>::tag;
        fd.owner = std::unique_ptr<DataSeries>(
            new DataSeries(src.promoted_data_type(target)));
        fd.ptr    = fd.owner->template contiguous_data<T>();
        fd.stride = static_cast<Index>(fd.owner->element_count());
    }
    return fd;
}

}  // namespace rel
