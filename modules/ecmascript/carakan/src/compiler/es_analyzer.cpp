/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Code analyzer.
 *
 * @author Jens Lindstrom
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_analyzer.h"
#include "modules/ecmascript/carakan/src/compiler/es_instruction_data.h"
#include "modules/ecmascript/carakan/src/compiler/es_native.h"

/* static */ BOOL
ES_Analyzer::NextInstruction(ES_CodeStatic *code, ES_CodeWord *&word)
{
    unsigned noperands = g_instruction_operand_count[word->instruction];

    if (noperands == UINT_MAX)
    {
#ifdef ES_COMBINED_ADD_SUPPORT
        OP_ASSERT(word->instruction == ESI_COPYN || word->instruction == ESI_ADDN || word->instruction == ESI_CONSTRUCT_OBJECT);
#endif // ES_COMBINED_ADD_SUPPORT

        if (word->instruction == ESI_CONSTRUCT_OBJECT)
            noperands = 2 + code->object_literal_classes[word[2].index].properties_count;
        else
            noperands = 2 + word[2].index;
    }

    word += 1 + noperands;

    return word != code->codewords + code->codewords_count;
}

#if defined ES_NATIVE_SUPPORT || defined ES_TEST_ANALYZER

static BOOL
IsNumberType(ES_ValueType type)
{
    return type == ESTYPE_INT32 || type == ESTYPE_DOUBLE || type == ESTYPE_INT32_OR_DOUBLE;
}

static unsigned
OrderValue(const ES_CodeOptimizationData::RegisterAccess &access)
{
    if (access.IsWrite())
        return access.IsImplicit() ? 0 : 2;
    else
        return access.IsExplicit() ? 1 : 3;
}

static BOOL
ShouldPrecede(const ES_CodeOptimizationData::RegisterAccess &access, const ES_CodeOptimizationData::RegisterAccess &other)
{
    if (access.cw_index < other.cw_index)
        return TRUE;
    else if (access.cw_index == other.cw_index)
    {
        OP_ASSERT(OrderValue(access) != OrderValue(other));
        return OrderValue(access) < OrderValue(other);
    }
    else
        return FALSE;
}

ES_Analyzer::ES_Analyzer(ES_Context *context, ES_Code *code, BOOL loop_dispatcher)
    : context(context),
      code(code),
      word(code->data->codewords),
      data(NULL),
      loop_dispatcher(loop_dispatcher),
      loop_io_start(UINT_MAX),
      loop_io_end(UINT_MAX),
      register_accesses(NULL),
      register_access_counts(NULL),
      previous_jump_target(NULL)
{
}

ES_Analyzer::~ES_Analyzer()
{
    if (data)
    {
        OP_DELETE(data);

        if (register_accesses)
            for (unsigned index = 0; index < code->data->register_frame_size; ++index)
                OP_DELETEA(register_accesses[index]);

        OP_DELETEA(register_accesses);
        OP_DELETEA(register_access_counts);
    }
}

static BOOL
ModifiesLexicalScope(ES_FunctionCodeStatic *data, BOOL include_self = FALSE)
{
    if (data->uses_eval)
        return TRUE;

    if (include_self && !data->is_static_function)
    {
        ES_CodeWord *word = data->codewords;

        do
            if (word->instruction == ESI_PUT_LEXICAL || word->instruction == ESI_PUT_SCOPE || word->instruction == ESI_GET_SCOPE_REF)
                return TRUE;
        while (ES_Analyzer::NextInstruction(data, word));
    }

    if (data->functions_count != data->static_functions_count)
        for (unsigned index = 0; index < data->functions_count; ++index)
            if (ModifiesLexicalScope(data->functions[index], TRUE))
                return TRUE;

    return FALSE;
}

