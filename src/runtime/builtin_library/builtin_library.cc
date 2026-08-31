// Builtin library: runtime introspection and DataArray utilities.
//
// datasets() / default_dataset() / variables() inspect the global Environment
// registries (datasets, variables via active env) and return the information
// as an Independent DataArray holding one String Scalar row per entry.
// what()/indep()/output() work on Values.

#include "builtin_library.h"
#include "environment.h"
#include "value.h"

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

namespace rel {
namespace builtin {
namespace {

std::string absolute_path(const std::string& rel)
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

}  // namespace

Value Datasets()
{
    const std::vector<std::string> names = Environment::DatasetNames();
    std::vector<std::string> rows;
    rows.reserve(names.size());
    for (std::size_t i = 0; i < names.size(); ++i)
        rows.push_back(names[i]);
    return Value::ArrayString(rows);
}

Value DefaultDataset()
{
    std::vector<std::string> rows;
    xdataset::Dataset* ds = Environment::DefaultDataset();
    if (ds)
        rows.push_back(ds->name());
    else
        rows.push_back("NO DEFAULT DATASET");
    return Value::ArrayString(rows);
}

Value Variables()
{
    return Value::ArrayString({});
}

Value What(const Value& v)
{
    std::vector<std::string> rows;
    rows.reserve(5);

    std::ostringstream dep;
    dep << '[';
    const std::vector<std::string>& names = v.indep_names();
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        if (i > 0) dep << ", ";
        dep << names[i];
    }
    dep << ']';

    rows.push_back("Dependency: " + dep.str());
    rows.push_back("Kind: " + std::string(
        v.is_dependent() ? "Dependent" : "Independent"));
    rows.push_back("Dimension: " + v.dimension_spec().to_string());
    rows.push_back("Data Shape: " + v.data_shape().to_string());
    rows.push_back("Data Type: " + std::string(xdataset::DataTypeToString(v.data_type())));
    if (v.unit().has_dimension())
        rows.push_back("Unit: " + v.unit().to_string());

