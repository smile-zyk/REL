# REL Formal Specification

本文档用于实现对齐、语法定义与测试基线制定。说明性内容请参考 `REL.md`。

## 1. 词法规则（Regex）

说明：
- 本节给出词法层面的推荐正则表达式。
- 除特别说明外，全部大小写敏感。
- 记号匹配采用最长匹配原则；同长度时按词法优先级处理（关键字优先于标识符）。
- 词法层只产出基础 token（如 `IDENTIFIER`、`DOT`、`DDOT`），节点引用由语法层组合。

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
OP_BNOT               = ~
OP_LNOT               = !
OP_QMARK              = \?
OP_COLON              = :
OP_ASSIGN             = =

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
STRING_DQ             = "(?:\\[nrfbt"\\]|\\x[0-9A-Fa-f]{2}|\\0[0-7]{3}|[^"\\])*"
STRING_RAW_DQ2        = ''(?:.|\r|\n)*?''
```

说明：
- `STRING_DQ` 支持转义：`\n \r \f \b \t \" \\ \xNN \0NNN`。
- `STRING_RAW_DQ2` 不做任何转义解释，仅在遇到下一对 `''` 时终止。
- 对不支持非贪婪正则的 lexer，实现时应采用状态机扫描：识别到起始 `''` 后逐字符前进，遇到下一对 `''` 结束。

### 1.4 数值字面量（基础）

```text
INT_DEC               = (?:0|[1-9][0-9]*)
INT_HEX               = 0[xX][0-9A-Fa-f]+
INT_OCT               = 0[0-7]+

REAL_NUM              = (?:(?:[0-9]+\.[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)
IMAG_NUM              = (?:(?:(?:[0-9]+\.[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)i)

NUMERIC_BASE          = (?:INT_HEX|INT_OCT|INT_DEC|REAL_NUM|IMAG_NUM)
```

### 1.5 数值后缀（缩放因子/单位）

```text
SCALE_FACTOR          = (?:T|G|M|K|k|_|m|u|n|p|f|a)
UNIT                  = (?:Hz|Ohm|Ohms|S|F|H|meter|meters|metre|metres|sec|V|A|W)
PREDEF_SCALED_UNIT    = (?:mil|mils|in|ft|mi|cm|PHz|dB|nmi)

NUMERIC_SUFFIX        = (?:(?:PREDEF_SCALED_UNIT)|(?:SCALE_FACTOR(?:UNIT)?)|(?:UNIT))
NUMERIC_LITERAL       = NUMERIC_BASE NUMERIC_SUFFIX?
```

语义约束：
- 命中 `PREDEF_SCALED_UNIT` 后，不允许再叠加 `SCALE_FACTOR` 或 `UNIT`。
- `SCALE_FACTOR` 与 `UNIT` 同时出现时，顺序必须为 `SCALE_FACTOR` 在前、`UNIT` 在后。

### 1.6 仿真节点引用（语法层组合）

```text
NODE_REF_FULL         = IDENTIFIER DOT IDENTIFIER DOT IDENTIFIER DOT IDENTIFIER
NODE_REF_DS_UNIQUE    = IDENTIFIER DDOT IDENTIFIER
NODE_REF_DEFAULT      = IDENTIFIER DOT IDENTIFIER DOT IDENTIFIER
```

说明：
- scanner 不直接产出 `NODE_REF_*` token。
- parser 基于 `IDENTIFIER`/`DOT`/`DDOT` 组合出节点引用结构。
- `node_ref_simple` 与 `identifier` 词法重叠，统一在语义阶段判定。

### 1.7 空白与注释

```text
NEWLINE               = (?:\r\n|\n|\r)+
WS                    = [ \t\f]+
```

说明：
- 词法层忽略 `WS`。
- 本语言不支持注释，`//` 与 `/* ... */` 不是合法语法。

## 2. 语法规则（BNF）

说明：
- 本节为表达式核心文法。
- 当运算符在 C 语言中存在时，其优先级与结合性与 C 对齐。
- REL 扩展运算符：`**`（右结合）、`::`。
- 逗号仅用于列表分隔，不是独立运算符。
- `<empty>` 表示空串（epsilon）。
- `::` 用于序列生成：`start::stop` 与 `start::step::stop`。

### 2.1 顶层

```bnf
<input> ::= <expr>
<expr> ::= <seq_expr>
```

说明：
- 形如 `identifier = expr` 的“方程绑定”属于宿主环境语法，用于保存表达式计算结果，不属于 REL 核心语法。

### 2.2 序列表达式（REL 扩展）

```bnf
<seq_expr> ::= <range_expr> | <conditional_expr>
<range_expr> ::= <conditional_expr> "::" <conditional_expr>
               | <conditional_expr> "::" <conditional_expr> "::" <conditional_expr>
```

说明：
- 两段式：`a::c`，其中 `a` 为 `start`，`c` 为 `stop`。
- 三段式：`a::b::c`，其中 `a` 为 `start`，`b` 为 `step`，`c` 为 `stop`。

### 2.3 条件表达式

```bnf
<conditional_expr> ::= <logical_or_expr> <conditional_tail>
<conditional_tail> ::= "?" <expr> ":" <conditional_expr> | <empty>

<if_expr> ::= "if" "(" <expr> ")" "then" <expr> <elseif_list> "else" <expr>
<elseif_list> ::= "elseif" "(" <expr> ")" "then" <expr> <elseif_list> | <empty>
```

### 2.4 逻辑与按位层

```bnf
<logical_or_expr> ::= <logical_and_expr> <logical_or_tail>
<logical_or_tail> ::= "||" <logical_and_expr> <logical_or_tail> | "OR" <logical_and_expr> <logical_or_tail> | <empty>

<logical_and_expr> ::= <bit_or_expr> <logical_and_tail>
<logical_and_tail> ::= "&&" <bit_or_expr> <logical_and_tail> | "AND" <bit_or_expr> <logical_and_tail> | <empty>

<bit_or_expr> ::= <bit_xor_expr> <bit_or_tail>
<bit_or_tail> ::= "|" <bit_xor_expr> <bit_or_tail> | <empty>

<bit_xor_expr> ::= <equality_expr> <bit_xor_tail>
<bit_xor_tail> ::= "^" <equality_expr> <bit_xor_tail> | <empty>
```

### 2.5 比较与移位层

```bnf
<equality_expr> ::= <relational_expr> <equality_tail>
<equality_tail> ::= "==" <relational_expr> <equality_tail> | "!=" <relational_expr> <equality_tail> | "EQUALS" <relational_expr> <equality_tail> | "NOTEQUALS" <relational_expr> <equality_tail> | <empty>

<relational_expr> ::= <shift_expr> <relational_tail>
<relational_tail> ::= "<" <shift_expr> <relational_tail> | "<=" <shift_expr> <relational_tail> | ">" <shift_expr> <relational_tail> | ">=" <shift_expr> <relational_tail> | <empty>

<shift_expr> ::= <additive_expr> <shift_tail>
<shift_tail> ::= "<<" <additive_expr> <shift_tail> | ">>" <additive_expr> <shift_tail> | <empty>
```

### 2.6 算术层

```bnf
<additive_expr> ::= <multiplicative_expr> <additive_tail>
<additive_tail> ::= "+" <multiplicative_expr> <additive_tail> | "-" <multiplicative_expr> <additive_tail> | <empty>

<multiplicative_expr> ::= <unary_expr> <multiplicative_tail>
<multiplicative_tail> ::= "*" <unary_expr> <multiplicative_tail> | "/" <unary_expr> <multiplicative_tail> | "%" <unary_expr> <multiplicative_tail> | <empty>

<unary_expr> ::= <unary_op> <unary_expr> | <power_expr>
<unary_op> ::= "!" | "NOT" | "~" | "-"

<power_expr> ::= <postfix_expr> <power_tail>
<power_tail> ::= "**" <power_expr> | <empty>
```

### 2.7 后缀、主表达式与列表

```bnf
<postfix_expr> ::= <primary_expr> <postfix_tail>
<postfix_tail> ::= "[" <index_list> "]" <postfix_tail> | <empty>

<primary_expr> ::= <literal>
                 | <identifier>
                 | <qualified_node_ref>
                 | <function_call>
                 | <if_expr>
                 | "(" <expr> ")"
                 | "[" <expr_list> "]"
                 | "{" <expr_list> "}"

<function_call> ::= <identifier> "(" <opt_expr_list> ")"
<opt_expr_list> ::= <expr_list> | <empty>

<expr_list> ::= <expr> <expr_list_tail>
<expr_list_tail> ::= "," <expr> <expr_list_tail> | <empty>

<index_list> ::= <index_item> <index_list_tail>
<index_list_tail> ::= "," <index_item> <index_list_tail> | <empty>
<index_item> ::= <expr> | <range_selector>
<range_selector> ::= "::"
                   | <expr> "::" <expr>
                   | <expr> "::" <expr> "::" <expr>
```

说明：
- `index_list` 中每个元素都可以是普通表达式，或序列选择器。
- 序列选择器支持 `::`、`start::stop`、`start::step::stop`（例如 `1::2::3`）。

### 2.8 字面量与标识符

```bnf
<literal> ::= "NULL" | <numeric_literal> | <string_literal> | <raw_string_literal>

<numeric_literal> ::= <numeric_base> <numeric_suffix_opt>
<numeric_suffix_opt> ::= <numeric_suffix> | <empty>
<numeric_suffix> ::= <predefined_scaled_unit> | <scale_factor> <unit_opt> | <unit>
<unit_opt> ::= <unit> | <empty>

<qualified_node_ref> ::= <identifier> "." "." <identifier>
                       | <identifier> "." <identifier> "." <identifier>
                       | <identifier> "." <identifier> "." <identifier> "." <identifier>

<identifier> ::= IDENTIFIER
<numeric_base> ::= NUMERIC_BASE
<predefined_scaled_unit> ::= PREDEF_SCALED_UNIT
<scale_factor> ::= SCALE_FACTOR
<unit> ::= UNIT
<string_literal> ::= STRING_DQ
<raw_string_literal> ::= STRING_RAW_DQ2
```

## 3. 附加约束

- REL 语言整体大小写敏感。
- 逗号只在 `expr_list` 中有效，不作为独立求值运算符。
- 单独的 `IDENTIFIER` 在语义阶段可被解析为函数名、内建名或 `node_ref_simple`。
- 方程绑定（`identifier = expr`）由宿主环境处理，REL 仅定义表达式计算语义。
- `index_list` 中允许裸 `::`，表示该维度选择整个 sweep 轴（如 `a[1,::,2]`）。
