#pragma once

#include "value.h"  // rel::Value

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
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
    //
    //  Everything in this header is inline so that external function plugins
    //  can construct Function objects without linking against rel_core.

    /// Native implementation of a registered REL function.
    typedef std::function<rel::Value(const std::vector<rel::Value>&)> NativeFunction;

    /// One parameter of a function; may carry a default value.
    struct FunctionParam
    {
        std::string name;
        bool has_default = false;
        rel::Value default_value;

        FunctionParam() = default;

        /// Required parameter (no default).
        explicit FunctionParam(std::string name_value)
            : name(std::move(name_value))
        {}

        /// Parameter with a fixed default value.
        FunctionParam(std::string name_value, rel::Value default_value_value)
            : name(std::move(name_value))
            , has_default(true)
            , default_value(std::move(default_value_value))
        {}
    };

    /// Convenience: a required parameter.
    inline FunctionParam Param(std::string name)
    {
        return FunctionParam(std::move(name));
    }

    /// Convenience: a parameter with a default value.
    inline FunctionParam Param(std::string name, rel::Value default_value)
    {
        return FunctionParam(std::move(name), std::move(default_value));
    }

    /// A user-registered function with parameter defaults.
    class Function
    {
    public:
        Function() = default;

        Function(std::string name_value,
                 std::vector<FunctionParam> params_value,
                 NativeFunction impl_value)
            : name_(std::move(name_value))
            , params_(std::move(params_value))
            , impl_(std::move(impl_value))
        {}

        const std::string& name() const { return name_; }
        const std::vector<FunctionParam>& params() const { return params_; }
        const NativeFunction& impl() const { return impl_; }

        /// Number of declared parameters.
        std::size_t arity() const { return params_.size(); }

        /// True when the parameter at `index` declares a default value.
        bool HasDefault(std::size_t index) const
        {
            return index < params_.size() && params_[index].has_default;
        }

        /// Default value of the parameter at `index`.
        /// Throws std::out_of_range when `index` is out of range, and
        /// std::logic_error when the parameter has no default.
        const rel::Value& DefaultValue(std::size_t index) const
        {
            if (index >= params_.size())
                throw std::out_of_range("Function::DefaultValue: parameter index out of range");
            if (!params_[index].has_default)
                throw std::logic_error("Function::DefaultValue: parameter has no default value");
            return params_[index].default_value;
        }

        /// Invoke the implementation with the fully-resolved argument list
        /// (defaults filled in, in declaration order).
        ///
        /// Throws std::runtime_error when:
        ///   - the argument count exceeds the declared arity,
        ///   - the function has no implementation.
        rel::Value Invoke(const std::vector<rel::Value>& args) const
        {
            if (!impl_)
                throw std::runtime_error("function '" + name_ + "' has no implementation");

            if (args.size() > params_.size())
            {
                std::ostringstream oss;
                oss << "function '" << name_ << "' expects at most " << params_.size()
                    << " argument(s), got " << args.size();
                throw std::runtime_error(oss.str());
            }

            return impl_(args);
        }

    private:
        std::string name_;
        std::vector<FunctionParam> params_;
        NativeFunction impl_;
    };

    /// A named collection of functions, registerable in one call
    /// (see Environment::RegisterLibrary).
    class FunctionLibrary
    {
    public:
        FunctionLibrary() = default;

        explicit FunctionLibrary(std::string name)
            : name_(std::move(name))
        {}

        const std::string& name() const { return name_; }
        const std::vector<Function>& functions() const { return functions_; }

        std::size_t size() const { return functions_.size(); }
        bool empty() const { return functions_.empty(); }

        /// Append one function.
        FunctionLibrary& Add(Function fn)
        {
            functions_.push_back(std::move(fn));
            return *this;
        }

        /// Build and append one function from its parts.
        FunctionLibrary& Add(const std::string& name,
                             std::vector<FunctionParam> params,
                             NativeFunction impl)
        {
            return Add(Function(name, std::move(params), std::move(impl)));
        }

    private:
        std::string name_;
        std::vector<Function> functions_;
    };

} // namespace rel
