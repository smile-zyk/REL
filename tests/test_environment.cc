// Environment tests powered by GoogleTest.

#include "eval/environment.h"

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
    env.define("x", rel::Value::real(3.14));
    env.define("y", rel::Value::integer(42));

    EXPECT_DOUBLE_EQ(env.get("x").as_measurement().as_scalar<double>(), 3.14);
    EXPECT_EQ(env.get("y").as_measurement().as_scalar<int>(), 42);
}

TEST(EnvironmentTest, GetUndefinedReturnsNull)
{
    rel::Environment env;
    EXPECT_TRUE(env.get("nonexistent").is_null());
}

TEST(EnvironmentTest, RedefineOverwrites)
{
    rel::Environment env;
    env.define("x", rel::Value::integer(1));
    env.define("x", rel::Value::integer(2));
    EXPECT_EQ(env.get("x").as_measurement().as_scalar<int>(), 2);
}

// =========================================================================
//  Built-in constants
// =========================================================================

TEST(EnvironmentTest, BuiltinPi)
{
    rel::Environment env;
    rel::init_builtin_constants(env);
    rel::Value v = env.get("PI");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 3.1415926535898);
}

TEST(EnvironmentTest, BuiltinLowercasePi)
{
    rel::Environment env;
    rel::init_builtin_constants(env);
    rel::Value v = env.get("pi");
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 3.1415926535898);
}

TEST(EnvironmentTest, BuiltinE)
{
    rel::Environment env;
    rel::init_builtin_constants(env);
    rel::Value v = env.get("e");
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 2.718281822);
}

TEST(EnvironmentTest, BuiltinBoltzmann)
{
    rel::Environment env;
    rel::init_builtin_constants(env);
    rel::Value v = env.get("boltzmann");
    EXPECT_TRUE(v.is_measurement());
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 1.380658e-23);
}

TEST(EnvironmentTest, BuiltinTinyReal)
{
    rel::Environment env;
    rel::init_builtin_constants(env);
    EXPECT_DOUBLE_EQ(env.get("tinyReal").as_measurement().as_scalar<double>(),
                     2.2e-308);
}

// =========================================================================
//  Dataset management
// =========================================================================

TEST(EnvironmentTest, AddDatasetAndSetDefault)
{
    rel::Environment env;
    xdataset::Dataset ds("noise");
    ds.AddBlock("SP1/SP", make_block_info());

    env.add_dataset(&ds);
    env.set_default_dataset("noise");

    ASSERT_NE(env.default_dataset(), nullptr);
    EXPECT_EQ(env.default_dataset()->name(), "noise");
}

TEST(EnvironmentTest, DefaultDatasetNullWhenNotSet)
{
    rel::Environment env;
    EXPECT_EQ(env.default_dataset(), nullptr);
}

// =========================================================================
//  Reference resolution: single segment (variables)
// =========================================================================

TEST(EnvironmentTest, ResolveSingleSegmentVariable)
{
    rel::Environment env;
    env.define("x", rel::Value::real(1.5));

    rel::Value v = env.resolve_reference({S("x")});
    EXPECT_TRUE(v.is_measurement());
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 1.5);
}

TEST(EnvironmentTest, ResolveSingleSegmentBuiltin)
{
    rel::Environment env;
    rel::init_builtin_constants(env);
    rel::Value v = env.resolve_reference({S("PI")});
    EXPECT_TRUE(v.is_measurement());
}

TEST(EnvironmentTest, ResolveSingleSegmentUnique)
{
    rel::Environment env;
    xdataset::Dataset ds("noise");
    ds.AddBlock("SP1/SP", make_block_info());
    env.add_dataset(&ds);
    env.set_default_dataset("noise");

    // Both "freq" and "Vout" are unique in this dataset.
    rel::Value v = env.resolve_reference({S("Vout")});
    EXPECT_TRUE(v.is_data_array());
}

TEST(EnvironmentTest, ResolveSingleSegmentUndefinedThrows)
{
    rel::Environment env;
    EXPECT_THROW(env.resolve_reference({S("no_such_var")}),
                 std::runtime_error);
}

// =========================================================================
//  Reference resolution: DDot (cross-dataset)
// =========================================================================

TEST(EnvironmentTest, ResolveDDot)
{
    rel::Environment env;
    xdataset::Dataset ds("noise");
    ds.AddBlock("SP1/SP", make_block_info());
    env.add_dataset(&ds);

    std::vector<rel::RefSegment> segs;
    segs.push_back(S("noise"));
    segs.push_back(S("Vout", rel::RefSeparator::DDot));

    rel::Value v = env.resolve_reference(segs);
    EXPECT_TRUE(v.is_data_array());
}

TEST(EnvironmentTest, ResolveDDotUnknownDatasetThrows)
{
    rel::Environment env;
    std::vector<rel::RefSegment> segs;
    segs.push_back(S("nope"));
    segs.push_back(S("x", rel::RefSeparator::DDot));

    EXPECT_THROW(env.resolve_reference(segs), std::runtime_error);
}

// =========================================================================
//  Reference resolution: Dot paths
// =========================================================================

TEST(EnvironmentTest, ResolveDotPathDefaultDataset)
{
    rel::Environment env;
    xdataset::Dataset ds("noise");
    ds.AddBlock("simulation/SP1/SP", make_block_info());
    env.add_dataset(&ds);
    env.set_default_dataset("noise");

    // simulation.SP1.SP.Vout
    std::vector<rel::RefSegment> segs;
    segs.push_back(Dot("simulation"));
    segs.push_back(Dot("SP1"));
    segs.push_back(Dot("SP"));
    segs.push_back(S("Vout"));

    rel::Value v = env.resolve_reference(segs);
    EXPECT_TRUE(v.is_data_array());
}

TEST(EnvironmentTest, ResolveDotPathExplicitDataset)
{
    rel::Environment env;
    xdataset::Dataset ds("noise");
    ds.AddBlock("simulation/SP1/SP", make_block_info());
    env.add_dataset(&ds);

    // noise.simulation.SP1.SP.Vout
    std::vector<rel::RefSegment> segs;
    segs.push_back(S("noise"));
    segs.push_back(Dot("simulation"));
    segs.push_back(Dot("SP1"));
    segs.push_back(Dot("SP"));
    segs.push_back(S("Vout"));

    rel::Value v = env.resolve_reference(segs);
    EXPECT_TRUE(v.is_data_array());
}

TEST(EnvironmentTest, ResolveDotPathMinimalBlockVar)
{
    rel::Environment env;
    xdataset::Dataset ds("noise");
    ds.AddBlock("SP", make_block_info());
    env.add_dataset(&ds);
    env.set_default_dataset("noise");

    // SP.Vout  (block + variable, no group)
    std::vector<rel::RefSegment> segs;
    segs.push_back(Dot("SP"));
    segs.push_back(S("Vout"));

    rel::Value v = env.resolve_reference(segs);
    EXPECT_TRUE(v.is_data_array());
}

TEST(EnvironmentTest, ResolveDotPathWithoutDefaultDatasetThrows)
{
    rel::Environment env;

    // SP.Vout — no default dataset set
    std::vector<rel::RefSegment> segs;
    segs.push_back(Dot("SP"));
    segs.push_back(S("Vout"));

    EXPECT_THROW(env.resolve_reference(segs), std::runtime_error);
}
