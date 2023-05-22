#pragma once

#define CPU_FLAG_C (1 << 0)
#define CPU_FLAG_Z (1 << 1)
#define CPU_FLAG_I (1 << 2)
#define CPU_FLAG_D (1 << 3)
#define CPU_FLAG_B (1 << 4)
#define CPU_FLAG_V (1 << 6)
#define CPU_FLAG_N (1 << 7)

#define CPU_OP_ASSERT    0x00
#define CPU_OP_NOP       0x01
#define CPU_OP_IFETCH    0x02
#define CPU_OP_READV     0x03
#define CPU_OP_READMEM   0x04
#define CPU_OP_READMEM2  0x05
#define CPU_OP_SETF      0x06
#define CPU_OP_CLEARF    0x07
#define CPU_OP_READA     0x08
#define CPU_OP_READX     0x09
#define CPU_OP_READY     0x0A
#define CPU_OP_READS     0x0B
#define CPU_OP_READP     0x0C
#define CPU_OP_READI     0x0D
#define CPU_OP_INDEXED_X 0x0E
#define CPU_OP_INDEXED_Y 0x0F
#define CPU_OP_CARRYADDR 0x10
#define CPU_OP_STACKR    0x11
#define CPU_OP_GET_SOURCE(x) ((x) & 0xFF)

#define CPU_OP_GET_VECTOR(x) (((x) >> 24) & 0xFFFF)

#define CPU_OP_WRITE_NOP     (0x00 << 8)
#define CPU_OP_WRITEPC_LO    (0x01 << 8)
#define CPU_OP_WRITEPC_HI    (0x02 << 8)
#define CPU_OP_WRITEADDR     (0x03 << 8)
#define CPU_OP_WRITEADDR_LO  (0x04 << 8)
#define CPU_OP_WRITEADDR_HI  (0x05 << 8)
#define CPU_OP_WRITEADDR2    (0x06 << 8)
#define CPU_OP_WRITEADDR2_LO (0x07 << 8)
#define CPU_OP_WRITEADDR2_HI (0x08 << 8)
#define CPU_OP_DECODE        (0x09 << 8)
#define CPU_OP_WRITEP        (0x0A << 8)
#define CPU_OP_WRITEA        (0x0B << 8)
#define CPU_OP_WRITEX        (0x0C << 8)
#define CPU_OP_WRITEY        (0x0D << 8)
#define CPU_OP_WRITES        (0x0E << 8)
#define CPU_OP_WRITEI        (0x0F << 8)
#define CPU_OP_WRITEMEM      (0x10 << 8)
#define CPU_OP_STACKW        (0x11 << 8)
#define CPU_OP_GET_WRITE(x) ((x) & 0xFF00)

#define CPU_OP_ALU_NOP       (0x00 << 16)
#define CPU_OP_ALU_INC       (0x01 << 16)
#define CPU_OP_ALU_DEC       (0x02 << 16)
#define CPU_OP_ALU_ADC       (0x03 << 16)
#define CPU_OP_ALU_SBC       (0x04 << 16)
#define CPU_OP_ALU_AND       (0x05 << 16)
#define CPU_OP_ALU_EOR       (0x06 << 16)
#define CPU_OP_ALU_ORA       (0x07 << 16)
#define CPU_OP_ALU_ASL       (0x08 << 16)
#define CPU_OP_ALU_LSR       (0x09 << 16)
#define CPU_OP_ALU_ROL       (0x0A << 16)
#define CPU_OP_ALU_ROR       (0x0B << 16)
#define CPU_OP_ALU_CMP       (0x0C << 16)
#define CPU_OP_ALU_CPX       (0x0D << 16)
#define CPU_OP_ALU_CPY       (0x0E << 16)
#define CPU_OP_GET_ALU(x)    ((x) & 0xFF0000)

// helpers/shorthands
#define CPU_OP_READV_(x)    (               CPU_OP_READV | (x) << 24)
#define CPU_OP_CLEARF_(x)   (CPU_OP_WRITEP | CPU_OP_CLEARF | (x) << 24)
#define CPU_OP_SETF_(x)     (CPU_OP_WRITEP | CPU_OP_SETF | (x) << 24)
#define CPU_OP_GET_FLAG(x)  (((x) >> 24) & 0xFF)
#define CPU_OP_END          (CPU_OP_DECODE | CPU_OP_IFETCH)

