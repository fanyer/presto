/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#ifndef ES_NATIVE_IA32_H
#define ES_NATIVE_IA32_H

#ifdef ARCHITECTURE_IA32
#ifdef ES_NATIVE_SUPPORT

#include "modules/ecmascript/carakan/src/util/es_codegenerator_ia32.h"

#define ARCHITECTURE_CAP_IMMEDIATE_INTEGER_OPERANDS 1
/**< IA32 instructions can have one 32-bit integer immediate operand. */
//#define ARCHITECTURE_CAP_IMMEDIATE_DOUBLE_OPERANDS 1
/**< IA32 instructions can not have immediate double operands. */

#define ARCHITECTURE_CAP_MEMORY_OPERANDS 1
/**< IA32 instructions can typically have one memory operand.  */

#define ARCHITECTURE_CAP_INTEGER_DIVISION 1
/**< IA32 supports integer division. */

//#define ARCHITECTURE_CAP_BINARY_EXPLICIT_TARGET_OPERAND
/**< IA32 binary arithmetic instructions never have an explicit target operand;
     the target operand always doubles as a source operand. */

//#define ARCHITECTURE_CAP_UNARY_EXPLICIT_TARGET_OPERAND
/**< IA32 binary arithmetic instructions never have an explicit target operand;
     the target operand always doubles as the source operand. */

//#define ARCHITECTURE_CAP_MULTIPLICATION_MEMORY_TARGET_OPERAND
/**< The IMUL instruction's destination operand must be a register. */

#define ARCHITECTURE_SHIFT_COUNT_REGISTER IA32_REGISTER_INDEX_CX
#define ARCHITECTURE_CAP_IMMEDIATE_SHIFT_COUNT

/* Indeces into the ES_Native::native_registers array.  Arranged in the order we
   prefer registers to be used; registers we'd rather was used first and
   registers we'd rather was left alone last.  ESI is only available in
   specialized dispatchers (otherwise it holds the register frame pointer.)  ECX
   we prefer to have available to use as a shift count register, so it's far
   down as well. */
enum IA32RegisterIndex
{
    IA32_REGISTER_INDEX_AX,
    IA32_REGISTER_INDEX_DX,
    IA32_REGISTER_INDEX_DI,
    IA32_REGISTER_INDEX_SI,
#ifdef ARCHITECTURE_AMD64
    IA32_REGISTER_INDEX_R8,
    IA32_REGISTER_INDEX_R9,
    IA32_REGISTER_INDEX_R10,
    IA32_REGISTER_INDEX_R11,
    IA32_REGISTER_INDEX_R12,
    IA32_REGISTER_INDEX_R13,
#endif // ARCHITECTURE_AMD64
    IA32_REGISTER_INDEX_CX,

    /*
    IA32_REGISTER_INDEX_BX,
    IA32_REGISTER_INDEX_BP,
#ifdef ARCHITECTURE_AMD64
    IA32_REGISTER_INDEX_R14,
    IA32_REGISTER_INDEX_R15,
#endif // ARCHITECTURE_AMD64
    */

    IA32_INTEGER_REGISTER_COUNT,

#ifdef ARCHITECTURE_SSE
    IA32_REGISTER_INDEX_XMM0 = IA32_INTEGER_REGISTER_COUNT,
    IA32_REGISTER_INDEX_XMM1,
    IA32_REGISTER_INDEX_XMM2,
    IA32_REGISTER_INDEX_XMM3,
    IA32_REGISTER_INDEX_XMM4,
    IA32_REGISTER_INDEX_XMM5,
    IA32_REGISTER_INDEX_XMM6,
#ifdef ARCHITECTURE_AMD64
    IA32_REGISTER_INDEX_XMM7,
    IA32_REGISTER_INDEX_XMM8,
    IA32_REGISTER_INDEX_XMM9,
    IA32_REGISTER_INDEX_XMM10,
    IA32_REGISTER_INDEX_XMM11,
    IA32_REGISTER_INDEX_XMM12,
    IA32_REGISTER_INDEX_XMM13,
    IA32_REGISTER_INDEX_XMM14,
#endif // ARCHITECTURE_AMD64
#endif // ARCHITECTURE_SSE

