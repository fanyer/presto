/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_CODEGENERATOR_IA32_H
#define ES_CODEGENERATOR_IA32_H

#ifdef ARCHITECTURE_IA32

typedef unsigned char ES_NativeInstruction;

enum ES_NativeJumpCondition
{
    ES_NATIVE_UNCONDITIONAL,

    ES_NATIVE_CONDITION_EQUAL,
    ES_NATIVE_CONDITION_NOT_EQUAL,

    ES_NATIVE_CONDITION_BELOW,
    ES_NATIVE_CONDITION_NOT_BELOW,
    ES_NATIVE_CONDITION_BELOW_EQUAL,
    ES_NATIVE_CONDITION_NOT_BELOW_EQUAL,

    ES_NATIVE_CONDITION_LESS,
    ES_NATIVE_CONDITION_NOT_LESS,
    ES_NATIVE_CONDITION_LESS_EQUAL,
    ES_NATIVE_CONDITION_NOT_LESS_EQUAL,

    ES_NATIVE_CONDITION_OVERFLOW,
    ES_NATIVE_CONDITION_NOT_OVERFLOW,

    ES_NATIVE_CONDITION_PARITY,
    ES_NATIVE_CONDITION_NOT_PARITY,

    ES_NATIVE_CONDITION_SIGN,
    ES_NATIVE_CONDITION_NOT_SIGN,

    ES_NATIVE_NOT_A_JUMP,

    ES_NATIVE_CONDITION_ZERO = ES_NATIVE_CONDITION_EQUAL,
    ES_NATIVE_CONDITION_NOT_ZERO = ES_NATIVE_CONDITION_NOT_EQUAL,

    ES_NATIVE_CONDITION_ABOVE = ES_NATIVE_CONDITION_NOT_BELOW_EQUAL,
    ES_NATIVE_CONDITION_NOT_ABOVE = ES_NATIVE_CONDITION_BELOW_EQUAL,
    ES_NATIVE_CONDITION_ABOVE_EQUAL = ES_NATIVE_CONDITION_NOT_BELOW,
    ES_NATIVE_CONDITION_NOT_ABOVE_EQUAL = ES_NATIVE_CONDITION_BELOW,

    ES_NATIVE_CONDITION_GREATER = ES_NATIVE_CONDITION_NOT_LESS_EQUAL,
    ES_NATIVE_CONDITION_NOT_GREATER = ES_NATIVE_CONDITION_LESS_EQUAL,
    ES_NATIVE_CONDITION_GREATER_EQUAL = ES_NATIVE_CONDITION_NOT_LESS,
    ES_NATIVE_CONDITION_NOT_GREATER_EQUAL = ES_NATIVE_CONDITION_LESS,

    ES_NATIVE_CONDITION_CARRY = ES_NATIVE_CONDITION_BELOW,
    ES_NATIVE_CONDITION_NOT_CARRY = ES_NATIVE_CONDITION_NOT_BELOW
};

enum { ES_NATIVE_MAXIMUM_INSTRUCTION_LENGTH = 15 };

#ifndef ARCHITECTURE_AMD64
#define ES_CODEGENERATOR_RELATIVE_CALLS
#endif // ARCHITECTURE_AMD64

#include "modules/ecmascript/carakan/src/util/es_codegenerator_base.h"

#undef Register
#undef REG_NONE

