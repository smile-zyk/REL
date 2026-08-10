# REL Plugin System — Build Architecture

> **Status**: Draft  
> **Date**: 2026-08-10  
> **Companion to**: `PYTHON_API.md` (Python 接口设计)

---

## 1. 设计目标

1. **`rel_runtime.dll` 零外部依赖** — 只做数据和注册表，插件加载迁出
2. **C++ 和 Python 共享统一 API 面** — 同一个 `rel_plugin_api.h`
3. **Python 可选** — 不装 Python 也能用 C++ 插件
4. **一份绑定源码，三份产物** — OBJECT library 编译一次，`rel.pyd` + `rel_python_plugin.dll` 共享
5. **未来 `rel.dll` 友好** — 不拖 Python 依赖

---

## 2. 产物全景

```
                            rel_module.cc  (PYBIND11_MODULE(rel, m))
                            xdataset_bindings.cc
                            rel_bindings.cc
                                    │
                                    ▼
                         rel_python_obj (OBJECT)
                           只编译一次, 产出 .obj
                                    │
                   ┌────────────────┼────────────────┐
                   ▼                ▼                ▼
              rel.pyd       rel_plugin.dll    rel_python_plugin.dll
         (外部 Python)    (C++ 插件加载器)    (Python 插件加载器)
```

### 依赖关系

```
                  xdataset.dll
                       ↑
                  rel_runtime.dll
                    ↑         ↑
                    │         │
            rel_plugin.dll  rel.pyd
               (C++)        (外部 import rel)
                    ↑
                    │
          rel_python_plugin.dll
          (Python embed)
```

### 职责表

| 产物 | 类型 | 职责 | Python 依赖 |
|------|------|------|:---:|
| `rel_runtime.dll` | SHARED | Value, Environment, 函数注册表, `RegisterFunction()` | ❌ |
| `rel_core` | STATIC | Scanner, Parser, AST, Evaluator | ❌ |
| **`rel_plugin.dll`** | SHARED | C++ DLL 加载, `rel::plugin::Load()` / `Unload()` | ❌ |
| **`rel_python_plugin.dll`** | SHARED | 嵌入 Python 解释器, `InitPython()` / `LoadPython()` | ✅ embed |
| **`rel.pyd`** | MODULE | 外部 `import rel` | ✅ module |
| `rel.exe` | EXE | CLI 驱动，可选链接 plugin DLL | ❌ |
| 未来 `rel.dll` | SHARED | 完整 REL API | ❌ |

---

## 3. 文件布局

```
REL/
├── CMakeLists.txt                   # 仅加 add_subdirectory(plugin)
│
├── src/runtime/
│   ├── environment.h/.cc            # 移除 LoadFunctionPlugin / UnloadFunctionPlugin
│   ├── rel_plugin.h                 # C ABI 头, 保留(插件 DLL 需要)
│   └── rel_plugin.cc               # 移出 → plugin/cpp/
│
├── plugin/
│   ├── CMakeLists.txt               # rel_plugin.dll + rel_python_plugin.dll + rel.pyd
│   ├── rel_plugin_api.h             # 统一 API 头 (给 host #include)
│   │
│   ├── cpp/
│   │   └── cpp_loader.cc            # 迁自 src/runtime/rel_plugin.cc
│   │
│   ├── python/
│   │   ├── rel_module.cc            # PYBIND11_MODULE(rel, m) 入口
│   │   ├── xdataset_bindings.cc     # Unit, Measurement, DataSeries, DataArray, Block, Dataset
│   │   ├── rel_bindings.cc          # Value, Param, register_function
│   │   └── python_loader.cc         # InitPython, LoadPython, ExecPython
│   │
│   ├── example/
│   │   ├── rel_plugin_sample.cc     # 迁自 plugins/sample/
│   │   └── example_functions.py     # 示例 Python 插件
│   │
│   ├── DESIGN.md                    # 编译架构 (本文档)
│   └── PYTHON_API.md                # Python 接口设计
│
└── tests/
    └── test_plugin.cc               # 适配 rel::plugin::Load
```

---

## 4. CMake 构建

### 4.1 顶层 CMakeLists.txt

