/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/compiler/es_native.h"
#include "modules/ecmascript/carakan/src/compiler/es_instruction_data.h"

#ifdef ES_NATIVE_SUPPORT

/* Insert allocation in the lists in its native and virtual registers. */
static void
InsertAllocation(ES_Native::RegisterAllocation *allocation, BOOL make_current)
{
    ES_Native::NativeRegister *nr = allocation->native_register;
    if (!nr->first_allocation)
    {
        nr->first_allocation = nr->last_allocation = allocation;
        allocation->native_previous = allocation->native_next = NULL;
    }
    else
    {
        ES_Native::RegisterAllocation *next = NULL, **ptr = &nr->last_allocation;

        while (*ptr && !((*ptr)->end < allocation->end || (*ptr)->end == allocation->end && ((*ptr)->start < allocation->end || allocation->type == ES_Native::RegisterAllocation::TYPE_WRITE)))
        {
            next = *ptr;
            ptr = &next->native_previous;
        }

        allocation->native_previous = *ptr;
        *ptr = allocation;

        if (allocation->native_previous)
            allocation->native_previous->native_next = allocation;
        else
            nr->first_allocation = allocation;

        allocation->native_next = next;

        if (next)
            next->native_previous = allocation;
        else
            nr->last_allocation = allocation;
    }
    if (make_current)
    {
        OP_ASSERT(!nr->current_allocation || nr->current_allocation->start != UINT_MAX);
        nr->current_allocation = allocation;
    }

    ES_Native::VirtualRegister *vr = allocation->virtual_register;
    if (!vr->first_allocation)
    {
        vr->first_allocation = vr->last_allocation = allocation;
        allocation->virtual_previous = allocation->virtual_next = NULL;
    }
    else
    {
        ES_Native::RegisterAllocation *next = NULL, **ptr = &vr->last_allocation;

        while (*ptr && !((*ptr)->end < allocation->end || (*ptr)->end == allocation->end && ((*ptr)->start < allocation->end || allocation->type == ES_Native::RegisterAllocation::TYPE_WRITE)))
        {
            next = *ptr;
            ptr = &next->virtual_previous;
        }

        allocation->virtual_previous = *ptr;
        *ptr = allocation;

        if (allocation->virtual_previous)
            allocation->virtual_previous->virtual_next = allocation;
        else
            vr->first_allocation = allocation;

        allocation->virtual_next = next;

        if (next)
            next->virtual_previous = allocation;
        else
            vr->last_allocation = allocation;
    }
    if (make_current)
    {
        OP_ASSERT(!vr->current_allocation || vr->current_allocation->start != UINT_MAX);
        vr->current_allocation = allocation;
    }

#ifdef DEBUG_ENABLE_OPASSERT
    OP_ASSERT(nr->first_allocation && !nr->first_allocation->native_previous);
    OP_ASSERT(nr->last_allocation && !nr->last_allocation->native_next);
    allocation = nr->first_allocation;
    while (allocation)
    {
        OP_ASSERT(!allocation->native_next || allocation->native_next->native_previous == allocation);
        allocation = allocation->native_next;
    }

    OP_ASSERT(vr->first_allocation && !vr->first_allocation->virtual_previous);
    OP_ASSERT(vr->last_allocation && !vr->last_allocation->virtual_next);
    allocation = vr->first_allocation;
    while (allocation)
    {
        OP_ASSERT(!allocation->virtual_next || allocation->virtual_next->virtual_previous == allocation);
        allocation = allocation->virtual_next;
    }
#endif // DEBUG_ENABLE_OPASSERT
}

#ifdef ARCHITECTURE_CAP_MEMORY_OPERANDS

static void
DeleteAllocation(ES_Native::RegisterAllocation *allocation)
{
    /* Remove from native register's list: */

    if (allocation->native_next)
        allocation->native_next->native_previous = allocation->native_previous;
    else
        allocation->native_register->last_allocation = allocation->native_previous;

    if (allocation->native_previous)
        allocation->native_previous->native_next = allocation->native_next;
    else
        allocation->native_register->first_allocation = allocation->native_next;

    /* Remove from virtual register's list: */

    if (allocation->virtual_next)
        allocation->virtual_next->virtual_previous = allocation->virtual_previous;
    else
        allocation->virtual_register->last_allocation = allocation->virtual_previous;

    if (allocation->virtual_previous)
        allocation->virtual_previous->virtual_next = allocation->virtual_next;
    else
        allocation->virtual_register->first_allocation = allocation->virtual_next;
}

#endif // ARCHITECTURE_CAP_MEMORY_OPERANDS

/* Start and insert allocation of 'native_register' for 'virtual_register'.  If
   'make_current' it is also set as the current one.  No checks at all. */
ES_Native::RegisterAllocation *
ES_Native::StartAllocation(RegisterAllocation::Type type, ArithmeticBlock *arithmetic_block, NativeRegister *native_register, VirtualRegister *virtual_register, BOOL make_current)
{
    RegisterAllocation *allocation = OP_NEWGRO_L(RegisterAllocation, (), Arena());

    allocation->arithmetic_block = arithmetic_block;
    allocation->native_register = native_register;
    allocation->virtual_register = virtual_register;
    allocation->start = UINT_MAX;
    allocation->end = cw_index;
    allocation->type = type;
    allocation->initial_argument_value = FALSE;
    allocation->intermediate_write = FALSE;

    InsertAllocation(allocation, make_current);

    return allocation;
}

/* Terminate 'allocation' and set the native and virtual registers' current
   allocation to NULL. */
void
ES_Native::TerminateAllocation(RegisterAllocation *allocation)
{
    if (allocation)
    {
        /* RegisterAllocation::start represents the first use.  If it's set, we
           don't redefine it; this simply means we avoid having a register
           allocation that begins before the allocated register is actually used. */
        if (allocation->start == UINT_MAX)
            allocation->start = allocation->end;

        /* Reset any 'current_allocation' pointers to this allocation.  That will
           make the native and virtual registers seem unallocated. */
        if (allocation == allocation->native_register->current_allocation)
        {
            allocation->native_register->previous_allocation = allocation;
            allocation->native_register->current_allocation = NULL;
        }
        if (allocation == allocation->virtual_register->current_allocation)
            allocation->virtual_register->current_allocation = NULL;

        /*
        if (allocation->intermediate_write && allocation->virtual_next)
            if (allocation->native_next)
                allocation->end = es_minu(allocation->native_next->start, allocation->virtual_next->start);
            else
                allocation->end = allocation->virtual_next->start;
        */
    }
}

static unsigned
AllocationMeaningfulness(ES_CodeOptimizationData *optimization_data, ES_Native::ArithmeticBlock *arithmetic_block, ES_Native::RegisterAllocation *allocation, unsigned cw_index)
{
    ES_Native::VirtualRegister *vr = allocation->virtual_register;
    unsigned check_cw_index = allocation->end;

    if (allocation->start != UINT_MAX)
        check_cw_index = allocation->start;

    const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteReadAt(check_cw_index, vr->index);

    if (access)
    {
        while (access->cw_index < arithmetic_block->start_cw_index)
        {
            ++access;

            if (access->cw_index >= cw_index)
                return 0;
        }

        return cw_index - access->cw_index;
    }
    else
        return 0;
}