static int CpuReset[] = {
    CPU_OP_NOP, CPU_OP_NOP, CPU_OP_NOP, CPU_OP_NOP, CPU_OP_NOP, CPU_OP_NOP, CPU_OP_NOP, // 7 cycles for reset
    CPU_OP_WRITEPC_LO | CPU_OP_READV_(0xFFFC),
    CPU_OP_WRITEPC_HI | CPU_OP_READV_(0xFFFD),
    CPU_OP_DECODE | CPU_OP_IFETCH
};

#define END CPU_OP_END

#define IFETCH(x) \
    (x) | CPU_OP_IFETCH

#define READMEM(x) \
    (x) | CPU_OP_READMEM

#define WRITEMEM(x) \
    CPU_OP_WRITEMEM | (x)

#define ZP \
    CPU_OP_WRITEADDR | CPU_OP_IFETCH

#define ZP_X \
    CPU_OP_WRITEADDR | CPU_OP_IFETCH,    \
    CPU_OP_WRITEADDR | CPU_OP_INDEXED_X

#define ZP_Y \
    CPU_OP_WRITEADDR | CPU_OP_IFETCH,    \
    CPU_OP_WRITEADDR | CPU_OP_INDEXED_Y

#define ABS \
    CPU_OP_WRITEADDR_LO | CPU_OP_IFETCH,  \
    CPU_OP_WRITEADDR_HI | CPU_OP_IFETCH

#define ABS_X \
    CPU_OP_WRITEADDR_LO | CPU_OP_IFETCH,    \
    CPU_OP_WRITEADDR_HI | CPU_OP_IFETCH,    \
    CPU_OP_WRITEADDR_LO | CPU_OP_INDEXED_X, \
    CPU_OP_WRITEADDR_HI | CPU_OP_CARRYADDR

#define ABS_Y \
    CPU_OP_WRITEADDR_LO | CPU_OP_IFETCH,    \
    CPU_OP_WRITEADDR_HI | CPU_OP_IFETCH,    \
    CPU_OP_WRITEADDR_LO | CPU_OP_INDEXED_Y, \
    CPU_OP_WRITEADDR_HI | CPU_OP_CARRYADDR

#define IND_X \
    CPU_OP_WRITEADDR     | CPU_OP_IFETCH,    /* read zp */ \
    CPU_OP_WRITEADDR2    | CPU_OP_INDEXED_X, /* add X, no carryaddr */ \
    CPU_OP_WRITEADDR_LO  | CPU_OP_READMEM2,  /* read word from result addr */ \
    CPU_OP_WRITEADDR_HI  | CPU_OP_READMEM2   /* . */

#define IND_Y \
    CPU_OP_WRITEADDR2   | CPU_OP_IFETCH,     /* read zp */ \
    CPU_OP_WRITEADDR_LO | CPU_OP_READMEM2,   /* read word from zp addr */ \
    CPU_OP_WRITEADDR_HI | CPU_OP_READMEM2,   /* . */ \
    CPU_OP_WRITEADDR    | CPU_OP_INDEXED_Y,  /* add Y */ \
    CPU_OP_WRITEADDR_HI | CPU_OP_CARRYADDR   /* cross page boundary */ \

// Not all of these are used for each load (i.e., LDX doesn't have zp,x)
#define LD(inst, x) \
    static int CpuOp##inst##_imm[]  = {        IFETCH(x) , END }; \
    static int CpuOp##inst##_zp[]   = { ZP   , READMEM(x), END }; \
    static int CpuOp##inst##_zpx[]  = { ZP_X , READMEM(x), END }; \
    static int CpuOp##inst##_zpy[]  = { ZP_Y , READMEM(x), END }; \
    static int CpuOp##inst##_abs[]  = { ABS  , READMEM(x), END }; \
    static int CpuOp##inst##_absX[] = { ABS_X, READMEM(x), END }; \
    static int CpuOp##inst##_absY[] = { ABS_Y, READMEM(x), END }; \
    static int CpuOp##inst##_indX[] = { IND_X, READMEM(x), END }; \
    static int CpuOp##inst##_indY[] = { IND_Y, READMEM(x), END }; 

LD(LDA, CPU_OP_WRITEA);
LD(LDX, CPU_OP_WRITEX);
LD(LDY, CPU_OP_WRITEY);

