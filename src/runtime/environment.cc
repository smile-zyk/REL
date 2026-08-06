#include "environment.h"

#include "dataset.h"
#include "dataset_io.h"
#include "touchstone_io.h"

#include <stdexcept>

namespace rel {

// =========================================================================
//  Static member definitions
// =========================================================================

std::unordered_map<std::string, rel::Value>
    Environment::builtin_constants_;
std::unordered_map<std::string, Function>
    Environment::functions_;
std::unordered_map<std::string, std::unique_ptr<xdataset::Dataset>>
    Environment::datasets_;
std::string Environment::default_dataset_name_;

// =========================================================================
//  Variables (instance)
// =========================================================================

void Environment::Define(const std::string& name, rel::Value value)
{
    if (builtin_constants_.find(name) != builtin_constants_.end())
        throw std::runtime_error(
            "cannot redefine builtin constant '" + name + "'");
    variables_[name] = std::move(value);
}

rel::Value Environment::Get(const std::string& name) const
{
    auto it = variables_.find(name);
    if (it != variables_.end())
        return it->second;
    return rel::Value();
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
//  Static: builtin constants
// =========================================================================

void Environment::InitBuiltinConstants()
{
    builtin_constants_["PI"]        = rel::Value::Real(3.1415926535898);
    builtin_constants_["pi"]        = rel::Value::Real(3.1415926535898);
    builtin_constants_["e"]         = rel::Value::Real(2.718281822);
    builtin_constants_["ln10"]      = rel::Value::Real(2.302585093);
    builtin_constants_["boltzmann"] = rel::Value::Real(1.380658e-23);
    builtin_constants_["qelectron"] = rel::Value::Real(1.60217733e-19);
    builtin_constants_["planck"]    = rel::Value::Real(6.6260755e-34);
    builtin_constants_["c0"]        = rel::Value::Real(2.99792e+08);
    builtin_constants_["e0"]        = rel::Value::Real(8.85419e-12);
    builtin_constants_["u0"]        = rel::Value::Real(12.5664e-07);
    builtin_constants_["tinyReal"]  = rel::Value::Real(2.2e-308);
    builtin_constants_["hugeReal"]  = rel::Value::Real(3.4e+38);
}

const rel::Value* Environment::FindConstant(const std::string& name)
{
    auto it = builtin_constants_.find(name);
    if (it != builtin_constants_.end())
        return &it->second;
    return nullptr;
}

std::vector<std::string> Environment::ConstantNames()
{
    std::vector<std::string> names;
    names.reserve(builtin_constants_.size());
    for (const auto& kv : builtin_constants_)
        names.push_back(kv.first);
    return names;
}

// =========================================================================
//  Static: functions
// =========================================================================

void Environment::InitBuiltinFunctions()
{
    RegisterLibrary(kBuiltinLibrary);
    RegisterLibrary(kMathLibrary);
}

void Environment::RegisterFunction(Function fn)
{
    functions_[fn.name()] = std::move(fn);
}

void Environment::RegisterLibrary(const FunctionLibrary& lib)
{
    for (const auto& fn : lib.functions())
        RegisterFunction(fn);
}

bool Environment::UnregisterFunction(const std::string& name)
{
    return functions_.erase(name) > 0;
}

const Function* Environment::FindFunction(const std::string& name)
{
    auto it = functions_.find(name);
    if (it != functions_.end())
        return &it->second;
    return nullptr;
}

std::vector<std::string> Environment::FunctionNames()
{
    std::vector<std::string> names;
    names.reserve(functions_.size());
    for (const auto& kv : functions_)
        names.push_back(kv.first);
    return names;
}

// =========================================================================
//  Static: datasets
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

xdataset::Dataset* Environment::DefaultDataset()
{
    if (default_dataset_name_.empty())
        return nullptr;
    auto it = datasets_.find(default_dataset_name_);
    if (it != datasets_.end())
        return it->second.get();
    return nullptr;
}

std::vector<std::string> Environment::DatasetNames()
{
    std::vector<std::string> names;
    names.reserve(datasets_.size());
    for (const auto& kv : datasets_)
        names.push_back(kv.first);
    return names;
}

// =========================================================================
//  Direct lookups (AST-free)
// =========================================================================

const rel::Value* Environment::LookupVariableOrConstant(
    const std::string& name) const
{
    auto it = variables_.find(name);
    if (it != variables_.end())
        return &it->second;

    return FindConstant(name);
}

xdataset::Dataset* Environment::FindDataset(const std::string& name)
{
    auto it = datasets_.find(name);
    if (it != datasets_.end())
        return it->second.get();
    return nullptr;
}

// =========================================================================
//  Persistent context: load datasets + plugins from config
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

    // ---- set default dataset ----
    if (!cfg.default_dataset.empty())
    {
        SetDefaultDataset(cfg.default_dataset);
    }
    else if (!cfg.datasets.empty())
    {
        SetDefaultDataset(cfg.datasets[0].name);
    }

    // ---- load plugins ----
    for (const auto& plugin_path : cfg.plugins)
    {
        std::string full_plugin_path = plugin_path;
        if (!full_plugin_path.empty() && full_plugin_path[0] != '/' && !base_dir.empty())
            full_plugin_path = base_dir + plugin_path;

        LoadedPlugin* plugin = LoadFunctionPlugin(full_plugin_path);
        if (!plugin)
        {
            throw std::runtime_error(
                "failed to load plugin: " + full_plugin_path);
        }
        // Store for later cleanup? For now plugins live until process exit.
        // In a full implementation, you'd track handles for UnloadFunctionPlugin.
        (void)plugin;
    }
}

} // namespace rel
