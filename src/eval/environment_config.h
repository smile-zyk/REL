#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace rel {

// =========================================================================
//  EnvironmentConfig — persistent dataset + variable context (JSON)
// =========================================================================
//
//  JSON schema:
//  {
//    "datasets": [
//      { "name": "noise",    "format": "hdf5", "path": "/data/noise.xdataset" },
//      { "name": "amplifier","format": "hdf5", "path": "/data/amp.xdataset" }
//    ],
//    "defines": [
//      { "name": "k",  "expression": "3 * PI" },
//      { "name": "r",  "expression": "Vout / ifreq" }
//    ]
//  }
//
//  The first dataset becomes the default.

struct DatasetConfig {
    std::string name;
    std::string format;
    std::string path;
};

struct DefineEntry {
    std::string name;
    std::string expression;
};

struct EnvironmentConfig {
    std::vector<DatasetConfig> datasets;
    std::vector<DefineEntry>   defines;

    /// Load from a JSON file.
    static EnvironmentConfig Load(const std::string& config_path);
};

// nlohmann/json deserialization
inline void from_json(const nlohmann::json& j, DatasetConfig& ds) {
    j.at("name").get_to(ds.name);
    j.at("format").get_to(ds.format);
    j.at("path").get_to(ds.path);
}
inline void from_json(const nlohmann::json& j, DefineEntry& de) {
    j.at("name").get_to(de.name);
    j.at("expression").get_to(de.expression);
}
inline void from_json(const nlohmann::json& j, EnvironmentConfig& cfg) {
    if (j.contains("datasets"))
        j.at("datasets").get_to(cfg.datasets);
    if (j.contains("defines"))
        j.at("defines").get_to(cfg.defines);
}

}  // namespace rel
