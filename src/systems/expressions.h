#pragma once

#include <cassert>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "util.h"

class Tenderizer {
public:
    enum class Meat {
        _HUNGRY,   // init state
        YUCKY,     // invalid token
        NAME, CONSTANT,
        PLUS, MINUS, BANG,
        ASTERISK, SLASH,
        EQUAL_TO, NOT_EQUAL_TO,
        LSHIFT, RSHIFT,
        CARET, PIPE, AMPERSAND, TILDE,
        POWER,
        LANGLE, RANGLE,
        LPAREN, RPAREN,
        COMMA,
        HASH,
        END        // end of tile
    };

    Tenderizer(std::istream&);

    Meat GetCurrentMeat() const { return current_meat; }
    std::string GetDisplayText() const { return display_text.str(); }
    std::string GetMeatText() const { return meat_text.str(); }
    int GetLocation() const { return location - 1; }

    bool Errored() const { return current_meat == Meat::YUCKY; }
    bool Finished() const { return Errored() || current_meat == Meat::END; }


    // Bite whatever's on the ground
    inline char Bite()
    {
        char c = input_stream.get();
        display_text << c;
        location++;
        return c;
    }

    // Peck at the floor until we find food
    inline char Peck()
    {
        char c;
        do {
            c = Bite();
        } while(c == ' ' || c == '\t');
        return c;
    }

    // Look, but don't peck
    inline char Look()
    {
        return input_stream.peek();
    }

    // Mmmm keep biting for a while
    inline void Satisfied()
    {
        char c = Look();
        while(c == ' ' || c == '\t') {
            Bite();
            c = Look();
        }
    }

    void Gobble(); // gobble, gobble

private:
    std::istream&     input_stream;
    std::stringstream display_text;
    std::stringstream meat_text;
    Meat              current_meat;
    int               location;
};


namespace BaseExpressionNodes {
    class Name;
}

class BaseExpressionNodeCreator;

class BaseExpressionNode;
typedef std::function<bool(std::shared_ptr<BaseExpressionNode>&, std::shared_ptr<BaseExpressionNode> const&, int, void*)> explore_callback_t;

class BaseExpressionNode : public std::enable_shared_from_this<BaseExpressionNode> {
public:
    BaseExpressionNode();
    virtual ~BaseExpressionNode();

    virtual int  GetExpressionNodeType() const = 0;
    virtual void Print(std::ostream&) = 0;

    virtual bool Evaluate(s64*, std::string&) const = 0;
    virtual bool Explore(explore_callback_t, int, void*) = 0;

    virtual bool Save(std::ostream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>) = 0;

    friend std::ostream& operator<<(std::ostream&, BaseExpressionNode&);
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

        std::shared_ptr<BaseExpressionNode> GetValue() { return value; }

