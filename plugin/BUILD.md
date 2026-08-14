# REL Extension System — Build Architecture

> **Status**: Updated  
> **Date**: 2026-08-14  
> **Companion to**: `PYTHON_API.md` (Python 接口设计)

---

## 1. 设计目标

1. **`rel_runtime.dll` 默认零外部依赖** — 不开 `BUILD_PYTHON` 时只有 xdataset
2. **一个编译选项控制 Python** — `option(BUILD_PYTHON)` 决定是否嵌入解释器 + pybind11 绑定
3. **C++ 扩展 = 静态库** — 不再有 DLL 插件加载机制；C++ 扩展是链接 `rel_runtime` 的静态库
4. **Python 是唯一的运行时插件** — 动态热扩展只走 `LoadPython` / `register_function`
5. **未来 `rel.dll` 友好** — 同样的 `BUILD_PYTHON` 选项继承

---

## 2. 产物全景

```
  rel_runtime.dll  ─────────────────────────────────────┐
  │                                                     │
  ├── value.cc, function.cc, environment.cc  ...        │
  ├── builtin_library/*    (builtin + math, 始终编译)     │
  │                                                     │
  └── [BUILD_PYTHON=ON]                                 │
      ├── python/rel_module.cc       PYBIND11_MODULE     │
      ├── python/xdataset_bindings.cc                    │
      ├── python/rel_bindings.cc                         │
      └── python/python_loader.cc    LoadPython / ExecPython

  rel_function_library_sample (STATIC)  ← C++ 扩展示例
       │  MakeLibrary()
       ▼
  rel.exe / rel_test 显式 RegisterLibrary
```

### 依赖关系

```
  BUILD_PYTHON=OFF:                BUILD_PYTHON=ON:

    xdataset.dll                      xdataset.dll
         ↑                                 ↑
    rel_runtime.dll                  rel_runtime.dll
         ↑         ↑                     ↑         ↑
    rel_core  扩展静态库          rel_core  扩展静态库  pybind11::embed
         ↑         ↑                     ↑         ↑
       rel.exe / rel_test               rel.exe / rel_test
```

### 职责表

| 产物 | 类型 | 职责 | Python 依赖 |
|------|------|------|:---:|
| `rel_runtime.dll` | SHARED | Value, Environment, 函数注册表, 内建库, (可选) Python 嵌入 | `BUILD_PYTHON` 控制 |
| `rel_core` | STATIC | Scanner, Parser, AST, Evaluator | ❌ |
| 扩展静态库 (如 `rel_function_library_sample`) | STATIC | C++ 函数库，`MakeLibrary()` 工厂 | ❌ |
| `rel.exe` | EXE | CLI 驱动，显式注册扩展 | ❌ |
| 未来 `rel.dll` | SHARED | 完整 REL API | `BUILD_PYTHON` 控制 |

---

## 3. 文件布局

```
REL/
├── CMakeLists.txt                       # BUILD_PYTHON option, rel_runtime / 扩展静态库目标
│
├── src/runtime/
│   ├── environment.h/.cc                # Environment（函数/常量/数据集注册表 + 配置加载）
│   ├── value.h/.cc                      # Value
│   │
│   ├── function/                        # 函数基础设施 (header-only)
│   │   ├── function.h                   # Function / FunctionParam / Param / ComputedParam
│   │   ├── function.cc                  # Function::Invoke
│   │   └── function_library.h           # FunctionLibrary
│   │
│   ├── builtin_library/                 # 内建函数库 (编译进 rel_runtime)
│   │   ├── builtin_library.h/.cc        # rel::builtin::MakeLibrary()
│   │   └── math_library.h/.cc           # rel::math::MakeLibrary()
│   │
│   ├── operation/                       # Value 运算符 + 数学运算
│   │
│   └── python/                          # [BUILD_PYTHON=ON] 才编译
│       ├── rel_module.cc                # PYBIND11_MODULE(rel, m) 入口 + __getattr__ 内建函数懒加载
│       ├── xdataset_bindings.cc         # Unit, Measurement, DataSeries, DataArray, Block, Dataset
│       ├── rel_bindings.cc              # Value, Param, ComputedParam, register_function, Function→callable 桥接
│       └── python_loader.cc             # LoadPython, ExecPython (惰性初始化)
│
├── src/function_library/                # C++ 扩展示例 (静态库)
│   ├── function_library_sample.h        # rel::function_library_sample::MakeLibrary()
│   └── function_library_sample.cc       # sincos(x) = sin(x) * cos(x)
│
├── plugin/                              # 仅文档
│   ├── BUILD.md                         # 本文档
│   └── PYTHON_API.md                    # Python 接口设计
│
└── tests/
    └── test_function_library.cc         # 测试静态库扩展 (sincos)
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
    src/runtime/function/function.cc
    src/runtime/builtin_library/builtin_library.cc
    src/runtime/builtin_library/math_library.cc
    src/runtime/operation/operator.cc
    src/runtime/operation/pipeline.cc
    src/runtime/operation/operation_helpers.cc
    src/runtime/operation/math_operation.cc
    src/runtime/environment.cc
    src/runtime/environment_config.cc
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

# ---- C++ 扩展静态库 (示例) ----
add_library(rel_function_library_sample STATIC
    src/function_library/function_library_sample.cc
)
target_include_directories(rel_function_library_sample PUBLIC
    src/function_library
    src/runtime
)
target_link_libraries(rel_function_library_sample PUBLIC rel_runtime)

# ---- rel.exe ----
add_executable(rel src/main.cc)
target_link_libraries(rel PRIVATE rel_core rel_function_library_sample)
```

