# REL Python API — Interface Design

> **Status**: Draft  
> **Date**: 2026-08-10  
> **Scope**: `rel_runtime` + `xdataset` 的完整 Python 接口

---

## 类型全景

```
用户可见类型（都在 `import rel` 下）:

    Unit                — 物理单位
    Measurement         — 带单位的标量 / 向量 / 矩阵（值类型）
    DataSeries          — 一列 Measurement, numpy 互操作
    DataArray           — 多维数组 + 坐标轴
    Value               — 统一入口, Measurement | DataArray 联合体
    Block               — 独立变量 + 因变量集合
    Dataset             — 树形数据集
    Param               — 函数参数描述符
    register_function() — 注册 Python 可调用对象

调用链:

    Dataset → Block → GetOrCreateDataArray → Value (DataArray-backed)
                                                ├── .data() → DataSeries → np.asarray()
                                                ├── .indep_data("freq") → DataSeries → np.asarray()
                                                ├── .as_data_array() → DataArray → .at() / .select()
                                                └── .as_measurement() → Measurement → np.asarray()

    Value.real(3.14) → Value (Measurement-backed)
                        ├── .data() → DataSeries (auto 1-row) → np.asarray()
                        └── .as_measurement() → Measurement → np.asarray()
```

---

## 0. numpy 互操作约定

**所有数值类型统一通过 `np.asarray()` 导出，无类型特定提取器。**

| 方向 | 方式 |
|------|------|
| Measurement → ndarray | `np.asarray(m)` — `__array__` 协议 |
| DataSeries → ndarray | `np.asarray(ds)` — `__array__` 协议, 零拷贝 (real/int) |
| Value/DataArray → ndarray | `np.asarray(v)` — 等价于 `np.asarray(v.data())` (flat) |
| ndarray → DataSeries | `rel.DataSeries.from_array(arr)` — 静态工厂, 拷贝 |

### `np.asarray()` — flat 视图

**DataSeries:**

| DataSeries 形态 | `np.asarray().shape` | dtype | 拷贝? |
|----------------|---------------------|-------|:---:|
| scalar real, N 行 | `(N,)` | `float64` | ❌ |
| scalar int, N 行 | `(N,)` | `int32` | ❌ |
| vector real, W, N 行 | `(N, W)` | `float64` | ❌ |
| matrix real, R×C, N 行 | `(N, R, C)` | `float64` | ❌ |
| scalar complex, N 行 | `(N,)` | `complex128` | ✅ |
| scalar boolean, N 行 | `(N,)` | `bool` | ✅ |
| scalar string, N 行 | `TypeError` | — | — |

**Measurement:**

| Measurement | `np.asarray().shape` | dtype |
|-------------|---------------------|-------|
| scalar real | `()` (0-d) | `float64` |
| scalar int | `()` (0-d) | `int32` |
| vector real, W | `(W,)` | `float64` |
| matrix real, R×C | `(R, C)` | `float64` |
| string / bool | `TypeError` | — |

---

## 1. Unit — 物理单位

```
C++: Unit { multiplier_ + UnitData (7 SI exponents) }
```

### API

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

---

## 2. Measurement — 值类型

```
C++: Measurement { Unit + DataKind + DataType + boost::variant<12 types> }
```

Measurement 是值类型，通常由 `DataSeries[i]` 获得或作为函数返回值构造。
数值通过 `np.asarray()` 统一导出。

### 2.1 构造

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

### 2.2 查询

```python
m.data_kind          # "scalar" | "vector" | "matrix"
m.data_type          # "real" | "integer" | "complex" | "string" | "boolean"
m.unit               # Unit
m.element_count      # int
```

### 2.3 值提取

**唯一出口：`__array__` 协议。**

