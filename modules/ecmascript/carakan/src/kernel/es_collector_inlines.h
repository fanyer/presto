/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2001 - 2006
 *
 * ECMAScript engine -- garbage collector inline functions
 *
 * @author Lars T Hansen
 */

#ifndef ES_COLLECTOR_INLINES_H
#define ES_COLLECTOR_INLINES_H

inline void
ES_RootData::AddTo(ES_Heap *heap)
{
    this->collection = heap->root_collection;
    this->root_next = heap->root_collection->root_objs.root_next;
    this->root_prev = &heap->root_collection->root_objs;
    heap->root_collection->root_objs.root_next->root_prev = this;
    heap->root_collection->root_objs.root_next = this;
}

inline
ES_GCRoot::ES_GCRoot(ES_Heap *heap)
{
    AddTo(heap);
}

inline ES_Free *
MakeFree(ES_Boxed *b, unsigned int nbytes)
{
    OP_ASSERT(nbytes >= sizeof(ES_Free));

    ES_Free *f = static_cast<ES_Free *>(b);

#ifdef ES_HARDCORE_GC_MODE
    if (g_hardcode_gc_mode)
        op_memset(b, 0xfe, nbytes);
#endif // ES_HARDCORE_GC_MODE
    OP_ASSERT(nbytes <= UINT16_MAX);

    f->SetHeader(nbytes, GCTAG_free);
    return f;
}

inline ES_Free *
MakeLargeFree(ES_Boxed *b)
{
    ES_Free *f = static_cast<ES_Free *>(b);

    f->SetHeader(UINT16_MAX, GCTAG_free | ES_Header::MASK_ON_LARGE_PAGE);
    return f;
}

inline BOOL
ES_Heap::AddDynamicRoot(ES_Boxed *obj)
{
    OP_STATUS r = 0;
    TRAP_AND_RETURN_VALUE_IF_ERROR(r, root_collection->dynamic_roots->Push(obj), FALSE);
    return TRUE;
}

inline void
ES_Heap::RemoveDynamicRoot(ES_Boxed *obj)
{
    root_collection->dynamic_roots->Remove(CAST_TO_BOXED(obj));
    if (root_collection->dynamic_roots->Size() == 0 && root_collection->root_objs.root_next == &root_collection->root_objs)
    {
        RegisterActivity();
        g_ecmaManager->MoveHeapToDestroyList(this);
    }
}

inline BOOL
ES_Heap::IsLocked()
{
    return locked != 0;
}

inline BOOL
ES_Heap::IsCollecting()
{
    return in_collector;
}

inline BOOL
ES_Heap::HasReasonableLockValue()
{
    return locked < 100000;
}

inline void
ES_Heap::Lock()
{
    locked++;
}

inline void
ES_Heap::Unlock()
{
    locked = 0;
}

inline void
ES_Heap::SetNeedsGC()
{
    external_needs_gc = TRUE;
}

inline void
ES_Heap::CollectIfNeeded(ES_Context *context)
{
    if (needs_gc && locked == 0)
        ForceCollect(context, GC_REASON_ALLOCLIMIT);
}

# define GC_ALIGN(size) static_cast<unsigned>(((size) + (ES_LIM_ARENA_ALIGN - 1)) & ~(ES_LIM_ARENA_ALIGN - 1))

