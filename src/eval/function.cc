#include "function.h"

#include <sstream>
#include <stdexcept>

namespace rel
{

Function::Function(std::string name_value,
                   std::vector<FunctionParam> params_value,
                   NativeFunction impl_value)
    : name_(std::move(name_value))
    , params_(std::move(params_value))
    , impl_(std::move(impl_value))
{}

bool Function::HasDefault(std::size_t index) const
{
    return index < params_.size() && params_[index].has_default;
}

const xdataset::Value& Function::DefaultValue(std::size_t index) const
{
    if (index >= params_.size())
        throw std::out_of_range("Function::DefaultValue: parameter index out of range");
    if (!params_[index].has_default)
        throw std::logic_error("Function::DefaultValue: parameter has no default value");
    return params_[index].default_value;
}

xdataset::Value Function::Invoke(const std::vector<xdataset::Value>& args) const
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

} // namespace rel
