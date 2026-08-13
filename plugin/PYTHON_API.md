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

### 4.7 维度遍历 — 暴露 `for_each_group_at_dim` / `for_each_leaf_row`

暴露 C++ 的 CSR 遍历原语，让 Python 插件可以实现任意聚合/变换逻辑
（`min`, `max`, `mean`, `std`, 自定义 reduce 等）。

#### 4.7.0 维度信息

```python
da.rank                            # int — 维度总数
da.flat_size                       # int — 总叶子数 (flat cell count)
da.group_count_at_dim(0)           # int — 第 0 维的分组数
```

#### 4.7.1 按维度分组 — `groups_at_dim(dim_idx)`

对标 `MultiDimensionSpec::for_each_group_at_dim(dim_idx, visitor)`。
返回 Python 生成器 `Iterator[DimGroup]`。

```python
for group in da.groups_at_dim(0):      # dim_idx: 0 = 最外层
    group.multi_index                  # (0,) — 前缀坐标, 长度 = dim_idx + 1 = 1
    group.flat_start                   # int — 组内首行 flat 索引 (含)
    group.flat_end                     # int — 组内末行+1 flat 索引 (不含)
    group.size                         # int — 组内叶子数 = flat_end - flat_start
```

| `dim_idx` | 分组粒度 | 例子 (3×4×5) |
|-----------|---------|-------------|
| 0 | 按 dim[0] 分组 | 3 组, 每组 20 个叶子 |
| 1 | 按 dim[0]×dim[1] 前缀分组 | 12 组, 每组 5 个叶子 |
| `rank - 1` | 退化: 每组 1 个叶子 | 60 组, 每 1 组 |

#### 4.7.2 组内逐行迭代 — `leaves(start, end)`

对标 `MultiDimensionSpec::for_each_leaf_row(visitor, start, end)`。
返回 Python 生成器 `Iterator[LeafRow]`。

```python
for group in da.groups_at_dim(1):
    for leaf in da.leaves(group.flat_start, group.flat_end):
        leaf.multi_index               # (0, 0, 0) — 完整 N 维坐标 tuple
        leaf.dim_indices               # (0, 0, 0) — 每维源行号 tuple
        leaf.row_flat                  # int — flat 索引, 直传 ds[leaf.row_flat]
```

也支持直接用范围：

```python
for leaf in da.leaves(100, 200):       # 迭代 flat 索引 [100, 200)
    ...
```

#### 4.7.3 示例
对标 C++ `DataArray::reduce_innermost()`，在 Python 中只需几行：

```python
def reduce_innermost(da, fn):
    """沿最内层维度对每组叶子做 fn 聚合."""
    rank = da.rank
    ds = da.data

    result = []
    for g in da.groups_at_dim(rank - 2):        # 沿最内层分组
        values = []
        for leaf in da.leaves(g.flat_start, g.flat_end):
            m = ds[leaf.row_flat]
            values.append(np.asarray(m).item())  # 0-d → Python scalar
        result.append(fn(values))
    return result

# 使用
min_vals = reduce_innermost(da, min)             # 每组的 min
max_vals = reduce_innermost(da, max)             # 每组的 max
avg_vals = reduce_innermost(da, np.mean)         # 每组的 mean
```
- 遍历全部叶子（不分组）

```python
for leaf in da.all_leaves():                     # 等价于 da.leaves(0, da.flat_size)
    m = da.data[leaf.row_flat]
    if np.asarray(m).item() > threshold:
        print(f"exceeded at {leaf.multi_index}: {m}")
```

#### 4.7.6 性能考量

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| `groups_at_dim(d)` | O(组数) | 预计算 CSR 偏移, 比逐行推导快 |
| `leaves(start, end)` | O(叶子数) | 每次 yield 跨 Python/C++ 边界, 允许 GIL 释放 |
| 大量数据聚合 | — | 建议用 numpy 批处理; 遍历适合千万行以下场景 |

### 4.8 变换与复制

```python
da2 = da.clone()                     # 深拷贝
```

