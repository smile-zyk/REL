# REL Formal Specification

## 1. 词法规则（Regex）

### 1.1 基础记号

```text
IDENTIFIER            = [A-Za-z_][A-Za-z0-9_]*

KW_IF                 = if
KW_THEN               = then
KW_ELSEIF             = elseif
KW_ELSE               = else
KW_AND                = AND
KW_OR                 = OR
KW_NOT                = NOT
KW_EQUALS             = EQUALS
KW_NOTEQUALS          = NOTEQUALS
KW_NULL               = NULL
```

### 1.2 运算符与分隔符记号

```text
OP_POW                = \*\*
OP_SEQ                = ::
OP_SHL                = <<
OP_SHR                = >>
OP_GE                 = >=
OP_LE                 = <=
OP_EQ                 = ==
OP_NE                 = !=
OP_LAND               = &&
OP_LOR                = \|\|
OP_LT                 = <
OP_GT                 = >
OP_ADD                = \+
OP_SUB                = -
OP_MUL                = \*
OP_DIV                = /
OP_MOD                = %
OP_BXOR               = \^
OP_BOR                = \|
OP_BAND               = &
OP_BNOT               = ~
OP_LNOT               = !
OP_QMARK              = \?
OP_COLON              = :

LPAREN                = \(
RPAREN                = \)
LBRACKET              = \[
RBRACKET              = \]
LBRACE                = \{
RBRACE                = \}
COMMA                 = ,
DOT                   = \.
DDOT                  = \.\.
```

### 1.3 字符串字面量

```text
STRING_LITERAL        = "(?:\\[nrfbt"\\]|\\x[0-9A-Fa-f]{2}|\\0[0-7]{3}|[^"\\])*"
RAW_STRING_LITERAL    = ''(?:[\s\S]*?)''
```

### 1.4 数值字面量（基础）

```text
INT_DEC               = (?:0|[1-9][0-9]*)
INT_HEX               = (?:0[xX][0-9A-Fa-f]+)
INT_OCT               = (?:0[0-7]+)

REAL_NUM              = (?:(?:[0-9]+\.[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)
IMAG_NUM              = (?:(?:(?:[0-9]+\.[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+|[0-9]+)i)

NUMERIC_BASE          = (?:(?:(?:(?:[0-9]+\.[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+|[0-9]+)i)|(?:0[xX][0-9A-Fa-f]+)|(?:(?:[0-9]+\.[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)|(?:0[0-7]+)|(?:0|[1-9][0-9]*))
```

### 1.5 数值后缀（缩放因子/单位）

```text
SCALE_FACTOR          = (?:T|G|M|K|k|_|m|u|n|p|f|a)
UNIT                  = (?:Hz|Ohm|Ohms|S|F|H|meter|meters|metre|metres|sec|V|A|W)
PREDEF_SCALED_UNIT    = (?:mil|mils|in|ft|mi|cm|PHz|dB|nmi)

NUMERIC_SUFFIX        = (?:(?:PREDEF_SCALED_UNIT)|(?:SCALE_FACTOR(?:UNIT)?)|(?:UNIT))
NUMERIC_LITERAL       = (?:NUMERIC_BASE(?:NUMERIC_SUFFIX)?)
```

### 1.6 空白与注释

```text
WS                    = [ \t\f\r\n]+
```

### 1.7 记号匹配策略

词法分析采用以下确定性规则，解析器据此可无歧义地切分记号：

