# REL Python Plugin — 实现与 API

> **Status**: Draft
> **Date**: 2026-08-15
> **Scope**: `rel_runtime` + `xdataset` 的 Python 嵌入实现与完整接口

---

## 1. 定位

- **本文档只覆盖 Python 插件**:`rel_runtime` 的 Python 嵌入实现与完整接口。
- C++ 函数扩展(静态库 + `MakeLibrary()` + `RegisterLibrary`)不在此范围,示例见
  `src/function_library/function_library_sample.{h,cc}`。
- **Python 是唯一的运行时插件**:动态热扩展只走 `LoadPython` / `register_function`。

### 核心原则(一条)

> **`py::function` 永不进入 `Environment` 的数据结构。** 所有 Python 对象只存在于
> 一个专门的、持 GIL 访问的容器里;`Environment` 里只保留纯 C++ 的 `std::string`
> 名字作为"钥匙"。

这一条原则同时消解了 GIL 边界、拷贝时序、静态析构顺序三类问题(详见 §4)。

---

## 2. 构建集成

### 2.1 编译开关

`rel_runtime.dll` 默认零外部依赖(只有 xdataset)。一个选项控制 Python:

```cmake
option(BUILD_PYTHON "Enable embedded Python + plugin support" OFF)

if(BUILD_PYTHON)
    find_package(Python3 COMPONENTS Interpreter Development.Embed REQUIRED)
    find_package(pybind11 CONFIG REQUIRED)
    # numpy include 路径 (${NUMPY_INCLUDE_DIR})
endif()
```

### 2.2 源文件与链接

```cmake
if(BUILD_PYTHON)
    list(APPEND REL_RUNTIME_SOURCES
        src/runtime/python/rel_module.cc
        src/runtime/python/xdataset_bindings.cc
        src/runtime/python/rel_bindings.cc
        src/runtime/python/python_loader.cc
    )
endif()

target_link_libraries(rel_runtime PUBLIC
    xdataset
    $<$<BOOL:${BUILD_PYTHON}>:pybind11::embed>
)
target_compile_definitions(rel_runtime PRIVATE
    $<$<BOOL:${BUILD_PYTHON}>:REL_HAS_PYTHON>
)
```

### 2.3 构建变体

| CMake 选项 | `rel_runtime.dll` 包含 |
|---|---|
| `BUILD_PYTHON=OFF` (默认) | 内建库，无 Python |
| `BUILD_PYTHON=ON` | 内建库 + Python 嵌入 + pybind11 绑定 |

### 2.4 文件布局

```
src/runtime/python/
    rel_module.cc         # PYBIND11_EMBEDDED_MODULE(rel, m) 入口 + __getattr__ 懒加载桥接
    xdataset_bindings.cc  # Unit, Measurement, DataSeries, DataArray, Block, Dataset
    rel_bindings.cc       # Value, Param, ComputedParam, register_function, FunctionProxy
    python_loader.cc      # LoadPython / ExecPython (惰性初始化 + 退出钩子)
```

---

## 3. 解释器生命周期(`python_loader.cc`)

### 3.1 惰性初始化

用户**不需要**显式调用 `InitPython()`。`LoadPython()` / `ExecPython()` 首次调用时自动:

```cpp
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

    // 宿主没有 Python → 全隔离初始化（持久对象，进程退出才 finalize）。
    // 注意：scoped_interpreter 底层就是 Py_Initialize()，默认会读 PYTHONPATH
    // 与 site-packages。需要真正隔离时改用 PyConfig：
    //   PyConfig cfg; PyConfig_InitIsolatedConfig(&cfg); Py_InitializeFromConfig(&cfg);
    g_interp = new pybind11::scoped_interpreter();
    return true;
}
```

| 场景 | 行为 |
|------|------|
| 宿主未初始化 Python（`rel.exe`） | 默认 `Py_Initialize()`（读 `PYTHONPATH` / `site-packages`）；**需要隔离时改用 `PyConfig_InitIsolatedConfig`** |
| 宿主已初始化 Python（嵌入 Maya/Blender 等） | 复用现有解释器，只注册 `rel` 模块，不改宿主状态 |

> **澄清**：`pybind11::scoped_interpreter` 并不"隔离"，它默认加载 site-packages。
> 需要真正隔离（不加载用户环境里任意 site-packages）时，必须用
> `PyConfig_InitIsolatedConfig` + `Py_InitializeFromConfig` 自行初始化，而不用
> `scoped_interpreter`。

