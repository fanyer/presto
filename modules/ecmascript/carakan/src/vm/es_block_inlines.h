/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * @author Daniel Spang
 */

#ifndef ES_BLOCK_INLINES_H
#define ES_BLOCK_INLINES_H

template <class T>
ES_BlockHead<T>::ES_BlockHead(unsigned initial_capacity, BOOL prefill/* = FALSE*/, T prototype_item/* = T()*/)
    : last_used_block(NULL), next_capacity(initial_capacity), extra_capacity(0), prefill(prefill), prototype_item(prototype_item), total_used(0)
{
}

template <class T>
OP_STATUS
ES_BlockHead<T>::Initialize(ES_Execution_Context *context)
{
    RETURN_IF_ERROR(AllocateBlock(context, next_capacity));
    Reset();
    return OpStatus::OK;
}

template <class T>
void
ES_BlockHead<T>::Reset()
{
    ES_Block<T> *block = static_cast<ES_Block<T> *>(Last());
    if (block)
        do
            block->Reset();
        while ((block = static_cast<ES_Block<T> *>(block->Pred())) != NULL);

    last_used_block = static_cast<ES_Block<T> *>(First());
    total_used = 0;
}

template <class T>
inline OP_STATUS
ES_BlockHead<T>::Allocate(ES_Execution_Context *context, T *&item, unsigned size/* = 1*/)
{
    total_used += size;

    if (last_used_block->Available() >= size)
        last_used_block->Allocate(item, size);
    else
        RETURN_IF_ERROR(AllocateInNextBlock(context, item, size, 0, 0));

#ifdef ES_DEBUG_BLOCK_USAGE
    DebugItemInfo *debug_info = OP_NEW(DebugItemInfo, (size, 0, FALSE));
    debug_info->Into(&debug_stack);
#endif // ES_DEBUG_BLOCK_USAGE

    return OpStatus::OK;
}

template <class T>
inline OP_STATUS
ES_BlockHead<T>::Allocate(ES_Execution_Context *context, BOOL &first_in_block, T *&item, unsigned size, unsigned overlap/* = 0*/, unsigned copy/* = 0*/)
{
    total_used += (size - overlap);

    if (size < overlap || last_used_block->Available() >= size - overlap)
    {
        last_used_block->Allocate(item, size - overlap);
        item -= overlap;
        first_in_block = FALSE;
    }
    else
    {
        RETURN_IF_ERROR(AllocateInNextBlock(context, item, size, overlap, copy));
        first_in_block = TRUE;
    }

#ifdef ES_DEBUG_BLOCK_USAGE
    DebugItemInfo *debug_info = OP_NEW(DebugItemInfo, (size, overlap, first_in_block));
    debug_info->Into(&debug_stack);
#endif // ES_DEBUG_BLOCK_USAGE

    return OpStatus::OK;
}

template <class T>
OP_STATUS
ES_BlockHead<T>::AllocateInNextBlock(ES_Execution_Context *context, T *&item, unsigned size, unsigned overlap, unsigned copy)
{
    BOOL allocated_new_block = FALSE;

    if (last_used_block == Last())
    {
        RETURN_IF_ERROR(AllocateBlock(context, size, last_used_block));
        allocated_new_block = TRUE;
    }
    else
    {
        ES_Block<T> *next_block = static_cast<ES_Block<T>*> (last_used_block->Suc());
        OP_ASSERT(next_block->Used() == 0);
        if (next_block->Capacity() < size)
        {
            RETURN_IF_ERROR(AllocateBlock(context, size, last_used_block));
            allocated_new_block = TRUE;
        }
    }

    ES_Block<T> *old_block = last_used_block;
    last_used_block = static_cast<ES_Block<T>*> (last_used_block->Suc());

    OP_ASSERT(last_used_block->Used() == 0);

    if (allocated_new_block && prefill)
        last_used_block->Populate(prototype_item);

    last_used_block->Allocate(item, size);

    if (copy > 0)
        op_memcpy(static_cast<ES_Block<T>*> (last_used_block)->storage, old_block->storage + old_block->used - overlap, copy * sizeof(T));

    return OpStatus::OK;
}

