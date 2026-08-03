// Builtin functions: runtime introspection.
//
// These functions inspect the host Environment (datasets, default dataset,
// registered variables) and return the information as an Independent
// DataArray holding one String Scalar row per entry.

#include "rel.h"

#include "eval/environment.h"

#include <string>
#include <vector>

namespace rel
{
    // ---- function factories ------------------------------------------------
    //
    //  Each make_xxx() builds the full Function object — name, parameter list
    //  (including defaults), and implementation — in one place.

    /// Build print_datasets() — list every registered Dataset.
    static Function make_print_datasets(Environment& env)
    {
        return Function(
            "print_datasets",
            std::vector<FunctionParam>(),
            [&env](const std::vector<xdataset::Value>&) -> xdataset::Value {
                std::vector<std::string> names = env.DatasetNames();
                std::vector<std::string> rows;
                rows.reserve(names.size() + 1);
                rows.push_back("datasets (" + std::to_string(names.size()) + "):");
                for (std::size_t i = 0; i < names.size(); ++i)
                    rows.push_back("  " + names[i]);
                return xdataset::Value::ArrayString(rows);
            });
    }

    /// Build print_dataset() — the current default Dataset.
    static Function make_print_dataset(Environment& env)
    {
        return Function(
            "print_dataset",
            std::vector<FunctionParam>(),
            [&env](const std::vector<xdataset::Value>&) -> xdataset::Value {
                std::vector<std::string> rows;
                xdataset::Dataset* ds = env.DefaultDataset();
                if (ds)
                    rows.push_back("current dataset: " + ds->name());
                else
                    rows.push_back("current dataset: (none)");
                return xdataset::Value::ArrayString(rows);
            });
    }

    /// Build print_variables() — every variable registered in the Environment.
    static Function make_print_variables(Environment& env)
    {
        return Function(
            "print_variables",
            std::vector<FunctionParam>(),
            [&env](const std::vector<xdataset::Value>&) -> xdataset::Value {
                std::vector<std::string> names = env.VariableNames();
                std::vector<std::string> rows;
                rows.reserve(names.size() + 1);
                rows.push_back("variables (" + std::to_string(names.size()) + "):");
                for (std::size_t i = 0; i < names.size(); ++i)
                    rows.push_back("  " + names[i]);
                return xdataset::Value::ArrayString(rows);
            });
    }

    void InitBuiltinFunctions(Environment& env)
    {
        env.RegisterFunction(make_print_datasets(env));
        env.RegisterFunction(make_print_dataset(env));
        env.RegisterFunction(make_print_variables(env));
    }

} // namespace rel
