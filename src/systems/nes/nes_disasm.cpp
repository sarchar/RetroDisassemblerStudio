#include <cassert>
#include <iomanip>
#include <sstream>
#include <string>

#include "systems/nes/nes_defs.h"
#include "systems/nes/nes_disasm.h"

using namespace std;

namespace NES {

static char const * const OPCODE_MNEMONICS[] = {
//      0          1          2          3          4          5          6          7          8          9          A          B          C          D          E          F
    "UNKNOWN", "ORA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "ORA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "ORA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "ORA"    , "UNKNOWN", "UNKNOWN", // 0
    "BPL"    , "ORA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "ORA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "ORA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "ORA"    , "UNKNOWN", "UNKNOWN", // 1
    "JSR"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", // 2
    "BMI"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", // 3
    "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "JMP"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", // 4
    "BVC"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", // 5
    "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "JMP"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", // 6
    "BVS"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "SEI"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", // 7
    "UNKNOWN", "STA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "STA"    , "UNKNOWN", "UNKNOWN", "DEY"    , "UNKNOWN", "TXA"    , "UNKNOWN", "UNKNOWN", "STA"    , "UNKNOWN", "UNKNOWN", // 8
    "BCC"    , "STA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "STA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "STA"    , "TXS"    , "UNKNOWN", "UNKNOWN", "STA"    , "UNKNOWN", "UNKNOWN", // 9
    "LDY"    , "LDA"    , "LDX"    , "UNKNOWN", "UNKNOWN", "LDA"    , "UNKNOWN", "UNKNOWN", "TAY"    , "LDA"    , "TAX"    , "UNKNOWN", "UNKNOWN", "LDA"    , "UNKNOWN", "UNKNOWN", // A
    "BCS"    , "LDA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "LDA"    , "UNKNOWN", "UNKNOWN", "TYA"    , "LDA"    , "TSX"    , "UNKNOWN", "UNKNOWN", "LDA"    , "UNKNOWN", "UNKNOWN", // B
    "CPY"    , "CMP"    , "UNKNOWN", "UNKNOWN", "CPY"    , "CMP"    , "UNKNOWN", "UNKNOWN", "INY"    , "CMP"    , "DEX"    , "UNKNOWN", "CPY"    , "CMP"    , "UNKNOWN", "UNKNOWN", // C
    "BNE"    , "CMP"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "CMP"    , "UNKNOWN", "UNKNOWN", "CLD"    , "CMP"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "CMP"    , "UNKNOWN", "UNKNOWN", // D
    "CPX"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "CPX"    , "UNKNOWN", "INC"    , "UNKNOWN", "INX"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "CPX"    , "UNKNOWN", "INC"    , "UNKNOWN", // E
    "BEQ"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "INC"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "INC"    , "UNKNOWN"  // F
};

static int const OPCODE_SIZES[] = {
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, // 0
    2, 2, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 3, 0, 0, // 1
    3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 2
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 3
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, // 4
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 5
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, // 6
    2, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, // 7
    0, 2, 0, 0, 0, 2, 0, 0, 1, 0, 1, 0, 0, 3, 0, 0, // 8
    2, 2, 0, 0, 0, 2, 0, 0, 1, 3, 1, 0, 0, 3, 0, 0, // 9
    2, 2, 2, 0, 0, 2, 0, 0, 1, 2, 1, 0, 0, 3, 0, 0, // A
    2, 2, 0, 0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 3, 0, 0, // B
    2, 2, 0, 0, 2, 2, 0, 0, 1, 2, 1, 0, 3, 3, 0, 0, // C
    2, 2, 0, 0, 0, 2, 0, 0, 1, 3, 0, 0, 0, 3, 0, 0, // D
    2, 0, 0, 0, 2, 0, 2, 0, 1, 0, 0, 0, 3, 0, 3, 0, // E
    2, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 3, 0  // F
};

static ADDRESSING_MODE const OPCODE_MODES[] = {
//        0              1              2              3              4              5              6              7              8              9              A              B              C              D              E              F
    UNIMPLEMENTED, AM_INDIRECT_X, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE  , UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_IMMEDIATE , UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE  , UNIMPLEMENTED, UNIMPLEMENTED, // 0
    AM_RELATIVE  , AM_INDIRECT_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_X, UNIMPLEMENTED, UNIMPLEMENTED, // 1
    AM_ABSOLUTE  , UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, // 2
    AM_RELATIVE  , UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, // 3
    UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_INDIRECT  , UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, // 4
    AM_RELATIVE  , UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, // 5
    UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_INDIRECT  , UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, // 6
    AM_RELATIVE  , UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_IMPLIED   , UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, // 7
    UNIMPLEMENTED, AM_INDIRECT_X, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE  , UNIMPLEMENTED, UNIMPLEMENTED, AM_IMPLIED   , UNIMPLEMENTED, AM_IMPLIED   , UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE  , UNIMPLEMENTED, UNIMPLEMENTED, // 8
    AM_RELATIVE  , AM_INDIRECT_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, UNIMPLEMENTED, UNIMPLEMENTED, AM_IMPLIED   , AM_ABSOLUTE_Y, AM_IMPLIED   , UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_X, UNIMPLEMENTED, UNIMPLEMENTED, // 9
    AM_IMMEDIATE , AM_INDIRECT_X, AM_IMMEDIATE , UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE  , UNIMPLEMENTED, UNIMPLEMENTED, AM_IMPLIED   , AM_IMMEDIATE , AM_IMPLIED   , UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE  , UNIMPLEMENTED, UNIMPLEMENTED, // A
    AM_RELATIVE  , AM_INDIRECT_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_Y, AM_IMPLIED   , UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_X, UNIMPLEMENTED, UNIMPLEMENTED, // B
    AM_IMMEDIATE , AM_INDIRECT_X, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE  , AM_ZEROPAGE  , UNIMPLEMENTED, UNIMPLEMENTED, AM_IMPLIED   , AM_IMMEDIATE , AM_IMPLIED   , UNIMPLEMENTED, AM_ABSOLUTE  , AM_ABSOLUTE  , UNIMPLEMENTED, UNIMPLEMENTED, // C
    AM_RELATIVE  , AM_INDIRECT_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, UNIMPLEMENTED, UNIMPLEMENTED, AM_IMPLIED   , AM_ABSOLUTE_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_X, UNIMPLEMENTED, UNIMPLEMENTED, // D
    AM_IMMEDIATE , UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE  , UNIMPLEMENTED, AM_ZEROPAGE  , UNIMPLEMENTED, AM_IMPLIED   , UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE  , UNIMPLEMENTED, AM_ABSOLUTE  , UNIMPLEMENTED, // E
    AM_RELATIVE  , UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_X, UNIMPLEMENTED  // F
};

Disassembler::Disassembler() 
{
}

Disassembler::~Disassembler() 
{
}

char const* Disassembler::GetInstructionC(u8 opcode)
{
    return OPCODE_MNEMONICS[opcode];
}

string Disassembler::GetInstruction(u8 opcode) 
{
    return string(OPCODE_MNEMONICS[opcode]);
}

int Disassembler::GetInstructionSize(u8 opcode) 
{
    return OPCODE_SIZES[opcode];
}

ADDRESSING_MODE Disassembler::GetAddressingMode(u8 opcode)
{
    return OPCODE_MODES[opcode];
}

std::string Disassembler::FormatOperand(u8 opcode, u8 const* operands)
{
    stringstream ss;

    switch(Disassembler::GetAddressingMode(opcode)) {
    case AM_IMMEDIATE:
        ss << "#$" << hex << setw(2) << setfill('0') << uppercase << (int)operands[0];
        break;

    case AM_ZEROPAGE:
        ss << "$" << hex << setw(2) << setfill('0') << uppercase << (int)operands[0] << "";
        break;

    case AM_ZEROPAGE_X:
        ss << "$" << hex << setw(2) << setfill('0') << uppercase << (int)operands[0] << ",X";
        break;

    case AM_ZEROPAGE_Y:
        ss << "$" << hex << setw(2) << setfill('0') << uppercase << (int)operands[0] << ",Y";
        break;

    case AM_ABSOLUTE:
    {
        u16 w = (u16)operands[0] | ((u16)operands[1] << 8);
        ss << "$" << hex << setw(4) << setfill('0') << uppercase << w;
        break;
    }

    case AM_ABSOLUTE_X:
    {
        u16 w = (u16)operands[0] | ((u16)operands[1] << 8);
        ss << "$" << hex << setw(4) << setfill('0') << uppercase << w << ",X";
        break;
    }

    case AM_ABSOLUTE_Y:
    {
        u16 w = (u16)operands[0] | ((u16)operands[1] << 8);
        ss << "$" << hex << setw(4) << setfill('0') << uppercase << w << ",Y";
        break;
    }

    case AM_INDIRECT:
    {
        u16 w = (u16)operands[0] | ((u16)operands[1] << 8);
        ss << "($" << hex << setw(4) << setfill('0') << uppercase << w << ")";
        break;
    }

    case AM_INDIRECT_X:
        ss << "($" << hex << setw(2) << setfill('0') << uppercase << (int)operands[0] << ",X)";
        break;

    case AM_INDIRECT_Y:
        ss << "($" << hex << setw(2) << setfill('0') << uppercase << (int)operands[0] << "),Y";
        break;

    case AM_RELATIVE:
    {
        ss << "rel $" << hex << setw(2) << setfill('0') << uppercase << (int)operands[0];
        break;
    }

    case AM_IMPLIED:
    default:
        assert(false); // don't call me
        return "";
    }

    return ss.str();
}

}
