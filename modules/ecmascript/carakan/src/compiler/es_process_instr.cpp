/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Optimizer: generated per-instruction code.
 *
 * @author modules/ecmascript/carakan/src/scripts/es_process_instr.cpp.py
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_analyzer.h"

#ifdef ES_NATIVE_SUPPORT

void
ES_Analyzer::ProcessInstruction(Input *&iptr, Output *&optr)
{
    switch (word++->instruction)
    {
    case ESI_LOAD_STRING:
        *optr++ = Output(word++->index, TRUE, ESTYPE_STRING);
        ++word;
        break;

    case ESI_LOAD_DOUBLE:
        *optr++ = Output(word++->index, TRUE, ESTYPE_DOUBLE);
        ++word;
        break;

    case ESI_LOAD_INT32:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32);
        ++word;
        break;

    case ESI_LOAD_NULL:
        *optr++ = Output(word++->index, TRUE, ESTYPE_NULL);
        break;

    case ESI_LOAD_UNDEFINED:
        *optr++ = Output(word++->index, TRUE, ESTYPE_UNDEFINED);
        break;

    case ESI_LOAD_TRUE:
        *optr++ = Output(word++->index, TRUE, ESTYPE_BOOLEAN);
        break;

    case ESI_LOAD_FALSE:
        *optr++ = Output(word++->index, TRUE, ESTYPE_BOOLEAN);
        break;

    case ESI_LOAD_GLOBAL_OBJECT:
        *optr++ = Output(word++->index, TRUE, ESTYPE_OBJECT);
        break;

    case ESI_TYPEOF:
        *optr++ = Output(word++->index, TRUE, ESTYPE_STRING);
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_TONUMBER:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_TOOBJECT:
        *optr++ = Output(word++->index, TRUE, ESTYPE_OBJECT);
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_TOPRIMITIVE:
        *optr++ = Output(word++->index);
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_TOPRIMITIVE1:
        iptr->index = word->index;
        iptr->has_type = FALSE;
        ++iptr;
        *optr++ = Output(word++->index);
        ++word;
        break;

    case ESI_IS_NULL_OR_UNDEFINED:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_IS_NOT_NULL_OR_UNDEFINED:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_ADD_IMM:
        *optr++ = Output(word++->index);
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_FORMAT_STRING:
        *optr++ = Output(word++->index, TRUE, ESTYPE_STRING);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_STRING;
        ++iptr;
        ++word;
        ++word;
        ++word;
        break;

    case ESI_SUB:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        break;

    case ESI_SUB_IMM:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        ++word;
        break;

    case ESI_MUL:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        break;

    case ESI_MUL_IMM:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        ++word;
        break;

    case ESI_DIV:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        break;

    case ESI_DIV_IMM:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        ++word;
        break;

    case ESI_REM:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        break;

    case ESI_REM_IMM:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        ++word;
        break;

    case ESI_LSHIFT:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        break;

    case ESI_LSHIFT_IMM:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        ++word;
        break;

    case ESI_RSHIFT_SIGNED:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        break;

    case ESI_RSHIFT_SIGNED_IMM:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        ++word;
        break;

    case ESI_RSHIFT_UNSIGNED:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        break;

    case ESI_RSHIFT_UNSIGNED_IMM:
        *optr++ = Output(word++->index);
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_NEG:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        break;

    case ESI_COMPL:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        break;

    case ESI_INC:
        iptr->index = word->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        break;

    case ESI_DEC:
        iptr->index = word->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32_OR_DOUBLE);
        break;

    case ESI_BITAND:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        break;

    case ESI_BITAND_IMM:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        ++word;
        break;

    case ESI_BITOR:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        break;

    case ESI_BITOR_IMM:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        ++word;
        break;

    case ESI_BITXOR:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        break;

    case ESI_BITXOR_IMM:
        *optr++ = Output(word++->index, TRUE, ESTYPE_INT32);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        ++word;
        break;

    case ESI_NOT:
        *optr++ = Output(word++->index, TRUE, ESTYPE_BOOLEAN);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_BOOLEAN;
        ++iptr;
        break;

    case ESI_EQ:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_EQ_IMM:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_NEQ:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_NEQ_IMM:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_EQ_STRICT:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_EQ_STRICT_IMM:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_NEQ_STRICT:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_NEQ_STRICT_IMM:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_LT:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_LT_IMM:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        ++word;
        break;

    case ESI_LTE:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_LTE_IMM:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        ++word;
        break;

    case ESI_GT:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_GT_IMM:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        ++word;
        break;

    case ESI_GTE:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_GTE_IMM:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32_OR_DOUBLE;
        ++iptr;
        ++word;
        break;

    case ESI_CONDITION:
        iptr->index = word->index;
        iptr->has_type = FALSE;
        ++iptr;
        *optr++ = Output(word++->index);
        break;

    case ESI_JUMP:
        ++word;
        break;

    case ESI_JUMP_INDIRECT:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_JUMP_IF_TRUE:
        ++word;
        ++word;
        break;

    case ESI_JUMP_IF_FALSE:
        ++word;
        ++word;
        break;

    case ESI_START_LOOP:
        ++word;
        break;

    case ESI_STORE_BOOLEAN:
        *optr++ = Output(word++->index, TRUE, ESTYPE_BOOLEAN);
        break;

    case ESI_GETN_IMM:
        *optr++ = Output(word++->index);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        ++word;
        ++word;
        break;

    case ESI_PUTN_IMM:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        ++word;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_INIT_PROPERTY:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        ++word;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_GET_LENGTH:
        *optr++ = Output(word++->index);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        ++word;
        ++word;
        break;

    case ESI_PUT_LENGTH:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        ++word;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_GETI_IMM:
        *optr++ = Output(word++->index);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        ++word;
        break;

    case ESI_PUTI_IMM:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        ++word;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_GET:
        *optr++ = Output(word++->index);
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_PUT:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_DEFINE_GETTER:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        ++word;
        ++word;
        ++word;
        break;

    case ESI_DEFINE_SETTER:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        ++word;
        ++word;
        ++word;
        break;

    case ESI_GET_SCOPE:
        *optr++ = Output(word++->index);
        ++word;
        ++word;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_GET_SCOPE_REF:
        *optr++ = Output(word++->index);
        *optr++ = Output(word++->index, TRUE, ESTYPE_OBJECT);
        ++word;
        ++word;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_PUT_SCOPE:
        ++word;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        *optr++ = Output(word++->index, TRUE);
        break;

    case ESI_DELETE_SCOPE:
        ++word;
        ++word;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_GET_GLOBAL:
        *optr++ = Output(word++->index);
        ++word;
        ++word;
        break;

    case ESI_PUT_GLOBAL:
        ++word;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        ++word;
        break;

    case ESI_GET_LEXICAL:
        *optr++ = Output(word++->index);
        ++word;
        ++word;
        break;

    case ESI_PUT_LEXICAL:
        ++word;
        ++word;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_DELETEN_IMM:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        ++word;
        break;

    case ESI_DELETEI_IMM:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        ++word;
        break;

    case ESI_DELETE:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_DECLARE_GLOBAL:
        ++word;
        break;

    case ESI_HASPROPERTY:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_INSTANCEOF:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        break;

    case ESI_ENUMERATE:
        *optr++ = Output(word++->index);
        iptr->index = word->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        *optr++ = Output(word++->index, TRUE, ESTYPE_OBJECT);
        *optr++ = Output(word++->index);
        break;

    case ESI_NEXT_PROPERTY:
        *optr++ = Output(word++->index, TRUE, ESTYPE_STRING);
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_OBJECT;
        ++iptr;
        iptr->index = word->index;
        iptr->has_type = FALSE;
        ++iptr;
        *optr++ = Output(word++->index);
        break;

    case ESI_REDIRECTED_CALL:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_APPLY:
        iptr->index = word->index;
        iptr->has_type = FALSE;
        ++iptr;
        *optr++ = Output(word++->index);
        ++word;
        break;

    case ESI_RETURN_VALUE:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_RETURN_NO_VALUE:
        break;

    case ESI_RETURN_FROM_EVAL:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_NEW_OBJECT:
        *optr++ = Output(word++->index, TRUE, ESTYPE_OBJECT);
        ++word;
        break;

    case ESI_NEW_ARRAY:
        *optr++ = Output(word++->index, TRUE, ESTYPE_OBJECT);
        ++word;
        break;

    case ESI_CONSTRUCT_ARRAY:
        *optr++ = Output(word++->index, TRUE, ESTYPE_OBJECT);
        ++word;
        ++word;
        break;

    case ESI_NEW_FUNCTION:
        *optr++ = Output(word++->index, TRUE, ESTYPE_OBJECT);
        ++word;
        ++word;
        break;

    case ESI_NEW_REGEXP:
        *optr++ = Output(word++->index, TRUE, ESTYPE_OBJECT);
        ++word;
        break;

    case ESI_TABLE_SWITCH:
        iptr->index = word++->index;
        iptr->has_type = TRUE;
        iptr->has_forced_type = TRUE;
        iptr->type = ESTYPE_INT32;
        ++iptr;
        ++word;
        break;

    case ESI_CATCH:
        *optr++ = Output(word++->index);
        break;

    case ESI_CATCH_SCOPE:
        *optr++ = Output(word++->index);
        ++word;
        break;

    case ESI_THROW:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        break;

    case ESI_THROW_BUILTIN:
        ++word;
        break;

    case ESI_RETHROW:
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        iptr->index = word++->index;
        iptr->has_type = FALSE;
        ++iptr;
        *optr++ = Output(word++->index);
        break;

    case ESI_EXIT:
        break;

    case ESI_EXIT_TO_BUILTIN:
        break;

    case ESI_EXIT_TO_EVAL:
        break;

#ifdef ECMASCRIPT_DEBUGGER
    case ESI_DEBUGGER_STOP:
        ++word;
        break;
#endif

    }
}

#endif // ES_NATIVE_SUPPORT
