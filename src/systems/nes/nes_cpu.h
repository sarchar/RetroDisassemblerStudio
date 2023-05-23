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
    void Step();

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
        //u16        addr2;
        //bool       carry_addr;
        int const* ops;
    } state;

    u64    cycle_count;

    // even though they're variables, we're changing the naming convention
    read_func_t Read;
    write_func_t Write;
};

}