template <class T>
void
ES_BlockHead<T>::Free(unsigned size/* = 1*/)
{
#ifdef ES_DEBUG_BLOCK_USAGE
    DebugItemInfo *debug_info = static_cast<DebugItemInfo *> (debug_stack.Last());
    OP_ASSERT(debug_info->size == size);
    OP_ASSERT(debug_info->overlap == 0);
    OP_ASSERT(debug_info->first_in_block == FALSE);
    debug_info->Out();
    OP_DELETE(debug_info);
#endif // ES_DEBUG_BLOCK_USAGE

    OP_ASSERT(last_used_block->Used() >= size);

    total_used -= size;

    if (last_used_block->Used() == size && last_used_block->Pred() !=  NULL)
    {
        last_used_block->Free(size);
        last_used_block =  static_cast<ES_Block<T>*> (last_used_block->Pred());
    }
    else
        last_used_block->Free(size);
}

template <class T>
void
ES_BlockHead<T>::Free(unsigned size, unsigned overlap, unsigned copy, BOOL first_in_block/* = TRUE*/)
{
#ifdef ES_DEBUG_BLOCK_USAGE
    DebugItemInfo *debug_info = static_cast<DebugItemInfo *> (debug_stack.Last());
    OP_ASSERT(debug_info->size == size);
    OP_ASSERT(debug_info->overlap == overlap);
    OP_ASSERT(debug_info->first_in_block == first_in_block);
    debug_info->Out();
    OP_DELETE(debug_info);
#endif // ES_DEBUG_BLOCK_USAGE

    OP_ASSERT(last_used_block->Used() >= size);

    total_used -= (size - overlap);

    if (first_in_block)
    {
        OP_ASSERT(last_used_block->Used() == size && last_used_block->Pred() !=  NULL);
        if (copy > 0)
        {
            ES_Block<T> *prev = static_cast<ES_Block<T>*> (last_used_block->Pred());
            op_memcpy(prev->storage + prev->used - overlap, last_used_block->storage, copy * sizeof(T));
        }

        last_used_block->Free(size);
        last_used_block =  static_cast<ES_Block<T>*> (last_used_block->Pred());
    }
    else
        last_used_block->Free(size - overlap);
}

template <class T>
inline void
ES_BlockHead<T>::Shrink(unsigned size)
{
#ifdef ES_DEBUG_BLOCK_USAGE
    DebugItemInfo *debug_info = static_cast<DebugItemInfo *> (debug_stack.Last());
    OP_ASSERT(size < debug_info->size); // You cannot shrink the whole block
    debug_info->size -= size;
#endif // ES_DEBUG_BLOCK_USAGE

    OP_ASSERT(last_used_block->Used() > size);

    last_used_block->Free(size);
}


template <class T>
inline void
ES_BlockHead<T>::LastItem(T *&item)
{
    if (last_used_block && last_used_block->Used() > 0)
        item = last_used_block->storage + last_used_block->used - 1;
    else
        item = NULL;
}

template <class T>
inline void
ES_BlockHead<T>::Trim()
{
    if (last_used_block)
    {
        Link *iter = last_used_block->Suc();
        while(iter)
        {
            iter->Out();
            OP_DELETE(iter);
            iter = last_used_block->Suc();
            next_capacity = next_capacity / grow_ratio;
        }
    }
}