### 4.9 不暴露

- `DataArray::CreateIndependent / CreateDependent` — 构造路径只有 Block 工厂
- `min() / max()` — C++ 内置函数可用 Python 端 `reduce_innermost()` 实现 (见 §4.7.3)

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

# 基本信息
block.name                           # "SP" (短名, 不含父路径)
block.name = "new_name"             # setter

# 变量枚举
block.dependents()                   # ["Vout", "Iout"]  — 因变量名
block.independents()                 # ["freq", "temp"]  — 自变量名

# 变量描述
spec = block.independent_spec("freq")
spec.name                            # "freq"
spec.data                            # DataSeries
dim = spec.dimension                 # DimensionSpec (RegularDim / RaggedDim)

spec = block.dependent_spec("Vout")
spec.name                            # "Vout"
spec.data                            # DataSeries

# 数据获取 (lazy cached)
da = block.GetOrCreateDataArray("Vout")  # → DataArray (组合了所有自变量维度)
df = block.GetOrCreateDataFrame()        # → DataFrame (逐行遍历)
```

---

## 7. Dataset — 数据集树

Dataset 是树形结构: 根是 `InternalNode`, 叶是 `Block`, 通过 `/` 分隔的路径访问。

### 7.1 构造

```python
# 空 Dataset
empty = rel.Dataset()
ds    = rel.Dataset("noise")          # 带名字

# 通常来自文件加载 (Environment.LoadFromConfig)
# Python 侧可通过 AddBlock 程序化构造
```

### 7.2 基本信息

```python
ds.name                              # "noise"
ds.name = "new_noise"               # setter
ds.block_count                       # int — 总 Block 数
```

### 7.3 节点查询

```python
ds.IsLeaf("simulation/SP1")          # bool — 是否为 Block (叶节点)
ds.Exists("simulation")              # bool — 路径是否存在 (任意节点)
ds.HasUniqueDataArray("Vout")        # bool — DataArray 名全局唯一
```

### 7.4 枚举

```python
ds.GetBlockNames()                   # ["SP1", "SP2"]               — 直接子 Block 名
ds.GetBlockNames("simulation")       # ["SP1", "SP2"]               — 指定 group 下的 Block
ds.GetGroupNames()                   # ["simulation"]               — 直接子 group 名
ds.GetGroupNames("simulation")       # ["nested"]                   — 指定 group 下的 group
ds.GetAllBlockPaths()               # ["simulation/SP1/SP", ...]   — 全部 Block 路径 (递归)
```

### 7.5 Block 操作

```python
# 获取
block = ds.GetBlock("simulation/SP1")  # → Block (读写)

# 添加 (程序化构造 Dataset)
block_info = rel.BlockCreateInfo(
    independents=[
        ("freq", freq_series, rel.RegularDim(100)),
        ("power", power_series, rel.RegularDim(5)),
    ],
    dependents=[
        ("Vout", vout_series),
        ("Iout", iout_series),
    ],
)
block = ds.AddBlock("simulation/SP1", block_info)

# 移除
ds.RemoveBlock("simulation/SP1")      # 移除单个 Block → 0 或 1
ds.RemoveGroup("simulation")          # 递归移除整个 group → 移除的 Block 数
```

### 7.6 DataArray 访问

```python
# 完整路径
da = ds.GetDataArray("simulation/SP1", "Vout")
# 唯一名称快捷访问 (当名称在整个 Dataset 中唯一)
da = ds.GetDataArray("Vout")

# 枚举某 Block 内的 DataArray 名
ds.GetDataArrayNames("simulation/SP1")  # ["freq", "temp", "Vout"]
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

### 8.4 内建函数自动暴露

**机制：单一事实源 `Environment::functions_` + PEP 562 懒加载桥接（`__getattr__`）。**

`builtin_library` 与 `math_library` 的全部函数（`sin`、`cos`、`db`、`dbm`、
`what`、`indep`、`output`、`min`/`max`/`sum`/`mean` 等）在 C++ 侧已经注册在
全局静态表 `Environment::functions_` 中。Python 侧**不逐个绑定**，而是：

