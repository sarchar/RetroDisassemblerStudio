#include <iomanip>
#include <iostream>

#include "magic_enum.hpp"
#include "util.h"

#include "systems/nes/nes_cpu.h"

using namespace std;

namespace NES {

static CPU::CPU_OP CpuReset[] = {
    CPU::CPU_OP::NOP, CPU::CPU_OP::NOP, CPU::CPU_OP::NOP, CPU::CPU_OP::NOP, CPU::CPU_OP::NOP, CPU::CPU_OP::NOP, CPU::CPU_OP::NOP, // 7 cycles for reset
    CPU::CPU_OP::RESET_LO,
    CPU::CPU_OP::RESET_HI,
    CPU::CPU_OP::IFETCH
};

static CPU::CPU_OP CpuOpNop[] = { CPU::CPU_OP::IFETCH };

static CPU::CPU_OP const* OpTable[256] = {
    /* 0x00 */ CpuOpNop, /* 0x01 */ CpuOpNop, /* 0x02 */ CpuOpNop, /* 0x03 */ CpuOpNop,
    /* 0x04 */ CpuOpNop, /* 0x05 */ CpuOpNop, /* 0x06 */ CpuOpNop, /* 0x07 */ CpuOpNop,
    /* 0x08 */ CpuOpNop, /* 0x09 */ CpuOpNop, /* 0x0A */ CpuOpNop, /* 0x0B */ CpuOpNop,
    /* 0x0C */ CpuOpNop, /* 0x0D */ CpuOpNop, /* 0x0E */ CpuOpNop, /* 0x0F */ CpuOpNop,
    /* 0x10 */ CpuOpNop, /* 0x11 */ CpuOpNop, /* 0x12 */ CpuOpNop, /* 0x13 */ CpuOpNop,
    /* 0x14 */ CpuOpNop, /* 0x15 */ CpuOpNop, /* 0x16 */ CpuOpNop, /* 0x17 */ CpuOpNop,
    /* 0x18 */ CpuOpNop, /* 0x19 */ CpuOpNop, /* 0x1A */ CpuOpNop, /* 0x1B */ CpuOpNop,
    /* 0x1C */ CpuOpNop, /* 0x1D */ CpuOpNop, /* 0x1E */ CpuOpNop, /* 0x1F */ CpuOpNop,
    /* 0x20 */ CpuOpNop, /* 0x21 */ CpuOpNop, /* 0x22 */ CpuOpNop, /* 0x23 */ CpuOpNop,
    /* 0x24 */ CpuOpNop, /* 0x25 */ CpuOpNop, /* 0x26 */ CpuOpNop, /* 0x27 */ CpuOpNop,
    /* 0x28 */ CpuOpNop, /* 0x29 */ CpuOpNop, /* 0x2A */ CpuOpNop, /* 0x2B */ CpuOpNop,
    /* 0x2C */ CpuOpNop, /* 0x2D */ CpuOpNop, /* 0x2E */ CpuOpNop, /* 0x2F */ CpuOpNop,
    /* 0x30 */ CpuOpNop, /* 0x31 */ CpuOpNop, /* 0x32 */ CpuOpNop, /* 0x33 */ CpuOpNop,
    /* 0x34 */ CpuOpNop, /* 0x35 */ CpuOpNop, /* 0x36 */ CpuOpNop, /* 0x37 */ CpuOpNop,
    /* 0x38 */ CpuOpNop, /* 0x39 */ CpuOpNop, /* 0x3A */ CpuOpNop, /* 0x3B */ CpuOpNop,
    /* 0x3C */ CpuOpNop, /* 0x3D */ CpuOpNop, /* 0x3E */ CpuOpNop, /* 0x3F */ CpuOpNop,
    /* 0x40 */ CpuOpNop, /* 0x41 */ CpuOpNop, /* 0x42 */ CpuOpNop, /* 0x43 */ CpuOpNop,
    /* 0x44 */ CpuOpNop, /* 0x45 */ CpuOpNop, /* 0x46 */ CpuOpNop, /* 0x47 */ CpuOpNop,
    /* 0x48 */ CpuOpNop, /* 0x49 */ CpuOpNop, /* 0x4A */ CpuOpNop, /* 0x4B */ CpuOpNop,
    /* 0x4C */ CpuOpNop, /* 0x4D */ CpuOpNop, /* 0x4E */ CpuOpNop, /* 0x4F */ CpuOpNop,
    /* 0x50 */ CpuOpNop, /* 0x51 */ CpuOpNop, /* 0x52 */ CpuOpNop, /* 0x53 */ CpuOpNop,
    /* 0x54 */ CpuOpNop, /* 0x55 */ CpuOpNop, /* 0x56 */ CpuOpNop, /* 0x57 */ CpuOpNop,
    /* 0x58 */ CpuOpNop, /* 0x59 */ CpuOpNop, /* 0x5A */ CpuOpNop, /* 0x5B */ CpuOpNop,
    /* 0x5C */ CpuOpNop, /* 0x5D */ CpuOpNop, /* 0x5E */ CpuOpNop, /* 0x5F */ CpuOpNop,
    /* 0x60 */ CpuOpNop, /* 0x61 */ CpuOpNop, /* 0x62 */ CpuOpNop, /* 0x63 */ CpuOpNop,
    /* 0x64 */ CpuOpNop, /* 0x65 */ CpuOpNop, /* 0x66 */ CpuOpNop, /* 0x67 */ CpuOpNop,
    /* 0x68 */ CpuOpNop, /* 0x69 */ CpuOpNop, /* 0x6A */ CpuOpNop, /* 0x6B */ CpuOpNop,
    /* 0x6C */ CpuOpNop, /* 0x6D */ CpuOpNop, /* 0x6E */ CpuOpNop, /* 0x6F */ CpuOpNop,
    /* 0x70 */ CpuOpNop, /* 0x71 */ CpuOpNop, /* 0x72 */ CpuOpNop, /* 0x73 */ CpuOpNop,
    /* 0x74 */ CpuOpNop, /* 0x75 */ CpuOpNop, /* 0x76 */ CpuOpNop, /* 0x77 */ CpuOpNop,
    /* 0x78 */ CpuOpNop, /* 0x79 */ CpuOpNop, /* 0x7A */ CpuOpNop, /* 0x7B */ CpuOpNop,
    /* 0x7C */ CpuOpNop, /* 0x7D */ CpuOpNop, /* 0x7E */ CpuOpNop, /* 0x7F */ CpuOpNop,
    /* 0x80 */ CpuOpNop, /* 0x81 */ CpuOpNop, /* 0x82 */ CpuOpNop, /* 0x83 */ CpuOpNop,
    /* 0x84 */ CpuOpNop, /* 0x85 */ CpuOpNop, /* 0x86 */ CpuOpNop, /* 0x87 */ CpuOpNop,
    /* 0x88 */ CpuOpNop, /* 0x89 */ CpuOpNop, /* 0x8A */ CpuOpNop, /* 0x8B */ CpuOpNop,
    /* 0x8C */ CpuOpNop, /* 0x8D */ CpuOpNop, /* 0x8E */ CpuOpNop, /* 0x8F */ CpuOpNop,
    /* 0x90 */ CpuOpNop, /* 0x91 */ CpuOpNop, /* 0x92 */ CpuOpNop, /* 0x93 */ CpuOpNop,
    /* 0x94 */ CpuOpNop, /* 0x95 */ CpuOpNop, /* 0x96 */ CpuOpNop, /* 0x97 */ CpuOpNop,
    /* 0x98 */ CpuOpNop, /* 0x99 */ CpuOpNop, /* 0x9A */ CpuOpNop, /* 0x9B */ CpuOpNop,
    /* 0x9C */ CpuOpNop, /* 0x9D */ CpuOpNop, /* 0x9E */ CpuOpNop, /* 0x9F */ CpuOpNop,
    /* 0xA0 */ CpuOpNop, /* 0xA1 */ CpuOpNop, /* 0xA2 */ CpuOpNop, /* 0xA3 */ CpuOpNop,
    /* 0xA4 */ CpuOpNop, /* 0xA5 */ CpuOpNop, /* 0xA6 */ CpuOpNop, /* 0xA7 */ CpuOpNop,
    /* 0xA8 */ CpuOpNop, /* 0xA9 */ CpuOpNop, /* 0xAA */ CpuOpNop, /* 0xAB */ CpuOpNop,
    /* 0xAC */ CpuOpNop, /* 0xAD */ CpuOpNop, /* 0xAE */ CpuOpNop, /* 0xAF */ CpuOpNop,
    /* 0xB0 */ CpuOpNop, /* 0xB1 */ CpuOpNop, /* 0xB2 */ CpuOpNop, /* 0xB3 */ CpuOpNop,
    /* 0xB4 */ CpuOpNop, /* 0xB5 */ CpuOpNop, /* 0xB6 */ CpuOpNop, /* 0xB7 */ CpuOpNop,
    /* 0xB8 */ CpuOpNop, /* 0xB9 */ CpuOpNop, /* 0xBA */ CpuOpNop, /* 0xBB */ CpuOpNop,
    /* 0xBC */ CpuOpNop, /* 0xBD */ CpuOpNop, /* 0xBE */ CpuOpNop, /* 0xBF */ CpuOpNop,
    /* 0xC0 */ CpuOpNop, /* 0xC1 */ CpuOpNop, /* 0xC2 */ CpuOpNop, /* 0xC3 */ CpuOpNop,
    /* 0xC4 */ CpuOpNop, /* 0xC5 */ CpuOpNop, /* 0xC6 */ CpuOpNop, /* 0xC7 */ CpuOpNop,
    /* 0xC8 */ CpuOpNop, /* 0xC9 */ CpuOpNop, /* 0xCA */ CpuOpNop, /* 0xCB */ CpuOpNop,
    /* 0xCC */ CpuOpNop, /* 0xCD */ CpuOpNop, /* 0xCE */ CpuOpNop, /* 0xCF */ CpuOpNop,
    /* 0xD0 */ CpuOpNop, /* 0xD1 */ CpuOpNop, /* 0xD2 */ CpuOpNop, /* 0xD3 */ CpuOpNop,
    /* 0xD4 */ CpuOpNop, /* 0xD5 */ CpuOpNop, /* 0xD6 */ CpuOpNop, /* 0xD7 */ CpuOpNop,
    /* 0xD8 */ CpuOpNop, /* 0xD9 */ CpuOpNop, /* 0xDA */ CpuOpNop, /* 0xDB */ CpuOpNop,
    /* 0xDC */ CpuOpNop, /* 0xDD */ CpuOpNop, /* 0xDE */ CpuOpNop, /* 0xDF */ CpuOpNop,
    /* 0xE0 */ CpuOpNop, /* 0xE1 */ CpuOpNop, /* 0xE2 */ CpuOpNop, /* 0xE3 */ CpuOpNop,
    /* 0xE4 */ CpuOpNop, /* 0xE5 */ CpuOpNop, /* 0xE6 */ CpuOpNop, /* 0xE7 */ CpuOpNop,
    /* 0xE8 */ CpuOpNop, /* 0xE9 */ CpuOpNop, /* 0xEA */ CpuOpNop, /* 0xEB */ CpuOpNop,
    /* 0xEC */ CpuOpNop, /* 0xED */ CpuOpNop, /* 0xEE */ CpuOpNop, /* 0xEF */ CpuOpNop,
    /* 0xF0 */ CpuOpNop, /* 0xF1 */ CpuOpNop, /* 0xF2 */ CpuOpNop, /* 0xF3 */ CpuOpNop,
    /* 0xF4 */ CpuOpNop, /* 0xF5 */ CpuOpNop, /* 0xF6 */ CpuOpNop, /* 0xF7 */ CpuOpNop,
    /* 0xF8 */ CpuOpNop, /* 0xF9 */ CpuOpNop, /* 0xFA */ CpuOpNop, /* 0xFB */ CpuOpNop,
    /* 0xFC */ CpuOpNop, /* 0xFD */ CpuOpNop, /* 0xFE */ CpuOpNop, /* 0xFF */ CpuOpNop
};

CPU::CPU(read_func_t const& read_func, write_func_t const& write_func)
    : Read(read_func), Write(write_func)
{
    Reset();
}

CPU::~CPU()
{
}

void CPU::Reset()
{
    state.ops = CpuReset;
    state.istep = 0;
    cycle_count = 0;
}

void CPU::Step()
{
    cout << "PC = $" << hex << setw(4) << setfill('0') << regs.PC << " State: " << magic_enum::enum_name(*state.ops) << endl;

    switch(*state.ops++) {
    case CPU_OP::NOP:
        break;

    case CPU_OP::RESET_LO:
        regs.PC = (regs.PC & 0xFF00) | (u16)Read(0xFFFC);
        break;

    case CPU_OP::RESET_HI:
        regs.PC = (regs.PC & 0x00FF) | ((u16)Read(0xFFFD) << 8);
        break;

    case CPU_OP::IFETCH:
        state.opcode = Read(regs.PC++);
        state.ops = OpTable[state.opcode];
        state.istep = 0;
        break;

    default:
        assert(false);
        break;
    }

    state.istep++;
    cycle_count++;
}


}
