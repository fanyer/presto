/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Virtual machine definition; instruction encodings and attributes;
 * assembler macros.
 *
 * @author Lars Thomas Hansen
 */

#ifndef ES_BYTECODE_H
#define ES_BYTECODE_H

#include "modules/ecmascript/carakan/src/vm/es_instruction.h"

class ES_NativeStackFrame;
class ES_Code;

class ES_CodeWord
{
public:
    typedef unsigned Index;
    static const Index IndexMax = UINT_MAX;
    typedef int Offset;
    typedef int Immediate;

    ES_CodeWord() {}
    ES_CodeWord(unsigned u) { index = u; }
    ES_CodeWord(int i) { offset = i; }

    union
    {
#ifdef ES_DIRECT_THREADING
# ifndef SIXTY_FOUR_BIT
        void *instruction;
        /**< Instruction code. */
# endif // SIXTY_FOUR_BIT
#else // ES_DIRECT_THREADING
        ES_Instruction instruction;
        /**< Instruction code. */
#endif // ES_DIRECT_THREADING

        Index index;
        /**< Index, for instance into register array. */

        Offset offset;
        /**< Offset, for instance jump offset. */

        Immediate immediate;
        /**< Immediade integer. */
    };
};

class ES_VirtualStackFrame
{
public:
    ES_CodeWord *ip;
    ES_Value_Internal *first_register;
    ES_Code *code;
    ES_Object *variable_object;
    ES_Arguments_Object *arguments_object;
#ifdef ES_NATIVE_SUPPORT
    ES_NativeStackFrame *native_stack_frame;
    ES_CodeWord *true_ip;
#endif // ES_NATIVE_SUPPORT
    unsigned argc;
    unsigned register_overlap;
#ifdef ES_NATIVE_SUPPORT
    unsigned next_frame_size;
#endif // ES_NATIVE_SUPPORT
    BOOL in_constructor;
    BOOL first_in_block;

    enum FrameType
    {
        NORMAL,

        VIA_TOPRIMITIVE,
        VIA_FUNCTION_APPLY,
        VIA_FUNCTION_CALL,
        VIA_FUNCTION_BIND
    };

    FrameType frame_type;
};

class ES_StackTraceElement
{
public:
    // FIXME: Could make this a TWEAK.
#if defined _STANDALONE || defined LINGOGI || defined WINGOGI
    /* Larger limit makes throwing exceptions more expensive, and exception
       objects larger.  10 is probably enough for all normal debugging
       situations, but for internal debugging of crap code, 50 might be more
       useful. */
    enum { STACK_TRACE_ELEMENTS = 50 };
#else // _STANDALONE || LINGOGI || WINGOGI
    enum { STACK_TRACE_ELEMENTS = 10 };
#endif // _STANDALONE || LINGOGI || WINGOGI

    ES_Code *code;
    ES_CodeWord *codeword;
    ES_VirtualStackFrame::FrameType frame_type;
};

#endif // ES_BYTECODE_H