### 4.2 构建变体

| CMake 选项 | `rel_runtime.dll` 包含 |
|---|---|
| `BUILD_PYTHON=OFF` (默认) | 内建库，无 Python |
| `BUILD_PYTHON=ON` | 内建库 + Python 嵌入 + pybind11 绑定 |

C++ 扩展静态库不依赖 `BUILD_PYTHON`，始终可链接。

---

## 5. API — `Environment` 直接暴露

不再需要 `rel_plugin.h` / C ABI / `LoadFunctionPlugin`。`Environment` 提供：

```cpp
// src/runtime/environment.h

namespace rel {

class Environment {
public:
    // ---- 函数注册表 (全局静态) ----

    /// Register REL's builtin function libraries ("builtin" + "math").
    static void InitBuiltinFunctions();

    /// Register a single function (overwrites同名 silently).
    static void RegisterFunction(Function fn);

    /// Register every function in a library.
    static void RegisterLibrary(const FunctionLibrary& lib);

    /// Remove a registered function by name.
    static bool UnregisterFunction(const std::string& name);

    /// Look up a registered function by name, or nullptr.
    static const Function* FindFunction(const std::string& name);

    /// Names of all registered functions.
    static std::vector<std::string> FunctionNames();

    // ---- Python 插件 (BUILD_PYTHON=ON 时可用, 否则抛 runtime_error) ----

    bool  LoadPython(const char* path);
    bool  ExecPython(const char* code);
    bool  IsPythonAvailable() const;
};

} // namespace rel
```

### 使用场景

```cpp
rel::Environment env;
rel::Environment::InitBuiltinConstants();
rel::Environment::InitBuiltinFunctions();                 // 内建库
rel::Environment::RegisterLibrary(my_ext::MakeLibrary()); // C++ 扩展 (静态库)

// Python 插件 (BUILD_PYTHON=ON, 首次调用自动初始化解释器):
env.LoadPython("my_funcs.py");
// 脚本中的 rel.register_function("snr", ...)
//   → 函数直接注册到 env 的注册表
```

### 惰性初始化

用户**不需要**显式调用 `InitPython()`。`LoadPython()` / `ExecPython()` 首次被调用时自动：

```cpp
// python_loader.cc (内部实现, pybind11 API)

#include <pybind11/embed.h>

// P0: scoped_interpreter 必须是持久对象（文件级 static 或 new 永不释放），
// 不能是局部变量——否则函数返回即析构、解释器立刻 finalize。
static pybind11::scoped_interpreter* g_interp = nullptr;

static bool EnsureInterpreter() {
    if (Py_IsInitialized()) {
        // 宿主已经初始化了 Python（如 Maya / Blender / QGIS）
        // 只注册 "rel" 模块, 不改变现有解释器状态
        return true;
    }

    // 宿主没有 Python → 全隔离初始化（持久对象，进程退出才 finalize）
    g_interp = new pybind11::scoped_interpreter();
    return true;
}

bool Environment::LoadPython(const char* path) {
    if (!EnsureInterpreter()) return false;

    pybind11::gil_scoped_acquire gil;

    // 每个插件文件一个独立的 globals dict — 互不污染
    pybind11::dict script_globals;
    script_globals["__builtins__"] = pybind11::module_::import("builtins");

    pybind11::eval_file(path, script_globals);
    return true;
}
```

> **插件之间完全隔离**：`a.py` 的 `x = 1` 不会出现在 `b.py` 中。`register_function()` 是唯一的跨插件通信渠道——它写入 `Environment` 的注册表，不依赖 Python 命名空间。

> **P0 — 静态析构顺序**：Python 注册的函数（`Function::impl_` 捕获 `py::function`）
> 存在 `Environment::functions_`（static `unordered_map`）。进程退出时若解释器先
> finalize、`functions_` 后析构，则 `py::function` 析构时 `Py_DECREF` 会访问已销毁的
> Python 运行时 → 段错误。必须在 finalize 前显式清空 Python 注册的函数。