ES_CodeOptimizationData *
ES_Analyzer::AnalyzeL()
{
    data = OP_NEW_L(ES_CodeOptimizationData, (code->data));

    data->calls_function = FALSE;
    data->accesses_property = FALSE;
    data->uses_this = code->type != ES_Code::TYPE_FUNCTION || static_cast<ES_FunctionCode *>(code)->GetData()->uses_eval;

    /* This doesn't handle the case that code external to the function modifies
       the values of formal parameters via the arguments array.  It should be
       possible to handle that at the point where the arguments array is
       created, by regenerating and replacing the native dispatcher. */
    if (code->type == ES_Code::TYPE_FUNCTION && code->CanHaveVariableObject())
        data->trampled_variables = ModifiesLexicalScope(static_cast<ES_FunctionCode *>(code)->GetData());
    else
        data->trampled_variables = FALSE;

    register_accesses = OP_NEWA_L(ES_CodeOptimizationData::RegisterAccess *, code->data->register_frame_size);
    op_memset(register_accesses, 0, code->data->register_frame_size * sizeof *register_accesses);

    register_access_counts = OP_NEWA_L(unsigned, code->data->register_frame_size);
    op_memset(register_access_counts, 0, code->data->register_frame_size * sizeof *register_access_counts);

    if (code->data->is_strict_mode)
        ProcessOutput(0, Output(0), TRUE);
    else
        ProcessOutput(0, Output(0, TRUE, ESTYPE_OBJECT), TRUE);

    unsigned index = 1;

    if (code->type == ES_Code::TYPE_FUNCTION)
    {
        ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);

        ProcessOutput(0, Output(index++, TRUE, ESTYPE_OBJECT), TRUE);

        if (fncode->GetData()->arguments_index != ES_FunctionCodeStatic::ARGUMENTS_NOT_USED)
            ProcessOutput(0, Output(fncode->GetData()->arguments_index, TRUE, ESTYPE_OBJECT), TRUE);

        for (unsigned formal_index = 0; formal_index < fncode->GetData()->formals_count; ++formal_index)
            ProcessOutput(0, Output(index++), TRUE);

        ES_Value_Internal undefined;
        unsigned locals_count = fncode->GetData()->LocalsCount();

        for (unsigned local_index = 0; local_index < locals_count; ++local_index)
            if (index == fncode->GetData()->arguments_index)
                ++index;
            else
                ProcessOutput(0, Output(index++, TRUE, ESTYPE_UNDEFINED), TRUE);
    }

    while (index < code->data->first_temporary_register)
        ProcessOutput(0, Output(index++), TRUE);

    if (loop_dispatcher)
    {
        unsigned stop = es_minu(code->data->register_frame_size, loop_io_start);

        while (index < stop)
            ProcessOutput(0, Output(index++), TRUE);

        for (unsigned loop_io_index = loop_io_start; loop_io_index != loop_io_end; ++loop_io_index)
            ProcessOutput(0, Output(loop_io_index), TRUE);
    }

    BOOL has_inferred_type = FALSE;

    for (ES_CodeWord *stop = code->data->codewords + code->data->codewords_count; word != stop;)
    {
        unsigned cw_index = word - code->data->codewords;

        switch (word->instruction)
        {
        case ESI_LOAD_STRING:
            {
                ProcessOutput(cw_index, Output(word[1].index, TRUE, ESTYPE_STRING, TRUE, cw_index));
                word += 3;
            }
            break;

        case ESI_LOAD_DOUBLE:
            {
                ProcessOutput(cw_index, Output(word[1].index, TRUE, ESTYPE_DOUBLE, TRUE, cw_index));
                word += 3;
            }
            break;

        case ESI_LOAD_INT32:
            {
                ProcessOutput(cw_index, Output(word[1].index, TRUE, ESTYPE_INT32, TRUE, cw_index));
                word += 3;
            }
            break;

        case ESI_LOAD_TRUE:
            {
                ProcessOutput(cw_index, Output(word[1].index, TRUE, ESTYPE_BOOLEAN, TRUE, cw_index));
                word += 2;
            }
            break;

        case ESI_LOAD_FALSE:
            {
                ProcessOutput(cw_index, Output(word[1].index, TRUE, ESTYPE_BOOLEAN, TRUE, cw_index));
                word += 2;
            }
            break;

        case ESI_COPY:
            {
                Input input(word[2].index);
                ProcessInput(cw_index, input);

                Output output(OutputFromInput(word[1].index, cw_index, input));

                ProcessOutput(cw_index, output);

                if (output.has_type)
                    has_inferred_type = TRUE;

                word += 3;
            }
            break;

        case ESI_COPYN:
            {
                ES_CodeWord::Index dst = word[1].index;

                for (unsigned index = 0; index < word[2].index; ++index)
                {
                    Input input(word[3 + index].index);
                    ProcessInput(cw_index, input);

                    Output output(OutputFromInput(dst++, cw_index, input));

                    ProcessOutput(cw_index, output);

                    if (output.has_type)
                        has_inferred_type = TRUE;
                }

                word += 3 + word[2].index;
            }
            break;

        case ESI_RSHIFT_UNSIGNED:
            {
                /* In theory, unsigned right shift produces a uint32, in other
                   words, a double.  But in practice, the result will always be
                   a smaller integer than the left hand side operand's numeric
                   value, unless the right hand side (the shift count) is zero.
                   If the left hand side operand is an int32, then the result
                   will inevitably also be an int32. */

                ES_CodeWord::Index dst = word[1].index;

                ProcessInput(cw_index, Input(word[2].index, TRUE, TRUE, ESTYPE_INT32_OR_DOUBLE));
                ProcessInput(cw_index, Input(word[3].index, TRUE, TRUE, ESTYPE_INT32));

                Output output(dst);

                unsigned inferred_type;

                ProcessRShiftUnsigned(cw_index, word, inferred_type);

                has_inferred_type = has_inferred_type || inferred_type != 0;

                output.has_type = TRUE;

                if (inferred_type == ESTYPE_BITS_INT32)
                    output.type = ESTYPE_INT32;
                else
                    output.type = ESTYPE_INT32_OR_DOUBLE;

                ProcessOutput(cw_index, output);

                word += 4;
            }
            break;

        case ESI_ADD:
            {
                ES_CodeWord::Index dst = word[1].index;

                ProcessInput(cw_index, Input(word[2].index));
                ProcessInput(cw_index, Input(word[3].index));

                Output output(dst);

                ES_ValueType type1, type2;
                BOOL known1 = KnownType(type1, word[2].index, cw_index), known2 = KnownType(type2, word[3].index, cw_index);
                if (known1 && known2 && IsNumberType(type1) && IsNumberType(type2))
                {
                    output.has_type = TRUE;
                    output.type = ESTYPE_INT32_OR_DOUBLE;
                    has_inferred_type = TRUE;
                }
                else if (known1 && type1 == ESTYPE_STRING || known2 && type2 == ESTYPE_STRING)
                {
                    output.has_type = TRUE;
                    output.type = ESTYPE_STRING;
                    has_inferred_type = TRUE;
                }

                ProcessOutput(cw_index, output);

                word += 4;
            }
            break;

#ifdef ES_COMBINED_ADD_SUPPORT
        case ESI_ADDN:
            {
                Input input(0, TRUE, TRUE, ESTYPE_STRING);

                for (unsigned index = 0; index < word[2].index; ++index)
                {
                    input.index = word[3 + index].index;
                    ProcessInput(cw_index, input);
                }

                ProcessOutput(cw_index, Output(word[1].index, TRUE, ESTYPE_STRING));

                word += 3 + word[2].index;
            }
            break;
#endif // ES_COMBINED_ADD_SUPPORT

        case ESI_CONSTRUCT_OBJECT:
            {
                Input input;
                input.has_type = FALSE;

                unsigned count = code->data->object_literal_classes[word[2].index].properties_count;

                for (unsigned property_index = 0; property_index < count; ++property_index)
                {
                    input.index = word[3 + property_index].index;
                    ProcessInput(cw_index, input);
                }

                ProcessOutput(cw_index, Output(word[1].index, TRUE, ESTYPE_OBJECT));

                word += 3 + count;
            }
            break;

        case ESI_SUB:
            {
                ES_CodeWord::Index dst = word[1].index;

                ProcessInput(cw_index, Input(word[2].index, TRUE, TRUE, ESTYPE_INT32_OR_DOUBLE));
                ProcessInput(cw_index, Input(word[3].index, TRUE, TRUE, ESTYPE_INT32_OR_DOUBLE));
                ProcessOutput(cw_index, Output(dst, TRUE, ESTYPE_INT32_OR_DOUBLE));

                word += 4;
            }
            break;

        case ESI_MUL:
        case ESI_REM:
            {
                ES_CodeWord::Index dst = word[1].index;

                ProcessInput(cw_index, Input(word[2].index, TRUE, TRUE, ESTYPE_INT32_OR_DOUBLE));
                ProcessInput(cw_index, Input(word[3].index, TRUE, TRUE, ESTYPE_INT32_OR_DOUBLE));
                ProcessOutput(cw_index, Output(dst, TRUE, ESTYPE_INT32_OR_DOUBLE));

                word += 4;
            }
            break;

        case ESI_INC:
        case ESI_DEC:
            {
                ES_ValueType type, output_type = ESTYPE_INT32_OR_DOUBLE;

                if (!IsTemporary(word[1].index))
                    if (KnownType(type, word[1].index, cw_index) && type == ESTYPE_INT32)
                        for (ES_CodeStatic::VariableRangeLimitSpan *vrls = code->data->first_variable_range_limit_span; vrls; vrls = vrls->next)
                            if (vrls->index == word[1].index && vrls->start <= cw_index && cw_index < vrls->end)
                            {
                                output_type = ESTYPE_INT32;
                                break;
                            }

                ProcessInput(cw_index, Input(word[1].index, TRUE, TRUE, output_type));
                ProcessOutput(cw_index, Output(word[1].index, TRUE, output_type));

                word += 2;
            }
            break;

        case ESI_EQ:
        case ESI_EQ_STRICT:
        case ESI_NEQ:
        case ESI_NEQ_STRICT:
        case ESI_LT:
        case ESI_LTE:
        case ESI_GT:
        case ESI_GTE:
            {
                ES_ValueType lhs_type, rhs_type;
                BOOL lhs_known = KnownType(lhs_type, word[1].index, cw_index), rhs_known = KnownType(rhs_type, word[2].index, cw_index);
                BOOL guess_numerical = !(lhs_known && lhs_type == ESTYPE_STRING) && !(rhs_known && rhs_type == ESTYPE_STRING);

                if (guess_numerical)
                {
                    ProcessInput(cw_index, Input(word[1].index, TRUE, FALSE, ESTYPE_INT32_OR_DOUBLE));
                    ProcessInput(cw_index, Input(word[2].index, TRUE, FALSE, ESTYPE_INT32_OR_DOUBLE));
                }
                else
                {
                    ProcessInput(cw_index, Input(word[1].index));
                    ProcessInput(cw_index, Input(word[2].index));
                }

                word += 3;
            }
            break;

        case ESI_EVAL:
        case ESI_CALL:
        case ESI_APPLY:
        case ESI_CONSTRUCT:
            {
                data->calls_function = TRUE;

                ES_CodeWord::Index frame = word[1].index;
                ES_CodeWord::Index argc = word[2].index;

                if (word->instruction == ESI_CALL && !(argc & 0x80000000u))
                    ProcessInput(cw_index, Input(frame, TRUE, TRUE, ESTYPE_OBJECT));

                argc &= ~0x80000000u;

                ProcessInput(cw_index, Input(frame + 1, TRUE, TRUE, ESTYPE_OBJECT));

                /* ESI_APPLY: (thisArg + expanded argument array); latter of length 'argc'. */
                for (unsigned argi = 0; argi < (word->instruction == ESI_APPLY ? argc + 1 : argc); ++argi)
                    ProcessInput(cw_index, Input(frame + 2 + argi));

                if (word->instruction == ESI_CONSTRUCT)
                    ProcessOutput(cw_index, Output(frame, TRUE, ESTYPE_OBJECT));
                else
                    ProcessOutput(cw_index, Output(word->instruction == ESI_APPLY ? frame + 1 : frame));

                if (word->instruction == ESI_EVAL)
                    word += 5;
                else
                    word += 3;
            }
            break;

        case ESI_REDIRECTED_CALL:
            {
                ProcessInput(cw_index, Input(word[1].index));
                ProcessInput(cw_index, Input(word[2].index));
                ProcessOutput(cw_index, Output(0));
                word += 3;
            }
            break;

        case ESI_JUMP:
        case ESI_JUMP_IF_TRUE:
        case ESI_JUMP_IF_FALSE:
            {
                unsigned target = (&word[1] - code->data->codewords) + word[1].offset;

                ES_CodeOptimizationData::JumpTarget **ptr, *previous = NULL;

                if (previous_jump_target && previous_jump_target->next && previous_jump_target->next->index <= target)
                {
                    ptr = &previous_jump_target->next;
                    previous = previous_jump_target;
                }
                else
                {
                    ptr = &data->first_jump_target;
                    previous = NULL;
                }

                while (*ptr && (*ptr)->index < target)
                {
                    previous = *ptr;
                    ptr = &(*ptr)->next;
                }

                previous_jump_target = previous;

                if (!*ptr || (*ptr)->index > target)
                {
                    ES_CodeOptimizationData::JumpTarget *jt = OP_NEW_L(ES_CodeOptimizationData::JumpTarget, ());

                    jt->index = target;
                    jt->number_of_forward_jumps = 0;
                    jt->number_of_backward_jumps = 0;

                    jt->previous = previous;
                    jt->next = *ptr;

                    *ptr = jt;

                    if (jt->next)
                        jt->next->previous = jt;
                    else
                        data->last_jump_target = jt;

                    ++data->jump_targets_count;
                }

                if (word[1].offset > 0)
                    ++(*ptr)->number_of_forward_jumps;
                else
                    ++(*ptr)->number_of_backward_jumps;

                if (word->instruction == ESI_JUMP)
                    word += 2;
                else
                    word += 3;
            }
            break;

        case ESI_TABLE_SWITCH:
            {
                ES_CodeStatic::SwitchTable &table = code->data->switch_tables[word[2].index];
                ES_CodeOptimizationData::JumpTarget **ptr = &data->first_jump_target, *previous = NULL;

                ProcessInput(cw_index, Input(word[1].index, TRUE, FALSE, ESTYPE_INT32));

                unsigned ncases = static_cast<unsigned>(table.maximum - table.minimum) + 1;
                for (unsigned i = 0; i < ncases + 1; ++i)
                {
                    unsigned target = i < ncases ?  table.codeword_indeces[table.minimum + static_cast<int>(i)] : table.default_codeword_index;

                    if (*ptr && (*ptr)->index > target)
                        ptr = &data->first_jump_target, previous = NULL;

                    while (*ptr && (*ptr)->index < target)
                    {
                        previous = *ptr;
                        ptr = &(*ptr)->next;
                    }

                    if (!*ptr || (*ptr)->index > target)
                    {
                        ES_CodeOptimizationData::JumpTarget *jt = OP_NEW_L(ES_CodeOptimizationData::JumpTarget, ());

                        jt->index = target;
                        jt->number_of_forward_jumps = 0;
                        jt->number_of_backward_jumps = 0;

                        jt->previous = previous;
                        jt->next = *ptr;

                        *ptr = jt;

                        if (jt->next)
                            jt->next->previous = jt;
                        else
                            data->last_jump_target = jt;

                        ++data->jump_targets_count;
                    }

                    if (target > cw_index)
                        ++(*ptr)->number_of_forward_jumps;
                    else
                        ++(*ptr)->number_of_backward_jumps;
                }

                word += 3;
            }
            break;

        case ESI_NEXT_PROPERTY:
            {
                ProcessOutput(cw_index, Output(word[1].index, TRUE));
                ProcessInput(cw_index, Input(word[2].index));
                ProcessInput(cw_index, Input(word[3].index, TRUE, TRUE, ESTYPE_OBJECT));
                ProcessInput(cw_index, Input(word[4].index));
                ProcessOutput(cw_index, Output(word[4].index));
                ProcessOutput(cw_index + 5, Output(word[1].index, TRUE, ESTYPE_STRING));

                word += 5;

                OP_ASSERT(word->instruction == ESI_JUMP_IF_FALSE);
            }
            break;

        case ESI_GET_SCOPE_REF:
            /* If a local is aliased by a temporary, we have no control of the effect this may have on the local;
               i.e., flag all as trampled. */
            if (word[5].index != UINT_MAX)
                data->trampled_variables = TRUE;
            /* fall through */
        case ESI_GET:
        case ESI_GETN_IMM:
        case ESI_GET_LENGTH:
        case ESI_GETI_IMM:
        case ESI_PUT:
        case ESI_PUTN_IMM:
        case ESI_INIT_PROPERTY:
        case ESI_PUT_LENGTH:
        case ESI_PUTI_IMM:
        case ESI_GET_GLOBAL:
        case ESI_PUT_GLOBAL:
        case ESI_GET_SCOPE:
        case ESI_PUT_SCOPE:
            data->accesses_property = TRUE;

        default:
            Input input[3], *iptr = input;
            Output output[3], *optr = output;

            ProcessInstruction(iptr, optr);

            for (Input *i = input; i != iptr; ++i)
                ProcessInput(cw_index, *i);
            for (Output *o = output; o != optr; ++o)
                ProcessOutput(cw_index, *o);
        }
    }

    while (ProcessJumps() && has_inferred_type && ReprocessInferredTypes())
        ;

    ES_CodeOptimizationData::RegisterAccess terminator(code->data->codewords_count, ES_CodeOptimizationData::RegisterAccess::FLAG_WRITE | ES_CodeOptimizationData::RegisterAccess::FLAG_IS_TERMINATOR);

    for (unsigned register_index = 0; register_index < code->data->register_frame_size; ++register_index)
    {
        AppendAccess(register_index, terminator, TRUE);

#ifdef DEBUG_ENABLE_OPASSERT
        ES_CodeOptimizationData::RegisterAccess *iter = register_accesses[register_index], *current_write = NULL;

        while (!iter->IsTerminator())
        {
            OP_ASSERT(ShouldPrecede(iter[0], iter[1]));

            if (iter->IsRead())
                OP_ASSERT(!current_write || iter->GetWriteIndex() == current_write->cw_index);
            else
                current_write = iter;

            ++iter;
        }
#endif // DEBUG_ENABLE_OPASSERT
    }

    data->register_accesses = register_accesses;
    register_accesses = NULL;

    data->register_access_counts = register_access_counts;
    register_access_counts = NULL;

    ES_CodeOptimizationData *ret = data;
    data = NULL;
    return ret;
}

