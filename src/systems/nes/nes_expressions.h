#pragma once

#include <memory>

#include "systems/expressions.h"

namespace NES {

class ExpressionHelper : public BaseExpressionHelper {
};

class ExpressionNode : public BaseExpressionNode {
};

namespace ExpressionNodes {
}

class ExpressionNodeCreator : public BaseExpressionNodeCreator {
};

class Expression : public BaseExpression {
public:
    std::shared_ptr<BaseExpressionNodeCreator> GetNodeCreator() override {
        return std::make_shared<ExpressionNodeCreator>();
    }
};

};
