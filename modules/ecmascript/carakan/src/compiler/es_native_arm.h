/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA 2010
 *
 * @author Jens Lindstrom
 */

#ifndef ES_NATIVE_ARM_H
#define ES_NATIVE_ARM_H

#ifdef ES_NATIVE_SUPPORT
#ifdef ARCHITECTURE_ARM

#include "modules/ecmascript/carakan/src/util/es_codegenerator_arm.h"

#define ARCHITECTURE_CAP_IMMEDIATE_INTEGER_OPERANDS 1
/**< ARM instructions can have one integer immediate operand.  The range is more
     limited than the IA32 instruction's immediates, but we emulate full 32-bit
     immediates using scratch registers, to keep things simple (and to optimize
     register usage a bit, too.) */

//#define ARCHITECTURE_CAP_IMMEDIATE_DOUBLE_OPERANDS 1
/**< ARM VFP instructions can not have immediate double operands. */

//#define ARCHITECTURE_CAP_MEMORY_OPERANDS 1
/**< ARM instructions can't have memory operands. */

#define ARCHITECTURE_CAP_BINARY_EXPLICIT_TARGET_OPERAND
/**< All binary ARM instructions have explicit target operands. */

#define ARCHITECTURE_CAP_UNARY_EXPLICIT_TARGET_OPERAND
/**< All unary ARM instructions have explicit target operands. */

#define ARCHITECTURE_CAP_IMMEDIATE_SHIFT_COUNT
/**< ARM shift instructions (MOV with a shifted operand) support immediate shift
     counts. */

/* Indeces into the ES_Native::native_registers array.  Arranged in the order we
   prefer registers to be used; registers we'd rather was used first and
   registers we'd rather was left alone last.  ESI is only available in
   specialized dispatchers (otherwise it holds the register frame pointer.)  ECX
   we prefer to have available to use as a shift count register, so it's far
   down as well. */
enum ARMRegisterIndex
{
    ARM_REGISTER_INDEX_R2,
    ARM_REGISTER_INDEX_R3,
    ARM_REGISTER_INDEX_R4,
    ARM_REGISTER_INDEX_R5,
    ARM_REGISTER_INDEX_R6,
    ARM_REGISTER_INDEX_R7,
    ARM_REGISTER_INDEX_R8,

    ARM_INTEGER_REGISTER_COUNT,

#ifdef ARCHITECTURE_ARM_VFP
    ARM_REGISTER_INDEX_D2 = ARM_INTEGER_REGISTER_COUNT,
    ARM_REGISTER_INDEX_D3,
    ARM_REGISTER_INDEX_D4,
    ARM_REGISTER_INDEX_D5,
    ARM_REGISTER_INDEX_D6,
    ARM_REGISTER_INDEX_D7,
    ARM_REGISTER_INDEX_D8,
    ARM_REGISTER_INDEX_D9,
    ARM_REGISTER_INDEX_D10,
    ARM_REGISTER_INDEX_D11,
    ARM_REGISTER_INDEX_D12,
    ARM_REGISTER_INDEX_D13,
    ARM_REGISTER_INDEX_D14,
    ARM_REGISTER_INDEX_D15,
#endif // ARCHITECTURE_ARM_VFP

    ARM_REGISTER_COUNT,
    ARM_DOUBLE_REGISTER_COUNT = ARM_REGISTER_COUNT - ARM_INTEGER_REGISTER_COUNT
};

/* Values for ES_Native::NativeRegister::register_code. */
#define ARM_REGISTER_CODE_R2 ES_CodeGenerator::REG_R2
#define ARM_REGISTER_CODE_R3 ES_CodeGenerator::REG_R3
#define ARM_REGISTER_CODE_R4 ES_CodeGenerator::REG_R4
#define ARM_REGISTER_CODE_R5 ES_CodeGenerator::REG_R5
#define ARM_REGISTER_CODE_R6 ES_CodeGenerator::REG_R6
#define ARM_REGISTER_CODE_R7 ES_CodeGenerator::REG_R7
#define ARM_REGISTER_CODE_R8 ES_CodeGenerator::REG_R8

#ifdef ARCHITECTURE_ARM_VFP
# define ARM_REGISTER_CODE_D2 ES_CodeGenerator::VFPREG_D2
# define ARM_REGISTER_CODE_D3 ES_CodeGenerator::VFPREG_D3
# define ARM_REGISTER_CODE_D4 ES_CodeGenerator::VFPREG_D4
# define ARM_REGISTER_CODE_D5 ES_CodeGenerator::VFPREG_D5
# define ARM_REGISTER_CODE_D6 ES_CodeGenerator::VFPREG_D6
# define ARM_REGISTER_CODE_D7 ES_CodeGenerator::VFPREG_D7
# define ARM_REGISTER_CODE_D8 ES_CodeGenerator::VFPREG_D8
# define ARM_REGISTER_CODE_D9 ES_CodeGenerator::VFPREG_D9
# define ARM_REGISTER_CODE_D10 ES_CodeGenerator::VFPREG_D10
# define ARM_REGISTER_CODE_D11 ES_CodeGenerator::VFPREG_D11
# define ARM_REGISTER_CODE_D12 ES_CodeGenerator::VFPREG_D12
# define ARM_REGISTER_CODE_D13 ES_CodeGenerator::VFPREG_D13
# define ARM_REGISTER_CODE_D14 ES_CodeGenerator::VFPREG_D14
# define ARM_REGISTER_CODE_D15 ES_CodeGenerator::VFPREG_D15
#endif // ARCHITECTURE_SSE

