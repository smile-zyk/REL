#include "rel.h"

#include "eval/environment.h"
#include "eval/evaluator.h"
#include "parser/parser.h"
#include "scanner/scanner.h"

#include <stdexcept>

namespace rel {

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
        throw std::runtime_error(result.errors[0].message);

    Evaluator evaluator(*env);
    return evaluator.Evaluate(*result.expr);
}

} // namespace rel
