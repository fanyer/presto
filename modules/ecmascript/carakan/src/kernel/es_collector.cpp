/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright Opera Software ASA  2001-2006
 *
 * @author Lars T Hansen
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript/carakan/src/kernel/es_collector_inlines.h"
#include "modules/ecmascript/carakan/src/object/es_object.h"
#include "modules/ecmascript/carakan/src/object/es_special_property.h"
#include "modules/ecmascript/carakan/src/object/es_string_object.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"
#include "modules/ecmascript/carakan/src/object/es_typedarray.h"
#include "modules/ecmascript/carakan/src/object/es_arguments.h"
#include "modules/ecmascript/carakan/src/compiler/es_native.h"
#include "modules/ecmascript/carakan/src/compiler/es_code.h"
#include "modules/pi/OpSystemInfo.h"

#ifdef ES_HARDCORE_GC_MODE
BOOL g_hardcode_gc_mode = FALSE;
#endif // ES_HARDCORE_GC_MODE

// #define MARKSTACK_LIMIT 2        // Only for provoking mark stack exhaustion during testing

/****************************************************************************
 *
 * Initialization and destruction
 */

/* static */ ES_MarkStack *
ES_MarkStack::MakeL()
{
    ES_MarkStack *markstack = OP_NEW_L(ES_MarkStack, ());

    markstack->segment = OP_NEW(ES_MarkStackSegment, ());
    if (!markstack->segment)
    {
        OP_DELETE(markstack);
        LEAVE(OpStatus::ERR_NO_MEMORY);
    }
    markstack->inuse = markstack->inuse_peak = 1;

    GC_DDATA.markstack_segment_peak = 1;

    return markstack;
}

void
ES_MarkStack::Overflow()
{
    ES_MarkStackSegment *thisseg = segment;
    ES_MarkStackSegment *newseg = NULL;

    if (!freelist)
    {
#ifdef MARKSTACK_LIMIT
        // For debugging the overflow logic.  Define MARKSTACK_LIMIT to
        // the maximum number of segments the stack can use.  In principle
        // even '1' should work.
        if (inuse < MARKSTACK_LIMIT)
            newseg = OP_NEW(ES_MarkStackSegment, ());
#else // MARKSTACK_LIMIT
        newseg = OP_NEW(ES_MarkStackSegment, ());
#endif // MARKSTACK_LIMIT

        if (!newseg)
        {
            // No more space for the mark stack.  Just reuse the current segment,
            // and let the marker pick up the pieces later.

            exhausted = TRUE;
            GC_DDATA.markstack_exhaustions++;

            thisseg->top = thisseg->elements;
            return;
        }
        else
            GC_DDATA.markstack_segment_peak++;
    }
    else
    {
        newseg = freelist;
        freelist = newseg->next;
    }

    thisseg->top = &thisseg->elements[ES_PARM_MARKSTACK_SEGMENT_SIZE-1];
    newseg->next = thisseg;

    segment = newseg;

    inuse++;
    inuse_peak = es_maxul(inuse_peak, inuse);
}

void
ES_MarkStack::Underflow()
{
    ES_MarkStackSegment *thisseg = segment;
    ES_MarkStackSegment *nextseg = thisseg->next;
    OP_ASSERT(nextseg != NULL);
    thisseg->top = &thisseg->elements[0];
    thisseg->elements[0] = NULL;
    thisseg->next = freelist;
    freelist = thisseg;
    segment = nextseg;

    inuse--;
}

/* static */ ES_RootCollection *
ES_RootCollection::MakeL()
{
    ES_RootCollection *root_collection = OP_NEW_L(ES_RootCollection, ());
    ANCHOR_PTR(ES_RootCollection, root_collection);

    root_collection->root_objs.root_next = &root_collection->root_objs;
    root_collection->root_objs.root_prev = &root_collection->root_objs;

    root_collection->dynamic_roots = OP_NEW_L(ESMM_DynamicRoots, ());
    root_collection->dynamic_roots->ConstructL();

    root_collection->ref_count = 1;

    ANCHOR_PTR_RELEASE(root_collection);
    return root_collection;
}

ES_RootCollection::ES_RootCollection()
    : dynamic_roots(NULL),
      heap(NULL)
{
}

ES_RootCollection::~ES_RootCollection()
{
    OP_DELETE(dynamic_roots);
}

void ES_RootCollection::IncRef()
{
    ++ref_count;
}

void ES_RootCollection::DecRef()
{
    --ref_count;
    if (ref_count == 0)
        OP_DELETE(this);
}

/*static*/ ES_Heap *
ES_Heap::MakeL(ES_MarkStack *markstack, ES_Runtime *runtime/* = NULL*/, ES_RootCollection *root_collection/* = NULL*/, ES_Heap *parent_runtime_heap/* = NULL*/)
{
#ifdef ES_COLLECTOR_TYPE_GENERATIONAL
    ES_Heap *heap = OP_NEW_L(ES_GenerationalHeap, ());
#endif // ES_COLLECTOR_TYPE_GENERATIONAL

#ifdef ES_COLLECTOR_TYPE_MARK_SWEEP
    ES_Heap *heap = OP_NEW_L(ES_MarkSweepHeap, ());
#endif // ES_COLLECTOR_TYPE_MARK_SWEEP
    ANCHOR_PTR(ES_Heap, heap);

#ifdef ES_NATIVE_SUPPORT
    heap->executable_memory = OP_NEW_L(OpExecMemoryManager, (MEMCLS_ECMASCRIPT_EXEC));
#endif // ES_NATIVE_SUPPORT

    if (parent_runtime_heap)
        // Reuse an old page allocator if runtimes seems to be related, i.e.,
        // the probability that their heaps will be merged is high.
        heap->SetPageAllocator(parent_runtime_heap->page_allocator);
    else
    {
        ES_PageAllocator *allocator = OP_NEW_L(ES_PageAllocator, (g_ecma_chunk_allocator));
        heap->SetPageAllocator(allocator);
    }

    heap->first_reference = OP_NEW_L(ES_HeapReference, (heap));

    if (root_collection)
    {
        root_collection->IncRef();
        heap->root_collection = root_collection;
    }
    else
        heap->root_collection = ES_RootCollection::MakeL();

    heap->markstack = ES_MarkStack::IncRef(markstack);

    heap->root_collection->heap = heap;
    heap->AddHeapToManager();

#ifdef ES_DBG_GC
    op_memset(&heap->debug_data, 0, sizeof(heap->debug_data));
#endif // ES_DBG_GC

    ANCHOR_PTR_RELEASE(heap);
    return heap;
}

ES_Heap::ES_Heap()
    : current_top(NULL),
      current_limit(NULL),
      bytes_live(0),
      bytes_live_external(0),
#ifdef ES_HEAP_DEBUGGER
      id(g_ecmaManager->GetHeapId()),
#endif // ES_HEAP_DEBUGGER
      markstack(NULL),
      root_collection(NULL),
      inside_marker(FALSE),
      in_collector(FALSE),
      needs_gc(FALSE),
      external_needs_gc(FALSE),
      static_string_data_links(NULL),
      prototypes(NULL),
      class_tables(NULL),
#ifdef ES_HARDCORE_GC_MODE
      in_allocate(FALSE),
#endif // ES_HARDCORE_GC_MODE
#ifdef _DEBUG
      collections_count(1000),
#endif // _DEBUG
      bytes_in_heap(0),
      pages_in_heap(0),
      bytes_limit(ES_PARM_SMALLEST_HEAP_SIZE),
      bytes_offline_limit(bytes_limit),
      bytes_live_peak(0),
      bytes_live_after_gc(0),
      bytes_live_external_after_gc(0),
      bytes_free_after_gc(0),
      locked(0),
      allocation_context(0),
      first_runtime(0),
      attached_runtimes(0),
      detached_runtimes(0),
      first_reference(0),
      page_allocator(0),
