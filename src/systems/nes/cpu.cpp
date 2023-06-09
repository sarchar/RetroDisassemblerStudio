// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 

// incredibly useful documentation:
//
// http://www.atarihq.com/danb/files/64doc.txt
// https://www.masswerk.at/6502/6502_instruction_set.html
//
#include <iomanip>
#include <iostream>

#include "magic_enum.hpp"
#include "util.h"

#include "systems/nes/cpu.h"

using namespace std;

namespace Systems::NES {

#include "systems/nes/cpu_tables.h"

CPU::CPU(read_func_t const& read_func, write_func_t const& write_func)
    : Read(read_func), Write(write_func)
{
}

CPU::~CPU()
{
}

void CPU::Reset()
{
    state.nmi = 0;
    state.ops = state.ops_base = CpuReset;
    state.istep = 0;
    regs.P |= 0x20;
    regs.PC = 0xFFFC;
    cycle_count = 0;
#if 0 // Uncomment to run the automation nestest.nes program
    state.ops = &CpuReset[2];
    regs.PC = 0xC000;
#endif
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
    u16 address_mux[] = { regs.PC, state.eaddr, (u16)state.intermediate, (u16)((u16)regs.S + 0x100) };
    u16 address = address_mux[(op & CPU_ADDRESS_mask) >> CPU_ADDRESS_shift];

    // set up read/write
    bool write = ((op & CPU_RW_mask) == CPU_WRITE);
    u8 data;

    if(write) {
        // set up data line
        u8 data_mux[] = { regs.A, regs.X, regs.Y, regs.P, (u8)(regs.P | CPU_FLAG_B),
            state.intermediate, (u8)regs.PC, (u8)(regs.PC >> 8) };
        data = data_mux[(op & CPU_DATA_BUS_mask) >> CPU_DATA_BUS_shift];
        Write(address, data);
    } else {
        // put Read on the wire
        data = Read(address, op == OPCODE_FETCH);
    }

    // check inc PC
    if((op & CPU_INCPC_mask) == CPU_INCPC) regs.PC += 1;

    // check inc EADDR
    if((op & CPU_INCEADDR_mask) == CPU_INCEADDR) state.eaddr += 1;
    else if((op & CPU_INCEADDR_LO_mask) == CPU_INCEADDR_LO) {
        state.eaddr = (state.eaddr & 0xFF00) | (u16)(u8)((state.eaddr & 0xFF) + 1);
    }

    // check inc INTM
    if((op & CPU_INCINTM_mask) == CPU_INCINTM) state.intermediate += 1;

    // check dec and inc stack
    if((op & CPU_INCS_mask) == CPU_INCS) regs.S += 1;
    if((op & CPU_DECS_mask) == CPU_DECS) regs.S -= 1;

    // setup the ALU
    auto alu_op = (op & CPU_ALU_OP_mask);
    u8 alu_out = 0xEE;
    u8 alu_a, alu_b, alu_c, alu_v, bit_v, bit_n;

    // perform the ALU op
    if(alu_op != CPU_ALU_OP_IDLE) { // optimization
        // select A input
        u8 alu_a_mux[] = { regs.A, regs.X, regs.Y, regs.S, 
            (u8)(regs.PC & 0x00FF), (u8)(regs.PC >> 8), (u8)(state.eaddr >> 8), regs.P,
            state.intermediate };
        alu_a = alu_a_mux[(op & CPU_ALU_A_mask) >> CPU_ALU_A_shift];

        // select B input
        u8 alu_b_mux[] = { 0, (u8)(state.eaddr & 0x00FF), state.intermediate, data, 0, 0, 0, 0, 
            CPU_FLAG_C, CPU_FLAG_D, CPU_FLAG_I, CPU_FLAG_V, CPU_FLAG_Z, CPU_FLAG_N, 0, 0 };
        alu_b = alu_b_mux[(op & CPU_ALU_B_mask) >> CPU_ALU_B_shift];

        // configure carry source
        u8 alu_c_mux[] = { (u8)((regs.P & CPU_FLAG_C) ? 1 : 0), 0, 1 };
        alu_c = alu_c_mux[(op & CPU_ALU_C_mask) >> CPU_ALU_C_shift];

        // initialize V to current V
        alu_v = (u8)((regs.P & CPU_FLAG_V) ? 1 : 0);

        switch(alu_op) {
        case CPU_ALU_OP_SBC:
            // SBC inverts the bits of alu_b, and uses carry as the two's complement 
            alu_b ^= 0xFF;
            // fallthru
        case CPU_ALU_OP_ADC:
        {
            u16 tmp = (u16)alu_a + (u16)alu_b + (u16)alu_c;
            alu_out = (u8)tmp;
            alu_c = tmp > 0xFF ? 1 : 0;
            
            // if original inputs had the same sign, and the result does not, signed overflow is detected
            alu_v = (((alu_a & 0x80) ^ (alu_b & 0x80)) == 0 
                     && ((alu_out & 0x80) ^ (alu_a & 0x80)) != 0) ? 1 : 0;
            break;
        }

        // specifically do not touch C and V flags in the AND, EOR, and ORA even for internal
        // operations, as they get latched by arithmetic instructions
        case CPU_ALU_OP_AND:
            alu_out = alu_a & alu_b;

            // bit_v and bit_n only used with BIT via the AND operation
            bit_v = (alu_b & 0x40) >> 6;
            bit_n = (alu_b & 0x80) >> 7;

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

    // N and Z are always updated from the bus
    bool ibus_n = (ibus & 0x80);
    bool ibus_z = ibus == 0;

    // check latch opcode
    if((op & CPU_LATCH_OPCODE_mask) == CPU_LATCH_OPCODE) {
        //cout << "PC: $" << hex << uppercase << setw(4) << setfill('0') << (regs.PC - 1)
        //     << " opcode: " << hex << uppercase << setw(2) << setfill('0') << (int)ibus << endl;
        state.opcode   = (u8)ibus;
        state.ops      = OpTable[state.opcode];
        state.ops_base = state.ops;
        state.istep    = 0;
        state.inst_pc  = regs.PC - 1; // PC is always incremented on the opcode latch cycle
        ret = true;
    }

    // check latch PC JMP
    // PC JMP takes the high byte from the data bus plus the low byte from EADDR or intermediate
    if((op & CPU_LATCH_PC_JMP_mask) == CPU_LATCH_PC_JMP) {
        regs.PC = (state.eaddr & 0x00FF) | ((u16)ibus << 8);
    } else if((op & CPU_LATCH_PC_JMPI_mask) == CPU_LATCH_PC_JMPI) {
        regs.PC = (u16)state.intermediate | ((u16)ibus << 8);
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
    if((op & CPU_LATCH_EADDR_HI_EXTC_mask) == CPU_LATCH_EADDR_HI_EXTC) {
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

    // set BRK vector
    if((op & CPU_LATCH_EADDR_BRK_mask) == CPU_LATCH_EADDR_BRK) {
        state.eaddr = 0xFFFE;
    }

    // check REGP latch
    if((op & CPU_LATCH_REGP_mask) == CPU_LATCH_REGP) {
        regs.P = ibus | 0x20; // bit 5 is always set
    }

    // A, X and Y always set N and Z flags when latched
#define NZ { \
        regs.P = (regs.P & ~CPU_FLAG_N) | (ibus_n ? CPU_FLAG_N : 0);  \
        regs.P = (regs.P & ~CPU_FLAG_Z) | (ibus_z ? CPU_FLAG_Z : 0);  \
    } 

    // check REGA latch
    if((op & CPU_LATCH_REGA_mask) == CPU_LATCH_REGA) {
        NZ;
        regs.A = ibus;
    }

    // check REGX latch
    if((op & CPU_LATCH_REGX_mask) == CPU_LATCH_REGX) {
        NZ;
        regs.X = ibus;
    }

    // check REGY latch
    if((op & CPU_LATCH_REGY_mask) == CPU_LATCH_REGY) {
        NZ;
        regs.Y = ibus;
    }

    // check REGS latch
    if((op & CPU_LATCH_REGS_mask) == CPU_LATCH_REGS) {
        regs.S = ibus;
    }

    // check INTM latch
    if((op & CPU_LATCH_INTM_mask) == CPU_LATCH_INTM) {
        if((op & CPU_LATCH_INTM_FLAGS_mask) == CPU_LATCH_INTM_FLAGS) {
            NZ;
        } else if((op & CPU_LATCH_INTM_CMP_mask) == CPU_LATCH_INTM_CMP) {
            NZ;
            regs.P = (regs.P & ~CPU_FLAG_C) | (alu_c ? CPU_FLAG_C : 0);
        } else if((op & CPU_LATCH_INTM_BIT_mask) == CPU_LATCH_INTM_BIT) {
            regs.P = (regs.P & ~CPU_FLAG_Z) | (ibus_z ? CPU_FLAG_Z : 0);
            regs.P = (regs.P & ~CPU_FLAG_N) | (bit_n ? CPU_FLAG_N : 0);
            regs.P = (regs.P & ~CPU_FLAG_V) | (bit_v ? CPU_FLAG_V : 0);
        }
        state.intermediate = ibus;
    }

    // set CV flags
    if((op & CPU_LATCH_CV_mask) == CPU_LATCH_CV) {
        regs.P = (regs.P & ~CPU_FLAG_C) | (alu_c ? CPU_FLAG_C : 0);
        regs.P = (regs.P & ~CPU_FLAG_V) | (alu_v ? CPU_FLAG_V : 0);
    }

    state.istep++;
    cycle_count++;

    // internal NMI signal goes high in the cycle AFTER the cycle where the edge was detected
    if(!state.do_nmi) state.did_nmi = false;
    state.do_nmi = state.nmi_detected;
    state.nmi_detected = state.nmi;

    // TODO NMI can hijack BRK if done before cycle 4 of the PPU scanline
    // check for NMI before opcode fetch
    if(state.do_nmi && !state.did_nmi && (state.ops && *state.ops == OPCODE_FETCH)) {
        state.did_nmi = 1;
        state.eaddr   = 0xFFFA;
        state.ops     = state.ops_base = CpuNMI;
        state.istep   = 0;
    }

	return ret;
}

bool CPU::Save(ostream& os, string& errmsg) const
{
    WriteVarInt(os, 0); // Reserved for future use in case we re-order components of structures

    os.write((char const*)&regs, sizeof(regs));

    WriteVarInt(os, state.nmi);
    WriteVarInt(os, state.nmi_detected);
    WriteVarInt(os, state.do_nmi);
    WriteVarInt(os, state.did_nmi);
    WriteVarInt(os, state.istep);
    WriteVarInt(os, state.opcode);
    WriteVarInt(os, state.intermediate);
    WriteVarInt(os, state.eaddr);
    WriteVarInt(os, state.inst_pc);

    // we can't save state.ops, so we use ops_base to determine what state it's in:
    // 0 - nullptr
    // 1 - CpuReset
    // 2 - CpuNMI
    // 3 - OpTable[state.opcode]
    // and we save the delta from that point
    if(state.ops == nullptr) WriteVarInt(os, 0);
    else {
        if(state.ops == CpuReset) WriteVarInt(os, 1);
        else if(state.ops == CpuNMI) WriteVarInt(os, 2);
        else WriteVarInt(os, 3);

        WriteVarInt(os, (uintptr_t)state.ops - (uintptr_t)state.ops_base);
    }

    WriteVarInt(os, cycle_count);

    errmsg = "Error saving CPU state"; // in case os.good() returns false
    return os.good();
}

bool CPU::Load(istream& is, string& errmsg)
{
    int r = ReadVarInt<int>(is); // reserved
    assert(r == 0);

    is.read((char*)&regs, sizeof(regs));

    state.nmi          = ReadVarInt<u8>(is);
    state.nmi_detected = ReadVarInt<u8>(is);
    state.do_nmi       = ReadVarInt<u8>(is);
    state.did_nmi      = ReadVarInt<u8>(is);
    state.istep        = ReadVarInt<u8>(is);
    state.opcode       = ReadVarInt<u8>(is);
    state.intermediate = ReadVarInt<u8>(is);
    state.eaddr        = ReadVarInt<u16>(is);
    state.inst_pc      = ReadVarInt<u16>(is);

    // determine state.ops (see Save())
    int v = ReadVarInt<int>(is);
    if(v == 0) { // nullptr
        state.ops = state.ops_base = nullptr;
    } else {
        if(v == 1)      state.ops_base = CpuReset;
        else if(v == 2) state.ops_base = CpuNMI;
        else if(v == 3) state.ops_base = OpTable[state.opcode];

        uintptr_t offs = ReadVarInt<uintptr_t>(is);
        state.ops = (u64 const*)((uintptr_t)state.ops_base + offs);
    }

    cycle_count = ReadVarInt<u64>(is);

    errmsg = "Error loading CPU state";
    return is.good();
}

}
