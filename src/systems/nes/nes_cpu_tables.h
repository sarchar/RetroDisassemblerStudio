#pragma once

typedef u64 CPU_INST;

#define ON(flag) (1ULL << (flag))
#define OFF(flag) 0ULL

// 2 bits: address bus MUX
#define CPU_ADDRESS_shift     0
#define CPU_ADDRESS_bits      2
#define CPU_ADDRESS_mask      (0x03 << CPU_ADDRESS_shift)
#define CPU_ADDRESS_BUS_PC    (0ULL << CPU_ADDRESS_shift)
#define CPU_ADDRESS_BUS_EADDR (1ULL << CPU_ADDRESS_shift)
#define CPU_ADDRESS_BUS_INTM  (2ULL << CPU_ADDRESS_shift)
#define CPU_ADDRESS_BUS_STACK (3ULL << CPU_ADDRESS_shift)

// 1 bit: R/W. every cycle produces a read or write. the 65C816 has VDA/VPA pins that would let us avoid that
#define CPU_RW_shift (CPU_ADDRESS_shift + CPU_ADDRESS_bits)
#define CPU_RW_bits  1
#define CPU_RW_mask  (1ULL << 2)
#define CPU_READ     OFF(CPU_RW_shift)
#define CPU_WRITE    ON(CPU_RW_shift)

// 1 bit: PC increment
#define CPU_INCPC_shift (CPU_RW_shift + CPU_RW_bits) 
#define CPU_INCPC_bits  1
#define CPU_INCPC_mask  (1ULL << CPU_INCPC_shift) 
#define CPU_INCPC       ON(CPU_INCPC_shift)

// 1 bit: intermediate increment
#define CPU_INCINTM_shift (CPU_INCPC_shift + CPU_INCPC_bits) 
#define CPU_INCINTM_bits  1
#define CPU_INCINTM_mask  (1ULL << CPU_INCINTM_shift) 
#define CPU_INCINTM       ON(CPU_INCINTM_shift)

// 1 bit: stack decrement
#define CPU_DECS_shift (CPU_INCINTM_shift + CPU_INCINTM_bits) 
#define CPU_DECS_bits  1
#define CPU_DECS_mask  (1ULL << CPU_DECS_shift) 
#define CPU_DECS       ON(CPU_DECS_shift)

// 1 bit: stack increment
#define CPU_INCS_shift (CPU_DECS_shift + CPU_DECS_bits) 
#define CPU_INCS_bits  1
#define CPU_INCS_mask  (1ULL << CPU_INCS_shift) 
#define CPU_INCS       ON(CPU_INCS_shift)

// 2 bits: internal bus: ALU or DATA lines
#define CPU_IBUS_shift (CPU_INCS_shift + CPU_INCS_bits)
#define CPU_IBUS_bits  2
#define CPU_IBUS_mask  (3ULL << CPU_IBUS_shift)
#define CPU_IBUS_DATA  (0ULL << CPU_IBUS_shift)
#define CPU_IBUS_ALU   (1ULL << CPU_IBUS_shift)

// 1 bit: latch opcode
#define CPU_LATCH_OPCODE_shift (CPU_IBUS_shift + CPU_IBUS_bits)
#define CPU_LATCH_OPCODE_bits  1
#define CPU_LATCH_OPCODE_mask  (1ULL << CPU_LATCH_OPCODE_shift)
#define CPU_LATCH_OPCODE       ON(CPU_LATCH_OPCODE_shift)

// 1 bit: PC JMP latch
#define CPU_LATCH_PC_JMP_shift (CPU_LATCH_OPCODE_shift + CPU_LATCH_OPCODE_bits)
#define CPU_LATCH_PC_JMP_bits  1
#define CPU_LATCH_PC_JMP_mask  (1ULL << CPU_LATCH_PC_JMP_shift)
#define CPU_LATCH_PC_JMP       ON(CPU_LATCH_PC_JMP_shift)

// 1 bit: PC JMP latch
#define CPU_LATCH_PC_BRANCH_shift (CPU_LATCH_PC_JMP_shift + CPU_LATCH_PC_JMP_bits)
#define CPU_LATCH_PC_BRANCH_bits  1
#define CPU_LATCH_PC_BRANCH_mask  (1ULL << CPU_LATCH_PC_BRANCH_shift)
#define CPU_LATCH_PC_BRANCH       ON(CPU_LATCH_PC_BRANCH_shift)

// 1 bit: PC BRANCH set
#define CPU_CHECK_BRANCH_SET_shift (CPU_LATCH_PC_BRANCH_shift + CPU_LATCH_PC_BRANCH_bits)
#define CPU_CHECK_BRANCH_SET_bits  1
#define CPU_CHECK_BRANCH_SET_mask  (1ULL << CPU_CHECK_BRANCH_SET_shift)
#define CPU_CHECK_BRANCH_SET       ON(CPU_CHECK_BRANCH_SET_shift)

// 1 bit: PC BRANCH clear
#define CPU_CHECK_BRANCH_CLEAR_shift (CPU_CHECK_BRANCH_SET_shift + CPU_CHECK_BRANCH_SET_bits)
#define CPU_CHECK_BRANCH_CLEAR_bits  1
#define CPU_CHECK_BRANCH_CLEAR_mask  (1ULL << CPU_CHECK_BRANCH_CLEAR_shift)
#define CPU_CHECK_BRANCH_CLEAR       ON(CPU_CHECK_BRANCH_CLEAR_shift)

// 1 bit: PC HI
#define CPU_LATCH_PC_HI_shift (CPU_CHECK_BRANCH_CLEAR_shift + CPU_CHECK_BRANCH_CLEAR_bits)
#define CPU_LATCH_PC_HI_bits  1
#define CPU_LATCH_PC_HI_mask  (1ULL << CPU_LATCH_PC_HI_shift)
#define CPU_LATCH_PC_HI       ON(CPU_LATCH_PC_HI_shift)

// 1 bit: EADDR_LO latch
#define CPU_LATCH_EADDR_LO_shift (CPU_LATCH_PC_HI_shift + CPU_LATCH_PC_HI_bits)
#define CPU_LATCH_EADDR_LO_bits  1
#define CPU_LATCH_EADDR_LO_mask  (1ULL << CPU_LATCH_EADDR_LO_shift)
#define CPU_LATCH_EADDR_LO       ON(CPU_LATCH_EADDR_LO_shift)

// 1 bit: EADDR_HI latch
#define CPU_LATCH_EADDR_HI_shift (CPU_LATCH_EADDR_LO_shift + CPU_LATCH_EADDR_LO_bits)
#define CPU_LATCH_EADDR_HI_bits  1
#define CPU_LATCH_EADDR_HI_mask  (1ULL << CPU_LATCH_EADDR_HI_shift)
#define CPU_LATCH_EADDR_HI       ON(CPU_LATCH_EADDR_HI_shift)

