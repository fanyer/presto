/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright Opera Software ASA  2001-2006
 *
 * @author Lars T Hansen
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#ifdef ES_MARK_SWEEP_COLLECTOR

#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"
#include "modules/ecmascript/carakan/src/es_currenturl.h"
#include "modules/pi/OpSystemInfo.h"

ES_MarkSweepHeap::ES_MarkSweepHeap()
    : arena(NULL), large_objects(NULL), freelist(NULL)
{
    ClearQuicklists();
}

/* virtual */
ES_MarkSweepHeap::~ES_MarkSweepHeap()
{
    while (arena)
    {
        ES_PageHeader *to_delete = arena;
        arena = arena->next;
        page_allocator->FreeFixed(to_delete);
    }

    OP_ASSERT(!large_objects);
}

/** The number of bytes allocated by the heap, directly
    and indirectly. */
#define ES_HEAP_LIVE_BYTES() (bytes_live + bytes_live_external)

/** The predicate for determining if a GC should be run due to
    "enough" allocations having been made. */
#define ES_HEAP_LIMIT_EXCEEDED() (ES_HEAP_LIVE_BYTES() > bytes_limit)

/** The predicate for determining if running a maintenance GC
    would be beneficial. Apart from comparing the heap's
    direct and indirect allocations with respect to a given
    overall limit, a larger set of detached runtimes will also
    make running a maintenance GC worthwhile. */
#define ES_HEAP_OFFLINE_LIMIT_EXCEEDED() (ES_HEAP_LIVE_BYTES() > bytes_offline_limit || attached_runtimes < detached_runtimes)

/* Simple policy: collect when allocation has exceeded the
 * limit, and after each collection, make the limit twice the
 * amount of live storage.  At the same op_time, guarantee that
 * there is a maximum amount of allocation between collections;
 * this limits heap growth when the amount of live data becomes
 * large.  When we switch to a generational model this will be
 * less of an issue.
 */

/* virtual */ void
ES_MarkSweepHeap::MaybeCollect(ES_Context *context)
{
    if (needs_gc || ES_HEAP_LIMIT_EXCEEDED())
        ForceCollect(context, GC_REASON_ALLOCLIMIT);
}


/* virtual */ void
ES_MarkSweepHeap::MaybeCollectOffline(ES_Context *context)
{
    if (needs_gc || ES_HEAP_OFFLINE_LIMIT_EXCEEDED())
        ForceCollect(context, GC_REASON_ALLOCLIMIT);
}

class ES_SuspendedCollect
    : public ES_SuspendedCall
{
public:
    ES_SuspendedCollect(ES_MarkSweepHeap *heap, GC_Reason reason)
        : heap(heap), reason(reason)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        heap->Collect(reason, context);
    }

private:
    ES_MarkSweepHeap *heap;
    GC_Reason reason;
};


/* virtual */ void
ES_MarkSweepHeap::ForceCollect(ES_Context *context, GC_Reason reason)
{
    OP_ASSERT(reason != GC_REASON_MAINTENANCE || contexts == 0);

    if (in_collector)
        return;

    if (!external_needs_gc && reason == GC_REASON_EXTERN && (bytes_live - bytes_live_after_gc) < ES_PARM_SMALLEST_GROWTH_BETWEEN_GCS)
        return;

    /**
     * GC_REASON_MERGE is done prior to creating a new environment
     * using this heap.  The condition is stolen from
     * MaybeCollectOffline(). So far, I have seen that doing a gc
     * prior to creating the environment is good for both performance
     * (for results on performance tests at least) and memory usage.
     * I haven't tinkered much with the condition, so quite possibly there
     * is a better one.
     */
    if (reason == GC_REASON_MERGE && !(needs_gc || ES_HEAP_OFFLINE_LIMIT_EXCEEDED()))
        return;

#ifdef ES_DBG_GC
    if (ES_HEAP_LIVE_BYTES() > bytes_limit)
    {
        GC_DDATA.overshoot_percent_sum += (double) (ES_HEAP_LIVE_BYTES() - bytes_limit) / bytes_limit;
        GC_DDATA.overshoot_percent_avg  = GC_DDATA.overshoot_percent_sum / (GC_DDATA.num_major_collections + 1);
    }
#endif // ES_DBG_GC

    HEAP_DEBUG_LIVENESS_FOR(this);

#ifdef _DEBUG
    unsigned dbg_bytes_live_before_gc = bytes_live;
#endif //_DEBUG
    in_collector = TRUE;
    if (!context || context->IsUsingStandardStack())
    {
#ifdef CRASHLOG_CAP_PRESENT
        ES_CurrentURL current_url(this);
#endif // CRASHLOG_CAP_PRESENT

        Collect(reason, context);
    }
    else
    {
        ES_SuspendedCollect collect(this, reason);
        static_cast<ES_Execution_Context *>(context)->SuspendedCall(&collect);
    }
    in_collector = FALSE;
#ifdef _DEBUG
    OP_ASSERT(bytes_live <= dbg_bytes_live_before_gc);
    /* Note: a similar assertion for external allocations cannot be made.
       A host object could increase its reported allocation the next time
       it is traced, bringing the heap's total up post GC. */
#endif // _DEBUG

    return;
}

