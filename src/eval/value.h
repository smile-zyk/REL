#pragma once

#include <boost/variant.hpp>

#include <memory>
#include <string>
#include "data_array.h"
#include "measurement.h"

namespace rel {

// =========================================================================
//  Value — unified result type for REL expression evaluation
// =========================================================================
//
//  Value is a three-way variant:
//    boost::blank               — null (NullExpr result, default-constructed)
//    xdataset::Measurement      — scalar / vector / matrix + unit (by value)
//    shared_ptr<DataArray>      — named variable with coordinate axes
//
//  Measurement is stored by value (~64 bytes on the stack); DataArray is
//  stored via shared_ptr to avoid deep copies when the same array is
//  returned through multiple evaluation paths.
//
//  Arithmetic, comparison, logical, bitwise, shift, and modulo operations
//  are NOT members of Value.  They are implemented in the Evaluator (which
//  delegates to the corresponding xdataset free functions).

class Value
{
public:
    using Storage = boost::variant<
        boost::blank,
        xdataset::Measurement,
        std::shared_ptr<xdataset::DataArray>
    >;

    // ---- construction --------------------------------------------------

    /// Default: null (boost::blank).
    Value();

    /// Implicit from Measurement.
    Value(xdataset::Measurement m);  // NOLINT(runtime/explicit)

    /// Implicit from DataArray shared_ptr.
    Value(std::shared_ptr<xdataset::DataArray> da);  // NOLINT(runtime/explicit)

    // Copy / move: compiler-generated is fine (variant + shared_ptr are
    // both deep-copyable / movable).
    Value(const Value&) = default;
    Value& operator=(const Value&) = default;
    Value(Value&&) = default;
    Value& operator=(Value&&) = default;

    // ---- type queries --------------------------------------------------

    /// True when this Value is null (boost::blank).
    bool is_null() const;

    /// True when this Value holds a Measurement.
    bool is_measurement() const;

    /// True when this Value holds a DataArray.
    bool is_data_array() const;

    // ---- accessors (throw boost::bad_get on type mismatch) -------------

    xdataset::Measurement& as_measurement();
    const xdataset::Measurement& as_measurement() const;

    xdataset::DataArray& as_data_array();
    const xdataset::DataArray& as_data_array() const;

    // ---- formatting ----------------------------------------------------

    /// Human-readable string.
    /// When `name` is empty: Measurement renders inline (e.g. "3.14 GHz"),
    /// DataArray renders as DataFrame with a default header.
    /// When `name` is given: Measurement is wrapped in a named DataFrame;
    /// DataArray uses the name as its header.  `max_rows` caps output rows
    /// (0 = no limit).
    std::string Format(const std::string& name = "data", int max_rows = 32) const;

    // ---- raw storage ---------------------------------------------------

    const Storage& storage() const { return storage_; }

    // ---- convenience factories -----------------------------------------

    static Value Null();
    static Value Real(double v);
    static Value Integer(int v);
    static Value BooleanValue(bool b);       // -> Integer 1 or 0
    static Value String(const std::string& s);

private:
    Storage storage_;
};

} // namespace rel
