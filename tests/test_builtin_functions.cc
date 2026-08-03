// Builtin function tests: runtime introspection (print_datasets,
// print_dataset, print_variables).

#include "rel.h"
#include "eval/environment.h"

#include "data_array.h"
#include "data_series.h"
#include "dataset.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
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
    EXPECT_TRUE(rows.empty());  // no datasets -> no rows
}

TEST(BuiltinFunctionTest, PrintDatasetsWithEntries)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    env.AddDataset(std::move(ds));

    std::vector<std::string> rows = payload(rel::Eval("print_datasets()", &env));
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0], "noise");
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
    EXPECT_EQ(rows[0], "NO DEFAULT DATASET");
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
    EXPECT_EQ(rows[0], "noise");
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
    ASSERT_EQ(rows.size(), 2u);

    // variables_ is an unordered map: "a"/"b" order is unspecified.
    bool has_a = false, has_b = false;
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        if (rows[i] == "a") has_a = true;
        if (rows[i] == "b") has_b = true;
    }
    EXPECT_TRUE(has_a);
    EXPECT_TRUE(has_b);
}

// =========================================================================
//  what
// =========================================================================

TEST(BuiltinFunctionTest, WhatScalarInteger)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::vector<std::string> rows = payload(rel::Eval("what(3)", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[0], "Dependency: []");
    EXPECT_EQ(rows[1], "Kind: Independent");
    EXPECT_EQ(rows[2], "Dimension: []");
    EXPECT_EQ(rows[3], "Data Shape: Scalar");
    EXPECT_EQ(rows[4], "Data Type: Integer");
}

TEST(BuiltinFunctionTest, WhatScalarReal)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::vector<std::string> rows = payload(rel::Eval("what(3.5)", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[3], "Data Shape: Scalar");
    EXPECT_EQ(rows[4], "Data Type: Double");
}

TEST(BuiltinFunctionTest, WhatString)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::vector<std::string> rows = payload(rel::Eval("what(\"abc\")", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[4], "Data Type: String");
}

TEST(BuiltinFunctionTest, WhatBoolean)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::vector<std::string> rows = payload(rel::Eval("what(1 == 1)", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[4], "Data Type: Boolean");
}

TEST(BuiltinFunctionTest, WhatVector)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::vector<std::string> rows = payload(rel::Eval("what({1, 2, 3})", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[0], "Dependency: []");
    EXPECT_EQ(rows[1], "Kind: Independent");
    EXPECT_EQ(rows[3], "Data Shape: Vector(3)");
    EXPECT_EQ(rows[4], "Data Type: Integer");
}

TEST(BuiltinFunctionTest, WhatMatrix)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::vector<std::string> rows = payload(rel::Eval("what({{1, 2}, {3, 4}})", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[3], "Data Shape: Matrix(2, 2)");
    EXPECT_EQ(rows[4], "Data Type: Integer");
}

TEST(BuiltinFunctionTest, WhatSweepArray)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::vector<std::string> rows = payload(rel::Eval("what([1, 2, 3])", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[0], "Dependency: []");
    EXPECT_EQ(rows[1], "Kind: Independent");
    EXPECT_EQ(rows[2], "Dimension: [3]");
    EXPECT_EQ(rows[3], "Data Shape: Scalar");
    EXPECT_EQ(rows[4], "Data Type: Integer");
}

TEST(BuiltinFunctionTest, WhatDatasetVariable)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());  // freq(2) indep, Vout dep
    env.AddDataset(std::move(ds));
    env.SetDefaultDataset("noise");

    // Vout: dependent on freq; the parser resolves it via unique lookup.
    std::vector<std::string> rows = payload(rel::Eval("what(Vout)", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[0], "Dependency: [freq]");
    EXPECT_EQ(rows[1], "Kind: Dependent");
    EXPECT_EQ(rows[2], "Dimension: [2]");
    EXPECT_EQ(rows[3], "Data Shape: Scalar");
    EXPECT_EQ(rows[4], "Data Type: Double");
}

// =========================================================================
//  indep
// =========================================================================

TEST(BuiltinFunctionTest, IndepByIndex)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());  // freq indep (2), Vout dep
    env.AddDataset(std::move(ds));
    env.SetDefaultDataset("noise");

    // indep(Vout, 1) — extract the first independent variable by 1-based index.
    rel::Value v = rel::Eval("indep(Vout, 1)", &env);
    ASSERT_TRUE(v.is_data_array());
    const xdataset::DataArray& da = v.as_data_array();
    EXPECT_EQ(da.data_kind(), xdataset::DataArrayKind::kIndependent);
    EXPECT_EQ(da.data().size(), 2u);
    EXPECT_EQ(da.data().data_type(), xdataset::DataType::kReal);
}

TEST(BuiltinFunctionTest, IndepByName)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    env.AddDataset(std::move(ds));
    env.SetDefaultDataset("noise");

    // indep(Vout, "freq") — extract by independent variable name.
    rel::Value v = rel::Eval("indep(Vout, \"freq\")", &env);
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data_kind(), xdataset::DataArrayKind::kIndependent);
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
}

TEST(BuiltinFunctionTest, IndepDefaultSelector)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    env.AddDataset(std::move(ds));
    env.SetDefaultDataset("noise");

    // indep(Vout) — selector defaults to 1 (first independent variable).
    rel::Value v = rel::Eval("indep(Vout)", &env);
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data_kind(), xdataset::DataArrayKind::kIndependent);
    EXPECT_EQ(v.as_data_array().data().size(), 2u);
}

TEST(BuiltinFunctionTest, IndepRequiresDataArray)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    // First argument must be a DataArray.
    EXPECT_THROW(rel::Eval("indep(1, 1)", &env), std::runtime_error);
}

