/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2010
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */
#ifndef ES_NATIVE_ARCH_H
#define ES_NATIVE_ARCH_H

#ifdef ES_NATIVE_SUPPORT

#if IEEE_BITS_HI
#  define VALUE_DVALUEH_OFFSET 4
#  define VALUE_DVALUEL_OFFSET 0
#else // IEEE_BITS_HI
#  define VALUE_DVALUEH_OFFSET 0
#  define VALUE_DVALUEL_OFFSET 4
#endif // IEEE_BITS_HI

#ifdef ES_VALUES_AS_NANS
#  define VALUE_TYPE_OFFSET VALUE_DVALUEH_OFFSET
#  define VALUE_IVALUE_OFFSET VALUE_DVALUEL_OFFSET
#  define VALUE_DVALUE_OFFSET 0
#else // ES_VALUES_AS_NANS
#  define VALUE_TYPE_OFFSET 0
#  define VALUE_IVALUE_OFFSET 8
#  define VALUE_DVALUE_OFFSET 8
#endif // ES_VALUES_AS_NANS

#if defined(_DEBUG) && defined(NATIVE_DISASSEMBLER_SUPPORT)
# define ES_DUMP_REGISTER_ALLOCATIONS
#endif // _DEBUG && NATIVE_DISASSEMBLER_SUPPORT

class ES_NativeStackFrame
{
public:
    static ES_Value_Internal *GetRegisterFrame(ES_NativeStackFrame *stack_frame);
    static unsigned GetRegisterFrameOffset(ES_NativeStackFrame *stack_frame);
    static ES_Code *&GetCode(ES_NativeStackFrame *stack_frame);
    static ES_CodeWord *GetCodeWord(ES_NativeStackFrame *stack_frame, ES_NativeStackFrame *previous_stack_frame);
    static ES_Arguments_Object *GetArgumentsObject(ES_NativeStackFrame *stack_frame);
    static void SetArgumentsObject(ES_NativeStackFrame *stack_frame, ES_Arguments_Object *arguments_object);
    static ES_Object *GetVariableObject(ES_NativeStackFrame *stack_frame);
    static void SetVariableObject(ES_NativeStackFrame *stack_frame, ES_Object *variable_object);
    static unsigned GetArgumentsCount(ES_NativeStackFrame *stack_frame);

    static unsigned GetFrameSize(ES_Code *code, BOOL include_return_address);
    static void *GetReturnAddress(ES_Code *code, ES_CodeWord *codeword);
    static void *GetReturnAddress(ES_NativeStackFrame *stack_frame);
    static void InitializeFrame(void *frame_start, ES_FrameStackIterator frame, void **&next_frame, BOOL include_return_address);
    static void SetRegisterFrame(ES_NativeStackFrame *stack_frame, ES_Value_Internal *register_frame);

    static ES_NativeStackFrame *GetNextFrame(ES_NativeStackFrame *stack_frame);
    static unsigned GetDepth(ES_NativeStackFrame *stack_frame);
};

/* static */ inline ES_Value_Internal *
ES_NativeStackFrame::GetRegisterFrame(ES_NativeStackFrame *stack_frame)
{
    return reinterpret_cast<ES_Value_Internal **>(stack_frame)[-1];
}

/* static */ inline unsigned
ES_NativeStackFrame::GetRegisterFrameOffset(ES_NativeStackFrame *stack_frame)
{
    return GetRegisterFrame(stack_frame) - GetRegisterFrame(GetNextFrame(stack_frame));
}

/* static */ inline ES_Code *&
ES_NativeStackFrame::GetCode(ES_NativeStackFrame *stack_frame)
{
    return reinterpret_cast<ES_Code **>(stack_frame)[-2];
}

/* static */ inline ES_Arguments_Object *
ES_NativeStackFrame::GetArgumentsObject(ES_NativeStackFrame *stack_frame)
{
    ES_Code *code = GetCode(stack_frame);
    if (code->type == ES_Code::TYPE_FUNCTION)
        return reinterpret_cast<ES_Arguments_Object **>(stack_frame)[-3];
    else
        return NULL;
}

/* static */ inline void
ES_NativeStackFrame::SetArgumentsObject(ES_NativeStackFrame *stack_frame, ES_Arguments_Object *arguments_object)
{
    OP_ASSERT(GetCode(stack_frame)->type == ES_Code::TYPE_FUNCTION);

    reinterpret_cast<ES_Arguments_Object **>(stack_frame)[-3] = arguments_object;
}

/* static */ inline ES_Object *
ES_NativeStackFrame::GetVariableObject(ES_NativeStackFrame *stack_frame)
{
    ES_Code *code = GetCode(stack_frame);
    if (code->type == ES_Code::TYPE_FUNCTION && code->CanHaveVariableObject())
#ifdef ARCHITECTURE_MIPS
        return reinterpret_cast<ES_Object **>(stack_frame)[-5];
#else
        return reinterpret_cast<ES_Object **>(stack_frame)[-4];
#endif
    else
        return NULL;
}

/* static */ inline void
ES_NativeStackFrame::SetVariableObject(ES_NativeStackFrame *stack_frame, ES_Object *variable_object)
{
    OP_ASSERT(GetCode(stack_frame)->type == ES_Code::TYPE_FUNCTION && GetCode(stack_frame)->CanHaveVariableObject());

#ifdef ARCHITECTURE_MIPS
    reinterpret_cast<ES_Object **>(stack_frame)[-5] = variable_object;
#else
    reinterpret_cast<ES_Object **>(stack_frame)[-4] = variable_object;
#endif
}

/* static */ inline ES_NativeStackFrame *
ES_NativeStackFrame::GetNextFrame(ES_NativeStackFrame *stack_frame)
{
    return *reinterpret_cast<ES_NativeStackFrame **>(stack_frame);
}

/* Only one of these will actually define anything. */
#include "modules/ecmascript/carakan/src/compiler/es_native_ia32.h"
#include "modules/ecmascript/carakan/src/compiler/es_native_arm.h"
#include "modules/ecmascript/carakan/src/compiler/es_native_mips.h"

#if 0
#include "modules/ecmascript/carakan/src/util/es_codegenerator_superh.h"
#include "modules/ecmascript/carakan/src/util/es_codegenerator_powerpc.h"
#endif // 0

#endif // ES_NATIVE_SUPPORT
#endif // ES_NATIVE_ARCH_H

