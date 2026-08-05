// Function plugin tests: load a plugin DLL, call its registered functions.

#include "rel.h"
#include "eval/environment.h"

#include "measurement.h"

#include <gtest/gtest.h>

#include <string>

#ifndef REL_SAMPLE_PLUGIN_PATH
#define REL_SAMPLE_PLUGIN_PATH ""
#endif

// =========================================================================
//  Loading
// =========================================================================

TEST(PluginTest, LoadRegistersFunctions)
{
    rel::Environment env;
    rel::Environment::InitBuiltinConstants();

    rel::LoadedPlugin* plugin = rel::Environment::LoadFunctionPlugin( REL_SAMPLE_PLUGIN_PATH);
    ASSERT_NE(plugin, nullptr);

    // Both plugin functions are registered.
    ASSERT_NE(rel::Environment::FindFunction("sqr"), nullptr);
    ASSERT_NE(rel::Environment::FindFunction("add3"), nullptr);

    rel::Environment::UnloadFunctionPlugin(plugin);
}

TEST(PluginTest, LoadNonexistentFails)
{
    rel::Environment env;
    EXPECT_EQ(rel::Environment::LoadFunctionPlugin( "no_such_plugin.dll"), nullptr);
}

// =========================================================================
//  Calling plugin functions
// =========================================================================

TEST(PluginTest, CallPluginFunction)
{
    rel::Environment env;
    rel::Environment::InitBuiltinConstants();

    rel::LoadedPlugin* plugin = rel::Environment::LoadFunctionPlugin( REL_SAMPLE_PLUGIN_PATH);
    ASSERT_NE(plugin, nullptr);

    rel::Value v = rel::Eval("sqr(4)", &env);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 16.0);

    rel::Environment::UnloadFunctionPlugin(plugin);
}

TEST(PluginTest, CallPluginFunctionWithDefaults)
{
    rel::Environment env;
    rel::Environment::InitBuiltinConstants();

    rel::LoadedPlugin* plugin = rel::Environment::LoadFunctionPlugin( REL_SAMPLE_PLUGIN_PATH);
    ASSERT_NE(plugin, nullptr);

    // add3(a, b, c = 10): omitted slot filled with the plugin's default.
    rel::Value v = rel::Eval("add3(1, 2)", &env);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 13);

    rel::Value v2 = rel::Eval("add3(1, 2, 3)", &env);
    EXPECT_EQ(v2.as_measurement().as_scalar<int>(), 6);

    rel::Environment::UnloadFunctionPlugin(plugin);
}

// =========================================================================
//  Unregister
// =========================================================================

TEST(PluginTest, UnregisterFunction)
{
    rel::Environment env;
    rel::Environment::InitBuiltinFunctions();

    ASSERT_NE(rel::Environment::FindFunction("datasets"), nullptr);
    EXPECT_TRUE(rel::Environment::UnregisterFunction("datasets"));
    EXPECT_EQ(rel::Environment::FindFunction("datasets"), nullptr);

    // Unregistering a nonexistent function returns false.
    EXPECT_FALSE(rel::Environment::UnregisterFunction("datasets"));
    EXPECT_FALSE(rel::Environment::UnregisterFunction("no_such_function"));
}
