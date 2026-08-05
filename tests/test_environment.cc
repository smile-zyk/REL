// Environment tests powered by GoogleTest.

#include "eval/environment.h"
#include "rel.h"

#include "data_series.h"
#include "dataset.h"
#include "measurement.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    // ---- helpers ------------------------------------------------------------

    xdataset::BlockCreateInfo make_block_info()
    {
        xdataset::BlockCreateInfo info;
        // freq: independent, 2 points
        info.independent_specs.push_back(
            xdataset::IndependentSpec{
                "freq",
                xdataset::DataSeries::CreateScalar<double>(2),
                xdataset::DimensionSpec::Regular(2)});
        // Vout: dependent, scalar
        info.dependent_specs.push_back(
            xdataset::DependentSpec{
                "Vout",
                xdataset::DataSeries::CreateScalar<double>(2)});
        return info;
    }

    xdataset::BlockCreateInfo make_block_info_alt()
    {
        xdataset::BlockCreateInfo info;
        info.independent_specs.push_back(
            xdataset::IndependentSpec{
                "power",
                xdataset::DataSeries::CreateScalar<double>(3),
                xdataset::DimensionSpec::Regular(3)});
        info.dependent_specs.push_back(
            xdataset::DependentSpec{
                "gain",
                xdataset::DataSeries::CreateScalar<double>(3)});
        return info;
    }

    /// Build a Dataset with the tree:
    ///   noise (Dataset)
    ///     simulation/SP1/SP (Block: freq, Vout)
    ///     simulation/HB   (Block: power, gain)
    xdataset::Dataset make_dataset()
    {
        xdataset::Dataset ds("noise");
        ds.AddBlock("simulation/SP1/SP", make_block_info());
        ds.AddBlock("simulation/HB",   make_block_info_alt());
        return ds;
    }

    rel::RefSegment S(const std::string& name, rel::RefSeparator sep = rel::RefSeparator::None)
    {
        return rel::RefSegment(name, sep);
    }

    rel::RefSegment Dot(const std::string& name)
    {
        return rel::RefSegment(name, rel::RefSeparator::Dot);
    }
} // namespace

// =========================================================================
//  Variables: define / get
// =========================================================================

TEST(EnvironmentTest, DefineAndGet)
{
    rel::Environment env;
    env.Define("x", rel::Value::Real(3.14));
    env.Define("y", rel::Value::Integer(42));

    EXPECT_DOUBLE_EQ(env.Get("x").as_measurement().as_scalar<double>(), 3.14);
    EXPECT_EQ(env.Get("y").as_measurement().as_scalar<int>(), 42);
}

TEST(EnvironmentTest, GetUndefinedReturnsDefault)
{
    rel::Environment env;
    EXPECT_TRUE(env.Get("nonexistent").is_measurement());
}

TEST(EnvironmentTest, RedefineOverwrites)
{
    rel::Environment env;
    env.Define("x", rel::Value::Integer(1));
    env.Define("x", rel::Value::Integer(2));
    EXPECT_EQ(env.Get("x").as_measurement().as_scalar<int>(), 2);
}

// =========================================================================
//  Built-in constants
// =========================================================================

TEST(EnvironmentTest, BuiltinPi)
{
    rel::Environment::InitBuiltinConstants();
    const rel::Value* v = rel::Environment::FindConstant("PI");
    ASSERT_NE(v, nullptr);
    EXPECT_TRUE(v->is_measurement());
    EXPECT_DOUBLE_EQ(v->as_measurement().as_scalar<double>(), 3.1415926535898);
}

TEST(EnvironmentTest, BuiltinLowercasePi)
{
    rel::Environment::InitBuiltinConstants();
    const rel::Value* v = rel::Environment::FindConstant("pi");
    ASSERT_NE(v, nullptr);
    EXPECT_DOUBLE_EQ(v->as_measurement().as_scalar<double>(), 3.1415926535898);
}

TEST(EnvironmentTest, BuiltinE)
{
    rel::Environment::InitBuiltinConstants();
    const rel::Value* v = rel::Environment::FindConstant("e");
    ASSERT_NE(v, nullptr);
    EXPECT_DOUBLE_EQ(v->as_measurement().as_scalar<double>(), 2.718281822);
}

TEST(EnvironmentTest, BuiltinBoltzmann)
{
    rel::Environment::InitBuiltinConstants();
    const rel::Value* v = rel::Environment::FindConstant("boltzmann");
    ASSERT_NE(v, nullptr);
    EXPECT_TRUE(v->is_measurement());
    EXPECT_DOUBLE_EQ(v->as_measurement().as_scalar<double>(), 1.380658e-23);
}

TEST(EnvironmentTest, BuiltinTinyReal)
{
    rel::Environment::InitBuiltinConstants();
    const rel::Value* v = rel::Environment::FindConstant("tinyReal");
    ASSERT_NE(v, nullptr);
    EXPECT_DOUBLE_EQ(v->as_measurement().as_scalar<double>(), 2.2e-308);
}

