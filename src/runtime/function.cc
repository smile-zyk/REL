// =============================================================================
//  REL -- Function implementation (default value resolution, invocation)
// =============================================================================

#include "function.h"

#include <sstream>
#include <stdexcept>
#include <string>

namespace rel
{

    Value Function::Invoke(const ArgMap& user_args) const
    {
        if (!impl_)
            throw std::runtime_error("function '" + name_ + "' has no implementation");

        ArgMap resolved;
        for (std::size_t i = 0; i < params_.size(); ++i)
        {
            const std::string& pname = params_[i].name;
            auto it = user_args.find(pname);
            if (it != user_args.end())
            {
                resolved[pname] = it->second;
            }
            else if (params_[i].has_computed_default)
            {
                resolved[pname] = params_[i].computed_default(resolved);  // only depends on preceding params
            }
            else if (params_[i].has_default)
            {
                resolved[pname] = params_[i].default_value;
            }
            else
            {
                std::ostringstream oss;
                oss << "missing argument '" << pname
                    << "' for function '" << name_ << "'";
                throw std::runtime_error(oss.str());
            }
        }

        return impl_(resolved);
    }

}  // namespace rel
