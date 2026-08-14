#pragma once

#include <string>
#include <vector>
#include <rapidjson/document.h>

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
//    "default_dataset": "noise"
//  }
//
//  "default_dataset" is optional; if omitted, the first dataset in "datasets"
//  becomes the default.

struct DatasetConfig {
    std::string name;
    std::string format;
    std::string path;
};

struct EnvironmentConfig {
    std::vector<DatasetConfig> datasets;
    std::string               default_dataset;

    /// Load from a JSON file.
    static EnvironmentConfig Load(const std::string& config_path);

private:
    static DatasetConfig     ParseDataset(const rapidjson::Value& v);
    static EnvironmentConfig Parse(const rapidjson::Document& doc);
};

}  // namespace rel