void
ES_MarkSweepHeap::TraceFromHeap()
{
    for (ES_PageHeader *p = large_objects; p != NULL; p = p->next)
    {
        ES_Boxed *o = p->GetFirst();
        if (IsMarked(o))
        {
            ClearMarked(o);
            Mark(o);
        }
    }

    for (ES_PageHeader *p = arena; p != NULL; p = p->next)
    {
        if (p->GetHasMarkedObjects())
        {
            for (ES_Boxed *o = p->GetFirst(); o != p->limit; o = reinterpret_cast<ES_Boxed*>(reinterpret_cast<char*>(o) + ObjectSize(o)))
            {
                if (IsMarked(o))
                {
                    ClearMarked(o);
                    Mark(o);
                }
            }
        }
    }
}

#define BYTES2UNITS(x) ((x)>>ES_LIM_ARENA_ALIGN_SHIFT)
#define UNITS2BYTES(x) ((x)<<ES_LIM_ARENA_ALIGN_SHIFT)

/* virtual */ OP_STATUS
ES_MarkSweepHeap::MergeWith(ES_Heap *other0)
{
    ES_MarkSweepHeap *other = static_cast<ES_MarkSweepHeap *>(other0);
    /* Check that neither heap is "busy". */
    OP_ASSERT(!inside_marker && !other->inside_marker);
    OP_ASSERT(!in_collector && !other->in_collector);

    /* Make sure the two heaps have the same cross-heap state. */
    OP_ASSERT(markstack == other->markstack);

    UpdateAndClearCurrent();
    other->UpdateAndClearCurrent();

    /* Transfer dynamic roots (i.e. protected objects.)  We're doing this first
       since it can fail, and is non-destructive. */
    for (ESMM_DynamicRootsSegment *s = other->root_collection->dynamic_roots->Segments(); s != NULL; s = s->next)
        for (int i = ES_PARM_DYNAMIC_ROOTS_SEGMENT_SIZE - 1; i >= 0; --i)
            if (s->entries[i].obj && !AddDynamicRoot(s->entries[i].obj))
            {
                for (ESMM_DynamicRootsSegment *s2 = other->root_collection->dynamic_roots->Segments(); s2 != NULL; s2 = s2->next)
                {
                    int segment_end = s == s2 ? (i + 1) : 0;
                    for (int i2 = ES_PARM_DYNAMIC_ROOTS_SEGMENT_SIZE - 1; i2 >= segment_end; --i2)
                        if (s2->entries[i2].obj)
                            RemoveDynamicRoot(s2->entries[i2].obj);
                    if (s == s2)
                        break;
                }

                return OpStatus::ERR_NO_MEMORY;
            }

    for (ESMM_DynamicRootsSegment *s = other->root_collection->dynamic_roots->Segments(); s != NULL; s = s->next)
        for (int i = ES_PARM_DYNAMIC_ROOTS_SEGMENT_SIZE - 1; i >= 0; --i)
            s->entries[i].obj = NULL;

    ES_PageHeader **page_ptr;
    ES_Free **free_ptr;

    /* Transfer normal heap pages. */
    if (other->arena)
    {
        page_ptr = &arena;
        while (*page_ptr)
            page_ptr = &(*page_ptr)->next;
        *page_ptr = other->arena;
        other->arena = NULL;
    }

    /* Transfer large objects. */
    if (other->large_objects)
    {
        page_ptr = &large_objects;
        while (*page_ptr)
            page_ptr = &(*page_ptr)->next;
        *page_ptr = other->large_objects;
        other->large_objects = NULL;
    }

#ifndef ES_GENERATIONAL_COLLECTOR
    /* Transfer root objects. */
    ES_RootData *root;
    while ((root = other->root_collection->root_objs.root_next) != &other->root_collection->root_objs)
    {
        root->Remove();
        root->AddTo(this);
        root->NewHeap(this);
    }
#endif // !ES_GENERATIONAL_COLLECTOR

    /* Merge quicklists. */
    for (unsigned nunits8 = 0; nunits8 < QUICK_CUTOFF_UNITS_MAX / 8; ++nunits8)
    {
        for (unsigned nunits = nunits8 * 8, stop = nunits + 8; nunits != stop; ++nunits)
            if (other->quicklists[nunits])
            {
                free_ptr = &quicklists[nunits];
                while (*free_ptr)
                    free_ptr = &(*free_ptr)->next;
                *free_ptr = other->quicklists[nunits];
                other->quicklists[nunits] = NULL;
            }

        quicklist_bitmap[nunits8] |= other->quicklist_bitmap[nunits8];
    }

    /* Merge freelists. */
    if (other->freelist)
    {
        free_ptr = &freelist;
        while (*free_ptr)
            free_ptr = &(*free_ptr)->next;
        *free_ptr = other->freelist;
        other->freelist = NULL;
    }

    /* I'm not sure it makes perfect sense to just add all of these... */
    bytes_in_heap += other->bytes_in_heap;
    pages_in_heap += other->pages_in_heap;
    bytes_live += other->bytes_live;
    bytes_live_external += other->bytes_live_external;
    bytes_limit += other->bytes_limit;
    bytes_offline_limit += other->bytes_offline_limit;
    bytes_live_after_gc += other->bytes_live_after_gc;
    bytes_live_external_after_gc += other->bytes_live_external_after_gc;
    bytes_free_after_gc += other->bytes_free_after_gc;

    /* Merge lists of runtimes, and update heap pointers in the runtimes. */
    ES_Runtime **runtime_ptr = &first_runtime;
    while (*runtime_ptr)
        runtime_ptr = &(*runtime_ptr)->GetNextRuntimePerHeap();
    ES_Runtime *moved_runtime = *runtime_ptr = other->first_runtime;
    while (moved_runtime)
    {
        moved_runtime->SetHeap(this);
        moved_runtime = moved_runtime->GetNextRuntimePerHeap();
    }
    other->first_runtime = NULL;

    while (ES_Allocation_Context *allocation_context = other->allocation_context)
        allocation_context->MoveToHeap(this);

    attached_runtimes += other->attached_runtimes;
    detached_runtimes += other->detached_runtimes;

    last_activity = g_op_time_info->GetRuntimeTickMS();
    last_gc = MIN(last_gc, other->last_gc);

    /* Merge list of references, and update heap pointer in them. */
    ES_HeapReference **reference_ptr = &first_reference;
    while (*reference_ptr)
        reference_ptr = &(*reference_ptr)->next;
    ES_HeapReference *moved_reference = *reference_ptr = other->first_reference;
    while (moved_reference)
    {
        moved_reference->heap = this;
        moved_reference = moved_reference->next;
    }
    other->first_reference = NULL;

    /* Merge lists of ES_StaticStringData references. */
    if (other->static_string_data_links)
    {
        StaticStringDataLink **link_ptr = &static_string_data_links;
        while (*link_ptr)
            link_ptr = &(*link_ptr)->next;
        *link_ptr = other->static_string_data_links;
        other->static_string_data_links = NULL;
    }

    /* Merge list of prototype classes */
    if (other->prototypes)
    {
        ES_Class_Singleton **link_ptr = &prototypes;
        while (*link_ptr)
            link_ptr = &(*link_ptr)->next;
        *link_ptr = other->prototypes;
        other->prototypes = NULL;
    }

    /* Merge list of prototype class tables. */
    if (other->class_tables)
    {
        ES_Code_ClassTable **link_ptr = &class_tables;
        while (*link_ptr)
            link_ptr = &(*link_ptr)->next;
        *link_ptr = other->class_tables;
        other->class_tables = NULL;
    }

#ifdef ES_HEAP_DEBUGGER
    for (unsigned index = 0; index < GCTAG_UNINITIALIZED; ++index)
        live_objects[index] += other->live_objects[index];
#endif // ES_HEAP_DEBUGGER

    page_allocator->MergeWith(other);

#ifdef ES_NATIVE_SUPPORT
    executable_memory->MergeWith(other->executable_memory);
    other->executable_memory = NULL;

    native_code_blocks.Append(&other->native_code_blocks);
    deprecated_native_code_blocks.Append(&other->deprecated_native_code_blocks);
#endif // ES_NATIVE_SUPPORT

    /* Merge lock and other internal states. */
    locked += other->locked;
    other->locked = 0;

    for (int i = 0; i < other->contexts; i++)
        IncContexts();

    external_needs_gc = other->external_needs_gc || external_needs_gc;

#ifdef ES_DBG_GC
    debug_data.MergeWith(&other->debug_data);
#endif // ES_DBG_GC

    /* Finally, destroy the now empty other heap. */
    ES_MarkSweepHeap::Destroy(other);

    return OpStatus::OK;
}

