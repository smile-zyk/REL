#include "value.h"

#include "data_frame.h"
#include "measurement.h"

#include <sstream>

namespace rel {

// =========================================================================
//  Construction
// =========================================================================

Value::Value() : storage_(boost::blank()) {}

Value::Value(xdataset::Measurement m) : storage_(std::move(m)) {}

Value::Value(std::shared_ptr<xdataset::DataArray> da) : storage_(std::move(da)) {}

// =========================================================================
//  Type queries
// =========================================================================

bool Value::is_null() const
{
    return storage_.which() == 0;
}

bool Value::is_measurement() const
{
    return storage_.which() == 1;
}

bool Value::is_data_array() const
{
    return storage_.which() == 2;
}

// =========================================================================
//  Accessors
// =========================================================================

xdataset::Measurement& Value::as_measurement()
{
    return boost::get<xdataset::Measurement>(storage_);
}

const xdataset::Measurement& Value::as_measurement() const
{
    return boost::get<xdataset::Measurement>(storage_);
}

xdataset::DataArray& Value::as_data_array()
{
    return *boost::get<std::shared_ptr<xdataset::DataArray>>(storage_);
}

const xdataset::DataArray& Value::as_data_array() const
{
    return *boost::get<std::shared_ptr<xdataset::DataArray>>(storage_);
}

// =========================================================================
//  Formatting
// =========================================================================

std::string Value::to_string() const
{
    if (is_null())
        return "NULL";

    if (is_measurement())
    {
        const xdataset::Measurement& m = as_measurement();
        return m.to_string();
    }

    // DataArray
    const xdataset::DataArray& da = as_data_array();
    return da.to_string();
}

std::string Value::to_string(const std::string& name)
{
    if (is_null())
        return "NULL";

    if (is_measurement())
    {
        const xdataset::Measurement& m = as_measurement();
        return m.to_dataframe(name).to_string();
    }

    // DataArray
    xdataset::DataArray& da = as_data_array();
    da.set_name(name);
    return da.to_string();
}

// =========================================================================
//  Factory helpers
// =========================================================================

Value Value::null_value()
{
    return Value();
}

Value Value::real(double v)
{
    return Value(xdataset::Measurement::Real(v));
}

Value Value::integer(int v)
{
    return Value(xdataset::Measurement::Integer(v));
}

Value Value::boolean_value(bool b)
{
    return Value(xdataset::Measurement::Integer(b ? 1 : 0));
}

Value Value::string_value(const std::string& s)
{
    return Value(xdataset::Measurement::String(s));
}

} // namespace rel
