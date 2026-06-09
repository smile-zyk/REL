#pragma once

#include "token.h"

#include <cstddef>
#include <string>
#include <vector>

namespace rel
{
    // Hand-written single-pass scanner that turns a string of source text
    // into a list of tokens defined in REL_Formal_Spec.md sections 1 and 2.
    //
    // Usage:
    //   Scanner s(line, line_no);
    //   for (const auto& t : s.scan_tokens()) { ... }
    //
    // The scanner is one-shot: construct, call scan_tokens() once, then
    // discard. The returned vector always ends with one END_OF_INPUT token.
    // Lexical errors produce INVALID tokens instead of throwing so the
    // parser can collect more than one diagnostic per source line.
    class Scanner
    {
    public:
        // `initial_line` lets a driver feed the scanner one source line at
        // a time (REPL / file mode) while still producing useful
        // line:column information.
        explicit Scanner(std::string source, int initial_line = 1);

        std::vector<Token> scan_tokens();

    private:
        // --- Driver ------------------------------------------------------
        void scan_one_token();
        void mark_token_start();

        // --- Cursor helpers ---------------------------------------------
        bool is_at_end() const;
        char peek(std::size_t offset = 0) const;
        char advance();
        bool match(char expected);

        // --- Token emission ---------------------------------------------
        void emit(TokenType type);
        void emit(TokenType type, std::string lexeme);

        // --- Sub-scanners -----------------------------------------------
        void scan_identifier_or_keyword();
        void scan_string_literal();
        void scan_raw_string_literal();
        void scan_numeric_literal();
        void scan_numeric_base();
        void scan_numeric_suffix();

        // --- State ------------------------------------------------------
        std::string source_;
        std::vector<Token> tokens_;
        std::size_t start_ = 0;       // start offset of the current token
        std::size_t current_ = 0;     // next char to consume
        std::size_t line_start_ = 0;  // offset of the current line's first char
        int line_ = 1;
        int start_line_ = 1;
        int start_col_ = 1;
    };
} // namespace rel
