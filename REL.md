# REL
REL(ResultsView Expression Language/后处理表达式语言) 是一种对仿真结果进行计算的语言，主要用于后处理表达式的计算。

主要特点为：
1. REL 是一种专用于基于多维仿真数据的结果计算表达式的DSL(Domain-Specific Language)
2. REL 操作的数据和表达式计算的结果统一用 **`Value`** 表达，底层为两种形态：**DataArray**（多维仿真数据）和 **Measurement**（单行数据），详见[数据类型](#数据类型)
3. REL 兼容Keysight ADS AEL Mesaure Expression的语法
4. REL 支持使用Python拓展更多的函数

> **文档定位**：本文档是 REL 语言本身的完整参考，同时面向**用户阅读**与 **AI agent 编写 REL 表达式**两种用途。
> 编写表达式时请特别注意：
> - 内建函数的完整签名与行为见[内建函数参考](#内建函数参考)，参数支持缺省（见[函数](#函数)）；
> - `[]` sweep 索引是 **0-based**、`()` 矩阵索引是 **1-based**（见[索引](#索引)），二者方向相反，勿混用；
> - 数值字面量可携带物理单位，运算时自动推导结果单位（见[数值字面量的缩放因子与物理单位](#数值字面量的缩放因子与物理单位)）；
> - 逻辑与比较运算的结果类型是 Boolean（`TRUE`/`FALSE`），可直接参与 `if` 条件与逻辑运算。

## 数据类型

REL 中的值统一用 **`Value`** 表达。`Value` 是求值与运算的统一载体：表达式、函数、运算符都以 `Value` 为输入输出，并共享同一套统一的元数据接口（类型、形状、单位、行数、坐标轴信息等），因此标量与多维数据可以无差别地混合参与运算。

`Value` 底层只有两种存储形态：

- **Measurement** — 单行数据。携带一个标量 / 向量 / 矩阵值和一个物理单位，不含坐标信息。Scalar 字面量、字符串、算术中间结果均以 Measurement 形态存储。Measurement 可视为 DataArray 中的一行，也可在纯值运算中独立出现。
- **DataArray** — 多维仿真数据结构。包含自身数据（若干行）和关联的坐标轴信息，区分两种角色：坐标轴变量（记录扫描点）和观测变量（挂载在某组坐标轴上的仿真结果）。DataArray 可以是一个仿真变量、一个 sweep 生成结果，或一次算术运算的产物。

`Value` 提供统一的接口，对两种底层形态一视同仁：

| 统一接口 | 含义 |
|---|---|
| `data_kind()` | 形状类别：`Scalar` / `Vector` / `Matrix` |
| `data_type()` | 元素类型：`Integer` / `Real` / `Complex` / `String` / `Boolean` |
| `data_shape()` | 具体尺寸 |
| `unit()` | 物理单位 |
| `rows()` | 行数（Measurement 恒为 1；DataArray 为其行数） |
| `is_scalar()` / `is_vector()` / `is_matrix()` | 形状判断 |
| `indep_names()` / `is_dependent()` / `dimension_spec()` | 坐标轴信息（Measurement 无坐标轴，返回空/单维） |
| `data()` / `indep()` | 自身数据 / 提取坐标轴 |

> 在本文档及宿主环境中，“值”即 `Value`。仅当需要区分“单行”与“多维”两种底层形态时，才使用 Measurement 与 DataArray 这两个术语。

两种底层形态共享同一套标量类型体系：

| 数据类型 | 说明 | 示例 |
|---|---|---|
| `Integer` | 整数 | `13`、`0x3E`、`0377` |
| `Real` | 双精度浮点数 | `10.3`、`25.4e-3` |
| `Complex` | 复数 | `3.5i`、`2i` |
| `String` | 字符串 | `"a string"`、`''\usr\local im.abc ''` |
| `Boolean` | 逻辑真/假 | `TRUE`、`FALSE` |

表达式求值结果是一个 `Value`：纯值运算（如 `3 + 4`、`{1,2,3}`）得到 Measurement 形态，生成器或变量引用（如 `[1,2,3]`、带坐标的变量）得到 DataArray 形态。两种形态在算术、比较、逻辑、索引等操作中均可混合参与，由统一的 `Value` 接口完成逐元素计算与行/形状广播。

一个 REL 输入由单个表达式构成：
```REL
REL expression
```

在宿主环境中，可以使用“标识符 = 表达式”的形式保存计算结果，例如：

```text
result_name = REL expression
```

该绑定形式属于宿主环境能力，不属于 REL 语法本身。绑定后的名称作为变量参与后续表达式求值（与内建常量重名的绑定会被拒绝）。

> 形式化语法与规则请参考：`REL_Formal_Spec.md`；底层数据模型请参考：`third_party/xdataset/docs/Architecture.md`

## REL表达式的构成
REL表达式由以下元素按照一定的语法规则构成
- 标识符(仿真节点/自定义标识符/内建常量)
- 字面量
- 函数(内建函数/外部拓展)
- 运算符
- 关键字

### 标识符
#### 仿真节点
仿真生成的变量在表达式中的引用方式可以具有不同程度的简化。

Dataset 是一个树形命名空间，用 `.` 分隔层级。一个仿真变量引用的完整路径由多段以 `.` 连接的标识符构成：倒数第二段是仿真结果名称，最后一段是该结果内的变量名，前面的若干段（可为零）是中间的命名层级。

例如 `noise.simulation.SP1.SP.Vout` 中：
- `noise` — Dataset 名称
- `simulation.SP1` — 中间命名层
- `SP` — 仿真结果
- `Vout` — 该结果内的变量

如果 VariableName 在整个 Dataset 的命名唯一，那么可以简写为：
`DatasetName..VariableName`
其中，双点号 `..` 表示该变量在该数据集中是唯一的。

如果当前 REL 运行的默认 Dataset 就是引用变量的 Dataset，则可以进一步简化为：
`仿真结果名.变量名` 或多级 `层级名.结果名.变量名`（省略 Dataset 名，段数≥2）

如果该变量在默认 Dataset 中唯一，则可以简写为：
`VariableName`

> 默认Dataset
REL解释器在运行时可以设置一个默认Dataset，切换默认Dataset会改变变量的数据输入

#### 自定义标识符
变量名和函数名统一使用如下标识符规则：

- 以字母或下划线开头
- 后续字符只能是字母、数字或下划线
- 大小写敏感
- 不能与关键字重名

仿真节点引用使用点分段形式，支持以下形式：

- 完整形式：`DatasetName.层级名.结果名.变量名`（倒数第二段为结果名，其余前缀为命名层级）
- 数据集唯一变量简写：`DatasetName..VariableName`
- 默认数据集下的路径：`层级名.结果名.变量名`（省略 Dataset 名，段数≥2）
- 默认数据集且变量唯一时：`VariableName`

其中每个名称段都必须满足标识符语法 `[A-Za-z_][A-Za-z0-9_]*`。

#### 内建常量

| 常量 | 描述 | 值 |
|---|---|---|
| `PI` | 圆周率 | `3.1415926535898` |
| `pi` | 圆周率 | `3.1415926535898` |
| `e` | 欧拉常数 | `2.718281822` |
| `ln10` | 10 的自然对数 | `2.302585093` |
| `boltzmann` | 玻尔兹曼常数 | `1.380658e-23 J/K` |
| `qelectron` | 电子电荷 | `1.60217733e-19 C` |
| `planck` | 普朗克常数 | `6.6260755e-34 J*s` |
| `c0` | 真空中的光速 | `2.99792e+08 m/s` |
| `e0` | 真空介电常数 | `8.85419e-12 F/m` |
| `u0` | 真空磁导率 | `12.5664e-07 H/m` |
| `tinyReal` | 浮点数最小值 | `2.2e-308` |
| `hugeReal` | 浮点数最大值 | `3.4e+38` |

> 内建常量是预定义标识符，并非关键字。解析器将其当作普通标识符（`reference`）处理，其数值在求值期解析，因此可被宿主环境的同名绑定遮蔽。

### 函数
函数的来源分为内建函数和python拓展函数，命名规则与“自定义标识符”中的标识符规则一致。

函数调用使用 `func(expr_list)` 形式时，参数槽允许缺省。相邻逗号用于跳过前面参数槽，以便给后续参数赋值，例如：

```text
func(a,,,,1)
```

约束：

- 缺省参数槽只能用于“后面仍有显式参数”的场景。
- 没有任何显式参数时，必须写 `func()`，不允许纯缺省形式。
- 参数列表末尾不允许缺省槽。

函数参数缺省的合法/非法示例：

| 示例 | 结论 | 说明 |
|---|---|---|
| `func()` | 合法 | 空参数列表。 |
| `func(,,a)` | 合法 | 前置参数槽使用缺省值，在后续位置给值。 |
| `func(,,)` | 非法 | 纯缺省形式；应写 `func()`。 |
| `func(,,a,,)` | 非法 | 尾部缺省不允许；应写 `func(,,a)`。 |

### 字面量
REL 支持的字面量形式和C语言基本一致，并额外支持空值和虚数字面量。支持的字面量形式有：
| 支持的字面量形式 | 描述 | 示例 |
|---|---|---|
| `NULL` | 空值 | `NULL` |
| `TRUE` | 逻辑真（Boolean 类型） | `TRUE` |
| `FALSE` | 逻辑假（Boolean 类型） | `FALSE` |
| 十进制整数常量 | 以十进制表示的整数常量 | `13` |
| 十六进制整数常量 | 以十六进制表示的整数常量，大小写不敏感 | `0x3E` |
| 八进制整数常量 | 以八进制表示的整数常量 | `0377` |
| 字符串常量 | 使用双引号括起来的字符串 | `"a string"` |
| 实数常量 | 浮点数或科学计数法表示的实数，大小写不敏感 | `10.3`；`25.4e-3` |
| 虚数常量 | 带有虚数单位 `i` 的常量 | `3.5i`；`2i` |

> 逻辑运算（`&&` / `\|\|` / `!`）与比较运算（`==`、`<` 等）的结果为 Boolean（`TRUE`/`FALSE`），
> 非零数值视为逻辑真。Boolean 也可参与算术运算（按 Integer 0/1 提升）。

字符串字面量由一个或多个用双引号（`" "`）括起来的字符组成。字符串字面量中可以包含不可打印字符，这些字符通过反斜杠转义来表示：

| 不可打印字符 | 描述 |
|---|---|
| `\n` | 换行 |
| `\r` | 回车 |
| `\f` | 换页 |
| `\b` | 退格 |
| `\t` | 制表符 |
| `\"` | 双引号 |
| `\\` | 反斜杠 |
| `\xNN` | 十六进制表示的字符（`N` 为 `0`–`9` 或 `A`–`F`，大小写不敏感） |
| `\0NNN` | 八进制表示的字符（`N` 为 `0`–`7`） |

如果不希望对控制字符进行转换，可以使用**两个单引号**将字符串括起来，而不是使用双引号。该形式下不进行任何转义处理，反斜杠仅作为普通字符。例如：

```text
'' \usr\local im.abc ''
```

也就是说，这种写法会“原样保留字符”，仅在遇到下一对 `''` 时结束。

#### 数值字面量的缩放因子与物理单位

在 REL 中，数值类型字面量（整数、实数、复数）支持在数值后追加**缩放因子**和/或**物理单位**，例如：`1.23MHz`、`50Ohm`、`8a`。

字面量后缀的规则如下：

1. **缩放因子可选**，**物理单位可选**。  
2. 当二者同时出现时，顺序必须为：**缩放因子在前，单位在后**，如`1.23MHz`，其中`1.23`为字面量，`M`为缩放因子，`Hz`为单位。  
3. 仅使用缩放因子（无单位）是合法的，例如 `8M`。
4. 使用物理单位时，若未显式指定缩放因子，则默认缩放系数为 `1.0`。 
5. 带物理单位的数值参与运算时，系统会依据量纲规则自动推导结果单位。

补充约束：

- 大小写严格敏感。
- 当命中 `predefined_scaled_unit` 时，不允许再叠加 `scale_factor` 或再次拼接 `unit`。
- 当 `scale_factor` 与 `unit` 同时出现时，顺序必须为 `scale_factor` 在前、`unit` 在后。

- **支持的缩放因子**

| 缩放因子 | 数值等价 | 含义 |
|---|---:|---|
| `T` | `10^12` | 太（Tera） |
| `G` | `10^9` | 吉（Giga） |
| `M` | `10^6` | 兆（Mega） |
| `K` | `10^3` | 千（kilo） |
| `k` | `10^3` | 千（kilo） |
| `_` | `1` | 无缩放 |
| `m` | `10^-3` | 毫（milli） |
| `u` | `10^-6` | 微（micro） |
| `n` | `10^-9` | 纳（nano） |
| `p` | `10^-12` | 皮（pico） |
| `f` | `10^-15` | 飞（femto） |
| `a` | `10^-18` | 阿（atto） |

- **支持的量纲单位**

| 单位 | 量纲 |
|---|---|
| `Hz` | 频率（Frequency） |
| `Ohm` `Ohms` | 电阻（Resistance） |
| `S` | 电导（Conductance） |
| `F` | 电容（Capacitance） |
| `H` | 电感（Inductance） |
| `meter` `meters` `metre` `metres` | 长度（Length） |
| `sec` | 时间（Time） |
| `V` | 电压（Voltage） |
| `A` | 电流（Current） |
| `W` | 功率（Power） |

> 注：无缩放因子时，上述单位默认缩放系数均为 `1.0`。

- **预定义缩放单位标识**

当数值后的 token 与下表中的预定义词**完全匹配**时，直接采用其内置的缩放系数与单位映射。

| 缩放单位 | 数值等价 | 单位 | 物理量名称 | 含义 |
|---|---:|---|---|---|
| `mil` | `2.54*10^-5` | `meter` `meters` `metre` `metres` | 长度（Length） | 密耳（mil）/千分之一英寸 |
| `mils` | `2.54*10^-5` | `meter` `meters` `metre` `metres` | 长度（Length） | 密耳（mils） |
| `in` | `2.54*10^-2` | `meter` `meters` `metre` `metres` | 长度（Length） | 英寸（inch） |
| `ft` | `12*2.54*10^-2` | `meter` `meters` `metre` `metres` | 长度（Length） | 英尺（foot） |
| `mi` | `5280*12*2.54*10^-2` | `meter` `meters` `metre` `metres` | 长度（Length） | 英里（mile） |
| `cm` | `1.0*10^-2` | `meter` `meters` `metre` `metres` | 长度（Length） | 厘米（centimeter） |
| `PHz` | `1.0*10^15` | `Hz` | 频率（Frequency） | 拍赫（petahertz） |
| `dB` | `1.0` | 无 | 无 | 分贝（decibel） |
| `nmi` | `1852` | `meter` `meters` `metre` `metres` | 长度（Length） | 海里（nautical mile） |

> 注意事项
>- 缩放因子与单位**区分大小写**。  
>- 单独的 `m` 表示缩放因子 milli，不表示长度单位 meter。  
>- 小写 `f` 表示缩放因子 femto；大写 `F` 表示单位 Farad。  
>- 小写 `a` 表示缩放因子 atto；大写 `A` 表示单位 Ampere。  
>- 预定义缩放单位（如 `mils`、`in`、`ft`、`mi`、`nmi`）**不可再叠加缩放因子**。  
>- `1in`（英寸）与虚数字面量后缀 `i` 存在词法歧义：`1in` 会被解析为 `1i`（虚数）+ `n`（缩放因子 nano）。使用英寸时请在数值与 `in` 之间加空格，如 `1 in`。

### 运算符及优先级

REL 表达式的优先级与 C 语言对齐（当 C 语言存在对应运算符时）；结合性则统一为左结合。

- C 对应运算符按 C 的优先级处理。
- C 不存在的 REL 扩展语法元素（如 `**`、`::`）按本节给出的规则处理。
- 所有二元运算符均为左结合（含 `**`、`?:`）；一元运算符按前缀一元规则处理。

例如，`expr + expr / expr` 解释为 `expr + (expr / expr)`。

括号 `()`、索引运算符 `[]` 和矩阵生成运算符 `{}` 具有最高优先级。  
一元运算符 `!`、`NOT`、`~` 和一元 `-` 次之。  
之后依次是幂运算（REL 扩展）、乘除取余、加减、移位、关系比较、相等比较、按位与、按位异或、按位或、逻辑与、逻辑或、条件运算符。

逻辑运算符 `!`、`NOT`、`&&`、`AND`、`||` 和 `OR` 用于逻辑判断。操作数会根据其逻辑值参与运算，运算结果为逻辑真或逻辑假。  
注意：`&&` / `AND` 与 `||` / `OR` 的**两个操作数都会被求值**（当前实现不做短路求值），因此不要依赖“右操作数在条件不满足时不求值”的行为。

REL 不支持注释语法（例如 `//` 或 `/* ... */` 均不属于语言语法）。

| 优先级 | 运算符 | 名称 / 描述 | 示例 |
|---|---|---|---|
| 1 | `()` | 函数调用、矩阵索引 / 表达式分组 | `func(expr_list)`；`expr(expr_list)`；`(expr)`|
| 1 | `[]` | sweep 索引器、sweep 生成器 | `expr[expr_list]`；`[expr_list]` |
| 1 | `{}` | 矩阵生成器 | `{expr_list}` |
| 2 | `**` | 幂运算（REL 扩展，左结合） | `expr ** expr` |
| 3 | `!` / `NOT` | 逻辑非 | `!expr`；`NOT expr` |
| 3 | `~` | 按位取反（一元） | `~expr` |
| 3 | `-` | 一元负号（对值取负） | `-expr` |
| 4 | `*` | 乘法 | `expr * expr` |
| 4 | `/` | 除法 | `expr / expr` |
| 4 | `%` | 整数除法取余（模） | `expr % expr` |
| 5 | `+` | 加法 | `expr + expr` |
| 5 | `-` | 减法 | `expr - expr` |
| 6 | `<<` | 按位左移 | `expr << expr` |
| 6 | `>>` | 按位右移 | `expr >> expr` |
| 7 | `>=` | 大于等于 | `expr >= expr` |
| 7 | `<=` | 小于等于 | `expr <= expr` |
| 7 | `>` | 大于 | `expr > expr` |
| 7 | `<` | 小于 | `expr < expr` |
| 8 | `==` / `EQUALS` | 等于 | `expr == expr`；`expr EQUALS expr` |
| 8 | `!=` / `NOTEQUALS` | 不等于 | `expr != expr`；`expr NOTEQUALS expr` |
| 9 | `&` | 按位与 | `expr & expr` |
| 10 | `^` | 按位异或 | `expr ^ expr` |
| 11 | `\|` | 按位或 | `expr \| expr` |
| 12 | `&&` / `AND` | 逻辑与 | `expr && expr`；`expr AND expr` |
| 13 | `\|\|` / `OR` | 逻辑或 | `expr \|\| expr`；`expr OR expr` |
| 14 | `?:` | 条件运算符（三元运算符，左结合） | `expr ? expr : expr` |
> 说明：`expr_list` 为用 `,`（逗号）分割的表达式序列（也可能是单个 `expr`）。逗号仅用于列表分隔，不作为独立运算符。

**运算语义要点**（详细的类型/形状/单位推导规则见 `third_party/xdataset/docs/Architecture.md`）：

- `*` 与 `/` 对**矩阵**操作数分别解释为矩阵乘法与 `A × inv(B)`；操作数含 Scalar 时退化为逐元素广播。需要纯逐元素乘/除时使用函数 `times()` / `rdivide()`（见[内建函数参考](#内建函数参考)）。
- `+ - * / %` 按提升规则确定结果类型：`Integer → Real → Complex`；其中 `/` 的 `Integer / Integer` 强制提升为 `Real`（如 `10 / 2` 结果是 `5.0`，不是 `5`）。
- 比较（`==`、`<` …）、逻辑（`&&`、`\|\|`、`!`）与按位运算的结果类型为 **Boolean / Integer**，无量纲；比较要求双方量纲一致。
- 所有二元运算在 DataArray 参与时**逐行**进行，且支持行广播（1 行的一方自动复制到另一方的行数）；单个 Measurement 与 DataArray 运算时自动广播到 DataArray 的每一行。

#### 序列生成器

序列生成器使用 `::` 运算符构造一段等差序列，仅能出现在索引或 `[]` / `{}` 生成器内部，不能作为通用表达式使用。

- `::`：裸序列。仅允许出现在索引上下文中，表示该 sweep 维度全选（如 `a[::,1]`）；出现在 `[]` / `{}` 生成器上下文中是非法的（`[::]`、`{::}` 均为错误写法）。
- `start::stop`：从 `start` 到 `stop` 的序列，步长默认为 `1`，包含两端点。例如 `1::5` 生成 `1, 2, 3, 4, 5`。
- `start::step::stop`：从 `start` 到 `stop`、步长为 `step` 的序列，包含 `start`，仅当 `stop` 能被步长精确命中时才包含 `stop`。`step` 允许为小数（如 `0.0::0.25::1.0` 生成 `0.0, 0.25, 0.5, 0.75, 1.0`），也允许为负值以生成递减序列（如 `5::-1::1` 生成 `5, 4, 3, 2, 1`）。若 `step` 为 `0`，或其符号与 `start`→`stop` 的方向不一致，则在求值期报错。

#### 索引

- `[]`：用作 **sweep 索引器**（`a[i]`、`a[i, j, k]`）或 **sweep 生成器**（`[expr_list]`）。

  **sweep 索引器**作用于左侧 DataArray（如 `a[i][j]` 的连续索引），按维度施加选择器，对应底层 `select()`：

  - 索引是 **0-based**（`a[0]` 取第一行，与 C 数组一致）；
  - 选择器数量少于维度数时，自动在前面补齐 `Any()`——即给出的选择器**从最内层维度开始**对齐。二维 `Vout[freq, power]` 中 `Vout[1]` 等价于 `Vout[::, 1]`（固定 power 维，保留 freq 维）；
  - 维度选择器可以是整数、`::`（全选）或 `start::step::stop` 序列；
  - **解包规则**：索引结果若收敛为单个值（Independent、1 行 1 单元），自动解包为 Measurement。例如一维 `Vout`（仅 freq 维）的 `Vout[1]` 返回 Measurement；而 `Vout[::]` 或二维 `Vout[::, 1]`（保留 freq 维）仍返回 DataArray；
  - 裸 `::`（`a[::]`）表示该维全选，结果仍是 DataArray。

  **sweep 生成器**将 `expr_list` 中每一项视为若干行数据，逐行纵向拼接为一个新的 Independent DataArray。不同 item 的行数可以不等（各行拼接），但每行的 shape 必须一致。一个 Scalar 字面量或 Measurement 被视为一行对应 shape 的数据。结果始终为 DataArray (Independent)，无上游坐标轴。

  ```
  [1, 2, 3]       → 3 个 Scalar → 3 行 Scalar → DataArray(3行, spec=[Regular(3)])
  [a(2行), b(3行)]  → 2 + 3 = 5 行 → DataArray(5行)
  [{1,2}, {3,4}]   → 2 个 Vector(2) → 2 行 Vector(2) → DataArray(2行)
  ```

- `()`：当左侧为矩阵对象（Matrix 或 Vector Measurement、或 DataArray）时解释为矩阵索引（如 `a(i, j)`、`a(1::1::3, 3)`），对应底层 `at()`；否则解释为函数调用。

  **矩阵索引使用 1-based 下标**（`a(1)` 取第一行/第一个元素），与 REL 面向用户的自然数下标一致；`()` 索引作用于数据的 Shape（矩阵的行/列），不改变维度结构。

- `{}`：用作 **矩阵生成器**（`{expr_list}`）。将 `expr_list` 中每一项视为若干行数据，合并为一个结果。

  **行数规则**：所有 item 的行数要么相等，要么为 1（仅一行的 item 会被广播重复以匹配最大行数）。例如 `(3行, 1行, 3行)` 允许，`(3行, 2行)` 不允许。每行的 shape（Scalar 或 Vector 或 Matrix）必须一致。

  **结果类型**：纯 Measurement（每个 item 恰好一行，视为一个标量或向量或矩阵值）时，结果升阶为 Measurement：Scalar × N → Vector(N)，Vector(w) × N → Matrix(N, w)。只要任一 item 是 DataArray，结果即为 DataArray。单一元素 `{5}` 保持原值（不解包成 Vector(1)）；嵌套 `{{1},{2}}` 产生 Matrix(2,1)。

  ```
  {1, 2, 3}         → 3 个 Scalar → Vector(3)                       [Measurement]
  {{1, 2}, {3, 4}}  → 2 个 Vector(2) → Matrix(2, 2)                 [Measurement]
  {[1,2], [3,4]}    → 2 个 DataArray(各1行Vector(2)) → 2行 Vector(2)  [DataArray]
  {DA(3行), M}       → 含 DataArray → M 广播到 3 行 → 结果保持 DataArray [DataArray]
  ```

### 条件表达式

`if-then-else` 结构提供了一种便捷方式，可针对完整多维变量的每个元素逐一应用条件判断。其语法如下：

```REL
A = if (condition) then true_expression else false_expression
```

其中：

- `condition`：条件表达式  
- `true_expression`：条件为真时返回的表达式  
- `false_expression`：条件为假时返回的表达式  

上述三者都可以是任意合法表达式。它们的维度和数据点数量需满足与基本运算符相同的匹配规则。

此外，还可以使用多层嵌套的 `if-then-else` 结构，例如：

```text
A = if (condition) then true_expression elseif (condition2) then true_expression else false_expression
```

结果的数据类型由 `true_expression` 和 `false_expression` 的类型共同决定；结果的大小由 `condition`、`true_expression` 和 `false_expression` 的大小共同决定。

---

#### 示例
以下示例展示了使用不同运算符编写条件表达式的方法：

```REL
boolV1 = 1
boolV2 = 1

eqOp    = if (boolV1 == 1) then 1 else 0
eqOp1   = if (boolV1 EQUALS 1) then 1 else 0

notEqOp  = if (boolV1 != 1) then 1 else 0
notEqOp1 = if (boolV1 NOTEQUALS 1) then 1 else 0

andOp   = if (boolV1 == 1 AND boolV2 == 1) then 1 else 0
andOp1  = if (boolV1 == 1 && boolV2 == 1) then 1 else 0

orOp    = if (boolV1 == 1 OR boolV2 == 1) then 1 else 0
orOp1   = if (boolV1 == 1 || boolV2 == 1) then 1 else 0
```

上述示例对应结果均为 `1`。

### 关键字
由于REL是专用于计算表达式的DSL,实际上并没有太多的关键字，用户自定义方程名、函数名和变量名不得与关键字重名。以下是在REL中有特殊含义的关键字：

| 关键字 | 分类 | 说明 | 示例 |
|---|---|---|---|
| `if` | 条件表达式 | 条件表达式起始关键字 | `A = if (x > 0) then 1 else 0` |
| `then` | 条件流程 | 指定条件为真时的分支表达式 | `if (cond) then expr1 else expr2` |
| `elseif` | 条件表达式 | 条件分支扩展关键字，用于多分支判断 | `if (c1) then a elseif (c2) then b else c` |
| `else` | 条件表达式 | 指定条件为假时的分支表达式 | `if (cond) then expr1 else expr2` |
| `AND` | 逻辑运算 | 逻辑与 | `if (a == 1 AND b == 1) then 1 else 0` |
| `OR` | 逻辑运算 | 逻辑或 | `if (a == 1 OR b == 1) then 1 else 0` |
| `NOT` | 逻辑运算 | 逻辑非 | `if (NOT (a == 1)) then 1 else 0` |
| `EQUALS` | 比较运算 | 等于 | `if (x EQUALS 10) then 1 else 0` |
| `NOTEQUALS` | 比较运算 | 不等于 | `if (x NOTEQUALS 10) then 1 else 0` |
| `TRUE` | 字面量 | 逻辑真 | `if (flag == TRUE) then 1 else 0` |
| `FALSE` | 字面量 | 逻辑假 | `if (flag == FALSE) then 1 else 0` |
| `NULL` | 字面量 | 空值字面量 | `A = NULL` |

REL 语言整体大小写敏感。

## 内建函数参考

函数来源为**内建库**（运行时自省 / 数据工具）与**数学库**（逐元素数学运算、归约、运算符内核）。
所有函数名与变量名共用标识符规则，大小写敏感。参数支持[缺省槽](#函数)（如 `db(2.0,,100)` 表示省略第二个参数）。

> 内建函数大多**逐元素**作用于 `Value`（无论 Measurement 还是 DataArray 形态）的每个单元，
> 因此天然支持标量、向量、矩阵与任意维度扫描数据；
> 单元类型为 Integer / Real（部分支持 Complex），输出一般提升为 Real；除特殊说明外保留输入单位。

### 内建库（builtin）

| 函数 | 签名 | 返回 | 说明 |
|---|---|---|---|
| `datasets` | `datasets()` | DataArray (Independent, String) | 当前注册的所有 Dataset 名，每行一个 |
| `default_dataset` | `default_dataset()` | DataArray (Independent, String) | 当前默认 Dataset 名；未设置时返回 `"NO DEFAULT DATASET"` |
| `variables` | `variables()` | DataArray (Independent, String) | 宿主环境绑定的用户变量名（当前实现返回空数组） |
| `what` | `what(x)` | DataArray (Independent, String) | x 的元信息：Dependency、Kind、Dimension、Data Shape、Data Type，带量纲时含 Unit |
| `indep` | `indep(da, selector = 1)` | DataArray (Independent) | 提取 da 的某个独立变量作为坐标轴。`selector` 为 1-based 整数（`1` = 最内层维度）或独立变量名字符串；缺省为 `1` |
| `output` | `output(da, variable_name = "data")` | Measurement (String) | 将 da 展开为 DataFrame 并写出 `<variable_name>.csv`（相对当前工作目录），返回文件路径字符串 |

`what()` 输出示例：

```
Dependency: [freq]
Kind: Dependent
Dimension: [2]
Data Shape: Scalar
Data Type: Real
Unit: V
```

### 数学库（math）

#### 逐元素一元函数

| 类别 | 函数 | 说明 |
|---|---|---|
| 三角函数 | `sin(x)` `cos(x)` `tan(x)` `cot(x)` | -- |
| 反三角函数 | `asin(x)` `acos(x)` `atan(x)` `acot(x)` | -- |
| 双曲函数 | `sinh(x)` `cosh(x)` `tanh(x)` `coth(x)` | -- |
| 反双曲函数 | `asinh(x)` `acosh(x)` `atanh(x)` `acoth(x)` | -- |
| 对数 / 指数 | `log(x)` `ln(x)` `log10(x)` `exp(x)` | **`log` 与 `ln` 都是自然对数**（互为别名）；以 10 为底请用 `log10` |
| 幂 / 根 | `sqrt(x)` `sqr(x)` | `sqr` 保持输入类型（`sqr(3)` → Integer `9`）；`sqrt` 输出 Real |
| 取整 | `ceil(x)` `floor(x)` `round(x)` `cint(x)` `fix(x)` `int(x)` `float(x)` | 输出 Integer（`float` 除外）：`round` 与 `cint` 同义（四舍五入）；`fix` 与 `int` 同义（向零截断）；`ceil` 上取整、`floor` 下取整；`float` 转为 Real |
| 角度转换 | `deg(x)` `rad(x)` | `deg`：弧度→度；`rad`：度→弧度 |
| 绝对值 / 符号 | `abs(x)` `sgn(x)` | `sgn` 返回 -1/0/1 |
| 复数 | `real(x)` `re(x)` `imag(x)` `im(x)` `conj(x)` `conjg(x)` `mag(x)` `phase(x)` | `re`/`im`/`conjg` 为别名；`mag` 与 `abs` 同义；**`phase` 返回角度制**（`phase(-1)` = 180） |
| dB 转换 | `dbmtow(x)` `wtodbm(x)` | `dbmtow`：dBm→W（10^(x/10) × 0.001）；`wtodbm`：W→dBm（10·log10(x·1000)） |
| 杂项 | `sinc(x)` `step(x)` | `sinc` = sin(πx)/(πx)，|x|≈0 时为 1；`step`：x>0 为 1、x<0 为 0、**x==0 为 0.5** |

#### 逐元素二元函数

| 函数 | 签名 | 说明 |
|---|---|---|
| `pow` | `pow(base, exp)` | 幂运算，与 `**` 同语义；指数必须无量纲，结果单位继承底数 |
| `root` | `root(x, n)` | n 次方根（`n` 无量纲） |
| `atan2` | `atan2(y, x)` | 反正切（四象限） |
| `max2` | `max2(a, b)` | 逐元素取较大者 |
| `min2` | `min2(a, b)` | 逐元素取较小者 |
| `db` | `db(r, z1 = 50Ω, z2 = 50Ω)` | 带阻抗归一化的 dB 换算：`20·log10(|r|) − 10·log10(z_out/z_in)`，其中 `z_out = |z2|²/Re(z2)`、`z_in = |z1|²/Re(z1)`（实阻抗时即 `z2/z1`）；`z1`/`z2` 缺省 50Ω |
| `dbm` | `dbm(v, z = 50Ω)` | 电压→dBm：`20·log10(|v|) − 10·log10(|z|/50) + 10` |

#### 归约函数（沿最内层维度）

`min(x)` `max(x)` `sum(x)` `mean(x)` 沿 DataArray 的**最内层维度**归约，要求单元为 Scalar：

| 函数 | 说明 |
|---|---|
| `min(x)` / `max(x)` | 最内层最小 / 最大值；比较规则：数值按大小、复数按模、字符串按字典序；保留输入类型与单位 |
| `sum(x)` | 最内层求和；支持 Integer / Real / Complex，保留单位 |
| `mean(x)` | 最内层平均；Integer 输入提升为 Real（结果单位不变） |

多维数据归约时保留外层维度（每个外层分组产生一行）；一维数据归约后为单行。例如 `min(Vout)` 返回一个 1 行结果，`min(S)`（bias×freq 二维）返回以 bias 为坐标轴、每行是该 bias 下全部频点的最小值。

#### 运算符内核函数

运算符也可作为函数调用，语义与运算符完全一致（支持行广播与形状广播）：

| 类别 | 函数 |
|---|---|
| 算术 | `add(x, y)` `subtract(x, y)` `multiply(x, y)` `divide(x, y)` `times(x, y)` `rdivide(x, y)` `mod(x, y)` `pow(x, y)` |
| 比较 | `equal(x, y)` `notequal(x, y)` `lessthan(x, y)` `greaterthan(x, y)` `lessequal(x, y)` `greaterequal(x, y)` |
| 逻辑 | `and(x, y)` `or(x, y)` `not(x)` |
| 按位 | `bitand(x, y)` `bitor(x, y)` `bitxor(x, y)` `bitnot(x)` `shiftleft(x, y)` `shiftright(x, y)` |
| 其他 | `negate(x)` `conditional(condition, true_value, false_value)` |

其中 `times` / `rdivide` 是**纯逐元素**乘/除（对应 MATLAB 的 `.*` / `./`），与矩阵语义的 `*` / `/`（`multiply` / `divide`）不同，用于对矩阵做逐元素广播运算。

## Python 拓展

REL 支持用 Python 编写插件函数（`BUILD_PYTHON=ON` 时可用），详见 `plugin/PYTHON.md`。核心用法：

```python
# my_plugins.py
import numpy as np
import rel

def snr(args):
    signal = np.asarray(args["signal"])   # rel.Value -> numpy 数组（SI 值）
    noise  = np.asarray(args["noise"])
    return 20.0 * np.log10(np.abs(signal) / np.abs(noise))   # ndarray -> 自动转 Value

rel.register_function("snr", [rel.Param("signal"), rel.Param("noise")], snr)
```

要点：

- 插件通过 `Environment::LoadPython(path)` / 宿主配置 `"python_plugins": ["plugins/snr.py"]` 加载（路径相对配置文件所在目录）；
- 注册函数名**不得与内建函数重名**（冲突直接报错）；
- 参数可用 `rel.Param("name", default)` 声明默认值（静态缺省，不参与广播推导）；
- `np.asarray(...)` 导出 SI 缩放后的值（`1 GHz` → `1e9`），单位通过 `args["x"].unit` 读取；`rel.eval(source)` 可解析并求值一段 REL 表达式（每次使用全新临时 Environment）。

## 编写 REL 表达式的实用模式

> 以下示例以默认 Dataset 含变量 `Vout`（Dependent，坐标 `freq(3) × power(2)`，单位 V）、
> `freq`（Independent，GHz）、`I`（Measurement，Vector(2)，A）为例。绑定形式 `name = expr` 由宿主环境提供。
> 注意：REL 本身**不支持注释**，下面的说明文字仅用于文档展示，不属于表达式语法。

**基础算术与单位推导**（除法归一化、乘法推导量纲）：

```REL
gain = Vout / 1V
power = Vout * I
freq_hz = freq * 1e9
ratio_db = dbm(Vout, 50Ohm)
mag_db = 20 * log10(abs(Vout))
```

说明：`Vout / 1V` 结果无量纲；`Vout * I` 逐行广播，`V × A → W`；`dbm` 第二参缺省即 50Ω。

**索引与切片**（`[]` 为 0-based、作用于维度，`()` 为 1-based、作用于矩阵行/列）：

```REL
v1 = Vout[1]
v2 = Vout[::, 1]
v3 = Vout[0::1::2, ::]
row = S(1, ::)
cell = S(2, 1)
```

说明：`Vout[1]` 固定最内层维度（power）到索引 1，保留 freq 维，等价于 `Vout[::, 1]`；
`S(1, ::)` 取矩阵第一行全部列。

**生成器**（`[]` 生成 Independent DataArray，`{}` 生成 Measurement 或 DataArray）：

```REL
freqs = [1.0, 1.5, 2.0, 2.5]
matrix = {{1, 2}, {3, 4}}
vec = {10, 20, 30}
```

说明：`freqs` 为 4 行 Scalar 的 Independent DataArray；`matrix` 为 Matrix(2,2) Measurement；
`vec` 为 Vector(3) Measurement。

**条件与归约**（`if…then…else` 逐元素、`min/max/sum/mean` 沿最内层维度）：

```REL
clipped = if (Vout > 2.5) then 2.5 else Vout
sel = if (freq > 2GHz) then 1 else 0
vmin = min(Vout)
vmax_by_freq = max(Vout[::, 0])
avg = mean(power)
```

说明：`clipped` 逐元素限幅；`vmin` 为全局最小值（1 行结果）；`Vout[::, 0]` 固定 power 后，
`max` 沿剩余 freq 维归约。

**字符串与自省**：

```REL
info = what(Vout)
ds = datasets()
d = default_dataset()
f = indep(Vout, "freq")
f2 = indep(Vout, 2)
```

说明：`what` 返回类型/维度/单位信息；`indep` 按名字或 1-based 索引（`2` = 从内向外第 2 维）提取坐标轴。

**常见易错点**：

- `[]` 与 `()` 下标方向相反：`Vout[0]`（0-based、作用于维度）≠ `S(1)`（1-based、作用于矩阵行/列）；
- `log(x)` 是自然对数；需要以 10 为底时用 `log10(x)`；
- `10 / 2` 结果是 `5.0`（Real），不是 `5`（Integer）；
- `*` 对矩阵是矩阵乘法；逐元素乘法用 `times(a, b)`；
- 逻辑比较（`Vout > 2.5`）返回 Boolean，需要数值时用 `if (cond) then 1 else 0` 转换；
- 数值与单位拼接（如 `1.23MHz`）大小写敏感，`m` 是毫、`M` 是兆；`1in` 会被解析为虚数 `1i` + 缩放因子 `n`，英寸请写作 `1 in`。