```cmake
# ---- rel_runtime ----
# 移除 src/runtime/rel_plugin.cc
add_library(rel_runtime SHARED
    src/runtime/value.cc
    src/runtime/function.cc
    ...(其他不变)...
    # 不再: src/runtime/rel_plugin.cc
)
# 移除 dlopen 依赖 (如果之前为 rel_plugin 加的)
target_link_libraries(rel_runtime PUBLIC xdataset)

# ---- 可选: Python 插件 ----
option(BUILD_REL_PLUGIN "Build plugin DLLs (C++ + Python)" ON)
option(BUILD_REL_PLUGIN_PYTHON "Enable Python plugin support" OFF)

if(BUILD_REL_PLUGIN)
    if(BUILD_REL_PLUGIN_PYTHON)
        # 检测 pybind11 + numpy
        find_package(Python3 COMPONENTS Interpreter Development.Module Development.Embed REQUIRED)
        find_package(pybind11 CONFIG REQUIRED)
        # numpy include 路径
    endif()
    add_subdirectory(plugin)
endif()

# ---- rel.exe 不含 Python ----
add_executable(rel src/main.cc)
target_link_libraries(rel PRIVATE rel_core)
# 可选: 运行时动态加载 rel_plugin.dll / rel_python_plugin.dll
```

### 4.2 plugin/CMakeLists.txt

```cmake
# =========================================================================
# ① OBJECT library — 绑定源码编译一次
# =========================================================================
# 仅在 BUILD_REL_PLUGIN_PYTHON=ON 时构建

if(BUILD_REL_PLUGIN_PYTHON)
    add_library(rel_python_obj OBJECT
        python/rel_module.cc
        python/xdataset_bindings.cc
        python/rel_bindings.cc
    )
    target_include_directories(rel_python_obj PRIVATE
        ${CMAKE_SOURCE_DIR}/src/runtime
        ${CMAKE_SOURCE_DIR}/third_party/xdataset/include/xdataset
        ${CMAKE_SOURCE_DIR}/third_party/xdataset
        ${NUMPY_INCLUDE_DIR}
    )
    target_link_libraries(rel_python_obj PUBLIC pybind11::headers)
endif()

# =========================================================================
# ② rel_plugin.dll — C++ 插件加载器
# =========================================================================

add_library(rel_plugin SHARED
    cpp/cpp_loader.cc           # 迁自 src/runtime/rel_plugin.cc
)
target_include_directories(rel_plugin PUBLIC
    ${CMAKE_SOURCE_DIR}/src/runtime
)
target_link_libraries(rel_plugin PUBLIC rel_runtime)
# 无 Python 依赖

# =========================================================================
# ③ rel_python_plugin.dll — Python 插件加载器
# =========================================================================

if(BUILD_REL_PLUGIN_PYTHON)
    add_library(rel_python_plugin SHARED
        python/python_loader.cc
        $<TARGET_OBJECTS:rel_python_obj>
    )
    target_include_directories(rel_python_plugin PUBLIC
        ${CMAKE_SOURCE_DIR}/src/runtime
    )
    target_link_libraries(rel_python_plugin PUBLIC
        rel_plugin           # 复用函数追踪等公共设施
        pybind11::embed      # 嵌入 Python 解释器
    )
endif()

# =========================================================================
# ④ rel.pyd — 外部 Python 可用
# =========================================================================

if(BUILD_REL_PLUGIN_PYTHON)
    pybind11_add_module(rel_pyd MODULE
        $<TARGET_OBJECTS:rel_python_obj>
    )
    target_link_libraries(rel_pyd PRIVATE rel_runtime)
    set_target_properties(rel_pyd PROPERTIES
        OUTPUT_NAME rel
        PREFIX ""
        SUFFIX ".pyd"
    )
    # 拷贝依赖 DLL 到 .pyd 同目录
    add_custom_command(TARGET rel_pyd POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:xdataset>
            $<TARGET_FILE_DIR:rel_pyd>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:rel_runtime>
            $<TARGET_FILE_DIR:rel_pyd>
    )
endif()
```

### 4.3 构建变体

| CMake 选项 | 产物 |
|---|---|
| `BUILD_REL_PLUGIN=OFF` | 无插件 DLL（只 `rel_runtime.dll` + `rel.exe`） |
| `BUILD_REL_PLUGIN=ON, PYTHON=OFF` | `rel_plugin.dll` 只有 C++ 能力 |
| `BUILD_REL_PLUGIN=ON, PYTHON=ON` | `rel_plugin.dll` + `rel_python_plugin.dll` + `rel.pyd` |