static void
MakeRoomForOneMore(ES_CodeOptimizationData::RegisterAccess *&accesses, unsigned &count, unsigned insert_index, BOOL final)
{
    enum { INITIAL_SIZE = 8 };

    unsigned size = INITIAL_SIZE;

    if (final)
        while (size < count)
            size += size;

    if (count == 0 || count >= INITIAL_SIZE && (count & (count - 1)) == 0 || final && size - count > INITIAL_SIZE)
    {
        unsigned new_count = final ? count + 1 : count == 0 ? static_cast<unsigned>(INITIAL_SIZE) : count + count;
        ES_CodeOptimizationData::RegisterAccess *new_accesses = OP_NEWA_L(ES_CodeOptimizationData::RegisterAccess, new_count);

        if (count != 0)
        {
            if (insert_index != 0)
                op_memcpy(new_accesses, accesses, insert_index * sizeof(ES_CodeOptimizationData::RegisterAccess));
            if (insert_index != count)
                op_memcpy(new_accesses + insert_index + 1, accesses + insert_index, (count - insert_index) * sizeof(ES_CodeOptimizationData::RegisterAccess));
        }

        OP_DELETEA(accesses);

        accesses = new_accesses;
    }
    else if (insert_index)
        op_memmove(accesses + insert_index + 1, accesses + insert_index, (count - insert_index) * sizeof(ES_CodeOptimizationData::RegisterAccess));
}

