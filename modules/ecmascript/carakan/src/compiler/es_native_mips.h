/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA 2011
 *
 * @author Fredrik Ohrn
 */

#ifndef ES_NATIVE_MIPS_H
#define ES_NATIVE_MIPS_H

#ifdef ES_NATIVE_SUPPORT
#ifdef ARCHITECTURE_MIPS

#include "modules/ecmascript/carakan/src/util/es_codegenerator_mips.h"

#define ARCHITECTURE_CAP_IMMEDIATE_INTEGER_OPERANDS 1
/**< MIPS instructions can have one integer immediate operand.  The range is more
     limited than the IA32 instruction's immediates, but we emulate full 32-bit
     immediates using scratch registers, to keep things simple (and to optimize
     register usage a bit, too.) */

//#define ARCHITECTURE_CAP_IMMEDIATE_DOUBLE_OPERANDS 1
/**< MIPS instructions can not have immediate double operands. */

//#define ARCHITECTURE_CAP_MEMORY_OPERANDS 1
/**< MIPS instructions can't have memory operands. */

#define ARCHITECTURE_CAP_INTEGER_DIVISION 1
/**< MIPS supports integer division. */

#define ARCHITECTURE_CAP_BINARY_EXPLICIT_TARGET_OPERAND
/**< All binary MIPS instructions have explicit target operands. */

#define ARCHITECTURE_CAP_UNARY_EXPLICIT_TARGET_OPERAND
/**< All unary MIPS instructions have explicit target operands. */

#define ARCHITECTURE_CAP_IMMEDIATE_SHIFT_COUNT
/**< MIPS shift instructions support immediate shift counts. */

#ifdef ES_DUMP_REGISTER_ALLOCATIONS
const char *const ARCHITECTURE_INTEGER_REGISTER_NAMES[] =
{
    "Z0", "AT", "V0", "V1", "A0", "A1", "A2", "A3", "T0", "T1", "T2", "T3", "T4", "T5", "T6", "T7",
    "S0", "S1", "S2", "S3", "S4", "S5", "S6", "S7", "T8", "T9", "K0", "K1", "GP", "SP", "FP", "RA"
};
const char *const ARCHITECTURE_DOUBLE_REGISTER_NAMES[] =
{
    "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15",
    "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "F25", "F26", "F27", "F28", "F29", "F30", "F31"
};
#endif // ES_DUMP_REGISTER_ALLOCATIONS

// The following enums *MUST* match the static initialization of the template arrays.

enum IntegerConstants
{
    CONST_ESTYPE_DOUBLE,
    CONST_ESTYPE_INT32_OR_DOUBLE,
    CONST_ESTYPE_INT32,
    CONST_ESTYPE_UNDEFINED,
    CONST_ESTYPE_NULL,
    CONST_ESTYPE_BOOLEAN,
    CONST_ESTYPE_BOXED,
    CONST_ESTYPE_STRING,
    CONST_ESTYPE_OBJECT,
    CONST_ES_VALUE_TYPE_INIT_MASK,
    CONST_INT_MAX,
    CONST_COPY_DATA_HANDLED,

    NUMBER_OF_INT_CONSTANTS
};

