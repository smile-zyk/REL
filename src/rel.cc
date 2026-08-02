#include "rel.h"

#include "eval/environment.h"
#include "eval/evaluator.h"
#include "parser/parser.h"
#include "scanner/scanner.h"

#include "measurement.h"

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
    env.Define("PI",        Value(xdataset::Measurement::Real(3.1415926535898)));
    env.Define("pi",        Value(xdataset::Measurement::Real(3.1415926535898)));
    env.Define("e",         Value(xdataset::Measurement::Real(2.718281822)));
    env.Define("ln10",      Value(xdataset::Measurement::Real(2.302585093)));
    env.Define("boltzmann", Value(xdataset::Measurement::Real(1.380658e-23)));
    env.Define("qelectron", Value(xdataset::Measurement::Real(1.60217733e-19)));
    env.Define("planck",    Value(xdataset::Measurement::Real(6.6260755e-34)));
    env.Define("c0",        Value(xdataset::Measurement::Real(2.99792e+08)));
    env.Define("e0",        Value(xdataset::Measurement::Real(8.85419e-12)));
    env.Define("u0",        Value(xdataset::Measurement::Real(12.5664e-07)));
    env.Define("tinyReal",  Value(xdataset::Measurement::Real(2.2e-308)));
    env.Define("hugeReal",  Value(xdataset::Measurement::Real(3.4e+38)));
}

} // namespace rel