/** Returns index of last access at 'cw_index', or first access after 'cw_index'
    if none occur at 'cw_index'. */
/* static */ unsigned
ES_Analyzer::FindRegisterAccessIndex(unsigned cw_index, ES_CodeOptimizationData::RegisterAccess *accesses, unsigned count)
{
    OP_ASSERT(count != 0);

    int low = 0, high = count - 1;

    if (accesses[high].cw_index < cw_index)
        return count;

    while (TRUE)
    {
        unsigned index = (low + high) / 2;

        if (cw_index <= accesses[index].cw_index)
        {
            if (index == 0 || cw_index > accesses[index - 1].cw_index)
            {
                while (index + 1 < count && cw_index == accesses[index + 1].cw_index)
                    ++index;

                return index;
            }

            high = index - 1;
        }
        else
            low = index + 1;
    }
}

void
ES_Analyzer::AppendAccess(unsigned register_index, const ES_CodeOptimizationData::RegisterAccess &access, BOOL final)
{
    ES_CodeOptimizationData::RegisterAccess *&accesses = register_accesses[register_index];
    unsigned &count = register_access_counts[register_index];

    MakeRoomForOneMore(accesses, count, count, final);

    unsigned index = count++;

    accesses[index] = access;

    if (access.IsRead() && index != 0)
        /* When appending a read, make its write cw_index point to the closest
           preceding write, either by copying the write cw_index from an
           immediately preceding read, or using the cw_index of the immediately
           preceding write. */
        if (accesses[index - 1].IsRead())
            accesses[index].data |= accesses[index - 1].data & ES_CodeOptimizationData::RegisterAccess::MASK_WRITE_CW_INDEX;
        else
            accesses[index].data |= accesses[index - 1].cw_index << ES_CodeOptimizationData::RegisterAccess::SHIFT_WRITE_CW_INDEX;
}

