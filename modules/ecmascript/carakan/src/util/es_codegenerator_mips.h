/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_CODEGENERATOR_MIPS_H
#define ES_CODEGENERATOR_MIPS_H

#ifdef ARCHITECTURE_MIPS

#include "modules/ecmascript/carakan/src/util/es_codegenerator_mips_opcodes.h"

// MIPS instructions are 32-bit values.
typedef unsigned ES_NativeInstruction;

// Instructions are output one at a time.
enum { ES_NATIVE_MAXIMUM_INSTRUCTION_LENGTH = 1 };

// Jump conditions for ES_CodeGenerator_Base::Jump
typedef unsigned ES_NativeJumpCondition;

#define ES_NATIVE_NOT_A_JUMP 0xffffffff

#define ES_NATIVE_UNCONDITIONAL MIPS_I(MIPS::BEQ, 0, 0, 0)

#define ES_NATIVE_CONDITION_EQ(reg1, reg2) MIPS_I(MIPS::BEQ, reg1, reg2, 0) // reg1 == reg2
#define ES_NATIVE_CONDITION_NE(reg1, reg2) MIPS_I(MIPS::BNE, reg1, reg2, 0) // reg1 != reg2

#define ES_NATIVE_CONDITION_GEZ(reg) MIPS_I(MIPS::REGIMM, reg, MIPS::BGEZ, 0) // reg >= 0
#define ES_NATIVE_CONDITION_GTZ(reg) MIPS_I(MIPS::BGTZ, reg, 0, 0) // reg > 0
#define ES_NATIVE_CONDITION_LEZ(reg) MIPS_I(MIPS::BLEZ, reg, 0, 0) // reg <= 0
#define ES_NATIVE_CONDITION_LTZ(reg) MIPS_I(MIPS::REGIMM, reg, MIPS::BLTZ, 0) // reg < 0

#define ES_NATIVE_CONDITION_EQZ(reg) ES_NATIVE_CONDITION_EQ(reg, 0) // reg == 0
#define ES_NATIVE_CONDITION_NEZ(reg) ES_NATIVE_CONDITION_NE(reg, 0) // reg != 0

#define ES_NATIVE_CONDITION_C1F(cc) MIPS_FB(cc, 0, 0, 0) // cc == 0
#define ES_NATIVE_CONDITION_C1T(cc) MIPS_FB(cc, 0, 1, 0) // cc != 0

#define ES_FUNCTION_CONSTANT_DATA_SECTION
#define ES_DATA_SECTION_MAX_SIZE USHRT_MAX
#define ES_DATA_SECTION_MAX_POSITIVE_OFFSET SHRT_MAX

#include "modules/ecmascript/carakan/src/util/es_codegenerator_base.h"

class ES_CodeGenerator : public ES_CodeGenerator_Base
{
public:
    ES_CodeGenerator(OpMemGroup *arena);

#ifdef NATIVE_DISASSEMBLER_SUPPORT
    void EnableDisassemble() { disassemble = TRUE; }
    BOOL DisassembleEnabled() { return disassemble; }

    void DumpCodeVector() { dump_cv = TRUE; }
#endif // NATIVE_DISASSEMBLER_SUPPORT
#ifdef NATIVE_DISASSEMBLER_SUPPORT
    BOOL disassemble, dump_cv;
#endif // NATIVE_DISASSEMBLER_SUPPORT

    enum Register
    {
        REG_ZERO, // always zero
        REG_AT,   // "assembler temporary"
        REG_V0,   // function return value
        REG_V1,   // -"-
        REG_A0,   // function arguments
        REG_A1,   // -"-
        REG_A2,   // -"-
        REG_A3,   // -"-
        REG_T0,   // temporaries (caller save)
        REG_T1,   // -"-
        REG_T2,   // -"-
        REG_T3,   // -"-
        REG_T4,   // -"-
        REG_T5,   // -"-
        REG_T6,   // -"-
        REG_T7,   // -"-
        REG_S0,   // saved temporaries (callee save)
        REG_S1,   // -"-
        REG_S2,   // -"-
        REG_S3,   // -"-
        REG_S4,   // -"-
        REG_S5,   // -"-
        REG_S6,   // -"-
        REG_S7,   // -"-
        REG_T8,   // temporaries (caller save)
        REG_T9,   // -"-
        REG_K0,   // reserved for OS kernel
        REG_K1,   // -"-
        REG_GP,   // global pointer
        REG_SP,   // stack pointer
        REG_FP,   // frame pointer
        REG_RA    // return address
    };

