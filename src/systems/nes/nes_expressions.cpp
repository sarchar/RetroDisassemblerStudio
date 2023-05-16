#include <iostream>
#include <memory>

#include "main.h"
#include "systems/expressions.h"
#include "systems/nes/nes_expressions.h"
#include "systems/nes/nes_label.h"
#include "systems/nes/nes_system.h"

using namespace std;

namespace NES {

namespace ExpressionNodes {
int OperandAddressOrLabel::base_expression_node_id = 0;
int Immediate::base_expression_node_id = 0;
int IndexedX::base_expression_node_id = 0;
int IndexedY::base_expression_node_id = 0;
int Accum::base_expression_node_id = 0;

void OperandAddressOrLabel::Print(std::ostream& ostream) const {
    if(auto system = MyApp::Instance()->GetProject()->GetSystem<System>()) {
        int offset = 0;
        if(auto memory_object = system->GetMemoryObject(where, &offset)) {
            if(nth < memory_object->labels.size()) {
                ostream << memory_object->labels[nth]->GetString();
                if(offset > 0) ostream << "+" << offset;
                return;
            }
        }
    }

    ostream << display;
}

}

void ExpressionNodeCreator::RegisterExpressionNodes()
{
    RegisterBaseExpressionNode<ExpressionNodes::Accum>();
    RegisterBaseExpressionNode<ExpressionNodes::Immediate>();
    RegisterBaseExpressionNode<ExpressionNodes::IndexedX>();
    RegisterBaseExpressionNode<ExpressionNodes::IndexedY>();
    RegisterBaseExpressionNode<ExpressionNodes::OperandAddressOrLabel>();
}

// we're going to interject immediate operands into the expression by letting
// and expression start with a '#'. this also means that elements in function list may contain
// immediates, but that won't be a problem for a long time. when it does, I feel sorry for whoever
// has to figure out what to do there (most likely me!)
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

}