### 3.2 脚本级隔离

每个 `.py` 独立 `globals` dict，互不污染:

```cpp
bool Environment::LoadPython(const char* path) {
    if (!EnsureInterpreter()) return false;

    pybind11::gil_scoped_acquire gil;

    pybind11::dict script_globals;
    script_globals["__builtins__"] = pybind11::module_::import("builtins");

    pybind11::eval_file(path, script_globals);
    return true;
}
```

| 隔离维度 | 效果 |
|------|------|
| 插件之间 | `a.py` 的 `x = 1` 不会出现在 `b.py` 中 |
| 宿主 `__main__` | 不受影响，宿主已有全局变量不暴露给插件，也不被插件修改 |
| `register_function()` | 唯一跨插件通信渠道 — 写入 `Environment` 注册表，不依赖 Python 命名空间 |

`rel` 模块通过 `PYBIND11_EMBEDDED_MODULE(rel, m) { ... }` 注册为 builtin module
（pybind11 2.11+ 原生支持），脚本中 `import rel` 直接可用。

### 3.3 退出钩子

```cpp
void ShutdownPython() {
    {
        pybind11::gil_scoped_acquire gil;   // 析构 py::function 必须持 GIL
        for (const auto& name : g_python_registered)
            Environment::UnregisterFunction(name);  // Function 是纯 C++，erase 安全
        g_python_registered.clear();
        g_py_callbacks.clear();             // py::function 析构，持 GIL
    }
    delete g_interp;      // 触发解释器 finalize；必须在清空函数之后
    g_interp = nullptr;
}
```

因为 §1 的原则（`Function` 里不含 `py::function`），静态析构顺序问题被消除:
`Environment::functions_` 析构时只释放 `std::string`，不碰 CPython 运行时。

---

## 4. 函数桥接(`rel_bindings.cc` + `rel_module.cc`)

### 4.1 模型:纯 C++ shim + GIL 保护的容器

`rel.register_function("snr", params, fn)` 做两件事:

1. **持 GIL** 把 `fn` 存进 `g_py_callbacks`（文件级 `unordered_map<string, py::function>`）。
2. 往 `Environment` 注册一个**纯 C++ shim**，其 `impl_` 只捕获 `std::string name`:

```cpp
// python_loader.cc —— 唯一持有 py::function 的地方，永远持 GIL 访问
namespace {
  std::unordered_map<std::string, py::function> g_py_callbacks;
  std::vector<std::string> g_python_registered;  // 退出前统一释放
}

Environment::RegisterFunction(Function(
    name, params,
    [name](const Function::ArgMap& args) -> Value {
        py::gil_scoped_acquire gil;              // 可重入：已在 GIL 中也无害
        auto it = g_py_callbacks.find(name);
        if (it == g_py_callbacks.end())
            throw std::runtime_error("python callback '" + name + "' gone");
        return it->second(convert_to_py(args));  // dict → 调 fn → 转 Value
    }));
```

**为什么这样简洁高效**:

- `impl_` 捕获的是 `std::string`，不是 `py::function` → `Function` 拷贝/析构全程纯
  C++，**不需要 GIL**，也不存在 `Py_INCREF`/`Py_DECREF` 时序问题。
- `py::function` 只在 `g_py_callbacks`，只在 GIL 内被碰。
- `gil_scoped_acquire` 可重入 → 统一在 shim 入口 acquire 一次，无论从纯 C++ 还是从
  Python 调用链进来都安全，**不需要在调用点判断"我是否已持 GIL"**。

### 4.2 对 C++ 侧透明

Python 注册的函数与内建 / C++ 扩展函数在注册表里平级:REPL、表达式求值(evaluator)
或其它函数调用它时,统一走同一条路径 → shim 获取 GIL → 调 Python 回调。Python 侧
无需感知调用方是谁,也无需为 C++ 侧做任何适配。

### 4.3 内建函数自动暴露 — `__getattr__` + FunctionProxy

**机制**：函数注册表 + PEP 562 懒加载桥接。

1. `rel` 模块定义 `__getattr__(name)`（Python 3.7+）。
2. 首次访问 `rel.sin` 时普通属性查找失败 → `__getattr__("sin")` → 查函数注册表。
3. 命中则返回一个轻量 **FunctionProxy**（不缓存 `Function` 副本）；未命中抛 `AttributeError`。

