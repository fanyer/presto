/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/vm/es_register_blocks.h"
#include "modules/ecmascript/carakan/src/vm/es_execution_context.h"

#ifdef ADS12 // Work round Brew linker issues:
#include "modules/ecmascript/carakan/src/vm/es_suspended_call_inlines.h"
template class ES_Suspended_DELETEA<ES_Value_Internal>;
# ifdef ES_NATIVE_SUPPORT
template class ES_Suspended_DELETEA<ES_RegisterBlocksAdjustment>;
# endif // ES_NATIVE_SUPPORT
#endif // ADS12

ES_RegisterBlocks::ES_RegisterBlocks(ES_Execution_Context *context)
    : ES_BlockHead<ES_Value_Internal>(256, TRUE)
#ifdef ES_NATIVE_SUPPORT
    , context(context)
    , adjustments(16)
    , current_depth(0)
#endif // ES_NATIVE_SUPPORT
{
    extra_capacity = 8;
}

#ifdef ES_NATIVE_SUPPORT

OP_STATUS
ES_RegisterBlocks::Initialize(ES_Execution_Context *context)
{
    RETURN_IF_ERROR(adjustments.Initialize(context));

    return ES_BlockHead<ES_Value_Internal>::Initialize(context);
}

OP_STATUS
ES_RegisterBlocks::Allocate(ES_Execution_Context *context, ES_Value_Internal *&item, unsigned size)
{
    ++current_depth;

    RETURN_IF_ERROR(AdjustUsed());

    return ES_BlockHead<ES_Value_Internal>::Allocate(context, item, size);
}

OP_STATUS
ES_RegisterBlocks::Allocate(ES_Execution_Context *context, BOOL &first_in_block, ES_Value_Internal *&item, unsigned size, unsigned overlap, unsigned copy)
{
    ++current_depth;

    RETURN_IF_ERROR(AdjustUsed());

    return ES_BlockHead<ES_Value_Internal>::Allocate(context, first_in_block, item, size, overlap, copy);
}

void
ES_RegisterBlocks::Free(unsigned size)
{
    ES_BlockHead<ES_Value_Internal>::Free(size);

    ReadjustUsed();

    --current_depth;

#ifdef ES_NATIVE_SUPPORT
    context->reg_limit = Limit();
#endif // ES_NATIVE_SUPPORT
}

void
ES_RegisterBlocks::Free(unsigned size, unsigned overlap, unsigned copy, BOOL first_in_block)
{
    ES_BlockHead<ES_Value_Internal>::Free(size, overlap, copy, first_in_block);

    ReadjustUsed();

    --current_depth;

#ifdef ES_NATIVE_SUPPORT
    context->reg_limit = Limit();
#endif // ES_NATIVE_SUPPORT
}

OP_BOOLEAN
ES_RegisterBlocks::AdjustUsed()
{
    if (context->need_adjust_register_usage)
    {
        ES_Block<ES_Value_Internal> *block = GetLastUsedBlock();

        ES_Value_Internal *reg = context->Registers();
        ES_Code *code = context->Code();

        if (reg >= block->Storage() && reg < block->Storage() + block->Capacity())
        {
            ES_RegisterBlocksAdjustment *adjustment;

            RETURN_IF_ERROR(adjustments.Allocate(context, adjustment));

            unsigned expected_used = (reg + code->data->register_frame_size) - block->Storage();
            unsigned original_used = block->Used();

            adjustment->allocation_depth = current_depth;
            adjustment->new_used = expected_used;
            adjustment->original_used = block->Used();
            adjustment->block = block;

            block->SetUsed(expected_used);
            total_used += (expected_used - original_used);

            context->need_adjust_register_usage = FALSE;
            return OpBoolean::IS_TRUE;
        }
    }

    return OpBoolean::IS_FALSE;
}

void
ES_RegisterBlocks::ReadjustUsed()
{
    ES_RegisterBlocksAdjustment *adjustment;

    adjustments.LastItem(adjustment);

    if (adjustment && adjustment->allocation_depth == current_depth)
    {
        OP_ASSERT(adjustment->block == GetLastUsedBlock() && adjustment->new_used == adjustment->block->Used());

        adjustment->block->SetUsed(adjustment->original_used);
        total_used -= (adjustment->new_used - adjustment->original_used);

        adjustments.Free(1);

        context->need_adjust_register_usage = TRUE;
    }
}

#endif // ES_NATIVE_SUPPORT
