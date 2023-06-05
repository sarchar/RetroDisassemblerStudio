// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#include <iostream>
#include <memory>

#include "systems/expressions.h"
#include "systems/nes/expressions.h"
#include "systems/nes/label.h"
#include "systems/nes/system.h"

#include "windows/nes/project.h"

using namespace std;

namespace Systems::NES {

namespace ExpressionNodes {
int Define::base_expression_node_id = 0;
int Label::base_expression_node_id = 0;
int Immediate::base_expression_node_id = 0;
int IndexedX::base_expression_node_id = 0;
int IndexedY::base_expression_node_id = 0;
int Accum::base_expression_node_id = 0;

void Define::Print(std::ostream& ostream) {
    ostream << define->GetString();
}

bool Define::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator>) 
{
    WriteString(os, define->GetString());
    if(!os.good()) {
        errmsg = "Error saving Define expression";
        return false;
    }
    return true;
}

shared_ptr<Define> Define::Load(istream& is, string& errmsg, shared_ptr<BaseExpressionNodeCreator>&) 
{
    string name;
    ReadString(is, name);
    if(!is.good()) {
        errmsg = "Error reading Define expression";
        return nullptr;
    }

    auto system = GetSystem();
    assert(system);
    auto define = system->FindDefine(name);
    assert(define);
    return make_shared<Define>(define);
}

Label::Label(GlobalMemoryLocation const& _where, int _nth, string const& _display)
    : where(_where), nth(_nth), display(_display), offset(0xDEADBEEF), long_mode(false)
{ }

bool Label::NoteReference(GlobalMemoryLocation const& source) {
    if(auto t = label.lock()) {
        // valid label
        t->NoteReference(source);
        return true;
    }

    if(Update()) return NoteReference(source);

    return false;
}

void Label::RemoveReference(GlobalMemoryLocation const& where) {
    if(auto t = label.lock()) {
        t->RemoveReference(where);
    }
}

bool Label::Update()
{
    // look up the labels at the saved address and use the nth one
    if(auto system = GetSystem()) {
        if(auto memory_object = system->GetMemoryObject(where, &offset)) {
            if(memory_object->labels.size()) {
                // found a label, so cache that
                nth = nth % memory_object->labels.size();
                label = memory_object->labels[nth];
                return true;
            }
        }
    }

    return false;
}

void Label::NextLabel()
{
    nth += 1;
    Reset();
}

void Label::Print(std::ostream& ostream) {
    // Use the label if it exists
    if(auto t = label.lock()) {
        ostream << t->GetString();
        if(offset > 0) {
            ostream << "+" << offset;
        } else if(offset < 0) {
            assert(false); // I wanna see this happen!
        }
        return;
    }

    // no label, display memory address instead
    ostream << display;
}

bool Label::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator>) 
{
    if(!where.Save(os, errmsg)) return false;
    // update the index if possible
    if(auto t = label.lock()) nth = t->GetIndex();
    WriteVarInt(os, nth);
    WriteString(os, display);
    WriteVarInt(os, (int)long_mode);
    if(!os.good()) {
        errmsg = "Error saving label";
        return false;
    }
    return true;
}

shared_ptr<Label> Label::Load(istream& is, string& errmsg, shared_ptr<BaseExpressionNodeCreator>&) 
{
    GlobalMemoryLocation where;
    if(!where.Load(is, errmsg)) return nullptr;

    int nth = ReadVarInt<int>(is);

    string display;
    ReadString(is, display);

    bool long_mode = (bool)ReadVarInt<int>(is);
    if(!is.good()) {
        errmsg = "Error loading Label";
        return nullptr;
    }

    auto ret = make_shared<Label>(where, nth, display);
    ret->SetLongMode(long_mode);
    return ret;
}

}

void ExpressionNodeCreator::RegisterExpressionNodes()
{
    RegisterBaseExpressionNode<ExpressionNodes::Accum>();
    RegisterBaseExpressionNode<ExpressionNodes::Immediate>();
    RegisterBaseExpressionNode<ExpressionNodes::IndexedX>();
    RegisterBaseExpressionNode<ExpressionNodes::IndexedY>();

    RegisterBaseExpressionNode<ExpressionNodes::Define>();
    RegisterBaseExpressionNode<ExpressionNodes::Label>();
}

// we're going to interject immediate operands into the expression by letting
// and expression start with a '#'. this also means that elements in function list may contain
// immediates, but that won't be a problem due to Explore() semantic checking, making sure that
// only the top level (root) node can be an Immediate
//
// immediate_expr: HASH expression
//               | expression
//               ;
shared_ptr<BaseExpressionNode> Expression::ParseExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& _node_creator, string& errmsg, int& errloc)
{
    auto node_creator = dynamic_pointer_cast<ExpressionNodeCreator>(_node_creator);

    if(tenderizer->GetCurrentMeat() == Tenderizer::Meat::HASH) {
        string display = tenderizer->GetDisplayText();
        tenderizer->Gobble();
        auto expr = BaseExpression::ParseExpression(tenderizer, _node_creator, errmsg, errloc);
        if(!expr) return nullptr;
        return node_creator->CreateImmediate(display, expr);
    } else {
        return BaseExpression::ParseExpression(tenderizer, _node_creator, errmsg, errloc);
    }
}

// We're going to take over the parenthese expressions so that we will allow a list when nested at depth 0
// We will also forbid list greater than lengths 1 and if the 2nd item is not "X" or "Y", at which point we
// can create an indexed node instead
//
// paren_expression: (if depth = 1) expression_list_of_length_1
//                 | expression
//                 ;
//
shared_ptr<BaseExpressionNode> Expression::ParseParenExpression(shared_ptr<Tenderizer>& tenderizer, shared_ptr<BaseExpressionNodeCreator>& _node_creator, string& errmsg, int& errloc)
{
    auto node_creator = dynamic_pointer_cast<ExpressionNodeCreator>(_node_creator);

    if(parens_depth != 1) {
        return BaseExpression::ParseParenExpression(tenderizer, _node_creator, errmsg, errloc);
    }

    // save location to start of the list
    auto loc = tenderizer->GetLocation();

    auto node = BaseExpression::ParseExpressionList(tenderizer, _node_creator, errmsg, errloc);
    if(!node) return nullptr;

    if(auto list = dynamic_pointer_cast<BaseExpressionNodes::ExpressionList>(node)) { // check if we got a list
        // validate length
        if(list->GetSize() != 2) {
            errmsg = "Invalid list of expressions";
            errloc = loc;
            return nullptr;
        }

        // get 2nd node, and make sure it's either X, or Y
        string display;
        auto name = dynamic_pointer_cast<BaseExpressionNodes::Name>(list->GetNode(1, &display));
        string str = name ? strlower(name->GetString()) : "";
        if(str != "x" && str != "y") {
            errmsg = "Invalid index (must be X or Y)";
            errloc = loc;
            return nullptr;
        }

        // let's convert this node into IndexedX or IndexedY
        display = display + name->GetString();
        auto value = list->GetNode(0);
        if(str == "x") {
            return node_creator->CreateIndexedX(value, display);
        } else {
            return node_creator->CreateIndexedY(value, display);
        }
    }

    return node;
}


}
