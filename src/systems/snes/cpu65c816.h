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
    inline bool GetE()     const { return registers.e; }
    inline u8   GetFlags() const { return registers.flags; }
    inline u16  GetPC()    const { return registers.pc; }
    inline u8   GetA()     const { return registers.a; }
    inline u8   GetB()     const { return registers.b; }
    inline u16  GetC()     const { return registers.c; }
    inline u16  GetX()     const { return registers.x; }
    inline u8   GetXL()    const { return registers.xl; }
    inline u16  GetY()     const { return registers.y; }
    inline u8   GetYL()    const { return registers.yl; }
    inline u8   GetPBR()   const { return registers.pbr; }
    inline u8   GetDBR()   const { return registers.dbr; }
    inline u16  GetD()     const { return registers.d; }
    inline u16  GetS()     const { return registers.s; }

private:
    inline bool IsReadCycle()  const { return (pins.rw_n.Sample() && (pins.vda.Sample() || pins.vpa.Sample())); }
    inline bool IsWriteCycle() const { return (!pins.rw_n.Sample() && (pins.vda.Sample() || pins.vpa.Sample())); }

    inline bool IsWordMemoryEnabled() const { return (registers.e == 0 && (registers.flags & CPU_FLAG_M) == 0); }
    inline bool IsWordIndexEnabled()  const { return (registers.e == 0 && (registers.flags & CPU_FLAG_X) == 0); }

    void StartReset();
    void Reset();

    void ClockFallingEdge();
    void FinishInstructionCycle(u8);
    void StepMemoryAccessCycle(bool, bool, u8);
    void StartInstructionCycle();
    void SetupPinsLowCycle();
    void SetupPinsLowCycleForFetch();
    void SetupPinsLowCycleForStore();

    void ClockRisingEdge();
    void SetupPinsHighCycle();

    struct { 
        u8 flags;
        u8 e;    // the E flag is on its own

        u16 pc;
        u8 ir;
        union {
            struct {
                u8 dl;
                u8 dh;
            };
            u16 d;
        };
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

    typedef u64 UC_OPCODE;

    // bits 0-3
    static unsigned int const UC_FETCH_SHIFT   = 0;
    static unsigned int const UC_FETCH_MASK    = (0x0F << UC_FETCH_SHIFT);
    static unsigned int const UC_FETCH_NONE    = 0 << UC_FETCH_SHIFT;
    static unsigned int const UC_FETCH_OPCODE  = 1 << UC_FETCH_SHIFT;
    static unsigned int const UC_FETCH_MEMORY  = 2 << UC_FETCH_SHIFT;
    static unsigned int const UC_FETCH_PC      = 3 << UC_FETCH_SHIFT;
    static unsigned int const UC_FETCH_A       = 4 << UC_FETCH_SHIFT;
    static unsigned int const UC_FETCH_X       = 5 << UC_FETCH_SHIFT;
    static unsigned int const UC_FETCH_Y       = 6 << UC_FETCH_SHIFT;
    static unsigned int const UC_FETCH_D       = 7 << UC_FETCH_SHIFT;
    static unsigned int const UC_FETCH_S       = 8 << UC_FETCH_SHIFT;
    static unsigned int const UC_FETCH_ZERO    = 9 << UC_FETCH_SHIFT;

    // bits 4-6
    static unsigned int const UC_STORE_SHIFT   = 4;
    static unsigned int const UC_STORE_MASK    = (0x0F << UC_STORE_SHIFT);
    static unsigned int const UC_STORE_NONE    = 0 << UC_STORE_SHIFT;
    static unsigned int const UC_STORE_MEMORY  = 2 << UC_STORE_SHIFT;
    static unsigned int const UC_STORE_PC      = 3 << UC_STORE_SHIFT;
    static unsigned int const UC_STORE_A       = 4 << UC_STORE_SHIFT;
    static unsigned int const UC_STORE_X       = 5 << UC_STORE_SHIFT;
    static unsigned int const UC_STORE_Y       = 6 << UC_STORE_SHIFT;
    static unsigned int const UC_STORE_D       = 7 << UC_STORE_SHIFT;
    static unsigned int const UC_STORE_S       = 8 << UC_STORE_SHIFT;
    static unsigned int const UC_STORE_IR      = 9 << UC_STORE_SHIFT;

    // bits 8-11
    static unsigned int const UC_OPCODE_SHIFT  = 8;
    static unsigned int const UC_OPCODE_MASK   = (0x07 << UC_OPCODE_SHIFT);
    static unsigned int const UC_NOP           = 0 << UC_OPCODE_SHIFT;
    static unsigned int const UC_DEC           = 1 << UC_OPCODE_SHIFT;
    static unsigned int const UC_INC           = 2 << UC_OPCODE_SHIFT;
    static unsigned int const UC_EOR           = 3 << UC_OPCODE_SHIFT;
    static unsigned int const UC_ORA           = 4 << UC_OPCODE_SHIFT;
    static unsigned int const UC_DEAD          = 5 << UC_OPCODE_SHIFT;

    UC_OPCODE const* current_uc_set;
    u8               current_uc_set_pc;
    UC_OPCODE        current_uc_opcode;

    enum ADDRESSING_MODE {
        // Implied - no memory fetch or write
        AM_IMPLIED = 0,
        
        // Accumulator is not used directly. We use AM_IMPLIED and specify UC_FETCH/STORE_A
        // AM_ACCUMULATOR,

        // Immediate (Byte or Word) - Read an operand byte or word, depending on the 
        // memory/index modes and treat it as an immediate value
        AM_IMMEDIATE,

        // AM_IMMEDIATE_M / AM_IMMEDIATE_X ? byte or word depending on M and X bits

        // Direct Page - Read one operand byte and add the D register to get the effective address
        AM_DIRECT_PAGE,

        // Direct Page indexed with X/Y - Read one operand byte and add the D register and 
        // the X/Y register to get the effective address
        AM_DIRECT_INDEXED_X,
        AM_DIRECT_INDEXED_Y,

        // Direct page indexed with X indirect - Read one operand byte, add X and then X
        // then read an effective address from that address
        AM_DIRECT_INDEXED_X_INDIRECT,
        AM_DIRECT_INDIRECT_INDEXED_Y,

        // Absolute - Read one operand bytes and treat it as the effective address
        AM_ABSOLUTE,
        AM_ABSOLUTE_INDEXED_X,
        AM_ABSOLUTE_INDEXED_Y,
        AM_ABSOLUTE_INDIRECT,

        // Stack - Push or Pull from the stack using the S register
        AM_STACK,

        // Psuedo- or internal modes
        AM_IMMEDIATE_WORD, // Immediate (Word) - Read an operand word and treat it as an immediate value
        AM_VECTOR  // like AM_IMMEDIATE_WORD but asserts VPn, uses operand_address instead of registers.pc
    };

    ADDRESSING_MODE current_addressing_mode;

    enum MEMORY_STEP {
        MS_INIT = 0,
        MS_FETCH_VECTOR_LOW,
        MS_FETCH_VECTOR_HIGH,
        MS_FETCH_OPERAND_LOW,
        MS_FETCH_OPERAND_HIGH,
        MS_FETCH_OPERAND_BANK,
        MS_FETCH_INDIRECT_LOW,
        MS_FETCH_INDIRECT_HIGH,
        MS_FETCH_INDIRECT_BANK,
        MS_FETCH_VALUE_LOW,
        MS_FETCH_VALUE_HIGH,
        MS_FETCH_VALUE_BANK,
        MS_FETCH_STACK_LOW,
        MS_FETCH_STACK_HIGH,
        // TODO indexed adds, etc
        // TODO stores, etc
        MS_ADD_DL_REGISTER,
        MS_ADD_X_REGISTER,
        MS_ADD_Y_REGISTER,
        MS_MODIFY_WAIT,
        MS_MODIFY,
        MS_WRITE_VALUE_LOW,
        MS_WRITE_VALUE_HIGH,
        MS_WRITE_STACK_HIGH,
        MS_WRITE_STACK_LOW
    };

    MEMORY_STEP current_memory_step;

    // helper functions for changing to new states
    bool ShouldFetchOperandHigh();
    bool ShouldFetchOperandBank();
    bool ShouldFetchIndirectBank();
    bool ShouldFetchValueHigh();
    bool ShouldFetchValueBank();

    void SetMemoryStepAfterOperandFetch(bool);
    void SetMemoryStepAfterIndirectAddressFetch(bool);
    void SetMemoryStepAfterDirectPageAdded(bool);
    void SetMemoryStepAfterIndexRegisterAdded(bool);

    // the value to be put on the line during the high cycle of a write operation
    u8  data_w_value;

    // This union will break on little endian machines, but that's a long way off for now
    union {
        struct {
            u8  as_byte;
            u8  high_byte;
            u8  bank_byte;
        };
        struct {
            u16 as_word;
            u8  _unused0;
        };
    } intermediate_data, operand_address, indirect_address;

    unsigned int intermediate_data_size;

private:
    static UC_OPCODE const DECA_UC[];
    static UC_OPCODE const EOR_UC[];
    static UC_OPCODE const INC_UC[];
    static UC_OPCODE const INCA_UC[];
    static UC_OPCODE const INX_UC[];
    static UC_OPCODE const JMP_UC[];
    static UC_OPCODE const LDA_UC[];
    static UC_OPCODE const LDX_UC[];
    static UC_OPCODE const LDY_UC[];
    static UC_OPCODE const NOP_UC[];
    static UC_OPCODE const ORA_UC[];
    static UC_OPCODE const PHA_UC[];
    static UC_OPCODE const PHD_UC[];
    static UC_OPCODE const PLA_UC[];
    static UC_OPCODE const STA_UC[];
    static UC_OPCODE const STZ_UC[];
    static UC_OPCODE const TXS_UC[];
    static UC_OPCODE const DEAD_INSTRUCTION[];
    static UC_OPCODE const * const INSTRUCTION_UCs[256];
    static ADDRESSING_MODE const INSTRUCTION_ADDRESSING_MODES[256];
};