    enum FPURegister
    {
        REG_F0,  REG_F1,  REG_F2,  REG_F3,  REG_F4,  REG_F5,  REG_F6,  REG_F7,  REG_F8,  REG_F9,
        REG_F10, REG_F11, REG_F12, REG_F13, REG_F14, REG_F15, REG_F16, REG_F17, REG_F18, REG_F19,
        REG_F20, REG_F21, REG_F22, REG_F23, REG_F24, REG_F25, REG_F26, REG_F27, REG_F28, REG_F29,
        REG_F30, REG_F31
    };

    enum FPUControlReg
    {
        REG_FIR = 0, REG_FCCR = 25, REG_FEXR = 26, REG_FENR = 28, REG_FCSR = 32
    };

    enum FPUType
    {
        FPU_NONE = 0,
        FPU_SINGLE, // Each register can hold a single FP value.
        FPU_DOUBLE  // Each register can hold a double FP value (F64).
    };

    enum FPUFormat
    {
        S  = 0, // Single floating point
        D  = 1, // Double floating point
        W  = 4, // 32-bit signed int
        L  = 5, // 64-bit signed int
        PS = 6  // Paired single floating point
    };

    enum FPUCondition
    {
        F = 0, UN, EQ, UEQ, OLT, ULT, OLE, ULE,
        SF, NGLE, SEQ, NGL, LT, NGE, LE, NGT
    };

    static FPUType GetFPUType();

    unsigned CalculateMaximumDistance(Block *src, Block *dst);
    const OpExecMemory *GetCode(OpExecMemoryManager *execmem, void *constant_reg = NULL);

    // All assembler functions follow MIPS style argument order, usually: dest, source, source.

    // Convenience "assembler macros", may output multiple instructions and/or clobber REG_AT.

    void LoadDataSectionPointer();
    void Load(Register rt, Constant *constant);
    void Load(FPURegister ft, Constant *constant);
    void Load(Register rt, int immediate);
    void Load(Register rt, void *ptr) { Load(rt, (INTPTR) ptr); }
    void Load(Register rt, void(*func)()) { Load(rt, (INTPTR) func); }

    void Load(Register rt, int offset, Register base);
    void Store(Register rt, int offset, Register base);

    void Move(Register rd, Register rs);

    void Add(Register rd, Register rt);

    void Add(Register rt, Register rs, int immediate);
    void Add(Register rt, int immediate);

    void And(Register rt, Register rs, unsigned immediate);
    void And(Register rt, unsigned immediate);

    void JumpEQ(JumpTarget *target, Register rt, Register rs, BOOL hint = FALSE);
    void JumpNE(JumpTarget *target, Register rt, Register rs, BOOL hint = FALSE);

    void JumpEQ(JumpTarget *target, Register rt, int immediate);
    void JumpNE(JumpTarget *target, Register rt, int immediate);

    void JumpLT(JumpTarget *target, Register rt, Register rs);
    void JumpLE(JumpTarget *target, Register rt, Register rs);
    void JumpGT(JumpTarget *target, Register rt, Register rs);
    void JumpGE(JumpTarget *target, Register rt, Register rs);

    void JumpLT(JumpTarget *target, Register rt, int immediate);
    void JumpLE(JumpTarget *target, Register rt, int immediate);
    void JumpGT(JumpTarget *target, Register rt, int immediate);
    void JumpGE(JumpTarget *target, Register rt, int immediate);

    // Unsigned comparison
    void JumpLTU(JumpTarget *target, Register rt, unsigned immediate);
    void JumpLEU(JumpTarget *target, Register rt, unsigned immediate);
    void JumpGTU(JumpTarget *target, Register rt, unsigned immediate);
    void JumpGEU(JumpTarget *target, Register rt, unsigned immediate);

    void Sub(Register rd, Register rt);

    void Sub(Register rt, Register rs, int immediate);
    void Sub(Register rt, int immediate);

    // Simple one-to-one assembler instructions.

    void ABS(FPUFormat fmt, FPURegister fd, FPURegister fs);

    void ADD(FPUFormat fmt, FPURegister fd, FPURegister fs, FPURegister ft);
    void ADDIU(Register rt, Register rs, short immediate);
    void ADDU(Register rd, Register rs, Register rt);