```python
class FunctionProxy:
    def __init__(self, name):
        self._name = name
    def __call__(self, *args, **kwargs):
        # 位置/关键字参数 → 参数名 → Environment::CallFunction(name, ...)
        return _call_function(self._name, args, kwargs)
```

**调用约定**（与 REL 一致）:位置参数按声明顺序映射到参数名，关键字参数按名映射，
省略的参数由 `Function::Invoke` 用默认值/计算默认值补齐。

```python
rel.sin(x)                       # → rel.Value
rel.db(r, 50, 75)                # 位置参数
rel.db(r=r, z1=50, z2=75)        # 关键字参数
rel.what(da)                     # builtin 库
rel.indep(da, "freq")            # 默认参数 selector=Integer(1)
```

**为什么用转发代理而非缓存**:

| 方案 | stale | dangling | GIL 拷贝 | 代价 |
|------|:---:|:---:|:---:|------|
| **转发代理（采用）** | ✅ | ✅ | ✅ | 每次多一次 map 查找（纳秒级） |
| 缓存 Function 副本 | ⚠️ 陈旧 | ⚠️ 悬挂 | ⚠️ 需 GIL | 热路径略快 |

转发代理每次现查 `CallFunction`，因此:
- **无 stale**:热更新 / 注销立即生效;
- **无 dangling**:不缓存 `py::function`，规避静态析构顺序问题;
- **无 GIL 拷贝**:不复制捕获 `py::function` 的 `std::function`。

**内建常量**:同一 `__getattr__` 依次查找 函数 → 常量（`rel.PI` / `rel.e` / `rel.c0` /
`rel.i` 等由 `Environment::FindConstant` 提供）。

---

## 5. Python API

### 5.0 类型全景

```
用户可见类型（都在 `import rel` 下）:

    Unit                — 物理单位
    Measurement         — 带单位的标量 / 向量 / 矩阵（值类型）
    DataSeries          — 一列 Measurement, numpy 互操作
    DataArray           — 多维数组 + 坐标轴
      DimGroup          —   维度分组 (multi_index, flat_start, flat_end)
      LeafRow           —   叶子行  (multi_index, dim_indices, row_flat)
    Value               — 统一入口, Measurement | DataArray 联合体
    Block               — 独立变量 + 因变量集合
      BlockCreateInfo   —   添加 Block 时的构造参数 (independents + dependents)
      IndependentSpec   —   自变量描述 (name, DataSeries, DimensionSpec)
      DependentSpec     —   因变量描述 (name, DataSeries)
    DimensionSpec       — 维度规格 (RegularDim / RaggedDim 的变体)
    DataFrame           — Block 的逐行表格视图
    Dataset             — 树形数据集
    Param               — 函数参数描述符 (必需 / 静态默认)
    ComputedParam       — 计算默认值的参数描述符
    register_function() — 注册 Python 可调用对象

调用链:

    Dataset → Block → GetOrCreateDataArray → Value (DataArray-backed)
                                                ├── .data() → DataSeries → np.asarray()
                                                ├── .indep_data("freq") → DataSeries → np.asarray()
                                                ├── .as_data_array() → DataArray → .at() / .select()
                                                ├── .as_data_array() → .groups_at_dim(d) → DimGroup
                                                │       └→ .leaves(start, end) → LeafRow → ds[row_flat]
                                                └── .as_measurement() → Measurement → np.asarray()

    Value.real(3.14) → Value (Measurement-backed)
                        ├── .data() → DataSeries (auto 1-row) → np.asarray()
                        └── .as_measurement() → Measurement → np.asarray()
```

### 5.1 numpy 互操作约定

**所有数值类型统一通过 `np.asarray()` 导出，无类型特定提取器。**

| 方向 | 方式 |
|------|------|
| Measurement → ndarray | `np.asarray(m)` — `__array__` 协议 |
| DataSeries → ndarray | `np.asarray(ds)` — `__array__` 协议, 零拷贝 (real/int) |
| Value/DataArray → ndarray | `np.asarray(v)` — 等价于 `np.asarray(v.data())` (flat) |
| ndarray → DataSeries | `rel.DataSeries.from_array(arr)` — 静态工厂, 拷贝 |

