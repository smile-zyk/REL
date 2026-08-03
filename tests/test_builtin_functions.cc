// Builtin function tests: runtime introspection (print_datasets,
// print_dataset, print_variables).

#include "rel.h"
#include "eval/environment.h"

#include "data_array.h"
#include "data_series.h"
#include "dataset.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace
{
    xdataset::BlockCreateInfo make_block_info()
    {
        xdataset::BlockCreateInfo info;
        info.independent_specs.push_back(
            xdataset::IndependentSpec{
                "freq",
                xdataset::DataSeries::CreateScalar<double>(2),
                xdataset::DimensionSpec::Regular(2)});
        info.dependent_specs.push_back(
            xdataset::DependentSpec{
                "Vout",
                xdataset::DataSeries::CreateScalar<double>(2)});
        return info;
    }

    /// Read the string rows out of a print builtin's result.
    std::vector<std::string> payload(const rel::Value& v)
    {
        EXPECT_TRUE(v.is_data_array());
        const xdataset::DataArray& da = v.as_data_array();
        EXPECT_EQ(da.data_kind(), xdataset::DataArrayKind::kIndependent);
        EXPECT_EQ(da.data().data_type(), xdataset::DataType::kString);

        std::vector<std::string> rows;
        rows.reserve(static_cast<std::size_t>(da.data().size()));
        for (std::size_t i = 0; i < static_cast<std::size_t>(da.data().size()); ++i)
            rows.push_back(da.data().scalar_at<std::string>(static_cast<xdataset::Index>(i)));
        return rows;
    }
} // namespace

// =========================================================================
//  print_datasets
// =========================================================================

TEST(BuiltinFunctionTest, PrintDatasetsEmpty)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    rel::Value v = rel::Eval("print_datasets()", &env);
    EXPECT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data_kind(), xdataset::DataArrayKind::kIndependent);
    EXPECT_EQ(v.as_data_array().data().data_type(), xdataset::DataType::kString);

    std::vector<std::string> rows = payload(v);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], "datasets (0):");
}

TEST(BuiltinFunctionTest, PrintDatasetsWithEntries)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    env.AddDataset(std::move(ds));

    std::vector<std::string> rows = payload(rel::Eval("print_datasets()", &env));
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0], "datasets (1):");
    EXPECT_EQ(rows[1], "  noise");
}

// =========================================================================
//  print_dataset
// =========================================================================

TEST(BuiltinFunctionTest, PrintDatasetNone)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::vector<std::string> rows = payload(rel::Eval("print_dataset()", &env));
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], "current dataset: (none)");
}

TEST(BuiltinFunctionTest, PrintDatasetDefault)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    env.AddDataset(std::move(ds));
    env.SetDefaultDataset("noise");

    std::vector<std::string> rows = payload(rel::Eval("print_dataset()", &env));
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], "current dataset: noise");
}

// =========================================================================
//  print_variables
// =========================================================================

TEST(BuiltinFunctionTest, PrintVariables)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);
    env.Define("a", rel::Value::Integer(1));
    env.Define("b", rel::Value::Real(2.0));

    std::vector<std::string> rows = payload(rel::Eval("print_variables()", &env));
    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(rows[0], "variables (2):");

    // variables_ is an unordered map: "a"/"b" order is unspecified.
    bool has_a = false, has_b = false;
    for (std::size_t i = 1; i < rows.size(); ++i)
    {
        if (rows[i] == "  a") has_a = true;
        if (rows[i] == "  b") has_b = true;
    }
    EXPECT_TRUE(has_a);
    EXPECT_TRUE(has_b);
}

// =========================================================================
//  Coexistence with user-registered functions and expressions
// =========================================================================

TEST(BuiltinFunctionTest, BuiltinsComposeInExpressions)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);
    rel::InitBuiltinConstants(env);

    // print_datasets() returns a DataArray; just confirm it can be stored.
    rel::Value v = rel::Eval("print_datasets()", &env);
    env.Define("info", v);
    EXPECT_EQ(env.Get("info").as_data_array().data().size(), 1u);
}
