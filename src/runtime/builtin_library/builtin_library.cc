// Builtin library: runtime introspection and DataArray utilities.
//
// datasets() / default_dataset() / variables() inspect the global Environment
// registries (datasets, variables via active env) and return the information
// as an Independent DataArray holding one String Scalar row per entry.
// what()/indep()/output() work on Values.

#include "builtin_library.h"
#include "environment.h"
#include "operation/operator.h"
#include "value.h"

#include <cmath>
#include <complex>
#include <cstdlib>
#include <limits>
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
    // Measurement inputs are accepted: as_data_array_view() lazily promotes
    // them to a 1-row Independent DataArray, so indep(1) yields the single
    // leaf index 0.  Only the name form can fail (no named independents).
    const xdataset::DataArray& da = da_val.as_data_array_view();

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

Value SweepSize(const Value& da)
{
    // Fully expanded row count = product of every dimension's size.
    // Measurement-backed Values are lazily promoted to a 1-row array, so a
    // Measurement always reports 1 (the promoted single row).
    const xdataset::MultiDimensionSpec& spec = da.dimension_spec();
    return Value::Integer(static_cast<int>(spec.compute_cell_count()));
}

Value SweepDim(const Value& da)
{
    // Number of independent dimensions (the rank).  Measurement-backed
    // Values promote to a single-dimension 1-row array, so they report 1.
    const xdataset::MultiDimensionSpec& spec = da.dimension_spec();
    return Value::Integer(static_cast<int>(spec.rank()));
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

// =========================================================================
//  Marker functions: mark / x_mark / y_mark
// =========================================================================
//
//  A "marker" is a highlighted (x, y) point on a plot of da's data against
//  the innermost independent axis.  All three functions return a 2-column
//  Dependent DataArray:
//      column 1: the x coordinate (innermost independent; for an Independent
//                DataArray the leaf-position index series),
//      column 2: the y coordinate (da's data value at that point).
//
//  Closeness semantics:
//    - mark(da, x, y): 2-D Euclidean distance, each axis normalized by its
//      own min-max span (mixed units stay comparable).
//    - x_mark(da, x) / y_mark(da, y): 1-D distance along x / data, evaluated
//      within EACH innermost slice -> one row per slice (multi-dim data
//      yields multiple rows).
//
//  Complex data: y distances compare by magnitude | |y_i| - |y| |.

namespace
{

    /// Measurement -> double (scalar Real/Integer; Complex uses .real()).
    const auto measurement_to_double = [](const xdataset::Measurement& m,
                                          const char* what) -> double
    {
        if (m.data_kind() != xdataset::DataKind::kScalar)
        {
            throw std::runtime_error(std::string(what) +
                " must be a scalar Measurement");
        }
        switch (m.data_type())
        {
            case xdataset::DataType::kReal:   return m.as_scalar<double>();
            case xdataset::DataType::kInteger: return m.as_scalar<int>();
            case xdataset::DataType::kComplex:
                return m.as_scalar<std::complex<double>>().real();
            default:
                throw std::runtime_error(std::string(what) +
                    " must be Real or Integer");
        }
    };

    /// True when the series holds scalar Real or Integer data.
    const auto is_real_scalar_series = [](const xdataset::DataSeries& s) -> bool
    {
        return s.data_kind() == xdataset::DataKind::kScalar &&
               (s.data_type() == xdataset::DataType::kReal ||
                s.data_type() == xdataset::DataType::kInteger);
    };

    /// Scalar Real/Integer row as double (no unit conversion; callers
    /// canonicalize first if units are involved).
    const auto scalar_series_at = [](const xdataset::DataSeries& s,
                                     xdataset::Index i) -> double
    {
        if (s.data_type() == xdataset::DataType::kReal)
            return s.scalar_at<double>(i);
        return static_cast<double>(s.scalar_at<int>(i));
    };

    /// |value| of a data row (Complex -> magnitude); only used for real/comp.
    const auto data_magnitude = [](const xdataset::DataSeries& s,
                                   xdataset::Index i) -> double
    {
        switch (s.data_type())
        {
            case xdataset::DataType::kReal:
                return std::abs(s.scalar_at<double>(i));
            case xdataset::DataType::kInteger:
                return std::abs(static_cast<double>(s.scalar_at<int>(i)));
            default:
                return std::abs(s.scalar_at<std::complex<double>>(i));
        }
    };

    /// Distance of data row `i` to target `y`: |value - y|, Complex uses
    /// magnitude | |y_i| - |y| |.
    const auto data_value_distance = [](const xdataset::DataSeries& s,
                                        xdataset::Index i, double y) -> double
    {
        switch (s.data_type())
        {
            case xdataset::DataType::kReal:
                return std::abs(s.scalar_at<double>(i) - y);
            case xdataset::DataType::kInteger:
                return std::abs(static_cast<double>(s.scalar_at<int>(i)) - y);
            default:
                return std::abs(data_magnitude(s, i) - std::abs(y));
        }
    };

    /// The x coordinate series used for marker x-selection:
    /// innermost independent (Dependent) or leaf index series (Independent).
    const auto marker_x_series = [](const xdataset::DataArray& da)
        -> xdataset::DataSeries
    {
        if (da.multi_dimension_spec().rank() == 0)
            throw std::runtime_error("mark: DataArray has no dimensions");
        if (da.data_kind() == xdataset::DataArrayKind::kDependent)
            return da.indep_data(1);   // 1 = innermost
        return da.self_index_series();
    };

    /// Marker row selection result.
    struct MarkRow
    {
        xdataset::Index flat_row;  // row of the selected leaf in the source
        xdataset::Index x_index;   // innermost-dimension index of the leaf

        MarkRow() : flat_row(0), x_index(0) {}
        MarkRow(xdataset::Index flat, xdataset::Index x)
            : flat_row(flat), x_index(x) {}
    };

    /// Build the flat 2-column (x, y) Dependent DataArray from selected rows.
    /// Column names follow the source variable names.
    Value build_marker_result(const xdataset::DataArray& da,
                              const std::vector<MarkRow>& rows)
    {
        const xdataset::DataSeries& data = da.data();
        const bool is_indep =
            (da.data_kind() == xdataset::DataArrayKind::kIndependent);
        const std::size_t n = rows.size();

        auto sample = [](xdataset::DataSeries& dst, xdataset::Index r,
                         const xdataset::DataSeries& src, xdataset::Index i)
        {
            switch (src.data_type())
            {
                case xdataset::DataType::kReal:
                    dst.scalar_at<double>(r) = src.scalar_at<double>(i); break;
                case xdataset::DataType::kInteger:
                    dst.scalar_at<int>(r) = src.scalar_at<int>(i); break;
                case xdataset::DataType::kComplex:
                    dst.scalar_at<std::complex<double>>(r) =
                        src.scalar_at<std::complex<double>>(i); break;
                default:
                    dst.scalar_at<std::string>(r) =
                        src.scalar_at<std::string>(i); break;
            }
        };

        // x column (innermost index), y column (source data row).
        xdataset::DataSeries x_src = marker_x_series(da);
        xdataset::DataSeries x_out(x_src.data_type(), x_src.data_shape());
        x_out.set_unit(x_src.unit());
        x_out.resize(n);
        xdataset::DataSeries y_out(data.data_type(), data.data_shape());
        y_out.set_unit(data.unit());
        y_out.resize(n);
        for (std::size_t r = 0; r < n; ++r)
        {
            sample(x_out, static_cast<xdataset::Index>(r),
                   x_src, rows[r].x_index);
            const xdataset::Index y_idx =
                is_indep ? rows[r].x_index : rows[r].flat_row;
            sample(y_out, static_cast<xdataset::Index>(r), data, y_idx);
        }

        std::string x_name;
        if (!is_indep)
        {
            const std::vector<std::string>& names = da.indep_names();
            if (!names.empty())
                x_name = names.back();   // innermost independent
        }
        if (x_name.empty())
            x_name = "x";
        std::string y_name = da.source_name();
        if (y_name.empty())
            y_name = "data";

        xdataset::DataArrayCreateInfo info;
        info.kind = xdataset::DataArrayKind::kDependent;
        info.multi_dimension_spec =
            xdataset::MultiDimensionSpec().add_regular(n);
        info.datas.emplace(x_name, std::move(x_out));
        info.datas[xdataset::DataArray::kSelf] = std::move(y_out);
        return Value(std::make_shared<xdataset::DataArray>(std::move(info)));
    }

    /// Closest leaf in [flat_start, flat_end) of da to target `t`,
    /// scored by |x - t| when by_x else |data - t|.
    const auto closest_leaf = [](const xdataset::DataArray& da,
                                 const xdataset::DataSeries& x_series,
                                 const xdataset::DataSeries& data,
                                 bool by_x, bool is_indep,
                                 xdataset::Index rank,
                                 xdataset::Index start, xdataset::Index end,
                                 double t) -> MarkRow
    {
        MarkRow best(start, 0);
        double best_d = std::numeric_limits<double>::infinity();
        da.for_each_leaf_row(
            [&](const xdataset::MultiDimensionSpec::LeafRow& leaf)
            {
                const xdataset::Index x_idx = leaf.dimension_row_indices[
                    static_cast<std::size_t>(rank) - 1];
                const double d = by_x
                    ? std::abs(scalar_series_at(x_series, x_idx) - t)
                    : data_value_distance(data,
                          is_indep ? x_idx : leaf.row_flat, t);
                if (d < best_d)
                {
                    best_d = d;
                    best = MarkRow(leaf.row_flat, x_idx);
                }
            },
            start, end);
        return best;
    };

}  // namespace

Value Mark(const Value& da_val, const Value& x_val, const Value& y_val)
{
    // A Measurement is lazily promoted to a 1-row Independent DataArray.
    const xdataset::DataArray& da = da_val.as_data_array_view();
    const std::size_t rank = da.multi_dimension_spec().rank();
    if (rank == 0)
        throw std::runtime_error("mark: DataArray has no dimensions");

    const xdataset::Measurement& xm = x_val.as_measurement();
    const xdataset::Measurement& ym = y_val.as_measurement();
    const double target_x = measurement_to_double(xm, "mark: x");
    const double target_y = measurement_to_double(ym, "mark: y");

    // X axis: real scalar series (innermost indep, or index series for
    // Independent).  Compare in canonical (base SI) units so a kHz-vs-GHz
    // request still matches a Hz-stored axis.
    xdataset::DataSeries x_series = marker_x_series(da).canonicalized();
    if (!is_real_scalar_series(x_series))
        throw std::runtime_error("mark: innermost independent axis must be scalar Real or Integer");

    const xdataset::DataSeries& data_series = da.data();
    if (data_series.data_kind() != xdataset::DataKind::kScalar)
        throw std::runtime_error("mark: data must be scalar");
    if (data_series.data_type() != xdataset::DataType::kReal &&
        data_series.data_type() != xdataset::DataType::kInteger &&
        data_series.data_type() != xdataset::DataType::kComplex)
        throw std::runtime_error("mark: data must be Real, Integer, or Complex");

    // Normalize each axis by its observed range for a unit-agnostic 2-D
    // Euclidean distance.
    double x_lo = 0.0, x_hi = 0.0;
    if (x_series.size() > 0)
    {
        x_lo = x_hi = scalar_series_at(x_series, 0);
        for (std::size_t i = 1; i < x_series.size(); ++i)
        {
            const double v = scalar_series_at(x_series,
                static_cast<xdataset::Index>(i));
            if (v < x_lo) x_lo = v;
            if (v > x_hi) x_hi = v;
        }
    }
    const double x_span = (x_hi - x_lo > 0.0) ? (x_hi - x_lo) : 1.0;

    double y_lo = 0.0, y_hi = 0.0;
    if (data_series.size() > 0)
    {
        y_lo = y_hi = data_magnitude(data_series, 0);
        for (std::size_t i = 1; i < data_series.size(); ++i)
        {
            const double v = data_magnitude(data_series,
                static_cast<xdataset::Index>(i));
            if (v < y_lo) y_lo = v;
            if (v > y_hi) y_hi = v;
        }
    }
    const double y_span = (y_hi - y_lo > 0.0) ? (y_hi - y_lo) : 1.0;

    // Scan every leaf: 2-D normalized distance.
    xdataset::Index best_row = 0;
    xdataset::Index best_x_idx = 0;
    double best_dist = std::numeric_limits<double>::infinity();
    bool first = true;
    da.for_each_leaf_row(
        [&](const xdataset::MultiDimensionSpec::LeafRow& leaf)
        {
            const std::vector<xdataset::Index>& dim_ri =
                leaf.dimension_row_indices;
            const xdataset::Index x_idx =
                dim_ri[static_cast<std::size_t>(rank) - 1];
            const xdataset::Index y_idx =
                (da.data_kind() == xdataset::DataArrayKind::kIndependent)
                ? x_idx : leaf.row_flat;
            const double xi = scalar_series_at(x_series, x_idx);
            const double yi = data_value_distance(data_series, y_idx, target_y);
            const double dx = (xi - target_x) / x_span;
            const double dy = yi / y_span;
            const double d = std::sqrt(dx * dx + dy * dy);
            if (first || d < best_dist)
            {
                first = false;
                best_dist = d;
                best_row = leaf.row_flat;
                best_x_idx = x_idx;
            }
        });

    // Single-row output: a 1-row 2-column array.
    std::vector<MarkRow> rows;
    rows.push_back(MarkRow{ best_row, best_x_idx });
    return build_marker_result(da, rows);
}

Value XMark(const Value& da_val, const Value& x_val)
{
    // A Measurement is lazily promoted to a 1-row Independent DataArray.
    const xdataset::DataArray& da = da_val.as_data_array_view();
    const std::size_t rank = da.multi_dimension_spec().rank();
    if (rank == 0)
        throw std::runtime_error("x_mark: DataArray has no dimensions");

    const xdataset::Measurement& xm = x_val.as_measurement();
    const double target_x = measurement_to_double(xm, "x_mark: x");

    xdataset::DataSeries x_series = marker_x_series(da).canonicalized();
    if (!is_real_scalar_series(x_series))
        throw std::runtime_error("x_mark: innermost independent axis must be scalar Real or Integer");

    const xdataset::DataSeries& data_series = da.data();
    if (data_series.data_kind() != xdataset::DataKind::kScalar)
        throw std::runtime_error("x_mark: data must be scalar");

    std::vector<MarkRow> rows;
    if (rank >= 2)
    {
        // One selection per innermost slice: group at the SECOND-innermost
        // level (each group spans one full innermost sweep).
        da.for_each_indep_group(2,
            [&](const xdataset::MultiDimensionSpec::DimGroup& g)
            {
                rows.push_back(closest_leaf(
                    da, x_series, data_series, true, false,
                    static_cast<xdataset::Index>(rank),
                    g.flat_start, g.flat_end, target_x));
            });
    }
    else
    {
        const xdataset::Index n = static_cast<xdataset::Index>(data_series.size());
        rows.push_back(closest_leaf(
            da, x_series, data_series, true, false,
            static_cast<xdataset::Index>(rank), 0, n, target_x));
    }

    return build_marker_result(da, rows);
}

Value YMark(const Value& da_val, const Value& y_val)
{
    // A Measurement is lazily promoted to a 1-row Independent DataArray.
    const xdataset::DataArray& da = da_val.as_data_array_view();
    const std::size_t rank = da.multi_dimension_spec().rank();
    if (rank == 0)
        throw std::runtime_error("y_mark: DataArray has no dimensions");

    const xdataset::Measurement& ym = y_val.as_measurement();
    const double target_y = measurement_to_double(ym, "y_mark: y");

    xdataset::DataSeries x_series = marker_x_series(da).canonicalized();
    if (!is_real_scalar_series(x_series))
        throw std::runtime_error("y_mark: innermost independent axis must be scalar Real or Integer");

    const xdataset::DataSeries& data_series = da.data();
    if (data_series.data_kind() != xdataset::DataKind::kScalar)
        throw std::runtime_error("y_mark: data must be scalar");
    if (data_series.data_type() != xdataset::DataType::kReal &&
        data_series.data_type() != xdataset::DataType::kInteger &&
        data_series.data_type() != xdataset::DataType::kComplex)
        throw std::runtime_error("y_mark: data must be Real, Integer, or Complex");

    const bool is_indep =
        (da.data_kind() == xdataset::DataArrayKind::kIndependent);

    std::vector<MarkRow> rows;
    if (rank >= 2)
    {
        da.for_each_indep_group(2,
            [&](const xdataset::MultiDimensionSpec::DimGroup& g)
            {
                rows.push_back(closest_leaf(
                    da, x_series, data_series, false, is_indep,
                    static_cast<xdataset::Index>(rank),
                    g.flat_start, g.flat_end, target_y));
            });
    }
    else
    {
        const xdataset::Index n = static_cast<xdataset::Index>(data_series.size());
        rows.push_back(closest_leaf(
            da, x_series, data_series, false, is_indep,
            static_cast<xdataset::Index>(rank), 0, n, target_y));
    }

    return build_marker_result(da, rows);
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

    lib.Add(Function("mark",
        std::vector<FunctionParam>{
            Param("da"),
            Param("x"),
            Param("y"),
        },
        [](const Function::ArgMap& args) {
            return Mark(args.at("da"), args.at("x"), args.at("y"));
        }));

    lib.Add(Function("x_mark",
        std::vector<FunctionParam>{
            Param("da"),
            Param("x"),
        },
        [](const Function::ArgMap& args) {
            return XMark(args.at("da"), args.at("x"));
        }));

    lib.Add(Function("y_mark",
        std::vector<FunctionParam>{
            Param("da"),
            Param("y"),
        },
        [](const Function::ArgMap& args) {
            return YMark(args.at("da"), args.at("y"));
        }));

    lib.Add(Function("output",
        std::vector<FunctionParam>{
            Param("da"),
            Param("variable_name", Value::String("data")),
        },
        [](const Function::ArgMap& args) {
            return Output(args.at("da"), args.at("variable_name"));
        }));

    lib.Add(Function("sweep_size", std::vector<FunctionParam>{ Param("da") },
        [](const Function::ArgMap& args) {
            return SweepSize(args.at("da"));
        }));

    lib.Add(Function("sweep_dim", std::vector<FunctionParam>{ Param("da") },
        [](const Function::ArgMap& args) {
            return SweepDim(args.at("da"));
        }));

    // vertcat(a, b) / horzcat(a, b) -- the {} generator operations exposed as
    // callable functions (MATLAB [A; B] / [A B]).  Registered here so Python
    // plugins can call rel.vertcat / rel.horzcat directly on rel.Value without
    // a numpy round-trip (units stay intact).
    lib.Add(Function("vertcat",
        std::vector<FunctionParam>{ Param("a"), Param("b") },
        [](const Function::ArgMap& args) {
            std::vector<Value> items;
            items.push_back(args.at("a"));
            items.push_back(args.at("b"));
            return rel::operation::OperationVertcat(items);
        }));

    lib.Add(Function("horzcat",
        std::vector<FunctionParam>{ Param("a"), Param("b") },
        [](const Function::ArgMap& args) {
            std::vector<Value> items;
            items.push_back(args.at("a"));
            items.push_back(args.at("b"));
            return rel::operation::OperationHorzcat(items);
        }));

    return lib;
}

}  // namespace builtin
}  // namespace rel
