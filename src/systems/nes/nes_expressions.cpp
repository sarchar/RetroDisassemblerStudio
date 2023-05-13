#include <iostream>

#include "main.h"
#include "systems/nes/nes_expressions.h"
#include "systems/nes/nes_label.h"
#include "systems/nes/nes_system.h"

namespace NES {

namespace ExpressionNodes {
int OperandAddressOrLabel::base_expression_node_id = 0;
int Immediate::base_expression_node_id = 0;
int IndexedX::base_expression_node_id = 0;
int IndexedY::base_expression_node_id = 0;
int Accum::base_expression_node_id = 0;

void OperandAddressOrLabel::Print(std::ostream& ostream) const {
    if(auto system = MyApp::Instance()->GetProject()->GetSystem<System>()) {
        if(auto memory_object = system->GetMemoryObject(where)) {
            if(nth < memory_object->labels.size()) {
                ostream << memory_object->labels[nth]->GetString();
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

}
