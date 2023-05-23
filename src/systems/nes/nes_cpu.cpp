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
    regs.PC = 0xFFFC;
    cycle_count = 0;
}

void CPU::Step()
{
    if(state.ops == nullptr) {
        cout << "[CPU::Step] invalid opcode $" << hex << setw(2) << setfill('0') << (int)state.opcode << endl;
        assert(false);
    }

    //cout << "PC = $" << hex << setw(4) << setfill('0') << regs.PC << " State: " << *state.ops << endl;
    auto op = *state.ops++;

    // set up address line. regs.PC should always be mux item 0
    u16 address_mux[] = { regs.PC, state.eaddr, (u16)state.intermediate };
    u16 address = address_mux[(op & CPU_ADDRESS_mask) >> CPU_ADDRESS_shift];

    // set up read/write
    bool write = ((op & CPU_RW_mask) == CPU_WRITE);
    u8 data;

    if(write) {
        // set up data line
        u8 data_mux[] = { regs.A, regs.X, regs.Y, 0, 0, 0, 0, 0 };
        data = data_mux[(op & CPU_DATA_BUS_mask) >> CPU_DATA_BUS_shift];
        Write(address, data);
    } else {
        // put Read on the wire
        data = Read(address);
    }

    // check inc PC
    if((op & CPU_INCPC_mask) == CPU_INCPC) regs.PC += 1;

    // check inc INTM
    if((op & CPU_INCINTM_mask) == CPU_INCINTM) state.intermediate += 1;

    // setup the ALU
    auto alu_op = (op & CPU_ALU_OP_mask);
    u8 alu_out = 0xEE;
    u8 alu_a, alu_b, alu_c;

    if(alu_op != CPU_ALU_OP_IDLE) { // optimization
        u8 alu_a_mux[] = { regs.A, regs.X, regs.Y, regs.S, 
            (u8)(regs.PC & 0x00FF), (u8)(regs.PC >> 8), (u8)(state.eaddr >> 8), regs.P };
        alu_a = alu_a_mux[(op & CPU_ALU_A_mask) >> CPU_ALU_A_shift];

        u8 alu_b_mux[] = { 0, (u8)(state.eaddr & 0x00FF), state.intermediate, 0, 0, 0, 0, 0, 
            CPU_FLAG_C, CPU_FLAG_D, CPU_FLAG_I, CPU_FLAG_V, CPU_FLAG_Z, CPU_FLAG_N, 0, 0 };
        alu_b = alu_b_mux[(op & CPU_ALU_B_mask) >> CPU_ALU_B_shift];

        u8 alu_c_mux[] = { (u8)((regs.P & CPU_FLAG_C) ? 1 : 0), 0, 1 };
        alu_c = alu_c_mux[(op & CPU_ALU_C_mask) >> CPU_ALU_C_shift];

        switch(alu_op) {
        case CPU_ALU_OP_ADC:
        {
            u16 tmp = (u16)alu_a + (u16)alu_b + (u16)alu_c;
            alu_out = (u8)tmp;
            alu_c = tmp > 0xFF ? 1 : 0;
            break;
        }
        case CPU_ALU_OP_AND:
            alu_out = alu_a & alu_b;
            break;

        case CPU_ALU_OP_OR:
            alu_out = alu_a | alu_b;
            break;

        case CPU_ALU_OP_CLRBIT:
            alu_out = alu_a & ~alu_b;
            break;

        default:
            assert(false);
            return;
        }
    }

    if((op & CPU_CHECK_BRANCH_SET) == CPU_CHECK_BRANCH_SET) {
        // branch failed, skip two decode steps
        if(!alu_out) state.ops += 2;
    }

    if((op & CPU_CHECK_BRANCH_CLEAR) == CPU_CHECK_BRANCH_CLEAR) {
        // branch failed, skip two decode steps
        if(alu_out) state.ops += 2;
    }

    // select data into the internal bus
    u8 ibus_mux[] = { data, alu_out };
    u8 ibus = ibus_mux[(op & CPU_IBUS_mask) >> CPU_IBUS_shift];

    // check latch opcode
    if((op & CPU_LATCH_OPCODE_mask) == CPU_LATCH_OPCODE) {
        state.opcode = (u8)ibus;
        state.ops    = OpTable[state.opcode];
        state.istep  = 0;
    }

    // check latch PC JMP
    if((op & CPU_LATCH_PC_JMP_mask) == CPU_LATCH_PC_JMP) {
        // PC JMP takes the high byte from the data bus plus the low byte from EADDR
        regs.PC = (state.eaddr & 0x00FF) | ((u16)ibus << 8);
    } 

    // perform a relative branch
    if((op & CPU_LATCH_PC_BRANCH) == CPU_LATCH_PC_BRANCH) {
        // low byte of PC already added on ibus
        regs.PC = (regs.PC & 0xFF00) | (u16)ibus;

        // we need to adjust the high byte based on whether the input was signed
        // (operand is in alu_b)
        // !(alu_b & 0x80) && !alu_c => added and landed in the same bank
        //  (alu_b & 0x80) &&  alu_c => subtracted and landed in the same bank
        // !(alu_b & 0x80) &&  alu_c => added and landed in the next bank
        //  (alu_b & 0x80) && !alu_c => subtracted and landed in the previous bank
        if(alu_c != (alu_b >> 7)) { // overflowed bank and need to adjust
            // alu_c set means add 1 to PCH
            // clear means sub 1 from PCH
            state.eaddr = alu_c ? 1 : 0xFF;
        } else {
            // no need to fix PCH, skip next add
            state.ops++;
        }
    }

    // check if PC HI latch
    if((op & CPU_LATCH_PC_HI) == CPU_LATCH_PC_HI) {
        regs.PC = (regs.PC & 0x00FF) | ((u16)ibus << 8);
    }

    // check EADDR latch
    if((op & CPU_LATCH_EADDR_mask) == CPU_LATCH_EADDR) {
        state.eaddr = (u16)ibus;
    }

    // check EADDR_LO latch
    if((op & CPU_LATCH_EADDR_LO_mask) == CPU_LATCH_EADDR_LO) {
        state.eaddr = (state.eaddr & 0xFF00) | (u16)ibus;
    }

    // check EADDR_HI latch
    if((op & CPU_LATCH_EADDR_HI_mask) == CPU_LATCH_EADDR_HI) {
        state.eaddr = (state.eaddr & 0x00FF) | ((u16)ibus << 8);
    }

    // check EADDR_HI_EXTC latch. bypass IBUS (take data directly)
    // and skip the next instruction if there's no ALU carry
    if((op & CPU_LATCH_EADDR_HI_EXTC_mask) == CPU_LATCH_EADDR_HI_EXT) {
        state.eaddr = (state.eaddr & 0x00FF) | ((u16)data << 8);
        state.intermediate = alu_c;
        if(!alu_c) state.ops++;
    }

    // check EADDR_HI_EXT latch. same as the EADDR_HI_EXTC but always
    // executes the high byte add following this instruction
    if((op & CPU_LATCH_EADDR_HI_EXT_mask) == CPU_LATCH_EADDR_HI_EXT) {
        state.eaddr = (state.eaddr & 0x00FF) | ((u16)data << 8);
        state.intermediate = alu_c;
    }

    // check REGP latch
    if((op & CPU_LATCH_REGP_mask) == CPU_LATCH_REGP) {
        regs.P = ibus;
    }

    // check REGA latch
    if((op & CPU_LATCH_REGA_mask) == CPU_LATCH_REGA) {
        regs.A = ibus;
    }

    // check REGX latch
    if((op & CPU_LATCH_REGX_mask) == CPU_LATCH_REGX) {
        regs.X = ibus;
    }

    // check REGY latch
    if((op & CPU_LATCH_REGY_mask) == CPU_LATCH_REGY) {
        regs.Y = ibus;
    }

    // check REGS latch
    if((op & CPU_LATCH_REGS_mask) == CPU_LATCH_REGS) {
        regs.S = ibus;
    }

    // check INTM latch
    if((op & CPU_LATCH_INTM_mask) == CPU_LATCH_INTM) {
        state.intermediate = ibus;
    }

/*
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

    case CPU_OP_ALU_RELPC:
    {
        // unsigned 8 to signed 8, sign extended to 16, back to unsigned 16 add
        u16 v = regs.PC + ((u16)(s16)(s8)tmp);
        state.intermediate = ((regs.PC & 0xFF00) != (tmp & 0xFF00));
        break;
    }

    case CPU_OP_ALU_INDEXED_X:
    {
        u16 x = (u16)state.addr + (u16)regs.X;
        state.intermediate = (u8)((x & 0xFF00) >> 8);
        state.carry_addr   = (bool)state.intermediate;
        state.addr = (state.addr & 0xFF00) | (u8)(x & 0x00FF);
        break;
    }

    case CPU_OP_ALU_INDEXED_Y:
    {
        u16 y = (u16)state.addr + (u16)regs.Y;
        state.intermediate = (u8)((y & 0xFF00) >> 8);
        state.carry_addr   = (bool)state.intermediate;
        state.addr = (state.addr & 0xFF00) | (u8)(y & 0x00FF);
        break;
    }

    case CPU_OP_ALU_CARRYADDR:
        // extra cycle when crossing page boundaries, but if there was no carry then we need to perform a normal operation
        if(state.carry_addr) {
            tmp = (u8)(state.addr >> 8) + state.intermediate;
        } else {
            // skip the current instruction and do the next
            Step();
            return;
        }
        break;

    case CPU_OP_INCADDR2:
        // increment only the low byte of addr2
        state.addr2 = (state.addr2 & 0xFF00) | (u16)((u8)(state.addr2 & 0xFF) + 1);
        break;
    }

    switch(CPU_OP_GET_WRITE(op)) {
    case CPU_OP_LATCH_NOP:
        break;

    case CPU_OP_LATCHPC_LO:
        regs.PC = (regs.PC & 0xFF00) | (u16)tmp;
        break;

    case CPU_OP_LATCHPC_HI:
        regs.PC = (regs.PC & 0x00FF) | ((u16)tmp << 8);
        break;

    case CPU_OP_LATCHADDR: // this state clears the high byte for zero page
        state.addr = (u16)tmp;
        break;

    case CPU_OP_LATCHADDR_LO:
        state.addr = (state.addr & 0xFF00) | (u16)tmp;
        break;

    case CPU_OP_LATCHADDR_HI:
        state.addr = (state.addr & 0x00FF) | ((u16)tmp << 8);
        break;

    case CPU_OP_LATCHADDR2: // this state clears the high byte for zero page
        state.addr2 = (u16)tmp;
        break;

    case CPU_OP_LATCHADDR2_LO:
        state.addr2 = (state.addr2 & 0xFF00) | (u16)tmp;
        break;

    case CPU_OP_LATCHADDR2_HI:
        state.addr2 = (state.addr2 & 0x00FF) | ((u16)tmp << 8);
        break;

    case CPU_OP_DECODE:
        state.opcode = (u8)tmp;
        state.ops = OpTable[state.opcode];
        state.istep = 0;
        break;

    case CPU_OP_LATCHP:
        regs.P = tmp;
        break;

    case CPU_OP_LATCHI:
        state.intermediate = tmp;
        break;

    case CPU_OP_LATCHA:
        regs.A = tmp;
        break;

    case CPU_OP_LATCHX:
        regs.X = tmp;
        break;

    case CPU_OP_LATCHY:
        regs.Y = tmp;
        break;

    case CPU_OP_LATCHS:
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
*/

    state.istep++;
    cycle_count++;
}


}