void
ES_MarkSweepHeap::UpdateAndClearCurrent()
{
    if (current_limit)
    {
        OP_ASSERT(current_top <= current_limit || static_cast<unsigned>(current_top - current_limit) == sizeof(ES_Free));

        if (current_top <= current_limit)
        {
            ES_Free *current = reinterpret_cast<ES_Free *>(current_top);
            unsigned size = current_limit - current_top + sizeof(ES_Free);
            OP_ASSERT(size <= UINT16_MAX);

            current->SetHeader(size, 0);

            if (size < LARGE_OBJECT_LIMIT)
            {
                unsigned units = BYTES2UNITS(size);
                AddToQuicklist(quicklists[units], quicklist_bitmap, units, current);
            }
            else
            {
                current->next = freelist;
                freelist = current;
            }
        }

        current_top = current_limit = NULL;
    }
}

/* virtual */ ES_Boxed *
ES_MarkSweepHeap::AllocateSmall(ES_Context *context, unsigned nbytes)
{
    unsigned nunits = BYTES2UNITS(nbytes);

    OP_ASSERT(nbytes < LARGE_OBJECT_LIMIT);

    {
        if (current_limit && current_top <= current_limit + sizeof(ES_Free))
        {
            OP_ASSERT(current_top > current_limit);
            ES_Boxed *object = reinterpret_cast<ES_Boxed *>(current_top - nbytes);
            unsigned padding_bytes = current_limit + sizeof(ES_Free) - current_top;
            OP_ASSERT(padding_bytes < sizeof(ES_Free));

            current_top += padding_bytes;
            OP_ASSERT(current_top == current_limit + sizeof(ES_Free));

            OP_ASSERT((nbytes + padding_bytes) < UINT16_MAX);
            object->SetHeader(nbytes + padding_bytes, 0);

            bytes_live += padding_bytes;

            return object;
        }

        current_top -= nbytes;
        UpdateAndClearCurrent();

    try_again:
        if (freelist)
        {
            bytes_live -= nbytes;
            SetCurrent(freelist);
            freelist = freelist->next;
            return Allocate(context, nbytes);
        }

        ES_Free **qlists = quicklists;
        unsigned char *qlist_bitmap = quicklist_bitmap;

        if (ES_Free *from_quicklist = RemoveFromQuicklist(qlists[nunits], qlist_bitmap, nunits))
            return CAST_TO_BOXED(from_quicklist);

        /* Slower cases: split objects in larger quicklists, or allocate a new
           page. */

        unsigned qindex = nunits + 2;

        if (qindex < QUICK_CUTOFF_UNITS)
        {
            unsigned offset = qindex & 7;

            ES_Free *block = NULL;
            unsigned char *bitmap;

            if (offset != 0)
                for (unsigned stop = qindex + 8 - offset; qindex != stop; ++qindex)
                    if ((block = RemoveFromQuicklist(qlists[qindex], qlist_bitmap, qindex)) != NULL)
                        goto allocate_from_block;

            bitmap = qlist_bitmap + (qindex >> 3);

            while (qindex < QUICK_CUTOFF_UNITS && !*bitmap)
            {
                qindex += 8;
                ++bitmap;
            }

            if (qindex < QUICK_CUTOFF_UNITS)
                for (;; ++qindex)
                    if ((block = RemoveFromQuicklist(qlists[qindex], qlist_bitmap, qindex)))
                    {
                    allocate_from_block:
                        unsigned size = UNITS2BYTES(qindex) - nbytes;

                        ES_Boxed *object = reinterpret_cast<ES_Boxed *>(reinterpret_cast<char *>(block) + size);

                        OP_ASSERT(nbytes != 0);
                        OP_ASSERT(size != 0);

                        object->SetHeader(es_minu(nbytes, UINT16_MAX), 0);

                        block->SetSize(es_minu(size, UINT16_MAX));

                        unsigned sizeunits = BYTES2UNITS(size);

                        OP_ASSERT(size >= sizeof(ES_Free));

                        AddToQuicklist(qlists[sizeunits], qlist_bitmap, sizeunits, block);

                        OP_ASSERT(ObjectSize(object) != 0);

                        return object;
                    }
        }

        // This will only be true once; if we run the GC and try again and still
        // fail to find a suitable block, this condition will be false (since
        // the GC adjusted bytes_limit to be bigger than what
        // bytes_live was after the collection) and we allocate a new
        // block instead.

        /**
         * Short circuiting the gc here when a runtime is being
         * created has the effect that we might bring in new pages
         * earlier than we would otherwise do. This is mostly a
         * performance thing. Doing the gc here is bad for performance
         * and in this case it doesn't really seem to affect memory
         * usage much (we have already attempted a gc before starting
         * to create the runtime). In the case where we create several
         * runtimes on the same heap they will end up filling the
         * slack from the previous runtime before getting here, so in
         * effect it is only the extra memory that the last runtime
         * brings in that affects memory usage.
         */

        if (!before_runtime_creation_bytes_live && (external_needs_gc || ES_HEAP_LIMIT_EXCEEDED()))
            if (locked)
                needs_gc = TRUE;
            else
            {
                // Restore and readjust bytes_live here since the GC will typically
                // touch it.

                bytes_live -= nbytes;
                ForceCollect(context, GC_REASON_ALLOCLIMIT);
                OP_ASSERT(!external_needs_gc);
                bytes_live += nbytes;

                if (!ES_HEAP_LIMIT_EXCEEDED())
                    goto try_again;
            }

        if (AllocateAndInsertPage(context, ES_PARM_ARENA_BLOCK_SIZE))
        {
            bytes_live -= nbytes;
            SetCurrent(freelist);
            freelist = freelist->next;
            return Allocate(context, nbytes);
        }
    }

    bytes_live -= nbytes;

    return NULL;
}