```python
arr = np.asarray(m)
#   scalar real    → ndarray(shape=(),         dtype=float64)
#   scalar int     → ndarray(shape=(),         dtype=int32)
#   vector real W  → ndarray(shape=(W,),       dtype=float64)
#   matrix real R×C → ndarray(shape=(R, C),    dtype=float64)
#   string / bool  → TypeError (用 str(m)/bool(m))

# 自然 Python 互操作
float(np.asarray(m))   # scalar → float
int(np.asarray(m))     # scalar → int
```

### 2.4 逐元素

```python
m.element_at(0)      # → Measurement (第 0 个元素, 保留 unit)
```

### 2.5 显示

```python
str(m)               # "3.14 GHz" 或 "[1.0, 2.0, 3.0] V"
repr(m)              # 同上
m.to_string()        # 同上
```

### 2.6 设计要点

- **`__array__` 统一出口**：不再需要 `scalar_real()` / `vector_int()` 等 8 个提取器
- **`data_type` 返回字符串**：用户可判断类型但不需要类型化提取
- **`element_at()` 是唯一不丢 unit 的降维操作**

---

## 3. DataSeries — 数据列

```
C++: DataSeries { T* 连续内存, N 行, DataKind/DataType/Unit }
```

DataSeries 是值的关键中转站：`Value.data()` / `Value.indep_data()` 返回它，
`np.asarray()` 从这里零拷贝导出。

### 3.1 获取

```python
ds = v.data()                      # 从 Value (Measurement 自动转 1-row)
ds = v.indep_data("freq")          # 自变量
ds = v.indep_data(0)               # 按索引
```

### 3.2 元数据

```python
len(ds)                            # 行数
ds.size                            # 同 len
ds.unit                            # Unit
ds.data_type                       # "real" | "integer" | "complex" | "string" | "boolean"
ds.data_kind                       # "scalar" | "vector" | "matrix"
```

### 3.3 逐行访问

```python
m = ds[0]                          # → Measurement
m = ds.measurement_at(5)           # → Measurement
```

### 3.4 迭代

```python
for m in ds:                       # Iterator[Measurement]
    val = np.asarray(m).item()     # 0-d → Python scalar
    print(val)
```

### 3.5 切片

```python
ds.iloc(0, 10)                     # → DataSeries (新对象, 行 0..9)
```

### 3.6 numpy 互操作

```python
# DataSeries → ndarray (唯一出口: __array__ 协议)
arr = np.asarray(ds)               # 零拷贝 (real/int), 拷贝 (complex/bool)

# ndarray → DataSeries (唯一入口: 静态工厂)
ds = rel.DataSeries.from_array(arr)
#   1d → scalar, 2d → vector, 3d → matrix
```

### 3.7 不暴露

- `scalar_at<T>()` mutable 引用 — 用 `set_data` 代替
- `append / resize / fill` — 用工厂批量构造

---

## 4. DataArray — 多维数组

```
C++: DataArray { DataSeriesMap datas_ + MultiDimensionSpec + DataArrayKind }
```

Python 端只读获取，原地修改走 `set_data` / `set_indep_data`。

### 4.1 获取

```python
# 唯一入口: Dataset → Block → GetOrCreateDataArray
da = block.GetOrCreateDataArray("Vout")

# 快捷查找 (当名称在整个 Dataset 中唯一)
da = dataset.GetDataArray("Vout")
```

### 4.2 元数据

```python
da.data_kind                       # DataArrayKind: DEPENDENT | INDEPENDENT
da.indep_names                     # ["freq", "power"]
da.multi_dimension_spec            # MultiDimensionSpec
```

### 4.3 数据访问

```python
ds = da.data                       # → DataSeries (因变量)
ds = da.indep_data(0)              # → DataSeries (第 0 个自变量, flat)
ds = da.indep_data("freq")         # → DataSeries (按名称, flat)
```

### 4.4 numpy 互操作

`np.asarray(da)` 返回 flat 视图（见 §0 全局约定）。DataArray 额外提供多维展开：