**DataSeries 形态 → `np.asarray()`**:

| DataSeries 形态 | shape | dtype | 拷贝? |
|----------------|-------|-------|:---:|
| scalar real, N 行 | `(N,)` | `float64` | ❌ |
| scalar int, N 行 | `(N,)` | `int32` | ❌ |
| vector real, W, N 行 | `(N, W)` | `float64` | ❌ |
| matrix real, R×C, N 行 | `(N, R, C)` | `float64` | ❌ |
| scalar complex, N 行 | `(N,)` | `complex128` | ✅ |
| scalar boolean, N 行 | `(N,)` | `bool` | ✅ |
| scalar string, N 行 | `TypeError` | — | — |

**Measurement → `np.asarray()`**:

| Measurement | shape | dtype |
|-------------|-------|-------|
| scalar real | `()` (0-d) | `float64` |
| scalar int | `()` (0-d) | `int32` |
| vector real, W | `(W,)` | `float64` |
| matrix real, R×C | `(R, C)` | `float64` |
| string / bool | `TypeError` | — |

> **零拷贝与生命周期**:`np.asarray(ds)` 承诺"零拷贝"时，底层 ndarray 的 `base`
> 必须通过 `py::keep_alive` 持有 `DataSeries` 对象。尤其 `Value.data()` 对
> Measurement-backed 返回的是**临时** 1-row DataSeries（§5.5），一旦 DataSeries
> 被 GC，ndarray 即指向已释放内存。绑定实现必须保证 ndarray 存活期间 DataSeries 存活。

### 5.2 Unit

```python
u1 = rel.Unit.parse("GHz")         # 工厂: REL 词汇表解析
u2 = rel.Unit()                    # 无量纲

u1.multiplier                      # 1e9
u1.has_dimension()                 # True
u1.same_dimension(u2)              # False

u3 = u1 * u2                       # 量纲乘法 → 新 Unit
u4 = u1 / rel.Unit.parse("Hz")     # 量纲除法 → 新 Unit

str(u1)                            # "GHz"
u1 == u2                           # bool
```

**没有 public 构造函数**，只能通过 `parse()` 或 `Unit()`。

### 5.3 Measurement — 值类型

```python
# 标量 (Python 字面量自动推断类型)
rel.Measurement(3.14)                          # double → Real scalar
rel.Measurement(42)                            # int → Integer scalar
rel.Measurement("hello")                       # str → String scalar
rel.Measurement(True)                          # bool → Boolean scalar

# 带单位
rel.Measurement(3.14, unit=rel.Unit.parse("GHz"))

# 向量 / 矩阵 (numpy array)
rel.Measurement(np.array([1.0, 2.0, 3.0]))     # 1-d → Real vector W=3
rel.Measurement(np.array([1, 2, 3]))           # 1-d → Integer vector W=3
rel.Measurement(np.array([[1, 2], [3, 4]]))    # 2-d → Real matrix 2×2

# 便捷工厂 (可带单位)
rel.Measurement.real(3.14, unit=rel.Unit.parse("GHz"))
rel.Measurement.integer(42)
rel.Measurement.string("hello")
rel.Measurement.boolean(True)
```

```python
m.data_kind          # "scalar" | "vector" | "matrix"
m.data_type          # "real" | "integer" | "complex" | "string" | "boolean"
m.unit               # Unit
m.element_count      # int
m.element_at(0)      # → Measurement (第 0 个元素, 保留 unit)
```

**唯一出口 `__array__`**:

```python
arr = np.asarray(m)
float(np.asarray(m))   # scalar → float
int(np.asarray(m))     # scalar → int
str(m)                 # "3.14 GHz" 或 "[1.0, 2.0, 3.0] V"
```

- **`__array__` 统一出口**:不再需要 `scalar_real()` / `vector_int()` 等 8 个提取器。
- **`element_at()` 是唯一不丢 unit 的降维操作**。

### 5.4 DataSeries — 数据列