void
ES_Analyzer::InsertAccess(unsigned register_index, const ES_CodeOptimizationData::RegisterAccess &access)
{
    ES_CodeOptimizationData::RegisterAccess *&accesses = register_accesses[register_index];
    unsigned &count = register_access_counts[register_index];

    /* We should have no reason to insert additional accesses before we've
       appended any initial accesses.  (Initial analysis appends, then jump
       processing inserts.) */
    OP_ASSERT(count != 0);

    unsigned index = FindRegisterAccessIndex(access.cw_index, accesses, count);

    if (index != count && ShouldPrecede(accesses[index], access))
        ++index;
    else
        while (index != 0 && ShouldPrecede(access, accesses[index - 1]))
            --index;

    MakeRoomForOneMore(accesses, count, index, FALSE);

    accesses[index] = access;
    ++count;

    if (access.IsRead())
        /* When inserting a read, make its write cw_index point to the closest
           preceding write, either by copying the write cw_index from an
           immediately preceding read, or using the cw_index of the immediately
           preceding write. */
        if (accesses[index - 1].IsRead())
            accesses[index].data |= accesses[index - 1].data & ES_CodeOptimizationData::RegisterAccess::MASK_WRITE_CW_INDEX;
        else
            accesses[index].data |= accesses[index - 1].cw_index << ES_CodeOptimizationData::RegisterAccess::SHIFT_WRITE_CW_INDEX;
    else
    {
        /* When inserting a write, we need to adjust the write index of any
           following reads to correctly point to this write. */
        ES_CodeOptimizationData::RegisterAccess *iter = &accesses[index + 1], *stop = &accesses[count];

        while (iter != stop)
            if (iter->IsRead())
            {
                iter->data = ((iter->data & ~ES_CodeOptimizationData::RegisterAccess::MASK_WRITE_CW_INDEX) |
                              access.cw_index << ES_CodeOptimizationData::RegisterAccess::SHIFT_WRITE_CW_INDEX);
                ++iter;
            }
            else
                break;
    }
}

void
ES_Analyzer::PropagateWriteInformation(unsigned register_index, ES_CodeOptimizationData::RegisterAccess *from)
{
    ES_CodeOptimizationData::RegisterAccess *accesses = register_accesses[register_index];
    unsigned count = register_access_counts[register_index];

    ES_CodeOptimizationData::RegisterAccess *iter = from + 1, *stop = accesses + count;
    ES_Value_Internal value;
    BOOL has_value = from->GetValue(code, value);

    while (iter != stop)
    {
        if (iter->IsWrite())
            if (iter->IsExplicit())
                break;
            else
            {
                iter->data |= from->GetType();

                if (has_value)
                {
                    ES_Value_Internal iter_value;
                    if (iter->GetValue(code, iter_value) && !ES_Value_Internal::IsSameValue(context, value, iter_value))
                        iter->data &= ~ES_CodeOptimizationData::RegisterAccess::FLAG_VALUE_IS_KNOWN;
                }
                else
                    iter->data &= ~ES_CodeOptimizationData::RegisterAccess::FLAG_VALUE_IS_KNOWN;
            }

        ++iter;
    }
}

ES_CodeOptimizationData::RegisterAccess *
ES_Analyzer::ExplicitWriteAt(unsigned register_index, unsigned cw_index)
{
    ES_CodeOptimizationData::RegisterAccess *accesses = register_accesses[register_index];
    unsigned count = register_access_counts[register_index];

    if (count == 0)
        return NULL;

    unsigned index = FindRegisterAccessIndex(cw_index, accesses, count);

    if (index < count && accesses[index].cw_index == cw_index)
    {
        if (accesses[index].IsImplicitRead())
            if (index == 0 || accesses[--index].cw_index != cw_index)
                return NULL;

        if (accesses[index].cw_index == cw_index && accesses[index].IsExplicitWrite())
            return &accesses[index];
    }

    return NULL;
}

ES_CodeOptimizationData::RegisterAccess *
ES_Analyzer::ImplicitWriteAt(unsigned register_index, unsigned cw_index)
{
    ES_CodeOptimizationData::RegisterAccess *accesses = register_accesses[register_index];
    unsigned count = register_access_counts[register_index];

    if (count == 0)
        return NULL;

    unsigned index = FindRegisterAccessIndex(cw_index, accesses, count);

    if (index < count && accesses[index].cw_index == cw_index)
    {
        while (!accesses[index].IsImplicitWrite())
            if (index == 0 || accesses[--index].cw_index != cw_index)
                return NULL;

        if (accesses[index].cw_index == cw_index && accesses[index].IsImplicitWrite())
            return &accesses[index];
    }

    return NULL;
}

ES_CodeOptimizationData::RegisterAccess *
ES_Analyzer::ExplicitReadAt(unsigned register_index, unsigned cw_index)
{
    ES_CodeOptimizationData::RegisterAccess *accesses = register_accesses[register_index];
    unsigned count = register_access_counts[register_index];

    if (count == 0)
        return NULL;

    unsigned index = FindRegisterAccessIndex(cw_index, accesses, count);

    if (index < count && accesses[index].cw_index == cw_index)
    {
        while (accesses[index].IsImplicitRead() || accesses[index].IsExplicitWrite())
            if (index == 0 || accesses[--index].cw_index != cw_index)
                return NULL;

        if (accesses[index].cw_index == cw_index && accesses[index].IsExplicitRead())
            return &accesses[index];
    }

    return NULL;
}

ES_CodeOptimizationData::RegisterAccess *
ES_Analyzer::ImplicitReadAt(unsigned register_index, unsigned cw_index)
{
    ES_CodeOptimizationData::RegisterAccess *accesses = register_accesses[register_index];
    unsigned count = register_access_counts[register_index];

    if (count == 0)
        return NULL;

    unsigned index = FindRegisterAccessIndex(cw_index, accesses, count);

    if (index < count && accesses[index].cw_index == cw_index && accesses[index].IsImplicitRead())
        return &accesses[index];
    else
        return NULL;
}