    IA32_REGISTER_COUNT,
    IA32_DOUBLE_REGISTER_COUNT = IA32_REGISTER_COUNT - IA32_INTEGER_REGISTER_COUNT
};

/* Values for ES_Native::NativeRegister::register_code. */
#define IA32_REGISTER_CODE_AX ES_CodeGenerator::REG_AX
#define IA32_REGISTER_CODE_CX ES_CodeGenerator::REG_CX
#define IA32_REGISTER_CODE_DX ES_CodeGenerator::REG_DX
#define IA32_REGISTER_CODE_BX ES_CodeGenerator::REG_BX
#define IA32_REGISTER_CODE_SP ES_CodeGenerator::REG_SP
#define IA32_REGISTER_CODE_BP ES_CodeGenerator::REG_BP
#define IA32_REGISTER_CODE_SI ES_CodeGenerator::REG_SI
#define IA32_REGISTER_CODE_DI ES_CodeGenerator::REG_DI
#ifdef ARCHITECTURE_AMD64
# define IA32_REGISTER_CODE_R8 ES_CodeGenerator::REG_R8
# define IA32_REGISTER_CODE_R9 ES_CodeGenerator::REG_R9
# define IA32_REGISTER_CODE_R10 ES_CodeGenerator::REG_R10
# define IA32_REGISTER_CODE_R11 ES_CodeGenerator::REG_R11
# define IA32_REGISTER_CODE_R12 ES_CodeGenerator::REG_R12
# define IA32_REGISTER_CODE_R13 ES_CodeGenerator::REG_R13
# define IA32_REGISTER_CODE_R14 ES_CodeGenerator::REG_R14
# define IA32_REGISTER_CODE_R15 ES_CodeGenerator::REG_R15
#endif // ARCHITECTURE_AMD64

#ifdef ARCHITECTURE_SSE
# define IA32_REGISTER_CODE_XMM0 ES_CodeGenerator::REG_XMM0
# define IA32_REGISTER_CODE_XMM1 ES_CodeGenerator::REG_XMM1
# define IA32_REGISTER_CODE_XMM2 ES_CodeGenerator::REG_XMM2
# define IA32_REGISTER_CODE_XMM3 ES_CodeGenerator::REG_XMM3
# define IA32_REGISTER_CODE_XMM4 ES_CodeGenerator::REG_XMM4
# define IA32_REGISTER_CODE_XMM5 ES_CodeGenerator::REG_XMM5
# define IA32_REGISTER_CODE_XMM6 ES_CodeGenerator::REG_XMM6
# define IA32_REGISTER_CODE_XMM7 ES_CodeGenerator::REG_XMM7
#ifdef ARCHITECTURE_AMD64
#  define IA32_REGISTER_CODE_XMM8 ES_CodeGenerator::REG_XMM8
#  define IA32_REGISTER_CODE_XMM9 ES_CodeGenerator::REG_XMM9
#  define IA32_REGISTER_CODE_XMM10 ES_CodeGenerator::REG_XMM10
#  define IA32_REGISTER_CODE_XMM11 ES_CodeGenerator::REG_XMM11
#  define IA32_REGISTER_CODE_XMM12 ES_CodeGenerator::REG_XMM12
#  define IA32_REGISTER_CODE_XMM13 ES_CodeGenerator::REG_XMM13
#  define IA32_REGISTER_CODE_XMM14 ES_CodeGenerator::REG_XMM14
#  define IA32_REGISTER_CODE_XMM15 ES_CodeGenerator::REG_XMM15
#endif // ARCHITECTURE_AMD64
#endif // ARCHITECTURE_SSE

