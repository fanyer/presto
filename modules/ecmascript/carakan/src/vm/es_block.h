/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * @author Daniel Spang
 */

#ifndef ES_BLOCK_H
#define ES_BLOCK_H

#include "modules/util/simset.h"

template <class T>
class ES_Block;

template <class T>
class ES_BlockItemIterator;

class ES_Execution_Context;

template <class T>
class ES_BlockHead : public AutoDeleteHead
{
    friend class ES_BlockItemIterator<T>;
public:
    ES_BlockHead(unsigned initial_capacity, BOOL prefill = FALSE, T prototype = T());

    OP_STATUS Initialize(ES_Execution_Context *context);

    void Reset();

    OP_STATUS Allocate(ES_Execution_Context *context, T *&item, unsigned size = 1);

    OP_STATUS Allocate(ES_Execution_Context *context, BOOL &first_in_block, T *&item, unsigned size, unsigned overlap, unsigned copy);
    /**< Allocate new items.

       @param[out] first_in_block is TRUE if allocated in new
       underlying block. Pass this to Free to make it behave correctly.

       @param[out] item pointer to the newly allocated block.

       @param[in] size number of items to allocate.

       @param[in] overlap how many items this allocation should overlap with
       previous.

       @param[in] copy how many items are important for the new block. I.e., if
       we have to allocate a new block to satisfy the allocation copy this
       amount of items to the new block. */

    void Free(unsigned size = 1);
    void Free(unsigned size, unsigned overlap, unsigned copy, BOOL first_in_block = FALSE);
    /**< Free the last allocation of items. Size must match the size
       of allocation. */

    void Shrink(unsigned size);
    /**< Shrink the current allocation by size. */

    void Trim();
    /**< Deallocate unused blocks. */

    void LastItem(T *&item);

    T *Limit() { return last_used_block->Storage() + last_used_block->Capacity(); }

    ES_Block<T> *GetLastUsedBlock() { return last_used_block; }

    unsigned TotalUsed() { return total_used; }

    void IncrementTotalUsed(unsigned value) { total_used += value; }
    void DecrementTotalUsed(unsigned value) { total_used -= value; }

protected:
    ES_BlockHead(); // Prevent usage of default constructor.
    OP_STATUS AllocateInNextBlock(ES_Execution_Context *context, T *&item, unsigned size, unsigned overlap, unsigned copy);
    OP_STATUS AllocateBlock(ES_Execution_Context *context, unsigned min_size, ES_Block<T> *follows = NULL);
    /**< Allocate a new block that is of size min_size large or
       grow_ratio times bigger than the last allocated block. */

    ES_Block<T> *last_used_block;
    unsigned next_capacity; /**< Size of next allocated block. */
    unsigned extra_capacity; /**< Number of extra elements to allocate in each block. */
    enum { grow_ratio = 2 };
    BOOL prefill; /**< Prefill items with prototype_item if true. */
    T prototype_item;
    unsigned total_used;

#ifdef ES_DEBUG_BLOCK_USAGE
    class DebugItemInfo : public Link
    {
    public:
        DebugItemInfo(unsigned size = 1, unsigned overlap = 0, BOOL first_in_block = TRUE) : size(size), overlap(overlap), first_in_block(first_in_block) {}
        unsigned size;
        unsigned overlap;
        BOOL first_in_block;
    };

    Head debug_stack;
#endif // ES_DEBUG_BLOCK_USAGE
};

template <class T>
class ES_Block : public Link
{
    friend class ES_BlockHead<T>;
public:
    ES_Block(unsigned capacity) : used(0), capacity(capacity) {}

    OP_STATUS Initialize(ES_Execution_Context *context, unsigned extra_capacity);

    void Reset();

    ~ES_Block();

    void Allocate(T *&item, unsigned size);
    void Free(unsigned size);

    unsigned Available() { return capacity - used; }
    unsigned Used() { return used; }
    unsigned Capacity() { return capacity; }

    T *Storage() { return storage; }

    void Populate(T prototype_item);

    void SetUsed(unsigned new_used) { used = new_used; }

private:
    ES_Execution_Context *context;
    T *storage;
    unsigned used;
    unsigned capacity, total_capacity;
};

template <class T>
class ES_BlockItemIterator
{
public:
    ES_BlockItemIterator(ES_BlockHead<T> *block_head, BOOL set_to_end = TRUE);

    ES_BlockItemIterator<T> *Prev();
    T *GetItem() { return current; }

private:
    T *current;
    ES_Block<T> *current_block;
};

#endif // ES_BLOCK_H