/* virtual */ ES_Boxed *
ES_MarkSweepHeap::AllocateLarge(ES_Context *context, unsigned nbytes)
{
    OP_ASSERT(nbytes >= LARGE_OBJECT_LIMIT);

    {
        if (ES_HEAP_LIMIT_EXCEEDED())
        {
            // Restore and readjust bytes_live here since the GC will typically
            // touch it.

            bytes_live -= nbytes;
            ForceCollect(context, GC_REASON_ALLOCLIMIT);
            bytes_live += nbytes;
        }

        ES_PageHeader *page = page_allocator->AllocateLarge(context, nbytes);

        if (page)
        {
            bytes_in_heap += nbytes;

            page->next = large_objects;
            large_objects = page;

            ES_Boxed *object = MakeLargeFree(page->GetFirst());

            return object;
        }
    }

    bytes_live -= nbytes;

    return NULL;
}

/* virtual */ ES_Boxed *
ES_MarkSweepHeap::AllocateSimpleSmall(ES_Context *, unsigned nbytes)
{
    unsigned nunits = BYTES2UNITS(nbytes);

    OP_ASSERT(nbytes < LARGE_OBJECT_LIMIT);

    ES_Free **qlists = quicklists;
    unsigned char *qlist_bitmap = quicklist_bitmap;

    if (ES_Free *from_quicklist = RemoveFromQuicklist(qlists[nunits], qlist_bitmap, nunits))
    {
        bytes_live += nbytes;
        return CAST_TO_BOXED(from_quicklist);
    }

    return NULL;
}