```python
ds = v.data()                      # 从 Value (Measurement 自动转 1-row)
ds = v.indep_data("freq")          # 自变量
ds = v.indep_data(0)               # 按索引

len(ds)                            # 行数
ds.size                            # 同 len
ds.unit                            # Unit
ds.data_type                       # "real" | "integer" | "complex" | "string" | "boolean"
ds.data_kind                       # "scalar" | "vector" | "matrix"

m = ds[0]                          # → Measurement
m = ds.measurement_at(5)           # → Measurement
for m in ds:                       # Iterator[Measurement]
    val = np.asarray(m).item()

ds.iloc(0, 10)                     # → DataSeries (新对象, 行 0..9)

# DataSeries → ndarray (唯一出口: __array__ 协议)
arr = np.asarray(ds)               # 零拷贝 (real/int), 拷贝 (complex/bool)

# ndarray → DataSeries (唯一入口: 静态工厂)
ds = rel.DataSeries.from_array(arr)
#   1d → scalar, 2d → vector, 3d → matrix
```

> **`from_array` 的 dtype 与单位**:仅接受连续、native 字节序的数值数组
> （`float64`→real、`int32`→integer、`complex128`→complex、`bool`→boolean）；
> 非连续 / 非 native / `object` / `str` dtype 抛 `TypeError`。返回的 DataSeries
> 单位为**无量纲**——numpy 往返会丢失单位，需保留单位时走 `Measurement` / `Value`
> 显式构造。Python `int` 超出 `int32` 范围抛 `OverflowError`。

**不暴露**:`scalar_at<T>()` mutable 引用（用 `set_data` 代替）；`append / resize / fill`
（用工厂批量构造）。

### 5.5 DataArray — 多维数组

```python
da = block.GetOrCreateDataArray("Vout")
da = dataset.GetDataArray("Vout")   # 快捷查找 (名称全局唯一时)

da.data_kind                       # DataArrayKind: DEPENDENT | INDEPENDENT
da.indep_names                     # ["freq", "power"]
da.multi_dimension_spec            # MultiDimensionSpec

ds = da.data                       # → DataSeries (因变量)
ds = da.indep_data(0)              # → DataSeries (第 0 个自变量, flat)
ds = da.indep_data("freq")         # → DataSeries (按名称, flat)
```

**numpy 互操作**:`np.asarray(da)` 返回 flat 视图;`da.ndarray()` 按
`multi_dimension_spec` 展开为多维 ndarray（含 Ragged 维度时抛 `ValueError`）。

| DataArray 维度结构 | `ndarray()` |
|---|---|
| 全 Regular | ✅ 多维 ndarray，shape = 各 dim size × (cell shape) |
| 含 Ragged | ❌ `ValueError` |

**原地修改**:

```python
da.set_data(new_data_series)           # 整体替换 self data
da.set_data(3, measurement)            # 替换 self data 第 3 行

da.set_indep_data(new_series)          # 替换最后一个自变量
da.set_indep_data(0, new_series)       # 按索引替换
da.set_indep_data("freq", new_series)  # 按名称替换

da.set_indep_data(0, 5, measurement)       # 第 0 个自变量, 第 5 行
da.set_indep_data("freq", 5, measurement)  # freq 自变量, 第 5 行
```

**多维索引**:

```python
da.at(freq=0)                        # freq 维度取索引 0
da.at(freq=0, power=3)               # 两个维度都精确
da.select(freq=[0, 1, 2])            # freq 取 3 个值
da.select(freq=[0, 1], power=3)
da.select(power=3)                   # freq 全通, power=3

da.at({"freq": 0})                 # dict 模式 (维度名非合法 identifier)
da.select({"freq": [0, 1, 2], "power": 3})
```

| | `at()` | `select()` |
|---|---|---|
| 单维度精确值 | 维度消失 (rank-1) | 维度保留 (size=1) |
| 对标 numpy | `arr[0, 3]` | `arr[[0,1,2], :]` |

**维度遍历**:

```python
da.rank                            # int — 维度总数
da.flat_size                       # int — 总叶子数
da.group_count_at_dim(0)           # int — 第 0 维的分组数

for group in da.groups_at_dim(0):      # 0 = 最外层
    group.multi_index                  # (0,) — 前缀坐标
    group.flat_start                   # 组内首行 flat 索引 (含)
    group.flat_end                     # 组内末行+1 flat 索引 (不含)
    group.size                         # = flat_end - flat_start

for leaf in da.leaves(group.flat_start, group.flat_end):
    leaf.multi_index               # (0, 0, 0) — 完整 N 维坐标 tuple
    leaf.dim_indices               # (0, 0, 0) — 每维源行号 tuple
    leaf.row_flat                  # flat 索引, 直传 ds[leaf.row_flat]

for leaf in da.leaves(100, 200):   # 迭代 flat 索引 [100, 200)
for leaf in da.all_leaves():       # 等价于 da.leaves(0, da.flat_size)
```

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| `groups_at_dim(d)` | O(组数) | 预计算 CSR 偏移 |
| `leaves(start, end)` | O(叶子数) | 每次 yield 跨 Python/C++ 边界, 允许 GIL 释放 |

