#pragma once

#define CPU_FLAG_C (1 << 0)
#define CPU_FLAG_Z (1 << 1)
#define CPU_FLAG_I (1 << 2)
#define CPU_FLAG_D (1 << 3)
#define CPU_FLAG_B (1 << 4)
#define CPU_FLAG_V (1 << 6)
#define CPU_FLAG_N (1 << 7)

//1#define CPU_OP_ASSERT    0x00
//1#define CPU_OP_NOP       0x01
//1#define CPU_OP_IFETCH    0x02
//1#define CPU_OP_READV     0x03
//1#define CPU_OP_READMEM   0x04
//1#define CPU_OP_READMEM2  0x05
//1#define CPU_OP_SETF      0x06
//1#define CPU_OP_CLEARF    0x07
//1#define CPU_OP_READA     0x08
//1#define CPU_OP_READX     0x09
//1#define CPU_OP_READY     0x0A
//1#define CPU_OP_READS     0x0B
//1#define CPU_OP_READP     0x0C
//1#define CPU_OP_READI     0x0D
//1#define CPU_OP_INDEXED_X 0x0E
//1#define CPU_OP_INDEXED_Y 0x0F
//1#define CPU_OP_CARRYADDR 0x10
//1#define CPU_OP_STACKR    0x11
//1#define CPU_OP_GET_SOURCE(x) ((x) & 0xFF)
//1
//1#define CPU_OP_GET_VECTOR(x) (((x) >> 24) & 0xFFFF)
//1
//1#define CPU_OP_LATCH_NOP     (0x00 << 8)
//1#define CPU_OP_LATCHPC_LO    (0x01 << 8)
//1#define CPU_OP_LATCHPC_HI    (0x02 << 8)
//1#define CPU_OP_LATCHADDR     (0x03 << 8)
//1#define CPU_OP_LATCHADDR_LO  (0x04 << 8)
//1#define CPU_OP_LATCHADDR_HI  (0x05 << 8)
//1#define CPU_OP_LATCHADDR2    (0x06 << 8)
//1#define CPU_OP_LATCHADDR2_LO (0x07 << 8)
//1#define CPU_OP_LATCHADDR2_HI (0x08 << 8)
//1#define CPU_OP_DECODE        (0x09 << 8)
//1#define CPU_OP_LATCHP        (0x0A << 8)
//1#define CPU_OP_LATCHA        (0x0B << 8)
//1#define CPU_OP_LATCHX        (0x0C << 8)
//1#define CPU_OP_LATCHY        (0x0D << 8)
//1#define CPU_OP_LATCHS        (0x0E << 8)
//1#define CPU_OP_LATCHI        (0x0F << 8)
//1#define CPU_OP_WRITEMEM      (0x10 << 8)
//1#define CPU_OP_STACKW        (0x11 << 8)
//1#define CPU_OP_GET_WRITE(x) ((x) & 0xFF00)
//1
//1#define CPU_OP_ALU_NOP       (0x00 << 16)
//1#define CPU_OP_ALU_INC       (0x01 << 16)
//1#define CPU_OP_ALU_DEC       (0x02 << 16)
//1#define CPU_OP_ALU_ADC       (0x03 << 16)
//1#define CPU_OP_ALU_SBC       (0x04 << 16)
//1#define CPU_OP_ALU_AND       (0x05 << 16)
//1#define CPU_OP_ALU_EOR       (0x06 << 16)
//1#define CPU_OP_ALU_ORA       (0x07 << 16)
//1#define CPU_OP_ALU_ASL       (0x08 << 16)
//1#define CPU_OP_ALU_LSR       (0x09 << 16)
//1#define CPU_OP_ALU_ROL       (0x0A << 16)
//1#define CPU_OP_ALU_ROR       (0x0B << 16)
//1#define CPU_OP_ALU_CMP       (0x0C << 16)
//1#define CPU_OP_ALU_CPX       (0x0D << 16)
//1#define CPU_OP_ALU_CPY       (0x0E << 16)
//1#define CPU_OP_GET_ALU(x)    ((x) & 0xFF0000)
//1
//1// helpers/shorthands
//1#define CPU_OP_READV_(x)    (               CPU_OP_READV | (x) << 24)
//1#define CPU_OP_CLEARF_(x)   (CPU_OP_LATCHP | CPU_OP_CLEARF | (x) << 24)
//1#define CPU_OP_SETF_(x)     (CPU_OP_LATCHP | CPU_OP_SETF | (x) << 24)
//1#define CPU_OP_GET_FLAG(x)  (((x) >> 24) & 0xFF)
//1#define CPU_OP_END          (CPU_OP_DECODE | CPU_OP_IFETCH)

//    CPU_OP_NOP, CPU_OP_NOP, CPU_OP_NOP, CPU_OP_NOP, CPU_OP_NOP, CPU_OP_NOP, CPU_OP_NOP, // 7 cycles for reset
//    CPU_OP_LATCHPC_LO | CPU_OP_READV_(0xFFFC),
//    CPU_OP_LATCHPC_HI | CPU_OP_READV_(0xFFFD),
//    CPU_OP_DECODE | CPU_OP_IFETCH
//};

#define IFETCH(x) \
    (x) | CPU_OP_IFETCH

#define READMEM(x) \
    CPU_ADDRESS_BUS_EADDR | CPU_READ | (x)

#define WRITEMEM(x) \
    CPU_ADDRESS_BUS_EADDR | CPU_WRITE | (x)

#define ZP \
    CPU_OP_LATCHADDR | CPU_OP_IFETCH