#ifdef _DEBUG

unsigned long
ES_MarkSweepHeap::CountBytesInArena()
{
    unsigned long nbytes = 0;

    ES_PageHeader *p = arena;
    while (p)
    {
        nbytes += p->GetSize();
        p = p->next;
    }

    return nbytes;
}

unsigned long
ES_MarkSweepHeap::CountBytesInLargeObjects()
{
    unsigned long nbytes = 0;

    ES_PageHeader *p = large_objects;
    while (p)
    {
        nbytes += p->GetSize();
        p = p->next;
    }

    return nbytes;
}

unsigned long
ES_MarkSweepHeap::CountBytesInQuicklists()
{
    unsigned long nbytes = 0;

    for (unsigned index = 0; index < QUICK_CUTOFF_UNITS_MAX; ++index)
    {
        ES_Free *x = quicklists[index];
        while (x)
        {
            nbytes += ObjectSize(x);
            x = x->next;
        }
    }

    return nbytes;
}

unsigned long
ES_MarkSweepHeap::CountSizeOfQuicklists()
{
    unsigned long count = 0;

    for (unsigned index = 0; index < QUICK_CUTOFF_UNITS_MAX; ++index)
    {
        ES_Free *x = quicklists[index];
        while (x)
        {
            count++;
            x = x->next;
        }
    }

    return count;
}

unsigned long
ES_MarkSweepHeap::CountBytesInFreelist()
{
    unsigned long nbytes = 0;

    ES_Free *x = freelist;
    while (x)
    {
        nbytes += ObjectSize(x);
        x = x->next;
    }

    return nbytes;
}

