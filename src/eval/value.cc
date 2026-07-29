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

std::string Value::Format(const std::string& name, int max_rows) const
{
    if (is_null())
        return "NULL";

    if (is_measurement())
    {
        const xdataset::Measurement& m = as_measurement();
        return m.to_dataframe(name).to_string();
    }

    // DataArray: render with custom or default variable name
    const xdataset::DataArray& da = as_data_array();
    const std::string& header = name.empty() ? "data" : name;
    return da.GetOrCreateDataFrame(header).to_string(max_rows);
}

// =========================================================================
//  Factory helpers
// =========================================================================

Value Value::Null()
{
    return Value();
}

Value Value::Real(double v)
{
    return Value(xdataset::Measurement::Real(v));
}

Value Value::Integer(int v)
{
    return Value(xdataset::Measurement::Integer(v));
}

Value Value::BooleanValue(bool b)
{
    return Value(xdataset::Measurement::Integer(b ? 1 : 0));
}

Value Value::String(const std::string& s)
{
    return Value(xdataset::Measurement::String(s));
}

} // namespace rel