// 1 bit: EADDR_HI_EXT latch
#define CPU_LATCH_EADDR_HI_EXT_shift (CPU_LATCH_EADDR_HI_shift + CPU_LATCH_EADDR_HI_bits)
#define CPU_LATCH_EADDR_HI_EXT_bits  1
#define CPU_LATCH_EADDR_HI_EXT_mask  (1ULL << CPU_LATCH_EADDR_HI_EXT_shift)
#define CPU_LATCH_EADDR_HI_EXT       ON(CPU_LATCH_EADDR_HI_EXT_shift)

// 1 bit: EADDR_HI_EXTC latch
#define CPU_LATCH_EADDR_HI_EXTC_shift (CPU_LATCH_EADDR_HI_EXT_shift + CPU_LATCH_EADDR_HI_EXT_bits)
#define CPU_LATCH_EADDR_HI_EXTC_bits  1
#define CPU_LATCH_EADDR_HI_EXTC_mask  (1ULL << CPU_LATCH_EADDR_HI_EXTC_shift)
#define CPU_LATCH_EADDR_HI_EXTC       ON(CPU_LATCH_EADDR_HI_EXTC_shift)

// 1 bit: EADDR latch
#define CPU_LATCH_EADDR_shift (CPU_LATCH_EADDR_HI_EXTC_shift + CPU_LATCH_EADDR_HI_EXTC_bits)
#define CPU_LATCH_EADDR_bits  1
#define CPU_LATCH_EADDR_mask  (1ULL << CPU_LATCH_EADDR_shift)
#define CPU_LATCH_EADDR       ON(CPU_LATCH_EADDR_shift)

// 4 bits: ALU op
#define CPU_ALU_OP_shift  (CPU_LATCH_EADDR_shift + CPU_LATCH_EADDR_bits)
#define CPU_ALU_OP_bits   4
#define CPU_ALU_OP_mask   (0x0FULL << CPU_ALU_OP_shift)
#define CPU_ALU_OP_IDLE   (0ULL << CPU_ALU_OP_shift)   // not an actual state on the cpu, but an optimization for us
#define CPU_ALU_OP_ADC    (1ULL << CPU_ALU_OP_shift)
#define CPU_ALU_OP_SBC    (2ULL << CPU_ALU_OP_shift)
#define CPU_ALU_OP_AND    (3ULL << CPU_ALU_OP_shift)
#define CPU_ALU_OP_OR     (4ULL << CPU_ALU_OP_shift)
#define CPU_ALU_OP_ORA    CPU_ALU_OP_OR                // hackish       
#define CPU_ALU_OP_EOR    (5ULL << CPU_ALU_OP_shift)
#define CPU_ALU_OP_ASL    (6ULL << CPU_ALU_OP_shift)
#define CPU_ALU_OP_LSR    (7ULL << CPU_ALU_OP_shift)
#define CPU_ALU_OP_ROL    (8ULL << CPU_ALU_OP_shift)
#define CPU_ALU_OP_ROR    (9ULL << CPU_ALU_OP_shift)
#define CPU_ALU_OP_CLRBIT (15ULL << CPU_ALU_OP_shift)

// 4 bits: ALU A source
#define CPU_ALU_A_shift    (CPU_ALU_OP_shift + CPU_ALU_OP_bits)
#define CPU_ALU_A_bits     4
#define CPU_ALU_A_mask     (0x0FULL << CPU_ALU_A_shift)
#define CPU_ALU_A_REGA     (0ULL << CPU_ALU_A_shift)
#define CPU_ALU_A_REGX     (1ULL << CPU_ALU_A_shift)
#define CPU_ALU_A_REGY     (2ULL << CPU_ALU_A_shift)
#define CPU_ALU_A_REGS     (3ULL << CPU_ALU_A_shift)
#define CPU_ALU_A_PC_LO    (4ULL << CPU_ALU_A_shift)
#define CPU_ALU_A_PC_HI    (5ULL << CPU_ALU_A_shift)
#define CPU_ALU_A_EADDR_HI (6ULL << CPU_ALU_A_shift)
#define CPU_ALU_A_REGP     (7ULL << CPU_ALU_A_shift)
#define CPU_ALU_A_INTM     (8ULL << CPU_ALU_A_shift)

// 4 bits: ALU B source
#define CPU_ALU_B_shift    (CPU_ALU_A_shift + CPU_ALU_A_bits)
#define CPU_ALU_B_bits     4
#define CPU_ALU_B_mask     (0x0FULL << CPU_ALU_B_shift)
#define CPU_ALU_B_ZERO     (0ULL << CPU_ALU_B_shift)
#define CPU_ALU_B_EADDR_LO (1ULL << CPU_ALU_B_shift)
#define CPU_ALU_B_INTM     (2ULL << CPU_ALU_B_shift)
#define CPU_ALU_B_DATA     (3ULL << CPU_ALU_B_shift)
#define CPU_ALU_B_FLAG_C   (8ULL << CPU_ALU_B_shift)
#define CPU_ALU_B_FLAG_D   (9ULL << CPU_ALU_B_shift)
#define CPU_ALU_B_FLAG_I   (10ULL << CPU_ALU_B_shift)
#define CPU_ALU_B_FLAG_V   (11ULL << CPU_ALU_B_shift)
#define CPU_ALU_B_FLAG_Z   (12ULL << CPU_ALU_B_shift)
#define CPU_ALU_B_FLAG_N   (13ULL << CPU_ALU_B_shift)

// 2 bits: C source
#define CPU_ALU_C_shift    (CPU_ALU_B_shift + CPU_ALU_B_bits)
#define CPU_ALU_C_bits     2
#define CPU_ALU_C_mask     (3ULL << CPU_ALU_C_shift)
#define CPU_ALU_C_DEFAULT  (0ULL << CPU_ALU_C_shift)
#define CPU_ALU_C_ZERO     (1ULL << CPU_ALU_C_shift)
#define CPU_ALU_C_ONE      (2ULL << CPU_ALU_C_shift)

// 1 bit: REGP latch
#define CPU_LATCH_REGP_shift (CPU_ALU_C_shift + CPU_ALU_C_bits)
#define CPU_LATCH_REGP_bits  1
#define CPU_LATCH_REGP_mask  (1ULL << CPU_LATCH_REGP_shift)
#define CPU_LATCH_REGP       ON(CPU_LATCH_REGP_shift)

// 1 bit: REGA latch
#define CPU_LATCH_REGA_shift (CPU_LATCH_REGP_shift + CPU_LATCH_REGP_bits)
#define CPU_LATCH_REGA_bits  1
#define CPU_LATCH_REGA_mask  (1ULL << CPU_LATCH_REGA_shift)
#define CPU_LATCH_REGA       ON(CPU_LATCH_REGA_shift)

// 1 bit: REGX latch
#define CPU_LATCH_REGX_shift (CPU_LATCH_REGA_shift + CPU_LATCH_REGA_bits)
#define CPU_LATCH_REGX_bits  1
#define CPU_LATCH_REGX_mask  (1ULL << CPU_LATCH_REGX_shift)
#define CPU_LATCH_REGX       ON(CPU_LATCH_REGX_shift)

