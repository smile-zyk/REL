#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ast/expr.h"
#include "eval/value.h"

namespace xdataset {
    class Dataset;
} // namespace xdataset

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
    void define(const std::string& name, Value value);

    /// Look up a variable in the flat table.
    /// Returns Value::null_value() when not found.
    Value get(const std::string& name) const;

    // ---- datasets ----

    /// Register a Dataset (does NOT take ownership).
    void add_dataset(xdataset::Dataset* ds);

    /// Set the default Dataset for unqualified references.
    void set_default_dataset(const std::string& name);

    /// The current default Dataset, or nullptr.
    xdataset::Dataset* default_dataset() const;

    // ---- reference resolution ----

    /// Convert parsed reference segments into a Value.
    ///
    /// Single-segment: variables_ → built-in constants → default dataset
    ///                 unique-name shortcut.
    /// Two-segment DDot: dataset..variable (unique across that dataset).
    /// Two-or-more Dot:  the last segment is the variable name,
    ///                    the second-to-last is the block name,
    ///                    all preceding segments form the group path.
    ///                    First segment may optionally be a dataset name.
    Value resolve_reference(const std::vector<RefSegment>& segments) const;

private:
    tsl::ordered_map<std::string, Value> variables_;
    tsl::ordered_map<std::string, xdataset::Dataset*> datasets_;
    std::string default_dataset_name_;
};

// =========================================================================
//  Free function — language semantics
// =========================================================================

/// Populate `env` with REL's built-in numeric constants (PI, e, etc.).
void init_builtin_constants(Environment& env);

} // namespace rel