/* Allocate a native register to use for 'virtual_register'.  If
   'native_register' is non-NULL, it's the one to allocate. */
ES_Native::RegisterAllocation *
ES_Native::AllocateRegister(RegisterAllocation::Type allocation_type, NativeRegister::Type register_type, ArithmeticBlock *arithmetic_block, VirtualRegister *virtual_register, NativeRegister *native_register)
{
    OP_ASSERT(ARCHITECTURE_HAS_FPU() || register_type == NativeRegister::TYPE_INTEGER);

    if (native_register && register_type != native_register->type)
        native_register = NULL;

    if (!native_register)
#ifdef IA32_X87_SUPPORT
        if (register_type == NativeRegister::TYPE_DOUBLE && IS_FPMODE_X87)
        {
            native_register = NR(integer_registers_count);

            OP_ASSERT(!native_register->current_allocation || native_register->current_allocation->end != cw_index || native_register->current_allocation->type == RegisterAllocation::TYPE_WRITE);
        }
        else
#endif // IA32_X87_SUPPORT
        {
            NativeRegister *candidate = NULL;
            unsigned candidate_meaningfulness = 0;

            unsigned start = (register_type == NativeRegister::TYPE_INTEGER) ? 0 : integer_registers_count;
            unsigned stop = (register_type == NativeRegister::TYPE_INTEGER) ? integer_registers_count : native_registers_count;
            unsigned index;

            for (index = start; index < stop; ++index)
            {
                NativeRegister *nr = NR(index);

                OP_ASSERT(nr->type == register_type);

                if (nr->current_allocation)
                {
                    if (nr->current_allocation->end == cw_index && nr->current_allocation->type != RegisterAllocation::TYPE_WRITE)
                        /* Allocated for current instruction already. */
                        continue;
                    else if (nr->current_allocation->start == cw_index && nr->current_allocation->type != RegisterAllocation::TYPE_WRITE)
                        /* Allocated for current instruction already. */
                        continue;
                }
                else if (nr->previous_allocation && nr->previous_allocation->start == cw_index)
                    continue;

                if (candidate && nr->current_allocation)
                    if (candidate_meaningfulness <= AllocationMeaningfulness(optimization_data, arithmetic_block, nr->current_allocation, cw_index))
                        continue;

                candidate = nr;

                if (!candidate->current_allocation)
                    break;

                candidate_meaningfulness = AllocationMeaningfulness(optimization_data, arithmetic_block, nr->current_allocation, cw_index);
            }

            native_register = candidate;
        }

    if (native_register->current_allocation)
    {
        OP_ASSERT(native_register->current_allocation->start != cw_index || native_register->current_allocation->type == RegisterAllocation::TYPE_WRITE);
        TerminateAllocation(native_register->current_allocation);
    }

#ifdef _DEBUG__
    if (RegisterAllocation *current_allocation = native_register->first_allocation)
    {
        do
            OP_ASSERT(current_allocation->start > cw_index || current_allocation->end < cw_index || current_allocation->start == cw_index && current_allocation->type == RegisterAllocation::TYPE_WRITE);
        while ((current_allocation = current_allocation->native_next) != NULL);
    }
#endif // _DEBUG

    RegisterAllocation *allocation = StartAllocation(allocation_type, arithmetic_block, native_register, virtual_register, virtual_register->current_allocation == NULL);

    if (allocation_type == RegisterAllocation::TYPE_SCRATCH || allocation_type == RegisterAllocation::TYPE_WRITE)
    {
        TerminateAllocation(allocation);

        if (allocation->virtual_next && allocation->virtual_next->type == RegisterAllocation::TYPE_READ && allocation->virtual_next->native_register->type == register_type)
            if (!allocation->virtual_next->native_previous)
            {
                allocation->virtual_next->start = allocation->end;
                allocation->virtual_next->type = RegisterAllocation::TYPE_COPY;
            }
    }

    return allocation;
}

ES_Native::RegisterAllocation *
ES_Native::AllocateRegister(RegisterAllocation::Type allocation_type, ES_ValueType value_type, ArithmeticBlock *arithmetic_block, VirtualRegister *virtual_register, NativeRegister *native_register)
{
    return AllocateRegister(allocation_type, value_type == ESTYPE_DOUBLE ? NativeRegister::TYPE_DOUBLE : NativeRegister::TYPE_INTEGER, arithmetic_block, virtual_register, native_register);
}

void
ES_Native::AllocateRegisters()
{
    unsigned *instruction_offsets = code->data->instruction_offsets;

    is_allocating_registers = TRUE;

    current_jump_target = optimization_data->first_jump_target;
    instruction_profiles = OP_NEWGROA_L(ArithmeticInstructionProfile, code->data->instruction_count, Arena());

    for (instruction_index = 0; instruction_index < code->data->instruction_count; ++instruction_index)
    {
        cw_index = instruction_offsets[instruction_index];

        if (current_jump_target && current_jump_target->index == cw_index)
            current_jump_target = current_jump_target->next;

        BOOL uncertain_instruction;
        unsigned failing_index = instruction_index;

        if (StartsArithmeticBlock(instruction_index, failing_index, uncertain_instruction))
        {
            unsigned start_instruction_index = instruction_index, last_uncertain_instruction_index = UINT_MAX;
            do
            {
                if (uncertain_instruction)
                    last_uncertain_instruction_index = instruction_index;
                ++instruction_index;
            }
            while (instruction_index != code->data->instruction_count && ContinuesArithmeticBlock(start_instruction_index, instruction_index, failing_index, uncertain_instruction));
            unsigned end_instruction_index = instruction_index;

            unsigned arithmetic_block_index = arithmetic_block_count++;
            ArithmeticBlock *arithmetic_block = OP_NEWGRO_L(ArithmeticBlock, (arithmetic_block_index), Arena());

            arithmetic_block->start_instruction_index = start_instruction_index;
            arithmetic_block->end_instruction_index = end_instruction_index;
            arithmetic_block->last_uncertain_instruction_index = last_uncertain_instruction_index;
            arithmetic_block->start_cw_index = instruction_offsets[start_instruction_index];
            arithmetic_block->end_cw_index = instruction_offsets[end_instruction_index];
            arithmetic_block->last_uncertain_cw_index = last_uncertain_instruction_index == UINT_MAX ? UINT_MAX : instruction_offsets[last_uncertain_instruction_index];
            arithmetic_block->returns = code->data->codewords[instruction_offsets[end_instruction_index - 1]].instruction == ESI_RETURN_VALUE;
            arithmetic_block->instruction_profiles = instruction_profiles + start_instruction_index;
            arithmetic_block->next = NULL;

            if (last_arithmetic_block)
                last_arithmetic_block = last_arithmetic_block->next = arithmetic_block;
            else
                first_arithmetic_block = last_arithmetic_block = arithmetic_block;

            AllocateRegistersInArithmeticBlock(arithmetic_block);

            instruction_index = end_instruction_index - 1;
        }
        else
            instruction_index = failing_index;
    }

    is_allocating_registers = FALSE;
}

