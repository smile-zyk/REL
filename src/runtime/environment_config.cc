#include "environment.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace rel {

namespace
{

// Read a string member if present and a string, else return `def`.
std::string GetString(const boost::json::object& o, const char* key,
                      const std::string& def = std::string())
{
    auto it = o.find(key);
    if (it == o.end() || !it->value().is_string())
        return def;
    const boost::json::string& s = it->value().as_string();
    return std::string(s.data(), s.size());
}

}  // anonymous namespace

DatasetConfig EnvironmentConfig::ParseDataset(const boost::json::object& v)
{
    DatasetConfig ds;
    ds.name   = GetString(v, "name");
    ds.format = GetString(v, "format");
    ds.path   = GetString(v, "path");
    return ds;
}

EnvironmentConfig EnvironmentConfig::Parse(const boost::json::object& doc)
{
    EnvironmentConfig cfg;

    auto it = doc.find("datasets");
    if (it != doc.end() && it->value().is_array())
    {
        for (const auto& ds : it->value().as_array())
        {
            if (ds.is_object())
                cfg.datasets.push_back(ParseDataset(ds.as_object()));
        }
    }

    cfg.default_dataset = GetString(doc, "default_dataset");

    it = doc.find("python_plugins");
    if (it != doc.end() && it->value().is_array())
    {
        for (const auto& p : it->value().as_array())
        {
            if (p.is_string())
            {
                const boost::json::string& s = p.as_string();
                cfg.python_plugins.push_back(std::string(s.data(), s.size()));
            }
        }
    }

    return cfg;
}

EnvironmentConfig EnvironmentConfig::Load(const std::string& config_path)
{
    std::ifstream in(config_path);
    if (!in)
        throw std::runtime_error("cannot open environment config: " + config_path);

    std::stringstream buf;
    buf << in.rdbuf();

    boost::json::value parsed;
    try
    {
        parsed = boost::json::parse(buf.str());
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error("JSON parse error in " + config_path +
                                 ": " + e.what());
    }
    if (!parsed.is_object())
        throw std::runtime_error("JSON root must be an object: " + config_path);

    return Parse(parsed.as_object());
}

}  // namespace rel
