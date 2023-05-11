#include "systems/expressions.h"

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