BOOL
ES_Native::IsSafeWrite(ArithmeticBlock *arithmetic_block, VirtualRegister *vr, unsigned cw_index)
{
    /* Returns TRUE if it is safe to write a value into 'vr' at 'cw_index'.  It
       is *not* safe if the previous value in 'vr' came from outside the
       arithmetic block, and was read inside the arithmetic block by the
       instruction at 'cw_index' or any instruction before it in the arithmetic
       block, and the instruction at 'cw_index' or any instruction after it in
       the arithmetic block can fail.  The reason it is not safe is that if any
       subsequent instruction fails, the whole arithmetic block will be executed
       again in bytecode mode, and in that case we must not have trampled any
       values needed during the reexecution. */

    if (cw_index > arithmetic_block->last_uncertain_cw_index)
        /* Beyond last uncertain instruction (last instruction that can fail):
           all writes are safe. */
        return TRUE;

    if (is_light_weight)
    {
        //OP_ASSERT(vr->index >= 2 + static_cast<ES_FunctionCode *>(code)->GetData()->formals_count);
        return TRUE;
    }

    /* Find the latest value in 'vr' that originates from outside the arithmetic
       block. */
    const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteReadAt(arithmetic_block->start_cw_index, vr->index);

    if (access)
    {
        const ES_CodeOptimizationData::RegisterAccess *read;

        while ((read = access->GetNextRead(TRUE)) != NULL)
            if (read->cw_index >= arithmetic_block->start_cw_index)
                return FALSE;
            else
                access = read;
    }

    return TRUE;
}

BOOL
ES_Native::IsIntermediateWrite(ArithmeticBlock *arithmetic_block, VirtualRegister *vr, unsigned cw_index)
{
    /* Returns TRUE if the write to 'vr' at 'cw_index' is followed by another
       write to 'vr' later in the arithmetic block, or if the value written is
       not accessed after the arithmetic block. */

    const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteAt(cw_index, vr->index);
    const ES_CodeOptimizationData::RegisterAccess *next = access->GetNextWrite();

    if (next->cw_index < arithmetic_block->end_cw_index)
        /* Value is overwritten inside the arithmetic block. */
        return TRUE;
    else if (arithmetic_block->returns)
        return TRUE;

    const ES_CodeOptimizationData::RegisterAccess *read = access->GetNextRead(TRUE);

    while (read)
        if (read->cw_index >= arithmetic_block->end_cw_index || read->IsImplicit())
            /* Read outside the arithmetic block. */
            return FALSE;
        else
            read = read->GetNextRead(TRUE);

    if (!vr->is_temporary && code->CanHaveVariableObject())
        /* Appears to be unread, but the possibility of a variable object means
           it can be anyways. */
        return FALSE;

    if (code->type == ES_Code::TYPE_FUNCTION && vr->index < static_cast<ES_FunctionCode *>(code)->GetData()->formals_count + 2)
        /* We're writing an argument so even if it appears to be unread, it can
           be read via the arguments array. */
        return FALSE;

    return TRUE;
}