inline ES_Boxed *
ES_Heap::Allocate(ES_Context *context, unsigned nbytes)
{
    HEAP_DEBUG_LIVENESS_FOR(this);

    // Enforce a minimum object size, then round up to an ES_LIM_ARENA_ALIGN boundary.
    // The upshot is that objects are at least 16 bytes (including header), and
    // always allocated with an 8-byte granularity.

    /* The "call" to GC_ALIGN is aligned in the GC_ALLOCATE macro.  Since the
       actual operand is usually a compile time constant, a reasonable compiler
       should be able to perform the alignment compile-time. */
    OP_ASSERT(nbytes == GC_ALIGN(nbytes));
    //nbytes = GC_ALIGN(nbytes);

#ifdef ES_HARDCORE_GC_MODE
    OP_ASSERT(locked || !in_allocate || !needs_gc);
    if (g_hardcode_gc_mode && !in_allocate)
        needs_gc = TRUE;
    in_allocate = TRUE;
#endif // ES_HARDCORE_GC_MODE

    if (needs_gc)
        ForceCollect(context, GC_REASON_ALLOCLIMIT);

    bytes_live += nbytes;

    ES_Boxed *object;

    if (nbytes < LARGE_OBJECT_LIMIT)
    {
#ifdef ES_DBG_GC
        debug_data.small_object_count++;
        debug_data.small_object_bytes += nbytes;
#endif // ES_DBG_GC
        object = reinterpret_cast<ES_Boxed *>(current_top);
        current_top += nbytes;

        if (current_top <= current_limit)
            object->SetHeader(nbytes, 0);
        else
            object = AllocateSmall(context, nbytes);
    }
    else
    {
#ifdef ES_DBG_GC
        debug_data.large_object_count++;
        debug_data.large_object_bytes += nbytes;
#endif // ES_DBG_GC
        object = AllocateLarge(context, nbytes);
    }

#ifdef ES_HARDCORE_GC_MODE
    in_allocate = FALSE;
#endif // ES_HARDCORE_GC_MODE

#ifdef _DEBUG
    if (object)
    {
        OP_ASSERT(!IsDebugMarked(object));
        OP_ASSERT((reinterpret_cast<UINTPTR>(object) & (ES_LIM_ARENA_ALIGN-1)) == 0);
        OP_ASSERT(object->GetPage() == object->GetPage()->GetFirst()->GetPage());
# ifdef ES_MOVABLE_OBJECTS
        object->trace_number = collections_count;
# endif // ES_MOVABLE_OBJECTS
    }
#endif // _DEBUG

    OP_ASSERT(object == NULL || GC_ALIGN(ObjectSize(object)) == ObjectSize(object));
    return object;
}

inline ES_Boxed *
ES_Heap::AllocateSimple(ES_Context *context, unsigned nbytes)
{
    return AllocateSimpleSmall(context, nbytes);
}

/* Allocation macros */

#ifdef ES_DBG_GC
#  define ACCOUNT(the_class)      GC_DDATA.SPLICE(cnt_, the_class)++
#else
#  define ACCOUNT(the_class)      ((void)0)
#endif

# define GC_ALLOCATION_SIZE(the_class) (sizeof(the_class) < sizeof(ES_Free) ? GC_ALIGN(sizeof(ES_Free)) : GC_ALIGN(sizeof(the_class)))

# define GC_ALLOCATE(context, var, the_class, args)    \
    do { \
        the_class* TMP_storage = (the_class*)context->heap->Allocate(context, GC_ALLOCATION_SIZE(the_class)); \
        if (TMP_storage == NULL) \
        { \
            context->AbortOutOfMemory(); \
        } \
        var = TMP_storage; \
        Initialize args; \
        ACCOUNT(the_class); \
    } while(0)

# define GC_ALLOCATE_WITH_EXTRA(context, var, extrabytes, the_class, args) \
    do { \
        the_class* TMP_storage = (the_class*)context->heap->Allocate(context, GC_ALIGN(sizeof(the_class)+(extrabytes))); \
        if (TMP_storage == NULL) \
        { \
            context->AbortOutOfMemory(); \
        } \
        var = TMP_storage; \
        Initialize args; \
        ACCOUNT(the_class); \
    } while(0)

# define GC_ALLOCATION_SIZE_EXTRA(the_class, extrabytes) GC_ALIGN(sizeof(the_class)+(extrabytes))

# define TRY_GC_ALLOCATE_WITH_EXTRA(context, var, extrabytes, the_class, args) \
    do { \
        the_class* TMP_storage = (the_class*)context->heap->Allocate(context, GC_ALLOCATION_SIZE_EXTRA(the_class, extrabytes)); \
        var = TMP_storage; \
        if (TMP_storage != NULL) \
        { \
            Initialize args; \
            ACCOUNT(the_class); \
        } \
    } while(0)

#ifdef ES_GENERATIONAL_COLLECTOR
# define GC_ALLOCATE_WITH_EXTRA_ON_SAME_GEN(context, var, extrabytes, the_class, args , ideal) \
    do { \
        the_class* TMP_storage = (the_class*)static_cast<ES_GenerationalHeap*>(context->heap)->AllocateOnSameGen(context, GC_ALIGN(sizeof(the_class)+(extrabytes)), ideal); \
        if (TMP_storage == NULL) \
        { \
            context->AbortOutOfMemory(); \
        } \
        var = TMP_storage; \
        Initialize args; \
        ACCOUNT(the_class); \
    } while(0)
#else // ES_GENERATIONAL_COLLECTOR
# define GC_ALLOCATE_WITH_EXTRA_ON_SAME_GEN(context, var, extrabytes, the_class, args , ideal) \
    GC_ALLOCATE_WITH_EXTRA(context, var, extrabytes, the_class, args)
#endif // ES_GENERATIONAL_COLLECTOR



