#include "environment.h"

#include "dataset.h"
#include "dataset_io.h"
#include "eval/evaluator.h"
#include "parser/parser.h"
#include "scanner/scanner.h"
#include "touchstone_io.h"

#include <sstream>
#include <stdexcept>

namespace rel {

// =========================================================================
//  Variables
// =========================================================================

void Environment::Define(const std::string& name, xdataset::Value value)
{
    variables_[name] = std::move(value);
}

xdataset::Value Environment::Get(const std::string& name) const
{
    auto it = variables_.find(name);
    if (it != variables_.end())
        return it->second;
    return xdataset::Value();
}

// =========================================================================
//  Datasets
// =========================================================================

void Environment::AddDataset(std::unique_ptr<xdataset::Dataset> ds)
{
    std::string name = ds->name();
    datasets_[std::move(name)] = std::move(ds);
}

std::unique_ptr<xdataset::Dataset> Environment::RemoveDataset(const std::string& name)
{
    auto it = datasets_.find(name);
    if (it == datasets_.end())
        return nullptr;

    if (default_dataset_name_ == name)
        default_dataset_name_.clear();

    auto ds = std::move(it->second);
    datasets_.erase(it);
    return ds;
}

void Environment::SetDefaultDataset(const std::string& name)
{
    default_dataset_name_ = name;
}

xdataset::Dataset* Environment::DefaultDataset() const
{
    if (default_dataset_name_.empty())
        return nullptr;
    auto it = datasets_.find(default_dataset_name_);
    if (it != datasets_.end())
        return it->second.get();
    return nullptr;
}

// =========================================================================
//  Functions
// =========================================================================

void Environment::RegisterFunction(Function fn)
{
    functions_[fn.name()] = std::move(fn);
}

const Function* Environment::FindFunction(const std::string& name) const
{
    auto it = functions_.find(name);
    if (it != functions_.end())
        return &it->second;
    return nullptr;
}

// =========================================================================
//  Introspection
// =========================================================================

std::vector<std::string> Environment::DatasetNames() const
{
    std::vector<std::string> names;
    names.reserve(datasets_.size());
    for (const auto& kv : datasets_)
        names.push_back(kv.first);
    return names;
}

std::vector<std::string> Environment::VariableNames() const
{
    std::vector<std::string> names;
    names.reserve(variables_.size());
    for (const auto& kv : variables_)
        names.push_back(kv.first);
    return names;
}

// =========================================================================
//  Reference resolution
// =========================================================================

xdataset::Value Environment::ResolveReference(
    const std::vector<RefSegment>& segments) const
{
    if (segments.empty())
        return xdataset::Value();

    // ---- 1 segment: variable lookup ---------------------------------
    if (segments.size() == 1)
    {
        const std::string& name = segments[0].name;

        // 1a) User-defined / built-in
        auto it = variables_.find(name);
        if (it != variables_.end())
            return it->second;

        // 1b) Unique lookup in default dataset
        xdataset::Dataset* ds = DefaultDataset();
        if (ds && ds->HasUniqueDataArray(name))
        {
            return xdataset::Value(ds->GetDataArray(name));
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

        xdataset::Dataset* ds = it->second.get();
        return xdataset::Value(ds->GetDataArray(segments[1].name));
    }

    // ---- ≥2 segments Dot: path navigation ---------------------------
    {
        // Determine which Dataset to use.
        xdataset::Dataset* ds = DefaultDataset();
        std::size_t start = 0;

        auto it = datasets_.find(segments[0].name);
        if (it != datasets_.end())
        {
            ds = it->second.get();
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

        return xdataset::Value(ds->GetDataArray(path.str(), segments.back().name));
    }
}

// =========================================================================
//  Persistent context: load datasets + pre-defined expressions
// =========================================================================

void Environment::LoadFromConfig(const std::string& config_path)
{
    EnvironmentConfig cfg = EnvironmentConfig::Load(config_path);

    // Resolve relative dataset paths against the config file's directory.
    std::string base_dir;
    {
        std::size_t slash = config_path.find_last_of("/\\");
        if (slash != std::string::npos)
            base_dir = config_path.substr(0, slash + 1);
    }

    // ---- load datasets ----
    for (auto& ds : cfg.datasets)
    {
        // Resolve relative paths
        std::string full_path = ds.path;
        if (!full_path.empty() && full_path[0] != '/' && !base_dir.empty())
            full_path = base_dir + ds.path;

        xdataset::Dataset loaded(ds.name);

        if (ds.format == "hdf5")
        {
            loaded = xdataset::DatasetIO::Load(ds.format, full_path);
            loaded.set_name(ds.name);
        }
        else if (ds.format == "touchstone")
        {
            xdataset::TouchstoneReader reader(full_path);
            loaded = reader.Read();
            loaded.set_name(ds.name);
        }
        else
        {
            throw std::runtime_error(
                "unsupported dataset format '" + ds.format + "'");
        }

        datasets_[ds.name] = std::unique_ptr<xdataset::Dataset>(
            new xdataset::Dataset(std::move(loaded)));
    }

    // First dataset entry → default
    if (!cfg.datasets.empty())
        SetDefaultDataset(cfg.datasets[0].name);

    // ---- evaluate define entries ----
    for (const auto& def : cfg.defines)
    {
        Scanner scanner(def.expression);
        ScanResult sr = scanner.Scan();
        if (!sr.Ok())
            throw std::runtime_error(
                "config 'define " + def.name + "': " + sr.errors[0].to_string());

        Parser parser(std::move(sr.tokens));
        ParseResult pr = parser.Parse();
        if (!pr.Ok())
            throw std::runtime_error(
                "config 'define " + def.name + "': " + pr.errors[0].message);

        Evaluator evaluator(*this);
        xdataset::Value result = evaluator.Evaluate(*pr.expr);
        Define(def.name, std::move(result));
    }
}

} // namespace rel
