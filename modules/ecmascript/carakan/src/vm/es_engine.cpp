/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * @author Daniel Spang
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/builtins/es_global_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_function_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_object_builtins.h"
#include "modules/ecmascript/carakan/src/vm/es_execution_context.h"
#include "modules/ecmascript/carakan/src/vm/es_instruction.h"
#include "modules/ecmascript/carakan/src/object/es_object.h"
#include "modules/ecmascript/carakan/src/object/es_array_object.h"
#include "modules/ecmascript/carakan/src/object/es_function.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"
#include "modules/ecmascript/carakan/src/object/es_arguments.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"
#include "modules/ecmascript/carakan/src/kernel/es_value.h"
#include "modules/ecmascript/carakan/src/compiler/es_code.h"
#include "modules/ecmascript/carakan/src/util/es_bcdebugger.h"

#ifdef ES_NATIVE_SUPPORT
# include "modules/ecmascript/carakan/src/compiler/es_native.h"
# include "modules/ecmascript/carakan/src/compiler/es_compiler.h"

#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
unsigned g_total_slow_case_calls = 0;
unsigned g_total_native_recompiles = 0;
#endif // ES_DEBUG_NATIVE_BEHAVIOUR
#endif // ES_NATIVE_SUPPORT

#ifdef ES_DIRECT_THREADING


#define CASE

#define INSTR_ENTRY(INSTR)                      \
    ESI_##INSTR:                                \
        IH_##INSTR();                           \
        goto *(ip++)->instruction);

#define INSTR_ENTRY_ALIAS(INSTR1, INSTR2)       \
    ESI_##INSTR1:                               \
        IH_##INSTR2();                          \
        goto *(ip++)->instruction);

#elif defined ES_NATIVE_SUPPORT

#define CASE case

#define IH_SETUP(noperands)                     \
    reg = Registers();                          \
    IH_SET_CONTEXT_IP(lip + noperands)

#define IH_SETUP_CODE(noperands)                \
    reg = Registers();                          \
    code = Code();                              \
    IH_SET_CONTEXT_IP(lip + noperands)

#define IH_ADDITIONAL_OPERANDS(noperands)       \
    IH_SET_CONTEXT_IP(ip + noperands)

#define INSTR_ENTRY(INSTR)                      \
    case ESI_##INSTR:                           \
        IH_##INSTR(lip);                        \
        break;

#define INSTR_ENTRY_ALIAS(INSTR1, INSTR2)       \
    case ESI_##INSTR1:                          \
        IH_##INSTR2(lip);                       \
        break;

#define SLOW_CASE_HANDLE_THROW()                \
    do                                          \
    {                                           \
        HandleThrow();                          \
        return FALSE;                           \
    }                                           \
    while (0)

#define SLOW_CASE_RETURN() return TRUE

#ifdef ES_DEBUG_NATIVE_BEHAVIOUR
# define TOTAL_SLOW_CASE_CALLS() ++g_total_slow_case_calls
#else // ES_DEBUG_NATIVE_BEHAVIOUR
# define TOTAL_SLOW_CASE_CALLS()
#endif // ES_DEBUG_NATIVE_BEHAVIOUR

#ifdef USE_CUSTOM_DOUBLE_OPS
# define ADD_DOUBLES(a, b) AddDoubles(a, b)
# define SUB_DOUBLES(a, b) SubtractDoubles(a, b)
# define MUL_DOUBLES(a, b) MultiplyDoubles(a, b)
# define DIV_DOUBLES(a, b) DivideDoubles(a, b)
#endif // USE_CUSTOM_DOUBLE_OPS

#ifdef _DEBUG
extern void ES_DebugEnteredSlowPath();
extern BOOL es_is_in_slow_path;

# define DEBUG_ENTERED_SLOW_PATH()              \
        if (!es_is_in_slow_path)                \
            ES_DebugEnteredSlowPath();
#else // _DEBUG
# define DEBUG_ENTERED_SLOW_PATH()
#endif // _DEBUG