# define GC_ALLOCATE_WITH_EXTRA_ALIGNED(context, var_main, var_extra, extrabytes, main_class, extra_class, main_args, extra_args) \
    do { \
        unsigned mainbytes = GC_ALIGN(sizeof(main_class)); \
        main_class* TMP_storage = (main_class*)context->heap->Allocate(context, GC_ALIGN(mainbytes+(extrabytes))); \
        if (TMP_storage == NULL) \
        { \
            context->AbortOutOfMemory(); \
        } \
        var_main = TMP_storage; \
        var_extra = reinterpret_cast<extra_class *>(reinterpret_cast<char *>(TMP_storage) + mainbytes); \
        var_main->SplitAllocation(var_extra); \
        Initialize main_args; \
        Initialize extra_args; \
        ACCOUNT(main_class); \
    } while(0)

# define GC_ALLOCATE_WITH_EXTRA_ALIGNED_PAYLOAD(context, var_main, var_payload, extrabytes, main_class, main_args) \
    do { \
        unsigned mainbytes = GC_ALIGN(sizeof(main_class)); \
        main_class* TMP_storage = (main_class*)context->heap->Allocate(context, GC_ALIGN(mainbytes+(extrabytes))); \
        if (TMP_storage == NULL) \
        { \
            context->AbortOutOfMemory(); \
        } \
        var_main = TMP_storage; \
        Initialize main_args; \
        ACCOUNT(main_class); \
        var_payload = reinterpret_cast<char *>(TMP_storage) + mainbytes; \
    } while(0)

/* Garbage collection macros */

#define DECLARE_MARK_STATE \
    ES_Boxed **stktop, **stklim

#define RESTORE_MARK_STATE \
    do { \
        stktop = markstack->segment->top; \
        stklim = &markstack->segment->elements[ES_PARM_MARKSTACK_SEGMENT_SIZE]; \
    } while (0)

#define SAVE_MARK_STATE \
    markstack->segment->top = stktop

#define GC_PUSH_BOXED(heap, x)                                \
    do                                                        \
        if (x)                                                \
            heap->PushBoxed(x);                               \
    while (0)

#define GC_PUSH_VALUE(heap, expr)               \
    heap->PushValue(expr);

inline BOOL
ES_Heap::Mark(ES_Boxed *object)
{
    if (!IsMarked(object))
    {
        PushBoxed(object);
        if (!inside_marker)
            TraceFromMarkStack();
        return FALSE;
    }
    else
        return TRUE;
}

inline void
ES_Heap::TraceFromRootObjects()
{
    for (ES_RootData *root = root_collection->root_objs.root_next; root != &root_collection->root_objs; root = root->root_next)
    {
        root->GCTrace();
        TraceFromMarkStack();
    }
}

inline void
ES_Heap::TraceFromMarkStack()
{
#ifdef ES_RECURSIVE_TRACING
    // Nothing to do.
    return;
#else // ES_RECURSIVE_TRACING
    inside_marker = TRUE;

    DECLARE_MARK_STATE;
    RESTORE_MARK_STATE;
    (void)stklim;

    while (TRUE)
    {
        ES_Boxed *obj = *(stktop--);
        if (obj == NULL)
            if (markstack->segment->next == NULL)
            {
                ++stktop;
                SAVE_MARK_STATE;
                break;
            }
            else
            {
                markstack->Underflow();
                RESTORE_MARK_STATE;
                obj = *(stktop--);
            }

        SAVE_MARK_STATE;
        ESMM::TraceObject(this, obj);
        RESTORE_MARK_STATE;
    }

    SAVE_MARK_STATE;

    inside_marker = FALSE;
#endif // ES_RECURSIVE_TRACING
}

inline
ES_CollectorLock::ES_CollectorLock(ES_Context *context, BOOL gc_on_destruction/* = FALSE*/)
    : context(context), released(FALSE), gc_on_destruction(gc_on_destruction)
{
    ++context->heap->locked;
}

inline
ES_CollectorLock::~ES_CollectorLock()
{
    if (!released && context->heap->locked != 0)
        if (--context->heap->locked == 0 && context->heap->needs_gc && gc_on_destruction)
            context->heap->ForceCollect(context, GC_REASON_ALLOCLIMIT);
}

inline void
ES_CollectorLock::Release()
{
    OP_ASSERT(released == FALSE);
    if (context->heap->locked != 0)
        --context->heap->locked;
    released = TRUE;
}

inline void
ES_CollectorLock::Acquire()
{
    OP_ASSERT(released == TRUE);
    ++context->heap->locked;
    released = FALSE;
}

#endif // ES_COLLECTOR_INLINES_H
