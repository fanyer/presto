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

#include "modules/ecmascript/carakan/src/compiler/es_analyzer.h"
#include "modules/ecmascript/carakan/src/compiler/es_native_arch.h"

#ifdef ES_NATIVE_SUPPORT
#if defined(ARCHITECTURE_ARM) || defined(ARCHITECTURE_IA32) || defined(ARCHITECTURE_MIPS)

/* static */ ES_CodeWord *
ES_NativeStackFrame::GetCodeWord(ES_NativeStackFrame *stack_frame, ES_NativeStackFrame *previous_stack_frame)
{
    if (ES_Code::CodeWordFromReturnAddress *iter = GetCode(stack_frame)->codeword_from_return_address)
    {
        void *return_address = reinterpret_cast<void **>(previous_stack_frame)[1];
        while (iter->return_address)
            if (iter->return_address == return_address)
                return iter->codeword;
            else
                ++iter;
    }

    return NULL;
}

/* static */ unsigned
ES_NativeStackFrame::GetArgumentsCount(ES_NativeStackFrame *stack_frame)
{
    ES_Code *code = GetCode(stack_frame);

    if (code->type == ES_Code::TYPE_FUNCTION)
    {
#ifdef ARCHITECTURE_MIPS
        return reinterpret_cast<UINTPTR *>(stack_frame)[-4];
#else
        int index;

        if (code->CanHaveVariableObject())
            index = -5;
        else
            index = -4;

        return reinterpret_cast<UINTPTR *>(stack_frame)[index];
#endif
    }
    else
        return 0;
}

/* static */ void *
ES_NativeStackFrame::GetReturnAddress(ES_Code *code, ES_CodeWord *codeword)
{
    if (ES_Code::CodeWordFromReturnAddress *iter = code->codeword_from_return_address)
        while (iter->return_address)
        {
            ES_CodeWord *iter_cw = iter->codeword;
            if (ES_Analyzer::NextInstruction(code->data, iter_cw) && iter_cw == codeword)
                return iter->return_address;
            else
                ++iter;
        }

    return NULL;
}

/* static */ void *
ES_NativeStackFrame::GetReturnAddress(ES_NativeStackFrame *stack_frame)
{
    return reinterpret_cast<void **>(stack_frame)[1];
}

/* static */ void
ES_NativeStackFrame::InitializeFrame(void *frame_start, ES_FrameStackIterator frame, void **&next_frame, BOOL include_return_address)
{
    ES_Code *code = frame.GetCode();
    ES_CodeWord *codeword = frame.GetCodeWord();

    void **ptr = reinterpret_cast<void **>(frame_start);

    if (include_return_address)
        *(ptr - GetFrameSize(code, TRUE) / sizeof(void *)) = GetReturnAddress(code, codeword);

    *next_frame = --ptr;
    next_frame = ptr;
    *--ptr = frame.GetRegisterFrame();
    *--ptr = code;
    *--ptr = frame.GetArgumentsObject();
#ifdef ARCHITECTURE_MIPS
    *--ptr = reinterpret_cast<void *>(static_cast<UINTPTR>(frame.GetArgumentsCount()));
#endif

    if (code->CanHaveVariableObject())
        *--ptr = frame.GetVariableObject();

#ifndef ARCHITECTURE_MIPS
    *--ptr = reinterpret_cast<void *>(static_cast<UINTPTR>(frame.GetArgumentsCount()));
#endif
}

/* static */ void
ES_NativeStackFrame::SetRegisterFrame(ES_NativeStackFrame *stack_frame, ES_Value_Internal *register_frame)
{
    reinterpret_cast<ES_Value_Internal **>(stack_frame)[-1] = register_frame;
}

/* static */ unsigned
ES_NativeStackFrame::GetDepth(ES_NativeStackFrame *stack_frame)
{
    unsigned depth = 0;
    do
    {
        stack_frame = GetNextFrame(stack_frame);
        ++depth;
    }
    while (GetCode(stack_frame));
    return depth;
}

#endif // ARCHITECTURE_ARM || ARCHITECTURE_IA32 || ARCHITECTURE_MIPS
#endif // ES_NATIVE_SUPPORT