1. `rel` 模块定义 `__getattr__(name)`（PEP 562，Python 3.7+）。
2. 首次访问 `rel.sin` 时普通属性查找失败 → 触发 `__getattr__("sin")`
   → `Environment::FindFunction("sin")`。
3. 命中则把 `Function` 包成 Python 可调用对象并**缓存**，后续直接命中缓存。
4. 未命中（既非函数也非常量）→ 抛 `AttributeError`。

**调用约定**（与 REL 一致）：位置参数按声明顺序映射到参数名，关键字参数按名
映射，省略的参数由 `Function::Invoke` 用默认值/计算默认值补齐。

```python
rel.sin(x)                       # → rel.Value (与 REL 中 sin(x) 一致)
rel.db(r, 50, 75)                # 位置参数
rel.db(r=r, z1=50, z2=75)        # 关键字参数
rel.db(r)                        # z1/z2 用默认值 50 Ohm 补齐
rel.what(da)                     # builtin 库
rel.indep(da, "freq")            # 默认参数 selector=Integer(1)
```

**入参 / 返回值转换**：入参按 §8.3 的对称规则自动转 `Value`（`rel.Value` /
`rel.Measurement` / `rel.DataArray` 直通；`float→Real`、`int→Integer`、
`str→String`、`bool→Boolean`）；返回值是 `rel.Value`，与 `register_function`
回调一致，可继续 `np.asarray(...)` / `.data()` 等。

**为什么是"自然自动注册"**：

| 要点 | 说明 |
|------|------|
| 单一事实源 | `Function` 已含 `name` + `params()` + `impl()`，`Invoke(ArgMap)` 是通用入口；桥接只做"收集显式参数 → 调 `Invoke`" |
| 懒加载 | `__getattr__` 在访问时解析，**后注册的函数也能被发现**——内建（启动时 `InitBuiltinFunctions()`）、C++ 插件（`LoadFunctionPlugin`）、Python `register_function` 全部适用，无需任何额外注册步骤 |
| 无命名冲突 | `__getattr__` 仅在普通属性查找失败时触发，`rel.Value` / `rel.Param` / `rel.register_function` 等真实属性优先 |
| 生命周期安全 | 内建函数常驻 `rel_runtime.dll`，包装时复制 `Function`（含 `std::function` 闭包）不悬挂 |

**实时性语义**（`__getattr__` 只在属性查找**失败**时触发，缓存只覆盖**已访问**过的名字）：

| 情形 | 是否实时 | 说明 |
|------|:---:|------|
| 注册**全新**函数名 | ✅ | 首次访问必"未命中" → `__getattr__` → `FindFunction`，任意时刻注册都能发现 |
| **覆盖**已缓存函数（同名重注册新签名） | ⚠️ 陈旧 | 命中缓存，不再走注册表；热更新需显式失效缓存 |
| **注销**已缓存函数 | ⚠️ 陈旧 | 缓存副本仍可调用（且可能悬挂，见下） |

#### 8.4.1 Stale（陈旧）的根因：Python 属性查找顺序

`__getattr__` 仅在普通属性查找**失败**时触发。一旦把 callable 写进模块 `m`：

```python
rel.sin      # 第 1 次：m 里没有 "sin" → __getattr__ → 包装 v1 → m.attr("sin") = v1
rel.sin      # 第 2 次：m.__dict__ 直接命中 v1，__getattr__ 不再触发
```

此后 C++ 侧 `RegisterFunction(新 sin)` 覆盖 map 里的 `Function`，但 Python 的
`m.__dict__["sin"]` 仍是旧 `v1` —— **Python 对象字典 ≠ C++ 注册表**，二者从此分叉。

#### 8.4.2 Dangling（悬挂）的根因：`std::function` 复制语义

`std::function` 被复制时复制的是**可调用对象（含捕获数据）**，但函数体机器码
不在 `std::function` 里，而在编译出那个 lambda 的 DLL/EXE 的 `.text` 段。

