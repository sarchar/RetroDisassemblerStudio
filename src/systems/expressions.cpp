#include <functional>

#include "systems/expressions.h"

using namespace std;

std::vector<std::shared_ptr<BaseExpressionNodeCreator::BaseExpressionNodeInfo>> BaseExpressionNodeCreator::expression_nodes;

BaseExpressionHelper::BaseExpressionHelper()
{
}

BaseExpressionHelper::~BaseExpressionHelper()
{
}


BaseExpressionNode::BaseExpressionNode()
{
}

BaseExpressionNode::~BaseExpressionNode()
{
}

std::ostream& operator<<(std::ostream& stream, BaseExpressionNode const& node)
{
    std::ios_base::fmtflags saveflags(stream.flags());

    node.Print(stream);
    //!stream << *e.root;
    //!stream << "GlobalMemoryLocation(address=0x" << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << p.address;
    //!stream << ", prg_rom_bank=" << std::dec << std::setw(0) << p.prg_rom_bank;
    //!stream << ", chr_rom_bank=" << std::dec << std::setw(0) << p.chr_rom_bank;
    //!stream << ", is_chr=" << p.is_chr;
    //!stream << ")";

    stream.flags(saveflags);
    return stream;
}

BaseExpressionNodeCreator::BaseExpressionNodeCreator()
{
}

BaseExpressionNodeCreator::~BaseExpressionNodeCreator()
{
}

void BaseExpressionNodeCreator::RegisterBaseExpressionNodes()
{
    RegisterBaseExpressionNode<BaseExpressionNodes::Constant<u8>>();
    RegisterBaseExpressionNode<BaseExpressionNodes::Constant<u16>>();
    RegisterBaseExpressionNode<BaseExpressionNodes::Name>();
    RegisterBaseExpressionNode<BaseExpressionNodes::AddOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::SubtractOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::MultiplyOp>();
    RegisterBaseExpressionNode<BaseExpressionNodes::DivideOp>();
}

bool BaseExpressionNodeCreator::Save(shared_ptr<BaseExpressionNode> const& node, ostream& os, string& errmsg)
{
    WriteVarInt(os, node->GetExpressionNodeType());
    if(!os.good()) return false;
    return node->Save(os, errmsg, shared_from_this());
}

std::shared_ptr<BaseExpressionNode> BaseExpressionNodeCreator::Load(std::istream& is, std::string& errmsg)
{
    u32 node_type = ReadVarInt<u32>(is);
    if(!is.good()) {
        errmsg = "Error reading expression node";
        return nullptr;
    }

    if(node_type >= expression_nodes.size()) {
        errmsg = "Invalid expression node type";
        return nullptr;
    }

    auto ptr = shared_from_this();
    return expression_nodes[node_type]->load(is, errmsg, ptr);
}

BaseExpression::BaseExpression()
{
}

BaseExpression::~BaseExpression()
{
}

std::ostream& operator<<(std::ostream& stream, BaseExpression const& e)
{
    std::ios_base::fmtflags saveflags(stream.flags());

    if(e.root) stream << *e.root;
    //!stream << "GlobalMemoryLocation(address=0x" << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << p.address;
    //!stream << ", prg_rom_bank=" << std::dec << std::setw(0) << p.prg_rom_bank;
    //!stream << ", chr_rom_bank=" << std::dec << std::setw(0) << p.chr_rom_bank;
    //!stream << ", is_chr=" << p.is_chr;
    //!stream << ")";

    stream.flags(saveflags);
    return stream;
};

namespace BaseExpressionNodes {

int Parens::base_expression_node_id = 0;
template <class T>
int Constant<T>::base_expression_node_id = 0;
int Name::base_expression_node_id = 0;
template <s64 (*T)(s64, s64)>
int BinaryOp<T>::base_expression_node_id = 0;

bool Parens::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator> creator) 
{
    WriteString(os, left);
    if(!creator->Save(value, os, errmsg)) return false;
    WriteString(os, right);
    return true;
}

std::shared_ptr<Parens> Parens::Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) 
{
    string left;
    ReadString(is, left);
    if(!is.good()) {
        errmsg = "Could not load Parens";
        return nullptr;
    }

    auto value = creator->Load(is, errmsg);
    if(!value) return nullptr;

    string right;
    ReadString(is, right);
    if(!is.good()) {
        errmsg = "Could not load Parens";
        return nullptr;
    }

    return std::make_shared<Parens>(left, value, right);
}

template <class T>
bool Constant<T>::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator> creator) 
{
    WriteVarInt(os, value);
    WriteString(os, display);
    return true;
}

template <class T>
std::shared_ptr<Constant<T>> Constant<T>::Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) 
{
    T value = ReadVarInt<T>(is);
    string display;
    ReadString(is, display);
    if(!is.good()) {
        errmsg = "Could not load Constant<T>";
        return nullptr;
    }

    return std::make_shared<Constant<T>>(value, display);
}

bool Name::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator> creator) 
{
    WriteString(os, name);
    return true;
}

std::shared_ptr<Name> Name::Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) 
{
    string display;
    ReadString(is, display);
    if(!is.good()) {
        errmsg = "Could not load Constant<T>";
        return nullptr;
    }

    return std::make_shared<Name>(display);
}

template <s64 (*T)(s64, s64)>
bool BinaryOp<T>::Save(ostream& os, string& errmsg, shared_ptr<BaseExpressionNodeCreator> creator) 
{
    if(!creator->Save(left, os, errmsg)) return false;
    WriteString(os, display);
    if(!creator->Save(right, os, errmsg)) return false;
    return true;
}

template <s64 (*T)(s64, s64)>
std::shared_ptr<BinaryOp<T>> BinaryOp<T>::Load(std::istream& is, std::string& errmsg, std::shared_ptr<BaseExpressionNodeCreator>& creator) 
{
    auto left = creator->Load(is, errmsg);
    if(!left) return nullptr;

    string display;
    ReadString(is, display);
    if(!is.good()) {
        errmsg = "Could not load Constant<T>";
        return nullptr;
    }

    auto right = creator->Load(is, errmsg);
    if(!right) return nullptr;

    return std::make_shared<BinaryOp<T>>(left, display, right);
}

}

