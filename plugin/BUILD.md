# REL Plugin System — Build Architecture

> **Status**: Draft  
> **Date**: 2026-08-10  
> **Companion to**: `PYTHON_API.md` (Python 接口设计)

---

## 1. 设计目标

1. **`rel_runtime.dll` 默认零外部依赖** — 不开 `BUILD_PYTHON` 时只有 xdataset
2. **一个编译选项控制 Python** — `option(BUILD_PYTHON)` 决定是否嵌入解释器 + pybind11 绑定
3. **无额外产物** — 不产 `rel_plugin.dll` / `rel_python_plugin.dll`，全部在 `rel_runtime.dll` 内
4. **不需要导出层** — 无需 `rel_plugin_api.h`、回调注入、ABI 兼容层
5. **未来 `rel.dll` 友好** — 同样的 `BUILD_PYTHON` 选项继承

---

## 2. 产物全景

```
  rel_runtime.dll  ─────────────────────────────────────┐
  │                                                     │
  ├── value.cc, function.cc, environment.cc  ...        │
  ├── rel_plugin.cc          (C++ 插件加载, 始终编译)     │
  │                                                     │
  └── [BUILD_PYTHON=ON]                                 │
      ├── python/rel_module.cc       PYBIND11_MODULE     │
      ├── python/xdataset_bindings.cc                    │
      ├── python/rel_bindings.cc                         │
      └── python/python_loader.cc    LoadPython / ExecPython
```

### 依赖关系

```
  BUILD_PYTHON=OFF:                BUILD_PYTHON=ON:

    xdataset.dll                      xdataset.dll
         ↑                                 ↑
    rel_runtime.dll                  rel_runtime.dll
         ↑                              ↑        ↑
    rel_core (STATIC)              rel_core   pybind11::embed
         ↑                              ↑
       rel.exe                        rel.exe
```

### 职责表

| 产物 | 类型 | 职责 | Python 依赖 |
|------|------|------|:---:|
| `rel_runtime.dll` | SHARED | Value, Environment, 注册表, C++ 插件加载, (可选) Python 嵌入 | `BUILD_PYTHON` 控制 |
| `rel_core` | STATIC | Scanner, Parser, AST, Evaluator | ❌ |
| `rel.exe` | EXE | CLI 驱动 | ❌ |
| 未来 `rel.dll` | SHARED | 完整 REL API | `BUILD_PYTHON` 控制 |

---

## 3. 文件布局

```
REL/
├── CMakeLists.txt                       # BUILD_PYTHON option, add_subdirectory(src)
│
├── src/runtime/
│   ├── environment.h/.cc                # 保留 LoadFunctionPlugin / UnloadFunctionPlugin
│   ├── rel_plugin.h                     # C ABI 头
│   ├── rel_plugin.cc                    # C++ DLL 插件加载 (不动)
│   │
│   └── python/                          # [BUILD_PYTHON=ON] 才编译
│       ├── rel_module.cc                # PYBIND11_MODULE(rel, m) 入口 + __getattr__ 内建函数懒加载
│       ├── xdataset_bindings.cc         # Unit, Measurement, DataSeries, DataArray, Block, Dataset
│       ├── rel_bindings.cc              # Value, Param, ComputedParam, register_function, Function→callable 桥接
│       └── python_loader.cc             # LoadPython, ExecPython (惰性初始化)
│
├── plugin/                              # 仅文档 + 示例
│   ├── BUILD.md                         # 本文档
│   ├── PYTHON_API.md                    # Python 接口设计
│   └── example/
│       ├── rel_plugin_sample.cc         # C++ 插件示例
│       └── example_functions.py         # Python 插件示例
│
└── tests/
    └── test_plugin.cc                   # 测试 LoadFunctionPlugin / LoadPython
```

---

## 4. CMake 构建

### 4.1 顶层 CMakeLists.txt

