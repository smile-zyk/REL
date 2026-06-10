// Entry point for the REL parser driver.
//
// Modes:
//   - REPL mode (no arguments): read one expression per line, parse it,
//     and print the AST.
//   - File mode (one argument): parse each non-empty line from a file.

#include "ast/ast_printer.h"
#include "parser/parser.h"
#include "scanner/scanner.h"

#include <fstream>
#include <iostream>
#include <string>

namespace
{
    int parse_and_print(const std::string& source, int line_no)
    {
        rel::Scanner scanner(source, line_no);
        rel::Parser parser(scanner.scan_tokens());
        rel::ParseResult result = parser.parse();

        if (!result.ok())
        {
            for (std::size_t i = 0; i < result.errors.size(); ++i)
            {
                const rel::ParseError& err = result.errors[i];
                std::cerr << "parse error " << err.line << ':' << err.column << ": "
                          << err.message << '\n';
            }
            return 1;
        }

        rel::AstPrinter printer;
        std::cout << printer.print(*result.expr) << '\n';
        return 0;
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
                if (parse_and_print(line, line_no) != 0) return 1;
            }
            ++line_no;
        }
        return 0;
    }

    int run_repl()
    {
        std::cout << "REL parser REPL. Each line is parsed independently.\n"
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
            parse_and_print(line, line_no);
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
