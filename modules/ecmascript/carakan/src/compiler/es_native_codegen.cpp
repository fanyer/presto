/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2010
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/compiler/es_native.h"
#include "modules/ecmascript/carakan/src/compiler/es_instruction_data.h"
#include "modules/ecmascript/carakan/src/util/es_instruction_string.h"

#ifdef ES_NATIVE_SUPPORT

ES_Native::Operand
ES_Native::InOperand(ArithmeticBlock *arithmetic_block, VirtualRegister *vr, BOOL ignore_register_allocation, BOOL prioritize_immediate)
{
    const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteReadAt(cw_index, vr->index);
    ES_Value_Internal value;

    if (prioritize_immediate && access->GetValue(code, value) && value.IsInt32())
        if (access->cw_index >= arithmetic_block->start_cw_index || !IsTrampled(vr))
        {
            ES_CodeWord *word = &code->data->codewords[access->GetWriteIndex()];

            OP_ASSERT(word->instruction == ESI_LOAD_INT32);

            return Operand(vr, word + 2);
        }

    if (!ignore_register_allocation)
        if (RegisterAllocation *allocation = vr->current_allocation)
        {
            while (allocation->end < cw_index && allocation->virtual_next)
                allocation = allocation->virtual_next;

            if (allocation->start < cw_index && allocation->end >= cw_index || allocation->start == cw_index && allocation->type != RegisterAllocation::TYPE_WRITE)
            {
                vr->current_allocation = allocation;
                allocation->native_register->current_allocation = allocation;

                return Operand(allocation->native_register);
            }
        }

    if (access->GetValue(code, value) && (value.IsInt32() || value.IsDouble()))
        if (access->cw_index >= arithmetic_block->start_cw_index || !IsTrampled(vr))
        {
            ES_CodeWord *word = &code->data->codewords[access->GetWriteIndex()];

            OP_ASSERT(word->instruction == ESI_LOAD_INT32 || word->instruction == ESI_LOAD_DOUBLE);

            if (word->instruction == ESI_LOAD_INT32)
                return Operand(vr, word + 2);
            else
                return Operand(vr, &code->data->doubles[word[2].index]);
        }

    return Operand(vr);
}

ES_Native::Operand
ES_Native::InOperand(VirtualRegister *vr, ES_CodeWord *immediate)
{
    if (RegisterAllocation *allocation = vr->current_allocation)
    {
        while (allocation->end < cw_index && allocation->virtual_next)
            allocation = allocation->virtual_next;

        if (allocation->start < cw_index && allocation->end >= cw_index || allocation->start == cw_index && allocation->type != RegisterAllocation::TYPE_WRITE)
        {
            vr->current_allocation = allocation;
            allocation->native_register->current_allocation = allocation;
            allocation->native_register->current_constant_value.SetInt32(immediate->immediate);

            return Operand(allocation->native_register);
        }
    }

    if (vr == ImmediateVR())
    {
        vr->value_known_in_arithmetic_block.SetInt32(immediate->immediate);
        vr->value_stored = FALSE;
    }

    return Operand(vr, immediate);
}

ES_Native::Operand
ES_Native::OutOperand(VirtualRegister *vr)
{
    if (RegisterAllocation *allocation = vr->current_allocation)
    {
        while ((allocation->start < cw_index || allocation->type != RegisterAllocation::TYPE_WRITE) && allocation->virtual_next && allocation->virtual_next->start <= cw_index)
            allocation = allocation->virtual_next;

        if (allocation->start == cw_index && allocation->type == RegisterAllocation::TYPE_WRITE)
            return Operand(allocation->native_register);
    }

    return Operand(vr);
}

unsigned
CountValidPropertyCaches(ES_Code *code, ES_CodeWord *word, BOOL is_get_cache)
{
    unsigned ncaches = 0;
    ES_Code::PropertyCache *cache = &(is_get_cache ? code->property_get_caches[word[4].index] : code->property_put_caches[word[4].index]);
    if (cache->class_id != ES_Class::NOT_CACHED_CLASS_ID)
    {
        while (cache)
        {
            if (ES_Code::IsSimplePropertyCache(cache, is_get_cache))
                ncaches++;

            cache = cache->next;
        }
    }

    return ncaches;
}

BOOL
AnalyzeGetCache(ES_Native::GetCacheAnalysis &analysis, ES_Code::PropertyCache *first_cache, OpMemGroup *arena)
{
    if (analysis.groups_count != 0 || analysis.negatives_count != 0)
        /* This cache has already been analyzed once. */
        return TRUE;

    ES_Code::PropertyCache *cache, *next, *previous, *first_per_storage_type[FIRST_SPECIAL_TYPE];

    if (first_cache->class_id == ES_Class::NOT_CACHED_CLASS_ID)
        return FALSE;

    op_memset(first_per_storage_type, 0, sizeof(first_per_storage_type));

    ES_Native::GetCacheGroup *groups = NULL;
    unsigned groups_count = 0;
    ES_Native::GetCacheNegative *negatives = NULL;
    unsigned negatives_count = 0;

    cache = first_cache;
    while (cache)
    {
        if (ES_Code::IsSimplePropertyCache(cache, TRUE))
            if (!cache->IsNegativeCache())
            {
                ES_StorageType storage_type = cache->GetStorageType();
                if (!first_per_storage_type[storage_type])
                    if (!cache->next || cache->class_id != cache->next->class_id || cache->next->GetStorageType() != storage_type)
                    {
                        first_per_storage_type[storage_type] = cache;
                        ++groups_count;
                    }
            }
            else if (!cache->next || cache->class_id != cache->next->class_id || !ES_Code::IsSimplePropertyCache(cache->next, TRUE))
                ++negatives_count;

        cache = cache->next;
    }

    OP_ASSERT(groups_count != 0 || negatives_count != 0);

    if (groups_count != 0)
    {
        groups = OP_NEWGROA_L(ES_Native::GetCacheGroup, groups_count, arena);
        groups_count = 0;

        cache = first_cache;
        previous = NULL;

        if (cache->next && cache->class_id == cache->next->class_id && (cache->IsNegativeCache() || cache->GetStorageType() == cache->next->GetStorageType()))
        {
            previous = cache;
            cache = cache->next;
        }

        BOOL has_empty_groups = FALSE;

        while (cache)
        {
            ES_StorageType storage_type;

            if (!cache->IsNegativeCache() && first_per_storage_type[storage_type = cache->GetStorageType()] == cache)
            {
                ES_Native::GetCacheGroup &group = groups[groups_count++];

                group.storage_type = storage_type;
                group.single_offset = TRUE;
                group.entries_count = 1;
                group.only_secondary_entries = FALSE;

                next = cache->next;
                while (next)
                {
                    if (ES_Code::IsSimplePropertyCache(next, TRUE) && !next->IsNegativeCache() && next->GetStorageType() == storage_type)
                        ++group.entries_count;

                    ES_Code::PropertyCache *next_next = next->next;

                    if (next_next && next->class_id == next_next->class_id && next->GetStorageType() == next_next->GetStorageType())
                        next_next = next_next->next;

                    next = next_next;
                }

                ES_Native::GetCacheGroup::Entry *entries = OP_NEWGROA_L(ES_Native::GetCacheGroup::Entry, group.entries_count, arena);
                group.entries = entries;

                unsigned secondary_entries_count = 0;

                next = cache;
                for (unsigned index = 0; index < group.entries_count;)
                {
                    ES_Native::GetCacheGroup::Entry &entry = entries[index];

                    entry.class_id = next->class_id;
                    entry.limit = next->GetLimit();

                    if (next->data.prototype_object)
                    {
                        entry.prototype = next->data.prototype_object;
                        entry.prototype_offset = next->GetOffset();
                        entry.prototype_storage_type = next->GetStorageType();

                        if (next->next && next->class_id == next->next->class_id)
                        {
                            OP_ASSERT(next->next->GetStorageType() != storage_type);
                            entry.prototype_secondary_entry = TRUE;
                            ++secondary_entries_count;
                        }
                        else
                            entry.prototype_secondary_entry = FALSE;
                    }
                    else
                    {
                        entry.positive = TRUE;
                        entry.positive_offset = next->GetOffset();
                    }

                    if (previous && previous->class_id == next->class_id)
                        if (previous->data.prototype_object)
                        {
                            OP_ASSERT(!entry.prototype);
                            entry.prototype = previous->data.prototype_object;
                            entry.prototype_offset = previous->GetOffset();
                            entry.prototype_storage_type = previous->GetStorageType();
                            entry.prototype_secondary_entry = FALSE;
                        }
                        else
                        {
                            OP_ASSERT(previous->IsNegativeCache());
                            entry.negative = TRUE;
                        }
                    else if (!next->object_class->NeedLimitCheck(entry.limit, next->data.prototype_object != NULL))
                        entry.limit = ES_LayoutIndex(UINT_MAX);

                    if (group.single_offset)
                    {
                        if (index == 0 && entry.positive)
                            group.common_offset = entry.positive_offset;
                        else if (entry.positive && entry.positive_offset != group.common_offset)
                            group.single_offset = FALSE;

                        if (entry.prototype && entry.prototype_storage_type == group.storage_type)
                            if (entry.positive && entry.positive_offset != entry.prototype_offset)
                                group.single_offset = FALSE;
                            else if (index == 0 && !entry.positive)
                                group.common_offset = entry.prototype_offset;
                            else if (entry.prototype_offset != group.common_offset)
                                group.single_offset = FALSE;
                    }

                    if (++index < group.entries_count)
                    {
                        do
                        {
                            previous = next;
                            next = next->next;
                        }
                        while (!ES_Code::IsSimplePropertyCache(next, TRUE) || next->IsNegativeCache() || next->GetStorageType() != storage_type);

                        if (next->next && next->class_id == next->next->class_id && next->next->GetStorageType() == next->GetStorageType())
                        {
                            previous = next;
                            next = next->next;
                        }
                    }
                }

                if (secondary_entries_count)
                    if (secondary_entries_count < group.entries_count)
                    {
                        /* Move secondary entries to the front of the list. */
                        ES_Native::GetCacheGroup::Entry *front = entries, *back = entries + group.entries_count - 1, temporary;

                        while (front < back)
                        {
                            while (front->prototype_secondary_entry)
                                ++front;
                            while (!back->prototype_secondary_entry)
                                --back;

                            if (front < back)
                            {
                                temporary = *front;
                                *front = *back;
                                *back = temporary;
                            }
                            else
                                break;
                        }
                    }
                    else
                        has_empty_groups = group.only_secondary_entries = TRUE;
            }

            previous = cache;
            cache = cache->next;
        }

        if (has_empty_groups)
        {
            /* Move empty groups (groups with only secondary entries) to the
               back of the list. */
            ES_Native::GetCacheGroup *front = groups, *back = groups + groups_count - 1, temporary;

            while (front < back)
            {
                while (!front->only_secondary_entries)
                    ++front;
                while (back->only_secondary_entries)
                    --back;

                if (front < back)
                {
                    temporary = *front;
                    *front = *back;
                    *back = temporary;
                }
                else
                    break;
            }
        }
    }

    if (negatives_count != 0)
    {
        negatives = OP_NEWGROA_L(ES_Native::GetCacheNegative, negatives_count, arena);
        negatives_count = 0;

        cache = first_cache;
        while (cache)
        {
            if (ES_Code::IsSimplePropertyCache(cache, TRUE) && cache->IsNegativeCache() && (!cache->next || cache->class_id != cache->next->class_id))
            {
                ES_Native::GetCacheNegative &missing = negatives[negatives_count++];

                missing.class_id = cache->class_id;

                if (cache->object_class->NeedLimitCheck(cache->GetLimit(), TRUE))
                    missing.limit = cache->GetLimit();
                else
                    missing.limit = ES_LayoutIndex(UINT_MAX);
            }

            cache = cache->next;
        }
    }

    analysis.groups = groups;
    analysis.groups_count = groups_count;
    analysis.negatives = negatives;
    analysis.negatives_count = negatives_count;

    return TRUE;
}

