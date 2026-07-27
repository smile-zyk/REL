// Scanner tests powered by GoogleTest.

#include "scanner/scanner.h"
#include "scanner/token.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

// Keep local helpers to minimize changes to existing test body style.
#define EXPECT(cond) EXPECT_TRUE(cond)
#define EXPECT_EQ_INT(a, b) EXPECT_EQ((a), (b))
#define EXPECT_EQ_STR(a, b) EXPECT_EQ(std::string(a), std::string(b))
#define RUN_TEST(name) name()

using rel::Scanner;
using rel::Token;
using rel::TokenType;

namespace
{
    std::vector<Token> scan(const std::string& src)
    {
        Scanner s(src);
        return s.scan_tokens();
    }
}

// ----------------------------------------------------------------------------
// Basic shape / EOF
// ----------------------------------------------------------------------------

void test_empty_source_emits_only_eof()
{
    auto t = scan("");
    EXPECT_EQ_INT(t.size(), 1u);
    EXPECT(t[0].type == TokenType::END_OF_INPUT);
}

void test_whitespace_only_source()
{
    auto t = scan(" \t\f\r");
    EXPECT_EQ_INT(t.size(), 1u);
    EXPECT(t[0].type == TokenType::END_OF_INPUT);
}

// ----------------------------------------------------------------------------
// Keywords vs identifiers (Spec 1.1 / 1.7 rule 4)
// ----------------------------------------------------------------------------

void test_all_keywords()
{
    auto t = scan("if then elseif else AND OR NOT EQUALS NOTEQUALS NULL");
    EXPECT_EQ_INT(t.size(), 11u);
    EXPECT(t[0].type == TokenType::KW_IF);
    EXPECT(t[1].type == TokenType::KW_THEN);
    EXPECT(t[2].type == TokenType::KW_ELSEIF);
    EXPECT(t[3].type == TokenType::KW_ELSE);
    EXPECT(t[4].type == TokenType::KW_AND);
    EXPECT(t[5].type == TokenType::KW_OR);
    EXPECT(t[6].type == TokenType::KW_NOT);
    EXPECT(t[7].type == TokenType::KW_EQUALS);
    EXPECT(t[8].type == TokenType::KW_NOTEQUALS);
    EXPECT(t[9].type == TokenType::KW_NULL);
    EXPECT(t[10].type == TokenType::END_OF_INPUT);
}

void test_builtin_constants_are_identifiers()
{
    auto t = scan("PI e ln10 reference");
    EXPECT_EQ_INT(t.size(), 5u);
    EXPECT(t[0].type == TokenType::IDENTIFIER);
    EXPECT_EQ_STR(t[0].lexeme, "PI");
    EXPECT(t[1].type == TokenType::IDENTIFIER);
    EXPECT_EQ_STR(t[1].lexeme, "e");
    EXPECT(t[2].type == TokenType::IDENTIFIER);
    EXPECT_EQ_STR(t[2].lexeme, "ln10");
}

void test_keywords_are_case_sensitive()
{
    // Lowercase versions of the all-caps keywords are plain identifiers.
    auto t = scan("and or not equals notequals null");
    EXPECT_EQ_INT(t.size(), 7u);
    for (int i = 0; i < 6; ++i) EXPECT(t[i].type == TokenType::IDENTIFIER);
}

// ----------------------------------------------------------------------------
// Operators (Spec 1.2 + 1.7 rule 1)
// ----------------------------------------------------------------------------

void test_multi_char_operators()
{
    auto t = scan("** :: << >> >= <= == != && || ..");
    EXPECT(t[0].type == TokenType::OP_POW);
    EXPECT(t[1].type == TokenType::OP_SEQ);
    EXPECT(t[2].type == TokenType::OP_SHL);
    EXPECT(t[3].type == TokenType::OP_SHR);
    EXPECT(t[4].type == TokenType::OP_GE);
    EXPECT(t[5].type == TokenType::OP_LE);
    EXPECT(t[6].type == TokenType::OP_EQ);
    EXPECT(t[7].type == TokenType::OP_NE);
    EXPECT(t[8].type == TokenType::OP_LAND);
    EXPECT(t[9].type == TokenType::OP_LOR);
    EXPECT(t[10].type == TokenType::DDOT);
}