    return Value::ArrayString(rows);
}

Value Indep(const Value& da_val, const Value& sel_val)
{
    if (!da_val.is_data_array())
        throw std::runtime_error("indep: first argument must be a DataArray");

    const xdataset::DataArray& da = da_val.as_data_array();

    if (sel_val.is_measurement() &&
        sel_val.as_measurement().data_type() == xdataset::DataType::kInteger)
    {
        int index = sel_val.as_measurement().as_scalar<int>();
        return Value(da.indep(index));
    }

    if (sel_val.is_measurement() &&
        sel_val.as_measurement().data_type() == xdataset::DataType::kString)
    {
        const std::string& name = sel_val.as_measurement().as_scalar<std::string>();
        return Value(da.indep(name));
    }

    throw std::runtime_error(
        "indep: second argument must be an Integer (index) or String (name)");
}

Value Output(const Value& v, const Value& var_name_val)
{
    if (!v.is_data_array())
        throw std::runtime_error("output: first argument must be a DataArray");

    const xdataset::Measurement& name_m = var_name_val.as_measurement();
    if (name_m.data_type() != xdataset::DataType::kString)
        throw std::runtime_error(
            "output: second argument must be a String variable name");
    const std::string& var_name = name_m.as_scalar<std::string>();

    const xdataset::DataArray& da = v.as_data_array();
    const std::string file_path = absolute_path(var_name + ".csv");
    da.GetOrCreateDataFrame(var_name).WriteToCsv(file_path);

    return Value::String(file_path);
}

Value Permute(const Value& data_val, const Value& perm_val)
{
    if (!data_val.is_data_array())
        throw std::runtime_error("permute: first argument must be a DataArray");

    const xdataset::DataArray& da = data_val.as_data_array();
    if (da.data_kind() != xdataset::DataArrayKind::kDependent)
        throw std::runtime_error(
            "permute: only dependent DataArrays can be permuted");

    const std::size_t rank = da.multi_dimension_spec().rank();

    // Collect the permutation vector as 1-based positions (1 = innermost,
    // N = outermost), listed in the desired result order innermost-first.  When omitted, the ComputedParam default supplies the
    // full reversal {N, ..., 1}.
    std::vector<int> user_perm;

    if (perm_val.is_measurement() &&
        perm_val.as_measurement().data_type() == xdataset::DataType::kInteger &&
        perm_val.as_measurement().data_kind() == xdataset::DataKind::kScalar)
    {
        user_perm.push_back(perm_val.as_measurement().as_scalar<int>());
    }
    else if (perm_val.is_measurement() &&
             perm_val.as_measurement().data_type() == xdataset::DataType::kInteger &&
             perm_val.as_measurement().data_kind() == xdataset::DataKind::kVector)
    {
        auto vec = perm_val.as_measurement().as_vector<int>();
        for (xdataset::Index i = 0; i < vec.size(); ++i)
            user_perm.push_back(vec(i));
    }
    else
    {
        throw std::runtime_error(
            "permute: second argument must be an Integer scalar or vector Measurement");
    }

    if (rank == 0)
        throw std::runtime_error("permute: DataArray has no dimensions");

    if (user_perm.size() < rank)
    {
        // Fewer than N entries: the missing (outer) dimensions -- everything
        // not referenced by the vector -- are appended at the end in their
        // natural (innermost-first) order.  Expressions stay robust when
        // outer sweeps are added.
        std::vector<bool> referenced(rank, false);
        for (int p : user_perm)
        {
            if (p < 1 || static_cast<std::size_t>(p) > rank)
                throw std::runtime_error(
                    "permute: permutation entry out of range: " + std::to_string(p));
            referenced[static_cast<std::size_t>(p) - 1] = true;
        }
        std::vector<int> padded = user_perm;
        for (std::size_t i = 0; i < rank; ++i)
            if (!referenced[i])
                padded.push_back(static_cast<int>(i + 1));
        user_perm.swap(padded);
    }

    if (user_perm.size() != rank)
        throw std::runtime_error(
            "permute: permutation has " + std::to_string(user_perm.size()) +
            " entries but the DataArray has rank " + std::to_string(rank));

    // DataArray::permute takes the same 1-based, innermost-first listing
    // (1 = innermost, N = outermost) -- pass through directly.
    std::vector<xdataset::Index> spec_perm;
    spec_perm.reserve(rank);
    for (int p : user_perm)
        spec_perm.push_back(static_cast<xdataset::Index>(p));
    return Value(da.permute(spec_perm));
}

Value Vs(const Value& dep_val, const Value& indep_val, const Value& name_val) 
{
    // Value column: unify via Value::data() (a Measurement is promoted to a
    // single row; a DataArray contributes its kSelf series).
    const xdataset::DataSeries& dep_series = dep_val.data();
    const xdataset::Index dep_rows =
        static_cast<xdataset::Index>(dep_series.size());

    std::string name;
    if (name_val.is_measurement() &&
        name_val.as_measurement().data_type() == xdataset::DataType::kString)
    {
        name = name_val.as_measurement().as_scalar<std::string>();
    }
    else
    {
        throw std::runtime_error("vs: indepName must be a String Measurement");
    }

    // Coordinate columns come entirely from the second argument, unified via
    // the Value array view (a Measurement is a single promoted row):
    //   - Dependent: its independent columns (the first `rank` entries).
    //   - Independent (including a promoted Measurement): ALL its columns --
    //     the prefix named columns plus the self series acting as the
    //     innermost coordinate column.
    const std::size_t rank = indep_val.dimension_spec().rank();
    if (rank == 0)
        throw std::runtime_error("vs: independent has no dimensions");

    std::vector<std::pair<std::string, const xdataset::DataSeries*> > cols;
    for (auto it = indep_val.datas().begin(); it != indep_val.datas().end(); ++it)
    {
        if (it->first == xdataset::DataArray::kSelf)
        {
            if (!indep_val.is_dependent())
            {
                // The self series doubles as the innermost coordinate column
                // of an Independent (this includes a promoted Measurement).
                std::string self_name = name;
                if (self_name.empty() && indep_val.has_source())
                    self_name = indep_val.source_name();
                if (self_name.empty())
                    self_name = "x";
                cols.emplace_back(self_name, &it->second);
            }
            break;   // Dependent: kSelf is the value column, not a coord.
        }
        cols.emplace_back(it->first, &it->second);
    }
    if (cols.size() != rank)
        throw std::runtime_error(
            "vs: independent coordinate column count mismatch");

    // Independent coordinates must be scalar data.
    for (std::size_t i = 0; i < cols.size(); ++i)
    {
        if (cols[i].second->data_kind() != xdataset::DataKind::kScalar)
            throw std::runtime_error(
                "vs: independent coordinate '" + cols[i].first +
                "' must be scalar");
    }

    // The grid implied by the coordinate columns must match the value
    // column's row count.
    const xdataset::Index cell_count = static_cast<xdataset::Index>(
        indep_val.dimension_spec().compute_cell_count());
    if (cell_count != dep_rows)
    {
        throw std::runtime_error(
            "vs: independent cell count " + std::to_string(cell_count) +
            " does not match dependent row count " + std::to_string(dep_rows));
    }

    xdataset::DataArrayCreateInfo info;
    info.kind = xdataset::DataArrayKind::kDependent;
    for (std::size_t i = 0; i < cols.size(); ++i)
        info.datas.emplace(cols[i].first, *cols[i].second);
    info.multi_dimension_spec = indep_val.dimension_spec();
    info.datas[xdataset::DataArray::kSelf] = dep_series;
    return Value(std::make_shared<xdataset::DataArray>(std::move(info)));
}

Value PlotVs(const Value& dep_val, const Value& indep_val)
{
    // Unified via the Value array view: the first argument's promoted array
    // is the dependent (a Measurement is a single 1-row array).
    const xdataset::DataArray& B = dep_val.as_data_array_view();
    const std::size_t rank = B.multi_dimension_spec().rank();
    if (rank == 0)
        throw std::runtime_error("plot_vs: DataArray has no dimensions");

    // ---- Case 1: independent IS one of B's independents (source match) ----
    // Only provenance counts: a DataArray obtained by direct reference
    // (block.xxx) carries its source Block + variable name (a promoted
    // Measurement never carries source).  If that variable is one of B's
    // independents, reorder so it becomes the innermost (plot) axis.
    // Derived values (no source) never match here.
    if (indep_val.has_source())
    {
        const std::vector<std::string>& names = B.indep_names();
        for (std::size_t i = 0; i < names.size(); ++i)
        {
            if (indep_val.source_block_path() == B.source_block_path() &&
                indep_val.source_name() == names[i])
            {
                // Move dimension i (spec index, 0 = outermost) to the
                // innermost (plot) position via an innermost-first perm.
                const xdataset::Index spec_idx = static_cast<xdataset::Index>(i);
                const xdataset::Index target_no =
                    static_cast<xdataset::Index>(rank) - spec_idx;
                std::vector<xdataset::Index> perm;
                perm.push_back(target_no);
                for (xdataset::Index no = static_cast<xdataset::Index>(rank); no >= 1; --no)
                    if (no != target_no)
                        perm.push_back(no);
                return Value(B.permute(perm));
            }
        }
    }

    // ---- Case 2: X has the size of the OUTERMOST independent dimension ----
    // (e.g. plot_vs(dbS11, CvalH) with CvalH = Cval/2: Cval is the outer
    // sweep.)  Treat this as a reversal permute (empty permutation:
    // outermost -> innermost), with the outermost variable's data replaced
    // by X.  Only the outermost dimension is considered; a size mismatch
    // falls through to Case 3.
    const xdataset::DataSeries& x_series = indep_val.data();
    const std::vector<std::string>& names = B.indep_names();
    if (!names.empty())
    {
        const std::string& outer_name = names[0];   // spec index 0 = outermost
        if (x_series.size() == B.indep_data(outer_name).size())
        {
            xdataset::DataArrayCreateInfo info;
            info.kind = xdataset::DataArrayKind::kDependent;
            info.multi_dimension_spec = B.multi_dimension_spec();
            std::size_t pos = 0;
            for (auto it = B.datas().begin(); it != B.datas().end(); ++it)
            {
                if (it->first == xdataset::DataArray::kSelf)
                    break;
                if (pos == 0)
                    info.datas.emplace(it->first, x_series);
                else
                    info.datas.emplace(it->first, it->second);
                ++pos;
            }
            info.datas[xdataset::DataArray::kSelf] = B.data();

            // Empty permutation = reversal {N, ..., 1}: the (now replaced)
            // outermost dimension becomes the innermost (plot) axis.
            return Value(xdataset::DataArray(std::move(info)).permute({}));
        }
    }

    // ---- Case 3: fall back to attach (vs semantics) ----
    // The independent is a full coordinate set whose grid matches the
    // dependent's row count.  Reuse Vs, which attaches the independent's
    // coordinate columns to the dependent's value column.
    return Vs(dep_val, indep_val, Value::String(""));
}

FunctionLibrary MakeLibrary()
{
    FunctionLibrary lib("builtin");

    lib.Add(Function("datasets", std::vector<FunctionParam>(),
        [](const Function::ArgMap&) { return Datasets(); }));
    lib.Add(Function("default_dataset", std::vector<FunctionParam>(),
        [](const Function::ArgMap&) { return DefaultDataset(); }));
    lib.Add(Function("variables", std::vector<FunctionParam>(),
        [](const Function::ArgMap&) { return Variables(); }));

    lib.Add(Function("what", std::vector<FunctionParam>{ Param("x") },
        [](const Function::ArgMap& args) { return What(args.at("x")); }));

    lib.Add(Function("indep",
        std::vector<FunctionParam>{
            Param("da"),
            Param("selector", Value::Integer(1)),
        },
        [](const Function::ArgMap& args) {
            return Indep(args.at("da"), args.at("selector"));
        }));

    lib.Add(Function("permute",
        std::vector<FunctionParam>{
            Param("data"),
            ComputedParam("permute_vector",
                [](const Function::ArgMap& resolved) -> Value
                {
                    // Default: full reversal of the data's dimensions,
                    // {N, ..., 2, 1} (1 = innermost, N = outermost).
                    const xdataset::DataArray& da =
                        resolved.at("data").as_data_array();
                    const std::size_t rank =
                        da.multi_dimension_spec().rank();
                    xdataset::VecXi perm(static_cast<Eigen::Index>(rank));
                    for (std::size_t i = 0; i < rank; ++i)
                        perm(static_cast<Eigen::Index>(i)) =
                            static_cast<int>(rank - i);
                    return Value::Vector(perm);
                }),
        },
        [](const Function::ArgMap& args) {
            return Permute(args.at("data"), args.at("permute_vector"));
        }));

    lib.Add(Function("vs",
        std::vector<FunctionParam>{
            Param("dependent"),
            Param("independent"),
            Param("indepName", Value::String("")),
        },
        [](const Function::ArgMap& args) {
            return Vs(args.at("dependent"), args.at("independent"),
                      args.at("indepName"));
        }));

    lib.Add(Function("plot_vs",
        std::vector<FunctionParam>{
            Param("dependent"),
            Param("independent"),
        },
        [](const Function::ArgMap& args) {
            return PlotVs(args.at("dependent"), args.at("independent"));
        }));

    lib.Add(Function("output",
        std::vector<FunctionParam>{
            Param("da"),
            Param("variable_name", Value::String("data")),
        },
        [](const Function::ArgMap& args) {
            return Output(args.at("da"), args.at("variable_name"));
        }));

    return lib;
}

}  // namespace builtin
}  // namespace rel