#undef LD
#define ST(inst, x) \
    static int CpuOp##inst##_imm[]  = {        IFETCH(x) , END }; \
    static int CpuOp##inst##_zp[]   = { ZP   , WRITEMEM(x), END }; \
    static int CpuOp##inst##_zpx[]  = { ZP_X , WRITEMEM(x), END }; \
    static int CpuOp##inst##_zpy[]  = { ZP_Y , WRITEMEM(x), END }; \
    static int CpuOp##inst##_abs[]  = { ABS  , WRITEMEM(x), END }; \
    static int CpuOp##inst##_absX[] = { ABS_X, WRITEMEM(x), END }; \
    static int CpuOp##inst##_absY[] = { ABS_Y, WRITEMEM(x), END }; \
    static int CpuOp##inst##_indX[] = { IND_X, WRITEMEM(x), END }; \
    static int CpuOp##inst##_indY[] = { IND_Y, WRITEMEM(x), END }; 

ST(STA, CPU_OP_READA);
ST(STX, CPU_OP_READX);
ST(STY, CPU_OP_READY);

#undef ST
#define T(inst, write, read) \
    static int CpuOp##inst[] = { write | read, END };

T(TAX, CPU_OP_WRITEX, CPU_OP_READA);
T(TAY, CPU_OP_WRITEY, CPU_OP_READA);
T(TSX, CPU_OP_WRITEX, CPU_OP_READS);
T(TXA, CPU_OP_WRITEA, CPU_OP_READX);
T(TXS, CPU_OP_WRITES, CPU_OP_READX);
T(TYA, CPU_OP_WRITEA, CPU_OP_READY);

#undef T
#define FL(x) \
    static int CpuOpSE##x[] = { CPU_OP_SETF_(CPU_FLAG_##x), END }; \
    static int CpuOpCL##x[] = { CPU_OP_CLEARF_(CPU_FLAG_##x), END };

FL(C);
FL(D);
FL(I);
FL(V);

#undef FL

#define ST(x) \
    static int CpyOpPH##x[] = {                        \
        CPU_OP_STACKW | CPU_OP_READ##x,                \
        CPU_OP_WRITES | CPU_OP_READS | CPU_OP_ALU_DEC, \
        END                                            \
    };                                                 \
    static int CpyOpPL##x[] = {                        \
        CPU_OP_WRITES | CPU_OP_READS | CPU_OP_ALU_INC, \
        CPU_OP_WRITE##x | CPU_OP_STACKR,               \
        END                                            \
    };

ST(A);
ST(P);

#undef ST

#define ID(inst, x) \
    static int CpuOpIN##inst[] = { CPU_OP_WRITE##x | CPU_OP_READ##x | CPU_OP_ALU_INC, END }; \
    static int CpuOpDE##inst[] = { CPU_OP_WRITE##x | CPU_OP_READ##x | CPU_OP_ALU_DEC, END };

ID(C, A);
ID(X, X);
ID(Y, Y);

#undef ID

#define A(x) \
    static int CpuOp##x##_imm[]  = {        IFETCH(CPU_OP_WRITEA)  | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_zp[]   = { ZP   , READMEM(CPU_OP_WRITEA) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_zpx[]  = { ZP_X , READMEM(CPU_OP_WRITEA) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_zpy[]  = { ZP_Y , READMEM(CPU_OP_WRITEA) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_abs[]  = { ABS  , READMEM(CPU_OP_WRITEA) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_absX[] = { ABS_X, READMEM(CPU_OP_WRITEA) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_absY[] = { ABS_Y, READMEM(CPU_OP_WRITEA) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_indX[] = { IND_X, READMEM(CPU_OP_WRITEA) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_indY[] = { IND_Y, READMEM(CPU_OP_WRITEA) | CPU_OP_ALU_##x, END }; 

A(ADC);
A(SBC);
A(AND);
A(EOR);
A(ORA);

#undef A

#define RMW(x) \
    static int CpuOp##x##_acc[]  = {                   \
        CPU_OP_WRITEA | CPU_OP_READA | CPU_OP_ALU_##x, \
        END };                                         \
    static int CpuOp##x##_zp[]  = {                    \
        ZP,                                            \
        READMEM(CPU_OP_WRITEI),                        \
        CPU_OP_WRITEI | CPU_OP_READI | CPU_OP_ALU_##x, \
        WRITEMEM(CPU_OP_READI), END };                 \
    static int CpuOp##x##_zpX[]  = {                   \
        ZP_X,                                          \
        READMEM(CPU_OP_WRITEI),                        \
        CPU_OP_WRITEI | CPU_OP_READI | CPU_OP_ALU_##x, \
        WRITEMEM(CPU_OP_READI), END };                 \
    static int CpuOp##x##_abs[]  = {                   \
        ABS,                                           \
        READMEM(CPU_OP_WRITEI),                        \
        CPU_OP_WRITEI | CPU_OP_READI | CPU_OP_ALU_##x, \
        WRITEMEM(CPU_OP_READI), END };                 \
    static int CpuOp##x##_absX[]  = {                  \
        ABS_X,                                         \
        READMEM(CPU_OP_WRITEI),                        \
        CPU_OP_WRITEI | CPU_OP_READI | CPU_OP_ALU_##x, \
        WRITEMEM(CPU_OP_READI), END };

