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

// =========================================================================
//  init_builtin_constants
// =========================================================================

void InitBuiltinConstants(Environment& env)
{
    env.Define("PI",        Value::Real(3.1415926535898));
    env.Define("pi",        Value::Real(3.1415926535898));
    env.Define("e",         Value::Real(2.718281822));
    env.Define("ln10",      Value::Real(2.302585093));
    env.Define("boltzmann", Value::Real(1.380658e-23));
    env.Define("qelectron", Value::Real(1.60217733e-19));
    env.Define("planck",    Value::Real(6.6260755e-34));
    env.Define("c0",        Value::Real(2.99792e+08));
    env.Define("e0",        Value::Real(8.85419e-12));
    env.Define("u0",        Value::Real(12.5664e-07));
    env.Define("tinyReal",  Value::Real(2.2e-308));
    env.Define("hugeReal",  Value::Real(3.4e+38));
}

// =========================================================================
//  RegisterFunction
// =========================================================================

void RegisterFunction(Environment& env,
                      std::string name,
                      std::vector<FunctionParam> params,
                      NativeFunction impl)
{
    env.RegisterFunction(Function(std::move(name),
                                  std::move(params),
                                  std::move(impl)));
}

} // namespace rel