class ES_CodeGenerator
    : public ES_CodeGenerator_Base
{
public:
    static BOOL SupportsSSE2();
    static BOOL SupportsSSE41();

    enum Register
    {
        REG_NONE = -256,

        REG_AX = 0,
        REG_CX,
        REG_DX,
        REG_BX,
        REG_SP,
        REG_BP,
        REG_SI,
        REG_DI,

#ifdef ARCHITECTURE_AMD64
        REG_R8,
        REG_R9,
        REG_R10,
        REG_R11,
        REG_R12,
        REG_R13,
        REG_R14,
        REG_R15,
#endif // ARCHITECTURE_AMD64

        REG_MAXIMUM
    };

#ifdef ARCHITECTURE_SSE
    enum XMM
    {
        REG_XMM_NONE = -256,

        REG_XMM0 = 0,
        REG_XMM1,
        REG_XMM2,
        REG_XMM3,
        REG_XMM4,
        REG_XMM5,
        REG_XMM6,
        REG_XMM7,
        REG_XMM8,
        REG_XMM9,
        REG_XMM10,
        REG_XMM11,
        REG_XMM12,
        REG_XMM13,
        REG_XMM14,
        REG_XMM15
    };
#endif // ARCHITECTURE_SSE

    enum Scale
    {
        SCALE_1 = 0,
        SCALE_2 = 1,
        SCALE_4 = 2,
        SCALE_8 = 3,

#ifdef ARCHITECTURE_AMD64
        SCALE_POINTER = SCALE_8
#else // ARCHITECTURE_AMD64
        SCALE_POINTER = SCALE_4
#endif // ARCHITECTURE_AMD64
    };

    enum OperandSize
    {
        OPSIZE_8 = 1,
        OPSIZE_16 = 2,
        OPSIZE_32 = 4,
        OPSIZE_64 = 8,
#ifdef ARCHITECTURE_AMD64
        OPSIZE_DEFAULT = OPSIZE_32,
        OPSIZE_POINTER = OPSIZE_64,
#else // ARCHITECTURE_AMD64
        OPSIZE_DEFAULT = OPSIZE_32,
        OPSIZE_POINTER = OPSIZE_32
#endif // ARCHITECTURE_AMD64
    };

    class Operand
    {
    public:
        enum Type { TYPE_NONE, TYPE_REGISTER, TYPE_XMM, TYPE_MEMORY, TYPE_IMMEDIATE, TYPE_IMMEDIATE64, TYPE_CONSTANT };

        Operand() {}
        Operand(Type type, unsigned reg) : type(type), base(REG_NONE), index(REG_NONE), scale(SCALE_1), disp(0), immediate(0), reg(reg) {}
        Operand(Type type, Register base) : type(type), base(base), index(REG_NONE), scale(SCALE_1), disp(0), immediate(0) {}

        Operand(Register base) : type(TYPE_REGISTER), base(base), index(REG_NONE), scale(SCALE_1), disp(0), immediate(0) {}
#ifdef ARCHITECTURE_SSE
        Operand(XMM xmm) : type(TYPE_XMM), base(REG_NONE), index(REG_NONE), scale(SCALE_1), disp(0), immediate(0), reg(0), xmm(xmm) {}
#endif // ARCHITECTURE_SSE
        Operand(Register base, int disp) : type(TYPE_MEMORY), base(base), index(REG_NONE), scale(SCALE_1), disp(disp), immediate(0), pointer(0) {}
        Operand(Register base, Register index, Scale scale) : type(TYPE_MEMORY), base(base), index(index), scale(scale), disp(0), immediate(0), pointer(0) {}
        Operand(Register base, Register index, Scale scale, int disp) : type(TYPE_MEMORY), base(base), index(index), scale(scale), disp(disp), immediate(0), pointer(0) {}
        Operand(const void *pointer, Type type) : type(type), base(REG_NONE), index(REG_NONE), scale(SCALE_1), disp(0), immediate(0), pointer(pointer) {}
#ifdef ARCHITECTURE_AMD64
        Operand(int immediate) : type(TYPE_IMMEDIATE), base(REG_NONE), index(REG_NONE), scale(SCALE_1), disp(0), immediate(immediate), pointer(0) {}
        Operand(INTPTR immediate) : type(TYPE_IMMEDIATE64), base(REG_NONE), index(REG_NONE), scale(SCALE_1), disp(0), immediate(immediate), pointer(0) {}
        Operand(Constant *constant) : type(TYPE_CONSTANT), base(REG_NONE), index(REG_NONE), scale(SCALE_1), disp(INT_MAX), immediate(0), pointer(0), constant(constant) {}
#else // ARCHITECTURE_AMD64
        Operand(INTPTR immediate) : type(TYPE_IMMEDIATE), base(REG_NONE), index(REG_NONE), scale(SCALE_1), disp(0), immediate(immediate), pointer(0) {}
        Operand(Constant *constant) : type(TYPE_CONSTANT), base(REG_NONE), index(REG_NONE), scale(SCALE_1), disp(INT_MAX), immediate(0), pointer(0), constant(constant) {}
#endif // ARCHITECTURE_AMD64

#ifdef ARCHITECTURE_AMD64
        inline BOOL UseREX() const;
#endif // ARCHITECTURE_AMD64

#ifdef ARCHITECTURE_AMD64
        BOOL UseSIB() const { return type == TYPE_MEMORY && ((base & 7) == REG_SP || base == REG_NONE || index != REG_NONE); }
#else // ARCHITECTURE_AMD64
        BOOL UseSIB() const { return type == TYPE_MEMORY && ((base & 7) == REG_SP || index != REG_NONE); }
#endif // ARCHITECTURE_AMD64
        BOOL UseDisp8() const { return type == TYPE_MEMORY && ((disp != 0 || (base & 7) == REG_BP) && disp >= -128 && disp <= 127 && base != REG_NONE); }
        BOOL UseDisp32() const { return type == TYPE_MEMORY && disp != 0 && (disp < -128 || disp > 127 || base == REG_NONE) || type == TYPE_CONSTANT; }

        Type type;
        Register base, index;
        Scale scale;
        int disp;
        INTPTR immediate;
        const void *pointer;
        unsigned reg;
#ifdef ARCHITECTURE_SSE
        XMM xmm;
#endif // ARCHITECTURE_SSE
        Constant *constant;
    };

    ES_CodeGenerator(OpMemGroup *arena = NULL);
    ~ES_CodeGenerator();

#ifdef NATIVE_DISASSEMBLER_SUPPORT
    void EnableDisassemble();
    BOOL DisassembleEnabled() { return disassemble; }
#endif // NATIVE_DISASSEMBLER_SUPPORT

    static Operand REGISTER(Register reg) { return Operand(Operand::TYPE_REGISTER, reg); }
    static Operand MEMORY(Register base, int disp = 0) { return Operand(base, disp); }
    static Operand MEMORY(Register base, Register index, Scale scale, int disp = 0) { return Operand(base, index, scale, disp); }
#ifdef ARCHITECTURE_AMD64
    static Operand MEMORY(const void *pointer) { return Operand(pointer, Operand::TYPE_MEMORY); }
    static Operand CONSTANT(Constant *constant) { return Operand(constant); }
    static Operand IMMEDIATE(INTPTR immediate) { if (immediate >= INT_MIN && immediate <= INT_MAX) return Operand(static_cast<int>(immediate)); else return Operand(immediate); }
#else // ARCHITECTURE_AMD64
    static Operand MEMORY(const void *pointer) { return Operand(REG_NONE, reinterpret_cast<INTPTR>(pointer)); }
    static Operand IMMEDIATE(INTPTR immediate) { return Operand(immediate); }
    static Operand IMMEDIATE(void *pointer) { return Operand(pointer, Operand::TYPE_IMMEDIATE); }
    static Operand CONSTANT(Constant *constant) { return Operand(constant); }
#endif // ARCHITECTURE_AMD64
    static Operand ZERO() { return IMMEDIATE(static_cast<INTPTR>(0)); }

    static BOOL IsSameRegister(const Operand &op1, const Operand &op2) { return op1.type == Operand::TYPE_REGISTER && op2.type == Operand::TYPE_REGISTER && op1.base == op2.base; }

#ifdef _DEBUG
    void NOP();
    void INT3();
#endif

    void MOV(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void MOVDQ(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void MOVSX(const Operand &source, const Operand &target, OperandSize msize, OperandSize rsize = OPSIZE_DEFAULT);
    void MOVZX(const Operand &source, const Operand &target, OperandSize msize, OperandSize rsize = OPSIZE_DEFAULT);
    void MovePointerIntoRegister(void *ptr, ES_CodeGenerator::Register reg);

    void ADD(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void ADC(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void SUB(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void SBB(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void LEA(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void AND(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void OR(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void XOR(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void SHL(const Operand &count, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void SAR(const Operand &count, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void SHR(const Operand &count, const Operand &target, OperandSize size = OPSIZE_DEFAULT);

    void NEG(const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void NOT(const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void INC(const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void DEC(const Operand &target, OperandSize size = OPSIZE_DEFAULT);

    void CDQ();
    /**< EDX:EAX = sign-extend(EAX) */

    void IMUL(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    /**< target := target * source (target must be register, source can be memory) */
    void IMUL(const Operand &source, int immediate, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    /**< target := source * immediate (target must be register, source can be memory) */
    void IMUL(const Operand &source, OperandSize size = OPSIZE_DEFAULT);
    /**< EDX:EAX := EAX * source (source can be memory) */

    void IDIV(const Operand &source, OperandSize size = OPSIZE_DEFAULT);
    /**< EAX := EDX:EAX / source, EDX := remainder */

    void CMP(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);
    void TEST(const Operand &source, const Operand &target, OperandSize size = OPSIZE_DEFAULT);

    void JMP(const Operand &target);

    typedef ES_NativeJumpCondition Condition;

    void CMOV(const Operand &source, const Operand &target, Condition condition, OperandSize size = OPSIZE_DEFAULT);
    void SET(const Operand &target, Condition condition);

    void FLD(const Operand &source, OperandSize size = OPSIZE_64);
    void FILD(const Operand &source, OperandSize size = OPSIZE_DEFAULT);
    void FLDZ();
    void FLD1();
    void FST(const Operand &target, BOOL pop, OperandSize size = OPSIZE_64);
    void FST(int target, BOOL pop);
    void FIST(const Operand &target, BOOL pop, OperandSize size = OPSIZE_DEFAULT);
    void FADD(const Operand &source);
    void FSUB(const Operand &source, BOOL reverse);
    void FMUL(const Operand &source);
    void FDIV(const Operand &source, BOOL reverse);
    void FADD(int source, int target, BOOL pop);
    void FSUB(int source, int target, BOOL pop, BOOL reverse);
    void FMUL(int source, int target, BOOL pop);
    void FDIV(int source, int target, BOOL pop, BOOL reverse);
    void FIADD(const Operand &source, OperandSize size = OPSIZE_32);
    void FISUB(const Operand &source, BOOL reverse, OperandSize size = OPSIZE_32);
    void FIMUL(const Operand &source, OperandSize size = OPSIZE_32);
    void FIDIV(const Operand &source, BOOL reverse, OperandSize size = OPSIZE_32);
    void FREMP();
    void FCHS();
    void FUCOMI(int operand, BOOL pop);
    void FXAM();
    void FNSTSW();

    void FSIN();
    void FCOS();
    void FSINCOS();
    void FSQRT();
    void FRNDINT();
    void FLDLN2();
    void FYL2X();

    void FNSTCW(const Operand &target);
    void FLDCW(const Operand &source);

    void EMMS();

#ifdef ARCHITECTURE_SSE
    void MOVSD(const Operand &source, XMM target);
    void MOVSD(XMM source, const Operand &target);
    void MOVSS(XMM source, const Operand &target);
    void MOVD(XMM source, const Operand &target);
    void MOVQ(XMM source, const Operand &target);
    void MOVDQA(const Operand &source, XMM target);
    void MOVDQA(XMM source, const Operand &target);
    void CVTSI2SD(const Operand &source, XMM target, OperandSize size = OPSIZE_DEFAULT);
    void CVTSI2SS(const Operand &source, XMM target, OperandSize size = OPSIZE_DEFAULT);
    void CVTSD2SI(const Operand &source, Register target, OperandSize size = OPSIZE_DEFAULT);
    void CVTTSD2SI(const Operand &source, Register target, OperandSize size = OPSIZE_DEFAULT);
    void CVTTSS2SI(const Operand &source, Register target, OperandSize size = OPSIZE_DEFAULT);
    void CVTSD2SS(const Operand &source, XMM target);
    void CVTSS2SD(const Operand &source, XMM target);
    void ADDSD(const Operand &source, XMM target);
    void SUBSD(const Operand &source, XMM target);
    void MULSD(const Operand &source, XMM target);
    void DIVSD(const Operand &source, XMM target);
    void ANDPD(const Operand &source, XMM target);
    void ANDNPD(const Operand &source, XMM target);
    void ORPD(const Operand &source, XMM target);
    void XORPD(const Operand &source, XMM target);
    void UCOMISD(const Operand &source, XMM target);
    void UCOMISS(const Operand &source, XMM target);
    void SQRTSD(const Operand &source, XMM target);
    void RCPSS(const Operand &source, XMM target);

    enum RoundMode { ROUND_MODE_nearest, ROUND_MODE_floor, ROUND_MODE_ceil };
    void ROUNDSD(const Operand &source, XMM target, RoundMode mode);
#endif // ARCHITECTURE_SSE

    void CALL(const Operand &target);
    void CALL(void *f);
    void CALL(void (*f)());

    void RET();
    void RET(short n);

    void PUSH(const Operand &op, OperandSize size = OPSIZE_POINTER);
    void POP(const Operand &op, OperandSize size = OPSIZE_POINTER);

    void CPUID();

    void PUSHFD();
    void POPFD();

#ifdef ES_NATIVE_SUPPORT
    virtual const OpExecMemory *GetCode(OpExecMemoryManager *execmem, void *constant_reg = NULL);
#endif // ES_NATIVE_SUPPORT

private:
#ifdef NATIVE_DISASSEMBLER_SUPPORT
    BOOL disassemble;
#endif // NATIVE_DISASSEMBLER_SUPPORT
    BOOL supports_SSE2, supports_SSE41;

#ifdef _DEBUG
    void Dump(unsigned char *begin, unsigned char *end);
    BOOL dump_array;
#endif // _DEBUG

    unsigned CalculateMaximumDistance(Block *src, Block *dst);

    Operand Dummy(unsigned reg) { return Operand(Operand::TYPE_NONE, reg); }

    inline void Write(char byte);
    inline void WriteByte(unsigned char byte);
    inline void Write(short i);
    inline void Write(int i);
    inline void Write(unsigned u);
#ifdef ARCHITECTURE_AMD64
    inline void Write(INTPTR i);
#endif // ARCHITECTURE_AMD64
    inline void Write(double d);
    inline void Write(const void *p);

    inline void WriteModRM(const Operand &rm, const Operand &reg);
    inline void WriteSIB(const Operand &rm);
    inline void WriteAddress(const Operand &operand);
    inline void WriteImmediate(INTPTR immediate, OperandSize size);

    void GenericMemX87(const Operand &mem, int opcode, int reg);
#ifdef ARCHITECTURE_SSE
    void GenericSSE2(unsigned prefix, unsigned op, const Operand &source, XMM target, OperandSize size = OPSIZE_32);
#endif // ARCHITECTURE_SSE

    void Generic2Operands(unsigned char OPs[4], const Operand &op1, const Operand &op2, OperandSize size = OPSIZE_DEFAULT, unsigned char extra_prefix = 0);
    void WriteJump(Condition condition, int offset, BOOL hint = FALSE, BOOL branch_taken = TRUE, BOOL force_rel32 = FALSE);
};

#endif // ARCHITECTURE_IA32
#endif // ES_CODEGENERATOR_IA32_H
