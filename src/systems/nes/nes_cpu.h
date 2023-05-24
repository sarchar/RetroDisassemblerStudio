#pragma once

#include <functional>

#include "util.h"

namespace NES {

class CPU {
public:
    typedef std::function<u8(u16)> read_func_t;
    typedef std::function<void(u16, u8)> write_func_t;

    CPU(read_func_t const& read_func, write_func_t const& write_func);
    ~CPU();

    void Reset();
    bool Step(); // return true on instruction decode cycle

    u64 GetNextUC()   const { auto ptr = state.ops; if(!ptr) return (u64)-1; else return *ptr; }
    u16 GetOpcode()   const { return state.opcode; }
    u16 GetOpcodePC() const { return state.inst_pc; }
    u16 GetPC()       const { return regs.PC; }

private:
    struct {
        u8  A, X, Y, S, P;
        u16 PC;
    } regs;

    struct {
        u8         istep;
        u8         opcode;
        u8         intermediate;
        u16        eaddr;
        u64 const* ops;

        u16        inst_pc;
    } state;

    u64    cycle_count;

    // even though they're variables, we're changing the naming convention
    read_func_t Read;
    write_func_t Write;
};

}