void test_single_char_operators()
{
    auto t = scan("+ - * / % ^ | & ~ ! ? : < >");
    TokenType expected[] = {
        TokenType::OP_ADD, TokenType::OP_SUB, TokenType::OP_MUL, TokenType::OP_DIV,
        TokenType::OP_MOD, TokenType::OP_BXOR, TokenType::OP_BOR, TokenType::OP_BAND,
        TokenType::OP_BNOT, TokenType::OP_LNOT, TokenType::OP_QMARK, TokenType::OP_COLON,
        TokenType::OP_LT, TokenType::OP_GT,
    };
    for (std::size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
    {
        EXPECT(t[i].type == expected[i]);
    }
}

void test_maximal_munch_packed_operators()
{
    // No spaces between operators; the scanner must split greedily.
    auto t = scan("**::==!=");
    EXPECT(t[0].type == TokenType::OP_POW);
    EXPECT(t[1].type == TokenType::OP_SEQ);
    EXPECT(t[2].type == TokenType::OP_EQ);
    EXPECT(t[3].type == TokenType::OP_NE);
}

void test_delimiters()
{
    auto t = scan("()[]{},");
    EXPECT(t[0].type == TokenType::LPAREN);
    EXPECT(t[1].type == TokenType::RPAREN);
    EXPECT(t[2].type == TokenType::LBRACKET);
    EXPECT(t[3].type == TokenType::RBRACKET);
    EXPECT(t[4].type == TokenType::LBRACE);
    EXPECT(t[5].type == TokenType::RBRACE);
    EXPECT(t[6].type == TokenType::COMMA);
}

// ----------------------------------------------------------------------------
// Dot vs DDOT (Spec 1.7 rule 1) and the leading-dot real edge case
// ----------------------------------------------------------------------------

void test_dot_member_access_vs_cross_dataset()
{
    auto t = scan("a.b a..b");
    EXPECT(t[0].type == TokenType::IDENTIFIER);
    EXPECT(t[1].type == TokenType::DOT);
    EXPECT(t[2].type == TokenType::IDENTIFIER);
    EXPECT(t[3].type == TokenType::IDENTIFIER);
    EXPECT(t[4].type == TokenType::DDOT);
    EXPECT(t[5].type == TokenType::IDENTIFIER);
}

void test_ddot_wins_against_real_in_5dotdot7()
{
    // "5..7" must split as int, DDOT, int (range-like), not as "5." + "." + "7".
    auto t = scan("5..7");
    EXPECT_EQ_INT(t.size(), 4u);
    EXPECT(t[0].type == TokenType::NUMERIC_BASE);
    EXPECT_EQ_STR(t[0].lexeme, "5");
    EXPECT(t[1].type == TokenType::DDOT);
    EXPECT(t[2].type == TokenType::NUMERIC_BASE);
    EXPECT_EQ_STR(t[2].lexeme, "7");
}

void test_leading_dot_real_literal()
{
    auto t = scan(".5 .25e6");
    EXPECT(t[0].type == TokenType::NUMERIC_BASE);
    EXPECT_EQ_STR(t[0].lexeme, ".5");
    EXPECT_EQ_STR(t[1].lexeme, ".25e6");
}

// ----------------------------------------------------------------------------
// Numeric literals (Spec 1.4 / 1.7 rule 2)
// ----------------------------------------------------------------------------

void test_integer_literals()
{
    auto t = scan("0 123 0xFF 0x1f 077");
    EXPECT_EQ_STR(t[0].lexeme, "0");
    EXPECT_EQ_STR(t[1].lexeme, "123");
    EXPECT_EQ_STR(t[2].lexeme, "0xFF");
    EXPECT_EQ_STR(t[3].lexeme, "0x1f");
    EXPECT_EQ_STR(t[4].lexeme, "077");
    for (int i = 0; i < 5; ++i) EXPECT(t[i].type == TokenType::NUMERIC_BASE);
}

void test_real_literals()
{
    auto t = scan("0.5 1.5e-3 1e5 1e+5");
    EXPECT_EQ_STR(t[0].lexeme, "0.5");
    EXPECT_EQ_STR(t[1].lexeme, "1.5e-3");
    EXPECT_EQ_STR(t[2].lexeme, "1e5");
    EXPECT_EQ_STR(t[3].lexeme, "1e+5");
}

void test_imaginary_literals()
{
    auto t = scan("2i 3.5i 1e5i");
    EXPECT_EQ_STR(t[0].lexeme, "2i");
    EXPECT_EQ_STR(t[1].lexeme, "3.5i");
    EXPECT_EQ_STR(t[2].lexeme, "1e5i");
}

void test_exponent_rollback_on_missing_digits()
{
    // "1e" is NOT a valid real; the scanner must back off so the "e" becomes
    // its own identifier.
    auto t = scan("1e");
    EXPECT_EQ_INT(t.size(), 3u);
    EXPECT(t[0].type == TokenType::NUMERIC_BASE);
    EXPECT_EQ_STR(t[0].lexeme, "1");
    EXPECT(t[1].type == TokenType::IDENTIFIER);
    EXPECT_EQ_STR(t[1].lexeme, "e");
}

void test_hex_does_not_eat_trailing_i()
{
    // IMAG_NUM is decimal-or-real plus 'i'; hex literals carry no imaginary.
    auto t = scan("0x1Fi");
    EXPECT(t[0].type == TokenType::NUMERIC_BASE);
    EXPECT_EQ_STR(t[0].lexeme, "0x1F");
    EXPECT(t[1].type == TokenType::IDENTIFIER);
    EXPECT_EQ_STR(t[1].lexeme, "i");
}

// ----------------------------------------------------------------------------
// Numeric suffix (Spec 1.5 / 1.7 rule 3)
// ----------------------------------------------------------------------------

void test_unit_alone()
{
    auto t = scan("8Hz");
    EXPECT_EQ_INT(t.size(), 3u);
    EXPECT(t[0].type == TokenType::NUMERIC_BASE);
    EXPECT(t[1].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[1].lexeme, "Hz");
}

void test_scale_factor_with_unit()
{
    auto t = scan("8kHz");
    EXPECT_EQ_INT(t.size(), 3u);
    EXPECT(t[0].type == TokenType::NUMERIC_BASE);
    EXPECT(t[1].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[1].lexeme, "kHz");
}

void test_scale_factor_alone()
{
    auto t = scan("8M");
    EXPECT_EQ_INT(t.size(), 3u);
    EXPECT(t[0].type == TokenType::NUMERIC_BASE);
    EXPECT(t[1].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[1].lexeme, "M");
}

void test_predef_scaled_unit()
{
    auto t = scan("8mil 8mils 8cm 8dB");
    EXPECT(t[1].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[1].lexeme, "mil");
    EXPECT(t[3].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[3].lexeme, "mils");
    EXPECT(t[5].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[5].lexeme, "cm");
    EXPECT(t[7].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[7].lexeme, "dB");
}

void test_spec_example_8ms_splits_as_8_m_and_s()
{
    // Spec 1.7 rule 3 example: "8ms" -> "8" + "m" + identifier "s".
    auto t = scan("8ms");
    EXPECT_EQ_INT(t.size(), 4u);
    EXPECT(t[0].type == TokenType::NUMERIC_BASE);
    EXPECT_EQ_STR(t[0].lexeme, "8");
    EXPECT(t[1].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[1].lexeme, "m");
    EXPECT(t[2].type == TokenType::IDENTIFIER);
    EXPECT_EQ_STR(t[2].lexeme, "s");
}

void test_unit_longest_match_meters_over_meter()
{
    auto t = scan("8meters 8meter");
    EXPECT(t[1].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[1].lexeme, "meters");
    EXPECT(t[3].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[3].lexeme, "meter");
}

void test_predef_overrides_scale_factor_form()
{
    // "mil" (3 chars, PREDEF) beats SF "m" + nothing (1 char).
    auto t = scan("8mil");
    EXPECT_EQ_INT(t.size(), 3u);
    EXPECT(t[1].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[1].lexeme, "mil");
}

void test_predef_followed_by_identifier_chars()
{
    // "8milky" -> "8" + NUMERIC_SUFFIX "mil" + IDENTIFIER "ky".
    auto t = scan("8milky");
    EXPECT_EQ_INT(t.size(), 4u);
    EXPECT(t[0].type == TokenType::NUMERIC_BASE);
    EXPECT(t[1].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[1].lexeme, "mil");
    EXPECT(t[2].type == TokenType::IDENTIFIER);
    EXPECT_EQ_STR(t[2].lexeme, "ky");
}

void test_unit_alone_meter_not_scale_factor_m_eter()
{
    // Although "m" is a SCALE_FACTOR, UNIT "meter" (5 chars) is longer than
    // SF "m" + (no unit, since "eter" is not a UNIT).
    auto t = scan("8meter");
    EXPECT_EQ_INT(t.size(), 3u);
    EXPECT(t[1].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[1].lexeme, "meter");
}

// ----------------------------------------------------------------------------
// String / raw-string literals (Spec 1.3)
// ----------------------------------------------------------------------------

void test_simple_string_literal()
{
    auto t = scan("\"hello world\"");
    EXPECT(t[0].type == TokenType::STRING_LITERAL);
    EXPECT_EQ_STR(t[0].lexeme, "\"hello world\"");
}

void test_string_with_escapes()
{
    // Body contains \n, \", \x1F and \0123 (octal: \0 + 3 octal digits).
    auto t = scan("\"a\\nb\\\"c\\x1Fd\\0123e\"");
    EXPECT(t[0].type == TokenType::STRING_LITERAL);
}

void test_unterminated_string_is_invalid()
{
    auto t = scan("\"oops");
    EXPECT(t[0].type == TokenType::INVALID);
}

void test_bad_escape_is_invalid()
{
    auto t = scan("\"\\q\"");
    EXPECT(t[0].type == TokenType::INVALID);
}

void test_raw_string_literal()
{
    auto t = scan("''verbatim \\n no escapes''");
    EXPECT(t[0].type == TokenType::RAW_STRING_LITERAL);
    EXPECT_EQ_STR(t[0].lexeme, "''verbatim \\n no escapes''");
}

// ----------------------------------------------------------------------------
// Error path
// ----------------------------------------------------------------------------

void test_unknown_character_is_invalid()
{
    auto t = scan("@");
    EXPECT(t[0].type == TokenType::INVALID);
    EXPECT_EQ_STR(t[0].lexeme, "@");
}

void test_bare_equals_is_invalid()
{
    // Spec has no bare '=' token, only '=='.
    auto t = scan("=");
    EXPECT(t[0].type == TokenType::INVALID);
}

// ----------------------------------------------------------------------------
// Position tracking
// ----------------------------------------------------------------------------

void test_line_and_column_tracking()
{
    auto t = scan("if x + 1");
    EXPECT_EQ_INT(t[0].line, 1);
    EXPECT_EQ_INT(t[0].column, 1);  // "if"
    EXPECT_EQ_INT(t[1].column, 4);  // "x"
    EXPECT_EQ_INT(t[2].column, 6);  // "+"
    EXPECT_EQ_INT(t[3].column, 8);  // "1"
}

void test_initial_line_offset()
{
    Scanner s("foo", 42);
    auto t = s.scan_tokens();
    EXPECT_EQ_INT(t[0].line, 42);
    EXPECT_EQ_INT(t[0].column, 1);
}

// ----------------------------------------------------------------------------
// A realistic expression
// ----------------------------------------------------------------------------

void test_realistic_if_expression()
{
    auto t = scan("if (x >= 8kHz) then 1.5e-3 else NULL");
    EXPECT(t[0].type == TokenType::KW_IF);
    EXPECT(t[1].type == TokenType::LPAREN);
    EXPECT(t[2].type == TokenType::IDENTIFIER);
    EXPECT_EQ_STR(t[2].lexeme, "x");
    EXPECT(t[3].type == TokenType::OP_GE);
    EXPECT(t[4].type == TokenType::NUMERIC_BASE);
    EXPECT_EQ_STR(t[4].lexeme, "8");
    EXPECT(t[5].type == TokenType::NUMERIC_SUFFIX);
    EXPECT_EQ_STR(t[5].lexeme, "kHz");
    EXPECT(t[6].type == TokenType::RPAREN);
    EXPECT(t[7].type == TokenType::KW_THEN);
    EXPECT(t[8].type == TokenType::NUMERIC_BASE);
    EXPECT_EQ_STR(t[8].lexeme, "1.5e-3");
    EXPECT(t[9].type == TokenType::KW_ELSE);
    EXPECT(t[10].type == TokenType::KW_NULL);
    EXPECT(t[11].type == TokenType::END_OF_INPUT);
}

TEST(ScannerTest, AllCases)
{
    RUN_TEST(test_empty_source_emits_only_eof);
    RUN_TEST(test_whitespace_only_source);

    RUN_TEST(test_all_keywords);
    RUN_TEST(test_builtin_constants_are_identifiers);
    RUN_TEST(test_keywords_are_case_sensitive);

    RUN_TEST(test_multi_char_operators);
    RUN_TEST(test_single_char_operators);
    RUN_TEST(test_maximal_munch_packed_operators);
    RUN_TEST(test_delimiters);

    RUN_TEST(test_dot_member_access_vs_cross_dataset);
    RUN_TEST(test_ddot_wins_against_real_in_5dotdot7);
    RUN_TEST(test_leading_dot_real_literal);

    RUN_TEST(test_integer_literals);
    RUN_TEST(test_real_literals);
    RUN_TEST(test_imaginary_literals);
    RUN_TEST(test_exponent_rollback_on_missing_digits);
    RUN_TEST(test_hex_does_not_eat_trailing_i);

    RUN_TEST(test_unit_alone);
    RUN_TEST(test_scale_factor_with_unit);
    RUN_TEST(test_scale_factor_alone);
    RUN_TEST(test_predef_scaled_unit);
    RUN_TEST(test_spec_example_8ms_splits_as_8_m_and_s);
    RUN_TEST(test_unit_longest_match_meters_over_meter);
    RUN_TEST(test_predef_overrides_scale_factor_form);
    RUN_TEST(test_predef_followed_by_identifier_chars);
    RUN_TEST(test_unit_alone_meter_not_scale_factor_m_eter);

    RUN_TEST(test_simple_string_literal);
    RUN_TEST(test_string_with_escapes);
    RUN_TEST(test_unterminated_string_is_invalid);
    RUN_TEST(test_bad_escape_is_invalid);
    RUN_TEST(test_raw_string_literal);

    RUN_TEST(test_unknown_character_is_invalid);
    RUN_TEST(test_bare_equals_is_invalid);

    RUN_TEST(test_line_and_column_tracking);
    RUN_TEST(test_initial_line_offset);

    RUN_TEST(test_realistic_if_expression);
}
