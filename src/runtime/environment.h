#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "value.h"  // rel::Value
#include "dataset.h"
#include "environment_config.h"
#include "function.h"

namespace rel {

// Opaque handle to a loaded function plugin.
struct LoadedPlugin;

// Builtin function libraries (defined in src/eval/builtin/).
extern FunctionLibrary kBuiltinLibrary;
extern FunctionLibrary kMathLibrary;

// =========================================================================
//  Environment — flat variable table + global shared context
// =========================================================================
//
//  A pure storage container for user-defined variables only.
//
//  Builtin constants, functions, and datasets are stored in global (static)
//  registries shared across all Environment instances.  This means:
//    - InitBuiltinConstants() / InitBuiltinFunctions() are called once.
//    - LoadFromConfig() loads datasets/plugins globally, not per-env.
//    - Define() rejects names that collide with builtin constants.
//

class REL_RUNTIME_API Environment
{
public:
    Environment() = default;

    // ---- variables (instance — user-defined only) ----

    /// Bind or rebind a name.  Overwrites existing bindings silently.
    /// Throws std::runtime_error when `name` collides with a builtin constant.
    void Define(const std::string& name, rel::Value value);

    /// Look up a user-defined variable in the flat table.
    /// Returns default Value when not found.
    rel::Value Get(const std::string& name) const;

    /// Names of all user-defined variables (unordered).
    std::vector<std::string> VariableNames() const;

    // ---- static: builtin constants ----------------------------------------

    /// Populate the global builtin-constant registry (PI, e, c0, ...).
    /// Call once during process startup.
    static void InitBuiltinConstants();

    /// Look up a builtin constant by name, or nullptr when not found.
    static const rel::Value* FindConstant(const std::string& name);

    /// Names of all registered builtin constants (unordered).
    static std::vector<std::string> ConstantNames();

    // ---- static: function registry ----------------------------------------

    /// Register REL's builtin function libraries ("builtin" + "math").
    /// Call once during process startup.
    static void InitBuiltinFunctions();

    /// Register a function in the global registry.
    /// Overwrites an existing registration with the same name silently.
    static void RegisterFunction(Function fn);

    /// Register every function in a library.
    static void RegisterLibrary(const FunctionLibrary& lib);

    /// Remove a registered function by name.
    /// Returns true when the function existed and was removed.
    static bool UnregisterFunction(const std::string& name);

    /// Look up a registered function by name, or nullptr when not found.
    static const Function* FindFunction(const std::string& name);

    /// Names of all registered functions (unordered).
    static std::vector<std::string> FunctionNames();

    // ---- static: dataset registry -----------------------------------------

    /// Register a Dataset, transferring ownership to the global registry.
    static void AddDataset(std::unique_ptr<xdataset::Dataset> ds);

    /// Remove a Dataset by name.  If it is the current default,
    /// the default is cleared.  Returns the removed Dataset, or nullptr.
    static std::unique_ptr<xdataset::Dataset> RemoveDataset(const std::string& name);

    /// Set the default Dataset for unqualified references.
    static void SetDefaultDataset(const std::string& name);

    /// The current default Dataset, or nullptr.
    static xdataset::Dataset* DefaultDataset();

    /// Names of all registered datasets (unordered).
    static std::vector<std::string> DatasetNames();

    // ---- static: persistent context ---------------------------------------

    /// Load datasets and plugins from a JSON config file.
    ///   - "datasets":         array of {name, format, path}
    ///   - "default_dataset":  optional, defaults to first dataset
    ///   - "plugin":           optional array of plugin shared-library paths
    /// Existing global registries (datasets, functions) are preserved.
    /// Call once during process startup.
    static void LoadFromConfig(const std::string& config_path);

    /// Load a function plugin (DLL / .so / .dylib) and register its functions
    /// in the global registry.  Returns an opaque handle, or nullptr on failure.
    static LoadedPlugin* LoadFunctionPlugin(const std::string& path);

    /// Unload a plugin previously returned by LoadFunctionPlugin.
    /// Unregisters the functions the plugin registered, then releases the library.
    static void UnloadFunctionPlugin(LoadedPlugin* plugin);

    // ---- direct lookups (AST-free) ----------------------------------------

    /// Look up a name in user variables, then builtin constants.
    /// Returns nullptr when not found in either.
    const rel::Value* LookupVariableOrConstant(const std::string& name) const;

    /// Find a registered Dataset by name, or nullptr if not found.
    static xdataset::Dataset* FindDataset(const std::string& name);

private:
    std::unordered_map<std::string, rel::Value> variables_;

    // ---- global (static) state --------------------------------------------
    static std::unordered_map<std::string, rel::Value>
        builtin_constants_;
    static std::unordered_map<std::string, Function>
        functions_;
    static std::unordered_map<std::string, std::unique_ptr<xdataset::Dataset>>
        datasets_;
    static std::string default_dataset_name_;
};

} // namespace rel
