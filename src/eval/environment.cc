#include "environment.h"

#include "dataset.h"

#include <tsl/ordered_map.h>

#include <sstream>
#include <stdexcept>

namespace rel {

// =========================================================================
//  Construction
// =========================================================================

Environment::Environment()
{
    // --- Built-in constants (REL.md, 内建常量) ------------------------
    // Values are in SI base units; unit annotations are informational only.
    define("PI",        Value::real(3.1415926535898));
    define("pi",        Value::real(3.1415926535898));
    define("e",         Value::real(2.718281822));
    define("ln10",      Value::real(2.302585093));
    define("boltzmann", Value::real(1.380658e-23));
    define("qelectron", Value::real(1.60217733e-19));
    define("planck",    Value::real(6.6260755e-34));
    define("c0",        Value::real(2.99792e+08));
    define("e0",        Value::real(8.85419e-12));
    define("u0",        Value::real(12.5664e-07));
    define("tinyReal",  Value::real(2.2e-308));
    define("hugeReal",  Value::real(3.4e+38));
}

// =========================================================================
//  Variables
// =========================================================================

void Environment::define(const std::string& name, Value value)
{
    variables_[name] = std::move(value);
}

Value Environment::get(const std::string& name) const
{
    auto it = variables_.find(name);
    if (it != variables_.end())
        return it->second;
    return Value::null_value();
}

// =========================================================================
//  Datasets
// =========================================================================

void Environment::add_dataset(xdataset::Dataset* ds)
{
    datasets_[ds->name()] = ds;
}

void Environment::set_default_dataset(const std::string& name)
{
    default_dataset_name_ = name;
}

xdataset::Dataset* Environment::default_dataset() const
{
    if (default_dataset_name_.empty())
        return nullptr;
    auto it = datasets_.find(default_dataset_name_);
    if (it != datasets_.end())
        return it->second;
    return nullptr;
}

// =========================================================================
//  Reference resolution
// =========================================================================

Value Environment::resolve_reference(
    const std::vector<RefSegment>& segments) const
{
    if (segments.empty())
        return Value::null_value();

    // ---- 1 segment: variable lookup ---------------------------------
    if (segments.size() == 1)
    {
        const std::string& name = segments[0].name;

        // 1a) User-defined / built-in
        Value v = get(name);
        if (!v.is_null())
            return v;

        // 1b) Unique lookup in default dataset
        xdataset::Dataset* ds = default_dataset();
        if (ds && ds->HasUniqueDataArray(name))
        {
            return Value(std::make_shared<xdataset::DataArray>(
                ds->GetDataArray(name)));
        }

        throw std::runtime_error(
            "undefined variable '" + name + "'");
    }

    // ---- 2 segments DDot: dataset..unique_variable ------------------
    if (segments.size() == 2 && segments[1].sep == RefSeparator::DDot)
    {
        auto it = datasets_.find(segments[0].name);
        if (it == datasets_.end())
        {
            throw std::runtime_error(
                "unknown Dataset '" + segments[0].name + "'");
        }

        xdataset::Dataset* ds = it->second;
        return Value(std::make_shared<xdataset::DataArray>(
            ds->GetDataArray(segments[1].name)));
    }

    // ---- ≥2 segments Dot: path navigation ---------------------------
    {
        // Determine which Dataset to use.
        xdataset::Dataset* ds = default_dataset();
        std::size_t start = 0;

        auto it = datasets_.find(segments[0].name);
        if (it != datasets_.end())
        {
            ds = it->second;
            start = 1;
        }

        if (!ds)
        {
            throw std::runtime_error(
                "no default Dataset set; cannot resolve '" +
                segments[0].name + "'");
        }

        // segments[start .. n-3]: group path (join with '/')
        // segments[n-2]:         block name
        // segments[n-1]:         variable name
        if (segments.size() < start + 2)
        {
            throw std::runtime_error(
                "reference needs at least block.variable after path");
        }

        std::ostringstream path;
        for (std::size_t i = start; i + 2 < segments.size(); ++i)
        {
            if (i > start) path << "/";
            path << segments[i].name;
        }
        if (path.tellp() > 0) path << "/";
        path << segments[segments.size() - 2].name;

        return Value(std::make_shared<xdataset::DataArray>(
            ds->GetDataArray(path.str(), segments.back().name)));
    }
}

// =========================================================================
//  Built-in lookup (static helper)
// =========================================================================

Value Environment::lookup_builtin(const std::string& name)
{
    // All built-ins are registered via define() in the constructor,
    // so get() on a default-constructed Environment does the job.
    // This static helper exists for callers that don't have an
    // Environment handy.
    Environment tmp;
    return tmp.get(name);
}

} // namespace rel
