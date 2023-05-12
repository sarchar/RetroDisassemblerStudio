#pragma once

#include <cassert>
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

    virtual bool Save(std::ostream&, std::string&) = 0;

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
        { assert(value); }

        virtual ~Parens() {}

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            return value->Evaluate(helper, result);
        }

        void Print(std::ostream& ostream) const override {
            ostream << left << *value << right;
        }

        bool Save(std::ostream& os, std::string& errmsg) override {
            WriteString(os, left);
            if(!value->Save(os, errmsg)) return false;
            WriteString(os, right);
            return true;
        }

    private:
        std::string left;
        std::shared_ptr<BaseExpressionNode> value;
        std::string right;
    };

    template <class T>
    class Constant : public BaseExpressionNode {
    public:
        Constant(T _value, std::string const& _display)
            : value(_value), display(_display)
        { }

        virtual ~Constant() {}

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const&, s64* result) const override {
            *result = (s64)value;
            return true;
        }

        void Print(std::ostream& ostream) const override {
            ostream << display;
        }

        bool Save(std::ostream& os, std::string& errmsg) override {
            WriteVarInt(os, value);
            WriteString(os, display);
            return true;
        }

    private:
        T           value;
        std::string display;
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

        bool Save(std::ostream& os, std::string& errmsg) override {
            WriteString(os, name);
            return true;
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
            assert(left);
            assert(right);
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

        bool Save(std::ostream& os, std::string& errmsg) override {
            if(!left->Save(os, errmsg)) return false;
            WriteString(os, display);
            if(!right->Save(os, errmsg)) return false;
            return true;
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
    typedef std::shared_ptr<BaseExpressionNode> BN;

    BaseExpressionNodeCreator();
    virtual ~BaseExpressionNodeCreator();

    virtual BN CreateParens(std::string const& left, BN& value, std::string const& right) {
        return std::make_shared<BaseExpressionNodes::Parens>(left, value, right);
    }

    virtual BN CreateConstantU8(u8 value, std::string const& display) {
        return std::make_shared<BaseExpressionNodes::Constant<u8>>(value, display);
    }

    virtual BN CreateConstantU16(u16 value, std::string const& display) {
        return std::make_shared<BaseExpressionNodes::Constant<u16>>(value, display);
    }

    virtual BN CreateName(std::string const& s) {
        return std::make_shared<BaseExpressionNodes::Name>(s);
    }

    virtual BN CreateAddOp(BN& left, BN& right, std::string const& display) {
        auto op = [](s64 a, s64 b)->s64 { return a + b; };
        return std::make_shared<BaseExpressionNodes::BinaryOp<BN>>(op, left, right, display);
    }

    virtual BN CreateSubtractOp(BN& left, BN& right, std::string const& display) {
        auto op = [](s64 a, s64 b)->s64 { return a - b; };
        return std::make_shared<BaseExpressionNodes::BinaryOp<BN>>(op, left, right, display);
    }

    virtual BN CreateMultiplyOp(BN& left, BN& right, std::string const& display) {
        auto op = [](s64 a, s64 b)->s64 { return a * b; };
        return std::make_shared<BaseExpressionNodes::BinaryOp<BN>>(op, left, right, display);
    }

    virtual BN CreateDivideOp(BN& left, BN& right, std::string const& display) {
        auto op = [](s64 a, s64 b)->s64 { return a / b; };
        return std::make_shared<BaseExpressionNodes::BinaryOp<BN>>(op, left, right, display);
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

    virtual bool Save(std::ostream& os, std::string& errmsg) {
        bool has_root = (bool)root;
        assert(sizeof(has_root) == 1);
        os.write((char*)&has_root, sizeof(has_root));
        if(root) return root->Save(os, errmsg);
        return true;
    }

    friend std::ostream& operator<<(std::ostream&, BaseExpression const&);

protected:
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

