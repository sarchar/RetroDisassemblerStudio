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
        OperandAddressOrLabel(std::shared_ptr<MemoryObject> const& _target, GlobalMemoryLocation const& _where, int _nth, std::string const& _display)
            : target(_target), where(_where), nth(_nth), display(_display)
        { }
        virtual ~OperandAddressOrLabel() { }

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const&, s64* result) const override {
            *result = where.address;
            return true;
        }

        void Print(std::ostream& ostream) const override;

        bool Save(std::ostream& os, std::string& errmsg) override {
            if(!where.Save(os, errmsg)) return false;
            WriteVarInt(os, nth);
            WriteString(os, display);
            return true;
        }

    private:
        std::weak_ptr<MemoryObject> target;
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

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            return false;
        }

        void Print(std::ostream& ostream) const override {
            ostream << display;
        }

        bool Save(std::ostream& os, std::string& errmsg) override {
            WriteString(os, display);
            return true;
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

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            return value->Evaluate(helper, result);
        }

        void Print(std::ostream& ostream) const override {
            ostream << display << *value;
        }

        bool Save(std::ostream& os, std::string& errmsg) override {
            WriteString(os, display);
            if(!value->Save(os, errmsg)) return false;
            return true;
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

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            return false;
        }

        void Print(std::ostream& ostream) const override {
            ostream << *base << display;
        }

        bool Save(std::ostream& os, std::string& errmsg) override {
            if(!base->Save(os, errmsg)) return false;
            WriteString(os, display);
            return true;
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

        bool Evaluate(std::shared_ptr<BaseExpressionHelper> const& helper, s64* result) const override {
            return false;
        }

        void Print(std::ostream& ostream) const override {
            ostream << *base << display;
        }

        bool Save(std::ostream& os, std::string& errmsg) override {
            if(!base->Save(os, errmsg)) return false;
            WriteString(os, display);
            return true;
        }

    private:
        std::shared_ptr<BaseExpressionNode> base;
        std::string display;
    };
}

class ExpressionNodeCreator : public BaseExpressionNodeCreator {
public:
    typedef std::shared_ptr<BaseExpressionNode> BN;

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

    BN CreateOperandAddressOrLabel(std::shared_ptr<MemoryObject> const& target, GlobalMemoryLocation const& where, int nth, std::string const& display) {
        return std::make_shared<ExpressionNodes::OperandAddressOrLabel>(target, where, nth, display);
    }
};

class Expression : public BaseExpression {
public:
    std::shared_ptr<BaseExpressionNodeCreator> GetNodeCreator() override {
        return std::make_shared<ExpressionNodeCreator>();
    }
};

};
