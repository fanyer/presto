/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Table of instruction names.
 *
 * @author modules/ecmascript/carakan/src/scripts/es_instructionhandlers.cpp.py
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#ifdef ES_NATIVE_SUPPORT

static void *
ViolateFunctionPointer(IH_FUNCTION_PTR(ptr))
{
    union { IH_FUNCTION_PTR(fn); void *v; } u;
    u.fn = ptr;
    return u.v;
}

void ES_SetupFunctionHandlers()
{
    void **ihs = g_ecma_instruction_handlers = OP_NEWA_L(void *, 122);

    ihs[ESI_LOAD_STRING] = ViolateFunctionPointer(&ES_Execution_Context::IH_LOAD_STRING);
    ihs[ESI_LOAD_DOUBLE] = ViolateFunctionPointer(&ES_Execution_Context::IH_LOAD_DOUBLE);
    ihs[ESI_LOAD_INT32] = ViolateFunctionPointer(&ES_Execution_Context::IH_LOAD_INT32);
    ihs[ESI_LOAD_NULL] = ViolateFunctionPointer(&ES_Execution_Context::IH_LOAD_NULL);
    ihs[ESI_LOAD_UNDEFINED] = ViolateFunctionPointer(&ES_Execution_Context::IH_LOAD_UNDEFINED);
    ihs[ESI_LOAD_TRUE] = ViolateFunctionPointer(&ES_Execution_Context::IH_LOAD_TRUE);
    ihs[ESI_LOAD_FALSE] = ViolateFunctionPointer(&ES_Execution_Context::IH_LOAD_FALSE);
    ihs[ESI_LOAD_GLOBAL_OBJECT] = ViolateFunctionPointer(&ES_Execution_Context::IH_LOAD_GLOBAL_OBJECT);
    ihs[ESI_COPY] = ViolateFunctionPointer(&ES_Execution_Context::IH_COPY);
    ihs[ESI_COPYN] = ViolateFunctionPointer(&ES_Execution_Context::IH_COPYN);
    ihs[ESI_TYPEOF] = ViolateFunctionPointer(&ES_Execution_Context::IH_TYPEOF);
    ihs[ESI_TONUMBER] = ViolateFunctionPointer(&ES_Execution_Context::IH_TONUMBER);
    ihs[ESI_TOOBJECT] = ViolateFunctionPointer(&ES_Execution_Context::IH_TOOBJECT);
    ihs[ESI_TOPRIMITIVE] = ViolateFunctionPointer(&ES_Execution_Context::IH_TOPRIMITIVE);
    ihs[ESI_TOPRIMITIVE1] = ViolateFunctionPointer(&ES_Execution_Context::IH_TOPRIMITIVE1);
    ihs[ESI_IS_NULL_OR_UNDEFINED] = ViolateFunctionPointer(&ES_Execution_Context::IH_IS_NULL_OR_UNDEFINED);
    ihs[ESI_IS_NOT_NULL_OR_UNDEFINED] = ViolateFunctionPointer(&ES_Execution_Context::IH_IS_NOT_NULL_OR_UNDEFINED);
    ihs[ESI_ADD] = ViolateFunctionPointer(&ES_Execution_Context::IH_ADD);
    ihs[ESI_ADD_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_ADD_IMM);
#ifdef ES_COMBINED_ADD_SUPPORT
    ihs[ESI_ADDN] = ViolateFunctionPointer(&ES_Execution_Context::IH_ADDN);
#endif
    ihs[ESI_FORMAT_STRING] = ViolateFunctionPointer(&ES_Execution_Context::IH_FORMAT_STRING);
    ihs[ESI_SUB] = ViolateFunctionPointer(&ES_Execution_Context::IH_SUB);
    ihs[ESI_SUB_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_SUB_IMM);
    ihs[ESI_MUL] = ViolateFunctionPointer(&ES_Execution_Context::IH_MUL);
    ihs[ESI_MUL_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_MUL_IMM);
    ihs[ESI_DIV] = ViolateFunctionPointer(&ES_Execution_Context::IH_DIV);
    ihs[ESI_DIV_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_DIV_IMM);
    ihs[ESI_REM] = ViolateFunctionPointer(&ES_Execution_Context::IH_REM);
    ihs[ESI_REM_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_REM_IMM);
    ihs[ESI_LSHIFT] = ViolateFunctionPointer(&ES_Execution_Context::IH_LSHIFT);
    ihs[ESI_LSHIFT_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_LSHIFT_IMM);
    ihs[ESI_RSHIFT_SIGNED] = ViolateFunctionPointer(&ES_Execution_Context::IH_RSHIFT_SIGNED);
    ihs[ESI_RSHIFT_SIGNED_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_RSHIFT_SIGNED_IMM);
    ihs[ESI_RSHIFT_UNSIGNED] = ViolateFunctionPointer(&ES_Execution_Context::IH_RSHIFT_UNSIGNED);
    ihs[ESI_RSHIFT_UNSIGNED_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_RSHIFT_UNSIGNED_IMM);
    ihs[ESI_NEG] = ViolateFunctionPointer(&ES_Execution_Context::IH_NEG);
    ihs[ESI_COMPL] = ViolateFunctionPointer(&ES_Execution_Context::IH_COMPL);
    ihs[ESI_INC] = ViolateFunctionPointer(&ES_Execution_Context::IH_INC);
    ihs[ESI_DEC] = ViolateFunctionPointer(&ES_Execution_Context::IH_DEC);
    ihs[ESI_BITAND] = ViolateFunctionPointer(&ES_Execution_Context::IH_BITAND);
    ihs[ESI_BITAND_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_BITAND_IMM);
    ihs[ESI_BITOR] = ViolateFunctionPointer(&ES_Execution_Context::IH_BITOR);
    ihs[ESI_BITOR_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_BITOR_IMM);
    ihs[ESI_BITXOR] = ViolateFunctionPointer(&ES_Execution_Context::IH_BITXOR);
    ihs[ESI_BITXOR_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_BITXOR_IMM);
    ihs[ESI_NOT] = ViolateFunctionPointer(&ES_Execution_Context::IH_NOT);
    ihs[ESI_EQ] = ViolateFunctionPointer(&ES_Execution_Context::IH_EQ);
    ihs[ESI_EQ_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_EQ_IMM);
    ihs[ESI_NEQ] = ViolateFunctionPointer(&ES_Execution_Context::IH_NEQ);
    ihs[ESI_NEQ_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_NEQ_IMM);
    ihs[ESI_EQ_STRICT] = ViolateFunctionPointer(&ES_Execution_Context::IH_EQ_STRICT);
    ihs[ESI_EQ_STRICT_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_EQ_STRICT_IMM);
    ihs[ESI_NEQ_STRICT] = ViolateFunctionPointer(&ES_Execution_Context::IH_NEQ_STRICT);
    ihs[ESI_NEQ_STRICT_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_NEQ_STRICT_IMM);
    ihs[ESI_LT] = ViolateFunctionPointer(&ES_Execution_Context::IH_LT);
    ihs[ESI_LT_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_LT_IMM);
    ihs[ESI_LTE] = ViolateFunctionPointer(&ES_Execution_Context::IH_LTE);
    ihs[ESI_LTE_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_LTE_IMM);
    ihs[ESI_GT] = ViolateFunctionPointer(&ES_Execution_Context::IH_GT);
    ihs[ESI_GT_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_GT_IMM);
    ihs[ESI_GTE] = ViolateFunctionPointer(&ES_Execution_Context::IH_GTE);
    ihs[ESI_GTE_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_GTE_IMM);
    ihs[ESI_CONDITION] = ViolateFunctionPointer(&ES_Execution_Context::IH_CONDITION);
    ihs[ESI_JUMP] = ViolateFunctionPointer(&ES_Execution_Context::IH_JUMP);
    ihs[ESI_JUMP_INDIRECT] = ViolateFunctionPointer(&ES_Execution_Context::IH_JUMP_INDIRECT);
    ihs[ESI_JUMP_IF_TRUE] = ViolateFunctionPointer(&ES_Execution_Context::IH_JUMP_IF_TRUE);
    ihs[ESI_JUMP_IF_FALSE] = ViolateFunctionPointer(&ES_Execution_Context::IH_JUMP_IF_FALSE);
    ihs[ESI_START_LOOP] = ViolateFunctionPointer(&ES_Execution_Context::IH_START_LOOP);
    ihs[ESI_STORE_BOOLEAN] = ViolateFunctionPointer(&ES_Execution_Context::IH_STORE_BOOLEAN);
    ihs[ESI_GETN_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_GETN_IMM);
    ihs[ESI_PUTN_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_PUTN_IMM);
    ihs[ESI_INIT_PROPERTY] = ViolateFunctionPointer(&ES_Execution_Context::IH_PUTN_IMM);
    ihs[ESI_GET_LENGTH] = ViolateFunctionPointer(&ES_Execution_Context::IH_GET_LENGTH);
    ihs[ESI_PUT_LENGTH] = ViolateFunctionPointer(&ES_Execution_Context::IH_PUT_LENGTH);
    ihs[ESI_GETI_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_GETI_IMM);
    ihs[ESI_PUTI_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_PUTI_IMM);
    ihs[ESI_GET] = ViolateFunctionPointer(&ES_Execution_Context::IH_GET);
    ihs[ESI_PUT] = ViolateFunctionPointer(&ES_Execution_Context::IH_PUT);
    ihs[ESI_DEFINE_GETTER] = ViolateFunctionPointer(&ES_Execution_Context::IH_DEFINE_GETTER);
    ihs[ESI_DEFINE_SETTER] = ViolateFunctionPointer(&ES_Execution_Context::IH_DEFINE_SETTER);
    ihs[ESI_GET_SCOPE] = ViolateFunctionPointer(&ES_Execution_Context::IH_GET_SCOPE);
    ihs[ESI_GET_SCOPE_REF] = ViolateFunctionPointer(&ES_Execution_Context::IH_GET_SCOPE_REF);
    ihs[ESI_PUT_SCOPE] = ViolateFunctionPointer(&ES_Execution_Context::IH_PUT_SCOPE);
    ihs[ESI_DELETE_SCOPE] = ViolateFunctionPointer(&ES_Execution_Context::IH_DELETE_SCOPE);
    ihs[ESI_GET_GLOBAL] = ViolateFunctionPointer(&ES_Execution_Context::IH_GET_GLOBAL);
    ihs[ESI_PUT_GLOBAL] = ViolateFunctionPointer(&ES_Execution_Context::IH_PUT_GLOBAL);
    ihs[ESI_GET_LEXICAL] = ViolateFunctionPointer(&ES_Execution_Context::IH_GET_LEXICAL);
    ihs[ESI_PUT_LEXICAL] = ViolateFunctionPointer(&ES_Execution_Context::IH_PUT_LEXICAL);
    ihs[ESI_DELETEN_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_DELETEN_IMM);
    ihs[ESI_DELETEI_IMM] = ViolateFunctionPointer(&ES_Execution_Context::IH_DELETEI_IMM);
    ihs[ESI_DELETE] = ViolateFunctionPointer(&ES_Execution_Context::IH_DELETE);
    ihs[ESI_DECLARE_GLOBAL] = ViolateFunctionPointer(&ES_Execution_Context::IH_DECLARE_GLOBAL);
    ihs[ESI_HASPROPERTY] = ViolateFunctionPointer(&ES_Execution_Context::IH_HASPROPERTY);
    ihs[ESI_INSTANCEOF] = ViolateFunctionPointer(&ES_Execution_Context::IH_INSTANCEOF);
    ihs[ESI_ENUMERATE] = ViolateFunctionPointer(&ES_Execution_Context::IH_ENUMERATE);
    ihs[ESI_NEXT_PROPERTY] = ViolateFunctionPointer(&ES_Execution_Context::IH_NEXT_PROPERTY);
    ihs[ESI_EVAL] = ViolateFunctionPointer(&ES_Execution_Context::IH_EVAL);
    ihs[ESI_CALL] = ViolateFunctionPointer(&ES_Execution_Context::IH_CALL);
    ihs[ESI_REDIRECTED_CALL] = ViolateFunctionPointer(&ES_Execution_Context::IH_REDIRECTED_CALL);
    ihs[ESI_APPLY] = ViolateFunctionPointer(&ES_Execution_Context::IH_APPLY);
    ihs[ESI_CONSTRUCT] = ViolateFunctionPointer(&ES_Execution_Context::IH_CONSTRUCT);
    ihs[ESI_RETURN_VALUE] = ViolateFunctionPointer(&ES_Execution_Context::IH_RETURN_VALUE);
    ihs[ESI_RETURN_NO_VALUE] = ViolateFunctionPointer(&ES_Execution_Context::IH_RETURN_NO_VALUE);
    ihs[ESI_RETURN_FROM_EVAL] = NULL;
    ihs[ESI_NEW_OBJECT] = ViolateFunctionPointer(&ES_Execution_Context::IH_NEW_OBJECT);
    ihs[ESI_CONSTRUCT_OBJECT] = ViolateFunctionPointer(&ES_Execution_Context::IH_CONSTRUCT_OBJECT);
    ihs[ESI_NEW_ARRAY] = ViolateFunctionPointer(&ES_Execution_Context::IH_NEW_ARRAY);
    ihs[ESI_CONSTRUCT_ARRAY] = ViolateFunctionPointer(&ES_Execution_Context::IH_CONSTRUCT_ARRAY);
    ihs[ESI_NEW_FUNCTION] = ViolateFunctionPointer(&ES_Execution_Context::IH_NEW_FUNCTION);
    ihs[ESI_NEW_REGEXP] = ViolateFunctionPointer(&ES_Execution_Context::IH_NEW_REGEXP);
    ihs[ESI_TABLE_SWITCH] = ViolateFunctionPointer(&ES_Execution_Context::IH_TABLE_SWITCH);
    ihs[ESI_CATCH] = ViolateFunctionPointer(&ES_Execution_Context::IH_CATCH);
    ihs[ESI_CATCH_SCOPE] = ViolateFunctionPointer(&ES_Execution_Context::IH_CATCH_SCOPE);
    ihs[ESI_THROW] = ViolateFunctionPointer(&ES_Execution_Context::IH_THROW);
    ihs[ESI_THROW_BUILTIN] = ViolateFunctionPointer(&ES_Execution_Context::IH_THROW_BUILTIN);
    ihs[ESI_RETHROW] = ViolateFunctionPointer(&ES_Execution_Context::IH_RETHROW);
    ihs[ESI_EXIT] = NULL;
    ihs[ESI_EXIT_TO_BUILTIN] = NULL;
    ihs[ESI_EXIT_TO_EVAL] = NULL;
#ifdef ECMASCRIPT_DEBUGGER
    ihs[ESI_DEBUGGER_STOP] = ViolateFunctionPointer(&ES_Execution_Context::IH_DEBUGGER_STOP);
#endif
}

#endif // ES_NATIVE_SUPPORT