```mermaid
flowchart LR
    subgraph map["Environment::functions_"]
        F["\"plugin_fn\" → Function 副本 #1"]
    end
    subgraph cache["Python 缓存"]
        C["Function 副本 #2"]
    end
    subgraph dll["plugin.dll (.text)"]
        CODE["lambda 机器码"]
    end
    F -.->|"impl_ 代码指针"| CODE
    C -.->|"复制后仍指向同一段代码"| CODE
```

复制 `Function` 只复制数据、不复制代码，所以每份副本的 `std::function` 调用的
都是插件 DLL 里的同一段机器码。精确悬挂序列：

```cpp
// 1. Python 首次访问，__getattr__ 缓存 Function 副本 #2（impl_ 代码指针 → plugin.dll）

// 2. 卸载插件
UnloadFunctionPlugin(plugin);
//    → UnregisterFunction: map 里副本 #1 被 erase
//    → FreeLibrary:       plugin.dll 代码段移出进程地址空间

// 3. 再调用 → 副本 #2 跳转到已卸载地址 → 段错误/UB
rel.plugin_fn(...)
```

`Environment` 原本靠"先 erase、后 FreeLibrary"的顺序保证安全（卸载时 map 里
已无指向插件代码的 `Function`）；Python 缓存的副本 #2 是**第三份拷贝**，绕开了保护。

#### 8.4.3 解法取舍

| 方案 | 核心 | stale | dangling | 代价 |
|------|------|:---:|:---:|------|
| **1. 只缓存内建** | 插件函数每次 `FindFunction` 现取，不复制缓存 | ✅ | ✅ | 插件函数每次多一次 map 查找；`is` 身份不保证 |
| **2. 副本持住 `LoadedPlugin`** | 缓存副本同时持插件引用，DLL 不卸载 | ❌ 仍需失效缓存 | ✅ | `LoadedPlugin` 改引用计数；仍要单独处理覆盖 |

**推荐方案 1**：用"不缓存"同时消掉 stale 与 dangling —— `FindFunction` 永远返回
注册表**当前**状态，覆盖自然拿到最新；卸载后返回 `nullptr` 自然抛 `AttributeError`
而非调野指针。代价仅一次 map 查找 + 函数 `is` 身份不保证（对函数通常无关紧要）。

实现要点：`Environment::InitBuiltinFunctions()` 时把内建函数名记入
`builtin_function_names_` 集合；`__getattr__` 仅当名字命中该集合才缓存，其余
（C++ 插件 / `register_function`）走 `FindFunction` 现取——每次包装新 callable，
或包装一个"每次现查 `FindFunction`"的转发代理。

**实现位置**（`src/runtime/python/`）：

| 文件 | 内容 |
|------|------|
| `rel_bindings.cc` | `Function` → Python callable 桥接类（`__call__(*args, **kwargs)`：参数名映射 + 缺省补全 + `Invoke`） |
| `rel_module.cc` | `rel.__getattr__(name)`：`FindFunction` → 包装 + 缓存；顺带 `FindConstant`（`rel.PI` / `rel.e` / `rel.c0` / `rel.i` 等） |

**示例**：

```python
import numpy as np
import rel

x = rel.Value.real(0.5)
y = rel.sin(x)                    # 懒加载 → Environment::FindFunction("sin")
print(np.asarray(y))              # 0.4794...

da = dataset.GetDataArray("S21")
rel.what(da)                      # 与 REL 中的 what(da) 完全一致
rel.db(da, 50, 50)                # db(r, z1=50, z2=50)
```

**与 §8.1 `register_function` 的关系**：`register_function` 是把 Python 可调用
对象**写入**注册表（Python→C++）；`__getattr__` 是把注册表里的 C++ `Function`
**读出**为 Python 可调用对象（C++→Python）。二者共用同一 `Function`/`Value`
机制，互为镜像。

**内建常量（可选扩展）**：同一 `__getattr__` 依次查找 函数 → 常量，即可让
`rel.PI`、`rel.c0`、`rel.i` 等自然可用。