// 1 bit: REGY latch
#define CPU_LATCH_REGY_shift (CPU_LATCH_REGX_shift + CPU_LATCH_REGX_bits)
#define CPU_LATCH_REGY_bits  1
#define CPU_LATCH_REGY_mask  (1ULL << CPU_LATCH_REGY_shift)
#define CPU_LATCH_REGY       ON(CPU_LATCH_REGY_shift)

// 1 bit: REGS latch
#define CPU_LATCH_REGS_shift (CPU_LATCH_REGY_shift + CPU_LATCH_REGY_bits)
#define CPU_LATCH_REGS_bits  1
#define CPU_LATCH_REGS_mask  (1ULL << CPU_LATCH_REGS_shift)
#define CPU_LATCH_REGS       ON(CPU_LATCH_REGS_shift)

// 1 bit: intermediate latch
#define CPU_LATCH_INTM_shift (CPU_LATCH_REGS_shift + CPU_LATCH_REGS_bits)
#define CPU_LATCH_INTM_bits  1
#define CPU_LATCH_INTM_mask  (1ULL << CPU_LATCH_INTM_shift)
#define CPU_LATCH_INTM       ON(CPU_LATCH_INTM_shift)

// 1 bit: set flags when latching intermediate
#define CPU_LATCH_INTM_FLAGS_shift (CPU_LATCH_INTM_shift + CPU_LATCH_INTM_bits)
#define CPU_LATCH_INTM_FLAGS_bits  1
#define CPU_LATCH_INTM_FLAGS_mask  (1ULL << CPU_LATCH_INTM_FLAGS_shift)
#define CPU_LATCH_INTM_FLAGS       ON(CPU_LATCH_INTM_FLAGS_shift)

// 1 bit: set NV flags from BIT operation when latching intermediate
#define CPU_LATCH_INTM_BIT_shift   (CPU_LATCH_INTM_FLAGS_shift + CPU_LATCH_INTM_FLAGS_bits)
#define CPU_LATCH_INTM_BIT_bits    1
#define CPU_LATCH_INTM_BIT_mask    (1ULL << CPU_LATCH_INTM_BIT_shift)
#define CPU_LATCH_INTM_BIT         ON(CPU_LATCH_INTM_BIT_shift)

// 3 bits: data bus
#define CPU_DATA_BUS_shift   (CPU_LATCH_INTM_BIT_shift + CPU_LATCH_INTM_BIT_bits)
#define CPU_DATA_BUS_bits    3
#define CPU_DATA_BUS_mask    (7ULL << CPU_DATA_BUS_shift)
#define CPU_DATA_BUS_REGA    (0ULL << CPU_DATA_BUS_shift)
#define CPU_DATA_BUS_REGX    (1ULL << CPU_DATA_BUS_shift)
#define CPU_DATA_BUS_REGY    (2ULL << CPU_DATA_BUS_shift)
#define CPU_DATA_BUS_REGP    (3ULL << CPU_DATA_BUS_shift)
#define CPU_DATA_BUS_INTM    (4ULL << CPU_DATA_BUS_shift)
#define CPU_DATA_BUS_PC_LO   (5ULL << CPU_DATA_BUS_shift)
#define CPU_DATA_BUS_PC_HI   (6ULL << CPU_DATA_BUS_shift)

// 1 bit: latch carry and overflow flags
#define CPU_LATCH_CV_shift   (CPU_DATA_BUS_shift + CPU_DATA_BUS_bits)
#define CPU_LATCH_CV_bits    1
#define CPU_LATCH_CV_mask    (1ULL << CPU_LATCH_CV_shift)
#define CPU_LATCH_CV         ON(CPU_LATCH_CV_shift)

#define OPCODE_FETCH \
    (CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_OPCODE)

#define READMEM(x) \
    (CPU_ADDRESS_BUS_EADDR | CPU_READ | CPU_IBUS_DATA | (x))

#define WRITEMEM(x) \
    (CPU_ADDRESS_BUS_EADDR | CPU_WRITE | (x))

#define ZP \
    (CPU_ADDRESS_BUS_PC    | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR)

#define ZP_X \
    /* fetch the operand into EADDR */                                               \
    CPU_ADDRESS_BUS_PC    | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR,  \
    /* 8-bit add X to the operand */                                                 \
    CPU_ALU_OP_ADC | CPU_ALU_A_REGX | CPU_ALU_B_EADDR_LO | CPU_ALU_C_ZERO | CPU_IBUS_DATA | CPU_LATCH_EADDR

#define ZP_Y \
    /* fetch the operand into EADDR */                                               \
    CPU_ADDRESS_BUS_PC    | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR,  \
    /* 8-bit add Y to the operand */                                                 \
    CPU_ALU_OP_ADC | CPU_ALU_A_REGY | CPU_ALU_B_EADDR_LO | CPU_ALU_C_ZERO | CPU_IBUS_DATA | CPU_LATCH_EADDR

#define ABS \
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR_LO,  \
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR_HI

#define ABS_X_(c) \
    /* Fetch the low byte of the operand into EADDR_LO */                                       \
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR_LO,             \
    /* A flaw in my design clearly shows. I cannot have the CPU_READ put on the IBUS */         \
    /* at the same time I need the output for the ALU (adding X for indexing). So I have */     \
    /* CPU_LATCH_EADDR_HI_EXT which bypasses the internal bus and also determines if we need */ \
    /* to skip the carry into EADDR_HI in the following instruction */                          \
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC |                 CPU_LATCH_EADDR_HI##c           \
       | CPU_ALU_OP_ADC | CPU_ALU_A_REGX | CPU_ALU_B_EADDR_LO | CPU_ALU_C_ZERO                  \
       | CPU_IBUS_ALU | CPU_LATCH_EADDR_LO,                                                     \
    /* Add 1 to the high byte of EADDR. This instruction is skipped if there's no carry */      \
    CPU_ALU_OP_ADC | CPU_ALU_A_EADDR_HI | CPU_ALU_B_INTM | CPU_ALU_C_ZERO                       \
       | CPU_IBUS_ALU | CPU_LATCH_EADDR_HI

// 4 cycles, sometimes 5
#define ABS_X ABS_X_(_EXTC)
// always 5 cycles
#define ABS_X_SLOW ABS_X_(_EXT)

// See ABS_X for notes
#define ABS_Y_(c) \
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR_LO,             \
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC |                 CPU_LATCH_EADDR_HI##c           \
       | CPU_ALU_OP_ADC | CPU_ALU_A_REGY | CPU_ALU_B_EADDR_LO | CPU_ALU_C_ZERO                  \
       | CPU_IBUS_ALU | CPU_LATCH_EADDR_LO,                                                     \
    CPU_ALU_OP_ADC | CPU_ALU_A_EADDR_HI | CPU_ALU_B_INTM | CPU_ALU_C_ZERO                       \
       | CPU_IBUS_ALU | CPU_LATCH_EADDR_HI