#ifdef ES_NATIVE_SUPPORT
      executable_memory(NULL),
#endif // ES_NATIVE_SUPPORT
      last_activity(0),
      last_gc(UINT_MAX),
      contexts(0),
      page_allocator_handle(this),
      suitable_heap_merge(0),
      before_runtime_creation_bytes_live(0)
{
#ifdef ES_HEAP_DEBUGGER
    for (unsigned index = 0; index < GCTAG_UNINITIALIZED; ++index)
        live_objects[index] = 0;
#endif // ES_HEAP_DEBUGGER
}

/* virtual */
ES_Heap::~ES_Heap()
{
    if (page_allocator) // allocation could have failed.
        page_allocator->RemoveHeap(&page_allocator_handle);

#ifdef ES_NATIVE_SUPPORT
    native_code_blocks.Clear();
    deprecated_native_code_blocks.Clear();

    OP_DELETE(executable_memory);
#endif // ES_NATIVE_SUPPORT

    while (ES_HeapReference *reference = first_reference)
    {
        first_reference = reference->next;
        OP_DELETE(reference);
    }
}

/* static */ void
ES_Heap::Destroy(ES_Heap *heap)
{
    OP_ASSERT(!heap->in_collector);

    unsigned int bytes_live_last_time = heap->bytes_in_heap;

    heap->Unlock();
    heap->prototypes = NULL;

    BOOL first = TRUE;
    while (bytes_live_last_time != 0)
    {
        heap->ForceCollect(NULL, GC_REASON_SHUTDOWN);

        if (!first && heap->bytes_live_after_gc >= bytes_live_last_time)
        {
            OP_ASSERT(!"Could not release all memory.");
            break;
        }

        first = FALSE;
        bytes_live_last_time = heap->bytes_live_after_gc;
    }
    OP_ASSERT(heap->bytes_live_external == 0);

    heap->in_collector = FALSE;

    if (heap->root_collection)
        heap->root_collection->DecRef();

    if (heap->markstack)
    {
        OP_ASSERT(heap->markstack->segment == NULL || heap->markstack->segment->next == NULL);

        ES_MarkStack::DecRef(heap->markstack);
    }

    heap->RemoveHeapFromManager();

    //OP_ASSERT(!heap->first_runtime);

    OP_DELETE(heap);
}

void
ES_Heap::DestroyedRuntime(ES_Runtime *runtime)
{
    ES_Runtime **runtime_ptr = &first_runtime;
    while (*runtime_ptr != runtime)
        runtime_ptr = &(*runtime_ptr)->GetNextRuntimePerHeap();
    *runtime_ptr = runtime->GetNextRuntimePerHeap();
}

void
ES_Heap::InitializeAllocationLimit()
{
    bytes_limit = static_cast<unsigned>(RT_DATA.load_factor * GetBytesLiveTotal());
    bytes_offline_limit = static_cast<unsigned>(RT_DATA.offline_load_factor * GetBytesLiveTotal());
}

void
ES_Heap::RegisterActivity()
{
    last_activity = g_op_time_info->GetRuntimeTickMS();

    if (last_gc > last_activity)
        last_gc = last_activity;
    g_ecmaManager->StartMaintenanceGC();
}

void
ES_Heap::ResetActivity()
{
    last_activity = 0;
}

#ifndef _STANDALONE
BOOL
ES_Heap::ActiveSinceLastGC()
{
    return last_activity > last_gc;
}

BOOL
ES_Heap::ActiveRecently(double current_time)
{
    return last_activity >= current_time - ES_PARM_MAINTENANCE_GC_HEAP_INACTIVE_TIME;
}

BOOL
ES_Heap::CollectedRecently(double current_time)
{
    return last_gc >= current_time - ES_PARM_MAINTENANCE_GC_SINCE_LAST_GC;
}

BOOL
ES_Heap::NeedsMaintenanceGC(double current_time)
{
    if (IsLocked() || !ActiveSinceLastGC())
        return FALSE;

    return (!CollectedRecently(current_time) ||
            attached_runtimes < detached_runtimes);
}
#endif // !_STANDALONE

void
ES_Heap::IncContexts()
{
    if (++contexts == 1)
        g_ecmaManager->MoveHeapToActiveList(this);
}

void
ES_Heap::DecContexts()
{
    OP_ASSERT(contexts > 0);
    if (--contexts == 0)
    {
#ifdef ES_NATIVE_SUPPORT
        FlushDeprecatedNativeDispatchers();
#endif // ES_NATIVE_SUPPORT
        RegisterActivity();
        OP_ASSERT(contexts == 0);

        if (root_collection->dynamic_roots->Size() == 0 && !locked)
            g_ecmaManager->MoveHeapToDestroyList(this);
        else
            g_ecmaManager->MoveHeapToInactiveList(this);
    }
}

BOOL
ES_Heap::AddStaticStringData(ES_Context *context, ES_StaticStringData *data)
{
    StaticStringDataLink *link;

    if (!context || context->IsUsingStandardStack())
        link = OP_NEW(StaticStringDataLink, (data));
    else
    {
        ES_Suspended_NEW1<StaticStringDataLink, ES_StaticStringData *> call(data);
        static_cast<ES_Execution_Context *>(context)->SuspendedCall(&call);
        link = call.storage;
    }

    if (!link)
        return FALSE;

    ES_StaticStringData::IncRef(data);

    link->next = static_string_data_links;
    static_string_data_links = link;

    return TRUE;
}

void
ES_Heap::AddPrototypeClass(ES_Class_Singleton *klass)
{
    OP_ASSERT(klass->next == NULL);
    klass->next = prototypes;
    prototypes = klass;
}

void
ES_Heap::AddClassTable(ES_Code_ClassTable *table)
{
    OP_ASSERT(table->next == NULL);
    table->next = class_tables;
    class_tables = table;
}

void
ES_Heap::TracePrototypeClasses()
{
    ES_Class_Singleton **ptr = &prototypes;

    while (*ptr != NULL)
    {
        if (!IsMarked(*ptr))
        {
            if (ES_Object *prototype = (*ptr)->Prototype())
                if (!prototype->GetInstances()->IsClass())
                    prototype->RemoveSingletonInstance(this, *ptr);

            *ptr = (*ptr)->next;
        }
        else
            ptr = &(*ptr)->next;
    }

    /* Remove any unmarked classes from the recorded class tables. */
    ES_Code_ClassTable **table_ptr = &class_tables;
    while (*table_ptr != NULL)
    {
        BOOL empty = TRUE;
        for (unsigned i = 0; i < ES_Code_ClassTable::MAX_OBJECT_LITERAL_COMPACT_CLASSES; i++)
        {
            if ((*table_ptr)->table[i])
                if (!IsMarked((*table_ptr)->table[i]))
                {
                    if (ES_Object *prototype = (*table_ptr)->table[i]->Prototype())
                        if (!prototype->GetInstances()->IsClass())
                        {
                            prototype->RemoveSingletonInstance(this, (*table_ptr)->table[i]);
                            (*table_ptr)->table[i] = NULL;
                        }
                }
                else
                    empty = FALSE;
        }

        if (empty)
        {
            ES_Code_ClassTable *class_table = *table_ptr;
            *table_ptr = (*table_ptr)->next;
            OP_DELETE(class_table);
        }
        else
            table_ptr = &(*table_ptr)->next;
    }
}

unsigned
ES_Heap::CountPrototypeClasses() const
{
    unsigned count = 0;
    ES_Class_Singleton *ptr = prototypes;
    while (ptr)
    {
        ++count;
        ptr = ptr->next;
    }

    return count;
}

BOOL
ES_Heap::IsPrototypeClassRecorded(ES_Class_Singleton *klass)
{
    if (!prototypes)
        return FALSE;
    else if (klass->next)
        return TRUE;

    /* Optimize? */
    ES_Class_Singleton *ptr = prototypes;
    while (ptr->next)
        ptr = ptr->next;

    return klass == ptr;
}

