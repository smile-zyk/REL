// Sample REL function plugin.
//
// Demonstrates the plugin ABI: it exports `rel_plugin_main`, builds a
// FunctionLibrary of two functions, and registers them through the host
// callback.  Plugins only need the header-only function.h (no rel_core
// link) plus the xdataset library for rel::Value.

#include "eval/rel_plugin.h"
#include "eval/function.h"

#include <vector>

namespace
{
    /// Read a scalar as double regardless of Integer/Real storage.
    double as_double(const rel::Value& v)
    {
        const xdataset::Measurement& m = v.as_measurement();
        if (m.data_type() == xdataset::DataType::kInteger)
            return static_cast<double>(m.as_scalar<int>());
        return m.as_scalar<double>();
    }

    /// sqr(x) — x * x
    rel::Function make_sqr()
    {
        return rel::Function(
            "sqr",
            std::vector<rel::FunctionParam>{ rel::Param("x") },
            [](const std::vector<rel::Value>& a) -> rel::Value {
                double x = as_double(a[0]);
                return rel::Value::Real(x * x);
            });
    }

    /// add3(a, b, c = 10) — a + b + c
    rel::Function make_add3()
    {
        return rel::Function(
            "add3",
            std::vector<rel::FunctionParam>{
                rel::Param("a"),
                rel::Param("b"),
                rel::Param("c", rel::Value::Integer(10)),
            },
            [](const std::vector<rel::Value>& a) -> rel::Value {
                int sum = 0;
                for (std::size_t i = 0; i < a.size(); ++i)
                    sum += a[i].as_measurement().as_scalar<int>();
                return rel::Value::Integer(sum);
            });
    }
} // namespace

extern "C" REL_PLUGIN_API int rel_plugin_main(const RelPluginApi* api,
                                              void* host_context)
{
    if (!api || api->api_version != REL_PLUGIN_API_VERSION)
        return 1;

    rel::FunctionLibrary lib("sample");
    lib.Add(make_sqr());
    lib.Add(make_add3());

    api->register_library(host_context, &lib);

    return 0;
}