#endif // _DEBUG

/* virtual */ void
ES_MarkSweepHeap::Free(ES_Boxed *block)
{
    unsigned osize = ObjectSize(block);

    if (sizeof(ES_Free) <= osize && osize < LARGE_OBJECT_LIMIT)
    {
        AddToFreeStore(static_cast<ES_Free *>(block), ObjectSize(block));
        bytes_live -= osize;
    }
}

BOOL
ES_MarkSweepHeap::Collect(GC_Reason reason, ES_Context *context/* = NULL*/)
{
    if (locked != 0)
    {
        needs_gc = TRUE;
        return FALSE;
    }

    needs_gc = FALSE;
    external_needs_gc = FALSE;
    detached_runtimes = 0;

    UpdateAndClearCurrent();

    bytes_live_peak = es_maxu(bytes_live_peak, bytes_live);

    GC_DDATA.bytes_in_heap_peak = es_maxul(GC_DDATA.bytes_in_heap_peak, bytes_in_heap);
    GC_DDATA.bytes_allocated += bytes_live - bytes_live_after_gc;
    GC_DDATA.bytes_allocated_external += bytes_live_external - bytes_live_external_after_gc;

    /* Reset external allocation counter; tracing will recompute
       it to its current (accurate) value. */
    bytes_live_external = 0;

    markstack->inuse_peak = markstack->inuse;

#ifdef ES_DBG_GC
    GC_DDATA.host_objects_traced = 0;
    GC_DDATA.host_objects_deleted = 0;
#endif

    inside_marker = FALSE;
    markstack->exhausted = FALSE;
    markstack->heap = this;

#ifdef ES_HEAP_DEBUGGER
    for (unsigned index = 0; index < GCTAG_UNINITIALIZED; ++index)
        live_objects[index] = 0;
#endif // ES_HEAP_DEBUGGER

    ESMM::ClearGCTimer();

    double trace_start = ESMM::GetGCTimer();

    TraceFromDynamicRoots();
    TraceFromRootObjects();

    TracePrototypeClasses();

    OP_ASSERT(markstack->segment->next == NULL);
    OP_ASSERT(*markstack->segment->top == NULL);

    while (markstack->exhausted)
    {
        markstack->exhausted = FALSE;
        TraceFromHeap();
    }

    OP_ASSERT(markstack->exhausted == FALSE);
    OP_ASSERT(markstack->segment->next == NULL);
    OP_ASSERT(*markstack->segment->top == NULL);

    GC_DDATA.ms_tracing_history[GC_DDATA.gc_history_pointer] = ESMM::GetGCTimer() - trace_start;

    markstack->heap = NULL;

    double sweep_start = ESMM::GetGCTimer();

    Sweep(reason, context);

    GC_DDATA.ms_sweeping_history[GC_DDATA.gc_history_pointer] = ESMM::GetGCTimer() - sweep_start;

    /* Done! */

    GC_DDATA.num_major_collections++;
    GC_DDATA.ms_collecting_history[GC_DDATA.gc_history_pointer] = ESMM::GetGCTimer();
    GC_DDATA.ms_major_collecting += GC_DDATA.ms_collecting_history[GC_DDATA.gc_history_pointer];
    GC_DDATA.ms_major_tracing += GC_DDATA.ms_tracing_history[GC_DDATA.gc_history_pointer];
    GC_DDATA.ms_major_sweeping += GC_DDATA.ms_sweeping_history[GC_DDATA.gc_history_pointer];
    GC_DDATA.gc_history_pointer = (GC_DDATA.gc_history_pointer + 1) % ARRAY_SIZE(GC_DDATA.ms_collecting_history);

    last_gc = g_op_time_info->GetRuntimeTickMS();

    return TRUE;
}

inline BOOL
SweepObject(ES_Boxed *b)
{
    if (IsMarked(b))
    {
        ClearMarked(b);
        return FALSE;
    }
    else
        return TRUE;
}