// I kinda like the idea of specifying more lower level microcode. I'd need to specify:
// * READ or WRITE signal
// * Address to put on bus
// * For READ, data to put on bus
// * ALU: operation, A input mux, B input mux, C carry source (C, 0, or 1)
// internal data mux: select READ data bus or ALU data bus, or another?
// * latch signals for each register
// in this method, every cycle runs roughly the same code
//
// So LDA zeropage,X might look like:
//
// LATCH_OPCODE, BUS_DATA, ALUOP(zzz), INCPC, READ(PC)
// RESET_EADDR_HI, LATCH_EADDR_LO,  BUS_DATA, ALUOP(zzz), INCPC, READ(PC)
// LATCH_EADDR_LO, BUS_ALU, ALUOP(ADC, a_in=X, b_in=EADDR, carry=0), READ(EADDR)
// LATCH_A, BUS_DATA, READ(EADDR)
//      
#define ON(flag) (1 << (flag))
#define OFF(flag) 0

// 2 bits: address bus MUX
#define CPU_ADDRESS_shift     0
#define CPU_ADDRESS_bits      2
#define CPU_ADDRESS_mask      0x03
#define CPU_ADDRESS_BUS_PC    (0 << 0)
#define CPU_ADDRESS_BUS_EADDR (1 << 0)

// 1 bit: R/W. every cycle produces a read or write. the 65C816 has VDA/VPA pins that would let us avoid that
#define CPU_RW_shift (CPU_ADDRESS_shift + CPU_ADDRESS_bits)
#define CPU_RW_bits  1
#define CPU_RW_mask  (1 << 2)
#define CPU_READ     OFF(CPU_RW_shift)
#define CPU_WRITE    ON(CPU_RW_shift)

// 1 bit: PC increment
#define CPU_INCPC_shift (CPU_RW_shift + CPU_RW_bits) 
#define CPU_INCPC_bits  1
#define CPU_INCPC_mask  (1 << CPU_INCPC_shift) 
#define CPU_INCPC       ON(CPU_INCPC_shift)

// 2 bits: internal bus: ALU or DATA lines
#define CPU_IBUS_shift (CPU_INCPC_shift + CPU_INCPC_bits)
#define CPU_IBUS_bits  2
#define CPU_IBUS_mask  (3 << CPU_IBUS_shift)
#define CPU_IBUS_DATA  (0 << CPU_IBUS_shift)
#define CPU_IBUS_ALU   (1 << CPU_IBUS_shift)

// 1 bit: latch opcode
#define CPU_LATCH_OPCODE_shift (CPU_IBUS_shift + CPU_IBUS_bits)
#define CPU_LATCH_OPCODE_bits  1
#define CPU_LATCH_OPCODE_mask  (1 << CPU_LATCH_OPCODE_shift)
#define CPU_LATCH_OPCODE       ON(CPU_LATCH_OPCODE_shift)

// 1 bit: PC JMP latch
#define CPU_LATCH_PC_JMP_shift (CPU_LATCH_OPCODE_shift + CPU_LATCH_OPCODE_bits)
#define CPU_LATCH_PC_JMP_bits  1
#define CPU_LATCH_PC_JMP_mask  (1 << CPU_LATCH_PC_JMP_shift)
#define CPU_LATCH_PC_JMP       ON(CPU_LATCH_PC_JMP_shift)

// 1 bit: PC JMP latch
#define CPU_LATCH_PC_BRANCH_shift (CPU_LATCH_PC_JMP_shift + CPU_LATCH_PC_JMP_bits)
#define CPU_LATCH_PC_BRANCH_bits  1
#define CPU_LATCH_PC_BRANCH_mask  (1 << CPU_LATCH_PC_BRANCH_shift)
#define CPU_LATCH_PC_BRANCH       ON(CPU_LATCH_PC_BRANCH_shift)

// 1 bit: PC BRANCH set
#define CPU_CHECK_BRANCH_SET_shift (CPU_LATCH_PC_BRANCH_shift + CPU_LATCH_PC_BRANCH_bits)
#define CPU_CHECK_BRANCH_SET_bits  1
#define CPU_CHECK_BRANCH_SET_mask  (1 << CPU_CHECK_BRANCH_SET_shift)
#define CPU_CHECK_BRANCH_SET       ON(CPU_CHECK_BRANCH_SET_shift)

// 1 bit: PC BRANCH clear
#define CPU_CHECK_BRANCH_CLEAR_shift (CPU_CHECK_BRANCH_SET_shift + CPU_CHECK_BRANCH_SET_bits)
#define CPU_CHECK_BRANCH_CLEAR_bits  1
#define CPU_CHECK_BRANCH_CLEAR_mask  (1 << CPU_CHECK_BRANCH_CLEAR_shift)
#define CPU_CHECK_BRANCH_CLEAR       ON(CPU_CHECK_BRANCH_CLEAR_shift)

// 1 bit: PC HI
#define CPU_LATCH_PC_HI_shift (CPU_CHECK_BRANCH_CLEAR_shift + CPU_CHECK_BRANCH_CLEAR_bits)
#define CPU_LATCH_PC_HI_bits  1
#define CPU_LATCH_PC_HI_mask  (1 << CPU_LATCH_PC_HI_shift)
#define CPU_LATCH_PC_HI       ON(CPU_LATCH_PC_HI_shift)

// 1 bit: EADDR_LO latch
#define CPU_LATCH_EADDR_LO_shift (CPU_LATCH_PC_HI_shift + CPU_LATCH_PC_HI_bits)
#define CPU_LATCH_EADDR_LO_bits  1
#define CPU_LATCH_EADDR_LO_mask  (1 << CPU_LATCH_EADDR_LO_shift)
#define CPU_LATCH_EADDR_LO       ON(CPU_LATCH_EADDR_LO_shift)