#ifdef ES_DUMP_REGISTER_ALLOCATIONS
const char *const ARCHITECTURE_INTEGER_REGISTER_NAMES[] = { "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8" };
# ifdef ARCHITECTURE_ARM_VFP
const char *const ARCHITECTURE_DOUBLE_REGISTER_NAMES[] = { "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "D10", "D11", "D12", "D13", "D14", "D15" };
#endif // ARCHITECTURE_ARM_VFP
#endif // ES_DUMP_REGISTER_ALLOCATIONS

class ES_ArchitecureMixin
{
public:
    ES_ArchitecureMixin(ES_Code *)
        : vfp_supported(ES_CodeGenerator::SupportsVFP()),
          c_global_object(NULL),
          c_code(NULL),
          c_codewords(NULL),
          c_ih(NULL),
          c_CallFromMachineCode(NULL),
          c_ConstructFromMachineCode(NULL),
          c_EvalFromMachineCode(NULL),
          c_ThrowFromMachineCode(NULL),
          c_GetNamedImmediate(NULL),
          c_PutNamedImmediate(NULL),
          c_GetGlobalImmediate(NULL),
          c_GetGlobal(NULL),
          c_PutGlobal(NULL),
          c_ReadLoopIO(NULL),
          c_WriteLoopIO(NULL),
          c_UpdateNativeDispatcher(NULL),
          c_ForceUpdateNativeDispatcher(NULL),
          c_ReturnAsBoolean(NULL),
          c_CompareShiftyObjects(NULL),
          c_Equals(NULL),
          c_UpdateComparison(NULL),
          c_EqStrict(NULL),
          c_EqFromMachineCode(NULL),
          c_ExecuteBytecode(NULL),
          c_MathPow(NULL),
          c_MathSin(NULL),
          c_MathCos(NULL),
          check_out_of_time(NULL)
    {
    }

    BOOL vfp_supported;

    ES_CodeGenerator::Constant *c_global_object;
    ES_CodeGenerator::Constant *c_code;
    ES_CodeGenerator::Constant *c_codewords;
    ES_CodeGenerator::Constant **c_ih;
    ES_CodeGenerator::Constant *c_CallFromMachineCode, *c_ConstructFromMachineCode, *c_EvalFromMachineCode, *c_ThrowFromMachineCode;
    ES_CodeGenerator::Constant *c_GetNamedImmediate, *c_PutNamedImmediate;
    ES_CodeGenerator::Constant *c_GetGlobalImmediate, *c_GetGlobal, *c_PutGlobal, *c_ReadLoopIO, *c_WriteLoopIO;
    ES_CodeGenerator::Constant *c_UpdateNativeDispatcher, *c_ForceUpdateNativeDispatcher;
    ES_CodeGenerator::Constant *c_ReturnAsBoolean, *c_CompareShiftyObjects, *c_Equals, *c_UpdateComparison;
    ES_CodeGenerator::Constant *c_EqStrict, *c_EqFromMachineCode;
    ES_CodeGenerator::Constant *c_ExecuteBytecode;
    ES_CodeGenerator::Constant *c_MathPow, *c_MathSin, *c_MathCos;

    ES_CodeGenerator::OutOfOrderBlock *check_out_of_time;

    unsigned StackSpaceAllocated();
    unsigned stack_space_allocated, stack_frame_padding;

    ES_CodeGenerator::Constant *GetInstructionHandler(ES_Instruction instruction);

    void LoadObjectOperand(unsigned source_index, ES_CodeGenerator::Register target);
    /**< Load the object pointer value from 'VR(source_index)' (which is assumed
         to be of known type, or having been type-checked,) or, if set, use the
         property value transfer members of ES_Native to load the object pointer
         directly from a property of another object. */

    void EmitAllocateObject(ES_Class *actual_klass, ES_Class *final_klass, unsigned property_count, unsigned *nindexed, ES_Compact_Indexed_Properties *representation, ES_CodeGenerator::JumpTarget *slow_case);

    void EmitInitProperty(ES_CodeWord *word, const ES_CodeGenerator::Register properties, unsigned index);

    void EmitFunctionCall(ES_CodeGenerator::Constant *function);

    void EmitTypeConversionHandlers(ES_CodeGenerator::Register source, ES_CodeGenerator::Register properties, unsigned offset, ES_CodeGenerator::JumpTarget *null_target, ES_CodeGenerator::JumpTarget *int_to_double_target);

    ES_CodeGenerator::JumpTarget *current_slow_case_main;
    /**< A jump target for jumping to the current slow case, bypassing the
         property value transfer flushing, or NULL if not applicable to the
         current slow case. */
};

#ifdef ARCHITECTURE_ARM_VFP
# define ARCHITECTURE_CAP_FPU
# define ARCHITECTURE_HAS_FPU() (vfp_supported)
#endif // ARCHITECTURE_ARM_VFP

#endif // ARCHITECTURE_ARM
#endif // ES_NATIVE_SUPPORT
#endif // ES_NATIVE_ARM_H
