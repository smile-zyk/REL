# REL
REL(ResultsView Expression Language/后处理表达式语言) 是一种对仿真结果进行计算的语言，主要用于后处理表达式的计算。

主要特点为：
1. REL 是一种专用于基于多维仿真数据的结果计算表达式的DSL(Domain-Specific Language)
2. REL 操作的数据和表达式计算的结果统一为一种多维仿真数据结构
3. REL 兼容Keysight ADS AEL Mesaure Expression的语法
4. REL 支持使用Python拓展更多的函数

一个 REL 输入由单个表达式构成：
```REL
REL expression
```

在宿主环境中，可以使用“标识符 = 表达式”的形式保存计算结果，例如：

```text
result_name = REL expression
```

该绑定形式属于宿主环境能力，不属于 REL 语法本身。

> 形式化语法与规则请参考：`REL_Formal_Spec.md`

## REL表达式的构成
REL表达式由以下元素按照一定的语法规则构成
- 标识符(仿真节点/自定义标识符/内建常量)
- 字面量
- 函数(内建函数/外部拓展)
- 运算符
- 关键字

### 标识符
#### 仿真节点
仿真生成的变量在表达式中的引用方式可以具有不同程度的简化。通常，一个仿真节点变量的完整名称定义如下：
`DatasetName.AnalysisName.AnalysisType.VariableName`
如果VariableName在整个Dataset的命名唯一，那么可以简写为:
`DatasetName..VariableName`
其中，双点号 `..` 表示该变量在该数据集中是唯一的。
如果当前REL运行的默认Dataset就是引用变量的Dataset，则可以进一步简化为:
`VariableName`
如果并非唯一，但是节点在默认Dataset下定义,可以简写为:
`AnalysisName.AnalysisType.VariableName`
在大多数情况下，一个Dataset只包含一次分析的结果，因此通常仅使用变量名即可完成引用。双点号 .. 最常见的用途，是在需要将变量明确关联到非默认Dataset时使用。
> 默认Dataset
REL解释器在运行时可以设置一个默认Dataset，切换默认Dataset会改变变量的数据输入

#### 自定义标识符
变量名和函数名统一使用如下标识符规则：

- 以字母或下划线开头
- 后续字符只能是字母、数字或下划线
- 大小写敏感
- 不能与关键字重名

仿真节点引用使用点分段形式，支持以下形式：

- 完整形式：`DatasetName.AnalysisName.AnalysisType.VariableName`
- 数据集唯一变量简写：`DatasetName..VariableName`
- 默认数据集下的完整分析路径：`AnalysisName.AnalysisType.VariableName`
- 默认数据集且变量唯一时：`VariableName`

其中每个名称段（`DatasetName`、`AnalysisName`、`AnalysisType`、`VariableName`）都必须满足标识符语法 `[A-Za-z_][A-Za-z0-9_]*`。

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
| 十进制整数常量 | 以十进制表示的整数常量 | `13` |
| 十六进制整数常量 | 以十六进制表示的整数常量，大小写不敏感 | `0x3E` |
| 八进制整数常量 | 以八进制表示的整数常量 | `0377` |
| 字符串常量 | 使用双引号括起来的字符串 | `"a string"` |
| 实数常量 | 浮点数或科学计数法表示的实数，大小写不敏感 | `10.3`；`25.4e-3` |
| 虚数常量 | 带有虚数单位 `i` 的常量 | `3.5i`；`2i` |

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
其中，`&&` / `AND` 的右操作数仅在左操作数为真时才会被求值；`||` / `OR` 的右操作数仅在左操作数为假时才会被求值。

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

#### 序列生成器

序列生成器使用 `::` 运算符构造一段等差序列，仅能出现在索引或 `[]` / `{}` 生成器内部，不能作为通用表达式使用。

- `::`：裸序列。仅允许出现在索引上下文中，表示该 sweep 维度全选（如 `a[::,1]`）；出现在 `[]` / `{}` 生成器上下文中是非法的（`[::]`、`{::}` 均为错误写法）。
- `start::stop`：从 `start` 到 `stop` 的序列，步长默认为 `1`，包含两端点。例如 `1::5` 生成 `1, 2, 3, 4, 5`。
- `start::step::stop`：从 `start` 到 `stop`、步长为 `step` 的序列，包含 `start`，仅当 `stop` 能被步长精确命中时才包含 `stop`。`step` 允许为小数（如 `0.0::0.25::1.0` 生成 `0.0, 0.25, 0.5, 0.75, 1.0`），也允许为负值以生成递减序列（如 `5::-1::1` 生成 `5, 4, 3, 2, 1`）。若 `step` 为 `0`，或其符号与 `start`→`stop` 的方向不一致，则在求值期报错。

#### 索引

- `[]`：用作 sweep 索引器（`a[i]`、`a[i, j, k]`）或 sweep 生成器（`[expr_list]`）。索引可连续使用，如 `a[i][j]`；生成器形式将 `expr_list` 元素拼接为一个 sweep，例如 `[1, 2, 3]` 与 `[1::1::3]` 都生成 sweep `1, 2, 3`。
- `()`：当左侧为矩阵对象时解释为矩阵索引（如 `a(i, j)`、`a(1::1::3, 3)`），否则解释为函数调用。
- `{}`：用作矩阵生成器（`{expr_list}`），将 `expr_list` 元素拼接为一个矩阵，例如 `{1, 2, 3}` 与 `{1::1::3}` 都生成矩阵 `1, 2, 3`。

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
| `NULL` | 字面量 | 空值字面量 | `A = NULL` |

REL 语言整体大小写敏感。