        bool Evaluate(s64* result, std::string& errmsg) const override {
            return value->Evaluate(result, errmsg);
        }

        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            // depth first into the expression tree
            if(!value->Explore(explore_callback, depth + 1, userdata)) return false; 
            // and then evaluate the actual node
            if(!explore_callback(value, shared_from_this(), depth, userdata)) return false;
            return true;
        }

        void Print(std::ostream& ostream) override {
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

        bool Evaluate(s64* result, std::string& errmsg) const override {
            // Constants are straightforward
            *result = (s64)value;
            return true;
        }

        // Constant<T> has no subnodes to explore
        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            return true;
        }

        void Print(std::ostream& ostream) override {
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

        std::string const& GetString() const { return name; }

        bool Evaluate(s64* result, std::string& errmsg) const override {
            // Names are not evaluatable
            std::stringstream ss;
            ss << "Unable to evaluate name `" << name << "`";
            errmsg = ss.str();
            return false;
        }

        // Name has no subnodes to explore
        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            return true;
        }

        void Print(std::ostream& ostream) override {
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

        bool Evaluate(s64* result, std::string& errmsg) const override {
            s64 left_value;
            if(!left->Evaluate(&left_value, errmsg)) return false;
            s64 right_value;
            if(!right->Evaluate(&right_value, errmsg)) return false;
            *result = T(left_value, right_value); // can't fail
            return true;
        }

        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            // depth first into the left expression tree
            if(!left->Explore(explore_callback, depth + 1, userdata)) return false; 
            // and then evaluate the node
            if(!explore_callback(left, shared_from_this(), depth, userdata)) return false;
            // and depth first into the right expression tree
            if(!right->Explore(explore_callback, depth + 1, userdata)) return false; 
            // and then evaluate the node
            if(!explore_callback(right, shared_from_this(), depth, userdata)) return false;
            return true;
        }

        void Print(std::ostream& ostream) override {
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
    using SubtractOp = BinaryOp<_subtract_op>;
    
    inline s64 _multiply_op(s64 a, s64 b) { return a * b; }
    using MultiplyOp = BinaryOp<_multiply_op>;

    inline s64 _divide_op(s64 a, s64 b) { return a / b; }
    using DivideOp = BinaryOp<_divide_op>;

    inline s64 _power_op(s64 a, s64 b) { return pow(a, b); }
    using PowerOp = BinaryOp<_power_op>;

    inline s64 _binary_or_op(s64 a, s64 b) { return a | b; }
    using OrOp = BinaryOp<_binary_or_op>;

    inline s64 _binary_xor_op(s64 a, s64 b) { return a ^ b; }
    using XorOp = BinaryOp<_binary_xor_op>;

    inline s64 _binary_and_op(s64 a, s64 b) { return a & b; }
    using AndOp = BinaryOp<_binary_and_op>;

    inline s64 _lshift_op(s64 a, s64 b) { return a << b; }
    using LShiftOp = BinaryOp<_lshift_op>;

    inline s64 _rshift_op(s64 a, s64 b) { return a << b; }
    using RShiftOp = BinaryOp<_rshift_op>;

    inline s64 _equalto_op(s64 a, s64 b) { return (s64)(a == b); }
    using EqualToOp = BinaryOp<_equalto_op>;

    inline s64 _notequalto_op(s64 a, s64 b) { return (s64)(a != b); }
    using NotEqualToOp = BinaryOp<_notequalto_op>;

    template<s64 (*T)(s64)>
    class UnaryOp : public BaseExpressionNode {
    public:
        UnaryOp(std::string const& _display, std::shared_ptr<BaseExpressionNode>& _value)
            : display(_display), value(_value)
        {
            assert(value);
        }

        virtual ~UnaryOp() {}

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return UnaryOp<T>::base_expression_node_id; }

        bool Evaluate(s64* result, std::string& errmsg) const override {
            s64 evaluated;
            if(!value->Evaluate(&evaluated, errmsg)) return false;
            *result = T(evaluated);
            return true;
        }

        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            // depth first into the expression tree
            if(!value->Explore(explore_callback, depth + 1, userdata)) return false; 
            // and then evaluate the node
            if(!explore_callback(value, shared_from_this(), depth, userdata)) return false;
            return true;
        }

        void Print(std::ostream& ostream) override {
            ostream << display << *value;
        }

        bool Save(std::ostream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>) override;
        static std::shared_ptr<UnaryOp<T>> Load(std::istream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>&);

    private:
        std::shared_ptr<BaseExpressionNode> value;
        std::string display;
    };

    inline s64 _positive_op(s64 a) { return +a; }
    using PositiveOp = UnaryOp<_positive_op>;

    inline s64 _negate_op(s64 a) { return -a; }
    using NegateOp = UnaryOp<_negate_op>;

    inline s64 _binary_not_op(s64 a) { return ~a; }
    using BinaryNotOp = UnaryOp<_binary_not_op>;

    inline s64 _logical_not_op(s64 a) { return (s64)!(bool)a; }
    using LogicalNotOp = UnaryOp<_logical_not_op>;

    class DereferenceOp : public BaseExpressionNode {
    public:
        typedef std::function<bool(s64 in, s64* out, std::string& errmsg)> dereference_func_t;

        DereferenceOp(std::string const& _display, std::shared_ptr<BaseExpressionNode>& _value)
            : display(_display), value(_value)
        {
            assert(value);
        }

        virtual ~DereferenceOp() {}

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return DereferenceOp::base_expression_node_id; }

        template<typename T>
        void SetDereferenceFunction(T const& func) { dereference_func = func; }

        bool Evaluate(s64* result, std::string& errmsg) const override {
            s64 evaluated;
            if(!value->Evaluate(&evaluated, errmsg)) return false;
            if(!dereference_func) {
                errmsg = "Dereference function not specified";
                return false;
            }
            if(!dereference_func(evaluated, result, errmsg)) return false;
            return true;
        }

        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            // depth first into the expression tree
            if(!value->Explore(explore_callback, depth + 1, userdata)) return false; 
            // and then evaluate the node
            if(!explore_callback(value, shared_from_this(), depth, userdata)) return false;
            return true;
        }

        void Print(std::ostream& ostream) override {
            ostream << display << *value;
        }

        bool Save(std::ostream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>) override;
        static std::shared_ptr<DereferenceOp> Load(std::istream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>&);

    private:
        std::shared_ptr<BaseExpressionNode> value;
        std::string display;
        dereference_func_t dereference_func;
    };


    class FunctionCall : public BaseExpressionNode {
    public:

        FunctionCall(std::string const& _display_name, std::string const& _name, 
                     std::string const& _lp_display,
                     std::shared_ptr<BaseExpressionNode>& _args, 
                     std::string const& _rp_display)
            : display_name(_display_name), name(_name),
              lp_display(_lp_display),
              args(_args),
              rp_display(_rp_display)
        { assert(args); }

        virtual ~FunctionCall() {}

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return FunctionCall::base_expression_node_id; }

        bool Evaluate(s64* result, std::string& errmsg) const override {
            std::stringstream ss;
            ss << "Function calls are not implemented, trying to call `" << name << "`";
            errmsg = ss.str();
            return false;
        }

        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            // depth first into the arguments
            if(!args->Explore(explore_callback, depth + 1, userdata)) return false; 
            // and then evaluate the node
            if(!explore_callback(args, shared_from_this(), depth, userdata)) return false;
            return true;
        }

        void Print(std::ostream& ostream) override {
            ostream << display_name << lp_display << *args << rp_display;
        }

        bool Save(std::ostream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>) override;
        static std::shared_ptr<FunctionCall> Load(std::istream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>&);

    private:
        std::string display_name;
        std::string name;
        std::string lp_display;
        std::shared_ptr<BaseExpressionNode> args;
        std::string rp_display;
    };

    struct BaseExpressionNodeListEntry {
        std::string display;
        std::shared_ptr<BaseExpressionNode> node;
    };

    class ExpressionList : public BaseExpressionNode {
    public:

        ExpressionList(std::vector<BaseExpressionNodeListEntry>& _list)
            : list(_list)
        { assert(list.size() >= 2); }

        virtual ~ExpressionList() {}

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return ExpressionList::base_expression_node_id; }

        int GetSize() const { return list.size(); }

        std::shared_ptr<BaseExpressionNode> GetNode(int i, std::string* out = nullptr) {
            assert(i < GetSize());
            if(out != nullptr) *out = list[i].display;
            return list[i].node;
        }

        bool Evaluate(s64* result, std::string& errmsg) const override {
            errmsg = "Expression lists aren't evaluatable";
            return false;
        }

        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            for(auto& e : list) {
                // depth first into each arguments
                if(!e.node->Explore(explore_callback, depth + 1, userdata)) return false; 
                // and then evaluate the node
                if(!explore_callback(e.node, shared_from_this(), depth, userdata)) return false;
            }
            return true;
        }

        void Print(std::ostream& ostream) override {
            for(auto& le : list) {
                ostream << le.display << *le.node;
            }
        }

        bool Save(std::ostream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>) override;
        static std::shared_ptr<ExpressionList> Load(std::istream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>&);

    private:
        std::vector<BaseExpressionNodeListEntry> list;
    };

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
        int id = BaseExpressionNodeCreator::expression_nodes.size() + expression_node_id_offset;
        auto info = std::make_shared<BaseExpressionNodeInfo>(BaseExpressionNodeInfo {
            .load = std::bind(&T::Load, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
        });
        BaseExpressionNodeCreator::expression_nodes.push_back(info);
        T::base_expression_node_id = id;
    }


    template<class T>
    BN CreateConstant(T value, std::string const& display) {
        return std::make_shared<BaseExpressionNodes::Constant<T>>(value, display);
    }

    // TODO get rid of this. we may want a String type in the future though
    // Code should use OperandAddressOrLabel
    BN CreateName(std::string const& s) {
        return std::make_shared<BaseExpressionNodes::Name>(s);
    }

    BN CreateParens(std::string const& left, BN& value, std::string const& right) {
        return std::make_shared<BaseExpressionNodes::Parens>(left, value, right);
    }

    BN CreateFunctionCall(std::string const& display_name, std::string const& name,
                          std::string const& lp_display, BN& args, std::string const& rp_display) {
        return std::make_shared<BaseExpressionNodes::FunctionCall>(display_name, name, lp_display, args, rp_display);
    }

    BN CreateList(std::vector<BaseExpressionNodes::BaseExpressionNodeListEntry>& list) {
        return std::make_shared<BaseExpressionNodes::ExpressionList>(list);
    }

    BN CreateAddOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::AddOp>(left, display, right);
    }

    BN CreateSubtractOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::SubtractOp>(left, display, right);
    }

    BN CreateMultiplyOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::MultiplyOp>(left, display, right);
    }

    BN CreateDivideOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::DivideOp>(left, display, right);
    }

    BN CreatePowerOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::PowerOp>(left, display, right);
    }

    BN CreateOrOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::OrOp>(left, display, right);
    }

    BN CreateXorOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::XorOp>(left, display, right);
    }

    BN CreateAndOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::AndOp>(left, display, right);
    }

    BN CreateLShiftOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::LShiftOp>(left, display, right);
    }

    BN CreateRShiftOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::RShiftOp>(left, display, right);
    }

    BN CreateEqualToOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::EqualToOp>(left, display, right);
    }

    BN CreateNotEqualToOp(BN& left, std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::NotEqualToOp>(left, display, right);
    }

    BN CreatePositiveOp(std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::PositiveOp>(display, right);
    }

    BN CreateNegateOp(std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::NegateOp>(display, right);
    }

    BN CreateBinaryNotOp(std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::BinaryNotOp>(display, right);
    }

    BN CreateLogicalNotOp(std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::LogicalNotOp>(display, right);
    }

    BN CreateDereferenceOp(std::string const& display, BN& right) {
        return std::make_shared<BaseExpressionNodes::DereferenceOp>(display, right);
    }

    virtual bool Save(std::shared_ptr<BaseExpressionNode> const&, std::ostream&, std::string&);
    virtual BN Load(std::istream&, std::string&);

