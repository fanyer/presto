/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2001 - 2006
 *
 * ECMAScript engine -- garbage collector.
 *
 * @author Lars T Hansen
 */

#ifndef ES_MARK_SWEEP_HEAP_H
#define ES_MARK_SWEEP_HEAP_H

#include "modules/util/simset.h"
#include "modules/ecmascript/carakan/src/kernel/es_boxed.h"
#include "modules/ecmascript/carakan/src/vm/es_block.h"

class ES_Value_Internal;
class ES_PageHeader;
class ES_MarkSweepHeap;

/** Representation of a garbage collected mark-sweep heap. */
class ES_MarkSweepHeap : public ES_Heap
{
public:
    ES_MarkSweepHeap();
    virtual ~ES_MarkSweepHeap();

    virtual OP_STATUS MergeWith(ES_Heap *other);
    /**< Add other's heap pages, quicklists, freelists and roots to ours, and
         destroy 'other'. */

    virtual void Free(ES_Boxed *block);

    BOOL AllocateAndInsertPage(ES_Context *context, unsigned nbytes);
    /**< Allocate page and insert it into the 'arena' list.  Also add the whole
         block of free memory in the page to 'freelist'. */

    virtual void MaybeCollect(ES_Context *context);
    /**< Make a policy decision about whether to run the collector, and run it
         if that is the decision. */

    virtual void MaybeCollectOffline(ES_Context *context);
    /**< Make a policy decision about whether to run the collector, and run it
         if that is the decision. Uses the lower offline load factor which makes
         it more likely to trigger a gc. */

    virtual void ForceCollect(ES_Context *context, GC_Reason reason);
    /**< Run a garbage collection. */

protected:
    friend class ES_SuspendedCollect;

    BOOL Collect(GC_Reason reason, ES_Context *context = NULL);
    /**< Collect garbage in this heap.  This means tracing all root objects, and
         then sweeping the arena. */

    void TraceFromHeap();
    /**< Trace from all objects on the heap. This function is used as a last resort
       when the mark stack has been exhausted. Then the heap contains marked but not
       fully traced objects. Repeatedly calling this function until it completes
       without exhausting the mark stack guarantees that all objects have been traced. */

    void Sweep(GC_Reason reason, ES_Context *context);
    /**< Sweep all arenas for garbage objects. */

    void SweepPages(ES_PageHeader **arena);
    /**< Sweep each page in the arena for garbage objects. */

    BOOL SweepPage(ES_PageHeader *page);
    /**< Sweep single page for garbage objects.  Called only by Sweep().
         Returns TRUE if the page contained no live objects.  */

#ifdef _DEBUG
    void SweepOnlyClear();
    /**< Clear all mark bits in the heap. */
#endif // _DEBUG

    void ClearQuicklists();
    /**< Clear all quicklists.  This is done every time the heap is sweeped.
         The quicklists are then repopulated as free blocks are found in the
         heap.  The reason this is done is that this is when adjacent free
         blocks are joined into larger free blocks; and thus an extent of memory
         that was already in a quicklist might end up in a different shape in a
         different quicklist, or even the freelist. */

    virtual ES_Boxed *AllocateSmall(ES_Context *context, unsigned nbytes);
    /**< Internal allocation. Called from Allocate when we can't allocate from
         current block. */

    virtual ES_Boxed *AllocateLarge(ES_Context *context, unsigned nbytes);
    /**< Internal allocation of large objects. */

    virtual ES_Boxed *AllocateSimpleSmall(ES_Context *context, unsigned nbytes);

    ES_PageHeader *AllocatePage(ES_Context *context, unsigned nbytes, unsigned extra = 0);
    /**< Allocate page containing 'nbytes' of memory, excluding page header. */

    inline void AddToFreeStore(ES_Free *object, unsigned objectsize);
    /**< Add object to quick- or freelist. */

    void InsertPage(ES_PageHeader *page);
    /**< Add page to 'arena' list and its full extent as an ES_Free object at
         the beginning of 'freelist'. */

#ifdef _DEBUG
    unsigned long CountBytesInArena();
    unsigned long CountBytesInLargeObjects();
    unsigned long CountBytesInQuicklists();
    unsigned long CountBytesInFreelist();
    unsigned long CountSizeOfQuicklists();
#endif // _DEBUG

    void UpdateAndClearCurrent();
    inline void SetCurrent(ES_Free *block);

    friend class ES_RootData;

    ES_PageHeader *arena;
    /**< Single linked list of pages used for objects smaller than
         LARGE_OBJECT_LIMIT bytes. */

    friend class ESMM;
    friend class ES_GenerationalHeap;

    ES_PageHeader *large_objects;
    /**< Single linked list of pages containing objects larger than
         LARGE_OBJECT_LIMIT.  Each page contains one object, and the pages are
         never reused for future allocations. */

    ES_Free *quicklists[QUICK_CUTOFF_UNITS_MAX];
    /**< Array of single linked lists of free blocks, AKA "quicklists". */

    unsigned char quicklist_bitmap[QUICK_CUTOFF_UNITS_MAX / sizeof(unsigned char)];
    /**< Bitmap where each quicklist is represented by one bit.  If the bit is
         zero, the quicklist is empty, otherwise it contains at least one
         object.  Used to speed up search for a non-empty quicklist in some
         situations. */

    ES_Free *freelist;
    /**< Free blocks that are bigger than LARGE_OBJECT_LIMIT.  Because objects
         larger than that are allocated directly by malloc via separate pages,
         any allocation that ends up using a free block from this list will
         always succeed.  Thus, any such allocation will carve what it needs off
         the first object in the freelist and then either add the remains to the
         appropriate quicklist or return it to the freelist. */

    ES_PageHeader *empty_pages;
    /**< Empty pages. */
};

#endif // ES_MARK_SWEEP_HEAP_H