enum FunctionConstants
{
    CONST_ALLOCATESIMPLE,
    CONST_EQUALS,
    CONST_EXC_ALLOCATEOBJECTFROMMACHINECODECOMPLEX,
    CONST_EXC_ALLOCATEOBJECTFROMMACHINECODESIMPLE,
    CONST_EXC_CALLFROMMACHINECODE,
    CONST_EXC_CHECKOUTOFTIMEFROMMACHINECODE,
    CONST_EXC_CONSTRUCTFROMMACHINECODE,
    CONST_EXC_CREATEARGUMENTSOBJECT,
    CONST_EXC_DETACHARGUMENTSOBJECT,
    CONST_EXC_DETACHVARIABLEOBJECT,
    CONST_EXC_EQFROMMACHINECODE,
    CONST_EXC_EQSTRICT,
    CONST_EXC_EVALFROMMACHINECODE,
    CONST_EXC_EXECUTEBYTECODE,
    CONST_EXC_FORCEUPDATENATIVEDISPATCHER,
    CONST_EXC_GETGLOBAL,
    CONST_EXC_GETGLOBALIMMEDIATE,
    CONST_EXC_GETNAMEDIMMEDIATE,
    CONST_EXC_LIGHTWEIGHTDISPATCHERFAILURE,
    CONST_EXC_PUTGLOBAL,
    CONST_EXC_PUTNAMEDIMMEDIATE,
    CONST_EXC_READLOOPIO,
    CONST_EXC_STACKORREGISTERLIMITEXCEEDED,
    CONST_EXC_THROWFROMMACHINECODE,
    CONST_EXC_UPDATECOMPARISON,
    CONST_EXC_UPDATENATIVEDISPATCHER,
    CONST_EXC_WRITELOOPIO,
    CONST_ESN_COMPARESHIFTYOBJECTS,
    CONST_ESN_GETDOUBLEASSWITCHVALUE,
    CONST_ESN_INSTANCEOF,
    CONST_ESN_MATHABS,
    CONST_ESN_MATHCEIL,
    CONST_ESN_MATHCOS,
    CONST_ESN_MATHFLOOR,
    CONST_ESN_MATHPOW,
    CONST_ESN_MATHSIN,
    CONST_ESN_MATHSQRT,
    CONST_ESN_MATHTAN,
    CONST_ESN_PREPARETHISOBJECT,
    CONST_ESN_PREPARETHISOBJECTLIGHTWEIGHT,
    CONST_ESN_STOREDOUBLEASFLOAT,
    CONST_ESN_STOREFLOATASDOUBLE,
    CONST_ESN_STOREINTASDOUBLE,
    CONST_ESN_STOREINTASFLOAT,
    CONST_ESN_STOREUINTASDOUBLE,
    CONST_ESN_GETDOUBLEASINT,
    CONST_ESN_GETDOUBLEASINTROUND,
    CONST_ES_VALUE_INTERNAL_RETURNASBOOLEAN,

    NUMBER_OF_FUNCTION_CONSTANTS
};

enum PointerConstants
{
    CONST_CODE,
    CONST_GLOBAL_OBJECT,

    NUMBER_OF_POINTER_CONSTANTS
};

typedef void(*FuncPtr)();

class ES_ArchitecureMixin
{
public:
    ES_ArchitecureMixin(ES_Code *code) :
        fpu_type(ES_CodeGenerator::GetFPUType()),
        condition_register(ES_CodeGenerator::REG_ZERO),
        frame_size(UINT_MAX)
    {
        op_memset(int_constants, 0, sizeof(int_constants));
        op_memset(function_constants, 0, sizeof(function_constants));
        op_memset(pointer_constants, 0, sizeof(pointer_constants));

        pointer_const_template[CONST_CODE] = code;
        pointer_const_template[CONST_GLOBAL_OBJECT] = code->global_object;
    }

    ES_CodeGenerator::Constant *GetConst(IntegerConstants c);
    ES_CodeGenerator::Constant *GetConst(FunctionConstants c);
    ES_CodeGenerator::Constant *GetConst(PointerConstants c);

    ES_CodeGenerator::FPUType fpu_type;

protected:
    static int int_const_template[NUMBER_OF_INT_CONSTANTS];
    static FuncPtr function_const_template[NUMBER_OF_FUNCTION_CONSTANTS];
    // The pointer template is not static since it's unique to each JS function.
    void *pointer_const_template[NUMBER_OF_POINTER_CONSTANTS];

    ES_CodeGenerator::Constant *int_constants[NUMBER_OF_INT_CONSTANTS];
    ES_CodeGenerator::Constant *function_constants[NUMBER_OF_FUNCTION_CONSTANTS];
    ES_CodeGenerator::Constant *pointer_constants[NUMBER_OF_POINTER_CONSTANTS];

    // If condition_register is REG_Z check the FPU condition codes instead.
    ES_CodeGenerator::Register condition_register;

    unsigned frame_size;

    ES_CodeGenerator::JumpTarget *current_slow_case_main;
    /**< A jump target for jumping to the current slow case, bypassing the
         property value transfer flushing, or NULL if not applicable to the
         current slow case. */
};

#define ARCHITECTURE_CAP_FPU
#define ARCHITECTURE_HAS_FPU() (fpu_type != ES_CodeGenerator::FPU_NONE)

#endif // ARCHITECTURE_MIPS
#endif // ES_NATIVE_SUPPORT
#endif // ES_NATIVE_MIPS_H
