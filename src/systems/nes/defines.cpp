// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#include <memory>

#include "systems/nes/defines.h"
#include "systems/nes/expressions.h"
#include "systems/nes/system.h"

#include "windows/nes/project.h"

using namespace std;

namespace Systems::NES {

Define::Define(std::string const& _name)
    : name(_name)
{
    reverse_references_changed = make_shared<reverse_references_changed_t>();
    expression = Expression::FromString("0");
}

Define::~Define()
{
}

bool Define::SetExpression(std::string const& s, string& errmsg)
{
    auto expr = make_shared<Expression>();
    int errloc;
    if(!expr->Set(s, errmsg, errloc)) {
        stringstream ss;
        ss << "Could not parse expression: " << errmsg << " (at offset " << errloc << ")";
        errmsg = ss.str();
        return false;
    }

    return SetExpression(expr, errmsg);
}

bool Define::SetExpression(std::shared_ptr<BaseExpression> const& expr, string& errmsg)
{
    // fixup the expression and allow only defines
    // no labels, allow defines, no deref, no modes, no long labels, allow enums
    FixupFlags fixup_flags = FIXUP_DEFINES | FIXUP_ENUMS;
    if(!GetSystem()->FixupExpression(expr, errmsg, fixup_flags)) return false;

    // and now the define must be evaluable!
    s64 result;
    if(!expr->Evaluate(&result, errmsg)) return false;

    // looks good, update references
    ClearReferences();
    expression = expr;
    cached_value = result;
    SetReferences();

    return true;
}

void Define::SetReferences()
{
    // Explore expression and mark each referenced Define and Enum that we're referring to it
    auto cb = [this](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        if(auto define_node = dynamic_pointer_cast<ExpressionNodes::Define>(node)) {
            define_node->GetDefine()->NoteReference(shared_from_this());
        } else if(auto ee_node = dynamic_pointer_cast<ExpressionNodes::EnumElement>(node)) {
            ee_node->GetEnumElement()->NoteReference(shared_from_this());
        }
        return true;
    };

    if(!expression->Explore(cb, nullptr)) assert(false); // false return shouldn't happen
}

void Define::ClearReferences()
{
    // Explore expression and mark each referenced Define and Enum that we're no longer referring to it
    auto cb = [this](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        if(auto define_node = dynamic_pointer_cast<ExpressionNodes::Define>(node)) {
            define_node->GetDefine()->RemoveReference(shared_from_this());
        } else if(auto ee_node = dynamic_pointer_cast<ExpressionNodes::EnumElement>(node)) {
            ee_node->GetEnumElement()->RemoveReference(shared_from_this());
        }
        return true;
    };

    if(!expression->Explore(cb, nullptr)) assert(false); // false return shouldn't happen
}

s64 Define::Evaluate()
{
    if(cached) return cached_value;
    string errmsg;
    if(!expression->Evaluate(&cached_value, errmsg)) assert(false); // should never happen since Expression has already been validated
    cached = true;
    return cached_value;
}

std::string Define::GetExpressionString()
{
    stringstream ss;
    ss << *expression;
    return ss.str();
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
    auto define = make_shared<Define>(name);
    define->expression = expression;
    return define;
}

}