// 1 bit: EADDR_HI latch
#define CPU_LATCH_EADDR_HI_shift (CPU_LATCH_EADDR_LO_shift + CPU_LATCH_EADDR_LO_bits)
#define CPU_LATCH_EADDR_HI_bits  1
#define CPU_LATCH_EADDR_HI_mask  (1 << CPU_LATCH_EADDR_HI_shift)
#define CPU_LATCH_EADDR_HI       ON(CPU_LATCH_EADDR_HI_shift)

// 4 bits: ALU op
#define CPU_ALU_OP_shift  (CPU_LATCH_EADDR_HI_shift + CPU_LATCH_EADDR_HI_bits)
#define CPU_ALU_OP_bits   4
#define CPU_ALU_OP_mask   (0x0F << CPU_ALU_OP_shift)
#define CPU_ALU_OP_IDLE   (0 << CPU_ALU_OP_shift)   // not an actual state on the cpu, but an optimization for us
#define CPU_ALU_OP_ADC    (1 << CPU_ALU_OP_shift)
#define CPU_ALU_OP_SBC    (2 << CPU_ALU_OP_shift)
#define CPU_ALU_OP_AND    (3 << CPU_ALU_OP_shift)
#define CPU_ALU_OP_OR     (4 << CPU_ALU_OP_shift)
#define CPU_ALU_OP_CLRBIT (15 << CPU_ALU_OP_shift)

// 3 bits: ALU A source
#define CPU_ALU_A_shift   (CPU_ALU_OP_shift + CPU_ALU_OP_bits)
#define CPU_ALU_A_bits    3
#define CPU_ALU_A_mask    (0x07 << CPU_ALU_A_shift)
#define CPU_ALU_A_REGA    (0 << CPU_ALU_A_shift)
#define CPU_ALU_A_REGX    (1 << CPU_ALU_A_shift)
#define CPU_ALU_A_REGY    (2 << CPU_ALU_A_shift)
#define CPU_ALU_A_REGS    (3 << CPU_ALU_A_shift)
#define CPU_ALU_A_PC_LO   (4 << CPU_ALU_A_shift)
#define CPU_ALU_A_PC_HI   (5 << CPU_ALU_A_shift)
#define CPU_ALU_A_INTM    (6 << CPU_ALU_A_shift)
#define CPU_ALU_A_REGP    (7 << CPU_ALU_A_shift)

// 4 bits: ALU B source
#define CPU_ALU_B_shift    (CPU_ALU_A_shift + CPU_ALU_A_bits)
#define CPU_ALU_B_bits     4
#define CPU_ALU_B_mask     (0x0F << CPU_ALU_B_shift)
#define CPU_ALU_B_ZERO     (0 << CPU_ALU_B_shift)
#define CPU_ALU_B_EADDR_LO (1 << CPU_ALU_B_shift)
#define CPU_ALU_B_FLAG_C   (8 << CPU_ALU_B_shift)
#define CPU_ALU_B_FLAG_D   (9 << CPU_ALU_B_shift)
#define CPU_ALU_B_FLAG_I   (10 << CPU_ALU_B_shift)
#define CPU_ALU_B_FLAG_V   (11 << CPU_ALU_B_shift)
#define CPU_ALU_B_FLAG_Z   (12 << CPU_ALU_B_shift)
#define CPU_ALU_B_FLAG_N   (13 << CPU_ALU_B_shift)

// 2 bits: C source
#define CPU_ALU_C_shift    (CPU_ALU_B_shift + CPU_ALU_B_bits)
#define CPU_ALU_C_bits     2
#define CPU_ALU_C_mask     (3 << CPU_ALU_C_shift)
#define CPU_ALU_C_DEFAULT  (0 << CPU_ALU_C_shift)
#define CPU_ALU_C_ZERO     (1 << CPU_ALU_C_shift)
#define CPU_ALU_C_ONE      (2 << CPU_ALU_C_shift)

// 1 bit: REGP latch
#define CPU_LATCH_REGP_shift (CPU_ALU_B_shift + CPU_ALU_B_bits)
#define CPU_LATCH_REGP_bits  1
#define CPU_LATCH_REGP_mask  (1 << CPU_LATCH_REGP_shift)
#define CPU_LATCH_REGP       ON(CPU_LATCH_REGP_shift)

// 1 bit: REGA latch
#define CPU_LATCH_REGA_shift (CPU_LATCH_REGP_shift + CPU_LATCH_REGP_bits)
#define CPU_LATCH_REGA_bits  1
#define CPU_LATCH_REGA_mask  (1 << CPU_LATCH_REGA_shift)
#define CPU_LATCH_REGA       ON(CPU_LATCH_REGA_shift)

// 1 bit: REGX latch
#define CPU_LATCH_REGX_shift (CPU_LATCH_REGA_shift + CPU_LATCH_REGA_bits)
#define CPU_LATCH_REGX_bits  1
#define CPU_LATCH_REGX_mask  (1 << CPU_LATCH_REGX_shift)
#define CPU_LATCH_REGX       ON(CPU_LATCH_REGX_shift)

// 1 bit: REGY latch
#define CPU_LATCH_REGY_shift (CPU_LATCH_REGX_shift + CPU_LATCH_REGX_bits)
#define CPU_LATCH_REGY_bits  1
#define CPU_LATCH_REGY_mask  (1 << CPU_LATCH_REGY_shift)
#define CPU_LATCH_REGY       ON(CPU_LATCH_REGY_shift)

// 1 bit: REGS latch
#define CPU_LATCH_REGS_shift (CPU_LATCH_REGY_shift + CPU_LATCH_REGY_bits)
#define CPU_LATCH_REGS_bits  1
#define CPU_LATCH_REGS_mask  (1 << CPU_LATCH_REGS_shift)
#define CPU_LATCH_REGS       ON(CPU_LATCH_REGS_shift)