```python
# Regular(100) × Regular(5), scalar real
da.ndarray()  # → ndarray(shape=(100, 5), dtype=float64)

# Regular(100) × Regular(5), vector real width=3
da.ndarray()  # → ndarray(shape=(100, 5, 3), dtype=float64)

# 含 Ragged 维度时抛出异常
da.ndarray()  # → ValueError: "ragged DataArray, use np.asarray() for flat view"
```

`ndarray()` 按 `multi_dimension_spec` 将 flat 数据展开为多维 ndarray。
与 `np.asarray().reshape()` 的区别：Ragged DataArray 明确拒绝而非产生错误形状。

| DataArray 维度结构 | `ndarray()` | 说明 |
|---|---|---|
| 全 Regular | ✅ 多维 ndarray | shape = 各 dim size × (cell shape) |
| 含 Ragged | ❌ `ValueError` | 各组长度不同，无法构建规则 ndarray |

### 4.5 原地修改

```python
# --- 替换自身数据 ---
da.set_data(new_data_series)           # 整体替换 self data
da.set_data(3, measurement)            # 替换 self data 第 3 行

# --- 替换自变量 ---
da.set_indep_data(new_series)          # 替换最后一个自变量
da.set_indep_data(0, new_series)       # 按索引替换
da.set_indep_data("freq", new_series)  # 按名称替换

# --- 替换自变量中的单个值 ---
da.set_indep_data(0, 5, measurement)       # 第 0 个自变量, 第 5 行
da.set_indep_data("freq", 5, measurement)  # freq 自变量, 第 5 行
```

### 4.6 多维索引

通过 kwargs 自动构建 `MultiIndexSelector`, key 名来自 `indep_names()`。

当维度名称不是合法 Python identifier（如包含空格、数字开头等），改用 dict 模式：

```python
# kwargs 模式 (推荐, 大多数情况)
da.at(freq=0)                        # freq 维度取索引 0
da.at(freq=0, power=3)               # 两个维度都精确

da.select(freq=[0, 1, 2])            # freq 取 3 个值
da.select(freq=[0, 1], power=3)      # freq 多个, power 精确

# 通配: 不传 = 全部
da.select(power=3)                    # freq 全通, power=3

# dict 模式
da.at({"freq": 0})                 # S-parameter 维度
da.select({"freq": [0, 1, 2], "power": 3})
```

| | `at()` | `select()` |
|---|---|---|
| 单维度 精确值 | 维度消失 (rank-1) | 维度保留 (size=1) |
| 对标 numpy | `arr[0, 3]` | `arr[[0,1,2], :]` |

**实现**：`at()` / `select()` 同时接受 `**kwargs` 或单个 `dict` 参数。

### 4.7 变换与复制

```python
da2 = da.clone()                     # 深拷贝
```

### 4.8 不暴露

- `DataArray::CreateIndependent / CreateDependent` — 构造路径只有文件加载
- `min() / max()` — V2 (已在 C++ builtin library 中作为 REL 函数暴露)

---

## 5. Value — 统一入口

```
C++: Value { boost::variant<Measurement, shared_ptr<DataArray>> }
```

Value 是 Python 用户的主要交互类型。所有静态工厂都在 Value 上, Measurement 托底。

### 5.1 构造

```python
# 包装已有对象
v = rel.Value(measurement)
v = rel.Value(data_array)

# 便捷工厂 — Measurement-backed
v = rel.Value.real(3.14)
v = rel.Value.real(3.14, unit=rel.Unit.parse("GHz"))
v = rel.Value.integer(42)
v = rel.Value.string("hello")
v = rel.Value.boolean(True)
v = rel.Value.complex(1+2j)

# 便捷工厂 — DataArray-backed
v = rel.Value.array_real([1.0, 2.0, 3.0])
v = rel.Value.array_integer([1, 2, 3])
v = rel.Value.array_string(["a", "b", "c"])
```