TEST(BuiltinFunctionTest, IndepRequiresIntOrString)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    env.AddDataset(std::move(ds));
    env.SetDefaultDataset("noise");

    // Second argument must be Integer or String — a Real is rejected.
    EXPECT_THROW(rel::Eval("indep(Vout, 1.5)", &env), std::runtime_error);
}

// =========================================================================
//  min / max
// =========================================================================

TEST(BuiltinFunctionTest, MinOfSweep)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    // [3, 1, 2] -> reduce innermost dimension -> min = 1
    rel::Value v = rel::Eval("min([3, 1, 2])", &env);
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 1u);
    EXPECT_EQ(v.as_data_array().data().scalar_at<int>(0), 1);
}

TEST(BuiltinFunctionTest, MaxOfSweep)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    // [3, 1, 2] -> max = 3
    rel::Value v = rel::Eval("max([3, 1, 2])", &env);
    ASSERT_TRUE(v.is_data_array());
    EXPECT_EQ(v.as_data_array().data().size(), 1u);
    EXPECT_EQ(v.as_data_array().data().scalar_at<int>(0), 3);
}

TEST(BuiltinFunctionTest, MinMaxOfRealSweep)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    rel::Value mn = rel::Eval("min([2.5, 0.5, 1.5])", &env);
    EXPECT_DOUBLE_EQ(mn.as_data_array().data().scalar_at<double>(0), 0.5);

    rel::Value mx = rel::Eval("max([2.5, 0.5, 1.5])", &env);
    EXPECT_DOUBLE_EQ(mx.as_data_array().data().scalar_at<double>(0), 2.5);
}

TEST(BuiltinFunctionTest, MinMaxRequireDataArray)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    // Scalar Measurement is not a DataArray.
    EXPECT_THROW(rel::Eval("min(1)", &env), std::runtime_error);
    EXPECT_THROW(rel::Eval("max(1)", &env), std::runtime_error);
    EXPECT_THROW(rel::Eval("min({1, 2})", &env), std::runtime_error);
}

TEST(BuiltinFunctionTest, MinMaxOfDatasetVariable)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    env.AddDataset(std::move(ds));
    env.SetDefaultDataset("noise");

    // Vout is a Dependent DataArray with 2 rows (freq sweep).
    rel::Value mn = rel::Eval("min(Vout)", &env);
    ASSERT_TRUE(mn.is_data_array());
    EXPECT_EQ(mn.as_data_array().data().size(), 1u);

    rel::Value mx = rel::Eval("max(Vout)", &env);
    ASSERT_TRUE(mx.is_data_array());
    EXPECT_EQ(mx.as_data_array().data().size(), 1u);
}

// =========================================================================
//  output
// =========================================================================

TEST(BuiltinFunctionTest, OutputWritesCsv)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    // Default variable_name "data" -> writes "data.csv" in the current dir,
    // returns the absolute path.
    rel::Value ret = rel::Eval("output([1, 2, 3])", &env);
    ASSERT_TRUE(ret.is_measurement());
    std::string path = ret.as_measurement().as_scalar<std::string>();
    EXPECT_NE(path.find("data.csv"), std::string::npos);
    EXPECT_TRUE(path.size() > 8 && (path[0] == '/' || path[1] == ':'));  // absolute

    // Verify the file exists at the returned path and has expected content.
    std::ifstream f(path.c_str());
    ASSERT_TRUE(f.good());
    std::stringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();
    f.close();

    // Column header named "data"; data rows present.
    EXPECT_NE(content.find("data"), std::string::npos);
    EXPECT_NE(content.find("1"), std::string::npos);
    EXPECT_NE(content.find("3"), std::string::npos);

    std::remove(path.c_str());
}

TEST(BuiltinFunctionTest, OutputCustomName)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    // Custom variable_name -> writes "<name>.csv", header uses the name.
    rel::Value ret = rel::Eval("output([1, 2, 3], \"result\")", &env);
    ASSERT_TRUE(ret.is_measurement());
    std::string path = ret.as_measurement().as_scalar<std::string>();
    EXPECT_NE(path.find("result.csv"), std::string::npos);

    std::ifstream f(path.c_str());
    ASSERT_TRUE(f.good());
    std::stringstream ss;
    ss << f.rdbuf();
    f.close();
    EXPECT_NE(ss.str().find("result"), std::string::npos);

    std::remove(path.c_str());
}

TEST(BuiltinFunctionTest, OutputDatasetVariable)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    env.AddDataset(std::move(ds));
    env.SetDefaultDataset("noise");

    rel::Value ret = rel::Eval("output(Vout, \"vout\")", &env);
    ASSERT_TRUE(ret.is_measurement());
    std::string path = ret.as_measurement().as_scalar<std::string>();
    EXPECT_NE(path.find("vout.csv"), std::string::npos);

    std::ifstream f(path.c_str());
    ASSERT_TRUE(f.good());
    std::stringstream ss;
    ss << f.rdbuf();
    f.close();

    EXPECT_NE(ss.str().find("freq"), std::string::npos);  // independent header
    std::remove(path.c_str());
}

TEST(BuiltinFunctionTest, OutputRequiresDataArray)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    // First argument must be a DataArray.
    EXPECT_THROW(rel::Eval("output(1)", &env), std::runtime_error);
}

TEST(BuiltinFunctionTest, OutputRequiresStringName)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    // Second argument must be a String variable name.
    EXPECT_THROW(rel::Eval("output([1], 1)", &env), std::runtime_error);
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
    EXPECT_TRUE(env.Get("info").is_data_array());

    // what() can inspect the stored array.
    std::vector<std::string> rows = payload(rel::Eval("what(info)", &env));
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[1], "Kind: Independent");
}