```cmake
# ---- 编译选项 ----
option(BUILD_PYTHON "Enable embedded Python + plugin support" OFF)

if(BUILD_PYTHON)
    find_package(Python3 COMPONENTS Interpreter Development.Embed REQUIRED)
    find_package(pybind11 CONFIG REQUIRED)
    # numpy include 路径
endif()

# ---- rel_runtime ----
set(REL_RUNTIME_SOURCES
    src/runtime/value.cc
    src/runtime/function.cc
    src/runtime/environment.cc
    src/runtime/rel_plugin.cc        # C++ 插件加载, 始终编译
    ...(其他)...
)

if(BUILD_PYTHON)
    list(APPEND REL_RUNTIME_SOURCES
        src/runtime/python/rel_module.cc
        src/runtime/python/xdataset_bindings.cc
        src/runtime/python/rel_bindings.cc
        src/runtime/python/python_loader.cc
    )
endif()

add_library(rel_runtime SHARED ${REL_RUNTIME_SOURCES})

target_include_directories(rel_runtime PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/xdataset/include/xdataset
    ${CMAKE_SOURCE_DIR}/third_party/xdataset
    ${NUMPY_INCLUDE_DIR}
)

target_link_libraries(rel_runtime PUBLIC
    xdataset
    $<$<BOOL:${BUILD_PYTHON}>:pybind11::embed>
)

target_compile_definitions(rel_runtime PRIVATE
    $<$<BOOL:${BUILD_PYTHON}>:REL_HAS_PYTHON>
)

# ---- rel.exe ----
add_executable(rel src/main.cc)
target_link_libraries(rel PRIVATE rel_core rel_runtime)
```

### 4.2 构建变体

| CMake 选项 | `rel_runtime.dll` 包含 |
|---|---|
| `BUILD_PYTHON=OFF` (默认) | C++ 插件加载，无 Python |
| `BUILD_PYTHON=ON` | C++ 插件加载 + Python 嵌入 + pybind11 绑定 |

---

## 5. API — `Environment` 直接暴露

不再需要 `rel_plugin_api.h`。`Environment` 原生支持：

```cpp
// src/runtime/environment.h

namespace rel {

class Environment {
public:
    // ---- C++ 插件 (始终可用) ----

    /// Load a C++ plugin DLL, register its functions into this Environment.
    void* LoadFunctionPlugin(const char* path);

    /// Unload a C++ plugin, unregister its functions.
    void  UnloadFunctionPlugin(void* handle);

    // ---- Python 插件 (BUILD_PYTHON=ON 时可用, 否则抛 runtime_error) ----

    /// Execute a .py script in an independent context.
    /// Each script gets its own globals dict — plugins cannot pollute
    /// each other or the host's __main__.
    /// Automatically initializes the Python interpreter on first call.
    bool  LoadPython(const char* path);

    /// Execute a Python string directly, likewise in an isolated context.
    bool  ExecPython(const char* code);

    /// True when the Python interpreter has been initialized.
    bool  IsPythonAvailable() const;
};

} // namespace rel
```

### 使用场景

```cpp
rel::Environment env;

// C++ 插件 (BUILD_PYTHON=OFF/ON 都能用):
void* h = env.LoadFunctionPlugin("my_funcs.dll");

// Python 插件 (BUILD_PYTHON=ON, 首次调用自动初始化解释器):
env.LoadPython("my_funcs.py");
// 脚本中的 rel.register_function("snr", ...)
//   → 函数直接注册到 env 的注册表

env.UnloadFunctionPlugin(h);
```

### 惰性初始化

用户**不需要**显式调用 `InitPython()`。`LoadPython()` / `ExecPython()` 首次被调用时自动：

```cpp
// python_loader.cc (内部实现, pybind11 API)

#include <pybind11/embed.h>

static bool EnsureInterpreter() {
    if (Py_IsInitialized()) {
        // 宿主已经初始化了 Python（如 Maya / Blender / QGIS）
        // 只注册 "rel" 模块, 不改变现有解释器状态
        return true;
    }

    // 宿主没有 Python → 全隔离初始化
    pybind11::scoped_interpreter guard{};
    // scoped_interpreter 内部使用 PyConfig, 不读 PYTHONPATH / site-packages

    // 注册 "rel" 为 builtin module
    PYBIND11_EMBEDDED_MODULE(rel, m) {
        // xdataset_bindings.cc 和 rel_bindings.cc 中的绑定
    }

    return true;
}

bool Environment::LoadPython(const char* path) {
    if (!EnsureInterpreter()) return false;

    pybind11::gil_scoped_acquire gil;

    // 每个插件文件一个独立的 globals dict — 互不污染
    pybind11::dict script_globals;
    script_globals["__builtins__"] = pybind11::module_::import("builtins");

    // 注入当前 Environment
    pybind11::module_ rel_mod = pybind11::module_::import("rel");
    rel_mod.attr("current_env") = pybind11::cast(this, pybind11::return_value_policy::reference);

    pybind11::eval_file(path, script_globals);
    return true;
}
```

> **插件之间完全隔离**：`a.py` 的 `x = 1` 不会出现在 `b.py` 中。`register_function()` 是唯一的跨插件通信渠道——它写入 `Environment` 的注册表，不依赖 Python 命名空间。

### `LoadFromConfig` 中的 plugin 字段

