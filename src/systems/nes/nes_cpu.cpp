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

bool CPU::Step()
{
    bool ret = false;

    if(state.ops == nullptr) {
        cout << "[CPU::Step] invalid opcode $" << hex << setw(2) << setfill('0') << (int)state.opcode 
             << " after " << dec << cycle_count << " cycles" << endl;
        return false;
    }

    //cout << "PC = $" << hex << setw(4) << setfill('0') << regs.PC << " State: " << *state.ops << endl;
    auto op = *state.ops++;

    // set up address line. regs.PC should always be mux item 0
    u16 address_mux[] = { regs.PC, state.eaddr, (u16)state.intermediate, (u16)0x100 + (u16)regs.S };
    u16 address = address_mux[(op & CPU_ADDRESS_mask) >> CPU_ADDRESS_shift];

    // set up read/write
    bool write = ((op & CPU_RW_mask) == CPU_WRITE);
    u8 data;

    if(write) {
        // set up data line
        u8 data_mux[] = { regs.A, regs.X, regs.Y, regs.P, state.intermediate, 
            (u8)regs.PC, (u8)(regs.PC >> 8), 0 };
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

    // check dec and inc stack
    if((op & CPU_INCS_mask) == CPU_INCS) regs.S += 1;
    if((op & CPU_DECS_mask) == CPU_DECS) regs.S -= 1;

    // setup the ALU
    auto alu_op = (op & CPU_ALU_OP_mask);
    u8 alu_out = 0xEE;
    u8 alu_a, alu_b, alu_c;

    // perform the ALU op
    if(alu_op != CPU_ALU_OP_IDLE) { // optimization
        u8 alu_a_mux[] = { regs.A, regs.X, regs.Y, regs.S, 
            (u8)(regs.PC & 0x00FF), (u8)(regs.PC >> 8), (u8)(state.eaddr >> 8), regs.P,
            state.intermediate };
        alu_a = alu_a_mux[(op & CPU_ALU_A_mask) >> CPU_ALU_A_shift];

        u8 alu_b_mux[] = { 0, (u8)(state.eaddr & 0x00FF), state.intermediate, data, 0, 0, 0, 0, 
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
        case CPU_ALU_OP_SBC:
        {
            u16 tmp = (u16)alu_a - (u16)alu_b - (u16)(alu_c ? 0 : 1);
            alu_out = (u8)tmp;
            alu_c = tmp > 0xFF ? 0 : 1;
            break;
        }
        case CPU_ALU_OP_AND:
            alu_out = alu_a & alu_b;
            break;

        case CPU_ALU_OP_OR:
            alu_out = alu_a | alu_b;
            break;

        case CPU_ALU_OP_EOR:
            alu_out = alu_a ^ alu_b;
            break;

        case CPU_ALU_OP_CLRBIT:
            alu_out = alu_a & ~alu_b;
            break;

        case CPU_ALU_OP_ASL:
            alu_c = alu_a >> 7;
            alu_out = alu_a << 1;
            break;

        case CPU_ALU_OP_LSR:
            alu_c = alu_a & 0x01;
            alu_out = alu_a >> 1;
            break;

        case CPU_ALU_OP_ROL:
            alu_out = (alu_a << 1) | alu_c;
            alu_c = alu_a >> 7;
            break;

        case CPU_ALU_OP_ROR:
            alu_out = (alu_a >> 1) | (alu_c << 7);
            alu_c = alu_a & 0x01;
            break;

        default:
            assert(false);
            return false;
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
        //cout << "opcode: " << hex << uppercase << setw(2) << setfill('0') << (int)ibus << endl;
        state.opcode  = (u8)ibus;
        state.ops     = OpTable[state.opcode];
        state.istep   = 0;
        state.inst_pc = regs.PC;
        ret = true;
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

    state.istep++;
    cycle_count++;

	return ret;
}


}