1. **最长匹配优先（maximal munch）**：在当前位置总是选择能匹配的最长记号。由此保证多字符运算符优先于其前缀单字符运算符，即 `**` 优先于 `*`、`::` 优先于 `:`、`..`（DDOT）优先于 `.`（DOT）、`<<`/`<=` 优先于 `<`、`>>`/`>=` 优先于 `>`、`!=` 优先于 `!`、`||` 优先于 `|`、`&&` 优先于 `&`。
2. **数值字面量整体最长匹配**：`NUMERIC_LITERAL = NUMERIC_BASE NUMERIC_SUFFIX?` 作为整体按最长匹配切分。当 `0` 后紧跟 `[xX]` 与至少一个十六进制位时识别为 `INT_HEX`（否则会被切为 `0` + 标识符 `x...`）；其余 `INT_DEC` / `REAL_NUM` / `IMAG_NUM` 形态共享同一吃字符路径，由最长匹配自然区分（末尾 `i` 归入 `IMAG_NUM`，出现 `.` 或 `[eE]` 归入 `REAL_NUM`，否则为 `INT_DEC`）。`INT_OCT` 与 `INT_DEC` 在词法层不区分，统一作为 `NUMERIC_BASE` 发射，进制由后续阶段按字面量首字符识别。
3. **数值后缀扫描**：在 `NUMERIC_BASE` 之后，按 `PREDEF_SCALED_UNIT → SCALE_FACTOR UNIT? → UNIT` 顺序做最长匹配以确定后缀；任何无法构成合法后缀的剩余字母不并入该字面量，而作为后续记号处理（通常随即因相邻 primary 触发语法错误，例如 `8ms` 切为 `8m` 与标识符 `s`）。`PREDEF_SCALED_UNIT` 命中后不再叠加 `SCALE_FACTOR` 或 `UNIT`。
4. **关键字与标识符**：先按 `IDENTIFIER` 最长匹配，再判定其是否为关键字（`if/then/elseif/else/AND/OR/NOT/EQUALS/NOTEQUALS/NULL`）。内建常量（如 `PI`、`e`、`ln10`）不是关键字，按普通标识符（`reference`）处理，其值在求值期解析。
5. **输入结束**：`<eof>` 表示输入耗尽，不对应任何字符记号。

## 2. 语法规则（BNF）

### 2.1 顶层

```bnf
<input> ::= <expr> <eof>
<expr> ::= <conditional_expr>
```

### 2.2 条件表达式

```bnf
<conditional_expr> ::= <logical_or_expr> <conditional_tail>
<conditional_tail> ::= OP_QMARK <expr> OP_COLON <logical_or_expr> <conditional_tail> | <empty>

<if_expr> ::= KW_IF LPAREN <expr> RPAREN KW_THEN <expr> <elseif_list> KW_ELSE <expr>
<elseif_list> ::= KW_ELSEIF LPAREN <expr> RPAREN KW_THEN <expr> <elseif_list> | <empty>
```

### 2.3 逻辑与按位层

```bnf
<logical_or_expr> ::= <logical_and_expr> <logical_or_tail>
<logical_or_tail> ::= OP_LOR <logical_and_expr> <logical_or_tail> | KW_OR <logical_and_expr> <logical_or_tail> | <empty>

<logical_and_expr> ::= <bit_or_expr> <logical_and_tail>
<logical_and_tail> ::= OP_LAND <bit_or_expr> <logical_and_tail> | KW_AND <bit_or_expr> <logical_and_tail> | <empty>

<bit_or_expr> ::= <bit_xor_expr> <bit_or_tail>
<bit_or_tail> ::= OP_BOR <bit_xor_expr> <bit_or_tail> | <empty>

<bit_xor_expr> ::= <bit_and_expr> <bit_xor_tail>
<bit_xor_tail> ::= OP_BXOR <bit_and_expr> <bit_xor_tail> | <empty>

<bit_and_expr> ::= <equality_expr> <bit_and_tail>
<bit_and_tail> ::= OP_BAND <equality_expr> <bit_and_tail> | <empty>
```

### 2.4 比较与移位层

```bnf
<equality_expr> ::= <relational_expr> <equality_tail>
<equality_tail> ::= OP_EQ <relational_expr> <equality_tail> | OP_NE <relational_expr> <equality_tail> | KW_EQUALS <relational_expr> <equality_tail> | KW_NOTEQUALS <relational_expr> <equality_tail> | <empty>

<relational_expr> ::= <shift_expr> <relational_tail>
<relational_tail> ::= OP_LT <shift_expr> <relational_tail> | OP_LE <shift_expr> <relational_tail> | OP_GT <shift_expr> <relational_tail> | OP_GE <shift_expr> <relational_tail> | <empty>

<shift_expr> ::= <additive_expr> <shift_tail>
<shift_tail> ::= OP_SHL <additive_expr> <shift_tail> | OP_SHR <additive_expr> <shift_tail> | <empty>
```

### 2.5 算术层