#define ABS_Y ABS_Y_(_EXTC)
#define ABS_Y_SLOW ABS_Y_(_EXT)

#define IND_X \
    /* fetch zeropage operand into intermediate */                                      \
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_INTM,         \
    /* put operand onto read bus, and add X to intermediate */                          \
    CPU_ADDRESS_BUS_INTM                                                                \
        | CPU_ALU_OP_ADC | CPU_ALU_A_REGX | CPU_ALU_B_INTM | CPU_ALU_C_ZERO             \
        | CPU_IBUS_ALU | CPU_LATCH_INTM,                                                \
    /* read word address from (intermediate) with 8-bit wrapping of INTM */             \
    CPU_ADDRESS_BUS_INTM | CPU_READ | CPU_INCINTM | CPU_IBUS_DATA | CPU_LATCH_EADDR_LO, \
    CPU_ADDRESS_BUS_INTM | CPU_READ               | CPU_IBUS_DATA | CPU_LATCH_EADDR_HI

#define IND_Y_(c) \
    /* fetch zeropage operand into intermediate */                                      \
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_INTM,         \
    /* read low byte of word address from (intermediate) with 8-bit wrapping of INTM */ \
    CPU_ADDRESS_BUS_INTM | CPU_READ | CPU_INCINTM | CPU_IBUS_DATA | CPU_LATCH_EADDR_LO, \
    /* read high byte from (intermediate), and simultaneously add Y to the low byte */  \
    /* we use LATCH_EADDER_HI_EXT* here to bypass the internal bus and read from the */ \
    /* data lines directly. _EXTC will skipp the high byte carry if possible */         \
    CPU_ADDRESS_BUS_INTM | CPU_READ                           | CPU_LATCH_EADDR_HI##c   \
        | CPU_ALU_OP_ADC | CPU_ALU_A_REGY | CPU_ALU_B_EADDR_LO | CPU_ALU_C_ZERO         \
        | CPU_IBUS_ALU | CPU_LATCH_EADDR_LO,                                            \
    /* fix up high byte of EADDR, which has its offset in intermediate */               \
    CPU_ADDRESS_BUS_EADDR                                                               \
        | CPU_ALU_OP_ADC | CPU_ALU_A_EADDR_HI | CPU_ALU_B_INTM | CPU_ALU_C_ZERO         \
        | CPU_IBUS_ALU | CPU_LATCH_EADDR_HI

#define IND_Y IND_Y_(_EXTC)
#define IND_Y_SLOW IND_Y_(_EXT)

// Not all of these are used for each load (i.e., LDX doesn't have zp,x)
#define LD(x) \
    static CPU_INST CpuOpLD##x##_imm[]  = {                                                \
        CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_REG##x, OPCODE_FETCH }; \
    static CPU_INST CpuOpLD##x##_zp[]   = { ZP   , READMEM(CPU_LATCH_REG##x), OPCODE_FETCH };             \
    static CPU_INST CpuOpLD##x##_zpx[]  = { ZP_X , READMEM(CPU_LATCH_REG##x), OPCODE_FETCH };             \
    static CPU_INST CpuOpLD##x##_zpy[]  = { ZP_Y , READMEM(CPU_LATCH_REG##x), OPCODE_FETCH };             \
    static CPU_INST CpuOpLD##x##_abs[]  = { ABS  , READMEM(CPU_LATCH_REG##x), OPCODE_FETCH };             \
    static CPU_INST CpuOpLD##x##_absx[] = { ABS_X, READMEM(CPU_LATCH_REG##x), OPCODE_FETCH };             \
    static CPU_INST CpuOpLD##x##_absy[] = { ABS_Y, READMEM(CPU_LATCH_REG##x), OPCODE_FETCH };             \
    static CPU_INST CpuOpLD##x##_indx[] = { IND_X, READMEM(CPU_LATCH_REG##x), OPCODE_FETCH };             \
    static CPU_INST CpuOpLD##x##_indy[] = { IND_Y, READMEM(CPU_LATCH_REG##x), OPCODE_FETCH }; 

LD(A);
LD(X);
LD(Y);

#undef LD

#define ST(x) \
    static CPU_INST CpuOpST##x##_zp[]   = { ZP        , WRITEMEM(CPU_DATA_BUS_REG##x), OPCODE_FETCH }; \
    static CPU_INST CpuOpST##x##_zpx[]  = { ZP_X      , WRITEMEM(CPU_DATA_BUS_REG##x), OPCODE_FETCH }; \
    static CPU_INST CpuOpST##x##_zpy[]  = { ZP_Y      , WRITEMEM(CPU_DATA_BUS_REG##x), OPCODE_FETCH }; \
    static CPU_INST CpuOpST##x##_abs[]  = { ABS       , WRITEMEM(CPU_DATA_BUS_REG##x), OPCODE_FETCH }; \
    static CPU_INST CpuOpST##x##_absx[] = { ABS_X_SLOW, WRITEMEM(CPU_DATA_BUS_REG##x), OPCODE_FETCH }; \
    static CPU_INST CpuOpST##x##_absy[] = { ABS_Y_SLOW, WRITEMEM(CPU_DATA_BUS_REG##x), OPCODE_FETCH }; \
    static CPU_INST CpuOpST##x##_indx[] = { IND_X     , WRITEMEM(CPU_DATA_BUS_REG##x), OPCODE_FETCH }; \
    static CPU_INST CpuOpST##x##_indy[] = { IND_Y_SLOW, WRITEMEM(CPU_DATA_BUS_REG##x), OPCODE_FETCH }; 

ST(A);
ST(X);
ST(Y);

#undef ST
#define T(inst, alu_a, latch) \
    static CPU_INST CpuOp##inst[] = {                          \
        CPU_ALU_OP_OR | CPU_ALU_A_REG##alu_a | CPU_ALU_B_ZERO  \
            | CPU_IBUS_ALU | CPU_LATCH_REG##latch,             \
        OPCODE_FETCH };

T(TAX, A, X);
T(TAY, A, Y);
T(TSX, S, X);
T(TXA, X, A);
T(TXS, X, S);
T(TYA, Y, A);

#undef T
#define FL(x) \
    static CPU_INST CpuOpSE##x[] = {                                                                   \
        CPU_IBUS_ALU | CPU_ALU_OP_OR     | CPU_ALU_A_REGP | CPU_ALU_B_FLAG_##x | CPU_LATCH_REGP,  \
        OPCODE_FETCH };                                                                           \
    static CPU_INST CpuOpCL##x[] = {                                                                   \
        CPU_IBUS_ALU | CPU_ALU_OP_CLRBIT | CPU_ALU_A_REGP | CPU_ALU_B_FLAG_##x | CPU_LATCH_REGP,  \
        OPCODE_FETCH };

FL(C);
FL(D);
FL(I);
FL(V);

#undef FL

