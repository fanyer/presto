/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2001 - 2006
 *
 * ECMAScript engine -- garbage collector inline functions
 *
 * @author Lars T Hansen
 */

#ifndef ES_MARK_SWEEP_HEAP_INLINES_H
#define ES_MARK_SWEEP_HEAP_INLINES_H

#ifdef ES_MARK_SWEEP_COLLECTOR

inline void
AddToQuicklist(ES_Free *&quicklist, unsigned char *bitmap, unsigned index, ES_Free *f)
{
    if (!quicklist)
        bitmap[index >> 3] |= 1u << (index & 7);

    f->next = quicklist;
    quicklist = f;
}

inline ES_Free *
RemoveFromQuicklist(ES_Free *&quicklist, unsigned char *bitmap, unsigned index)
{
    OP_ASSERT(!quicklist == !(bitmap[index >> 3] & (1u << (index & 7))));

    if (quicklist)
    {
        ES_Free *f = quicklist;

        if ((quicklist = f->next) == NULL)
        {
            bitmap[index >> 3] ^= 1u << (index & 7);
            OP_ASSERT(0 == (bitmap[index >> 3] & (1u << (index & 7))));
        }

        return f;
    }
    else
        return NULL;
}

#define BYTES2UNITS(x) ((x)>>ES_LIM_ARENA_ALIGN_SHIFT)
#define UNITS2BYTES(x) ((x)<<ES_LIM_ARENA_ALIGN_SHIFT)

inline void
ES_MarkSweepHeap::SetCurrent(ES_Free *block)
{
    current_top = reinterpret_cast<char *>(block);
    current_limit = current_top + ObjectSize(block) - sizeof(ES_Free);
    current_page = block->GetPage();
}

inline void
ES_Heap::PushBoxed(ES_Boxed *object)
{
    if (object != NULL)
    {
        OP_ASSERT(object->GCTag() != 0 && object->GCTag() < GCTAG_UNINITIALIZED);
        OP_ASSERT(ObjectSize(object) != 0xCCCCCCCC);
        //OP_ASSERT(!IsDebugMarked(object));
        if (!IsMarked(object))
        {
            OP_ASSERT(object->GetPage()->GetFirst() <= object);
            OP_ASSERT(object < object->GetPage()->limit);
            OP_ASSERT((UINTPTR)object > 100000);
            SetMarked(object);
            if (object->GCTag() >= GCTAG_FIRST_THAT_NEED_TRACING)
            {
#ifdef ES_RECURSIVE_TRACING
                ESMM::TraceObject(this, object);
#else // ES_RECURSIVE_TRACING
                DECLARE_MARK_STATE;
                RESTORE_MARK_STATE;
                ++stktop;
                if (stktop == stklim)
                {
                    markstack->Overflow();
                    RESTORE_MARK_STATE;
                    ++stktop;
                }
                *stktop = object;
                SAVE_MARK_STATE;
#endif // ES_RECURSIVE_TRACING
            }
        }
    }
}

inline void
ES_Heap::PushValue(const ES_Value_Internal &value)
{
    if (value.IsDecodedBoxed())
        PushBoxed(value.GetDecodedBoxed());
}

#endif // ES_MARK_SWEEP_COLLECTOR
#endif // ES_MARK_SWEEP_HEAP_INLINES_H
