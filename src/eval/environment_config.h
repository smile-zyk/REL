#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace rel {

// =========================================================================
//  EnvironmentConfig — persistent dataset + plugin context (JSON)
// =========================================================================
//
//  JSON schema:
//  {
//    "datasets": [
//      { "name": "noise",    "format": "hdf5", "path": "/data/noise.xdataset" },
//      { "name": "amplifier","format": "hdf5", "path": "/data/amp.xdataset" }
//    ],
//    "default_dataset": "noise",
//    "plugin": [
//      "/path/to/my_plugin.dylib"
//    ]
//  }
//
//  "default_dataset" is optional; if omitted, the first dataset in "datasets"
//  becomes the default.  "plugin" is an optional array of shared-library paths.

struct DatasetConfig {
    std::string name;
    std::string format;
    std::string path;
};

struct EnvironmentConfig {
    std::vector<DatasetConfig> datasets;
    std::string               default_dataset;
    std::vector<std::string>  plugins;

    /// Load from a JSON file.
    static EnvironmentConfig Load(const std::string& config_path);
};

// nlohmann/json deserialization
inline void from_json(const nlohmann::json& j, DatasetConfig& ds) {
    j.at("name").get_to(ds.name);
    j.at("format").get_to(ds.format);
    j.at("path").get_to(ds.path);
}
inline void from_json(const nlohmann::json& j, EnvironmentConfig& cfg) {
    if (j.contains("datasets"))
        j.at("datasets").get_to(cfg.datasets);
    if (j.contains("default_dataset"))
        j.at("default_dataset").get_to(cfg.default_dataset);
    if (j.contains("plugin"))
        j.at("plugin").get_to(cfg.plugins);
}

}  // namespace rel