### 5.2 类型查询

```python
v.is_measurement()                   # bool
v.is_data_array()                    # bool
v.is_scalar()                        # bool (Measurement + DataArray 通用)
v.is_vector()                        # bool
v.is_matrix()                        # bool
v.is_dependent()                     # bool (DataArray only, Measurement→false)
v.is_canonicalized()                # bool
```

### 5.3 元数据 (统一层, 无需下沉)

```python
v.data_kind                          # "scalar" | "vector" | "matrix"
v.data_type                          # "real" | "integer" | ...
v.unit                               # Unit
v.rows                               # int (Measurement=1)
v.indep_names                        # [str] (Measurement→[])
```

### 5.4 下沉到具体类型

```python
m  = v.as_measurement()              # → Measurement  (throw if DataArray)
da = v.as_data_array()               # → DataArray    (throw if Measurement)
```

下沉后可用 Measurement/DataArray 的全部 API。

### 5.5 数据访问 (统一返回 DataSeries)

```python
ds = v.data()                        # → DataSeries
#   Measurement-backed → 临时 1-row DataSeries (从 Measurement 构建, 非转换)
#   DataArray-backed   → 直传 da.data()

ds = v.indep_data("freq")            # → DataSeries
ds = v.indep_data(0)                 # Measurement-backed → RuntimeError
```

### 5.6 numpy 互操作

`np.asarray(v)` 返回 flat 视图（见 §0 全局约定）。`v.ndarray()` 委托给底层 DataArray
（DataArray-backed 时有效，Measurement-backed 抛 `TypeError`）。

### 5.7 原地修改

**Measurement-backed：只替换 Measurement。`set_indep_data` / `indep_data` 抛 RuntimeError。**

```python
v.set_data(measurement)              # 替换为新 Measurement
v.set_data(new_data_series)          # 提取第一行 Measurement, 替换 backing
v.set_data(0, some_measurement)      # row 必须为 0 (Measurement 只有一行)
```

**DataArray-backed：操作 self data 和 indep data。**

```python
v.set_data(new_data_series)          # 替换 self data
v.set_data(3, some_measurement)      # 替换第 3 行 self data

v.set_indep_data(new_series)         # 替换最后一个自变量
v.set_indep_data(0, new_series)      # 按索引
v.set_indep_data("freq", new_series) # 按名称
v.set_indep_data(0, 5, measurement)  # 自变量第 5 行
v.set_indep_data("freq", 5, measurement)
```

### 5.8 变换与显示

```python
v2 = v.canonicalized()               # 归一化 (吸收 multiplier)
v2 = v.clone()                       # 深拷贝
v.format()                           # DataFrame 格式字符串
v.format("my_var", max_rows=10)      # 带名字 + 行数限制
str(v)                               # v.format()
```

---

## 6. Block — 数据块

```python
block = dataset.GetBlock("simulation/SP1")

block.name                           # "SP"
block.dependents()                   # ["Vout", "Iout"]
block.independents()                 # ["freq", "temp"]

da = block.GetOrCreateDataArray("Vout")  # → DataArray
```

---

## 7. Dataset — 数据集树

```python
ds = rel.Dataset("noise")            # 构造空 Dataset (通常来自文件加载)

ds.name                              # "noise"
ds.name = "new_noise"               # setter

ds.GetBlockNames()                   # ["simulation/SP1", "simulation/SP2"]
ds.GetBlockNames("simulation")       # ["SP1", "SP2"]

ds.GetDataArrayNames("simulation/SP1")  # ["freq", "temp", "Vout"]

# 完整路径
da = ds.GetDataArray("simulation/SP1", "Vout")
# 唯一名称快捷访问
da = ds.GetDataArray("Vout")

ds.HasBlock("simulation/SP1")        # bool
ds.HasUniqueDataArray("Vout")        # bool
```

---

## 8. 函数注册

### 8.1 API