    void AND(Register rd, Register rs, Register rt);
    void ANDI(Register rt, Register rs, unsigned short immediate);

    void B(short offset);

    void BC1F(short offset, int cc = 0);
    void BC1T(short offset, int cc = 0);

    void BEQ(Register rs, Register rt, short offset);
    void BNE(Register rs, Register rt, short offset);

    void BREAK(unsigned code = 0);

    void C(FPUCondition cond, FPUFormat fmt, FPURegister fs, FPURegister ft, int cc = 0);

    void CEIL(FPUFormat dfmt, FPUFormat sfmt, FPURegister fd, FPURegister fs);

    void CFC1(Register rt, FPUControlReg fs);

    void CVT(FPUFormat dfmt, FPUFormat sfmt, FPURegister fd, FPURegister fs);

    void DIV(Register rs, Register rt);
    void DIV(FPUFormat fmt, FPURegister fd, FPURegister fs, FPURegister ft);

    void FLOOR(FPUFormat dfmt, FPUFormat sfmt, FPURegister fd, FPURegister fs);

    void JALR(Register rs, Register rd = REG_RA);
    void JR(Register rs = REG_RA);

    void LB(Register rt, short offset, Register base);
    void LBU(Register rt, short offset, Register base);
    void LDC1(FPURegister ft, short offset, Register base);
    void LH(Register rt, short offset, Register base);
    void LHU(Register rt, short offset, Register base);
    void LUI(Register rt, unsigned short immediate);
    void LW(Register rt, short offset, Register base);
    void LWC1(FPURegister ft, short offset, Register base);

    void MFC1(Register rt, FPURegister fs);
    void MFHC1(Register rt, FPURegister fs);

    void MFHI(Register rd);
    void MFLO(Register rd);

    void MOV(FPUFormat fmt, FPURegister fd, FPURegister fs);

    void MOVF(Register rd, Register rs, int cc = 0);
    void MOVN(Register rd, Register rs, Register rt);
    void MOVT(Register rd, Register rs, int cc = 0);
    void MOVZ(Register rd, Register rs, Register rt);

    void MTC1(Register rt, FPURegister fs);
    void MTHC1(Register rt, FPURegister fs);

    void MUL(FPUFormat fmt, FPURegister fd, FPURegister fs, FPURegister ft);
    void MULT(Register rs, Register rt);

    void NEG(FPUFormat fmt, FPURegister fd, FPURegister fs);

    void NOP();

    void NOR(Register rd, Register rs, Register rt);
    void OR(Register rd, Register rs, Register rt);
    void ORI(Register rt, Register rs, unsigned short immediate);

    void SB(Register rt, short offset, Register base);

    void SDC1(FPURegister ft, short offset, Register base);

    void SH(Register rt, short offset, Register base);

    void SLL(Register rd, Register rt, unsigned sa);
    void SLLV(Register rd, Register rt, Register rs);

    void SLT(Register rd, Register rs, Register rt);
    void SLTI(Register rt, Register rs, short immediate);
    void SLTIU(Register rt, Register rs, unsigned short immediate);
    void SLTU(Register rd, Register rs, Register rt);

    void SQRT(FPUFormat fmt, FPURegister fd, FPURegister fs);

    void SRA(Register rd, Register rt, int sa);
    void SRAV(Register rd, Register rt, Register rs);
    void SRL(Register rd, Register rt, int sa);
    void SRLV(Register rd, Register rt, Register rs);

    void SUB(FPUFormat fmt, FPURegister fd, FPURegister fs, FPURegister ft);
    void SUBU(Register rd, Register rs, Register rt);

    void SW(Register rt, short offset, Register base);
    void SWC1(FPURegister ft, short offset, Register base);

    void ROUND(FPUFormat dfmt, FPUFormat sfmt, FPURegister fd, FPURegister fs);
    void TRUNC(FPUFormat dfmt, FPUFormat sfmt, FPURegister fd, FPURegister fs);

    void XOR(Register rd, Register rs, Register rt);
    void XORI(Register rt, Register rs, unsigned short immediate);
};

#include "modules/ecmascript/carakan/src/util/es_codegenerator_mips_inlines.h"

#endif // ARCHITECTURE_MIPS
#endif // ES_CODEGENERATOR_MIPS_H
