/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Disassembler: generated per-instruction code.
 *
 * @author modules/ecmascript/carakan/src/scripts/es_disassembler_instr.cpp.py
 */

#include "core/pch.h"

#ifdef ES_DISASSEMBLER_SUPPORT

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_disassembler.h"

void
ES_Disassembler::DisassembleInstruction(ES_Code *code, ES_CodeWord *&word, ES_CodeWord *codewords)
{
    switch (word++->instruction)
    {
    case ESI_LOAD_STRING:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputString(context, code, word++->index);
        break;

    case ESI_LOAD_DOUBLE:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputDouble(context, code, word++->index);
        break;

    case ESI_LOAD_INT32:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputImmediate(word++->immediate, UNI_L("value"));
        break;

    case ESI_LOAD_NULL:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        break;

    case ESI_LOAD_UNDEFINED:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        break;

    case ESI_LOAD_TRUE:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        break;

    case ESI_LOAD_FALSE:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        break;

    case ESI_LOAD_GLOBAL_OBJECT:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        break;

    case ESI_COPY:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        break;

    case ESI_COPYN:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputImmediate(word++->immediate, UNI_L("count"));
        OutputRegister(word++->index & 0x7fffffffu, "srcN");
        break;

    case ESI_TYPEOF:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        break;

    case ESI_TONUMBER:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        break;

    case ESI_TOOBJECT:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        break;

    case ESI_TOPRIMITIVE:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        OutputImmediate(word++->immediate, UNI_L("hint"));
        break;

    case ESI_TOPRIMITIVE1:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputImmediate(word++->immediate, UNI_L("hint"));
        break;

    case ESI_IS_NULL_OR_UNDEFINED:
        OutputRegister(word++->index & 0x7fffffffu, "src");
        break;

    case ESI_IS_NOT_NULL_OR_UNDEFINED:
        OutputRegister(word++->index & 0x7fffffffu, "src");
        break;

    case ESI_ADD:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_ADD_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

#ifdef ES_COMBINED_ADD_SUPPORT
    case ESI_ADDN:
        {
            OutputRegister(word++->index, "dst");
            unsigned count = word++->index;
            OutputImmediate(count, UNI_L("count"));
            for (unsigned index = 0; index < count; ++index)
                OutputRegister(word++->index);
        }
        break;
#endif

    case ESI_FORMAT_STRING:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputString(context, code, word++->index);
        OutputString(context, code, word++->index);
        OutputImmediate(word++->immediate, UNI_L("cache"));
        break;

    case ESI_SUB:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_SUB_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_MUL:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_MUL_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_DIV:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "dividend");
        OutputRegister(word++->index & 0x7fffffffu, "divisor");
        break;

    case ESI_DIV_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "dividend");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_REM:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "dividend");
        OutputRegister(word++->index & 0x7fffffffu, "divisor");
        break;

    case ESI_REM_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "dividend");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_LSHIFT:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        OutputRegister(word++->index & 0x7fffffffu, "count");
        break;

    case ESI_LSHIFT_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        OutputImmediate(word++->immediate, UNI_L("count"));
        break;

    case ESI_RSHIFT_SIGNED:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        OutputRegister(word++->index & 0x7fffffffu, "count");
        break;

    case ESI_RSHIFT_SIGNED_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        OutputImmediate(word++->immediate, UNI_L("count"));
        break;

    case ESI_RSHIFT_UNSIGNED:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        OutputRegister(word++->index & 0x7fffffffu, "count");
        break;

    case ESI_RSHIFT_UNSIGNED_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        OutputImmediate(word++->immediate, UNI_L("count"));
        break;

    case ESI_NEG:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        break;

    case ESI_COMPL:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        break;

    case ESI_INC:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        break;

    case ESI_DEC:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        break;

    case ESI_BITAND:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_BITAND_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_BITOR:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_BITOR_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_BITXOR:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_BITXOR_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_NOT:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        OutputRegister(word++->index & 0x7fffffffu, "src");
        break;

    case ESI_EQ:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_EQ_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_NEQ:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_NEQ_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_EQ_STRICT:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_EQ_STRICT_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_NEQ_STRICT:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_NEQ_STRICT_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_LT:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_LT_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_LTE:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_LTE_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_GT:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_GT_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_GTE:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputRegister(word++->index & 0x7fffffffu, "rhs");
        break;

    case ESI_GTE_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "lhs");
        OutputImmediate(word++->immediate, UNI_L("imm"));
        break;

    case ESI_CONDITION:
        OutputRegister(word++->index & 0x7fffffffu, "src");
        break;

    case ESI_JUMP:
        OutputOffset(word->offset, word - codewords);
        ++word;
        break;

    case ESI_JUMP_INDIRECT:
        OutputRegister(word++->index & 0x7fffffffu, "address");
        break;

    case ESI_JUMP_IF_TRUE:
        OutputOffset(word->offset, word - codewords);
        ++word;
        OutputImmediate(word++->immediate, UNI_L("data"));
        break;

    case ESI_JUMP_IF_FALSE:
        OutputOffset(word->offset, word - codewords);
        ++word;
        OutputImmediate(word++->immediate, UNI_L("data"));
        break;

    case ESI_START_LOOP:
        OutputImmediate(word++->immediate, UNI_L("end"));
        break;

    case ESI_STORE_BOOLEAN:
        OutputRegister(word++->index & 0x7fffffffu, "dst");
        break;

    case ESI_GETN_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputIdentifier(code, *word++);
        OutputPropertyCache(code, word++->immediate);
        break;

    case ESI_PUTN_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputIdentifier(code, *word++);
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputPropertyCache(code, word++->immediate);
        break;

    case ESI_INIT_PROPERTY:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputIdentifier(code, *word++);
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputPropertyCache(code, word++->immediate);
        break;

    case ESI_GET_LENGTH:
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputIdentifier(code, *word++);
        OutputPropertyCache(code, word++->immediate);
        break;

    case ESI_PUT_LENGTH:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputIdentifier(code, *word++);
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputPropertyCache(code, word++->immediate);
        break;

    case ESI_GETI_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputImmediate(word++->immediate, UNI_L("index"));
        break;

    case ESI_PUTI_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputImmediate(word++->immediate, UNI_L("index"));
        OutputRegister(word++->index & 0x7fffffffu, "value");
        break;

    case ESI_GET:
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputRegister(word++->index & 0x7fffffffu, "name");
        break;

    case ESI_PUT:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputRegister(word++->index & 0x7fffffffu, "name");
        OutputRegister(word++->index & 0x7fffffffu, "value");
        break;

    case ESI_DEFINE_GETTER:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputIdentifier(code, *word++);
        OutputImmediate(word++->index, UNI_L("getter"));
        OutputImmediate(word++->immediate, UNI_L("innerScopes"));
        break;

    case ESI_DEFINE_SETTER:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputIdentifier(code, *word++);
        OutputImmediate(word++->index, UNI_L("setter"));
        OutputImmediate(word++->immediate, UNI_L("innerScopes"));
        break;

    case ESI_GET_SCOPE:
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputIdentifier(code, *word++);
        OutputImmediate(word++->immediate, UNI_L("innerScopes"));
        OutputRegister(word++->index & 0x7fffffffu, "local");
        OutputGlobalCache(code, word++->immediate);
        break;

    case ESI_GET_SCOPE_REF:
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputIdentifier(code, *word++);
        OutputImmediate(word++->immediate, UNI_L("innerScopes"));
        OutputRegister(word++->index & 0x7fffffffu, "local");
        OutputGlobalCache(code, word++->immediate);
        break;

    case ESI_PUT_SCOPE:
        OutputIdentifier(code, *word++);
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputImmediate(word++->immediate, UNI_L("innerScopes"));
        OutputRegister(word++->index & 0x7fffffffu, "local");
        break;

    case ESI_DELETE_SCOPE:
        OutputIdentifier(code, *word++);
        OutputImmediate(word++->immediate, UNI_L("innerScopes"));
        OutputRegister(word++->index & 0x7fffffffu, "local");
        break;

    case ESI_GET_GLOBAL:
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputIdentifier(code, *word++);
        OutputGlobalCache(code, word++->immediate);
        break;

    case ESI_PUT_GLOBAL:
        OutputIdentifier(code, *word++);
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputGlobalCache(code, word++->immediate);
        break;

    case ESI_GET_LEXICAL:
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputImmediate(word++->immediate, UNI_L("scope"));
        OutputImmediate(word++->immediate, UNI_L("index"));
        break;

    case ESI_PUT_LEXICAL:
        OutputImmediate(word++->immediate, UNI_L("scope"));
        OutputImmediate(word++->immediate, UNI_L("index"));
        OutputRegister(word++->index & 0x7fffffffu, "value");
        break;

    case ESI_DELETEN_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputIdentifier(code, *word++);
        break;

    case ESI_DELETEI_IMM:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputImmediate(word++->immediate, UNI_L("index"));
        break;

    case ESI_DELETE:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputRegister(word++->index & 0x7fffffffu, "name");
        break;

    case ESI_DECLARE_GLOBAL:
        OutputIdentifier(code, *word++);
        break;

    case ESI_HASPROPERTY:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputRegister(word++->index & 0x7fffffffu, "name");
        break;

    case ESI_INSTANCEOF:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputRegister(word++->index & 0x7fffffffu, "constructor");
        break;

    case ESI_ENUMERATE:
        OutputRegister(word++->index & 0x7fffffffu, "enum");
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputRegister(word++->index & 0x7fffffffu, "count");
        break;

    case ESI_NEXT_PROPERTY:
        OutputRegister(word++->index & 0x7fffffffu, "name");
        OutputRegister(word++->index & 0x7fffffffu, "enum");
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputRegister(word++->index & 0x7fffffffu, "count");
        break;

    case ESI_EVAL:
        OutputRegister(word++->index, "frame");
        OutputImmediate(static_cast<int>(word->index & 0x7fffffffu), UNI_L("argc"));
        if ((word->immediate & 0x80000000u) != 0)
            output.Append(" [this = global object]");
        ++word;
        OutputImmediate(word++->immediate, UNI_L("innerScopes"));
        OutputImmediate(word++->immediate, UNI_L("innerScopes"));
        break;

    case ESI_CALL:
        OutputRegister(word++->index, "frame");
        OutputImmediate(static_cast<int>(word->index & 0x7fffffffu), UNI_L("argc"));
        if ((word->immediate & 0x80000000u) != 0)
            output.Append(" [this = global object]");
        ++word;
        break;

    case ESI_REDIRECTED_CALL:
        OutputRegister(word++->index & 0x7fffffffu, "function");
        OutputRegister(word++->index & 0x7fffffffu, "apply");
        break;

    case ESI_APPLY:
        OutputRegister(word++->index & 0x7fffffffu, "frame");
        OutputImmediate(word++->immediate, UNI_L("argc"));
        break;

    case ESI_CONSTRUCT:
        OutputRegister(word++->index & 0x7fffffffu, "frame");
        OutputImmediate(word++->immediate, UNI_L("argc"));
        break;

    case ESI_RETURN_VALUE:
        OutputRegister(word++->index & 0x7fffffffu, "value");
        break;

    case ESI_RETURN_NO_VALUE:
        break;

    case ESI_RETURN_FROM_EVAL:
        OutputRegister(word++->index & 0x7fffffffu, "value");
        break;

    case ESI_NEW_OBJECT:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputImmediate(word++->immediate, UNI_L("class"));
        break;

    case ESI_CONSTRUCT_OBJECT:
        {
            OutputRegister(word++->index, "dst");
            unsigned count = code->data->object_literal_classes[word++->index].properties_count;
            OutputImmediate(count, UNI_L("class"));
            for (unsigned index = 0; index < count; ++index)
                OutputRegister(word++->index);
        }
        break;

    case ESI_NEW_ARRAY:
        OutputRegister(word++->index & 0x7fffffffu, "array");
        OutputImmediate(word++->immediate, UNI_L("length"));
        break;

    case ESI_CONSTRUCT_ARRAY:
        OutputRegister(word++->index & 0x7fffffffu, "array");
        OutputImmediate(word++->immediate, UNI_L("length"));
        OutputImmediate(word++->immediate, UNI_L("template"));
        break;

    case ESI_NEW_FUNCTION:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputImmediate(word++->index, UNI_L("function"));
        OutputImmediate(word++->immediate, UNI_L("innerScopes"));
        break;

    case ESI_NEW_REGEXP:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputImmediate(word++->index, UNI_L("regexp"));
        break;

    case ESI_TABLE_SWITCH:
        OutputRegister(word++->index & 0x7fffffffu, "value");
        OutputImmediate(word++->immediate, UNI_L("table"));
        break;

    case ESI_CATCH:
        OutputRegister(word++->index & 0x7fffffffu, "exception");
        break;

    case ESI_CATCH_SCOPE:
        OutputRegister(word++->index & 0x7fffffffu, "object");
        OutputIdentifier(code, *word++);
        break;

    case ESI_THROW:
        OutputRegister(word++->index & 0x7fffffffu, "exception");
        break;

    case ESI_THROW_BUILTIN:
        OutputImmediate(word++->immediate, UNI_L("exception"));
        break;

    case ESI_RETHROW:
        OutputRegister(word++->index & 0x7fffffffu, "exception");
        OutputRegister(word++->index & 0x7fffffffu, "target");
        OutputRegister(word++->index & 0x7fffffffu, "nexttarget");
        break;

    case ESI_EXIT:
        break;

    case ESI_EXIT_TO_BUILTIN:
        break;

    case ESI_EXIT_TO_EVAL:
        break;

#ifdef ECMASCRIPT_DEBUGGER
    case ESI_DEBUGGER_STOP:
        OutputImmediate(word++->immediate, UNI_L("index"));
        break;
#endif

    default:
        output.AppendL("BROKEN PROGRAM!\n");
    }
}

#endif // ES_DISASSEMBLER_SUPPORT
