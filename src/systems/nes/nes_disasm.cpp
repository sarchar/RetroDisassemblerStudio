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
    "BRK"    , "ORA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "ORA"    , "ASL"    , "UNKNOWN", "PHP"    , "ORA"    , "ASL"    , "UNKNOWN", "UNKNOWN", "ORA"    , "ASL"    , "UNKNOWN", // 0
    "BPL"    , "ORA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "ORA"    , "ASL"    , "UNKNOWN", "CLC"    , "ORA"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "ORA"    , "ASL"    , "UNKNOWN", // 1
    "JSR"    , "AND"    , "UNKNOWN", "UNKNOWN", "BIT"    , "AND"    , "ROL"    , "UNKNOWN", "PLP"    , "AND"    , "ROL"    , "UNKNOWN", "BIT"    , "AND"    , "ROL"    , "UNKNOWN", // 2
    "BMI"    , "AND"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "AND"    , "ROL"    , "UNKNOWN", "SEC"    , "AND"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "AND"    , "ROL"    , "UNKNOWN", // 3
    "RTI"    , "EOR"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "EOR"    , "LSR"    , "UNKNOWN", "PHA"    , "EOR"    , "LSR"    , "UNKNOWN", "JMP"    , "EOR"    , "LSR"    , "UNKNOWN", // 4
    "BVC"    , "EOR"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "EOR"    , "LSR"    , "UNKNOWN", "CLI"    , "EOR"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "EOR"    , "LSR"    , "UNKNOWN", // 5
    "RTS"    , "ADC"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "ADC"    , "ROR"    , "UNKNOWN", "PLA"    , "ADC"    , "ROR"    , "UNKNOWN", "JMP"    , "ADC"    , "ROR"    , "UNKNOWN", // 6
    "BVS"    , "ADC"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "ADC"    , "ROR"    , "UNKNOWN", "SEI"    , "ADC"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "ADC"    , "ROR"    , "UNKNOWN", // 7
    "UNKNOWN", "STA"    , "UNKNOWN", "UNKNOWN", "STY"    , "STA"    , "STX"    , "UNKNOWN", "DEY"    , "UNKNOWN", "TXA"    , "UNKNOWN", "STY"    , "STA"    , "STX"    , "UNKNOWN", // 8
    "BCC"    , "STA"    , "UNKNOWN", "UNKNOWN", "STY"    , "STA"    , "STX"    , "UNKNOWN", "TYA"    , "STA"    , "TXS"    , "UNKNOWN", "UNKNOWN", "STA"    , "UNKNOWN", "UNKNOWN", // 9
    "LDY"    , "LDA"    , "LDX"    , "UNKNOWN", "LDY"    , "LDA"    , "LDX"    , "UNKNOWN", "TAY"    , "LDA"    , "TAX"    , "UNKNOWN", "LDY"    , "LDA"    , "LDX"    , "UNKNOWN", // A
    "BCS"    , "LDA"    , "UNKNOWN", "UNKNOWN", "LDY"    , "LDA"    , "LDX"    , "UNKNOWN", "CLV"    , "LDA"    , "TSX"    , "UNKNOWN", "LDY"    , "LDA"    , "LDX"    , "UNKNOWN", // B
    "CPY"    , "CMP"    , "UNKNOWN", "UNKNOWN", "CPY"    , "CMP"    , "DEC"    , "UNKNOWN", "INY"    , "CMP"    , "DEX"    , "UNKNOWN", "CPY"    , "CMP"    , "DEC"    , "UNKNOWN", // C
    "BNE"    , "CMP"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "CMP"    , "DEC"    , "UNKNOWN", "CLD"    , "CMP"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "CMP"    , "DEC"    , "UNKNOWN", // D
    "CPX"    , "SBC"    , "UNKNOWN", "UNKNOWN", "CPX"    , "SBC"    , "INC"    , "UNKNOWN", "INX"    , "SBC"    , "UNKNOWN", "UNKNOWN", "CPX"    , "SBC"    , "INC"    , "UNKNOWN", // E
    "BEQ"    , "SBC"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "SBC"    , "INC"    , "UNKNOWN", "SED"    , "SBC"    , "UNKNOWN", "UNKNOWN", "UNKNOWN", "SBC"    , "INC"    , "UNKNOWN"  // F
};

static int const OPCODE_SIZES[] = {
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    1, 2, 0, 0, 0, 2, 2, 0, 1, 2, 1, 0, 0, 3, 3, 0, // 0
    2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0, // 1
    3, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0, // 2
    2, 2, 0, 0, 0, 2, 3, 0, 1, 3, 0, 0, 0, 3, 3, 0, // 3
    1, 2, 0, 0, 0, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0, // 4
    2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0, // 5
    1, 2, 0, 0, 0, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0, // 6
    2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0, // 7
    0, 2, 0, 0, 2, 2, 2, 0, 1, 0, 1, 0, 3, 3, 3, 0, // 8
    2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 0, 3, 0, 0, // 9
    2, 2, 2, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0, // A
    2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0, // B
    2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0, // C
    2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0, // D
    2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 0, 0, 3, 3, 3, 0, // E
    2, 2, 0, 0, 0, 2, 2, 0, 1, 3, 0, 0, 0, 3, 3, 0  // F
};