```bnf
<additive_expr> ::= <multiplicative_expr> <additive_tail>
<additive_tail> ::= OP_ADD <multiplicative_expr> <additive_tail> | OP_SUB <multiplicative_expr> <additive_tail> | <empty>

<multiplicative_expr> ::= <unary_expr> <multiplicative_tail>
<multiplicative_tail> ::= OP_MUL <unary_expr> <multiplicative_tail> | OP_DIV <unary_expr> <multiplicative_tail> | OP_MOD <unary_expr> <multiplicative_tail> | <empty>

<unary_expr> ::= <unary_op> <unary_expr> | <power_expr>
<unary_op> ::= OP_LNOT | KW_NOT | OP_BNOT | OP_SUB

<power_expr> ::= <postfix_expr> <power_tail>
<power_tail> ::= OP_POW <power_rhs> <power_tail> | <empty>
<power_rhs> ::= <unary_op> <power_rhs> | <postfix_expr>
```

### 2.6 后缀、主表达式与列表

```bnf
<postfix_expr> ::= <primary_expr> <postfix_tail>
<postfix_tail> ::= LBRACKET <index_list> RBRACKET <postfix_tail> | LPAREN <call_arg_list_opt> RPAREN <postfix_tail> | <empty>

<primary_expr> ::= <literal>
                 | <reference>
                 | <if_expr>
                 | LPAREN <expr> RPAREN
                 | <sweep_generator>
                 | <matrix_generator>

<seq_expr> ::= OP_SEQ | <expr> <seq_tail>
<seq_tail> ::= OP_SEQ <expr> <seq_stop_opt> | <empty>
<seq_stop_opt> ::= OP_SEQ <expr> | <empty>

<call_arg_list_opt> ::= <call_arg_list> | <empty>
<call_arg_list> ::= <call_arg_slot> <call_arg_tail>
<call_arg_tail> ::= COMMA <call_arg_slot> <call_arg_tail> | <empty>
<call_arg_slot> ::= <seq_expr> | <empty>

<sweep_generator> ::= LBRACKET <item_list> RBRACKET
<matrix_generator> ::= LBRACE <item_list> RBRACE

<index_list> ::= <item_list>
<item_list> ::= <seq_expr> <item_list_tail>
<item_list_tail> ::= COMMA <seq_expr> <item_list_tail> | <empty>
```

### 2.7 字面量与标识符

```bnf
<literal> ::= KW_NULL | <numeric_literal> | <string_literal> | <raw_string_literal>

<numeric_literal> ::= <numeric_base> <numeric_suffix_opt>
<numeric_suffix_opt> ::= <numeric_suffix> | <empty>
<numeric_suffix> ::= <predefined_scaled_unit> | <scale_factor> <unit_opt> | <unit>
<unit_opt> ::= <unit> | <empty>

<reference> ::= IDENTIFIER <reference_tail>
<reference_tail> ::= DOT IDENTIFIER <reference_tail>
                   | DDOT IDENTIFIER <reference_tail>
                   | <empty>

<numeric_base> ::= NUMERIC_BASE
<predefined_scaled_unit> ::= PREDEF_SCALED_UNIT
<scale_factor> ::= SCALE_FACTOR
<unit> ::= UNIT
<string_literal> ::= STRING_LITERAL
<raw_string_literal> ::= RAW_STRING_LITERAL
```

## 3. 语义说明

### 3.1 引用解析（\<reference\>）

`<reference>` 是点 (`.`) 或双点 (`..`) 分隔的标识符序列，对应 runtime 中的变量查找与 Dataset 路径导航。

底层 Dataset 为树形结构：Group（内部节点，仅含子 Group）→ Block（叶子节点，含独立/依赖变量 DataArray）。C++ API 使用 `/` 分隔路径段，REL 语法使用 `.`。

| 段数 | 文法形式 | 语义 |
|---|---|---|
| N≥3 | `a.b...x.y` | 尾部两段 `x.y` = Block 名 + DataArray 名；前面任意段为 Group 路径（对应 C++ `a/b/.../x`）。若首段为 Dataset 名则从该 Dataset 查找；否则从默认 Dataset 查找。 |
| 2 | `a..y` | 跨 Dataset 唯一变量查找：在 Dataset `a` 中查找全局唯一的 DataArray `y`。 |
| 1 | `y` | 默认 Dataset 中的变量查找：先查内建常量，再查 Dataset 中全局唯一的 DataArray `y`。 |

例：
- `noise.simulation.SP1.SP.Vout` → `GetDataArray("simulation/SP1/SP", "Vout")` on Dataset `"noise"`
- `simulation.SP1.SP.Vout` → 默认 Dataset 的同一路径
- `noise..Vout` → `GetDataArray("Vout")` on Dataset `"noise"`（全局唯一）
- `Vout` → 默认 Dataset 的 `GetDataArray("Vout")`（唯一时）
```