**变换**:`da.clone()` 深拷贝。

**不暴露**:`DataArray::CreateIndependent / CreateDependent`（构造路径只有 Block 工厂）;
`min() / max()`（C++ 内置函数可用 Python 端 `reduce_innermost()` 实现）。

### 5.6 Value — 统一入口

```python
v = rel.Value(measurement)
v = rel.Value(data_array)

v = rel.Value.real(3.14)
v = rel.Value.real(3.14, unit=rel.Unit.parse("GHz"))
v = rel.Value.integer(42)
v = rel.Value.string("hello")
v = rel.Value.boolean(True)
v = rel.Value.complex(1+2j)

v = rel.Value.array_real([1.0, 2.0, 3.0])
v = rel.Value.array_integer([1, 2, 3])
v = rel.Value.array_string(["a", "b", "c"])
```

```python
v.is_measurement()                   # bool
v.is_data_array()                    # bool
v.is_scalar() / v.is_vector() / v.is_matrix()
v.is_dependent()                     # DataArray only
v.is_canonicalized()

v.data_kind / v.data_type / v.unit / v.rows / v.indep_names

m  = v.as_measurement()              # → Measurement  (throw if DataArray)
da = v.as_data_array()               # → DataArray    (throw if Measurement)

ds = v.data()                        # → DataSeries
#   Measurement-backed → 临时 1-row DataSeries
#   DataArray-backed   → 直传 da.data()
ds = v.indep_data("freq")
ds = v.indep_data(0)                 # Measurement-backed → RuntimeError
```

> **临时对象生命周期**:`v.data()` 对 Measurement-backed 返回的 1-row DataSeries 是
> 临时对象，`np.asarray(v.data())` 得到的 ndarray 必须用 `py::keep_alive` 持有该
> DataSeries（见 §5.1），否则 DataSeries 被回收后 ndarray 立即悬空。建议先绑定局部
> 变量 `ds = v.data()` 再 `np.asarray(ds)`。

**原地修改**:

```python
# Measurement-backed: 只替换 Measurement; set_indep_data / indep_data 抛 RuntimeError
v.set_data(measurement)
v.set_data(new_data_series)          # 提取第一行 Measurement, 替换 backing
v.set_data(0, some_measurement)      # row 必须为 0

# DataArray-backed: 操作 self data 和 indep data
v.set_data(new_data_series)
v.set_data(3, some_measurement)
v.set_indep_data(new_series)
v.set_indep_data(0, new_series)
v.set_indep_data("freq", new_series)
v.set_indep_data(0, 5, measurement)
v.set_indep_data("freq", 5, measurement)
```

**变换与显示**:`v.canonicalized()` / `v.clone()` / `v.format(name, max_rows)` / `str(v)`。

### 5.7 Block — 数据块

```python
block = dataset.GetBlock("simulation/SP1")

block.name                           # "SP" (短名, 不含父路径)
block.name = "new_name"             # setter

block.dependents()                   # ["Vout", "Iout"]
block.independents()                 # ["freq", "temp"]

spec = block.independent_spec("freq")
spec.name / spec.data / spec.dimension   # DimensionSpec (RegularDim / RaggedDim)

spec = block.dependent_spec("Vout")
spec.name / spec.data

da = block.GetOrCreateDataArray("Vout")  # → DataArray (lazy cached)
df = block.GetOrCreateDataFrame()        # → DataFrame (逐行遍历)
```

### 5.8 Dataset — 数据集树