ES_CodeOptimizationData::RegisterAccess *
ES_Analyzer::WriteReadAt(unsigned register_index, unsigned cw_index)
{
    ES_CodeOptimizationData::RegisterAccess *accesses = register_accesses[register_index];
    unsigned count = register_access_counts[register_index];

    if (count == 0)
        return NULL;

    unsigned index = FindRegisterAccessIndex(cw_index, accesses, count);

    if (index == count)
        --index;

    /* We don't care about things happening after 'cw_index'. */
    while (index != 0 && cw_index < accesses[index].cw_index)
        --index;

    /* We also don't care about things happening at 'cw_index' unless it happens
       to be an implicit write. */
    while (index != 0 && cw_index == accesses[index].cw_index && !accesses[index].IsImplicitWrite())
        --index;

    /* And we don't care about reads at all. */
    if (accesses[index].IsRead() && index != 0)
        if (accesses[--index].IsRead() && index != 0)
            if (accesses[--index].IsRead())
            {
                /* We seem to be looking at a possibly long list of reads.  Find
                   the write with a binary search using the write index recorded
                   in each read access. */

                unsigned write_cw_index = accesses[index].GetWriteIndex();

                index = FindRegisterAccessIndex(write_cw_index, accesses, count);

                OP_ASSERT(accesses[index].cw_index == write_cw_index || write_cw_index == 0);

                while (index != 0 && accesses[index].IsRead())
                    --index;

                OP_ASSERT(accesses[index].cw_index == write_cw_index || write_cw_index == 0);
            }

    /* Now, if we've found a write occur before 'cw_index' or an implicit write
       occuring at 'cw_index', return it, otherwise return NULL. */
    if (accesses[index].IsWrite() && (accesses[index].cw_index < cw_index || accesses[index].IsImplicit()))
        return &accesses[index];
    else
        return NULL;
}

BOOL
ES_Analyzer::IsTrampled(unsigned register_index)
{
    if (code->type == ES_Code::TYPE_FUNCTION && register_index >= 2)
        if (register_index < 2 + static_cast<ES_FunctionCodeStatic *>(code->data)->formals_count)
            /* A formal parameter can always be trampled via the arguments
               array, even if the function doesn't appear to be using it. */
            return TRUE;
        else if (data->trampled_variables && register_index < code->data->first_temporary_register)
            /* A local variable can be trampled if there are nested functions
               that access their lexical scope. */
            return TRUE;

    return FALSE;
}

BOOL
ES_Analyzer::KnownType(ES_ValueType &type, unsigned register_index, unsigned cw_index)
{
    if (IsTrampled(register_index))
        return FALSE;

    ES_CodeOptimizationData::RegisterAccess *access = WriteReadAt(register_index, cw_index);

    if (access->IsSingleType())
    {
        type = access->GetSingleType();
        return TRUE;
    }
    else if (access->IsNumberType())
    {
        type = ESTYPE_INT32_OR_DOUBLE;
        return TRUE;
    }
    else
        return FALSE;
}

BOOL
ES_Analyzer::KnownValue(ES_Value_Internal &value, unsigned &value_cw_index, unsigned register_index, unsigned cw_index)
{
    if (IsTrampled(register_index))
        return FALSE;

    ES_CodeOptimizationData::RegisterAccess *access = WriteReadAt(register_index, cw_index);

    if (access->IsValueKnown())
    {
        value_cw_index = access->GetWriteIndex();
        return access->GetValue(code, value);
    }
    else
        return FALSE;
}

ES_Analyzer::Output
ES_Analyzer::OutputFromInput(unsigned register_index, unsigned cw_index, const Input &input)
{
    if (!IsTrampled(input.index))
    {
        ES_Value_Internal value;
        unsigned value_cw_index;
        if (KnownValue(value, value_cw_index, input.index, cw_index))
            return Output(register_index, TRUE, value.Type(), TRUE, value_cw_index);

        ES_ValueType type;
        if (KnownType(type, input.index, cw_index))
            return Output(register_index, TRUE, type);
    }

    return Output(register_index);
}

void
ES_Analyzer::ProcessInput(unsigned cw_index, const Input &input)
{
    if (input.index != UINT_MAX)
    {
        if (input.index == 0)
            data->uses_this = TRUE;

        ES_CodeOptimizationData::RegisterAccess *access = ExplicitReadAt(input.index, cw_index);

        if (!access)
        {
            unsigned data = (ES_CodeOptimizationData::RegisterAccess::FLAG_READ |
                             ES_CodeOptimizationData::RegisterAccess::FLAG_EXPLICIT);

            if (input.has_type)
            {
                data |= ES_Value_Internal::TypeBits(input.type);
                if (input.has_forced_type)
                    data |= ES_CodeOptimizationData::RegisterAccess::FLAG_TYPE_IS_FORCED;
            }
            else
                data |= ES_CodeOptimizationData::RegisterAccess::MASK_TYPE;

            AppendAccess(input.index, ES_CodeOptimizationData::RegisterAccess(cw_index, data));
        }
        else
        {
            if (access->IsReadTypeForced())
                if (!input.has_type || access->GetType() != ES_Value_Internal::TypeBits(input.type) || !input.has_forced_type)
                    access->data ^= ES_CodeOptimizationData::RegisterAccess::FLAG_TYPE_IS_FORCED;

            if (input.has_type)
                access->data |= ES_Value_Internal::TypeBits(input.type);
            else
                access->data |= ES_CodeOptimizationData::RegisterAccess::MASK_TYPE;
        }
    }
}

void
ES_Analyzer::ProcessOutput(unsigned cw_index, const Output &output, BOOL is_implicit)
{
    if (output.index != INT_MAX)
    {
        if (output.optional)
            ProcessInput(cw_index, Input(output.index));

        ES_CodeOptimizationData::RegisterAccess *existing = ExplicitWriteAt(output.index, cw_index);

        unsigned data = (ES_CodeOptimizationData::RegisterAccess::FLAG_WRITE |
                         (is_implicit ? ES_CodeOptimizationData::RegisterAccess::FLAG_IMPLICIT
                                      : ES_CodeOptimizationData::RegisterAccess::FLAG_EXPLICIT));

        if (output.has_type)
        {
            data |= ES_Value_Internal::TypeBits(output.type);
            if (output.has_value)
                data |= (ES_CodeOptimizationData::RegisterAccess::FLAG_VALUE_IS_KNOWN |
                         (output.value_cw_index << ES_CodeOptimizationData::RegisterAccess::SHIFT_WRITE_CW_INDEX));
        }
        else
            data |= ES_CodeOptimizationData::RegisterAccess::MASK_TYPE;

        if (existing)
            existing->data = data;
        else
            AppendAccess(output.index, ES_CodeOptimizationData::RegisterAccess(cw_index, data));
    }
}