private:
    static std::vector<std::shared_ptr<BaseExpressionNodeInfo>> expression_nodes;
    static int expression_node_id_offset;
};

class Tenderizer;

// implement expressions as abstract syntax trees
class BaseExpression : public std::enable_shared_from_this<BaseExpression> {
public:
    BaseExpression();
    virtual ~BaseExpression();

    template <class T>
    void Set(T& t) { root = t; }
    bool Set(std::string const&, std::string& errmsg, int& errloc, bool start_list = true);

    std::shared_ptr<BaseExpressionNode> GetRoot() { return root; }

    bool Evaluate(s64* result, std::string& errmsg) { 
        if(!root) {
            errmsg = "No expression set";
            return false;
        }
        return root->Evaluate(result, errmsg); 
    }

    bool Explore(explore_callback_t, void*);

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
    int parens_depth;

protected:
    virtual std::shared_ptr<BaseExpressionNode> ParseExpressionList    (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);
    virtual std::shared_ptr<BaseExpressionNode> ParseExpression        (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);
    virtual std::shared_ptr<BaseExpressionNode> ParseOrExpression      (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);
    virtual std::shared_ptr<BaseExpressionNode> ParseXorExpression     (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);
    virtual std::shared_ptr<BaseExpressionNode> ParseAndExpression     (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);
    virtual std::shared_ptr<BaseExpressionNode> ParseEqualityExpression(std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);
    virtual std::shared_ptr<BaseExpressionNode> ParseShiftExpression   (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);
    virtual std::shared_ptr<BaseExpressionNode> ParseAddExpression     (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);
    virtual std::shared_ptr<BaseExpressionNode> ParseMulExpression     (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);
    virtual std::shared_ptr<BaseExpressionNode> ParsePowerExpression   (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);
    virtual std::shared_ptr<BaseExpressionNode> ParseUnaryExpression   (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);
    virtual std::shared_ptr<BaseExpressionNode> ParsePrimaryExpression (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);
    virtual std::shared_ptr<BaseExpressionNode> ParseParenExpression   (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&);

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
