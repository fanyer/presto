/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_CODEGENERATOR_ARM_H
#define ES_CODEGENERATOR_ARM_H

#if defined ARCHITECTURE_ARM || defined ARCHITECTURE_THUMB

typedef unsigned ES_NativeInstruction;

enum ES_NativeJumpCondition
{
    ES_NATIVE_CONDITION_EQUAL,
    ES_NATIVE_CONDITION_NOT_EQUAL,
    ES_NATIVE_CONDITION_HIGHER_OR_SAME, // unsigned
    ES_NATIVE_CONDITION_LOWER, // unsigned
    ES_NATIVE_CONDITION_NEGATIVE,
    ES_NATIVE_CONDITION_POSITIVE_OR_ZERO,
    ES_NATIVE_CONDITION_OVERFLOW,
    ES_NATIVE_CONDITION_NOT_OVERFLOW,
    ES_NATIVE_CONDITION_HIGHER, // unsigned
    ES_NATIVE_CONDITION_LOWER_OR_SAME, // unsigned
    ES_NATIVE_CONDITION_GREATER_OR_EQUAL, // signed
    ES_NATIVE_CONDITION_LESS, // signed
    ES_NATIVE_CONDITION_GREATER, // signed
    ES_NATIVE_CONDITION_LESS_OR_EQUAL, // signed
    ES_NATIVE_CONDITION_ALWAYS,
    ES_NATIVE_CONDITION_NEVER,

    ES_NATIVE_UNCONDITIONAL = ES_NATIVE_CONDITION_ALWAYS,
    ES_NATIVE_NOT_A_JUMP = -1
};

enum { ES_NATIVE_MAXIMUM_INSTRUCTION_LENGTH = 1 };

/* This is well below the FLDD limit of 256, but keeping the block size small
   will keep the constants more evenly dispersed in the code, and lessen the
   need to emit constants multiple times. */
#define ES_MAXIMUM_BLOCK_LENGTH 64

#include "modules/ecmascript/carakan/src/util/es_codegenerator_base.h"

#ifdef ARCHITECTURE_ARM
# define ARM_INLINE inline
#else // ARCHITECTURE_ARM
# define ARM_INLINE
#endif // ARCHITECTURE_ARM