// 3 bits: data bus
#define CPU_DATA_BUS_shift   (CPU_LATCH_REGY_shift + CPU_LATCH_REGY_bits)
#define CPU_DATA_BUS_bits    3
#define CPU_DATA_BUS_mask    (7 << CPU_DATA_BUS_shift)
#define CPU_DATA_BUS_REGA    (0 << CPU_DATA_BUS_shift)
#define CPU_DATA_BUS_REGX    (1 << CPU_DATA_BUS_shift)
#define CPU_DATA_BUS_REGY    (2 << CPU_DATA_BUS_shift)

#define OPCODE_FETCH \
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_OPCODE

static int CpuReset[] = {
    // fetch the vector in regs.PC and jump to it
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR_LO,
    CPU_ADDRESS_BUS_PC | CPU_READ             | CPU_IBUS_DATA | CPU_LATCH_PC_JMP,
    // fetch the next instruction
    OPCODE_FETCH
};

#define ZP_X \
    CPU_LATCH_OPCODE | CPU_IBUS_DATA | CPU_OP_INCPC | CPU_OP_READ | CPU_ADDRESS_BUS_PC,

//!// zero page indexed X and Y never carry (4 cycles):
//!// inst, operand, bogus read and index, read
//!#define ZP_X \
//!    CPU_OP_LATCHADDR    | CPU_OP_IFETCH,    \
//!    CPU_OP_LATCH_NOP    | CPU_OP_READMEM | CPU_OP_ALU_INDEXED_X
//!
//!#define ZP_Y \
//!    CPU_OP_LATCHADDR    | CPU_OP_IFETCH,    \
//!    CPU_OP_LATCH_NOP    | CPU_OP_READMEM | CPU_OP_ALU_INDEXED_Y
//!
#define ABS \
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR_LO,  \
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR_HI

//!// absolute X and Y only incur a 5th carry cycle if the page boundary changes
//!// inst, operand, operand + indexed -> low addr byte, high addr byte?, readmem
//!#define ABS_X \
//!    CPU_OP_LATCHADDR    | CPU_OP_IFETCH,    \
//!    CPU_OP_LATCHADDR_HI | CPU_OP_IFETCH | CPU_OP_ALU_INDEXED_X | CPU_OP_HOP_ON_CARRYADDR,    \
//!    CPU_OP_LATCHADDR_HI | CPU_OP_CARRYADDR
//!
//!#define ABS_Y \
//!    CPU_OP_LATCHADDR    | CPU_OP_IFETCH,    \
//!    CPU_OP_LATCHADDR_HI | CPU_OP_IFETCH | CPU_OP_ALU_INDEXED_Y | CPU_OP_HOP_ON_CARRYADDR,    \
//!    CPU_OP_LATCHADDR_HI | CPU_OP_CARRYADDR
//!
//!#define IND_X \
//!    CPU_OP_LATCHADDR2   | CPU_OP_IFETCH,    /* read zp */ \
//!    CPU_OP_LATCHADDR_LO | CPU_OP_READMEM2 | CPU_OP_ALU_INCADDR2,   /* read pointer low, increment address */ \
//!    CPU_OP_LATCHADDR_HI | CPU_OP_READMEM2 | CPU_OP_ALU_INDEXED_X, /* add X */ \
//!    CPU_OP_LATCHADDR_HI | CPU_OP_READMEM  | CPU_OP_ALU_CARRYADDR /* add high byte */
//!
//!#define IND_Y \
//!    CPU_OP_LATCHADDR2   | CPU_OP_IFETCH,     /* read zp */ \
//!    CPU_OP_LATCHADDR_LO | CPU_OP_READMEM2,   /* read word from zp addr */ \
//!    CPU_OP_LATCHADDR_HI | CPU_OP_READMEM2,   /* . */ \
//!    CPU_OP_LATCHADDR    | CPU_OP_INDEXED_Y,  /* add Y */ \
//!    CPU_OP_LATCHADDR_HI | CPU_OP_CARRYADDR   /* cross page boundary */ \
//!
//!// Not all of these are used for each load (i.e., LDX doesn't have zp,x)
#define LD(inst, x) \
    static int CpuOp##inst##_imm[]  = {                                                \
        CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | x, OPCODE_FETCH }; \
    static int CpuOp##inst##_abs[]  = { ABS  , READMEM(x), OPCODE_FETCH };
//!    static int CpuOp##inst##_zp[]   = { ZP   , READMEM(x), END }; \
//!    static int CpuOp##inst##_zpx[]  = { ZP_X , READMEM(x), END }; \
//!    static int CpuOp##inst##_zpy[]  = { ZP_Y , READMEM(x), END };
//!    static int CpuOp##inst##_absX[] = { ABS_X, READMEM(x), END }; \
//!    static int CpuOp##inst##_absY[] = { ABS_Y, READMEM(x), END }; \
//!    static int CpuOp##inst##_indX[] = { IND_X, READMEM(x), END }; \
//!    static int CpuOp##inst##_indY[] = { IND_Y, READMEM(x), END }; 

LD(LDA, CPU_LATCH_REGA);
LD(LDX, CPU_LATCH_REGX);
LD(LDY, CPU_LATCH_REGY);

#undef LD

//!    static int CpuOp##inst##_imm[]  = {        IFETCH(x) , END }; \
//!    static int CpuOp##inst##_zp[]   = { ZP   , WRITEMEM(x), END }; \
//!    static int CpuOp##inst##_zpx[]  = { ZP_X , WRITEMEM(x), END }; \
//!    static int CpuOp##inst##_zpy[]  = { ZP_Y , WRITEMEM(x), END };
#define ST(inst, x) \
    static int CpuOp##inst##_abs[] = { ABS, WRITEMEM(x), OPCODE_FETCH };