---

## 6. 跨扩展函数调用

### 6.1 模型：单一注册表 + 直接调用

函数注册表是全局静态的（`Environment::functions_`），内建、C++ 扩展、Python 插件
注册的函数都在同一张表里。C++ 扩展链接 `rel_runtime`，直接拿到 `Environment`
与 `Function` 的导出符号，**无需任何 C ABI / 回调**：

```cpp
// src/function_library/function_library_sample.cc
#include "function_library_sample.h"
#include "environment.h"

namespace rel {
namespace function_library_sample {
namespace {

/// 按名调用已注册函数，单参数 "x"。
Value CallRegistered(const char* name, const Value& x)
{
    const Function* fn = Environment::FindFunction(name);
    if (!fn)
        throw std::runtime_error(std::string("sincos: '") + name
                                 + "' is not registered");

    // 拷贝后再 Invoke：实现可能重注册并 rehash 注册表（与 Evaluator 一致）。
    Function copy = *fn;
    Function::ArgMap args;
    args["x"] = x;
    return copy.Invoke(args);
}

}  // namespace

FunctionLibrary MakeLibrary()
{
    FunctionLibrary lib("function_library_sample");
    lib.Add(Function("sincos",
        std::vector<FunctionParam>{ Param("x") },
        [](const Function::ArgMap& args) -> Value {
            const Value& x = args.at("x");
            const Value s = CallRegistered("sin", x);  // 先 sin
            const Value c = CallRegistered("cos", x);  // 再 cos
            double sv = s.as_measurement().as_scalar<double>();
            double cv = c.as_measurement().as_scalar<double>();
            return Value::Real(sv * cv);
        }));
    return lib;
}

}  // namespace function_library_sample
}  // namespace rel
```

三个方向全部打通，最终都落到 `Environment::FindFunction` + `Function::Invoke`：

| 调用方 \ 被调方 | 内建 | C++ 扩展 | Python 插件 |
|------|:---:|:---:|:---:|
| 内建 | ✅ | ✅ | ✅ |
| C++ 扩展 | ✅ `FindFunction` | ✅ `FindFunction` | ✅ `FindFunction`（impl 是桥接 lambda，获取 GIL 调 Python） |
| Python 插件 | ✅ `rel.sin(...)` | ✅ `rel.sincos(...)` | ✅ `rel.my_fn(...)` |

（Python 插件通过 `PYTHON_API.md` §8.4 的 `__getattr__` 桥接调用。）

### 6.2 注意事项

- **指针生命周期**：`FindFunction` 返回的指针在 `Invoke` 期间可能因 rehash 失效，
  **必须先拷贝 `Function` 再 `Invoke`**（与 `Evaluator::try_function_call` 相同）。
- **GIL 重入**：C++ 扩展函数若在 Python 调用链中被调用，再经 `FindFunction` 调用
  Python 注册的函数时会重入 Python（桥接 lambda 内 `gil_scoped_acquire`）。同一线程
  重入 GIL 是安全的，但桥接需用可重入 acquire 避免死锁。
- **注册顺序**：调用方扩展必须在被调方（内建 `sin`/`cos`）注册之后才 `RegisterLibrary`。
  `main.cc` 中 `InitBuiltinFunctions()` 在扩展 `RegisterLibrary` 之前，天然满足。

---

## 7. 迁移清单（已完成）

| 步骤 | 内容 | 状态 |
|------|------|------|
| 1 | `builtin_library` / `math_library` 移入 `src/runtime/builtin_library/`，命名空间 `rel::builtin` / `rel::math`，工厂改 `MakeLibrary()` | ✅ |
| 2 | 删除 DLL 插件机制：`rel_plugin.{h,cc}`、`LoadFunctionPlugin` / `UnloadFunctionPlugin`、`EnvironmentConfig::plugins`、`plugins/sample/`、`tests/test_plugin.cc` | ✅ |
| 3 | 新增 `src/function_library/function_library_sample.{h,cc}`（`sincos` 调 `sin`+`cos`），CMake 目标 `rel_function_library_sample` STATIC | ✅ |
| 4 | `rel.exe` / `rel_test` 链接扩展静态库；`main.cc` 显式 `RegisterLibrary` | ✅ |
| 5 | 实现 Python 绑定 (`src/runtime/python/*.cc`)，含 `__getattr__` 内建函数懒加载 | ⏳ 待做 |
| 6 | 顶层 CMakeLists 加 `BUILD_PYTHON` option + 条件编译 | ⏳ 待做 |
| 7 | Python 测试 (`BUILD_PYTHON=ON`) | ⏳ 待做 |

C++ 侧已无破坏性变更（不再有 ABI 版本号）；剩余待做项均为 Python 绑定（新代码）。

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