因为 `LoadFunctionPlugin` 就在 `rel_runtime.dll` 内，`LoadFromConfig` 直接调用即可，**不需要回调注入**。

```json
{
    "plugins": [
        "my_funcs.dll",
        "my_funcs.py"
    ]
}
```
→ `LoadFunctionPlugin("my_funcs.dll")` + `LoadPython("my_funcs.py")`，都是 `Environment` 的内置方法。

---

## 6. 跨插件函数调用（插件 ABI v4）

### 6.1 问题：插件无法调用其他函数

函数注册表是全局静态的（`Environment::functions_`），内建、DLL 插件、Python
插件注册的函数都在同一张表里。但：

- `Environment::FindFunction` / `Function::Invoke` 都在 `rel_runtime.dll` 内；
- DLL 插件只链接 `xdataset`（不链接 `rel_runtime`），拿不到这两个符号；
- `RelPluginApi` 只有 `register_library`（单向：插件→宿主），没有反向的
  "查找/调用"服务。

结果：**插件函数实现里无法调用其他函数**（内建、其他 DLL 插件、Python 插件），
而 REL 表达式里 `foo(bar(x))` 可以自然组合。这造成宿主与插件的能力不对等。

### 6.2 方案：Invoke 变 inline + `find_function` 回调

两个改动，让插件作者写调用代码的方式与 `Evaluator::try_function_call` 完全一致：

**（1）`Function::Invoke` 变为 header-only（inline）**

`Invoke` 只依赖 `name_` / `params_` / `impl_`（均为 `function.h` 内字段），不碰
`Environment`。把 `function.cc` 的实现移入 `function.h` 末尾（inline），并去掉
`REL_RUNTIME_API` 导出。插件已 include header-only 的 `function.h`，于是可直接
调用 `Invoke`，无需链接 `rel_runtime`。

**（2）`RelPluginApi` 新增 `find_function` 回调**

```c
/// Host-provided: look up a registered function by name.
/// Returns a pointer valid until the next host registry mutation, or nullptr
/// when not found. Caller must copy the Function before invoking (Invoke may
/// re-register and rehash the map, invalidating the pointer).
typedef const void* (*RelFindFunctionFn)(void* host_context, const char* name);

typedef struct RelPluginApi {
    int api_version;                        // REL_PLUGIN_API_VERSION = 4
    RelRegisterLibraryFn register_library;
    RelFindFunctionFn   find_function;      // 新增
} RelPluginApi;
```

host 侧（`rel_plugin.cc`）：

```cpp
const void* host_find_function(void* ctx, const char* name) {
    return static_cast<const void*>(Environment::FindFunction(name));
}
```

### 6.3 插件用法

```cpp
extern "C" REL_PLUGIN_API int rel_plugin_main(const RelPluginApi* api, void* ctx) {
    auto find_fn = [api, ctx](const char* name) -> const rel::Function* {
        return static_cast<const rel::Function*>(api->find_function(ctx, name));
    };

    rel::FunctionLibrary lib("my_plugin");
    lib.Add(rel::Function("twice_sin",
        { rel::Param("x") },
        [find_fn](const rel::Function::ArgMap& a) -> rel::Value {
            const rel::Function* fn = find_fn("sin");   // 内建 / 其他 DLL / Python
            if (!fn) throw std::runtime_error("sin not found");
            rel::Function copy = *fn;                    // 拷贝后再 Invoke（防 rehash 失效）
            rel::Value s = copy.Invoke(a);               // 与 Evaluator 相同的调用路径
            double x = s.as_measurement().as_scalar<double>();
            return rel::Value::Real(2 * x);
        }));

    api->register_library(ctx, &lib);
    return 0;
}
```

四个方向全部打通：

| 调用方 \ 被调方 | 内建 | DLL 插件 | Python 插件 |
|------|:---:|:---:|:---:|
| 内建 | ✅ | ✅ | ✅ |
| DLL 插件 | ✅ `find_function` | ✅ `find_function` | ✅ `find_function`（impl 是桥接 lambda，获取 GIL 调 Python） |
| Python 插件 | ✅ `rel.sin(...)` | ✅ `rel.sqr(...)` | ✅ `rel.my_fn(...)` |

（Python 插件通过 `PYTHON_API.md` §8.4 的 `__getattr__` 桥接调用，最终同样落到
`Environment::FindFunction` + `Invoke`。）

### 6.4 注意事项

- **ABI 版本**：`RelPluginApi` 结构体布局变化，`REL_PLUGIN_API_VERSION` 3 → 4，
  旧插件（v3）需重新编译。
