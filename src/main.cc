// Entry point for the REL interpreter.
//
// Modes:
//   - REPL mode (no arguments): read one expression or binding per line,
//     evaluate it, and print the result.
//   - File mode (one argument): evaluate each non-empty line from a file.

#include "eval/environment.h"
#include "eval/evaluator.h"
#include "parser/parser.h"
#include "scanner/scanner.h"

#ifdef _WIN32
// No readline on Windows yet — fall back to std::getline.
#elif __APPLE__
#include <editline/readline.h>
#else
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
    bool is_ident_start(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    bool is_ident_char(char c)
    {
        return is_ident_start(c) || (c >= '0' && c <= '9');
    }

    bool is_valid_identifier(const std::string& name)
    {
        if (name.empty() || !is_ident_start(name[0]))
            return false;
        for (std::size_t i = 1; i < name.size(); ++i)
            if (!is_ident_char(name[i]))
                return false;
        if (name == "if" || name == "then" || name == "elseif" || name == "else" ||
            name == "AND" || name == "OR" || name == "NOT" ||
            name == "EQUALS" || name == "NOTEQUALS" || name == "NULL")
            return false;
        return true;
    }

    std::size_t find_binding_eq(const std::string& line)
    {
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            if (line[i] != '=') continue;
            if (i > 0)
            {
                char prev = line[i - 1];
                if (prev == '=' || prev == '!' || prev == '<' || prev == '>')
                    continue;
            }
            if (i + 1 < line.size() && line[i + 1] == '=')
                continue;
            return i;
        }
        return std::string::npos;
    }

    std::string trim(const std::string& s)
    {
        std::size_t b = 0;
        while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
        std::size_t e = s.size();
        while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
        return s.substr(b, e - b);
    }

    int parse_and_eval(rel::Environment& env,
                       const std::string& source,
                       int line_no)
    {
        rel::Scanner scanner(source, line_no);
        rel::Parser parser(scanner.scan_tokens());
        rel::ParseResult result = parser.parse();

        if (!result.ok())
        {
            for (std::size_t i = 0; i < result.errors.size(); ++i)
            {
                const rel::ParseError& err = result.errors[i];
                const char* label = (err.kind == rel::ErrorKind::Lexical)
                                   ? "lexical error"
                                   : "syntax error";
            std::cerr << label << ": line " << err.line << ", column "
                      << err.column << ": " << err.message << '\n';
            }
            return 1;
        }

        rel::Evaluator evaluator(env);
        rel::Value value;
        try {
            value = evaluator.evaluate(*result.expr);
        } catch (const std::exception& e) {
            std::cerr << "runtime error: " << e.what() << std::endl;
            return 1;
        }
        std::cout << value.to_string() << '\n';
        return 0;
    }

    int eval_line(rel::Environment& env, const std::string& line, int line_no)
    {
        std::size_t eq = find_binding_eq(line);
        if (eq == std::string::npos)
            return parse_and_eval(env, line, line_no);

        std::string name = trim(line.substr(0, eq));
        std::string expr_str = trim(line.substr(eq + 1));

        if (!is_valid_identifier(name))
        {
            std::cerr << "error " << line_no << ": invalid identifier '"
                      << name << "'\n";
            return 1;
        }

        rel::Scanner scanner(expr_str, line_no);
        rel::Parser parser(scanner.scan_tokens());
        rel::ParseResult result = parser.parse();

        if (!result.ok())
        {
            for (std::size_t i = 0; i < result.errors.size(); ++i)
            {
                const rel::ParseError& err = result.errors[i];
                const char* label = (err.kind == rel::ErrorKind::Lexical)
                                   ? "lexical error"
                                   : "syntax error";
            std::cerr << label << ": line " << err.line << ", column "
                      << err.column << ": " << err.message << '\n';
            }
            return 1;
        }

        rel::Evaluator evaluator(env);
        rel::Value v;
        try {
            v = evaluator.evaluate(*result.expr);
        } catch (const std::exception& e) {
            std::cerr << "runtime error: " << e.what() << std::endl;
            return 1;
        }
        env.define(name, v);
        std::cout << v.to_string(name) << '\n';
        return 0;
    }

    int run_file(const char* path)
    {
        rel::Environment env;
        rel::init_builtin_constants(env);

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
                eval_line(env, line, line_no);
            }
            ++line_no;
        }
        return 0;
    }

    int run_repl()
    {
        rel::Environment env;
        rel::init_builtin_constants(env);

        std::cout << "REL interpreter.\n"
                  << "  expr        - evaluate and print\n"
                  << "  name = expr - bind and print\n"
                  << "  Ctrl+D (Unix) / Ctrl+Z (Windows) to exit.\n";

        int line_no = 1;
        while (true)
        {
#ifdef _WIN32
            // TODO: enable readline on Windows (e.g. via vcpkg port)
            std::cout << ">>> " << std::flush;
            std::string line;
            if (!std::getline(std::cin, line))
            {
                std::cout << '\n';
                break;
            }
            if (line.empty()) continue;
            eval_line(env, line, line_no);
            ++line_no;
#else
            char* raw = readline(">>> ");
            if (!raw)
            {
                std::cout << '\n';
                break;
            }
            std::string line(raw);
            if (!line.empty())
            {
                add_history(raw);
                free(raw);
                eval_line(env, line, line_no);
                ++line_no;
            }
            else
            {
                free(raw);
            }
#endif
        }
        return 0;
    }
}

int main(int argc, char** argv)
{
    if (argc > 2)
    {
        std::cerr << "usage: " << (argc > 0 ? argv[0] : "rel")
                  << " [path-to-file]\n";
        return 2;
    }
    if (argc == 2) return run_file(argv[1]);
    return run_repl();
}
