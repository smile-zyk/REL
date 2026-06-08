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
STRING_RAW_DQ2        = ''(?:[\s\S]*?)''
```

### 1.4 数值字面量（基础）

```text
INT_DEC               = (?:0|[1-9][0-9]*)
INT_HEX               = (?:0[xX][0-9A-Fa-f]+)
INT_OCT               = (?:0[0-7]+)

REAL_NUM              = (?:(?:[0-9]+\.[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)
IMAG_NUM              = (?:(?:(?:[0-9]+\.[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)i)

NUMERIC_BASE          = (?:(?:0[xX][0-9A-Fa-f]+)|(?:0[0-7]+)|(?:0|[1-9][0-9]*)|(?:(?:[0-9]+\.[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)|(?:(?:(?:[0-9]+\.[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)i))
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

## 2. 语法规则（BNF）

### 2.1 顶层

```bnf
<input> ::= <expr> <eof>
<expr> ::= <conditional_expr>
```

### 2.2 条件表达式

```bnf
<conditional_expr> ::= <if_expr> | <logical_or_expr> <conditional_tail>
<conditional_tail> ::= OP_QMARK <expr> OP_COLON <conditional_expr> | <empty>

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

<bit_xor_expr> ::= <equality_expr> <bit_xor_tail>
<bit_xor_tail> ::= OP_BXOR <equality_expr> <bit_xor_tail> | <empty>
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
<power_tail> ::= OP_POW <power_expr> | <empty>
```

### 2.6 后缀、主表达式与列表

```bnf
<postfix_expr> ::= <primary_expr> <postfix_tail>
<postfix_tail> ::= LBRACKET <index_list> RBRACKET <postfix_tail> | LPAREN <call_arg_list_opt> RPAREN <postfix_tail> | <empty>

<primary_expr> ::= <literal>
                 | <reference>
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
<string_literal> ::= STRING_DQ
<raw_string_literal> ::= STRING_RAW_DQ2
```