//!    static int CpuOp##inst##_abs[]  = { ABS  , WRITEMEM(x), END }; \
//!    static int CpuOp##inst##_absX[] = { ABS_X, WRITEMEM(x), END }; \
//!    static int CpuOp##inst##_absY[] = { ABS_Y, WRITEMEM(x), END }; \
//!    static int CpuOp##inst##_indX[] = { IND_X, WRITEMEM(x), END }; \
//!    static int CpuOp##inst##_indY[] = { IND_Y, WRITEMEM(x), END }; 

ST(STA, CPU_DATA_BUS_REGA);
ST(STX, CPU_DATA_BUS_REGX);
ST(STY, CPU_DATA_BUS_REGY);

#undef ST
#define T(inst, alu_a, latch) \
    static int CpuOp##inst[] = { CPU_ALU_OP_OR | alu_a | CPU_ALU_B_ZERO | latch, OPCODE_FETCH };

T(TAX, CPU_ALU_A_REGA, CPU_LATCH_REGX);
T(TAY, CPU_ALU_A_REGA, CPU_LATCH_REGY);
T(TSX, CPU_ALU_A_REGS, CPU_LATCH_REGX);
T(TXA, CPU_ALU_A_REGX, CPU_LATCH_REGA);
T(TXS, CPU_ALU_A_REGX, CPU_LATCH_REGS);
T(TYA, CPU_ALU_A_REGY, CPU_LATCH_REGA);

#undef T
#define FL(x) \
    static int CpuOpSE##x[] = {                                                                   \
        CPU_IBUS_ALU | CPU_ALU_OP_OR     | CPU_ALU_A_REGP | CPU_ALU_B_FLAG_##x | CPU_LATCH_REGP,  \
        OPCODE_FETCH };                                                                           \
    static int CpuOpCL##x[] = {                                                                   \
        CPU_IBUS_ALU | CPU_ALU_OP_CLRBIT | CPU_ALU_A_REGP | CPU_ALU_B_FLAG_##x | CPU_LATCH_REGP,  \
        OPCODE_FETCH };

FL(C);
FL(D);
FL(I);
FL(V);

#undef FL
//!
//!#define ST(x) \
//!    static int CpyOpPH##x[] = {                        \
//!        CPU_OP_STACKW | CPU_OP_READ##x,                \
//!        CPU_OP_LATCHS | CPU_OP_READS | CPU_OP_ALU_DEC, \
//!        END                                            \
//!    };                                                 \
//!    static int CpyOpPL##x[] = {                        \
//!        CPU_OP_LATCHS | CPU_OP_READS | CPU_OP_ALU_INC, \
//!        CPU_OP_LATCH##x | CPU_OP_STACKR,               \
//!        END                                            \
//!    };
//!
//!ST(A);
//!ST(P);
//!
//!#undef ST
//!
//!#define ID(inst, x) \
//!    static int CpuOpIN##inst[] = { CPU_OP_LATCH##x | CPU_OP_READ##x | CPU_OP_ALU_INC, END }; \
//!    static int CpuOpDE##inst[] = { CPU_OP_LATCH##x | CPU_OP_READ##x | CPU_OP_ALU_DEC, END };
//!
//!ID(C, A);
//!ID(X, X);
//!ID(Y, Y);
//!
//!#undef ID
//!
//!#define A(x) \
//!    static int CpuOp##x##_imm[]  = {        IFETCH(CPU_OP_LATCHA)  | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_zp[]   = { ZP   , READMEM(CPU_OP_LATCHA) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_zpx[]  = { ZP_X , READMEM(CPU_OP_LATCHA) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_zpy[]  = { ZP_Y , READMEM(CPU_OP_LATCHA) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_abs[]  = { ABS  , READMEM(CPU_OP_LATCHA) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_absX[] = { ABS_X, READMEM(CPU_OP_LATCHA) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_absY[] = { ABS_Y, READMEM(CPU_OP_LATCHA) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_indX[] = { IND_X, READMEM(CPU_OP_LATCHA) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_indY[] = { IND_Y, READMEM(CPU_OP_LATCHA) | CPU_OP_ALU_##x, END }; 
//!
//!A(ADC);
//!A(SBC);
//!A(AND);
//!A(EOR);
//!A(ORA);
//!
//!#undef A
//!
//!#define RMW(x) \
//!    static int CpuOp##x##_acc[]  = {                   \
//!        CPU_OP_LATCHA | CPU_OP_READA | CPU_OP_ALU_##x, \
//!        END };                                         \
//!    static int CpuOp##x##_zp[]  = {                    \
//!        ZP,                                            \
//!        READMEM(CPU_OP_LATCHI),                        \
//!        CPU_OP_LATCHI | CPU_OP_READI | CPU_OP_ALU_##x, \
//!        WRITEMEM(CPU_OP_READI), END };                 \
//!    static int CpuOp##x##_zpX[]  = {                   \
//!        ZP_X,                                          \
//!        READMEM(CPU_OP_LATCHI),                        \
//!        CPU_OP_LATCHI | CPU_OP_READI | CPU_OP_ALU_##x, \
//!        WRITEMEM(CPU_OP_READI), END };                 \
//!    static int CpuOp##x##_abs[]  = {                   \
//!        ABS,                                           \
//!        READMEM(CPU_OP_LATCHI),                        \
//!        CPU_OP_LATCHI | CPU_OP_READI | CPU_OP_ALU_##x, \
//!        WRITEMEM(CPU_OP_READI), END };                 \
//!    static int CpuOp##x##_absX[]  = {                  \
//!        ABS_X,                                         \
//!        READMEM(CPU_OP_LATCHI),                        \
//!        CPU_OP_LATCHI | CPU_OP_READI | CPU_OP_ALU_##x, \
//!        WRITEMEM(CPU_OP_READI), END };
//!
//!RMW(ASL);
//!RMW(LSR);
//!RMW(ROL);
//!RMW(ROR);
//!
//!#undef RMW
//!
//!#define CP(x) \
//!    static int CpuOp##x##_imm[]  = {        IFETCH(CPU_OP_LATCH_NOP)  | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_zp[]   = { ZP   , READMEM(CPU_OP_LATCH_NOP) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_zpx[]  = { ZP_X , READMEM(CPU_OP_LATCH_NOP) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_zpy[]  = { ZP_Y , READMEM(CPU_OP_LATCH_NOP) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_abs[]  = { ABS  , READMEM(CPU_OP_LATCH_NOP) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_absX[] = { ABS_X, READMEM(CPU_OP_LATCH_NOP) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_absY[] = { ABS_Y, READMEM(CPU_OP_LATCH_NOP) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_indX[] = { IND_X, READMEM(CPU_OP_LATCH_NOP) | CPU_OP_ALU_##x, END }; \
//!    static int CpuOp##x##_indY[] = { IND_Y, READMEM(CPU_OP_LATCH_NOP) | CPU_OP_ALU_##x, END }; 
//!
//!CP(CMP);
//!CP(CPX);
//!CP(CPY);
//!
//!#undef CP