RMW(ASL);
RMW(LSR);
RMW(ROL);
RMW(ROR);

#undef RMW

#define CP(x) \
    static int CpuOp##x##_imm[]  = {        IFETCH(CPU_OP_WRITE_NOP)  | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_zp[]   = { ZP   , READMEM(CPU_OP_WRITE_NOP) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_zpx[]  = { ZP_X , READMEM(CPU_OP_WRITE_NOP) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_zpy[]  = { ZP_Y , READMEM(CPU_OP_WRITE_NOP) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_abs[]  = { ABS  , READMEM(CPU_OP_WRITE_NOP) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_absX[] = { ABS_X, READMEM(CPU_OP_WRITE_NOP) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_absY[] = { ABS_Y, READMEM(CPU_OP_WRITE_NOP) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_indX[] = { IND_X, READMEM(CPU_OP_WRITE_NOP) | CPU_OP_ALU_##x, END }; \
    static int CpuOp##x##_indY[] = { IND_Y, READMEM(CPU_OP_WRITE_NOP) | CPU_OP_ALU_##x, END }; 

CP(CMP);
CP(CPX);
CP(CPY);

#undef CP

static int CpuOpNOP[] = { CPU_OP_NOP, CPU_OP_END };

static int CpuOpUnimpl[] = { CPU_OP_ASSERT };

