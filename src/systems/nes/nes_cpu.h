#pragma once

#include <functional>

#include "util.h"

namespace NES {

class CPU {
public:
    typedef std::function<u8(u16)> read_func_t;
    typedef std::function<void(u16, u8)> write_func_t;

    enum class CPU_OP {
        NOP,
        RESET_LO,
        RESET_HI,
        IFETCH // end of instruction
    };


    CPU(read_func_t const& read_func, write_func_t const& write_func);
    ~CPU();

    void Reset();
    void Step();

private:
    struct {
        int A, X, Y, P, PC;
    } regs;

    struct {
        u8            istep;
        u8            opcode;
        CPU_OP const* ops;
    } state;

    u64    cycle_count;

    // even though they're variables, we're changing the naming convention
    read_func_t Read;
    write_func_t Write;
};

}