---

## 5. 统一 API (`rel_plugin_api.h`)

```cpp
#ifndef REL_PLUGIN_API_H_
#define REL_PLUGIN_API_H_

namespace rel {
namespace plugin {

// =========================================================================
//  C++ DLL 插件 (rel_plugin.dll)
// =========================================================================

/// Load a C++ plugin DLL.  Returns opaque handle or nullptr.
void* Load(const char* path);

/// Unload a C++ plugin.  Unregisters functions, releases library.
void  Unload(void* plugin);

// =========================================================================
//  Python 插件 (rel_python_plugin.dll) — 仅当链接时可用
// =========================================================================

/// Initialize embedded Python + register "rel" builtin module.
bool  InitPython();

/// Execute a .py file.  Functions registered via rel.register_function()
/// land in the global registry immediately.
bool  LoadPython(const char* path);

/// Execute a Python string directly.
bool  ExecPython(const char* code);

/// True when InitPython() succeeded.
bool  IsPythonAvailable();

/// Shut down the interpreter (optional).
void  ShutdownPython();

} // namespace plugin
} // namespace rel

#endif
```

### 使用场景

```cpp
// 只用 C++ 插件:
#include "rel_plugin_api.h"
// 链接: rel_plugin.dll

void* h = rel::plugin::Load("my_funcs.dll");
rel::plugin::Unload(h);

// C++ + Python:
#include "rel_plugin_api.h"
// 链接: rel_plugin.dll + rel_python_plugin.dll

rel::plugin::InitPython();
rel::plugin::Load("my_funcs.dll");
rel::plugin::LoadPython("my_funcs.py");
rel::plugin::ShutdownPython();

// 只用外部 Python:
// 不需要这个头, 不需要链接任何 plugin DLL
// python -c "import rel"
```

---

## 6. 迁移清单

| 步骤 | 内容 | 影响 |
|------|------|------|
| 1 | 创建 `plugin/` 目录结构 | 新文件 |
| 2 | `rel_plugin.cc` → `plugin/cpp/cpp_loader.cc` | 代码搬迁 |
| 3 | `Environment` 移除 `LoadFunctionPlugin` / `UnloadFunctionPlugin` | 破坏性: 调用方需适配 |
| 4 | `rel_plugin_sample` → `plugin/example/` | 搬迁 |
| 5 | `test_plugin.cc` 适配 `rel::plugin::Load()` | 测试适配 |
| 6 | `Environment::LoadFromConfig()` 中 `"plugin"` 回调 | 见 §7 |
| 7 | 实现 Python 绑定 (`plugin/python/*.cc`) | 新代码 |
| 8 | 实现 `rel_python_plugin.dll` | 新目标 |
| 9 | 实现 `rel.pyd` | 新目标 |
| 10 | `rel.exe` 支持 `--py` / `--plugin` | CLI 扩展 |

---

## 7. 开放问题

### 7.1 LoadFromConfig 中的 plugin 字段

当前 `Environment::LoadFromConfig()` 直接调用 `LoadFunctionPlugin()`(在 `rel_runtime.dll` 内)。
迁移后 `LoadFunctionPlugin` 不再存在于 `rel_runtime.dll`。

**方案**：通过回调注入。

```cpp
// rel_plugin_api.h
using PluginLoader = void* (*)(const char* path);
void SetPluginLoader(PluginLoader loader);

// host 初始化时注册
rel::plugin::SetPluginLoader(&rel::plugin::Load);
```

### 7.2 外部 Python `import rel` 的能力边界

外部 Python 没有正在运行的 `Environment`，因此：
- ✅ 构造/操作 `Measurement`, `DataArray`, `Dataset`
- ✅ 读写 HDF5 / Touchstone 文件
- ❌ 调用 REL 表达式求值（没有 Evaluator）
- ❌ 访问内置函数（没有初始化）

V2 可加入 `rel.eval("sin(pi/2)")` 轻量求值入口。

### 7.3 MinGW vs MSVC ABI

当前项目使用 MinGW g++。`.pyd` 和 `rel_python_plugin.dll` 必须用 MinGW Python
(`C:/msys64/mingw64/bin/python.exe`)。若用 MSVC 编译则与官方 CPython ABI 一致。
