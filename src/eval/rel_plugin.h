#pragma once

// =========================================================================
//  rel_plugin.h — C ABI for REL function plugins
// =========================================================================
//
//  A REL plugin is a shared library (DLL / .so / .dylib) that exports a
//  single entry point `rel_plugin_main`.  The host loads the library, calls
//  the entry point with a RelPluginApi, and the plugin registers its
//  functions through the api->register_library callback.
//
//  A plugin should:
//    - include this header and "eval/function.h" (Function / FunctionParam /
//      FunctionLibrary are header-only, so no rel_core link is needed),
//    - link against the xdataset library (for xdataset::Value),
//    - export:  REL_PLUGIN_API int rel_plugin_main(const RelPluginApi*, void*)
//
//  Registered functions live inside the plugin library: the library must
//  stay loaded (see rel::LoadFunctionPlugin / rel::UnloadFunctionPlugin)
//  while those functions may still be called.

#ifdef _WIN32
    #define REL_PLUGIN_API __declspec(dllexport)
#else
    #define REL_PLUGIN_API __attribute__((visibility("default")))
#endif

/// Bump when the ABI changes (RelPluginApi layout / semantics).
#define REL_PLUGIN_API_VERSION 3

#ifdef __cplusplus
extern "C" {
#endif

/// Callback: register a whole library (a rel::FunctionLibrary).
/// `library` points to a rel::FunctionLibrary; the host copies the functions,
/// so the pointer only needs to stay valid for the duration of this call.
typedef void (*RelRegisterLibraryFn)(void* host_context, const void* library);

/// Services the host provides to a plugin.
typedef struct RelPluginApi
{
    int api_version;                    ///< Must equal REL_PLUGIN_API_VERSION.
    RelRegisterLibraryFn register_library;
} RelPluginApi;

/// Plugin entry point.  Returns 0 on success, non-zero on failure.
typedef int (*RelPluginMainFn)(const RelPluginApi* api, void* host_context);

#ifdef __cplusplus
}
#endif
