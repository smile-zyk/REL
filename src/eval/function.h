#pragma once

#include "value.h"  // xdataset::Value

#include <functional>
#include <string>
#include <vector>

namespace rel
{
    // =====================================================================
    //  Function registry — host-registered callable functions
    // =====================================================================
    //
    //  A registered function may declare parameters with optional default
    //  values.  Call sites may omit any parameter slot (per REL's default
    //  argument slots, e.g. `func(a, , c)`), and omitted slots are filled
    //  with the declared default.  Defaults do not need to be trailing:
    //  any parameter that has a default may be skipped at the call site.
    //
    //  Slot resolution (evaluating explicit arguments, filling omitted slots
    //  with defaults) is the caller's job — the Evaluator queries
    //  HasDefault()/DefaultValue() per slot and passes the fully-resolved
    //  argument list to Invoke().

    /// Native implementation of a registered REL function.
    typedef std::function<xdataset::Value(const std::vector<xdataset::Value>&)> NativeFunction;

    /// One parameter of a function; may carry a default value.
    struct FunctionParam
    {
        std::string name;
        bool has_default = false;
        xdataset::Value default_value;

        FunctionParam() = default;

        /// Required parameter (no default).
        explicit FunctionParam(std::string name_value)
            : name(std::move(name_value))
        {}

        /// Parameter with a fixed default value.
        FunctionParam(std::string name_value, xdataset::Value default_value_value)
            : name(std::move(name_value))
            , has_default(true)
            , default_value(std::move(default_value_value))
        {}
    };

    /// A user-registered function with parameter defaults.
    class Function
    {
    public:
        Function() = default;

        Function(std::string name_value,
                 std::vector<FunctionParam> params_value,
                 NativeFunction impl_value);

        const std::string& name() const { return name_; }
        const std::vector<FunctionParam>& params() const { return params_; }
        const NativeFunction& impl() const { return impl_; }

        /// Number of declared parameters.
        std::size_t arity() const { return params_.size(); }

        /// True when the parameter at `index` declares a default value.
        bool HasDefault(std::size_t index) const;

        /// Default value of the parameter at `index`.
        /// Throws std::out_of_range when `index` is out of range, and
        /// std::logic_error when the parameter has no default.
        const xdataset::Value& DefaultValue(std::size_t index) const;

        /// Invoke the implementation with the fully-resolved argument list
        /// (defaults filled in, in declaration order).
        ///
        /// Throws std::runtime_error when:
        ///   - the argument count exceeds the declared arity,
        ///   - the function has no implementation.
        xdataset::Value Invoke(const std::vector<xdataset::Value>& args) const;

    private:
        std::string name_;
        std::vector<FunctionParam> params_;
        NativeFunction impl_;
    };

} // namespace rel