#define BR(inst,flag,state) \
    static int CpuOp##inst##[] = {                                                                  \
            /* read operand into low byte of EADDR, and set alu_out to our branch test */           \
            /* AND processor flag with branch check */                                              \
            CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR_LO          \
              | CPU_ALU_OP_AND | CPU_ALU_A_REGP | CPU_ALU_B_FLAG_##flag | CPU_CHECK_BRANCH_##state, \
            /* if we get this instruction, the branch will occur, so add operand to PC low byte using the ALU */ \
            /* CPU_LATCH_PC_BRANCH sets up the low byte of EADDR to correctly fix PCH */            \
            CPU_ALU_OP_ADC | CPU_ALU_A_PC_LO | CPU_ALU_B_EADDR_LO | CPU_ALU_C_ZERO | CPU_IBUS_ALU   \
              | CPU_LATCH_PC_BRANCH,                                                                \
            /* CPU_LATCH_PC_BRANCH_* will skip this if not necessary: update PC high byte */        \
            CPU_ALU_OP_ADC | CPU_ALU_A_PC_HI | CPU_ALU_B_EADDR_LO | CPU_ALU_C_ZERO | CPU_IBUS_ALU   \
              | CPU_LATCH_PC_HI,                                                                    \
            OPCODE_FETCH };

BR(BNE,Z,CLEAR);
BR(BEQ,Z,SET);
BR(BVC,V,CLEAR);
BR(BVS,V,SET);
BR(BCC,C,CLEAR);
BR(BCS,C,SET);
BR(BPL,N,CLEAR);
BR(BMI,N,SET);

#undef BR

//static int CpuOpNOP[] = { CPU_OP_NOP, CPU_OP_END };

static int* CpuOpUnimpl = nullptr;
//!static int CpuOpUnimpl[] = { 
//!    CPU_OP_ASSERT 
//!};

