#include <iostream>
#include <memory>

#include "main.h"
#include "systems/expressions.h"
#include "systems/nes/nes_expressions.h"
#include "systems/nes/nes_label.h"
#include "systems/nes/nes_project.h"
#include "systems/nes/nes_system.h"

using namespace std;

namespace NES {

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

    auto system = MyApp::Instance()->GetProject()->GetSystem<System>();
    assert(system);
    auto define = system->FindDefine(name);
    assert(define);
    return make_shared<Define>(define);
}

Label::Label(shared_ptr<NES::Label> const& _label, string const& _display)
    : label(_label), display(_display)
{ 
    where = _label->GetMemoryLocation();
}

Label::Label(GlobalMemoryLocation const& _where, string const& _display)
    : where(_where), display(_display)
{ }

bool Label::NoteReference(GlobalMemoryLocation const& source) {
    if(auto t = label.lock()) {
        // valid label
        t->NoteReference(source);
        return true;
    }

    // No label, so try looking it up. We can't assume anything about the nth label at the address
    // now that our old label is gone
    if(auto system = MyApp::Instance()->GetProject()->GetSystem<System>()) {
        auto labels = system->GetLabelsAt(where);
        if(labels.size()) {
            // found a label, so cache that
            label = labels[0];
            return NoteReference(source);
        }
    }

    return false;
}

void Label::RemoveReference(GlobalMemoryLocation const& where) {
    if(auto t = label.lock()) {
        t->RemoveReference(where);
    }
}

void Label::Print(std::ostream& ostream) {
    // Use the label if it exists
    if(auto t = label.lock()) {
        ostream << t->GetString();
        return;
    }

    // no label, display memory address instead
    ostream << display;
}

bool Label::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator>) 
{
    WriteString(os, display);
    if(auto t = label.lock()) {
        WriteVarInt(os, 1);
        WriteString(os, t->GetString());
    } else {
        WriteVarInt(os, 0);
        if(!where.Save(os, errmsg)) return false;
    }
    return true;
}

shared_ptr<Label> Label::Load(istream& is, string& errmsg, shared_ptr<BaseExpressionNodeCreator>&) 
{
    string display;
    ReadString(is, display);

    bool valid = (ReadVarInt<int>(is) == 1);
    if(!is.good()) {
        errmsg = "Error loading Label";
        return nullptr;
    }

    if(valid) {
        string name;
        ReadString(is, name);
        if(!is.good()) {
            errmsg = "Error loading label name";
            return nullptr;
        }

        // look up the label. since it was valid at save, it better be valid now
        auto system = MyApp::Instance()->GetProject()->GetSystem<System>();
        assert(system);
        auto label = system->FindLabel(name);
        assert(label);
        return make_shared<Label>(label, display);
    }

    // label was not valid, so use the memory location instead
    GlobalMemoryLocation where;
    if(!where.Load(is, errmsg)) return nullptr;

    return make_shared<Label>(where, display);
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
