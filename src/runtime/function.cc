// =============================================================================
//  REL -- Function implementation (default value resolution, invocation)
// =============================================================================

#include "function.h"

#include <sstream>
#include <stdexcept>
#include <string>

namespace rel
{

    const Value& Function::DefaultValue(std::size_t index) const
    {
        if (index >= params_.size())
            throw std::out_of_range("Function::DefaultValue: parameter index out of range");
        if (!params_[index].has_default)
            throw std::logic_error("Function::DefaultValue: parameter has no default value");
        return params_[index].default_value;
    }

    Value Function::Invoke(const std::vector<Value>& args) const
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

}  // namespace rel