BOOL
ES_Analyzer::ProcessJumps()
{
    BOOL something_changed = FALSE, something_changed_now;

    do
    {
        ES_CodeWord *word = code->data->codewords;

        something_changed_now = FALSE;

        do
        {
            unsigned from = word - code->data->codewords, to;
            switch (word->instruction)
            {
            case ESI_JUMP:
            case ESI_JUMP_IF_TRUE:
            case ESI_JUMP_IF_FALSE:
                to = from + 1 + word[1].offset;
                if (ProcessJump(from, to))
                    something_changed = something_changed_now = TRUE;
                break;

            case ESI_TABLE_SWITCH:
                ES_CodeStatic::SwitchTable &table = code->data->switch_tables[word[2].index];

                for (int value = table.minimum; value <= table.maximum + 1; ++value)
                {
                    unsigned target = value > table.maximum ? table.default_codeword_index : table.codeword_indeces[value];
                    ProcessJump(from, target);
                }
            }
        }
        while (NextInstruction(code->data, word));
    }
    while (something_changed_now);

    return something_changed;
}

BOOL
ES_Analyzer::ProcessJump(unsigned from_cw_index, unsigned to_cw_index)
{
    BOOL something_changed = FALSE;

    for (unsigned register_index = 0; register_index < code->data->register_frame_size; ++register_index)
    {
        ES_CodeOptimizationData::RegisterAccess *&accesses = register_accesses[register_index];
        unsigned &count = register_access_counts[register_index];

        if (count == 0)
            continue;

        ES_CodeOptimizationData::RegisterAccess *from_write = WriteReadAt(register_index, from_cw_index);
        ES_CodeOptimizationData::RegisterAccess *to_write = WriteReadAt(register_index, to_cw_index);

        if (!to_write || !from_write)
        {
            OP_ASSERT(register_index >= code->data->first_temporary_register);
            continue;
        }

        if (from_write != to_write)
        {
            if (register_index >= code->data->first_temporary_register)
            {
                /* Strategy: If there are no recorded reads between where we
                   would insert the implicit write for this jump and the first
                   explicit write to the same register, then this is a "false"
                   dependency.  This is very common for temporary registers.

                   We don't do this check for non-temporaries.  In theory, the
                   same logic applies, but it's less common, and such things as
                   the variable objects and arguments arrays complicate the
                   situation by introducing the possibility of "hidden" reads or
                   writes. So to keep things simple, we just disregard this
                   possibility. */

                unsigned index = FindRegisterAccessIndex(to_cw_index, accesses, count);

                while (index != 0 && accesses[index - 1].cw_index == to_cw_index)
                    --index;

                while (index < count)
                    if (accesses[index].IsRead())
                        goto process_jump;
                    else if (accesses[index].IsExplicitWrite())
                        break;
                    else
                        ++index;

                continue;
            }

        process_jump:
            ES_CodeOptimizationData::RegisterAccess *implicit_read = ImplicitReadAt(register_index, from_cw_index);

            unsigned read_data = (ES_CodeOptimizationData::RegisterAccess::FLAG_READ |
                                  ES_CodeOptimizationData::RegisterAccess::FLAG_IMPLICIT |
                                  ES_CodeOptimizationData::RegisterAccess::MASK_TYPE);

            unsigned write_data = (ES_CodeOptimizationData::RegisterAccess::FLAG_WRITE |
                                   ES_CodeOptimizationData::RegisterAccess::FLAG_IMPLICIT);
            unsigned write_type;

            if (!IsTrampled(register_index) && from_write->IsTypeKnown() && to_write->IsTypeKnown())
            {
                write_type = from_write->GetType() | to_write->GetType();

                ES_Value_Internal from_value, to_value;
                if (from_write->GetValue(code, from_value) && to_write->GetValue(code, to_value) && ES_Value_Internal::IsSameValue(context, from_value, to_value))
                    write_data |= ((to_write->GetWriteIndex() << ES_CodeOptimizationData::RegisterAccess::SHIFT_WRITE_CW_INDEX) |
                                   ES_CodeOptimizationData::RegisterAccess::FLAG_VALUE_IS_KNOWN);
            }
            else
                write_type = ES_CodeOptimizationData::RegisterAccess::MASK_TYPE;

            write_data |= write_type;

            if (!implicit_read)
            {
                something_changed = TRUE;
                InsertAccess(register_index, ES_CodeOptimizationData::RegisterAccess(from_cw_index, read_data));
            }
            else
            {
                read_data |= implicit_read->data & ES_CodeOptimizationData::RegisterAccess::MASK_WRITE_CW_INDEX;

                if (implicit_read->data != read_data)
                {
                    something_changed = TRUE;
                    implicit_read->data = read_data;
                }
            }

            ES_CodeOptimizationData::RegisterAccess *implicit_write = ImplicitWriteAt(register_index, to_cw_index);

            if (!implicit_write)
            {
                something_changed = TRUE;
                InsertAccess(register_index, ES_CodeOptimizationData::RegisterAccess(to_cw_index, write_data));

                PropagateWriteInformation(register_index, ImplicitWriteAt(register_index, to_cw_index));
            }
            else if (implicit_write->data != write_data)
            {
                OP_ASSERT(implicit_write->IsWrite());

                something_changed = TRUE;
                implicit_write->data = write_data;

                PropagateWriteInformation(register_index, implicit_write);
            }
        }
    }

    return something_changed;
}