#define ST(x) \
    static CPU_INST CpuOpPH##x[] = {                   \
        /* garbage read */                             \
        CPU_ADDRESS_BUS_PC | CPU_READ,                 \
        CPU_ADDRESS_BUS_STACK | CPU_WRITE | CPU_DATA_BUS_REG##x | CPU_DECS, \
        OPCODE_FETCH                                   \
    };                                                 \
    static CPU_INST CpuOpPL##x[] = {                   \
        /* garbage read */                             \
        CPU_ADDRESS_BUS_PC | CPU_READ,                 \
        CPU_ADDRESS_BUS_STACK | CPU_READ | CPU_INCS,   \
        CPU_ADDRESS_BUS_STACK | CPU_READ | CPU_IBUS_DATA | CPU_LATCH_REG##x, \
        OPCODE_FETCH                                   \
    };

ST(A);
ST(P);

#undef ST

#define ID(x) \
    static CPU_INST CpuOpIN##x[] = { \
        CPU_ALU_OP_ADC | CPU_ALU_A_REG##x | CPU_ALU_B_ZERO | CPU_ALU_C_ONE | CPU_IBUS_ALU | CPU_LATCH_REG##x, \
        OPCODE_FETCH }; \
    static CPU_INST CpuOpDE##x[] = { \
        CPU_ALU_OP_SBC | CPU_ALU_A_REG##x | CPU_ALU_B_ZERO | CPU_ALU_C_ZERO | CPU_IBUS_ALU | CPU_LATCH_REG##x, \
        OPCODE_FETCH };

ID(X);
ID(Y);

#undef ID

#define READMEM_ALU \
    CPU_ADDRESS_BUS_EADDR | CPU_READ | CPU_ALU_B_DATA \
        | CPU_IBUS_ALU

#define A(x) \
    static CPU_INST CpuOp##x##_imm[]  = {                       \
        CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC               \
            | CPU_ALU_OP_##x | CPU_ALU_A_REGA | CPU_ALU_B_DATA  \
            | CPU_IBUS_ALU | CPU_LATCH_REGA,                    \
        OPCODE_FETCH }; \
    static CPU_INST CpuOp##x##_zp[]   = { ZP   , READMEM_ALU | CPU_ALU_OP_##x | CPU_ALU_A_REGA | CPU_LATCH_REGA | CPU_LATCH_CV, OPCODE_FETCH }; \
    static CPU_INST CpuOp##x##_zpx[]  = { ZP_X , READMEM_ALU | CPU_ALU_OP_##x | CPU_ALU_A_REGA | CPU_LATCH_REGA | CPU_LATCH_CV, OPCODE_FETCH }; \
    static CPU_INST CpuOp##x##_zpy[]  = { ZP_Y , READMEM_ALU | CPU_ALU_OP_##x | CPU_ALU_A_REGA | CPU_LATCH_REGA | CPU_LATCH_CV, OPCODE_FETCH }; \
    static CPU_INST CpuOp##x##_abs[]  = { ABS  , READMEM_ALU | CPU_ALU_OP_##x | CPU_ALU_A_REGA | CPU_LATCH_REGA | CPU_LATCH_CV, OPCODE_FETCH }; \
    static CPU_INST CpuOp##x##_absx[] = { ABS_X, READMEM_ALU | CPU_ALU_OP_##x | CPU_ALU_A_REGA | CPU_LATCH_REGA | CPU_LATCH_CV, OPCODE_FETCH }; \
    static CPU_INST CpuOp##x##_absy[] = { ABS_Y, READMEM_ALU | CPU_ALU_OP_##x | CPU_ALU_A_REGA | CPU_LATCH_REGA | CPU_LATCH_CV, OPCODE_FETCH }; \
    static CPU_INST CpuOp##x##_indx[] = { IND_X, READMEM_ALU | CPU_ALU_OP_##x | CPU_ALU_A_REGA | CPU_LATCH_REGA | CPU_LATCH_CV, OPCODE_FETCH }; \
    static CPU_INST CpuOp##x##_indy[] = { IND_Y, READMEM_ALU | CPU_ALU_OP_##x | CPU_ALU_A_REGA | CPU_LATCH_REGA | CPU_LATCH_CV, OPCODE_FETCH }; 

A(ADC);
A(SBC);
A(AND);
A(EOR);
A(ORA);

#undef A

#define INCDEC(m,M) \
    static CPU_INST CpuOpINC_##m[]   = {                                           \
        M,                                                                         \
        /* read memory */                                                          \
        CPU_ADDRESS_BUS_EADDR | CPU_READ | CPU_IBUS_DATA | CPU_LATCH_INTM,         \
        /* write back memory, perform operation */                                 \
        CPU_ADDRESS_BUS_EADDR | CPU_WRITE | CPU_DATA_BUS_INTM                      \
            | CPU_ALU_OP_ADC | CPU_ALU_A_INTM | CPU_ALU_B_ZERO | CPU_ALU_C_ONE     \
            | CPU_IBUS_ALU | CPU_LATCH_INTM | CPU_LATCH_INTM_FLAGS,                \
        /* write back memory */                                                    \
        CPU_ADDRESS_BUS_EADDR | CPU_WRITE | CPU_DATA_BUS_INTM,                     \
        OPCODE_FETCH };                                                            \
    static CPU_INST CpuOpDEC_##m[]   = {                                           \
        M,                                                                         \
        /* read memory */                                                          \
        CPU_ADDRESS_BUS_EADDR | CPU_READ | CPU_IBUS_DATA | CPU_LATCH_INTM,         \
        /* write back memory, perform operation */                                 \
        CPU_ADDRESS_BUS_EADDR | CPU_WRITE | CPU_DATA_BUS_INTM                      \
            | CPU_ALU_OP_SBC | CPU_ALU_A_INTM | CPU_ALU_B_ZERO | CPU_ALU_C_ZERO    \
            | CPU_IBUS_ALU | CPU_LATCH_INTM | CPU_LATCH_INTM_FLAGS,                \
        /* write back memory */                                                    \
        CPU_ADDRESS_BUS_EADDR | CPU_WRITE | CPU_DATA_BUS_INTM,                     \
        OPCODE_FETCH };                                                            \

INCDEC(zp,ZP);
INCDEC(zpx,ZP_X);
INCDEC(abs,ABS);
INCDEC(absx,ABS_X_SLOW);

#undef INCDEC

static CPU_INST CpuOpBIT_zp[]   = { 
    ZP, 
    READMEM_ALU | CPU_ALU_OP_AND | CPU_ALU_A_REGA
        | CPU_LATCH_INTM | CPU_LATCH_INTM_BIT,
    OPCODE_FETCH 
}; 

static CPU_INST CpuOpBIT_abs[]  = { 
    ABS, 
    READMEM_ALU | CPU_ALU_OP_AND | CPU_ALU_A_REGA
        | CPU_LATCH_INTM | CPU_LATCH_INTM_BIT,
    OPCODE_FETCH 
};

#undef READMEM_ALU

