#pragma once

#include <memory>

#include "systems/expressions.h"
#include "systems/nes/nes_memory.h"

namespace NES {

class ExpressionHelper : public BaseExpressionHelper {
};

class ExpressionNode : public BaseExpressionNode {
};

namespace ExpressionNodes {
    class OperandAddressOrLabel : public ExpressionNode {
    public:
        OperandAddressOrLabel(GlobalMemoryLocation const& _where, int _nth, std::string const& _display)
            : where(_where), nth(_nth), display(_display)
        { }
        virtual ~OperandAddressOrLabel() { }

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return OperandAddressOrLabel::base_expression_node_id; }

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const&, s64* result) const override {
            *result = where.address;
            return true;
        }

        void Print(std::ostream& ostream) const override;

        bool Save(std::ostream& os, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>) override {
            if(!where.Save(os, errmsg)) return false;
            WriteVarInt(os, nth);
            WriteString(os, display);
            return true;
        }

        static std::shared_ptr<OperandAddressOrLabel> Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>&) {
            GlobalMemoryLocation where;
            if(!where.Load(is, errmsg)) return nullptr;
            int nth = ReadVarInt<int>(is);
            std::string display;
            ReadString(is, display);
            if(!is.good()) {
                errmsg = "Error loading OperandAddressOrLabel";
                return nullptr;
            }
            return std::make_shared<OperandAddressOrLabel>(where, nth, display);
        }

    private:
        GlobalMemoryLocation        where;
        int nth;
        std::string                 display;
    };

    class Accum : public ExpressionNode {
    public:
        Accum(std::string const& _display)
            : display(_display)
        { }
        virtual ~Accum() { }

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return Accum::base_expression_node_id; }

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            return false;
        }

        void Print(std::ostream& ostream) const override {
            ostream << display;
        }

        bool Save(std::ostream& os, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>) override {
            WriteString(os, display);
            return true;
        }

        static std::shared_ptr<Accum> Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>&) {
            std::string display;
            ReadString(is, display);
            if(!is.good()) {
                errmsg = "Could not load Accum";
                return nullptr;
            }
            return std::make_shared<Accum>(display);
        }
    private:
        std::string display;
    };

    class Immediate : public ExpressionNode {
    public:
        Immediate(std::string const& _display, std::shared_ptr<BaseExpressionNode>& _value)
            : display(_display), value(_value)
        { assert(value); }
        virtual ~Immediate() { }

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return Immediate::base_expression_node_id; }

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            return value->Evaluate(helper, result);
        }

        void Print(std::ostream& ostream) const override {
            ostream << display << *value;
        }

        bool Save(std::ostream& os, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator> creator) override {
            WriteString(os, display);
            if(!creator->Save(value, os, errmsg)) return false;
            return true;
        }

        static std::shared_ptr<Immediate> Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) {
            std::string display;
            ReadString(is, display);
            if(!is.good()) {
                errmsg = "Could not load Accum";
                return nullptr;
            }
            auto value = creator->Load(is, errmsg);
            if(!value) return nullptr;
            return std::make_shared<Immediate>(display, value);
        }
    private:
        std::string display;
        std::shared_ptr<BaseExpressionNode> value;
    };

    class IndexedX : public ExpressionNode {
    public:
        IndexedX(std::shared_ptr<BaseExpressionNode>& _base, std::string const& _display)
            : base(_base), display(_display)
        { }
        virtual ~IndexedX() { }

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return IndexedX::base_expression_node_id; }

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            return false;
        }

        void Print(std::ostream& ostream) const override {
            ostream << *base << display;
        }

        bool Save(std::ostream& os, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator> creator) override {
            if(!creator->Save(base, os, errmsg)) return false;
            WriteString(os, display);
            return true;
        }

        static std::shared_ptr<IndexedX> Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) {
            auto base = creator->Load(is, errmsg);
            if(!base) return nullptr;

            std::string display;
            ReadString(is, display);
            if(!is.good()) {
                errmsg = "Could not load IndexedX";
                return nullptr;
            }

            return std::make_shared<IndexedX>(base, display);
        }
    private:
        std::shared_ptr<BaseExpressionNode> base;
        std::string display;
    };

    class IndexedY : public ExpressionNode {
    public:
        IndexedY(std::shared_ptr<BaseExpressionNode>& _base, std::string const& _display)
            : base(_base), display(_display)
        { }
        virtual ~IndexedY() { }

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return IndexedY::base_expression_node_id; }

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            return false;
        }

        void Print(std::ostream& ostream) const override {
            ostream << *base << display;
        }

        bool Save(std::ostream& os, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator> creator) override {
            if(!creator->Save(base, os, errmsg)) return false;
            WriteString(os, display);
            return true;
        }

        static std::shared_ptr<IndexedY> Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) {
            auto base = creator->Load(is, errmsg);
            if(!base) return nullptr;

            std::string display;
            ReadString(is, display);
            if(!is.good()) {
                errmsg = "Could not load IndexedY";
                return nullptr;
            }

            return std::make_shared<IndexedY>(base, display);
        }
    private:
        std::shared_ptr<BaseExpressionNode> base;
        std::string display;
    };
}

class ExpressionNodeCreator : public BaseExpressionNodeCreator {
public:
    typedef std::shared_ptr<BaseExpressionNode> BN;

    static void RegisterExpressionNodes();

    BN CreateAccum(std::string const& display) {
        return std::make_shared<ExpressionNodes::Accum>(display);
    }

    BN CreateImmediate(std::string const& display, BN& value) {
        return std::make_shared<ExpressionNodes::Immediate>(display, value);
    }

    BN CreateIndexedX(BN& base, std::string const& display) {
        return std::make_shared<ExpressionNodes::IndexedX>(base, display);
    }

    BN CreateIndexedY(BN& base, std::string const& display) {
        return std::make_shared<ExpressionNodes::IndexedY>(base, display);
    }

    BN CreateOperandAddressOrLabel(GlobalMemoryLocation const& where, int nth, std::string const& display) {
        return std::make_shared<ExpressionNodes::OperandAddressOrLabel>(where, nth, display);
    }
};

class Expression : public BaseExpression {
public:
    std::shared_ptr<BaseExpressionNodeCreator> GetNodeCreator() override {
        return std::make_shared<ExpressionNodeCreator>();
    }
};

};