```python
empty = rel.Dataset()
ds    = rel.Dataset("noise")          # 带名字
ds.name = "new_noise"                # setter
ds.block_count                       # int — 总 Block 数

ds.IsLeaf("simulation/SP1")          # bool
ds.Exists("simulation")              # bool
ds.HasUniqueDataArray("Vout")        # bool

ds.GetBlockNames()                   # ["SP1", "SP2"] — 直接子 Block 名
ds.GetBlockNames("simulation")
ds.GetGroupNames()                   # ["simulation"]
ds.GetGroupNames("simulation")       # ["nested"]
ds.GetAllBlockPaths()                # ["simulation/SP1/SP", ...]

block = ds.GetBlock("simulation/SP1")

# 添加 (程序化构造 Dataset)
block_info = rel.BlockCreateInfo(
    independents=[
        ("freq",  freq_series,  rel.RegularDim(100)),
        ("power", power_series, rel.RegularDim(5)),
    ],
    dependents=[
        ("Vout", vout_series),
        ("Iout", iout_series),
    ],
)
block = ds.AddBlock("simulation/SP1", block_info)

ds.RemoveBlock("simulation/SP1")      # → 0 或 1
ds.RemoveGroup("simulation")          # → 移除的 Block 数

da = ds.GetDataArray("simulation/SP1", "Vout")
da = ds.GetDataArray("Vout")          # 唯一名称快捷访问
ds.GetDataArrayNames("simulation/SP1")  # ["freq", "temp", "Vout"]
```

### 5.9 函数注册

```python
rel.register_function("my_add", [
    rel.Param("a"),
    rel.Param("b", default=rel.Value.integer(10)),
], my_python_fn)

rel.unregister_function("my_add")
rel.function_names()                 # → ["my_add", "sin", ...]
```

**回调签名**:

```python
def my_python_fn(args: dict) -> auto:
    """
    args: dict[str, Value]  — 参数名 → Value
    returns: 自动转换为 Value (见下)
    """
    v = args["a"]
    x = np.asarray(v)
    return x * 2                     # ndarray → 自动转 Value
```

**返回值自动转换**:

| Python 返回 | → Value |
|-------------|---------|
| `rel.Value` | 直通 |
| `rel.Measurement` | `Value(m)` |
| `rel.DataArray` | `Value(da)` |
| `np.ndarray`（1/2/3-d 数值） | 经 `DataSeries.from_array` → `Value` |
| `float` / `np.float64` | `Value::Real(v)` |
| `int` / `np.int32` | `Value::Integer(v)`（超 `int32` 抛 `OverflowError`） |
| `complex` / `np.complex128` | `Value::Complex(v)` |
| `str` | `Value::String(v)` |
| `bool` / `np.bool_` | `Value::Boolean(v)` |
| `list` / `tuple`（数值） | 先 `np.asarray` 再按 ndarray 规则 |
| 其他（`None`、`dict` 等） | `TypeError` |

> **ndarray 往返丢单位**:`np.asarray()` 只导出数值、不带单位。裸返回 ndarray 得到的
> Value 单位为无量纲。需保留单位时显式返回 `rel.Value` / `rel.Measurement`。

### 5.10 Param — 参数描述符

```python
rel.Param("a")                                  # 必需参数
rel.Param("b", default=rel.Value.integer(10))   # 静态默认值
rel.ComputedParam("c", lambda args: ...)        # 计算默认值
```

| 形态 | 工厂 | 省略时 |
|------|------|--------|
| 必需参数 | `rel.Param("a")` | 调用报错 `missing argument 'a'` |
| 静态默认值 | `rel.Param("b", default=...)` | 用固定 `Value` 填充 |
| 计算默认值 | `rel.ComputedParam("c", fn)` | 解析时调用 `fn` 计算填充 |

**ComputedParam 回调签名**（与函数回调一致）:

```python
def default_fn(args: dict) -> auto:
    # args: 已解析的前置参数 (声明顺序在 name 之前)
    return ...
```

**约束**:`fn` 只能读取声明顺序在 `name` **之前**的参数，不能引用自身或后续参数。

```python
def double_a(args):
    x = np.asarray(args["a"])
    return x * 2

rel.register_function("scaled", [
    rel.Param("a"),
    rel.ComputedParam("b", double_a),
], my_python_fn)
# scaled(5) 时, b 解析为 10
```

> **ComputedParam 与 Python 回调**:若 `ComputedParam` 的默认值回调也来自 Python，它
> 同样适用 §4.1 的 shim 模式——`FunctionParam` 里不存 `py::function`，只存名字，由
> shim 在解析时持 GIL 现取。

---

## 6. 错误处理

C++ 异常自动映射:

| C++ 异常 | Python 异常 |
|----------|-------------|
| `boost::bad_get` | `TypeError` |
| `std::invalid_argument` | `ValueError` |
| `std::domain_error` | `ValueError` |
| `std::length_error` | `ValueError` |
| `std::out_of_range` | `IndexError` |
| `std::overflow_error` | `OverflowError` |
| `std::runtime_error` | `RuntimeError` |
| `std::bad_alloc` | `MemoryError` |
| REL 自身错误（`src/core/error.h`） | 需 `py::register_exception` 显式注册，否则落为 `RuntimeError` |

---

## 7. 使用场景

```python
import numpy as np
import rel

# A: 加载 + numpy 处理 + 写回
env = rel.Environment()
env.LoadFromConfig("test_env.json")
dataset = env.DefaultDataset()
da = dataset.GetDataArray("S21")
arr = np.abs(np.asarray(da))
da.set_data(rel.DataSeries.from_array(arr))

# B: 多维索引
point  = da.at(freq=0, power=3)
subset = da.select(freq=list(range(10)))
trace  = da.select(power=0)

# C: 注册 Python 函数
def snr(args):
    signal = np.asarray(args["signal"])
    noise  = np.asarray(args["noise"])
    return 20 * np.log10(signal / noise)

rel.register_function("snr", [
    rel.Param("signal"),
    rel.Param("noise"),
], snr)

# D: 逐行遍历
for m in v.data():
    val = np.asarray(m).item()
    if val > threshold:
        print(f"exceeded: {val} {m.unit}")

# E: Value 统一操作 (不知道 backed 类型)
def inspect(v):
    print(f"rows={v.rows}, kind={v.data_kind}, type={v.data_type}")
    arr = np.asarray(v)
    if v.is_data_array():
        print(f"indep: {v.as_data_array().indep_names}")

# F: 程序化构造 Dataset
freq_vals  = rel.DataSeries.from_array(np.linspace(1e9, 10e9, 100))
power_vals = rel.DataSeries.from_array(np.array([-30, -20, -10, 0, 10]))
vout_data  = rel.DataSeries.from_array(np.random.randn(100, 5))
info = rel.BlockCreateInfo(
    independents=[("freq", freq_vals, rel.RegularDim(100)),
                  ("power", power_vals, rel.RegularDim(5))],
    dependents=[("Vout", vout_data)],
)
ds = rel.Dataset("my_data")
ds.AddBlock("simulation/SP1", info)
```

---

## 8. 开放问题

- **`scoped_interpreter` 隔离**:默认读 site-packages；真正隔离需 `PyConfig_InitIsolatedConfig`（§3.1）。
- **Python 版本耦合**:`BUILD_PYTHON=ON` 时 `rel_runtime.dll` 链接 `pybind11::embed`，须与系统 Python ABI 一致。默认 `BUILD_PYTHON=OFF`，由下游集成方显式开启。
- **外部 `import rel` 不可用**:嵌入模式不产 `.pyd`，无法在独立 Python 进程 `import rel`。V2 可单独产出 `rel.pyd` 共享同一套绑定源码，或经子进程传递序列化数据。
- **多线程**:`Environment::functions_` 已由 `functions_mutex_` 保护（`RegisterFunction` /
  `UnregisterFunction` / `FindFunction` / `FunctionNames` / `CopyFunction` 均持锁）。
  `CallFunction` 与 evaluator 经 `CopyFunction` 在锁内拷贝 `Function`、锁外 `Invoke`，故
  函数实现里再注册其它函数不会死锁。剩余注意点:`FindFunction` 返回的裸指针仅在下次
  注册表变更前有效（存在性检查用），需要长期持有请用 `CopyFunction`；`datasets_` /
  `default_dataset_name_` / `builtin_constants_` 暂未加锁（后者为一次性初始化后只读）。
- **子解释器** (`Py_NewInterpreter`):提供更强模块级隔离，但 pybind11 扩展模块跨子解释器共享受限，当前不推荐，等 Python 3.12+ PEP 684 成熟后再评估。

### V2 候选（不在此文档范围）

- `str(m)` / `bool(m)` 自动生效（string / boolean Measurement）
- `DataArray.transform()` 暴露 Python callback
- `DataSeries` 的 `mean() / max() / std()` 等聚合
- `rel.eval("sin(pi/2)")` 外部 Python 调用 REL 表达式
- `Block` / `Dataset` 写入 HDF5 / Touchstone 等格式