**插件互调（跨插件函数调用）**：`__getattr__` 桥接只解决了 **Python 侧**调用任意
函数的能力；**DLL（C++）插件**调用其他函数还需 `RelPluginApi` 增加 `find_function`
回调 + `Function::Invoke` inline 化（见 `BUILD.md` §6）。二者合流后四向互调全部
打通：内建 / DLL / Python 两两可调，最终都落到 `Environment::FindFunction` +
`Function::Invoke`，与 REL 表达式求值同一条路径。

---

## 9. Param — 函数参数描述符

### 9.1 三种参数形态

```python
# 必需参数
rel.Param("a")

# 带静态默认值
rel.Param("b", default=rel.Value.integer(10))

# 带计算默认值 (ComputedParam)
rel.ComputedParam("c", lambda args: rel.Value.real(...))
```

| 形态 | 工厂 | 省略时 |
|------|------|--------|
| 必需参数 | `rel.Param("a")` | 调用报错 `missing argument 'a'` |
| 静态默认值 | `rel.Param("b", default=...)` | 用固定 `Value` 填充 |
| 计算默认值 | `rel.ComputedParam("c", fn)` | 解析时调用 `fn` 计算填充 |

### 9.2 ComputedParam — 计算默认值

对标 C++ `ComputedParam(name, fn)`。默认值不是静态常量，而是在调用解析时
从**已解析的前置参数**动态计算得出（`Function::Invoke` 按声明顺序解析参数，
计算默认值只能引用位置在它之前的参数）。

```python
rel.ComputedParam("name", fn)
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `name` | `str` | 参数名 |
| `fn` | `callable` | 计算默认值的回调 |

回调签名（与 §8.2 相同，返回值转换规则见 §8.3）：

```python
def default_fn(args: dict) -> auto:
    """
    args: dict[str, Value] — 已解析的前置参数 (声明顺序在 name 之前)
    returns: 自动转换为 Value (见 8.3)
    """
    return ...
```

**约束**：`fn` 只能读取声明顺序在 `name` **之前**的参数，不能引用自身或后续参数。

示例：

```python
def double_a(args):
    x = np.asarray(args["a"])        # → ndarray
    return x * 2                     # ndarray → 自动转 Value (见 §8.3)

rel.register_function("scaled", [
    rel.Param("a"),
    rel.ComputedParam("b", double_a),
], my_python_fn)

# 调用 scaled(5) 时，b 解析为 10
```

---

## 10. 使用场景

### 场景 A：加载数据 + numpy 处理 + 写回

```python
import numpy as np
import rel

# 加载 (从 JSON 配置文件)
env = rel.Environment()
env.LoadFromConfig("test_env.json")
dataset = env.DefaultDataset()
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

### 场景 F：程序化构造 Dataset

```python
import numpy as np
import rel

# 从 numpy array 构建 DataSeries
freq_vals   = rel.DataSeries.from_array(np.linspace(1e9, 10e9, 100))
power_vals  = rel.DataSeries.from_array(np.array([-30, -20, -10, 0, 10]))
vout_data   = rel.DataSeries.from_array(np.random.randn(100, 5))

# 构造 Block
info = rel.BlockCreateInfo(
    independents=[
        ("freq",  freq_vals,  rel.RegularDim(100)),
        ("power", power_vals, rel.RegularDim(5)),
    ],
    dependents=[
        ("Vout", vout_data),
    ],
)

ds = rel.Dataset("my_data")
ds.AddBlock("simulation/SP1", info)

# 验证
da = ds.GetDataArray("simulation/SP1", "Vout")
print(da.rank)     # 2
print(da.flat_size) # 500
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
- `DataSeries` 的 `mean() / max() / std()` 等聚合（Python 端已可通过遍历实现）
- `rel.eval("sin(pi/2)")` 外部 Python 调用 REL 表达式
- `Block` / `Dataset` 写入 HDF5 / Touchstone 等格式