BOOL
ES_Analyzer::ReprocessInferredTypes()
{
    ES_CodeWord *word = code->data->codewords;
    BOOL something_changed = FALSE;

    do
    {
        unsigned cw_index = word - code->data->codewords;

        switch (word->instruction)
        {
        case ESI_COPY:
            if (ReprocessCopy(cw_index, word[2].index, word[1].index))
                something_changed = TRUE;
            break;

        case ESI_COPYN:
            {
                ES_CodeWord::Index dst = word[1].index;

                for (unsigned index = 0; index < word[2].index; ++index)
                    if (ReprocessCopy(cw_index, word[3 + index].index, dst++))
                        something_changed = TRUE;
            }
            break;

        case ESI_RSHIFT_UNSIGNED:
            {
                unsigned inferred_type;

                ProcessRShiftUnsigned(cw_index, word, inferred_type);

                if (ReprocessInferredType(cw_index, inferred_type, FALSE, 0, word[1].index))
                    something_changed = TRUE;
            }
            break;

        case ESI_ADD:
            {
                ES_ValueType type1, type2;
                unsigned type_target;
                BOOL known1 = KnownType(type1, word[2].index, cw_index), known2 = KnownType(type2, word[3].index, cw_index);
                if (known1 && known2 && IsNumberType(type1) && IsNumberType(type2))
                    type_target = ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE;
                else if (known1 && type1 == ESTYPE_STRING || known2 && type2 == ESTYPE_STRING)
                    type_target = ESTYPE_BITS_STRING;
                else
                    type_target = ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE | ESTYPE_BITS_STRING;

                if (ReprocessInferredType(cw_index, type_target, FALSE, 0, word[1].index))
                    something_changed = TRUE;
            }
            break;

        case ESI_SUB:
            {
                if (ReprocessInferredType(cw_index, ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE, FALSE, 0, word[1].index))
                    something_changed = TRUE;
            }
            break;
        }
    }
    while (NextInstruction(code->data, word));

#ifdef _DEBUG
    if (something_changed)
    {
#ifdef VERBOSE_REPROCESSING
        printf("Changes detected, running again to ensure they are stable:\n");
#endif // VERBOSE_REPROCESSING
        OP_ASSERT(!ReprocessInferredTypes());
#ifdef VERBOSE_REPROCESSING
        printf("Done.\n");
#endif // VERBOSE_REPROCESSING
    }
#endif // _DEBUG

    return something_changed;
}

#ifdef VERBOSE_REPROCESSING
static const char *ValueTypeToString(ES_ValueType type)
{
    switch (type)
    {
    case ESTYPE_NULL: return "null";
    case ESTYPE_BOOLEAN: return "boolean";
    case ESTYPE_INT32: return "int32";
    case ESTYPE_DOUBLE: return "double";
    case ESTYPE_INT32_OR_DOUBLE: return "int32/double";
    case ESTYPE_STRING: return "string";
    case ESTYPE_OBJECT: return "object";
    default: return "unhandled type";
    }
}
static void REPORT_TYPE_CHANGE(unsigned cw_index, unsigned reg_index, BOOL old_known, ES_ValueType old_type, BOOL new_known, ES_ValueType new_type)
{
    printf("At index %u: reg(%u) changed, %s => %s\n", cw_index, reg_index, old_known ? ValueTypeToString(old_type) : "unknown", new_known ? ValueTypeToString(new_type) : "unknown");
}
#else // VERBOSE_REPROCESSING
#define REPORT_TYPE_CHANGE(cw_index, reg_index, old_known, old_type, new_known, new_type)
#endif // VERBOSE_REPROCESSING

BOOL
ES_Analyzer::ReprocessInferredType(unsigned cw_index, unsigned inferred_type, BOOL inferred_value, unsigned value_write_index, ES_CodeWord::Index target)
{
    ES_CodeOptimizationData::RegisterAccess *explicit_write = ExplicitWriteAt(target, cw_index);
    BOOL changed = FALSE;

    OP_ASSERT(!inferred_value || !explicit_write->IsValueKnown() || value_write_index == explicit_write->GetWriteIndex());

    unsigned previous_data = explicit_write->data;

    if (inferred_type != explicit_write->GetType() || !inferred_value != !explicit_write->IsValueKnown())
    {
        explicit_write->data = (inferred_type | explicit_write->GetType() |
                                ES_CodeOptimizationData::RegisterAccess::FLAG_WRITE |
                                ES_CodeOptimizationData::RegisterAccess::FLAG_EXPLICIT);

        if (inferred_value)
            explicit_write->data |= (ES_CodeOptimizationData::RegisterAccess::FLAG_VALUE_IS_KNOWN |
                                     (value_write_index << ES_CodeOptimizationData::RegisterAccess::SHIFT_WRITE_CW_INDEX));

        PropagateWriteInformation(target, explicit_write);

        if (explicit_write->data != previous_data)
            changed = TRUE;
    }

    return changed;
}

BOOL
ES_Analyzer::ReprocessCopy(unsigned cw_index, ES_CodeWord::Index from, ES_CodeWord::Index to)
{
    if (IsTrampled(from))
        return ReprocessInferredType(cw_index, ES_CodeOptimizationData::RegisterAccess::MASK_TYPE, FALSE, 0, to);
    else
    {
        ES_CodeOptimizationData::RegisterAccess *access = WriteReadAt(from, cw_index);
        return ReprocessInferredType(cw_index, access->GetType(), access->IsValueKnown(), access->GetWriteIndex(), to);
    }
}

void
ES_Analyzer::ProcessRShiftUnsigned(unsigned cw_index, ES_CodeWord *word, unsigned &inferred_type)
{
    inferred_type = 0;

    ES_Value_Internal rhs_value;
    unsigned rhs_value_cw_index;
    BOOL rhs_known = KnownValue(rhs_value, rhs_value_cw_index, word[3].index, cw_index);

    if (rhs_known && rhs_value.IsInt32() && rhs_value.GetInt32() != 0)
        /* Shift-count known to be non-zero => result is an uint32 with the most
           significant bit cleared, meaning it is int32 compatible. */
        inferred_type = ESTYPE_BITS_INT32;
    else
    {
        ES_Value_Internal lhs_value;
        unsigned lhs_value_cw_index;
        BOOL lhs_known = KnownValue(lhs_value, lhs_value_cw_index, word[2].index, cw_index);

        /* Shift-count is unknown or zero: */
        if (lhs_known && (lhs_value.IsInt32() && lhs_value.GetInt32() >= 0 || lhs_value.IsBoolean()))
            /* Left-hand side is known to be positive => result is int32
               compatible. */
            inferred_type = ESTYPE_BITS_INT32;
        else
            /* Left-hand side is or might be negative, and right-hand side is or
               might be zero => result might be an uint32 above INT_MAX. */
            if (rhs_known)
                inferred_type = ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE;
    }
}

#endif // ES_NATIVE_SUPPORT || ES_TEST_ANALYZER
