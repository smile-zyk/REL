#include "rel.h"

#include "environment.h"
#include "evaluator.h"
#include "parser.h"
#include "scanner.h"

#include <cctype>
#include <stdexcept>
#include <string>

namespace rel {
namespace {

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
        name == "EQUALS" || name == "NOTEQUALS")
        return false;
    return true;
}

std::string trim(const std::string& s)
{
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

/// Find the top-level binding '=' of a `name = expr` line.  Skips the
/// comparison/assignment operators (==, !=, <=, >=, =), returning npos when
/// the line is a plain expression.
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

}  // namespace

Value Eval(const std::string& source, Environment* env)
{
    Environment temp_env;
    if (!env) env = &temp_env;

    Scanner scanner(source);
    ScanResult sr = scanner.Scan();
    if (!sr.Ok())
        throw std::runtime_error(sr.errors[0].to_string());
    Parser parser(std::move(sr.tokens));
    ParseResult result = parser.Parse();

    if (!result.Ok())
        throw std::runtime_error(result.errors[0].to_string());

    Evaluator evaluator(*env);
    return evaluator.Evaluate(*result.expr);
}

void Exec(const std::string& source, Environment& env)
{
    std::size_t eq = find_binding_eq(source);
    if (eq == std::string::npos)
    {
        // Plain expression: evaluate and discard the result.
        Eval(source, &env);
        return;
    }

    std::string name = trim(source.substr(0, eq));
    std::string expr_str = trim(source.substr(eq + 1));
    if (!is_valid_identifier(name))
        throw std::runtime_error("invalid identifier '" + name + "'");

    Value v = Eval(expr_str, &env);
    env.Define(name, v);
}

} // namespace rel