void
ES_MarkSweepHeap::Sweep(GC_Reason reason, ES_Context *context)
{
    ClearQuicklists();

    freelist = NULL;
    bytes_live_after_gc = 0;
    bytes_free_after_gc = 0;
    empty_pages = NULL;

    SweepPages(&large_objects);

    while (empty_pages)
    {
        ES_PageHeader *page = empty_pages;
        empty_pages = page->next;

        page_allocator->FreeLarge(context, page);
    }

    SweepPages(&arena);

    ESMM::SweepRuntimes(first_runtime);

    StaticStringDataLink **linkp = &static_string_data_links;

    while (*linkp)
        if (!IsMarked((*linkp)->data->storage))
        {
            StaticStringDataLink *to_remove = *linkp;
            *linkp = to_remove->next;

            ES_StaticStringData::DecRef(to_remove->data);
            OP_DELETE(to_remove);
        }
        else
        {
            ClearMarked((*linkp)->data->storage);
            linkp = &(*linkp)->next;
        }

    bytes_live = bytes_live_after_gc;
    bytes_live_external_after_gc = bytes_live_external;

    bytes_limit = es_minu(es_maxu(static_cast<unsigned>(RT_DATA.load_factor * ES_HEAP_LIVE_BYTES()), ES_PARM_SMALLEST_HEAP_SIZE),
                          ES_HEAP_LIVE_BYTES() + ES_PARM_LARGEST_GROWTH_BETWEEN_GCS);

    unsigned bytes_limit_heap = es_minu(es_maxu(static_cast<unsigned>(RT_DATA.load_factor * bytes_live), ES_PARM_SMALLEST_HEAP_SIZE),
                                        bytes_live + ES_PARM_LARGEST_GROWTH_BETWEEN_GCS);

    bytes_offline_limit = es_minu(es_maxu(static_cast<unsigned>(RT_DATA.offline_load_factor * ES_HEAP_LIVE_BYTES()), ES_PARM_SMALLEST_HEAP_SIZE),
                                  ES_HEAP_LIVE_BYTES() + ES_PARM_LARGEST_GROWTH_BETWEEN_GCS);

    if (reason != GC_REASON_MAINTENANCE)
        while (empty_pages && bytes_in_heap < bytes_limit_heap)
        {
            ES_PageHeader *page = empty_pages;
            empty_pages = page->next;

            InsertPage(page);
        }

    while (empty_pages)
    {
        ES_PageHeader *page = empty_pages;
        empty_pages = page->next;
        page->next = NULL;

        page_allocator->FreeFixed(page);
    }

    if (freelist)
    {
        SetCurrent(freelist);
        freelist = freelist->next;
    }
}

void
ES_MarkSweepHeap::SweepPages(ES_PageHeader **pp)
{
    ES_PageHeader *p = *pp;

    while (p != NULL)
    {
        if (!p->GetHasMarkedObjects() || SweepPage(p))
        {
            unsigned pagesize = reinterpret_cast<char*>(p->limit) - reinterpret_cast<char*>(p->GetFirst());

            *pp = p->next;

            bytes_in_heap -= pagesize;
            pages_in_heap -= 1;

            p->next = empty_pages;
            empty_pages = p;
        }
        else
        {
            p->SetHasMarkedObjects(p->GetHasMustDestroyObjects());
            pp = &p->next;
        }

        p = *pp;
    }
}

BOOL
ES_MarkSweepHeap::SweepPage(ES_PageHeader *page)
{
    op_yield();

    ES_Boxed *o = page->GetFirst();
    ES_Free *free = NULL;
    unsigned live_bytes = 0, free_bytes = 0, freesize = 0;

    unsigned bits = o->Bits();
    unsigned osize = ObjectSize(o);

    ES_Boxed *next = reinterpret_cast<ES_Boxed*>(reinterpret_cast<char*>(o) + osize);

    page->ClearHasMustDestroyObjects();

    do
    {
        OP_ASSERT(osize != 0);
        OP_ASSERT(o->GCTag() == GCTAG_free || reinterpret_cast<ES_PageHeader *>(reinterpret_cast<char *>(o) - o->GetPageOffset()) == page);

        unsigned next_bits = next->Bits();
        unsigned next_osize = ObjectSize(next);

        if ((bits & ES_Header::MASK_GCTAG) == GCTAG_free)
            goto add_free;
        else if (SweepObject(o))
        {
            if (bits & ES_Header::MASK_NEED_DESTROY)
                ESMM::DestroyObject(o);

        add_free:
            if (free)
                freesize += osize;
            else
            {
                free = reinterpret_cast<ES_Free *>(o);
                freesize = osize;
            }
        }
        else
        {
            if (bits & ES_Header::MASK_NEED_DESTROY)
                page->SetHasMustDestroyObjects();

            live_bytes += osize;

            if (free)
            {
                free_bytes += freesize;

                if (freesize >= sizeof(ES_Free))
                    AddToFreeStore(free, freesize);

                free = NULL;
            }
        }

        o = next;

        next = reinterpret_cast<ES_Boxed*>(reinterpret_cast<char*>(o) + next_osize);

        bits = next_bits;
        osize = next_osize;
    }
    while (osize != 0);

    BOOL is_empty = FALSE;

    if (free)
        if (CAST_TO_BOXED(free) == page->GetFirst())
            is_empty = TRUE;
        else
        {
            free_bytes += freesize;

            if (freesize >= sizeof(ES_Free))
                AddToFreeStore(free, freesize);
        }

    bytes_live_after_gc += live_bytes;
    bytes_free_after_gc += free_bytes;

    return is_empty;
}

