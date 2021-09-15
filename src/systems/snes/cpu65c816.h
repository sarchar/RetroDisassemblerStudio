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
        Wire     phi2    { "cpu65c816.phi2" };
        Wire     reset_n { "cpu65c816.reset_n" };
        Wire     e       { "cpu65c816.e" };
        Wire     rw_n    { "cpu65c816.rw_n" };
        Wire     vda     { "cpu65c816.vda" };
        Wire     vpa     { "cpu65c816.vpa" };
        Wire     vp_n    { "cpu65c816.vp_n" };
        Wire     mx      { "cpu65c816.mx" };
        Bus<u16> a       { "cpu65c816.a" };
        Bus<u8>  db      { "cpu65c816.db" };

        // phi2_setup isn't a true 65c816 pin, but its use is to simulate the setup time
        // on the DB/VPA/VDA/VPn/RWn lines. I.e., We can't assert DB immediately when the clock
        // falls because something else, like RAM, might be asserting DATA that we're going to
        // latch on the falling edge.  On the falling clock edge, we set VDA and VPA to 0 so the 
        // data address is considered invalid and all devices release the data bus (which
        // happens one master clock later -- that was for simulating HOLD times). One master clock
        // after that, we can set up the R/W lines, which I guess technically is later than 
        // an actual CPU but still happens before the rising PHI2 clock.
        Wire     signal_setup { "cpu65c816.signal_setup" };
    } pins;

    // Debugging interface
public:
    bool GetE() const { return registers.e; }
    u8   GetFlags() const { return registers.flags; }

    u16  GetPC() const { return registers.pc; }
    u8   GetA()  const { return registers.a; }
    u8   GetB()  const { return registers.b; }
    u16  GetC()  const { return registers.c; }
    u16  GetX()  const { return registers.x; }
    u8   GetXL() const { return registers.xl; }
    u16  GetY()  const { return registers.y; }
    u8   GetYL() const { return registers.yl; }

private:
    inline bool IsReadCycle()  const { return (pins.rw_n.Sample() && (pins.vda.Sample() || pins.vpa.Sample())); }
    inline bool IsWriteCycle() const { return (!pins.rw_n.Sample() && (pins.vda.Sample() || pins.vpa.Sample())); }

    void StartReset();
    void Reset();

    void ClockFallingEdge();
    void FinishInstructionCycle(u8);
    void StartInstructionCycle();
    void SetupPinsLowCycle();

    void ClockRisingEdge();
    void SetupPinsHighCycle();

    struct { 
        u8 flags;
        u8 e;    // the E flag is on its own

        u16 pc;
        u8 ir;
        u16 d;
        u8 pbr;
        u8 dbr;
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

    enum INSTRUCTION_CYCLE {
        IC_VECTOR_PULL_LOW,
        IC_VECTOR_PULL_HIGH,
        IC_OPCODE_FETCH,
        IC_WORD_IMM_LOW,
        IC_WORD_IMM_HIGH,
        IC_STORE_PC_OPCODE_FETCH,
        IC_DEAD
    };
    INSTRUCTION_CYCLE const* current_instruction_cycle_set;
    u8                       current_instruction_cycle_set_pc;
    INSTRUCTION_CYCLE        instruction_cycle;

    u16 data_fetch_address;
    u16 word_immediate;

private:
    static INSTRUCTION_CYCLE const VECTOR_PULL_UC[];
    static INSTRUCTION_CYCLE const JMP_UC[];
    static INSTRUCTION_CYCLE const DEAD_INSTRUCTION[];
    static INSTRUCTION_CYCLE const * const INSTRUCTION_UCs[256];
};
