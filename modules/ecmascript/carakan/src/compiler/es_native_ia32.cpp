/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002-2010
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */
#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/compiler/es_native.h"
#include "modules/ecmascript/carakan/src/compiler/es_instruction_data.h"
#include "modules/ecmascript/carakan/src/compiler/es_analyzer.h"

#ifdef ES_NATIVE_SUPPORT
#ifdef ARCHITECTURE_IA32

#if defined _MSC_VER
# define USE_FASTCALL_FOR_INSTRUCTION_HANDLERS
# define USE_FASTCALL_CALLING_CONVENTION
#endif // _MSC_VER

#define CONTEXT_POINTER ES_CodeGenerator::REG_BP
#define CONTEXT_POINTER_INDEX IA32_REGISTER_INDEX_BP

#define REGISTER_FRAME_POINTER ES_CodeGenerator::REG_BX
#define REGISTER_FRAME_POINTER_INDEX IA32_REGISTER_INDEX_BX

#ifdef ARCHITECTURE_AMD64
# define CODE_POINTER ES_CodeGenerator::REG_R15
# define CODE_POINTER_INDEX IA32_REGISTER_INDEX_R15

# define INSTRUCTION_HANDLERS_POINTER ES_CodeGenerator::REG_R14
# define INSTRUCTION_HANDLERS_POINTER_INDEX IA32_REGISTER_INDEX_R14
#endif // ARCHITECTURE_AMD64

/* The purpose of using different registers is mainly to make it convenient to
   generate code for BytecodeToNativeTrampoline() and for the call to
   ES_Execution_Context::StackOrRegisterLimitExceeded() in GeneratePrologue(). */
#ifdef ARCHITECTURE_AMD64_UNIX
# define PARAMETERS_COUNT ES_CodeGenerator::REG_CX
# define PARAMETERS_COUNT_INDEX IA32_REGISTER_INDEX_CX
#elif defined(ARCHITECTURE_AMD64_WINDOWS)
# define PARAMETERS_COUNT ES_CodeGenerator::REG_R9
# define PARAMETERS_COUNT_INDEX IA32_REGISTER_INDEX_R9
#else // ARCHITECTURE_AMD64
# define PARAMETERS_COUNT ES_CodeGenerator::REG_DI
# define PARAMETERS_COUNT_INDEX IA32_REGISTER_INDEX_DI
#endif // ARCHITECTURE_AMD64

#ifdef ES_VALUES_AS_NANS
# define VALUE_VALUE_OFFSET 0
# define VALUE_VALUE2_OFFSET 4
#else // ES_VALUES_AS_NANS
# define VALUE_VALUE_OFFSET 8
# define VALUE_VALUE2_OFFSET 12
#endif // ES_VALUES_AS_NANS

#define VALUE_INDEX_TO_OFFSET(index) (index) * sizeof(ES_Value_Internal)
#define VALUE_INDEX_MEMORY(ptr, index) ES_CodeGenerator::MEMORY(ptr, (index) * sizeof(ES_Value_Internal))

#define VALUE(ptr) ES_CodeGenerator::MEMORY(ptr)
#define VALUE_WITH_OFFSET(ptr, offset) ES_CodeGenerator::MEMORY(ptr, offset)

#define VALUE_TYPE(ptr) ES_CodeGenerator::MEMORY(ptr, VALUE_TYPE_OFFSET)
#define VALUE_VALUE(ptr) ES_CodeGenerator::MEMORY(ptr, VALUE_VALUE_OFFSET)
#define VALUE_VALUE2(ptr) ES_CodeGenerator::MEMORY(ptr, VALUE_VALUE2_OFFSET)

#define VR_WITH_OFFSET(offset) ES_CodeGenerator::MEMORY(REGISTER_FRAME_POINTER, offset)
#define VR_WITH_OFFSET_TYPE(offset) ES_CodeGenerator::MEMORY(REGISTER_FRAME_POINTER, offset + VALUE_TYPE_OFFSET)
#define VR_WITH_OFFSET_VALUE(offset) ES_CodeGenerator::MEMORY(REGISTER_FRAME_POINTER, offset + VALUE_VALUE_OFFSET)

#define VR_WITH_INDEX(index) ES_CodeGenerator::MEMORY(REGISTER_FRAME_POINTER, VALUE_INDEX_TO_OFFSET(index))
#define VR_WITH_INDEX_TYPE(index) ES_CodeGenerator::MEMORY(REGISTER_FRAME_POINTER, VALUE_INDEX_TO_OFFSET(index) + VALUE_TYPE_OFFSET)
#define VR_WITH_INDEX_VALUE(index) ES_CodeGenerator::MEMORY(REGISTER_FRAME_POINTER, VALUE_INDEX_TO_OFFSET(index) + VALUE_VALUE_OFFSET)

#define VALUE_TYPE_INDEX(ptr, index) ES_CodeGenerator::MEMORY(ptr, (index) * sizeof(ES_Value_Internal) + VALUE_TYPE_OFFSET)
#define VALUE_VALUE_INDEX(ptr, index) ES_CodeGenerator::MEMORY(ptr, (index) * sizeof(ES_Value_Internal) + VALUE_VALUE_OFFSET)
#define VALUE_VALUE2_INDEX(ptr, index) ES_CodeGenerator::MEMORY(ptr, (index) * sizeof(ES_Value_Internal) + VALUE_VALUE2_OFFSET)

/* Byte offset from REG_SP where stack-bound register values are located. The scratch space
   is only used when saving/restoring EFLAGS (EmitSaveCondition() / EmitRestoreCondition()). */
#ifdef ES_VALUES_AS_NANS
#define STACK_VALUE_OFFSET 8
#else
#define STACK_VALUE_OFFSET 0
#endif // ES_VALUES_AS_NANS

#define DECLARE_NOTHING() ES_DECLARE_NOTHING()
#define ES_OFFSETOF_SUBOBJECT(basecls, subcls) (static_cast<int>(reinterpret_cast<UINTPTR>(static_cast<basecls *>(reinterpret_cast<subcls *>(static_cast<UINTPTR>(0x100))))) - 0x100)
#define OBJECT_MEMBER(reg, cls, member) ES_CodeGenerator::MEMORY(reg, ES_OFFSETOF(cls, member))

static void
MovePointerIntoRegister(ES_CodeGenerator &cg, void *ptr, ES_CodeGenerator::Register reg)
{
    cg.MovePointerIntoRegister(ptr, reg);
}

static void
MovePointerInto(ES_CodeGenerator &cg, void *ptr, const ES_CodeGenerator::Operand &operand, ES_CodeGenerator::Register reg)
{
#ifdef ARCHITECTURE_AMD64
    cg.MovePointerIntoRegister(ptr, reg);
    cg.MOV(reg, operand, ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::IMMEDIATE(ptr), operand, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64
}

static void
StoreDoubleInValue(ES_Native *native, ES_CodeGenerator::XMM reg, ES_CodeGenerator::Register ptr, int offset)
{
    ES_CodeGenerator &cg = native->cg;

#ifdef ES_VALUES_AS_NANS
    ES_CodeGenerator::JumpTarget *is_number = cg.ForwardJump();

    cg.UCOMISD(reg, reg);
    cg.Jump(is_number, ES_NATIVE_CONDITION_NOT_PARITY, TRUE, TRUE);
    cg.MOVSD(native->NaN(cg), reg);
    cg.SetJumpTarget(is_number);
    cg.MOVSD(reg, ES_CodeGenerator::MEMORY(ptr, offset));
#else // ES_VALUES_AS_NANS
    cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), ES_CodeGenerator::MEMORY(ptr, offset + VALUE_TYPE_OFFSET));
    cg.MOVSD(reg, ES_CodeGenerator::MEMORY(ptr, offset + VALUE_VALUE_OFFSET));
#endif // ES_VALUES_AS_NANS
}

static void
StoreDoubleInRegister(ES_Native *native, ES_CodeGenerator::XMM reg, ES_Native::VirtualRegister *vr)
{
    StoreDoubleInValue(native, reg, REGISTER_FRAME_POINTER, VALUE_INDEX_TO_OFFSET(vr->index));
}

static void
ComparePointerToRegister(ES_CodeGenerator &cg, void *ptr, ES_CodeGenerator::Register reg)
{
#ifdef ARCHITECTURE_AMD64
    if (reinterpret_cast<UINTPTR>(ptr) <= INT_MAX)
        /* 32-bit operation sign-extends to 64 bits, so the operand must be less than INT_MAX. */
        cg.CMP(ES_CodeGenerator::IMMEDIATE(reinterpret_cast<UINTPTR>(ptr)), reg, ES_CodeGenerator::OPSIZE_64);
    else
    {
        ES_CodeGenerator::Constant *constant = cg.NewConstant(ptr);
        cg.CMP(ES_CodeGenerator::CONSTANT(constant), reg, ES_CodeGenerator::OPSIZE_64);
    }
#else // ARCHITECTURE_AMD64
    cg.CMP(ES_CodeGenerator::IMMEDIATE(ptr), reg, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64
}

static ES_CodeGenerator::Register
Native2Register(ES_Native::NativeRegister *native_register)
{
    OP_ASSERT(native_register->type == ES_Native::NativeRegister::TYPE_INTEGER);

    return static_cast<ES_CodeGenerator::Register>(native_register->register_code);
}

static ES_CodeGenerator::Operand
Native2Operand(ES_Native::NativeRegister *native_register)
{
    if (native_register->type == ES_Native::NativeRegister::TYPE_INTEGER)
        return ES_CodeGenerator::REGISTER(Native2Register(native_register));
    else
        return ES_CodeGenerator::XMM(static_cast<ES_CodeGenerator::XMM>(native_register->register_code));
}

#if 0
static ES_CodeGenerator::Register
Operand2Register(const ES_Native::Operand &operand)
{
    OP_ASSERT(operand.native_register != NULL);
    OP_ASSERT(operand.native_register->type == ES_Native::NativeRegister::TYPE_INTEGER);

    return static_cast<ES_CodeGenerator::Register>(operand.native_register->register_code);
}
#endif // 0

#ifdef ARCHITECTURE_SSE

static ES_CodeGenerator::XMM
Native2XMM(ES_Native::NativeRegister *nr)
{
    OP_ASSERT(nr->type == ES_Native::NativeRegister::TYPE_DOUBLE);

    return static_cast<ES_CodeGenerator::XMM>(nr->register_code);
}

static ES_CodeGenerator::XMM
Operand2XMM(const ES_Native::Operand &operand)
{
    OP_ASSERT(operand.native_register != NULL);

    return Native2XMM(operand.native_register);
}

#endif // ARCHITECTURE_SSE

static ES_CodeGenerator::Operand
REGISTER_TYPE(ES_Native::VirtualRegister *vr)
{
    OP_ASSERT(vr->stack_frame_offset == INT_MAX);
    return VALUE_TYPE_INDEX(REGISTER_FRAME_POINTER, vr->index);
}

/** Generate the operand for accessing the value of 'vr'. If 'saved_condition' holds,
 *  EFLAGS has been pushed onto the stack. Adjust offset to values stored on the
 *  stack accordingly.
 */
static ES_CodeGenerator::Operand
REGISTER_VALUE(ES_Native::VirtualRegister *vr, BOOL saved_condition = FALSE)
{
    if (vr->stack_frame_offset == INT_MAX)
        return VALUE_VALUE_INDEX(REGISTER_FRAME_POINTER, vr->index);
    else if (vr->stack_frame_offset == INT_MAX - 1)
        return Native2Operand(vr->stack_frame_register);
    else
    {
        int offset = 4 + (STACK_VALUE_OFFSET - (saved_condition ? sizeof(void *) : 0)) + vr->stack_frame_offset + (vr->stack_frame_type == ESTYPE_DOUBLE ? 4 : 0);
        return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, -offset);
    }
}

static ES_CodeGenerator::Operand
REGISTER_VALUE2(ES_Native::VirtualRegister *vr, BOOL saved_condition = FALSE)
{
    if (vr->stack_frame_offset == INT_MAX)
        return VALUE_VALUE2_INDEX(REGISTER_FRAME_POINTER, vr->index);
    else
    {
        OP_ASSERT(vr->stack_frame_offset != INT_MAX - 1);
        int offset = 0 + (STACK_VALUE_OFFSET - (saved_condition ? sizeof(void *) : 0)) + vr->stack_frame_offset + (vr->stack_frame_type == ESTYPE_DOUBLE ? 4 : 0);
        return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, -offset);
    }
}

static ES_CodeGenerator::Operand
Operand2Operand(const ES_Native::Operand &operand, BOOL double_op = FALSE)
{
    if (operand.native_register)
        return Native2Operand(operand.native_register);
    else if (operand.value && !double_op)
        return ES_CodeGenerator::IMMEDIATE(DOUBLE2INT32(*operand.value));
    else if (operand.codeword && !double_op)
        return ES_CodeGenerator::IMMEDIATE(operand.codeword->immediate);
    else
        return REGISTER_VALUE(operand.virtual_register);
}

static void
WriteHeader(ES_CodeGenerator &cg, unsigned bits, unsigned bytesize, ES_CodeGenerator::Register box, unsigned offset = 0)
{
    OP_ASSERT(bits <= UINT16_MAX);
    OP_ASSERT(bytesize <= UINT16_MAX);
    cg.MOV(ES_CodeGenerator::IMMEDIATE((bytesize << 16) | bits), ES_CodeGenerator::MEMORY(box, offset), ES_CodeGenerator::OPSIZE_32);
}

static ES_CodeGenerator::OutOfOrderBlock *
RecoverFromFailedPropertyValueTransfer(ES_Native *native, ES_Native::VirtualRegister *target_vr, ES_CodeGenerator::Register transfer_register, BOOL is_whatever)
{
    if (!native->is_light_weight)
    {
        ES_CodeGenerator &cg = native->cg;
        ES_CodeGenerator::OutOfOrderBlock *recover = cg.StartOutOfOrderBlock();

        cg.SetOutOfOrderContinuationPoint(native->current_slow_case);
        native->current_slow_case = NULL;

        native->property_value_fail = cg.ForwardJump();
        native->EmitRegisterTypeCheck(target_vr, ESTYPE_OBJECT, native->property_value_fail);

        int register_offset = static_cast<int>(VALUE_INDEX_TO_OFFSET(target_vr->index)) - static_cast<int>(native->property_value_offset);
        if (!is_whatever)
            register_offset += VALUE_VALUE_OFFSET;

        if (register_offset)
            cg.LEA(VR_WITH_OFFSET(register_offset), transfer_register, ES_CodeGenerator::OPSIZE_POINTER);
        else
            cg.MOV(REGISTER_FRAME_POINTER, transfer_register, ES_CodeGenerator::OPSIZE_POINTER);

        cg.EndOutOfOrderBlock();

        return recover;
    }
    else
        return NULL;
}

static void
ConvertDoubleToInt(ES_Native *native, ES_Native::VirtualRegister *source, ES_CodeGenerator::Register target, BOOL use_int32_range, BOOL do_truncate, ES_CodeGenerator::JumpTarget *slow_case)
{
    ES_CodeGenerator &cg = native->cg;

#ifdef IA32_X87_SUPPORT
    ES_CodeGenerator::JumpTarget *nontrivial = slow_case;
    ES_CodeGenerator::JumpTarget *trivial = cg.ForwardJump();

    if (native->IS_FPMODE_X87)
    {
        ES_CodeGenerator::JumpTarget *nontrivial1 = cg.ForwardJump();
        cg.FLD(REGISTER_VALUE(source));
        cg.FLD(native->IntMin(cg));
        cg.FUCOMI(1, TRUE);
        cg.Jump(nontrivial1, ES_NATIVE_CONDITION_CARRY, TRUE, FALSE);
        cg.FLD(native->IntMax(cg));
        cg.FUCOMI(1, TRUE);
        cg.Jump(trivial, ES_NATIVE_CONDITION_CARRY, TRUE, TRUE);
        cg.SetJumpTarget(nontrivial1);
        cg.FST(0, TRUE);
        cg.Jump(nontrivial);
        cg.SetJumpTarget(trivial);
        cg.FIST(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, -4), TRUE, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, -4), target, ES_CodeGenerator::OPSIZE_32);
    }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
    if (native->IS_FPMODE_SSE2)
    {
#ifdef ARCHITECTURE_AMD64
        ES_CodeGenerator::XMM xmm = ES_CodeGenerator::REG_XMM15;
#else // ARCHITECTURE_AMD64
        ES_CodeGenerator::XMM xmm = ES_CodeGenerator::REG_XMM7;
#endif // ARCHITECTURE_AMD64

        cg.MOVSD(REGISTER_VALUE(source), xmm);

#ifdef ES_MAGIC_NUMBERS_DOUBLE2INT32
        ES_CodeGenerator::OutOfOrderBlock *special_case = cg.StartOutOfOrderBlock();
        cg.MOV(ES_CodeGenerator::ZERO(), target, ES_CodeGenerator::OPSIZE_32);
        cg.EndOutOfOrderBlock();

        cg.UCOMISD(xmm, xmm);
        cg.ADDSD(xmm, xmm);
        cg.ADDSD(Magic(cg), xmm);
        cg.Jump(special_case->GetJumpTarget(), ES_NATIVE_CONDITION_PARITY, TRUE, FALSE);
        cg.MOVQ(xmm, target);
        cg.SAR(ES_CodeGenerator::IMMEDIATE(1), target, ES_CodeGenerator::OPSIZE_64);
        cg.SetOutOfOrderContinuationPoint(special_case);
#else // ES_MAGIC_NUMBERS_DOUBLE2INT32
#ifndef IA32_X87_SUPPORT
        ES_CodeGenerator::JumpTarget *nontrivial = slow_case;
        ES_CodeGenerator::JumpTarget *trivial = cg.ForwardJump();
#endif // IA32_X87_SUPPORT

        cg.UCOMISD(use_int32_range ? native->IntMin(cg) : native->LongMin(cg), xmm);
        cg.Jump(nontrivial, ES_NATIVE_CONDITION_CARRY, TRUE, FALSE); // This also catches "unordered", that is, NaN.

        cg.UCOMISD(use_int32_range ? native->IntMax(cg) : native->LongMax(cg), xmm);
        cg.Jump(trivial, ES_NATIVE_CONDITION_CARRY, TRUE, TRUE);
        cg.Jump(nontrivial, ES_NATIVE_CONDITION_NOT_ZERO, TRUE, FALSE);
        cg.SetJumpTarget(trivial);
        if (do_truncate)
#ifdef ARCHITECTURE_AMD64
            cg.CVTTSD2SI(xmm, target, ES_CodeGenerator::OPSIZE_64);
#else
            cg.CVTTSD2SI(xmm, target, ES_CodeGenerator::OPSIZE_32);
#endif
       else
#ifdef ARCHITECTURE_AMD64
            cg.CVTSD2SI(xmm, target, ES_CodeGenerator::OPSIZE_64);
#else
            cg.CVTSD2SI(xmm, target, ES_CodeGenerator::OPSIZE_32);
#endif
#endif // ES_MAGIC_NUMBERS_DOUBLE2INT32
    }
#endif // IA32_SSE2_SUPPORT
}

#ifdef ARCHITECTURE_AMD64

ES_CodeGenerator::Operand
ES_ArchitecureMixin::Zero(ES_CodeGenerator &cg)
{
    if (!zero)
        zero = cg.NewConstant(0.0);

    return ES_CodeGenerator::CONSTANT(zero);
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::NegativeZero(ES_CodeGenerator &cg)
{
    if (!negative_zero)
        negative_zero = cg.NewConstant(-0.0);

    return ES_CodeGenerator::CONSTANT(negative_zero);
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::One(ES_CodeGenerator &cg)
{
    if (!one)
        one = cg.NewConstant(1.0);

    return ES_CodeGenerator::CONSTANT(one);
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::IntMin(ES_CodeGenerator &cg)
{
    if (!intmin)
        intmin = cg.NewConstant(static_cast<double>(INT_MIN));

    return ES_CodeGenerator::CONSTANT(intmin);
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::IntMax(ES_CodeGenerator &cg)
{
    if (!intmax)
        intmax = cg.NewConstant(static_cast<double>(INT_MAX));

    return ES_CodeGenerator::CONSTANT(intmax);
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::LongMin(ES_CodeGenerator &cg)
{
    if (!longmin)
        longmin = cg.NewConstant(static_cast<double>(LONG_MIN));

    return ES_CodeGenerator::CONSTANT(longmin);
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::LongMax(ES_CodeGenerator &cg)
{
    if (!longmax)
        longmax = cg.NewConstant(static_cast<double>(LONG_MAX));

    return ES_CodeGenerator::CONSTANT(longmax);
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::NaN(ES_CodeGenerator &cg)
{
    if (!nan)
        nan = cg.NewConstant(op_nan(0));

    return ES_CodeGenerator::CONSTANT(nan);
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::UintMaxPlus1(ES_CodeGenerator &cg)
{
    if (!uintmax_plus_1)
        uintmax_plus_1 = cg.NewConstant(static_cast<double>(UINT_MAX) + 1);

    return ES_CodeGenerator::CONSTANT(uintmax_plus_1);
}

#ifdef ES_MAGIC_NUMBERS_DOUBLE2INT32

ES_CodeGenerator::Operand
ES_ArchitecureMixin::Magic(ES_CodeGenerator &cg)
{
    if (!magic)
        magic = cg.NewConstant(6755399441055744.0);

    return ES_CodeGenerator::CONSTANT(magic);
}

#endif // ES_MAGIC_NUMBERS_DOUBLE2INT32
#else // ARCHITECTURE_AMD64

const double g_ES_zero = 0.0;
const double g_ES_negative_zero = -0.0;
const double g_ES_one = 1.0;
const double g_ES_INT_MIN = INT_MIN;
const double g_ES_INT_MAX = INT_MAX;
const double g_ES_UINT_MAX = UINT_MAX;
const double g_ES_UINT_MAX_plus_1 = 1.0 + UINT_MAX;
const unsigned g_ES_NaN[2] = { 0, ESTYPE_DOUBLE };
#ifdef ES_MAGIC_NUMBERS_DOUBLE2INT32
const double g_ES_magic = 6755399441055744.0;
#endif // ES_MAGIC_NUMBERS_DOUBLE2INT32

ES_CodeGenerator::Operand
ES_ArchitecureMixin::Zero(ES_CodeGenerator &cg)
{
    return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(&g_ES_zero));
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::NegativeZero(ES_CodeGenerator &cg)
{
    return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(&g_ES_negative_zero));
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::One(ES_CodeGenerator &cg)
{
    return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(&g_ES_one));
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::IntMin(ES_CodeGenerator &cg)
{
    return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(&g_ES_INT_MIN));
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::IntMax(ES_CodeGenerator &cg)
{
    return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(&g_ES_INT_MAX));
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::LongMin(ES_CodeGenerator &cg)
{
    return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(&g_ES_INT_MIN));
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::LongMax(ES_CodeGenerator &cg)
{
    return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(&g_ES_INT_MAX));
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::NaN(ES_CodeGenerator &cg)
{
    return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(&g_ES_NaN));
}

ES_CodeGenerator::Operand
ES_ArchitecureMixin::UintMaxPlus1(ES_CodeGenerator &cg)
{
    return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(&g_ES_UINT_MAX_plus_1));
}

#ifdef ES_MAGIC_NUMBERS_DOUBLE2INT32

ES_CodeGenerator::Operand
ES_ArchitecureMixin::Magic(ES_CodeGenerator &cg)
{
    return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(&g_ES_magic));
}

#endif // ES_MAGIC_NUMBERS_DOUBLE2INT32
#endif // ARCHITECTURE_AMD64

#if defined USE_FASTCALL_CALLING_CONVENTION || defined ARCHITECTURE_AMD64
# define DEFAULT_USABLE_STACKSPACE 0
#else // USE_FASTCALL_CALLING_CONVENTION || ARCHITECTURE_AMD64
# define DEFAULT_USABLE_STACKSPACE (is_light_weight ? 0 : 8)
#endif // USE_FASTCALL_CALLING_CONVENTION || ARCHITECTURE_AMD64

#define IA32FUNCTIONCALL(arguments, function) IA32FunctionCall call(cg, arguments, reinterpret_cast<void (*)()>(&function), DEFAULT_USABLE_STACKSPACE)
#define IA32FUNCTIONCALLP(arguments, function) IA32FunctionCall call(cg, arguments, reinterpret_cast<void (*)()>(function), DEFAULT_USABLE_STACKSPACE)
#define IA32FUNCTIONCALLU(arguments, function, usable_stack_space) IA32FunctionCall call(cg, arguments, reinterpret_cast<void (*)()>(&function), usable_stack_space)
#define IA32FUNCTIONCALLR(arguments, usable_stack_space) IA32FunctionCall call(cg, arguments, NULL, usable_stack_space)

class IA32FunctionCall
{
public:
    IA32FunctionCall(ES_CodeGenerator &cg, const char *arguments, void (*fn)(), unsigned usable_stack_space)
        : cg(cg),
          arguments(arguments),
          fn(fn),
          stack_space(0),
          usable_stack_space(usable_stack_space),
#ifdef USE_FASTCALL_CALLING_CONVENTION
          stack_space_freed_by_callee(0),
#endif // USE_FASTCALL_CALLING_CONVENTION
          argc(0),
          offset(0)
    {
    }

    void AllocateStackSpace()
    {
        OP_ASSERT(op_strlen(arguments) <= 4);
#ifndef ARCHITECTURE_AMD64
# ifdef USE_FASTCALL_CALLING_CONVENTION
        const char *args = arguments + es_minu(op_strlen(arguments), 2);
# else // USE_FASTCALL_CALLING_CONVENTION
        const char *args = arguments;
# endif // USE_FASTCALL_CALLING_CONVENTION

        while (*args)
        {
            if (*args == 'p')
                stack_space += sizeof(void *);
            else
            {
                OP_ASSERT(*args == 'i' || *args == 'u');
                stack_space += sizeof(int);
            }

            ++args;
        }

# ifdef USE_FASTCALL_CALLING_CONVENTION
        stack_space_freed_by_callee = stack_space;
# endif // USE_FASTCALL_CALLING_CONVENTION

        if (stack_space > usable_stack_space)
        {
            while ((stack_space - usable_stack_space) & (ES_STACK_ALIGNMENT - 1))
                stack_space += sizeof(void *);

            cg.SUB(ES_CodeGenerator::IMMEDIATE(stack_space - usable_stack_space), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
        }
#endif // !ARCHITECTURE_AMD64
    }

    void AddArgument(const ES_CodeGenerator::Operand &op)
    {
        ES_CodeGenerator::OperandSize opsize = (arguments[argc] == 'p') ? ES_CodeGenerator::OPSIZE_POINTER : ES_CodeGenerator::OPSIZE_32;
#ifdef ARCHITECTURE_AMD64
        OP_ASSERT(arguments[argc] != 'u' || op.type != ES_CodeGenerator::Operand::TYPE_IMMEDIATE64);
#endif // ARCHITECTURE_AMD64
#ifdef ARCHITECTURE_AMD64_UNIX
        switch (argc++)
        {
        /* Note: ES_CodeGenerator eliminates the MOV if the source operand
                 equals the target operand.  Our caller sometimes uses that to
                 calculate the argument into the correct register, and then just
                 let us know that it's there. */
        case 0:
            cg.MOV(op, ES_CodeGenerator::REG_DI, opsize);
            break;
        case 1:
            OP_ASSERT(op.base != ES_CodeGenerator::REG_DI && op.index != ES_CodeGenerator::REG_DI);
            cg.MOV(op, ES_CodeGenerator::REG_SI, opsize);
            break;
        case 2:
            OP_ASSERT(op.base != ES_CodeGenerator::REG_DI && op.index != ES_CodeGenerator::REG_DI);
            OP_ASSERT(op.base != ES_CodeGenerator::REG_SI && op.index != ES_CodeGenerator::REG_SI);
            cg.MOV(op, ES_CodeGenerator::REG_DX, opsize);
            break;
        case 3:
            OP_ASSERT(op.base != ES_CodeGenerator::REG_DI && op.index != ES_CodeGenerator::REG_DI);
            OP_ASSERT(op.base != ES_CodeGenerator::REG_SI && op.index != ES_CodeGenerator::REG_SI);
            OP_ASSERT(op.base != ES_CodeGenerator::REG_DX && op.index != ES_CodeGenerator::REG_DX);
            cg.MOV(op, ES_CodeGenerator::REG_CX, opsize);
            break;
        default:
            OP_ASSERT(FALSE);
        }
#elif defined(ARCHITECTURE_AMD64_WINDOWS)
        switch (argc++)
        {
        /* Note: ES_CodeGenerator eliminates the MOV if the source operand
                 equals the target operand.  Our caller sometimes uses that to
                 calculate the argument into the correct register, and then just
                 let us know that it's there. */
        case 0:
            cg.MOV(op, ES_CodeGenerator::REG_CX, opsize);
            break;
        case 1:
            OP_ASSERT(op.base != ES_CodeGenerator::REG_CX && op.index != ES_CodeGenerator::REG_CX);
            cg.MOV(op, ES_CodeGenerator::REG_DX, opsize);
            break;
        case 2:
            OP_ASSERT(op.base != ES_CodeGenerator::REG_CX && op.index != ES_CodeGenerator::REG_CX);
            OP_ASSERT(op.base != ES_CodeGenerator::REG_DX && op.index != ES_CodeGenerator::REG_DX);
            cg.MOV(op, ES_CodeGenerator::REG_R8, opsize);
            break;
        case 3:
            OP_ASSERT(op.base != ES_CodeGenerator::REG_CX && op.index != ES_CodeGenerator::REG_CX);
            OP_ASSERT(op.base != ES_CodeGenerator::REG_DX && op.index != ES_CodeGenerator::REG_DX);
            OP_ASSERT(op.base != ES_CodeGenerator::REG_R8 && op.index != ES_CodeGenerator::REG_R8);
            cg.MOV(op, ES_CodeGenerator::REG_R9, opsize);
            break;
        default:
            OP_ASSERT(FALSE);
        }
#else // ARCHITECTURE_AMD64
# ifdef USE_FASTCALL_CALLING_CONVENTION
        if (argc == 0)
            cg.MOV(op, ES_CodeGenerator::REG_CX, opsize);
        else if (argc == 1)
        {
            OP_ASSERT(op.base != ES_CodeGenerator::REG_CX && op.index != ES_CodeGenerator::REG_CX);
            cg.MOV(op, ES_CodeGenerator::REG_DX, opsize);
        }
        else
# endif // USE_FASTCALL_CALLING_CONVENTION
        {
# ifdef USE_FASTCALL_CALLING_CONVENTION
            OP_ASSERT(op.base != ES_CodeGenerator::REG_CX && op.index != ES_CodeGenerator::REG_CX);
            OP_ASSERT(op.base != ES_CodeGenerator::REG_DX && op.index != ES_CodeGenerator::REG_DX);
# endif // USE_FASTCALL_CALLING_CONVENTION

            ES_CodeGenerator::Operand use_op;

            if (op.type == ES_CodeGenerator::Operand::TYPE_MEMORY)
            {
                use_op = ES_CodeGenerator::REG_AX;
                cg.MOV(op, use_op, opsize);
            }
            else
                use_op = op;

            cg.MOV(use_op, ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, offset), opsize);
            offset += opsize;
        }

        ++argc;
#endif // ARCHITECTURE_AMD64_UNIX
    }

    void Call(ES_CodeGenerator::Register reg = ES_CodeGenerator::REG_NONE)
    {
        if (fn)
            cg.CALL(fn);
        else
            cg.CALL(reg);
    }

    void FreeStackSpace(unsigned extra_space = 0)
    {
#ifdef USE_FASTCALL_CALLING_CONVENTION
        /* Callee frees stack space. */
        unsigned to_free = extra_space + (stack_space - stack_space_freed_by_callee);
#else // USE_FASTCALL_CALLING_CONVENTION
        unsigned to_free = (stack_space > usable_stack_space ? stack_space - usable_stack_space : 0) + extra_space;
#endif // USE_FASTCALL_CALLING_CONVENTION

        if (to_free != 0)
            cg.ADD(ES_CodeGenerator::IMMEDIATE(to_free), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    }

private:
    ES_CodeGenerator &cg;
    const char *arguments;
    void (*fn)();
    unsigned stack_space, usable_stack_space;
#ifdef USE_FASTCALL_CALLING_CONVENTION
    unsigned stack_space_freed_by_callee;
#endif // USE_FASTCALL_CALLING_CONVENTION
    unsigned argc, offset;
};

#ifdef DEBUG_SLOW_CASING
class ES_SuspendedPrintf : public ES_SuspendedCall
{
public:
    ES_SuspendedPrintf(ES_Execution_Context *context, const char *debug_text)
        : debug_text(debug_text)
    {
        context->SuspendedCall(this);
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        printf(debug_text);
    }

    const char *debug_text;
};

void ES_CALLING_CONVENTION
DoAnnotateSlowCase(ES_Execution_Context *context, const char *debug_text)
{
    ES_SuspendedPrintf call(context, debug_text);
}


static ES_CodeGenerator::JumpTarget *
AnnotateSlowCase(ES_CodeGenerator &cg, ES_CodeGenerator::JumpTarget *slow_case, const char *debug_text)
{
    ES_CodeGenerator::OutOfOrderBlock *new_slow_case_block = cg.StartOutOfOrderBlock(FALSE);

    cg.MovePointerIntoRegister((void*) debug_text, ES_CodeGenerator::REG_CX);

    IA32FUNCTIONCALL("pp", DoAnnotateSlowCase);

    call.AllocateStackSpace();
    call.AddArgument(CONTEXT_POINTER);
    call.AddArgument(ES_CodeGenerator::REG_CX);
    call.Call();
    call.FreeStackSpace();

    cg.Jump(slow_case);
    cg.EndOutOfOrderBlock();

    return new_slow_case_block->GetJumpTarget();
}


#define SLOW_CASE(target, str) AnnotateSlowCase(cg, target, str "\n")
#else
#define SLOW_CASE(target, str) target
#endif

static void
CopyValue(ES_CodeGenerator &cg, ES_CodeGenerator::Register source, unsigned source_offset, ES_CodeGenerator::Register target, unsigned target_offset, ES_CodeGenerator::Register scratch1, ES_CodeGenerator::Register scratch2 = ES_CodeGenerator::REG_NONE)
{
    if (scratch2 != ES_CodeGenerator::REG_NONE)
    {
#ifdef ES_VALUES_AS_NANS
        cg.MOV(ES_CodeGenerator::MEMORY(source, source_offset + VALUE_VALUE_OFFSET), scratch1, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::MEMORY(source, source_offset + VALUE_VALUE2_OFFSET), scratch2, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset + VALUE_VALUE_OFFSET), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(scratch2, ES_CodeGenerator::MEMORY(target, target_offset + VALUE_VALUE2_OFFSET), ES_CodeGenerator::OPSIZE_32);
#else // ES_VALUES_AS_NANS
        cg.MOV(ES_CodeGenerator::MEMORY(source, source_offset + VALUE_TYPE_OFFSET), scratch1, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::MEMORY(source, source_offset + VALUE_VALUE_OFFSET), scratch2, ES_CodeGenerator::OPSIZE_64);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset + VALUE_TYPE_OFFSET), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(scratch2, ES_CodeGenerator::MEMORY(target, target_offset + VALUE_VALUE_OFFSET), ES_CodeGenerator::OPSIZE_64);
#endif // ES_VALUES_AS_NANS
    }
    else
    {
#ifndef ES_VALUES_AS_NANS
        cg.MOV(ES_CodeGenerator::MEMORY(source, source_offset + VALUE_TYPE_OFFSET), scratch1, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset + VALUE_TYPE_OFFSET), ES_CodeGenerator::OPSIZE_32);
#endif // ES_VALUES_AS_NANS

#ifdef ARCHITECTURE_AMD64
        cg.MOV(ES_CodeGenerator::MEMORY(source, source_offset + VALUE_VALUE_OFFSET), scratch1, ES_CodeGenerator::OPSIZE_64);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset + VALUE_VALUE_OFFSET), ES_CodeGenerator::OPSIZE_64);
#else // ARCHITECTURE_AMD64
        cg.MOV(ES_CodeGenerator::MEMORY(source, source_offset + VALUE_VALUE_OFFSET), scratch1, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset + VALUE_VALUE_OFFSET), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::MEMORY(source, source_offset + VALUE_VALUE2_OFFSET), scratch1, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset + VALUE_VALUE2_OFFSET), ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64
    }
}

static void
CopyValue(ES_CodeGenerator &cg, ES_CodeGenerator::Register source, unsigned source_offset, ES_Native::VirtualRegister *target_vr, ES_CodeGenerator::Register scratch1, ES_CodeGenerator::Register scratch2 = ES_CodeGenerator::REG_NONE)
{
    return CopyValue(cg, source, source_offset, REGISTER_FRAME_POINTER, VALUE_INDEX_TO_OFFSET(target_vr->index), scratch1, scratch2);
}

static void
CopyValue(ES_CodeGenerator &cg, ES_Native::VirtualRegister *source_vr, ES_CodeGenerator::Register target, unsigned target_offset, ES_CodeGenerator::Register scratch1, ES_CodeGenerator::Register scratch2 = ES_CodeGenerator::REG_NONE)
{
    return CopyValue(cg, REGISTER_FRAME_POINTER, VALUE_INDEX_TO_OFFSET(source_vr->index), target, target_offset, scratch1, scratch2);
}

static void
CopyDataToValue(ES_CodeGenerator &cg, ES_CodeGenerator::Register source, ES_CodeGenerator::Register source_type, ES_Native::VirtualRegister *target_vr, ES_CodeGenerator::Register scratch)
{
    ES_CodeGenerator::OutOfOrderBlock *handle_untyped = cg.StartOutOfOrderBlock();
    CopyValue(cg, source, 0, target_vr, scratch, source_type);
    cg.EndOutOfOrderBlock();

#ifdef ES_VALUES_AS_NANS
    unsigned handled = (~(ES_STORAGE_BITS_DOUBLE | ES_STORAGE_BITS_WHATEVER) << ES_STORAGE_TYPE_SHIFT) & ES_STORAGE_TYPE_MASK;
#else
    /* ES_STORAGE_BITS_WHATEVER is 0, hence the only 'untyped'. */
    unsigned handled = ES_STORAGE_TYPE_MASK;
#endif // ES_VALUES_AS_NANS

    cg.TEST(ES_CodeGenerator::IMMEDIATE(handled), source_type, ES_CodeGenerator::OPSIZE_32);
    cg.Jump(handle_untyped->GetJumpTarget(), ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);
    ES_CodeGenerator::JumpTarget *non_null_target = cg.ForwardJump();

    ES_CodeGenerator::OutOfOrderBlock *handle_null = cg.StartOutOfOrderBlock();
    cg.TEST(scratch, scratch, ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(non_null_target, ES_NATIVE_CONDITION_NOT_ZERO);
    cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_NULL), VR_WITH_INDEX_TYPE(target_vr->index), ES_CodeGenerator::OPSIZE_32);
    cg.EndOutOfOrderBlock();

    cg.MOV(ES_CodeGenerator::MEMORY(source), scratch, ES_CodeGenerator::OPSIZE_POINTER);
    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_STORAGE_NULL_MASK), source_type, ES_CodeGenerator::OPSIZE_32);
    cg.Jump(handle_null->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_ZERO);

    cg.SetJumpTarget(non_null_target);

#ifdef ES_VALUES_AS_NANS
    cg.OR(ES_CodeGenerator::IMMEDIATE(ES_VALUE_TYPE_INIT_MASK), source_type, ES_CodeGenerator::OPSIZE_32);
#else
    cg.AND(ES_CodeGenerator::IMMEDIATE(ES_VALUE_TYPE_MASK), source_type, ES_CodeGenerator::OPSIZE_32);
#endif // ES_VALUES_AS_NANS
    cg.MOV(scratch, VR_WITH_INDEX_VALUE(target_vr->index), ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(source_type, VR_WITH_INDEX_TYPE(target_vr->index), ES_CodeGenerator::OPSIZE_32);

    cg.SetOutOfOrderContinuationPoint(handle_untyped);
    cg.SetOutOfOrderContinuationPoint(handle_null);
}

static void
CopyTypedDataToValue(ES_CodeGenerator &cg, ES_CodeGenerator::Register source, unsigned source_offset, ES_StorageType source_type, ES_CodeGenerator::Register target, unsigned target_offset, ES_CodeGenerator::Register scratch1, ES_CodeGenerator::Register scratch2 = ES_CodeGenerator::REG_NONE)
{
#ifdef ES_VALUES_AS_NANS
    if (source_type == ES_STORAGE_WHATEVER || source_type == ES_STORAGE_DOUBLE)
#else
    if (source_type == ES_STORAGE_WHATEVER)
#endif // ES_VALUES_AS_NANS
        CopyValue(cg, source, source_offset, target, target_offset, scratch1, scratch2);
    else
    {
        ES_CodeGenerator::OperandSize size = ES_Layout_Info::SizeOf(source_type) > 4 ? ES_CodeGenerator::OPSIZE_64 : ES_CodeGenerator::OPSIZE_32;

#ifndef ARCHITECTURE_AMD64
        OP_ASSERT(size == ES_CodeGenerator::OPSIZE_32 || source_type == ES_STORAGE_UNDEFINED);
#endif // ARCHITECTURE_AMD64

        if (source_type != ES_STORAGE_NULL && source_type != ES_STORAGE_UNDEFINED)
            cg.MOV(ES_CodeGenerator::MEMORY(source, source_offset), scratch1, size);

        ES_CodeGenerator::OutOfOrderBlock *null_block = NULL;

        if (ES_Layout_Info::IsNullable(source_type))
        {
            null_block = cg.StartOutOfOrderBlock();
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_NULL), ES_CodeGenerator::MEMORY(target, target_offset + VALUE_TYPE_OFFSET), ES_CodeGenerator::OPSIZE_32);
            cg.EndOutOfOrderBlock();

            cg.TEST(scratch1, scratch1, size);
            cg.Jump(null_block->GetJumpTarget(), ES_NATIVE_CONDITION_ZERO);
        }

        cg.MOV(ES_CodeGenerator::IMMEDIATE(ES_Value_Internal::ConvertToValueType(source_type)), ES_CodeGenerator::MEMORY(target, target_offset + VALUE_TYPE_OFFSET), ES_CodeGenerator::OPSIZE_32);

        if (source_type != ES_STORAGE_NULL && source_type != ES_STORAGE_UNDEFINED)
            cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset + VALUE_VALUE_OFFSET), size);

        if (null_block)
            cg.SetOutOfOrderContinuationPoint(null_block);
    }
}

static void
CopyTypedDataToValue(ES_CodeGenerator &cg, ES_CodeGenerator::Register source, unsigned source_offset, ES_StorageType source_type, ES_Native::VirtualRegister *target_vr, ES_CodeGenerator::Register scratch1, ES_CodeGenerator::Register scratch2 = ES_CodeGenerator::REG_NONE)
{
    return CopyTypedDataToValue(cg, source, source_offset, source_type, REGISTER_FRAME_POINTER, VALUE_INDEX_TO_OFFSET(target_vr->index), scratch1, scratch2);
}

static void
CopyValueToData2(ES_CodeGenerator &cg, ES_CodeGenerator::Register source, ES_CodeGenerator::Register target, unsigned target_offset,
                 ES_CodeGenerator::JumpTarget *size_4_target, ES_CodeGenerator::JumpTarget *size_8_target, ES_CodeGenerator::JumpTarget *size_16_target,
                 ES_CodeGenerator::Register scratch1, BOOL align = FALSE)
{
    OP_ASSERT(size_4_target || size_8_target || size_16_target);

    ES_CodeGenerator::JumpTarget *done_target = NULL;
#ifdef ARCHITECTURE_AMD64
    if (size_16_target)
    {
        cg.SetJumpTarget(size_16_target, align);
        align = FALSE;
        cg.MOV(ES_CodeGenerator::MEMORY(source, 8), scratch1, ES_CodeGenerator::OPSIZE_64);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset + 8), ES_CodeGenerator::OPSIZE_64);
    }
    if (size_8_target || size_16_target)
    {
        if (size_8_target)
        {
            cg.SetJumpTarget(size_8_target, align);
            align = FALSE;
        }

        cg.MOV(ES_CodeGenerator::MEMORY(source), scratch1, ES_CodeGenerator::OPSIZE_64);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset), ES_CodeGenerator::OPSIZE_64);
        if (size_4_target)
            cg.Jump(done_target ? done_target : (done_target = cg.ForwardJump()));
    }
    if (size_4_target)
    {
        cg.SetJumpTarget(size_4_target, align);
        cg.MOV(ES_CodeGenerator::MEMORY(source), scratch1, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset), ES_CodeGenerator::OPSIZE_32);
    }
#else
    if (size_4_target)
    {
        cg.SetJumpTarget(size_4_target, align);
        cg.MOV(ES_CodeGenerator::MEMORY(source), scratch1, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset), ES_CodeGenerator::OPSIZE_32);
        if (size_8_target)
            cg.Jump(done_target ? done_target : (done_target = cg.ForwardJump()));
    }
    if (size_8_target)
    {
        cg.SetJumpTarget(size_8_target, align);
        cg.MOV(ES_CodeGenerator::MEMORY(source), scratch1, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::MEMORY(source, 4), scratch1, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset + 4), ES_CodeGenerator::OPSIZE_32);
    }
#endif // ARCHITECTURE_AMD64

    if (done_target)
        cg.SetJumpTarget(done_target);
}

static void
CopyValueToData(ES_CodeGenerator &cg, ES_CodeGenerator::Register source, unsigned source_offset, ES_CodeGenerator::Register target, unsigned target_offset, ES_CodeGenerator::Register target_type, ES_CodeGenerator::Register scratch, ES_CodeGenerator::JumpTarget *slow_case_target)
{
    ES_CodeGenerator::OutOfOrderBlock *handle_untyped = cg.StartOutOfOrderBlock();
    CopyValue(cg, source, source_offset, target, target_offset, scratch, target_type);
    cg.EndOutOfOrderBlock();

#ifdef ES_VALUES_AS_NANS
    unsigned handled = (~(ES_STORAGE_BITS_DOUBLE | ES_STORAGE_BITS_WHATEVER) << ES_STORAGE_TYPE_SHIFT) & ES_STORAGE_TYPE_MASK;
#else
    unsigned handled = (~ES_STORAGE_BITS_WHATEVER << ES_STORAGE_TYPE_SHIFT) & ES_STORAGE_TYPE_MASK;
#endif // ES_VALUES_AS_NANS
    cg.TEST(ES_CodeGenerator::IMMEDIATE(handled), target_type, ES_CodeGenerator::OPSIZE_32);
    cg.Jump(handle_untyped->GetJumpTarget(), ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);

    cg.MOV(target_type, scratch, ES_CodeGenerator::OPSIZE_32);

    ES_CodeGenerator::JumpTarget *non_null = cg.ForwardJump(), *done_target = cg.ForwardJump();
    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_STORAGE_NULL_MASK), target_type, ES_CodeGenerator::OPSIZE_32);
    cg.Jump(non_null, ES_NATIVE_CONDITION_ZERO);
    cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_NULL), ES_CodeGenerator::MEMORY(source, source_offset + VALUE_TYPE_OFFSET), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(non_null, ES_NATIVE_CONDITION_NOT_EQUAL);
    cg.MOV(ES_CodeGenerator::ZERO(), ES_CodeGenerator::MEMORY(target, target_offset), ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(done_target);
    cg.SetJumpTarget(non_null);

#ifdef ES_VALUES_AS_NANS
    cg.OR(ES_CodeGenerator::IMMEDIATE(ES_VALUE_TYPE_INIT_MASK), scratch, ES_CodeGenerator::OPSIZE_32);
#else
    cg.AND(ES_CodeGenerator::IMMEDIATE(ES_VALUE_TYPE_MASK), scratch, ES_CodeGenerator::OPSIZE_32);
#endif // ES_VALUES_AS_NANS

    cg.CMP(scratch, ES_CodeGenerator::MEMORY(source, source_offset + VALUE_TYPE_OFFSET), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(slow_case_target, ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);

#ifdef ARCHITECTURE_AMD64
    ES_CodeGenerator::JumpTarget *jt_size_four = cg.ForwardJump();

    cg.TEST(ES_CodeGenerator::IMMEDIATE(4 << ES_STORAGE_SIZE_SHIFT), target_type, ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::MEMORY(source, source_offset + VALUE_VALUE_OFFSET), scratch, ES_CodeGenerator::OPSIZE_32);
    cg.MOV(scratch, ES_CodeGenerator::MEMORY(target, target_offset), ES_CodeGenerator::OPSIZE_32);
#ifdef ARCHITECTURE_AMD64
    cg.Jump(jt_size_four, ES_NATIVE_CONDITION_NOT_ZERO, TRUE, FALSE);

    cg.MOV(ES_CodeGenerator::MEMORY(source, source_offset + VALUE_VALUE_OFFSET + 4), scratch, ES_CodeGenerator::OPSIZE_32);
    cg.MOV(scratch, ES_CodeGenerator::MEMORY(target, target_offset + 4), ES_CodeGenerator::OPSIZE_32);
    cg.SetJumpTarget(jt_size_four);
#endif // ARCHITECTURE_AMD64

    cg.SetJumpTarget(done_target);
    cg.SetOutOfOrderContinuationPoint(handle_untyped);
}

static void
CopyValueToTypedData(ES_CodeGenerator &cg, ES_Native::VirtualRegister *source_vr, ES_CodeGenerator::Register target, unsigned target_offset, ES_StorageType target_type, ES_CodeGenerator::Register scratch1, ES_CodeGenerator::Register scratch2 = ES_CodeGenerator::REG_NONE)
{
#ifdef ES_VALUES_AS_NANS
    if (target_type == ES_STORAGE_WHATEVER || target_type == ES_STORAGE_DOUBLE)
#else
    if (target_type == ES_STORAGE_WHATEVER)
#endif // ES_VALUES_AS_NANS
        CopyValue(cg, source_vr, target, target_offset, scratch1, scratch2);
    else if (target_type == ES_STORAGE_NULL)
        cg.MOV(ES_CodeGenerator::ZERO(), ES_CodeGenerator::MEMORY(target, target_offset), ES_CodeGenerator::OPSIZE_POINTER);
    else if (target_type != ES_STORAGE_UNDEFINED)
    {
#ifdef ARCHITECTURE_AMD64
        ES_CodeGenerator::OperandSize size = ES_Layout_Info::SizeOf(target_type) == 4 ? ES_CodeGenerator::OPSIZE_32 : ES_CodeGenerator::OPSIZE_64;
#else // ARCHITECTURE_AMD64
        ES_CodeGenerator::OperandSize size = ES_CodeGenerator::OPSIZE_32;
#endif // ARCHITECTURE_AMD64
        cg.MOV(REGISTER_VALUE(source_vr), scratch1, size);
        cg.MOV(scratch1, ES_CodeGenerator::MEMORY(target, target_offset), size);
    }
}

void
ES_ArchitecureMixin::LoadObjectOperand(unsigned source_index, ES_CodeGenerator::Register target)
{
    ES_Native *self = static_cast<ES_Native *>(this);
    ES_Native::VirtualRegister *source = self->VR(source_index);

    if (self->property_value_read_vr && self->property_value_nr)
    {
        OP_ASSERT(self->property_value_read_vr == source);
        unsigned value_offset = self->property_value_needs_type_check ? VALUE_VALUE_OFFSET : 0;
        self->cg.MOV(ES_CodeGenerator::MEMORY(Native2Register(self->property_value_nr), self->property_value_offset + value_offset), target, ES_CodeGenerator::OPSIZE_POINTER);
    }
    else
        self->cg.MOV(REGISTER_VALUE(source), target, ES_CodeGenerator::OPSIZE_POINTER);
}

ES_Boxed *ES_CALLING_CONVENTION
AllocateSimple(ES_Heap *heap, ES_Context *context, unsigned nbytes) REALIGN_STACK;

ES_Boxed *ES_CALLING_CONVENTION
AllocateSimple(ES_Heap *heap, ES_Context *context, unsigned nbytes)
{
    return heap->AllocateSimple(context, nbytes);
}

void
ES_ArchitecureMixin::EmitAllocateObject(ES_Class *actual_klass, ES_Class *final_klass, unsigned property_count, unsigned *nindexed, ES_Compact_Indexed_Properties *representation, ES_CodeGenerator::JumpTarget *slow_case)
{
    OP_ASSERT(!actual_klass->HasHashTableProperties());
    DECLARE_NOTHING();

    ES_Native *self = static_cast<ES_Native *>(this);
    ES_CodeGenerator &cg = self->cg;

    ES_Native::ObjectAllocationData data;

    self->GetObjectAllocationData(data, actual_klass, final_klass, property_count, nindexed, representation);

    OP_ASSERT(data.named_bytes >= sizeof(ES_Box) + (final_klass ? final_klass : actual_klass)->SizeOf(property_count));

    unsigned nbytes = data.main_bytes + data.named_bytes + data.indexed_bytes;

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register heap(ES_CodeGenerator::REG_CX);
    ES_CodeGenerator::Register current_top(ES_CodeGenerator::REG_DX);
    ES_CodeGenerator::Register properties(ES_CodeGenerator::REG_DI);
    ES_CodeGenerator::Register indexed_properties(ES_CodeGenerator::REG_SI);

    ES_CodeGenerator::OutOfOrderBlock *simple_allocation = cg.StartOutOfOrderBlock();

    {
#if !defined(USE_FASTCALL_CALLING_CONVENTION) && !defined(ARCHITECTURE_AMD64)
        BOOL is_light_weight = self->is_light_weight;
#endif // !USE_FASTCALL_CALLING_CONVENTION && !ARCHITECTURE_AMD64
        IA32FUNCTIONCALL("ppu", AllocateSimple);

        call.AllocateStackSpace();
        call.AddArgument(heap);
        call.AddArgument(CONTEXT_POINTER);
        call.AddArgument(ES_CodeGenerator::IMMEDIATE(nbytes));
        call.Call();
        call.FreeStackSpace();

        cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
        cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO);
        cg.EndOutOfOrderBlock();

        slow_case = simple_allocation->GetJumpTarget();
    }

    cg.MOV(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, heap), heap, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(OBJECT_MEMBER(heap, ES_Heap, current_top), object, ES_CodeGenerator::OPSIZE_POINTER);
    cg.LEA(ES_CodeGenerator::MEMORY(object, nbytes), current_top, ES_CodeGenerator::OPSIZE_POINTER);
    cg.CMP(OBJECT_MEMBER(heap, ES_Heap, current_limit), current_top, ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_BELOW_EQUAL);

    cg.MOV(current_top, OBJECT_MEMBER(heap, ES_Heap, current_top), ES_CodeGenerator::OPSIZE_POINTER);
    cg.ADD(ES_CodeGenerator::IMMEDIATE(nbytes), OBJECT_MEMBER(heap, ES_Heap, bytes_live), ES_CodeGenerator::OPSIZE_32);

    cg.SetOutOfOrderContinuationPoint(simple_allocation);

    cg.LEA(ES_CodeGenerator::MEMORY(object, data.main_bytes + sizeof(ES_Box)), properties, ES_CodeGenerator::OPSIZE_POINTER);

    if (nindexed)
        if (representation)
            MovePointerIntoRegister(cg, representation, indexed_properties);
        else
            cg.LEA(ES_CodeGenerator::MEMORY(object, data.main_bytes + data.named_bytes), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);

    WriteHeader(cg, data.gctag, data.main_bytes, object);
    cg.MOV(ES_CodeGenerator::IMMEDIATE(data.object_bits), OBJECT_MEMBER(object, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(ES_CodeGenerator::IMMEDIATE(property_count), OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);

#ifdef ARCHITECTURE_AMD64
    ES_CodeGenerator::Register klass(current_top);
    MovePointerIntoRegister(cg, actual_klass, klass);
    cg.MOV(klass, OBJECT_MEMBER(object, ES_Object, klass), ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::IMMEDIATE(actual_klass), OBJECT_MEMBER(object, ES_Object, klass), ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

    cg.MOV(properties, OBJECT_MEMBER(object, ES_Object, properties), ES_CodeGenerator::OPSIZE_POINTER);
    if (nindexed)
        cg.MOV(indexed_properties, OBJECT_MEMBER(object, ES_Object, indexed_properties), ES_CodeGenerator::OPSIZE_POINTER);
    else
        cg.MOV(ES_CodeGenerator::ZERO(), OBJECT_MEMBER(object, ES_Object, indexed_properties), ES_CodeGenerator::OPSIZE_POINTER);

    WriteHeader(cg, GCTAG_ES_Box, data.named_bytes, object, data.main_bytes);

    if (nindexed)
    {
        /* Set length property. */
        cg.MOV(ES_CodeGenerator::IMMEDIATE(data.length), ES_CodeGenerator::MEMORY(properties), ES_CodeGenerator::OPSIZE_32);

        if (!representation)
        {
            WriteHeader(cg, GCTAG_ES_Compact_Indexed_Properties, data.indexed_bytes, indexed_properties);
            cg.MOV(ES_CodeGenerator::IMMEDIATE(*nindexed), OBJECT_MEMBER(indexed_properties, ES_Compact_Indexed_Properties, capacity), ES_CodeGenerator::OPSIZE_32);
            cg.MOV(ES_CodeGenerator::IMMEDIATE(data.length), OBJECT_MEMBER(indexed_properties, ES_Compact_Indexed_Properties, top), ES_CodeGenerator::OPSIZE_32);
        }
    }
}

#define OPERAND2OPERAND(operand) Operand2Operand(operand)

/* These machine code arrays can be regenerated by defining the
   DUMP_TRAMPOLINE_CODE_VECTORS macro below, running an empty script,
   and passing the output through the script mangle-trampolines.py in
   modules/ecmascript/carakan/src/scripts. */

#ifdef ARCHITECTURE_AMD64_UNIX

const unsigned char cv_BytecodeToNativeTrampoline_sse2[] =
{
    0x55,                               // push   %rbp
    0x53,                               // push   %rbx
    0x41, 0x54,                         // push   %r12
    0x41, 0x55,                         // push   %r13
    0x41, 0x56,                         // push   %r14
    0x41, 0x57,                         // push   %r15
    0x48, 0x8b, 0x2f,                   // mov    (%rdi),%rbp
    0x48, 0x8b, 0x47, 0x18,             // mov    0x18(%rdi),%rax
    0xff, 0x30,                         // pushq  (%rax)
    0x48, 0x89, 0x20,                   // mov    %rsp,(%rax)
    0x68, 0x00, 0x00, 0x00, 0x00,       // pushq  $0x0
    0x48, 0x8b, 0x47, 0x10,             // mov    0x10(%rdi),%rax
    0x48, 0x89, 0x20,                   // mov    %rsp,(%rax)
    0xff, 0x77, 0x08,                   // pushq  0x8(%rdi)
    0x68, 0x00, 0x00, 0x00, 0x00,       // pushq  $0x0
    0x48, 0x8b, 0x5f, 0x28,             // mov    0x28(%rdi),%rbx
    0x4c, 0x8b, 0x77, 0x20,             // mov    0x20(%rdi),%r14
    0x8b, 0xce,                         // mov    %esi,%ecx
    0x57,                               // push   %rdi
    0x4c, 0x8b, 0x5f, 0x48,             // mov    0x48(%rdi),%r11
    0x4c, 0x8b, 0x67, 0x38,             // mov    0x38(%rdi),%r12
    0x4c, 0x8b, 0x6f, 0x40,             // mov    0x40(%rdi),%r13
    0xff, 0x57, 0x30,                   // callq  *0x30(%rdi)
    0x5f,                               // pop    %rdi
    0x48, 0x83, 0xc4, 0x18,             // add    $0x18,%rsp
    0x48, 0x8b, 0x47, 0x10,             // mov    0x10(%rdi),%rax
    0x48, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        // movq   $0x0,(%rax)
    0x48, 0x8b, 0x47, 0x18,             // mov    0x18(%rdi),%rax
    0x8f, 0x00,                         // popq   (%rax)
    0x41, 0x5f,                         // pop    %r15
    0x41, 0x5e,                         // pop    %r14
    0x41, 0x5d,                         // pop    %r13
    0x41, 0x5c,                         // pop    %r12
    0x5b,                               // pop    %rbx
    0x5d,                               // pop    %rbp
    0xb8, 0x01, 0x00, 0x00, 0x00,       // mov    $0x1,%eax
    0xc3                                // retq
};

const unsigned char cv_ReconstructNativeStackTrampoline[] =
{
    0x48, 0x8b, 0xf4,                   // mov    %rsp,%rsi
    0x49, 0x8b, 0xe3,                   // mov    %r11,%rsp
    0x48, 0x83, 0xe4, 0xf0,             // and    $0xfffffffffffffff0,%rsp
    0x51,                               // push   %rcx
    0x48, 0x83, 0xec, 0x08,             // sub    $0x8,%rsp
    0x48, 0x8b, 0xfd,                   // mov    %rbp,%rdi
    0x41, 0xff, 0xd5,                   // callq  *%r13
    0x48, 0x83, 0xc4, 0x08,             // add    $0x8,%rsp
    0x59,                               // pop    %rcx
    0x48, 0x8b, 0xe0,                   // mov    %rax,%rsp
    0x41, 0xff, 0xe4                    // jmpq   *%r12
};

const unsigned char cv_ThrowFromMachineCode_sse2[] =
{
    0x48, 0x8b, 0x2f,                   // mov    (%rdi),%rbp
    0x48, 0x8b, 0x47, 0x08,             // mov    0x8(%rdi),%rax
    0x48, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        // movq   $0x0,(%rax)
    0x48, 0x8b, 0x47, 0x10,             // mov    0x10(%rdi),%rax
    0x48, 0x8b, 0x20,                   // mov    (%rax),%rsp
    0x8f, 0x00,                         // popq   (%rax)
    0x41, 0x5f,                         // pop    %r15
    0x41, 0x5e,                         // pop    %r14
    0x41, 0x5d,                         // pop    %r13
    0x41, 0x5c,                         // pop    %r12
    0x5b,                               // pop    %rbx
    0x5d,                               // pop    %rbp
    0xb8, 0x00, 0x00, 0x00, 0x00,       // mov    $0x0,%eax
    0xc3                                // retq
};

#elif defined(ARCHITECTURE_AMD64_WINDOWS)

const unsigned char cv_BytecodeToNativeTrampoline_sse2[] =
{
    0x53,                               // push   %rbx
    0x55,                               // push   %rbp
    0x56,                               // push   %rsi
    0x57,                               // push   %rdi
    0x41, 0x54,                         // push   %r12
    0x41, 0x55,                         // push   %r13
    0x41, 0x56,                         // push   %r14
    0x41, 0x57,                         // push   %r15
    0x48, 0x8b, 0x29,                   // mov    (%rcx),%rbp
    0x48, 0x8b, 0x41, 0x18,             // mov    0x18(%rcx),%rax
    0xff, 0x30,                         // pushq  (%rax)
    0x48, 0x89, 0x20,                   // mov    %rsp,(%rax)
    0x6a, 0x00,                         // pushq  $0x0
    0x48, 0x8b, 0x41, 0x10,             // mov    0x10(%rcx),%rax
    0x48, 0x89, 0x20,                   // mov    %rsp,(%rax)
    0xff, 0x71, 0x08,                   // pushq  0x8(%rcx)
    0x6a, 0x00,                         // pushq  $0x0
    0x48, 0x8b, 0x59, 0x28,             // mov    0x28(%rcx),%rbx
    0x4c, 0x8b, 0x71, 0x20,             // mov    0x20(%rcx),%r14
    0x49, 0x89, 0xd1,                   // mov    %rdx,%r9
    0x51,                               // push   %rcx
    0x4c, 0x8b, 0x59, 0x48,             // mov    0x48(%rcx),%r11
    0x4c, 0x8b, 0x61, 0x38,             // mov    0x38(%rcx),%r12
    0x4c, 0x8b, 0x69, 0x40,             // mov    0x40(%rcx),%r13
    0x48, 0x83, 0xec, 0x20,             // sub    $0x20, %rsp
    0xff, 0x51, 0x30,                   // callq  *0x30(%rcx)
    0x48, 0x83, 0xc4, 0x20,             // add    $0x20, %rsp
    0x59,                               // pop    %rcx
    0x48, 0x83, 0xc4, 0x18,             // add    $0x18,%rsp
    0x48, 0x8b, 0x41, 0x10,             // mov    0x10(%rcx),%rax
    0x48, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        // movq   $0x0,(%rax)
    0x48, 0x8b, 0x41, 0x18,             // mov    0x18(%rcx),%rax
    0x8f, 0x00,                         // popq   (%rax)
    0x41, 0x5f,                         // pop    %r15
    0x41, 0x5e,                         // pop    %r14
    0x41, 0x5d,                         // pop    %r13
    0x41, 0x5c,                         // pop    %r12
    0x5f,                               // pop    %rdi
    0x5e,                               // pop    %rsi
    0x5d,                               // pop    %rbp
    0x5b,                               // pop    %rbx
    0xb8, 0x01, 0x00, 0x00, 0x00,       // mov    $0x1,%eax
    0xc3                                // retq
};

const unsigned char cv_ReconstructNativeStackTrampoline[] =
{
    0x48, 0x89, 0xe2,                   // mov    %rsp,%rdx
    0x4c, 0x89, 0xdc,                   // mov    %r11,%rsp
    0x48, 0x83, 0xe4, 0xf0,             // and    $0xfffffffffffffff0,%rsp
    0x41, 0x51,                         // push   %r9
    0x48, 0x89, 0xe9,                   // mov    %rbp,%rcx
    0x48, 0x83, 0xec, 0x28,             // sub    $0x28, %rsp
    0x41, 0xff, 0xd5,                   // callq  *%r13
    0x48, 0x83, 0xc4, 0x28,             // add    $0x28, %rsp
    0x41, 0x59,                         // pop    %r9
    0x48, 0x89, 0xc4,                   // mov    %rax,%rsp
    0x41, 0xff, 0xe4                    // jmpq   *%r12
};

const unsigned char cv_ThrowFromMachineCode_sse2[] =
{
    0x48, 0x8b, 0x29,                   // mov    (%rcx),%rbp
    0x48, 0x8b, 0x41, 0x08,             // mov    0x8(%rcx),%rax
    0x48, 0xc7, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        // movq   $0x0,(%rax)
    0x48, 0x8b, 0x41, 0x10,             // mov    0x10(%rcx),%rax
    0x48, 0x8b, 0x20,                   // mov    (%rax),%rsp
    0x8f, 0x00,                         // popq   (%rax)
    0x41, 0x5f,                         // pop    %r15
    0x41, 0x5e,                         // pop    %r14
    0x41, 0x5d,                         // pop    %r13
    0x41, 0x5c,                         // pop    %r12
    0x5f,                               // pop    %rdi
    0x5e,                               // pop    %rsi
    0x5d,                               // pop    %rbp
    0x5b,                               // pop    %rbx
    0xb8, 0x00, 0x00, 0x00, 0x00,       // mov    $0x0,%eax
    0xc3                                // retq
};

#else // ARCHITECTURE_AMD64_UNIX

const unsigned char cv_BytecodeToNativeTrampoline_sse2[] =
{
    0x55,                               // push   %ebp
    0x53,                               // push   %ebx
    0x56,                               // push   %esi
    0x57,                               // push   %edi
    0x8b, 0x7c, 0x24, 0x14,             // mov    0x14(%esp),%edi
    0x8b, 0x2f,                         // mov    (%edi),%ebp
    0x8b, 0x47, 0x0c,                   // mov    0xc(%edi),%eax
    0xff, 0x30,                         // pushl  (%eax)
    0x89, 0x20,                         // mov    %esp,(%eax)
    0x68, 0x00, 0x00, 0x00, 0x00,       // push   $0x0
    0x8b, 0x47, 0x08,                   // mov    0x8(%edi),%eax
    0x89, 0x20,                         // mov    %esp,(%eax)
    0xff, 0x77, 0x04,                   // pushl  0x4(%edi)
    0x68, 0x00, 0x00, 0x00, 0x00,       // push   $0x0
    0x8b, 0x5f, 0x10,                   // mov    0x10(%edi),%ebx
    0x8b, 0x47, 0x14,                   // mov    0x14(%edi),%eax
    0x8b, 0x77, 0x1c,                   // mov    0x1c(%edi),%esi
    0x83, 0xec, 0x04,                   // sub    $0x4,%esp
    0xff, 0x77, 0x20,                   // pushl  0x20(%edi)
    0xff, 0x77, 0x18,                   // pushl  0x18(%edi)
    0x8b, 0x7c, 0x24, 0x34,             // mov    0x34(%esp),%edi
    0xff, 0xd0,                         // call   *%eax
    0x83, 0xc4, 0x0c,                   // add    $0xc,%esp
    0x8b, 0x7c, 0x24, 0x24,             // mov    0x24(%esp),%edi
    0x83, 0xc4, 0x0c,                   // add    $0xc,%esp
    0x8b, 0x47, 0x08,                   // mov    0x8(%edi),%eax
    0xc7, 0x00, 0x00, 0x00, 0x00, 0x00, // movl   $0x0,(%eax)
    0x8b, 0x47, 0x0c,                   // mov    0xc(%edi),%eax
    0x8f, 0x00,                         // popl   (%eax)
    0x5f,                               // pop    %edi
    0x5e,                               // pop    %esi
    0x5b,                               // pop    %ebx
    0x5d,                               // pop    %ebp
    0xb8, 0x01, 0x00, 0x00, 0x00,       // mov    $0x1,%eax
    0xc3                                // ret
};

const unsigned char cv_ThrowFromMachineCode_sse2[] =
{
    0x8b, 0x7c, 0x24, 0x04,             // mov    0x4(%esp),%edi
    0x8b, 0x2f,                         // mov    (%edi),%ebp
    0x8b, 0x47, 0x04,                   // mov    0x4(%edi),%eax
    0xc7, 0x00, 0x00, 0x00, 0x00, 0x00, // movl   $0x0,(%eax)
    0x8b, 0x47, 0x08,                   // mov    0x8(%edi),%eax
    0x8b, 0x20,                         // mov    (%eax),%esp
    0x8f, 0x00,                         // popl   (%eax)
    0x5f,                               // pop    %edi
    0x5e,                               // pop    %esi
    0x5b,                               // pop    %ebx
    0x5d,                               // pop    %ebp
    0xb8, 0x00, 0x00, 0x00, 0x00,       // mov    $0x0,%eax
    0xc3                                // ret
};

const unsigned char cv_BytecodeToNativeTrampoline_x87[] =
{
    0x55,                               // push   %ebp
    0x53,                               // push   %ebx
    0x56,                               // push   %esi
    0x57,                               // push   %edi
    0x8b, 0x7c, 0x24, 0x14,             // mov    0x14(%esp),%edi
    0x83, 0xec, 0x04,                   // sub    $0x4,%esp
    0xd9, 0x3c, 0x24,                   // fnstcw (%esp)
    0x66, 0x8b, 0x04, 0x24,             // mov    (%esp),%ax
    0x66, 0x81, 0xc8, 0x3f, 0x0f,       // or     $0xf3f,%ax
    0x66, 0x89, 0x44, 0x24, 0x02,       // mov    %ax,0x2(%esp)
    0xd9, 0x6c, 0x24, 0x02,             // fldcw  0x2(%esp)
    0x8b, 0x2f,                         // mov    (%edi),%ebp
    0x8b, 0x47, 0x0c,                   // mov    0xc(%edi),%eax
    0xff, 0x30,                         // pushl  (%eax)
    0x89, 0x20,                         // mov    %esp,(%eax)
    0x68, 0x00, 0x00, 0x00, 0x00,       // push   $0x0
    0x8b, 0x47, 0x08,                   // mov    0x8(%edi),%eax
    0x89, 0x20,                         // mov    %esp,(%eax)
    0xff, 0x77, 0x04,                   // pushl  0x4(%edi)
    0x68, 0x00, 0x00, 0x00, 0x00,       // push   $0x0
    0x8b, 0x5f, 0x10,                   // mov    0x10(%edi),%ebx
    0x8b, 0x47, 0x14,                   // mov    0x14(%edi),%eax
    0x8b, 0x77, 0x1c,                   // mov    0x1c(%edi),%esi
    0xff, 0x77, 0x20,                   // pushl  0x20(%edi)
    0xff, 0x77, 0x18,                   // pushl  0x18(%edi)
    0x8b, 0x7c, 0x24, 0x34,             // mov    0x34(%esp),%edi
    0xff, 0xd0,                         // call   *%eax
    0x83, 0xc4, 0x08,                   // add    $0x8,%esp
    0x8b, 0x7c, 0x24, 0x28,             // mov    0x28(%esp),%edi
    0x83, 0xc4, 0x0c,                   // add    $0xc,%esp
    0x8b, 0x47, 0x08,                   // mov    0x8(%edi),%eax
    0xc7, 0x00, 0x00, 0x00, 0x00, 0x00, // movl   $0x0,(%eax)
    0x8b, 0x47, 0x0c,                   // mov    0xc(%edi),%eax
    0x8f, 0x00,                         // popl   (%eax)
    0xd9, 0x2c, 0x24,                   // fldcw  (%esp)
    0x83, 0xc4, 0x04,                   // add    $0x4,%esp
    0x5f,                               // pop    %edi
    0x5e,                               // pop    %esi
    0x5b,                               // pop    %ebx
    0x5d,                               // pop    %ebp
    0xb8, 0x01, 0x00, 0x00, 0x00,       // mov    $0x1,%eax
    0xc3                                // ret
};

const unsigned char cv_ThrowFromMachineCode_x87[] =
{
    0x8b, 0x7c, 0x24, 0x04,             // mov    0x4(%esp),%edi
    0x8b, 0x2f,                         // mov    (%edi),%ebp
    0x8b, 0x47, 0x04,                   // mov    0x4(%edi),%eax
    0xc7, 0x00, 0x00, 0x00, 0x00, 0x00, // movl   $0x0,(%eax)
    0x8b, 0x47, 0x08,                   // mov    0x8(%edi),%eax
    0x8b, 0x20,                         // mov    (%eax),%esp
    0x8f, 0x00,                         // popl   (%eax)
    0xd9, 0x2c, 0x24,                   // fldcw  (%esp)
    0x83, 0xc4, 0x04,                   // add    $0x4,%esp
    0x5f,                               // pop    %edi
    0x5e,                               // pop    %esi
    0x5b,                               // pop    %ebx
    0x5d,                               // pop    %ebp
    0xb8, 0x00, 0x00, 0x00, 0x00,       // mov    $0x0,%eax
    0xc3                                // ret
};

const unsigned char cv_ReconstructNativeStackTrampoline[] =
{
    0x8b, 0x44, 0x24, 0x04,             // mov    0x4(%esp),%eax
    0x8b, 0xd4,                         // mov    %esp,%edx
    0x8b, 0x64, 0x24, 0x08,             // mov    0x8(%esp),%esp
    0x83, 0xec, 0x0c,                   // sub    $0xc,%esp
    0x83, 0xe4, 0xf0,                   // and    $0xfffffff0,%esp
    0x89, 0x44, 0x24, 0x08,             // mov    %eax,0x8(%esp)
    0x89, 0x54, 0x24, 0x04,             // mov    %edx,0x4(%esp)
    0x89, 0x2c, 0x24,                   // mov    %ebp,(%esp)
    0xff, 0xd6,                         // call   *%esi
    0x83, 0xc4, 0x08,                   // add    $0x8,%esp
    0x5e,                               // pop    %esi
    0x8b, 0xe0,                         // mov    %eax,%esp
    0xff, 0xe6                          // jmp    *%esi
};

#endif // ARCHITECTURE_AMD64_UNIX

//#define DUMP_TRAMPOLINE_CODE_VECTORS
#ifdef DUMP_TRAMPOLINE_CODE_VECTORS

static ES_CodeGenerator::Operand
FunctionArgument(const char *format, unsigned index, unsigned stack_frame_offset)
{
#ifdef ARCHITECTURE_AMD64
    BOOL is_double = format[index] == 'd';

    for (unsigned previous = 0; previous < index; ++previous)
        if ((format[previous] == 'd') != is_double)
            --index;

    if (is_double)
        return static_cast<ES_CodeGenerator::XMM>(ES_CodeGenerator::REG_XMM0 + index);
    else
        switch (index)
        {
        case 0: return ES_CodeGenerator::REG_DI; break;
        case 1: return ES_CodeGenerator::REG_SI; break;
        case 2: return ES_CodeGenerator::REG_DX; break;
        case 3: return ES_CodeGenerator::REG_CX; break;
        case 4: return ES_CodeGenerator::REG_R8; break;
        case 5: return ES_CodeGenerator::REG_R9; break;
        default:
            OP_ASSERT(FALSE);
            return ES_CodeGenerator::Operand();
        }
#else // ARCHITECTURE_AMD64
    /* Return address. */
    stack_frame_offset += sizeof(void *);

    for (unsigned previous = 0; previous < index; ++previous)
        switch (format[previous])
        {
        case 'd':
            stack_frame_offset += sizeof(double);
            break;

        case 'p':
            stack_frame_offset += sizeof(void *);
            break;

        default:
            stack_frame_offset += sizeof(int);
        }

    return ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, stack_frame_offset);
#endif // ARCHITECTURE_AMD64
}

extern "C" { void ES_NativeDisassemble(unsigned char *, unsigned char *, int); }

static void
DumpNativeCode(const char *key, ES_ExecutableMemory::Block *block)
{
    extern FILE *g_native_disassembler_file;
    g_native_disassembler_file = stdout;

    fprintf(g_native_disassembler_file, "const unsigned char %s[] =\n{\n", key);

    ES_NativeDisassemble(static_cast<unsigned char *>(block->memory), static_cast<unsigned char *>(block->memory) + block->size, 1);

    fprintf(g_native_disassembler_file, "};\n");

    g_native_disassembler_file = NULL;
}

#endif // DUMP_TRAMPOLINE_CODE_VECTORS

/* static */ BOOL
(*ES_Native::GetBytecodeToNativeTrampoline())(void **, unsigned)
{
    union
    {
        const void *ptr;
        BOOL (*fn)(void **, unsigned);
    } cast;

#ifndef DUMP_TRAMPOLINE_CODE_VECTORS
#ifdef CONSTANT_DATA_IS_EXECUTABLE
#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87_STATIC)
        cast.ptr = cv_BytecodeToNativeTrampoline_x87;
    else
#endif // IA32_X87_SUPPORT
        cast.ptr = cv_BytecodeToNativeTrampoline_sse2;
#else // CONSTANT_DATA_IS_EXECUTABLE
    static const OpExecMemory *&g_block = g_ecma_bytecode_to_native_block;

    const unsigned char *ptr;
    unsigned len;

#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87_STATIC)
    {
        ptr = cv_BytecodeToNativeTrampoline_x87;
        len = sizeof cv_BytecodeToNativeTrampoline_x87;
    }
    else
#endif // IA32_X87_SUPPORT
    {
        ptr = cv_BytecodeToNativeTrampoline_sse2;
        len = sizeof cv_BytecodeToNativeTrampoline_sse2;
    }

    if (!g_block)
    {
        g_block = g_executableMemory->AllocateL(len);
        op_memcpy(g_block->address, ptr, len);
        OpExecMemoryManager::FinalizeL(g_block);
    }

    cast.ptr = g_block->address;
#endif // CONSTANT_DATA_IS_EXECUTABLE
#else // DUMP_TRAMPOLINE_CODE_VECTORS
    ES_CodeGenerator cg(g_executableMemory);

    /* prologue; save registers that are callee-save. */
    cg.PUSH(ES_CodeGenerator::REG_BP);
    cg.PUSH(ES_CodeGenerator::REG_BX);

    unsigned stack_space_allocated = 2 * sizeof(void *);

    const ES_CodeGenerator::Register pointers(ES_CodeGenerator::REG_DI);
    const ES_CodeGenerator::Register scratch(ES_CodeGenerator::REG_AX);

#ifdef ARCHITECTURE_AMD64
# ifdef ARCHITECTURE_AMD64_WINDOWS
    cg.PUSH(ES_CodeGenerator::REG_SI);
    cg.PUSH(ES_CodeGenerator::REG_DI);
# endif // ARCHITECTURE_AMD64_WINDOWS
    cg.PUSH(ES_CodeGenerator::REG_R12);
    cg.PUSH(ES_CodeGenerator::REG_R13);
    cg.PUSH(ES_CodeGenerator::REG_R14);
    cg.PUSH(ES_CodeGenerator::REG_R15);

# ifdef ARCHITECTURE_AMD64_WINDOWS
    stack_space_allocated += 6 * sizeof(void *);
# else
    stack_space_allocated += 4 * sizeof(void *);
# endif // ARCHITECTURE_AMD64_WINDOWS
#else // ARCHITECTURE_AMD64
    cg.PUSH(ES_CodeGenerator::REG_SI);
    cg.PUSH(ES_CodeGenerator::REG_DI);

    stack_space_allocated += 2 * sizeof(void *);

    cg.MOV(FunctionArgument("pu", 0, stack_space_allocated), pointers, ES_CodeGenerator::OPSIZE_POINTER);

#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87_STATIC)
    {
        cg.SUB(ES_CodeGenerator::IMMEDIATE(4), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
        cg.FNSTCW(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP));
        cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_16);
        cg.OR(ES_CodeGenerator::IMMEDIATE(0xf3f), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_16);
        cg.MOV(ES_CodeGenerator::REG_AX, ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, 2), ES_CodeGenerator::OPSIZE_16);
        cg.FLDCW(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, 2));

        stack_space_allocated += 4;
    }
#endif // IA32_X87_SUPPORT
#endif // ARCHITECTURE_AMD64

#define POINTER(name) ES_CodeGenerator::MEMORY(pointers, TRAMPOLINE_POINTER_##name * sizeof(void *))

    /* context => EBP (see definition of CONTEXT_POINTER) */
    cg.MOV(POINTER(CONTEXT), CONTEXT_POINTER, ES_CodeGenerator::OPSIZE_POINTER);

    /* Save previous context->stack_ptr */
    cg.MOV(POINTER(CONTEXT_STACK_PTR), scratch, ES_CodeGenerator::OPSIZE_POINTER);
    cg.PUSH(ES_CodeGenerator::MEMORY(scratch), ES_CodeGenerator::OPSIZE_POINTER);
    stack_space_allocated += sizeof(void *);

    /* ESP => context->stack_ptr */
    cg.MOV(ES_CodeGenerator::REG_SP, ES_CodeGenerator::MEMORY(scratch), ES_CodeGenerator::OPSIZE_POINTER);

    /* Set-up a terminating native stack frame, with a register frame
       pointer corresponding to the calling code's register frame. */
    cg.PUSH(ES_CodeGenerator::ZERO(), ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(POINTER(CONTEXT_NATIVE_STACK_FRAME), scratch, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(ES_CodeGenerator::REG_SP, ES_CodeGenerator::MEMORY(scratch), ES_CodeGenerator::OPSIZE_POINTER);

    cg.PUSH(POINTER(CONTEXT_REG), ES_CodeGenerator::OPSIZE_POINTER);
    cg.PUSH(ES_CodeGenerator::ZERO(), ES_CodeGenerator::OPSIZE_POINTER);

    stack_space_allocated += 3 * sizeof(void *);

    /* reg => EBX (see definition of REGISTER_FRAME_POINTER) */
    cg.MOV(POINTER(REGISTER_FRAME), REGISTER_FRAME_POINTER, ES_CodeGenerator::OPSIZE_POINTER);

#ifdef ARCHITECTURE_AMD64
    /* g_ecma_instruction_handlers => R14 */
    cg.MOV(POINTER(INSTRUCTION_HANDLERS), INSTRUCTION_HANDLERS_POINTER, ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
    cg.MOV(POINTER(TRAMPOLINE_DISPATCHER), scratch, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(POINTER(RECONSTRUCT_NATIVE_STACK), ES_CodeGenerator::REG_SI, ES_CodeGenerator::OPSIZE_POINTER);
    cg.PUSH(POINTER(STACK_LIMIT));
    cg.PUSH(POINTER(NATIVE_DISPATCHER));
    stack_space_allocated += 2 * sizeof(void *);
#endif // ARCHITECTURE_AMD64

    /* argc => EDI (see definition of PARAMETERS_COUNT) */
    cg.MOV(FunctionArgument("pu", 1, stack_space_allocated), PARAMETERS_COUNT, ES_CodeGenerator::OPSIZE_32);

    unsigned dropped = 3 * sizeof(void *);

#ifdef ARCHITECTURE_AMD64
    cg.PUSH(pointers, ES_CodeGenerator::OPSIZE_POINTER);

    stack_space_allocated += sizeof(void *);

    if ((stack_space_allocated + sizeof(void *)) & 15)
    {
        cg.SUB(ES_CodeGenerator::IMMEDIATE(sizeof(void *)), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
        stack_space_allocated += sizeof(void *);
        dropped += sizeof(void *);
    }

    /* call the native code */
    cg.MOV(POINTER(STACK_LIMIT), ES_CodeGenerator::REG_R11, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(POINTER(NATIVE_DISPATCHER), ES_CodeGenerator::REG_R12, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(POINTER(RECONSTRUCT_NATIVE_STACK), ES_CodeGenerator::REG_R13, ES_CodeGenerator::OPSIZE_POINTER);
    cg.CALL(POINTER(TRAMPOLINE_DISPATCHER));
#else // ARCHITECTURE_AMD64
    /* call the native code */
    cg.CALL(scratch);
    cg.ADD(ES_CodeGenerator::IMMEDIATE(2 * sizeof(void *)), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);

    stack_space_allocated -= 2 * sizeof(void *);
#endif // ARCHITECTURE_AMD64

#ifdef ARCHITECTURE_AMD64
    cg.POP(pointers, ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
    cg.MOV(FunctionArgument("pu", 0, stack_space_allocated), pointers, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

    cg.ADD(ES_CodeGenerator::IMMEDIATE(dropped), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);

    cg.MOV(POINTER(CONTEXT_NATIVE_STACK_FRAME), scratch, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(ES_CodeGenerator::ZERO(), ES_CodeGenerator::MEMORY(scratch), ES_CodeGenerator::OPSIZE_POINTER);

    /* Restore previous context->stack_ptr */
    cg.MOV(POINTER(CONTEXT_STACK_PTR), scratch, ES_CodeGenerator::OPSIZE_POINTER);
    cg.POP(ES_CodeGenerator::MEMORY(scratch), ES_CodeGenerator::OPSIZE_POINTER);

    /* epilogue; restore previously saved registers and return true. */
#ifdef ARCHITECTURE_AMD64
    cg.POP(ES_CodeGenerator::REG_R15);
    cg.POP(ES_CodeGenerator::REG_R14);
    cg.POP(ES_CodeGenerator::REG_R13);
    cg.POP(ES_CodeGenerator::REG_R12);
# ifdef ARCHITECTURE_AMD64_WINDOWS
    cg.POP(ES_CodeGenerator::REG_DI);
    cg.POP(ES_CodeGenerator::REG_SI);
# endif // ARCHITECTURE_AMD64_WINDOWS
#else // ARCHITECTURE_AMD64
#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87_STATIC)
    {
        cg.FLDCW(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP));
        cg.ADD(ES_CodeGenerator::IMMEDIATE(4), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    }
#endif // IA32_X87_SUPPORT

    cg.POP(ES_CodeGenerator::REG_DI);
    cg.POP(ES_CodeGenerator::REG_SI);
#endif // ARCHITECTURE_AMD64

    cg.POP(ES_CodeGenerator::REG_BX);
    cg.POP(ES_CodeGenerator::REG_BP);

    cg.MOV(ES_CodeGenerator::IMMEDIATE(1), ES_CodeGenerator::REG_AX);
    cg.RET();

    cg.EnableDisassemble();

    ES_ExecutableMemory::Block *block = cg.GetCode();

    DumpNativeCode("cv_BytecodeToNativeTrampoline", block);

    cast.ptr = block->memory;
#undef POINTER
#endif // DUMP_TRAMPOLINE_CODE_VECTORS

    return cast.fn;
}

/* static */ void *
ES_Native::GetReconstructNativeStackTrampoline(BOOL prologue_entry_point)
{
#ifndef DUMP_TRAMPOLINE_CODE_VECTORS
#ifdef CONSTANT_DATA_IS_EXECUTABLE
    return const_cast<unsigned char *>(cv_ReconstructNativeStackTrampoline);
#else // CONSTANT_DATA_IS_EXECUTABLE
    static const OpExecMemory *&g_block = g_opera->ecmascript_module.reconstruct_native_stack1_block;

    if (!g_block)
    {
        g_block = g_executableMemory->AllocateL(sizeof cv_ReconstructNativeStackTrampoline);
        op_memcpy(g_block->address, cv_ReconstructNativeStackTrampoline, sizeof cv_ReconstructNativeStackTrampoline);
        OpExecMemoryManager::FinalizeL(g_block);
    }

    return g_block->address;
#endif // CONSTANT_DATA_IS_EXECUTABLE
#else // DUMP_TRAMPOLINE_CODE_VECTORS
    ES_CodeGenerator cg(g_executableMemory);

#ifdef ARCHITECTURE_AMD64
#ifdef ARCHITECTURE_AMD64_UNIX
    ES_CodeGenerator::Register stack_pointer(ES_CodeGenerator::REG_SI), reconstruct_native_stack(ES_CodeGenerator::REG_R13);
#else
    ES_CodeGenerator::Register stack_pointer(ES_CodeGenerator::REG_DX), reconstruct_native_stack(ES_CodeGenerator::REG_R13);
#endif // ARCHITECTURE_AMD64_UNIX

    cg.MOV(ES_CodeGenerator::REG_SP, stack_pointer, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(ES_CodeGenerator::REG_R11, ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    cg.AND(ES_CodeGenerator::IMMEDIATE(~15), ES_CodeGenerator::ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    cg.PUSH(PARAMETERS_COUNT);
    cg.SUB(ES_CodeGenerator::IMMEDIATE(8), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
    ES_CodeGenerator::Register stack_pointer(ES_CodeGenerator::REG_DX), reconstruct_native_stack(ES_CodeGenerator::REG_SI);

    cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(ES_CodeGenerator::REG_SP, stack_pointer, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, 2 * sizeof(void *)), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    cg.AND(ES_CodeGenerator::IMMEDIATE(~15), ES_CodeGenerator::ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    cg.PUSH(ES_CodeGenerator::REG_AX);
#endif // ARCHITECTURE_AMD64

    IA32FUNCTIONCALLR("pp", 0);

    call.AllocateStackSpace();
    call.AddArgument(CONTEXT_POINTER);
    call.AddArgument(stack_pointer);
    call.Call(reconstruct_native_stack);
    call.FreeStackSpace();

#ifdef ARCHITECTURE_AMD64
    cg.ADD(ES_CodeGenerator::IMMEDIATE(8), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    cg.POP(PARAMETERS_COUNT);
    cg.MOV(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    cg.JMP(ES_CodeGenerator::REG_R12);
#else // ARCHITECTURE_AMD64
    cg.POP(ES_CodeGenerator::REG_SI);
    cg.MOV(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    cg.JMP(ES_CodeGenerator::REG_SI);
#endif // ARCHITECTURE_AMD64

    cg.EnableDisassemble();

    ES_ExecutableMemory::Block *block = cg.GetCode();

    DumpNativeCode("cv_ReconstructNativeStackTrampoline", block);

    return block->memory;
#endif // DUMP_TRAMPOLINE_CODE_VECTORS
}

/* static */ void
(*ES_Native::GetThrowFromMachineCode())(void **)
{
    union
    {
        const void *ptr;
        void (*fn)(void **);
    } cast;

#ifndef DUMP_TRAMPOLINE_CODE_VECTORS
#ifdef CONSTANT_DATA_IS_EXECUTABLE
#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87_STATIC)
        cast.ptr = cv_ThrowFromMachineCode_x87;
    else
#endif // IA32_X87_SUPPORT
        cast.ptr = cv_ThrowFromMachineCode_sse2;
#else // CONSTANT_DATA_IS_EXECUTABLE
    static const OpExecMemory *&g_block = g_ecma_throw_from_machine_code_block;

    const void *ptr;
    unsigned len;

#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87_STATIC)
    {
        ptr = cv_ThrowFromMachineCode_x87;
        len = sizeof cv_ThrowFromMachineCode_x87;
    }
    else
#endif // IA32_X87_SUPPORT
    {
        ptr = cv_ThrowFromMachineCode_sse2;
        len = sizeof cv_ThrowFromMachineCode_sse2;
    }

    if (!g_block)
    {
        g_block = g_executableMemory->AllocateL(len);
        op_memcpy(g_block->address, ptr, len);
        OpExecMemoryManager::FinalizeL(g_block);
    }

    cast.ptr = g_block->address;
#endif // CONSTANT_DATA_IS_EXECUTABLE
#else // DUMP_TRAMPOLINE_CODE_VECTORS
    ES_CodeGenerator cg(g_executableMemory);

# ifdef ARCHITECTURE_AMD64_WINDOWS
    const ES_CodeGenerator::Register pointers(ES_CodeGenerator::REG_CX);
# else
    const ES_CodeGenerator::Register pointers(ES_CodeGenerator::REG_DI);
# endif // ARCHITECTURE_AMD64_WINDOWS
    const ES_CodeGenerator::Register scratch(ES_CodeGenerator::REG_AX);

#define POINTER(name) ES_CodeGenerator::MEMORY(pointers, THROW_POINTER_##name * sizeof(void *))

#ifndef ARCHITECTURE_AMD64
    cg.MOV(FunctionArgument("p", 0, 0), pointers, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

    /* context => EBP */
    cg.MOV(POINTER(CONTEXT), CONTEXT_POINTER, ES_CodeGenerator::OPSIZE_POINTER);

    /* NULL => context->native_stack_frame */
    cg.MOV(POINTER(CONTEXT_NATIVE_STACK_FRAME), scratch, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(ES_CodeGenerator::ZERO(), ES_CodeGenerator::MEMORY(scratch), ES_CodeGenerator::OPSIZE_POINTER);

    /* context->stack_ptr => ESP */
    cg.MOV(POINTER(CONTEXT_STACK_PTR), scratch, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(ES_CodeGenerator::MEMORY(scratch), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);

    /* Restore previous context->stack_ptr */
    cg.POP(ES_CodeGenerator::MEMORY(scratch), ES_CodeGenerator::OPSIZE_POINTER);

    /* epilogue; restore registers saved by the BytecodeToNativeTrampoline and return false. */
#ifdef ARCHITECTURE_AMD64
    cg.POP(ES_CodeGenerator::REG_R15, ES_CodeGenerator::OPSIZE_POINTER);
    cg.POP(ES_CodeGenerator::REG_R14, ES_CodeGenerator::OPSIZE_POINTER);
    cg.POP(ES_CodeGenerator::REG_R13, ES_CodeGenerator::OPSIZE_POINTER);
    cg.POP(ES_CodeGenerator::REG_R12, ES_CodeGenerator::OPSIZE_POINTER);
# ifdef ARCHITECTURE_AMD64_WINDOWS
    cg.POP(ES_CodeGenerator::REG_DI, ES_CodeGenerator::OPSIZE_POINTER);
    cg.POP(ES_CodeGenerator::REG_SI, ES_CodeGenerator::OPSIZE_POINTER);
# endif // ARCHITECTURE_AMD64_WINDOWS
#else // ARCHITECTURE_AMD64
#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87_STATIC)
    {
        cg.FLDCW(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP));
        cg.ADD(ES_CodeGenerator::IMMEDIATE(4), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    }
#endif // IA32_X87_SUPPORT

    cg.POP(ES_CodeGenerator::REG_DI, ES_CodeGenerator::OPSIZE_POINTER);
    cg.POP(ES_CodeGenerator::REG_SI, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

    cg.POP(ES_CodeGenerator::REG_BX);
    cg.POP(ES_CodeGenerator::REG_BP);

    cg.MOV(ES_CodeGenerator::ZERO(), ES_CodeGenerator::REG_AX);
    cg.RET();

    ES_ExecutableMemory::Block *block = cg.GetCode();

    DumpNativeCode("cv_ThrowFromMachineCode", block);

    cast.ptr = block->memory;
#undef POINTER
#endif // DUMP_TRAMPOLINE_CODE_VECTORS

    return cast.fn;
}

#ifdef USE_CUSTOM_DOUBLE_OPS

/* static */ double
(*ES_Native::CreateAddDoubles())(double lhs, double rhs)
{
    ES_CodeGenerator cg(g_executableMemory);

#ifdef ARCHITECTURE_AMD64
    cg.ADDSD(ES_CodeGenerator::REG_XMM1, ES_CodeGenerator::REG_XMM0);
#else // ARCHITECTURE_AMD64
    cg.MOVSD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)), ES_CodeGenerator::REG_XMM0);
    cg.ADDSD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *) + sizeof(double)), ES_CodeGenerator::REG_XMM0);
    cg.MOVSD(ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)));
    cg.FLD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)));
#endif // ARCHITECTURE_AMD64
    cg.RET();

    return reinterpret_cast<double (*)(double, double)>(cg.GetCode()->address);
}

/* static */ double
(*ES_Native::CreateSubtractDoubles())(double lhs, double rhs)
{
    ES_CodeGenerator cg(g_executableMemory);

#ifdef ARCHITECTURE_AMD64
    cg.SUBSD(ES_CodeGenerator::REG_XMM1, ES_CodeGenerator::REG_XMM0);
#else // ARCHITECTURE_AMD64
    cg.MOVSD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)), ES_CodeGenerator::REG_XMM0);
    cg.SUBSD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *) + sizeof(double)), ES_CodeGenerator::REG_XMM0);
    cg.MOVSD(ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)));
    cg.FLD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)));
#endif // ARCHITECTURE_AMD64
    cg.RET();

    return reinterpret_cast<double (*)(double, double)>(cg.GetCode()->address);
}

/* static */ double
(*ES_Native::CreateMultiplyDoubles())(double lhs, double rhs)
{
    ES_CodeGenerator cg(g_executableMemory);

#ifdef ARCHITECTURE_AMD64
    cg.MULSD(ES_CodeGenerator::REG_XMM1, ES_CodeGenerator::REG_XMM0);
#else // ARCHITECTURE_AMD64
    cg.MOVSD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)), ES_CodeGenerator::REG_XMM0);
    cg.MULSD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *) + sizeof(double)), ES_CodeGenerator::REG_XMM0);
    cg.MOVSD(ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)));
    cg.FLD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)));
#endif // ARCHITECTURE_AMD64
    cg.RET();

    return reinterpret_cast<double (*)(double, double)>(cg.GetCode()->address);
}

/* static */ double
(*ES_Native::CreateDivideDoubles())(double lhs, double rhs)
{
    ES_CodeGenerator cg(g_executableMemory);

#ifdef ARCHITECTURE_AMD64
    cg.DIVSD(ES_CodeGenerator::REG_XMM1, ES_CodeGenerator::REG_XMM0);
#else // ARCHITECTURE_AMD64
    cg.MOVSD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)), ES_CodeGenerator::REG_XMM0);
    cg.DIVSD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *) + sizeof(double)), ES_CodeGenerator::REG_XMM0);
    cg.MOVSD(ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)));
    cg.FLD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)));
#endif // ARCHITECTURE_AMD64
    cg.RET();

    return reinterpret_cast<double (*)(double, double)>(cg.GetCode()->address);
}

/* static */ double
(*ES_Native::CreateSin())(double arg)
{
    ES_CodeGenerator cg(g_executableMemory);

#ifdef ARCHITECTURE_AMD64
    cg.SUB(ES_CodeGenerator::IMMEDIATE(8), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOVSD(ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP));
    cg.FLD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP));
    cg.FSIN();
    cg.FST(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP), TRUE);
    cg.MOVSD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP), ES_CodeGenerator::REG_XMM0);
    cg.ADD(ES_CodeGenerator::IMMEDIATE(8), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
    cg.FLD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)));
    cg.FSIN();
#endif // ARCHITECTURE_AMD64
    cg.RET();

    return reinterpret_cast<double (*)(double)>(cg.GetCode()->address);
}

/* static */ double
(*ES_Native::CreateCos())(double arg)
{
    ES_CodeGenerator cg(g_executableMemory);

#ifdef ARCHITECTURE_AMD64
    cg.SUB(ES_CodeGenerator::IMMEDIATE(8), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOVSD(ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP));
    cg.FLD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP));
    cg.FCOS();
    cg.FST(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP), TRUE);
    cg.MOVSD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP), ES_CodeGenerator::REG_XMM0);
    cg.ADD(ES_CodeGenerator::IMMEDIATE(8), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
    cg.FLD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)));
    cg.FCOS();
#endif // ARCHITECTURE_AMD64
    cg.RET();

    return reinterpret_cast<double (*)(double)>(cg.GetCode()->address);
}

/* static */ double
(*ES_Native::CreateTan())(double arg)
{
    ES_CodeGenerator cg(g_executableMemory);

#ifdef ARCHITECTURE_AMD64
    cg.SUB(ES_CodeGenerator::IMMEDIATE(8), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOVSD(ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP));
    cg.FLD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP));
    cg.FSINCOS();
    cg.FDIV(0, 1, TRUE, FALSE);
    cg.FST(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP), TRUE);
    cg.MOVSD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP), ES_CodeGenerator::REG_XMM0);
    cg.ADD(ES_CodeGenerator::IMMEDIATE(8), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
    cg.FLD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, sizeof(void *)));
    cg.FSINCOS();
    cg.FDIV(0, 1, TRUE, FALSE);
#endif // ARCHITECTURE_AMD64
    cg.RET();
    return reinterpret_cast<double (*)(double)>(cg.GetCode()->address);
}

#endif // USE_CUSTOM_DOUBLE_OPS

#ifdef DEBUG_ENABLE_OPASSERT

/* static */ BOOL
ES_Native::IsAddressInBytecodeToNativeTrampoline(void *address)
{
#if !defined DUMP_TRAMPOLINE_CODE_VECTORS && defined CONSTANT_DATA_IS_EXECUTABLE
    const unsigned char *cv;
    unsigned cv_length;

#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87_STATIC)
    {
        cv = cv_BytecodeToNativeTrampoline_x87;
        cv_length = ARRAY_SIZE(cv_BytecodeToNativeTrampoline_x87);
    }
    else
#endif // IA32_X87_SUPPORT
    {
        cv = cv_BytecodeToNativeTrampoline_sse2;
        cv_length = ARRAY_SIZE(cv_BytecodeToNativeTrampoline_sse2);
    }

    return reinterpret_cast<const unsigned char *>(address) >= cv && reinterpret_cast<const unsigned char *>(address) < cv + cv_length;
#else // !DUMP_TRAMPOLINE_CODE_VECTORS && CONSTANT_DATA_IS_EXECUTABLE
    return TRUE;
#endif // !DUMP_TRAMPOLINE_CODE_VECTORS && CONSTANT_DATA_IS_EXECUTABLE
}

#endif // DEBUG_ENABLE_OPASSERT

void
ES_Native::UpdateCode(ES_Code *new_code)
{
    code = new_code;
}

BOOL
ES_Native::CheckLimits()
{
    return TRUE;
}

void
ES_Native::InitializeNativeRegisters()
{
    integer_registers_count = IA32_INTEGER_REGISTER_COUNT;
    double_registers_count = ARCHITECTURE_HAS_FPU() ? (fpmode == FPMODE_X87 ? 1 : IA32_DOUBLE_REGISTER_COUNT) : 0;

    native_registers_count = integer_registers_count + double_registers_count;
    native_registers = OP_NEWGROA_L(NativeRegister, native_registers_count, Arena());

    for (unsigned index = 0; index < native_registers_count; ++index)
    {
        NativeRegister &native_register = native_registers[index];
        native_register.first_allocation = native_register.last_allocation = native_register.current_allocation = native_register.previous_allocation = NULL;
        native_register.type = index < integer_registers_count ? NativeRegister::TYPE_INTEGER : NativeRegister::TYPE_DOUBLE;
    }

    native_registers[IA32_REGISTER_INDEX_AX].register_code = IA32_REGISTER_CODE_AX;
    native_registers[IA32_REGISTER_INDEX_CX].register_code = IA32_REGISTER_CODE_CX;
    native_registers[IA32_REGISTER_INDEX_DX].register_code = IA32_REGISTER_CODE_DX;
    //native_registers[IA32_REGISTER_INDEX_BX].register_code = IA32_REGISTER_CODE_BX;
    //native_registers[IA32_REGISTER_INDEX_BP].register_code = IA32_REGISTER_CODE_BP;
    native_registers[IA32_REGISTER_INDEX_DI].register_code = IA32_REGISTER_CODE_DI;
    native_registers[IA32_REGISTER_INDEX_SI].register_code = IA32_REGISTER_CODE_SI;

#ifdef ARCHITECTURE_AMD64
    native_registers[IA32_REGISTER_INDEX_R8].register_code = IA32_REGISTER_CODE_R8;
    native_registers[IA32_REGISTER_INDEX_R9].register_code = IA32_REGISTER_CODE_R9;
    native_registers[IA32_REGISTER_INDEX_R10].register_code = IA32_REGISTER_CODE_R10;
    native_registers[IA32_REGISTER_INDEX_R11].register_code = IA32_REGISTER_CODE_R11;
    native_registers[IA32_REGISTER_INDEX_R12].register_code = IA32_REGISTER_CODE_R12;
    native_registers[IA32_REGISTER_INDEX_R13].register_code = IA32_REGISTER_CODE_R13;
    //native_registers[IA32_REGISTER_INDEX_R14].register_code = IA32_REGISTER_CODE_R14;
    //native_registers[IA32_REGISTER_INDEX_R15].register_code = IA32_REGISTER_CODE_R15;
#endif // ARCHITECTURE_AMD64

    if (ARCHITECTURE_HAS_FPU())
    {
        native_registers[IA32_REGISTER_INDEX_XMM0].register_code = IA32_REGISTER_CODE_XMM0;

        if (IS_FPMODE_SSE2)
        {
            native_registers[IA32_REGISTER_INDEX_XMM1].register_code = IA32_REGISTER_CODE_XMM1;
            native_registers[IA32_REGISTER_INDEX_XMM2].register_code = IA32_REGISTER_CODE_XMM2;
            native_registers[IA32_REGISTER_INDEX_XMM3].register_code = IA32_REGISTER_CODE_XMM3;
            native_registers[IA32_REGISTER_INDEX_XMM4].register_code = IA32_REGISTER_CODE_XMM4;
            native_registers[IA32_REGISTER_INDEX_XMM5].register_code = IA32_REGISTER_CODE_XMM5;
            native_registers[IA32_REGISTER_INDEX_XMM6].register_code = IA32_REGISTER_CODE_XMM6;

#ifdef ARCHITECTURE_AMD64
            native_registers[IA32_REGISTER_INDEX_XMM7].register_code = IA32_REGISTER_CODE_XMM7;
            native_registers[IA32_REGISTER_INDEX_XMM8].register_code = IA32_REGISTER_CODE_XMM8;
            native_registers[IA32_REGISTER_INDEX_XMM9].register_code = IA32_REGISTER_CODE_XMM9;
            native_registers[IA32_REGISTER_INDEX_XMM10].register_code = IA32_REGISTER_CODE_XMM10;
            native_registers[IA32_REGISTER_INDEX_XMM11].register_code = IA32_REGISTER_CODE_XMM11;
            native_registers[IA32_REGISTER_INDEX_XMM12].register_code = IA32_REGISTER_CODE_XMM12;
            native_registers[IA32_REGISTER_INDEX_XMM13].register_code = IA32_REGISTER_CODE_XMM13;
            native_registers[IA32_REGISTER_INDEX_XMM14].register_code = IA32_REGISTER_CODE_XMM14;
#endif // ARCHITECTURE_AMD64
        }
    }
    stack_space_allocated = 0;
}

void
ES_Native::EmitLoadRegisterValue(NativeRegister *target, VirtualRegister *source, ES_CodeGenerator::JumpTarget *type_check_fail)
{
    ES_ValueType type = target->type == NativeRegister::TYPE_INTEGER ? ESTYPE_INT32 : ESTYPE_DOUBLE;

    if (type_check_fail)
#ifdef ES_VALUES_AS_NANS
        cg.CMP(ES_CodeGenerator::IMMEDIATE(type), REGISTER_TYPE(source), ES_CodeGenerator::OPSIZE_32);
#else // ES_VALUES_AS_NANS
        cg.TEST(ES_CodeGenerator::IMMEDIATE(type), REGISTER_TYPE(source), ES_CodeGenerator::OPSIZE_32);
#endif // ES_VALUES_AS_NANS

    if (target->type == NativeRegister::TYPE_INTEGER)
        cg.MOV(REGISTER_VALUE(source), Native2Operand(target), ES_CodeGenerator::OPSIZE_32);
#ifdef IA32_X87_SUPPORT
    else if (IS_FPMODE_X87)
        cg.FLD(REGISTER_VALUE(source));
#endif // IA32_X87_SUPPORT
#ifdef IA32_SSE2_SUPPORT
    else if (IS_FPMODE_SSE2)
        cg.MOVSD(REGISTER_VALUE(source), static_cast<ES_CodeGenerator::XMM>(target->register_code));
#endif // IA32_SSE2_SUPPORT

    if (type_check_fail)
#ifdef ES_VALUES_AS_NANS
        cg.Jump(type_check_fail, type == ESTYPE_DOUBLE ? ES_NATIVE_CONDITION_GREATER : ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);
#else // ES_VALUES_AS_NANS
        cg.Jump(type_check_fail, ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);
#endif // ES_VALUES_AS_NANS
}

BOOL
ES_Native::IsComplexStore(VirtualRegister *target, NativeRegister *source)
{
#ifdef ES_VALUES_AS_NANS
    return (source->type != NativeRegister::TYPE_INTEGER && target->stack_frame_offset == INT_MAX);
#else
    return FALSE;
#endif // ES_VALUES_AS_NANS
}

void
ES_Native::EmitSaveCondition()
{
#ifdef ES_VALUES_AS_NANS
    cg.PUSHFD();
#endif // ES_VALUES_AS_NANS
}

void
ES_Native::EmitRestoreCondition()
{
#ifdef ES_VALUES_AS_NANS
    cg.POPFD();
#endif // ES_VALUES_AS_NANS
}

void
ES_Native::EmitStoreRegisterValue(VirtualRegister *target, NativeRegister *source, BOOL write_type, BOOL saved_condition)
{
    if (target->stack_frame_offset != INT_MAX)
    {
        OP_ASSERT(target->stack_frame_type == (source->type == NativeRegister::TYPE_INTEGER ? ESTYPE_INT32 : ESTYPE_DOUBLE));

        write_type = FALSE;
    }

    if (source->type == NativeRegister::TYPE_INTEGER)
    {
        cg.MOV(Native2Operand(source), REGISTER_VALUE(target, saved_condition), ES_CodeGenerator::OPSIZE_32);

        if (write_type)
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(target), ES_CodeGenerator::OPSIZE_32);
    }
    else if (target->stack_frame_offset == INT_MAX)
    {
#ifdef IA32_X87_SUPPORT
        if (IS_FPMODE_X87)
        {
#ifdef ES_VALUES_AS_NANS
            cg.FUCOMI(0, FALSE);
#endif // ES_VALUES_AS_NANS
            cg.FST(REGISTER_VALUE(target, saved_condition), TRUE);
        }
#endif // IA32_X87_SUPPORT
#ifdef IA32_SSE2_SUPPORT
        if (IS_FPMODE_SSE2)
        {
#ifdef ES_VALUES_AS_NANS
            cg.UCOMISD(Native2XMM(source), Native2XMM(source));
#endif // ES_VALUES_AS_NANS
            cg.MOVSD(Native2XMM(source), REGISTER_VALUE(target, saved_condition));
        }
#endif // IA32_SSE2_SUPPORT

#ifdef ES_VALUES_AS_NANS
        /* If the source double was NaN, and one of the NaNs we use to represent
           other types of values, we need to normalize it to a NaN we recognize
           as a double. */

        ES_CodeGenerator::JumpTarget *not_NaN = cg.ForwardJump();
        cg.Jump(not_NaN, ES_NATIVE_CONDITION_NOT_PARITY, TRUE, TRUE);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), REGISTER_TYPE(target), ES_CodeGenerator::OPSIZE_32);
        cg.SetJumpTarget(not_NaN);
#else // ES_VALUES_AS_NANS
        if (write_type)
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), REGISTER_TYPE(target), ES_CodeGenerator::OPSIZE_32);
#endif // ES_VALUES_AS_NANS
    }
    else
    {
#ifdef IA32_X87_SUPPORT
        if (IS_FPMODE_X87)
            cg.FST(REGISTER_VALUE(target, saved_condition), TRUE);
#endif // IA32_X87_SUPPORT
#ifdef IA32_SSE2_SUPPORT
        if (IS_FPMODE_SSE2)
            cg.MOVSD(Native2XMM(source), REGISTER_VALUE(target, saved_condition));
#endif // IA32_SSE2_SUPPORT
    }
}

void
ES_Native::EmitStoreGlobalObject(VirtualRegister *target)
{
#ifdef ARCHITECTURE_AMD64
    if (reinterpret_cast<UINTPTR>(code->global_object) <= UINT_MAX)
        cg.MOV(ES_CodeGenerator::IMMEDIATE(reinterpret_cast<UINTPTR>(code->global_object)), REGISTER_VALUE(target), ES_CodeGenerator::OPSIZE_POINTER);
    else
    {
        MovePointerIntoRegister(cg, code->global_object, ES_CodeGenerator::REG_AX);
        cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(target), ES_CodeGenerator::OPSIZE_POINTER);
    }
#else // ARCHITECTURE_AMD64
        cg.MOV(ES_CodeGenerator::IMMEDIATE(code->global_object), REGISTER_VALUE(target), ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

    cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(target), ES_CodeGenerator::OPSIZE_32);
}

void
ES_Native::EmitStoreConstantBoolean(VirtualRegister *target, BOOL value)
{
    cg.MOV(ES_CodeGenerator::IMMEDIATE(!!value), REGISTER_VALUE(target), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), REGISTER_TYPE(target), ES_CodeGenerator::OPSIZE_32);
}

void
ES_Native::EmitConvertRegisterValue(NativeRegister *target, VirtualRegister *source, unsigned handled, unsigned possible, ES_CodeGenerator::JumpTarget *slow_case)
{
    //OP_ASSERT(handled == (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE) || target->type == NativeRegister::TYPE_DOUBLE && handled == ESTYPE_BITS_INT32 || target->type == NativeRegister::TYPE_INTEGER && handled == ESTYPE_BITS_DOUBLE);

    if (target->type == NativeRegister::TYPE_DOUBLE)
        if (handled == ESTYPE_BITS_INT32)
        {
            if (possible != handled)
                EmitRegisterTypeCheck(source, ESTYPE_INT32, slow_case);

#ifdef IA32_X87_SUPPORT
            if (IS_FPMODE_X87)
                cg.FILD(REGISTER_VALUE(source), ES_CodeGenerator::OPSIZE_32);
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
            if (IS_FPMODE_SSE2)
                cg.CVTSI2SD(REGISTER_VALUE(source), static_cast<ES_CodeGenerator::XMM>(target->register_code));
#endif // IA32_SSE2_SUPPORT
        }
        else
        {
            ES_CodeGenerator::JumpTarget *was_integer = NULL;

            if ((handled & ESTYPE_BITS_INT32) != 0)
            {
                ES_CodeGenerator::JumpTarget *not_integer = cg.ForwardJump();

                was_integer = cg.ForwardJump();

                cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(source), ES_CodeGenerator::OPSIZE_32);
                cg.Jump(not_integer, ES_NATIVE_CONDITION_NOT_EQUAL);

#ifdef IA32_X87_SUPPORT
                if (IS_FPMODE_X87)
                    cg.FILD(REGISTER_VALUE(source), ES_CodeGenerator::OPSIZE_32);
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
                if (IS_FPMODE_SSE2)
                    cg.CVTSI2SD(REGISTER_VALUE(source), static_cast<ES_CodeGenerator::XMM>(target->register_code));
#endif // IA32_SSE2_SUPPORT

                cg.Jump(was_integer);
                cg.SetJumpTarget(not_integer);
            }

            ES_CodeGenerator::OutOfOrderBlock *set_NaN = NULL;

            if ((handled & ESTYPE_BITS_UNDEFINED) != 0)
            {
                set_NaN = cg.StartOutOfOrderBlock();

#ifdef IA32_X87_SUPPORT
                if (IS_FPMODE_X87)
                    cg.FLD(NaN(cg));
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
                if (IS_FPMODE_SSE2)
                    cg.MOVSD(NaN(cg), static_cast<ES_CodeGenerator::XMM>(target->register_code));
#endif // IA32_SSE2_SUPPORT

                cg.EndOutOfOrderBlock();

                cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_UNDEFINED), REGISTER_TYPE(source), ES_CodeGenerator::OPSIZE_32);
                cg.Jump(set_NaN->GetJumpTarget(), ES_NATIVE_CONDITION_EQUAL);
            }

            if (possible != handled)
                EmitRegisterTypeCheck(source, ESTYPE_DOUBLE, slow_case);

#ifdef IA32_X87_SUPPORT
            if (IS_FPMODE_X87)
                cg.FLD(REGISTER_VALUE(source));
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
            if (IS_FPMODE_SSE2)
                cg.MOVSD(REGISTER_VALUE(source), static_cast<ES_CodeGenerator::XMM>(target->register_code));
#endif // IA32_SSE2_SUPPORT

            if (was_integer)
                cg.SetJumpTarget(was_integer);

            if (set_NaN)
                cg.SetOutOfOrderContinuationPoint(set_NaN);
        }
    else
    {
        ES_CodeGenerator::JumpTarget *is_integer = NULL, *is_finished = NULL;
        ES_CodeGenerator::Register cg_target = static_cast<ES_CodeGenerator::Register>(target->register_code);

        if (handled == ESTYPE_BITS_DOUBLE)
        {
            if (handled != possible)
                EmitRegisterTypeCheck(source, ESTYPE_DOUBLE, slow_case);
        }
        else
        {
            if ((handled & ESTYPE_BITS_INT32) != 0)
            {
                is_integer = cg.ForwardJump();

#ifdef ARCHITECTURE_AMD64
                cg.TEST(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32 | ESTYPE_BOOLEAN), REGISTER_TYPE(source), ES_CodeGenerator::OPSIZE_32);
                cg.Jump(is_integer, ES_NATIVE_CONDITION_NOT_ZERO);
#else // ARCHITECTURE_AMD64
                cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(source), ES_CodeGenerator::OPSIZE_32);
                cg.Jump(is_integer, ES_NATIVE_CONDITION_EQUAL);
#endif // ARCHITECTURE_AMD64
            }

            if ((handled & ESTYPE_BITS_NULL) != 0)
            {
                if (!is_finished)
                    is_finished = cg.ForwardJump();

                cg.MOV(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);
                cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_NULL), REGISTER_TYPE(source), ES_CodeGenerator::OPSIZE_32);
                cg.Jump(is_finished, ES_NATIVE_CONDITION_EQUAL);
            }

            if ((handled & ESTYPE_BITS_UNDEFINED) != 0)
            {
                if (!is_finished)
                    is_finished = cg.ForwardJump();

                cg.MOV(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);
                cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_UNDEFINED), REGISTER_TYPE(source), ES_CodeGenerator::OPSIZE_32);
                cg.Jump(is_finished, ES_NATIVE_CONDITION_EQUAL);
            }

            if (handled != possible)
                if ((handled & ESTYPE_BITS_DOUBLE) != 0)
                    EmitRegisterTypeCheck(source, ESTYPE_DOUBLE, slow_case);
                else
                    cg.Jump(slow_case);
        }

        if ((handled & ESTYPE_BITS_DOUBLE) != 0)
            ConvertDoubleToInt(this, source, cg_target, FALSE, TRUE, slow_case);

        if (is_integer)
        {
            ES_CodeGenerator::JumpTarget *finished = NULL;

            if ((handled & ESTYPE_BITS_DOUBLE) != 0)
            {
                finished = cg.ForwardJump();
                cg.Jump(finished);
            }

            cg.SetJumpTarget(is_integer);
            cg.MOV(REGISTER_VALUE(source), Native2Operand(target), ES_CodeGenerator::OPSIZE_32);

            if (finished)
                cg.SetJumpTarget(finished);
        }

        if (is_finished)
            cg.SetJumpTarget(is_finished);
    }
}

void
ES_Native::EmitSetRegisterType(VirtualRegister *target, ES_ValueType type)
{
    OP_ASSERT(type != ESTYPE_INT32_OR_DOUBLE);

#ifdef ES_VALUES_AS_NANS
    if (type != ESTYPE_DOUBLE)
#endif // ES_VALUES_AS_NANS
        cg.MOV(ES_CodeGenerator::IMMEDIATE(type), REGISTER_TYPE(target), ES_CodeGenerator::OPSIZE_32);
}

void
ES_Native::EmitSetRegisterValue(VirtualRegister *virtual_register, const ES_Value_Internal &value, BOOL write_type, BOOL saved_condition)
{
    if (value.IsInt32() || value.IsBoolean())
        cg.MOV(ES_CodeGenerator::IMMEDIATE(value.IsInt32() ? value.GetInt32() : INT32(value.GetBoolean())), REGISTER_VALUE(virtual_register, saved_condition), ES_CodeGenerator::OPSIZE_32);
    else if (value.IsDouble())
    {
#ifdef ES_VALUES_AS_NANS
        if (!op_isnan(value.GetDouble()))
#endif // ES_VALUES_AS_NANS
        {
            UINT32 hi, low;

            op_explode_double(value.GetDouble(), hi, low);

            cg.MOV(ES_CodeGenerator::IMMEDIATE(low), REGISTER_VALUE(virtual_register, saved_condition), ES_CodeGenerator::OPSIZE_32);
            cg.MOV(ES_CodeGenerator::IMMEDIATE(hi), REGISTER_VALUE2(virtual_register, saved_condition), ES_CodeGenerator::OPSIZE_32);
        }
    }
    else
        OP_ASSERT(value.IsNull() || value.IsUndefined());

#ifdef ES_VALUES_AS_NANS
    if (write_type && !value.IsDouble() || op_isnan(value.GetDouble()))
#else // ES_VALUES_AS_NANS
    if (write_type)
#endif // ES_VALUES_AS_NANS
        cg.MOV(ES_CodeGenerator::IMMEDIATE(value.Type()), REGISTER_TYPE(virtual_register), ES_CodeGenerator::OPSIZE_32);
}

void
ES_Native::EmitSetRegisterValueFromStackFrameStorage(VirtualRegister *vr)
{
    if (vr->stack_frame_offset == INT_MAX - 1)
    {
        if (vr->stack_frame_type == ESTYPE_DOUBLE)
        {
#ifdef IA32_X87_SUPPORT
            if (IS_FPMODE_X87)
                cg.FST(REGISTER_VALUE(vr), TRUE);
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
            if (IS_FPMODE_SSE2)
                cg.MOVSD(REGISTER_VALUE(vr).xmm, VALUE_VALUE_INDEX(REGISTER_FRAME_POINTER, vr->index));
#endif // IA32_SSE2_SUPPORT

#ifdef ES_VALUES_AS_NANS
            cg.MOV(VALUE_TYPE_INDEX(REGISTER_FRAME_POINTER, vr->index), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);

            ES_CodeGenerator::JumpTarget *not_NaN = cg.ForwardJump();
            cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(not_NaN, ES_NATIVE_CONDITION_LESS_EQUAL, TRUE, TRUE);
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), VALUE_TYPE_INDEX(REGISTER_FRAME_POINTER, vr->index), ES_CodeGenerator::OPSIZE_32);
            cg.SetJumpTarget(not_NaN);
#else // ES_VALUES_AS_NANS
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), VALUE_TYPE_INDEX(REGISTER_FRAME_POINTER, vr->index), ES_CodeGenerator::OPSIZE_32);
#endif // ES_VALUES_AS_NANS
        }
        else
        {
            cg.MOV(REGISTER_VALUE(vr), VALUE_VALUE_INDEX(REGISTER_FRAME_POINTER, vr->index), ES_CodeGenerator::OPSIZE_32);
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), VALUE_TYPE_INDEX(REGISTER_FRAME_POINTER, vr->index), ES_CodeGenerator::OPSIZE_32);
        }
    }
    else if (vr->stack_frame_type == ESTYPE_DOUBLE)
    {
#ifdef ARCHITECTURE_AMD64
        cg.MOV(REGISTER_VALUE(vr), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_64);

        vr->stack_frame_offset = INT_MAX;

        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), REGISTER_TYPE(vr), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(vr), ES_CodeGenerator::OPSIZE_64);
#else // ARCHITECTURE_AMD64
        ES_CodeGenerator::JumpTarget *not_NaN = cg.ForwardJump();

        cg.MOV(REGISTER_VALUE(vr), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(REGISTER_VALUE2(vr), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);

        vr->stack_frame_offset = INT_MAX;

        cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(vr), ES_CodeGenerator::OPSIZE_32);
        cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);
        cg.Jump(not_NaN, ES_NATIVE_CONDITION_LESS_EQUAL, TRUE, TRUE);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);
        cg.SetJumpTarget(not_NaN);
        cg.MOV(ES_CodeGenerator::REG_DX, REGISTER_TYPE(vr), ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64
    }
    else
    {
        cg.MOV(REGISTER_VALUE(vr), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);

        vr->stack_frame_offset = INT_MAX;

        cg.MOV(ES_CodeGenerator::IMMEDIATE(vr->stack_frame_type), REGISTER_TYPE(vr), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(vr), ES_CodeGenerator::OPSIZE_32);
    }
}

void
ES_Native::EmitRegisterCopy(const Operand &source, const Operand &target, ES_CodeGenerator::JumpTarget *slow_case)
{
    if (source.native_register && target.virtual_register)
        EmitStoreRegisterValue(target.virtual_register, source.native_register, TRUE);
    else if (source.virtual_register && target.native_register)
    {
        if (target.native_register->type == NativeRegister::TYPE_INTEGER)
            cg.MOV(Operand2Operand(source), Operand2Operand(target), ES_CodeGenerator::OPSIZE_32);
        else
        {
            OP_ASSERT(!source.codeword && !source.value);

#ifdef IA32_X87_SUPPORT
            if (IS_FPMODE_X87)
                cg.FLD(Operand2Operand(source));
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
            if (IS_FPMODE_SSE2)
                cg.MOVSD(Operand2Operand(source), static_cast<ES_CodeGenerator::XMM>(target.native_register->register_code));
#endif // IA32_SSE2_SUPPORT
        }
    }
    else if (source.codeword)
        EmitRegisterInt32Assign(source, target);
    else if (source.native_register && target.native_register)
    {
        if (target.native_register->type == NativeRegister::TYPE_DOUBLE)
        {
#ifdef IA32_X87_SUPPORT
            if (IS_FPMODE_X87)
            {
                /* We have only one DOUBLE register, so it would be odd to be
                   asked to copy the value from one to another... */
                OP_ASSERT(source.native_register->type == NativeRegister::TYPE_INTEGER);
                cg.FILD(Operand2Operand(source), ES_CodeGenerator::OPSIZE_32);
            }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
            if (IS_FPMODE_SSE2)
                if (source.native_register->type == NativeRegister::TYPE_DOUBLE)
                    cg.MOVSD(Operand2Operand(source), Operand2XMM(target));
                else
                    cg.CVTSI2SD(Operand2Operand(source), Operand2XMM(target));
#endif // IA32_SSE2_SUPPORT
        }
        else if (source.native_register->type == NativeRegister::TYPE_DOUBLE)
        {
            OP_ASSERT(slow_case);

            ES_CodeGenerator::XMM rsource = static_cast<ES_CodeGenerator::XMM>(source.native_register->register_code);
            ES_CodeGenerator::Register rtarget = static_cast<ES_CodeGenerator::Register>(target.native_register->register_code);

            cg.UCOMISD(LongMin(cg), rsource);
            cg.Jump(slow_case, ES_NATIVE_CONDITION_CARRY);

            ES_CodeGenerator::JumpTarget *trivial = cg.ForwardJump();

            cg.UCOMISD(LongMax(cg), rsource);
            cg.Jump(trivial, ES_NATIVE_CONDITION_CARRY);
            cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_ZERO);
            cg.SetJumpTarget(trivial);
            cg.CVTTSD2SI(rsource, rtarget, ES_CodeGenerator::OPSIZE_POINTER);
        }
        else
            cg.MOV(Operand2Operand(source), Operand2Operand(target), ES_CodeGenerator::OPSIZE_32);
    }
    else
        OP_ASSERT(FALSE);
}

void
ES_Native::EmitRegisterCopy(VirtualRegister *source, VirtualRegister *target)
{
    OP_ASSERT(target->stack_frame_offset == INT_MAX);

    ES_ValueType type = ESTYPE_UNDEFINED;

    if (source->stack_frame_offset != INT_MAX)
    {
        type = source->stack_frame_type;

#ifdef ES_VALUES_AS_NANS
        if (source->stack_frame_type == ESTYPE_DOUBLE)
        {
            EmitLoadRegisterValue(NR(IA32_REGISTER_INDEX_XMM0), source, NULL);
            EmitStoreRegisterValue(target, NR(IA32_REGISTER_INDEX_XMM0), TRUE);
            return;
        }
#endif // ES_VALUES_AS_NANS
    }

#ifdef ES_VALUES_AS_NANS
    if (type != ESTYPE_UNDEFINED || GetType(source, type) && type != ESTYPE_INT32_OR_DOUBLE && type != ESTYPE_DOUBLE)
#else // ES_VALUES_AS_NANS
    if (type != ESTYPE_UNDEFINED || GetType(source, type) && type != ESTYPE_INT32_OR_DOUBLE)
#endif // ES_VALUES_AS_NANS
    {
        ES_CodeGenerator::OperandSize value_opsize = ES_CodeGenerator::OPSIZE_32;

#ifndef ES_VALUES_AS_NANS
        if (type == ESTYPE_DOUBLE || type == ESTYPE_STRING || type == ESTYPE_OBJECT)
            value_opsize = ES_CodeGenerator::OPSIZE_64;
#endif // ES_VALUES_AS_NANS

        cg.MOV(REGISTER_VALUE(source), ES_CodeGenerator::REG_AX, value_opsize);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(type), REGISTER_TYPE(target), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(target), value_opsize);
    }
    else
    {
        cg.MOV(REGISTER_TYPE(source), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(REGISTER_VALUE(source), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_TYPE(target), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::REG_DX, REGISTER_VALUE(target), ES_CodeGenerator::OPSIZE_POINTER);
    }
}

void
ES_Native::EmitRegisterInt32Copy(VirtualRegister *source, VirtualRegister *target)
{
    cg.MOV(REGISTER_VALUE(source), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
    cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(target), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(target), ES_CodeGenerator::OPSIZE_32);
}

void
ES_Native::EmitRegisterInt32Assign(const Operand &source, const Operand &target)
{
    if (target.native_register && target.native_register->type == NativeRegister::TYPE_DOUBLE)
    {
        OP_ASSERT(source.codeword);

#ifdef IA32_X87_SUPPORT
        if (IS_FPMODE_X87)
            if (source.codeword->immediate == 0)
                cg.FLDZ();
            else if (source.codeword->immediate == 1)
                cg.FLD1();
            else
            {
#ifdef ARCHITECTURE_AMD64
                ES_CodeGenerator::Constant *constant = cg.NewConstant(source.codeword->immediate);
                cg.FILD(ES_CodeGenerator::CONSTANT(constant), ES_CodeGenerator::OPSIZE_32);
#else // ARCHITECTURE_AMD64
                cg.FILD(ES_CodeGenerator::MEMORY(source.codeword), ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64
            }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
        if (IS_FPMODE_SSE2)
        {
#ifdef ARCHITECTURE_AMD64
            ES_CodeGenerator::Constant *constant = cg.NewConstant(source.codeword->immediate);
            cg.CVTSI2SD(ES_CodeGenerator::CONSTANT(constant), Operand2XMM(target));
#else // ARCHITECTURE_AMD64
            cg.CVTSI2SD(ES_CodeGenerator::MEMORY(source.codeword), Operand2XMM(target), ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64
        }
#endif // IA32_SSE2_SUPPORT
    }
    else
    {
        cg.MOV(Operand2Operand(source), Operand2Operand(target), ES_CodeGenerator::OPSIZE_32);

        if (target.virtual_register)
            EmitSetRegisterType(target.virtual_register, ESTYPE_INT32);
    }
}

void
ES_Native::EmitRegisterInt32Assign(int source, NativeRegister *target)
{
    cg.MOV(ES_CodeGenerator::IMMEDIATE(source), Native2Operand(target), ES_CodeGenerator::OPSIZE_32);
}

void
ES_Native::EmitRegisterDoubleAssign(const double *value, const Operand &target)
{
    ES_CodeGenerator::Operand source;

#ifdef ARCHITECTURE_AMD64
    source = ES_CodeGenerator::CONSTANT(cg.NewConstant(*value));
#else // ARCHITECTURE_AMD64
    source = ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(value));
#endif // ARCHITECTURE_AMD64

#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87)
        if (target.native_register)
            cg.FLD(source);
        else
        {
#ifdef ARCHITECTURE_AMD64
            cg.MOV(source, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_64);
            cg.MOV(ES_CodeGenerator::REG_AX, Operand2Operand(target), ES_CodeGenerator::OPSIZE_64);
#else // ARCHITECTURE_AMD64
            cg.FLD(source);
            cg.FST(Operand2Operand(target), TRUE);
#endif // ARCHITECTURE_AMD64

#ifndef ES_VALUES_AS_NANS
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), REGISTER_TYPE(target.virtual_register), ES_CodeGenerator::OPSIZE_32);
#endif // ES_VALUES_AS_NANS
        }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
    if (IS_FPMODE_SSE2)
        if (target.native_register)
            cg.MOVSD(source, static_cast<ES_CodeGenerator::XMM>(target.native_register->register_code));
        else
        {
#ifdef ARCHITECTURE_AMD64
            cg.MOV(source, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_64);
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), REGISTER_TYPE(target.virtual_register), ES_CodeGenerator::OPSIZE_32);
            cg.MOV(ES_CodeGenerator::REG_AX, Operand2Operand(target), ES_CodeGenerator::OPSIZE_64);
#else // ARCHITECTURE_AMD64
            cg.MOVSD(source, ES_CodeGenerator::REG_XMM0);
            cg.MOVSD(ES_CodeGenerator::REG_XMM0, Operand2Operand(target));
#endif // ARCHITECTURE_AMD64
        }
#endif // IA32_SSE2_SUPPORT
}

void
ES_Native::EmitRegisterStringAssign(JString *value, const Operand &target)
{
#ifdef ARCHITECTURE_AMD64
    if (target.native_register)
        cg.MOV(ES_CodeGenerator::CONSTANT(cg.NewConstant(value)), Operand2Operand(target), ES_CodeGenerator::OPSIZE_POINTER);
    else
    {
        cg.MOV(ES_CodeGenerator::CONSTANT(cg.NewConstant(value)), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(ES_CodeGenerator::REG_AX, Operand2Operand(target), ES_CodeGenerator::OPSIZE_POINTER);
    }
#else // ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::IMMEDIATE(reinterpret_cast<UINTPTR>(value)), Operand2Operand(target), ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

    if (target.virtual_register)
        EmitSetRegisterType(target.virtual_register, ESTYPE_STRING);
}

void
ES_Native::EmitRegisterTypeCheck(VirtualRegister *source, ES_ValueType value_type, ES_CodeGenerator::JumpTarget *slow_case, BOOL invert_result)
{
    OP_ASSERT((property_value_fail && property_value_fail == slow_case) || property_value_needs_type_check || property_value_read_vr == NULL || property_value_read_vr != source);

    if (!slow_case)
    {
        if (!current_slow_case)
            EmitSlowCaseCall();

        slow_case = current_slow_case->GetJumpTarget();
    }

    ES_CodeGenerator::Operand op_type;

    if (property_value_read_vr == source && property_value_nr && !property_value_fail)
    {
        OP_ASSERT(property_value_needs_type_check);
        op_type = ES_CodeGenerator::MEMORY(Native2Register(property_value_nr), property_value_offset + VALUE_TYPE_OFFSET);
    }
    else
        op_type = REGISTER_TYPE(source);

#ifdef ES_VALUES_AS_NANS
    if (value_type == ESTYPE_INT32_OR_DOUBLE)
    {
        if (invert_result)
        {
            cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), op_type, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(slow_case, ES_NATIVE_CONDITION_EQUAL, TRUE, FALSE);
            cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), op_type, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(slow_case, ES_NATIVE_CONDITION_LESS_EQUAL, TRUE, TRUE);
        }
        else
        {
            ES_CodeGenerator::JumpTarget *jt_match = cg.ForwardJump();

            cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), op_type, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(jt_match, ES_NATIVE_CONDITION_EQUAL, TRUE, TRUE);
            cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), op_type, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(slow_case, ES_NATIVE_CONDITION_GREATER, TRUE, FALSE);
            cg.SetJumpTarget(jt_match);
        }
    }
    else
    {
        cg.CMP(ES_CodeGenerator::IMMEDIATE(value_type), op_type, ES_CodeGenerator::OPSIZE_32);
        ES_NativeJumpCondition condition;
        if (invert_result)
            condition = value_type == ESTYPE_DOUBLE ? ES_NATIVE_CONDITION_NOT_GREATER : ES_NATIVE_CONDITION_EQUAL;
        else
            condition = value_type == ESTYPE_DOUBLE ? ES_NATIVE_CONDITION_GREATER : ES_NATIVE_CONDITION_NOT_EQUAL;

        cg.Jump(slow_case, condition, TRUE, FALSE);
    }
#else // ES_VALUES_AS_NANS
    cg.TEST(ES_CodeGenerator::IMMEDIATE(value_type), op_type, ES_CodeGenerator::OPSIZE_32);
    cg.Jump(slow_case, invert_result ? ES_NATIVE_CONDITION_NOT_ZERO : ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);
#endif // ES_VALUES_AS_NANS
}

void
ES_Native::EmitToPrimitive(VirtualRegister *source)
{
    if (!current_slow_case)
        EmitSlowCaseCall();

#ifdef ES_VALUES_AS_NANS
    cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(source), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_EQUAL, TRUE, FALSE);
#else // ES_VALUES_AS_NANS
    cg.TEST(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(source), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_ZERO, TRUE, FALSE);
#endif // ES_VALUES_AS_NANS
}

void
ES_Native::EmitInstructionHandlerCall()
{
    if (is_light_weight)
        LEAVE(OpStatus::ERR);

    ES_CodeWord *word = CurrentCodeWord();
    ES_Instruction instruction = word->instruction;

    if (instruction == ESI_TOPRIMITIVE)
        instruction = ESI_TOPRIMITIVE1;

    if (word == entry_point_cw && !entry_point_jump_target)
        entry_point_jump_target = EmitEntryPoint();

    switch (instruction)
    {
    case ESI_EVAL:
    case ESI_CALL:
    case ESI_CONSTRUCT:
        {
            void *(ES_CALLING_CONVENTION *fn)(ES_Execution_Context *, unsigned);

            if (word->instruction == ESI_EVAL || word->instruction == ESI_CALL)
                fn = &ES_Execution_Context::CallFromMachineCode;
            else
                fn = &ES_Execution_Context::ConstructFromMachineCode;

            IA32FUNCTIONCALLP("pu", fn);

            call.AllocateStackSpace();
            call.AddArgument(CONTEXT_POINTER);
            call.AddArgument(ES_CodeGenerator::IMMEDIATE(cw_index + (constructor ? code->data->codewords_count : 0)));
            call.Call();
            call.FreeStackSpace();

            ES_CodeGenerator::JumpTarget *not_updated, *use_not_updated;

            if (!current_slow_case)
                not_updated = use_not_updated = cg.ForwardJump();
            else
            {
                not_updated = NULL;
                use_not_updated = current_slow_case->GetContinuationTarget();
            }

            cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
            cg.Jump(use_not_updated, ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);
            cg.JMP(ES_CodeGenerator::REG_AX);

            if (not_updated)
                cg.SetJumpTarget(not_updated);
        }
        break;

    case ESI_GETN_IMM:
    case ESI_GET_LENGTH:
    case ESI_PUTN_IMM:
    case ESI_INIT_PROPERTY:
        {
            IA32FUNCTIONCALLP("pu", (word->instruction == ESI_PUTN_IMM || word->instruction == ESI_INIT_PROPERTY) ? &ES_Execution_Context::PutNamedImmediate: &ES_Execution_Context::GetNamedImmediate);

            call.AllocateStackSpace();
            call.AddArgument(CONTEXT_POINTER);
            call.AddArgument(ES_CodeGenerator::IMMEDIATE(cw_index + (constructor ? code->data->codewords_count : 0)));
            call.Call();
            call.FreeStackSpace();

            ES_CodeGenerator::JumpTarget *not_updated = current_slow_case ? current_slow_case->GetContinuationTarget() : cg.ForwardJump();

            cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
            cg.Jump(not_updated, ES_NATIVE_CONDITION_ZERO);
            cg.JMP(ES_CodeGenerator::REG_AX);

            if (!current_slow_case)
                cg.SetJumpTarget(not_updated);

            jump_to_continuation_point = FALSE;
        }
        break;

    case ESI_GET_GLOBAL:
        if (code->global_caches[word[3].index].class_id == ES_Class::GLOBAL_IMMEDIATE_CLASS_ID)
        {
            IA32FUNCTIONCALLP("pu", ES_Execution_Context::GetGlobalImmediate);

            call.AllocateStackSpace();
            call.AddArgument(CONTEXT_POINTER);
            call.AddArgument(ES_CodeGenerator::IMMEDIATE(cw_index));
            call.Call();
            call.FreeStackSpace();

            cg.JMP(ES_CodeGenerator::REG_AX);

            jump_to_continuation_point = FALSE;
        }
        else
            goto generic_instruction;
        break;

    case ESI_REDIRECTED_CALL:
        {
            DECLARE_NOTHING();

            cg.MOV(ES_CodeGenerator::IMMEDIATE(constructor ? 1 : 0), OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, in_constructor), ES_CodeGenerator::OPSIZE_32);
        }

    generic_instruction:
    default:
        {
#ifdef ARCHITECTURE_AMD64
            cg.MOV(ES_CodeGenerator::MEMORY(INSTRUCTION_HANDLERS_POINTER, word->instruction * sizeof(g_ecma_instruction_handlers[0])), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
# ifdef ARCHITECTURE_AMD64_UNIX
            MovePointerIntoRegister(cg, word + 1, ES_CodeGenerator::REG_SI);
            cg.MOV(CONTEXT_POINTER, ES_CodeGenerator::REG_DI, ES_CodeGenerator::OPSIZE_POINTER);
# else
            MovePointerIntoRegister(cg, word + 1, ES_CodeGenerator::REG_DX);
            cg.MOV(CONTEXT_POINTER, ES_CodeGenerator::REG_CX, ES_CodeGenerator::OPSIZE_POINTER);
# endif // ARCHITECTURE_AMD64_UNIX
            cg.CALL(ES_CodeGenerator::REG_AX);
#else // ARCHITECTURE_AMD64
            IA32FUNCTIONCALLP("pp", g_ecma_instruction_handlers[word->instruction]);

            call.AllocateStackSpace();
            call.AddArgument(CONTEXT_POINTER);
            call.AddArgument(ES_CodeGenerator::IMMEDIATE(word + 1));
            call.Call();
            call.FreeStackSpace();
#endif // ARCHITECTURE_AMD64

            /* Clear the in_constructor flag set prior to call */
            if (instruction == ESI_REDIRECTED_CALL && constructor)
            {
                DECLARE_NOTHING();
                cg.MOV(ES_CodeGenerator::ZERO(), OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, in_constructor), ES_CodeGenerator::OPSIZE_32);
            }

            if (word->instruction == ESI_GET || word->instruction == ESI_GETI_IMM || word->instruction == ESI_PUT || word->instruction == ESI_PUTI_IMM)
            {
                IA32FUNCTIONCALL("pu", ES_Execution_Context::UpdateNativeDispatcher);

                call.AllocateStackSpace();
                call.AddArgument(CONTEXT_POINTER);
                call.AddArgument(ES_CodeGenerator::IMMEDIATE(code->data->instruction_offsets[instruction_index + 1] + (constructor ? code->data->codewords_count : 0)));
                call.Call();
                call.FreeStackSpace();

                ES_CodeGenerator::JumpTarget *not_updated, *use_not_updated;

                if (!current_slow_case)
                    not_updated = use_not_updated = cg.ForwardJump();
                else
                {
                    not_updated = NULL;
                    use_not_updated = current_slow_case->GetContinuationTarget();
                }

                cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
                cg.Jump(use_not_updated, ES_NATIVE_CONDITION_ZERO);
                cg.JMP(ES_CodeGenerator::REG_AX);

                if (not_updated)
                    cg.SetJumpTarget(not_updated);
            }
        }
        break;
    }
}

void
ES_Native::EmitSlowCaseCall(BOOL failed_inlined_function)
{
    if (is_light_weight)
    {
        if (!light_weight_failure)
            EmitLightWeightFailure();

/* We don't have light-weight dispatchers with side-effects ATM. */
#if 1
        current_slow_case = light_weight_failure;
#else // 1
        if (is_side_effect_free)
            current_slow_case = light_weight_failure;
        else
        {
            current_slow_case = cg.StartOutOfOrderBlock();

            cg.MOV(ES_CodeGenerator::IMMEDIATE(cw_index), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(light_weight_failure->GetJumpTarget());
            cg.EndOutOfOrderBlock(FALSE);
        }
#endif // 0
    }
    else
    {
        current_slow_case = cg.StartOutOfOrderBlock();

#ifdef ES_SLOW_CASE_PROFILING
        extern BOOL g_slow_case_summary;
        if (g_slow_case_summary)
        {
            MovePointerIntoRegister(cg, context->rt_data->slow_case_calls + CurrentCodeWord()->instruction, ES_CodeGenerator::REG_AX);
            cg.ADD(ES_CodeGenerator::IMMEDIATE(1), ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_AX), ES_CodeGenerator::OPSIZE_32);
        }
#endif // ES_SLOW_CASE_PROFILING


        jump_to_continuation_point = TRUE;

        if (property_value_read_vr && property_value_nr)
        {
            CopyTypedDataToValue(cg, Native2Register(property_value_nr), property_value_offset, property_value_needs_type_check ? ES_STORAGE_WHATEVER : ES_STORAGE_OBJECT, property_value_read_vr, ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_DX);

            current_slow_case_main = cg.Here();
        }
        else
            current_slow_case_main = NULL;

        if (failed_inlined_function)
        {
            DECLARE_NOTHING();

#ifdef ARCHITECTURE_AMD64
            cg.MOV(ES_CodeGenerator::IMMEDIATE(1), OBJECT_MEMBER(CODE_POINTER, ES_Code, has_failed_inlined_function), ES_CodeGenerator::OPSIZE_32);
#else // ARCHITECTURE_AMD64
            cg.MOV(ES_CodeGenerator::IMMEDIATE(1), ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(code) + ES_OFFSETOF(ES_Code, has_failed_inlined_function)), ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64
        }

        EmitInstructionHandlerCall();

        cg.EndOutOfOrderBlock(jump_to_continuation_point);
    }

    if (property_value_fail)
    {
        cg.StartOutOfOrderBlock();
        cg.SetJumpTarget(property_value_fail);

        if (is_light_weight || current_slow_case_main == NULL)
            cg.Jump(current_slow_case->GetJumpTarget());
        else
            cg.Jump(current_slow_case_main);

        cg.EndOutOfOrderBlock(FALSE);
        property_value_fail = NULL;
    }
}

void
ES_Native::EmitExecuteBytecode(unsigned start_instruction_index, unsigned end_instruction_index, BOOL last_in_slow_case)
{
    IA32FUNCTIONCALL("puu", ES_Execution_Context::ExecuteBytecode);

    call.AllocateStackSpace();
    call.AddArgument(CONTEXT_POINTER);
    call.AddArgument(ES_CodeGenerator::IMMEDIATE(start_instruction_index));
    call.AddArgument(ES_CodeGenerator::IMMEDIATE(static_cast<int>(constructor ? ~end_instruction_index : end_instruction_index)));
    call.Call();
    call.FreeStackSpace();

    ES_CodeGenerator::JumpTarget *not_updated = last_in_slow_case ? current_slow_case->GetContinuationTarget() : cg.ForwardJump();

    cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(not_updated, ES_NATIVE_CONDITION_ZERO);
    cg.JMP(ES_CodeGenerator::REG_AX);

    if (!last_in_slow_case)
        cg.SetJumpTarget(not_updated);
}

static void
SetupNativeStackFrame(ES_CodeGenerator &cg, ES_Code *code, unsigned &stack_space_allocated, BOOL &has_variable_object, BOOL entry_point = FALSE)
{
    unsigned local_stack_space_allocated = 0;

    DECLARE_NOTHING();

    /* Push previous frame pointer. */
    cg.PUSH(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, native_stack_frame), ES_CodeGenerator::OPSIZE_POINTER);
    local_stack_space_allocated += sizeof(void *);

    /* Store new frame pointer in context. */
    cg.MOV(ES_CodeGenerator::REG_SP, OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, native_stack_frame), ES_CodeGenerator::OPSIZE_POINTER);

    /* Virtual register frame pointer. */
    cg.PUSH(REGISTER_FRAME_POINTER);
    local_stack_space_allocated += sizeof(void *);

#ifdef ARCHITECTURE_AMD64
    /* code => R15 (see definition of CODE_POINTER) */
    MovePointerIntoRegister(cg, code, CODE_POINTER);

    /* Pointer to current ES_Code object. */
    cg.PUSH(CODE_POINTER);
    local_stack_space_allocated += sizeof(void *);
#else // ARCHITECTURE_AMD64
    /* Pointer to current ES_Code object. */
    cg.PUSH(ES_CodeGenerator::IMMEDIATE(reinterpret_cast<UINTPTR>(code)), ES_CodeGenerator::OPSIZE_POINTER);
    local_stack_space_allocated += sizeof(void *);
#endif // ARCHITECTURE_AMD64

    if (code->type == ES_Code::TYPE_FUNCTION)
    {
        /* Arguments object (NULL initially.) */
        if (entry_point)
        {
            cg.PUSH(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, arguments_object), ES_CodeGenerator::OPSIZE_POINTER);
            cg.MOV(ES_CodeGenerator::ZERO(), OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, arguments_object), ES_CodeGenerator::OPSIZE_POINTER);
        }
        else
            cg.PUSH(ES_CodeGenerator::ZERO(), ES_CodeGenerator::OPSIZE_POINTER);
        local_stack_space_allocated += sizeof(void *);

        has_variable_object = code->CanHaveVariableObject();

        if (has_variable_object)
        {
            /* Variable object (NULL initially.) */
            if (entry_point)
            {
                cg.PUSH(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, variable_object), ES_CodeGenerator::OPSIZE_POINTER);
                cg.MOV(ES_CodeGenerator::ZERO(), OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, variable_object), ES_CodeGenerator::OPSIZE_POINTER);
            }
            else
                cg.PUSH(ES_CodeGenerator::ZERO(), ES_CodeGenerator::OPSIZE_POINTER);
            local_stack_space_allocated += sizeof(void *);
        }

        /* Argument count. */
        cg.PUSH(PARAMETERS_COUNT);
        local_stack_space_allocated += sizeof(void *);
    }
    else
        has_variable_object = FALSE;

#ifdef ARCHITECTURE_AMD64
    /* Align stack if necessary. */
    if ((local_stack_space_allocated + sizeof(void *)) & 15)
    {
        cg.SUB(ES_CodeGenerator::IMMEDIATE(sizeof(void *)), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
        local_stack_space_allocated += sizeof(void *);
    }
#elif defined USE_FASTCALL_CALLING_CONVENTION
    /* Align stack if necessary. */
    if ((local_stack_space_allocated + sizeof(void *)) & 7)
    {
        cg.SUB(ES_CodeGenerator::IMMEDIATE(sizeof(void *)), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
        local_stack_space_allocated += sizeof(void *);
    }
#else // USE_FASTCALL_CALLING_CONVENTION || ARCHITECTURE_AMD64
    /* Allocate space for two pointer arguments (and align the stack pointer.) */
    unsigned previously_allocated = local_stack_space_allocated;
    local_stack_space_allocated += 2 * sizeof(void *);
    while ((local_stack_space_allocated + sizeof(void *)) & (ES_STACK_ALIGNMENT - 1))
        local_stack_space_allocated += sizeof(void *);
    cg.SUB(ES_CodeGenerator::IMMEDIATE(local_stack_space_allocated - previously_allocated), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
#endif // USE_FASTCALL_CALLING_CONVENTION || ARCHITECTURE_AMD64

    if (!entry_point)
        stack_space_allocated = local_stack_space_allocated;
}

void
ES_Native::EmitLightWeightFailure()
{
    if (!light_weight_failure)
    {
        OP_ASSERT(!is_inlined_function_call);

        light_weight_failure = cg.StartOutOfOrderBlock();

        cg.MOV(ES_CodeGenerator::IMMEDIATE(static_cast<ES_FunctionCode *>(code)->GetData()->formals_count), PARAMETERS_COUNT);

        int align_adjust = ES_STACK_ALIGNMENT - sizeof(void *);
        if (align_adjust != 0)
            cg.ADD(ES_CodeGenerator::IMMEDIATE(align_adjust), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);

        light_weight_wrong_argc = cg.Here();

        BOOL has_variable_object;

        SetupNativeStackFrame(cg, code, stack_space_allocated, has_variable_object);

        IA32FUNCTIONCALL("p", ES_Execution_Context::LightWeightDispatcherFailure);

        call.AllocateStackSpace();
        call.AddArgument(CONTEXT_POINTER);
        call.Call();
        call.FreeStackSpace();

        ES_CodeGenerator::JumpTarget *no_entry_point = cg.ForwardJump();

        cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
        cg.Jump(no_entry_point, ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);
        cg.JMP(ES_CodeGenerator::REG_AX);
        cg.SetJumpTarget(no_entry_point);
        cg.ADD(ES_CodeGenerator::IMMEDIATE(stack_space_allocated), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
        cg.RET();

        cg.EndOutOfOrderBlock(FALSE);
    }
}

void
ES_Native::EmitInt32Arithmetic(Int32ArithmeticType type, const Operand &target, const Operand &lhs, const Operand &rhs, BOOL as_condition, ES_CodeGenerator::JumpTarget *overflow_target)
{
    ES_CodeGenerator::Operand cg_source, cg_target;

    OP_ASSERT(target.virtual_register || target.native_register);
    OP_ASSERT(!target.virtual_register || target.virtual_register == lhs.virtual_register || target.virtual_register == rhs.virtual_register);
    OP_ASSERT(!target.native_register || target.native_register == lhs.native_register || target.native_register == rhs.native_register || type == INT32ARITHMETIC_REM);
    OP_ASSERT(!(type == INT32ARITHMETIC_LSHIFT || type == INT32ARITHMETIC_RSHIFT_SIGNED || type == INT32ARITHMETIC_RSHIFT_UNSIGNED) || rhs.IsImmediate() || rhs.native_register && rhs.native_register->register_code == IA32_REGISTER_CODE_CX);

    if (target.virtual_register ? target.virtual_register == lhs.virtual_register : target.native_register == lhs.native_register)
        cg_source = OPERAND2OPERAND(rhs);
    else
    {
        OP_ASSERT(!(type == INT32ARITHMETIC_LSHIFT || type == INT32ARITHMETIC_RSHIFT_SIGNED || type == INT32ARITHMETIC_RSHIFT_UNSIGNED));
        cg_source = OPERAND2OPERAND(lhs);
    }

    if (cg_source.type == ES_CodeGenerator::Operand::TYPE_IMMEDIATE && (type == INT32ARITHMETIC_LSHIFT || type == INT32ARITHMETIC_RSHIFT_SIGNED || type == INT32ARITHMETIC_RSHIFT_UNSIGNED))
        cg_source.immediate = static_cast<unsigned>(cg_source.immediate) & 31;

    if (lhs.virtual_register && !lhs.codeword)
        UseInPlace(lhs.virtual_register);

    if (rhs.virtual_register && !rhs.codeword)
        UseInPlace(rhs.virtual_register);

    if (target.virtual_register && (target.virtual_register == lhs.virtual_register || target.virtual_register == rhs.virtual_register))
        UseInPlace(target.virtual_register);

    cg_target = OPERAND2OPERAND(target);

    switch (type)
    {
    case INT32ARITHMETIC_LSHIFT:
        cg.SHL(cg_source, cg_target, ES_CodeGenerator::OPSIZE_32);
        break;

    case INT32ARITHMETIC_RSHIFT_SIGNED:
        cg.SAR(cg_source, cg_target, ES_CodeGenerator::OPSIZE_32);
        break;

    case INT32ARITHMETIC_RSHIFT_UNSIGNED:
        if (!rhs.codeword || rhs.codeword->immediate != 0)
            cg.SHR(cg_source, cg_target, ES_CodeGenerator::OPSIZE_32);
        else if (overflow_target)
            cg.OR(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);

        if (overflow_target && (!rhs.codeword || rhs.codeword->immediate == 0))
            cg.Jump(overflow_target, ES_NATIVE_CONDITION_SIGN);
        break;

    case INT32ARITHMETIC_AND:
        cg.AND(cg_source, cg_target, ES_CodeGenerator::OPSIZE_32);
        break;

    case INT32ARITHMETIC_OR:
        cg.OR(cg_source, cg_target, ES_CodeGenerator::OPSIZE_32);
        break;

    case INT32ARITHMETIC_XOR:
        cg.XOR(cg_source, cg_target, ES_CodeGenerator::OPSIZE_32);
        break;

    case INT32ARITHMETIC_ADD:
    case INT32ARITHMETIC_SUB:
        if (type == INT32ARITHMETIC_ADD)
            if (cg_source.type != ES_CodeGenerator::Operand::TYPE_IMMEDIATE || cg_source.immediate != 0 || as_condition)
                cg.ADD(cg_source, cg_target, ES_CodeGenerator::OPSIZE_32);
            else
                overflow_target = NULL;
        else if (ES_CodeGenerator::IsSameRegister(cg_source, cg_target) && !as_condition)
        {
            /* 'x-x' is always zero, right? */
            cg.MOV(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);
            overflow_target = NULL;
        }
        else
            cg.SUB(cg_source, cg_target, ES_CodeGenerator::OPSIZE_32);

        if (overflow_target)
            cg.Jump(overflow_target, ES_NATIVE_CONDITION_OVERFLOW, TRUE, FALSE);
        break;

    case INT32ARITHMETIC_MUL:
        if (cg_source.type == ES_CodeGenerator::Operand::TYPE_IMMEDIATE)
        {
            if (overflow_target && cg_source.immediate == 0)
            {
                /* If the immediate is zero, then the result is negative zero if
                   the other operand is negative. */
                cg.CMP(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(overflow_target, ES_NATIVE_CONDITION_LESS);
            }

            cg.IMUL(cg_target, cg_source.immediate, cg_target, ES_CodeGenerator::OPSIZE_32);

            if (overflow_target)
                cg.Jump(overflow_target, ES_NATIVE_CONDITION_OVERFLOW, TRUE, FALSE);

            if (overflow_target && cg_source.immediate < 0)
            {
                /* If the immediate is negative, then a zero result should be
                   negative, since the only way it could occur (with an integer
                   operand) is if the integer operand was positive zero. */
                cg.CMP(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(overflow_target, ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);
            }
            else if (as_condition)
                cg.CMP(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);
        }
        else
        {
            if (overflow_target)
            {
                ES_CodeGenerator::OutOfOrderBlock *slow_case = cg.StartOutOfOrderBlock();

                ES_CodeGenerator::JumpTarget *first_not_zero = cg.ForwardJump();
                cg.Jump(first_not_zero, ES_NATIVE_CONDITION_NOT_ZERO);
                cg.CMP(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(overflow_target, ES_NATIVE_CONDITION_LESS);
                cg.Jump(slow_case->GetContinuationTarget());
                cg.SetJumpTarget(first_not_zero);
                cg.CMP(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(overflow_target, ES_NATIVE_CONDITION_ZERO);
                cg.EndOutOfOrderBlock();

                cg.CMP(ES_CodeGenerator::ZERO(), cg_source, ES_CodeGenerator::OPSIZE_32);
                /* If one operand is positive non-zero, the result can never be
                   negative zero, since that happens when one operand is zero
                   and the other is negative. */
                cg.Jump(slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_GREATER, TRUE, FALSE);

                cg.SetOutOfOrderContinuationPoint(slow_case);
            }

            cg.IMUL(cg_source, cg_target, ES_CodeGenerator::OPSIZE_32);

            if (overflow_target)
                cg.Jump(overflow_target, ES_NATIVE_CONDITION_OVERFLOW, TRUE, FALSE);

            if (as_condition)
                cg.CMP(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);
        }
        break;

    case INT32ARITHMETIC_DIV:
    case INT32ARITHMETIC_REM:
        OP_ASSERT(target.native_register == NR(type == INT32ARITHMETIC_DIV ? IA32_REGISTER_INDEX_AX : IA32_REGISTER_INDEX_DX));
        OP_ASSERT(lhs.native_register == NR(IA32_REGISTER_INDEX_AX) || rhs.native_register == NR(IA32_REGISTER_INDEX_AX));
        OP_ASSERT(lhs.native_register != NR(IA32_REGISTER_INDEX_DX) && rhs.native_register != NR(IA32_REGISTER_INDEX_DX));

        /* Note: overflow_target can only be NULL if the outcome is immediately truncated to an int32;
           that condition is not asserted for here, but assumed to hold. */

        ES_CodeGenerator::JumpTarget *done_target = NULL;

        ES_CodeGenerator::Operand op_rhs(Operand2Operand(rhs));

        BOOL emit_idivision = TRUE;
        if (op_rhs.type == ES_CodeGenerator::Operand::TYPE_IMMEDIATE)
        {
            if (op_rhs.immediate == 0)
            {
                if (overflow_target)
                {
                    cg.Jump(overflow_target);
                    return;
                }
                else
                {
                    cg.MOV(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);
                    emit_idivision = FALSE;
                }
            }
            else
            {
#ifdef ARCHITECTURE_AMD64
                op_rhs = ES_CodeGenerator::CONSTANT(cg.NewConstant(static_cast<int>(op_rhs.immediate)));
#else // ARCHITECTURE_AMD64
                op_rhs = ES_CodeGenerator::MEMORY(rhs.codeword);
#endif // ARCHITECTURE_AMD64
            }
        }
        else
        {
            cg.CMP(ES_CodeGenerator::ZERO(), op_rhs, ES_CodeGenerator::OPSIZE_32);
            if (overflow_target)
                cg.Jump(overflow_target, ES_NATIVE_CONDITION_EQUAL, TRUE, FALSE);
            else
            {
                ES_CodeGenerator::JumpTarget *is_non_zero = cg.ForwardJump();
                cg.Jump(is_non_zero, ES_NATIVE_CONDITION_NOT_EQUAL, FALSE, TRUE);
                cg.MOV(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);
                done_target = cg.ForwardJump();
                cg.Jump(done_target);
                cg.SetJumpTarget(is_non_zero);
            }
        }

        if (emit_idivision)
        {
            cg.CDQ();
            cg.IDIV(op_rhs, ES_CodeGenerator::OPSIZE_32);

            if (type == INT32ARITHMETIC_DIV)
            {
                cg.TEST(ES_CodeGenerator::REG_DX, ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(overflow_target, ES_NATIVE_CONDITION_NOT_ZERO, TRUE, FALSE);
            }
        }

        if (done_target)
            cg.SetJumpTarget(done_target);

        if (as_condition)
          cg.CMP(ES_CodeGenerator::ZERO(), Operand2Operand(target), ES_CodeGenerator::OPSIZE_32);
    }
}

void
ES_Native::EmitInt32Complement(const Operand &target, const Operand &source, BOOL as_condition)
{
    if (target.virtual_register)
        UseInPlace(target.virtual_register);

    ES_CodeGenerator::Operand cg_target;

    OP_ASSERT(target == source);

    cg_target = OPERAND2OPERAND(target);

    cg.NOT(cg_target, ES_CodeGenerator::OPSIZE_32);

    if (as_condition)
        cg.CMP(ES_CodeGenerator::ZERO(), cg_target, ES_CodeGenerator::OPSIZE_32);
}

void
ES_Native::EmitInt32IncOrDec(BOOL inc, const Operand &target, ES_CodeGenerator::JumpTarget *overflow_target)
{
    if (target.virtual_register)
        UseInPlace(target.virtual_register);

    ES_CodeGenerator::Operand cg_target(OPERAND2OPERAND(target));

    if (inc)
        cg.ADD(ES_CodeGenerator::IMMEDIATE(1), cg_target, ES_CodeGenerator::OPSIZE_32);
    else
        cg.SUB(ES_CodeGenerator::IMMEDIATE(1), cg_target, ES_CodeGenerator::OPSIZE_32);

    if (overflow_target)
    {
        if (IsPreConditioned(VR(code->data->codewords[cw_index + 1].index), ESTYPE_INT32))
        {
            DECLARE_NOTHING();

            ES_CodeGenerator::OutOfOrderBlock *ooo = cg.StartOutOfOrderBlock();

#ifdef ARCHITECTURE_AMD64
            cg.MOV(ES_CodeGenerator::IMMEDIATE(1), OBJECT_MEMBER(CODE_POINTER, ES_Code, has_failed_preconditions), ES_CodeGenerator::OPSIZE_32);
#else // ARCHITECTURE_AMD64
            cg.MOV(ES_CodeGenerator::IMMEDIATE(1), ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(code) + ES_OFFSETOF(ES_Code, has_failed_preconditions)), ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64

            cg.Jump(overflow_target);
            cg.EndOutOfOrderBlock(FALSE);

            overflow_target = ooo->GetJumpTarget();
        }

        cg.Jump(overflow_target, ES_NATIVE_CONDITION_OVERFLOW, TRUE, FALSE);
    }
}

void
ES_Native::EmitInt32Negate(const Operand &target, const Operand &source, BOOL as_condition, ES_CodeGenerator::JumpTarget *overflow_target)
{
    ES_CodeGenerator::Operand cg_target;

    if (target != source)
    {
        OP_ASSERT(!(target.virtual_register && source.virtual_register));
        cg.MOV(Operand2Operand(source), Operand2Operand(target), ES_CodeGenerator::OPSIZE_32);
    }
    else if (target.virtual_register)
        UseInPlace(target.virtual_register);

    cg_target = OPERAND2OPERAND(target);

    cg.NEG(cg_target, ES_CodeGenerator::OPSIZE_32);

    if (overflow_target)
    {
        cg.Jump(overflow_target, ES_NATIVE_CONDITION_ZERO);
        cg.Jump(overflow_target, ES_NATIVE_CONDITION_OVERFLOW, TRUE, FALSE);
    }
}

void
ES_Native::EmitInt32Relational(RelationalType relational_type, const Operand &lhs, const Operand &rhs, const Operand *value_target, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target, ArithmeticBlock *arithmetic_block)
{
    OP_ASSERT(!lhs.IsVirtual() || !rhs.IsVirtual());

    ES_CodeGenerator::Operand cg_source(OPERAND2OPERAND(rhs)), cg_target(OPERAND2OPERAND(lhs));

    if (cg_target.type == ES_CodeGenerator::Operand::TYPE_IMMEDIATE)
    {
        cg.CMP(cg_target, cg_source);

        switch (relational_type)
        {
        case RELATIONAL_LT:  relational_type = RELATIONAL_GT; break;
        case RELATIONAL_LTE: relational_type = RELATIONAL_GTE; break;
        case RELATIONAL_GT:  relational_type = RELATIONAL_LT; break;
        case RELATIONAL_GTE: relational_type = RELATIONAL_LTE; break;
        }
    }
    else
        cg.CMP(cg_source, cg_target);

    ES_CodeGenerator::JumpTarget *use_true_target = true_target;
    ES_CodeGenerator::JumpTarget *use_false_target = false_target;

    if (value_target)
    {
        use_false_target = cg.ForwardJump();

        if (value_target->virtual_register->stack_frame_offset == INT_MAX)
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), REGISTER_TYPE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
    }

    ES_CodeGenerator::Condition condition;
    ES_CodeGenerator::JumpTarget *target;

    if (use_false_target)
    {
        switch (relational_type)
        {
        case RELATIONAL_EQ:  condition = ES_NATIVE_CONDITION_NOT_EQUAL; break;
        case RELATIONAL_NEQ: condition = ES_NATIVE_CONDITION_EQUAL; break;
        case RELATIONAL_LT:  condition = ES_NATIVE_CONDITION_NOT_LESS; break;
        case RELATIONAL_LTE: condition = ES_NATIVE_CONDITION_NOT_LESS_EQUAL; break;
        case RELATIONAL_GT:  condition = ES_NATIVE_CONDITION_NOT_GREATER; break;
        default:             condition = ES_NATIVE_CONDITION_NOT_GREATER_EQUAL;
        }

        target = use_false_target;
    }
    else
    {
        switch (relational_type)
        {
        case RELATIONAL_EQ:  condition = ES_NATIVE_CONDITION_EQUAL; break;
        case RELATIONAL_NEQ: condition = ES_NATIVE_CONDITION_NOT_EQUAL; break;
        case RELATIONAL_LT:  condition = ES_NATIVE_CONDITION_LESS; break;
        case RELATIONAL_LTE: condition = ES_NATIVE_CONDITION_LESS_EQUAL; break;
        case RELATIONAL_GT:  condition = ES_NATIVE_CONDITION_GREATER; break;
        default:             condition = ES_NATIVE_CONDITION_GREATER_EQUAL;
        }

        target = use_true_target;
    }

#ifdef ES_VALUES_AS_NANS
    ES_CodeGenerator::OutOfOrderBlock *trampoline = NULL;
#endif // ES_VALUES_AS_NANS

    if (true_target || false_target)
    {
#ifdef ES_VALUES_AS_NANS
        if (arithmetic_block)
        {
            BOOL more_to_flush = FlushToVirtualRegisters(arithmetic_block, TRUE);

            if (more_to_flush)
            {
                trampoline = cg.StartOutOfOrderBlock();

                FlushToVirtualRegisters(arithmetic_block, FALSE, TRUE);

                if (true_target)
                {
                    cg.Jump(true_target);
                    if (target == true_target)
                        target = trampoline->GetJumpTarget();
                    true_target = trampoline->GetJumpTarget();
                }
                else
                {
                    cg.Jump(false_target);
                    if (target == false_target)
                        target = trampoline->GetJumpTarget();
                    false_target = trampoline->GetJumpTarget();
                }

                cg.EndOutOfOrderBlock(FALSE);
            }
        }
#else // ES_VALUES_AS_NANS
        if (arithmetic_block)
            FlushToVirtualRegisters(arithmetic_block);
#endif // ES_VALUES_AS_NANS
    }

    cg.Jump(target, condition);

    if (value_target)
    {
        if (!true_target)
            use_true_target = cg.ForwardJump();

        cg.MOV(ES_CodeGenerator::IMMEDIATE(1), REGISTER_VALUE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(use_true_target);
        cg.SetJumpTarget(use_false_target);
        cg.MOV(ES_CodeGenerator::ZERO(), REGISTER_VALUE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        if (false_target)
            cg.Jump(false_target);
        if (!true_target)
            cg.SetJumpTarget(use_true_target);
    }

#ifdef ES_VALUES_AS_NANS
    if (trampoline)
        FlushToVirtualRegisters(arithmetic_block, FALSE, FALSE);
#endif // ES_VALUES_AS_NANS
}

void
ES_Native::EmitDoubleArithmetic(DoubleArithmeticType type, const Operand &target, const Operand &lhs, const Operand &rhs, BOOL as_condition)
{
    OP_ASSERT(target.native_register && (target.native_register == lhs.native_register || target.native_register == rhs.native_register));

#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87)
    {
        ES_CodeGenerator::Operand cg_source;
        BOOL is_integer = FALSE;
        BOOL reverse = FALSE;

        if (lhs != rhs)
        {
            const Operand *other;

            if (lhs.virtual_register)
            {
                reverse = TRUE;
                other = &lhs;
            }
            else
                other = &rhs;

            OP_ASSERT(other->virtual_register);

            if (other->value)
            {
                is_integer = FALSE;

#ifdef ARCHITECTURE_AMD64
                ES_CodeGenerator::Constant *constant = cg.NewConstant(other->value->GetDouble());
                cg_source = ES_CodeGenerator::MEMORY(constant);
#else // ARCHITECTURE_AMD64
                cg_source = ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(other->value) + VALUE_VALUE_OFFSET);
#endif // ARCHITECTURE_AMD64
            }
            else if (other->codeword)
            {
                is_integer = TRUE;

#ifdef ARCHITECTURE_AMD64
                ES_CodeGenerator::Constant *constant = cg.NewConstant(other->codeword->immediate);
                cg_source = ES_CodeGenerator::MEMORY(constant);
#else // ARCHITECTURE_AMD64
                cg_source = ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(other->codeword));
#endif // ARCHITECTURE_AMD64
            }
            else
                cg_source = REGISTER_VALUE(other->virtual_register);
        }

        switch (type)
        {
        case DOUBLEARITHMETIC_ADD:
            if (lhs == rhs)
                cg.FADD(0, 0, FALSE);
            else if (is_integer)
                cg.FIADD(cg_source);
            else
                cg.FADD(cg_source);
            break;
        case DOUBLEARITHMETIC_SUB:
            if (lhs == rhs)
                cg.FSUB(0, 0, FALSE, FALSE);
            else if (is_integer)
                cg.FISUB(cg_source, reverse);
            else
                cg.FSUB(cg_source, reverse);
            break;
        case DOUBLEARITHMETIC_MUL:
            if (lhs == rhs)
                cg.FMUL(0, 0, FALSE);
            else if (is_integer)
                cg.FIMUL(cg_source);
            else
                cg.FMUL(cg_source);
            break;
        default:
            if (lhs == rhs)
                cg.FDIV(0, 0, FALSE, FALSE);
            else if (is_integer)
                cg.FIDIV(cg_source, reverse);
            else
                cg.FDIV(cg_source, reverse);
            break;
        }

        if (as_condition)
            cg.FUCOMI(0, TRUE);
    }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
    if (IS_FPMODE_SSE2)
    {
        OP_ASSERT(type != DOUBLEARITHMETIC_SUB || type != DOUBLEARITHMETIC_DIV || target.native_register == lhs.native_register);

        if (lhs.virtual_register)
            UseInPlace(lhs.virtual_register);
        if (rhs.virtual_register)
            UseInPlace(rhs.virtual_register);

        ES_CodeGenerator::Operand cg_source;

        if (target.native_register == lhs.native_register)
            cg_source = Operand2Operand(rhs, TRUE);
        else
            cg_source = Operand2Operand(lhs, TRUE);

        ES_CodeGenerator::XMM cg_target = static_cast<ES_CodeGenerator::XMM>(target.native_register->register_code);

        switch (type)
        {
        case DOUBLEARITHMETIC_ADD:
            cg.ADDSD(cg_source, cg_target);
            break;
        case DOUBLEARITHMETIC_SUB:
            cg.SUBSD(cg_source, cg_target);
            break;
        case DOUBLEARITHMETIC_MUL:
            cg.MULSD(cg_source, cg_target);
            break;
        default:
            cg.DIVSD(cg_source, cg_target);
            break;
        }

        if (as_condition)
            cg.UCOMISD(Zero(cg), cg_target);
    }
#endif // IA32_SSE2_SUPPORT
}

void
ES_Native::EmitDoubleRelational(RelationalType relational_type, const Operand &lhs, const Operand &rhs, const Operand *value_target, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target, ArithmeticBlock *arithmetic_block)
{
    BOOL swap_operands = FALSE;

#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87)
        swap_operands = lhs.native_register != NULL;
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
    if (IS_FPMODE_SSE2)
        swap_operands = lhs.native_register == NULL;
#endif // IA32_SSE2_SUPPORT

    Operand use_lhs, use_rhs;

    if (swap_operands)
    {
        switch (relational_type)
        {
        case RELATIONAL_LT:  relational_type = RELATIONAL_GT; break;
        case RELATIONAL_LTE: relational_type = RELATIONAL_GTE; break;
        case RELATIONAL_GT:  relational_type = RELATIONAL_LT; break;
        case RELATIONAL_GTE: relational_type = RELATIONAL_LTE;
        }

        use_lhs = rhs;
        use_rhs = lhs;
    }
    else
    {
        use_lhs = lhs;
        use_rhs = rhs;
    }

    if (lhs.virtual_register)
        UseInPlace(lhs.virtual_register);
    if (rhs.virtual_register)
        UseInPlace(rhs.virtual_register);

#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87)
        if (lhs == rhs)
            cg.FUCOMI(0, TRUE);
        else
        {
            ES_CodeGenerator::Operand cg_source;

            if (use_lhs.codeword)
                cg.FILD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(use_lhs.codeword)), ES_CodeGenerator::OPSIZE_32);
            else
            {
                if (use_lhs.value)
                {
#ifdef ARCHITECTURE_AMD64
                    ES_CodeGenerator::Constant *constant = cg.NewConstant(use_lhs.value->GetNumAsDouble());
                    cg_source = ES_CodeGenerator::CONSTANT(constant);
#else // ARCHITECTURE_AMD64
                    cg_source = ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(use_lhs.value) + VALUE_VALUE_OFFSET);
#endif // ARCHITECTURE_AMD64
                }
                else
                    cg_source = Operand2Operand(use_lhs, TRUE);

                cg.FLD(cg_source);
            }

            cg.FUCOMI(1, TRUE);
            cg.FST(0, TRUE);
        }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
    if (IS_FPMODE_SSE2)
    {
        OP_ASSERT(use_lhs.native_register && use_lhs.native_register->type == NativeRegister::TYPE_DOUBLE);
        OP_ASSERT(use_rhs.native_register || use_rhs.virtual_register);
        OP_ASSERT(!use_rhs.native_register || use_rhs.native_register->type == NativeRegister::TYPE_DOUBLE);

        cg.UCOMISD(Operand2Operand(use_rhs, TRUE), static_cast<ES_CodeGenerator::XMM>(use_lhs.native_register->register_code));
    }
#endif // IA32_SSE2_SUPPORT

    ES_CodeGenerator::JumpTarget *use_true_target = true_target;
    ES_CodeGenerator::JumpTarget *use_false_target = false_target;

    if (value_target)
    {
        use_false_target = cg.ForwardJump();

        if (value_target->virtual_register->stack_frame_offset == INT_MAX)
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), REGISTER_TYPE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
    }

    ES_CodeGenerator::Condition condition;
    ES_CodeGenerator::JumpTarget *target;

    if (use_false_target)
    {
        switch (relational_type)
        {
        case RELATIONAL_EQ:  condition = ES_NATIVE_CONDITION_NOT_EQUAL; break;
        case RELATIONAL_NEQ: condition = ES_NATIVE_CONDITION_EQUAL; break;
        case RELATIONAL_LT:  condition = ES_NATIVE_CONDITION_NOT_BELOW; break;
        case RELATIONAL_LTE: condition = ES_NATIVE_CONDITION_NOT_BELOW_EQUAL; break;
        case RELATIONAL_GT:  condition = ES_NATIVE_CONDITION_NOT_ABOVE; break;
        default:             condition = ES_NATIVE_CONDITION_NOT_ABOVE_EQUAL;
        }

        target = use_false_target;
    }
    else
    {
        switch (relational_type)
        {
        case RELATIONAL_EQ:  condition = ES_NATIVE_CONDITION_EQUAL; break;
        case RELATIONAL_NEQ: condition = ES_NATIVE_CONDITION_NOT_EQUAL; break;
        case RELATIONAL_LT:  condition = ES_NATIVE_CONDITION_BELOW; break;
        case RELATIONAL_LTE: condition = ES_NATIVE_CONDITION_BELOW_EQUAL; break;
        case RELATIONAL_GT:  condition = ES_NATIVE_CONDITION_ABOVE; break;
        default:             condition = ES_NATIVE_CONDITION_ABOVE_EQUAL;
        }

        target = use_true_target;
    }

#ifdef ES_VALUES_AS_NANS
    ES_CodeGenerator::OutOfOrderBlock *trampoline = NULL;
#endif // ES_VALUES_AS_NANS

    if (true_target || false_target)
    {
#ifdef ES_VALUES_AS_NANS
        if (arithmetic_block)
        {
            BOOL more_to_flush = FlushToVirtualRegisters(arithmetic_block, TRUE);

            if (more_to_flush && (true_target || false_target))
            {
                trampoline = cg.StartOutOfOrderBlock();

                FlushToVirtualRegisters(arithmetic_block, FALSE, TRUE);

                if (true_target)
                {
                    cg.Jump(true_target);
                    if (target == true_target)
                        target = trampoline->GetJumpTarget();
                    true_target = trampoline->GetJumpTarget();
                }
                else
                {
                    cg.Jump(false_target);
                    if (target == false_target)
                        target = trampoline->GetJumpTarget();
                    false_target = trampoline->GetJumpTarget();
                }

                cg.EndOutOfOrderBlock(FALSE);
            }
        }
#else // ES_VALUES_AS_NANS
        if (arithmetic_block)
            FlushToVirtualRegisters(arithmetic_block);
#endif // ES_VALUES_AS_NANS
    }

    ES_CodeGenerator::JumpTarget *nan_target, *skip = NULL;

    if (relational_type == RELATIONAL_NEQ)
        nan_target = use_true_target;
    else
        nan_target = use_false_target;

    if (nan_target)
        /* We have a direct jump target to use for unordered comparisons, so
           just jump there if the parity flag is set. */
        cg.Jump(nan_target, ES_NATIVE_CONDITION_PARITY, TRUE, FALSE);
    else
    {
        /* We don't have such a direct target, meaning execution is supposed to
           continue straight forward if the comparison is unordered.  What we
           need to do is skip pas the "real" conditional jump if the comparison
           was unordered. */
        skip = cg.ForwardJump();
        cg.Jump(skip, ES_NATIVE_CONDITION_PARITY, TRUE, FALSE);
    }

    cg.Jump(target, condition);

    if (skip)
        cg.SetJumpTarget(skip);

    if (value_target)
    {
        if (!true_target)
            use_true_target = cg.ForwardJump();

        cg.MOV(ES_CodeGenerator::IMMEDIATE(1), REGISTER_VALUE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(use_true_target);
        cg.SetJumpTarget(use_false_target);
        cg.MOV(ES_CodeGenerator::ZERO(), REGISTER_VALUE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        if (false_target)
            cg.Jump(false_target);
        if (!true_target)
            cg.SetJumpTarget(use_true_target);
    }

#ifdef ES_VALUES_AS_NANS
    if (trampoline)
        FlushToVirtualRegisters(arithmetic_block, FALSE, FALSE);
#endif // ES_VALUES_AS_NANS
}

#ifdef ARCHITECTURE_SSE
#ifndef ARCHITECTURE_AMD64
unsigned g_double_negate_mask_words[7] = { 0, 0, 0, 0, 0, 0, 0 }, *g_double_negate_mask = NULL;
#endif // ARCHITECTURE_AMD64
#endif // ARCHITECTURE_SSE

void
ES_Native::EmitDoubleNegate(const Operand &target, const Operand &source, BOOL as_condition)
{
    OP_ASSERT(target.native_register && target.native_register->type == NativeRegister::TYPE_DOUBLE);

    if (source.virtual_register)
        UseInPlace(source.virtual_register);

#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87)
    {
        if (target != source)
            cg.FLD(Operand2Operand(source, TRUE));

        cg.FCHS();
    }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
    if (IS_FPMODE_SSE2)
    {
        if (source != target)
            cg.MOVSD(Operand2Operand(source, TRUE), Operand2XMM(target));

#ifdef ARCHITECTURE_AMD64
        if (!negate_mask)
            negate_mask = cg.NewConstant(op_implode_double(0x80000000u, 0), 0.0);

        cg.XORPD(ES_CodeGenerator::CONSTANT(negate_mask), Operand2XMM(target));

        if (as_condition)
        {
            if (!zero)
                zero = cg.NewConstant(0.0);

            cg.UCOMISD(ES_CodeGenerator::CONSTANT(zero), Operand2XMM(target));
        }
#else // ARCHITECTURE_AMD64
        if (!g_double_negate_mask)
        {
            g_double_negate_mask = g_double_negate_mask_words;
            while ((reinterpret_cast<UINTPTR>(g_double_negate_mask) & 0xfu) != 0)
                ++g_double_negate_mask;
            g_double_negate_mask[1] = 0x80000000u;
        }

        cg.XORPD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(g_double_negate_mask)), Operand2XMM(target));

        if (as_condition)
            cg.UCOMISD(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(&g_ES_zero)), Operand2XMM(target));
#endif // ARCHITECTURE_AMD64
    }
#endif // IA32_SSE2_SUPPORT
}

void
ES_Native::EmitDoubleIncOrDec(BOOL inc, const Operand &target)
{
    OP_ASSERT(target.native_register && target.native_register->type == NativeRegister::TYPE_DOUBLE);

#ifdef IA32_X87_SUPPORT
    if (IS_FPMODE_X87)
    {
        cg.FLD1();

        if (inc)
            cg.FADD(0, 1, TRUE);
        else
            cg.FSUB(0, 1, TRUE, FALSE);
    }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
    if (IS_FPMODE_SSE2)
        if (inc)
            cg.ADDSD(One(cg), Operand2XMM(target));
        else
            cg.SUBSD(One(cg), Operand2XMM(target));
#endif // IA32_SSE2_SUPPORT
}

void
ES_Native::EmitNullOrUndefinedComparison(BOOL eq, VirtualRegister *vr, const Operand *value_target, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target)
{
#ifdef ES_VALUES_AS_NANS
    ES_CodeGenerator::JumpTarget *use_true_target, *use_false_target;

    if (value_target)
    {
        use_true_target = NULL;
        use_false_target = cg.ForwardJump();
    }
    else
    {
        use_true_target = true_target;
        use_false_target = false_target;
    }

    if (use_true_target)
    {
        ES_CodeGenerator::JumpTarget *is_equal = eq ? use_true_target : cg.ForwardJump();
        ES_CodeGenerator::JumpTarget *is_not_equal = eq ? cg.ForwardJump() : use_true_target;

        cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_NULL), REGISTER_TYPE(vr), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(is_equal, ES_NATIVE_CONDITION_EQUAL);
        cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_UNDEFINED), REGISTER_TYPE(vr), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(is_equal, ES_NATIVE_CONDITION_EQUAL);
        cg.Jump(is_not_equal);

        if (is_equal != use_true_target)
            cg.SetJumpTarget(is_equal);
        if (is_not_equal != use_true_target)
            cg.SetJumpTarget(is_not_equal);
    }
    else
    {
        ES_CodeGenerator::JumpTarget *is_equal = eq ? cg.ForwardJump() : use_false_target;
        ES_CodeGenerator::JumpTarget *is_not_equal = eq ? use_false_target : cg.ForwardJump();

        cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_NULL), REGISTER_TYPE(vr), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(is_equal, ES_NATIVE_CONDITION_EQUAL);
        cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_UNDEFINED), REGISTER_TYPE(vr), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(is_equal, ES_NATIVE_CONDITION_EQUAL);
        cg.Jump(is_not_equal);

        if (is_equal != use_false_target)
            cg.SetJumpTarget(is_equal);
        if (is_not_equal != use_false_target)
            cg.SetJumpTarget(is_not_equal);
    }

    if (value_target)
    {
        use_true_target = true_target ? true_target : cg.ForwardJump();

        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), REGISTER_TYPE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(1), REGISTER_VALUE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(use_true_target);
        cg.SetJumpTarget(use_false_target);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), REGISTER_TYPE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::ZERO(), REGISTER_VALUE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);

        if (false_target)
            cg.Jump(false_target);

        if (use_true_target != true_target)
            cg.SetJumpTarget(use_true_target);
    }
#else // ES_VALUES_AS_NANS
    if (value_target)
        cg.XOR(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);

    cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_NULL + 1), REGISTER_TYPE(vr), ES_CodeGenerator::OPSIZE_32);

    if (value_target)
    {
        cg.ADC(ES_CodeGenerator::ZERO(), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
        if (!eq)
            cg.XOR(ES_CodeGenerator::IMMEDIATE(1), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), REGISTER_TYPE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);

        if (true_target)
            cg.Jump(true_target, ES_NATIVE_CONDITION_NOT_ZERO);
        else if (false_target)
            cg.Jump(false_target, ES_NATIVE_CONDITION_ZERO);
    }
    else if (true_target)
        cg.Jump(true_target, eq ? ES_NATIVE_CONDITION_BELOW : ES_NATIVE_CONDITION_NOT_BELOW);
    else
        cg.Jump(false_target, !eq ? ES_NATIVE_CONDITION_BELOW : ES_NATIVE_CONDITION_NOT_BELOW);
#endif // ARCHITECTURE_IA32
}

void
ES_Native::EmitStrictNullOrUndefinedComparison(BOOL eq, VirtualRegister *vr, ES_ValueType type, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target)
{
#ifdef ES_VALUES_AS_NANS
    cg.CMP(ES_CodeGenerator::IMMEDIATE(type), REGISTER_TYPE(vr), ES_CodeGenerator::OPSIZE_32);
    if (true_target)
        cg.Jump(true_target, eq ? ES_NATIVE_CONDITION_EQUAL : ES_NATIVE_CONDITION_NOT_EQUAL);
    else
        cg.Jump(false_target, !eq ? ES_NATIVE_CONDITION_EQUAL : ES_NATIVE_CONDITION_NOT_EQUAL);
#else // ES_VALUES_AS_NANS
    cg.TEST(ES_CodeGenerator::IMMEDIATE(type), REGISTER_TYPE(vr), ES_CodeGenerator::OPSIZE_32);
    if (true_target)
        cg.Jump(true_target, eq ? ES_NATIVE_CONDITION_NOT_ZERO : ES_NATIVE_CONDITION_ZERO);
    else
        cg.Jump(false_target, !eq ? ES_NATIVE_CONDITION_NOT_ZERO : ES_NATIVE_CONDITION_ZERO);
#endif // ARCHITECTURE_IA32
}

void
ES_Native::EmitComparison(BOOL eq, BOOL strict, VirtualRegister *lhs, VirtualRegister *rhs, unsigned handled_types, const Operand *value_target, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target)
{
    if (!eq)
    {
        ES_CodeGenerator::JumpTarget *temporary = false_target;
        false_target = true_target;
        true_target = temporary;
    }

    ES_CodeGenerator::JumpTarget *use_true_target = true_target;
    ES_CodeGenerator::JumpTarget *use_false_target = false_target;

    if (value_target || !use_true_target)
        use_true_target = cg.ForwardJump();
    if (value_target || !use_false_target)
        use_false_target = cg.ForwardJump();

    cg.MOV(REGISTER_TYPE(rhs), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);

    ES_CodeGenerator::JumpTarget *jt_slow_case = cg.ForwardJump();

#ifdef ES_VALUES_AS_NANS
    /* A plain comparison between the type fields can't be trusted as 'not
       strictly equal' if both operands are doubles but can be if only one is.
       So we pick a random operand and check if it is a double, and if so fails
       to slow case.  (Typically, comparisons between numbers will not come this
       way because they are handled by arithmetic blocks.)  */
    cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
    cg.Jump(jt_slow_case, ES_NATIVE_CONDITION_NOT_GREATER, TRUE, FALSE);
#endif // ES_VALUES_AS_NANS

    cg.CMP(REGISTER_TYPE(lhs), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
    /* Strict (in)equality cannot be determined either by just comparing type tags;
       an integral value could be represented as both a double and an int32 (including +/- 0).
       Hence, falling into EqStrict() and its checks is required to confirm.

       An optimisation would be to test for numeric tags before taking the hit of a slow call.
     */
    cg.Jump(jt_slow_case, ES_NATIVE_CONDITION_NOT_EQUAL);

    if (handled_types)
    {
        ES_CodeGenerator::JumpTarget *jt_next = NULL;

        if (handled_types & ESTYPE_BITS_NULL)
        {
            jt_next = cg.ForwardJump();

            cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_NULL), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(jt_next, ES_NATIVE_CONDITION_NOT_EQUAL);
            cg.Jump(use_true_target);
        }

        if (handled_types & ESTYPE_BITS_UNDEFINED)
        {
            if (jt_next)
                cg.SetJumpTarget(jt_next);
            jt_next = cg.ForwardJump();

            cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_UNDEFINED), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(jt_next, ES_NATIVE_CONDITION_NOT_EQUAL);
            cg.Jump(use_true_target);
        }

        if (handled_types & (ESTYPE_BITS_BOOLEAN | ESTYPE_BITS_INT32))
        {
            if (jt_next)
                cg.SetJumpTarget(jt_next);
            jt_next = cg.ForwardJump();

#ifdef ES_VALUES_AS_NANS
            if ((handled_types & (ESTYPE_BITS_BOOLEAN | ESTYPE_BITS_INT32)) == (ESTYPE_BITS_BOOLEAN | ESTYPE_BITS_INT32))
            {
                ES_CodeGenerator::JumpTarget *jt_handled = cg.ForwardJump();
                cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(jt_handled, ES_NATIVE_CONDITION_EQUAL);
                cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(jt_next, ES_NATIVE_CONDITION_NOT_EQUAL);
                cg.SetJumpTarget(jt_handled);
            }
            else
            {
                ES_ValueType type = (handled_types & ESTYPE_BITS_BOOLEAN) ? ESTYPE_BOOLEAN : ESTYPE_INT32;
                cg.CMP(ES_CodeGenerator::IMMEDIATE(type), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(jt_next, ES_NATIVE_CONDITION_NOT_EQUAL);
            }
#else // ES_VALUES_AS_NANS
            cg.TEST(ES_CodeGenerator::IMMEDIATE(((handled_types & ESTYPE_BITS_BOOLEAN) ? ESTYPE_BOOLEAN : 0) | ((handled_types & ESTYPE_BITS_INT32) ? ESTYPE_INT32 : 0)), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(jt_next, ES_NATIVE_CONDITION_ZERO);
#endif // ES_VALUES_AS_NANS

            cg.MOV(REGISTER_VALUE(lhs), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);
            cg.CMP(REGISTER_VALUE(rhs), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(use_false_target, ES_NATIVE_CONDITION_NOT_EQUAL);
            cg.Jump(use_true_target);
        }

        if (handled_types & ESTYPE_BITS_OBJECT)
        {
            DECLARE_NOTHING();

            if (jt_next)
                cg.SetJumpTarget(jt_next);
            jt_next = cg.ForwardJump();

            cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(jt_next, ES_NATIVE_CONDITION_NOT_EQUAL);

#ifdef ARCHITECTURE_AMD64_UNIX
            const ES_CodeGenerator::Register reg_lhs = ES_CodeGenerator::REG_SI, reg_rhs = ES_CodeGenerator::REG_DX;
#elif defined(ARCHITECTURE_AMD64_WINDOWS)
            const ES_CodeGenerator::Register reg_lhs = ES_CodeGenerator::REG_DX, reg_rhs = ES_CodeGenerator::REG_R8;
#else // ARCHITECTURE_AMD64
            const ES_CodeGenerator::Register reg_lhs = ES_CodeGenerator::REG_DX, reg_rhs = ES_CodeGenerator::REG_DI;
#endif // ARCHITECTURE_AMD64

            cg.MOV(REGISTER_VALUE(lhs), reg_lhs, ES_CodeGenerator::OPSIZE_POINTER);
            cg.MOV(REGISTER_VALUE(rhs), reg_rhs, ES_CodeGenerator::OPSIZE_POINTER);
            cg.CMP(reg_lhs, reg_rhs, ES_CodeGenerator::OPSIZE_POINTER);
            cg.Jump(use_true_target, ES_NATIVE_CONDITION_EQUAL);

            ES_CodeGenerator::OutOfOrderBlock *compare_shifty_objects = cg.StartOutOfOrderBlock();

            IA32FUNCTIONCALL("ppp", ES_Native::CompareShiftyObjects);

            call.AllocateStackSpace();
            call.AddArgument(CONTEXT_POINTER);
            call.AddArgument(reg_lhs);
            call.AddArgument(reg_rhs);
            call.Call();
            call.FreeStackSpace();

            cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(use_true_target, ES_NATIVE_CONDITION_NOT_ZERO);
            cg.Jump(use_false_target);

            cg.EndOutOfOrderBlock(FALSE);

            cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_MULTIPLE_IDENTITIES), OBJECT_MEMBER(reg_lhs, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
            cg.Jump(compare_shifty_objects->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_ZERO, TRUE, FALSE);

            cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_MULTIPLE_IDENTITIES), OBJECT_MEMBER(reg_rhs, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
            cg.Jump(compare_shifty_objects->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_ZERO, TRUE, FALSE);

            cg.Jump(use_false_target);
        }

        if (handled_types & ESTYPE_BITS_STRING)
        {
            DECLARE_NOTHING();

            if (jt_next)
                cg.SetJumpTarget(jt_next);
            jt_next = cg.ForwardJump();

            cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_STRING), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(jt_next, ES_NATIVE_CONDITION_NOT_EQUAL);

#ifdef ARCHITECTURE_AMD64_UNIX
            const ES_CodeGenerator::Register reg_lhs = ES_CodeGenerator::REG_DI, reg_rhs = ES_CodeGenerator::REG_SI;
#elif defined(ARCHITECTURE_AMD64_WINDOWS)
            const ES_CodeGenerator::Register reg_lhs = ES_CodeGenerator::REG_CX, reg_rhs = ES_CodeGenerator::REG_DX;
#else // ARCHITECTURE_AMD64
            const ES_CodeGenerator::Register reg_lhs = ES_CodeGenerator::REG_CX, reg_rhs = ES_CodeGenerator::REG_DX;
#endif // ARCHITECTURE_AMD64

            cg.MOV(REGISTER_VALUE(lhs), reg_lhs, ES_CodeGenerator::OPSIZE_POINTER);
            cg.MOV(REGISTER_VALUE(rhs), reg_rhs, ES_CodeGenerator::OPSIZE_POINTER);
            cg.CMP(reg_lhs, reg_rhs, ES_CodeGenerator::OPSIZE_POINTER);
            cg.Jump(use_true_target, ES_NATIVE_CONDITION_EQUAL);

            cg.MOV(OBJECT_MEMBER(reg_lhs, JString, length), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.CMP(OBJECT_MEMBER(reg_rhs, JString, length), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(use_false_target, ES_NATIVE_CONDITION_NOT_EQUAL);

            ES_CodeGenerator::JumpTarget *call_equals = cg.ForwardJump();

            cg.CMP(ES_CodeGenerator::IMMEDIATE(1), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(call_equals, ES_NATIVE_CONDITION_NOT_EQUAL);

            cg.MOV(ES_CodeGenerator::MEMORY(reg_lhs), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.OR(ES_CodeGenerator::MEMORY(reg_rhs), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.AND(ES_CodeGenerator::IMMEDIATE(ES_Header::MASK_IS_BUILTIN_STRING), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(use_false_target, ES_NATIVE_CONDITION_NOT_ZERO);

            cg.SetJumpTarget(call_equals);

            IA32FUNCTIONCALL("pp", Equals);

            call.AllocateStackSpace();
            call.AddArgument(reg_lhs);
            call.AddArgument(reg_rhs);
            call.Call();
            call.FreeStackSpace();

            cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(use_true_target, ES_NATIVE_CONDITION_NOT_ZERO);
            cg.Jump(use_false_target);
        }

        if (jt_next)
        {
            ES_CodeGenerator::OutOfOrderBlock *slow_case = cg.StartOutOfOrderBlock();

            cg.SetJumpTarget(jt_next);

            IA32FUNCTIONCALL("pu", ES_Execution_Context::UpdateComparison);

            call.AllocateStackSpace();
            call.AddArgument(CONTEXT_POINTER);
            call.AddArgument(ES_CodeGenerator::IMMEDIATE(cw_index));
            call.Call();
            call.FreeStackSpace();

            cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
            cg.Jump(slow_case->GetContinuationTarget(), ES_NATIVE_CONDITION_ZERO);
            cg.JMP(ES_CodeGenerator::REG_AX);

            cg.EndOutOfOrderBlock(FALSE);
            cg.SetOutOfOrderContinuationPoint(slow_case);
        }
    }

    cg.SetJumpTarget(jt_slow_case);

#ifdef ARCHITECTURE_AMD64_UNIX
    const ES_CodeGenerator::Register reg_lhs = ES_CodeGenerator::REG_SI, reg_rhs = ES_CodeGenerator::REG_DX;
#elif defined(ARCHITECTURE_AMD64_WINDOWS)
    const ES_CodeGenerator::Register reg_lhs = ES_CodeGenerator::REG_DX, reg_rhs = ES_CodeGenerator::REG_R8;
#else // ARCHITECTURE_AMD64
    const ES_CodeGenerator::Register reg_lhs = ES_CodeGenerator::REG_DX, reg_rhs = ES_CodeGenerator::REG_AX;
#endif // ARCHITECTURE_AMD64

    cg.LEA(VALUE_INDEX_MEMORY(REGISTER_FRAME_POINTER, lhs->index), reg_lhs, ES_CodeGenerator::OPSIZE_POINTER);
    cg.LEA(VALUE_INDEX_MEMORY(REGISTER_FRAME_POINTER, rhs->index), reg_rhs, ES_CodeGenerator::OPSIZE_POINTER);

    if (strict)
    {
        IA32FUNCTIONCALL("ppp", ES_Execution_Context::EqStrict);

        call.AllocateStackSpace();
        call.AddArgument(CONTEXT_POINTER);
        call.AddArgument(reg_lhs);
        call.AddArgument(reg_rhs);
        call.Call();
        call.FreeStackSpace();
    }
    else
    {
        IA32FUNCTIONCALL("pppu", ES_Execution_Context::EqFromMachineCode);

        call.AllocateStackSpace();
        call.AddArgument(CONTEXT_POINTER);
        call.AddArgument(reg_lhs);
        call.AddArgument(reg_rhs);
        call.AddArgument(ES_CodeGenerator::IMMEDIATE(cw_index));
        call.Call();
        call.FreeStackSpace();
    }

    cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
    cg.Jump(use_true_target, ES_NATIVE_CONDITION_NOT_ZERO);
    cg.Jump(use_false_target);

    if (value_target)
    {
        ES_CodeGenerator::JumpTarget *jt_when_true = true_target ? true_target : cg.ForwardJump();

        cg.SetJumpTarget(use_true_target);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), REGISTER_TYPE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(eq ? 1 : 0), REGISTER_VALUE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(jt_when_true);

        cg.SetJumpTarget(use_false_target);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), REGISTER_TYPE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(eq ? 0 : 1), REGISTER_VALUE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        if (false_target)
            cg.Jump(false_target);

        if (!true_target)
            cg.SetJumpTarget(jt_when_true);
    }
    else
    {
        if (!true_target)
            cg.SetJumpTarget(use_true_target);
        if (!false_target)
            cg.SetJumpTarget(use_false_target);
    }
}

static void
GetPrototypeFromObject(ES_CodeGenerator &cg, ES_CodeGenerator::Register reg)
{
    DECLARE_NOTHING();

    cg.MOV(OBJECT_MEMBER(reg, ES_Object, klass), reg, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(OBJECT_MEMBER(reg, ES_Class, static_data), reg, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(OBJECT_MEMBER(reg, ES_Class_Data, prototype), reg, ES_CodeGenerator::OPSIZE_POINTER);
}

int ES_CALLING_CONVENTION
InstanceOf(ES_Object *constructor, ES_Execution_Context *context, ES_Object *object) REALIGN_STACK;

int ES_CALLING_CONVENTION
InstanceOf(ES_Object *constructor, ES_Execution_Context *context, ES_Object *object)
{
    return object->InstanceOf(context, constructor);
}

void
ES_Native::EmitInstanceOf(VirtualRegister *vr_object, VirtualRegister *vr_constructor, const Operand *value_target, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::JumpTarget *use_true_target = true_target;
    ES_CodeGenerator::JumpTarget *use_false_target = false_target;

    if (value_target || !true_target)
        use_true_target = cg.ForwardJump();
    if (value_target || !false_target)
        use_false_target = cg.ForwardJump();

    if (!current_slow_case)
        EmitSlowCaseCall();

    /* right-hand side not an object => exception (because it must be a
       function, and ToObject() never produces a function) */
    if (!IsType(vr_constructor, ESTYPE_OBJECT))
    {
        cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(vr_constructor), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);
    }

    ES_CodeGenerator::Register constructor(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register bits(ES_CodeGenerator::REG_DX);

    /* right-hand side not a function => exception */
    cg.MOV(REGISTER_VALUE(vr_constructor), constructor, ES_CodeGenerator::OPSIZE_POINTER);

    cg.MOV(ES_CodeGenerator::MEMORY(constructor), bits, ES_CodeGenerator::OPSIZE_32);
    cg.AND(ES_CodeGenerator::IMMEDIATE(ES_Header::MASK_GCTAG), bits, ES_CodeGenerator::OPSIZE_32);
    cg.CMP(ES_CodeGenerator::IMMEDIATE(GCTAG_ES_Object_Function), bits, ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);

    /* left-hand side not an object => false */
    if (!IsType(vr_object, ESTYPE_OBJECT))
    {
        cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(vr_object), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(use_false_target, ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);
    }

    ES_CodeGenerator::JumpTarget *simple_case = cg.ForwardJump();
    ES_CodeGenerator::Register constructor_class(ES_CodeGenerator::REG_DX);

    cg.MOV(OBJECT_MEMBER(constructor, ES_Object, klass), constructor_class, ES_CodeGenerator::OPSIZE_POINTER);
    cg.CMP(ES_CodeGenerator::IMMEDIATE(code->global_object->GetNativeFunctionWithPrototypeClass()->GetId(context)), OBJECT_MEMBER(constructor_class, ES_Class, class_id), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(simple_case, ES_NATIVE_CONDITION_EQUAL);
    cg.CMP(ES_CodeGenerator::IMMEDIATE(code->global_object->GetBuiltInConstructorClass()->GetId(context)), OBJECT_MEMBER(constructor_class, ES_Class, class_id), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(simple_case, ES_NATIVE_CONDITION_EQUAL, TRUE, TRUE);

    IA32FUNCTIONCALL("ppp", ::InstanceOf);

    call.AllocateStackSpace();
    call.AddArgument(constructor);
    call.AddArgument(CONTEXT_POINTER);
    call.AddArgument(REGISTER_VALUE(vr_object));
    call.Call();
    call.FreeStackSpace();

    cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
    cg.Jump(use_true_target, ES_NATIVE_CONDITION_NOT_ZERO);
    cg.Jump(use_false_target);

    cg.SetJumpTarget(simple_case);

    ES_CodeGenerator::Register constructor_properties(ES_CodeGenerator::REG_DX);
    ES_CodeGenerator::Register constructor_prototype(ES_CodeGenerator::REG_DX);

    /* constructor.prototype not an object => exception */
    cg.MOV(OBJECT_MEMBER(constructor, ES_Object, properties), constructor_properties, ES_CodeGenerator::OPSIZE_POINTER);
    unsigned prototype_offset = ES_Function::GetPropertyOffset(ES_Function::PROTOTYPE);
    cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), ES_CodeGenerator::MEMORY(constructor_properties, prototype_offset + VALUE_TYPE_OFFSET), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(ES_CodeGenerator::MEMORY(constructor_properties, prototype_offset + VALUE_VALUE_OFFSET), constructor_prototype, ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);

    ES_CodeGenerator::Register object_prototype(ES_CodeGenerator::REG_CX);

    cg.MOV(REGISTER_VALUE(vr_object), object_prototype, ES_CodeGenerator::OPSIZE_POINTER);
    GetPrototypeFromObject(cg, object_prototype);

    ES_CodeGenerator::JumpTarget *loop = cg.Here(TRUE);

    cg.CMP(object_prototype, constructor_prototype, ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(use_true_target, ES_NATIVE_CONDITION_EQUAL);

    GetPrototypeFromObject(cg, object_prototype);

    cg.TEST(object_prototype, object_prototype, ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(loop, ES_NATIVE_CONDITION_NOT_ZERO);

    cg.Jump(use_false_target);

    cg.SetOutOfOrderContinuationPoint(current_slow_case);
    current_slow_case = NULL;

    cg.CMP(ES_CodeGenerator::ZERO(), OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, implicit_bool), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(use_true_target, ES_NATIVE_CONDITION_NOT_EQUAL);
    cg.Jump(use_false_target);

    if (value_target)
    {
        ES_CodeGenerator::JumpTarget *jt_when_true = true_target ? true_target : cg.ForwardJump();

        cg.SetJumpTarget(use_true_target);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), REGISTER_TYPE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(1), REGISTER_VALUE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(jt_when_true);

        cg.SetJumpTarget(use_false_target);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), REGISTER_TYPE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::ZERO(), REGISTER_VALUE(value_target->virtual_register), ES_CodeGenerator::OPSIZE_32);
        if (false_target)
            cg.Jump(false_target);

        if (!true_target)
            cg.SetJumpTarget(jt_when_true);
    }
    else
    {
        if (!true_target)
            cg.SetJumpTarget(use_true_target);
        if (!false_target)
            cg.SetJumpTarget(use_false_target);
    }
}

void
ES_Native::EmitConditionalJump(ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target, BOOL check_implicit_boolean, ArithmeticBlock *arithmetic_block)
{
#ifdef ES_VALUES_AS_NANS
    BOOL more_to_flush = FALSE;
#endif // ES_VALUES_AS_NANS

    if (check_implicit_boolean)
    {
        DECLARE_NOTHING();
        cg.CMP(ES_CodeGenerator::ZERO(), OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, implicit_bool), ES_CodeGenerator::OPSIZE_32);
    }
    else if (arithmetic_block)
    {
#ifdef ES_VALUES_AS_NANS
        more_to_flush = FlushToVirtualRegisters(arithmetic_block, TRUE);

        if (more_to_flush)
        {
            ES_CodeGenerator::OutOfOrderBlock *trampoline = cg.StartOutOfOrderBlock();

            FlushToVirtualRegisters(arithmetic_block, FALSE, TRUE);

            if (true_target)
            {
                cg.Jump(true_target);
                true_target = trampoline->GetJumpTarget();
            }
            else
            {
                cg.Jump(false_target);
                false_target = trampoline->GetJumpTarget();
            }

            cg.EndOutOfOrderBlock(FALSE);
        }
#else // ES_VALUES_AS_NANS
        FlushToVirtualRegisters(arithmetic_block);
#endif // ES_VALUES_AS_NANS
    }

    if (true_target)
        cg.Jump(true_target, ES_NATIVE_CONDITION_NOT_ZERO);
    else
        cg.Jump(false_target, ES_NATIVE_CONDITION_ZERO);

#ifdef ES_VALUES_AS_NANS
    if (more_to_flush)
        FlushToVirtualRegisters(arithmetic_block, FALSE, FALSE);
#endif // ES_VALUES_AS_NANS
}

void
ES_Native::EmitConditionalJumpOnValue(VirtualRegister *value, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target)
{
    ES_CodeGenerator::JumpTarget *simple_case = cg.ForwardJump();

    cg.MOV(REGISTER_VALUE(value), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);

#ifdef ES_VALUES_AS_NANS
    cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), REGISTER_TYPE(value), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(simple_case, ES_NATIVE_CONDITION_EQUAL, TRUE, TRUE);
    cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(value), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(simple_case, ES_NATIVE_CONDITION_EQUAL, TRUE, TRUE);
#else // ES_VALUES_AS_NANS
    cg.TEST(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN | ESTYPE_INT32), REGISTER_TYPE(value), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(simple_case, ES_NATIVE_CONDITION_NOT_ZERO, TRUE, TRUE);
#endif // ES_VALUES_AS_NANS

    IA32FUNCTIONCALL("p", ES_Value_Internal::ReturnAsBoolean);

#ifdef ARCHITECTURE_AMD64_UNIX
    const ES_CodeGenerator::Register pointer = ES_CodeGenerator::REG_DI;
#elif defined(ARCHITECTURE_AMD64_WINDOWS)
    const ES_CodeGenerator::Register pointer = ES_CodeGenerator::REG_CX;
#else // ARCHITECTURE_AMD64
    const ES_CodeGenerator::Register pointer = ES_CodeGenerator::REG_CX;
#endif // ARCHITECTURE_AMD64

    cg.LEA(VALUE_INDEX_MEMORY(REGISTER_FRAME_POINTER, value->index), pointer, ES_CodeGenerator::OPSIZE_POINTER);

    call.AllocateStackSpace();
    call.AddArgument(pointer);
    call.Call();
    call.FreeStackSpace();

    cg.SetJumpTarget(simple_case);
    cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);

    if (true_target)
        cg.Jump(true_target, ES_NATIVE_CONDITION_NOT_ZERO);
    else
        cg.Jump(false_target, ES_NATIVE_CONDITION_ZERO);
}

static void
EmitInitProperty(ES_CodeGenerator &cg, ES_Code *code, ES_CodeWord *word, const ES_CodeGenerator::Operand &op_type, const ES_CodeGenerator::Operand &op_value)
{
    ES_CodeGenerator::Register scratch1(ES_CodeGenerator::REG_CX);
    ES_CodeGenerator::Register scratch2(ES_CodeGenerator::REG_DX);

    switch (word->instruction)
    {
    case ESI_LOAD_STRING:
        {
            JString *value = code->GetString(word[2].index);

#ifdef ARCHITECTURE_AMD64
            MovePointerIntoRegister(cg, value, scratch1);
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_STRING), op_type, ES_CodeGenerator::OPSIZE_32);
            cg.MOV(scratch1, op_value, ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_STRING), op_type, ES_CodeGenerator::OPSIZE_32);
            cg.MOV(ES_CodeGenerator::IMMEDIATE(value), op_value, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64
        }
        break;

    case ESI_LOAD_DOUBLE:
        {
            double &value = code->data->doubles[word[2].index];

#ifdef ES_VALUES_AS_NANS
            if (op_isnan(value))
                cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), op_type, ES_CodeGenerator::OPSIZE_32);
            else
            {
                cg.MOVSD(ES_CodeGenerator::MEMORY(&value), ES_CodeGenerator::REG_XMM0);
                cg.MOVSD(ES_CodeGenerator::REG_XMM0, op_value);
            }
#else // ES_VALUES_AS_NANS
            ES_CodeGenerator::Constant *constant = cg.NewConstant(value);

            cg.MOVSD(ES_CodeGenerator::CONSTANT(constant), ES_CodeGenerator::REG_XMM0);
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), op_type, ES_CodeGenerator::OPSIZE_32);
            cg.MOVSD(ES_CodeGenerator::REG_XMM0, op_value);
#endif // ES_VALUES_AS_NANS
        }
        break;

    case ESI_LOAD_INT32:
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), op_type, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(word[2].immediate), op_value, ES_CodeGenerator::OPSIZE_32);
        break;

    case ESI_LOAD_NULL:
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_NULL), op_type, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::ZERO(), op_value, ES_CodeGenerator::OPSIZE_POINTER);
        break;

    case ESI_LOAD_UNDEFINED:
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_UNDEFINED), op_type, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(1), op_value, ES_CodeGenerator::OPSIZE_32); // "hidden boolean"
        break;

    case ESI_LOAD_TRUE:
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), op_type, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(1), op_value, ES_CodeGenerator::OPSIZE_32);
        break;

    case ESI_LOAD_FALSE:
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOOLEAN), op_type, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::ZERO(), op_value, ES_CodeGenerator::OPSIZE_32);
        break;

    case ESI_PUTI_IMM:
    case ESI_PUTN_IMM:
    case ESI_INIT_PROPERTY:
        {
            if (word->instruction == ESI_PUTI_IMM)
                cg.MOV(ES_CodeGenerator::IMMEDIATE(1), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);

            cg.MOV(VALUE_TYPE_INDEX(REGISTER_FRAME_POINTER, word[3].index), scratch1, ES_CodeGenerator::OPSIZE_32);
            cg.MOV(VALUE_VALUE_INDEX(REGISTER_FRAME_POINTER, word[3].index), scratch2, ES_CodeGenerator::OPSIZE_POINTER);

            if (word->instruction == ESI_PUTI_IMM)
            {
                cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_UNDEFINED), scratch1, ES_CodeGenerator::OPSIZE_32);
                cg.CMOV(ES_CodeGenerator::REG_AX, scratch2, ES_NATIVE_CONDITION_EQUAL, ES_CodeGenerator::OPSIZE_POINTER);
            }

            cg.MOV(scratch1, op_type, ES_CodeGenerator::OPSIZE_32);
            cg.MOV(scratch2, op_value, ES_CodeGenerator::OPSIZE_POINTER);
        }
        break;

    default:
        OP_ASSERT(FALSE);
    }
}

static void
JumpToSize(ES_CodeGenerator &cg, unsigned size, ES_CodeGenerator::JumpTarget *&size_4_target, ES_CodeGenerator::JumpTarget *&size_8_target, ES_CodeGenerator::JumpTarget *&size_16_target)
{
    switch (size)
    {
    case 4:
        if (!size_4_target)
            size_4_target = cg.ForwardJump();
        cg.Jump(size_4_target);
        break;
    case 8:
        if (!size_8_target)
            size_8_target = cg.ForwardJump();
        cg.Jump(size_8_target);
        break;
#ifndef ES_VALUES_AS_NANS
    case 16:
        if (!size_16_target)
            size_16_target = cg.ForwardJump();
        cg.Jump(size_16_target);
        break;
#endif // ES_VALUES_AS_NANS
    default: OP_ASSERT(!"Should never happen");
    }
}

void
ES_Native::EmitTypeConversionHandlers(ES_Native::VirtualRegister *source_vr, ES_CodeGenerator::Register properties, unsigned offset, ES_CodeGenerator::JumpTarget *null_target, ES_CodeGenerator::JumpTarget *int_to_double_target)
{
    ES_CodeGenerator::OutOfOrderBlock *int_to_double_block = NULL;
    if (int_to_double_target)
    {
        int_to_double_block = cg.StartOutOfOrderBlock();
        cg.SetJumpTarget(int_to_double_target);
#ifdef IA32_X87_SUPPORT
        if (IS_FPMODE_X87)
        {
            cg.FILD(REGISTER_VALUE(source_vr), ES_CodeGenerator::OPSIZE_32);
            cg.FST(ES_CodeGenerator::MEMORY(properties, offset), TRUE);
        }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
        if (IS_FPMODE_SSE2)
        {
#ifdef ARCHITECTURE_AMD64
            ES_CodeGenerator::XMM xmm = ES_CodeGenerator::REG_XMM15;
#else // ARCHITECTURE_AMD64
            ES_CodeGenerator::XMM xmm = ES_CodeGenerator::REG_XMM7;
#endif // ARCHITECTURE_AMD64
            cg.CVTSI2SD(REGISTER_VALUE(source_vr), xmm);
            cg.MOVSD(xmm, ES_CodeGenerator::MEMORY(properties, offset));
        }
#endif // IA32_SSE2_SUPPORT
        cg.EndOutOfOrderBlock();
    }

    ES_CodeGenerator::OutOfOrderBlock *null_block = NULL;
    if (null_target)
    {
        null_block = cg.StartOutOfOrderBlock();
        cg.SetJumpTarget(null_target);
        cg.MOV(ES_CodeGenerator::ZERO(), ES_CodeGenerator::MEMORY(properties, offset), ES_CodeGenerator::OPSIZE_POINTER);
        cg.EndOutOfOrderBlock();
    }

    if (int_to_double_block)
        cg.SetOutOfOrderContinuationPoint(int_to_double_block);

    if (null_block)
        cg.SetOutOfOrderContinuationPoint(null_block);
}

void
ES_Native::EmitNewObject(VirtualRegister *target_vr, ES_Class *klass, ES_CodeWord *first_property, unsigned nproperties)
{
    OP_ASSERT(nproperties == klass->Count());

    EmitSlowCaseCall();
    ES_CodeGenerator::JumpTarget *slow_case = current_slow_case->GetJumpTarget();
    EmitAllocateObject(klass, klass, 0, NULL, NULL, slow_case);

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX); // see EmitAllocateObject()
    ES_CodeGenerator::Register properties(ES_CodeGenerator::REG_DI); // see EmitAllocateObject()

    cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(target_vr), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(object, REGISTER_VALUE(target_vr), ES_CodeGenerator::OPSIZE_POINTER);

    for (unsigned index = 0; index < nproperties; ++index)
    {
        ES_Layout_Info layout = klass->GetLayoutInfoAtIndex(ES_LayoutIndex(index));
        ES_StorageType type = layout.GetStorageType();

        ES_CodeGenerator::JumpTarget *null_target = NULL, *int_to_double_target = NULL;;

        ES_ValueType value_type;
        VirtualRegister *source_vr = VR(first_property[index].index);
        BOOL known_type = GetType(source_vr, value_type);
        ES_StorageType source_type = ES_STORAGE_WHATEVER;
        if (known_type)
            source_type = ES_Value_Internal::ConvertToStorageType(value_type);

        BOOL skip_write = FALSE;
        if (type != ES_STORAGE_WHATEVER && source_type != type)
            skip_write = EmitTypeConversionCheck(type, source_type, source_vr, current_slow_case->GetJumpTarget(), null_target, int_to_double_target);

        if (!skip_write)
            CopyValueToTypedData(cg, source_vr, properties, layout.GetOffset(), layout.GetStorageType(), ES_CodeGenerator::REG_CX, ES_CodeGenerator::REG_DX);

        EmitTypeConversionHandlers(source_vr, properties, layout.GetOffset(), null_target, int_to_double_target);
    }

    EmitNewObjectSetPropertyCount(nproperties);
}

void
ES_Native::EmitNewObjectValue(ES_CodeWord *word, unsigned offset)
{
    ES_CodeGenerator::Register properties(ES_CodeGenerator::REG_DI); // see EmitAllocateObject()
    ES_CodeGenerator::Operand op_type(ES_CodeGenerator::MEMORY(properties, offset + VALUE_TYPE_OFFSET));
    ES_CodeGenerator::Operand op_value(ES_CodeGenerator::MEMORY(properties, offset + VALUE_VALUE_OFFSET));

    EmitInitProperty(cg, code, word, op_type, op_value);
}

void
ES_Native::EmitNewObjectSetPropertyCount(unsigned nproperties)
{
    DECLARE_NOTHING();
    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX); // see EmitAllocateObject()
    cg.MOV(ES_CodeGenerator::IMMEDIATE(nproperties), OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);
}

void
ES_Native::EmitNewArray(VirtualRegister *target_vr, unsigned length, unsigned &capacity, ES_Compact_Indexed_Properties *representation)
{
    DECLARE_NOTHING();

    ES_Class *klass = code->global_object->GetArrayClass();

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register indexed_properties(ES_CodeGenerator::REG_SI); // see EmitAllocateObject()

    if (CanAllocateObject(klass, length))
    {
        ES_CodeGenerator::OutOfOrderBlock *slow_case = cg.StartOutOfOrderBlock();

        EmitInstructionHandlerCall();

        if (!representation)
        {
            cg.MOV(REGISTER_VALUE(target_vr), object, ES_CodeGenerator::OPSIZE_POINTER);
            cg.MOV(OBJECT_MEMBER(object, ES_Object, indexed_properties), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);
        }

        cg.EndOutOfOrderBlock();

        ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX); // see EmitAllocateObject()
        EmitAllocateObject(klass, klass, 1, &length, representation, slow_case->GetJumpTarget());

        capacity = length;

        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(target_vr), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(object, REGISTER_VALUE(target_vr), ES_CodeGenerator::OPSIZE_POINTER);

        cg.SetOutOfOrderContinuationPoint(slow_case);
    }
    else
    {
        capacity = length;

        EmitInstructionHandlerCall();

        if (!representation)
        {
            cg.MOV(REGISTER_VALUE(target_vr), object, ES_CodeGenerator::OPSIZE_POINTER);
            cg.MOV(OBJECT_MEMBER(object, ES_Object, indexed_properties), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);
        }
    }
}

void
ES_Native::EmitNewArrayValue(VirtualRegister *target_vr, ES_CodeWord *word, unsigned index)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::Register indexed_properties(ES_CodeGenerator::REG_SI); // see EmitAllocateObject()
    ES_CodeGenerator::Operand op_type(ES_CodeGenerator::MEMORY(indexed_properties, ES_OFFSETOF(ES_Compact_Indexed_Properties, values) + VALUE_INDEX_TO_OFFSET(index) + VALUE_TYPE_OFFSET));
    ES_CodeGenerator::Operand op_value(ES_CodeGenerator::MEMORY(indexed_properties, ES_OFFSETOF(ES_Compact_Indexed_Properties, values) + VALUE_INDEX_TO_OFFSET(index) + VALUE_VALUE_OFFSET));

    EmitInitProperty(cg, code, word, op_type, op_value);
}

void
ES_Native::EmitNewArrayReset(VirtualRegister *target_vr, unsigned start_index, unsigned end_index)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::Register indexed_properties(ES_CodeGenerator::REG_SI); // see EmitAllocateObject()

    for (unsigned index = start_index; index < end_index; ++index)
    {
        ES_CodeGenerator::Operand op_type(ES_CodeGenerator::MEMORY(indexed_properties, ES_OFFSETOF(ES_Compact_Indexed_Properties, values) + VALUE_INDEX_TO_OFFSET(index) + VALUE_TYPE_OFFSET));
        ES_CodeGenerator::Operand op_value(ES_CodeGenerator::MEMORY(indexed_properties, ES_OFFSETOF(ES_Compact_Indexed_Properties, values) + VALUE_INDEX_TO_OFFSET(index) + VALUE_VALUE_OFFSET));

        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_UNDEFINED), op_type, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::ZERO(), op_value, ES_CodeGenerator::OPSIZE_32); // "hidden boolean"
    }
}

void
ES_Native::EmitCall(VirtualRegister *this_vr, VirtualRegister *function_vr, unsigned argc, ES_BuiltInFunction builtin)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::JumpTarget *slow_case;

    if (!current_slow_case)
        EmitSlowCaseCall();

    slow_case = current_slow_case->GetJumpTarget();

    BOOL is_call = CurrentCodeWord()->instruction != ESI_CONSTRUCT;

    if (builtin != ES_BUILTIN_FN_NONE && builtin != ES_BUILTIN_FN_DISABLE)
    {
        if (builtin == ES_BUILTIN_FN_OTHER)
        {
        normal_builtin_call:
            ES_CodeGenerator::Register function(ES_CodeGenerator::REG_AX);
            ES_CodeGenerator::Register header(ES_CodeGenerator::REG_DX);

            cg.MOV(REGISTER_VALUE(function_vr), function, ES_CodeGenerator::OPSIZE_POINTER);
            cg.MOV(OBJECT_MEMBER(function, ES_Object, hdr.header), header, ES_CodeGenerator::OPSIZE_32);
            cg.AND(ES_CodeGenerator::IMMEDIATE(ES_Header::MASK_GCTAG), header, ES_CodeGenerator::OPSIZE_32);
            cg.CMP(ES_CodeGenerator::IMMEDIATE(GCTAG_ES_Object_Function), header, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);

            ES_CodeGenerator::Register fncode(ES_CodeGenerator::REG_CX);
            cg.MOV(OBJECT_MEMBER(function, ES_Function, function_code), fncode, ES_CodeGenerator::OPSIZE_POINTER);
            cg.TEST(fncode, fncode, ES_CodeGenerator::OPSIZE_POINTER);
            cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_ZERO, TRUE, FALSE);

            ES_CodeGenerator::Register builtin_call(ES_CodeGenerator::REG_AX);
            cg.MOV(OBJECT_MEMBER(function, ES_Function, data.builtin.call), builtin_call, ES_CodeGenerator::OPSIZE_POINTER);

#ifdef ARCHITECTURE_AMD64_UNIX
            ES_CodeGenerator::Register argv(ES_CodeGenerator::REG_DX);
            ES_CodeGenerator::Register return_value(ES_CodeGenerator::REG_CX);
#elif defined ARCHITECTURE_AMD64_WINDOWS
            ES_CodeGenerator::Register argv(ES_CodeGenerator::REG_R8);
            ES_CodeGenerator::Register return_value(ES_CodeGenerator::REG_R9);
#else
            ES_CodeGenerator::Register argv(ES_CodeGenerator::REG_DI);
            ES_CodeGenerator::Register return_value(ES_CodeGenerator::REG_SI);
#endif

            cg.LEA(ES_CodeGenerator::MEMORY(REGISTER_FRAME_POINTER, sizeof(ES_Value_Internal) * (this_vr->index + 2)), argv, ES_CodeGenerator::OPSIZE_POINTER);
            cg.LEA(ES_CodeGenerator::MEMORY(REGISTER_FRAME_POINTER, sizeof(ES_Value_Internal) * this_vr->index), return_value, ES_CodeGenerator::OPSIZE_POINTER);

            IA32FUNCTIONCALLR("pupp", DEFAULT_USABLE_STACKSPACE);
            call.AllocateStackSpace();
            call.AddArgument(CONTEXT_POINTER);
            call.AddArgument(ES_CodeGenerator::IMMEDIATE(argc));
            call.AddArgument(argv);
            call.AddArgument(return_value);
            call.Call(builtin_call);
            call.FreeStackSpace();

            ES_CodeGenerator::OutOfOrderBlock *exception_thrown = cg.StartOutOfOrderBlock();
            {
                /* This call doesn't return. */

                IA32FUNCTIONCALL("p", ES_Execution_Context::ThrowFromMachineCode);
                call.AddArgument(CONTEXT_POINTER);
                call.Call();
                call.FreeStackSpace();
            }
            cg.EndOutOfOrderBlock(FALSE);

            cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(exception_thrown->GetJumpTarget(), ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);

            return;
        }

        OP_ASSERT(is_call);

        BOOL arg_int32 = FALSE;
        unsigned char profile_data = code->data->profile_data[cw_index + 2];

        if (builtin == ES_BUILTIN_FN_sin || builtin == ES_BUILTIN_FN_cos || builtin == ES_BUILTIN_FN_tan || builtin == ES_BUILTIN_FN_sqrt || builtin == ES_BUILTIN_FN_floor || builtin == ES_BUILTIN_FN_ceil || builtin == ES_BUILTIN_FN_abs || builtin == ES_BUILTIN_FN_log)
        {
            if (argc != 1)
                goto normal_builtin_call;

            if (profile_data)
            {
                if (profile_data == ESTYPE_BITS_INT32)
                {
                    EmitRegisterTypeCheck(function_vr + 1, ESTYPE_INT32, slow_case);
                    arg_int32 = TRUE;
                }
                else if (builtin == ES_BUILTIN_FN_abs)
                    goto normal_call;
                else if ((profile_data & ESTYPE_BITS_DOUBLE) != 0)
                    EmitRegisterTypeCheck(function_vr + 1, ESTYPE_DOUBLE, slow_case);
                else
                    goto normal_builtin_call;
            }
            else
                EmitRegisterTypeCheck(function_vr + 1, ESTYPE_DOUBLE, slow_case);
        }
        else if (builtin == ES_BUILTIN_FN_push && argc != 1)
            goto normal_builtin_call;
        else if (builtin == ES_BUILTIN_FN_pow)
        {
            if (argc != 2)
                goto normal_builtin_call;

            if ((profile_data & ~(ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE)) != 0)
                goto normal_builtin_call;
        }
        else if (builtin == ES_BUILTIN_FN_parseInt)
        {
            if (argc < 1 || argc > 2)
                goto normal_builtin_call;

            if ((profile_data & (ESTYPE_BITS_INT32 | ESTYPE_BITS_STRING)) == 0
#ifdef IA32_SSE2_SUPPORT
                && (!ARCHITECTURE_HAS_FPU() || (profile_data & ESTYPE_BITS_DOUBLE) == 0)
#endif // IA32_SSE2_SUPPORT
               )
                goto normal_builtin_call;
        }

        cg.MOV(REGISTER_VALUE(function_vr), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Object, object_bits), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);
        cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_IS_BUILTIN_FN), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);
        cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);

        cg.SHR(ES_CodeGenerator::IMMEDIATE(ES_Object::FUNCTION_ID_SHIFT), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);
        cg.CMP(ES_CodeGenerator::IMMEDIATE(builtin), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);
        cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);

        if (ARCHITECTURE_HAS_FPU() && (builtin == ES_BUILTIN_FN_sin || builtin == ES_BUILTIN_FN_cos || builtin == ES_BUILTIN_FN_tan || builtin == ES_BUILTIN_FN_sqrt || builtin == ES_BUILTIN_FN_floor || builtin == ES_BUILTIN_FN_ceil || builtin == ES_BUILTIN_FN_log))
        {
            OP_ASSERT(argc == 1);

            ES_CodeGenerator::JumpTarget *is_int32 = NULL, *is_double = NULL;

#ifdef IA32_SSE2_SUPPORT
            BOOL result_in_xmm0 = TRUE;

            if (IS_FPMODE_SSE2 && (builtin == ES_BUILTIN_FN_sqrt || cg.SupportsSSE41() && (builtin == ES_BUILTIN_FN_floor || builtin == ES_BUILTIN_FN_ceil)))
            {
                ES_CodeGenerator::Operand op_source;

                if (arg_int32)
                {
                    cg.CVTSI2SD(REGISTER_VALUE(function_vr + 1), ES_CodeGenerator::REG_XMM0);
                    op_source = ES_CodeGenerator::REG_XMM0;
                }
                else
                    op_source = REGISTER_VALUE(function_vr + 1);

                if (builtin == ES_BUILTIN_FN_sqrt)
                    cg.SQRTSD(op_source, ES_CodeGenerator::REG_XMM0);
                else
                {
                    if (builtin == ES_BUILTIN_FN_floor)
                        cg.ROUNDSD(op_source, ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::ROUND_MODE_floor);
                    else
                        cg.ROUNDSD(op_source, ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::ROUND_MODE_ceil);

                    is_int32 = cg.ForwardJump();
                    is_double = cg.ForwardJump();

                    ES_CodeGenerator::OutOfOrderBlock *check_negative_zero = cg.StartOutOfOrderBlock();
                    cg.CVTSD2SS(ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::REG_XMM1);
                    cg.RCPSS(ES_CodeGenerator::REG_XMM1, ES_CodeGenerator::REG_XMM1);
                    cg.UCOMISS(Zero(cg), ES_CodeGenerator::REG_XMM1);
                    cg.Jump(is_int32, ES_NATIVE_CONDITION_NOT_CARRY); // This does not catch "unordered", that is, NaN.
                    cg.EndOutOfOrderBlock();

                    cg.UCOMISD(IntMin(cg), ES_CodeGenerator::REG_XMM0);
                    cg.Jump(is_double, ES_NATIVE_CONDITION_CARRY); // This also catches "unordered", that is, NaN.

                    cg.UCOMISD(Zero(cg), ES_CodeGenerator::REG_XMM0);
                    cg.Jump(check_negative_zero->GetJumpTarget(), ES_NATIVE_CONDITION_ZERO); // This also catches "unordered", that is, NaN.

                    cg.UCOMISD(IntMax(cg), ES_CodeGenerator::REG_XMM0);
                    cg.Jump(is_int32, ES_NATIVE_CONDITION_CARRY);

                    cg.SetOutOfOrderContinuationPoint(check_negative_zero);
                }

                if (is_double)
                    cg.SetJumpTarget(is_double);

                StoreDoubleInRegister(this, ES_CodeGenerator::REG_XMM0, this_vr);
            }
            else
#endif // IA32_SSE2_SUPPORT
            {
                if (builtin == ES_BUILTIN_FN_log)
                    cg.FLDLN2();

                if (arg_int32)
                    cg.FILD(REGISTER_VALUE(function_vr + 1), ES_CodeGenerator::OPSIZE_32);
                else
                    cg.FLD(REGISTER_VALUE(function_vr + 1), ES_CodeGenerator::OPSIZE_64);

                if (builtin == ES_BUILTIN_FN_sin)
                    cg.FSIN();
                else if (builtin == ES_BUILTIN_FN_cos)
                    cg.FCOS();
                else if (builtin == ES_BUILTIN_FN_tan)
                {
                    cg.FSINCOS();
                    cg.FDIV(0, 1, TRUE, FALSE);
                }
                else if (builtin == ES_BUILTIN_FN_sqrt)
                    cg.FSQRT();
                else if (builtin == ES_BUILTIN_FN_log)
                    cg.FYL2X();
                else if (builtin == ES_BUILTIN_FN_floor || builtin == ES_BUILTIN_FN_ceil)
                {
                    int RC = builtin == ES_BUILTIN_FN_floor ? 1 : 2;

                    cg.FNSTCW(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, -2));
                    cg.MOV(ES_CodeGenerator::IMMEDIATE(0x033f | (RC << 10)), ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, -4), ES_CodeGenerator::OPSIZE_16);
                    cg.FLDCW(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, -4));
                    cg.FRNDINT();
                    cg.FLDCW(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, -2));

                    is_int32 = cg.ForwardJump();
                    is_double = cg.ForwardJump();
                    result_in_xmm0 = FALSE;

                    ES_CodeGenerator::OutOfOrderBlock *check_negative_zero = cg.StartOutOfOrderBlock();
                    cg.FXAM();
                    cg.FNSTSW();
                    cg.TEST(ES_CodeGenerator::IMMEDIATE(0x200), ES_CodeGenerator::REG_AX);
                    cg.Jump(is_int32, ES_NATIVE_CONDITION_ZERO);
                    cg.EndOutOfOrderBlock();

                    cg.FLD(IntMax(cg));
                    cg.FUCOMI(1, TRUE);

                    cg.Jump(is_double, ES_NATIVE_CONDITION_CARRY);

                    cg.FLD(Zero(cg));
                    cg.FUCOMI(1, TRUE);

                    cg.Jump(check_negative_zero->GetJumpTarget(), ES_NATIVE_CONDITION_ZERO);

                    cg.FLD(IntMin(cg));
                    cg.FUCOMI(1, TRUE);

                    cg.Jump(is_int32, ES_NATIVE_CONDITION_CARRY);

                    cg.SetOutOfOrderContinuationPoint(check_negative_zero);
                }

                if (is_double)
                    cg.SetJumpTarget(is_double);

                cg.FST(REGISTER_VALUE(this_vr), TRUE);
            }

#ifdef ES_VALUES_AS_NANS
#else // ES_VALUES_AS_NANS
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), REGISTER_TYPE(this_vr), ES_CodeGenerator::OPSIZE_32);
#endif // ES_VALUES_AS_NANS

            if (is_int32)
            {
                ES_CodeGenerator::JumpTarget *skip = cg.ForwardJump();

                cg.Jump(skip);
                cg.SetJumpTarget(is_int32);

#ifdef IA32_SSE2_SUPPORT
                if (result_in_xmm0)
                {
#ifdef ARCHITECTURE_AMD64
                    cg.CVTTSD2SI(ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_64);
#else // ARCHITECTURE_AMD64
                    cg.CVTTSD2SI(ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64

                    cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(this_vr), ES_CodeGenerator::OPSIZE_32);
                }
                else
#endif // IA32_SSE2_SUPPORT
                    cg.FIST(REGISTER_VALUE(this_vr), TRUE, ES_CodeGenerator::OPSIZE_32);

                cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(this_vr), ES_CodeGenerator::OPSIZE_32);

                cg.SetJumpTarget(skip);
            }
        }
        else if (builtin == ES_BUILTIN_FN_abs)
        {
            ES_CodeGenerator::JumpTarget *positive = cg.ForwardJump();

            cg.MOV(REGISTER_VALUE(this_vr + 2), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(positive, ES_NATIVE_CONDITION_NOT_SIGN);
            cg.NEG(ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(slow_case, ES_NATIVE_CONDITION_SIGN);
            cg.SetJumpTarget(positive);

            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(this_vr), ES_CodeGenerator::OPSIZE_32);
            cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(this_vr), ES_CodeGenerator::OPSIZE_32);
        }
        else if (builtin == ES_BUILTIN_FN_charCodeAt || builtin == ES_BUILTIN_FN_charAt)
        {
            EmitRegisterTypeCheck(this_vr, ESTYPE_STRING, slow_case);
            if (argc > 0)
                EmitRegisterTypeCheck(this_vr + 2, ESTYPE_INT32, slow_case);

            EmitInt32StringIndexedGet(this_vr, this_vr, argc > 0 ? this_vr + 2 : NULL, 0, builtin == ES_BUILTIN_FN_charCodeAt);
        }
        else if (builtin == ES_BUILTIN_FN_fromCharCode && argc == 1)
        {
            VirtualRegister *arg_vr = this_vr + 2;
            ES_Value_Internal arg_value;

            if (IsImmediate(arg_vr, arg_value, TRUE))
            {
                if (arg_value.IsInt32())
                {
                    int arg = arg_value.GetNumAsInt32();
                    if (arg >= 0 && arg < STRING_NUMCHARS)
                    {
                        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_STRING), REGISTER_TYPE(this_vr), ES_CodeGenerator::OPSIZE_32);
                        MovePointerInto(cg, context->rt_data->strings[STRING_nul + arg], REGISTER_VALUE(this_vr), ES_CodeGenerator::REG_AX);
                        return;
                    }
                }
                goto normal_builtin_call;
            }
            else
            {
                ES_CodeGenerator::Register arg_value(ES_CodeGenerator::REG_CX);
                EmitRegisterTypeCheck(arg_vr, ESTYPE_INT32, slow_case);
                cg.MOV(REGISTER_VALUE(arg_vr), arg_value, ES_CodeGenerator::OPSIZE_32);
                cg.CMP(ES_CodeGenerator::IMMEDIATE(STRING_NUMCHARS), arg_value, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(slow_case, ES_NATIVE_CONDITION_ABOVE_EQUAL, TRUE, FALSE);

                ES_CodeGenerator::Register result(ES_CodeGenerator::REG_DX);

                cg.MOV(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, rt_data), result, ES_CodeGenerator::OPSIZE_POINTER);
                cg.MOV(ES_CodeGenerator::MEMORY(result, arg_value, ES_CodeGenerator::SCALE_POINTER, ES_OFFSETOF(ESRT_Data, strings) + STRING_nul * sizeof(void *)), result, ES_CodeGenerator::OPSIZE_POINTER);

                cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_STRING), REGISTER_TYPE(this_vr), ES_CodeGenerator::OPSIZE_32);
                cg.MOV(result, REGISTER_VALUE(this_vr), ES_CodeGenerator::OPSIZE_POINTER);
            }
        }
        else if (builtin == ES_BUILTIN_FN_pow)
        {
            VirtualRegister *number_vr = this_vr + 2, *exp_vr = this_vr + 3;
            ES_Value_Internal exp_value;

            if (IsImmediate(exp_vr, exp_value, TRUE))
                if (exp_value.IsNumber())
                {
                    double exp = exp_value.GetNumAsDouble();

                    if (op_isfinite(exp))
                        if (exp == 2)
                        {
                            if (!IsType(number_vr, ESTYPE_INT32) && !IsType(number_vr, ESTYPE_DOUBLE))
                                EmitRegisterTypeCheck(number_vr, ESTYPE_INT32, slow_case);

                            if (!IsType(number_vr, ESTYPE_DOUBLE))
                            {
                                cg.MOV(REGISTER_VALUE(number_vr), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
                                cg.IMUL(ES_CodeGenerator::REGISTER(ES_CodeGenerator::REG_AX), ES_CodeGenerator::REGISTER(ES_CodeGenerator::REG_AX), ES_CodeGenerator::OPSIZE_32);
                                cg.Jump(slow_case, ES_NATIVE_CONDITION_OVERFLOW);
                                cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(this_vr), ES_CodeGenerator::OPSIZE_32);
                                cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(this_vr), ES_CodeGenerator::OPSIZE_32);
                                return;
                            }
                        }
                        else if (ARCHITECTURE_HAS_FPU() && (exp == 0.5 || exp == -0.5))
                        {
                            if (!IsType(number_vr, ESTYPE_INT32) && !IsType(number_vr, ESTYPE_DOUBLE))
                                EmitRegisterTypeCheck(number_vr, ESTYPE_INT32, slow_case);

                            if (!IsType(number_vr, ESTYPE_DOUBLE))
                            {
                                cg.CVTSI2SD(REGISTER_VALUE(number_vr), ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::OPSIZE_32);
                                cg.SQRTSD(ES_CodeGenerator::REG_XMM0, ES_CodeGenerator::REG_XMM0);
                            }
                            else
                                cg.SQRTSD(REGISTER_VALUE(number_vr), ES_CodeGenerator::REG_XMM0);

                            ES_CodeGenerator::XMM result;

                            if (exp == 0.5)
                                result = ES_CodeGenerator::REG_XMM0;
                            else
                            {
                                result = ES_CodeGenerator::REG_XMM1;

                                cg.MOV(ES_CodeGenerator::IMMEDIATE(1), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
                                cg.CVTSI2SD(ES_CodeGenerator::REG_AX, result, ES_CodeGenerator::OPSIZE_32);
                                cg.DIVSD(ES_CodeGenerator::REG_XMM0, result);
                            }

                            StoreDoubleInRegister(this, result, this_vr);
                            return;
                        }
                }

            IA32FUNCTIONCALL("p", ES_Native::MathPow);

#ifdef ARCHITECTURE_AMD64_UNIX
            ES_CodeGenerator::Register reg(ES_CodeGenerator::REG_DI);
#elif defined(ARCHITECTURE_AMD64_WINDOWS)
            ES_CodeGenerator::Register reg(ES_CodeGenerator::REG_CX);
#else // ARCHITECTURE_AMD64
            ES_CodeGenerator::Register reg(ES_CodeGenerator::REG_CX);
#endif // ARCHITECTURE_AMD64

            cg.LEA(VALUE_INDEX_MEMORY(REGISTER_FRAME_POINTER, this_vr->index), reg, ES_CodeGenerator::OPSIZE_POINTER);

            call.AllocateStackSpace();
            call.AddArgument(reg);
            call.Call();
            call.FreeStackSpace();

            cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);
        }
        else if (builtin == ES_BUILTIN_FN_push)
        {
            ANNOTATE("       EmitCall(): inline Array.prototype.push");

            EmitRegisterTypeCheck(this_vr, ESTYPE_OBJECT, slow_case);

            ES_Value_Internal value;
            EmitInt32IndexedPut(this_vr, NULL, 0, this_vr + 2, FALSE, FALSE, value, TRUE);
        }
        else if (builtin == ES_BUILTIN_FN_parseInt)
        {
            ANNOTATE("       EmitCall(): inline parseInt");

            VirtualRegister *arg_vr = this_vr + 2;
            ES_CodeGenerator::JumpTarget *jt_next = NULL, *jt_finished = NULL;

            if (ARCHITECTURE_HAS_FPU())
                profile_data &= ESTYPE_BITS_INT32 | ESTYPE_BITS_STRING | ESTYPE_BITS_DOUBLE;
            else
                profile_data &= ESTYPE_BITS_INT32 | ESTYPE_BITS_STRING;

            unsigned known_radix = 0;
            if (argc == 2)
            {
                /* Not unlikely that it will be a literal radix argument. */
                ES_Value_Internal radix_value;
                if (IsImmediate(arg_vr + 1, radix_value))
                    if (radix_value.IsInt32())
                    {
                        int radix = radix_value.GetInt32();
                        if (radix == 16 && (profile_data & ESTYPE_BITS_STRING) != 0)
                        {
                            EmitRegisterTypeCheck(arg_vr, ESTYPE_STRING, slow_case);
                            profile_data = ESTYPE_BITS_STRING;
                            known_radix = 16;
                        }
                        else if (radix != 10)
                            goto normal_call;
                        else
                            known_radix = 10;
                    }
                    else
                        goto normal_call;
                else
                {
                    EmitRegisterTypeCheck(arg_vr + 1, ESTYPE_INT32, slow_case);
                    cg.MOV(REGISTER_VALUE(arg_vr + 1), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
                    ES_CodeGenerator::JumpTarget *jt_start = NULL;
                    if ((profile_data & ESTYPE_BITS_STRING) != 0)
                    {
                        ES_CodeGenerator::JumpTarget *jt_is_not_string = cg.ForwardJump();
                        cg.CMP(ES_CodeGenerator::IMMEDIATE(16), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
                        cg.Jump(jt_is_not_string, ES_NATIVE_CONDITION_NOT_EQUAL);
                        EmitRegisterTypeCheck(arg_vr, ESTYPE_STRING, slow_case);
                        cg.Jump(jt_start = cg.ForwardJump());
                        cg.SetJumpTarget(jt_is_not_string);
                    }
                    cg.CMP(ES_CodeGenerator::IMMEDIATE(10), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_EQUAL);
                    if (jt_start)
                        cg.SetJumpTarget(jt_start);
                }
            }

            if ((profile_data & ESTYPE_BITS_INT32) != 0)
            {
                profile_data ^= ESTYPE_BITS_INT32;
                jt_next = profile_data == 0 ? slow_case : cg.ForwardJump();

                EmitRegisterTypeCheck(arg_vr, ESTYPE_INT32, jt_next);

                cg.MOV(REGISTER_VALUE(arg_vr), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);

                if (profile_data != 0)
                    cg.Jump(jt_finished = cg.ForwardJump());
            }
            if ((profile_data & ESTYPE_BITS_DOUBLE) != 0)
            {
                if (jt_next)
                    cg.SetJumpTarget(jt_next);

                profile_data ^= ESTYPE_BITS_DOUBLE;
                jt_next = profile_data == 0 ? slow_case : cg.ForwardJump();

                EmitRegisterTypeCheck(arg_vr, ESTYPE_DOUBLE, jt_next);
                ConvertDoubleToInt(this, arg_vr, ES_CodeGenerator::REG_AX, TRUE, TRUE, slow_case);
                cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
                cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO);

                if (profile_data != 0)
                    cg.Jump(jt_finished ? jt_finished : (jt_finished = cg.ForwardJump()));
            }
            if ((profile_data & ESTYPE_BITS_STRING) != 0)
            {
                if (jt_next)
                    cg.SetJumpTarget(jt_next);

                if (!jt_finished)
                    jt_finished = cg.ForwardJump();

                ES_CodeGenerator::Register number(ES_CodeGenerator::REG_AX);
                ES_CodeGenerator::Register string(ES_CodeGenerator::REG_DX);
                ES_CodeGenerator::Register value(ES_CodeGenerator::REG_DI);
                ES_CodeGenerator::Register length(ES_CodeGenerator::REG_CX);
                ES_CodeGenerator::Register character(ES_CodeGenerator::REG_SI);

                EmitRegisterTypeCheck(arg_vr, ESTYPE_STRING, slow_case);

                cg.MOV(REGISTER_VALUE(arg_vr), string, ES_CodeGenerator::OPSIZE_POINTER);
                cg.MOV(OBJECT_MEMBER(string, JString, value), value, ES_CodeGenerator::OPSIZE_POINTER);

                /* Make sure the string isn't segmented: */
                cg.TEST(ES_CodeGenerator::IMMEDIATE(1), value, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_ZERO, TRUE, FALSE);

                ES_CodeGenerator::Register offset(length);
                cg.MOV(OBJECT_MEMBER(string, JString, offset), offset, ES_CodeGenerator::OPSIZE_32);
                cg.AND(ES_CodeGenerator::IMMEDIATE(0xffffff), offset, ES_CodeGenerator::OPSIZE_32);
                cg.LEA(ES_CodeGenerator::MEMORY(value, offset, ES_CodeGenerator::SCALE_2, ES_OFFSETOF(JStringStorage, storage)), value, ES_CodeGenerator::OPSIZE_POINTER);

                /* Empty string => NaN, which we don't handle here. */
                cg.MOV(OBJECT_MEMBER(string, JString, length), length, ES_CodeGenerator::OPSIZE_32);
                cg.CMP(ES_CodeGenerator::ZERO(), length, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(slow_case, ES_NATIVE_CONDITION_EQUAL);

                /* Record start of string value. */
                ES_CodeGenerator::Register start_value(string);
                cg.MOV(value, start_value, ES_CodeGenerator::OPSIZE_32);

                /* Block for scanning string as a hexadecimal. */
                ES_CodeGenerator::JumpTarget *jt_is_hex_digit = NULL;
                ES_CodeGenerator::OutOfOrderBlock *as_hex = NULL;
                if (known_radix != 10)
                {
                    as_hex = cg.StartOutOfOrderBlock();
                    jt_is_hex_digit = cg.ForwardJump();
                    ES_CodeGenerator::JumpTarget *jt_hex_finished = cg.ForwardJump();

                    /* Adjust 'start_value' to point at first character after 0x prefix. */
                    cg.ADD(ES_CodeGenerator::IMMEDIATE(2), value, ES_CodeGenerator::OPSIZE_POINTER);
                    cg.SUB(ES_CodeGenerator::IMMEDIATE(1), length, ES_CodeGenerator::OPSIZE_32);
                    cg.MOV(value, start_value, ES_CodeGenerator::OPSIZE_32);
                    cg.ADD(ES_CodeGenerator::IMMEDIATE(2), start_value, ES_CodeGenerator::OPSIZE_32);

                    ES_CodeGenerator::JumpTarget *jt_hex_continue = cg.Here();
                    cg.ADD(ES_CodeGenerator::IMMEDIATE(2), value, ES_CodeGenerator::OPSIZE_POINTER);
                    cg.SUB(ES_CodeGenerator::IMMEDIATE(1), length, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(jt_hex_finished, ES_NATIVE_CONDITION_ZERO);
                    ES_CodeGenerator::JumpTarget *jt_not_decimal = cg.ForwardJump();

                    cg.SetJumpTarget(jt_is_hex_digit);
                    cg.MOVZX(ES_CodeGenerator::MEMORY(value), character, ES_CodeGenerator::OPSIZE_16);
                    cg.SUB(ES_CodeGenerator::IMMEDIATE('0'), character, ES_CodeGenerator::OPSIZE_32);
                    cg.CMP(ES_CodeGenerator::IMMEDIATE(10), character, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(jt_not_decimal, ES_NATIVE_CONDITION_NOT_BELOW);

                    cg.IMUL(number, 16, number, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(slow_case, ES_NATIVE_CONDITION_OVERFLOW);

                    cg.ADD(character, number, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(jt_hex_continue, ES_NATIVE_CONDITION_NOT_OVERFLOW);

                    cg.Jump(slow_case);
                    cg.SetJumpTarget(jt_hex_finished);
                    cg.CMP(value, start_value, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(jt_finished, ES_NATIVE_CONDITION_NOT_EQUAL);
                    cg.Jump(slow_case);

                    cg.SetJumpTarget(jt_not_decimal);
                    /* hex digit is an alpha (possibly), use 0xffdf (= ~('A' ^ 'a')) to translate (character - '0') 0x20 values down. */
                    cg.AND(ES_CodeGenerator::IMMEDIATE(0xffdf), character, ES_CodeGenerator::OPSIZE_32);
                    /* 17 = 'A' - '0';  translate further to make it zero based. */
                    cg.SUB(ES_CodeGenerator::IMMEDIATE(17), character, ES_CodeGenerator::OPSIZE_32);
                    cg.CMP(ES_CodeGenerator::IMMEDIATE(5), character, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(jt_hex_finished, ES_NATIVE_CONDITION_ABOVE);

                    cg.IMUL(number, 16, number, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(slow_case, ES_NATIVE_CONDITION_OVERFLOW);

                    cg.LEA(ES_CodeGenerator::MEMORY(number, character, ES_CodeGenerator::SCALE_1, 0xa), number, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(jt_hex_continue);
                    cg.EndOutOfOrderBlock(FALSE);
                }

                cg.MOV(ES_CodeGenerator::ZERO(), number, ES_CodeGenerator::OPSIZE_32);
                if (argc == 2 && known_radix != 10)
                {
                    ES_CodeGenerator::JumpTarget *is_base10 = NULL;

                    if (known_radix != 16)
                    {
                        is_base10 = cg.ForwardJump();
                        /* Radix is either 10 or 16. */
                        cg.MOV(REGISTER_VALUE(arg_vr + 1), character, ES_CodeGenerator::OPSIZE_32);
                        cg.CMP(ES_CodeGenerator::IMMEDIATE(10), character, ES_CodeGenerator::OPSIZE_32);
                        cg.Jump(is_base10, ES_NATIVE_CONDITION_EQUAL);
                    }

                    /* Match optional 0{x,X} prefix. */
                    cg.MOVZX(ES_CodeGenerator::MEMORY(value), character, ES_CodeGenerator::OPSIZE_16);
                    cg.CMP(ES_CodeGenerator::IMMEDIATE('0'), character, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(jt_is_hex_digit, ES_NATIVE_CONDITION_NOT_EQUAL);

                    cg.CMP(ES_CodeGenerator::IMMEDIATE(1), length, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(slow_case, ES_NATIVE_CONDITION_BELOW_EQUAL);

                    cg.MOVZX(ES_CodeGenerator::MEMORY(value, 2), character, ES_CodeGenerator::OPSIZE_16);
                    /* Uppercase alpha character. */
                    cg.AND(ES_CodeGenerator::IMMEDIATE(0xffdf), character, ES_CodeGenerator::OPSIZE_32);
                    cg.CMP(ES_CodeGenerator::IMMEDIATE('X'), character, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(jt_is_hex_digit, ES_NATIVE_CONDITION_NOT_EQUAL);
                    cg.Jump(as_hex->GetJumpTarget());

                    if (is_base10)
                        cg.SetJumpTarget(is_base10);
                }
                else if (known_radix != 10)
                {
                    /* Check if a hex prefix. */
                    ES_CodeGenerator::JumpTarget *jt_first_digit = cg.ForwardJump();
                    cg.CMP(ES_CodeGenerator::IMMEDIATE(2), length, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(jt_first_digit, ES_NATIVE_CONDITION_BELOW);
                    cg.MOV(ES_CodeGenerator::MEMORY(value), character, ES_CodeGenerator::OPSIZE_32);
                    /* Uppercase the alpha character ('x' => 'X') */
                    cg.AND(ES_CodeGenerator::IMMEDIATE(0xffdfffff), character, ES_CodeGenerator::OPSIZE_32);
                    /* 0x0058 = 'X', 0x0030 = '0' */
                    cg.CMP(ES_CodeGenerator::IMMEDIATE(0x00580030), character, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(as_hex->GetJumpTarget(), ES_NATIVE_CONDITION_EQUAL);

                    cg.SetJumpTarget(jt_first_digit);
                }

                if (known_radix != 16)
                {
                    /* Load first character into 'number', jump to slow case if not
                       decimal digit, or if zero (to avoid the hex+octal case.) */
                    cg.MOVZX(ES_CodeGenerator::MEMORY(value), number, ES_CodeGenerator::OPSIZE_16);
                    cg.SUB(ES_CodeGenerator::IMMEDIATE('0'), number, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO);
                    cg.CMP(ES_CodeGenerator::IMMEDIATE(10), number, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_BELOW);

                    ES_CodeGenerator::JumpTarget *jt_continue = cg.Here();

                    cg.ADD(ES_CodeGenerator::IMMEDIATE(2), value, ES_CodeGenerator::OPSIZE_POINTER);
                    cg.SUB(ES_CodeGenerator::IMMEDIATE(1), length, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(jt_finished, ES_NATIVE_CONDITION_ZERO);

                    cg.MOVZX(ES_CodeGenerator::MEMORY(value), character, ES_CodeGenerator::OPSIZE_16);
                    cg.SUB(ES_CodeGenerator::IMMEDIATE('0'), character, ES_CodeGenerator::OPSIZE_32);
                    cg.CMP(ES_CodeGenerator::IMMEDIATE(10), character, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(jt_finished, ES_NATIVE_CONDITION_NOT_BELOW);

                    cg.IMUL(number, 10, number, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(slow_case, ES_NATIVE_CONDITION_OVERFLOW);

                    cg.ADD(character, number, ES_CodeGenerator::OPSIZE_32);
                    cg.Jump(jt_continue, ES_NATIVE_CONDITION_NOT_OVERFLOW);

                    cg.Jump(slow_case);
                }
            }

            if (jt_finished)
                cg.SetJumpTarget(jt_finished);

            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(this_vr), ES_CodeGenerator::OPSIZE_32);
            cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(this_vr), ES_CodeGenerator::OPSIZE_32);
        }
        else
            goto normal_call;
    }
    else
    {
    normal_call:
        /* Check that the called function is a native function. */
        cg.MOV(REGISTER_VALUE(function_vr), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);

        cg.TEST(ES_CodeGenerator::IMMEDIATE(is_call ? ES_Object::MASK_IS_NATIVE_FN : ES_Object::MASK_IS_DISPATCHED_CTOR), OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO);

        /* Fetch the native dispatcher. */
        if (is_call)
        {
            cg.MOV(OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Function, function_code), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
            cg.MOV(OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Code, native_dispatcher), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
            cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
            cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO);
        }
        else
            cg.MOV(OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Function, data.native.native_ctor_dispatcher), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);

        cg.MOV(ES_CodeGenerator::IMMEDIATE(argc), PARAMETERS_COUNT, ES_CodeGenerator::OPSIZE_32);
        cg.ADD(ES_CodeGenerator::IMMEDIATE(VALUE_INDEX_TO_OFFSET(this_vr->index)), REGISTER_FRAME_POINTER, ES_CodeGenerator::OPSIZE_POINTER);

        cg.CALL(ES_CodeGenerator::REG_AX);

        SetFunctionCallReturnTarget(cg.Here());

        cg.SUB(ES_CodeGenerator::IMMEDIATE(VALUE_INDEX_TO_OFFSET(this_vr->index)), REGISTER_FRAME_POINTER, ES_CodeGenerator::OPSIZE_POINTER);

#ifdef ARCHITECTURE_AMD64
        //MovePointerIntoRegister(cg, code, CODE_POINTER);
        cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, StackSpaceAllocated() - 3 * sizeof(void *)), CODE_POINTER, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64
    }
}

void
ES_Native::EmitRedirectedCall(VirtualRegister *function_vr, VirtualRegister *apply_vr, BOOL check_constructor_object_result)
{
    DECLARE_NOTHING();

    if (!current_slow_case)
        EmitSlowCaseCall();

    ES_CodeGenerator::JumpTarget *slow_case = current_slow_case->GetJumpTarget();

    /* Check that apply_vr contains the built-in apply function. */
    cg.MOV(REGISTER_VALUE(apply_vr), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_IS_APPLY_FN), OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);

    /* Check that 'argc' is not more than ES_REDIRECTED_CALL_MAXIMUM_ARGUMENTS. */
    int stack_offset = code->CanHaveVariableObject() ? 6 : 5;
    cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, StackSpaceAllocated() - stack_offset * sizeof(void *)), PARAMETERS_COUNT, ES_CodeGenerator::OPSIZE_POINTER);
    cg.CMP(ES_CodeGenerator::IMMEDIATE(ES_REDIRECTED_CALL_MAXIMUM_ARGUMENTS), PARAMETERS_COUNT, ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(slow_case, ES_NATIVE_CONDITION_ABOVE, TRUE, FALSE);

    cg.MOV(REGISTER_VALUE(function_vr), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);

    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_IS_NATIVE_FN), OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO);

    cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(VR(1)), ES_CodeGenerator::OPSIZE_POINTER);

    cg.MOV(OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Function, function_code), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO);

    cg.MOV(OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Code, native_dispatcher), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO);

    if (constructor)
    {
        cg.SUB(ES_CodeGenerator::IMMEDIATE(static_cast<INTPTR>(ES_STACK_ALIGNMENT - sizeof(void *))), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
        cg.PUSH(REGISTER_VALUE(VR(0)));
    }

    cg.CALL(ES_CodeGenerator::REG_AX);

    SetFunctionCallReturnTarget(cg.Here());

    if (constructor)
    {
        ES_CodeGenerator::JumpTarget *keep_object_result = NULL;
        if (check_constructor_object_result)
        {
            /* Restore reg[0] if return value isn't an object. */

            cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(VR(0)), ES_CodeGenerator::OPSIZE_32);

            keep_object_result = cg.ForwardJump();
            cg.Jump(keep_object_result, ES_NATIVE_CONDITION_EQUAL);
        }

        cg.POP(REGISTER_VALUE(VR(0)));
        cg.ADD(ES_CodeGenerator::IMMEDIATE(static_cast<INTPTR>(ES_STACK_ALIGNMENT - sizeof(void *))), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(VR(0)), ES_CodeGenerator::OPSIZE_32);

        ES_CodeGenerator::JumpTarget *done = NULL;
        if (keep_object_result)
        {
            done = cg.ForwardJump();
            cg.Jump(done);

            cg.SetJumpTarget(keep_object_result);
            cg.ADD(ES_CodeGenerator::IMMEDIATE(static_cast<INTPTR>(ES_STACK_ALIGNMENT)), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
        }

        if (done)
            cg.SetJumpTarget(done);
    }

#ifdef ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, StackSpaceAllocated() - 3 * sizeof(void *)), CODE_POINTER, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64
}

void
ES_Native::EmitApply(VirtualRegister *apply_vr, VirtualRegister *this_vr, VirtualRegister *function_vr, unsigned argc)
{
    DECLARE_NOTHING();

    if (!current_slow_case)
        EmitSlowCaseCall();

    ES_CodeGenerator::JumpTarget *slow_case = current_slow_case->GetJumpTarget();

    /* Check that apply_vr contains the built-in apply function. */
    cg.MOV(REGISTER_VALUE(apply_vr), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_IS_APPLY_FN), OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO);

    /* Check that function_vr contains a machine coded function. */
    cg.MOV(REGISTER_VALUE(function_vr), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_IS_NATIVE_FN), OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO);
    cg.MOV(OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Function, function_code), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_FunctionCode, native_dispatcher), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO);

    cg.MOV(ES_CodeGenerator::IMMEDIATE(argc), PARAMETERS_COUNT, ES_CodeGenerator::OPSIZE_32);
    cg.ADD(ES_CodeGenerator::IMMEDIATE(VALUE_INDEX_TO_OFFSET(this_vr->index)), REGISTER_FRAME_POINTER, ES_CodeGenerator::OPSIZE_POINTER);

    cg.CALL(ES_CodeGenerator::REG_AX);

    SetFunctionCallReturnTarget(cg.Here());

    cg.SUB(ES_CodeGenerator::IMMEDIATE(VALUE_INDEX_TO_OFFSET(this_vr->index)), REGISTER_FRAME_POINTER, ES_CodeGenerator::OPSIZE_POINTER);

#ifdef ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, StackSpaceAllocated() - 3 * sizeof(void *)), CODE_POINTER, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64
}

void
ES_Native::EmitEval(VirtualRegister *this_vr, VirtualRegister *function_vr, unsigned argc)
{
    DECLARE_NOTHING();

    if (!current_slow_case)
        EmitSlowCaseCall();

    ES_CodeGenerator::JumpTarget *slow_case = current_slow_case->GetJumpTarget();

    cg.MOV(REGISTER_VALUE(function_vr), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_IS_EVAL_FN), OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);

    IA32FUNCTIONCALL("pp", ES_Execution_Context::EvalFromMachineCode);

    call.AllocateStackSpace();
    call.AddArgument(CONTEXT_POINTER);
    call.AddArgument(ES_CodeGenerator::IMMEDIATE(cw_index));
    call.Call();
    call.FreeStackSpace();
}

ES_CodeGenerator::OutOfOrderBlock *
ES_Native::EmitInlinedCallPrologue(VirtualRegister *this_vr, VirtualRegister *function_vr, ES_Function *function, unsigned char *mark_failure, unsigned argc, BOOL function_fetched)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::OutOfOrderBlock *failure = cg.StartOutOfOrderBlock();

    cg.SUB(ES_CodeGenerator::IMMEDIATE(VALUE_INDEX_TO_OFFSET(this_vr->index)), REGISTER_FRAME_POINTER, ES_CodeGenerator::OPSIZE_POINTER);

    if (!function_fetched)
    {
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(function_vr), ES_CodeGenerator::OPSIZE_32);
#ifdef ARCHITECTURE_AMD64
        MovePointerIntoRegister(cg, function, ES_CodeGenerator::REG_AX);
        cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(function_vr), ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
        cg.MOV(ES_CodeGenerator::IMMEDIATE(function), REGISTER_VALUE(function_vr), ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64
    }

#ifdef ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::IMMEDIATE(1), OBJECT_MEMBER(CODE_POINTER, ES_Code, has_failed_inlined_function), ES_CodeGenerator::OPSIZE_32);
#else // ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::IMMEDIATE(1), ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(code) + ES_OFFSETOF(ES_Code, has_failed_inlined_function)), ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64

    /* Update profile data to indicate that the instruction failed inlining. */
#ifdef ARCHITECTURE_AMD64
    MovePointerIntoRegister(cg, mark_failure, ES_CodeGenerator::REG_AX);
    cg.OR(ES_CodeGenerator::IMMEDIATE(ES_CodeStatic::GET_FAILED_INLINE), ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_AX), ES_CodeGenerator::OPSIZE_8);
#else
    cg.OR(ES_CodeGenerator::IMMEDIATE(ES_CodeStatic::GET_FAILED_INLINE), ES_CodeGenerator::MEMORY((void*)mark_failure), ES_CodeGenerator::OPSIZE_8);
#endif // ARCHITECTURE_AMD64

    IA32FUNCTIONCALL("pu", ES_Execution_Context::ForceUpdateNativeDispatcher);

    call.AllocateStackSpace();
    call.AddArgument(CONTEXT_POINTER);
    call.AddArgument(ES_CodeGenerator::IMMEDIATE(cw_index + (constructor ? code->data->codewords_count : 0)));
    call.Call();
    call.FreeStackSpace();

    cg.JMP(ES_CodeGenerator::REG_AX);
    cg.EndOutOfOrderBlock(FALSE);

    cg.ADD(ES_CodeGenerator::IMMEDIATE(VALUE_INDEX_TO_OFFSET(this_vr->index)), REGISTER_FRAME_POINTER, ES_CodeGenerator::OPSIZE_POINTER);

    return failure;
}

void
ES_Native::EmitInlinedCallEpilogue(VirtualRegister *this_vr, VirtualRegister *function_vr, unsigned argc)
{
    cg.SUB(ES_CodeGenerator::IMMEDIATE(VALUE_INDEX_TO_OFFSET(this_vr->index)), REGISTER_FRAME_POINTER, ES_CodeGenerator::OPSIZE_POINTER);

#ifdef ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, StackSpaceAllocated() - 3 * sizeof(void *)), CODE_POINTER, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64
}

void
ES_Native::EmitReturn()
{
    if (cw_index != code->data->instruction_offsets[code->data->instruction_count - 1])
        cg.Jump(epilogue_jump_target);
}

void
ES_Native::EmitNormalizeValue(VirtualRegister *vr, ES_CodeGenerator::JumpTarget *slow_case)
{
    ES_CodeGenerator::JumpTarget *is_int = cg.ForwardJump();

    EmitRegisterTypeCheck(vr, ESTYPE_INT32, is_int, TRUE);

    EmitRegisterTypeCheck(vr, ESTYPE_DOUBLE, slow_case);

    if (!slow_case)
    {
        OP_ASSERT(current_slow_case != NULL);
        slow_case = current_slow_case->GetJumpTarget();
    }

    ES_CodeGenerator::XMM xmm0 = ES_CodeGenerator::REG_XMM0;
    ES_CodeGenerator::XMM xmm1 = ES_CodeGenerator::REG_XMM1;
    ES_CodeGenerator::Register eax = ES_CodeGenerator::REG_AX;

    cg.MOVSD(REGISTER_VALUE(vr), xmm0);
    cg.CVTTSD2SI(xmm0, eax);
    cg.CVTSI2SD(eax, xmm1);
    cg.UCOMISD(xmm0, xmm1);
    cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_EQUAL);
    cg.Jump(slow_case, ES_NATIVE_CONDITION_PARITY);

    cg.MOV(ES_CodeGenerator::REG_AX, REGISTER_VALUE(vr), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(vr), ES_CodeGenerator::OPSIZE_32);

    cg.SetJumpTarget(is_int);
}

void
ES_Native::EmitInt32IndexedGet(VirtualRegister *target_vr, VirtualRegister *object_vr, VirtualRegister *index_vr, unsigned constant_index)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register properties(ES_CodeGenerator::REG_CX);
    ES_CodeGenerator::Register index(ES_CodeGenerator::REG_DX);

    if (!current_slow_case)
        EmitSlowCaseCall();

    LoadObjectOperand(object_vr->index, object);

    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_SIMPLE_COMPACT_INDEXED), OBJECT_MEMBER(object, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(OBJECT_MEMBER(object, ES_Object, indexed_properties), properties, ES_CodeGenerator::OPSIZE_POINTER);

    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);

    if (!index_vr)
    {
        if (constant_index != 0)
        {
            cg.CMP(ES_CodeGenerator::IMMEDIATE(constant_index), OBJECT_MEMBER(properties, ES_Compact_Indexed_Properties, capacity), ES_CodeGenerator::OPSIZE_32);
            cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW_EQUAL, TRUE, FALSE);
        }

        cg.LEA(ES_CodeGenerator::MEMORY(properties, ES_OFFSETOF(ES_Compact_Indexed_Properties, values[0]) + VALUE_INDEX_TO_OFFSET(constant_index)), index, ES_CodeGenerator::OPSIZE_POINTER);
    }
    else
    {
        cg.MOV(REGISTER_VALUE(index_vr), index, ES_CodeGenerator::OPSIZE_32);

        /* Check that index is within array's capacity: */
        cg.CMP(index, OBJECT_MEMBER(properties, ES_Compact_Indexed_Properties, capacity), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW_EQUAL, TRUE, FALSE);

#ifdef ARCHITECTURE_AMD64
        cg.SHL(ES_CodeGenerator::IMMEDIATE(1), index, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

        cg.LEA(ES_CodeGenerator::MEMORY(properties, index, ES_CodeGenerator::SCALE_8, ES_OFFSETOF(ES_Compact_Indexed_Properties, values[0])), index, ES_CodeGenerator::OPSIZE_POINTER);
    }

    cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_UNDEFINED), VALUE_TYPE(index), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_EQUAL, TRUE, FALSE);

    if (property_value_write_vr)
    {
        OP_ASSERT(property_value_write_vr == target_vr);

        SetPropertyValueTransferRegister(NR(IA32_REGISTER_INDEX_SI), 0, TRUE);

        cg.MOV(index, ES_CodeGenerator::REG_SI, ES_CodeGenerator::OPSIZE_POINTER);

        if (current_slow_case)
        {
            ES_CodeGenerator::OutOfOrderBlock *recover = cg.StartOutOfOrderBlock();

            cg.SetOutOfOrderContinuationPoint(current_slow_case);
            current_slow_case = NULL;

            cg.LEA(VALUE_INDEX_MEMORY(REGISTER_FRAME_POINTER, target_vr->index), ES_CodeGenerator::REG_SI, ES_CodeGenerator::OPSIZE_POINTER);

            cg.EndOutOfOrderBlock();
            cg.SetOutOfOrderContinuationPoint(recover);
        }
    }
    else
        CopyValue(cg, index, VALUE_INDEX_TO_OFFSET(0), target_vr, object, properties);
}

void
ES_Native::EmitInt32IndexedPut(VirtualRegister *object_vr, VirtualRegister *index_vr, unsigned constant_index, VirtualRegister *source_vr, BOOL known_type, BOOL known_value, const ES_Value_Internal &value, BOOL is_push)
{
    /* EmitInt32IndexedPut doubles as the generator of inlined calls
       to Array.prototype.push if called with 'is_push' set to TRUE. */

    DECLARE_NOTHING();

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register indexed_properties(ES_CodeGenerator::REG_CX);
    ES_CodeGenerator::Register index(ES_CodeGenerator::REG_DX);
    ES_CodeGenerator::Register index_plus_one(ES_CodeGenerator::REG_SI);
    ES_CodeGenerator::Register properties(ES_CodeGenerator::REG_DI);
    ES_CodeGenerator::Register klass(properties);

    unsigned index_offset = code->global_object->GetArrayClass()->GetLayoutInfoAtIndex(ES_LayoutIndex(0)).GetOffset();

    LoadObjectOperand(object_vr->index, object);

    if (!current_slow_case)
        EmitSlowCaseCall();

    ES_CodeGenerator::JumpTarget *slow_case;

    if (!is_light_weight && property_value_read_vr && property_value_nr)
    {
        ES_CodeGenerator::OutOfOrderBlock *flush_object_vr = cg.StartOutOfOrderBlock();

        cg.MOV(object, REGISTER_VALUE(object_vr), ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(object_vr), ES_CodeGenerator::OPSIZE_32);

        cg.Jump(current_slow_case_main);
        cg.EndOutOfOrderBlock(FALSE);

        slow_case = flush_object_vr->GetJumpTarget();
    }
    else
        slow_case = current_slow_case->GetJumpTarget();

    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_MUTABLE_COMPACT_INDEXED), OBJECT_MEMBER(object, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(OBJECT_MEMBER(object, ES_Object, indexed_properties), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);

    cg.Jump(slow_case, ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);

    ES_Value_Internal index_value;

    cg.MOV(OBJECT_MEMBER(object, ES_Object, klass), klass, ES_CodeGenerator::OPSIZE_POINTER);

    if (is_push)
    {
        /* Theory: When inlining Array.prototype.push, do the class check
           early since we'll need to get the 'length' property from the
           array object. If the class id check holds, it is not just
           guaranteed that the object is an array, but also that the type
           of length is UINT32. */
        cg.CMP(ES_CodeGenerator::IMMEDIATE(code->global_object->GetArrayClass()->GetId(context)), OBJECT_MEMBER(klass, ES_Class, class_id), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);
        cg.MOV(OBJECT_MEMBER(object, ES_Object, properties), properties, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(VALUE_WITH_OFFSET(properties, index_offset), index, ES_CodeGenerator::OPSIZE_32);
    }
    else if (index_vr)
        if (IsImmediate(index_vr, index_value) && index_value.IsInt32() && index_value.GetInt32() >= 0)
        {
            constant_index = index_value.GetInt32();
            index_vr = NULL;
        }
        else
            cg.MOV(REGISTER_VALUE(index_vr), index, ES_CodeGenerator::OPSIZE_32);

    ES_CodeGenerator::OutOfOrderBlock *check_capacity_and_length = !is_push ? cg.StartOutOfOrderBlock() : NULL;
    ES_CodeGenerator::JumpTarget *no_length_update = !is_push ? check_capacity_and_length->GetContinuationTarget() : NULL;

    ANNOTATE("       EmitInt32IndexedPut(): check capacity and length");

    if (!index_vr && !is_push)
        cg.CMP(ES_CodeGenerator::IMMEDIATE(constant_index), OBJECT_MEMBER(indexed_properties, ES_Compact_Indexed_Properties, capacity), ES_CodeGenerator::OPSIZE_32);
    else
        cg.CMP(index, OBJECT_MEMBER(indexed_properties, ES_Compact_Indexed_Properties, capacity), ES_CodeGenerator::OPSIZE_32);

    cg.Jump(slow_case, ES_NATIVE_CONDITION_BELOW_EQUAL, TRUE, FALSE);

    /* Theory: length is an UINT32.  If the value stored is not an INT32, it
       must be a double containing an integer x, INT_MAX<x<=UINT_MAX.  Since our
       index is an INT32, we don't need to handle the double case.  The biggest
       index we can get is INT_MAX, and if the length is a double it is at least
       INT_MAX+1, thus the index is smaller than the length, and we don't need
       to update the length. Verify if the length is updatable and a UINT32 by first
       comparing the array object against GCTAG_ES_Object_Array, followed
       by a class ID check with respect to the array class on the global object.
       If both hold, adjust the UINT32 length in native code. */

    if (!is_push)
    {
        ES_CodeGenerator::Register bits(ES_CodeGenerator::REG_SI);
        cg.MOV(ES_CodeGenerator::MEMORY(object), bits, ES_CodeGenerator::OPSIZE_32);
        cg.AND(ES_CodeGenerator::IMMEDIATE(ES_Header::MASK_GCTAG), bits, ES_CodeGenerator::OPSIZE_32);
        cg.CMP(ES_CodeGenerator::IMMEDIATE(GCTAG_ES_Object_Array), bits, ES_CodeGenerator::OPSIZE_32);
        cg.Jump(no_length_update, ES_NATIVE_CONDITION_NOT_EQUAL, FALSE, TRUE);
        cg.CMP(ES_CodeGenerator::IMMEDIATE(code->global_object->GetArrayClass()->GetId(context)), OBJECT_MEMBER(klass, ES_Class, class_id), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_EQUAL, FALSE, TRUE);
        cg.MOV(OBJECT_MEMBER(object, ES_Object, properties), properties, ES_CodeGenerator::OPSIZE_POINTER);
    }
    if (index_vr || is_push)
    {
        if (!is_push)
        {
            cg.CMP(index, VALUE_WITH_OFFSET(properties, index_offset), ES_CodeGenerator::OPSIZE_32);
            cg.Jump(no_length_update, ES_NATIVE_CONDITION_ABOVE);
        }

        /* The commented out code here is meant to handle the special case of
           updating length when index INT_MAX is set.  The length will then be
           INT_MAX+1, and the addition we do below would overflow.  However, for
           this to happen we need a compact array of capacity INT_MAX or more.
           This object would be at least 16 gigabytes on a 32-bit system and 32
           gigabytes on a 64-bit system.  Obviously, a 16 gigabyte object on a
           32-bit system is impossible, so this only affects 64-bit systems.
           And emitting extra code for every indexed put to handle a special
           case that only occurs if you have first allocated (and to some degree
           initialized) a 32 gigabytes large object seems pointless at this
           time.

        ES_CodeGenerator::OutOfOrderBlock *overflowed_length = cg.StartOutOfOrderBlock();

#ifndef ES_VALUES_AS_NANS
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_DI, VALUE_TYPE_OFFSET));
#endif // ES_VALUES_AS_NANS

        cg.MOV(ES_CodeGenerator::IMMEDIATE(0x41E00000), ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_DI, VALUE_VALUE_OFFSET));
        cg.MOV(ES_CodeGenerator::ZERO(), ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_DI, VALUE_VALUE2_OFFSET));

        cg.EndOutOfOrderBlock();
        */

        cg.LEA(ES_CodeGenerator::MEMORY(index, 1), index_plus_one, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(index_plus_one, VALUE_WITH_OFFSET(properties, index_offset), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(index_plus_one, OBJECT_MEMBER(indexed_properties, ES_Compact_Indexed_Properties, top), ES_CodeGenerator::OPSIZE_32);

        /*
        cg.Jump(overflowed_length->GetJumpTarget(), ES_NATIVE_CONDITION_OVERFLOW, TRUE, FALSE);
        cg.SetOutOfOrderContinuationPoint(overflowed_length);
        */
    }
    else
    {
        cg.CMP(ES_CodeGenerator::IMMEDIATE(constant_index), VALUE_WITH_OFFSET(properties, index_offset), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(no_length_update, ES_NATIVE_CONDITION_ABOVE);

        cg.MOV(ES_CodeGenerator::IMMEDIATE(constant_index + 1), VALUE_WITH_OFFSET(properties, index_offset), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(constant_index + 1), OBJECT_MEMBER(indexed_properties, ES_Compact_Indexed_Properties, top), ES_CodeGenerator::OPSIZE_32);
    }

    if (check_capacity_and_length)
    {
        cg.EndOutOfOrderBlock();

        /* Check that index is within array's capacity. */
        if (!index_vr)
            cg.CMP(ES_CodeGenerator::IMMEDIATE(constant_index), OBJECT_MEMBER(indexed_properties, ES_Compact_Indexed_Properties, top), ES_CodeGenerator::OPSIZE_32);
        else
            cg.CMP(index, OBJECT_MEMBER(indexed_properties, ES_Compact_Indexed_Properties, top), ES_CodeGenerator::OPSIZE_32);

        cg.Jump(check_capacity_and_length->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW_EQUAL, TRUE, FALSE);
        cg.SetOutOfOrderContinuationPoint(check_capacity_and_length);
    }

    if (index_vr || is_push)
    {
#ifdef ARCHITECTURE_AMD64
        cg.SHL(ES_CodeGenerator::IMMEDIATE(1), index, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

        cg.LEA(ES_CodeGenerator::MEMORY(indexed_properties, index, ES_CodeGenerator::SCALE_8, ES_OFFSETOF(ES_Compact_Indexed_Properties, values[0])), index, ES_CodeGenerator::OPSIZE_POINTER);
    }
    else
        cg.LEA(ES_CodeGenerator::MEMORY(indexed_properties, ES_OFFSETOF(ES_Compact_Indexed_Properties, values[0]) + VALUE_INDEX_TO_OFFSET(constant_index)), index, ES_CodeGenerator::OPSIZE_POINTER);

    if (known_type)
    {
        OP_ASSERT(value.Type() != ESTYPE_INT32_OR_DOUBLE);

        cg.MOV(ES_CodeGenerator::IMMEDIATE(value.Type()), VALUE_TYPE(index), ES_CodeGenerator::OPSIZE_32);

        if (known_value && (value.IsBoolean() || value.IsInt32()))
            cg.MOV(ES_CodeGenerator::IMMEDIATE(value.IsBoolean() ? INT32(value.GetBoolean()) : value.GetInt32()), VALUE_VALUE(index), ES_CodeGenerator::OPSIZE_32);
        else if (value.IsUndefined())
            cg.MOV(ES_CodeGenerator::IMMEDIATE(1), VALUE_VALUE(index), ES_CodeGenerator::OPSIZE_32);
        else if (!value.IsNull())
        {
#ifdef ARCHITECTURE_AMD64
            ES_CodeGenerator::OperandSize size = (value.IsBoolean() || value.IsInt32()) ? ES_CodeGenerator::OPSIZE_32 : ES_CodeGenerator::OPSIZE_64;

            cg.MOV(REGISTER_VALUE(source_vr), object, size);
            cg.MOV(object, VALUE_VALUE(index), size);
#else // ARCHITECTURE_AMD64
            if (!value.IsDouble())
            {
                cg.MOV(REGISTER_VALUE(source_vr), object, ES_CodeGenerator::OPSIZE_32);
                cg.MOV(object, VALUE_VALUE(index), ES_CodeGenerator::OPSIZE_32);
            }
            else
            {
                cg.MOV(REGISTER_VALUE(source_vr), object, ES_CodeGenerator::OPSIZE_32);
                cg.MOV(REGISTER_VALUE2(source_vr), properties, ES_CodeGenerator::OPSIZE_32);
                cg.MOV(object, VALUE_VALUE(index), ES_CodeGenerator::OPSIZE_32);
                cg.MOV(properties, VALUE_VALUE2(index), ES_CodeGenerator::OPSIZE_32);
            }
#endif // ARCHITECTURE_AMD64
        }
    }
    else
    {
        ES_CodeGenerator::JumpTarget *not_undefined = cg.ForwardJump(), *finished = cg.ForwardJump();

        cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_UNDEFINED), REGISTER_TYPE(source_vr), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(not_undefined, ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, TRUE);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(1), VALUE_VALUE(index), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_UNDEFINED), VALUE_TYPE(index), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(finished);
        cg.SetJumpTarget(not_undefined);

        CopyValue(cg, source_vr, index, VALUE_INDEX_TO_OFFSET(0), object, properties);

        cg.SetJumpTarget(finished);
    }

    if (is_push)
    {
        /* The return value of Array.prototype.push is the new value of the
           'length' property. We know that 'object_vr' is the receiver of
           the call to Array.prototype.push and that that register doubles as
           return register, so write the new length here instead of doing a
           EmitLengthGet which will check types that we've already checked. */
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(object_vr), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(index_plus_one, REGISTER_VALUE(object_vr), ES_CodeGenerator::OPSIZE_32);
    }
}

void
ES_Native::EmitInt32ByteArrayGet(VirtualRegister *target_vr, VirtualRegister *object_vr, VirtualRegister *index_vr, unsigned constant_index)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register properties(ES_CodeGenerator::REG_CX);
    ES_CodeGenerator::Register index(ES_CodeGenerator::REG_DX);
    ES_CodeGenerator::Register value(ES_CodeGenerator::REG_SI);

    if (!current_slow_case)
        EmitSlowCaseCall();

    LoadObjectOperand(object_vr->index, object);

    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_BYTE_ARRAY_INDEXED), OBJECT_MEMBER(object, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(OBJECT_MEMBER(object, ES_Object, indexed_properties), properties, ES_CodeGenerator::OPSIZE_POINTER);

    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);

    ES_Value_Internal index_value;
    if (!index_vr || IsImmediate(index_vr, index_value) && index_value.IsInt32() && index_value.GetInt32() >= 0)
    {
        if (index_vr)
            constant_index = index_value.GetInt32();

        cg.CMP(ES_CodeGenerator::IMMEDIATE(constant_index), OBJECT_MEMBER(properties, ES_Byte_Array_Indexed, capacity), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW_EQUAL, TRUE, FALSE);

        cg.MOV(OBJECT_MEMBER(properties, ES_Byte_Array_Indexed, byte_array), properties, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOVZX(ES_CodeGenerator::MEMORY(properties, constant_index), value, ES_CodeGenerator::OPSIZE_8);
    }
    else
    {
        cg.MOV(REGISTER_VALUE(index_vr), index, ES_CodeGenerator::OPSIZE_32);

        /* Check that index is within array's capacity: */
        cg.CMP(index, OBJECT_MEMBER(properties, ES_Byte_Array_Indexed, capacity), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW_EQUAL, TRUE, FALSE);

        cg.MOV(OBJECT_MEMBER(properties, ES_Byte_Array_Indexed, byte_array), properties, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOVZX(ES_CodeGenerator::MEMORY(properties, index, ES_CodeGenerator::SCALE_1), value, ES_CodeGenerator::OPSIZE_8);
    }

    cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(target_vr), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(value, REGISTER_VALUE(target_vr), ES_CodeGenerator::OPSIZE_32);
}

void
ES_Native::EmitInt32ByteArrayPut(VirtualRegister *object_vr, VirtualRegister *index_vr, unsigned constant_index, VirtualRegister *source_vr, int *known_value)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_DI);
    ES_CodeGenerator::Register properties(ES_CodeGenerator::REG_CX);
    ES_CodeGenerator::Register index(ES_CodeGenerator::REG_DX);
    ES_CodeGenerator::Register value(ES_CodeGenerator::REG_AX);

    if (!current_slow_case)
        EmitSlowCaseCall();

    LoadObjectOperand(object_vr->index, object);

    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_BYTE_ARRAY_INDEXED), OBJECT_MEMBER(object, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(OBJECT_MEMBER(object, ES_Object, indexed_properties), properties, ES_CodeGenerator::OPSIZE_POINTER);

    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);

    ES_CodeGenerator::Operand op_value;

    if (known_value)
        op_value = ES_CodeGenerator::IMMEDIATE(static_cast<int>(static_cast<signed char>(*known_value)));
    else
    {
        cg.MOV(REGISTER_VALUE(source_vr), value, ES_CodeGenerator::OPSIZE_32);
        op_value = value;
    }

    ES_Value_Internal index_value;
    if (!index_vr || IsImmediate(index_vr, index_value) && index_value.IsInt32() && index_value.GetInt32() >= 0)
    {
        if (index_vr)
            constant_index = index_value.GetInt32();

        cg.CMP(ES_CodeGenerator::IMMEDIATE(constant_index), OBJECT_MEMBER(properties, ES_Byte_Array_Indexed, capacity), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW_EQUAL, TRUE, FALSE);

        cg.MOV(OBJECT_MEMBER(properties, ES_Byte_Array_Indexed, byte_array), properties, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(op_value, ES_CodeGenerator::MEMORY(properties, constant_index), ES_CodeGenerator::OPSIZE_8);
    }
    else
    {
        cg.MOV(REGISTER_VALUE(index_vr), index, ES_CodeGenerator::OPSIZE_32);

        /* Check that index is within array's capacity: */
        cg.CMP(index, OBJECT_MEMBER(properties, ES_Byte_Array_Indexed, capacity), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW_EQUAL, TRUE, FALSE);

        cg.MOV(OBJECT_MEMBER(properties, ES_Byte_Array_Indexed, byte_array), properties, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(op_value, ES_CodeGenerator::MEMORY(properties, index, ES_CodeGenerator::SCALE_1), ES_CodeGenerator::OPSIZE_8);
    }
}

static void
ConvertUint32ToDouble(ES_Native *native, ES_Native::VirtualRegister *target_vr, ES_CodeGenerator::Register &value)
{
    ES_CodeGenerator &cg = native->cg;
#ifdef IA32_X87_SUPPORT
    if (native->IS_FPMODE_X87)
    {
        cg.MOV(value, REGISTER_VALUE(target_vr), ES_CodeGenerator::OPSIZE_32);
        cg.FILD(REGISTER_VALUE(target_vr));
        cg.FADD(native->UintMaxPlus1(cg));
        cg.FST(REGISTER_VALUE(target_vr), TRUE, ES_CodeGenerator::OPSIZE_64);
    }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
    if (native->IS_FPMODE_SSE2)
    {
#ifdef ARCHITECTURE_AMD64
        ES_CodeGenerator::XMM xmm = ES_CodeGenerator::REG_XMM15;
        cg.CVTSI2SD(value, xmm, ES_CodeGenerator::OPSIZE_64);
        cg.MOV(ESTYPE_DOUBLE, REGISTER_TYPE(target_vr));
#else // ARCHITECTURE_AMD64
        ES_CodeGenerator::XMM xmm = ES_CodeGenerator::REG_XMM7;
        cg.CVTSI2SD(value, xmm);
        cg.ADDSD(native->UintMaxPlus1(cg), xmm);
#endif // ARCHITECTURE_AMD64
        cg.MOVSD(xmm, REGISTER_VALUE(target_vr));
    }
#endif // IA32_SSE2_SUPPORT
}

static void
ConvertSingleToDouble(ES_Native *native, ES_Native::VirtualRegister *target_vr, const ES_CodeGenerator::Operand &value)
{
    ES_CodeGenerator &cg = native->cg;
#ifdef IA32_X87_SUPPORT
    if (native->IS_FPMODE_X87)
    {
        cg.FLD(value, ES_CodeGenerator::OPSIZE_32);
        cg.FST(REGISTER_VALUE(target_vr), TRUE, ES_CodeGenerator::OPSIZE_64);
#ifndef ES_VALUES_AS_NANS
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), REGISTER_TYPE(target_vr));
#endif // ES_VALUES_AS_NANS
    }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
    if (native->IS_FPMODE_SSE2)
    {
#ifdef ARCHITECTURE_AMD64
        ES_CodeGenerator::XMM xmm = ES_CodeGenerator::REG_XMM15;
#else // ARCHITECTURE_AMD64
        ES_CodeGenerator::XMM xmm = ES_CodeGenerator::REG_XMM7;
#endif // ARCHITECTURE_AMD64
        cg.CVTSS2SD(value, xmm);
        StoreDoubleInRegister(native, xmm, target_vr);
    }
#endif // IA32_SSE2_SUPPORT
}

void
ES_Native::EmitInt32TypedArrayGet(VirtualRegister *target_vr, VirtualRegister *object_vr, VirtualRegister *index_vr, unsigned constant_index, unsigned type_array_bits)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_DI);
    ES_CodeGenerator::Register indexed_properties(ES_CodeGenerator::REG_CX);
    ES_CodeGenerator::Register index(ES_CodeGenerator::REG_DX);

    if (!current_slow_case)
        EmitSlowCaseCall();

    LoadObjectOperand(object_vr->index, object);

    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_TYPE_ARRAY_INDEXED), OBJECT_MEMBER(object, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(OBJECT_MEMBER(object, ES_Object, indexed_properties), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);

    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);

    ES_Value_Internal index_value;
    if (!index_vr || IsImmediate(index_vr, index_value) && index_value.IsInt32() && index_value.GetInt32() >= 0)
    {
        if (index_vr)
        {
            constant_index = index_value.GetInt32();
            index_vr = NULL;
        }

        cg.CMP(ES_CodeGenerator::IMMEDIATE(constant_index), OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, capacity), ES_CodeGenerator::OPSIZE_32);
    }
    else
    {
        constant_index = 0;
        cg.MOV(REGISTER_VALUE(index_vr), index, ES_CodeGenerator::OPSIZE_32);

        cg.CMP(index, OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, capacity), ES_CodeGenerator::OPSIZE_32);
    }

    /* Check that index is within array's capacity: */
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW_EQUAL, TRUE, FALSE);

    ES_CodeGenerator::Register kind(ES_CodeGenerator::REG_AX);
    cg.MOV(OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, kind), kind, ES_CodeGenerator::OPSIZE_32);

    ES_CodeGenerator::JumpTarget *slow_case = current_slow_case->GetJumpTarget();
    ES_CodeGenerator::JumpTarget *done_int_target = NULL;
    ES_CodeGenerator::JumpTarget *done_target = NULL;

    unsigned possible = ES_Type_Array_Indexed::Int8Array;
    unsigned handled = type_array_bits;

    /* Iterate over all possible values in the ES_Type_Array_Indexed::TypeArrayKind enum.
       The variable handled contains bits mapping to the values in the enum, and is
       gathered by the profiling steps. Right shifting it will make us only emit code for
       array kinds seen */
    ES_CodeGenerator::Register value(object);
    for (; handled; possible++)
    {
        /* If the profile says that we've never seen the array kind, then skip it .*/
        if (!(handled & 1))
        {
            handled = handled >> 1;
            continue;
        }

        handled = handled >> 1;

        BOOL have_next = handled != 0;

        ES_CodeGenerator::JumpTarget *next;
        if (have_next)
            next = cg.ForwardJump();
        else
            next = slow_case;

        cg.CMP(ES_CodeGenerator::IMMEDIATE(possible), kind, ES_CodeGenerator::OPSIZE_32);
        cg.Jump(next, ES_NATIVE_CONDITION_NOT_EQUAL, FALSE, TRUE);

        ES_CodeGenerator::Register offset = index;

        ES_CodeGenerator::OperandSize operand_size = ES_CodeGenerator::OPSIZE_8;
        BOOL sign = TRUE;
        unsigned needs_shift = 0;

        /* Find out the parameters needed to emit code. Size, sign and how the
           memory is needed to be shifted before read. */
        switch (possible)
        {
        case ES_Type_Array_Indexed::Uint8Array:
        case ES_Type_Array_Indexed::Uint8ClampedArray:
            sign = FALSE;
            /* fall through */
        case ES_Type_Array_Indexed::Int8Array:
            break;
        case ES_Type_Array_Indexed::Uint16Array:
            sign = FALSE;
            /* fall through */
        case ES_Type_Array_Indexed::Int16Array:
            needs_shift = 1;
            operand_size = ES_CodeGenerator::OPSIZE_16;
            break;
        case ES_Type_Array_Indexed::Uint32Array:
            sign = FALSE;
            /* fall through */
        case ES_Type_Array_Indexed::Int32Array:
            needs_shift = 2;
            operand_size = ES_CodeGenerator::OPSIZE_32;
            break;
        case ES_Type_Array_Indexed::Float32Array:
            needs_shift = 2;
            operand_size = ES_CodeGenerator::OPSIZE_32;
            break;
        case ES_Type_Array_Indexed::Float64Array:
            needs_shift = 3;
            operand_size = ES_CodeGenerator::OPSIZE_64;
            break;
        }

        if (index_vr)
        {
            if (needs_shift)
                cg.SHL(ES_CodeGenerator::IMMEDIATE(needs_shift), offset, ES_CodeGenerator::OPSIZE_32);

            cg.ADD(OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, byte_offset), offset, ES_CodeGenerator::OPSIZE_32);
        }
        else
            cg.MOV(OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, byte_offset), offset, ES_CodeGenerator::OPSIZE_32);

        cg.MOV(OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, byte_array), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(OBJECT_MEMBER(indexed_properties, ES_Byte_Array_Indexed, byte_array), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);

        /* If the array kind is one of the int kinds then the emitted code only depend on size and sign. */
        if (possible < ES_Type_Array_Indexed::Float32Array)
        {
            if (operand_size == ES_CodeGenerator::OPSIZE_32)
                cg.MOV(ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size), value, operand_size);
            else if (sign)
                cg.MOVSX(ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size), value, operand_size);
            else
                cg.MOVZX(ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size), value, operand_size);

            /* Check if we need to promote an unsigned integer to a double. */
            if (possible == ES_Type_Array_Indexed::Uint32Array)
            {
                if (!done_target)
                    done_target = cg.ForwardJump();

                ES_CodeGenerator::JumpTarget *to_int32 = cg.ForwardJump();
                cg.TEST(value, value, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(to_int32, ES_NATIVE_CONDITION_NOT_SIGN);
                ConvertUint32ToDouble(this, target_vr, value);
                cg.Jump(done_target);
                cg.SetJumpTarget(to_int32);
            }

            if (have_next)
            {
                if (!done_int_target)
                    done_int_target = cg.ForwardJump();

                cg.Jump(done_int_target);
            }
        }
        else if (possible == ES_Type_Array_Indexed::Float32Array)
        {
            /* We don't have floats in ES_Value_Internal. */
            ConvertSingleToDouble(this, target_vr, ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size));

            if (type_array_bits || done_int_target)
            {
                if (!done_target)
                    done_target = cg.ForwardJump();

                cg.Jump(done_target);
            }
        }
        else
        {
            cg.LEA(ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);
            CopyTypedDataToValue(cg, indexed_properties, 0, ES_STORAGE_DOUBLE, target_vr, ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_DX);

            if (done_int_target)
            {
                if (!done_target)
                    done_target = cg.ForwardJump();

                cg.Jump(done_target);
            }
        }

        if (have_next)
            cg.SetJumpTarget(next);
    }

    if (done_int_target)
        cg.SetJumpTarget(done_int_target);

    if (type_array_bits & ES_Type_Array_Indexed::AnyIntArray)
    {
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(target_vr), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(value, REGISTER_VALUE(target_vr), ES_CodeGenerator::OPSIZE_32);
    }

    if (done_target)
        cg.SetJumpTarget(done_target);
}

static void
ConvertIntToFloat(ES_Native *native, ES_Native::VirtualRegister *source_vr, const ES_CodeGenerator::Operand &target, ES_CodeGenerator::OperandSize size)
{
    ES_CodeGenerator &cg = native->cg;
#ifdef IA32_X87_SUPPORT
    if (native->IS_FPMODE_X87)
    {
        cg.FILD(REGISTER_VALUE(source_vr));
        cg.FST(target, TRUE, size);
    }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
    if (native->IS_FPMODE_SSE2)
    {
#ifdef ARCHITECTURE_AMD64
        ES_CodeGenerator::XMM xmm = ES_CodeGenerator::REG_XMM15;
#else // ARCHITECTURE_AMD64
        ES_CodeGenerator::XMM xmm = ES_CodeGenerator::REG_XMM7;
#endif // ARCHITECTURE_AMD64

        if (size == ES_CodeGenerator::OPSIZE_32)
        {
            cg.CVTSI2SS(REGISTER_VALUE(source_vr), xmm, ES_CodeGenerator::OPSIZE_32);
            cg.MOVSS(xmm, target);
        }
        else
        {
            cg.CVTSI2SD(REGISTER_VALUE(source_vr), xmm, ES_CodeGenerator::OPSIZE_32);
            cg.MOVSD(xmm, target);
        }
    }
#endif // IA32_SSE2_SUPPORT
}

static void
ConvertDoubleToSingle(ES_Native *native, ES_Native::VirtualRegister *source_vr, const ES_CodeGenerator::Operand &target)
{
    ES_CodeGenerator &cg = native->cg;
#ifdef IA32_X87_SUPPORT
    if (native->IS_FPMODE_X87)
    {
        cg.FLD(REGISTER_VALUE(source_vr), ES_CodeGenerator::OPSIZE_64);
        cg.FST(target, TRUE, ES_CodeGenerator::OPSIZE_32);
    }
#endif // IA32_X87_SUPPORT

#ifdef IA32_SSE2_SUPPORT
    if (native->IS_FPMODE_SSE2)
    {
#ifdef ARCHITECTURE_AMD64
        ES_CodeGenerator::XMM xmm = ES_CodeGenerator::REG_XMM15;
#else // ARCHITECTURE_AMD64
        ES_CodeGenerator::XMM xmm = ES_CodeGenerator::REG_XMM7;
#endif // ARCHITECTURE_AMD64
        cg.CVTSD2SS(REGISTER_VALUE(source_vr), xmm);
        cg.MOVSS(xmm, target);
    }
#endif // IA32_SSE2_SUPPORT
}

static int
ToSize(unsigned value, ES_CodeGenerator::OperandSize operand_size)
{
    switch(operand_size)
    {
        case ES_CodeGenerator::OPSIZE_8:
            return static_cast<signed char>(value);
        case ES_CodeGenerator::OPSIZE_16:
            return static_cast<signed short>(value);
        case ES_CodeGenerator::OPSIZE_32:
            return value;
        default:
            OP_ASSERT(!"Should not happen");
            return value;
    }
}

void
ES_Native::EmitInt32TypedArrayPut(VirtualRegister *object_vr, VirtualRegister *index_vr, unsigned constant_index, unsigned type_array_bits, VirtualRegister *source_vr, unsigned char source_type_bits, ES_Value_Internal *known_value, ES_ValueType *known_type)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register index(ES_CodeGenerator::REG_DX);
    ES_CodeGenerator::Register value(ES_CodeGenerator::REG_CX);

    ANNOTATE("    EmitInt32TypedArrayPut");

    OP_ASSERT(source_type_bits != 0 || known_type || known_value);

    if (!current_slow_case)
        EmitSlowCaseCall();

    ES_CodeGenerator::JumpTarget *slow_case = current_slow_case->GetJumpTarget();

    LoadObjectOperand(object_vr->index, object);

    ES_CodeGenerator::Register indexed_properties(object);

    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_TYPE_ARRAY_INDEXED), OBJECT_MEMBER(object, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(OBJECT_MEMBER(object, ES_Object, indexed_properties), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);

    cg.Jump(SLOW_CASE(slow_case, "Not a static type array"), ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);

    ES_Value_Internal index_value;
    if (!index_vr || IsImmediate(index_vr, index_value) && index_value.IsInt32() && index_value.GetInt32() >= 0)
    {
        if (index_vr)
        {
            constant_index = index_value.GetInt32();
            index_vr = NULL;
        }

        cg.CMP(ES_CodeGenerator::IMMEDIATE(constant_index), OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, capacity), ES_CodeGenerator::OPSIZE_32);
    }
    else
    {
        constant_index = 0;
        cg.MOV(REGISTER_VALUE(index_vr), index, ES_CodeGenerator::OPSIZE_32);

        cg.CMP(index, OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, capacity), ES_CodeGenerator::OPSIZE_32);
    }

    /* Check that index is within array's capacity: */
    cg.Jump(SLOW_CASE(slow_case, "Not within typed array capacity"), ES_NATIVE_CONDITION_BELOW_EQUAL, TRUE, FALSE);

    ES_CodeGenerator::Register kind_bits(ES_CodeGenerator::REG_DI);
    cg.MOV(OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, kind), ES_CodeGenerator::REG_CX, ES_CodeGenerator::OPSIZE_32);
    cg.MOV(ES_CodeGenerator::IMMEDIATE(1), kind_bits, ES_CodeGenerator::OPSIZE_32);
    cg.SHL(ES_CodeGenerator::REG_CX, kind_bits, ES_CodeGenerator::OPSIZE_32);

    ES_CodeGenerator::OutOfOrderBlock *is_int = NULL;
    ES_CodeGenerator::OutOfOrderBlock *is_double = NULL;

    ES_CodeGenerator::Register offset(index);

    if (!index_vr)
    {
        cg.MOV(OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, byte_offset), offset, ES_CodeGenerator::OPSIZE_32);
        cg.MOV(OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, byte_array), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(OBJECT_MEMBER(indexed_properties, ES_Byte_Array_Indexed, byte_array), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);
    }

    ES_ValueType types[2] = { ESTYPE_INT32, ESTYPE_DOUBLE };
    ES_ValueType type;

    /* We will begin by dispatching on the type of the written value, but we
       will only emit code for types that we've seen. The only possible types
       are ESTYPE_INT32 and ESTYPE_DOUBLE. */
    for (unsigned i = 0; i < 2; ++i)
    {
        type = types[i];

        if (!(known_type && *known_type == type || (source_type_bits & ES_Value_Internal::TypeBits(type))))
            continue;

        ES_CodeGenerator::OutOfOrderBlock *is_type = NULL;
        ES_CodeGenerator::JumpTarget *done_target = NULL;

        /* If we've seen both ESTYPE_INT32 and ESTYPE_DOUBLE we will emit the
           code in an out of order block which we will jump to later based on a
           type check. */
        if (!known_type && (source_type_bits & ~ES_Value_Internal::TypeBits(type)) != 0)
            is_type = cg.StartOutOfOrderBlock();
        else
        {
            /* Otherwise, if we've only seen one, do the type check now and
               go to slow case if it fails. */
            if (!known_type)
                EmitRegisterTypeCheck(source_vr, type, SLOW_CASE(slow_case, "Typed array type check failed"));

            source_type_bits = 0;
        }

        unsigned possible = ES_Type_Array_Indexed::Int8Array;
        unsigned handled = type_array_bits;

        OP_ASSERT(handled);

        /* Iterate over all possible values in the ES_Type_Array_Indexed::TypeArrayKind enum.
           The variable handled contains bits mapping to the values in the enum, and is
           gathered by the profiling steps. Right shifting it will make us only emit code for
           array kinds seen */
        for (; handled; ++possible)
        {
            /* If the profile says that we've never seen the array kind, then skip it .*/
            if (!(handled & 1))
            {
                handled = handled >> 1;
                continue;
            }

            handled = handled >> 1;

            ES_CodeGenerator::OperandSize operand_size;
            unsigned shift;
            ES_Type_Array_Indexed::TypeArrayBits bits;

            /* Find out the parameters needed to emit code. Size and how the memory is
               needed to be shifted before written to. And also, we handle kinds by size
               since at this point the sign is not important. This means that we adjust
               handled and possible to skip ahead in the iteration. */
            switch (possible)
            {
            case ES_Type_Array_Indexed::Int8Array:
                handled = handled >> 1;
                ++possible;
                /* fall through */
            case ES_Type_Array_Indexed::Uint8Array:
            case ES_Type_Array_Indexed::Uint8ClampedArray:
                shift = 0; operand_size = ES_CodeGenerator::OPSIZE_8;
                bits = ES_Type_Array_Indexed::AnyInt8Array;
                break;
            case ES_Type_Array_Indexed::Int16Array:
                handled = handled >> 1;
                ++possible;
                /* fall through */
            case ES_Type_Array_Indexed::Uint16Array:
                shift = 1; operand_size = ES_CodeGenerator::OPSIZE_16;
                bits = ES_Type_Array_Indexed::AnyInt16Array;
                break;
            case ES_Type_Array_Indexed::Int32Array:
                handled = handled >> 1;
                ++possible;
                /* fall through */
            case ES_Type_Array_Indexed::Uint32Array:
                shift = 2; operand_size = ES_CodeGenerator::OPSIZE_32;
                bits = ES_Type_Array_Indexed::AnyInt32Array;
                break;
            case ES_Type_Array_Indexed::Float32Array:
                shift = 2; operand_size = ES_CodeGenerator::OPSIZE_32;
                bits = ES_Type_Array_Indexed::Float32ArrayBit;
                break;
            case ES_Type_Array_Indexed::Float64Array:
                shift = 3; operand_size = ES_CodeGenerator::OPSIZE_64;
                bits = ES_Type_Array_Indexed::Float64ArrayBit;
                break;
            default:
                OP_ASSERT(!"Should not happen");
                shift = 0; operand_size = ES_CodeGenerator::OPSIZE_8;
                bits = ES_Type_Array_Indexed::AnyIntArray;
                break;
            }

            ES_CodeGenerator::JumpTarget *next = NULL;
            BOOL have_next = handled != 0;
            if (have_next)
            {
                if (!done_target)
                    done_target = cg.ForwardJump();

                next = cg.ForwardJump();
            }
            else
                next = SLOW_CASE(slow_case, "No more cached entries for typed array");

            cg.TEST(ES_CodeGenerator::IMMEDIATE(static_cast<unsigned>(bits)), kind_bits, ES_CodeGenerator::OPSIZE_32);
            cg.Jump(next, ES_NATIVE_CONDITION_ZERO, FALSE, TRUE);

            if (index_vr)
            {
                if (shift)
                    cg.SHL(ES_CodeGenerator::IMMEDIATE(shift), offset, ES_CodeGenerator::OPSIZE_32);

                cg.ADD(OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, byte_offset), offset, ES_CodeGenerator::OPSIZE_32);
                cg.MOV(OBJECT_MEMBER(indexed_properties, ES_Type_Array_Indexed, byte_array), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);
                cg.MOV(OBJECT_MEMBER(indexed_properties, ES_Byte_Array_Indexed, byte_array), indexed_properties, ES_CodeGenerator::OPSIZE_POINTER);
            }

            if (type == ESTYPE_INT32)
            {
                /* When writing to an int array we only need to take the size into consideration.
                   When writing an int to a float array we need to convert it to the correct float. */
                if (possible < ES_Type_Array_Indexed::Float32Array)
                {
                    if (known_value)
                    {
                        unsigned imm_value;
                        if (possible == ES_Type_Array_Indexed::Uint8ClampedArray)
                            imm_value = known_value->GetNumAsUint8Clamped();
                        else
                            imm_value = known_value->GetNumAsUInt32();

                        cg.MOV(ES_CodeGenerator::IMMEDIATE(ToSize(imm_value, operand_size)), ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size), operand_size);
                    }
                    else
                    {
                        cg.MOV(REGISTER_VALUE(source_vr), value, ES_CodeGenerator::OPSIZE_32);

                        cg.CMP(ES_CodeGenerator::IMMEDIATE(0x1 << static_cast<unsigned>(ES_Type_Array_Indexed::Uint8ClampedArray)), kind_bits, ES_CodeGenerator::OPSIZE_32);
                        if (possible == ES_Type_Array_Indexed::Uint8ClampedArray)
                        {
                            ES_CodeGenerator::JumpTarget *do_clamp = cg.ForwardJump();
                            ES_CodeGenerator::JumpTarget *clamp_done = cg.ForwardJump();
                            cg.Jump(clamp_done, ES_NATIVE_CONDITION_NOT_EQUAL, FALSE, TRUE);

                            cg.CMP(ES_CodeGenerator::IMMEDIATE(0xff), value, ES_CodeGenerator::OPSIZE_32);
                            cg.Jump(do_clamp, ES_NATIVE_CONDITION_NOT_BELOW_EQUAL);
                            cg.Jump(clamp_done);
                            cg.SetJumpTarget(do_clamp);
                            cg.SAR(ES_CodeGenerator::IMMEDIATE(31), value, ES_CodeGenerator::OPSIZE_32);
                            cg.NOT(value, ES_CodeGenerator::OPSIZE_32);
                            cg.SetJumpTarget(clamp_done);
                        }
                        else
                            cg.Jump(SLOW_CASE(slow_case, "Unhandled Uint8Array"), ES_NATIVE_CONDITION_EQUAL, FALSE, TRUE);

                        cg.MOV(value, ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size), operand_size);
                    }
                }
                else
                    ConvertIntToFloat(this, source_vr, ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size), operand_size);
            }
            else
            {
                /* When writing a double to an int array or a float32 array we need to convert it,
                   otherwise we can just write it. */
                if (possible < ES_Type_Array_Indexed::Float32Array)
                {
                    if (known_value)
                    {
                        unsigned imm_value;
                        if (possible == ES_Type_Array_Indexed::Uint8ClampedArray)
                            imm_value = known_value->GetNumAsUint8Clamped();
                        else
                            imm_value = known_value->GetNumAsUInt32();

                        cg.MOV(ES_CodeGenerator::IMMEDIATE(ToSize(imm_value, operand_size)), ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size), operand_size);
                    }
                    else
                    {
                        ConvertDoubleToInt(this, source_vr, value, FALSE, possible != ES_Type_Array_Indexed::Uint8ClampedArray, SLOW_CASE(slow_case, "Double conversion failed"));
                        if (possible == ES_Type_Array_Indexed::Uint8ClampedArray)
                        {
                            ES_CodeGenerator::JumpTarget *do_clamp = cg.ForwardJump();
                            ES_CodeGenerator::JumpTarget *clamp_done = cg.ForwardJump();

                            cg.CMP(ES_CodeGenerator::IMMEDIATE(0xff), value, ES_CodeGenerator::OPSIZE_32);
                            cg.Jump(do_clamp, ES_NATIVE_CONDITION_NOT_BELOW_EQUAL);
                            cg.Jump(clamp_done);
                            cg.SetJumpTarget(do_clamp);
                            cg.SAR(ES_CodeGenerator::IMMEDIATE(31), value, ES_CodeGenerator::OPSIZE_32);
                            cg.NOT(value, ES_CodeGenerator::OPSIZE_32);
                            cg.SetJumpTarget(clamp_done);
                        }
                        cg.MOV(value, ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size), operand_size);
                    }
                }
                else if (possible == ES_Type_Array_Indexed::Float32Array)
                    ConvertDoubleToSingle(this, source_vr, ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size));
                else
                {
#ifdef ARCHITECTURE_AMD64
                    cg.MOV(REGISTER_VALUE(source_vr), value, operand_size);
                    cg.MOV(value, ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size), operand_size);
#else // ARCHITECTURE_AMD64
                    ES_CodeGenerator::Register scratch(ES_CodeGenerator::REG_SI);
                    cg.MOV(REGISTER_VALUE(source_vr), value, ES_CodeGenerator::OPSIZE_32);
                    cg.MOV(REGISTER_VALUE2(source_vr), scratch, ES_CodeGenerator::OPSIZE_32);
                    cg.MOV(value, ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size + VALUE_VALUE_OFFSET), ES_CodeGenerator::OPSIZE_32);
                    cg.MOV(scratch, ES_CodeGenerator::MEMORY(indexed_properties, offset, ES_CodeGenerator::SCALE_1, constant_index * operand_size + VALUE_VALUE2_OFFSET), ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64
                }
            }

            if (done_target)
                cg.Jump(done_target);

            if (have_next)
                cg.SetJumpTarget(next);
        }

        if (done_target)
            cg.SetJumpTarget(done_target);

        if (is_type)
            cg.EndOutOfOrderBlock();

        if (type == ESTYPE_INT32)
            is_int = is_type;
        else
            is_double = is_type;
    }

    /* If we've emitted an out of order block to handle writing ints, emit a
       type based jump to that out of order block.*/
    if (is_int)
        EmitRegisterTypeCheck(source_vr, ESTYPE_INT32, is_int->GetJumpTarget(), TRUE);

    /* If we've emitted an out of order block to handle writing doubles, emit a
       type based jump to that out of order block.*/
    if (is_double)
        EmitRegisterTypeCheck(source_vr, ESTYPE_DOUBLE, is_double->GetJumpTarget(), TRUE);

    /* If we haven't laid out the writing in the straight line code, then we need to
       jump to slow case here if none of the type checks have succeeded. */
    if (is_int || is_double)
        cg.Jump(SLOW_CASE(slow_case, "Not an int or double"));

    if (is_int)
        cg.SetOutOfOrderContinuationPoint(is_int);

    if (is_double)
        cg.SetOutOfOrderContinuationPoint(is_double);
}

void
ES_Native::EmitGetEnumerated(VirtualRegister *target_vr, VirtualRegister *object_vr, VirtualRegister *name_vr)
{
    DECLARE_NOTHING();

    if (!current_slow_case)
        EmitSlowCaseCall();

    ES_CodeGenerator::Register name(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_DX);
    ES_CodeGenerator::Register klass(ES_CodeGenerator::REG_CX);
    ES_CodeGenerator::Register property(ES_CodeGenerator::REG_DI);
    ES_CodeGenerator::Register type(object);

    cg.MOV(REGISTER_VALUE(name_vr), name, ES_CodeGenerator::OPSIZE_POINTER);

    LoadObjectOperand(object_vr->index, object);

    cg.CMP(name, OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, enumerated_string), ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL);

    cg.MOV(OBJECT_MEMBER(object, ES_Object, klass), klass, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(OBJECT_MEMBER(klass, ES_Class, class_id), klass, ES_CodeGenerator::OPSIZE_32);

    ES_CodeGenerator::OutOfOrderBlock *indexed = cg.StartOutOfOrderBlock();

    cg.CMP(ES_CodeGenerator::IMMEDIATE(ES_Class::NOT_CACHED_CLASS_ID), OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, enumerated_class_id), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL);

    ES_CodeGenerator::Register index(ES_CodeGenerator::REG_AX);

    cg.MOV(OBJECT_MEMBER(object, ES_Object, indexed_properties), property, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, enumerated_index), index, ES_CodeGenerator::OPSIZE_32);

    cg.TEST(ES_CodeGenerator::IMMEDIATE(ES_Object::MASK_SIMPLE_COMPACT_INDEXED), OBJECT_MEMBER(object, ES_Object, object_bits), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_ZERO);

    cg.CMP(index, OBJECT_MEMBER(property, ES_Compact_Indexed_Properties, capacity), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW_EQUAL);

#ifdef ES_VALUES_AS_NANS
    cg.LEA(ES_CodeGenerator::MEMORY(property, index, ES_CodeGenerator::SCALE_8, ES_OFFSETOF(ES_Compact_Indexed_Properties, values)), property, ES_CodeGenerator::OPSIZE_POINTER);
#else // ES_VALUES_AS_NANS
    cg.SHL(ES_CodeGenerator::IMMEDIATE(4), index, ES_CodeGenerator::OPSIZE_POINTER);
    cg.LEA(ES_CodeGenerator::MEMORY(property, index, ES_CodeGenerator::SCALE_1, ES_OFFSETOF(ES_Compact_Indexed_Properties, values)), property, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ES_VALUES_AS_NANS

    cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_UNDEFINED), ES_CodeGenerator::MEMORY(property, VALUE_TYPE_OFFSET), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_EQUAL);

    CopyValue(cg, property, VALUE_INDEX_TO_OFFSET(0), target_vr, name, object);

    cg.EndOutOfOrderBlock();

    cg.CMP(klass, OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, enumerated_class_id), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(indexed->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL);

    cg.MOV(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, enumerated_limit), property, ES_CodeGenerator::OPSIZE_32);
    cg.CMP(property, OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_LESS_EQUAL);

    cg.MOV(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, enumerated_cached_offset), property, ES_CodeGenerator::OPSIZE_32);

    cg.ADD(OBJECT_MEMBER(object, ES_Object, properties), property, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, enumerated_cached_type), type, ES_CodeGenerator::OPSIZE_32);

    CopyDataToValue(cg, property, type, target_vr, name);

    cg.SetOutOfOrderContinuationPoint(indexed);
}

void
ES_Native::EmitPutEnumerated(VirtualRegister *object_vr, VirtualRegister *name_vr, VirtualRegister *source_vr)
{
    DECLARE_NOTHING();

    if (!current_slow_case)
        EmitSlowCaseCall();

    ES_CodeGenerator::Register name(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_DX);
    ES_CodeGenerator::Register klass(ES_CodeGenerator::REG_CX);
    ES_CodeGenerator::Register property(ES_CodeGenerator::REG_DI);
    ES_CodeGenerator::Register type(ES_CodeGenerator::REG_CX);

    cg.MOV(REGISTER_VALUE(name_vr), name, ES_CodeGenerator::OPSIZE_POINTER);

    LoadObjectOperand(object_vr->index, object);

    cg.CMP(name, OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, enumerated_string), ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL);

    cg.MOV(OBJECT_MEMBER(object, ES_Object, klass), klass, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(OBJECT_MEMBER(klass, ES_Class, class_id), klass, ES_CodeGenerator::OPSIZE_32);

    cg.CMP(klass, OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, enumerated_class_id), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL);

    cg.MOV(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, enumerated_limit), property, ES_CodeGenerator::OPSIZE_32);
    cg.CMP(property, OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_LESS_EQUAL);

    cg.MOV(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, enumerated_cached_offset), property, ES_CodeGenerator::OPSIZE_32);
    cg.MOV(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, enumerated_cached_type), type, ES_CodeGenerator::OPSIZE_32);

    cg.ADD(OBJECT_MEMBER(object, ES_Object, properties), property, ES_CodeGenerator::OPSIZE_POINTER);

    CopyValueToData(cg, REGISTER_FRAME_POINTER, VALUE_INDEX_TO_OFFSET(source_vr->index), property, VALUE_INDEX_TO_OFFSET(0), type, name, current_slow_case->GetJumpTarget());
}

void
ES_Native::EmitInt32StringIndexedGet(VirtualRegister *target_vr, VirtualRegister *object_vr, VirtualRegister *index_vr, unsigned constant_index, BOOL returnCharCode)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::Register string(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register value(ES_CodeGenerator::REG_DX);

    if (!current_slow_case)
        EmitSlowCaseCall();

    LoadObjectOperand(object_vr->index, string);

    cg.MOV(OBJECT_MEMBER(string, JString, value), value, ES_CodeGenerator::OPSIZE_POINTER);

    /* Make sure the string isn't segmented: */
    cg.TEST(ES_CodeGenerator::IMMEDIATE(1), value, ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_ZERO, TRUE, FALSE);

    ES_CodeGenerator::Register offset(ES_CodeGenerator::REG_CX);

    cg.MOV(OBJECT_MEMBER(string, JString, offset), offset, ES_CodeGenerator::OPSIZE_32);
    cg.LEA(ES_CodeGenerator::MEMORY(value, offset, ES_CodeGenerator::SCALE_2, ES_OFFSETOF(JStringStorage, storage)), value, ES_CodeGenerator::OPSIZE_POINTER);

    ES_CodeGenerator::Register index(ES_CodeGenerator::REG_CX);

    if (index_vr)
    {
        cg.MOV(REGISTER_VALUE(index_vr), index, ES_CodeGenerator::OPSIZE_32);
        cg.CMP(index, OBJECT_MEMBER(string, JString, length), ES_CodeGenerator::OPSIZE_32);
    }
    else
        cg.CMP(ES_CodeGenerator::IMMEDIATE(constant_index), OBJECT_MEMBER(string, JString, length), ES_CodeGenerator::OPSIZE_32);

    /* Note: unsigned comparison to catch negative index.  Not quite correct if
       the string is longer than 2^31 characters.  Let's hope it's not... */
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_ABOVE, TRUE, FALSE);

    ES_CodeGenerator::Register character(ES_CodeGenerator::REG_AX);

    if (index_vr)
        cg.MOVZX(ES_CodeGenerator::MEMORY(value, index, ES_CodeGenerator::SCALE_2), character, ES_CodeGenerator::OPSIZE_16);
    else
        cg.MOVZX(ES_CodeGenerator::MEMORY(value, constant_index * 2), character, ES_CodeGenerator::OPSIZE_16);

    if (returnCharCode)
    {
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(target_vr), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(character, REGISTER_VALUE(target_vr), ES_CodeGenerator::OPSIZE_32);
    }
    else
    {
        cg.CMP(ES_CodeGenerator::IMMEDIATE(STRING_NUMCHARS), character, ES_CodeGenerator::OPSIZE_32);
        cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_ABOVE_EQUAL, TRUE, FALSE);

        ES_CodeGenerator::Register result(ES_CodeGenerator::REG_DX);

        cg.MOV(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, rt_data), result, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(ES_CodeGenerator::MEMORY(result, character, ES_CodeGenerator::SCALE_POINTER, ES_OFFSETOF(ESRT_Data, strings) + STRING_nul * sizeof(void *)), result, ES_CodeGenerator::OPSIZE_POINTER);

        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_STRING), REGISTER_TYPE(target_vr), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(result, REGISTER_VALUE(target_vr), ES_CodeGenerator::OPSIZE_POINTER);
    }
}

void
ES_Native::EmitPrimitiveNamedGet(VirtualRegister *target_vr, ES_Object *check_object, ES_Object *property_object, unsigned class_id, unsigned property_offset, ES_StorageType storage_type, ES_Object *function)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register object_class(ES_CodeGenerator::REG_CX);
    ES_CodeGenerator::Register value(ES_CodeGenerator::REG_SI);

    MovePointerIntoRegister(cg, check_object, object);

    cg.MOV(OBJECT_MEMBER(object, ES_Object, klass), object_class, ES_CodeGenerator::OPSIZE_POINTER);
    cg.CMP(ES_CodeGenerator::IMMEDIATE(class_id), OBJECT_MEMBER(object_class, ES_Class, class_id), ES_CodeGenerator::OPSIZE_32);

    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);

    if (function)
    {
#ifdef ARCHITECTURE_AMD64
        MovePointerIntoRegister(cg, function, object);

        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(target_vr), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(object, REGISTER_VALUE(target_vr), ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(target_vr), ES_CodeGenerator::OPSIZE_32);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(function), REGISTER_VALUE(target_vr), ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64
    }
    else if (property_object)
    {
        if (check_object != property_object)
            MovePointerIntoRegister(cg, property_object, object);

        cg.MOV(OBJECT_MEMBER(object, ES_Object, properties), value, ES_CodeGenerator::OPSIZE_POINTER);

        if (property_value_write_vr == target_vr && (storage_type == ES_STORAGE_OBJECT || storage_type == ES_STORAGE_WHATEVER))
        {
            SetPropertyValueTransferRegister(NR(IA32_REGISTER_INDEX_SI), property_offset, storage_type != ES_STORAGE_OBJECT);
            if (current_slow_case)
                if (ES_CodeGenerator::OutOfOrderBlock *recover_from_failure = RecoverFromFailedPropertyValueTransfer(this, target_vr, ES_CodeGenerator::REG_SI, storage_type == ES_STORAGE_WHATEVER))
                    cg.SetOutOfOrderContinuationPoint(recover_from_failure);
        }
        else
            CopyTypedDataToValue(cg, value, property_offset, storage_type, target_vr, object, object_class);
    }
    else
        EmitSetRegisterType(target_vr, ESTYPE_UNDEFINED);
}

void
ES_Native::EmitFetchFunction(VirtualRegister *target_vr, ES_Object *function, VirtualRegister *object_vr, unsigned class_id, BOOL fetch_value)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register object_class(ES_CodeGenerator::REG_CX);

    LoadObjectOperand(object_vr->index, object);

    cg.MOV(OBJECT_MEMBER(object, ES_Object, klass), object_class, ES_CodeGenerator::OPSIZE_POINTER);
    cg.CMP(ES_CodeGenerator::IMMEDIATE(class_id), OBJECT_MEMBER(object_class, ES_Class, class_id), ES_CodeGenerator::OPSIZE_32);

    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);

    if (fetch_value)
    {
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(target_vr), ES_CodeGenerator::OPSIZE_32);

#ifdef ARCHITECTURE_AMD64
        MovePointerIntoRegister(cg, function, object);
        cg.MOV(object, REGISTER_VALUE(target_vr), ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
        cg.MOV(ES_CodeGenerator::IMMEDIATE(function), REGISTER_VALUE(target_vr), ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64
    }
}

static void
ES_EmitGetCachePositiveBranch(ES_CodeGenerator &cg, const ES_Native::GetCacheGroup &group, const ES_Native::GetCacheGroup::Entry &entry, ES_CodeGenerator::JumpTarget **copy_value, ES_CodeGenerator::Register object, ES_CodeGenerator::Register value, BOOL properties_loaded, BOOL jump_when_finished)
{
    DECLARE_NOTHING();

    OP_ASSERT(entry.positive);

    if (!properties_loaded)
        cg.MOV(OBJECT_MEMBER(object, ES_Object, properties), value, ES_CodeGenerator::OPSIZE_POINTER);

    if (!group.single_offset && entry.positive_offset)
        cg.ADD(ES_CodeGenerator::IMMEDIATE(entry.positive_offset), value, ES_CodeGenerator::OPSIZE_POINTER);

    if (jump_when_finished)
        cg.Jump(copy_value[group.storage_type]);
}

static const ES_Native::GetCacheGroup *
ES_FindGroupByStorageType(const ES_Native::GetCacheGroup *groups, ES_StorageType storage_type)
{
    while (groups->storage_type != storage_type)
        ++groups;
    return groups;
}

static void
ES_EmitGetCacheNegativeBranch(ES_CodeGenerator &cg, const ES_Native::GetCacheGroup *groups, const ES_Native::GetCacheGroup &group, const ES_Native::GetCacheGroup::Entry &entry, ES_CodeGenerator::JumpTarget **copy_value, ES_CodeGenerator::Register object, ES_CodeGenerator::Register value, BOOL jump_when_finished)
{
    DECLARE_NOTHING();

    OP_ASSERT(entry.prototype);

    const ES_Native::GetCacheGroup *actual_group;
    if (entry.prototype_storage_type == group.storage_type)
        actual_group = &group;
    else
    {
        actual_group = ES_FindGroupByStorageType(groups, entry.prototype_storage_type);
        jump_when_finished = TRUE;
    }

#ifdef ARCHITECTURE_AMD64
    MovePointerIntoRegister(cg, entry.prototype, object);
    cg.MOV(OBJECT_MEMBER(object, ES_Object, properties), value, ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(entry.prototype) + ES_OFFSETOF(ES_Object, properties)), value, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

    if (!actual_group->single_offset && entry.prototype_offset)
        cg.ADD(ES_CodeGenerator::IMMEDIATE(entry.prototype_offset), value, ES_CodeGenerator::OPSIZE_POINTER);

    if (jump_when_finished)
        cg.Jump(copy_value[actual_group->storage_type]);
}

void
ES_Native::EmitNamedGet(VirtualRegister *target_vr, VirtualRegister *object_vr, const ES_Native::GetCacheAnalysis &analysis, BOOL for_inlined_function_call, BOOL fetch_value)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register object_class(ES_CodeGenerator::REG_CX);
    ES_CodeGenerator::Register value;

    const ES_Native::GetCacheGroup *groups = analysis.groups;
    unsigned groups_count = analysis.groups_count;
    const ES_Native::GetCacheNegative *negatives = analysis.negatives;
    unsigned negatives_count = analysis.negatives_count;

    /* Storage type indexed array of jump targets for jumping to the code that
       actually copies the value from a certain storage type.  Only set for
       those storage types we actually handle.  */
    ES_CodeGenerator::JumpTarget *copy_value[FIRST_SPECIAL_TYPE] = { NULL };

    BOOL load_properties = FALSE;
    unsigned positive_count = 0;

    for (unsigned group_index = 0; group_index < groups_count; ++group_index)
    {
        const GetCacheGroup &group = groups[group_index];

        if (!load_properties)
            for (unsigned entry_index = 0; !load_properties && entry_index < group.entries_count; ++entry_index)
                if (group.entries[entry_index].positive && ++positive_count > 1)
                    load_properties = TRUE;

        if (!copy_value[group.storage_type])
            copy_value[group.storage_type] = cg.ForwardJump();
    }

    if (property_value_read_vr)
        value = ES_CodeGenerator::REG_DX;
    else
        value = ES_CodeGenerator::REG_SI;

    ES_CodeGenerator::JumpTarget *jt_not_found = negatives_count ? cg.ForwardJump() : NULL;
    ES_CodeGenerator::JumpTarget *slow_case = current_slow_case->GetJumpTarget();

    LoadObjectOperand(object_vr->index, object);

    property_value_nr = NULL;

    cg.MOV(OBJECT_MEMBER(object, ES_Object, klass), object_class, ES_CodeGenerator::OPSIZE_POINTER);

    ES_CodeGenerator::JumpTarget *jt_finished = cg.ForwardJump();

    ES_CodeGenerator::Register class_id(ES_CodeGenerator::REG_DI);
    ES_CodeGenerator::Operand op_class_id;

    if (groups_count + negatives_count > 1 || groups_count == 1 && groups[0].entries_count > 1)
    {
        cg.MOV(OBJECT_MEMBER(object_class, ES_Class, class_id), class_id, ES_CodeGenerator::OPSIZE_32);
        op_class_id = ES_CodeGenerator::REGISTER(class_id);
    }
    else
        op_class_id = OBJECT_MEMBER(object_class, ES_Class, class_id);

    if (load_properties)
        cg.MOV(OBJECT_MEMBER(object, ES_Object, properties), value, ES_CodeGenerator::OPSIZE_POINTER);

    ES_CodeGenerator::OutOfOrderBlock *recover_from_failure = NULL;
    BOOL property_value_transfer = property_value_write_vr == target_vr;

    if (property_value_transfer)
    {
        SetPropertyValueTransferRegister(NR(IA32_REGISTER_INDEX_SI), property_value_write_offset, FALSE);

        recover_from_failure = RecoverFromFailedPropertyValueTransfer(this, target_vr, ES_CodeGenerator::REG_SI, FALSE);
    }

    /* Somewhat silly, but makes it easier to read the code below.  :-) */
    BOOL properties_loaded = load_properties;

    ES_CodeGenerator::JumpTarget *jt_next_group = NULL;

    for (unsigned group_index = 0; group_index < groups_count; ++group_index)
    {
        const GetCacheGroup &group = groups[group_index];
        BOOL last_group = group_index == groups_count - 1;
        BOOL last_group_with_entries = last_group || groups[group_index + 1].only_secondary_entries;

        if (!jt_next_group)
            jt_next_group = last_group_with_entries && !negatives_count ? slow_case : cg.ForwardJump();

        unsigned entry_index = 0;

        while (entry_index < group.entries_count)
        {
            const GetCacheGroup::Entry &entry = group.entries[entry_index];
            if (entry.prototype && entry.prototype_secondary_entry)
            {
                OP_ASSERT(!entry.positive && !entry.negative);
                ++entry_index;
            }
            else
                break;
        }

        for (; entry_index < group.entries_count; ++entry_index)
        {
            const GetCacheGroup::Entry &entry = group.entries[entry_index];

            BOOL last_entry = entry_index == group.entries_count - 1;
            ES_CodeGenerator::JumpTarget *jt_next_entry = NULL;

            cg.CMP(ES_CodeGenerator::IMMEDIATE(entry.class_id), op_class_id, ES_CodeGenerator::OPSIZE_32);

            BOOL need_positive_cache_hit_code = FALSE;
            BOOL need_negative_cache_hit_code = FALSE;
            BOOL need_limit_check = FALSE;

            if (entry.positive)
            {
                if (!properties_loaded)
                    /* Need to load properties pointer. */
                    need_positive_cache_hit_code = TRUE;

                if (!group.single_offset && entry.positive_offset)
                    /* Need to add offset to 'properties'. */
                    need_positive_cache_hit_code = TRUE;
            }

            if (entry.limit != UINT_MAX)
                /* Need limit check. */
                need_limit_check = TRUE;

            if (entry.prototype)
            {
                /* All secondary entries should precede any regular entries, so
                   the loop above should have skipped them all. */
                OP_ASSERT(!entry.prototype_secondary_entry);

                /* Need to fetch properties pointer from prototype object. */
                need_negative_cache_hit_code = TRUE;
            }

            if (!need_positive_cache_hit_code && !need_negative_cache_hit_code && !need_limit_check)
                /* The class ID check is all we need. */
                if (last_entry)
                    /* This is the last entry; jump to next group, or slow-case,
                       if class ID check failed, and fall through to the value
                       copying code if it succeeded. */
                    cg.Jump(jt_next_group, ES_NATIVE_CONDITION_NOT_EQUAL, jt_next_group == slow_case, FALSE);
                else
                    /* Otherwise jump to the value copying code if the class ID
                       check succeeded, and fall through to the next entry if it
                       failed. */
                    cg.Jump(copy_value[group.storage_type], ES_NATIVE_CONDITION_EQUAL);
            else
            {
                /* We need to execute one or more instructions after a
                   successful class ID check, so first jump to the next entry,
                   or next group (or slow-case,) if it failed. */
                if (last_entry)
                    cg.Jump(jt_next_group, ES_NATIVE_CONDITION_NOT_EQUAL, jt_next_group == slow_case, FALSE);
                else
                    cg.Jump(jt_next_entry = cg.ForwardJump(), ES_NATIVE_CONDITION_NOT_EQUAL);

                if (need_limit_check)
                {
                    /* Perform limit check. */
                    cg.CMP(ES_CodeGenerator::IMMEDIATE(entry.limit), OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);

                    if (!need_negative_cache_hit_code)
                    {
                        /* This cache either has no negative side at all, or
                           doesn't need to execute any instructions if the limit
                           check fails. */
                        ES_CodeGenerator::JumpTarget *jt_negative;
                        if (entry.negative)
                        {
                            /* Failed limit check => property doesn't exist. */
                            if (!jt_not_found)
                                jt_not_found = cg.ForwardJump();
                            jt_negative = jt_not_found;
                        }
                        else
                            /* Failed limit check => cache entry not valid, and
                               since we've done a class ID check, we know no
                               other cache entry is valid either. */
                            jt_negative = slow_case;

                        if (!need_positive_cache_hit_code && !last_entry)
                        {
                            cg.Jump(copy_value[group.storage_type], ES_NATIVE_CONDITION_ABOVE, jt_negative == slow_case, TRUE);
                            cg.Jump(jt_negative);
                        }
                        else
                        {
                            cg.Jump(jt_negative, ES_NATIVE_CONDITION_BELOW_EQUAL, jt_negative == slow_case, FALSE);
                            ES_EmitGetCachePositiveBranch(cg, group, entry, copy_value, object, value, properties_loaded, !last_entry);
                        }
                    }
                    else if (!need_positive_cache_hit_code)
                    {
                        /* This cache either has no positive side at all, or
                           doesn't need to execute any additional instructions
                           if the limit check succeeded, but do if the limit
                           check failed. */
                        ES_CodeGenerator::JumpTarget *jt_positive;
                        if (entry.positive)
                            /* Successful limit check => copy value (at single
                               offset, or zero offset.) */
                            jt_positive = copy_value[group.storage_type];
                        else
                            /* "Successful" limit check => cache entry not
                               valid, and since we've done a class ID check, we
                               know no other cache entry is valid either. */
                            jt_positive = slow_case;

                        cg.Jump(jt_positive, ES_NATIVE_CONDITION_ABOVE, jt_positive == slow_case, FALSE);

                        ES_EmitGetCacheNegativeBranch(cg, groups, group, entry, copy_value, object, value, !last_entry);
                    }
                    else
                    {
                        /* We need to execute some additional instruction both
                           on successful and failed limit check.  Generate jump
                           that skips the "negative branch" if the limit check
                           fails. */
                        ES_CodeGenerator::JumpTarget *jt_positive = cg.ForwardJump();
                        cg.Jump(jt_positive, ES_NATIVE_CONDITION_ABOVE);

                        ES_EmitGetCacheNegativeBranch(cg, groups, group, entry, copy_value, object, value, TRUE);

                        cg.SetJumpTarget(jt_positive);

                        ES_EmitGetCachePositiveBranch(cg, group, entry, copy_value, object, value, properties_loaded, !last_entry);
                    }
                }
                else
                {
                    /* No limit check needed, but some additional instructions
                       need to be executed. */

                    /* Since we'll have no limit check, we must have exactly one
                       branch of additional instructions. */
                    OP_ASSERT(need_positive_cache_hit_code != need_negative_cache_hit_code);

                    if (need_positive_cache_hit_code)
                        ES_EmitGetCachePositiveBranch(cg, group, entry, copy_value, object, value, properties_loaded, !last_entry);
                    else
                        ES_EmitGetCacheNegativeBranch(cg, groups, group, entry, copy_value, object, value, !last_entry);
                }

                if (jt_next_entry)
                    cg.SetJumpTarget(jt_next_entry);
            }
        }

        unsigned offset_immediate = group.single_offset ? group.common_offset : 0;

        cg.SetJumpTarget(copy_value[group.storage_type]);

        if (property_value_transfer)
        {
            if (group.storage_type == ES_STORAGE_WHATEVER)
            {
                cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), ES_CodeGenerator::MEMORY(value, offset_immediate + VALUE_TYPE_OFFSET), ES_CodeGenerator::OPSIZE_32);
                cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_EQUAL);

                offset_immediate += VALUE_VALUE_OFFSET;
            }
            else if (group.storage_type == ES_STORAGE_OBJECT_OR_NULL)
            {
                cg.CMP(ES_CodeGenerator::ZERO(), ES_CodeGenerator::MEMORY(value, offset_immediate), ES_CodeGenerator::OPSIZE_POINTER);
                cg.Jump(slow_case, ES_NATIVE_CONDITION_EQUAL);
            }

            if (value != ES_CodeGenerator::REG_SI)
                cg.MOV(value, ES_CodeGenerator::REG_SI, ES_CodeGenerator::OPSIZE_POINTER);

            if (offset_immediate != property_value_offset)
                cg.ADD(ES_CodeGenerator::IMMEDIATE(offset_immediate), ES_CodeGenerator::REG_SI, ES_CodeGenerator::OPSIZE_POINTER);
        }
        else if (fetch_value)
            CopyTypedDataToValue(cg, value, offset_immediate, group.storage_type, target_vr, object, object_class);

        if (!last_group || jt_not_found)
            cg.Jump(jt_finished);

        if (!last_group_with_entries)
        {
            cg.SetJumpTarget(jt_next_group);
            jt_next_group = NULL;
        }
    }

    if (negatives_count)
    {
        if (jt_next_group)
            cg.SetJumpTarget(jt_next_group);

        for (unsigned index = 0; index < negatives_count; ++index)
        {
            BOOL last_missing = index == negatives_count - 1;

            cg.CMP(ES_CodeGenerator::IMMEDIATE(negatives[index].class_id), op_class_id, ES_CodeGenerator::OPSIZE_32);

            if (negatives[index].limit == UINT_MAX)
                if (last_missing)
                    cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_EQUAL);
                else
                    cg.Jump(jt_not_found, ES_NATIVE_CONDITION_EQUAL);
            else
            {
                ES_CodeGenerator::JumpTarget *jt_next = NULL;

                if (last_missing)
                    jt_next = slow_case;
                else
                    jt_next = cg.ForwardJump();

                cg.Jump(jt_next, ES_NATIVE_CONDITION_NOT_EQUAL);

                cg.CMP(ES_CodeGenerator::IMMEDIATE(negatives[index].limit), OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);

                if (last_missing)
                    cg.Jump(slow_case, ES_NATIVE_CONDITION_ABOVE);
                else
                {
                    cg.Jump(jt_not_found, ES_NATIVE_CONDITION_BELOW_EQUAL);
                    cg.Jump(slow_case);

                    cg.SetJumpTarget(jt_next);
                }
            }
        }
    }

    if (jt_not_found)
    {
        cg.SetJumpTarget(jt_not_found);
        EmitSetRegisterType(target_vr, ESTYPE_UNDEFINED);
    }

    cg.SetJumpTarget(jt_finished);

    if (!property_value_nr)
        property_value_write_vr = NULL;

    if (recover_from_failure)
        cg.SetOutOfOrderContinuationPoint(recover_from_failure);
}

static void
ES_EmitPropertyArraySizeCheck(ES_CodeGenerator &cg, ES_CodeGenerator::Register properties, unsigned size, ES_CodeGenerator::Register scratch, ES_CodeGenerator::JumpTarget *failure)
{
    ES_DECLARE_NOTHING();

    /* Offset from 'properties' to the size part of ES_Header::header. */
    int objectsize_offset = -static_cast<int>(sizeof(ES_Header)) + 2;

    cg.MOVZX(ES_CodeGenerator::MEMORY(properties, objectsize_offset), scratch, ES_CodeGenerator::OPSIZE_16);

    if (size >= LARGE_OBJECT_LIMIT)
    {
        /* If the object's size is not 0xffff, then it's not allocated as a
           large object, meaning its size is smaller than LARGE_OBJECT_LIMIT and
           thus smaller than 'size'. */
        cg.CMP(ES_CodeGenerator::IMMEDIATE(0xffff), scratch, ES_CodeGenerator::OPSIZE_32);
        cg.Jump(failure, ES_NATIVE_CONDITION_NOT_EQUAL);

        /* Offset from 'properties' to ES_PageHeader::limit. */
        int pagelimit_offset = -static_cast<int>(sizeof(ES_Header)) - ES_PageHeader::HeaderSize() + ES_OFFSETOF(ES_PageHeader, limit);

        /* The object must be allocated as a large object, so calculate the
           page's actual size and use that as the size of the object. */
        cg.MOV(ES_CodeGenerator::MEMORY(properties, pagelimit_offset), scratch, ES_CodeGenerator::OPSIZE_POINTER);
        cg.SUB(properties, scratch, ES_CodeGenerator::OPSIZE_POINTER);
        cg.ADD(ES_CodeGenerator::IMMEDIATE(static_cast<int>(sizeof(ES_Header))), scratch, ES_CodeGenerator::OPSIZE_32);
    }

    /* Check that the object's size is at least 'size'. */
    cg.CMP(ES_CodeGenerator::IMMEDIATE(size), scratch, ES_CodeGenerator::OPSIZE_32);
    cg.Jump(failure, ES_NATIVE_CONDITION_LESS, TRUE, FALSE);
}

void
ES_Native::EmitNamedPut(VirtualRegister *object_vr, VirtualRegister *source_vr, JString *name, BOOL has_complex_cache, ES_StorageType source_type)
{
    DECLARE_NOTHING();

    const ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
    const ES_CodeGenerator::Register object_class(ES_CodeGenerator::REG_CX);
    const ES_CodeGenerator::Register properties(ES_CodeGenerator::REG_SI);

    ES_CodeWord *word = CurrentCodeWord();
    unsigned constant_cached_offset = 0;
    ES_StorageType constant_cached_type = ES_STORAGE_UNDEFINED;

    LoadObjectOperand(object_vr->index, object);

    ES_Code::PropertyCache *cache = &code->property_put_caches[word[4].index];

    OP_ASSERT(cache->class_id != ES_Class::NOT_CACHED_CLASS_ID);

    if (!current_slow_case)
        EmitSlowCaseCall();

    cg.MOV(OBJECT_MEMBER(object, ES_Object, properties), properties, ES_CodeGenerator::OPSIZE_POINTER);

    ES_CodeGenerator::JumpTarget *slow_case;

    if (!is_light_weight && property_value_read_vr && property_value_nr)
    {
        ES_CodeGenerator::OutOfOrderBlock *flush_object_vr = cg.StartOutOfOrderBlock();

        cg.MOV(object, REGISTER_VALUE(object_vr), ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(object_vr), ES_CodeGenerator::OPSIZE_32);

        cg.Jump(current_slow_case_main);
        cg.EndOutOfOrderBlock(FALSE);

        slow_case = flush_object_vr->GetJumpTarget();
    }
    else
        slow_case = current_slow_case->GetJumpTarget();

    cg.MOV(OBJECT_MEMBER(object, ES_Object, klass), object_class, ES_CodeGenerator::OPSIZE_POINTER);

    while (!ES_Code::IsSimplePropertyCache(cache, FALSE))
        cache = cache->next;

    OP_ASSERT(cache);

    ES_CodeGenerator::JumpTarget *null_target = NULL, *int_to_double_target = NULL;

    if (has_complex_cache)
    {
        ES_CodeGenerator::JumpTarget *size_4_target = NULL, *size_8_target = NULL, *size_16_target = NULL;
        const ES_CodeGenerator::Register source(ES_CodeGenerator::REG_DX);

        cg.LEA(VR_WITH_INDEX(source_vr->index), source, ES_CodeGenerator::OPSIZE_POINTER);

        unsigned cache_size = 0;
        ES_CodeGenerator::JumpTarget *new_class_slow_case = NULL;

        while (TRUE)
        {
            ES_Code::PropertyCache *next_cache = cache_size++ < MAX_PROPERTY_CACHE_SIZE ? cache->next : NULL;

            while (next_cache && (!ES_Code::IsSimplePropertyCache(next_cache, FALSE) || cache->class_id == next_cache->class_id))
                next_cache = next_cache->next;

            unsigned current_id = cache->class_id;

            ES_CodeGenerator::JumpTarget *jt_next_unpaired = next_cache ? cg.ForwardJump() : slow_case;

            cg.CMP(ES_CodeGenerator::IMMEDIATE(cache->class_id), OBJECT_MEMBER(object_class, ES_Class, class_id), ES_CodeGenerator::OPSIZE_32);
            cg.Jump(jt_next_unpaired, ES_NATIVE_CONDITION_NOT_EQUAL);

            ES_CodeGenerator::JumpTarget *jt_next = NULL;

            while (cache->class_id == current_id)
            {
                ES_StorageType type = cache->GetStorageType();
                ES_CodeGenerator::JumpTarget *current_slow_case = slow_case;
                jt_next = cache->next != next_cache ? cg.ForwardJump() : NULL;

                BOOL needs_limit_check = cache->object_class->NeedLimitCheck(cache->GetLimit(), cache->data.new_class != NULL);
                unsigned size = ES_Layout_Info::SizeOf(type);

                if (needs_limit_check)
                {
                    cg.CMP(ES_CodeGenerator::IMMEDIATE(cache->GetLimit()), OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);

                    /* The cache limit checks are as follows:

                    In both cases the stored cache limit is the index that we wrote the property to originally.

                    For the case where cache->data.new_class is not NULL, we are appending a new property
                    which means that the stored index must at this point be equal to the property count, and
                    if it isn't the cache is invalid.

                    If cache->data.new_class is NULL we are writing into an object, and thus for the cache to
                    be valid, the cache limit must be less than the property count, i.e. if the property count
                    is less than or equal to the cache limit the cache is invalid.
                    */

                    if (cache->data.new_class)
                        cg.Jump(jt_next ? jt_next : slow_case, ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);
                    else
                        cg.Jump(jt_next ? jt_next : slow_case, ES_NATIVE_CONDITION_LESS_EQUAL, TRUE, FALSE);
                }

                if (cache->data.new_class)
                {
                    OP_ASSERT(!cache->data.new_class->HasHashTableProperties());

                    ES_CodeGenerator::Register scratch(ES_CodeGenerator::REG_DI);

                    ES_EmitPropertyArraySizeCheck(cg, properties, cache->GetOffset() + size + sizeof(ES_Header), scratch, slow_case);

                    if (cache->data.new_class != cache->object_class)
                        MovePointerInto(cg, cache->data.new_class, OBJECT_MEMBER(object, ES_Object, klass), scratch);
                    cg.ADD(ES_CodeGenerator::IMMEDIATE(1), OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);

                    if (!new_class_slow_case)
                    {
                        ES_CodeGenerator::OutOfOrderBlock *revert_put_cached_new = cg.StartOutOfOrderBlock();
                        cg.MOV(object_class, OBJECT_MEMBER(object, ES_Object, klass), ES_CodeGenerator::OPSIZE_POINTER);
                        cg.SUB(ES_CodeGenerator::IMMEDIATE(1), OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);
                        cg.Jump(slow_case);
                        cg.EndOutOfOrderBlock(FALSE);
                        new_class_slow_case = revert_put_cached_new->GetJumpTarget();
                    }

                    current_slow_case = new_class_slow_case;
                }

                if (cache->GetOffset() != 0)
                    cg.ADD(ES_CodeGenerator::IMMEDIATE(cache->GetOffset()), properties, ES_CodeGenerator::OPSIZE_POINTER);

                BOOL skip_write = FALSE;

                if (type != ES_STORAGE_WHATEVER && source_type != type)
                    skip_write = EmitTypeConversionCheck(type, source_type, source_vr, current_slow_case, null_target, int_to_double_target);

                if (type == ES_STORAGE_NULL)
                {
                    if (!null_target)
                        null_target = cg.ForwardJump();

                    cg.Jump(null_target);
                }
                else if (!skip_write)
                {
#ifndef ES_VALUES_AS_NANS
                    if (type != ES_STORAGE_WHATEVER)
                        cg.ADD(ES_CodeGenerator::IMMEDIATE(VALUE_VALUE_OFFSET), source, ES_CodeGenerator::OPSIZE_POINTER);
#endif
                    JumpToSize(cg, size, size_4_target, size_8_target, size_16_target);
                }

                if (jt_next)
                    cg.SetJumpTarget(jt_next);

                if (cache->next)
                    cache = cache->next;
                else
                {
                    OP_ASSERT(next_cache == NULL);
                    break;
                }
            }

            if (next_cache)
            {
                cg.SetJumpTarget(jt_next_unpaired);
                cache = next_cache;
            }
            else
                break;
        }

        if (size_4_target || size_8_target || size_16_target)
            CopyValueToData2(cg, source, properties, constant_cached_offset, size_4_target, size_8_target, size_16_target, object, FALSE);
    }
    else
    {
        cg.CMP(ES_CodeGenerator::IMMEDIATE(cache->class_id), OBJECT_MEMBER(object_class, ES_Class, class_id), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);

        constant_cached_offset = cache->GetOffset();
        constant_cached_type = cache->GetStorageType();

        if (cache->data.new_class)
        {
            OP_ASSERT(!cache->data.new_class->HasHashTableProperties());
            if (cache->object_class->NeedLimitCheck(cache->GetLimit(), TRUE))
            {
                cg.CMP(ES_CodeGenerator::IMMEDIATE(cache->GetLimit()), OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);
                cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);
            }

            ES_CodeGenerator::Register scratch(ES_CodeGenerator::REG_DI);

            ES_EmitPropertyArraySizeCheck(cg, properties, constant_cached_offset + ES_Layout_Info::SizeOf(constant_cached_type) + sizeof(ES_Header), scratch, slow_case);

            if (cache->data.new_class != cache->object_class)
                MovePointerInto(cg, cache->data.new_class, OBJECT_MEMBER(object, ES_Object, klass), scratch);
            cg.ADD(ES_CodeGenerator::IMMEDIATE(1), OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);

            ES_CodeGenerator::OutOfOrderBlock *revert_put_cached_new = cg.StartOutOfOrderBlock();
            if (cache->data.new_class != cache->object_class)
                cg.MOV(object_class, OBJECT_MEMBER(object, ES_Object, klass), ES_CodeGenerator::OPSIZE_POINTER);
            cg.SUB(ES_CodeGenerator::IMMEDIATE(1), OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);
            cg.Jump(slow_case);
            cg.EndOutOfOrderBlock(FALSE);
            slow_case = revert_put_cached_new->GetJumpTarget();
        }
        else if (cache->object_class->NeedLimitCheck(cache->GetLimit(), FALSE))
        {
            cg.CMP(ES_CodeGenerator::IMMEDIATE(cache->GetLimit()), OBJECT_MEMBER(object, ES_Object, property_count), ES_CodeGenerator::OPSIZE_32);
            cg.Jump(slow_case, ES_NATIVE_CONDITION_LESS_EQUAL, TRUE, FALSE);
        }

        BOOL skip_write = FALSE;

        if (constant_cached_type != ES_STORAGE_WHATEVER && source_type != constant_cached_type)
            skip_write = EmitTypeConversionCheck(constant_cached_type, source_type, source_vr, slow_case, null_target, int_to_double_target);

        if (!skip_write)
            CopyValueToTypedData(cg, source_vr, properties, constant_cached_offset, constant_cached_type, object, object_class);
    }

    EmitTypeConversionHandlers(source_vr, properties, constant_cached_offset, null_target, int_to_double_target);
}

void
ES_Native::EmitLengthGet(VirtualRegister *target_vr, VirtualRegister *object_vr, unsigned handled, unsigned possible, ES_CodeGenerator::JumpTarget *slow_case)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::OutOfOrderBlock *handle_object = NULL;

    if (handled & ESTYPE_BITS_OBJECT)
    {
        if (!slow_case)
        {
            if (!current_slow_case)
                EmitSlowCaseCall();
            slow_case = current_slow_case->GetJumpTarget();
        }

        if (handled & ESTYPE_BITS_STRING)
            handle_object = cg.StartOutOfOrderBlock();

        if (handled != possible)
            EmitRegisterTypeCheck(object_vr, ESTYPE_OBJECT, slow_case);

        ES_CodeGenerator::Register array(ES_CodeGenerator::REG_AX);
        ES_CodeGenerator::Register klass(ES_CodeGenerator::REG_DX);
        ES_CodeGenerator::Register properties(ES_CodeGenerator::REG_SI);

        LoadObjectOperand(object_vr->index, array);

        cg.MOV(OBJECT_MEMBER(array, ES_Object, klass), klass, ES_CodeGenerator::OPSIZE_POINTER);
        cg.CMP(ES_CodeGenerator::IMMEDIATE(code->global_object->GetArrayClass()->GetId(context)), OBJECT_MEMBER(klass, ES_Class, class_id), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);
        cg.MOV(OBJECT_MEMBER(array, ES_Object, properties), properties, ES_CodeGenerator::OPSIZE_POINTER);

        CopyTypedDataToValue(cg, properties, 0, ES_STORAGE_INT32, target_vr, ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_DX);

        if (!handle_object)
            return;

        cg.EndOutOfOrderBlock();
        slow_case = handle_object->GetJumpTarget();
    }

    if (possible != ESTYPE_BITS_STRING)
    {
        if (!slow_case)
        {
            if (!current_slow_case)
                EmitSlowCaseCall();
            slow_case = current_slow_case->GetJumpTarget();
        }

        EmitRegisterTypeCheck(object_vr, ESTYPE_STRING, slow_case);
    }

    ES_CodeGenerator::Register string(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register length(ES_CodeGenerator::REG_DX);

    LoadObjectOperand(object_vr->index, string);

    cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_INT32), REGISTER_TYPE(target_vr), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(OBJECT_MEMBER(string, JString, length), length, ES_CodeGenerator::OPSIZE_32);
    cg.MOV(length, REGISTER_VALUE(target_vr), ES_CodeGenerator::OPSIZE_32);

    if (handle_object)
        cg.SetOutOfOrderContinuationPoint(handle_object);
}

void
ES_Native::EmitFetchPropertyValue(VirtualRegister *target_vr, VirtualRegister *object_vr, ES_Object *static_object, ES_Class *klass, unsigned property_index)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
    ES_CodeGenerator::Register properties(ES_CodeGenerator::REG_SI);

    if (object_vr)
        cg.MOV(REGISTER_VALUE(object_vr), object, ES_CodeGenerator::OPSIZE_POINTER);
    else
        MovePointerIntoRegister(cg, static_object, object);

    cg.MOV(OBJECT_MEMBER(object, ES_Object, properties), properties, ES_CodeGenerator::OPSIZE_POINTER);

    ES_Layout_Info layout = klass->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(property_index));

    ES_StorageType storage_type = layout.GetStorageType();
    if (property_value_write_vr && (storage_type == ES_STORAGE_OBJECT || storage_type == ES_STORAGE_WHATEVER))
    {
        OP_ASSERT(property_value_write_vr == target_vr);

        SetPropertyValueTransferRegister(NR(IA32_REGISTER_INDEX_SI), layout.GetOffset(), storage_type != ES_STORAGE_OBJECT);
    }
    else
        CopyTypedDataToValue(cg, properties, layout.GetOffset(), storage_type, target_vr, ES_CodeGenerator::REG_DI, ES_CodeGenerator::REG_SI);
}

void
ES_Native::EmitGlobalGet(VirtualRegister *target_vr, unsigned class_id, unsigned cached_offset, unsigned cached_type)
{
    DECLARE_NOTHING();

    if (class_id != ES_Class::NOT_CACHED_CLASS_ID)
    {
        if (!current_slow_case)
            EmitSlowCaseCall();

        ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
        ES_CodeGenerator::Register object_class(ES_CodeGenerator::REG_CX);
        ES_CodeGenerator::Register properties(ES_CodeGenerator::REG_SI);
        ES_StorageType type = ES_Value_Internal::StorageTypeFromCachedTypeBits(cached_type);

#ifdef ARCHITECTURE_AMD64
        cg.MOV(OBJECT_MEMBER(CODE_POINTER, ES_Code, global_object), object, ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
        cg.MOV(ES_CodeGenerator::IMMEDIATE(reinterpret_cast<UINTPTR>(code->global_object)), object, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

        cg.MOV(OBJECT_MEMBER(object, ES_Object, klass), object_class, ES_CodeGenerator::OPSIZE_POINTER);
        cg.CMP(ES_CodeGenerator::IMMEDIATE(class_id), OBJECT_MEMBER(object_class, ES_Class, class_id), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL);
        cg.MOV(OBJECT_MEMBER(object, ES_Object, properties), properties, ES_CodeGenerator::OPSIZE_POINTER);

        if (property_value_write_vr && (type == ES_STORAGE_OBJECT || type == ES_STORAGE_WHATEVER))
        {
            OP_ASSERT(property_value_write_vr == target_vr);

            SetPropertyValueTransferRegister(NR(IA32_REGISTER_INDEX_SI), cached_offset, type != ES_STORAGE_OBJECT);

            if (current_slow_case)
            {
                ES_CodeGenerator::OutOfOrderBlock *recover_from_failure = RecoverFromFailedPropertyValueTransfer(this, target_vr, ES_CodeGenerator::REG_SI, type == ES_STORAGE_WHATEVER);

                if (recover_from_failure)
                    cg.SetOutOfOrderContinuationPoint(recover_from_failure);
            }
        }
        else
            CopyTypedDataToValue(cg, properties, cached_offset, type, target_vr, object, object_class);
    }
    else
        EmitInstructionHandlerCall();

    if (!property_value_nr)
        property_value_write_vr = NULL;
}

void
ES_Native::EmitGlobalPut(VirtualRegister *source_vr, unsigned class_id, unsigned cached_offset, unsigned cached_type, ES_StorageType source_type)
{
    DECLARE_NOTHING();

    if (class_id != ES_Class::NOT_CACHED_CLASS_ID)
    {
        if (!current_slow_case)
            EmitSlowCaseCall();

        ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
        ES_CodeGenerator::Register object_class(ES_CodeGenerator::REG_CX);
        ES_CodeGenerator::Register properties(ES_CodeGenerator::REG_SI);
        ES_StorageType type = ES_Value_Internal::StorageTypeFromCachedTypeBits(cached_type);

#ifdef ARCHITECTURE_AMD64
        cg.MOV(OBJECT_MEMBER(CODE_POINTER, ES_Code, global_object), object, ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
        cg.MOV(ES_CodeGenerator::IMMEDIATE(reinterpret_cast<UINTPTR>(code->global_object)), object, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

        ES_CodeGenerator::JumpTarget *null_target = NULL, *int_to_double_target = NULL, *slow_case = current_slow_case->GetJumpTarget();
        cg.MOV(OBJECT_MEMBER(object, ES_Object, klass), object_class, ES_CodeGenerator::OPSIZE_POINTER);
        cg.CMP(ES_CodeGenerator::IMMEDIATE(class_id), OBJECT_MEMBER(object_class, ES_Class, class_id), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(slow_case, ES_NATIVE_CONDITION_NOT_EQUAL);
        cg.MOV(OBJECT_MEMBER(object, ES_Object, properties), properties, ES_CodeGenerator::OPSIZE_POINTER);

        BOOL skip_write = FALSE;
        if (type != ES_STORAGE_WHATEVER && source_type != type)
            skip_write = EmitTypeConversionCheck(type, source_type, source_vr, slow_case, null_target, int_to_double_target);

        if (!skip_write)
            CopyValueToTypedData(cg, source_vr, properties, cached_offset, type, object, object_class);

        EmitTypeConversionHandlers(source_vr, properties, cached_offset, null_target, int_to_double_target);
    }
    else
        EmitInstructionHandlerCall();
}

static void
LoadVariableValues(ES_CodeGenerator &cg, ES_Value_Internal **variable_values, ES_CodeGenerator::Register r)
{
#ifdef ARCHITECTURE_AMD64
    if (reinterpret_cast<UINTPTR>(variable_values) <= INT_MAX)
        cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, static_cast<int>(reinterpret_cast<UINTPTR>(variable_values))), r, ES_CodeGenerator::OPSIZE_POINTER);
    else
    {
        MovePointerIntoRegister(cg, variable_values, r);
        cg.MOV(ES_CodeGenerator::MEMORY(r), r, ES_CodeGenerator::OPSIZE_POINTER);
    }
#else // ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::MEMORY(variable_values), r, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64
}

void
ES_Native::EmitGlobalImmediateGet(VirtualRegister *target, unsigned index, BOOL for_inlined_function_call, BOOL fetch_value)
{
    if (for_inlined_function_call)
    {
        DECLARE_NOTHING();

        ES_CodeGenerator::OutOfOrderBlock *failed_inlined_function = cg.StartOutOfOrderBlock();

#ifdef ARCHITECTURE_AMD64
        cg.MOV(ES_CodeGenerator::IMMEDIATE(1), OBJECT_MEMBER(CODE_POINTER, ES_Code, has_failed_inlined_function), ES_CodeGenerator::OPSIZE_32);
#else // ARCHITECTURE_AMD64
        cg.MOV(ES_CodeGenerator::IMMEDIATE(1), ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(code) + ES_OFFSETOF(ES_Code, has_failed_inlined_function)), ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64

        EmitInstructionHandlerCall();

        cg.EndOutOfOrderBlock(FALSE);

#ifdef ARCHITECTURE_AMD64
        MovePointerIntoRegister(cg, code->global_object, ES_CodeGenerator::REG_AX);
        cg.CMP(ES_CodeGenerator::IMMEDIATE(code->global_object->immediate_function_serial_nr), OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Global_Object, immediate_function_serial_nr), ES_CodeGenerator::OPSIZE_32);
#else // ARCHITECTURE_AMD64
        cg.CMP(ES_CodeGenerator::IMMEDIATE(code->global_object->immediate_function_serial_nr), ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(code->global_object) + ES_OFFSETOF(ES_Global_Object, immediate_function_serial_nr)), ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64

        cg.Jump(failed_inlined_function->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);
    }

    if (fetch_value)
    {
        LoadVariableValues(cg, &code->global_object->variable_values, ES_CodeGenerator::REG_AX);
        CopyValue(cg, ES_CodeGenerator::REG_AX, VALUE_INDEX_TO_OFFSET(index), target, ES_CodeGenerator::REG_CX, ES_CodeGenerator::REG_DX);
    }
}

void
ES_Native::EmitGlobalImmediatePut(unsigned index, VirtualRegister *source)
{
    LoadVariableValues(cg, &code->global_object->variable_values, ES_CodeGenerator::REG_AX);
    CopyValue(cg, source, ES_CodeGenerator::REG_AX, VALUE_INDEX_TO_OFFSET(index), ES_CodeGenerator::REG_CX, ES_CodeGenerator::REG_DX);
}

void
ES_Native::EmitLexicalGet(VirtualRegister *target_vr, VirtualRegister *function_vr, unsigned scope_index, unsigned variable_index)
{
    DECLARE_NOTHING();

    if (!current_slow_case)
        EmitSlowCaseCall();

    const ES_CodeGenerator::Register function(ES_CodeGenerator::REG_AX), variables(ES_CodeGenerator::REG_AX), properties(ES_CodeGenerator::REG_AX);

    cg.MOV(REGISTER_VALUE(function_vr), function, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(ES_CodeGenerator::MEMORY(function, ES_OFFSETOF(ES_Function, scope_chain) + scope_index * sizeof(ES_Object *)), variables, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(OBJECT_MEMBER(variables, ES_Object, properties), properties, ES_CodeGenerator::OPSIZE_POINTER);

    /* Since ES_Value_Internal is always aligned to an 8 byte boundary,
       and property storage starts at a 4 byte, but not a 8 byte boundary
       and the first value is located at +4, add 4 to the offset. */
    cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_BOXED), ES_CodeGenerator::MEMORY(properties, VALUE_INDEX_TO_OFFSET(variable_index) + VALUE_TYPE_OFFSET + 4), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_EQUAL, TRUE, FALSE);

    CopyValue(cg, properties, VALUE_INDEX_TO_OFFSET(variable_index) + 4, target_vr, ES_CodeGenerator::REG_CX, ES_CodeGenerator::REG_DX);
}

void
ES_Native::EmitFormatString(VirtualRegister *target, VirtualRegister *source, unsigned cache_index)
{
    if (!current_slow_case)
        EmitSlowCaseCall();

#ifdef ARCHITECTURE_AMD64
    cg.MovePointerIntoRegister(&code->format_string_caches[cache_index].from, ES_CodeGenerator::REG_AX);
    cg.MovePointerIntoRegister(&code->format_string_caches[cache_index].to, ES_CodeGenerator::REG_DX);
    cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_AX), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_DX), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_POINTER);
#else // ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::MEMORY(&code->format_string_caches[cache_index].from), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
    cg.MOV(ES_CodeGenerator::MEMORY(&code->format_string_caches[cache_index].to), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

    cg.CMP(ES_CodeGenerator::REG_AX, REGISTER_VALUE(source), ES_CodeGenerator::OPSIZE_POINTER);
    cg.Jump(current_slow_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL);

    cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_STRING), REGISTER_TYPE(target), ES_CodeGenerator::OPSIZE_32);
    cg.MOV(ES_CodeGenerator::REG_DX, REGISTER_VALUE(target), ES_CodeGenerator::OPSIZE_POINTER);
}

void
ES_Native::EmitTableSwitch(VirtualRegister *value, int minimum, int maximum, ES_CodeGenerator::Constant *table, ES_CodeGenerator::JumpTarget *default_target)
{
    DECLARE_NOTHING();
    ES_ValueType known_type;

    if (!GetType(value, known_type))
        known_type = ESTYPE_UNDEFINED;
    else if (known_type != ESTYPE_INT32 && known_type != ESTYPE_DOUBLE && known_type != ESTYPE_INT32_OR_DOUBLE)
    {
        cg.Jump(default_target);
        return;
    }

    if (known_type != ESTYPE_INT32)
    {
        ES_CodeGenerator::OutOfOrderBlock *convert_double = NULL;
        if (known_type != ESTYPE_DOUBLE)
            convert_double = cg.StartOutOfOrderBlock();

#ifdef ARCHITECTURE_AMD64_UNIX
        ES_CodeGenerator::Register r(ES_CodeGenerator::REG_SI);
#elif defined(ARCHITECTURE_AMD64_WINDOWS)
        ES_CodeGenerator::Register r(ES_CodeGenerator::REG_DX);
#else // ARCHITECTURE_AMD64
        ES_CodeGenerator::Register r(ES_CodeGenerator::REG_DX);
#endif // ARCHITECTURE_AMD64

        cg.LEA(VALUE_INDEX_MEMORY(REGISTER_FRAME_POINTER, value->index), r, ES_CodeGenerator::OPSIZE_POINTER);

        IA32FUNCTIONCALL("pp", ES_Native::GetDoubleAsSwitchValue);

        call.AllocateStackSpace();
        call.AddArgument(CONTEXT_POINTER);
        call.AddArgument(r);
        call.Call();
        call.FreeStackSpace();

        cg.CMP(ES_CodeGenerator::ZERO(), OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, implicit_bool), ES_CodeGenerator::OPSIZE_32);
        cg.Jump(default_target, ES_NATIVE_CONDITION_NOT_EQUAL);
        if (convert_double)
        {
            cg.EndOutOfOrderBlock();

#ifdef ES_VALUES_AS_NANS
            cg.CMP(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), REGISTER_TYPE(value), ES_CodeGenerator::OPSIZE_32);
            cg.Jump(convert_double->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_GREATER);
#else // ES_VALUES_AS_NANS
            cg.TEST(ES_CodeGenerator::IMMEDIATE(ESTYPE_DOUBLE), REGISTER_TYPE(value), ES_CodeGenerator::OPSIZE_32);
            cg.Jump(convert_double->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_ZERO);
#endif // ES_VALUES_AS_NANS

            if (known_type != ESTYPE_INT32_OR_DOUBLE)
                EmitRegisterTypeCheck(value, ESTYPE_INT32, default_target);

            cg.MOV(REGISTER_VALUE(value), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);

            cg.SetOutOfOrderContinuationPoint(convert_double);
        }
    }
    else
        cg.MOV(REGISTER_VALUE(value), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);

    cg.LEA(ES_CodeGenerator::CONSTANT(table), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_POINTER);

    if (minimum != 0)
    {
        cg.SUB(ES_CodeGenerator::IMMEDIATE(minimum), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
        cg.Jump(default_target, ES_NATIVE_CONDITION_SIGN);
        cg.Jump(default_target, ES_NATIVE_CONDITION_OVERFLOW);
    }
    else
    {
        cg.CMP(ES_CodeGenerator::ZERO(), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
        cg.Jump(default_target, ES_NATIVE_CONDITION_LESS);
    }

    cg.CMP(ES_CodeGenerator::IMMEDIATE(maximum - minimum), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_32);
    cg.Jump(default_target, ES_NATIVE_CONDITION_GREATER);

    cg.JMP(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_DX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::SCALE_POINTER));
}

ES_CodeGenerator::JumpTarget *
ES_Native::EmitPreConditionsStart(unsigned cw_index)
{
    DECLARE_NOTHING();

    ES_CodeGenerator::OutOfOrderBlock *slow_case = cg.StartOutOfOrderBlock();

#ifdef ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::IMMEDIATE(1), OBJECT_MEMBER(CODE_POINTER, ES_Code, has_failed_preconditions), ES_CodeGenerator::OPSIZE_32);
#else // ARCHITECTURE_AMD64
    cg.MOV(ES_CodeGenerator::IMMEDIATE(1), ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_NONE, reinterpret_cast<UINTPTR>(code) + ES_OFFSETOF(ES_Code, has_failed_preconditions)), ES_CodeGenerator::OPSIZE_32);
#endif // ARCHITECTURE_AMD64

    IA32FUNCTIONCALL("pu", ES_Execution_Context::ForceUpdateNativeDispatcher);

    call.AllocateStackSpace();
    call.AddArgument(CONTEXT_POINTER);
    call.AddArgument(ES_CodeGenerator::IMMEDIATE(cw_index + (constructor ? code->data->codewords_count : 0)));
    call.Call();
    call.FreeStackSpace();

    cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_AX), ES_CodeGenerator::REG_DX, ES_CodeGenerator::OPSIZE_32);

    cg.JMP(ES_CodeGenerator::REG_AX);
    cg.EndOutOfOrderBlock(FALSE);

    return slow_case->GetJumpTarget();
}

void
ES_Native::EmitArithmeticBlockSlowPath(ArithmeticBlock *arithmetic_block)
{
    DECLARE_NOTHING();

    unsigned end_instruction_index = arithmetic_block->end_instruction_index;
    unsigned last_instruction_index = end_instruction_index - 1;
    unsigned last_cw_index = code->data->instruction_offsets[last_instruction_index];

    switch (code->data->codewords[last_cw_index].instruction)
    {
    case ESI_RETURN_VALUE:
    case ESI_JUMP_IF_TRUE:
    case ESI_JUMP_IF_FALSE:
        end_instruction_index = last_instruction_index;
    }

    unsigned end_cw_index = code->data->instruction_offsets[end_instruction_index];

    arithmetic_block->slow_case = current_slow_case = cg.StartOutOfOrderBlock();

    if (!is_light_weight)
        EmitExecuteBytecode(arithmetic_block->start_instruction_index, end_instruction_index, end_cw_index != last_cw_index);

    if (end_cw_index == last_cw_index)
    {
        if (end_cw_index == entry_point_cw_index)
            entry_point_jump_target = EmitEntryPoint();

        switch (code->data->codewords[last_cw_index].instruction)
        {
        case ESI_RETURN_VALUE:
            cw_index = last_cw_index;

            EmitRegisterCopy(VR(code->data->codewords[last_cw_index + 1].index), VR(0));
            cg.Jump(epilogue_jump_target);

            cg.EndOutOfOrderBlock(FALSE);
            break;

        case ESI_JUMP_IF_TRUE:
        case ESI_JUMP_IF_FALSE:
            ES_CodeGenerator::JumpTarget *true_target = NULL, *false_target = NULL;
            Operand value_target;

            GetConditionalTargets(last_cw_index, value_target, true_target, false_target);

            cg.CMP(ES_CodeGenerator::ZERO(), OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, implicit_bool), ES_CodeGenerator::OPSIZE_32);

            if (true_target)
                cg.Jump(true_target, ES_NATIVE_CONDITION_NOT_ZERO);
            else
                cg.Jump(false_target, ES_NATIVE_CONDITION_ZERO);

            cg.EndOutOfOrderBlock();
        }
    }
    else
        cg.EndOutOfOrderBlock();
}

ES_CodeGenerator::JumpTarget *
ES_Native::EmitEntryPoint(BOOL custom_light_weight_entry)
{
    if (is_light_weight)
    {
        DECLARE_NOTHING();

        if (entry_point_cw_index == 0 && prologue_entry_point)
            return NULL;

        ES_CodeGenerator::JumpTarget *real_entry_point = NULL;
        ES_CodeGenerator::OutOfOrderBlock *ooo = NULL;

        if (!custom_light_weight_entry)
        {
            real_entry_point = cg.Here();
            ooo = cg.StartOutOfOrderBlock();
        }

        cg.ADD(ES_CodeGenerator::IMMEDIATE(StackSpaceAllocated() - sizeof(void *)), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
        cg.POP(ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(ES_CodeGenerator::REG_AX, OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, native_stack_frame), ES_CodeGenerator::OPSIZE_POINTER);

        int align_adjust = ES_STACK_ALIGNMENT - sizeof(void *);
        if (align_adjust)
            cg.SUB(ES_CodeGenerator::IMMEDIATE(align_adjust), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);

        if (!custom_light_weight_entry)
        {
            cg.Jump(real_entry_point);
            cg.EndOutOfOrderBlock(FALSE);

            return ooo->GetJumpTarget();
        }
        else
            return NULL;
    }
#ifdef ES_STACK_RECONSTRUCTION_SUPPORTED
#ifdef ARCHITECTURE_AMD64
    else if (prologue_entry_point)
    {
        ES_CodeGenerator::JumpTarget *real_entry_point = cg.Here();
        ES_CodeGenerator::OutOfOrderBlock *ooo = cg.StartOutOfOrderBlock();

        MovePointerIntoRegister(cg, code, CODE_POINTER);

        cg.Jump(real_entry_point);
        cg.EndOutOfOrderBlock(FALSE);

        return ooo->GetJumpTarget();
    }
#endif // ARCHITECTURE_AMD64
#else // ES_STACK_RECONSTRUCTION_SUPPORTED
    else if (prologue_entry_point)
    {
        ES_CodeGenerator::JumpTarget *real_entry_point = cg.Here();
        ES_CodeGenerator::OutOfOrderBlock *ooo = cg.StartOutOfOrderBlock();

        BOOL has_variable_object;

        SetupNativeStackFrame(cg, code, stack_space_allocated, has_variable_object, TRUE);

        cg.Jump(real_entry_point);
        cg.EndOutOfOrderBlock(FALSE);

        return ooo->GetJumpTarget();
    }
#endif // ES_STACK_RECONSTRUCTION_SUPPORTED
    else
        return cg.Here();
}

ES_CodeGenerator::JumpTarget *
ES_Native::EmitInlineResumption(unsigned class_id, VirtualRegister *object_vr, ES_CodeGenerator::JumpTarget *failure, unsigned char *mark_failure)
{
    DECLARE_NOTHING();

    entry_point_jump_target = EmitEntryPoint(FALSE);
    ES_CodeGenerator::OutOfOrderBlock *extra_check = cg.StartOutOfOrderBlock();

    ES_CodeGenerator::Register scratch(ES_CodeGenerator::REG_DI);
    LoadObjectOperand(object_vr->index, scratch);

    cg.MOV(OBJECT_MEMBER(scratch, ES_Object, klass), scratch, ES_CodeGenerator::OPSIZE_POINTER);
    cg.CMP(ES_CodeGenerator::IMMEDIATE(class_id), OBJECT_MEMBER(scratch, ES_Class, class_id), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(entry_point_jump_target, ES_NATIVE_CONDITION_EQUAL, TRUE, FALSE);

    /* Update profile data to indicate that the instruction failed inlining. */
#ifdef ARCHITECTURE_AMD64
    MovePointerIntoRegister(cg, mark_failure, scratch);
    cg.OR(ES_CodeGenerator::IMMEDIATE(ES_CodeStatic::GET_FAILED_INLINE), ES_CodeGenerator::MEMORY(scratch), ES_CodeGenerator::OPSIZE_8);
#else
    cg.OR(ES_CodeGenerator::IMMEDIATE(ES_CodeStatic::GET_FAILED_INLINE), ES_CodeGenerator::MEMORY((void*)mark_failure), ES_CodeGenerator::OPSIZE_8);
#endif // ARCHITECTURE_AMD64
    cg.Jump(failure);

    cg.EndOutOfOrderBlock(FALSE);
    return extra_check->GetJumpTarget();
}

void
ES_Native::EmitTick()
{
    DECLARE_NOTHING();

    ES_CodeGenerator::OutOfOrderBlock *check = cg.StartOutOfOrderBlock();

    IA32FUNCTIONCALL("p", ES_Execution_Context::CheckOutOfTimeFromMachineCode);
    call.AllocateStackSpace();
    call.AddArgument(CONTEXT_POINTER);
    call.Call();
    call.FreeStackSpace();

    cg.EndOutOfOrderBlock();
    cg.SUB(ES_CodeGenerator::IMMEDIATE(1), OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, time_until_check), ES_CodeGenerator::OPSIZE_32);
    cg.Jump(check->GetJumpTarget(), ES_NATIVE_CONDITION_ZERO, TRUE, FALSE);
    cg.SetOutOfOrderContinuationPoint(check);
}

unsigned
ES_ArchitecureMixin::StackSpaceAllocated()
{
    ES_Native *self = static_cast<ES_Native *>(this);

    unsigned stack_space_allocated = 3 * sizeof(void *);

    if (self->code->type == ES_Code::TYPE_FUNCTION)
    {
        stack_space_allocated += sizeof(void *);

        if (self->code->CanHaveVariableObject())
            stack_space_allocated += sizeof(void *);

        stack_space_allocated += sizeof(void *);
    }

#ifdef ARCHITECTURE_AMD64
    /* Align stack if necessary. */
    if ((stack_space_allocated + sizeof(void *)) & 15)
        stack_space_allocated += sizeof(void *);
#elif defined USE_FASTCALL_CALLING_CONVENTION
    /* Align stack if necessary. */
    if ((stack_space_allocated + sizeof(void *)) & 7)
        stack_space_allocated += sizeof(void *);
#else // USE_FASTCALL_CALLING_CONVENTION || ARCHITECTURE_AMD64
    stack_space_allocated += 2 * sizeof(void *);
    while ((stack_space_allocated + sizeof(void *)) & (ES_STACK_ALIGNMENT - 1))
        stack_space_allocated += sizeof(void *);
#endif // USE_FASTCALL_CALLING_CONVENTION || ARCHITECTURE_AMD64

    return stack_space_allocated;
}

void
ES_Native::GeneratePrologue()
{
    ANNOTATE("ES_Native::GeneratePrologue");

    if (!constructor && code->data->codewords_count == 1 && code->data->codewords[0].instruction == ESI_RETURN_NO_VALUE)
        return;

    cg.StartPrologue();

    DECLARE_NOTHING();

    if (entry_point_cw == code->data->codewords && prologue_entry_point && !entry_point_jump_target)
        entry_point_jump_target = cg.Here();

    BOOL has_variable_object = FALSE;

    if (!is_trivial)
    {
        if (is_light_weight)
        {
            if (!light_weight_failure)
                EmitLightWeightFailure();

            if (is_inlined_function_call)
            {
                ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);

                for (unsigned index = inlined_call_argc; index < fncode->GetData()->formals_count; ++index)
                    EmitSetRegisterType(VR(2 + index), ESTYPE_UNDEFINED);
            }
            else
            {
                cg.CMP(ES_CodeGenerator::IMMEDIATE(static_cast<ES_FunctionCode *>(code)->GetData()->formals_count), PARAMETERS_COUNT, ES_CodeGenerator::OPSIZE_32);
                cg.Jump(light_weight_wrong_argc, ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);
            }

            if (optimization_data->uses_this && !code->data->is_strict_mode)
            {
                ES_CodeGenerator::OutOfOrderBlock *prepare_this = cg.StartOutOfOrderBlock();

                int align_adjust = ES_STACK_ALIGNMENT - sizeof(void *);
                if (align_adjust != 0 && !is_inlined_function_call)
                    cg.SUB(ES_CodeGenerator::IMMEDIATE(align_adjust), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);

                IA32FUNCTIONCALLU("pp", ES_Native::PrepareThisObjectLightWeight, 0);

                call.AllocateStackSpace();
                call.AddArgument(CONTEXT_POINTER);
                call.AddArgument(REGISTER_FRAME_POINTER);
                call.Call();
                call.FreeStackSpace();

                if (align_adjust != 0 && !is_inlined_function_call)
                    cg.ADD(ES_CodeGenerator::IMMEDIATE(align_adjust), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);

                cg.EndOutOfOrderBlock();

                EmitRegisterTypeCheck(VR(0), ESTYPE_OBJECT, prepare_this->GetJumpTarget());

                cg.SetOutOfOrderContinuationPoint(prepare_this);
            }

#ifdef ARCHITECTURE_AMD64
            if (light_weight_accesses_cached_globals)
                MovePointerIntoRegister(cg, code, CODE_POINTER);
#endif // ARCHITECTURE_AMD64
        }
        else
        {
#ifdef ES_STACK_RECONSTRUCTION_SUPPORTED
            /* A loop dispatcher will have its stack frame created by
               ES_Execution_Context::ReconstructNativeStack(). */
            if (loop_dispatcher)
            {
#ifdef ARCHITECTURE_AMD64
                MovePointerIntoRegister(cg, code, CODE_POINTER);
#endif // ARCHITECTURE_AMD64
            }
            else
#endif // ES_STACK_RECONSTRUCTION_SUPPORTED
            {
                SetupNativeStackFrame(cg, code, stack_space_allocated, has_variable_object);

                OP_ASSERT(StackSpaceAllocated() == stack_space_allocated);
            }

            ES_CodeGenerator::OutOfOrderBlock *stack_or_register_limit_exceeded = cg.StartOutOfOrderBlock();

            IA32FunctionCall call(cg, "pu", reinterpret_cast<void (*)()>(&ES_Execution_Context::StackOrRegisterLimitExceeded), DEFAULT_USABLE_STACKSPACE);

            call.AllocateStackSpace();
            call.AddArgument(CONTEXT_POINTER); // ES_Execution_Context *context
            call.AddArgument(ES_CodeGenerator::IMMEDIATE(constructor)); // BOOL is_constructor
            call.Call();
            call.FreeStackSpace(stack_space_allocated);

            cg.RET();
            cg.EndOutOfOrderBlock(FALSE);

            if (entry_point_cw == code->data->codewords && !prologue_entry_point)
            {
                ES_CodeGenerator::OutOfOrderBlock *entry_point = cg.StartOutOfOrderBlock();

                entry_point_jump_target = cg.Here();

                cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, stack_space_allocated - (has_variable_object ? 6 : 5) * sizeof(void *)), PARAMETERS_COUNT, ES_CodeGenerator::OPSIZE_64);
                cg.EndOutOfOrderBlock();
                cg.SetOutOfOrderContinuationPoint(entry_point);
            }

            /* Check if we've reached the stack space limit. */
            /* NOTE: The observant reader will have noticed that in step 3 we've
               already used the CPU stack frame.  This is okay though; the stack
               limit includes a rather big safety zone, so we haven't actually run
               out of stack, we're only about to do it. */
            cg.CMP(OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, stack_limit), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
            cg.Jump(stack_or_register_limit_exceeded->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW, TRUE, FALSE);

            /* Check if there is enough room for our register frame. */
            cg.LEA(VR_WITH_INDEX(code->data->register_frame_size), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
            cg.CMP(ES_CodeGenerator::REG_AX, OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, reg_limit), ES_CodeGenerator::OPSIZE_POINTER);
            cg.Jump(stack_or_register_limit_exceeded->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW, TRUE, FALSE);
        }
    }
    else if (code->type == ES_Code::TYPE_FUNCTION)
    {
        ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);
        ES_FunctionCodeStatic *data = fncode->GetData();

        if (data->formals_count != 0)
            if (is_inlined_function_call)
            {
                for (unsigned index = inlined_call_argc; index < data->formals_count; ++index)
                    EmitSetRegisterType(VR(2 + index), ESTYPE_UNDEFINED);
            }
            else
            {
                ES_CodeGenerator::OutOfOrderBlock *initialize_formals = cg.StartOutOfOrderBlock();

                if (data->formals_count > 1)
                {
                    cg.NEG(PARAMETERS_COUNT);
                    cg.ADD(ES_CodeGenerator::IMMEDIATE(data->formals_count), PARAMETERS_COUNT);
                }

                unsigned vr_index = 2 + data->formals_count - 1;

                while (TRUE)
                {
                    EmitSetRegisterType(VR(vr_index), ESTYPE_UNDEFINED);

                    if (--vr_index < 2)
                        break;
                    else
                    {
                        cg.SUB(ES_CodeGenerator::IMMEDIATE(1), PARAMETERS_COUNT);
                        cg.Jump(initialize_formals->GetContinuationTarget(), ES_NATIVE_CONDITION_ZERO);
                    }
                }

                cg.EndOutOfOrderBlock();

                cg.CMP(ES_CodeGenerator::IMMEDIATE(data->formals_count), PARAMETERS_COUNT);
                cg.Jump(initialize_formals->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW, TRUE, FALSE);

                cg.SetOutOfOrderContinuationPoint(initialize_formals);
            }
    }

    int align_adjust = ES_STACK_ALIGNMENT - sizeof(void *);
    if (align_adjust != 0 && (is_trivial || is_light_weight) && !is_inlined_function_call)
        cg.SUB(ES_CodeGenerator::IMMEDIATE(align_adjust), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);

    if (code->type == ES_Code::TYPE_FUNCTION && !is_light_weight)
    {
        if (!constructor && optimization_data->uses_this && !code->data->is_strict_mode)
        {
            ES_CodeGenerator::OutOfOrderBlock *prepare_this = cg.StartOutOfOrderBlock();

            IA32FUNCTIONCALL("pp", ES_Native::PrepareThisObject);

            call.AllocateStackSpace();
            call.AddArgument(CONTEXT_POINTER);
            call.AddArgument(REGISTER_FRAME_POINTER);
            call.Call();
            call.FreeStackSpace();

#ifdef ARCHITECTURE_AMD64
            cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, stack_space_allocated - (has_variable_object ? 6 : 5) * sizeof(void *)), PARAMETERS_COUNT, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64

            cg.EndOutOfOrderBlock();

            EmitRegisterTypeCheck(VR(0), ESTYPE_OBJECT, prepare_this->GetJumpTarget());

            cg.SetOutOfOrderContinuationPoint(prepare_this);
        }

        ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);
        ES_FunctionCodeStatic *data = fncode->GetData();
        ES_CodeGenerator::OutOfOrderBlock *create_arguments_object;

        /* Create arguments array, if necessary. */
        BOOL may_use_arguments = !is_trivial && (!data->is_strict_mode || data->has_redirected_call);
        if (data->arguments_index == ES_FunctionCodeStatic::ARGUMENTS_NOT_USED && may_use_arguments)
            create_arguments_object = cg.StartOutOfOrderBlock();
        else
            create_arguments_object = NULL;

        if (data->arguments_index != ES_FunctionCodeStatic::ARGUMENTS_NOT_USED || may_use_arguments)
        {
            IA32FUNCTIONCALL("pppu", ES_Execution_Context::CreateArgumentsObject);

            call.AllocateStackSpace();
            call.AddArgument(CONTEXT_POINTER);
            call.AddArgument(REGISTER_VALUE(VR(1)));
            call.AddArgument(REGISTER_FRAME_POINTER);
            call.AddArgument(PARAMETERS_COUNT);
            call.Call();
            call.FreeStackSpace();

            if (data->formals_count != 0)
                cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, stack_space_allocated - (has_variable_object ? 6 : 5) * sizeof(void *)), PARAMETERS_COUNT, ES_CodeGenerator::OPSIZE_POINTER);

            if (create_arguments_object)
                cg.EndOutOfOrderBlock();
        }

        ES_CodeGenerator::OutOfOrderBlock *initialize_formals;

        /* Step 7: Set formals for which no value was provided to undefined. */
        if (data->formals_count != 0)
        {
            initialize_formals = cg.StartOutOfOrderBlock();

            if (data->formals_count > 1)
            {
                cg.NEG(PARAMETERS_COUNT);
                cg.ADD(ES_CodeGenerator::IMMEDIATE(data->formals_count), PARAMETERS_COUNT);
            }

            unsigned vr_index = 2 + data->formals_count - 1;

            while (TRUE)
            {
                EmitSetRegisterType(VR(vr_index), ESTYPE_UNDEFINED);

                if (--vr_index < 2)
                    break;
                else
                {
                    cg.SUB(ES_CodeGenerator::IMMEDIATE(1), PARAMETERS_COUNT);
                    cg.Jump(initialize_formals->GetContinuationTarget(), ES_NATIVE_CONDITION_ZERO);
                }
            }

            cg.EndOutOfOrderBlock();
        }
        else
            initialize_formals = NULL;

        if (create_arguments_object || initialize_formals)
        {
            cg.CMP(ES_CodeGenerator::IMMEDIATE(data->formals_count), PARAMETERS_COUNT);

            if (initialize_formals)
                cg.Jump(initialize_formals->GetJumpTarget(), ES_NATIVE_CONDITION_BELOW, TRUE, FALSE);
            if (create_arguments_object)
                cg.Jump(create_arguments_object->GetJumpTarget(), ES_NATIVE_CONDITION_ABOVE, TRUE, FALSE);
        }

        if (initialize_formals)
            cg.SetOutOfOrderContinuationPoint(initialize_formals);
        if (create_arguments_object)
            cg.SetOutOfOrderContinuationPoint(create_arguments_object);

        if (!is_trivial)
            EmitTick();

        if (constructor)
        {
            ES_CodeGenerator::Register object(ES_CodeGenerator::REG_AX);
            BOOL inline_allocation = constructor_final_class && CanAllocateObject(constructor_final_class, 0);
            ES_CodeGenerator::OutOfOrderBlock *complex_case = cg.StartOutOfOrderBlock();

            {
                IA32FUNCTIONCALLU("pp", ES_Execution_Context::AllocateObjectFromMachineCodeComplex, is_trivial ? 0 : DEFAULT_USABLE_STACKSPACE);

                call.AllocateStackSpace();
                call.AddArgument(CONTEXT_POINTER);
                call.AddArgument(REGISTER_VALUE(VR(1)));
                call.Call();
                call.FreeStackSpace();

                cg.EndOutOfOrderBlock();
            }

            /* Check that the constructor's prototype property has the expected value. */
            cg.MOV(REGISTER_VALUE(VR(1)), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
            cg.MOV(OBJECT_MEMBER(ES_CodeGenerator::REG_AX, ES_Object, properties), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);

            unsigned prototype_offset = ES_Function::GetPropertyOffset(ES_Function::PROTOTYPE);
            if (constructor_prototype)
            {
                cg.CMP(ESTYPE_OBJECT, ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_AX, prototype_offset + VALUE_TYPE_OFFSET), ES_CodeGenerator::OPSIZE_32);
                cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_AX, prototype_offset + VALUE_VALUE_OFFSET), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
                cg.Jump(complex_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);
                ComparePointerToRegister(cg, constructor_prototype, ES_CodeGenerator::REG_AX);
                cg.Jump(complex_case->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_EQUAL, TRUE, FALSE);
            }
            else
            {
                cg.CMP(ESTYPE_OBJECT, ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_AX, prototype_offset + VALUE_TYPE_OFFSET), ES_CodeGenerator::OPSIZE_32);
                cg.Jump(complex_case->GetJumpTarget(), ES_NATIVE_CONDITION_EQUAL, TRUE, FALSE);
            }

            ES_CodeGenerator::OutOfOrderBlock *non_trivial;

            if (inline_allocation)
                non_trivial = cg.StartOutOfOrderBlock();
            else
                non_trivial = NULL;

            {
                /* Step C2: Allocate the 'this' object. */
                IA32FUNCTIONCALLU("pp", ES_Execution_Context::AllocateObjectFromMachineCodeSimple, is_trivial ? 0 : DEFAULT_USABLE_STACKSPACE);

                call.AllocateStackSpace();
                call.AddArgument(CONTEXT_POINTER);
#ifdef ARCHITECTURE_AMD64_UNIX
                MovePointerIntoRegister(cg, constructor_class, ES_CodeGenerator::REG_SI);
                call.AddArgument(ES_CodeGenerator::REG_SI);
#elif defined(ARCHITECTURE_AMD64_WINDOWS)
                MovePointerIntoRegister(cg, constructor_class, ES_CodeGenerator::REG_DX);
                call.AddArgument(ES_CodeGenerator::REG_DX);
#else // ARCHITECTURE_AMD64_UNIX
                call.AddArgument(ES_CodeGenerator::IMMEDIATE(constructor_class));
#endif // ARCHITECTURE_AMD64_UNIX
                call.Call();
                call.FreeStackSpace();
            }

            if (non_trivial)
            {
                cg.EndOutOfOrderBlock();

                EmitAllocateObject(constructor_class, constructor_final_class, 0, 0, NULL, non_trivial->GetJumpTarget());

                cg.SetOutOfOrderContinuationPoint(non_trivial);
            }

            cg.SetOutOfOrderContinuationPoint(complex_case);

            cg.MOV(object, REGISTER_VALUE(VR(0)), ES_CodeGenerator::OPSIZE_POINTER);
            cg.MOV(ES_CodeGenerator::IMMEDIATE(ESTYPE_OBJECT), REGISTER_TYPE(VR(0)), ES_CodeGenerator::OPSIZE_32);
        }

#ifdef ES_NATIVE_PROFILING
        MovePointerIntoRegister(cg, code, ES_CodeGenerator::REG_DI);

        IA32FUNCTIONCALLU("p", ES_Execution_Context::EnteringFunction, DEFAULT_USABLE_STACKSPACE);

        call.AllocateStackSpace();
        call.AddArgument(ES_CodeGenerator::REG_DI);
        call.Call();
        call.FreeStackSpace();
#endif // ES_NATIVE_PROFILING
    }

    if (loop_dispatcher && first_loop_io)
    {
        IA32FUNCTIONCALLP("ppp", ES_Execution_Context::ReadLoopIO);

        call.AllocateStackSpace();
        call.AddArgument(CONTEXT_POINTER);
#ifdef ARCHITECTURE_AMD64
        call.AddArgument(CODE_POINTER);
#else // ARCHITECTURE_AMD64
        call.AddArgument(ES_CodeGenerator::IMMEDIATE(reinterpret_cast<UINTPTR>(code)));
#endif // ARCHITECTURE_AMD64
        call.AddArgument(REGISTER_FRAME_POINTER);
        call.Call();
        call.FreeStackSpace();
    }

    cg.EndPrologue();
}

void
ES_Native::GenerateEpilogue()
{
    DECLARE_NOTHING();

    ANNOTATE("ES_Native::GenerateEpilogue");

    cg.SetJumpTarget(epilogue_jump_target);

    if (!constructor && code->data->codewords_count == 1 && code->data->codewords[0].instruction == ESI_RETURN_NO_VALUE)
    {
        if (!is_inlined_function_call)
            cg.RET();
        return;
    }

    if (loop_dispatcher && first_loop_io)
    {
        IA32FUNCTIONCALLP("ppp", ES_Execution_Context::WriteLoopIO);

        call.AllocateStackSpace();
        call.AddArgument(CONTEXT_POINTER);
#ifdef ARCHITECTURE_AMD64
        call.AddArgument(CODE_POINTER);
#else // ARCHITECTURE_AMD64
        call.AddArgument(ES_CodeGenerator::IMMEDIATE(reinterpret_cast<UINTPTR>(code)));
#endif // ARCHITECTURE_AMD64
        call.AddArgument(REGISTER_FRAME_POINTER);
        call.Call();
        call.FreeStackSpace();
    }

    if (code->type == ES_Code::TYPE_FUNCTION && !is_light_weight)
    {
        ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);
        ES_FunctionCodeStatic *data = fncode->GetData();

        if (data->arguments_index != ES_FunctionCodeStatic::ARGUMENTS_NOT_USED || !is_trivial)
        {
            ES_CodeGenerator::OutOfOrderBlock *detach_arguments;

            cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, stack_space_allocated - 4 * sizeof(void *)), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);

            if (data->arguments_index == ES_FunctionCodeStatic::ARGUMENTS_NOT_USED)
                detach_arguments = cg.StartOutOfOrderBlock();
            else
                detach_arguments = NULL;

            IA32FunctionCall call(cg, "pp", reinterpret_cast<void (*)()>(&ES_Execution_Context::DetachArgumentsObject), DEFAULT_USABLE_STACKSPACE);

            call.AllocateStackSpace();
            call.AddArgument(CONTEXT_POINTER);
            call.AddArgument(ES_CodeGenerator::REG_AX);
            call.Call();
            call.FreeStackSpace();

            if (detach_arguments)
            {
                cg.EndOutOfOrderBlock();

                cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
                cg.Jump(detach_arguments->GetJumpTarget(), ES_NATIVE_CONDITION_NOT_ZERO, TRUE, FALSE);

                cg.SetOutOfOrderContinuationPoint(detach_arguments);
            }

            if (code->CanHaveVariableObject())
            {
                ES_CodeGenerator::JumpTarget *no_variables = cg.ForwardJump();

                cg.MOV(ES_CodeGenerator::MEMORY(ES_CodeGenerator::REG_SP, stack_space_allocated - 5 * sizeof(void *)), ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
                cg.TEST(ES_CodeGenerator::REG_AX, ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
                cg.Jump(no_variables, ES_NATIVE_CONDITION_ZERO, TRUE, TRUE);

                IA32FunctionCall call(cg, "pp", reinterpret_cast<void (*)()>(&ES_Execution_Context::DetachVariableObject), DEFAULT_USABLE_STACKSPACE);

                call.AllocateStackSpace();
                call.AddArgument(CONTEXT_POINTER);
                call.AddArgument(ES_CodeGenerator::REG_AX);
                call.Call();
                call.FreeStackSpace();

                cg.SetJumpTarget(no_variables);
            }
        }

#ifdef ES_NATIVE_PROFILING
        MovePointerIntoRegister(cg, code, ES_CodeGenerator::REG_DI);

        IA32FUNCTIONCALLU("p", ES_Execution_Context::LeavingFunction, DEFAULT_USABLE_STACKSPACE);

        call.AllocateStackSpace();
        call.AddArgument(ES_CodeGenerator::REG_DI);
        call.Call();
        call.FreeStackSpace();
#endif // ES_NATIVE_PROFILING
    }

    if (is_trivial || is_light_weight)
    {
        int align_adjust = ES_STACK_ALIGNMENT - sizeof(void *);
        if (align_adjust != 0 && !is_inlined_function_call)
            cg.ADD(ES_CodeGenerator::IMMEDIATE(align_adjust), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
    }
    else
    {
        cg.ADD(ES_CodeGenerator::IMMEDIATE(StackSpaceAllocated() - sizeof(void *)), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
        cg.POP(ES_CodeGenerator::REG_AX, ES_CodeGenerator::OPSIZE_POINTER);
        cg.MOV(ES_CodeGenerator::REG_AX, OBJECT_MEMBER(CONTEXT_POINTER, ES_Execution_Context, native_stack_frame), ES_CodeGenerator::OPSIZE_POINTER);
    }

    if (!is_inlined_function_call)
        cg.RET();
}

/* static */ unsigned
ES_NativeStackFrame::GetFrameSize(ES_Code *code, BOOL include_return_address)
{
    /* Return address, previous stack frame, register frame pointer and code
       pointer: */
    unsigned frame_size = 3 * sizeof(void *);

    if (code->type == ES_Code::TYPE_FUNCTION)
    {
        /* Arguments object and arguments count: */
        frame_size += 2 * sizeof(void *);

        if (code->CanHaveVariableObject())
            /* Variable object: */
            frame_size += sizeof(void *);
    }

#ifdef ARCHITECTURE_AMD64
    /* Align stack if necessary: */
    if ((frame_size + sizeof(void *)) & 15)
        frame_size += sizeof(void *);
#elif defined USE_FASTCALL_CALLING_CONVENTION
    /* Align stack if necessary: */
    if ((frame_size + sizeof(void *)) & 7)
        frame_size += sizeof(void *);
#else // USE_FASTCALL_CALLING_CONVENTION || ARCHITECTURE_AMD64
    /* Space for two arguments for function calls: */
    frame_size += 2 * sizeof(void *);

    /* Align stack if necessary: */
    while ((frame_size + sizeof(void *)) & (ES_STACK_ALIGNMENT - 1))
        frame_size += sizeof(void *);
#endif // USE_FASTCALL_CALLING_CONVENTION || ARCHITECTURE_AMD64

    if (include_return_address)
        frame_size += sizeof(void *);

    return frame_size;
}

#endif // ARCHITECTURE_IA32
#endif // ES_NATIVE_SUPPORT
