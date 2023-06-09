// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <functional>

#include "util.h"

#define CPU_FLAG_C (1 << 0)
#define CPU_FLAG_Z (1 << 1)
#define CPU_FLAG_I (1 << 2)
#define CPU_FLAG_D (1 << 3)
#define CPU_FLAG_B (1 << 4)
#define CPU_FLAG_V (1 << 6)
#define CPU_FLAG_N (1 << 7)

namespace Systems::NES {

class CPU {
public:
    typedef std::function<u8(u16, bool)> read_func_t;
    typedef std::function<void(u16, u8)> write_func_t;

    CPU(read_func_t const& read_func, write_func_t const& write_func);
    ~CPU();

    void Reset();
    bool Step(); // return true on instruction decode cycle 
    inline void DmaStep() { cycle_count++; }
    inline void Nmi(int high) { state.nmi = high; }

    inline s64  GetNextUC()     const { auto ptr = state.ops; if(!ptr) return (u64)-1; else return *ptr; }
    inline u16  GetOpcode()     const { return state.opcode; }
    inline u16  GetOpcodePC()   const { return state.inst_pc; }
    inline int  GetIStep()      const { return state.istep; }
    inline u16  GetPC()         const { return regs.PC; }
    inline u8   GetP()          const { return regs.P; }
    inline u8   GetA()          const { return regs.A; }
    inline u8   GetX()          const { return regs.X; }
    inline u8   GetY()          const { return regs.Y; }
    inline u16  GetS()          const { return regs.S; }
    inline u64  GetCycleCount() const { return cycle_count; }

    inline void SetPC(u16 v)          { regs.PC = v; }
    inline void SetP (u8  v)          { regs.P  = v; }
    inline void SetA (u8  v)          { regs.A  = v; }
    inline void SetX (u8  v)          { regs.X  = v; }
    inline void SetY (u8  v)          { regs.Y  = v; }
    inline void SetS (u16 v)          { regs.S  = v; }

    // the definition for CPU_READ is in nes_cpu_tables.h, but this inline optimization is worth it
    inline bool IsReadCycle()  const { return state.ops && ((*state.ops & 0x04) == 0); }

    bool Save(std::ostream&, std::string&) const;
    bool Load(std::istream&, std::string&);

private:
    struct {
        // Do not re-order this structure without fixing Save/Load
        u8  A, X, Y, S, P;
        u16 PC;
    } regs;

    struct {
        // This structure can safely be reordered
        u8         nmi;
        u8         nmi_detected;
        u8         do_nmi;
        u8         did_nmi;
        u8         istep;
        u8         opcode;
        u8         intermediate;
        u16        eaddr;
        u64 const* ops;
        u64 const* ops_base;

        u16        inst_pc;
    } state;

    u64    cycle_count;

    // even though they're variables, we're changing the naming convention
    read_func_t Read;
    write_func_t Write;
};

}