void
ES_Native::GenerateCode()
{
    ES_CodeWord *codewords = code->data->codewords;
    unsigned *instruction_offsets = code->data->instruction_offsets;
    ArithmeticBlock *arithmetic_block = first_arithmetic_block;

    AnalyzePropertyCaches(code);

    if (code->data->property_get_caches_count != 0)
        get_cache_analyses = OP_NEWGROA_L(GetCacheAnalysis, code->data->property_get_caches_count, arena);
    else
        get_cache_analyses = NULL;

    current_jump_target = optimization_data->first_jump_target;

    unsigned index;

    for (index = 0; index < virtual_registers_count; ++index)
    {
        VirtualRegister *vr = VR(index);
        vr->current_allocation = vr->first_allocation;
    }

    ImmediateVR()->current_allocation = ImmediateVR()->first_allocation;
    ScratchAllocationsVR()->current_allocation = ScratchAllocationsVR()->first_allocation;

    for (index = 0; index < native_registers_count; ++index)
    {
        NativeRegister *nr = NR(index);
        nr->current_allocation = nr->first_allocation;
    }

    instruction_index = 0;

    if (code->type == ES_Code::TYPE_FUNCTION)
    {
        ES_FunctionCode *fncode = static_cast<ES_FunctionCode *>(code);
        ES_FunctionCodeStatic *data = fncode->GetData();

        if (data->arguments_index != ES_FunctionCodeStatic::ARGUMENTS_NOT_USED || optimization_data->uses_this || code->data->register_frame_size >= ES_Native::LIGHT_WEIGHT_MAX_REGISTERS)
            is_trivial = FALSE;

        for (index = 2 + data->formals_count; index < data->first_temporary_register; ++index)
            if (index != data->arguments_index)
                if (fncode->CanHaveVariableObject())
                    EmitSetRegisterType(VR(index), ESTYPE_UNDEFINED);
                else
                {
                    const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->register_accesses[index];

                    if (access->cw_index == 0 && access->IsTypeKnown() && access->GetType() == ESTYPE_BITS_UNDEFINED && access->IsWrittenValueRead())
                        EmitSetRegisterType(VR(index), ESTYPE_UNDEFINED);
                }
    }

    for (; instruction_index < code->data->instruction_count; ++instruction_index)
    {
        cw_index = instruction_offsets[instruction_index];

        if (enumeration_object_register && cw_index >= enumeration_end_index)
            enumeration_object_register = enumeration_name_register = NULL;

        if (current_jump_target && current_jump_target->index == cw_index)
            current_jump_target = current_jump_target->next;

        if (next_jump_target && next_jump_target->data->index == cw_index)
        {
            if (next_jump_target->forward_jump)
                cg.SetJumpTarget(next_jump_target->forward_jump);
            if (next_jump_target->data->number_of_backward_jumps != 0)
            {
                next_jump_target->here = cg.Here(TRUE);
                EmitTick();
            }

            if (++next_jump_target == jump_targets + jump_targets_count)
                next_jump_target = NULL;
        }

        if (current_loop && current_loop->end_cw_index == cw_index)
            current_loop = NULL;

        ES_CodeWord *word = &codewords[cw_index];

        if (word == entry_point_cw && !entry_point_jump_target)
            entry_point_jump_target = EmitEntryPoint();

        if (arithmetic_block && arithmetic_block->start_instruction_index == instruction_index)
        {
            GenerateCodeInArithmeticBlock(arithmetic_block);

            instruction_index = arithmetic_block->end_instruction_index - 1;

            arithmetic_block = arithmetic_block->next;
        }
        else
        {
            BOOL trivial_instruction = FALSE;

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
            if (!(constructor && word->instruction == ESI_RETURN_NO_VALUE && cw_index + 1 == code->data->codewords_count))
            {
                TempBuffer &buffer = annotator.DisassembleInstruction(code, word);
                cg.Annotate(buffer.GetStorage(), buffer.Length());
            }
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

            switch (word->instruction)
            {
            case ESI_LOAD_STRING:
            case ESI_LOAD_DOUBLE:
                {
                    trivial_instruction = TRUE;

                    VirtualRegister *target_vr;
                    BOOL is_return = FALSE;

                    if (word[3].instruction == ESI_RETURN_VALUE && word[1].index == word[4].index && !constructor)
                    {
                        target_vr = VR(0);
                        is_return = TRUE;
                    }
                    else
                        target_vr = VR(word[1].index);

                    if (word->instruction == ESI_LOAD_DOUBLE)
                        EmitRegisterDoubleAssign(&code->data->doubles[word[2].index], target_vr);
                    else
                        EmitRegisterStringAssign(code->GetString(word[2].index), target_vr);

                    if (is_return)
                        EmitReturn();
                }
                break;

            case ESI_LOAD_INT32:
            case ESI_LOAD_TRUE:
            case ESI_LOAD_FALSE:
                {
                    trivial_instruction = TRUE;

                    VirtualRegister *target_vr;
                    unsigned next_cw_index = instruction_offsets[instruction_index + 1];
                    BOOL is_return = FALSE;

                    if (codewords[next_cw_index].instruction == ESI_RETURN_VALUE && word[1].index == codewords[next_cw_index + 1].index && !constructor)
                    {
                        target_vr = VR(0);
                        is_return = TRUE;
                    }
                    else
                        target_vr = VR(word[1].index);

                    ES_Value_Internal value;

                    if (word->instruction == ESI_LOAD_INT32)
                        value.SetInt32(word[2].immediate);
                    else
                        value.SetBoolean(word->instruction == ESI_LOAD_TRUE);

                    EmitSetRegisterValue(target_vr, value, TRUE);

                    if (is_return)
                        EmitReturn();
                }
                break;

            case ESI_LOAD_NULL:
                trivial_instruction = TRUE;
                EmitSetRegisterType(VR(word[1].index), ESTYPE_NULL);
                break;

            case ESI_LOAD_UNDEFINED:
                trivial_instruction = TRUE;
                EmitSetRegisterType(VR(word[1].index), ESTYPE_UNDEFINED);
                break;

            case ESI_LOAD_GLOBAL_OBJECT:
                trivial_instruction = TRUE;
                EmitStoreGlobalObject(VR(word[1].index));
                break;

            case ESI_COPY:
                trivial_instruction = TRUE;
                EmitRegisterCopy(VR(word[2].index), VR(word[1].index));
                break;

            case ESI_TONUMBER:
                {
                    VirtualRegister *source_vr = VR(word[2].index);
                    VirtualRegister *target_vr = VR(word[1].index);

                    if (!IsType(source_vr, ESTYPE_INT32))
                        EmitRegisterTypeCheck(source_vr, ESTYPE_INT32, NULL);

                    if (source_vr != target_vr)
                        EmitRegisterInt32Copy(source_vr, target_vr);
                }
                break;

            case ESI_TOPRIMITIVE:
                /* Note: the 'hint' immediate is willfully ignored; not made use
                   of during native code generation. */
                if (word[2].index != word[1].index)
                    EmitRegisterCopy(VR(word[2].index), VR(word[1].index));

            case ESI_TOPRIMITIVE1:
                EmitToPrimitive(VR(word[1].index));
                break;

            case ESI_IS_NULL_OR_UNDEFINED:
            case ESI_IS_NOT_NULL_OR_UNDEFINED:
                {
                    Operand *value_target = NULL, actual_value_target;
                    ES_CodeGenerator::JumpTarget *true_target = NULL, *false_target = NULL;

                    if (GetConditionalTargets(cw_index + 2, actual_value_target, true_target, false_target))
                    {
                        if (actual_value_target.IsValid())
                            value_target = &actual_value_target;

                        EmitNullOrUndefinedComparison(word->instruction == ESI_IS_NULL_OR_UNDEFINED, VR(word[1].index), value_target, true_target, false_target);

                        if (value_target)
                            ++instruction_index;
                        if (true_target || false_target)
                            ++instruction_index;
                    }
                    else
                        EmitSlowInstructionCall();
                }
                break;

            case ESI_FORMAT_STRING:
                {
                    if (code->data->profile_data && code->data->profile_data[cw_index + 5] != 0)
                    {
                        VirtualRegister *source_vr = VR(word[2].index);
                        VirtualRegister *target_vr = VR(word[1].index);

                        if (!IsType(source_vr, ESTYPE_STRING))
                            EmitRegisterTypeCheck(source_vr, ESTYPE_STRING, NULL);

                        EmitFormatString(target_vr, source_vr, word[5].index);
                    }
                    else
                        EmitSlowInstructionCall();
                }
                break;

            case ESI_EQ:
            case ESI_NEQ:
            case ESI_EQ_STRICT:
            case ESI_NEQ_STRICT:
                {
                    Operand *value_target = NULL, actual_value_target;
                    ES_CodeGenerator::JumpTarget *true_target = NULL, *false_target = NULL;

                    if (GetConditionalTargets(cw_index + 3, actual_value_target, true_target, false_target))
                    {
                        if (actual_value_target.IsValid())
                            value_target = &actual_value_target;

                        BOOL is_equal = word->instruction == ESI_EQ || word->instruction == ESI_EQ_STRICT;
                        BOOL is_strict = word->instruction == ESI_EQ_STRICT || word->instruction == ESI_NEQ_STRICT;
                        ES_ValueType rhs_type;

                        if (!value_target && is_strict && GetType(VR(word[2].index), rhs_type) && rhs_type == ESTYPE_NULL)
                            EmitStrictNullOrUndefinedComparison(is_equal, VR(word[1].index), ESTYPE_NULL, true_target, false_target);
                        else
                        {
                            unsigned handled_types = 0;

                            if (code->data->profile_data)
                                handled_types = code->data->profile_data[cw_index + 1] | code->data->profile_data[cw_index + 2];

                            ArithmeticInstructionProfile *ip = &instruction_profiles[instruction_index];

                            if (ip->lhs_possible != ESTYPE_BITS_ALL_TYPES)
                                handled_types = handled_types ? handled_types & ip->lhs_possible : ip->lhs_possible;
                            if (ip->rhs_possible != ESTYPE_BITS_ALL_TYPES)
                                handled_types = handled_types ? handled_types & ip->rhs_possible : ip->rhs_possible;

                            EmitComparison(is_equal, is_strict, VR(word[1].index), VR(word[2].index), handled_types, value_target, true_target, false_target);

                            if (value_target)
                                ++instruction_index;
                        }

                        if (true_target || false_target)
                            if (&codewords[instruction_offsets[++instruction_index]] == entry_point_cw)
                            {
                                ES_CodeGenerator::OutOfOrderBlock *ooo = cg.StartOutOfOrderBlock();
                                entry_point_jump_target = EmitEntryPoint();
                                EmitConditionalJump(true_target, false_target, TRUE);
                                cg.EndOutOfOrderBlock();
                                cg.SetOutOfOrderContinuationPoint(ooo);
                            }
                    }
                    else
                        EmitSlowInstructionCall();
                }
                break;

            case ESI_INSTANCEOF:
                {
                    Operand *value_target = NULL, actual_value_target;
                    ES_CodeGenerator::JumpTarget *true_target = NULL, *false_target = NULL;

                    if (GetConditionalTargets(cw_index + 3, actual_value_target, true_target, false_target))
                    {
                        if (actual_value_target.IsValid())
                            value_target = &actual_value_target;

                        EmitInstanceOf(VR(word[1].index), VR(word[2].index), value_target, true_target, false_target);

                        if (value_target)
                            ++instruction_index;
                        if (true_target || false_target)
                            if (&codewords[instruction_offsets[++instruction_index]] == entry_point_cw)
                            {
                                ES_CodeGenerator::OutOfOrderBlock *ooo = cg.StartOutOfOrderBlock();
                                entry_point_jump_target = EmitEntryPoint();
                                EmitConditionalJump(true_target, false_target, TRUE);
                                cg.EndOutOfOrderBlock();
                                cg.SetOutOfOrderContinuationPoint(ooo);
                            }
                    }
                    else
                        EmitSlowInstructionCall();
                }
                break;

            case ESI_GET:
            case ESI_GETI_IMM:
                {
                    VirtualRegister *target_vr = VR(word[1].index & 0x7fffffffu);
                    VirtualRegister *object_vr = VR(word[2].index);
                    VirtualRegister *index_vr = word->instruction == ESI_GET ? VR(word[3].index) : NULL;
                    unsigned index = word->instruction == ESI_GETI_IMM ? word[3].index : 0;

                    ES_Value_Internal value;
                    if (index_vr && IsImmediate(index_vr, value) && value.IsInt32() && value.GetInt32() > 0)
                    {
                        index_vr = NULL;
                        index = value.GetInt32();
                    }

                    property_value_write_vr = NULL;

                    if (index_vr && index_vr == enumeration_name_register)
                    {
                        if (!IsType(object_vr, ESTYPE_OBJECT) && !IsPreConditioned(object_vr, ESTYPE_OBJECT))
                            EmitRegisterTypeCheck(object_vr, ESTYPE_OBJECT, NULL);
                        if (!IsType(index_vr, ESTYPE_STRING) && !IsPreConditioned(index_vr, ESTYPE_STRING))
                            EmitRegisterTypeCheck(index_vr, ESTYPE_STRING, NULL);

                        EmitGetEnumerated(target_vr, object_vr, index_vr);
                    }
                    else if (IsType(object_vr, ESTYPE_STRING) || code->data->profile_data && ES_Indexed_Properties::ToTypeBits(code->data->profile_data[cw_index + 2]) == ES_Indexed_Properties::TYPE_BITS_STRING)
                    {
                        if (!IsType(object_vr, ESTYPE_STRING))
                            EmitRegisterTypeCheck(object_vr, ESTYPE_STRING, NULL);
                        if (index_vr && !IsType(index_vr, ESTYPE_INT32))
                            EmitRegisterTypeCheck(index_vr, ESTYPE_INT32, NULL);

                        EmitInt32StringIndexedGet(target_vr, object_vr, index_vr, index);
                    }
                    else
                    {
                        if (!IsType(object_vr, ESTYPE_OBJECT) && !IsPreConditioned(object_vr, ESTYPE_OBJECT))
                            EmitRegisterTypeCheck(object_vr, ESTYPE_OBJECT, NULL);
                        if (index_vr && !index_vr->integer_no_overflow && !IsType(index_vr, ESTYPE_INT32))
                            EmitRegisterTypeCheck(index_vr, ESTYPE_INT32, NULL);

                        if (code->data->profile_data && ES_Indexed_Properties::ToTypeBits(code->data->profile_data[cw_index + 2]) == ES_Indexed_Properties::TYPE_BITS_BYTE_ARRAY)
                            EmitInt32ByteArrayGet(target_vr, object_vr, index_vr, index);
                        else if (code->data->profile_data && ES_Indexed_Properties::ToTypeBits(code->data->profile_data[cw_index + 2]) == ES_Indexed_Properties::TYPE_BITS_TYPE_ARRAY)
                        {
                            unsigned type_array_bits = ES_Indexed_Properties::ToTypeArrayBits(code->data->profile_data[cw_index + 2], code->data->profile_data[cw_index + 3]);
                            EmitInt32TypedArrayGet(target_vr, object_vr, index_vr, index, type_array_bits);
                        }
                        else
                        {
                            if (CheckPropertyValueTransfer(target_vr))
                                property_value_write_vr = target_vr;

                            EmitInt32IndexedGet(target_vr, object_vr, index_vr, index);
                        }
                    }
                }
                break;

            case ESI_PUT:
            case ESI_PUTI_IMM:
                {
                    VirtualRegister *object_vr = VR(word[1].index);
                    VirtualRegister *index_vr = word->instruction == ESI_PUT ? VR(word[2].index) : NULL;
                    VirtualRegister *source_vr = VR(word[3].index);
                    unsigned constant_index = word[2].index;

                    if (!IsType(object_vr, ESTYPE_OBJECT) && !IsPreConditioned(object_vr, ESTYPE_OBJECT))
                        EmitRegisterTypeCheck(object_vr, ESTYPE_OBJECT, NULL);

                    if (index_vr && index_vr == enumeration_name_register)
                    {
                        if (!IsType(index_vr, ESTYPE_STRING) && !IsPreConditioned(index_vr, ESTYPE_STRING))
                            EmitRegisterTypeCheck(index_vr, ESTYPE_STRING, NULL);

                        EmitPutEnumerated(object_vr, index_vr, source_vr);
                    }
                    else
                    {
                        if (index_vr)
                        {
                            if (!index_vr->integer_no_overflow && !IsType(index_vr, ESTYPE_INT32))
                            {
                                if (code->data->profile_data && (code->data->profile_data[cw_index + 2] & 0x40) != 0)
                                    EmitNormalizeValue(index_vr);
                                else
                                    EmitRegisterTypeCheck(index_vr, ESTYPE_INT32, NULL);
                            }

                            ES_Value_Internal value;
                            if (IsImmediate(index_vr, value) && value.IsInt32() && value.GetInt32() >= 0)
                            {
                                index_vr = NULL;
                                constant_index = value.GetInt32();
                            }
                        }

                        BOOL known_type = FALSE, known_value = FALSE;
                        ES_ValueType type;
                        ES_Value_Internal value;

                        if (IsImmediate(source_vr, value, TRUE))
                        {
                            known_type = known_value = TRUE;
                            type = value.Type();
                        }
                        else if (GetType(source_vr, type) && type != ESTYPE_INT32_OR_DOUBLE)
                        {
                            known_type = TRUE;
                            value.SetType(type);
                        }

                        if (code->data->profile_data && ES_Indexed_Properties::ToTypeBits(code->data->profile_data[cw_index + 1]) == ES_Indexed_Properties::TYPE_BITS_BYTE_ARRAY && (!known_type || value.IsInt32() || known_value && value.IsDouble()))
                        {
                            if (!known_type)
                                EmitRegisterTypeCheck(source_vr, ESTYPE_INT32, NULL);

                            int known_int32_value;

                            if (known_value)
                                known_int32_value = value.GetNumAsInt32();

                            EmitInt32ByteArrayPut(object_vr, index_vr, constant_index, source_vr, known_value ? &known_int32_value : NULL);
                        }
                        else if (code->data->profile_data && ES_Indexed_Properties::ToTypeBits(code->data->profile_data[cw_index + 1]) == ES_Indexed_Properties::TYPE_BITS_TYPE_ARRAY)
                        {
                            unsigned type_array_bits = ES_Indexed_Properties::ToTypeArrayBits(code->data->profile_data[cw_index + 1], code->data->profile_data[cw_index + 2]);
                            unsigned char source_type_bits = code->data->profile_data[cw_index + 3] & (ESTYPE_BITS_DOUBLE | ESTYPE_BITS_INT32);

                            known_type = known_type && (value.IsDouble() || value.IsInt32());

                            if (known_type || source_type_bits != 0)
                                EmitInt32TypedArrayPut(object_vr, index_vr, constant_index, type_array_bits, source_vr, source_type_bits, known_type && known_value ? &value : NULL, known_type ? &type : NULL);
                            else
                                EmitSlowInstructionCall();
                        }
                        else
                            EmitInt32IndexedPut(object_vr, index_vr, constant_index, source_vr, known_type, known_value, value);
                    }
                }
                break;

            case ESI_GETN_IMM:
                {
                    VirtualRegister *target_vr = VR(word[1].index);
                    VirtualRegister *object_vr = VR(word[2].index);
                    JString *name = code->GetString(word[3].index & 0x7fffffff);

                    unsigned ncaches = PropertyCacheSize(word[4].index, TRUE);

                    if (ncaches == 0)
                        EmitSlowInstructionCall();
                    else
                    {
                        BOOL for_inlined_function_call = FALSE;

                        if (CheckPropertyValueTransfer(target_vr))
                            property_value_write_vr = target_vr;
                        else
                        {
                            property_value_write_vr = NULL;
                            BOOL needs_class_check_on_entry = FALSE;
                            for_inlined_function_call = CheckInlineFunction(cw_index, target_vr, &needs_class_check_on_entry);

                            if (for_inlined_function_call)
                            {
                                EmitSlowCaseCall(TRUE);
                                if (needs_class_check_on_entry)
                                {
                                    unsigned cache_index = word[4].index;
                                    ES_Code::PropertyCache *cache = &code->property_get_caches[cache_index];
                                    unsigned char *profile_data = code->data->profile_data;
                                    if (!profile_data)
                                    {
                                        context->AllocateProfileData(code->data);
                                        profile_data = code->data->profile_data;
                                    }
                                    unsigned char *mark_failure = profile_data + cw_index + 3;
                                    entry_point_jump_target = EmitInlineResumption(cache->class_id, object_vr, current_slow_case->GetJumpTarget(), mark_failure);
                                }
                            }
                        }

                        if (is_inlined_function_call && object_vr->index == 0)
                        {
                            ES_Class *klass = this_object_class;
                            ES_Object *prototype = NULL;

                            while (TRUE)
                            {
                                ES_Property_Info info;

                                if (ES_Class::Find(klass, name, info, UINT_MAX))
                                {
                                    ES_PropertyIndex index = klass->IndexOf(name);
                                    if (ES_Class::ActualClass(this_object_class)->NeedLimitCheck(klass->GetPropertyInfoAtIndex(index).Index(), FALSE))
                                        goto normal_access;

                                    EmitFetchPropertyValue(target_vr, prototype ? NULL : object_vr, prototype, klass, info.Index());
                                    if (!property_value_nr)
                                        property_value_write_vr = NULL;
                                    break;
                                }
                                else if (klass->HasHashTableProperties() || ES_Class::ActualClass(this_object_class)->NeedLimitCheck())
                                    goto normal_access;

                                prototype = klass->Prototype();

                                if (prototype)
                                    klass = prototype->Class();
                                else
                                {
                                    EmitSetRegisterType(target_vr, ESTYPE_UNDEFINED);
                                    break;
                                }
                            }
                        }
                        else
                        {
                        normal_access:
                            /* FIXME: Should have proper handling of more, and
                               mixed, cases here. */
                            ES_ValueType object_type = ESTYPE_OBJECT;

                            if (IsType(object_vr, ESTYPE_STRING))
                                object_type = ESTYPE_STRING;
                            else if (code->data->profile_data)
                            {
                                unsigned profiled_object_type = code->data->profile_data[cw_index + 2];
                                if (profiled_object_type == ESTYPE_BITS_STRING)
                                    object_type = ESTYPE_STRING;
                                else if (profiled_object_type && (profiled_object_type & (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE)) == profiled_object_type)
                                    object_type = ESTYPE_INT32_OR_DOUBLE;
                            }

                            if (!current_slow_case)
                                EmitSlowCaseCall();

                            if (!IsType(object_vr, object_type))
                                EmitRegisterTypeCheck(object_vr, object_type);

                            BOOL fetch_value = !for_inlined_function_call || inlined_function_call->function_fetched;

                            if (object_type != ESTYPE_OBJECT)
                            {
                                ES_Global_Object::PrototypeIndex prototype_index;

                                switch (object_type)
                                {
                                default: OP_ASSERT(FALSE);
                                case ESTYPE_STRING: prototype_index = ES_Global_Object::PI_STRING; break;
                                case ESTYPE_INT32_OR_DOUBLE:
                                case ESTYPE_INT32:
                                case ESTYPE_DOUBLE: prototype_index = ES_Global_Object::PI_NUMBER; break;
                                case ESTYPE_BOOLEAN: prototype_index = ES_Global_Object::PI_BOOLEAN;
                                }

                                ES_Object *immediate_prototype = code->global_object->prototypes[prototype_index], *prototype = immediate_prototype;
                                ES_Property_Info info;
                                ES_Layout_Info layout;

                                while (prototype)
                                    if (prototype->Class()->Find(name, info, prototype->Count()))
                                    {
                                        layout = prototype->Class()->GetLayoutInfo(info);
                                        break;
                                    }
                                    else
                                        prototype = prototype->Class()->Prototype();

                                ES_Object *function = NULL;

                                if (prototype && info.IsFunction())
                                {
                                    ES_Value_Internal value;
                                    prototype->GetCachedAtIndex(ES_PropertyIndex(info.data.index), value);

                                    function = value.GetObject();
                                    function->SetHasBeenInlined();
                                }

                                EmitPrimitiveNamedGet(target_vr, immediate_prototype, prototype, immediate_prototype->Class()->GetId(context), layout.GetOffset(), layout.GetStorageType(), function);

                                if (!property_value_nr)
                                    property_value_write_vr = NULL;
                            }
                            else
                            {
                                unsigned cache_index = word[4].index;
                                ES_Code::PropertyCache *cache = &code->property_get_caches[cache_index];

                                if (cache->data.prototype_object && !cache->next)
                                {
                                    ES_PropertyIndex index = cache->data.prototype_object->Class()->IndexOf(name);

                                    unsigned char *profile_data = code->data->profile_data;
                                    /* A cache hit is recorded in the instruction's profile data.
                                       Only attempt to set up function inlining from the (shared)
                                       cache if that has been recorded. */
                                    BOOL cache_hit = profile_data && (*(profile_data + cw_index + 3) & ES_CodeStatic::GET_CACHE_HIT) != 0;
                                    if (cache_hit && cache->class_id == cache->object_class->Id() && cache->data.prototype_object->Class()->GetPropertyInfoAtIndex(index).IsFunction())
                                    {
                                        /* If a function inlining candidate is known to have failed already,
                                           do not try again but fall into the slow case straight away. */
                                        if (profile_data && (*(profile_data + cw_index + 3) & ES_CodeStatic::GET_FAILED_INLINE) != 0)
                                        {
                                            cg.Jump(current_slow_case->GetJumpTarget());
                                            break;
                                        }

                                        OP_ASSERT(ES_Layout_Info::CheckType(ES_STORAGE_OBJECT, cache->GetStorageType()));

                                        ES_Value_Internal value;
                                        cache->data.prototype_object->GetCached(cache->GetOffset(), cache->cached_type, value);

                                        OP_ASSERT(value.IsObject() && value.GetObject()->IsFunctionObject());

                                        value.GetObject()->SetHasBeenInlined();

                                        EmitFetchFunction(target_vr, value.GetObject(), object_vr, cache->class_id, fetch_value);

                                        property_value_write_vr = NULL;
                                        break;
                                    }
                                }

                                GetCacheAnalysis &analysis = get_cache_analyses[cache_index];
                                AnalyzeGetCache(analysis, cache, arena);

                                if (property_value_write_vr == target_vr)
                                {
                                    if (analysis.negatives_count != 0)
                                        property_value_write_vr = NULL;

                                    for (unsigned group_index = 0; property_value_write_vr && group_index < analysis.groups_count; ++group_index)
                                    {
                                        const GetCacheGroup &group = analysis.groups[group_index];

                                        switch (group.storage_type)
                                        {
                                        case ES_STORAGE_OBJECT:
                                        case ES_STORAGE_OBJECT_OR_NULL:
                                        case ES_STORAGE_WHATEVER:
                                            break;

                                        default:
                                            property_value_write_vr = NULL;
                                            continue;
                                        }

                                        for (unsigned entry_index = 0; entry_index < group.entries_count; ++entry_index)
                                        {
                                            const GetCacheGroup::Entry &entry = group.entries[entry_index];
                                            if (entry.negative)
                                            {
                                                property_value_write_vr = NULL;
                                                break;
                                            }
                                        }
                                    }

                                    property_value_write_offset = 0;

                                    if (property_value_write_vr && analysis.groups[0].single_offset)
                                    {
                                        property_value_write_offset = analysis.groups[0].common_offset;

                                        if (analysis.groups[0].storage_type == ES_STORAGE_WHATEVER)
                                            property_value_write_offset += VALUE_IVALUE_OFFSET;

                                        for (unsigned group_index = 1; group_index < analysis.groups_count; ++group_index)
                                        {
                                            const GetCacheGroup &group = analysis.groups[group_index];

                                            if (!group.single_offset)
                                            {
                                                property_value_write_offset = 0;
                                                break;
                                            }

                                            unsigned value_offset = group.common_offset;

                                            if (group.storage_type == ES_STORAGE_WHATEVER)
                                                value_offset += VALUE_IVALUE_OFFSET;

                                            if (value_offset != property_value_write_offset)
                                            {
                                                property_value_write_offset = 0;
                                                break;
                                            }
                                        }
                                    }
                                }

                                EmitNamedGet(target_vr, object_vr, analysis, for_inlined_function_call, fetch_value);
                            }
                        }
                    }
                }
                break;

            case ESI_GET_LENGTH:
                {
                    VirtualRegister *target_vr = VR(word[1].index);
                    VirtualRegister *object_vr = VR(word[2].index);

                    ES_ValueType type;
                    unsigned handled, possible;

                    if (GetType(object_vr, type))
                        handled = possible = ES_Value_Internal::TypeBits(type);
                    else
                    {
                        type = ESTYPE_OBJECT;
                        possible = ESTYPE_BITS_ALL_TYPES;

                        if (code->data->profile_data && code->data->profile_data[cw_index + 2])
                            handled = code->data->profile_data[cw_index + 2];
                        else
                            handled = ESTYPE_BITS_OBJECT;
                    }

                    unsigned cache_index = word[4].index;
                    ES_Code::PropertyCache *cache = &code->property_get_caches[cache_index];

                    if (cache->class_id != 0)
                    {
                        ES_CodeGenerator::OutOfOrderBlock *ooo = (handled & ESTYPE_BITS_STRING) ? cg.StartOutOfOrderBlock() : NULL;

                        unsigned ncaches = PropertyCacheSize(cache_index, TRUE);

                        /* If we don't have any usable caches just go to slow case. */
                        if (ncaches == 0)
                            EmitSlowInstructionCall();
                        else
                        {
                            EmitSlowCaseCall();

                            if (!IsType(object_vr, ESTYPE_OBJECT) && !IsPreConditioned(object_vr, ESTYPE_OBJECT))
                                EmitRegisterTypeCheck(object_vr, ESTYPE_OBJECT);

                            GetCacheAnalysis &analysis = get_cache_analyses[cache_index];

                            AnalyzeGetCache(analysis, cache, arena);
                            EmitNamedGet(target_vr, object_vr, analysis);
                        }

                        if (handled & ESTYPE_BITS_STRING)
                        {
                            cg.EndOutOfOrderBlock();

                            EmitLengthGet(target_vr, object_vr, handled, possible, ooo->GetJumpTarget());

                            cg.SetOutOfOrderContinuationPoint(ooo);
                        }
                    }
                    else
                        EmitLengthGet(target_vr, object_vr, handled, possible, NULL);
                }
                break;

            case ESI_PUTN_IMM:
            case ESI_INIT_PROPERTY:
                {
                    VirtualRegister *object_vr = VR(word[1].index);
                    VirtualRegister *source_vr = VR(word[3].index);

                    ES_ValueType source_type;
                    BOOL known_type = GetType(source_vr, source_type);
                    ES_StorageType source_storage_type = known_type ? ES_Value_Internal::ConvertToStorageType(source_type) : ES_STORAGE_WHATEVER;

                    unsigned handled = PropertyCacheType(word[4].index, FALSE);

                    if ((handled & ESTYPE_BITS_DOUBLE) && (code->data->profile_data && code->data->profile_data[cw_index + 3] & ESTYPE_BITS_INT32))
                        handled |= ESTYPE_BITS_INT32;

                    unsigned ncaches = PropertyCacheSize(word[4].index, FALSE);

                    if (ncaches == 0 || known_type && (ES_Value_Internal::TypeBits(source_type) & handled) == 0)
                        EmitSlowInstructionCall();
                    else
                    {
                        if (!IsType(object_vr, ESTYPE_OBJECT))
                            EmitRegisterTypeCheck(object_vr, ESTYPE_OBJECT, NULL);

                        if (!known_type && ES_Value_Internal::IsMonoTypeBits(handled))
                        {
                            ES_ValueType handled_type;
                            if ((handled & (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE)) != (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE))
                                handled_type = ES_Value_Internal::TypeFromBits(handled);
                            else
                                handled_type = ESTYPE_INT32_OR_DOUBLE;

                            EmitRegisterTypeCheck(source_vr, handled_type, NULL);
                            source_storage_type = ES_Value_Internal::ConvertToStorageType(handled_type);
                        }

                        EmitNamedPut(object_vr, source_vr, code->GetString(word[2].index), ncaches > 1, source_storage_type);
                    }
                }
                break;

            case ESI_GET_GLOBAL:
                {
                    VirtualRegister *target_vr = VR(word[1].index & 0x7fffffff);
                    JString *name = code->GetString(word[2].index);
                    ES_Code::GlobalCache &cache = code->global_caches[word[3].index];

                    if (cache.class_id == ES_Class::GLOBAL_IMMEDIATE_CLASS_ID)
                    {
                        BOOL for_inlined_function_call = CheckInlineFunction(cw_index, target_vr, NULL);

                        EmitGlobalImmediateGet(target_vr, cache.cached_index, for_inlined_function_call, !for_inlined_function_call || inlined_function_call->function_fetched);
                    }
                    else
                    {
                        ES_Property_Info info;

                        if (cache.class_id != code->global_object->Class()->Id())
                            if (ES_Class::Find(code->global_object->Class(), name, info, code->global_object->Count()) && info.CanCacheGet())
                            {
                                cache.class_id = code->global_object->Class()->GetId(context);
                                ES_Layout_Info layout = code->global_object->Class()->GetLayoutInfo(info);
                                cache.cached_offset = layout.GetOffset();
                                cache.cached_type = ES_Value_Internal::CachedTypeBits(layout.GetStorageType());
                            }

                        if (CheckPropertyValueTransfer(target_vr))
                            property_value_write_vr = target_vr;

                        EmitGlobalGet(target_vr, cache.class_id, cache.cached_offset, cache.cached_type);
                    }
                }
                break;

            case ESI_PUT_GLOBAL:
                {
                    JString *name = code->GetString(word[1].index);
                    VirtualRegister *source_vr = VR(word[2].index);
                    ES_Code::GlobalCache &cache = code->global_caches[word[3].index];

                    ES_ValueType source_type;
                    BOOL known_type = GetType(source_vr, source_type);
                    ES_StorageType source_storage_type = known_type ? ES_Value_Internal::ConvertToStorageType(source_type) : ES_STORAGE_WHATEVER;

                    if (cache.class_id == ES_Class::GLOBAL_IMMEDIATE_CLASS_ID)
                        EmitGlobalImmediatePut(cache.cached_index, source_vr);
                    else
                    {
                        ES_Property_Info info;

                        if (cache.class_id != code->global_object->Class()->Id())
                            if (ES_Class::Find(code->global_object->Class(), name, info, code->global_object->Count()) && info.CanCachePut())
                            {
                                cache.class_id = code->global_object->Class()->GetId(context);
                                ES_Layout_Info layout = code->global_object->Class()->GetLayoutInfo(info);
                                cache.cached_offset = layout.GetOffset();
                                cache.cached_type = ES_Value_Internal::CachedTypeBits(layout.GetStorageType());
                            }

                        unsigned handled = ES_Value_Internal::TypeBits(ES_Value_Internal::ValueTypeFromCachedTypeBits(cache.cached_type));

                        if ((handled & ESTYPE_BITS_DOUBLE) && known_type && (source_type == ESTYPE_INT32))
                            handled |= ESTYPE_BITS_INT32;

                        if (known_type && (ES_Value_Internal::TypeBits(source_type) & handled) == 0)
                            EmitSlowInstructionCall();
                        else
                        {
                            if (!known_type && ES_Value_Internal::IsMonoTypeBits(handled))
                            {
                                ES_ValueType handled_type;
                                if ((handled & (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE)) != (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE))
                                    handled_type = ES_Value_Internal::TypeFromBits(handled);
                                else
                                    handled_type = ESTYPE_INT32_OR_DOUBLE;

                                EmitRegisterTypeCheck(source_vr, handled_type, NULL);
                                source_storage_type = ES_Value_Internal::ConvertToStorageType(handled_type);
                            }

                            EmitGlobalPut(source_vr, cache.class_id, cache.cached_offset, cache.cached_type, source_storage_type);
                        }
                    }
                }
                break;

            case ESI_GET_LEXICAL:
                {
                    VirtualRegister *target_vr = VR(word[1].index);
                    VirtualRegister *function_vr = VR(1);

                    EmitLexicalGet(target_vr, function_vr, word[2].index, word[3].index);
                }
                break;

            case ESI_CONDITION:
                if (word[2].instruction == ESI_JUMP_IF_TRUE || word[2].instruction == ESI_JUMP_IF_FALSE)
                {
                    Operand value_target;
                    ES_CodeGenerator::JumpTarget *true_target = NULL, *false_target = NULL;

                    if (&word[2] == entry_point_cw)
                        entry_point_jump_target = EmitEntryPoint();

                    GetConditionalTargets(cw_index + 2, value_target, true_target, false_target);
                    EmitConditionalJumpOnValue(VR(word[1].index), true_target, false_target);

                    ++instruction_index;
                }
                else
                    EmitSlowInstructionCall();
                break;

            case ESI_JUMP:
                {
                    unsigned jump_target_cw_index = cw_index + 1 + word[1].offset;
                    JumpTarget *jump_target = FindJumpTarget(jump_target_cw_index);
                    ES_CodeGenerator::JumpTarget *cg_target;

                    if (jump_target_cw_index <= cw_index)
                        cg_target = jump_target->here;
                    else
                    {
                        if (!jump_target->forward_jump)
                            jump_target->forward_jump = cg.ForwardJump();

                        cg_target = jump_target->forward_jump;
                    }

                    cg.Jump(cg_target);
                }
                break;

            case ESI_JUMP_IF_TRUE:
            case ESI_JUMP_IF_FALSE:
                {
                    Operand value_target;
                    ES_CodeGenerator::JumpTarget *true_target = NULL, *false_target = NULL;

                    GetConditionalTargets(cw_index, value_target, true_target, false_target);

                    EmitConditionalJump(true_target, false_target, TRUE);
                }
                break;

            case ESI_START_LOOP:
                if (!current_loop)
                    StartLoop(cw_index, word[1].index);
                break;

            case ESI_EVAL:
            case ESI_CALL:
            case ESI_APPLY:
            case ESI_CONSTRUCT:
                {
                    VirtualRegister *this_vr = VR(word[1].index);
                    VirtualRegister *fn_vr = VR(this_vr->index + 1);
                    unsigned argc = word[2].index;

                    ES_BuiltInFunction builtin = code->data->profile_data ? static_cast<ES_BuiltInFunction>(code->data->profile_data[cw_index + 1]) : ES_BUILTIN_FN_NONE;
                    ES_Function *inlined_function = NULL;
                    unsigned char *mark_failed_inline = NULL;
                    ES_Native *native = NULL;
                    BOOL function_fetched = TRUE;
                    BOOL uses_this_object = TRUE;

                    if (inlined_function_call && inlined_function_call->call_cw_index == cw_index && inlined_function_call->native)
                    {
                        inlined_function = inlined_function_call->function;
                        native = inlined_function_call->native;
                        function_fetched = inlined_function_call->function_fetched;
                        uses_this_object = inlined_function_call->uses_this_object;
                        unsigned char *profile_data = code->data->profile_data;
                        if (!profile_data)
                        {
                            context->AllocateProfileData(code->data);
                            profile_data = code->data->profile_data;
                        }
                        mark_failed_inline = profile_data + inlined_function_call->get_cw_index + 3;

                        inlined_function_call = inlined_function_call->next;
                    }

                    if (builtin > ES_BUILTIN_FN_LAST_INLINED)
                        builtin = ES_BUILTIN_FN_NONE;

                    if (word->instruction != ESI_CONSTRUCT && word->instruction != ESI_APPLY)
                        if ((argc & 0x80000000u) != 0)
                        {
                            if (uses_this_object)
                                EmitSetRegisterType(this_vr, ESTYPE_UNDEFINED);

                            argc &= 0x7fffffffu;
                        }

                    if (inlined_function /* && entry_point_cw != word */)
                    {
                        ES_CodeGenerator::OutOfOrderBlock *failure = EmitInlinedCallPrologue(this_vr, fn_vr, inlined_function, mark_failed_inline, argc, function_fetched);

                        native->GenerateInlinedFunctionCall(failure);

                        EmitInlinedCallEpilogue(this_vr, fn_vr, argc);
                    }
                    else if (word->instruction == ESI_APPLY)
                    {
                        VirtualRegister *applied_vr = VR(this_vr->index + 2);

                        if (!IsType(this_vr, ESTYPE_OBJECT))
                            EmitRegisterTypeCheck(this_vr, ESTYPE_OBJECT, NULL);
                        if (!IsType(applied_vr, ESTYPE_OBJECT))
                            EmitRegisterTypeCheck(applied_vr, ESTYPE_OBJECT, NULL);

                        EmitApply(this_vr, fn_vr, applied_vr, argc);
                    }
                    else
                    {
                        if (!IsType(fn_vr, ESTYPE_OBJECT))
                            EmitRegisterTypeCheck(fn_vr, ESTYPE_OBJECT, NULL);

                        if (word->instruction == ESI_EVAL)
                            EmitEval(this_vr, fn_vr, argc);
                        else
                            EmitCall(this_vr, fn_vr, argc, builtin);
                    }
                }
                break;

            case ESI_REDIRECTED_CALL:
                {
                    VirtualRegister *function_vr = VR(word[1].index);
                    VirtualRegister *apply_vr = VR(word[2].index);

                    if (!IsType(function_vr, ESTYPE_OBJECT))
                        EmitRegisterTypeCheck(function_vr, ESTYPE_OBJECT, NULL);
                    if (!IsType(apply_vr, ESTYPE_OBJECT))
                        EmitRegisterTypeCheck(apply_vr, ESTYPE_OBJECT, NULL);

                    ES_CodeWord *next_instruction = &codewords[instruction_offsets[instruction_index + 1]];
                    OP_ASSERT(next_instruction->instruction == ESI_RETURN_VALUE || next_instruction->instruction == ESI_RETURN_NO_VALUE);
                    EmitRedirectedCall(function_vr, apply_vr, constructor && next_instruction->instruction == ESI_RETURN_VALUE);
                }
                break;

            case ESI_RETURN_VALUE:
                {
                    trivial_instruction = TRUE;

                    VirtualRegister *value_vr = VR(word[1].index);

                    if (value_vr->index != 0)
                    {
                        if (constructor && !IsType(value_vr, ESTYPE_OBJECT))
                        {
                            ES_CodeGenerator::JumpTarget *jt_not_object = cg.ForwardJump();
                            EmitRegisterTypeCheck(value_vr, ESTYPE_OBJECT, jt_not_object);
                            EmitRegisterCopy(value_vr, VR(0));
                            cg.SetJumpTarget(jt_not_object);
                        }
                        else
                            EmitRegisterCopy(value_vr, VR(0));
                    }

                    EmitReturn();
                }
                break;

            case ESI_RETURN_NO_VALUE:
                trivial_instruction = TRUE;
                if (!constructor)
                    EmitSetRegisterType(VR(0), ESTYPE_UNDEFINED);
                EmitReturn();
                break;

            case ESI_CONSTRUCT_OBJECT:
                {
                    VirtualRegister *target_vr = VR(word[1].index);
                    ES_Class *klass = code->GetObjectLiteralClass(context, word[2].index);

                    if (CanAllocateObject(klass, 0))
                        EmitNewObject(target_vr, klass, word + 3, klass->Count());
                    else
                        EmitSlowInstructionCall();
                }
                break;

            case ESI_NEW_ARRAY:
                {
                    VirtualRegister *target_vr = VR(word[1].index);
                    unsigned length = word[2].index, capacity;

                    EmitNewArray(target_vr, length, capacity, NULL);

                    ES_CodeWord *valueword = word + 3, *putword = valueword;
                    unsigned initindex = 0;

                another_element:
                    if (initindex < length)
                    {
                        int noperands = 1;

                        if (next_jump_target && valueword == &codewords[next_jump_target->data->index])
                            break;

                        switch (valueword->instruction)
                        {
                        case ESI_LOAD_STRING:
                        case ESI_LOAD_DOUBLE:
                        case ESI_LOAD_INT32:
                            ++noperands;

                        case ESI_LOAD_NULL:
                        case ESI_LOAD_UNDEFINED:
                        case ESI_LOAD_TRUE:
                        case ESI_LOAD_FALSE:
                            putword = valueword + 1 + noperands;

                            if (!VR(valueword[1].index)->is_temporary)
                                break;

                        case ESI_PUTI_IMM:
                            if (putword->instruction == ESI_PUTI_IMM &&
                                putword[1].index == word[1].index &&
                                (valueword->instruction == ESI_PUTI_IMM || putword[3].index == valueword[1].index))
                            {
                                unsigned putindex = putword[2].index;

                                if (putindex >= length)
                                    break;

                                if (initindex < putindex)
                                    EmitNewArrayReset(target_vr, initindex, putindex);

                                EmitNewArrayValue(target_vr, valueword, putindex);

                                instruction_index += valueword->instruction == ESI_PUTI_IMM ? 1 : 2;
                                initindex = putindex + 1;
                                valueword = putword += 4;

                                goto another_element;
                            }
                        }
                    }

                    if (initindex < capacity)
                        EmitNewArrayReset(target_vr, initindex, capacity);
                }
                break;

            case ESI_CONSTRUCT_ARRAY:
                {
                    VirtualRegister *target_vr = VR(word[1].index);
                    unsigned length = word[2].index, capacity;
                    ES_Compact_Indexed_Properties *&representation = code->constant_array_literals[word[3].index];

                    if (!representation)
                        representation = ES_Compact_Indexed_Properties::Make(context, code, code->data->constant_array_literals[word[3].index]);

                    EmitNewArray(target_vr, length, capacity, representation);
                }
                break;

            case ESI_TABLE_SWITCH:
                {
                    ES_CodeStatic::SwitchTable *data1 = &code->data->switch_tables[word[2].index];
                    ES_Code::SwitchTable *data2 = &code->switch_tables[word[2].index];

                    SwitchTable *table = OP_NEWGRO_L(SwitchTable, (data1, data2), Arena());
                    unsigned ncases = data1->maximum - data1->minimum + 1;

                    table->targets = OP_NEWGROA_L(ES_CodeGenerator::JumpTarget *, ncases, Arena());
                    table->blob = cg.NewBlob(ncases * sizeof(void *));

                    unsigned *codeword_indeces = data1->codeword_indeces, default_codeword_index = data1->default_codeword_index;

                    for (unsigned i = 0; i < ncases || !table->default_target; ++i)
                    {
                        unsigned target_cw_index;

                        if (i < ncases)
                            target_cw_index = codeword_indeces[data1->minimum + static_cast<int>(i)];
                        else
                            target_cw_index = default_codeword_index;

                        if (target_cw_index == default_codeword_index && table->default_target)
                        {
                            if (i < ncases)
                                table->targets[i] = table->default_target;

                            continue;
                        }

                        unsigned index;
                        for (index = 0; jump_targets[index].data->index != target_cw_index; ++index)
                            ;
                        JumpTarget *target = &jump_targets[index];

                        if (!target->forward_jump && !target->here)
                            target->forward_jump = cg.ForwardJump();

                        ES_CodeGenerator::JumpTarget *jt = target->forward_jump ? target->forward_jump : target->here;

                        if (i < ncases)
                            table->targets[i] = jt;

                        if (target_cw_index == default_codeword_index)
                            table->default_target = jt;
                    }

                    if (!first_switch_table)
                        first_switch_table = last_switch_table = table;
                    else
                        last_switch_table = last_switch_table->next = table;

                    EmitTableSwitch(VR(word[1].index), data1->minimum, data1->maximum, table->blob, table->default_target);
                }
                break;

            default:
                //OP_ASSERT(!IsHandledInline(word));

                if (word->instruction == ESI_ENUMERATE)
                {
                    OP_ASSERT(word[4].instruction == ESI_NEXT_PROPERTY && word[9].instruction == ESI_JUMP_IF_FALSE);

                    enumeration_object_register = VR(word[2].index);
                    enumeration_name_register = VR(word[5].index);
                    enumeration_end_index = cw_index + 10 + word[10].offset;
                }

                EmitSlowInstructionCall();
                break;
            }

            if (!trivial_instruction)
                is_trivial = FALSE;

            if (current_slow_case)
            {
                cg.SetOutOfOrderContinuationPoint(current_slow_case);
                current_slow_case = NULL;
            }

            if (property_value_write_vr)
            {
                OP_ASSERT(property_value_nr);

                property_value_read_vr = property_value_write_vr;
                property_value_write_vr = NULL;
            }
            else
            {
                property_value_read_vr = NULL;
                ClearPropertyValueTransferRegister();
            }
        }
    }

    OP_ASSERT(property_value_fail == NULL);
}

