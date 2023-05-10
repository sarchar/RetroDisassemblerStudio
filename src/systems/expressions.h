#pragma once

#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "util.h"

namespace BaseExpressionNodes {
    class Name;
}

class BaseExpressionHelper;

class BaseExpressionNode : public std::enable_shared_from_this<BaseExpressionNode> {
public:
    BaseExpressionNode();
    virtual ~BaseExpressionNode();

    virtual void Print(std::ostream&) const = 0;

    virtual bool Evaluate(std::shared_ptr<BaseExpressionHelper> const&, s64*) const = 0;

    friend std::ostream& operator<<(std::ostream&, BaseExpressionNode const&);
};

class BaseExpressionHelper : public std::enable_shared_from_this<BaseExpressionHelper> {
public:
    BaseExpressionHelper();
    virtual ~BaseExpressionHelper();

    virtual std::shared_ptr<BaseExpressionNode> ResolveName(std::string const&) = 0;
};

namespace BaseExpressionNodes {
    class Parens : public BaseExpressionNode {
    public:
        Parens(std::string const& _left, std::shared_ptr<BaseExpressionNode>& _value, std::string const& _right)
            : left(_left), value(_value), right(_right)
        { }

        virtual ~Parens() {}

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            return value->Evaluate(helper, result);
        }

        void Print(std::ostream& ostream) const override {
            ostream << left << *value << right;
        }

    private:
        std::string left;
        std::shared_ptr<BaseExpressionNode> value;
        std::string right;
    };

    class ConstantU8 : public BaseExpressionNode {
    public:
        ConstantU8(std::string const& vstr)
            : value_str(vstr)
        {
            // parse value_str
            std::stringstream ss;
            if(vstr[0] == L'$') {
                ss << std::hex << vstr.substr(1);
            } else {
                ss << vstr;
            }

            u64 v;
            ss >> v;

            value = (u8)(v & 0xFF);
        }

        virtual ~ConstantU8() {}

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            *result = (s64)value;
            return true;
        }

        void Print(std::ostream& ostream) const override {
            ostream << value_str;
        }

    private:
        std::string value_str;
        u8          value;
    };

    class Name : public BaseExpressionNode {
    public:
        Name(std::string const& _name)
            : name(_name)
        {
        }

        virtual ~Name() {}

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            auto name_expression = helper->ResolveName(name);
            if(name_expression) return name_expression->Evaluate(helper, result);
            return false;
        }

        void Print(std::ostream& ostream) const override {
            ostream << name;
        }

    private:
        std::string name;
    };

    using BinaryOpFunc = std::function<s64(s64, s64)>;

    template<class T>
    class BinaryOp : public BaseExpressionNode {
    public:
        BinaryOp(BinaryOpFunc _op, T& _left, T& _right, std::string const& _display)
            : op(_op), left(_left), right(_right), display(_display)
        {
        }

        virtual ~BinaryOp() {}

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            s64 left_value;
            if(!left->Evaluate(helper, &left_value)) return false;
            s64 right_value;
            if(!right->Evaluate(helper, &right_value)) return false;
            *result = op(left_value, right_value);
            return true;
        }

        void Print(std::ostream& ostream) const override {
            ostream << *left << display << *right;
        }

    private:
        BinaryOpFunc op;
        std::shared_ptr<BaseExpressionNode> left;
        std::shared_ptr<BaseExpressionNode> right;
        std::string display;
    };
}

class BaseExpressionNodeCreator : public std::enable_shared_from_this<BaseExpressionNodeCreator> {
public:
    typedef std::shared_ptr<BaseExpressionNode> N;

    BaseExpressionNodeCreator();
    virtual ~BaseExpressionNodeCreator();

    virtual N CreateParens(std::string const& left, N& value, std::string const& right) {
        return std::make_shared<BaseExpressionNodes::Parens>(left, value, right);
    }

    virtual N CreateConstantU8(std::string const& s) {
        return std::make_shared<BaseExpressionNodes::ConstantU8>(s);
    }

    virtual N CreateName(std::string const& s) {
        return std::make_shared<BaseExpressionNodes::Name>(s);
    }

    virtual N CreateAddOp(N& left, N& right, std::string const& display) {
        auto op = [](s64 a, s64 b)->s64 { return a + b; };
        return std::make_shared<BaseExpressionNodes::BinaryOp<N>>(op, left, right, display);
    }

    virtual N CreateSubtractOp(N& left, N& right, std::string const& display) {
        auto op = [](s64 a, s64 b)->s64 { return a - b; };
        return std::make_shared<BaseExpressionNodes::BinaryOp<N>>(op, left, right, display);
    }

    virtual N CreateMultiplyOp(N& left, N& right, std::string const& display) {
        auto op = [](s64 a, s64 b)->s64 { return a * b; };
        return std::make_shared<BaseExpressionNodes::BinaryOp<N>>(op, left, right, display);
    }

    virtual N CreateDivideOp(N& left, N& right, std::string const& display) {
        auto op = [](s64 a, s64 b)->s64 { return a / b; };
        return std::make_shared<BaseExpressionNodes::BinaryOp<N>>(op, left, right, display);
    }
};

// implement expressions as abstract syntax trees
class BaseExpression : public std::enable_shared_from_this<BaseExpression> {
public:
    BaseExpression();
    virtual ~BaseExpression();

    template <class T>
    void Set(T& t) { root = t; }

    bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) { 
        return root->Evaluate(helper, result); 
    }

    virtual std::shared_ptr<BaseExpressionNodeCreator> GetNodeCreator() = 0;

    friend std::ostream& operator<<(std::ostream&, BaseExpression const&);

private:
    std::shared_ptr<BaseExpressionNode> root;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class ExpressionNodeCreator : public BaseExpressionNodeCreator {
public:
    ExpressionNodeCreator()
    {
    }

    virtual ~ExpressionNodeCreator()
    {
    }

};

class Expression : public BaseExpression {
public:
    Expression()
    {
    }

    virtual ~Expression()
    {
    }

    virtual std::shared_ptr<BaseExpressionNodeCreator> GetNodeCreator()
    {
        return std::make_shared<ExpressionNodeCreator>();
    }
};