template <class T>
OP_STATUS
ES_BlockHead<T>::AllocateBlock(ES_Execution_Context *context, unsigned min_size, ES_Block<T> *follows/* = NULL*/)
{
    ES_Block<T>* new_block;

    if (min_size < next_capacity)
        min_size = next_capacity;

    if (context->IsUsingStandardStack())
        new_block = OP_NEW((ES_Block<T>), (min_size));
    else
    {
        ES_Suspended_NEW1<ES_Block<T>, unsigned> call(min_size);
        context->SuspendedCall(&call);
        new_block = call.storage;
    }

    if (!new_block)
        return OpStatus::ERR_NO_MEMORY;

    OP_STATUS result = new_block->Initialize(context, extra_capacity);
    if (OpStatus::IsError(result))
    {
        OP_DELETE(new_block);
        return result;
    }

    if (prefill)
        new_block->Populate(prototype_item);

    if (follows)
        new_block->Follow(follows);
    else
        new_block->Into(this);

    next_capacity = next_capacity * grow_ratio;

    return OpStatus::OK;
}

template <class T>
inline OP_STATUS
ES_Block<T>::Initialize(ES_Execution_Context *ctx, unsigned extra_capacity)
{
    context = ctx;
    total_capacity = capacity + extra_capacity;

#ifdef ES_OVERRUN_PROTECTION
    ES_Suspended_NEWA_OverrunProtected<T> call(total_capacity);
#else // ES_OVERRUN_PROTECTION
    ES_Suspended_NEWA<T> call(total_capacity);
#endif // ES_OVERRUN_PROTECTION

    if (context->IsUsingStandardStack())
        call.DoCall(context);
    else
        context->SuspendedCall(&call);

    storage = call.storage;

    if (!storage)
        return OpStatus::ERR_NO_MEMORY;
    return OpStatus::OK;
}

template <class T>
inline void
ES_Block<T>::Reset()
{
    used = 0;
}

template <class T>
inline
ES_Block<T>::~ES_Block()
{
#ifdef ES_OVERRUN_PROTECTION
    ES_Suspended_DELETEA_OverrunProtected<T> call(storage, total_capacity);
#else // ES_OVERRUN_PROTECTION
    ES_Suspended_DELETEA<T> call(storage);
#endif // ES_OVERRUN_PROTECTION

    if (context->IsUsingStandardStack())
        call.DoCall(context);
    else
        context->SuspendedCall(&call);
}

template <class T>
inline void
ES_Block<T>::Allocate(T *&item, unsigned size)
{
    OP_ASSERT(!Pred() || static_cast<ES_Block<T> *>(Pred())->used != 0);

    item = storage + used;
    used += size;
    OP_ASSERT(used <= capacity);
}

template <class T>
inline void
ES_Block<T>::Free(unsigned size)
{
    OP_ASSERT(!Suc() || static_cast<ES_Block<T> *>(Suc())->used == 0);

    OP_ASSERT((int) size <= (int) used);
    used -= size;
}

template <class T>
inline void
ES_Block<T>::Populate(T prototype_item)
{
    for (T *iter = storage, *end = storage + capacity; iter < end; ++iter)
        *iter = prototype_item;
}

template <class T>
ES_BlockItemIterator<T>::ES_BlockItemIterator(ES_BlockHead<T> *block_head, BOOL set_to_end/* = TRUE*/)
{
    if (set_to_end)
    {
        current_block = block_head->last_used_block;
        block_head->LastItem(current);
    }
    else
    {
        current_block = static_cast<ES_Block<T> *>(block_head->First());
        while (current_block)
        {
            if(current_block->Used())
            {
                current = current_block->Storage();
                return;
            }
            else
                current_block = static_cast<ES_Block<T> *>(current_block->Suc());
        }
        current = NULL;
    }
}


template <class T>
ES_BlockItemIterator<T>*
ES_BlockItemIterator<T>::Prev()
{
    OP_ASSERT(current >= current_block->Storage() && current < current_block->Storage() + current_block->Used());

    if (current != current_block->Storage())
        --current;
    else
    {
        current_block = static_cast<ES_Block<T> *>(current_block->Pred());

        while (current_block)
        {
            unsigned used =  current_block->Used();
            if (used)
            {
                current = current_block->Storage() + used - 1;
                return this;
            }
            else
                current_block = static_cast<ES_Block<T> *>(current_block->Pred());
        }

        current = NULL;
    }

    return this;
}

#endif // ES_BLOCK_INLINES_H
