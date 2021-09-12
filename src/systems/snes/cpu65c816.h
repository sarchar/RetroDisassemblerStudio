#pragma once

#include "util.h"
#include "wires.h"

class CPU65C816 {
public:
    CPU65C816();
    ~CPU65C816();

    struct {
        Wire phi2    { "cpu65c816.phi2" };
//        Bus<u8>  a;
//        Bus<u16> db;
        Wire reset_n { "cpu65c816.reset_n" };
    } pins;

private:
    void StartReset();
    void ClockLow();
    void ClockHigh();

    u16 pc;
    u16 dp;
    u16 pb;
    u16 db;

    u8 inst_cycle;

    enum {
        STATE_PRE_RESET,
        STATE_RESET,
        STATE_RUNNING
    } current_state;
};