void
ES_Heap::SuitableForReuse(BOOL merge)
{
    suitable_heap_merge += merge ? 1 : -1;
    OP_ASSERT(suitable_heap_merge >= 0 && suitable_heap_merge <= 2 || !"Can max be 2 in case both Identity() and DOM_Environment::Create() decided it was a good time to avoid creating new heaps");
    if (!merge && before_runtime_creation_bytes_live)
    {
        /**
         * The idea here is that creating a runtime adds a lot
         * of memory that can't be gc:ed away.  However, the
         * extra memory will push us closer to the gc
         * limit. Compensating for the extra memory means that
         * we will avoid some largely futile gc:s.
         */
        int delta = bytes_live - before_runtime_creation_bytes_live;
        OP_ASSERT(delta > 0);
        bytes_live_after_gc += delta;
        bytes_limit += static_cast<unsigned>(ES_PARM_INVERSE_LOAD_FACTOR * delta);
        bytes_offline_limit += static_cast<unsigned>(ES_PARM_INVERSE_OFFLINE_LOAD_FACTOR * delta);
        before_runtime_creation_bytes_live = 0;
    }
}

BOOL
ES_Heap::HasHighFragmentation()
{
    OP_NEW_DBG("ES_Heap::HasHighFragmentation", "es_memory");
    // This is tuned to say YES on pages that create many temporary iframes that
    // merge with their parents and produce almost-empty pages that we can't reuse.
    // Examples: test-suite at http://test262.ecmascript.org/ and
    // http://t/core/standards/html-parsing/html5lib_harness/index.html

    // It is tuned to say NO on pages that produce lots of small nice iframes such as
    // sunspider, because those might benefit a lot from fast GC.
    OP_ASSERT(bytes_live <= bytes_in_heap);
    unsigned bytes_free = bytes_in_heap - bytes_live;
    BOOL fragmented = bytes_free > (ES_PARM_INVERSE_LOAD_FACTOR - 1.0 + 0.15) * bytes_live &&
        bytes_free > 2 * ES_Chunk::TOTAL_SIZE ||
        bytes_free > 100 * ES_Chunk::TOTAL_SIZE;
    OP_DBG(("Fragmentation: live=%dKB, free=%dKB (factor=%g) -> %s\n", bytes_live / 1024, bytes_free / 1024, double(bytes_free)/double(bytes_live), fragmented ? "FRAGMENTED" : "NOT fragmented"));
    return fragmented;
}


#ifdef ES_NATIVE_SUPPORT

void
ES_Heap::AddNativeDispatcher(ES_NativeCodeBlock *block)
{
    block->Into(&native_code_blocks);
}

void
ES_Heap::DeprecateNativeDispatcher(ES_NativeCodeBlock *block)
{
    block->Deprecate();
    block->Out();
    block->Into(&deprecated_native_code_blocks);
}

void
ES_Heap::FlushNativeDispatchers()
{
    native_code_blocks.Clear();
}

void
ES_Heap::FlushDeprecatedNativeDispatchers()
{
    deprecated_native_code_blocks.Clear();
}

#endif // ES_NATIVE_SUPPORT

void
ES_Heap::AddHeapToManager()
{
    OP_ASSERT(contexts == 0);
    g_ecmaManager->AddHeap(this);
}

void
ES_Heap::RemoveHeapFromManager()
{
    g_ecmaManager->RemoveHeap(this);
}

void
ES_Heap::TraceFromDynamicRoots()
{
    for (ESMM_DynamicRootsSegment *s = root_collection->dynamic_roots->Segments(); s != NULL; s = s->next)
        for (int i = ES_PARM_DYNAMIC_ROOTS_SEGMENT_SIZE - 1; i >= 0; --i)
            if (ES_Boxed *&o = s->entries[i].obj)
                Mark(o);
}

void
ES_Heap::SetPageAllocator(ES_PageAllocator *allocator)
{
    if (page_allocator)
        page_allocator->RemoveHeap(&page_allocator_handle);
    page_allocator = allocator;
    page_allocator->AddHeap(&page_allocator_handle);
}

#ifdef ES_DBG_GC
void
ES_HeapDebugData::MergeWith(ES_HeapDebugData* other)
{
    small_object_count += other->small_object_count;
    large_object_count += other->large_object_count;
    small_object_bytes += other->small_object_bytes;
    large_object_bytes += other->large_object_bytes;
}
#endif // ES_DBG_GC
/****************************************************************************
 *
 * Roots management
 */

/* virtual */
ES_GCRoot::~ES_GCRoot()
{
    Remove();
}

void
ES_RootData::Remove()
{
    root_prev->root_next = root_next;
    root_next->root_prev = root_prev;
    root_prev = root_next = 0;
}

/* virtual */
ES_RootData::~ES_RootData()
{
    /* Nothing -- here to kill GCC warnings */
}

/* virtual */
void ES_RootData::GCTrace()
{
    OP_ASSERT(!"Should not happen");
}

/* virtual */
void ES_RootData::NewHeap(ES_Heap *heap)
{
}

ESMM_DynamicRoots::ESMM_DynamicRoots()
{
    segments = NULL;
    free = NULL;
    allocated = 0;
    population = 0;
    table = NULL;
    tablesize = 0;
}

ESMM_DynamicRoots::~ESMM_DynamicRoots()
{
    OP_DELETEA(table);
    while (segments != NULL)
    {
        ESMM_DynamicRootsSegment* tmp = segments;
        segments = segments->next;
        OP_DELETE(tmp);
    }
}

static const unsigned int tablesizes[] = // These are primes spaced at at least 3/2 growth intervals
{ 47, 71, 103, 157, 229, 347, 521, 769, 1163, 1733, 2609, 3907, 5839, 8761, 13147, 19709, 29567, 44351, 66509, 99761 };

void ESMM_DynamicRoots::ConstructL()
{
    size_t i;

    table = OP_NEWA_L(ESMM_DynamicRootsEntry*, tablesizes[0]);
    tablesize = tablesizes[0];
    for (i=0 ; i < tablesize ; i++)
        table[i] = NULL;
}

// Hash function discards low four bits because these are pointer values and
// are likely to be aligned.  This function seems to work well both with
// prime and non-prime table sizes.

inline unsigned ESMM_DynamicRoots::Hash(ES_Boxed* obj, unsigned m)
{
    return (reinterpret_cast<UINTPTR>(obj) >> 4) % m;
}

// OPTIMIZEME: Ought to be able to shrink the hash table and free unused
// storage segments, if the population becomes small after having been
// large.  (The segment storage can be compacted when the table is
// shrunk, for example.)  Not doing so actually constitutes a
// Class II storage leak.
//
// With the new DOM code, the hash table probably no longer grows very large;
// it's unclear that it still is needed.