// =========================================================================
//  Dataset management
// =========================================================================

TEST(EnvironmentTest, AddDatasetAndSetDefault)
{
    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());

    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    ASSERT_NE(rel::Environment::DefaultDataset(), nullptr);
    EXPECT_EQ(rel::Environment::DefaultDataset()->name(), "noise");
}

TEST(EnvironmentTest, DefaultDatasetNullWhenNotSet)
{
    EXPECT_EQ(rel::Environment::DefaultDataset(), nullptr);
}

// =========================================================================
//  Reference resolution: single segment (variables)
// =========================================================================

TEST(EnvironmentTest, ResolveSingleSegmentVariable)
{
    rel::Environment env;
    env.Define("x", rel::Value::Real(1.5));

    rel::Value v = env.ResolveReference({S("x")});
    EXPECT_TRUE(v.is_measurement());
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 1.5);
}

TEST(EnvironmentTest, ResolveSingleSegmentBuiltin)
{
    rel::Environment env;
    rel::Environment::InitBuiltinConstants();
    rel::Value v = env.ResolveReference({S("PI")});
    EXPECT_TRUE(v.is_measurement());
}

TEST(EnvironmentTest, ResolveSingleSegmentUnique)
{
    rel::Environment env;
    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    // Both "freq" and "Vout" are unique in this dataset.
    rel::Value v = env.ResolveReference({S("Vout")});
    EXPECT_TRUE(v.is_data_array());
}

TEST(EnvironmentTest, ResolveSingleSegmentUndefinedThrows)
{
    rel::Environment env;
    EXPECT_THROW(env.ResolveReference({S("no_such_var")}),
                 std::runtime_error);
}

// =========================================================================
//  Reference resolution: DDot (cross-dataset)
// =========================================================================

TEST(EnvironmentTest, ResolveDDot)
{
    rel::Environment env;
    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP1/SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));

    std::vector<rel::RefSegment> segs;
    segs.push_back(S("noise"));
    segs.push_back(S("Vout", rel::RefSeparator::DDot));

    rel::Value v = env.ResolveReference(segs);
    EXPECT_TRUE(v.is_data_array());
}

TEST(EnvironmentTest, ResolveDDotUnknownDatasetThrows)
{
    rel::Environment env;
    std::vector<rel::RefSegment> segs;
    segs.push_back(S("nope"));
    segs.push_back(S("x", rel::RefSeparator::DDot));

    EXPECT_THROW(env.ResolveReference(segs), std::runtime_error);
}

// =========================================================================
//  Reference resolution: Dot paths
// =========================================================================

TEST(EnvironmentTest, ResolveDotPathDefaultDataset)
{
    rel::Environment env;
    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("simulation/SP1/SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    // simulation.SP1.SP.Vout
    std::vector<rel::RefSegment> segs;
    segs.push_back(Dot("simulation"));
    segs.push_back(Dot("SP1"));
    segs.push_back(Dot("SP"));
    segs.push_back(S("Vout"));

    rel::Value v = env.ResolveReference(segs);
    EXPECT_TRUE(v.is_data_array());
}

TEST(EnvironmentTest, ResolveDotPathExplicitDataset)
{
    rel::Environment env;
    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("simulation/SP1/SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));

    // noise.simulation.SP1.SP.Vout
    std::vector<rel::RefSegment> segs;
    segs.push_back(S("noise"));
    segs.push_back(Dot("simulation"));
    segs.push_back(Dot("SP1"));
    segs.push_back(Dot("SP"));
    segs.push_back(S("Vout"));

    rel::Value v = env.ResolveReference(segs);
    EXPECT_TRUE(v.is_data_array());
}

TEST(EnvironmentTest, ResolveDotPathMinimalBlockVar)
{
    rel::Environment env;
    std::unique_ptr<xdataset::Dataset> ds(new xdataset::Dataset("noise"));
    ds->AddBlock("SP", make_block_info());
    rel::Environment::AddDataset(std::move(ds));
    rel::Environment::SetDefaultDataset("noise");

    // SP.Vout  (block + variable, no group)
    std::vector<rel::RefSegment> segs;
    segs.push_back(Dot("SP"));
    segs.push_back(S("Vout"));

    rel::Value v = env.ResolveReference(segs);
    EXPECT_TRUE(v.is_data_array());
}

TEST(EnvironmentTest, ResolveDotPathWithoutDefaultDatasetThrows)
{
    rel::Environment env;

    // SP.Vout -- no default dataset set
    std::vector<rel::RefSegment> segs;
    segs.push_back(Dot("SP"));
    segs.push_back(S("Vout"));

    EXPECT_THROW(env.ResolveReference(segs), std::runtime_error);
}