static ADDRESSING_MODE const OPCODE_MODES[] = {
//        0              1              2              3              4              5              6              7              8              9              A              B              C              D              E              F
    AM_IMPLIED   , AM_INDIRECT_X, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE  , AM_ZEROPAGE  , UNIMPLEMENTED, AM_IMPLIED   , AM_IMMEDIATE , AM_ACCUM     , UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE  , AM_ABSOLUTE  , UNIMPLEMENTED, // 0
    AM_RELATIVE  , AM_INDIRECT_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, AM_ZEROPAGE_X, UNIMPLEMENTED, AM_IMPLIED   , AM_ABSOLUTE_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_X, AM_ABSOLUTE_X, UNIMPLEMENTED, // 1
    AM_ABSOLUTE  , AM_INDIRECT_X, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE  , AM_ZEROPAGE  , AM_ZEROPAGE  , UNIMPLEMENTED, AM_IMPLIED   , AM_IMMEDIATE , AM_ACCUM     , UNIMPLEMENTED, AM_ABSOLUTE  , AM_ABSOLUTE  , AM_ABSOLUTE  , UNIMPLEMENTED, // 2
    AM_RELATIVE  , AM_INDIRECT_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, AM_ZEROPAGE_X, UNIMPLEMENTED, AM_IMPLIED   , AM_ABSOLUTE_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_X, AM_ABSOLUTE_X, UNIMPLEMENTED, // 3
    AM_IMPLIED   , AM_INDIRECT_X, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE  , AM_ZEROPAGE  , UNIMPLEMENTED, AM_IMPLIED   , AM_IMMEDIATE , AM_ACCUM     , UNIMPLEMENTED, AM_ABSOLUTE  , AM_ABSOLUTE  , AM_ABSOLUTE  , UNIMPLEMENTED, // 4
    AM_RELATIVE  , AM_INDIRECT_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, AM_ZEROPAGE_X, UNIMPLEMENTED, AM_IMPLIED   , AM_ABSOLUTE_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_X, AM_ABSOLUTE_X, UNIMPLEMENTED, // 5
    AM_IMPLIED   , AM_INDIRECT_X, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE  , AM_ZEROPAGE  , UNIMPLEMENTED, AM_IMPLIED   , AM_IMMEDIATE , AM_ACCUM     , UNIMPLEMENTED, AM_INDIRECT  , AM_ABSOLUTE  , AM_ABSOLUTE  , UNIMPLEMENTED, // 6
    AM_RELATIVE  , AM_INDIRECT_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, AM_ZEROPAGE_X, UNIMPLEMENTED, AM_IMPLIED   , AM_ABSOLUTE_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_X, AM_ABSOLUTE_X, UNIMPLEMENTED, // 7
    UNIMPLEMENTED, AM_INDIRECT_X, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE  , AM_ZEROPAGE  , AM_ZEROPAGE  , UNIMPLEMENTED, AM_IMPLIED   , UNIMPLEMENTED, AM_IMPLIED   , UNIMPLEMENTED, AM_ABSOLUTE  , AM_ABSOLUTE  , AM_ABSOLUTE  , UNIMPLEMENTED, // 8
    AM_RELATIVE  , AM_INDIRECT_Y, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, AM_ZEROPAGE_X, AM_ZEROPAGE_Y, UNIMPLEMENTED, AM_IMPLIED   , AM_ABSOLUTE_Y, AM_IMPLIED   , UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_X, UNIMPLEMENTED, UNIMPLEMENTED, // 9
    AM_IMMEDIATE , AM_INDIRECT_X, AM_IMMEDIATE , UNIMPLEMENTED, AM_ZEROPAGE  , AM_ZEROPAGE  , AM_ZEROPAGE  , UNIMPLEMENTED, AM_IMPLIED   , AM_IMMEDIATE , AM_IMPLIED   , UNIMPLEMENTED, AM_ABSOLUTE  , AM_ABSOLUTE  , AM_ABSOLUTE  , UNIMPLEMENTED, // A
    AM_RELATIVE  , AM_INDIRECT_Y, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, AM_ZEROPAGE_X, AM_ZEROPAGE_Y, UNIMPLEMENTED, AM_IMPLIED   , AM_ABSOLUTE_Y, AM_IMPLIED   , UNIMPLEMENTED, AM_ABSOLUTE_X, AM_ABSOLUTE_X, AM_ABSOLUTE_Y, UNIMPLEMENTED, // B
    AM_IMMEDIATE , AM_INDIRECT_X, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE  , AM_ZEROPAGE  , AM_ZEROPAGE  , UNIMPLEMENTED, AM_IMPLIED   , AM_IMMEDIATE , AM_IMPLIED   , UNIMPLEMENTED, AM_ABSOLUTE  , AM_ABSOLUTE  , AM_ABSOLUTE  , UNIMPLEMENTED, // C
    AM_RELATIVE  , AM_INDIRECT_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, AM_ZEROPAGE_X, UNIMPLEMENTED, AM_IMPLIED   , AM_ABSOLUTE_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_X, AM_ABSOLUTE_X, UNIMPLEMENTED, // D
    AM_IMMEDIATE , AM_INDIRECT_X, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE  , AM_ZEROPAGE  , AM_ZEROPAGE  , UNIMPLEMENTED, AM_IMPLIED   , AM_IMMEDIATE , UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE  , AM_ABSOLUTE  , AM_ABSOLUTE  , UNIMPLEMENTED, // E
    AM_RELATIVE  , AM_INDIRECT_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ZEROPAGE_X, AM_ZEROPAGE_X, UNIMPLEMENTED, AM_IMPLIED   , AM_ABSOLUTE_Y, UNIMPLEMENTED, UNIMPLEMENTED, UNIMPLEMENTED, AM_ABSOLUTE_X, AM_ABSOLUTE_X, UNIMPLEMENTED  // F
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
    case AM_ACCUM:
        ss << "A" << endl;
        break;

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