void
ES_MarkSweepHeap::ClearQuicklists()
{
    ES_Free **qlists = quicklists;

    for (unsigned i = 0; i < QUICK_CUTOFF_UNITS_MAX; ++i)
        qlists[i] = NULL;

    op_memset(quicklist_bitmap, 0, QUICK_CUTOFF_UNITS_MAX / sizeof(unsigned char));
}

ES_PageHeader *
ES_MarkSweepHeap::AllocatePage(ES_Context *context, unsigned nbytes, unsigned extra)
{
#ifdef DEBUG_ENABLE_OPASSERT
    unsigned int request = nbytes;
#endif // DEBUG_ENABLE_OPASSERT

    nbytes += sizeof(ES_PageHeader) + (ES_LIM_ARENA_ALIGN - 1);

    void *storage;

    if (context->IsUsingStandardStack())
        storage = op_malloc(nbytes + extra);
    else
    {
        ES_SuspendedMalloc call(nbytes + extra);
        static_cast<ES_Execution_Context *>(context)->SuspendedCall(&call);
        storage = call.storage;
    }

    if (storage == NULL)
        return NULL;

    ES_PageHeader *page = reinterpret_cast<ES_PageHeader *>(storage);

    page->limit = reinterpret_cast<ES_Boxed *>((reinterpret_cast<UINTPTR>(storage) + nbytes) & ~(ES_LIM_ARENA_ALIGN - 1));
    page->next = NULL;
    page->ClearHasMarkedObjects();
    page->ClearHasMustDestroyObjects();

    GC_DDATA.pages_in_heap_peak = es_maxul(GC_DDATA.pages_in_heap_peak, pages_in_heap);

    OP_ASSERT(reinterpret_cast<char *>(page->limit) - reinterpret_cast<char *>(page->GetFirst()) == static_cast<ptrdiff_t>(request));
    OP_ASSERT(reinterpret_cast<char *>(page->limit) <= reinterpret_cast<char *>(page) + nbytes);

#ifdef ES_HARDCORE_GC_MODE
    if (g_hardcode_gc_mode)
        op_memset(page->GetFirst(), 0xfe, nbytes - sizeof(ES_PageHeader) - (ES_LIM_ARENA_ALIGN - 1));
#endif // ES_HARDCORE_GC_MODE

    if (extra)
        page->limit->ClearHeader();

    return page;
}

void
ES_MarkSweepHeap::InsertPage(ES_PageHeader *page)
{
    unsigned nbytes = reinterpret_cast<char *>(page->limit) - reinterpret_cast<char *>(page->GetFirst());

    bytes_in_heap += nbytes;
    pages_in_heap += 1;

    page->next = arena;
    arena = page;

    ES_Free *f = MakeFree(page->GetFirst(), nbytes);
    f->next = freelist;
    freelist = f;

    OP_ASSERT(reinterpret_cast<ES_Boxed *>(reinterpret_cast<char *>(page->GetFirst()) + ObjectSize(page->GetFirst())) == page->limit);
}

BOOL
ES_MarkSweepHeap::AllocateAndInsertPage(ES_Context *context, unsigned)
{
    ES_PageHeader *p = page_allocator->AllocateFixed(context);

    if (p == NULL)
        return FALSE;

    InsertPage(p);

    return TRUE;
}

void
ES_MarkSweepHeap::AddToFreeStore(ES_Free *block, unsigned blocksize)
{
    OP_ASSERT(blocksize >= sizeof(ES_Free));

    block = MakeFree(block, blocksize);

    unsigned int nunits = BYTES2UNITS(blocksize);
    if (nunits < QUICK_CUTOFF_UNITS)
        AddToQuicklist(quicklists[nunits], quicklist_bitmap, nunits, block);
    else
    {
        block->next = freelist;
        freelist = block;
    }
}

#undef ES_HEAP_LIVE_BYTES
#undef ES_HEAP_LIMIT_EXCEEDED
#undef ES_HEAP_OFFLINE_LIMIT_EXCEEDED
#endif // ES_MARK_SWEEP_COLLECTOR