- **指针生命周期**：`find_function` 返回的指针在 `Invoke` 期间可能因 rehash 失效，
  **必须先拷贝 `Function` 再 `Invoke`**（与 `Evaluator::try_function_call` 相同）。
- **GIL 重入**：DLL 插件函数若在 Python 调用链中被调用，再经 `find_function`
  调用 Python 注册的函数时会重入 Python（桥接 lambda 内 `gil_scoped_acquire`）。
  同一线程重入 GIL 是安全的，但桥接需用可重入 acquire 避免死锁。
- **`function.cc` 清空**：`Invoke` 是 `function.cc` 唯一成员实现，inline 化后该
  文件可删除或清空。

---

## 7. 迁移清单

| 步骤 | 内容 | 影响 |
|------|------|------|
| 1 | 创建 `src/runtime/python/` 目录 | 新文件 |
| 2 | `function.cc` 的 `Invoke` 移入 `function.h`（inline），去掉 `REL_RUNTIME_API` 导出 | 移除导出符号，`function.cc` 清空/删除 |
| 3 | `rel_plugin.h` 加 `RelFindFunctionFn`，`REL_PLUGIN_API_VERSION` 3 → 4 | **ABI 破坏**，旧插件（v3）需重新编译 |
| 4 | `rel_plugin.cc` 加 `host_find_function` 回调实现 | 小改动 |
| 5 | 实现 Python 绑定 (`src/runtime/python/*.cc`)，含 `__getattr__` 内建函数懒加载 | 新代码 |
| 6 | 顶层 CMakeLists 加 `BUILD_PYTHON` option + 条件编译 | CMake 改动 |
| 7 | `test_plugin.cc` 加跨插件调用测试 + Python 测试 (`BUILD_PYTHON=ON`) | 新测试 |
| 8 | `rel.exe` 支持 `--py` flag | CLI 扩展 |

对比之前的 9-10 步方案，核心 Python 绑定部分仍无破坏性变更；唯一破坏点在第 3 步的
插件 ABI 版本号（3 → 4），仅影响已编译的旧版 DLL 插件，需重新编译。

---

## 8. 开放问题

### 8.1 双层隔离：进程级 + 脚本级

**第一层 — 进程级隔离**（自动，用户无感）：

`LoadPython()` / `ExecPython()` 首次调用时通过 pybind11 自动初始化 CPython。
`pybind11::scoped_interpreter` 底层对应 `Py_Initialize()`，一个进程只能调一次。

| 场景 | 行为 |
|------|------|
| 宿主未初始化 Python（`rel.exe`） | pybind11 创建隔离解释器，不读 `PYTHONPATH` / `site-packages` |
| 宿主已初始化 Python（嵌入 Maya/Blender 等） | 复用现有解释器，只注册 `rel` 模块，不改宿主状态 |

**第二层 — 脚本级隔离**（每个 `.py` 独立 `globals` dict）：

```cpp
// 每个脚本独立上下文, 不共享 __main__
pybind11::dict script_globals;
script_globals["__builtins__"] = pybind11::module_::import("builtins");
pybind11::eval_file("my_funcs.py", script_globals);
// a.py 的 x = 1 不会出现在 b.py 中
```

| 隔离维度 | 效果 |
|------|------|
| 插件之间 | `a.py` 和 `b.py` 不共享任何变量，完全隔离 |
| 宿主 `__main__` | 不受影响，宿主已有的全局变量不暴露给插件，也不被插件修改 |
| `register_function()` | 唯一跨插件通信渠道 — 写入 `Environment` 注册表，不依赖 Python 命名空间 |

`rel` 模块通过 `PYBIND11_EMBEDDED_MODULE(rel, m) { ... }` 注册为 builtin module，
pybind11 2.11+ 原生支持，Python 脚本中 `import rel` 直接可用。

> **子解释器** (`Py_NewInterpreter`) 提供更强的模块级隔离，但 pybind11 扩展模块跨子解释器共享受限，当前不推荐，等 Python 3.12+ PEP 684 生态成熟后再评估。

### 8.2 Python 版本耦合

`BUILD_PYTHON=ON` 时 `rel_runtime.dll` 链接 `pybind11::embed`，必须与系统 Python ABI 一致。
解决方案：默认 `BUILD_PYTHON=OFF`，由下游集成方显式开启并自行对接 Python。

### 8.3 外部 `import rel` 不可用

嵌入模式不产 `.pyd`，无法在独立 Python 进程中 `import rel`。如需此能力，V2 可：
- 单独产出 `rel.pyd` 共享同一套绑定源码
- 或通过 `rel_runtime.dll` 启动子进程传递序列化数据
