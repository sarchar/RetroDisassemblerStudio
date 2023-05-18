#include <memory>

#include "systems/nes/nes_defines.h"
#include "systems/nes/nes_expressions.h"

using namespace std;

namespace NES {

Define::Define(std::string const& _name, shared_ptr<Expression>& _expression)
    : name(_name), expression(_expression)
{
}

Define::~Define()
{
}

s64 Define::Evaluate()
{
    if(cached) return cached_value;
    string errmsg;
    if(!expression->Evaluate(&cached_value, errmsg)) assert(false); // should never happen since Expression has already been validated
    cached = true;
    return cached_value;
}

std::string const& Define::GetExpressionString()
{
    if(cached_expression_string) return expression_string;
    stringstream ss;
    ss << *expression;
    expression_string = ss.str();
    cached_expression_string = true;
    return expression_string;
}

bool Define::Save(std::ostream& os, std::string& errmsg)
{
    WriteString(os, name);
    if(!os.good()) {
        errmsg = "Error saving Define";
        return false;
    }
    return expression->Save(os, errmsg);
}

shared_ptr<Define> Define::Load(std::istream& is, std::string& errmsg)
{
    string name;
    ReadString(is, name);
    if(!is.good()) {
        errmsg = "Error loading Define";
        return nullptr;
    }
    auto expression = make_shared<Expression>();
    if(!expression->Load(is, errmsg)) return nullptr;
    return make_shared<Define>(name, expression);
}

}
