#include "eval/environment_config.h"

#include <fstream>
#include <stdexcept>

namespace rel {

EnvironmentConfig EnvironmentConfig::Load(const std::string& config_path)
{
    std::ifstream in(config_path);
    if (!in)
        throw std::runtime_error("cannot open environment config: " + config_path);

    nlohmann::json j;
    try {
        in >> j;
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error(
            "JSON parse error in " + config_path + ": " + e.what());
    }

    return j.get<EnvironmentConfig>();
}

}  // namespace rel