void ESMM_DynamicRoots::Push(ES_Boxed* obj)
{
    if (population > tablesize*ES_PARM_DYNAMIC_ROOTS_LOAD_FACTOR)
    {
        // Grow the hash table; rehash
        size_t newtablesize, i, h;
        ESMM_DynamicRootsEntry** newtable;

        for (i=0 ; i < ARRAY_SIZE(tablesizes)-1 && tablesize != tablesizes[i] ; i++)
            ;
        if (i < ARRAY_SIZE(tablesizes)-1)
            newtablesize = tablesizes[i+1];
        else
            newtablesize = DOUBLE2INT32(tablesize*ES_PARM_DYNAMIC_ROOTS_GROWTH_FACTOR);
        newtable = OP_NEWA_L(ESMM_DynamicRootsEntry*, newtablesize);

        for (i=0 ; i < newtablesize ; i++)
            newtable[i] = NULL;

        ESMM_DynamicRootsSegment *s;
        for (s=segments ; s != NULL ; s=s->next)
        {
            for (i=0 ; i < ES_PARM_DYNAMIC_ROOTS_SEGMENT_SIZE ; i++)
            {
                if (s->entries[i].obj != NULL)
                {
                    h = Hash(s->entries[i].obj,newtablesize);
                    s->entries[i].next = newtable[h];
                    newtable[h] = &s->entries[i];
                }
            }
        }

        OP_DELETEA(table);
        table = newtable;
        tablesize = newtablesize;
    }

    if (free == NULL)
    {
        // Add a segment to the free list
        int i;
        ESMM_DynamicRootsSegment* s = OP_NEW_L(ESMM_DynamicRootsSegment, ());
        s->next = segments;
        segments = s;
        for (i=0 ; i < ES_PARM_DYNAMIC_ROOTS_SEGMENT_SIZE ; i++)
        {
            s->entries[i].obj = NULL;
            s->entries[i].next = &s->entries[i+1];
        }
        s->entries[ES_PARM_DYNAMIC_ROOTS_SEGMENT_SIZE-1].next = NULL;
        free = &s->entries[0];
#ifdef ES_DBG_GC
        GC_DDATA.dynamic_roots_segments++;
#endif
    }

    size_t h = Hash(obj,tablesize);
    ESMM_DynamicRootsEntry* node = free;
    free = free->next;
    node->obj = obj;
    node->next = table[h];
    table[h] = node;
    population++;
}

void ESMM_DynamicRoots::Remove(ES_Boxed* obj)
{
    size_t h = Hash(obj,tablesize);
    ESMM_DynamicRootsEntry *p, *q;

#ifdef ES_DBG_GC
    GC_DDATA.dynamic_roots_probes++;
#endif
    for (p=table[h], q=NULL ; p != NULL && p->obj != obj ; q=p, p=p->next)
    {
#ifdef ES_DBG_GC
        GC_DDATA.dynamic_roots_probes++;
#endif
    }

    if (p != NULL)
    {
        if (q != NULL)
            q->next = p->next;
        else
            table[h] = p->next;
        p->obj = NULL;
        p->next = free;
        free = p;
        population--;
        return;
    }
}

/****************************************************************************
 *
 * Utilities
 */

void ESMM::ClearGCTimer()
{
    GC_DDATA.gc_timer = g_op_time_info->GetRuntimeMS();
}

double ESMM::GetGCTimer()
{
    return g_op_time_info->GetRuntimeMS() - GC_DDATA.gc_timer;
}

#ifdef ES_DBG_GC
// Compute max(0,floor(log2(k)))
int ilog2(unsigned int k)
{
    int i=0;

    for (;;)
    {
        k >>= 1;
        if (k == 0) break;
        ++i;
    }
    return i;
}
#endif


/****************************************************************************
 *
 * Tracing
 */

#ifdef ES_DBG_TRACE_GC

/* static */ void
ESMM::WriteTraceHeader()
{
    // full names, useful for code processing the data
    fprintf(GC_DATA.trace_fp,
             "\n\n(("
             // Data for last GC: storage
             "reason "
             "ms-collecting "
             "ms-tracing "
             "ms-sweeping "
             "bytes-live-before "
             "bytes-live-after "
             "bytes-free-before "
             "bytes-free-after "
             "bytes-limit-after "
             "bytes-in-heap-before "    // approx live-before + free-before
             "bytes-in-heap-after "        // approx live-after + free-after
             "pages-in-heap-before "
             "pages-in-heap-after "
             "markstack-segment-used "
             "symbols-after "
             "fragmentation "
             ")\n ("
             // Data for last GC: object demographics
             "host-objects-traced "
             "host-objects-deleted "
             ")\n ("
             // Cumulative: storage
             "bytes-allocated "
             "bytes-in-heap-peak "
             "pages-in-heap-peak "
             "markstack-segment-peak "
             "symbols-peak "
             "quick-hits "
             "quick-misses "
             "small-objects "
             "large-objects "
             "very-large-objects "
             "markstack-exhaustions "
             "dynamic-roots-peak "
             "overshoot-percent-avg"
             ")\n ("
             // Cumulative: object demographics
             "cnt_ES_Object "
             "cnt_ES_Array "
             "cnt_ES_Function "
             "cnt_ES_Builtin_Function "
             "cnt_ES_Foreign_Object "
             "cnt_ES_Foreign_Function "
             "cnt_ES_Global_Object "
             "cnt_ES_RegExp "
             "cnt_ES_Compiled_Code "
             "cnt_ES_Boxed_Array "
             "cnt_ES_Lexical_Rib "
             "cnt_ES_Reified_Rib "
             "cnt_ES_Latent_Arguments "
             "cnt_JString "
             "cnt_JStringStorage "
             "cnt_ES_Property_Object "
             "cnt_ES_PropertyBucket "
             "cnt_ES_PropertyTable "
             "demo_JStringStorage "
             "demo_ES_Lexical_Rib "
             "demo_ES_Boxed_Array "
             "))\n\n");

    // Sizes of the various object and value types
    fprintf(GC_DATA.trace_fp,
             "; ("
             "(ES_Object %d) "
             "(ES_Array %d) "
             "(ES_Function %d) "
             "(ES_Builtin_Function %d) "
             "(ES_Foreign_Object %d) "
             "(ES_Foreign_Function %d) "
             "(ES_Global_Object %d) "
             "(ES_RegExp %d) "
             "(ES_Compiled_Code %d) "
             "(ES_Boxed_Array %d) "
             "(ES_Lexical_Rib %d) "
             "(ES_Reified_Rib %d) "
             "(ES_Latent_Arguments %d) "
             "(JString %d) "
             "(JStringStorage %d) "
             "(ES_Property_Object %d) "
             "(JS_Value %d) "
             ")\n\n",
             sizeof(ES_Object),
             sizeof(ES_Array),
             sizeof(ES_Function),
             sizeof(ES_Builtin_Function),
             sizeof(ES_Foreign_Object),
             sizeof(ES_Foreign_Function),
             sizeof(ES_Global_Object),
             sizeof(ES_RegExp),
             sizeof(ES_Compiled_Code),
             sizeof(ES_Boxed_Array),
             sizeof(ES_Lexical_Rib),
             sizeof(ES_Reified_Rib),
             sizeof(ES_Latent_Arguments),
             sizeof(JString),
             sizeof(JStringStorage),
             sizeof(ES_Property_Object),
             sizeof(JS_Value));
    fflush(GC_DATA.trace_fp);
}