class ES_CodeGenerator
    : public ES_CodeGenerator_Base
{
public:
    ES_CodeGenerator(OpMemGroup *arena = NULL);

#ifdef ARCHITECTURE_THUMB
    enum FullRegister
#else // ARCHITECTURE_THUMB
    enum Register
#endif // ARCHITECTURE_THUMB
    {
        REG_R0,
        REG_R1,
        REG_R2,
        REG_R3,
        REG_R4,
        REG_R5,
        REG_R6,
        REG_R7,
        REG_R8,
        REG_R9,
        REG_R10,
        REG_R11,
        REG_R12,
        REG_R13,
        REG_R14,
        REG_R15,

#ifdef ARCHITECTURE_ARM
        REG_R0L = REG_R0,
        REG_R1L = REG_R1,
        REG_R2L = REG_R2,
        REG_R3L = REG_R3,
        REG_R4L = REG_R4,
        REG_R5L = REG_R5,
        REG_R6L = REG_R6,
        REG_R7L = REG_R7,
#endif // ARCHITECTURE_ARM

        REG_SP = REG_R13,
        REG_LR = REG_R14,
        REG_PC = REG_R15
    };

#ifdef ARCHITECTURE_THUMB
    enum Register
    {
        REG_R0L,
        REG_R1L,
        REG_R2L,
        REG_R3L,
        REG_R4L,
        REG_R5L,
        REG_R6L,
        REG_R7L
    };
#else // ARCHITECTURE_THUMB
    typedef Register FullRegister;
#endif // ARCHITECTURE_THUMB

    enum ShiftType
    {
        SHIFT_LOGICAL_LEFT,
        SHIFT_LOGICAL_RIGHT,
        SHIFT_ARITHMETIC_RIGHT,
        SHIFT_ROTATE_RIGHT
    };

    enum SetConditionCodes
    {
        SET_CONDITION_CODES,
        UNSET_CONDITION_CODES
    };

    class Operand
    {
    public:
        Operand(Register base);
        Operand(Register base, Register shift_register, ShiftType shift_type = ES_CodeGenerator::SHIFT_LOGICAL_LEFT);
        Operand(Register base, unsigned shift_amount, ShiftType shift_type = ES_CodeGenerator::SHIFT_LOGICAL_LEFT);
        Operand(int immediate);

        BOOL is_register;
        union
        {
            struct
            {
                Register base;
                ShiftType shift_type;

                BOOL is_register_shift;
                union
                {
                    Register shift_register;
                    unsigned shift_amount;
                } u;
            } reg;
            struct
            {
                unsigned reduced_immediate;
            } imm;
        } u;

        static BOOL EncodeImmediate(int immediate, unsigned *reduced_immediate = NULL, BOOL *negated = NULL, BOOL *inverted = NULL);

    protected:
        Operand() : is_register(FALSE) {}
    };

    class NegOperand
        : public Operand
    {
    public:
        NegOperand(const Operand &other) : Operand(other), negated(FALSE) {}
        NegOperand(Register base) : Operand(base), negated(FALSE) {}
        NegOperand(Register base, Register shift_register, ShiftType shift_type = ES_CodeGenerator::SHIFT_LOGICAL_LEFT) : Operand(base, shift_register, shift_type), negated(FALSE) {}
        NegOperand(Register base, unsigned shift_amount, ShiftType shift_type = ES_CodeGenerator::SHIFT_LOGICAL_LEFT) : Operand(base, shift_amount, shift_type), negated(FALSE) {}
        NegOperand(int immediate);

        static BOOL EncodeImmediate(int immediate);

        BOOL negated;
    };

    class NotOperand
        : public Operand
    {
    public:
        NotOperand(const Operand &other) : Operand(other), inverted(FALSE) {}
        NotOperand(Register base) : Operand(base), inverted(FALSE) {}
        NotOperand(Register base, Register shift_register, ShiftType shift_type = ES_CodeGenerator::SHIFT_LOGICAL_LEFT) : Operand(base, shift_register, shift_type), inverted(FALSE) {}
        NotOperand(Register base, unsigned shift_amount, ShiftType shift_type = ES_CodeGenerator::SHIFT_LOGICAL_LEFT) : Operand(base, shift_amount, shift_type), inverted(FALSE) {}
        NotOperand(int immediate);

        static BOOL EncodeImmediate(int immediate);

        BOOL inverted;
    };

#ifdef DEBUG_ENABLE_OPASSERT
    void BKPT(unsigned short imm);
#endif // DEBUG_ENABLE_OPASSERT

    void BX(FullRegister src, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void BLX(FullRegister src, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    inline void NOP();

    inline void AND(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void EOR(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void SUB(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void RSB(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void ADD(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
#ifdef ARCHITECTURE_THUMB
    void ADD(FullRegister src, FullRegister dst);
#endif // ARCHITECTURE_THUMB
    ARM_INLINE void AddToSP(unsigned amount);
    ARM_INLINE void AddToSP(unsigned amount, Register dst);
    ARM_INLINE void AddToPC(unsigned amount, Register dst);
    inline void ADC(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void SBC(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void RSC(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void TST(Register op1, const Operand &op2, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void TEQ(Register op1, const Operand &op2, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void CMP(Register op1, const NegOperand &op2, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
#ifdef ARCHITECTURE_THUMB
    void CMP(FullRegister op1, FullRegister op2);
#endif // ARCHITECTURE_THUMB
    inline void CMN(Register op1, const Operand &op2, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void ORR(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void MOV(              const NotOperand &op2, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
#ifdef ARCHITECTURE_THUMB
    void MOV(FullRegister src, FullRegister dst);
#endif // ARCHITECTURE_THUMB
    inline void BIC(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void MVN(              const Operand &op2, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    inline void MUL(Register op1, Register op2, Register dst,               SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void MLA(Register op1, Register op2, Register acc, Register dst, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

   void SMULL(Register op1, Register op2, Register dsthigh, Register dstlow, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
   void UMULL(Register op1, Register op2, Register dsthigh, Register dstlow, SetConditionCodes set_condition_codes = UNSET_CONDITION_CODES, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    enum LoadStoreType
    {
        LOAD_STORE_WORD,
        LOAD_STORE_UNSIGNED_BYTE,
        LOAD_STORE_SIGNED_BYTE,
        LOAD_STORE_HALF_WORD,
        LOAD_STORE_DOUBLE_WORD,
        LOAD_STORE_DOUBLE
    };

    static inline BOOL SupportedOffset(int offset, LoadStoreType type = LOAD_STORE_WORD);

    inline void LDRB(Register base, int offset, Register dst, BOOL write_back = FALSE, BOOL pre_indexing = TRUE, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void LDRB(Register base, Register offset, ShiftType shift_type, unsigned shift_amount, BOOL up, Register dst, BOOL write_back = FALSE, BOOL pre_indexing = TRUE, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
           void LDRSB(Register base, int offset, Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
           void LDRSB(Register base, Register offset, BOOL up, Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
           void LDRH(Register base, int offset, Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
           void LDRH(Register base, Register offset, BOOL up, Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
           void LDRSH(Register base, int offset, Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
           void LDRSH(Register base, Register offset, BOOL up, Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void LDR(Register base, int offset, Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void LDR(Register base, int offset, Register dst, BOOL write_back, BOOL pre_indexing, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void LDR(Register base, Register offset, ShiftType shift_type, unsigned shift_amount, BOOL up, Register dst, BOOL write_back = FALSE, BOOL pre_indexing = TRUE, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
           void LDR(Constant *constant, Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
           void LDRD(Register base, int offset, Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void STRB(Register src, Register base, int offset = 0, BOOL write_back = FALSE, BOOL pre_indexing = TRUE, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void STRB(Register src, Register base, Register offset, ShiftType shift_type, unsigned shift_amount, BOOL up, BOOL write_back = FALSE, BOOL pre_indexing = TRUE, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void STRH(Register src, Register base, int offset = 0, BOOL write_back = FALSE, BOOL pre_indexing = TRUE, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void STRH(Register src, Register base, Register offset, BOOL up, BOOL write_back = FALSE, BOOL pre_indexing = TRUE, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void STR(Register src, Register base, int offset, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void STR(Register src, Register base, int offset, BOOL write_back, BOOL pre_indexing, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void STR(Register src, Register base, Register offset, ShiftType shift_type, unsigned shift_amount, BOOL up, BOOL write_back = FALSE, BOOL pre_indexing = TRUE, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
           void STRD(Register src, Register base, int offset, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    inline void LDM(Register base, unsigned registers, BOOL write_back = FALSE, BOOL up = TRUE, BOOL pre_indexing = FALSE, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
           void LDM(Constant *constant, unsigned registers, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline void STM(Register base, unsigned registers, BOOL write_back = FALSE, BOOL up = TRUE, BOOL pre_indexing = FALSE, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    inline     void PUSH(Register src, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    inline     void POP(Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    ARM_INLINE void PUSH(unsigned registers, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    ARM_INLINE void POP(unsigned registers, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    void MRS(Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void MSR(Register src, BOOL control, BOOL extensions, BOOL status, BOOL flags, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    void MCRR(unsigned coproc, unsigned opcode, Register Rd, Register Rn, unsigned CRm, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    static BOOL SupportsVFP();

#ifdef ARCHITECTURE_ARM_VFP
    enum VFPSingleRegister
    {
        VFPREG_S0, VFPREG_S1,
        VFPREG_S2, VFPREG_S3,
        VFPREG_S4, VFPREG_S5,
        VFPREG_S6, VFPREG_S7,
        VFPREG_S8, VFPREG_S9,
        VFPREG_S10, VFPREG_S11,
        VFPREG_S12, VFPREG_S13,
        VFPREG_S14, VFPREG_S15,
        VFPREG_S16, VFPREG_S17,
        VFPREG_S18, VFPREG_S19,
        VFPREG_S20, VFPREG_S21,
        VFPREG_S22, VFPREG_S23,
        VFPREG_S24, VFPREG_S25,
        VFPREG_S26, VFPREG_S27,
        VFPREG_S28, VFPREG_S29,
        VFPREG_S30, VFPREG_S31
    };
    enum VFPDoubleRegister
    {
        VFPREG_D0,
        VFPREG_D1,
        VFPREG_D2,
        VFPREG_D3,
        VFPREG_D4,
        VFPREG_D5,
        VFPREG_D6,
        VFPREG_D7,
        VFPREG_D8,
        VFPREG_D9,
        VFPREG_D10,
        VFPREG_D11,
        VFPREG_D12,
        VFPREG_D13,
        VFPREG_D14,
        VFPREG_D15
    };
    enum VFPSystemRegister
    {
        VFPSID,
        VFPSCR,
        VFPEXC
    };

    void FLDS(Register base, int offset, VFPSingleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FSTS(VFPSingleRegister dst, Register base, int offset, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FLDD(Register base, int offset, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FLDD(Constant *constant, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FSTD(VFPDoubleRegister dst, Register base, int offset, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    void FMSR(Register src, VFPSingleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FMRS(VFPSingleRegister src, Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FMRX(VFPSystemRegister src, Register dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FMXR(Register src, VFPSystemRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FMDRR(Register high, Register low, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FMRRD(VFPDoubleRegister src, Register high, Register low, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    void FCPYD(VFPDoubleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FNEGD(VFPDoubleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FABSD(VFPDoubleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    void FADDD(VFPDoubleRegister lhs, VFPDoubleRegister rhs, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FSUBD(VFPDoubleRegister lhs, VFPDoubleRegister rhs, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FMULD(VFPDoubleRegister lhs, VFPDoubleRegister rhs, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FDIVD(VFPDoubleRegister lhs, VFPDoubleRegister rhs, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    void FCVTDS(VFPSingleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FCVTSD(VFPDoubleRegister src, VFPSingleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FTOSID(VFPDoubleRegister src, VFPSingleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FTOSIZD(VFPDoubleRegister src, VFPSingleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FSITOD(VFPSingleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FSITOS(VFPSingleRegister src, VFPSingleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    void FUITOD(VFPSingleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    void FSQRTD(VFPDoubleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);

    void FCMPD(VFPDoubleRegister lhs, VFPDoubleRegister rhs, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FCMPZD(VFPDoubleRegister lhs, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void FMSTAT(ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
#endif // ARCHITECTURE_ARM_VFP

    virtual const OpExecMemory *GetCode(OpExecMemoryManager *execmem, void *constant_reg = NULL);

#ifdef NATIVE_DISASSEMBLER_SUPPORT
    void EnableDisassemble() { disassemble = TRUE; }
    BOOL DisassembleEnabled();

    void DumpCodeVector() { dump_cv = TRUE; }
#endif // NATIVE_DISASSEMBLER_SUPPORT

    /** Allocate and return a constant of type pointer-to-function. */
    Constant *NewFunction(void (*p)())
    {
#ifdef EPOC // Brew/RVCT too ?
        /* The RVCT compiler can't cope with reinterpret_cast<void*> on a
         * function pointer (although it can reinterpret_cast between distinct
         * function pointer types); this function exists to code round that
         * limitation while still leaving it possible to find all the places
         * that casts happen.  Would ideally be #if'd on a define specific to
         * the RVCT compiler, but there doesn't appear to be one.  We could
         * define our own (c.f. the ADS12 define) but we don't yet.
         */
        return NewConstant((void*)p);
#else
        return NewConstant(reinterpret_cast<void *>(p));
#endif
    }


private:
    enum DataProcessingOpCode { OPCODE_AND, OPCODE_EOR, OPCODE_SUB, OPCODE_RSB, OPCODE_ADD, OPCODE_ADC,
                                OPCODE_SBC, OPCODE_RSC, OPCODE_TST, OPCODE_TEQ, OPCODE_CMP, OPCODE_CMN,
                                OPCODE_ORR, OPCODE_MOV, OPCODE_BIC, OPCODE_MVN };

    void DataProcessing(DataProcessingOpCode opcode, Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition);

    void Multiply(BOOL accumulate, Register op1, Register op2, Register acc, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition);

    enum OperandSize { OPSIZE_8 = 1, OPSIZE_16 = 2, OPSIZE_32 = 4 };

    void SingleDataTransfer(BOOL load, BOOL write_back, OperandSize opsize, BOOL up, BOOL pre_indexing, Register base, BOOL register_offset, unsigned offset_imm, Register offset_reg, ShiftType offset_shift_type, unsigned offset_shift_amount, Register src_dst, ES_NativeJumpCondition condition);
    void BlockDataTransfer(BOOL load, BOOL write_back, BOOL up, BOOL pre_indexing, Register base, unsigned registers, ES_NativeJumpCondition condition = ES_NATIVE_CONDITION_ALWAYS);
    void HalfWordDataTransfer(BOOL load, BOOL write_back, BOOL up, BOOL pre_indexing, Register base, BOOL register_offset, unsigned offset_imm, Register offset_reg, Register src_dst, ES_NativeJumpCondition condition);

#ifdef NATIVE_DISASSEMBLER_SUPPORT
    BOOL disassemble, dump_cv;
#endif // NATIVE_DISASSEMBLER_SUPPORT

    unsigned maximum_constant_range;

    BOOL OutputConstant(Constant *constant, unsigned *base, unsigned *&current, BOOL include_addressed);
};

#include "modules/ecmascript/carakan/src/util/es_codegenerator_arm_inlines.h"

#endif // ARCHITECTURE_ARM || ARCHITECTURE_THUMB
#endif // ES_CODEGENERATOR_ARM_H