#define PROFILING_START()                       \
    if (TRUE)                                   \
    {                                           \
        ES_Code *code = Code();                 \
        ES_CodeStatic *data = code->data;       \
        DEBUG_ENTERED_SLOW_PATH();              \
        if (!data->profile_data)                \
            AllocateProfileData();              \
        unsigned char *pd = data->profile_data + ((lip - 1) - data->codewords);

#define PROFILING_STORE_REGISTER_TYPE(rindex)   \
        unsigned rtype_##rindex = reg[rindex].TypeBits()

#define PROFILING_RECORD_REGISTER_TYPE(rindex, oindex) \
        if ((pd[oindex] & rtype_##rindex) == 0) \
        {                                       \
            pd[oindex] |= rtype_##rindex;       \
            pd[0] = 1;                          \
        }

#define PROFILING_RECORD_DATA(data, oindex)     \
        if ((data) != 0 && (pd[oindex] & (data)) == 0) \
        {                                       \
            pd[oindex] |= data;                 \
            pd[0] = 1;                          \
        }

#define PROFILING_END()                         \
        if (pd[0])                              \
            slow_path_flag = TRUE;              \
        TOTAL_SLOW_CASE_CALLS();                \
    }

#define PROFILING_END_WEIGHTED(weight)          \
        if (pd[0])                              \
            code->slow_case_calls += (weight);  \
        TOTAL_SLOW_CASE_CALLS();                \
    }

#define PROFILING_RECORD_SLOW_CASE_CALL(weight)                         \
    if (TRUE)                                                           \
    {                                                                   \
        ES_Code *code = Code();                                         \
        ES_CodeStatic *data = code->data;                               \
        if (data->profile_data)                                         \
        {                                                               \
            unsigned char *pd = data->profile_data + ((lip - 1) - data->codewords); \
            if (pd[0])                                                  \
                code->slow_case_calls += weight;                        \
            TOTAL_SLOW_CASE_CALLS();                                    \
            if (weight == ES_JIT_WEIGHT_CACHED_GET)                     \
                pd[3] |= ES_CodeStatic::GET_CACHE_HIT;                  \
        }                                                               \
    }                                                                   \

#define PROFILING_SET_INSTRUCTION_FLAG()                                \
    do                                                                  \
    {                                                                   \
        ES_Code *code = Code();                                         \
        ES_CodeStatic *data = code->data;                               \
        if (!data->profile_data)                                        \
            AllocateProfileData();                                      \
        unsigned char *pd = data->profile_data + ((lip - 1) - data->codewords); \
        pd[0] = 1;                                                      \
    }                                                                   \
    while (0)

#define PROFILING_RECORD_GET_CACHE_HIT()                                \
    do                                                                  \
    {                                                                   \
        ES_Code *code = Code();                                         \
        ES_CodeStatic *data = code->data;                               \
        if (data->profile_data)                                         \
        {                                                               \
            unsigned char *pd = data->profile_data + ((lip - 1) - data->codewords); \
            pd[3] |= ES_CodeStatic::GET_CACHE_HIT;                      \
        }                                                               \
    }                                                                   \
    while (0)

#else // ES_DIRECT_THREADING


#define CASE case

#define INSTR_ENTRY(INSTR)                      \
    case ESI_##INSTR:                           \
        IH_##INSTR();                           \
        break;

#define INSTR_ENTRY_ALIAS(INSTR1, INSTR2)       \
    case ESI_##INSTR1:                          \
        IH_##INSTR2();                          \
        break;

#define IH_SETUP(noperands)                     \
    ES_CodeWord *lip = ip;                      \
    ip += noperands

#define IH_SETUP_CODE(noperands)                \
    IH_SETUP(noperands)

#define IH_ADDITIONAL_OPERANDS(noperands)       \
    ip += noperands

#define SLOW_CASE_HANDLE_THROW()                \
    do                                          \
    {                                           \
        HandleThrow();                          \
        return FALSE;                           \
    }                                           \
    while (0)

#define SLOW_CASE_RETURN() return TRUE

#define PROFILING_START()
#define PROFILING_STORE_REGISTER_TYPE(rindex)
#define PROFILING_RECORD_REGISTER_TYPE(rindex, oindex)
#define PROFILING_RECORD_DATA(data, oindex)
#define PROFILING_END()
#define PROFILING_END_WEIGHTED(weight)
#define PROFILING_RECORD_SLOW_CASE_CALL(weight)
#define PROFILING_SET_INSTRUCTION_FLAG()
#define PROFILING_RECORD_GET_CACHE_HIT()

#endif // ES_DIRECT_THREADING


#ifdef ECMASCRIPT_DEBUGGER
# define HANDLE_THROW()                                             \
    do                                                              \
    {                                                               \
        SignalToDebugger(ES_DebugListener::ESEV_ERROR, lip);        \
        HandleThrow();                                              \
    }                                                               \
    while (0)
#else // ECMASCRIPT_DEBUGGER
# define HANDLE_THROW() HandleThrow()
#endif // ECMASCRIPT_DEBUGGER

#define HANDLE_FINALLY(t) HandleFinally(t)

#define START_USING_SCRATCH_VALUES ES_Value_Internal *scratch_value_storage; StartUsingScratchRegisters(scratch_value_storage)
#define END_USING_SCRATCH_VALUES EndUsingScratchRegisters(scratch_value_storage)

#ifndef ADD_DOUBLES
# define ADD_DOUBLES(a, b) ((a) + (b))
# define SUB_DOUBLES(a, b) ((a) - (b))
# define MUL_DOUBLES(a, b) ((a) * (b))
# define DIV_DOUBLES(a, b) ((a) / (b))
#endif // ADD_DOUBLES

#ifdef SIXTY_FOUR_BIT
# define GET_CODE_PARAM Code()
# define SET_CODE_PARAM , Code()
#else // SIXTY_FOUR_BIT
# define GET_CODE_PARAM
# define SET_CODE_PARAM
#endif // SIXTY_FOUR_BIT

#ifdef _DEBUG
unsigned g_instructions_executed = 0;
#endif // _DEBUG

#define TOOBJECT(value, ret)                              \
    do                                                    \
    {                                                     \
        if (!(value).ToObject(this, FALSE))               \
        {                                                 \
            ThrowErrorBase(lip - 1, TYPE_ERROR, EXPECTED_OBJECT); \
            ret;                                          \
        }                                                 \
    }                                                     \
    while (0)

#ifdef DEBUG_ENABLE_OPASSERT
// #define DISABLE_PROPERTY_READ_CACHE
// #define DISABLE_PROPERTY_WRITE_CACHE
#endif // DEBUG_ENABLE_OPASSERT

#define PROPERTY_CACHE_SIZE MAX_PROPERTY_CACHE_SIZE

ES_Value_Internal
ES_Execution_Context::Execute()
{
    //OP_ASSERT(!heap->IsLocked());

#ifdef ES_NATIVE_SUPPORT
    if (code->type == ES_Code::TYPE_PROGRAM && code->native_dispatcher)
    {
        CallBytecodeToNativeTrampoline(NULL, reg, code->native_code_block, code->native_dispatcher, 0);
        return return_value;
    }
#endif // ES_NATIVE_SUPPORT

#ifdef _DEBUG
    BOOL saved_scratch_values_used = scratch_values_used;
#endif // _DEBUG

#ifdef ES_DIRECT_THREADING
    goto *(ip++)->instruction;
#else
#ifdef ES_BYTECODE_DEBUGGER
    ES_CodeWord *previous_ip = NULL;
#endif // ES_BYTECODE_DEBUGGER
    for (;;)
    {
#ifdef ES_BYTECODE_DEBUGGER
        if (bytecode_debugger_enabled)
        {
            ES_BytecodeDebugger::AtInstruction(this, code, previous_ip, ip);
            previous_ip = ip;
        }
#endif // ES_BYTECODE_DEBUGGER

#ifdef ES_BYTECODE_LOGGER
        bytecode_logger.LogInstruction(ip->instruction);
#endif // ES_BYTECODE_LOGGER

#ifdef _DEBUG
        ++g_instructions_executed;
#endif // _DEBUG

        OP_ASSERT(current_exception.IsNoExceptionCookie() || ip->instruction == ESI_CATCH || ip->instruction == ESI_CATCH_SCOPE || ip->instruction == ESI_EXIT_TO_BUILTIN || ip->instruction == ESI_EXIT_TO_EVAL || ip->instruction == ESI_EXIT);

        // FIXME: Enable this when we have stopped over-locking.
        // OP_ASSERT(!heap->IsLocked());

#ifdef ES_NATIVE_SUPPORT
        ES_CodeWord *lip = ip + 1;
        switch (ip->instruction)
#else // ES_NATIVE_SUPPORT
        switch ((ip++)->instruction)
#endif // ES_NATIVE_SUPPORT
#endif // ES_DIRECT_THREADING
        {
            INSTR_ENTRY(LOAD_STRING);
            INSTR_ENTRY(LOAD_DOUBLE);
            INSTR_ENTRY(LOAD_INT32);
            INSTR_ENTRY(LOAD_NULL);
            INSTR_ENTRY(LOAD_UNDEFINED);
            INSTR_ENTRY(LOAD_TRUE);
            INSTR_ENTRY(LOAD_FALSE);
            INSTR_ENTRY(LOAD_GLOBAL_OBJECT);
            INSTR_ENTRY(COPY);
            INSTR_ENTRY(COPYN);
            INSTR_ENTRY(TYPEOF);
            INSTR_ENTRY(TONUMBER);
            INSTR_ENTRY(TOOBJECT);
            INSTR_ENTRY(TOPRIMITIVE);
            INSTR_ENTRY(TOPRIMITIVE1);
            INSTR_ENTRY(IS_NULL_OR_UNDEFINED);
            INSTR_ENTRY(IS_NOT_NULL_OR_UNDEFINED);
            INSTR_ENTRY(ADD);
            INSTR_ENTRY(ADD_IMM);
#ifdef ES_COMBINED_ADD_SUPPORT
            INSTR_ENTRY(ADDN);
#endif // ES_COMBINED_ADD_SUPPORT
            INSTR_ENTRY(FORMAT_STRING);
            INSTR_ENTRY(SUB);
            INSTR_ENTRY(SUB_IMM);
            INSTR_ENTRY(MUL);
            INSTR_ENTRY(MUL_IMM);
            INSTR_ENTRY(DIV);
            INSTR_ENTRY(DIV_IMM);
            INSTR_ENTRY(REM);
            INSTR_ENTRY(REM_IMM);
            INSTR_ENTRY(LSHIFT);
            INSTR_ENTRY(LSHIFT_IMM);
            INSTR_ENTRY(RSHIFT_SIGNED);
            INSTR_ENTRY(RSHIFT_SIGNED_IMM);
            INSTR_ENTRY(RSHIFT_UNSIGNED);
            INSTR_ENTRY(RSHIFT_UNSIGNED_IMM);
            INSTR_ENTRY(NEG);
            INSTR_ENTRY(COMPL);
            INSTR_ENTRY(INC);
            INSTR_ENTRY(DEC);
            INSTR_ENTRY(BITAND);
            INSTR_ENTRY(BITAND_IMM);
            INSTR_ENTRY(BITOR);
            INSTR_ENTRY(BITOR_IMM);
            INSTR_ENTRY(BITXOR);
            INSTR_ENTRY(BITXOR_IMM);
            INSTR_ENTRY(NOT);
            INSTR_ENTRY(EQ);
            INSTR_ENTRY(EQ_IMM);
            INSTR_ENTRY(NEQ);
            INSTR_ENTRY(NEQ_IMM);
            INSTR_ENTRY(EQ_STRICT);
            INSTR_ENTRY(EQ_STRICT_IMM);
            INSTR_ENTRY(NEQ_STRICT);
            INSTR_ENTRY(NEQ_STRICT_IMM);
            INSTR_ENTRY(LT);
            INSTR_ENTRY(LT_IMM);
            INSTR_ENTRY(LTE);
            INSTR_ENTRY(LTE_IMM);
            INSTR_ENTRY(GT);
            INSTR_ENTRY(GT_IMM);
            INSTR_ENTRY(GTE);
            INSTR_ENTRY(GTE_IMM);
            INSTR_ENTRY(CONDITION);
            INSTR_ENTRY(JUMP);
            INSTR_ENTRY(JUMP_INDIRECT);
            INSTR_ENTRY(JUMP_IF_TRUE);
            INSTR_ENTRY(JUMP_IF_FALSE);
            INSTR_ENTRY(START_LOOP);
            INSTR_ENTRY(STORE_BOOLEAN);
            INSTR_ENTRY(GETN_IMM);
            INSTR_ENTRY(PUTN_IMM);
            INSTR_ENTRY_ALIAS(INIT_PROPERTY, PUTN_IMM);
            INSTR_ENTRY(GET_LENGTH);
            INSTR_ENTRY(PUT_LENGTH);
            INSTR_ENTRY(GETI_IMM);
            INSTR_ENTRY(PUTI_IMM);
            INSTR_ENTRY(GET);
            INSTR_ENTRY(PUT);
            INSTR_ENTRY(DEFINE_GETTER);
            INSTR_ENTRY(DEFINE_SETTER);
            INSTR_ENTRY(GET_SCOPE);
            INSTR_ENTRY(GET_SCOPE_REF);
            INSTR_ENTRY(PUT_SCOPE);
            INSTR_ENTRY(DELETE_SCOPE);
            INSTR_ENTRY(GET_GLOBAL);
            INSTR_ENTRY(PUT_GLOBAL);
            INSTR_ENTRY(GET_LEXICAL);
            INSTR_ENTRY(PUT_LEXICAL);
            INSTR_ENTRY(DELETEN_IMM);
            INSTR_ENTRY(DELETEI_IMM);
            INSTR_ENTRY(DELETE);
            INSTR_ENTRY(DECLARE_GLOBAL);
            INSTR_ENTRY(HASPROPERTY);
            INSTR_ENTRY(INSTANCEOF);
            INSTR_ENTRY(ENUMERATE);
            INSTR_ENTRY(NEXT_PROPERTY);
            INSTR_ENTRY(EVAL);
            INSTR_ENTRY(CALL);
            INSTR_ENTRY(REDIRECTED_CALL);
            INSTR_ENTRY(APPLY);
            INSTR_ENTRY(CONSTRUCT);
            INSTR_ENTRY(RETURN_VALUE);
            INSTR_ENTRY(RETURN_NO_VALUE);
            INSTR_ENTRY(NEW_OBJECT);
            INSTR_ENTRY(CONSTRUCT_OBJECT);
            INSTR_ENTRY(NEW_ARRAY);
            INSTR_ENTRY(CONSTRUCT_ARRAY);
            INSTR_ENTRY(NEW_FUNCTION);
            INSTR_ENTRY(NEW_REGEXP);
            INSTR_ENTRY(TABLE_SWITCH);
            INSTR_ENTRY(CATCH);
            INSTR_ENTRY(CATCH_SCOPE);
            INSTR_ENTRY(THROW);
            INSTR_ENTRY(THROW_BUILTIN);
            INSTR_ENTRY(RETHROW);
#ifdef ECMASCRIPT_DEBUGGER
            INSTR_ENTRY(DEBUGGER_STOP);
#endif // ECMASCRIPT_DEBUGGER

        CASE ESI_RETURN_FROM_EVAL:
            {
#ifndef ES_NATIVE_SUPPORT
                ES_CodeWord *lip = ip;
#endif // ES_NATIVE_SUPPORT

                if (lip->index == UINT_MAX)
                    return_value.SetUndefined();
                else
                    return_value = reg[lip->index];

                register_blocks.Free(code->data->register_frame_size, overlap, overlap, first_in_block);

                frame_stack.Pop(this);

                return return_value;
            }

        CASE ESI_EXIT_TO_BUILTIN:
            {
                return_value = reg[0];

                register_blocks.Free(1, overlap, 0, first_in_block);

                frame_stack.Pop(this);

                OP_ASSERT(eval_status == ES_NORMAL);
                return return_value;
            }

        CASE ESI_EXIT_TO_EVAL:
            {
                OP_ASSERT(!current_exception.IsNoExceptionCookie());

                register_blocks.Free(overlap, overlap, 0, first_in_block);

                frame_stack.Pop(this);

                OP_ASSERT(eval_status == ES_NORMAL);
                return return_value;
            }

        CASE ESI_EXIT:
            {
                /* It could happen that host code tried to suspend this context
                   using ES_Runtime::SuspendContext() just before it finished,
                   and that we didn't have a chance to yield execution before
                   reaching this point.  It's probably an error on the host
                   side; trying to block a thread at a point where we don't
                   support suspending.

                   But returning ES_SUSPENDED when the context has actually
                   finished already is exceptionally bad, so to be safe,
                   override ES_SUSPENDED to ES_NORMAL just in case. */
                if (eval_status == ES_SUSPENDED)
                    eval_status = ES_NORMAL;
                else
                    OP_ASSERT(eval_status == ES_NORMAL || eval_status == ES_BLOCKED);

                if (generate_result && current_exception.IsNoExceptionCookie() && frame_stack.IsEmpty())
                {
                    if (convert_result_to_string && !reg[0].IsUndefined())
                        reg[0].ToString(this);

                    return_value = reg[0];
                }

                return return_value;
            }
        default:
            OP_ASSERT(!"Instruction not added!");
            break;
        }
#ifndef ES_DIRECT_THREADING

#ifdef _DEBUG
        OP_ASSERT(saved_scratch_values_used == scratch_values_used);
#endif // _DEBUG
    }
#endif // ES_DIRECT_THREADING
}

IH_SPECIFIER
ES_Execution_Context::ExecuteSingleInstruction(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    switch ((lip++)->instruction)
#else // ES_NATIVE_SUPPORT
    switch ((ip++)->instruction)
#endif // ES_NATIVE_SUPPORT
    {
        INSTR_ENTRY(LOAD_STRING);
        INSTR_ENTRY(LOAD_DOUBLE);
        INSTR_ENTRY(LOAD_INT32);
        INSTR_ENTRY(LOAD_NULL);
        INSTR_ENTRY(LOAD_UNDEFINED);
        INSTR_ENTRY(LOAD_TRUE);
        INSTR_ENTRY(LOAD_FALSE);
        INSTR_ENTRY(LOAD_GLOBAL_OBJECT);
        INSTR_ENTRY(COPY);
        INSTR_ENTRY(COPYN);
        INSTR_ENTRY(TYPEOF);
        INSTR_ENTRY(TONUMBER);
        INSTR_ENTRY(TOOBJECT);
        INSTR_ENTRY(TOPRIMITIVE);
        INSTR_ENTRY(TOPRIMITIVE1);
        INSTR_ENTRY(IS_NULL_OR_UNDEFINED);
        INSTR_ENTRY(IS_NOT_NULL_OR_UNDEFINED);
        INSTR_ENTRY(ADD);
        INSTR_ENTRY(ADD_IMM);
#ifdef ES_COMBINED_ADD_SUPPORT
        INSTR_ENTRY(ADDN);
#endif // ES_COMBINED_ADD_SUPPORT
        INSTR_ENTRY(FORMAT_STRING);
        INSTR_ENTRY(SUB);
        INSTR_ENTRY(SUB_IMM);
        INSTR_ENTRY(MUL);
        INSTR_ENTRY(MUL_IMM);
        INSTR_ENTRY(DIV);
        INSTR_ENTRY(DIV_IMM);
        INSTR_ENTRY(REM);
        INSTR_ENTRY(REM_IMM);
        INSTR_ENTRY(LSHIFT);
        INSTR_ENTRY(LSHIFT_IMM);
        INSTR_ENTRY(RSHIFT_SIGNED);
        INSTR_ENTRY(RSHIFT_SIGNED_IMM);
        INSTR_ENTRY(RSHIFT_UNSIGNED);
        INSTR_ENTRY(RSHIFT_UNSIGNED_IMM);
        INSTR_ENTRY(NEG);
        INSTR_ENTRY(COMPL);
        INSTR_ENTRY(INC);
        INSTR_ENTRY(DEC);
        INSTR_ENTRY(BITAND);
        INSTR_ENTRY(BITAND_IMM);
        INSTR_ENTRY(BITOR);
        INSTR_ENTRY(BITOR_IMM);
        INSTR_ENTRY(BITXOR);
        INSTR_ENTRY(BITXOR_IMM);
        INSTR_ENTRY(NOT);
        INSTR_ENTRY(EQ);
        INSTR_ENTRY(EQ_IMM);
        INSTR_ENTRY(NEQ);
        INSTR_ENTRY(NEQ_IMM);
        INSTR_ENTRY(EQ_STRICT);
        INSTR_ENTRY(EQ_STRICT_IMM);
        INSTR_ENTRY(NEQ_STRICT);
        INSTR_ENTRY(NEQ_STRICT_IMM);
        INSTR_ENTRY(LT);
        INSTR_ENTRY(LT_IMM);
        INSTR_ENTRY(LTE);
        INSTR_ENTRY(LTE_IMM);
        INSTR_ENTRY(GT);
        INSTR_ENTRY(GT_IMM);
        INSTR_ENTRY(GTE);
        INSTR_ENTRY(GTE_IMM);
        INSTR_ENTRY(CONDITION);
        INSTR_ENTRY(JUMP);
        INSTR_ENTRY(JUMP_INDIRECT);
        INSTR_ENTRY(JUMP_IF_TRUE);
        INSTR_ENTRY(JUMP_IF_FALSE);
        INSTR_ENTRY(START_LOOP);
        INSTR_ENTRY(STORE_BOOLEAN);
        INSTR_ENTRY(GETN_IMM);
        INSTR_ENTRY(PUTN_IMM);
        INSTR_ENTRY_ALIAS(INIT_PROPERTY, PUTN_IMM);
        INSTR_ENTRY(GET_LENGTH);
        INSTR_ENTRY(PUT_LENGTH);
        INSTR_ENTRY(GETI_IMM);
        INSTR_ENTRY(PUTI_IMM);
        INSTR_ENTRY(GET);
        INSTR_ENTRY(PUT);
        INSTR_ENTRY(DEFINE_GETTER);
        INSTR_ENTRY(DEFINE_SETTER);
        INSTR_ENTRY(GET_SCOPE);
        INSTR_ENTRY(GET_SCOPE_REF);
        INSTR_ENTRY(PUT_SCOPE);
        INSTR_ENTRY(DELETE_SCOPE);
        INSTR_ENTRY(GET_GLOBAL);
        INSTR_ENTRY(PUT_GLOBAL);
        INSTR_ENTRY(GET_LEXICAL);
        INSTR_ENTRY(PUT_LEXICAL);
        INSTR_ENTRY(DELETEN_IMM);
        INSTR_ENTRY(DELETEI_IMM);
        INSTR_ENTRY(DELETE);
        INSTR_ENTRY(DECLARE_GLOBAL);
        INSTR_ENTRY(HASPROPERTY);
        INSTR_ENTRY(INSTANCEOF);
        INSTR_ENTRY(ENUMERATE);
        INSTR_ENTRY(NEXT_PROPERTY);
        INSTR_ENTRY(EVAL);
        INSTR_ENTRY(CALL);
        INSTR_ENTRY(REDIRECTED_CALL);
        INSTR_ENTRY(CONSTRUCT);
        INSTR_ENTRY(RETURN_VALUE);
        INSTR_ENTRY(RETURN_NO_VALUE);
        INSTR_ENTRY(NEW_OBJECT);
        INSTR_ENTRY(CONSTRUCT_OBJECT);
        INSTR_ENTRY(NEW_ARRAY);
        INSTR_ENTRY(CONSTRUCT_ARRAY);
        INSTR_ENTRY(NEW_FUNCTION);
        INSTR_ENTRY(NEW_REGEXP);
        INSTR_ENTRY(TABLE_SWITCH);
        INSTR_ENTRY(CATCH);
        INSTR_ENTRY(CATCH_SCOPE);
        INSTR_ENTRY(THROW);
        INSTR_ENTRY(THROW_BUILTIN);
        INSTR_ENTRY(RETHROW);

    default:
        OP_ASSERT(FALSE);
    }

    IH_RETURN;
}

/* Instruction handlers */

IH_SPECIFIER ES_Execution_Context::IH_LOAD_STRING(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;

    reg[dst].SetString(String(src));

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_LOAD_DOUBLE(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;

    reg[dst].SetDouble(Code()->data->doubles[src]);

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_LOAD_INT32(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Immediate src = lip[1].immediate;

    reg[dst].SetInt32(src);

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_LOAD_NULL(IH_PARAM_DECL)
{
    IH_SETUP(1);

    ES_CodeWord::Index dst = lip->index;

    reg[dst].SetNull();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_LOAD_UNDEFINED(IH_PARAM_DECL)
{
    IH_SETUP(1);

    ES_CodeWord::Index dst = lip->index;

    reg[dst].SetUndefined();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_LOAD_TRUE(IH_PARAM_DECL)
{
    IH_SETUP(1);

    ES_CodeWord::Index dst = lip->index;

    reg[dst].SetTrue();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_LOAD_FALSE(IH_PARAM_DECL)
{
    IH_SETUP(1);

    ES_CodeWord::Index dst = lip->index;

    reg[dst].SetFalse();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_LOAD_GLOBAL_OBJECT(IH_PARAM_DECL)
{
    IH_SETUP(1);

    ES_CodeWord::Index dst = lip->index;

    reg[dst].SetObject(GetGlobalObject());

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_COPY(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;

    reg[dst] = reg[src];

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_COPYN(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index count = lip[1].index;

    IH_ADDITIONAL_OPERANDS(count);

    ES_CodeWord::Index count_plus_2 = count + 2;

    for (unsigned index = 2; index < count_plus_2; ++index)
        reg[dst++] = reg[lip[index].index];

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_TYPEOF(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;

    reg[dst].SetString(reg[src].TypeString(this));

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_TONUMBER(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;

    reg[dst] = reg[src];

    if (!reg[dst].ToNumber(this))
        HANDLE_THROW();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_TOOBJECT(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;

    reg[dst] = reg[src];

    TOOBJECT(reg[dst], HANDLE_THROW(); IH_RETURN);
    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_TOPRIMITIVE(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;
    ES_CodeWord::Immediate hint = lip[2].immediate;

    reg[dst] = reg[src];

    if (reg[dst].IsObject() && !reg[dst].ToPrimitive(this, static_cast<ES_Value_Internal::HintType>(hint)))
        HANDLE_THROW();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_TOPRIMITIVE1(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst_src = lip[0].index;
    ES_CodeWord::Immediate hint = lip[1].immediate;

    if (reg[dst_src].IsObject() && !reg[dst_src].ToPrimitive(this, static_cast<ES_Value_Internal::HintType>(hint)))
        HANDLE_THROW();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_IS_NULL_OR_UNDEFINED(IH_PARAM_DECL)
{
    IH_SETUP(1);

    implicit_bool = reg[lip[0].index].IsNullOrUndefined() || reg[lip[0].index].IsHiddenObject(this);

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_IS_NOT_NULL_OR_UNDEFINED(IH_PARAM_DECL)
{
    IH_SETUP(1);

    implicit_bool = !(reg[lip[0].index].IsNullOrUndefined() || reg[lip[0].index].IsHiddenObject(this));

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_ADD(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Index src2 = lip[2].index;

    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);

    if (reg[src1].IsInt32() && reg[src2].IsInt32())
    {
        INT32 isrc1 = reg[src1].GetInt32();
        INT32 isrc2 = reg[src2].GetInt32();
        INT32 result = isrc1 + isrc2;
        if ((isrc1 ^ isrc2) < 0 || (result ^ isrc1) >= 0)
        {
            reg[dst].SetInt32(result);
            goto profiling;
        }
    }

    if (reg[src1].IsNumber() && reg[src2].IsNumber())
    {
        reg[dst].SetNumber(ADD_DOUBLES(reg[src1].GetNumAsDouble(), reg[src2].GetNumAsDouble()));
        goto profiling;
    }
    else
    {
        /* dst might be the same register as a src */
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];
        src_val2 = reg[src2];

        if (!AddSlow(&reg[dst], &src_val1, &src_val2, scratch_value_storage))
            IH_RETURN;
    }

profiling:
    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_RECORD_REGISTER_TYPE(src2, 3);
    PROFILING_END();

    IH_RETURN;
}

#ifdef ES_COMBINED_ADD_SUPPORT

IH_SPECIFIER ES_Execution_Context::IH_ADDN(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index count = lip[1].index;

    IH_ADDITIONAL_OPERANDS(count);

    lip += 2;

    OP_ASSERT(!reg[lip[0].index].IsObject());

    reg[lip[0].index].ToString(this);

    unsigned length = Length(reg[lip[0].index].GetString()), index = 0;

    /* This instruction's arguments are guaranteed to be temporaries and
       primitives, so we're allowed to modify them in-place, and we can assume
       that ToString doesn't fail. */

    while (++index != count)
    {
        OP_ASSERT(!reg[lip[index].index].IsObject());
        reg[lip[index].index].ToString(this);
        length += Length(reg[lip[index].index].GetString());
    }

    if (length == 0)
        reg[dst].SetString(rt_data->strings[STRING_empty]);
    else if (length == 1)
    {
        for (index = 0; index != count; ++index)
        {
            JString *string = reg[lip[index].index].GetString();
            if (Length(string) != 0)
            {
                reg[dst].SetString(string);
                break;
            }
        }
    }
    else if (sizeof(JStringStorage) + length * 2 < sizeof(JStringSegmented) + count * (sizeof(JStringStorage *) + 2 * sizeof(unsigned)))
    {
        JString *result = JString::Make(this, length);
        reg[dst].SetString(result);

        uni_char *ptr = Storage(this, result);

        for (index = 0; index != count; ++index)
        {
            JString *string = reg[lip[index].index].GetString();
            op_memcpy(ptr, Storage(this, string), Length(string) * sizeof(uni_char));
            ptr += Length(string);
        }
    }
    else
    {
        unsigned nsegments = 0;

        for (index = 0; index != count; ++index)
        {
            JString *string = reg[lip[index].index].GetString();
            unsigned scount = GetSegmentCount(string);

            if (scount > 64)
            {
                Storage(this, string);
                ++nsegments;
            }
            else
                nsegments += scount;
        }

        JStringSegmented *segmented = JStringSegmented::Make(this, nsegments);

        JStringStorage **bases = segmented->Bases();
        unsigned *offsets = segmented->Offsets();
        unsigned *lengths = segmented->Lengths();

        for (index = 0; index != count; ++index)
        {
            JString *string = reg[lip[index].index].GetString();
            if (Length(string) != 0)
                if (IsSegmented(string))
                {
                    for (JSegmentIterator iter(string); iter.Next(); )
                    {
                        *bases++ = iter.GetBase();
                        *offsets++ = iter.GetOffset();
                        *lengths++ = iter.GetLength();
                    }
                }
                else
                {
                    *bases++ = string->value;
                    *offsets++ = string->offset;
                    *lengths++ = Length(string);
                }
        }

        ES_CollectorLock gclock(this);
        reg[dst].SetString(JString::Make(this, segmented, length));
    }

    IH_RETURN;
}

#endif // ES_COMBINED_ADD_SUPPORT

IH_SPECIFIER ES_Execution_Context::IH_FORMAT_STRING(IH_PARAM_DECL)
{
    IH_SETUP_CODE(5);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index value = lip[1].index;
    ES_CodeWord::Index prefix = lip[2].index;
    ES_CodeWord::Index suffix = lip[3].index;
    ES_CodeWord::Index cache = lip[4].index;

    JString *string;

    if (reg[value].IsString())
        string = reg[value].GetString();
    else if (reg[value].IsDouble() && numbertostring_cache_number != 0 && reg[value].GetDouble() == numbertostring_cache_number)
        string = numbertostring_cache_string;
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[value];

        if (src_val1.IsObject())
            if (!src_val1.ToPrimitive(this, ES_Value_Internal::HintNone))
            {
                END_USING_SCRATCH_VALUES;
                HANDLE_THROW();
                IH_RETURN;
            }

        src_val1.ToString(this);
        string = src_val1.GetString();

        END_USING_SCRATCH_VALUES;
    }

    ES_CollectorLock gclock(this, TRUE);

    if (prefix == UINT_MAX && suffix == UINT_MAX)
    {
        reg[dst].SetString(string);
        IH_RETURN;
    }

    if (code->format_string_caches[cache].from == string)
    {
#ifdef ES_NATIVE_SUPPORT
        ES_CodeStatic *data = code->data;

        if (data->profile_data)
            (data->profile_data + (lip - data->codewords))[4] = 1;
#endif // ES_NATIVE_SUPPORT

        reg[dst].SetString(code->format_string_caches[cache].to);
        IH_RETURN;
    }

    unsigned length = Length(string);

    if (length == 0)
        if (prefix == UINT_MAX)
        {
            reg[dst] = code->GetString(suffix);
            IH_RETURN;
        }
        else if (suffix == UINT_MAX)
        {
            reg[dst] = code->GetString(prefix);
            IH_RETURN;
        }

    JString *result;

    if (prefix == UINT_MAX && !IsSegmented(string))
    {
        result = Share(this, string);
        Append(this, result, code->GetString(suffix));
    }
    else
    {
        unsigned nsegments = GetSegmentCount(string);

        if (nsegments > 64)
        {
            Storage(this, string);
            nsegments = 1;
        }

        if (prefix != UINT_MAX && suffix != UINT_MAX)
            nsegments += 2;
        else
            nsegments += 1;

        unsigned total_length = length;

        if (prefix != UINT_MAX)
            total_length += Length(code->GetString(prefix));

        if (suffix != UINT_MAX)
            total_length += Length(code->GetString(suffix));

        if (total_length < 64)
        {
            result = JString::Make(this, total_length);

            uni_char *storage = Storage(this, result);

            if (prefix != UINT_MAX)
            {
                JString *string = code->GetString(prefix);

                op_memcpy(storage, Storage(this, string), UNICODE_SIZE(Length(string)));
                storage += Length(string);
            }

            if (Length(string) != 0)
            {
                op_memcpy(storage, Storage(this, string), UNICODE_SIZE(Length(string)));
                storage += Length(string);
            }

            if (suffix != UINT_MAX)
            {
                JString *string = code->GetString(suffix);

                op_memcpy(storage, Storage(this, string), UNICODE_SIZE(Length(string)));
            }
        }
        else
        {
            JStringSegmented *segmented = JStringSegmented::Make(this, nsegments);
            JStringStorage **bases = segmented->Bases();
            unsigned *offsets = segmented->Offsets();
            unsigned *lengths = segmented->Lengths();
            unsigned index = 0;

            if (prefix != UINT_MAX)
            {
                JString *string = code->GetString(prefix);
                OP_ASSERT(Length(string));

                bases[0] = string->value;
                offsets[0] = string->offset;
                length += lengths[0] = Length(string);

                ++index;
            }

            if (Length(string) != 0)
                if (IsSegmented(string))
                {
                    for (JSegmentIterator iter(string); iter.Next(); ++index)
                    {
                        bases[index] = iter.GetBase();
                        offsets[index] = iter.GetOffset();
                        lengths[index] = iter.GetLength();
                    }
                }
                else
                {
                    bases[index] = string->value;
                    offsets[index] = string->offset;
                    lengths[index] = Length(string);

                    ++index;
                }

            if (suffix != UINT_MAX)
            {
                JString *string = code->GetString(suffix);

                bases[index] = string->value;
                offsets[index] = string->offset;
                length += lengths[index] = Length(string);
            }

            result = JString::Make(this, segmented, length);
        }
    }

    Code()->format_string_caches[cache].from = string;
    Code()->format_string_caches[cache].to = result;

    reg[dst].SetString(result);

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_ADD_IMM(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Immediate src2 = lip[2].immediate;

    PROFILING_STORE_REGISTER_TYPE(src1);

    if (reg[src1].IsInt32())
    {
        INT32 isrc1 = reg[src1].GetInt32();
        INT32 result = isrc1 + src2;
        if ((isrc1 ^ src2) < 0 || (result ^ isrc1) >= 0)
        {
            reg[dst].SetInt32(result);
            goto profiling;
        }
    }

    if (reg[src1].IsNumber())
        reg[dst].SetNumber(reg[src1].GetNumAsDouble() + src2);
    else
    {
        START_USING_SCRATCH_VALUES;

        /* dst might be the same register as a src */
        src_val1 = reg[src1];
        src_val2 = ES_Value_Internal(src2);

        if (!AddSlow(&reg[dst], &src_val1, &src_val2, scratch_value_storage))
            IH_RETURN;
    }

profiling:
    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_END();

    IH_RETURN;
}

SLOW_CASE_RETURN_VALUE ES_Execution_Context::AddSlow(ES_Value_Internal *dst, ES_Value_Internal *src1, ES_Value_Internal *src2, ES_Value_Internal *scratch_value_storage)
{
    if (src1->IsString() && src2->IsString())
    add_strings:
        if (Length(src1->GetString()) == 0)
            dst->SetString(src2->GetString());
        else if (Length(src2->GetString()) == 0)
            dst->SetString(src1->GetString());
        else
        {
            JString *dst_str = Share(this, src1->GetString());
            dst->SetString(dst_str);
            Append(this, dst_str, src2->GetString());
        }
    else
    {
        if (src1->IsObject())
            if (!src1->ToPrimitive(this, ES_Value_Internal::HintNone))
            {
                END_USING_SCRATCH_VALUES;
                SLOW_CASE_HANDLE_THROW();
            }

        if (src2->IsObject())
            if (!src2->ToPrimitive(this, ES_Value_Internal::HintNone))
            {
                END_USING_SCRATCH_VALUES;
                SLOW_CASE_HANDLE_THROW();
            }

        if (src1->IsString() || src2->IsString())
        {
            if (!src1->ToString(this) || !src2->ToString(this)) // one of them is already string so no need to lock
            {
                END_USING_SCRATCH_VALUES;
                SLOW_CASE_HANDLE_THROW();
            }

            goto add_strings;
        }
        else if (!src1->ToNumber(this) || !src2->ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            SLOW_CASE_HANDLE_THROW();
        }
        else if (src1->IsInt32() && src2->IsInt32())
        {
            INT32 isrc1 = src1->GetInt32();
            INT32 isrc2 = src2->GetInt32();
            INT32 result = isrc1 + isrc2;
            if ((isrc1 ^ isrc2) < 0 || (result ^ isrc1) >= 0)
            {
                dst->SetInt32(result);
                END_USING_SCRATCH_VALUES;
                SLOW_CASE_RETURN();
            }
        }
        OP_ASSERT(src1->IsNumber() && src2->IsNumber());
        dst->SetNumber(src1->GetNumAsDouble() + src2->GetNumAsDouble());
    }

    END_USING_SCRATCH_VALUES;
    SLOW_CASE_RETURN();
}

IH_SPECIFIER ES_Execution_Context::IH_SUB(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Index src2 = lip[2].index;

    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);

    if (reg[src1].IsInt32() && reg[src2].IsInt32())
    {
        INT32 isrc1 = reg[src1].GetInt32();
        INT32 isrc2 = reg[src2].GetInt32();
        INT32 result = isrc1 - isrc2;
        if ((isrc1 ^ isrc2) >= 0 || (result ^ isrc1) >= 0)
        {
            reg[dst].SetInt32(result);
            goto profiling;
        }
    }

    if (reg[src1].IsNumber() && reg[src2].IsNumber())
    {
        reg[dst].SetNumber(SUB_DOUBLES(reg[src1].GetNumAsDouble(), reg[src2].GetNumAsDouble()));
        goto profiling;
    }
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];
        src_val2 = reg[src2];

        if (!SubSlow(&reg[dst], &src_val1, &src_val2, scratch_value_storage))
            IH_RETURN;
    }

profiling:
    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_RECORD_REGISTER_TYPE(src2, 3);
    PROFILING_END();

    IH_RETURN;
}

SLOW_CASE_RETURN_VALUE ES_Execution_Context::SubSlow(ES_Value_Internal *dst, ES_Value_Internal *src1, ES_Value_Internal *src2, ES_Value_Internal *scratch_value_storage)
{
    if (!src1->ToNumber(this) || !src2->ToNumber(this))
    {
        END_USING_SCRATCH_VALUES;
        SLOW_CASE_HANDLE_THROW();
    }
    else if (src1->IsInt32() && src2->IsInt32())
    {
        INT32 isrc1 = src1->GetInt32();
        INT32 isrc2 = src2->GetInt32();
        INT32 result = isrc1 - isrc2;
        if ((isrc1 ^ isrc2) >= 0 || (result ^ isrc1) >= 0)
            dst->SetInt32(result);
    }
    else
    {
        OP_ASSERT(src1->IsNumber() && src2->IsNumber());
        dst->SetNumber(src1->GetNumAsDouble() - src2->GetNumAsDouble());
    }

    END_USING_SCRATCH_VALUES;
    SLOW_CASE_RETURN();
}

IH_SPECIFIER ES_Execution_Context::IH_SUB_IMM(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Immediate src2 = lip[2].immediate;

    PROFILING_STORE_REGISTER_TYPE(src1);

    if (reg[src1].IsInt32())
    {
        INT32 isrc1 = reg[src1].GetInt32();
        INT32 result = isrc1 - src2;
        if ((isrc1 ^ src2) >= 0 || (result ^ isrc1) >= 0)
        {
            reg[dst].SetInt32(result);
            goto profiling;
        }
    }

    if (reg[src1].IsNumber())
        reg[dst].SetNumber(reg[src1].GetNumAsDouble() - src2);
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];

        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            reg[dst].SetNumber(src_val1.GetNumAsDouble() - src2);
            END_USING_SCRATCH_VALUES;
        }
    }

profiling:
    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_MUL(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Index src2 = lip[2].index;

    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);

    if (reg[src1].IsInt32() && reg[src2].IsInt32())
    {
        int i1 = reg[src1].GetInt32();
        int i2 = reg[src2].GetInt32();

        const int limit = 46341;

        // (i1 | i2) >> 15 == 0
        if (i1 < limit && i1 > -limit && i2 < limit && i2 > -limit)
        {
            int result = i1 * i2;
            if (result != 0 || (i1 < 0) == (i2 < 0))
            {
                reg[dst].SetInt32(result);
                goto profiling;
            }
            // result should be negative zero
        }
    }

    if (reg[src1].IsDouble() && reg[src2].IsDouble())
    {
        reg[dst].SetNumber(MUL_DOUBLES(reg[src1].GetDouble(), reg[src2].GetDouble()));
        goto profiling;
    }

    if (reg[src1].IsNumber() && reg[src2].IsNumber())
    {
        reg[dst].SetNumber(MUL_DOUBLES(reg[src1].GetNumAsDouble(), reg[src2].GetNumAsDouble()));
        goto profiling;
    }

    START_USING_SCRATCH_VALUES;

    src_val1 = reg[src1];
    src_val2 = reg[src2];

    if (!MulSlow(&reg[dst], &src_val1, &src_val2, scratch_value_storage))
        IH_RETURN;

profiling:
    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_RECORD_REGISTER_TYPE(src2, 3);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_MUL_IMM(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    int src2 = lip[2].immediate;

    PROFILING_STORE_REGISTER_TYPE(src1);

    if (reg[src1].IsInt32())
    {
        int i1 = reg[src1].GetInt32();

        const int limit = 46341;

        // (i1 | src2) >> 15 == 0
        if (i1 < limit && i1 > -limit && src2 < limit && src2 > -limit)
        {
            int result = i1 * src2;
            if (result != 0 || (i1 < 0) == (src2 < 0))
            {
                reg[dst].SetInt32(result);
                goto profiling;
            }
            // result should be negative zero
        }
    }

    if (reg[src1].IsDouble())
    {
        reg[dst].SetNumber(MUL_DOUBLES(reg[src1].GetDouble(), src2));
        goto profiling;
    }

    if (reg[src1].IsNumber())
    {
        reg[dst].SetNumber(MUL_DOUBLES(reg[src1].GetNumAsDouble(), src2));
        goto profiling;
    }

    START_USING_SCRATCH_VALUES;

    src_val1 = reg[src1];
    src_val2.SetInt32(src2);

    if (!MulSlow(&reg[dst], &src_val1, &src_val2, scratch_value_storage))
        IH_RETURN;

profiling:
    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_END();

    IH_RETURN;
}

SLOW_CASE_RETURN_VALUE ES_Execution_Context::MulSlow(ES_Value_Internal *dst, ES_Value_Internal *src1, ES_Value_Internal *src2, ES_Value_Internal *scratch_value_storage)
{
    if (!src1->ToNumber(this) || !src2->ToNumber(this))
    {
        END_USING_SCRATCH_VALUES;
        SLOW_CASE_HANDLE_THROW();
    }
    else
        dst->SetNumber(src1->GetNumAsDouble() * src2->GetNumAsDouble());

    END_USING_SCRATCH_VALUES;
    SLOW_CASE_RETURN();
}

IH_SPECIFIER ES_Execution_Context::IH_DIV(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Index src2 = lip[2].index;

    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);

    double result;

    if (reg[src1].IsNumber() && reg[src2].IsNumber())
        result = DIV_DOUBLES(reg[src1].GetNumAsDouble(), reg[src2].GetNumAsDouble());
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];
        src_val2 = reg[src2];

        if (!src_val1.ToNumber(this) || !src_val2.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
            result = DIV_DOUBLES(src_val1.GetNumAsDouble(), src_val2.GetNumAsDouble());

        END_USING_SCRATCH_VALUES;
    }

    BOOL store_as_double = TRUE;

    if (reg[src2].IsInt32())
    {
        int divisor = reg[src2].GetNumAsInt32();
        if ((divisor & (divisor - 1)) == 0)
            store_as_double = FALSE;
    }

    if (store_as_double)
        reg[dst].SetDouble(result);
    else
        reg[dst].SetNumber(result);

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_RECORD_REGISTER_TYPE(src2, 3);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_DIV_IMM(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    int src2 = lip[2].immediate;

    PROFILING_STORE_REGISTER_TYPE(src1);

    if (reg[src1].IsNumber())
        reg[dst].SetNumber(DIV_DOUBLES(reg[src1].GetNumAsDouble(), src2));
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];

        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
            reg[dst].SetNumber(DIV_DOUBLES(src_val1.GetNumAsDouble(), src2));

        END_USING_SCRATCH_VALUES;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_REM(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Index src2 = lip[2].index;

    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);

    double d1 = 0;
    double d2 = 0;

    if (reg[src1].IsNumber() && reg[src2].IsNumber())
    {
        d1 = reg[src1].GetNumAsDouble();
        d2 = reg[src2].GetNumAsDouble();
    }
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];
        src_val2 = reg[src2];

        if (!src_val1.ToNumber(this) || !src_val2.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            d1 = src_val1.GetNumAsDouble();
            d2 = src_val2.GetNumAsDouble();
        }

        END_USING_SCRATCH_VALUES;
    }

    double r;
    if (op_isnan(d1) || op_isnan(d2) || op_isinf(d1) || d2 == 0)
        r = op_nan(NULL);
    else if (op_isinf(d2) || d1 == 0)
        r = d1;
    else
        r = op_fmod(d1, d2);

    reg[dst].SetNumber(r);

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_RECORD_REGISTER_TYPE(src2, 3);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_REM_IMM(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    int src2 = lip[2].immediate;

    PROFILING_STORE_REGISTER_TYPE(src1);

    double d1 = 0;
    double d2 = src2;

    if (reg[src1].IsNumber())
        d1 = reg[src1].GetNumAsDouble();
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];

        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
            d1 = src_val1.GetNumAsDouble();

        END_USING_SCRATCH_VALUES;
    }

    double r;
    if (op_isnan(d1) || op_isinf(d1) || d2 == 0)
        r = op_nan(NULL);
    else if (d1 == 0)
        r = d1;
    else
        r = op_fmod(d1, d2);

    reg[dst].SetNumber(r);

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_LSHIFT(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;
    ES_CodeWord::Index count = lip[2].index;

    PROFILING_STORE_REGISTER_TYPE(src);
    PROFILING_STORE_REGISTER_TYPE(count);

    if (reg[src].IsInt32() && reg[count].IsInt32())
        reg[dst].SetInt32(reg[src].GetInt32() << (static_cast<UINT32> (reg[count].GetInt32()) & 0x1F));
    else if (reg[src].IsNumber() && reg[count].IsNumber())
        reg[dst].SetNumber(reg[src].GetNumAsInt32() << (reg[count].GetNumAsUInt32() & 0x1F));
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src];
        src_val2 = reg[count];

        if (!src_val1.ToNumber(this) || !src_val2.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
            reg[dst].SetNumber(src_val1.GetNumAsInt32() << (src_val2.GetNumAsUInt32() & 0x1F));

        END_USING_SCRATCH_VALUES;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src, 2);
    PROFILING_RECORD_REGISTER_TYPE(count, 3);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_RSHIFT_SIGNED(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;
    ES_CodeWord::Index count = lip[2].index;

    PROFILING_STORE_REGISTER_TYPE(src);
    PROFILING_STORE_REGISTER_TYPE(count);

    if (reg[src].IsInt32() && reg[count].IsInt32())
        reg[dst].SetInt32(reg[src].GetInt32() >> (static_cast<UINT32> (reg[count].GetInt32()) & 0x1F));
    else if (reg[src].IsNumber() && reg[count].IsNumber())
        reg[dst].SetNumber(reg[src].GetNumAsInt32() >> (reg[count].GetNumAsUInt32() & 0x1F));
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src];
        src_val2 = reg[count];

        if (!src_val1.ToNumber(this) || !src_val2.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
            reg[dst].SetNumber(src_val1.GetNumAsInt32() >> (src_val2.GetNumAsUInt32() & 0x1F));

        END_USING_SCRATCH_VALUES;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src, 2);
    PROFILING_RECORD_REGISTER_TYPE(count, 3);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_RSHIFT_UNSIGNED(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;
    ES_CodeWord::Index count = lip[2].index;

    PROFILING_STORE_REGISTER_TYPE(src);
    PROFILING_STORE_REGISTER_TYPE(count);

    if (reg[src].IsInt32() && reg[count].IsInt32())
        reg[dst].SetUInt32(static_cast<UINT32>(reg[src].GetInt32()) >> (static_cast<UINT32>(reg[count].GetInt32()) & 0x1F));
    else if (reg[src].IsNumber() && reg[count].IsNumber())
        reg[dst].SetUInt32(reg[src].GetNumAsUInt32() >> (reg[count].GetNumAsUInt32() & 0x1F));
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src];
        src_val2 = reg[count];

        if (!src_val1.ToNumber(this) || !src_val2.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
            reg[dst].SetNumber(src_val1.GetNumAsUInt32() >> (src_val2.GetNumAsUInt32() & 0x1F));

        END_USING_SCRATCH_VALUES;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src, 2);
    PROFILING_RECORD_REGISTER_TYPE(count, 3);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_LSHIFT_IMM(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;
    ES_CodeWord::Immediate count = lip[2].immediate & 0x1F;

    PROFILING_STORE_REGISTER_TYPE(src);

    if (reg[src].IsInt32())
        reg[dst].SetInt32(reg[src].GetInt32() << count);
    else if (reg[src].IsNumber())
        reg[dst].SetNumber(reg[src].GetNumAsInt32() << count);
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src];
        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            reg[dst].SetNumber(src_val1.GetNumAsInt32() << count);
            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_RSHIFT_SIGNED_IMM(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;
    ES_CodeWord::Immediate count = lip[2].immediate & 0x1F;

    PROFILING_STORE_REGISTER_TYPE(src);

    if (reg[src].IsInt32())
        reg[dst].SetInt32(reg[src].GetInt32() >> count);
    else if (reg[src].IsNumber())
        reg[dst].SetNumber(reg[src].GetNumAsInt32() >> count);
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src];
        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            reg[dst].SetNumber(src_val1.GetNumAsInt32() >> count);
            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_RSHIFT_UNSIGNED_IMM(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;
    ES_CodeWord::Immediate count = lip[2].immediate & 0x1F;

    PROFILING_STORE_REGISTER_TYPE(src);

    if (reg[src].IsNumber())
        reg[dst].SetNumber(reg[src].GetNumAsUInt32() >> count);
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src];
        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            reg[dst].SetNumber(src_val1.GetNumAsUInt32() >> count);
            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_NEG(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;

    PROFILING_STORE_REGISTER_TYPE(src);

    if (reg[src].IsInt32() && reg[src].GetInt32() != 0 && reg[src].GetInt32() != INT_MIN)
        reg[dst].SetInt32(- reg[src].GetInt32());
    else if (reg[src].IsNumber())
        reg[dst].SetNumber(- reg[src].GetNumAsDouble());
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src];
        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
            reg[dst].SetNumber(-src_val1.GetNumAsDouble());

        END_USING_SCRATCH_VALUES;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_COMPL(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;

    PROFILING_STORE_REGISTER_TYPE(src);

    if (reg[src].IsInt32())
        reg[dst].SetInt32(~reg[src].GetInt32());
    else if (reg[src].IsNumber())
        reg[dst].SetInt32(~reg[src].GetNumAsInt32());
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src];
        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            reg[dst].SetNumber(~src_val1.GetNumAsInt32());
            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_INC(IH_PARAM_DECL)
{
    IH_SETUP(1);

    ES_CodeWord::Index dst_src = lip->index;

    if (reg[dst_src].IsInt32() && reg[dst_src].GetInt32() != INT_MAX)
        reg[dst_src].SetInt32(reg[dst_src].GetInt32() + 1);
    else if (reg[dst_src].IsNumber())
        reg[dst_src].SetNumber(reg[dst_src].GetNumAsDouble() + 1);
    else
    {
        if (!reg[dst_src].ToNumber(this))
            HANDLE_THROW();
        else
            reg[dst_src].SetNumber(reg[dst_src].GetNumAsDouble() + 1);
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_DEC(IH_PARAM_DECL)
{
    IH_SETUP(1);

    ES_CodeWord::Index dst_src = lip->index;

    if (reg[dst_src].IsInt32() && reg[dst_src].GetInt32() != INT_MIN)
        reg[dst_src].SetInt32(reg[dst_src].GetInt32() - 1);
    else if (reg[dst_src].IsNumber())
        reg[dst_src].SetNumber(reg[dst_src].GetNumAsDouble() - 1);
    else
    {
        if (!reg[dst_src].ToNumber(this))
            HANDLE_THROW();
        else
            reg[dst_src].SetNumber(reg[dst_src].GetNumAsDouble() - 1);
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_BITAND(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Index src2 = lip[2].index;

    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);

    if (reg[src1].IsInt32() && reg[src2].IsInt32())
        reg[dst].SetInt32(reg[src1].GetInt32() & reg[src2].GetInt32());
    else if (reg[src1].IsNumber() && reg[src2].IsNumber())
        reg[dst].SetInt32(reg[src1].GetNumAsInt32() & reg[src2].GetNumAsInt32());
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];
        src_val2 = reg[src2];

        if (!src_val1.ToNumber(this) || !src_val2.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
            reg[dst].SetInt32(src_val1.GetNumAsInt32() & src_val2.GetNumAsInt32());

        END_USING_SCRATCH_VALUES;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_RECORD_REGISTER_TYPE(src2, 3);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_BITAND_IMM(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Immediate src2 = lip[2].immediate;

    PROFILING_STORE_REGISTER_TYPE(src1);

    if (reg[src1].IsInt32())
        reg[dst].SetInt32(reg[src1].GetInt32() & src2);
    else if (reg[src1].IsNumber())
        reg[dst].SetInt32(reg[src1].GetNumAsInt32() & src2);
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];

        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            reg[dst].SetInt32(src_val1.GetNumAsInt32() & src2);
            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_BITOR(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Index src2 = lip[2].index;

    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);

    if (reg[src1].IsInt32() && reg[src2].IsInt32())
        reg[dst].SetInt32(reg[src1].GetInt32() | reg[src2].GetInt32());
    else if (reg[src1].IsNumber() && reg[src2].IsNumber())
        reg[dst].SetInt32(reg[src1].GetNumAsInt32() | reg[src2].GetNumAsInt32());
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];
        src_val2 = reg[src2];

        if (!src_val1.ToNumber(this) || !src_val2.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
            reg[dst].SetInt32(src_val1.GetNumAsInt32() | src_val2.GetNumAsInt32());

        END_USING_SCRATCH_VALUES;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_RECORD_REGISTER_TYPE(src2, 3);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_BITOR_IMM(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Immediate src2 = lip[2].immediate;

    PROFILING_STORE_REGISTER_TYPE(src1);

    if (reg[src1].IsInt32())
        reg[dst].SetInt32(reg[src1].GetInt32() | src2);
    else if (reg[src1].IsNumber())
        reg[dst].SetInt32(reg[src1].GetNumAsInt32() | src2);
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];

        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            reg[dst].SetInt32(src_val1.GetNumAsInt32() | src2);
            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_BITXOR(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Index src2 = lip[2].index;

    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);

    if (reg[src1].IsInt32() && reg[src2].IsInt32())
        reg[dst].SetInt32(reg[src1].GetInt32() ^ reg[src2].GetInt32());
    else if (reg[src1].IsNumber() && reg[src2].IsNumber())
        reg[dst].SetInt32(reg[src1].GetNumAsInt32() ^ reg[src2].GetNumAsInt32());
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];
        src_val2 = reg[src2];

        if (!src_val1.ToNumber(this) || !src_val2.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
            reg[dst].SetInt32(src_val1.GetNumAsInt32() ^ src_val2.GetNumAsInt32());

        END_USING_SCRATCH_VALUES;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_RECORD_REGISTER_TYPE(src2, 3);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_BITXOR_IMM(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src1 = lip[1].index;
    ES_CodeWord::Immediate src2 = lip[2].index;

    PROFILING_STORE_REGISTER_TYPE(src1);

    if (reg[src1].IsInt32())
        reg[dst].SetInt32(reg[src1].GetInt32() ^ src2);
    else if (reg[src1].IsNumber())
        reg[dst].SetInt32(reg[src1].GetNumAsInt32() ^ src2);
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];

        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            reg[dst].SetInt32(src_val1.GetNumAsInt32() ^ src2);
            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(dst);
    PROFILING_RECORD_REGISTER_TYPE(dst, 1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_NOT(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index src = lip[1].index;

    if (reg[src].IsBoolean())
        reg[dst].SetBoolean(!reg[src].GetBoolean());
    else
        reg[dst].SetBoolean(!reg[src].AsBoolean().GetBoolean());

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_EQ(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Index src2 = lip[1].index;

    START_USING_SCRATCH_VALUES;

    src_val1 = reg[src1];
    src_val2 = reg[src2];

    if (!Eq(&src_val1, &src_val2))
    {
        END_USING_SCRATCH_VALUES;
        HANDLE_THROW();
        IH_RETURN;
    }

    END_USING_SCRATCH_VALUES;

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_RECORD_REGISTER_TYPE(src2, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_EQ_IMM(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Immediate src2 = lip[1].immediate;

    switch (reg[src1].Type())
    {
    case ESTYPE_INT32:
        implicit_bool = reg[src1].GetInt32() == src2;
        break;
    case ESTYPE_DOUBLE:
        implicit_bool = reg[src1].GetDouble() == src2;
        break;
    default:
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];
        src_val2 = src2;

        if (!EqSlow(&src_val1, &src_val2))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }

        END_USING_SCRATCH_VALUES;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_NEQ(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Index src2 = lip[1].index;

    START_USING_SCRATCH_VALUES;

    src_val1 = reg[src1];
    src_val2 = reg[src2];

    if (!Eq(&src_val1, &src_val2))
    {
        END_USING_SCRATCH_VALUES;
        HANDLE_THROW();
        IH_RETURN;
    }

    implicit_bool = !implicit_bool;

    END_USING_SCRATCH_VALUES;

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_RECORD_REGISTER_TYPE(src2, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_NEQ_IMM(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Immediate src2 = lip[1].immediate;

    switch (reg[src1].Type())
    {
    case ESTYPE_INT32:
        implicit_bool = reg[src1].GetInt32() == src2;
        break;
    case ESTYPE_DOUBLE:
        implicit_bool = reg[src1].GetDouble() == src2;
        break;
    default:
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];
        src_val2 = src2;

        if (!EqSlow(&src_val1, &src_val2))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }

        END_USING_SCRATCH_VALUES;
    }
    implicit_bool = !implicit_bool;

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_END();

    IH_RETURN;
}

#ifdef ES_NATIVE_SUPPORT

/* static */ int ES_CALLING_CONVENTION
ES_Execution_Context::EqFromMachineCode(ES_Execution_Context *context, ES_Value_Internal *src1, ES_Value_Internal *src2, unsigned cw_index)
{
    ES_Value_Internal *scratch_value_storage;

    context->StartUsingScratchRegisters(scratch_value_storage);

    context->reg = ES_NativeStackFrame::GetRegisterFrame(context->native_stack_frame);
    context->src_val1 = *src1;
    context->src_val2 = *src2;

    if (!context->Eq(&context->src_val1, &context->src_val2))
    {
        context->EndUsingScratchRegisters(scratch_value_storage);
        ThrowFromMachineCode(context);
    }

    context->EndUsingScratchRegisters(scratch_value_storage);
    return context->implicit_bool;
}

/* static */ void *ES_CALLING_CONVENTION
ES_Execution_Context::UpdateComparison(ES_Execution_Context *context, unsigned cw_index)
{
    ES_Value_Internal *reg = context->Registers();
    ES_Code *code = context->Code();
    ES_CodeWord *codewords = code->data->codewords;

    if (!code->data->profile_data)
        context->AllocateProfileData();

    unsigned char *pd = code->data->profile_data + cw_index;
    unsigned bits1 = reg[codewords[cw_index + 1].index].TypeBits();
    unsigned bits2 = reg[codewords[cw_index + 2].index].TypeBits();

    if ((pd[1] & bits1) == 0)
    {
        pd[1] |= bits1;
        pd[0] = 1;
    }
    if ((pd[2] & bits2) == 0)
    {
        pd[2] |= bits2;
        pd[0] = 1;
    }
    if (pd[0])
        ++code->slow_case_calls;

    return UpdateNativeDispatcher(context, cw_index);
}

#endif // ES_NATIVE_SUPPORT

inline BOOL
ES_Execution_Context::Eq(ES_Value_Internal *src1, ES_Value_Internal *src2)
{
    ES_ValueType type1 = src1->TypeTag();
    if (type1 == src2->TypeTag())
        switch (type1)
        {
        case ESTYPE_UNDEFINED:
        case ESTYPE_NULL:
            implicit_bool = TRUE;
            break;
        default:
#ifndef ES_VALUES_AS_NANS
            OP_ASSERT(!"Unknown typetag");
            implicit_bool = FALSE;
            break;
        case ESTYPE_DOUBLE:
#endif // ES_VALUES_AS_NANS
            implicit_bool = EqDouble(src1->GetDouble(), src2->GetDouble());
            break;
        case ESTYPE_INT32:
            implicit_bool = src1->GetInt32() == src2->GetInt32();
            break;
        case ESTYPE_BOOLEAN:
            implicit_bool = src1->GetBoolean() == src2->GetBoolean();
            break;
        case ESTYPE_STRING:
            implicit_bool = Equals(src1->GetString(), src2->GetString());
            break;
        case ESTYPE_OBJECT:
            if (src1->GetObject() == src2->GetObject())
                implicit_bool = TRUE;
            else
                implicit_bool = src1->GetObject(this) == src2->GetObject(this);
            break;
        }
    else
    {
        // Handle the case -0 == 0 where -0 is double and 0 is int32.
        if (src1->IsDouble() && src2->IsInt32())
            implicit_bool = EqDouble(src1->GetDouble(), INT2DOUBLE(src2->GetInt32()));
        else if (src1->IsInt32() && src2->IsDouble())
            implicit_bool = EqDouble(INT2DOUBLE(src1->GetInt32()), src2->GetDouble());
        else if (src1->IsObject() && src2->IsNull() || src1->IsNull() && src2->IsObject())
            implicit_bool = FALSE;
        else
            return EqSlow(src1, src2);
    }

    return TRUE;
}

BOOL
ES_Execution_Context::EqSlow(ES_Value_Internal *src1, ES_Value_Internal *src2)
{
    if (src1->IsNull() && src2->IsUndefined() || src1->IsUndefined() && src2->IsNull())
    {
        implicit_bool = TRUE;
    }
    else if (src1->IsNumber() && src2->IsString())
    {
        if (!src2->ToNumber(this))
            return FALSE;
        else
            return Eq(src1, src2);
    }
    else if (src1->IsString() && src2->IsNumber())
    {
        if (!src1->ToNumber(this))
            return FALSE;
        else
            return Eq(src1, src2);
    }
    else if (src1->IsBoolean())
    {
        if (!src1->ToNumber(this))
            return FALSE;
        else
            return Eq(src1, src2);
    }
    else if (src2->IsBoolean())
    {
        if (!src2->ToNumber(this))
            return FALSE;
        else
            return Eq(src1, src2);
    }
    else if ((src1->IsString() || src1->IsNumber()) && src2->IsObject())
    {
        /* 11.9.3.8: ToPrimitive(no-hint) */
        if (!src2->ToPrimitive(this, ES_Value_Internal::HintNone))
            return FALSE;
        else
            return Eq(src1, src2);
    }
    else if (src1->IsObject() && (src2->IsString() || src2->IsNumber()))
    {
        /* 11.9.3.9: ToPrimitive(no-hint) */
        if (!src1->ToPrimitive(this, ES_Value_Internal::HintNone))
            return FALSE;
        else
            return Eq(src1, src2);
    }
    else if (src1->IsNullOrUndefined())
        implicit_bool = src2->IsHiddenObject(this);
    else if (src2->IsNullOrUndefined())
        implicit_bool = src1->IsHiddenObject(this);
    else
        implicit_bool = FALSE;

    return TRUE;
}

IH_SPECIFIER ES_Execution_Context::IH_EQ_STRICT(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Index src2 = lip[1].index;

    implicit_bool = EqStrict(this, &reg[src1], &reg[src2]);

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_RECORD_REGISTER_TYPE(src2, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_EQ_STRICT_IMM(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index op = lip[0].index;
    ES_Value_Internal &src1 = reg[op];
    ES_CodeWord::Immediate src2 = lip[1].immediate;

    switch (src1.Type())
    {
    case ESTYPE_INT32:
        implicit_bool = src1.GetInt32() == src2;
        break;
    case ESTYPE_DOUBLE:
        implicit_bool = src1.GetDouble() == src2;
        break;
    default:
        implicit_bool = FALSE;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(op);
    PROFILING_RECORD_REGISTER_TYPE(op, 1);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_NEQ_STRICT(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Index src2 = lip[1].index;

    implicit_bool = !EqStrict(this, &reg[src1], &reg[src2]);

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_RECORD_REGISTER_TYPE(src2, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_NEQ_STRICT_IMM(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index op = lip[0].index;
    ES_Value_Internal &src1 = reg[op];
    ES_CodeWord::Immediate src2 = lip[1].immediate;

    switch (src1.Type())
    {
    case ESTYPE_INT32:
        implicit_bool = src1.GetInt32() != src2;
        break;
    case ESTYPE_DOUBLE:
        implicit_bool = src1.GetDouble() != src2;
        break;
    default:
        implicit_bool = TRUE;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(op);
    PROFILING_RECORD_REGISTER_TYPE(op, 1);
    PROFILING_END();

    IH_RETURN;
}

/* static */ int ES_CALLING_CONVENTION
ES_Execution_Context::EqStrict(ES_Execution_Context *context, ES_Value_Internal *src1, ES_Value_Internal *src2)
{
    ES_ValueType type1 = src1->TypeTag();
    if (type1 == src2->TypeTag())
        switch (type1)
        {
        case ESTYPE_UNDEFINED:
        case ESTYPE_NULL:
            return TRUE;
        default:
#ifndef ES_VALUES_AS_NANS
            OP_ASSERT(!"Unknown typetag");
            return FALSE;
        case ESTYPE_DOUBLE:
#endif // ES_VALUES_AS_NANS
            return EqDouble(src1->GetDouble(), src2->GetDouble());
        case ESTYPE_INT32:
            return src1->GetInt32() == src2->GetInt32();
        case ESTYPE_BOOLEAN:
            return src1->GetBoolean() == src2->GetBoolean();
        case ESTYPE_STRING:
            return Equals(src1->GetString(), src2->GetString());
        case ESTYPE_OBJECT:
            return src1->GetObject(context) == src2->GetObject(context);
        }
    else
    {
        // Handle the case -0 == 0 where -0 is double and 0 is int32.
        if (src1->IsDouble())
            if (src2->IsInt32())
                return EqDouble(src1->GetDouble(), INT2DOUBLE(src2->GetInt32()));
#ifdef ES_VALUES_AS_NANS
            else if (src2->IsDouble())
                return EqDouble(src1->GetDouble(), src2->GetDouble());
#endif // ES_VALUES_AS_NANS
            else
                return FALSE;
        else if (src1->IsInt32() && src2->IsDouble())
            return EqDouble(INT2DOUBLE(src1->GetInt32()), src2->GetDouble());
        else if (src1->IsUndefined() && src2->IsObject() && src2->GetObject()->IsHiddenObject())
            return TRUE;
        else if (src2->IsUndefined() && src1->IsObject() && src1->GetObject()->IsHiddenObject())
            return TRUE;
        else
            return FALSE;
    }
}

IH_SPECIFIER ES_Execution_Context::IH_LT(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Index src2 = lip[1].index;

    if (reg[src1].IsInt32() && reg[src2].IsInt32())
        implicit_bool = reg[src1].GetInt32() < reg[src2].GetInt32();
    else
    {
        if (reg[src1].IsNumber() && reg[src2].IsNumber())
            RelationalNumberComparision(implicit_bool, reg[src1], reg[src2], FALSE);
        else
        {
            START_USING_SCRATCH_VALUES;

            src_val1 = reg[src1];
            src_val2 = reg[src2];

            if (!RelationalComparision(implicit_bool, &src_val1, &src_val2, TRUE, FALSE))
            {
                END_USING_SCRATCH_VALUES;
                HANDLE_THROW();
                IH_RETURN;
            }

            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_RECORD_REGISTER_TYPE(src2, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_LT_IMM(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Immediate src2 = lip[1].immediate;

    if (reg[src1].IsInt32())
        implicit_bool = reg[src1].GetInt32() < src2;
    else if (reg[src1].IsNumber())
    {
#if defined ES_NEED_GUARD_AGAINST_NAN || defined NAN_EQUALS_EVERYTHING
        double d = reg[src1].GetNumAsDouble();
        if (op_isnan(d))
            implicit_bool = FALSE;
        else
            implicit_bool = d < src2;
#endif
        implicit_bool = reg[src1].GetNumAsDouble() < src2;
    }
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];

        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            RelationalNumberComparision(implicit_bool, src_val1, src2, FALSE);
            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_LTE(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Index src2 = lip[1].index;

    if (reg[src1].IsInt32() && reg[src2].IsInt32())
        implicit_bool = reg[src1].GetInt32() <= reg[src2].GetInt32();
    else
    {
        if (reg[src1].IsNumber() && reg[src2].IsNumber())
        {
            RelationalNumberComparision(implicit_bool, reg[src2], reg[src1], TRUE);
            implicit_bool = !implicit_bool;
        }
        else
        {
            START_USING_SCRATCH_VALUES;

            src_val1 = reg[src1];
            src_val2 = reg[src2];

            if (!RelationalComparision(implicit_bool, &src_val2, &src_val1, FALSE, TRUE))
            {
                END_USING_SCRATCH_VALUES;
                HANDLE_THROW();
                IH_RETURN;
            }

            implicit_bool = !implicit_bool;

            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_RECORD_REGISTER_TYPE(src2, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_LTE_IMM(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Immediate src2 = lip[1].immediate;

    if (reg[src1].IsInt32())
        implicit_bool = reg[src1].GetInt32() <= src2;
    else if (reg[src1].IsNumber())
    {
#if defined ES_NEED_GUARD_AGAINST_NAN || defined NAN_EQUALS_EVERYTHING
        double d = reg[src1].GetNumAsDouble();
        if (op_isnan(d))
            implicit_bool = FALSE;
        else
            implicit_bool = d <= src2;
#endif
        implicit_bool = reg[src1].GetNumAsDouble() <= src2;
    }
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];

        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            RelationalNumberComparision(implicit_bool, src2, src_val1, TRUE);
            END_USING_SCRATCH_VALUES;
        }

        implicit_bool = !implicit_bool;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_GT(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Index src2 = lip[1].index;

    if (reg[src1].IsInt32() && reg[src2].IsInt32())
        implicit_bool = reg[src1].GetInt32() > reg[src2].GetInt32();
    else
    {
        if (reg[src1].IsNumber() && reg[src2].IsNumber())
            RelationalNumberComparision(implicit_bool, reg[src2], reg[src1], FALSE);
        else
        {
            START_USING_SCRATCH_VALUES;

            src_val1 = reg[src1];
            src_val2 = reg[src2];

            if (!RelationalComparision(implicit_bool, &src_val2, &src_val1, FALSE, FALSE))
            {
                END_USING_SCRATCH_VALUES;
                HANDLE_THROW();
                IH_RETURN;
            }

            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_RECORD_REGISTER_TYPE(src2, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_GT_IMM(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Immediate src2 = lip[1].immediate;

    if (reg[src1].IsInt32())
        implicit_bool = reg[src1].GetInt32() > src2;
    else if (reg[src1].IsNumber())
    {
#if defined ES_NEED_GUARD_AGAINST_NAN || defined NAN_EQUALS_EVERYTHING
        double d = reg[src1].GetNumAsDouble();
        if (op_isnan(d))
            implicit_bool = FALSE;
        else
            implicit_bool = d > src2;
#endif
        implicit_bool = reg[src1].GetNumAsDouble() > src2;
    }
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];

        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            RelationalNumberComparision(implicit_bool, src2, src_val1, FALSE);
            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_GTE(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Index src2 = lip[1].index;

    if (reg[src1].IsInt32() && reg[src2].IsInt32())
        implicit_bool = reg[src1].GetInt32() >= reg[src2].GetInt32();
    else
    {
        if (reg[src1].IsNumber() && reg[src2].IsNumber())
        {
            RelationalNumberComparision(implicit_bool, reg[src1], reg[src2], TRUE);
            implicit_bool = !implicit_bool;
        }
        else
        {
            START_USING_SCRATCH_VALUES;

            src_val1 = reg[src1];
            src_val2 = reg[src2];

            if (!RelationalComparision(implicit_bool, &src_val1, &src_val2, TRUE, TRUE))
            {
                END_USING_SCRATCH_VALUES;
                HANDLE_THROW();
                IH_RETURN;
            }

            implicit_bool = !implicit_bool;

            END_USING_SCRATCH_VALUES;
        }
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_STORE_REGISTER_TYPE(src2);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_RECORD_REGISTER_TYPE(src2, 2);
    PROFILING_END();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_GTE_IMM(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index src1 = lip[0].index;
    ES_CodeWord::Immediate src2 = lip[1].immediate;

    if (reg[src1].IsInt32())
        implicit_bool = reg[src1].GetInt32() >= src2;
    else if (reg[src1].IsNumber())
    {
#if defined ES_NEED_GUARD_AGAINST_NAN || defined NAN_EQUALS_EVERYTHING
        double d = reg[src1].GetNumAsDouble();
        if (op_isnan(d))
            implicit_bool = FALSE;
        else
            implicit_bool = d >= src2;
#endif
        implicit_bool = reg[src1].GetNumAsDouble() >= src2;
    }
    else
    {
        START_USING_SCRATCH_VALUES;

        src_val1 = reg[src1];

        if (!src_val1.ToNumber(this))
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }
        else
        {
            RelationalNumberComparision(implicit_bool, src_val1, src2, TRUE);
            END_USING_SCRATCH_VALUES;
        }

        implicit_bool = !implicit_bool;
    }

    PROFILING_START();
    PROFILING_STORE_REGISTER_TYPE(src1);
    PROFILING_RECORD_REGISTER_TYPE(src1, 1);
    PROFILING_END();

    IH_RETURN;
}

void
ES_Execution_Context::RelationalNumberComparision(unsigned &result, ES_Value_Internal src1, ES_Value_Internal src2, BOOL inverted)
{
    OP_ASSERT(src1.IsNumber() && src2.IsNumber());

    if (src1.IsInt32() && src2.IsInt32())
        result = src1.GetInt32() < src2.GetInt32();
    else
    {
        double d1 = src1.GetNumAsDouble();
        double d2 = src2.GetNumAsDouble();
        if (op_isnan(d1) || op_isnan(d2))
            if (inverted)
                result = TRUE;
            else
                result = FALSE;
        else
            result = d1 < d2;
    }
}

BOOL
ES_Execution_Context::RelationalComparision(unsigned &result, ES_Value_Internal *src1, ES_Value_Internal *src2, BOOL left_first, BOOL inverted)
{
    /* Spec reference: 11.8.5 */
    if (left_first)
    {
        if (src1->IsObject() && !src1->ToPrimitive(this, ES_Value_Internal::HintNumber))
            return FALSE;
        else if (src2->IsObject() && !src2->ToPrimitive(this, ES_Value_Internal::HintNumber))
            return FALSE;
    }
    else
    {
        if (src2->IsObject() && !src2->ToPrimitive(this, ES_Value_Internal::HintNumber))
            return FALSE;
        else if (src1->IsObject() && !src1->ToPrimitive(this, ES_Value_Internal::HintNumber))
            return FALSE;
    }

    if (src1->IsString() && src2->IsString())
        result = Compare(this, src1->GetString(), src2->GetString()) < 0;
    else if (!src1->ToNumber(this) || !src2->ToNumber(this))
        return FALSE;
    else
        RelationalNumberComparision(result, *src1, *src2, inverted);

    return TRUE;
}

IH_SPECIFIER ES_Execution_Context::IH_CONDITION(IH_PARAM_DECL)
{
    IH_SETUP(1);

    ES_CodeWord::Index src = lip->index;

    if (reg[src].IsBoolean())
        implicit_bool = reg[src].GetBoolean();
    else if (reg[src].IsObject())
    {
        if (reg[src].GetObject()->IsHiddenObject())
        {
            reg[src].SetUndefined();
            implicit_bool = FALSE;
        }
        else
            implicit_bool = TRUE;
    }
    else
        implicit_bool = reg[src].AsBoolean().GetBoolean();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_JUMP(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    IH_SETUP(1);

    ES_CodeWord::Offset offset = lip->offset;

    IH_SET_CONTEXT_IP(lip + offset);

    if (--time_until_check == 0)
        CheckOutOfTime();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_JUMP_INDIRECT(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    IH_SETUP(1);

    ES_CodeWord::Index value = lip->index;

    IH_SET_CONTEXT_IP(code->data->codewords + reg[value].GetInt32());

    if (--time_until_check == 0)
        CheckOutOfTime();

    IH_RETURN;
}

#ifdef ES_NATIVE_SUPPORT

class ES_CreateNativeLoopDispatcher
    : public ES_SuspendedCall
{
public:
    ES_CreateNativeLoopDispatcher(ES_Execution_Context *context, ES_Code *code, unsigned loop_index)
        : nc(context, code),
          code(code),
          loop_index(loop_index),
          status(OpStatus::OK),
          success(FALSE)
    {
        context->SuspendedCall(this);
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        ES_CollectorLock gclock(context);

        TRAP(status, success = nc.CreateNativeLoopDispatcher(loop_index));
    }

private:
    ES_Native nc;
    ES_Code *code;
    unsigned loop_index;

public:
    OP_STATUS status;
    BOOL success;
};

#endif // ES_NATIVE_SUPPORT

IH_SPECIFIER ES_Execution_Context::IH_JUMP_IF_TRUE(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    IH_SETUP(2);

    ES_CodeWord::Offset offset = lip[0].offset;

    if (implicit_bool)
    {
        if (--time_until_check == 0)
            CheckOutOfTime();

#ifdef ES_NATIVE_SUPPORT
        if (offset < 0 && UseNativeDispatcher())
        {
            unsigned loop_index = lip[1].index;
            if (loop_index != UINT_MAX && code->data->loop_data && !code->is_strict_eval)
            {
                ES_Code *loop_dispatcher_code = code->loop_dispatcher_codes[loop_index];
                ES_NativeCodeBlock *code_block = loop_dispatcher_code ? loop_dispatcher_code->native_code_block : NULL;

                if (code_block && !code_block->IsLoopValid(code->global_object))
                {
                    code_block->Reset();
                    heap->DeprecateNativeDispatcher(code_block);
                    loop_dispatcher_code = NULL;
                }

            use_loop_dispatcher:
                if (loop_dispatcher_code && reg + loop_dispatcher_code->data->register_frame_size <= register_blocks.Limit())
                {
                    ES_Value_Internal *stored_reg = reg;
                    ES_Code *stored_code = code;

                    native_dispatcher_reg = reg;
                    code = loop_dispatcher_code;

#ifdef ES_STACK_RECONSTRUCTION_SUPPORTED
                    BOOL reconstruct_native_stack = TRUE;
#else // ES_STACK_RECONSTRUCTION_SUPPORTED
                    BOOL reconstruct_native_stack = FALSE;
#endif // ES_STACK_RECONSTRUCTION_SUPPORTED

                    BOOL success = CallBytecodeToNativeTrampoline(NULL, reg, loop_dispatcher_code->native_code_block, loop_dispatcher_code->native_dispatcher, 0, reconstruct_native_stack, FALSE);

                    reg = stored_reg;
                    code = stored_code;

                    if (!success)
                    {
                        HANDLE_THROW();
                        IH_RETURN;
                    }

                    IH_SET_CONTEXT_IP(lip + 2);
                    IH_RETURN;
                }
                else if (++code->loop_counters[loop_index] == 10) // FIXME: "10" is a bit hardcoded and arbitrary
                {
                    ES_CreateNativeLoopDispatcher suspended(this, code, loop_index);

                    if (suspended.success)
                    {
                        loop_dispatcher_code = code->loop_dispatcher_codes[loop_index];
                        goto use_loop_dispatcher;
                    }
                    else if (OpStatus::IsMemoryError(suspended.status))
                        AbortOutOfMemory();
                }
            }
            else if (code->type == ES_Code::TYPE_FUNCTION && code->data->CanHaveNativeDispatcher() && !in_constructor)
            {
                /* This would throw the regeneration mechanism off; but since
                   ESI_JUMP_IF_TRUE is always inlined in native code, it being
                   called means we're executing in bytecode mode, and the
                   regeneration mechanism won't come into play until we generate
                   native code in the first place, and when we do, the slow case
                   counter is reset. */
                code->slow_case_calls += -offset;

                if (code->ShouldGenerateNativeDispatcher())
                {
                    IH_SET_CONTEXT_IP(lip + offset);

                    ES_AnalyzeAndGenerateNativeDispatcher analyze_and_generate_native_dispatcher(this, static_cast<ES_Function *>(reg[1].GetObject()), ip);
                    if (OpStatus::IsMemoryError(analyze_and_generate_native_dispatcher.status))
                        AbortOutOfMemory();

                    if (code->native_dispatcher)
                    {
                        ES_Value_Internal *stored_reg = reg;
                        ES_Code *stored_code = code;
                        unsigned stored_overlap = overlap;
                        BOOL stored_first_in_block = first_in_block;

                        native_dispatcher_reg = reg;

#ifdef ES_STACK_RECONSTRUCTION_SUPPORTED
                        BOOL reconstruct_native_stack = TRUE;
#else // ES_STACK_RECONSTRUCTION_SUPPORTED
                        BOOL reconstruct_native_stack = FALSE;
#endif // ES_STACK_RECONSTRUCTION_SUPPORTED

                        BOOL native_success = CallBytecodeToNativeTrampoline(NULL, reg, analyze_and_generate_native_dispatcher.native_code_block, analyze_and_generate_native_dispatcher.entry_point, argc, reconstruct_native_stack, FALSE);

                        reg = stored_reg;
                        code = stored_code;

                        if (!reconstruct_native_stack)
                            register_blocks.Free(stored_code->data->register_frame_size, stored_overlap, 1, stored_first_in_block);
                        else
                        {
                            ES_FrameStackIterator frames(this);

                            frames.Next();
                            frames.Next();

                            register_blocks.Free(frames.GetVirtualFrame()->next_frame_size, overlap, 1, first_in_block);
                        }

                        frame_stack.Pop(this);

                        if (!native_success)
                            if (ip->instruction != ESI_EXIT_TO_BUILTIN && ip->instruction != ESI_EXIT_TO_EVAL && ip->instruction != ESI_EXIT)
                                HANDLE_THROW();

                        IH_RETURN;
                    }
                }
            }
        }
#endif // ES_NATIVE_SUPPORT

        IH_SET_CONTEXT_IP(lip + offset);
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_JUMP_IF_FALSE(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    IH_SETUP(2);

    ES_CodeWord::Offset offset = lip[0].offset;

    if (!implicit_bool)
    {
        IH_SET_CONTEXT_IP(lip + offset);

        if (--time_until_check == 0)
            CheckOutOfTime();
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_START_LOOP(IH_PARAM_DECL)
{
    IH_SETUP(1);
    (void)lip;
    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_STORE_BOOLEAN(IH_PARAM_DECL)
{
    IH_SETUP(1);

    ES_CodeWord::Index dst = lip->index;

    reg[dst].SetBoolean(!!implicit_bool);

    IH_RETURN;
}

#ifdef ES_DEBUG_COMPACT_OBJECTS
static BOOL
DebugPropertyCache(ES_Code::PropertyCache *debug_cache)
{
    BOOL valid_cache = TRUE;
    unsigned debug_class_id = debug_cache->class_id;
    ES_Boxed *debug_data = debug_cache->data.boxed;
    BOOL is_negative = debug_cache->IsNegativeCache();
    debug_cache = debug_cache->next;
    ES_Code::PropertyCache *debug_cache_next = NULL;

    while (debug_cache && valid_cache)
    {
        debug_cache_next = debug_cache->next;
        ES_Code::PropertyCache *debug_debug_cache = debug_cache;

        if (debug_debug_cache && debug_debug_cache->class_id == debug_class_id)
        {
            valid_cache = debug_data != debug_debug_cache->data.boxed ||
                          ((is_negative || debug_cache->IsNegativeCache()) && is_negative != debug_cache->IsNegativeCache());

            debug_debug_cache = debug_debug_cache->next;
        }

        while (debug_debug_cache && valid_cache)
        {
            valid_cache = debug_class_id != debug_debug_cache->class_id;
            debug_debug_cache = debug_debug_cache->next;
        }

        debug_class_id = debug_cache->class_id;
        debug_data = debug_cache->data.boxed;
        is_negative = debug_cache->IsNegativeCache();
        debug_cache = debug_cache_next;
    }

    return valid_cache;
}
#endif // ES_DEBUG_COMPACT_OBJECTS

static void
RemoveStaleCacheRecord(ES_Execution_Context *context, ES_Code *code, BOOL is_get, ES_CodeWord::Index index, ES_Code::PropertyCache *cache)
{
    if (cache)
    {
        if (cache->next && cache->next->class_id == cache->class_id)
            code->RemoveCacheRecord(context, is_get, index, cache->next);
        code->RemoveCacheRecord(context, is_get, index, cache);
    }
}

BOOL ES_Execution_Context::IH_GETN_IMM_UNCACHED(ES_CodeWord *lip, ES_CodeWord::Index dst, ES_CodeWord::Index obj, ES_Code::PropertyCache *reuse_cache)
{
    JString *id = String(lip[2].index);

    ES_Object *object, *resolved_object;
    ES_Class *object_class;

    START_USING_SCRATCH_VALUES;

    unsigned cache_limit, cached_offset;
    if (reg[obj].IsObject())
    {
        resolved_object = reg[obj].GetObject(this);
        object = reg[obj].GetObject();
        object_class = object->Class();
        cache_limit = resolved_object->Count();
    }
    else if (reg[obj].AsObject(this, src_val1, FALSE))
    {
        resolved_object = object = src_val1.GetObject();
        object_class = GetGlobalObject()->GetPrimitiveClass(reg[obj]);
        cache_limit = GetGlobalObject()->GetPrimitivePrototype(reg[obj])->Count();

        PROFILING_START();
        PROFILING_STORE_REGISTER_TYPE(obj);
        PROFILING_RECORD_REGISTER_TYPE(obj, 2);
        PROFILING_END_WEIGHTED(1);
    }
    else
    {
        ThrowErrorBase(lip - 1, TYPE_ERROR, EXPECTED_OBJECT);

        END_USING_SCRATCH_VALUES;
        return FALSE;
    }

    ES_Object *prototype_object;
    ES_PropertyIndex cached_index;

#ifdef ES_DEBUG_COMPACT_OBJECTS
    extern void DebugVerifyObject(ES_Object *object);
    DebugVerifyObject(resolved_object);
#endif // ES_DEBUG_COMPACT_OBJECTS

    GetResult res = resolved_object->GetL(this, reg[obj], id, reg[dst], prototype_object, cached_index);

#ifdef ES_DEBUG_COMPACT_OBJECTS
    DebugVerifyObject(resolved_object);
#endif // ES_DEBUG_COMPACT_OBJECTS

#ifdef DISABLE_PROPERTY_READ_CACHE
    switch (GET_CANNOT_CACHE(res))
#else
    switch (res)
#endif // DISABLE_PROPERTY_READ_CACHE
    {
    default:
        RemoveStaleCacheRecord(this, code, TRUE, lip[3].index, reuse_cache);
        break;

    case PROP_GET_NOT_FOUND_CAN_CACHE:
        cached_index = ES_PropertyIndex(UINT_MAX);
        reg[dst].SetUndefined();

    case PROP_GET_OK_CAN_CACHE:
        if (resolved_object != object && object->HasMultipleIdentities())
            if (object->IsProxyCachingDisabled())
            {
                END_USING_SCRATCH_VALUES;
                return TRUE;
            }
            else
            {
                if (!object->IsProxyInstanceAdded())
                {
                    resolved_object->Class()->AddInstance(this, object->Class()->GetRootClass());
                    object->SetProxyInstanceAdded();
                }

                if (!prototype_object && cached_index != UINT_MAX)
                    prototype_object = resolved_object;
            }

        ES_StorageType cached_type;

        BOOL negative_cache = cached_index == UINT_MAX;
        if (!negative_cache)
        {
            ES_Class *klass = prototype_object ? prototype_object->Class() : object_class;
            ES_Property_Info info = klass->GetPropertyInfoAtIndex(cached_index);
            ES_Layout_Info layout = klass->GetLayoutInfoAtIndex(info.Index());
            cached_offset = layout.GetOffset();
            cached_type = layout.GetStorageType();
            cache_limit = prototype_object == NULL ? info.Index() : cache_limit;
        }
        else
        {
            cached_type = ES_STORAGE_WHATEVER;
            cached_offset = UINT_MAX;

            ES_Property_Info info;
            if (object_class->Find(id, info, object_class->Count()))
                cache_limit = info.Index();
            else
                cache_limit = object_class->Count();
        }

        if (!ES_Code::CanCache(cached_offset, cache_limit, negative_cache))
        {
            END_USING_SCRATCH_VALUES;
            return TRUE;
        }

        if (reuse_cache)
        {
            /* Paired caches should never fail, so assert that the reuse
               cache isn't a paired cache. */
            OP_ASSERT(!reuse_cache->next || reuse_cache->next->class_id != reuse_cache->class_id);

            BOOL negative_cache = reuse_cache->IsNegativeCache() || reuse_cache->data.prototype_object;

            /* We should never reuse a cache with the wrong class id, those
               caches should instead be removed and re-allocated. Should be
               fast enough using the property cache cache. */
            OP_ASSERT(reuse_cache->class_id == ES_Class::ActualClass(object_class)->Id());

            /* We should never reuse a positive cache when having a positive
               lookup, in that case the class id check should've failed. */
            OP_ASSERT(negative_cache || cached_index == UINT_MAX || prototype_object);

            /* If we're caching a negative lookup or a lookup in the prototype,
               and we see a negative cache it means that the limit check failed
               for the cache, in this case the cache is still valid, it is just
               the limit check that is too restrictive. Then we're free to just
               reuse the cache.

               If the cache is negative but we're caching a positive or vice
               versa, then we should insert a new cache, keeping the invariant
               that the negative cache should come before a postive cache. (N.B.
               we say negative and mean negative or in the prototype.) */

            if (!negative_cache || cached_index != UINT_MAX && !prototype_object)
            {
                ES_Code::PropertyCache *new_cache = AllocatePropertyCache();
                if (!new_cache)
                    AbortOutOfMemory();

                if (!negative_cache)
                {
                    *new_cache = *reuse_cache;
                    reuse_cache->next = new_cache;
                }
                else
                {
                    new_cache->next = reuse_cache->next;
                    reuse_cache->next = new_cache;
                    reuse_cache = new_cache;
                }
            }
        }

        ES_Code::PropertyCache &cache = reuse_cache ? *reuse_cache : code->property_get_caches[lip[3].index];

        PROFILING_SET_INSTRUCTION_FLAG();

        if (cache.class_id == 0 || reuse_cache)
        {
        set_cache:
#ifdef ES_PROPERTY_CACHE_PROFILING
            ++rt_data->pcache_fills;

            if (&cache != &code->property_get_caches[lip[3].index] || cache.next)
                ++rt_data->pcache_fills_poly;
#endif // ES_PROPERTY_CACHE_PROFILING

            ES_Class *cached_class = ES_Class::ActualClass(object_class);
            cache.class_id = cached_class->GetId(this);
            cache.data.prototype_object = prototype_object;
            cache.object_class = cached_class;
            cache.cached_type = ES_Value_Internal::CachedTypeBits(cached_type);

            cache.SetOffsetAndLimit(cached_offset, cache_limit);

            PROFILING_RECORD_GET_CACHE_HIT();
        }
        else
        {
            ES_Code::PropertyCache *before_last = &cache;

            for (unsigned length = 0; before_last->next && length < PROPERTY_CACHE_SIZE; before_last = before_last->next)
                if (before_last->class_id != before_last->next->class_id)
                    length++;

            if (before_last->next)
            {
                while (before_last->next->next)
                    RemoveStaleCacheRecord(this, code, TRUE, lip[3].index, before_last->next->next);

                OP_ASSERT(!before_last->next->next);
                *before_last->next = cache;
                cache.next = before_last->next;
                before_last->next = NULL;
            }
            else
            {
                ES_Code::PropertyCache *previous_first = AllocatePropertyCache();
                if (!previous_first)
                    AbortOutOfMemory();
                *previous_first = cache;
                cache.next = previous_first;

#ifdef ES_PROPERTY_CACHE_PROFILING
                rt_data->pcache_allocated_property += 8 + sizeof(ES_Code::PropertyCache);
#endif // ES_PROPERTY_CACHE_PROFILING
            }

            goto set_cache;
        }

        END_USING_SCRATCH_VALUES;
        return TRUE;
    }

    END_USING_SCRATCH_VALUES;

    if (res == PROP_GET_FAILED)
        return FALSE;
    else if (GET_NOT_FOUND(res))
        reg[dst].SetUndefined();

    return TRUE;
}

#ifdef _DEBUG
unsigned g_GETN_IMM_CACHED_total = 0, g_GETN_IMM_CACHED_misses = 0;
#endif // _DEBUG

IH_SPECIFIER ES_Execution_Context::IH_GETN_IMM(IH_PARAM_DECL)
{
    IH_SETUP_CODE(4);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index obj = lip[1].index;
    ES_Code::PropertyCache *cache = &code->property_get_caches[lip[3].index], *reuse_cache = NULL;

    if (cache->class_id != 0)
    {
#ifdef _DEBUG
        ++g_GETN_IMM_CACHED_total;
#endif // _DEBUG

#ifdef ES_DEBUG_COMPACT_OBJECTS
        OP_ASSERT(DebugPropertyCache(cache));
#endif // ES_DEBUG_COMPACT_OBJECTS

        ES_Object *object;
        ES_Class *object_class, *actual_class;

        if (reg[obj].IsObject())
        {
            object = reg[obj].GetObject(this);
            object_class = reg[obj].GetObject()->Class();

#ifdef ES_DEBUG_COMPACT_OBJECTS
            extern void DebugVerifyObject(ES_Object *object);
            DebugVerifyObject(object);
#endif // ES_DEBUG_COMPACT_OBJECTS

        validate_cache:
            actual_class = ES_Class::ActualClass(object_class);
            unsigned class_id = actual_class->Id();

            if (actual_class->NeedLimitCheck())
            {
                while (cache)
                {
                    if (cache->class_id == class_id)
                    {
                        PROFILING_RECORD_SLOW_CASE_CALL(ES_JIT_WEIGHT_CACHED_GET);

                        if (!reuse_cache)
                            reuse_cache = cache;

                        if (cache->IsNegativeCache() || cache->data.prototype_object)
                        {
                            if (object_class->HasHashTableProperties())
                            {
                                GetResult result = object->GetNonClassL(this, String(lip[2].index), reg[dst]);

                                if (GET_OK(result))
                                    IH_RETURN;
                                else if (result == PROP_GET_FAILED)
                                {
                                    HANDLE_THROW();
                                    IH_RETURN;
                                }
                            }

                            BOOL is_paired_cache = cache->next != NULL && cache->next->class_id == class_id;
                            unsigned limit = is_paired_cache ? cache->next->GetLimit() : cache->GetLimit();

                            if (limit < object->Count())
                                if (is_paired_cache)
                                {
                                    cache = cache->next;
                                    goto cache_hit;
                                }
                                else
                                    goto cache_miss;
                            else if (cache->IsNegativeCache())
                                reg[dst].SetUndefined();
                            else
                                cache->data.prototype_object->GetCached(cache->GetOffset(), cache->cached_type, reg[dst]);
                        }
                        else if (cache->GetLimit() < object->Count())
                        {
cache_hit:
                            object->GetCached(cache->GetOffset(), cache->cached_type, reg[dst]);
                        }
                        else
                            goto cache_miss;

#ifdef ES_DEBUG_COMPACT_OBJECTS
                        DebugVerifyObject(object);
#endif // ES_DEBUG_COMPACT_OBJECTS

                        IH_RETURN;
                    }
#ifdef ES_PROPERTY_CACHE_PROFILING
                    ++rt_data->pcache_misses;
#endif // ES_PROPERTY_CACHE_PROFILING

                    if (cache->object_class == actual_class)
                    {
                        RemoveStaleCacheRecord(this, code, TRUE, lip[3].index, cache);
                        goto cache_miss;
                    }
                    else
                        cache = cache->next;
                }
            }
            else
            {
                while (cache)
                {
                    if (cache->class_id == class_id)
                    {
                        PROFILING_RECORD_SLOW_CASE_CALL(ES_JIT_WEIGHT_CACHED_GET);
                        if (!cache->data.prototype_object)
                            if (!cache->IsNegativeCache())
                                object->GetCached(cache->GetOffset(), cache->cached_type, reg[dst]);
                            else if (!object_class->HasHashTableProperties())
                                reg[dst].SetUndefined();
                            else
                            {
                                GetResult result = object->GetNonClassL(this, String(lip[2].index), reg[dst]);

                                if (GET_NOT_FOUND(result))
                                    reg[dst].SetUndefined();
                                else if (result == PROP_GET_FAILED)
                                    HANDLE_THROW();
                            }
                        else
                        {
                            if (object_class->HasHashTableProperties())
                            {
                                GetResult result = object->GetNonClassL(this, String(lip[2].index), reg[dst]);

                                if (GET_OK(result))
                                    IH_RETURN;
                                else if (result == PROP_GET_FAILED)
                                {
                                    HANDLE_THROW();
                                    IH_RETURN;
                                }
                            }

                            cache->data.prototype_object->GetCached(cache->GetOffset(), cache->cached_type, reg[dst]);
                        }

#ifdef ES_DEBUG_COMPACT_OBJECTS
                        DebugVerifyObject(object);
#endif // ES_DEBUG_COMPACT_OBJECTS
                        IH_RETURN;
                    }
#ifdef ES_PROPERTY_CACHE_PROFILING
                    ++rt_data->pcache_misses;
#endif // ES_PROPERTY_CACHE_PROFILING

                    if (cache->object_class == actual_class)
                    {
                        RemoveStaleCacheRecord(this, code, TRUE, lip[3].index, cache);
                        goto cache_miss;
                    }
                    else
                        cache = cache->next;
                }
            }
        }
        else if (reg[obj].IsString() && String(lip[2].index)->Equals(UNI_L("length"), 6))
        {
            reg[dst].SetUInt32(Length(reg[obj].GetString()));
            IH_RETURN;
        }
        else if ((object = GetGlobalObject()->GetPrimitivePrototype(reg[obj])) != NULL)
        {
            PROFILING_START();
            PROFILING_STORE_REGISTER_TYPE(obj);
            PROFILING_RECORD_REGISTER_TYPE(obj, 2);
            PROFILING_END_WEIGHTED(1);

            object_class = GetGlobalObject()->GetPrimitiveClass(reg[obj]);
            goto validate_cache;
        }
    }

cache_miss:
#ifdef ES_PROPERTY_CACHE_PROFILING
    ++rt_data->pcache_fails;
#endif // ES_PROPERTY_CACHE_PROFILING

#ifdef _DEBUG
    ++g_GETN_IMM_CACHED_misses;
#endif // _DEBUG

    // Cache miss. Apply the non-cached instruction handler on the
    // same operands.
    if (!IH_GETN_IMM_UNCACHED(lip, dst, obj, reuse_cache))
        HANDLE_THROW();

    IH_RETURN;
}

BOOL ES_Execution_Context::IH_PUTN_IMM_UNCACHED(ES_CodeWord *lip, ES_CodeWord::Index obj, ES_CodeWord::Index value, ES_Code::PropertyCache *reuse_cache)
{
    JString *id = String(lip[1].index);
    // Three unused operands

    START_USING_SCRATCH_VALUES;

    ES_Value_Internal &object_val = src_val1;
    object_val = reg[obj];

    TOOBJECT(object_val, END_USING_SCRATCH_VALUES; return FALSE);

    ES_Class *new_class = NULL;
    ES_Class *old_class = object_val.GetObject(this)->Class();

#ifdef ES_DEBUG_COMPACT_OBJECTS
    extern void DebugVerifyObject(ES_Object *object);
    DebugVerifyObject(object_val.GetObject(this));
#endif // ES_DEBUG_COMPACT_OBJECTS

    ES_Object *object = object_val.GetObject(this);
    unsigned property_count = object->Count();
    ES_PropertyIndex cached_index;
    PutResult res;
    if ((lip - 1)->instruction == ESI_INIT_PROPERTY)
    {
        if (id->Equals("__proto__", 9))
            res = object->PutPrototypeL(this, reg[value]);
        else
        {
            object->InitPropertyL(this, id, reg[value]);
            res = PROP_PUT_OK;
        }
        cached_index = ES_PropertyIndex(0);
    }
    else
        res = object->PutL(this, reg[obj], id, reg[value], cached_index);

#ifdef ES_DEBUG_COMPACT_OBJECTS
    DebugVerifyObject(object_val.GetObject(this));
#endif // ES_DEBUG_COMPACT_OBJECTS

#ifdef DISABLE_PROPERTY_WRITE_CACHE
    switch (PUT_CANNOT_CACHE(res))
#else
    switch (res)
#endif // DISABLE_PROPERTY_WRITE_CACHE
    {
    case PROP_PUT_OK_CAN_CACHE_NEW:
        new_class = object_val.GetObject(this)->Class();
    case PROP_PUT_OK_CAN_CACHE:
        if (reg[obj].IsObject() && object_val.GetObject(this) == reg[obj].GetObject())
        {
            ES_Layout_Info layout = (new_class ? new_class : old_class)->GetLayoutInfoAtInfoIndex(cached_index);
            unsigned cached_offset = layout.GetOffset();
            ES_LayoutIndex cache_limit = new_class ? ES_LayoutIndex(property_count) : old_class->GetPropertyInfoAtIndex(cached_index).Index();

            if (!ES_Code::CanCache(cached_offset, cache_limit, FALSE))
            {
                END_USING_SCRATCH_VALUES;
                return TRUE;
            }

            if (reuse_cache && reuse_cache->class_id == old_class->Id())
            {
                BOOL current_is_correct = (new_class != NULL && reuse_cache->data.new_class != NULL) || new_class == reuse_cache->data.new_class;

                if (reuse_cache->next && reuse_cache->next->class_id == reuse_cache->class_id)
                {
                    if (!current_is_correct)
                        reuse_cache = reuse_cache->next;
                }
                else if (!current_is_correct)
                {
                    ES_Code::PropertyCache *new_cache = AllocatePropertyCache();
                    if (!new_cache)
                        AbortOutOfMemory();

                    new_cache->next = reuse_cache->next;
                    reuse_cache->next = new_cache;
                    reuse_cache = new_cache;
                }
            }

            ES_Code::PropertyCache &cache = reuse_cache ? *reuse_cache : code->property_put_caches[lip[3].index];

            PROFILING_SET_INSTRUCTION_FLAG();

            PROFILING_START();
            PROFILING_STORE_REGISTER_TYPE(value);
            PROFILING_RECORD_REGISTER_TYPE(value, 3);
            PROFILING_END();

            if (cache.class_id == 0 || reuse_cache)
            {
            set_cache:
#ifdef ES_PROPERTY_CACHE_PROFILING
                ++rt_data->pcache_fills;

                if (&cache != &code->property_put_caches[lip[3].index] || cache.next)
                    ++rt_data->pcache_fills_poly;
#endif // ES_PROPERTY_CACHE_PROFILING

                cache.class_id = old_class->GetId(this);
                cache.cached_type = ES_Value_Internal::CachedTypeBits(layout.GetStorageType());
                cache.data.new_class = new_class;
                cache.object_class = old_class;

                cache.SetOffsetAndLimit(layout.GetOffset(), cache_limit);
                reuse_cache = NULL;
            }
            else
            {
                ES_Code::PropertyCache *before_last = &cache;
                for (unsigned length = 0; before_last->next && length < PROPERTY_CACHE_SIZE; before_last = before_last->next, length++)
                    if (before_last->class_id != before_last->next->class_id)
                        length++;

                if (before_last->next)
                {
                    while (before_last->next->next)
                        RemoveStaleCacheRecord(this, code, FALSE, lip[3].index, before_last->next->next);

                    OP_ASSERT(!before_last->next->next);
                    *before_last->next = cache;
                    cache.next = before_last->next;
                    before_last->next = NULL;
                }
                else
                {
                    ES_Code::PropertyCache *previous_first = AllocatePropertyCache();
                    if (!previous_first)
                        AbortOutOfMemory();
                    *previous_first = cache;
                    cache.next = previous_first;

#ifdef ES_PROPERTY_CACHE_PROFILING
                    rt_data->pcache_allocated_property += 8 + sizeof(ES_Code::PropertyCache);
#endif // ES_PROPERTY_CACHE_PROFILING
                }

                goto set_cache;
            }
        }
    default:
        RemoveStaleCacheRecord(this, code, FALSE, lip[3].index, reuse_cache);
        END_USING_SCRATCH_VALUES;

        if (res == PROP_PUT_READ_ONLY && code->data->is_strict_mode)
        {
            ThrowTypeError("Invalid assignment in strict mode", lip - 1);
            return FALSE;
        }
        break;

    case PROP_PUT_FAILED:
        END_USING_SCRATCH_VALUES;
        return FALSE;
    }

    return TRUE;
}

#ifdef _DEBUG
unsigned g_PUTN_IMM_CACHED_total = 0, g_PUTN_IMM_CACHED_misses = 0;
#endif // _DEBUG

IH_SPECIFIER ES_Execution_Context::IH_PUTN_IMM(IH_PARAM_DECL)
{
    IH_SETUP_CODE(4);

    ES_CodeWord::Index obj = lip[0].index;
    ES_CodeWord::Index value = lip[2].index;
    ES_Code::PropertyCache *cache = &code->property_put_caches[lip[3].index], *reuse_cache = NULL;

    if (cache->class_id != 0)
    {
#ifdef _DEBUG
        ++g_GETN_IMM_CACHED_total;
#endif // _DEBUG

#ifdef ES_DEBUG_COMPACT_OBJECTS
        OP_ASSERT(DebugPropertyCache(cache));
#endif // ES_DEBUG_COMPACT_OBJECTS

        if (reg[obj].IsObject())
        {
            ES_Object *object = reg[obj].GetObject(this);
            ES_Class *object_class = object->Class();
            unsigned class_id = object_class->Id();

#ifdef ES_DEBUG_COMPACT_OBJECTS
            extern void DebugVerifyObject(ES_Object *object);
            DebugVerifyObject(object);
#endif // ES_DEBUG_COMPACT_OBJECTS

            if (object_class->NeedLimitCheck())
            {
                while (cache)
                {
                    if (cache->class_id == class_id)
                    {
                        if (!reuse_cache)
                            reuse_cache = cache;
#ifdef ES_NATIVE_SUPPORT
                        if (!cache->data.new_class || (cache->GetOffset() + cache->GetStorageSize()) < object->Capacity())
                            PROFILING_RECORD_SLOW_CASE_CALL(ES_JIT_WEIGHT_CACHED_PUT);
#endif // ES_NATIVE_SUPPORT

                        if (!reg[value].CheckType(cache->GetStorageType()))
                            goto cache_miss;

                        if (cache->data.new_class)
                            if (cache->GetLimit() == object->Count())
                                object->PutCachedNew(this, cache->GetOffset(), cache->cached_type, reg[value], cache->data.new_class);
                            else if (cache->next && cache->class_id == cache->next->class_id)
                                goto next_cache;
                            else
                                goto cache_miss;
                        else
                            if (cache->GetLimit() < object->Count())
                                object->PutCachedAtOffset(cache->GetOffset(), cache->cached_type, reg[value]);
                            else if (cache->next && cache->class_id == cache->next->class_id)
                                goto next_cache;
                            else
                                goto cache_miss;

#ifdef ES_DEBUG_COMPACT_OBJECTS
                        DebugVerifyObject(object);
#endif // ES_DEBUG_COMPACT_OBJECTS

                        PROFILING_START();
                        PROFILING_STORE_REGISTER_TYPE(value);
                        PROFILING_RECORD_REGISTER_TYPE(value, 3);
                        PROFILING_END();

                        IH_RETURN;
                    }
#ifdef ES_PROPERTY_CACHE_PROFILING
                    ++rt_data->pcache_misses;
#endif // ES_PROPERTY_CACHE_PROFILING

                    if (cache->object_class == object_class)
                    {
                        RemoveStaleCacheRecord(this, code, FALSE, lip[3].index, cache);
                        reuse_cache = NULL;
                        goto cache_miss;
                    }
                    else
                    {
                    next_cache:
                        cache = cache->next;
                    }
                }
            }
            else
            {
                while (cache)
                {
                    if (cache->class_id == class_id)
                    {
#ifdef ES_NATIVE_SUPPORT
                        if (!cache->data.new_class || (cache->GetOffset() + cache->GetStorageSize()) < object->Capacity())
                            PROFILING_RECORD_SLOW_CASE_CALL(ES_JIT_WEIGHT_CACHED_PUT);
#endif // ES_NATIVE_SUPPORT
                        if (!reg[value].CheckType(cache->GetStorageType()))
                        {
                            reuse_cache = cache;
                            goto cache_miss;
                        }

                        if (cache->data.new_class)
                            object->PutCachedNew(this, cache->GetOffset(), cache->cached_type, reg[value], cache->data.new_class);
                        else
                            object->PutCachedAtOffset(cache->GetOffset(), cache->cached_type, reg[value]);

#ifdef ES_DEBUG_COMPACT_OBJECTS
                        DebugVerifyObject(object);
#endif // ES_DEBUG_COMPACT_OBJECTS

                        PROFILING_START();
                        PROFILING_STORE_REGISTER_TYPE(value);
                        PROFILING_RECORD_REGISTER_TYPE(value, 3);
                        PROFILING_END();

                        IH_RETURN;
                    }
#ifdef ES_PROPERTY_CACHE_PROFILING
                    ++rt_data->pcache_misses;
#endif // ES_PROPERTY_CACHE_PROFILING

                    if (cache->object_class == object_class)
                    {
                        RemoveStaleCacheRecord(this, code, FALSE, lip[3].index, cache);
                        goto cache_miss;
                    }
                    else
                    {
                        cache = cache->next;
                    }
                }
            }
        }
    }

cache_miss:
#ifdef ES_PROPERTY_CACHE_PROFILING
    ++rt_data->pcache_fails;
#endif // ES_PROPERTY_CACHE_PROFILING

#ifdef _DEBUG
    ++g_PUTN_IMM_CACHED_misses;
#endif // _DEBUG

    // Cache miss. Apply the non-cached instruction handler on the
    // same operands.
    if (!IH_PUTN_IMM_UNCACHED(lip, obj, value, reuse_cache))
        HANDLE_THROW();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_GET_LENGTH(IH_PARAM_DECL)
{
    IH_SETUP_CODE(4);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index obj = lip[1].index;

    PROFILING_STORE_REGISTER_TYPE(obj);

    PROFILING_START();
    PROFILING_RECORD_REGISTER_TYPE(obj, 2);
    PROFILING_END_WEIGHTED(1);

    if (reg[obj].IsObject())
    {
        ES_Object *object = reg[obj].GetObject(this);

        if (object->HasIntrinsicLength())
        {
            object->GetCachedAtIndex(ES_PropertyIndex(0), reg[dst]);
            IH_RETURN;
        }
    }
    else if (reg[obj].IsString())
    {
        reg[dst].SetUInt32(reg[obj].GetString()->length);

        IH_RETURN;
    }

    IH_SET_CONTEXT_IP(lip);
    IH_RETURN_REDIRECTED(IH_GETN_IMM(IH_PARAM(lip)));
}

IH_SPECIFIER ES_Execution_Context::IH_PUT_LENGTH(IH_PARAM_DECL)
{
    IH_SETUP_CODE(4);

    ES_CodeWord::Index obj = lip[0].index;
    ES_CodeWord::Index value = lip[2].index;

    START_USING_SCRATCH_VALUES;

    if (reg[obj].IsObject())
    {
        ES_Object *object = reg[obj].GetObject(this);
        ES_Value_Internal &length_val = src_val1;

        if (object->IsArrayObject())
        {
            // Special treatment for Array.prototype.length

            object->GetCachedAtIndex(ES_PropertyIndex(0), length_val);

            UINT32 old_length = length_val.GetNumAsUInt32(), new_length;

            length_val = reg[value];

            if (!length_val.ToNumber(this))
                goto handle_error;
            else if (length_val.IsInt32())
            {
                if (length_val.GetInt32() < 0)
                {
                invalid_length:
                    ThrowRangeError("invalid array length", lip - 1);
                    goto handle_error;
                }

                new_length = length_val.GetInt32();
            }
            else
            {
                new_length = length_val.GetNumAsUInt32();

                if (UNSIGNED2DOUBLE(new_length) != length_val.GetDouble())
                    goto invalid_length;
            }

            if (!object->Class()->GetPropertyInfoAtIndex(ES_PropertyIndex(0)).IsReadOnly())
            {
                if (new_length < old_length)
                    new_length = ES_Indexed_Properties::TruncateL(this, object, object->GetIndexedProperties(), new_length, old_length);

                ES_Array::SetLength(this, object, new_length);
            }

            END_USING_SCRATCH_VALUES;
            IH_RETURN;
        }
    }

    END_USING_SCRATCH_VALUES;

    IH_SET_CONTEXT_IP(lip);
    IH_RETURN_REDIRECTED(IH_PUTN_IMM(IH_PARAM(lip)));

handle_error:
    END_USING_SCRATCH_VALUES;
    HANDLE_THROW();
    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_GETI_IMM(IH_PARAM_DECL)
{
    IH_SETUP_CODE(3);

    ES_CodeWord::Index dst = lip[0].index & 0x7fffffffu;
    ES_CodeWord::Index obj = lip[1].index;
    ES_CodeWord::Index index = lip[2].index;

    GetResult res;

    if (reg[obj].IsObject())
    {
        ES_Object *object = reg[obj].GetObject(this);

        res = object->GetL(this, reg[obj], index, reg[dst]);

#ifdef ES_NATIVE_SUPPORT
        if (ES_Indexed_Properties *properties = object->GetIndexedProperties())
        {
            unsigned bits = ES_Indexed_Properties::GetTypeBits(properties);

            PROFILING_START();
            PROFILING_RECORD_DATA(bits & 0xff, 2);
            PROFILING_RECORD_DATA((bits >> 8) & 0xff, 3);
            PROFILING_END_WEIGHTED(bits != ES_Indexed_Properties::TYPE_BITS_SPARSE ? 2 : 0);
        }
#endif // ES_NATIVE_SUPPORT
    }
    else
    {
        if (reg[obj].IsString())
            if (index < Length(reg[obj].GetString()))
            {
                JString *string = reg[obj].GetString();
                uni_char c = Element(string, index);

                PROFILING_START();
                PROFILING_RECORD_DATA(ES_Indexed_Properties::TYPE_BITS_STRING, 2);
                PROFILING_END_WEIGHTED(c < STRING_NUMCHARS ? 2 : 0);

                if (c < STRING_NUMCHARS)
                    string = rt_data->strings[STRING_nul + c];
                else
                    string = JString::Make(this, string, index, 1);

                reg[dst].SetString(string);

                IH_RETURN;
            }

        START_USING_SCRATCH_VALUES;

        if (reg[obj].AsObject(this, src_val1, FALSE))
            res = src_val1.GetObject()->GetL(this, reg[obj], index, reg[dst]);
        else
        {
            ThrowErrorBase(lip - 1, TYPE_ERROR, EXPECTED_OBJECT);
            res = PROP_GET_FAILED;
        }

        END_USING_SCRATCH_VALUES;
    }

    switch (res)
    {
    case PROP_GET_FAILED:
        HANDLE_THROW();
        break;
    case PROP_GET_NOT_FOUND:
    case PROP_GET_NOT_FOUND_CAN_CACHE:
        reg[dst].SetUndefined();
        break;
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_PUTI_IMM(IH_PARAM_DECL)
{
    IH_SETUP_CODE(3);

    ES_CodeWord::Index obj = lip[0].index;
    ES_CodeWord::Index index = lip[1].index;
    ES_CodeWord::Index value = lip[2].index;

    START_USING_SCRATCH_VALUES;

    ES_Value_Internal &object_val = src_val1;
    object_val = reg[obj];

    TOOBJECT(object_val, END_USING_SCRATCH_VALUES; HANDLE_THROW(); IH_RETURN);

    ES_Object *object = object_val.GetObject(this);
    GC_STACK_ANCHOR(this, object);

    PutResult res = object->PutL(this, index, reg[value]);

#ifdef ES_NATIVE_SUPPORT
    if (ES_Indexed_Properties *properties = object->GetIndexedProperties())
    {
        unsigned bits = ES_Indexed_Properties::GetTypeBits(properties);

        PROFILING_START();
        PROFILING_RECORD_DATA(bits & 0xff, 1);
        PROFILING_RECORD_DATA((bits >> 8) & 0xff, 2);
        PROFILING_STORE_REGISTER_TYPE(value);
        PROFILING_RECORD_REGISTER_TYPE(value, 3);
        PROFILING_END_WEIGHTED(bits != ES_Indexed_Properties::TYPE_BITS_SPARSE ? 2 : 0);
    }
#endif // ES_NATIVE_SUPPORT

    END_USING_SCRATCH_VALUES;

    if (res == PROP_PUT_FAILED)
        HANDLE_THROW();
    else if (res == PROP_PUT_READ_ONLY && code->data->is_strict_mode)
    {
        ThrowTypeError("Invalid assignment in strict mode", lip - 1);
        HANDLE_THROW();
    }
    else if (object->IsArrayObject() && PUT_OK(res))
    {
        ES_Value_Internal length_val;
        object->GetCachedAtIndex(ES_PropertyIndex(0), length_val);
        OP_ASSERT(length_val.IsNumber());
        UINT32 length = length_val.GetNumAsUInt32();
        if (!(index < length))
            ES_Array::SetLength(this, object, index + 1);
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_GET(IH_PARAM_DECL)
{
    IH_SETUP_CODE(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index obj = lip[1].index;
    ES_CodeWord::Index prop = lip[2].index;

    START_USING_SCRATCH_VALUES;

#ifdef ES_BASE_REFERENCE_INFORMATION
    BOOL want_object = (dst & 0x80000000u) != 0;
#endif // ES_BASE_REFERENCE_INFORMATION

    dst &= 0x7fffffffu;

    ES_Value_Internal &name = reg[prop];
    BOOL primitive_string = FALSE;
    ES_Object *object;

    if (reg[obj].IsObject())
        object = reg[obj].GetObject(this);
    else
    {
        if (reg[obj].IsString())
            if (name.IsUInt32())
            {
                UINT32 index = name.GetNumAsUInt32();
                if (index < Length(reg[obj].GetString()))
                {
                    JString *string = reg[obj].GetString();
                    uni_char c = Element(string, index);

                    PROFILING_START();
                    PROFILING_RECORD_DATA(ES_Indexed_Properties::TYPE_BITS_STRING, 2);
                    PROFILING_END_WEIGHTED(c < STRING_NUMCHARS ? 2 : 0);

                    if (c < STRING_NUMCHARS)
                        string = rt_data->strings[STRING_nul + c];
                    else
                        string = JString::Make(this, string, index, 1);

                    reg[dst].SetString(string);

                    END_USING_SCRATCH_VALUES;
                    IH_RETURN;
                }
            }
            else
                primitive_string = TRUE;

        if (!reg[obj].AsObject(this, src_val1, FALSE))
        {
#ifdef ES_BYTECODE_DEBUGGER
            if (bytecode_debugger_enabled)
                ES_BytecodeDebugger::AtError(this, code, lip - 1, "TypeError", "value is not a function");
#endif // ES_BYTECODE_DEBUGGER

            ThrowErrorBase(lip - 1, TYPE_ERROR, EXPECTED_OBJECT);

            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
            IH_RETURN;
        }

        object = src_val1.GetObject();
    }

    GetResult res = PROP_GET_FAILED;
    unsigned index;

    if (name.IsUInt32() && UINT_MAX != (index = name.GetNumAsUInt32()))
    {
        // Array access
        res = object->GetL(this, reg[obj], index, reg[dst]);

#ifdef ES_NATIVE_SUPPORT
        if (ES_Indexed_Properties *properties = object->GetIndexedProperties())
        {
            unsigned bits = ES_Indexed_Properties::GetTypeBits(properties);

            PROFILING_START();
            PROFILING_RECORD_DATA(bits & 0xff, 2);
            PROFILING_RECORD_DATA((bits >> 8) & 0xff, 3);
            PROFILING_END_WEIGHTED(bits != ES_Indexed_Properties::TYPE_BITS_SPARSE ? 2 : 0);
        }
#endif // ES_NATIVE_SUPPORT
    }
    else
    {
        ES_Value_Internal &str = src_val2;
        str = name;

        if (str.ToString(this)) // Failure handled below.
        {
            JString *ident = str.GetString();

            if (ident == enumerated_string && object->Class()->Id() == enumerated_class_id && enumerated_limit < object->Count())
            {
                res = object->GetCached(enumerated_cached_offset, enumerated_cached_type, reg[dst]);
                OP_ASSERT(!reg[dst].IsSpecial());
            }
            else if (convertindex(Storage(this, ident), Length(ident), index))
                // Index in string
                res = object->GetL(this, index, reg[dst]);
            else
            {
                if (primitive_string && ident->Equals(UNI_L("length"), 6))
                {
                    reg[dst].SetUInt32(Length(reg[obj].GetString()));
                    res = PROP_GET_OK;
                }
                else
                    res = object->GetL(this, ident, reg[dst]);
            }
        }
    }

    END_USING_SCRATCH_VALUES;

    switch (res)
    {
    case PROP_GET_FAILED:
        reg[dst].SetUndefined();
        HANDLE_THROW();
        break;
    case PROP_GET_NOT_FOUND:
    case PROP_GET_NOT_FOUND_CAN_CACHE:
#ifdef ES_BASE_REFERENCE_INFORMATION
        if (want_object)
        {
            base_reference_object = object;
            base_reference_register = prop;
            base_reference_name = NULL;
        }
#endif // ES_BASE_REFERENCE_INFORMATION
        reg[dst].SetUndefined();
        break;
#ifdef ES_BASE_REFERENCE_INFORMATION
    default:
        if (want_object && reg[dst].IsNullOrUndefined())
        {
            base_reference_object = object;
            base_reference_register = prop;
            base_reference_name = NULL;
        }
#endif // ES_BASE_REFERENCE_INFORMATION
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_PUT(IH_PARAM_DECL)
{
    IH_SETUP_CODE(3);

    ES_CodeWord::Index obj = lip[0].index;
    ES_CodeWord::Index prop = lip[1].index;
    ES_CodeWord::Index value = lip[2].index;

#ifdef ES_NATIVE_SUPPORT
    BOOL double_index = reg[prop].Type() == ESTYPE_DOUBLE;
#endif // ES_NATIVE_SUPPORT
    ES_Value_Internal &name = reg[prop];

    START_USING_SCRATCH_VALUES;

    ES_Value_Internal &object_val = src_val1;
    ES_Value_Internal &length_val = src_val2;

    object_val = reg[obj];

    TOOBJECT(object_val, END_USING_SCRATCH_VALUES; HANDLE_THROW(); IH_RETURN);

    ES_Object *object = object_val.GetObject(this);

    PutResult res = PROP_PUT_FAILED;
    unsigned index;

    UINT32 length;

    // Get array length.
    if (object->IsArrayObject())
    {
        object->GetCachedAtIndex(ES_PropertyIndex(0), length_val);
        if (!length_val.ToNumber(this))
            OP_ASSERT(!"Can this happen?");
        length = length_val.GetNumAsUInt32();
    }
    else
        length = UINT_MAX;

    if (name.IsUInt32() && UINT_MAX != (index = name.GetNumAsUInt32()))
    {
        // Array access
        res = object->PutL(this, index, reg[value]);
        if (PUT_OK(res))
            if (!(index < length))
            {
                OP_ASSERT(object->IsArrayObject());
                ES_Array::SetLength(this, object, index + 1);
            }
            else
                ES_Indexed_Properties::AdjustTop(object->GetIndexedProperties(), length);

#ifdef ES_NATIVE_SUPPORT
        if (ES_Indexed_Properties *properties = object->GetIndexedProperties())
        {
            unsigned bits = ES_Indexed_Properties::GetTypeBits(properties);

            PROFILING_START();
            PROFILING_RECORD_DATA(bits & 0xff, 1);
            PROFILING_RECORD_DATA((bits >> 8) & 0xff, 2);
            PROFILING_RECORD_DATA(double_index ? 0x40 : 0, 2);
            PROFILING_STORE_REGISTER_TYPE(value);
            PROFILING_RECORD_REGISTER_TYPE(value, 3);
            PROFILING_END_WEIGHTED(bits != ES_Indexed_Properties::TYPE_BITS_SPARSE ? 2 : 0);
        }
#endif // ES_NATIVE_SUPPORT
    }
    else
    {
        ES_Value_Internal &str = src_val3;
        str = name;

        if (str.ToString(this)) // Failure handled below.
        {
            if (convertindex(Storage(this, str.GetString()), Length(str.GetString()), index))
            {
                // Index in string
                res = object->PutL(this, index, reg[value]);
                if (PUT_OK(res))
                    if (!(index < length))
                    {
                        OP_ASSERT(object->IsArrayObject());
                        ES_Array::SetLength(this, object, index + 1);
                    }
                    else
                        ES_Indexed_Properties::AdjustTop(object->GetIndexedProperties(), length);
            }
            else if (Equals(str.GetString(), rt_data->strings[STRING_proto]))
                res = object->PutPrototypeL(this, reg[value]);
            else
            {
                if (object->IsArrayObject() && Equals(str.GetString(), rt_data->strings[STRING_length]))
                {
                    // Speacial treatment for Array.prototype.length
                    length_val = reg[value];
                    if (!length_val.ToNumber(this))
                    {
                        END_USING_SCRATCH_VALUES;
                        HANDLE_THROW();
                        IH_RETURN;
                    }

                    UINT32 new_length = length_val.GetNumAsUInt32();
                    if (new_length < length)
                        new_length = ES_Indexed_Properties::TruncateL(this, object, object->GetIndexedProperties(), new_length, length);

                    ES_Array::SetLength(this, object, new_length);

                    res = PROP_PUT_OK;
                }
                else if (str.GetString() == enumerated_string)
                    if (object->Class()->Id() == enumerated_class_id && enumerated_limit < object->Count() && reg[value].CheckType(ES_Value_Internal::StorageTypeFromCachedTypeBits(enumerated_cached_type)))
                        res = object->PutCachedAtOffset(enumerated_cached_offset, enumerated_cached_type, reg[value]);
                    else
                        res = object->PutL(this, str.GetString(), reg[value]);
                else
                    res = object->PutNonClassL(this, str.GetString(), reg[value]);
            }
        }
    }

    END_USING_SCRATCH_VALUES;

    if (res == PROP_PUT_FAILED)
        HANDLE_THROW();
    else if (res == PROP_PUT_READ_ONLY && code->data->is_strict_mode)
    {
        ThrowTypeError("Invalid assignment in strict mode", lip - 1);
        HANDLE_THROW();
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_DEFINE_GETTER(IH_PARAM_DECL)
{
    IH_SETUP(4);

    ES_CodeWord::Index obj = lip[0].index;
    JString *name = String(lip[1].index);
    ES_CodeWord::Index fn = lip[2].index;
    ES_CodeWord::Index scope_index = lip[3].index;

    START_USING_SCRATCH_VALUES;

    ES_Value_Internal &object_val = src_val1;
    object_val = reg[obj];

    TOOBJECT(object_val, END_USING_SCRATCH_VALUES; HANDLE_THROW(); IH_RETURN);

    ES_Object *object = object_val.GetObject();

    ES_Function *getter = NewFunction(Code()->functions[fn], scope_index != UINT_MAX ? &Code()->data->inner_scopes[scope_index] : NULL);
    ES_CollectorLock gclock(this);

    unsigned index;

    if (convertindex(Storage(this, name), Length(name), index))
        name = NULL;

    ES_Object::PropertyDescriptor desc;

    desc.SetEnumerable(TRUE);
    desc.SetConfigurable(TRUE);
    desc.SetGetter(getter);

    const char *message;

    BOOL success = object->DefineOwnPropertyL(this, name, index, desc, message);

    END_USING_SCRATCH_VALUES;

    if (!success)
        HANDLE_THROW();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_DEFINE_SETTER(IH_PARAM_DECL)
{
    IH_SETUP(4);

    ES_CodeWord::Index obj = lip[0].index;
    JString *name = String(lip[1].index);
    ES_CodeWord::Index fn = lip[2].index;
    ES_CodeWord::Index scope_index = lip[3].index;

    START_USING_SCRATCH_VALUES;

    ES_Value_Internal &object_val = src_val1;
    object_val = reg[obj];

    TOOBJECT(object_val, END_USING_SCRATCH_VALUES; HANDLE_THROW(); IH_RETURN);

    ES_Object *object = object_val.GetObject();

    ES_Function *setter = NewFunction(Code()->functions[fn], scope_index != UINT_MAX ? &Code()->data->inner_scopes[scope_index] : NULL);
    ES_CollectorLock gclock(this);

    unsigned index;

    if (convertindex(Storage(this, name), Length(name), index))
        name = NULL;

    ES_Object::PropertyDescriptor desc;

    desc.SetEnumerable(TRUE);
    desc.SetConfigurable(TRUE);
    desc.SetSetter(setter);

    const char *message;

    BOOL success = object->DefineOwnPropertyL(this, name, index, desc, message);

    END_USING_SCRATCH_VALUES;

    if (!success)
        HANDLE_THROW();
}

IH_SPECIFIER ES_Execution_Context::IH_GET_SCOPE(IH_PARAM_DECL)
{
    IH_SETUP_CODE(5);

    ES_CodeWord::Index dst = lip[0].index;
    JString *id = String(lip[1].index);
    ES_CodeWord::Index scope_idx = lip[2].index;
    ES_CodeWord::Index local_variable = lip[3].index;
    ES_Code::GlobalCache &cache = Code()->global_caches[lip[4].index];

    BOOL is_cached = cache.class_id != ES_Class::NOT_CACHED_CLASS_ID;

    BOOL used_in_typeof = FALSE;
    if ((dst & 0x80000000u) != 0)
    {
        dst = dst & 0x7fffffffu;
        used_in_typeof = TRUE;
    }

    GetResult res = PROP_GET_FAILED;

    // Inner scope (i.e., inside with())
    if (scope_idx != ES_CodeWord::IndexMax)
    {
        ES_CodeStatic::InnerScope &inner_scope = code->data->inner_scopes[scope_idx];

        for (int i = inner_scope.registers_count - 1; i >= 0; --i)
        {
            START_USING_SCRATCH_VALUES;

            ES_Value_Internal &object_val = src_val1;
            object_val = reg[inner_scope.registers[i]];
            OP_ASSERT(object_val.IsObject());

            ES_Object *object = object_val.GetObject(this);

            res = object->GetL(this, id, reg[dst]);

            END_USING_SCRATCH_VALUES;

            if (GET_OK(res))
                IH_RETURN;
            else if (res == PROP_GET_FAILED)
            {
                HANDLE_THROW();
                IH_RETURN;
            }
        }
    }

    // Local variable
    if (local_variable != ES_CodeWord::IndexMax)
    {
        reg[dst] = reg[local_variable];
        IH_RETURN;
    }

    ES_FrameStackIterator frames(this);
    ES_Code *code = NULL;
    ES_Object *variable_object = NULL;

    do
    {
        frames.Next();
        if (frames.GetCode())
            code = frames.GetCode();
        variable_object = frames.GetVariableObject();
    }
    while (!code || code->type == ES_Code::TYPE_EVAL_PLAIN);

    ES_PropertyIndex cached_index;

    // ES_Function scope
    if (code->type == ES_Code::TYPE_FUNCTION)
    {
        // Variable object
        /*
          e.g.:
          function()
          {
            eval("var x = 1");
            x;
          }
         */
        if (variable_object && (!is_cached || !variable_object->IsVariablesObject()))
        {
            res = variable_object->GetL(this, id, reg[dst]);
            if (GET_OK(res))
                IH_RETURN;
            else if (res == PROP_GET_FAILED)
            {
                HANDLE_THROW();
                IH_RETURN;
            }
        }

        ES_FunctionCode *function_code = static_cast<ES_FunctionCode *>(code);
        ES_FunctionCodeStatic *function_code_data = function_code->GetData();

        if (function_code_data->is_function_expr)
            if (JString *name = function_code->GetName())
                if (Equals(id, name))
                {
                    reg[dst] = reg[1];
                    IH_RETURN;
                }

        ES_Function *current_function = static_cast<ES_Function *> (reg[1].GetObject(this));
        unsigned scope_chain_length = current_function->GetScopeChainLength();
        ES_Object **scope_chain = current_function->GetScopeChain();

        for (unsigned i = 0; i < scope_chain_length; ++i)
        {
            ES_Object *object = scope_chain[i];
            if (!is_cached || !object->IsVariablesObject())
            {
                res = object->GetL(this, id, reg[dst]);

                if (GET_OK(res))
                    IH_RETURN;
                else if (res == PROP_GET_FAILED)
                    goto fail;
            }
        }
    }

    // ES_Code scope
    for (unsigned i = code->scope_chain_length; i != 0;)
    {
        ES_Object *object = code->scope_chain[--i];

        res = object->GetL(this, id, reg[dst]);

        if (GET_OK(res))
            IH_RETURN;
        else if (res == PROP_GET_FAILED)
            goto fail;
    }

    // External scope
    for (unsigned j = 0; j < external_scope_chain_length; ++j)
    {
        ES_Object *object = external_scope_chain[j];

        res = object->GetL(this, id, reg[dst]);

        if (GET_OK(res))
            IH_RETURN;
        else if (res == PROP_GET_FAILED)
            goto fail;
    }

    ES_Global_Object *global_object;
    global_object = GetGlobalObject();

    if (cache.class_id == ES_Class::GLOBAL_IMMEDIATE_CLASS_ID)
    {
    immediate:
        reg[dst] = global_object->GetVariableValue(cache.cached_index);
        goto finished;
    }
    else if (cache.class_id == global_object->Class()->Id())
    {
        global_object->GetCached(cache.cached_offset, cache.cached_type, reg[dst]);
        goto finished;
    }
    else if (cache.class_id == ES_Class::NOT_CACHED_CLASS_ID && cache.cached_offset == UINT_MAX)
        if (global_object->GetVariableIndex(cache.cached_index, id))
        {
            cache.class_id = ES_Class::GLOBAL_IMMEDIATE_CLASS_ID;
            goto immediate;
        }
        else
            cache.cached_offset = 0;

    ES_Object *prototype_object;

    // Global object
    res = global_object->GetL(this, global_object, id, reg[dst], prototype_object, cached_index);

    switch (res)
    {
    case PROP_GET_FAILED:
    fail:
        HANDLE_THROW();
        break;
    case PROP_GET_NOT_FOUND:
    case PROP_GET_NOT_FOUND_CAN_CACHE:
        if (!used_in_typeof)
        {
            // When value is used in typeof, do not throw if not found.
            ThrowReferenceError("Undefined variable: ", Storage(this, id), Length(id), lip - 1);
            HANDLE_THROW();
        }
        else
            reg[dst].SetUndefined();
        break;

    case PROP_GET_OK_CAN_CACHE:
        if (!prototype_object)
        {
            cache.class_id = global_object->Class()->GetId(this);
            ES_Layout_Info layout = global_object->Class()->GetLayoutInfoAtInfoIndex(cached_index);
            cache.cached_offset = layout.GetOffset();
            cache.cached_type = ES_Value_Internal::CachedTypeBits(layout.GetStorageType());
            cache.cached_index = cached_index;
        }
    }

finished:
    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_GET_SCOPE_REF(IH_PARAM_DECL)
{
    IH_SETUP_CODE(6);

    BOOL for_call = (lip[0].index & 0x80000000u) != 0;
    ES_CodeWord::Index dst_value = lip[0].index & 0x7fffffffu;
    ES_CodeWord::Index dst_base = lip[1].index;
    JString *id = String(lip[2].index);
    ES_CodeWord::Index scope_idx = lip[3].index;
    ES_CodeWord::Index local_variable = lip[4].index;
    ES_Code::GlobalCache *cache = &Code()->global_caches[lip[5].index];

    BOOL is_cached = cache->class_id != ES_Class::NOT_CACHED_CLASS_ID;

    GetResult res = PROP_GET_FAILED;

    // Inner scope
    if (scope_idx != ES_CodeWord::IndexMax)
    {
        ES_CodeStatic::InnerScope &inner_scope = code->data->inner_scopes[scope_idx];

        for (int i = inner_scope.registers_count - 1; i >= 0; --i)
        {
            START_USING_SCRATCH_VALUES;

            ES_Value_Internal &object_val = src_val1;
            object_val = reg[inner_scope.registers[i]];
            OP_ASSERT(object_val.IsObject());
            ES_Object *object = object_val.GetObject(this);

            res = object->GetL(this, id, reg[dst_value]);

            END_USING_SCRATCH_VALUES;

            if (GET_OK(res))
            {
                if (for_call && object->IsScopeObject())
                    reg[dst_base].SetUndefined();
                else
                    reg[dst_base].SetObject(object);
                IH_RETURN;
            }
            else if (res == PROP_GET_FAILED)
            {
                HANDLE_THROW();
                IH_RETURN;
            }
        }
    }

    ES_FrameStackIterator frames(this);
    ES_Code *code = NULL;
    ES_Object *variable_object = NULL;

    do
    {
        frames.Next();
        if (frames.GetCode())
            code = frames.GetCode();
        variable_object = frames.GetVariableObject();
    }
    while (!code || code->type == ES_Code::TYPE_EVAL_PLAIN);

    // Local variable
    if (local_variable != ES_CodeWord::IndexMax)
    {
        reg[dst_value] = reg[local_variable];

        if (for_call)
            reg[dst_base].SetUndefined();
        else
        {
            if (!variable_object)
                variable_object = CreateAndPropagateVariableObject(frames, static_cast<ES_FunctionCode *>(code), reg + 2);

            reg[dst_base].SetObject(variable_object);
        }

        IH_RETURN;
    }

    ES_PropertyIndex cached_index;
    ES_Object *prototype_object = NULL;

    // ES_Function scope
    if (code->type == ES_Code::TYPE_FUNCTION)
    {
        // Variable object
        if (variable_object && (!is_cached || !variable_object->IsVariablesObject()))
        {
            res = variable_object->GetL(this, id, reg[dst_value]);
            if (GET_OK(res))
            {
                if (for_call)
                    reg[dst_base].SetUndefined();
                else
                    reg[dst_base].SetObject(variable_object);
                IH_RETURN;
            }
            else if (res == PROP_GET_FAILED)
            {
                HANDLE_THROW();
                IH_RETURN;
            }
        }

        ES_Function *current_function = static_cast<ES_Function *> (reg[1].GetObject(this));
        unsigned scope_chain_length = current_function->GetScopeChainLength();
        ES_Object **scope_chain = current_function->GetScopeChain();

        for (unsigned i = 0; i < scope_chain_length; ++i)
        {
            ES_Object *object = scope_chain[i];
            if (!is_cached || !object->IsVariablesObject())
            {
                res = object->GetL(this, id, reg[dst_value]);

                if (GET_OK(res))
                {
                    if (for_call && object->IsScopeObject())
                        reg[dst_base].SetUndefined();
                    else
                        reg[dst_base].SetObject(object);
                    IH_RETURN;
                }
                else if (res == PROP_GET_FAILED)
                    goto fail;
            }
        }

        ES_FunctionCode *function_code = static_cast<ES_FunctionCode *>(code);
        ES_FunctionCodeStatic *function_code_data = function_code->GetData();

        if (function_code_data->is_function_expr)
            if (JString *name = function_code->GetName())
                if (Equals(id, name))
                {
                    if (for_call)
                        reg[dst_base].SetUndefined();
                    else
                    {
                        ES_Object *tmp = ES_Object::Make(this, GetGlobalObject()->GetObjectClass());
                        reg[dst_base].SetObject(tmp);
                    }
                    reg[dst_value] = reg[1];
                    IH_RETURN;
                }
    }

    // ES_Code scope
    for (unsigned i = code->scope_chain_length; i != 0;)
    {
        ES_Object *object = code->scope_chain[--i];

        res = object->GetL(this, id, reg[dst_value]);

        if (GET_OK(res))
        {
            if (for_call && object->IsScopeObject())
                reg[dst_base].SetUndefined();
            else
                reg[dst_base].SetObject(object);
            IH_RETURN;
        }
        else if (res == PROP_GET_FAILED)
            goto fail;
    }

    // External scope
    for (unsigned j = 0; j < external_scope_chain_length; ++j)
    {
        ES_Object *object = external_scope_chain[j];

        res = object->GetL(this, id, reg[dst_value]);

        if (GET_OK(res))
        {
            reg[dst_base].SetObject(object);
            IH_RETURN;
        }
        else if (res == PROP_GET_FAILED)
            goto fail;
    }

    ES_Global_Object *global_object;
    global_object = GetGlobalObject();

    if (cache)
    {
        if (cache->class_id == ES_Class::GLOBAL_IMMEDIATE_CLASS_ID)
        {
        immediate:
            reg[dst_value] = global_object->GetVariableValue(cache->cached_index);
            goto finished;
        }
        else if (cache->class_id == global_object->Class()->Id())
        {
            global_object->GetCached(cache->cached_offset, cache->cached_type, reg[dst_value]);
            goto finished;
        }
        else if (cache->class_id == ES_Class::NOT_CACHED_CLASS_ID && cache->cached_offset == UINT_MAX)
            if (global_object->GetVariableIndex(cache->cached_index, id))
            {
                cache->class_id = ES_Class::GLOBAL_IMMEDIATE_CLASS_ID;
                goto immediate;
            }
            else
                cache->cached_offset = 0;
    }

    res = global_object->GetL(this, global_object, id, reg[dst_value], prototype_object, cached_index);

    switch (res)
    {
    case PROP_GET_FAILED:
    fail:
        HANDLE_THROW();
        break;
    case PROP_GET_NOT_FOUND:
    case PROP_GET_NOT_FOUND_CAN_CACHE:
        ThrowReferenceError("Undefined variable: ", Storage(this, id), Length(id), lip - 1);
        HANDLE_THROW();
        break;

    case PROP_GET_OK_CAN_CACHE:
        if (!prototype_object)
        {
            cache->class_id = global_object->Class()->GetId(this);
            ES_Layout_Info layout = global_object->Class()->GetLayoutInfoAtInfoIndex(cached_index);
            cache->cached_offset = layout.GetOffset();
            cache->cached_type = ES_Value_Internal::CachedTypeBits(layout.GetStorageType());
            cache->cached_index = cached_index;
        }

    default:
    finished:
        if (for_call)
            reg[dst_base].SetUndefined();
        else
            reg[dst_base].SetObject(global_object);
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_PUT_SCOPE(IH_PARAM_DECL)
{
    IH_SETUP_CODE(4);

    JString *id = String(lip[0].index);
    ES_CodeWord::Index value = lip[1].index;
    ES_CodeWord::Index scope_idx = lip[2].index;
    ES_CodeWord::Index local_variable = lip[3].index;
    ES_FrameStackIterator frames(this);
    ES_Code *actual_code = NULL;
    ES_Object *variable_object = NULL;
    ES_Object *global_object = GetGlobalObject();

    PutResult res = PROP_PUT_FAILED;

    // Inner scope
    if (scope_idx != ES_CodeWord::IndexMax)
    {
        ES_CodeStatic::InnerScope &inner_scope = code->data->inner_scopes[scope_idx];

        for (int i = inner_scope.registers_count - 1; i >= 0; --i)
        {
            START_USING_SCRATCH_VALUES;

            ES_Value_Internal &object_val = src_val1;
            object_val = reg[inner_scope.registers[i]];
            OP_ASSERT(object_val.IsObject());

            ES_Object *object = object_val.GetObject(this);

            if (object->HasProperty(this, id))
            {
                SetIsInIdentifierExpression(TRUE);
                res = object->PutL(this, id, reg[value]);
                SetIsInIdentifierExpression(FALSE);

                END_USING_SCRATCH_VALUES;

                if (res == PROP_PUT_FAILED)
                    goto fail;
                else if (res == PROP_PUT_READ_ONLY && code->data->is_strict_mode)
                    goto strict_read_only;

                IH_RETURN;
            }

            END_USING_SCRATCH_VALUES;
        }
    }

    // Local variable
    if (local_variable != ES_CodeWord::IndexMax)
    {
        reg[local_variable] = reg[value];
        IH_RETURN;
    }

    do
    {
        frames.Next();
        if (frames.GetCode())
            actual_code = frames.GetCode();
        variable_object = frames.GetVariableObject();
    }
    while (!actual_code || actual_code->type == ES_Code::TYPE_EVAL_PLAIN);

    // ES_Function scope
    if (actual_code->type == ES_Code::TYPE_FUNCTION)
    {
        // Variable object
        if (variable_object)
        {
            if (variable_object->HasProperty(this, id))
            {
                res = variable_object->PutL(this, id, reg[value]);

                if (res == PROP_PUT_FAILED)
                    goto fail;
                else if (res == PROP_PUT_READ_ONLY && code->data->is_strict_mode)
                    goto strict_read_only;

                IH_RETURN;
            }
        }

        ES_Function *current_function = static_cast<ES_Function *> (reg[1].GetObject(this));
        unsigned scope_chain_length = current_function->GetScopeChainLength();
        ES_Object **scope_chain = current_function->GetScopeChain();

        for (unsigned i = 0; i < scope_chain_length; ++i)
        {
            ES_Object *object = scope_chain[i];
            if (object->HasProperty(this, id))
            {
                SetIsInIdentifierExpression(TRUE);
                res = object->PutL(this, id, reg[value]);
                SetIsInIdentifierExpression(FALSE);

                if (res == PROP_PUT_FAILED)
                    goto fail;
                else if (res == PROP_PUT_READ_ONLY && code->data->is_strict_mode)
                    goto strict_read_only;

                IH_RETURN;
            }
        }

        ES_FunctionCode *function_code = static_cast<ES_FunctionCode *>(actual_code);
        ES_FunctionCodeStatic *function_code_data = function_code->GetData();

        if (function_code_data->is_function_expr)
            if (JString *name = function_code->GetName())
                if (Equals(id, name))
                    // Function identifier is ReadOnly, ECMA spec 13.
                    if (code->data->is_strict_mode)
                        goto strict_read_only;
                    else
                        IH_RETURN;
    }

    // ES_Code scope
    for (unsigned i = actual_code->scope_chain_length; i != 0;)
    {
        ES_Object *object = actual_code->scope_chain[--i];
        if (object->HasProperty(this, id))
        {
            SetIsInIdentifierExpression(TRUE);
            res = object->PutL(this, id, reg[value]);
            SetIsInIdentifierExpression(FALSE);

            if (res == PROP_PUT_FAILED)
                goto fail;
            else if (res == PROP_PUT_READ_ONLY && code->data->is_strict_mode)
                goto strict_read_only;

            IH_RETURN;
        }
    }

    // External scope
    for (unsigned j = 0; j < external_scope_chain_length; ++j)
    {
        ES_Object *object = external_scope_chain[j];
        if (object->HasProperty(this, id))
        {
            SetIsInIdentifierExpression(TRUE);
            res = object->PutL(this, id, reg[value]);
            SetIsInIdentifierExpression(FALSE);

            if (res == PROP_PUT_FAILED)
                goto fail;
            else if (res == PROP_PUT_READ_ONLY && code->data->is_strict_mode)
                goto strict_read_only;

            IH_RETURN;
        }
    }

    if (code->data->is_strict_mode)
        if (!global_object->HasProperty(this, id))
        {
            ThrowReferenceError("Assignment to unresolved reference in strict mode code: ", Storage(this, id), Length(id), lip - 1);
            goto fail;
        }

    // Global object
    SetIsInIdentifierExpression(TRUE);
    res = global_object->PutL(this, id, reg[value]);
    SetIsInIdentifierExpression(FALSE);

    if (res == PROP_PUT_FAILED)
        goto fail;
    else if (res == PROP_PUT_READ_ONLY && code->data->is_strict_mode)
        goto strict_read_only;

    IH_RETURN;

strict_read_only:
    ThrowTypeError("Invalid assignment in strict mode", lip - 1);
fail:
    HANDLE_THROW();
    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_DELETE_SCOPE(IH_PARAM_DECL)
{
    IH_SETUP_CODE(3);

    JString *id = String(lip[0].index);
    ES_CodeWord::Index scope_idx = lip[1].index;
    ES_CodeWord::Index local_variable = lip[2].index;
    ES_FrameStackIterator frames(this);
    ES_Code *actual_code = NULL;
    ES_Object *variable_object = NULL;
    BOOL success = TRUE;

    // Inner scope
    if (scope_idx != ES_CodeWord::IndexMax)
    {
        ES_CodeStatic::InnerScope &inner_scope = code->data->inner_scopes[scope_idx];

        for (int i = inner_scope.registers_count - 1; i >= 0; --i)
        {
            START_USING_SCRATCH_VALUES;

            ES_Value_Internal &object_val = src_val1;
            object_val = reg[inner_scope.registers[i]];
            OP_ASSERT(object_val.IsObject());

            if (object_val.GetObject(this)->HasProperty(this, id))
            {
                success = object_val.GetObject(this)->DeleteL(this, id, implicit_bool);

                END_USING_SCRATCH_VALUES;

                goto finished;
            }

            END_USING_SCRATCH_VALUES;
        }
    }

    // Local variable
    if (local_variable != ES_CodeWord::IndexMax)
    {
        /* The variable object, if we had one, has a DontDelete property by this
           name, so we should just return false. */
        implicit_bool = FALSE;
        goto finished;
    }

    do
    {
        frames.Next();
        if (frames.GetCode())
            actual_code = frames.GetCode();
        variable_object = frames.GetVariableObject();
    }
    while (!actual_code || actual_code->type == ES_Code::TYPE_EVAL_PLAIN);

    // ES_Function scope
    if (actual_code->type == ES_Code::TYPE_FUNCTION)
    {
        // Variable object
        if (variable_object && variable_object->HasProperty(this, id)) // FIXME: check if a full HasProperty is needed
        {
            success = variable_object->DeleteL(this, id, implicit_bool);
            goto finished;
        }

        ES_Function *current_function = static_cast<ES_Function *> (reg[1].GetObject(this));
        unsigned scope_chain_length = current_function->GetScopeChainLength();
        ES_Object **scope_chain = current_function->GetScopeChain();

        for (unsigned i = 0; i < scope_chain_length; ++i)
        {
            ES_Object *object = scope_chain[i];
            if (object->HasProperty(this, id)) // FIXME: check if a full HasProperty is needed
            {
                success = object->DeleteL(this, id, implicit_bool);
                goto finished;
            }
        }
    }

    // ES_Code scope
    for (unsigned i = actual_code->scope_chain_length; i != 0;)
    {
        ES_Object *object = actual_code->scope_chain[--i];
        if (object->HasProperty(this, id))
        {
            success = object->DeleteL(this, id, implicit_bool);
            goto finished;
        }
    }

    // Global object
    success = GetGlobalObject()->DeleteL(this, id, implicit_bool);

finished:
    if (success && !implicit_bool && code->data->is_strict_mode)
    {
        ThrowTypeError("Invalid use of delete in strict mode");
        success = FALSE;
    }

    if (!success)
        HANDLE_THROW();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_GET_GLOBAL(IH_PARAM_DECL)
{
    IH_SETUP_CODE(3);

    ES_CodeWord::Index dst = lip[0].index;
    JString *id = String(lip[1].index);
    ES_Global_Object *global_object = code->global_object;

    ES_Code::GlobalCache &cache = Code()->global_caches[lip[2].index];

    BOOL used_in_typeof = FALSE;

    if ((dst & 0x80000000u) != 0)
    {
        dst = dst & 0x7fffffffu;
        used_in_typeof = TRUE;
    }

    if (cache.class_id == ES_Class::GLOBAL_IMMEDIATE_CLASS_ID)
        reg[dst] = global_object->GetVariableValue(cache.cached_index);
    else if (cache.class_id == global_object->Class()->Id())
        global_object->GetCached(cache.cached_offset, cache.cached_type, reg[dst]);
    else
    {
        ES_Object *prototype_object;
        ES_PropertyIndex cached_index;

        GetResult res = global_object->GetL(this, global_object, id, reg[dst], prototype_object, cached_index);

        switch (res)
        {
        case PROP_GET_OK_CAN_CACHE:
            /* Don't cache the rare case of finding the property in the
               prototype.  This makes the case of cached access simpler and
               somewhat faster. */
            if (!prototype_object)
            {
                cache.class_id = global_object->Class()->GetId(this);
                ES_Property_Info info = global_object->Class()->GetPropertyInfoAtIndex(cached_index);
                ES_Layout_Info layout = global_object->Class()->GetLayoutInfoAtIndex(info.Index());
                cache.cached_offset = layout.GetOffset();
                cache.cached_type = ES_Value_Internal::CachedTypeBits(layout.GetStorageType());
                cache.cached_index = info.Index();
            }
            break;
        case PROP_GET_FAILED:
            HANDLE_THROW();
            break;
        case PROP_GET_NOT_FOUND:
        case PROP_GET_NOT_FOUND_CAN_CACHE:
            if (!used_in_typeof)
            {
                // When value is used in typeof, do not throw if not found.
                ThrowReferenceError("Undefined variable: ", Storage(this, id), Length(id), lip - 1);
                HANDLE_THROW();
            }
            else
                reg[dst].SetUndefined();
            break;
        }
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_PUT_GLOBAL(IH_PARAM_DECL)
{
    IH_SETUP_CODE(3);

    JString *id = String(lip[0].index);
    ES_CodeWord::Index value = lip[1].index;
    ES_Global_Object *global_object = GetGlobalObject();

    ES_Code::GlobalCache &cache = Code()->global_caches[lip[2].index];

    if (cache.class_id == ES_Class::GLOBAL_IMMEDIATE_CLASS_ID)
        global_object->GetVariableValue(cache.cached_index) = reg[value];
    else if (cache.class_id == global_object->Class()->Id() && reg[value].CheckType(ES_Value_Internal::StorageTypeFromCachedTypeBits(cache.cached_type)))
        global_object->PutCachedAtOffset(cache.cached_offset, cache.cached_type, reg[value]);
    else
    {
        if (code->data->is_strict_mode)
            if (!global_object->HasProperty(this, id))
            {
                ThrowReferenceError("Assignment to unresolved reference in strict mode code: ", Storage(this, id), Length(id), lip - 1);
                HANDLE_THROW();
            }

        ES_PropertyIndex cached_index;
        SetIsInIdentifierExpression(TRUE);
        PutResult res = global_object->PutL(this, global_object, id, reg[value], cached_index);
        SetIsInIdentifierExpression(FALSE);

        if (res == PROP_PUT_FAILED)
            HANDLE_THROW();
        else if (res == PROP_PUT_READ_ONLY && code->data->is_strict_mode)
        {
            ThrowTypeError("Invalid assignment in strict mode", lip - 1);
            HANDLE_THROW();
        }
        else if (res == PROP_PUT_OK_CAN_CACHE && lip[2].index != UINT_MAX)
        {
            cache.class_id = global_object->Class()->GetId(this);
            ES_Property_Info info = global_object->Class()->GetPropertyInfoAtIndex(cached_index);
            ES_Layout_Info layout = global_object->Class()->GetLayoutInfoAtIndex(info.Index());
            cache.cached_offset = layout.GetOffset();
            cache.cached_type = ES_Value_Internal::CachedTypeBits(layout.GetStorageType());
            cache.cached_index = info.Index();
        }
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_GET_LEXICAL(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index scope_index = lip[1].index;
    ES_CodeWord::Index variable_index = lip[2].index;

    static_cast<ES_Function *>(reg[1].GetObject(this))->GetLexical(reg[dst], scope_index, ES_PropertyIndex(variable_index));

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_PUT_LEXICAL(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index scope_index = lip[0].index;
    ES_CodeWord::Index variable_index = lip[1].index;
    ES_CodeWord::Index value = lip[2].index;

    static_cast<ES_Function *>(reg[1].GetObject(this))->PutLexical(scope_index, ES_PropertyIndex(variable_index), reg[value]);

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_DELETEN_IMM(IH_PARAM_DECL)
{
    IH_SETUP_CODE(2);

    ES_CodeWord::Index obj = lip[0].index;
    JString *id = String(lip[1].index);

    if (reg[obj].IsNullOrUndefined())
        implicit_bool = TRUE;
    else
    {
        START_USING_SCRATCH_VALUES;

        ES_Value_Internal &object_val = src_val1;
        object_val = reg[obj];
        object_val.ToObject(this, FALSE);

#ifdef ES_DEBUG_COMPACT_OBJECTS
        extern void DebugVerifyObject(ES_Object *object);
        DebugVerifyObject(object_val.GetObject(this));
#endif // ES_DEBUG_COMPACT_OBJECTS

        BOOL success = object_val.GetObject(this)->DeleteL(this, id, implicit_bool);

#ifdef ES_DEBUG_COMPACT_OBJECTS
        DebugVerifyObject(object_val.GetObject(this));
#endif // ES_DEBUG_COMPACT_OBJECTS

        END_USING_SCRATCH_VALUES;

        if (success && !implicit_bool && code->data->is_strict_mode)
        {
            ThrowTypeError("Invalid use of delete in strict mode");
            success = FALSE;
        }

        if (!success)
            HANDLE_THROW();
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_DELETEI_IMM(IH_PARAM_DECL)
{
    IH_SETUP_CODE(2);

    ES_CodeWord::Index obj = lip[0].index;
    ES_CodeWord::Index index = lip[1].index;

    if (reg[obj].IsNullOrUndefined())
        implicit_bool = TRUE;
    else
    {
        START_USING_SCRATCH_VALUES;

        ES_Value_Internal &object_val = src_val1;
        object_val = reg[obj];
        object_val.ToObject(this, FALSE);

        BOOL success = object_val.GetObject(this)->DeleteL(this, index, implicit_bool);

        END_USING_SCRATCH_VALUES;

        if (success && !implicit_bool && code->data->is_strict_mode)
        {
            ThrowTypeError("Invalid use of delete in strict mode");
            success = FALSE;
        }

        if (!success)
            HANDLE_THROW();
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_DELETE(IH_PARAM_DECL)
{
    IH_SETUP_CODE(2);

    ES_CodeWord::Index obj = lip[0].index;
    ES_CodeWord::Index prop = lip[1].index;
    ES_Value_Internal &name = reg[prop];

    if (reg[obj].IsNullOrUndefined())
        implicit_bool = TRUE;
    else
    {
        START_USING_SCRATCH_VALUES;

        ES_Value_Internal &object_val = src_val1;
        object_val = reg[obj];
        object_val.ToObject(this, FALSE);

        unsigned index;
        BOOL success;

        if (name.IsUInt32() && UINT_MAX != (index = name.GetNumAsUInt32()))
            // Array access
            success = object_val.GetObject(this)->DeleteL(this, index, implicit_bool);
        else
        {
            ES_Value_Internal &str = src_val2;
            str = name;

            if (str.ToString(this))
                if (convertindex(Storage(this, str.GetString()), Length(str.GetString()), index))
                    // Index in string
                    success = object_val.GetObject(this)->DeleteL(this, index, implicit_bool);
                else
                    success = object_val.GetObject(this)->DeleteL(this, str.GetString(), implicit_bool);
            else
                success = FALSE;
        }

        END_USING_SCRATCH_VALUES;

        if (success && !implicit_bool && code->data->is_strict_mode)
        {
            ThrowTypeError("Invalid use of delete in strict mode");
            success = FALSE;
        }

        if (!success)
            HANDLE_THROW();
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_DECLARE_GLOBAL(IH_PARAM_DECL)
{
    IH_SETUP(1);

    JString *id = String(lip->index);
    ES_Object *global_object = GetGlobalObject();

    if (!global_object->HasProperty(this, id)) // FIXME: check if a full HasProperty is needed
    {
        ES_Value_Internal value;
        PutResult res = global_object->PutL(this, id, value);
        if (res == PROP_PUT_FAILED)
            HANDLE_THROW();
    }

    IH_RETURN;
}


IH_SPECIFIER ES_Execution_Context::IH_HASPROPERTY(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index obj = lip[0].index;
    ES_CodeWord::Index prop = lip[1].index;

    ES_Value_Internal &name = reg[prop];

    if (reg[obj].IsObject())
    {
        START_USING_SCRATCH_VALUES;

        ES_Value_Internal &str = src_val1;
        str = name;

        if (str.ToString(this))
        {
            unsigned index;
            JString *string = str.GetString();
            BOOL has_property;
            BOOL is_secure = FALSE;
            if (convertindex(Storage(this, string), Length(string), index))
                has_property = reg[obj].GetObject(this)->HasProperty(this, index, is_secure);
            else
                has_property = reg[obj].GetObject(this)->HasProperty(this, string, is_secure);

            END_USING_SCRATCH_VALUES;
            if (!is_secure)
            {
                ThrowReferenceError("Security error: attempted to read protected variable: ", Storage(this, string), Length(string));
                HANDLE_THROW();
            }
            implicit_bool = has_property;
        }
        else
        {
            END_USING_SCRATCH_VALUES;
            HANDLE_THROW();
        }
    }
    else
    {
        ThrowTypeError("Operator 'in' applied to non-object", lip - 1);
        HANDLE_THROW();
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_INSTANCEOF(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index obj = lip[0].index;
    ES_CodeWord::Index cons = lip[1].index;

    if (reg[cons].IsObject() && reg[cons].GetObject()->IsFunctionObject())
    {
        if (!reg[obj].IsObject())
        {
            implicit_bool = FALSE;
            IH_RETURN;
        }

        ES_Object *constructor = reg[cons].GetObject(this);

        InstanceOfResult res = reg[obj].GetObject(this)->InstanceOf(this, constructor);
        if (res != INSTANCE_BAD)
        {
            implicit_bool = res;
            IH_RETURN;
        }
    }

    ThrowTypeError("Second argument to 'instanceof' does not implement [[HasInstance]]", lip - 1);
    HANDLE_THROW();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_ENUMERATE(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index props = lip[0].index;
    ES_CodeWord::Index obj = lip[1].index;
    ES_CodeWord::Index count = lip[2].index;

    START_USING_SCRATCH_VALUES;

    ES_Value_Internal &object_val = src_val1;
    object_val = reg[obj];
    reg[count].SetUInt32(0);

    if (object_val.ToObject(this, FALSE))
    {
        ES_Object *object = object_val.GetObject(this);
        reg[obj].SetObject(object);

        ES_Boxed *names = object->GetPropertyNamesL(this, TRUE, FALSE);
        reg[props].SetBoxed(names);
    }
    else
    {
        reg[obj].SetObject(context_global_object);
        reg[props].SetBoxed(NULL);
    }

    END_USING_SCRATCH_VALUES;
    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_NEXT_PROPERTY(IH_PARAM_DECL)
{
    IH_SETUP(4);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index props = lip[1].index;
    ES_CodeWord::Index obj = lip[2].index;
    ES_CodeWord::Index count = lip[3].index;

    implicit_bool = FALSE;

    OP_ASSERT(reg[obj].IsObject());
    ES_Object *object = reg[obj].GetObject();

    ES_Boxed *list = reg[props].GetBoxed();

    while (list && !implicit_bool)
    {
        ES_Boxed *head, *tail = NULL;

        enumerated_string = NULL;

        if (list->GCTag() == GCTAG_ES_Boxed_List)
        {
            head = static_cast<ES_Boxed_List *>(list)->head;
            tail = static_cast<ES_Boxed_List *>(list)->tail;
        }
        else
            head = list;

        switch (head->GCTag())
        {
        case GCTAG_ES_Compact_Indexed_Properties:
        case GCTAG_ES_Sparse_Indexed_Properties:
            {
                ES_Indexed_Properties *names = static_cast<ES_Indexed_Properties *>(head);
                ES_Indexed_Property_Iterator iterator(this, NULL, names);
                unsigned index;
                BOOL has_next = FALSE;

                enumerated_class_id = ES_Class::NOT_CACHED_CLASS_ID;

                if (iterator.LowerBound(index, reg[count].GetNumAsUInt32()))
                    do
                    {
                        iterator.GetValue(reg[dst]); // should always succeed

                        if (reg[dst].IsString())
                        {
                            BOOL is_secure = TRUE;
                            ES_Property_Info info;
                            if (object->HasOwnProperty(this, reg[dst].GetString(), info, is_secure))
                            {
                                implicit_bool = TRUE;
                                if (info.IsClassProperty() && !info.IsSpecial() && !info.IsFunction())
                                {
                                    enumerated_class_id = object->Class()->GetId(this);
                                    enumerated_string = reg[dst].GetString();
                                    ES_Layout_Info layout = object->Class()->GetLayoutInfo(info);
                                    enumerated_cached_offset = layout.GetOffset();
                                    enumerated_cached_type = ES_Value_Internal::CachedTypeBits(layout.GetStorageType());
                                    enumerated_limit = info.Index();
                                }
                            }
                            else if (object->Class()->Prototype() && object->Class()->Prototype()->HasProperty(this, reg[dst].GetString()))
                                implicit_bool = TRUE;
                        }
                        else if (object->HasProperty(this, reg[dst].GetNumAsUInt32()))
                        {
                            enumerated_index = reg[dst].GetNumAsUInt32();
                            reg[dst].ToString(this);
                            implicit_bool = TRUE;
                            enumerated_string = reg[dst].GetString();
                        }
                    } while ((has_next = iterator.Next(index)) && !implicit_bool);

                if (has_next)
                    reg[count].SetUInt32(index);
                else
                    reg[count].SetUInt32(UINT_MAX);
            }
            break;
        case GCTAG_ES_Class_Node:
        case GCTAG_ES_Class_Compact_Node:
        case GCTAG_ES_Class_Compact_Node_Frozen:
        case GCTAG_ES_Class_Singleton:
        case GCTAG_ES_Class_Hash:
            {
                unsigned index = reg[count].GetNumAsUInt32();

                ES_Class *klass = static_cast<ES_Class *>(head);
                while (index < klass->CountProperties())
                {
                    ES_Property_Info info;
                    JString *name;
                    klass->Lookup(ES_PropertyIndex(index), name, info);

                    if (info.IsDontEnum())
                    {
                        ++index;
                        continue;
                    }

                    BOOL in_prototype = FALSE;
                    if (klass == object->Class() || (in_prototype = object->HasProperty(this, name)))
                    {
                        reg[dst].SetString(name);
                        implicit_bool = TRUE;

                        if (!in_prototype && !info.IsSpecial() && !info.IsFunction())
                        {
                            enumerated_class_id = klass->GetId(this);
                            enumerated_string = name;
                            ES_Layout_Info layout = object->Class()->GetLayoutInfo(info);
                            enumerated_cached_offset = layout.GetOffset();
                            enumerated_cached_type = ES_Value_Internal::CachedTypeBits(layout.GetStorageType());
                            enumerated_limit = info.Index();
                        }

                        break;
                    }
                    ++index;
                }

                reg[count].SetUInt32(++index);
            }
            break;
        case GCTAG_ES_Object_String:
            {
                unsigned index = reg[count].GetNumAsUInt32();
                unsigned length = Length(static_cast<ES_String_Object *>(head)->GetValue());

                if (index < length)
                {
                    reg[dst].SetUInt32(index);
                    reg[dst].ToString(this);
                    implicit_bool = TRUE;
                    reg[count].SetUInt32(++index);
                    break;
                }
            }
            break;
        }
        if (!implicit_bool)
        {
            list = tail;
            reg[props].SetBoxed(list);
            reg[count].SetUInt32(0);
        }
    }

    IH_RETURN;
}

void ES_Execution_Context::SetupCallState(ES_CodeWord::Index argc, ES_Function *function)
{
    ES_FunctionCode *called_code = function->GetFunctionCode();
    ES_FunctionCodeStatic *called_code_data = called_code->GetData();

    code = called_code;
    ip = called_code->data->codewords;
    variable_object = NULL;
    this->argc = argc;

    for (unsigned index = 2 + argc, stop = 2 + called_code_data->formals_count; index < stop; ++index)
        reg[index].SetUndefined();

    BOOL create_arguments_object = called_code_data->arguments_index != ES_FunctionCodeStatic::ARGUMENTS_NOT_USED;

    if (!called_code_data->is_strict_mode || called_code_data->has_redirected_call)
    {
        if (argc > called_code_data->formals_count)
            create_arguments_object = TRUE;

#ifdef ECMASCRIPT_DEBUGGER
        // If we are debugging we always create the arguments object as we may
        // need to send the object to interrupting contexts. See
        // ES_EngineDebugBackend::Eval for details
        if (IsDebugged())
            create_arguments_object = TRUE;
#endif // ECMASCRIPT_DEBUGGER
    }

    if (create_arguments_object)
        CreateArgumentsObject(this, function, reg, argc);

    // From now on we don't need the registers occupied by arguments anymore
    // so we can shrink the frame. This also ensures that we will free the
    // same amount of registers when we return from the call.
    int shrink_size = argc + 2 - called_code_data->register_frame_size;
    if (shrink_size > 0)
        register_blocks.Shrink(shrink_size);

    for (unsigned index = 2 + called_code_data->formals_count, stop = 2 + called_code_data->formals_count + called_code_data->LocalsCount(); index < stop; ++index)
        if (index != called_code_data->arguments_index)
            reg[index].SetUndefined();
}

void ES_Execution_Context::PerformFunctionCall(ES_CodeWord *lip, const ES_Value_Internal &function_val, unsigned rel_frame_start, unsigned argc, BOOL new_in_constructor)
{
    ES_Function *function = static_cast<ES_Function *> (function_val.GetObject(this));

    if (--time_until_check == 0)
        CheckOutOfTime();

    ES_FunctionCode *called_code;
    if (!function->IsHostObject() && (called_code = function->GetFunctionCode()))
    {
        OP_ASSERT(!function->IsHostObject());

#ifdef ES_NATIVE_SUPPORT
        BOOL was_in_constructor = !!in_constructor;
#endif // ES_NATIVE_SUPPORT

        if (!called_code->data->is_strict_mode)
        {
            if (!reg[rel_frame_start].ToObject(this, FALSE))
                reg[rel_frame_start].SetObject(GetGlobalObject());

            OP_ASSERT(!reg[rel_frame_start].GetObject()->IsScopeObject());
        }

        ABORT_IF_MEMORY_ERROR(frame_stack.Push(this));

        ES_Value_Internal *next_reg;

        overlap = code->data->register_frame_size - rel_frame_start;
        ABORT_IF_MEMORY_ERROR(register_blocks.Allocate(this, first_in_block, next_reg, MAX(argc + 2, called_code->data->register_frame_size), overlap, argc + 2));

        in_constructor = new_in_constructor;

#ifdef ES_BYTECODE_DEBUGGER
        if (bytecode_debugger_enabled)
            ES_BytecodeDebugger::AtFunction(this, called_code);
#endif // ES_BYTECODE_DEBUGGER

#ifdef ES_NATIVE_SUPPORT
        if (!was_in_constructor && UseNativeDispatcher())
        {
            void *native_dispatcher = called_code->native_dispatcher;
            ES_NativeCodeBlock *ncblock = called_code->native_code_block;

            if (!native_dispatcher && called_code->data->CanHaveNativeDispatcher())
            {
                called_code->slow_case_calls += called_code->data->codewords_count;

                if (called_code->ShouldGenerateNativeDispatcher())
                {
                    ES_AnalyzeAndGenerateNativeDispatcher analyze_and_generate_native_dispatcher(this, function);
                    if (OpStatus::IsMemoryError(analyze_and_generate_native_dispatcher.status))
                        AbortOutOfMemory();

                    native_dispatcher = called_code->native_dispatcher;
                    ncblock = called_code->native_code_block;
                }
            }

            if (native_dispatcher)
            {
                OP_ASSERT(!need_adjust_register_usage);

                if (!code->native_dispatcher)
                    code->slow_case_calls += ES_JIT_WEIGHT_CALLED_FUNCTION;

#ifdef ES_STACK_RECONSTRUCTION_SUPPORTED
                BOOL reconstruct_native_stack = code->native_dispatcher != NULL && !first_in_block && ES_NativeStackFrame::GetReturnAddress(code, ip);
#else // ES_STACK_RECONSTRUCTION_SUPPORTED
                BOOL reconstruct_native_stack = FALSE;
#endif // ES_STACK_RECONSTRUCTION_SUPPORTED

                ES_Code *calling_code = code;

                if (reconstruct_native_stack)
                {
                    code = called_code;

                    /* Signals that we're making an initial call, so the
                       top-most frame doesn't need to be reconstructed. */
                    native_dispatcher_reg = NULL;

                    this->argc = argc;
                }

                if (first_in_block)
                {
                    reg = next_reg;
                    calling_code = NULL;
                }

                BOOL native_success = CallBytecodeToNativeTrampoline(calling_code, next_reg, ncblock, native_dispatcher, argc, reconstruct_native_stack);

                need_adjust_register_usage = FALSE;

                if (!reconstruct_native_stack)
                    register_blocks.Free(MAX(2 + argc, called_code->data->register_frame_size), overlap, 1, first_in_block);
                else
                {
                    ES_FrameStackIterator frames(this);

                    frames.Next();
                    frames.Next();

                    register_blocks.Free(frames.GetVirtualFrame()->next_frame_size, overlap, 1, first_in_block);
                }

                frame_stack.Pop(this);

                if (!native_success)
                    if (ip->instruction != ESI_EXIT_TO_BUILTIN && ip->instruction != ESI_EXIT_TO_EVAL && ip->instruction != ESI_EXIT)
                        HANDLE_THROW();

                return;
            }
        }
#endif // ES_NATIVE_SUPPORT

        reg = next_reg;

#ifdef ECMASCRIPT_DEBUGGER
        ES_Value_Internal this_value;
        if (IsDebugged())
        {
            this_value = reg[0];
            SetCallDetails(this_value, function, argc, reg + 2);
            SignalToDebugger(ES_DebugListener::ESEV_CALLSTARTING, lip);
        }
#endif // ECMASCRIPT_DEBUGGER

        SetupCallState(argc, function);

#ifdef ECMASCRIPT_DEBUGGER
        if (IsDebugged())
        {
            SetCallDetails(this_value, function);
            SignalToDebugger(ES_DebugListener::ESEV_ENTERFN, NULL);
        }
#endif // ECMASCRIPT_DEBUGGER
    }
    else
    {
        ES_Function::BuiltIn *call;
        if (function->IsHostObject())
            call = ES_Host_Function::Call;
        else
            call = function->GetCall();

        BOOL success;

#ifdef ECMASCRIPT_DEBUGGER
        if (IsDebugged())
        {
            ES_Value_Internal this_value = reg[rel_frame_start];

            /* These objects are reinstated after the call(), protect across. */
            ES_StackValueAnchor this_anchor(this, this_value);
            ES_StackPointerAnchor function_anchor(this, function);

            SetCallDetails(this_value, function, argc, reg + rel_frame_start + 2);
            SignalToDebugger(ES_DebugListener::ESEV_CALLSTARTING, lip - 1);

            success = call(this, argc, reg + rel_frame_start + 2, reg + rel_frame_start);

            if (success)
            {
                SetCallDetails(this_value, function);
                return_value = reg[rel_frame_start];
                SignalToDebugger(ES_DebugListener::ESEV_CALLCOMPLETED, lip - 1);
            }
        }
        else
#endif // ECMASCRIPT_DEBUGGER
            success = call(this, argc, reg + rel_frame_start + 2, reg + rel_frame_start);

        if (!success)
            HANDLE_THROW();
        else
            OP_ASSERT(current_exception.IsNoExceptionCookie());
    }
}

IH_SPECIFIER ES_Execution_Context::IH_EVAL(IH_PARAM_DECL)
{
    IH_SETUP(4);

    ES_CodeWord::Index rel_frame_start = lip[0].index;
    ES_CodeWord::Index argc = lip[1].index;

    const ES_Value_Internal &function_val = reg[rel_frame_start+1];
    if (!function_val.IsObject() || !function_val.GetObject(this)->IsFunctionObject())
    {
        // TODO: replace value with its name.
#ifdef ES_BYTECODE_DEBUGGER
        if (bytecode_debugger_enabled)
            ES_BytecodeDebugger::AtError(this, code, lip - 1, "TypeError", "value is not a function");
#endif // ES_BYTECODE_DEBUGGER

        ThrowErrorBase(lip - 1, TYPE_ERROR, EXPECTED_FUNCTION);
        HANDLE_THROW();
        IH_RETURN;
    }

    if (frame_stack.TotalUsed() > ES_MAXIMUM_FUNCTION_RECURSION)
    {
        ThrowRangeError("Maximum recursion depth exceeded", lip - 1);
        HANDLE_THROW();
        IH_RETURN;
    }

    //OP_ASSERT(argc & 0x80000000u);     // Highest bit should always be set for eval.

    argc &= 0x7fffffffu;

    ES_Function *function = static_cast<ES_Function *> (function_val.GetObject(this));

    if (--time_until_check == 0)
        CheckOutOfTime();

    ES_Function::BuiltIn *call = function->GetCall();
    if (call && call == &ES_GlobalBuiltins::eval)
    {
#ifdef ECMASCRIPT_DEBUGGER
        if (IsDebugged())
        {
            SetCallDetails(reg[0], function, argc, reg + rel_frame_start + 2);
            SignalToDebugger(ES_DebugListener::ESEV_CALLSTARTING, lip - 1);
        }
#endif // ECMASCRIPT_DEBUGGER

        ES_Value_Internal *ret_reg = reg + rel_frame_start;
        if (!ES_GlobalBuiltins::Eval(this, argc, ret_reg + 2, ret_reg, Code(), lip - 1))
            HANDLE_THROW();

#ifdef ECMASCRIPT_DEBUGGER
        if (IsDebugged())
        {
            return_value = *ret_reg;
            SignalToDebugger(ES_DebugListener::ESEV_CALLCOMPLETED, lip - 1);
        }
#endif // ECMASCRIPT_DEBUGGER

        IH_RETURN;
    }
    else
    {
        // Not eval, fallback to call

        reg[rel_frame_start].SetObject(GetGlobalObject());

        PerformFunctionCall(lip, function_val, rel_frame_start, argc, FALSE);

        IH_RETURN;
    }
}

IH_SPECIFIER ES_Execution_Context::IH_CALL(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    IH_SETUP(2);

    ES_CodeWord::Index rel_frame_start = lip[0].index;
    ES_CodeWord::Index argc = lip[1].index;

    const ES_Value_Internal &function_val = reg[rel_frame_start+1];
    ES_Object *object = NULL;

    if (!function_val.IsObject() ||
        !(object = function_val.GetObject(this))->IsFunctionObject()
#ifdef ES_REGEXP_IS_CALLABLE
        && !object->IsRegExpObject()
#endif //ES_REGEXP_IS_CALLABLE
        )
    {
        // TODO: replace value with its name.
#ifdef ES_BYTECODE_DEBUGGER
        if (bytecode_debugger_enabled)
            ES_BytecodeDebugger::AtError(this, code, lip - 1, "TypeError", "value is not a function");
#endif // ES_BYTECODE_DEBUGGER

        ThrowErrorBase(lip - 1, TYPE_ERROR, EXPECTED_FUNCTION);
        HANDLE_THROW();
        IH_RETURN;
    }

    if (frame_stack.TotalUsed() > ES_MAXIMUM_FUNCTION_RECURSION)
    {
        ThrowRangeError("Maximum recursion depth exceeded", lip - 1);
        HANDLE_THROW();
        IH_RETURN;
    }

    if (argc & 0x80000000u)
    {
#ifdef ES_REGEXP_IS_CALLABLE
        if (object->IsRegExpObject())
            reg[rel_frame_start].SetObject(object);
        else
#endif // ES_REGEXP_IS_CALLABLE
            reg[rel_frame_start].SetUndefined();
        argc ^= 0x80000000u;
    }

    PerformFunctionCall(lip, function_val, rel_frame_start, argc, FALSE);

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_REDIRECTED_CALL(IH_PARAM_DECL)
{
    IH_SETUP(2);

#ifdef ES_NATIVE_SUPPORT
    //OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

#ifdef ES_NATIVE_SUPPORT
    unsigned argc = native_stack_frame ? ES_NativeStackFrame::GetArgumentsCount(native_stack_frame) : arguments_object ? arguments_object->GetLength() : 0;
#else // ES_NATIVE_SUPPORT
    unsigned argc = arguments_object ? arguments_object->GetLength() : 0;
#endif // ES_NATIVE_SUPPORT

    const ES_Value_Internal &function = reg[lip[0].index];
    const ES_Value_Internal &apply = reg[lip[1].index];

    BOOL stored_in_constructor = !!in_constructor;

    if (!apply.IsObject() || !apply.GetObject(this)->IsFunctionObject())
    {
        ThrowErrorBase(lip - 1, TYPE_ERROR, EXPECTED_FUNCTION);
        HANDLE_THROW();
        IH_RETURN;
    }

    ES_Value_Internal local_return_value, *use_return_value;

    /* The instruction following this one will be either ESI_RETURN_VALUE or
       ESI_RETURN_NO_VALUE.  If it's the former the return value from the
       redirected-to function should be propagated (unless the current call is a
       constructor call and the returned value isn't an object.)  If it's the
       latter, the return value from the redirected-to function should be
       discarded. */

    if (ip->instruction == ESI_RETURN_VALUE)
    {
        /* The bytecode compiler should have generated

             ESI_RETURN_VALUE reg(0)

           for this case. */
        OP_ASSERT(ip[1].index == 0);

        if (stored_in_constructor)
            use_return_value = &local_return_value;
        else
            use_return_value = &reg[0];
    }
    else
    {
        /* We only expect an ESI_REDIRECTED_CALL instruction to be followed
           by ESI_RETURN_VALUE or ESI_RETURN_NO_VALUE. */
        OP_ASSERT(ip->instruction == ESI_RETURN_NO_VALUE);

        use_return_value = NULL;
    }

    if (static_cast<ES_Function *>(apply.GetObject(this))->GetCall() != ES_FunctionBuiltins::apply || argc > ES_REDIRECTED_CALL_MAXIMUM_ARGUMENTS)
    {
        ES_FrameStackIterator frames(this);

        frames.Next();

        if (!frames.GetArgumentsObject())
            frames.SetArgumentsObject(ES_Arguments_Object::Make(this, static_cast<ES_Function *>(reg[1].GetObject(this)), reg, 0));

        ES_Object *args = frames.GetArgumentsObject();

        static_cast<ES_Arguments_Object *>(args)->SetUsed();

        ES_Value_Internal *registers = SetupFunctionCall(apply.GetObject(this), 2);

        registers[0] = function;
        registers[1] = apply;
        registers[2] = reg[0];
        registers[3].SetObject(args);

        if (!CallFunction(registers, 2, use_return_value))
            HANDLE_THROW();

        if (stored_in_constructor && use_return_value && use_return_value->IsObject())
            reg[0] = *use_return_value;

        IH_RETURN;
    }

    if (!function.IsObject() || !function.GetObject(this)->IsFunctionObject())
    {
        ThrowErrorBase(lip - 1, TYPE_ERROR, EXPECTED_FUNCTION);
        HANDLE_THROW();
        IH_RETURN;
    }

#ifdef ES_NATIVE_SUPPORT
    if (native_stack_frame)
        goto full_call;
#endif

    if (stored_in_constructor)
    {
        ES_Function *fn = static_cast<ES_Function *>(function.GetObject(this));

        if (!fn->GetFunctionCode() || !fn->GetFunctionCode()->GetData()->is_void_function)
            goto full_call;
    }

    reg[1] = function;

    PerformFunctionCall(lip, function, 0, argc, stored_in_constructor);

    IH_RETURN;

full_call:
    ES_Value_Internal *registers = SetupFunctionCall(function.GetObject(this), argc, ES_VirtualStackFrame::VIA_FUNCTION_APPLY);

    registers[0] = reg[0];
    registers[1] = function;

    for (unsigned index = 2; index < argc + 2; ++index)
        registers[index] = reg[index];

    if (!CallFunction(registers, argc, use_return_value))
        HANDLE_THROW();

    if (stored_in_constructor && use_return_value && use_return_value->IsObject())
        reg[0] = *use_return_value;

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_APPLY(IH_PARAM_DECL)
{
    IH_SETUP_CODE(2);

    unsigned frame = lip[0].index;
    unsigned argc = lip[1].index;

    ES_Value_Internal &apply = reg[frame];
    ES_Value_Internal &this_object = reg[frame + 1];
    ES_Value_Internal &function = reg[frame + 2];

    if (!apply.IsObject() || !apply.GetObject(this)->IsFunctionObject())
    {
        ThrowErrorBase(lip - 1, TYPE_ERROR, EXPECTED_FUNCTION);
        HANDLE_THROW();
        IH_RETURN;
    }

    if (static_cast<ES_Function *>(apply.GetObject(this))->GetCall() == ES_FunctionBuiltins::apply)
    {
        if (!function.IsObject() || !function.GetObject(this)->IsFunctionObject())
        {
            ThrowErrorBase(lip - 1, TYPE_ERROR, EXPECTED_FUNCTION);
            HANDLE_THROW();
            IH_RETURN;
        }

        ES_Object *fn = function.GetObject(this);
        BOOL is_strict_mode;

        if (this_object.IsNullOrUndefined() && fn->IsNativeFunctionObject(is_strict_mode) && !is_strict_mode)
            this_object.SetObject(GetGlobalObject());

#ifdef ES_NATIVE_SUPPORT
        if (native_stack_frame)
        {
            ES_Value_Internal *registers = SetupFunctionCall(fn, argc);

            registers[0] = this_object;
            registers[1] = function;

            for (unsigned index = 0; index < argc; ++index)
                registers[2 + index] = reg[frame + 3 + index];

            if (!CallFunction(registers, argc, &reg[frame + 1]))
                HANDLE_THROW();
        }
        else
#endif // ES_NATIVE_SUPPORT
            PerformFunctionCall(lip, function, frame + 1, argc, FALSE);
    }
    else
    {
        ES_Object *array = ES_Array::Make(this, GetGlobalObject(), argc, argc);
        ES_Compact_Indexed_Properties *compact = static_cast<ES_Compact_Indexed_Properties *>(array->GetIndexedProperties());

        for (unsigned rindex = frame + 3, windex = 0; windex < argc; ++rindex, ++windex)
            compact->PutSimpleNew(windex, reg[rindex]);

#ifdef ES_NATIVE_SUPPORT
        if (native_stack_frame)
        {
            ES_Value_Internal *registers = SetupFunctionCall(apply.GetObject(this), 2);

            registers[0] = function;
            registers[1] = apply;
            registers[2] = this_object;
            registers[3].SetObject(array);

            if (!CallFunction(registers, 2, &reg[frame + 1]))
                HANDLE_THROW();
        }
        else
#endif // ES_NATIVE_SUPPORT
        {
            ES_Value_Internal this_object_copy = this_object;

            reg[frame + 1] = function;
            reg[frame + 2] = apply;
            reg[frame + 3] = this_object_copy;
            reg[frame + 4].SetObject(array);

            PerformFunctionCall(lip, reg[frame + 2], frame + 1, 2, FALSE);
        }
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_CONSTRUCT(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    IH_SETUP(2);

    ES_CodeWord::Index rel_frame_start = lip[0].index;
    ES_CodeWord::Index argc = lip[1].index;

    const ES_Value_Internal &function_val = reg[rel_frame_start+1];
    if (!function_val.IsObject() || !function_val.GetObject(this)->IsFunctionObject())
    {
        // TODO: replace value with its name.
#ifdef ES_BYTECODE_DEBUGGER
        if (bytecode_debugger_enabled)
            ES_BytecodeDebugger::AtError(this, code, lip - 1, "TypeError", "value is not a constructor");
#endif // ES_BYTECODE_DEBUGGER

        ThrowErrorBase(lip - 1, TYPE_ERROR, EXPECTED_CONSTRUCTOR);
        HANDLE_THROW();
        IH_RETURN;
    }

    if (frame_stack.TotalUsed() > ES_MAXIMUM_FUNCTION_RECURSION)
    {
        ThrowRangeError("Maximum recursion depth exceeded", lip - 1);
        HANDLE_THROW();
        IH_RETURN;
    }

    ES_Function *function = static_cast<ES_Function *> (function_val.GetObject(this));

    if (--time_until_check == 0)
        CheckOutOfTime();

    ES_FunctionCode *called_code;
    if (!function->IsHostObject() && (called_code = function->GetFunctionCode()))
    {
        ABORT_IF_MEMORY_ERROR(frame_stack.Push(this));

        overlap = code->data->register_frame_size - rel_frame_start;
        ABORT_IF_MEMORY_ERROR(register_blocks.Allocate(this, first_in_block, reg, MAX(argc + 2, called_code->data->register_frame_size), overlap, argc + 2));

        in_constructor = TRUE;

        ES_Global_Object *global_object = GetGlobalObject();
        ES_Value_Internal prototype_value;
        ES_Object *prototype;
        GetResult result;

        if (function->Class() == global_object->GetNativeFunctionClass() || function->Class() == global_object->GetNativeFunctionWithPrototypeClass())
        {
            function->GetCachedAtIndex(ES_PropertyIndex(ES_Function::PROTOTYPE), prototype_value);

            if (prototype_value.IsSpecial())
                result = static_cast<ES_Special_Property *>(prototype_value.GetDecodedBoxed())->SpecialGetL(this, function, prototype_value, function);
            else
                result = PROP_GET_OK;
        }
        else
            result = function->GetL(this, RT_DATA.idents[ESID_prototype], prototype_value);

        if ((result == PROP_GET_OK || result == PROP_GET_OK_CAN_CACHE) && prototype_value.IsObject())
            prototype = prototype_value.GetObject(this);
        else
            prototype = global_object->GetObjectPrototype();

        ES_CollectorLock gclock(this, TRUE);
        ES_Class *klass = prototype->SetSubObjectClass(this);
        ES_Object *this_object = ES_Object::Make(this, klass);

        reg[0].SetObject(this_object);

#ifdef ECMASCRIPT_DEBUGGER
        if (IsDebugged())
        {
            SetCallDetails(reg[0], function, argc, reg + 2);
            SignalToDebugger(ES_DebugListener::ESEV_CALLSTARTING, lip - 1);
        }
#endif // ECMASCRIPT_DEBUGGER

        SetupCallState(argc, function);

#ifdef ECMASCRIPT_DEBUGGER
        SignalToDebugger(ES_DebugListener::ESEV_ENTERFN, ip);
#endif // ECMASCRIPT_DEBUGGER
    }
    else
    {
        ES_Function::BuiltIn *construct;
        if (function->IsHostObject())
            construct = ES_Host_Function::Construct;
        else
            construct = function->GetConstruct();

        if (!construct)
        {
            ThrowErrorBase(lip - 1, TYPE_ERROR, EXPECTED_CONSTRUCTOR);
            HANDLE_THROW();
            IH_RETURN;
        }

#ifdef ECMASCRIPT_DEBUGGER
        if (IsDebugged())
        {
            SetCallDetails(ES_Value_Internal(), function, argc, reg + rel_frame_start + 2);
            SignalToDebugger(ES_DebugListener::ESEV_CALLSTARTING, lip - 1);
        }
#endif // ECMASCRIPT_DEBUGGER

        if (!construct(this, argc, reg + rel_frame_start + 2, reg + rel_frame_start))
        {
            HANDLE_THROW();
            IH_RETURN;
        }

#ifdef ECMASCRIPT_DEBUGGER
        if (IsDebugged())
        {
            return_value = reg[rel_frame_start];
            SignalToDebugger(ES_DebugListener::ESEV_CALLCOMPLETED, lip - 1);
        }
#endif // ECMASCRIPT_DEBUGGER

        OP_ASSERT(reg[rel_frame_start].IsObject());
    }

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_RETURN_VALUE(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    IH_SETUP(1);

#ifdef ECMASCRIPT_DEBUGGER
    BOOL is_debugging_function = IsDebugged() && Code()->type == ES_Code::TYPE_FUNCTION;
    if (is_debugging_function)
        SetCallDetails(reg[0], reg[1].GetObject(this));
#endif // ECMASCRIPT_DEBUGGER

    ES_CodeWord::Index ret = lip[0].index;

    if (ret != 0 && (!in_constructor || reg[ret].IsObject()))
        reg[0] = reg[ret];

#ifdef ECMASCRIPT_DEBUGGER
    if (is_debugging_function)
    {
        return_value = reg[0];
        SignalToDebugger(ES_DebugListener::ESEV_RETURN, lip - 1);
        SignalToDebugger(ES_DebugListener::ESEV_LEAVEFN, lip - 1);
    }
#endif // ECMASCRIPT_DEBUGGER

    register_blocks.Free(code->data->register_frame_size, overlap, 1, first_in_block);

    frame_stack.Pop(this);

#ifdef ECMASCRIPT_DEBUGGER
    if (is_debugging_function)
        SignalToDebugger(ES_DebugListener::ESEV_CALLCOMPLETED, ip - 1);
#endif // ECMASCRIPT_DEBUGGER

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_RETURN_NO_VALUE(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    IH_SETUP(0);

#ifdef ECMASCRIPT_DEBUGGER
    BOOL is_debugging_function = IsDebugged() && Code()->type == ES_Code::TYPE_FUNCTION;
    if (is_debugging_function)
    {
        SetCallDetails(reg[0], reg[1].GetObject(this));
        return_value.SetUndefined();
        SignalToDebugger(ES_DebugListener::ESEV_RETURN, lip - 1);
        SignalToDebugger(ES_DebugListener::ESEV_LEAVEFN, lip - 1);
    }
#endif // ECMASCRIPT_DEBUGGER

    if (!in_constructor)
        reg[0].SetUndefined();

    register_blocks.Free(code->data->register_frame_size, overlap, 1, first_in_block);

    frame_stack.Pop(this);

#ifdef ECMASCRIPT_DEBUGGER
    if (is_debugging_function)
        SignalToDebugger(ES_DebugListener::ESEV_CALLCOMPLETED, ip - 1);
#endif // ECMASCRIPT_DEBUGGER

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_NEW_OBJECT(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index ret = lip[0].index;
    ES_Class *klass = Code()->GetObjectLiteralClass(this, lip[1].index);

    reg[ret].SetObject(ES_Object::Make(this, klass));

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_CONSTRUCT_OBJECT(IH_PARAM_DECL)
{
    IH_SETUP_CODE(2);

    ES_CodeWord::Index dst = lip[0].index;
    unsigned klass_index = lip[1].index;
    ES_Class *klass = code->GetObjectLiteralClass(this, klass_index, lip + 2, reg);
    ES_CodeWord::Index count = code->GetObjectLiteralClassCount(klass_index);

    IH_ADDITIONAL_OPERANDS(count);

    lip += 2;

    ES_Object *object;

    reg[dst].SetObject(object = ES_Object::Make(this, klass, count));

    BOOL klass_changed = FALSE;

    unsigned index = 0;
    for ( ; index < count; ++index)
    {
        ES_CodeWord::Index value = lip[index].index;
        if (reg[value].CheckType(klass->GetLayoutInfoAtIndex(ES_LayoutIndex(index)).GetStorageType()))
            object->PutCachedAtIndex(ES_PropertyIndex(index), reg[value]);
        else
        {
            object->SetPropertyCount(index);
            break;
        }
    }

    klass_changed = index != count;

    for ( ; index < count; ++index)
    {
        ES_CodeWord::Index value = lip[index].index;
        object->PutL(this, klass->GetNameAtIndex(ES_PropertyIndex(index)), reg[value]);
    }

    if (klass_changed)
        code->SetObjectLiteralClass(this, klass_index, object->Class());

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_NEW_ARRAY(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index len = lip[1].index;
    ES_Global_Object *global_object = GetGlobalObject();
    ES_Object *array;

    reg[dst].SetObject(array = ES_Array::Make(this, global_object, ES_Compact_Indexed_Properties::AppropriateCapacity(len)));

    ES_Array::SetLength(this, array, len);

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_CONSTRUCT_ARRAY(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index len = lip[1].index;
    ES_CodeWord::Index cal_index = lip[2].index;
    ES_Global_Object *global_object = GetGlobalObject();
    ES_Object *array;
    ES_CodeStatic::ConstantArrayLiteral &cal = Code()->data->constant_array_literals[cal_index];
    ES_Compact_Indexed_Properties *representation = Code()->constant_array_literals[cal_index];
    ES_Code *code = Code();

    if (!representation)
        code->constant_array_literals[cal_index] = representation = ES_Compact_Indexed_Properties::Make(this, code, cal);

    reg[dst].SetObject(array = ES_Array::Make(this, global_object, 0, len));

    array->SetIndexedProperties(representation);

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_NEW_FUNCTION(IH_PARAM_DECL)
{
    IH_SETUP(3);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index index = lip[1].index;
    ES_CodeWord::Index scope_index = lip[2].index;

    reg[dst].SetObject(NewFunction(Code()->functions[index], scope_index != UINT_MAX ? &Code()->data->inner_scopes[scope_index] : NULL));

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_NEW_REGEXP(IH_PARAM_DECL)
{
    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    ES_CodeWord::Index index = lip[1].index;
    ES_Global_Object *global_object = GetGlobalObject();
    ES_RegExp_Object *regexp;
    ES_Code *code = Code();

    BOOL can_share = (index & 0x80000000u) != 0;
    index &= 0x7fffffffu;

    if (can_share && (regexp = code->regexps[index]) != NULL)
    {
        if (global_object->IsPrototypeUnmodified(ES_Global_Object::PI_REGEXP))
        {
            regexp->Reset();
            reg[dst].SetObject(regexp);
            IH_RETURN;
        }

        code->regexps[index] = NULL;
    }

    ES_RegExp_Information &info = code->data->regexps[index];

    regexp = ES_RegExp_Object::Make(this, global_object, info, code->GetString(info.source));

    if (can_share)
        code->regexps[index] = regexp;

    reg[dst].SetObject(regexp);
    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_TABLE_SWITCH(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    IH_SETUP(2);

    ES_CodeWord::Index value = lip[0].index;
    ES_CodeStatic::SwitchTable &table = code->data->switch_tables[lip[1].index];
    int index;

    if (reg[value].IsInt32())
    {
        index = reg[value].GetInt32();

    use_index:
        if (index >= table.minimum && index <= table.maximum)
        {
            ip = code->data->codewords + table.codeword_indeces[index];
            IH_RETURN;
        }
    }
    else if (reg[value].IsDouble())
    {
        ES_Value_Internal converted(reg[value].GetDouble());

        if (converted.IsInt32())
        {
            index = converted.GetInt32();
            goto use_index;
        }
        else if (converted.GetDouble() == 0)
        {
            index = 0;
            goto use_index;
        }
    }

    ip = code->data->codewords + table.default_codeword_index;

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_CATCH(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    IH_SETUP(1);

    ES_CodeWord::Index dst = lip->index;

    reg[dst] = current_exception;
    current_exception.SetNoExceptionCookie();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_CATCH_SCOPE(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    IH_SETUP(2);

    ES_CodeWord::Index dst = lip[0].index;
    JString *name = String(lip[1].index);

    ES_Object *scope;

    reg[dst].SetObject(scope = ES_Object::Make(this, GetGlobalObject()->GetObjectClass()));

    scope->SetIsScopeObject();
    scope->PutL(this, name, current_exception);

    current_exception.SetNoExceptionCookie();

    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_THROW(IH_PARAM_DECL)
{
    IH_SETUP(1);

    ES_CodeWord::Index exception = lip->index;

    current_exception = reg[exception];

    ES_Error *error;

    if (current_exception.IsObject() && current_exception.GetObject()->IsErrorObject())
    {
        error = static_cast<ES_Error *>(current_exception.GetObject());
        if (error->GetStackTraceLength() != 0)
            error = NULL;
    }
    else
        error = NULL;

    CaptureStackTrace(lip - 1, error);

# ifdef ECMASCRIPT_DEBUGGER
    SignalToDebugger(ES_DebugListener::ESEV_EXCEPTION, lip);
# endif // ECMASCRIPT_DEBUGGER

    HANDLE_THROW();
    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_THROW_BUILTIN(IH_PARAM_DECL)
{
    IH_SETUP(1);

    unsigned exception = lip->index;

    if (exception == ES_CodeStatic::TYPEERROR_EXPECTED_LVALUE)
        ThrowErrorBase(lip - 1, REFERENCE_ERROR, EXPECTED_LVALUE);
    else
        ThrowTypeError("Invalid assignment in strict mode", lip - 1);

    HANDLE_THROW();
    IH_RETURN;
}

IH_SPECIFIER ES_Execution_Context::IH_RETHROW(IH_PARAM_DECL)
{
#ifdef ES_NATIVE_SUPPORT
    OP_ASSERT(!native_stack_frame);
#endif // ES_NATIVE_SUPPORT

    IH_SETUP(3);

    ES_CodeWord::Index exception = lip[0].index;
    ES_CodeWord::Index target = lip[1].index;
    ES_CodeWord::Index nexttarget = lip[2].index;

    if (!reg[exception].IsNoExceptionCookie())
    {
        current_exception = reg[exception];
        HANDLE_THROW();
        IH_RETURN;
    }

    // handle finally sets implicit bool
    HANDLE_FINALLY(reg[target].GetInt32());

    if (implicit_bool)
        reg[nexttarget] = reg[target];

    IH_RETURN;
}

#ifdef ECMASCRIPT_DEBUGGER
IH_SPECIFIER ES_Execution_Context::IH_DEBUGGER_STOP(IH_PARAM_DECL)
{
    IH_SETUP(1);

    ES_DebugListener::EventType type = static_cast<ES_DebugListener::EventType>(lip->index);
    SignalToDebugger(type, lip);

    IH_RETURN;
}
#endif // ECMASCRIPT_DEBUGGER