#define RMW(x) \
    static CPU_INST CpuOp##x##_acc[]  = {                                      \
        CPU_ALU_OP_##x | CPU_ALU_A_REGA | CPU_IBUS_ALU | CPU_LATCH_REGA        \
            | CPU_LATCH_CV,                                                    \
        OPCODE_FETCH };                                                        \
    static CPU_INST CpuOp##x##_zp[]  = {                                       \
        ZP,                                                                    \
        READMEM(CPU_LATCH_INTM),                                               \
        WRITEMEM(CPU_DATA_BUS_INTM)                                            \
            | CPU_ALU_OP_##x | CPU_ALU_A_INTM | CPU_IBUS_ALU | CPU_LATCH_INTM  \
            | CPU_LATCH_INTM_FLAGS | CPU_LATCH_CV,                             \
        WRITEMEM(CPU_DATA_BUS_INTM), OPCODE_FETCH };                           \
    static CPU_INST CpuOp##x##_zpx[]  = {                                      \
        ZP_X,                                                                  \
        READMEM(CPU_LATCH_INTM),                                               \
        WRITEMEM(CPU_DATA_BUS_INTM)                                            \
            | CPU_ALU_OP_##x | CPU_ALU_A_INTM | CPU_IBUS_ALU | CPU_LATCH_INTM  \
            | CPU_LATCH_INTM_FLAGS | CPU_LATCH_CV,                             \
        WRITEMEM(CPU_DATA_BUS_INTM), OPCODE_FETCH };                           \
    static CPU_INST CpuOp##x##_abs[]  = {                                      \
        ABS,                                                                   \
        READMEM(CPU_LATCH_INTM),                                               \
        WRITEMEM(CPU_DATA_BUS_INTM)                                            \
            | CPU_ALU_OP_##x | CPU_ALU_A_INTM | CPU_IBUS_ALU | CPU_LATCH_INTM  \
            | CPU_LATCH_INTM_FLAGS | CPU_LATCH_CV,                             \
        WRITEMEM(CPU_DATA_BUS_INTM), OPCODE_FETCH };                           \
    static CPU_INST CpuOp##x##_absx[]  = {                                     \
        ABS_X,                                                                 \
        READMEM(CPU_LATCH_INTM),                                               \
        WRITEMEM(CPU_DATA_BUS_INTM)                                            \
            | CPU_ALU_OP_##x | CPU_ALU_A_INTM | CPU_IBUS_ALU | CPU_LATCH_INTM  \
            | CPU_LATCH_INTM_FLAGS | CPU_LATCH_CV,                             \
        WRITEMEM(CPU_DATA_BUS_INTM), OPCODE_FETCH };

RMW(ASL);
RMW(LSR);
RMW(ROL);
RMW(ROR);

#undef RMW

// Compare ops get latched into the intermediate so they can set NZ flags
#define CP_OP \
    CPU_ADDRESS_BUS_EADDR | CPU_READ | CPU_ALU_OP_SBC | CPU_ALU_B_DATA | CPU_ALU_C_ZERO \
        | CPU_IBUS_ALU | CPU_LATCH_INTM | CPU_LATCH_INTM_FLAGS

#define CP(inst, x) \
    static CPU_INST CpuOp##inst##_imm[]  = {                                                       \
        CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC                                                  \
            | CPU_ALU_OP_SBC | CPU_ALU_A_REG##x | CPU_ALU_B_DATA | CPU_ALU_C_ZERO | CPU_IBUS_ALU,  \
        OPCODE_FETCH };                                                                            \
    static CPU_INST CpuOp##inst##_zp[]   = { ZP   , CP_OP | CPU_ALU_A_REG##x, OPCODE_FETCH };      \
    static CPU_INST CpuOp##inst##_zpx[]  = { ZP_X , CP_OP | CPU_ALU_A_REG##x, OPCODE_FETCH };      \
    static CPU_INST CpuOp##inst##_zpy[]  = { ZP_Y , CP_OP | CPU_ALU_A_REG##x, OPCODE_FETCH };      \
    static CPU_INST CpuOp##inst##_abs[]  = { ABS  , CP_OP | CPU_ALU_A_REG##x, OPCODE_FETCH };      \
    static CPU_INST CpuOp##inst##_absx[] = { ABS_X, CP_OP | CPU_ALU_A_REG##x, OPCODE_FETCH };      \
    static CPU_INST CpuOp##inst##_absy[] = { ABS_Y, CP_OP | CPU_ALU_A_REG##x, OPCODE_FETCH };      \
    static CPU_INST CpuOp##inst##_indx[] = { IND_X, CP_OP | CPU_ALU_A_REG##x, OPCODE_FETCH };      \
    static CPU_INST CpuOp##inst##_indy[] = { IND_Y, CP_OP | CPU_ALU_A_REG##x, OPCODE_FETCH }; 

CP(CMP, A);
CP(CPX, X);
CP(CPY, Y);

#undef CP

#define BR(inst,flag,state) \
    static CPU_INST CpuOp##inst##[] = {                                                                  \
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

static CPU_INST CpuOpJSR[] = {
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR_LO,
    CPU_ADDRESS_BUS_STACK | CPU_READ, // internal operation, no effect
    CPU_ADDRESS_BUS_STACK | CPU_WRITE | CPU_DATA_BUS_PC_HI | CPU_DECS,
    CPU_ADDRESS_BUS_STACK | CPU_WRITE | CPU_DATA_BUS_PC_LO | CPU_DECS,
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_IBUS_DATA | CPU_LATCH_PC_JMP,
    OPCODE_FETCH,
};

static CPU_INST CpuOpJMP_abs[] = {
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR_LO,
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_IBUS_DATA | CPU_LATCH_PC_JMP,
    OPCODE_FETCH,
};

static CPU_INST CpuOpRTS[] = {
    CPU_ADDRESS_BUS_PC | CPU_READ, // internal operation, no effect
    CPU_ADDRESS_BUS_STACK | CPU_READ | CPU_INCS, // read ignored
    CPU_ADDRESS_BUS_STACK | CPU_READ | CPU_INCS | CPU_IBUS_DATA | CPU_LATCH_EADDR_LO,
    CPU_ADDRESS_BUS_STACK | CPU_READ            | CPU_IBUS_DATA | CPU_LATCH_PC_JMP,
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC,
    OPCODE_FETCH,
};

static CPU_INST CpuOpNOP[] = { 0, OPCODE_FETCH };

static CPU_INST* CpuOpUnimpl = nullptr;
//!static int CpuOpUnimpl[] = { 
//!    CPU_OP_ASSERT 
//!};

static CPU_INST CpuReset[] = {
    // fetch the vector in regs.PC and jump to it
    CPU_ADDRESS_BUS_PC | CPU_READ | CPU_INCPC | CPU_IBUS_DATA | CPU_LATCH_EADDR_LO,
    CPU_ADDRESS_BUS_PC | CPU_READ             | CPU_IBUS_DATA | CPU_LATCH_PC_JMP,
    // fetch the next instruction
    OPCODE_FETCH
};