```python
# 注册
rel.register_function("my_add", [
    rel.Param("a"),
    rel.Param("b", default=rel.Value.integer(10)),
], my_python_fn)

# 查询与注销
rel.unregister_function("my_add")
rel.function_names()                 # → ["my_add", "sin", ...]
```

### 8.2 回调签名

```python
def my_python_fn(args: dict) -> auto:
    """
    args: dict[str, Value]  — 参数名 → Value
    returns: 自动转换为 Value (见 8.3)
    """
    v = args["a"]                    # type: Value
    x = np.asarray(v)                # → ndarray
    return x * 2                     # ndarray → 自动转 Value
```

### 8.3 返回值自动转换

| Python 返回 | → Value |
|-------------|---------|
| `rel.Value` | 直通 |
| `rel.Measurement` | `Value(m)` |
| `rel.DataArray` | `Value(da)` |
| `float` | `Value::Real(v)` |
| `int` | `Value::Integer(v)` |
| `str` | `Value::String(v)` |
| `bool` | `Value::Boolean(v)` |

用户写 `return a + b` 即可, 不必 `return rel.Value.real(a + b)`。

---

## 9. Param — 函数参数描述符

```python
rel.Param("a")                       # 必需参数
rel.Param("b", default=rel.Value.integer(10))  # 带默认值
```

---

## 10. 使用场景

### 场景 A：加载数据 + numpy 处理 + 写回

```python
import numpy as np
import rel

# 加载
dataset = load_dataset_somehow()     # 环境加载
da = dataset.GetDataArray("S21")

# numpy 处理
arr = np.asarray(da)                 # flat
processed = np.abs(arr)

# 写回
new_ds = rel.DataSeries.from_array(processed)
da.set_data(new_ds)
```

### 场景 B：多维索引

```python
da = dataset.GetDataArray("Vout")    # Regular(100)×Regular(5)

# 取一个点
point = da.at(freq=0, power=3)

# 取 freq 前 10 个值的所有 power
subset = da.select(freq=list(range(10)))

# 通配: 取 power=0 的所有 freq
trace = da.select(power=0)
```

### 场景 C：注册 Python 函数

```python
import rel

def snr(args):
    signal = np.asarray(args["signal"])     # → ndarray
    noise  = np.asarray(args["noise"])      # → ndarray
    return 20 * np.log10(signal / noise)

rel.register_function("snr", [
    rel.Param("signal"),
    rel.Param("noise"),
], snr)
```

### 场景 D：逐行遍历

```python
for m in v.data():
    val = np.asarray(m).item()       # 0-d array → Python scalar
    if val > threshold:
        print(f"exceeded: {val} {m.unit}")
```

### 场景 E：Value 统一操作 (不知道 backed 类型)

```python
def inspect(v):
    """无论 Measurement 还是 DataArray, 统一处理"""
    print(f"rows={v.rows}, kind={v.data_kind}, type={v.data_type}")
    arr = np.asarray(v)
    print(f"mean={np.mean(arr)}")

    if v.is_data_array():
        da = v.as_data_array()
        print(f"indep: {da.indep_names}")
```

---

## 11. 错误处理

C++ 异常自动映射：

| C++ 异常 | Python 异常 |
|----------|-------------|
| `boost::bad_get` | `TypeError` |
| `std::invalid_argument` | `ValueError` |
| `std::runtime_error` | `RuntimeError` |
| `std::out_of_range` | `IndexError` |
| `std::bad_alloc` | `MemoryError` |

---

## 12. V2 候选 (不在此文档范围)

- `str(m)` / `bool(m)` 自动生效 (string / boolean Measurement)
- `DataArray.transform()` 暴露 Python callback
- `DataSeries` 的 `mean() / max() / std()` 等聚合
- `rel.eval("sin(pi/2)")` 外部 Python 调用 REL 表达式
- `Block` / `Dataset` 的添加和写入