#define ARCHITECTURE_SPECIALIZED_ARGUMENTS_COUNT 2
const unsigned ARCHITECTURE_ARGUMENT_VALUE_REGISTERS[] = { IA32_REGISTER_INDEX_DI, IA32_REGISTER_INDEX_SI };

#ifdef ES_DUMP_REGISTER_ALLOCATIONS
#ifdef ARCHITECTURE_AMD64
const char *const ARCHITECTURE_INTEGER_REGISTER_NAMES[] = { "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI", "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15" };
const char *const ARCHITECTURE_DOUBLE_REGISTER_NAMES[] = { "XMM0", "XMM1", "XMM2", "XMM3", "XMM4", "XMM5", "XMM6", "XMM7", "XMM8", "XMM9", "XMM10", "XMM11", "XMM12", "XMM13", "XMM14", "XMM15" };
#else // ARCHITECTURE_AMD64
const char *const ARCHITECTURE_INTEGER_REGISTER_NAMES[] = { "EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI" };
const char *const ARCHITECTURE_DOUBLE_REGISTER_NAMES[] = { "XMM0", "XMM1", "XMM2", "XMM3", "XMM4", "XMM5", "XMM6", "XMM7" };
#endif // ARCHITECTURE_AMD64
#endif // ES_DUMP_REGISTER_ALLOCATIONS

#ifdef _STANDALONE
extern BOOL g_force_fpmode_x87;
#endif // _STANDALONE

#define ARCHITECTURE_CAP_FPU
#define ARCHITECTURE_HAS_FPU() (fpmode == FPMODE_SSE2)

#ifndef ARCHITECTURE_AMD64
# define IA32_X87_SUPPORT
#endif // ARCHITECTURE_AMD64

#define IA32_SSE2_SUPPORT

class ES_ArchitecureMixin
{
public:
    ES_ArchitecureMixin(ES_Code *)
#ifdef _STANDALONE
        : fpmode(ES_CodeGenerator::SupportsSSE2() && !g_force_fpmode_x87 ? FPMODE_SSE2 : FPMODE_X87),
#else // _STANDALONE
        : fpmode(ES_CodeGenerator::SupportsSSE2() ? FPMODE_SSE2 : FPMODE_X87),
#endif // _STANDALONE
          stack_space_allocated(0)
        , jump_to_continuation_point(FALSE)
#ifdef ARCHITECTURE_AMD64
        , negate_mask(NULL)
        , zero(NULL)
        , negative_zero(NULL)
        , one(NULL)
        , intmin(NULL)
        , intmax(NULL)
        , longmin(NULL)
        , longmax(NULL)
        , nan(NULL)
#ifdef ES_MAGIC_NUMBERS_DOUBLE2INT32
        , magic(NULL)
#endif // ES_MAGIC_NUMBERS_DOUBLE2INT32
#endif // ARCHITECTURE_AMD64
    {
    }

    enum FPMode { FPMODE_X87, FPMODE_SSE2 } fpmode;

    unsigned stack_space_allocated;
    BOOL jump_to_continuation_point;