void
ES_Native::AllocateRegistersInArithmeticBlock(ArithmeticBlock *arithmetic_block)
{
    ES_CodeWord *codewords = code->data->codewords;
    unsigned *instruction_offsets = code->data->instruction_offsets;

    unsigned start_instruction_index = arithmetic_block->start_instruction_index;
    unsigned end_instruction_index = arithmetic_block->end_instruction_index;
    ArithmeticInstructionProfile *instruction_profiles = arithmetic_block->instruction_profiles;

    start_cw_index = instruction_offsets[start_instruction_index];

    for (instruction_index = end_instruction_index; instruction_index-- > start_instruction_index;)
    {
        cw_index = instruction_offsets[instruction_index];

        ES_CodeWord *word = &codewords[cw_index];
        ArithmeticInstructionProfile *ip = &instruction_profiles[instruction_index - start_instruction_index];
        ES_Instruction instruction = word->instruction;
        BOOL immediate_rhs = FALSE;

        switch (instruction)
        {
        case ESI_LOAD_DOUBLE:
            {
                VirtualRegister *target_vr = VR(word[1].index);
                NativeRegister *target_nr = target_vr->CurrentNR();

                Use(target_vr);

                if (target_nr)
                {
                    RegisterAllocation *target_allocation = target_nr->current_allocation;

                    target_allocation->type = RegisterAllocation::TYPE_WRITE;
                    target_allocation->intermediate_write = IsIntermediateWrite(arithmetic_block, target_vr, cw_index);

                    TerminateAllocation(target_allocation);
                }
            }
            break;

        case ESI_LOAD_INT32:
            {
                VirtualRegister *target_vr = VR(word[1].index);
                NativeRegister *target_nr = target_vr->CurrentNR();

                Use(target_vr);

                if (target_nr)
                {
                    RegisterAllocation *target_allocation = target_nr->current_allocation;

                    target_allocation->type = RegisterAllocation::TYPE_WRITE;
                    target_allocation->intermediate_write = IsIntermediateWrite(arithmetic_block, target_vr, cw_index);

                    TerminateAllocation(target_allocation);
                }
            }
            break;

        case ESI_COPY:
            {
                VirtualRegister *target_vr = VR(word[1].index);
                VirtualRegister *source_vr = VR(word[2].index);

                const ES_CodeOptimizationData::RegisterAccess *source_write = optimization_data->FindWriteReadAt(cw_index, source_vr->index);
                const ES_CodeOptimizationData::RegisterAccess *source_next = source_write->GetNextWrite();

                /* If the source value is overwritten inside the arithmetic
                   block, we can't delay the copy.  ("Delay" means execute copy
                   at the end of the arithmetic block.) */
                BOOL can_delay_copy = !source_next || source_next->cw_index >= arithmetic_block->end_cw_index;

                if (can_delay_copy)
                {
                    const ES_CodeOptimizationData::RegisterAccess *target_write = optimization_data->FindWriteAt(cw_index, target_vr->index);
                    const ES_CodeOptimizationData::RegisterAccess *target_read = target_write->GetNextRead();

                    /* If the target value is not read inside the arithmetic
                       block, then delay the copy until the end of the
                       arithmetic block. */
                    if (!target_read || target_read->cw_index >= arithmetic_block->end_cw_index || target_read->IsImplicit())
                    {
                        arithmetic_block->AddDelayedCopy(cw_index, source_vr, target_vr, Arena());

                        RegisterAllocation *source_allocation = source_vr->current_allocation;
                        if (!source_allocation)
                            source_allocation = AllocateRegister(RegisterAllocation::TYPE_READ, ip->target_type, arithmetic_block, source_vr);
                        TerminateAllocation(source_allocation);
                        break;
                    }
                }

                RegisterAllocation *target_allocation = target_vr->current_allocation;

                if (!target_allocation)
                    target_allocation = AllocateRegister(RegisterAllocation::TYPE_WRITE, ip->target_type, arithmetic_block, target_vr);

                target_allocation->type = RegisterAllocation::TYPE_WRITE;
                target_allocation->intermediate_write = IsIntermediateWrite(arithmetic_block, target_vr, cw_index);
                target_allocation->start = cw_index;

                TerminateAllocation(target_allocation);

                if (!source_vr->current_allocation)
                {
                    RegisterAllocation::Type allocation_type;

                    if (ip->lhs_handled == ES_Value_Internal::TypeBits(ip->target_type))
                        allocation_type = RegisterAllocation::TYPE_READ;
                    else
                        allocation_type = RegisterAllocation::TYPE_CONVERT;

                    AllocateRegister(allocation_type, ip->target_type, arithmetic_block, source_vr, target_allocation->native_register);
                }
                else
                    source_vr->current_allocation->start = cw_index;
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
        case ESI_EQ_IMM:
        case ESI_NEQ_IMM:
        case ESI_EQ_STRICT_IMM:
        case ESI_NEQ_STRICT_IMM:
        case ESI_LT_IMM:
        case ESI_LTE_IMM:
        case ESI_GT_IMM:
        case ESI_GTE_IMM:
            immediate_rhs = TRUE;
            /* Every ESI_*_IMM arithmetic instruction's codeword value is one
               more than its non-IMM sibling; thus by subtracting one we get the
               non-IMM sibling, which simplifies the rest of the code here which
               doesn't usually care whether it was an IMM instruction or not. */
            instruction = static_cast<ES_Instruction>(instruction - 1);

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
        case ESI_EQ:
        case ESI_NEQ:
        case ESI_EQ_STRICT:
        case ESI_NEQ_STRICT:
        case ESI_LT:
        case ESI_LTE:
        case ESI_GT:
        case ESI_GTE:
            {
#ifdef ARCHITECTURE_CAP_MEMORY_OPERANDS
#ifndef ARCHITECTURE_CAP_MULTIPLICATION_MEMORY_TARGET_OPERAND
                BOOL is_multiplication = instruction == ESI_MUL;
#endif // ARCHITECTURE_CAP_MULTIPLICATION_MEMORY_TARGET_OPERAND
#endif // ARCHITECTURE_CAP_MEMORY_OPERANDS

                BOOL is_shift = instruction >= ESI_LSHIFT && instruction <= ESI_RSHIFT_UNSIGNED;
                BOOL is_comparison = instruction >= ESI_EQ && instruction <= ESI_GTE;
                BOOL is_bitop = instruction >= ESI_BITAND && instruction <= ESI_BITXOR;

                VirtualRegister *target_vr;
                VirtualRegister *lhs_vr;
                VirtualRegister *rhs_vr;

                if (is_comparison)
                {
                    if (ip->lhs_constant && ip->rhs_constant)
                        break;

                    target_vr = NULL;
                    lhs_vr = VR(word[1].index);
                    rhs_vr = immediate_rhs ? ImmediateVR() : VR(word[2].index);
                }
                else
                {
                    target_vr = VR(word[1].index);
                    lhs_vr = VR(word[2].index);
                    rhs_vr = immediate_rhs ? ImmediateVR() : VR(word[3].index);
                }

                BOOL use_double_op = ip->target_type == ESTYPE_DOUBLE;
#ifdef ARCHITECTURE_CAP_MEMORY_OPERANDS
                BOOL can_overflow = !use_double_op && (/*instruction == ESI_ADD || instruction == ESI_SUB ||*/ instruction == ESI_MUL);
#endif // ARCHITECTURE_CAP_MEMORY_OPERANDS
                BOOL is_commutative = !is_shift && instruction != ESI_SUB && instruction != ESI_DIV;
                BOOL prefer_memory_operand = (use_double_op ? double_registers_count : integer_registers_count) < 2;

                NativeRegister::Type register_type;
                if (!use_double_op)
                    register_type = NativeRegister::TYPE_INTEGER;
                else
                    register_type = NativeRegister::TYPE_DOUBLE;

                /* Shift and bitwise ops are int32 only: */
                OP_ASSERT(!use_double_op || instruction == ESI_ADD || instruction == ESI_SUB || instruction == ESI_MUL || instruction == ESI_DIV || is_comparison);
                /* Division is double only: */
                //OP_ASSERT(use_double_op || instruction != ESI_DIV);

                /* Set up lhs_is_constant and rhs_is_constant to reflect whether
                   we know the value of either operand statically, and whether
                   we can ever possibly use them as immediate operands. */

#if defined ARCHITECTURE_CAP_IMMEDIATE_INTEGER_OPERANDS || defined ARCHITECTURE_CAP_IMMEDIATE_DOUBLE_OPERANDS
                BOOL lhs_is_constant = ip->lhs_constant != 0;
                BOOL rhs_is_constant = ip->rhs_constant != 0;

#ifndef ARCHITECTURE_CAP_IMMEDIATE_INTEGER_OPERANDS
                if (!use_double_op)
                    lhs_is_constant = rhs_is_constant = FALSE;
#endif // ARCHITECTURE_CAP_IMMEDIATE_INTEGER_OPERANDS
#ifdef ARCHITECTURE_IA32
                if (use_double_op && IS_FPMODE_SSE2)
                    lhs_is_constant = rhs_is_constant = FALSE;
#else // ARCHITECTURE_IA32
#ifndef ARCHITECTURE_CAP_IMMEDIATE_DOUBLE_OPERANDS
                if (use_double_op)
                    lhs_is_constant = rhs_is_constant = FALSE;
#endif // ARCHITECTURE_CAP_IMMEDIATE_DOUBLE_OPERANDS
#endif // ARCHITECTURE_IA32
#else // ARCHITECTURE_CAP_IMMEDIATE_INTEGER_OPERANDS || ARCHITECTURE_CAP_IMMEDIATE_DOUBLE_OPERANDS
                const BOOL lhs_is_constant = FALSE;
                const BOOL rhs_is_constant = FALSE;
#endif // ARCHITECTURE_CAP_IMMEDIATE_INTEGER_OPERANDS || ARCHITECTURE_CAP_IMMEDIATE_DOUBLE_OPERANDS

                RegisterAllocation::Type lhs_allocation_type = ip->lhs_handled == ES_Value_Internal::TypeBits(ip->target_type) || is_bitop && ip->lhs_handled == ESTYPE_BITS_BOOLEAN && ES_Value_Internal::TypeBits(ip->target_type) == ESTYPE_BITS_INT32 ? RegisterAllocation::TYPE_READ : RegisterAllocation::TYPE_CONVERT;
                RegisterAllocation::Type rhs_allocation_type = ip->rhs_handled == ES_Value_Internal::TypeBits(ip->target_type) || is_bitop && ip->rhs_handled == ESTYPE_BITS_BOOLEAN && ES_Value_Internal::TypeBits(ip->target_type) == ESTYPE_BITS_INT32 ? RegisterAllocation::TYPE_READ : RegisterAllocation::TYPE_CONVERT;

                NativeRegister *lhs_nr = lhs_vr->CurrentNR();
                NativeRegister *rhs_nr = rhs_vr->CurrentNR();
                NativeRegister *target_nr = NULL;

#ifdef ARCHITECTURE_IA32
                if (!use_double_op && (instruction == ESI_DIV || instruction == ESI_REM))
                {
                    unsigned target_index;

                    if (instruction == ESI_DIV)
                        target_index = IA32_REGISTER_INDEX_AX;
                    else
                        target_index = IA32_REGISTER_INDEX_DX;

                    TerminateAllocation(NR(IA32_REGISTER_INDEX_DX)->current_allocation);

                    AllocateRegister(RegisterAllocation::TYPE_SCRATCH, NativeRegister::TYPE_INTEGER, arithmetic_block, ScratchAllocationsVR(), NR(IA32_REGISTER_INDEX_AX));

                    RegisterAllocation *target_allocation;

                    target_nr = target_vr->CurrentNR();

                    if (target_nr != NR(target_index))
                    {
                        TerminateAllocation(NR(target_index)->current_allocation);
                        TerminateAllocation(target_vr->current_allocation);

                        target_allocation = AllocateRegister(RegisterAllocation::TYPE_WRITE, NativeRegister::TYPE_INTEGER, arithmetic_block, target_vr, NR(target_index));
                        target_nr = target_allocation->native_register;
                    }
                    else
                    {
                        target_allocation = target_nr->current_allocation;
                        target_allocation->start = cw_index;
                        target_allocation->type = RegisterAllocation::TYPE_WRITE;
                    }

                    target_allocation->intermediate_write = IsIntermediateWrite(arithmetic_block, target_vr, cw_index);

                    TerminateAllocation(target_nr->current_allocation);

                    if (lhs_vr->current_allocation)
                        TerminateAllocation(lhs_vr->current_allocation);

                    AllocateRegister(lhs_allocation_type, NativeRegister::TYPE_INTEGER, arithmetic_block, lhs_vr, NR(IA32_REGISTER_INDEX_AX));
                    AllocateRegister(RegisterAllocation::TYPE_SCRATCH, NativeRegister::TYPE_INTEGER, arithmetic_block, ScratchAllocationsVR(), NR(IA32_REGISTER_INDEX_DX));

                    if (lhs_vr != rhs_vr)
                        if (rhs_is_constant)
                            AllocateRegister(rhs_allocation_type, NativeRegister::TYPE_INTEGER, arithmetic_block, rhs_vr);
                        else if (rhs_vr->current_allocation && rhs_vr->current_allocation->native_register->type != NativeRegister::TYPE_INTEGER)
                            TerminateAllocation(rhs_vr->current_allocation);
                        else
                            Use(rhs_vr);

                    if (immediate_rhs)
                        TerminateAllocation(rhs_vr->current_allocation);

                    break;
                }
#endif // ARCHITECTURE_IA32

                RegisterAllocation *target_allocation = target_vr ? target_vr->current_allocation : NULL;

                /* Make sure registers currenly allocated for the left hand side
                   or right hand side are of the correct format, and simply
                   terminate the allocations if they are not. */

                if (lhs_nr && lhs_nr->type != register_type)
                {
                    TerminateAllocation(lhs_nr->current_allocation);
                    lhs_nr = NULL;
                }
                if (rhs_nr && rhs_nr->type != register_type)
                {
                    TerminateAllocation(rhs_nr->current_allocation);
                    rhs_nr = NULL;
                }

                BOOL lhs_done = FALSE;
                BOOL rhs_done = FALSE;

#ifdef ARCHITECTURE_SHIFT_COUNT_REGISTER
                /* The architecture has a special register that must be used for
                   the right hand side (shift count) operand of a shift.  We
                   have to allocate it to hold the right hand side operand,
                   unless the shift count is a constant. */
                if (is_shift)
#ifdef ARCHITECTURE_CAP_IMMEDIATE_SHIFT_COUNT
                    if (!rhs_is_constant)
#endif // ARCHITECTURE_CAP_IMMEDIATE_SHIFT_COUNT
                    {
                        rhs_nr = NR(ARCHITECTURE_SHIFT_COUNT_REGISTER);

                        TerminateAllocation(rhs_vr->current_allocation);
                        TerminateAllocation(rhs_nr->current_allocation);
                        AllocateRegister(rhs_allocation_type, register_type, arithmetic_block, rhs_vr, rhs_nr);

                        rhs_done = TRUE;

                        if (lhs_vr == rhs_vr)
                        {
                            lhs_nr = rhs_nr;
                            lhs_done = TRUE;
                        }
                        else
                            lhs_nr = lhs_vr->CurrentNR();

                        if (target_allocation && target_allocation->native_register == rhs_nr)
                            target_allocation = NULL;
                    }
#endif // ARCHITECTURE_SHIFT_COUNT_REGISTER

                BOOL perform_in_place;

                /* Figure out whether we can and want to perform the operation
                   in-place in the virtual register. */

                if (is_comparison)
                    perform_in_place = FALSE;
                else
                {
                    target_nr = target_allocation ? target_allocation->native_register : NULL;
                    perform_in_place = target_nr == NULL && !target_vr->is_temporary;

                    if (target_nr && target_nr->type != register_type)
                    {
                        if (target_nr->type == NativeRegister::TYPE_INTEGER)
                        {
                            TerminateAllocation(target_allocation);
                            target_nr = NULL;
                        }
                        else
                        {
                            /* A subsequent instruction uses the result of this one,
                               but in the opposite format.  Convert its register
                               allocation from TYPE_CONVERT (since the other
                               instruction knew it wanted the "wrong" format) to
                               TYPE_COPY, and make sure that we produce our result
                               in a register so it has something to copy from. */
                            OP_ASSERT(target_allocation->type == RegisterAllocation::TYPE_CONVERT);
                            target_allocation->start = cw_index;
                            target_allocation->type = RegisterAllocation::TYPE_COPY;
                            TerminateAllocation(target_allocation);
                            target_nr = NULL;
                        }
                        perform_in_place = FALSE;
                    }
                    else if (target_vr == lhs_vr && target_vr == rhs_vr)
                        perform_in_place = FALSE;
                    else if (!IsSafeWrite(arithmetic_block, target_vr, cw_index))
                        /* We can't perform the operation in-place because that
                           would cause the arithmetic block to have side-effects
                           that would interfere with reexecution. */
                        perform_in_place = FALSE;
                    else if (IsIntermediateWrite(arithmetic_block, target_vr, cw_index))
                        /* We don't *need* to write the result to the virtual
                           register at all, so why force it by performing the
                           operation in-place? */
                        perform_in_place = FALSE;
#ifdef ARCHITECTURE_CAP_MEMORY_OPERANDS
#ifndef ARCHITECTURE_CAP_BINARY_EXPLICIT_TARGET_OPERAND
                    /* If the target operand must be the same as one of the
                       source operands, then the target virtual register must be
                       the same as one of the source virtual registers for
                       in-place operation to be possible.  And for shifts,
                       subtraction and division, since they are not commutative,
                       the left hand side's virtual register specificly must be
                       the same as the target virtual for in-place operation to
                       be possible. */
                    else if (!(target_vr == lhs_vr && !lhs_is_constant) && !(target_vr == rhs_vr && !rhs_is_constant && is_commutative))
                        perform_in_place = FALSE;
#endif // ARCHITECTURE_CAP_BINARY_EXPLICIT_TARGET_OPERAND
#ifdef ARCHITECTURE_IA32
                    /* All SSE2 instructions used need an XMM register as the
                       left-hand-side and target operand. */
                    else if (use_double_op)
                        perform_in_place = FALSE;
#endif // ARCHITECTURE_IA32
#ifndef ARCHITECTURE_CAP_MULTIPLICATION_MEMORY_TARGET_OPERAND
                    /* Architecture's multiplication operation doesn't support a
                       memory target operand even though other binary operations
                       do.  */
                    else if (is_multiplication)
                        perform_in_place = FALSE;
#endif // ARCHITECTURE_CAP_MULTIPLICATION_MEMORY_TARGET_OPERAND
                    /* An integer operation that can overflow.  If performed
                       in-place, and one of the source operands is the same as
                       the target operand, that source operand would be trampled
                       by the overflowing operation. */
                    else if (can_overflow && (target_vr == lhs_vr || target_vr == rhs_vr))
                        perform_in_place = FALSE;
                    /* If operands are known to be of the wrong type in their
                       virtual registers, the allocation types will be
                       TYPE_CONVERT or TYPE_COPY and in that case we can't use
                       or modify them in-place. */
                    else if (lhs_allocation_type != RegisterAllocation::TYPE_READ || rhs_allocation_type != RegisterAllocation::TYPE_READ)
                        perform_in_place = FALSE;
#else // ARCHITECTURE_CAP_MEMORY_OPERANDS
                    /* If the architecture doesn't have direct memory operands
                       at all, in-place operations are naturally not an
                       option. */
                    perform_in_place = FALSE;
#endif // ARCHITECTURE_CAP_MEMORY_OPERANDS

#ifndef ARCHITECTURE_CAP_BINARY_EXPLICIT_TARGET_OPERAND
                    if (lhs_done && rhs_done && target_nr)
                    {
                        if (target_nr != lhs_nr && target_nr != rhs_nr)
                            TerminateAllocation(target_nr->current_allocation);

                        target_nr = NULL;
                    }
#endif // ARCHITECTURE_CAP_BINARY_EXPLICIT_TARGET_OPERAND

                    if (target_nr)
                    {
                        target_allocation->start = cw_index;
                        target_allocation->type = RegisterAllocation::TYPE_WRITE;
                    }
                    else if (!perform_in_place)
                    {
                        if (lhs_done && lhs_nr)
                            target_nr = lhs_nr;
                        else if (rhs_done && rhs_nr && is_commutative)
                            target_nr = rhs_nr;

                        if (target_allocation && target_allocation->native_register == target_nr)
                        {
                            target_allocation->start = cw_index;

                            if (target_allocation->type == RegisterAllocation::TYPE_CONVERT)
                                target_allocation->type = RegisterAllocation::TYPE_COPY;
                            else
                                target_allocation->type = RegisterAllocation::TYPE_WRITE;
                        }
                        else
                        {
                            target_allocation = AllocateRegister(RegisterAllocation::TYPE_WRITE, register_type, arithmetic_block, target_vr, target_nr);
                            target_nr = target_allocation->native_register;
                        }
                    }

                    if (target_allocation)
                    {
                        target_allocation->intermediate_write = IsIntermediateWrite(arithmetic_block, target_vr, cw_index);

                        TerminateAllocation(target_allocation);
                    }
                }

                lhs_nr = lhs_vr->CurrentNR();
                rhs_nr = rhs_vr->CurrentNR();

#ifndef ARCHITECTURE_CAP_BINARY_EXPLICIT_TARGET_OPERAND
                /* Architecture doesn't have explicit target operands, meaning
                   the value of one of the source operands must be in the
                   register used for the target operand. */
                if (!perform_in_place && !is_comparison)
                {
                    OP_ASSERT(target_nr != NULL);

                    BOOL lhs_is_target = FALSE;

                    /* We consider ourselves to be done with the right hand side
                       already. */
                    if (rhs_done)
                        lhs_is_target = TRUE;
                    /* Otherwise, unless we consider ourselves to be done the
                       the left hand side already: */
                    else if (!lhs_done)
                        /* Subtraction and division aren't commutative, so the
                           left hand side operand must be the target operand. */
#ifdef ARCHITECTURE_IA32
                        if (is_shift || (instruction == ESI_SUB || instruction == ESI_DIV) && (!use_double_op || IS_FPMODE_SSE2))
#else // ARCHITECTURE_IA32
                        if (is_shift || instruction == ESI_SUB || instruction == ESI_DIV)
#endif // ARCHITECTURE_IA32
                            lhs_is_target = TRUE;
                        /* The right hand side is a constant.  Even if the left
                           hand side happens to be one too, we might as well use
                           the left hand side operand as the target operand. */
                        else if (rhs_is_constant)
                            lhs_is_target = TRUE;
                        /* The right hand side operand has an allocated register
                           already, so we can let it stay there and use the left
                           hand side operand as the target operand. */
                        else if (rhs_nr)
                            lhs_is_target = TRUE;

                    if (lhs_is_target)
                    {
                        TerminateAllocation(lhs_vr->current_allocation);

                        lhs_nr = AllocateRegister(lhs_allocation_type, register_type, arithmetic_block, lhs_vr, target_nr)->native_register;
                        lhs_done = TRUE;

                        if (lhs_vr == rhs_vr)
                        {
                            rhs_nr = lhs_nr;
                            rhs_done = TRUE;
                        }
#ifdef IA32_X87_SUPPORT
                        else if (IS_FPMODE_X87)
                            rhs_done = TRUE;
#endif // IA32_X87_SUPPORT
                    }
                    else
                    {
                        TerminateAllocation(rhs_vr->current_allocation);

                        rhs_nr = AllocateRegister(rhs_allocation_type, register_type, arithmetic_block, rhs_vr, target_nr)->native_register;
                        rhs_done = TRUE;

                        if (lhs_vr == rhs_vr)
                        {
                            lhs_nr = rhs_nr;
                            lhs_done = TRUE;
                        }
#ifdef IA32_X87_SUPPORT
                        else if (IS_FPMODE_X87)
                            lhs_done = TRUE;
#endif // IA32_X87_SUPPORT
                    }
                }
                /* If performing operation in-place, set either the left hand
                   side operand or the right hand side operand as finished.
                   Note that both can't be set as finished if they are the same,
                   one must be in a register. */
                else if (target_vr == lhs_vr)
                    lhs_done = TRUE;
                else if (target_vr == rhs_vr)
                    rhs_done = TRUE;
#endif // ARCHITECTURE_CAP_BINARY_EXPLICIT_TARGET_OPERAND

#ifdef ARCHITECTURE_CAP_MEMORY_OPERANDS
                BOOL use_memory_operand = TRUE;

                if (!target_nr && !is_comparison)
                    use_memory_operand = FALSE;
#else // ARCHITECTURE_CAP_MEMORY_OPERANDS
                BOOL use_memory_operand = FALSE;
#endif // ARCHITECTURE_CAP_MEMORY_OPERANDS

#ifdef ARCHITECTURE_IA32
                BOOL use_immediate_operand = !use_double_op || IS_FPMODE_X87;
#else // ARCHITECTURE_IA32
#if defined ARCHITECTURE_CAP_IMMEDIATE_INTEGER_OPERANDS && defined ARCHITECTURE_CAP_IMMEDIATE_DOUBLE_OPERANDS
                BOOL use_immediate_operand = TRUE;
#elif defined ARCHITECTURE_CAP_IMMEDIATE_INTEGER_OPERANDS
                BOOL use_immediate_operand = !use_double_op;
#elif defined ARCHITECTURE_CAP_IMMEDIATE_DOUBLE_OPERANDS
                BOOL use_immediate_operand = use_double_op;
#else // ARCHITECTURE_CAP_IMMEDIATE_INTEGER_OPERANDS || ARCHITECTURE_CAP_IMMEDIATE_DOUBLE_OPERANDS
                BOOL use_immediate_operand = FALSE;
#endif // ARCHITECTURE_CAP_IMMEDIATE_INTEGER_OPERANDS || ARCHITECTURE_CAP_IMMEDIATE_DOUBLE_OPERANDS
#endif // ARCHITECTURE_IA32

                if (!lhs_done)
                {
                    BOOL allocate_lhs = TRUE;

                    if (lhs_allocation_type == RegisterAllocation::TYPE_READ)
                        if (lhs_is_constant && use_immediate_operand && !(rhs_is_constant && !rhs_nr))
                            allocate_lhs = use_immediate_operand = FALSE;
                        else if ((!lhs_vr->is_temporary || prefer_memory_operand) && use_memory_operand && lhs_allocation_type == RegisterAllocation::TYPE_READ && lhs_vr != rhs_vr)
                            allocate_lhs = use_memory_operand = FALSE;

                    if (allocate_lhs)
                    {
                        if (lhs_vr->current_allocation && lhs_vr->current_allocation->type == lhs_allocation_type)
                            lhs_vr->current_allocation->start = cw_index;
                        else
                            lhs_nr = AllocateRegister(lhs_allocation_type, register_type, arithmetic_block, lhs_vr)->native_register;

                        if (lhs_vr == rhs_vr && !rhs_nr)
                        {
                            OP_ASSERT(!rhs_done);
                            rhs_done = TRUE;
                            rhs_nr = lhs_nr;
                        }
#ifdef IA32_X87_SUPPORT
                        else if (IS_FPMODE_X87)
                            rhs_done = TRUE;
#endif // IA32_X87_SUPPORT
                    }
                    else
                        lhs_nr = NULL;
                }

                if (!rhs_done)
                {
                    BOOL allocate_rhs = TRUE;

                    if (rhs_is_constant && use_immediate_operand)
                        allocate_rhs = use_immediate_operand = FALSE;
                    else if ((!rhs_vr->is_temporary || prefer_memory_operand) && use_memory_operand && rhs_allocation_type == RegisterAllocation::TYPE_READ)
                        allocate_rhs = use_memory_operand = FALSE;

                    if (allocate_rhs)
                        if (rhs_vr->current_allocation && rhs_vr->current_allocation->type == rhs_allocation_type)
                            rhs_vr->current_allocation->start = cw_index;
                        else
                        {
                            rhs_nr = AllocateRegister(rhs_allocation_type, register_type, arithmetic_block, rhs_vr)->native_register;

                            if (lhs_vr == rhs_vr)
                                lhs_nr = rhs_nr;
                        }
                    else
                        rhs_nr = NULL;
                }

                OP_ASSERT((lhs_nr == rhs_nr) == (lhs_vr == rhs_vr) || !lhs_nr && !rhs_nr);

                if (immediate_rhs)
                    TerminateAllocation(rhs_vr->current_allocation);
            }
            break;

        case ESI_COMPL:
        case ESI_NEG:
        case ESI_INC:
        case ESI_DEC:
            {
                VirtualRegister *target_vr = VR(word[1].index);
                VirtualRegister *source_vr;

                if (instruction == ESI_COMPL || instruction == ESI_NEG)
                    source_vr = VR(word[2].index);
                else
                    source_vr = target_vr;

                NativeRegister *target_nr = target_vr->CurrentNR();

#ifdef ARCHITECTURE_CAP_MEMORY_OPERANDS
                if (!target_nr && source_vr == target_vr && ip->target_type == ESTYPE_INT32 && is_light_weight)
                {
                    const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteReadAt(cw_index, source_vr->index);

                    if (access->cw_index < arithmetic_block->start_cw_index)
                        break;
                }
#endif // ARCHITECTURE_CAP_MEMORY_OPERANDS

                unsigned prefered_handled;
                NativeRegister::Type register_type;

                if (ip->target_type == ESTYPE_INT32)
                    prefered_handled = ESTYPE_BITS_INT32, register_type = NativeRegister::TYPE_INTEGER;
                else
                    prefered_handled = ESTYPE_BITS_DOUBLE, register_type = NativeRegister::TYPE_DOUBLE;

                if (target_nr && target_nr->type != register_type && target_nr->type == NativeRegister::TYPE_INTEGER)
                {
                    TerminateAllocation(target_nr->current_allocation);
                    target_nr = NULL;
                }

                Use(target_vr);

                if (target_nr && target_nr->type != register_type)
                {
                    OP_ASSERT(target_nr->current_allocation->type == RegisterAllocation::TYPE_CONVERT);
                    target_nr->current_allocation->start = cw_index;
                    target_nr->current_allocation->type = RegisterAllocation::TYPE_COPY;
                    TerminateAllocation(target_nr->current_allocation);
                    target_nr = NULL;
                }

                RegisterAllocation *target_allocation;

                if (!target_nr)
                {
                    target_allocation = AllocateRegister(RegisterAllocation::TYPE_WRITE, register_type, arithmetic_block, target_vr);
                    target_nr = target_allocation->native_register;
                }
                else
                {
                    target_allocation = target_nr->current_allocation;
                    target_allocation->type = RegisterAllocation::TYPE_WRITE;
                }

                target_allocation->intermediate_write = IsIntermediateWrite(arithmetic_block, target_vr, cw_index);

                TerminateAllocation(target_allocation);

                RegisterAllocation::Type allocation_type;
                if (ip->lhs_handled == prefered_handled)
                    allocation_type = RegisterAllocation::TYPE_READ;
                else
                {
                    if (ip->target_type == ESTYPE_INT32)
                    {
                        OP_ASSERT(instruction == ESI_COMPL);
                        AllocateRegister(RegisterAllocation::TYPE_SCRATCH, NativeRegister::TYPE_DOUBLE, arithmetic_block, ScratchAllocationsVR());
                        AllocateRegister(RegisterAllocation::TYPE_SCRATCH, NativeRegister::TYPE_DOUBLE, arithmetic_block, ScratchAllocationsVR());
                    }
                    allocation_type = RegisterAllocation::TYPE_CONVERT;
                }

                TerminateAllocation(source_vr->current_allocation);
                AllocateRegister(allocation_type, register_type, arithmetic_block, source_vr, target_nr);
            }
            break;

        case ESI_STORE_BOOLEAN:
            {
                VirtualRegister *vr = VR(word[1].index);

                TerminateAllocation(vr->current_allocation);
            }
            break;

        case ESI_RETURN_VALUE:
            {
                VirtualRegister *vr = VR(word[1].index);

                if (!ip->lhs_constant)
                    AllocateRegister(RegisterAllocation::TYPE_READ, ip->target_type == ESTYPE_INT32 ? NativeRegister::TYPE_INTEGER : NativeRegister::TYPE_DOUBLE, arithmetic_block, vr);
            }
            break;

        case ESI_CONDITION:
        case ESI_JUMP_IF_TRUE:
        case ESI_JUMP_IF_FALSE:
            break;

        default:
            OP_ASSERT(!"Unhandled instruction!");
        }
    }

    for (unsigned index = 0; index < virtual_registers_count; ++index)
    {
        VirtualRegister *vr = VR(index);

        TerminateAllocation(vr->current_allocation);

        RegisterAllocation *allocation = vr->last_allocation;

        /* Extend all "final" allocations that need to be written down to
           virtual registers at the end of the arithmetic block to the actual
           end of the arithmetic block. */
        if (allocation && allocation->start >= arithmetic_block->start_cw_index && !allocation->native_next)
            if (allocation->type == RegisterAllocation::TYPE_WRITE || allocation->type == RegisterAllocation::TYPE_COPY)
                if (!allocation->intermediate_write)
                {
                    const ES_CodeOptimizationData::RegisterAccess *access = optimization_data->FindWriteAt(allocation->start, vr->index);
                    const ES_CodeOptimizationData::RegisterAccess *next = access->GetNextWrite();

                    if (next && next->cw_index < arithmetic_block->end_cw_index)
                        /* Can't extend if there's a write to this register
                           later in the arithmetic block that just doesn't have
                           a register allocation. */
                        continue;

                    allocation->end = arithmetic_block->end_cw_index;
                }
    }

#ifdef ARCHITECTURE_CAP_MEMORY_OPERANDS
    for (unsigned index = 0; index < virtual_registers_count; ++index)
    {
        VirtualRegister *vr = VR(index);

        if (RegisterAllocation *allocation = vr->last_allocation)
            while (allocation && allocation->start >= arithmetic_block->start_cw_index)
            {
                if (allocation->start == allocation->end && allocation->type == RegisterAllocation::TYPE_READ)
                    switch (codewords[allocation->start].instruction)
                    {
                    case ESI_MUL:
                    case ESI_DIV:
                    case ESI_REM:
                        if (allocation->native_register->type == NativeRegister::TYPE_INTEGER)
                            break;

                    case ESI_ADD:
                    case ESI_SUB:
                    case ESI_BITAND:
                    case ESI_BITOR:
                    case ESI_BITXOR:
                        VirtualRegister *target_vr = VR(codewords[allocation->start + 1].index);

                        if (allocation->native_next && allocation->native_next->start == allocation->end && allocation->native_next->virtual_register == target_vr)
                            /* Operation is performed in-place in this native
                               register: the read is required to prime the
                               native register for the in-place operation. */
                            break;

                        RegisterAllocation *target_allocation = target_vr->last_allocation;
                        while (target_allocation)
                            if (target_allocation->type == RegisterAllocation::TYPE_WRITE && target_allocation->start == allocation->start)
                                break;
                            else if (target_allocation->start < allocation->start)
                                target_allocation = NULL;
                            else
                                target_allocation = target_allocation->virtual_previous;

                        if (!target_allocation)
                            break;

                        RegisterAllocation *previous = allocation->virtual_previous;
                        DeleteAllocation(allocation);
                        allocation = previous;

                        continue;
                    }

                allocation = allocation->virtual_previous;
            }
    }
#endif // ARCHITECTURE_CAP_MEMORY_OPERANDS

    for (unsigned index = 0; index < native_registers_count; ++index)
        if (NR(index)->current_allocation)
            TerminateAllocation(NR(index)->current_allocation);

#ifdef ES_DUMP_REGISTER_ALLOCATIONS
#ifdef NATIVE_DISASSEMBLER_SUPPORT
    if (cg.DisassembleEnabled())
    {
        printf("--------------------------------------------------------------------------------\nRegister allocations for block [%u -> %u : %u]:\n", arithmetic_block->start_cw_index, arithmetic_block->end_cw_index, arithmetic_block->last_uncertain_cw_index);
        for (unsigned index = 0; index < native_registers_count; ++index)
            if (NR(index)->first_allocation && NR(index)->first_allocation->type != RegisterAllocation::TYPE_SPECIAL)
            {
                RegisterAllocation *allocation = NR(index)->first_allocation;

                while (allocation && allocation->start < arithmetic_block->start_cw_index)
                    allocation = allocation->native_next;

                if (allocation)
                {
                    if (NR(index)->type == NativeRegister::TYPE_INTEGER)
                        printf("  %s: ", ARCHITECTURE_INTEGER_REGISTER_NAMES[NR(index)->register_code]);
                    else
                        printf("  %s: ", ARCHITECTURE_DOUBLE_REGISTER_NAMES[NR(index)->register_code]);

                    BOOL first = TRUE;

                    for (; allocation; allocation = allocation->native_next)
                    {
                        if (first)
                            first = FALSE;
                        else
                            printf(", ");

                        if (allocation->type == RegisterAllocation::TYPE_SCRATCH)
                            printf("scratch:%u", allocation->start);
                        else
                        {
                            const char *ts;
                            if (allocation->type == RegisterAllocation::TYPE_READ)
                                ts = "R:";
                            else if (allocation->type == RegisterAllocation::TYPE_WRITE)
                                ts = "W:";
                            else if (allocation->type == RegisterAllocation::TYPE_COPY)
                                ts = "CPY:";
                            else if (allocation->type == RegisterAllocation::TYPE_CONVERT)
                                ts = "CVT:";
                            else
                                ts = "";

                            const char *lparen, *rparen;
                            if (allocation->intermediate_write)
                                lparen = "(", rparen = ")";
                            else
                                lparen = "", rparen = "";

                            if (allocation->start == UINT_MAX)
                                printf("[%sINF -> %u]=%sreg(%u)%s", ts, allocation->end, lparen, allocation->virtual_register->index, rparen);
                            else
                                printf("[%s%u -> %u]=%sreg(%u)%s", ts, allocation->start, allocation->end, lparen, allocation->virtual_register->index, rparen);
                        }
                    }
                    printf("\n");
                }
            }
        printf("--------------------------------------------------------------------------------\n");
    }
#endif // NATIVE_DISASSEMBLER_SUPPORT
#endif // ES_DUMP_REGISTER_ALLOCATIONS
}

#endif // ES_NATIVE_SUPPORT