/* static */ void
ESMM::WriteTraceRecord(GC_Reason reason,
                        unsigned long bytes_live_before, unsigned long bytes_free_before,
                        unsigned long bytes_in_heap_before, unsigned long pages_in_heap_before)
{
    int i=GC_DDATA.gc_history_pointer;
    if (i==0)
        i=ARRAY_SIZE(GC_DDATA.ms_collecting_history)-1;
    else
        i=i-1;
    fprintf(GC_DATA.trace_fp, "((%d %f %f %f %lu %lu %lu %lu %d %lu %lu %lu %lu %lu %lu %3.1f)\n",
             (int)reason,
             GC_DDATA.ms_collecting_history[i],
             GC_DDATA.ms_tracing_history[i],
             GC_DDATA.ms_sweeping_history[i],
             bytes_live_before,
             GC_DATA.bytes_live_after_gc,
             bytes_free_before,
             GC_DATA.bytes_free_after_gc,
             bytes_limit,
             bytes_offline_limit,
             bytes_in_heap_before,
             GC_DATA.bytes_in_heap,
             pages_in_heap_before,
             GC_DATA.pages_in_heap,
             GC_DATA.markstack_inuse_peak,
             IDENTIFIER_POOL.Population(),
             double(bytes_free_before) / double(bytes_live_before) * 100.0
             );

    fprintf(GC_DATA.trace_fp, " (%lu %lu)\n",
             GC_DDATA.host_objects_traced,
             GC_DDATA.host_objects_deleted
             );

    fprintf(GC_DATA.trace_fp, " (%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %2.1f)\n",
             GC_DDATA.bytes_allocated,
             GC_DDATA.bytes_in_heap_peak,
             GC_DDATA.pages_in_heap_peak,
             GC_DDATA.markstack_segment_peak,
             GC_DDATA.symbols_peak,
             GC_DDATA.quicklist_hits,
             GC_DDATA.quicklist_misses,
             GC_DDATA.small_objects,
             GC_DDATA.large_objects,
             GC_DDATA.very_large_objects,
             GC_DDATA.markstack_exhaustions,
             GC_DDATA.dynamic_roots_peak,
             GC_DDATA.overshoot_percent_avg*100.0);

    fprintf(GC_DATA.trace_fp, " (%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu ",
             GC_DDATA.cnt_ES_Object,
             GC_DDATA.cnt_ES_Array,
             GC_DDATA.cnt_ES_Function,
             GC_DDATA.cnt_ES_Builtin_Function,
             GC_DDATA.cnt_ES_Foreign_Object,
             GC_DDATA.cnt_ES_Foreign_Function,
             GC_DDATA.cnt_ES_Global_Object,
             GC_DDATA.cnt_ES_RegExp,
             GC_DDATA.cnt_ES_Compiled_Code,
             GC_DDATA.cnt_ES_Boxed_Array,
             GC_DDATA.cnt_ES_Lexical_Rib,
             GC_DDATA.cnt_ES_Reified_Rib,
             GC_DDATA.cnt_ES_Latent_Arguments,
             GC_DDATA.cnt_JString,
             GC_DDATA.cnt_JStringStorage,
             GC_DDATA.cnt_ES_Property_Object,
             GC_DDATA.cnt_ES_PropertyBucket,
             GC_DDATA.cnt_ES_PropertyTable,
             0UL
             );

    fprintf(GC_DATA.trace_fp, "(");
    for (i=0 ; i < 32 ; i++)
        fprintf(GC_DATA.trace_fp, "%lu ", GC_DDATA.demo_JStringStorage[i]);
    fprintf(GC_DATA.trace_fp, ") ");

    fprintf(GC_DATA.trace_fp, "(");
    for (i=0 ; i < 32 ; i++)
        fprintf(GC_DATA.trace_fp, "%lu ", GC_DDATA.demo_ES_Lexical_Rib[i]);
    fprintf(GC_DATA.trace_fp, ") ");

    fprintf(GC_DATA.trace_fp, "(");
    for (i=0 ; i < 32 ; i++)
        fprintf(GC_DATA.trace_fp, "%lu ", GC_DDATA.demo_ES_Boxed_Array[i]);
    fprintf(GC_DATA.trace_fp, ") ");

    fprintf(GC_DATA.trace_fp, "))\n");

    fflush(GC_DATA.trace_fp);
}

unsigned long
ESMM::CountBytesFree()
{
    // Bytes free is sum of data on quick lists + sum of data on free list
    unsigned long nbytes = 0;
    ES_Free *p;
    int i;

    ES_Free** quicklists;
    quicklists = GC_DATA.quicklists;
    for (i=0 ; i < QUICK_CUTOFF_UNITS ; i++)
        for (p = quicklists[i]; p != NULL ; p = p->next)
            nbytes += p->ObjectSize();

    for (p=GC_DATA.freelist ; p != NULL ; p = p->next)
        nbytes += p->ObjectSize();

    return nbytes;
}
#endif // ES_DBG_TRACE_GC

#ifdef ES_DBG_DEMOGRAPHICS
// FIXME  Debug code.  Object demographics not yet implemented for the new arena.
// The GC must update the demographics information when destroying an object.

/* Object demographics are maintained in a matrix where the row is the
   number of named properties and the column is the number of indexed
   properties (including private properties for host objects).  If an
   object has 3 named and 5 indexed properties at destruction, then
   the entry [3][5] is incremented by 1.

   The last bin includes data for objects larger than that size.
   */
size_t property_demographics[100][100];
BOOL firsttime = TRUE;

ES_Object::~ES_Object()    // Obviously wrong, now.
{
    size_t named, indexed;

    ::CountProperties(&properties, &named, &indexed);
    named_properties += named;
    indexed_properties += indexed;

    if (named_properties > 99) named_properties = 99;
    if (indexed_properties > 99) indexed_properties = 99;

    property_demographics[named_properties][indexed_properties] += 1;
}

void WriteObjectDemographics()
{
    FILE *fp = fopen("C:/klient/demos.txt", "a");
    if (fp == NULL)
    {
        OP_ASSERT(!"Failed to open file");
        return;
    }

    if (firsttime)
        fprintf(fp, "\n\n===========================================\n\n");
    firsttime = FALSE;
    fprintf(fp, "(");
    for (int i=0 ; i < 100 ; i++)
    {
        BOOL any=FALSE;
        for (int j=0 ; j < 100 ; j++)
        {
            if (property_demographics[i][j] > 0)
            {
                fprintf(fp, "(%d %d %d) ", i, j, property_demographics[i][j]);
                any = TRUE;
            }
        }
        if (any)
            fprintf(fp, "\n ");
    }
    fprintf(fp, ")\n\n");
    fclose(fp);
}
#endif // ES_DBG_DEMOGRAPHICS

static void
ES_TraceCaches(ES_Heap *heap, ES_Code::PropertyCache *caches, unsigned count)
{
    for (unsigned index = 0; index < count; ++index)
    {
        ES_Code::PropertyCache *cache = &caches[index];

        do
        {
            GC_PUSH_BOXED(heap, cache->data.boxed);
            GC_PUSH_BOXED(heap, cache->object_class);
        }
        while ((cache = cache->next) != NULL);
    }
}

