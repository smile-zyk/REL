// Builtin functions: runtime introspection.
//
// These functions inspect the host Environment (datasets, default dataset,
// registered variables) and return the information as an Independent
// DataArray holding one String Scalar row per entry.

#include "rel.h"

#include "eval/environment.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <limits.h>
    #include <unistd.h>
#endif

namespace rel
{
    // ---- helpers -----------------------------------------------------------

    /// Resolve a (possibly relative) path to an absolute path.
    /// Falls back to the input on failure.
    static std::string absolute_path(const std::string& rel)
    {
#ifdef _WIN32
        char* full = _fullpath(nullptr, rel.c_str(), 0);
        if (!full)
            return rel;
        std::string result(full);
        std::free(full);
        return result;
#else
        char buf[PATH_MAX];
        if (!getcwd(buf, sizeof(buf)))
            return rel;
        return std::string(buf) + "/" + rel;
#endif
    }

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
                for (std::size_t i = 0; i < names.size(); ++i)
                    rows.push_back(names[i]);
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
                    rows.push_back(ds->name());
                else
                    rows.push_back("NO DEFAULT DATASET");
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
                for (std::size_t i = 0; i < names.size(); ++i)
                    rows.push_back(names[i]);
                return xdataset::Value::ArrayString(rows);
            });
    }

    // ---- what(x) -----------------------------------------------------------

    /// Build what(x) — inspect a Value as 5 rows of ArrayString:
    ///   Dependency / Kind / Dimension / Data Shape / Data Type.
    /// Rendering uses the shared rel::Format* helpers.
    static Function make_what()
    {
        return Function(
            "what",
            std::vector<FunctionParam>{ Param("x") },
            [](const std::vector<xdataset::Value>& args) -> xdataset::Value {
                const xdataset::Value& v = args[0];
                std::vector<std::string> rows;
                rows.reserve(5);

                if (v.is_data_array())
                {
                    const xdataset::DataArray& da = v.as_data_array();

                    // Dependency: "[a, b, c]" from the independent names.
                    std::ostringstream dep;
                    dep << '[';
                    const std::vector<std::string>& names = da.indep_names();
                    for (std::size_t i = 0; i < names.size(); ++i)
                    {
                        if (i > 0) dep << ", ";
                        dep << names[i];
                    }
                    dep << ']';

                    rows.push_back("Dependency: " + dep.str());
                    rows.push_back("Kind: " + std::string(
                        da.data_kind() == xdataset::DataArrayKind::kDependent
                            ? "Dependent" : "Independent"));
                    rows.push_back("Dimension: " + FormatDimensionSpec(da.multi_dimension_spec()));
                    rows.push_back("Data Shape: " + FormatDataShape(da.data().data_kind(),
                                                                    da.data().data_shape()));
                    rows.push_back("Data Type: " + FormatDataType(da.data().data_type()));
                }
                else
                {
                    const xdataset::Measurement& m = v.as_measurement();
                    rows.push_back("Dependency: []");
                    rows.push_back("Kind: Independent");
                    rows.push_back("Dimension: []");
                    rows.push_back("Data Shape: " + FormatDataShape(m.data_kind(), m.shape()));
                    rows.push_back("Data Type: " + FormatDataType(m.data_type()));
                }

                return xdataset::Value::ArrayString(rows);
            });
    }

    // ---- indep(da, selector) -----------------------------------------------

    /// Build indep(da, selector = 1) — extract an independent variable from a
    /// DataArray.  `selector` is either an Integer (1-based index, default 1)
    /// or a String (independent variable name); it is forwarded to
    /// DataArray::indep.
    static Function make_indep()
    {
        return Function(
            "indep",
            std::vector<FunctionParam>{
                Param("da"),
                Param("selector", xdataset::Value::Integer(1)),
            },
            [](const std::vector<xdataset::Value>& args) -> xdataset::Value {
                const xdataset::Value& da_val = args[0];
                const xdataset::Value& sel_val = args[1];

                if (!da_val.is_data_array())
                    throw std::runtime_error("indep: first argument must be a DataArray");

                const xdataset::DataArray& da = da_val.as_data_array();

                if (sel_val.is_measurement() &&
                    sel_val.as_measurement().data_type() == xdataset::DataType::kInteger)
                {
                    // 1-based index, matching DataArray::indep's convention.
                    int index = sel_val.as_measurement().as_scalar<int>();
                    return xdataset::Value(da.indep(index));
                }

                if (sel_val.is_measurement() &&
                    sel_val.as_measurement().data_type() == xdataset::DataType::kString)
                {
                    const std::string& name = sel_val.as_measurement().as_scalar<std::string>();
                    return xdataset::Value(da.indep(name));
                }

                throw std::runtime_error(
                    "indep: second argument must be an Integer (index) or String (name)");
            });
    }

    // ---- min(da) / max(da) -------------------------------------------------

    /// Shared implementation for min(da) and max(da): reduce along the
    /// innermost dimension of a DataArray (via DataArray::min/max).
    /// Only DataArrays are accepted.
    static Function make_reduce_minmax(const char* name, bool want_max)
    {
        return Function(
            name,
            std::vector<FunctionParam>{ Param("da") },
            [name, want_max](const std::vector<xdataset::Value>& args) -> xdataset::Value {
                const xdataset::Value& v = args[0];
                if (!v.is_data_array())
                    throw std::runtime_error(std::string(name) +
                                             ": argument must be a DataArray");
                return xdataset::Value(want_max
                                           ? v.as_data_array().max()
                                           : v.as_data_array().min());
            });
    }

    /// Build min(da).
    static Function make_min()
    {
        return make_reduce_minmax("min", /*want_max=*/false);
    }

    /// Build max(da).
    static Function make_max()
    {
        return make_reduce_minmax("max", /*want_max=*/true);
    }

    // ---- output(da, variable_name = "data") --------------------------------

    /// Build output(da, variable_name = "data") — write a DataArray's
    /// DataFrame view to "<variable_name>.csv" in the current directory.
    /// Returns a String Measurement carrying the absolute path written.
    static Function make_output()
    {
        return Function(
            "output",
            std::vector<FunctionParam>{
                Param("da"),
                Param("variable_name", xdataset::Value::String("data")),
            },
            [](const std::vector<xdataset::Value>& args) -> xdataset::Value {
                const xdataset::Value& v = args[0];
                if (!v.is_data_array())
                    throw std::runtime_error("output: first argument must be a DataArray");

                const xdataset::Measurement& name_m = args[1].as_measurement();
                if (name_m.data_type() != xdataset::DataType::kString)
                    throw std::runtime_error(
                        "output: second argument must be a String variable name");
                const std::string& var_name = name_m.as_scalar<std::string>();

                const xdataset::DataArray& da = v.as_data_array();
                const std::string file_path = absolute_path(var_name + ".csv");
                da.GetOrCreateDataFrame(var_name).WriteToCsv(file_path);

                return xdataset::Value::String(file_path);
            });
    }

    /// Build the full builtin library.
    static FunctionLibrary make_builtin_library(Environment& env)
    {
        FunctionLibrary lib("builtin");
        lib.Add(make_print_datasets(env));
        lib.Add(make_print_dataset(env));
        lib.Add(make_print_variables(env));
        lib.Add(make_what());
        lib.Add(make_indep());
        lib.Add(make_min());
        lib.Add(make_max());
        lib.Add(make_output());
        return lib;
    }

    void InitBuiltinFunctions(Environment& env)
    {
        env.RegisterLibrary(make_builtin_library(env));
    }

} // namespace rel
