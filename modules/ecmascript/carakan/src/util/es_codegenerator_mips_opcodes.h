
#ifndef ES_CODEGENERATOR_MIPS_OPCODES_H
#define ES_CODEGENERATOR_MIPS_OPCODES_H

// Register
#define MIPS_R(op, rs, rt, rd, x, s) (((op) << 26) | ((rs) << 21) | ((rt) << 16) | ((rd) << 11) | ((x) << 6) | (s))

// Immediate
#define MIPS_I(op, rs, rt, imm) (((op) << 26) | ((rs) << 21) | ((rt) << 16) | ((imm) & 0xffff))

// Jump
#define MIPS_J(op, index) (((op) << 26) | ((index) & 0x3ffffff))

// FPU Transfer
#define MIPS_FT(rs, rt, fs) ((MIPS::COP1 << 26) | ((rs) << 21) | ((rt) << 16) | ((fs) << 11))

// FPU Branch
#define MIPS_FB(cc, nd, tf, offset) ((MIPS::COP1 << 26) | (MIPS::BC1 << 21) | ((cc) << 18) | ((nd) << 17) | ((tf) << 16) | ((offset) & 0xffff))

// FPU Register op
#define MIPS_FR(fmt, ft, fs, fd, func) ((MIPS::COP1 << 26) | 0x2000000 | ((fmt) << 21) | ((ft) << 16) | ((fs) << 11) | ((fd) << 6) | (func))


class MIPS // Think namespace.
{
public:

    enum OpCode // Sparse 8x8 table
    {
        SPECIAL = 0, REGIMM, J, JAL, BEQ, BNE, BLEZ, BGTZ,
        ADDI, ADDIU, SLTI, SLTIU, ANDI, ORI, XORI, LUI,
        COP0, COP1, COP2, COP1X, BEQL, BNEL, BLEZL, BGTZL,
        SPECIAL2 = 0x1c, JALX, SPECIAL3 = 0x1f,
        LB, LH, LWL, LW, LBU, LHU, LWR,
        SB = 0x28, SH, SWL, SW, SWR = 0x2e, CACHE,
        LL, LWC1, LWC2, PREF, LDC1 = 0x35, LDC2,
        SC = 0x38, SWC1, SWC2, SDC1 = 0x3d, SDC2
    };

    enum Special // Sparse 8x8 table
    {
        SLL = 0, MOVCI, SRL, SRA, SLLV, SRLV = 0x06, SRAV,
        JR, JALR, MOVZ, MOVN, SYSCALL, BREAK, SYNC = 0x0f,
        MFHI, MTHI, MFLO, MTLO,
        MULT = 0x18, MULTU, DIV, DIVU,
        ADD = 0x20, ADDU, SUB, SUBU, AND, OR, XOR, NOR,
        SLT = 0x2a, SLTU,
        TGE = 0x30, TGEU, TLT, TLTU, TEQ, TNE = 0x36
    };

    enum Special2 // Very sparse 8x8 table
    {
        MADD = 0, MADDU, MUL, MSUB = 4, MSUBU,
        CLZ = 0x20, CLO,
        SDBBP = 0x3f
    };

    enum Special3 // Very sparse 8x8 table
    {
        EXT = 0, INS = 4,
        BSHFL = 0x20
    };

    enum RegImm // Sparse 8x4 table
    {
        BLTZ = 0, BGEZ, BLTZL, BGEZL,
        TGEI = 0x08, TGEIU, TLTI, TLTIU, TEQI, TNEI = 0x0e,
        BLTZAL = 0x10, BGEZAL, BLTZALL, BGEZALL,
        SYNCI = 0x1f
    };

    // FPU

    enum Cop1Rs // Sparse 8x4 table
    {
        MFC1 = 0, CFC1 = 0x02, MFHC1, MTC1, CTC1 = 0x06, MTHC1,
        BC1, BC1ANY2, BC1ANY4,
        S = 0x10, D, W = 0x14, L, PS
    };

    enum Cop1Function // Sparse 8x8 table
    {
        F_ADD = 0, F_SUB, F_MUL, F_DIV, SQRT, ABS, MOV, NEG,
        ROUND_L, TRUNC_L, CEIL_L, FLOOR_L, ROUND_W, TRUNC_W, CEIL_W, FLOOR_W,
        MOVCF = 0x11, F_MOVZ, F_MOVN, RECIP = 0x15, RSQRT,
        RECIP2 = 0x1c, RECIP1, RSQRT1, RSQRT2,
        CVT_S, CVT_D, CVT_W = 0x24, CVT_L, CVT_PS,
        F = 0x30, UN, EQ, UEQ, OLT, ULT, OLE, ULE,
        SF, NGLE, SEQ, NGL, LT, NGE, LE, NGT
    };
};

#endif // ES_CODEGENERATOR_MIPS_OPCODES_H