static int const* OpTable[256] = {
    /* 0x00 */ CpuOpUnimpl,         /* 0x01 */ CpuOpUnimpl,         /* 0x02 */ CpuOpUnimpl,         /* 0x03 */ CpuOpUnimpl,
    /* 0x04 */ CpuOpUnimpl,         /* 0x05 */ CpuOpUnimpl,         /* 0x06 */ CpuOpUnimpl,         /* 0x07 */ CpuOpUnimpl,
    /* 0x08 */ CpuOpUnimpl,         /* 0x09 */ CpuOpUnimpl,         /* 0x0A */ CpuOpUnimpl,         /* 0x0B */ CpuOpUnimpl,
    /* 0x0C */ CpuOpUnimpl,         /* 0x0D */ CpuOpUnimpl,         /* 0x0E */ CpuOpUnimpl,         /* 0x0F */ CpuOpUnimpl,
    /* 0x10 */ CpuOpBPL,            /* 0x11 */ CpuOpUnimpl,         /* 0x12 */ CpuOpUnimpl,         /* 0x13 */ CpuOpUnimpl,
    /* 0x14 */ CpuOpUnimpl,         /* 0x15 */ CpuOpUnimpl,         /* 0x16 */ CpuOpUnimpl,         /* 0x17 */ CpuOpUnimpl,
    /* 0x18 */ CpuOpCLC,            /* 0x19 */ CpuOpUnimpl,         /* 0x1A */ CpuOpUnimpl,         /* 0x1B */ CpuOpUnimpl,
    /* 0x1C */ CpuOpUnimpl,         /* 0x1D */ CpuOpUnimpl,         /* 0x1E */ CpuOpUnimpl,         /* 0x1F */ CpuOpUnimpl,
    /* 0x20 */ CpuOpUnimpl,         /* 0x21 */ CpuOpUnimpl,         /* 0x22 */ CpuOpUnimpl,         /* 0x23 */ CpuOpUnimpl,
    /* 0x24 */ CpuOpUnimpl,         /* 0x25 */ CpuOpUnimpl,         /* 0x26 */ CpuOpUnimpl,         /* 0x27 */ CpuOpUnimpl,
    /* 0x28 */ CpuOpUnimpl,         /* 0x29 */ CpuOpUnimpl,         /* 0x2A */ CpuOpUnimpl,         /* 0x2B */ CpuOpUnimpl,
    /* 0x2C */ CpuOpUnimpl,         /* 0x2D */ CpuOpUnimpl,         /* 0x2E */ CpuOpUnimpl,         /* 0x2F */ CpuOpUnimpl,
    /* 0x30 */ CpuOpBMI,            /* 0x31 */ CpuOpUnimpl,         /* 0x32 */ CpuOpUnimpl,         /* 0x33 */ CpuOpUnimpl,
    /* 0x34 */ CpuOpUnimpl,         /* 0x35 */ CpuOpUnimpl,         /* 0x36 */ CpuOpUnimpl,         /* 0x37 */ CpuOpUnimpl,
    /* 0x38 */ CpuOpSEC,            /* 0x39 */ CpuOpUnimpl,         /* 0x3A */ CpuOpUnimpl,         /* 0x3B */ CpuOpUnimpl,
    /* 0x3C */ CpuOpUnimpl,         /* 0x3D */ CpuOpUnimpl,         /* 0x3E */ CpuOpUnimpl,         /* 0x3F */ CpuOpUnimpl,
    /* 0x40 */ CpuOpUnimpl,         /* 0x41 */ CpuOpUnimpl,         /* 0x42 */ CpuOpUnimpl,         /* 0x43 */ CpuOpUnimpl,
    /* 0x44 */ CpuOpUnimpl,         /* 0x45 */ CpuOpUnimpl,         /* 0x46 */ CpuOpUnimpl,         /* 0x47 */ CpuOpUnimpl,
    /* 0x48 */ CpuOpUnimpl,         /* 0x49 */ CpuOpUnimpl,         /* 0x4A */ CpuOpUnimpl,         /* 0x4B */ CpuOpUnimpl,
    /* 0x4C */ CpuOpUnimpl,         /* 0x4D */ CpuOpUnimpl,         /* 0x4E */ CpuOpUnimpl,         /* 0x4F */ CpuOpUnimpl,
    /* 0x50 */ CpuOpUnimpl,         /* 0x51 */ CpuOpUnimpl,         /* 0x52 */ CpuOpUnimpl,         /* 0x53 */ CpuOpUnimpl,
    /* 0x54 */ CpuOpUnimpl,         /* 0x55 */ CpuOpUnimpl,         /* 0x56 */ CpuOpUnimpl,         /* 0x57 */ CpuOpUnimpl,
    /* 0x58 */ CpuOpCLI,            /* 0x59 */ CpuOpUnimpl,         /* 0x5A */ CpuOpUnimpl,         /* 0x5B */ CpuOpUnimpl,
    /* 0x5C */ CpuOpUnimpl,         /* 0x5D */ CpuOpUnimpl,         /* 0x5E */ CpuOpUnimpl,         /* 0x5F */ CpuOpUnimpl,
    /* 0x60 */ CpuOpUnimpl,         /* 0x61 */ CpuOpUnimpl,         /* 0x62 */ CpuOpUnimpl,         /* 0x63 */ CpuOpUnimpl,
    /* 0x64 */ CpuOpUnimpl,         /* 0x65 */ CpuOpUnimpl,         /* 0x66 */ CpuOpUnimpl,         /* 0x67 */ CpuOpUnimpl,
    /* 0x68 */ CpuOpUnimpl,         /* 0x69 */ CpuOpUnimpl,         /* 0x6A */ CpuOpUnimpl,         /* 0x6B */ CpuOpUnimpl,
    /* 0x6C */ CpuOpUnimpl,         /* 0x6D */ CpuOpUnimpl,         /* 0x6E */ CpuOpUnimpl,         /* 0x6F */ CpuOpUnimpl,
    /* 0x70 */ CpuOpUnimpl,         /* 0x71 */ CpuOpUnimpl,         /* 0x72 */ CpuOpUnimpl,         /* 0x73 */ CpuOpUnimpl,
    /* 0x74 */ CpuOpUnimpl,         /* 0x75 */ CpuOpUnimpl,         /* 0x76 */ CpuOpUnimpl,         /* 0x77 */ CpuOpUnimpl,
    /* 0x78 */ CpuOpSEI,            /* 0x79 */ CpuOpUnimpl,         /* 0x7A */ CpuOpUnimpl,         /* 0x7B */ CpuOpUnimpl,
    /* 0x7C */ CpuOpUnimpl,         /* 0x7D */ CpuOpUnimpl,         /* 0x7E */ CpuOpUnimpl,         /* 0x7F */ CpuOpUnimpl,
    /* 0x80 */ CpuOpUnimpl,         /* 0x81 */ CpuOpUnimpl,         /* 0x82 */ CpuOpUnimpl,         /* 0x83 */ CpuOpUnimpl,
    /* 0x84 */ CpuOpUnimpl,         /* 0x85 */ CpuOpUnimpl,         /* 0x86 */ CpuOpUnimpl,         /* 0x87 */ CpuOpUnimpl,
    /* 0x88 */ CpuOpUnimpl,         /* 0x89 */ CpuOpUnimpl,         /* 0x8A */ CpuOpTXA,            /* 0x8B */ CpuOpUnimpl,
    /* 0x8C */ CpuOpSTY_abs,        /* 0x8D */ CpuOpSTA_abs,        /* 0x8E */ CpuOpSTX_abs,        /* 0x8F */ CpuOpUnimpl,
    /* 0x90 */ CpuOpBCC,            /* 0x91 */ CpuOpUnimpl,         /* 0x92 */ CpuOpUnimpl,         /* 0x93 */ CpuOpUnimpl,
    /* 0x94 */ CpuOpUnimpl,         /* 0x95 */ CpuOpUnimpl,         /* 0x96 */ CpuOpUnimpl,         /* 0x97 */ CpuOpUnimpl,
    /* 0x98 */ CpuOpTYA,            /* 0x99 */ CpuOpUnimpl,         /* 0x9A */ CpuOpTXS,            /* 0x9B */ CpuOpUnimpl,
    /* 0x9C */ CpuOpUnimpl,         /* 0x9D */ CpuOpUnimpl,         /* 0x9E */ CpuOpUnimpl,         /* 0x9F */ CpuOpUnimpl,
    /* 0xA0 */ CpuOpLDY_imm,        /* 0xA1 */ CpuOpUnimpl,         /* 0xA2 */ CpuOpLDX_imm,        /* 0xA3 */ CpuOpUnimpl,
    /* 0xA4 */ CpuOpUnimpl,         /* 0xA5 */ CpuOpUnimpl,         /* 0xA6 */ CpuOpUnimpl,         /* 0xA7 */ CpuOpUnimpl,
    /* 0xA8 */ CpuOpTAY,            /* 0xA9 */ CpuOpLDA_imm,        /* 0xAA */ CpuOpTAX,            /* 0xAB */ CpuOpUnimpl,
    /* 0xAC */ CpuOpLDY_abs,        /* 0xAD */ CpuOpLDA_abs,        /* 0xAE */ CpuOpLDX_abs,        /* 0xAF */ CpuOpUnimpl,
    /* 0xB0 */ CpuOpBCS,            /* 0xB1 */ CpuOpUnimpl,         /* 0xB2 */ CpuOpUnimpl,         /* 0xB3 */ CpuOpUnimpl,
    /* 0xB4 */ CpuOpUnimpl,         /* 0xB5 */ CpuOpUnimpl,         /* 0xB6 */ CpuOpUnimpl,         /* 0xB7 */ CpuOpUnimpl,
    /* 0xB8 */ CpuOpCLV,            /* 0xB9 */ CpuOpUnimpl,         /* 0xBA */ CpuOpTSX,            /* 0xBB */ CpuOpUnimpl,
    /* 0xBC */ CpuOpUnimpl,         /* 0xBD */ CpuOpUnimpl,         /* 0xBE */ CpuOpUnimpl,         /* 0xBF */ CpuOpUnimpl,
    /* 0xC0 */ CpuOpUnimpl,         /* 0xC1 */ CpuOpUnimpl,         /* 0xC2 */ CpuOpUnimpl,         /* 0xC3 */ CpuOpUnimpl,
    /* 0xC4 */ CpuOpUnimpl,         /* 0xC5 */ CpuOpUnimpl,         /* 0xC6 */ CpuOpUnimpl,         /* 0xC7 */ CpuOpUnimpl,
    /* 0xC8 */ CpuOpUnimpl,         /* 0xC9 */ CpuOpUnimpl,         /* 0xCA */ CpuOpUnimpl,         /* 0xCB */ CpuOpUnimpl,
    /* 0xCC */ CpuOpUnimpl,         /* 0xCD */ CpuOpUnimpl,         /* 0xCE */ CpuOpUnimpl,         /* 0xCF */ CpuOpUnimpl,
    /* 0xD0 */ CpuOpBNE,            /* 0xD1 */ CpuOpUnimpl,         /* 0xD2 */ CpuOpUnimpl,         /* 0xD3 */ CpuOpUnimpl,
    /* 0xD4 */ CpuOpUnimpl,         /* 0xD5 */ CpuOpUnimpl,         /* 0xD6 */ CpuOpUnimpl,         /* 0xD7 */ CpuOpUnimpl,
    /* 0xD8 */ CpuOpCLD,            /* 0xD9 */ CpuOpUnimpl,         /* 0xDA */ CpuOpUnimpl,         /* 0xDB */ CpuOpUnimpl,
    /* 0xDC */ CpuOpUnimpl,         /* 0xDD */ CpuOpUnimpl,         /* 0xDE */ CpuOpUnimpl,         /* 0xDF */ CpuOpUnimpl,
    /* 0xE0 */ CpuOpUnimpl,         /* 0xE1 */ CpuOpUnimpl,         /* 0xE2 */ CpuOpUnimpl,         /* 0xE3 */ CpuOpUnimpl,
    /* 0xE4 */ CpuOpUnimpl,         /* 0xE5 */ CpuOpUnimpl,         /* 0xE6 */ CpuOpUnimpl,         /* 0xE7 */ CpuOpUnimpl,
    /* 0xE8 */ CpuOpUnimpl,         /* 0xE9 */ CpuOpUnimpl,         /* 0xEA */ CpuOpUnimpl,         /* 0xEB */ CpuOpUnimpl,
    /* 0xEC */ CpuOpUnimpl,         /* 0xED */ CpuOpUnimpl,         /* 0xEE */ CpuOpUnimpl,         /* 0xEF */ CpuOpUnimpl,
    /* 0xF0 */ CpuOpBEQ,            /* 0xF1 */ CpuOpUnimpl,         /* 0xF2 */ CpuOpUnimpl,         /* 0xF3 */ CpuOpUnimpl,
    /* 0xF4 */ CpuOpUnimpl,         /* 0xF5 */ CpuOpUnimpl,         /* 0xF6 */ CpuOpUnimpl,         /* 0xF7 */ CpuOpUnimpl,
    /* 0xF8 */ CpuOpSED,            /* 0xF9 */ CpuOpUnimpl,         /* 0xFA */ CpuOpUnimpl,         /* 0xFB */ CpuOpUnimpl,
    /* 0xFC */ CpuOpUnimpl,         /* 0xFD */ CpuOpUnimpl,         /* 0xFE */ CpuOpUnimpl,         /* 0xFF */ CpuOpUnimpl,
};


