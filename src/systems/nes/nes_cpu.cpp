#include <iomanip>
#include <iostream>

#include "magic_enum.hpp"
#include "util.h"

#include "systems/nes/nes_cpu.h"

using namespace std;

namespace NES {

#include "systems/nes/nes_cpu_tables.h"

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
    //cout << "PC = $" << hex << setw(4) << setfill('0') << regs.PC << " State: " << *state.ops << endl;

    auto op = *state.ops++;
    u8 tmp;

    switch(CPU_OP_GET_SOURCE(op)) {
    case CPU_OP_ASSERT:
        cout << "unhandled opcode $" << hex << (int)state.opcode << endl;
        assert(false);
        break;

    case CPU_OP_NOP:
        break;
        
    case CPU_OP_READV:
        tmp = Read(CPU_OP_GET_VECTOR(op));
        break;

    case CPU_OP_READMEM:
        tmp = Read(state.addr++); // TODO does this wrap at word boundary?
        break;

    case CPU_OP_READMEM2:
        tmp = Read(state.addr2++); // TODO does this wrap at word boundary?
        break;

    case CPU_OP_READA:
        tmp = regs.A;
        break;

    case CPU_OP_READX:
        tmp = regs.X;
        break;

    case CPU_OP_READY:
        tmp = regs.Y;
        break;

    case CPU_OP_READS:
        tmp = regs.S;
        break;

    case CPU_OP_READP:
        tmp = regs.P;
        break;

    case CPU_OP_READI:
        tmp = state.intermediate;
        break;

    case CPU_OP_IFETCH:
        tmp = Read(regs.PC++);
        break;

    case CPU_OP_CLEARF:
        tmp = (regs.P & ~(u8)CPU_OP_GET_FLAG(op));
        break;

    case CPU_OP_SETF:
        tmp = (regs.P | CPU_OP_GET_FLAG(op));
        break;

    case CPU_OP_INDEXED_X:
        state.carry_addr = (state.addr + (u16)regs.X) >= 0x100;
        tmp = (u8)(state.addr + (u16)regs.X);
        break;

    case CPU_OP_INDEXED_Y:
        state.carry_addr = (state.addr + (u16)regs.Y) >= 0x100;
        tmp = (u8)(state.addr + (u16)regs.Y);
        break;

    case CPU_OP_CARRYADDR:
        // extra cycle when crossing page boundaries, but if there was no carry then we need to perform a normal operation
        if(state.carry_addr) {
            tmp = (u8)(state.addr >> 8) + 1;
        } else {
            // skip the current instruction and do the next
            Step();
            return;
        }

    case CPU_OP_STACKR:
        tmp = (u8)Read((u16)regs.S + 0x100);
        break;
    }

    switch(CPU_OP_GET_ALU(op)) {
    case CPU_OP_ALU_NOP:
        break;

    case CPU_OP_ALU_INC:
        tmp = tmp + 1;
        break;

    case CPU_OP_ALU_DEC:
        tmp = tmp - 1;
        break;

    case CPU_OP_ALU_ADC:
        tmp = regs.A + tmp + (regs.P & CPU_FLAG_C) ? 1 : 0;
        break;

    case CPU_OP_ALU_SBC:
        tmp = regs.A - tmp - (regs.P & CPU_FLAG_C) ? 0 : 1;
        break;

    case CPU_OP_ALU_AND:
        tmp = regs.A & tmp;
        break;

    case CPU_OP_ALU_EOR:
        tmp = regs.A ^ tmp;
        break;

    case CPU_OP_ALU_ORA:
        tmp = regs.A | tmp;
        break;

    case CPU_OP_ALU_ASL:
        regs.P = (regs.P & ~CPU_FLAG_C) | ((tmp & 0x80) ? CPU_FLAG_C : 0);
        tmp = tmp << 1;
        break;

    case CPU_OP_ALU_LSR:
        regs.P = (regs.P & ~CPU_FLAG_C) | ((tmp & 0x01) ? CPU_FLAG_C : 0);
        tmp = tmp >> 1;
        break;

    case CPU_OP_ALU_ROL:
    {
        u8 oldp = regs.P;
        regs.P = (regs.P & ~CPU_FLAG_C) | ((tmp & 0x80) ? CPU_FLAG_C : 0);
        tmp = (tmp << 1) | ((oldp & CPU_FLAG_C) ? 1 : 0);
        break;
    }

    case CPU_OP_ALU_ROR:
    {
        u8 oldp = regs.P;
        regs.P = (regs.P & ~CPU_FLAG_C) | ((tmp & 0x01) ? CPU_FLAG_C : 0);
        tmp = (tmp >> 1) | ((oldp & CPU_FLAG_C) ? 0x80 : 0);
        break;
    }

    case CPU_OP_ALU_CMP:
        tmp = regs.A - tmp;
        break;

    case CPU_OP_ALU_CPX:
        tmp = regs.X - tmp;
        break;

    case CPU_OP_ALU_CPY:
        tmp = regs.Y - tmp;
        break;
    }

    switch(CPU_OP_GET_WRITE(op)) {
    case CPU_OP_WRITE_NOP:
        break;

    case CPU_OP_WRITEPC_LO:
        regs.PC = (regs.PC & 0xFF00) | (u16)tmp;
        break;

    case CPU_OP_WRITEPC_HI:
        regs.PC = (regs.PC & 0x00FF) | ((u16)tmp << 8);
        break;

    case CPU_OP_WRITEADDR: // this state clears the high byte for zero page
        state.addr = (u16)tmp;
        break;

    case CPU_OP_WRITEADDR_LO:
        state.addr = (state.addr & 0xFF00) | (u16)tmp;
        break;

    case CPU_OP_WRITEADDR_HI:
        state.addr = (state.addr & 0x00FF) | ((u16)tmp << 8);
        break;

    case CPU_OP_WRITEADDR2: // this state clears the high byte for zero page
        state.addr2 = (u16)tmp;
        break;

    case CPU_OP_WRITEADDR2_LO:
        state.addr2 = (state.addr2 & 0xFF00) | (u16)tmp;
        break;

    case CPU_OP_WRITEADDR2_HI:
        state.addr2 = (state.addr2 & 0x00FF) | ((u16)tmp << 8);
        break;

    case CPU_OP_DECODE:
        state.opcode = (u8)tmp;
        state.ops = OpTable[state.opcode];
        state.istep = 0;
        break;

    case CPU_OP_WRITEP:
        regs.P = tmp;
        break;

    case CPU_OP_WRITEI:
        state.intermediate = tmp;
        break;

    case CPU_OP_WRITEA:
        regs.A = tmp;
        break;

    case CPU_OP_WRITEX:
        regs.X = tmp;
        break;

    case CPU_OP_WRITEY:
        regs.Y = tmp;
        break;

    case CPU_OP_WRITES:
        regs.S = tmp;
        break;

    case CPU_OP_WRITEMEM:
        Write(state.addr, tmp);
        break;

    case CPU_OP_STACKW:
        Write((u16)regs.S + 0x100, tmp);
        break;

    default:
        assert(false);
        break;
    }

    state.istep++;
    cycle_count++;
}


}