    ES_CodeGenerator::Operand Zero(ES_CodeGenerator &cg);
    ES_CodeGenerator::Operand NegativeZero(ES_CodeGenerator &cg);
    ES_CodeGenerator::Operand One(ES_CodeGenerator &cg);
    ES_CodeGenerator::Operand IntMin(ES_CodeGenerator &cg);
    ES_CodeGenerator::Operand IntMax(ES_CodeGenerator &cg);
    ES_CodeGenerator::Operand LongMin(ES_CodeGenerator &cg);
    ES_CodeGenerator::Operand LongMax(ES_CodeGenerator &cg);
    ES_CodeGenerator::Operand NaN(ES_CodeGenerator &cg);
    ES_CodeGenerator::Operand UintMaxPlus1(ES_CodeGenerator &cg);

#ifdef ES_MAGIC_NUMBERS_DOUBLE2INT32
    ES_CodeGenerator::Operand Magic(ES_CodeGenerator &cg);
#endif // ES_MAGIC_NUMBERS_DOUBLE2INT32

#ifdef ARCHITECTURE_AMD64
    ES_CodeGenerator::Constant *negate_mask;
    ES_CodeGenerator::Constant *zero;
    ES_CodeGenerator::Constant *negative_zero;
    ES_CodeGenerator::Constant *one;
    ES_CodeGenerator::Constant *intmin;
    ES_CodeGenerator::Constant *intmax;
    ES_CodeGenerator::Constant *longmin;
    ES_CodeGenerator::Constant *longmax;
    ES_CodeGenerator::Constant *nan;
    ES_CodeGenerator::Constant *uintmax_plus_1;
#ifdef ES_MAGIC_NUMBERS_DOUBLE2INT32
    ES_CodeGenerator::Constant *magic;
#endif // ES_MAGIC_NUMBERS_DOUBLE2INT32
#endif // ARCHITECTURE_AMD64

    unsigned StackSpaceAllocated();

    void LoadObjectOperand(unsigned source_index, ES_CodeGenerator::Register target);
    /**< Load the object pointer value from 'VR(source_index)' (which is assumed
         to be of known type, or having been type-checked,) or, if set, use the
         property value transfer members of ES_Native to load the object pointer
         directly from a property of another object. */

    void EmitAllocateObject(ES_Class *actual_klass, ES_Class *final_klass, unsigned property_count, unsigned *nindexed, ES_Compact_Indexed_Properties *representation, ES_CodeGenerator::JumpTarget *slow_case);

#if defined IA32_X87_SUPPORT || defined IA32_SSE2_SUPPORT
    inline BOOL IsFPModeX87() const;
    inline BOOL IsFPModeSSE2() const;
#endif

    ES_CodeGenerator::JumpTarget *current_slow_case_main;
    /**< A jump target for jumping to the current slow case, bypassing the
         property value transfer flushing, or NULL if not applicable to the
         current slow case. */
};

#if defined IA32_X87_SUPPORT || defined IA32_SSE2_SUPPORT
inline BOOL
ES_ArchitecureMixin::IsFPModeX87() const
{
# if defined IA32_X87_SUPPORT && defined IA32_SSE2_SUPPORT
    return fpmode == FPMODE_X87;
# elif defined IA32_X87_SUPPORT
    return TRUE;
# elif defined IA32_SSE2_SUPPORT
    return FALSE;
# endif
}

inline BOOL
ES_ArchitecureMixin::IsFPModeSSE2() const
{
#if defined IA32_X87_SUPPORT && defined IA32_SSE2_SUPPORT
    return fpmode == FPMODE_SSE2;
#elif defined IA32_X87_SUPPORT
    return FALSE;
#elif defined IA32_SSE2_SUPPORT
    return TRUE;
#endif
}
#endif // defined IA32_X87_SUPPORT || defined IA32_SSE2_SUPPORT

#if defined IA32_X87_SUPPORT || defined IA32_SSE2_SUPPORT
# define IS_FPMODE_X87 IsFPModeX87()
# define IS_FPMODE_SSE2 IsFPModeSSE2()
#endif
#if defined IA32_X87_SUPPORT && defined IA32_SSE2_SUPPORT
# ifdef _STANDALONE
#  define IS_FPMODE_X87_STATIC (!ES_CodeGenerator::SupportsSSE2() || g_force_fpmode_x87)
# else // _STANDALONE
#  define IS_FPMODE_X87_STATIC !ES_CodeGenerator::SupportsSSE2()
# endif // _STANDALONE
#endif

#endif // ES_NATIVE_SUPPORT
#endif // ARCHITECTURE_IA32
#endif // ES_NATIVE_IA32_H
