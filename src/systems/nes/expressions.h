// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <memory>

#include "systems/expressions.h"
#include "systems/nes/defines.h"
#include "systems/nes/memory.h"

namespace Systems::NES {

class Label;

class ExpressionNode : public BaseExpressionNode {
};

namespace ExpressionNodes {
    class Define : public ExpressionNode {
    public:
        Define(std::shared_ptr<NES::Define> const& _define)
            : define(_define) {}
        virtual ~Define() { }

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return Define::base_expression_node_id; }

        std::shared_ptr<NES::Define> GetDefine() { return define; }

        // Define evaluate to their value, simple
        bool Evaluate(s64* result, std::string& errmsg) const override {
            *result = define->Evaluate();
            return true;
        }

        // Define has no child ExpressionNode
        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            return true;
        }

        void Print(std::ostream& ostream) override;

        bool Save(std::ostream& os, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>) override;
        static std::shared_ptr<Define> Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>&);

    private:
        std::shared_ptr<NES::Define> define;
    };

    class Label : public ExpressionNode {
    public:
        Label(GlobalMemoryLocation const&, int _nth, std::string const& _display);
        virtual ~Label() { }

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return Label::base_expression_node_id; }

        bool NoteReference(GlobalMemoryLocation const&);
        void RemoveReference(GlobalMemoryLocation const&);

        void SetLongMode(bool _v) { long_mode = _v; }

        // You're expected to call RemoveReference before and NoteReference() after
        void NextLabel();
        bool Update();

        void Reset() {
            label.reset();
        }

        std::shared_ptr<NES::Label> GetLabel() { return label.lock(); }
        GlobalMemoryLocation const& GetTarget() const { return where; }
        std::string const&          GetDisplay() const { return display; }
        int                         GetNth() const { return nth; }

        // Labels evaluate to their address, whether they be zero page or not
        bool Evaluate(s64* result, std::string& errmsg) const override {
            if(long_mode) {
                *result = where.address + offset;
                *result += (where.is_chr ? where.chr_rom_bank : where.prg_rom_bank) * 0x10000;
            } else {
                *result = where.address + offset;
            }
            return true;
        }

        // Label has no child ExpressionNode
        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            return true;
        }

        void Print(std::ostream& ostream) override;

        bool Save(std::ostream& os, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>) override;
        static std::shared_ptr<Label> Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>&);

    private:
        std::weak_ptr<NES::Label>   label;
        GlobalMemoryLocation        where;
        int                         nth;
        int                         offset;
        std::string                 display;
        bool                        long_mode;
    };

    class Accum : public ExpressionNode {
    public:
        Accum(std::string const& _display)
            : display(_display)
        { }
        virtual ~Accum() { }

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return Accum::base_expression_node_id; }

        bool Evaluate(s64* result, std::string& errmsg) const override {
            errmsg = "Accum cannot be evaluated";
            return false;
        }

        // Accum has no child ExpressionNode
        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            return true;
        }

        void Print(std::ostream& ostream) override {
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

        std::shared_ptr<BaseExpressionNode> GetValue() { return value; }

        bool Evaluate(s64* result, std::string& errmsg) const override {
            errmsg = "Immediate nodes are not evaluateable";
            return false;
        }

        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            // depth first into the expression tree
            if(!value->Explore(explore_callback, depth + 1, userdata)) return false; 
            // and then evaluate the actual node
            if(!explore_callback(value, shared_from_this(), depth, userdata)) return false;
            return true;
        }

        void Print(std::ostream& ostream) override {
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

        std::shared_ptr<BaseExpressionNode> GetBase() { return base; }

        bool Evaluate(s64* result, std::string& errmsg) const override {
            errmsg = "IndexedX nodes are not evaluateable";
            return false;
        }

        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            // depth first into the expression tree
            if(!base->Explore(explore_callback, depth + 1, userdata)) return false; 
            // and then evaluate the actual node
            if(!explore_callback(base, shared_from_this(), depth, userdata)) return false;
            return true;
        }

        void Print(std::ostream& ostream) override {
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

        std::shared_ptr<BaseExpressionNode> GetBase() { return base; }

        bool Evaluate(s64* result, std::string& errmsg) const override {
            errmsg = "IndexedY nodes are not evaluateable";
            return false;
        }

        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            // depth first into the expression tree
            if(!base->Explore(explore_callback, depth + 1, userdata)) return false; 
            // and then evaluate the actual node
            if(!explore_callback(base, shared_from_this(), depth, userdata)) return false;
            return true;
        }

        void Print(std::ostream& ostream) override {
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

    class SystemInstanceState : public BaseExpressionNode {
    public:
        typedef std::function<s64()> get_state_func_t;

        SystemInstanceState(std::string const& _display)
            : display(_display)
        {
        }

        virtual ~SystemInstanceState() {}

        static int base_expression_node_id;
        int GetExpressionNodeType() const override { return SystemInstanceState::base_expression_node_id; }

        std::string const& GetString() const { return display; }

        template<typename T>
        void SetGetStateFunction(T const& func) { get_state_func = func; }

        bool Evaluate(s64* result, std::string& errmsg) const override {
            // get_state_func never fails. if anything it returns garbage data
            if(!get_state_func) {
                errmsg = "Get state function not specified";
                return false;
            }
            *result = get_state_func();
            return true;
        }

        bool Explore(explore_callback_t explore_callback, int depth, void* userdata) override {
            // SystemInstanceState has no child ExpressionNode
            return true;
        }

        void Print(std::ostream& ostream) override {
            ostream << display;
        }

        bool Save(std::ostream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>) override;
        static std::shared_ptr<SystemInstanceState> Load(std::istream&, std::string&, std::shared_ptr<BaseExpressionNodeCreator>&);

    private:
        std::string display;
        get_state_func_t get_state_func;
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

    BN CreateDefine(std::shared_ptr<Define> const& define) {
        return std::make_shared<ExpressionNodes::Define>(define);
    }

    BN CreateLabel(GlobalMemoryLocation const& label_address, int nth, std::string const& display) {
        return std::make_shared<ExpressionNodes::Label>(label_address, nth, display);
    }

    BN CreateSystemInstanceState(std::string const& display) {
        return std::make_shared<ExpressionNodes::SystemInstanceState>(display);
    }
};

class Expression : public BaseExpression {
public:
    std::shared_ptr<BaseExpressionNodeCreator> GetNodeCreator() override {
        return std::make_shared<ExpressionNodeCreator>();
    }

protected:
    std::shared_ptr<BaseExpressionNode> ParseExpression     (std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&) override;
    std::shared_ptr<BaseExpressionNode> ParseParenExpression(std::shared_ptr<Tenderizer>&, std::shared_ptr<BaseExpressionNodeCreator>&, std::string&, int&) override;
};

} // namespace Systems::NES
