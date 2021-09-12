#pragma once

#include "util.h"
#include "wires.h"

#define CPU_FLAG_E 0x01
#define CPU_FLAG_C 0x01
#define CPU_FLAG_Z 0x02
#define CPU_FLAG_I 0x04
#define CPU_FLAG_D 0x08
#define CPU_FLAG_B 0x10
#define CPU_FLAG_X 0x10
#define CPU_FLAG_M 0x20
#define CPU_FLAG_V 0x40
#define CPU_FLAG_N 0x80

class CPU65C816 {
public:
    CPU65C816();
    ~CPU65C816();

    struct {
        Wire phi2    { "cpu65c816.phi2" };
        Wire reset_n { "cpu65c816.reset_n" };
        Wire e       { "cpu65c816.e" };
        Wire rw_n    { "cpu65c816.rw_n" };
        Wire vda     { "cpu65c816.vda" };
        Wire vpa     { "cpu65c816.vpa" };
        Wire vp_n    { "cpu65c816.vp_n" };
        Wire mx      { "cpu65c816.mx" };
//        Bus<u8>  a;
//        Bus<u16> db;
    } pins;

private:
    void StartReset();
    void Reset();
    void ClockLow();
    void ClockHigh();

    void SetupPinsLowCycle();
    void SetupPinsHighCycle();

    struct { 
        u8 flags;
        u8 e;    // the E flag is on its own

        u16 pc;
        u8 ir;
        u16 d;
        u16 pbr;
        u16 dbr;
        union {
            struct {
                u8 a;
                u8 b;
            };
            u16 c;
        };
        union {
            struct {
                u8 xl;
                u8 xh;
            };
            u16 x;
        };
        union {
            struct {
                u8 yl;
                u8 yh;
            };
            u16 y;
        };
        union {
            struct {
                u8 sl;
                u8 sh;
            };
            u16 s;
        };
    } registers;

    bool vector_pull;

    enum {
        IC_OPCODE_FETCH,
        IC_VECTOR_PULL
    } instruction_cycle;

    enum {
        STATE_RESET,
        STATE_RUNNING
    } current_state;
};