static int const* OpTable[256] = {
    /* 0x00 */ CpuOpUnimpl,         /* 0x01 */ CpuOpUnimpl,         /* 0x02 */ CpuOpUnimpl,         /* 0x03 */ CpuOpUnimpl,
    /* 0x04 */ CpuOpUnimpl,         /* 0x05 */ CpuOpUnimpl,         /* 0x06 */ CpuOpUnimpl,         /* 0x07 */ CpuOpUnimpl,
    /* 0x08 */ CpuOpUnimpl,         /* 0x09 */ CpuOpUnimpl,         /* 0x0A */ CpuOpUnimpl,         /* 0x0B */ CpuOpUnimpl,
    /* 0x0C */ CpuOpUnimpl,         /* 0x0D */ CpuOpUnimpl,         /* 0x0E */ CpuOpUnimpl,         /* 0x0F */ CpuOpUnimpl,
    /* 0x10 */ CpuOpUnimpl,         /* 0x11 */ CpuOpUnimpl,         /* 0x12 */ CpuOpUnimpl,         /* 0x13 */ CpuOpUnimpl,
    /* 0x14 */ CpuOpUnimpl,         /* 0x15 */ CpuOpUnimpl,         /* 0x16 */ CpuOpUnimpl,         /* 0x17 */ CpuOpUnimpl,
    /* 0x18 */ CpuOpUnimpl,         /* 0x19 */ CpuOpUnimpl,         /* 0x1A */ CpuOpUnimpl,         /* 0x1B */ CpuOpUnimpl,
    /* 0x1C */ CpuOpUnimpl,         /* 0x1D */ CpuOpUnimpl,         /* 0x1E */ CpuOpUnimpl,         /* 0x1F */ CpuOpUnimpl,
    /* 0x20 */ CpuOpUnimpl,         /* 0x21 */ CpuOpUnimpl,         /* 0x22 */ CpuOpUnimpl,         /* 0x23 */ CpuOpUnimpl,
    /* 0x24 */ CpuOpUnimpl,         /* 0x25 */ CpuOpUnimpl,         /* 0x26 */ CpuOpUnimpl,         /* 0x27 */ CpuOpUnimpl,
    /* 0x28 */ CpuOpUnimpl,         /* 0x29 */ CpuOpUnimpl,         /* 0x2A */ CpuOpUnimpl,         /* 0x2B */ CpuOpUnimpl,
    /* 0x2C */ CpuOpUnimpl,         /* 0x2D */ CpuOpUnimpl,         /* 0x2E */ CpuOpUnimpl,         /* 0x2F */ CpuOpUnimpl,
    /* 0x30 */ CpuOpUnimpl,         /* 0x31 */ CpuOpUnimpl,         /* 0x32 */ CpuOpUnimpl,         /* 0x33 */ CpuOpUnimpl,
    /* 0x34 */ CpuOpUnimpl,         /* 0x35 */ CpuOpUnimpl,         /* 0x36 */ CpuOpUnimpl,         /* 0x37 */ CpuOpUnimpl,
    /* 0x38 */ CpuOpUnimpl,         /* 0x39 */ CpuOpUnimpl,         /* 0x3A */ CpuOpUnimpl,         /* 0x3B */ CpuOpUnimpl,
    /* 0x3C */ CpuOpUnimpl,         /* 0x3D */ CpuOpUnimpl,         /* 0x3E */ CpuOpUnimpl,         /* 0x3F */ CpuOpUnimpl,
    /* 0x40 */ CpuOpUnimpl,         /* 0x41 */ CpuOpUnimpl,         /* 0x42 */ CpuOpUnimpl,         /* 0x43 */ CpuOpUnimpl,
    /* 0x44 */ CpuOpUnimpl,         /* 0x45 */ CpuOpUnimpl,         /* 0x46 */ CpuOpUnimpl,         /* 0x47 */ CpuOpUnimpl,
    /* 0x48 */ CpuOpUnimpl,         /* 0x49 */ CpuOpUnimpl,         /* 0x4A */ CpuOpUnimpl,         /* 0x4B */ CpuOpUnimpl,
    /* 0x4C */ CpuOpUnimpl,         /* 0x4D */ CpuOpUnimpl,         /* 0x4E */ CpuOpUnimpl,         /* 0x4F */ CpuOpUnimpl,
    /* 0x50 */ CpuOpUnimpl,         /* 0x51 */ CpuOpUnimpl,         /* 0x52 */ CpuOpUnimpl,         /* 0x53 */ CpuOpUnimpl,
    /* 0x54 */ CpuOpUnimpl,         /* 0x55 */ CpuOpUnimpl,         /* 0x56 */ CpuOpUnimpl,         /* 0x57 */ CpuOpUnimpl,
    /* 0x58 */ CpuOpUnimpl,         /* 0x59 */ CpuOpUnimpl,         /* 0x5A */ CpuOpUnimpl,         /* 0x5B */ CpuOpUnimpl,
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
    /* 0x88 */ CpuOpUnimpl,         /* 0x89 */ CpuOpUnimpl,         /* 0x8A */ CpuOpUnimpl,         /* 0x8B */ CpuOpUnimpl,
    /* 0x8C */ CpuOpUnimpl,         /* 0x8D */ CpuOpSTA_abs,        /* 0x8E */ CpuOpUnimpl,         /* 0x8F */ CpuOpUnimpl,
    /* 0x90 */ CpuOpUnimpl,         /* 0x91 */ CpuOpUnimpl,         /* 0x92 */ CpuOpUnimpl,         /* 0x93 */ CpuOpUnimpl,
    /* 0x94 */ CpuOpUnimpl,         /* 0x95 */ CpuOpUnimpl,         /* 0x96 */ CpuOpUnimpl,         /* 0x97 */ CpuOpUnimpl,
    /* 0x98 */ CpuOpUnimpl,         /* 0x99 */ CpuOpUnimpl,         /* 0x9A */ CpuOpUnimpl,         /* 0x9B */ CpuOpUnimpl,
    /* 0x9C */ CpuOpUnimpl,         /* 0x9D */ CpuOpUnimpl,         /* 0x9E */ CpuOpUnimpl,         /* 0x9F */ CpuOpUnimpl,
    /* 0xA0 */ CpuOpUnimpl,         /* 0xA1 */ CpuOpUnimpl,         /* 0xA2 */ CpuOpUnimpl,         /* 0xA3 */ CpuOpUnimpl,
    /* 0xA4 */ CpuOpUnimpl,         /* 0xA5 */ CpuOpUnimpl,         /* 0xA6 */ CpuOpUnimpl,         /* 0xA7 */ CpuOpUnimpl,
    /* 0xA8 */ CpuOpUnimpl,         /* 0xA9 */ CpuOpLDA_imm,        /* 0xAA */ CpuOpUnimpl,         /* 0xAB */ CpuOpUnimpl,
    /* 0xAC */ CpuOpUnimpl,         /* 0xAD */ CpuOpUnimpl,         /* 0xAE */ CpuOpUnimpl,         /* 0xAF */ CpuOpUnimpl,
    /* 0xB0 */ CpuOpUnimpl,         /* 0xB1 */ CpuOpUnimpl,         /* 0xB2 */ CpuOpUnimpl,         /* 0xB3 */ CpuOpUnimpl,
    /* 0xB4 */ CpuOpUnimpl,         /* 0xB5 */ CpuOpUnimpl,         /* 0xB6 */ CpuOpUnimpl,         /* 0xB7 */ CpuOpUnimpl,
    /* 0xB8 */ CpuOpUnimpl,         /* 0xB9 */ CpuOpUnimpl,         /* 0xBA */ CpuOpUnimpl,         /* 0xBB */ CpuOpUnimpl,
    /* 0xBC */ CpuOpUnimpl,         /* 0xBD */ CpuOpUnimpl,         /* 0xBE */ CpuOpUnimpl,         /* 0xBF */ CpuOpUnimpl,
    /* 0xC0 */ CpuOpUnimpl,         /* 0xC1 */ CpuOpUnimpl,         /* 0xC2 */ CpuOpUnimpl,         /* 0xC3 */ CpuOpUnimpl,
    /* 0xC4 */ CpuOpUnimpl,         /* 0xC5 */ CpuOpUnimpl,         /* 0xC6 */ CpuOpUnimpl,         /* 0xC7 */ CpuOpUnimpl,
    /* 0xC8 */ CpuOpUnimpl,         /* 0xC9 */ CpuOpUnimpl,         /* 0xCA */ CpuOpUnimpl,         /* 0xCB */ CpuOpUnimpl,
    /* 0xCC */ CpuOpUnimpl,         /* 0xCD */ CpuOpUnimpl,         /* 0xCE */ CpuOpUnimpl,         /* 0xCF */ CpuOpUnimpl,
    /* 0xD0 */ CpuOpUnimpl,         /* 0xD1 */ CpuOpUnimpl,         /* 0xD2 */ CpuOpUnimpl,         /* 0xD3 */ CpuOpUnimpl,
    /* 0xD4 */ CpuOpUnimpl,         /* 0xD5 */ CpuOpUnimpl,         /* 0xD6 */ CpuOpUnimpl,         /* 0xD7 */ CpuOpUnimpl,
    /* 0xD8 */ CpuOpCLD,            /* 0xD9 */ CpuOpUnimpl,         /* 0xDA */ CpuOpUnimpl,         /* 0xDB */ CpuOpUnimpl,
    /* 0xDC */ CpuOpUnimpl,         /* 0xDD */ CpuOpUnimpl,         /* 0xDE */ CpuOpUnimpl,         /* 0xDF */ CpuOpUnimpl,
    /* 0xE0 */ CpuOpUnimpl,         /* 0xE1 */ CpuOpUnimpl,         /* 0xE2 */ CpuOpUnimpl,         /* 0xE3 */ CpuOpUnimpl,
    /* 0xE4 */ CpuOpUnimpl,         /* 0xE5 */ CpuOpUnimpl,         /* 0xE6 */ CpuOpUnimpl,         /* 0xE7 */ CpuOpUnimpl,
    /* 0xE8 */ CpuOpUnimpl,         /* 0xE9 */ CpuOpUnimpl,         /* 0xEA */ CpuOpNOP,            /* 0xEB */ CpuOpUnimpl,
    /* 0xEC */ CpuOpUnimpl,         /* 0xED */ CpuOpUnimpl,         /* 0xEE */ CpuOpUnimpl,         /* 0xEF */ CpuOpUnimpl,
    /* 0xF0 */ CpuOpUnimpl,         /* 0xF1 */ CpuOpUnimpl,         /* 0xF2 */ CpuOpUnimpl,         /* 0xF3 */ CpuOpUnimpl,
    /* 0xF4 */ CpuOpUnimpl,         /* 0xF5 */ CpuOpUnimpl,         /* 0xF6 */ CpuOpUnimpl,         /* 0xF7 */ CpuOpUnimpl,
    /* 0xF8 */ CpuOpUnimpl,         /* 0xF9 */ CpuOpUnimpl,         /* 0xFA */ CpuOpUnimpl,         /* 0xFB */ CpuOpUnimpl,
    /* 0xFC */ CpuOpUnimpl,         /* 0xFD */ CpuOpUnimpl,         /* 0xFE */ CpuOpUnimpl,         /* 0xFF */ CpuOpUnimpl,
};


