// Entry point for the REL scanner driver.
//
// Modes:
//   - REPL mode (no arguments): read a single expression per line from stdin
//     and print the tokens it produces.
//   - File mode (one argument):  treat the argument as a path; read the file
//     line by line and tokenise each line independently.
//
// REL expressions are single-line by design, so both modes share the same
// "one line in, one token stream out" loop.

#include "scanner/scanner.h"
#include "scanner/token.h"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    // Render one line's worth of tokens in a debug-friendly, column-aligned
    // format. We compose it here (rather than in token.cc) because this is
    // really a driver concern - the scanner library itself only exposes the
    // diagnostic form via to_string(Token).
    std::string format_token_for_dump(const rel::Token& tok)
    {
        constexpr std::size_t kTypeColumnWidth = 20;
        std::string type_name = rel::to_string(tok.type);
        if (type_name.size() < kTypeColumnWidth)
        {
            type_name.append(kTypeColumnWidth - type_name.size(), ' ');
        }
        else
        {
            type_name.push_back(' ');
        }

        std::ostringstream oss;
        oss << '[' << std::setw(3) << tok.line << ':' << std::setw(3) << tok.column
            << "] " << type_name << '`' << tok.lexeme << '`';
        return oss.str();
    }

    void scan_and_print(const std::string& source, int line_no)
    {
        rel::Scanner scanner(source, line_no);
        for (const auto& tok : scanner.scan_tokens())
        {
            std::cout << format_token_for_dump(tok) << '\n';
        }
    }

    int run_file(const char* path)
    {
        std::ifstream file(path);
        if (!file)
        {
            std::cerr << "rel: cannot open file '" << path << "'\n";
            return 1;
        }

        std::string line;
        int line_no = 1;
        while (std::getline(file, line))
        {
            if (!line.empty())
            {
                std::cout << "--- line " << line_no << ": " << line << '\n';
                scan_and_print(line, line_no);
            }
            ++line_no;
        }
        return 0;
    }

    int run_repl()
    {
        std::cout << "REL scanner REPL. Each line is scanned independently.\n"
                  << "Press Ctrl+Z then Enter (Windows) or Ctrl+D (Unix) to exit.\n";

        std::string line;
        int line_no = 1;
        while (true)
        {
            std::cout << ">>> " << std::flush;
            if (!std::getline(std::cin, line))
            {
                std::cout << '\n';
                break;
            }
            if (line.empty()) continue;
            scan_and_print(line, line_no);
            ++line_no;
        }
        return 0;
    }
}

int main(int argc, char** argv)
{
    if (argc > 2)
    {
        std::cerr << "usage: " << (argc > 0 ? argv[0] : "rel") << " [path-to-file]\n";
        return 2;
    }
    if (argc == 2) return run_file(argv[1]);
    return run_repl();
}
