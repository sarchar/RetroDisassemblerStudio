#include <iostream>

#include "systems/nes/nes_expressions.h"
#include "systems/nes/nes_label.h"

namespace NES {

namespace ExpressionNodes {

    void OperandAddressOrLabel::Print(std::ostream& ostream) const {
        if(auto t = target.lock()) {
            if(nth < t->labels.size()) {
                ostream << t->labels[nth]->GetString();
                return;
            }
        }

        ostream << display;
    }

}
}

