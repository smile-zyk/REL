#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast/expr.h"
#include "value.h"  // xdataset::Value
#include "dataset.h"
#include "environment_config.h"

namespace rel {

// =========================================================================
//  Environment — flat variable table + Dataset context
// =========================================================================
//
//  A pure storage container.  It does NOT know about language semantics
//  such as built-in constants (PI, e, ...) — those are injected from the
//  outside via init_builtin_constants().
//
//  resolve_reference() converts a parsed ReferenceExpr's segments into
//  a Value by walking the Dataset tree or looking up variables.

class Environment
{
public:
    Environment() = default;

    // ---- variables ----

    /// Bind or rebind a name.  Overwrites existing bindings silently.
    void Define(const std::string& name, xdataset::Value value);

    /// Look up a variable in the flat table.
    /// Returns default Value when not found.
    xdataset::Value Get(const std::string& name) const;

    // ---- datasets ----

    /// Register a Dataset, transferring ownership to the Environment.
    void AddDataset(std::unique_ptr<xdataset::Dataset> ds);

    /// Remove a Dataset by name.  If it is the current default,
    /// the default is cleared.  Returns the removed Dataset, or nullptr.
    std::unique_ptr<xdataset::Dataset> RemoveDataset(const std::string& name);

    /// Set the default Dataset for unqualified references.
    void SetDefaultDataset(const std::string& name);

    /// The current default Dataset, or nullptr.
    xdataset::Dataset* DefaultDataset() const;

    // ---- persistent context ----------------------------------------------

    /// Load datasets and pre-defined expressions from a config file.
    /// The first dataset entry becomes the default dataset.
    /// define entries are evaluated after all datasets are loaded.
    /// Existing datasets and variables are preserved.
    void LoadFromConfig(const std::string& config_path);

    // ---- reference resolution ----

    /// Convert parsed reference segments into a Value.
    ///
    /// Single-segment: variables_ -> built-in constants -> default dataset
    ///                 unique-name shortcut.
    /// Two-segment DDot: dataset..variable (unique across that dataset).
    /// Two-or-more Dot:  the last segment is the variable name,
    ///                    the second-to-last is the block name,
    ///                    all preceding segments form the group path.
    ///                    First segment may optionally be a dataset name.
    xdataset::Value ResolveReference(const std::vector<RefSegment>& segments) const;

private:
    std::unordered_map<std::string, xdataset::Value> variables_;
    std::unordered_map<std::string, std::unique_ptr<xdataset::Dataset>> datasets_;
    std::string default_dataset_name_;
};

} // namespace rel