void
ESMM::TraceObject(ES_Heap *heap, ES_Boxed *o)
{
    if (o->GCTag() >= GCTAG_ES_Object)
    {
        // o is derived from ES_Object

        ES_Object* eo = static_cast<ES_Object *>(o);

        eo->ClearProxyInstanceAdded();

        // Mark class and properties first, all objects have them.

        GC_PUSH_BOXED(heap, eo->klass);

        if (eo->properties && !eo->IsVariablesObject())
        {
            SetMarked(eo->GetProperties());
            if (unsigned property_count = eo->Count())
            {
                ES_Property_Table *t = eo->Class()->GetPropertyTable();

                for (unsigned index = 0; index < property_count; ++index)
                {
                    ES_Layout_Info l = t->GetLayoutInfoAtIndex(ES_LayoutIndex(index));
                    if (l.IsBoxedType())
                        GC_PUSH_BOXED(heap, eo->GetBoxed(l));
                    else if (l.IsUntyped())
                    {
                        OP_ASSERT(!l.IsSpecialType());
                        GC_PUSH_VALUE(heap, *eo->GetValue_loc(l));
                    }
                }
            }
        }

        GC_PUSH_BOXED(heap, eo->indexed_properties);

        // If the object is foreign or function, it requires extra processing

        if (eo->IsHostObject(ES_Object::IncludeInactive))
        {
            if (eo->IsGlobalObject())
            {
                ES_Global_Object* g = static_cast<ES_Global_Object *>(eo);
                unsigned i;

                for (i = 0; i < ES_Global_Object::CI_MAXIMUM; ++i)
                    GC_PUSH_BOXED(heap, g->classes[i]);

                for (i = 0; i < ES_Global_Object::PI_MAXIMUM; ++i)
                    GC_PUSH_BOXED(heap, g->prototypes[i]);

                GC_PUSH_BOXED(heap, g->variables);
                for (i = 0; i < g->variables->GetCount(); ++i)
                    GC_PUSH_VALUE(heap, g->variable_values[i]);

                GC_PUSH_BOXED(heap, g->String_prototype_toString.object);
                GC_PUSH_BOXED(heap, g->Number_prototype_valueOf.object);
                GC_PUSH_BOXED(heap, g->cached_parsed_date);

                /* Forget instead of keeping alive: */
                g->dynamic_regexp_cache = NULL;
                g->dynamic_regexp_cache_g = NULL;
                g->dynamic_regexp_cache_i = NULL;
                g->dynamic_regexp_cache_gi = NULL;

                GC_PUSH_BOXED(heap, g->special_stacktrace);
                GC_PUSH_BOXED(heap, g->special_stack);

                GC_PUSH_BOXED(heap, g->special_function_name);
                GC_PUSH_BOXED(heap, g->special_function_length);
                GC_PUSH_BOXED(heap, g->special_function_prototype);
                GC_PUSH_BOXED(heap, g->special_function_arguments);
                GC_PUSH_BOXED(heap, g->special_function_caller);
                GC_PUSH_BOXED(heap, g->special_function_throw_type_error);

                GC_PUSH_BOXED(heap, g->default_function_properties);
                GC_PUSH_BOXED(heap, g->default_builtin_function_properties);
                GC_PUSH_BOXED(heap, g->default_strict_function_properties);
            }

            if (eo->IsHostFunctionObject())
                GC_PUSH_BOXED(heap, static_cast<ES_Host_Function *>(eo)->function_name);

            ES_Host_Object* f = static_cast<ES_Host_Object *>(eo);
            if (f->GetHostObject() != NULL)
            {
#ifdef ES_DBG_GC
                GC_DDATA.host_objects_traced++;
#endif
                EcmaScript_Object *object = f->GetHostObject();
                object->SetNativeObject(f);

                ES_Object *reaper = *object->GetRuntime()->runtime_reaper;
                GC_PUSH_BOXED(heap, reaper);
                object->GetRuntime()->runtime_reaper->SetNativeObject(reaper);

                if (f->MustTraceForeignObject())
                    object->GCTrace();
            }
        }
        else
        {
            OP_ASSERT(!eo->IsGlobalObject());

            switch (o->GCTag())
            {
            case GCTAG_ES_Object_Function:
                {
                    ES_Function *function = static_cast<ES_Function *>(eo);

                    if (function->function_code)
                    {
                        GC_PUSH_BOXED(heap, function->function_code);
                        GC_PUSH_BOXED(heap, function->data.native.unused_arguments);
                    }
                    else if (function->IsBoundFunction())
                    {
                        ES_BoundFunction *bound = static_cast<ES_BoundFunction *>(function);

                        GC_PUSH_BOXED(heap, bound->GetTargetFunction());
                        GC_PUSH_VALUE(heap, bound->GetBoundThis());

                        for (unsigned index = 0; index < bound->GetBoundArgsCount(); ++index)
                            GC_PUSH_VALUE(heap, bound->GetBoundArgs()[index]);
                    }

                    GC_PUSH_BOXED(heap, function->GetGlobalObject());

                    for (unsigned index = 0; index < function->GetScopeChainLength(); ++index)
                        GC_PUSH_BOXED(heap, function->GetScopeChain()[index]);
                }
                break;

            case GCTAG_ES_Object_Arguments:
                {
                    ES_Arguments_Object *arguments = static_cast<ES_Arguments_Object *>(eo);
                    GC_PUSH_BOXED(heap, arguments->GetFunction());
                }
                break;

            case GCTAG_ES_Object_String:
                {
                    ES_String_Object *string = static_cast<ES_String_Object *>(eo);
                    GC_PUSH_BOXED(heap, string->GetValue());
                }
                break;

            case GCTAG_ES_Object_RegExp:
                {
                    ES_RegExp_Object *regexp = static_cast<ES_RegExp_Object *>(eo);
                    GC_PUSH_BOXED(heap, regexp->last_ctor);
                    GC_PUSH_BOXED(heap, regexp->dynamic_match_array);

                    /* Weak reference: invalidate the cache instead of keeping the string alive. */
                    regexp->previous_string_positive = NULL;
                    regexp->previous_string_negative[0] = NULL;
                    regexp->previous_string_negative[1] = NULL;
                }
                break;

            case GCTAG_ES_Object_RegExp_CTOR:
                {
                    ES_RegExp_Constructor *ctor = static_cast<ES_RegExp_Constructor *>(eo);
                    GC_PUSH_BOXED(heap, ctor->object);
                    GC_PUSH_BOXED(heap, ctor->string);
                }
                break;

            case GCTAG_ES_Object_Error:
                {
                    ES_Error *error = static_cast<ES_Error *>(eo);

                    for (unsigned index = 0; index < error->stacktrace_length; ++index)
                        GC_PUSH_BOXED(heap, error->stacktrace[index].code);

                    error->stacktrace_strings[0] = NULL;
                    error->stacktrace_strings[1] = NULL;
                }
                break;
            case GCTAG_ES_Object_TypedArray:
                {
                    ES_TypedArray *typed_array = static_cast<ES_TypedArray *>(eo);
                    GC_PUSH_BOXED(heap, typed_array->array_buffer);
                    GC_PUSH_BOXED(heap, typed_array->next);
                }
                break;
            case GCTAG_ES_Object_ArrayBuffer:
                {
                    ES_ArrayBuffer *array_buffer = static_cast<ES_ArrayBuffer *>(eo);
                    GC_PUSH_BOXED(heap, array_buffer->typed_array_view);
                }
                break;
            }
        }
    }
    else
    {
        // Not derived from ES_Object
        // OPTIMIZEME: rearrange cases to match frequency of occurrence
        unsigned int gctag = o->GCTag();

        switch (gctag)
        {
        case GCTAG_JString:
            if (IsSegmented(static_cast<JString *>(o)))
            {
                JStringSegmented *seg = GetSegmented(static_cast<JString *>(o));
                GC_PUSH_BOXED(heap, seg);
                SetSegmented(static_cast<JString *>(o), seg);
            }
            else
                GC_PUSH_BOXED(heap, static_cast<JString *>(o)->value);
            break;

        case GCTAG_JStringSegmented:
            {
                JStringSegmented *segmented = static_cast<JStringSegmented *>(o);
                JStringStorage **base = segmented->Bases();
                for (unsigned index = 0; index < segmented->nsegments; ++index)
                    GC_PUSH_BOXED(heap, base[index]);
            }
            break;

        case GCTAG_ES_Identifier_Array:
            {
                ES_Identifier_Array *array = static_cast<ES_Identifier_Array *>(o);
                JString **identifiers = array->identifiers;
                for (unsigned index = 0; index < array->nused; ++index)
                    GC_PUSH_BOXED(heap, identifiers[index]);
            }
            break;

        case GCTAG_ES_IdentifierCell_Array:
            {
                ES_IdentifierCell_Array *array = static_cast<ES_IdentifierCell_Array *>(o);
                ES_IdentifierCell_Array::Cell *cells = array->cells;
                for (unsigned index = 0; index < array->size; ++index)
                    GC_PUSH_BOXED(heap, cells[index].key);
            }
            break;

        case GCTAG_ES_Property_Table:
            {
                GC_PUSH_BOXED(heap, static_cast<ES_Property_Table *>(o)->properties);
                GC_PUSH_BOXED(heap, static_cast<ES_Property_Table *>(o)->property_info);
                GC_PUSH_BOXED(heap, static_cast<ES_Property_Table *>(o)->layout_info);
            }
            break;

        case GCTAG_ES_Property_Value_Table:
            {
                ES_Property_Value_Table *table = static_cast<ES_Property_Value_Table *>(o);
                GC_PUSH_BOXED(heap, table->properties);
                for (unsigned index = 0; index < table->used; ++index)
                    GC_PUSH_VALUE(heap, table->property_value[index].value);
            }
            break;

        case GCTAG_ES_Compact_Indexed_Properties:
            {
                ES_Compact_Indexed_Properties *compact = static_cast<ES_Compact_Indexed_Properties *>(o);
                ES_Value_Internal *values = compact->values;
                for (unsigned index = 0, capacity = compact->capacity; index < capacity; ++index)
                    GC_PUSH_VALUE(heap, values[index]);
            }
            break;

        case GCTAG_ES_Sparse_Indexed_Properties:
            {
                ES_Sparse_Indexed_Properties *sparse = static_cast<ES_Sparse_Indexed_Properties *>(o);

                GC_PUSH_BOXED(heap, sparse->blocks);

                ES_Box **blocks = reinterpret_cast<ES_Box **>(sparse->blocks->Unbox());

                for (unsigned block_index = 0; block_index < sparse->blocks_used; ++block_index)
                {
                    GC_PUSH_BOXED(heap, blocks[block_index]);

                    ES_Sparse_Indexed_Properties::Node *nodes = reinterpret_cast<ES_Sparse_Indexed_Properties::Node *>(blocks[block_index]->Unbox());

                    for (unsigned node_index = 0; node_index < ES_Sparse_Indexed_Properties::NODES_PER_BLOCK; ++node_index)
                        if (nodes[node_index].index != UINT_MAX)
                            GC_PUSH_VALUE(heap, nodes[node_index].value);
                }
            }
            break;

        case GCTAG_ES_Byte_Array_Indexed:
            {
                ES_Byte_Array_Indexed *byte_array = static_cast<ES_Byte_Array_Indexed *>(o);
                heap->RecordExternalBytes(byte_array->capacity);
            }
            break;
        case GCTAG_ES_Type_Array_Indexed:
            {
                ES_Type_Array_Indexed *typed_array = static_cast<ES_Type_Array_Indexed *>(o);
                GC_PUSH_BOXED(heap, typed_array->byte_array);
            }
            break;

        case GCTAG_ES_Identifier_Boxed_Hash_Table:
            GC_PUSH_BOXED(heap, static_cast<ES_Identifier_Boxed_Hash_Table *>(o)->array);

        case GCTAG_ES_Identifier_Hash_Table:
            {
                ES_Identifier_Hash_Table *table = static_cast<ES_Identifier_Hash_Table *>(o);
                GC_PUSH_BOXED(heap, table->cells);
            }
            break;

        case GCTAG_ES_Identifier_List:
            {
                ES_Identifier_List *list = static_cast<ES_Identifier_List *>(o);
                GC_PUSH_BOXED(heap, list->indices);
                GC_PUSH_BOXED(heap, list->identifiers);
            }
            break;

        case GCTAG_ES_Boxed_Array:
            {
                ES_Boxed_Array *ba = static_cast<ES_Boxed_Array *>(o);
                for (unsigned index = 0, limit = ba->nused; index < limit; ++index)
                    GC_PUSH_BOXED(heap, ba->slots[index]);
            }
            break;

        case GCTAG_ES_Boxed_List:
            {
                ES_Boxed_List *bl = static_cast<ES_Boxed_List *>(o);
                GC_PUSH_BOXED(heap, bl->head);
                GC_PUSH_BOXED(heap, bl->tail);
            }
            break;

        case GCTAG_ES_Class_Node:
        case GCTAG_ES_Class_Compact_Node:
        case GCTAG_ES_Class_Compact_Node_Frozen:
        case GCTAG_ES_Class_Singleton:
        case GCTAG_ES_Class_Hash:
        {
            ES_Class *cls = static_cast<ES_Class *>(o);
            GC_PUSH_BOXED(heap, cls->property_table);
            GC_PUSH_BOXED(heap, cls->static_data);
            GC_PUSH_BOXED(heap, cls->sibling);
            GC_PUSH_BOXED(heap, cls->extra);

            switch (gctag)
            {
            case GCTAG_ES_Class_Node:
            {
                ES_Class_Node *cls = static_cast<ES_Class_Node *>(o);
                GC_PUSH_BOXED(heap, cls->parent);
                GC_PUSH_BOXED(heap, cls->children);
                break;
            }
            case GCTAG_ES_Class_Compact_Node:
            case GCTAG_ES_Class_Compact_Node_Frozen:
            {
                ES_Class_Compact_Node *cls = static_cast<ES_Class_Compact_Node *>(o);
                GC_PUSH_BOXED(heap, cls->parent);
                GC_PUSH_BOXED(heap, cls->children);
                break;
            }
            case GCTAG_ES_Class_Singleton:
            {
                ES_Class_Singleton *cls = static_cast<ES_Class_Singleton *>(o);
                GC_PUSH_BOXED(heap, cls->data);
                GC_PUSH_BOXED(heap, cls->GetSerialsBox());
                GC_PUSH_BOXED(heap, cls->GetHashTable());

                if (cls->instances)
                {
                    if (!cls->instances->IsClass())
                    {
                        /* If the singleton has instances then we know
                           that it is rooted in the list of prototypes
                           in the heap. In that case we consider the
                           references from instances to be weak, and
                           should be resolved after tracing is
                           complete. We still need to traverse the
                           structure of the singleton class and mark
                           it as live, but we do not want to mark
                           reachable classes as live.
                        */

                        SetMarked(cls->instances);
                        OP_ASSERT(cls->instances->GCTag() == GCTAG_ES_Identifier_Boxed_Hash_Table);
                        ES_Identifier_Boxed_Hash_Table *table = static_cast<ES_Identifier_Boxed_Hash_Table *>(cls->instances);
                        SetMarked(table->array);
                        SetMarked(table->cells);
                        ES_Boxed_Array *array = table->GetValues();

                        for (unsigned i = 0; i < array->nused; ++i)
                        {
                            if (array->slots[i]->GCTag() == GCTAG_ES_Boxed_List)
                            {
                                ES_Boxed_List *list = static_cast<ES_Boxed_List *>(array->slots[i]);
                                while (list)
                                {
                                    SetMarked(list);
                                    list = list->tail;
                                }
                            }
                            else
                                GC_PUSH_BOXED(heap, array->slots[i]);
                        }
                    }
                    else
                        GC_PUSH_BOXED(heap, cls->instances);
                }

                break;
            }
            case GCTAG_ES_Class_Hash:
            {
                ES_Class_Hash *cls = static_cast<ES_Class_Hash *>(o);
                GC_PUSH_BOXED(heap, cls->klass);
                GC_PUSH_BOXED(heap, cls->data);
                GC_PUSH_BOXED(heap, cls->GetSerialsBox());
                GC_PUSH_BOXED(heap, cls->GetHashTable());
                break;
            }
            default:
                break;
            }
            break;
        }

        case GCTAG_ES_Class_Extra:
            {
                ES_Class_Extra *extra = static_cast<ES_Class_Extra *>(o);
                GC_PUSH_BOXED(heap, extra->klass);
                GC_PUSH_BOXED(heap, extra->next);
            }
            break;

        case GCTAG_ES_Class_Data:
            {
                ES_Class_Data *cld = static_cast<ES_Class_Data *>(o);

                GC_PUSH_BOXED(heap, cld->root);
                GC_PUSH_BOXED(heap, cld->main);
                GC_PUSH_BOXED(heap, cld->prototype);
                /* NOTE: object_name_string is now assumed to always be 'shared',
                   and live in a separate heap (where it is separately kept alive),
                   hence tracing it from ES_Class_Data is inappropriate and not required. */

                if (cld->instance)
                    if (cld->instance->GCTag() >= GCTAG_ES_Object)
                        GC_PUSH_BOXED(heap, cld->instance);
                    else
                    {
                        ES_Boxed_List *first = static_cast<ES_Boxed_List *>(cld->instance);
                        ES_Boxed_List **pointer = &first;
                        while (*pointer)
                        {
                            ES_Boxed_List *list = *pointer;
                            if (list->head->IsClass())
                            {
                                ES_Class::InvalidateTree(static_cast<ES_Class *>(list->head));
                                *pointer = list->tail;
                            }
                            else
                            {
                                GC_PUSH_BOXED(heap, list);
                                pointer = &list->tail;
                            }
                        }
                        cld->instance = first;
                    }
            }
            break;

        case GCTAG_ES_ProgramCode:
        case GCTAG_ES_FunctionCode:
            {
                ES_Code *code = static_cast<ES_Code *>(o);
                ES_CodeStatic *data = code->data;
                unsigned index;

                GC_PUSH_BOXED(heap, code->global_object);
                GC_PUSH_BOXED(heap, code->parent_code);

                if (!code->data->source_storage_owner)
                    GC_PUSH_BOXED(heap, code->data->source_storage);
                if (!code->data->string_storage_owner)
                    GC_PUSH_BOXED(heap, code->data->string_storage);

                for (index = 0; index < data->strings_count; ++index)
                    GC_PUSH_BOXED(heap, code->strings[index]);
                for (index = 0; index < data->regexps_count; ++index)
                    GC_PUSH_BOXED(heap, code->regexps[index]);

                for (index = 0; index < data->object_literal_classes_count; ++index)
                    GC_PUSH_BOXED(heap, code->object_literal_classes[index]);

                if (code->object_literal_prototype_classes)
                    for (index = 0; index < ES_Code_ClassTable::MAX_OBJECT_LITERAL_COMPACT_CLASSES; ++index)
                        GC_PUSH_BOXED(heap, code->object_literal_prototype_classes->table[index]);

                ES_Compact_Indexed_Properties **cal = code->constant_array_literals;
                for (index = 0; index < data->constant_array_literals_count; ++index, ++cal)
                    GC_PUSH_BOXED(heap, *cal);

                for (index = 0; index < data->functions_count; ++index)
                    GC_PUSH_BOXED(heap, code->functions[index]);

                GC_PUSH_BOXED(heap, code->url);

                ES_TraceCaches(heap, code->property_get_caches, data->property_get_caches_count);
                ES_TraceCaches(heap, code->property_put_caches, data->property_put_caches_count);

#ifdef ES_NATIVE_SUPPORT
                for (index = 0; index < data->loop_data_count; ++index)
                    GC_PUSH_BOXED(heap, code->loop_dispatcher_codes[index]);
#endif // ES_NATIVE_SUPPORT

                /* Instead of keeping the objects referenced by these caches
                   alive by tracing them, we just reset them.  GC runs seldom,
                   so a frequently used cache's hit frequency will be almost
                   unaffected, and it avoids the problem that an unfrequently
                   used cache keeps an expensive object alive forever. */
                for (index = 0; index < data->format_string_caches_count; ++index)
                    code->format_string_caches[index].from = code->format_string_caches[index].to = NULL;
                for (index = 0; index < data->eval_caches_count; ++index)
                {
                    code->eval_caches[index].source = NULL;
                    code->eval_caches[index].code = NULL;
                }

                for (index = 0; index < code->scope_chain_length; ++index)
                    GC_PUSH_BOXED(heap, code->scope_chain[index]);

                if (o->GCTag() == GCTAG_ES_ProgramCode)
                {
                    ES_ProgramCode *pc = static_cast<ES_ProgramCode *>(code);
                    GC_PUSH_BOXED(heap, pc->long_string_literal_table);
                }
                else
                {
                    ES_FunctionCode *fc = static_cast<ES_FunctionCode *>(code);
                    GC_PUSH_BOXED(heap, fc->klass);
                }
            }
            break;

        case GCTAG_ES_Properties:
            {
                ES_Properties *prop = static_cast<ES_Properties*>(o);
                for (unsigned index = 0; index < prop->used; ++index)
                    GC_PUSH_VALUE(heap, prop->slots[index]);
            }
            break;

        case GCTAG_ES_Special_Aliased:
            //GC_PUSH_VALUE(heap, *static_cast<ES_Special_Aliased *>(o)->property);
            break;

        case GCTAG_ES_Special_Mutable_Access:
            GC_PUSH_BOXED(heap, static_cast<ES_Special_Mutable_Access *>(o)->getter);
            GC_PUSH_BOXED(heap, static_cast<ES_Special_Mutable_Access *>(o)->setter);
            break;

        case GCTAG_ES_Special_Global_Variable:
        case GCTAG_ES_Special_Function_Name:
        case GCTAG_ES_Special_Function_Length:
        case GCTAG_ES_Special_Function_Prototype:
        case GCTAG_ES_Special_Function_Arguments:
        case GCTAG_ES_Special_Function_Caller:
        case GCTAG_ES_Special_RegExp_Capture:
        case GCTAG_ES_Special_Error_StackTrace:
            break;

        default:
            OP_ASSERT(!"Unknown object type");
        }
    }
}