void
ES_Native::GenerateCodeInArithmeticBlock(ArithmeticBlock *arithmetic_block)
{
    ES_CodeWord *codewords = code->data->codewords;
    unsigned *instruction_offsets = code->data->instruction_offsets;

    unsigned start_instruction_index = arithmetic_block->start_instruction_index;
    unsigned end_instruction_index = arithmetic_block->end_instruction_index;
    ArithmeticInstructionProfile *instruction_profiles = arithmetic_block->instruction_profiles;
    BOOL returned = FALSE, flushed = FALSE;

    start_cw_index = instruction_offsets[start_instruction_index];

    ES_CodeGenerator::JumpTarget *slow_case = NULL;

    if (arithmetic_block->last_uncertain_instruction_index != UINT_MAX || entry_point_cw_index > arithmetic_block->start_cw_index && entry_point_cw_index < arithmetic_block->end_cw_index)
    {
        if (!is_light_weight || entry_point_cw_index > arithmetic_block->start_cw_index && entry_point_cw_index < arithmetic_block->end_cw_index)
            EmitArithmeticBlockSlowPath(arithmetic_block);

        if (!is_light_weight)
            slow_case = arithmetic_block->slow_case->GetJumpTarget();
        else
        {
            EmitLightWeightFailure();
            slow_case = light_weight_failure->GetJumpTarget();
        }
    }

    for (unsigned index = 0; index < native_registers_count; ++index)
        NR(index)->current_constant_value.SetUndefined();

    cw_index = arithmetic_block->start_cw_index;

    for (unsigned index = 0; index < virtual_registers_count; ++index)
    {
        VirtualRegister *vr = VR(index);

        /* We can't trust GetType() to determine this for temporary registers,
           because writes to temporary registers inside arithmetic blocks are
           often eliminated along with the corresponding register type
           update. */
        if (vr->is_temporary || !GetType(vr, vr->type_known_in_arithmetic_block))
            vr->type_known_in_arithmetic_block = ESTYPE_UNDEFINED;

        vr->value_known_in_arithmetic_block.SetUndefined();
        vr->value_stored = TRUE;
        vr->value_needs_flush = FALSE;

        if (RegisterAllocation *allocation = vr->current_allocation)
        {
            while (allocation && allocation->arithmetic_block->index < arithmetic_block->index)
                allocation = allocation->virtual_next;

            vr->current_allocation = allocation;
        }
    }

    VirtualRegister *immediate_vr = ImmediateVR();

    if (RegisterAllocation *allocation = immediate_vr->current_allocation)
    {
        while (allocation && allocation->arithmetic_block->index < arithmetic_block->index)
            allocation = allocation->virtual_next;

        immediate_vr->current_allocation = allocation;
    }

    for (instruction_index = start_instruction_index; instruction_index < end_instruction_index && !returned; ++instruction_index)
    {
        cw_index = instruction_offsets[instruction_index];

        ES_CodeWord *word = &codewords[cw_index];
        ArithmeticInstructionProfile *ip = &instruction_profiles[instruction_index - start_instruction_index];

        unsigned target_operand_index = 1, lhs_operand_index = 2, rhs_operand_index = 3, immediate_operand_index = UINT_MAX;

        switch (word->instruction)
        {
        case ESI_LOAD_DOUBLE:
        case ESI_LOAD_INT32:
            lhs_operand_index = rhs_operand_index = UINT_MAX;
            break;

        case ESI_LSHIFT_IMM:
        case ESI_RSHIFT_SIGNED_IMM:
        case ESI_RSHIFT_UNSIGNED_IMM:
        case ESI_ADD_IMM:
        case ESI_SUB_IMM:
        case ESI_MUL_IMM:
        case ESI_DIV_IMM:
        case ESI_REM_IMM:
        case ESI_BITAND_IMM:
        case ESI_BITOR_IMM:
        case ESI_BITXOR_IMM:
            immediate_operand_index = 3;

        case ESI_COMPL:
        case ESI_NEG:
            rhs_operand_index = UINT_MAX;
            break;

        case ESI_EQ:
        case ESI_NEQ:
        case ESI_EQ_STRICT:
        case ESI_NEQ_STRICT:
        case ESI_LT:
        case ESI_LTE:
        case ESI_GT:
        case ESI_GTE:
            target_operand_index = UINT_MAX;
            lhs_operand_index = 1;
            rhs_operand_index = 2;
            break;

        case ESI_EQ_IMM:
        case ESI_NEQ_IMM:
        case ESI_EQ_STRICT_IMM:
        case ESI_NEQ_STRICT_IMM:
        case ESI_LT_IMM:
        case ESI_LTE_IMM:
        case ESI_GT_IMM:
        case ESI_GTE_IMM:
            target_operand_index = UINT_MAX;
            lhs_operand_index = 1;
            rhs_operand_index = UINT_MAX;
            immediate_operand_index = 2;
            break;

        case ESI_INC:
        case ESI_DEC:
            lhs_operand_index = 1;
            rhs_operand_index = UINT_MAX;
            break;

        case ESI_JUMP_IF_TRUE:
        case ESI_JUMP_IF_FALSE:
            target_operand_index = lhs_operand_index = rhs_operand_index = UINT_MAX;
            break;

        case ESI_RETURN_VALUE:
            lhs_operand_index = 1;
            target_operand_index = rhs_operand_index = UINT_MAX;
        }

        unsigned index, max;

        for (index = 2; index < virtual_registers_count; ++index)
        {
            VirtualRegister *vr = VR(index);

            if (RegisterAllocation *allocation = vr->current_allocation)
            {
                while (allocation && allocation->end < cw_index)
                    allocation = allocation->virtual_next;

                if (allocation && allocation->type == RegisterAllocation::TYPE_WRITE && allocation->start < cw_index && allocation->end == cw_index)
                {
                    if (allocation->intermediate_write)
                    {
                        const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteAt(allocation->start, vr->index);
                        const ES_CodeOptimizationData::RegisterAccess *read = access->GetNextRead();

                        while (read)
                        {
                            if (read->cw_index > allocation->end)
                                break;

                            read = read->GetNextRead();
                        }

                        if (!read)
                            /* Value not read "outside" the register allocation
                               means there's no need to write the value to the
                               virtual register at all. */
                            continue;
                    }

                    NativeRegister *nr = allocation->native_register;
                    ES_ValueType type = nr->type == NativeRegister::TYPE_INTEGER ? ESTYPE_INT32 : ESTYPE_DOUBLE;

                    if (vr->stack_frame_offset != INT_MAX)
                        FreeStackFrameStorage(vr);

                    if (arithmetic_block->last_uncertain_cw_index != UINT_MAX && cw_index <= arithmetic_block->last_uncertain_cw_index && !IsSafeWrite(arithmetic_block, vr, allocation->start))
                        AllocateStackFrameStorage(arithmetic_block, vr, type);

                    if (!vr->value_stored)
                    {
                        EmitSetRegisterValue(vr, vr->value_known_in_arithmetic_block, vr->stack_frame_offset == INT_MAX);

                        vr->value_stored = TRUE;
                        vr->value_needs_flush = FALSE;
                    }
                    else
                        EmitStoreRegisterValue(vr, nr, vr->stack_frame_offset == INT_MAX);

                    allocation->type = RegisterAllocation::TYPE_READ;
                }
            }
        }

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
        TempBuffer &buffer = annotator.DisassembleInstruction(code, word);
        cg.Annotate(buffer.GetStorage(), buffer.Length());
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

        if (RegisterAllocation *allocation = immediate_vr->current_allocation)
        {
            while (allocation->virtual_next && allocation->end < cw_index)
                allocation = allocation->virtual_next;

            immediate_vr->current_allocation = allocation;

            while (allocation->virtual_next && (allocation->virtual_next->type == RegisterAllocation::TYPE_READ || allocation->virtual_next->type == RegisterAllocation::TYPE_CONVERT) && allocation->virtual_next->start <= cw_index)
                allocation = allocation->virtual_next;

            if (allocation->start == cw_index)
            {
                OP_ASSERT(immediate_operand_index != UINT_MAX);

                EmitRegisterInt32Assign(Operand(immediate_vr, &word[immediate_operand_index]), Operand(allocation->native_register));
            }
        }

        for (index = 1, max = g_instruction_operand_count[word->instruction] + 1; index < max; ++index)
            if (index == lhs_operand_index || index == rhs_operand_index && word[lhs_operand_index].index != word[rhs_operand_index].index)
            {
                VirtualRegister *vr = VR(word[index].index);
                unsigned handled, possible;
                BOOL constant;

                if (index == lhs_operand_index)
                    handled = ip->lhs_handled, possible = ip->lhs_possible, constant = ip->lhs_constant;
                else
                    handled = ip->rhs_handled, possible = ip->rhs_possible, constant = ip->rhs_constant;

                if (RegisterAllocation *allocation = vr->current_allocation)
                {
                    while (allocation->virtual_next && allocation->end < cw_index)
                        allocation = allocation->virtual_next;

                    vr->current_allocation = allocation;

                    while (allocation->virtual_next && (allocation->virtual_next->type == RegisterAllocation::TYPE_READ || allocation->virtual_next->type == RegisterAllocation::TYPE_CONVERT) && allocation->virtual_next->start <= cw_index)
                        allocation = allocation->virtual_next;

                    if (allocation->start == cw_index)
                    {
                        NativeRegister *nr = allocation->native_register;

                        if (allocation->type == RegisterAllocation::TYPE_READ)
                        {
                            if (constant)
                                if (nr->type == NativeRegister::TYPE_INTEGER)
                                    EmitRegisterInt32Assign(InOperand(arithmetic_block, vr, TRUE), Operand(allocation->native_register));
                                else
                                    EmitRegisterDoubleAssign(InOperand(arithmetic_block, vr, TRUE).value, Operand(allocation->native_register));
                            else
                            {
                                ES_CodeGenerator::JumpTarget *type_check_fail;

                                if (handled != possible && !IsPreConditioned(vr, ES_Value_Internal::TypeFromBits(handled)))
                                    type_check_fail = slow_case;
                                else
                                    type_check_fail = NULL;

                                vr->type_known_in_arithmetic_block = ES_Value_Internal::TypeFromBits(handled);

                                EmitLoadRegisterValue(nr, vr, type_check_fail);
                            }

                            vr->current_allocation = allocation;

                            nr->current_allocation = allocation;
                            nr->current_constant_value.SetUndefined();

                            continue;
                        }
                        else if (allocation->type == RegisterAllocation::TYPE_CONVERT)
                        {
                            if (!vr->value_stored)
                            {
                                EmitSetRegisterValue(vr, vr->value_known_in_arithmetic_block, vr->stack_frame_offset == INT_MAX, FALSE);
                                vr->value_stored = TRUE;
                                vr->value_needs_flush = FALSE;
                            }
                            EmitConvertRegisterValue(nr, vr, handled, possible, slow_case);

                            vr->current_allocation = allocation;

                            nr->current_allocation = allocation;
                            nr->current_constant_value.SetUndefined();

                            continue;
                        }
                    }
                }

                if (handled != possible)
                {
                    if ((handled & (handled - 1)) != 0 || !IsPreConditioned(vr, ip->target_type))
                        EmitRegisterTypeCheck(vr, ip->target_type, slow_case);

                    vr->type_known_in_arithmetic_block = ip->target_type;
                }
            }

        BOOL trivial_instruction = FALSE;
        BOOL result_as_condition = FALSE;
        BOOL restore_condition = FALSE;
        BOOL immediate_rhs = FALSE;

        switch (word->instruction)
        {
        case ESI_LOAD_DOUBLE:
            {
                trivial_instruction = TRUE;

                VirtualRegister *target_vr = VR(word[1].index);

                target_vr->value_known_in_arithmetic_block.SetDouble(code->data->doubles[word[2].index]);
                target_vr->value_stored = FALSE;
                target_vr->value_needs_flush = !IsIntermediateWrite(arithmetic_block, target_vr, cw_index);

                Operand target(OutOperand(target_vr));

                ES_Value_Internal value(code->data->doubles[word[2].index]);

                if (target.native_register)
                {
                    if (target.native_register->type == NativeRegister::TYPE_INTEGER)
                    {
                        int ivalue = value.GetNumAsInt32();

                        if (target.native_register->current_constant_value.IsInt32() && target.native_register->current_constant_value.GetInt32() == ivalue)
                        {
                            target_operand_index = UINT_MAX;
                            break;
                        }

                        EmitRegisterInt32Assign(ivalue, target.native_register);

                        target.native_register->current_constant_value.SetInt32(ivalue);
                    }
                    else
                    {
                        double dvalue = code->data->doubles[word[2].index];

                        if (target.native_register->current_constant_value.IsDouble() && target.native_register->current_constant_value.GetDouble() == dvalue)
                        {
                            target_operand_index = UINT_MAX;
                            break;
                        }

                        EmitRegisterDoubleAssign(&code->data->doubles[word[2].index], target);

                        target.native_register->current_constant_value.SetDouble(dvalue);
                    }
                }
            }
            break;

        case ESI_LOAD_INT32:
            {
                trivial_instruction = TRUE;

                VirtualRegister *target_vr = VR(word[1].index);

                target_vr->value_known_in_arithmetic_block.SetInt32(word[2].immediate);
                target_vr->value_stored = FALSE;
                target_vr->value_needs_flush = !IsIntermediateWrite(arithmetic_block, target_vr, cw_index);

                Operand target(OutOperand(target_vr));

                if (target.native_register)
                {
                    if (target.native_register->current_constant_value.IsInt32() && target.native_register->current_constant_value.GetInt32() == word[2].immediate)
                    {
                        target_operand_index = UINT_MAX;
                        break;
                    }

                    Operand source(target_vr, &word[2]);

                    EmitRegisterInt32Assign(source, target);

                    target.native_register->current_constant_value.SetInt32(word[2].immediate);
                }
            }
            break;

        case ESI_COPY:
            {
                trivial_instruction = TRUE;

                VirtualRegister *target_vr = VR(word[1].index);
                VirtualRegister *source_vr = VR(word[2].index);

                Operand source(InOperand(arithmetic_block, source_vr));
                Operand target(OutOperand(target_vr));

                if (target.native_register)
                {
                    OP_ASSERT(source.native_register);

                    if (source != target)
                        EmitRegisterCopy(source, target, slow_case);
                }
            }
            break;

        case ESI_LSHIFT_IMM:
        case ESI_RSHIFT_SIGNED_IMM:
        case ESI_RSHIFT_UNSIGNED_IMM:
        case ESI_ADD_IMM:
        case ESI_SUB_IMM:
        case ESI_MUL_IMM:
        case ESI_DIV_IMM:
        case ESI_REM_IMM:
        case ESI_BITAND_IMM:
        case ESI_BITOR_IMM:
        case ESI_BITXOR_IMM:
            immediate_rhs = TRUE;

        case ESI_LSHIFT:
        case ESI_RSHIFT_SIGNED:
        case ESI_RSHIFT_UNSIGNED:
        case ESI_ADD:
        case ESI_SUB:
        case ESI_MUL:
        case ESI_DIV:
        case ESI_REM:
        case ESI_BITAND:
        case ESI_BITOR:
        case ESI_BITXOR:
            {
                VirtualRegister *target_vr = VR(word[1].index);
                VirtualRegister *lhs_vr = VR(word[2].index);
                VirtualRegister *rhs_vr = immediate_rhs ? ImmediateVR() : VR(word[3].index);

                result_as_condition = instruction_index + 1 < end_instruction_index && word[4].instruction == ESI_CONDITION;

                if (ip->target_type == ESTYPE_INT32)
                {
                    Int32ArithmeticType optype;
                    BOOL is_shift = FALSE;

                    switch (word->instruction)
                    {
                    default: OP_ASSERT(FALSE);

                    case ESI_LSHIFT:
                    case ESI_LSHIFT_IMM:          optype = INT32ARITHMETIC_LSHIFT; is_shift = TRUE; break;
                    case ESI_RSHIFT_SIGNED:
                    case ESI_RSHIFT_SIGNED_IMM:   optype = INT32ARITHMETIC_RSHIFT_SIGNED; is_shift = TRUE; break;
                    case ESI_RSHIFT_UNSIGNED:
                    case ESI_RSHIFT_UNSIGNED_IMM: optype = INT32ARITHMETIC_RSHIFT_UNSIGNED; is_shift = TRUE; break;
                    case ESI_ADD:
                    case ESI_ADD_IMM:             optype = INT32ARITHMETIC_ADD; break;
                    case ESI_SUB:
                    case ESI_SUB_IMM:             optype = INT32ARITHMETIC_SUB; break;
                    case ESI_MUL:
                    case ESI_MUL_IMM:             optype = INT32ARITHMETIC_MUL; break;
                    case ESI_DIV:
                    case ESI_DIV_IMM:             optype = INT32ARITHMETIC_DIV; break;
                    case ESI_REM:
                    case ESI_REM_IMM:             optype = INT32ARITHMETIC_REM; break;
                    case ESI_BITAND:
                    case ESI_BITAND_IMM:          optype = INT32ARITHMETIC_AND; break;
                    case ESI_BITOR:
                    case ESI_BITOR_IMM:           optype = INT32ARITHMETIC_OR; break;
                    case ESI_BITXOR:
                    case ESI_BITXOR_IMM:          optype = INT32ARITHMETIC_XOR; break;
                    }

                    Operand lhs(InOperand(arithmetic_block, lhs_vr));
                    Operand rhs(immediate_rhs ? InOperand(rhs_vr, &word[3]) : InOperand(arithmetic_block, rhs_vr, FALSE, is_shift));
                    Operand target(OutOperand(target_vr));

                    EmitInt32Arithmetic(optype, target, lhs, rhs, result_as_condition, ip->truncate_to_int32 == 0 ? slow_case : NULL);

                    if (target.native_register)
                        target.native_register->current_constant_value.SetUndefined();
                }
                else
                {
                    DoubleArithmeticType optype;

                    switch (word->instruction)
                    {
                    default: OP_ASSERT(FALSE);
                    case ESI_ADD:
                    case ESI_ADD_IMM: optype = DOUBLEARITHMETIC_ADD; break;
                    case ESI_SUB:
                    case ESI_SUB_IMM: optype = DOUBLEARITHMETIC_SUB; break;
                    case ESI_MUL:
                    case ESI_MUL_IMM: optype = DOUBLEARITHMETIC_MUL; break;
                    case ESI_DIV:
                    case ESI_DIV_IMM: optype = DOUBLEARITHMETIC_DIV; break;
                    }

                    Operand lhs(InOperand(arithmetic_block, lhs_vr));
                    Operand rhs(immediate_rhs ? InOperand(rhs_vr, &word[3]) : InOperand(arithmetic_block, rhs_vr));
                    Operand target(OutOperand(target_vr));

                    EmitDoubleArithmetic(optype, target, lhs, rhs, result_as_condition);

                    if (target.native_register)
                        target.native_register->current_constant_value.SetUndefined();
                }

            }
            break;

        case ESI_EQ_IMM:
        case ESI_NEQ_IMM:
        case ESI_EQ_STRICT_IMM:
        case ESI_NEQ_STRICT_IMM:
        case ESI_LT_IMM:
        case ESI_LTE_IMM:
        case ESI_GT_IMM:
        case ESI_GTE_IMM:
            immediate_rhs = TRUE;

        case ESI_EQ:
        case ESI_NEQ:
        case ESI_EQ_STRICT:
        case ESI_NEQ_STRICT:
        case ESI_LT:
        case ESI_LTE:
        case ESI_GT:
        case ESI_GTE:
            {
                VirtualRegister *lhs_vr = VR(word[1].index);
                VirtualRegister *rhs_vr = immediate_rhs ? ImmediateVR() : VR(word[2].index);

                ES_Value_Internal immediate;

                Operand *value_target = NULL, actual_value_target;
                ES_CodeGenerator::JumpTarget *true_target = NULL, *false_target = NULL;

                if (GetConditionalTargets(cw_index + 3, actual_value_target, true_target, false_target))
                    if (actual_value_target.IsValid())
                        value_target = &actual_value_target;

                Operand lhs(InOperand(arithmetic_block, lhs_vr));
                Operand rhs(immediate_rhs ? InOperand(rhs_vr, &word[2]) : InOperand(arithmetic_block, rhs_vr));

                if (lhs.IsImmediate() && rhs.IsImmediate())
                {
                    double lhs_value = lhs.value ? *lhs.value : lhs.codeword->immediate;
                    double rhs_value = rhs.value ? *rhs.value : rhs.codeword->immediate;

                    BOOL condition;

                    if (op_isnan(lhs_value) || op_isnan(rhs_value))
                        condition = FALSE;
                    else
                        switch (word->instruction)
                        {
                        default:
                        case ESI_EQ:
                        case ESI_EQ_IMM:
                        case ESI_EQ_STRICT:
                        case ESI_EQ_STRICT_IMM:  condition = lhs_value == rhs_value; break;
                        case ESI_NEQ:
                        case ESI_NEQ_IMM:
                        case ESI_NEQ_STRICT:
                        case ESI_NEQ_STRICT_IMM: condition = lhs_value != rhs_value; break;
                        case ESI_LT:
                        case ESI_LT_IMM:         condition = lhs_value < rhs_value; break;
                        case ESI_LTE:
                        case ESI_LTE_IMM:        condition = lhs_value <= rhs_value; break;
                        case ESI_GT:
                        case ESI_GT_IMM:         condition = lhs_value > rhs_value; break;
                        case ESI_GTE:
                        case ESI_GTE_IMM:        condition = lhs_value >= rhs_value; break;
                        }

                    if (false_target || true_target)
                    {
                        FlushToVirtualRegisters(arithmetic_block);
                        flushed = TRUE;
                    }

                    if (value_target)
                        EmitStoreConstantBoolean(value_target->virtual_register, condition);
                    if (true_target && condition)
                        cg.Jump(true_target);
                    else if (false_target && !condition)
                        cg.Jump(false_target);
                }
                else
                {
                    RelationalType optype;

                    switch (word->instruction)
                    {
                    default:
                    case ESI_EQ:
                    case ESI_EQ_IMM:
                    case ESI_EQ_STRICT:
                    case ESI_EQ_STRICT_IMM:  optype = RELATIONAL_EQ; break;
                    case ESI_NEQ:
                    case ESI_NEQ_IMM:
                    case ESI_NEQ_STRICT:
                    case ESI_NEQ_STRICT_IMM: optype = RELATIONAL_NEQ; break;
                    case ESI_LT:
                    case ESI_LT_IMM:         optype = RELATIONAL_LT; break;
                    case ESI_LTE:
                    case ESI_LTE_IMM:        optype = RELATIONAL_LTE; break;
                    case ESI_GT:
                    case ESI_GT_IMM:         optype = RELATIONAL_GT; break;
                    case ESI_GTE:
                    case ESI_GTE_IMM:        optype = RELATIONAL_GTE; break;
                    }

                    if (value_target || true_target || false_target)
                    {
                        flushed = true_target || false_target;

                        if (value_target)
                            if (arithmetic_block->last_uncertain_cw_index != UINT_MAX && cw_index < arithmetic_block->last_uncertain_cw_index && !IsSafeWrite(arithmetic_block, value_target->virtual_register, cw_index))
                                AllocateStackFrameStorage(arithmetic_block, value_target->virtual_register, ESTYPE_BOOLEAN);
                            else
                                ConvertStackFrameStorage(arithmetic_block, value_target->virtual_register, ESTYPE_BOOLEAN);

                        if (ip->target_type == ESTYPE_INT32)
                            EmitInt32Relational(optype, lhs, rhs, value_target, true_target, false_target, arithmetic_block);
                        else
                            EmitDoubleRelational(optype, lhs, rhs, value_target, true_target, false_target, arithmetic_block);
                    }
                }
            }
            break;

        case ESI_NEG:
        case ESI_COMPL:
            {
                VirtualRegister *target_vr = VR(word[1].index);
                VirtualRegister *source_vr = VR(word[2].index);

                Operand source(InOperand(arithmetic_block, source_vr));
                Operand target(OutOperand(target_vr));

                result_as_condition = instruction_index + 1 < end_instruction_index && word[3].instruction == ESI_CONDITION;

                if (ip->target_type == ESTYPE_INT32)
                    if (word->instruction == ESI_COMPL)
                        EmitInt32Complement(target, source, result_as_condition);
                    else
                        EmitInt32Negate(target, source, result_as_condition, slow_case);
                else
                    EmitDoubleNegate(target, source, result_as_condition);

                if (target.native_register)
                    target.native_register->current_constant_value.SetUndefined();

            }
            break;

        case ESI_CONDITION:
            {
                ES_CodeGenerator::JumpTarget *true_target = NULL, *false_target = NULL;
                ES_CodeWord *next_instruction = &codewords[instruction_offsets[instruction_index + 1]];

                OP_ASSERT(next_instruction->instruction == ESI_JUMP_IF_TRUE || next_instruction->instruction == ESI_JUMP_IF_FALSE);

                unsigned jump_target_cw_index = cw_index + 3 + next_instruction[1].offset;
                JumpTarget *jump_target = FindJumpTarget(jump_target_cw_index);
                ES_CodeGenerator::JumpTarget *cg_target;

                if (jump_target_cw_index < cw_index)
                    cg_target = jump_target->here;
                else
                {
                    if (!jump_target->forward_jump)
                        jump_target->forward_jump = cg.ForwardJump();

                    cg_target = jump_target->forward_jump;
                }

                if (next_instruction->instruction == ESI_JUMP_IF_TRUE)
                    true_target = cg_target;
                else
                    false_target = cg_target;

                flushed = TRUE;

                EmitConditionalJump(true_target, false_target, FALSE, arithmetic_block);
            }
            break;

        case ESI_INC:
        case ESI_DEC:
            {
                VirtualRegister *target_vr = VR(word[1].index);

                result_as_condition = instruction_index + 1 < end_instruction_index && word[2].instruction == ESI_CONDITION;
                if (ip->target_type == ESTYPE_INT32)
                {
                    const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteAt(cw_index, target_vr->index);

                    BOOL check_overflow = !access->IsTypeKnown() || access->GetType() != ESTYPE_BITS_INT32;

                    Operand target(OutOperand(target_vr));

                    EmitInt32IncOrDec(word->instruction == ESI_INC, target, check_overflow ? slow_case : NULL);
                }
                else
                {
                    Operand target(OutOperand(target_vr));

                    EmitDoubleIncOrDec(word->instruction == ESI_INC, target);
                }
            }
            break;

        case ESI_RETURN_VALUE:
            {
                trivial_instruction = TRUE;

                if (!constructor)
                {
                    VirtualRegister *value_vr = VR(word[1].index);

                    Operand value(InOperand(arithmetic_block, value_vr));
                    Operand target(OutOperand(VR(0)));

                    OP_ASSERT(value.IsNative() || !value.IsVirtual());
                    OP_ASSERT(target.virtual_register != NULL);

                    if (value.native_register)
                        EmitStoreRegisterValue(target.virtual_register, value.native_register);
                    else if (value.value)
                        EmitSetRegisterValue(target.virtual_register, *value.value, TRUE);
                    else
                        EmitSetRegisterValue(target.virtual_register, ES_Value_Internal(value.codeword->immediate), TRUE);
                }

                returned = TRUE;
            }
            break;

        case ESI_RETURN_NO_VALUE:
            {
                trivial_instruction = TRUE;

                if (!constructor)
                    EmitSetRegisterType(VR(0), ESTYPE_UNDEFINED);

                returned = TRUE;
            }
            break;
        }

        if (!trivial_instruction)
            is_trivial = FALSE;

        VirtualRegister *target_vr = NULL;

        if (target_operand_index != UINT_MAX)
        {
            target_vr = VR(word[target_operand_index].index);

            if (RegisterAllocation *allocation = target_vr->current_allocation)
            {
                while (allocation->virtual_next && allocation->virtual_next->start <= cw_index)
                    allocation = allocation->virtual_next;

                if (allocation->start == cw_index && allocation->type == RegisterAllocation::TYPE_COPY)
                    if (allocation->virtual_previous && allocation->virtual_previous->end == cw_index)
                    {
                        EmitRegisterCopy(allocation->virtual_previous->native_register, allocation->native_register, slow_case);
                        allocation->native_register->current_constant_value.SetUndefined();

                        //if (allocation->virtual_previous->type == RegisterAllocation::TYPE_WRITE && !allocation->virtual_previous->intermediate_write)
                        //    allocation->type = RegisterAllocation::TYPE_WRITE;
                    }
            }

            if (word->instruction != ESI_LOAD_DOUBLE && word->instruction != ESI_LOAD_INT32)
                target_vr->value_stored = TRUE;
        }

        for (unsigned index = 0; index < virtual_registers_count; ++index)
        {
            VirtualRegister *vr = VR(index);

            if (RegisterAllocation *allocation = vr->current_allocation)
            {
                while (allocation->virtual_next && allocation->virtual_next->end <= cw_index)
                    allocation = allocation->virtual_next;

                if (allocation->type != RegisterAllocation::TYPE_WRITE)
                    continue;

                if (allocation->intermediate_write)
                {
                    /* An intermediate write: the value is local to the
                       arithmetic block.  But we need to check whether it is
                       used later in the arithmetic block, after the allocation
                       ends, in which case we still need to write the value into
                       the virtual register, or otherwise keep it for future
                       uses. */

                    const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteAt(allocation->start, vr->index);
                    const ES_CodeOptimizationData::RegisterAccess *read = access->GetNextRead();

                    while (read)
                    {
                        if (read->cw_index > allocation->end)
                            break;

                        read = read->GetNextRead();
                    }

                    if (!read)
                        continue;

                    /* Check if this allocation (including any chain of
                       TYPE_COPY allocations following it) covers all the reads
                       of the value; if so, no need to flush, if not, we still
                       need to flush. */
                    RegisterAllocation *previous_allocation = allocation, *next_allocation = allocation->virtual_next;

                    while (next_allocation && next_allocation->start == previous_allocation->end && next_allocation->type == RegisterAllocation::TYPE_COPY)
                    {
                        previous_allocation = next_allocation;
                        next_allocation = next_allocation->virtual_next;
                    }

                    if (access->GetLastReadIndex() <= previous_allocation->end)
                        continue;
                }

                if (allocation->start >= arithmetic_block->start_cw_index && allocation->start <= cw_index && cw_index <= allocation->end && (arithmetic_block->last_uncertain_cw_index == UINT_MAX || allocation->end <= arithmetic_block->last_uncertain_cw_index || cw_index >= arithmetic_block->last_uncertain_cw_index) /* && (!allocation->virtual_next || allocation->virtual_next->start != cw_index) */)
                {
                    NativeRegister *nr = allocation->native_register;
                    ES_ValueType type;

                    if (!vr->value_stored)
                        type = vr->value_known_in_arithmetic_block.Type();
                    else
                        type = nr->type == NativeRegister::TYPE_INTEGER ? ESTYPE_INT32 : ESTYPE_DOUBLE;

                    if (vr->stack_frame_offset != INT_MAX)
                        FreeStackFrameStorage(vr);

                    if (arithmetic_block->last_uncertain_cw_index != UINT_MAX && cw_index < arithmetic_block->last_uncertain_cw_index && !IsSafeWrite(arithmetic_block, vr, allocation->start))
                        AllocateStackFrameStorage(arithmetic_block, vr, type);

                    if (!vr->value_stored)
                    {
                        EmitSetRegisterValue(vr, vr->value_known_in_arithmetic_block, vr->stack_frame_offset == INT_MAX, restore_condition);

                        vr->value_stored = TRUE;
                        vr->value_needs_flush = FALSE;
                    }
                    else
                    {
                        /* Saving the register value may upset flags set by the condition. */
                        if (result_as_condition && IsComplexStore(vr, nr))
                        {
                            EmitSaveCondition();
                            result_as_condition = FALSE;
                            restore_condition = TRUE;
                        }

                        EmitStoreRegisterValue(vr, nr, vr->stack_frame_offset == INT_MAX, restore_condition);
                    }

                    allocation->type = RegisterAllocation::TYPE_READ;
                }
            }
        }
        if (restore_condition)
            EmitRestoreCondition();
    }

    if (!flushed && (!returned || code->CanHaveVariableObject()))
        FlushToVirtualRegisters(arithmetic_block);

    if (arithmetic_block->slow_case)
    {
        cg.SetOutOfOrderContinuationPoint(arithmetic_block->slow_case);
        is_trivial = FALSE;
    }

    if (returned)
        EmitReturn();

    for (unsigned index = 0; index < virtual_registers_count; ++index)
    {
        VirtualRegister *vr = VR(index);

        if (vr->stack_frame_offset != INT_MAX)
            FreeStackFrameStorage(vr);
    }

    current_slow_case = NULL;
}

OP_STATUS
ES_Native::Finish(ES_NativeCodeBlock *ncblock)
{
    void *constant_reg = NULL;

#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
    RETURN_IF_ERROR(cg.CreateDataSection(ncblock));
    constant_reg = ncblock->GetDataSectionRegister();
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION

    const OpExecMemory *block = cg.GetCode(context->heap->GetExecutableMemory(), constant_reg);

    ncblock->SetExecMemory(block);

    SwitchTable *switch_table = first_switch_table;
    while (switch_table)
    {
        if (void **jump_table = reinterpret_cast<void **>(cg.GetBlobStorage(block->address, switch_table->blob)))
        {
            unsigned ncases = switch_table->data1->maximum - switch_table->data1->minimum + 1;

            for (unsigned index = 0; index < ncases; ++index)
                jump_table[index] = switch_table->targets[index]->GetAddress(block->address);
        }

        switch_table = switch_table->next;
    }

    cg.Finalize(context->heap->GetExecutableMemory(), block);

    return OpStatus::OK;
}
#endif // ES_NATIVE_SUPPORT