static CPU_INST const* OpTable[256] = {
    /* 0x00 */ CpuOpUnimpl,         /* 0x01 */ CpuOpORA_indx,       /* 0x02 */ CpuOpUnimpl,         /* 0x03 */ CpuOpUnimpl,
    /* 0x04 */ CpuOpUnimpl,         /* 0x05 */ CpuOpORA_zp,         /* 0x06 */ CpuOpASL_zp,         /* 0x07 */ CpuOpUnimpl,
    /* 0x08 */ CpuOpPHP,            /* 0x09 */ CpuOpORA_imm,        /* 0x0A */ CpuOpASL_acc,        /* 0x0B */ CpuOpUnimpl,
    /* 0x0C */ CpuOpUnimpl,         /* 0x0D */ CpuOpORA_abs,        /* 0x0E */ CpuOpASL_abs,        /* 0x0F */ CpuOpUnimpl,
    /* 0x10 */ CpuOpBPL,            /* 0x11 */ CpuOpORA_indy,       /* 0x12 */ CpuOpUnimpl,         /* 0x13 */ CpuOpUnimpl,
    /* 0x14 */ CpuOpUnimpl,         /* 0x15 */ CpuOpORA_zpx,        /* 0x16 */ CpuOpASL_zpx,        /* 0x17 */ CpuOpUnimpl,
    /* 0x18 */ CpuOpCLC,            /* 0x19 */ CpuOpORA_absy,       /* 0x1A */ CpuOpUnimpl,         /* 0x1B */ CpuOpUnimpl,
    /* 0x1C */ CpuOpUnimpl,         /* 0x1D */ CpuOpORA_absx,       /* 0x1E */ CpuOpASL_absx,       /* 0x1F */ CpuOpUnimpl,
    /* 0x20 */ CpuOpJSR,            /* 0x21 */ CpuOpAND_indx,       /* 0x22 */ CpuOpUnimpl,         /* 0x23 */ CpuOpUnimpl,
    /* 0x24 */ CpuOpBIT_zp,         /* 0x25 */ CpuOpAND_zp,         /* 0x26 */ CpuOpROL_zp,         /* 0x27 */ CpuOpUnimpl,
    /* 0x28 */ CpuOpPLP,            /* 0x29 */ CpuOpAND_imm,        /* 0x2A */ CpuOpROL_acc,        /* 0x2B */ CpuOpUnimpl,
    /* 0x2C */ CpuOpBIT_abs,        /* 0x2D */ CpuOpAND_abs,        /* 0x2E */ CpuOpROL_abs,        /* 0x2F */ CpuOpUnimpl,
    /* 0x30 */ CpuOpBMI,            /* 0x31 */ CpuOpAND_indy,       /* 0x32 */ CpuOpUnimpl,         /* 0x33 */ CpuOpUnimpl,
    /* 0x34 */ CpuOpUnimpl,         /* 0x35 */ CpuOpAND_zpx,        /* 0x36 */ CpuOpROL_zpx,        /* 0x37 */ CpuOpUnimpl,
    /* 0x38 */ CpuOpSEC,            /* 0x39 */ CpuOpAND_absy,       /* 0x3A */ CpuOpUnimpl,         /* 0x3B */ CpuOpUnimpl,
    /* 0x3C */ CpuOpUnimpl,         /* 0x3D */ CpuOpAND_absx,       /* 0x3E */ CpuOpROL_absx,       /* 0x3F */ CpuOpUnimpl,
    /* 0x40 */ CpuOpUnimpl,         /* 0x41 */ CpuOpEOR_indx,       /* 0x42 */ CpuOpUnimpl,         /* 0x43 */ CpuOpUnimpl,
    /* 0x44 */ CpuOpUnimpl,         /* 0x45 */ CpuOpEOR_zp,         /* 0x46 */ CpuOpLSR_zp,         /* 0x47 */ CpuOpUnimpl,
    /* 0x48 */ CpuOpPHA,            /* 0x49 */ CpuOpEOR_imm,        /* 0x4A */ CpuOpLSR_acc,        /* 0x4B */ CpuOpUnimpl,
    /* 0x4C */ CpuOpJMP_abs,        /* 0x4D */ CpuOpEOR_abs,        /* 0x4E */ CpuOpLSR_abs,        /* 0x4F */ CpuOpUnimpl,
    /* 0x50 */ CpuOpUnimpl,         /* 0x51 */ CpuOpEOR_indy,       /* 0x52 */ CpuOpUnimpl,         /* 0x53 */ CpuOpUnimpl,
    /* 0x54 */ CpuOpUnimpl,         /* 0x55 */ CpuOpEOR_zpx,        /* 0x56 */ CpuOpLSR_zpx,        /* 0x57 */ CpuOpUnimpl,
    /* 0x58 */ CpuOpCLI,            /* 0x59 */ CpuOpEOR_absy,       /* 0x5A */ CpuOpUnimpl,         /* 0x5B */ CpuOpUnimpl,
    /* 0x5C */ CpuOpUnimpl,         /* 0x5D */ CpuOpEOR_absx,       /* 0x5E */ CpuOpLSR_absx,       /* 0x5F */ CpuOpUnimpl,
    /* 0x60 */ CpuOpRTS,            /* 0x61 */ CpuOpADC_indx,       /* 0x62 */ CpuOpUnimpl,         /* 0x63 */ CpuOpUnimpl,
    /* 0x64 */ CpuOpUnimpl,         /* 0x65 */ CpuOpADC_zp,         /* 0x66 */ CpuOpROR_zp,         /* 0x67 */ CpuOpUnimpl,
    /* 0x68 */ CpuOpPLA,            /* 0x69 */ CpuOpADC_imm,        /* 0x6A */ CpuOpROR_acc,        /* 0x6B */ CpuOpUnimpl,
    /* 0x6C */ CpuOpUnimpl,         /* 0x6D */ CpuOpADC_abs,        /* 0x6E */ CpuOpROR_abs,        /* 0x6F */ CpuOpUnimpl,
    /* 0x70 */ CpuOpUnimpl,         /* 0x71 */ CpuOpADC_indy,       /* 0x72 */ CpuOpUnimpl,         /* 0x73 */ CpuOpUnimpl,
    /* 0x74 */ CpuOpUnimpl,         /* 0x75 */ CpuOpADC_zpx,        /* 0x76 */ CpuOpROR_zpx,        /* 0x77 */ CpuOpUnimpl,
    /* 0x78 */ CpuOpSEI,            /* 0x79 */ CpuOpADC_absy,       /* 0x7A */ CpuOpUnimpl,         /* 0x7B */ CpuOpUnimpl,
    /* 0x7C */ CpuOpUnimpl,         /* 0x7D */ CpuOpADC_absx,       /* 0x7E */ CpuOpROR_absx,       /* 0x7F */ CpuOpUnimpl,
    /* 0x80 */ CpuOpUnimpl,         /* 0x81 */ CpuOpSTA_indx,       /* 0x82 */ CpuOpUnimpl,         /* 0x83 */ CpuOpUnimpl,
    /* 0x84 */ CpuOpSTY_zp,         /* 0x85 */ CpuOpSTA_zp,         /* 0x86 */ CpuOpSTX_zp,         /* 0x87 */ CpuOpUnimpl,
    /* 0x88 */ CpuOpDEY,            /* 0x89 */ CpuOpUnimpl,         /* 0x8A */ CpuOpTXA,            /* 0x8B */ CpuOpUnimpl,
    /* 0x8C */ CpuOpSTY_abs,        /* 0x8D */ CpuOpSTA_abs,        /* 0x8E */ CpuOpSTX_abs,        /* 0x8F */ CpuOpUnimpl,
    /* 0x90 */ CpuOpBCC,            /* 0x91 */ CpuOpSTA_indy,       /* 0x92 */ CpuOpUnimpl,         /* 0x93 */ CpuOpUnimpl,
    /* 0x94 */ CpuOpSTY_zpx,        /* 0x95 */ CpuOpSTA_zpx,        /* 0x96 */ CpuOpSTX_zpy,        /* 0x97 */ CpuOpUnimpl,
    /* 0x98 */ CpuOpTYA,            /* 0x99 */ CpuOpSTA_absy,       /* 0x9A */ CpuOpTXS,            /* 0x9B */ CpuOpUnimpl,
    /* 0x9C */ CpuOpUnimpl,         /* 0x9D */ CpuOpSTA_absx,       /* 0x9E */ CpuOpUnimpl,         /* 0x9F */ CpuOpUnimpl,
    /* 0xA0 */ CpuOpLDY_imm,        /* 0xA1 */ CpuOpLDA_indx,       /* 0xA2 */ CpuOpLDX_imm,        /* 0xA3 */ CpuOpUnimpl,
    /* 0xA4 */ CpuOpLDY_zp,         /* 0xA5 */ CpuOpLDA_zp,         /* 0xA6 */ CpuOpLDX_zp,         /* 0xA7 */ CpuOpUnimpl,
    /* 0xA8 */ CpuOpTAY,            /* 0xA9 */ CpuOpLDA_imm,        /* 0xAA */ CpuOpTAX,            /* 0xAB */ CpuOpUnimpl,
    /* 0xAC */ CpuOpLDY_abs,        /* 0xAD */ CpuOpLDA_abs,        /* 0xAE */ CpuOpLDX_abs,        /* 0xAF */ CpuOpUnimpl,
    /* 0xB0 */ CpuOpBCS,            /* 0xB1 */ CpuOpLDA_indy,       /* 0xB2 */ CpuOpUnimpl,         /* 0xB3 */ CpuOpUnimpl,
    /* 0xB4 */ CpuOpLDY_zpx,        /* 0xB5 */ CpuOpLDA_zpx,        /* 0xB6 */ CpuOpLDX_zpy,        /* 0xB7 */ CpuOpUnimpl,
    /* 0xB8 */ CpuOpCLV,            /* 0xB9 */ CpuOpLDA_absy,       /* 0xBA */ CpuOpTSX,            /* 0xBB */ CpuOpUnimpl,
    /* 0xBC */ CpuOpLDY_absx,       /* 0xBD */ CpuOpLDA_absx,       /* 0xBE */ CpuOpLDX_absy,       /* 0xBF */ CpuOpUnimpl,
    /* 0xC0 */ CpuOpCPY_imm,        /* 0xC1 */ CpuOpCMP_indx,       /* 0xC2 */ CpuOpUnimpl,         /* 0xC3 */ CpuOpUnimpl,
    /* 0xC4 */ CpuOpCPY_zp,         /* 0xC5 */ CpuOpCMP_zp,         /* 0xC6 */ CpuOpDEC_zp,         /* 0xC7 */ CpuOpUnimpl,
    /* 0xC8 */ CpuOpINY,            /* 0xC9 */ CpuOpCMP_imm,        /* 0xCA */ CpuOpDEX,            /* 0xCB */ CpuOpUnimpl,
    /* 0xCC */ CpuOpCPY_abs,        /* 0xCD */ CpuOpCMP_abs,        /* 0xCE */ CpuOpDEC_abs,        /* 0xCF */ CpuOpUnimpl,
    /* 0xD0 */ CpuOpBNE,            /* 0xD1 */ CpuOpCMP_indy,       /* 0xD2 */ CpuOpUnimpl,         /* 0xD3 */ CpuOpUnimpl,
    /* 0xD4 */ CpuOpUnimpl,         /* 0xD5 */ CpuOpCMP_zpx,        /* 0xD6 */ CpuOpDEC_zpx,        /* 0xD7 */ CpuOpUnimpl,
    /* 0xD8 */ CpuOpCLD,            /* 0xD9 */ CpuOpCMP_absy,       /* 0xDA */ CpuOpUnimpl,         /* 0xDB */ CpuOpUnimpl,
    /* 0xDC */ CpuOpUnimpl,         /* 0xDD */ CpuOpCMP_absx,       /* 0xDE */ CpuOpDEC_absx,       /* 0xDF */ CpuOpUnimpl,
    /* 0xE0 */ CpuOpCPX_imm,        /* 0xE1 */ CpuOpSBC_indx,       /* 0xE2 */ CpuOpUnimpl,         /* 0xE3 */ CpuOpUnimpl,
    /* 0xE4 */ CpuOpCPX_zp,         /* 0xE5 */ CpuOpSBC_zp,         /* 0xE6 */ CpuOpINC_zp,         /* 0xE7 */ CpuOpUnimpl,
    /* 0xE8 */ CpuOpINX,            /* 0xE9 */ CpuOpSBC_imm,        /* 0xEA */ CpuOpNOP,            /* 0xEB */ CpuOpUnimpl,
    /* 0xEC */ CpuOpCPX_abs,        /* 0xED */ CpuOpSBC_abs,        /* 0xEE */ CpuOpINC_abs,        /* 0xEF */ CpuOpUnimpl,
    /* 0xF0 */ CpuOpBEQ,            /* 0xF1 */ CpuOpSBC_indy,       /* 0xF2 */ CpuOpUnimpl,         /* 0xF3 */ CpuOpUnimpl,
    /* 0xF4 */ CpuOpUnimpl,         /* 0xF5 */ CpuOpSBC_zpx,        /* 0xF6 */ CpuOpINC_zpx,        /* 0xF7 */ CpuOpUnimpl,
    /* 0xF8 */ CpuOpSED,            /* 0xF9 */ CpuOpSBC_absy,       /* 0xFA */ CpuOpUnimpl,         /* 0xFB */ CpuOpUnimpl,
    /* 0xFC */ CpuOpUnimpl,         /* 0xFD */ CpuOpSBC_absx,       /* 0xFE */ CpuOpINC_absx,       /* 0xFF */ CpuOpUnimpl,
};


