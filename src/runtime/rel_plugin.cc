// Host-side loader for REL function plugins.
//
// LoadFunctionPlugin opens a plugin shared library (DLL / .so / .dylib),
// resolves its `rel_plugin_main` entry point, and calls it so the plugin can
// register its functions in the global function registry.
//
// UnloadFunctionPlugin first unregisters every function the plugin registered
// (so the global registry no longer references code inside the plugin library),
// then releases the library.

#include "rel_plugin.h"

#include "environment.h"
#include "function.h"

#include <string>
#include <vector>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace rel
{
    namespace
    {
        /// Host-side state passed to a plugin's rel_plugin_main as
        /// host_context: tracks the names of the functions the plugin
        /// registers (for later unregistration).
        struct PluginContext
        {
            std::vector<std::string> function_names;
        };

        /// Host-side implementation of RelRegisterLibraryFn.
        /// Registers the entire library into the global static function registry.
        void host_register_library(void* host_context, const void* library)
        {
            PluginContext* ctx = static_cast<PluginContext*>(host_context);
            const FunctionLibrary* lib = static_cast<const FunctionLibrary*>(library);
            Environment::RegisterLibrary(*lib);
            for (const auto& fn : lib->functions())
                ctx->function_names.push_back(fn.name());
        }

        const RelPluginApi kPluginApi = {
            REL_PLUGIN_API_VERSION,
            &host_register_library,
        };
    } // namespace

    struct LoadedPlugin
    {
        std::vector<std::string> function_names;
#ifdef _WIN32
        HMODULE handle;
#else
        void* handle;
#endif
    };

    LoadedPlugin* Environment::LoadFunctionPlugin(const std::string& path)
    {
#ifdef _WIN32
        HMODULE handle = LoadLibraryA(path.c_str());
        if (!handle)
            return nullptr;

        RelPluginMainFn main_fn =
            reinterpret_cast<RelPluginMainFn>(GetProcAddress(handle, "rel_plugin_main"));
        if (!main_fn)
        {
            FreeLibrary(handle);
            return nullptr;
        }

        PluginContext ctx;
        if (main_fn(&kPluginApi, &ctx) != 0)
        {
            FreeLibrary(handle);
            return nullptr;
        }

        LoadedPlugin* plugin = new LoadedPlugin;
        plugin->function_names = std::move(ctx.function_names);
        plugin->handle = handle;
        return plugin;
#else
        void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle)
            return nullptr;

        RelPluginMainFn main_fn =
            reinterpret_cast<RelPluginMainFn>(dlsym(handle, "rel_plugin_main"));
        if (!main_fn)
        {
            dlclose(handle);
            return nullptr;
        }

        PluginContext ctx;
        if (main_fn(&kPluginApi, &ctx) != 0)
        {
            dlclose(handle);
            return nullptr;
        }

        LoadedPlugin* plugin = new LoadedPlugin;
        plugin->function_names = std::move(ctx.function_names);
        plugin->handle = handle;
        return plugin;
#endif
    }

    void Environment::UnloadFunctionPlugin(LoadedPlugin* plugin)
    {
        if (!plugin)
            return;

        // Unregister the functions this plugin registered so the global
        // registry no longer references code inside the plugin library
        // (their std::function closures live in the plugin DLL).
        for (const auto& name : plugin->function_names)
            Environment::UnregisterFunction(name);

#ifdef _WIN32
        FreeLibrary(plugin->handle);
#else
        dlclose(plugin->handle);
#endif
        delete plugin;
    }

} // namespace rel
