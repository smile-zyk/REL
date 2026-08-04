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
    rel::InitBuiltinConstants(env);

    rel::LoadedPlugin* plugin = rel::LoadFunctionPlugin(env, REL_SAMPLE_PLUGIN_PATH);
    ASSERT_NE(plugin, nullptr);

    // Both plugin functions are registered.
    ASSERT_NE(env.FindFunction("sqr"), nullptr);
    ASSERT_NE(env.FindFunction("add3"), nullptr);

    rel::UnloadFunctionPlugin(plugin);
}

TEST(PluginTest, LoadNonexistentFails)
{
    rel::Environment env;
    EXPECT_EQ(rel::LoadFunctionPlugin(env, "no_such_plugin.dll"), nullptr);
}

// =========================================================================
//  Calling plugin functions
// =========================================================================

TEST(PluginTest, CallPluginFunction)
{
    rel::Environment env;
    rel::InitBuiltinConstants(env);

    rel::LoadedPlugin* plugin = rel::LoadFunctionPlugin(env, REL_SAMPLE_PLUGIN_PATH);
    ASSERT_NE(plugin, nullptr);

    rel::Value v = rel::Eval("sqr(4)", &env);
    EXPECT_TRUE(v.is_measurement());
    EXPECT_DOUBLE_EQ(v.as_measurement().as_scalar<double>(), 16.0);

    rel::UnloadFunctionPlugin(plugin);
}

TEST(PluginTest, CallPluginFunctionWithDefaults)
{
    rel::Environment env;
    rel::InitBuiltinConstants(env);

    rel::LoadedPlugin* plugin = rel::LoadFunctionPlugin(env, REL_SAMPLE_PLUGIN_PATH);
    ASSERT_NE(plugin, nullptr);

    // add3(a, b, c = 10): omitted slot filled with the plugin's default.
    rel::Value v = rel::Eval("add3(1, 2)", &env);
    EXPECT_EQ(v.as_measurement().as_scalar<int>(), 13);

    rel::Value v2 = rel::Eval("add3(1, 2, 3)", &env);
    EXPECT_EQ(v2.as_measurement().as_scalar<int>(), 6);

    rel::UnloadFunctionPlugin(plugin);
}

// =========================================================================
//  Unregister
// =========================================================================

TEST(PluginTest, UnregisterFunction)
{
    rel::Environment env;
    rel::InitBuiltinFunctions(env);

    ASSERT_NE(env.FindFunction("datasets"), nullptr);
    EXPECT_TRUE(env.UnregisterFunction("datasets"));
    EXPECT_EQ(env.FindFunction("datasets"), nullptr);

    // Unregistering a nonexistent function returns false.
    EXPECT_FALSE(env.UnregisterFunction("datasets"));
    EXPECT_FALSE(env.UnregisterFunction("no_such_function"));
}