/* static */ void
ESMM::DestroyObject(ES_Boxed *o)
{
    GC_Tag gctag = o->GCTag();

    if (gctag == GCTAG_ES_ProgramCode)
        ES_ProgramCode::Destroy(static_cast<ES_ProgramCode *>(o));
    else if (gctag == GCTAG_ES_FunctionCode)
        ES_FunctionCode::Destroy(static_cast<ES_FunctionCode *>(o));
    else if (gctag == GCTAG_ES_Byte_Array_Indexed)
        ES_Byte_Array_Indexed::Destroy(static_cast<ES_Byte_Array_Indexed *>(o));
    else if (gctag >= GCTAG_ES_Object)
    {
        ES_Object *eo = static_cast<ES_Object *>(o);

#ifdef ECMASCRIPT_DEBUGGER
        if (eo->IsTrackedByDebugger() && g_ecmaManager->GetDebugListener())
            g_ecmaManager->GetDebugListener()->DestroyObject(eo);
#endif // ECMASCRIPT_DEBUGGER

        if (eo->IsGlobalObject())
        {
            ES_Global_Object *go = static_cast<ES_Global_Object *>(eo);
            ES_Global_Object::Destroy(go);
            ES_Host_Object::Destroy(go);
        }
        else if (eo->IsHostObject(ES_Object::IncludeInactive))
        {
#ifdef ES_DBG_GC
            GC_DDATA.host_objects_deleted++;
#endif // ES_DBG_GC

            ES_Host_Object::Destroy(static_cast<ES_Host_Object *>(eo));
        }
        else if (eo->IsFunctionObject())
        {
            OP_ASSERT(!eo->IsHostObject());
            ES_Function::Destroy(static_cast<ES_Function *>(eo));
        }
        else if (eo->IsRegExpObject())
            ES_RegExp_Object::Destroy(static_cast<ES_RegExp_Object *>(eo));
        else if (eo->IsTypedArrayObject())
            ES_TypedArray::Destroy(static_cast<ES_TypedArray *>(eo));
    }

#ifdef ES_HARDCORE_GC_MODE
    if (g_hardcode_gc_mode)
        op_memset(o + 1, 0xfe, ObjectSize(o) - sizeof *o);
#endif // ES_HARDCORE_GC_MODE

    o->InitGCTag(GCTAG_free);
}

/* static */ void
ESMM::SweepRuntimes(ES_Runtime *&first_runtime)
{
    ES_Runtime **ptr = &first_runtime;

    while (ES_Runtime *runtime = *ptr)
        if (!runtime->runtime_reaper)
        {
            *ptr = runtime->next_runtime_per_heap;
            OP_DELETE(runtime);
        }
        else
            ptr = &runtime->next_runtime_per_heap;
}

OP_STATUS
ES_ObjectReference::Protect(ES_Runtime *runtime, ES_Object *object0)
{
    if (object == object0)
        return OpStatus::OK;

    Unprotect();

    if (runtime->heap->AddDynamicRoot(object0))
    {
        heap = runtime->heap->GetReference();
        object = object0;
        return OpStatus::OK;
    }
    else
        return OpStatus::ERR_NO_MEMORY;
}

void
ES_ObjectReference::Unprotect()
{
    if (object)
    {
        heap->GetHeap()->RemoveDynamicRoot(object);

        heap = NULL;
        object = NULL;
    }
}

#ifdef ES_HEAP_DEBUGGER

unsigned *g_ecmaLiveObjects;

#endif // ES_HEAP_DEBUGGER
