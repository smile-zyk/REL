#include "rel.h"

#include "eval/environment.h"
#include "eval/evaluator.h"
#include "parser/parser.h"
#include "scanner/scanner.h"

#include <stdexcept>

namespace rel {

Value eval(const std::string& source)
{
    Scanner scanner(source);
    ScanResult sr = scanner.scan();
    if (!sr.ok())
        throw std::runtime_error(sr.errors[0].to_string());
    Parser parser(std::move(sr.tokens));
    ParseResult result = parser.parse();

    if (!result.ok())
        throw std::runtime_error(result.errors[0].message);

    Environment env;
    Evaluator evaluator(env);
    return evaluator.evaluate(*result.expr);
}

} // namespace rel
