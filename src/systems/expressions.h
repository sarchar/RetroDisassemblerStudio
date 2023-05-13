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
class BaseExpressionNodeCreator;

class BaseExpressionNode : public std::enable_shared_from_this<BaseExpressionNode> {
public:
    BaseExpressionNode();
    virtual ~BaseExpressionNode();

    virtual int  GetExpressionNodeType() const = 0;
    virtual void Print(std::ostream&) const = 0;

    virtual bool Evaluate(std::shared_ptr<BaseExpressionHelper> const&, s64*) const = 0;

    virtual bool Save(std::ostream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>) = 0;

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

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return Parens::base_expression_node_id; }

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            return value->Evaluate(helper, result);
        }

        void Print(std::ostream& ostream) const override {
            ostream << left << *value << right;
        }

        bool Save(std::ostream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>) override;
        static std::shared_ptr<Parens> Load(std::istream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>&);

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

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return Constant<T>::base_expression_node_id; }

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const&, s64* result) const override {
            *result = (s64)value;
            return true;
        }

        void Print(std::ostream& ostream) const override {
            ostream << display;
        }

        bool Save(std::ostream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>) override;
        static std::shared_ptr<Constant<T>> Load(std::istream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>&);

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

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return Name::base_expression_node_id; }

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            auto name_expression = helper->ResolveName(name);
            if(name_expression) return name_expression->Evaluate(helper, result);
            return false;
        }

        void Print(std::ostream& ostream) const override {
            ostream << name;
        }

        bool Save(std::ostream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>) override;
        static std::shared_ptr<Name> Load(std::istream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>&);

    private:
        std::string name;
    };

    template<s64 (*T)(s64, s64)>
    class BinaryOp : public BaseExpressionNode {
    public:
        BinaryOp(std::shared_ptr<BaseExpressionNode>& _left, std::string const& _display, std::shared_ptr<BaseExpressionNode>& _right)
            : left(_left), display(_display), right(_right)
        {
            assert(left);
            assert(right);
        }

        virtual ~BinaryOp() {}

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return BinaryOp<T>::base_expression_node_id; }

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            s64 left_value;
            if(!left->Evaluate(helper, &left_value)) return false;
            s64 right_value;
            if(!right->Evaluate(helper, &right_value)) return false;
            *result = T(left_value, right_value);
            return true;
        }

        void Print(std::ostream& ostream) const override {
            ostream << *left << display << *right;
        }

        bool Save(std::ostream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>) override;
        static std::shared_ptr<BinaryOp<T>> Load(std::istream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>&);

    private:
        std::shared_ptr<BaseExpressionNode> left;
        std::shared_ptr<BaseExpressionNode> right;
        std::string display;
    };

    inline s64 _add_op(s64 a, s64 b) { return a + b; }
    using AddOp = BinaryOp<_add_op>;

    inline s64 _subtract_op(s64 a, s64 b) { return a - b; }
    using SubtractOp = BinaryOp<_add_op>;
    
    inline s64 _multiply_op(s64 a, s64 b) { return a * b; }
    using MultiplyOp = BinaryOp<_add_op>;

    inline s64 _divide_op(s64 a, s64 b) { return a / b; }
    using DivideOp = BinaryOp<_add_op>;
}

class BaseExpressionNodeCreator : public std::enable_shared_from_this<BaseExpressionNodeCreator> {
public:
    typedef std::shared_ptr<BaseExpressionNode> BN;

    struct BaseExpressionNodeInfo {
        std::function<std::shared_ptr<BaseExpressionNode>(std::istream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>&)> load;
    };

    BaseExpressionNodeCreator();
    virtual ~BaseExpressionNodeCreator();

    static void RegisterBaseExpressionNodes();
    template <class T>
    static void RegisterBaseExpressionNode() {
        int id = BaseExpressionNodeCreator::expression_nodes.size();
        auto info = std::make_shared<BaseExpressionNodeInfo>(BaseExpressionNodeInfo {
            .load = std::bind(&T::Load, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
        });
        BaseExpressionNodeCreator::expression_nodes.push_back(info);
        T::base_expression_node_id = id;
    }


    virtual BN CreateParens(std::string const& left, BN& value, std::string const& right) {
        return std::make_shared<BaseExpressionNodes::Parens>(left, value, right);
    }

    virtual BN CreateConstantU8(u8 value, std::string const& display) {
        return std::make_shared<BaseExpressionNodes::Constant<u8>>(value, display);
    }

    virtual BN CreateConstantU16(u16 value, std::string const& display) {
        return std::make_shared<BaseExpressionNodes::Constant<u16>>(value, display);
    }

    // TODO get rid of this. we may want a String type in the future though
    // Code should use OperandAddressOrLabel
    virtual BN CreateName(std::string const& s) {
        return std::make_shared<BaseExpressionNodes::Name>(s);
    }

    virtual BN CreateAddOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::AddOp>(left, display, right);
    }

    virtual BN CreateSubtractOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::SubtractOp>(left, display, right);
    }

    virtual BN CreateMultiplyOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::MultiplyOp>(left, display, right);
    }

    virtual BN CreateDivideOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::DivideOp>(left, display, right);
    }

    virtual bool Save(std::shared_ptr<BaseExpressionNode> const&, std::ostream&, std::string&);
    virtual BN Load(std::istream&, std::string&);

private:
    static std::vector<std::shared_ptr<BaseExpressionNodeInfo>> expression_nodes;
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
        if(root) {
            auto nc = GetNodeCreator();
            return nc->Save(root, os, errmsg);
        }
        return true;
    }

    virtual bool Load(std::istream& is, std::string& errmsg) {
        bool has_root;
        is.read((char*)&has_root, sizeof(has_root));
        if(has_root) {
            // here we need to know what the node type that's been serialized is
            auto nc = GetNodeCreator();
            root = nc->Load(is, errmsg);
            if(!root) return false;
        }
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

