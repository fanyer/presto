/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2001 - 2006
 *
 * ECMAScript engine -- garbage collector.
 *
 * @author Lars T Hansen
 */

/**
 * @section GC                    Garbage collector
 *
 * @subsection Intro              Introduction.
 *
 * There is only a single ECMAScript heap in the system, regardless of
 * the number of execution contexts that are live; all contexts share
 * the heap, and there is no restriction on pointers between objects
 * allocated to one context and those allocated to another.
 *
 * Garbage collections can only be triggered at safe points: when no
 * contexts are executing.  The decision about how often to collect
 * must also be taken outside the collector (see section 'Accounting'
 * below).  Client code should call ESMM::Collect() to trigger a
 * garbage collection.
 *
 *
 * @subsection Types              Object types, allocation, and deallocation.
 *
 * There are two classes of objects: roots and collectible objects.
 *
 * Any objects that contain pointers to objects in the heap but that
 * are not themselves garbage collectible are root objects.  Every root
 * object must inherit from the ES_GCRoot class and must implement the
 * pure virtual function GCTrace, as described below.
 *
 * A collectible object lives within the garbage collected heap and is
 * managed by the collector.  It must be a POD-struct containing an
 * ES_Header object as its first element and must be included in the
 * ES_Boxed union, see es_heap_object.h and es_boxed.h.  In addition,
 * the garbage collector's MarkFromMarkStack() method must be told
 * how to recognize it and trace it.  If the object needs to be destroyed
 * when it is collected, then the garbage collector's SweepPage() method
 * must be told how to recognize it and destroy it.
 *
 * Allocation is abstracted behind the macro GC_ALLOCATE(), which takes
 * a variable, a class name, and a parameter list (including parentheses),
 * allocates storage from the collected heap, runs the initializer
 * on that storage, and stores the pointer in the variable.
 *
 * Most collectible objects are not destroyed at all; the storage is
 * simply reclaimed.  Some objects require destruction because they
 * reference external resources (eg host objects), and the sweeper
 * takes care of destroying these.  There is no common destruction
 * protocol, and the C++ destructor is never used.
 *
 *
 * @subsection Roots              Static or permanent roots.
 *
 * Some locations may contain pointers to garbage-collected objects
 * but cannot themselves be subclassed from ES_GCRoot; global
 * pointer variables are an example of this.  These locations may be
 * added to the list of permanent roots by calling
 * ESMM::AddStaticRoot().  The static roots must contain pointers
 * to collectible objects or NULL.
 *
 *
 * @subsection Destruction        Destruction dependencies.
 *
 * In some cases, a set of objects becomes collectible together but
 * must be destroyed in two phases because a subset of the objects
 * use some of the collectible objects during destruction.  This happens
 * for example when foreign-objects are deleted: they must be destroyed
 * before any runtimes and also before any identifiers, because they
 * use both in their destruction.
 *
 * To handle this, the collector allows a bit called 'phase2' to be set
 * on collectible objects so that whenever a collection occurs, all
 * objects with phase2=0 are deleted before objects with phase2=1.
 *
 * In the current implementation, objects with phase2=1 will be
 * destroyed in the next collection cycle, this means there may be
 * some floating garbage.  The amount is expected to be small generally.
 *
 *
 * @subsection Tracing            Tracing pointers.
 *
 * The working principle of the collector is that each collectible
 * object that is reachable by a chain of pointers from a root is
 * retained and that all other collectible objects are collected and
 * deleted.  Initially the collector calls the GCTrace() method
 * of each root object.  For each pointer p from the root object to a
 * collectible object the GCTrace() method must call Heap::Mark(p) to
 * mark the object as live; this will invoke MarkFromMarkStack(p),
 * which will push p onto the mark stack and run the marker until
 * the mark stack is empty.
 *
 * Once all reachable objects are marked, the collector will sweep
 * the heap and delete unreachable objects.
 *
 *
 * @subsection Accounting         Accounting and GC triggering.
 *
 * The collector keeps track of the amount of space allocated to
 * the heap, in GC_DATA.bytes_in_heap.  Space grows in large chunks,
 * normally.
 *
 * Following GC the collector reports the amount of heap space that
 * is occupied by live data, through GC_DATA.bytes_live_after_gc.
 *
 * Following GC the variable GC_DATA.bytes_live is the same as
 * GC_DATA.bytes_live_after_gc, but it is updated when objects are
 * allocated.
 *
 * None of these figures include space allocated outside the
 * garbage-collected objects even if tied to those objects;
 * chiefly this concerns host object storage and compiled regex
 * nodes.
 *
 * The preferred mode of triggering GC is by computing, following
 * a GC, the peak amount of storage that may be allocated to the
 * heap, and to trigger the GC when GC_DATA.bytes_live exceeds
 * that amount.
 */

/* Some design notes:
 *
 * Tracing.
 *
 * Logically, tracing happens by "coloring" the objects in the heap.  An
 * object that has not been seen by the GC is white.  Once it is seen, it
 * is colored gray; and it is colored black once all its children have been
 * colored gray or black.  Tracing is complete when all roots are black
 * and there are no gray objects.
 *
 * Tricolor marking is implemented using the mark stack and a single mark
 * bit per object.  A white object is not on the mark stack and its bit is 0.
 * When it is seen it is colored gray by pushing it onto the stack and setting
 * its bit to 1.  It is colored black by popping it off the stack and coloring
 * all its white children gray.
 *
 * The foreign-object marking depends on this: when the DOM code calls in to
 * the engine to mark an object, it receives a return value stating whether
 * the object was already colored, and it uses that value to drive its
 * own traversal of the DOM structure.  To avoid nonlinear behavior in the
 * DOM marker, a white ECMAScript object must be colored immediately
 * so that a second attempt to mark it by the DOM code will return a value
 * stating that the object was already colored.
 *
 *
 * Mark stack.
 *
 * The collector allocates the mark stack in segments, allocating new segments
 * on demand and keeping a cache of such segments for better performance.
 *
 * Reasonably extensive testing shows that for normal sites (I tested a list
 * of top-100 sites I got from QA) the mark stack does not become very deep:
 * it peaked at 5 segments of 50 elements each, ie, 250 entries.  At this
 * point Opera had started throwing out documents.  On a larger machine more
 * documents would be retained, but there would also be more space for the
 * stack.
 *
 * On small devices Opera may be unable to grow the mark stack due to OOM
 * during garbage collection (though this is fairly unlikely).  Mark stack
 * exhaustion is handled by three strategies:
 *
 *  - long property list chains are marked piecewise (to reduce the chance
 *    of blowing the stack)
 *
 *  - if the stack is exhausted, a fallback marking routine is run to
 *    complete the marking starting from all objects in the heap that
 *    would have been gray, had the stack not been exhausted (so exhaustion
 *    does not compromise correctness)
 *
 *  - mark stack exhaustion does not prevent the object currently being
 *    blackened from becoming blackened (so progress is always guaranteed)
 *
 * Since the mark stack is normally very shallow, all mark stack segments
 * are cached between collections, but all but one are freed when a GC is
 * requested explicitly by Opera.
 *
 *
 * Mark bits.
 *
 * Mark bits are kept in the objects themselves.  This simplifies the design,
 * because it is easy to find the bit given the object pointer, but means
 * that there will be more write traffic through the cache than with an
 * external, packed bitmap.
 */

#ifndef ES_COLLECTOR_H
#define ES_COLLECTOR_H

/* All tunable parameters controlling allocation and garbage collection.
   For now, just define old names in terms of new names, but should clean
   this up. */

//#define ES_HARDCORE_GC_MODE
extern BOOL g_hardcode_gc_mode;

#ifdef ES_MOVABLE_OBJECTS

# define LARGE_OBJECT_LIMIT ES_PARM_SMALL_PAGE_SIZE / 2

#else // ES_MOVABLE_OBJECTS

#define LARGE_OBJECT_LIMIT_MAX 2048u
# ifdef ES_HARDCORE_GC_MODE
#  define LARGE_OBJECT_LIMIT (g_hardcode_gc_mode ? 0 : LARGE_OBJECT_LIMIT_MAX)
# else // ES_HARDCORE_GC_MODE
#  define LARGE_OBJECT_LIMIT LARGE_OBJECT_LIMIT_MAX
# endif // ES_HARDCORE_GC_MODE

#endif // ES_MOVABLE_OBJECTS

#define QUICK_CUTOFF_UNITS            (LARGE_OBJECT_LIMIT/ES_LIM_ARENA_ALIGN)
#define QUICK_CUTOFF_UNITS_MAX        (LARGE_OBJECT_LIMIT_MAX/ES_LIM_ARENA_ALIGN)

#include "modules/util/simset.h"
#include "modules/ecmascript/carakan/src/kernel/es_boxed.h"
#include "modules/ecmascript/carakan/src/vm/es_block.h"

class ES_Value_Internal;
class ES_PageHeader;
class ES_MarkSweepHeap;
class ES_StaticStringData;
class ES_Class_Singleton;
class ES_Code_ClassTable;
class ES_NativeCodeBlock;
class ES_RootCollection;
class OpExecMemoryManager;

enum GC_Reason
{
    // Note, gctrace processing code knows about the assigned values, pls don't change them
    GC_REASON_ALLOCLIMIT=0,        // Allocation limit reached
    GC_REASON_EXTERN=1,            // Triggered by external agent: script, user, OOM code, document flushing
    GC_REASON_SHUTDOWN=2,          // Engine shutting down
    GC_REASON_MAINTENANCE=3,       // Triggered by periodic running maintenance mechanism
    GC_REASON_DEBUG=4,              // Triggered by selftests and other debugging tools.
    GC_REASON_MERGE=5              // Before heap merge
};

/****************************************************************************
 *
 * Base object types
 */

/** Data carrier class for objects derived from ES_GCRoot */
class ES_RootData
{
public:
    virtual ~ES_RootData();                     // To kill GCC warnings

    ES_RootData  *root_next;                 // Next root in a two-way list
    ES_RootData  *root_prev;                 // Previous root in a two-way list

    virtual void GCTrace();
    virtual void NewHeap(ES_Heap *heap);

    void Remove();
    void AddTo(ES_Heap *heap);

private:
    ES_RootCollection *collection;
};


/** Base class for automatically tracked garbage collection roots. */
class ES_GCRoot : public ES_RootData
{
public:
    ES_GCRoot(ES_Heap *heap);

    virtual ~ES_GCRoot();
};



/** Class for objects on the free list.  This too needs to be small, because
    no object smaller than this can be on the free list but will instead be
    lost as internal fragmentation in other objects.
*/
class ES_Free : public ES_Boxed
{
public:
    ES_Free*    next;
};

class ES_ForwardingPointer : public ES_Boxed
{
public:
    ES_Boxed* forwarded;
};


/****************************************************************************
 *
 * GC management
 */

class ES_MarkStackSegment
{
public:
    ES_MarkStackSegment()
        : next(NULL)
    {
        elements[0] = NULL;
        top = &elements[0];
    }

    ES_Boxed *elements[ES_PARM_MARKSTACK_SEGMENT_SIZE];
    /**< Pointers to be marked. */

    ES_Boxed **top;
    /**< Points to top element; valid only when not inside MarkFromStack(). */

    ES_MarkStackSegment *next;
    /**< Older segments on the stack. */
};

/** Tracing data structures that are shared between a group of heaps. */
class ES_MarkStack
{
public:
    static ES_MarkStack *MakeL();
    /**< Create mark stack.  The object is reference counted, and the caller
         receives one reference to the created object. */

    ES_MarkStackSegment *segment;
    /**< Current markstack segment, never NULL. */

    ES_MarkStackSegment *freelist;
    /**< Free markstack segment nodes, or NULL. */

    BOOL exhausted;
    /**< Set to TRUE if mark stack could not be extended during marking. */

    void Overflow();
    /**< The mark stack segment 'markstack' is full.  Try to switch to a new
         one, either by reusing one from 'markstack_freelist' or by allocating
         one from the C++ heap. */

    void Underflow();
    /**< All objects in the mark stack segment 'markstack' have been traced; to
         the previous segment if there is one, otherwise return FALSE. */

    unsigned long inuse;      ///< Number of mark stack segments in use at present.
    unsigned long inuse_peak; ///< Peak number of mark stack segments in use during collection cycle.

    static ES_MarkStack *IncRef(ES_MarkStack *markstack)
    {
        ++markstack->ref_count;
        return markstack;
    }

    static void DecRef(ES_MarkStack *markstack)
    {
        if (--markstack->ref_count == 0)
            OP_DELETE(markstack);
    }

    ES_Heap *heap;
    /**< Set while this mark stack is being used to mark objects in this
         heap.  Thus always set while marking. */

private:
    ES_MarkStack()
        : segment(NULL),
          freelist(NULL),
          exhausted(FALSE),
          inuse(0),
          inuse_peak(0),
          heap(NULL),
          ref_count(1)
    {
    }

    ~ES_MarkStack()
    {
        ES_MarkStackSegment *s, *n;

        n = segment;
        while ((s = n) != NULL)
        {
            n = n->next;
            OP_DELETE(s);
        }

        n = freelist;
        while ((s = n) != NULL)
        {
            n = n->next;
            OP_DELETE(s);
        }
    }

    unsigned ref_count;
};

/**
 * Collection of roots.
 */
class ES_RootCollection
{
public:
    static ES_RootCollection *MakeL();
    ES_RootCollection();
    ~ES_RootCollection();

    void IncRef();
    void DecRef();

    ES_RootData root_objs;
    /**< Doubly linked list of root objects.  Typical root objects are execution
         contexts. */

    ESMM_DynamicRoots *dynamic_roots;
    /**< Externally referenced objects, excluded from garbage collection for
         whatever reason. */

    ES_Heap *heap;

private:
    unsigned ref_count;
};

#ifdef ES_DBG_GC
struct ES_HeapDebugData
{
    void MergeWith(ES_HeapDebugData *other);

    unsigned small_object_count;
    unsigned large_object_count;
    unsigned small_object_bytes;
    unsigned large_object_bytes;
};
#endif // ES_DBG_GC

class ES_HeapHandle : public Link
{
public:
    ES_HeapHandle(ES_Heap *heap) : heap(heap) {}
    ES_Heap *heap;
};

class ES_HeapReference
{
public:
    ES_Heap *GetHeap() { return heap; }

private:
    friend class ES_Heap;
    friend class ES_MarkSweepHeap;

    ES_HeapReference(ES_Heap *heap) : heap(heap), next(NULL) {}

    ES_Heap *heap;
    ES_HeapReference *next;
};

/**
 * Base class to garbage collected heaps.
 */
class ES_Heap : public Link
{
public:
    static ES_Heap *MakeL(ES_MarkStack *markstack, ES_Runtime *runtime = NULL, ES_RootCollection *root_collection = NULL, ES_Heap *parent_runtime_heap = NULL);
    /**< Create new heap. */

    static void Destroy(ES_Heap *heap);
    /**< Destroys a heap.  It is an error if there are any root objects or
         externally referenced objects on the heap when it is destroyed. */

    void DestroyedRuntime(ES_Runtime *runtime);
    /**< Remove runtime from this heap's list of runtimes. */

    void InitializeAllocationLimit();
    /**< Called when runtime's been initialized.  Sets bytes_limit to an
         appropriate initial value. */

    ES_Boxed *Allocate(ES_Context *context, unsigned nbytes);
    /**< Allocate an object of size 'nbytes'. If the object is small it will try
         the fast path, i.e., allocate it from the current block. Otherwise it
         will call AllocateSmall or AllocateLarge.

         The object will be initialized to contain a basic ES_Boxed object. At
         least its GC_Tag and/or CLASSID should be set by the caller immediately
         following the allocation. This function may garbage collect this heap
         prior to allocating the object.

         Returns NULL on failure. */

    ES_Boxed *AllocateSimple(ES_Context *context, unsigned nbytes);
    /**< Allocate an object of size 'nbytes'. Similar to 'Allocate',
         but intended to be called when a full Allocate isn't
         possible, e.g. from JIT code.  Should not trigger a collect,
         and should only try fast paths before giving up. It is
         assumed that if AllocateSimple returns NULL you should
         default to calling Allocate. */

    BOOL AddDynamicRoot(ES_Boxed *obj);
    /**< @return TRUE if success. */

    void RemoveDynamicRoot(ES_Boxed *obj);

    BOOL AddStaticStringData(ES_Context *context, ES_StaticStringData *data);
    /**< Signal that JString objects on this heap reference the static
         JStringStorage object owned by 'data'.  This function acquires a
         reference to 'data' which is released once a GC has confirmed that no
         references to the JStringStorage object remains. */

    /**< @return TRUE if heap is locked from being garbage collected. */

    void AddPrototypeClass(ES_Class_Singleton *klass);
    /**< Add 'klass' to the list of prototype classes on the current
         heap. This list is known to not contain any of the classes
         rooted in the global object. */

    unsigned CountPrototypeClasses() const;
    /**< Count the number of prototypes currently allocated on the
         heap. */

    void AddClassTable(ES_Code_ClassTable *table);
    /**< Add 'table' to the list of class tables on the current heap.
         This list contains object class references that are weakly
         referenced from other objects, leaving the garbage collector
         to remove unmarked references from their class trees after
         having completed the mark phase. */

    void TracePrototypeClasses();
    /**< Trace any "weak" class references maintained and recorded by
         the current heap. These class references are instances of
         prototype objects and any unmarked classes will be pruned
         from the class tree. */

    BOOL IsPrototypeClassRecorded(ES_Class_Singleton *klass);
    /**< Returns TRUE if the given class is already recorded as
         a prototype class by this heap. */

    BOOL IsLocked();
    /**< @return TRUE if heap is locked from being garbage collected. */

    BOOL IsCollecting();
    /**< @return TRUE if heap is currently running a GC. */

    BOOL HasReasonableLockValue();
    /**< @return TRUE if heap is has a reasonable lock value. */

    void Lock();
    /**< Locks the heap. */

    void Unlock();
    /**< Unlocks the heap. Used when we yield the exectution. */

    void SetNeedsGC();
    /**< Called when Someone thinks there is garbage to collect on this heap. */

    void RegisterActivity();
    void ResetActivity();
    unsigned LastGC() { return last_gc; }
    unsigned LastActivity() { return last_activity; }

#ifndef _STANDALONE
    BOOL ActiveSinceLastGC();
    BOOL ActiveRecently(double current_time);
    BOOL CollectedRecently(double current_time);

    BOOL NeedsMaintenanceGC(double current_time);
    /**< @return TRUE if heap thinks it is meaningfull to run a (maintenance)
         garbage collect on it. */
#endif // !_STANDALONE

    void IncContexts();
    /**< Increases context counter and moves the heap to the active list if it has any contexts. */

    void DecContexts();
    /**< Decreases context counter and moves the heap to the inactive list if it has no contexts. */

    void CollectIfNeeded(ES_Context *context);

    virtual void MaybeCollect(ES_Context *context) = 0;
    /**< Make a policy decision about whether to run the collector, and run it
         if that is the decision. */

    virtual void MaybeCollectOffline(ES_Context *context) = 0;
    /**< Make a policy decision about whether to run the collector, and run it
         if that is the decision. Uses the lower offline load factor which makes
         it more likely to trigger a gc. */

    virtual void ForceCollect(ES_Context *context, GC_Reason reason) = 0;
    /**< Run a garbage collection. */

    BOOL Mark(ES_Boxed *object);
    /**< If 'object' isn't marked, mark it and return FALSE, otherwise return
         TRUE. */

    virtual OP_STATUS MergeWith(ES_Heap *other) = 0;
    /**< Add other's heap internal data such as pages and roots to ours, and
         destroy 'other'. */

    virtual void Free(ES_Boxed *block) = 0;

    ES_MarkStack *GetMarkStack() { return markstack; }
    ES_PageAllocator *GetPageAllocator() { return page_allocator; }
    void SetPageAllocator(ES_PageAllocator *allocator);

    inline void PushBoxed(ES_Boxed *boxer);
    inline void PushValue(const ES_Value_Internal &value);

#ifdef ES_NATIVE_SUPPORT
    void AddNativeDispatcher(ES_NativeCodeBlock *block);
    void DeprecateNativeDispatcher(ES_NativeCodeBlock *block);
    void FlushNativeDispatchers();
    void FlushDeprecatedNativeDispatchers();

    OpExecMemoryManager *GetExecutableMemory() { return executable_memory; }
#endif // ES_NATIVE_SUPPORT

    unsigned Id() { return id; }

    void RecordExternalBytes(unsigned bytes) { bytes_live_external += bytes; }

    unsigned GetBytesLive() { return bytes_live; }
    unsigned GetBytesLiveTotal() { return (bytes_live + bytes_live_external); }
    unsigned GetBytesLivePeak() { return bytes_live > bytes_live_peak ? bytes_live : bytes_live_peak; }
    unsigned GetBytesLimit() { return bytes_limit; }
    unsigned GetBytesOfflineLimit() { return bytes_offline_limit; }
    unsigned GetBytesInHeap() { return bytes_in_heap; }

    ES_Allocation_Context *GetAllocationContext() { return allocation_context; };
    void SetAllocationContext(ES_Allocation_Context *context) { allocation_context = context; }

    ES_Runtime *GetFirstRuntime() { return first_runtime; }
    void AddRuntime(ES_Runtime *runtime)
    {
        runtime->GetNextRuntimePerHeap() = first_runtime;
        first_runtime = runtime;
        ++attached_runtimes;
    }

    void DetachedRuntime() { --attached_runtimes; ++detached_runtimes; }

    ES_HeapReference *GetReference() { return first_reference; }

    BOOL IsSuitableForReuse() { return suitable_heap_merge > 0; }

    void BeforeRuntimeCreation() { before_runtime_creation_bytes_live = bytes_live; }

    void SuitableForReuse(BOOL merge);
    /**< See ES_Runtime::SuitableForReuse() */

    BOOL HasHighFragmentation();
    /**< See ES_Runtime::HasHighMemoryFragmentation() */

    BOOL InCollector() { return in_collector; }
    /**< Returns TRUE if Collect() is currently active for this heap. */

protected:
    friend class OpAutoPtr<ES_Heap>;
    ES_Heap();
    virtual ~ES_Heap();

    void AddHeapToManager();
    void RemoveHeapFromManager();

    void TraceFromMarkStack();
    /**< Main tracing function. Trace all objects on the mark stack. */

    void TraceFromRootObjects();
    /**< Trace each root object in 'root_collection->root_objs'. */

    void TraceFromDynamicRoots();
    /**< Trace each dynamic root object in 'root_collection->dynamic_roots'. */

    virtual ES_Boxed *AllocateSmall(ES_Context *context, unsigned nbytes) = 0;
    /**< Internal allocation of small objects. Called from Allocate when we
         can't allocate from current block. */

    virtual ES_Boxed *AllocateLarge(ES_Context *context, unsigned nbytes) = 0;
    /**< Internal allocation of large objects. */

    virtual ES_Boxed *AllocateSimpleSmall(ES_Context *context, unsigned nbytes) = 0;
    /**< Internal allocation of small objects. Similar to AllocateSmall, but must
         not trigger a collect. Should only follow fast paths. */

    friend class ES_SuspendedCollect;

    friend class ES_RootData;

#ifdef ES_NATIVE_SUPPORT
public:
#endif // ES_NATIVE_SUPPORT

    char *current_top, *current_limit;
    ES_PageHeader *current_page;

    unsigned bytes_live;           ///< Bytes allocated to objects (some of which may be floating garbage).
    unsigned bytes_live_external;  ///< Bytes externally allocated by heap objects.

    unsigned id;

#ifdef ES_NATIVE_SUPPORT
protected:
#endif // ES_NATIVE_SUPPORT

    ES_MarkStack *markstack;
    /**< Mark stack, used during marking/tracing stage of GC.  Shared between
         different heaps. */

    ES_RootCollection *root_collection;
    /**< Collection of roots. Shared between heaps in collectors using more than
       one heap, such as the generational collector. */

    friend class ESMM;

    BOOL inside_marker;
    /**< TRUE if an instance of MarkFromMarkStack() is active. */

    BOOL in_collector;
    /**< TRUE when we are currently collecting garbage on this heap. */

    BOOL needs_gc;
    /**< TRUE when we tried to run a gc but the heap was locked so we should try
       again later. */

    BOOL external_needs_gc;
    /**< TRUE when someone thinks there is garbage to collect on this
       heap. Typically when a runtime, that was using this heap, was
       detached. */

    class StaticStringDataLink
    {
    public:
        StaticStringDataLink(ES_StaticStringData *data) : data(data) {}

        ES_StaticStringData *data;
        StaticStringDataLink *next;
    };

    StaticStringDataLink *static_string_data_links;

    ES_Class_Singleton *prototypes;

    ES_Code_ClassTable *class_tables;

#ifdef ES_HARDCORE_GC_MODE
    BOOL in_allocate;
    /**< TRUE when we already are in Allocate(). Prevents infinite GC recursion. */
#endif // ES_HARDCORE_GC_MODE

    friend class ES_CollectorLock;

#ifdef _DEBUG
    unsigned collections_count;
#endif // _DEBUG

    // Accounting
    unsigned bytes_in_heap;        ///< Bytes allocated to the heap (not necessarily to live data).
    unsigned pages_in_heap;        ///< Pages (blocks) allocated to the heap.

    unsigned bytes_limit;          ///< Allocation limit during execution (when 'bytes_live > bytes_offline_limit' we should run the GC).
    unsigned bytes_offline_limit;  ///< Allocation limit when not running a context (when 'bytes_live > bytes_runnning_limit' we should run the GC).
    unsigned bytes_live_peak;      ///< Peak live bytes.  This variable is only updated after GC, but GetBytesLivePeak() returns the true peak at all times.
    unsigned bytes_live_after_gc;  ///< Bytes live following the previous GC (accurate).
    unsigned bytes_live_external_after_gc;  ///< External bytes allocated following the previous GC (accurate).
    unsigned bytes_free_after_gc;  ///< Bytes found free during the previous GC (accurate).

    unsigned locked;
    /**< Heap locked if non-zero; don't run collector.  This is used during
         compound allocations to keep the different parts of the allocation from
         being garbage collected later during the compound allocation.

         Can only modified by ES_CollectorLock. */

    ES_Allocation_Context *allocation_context;

    ES_Runtime *first_runtime;
    unsigned attached_runtimes;
    unsigned detached_runtimes;

    ES_HeapReference *first_reference;

    ES_PageAllocator *page_allocator;

#ifdef ES_NATIVE_SUPPORT
    OpExecMemoryManager *executable_memory;

    Head native_code_blocks;
    Head deprecated_native_code_blocks;
#endif // ES_NATIVE_SUPPORT

    unsigned last_activity, last_gc;
    int contexts;

    ES_HeapHandle page_allocator_handle;

    // Non-zero if we should merge the next connected heap we see.
    int suitable_heap_merge;
    unsigned before_runtime_creation_bytes_live;

#ifdef ES_DBG_GC
   ES_HeapDebugData debug_data;
#endif // ES_DBG_GC

#ifdef ES_HEAP_DEBUGGER
public:
    unsigned live_objects[GCTAG_UNINITIALIZED];
#endif // ES_HEAP_DEBUGGER
};

/* There are no instances of ESMM, it's just a placeholder for static functions */
class ESMM
{
private:
    ESMM();                                                      // There are no instances
    ~ESMM();                                                     // There are no instances

public:
    static void ClearGCTimer();
    static double GetGCTimer();

    static void TraceObject(ES_Heap *heap, ES_Boxed *o);
    static void DestroyObject(ES_Boxed *o);

    static void Collect(BOOL release_maximal_memory);
        /**< Run a garbage collection.

             @param release_maximal_memory (in) If TRUE, then:
                        - a symbol table collection is forced
                        - empty pages are returned to the system memory pool rather
                          than being used to replenish free lists
                        - the mark stack is aggressively pruned following GC

                        (If we ever get around to implementing compaction/relocation,
                        this is when compaction will happen, in an attempt to free up
                        more pages.)
             */

    static BOOL MarkReturn(ES_Boxed *object);

    static void SweepRuntimes(ES_Runtime *&first_runtime);
};


struct ESMM_Debug
{
    // Collection performance counters
    double ms_major_collecting;             // Cumulative major GC time
    double ms_major_tracing;                // Cumulative major GC trace time
    double ms_major_sweeping;               // Cumulative major GC sweep time
    double ms_minor_collecting;             // Cumulative minor GC time
    double ms_minor_tracing;                // Cumulative minor GC trace time
    double ms_minor_sweeping;               // Cumulative minor GC sweep time
    double ms_collecting_history[10]; // Last collection times (circular buffer)
    double ms_tracing_history[10];    // Last trace times (circular buffer)
    double ms_sweeping_history[10];   // Last sweep times (circular buffer)
    unsigned long num_major_collections;            // Cumulative number of GCs
    unsigned long num_minor_collections;            // Cumulative number of GCs
    unsigned long bytes_allocated;            // Cumulative bytes allocated (accurate)
    unsigned long bytes_allocated_external;   // Cumulative bytes allocated externally (accurate)
    unsigned long bytes_reclaimed;            // Cumulative bytes reclaimed (not accurate: includes fragmentation and unused space)
    unsigned long bytes_in_heap_peak;       // Largest number of bytes allocated to heap immediately prior to GC
    unsigned long pages_in_heap_peak;        // Largest number of pages allocated to heap immediately prior to GC
    unsigned long markstack_segment_peak;    // Largest number of live markstack segments during tracing
    unsigned long symbols_peak;                // Largest number of live symbols immediately prior to pool GC
    unsigned long markstack_exhaustions;    // Number of exhaustions of mark stack

    // Working storage
    unsigned int  gc_history_pointer;       // Pointer into history buffer
    double        gc_timer;                    // Timestamp before GC

#ifdef ES_DBG_GC
    // Counters for allocation activity, too expensive normally but good to know
    unsigned long quicklist_hits;            // Cumulative: hit quicklist on first try
    unsigned long quicklist_misses;            // Cumulative: hit quicklist, but not on first try
    unsigned long small_objects;            // Cumulative: number of objects allocated from within quicklists
    unsigned long large_objects;            // Cumulative: number of objects allocated outside quicklists
    unsigned long very_large_objects;        // Cumulative: number of objects bigger than standard page size

    // Counters for host object activity
    unsigned long host_objects_traced;        // During last GC
    unsigned long host_objects_deleted;       // During last GC

    // Other performance measurements
    unsigned long dynamic_roots_peak;        // Peak length of dynamic roots list
    unsigned long dynamic_roots_insertions;    // Number of insertions
    unsigned long dynamic_roots_removals;    // Number of removals
    unsigned long dynamic_roots_probes;        // Number of probes for removals
    unsigned long dynamic_roots_segments;    // Number of segments currently allocated

    // Stats variables to judge whether GC is prompt enough: overshoot is
    // the amount of storage allocated beyond the gc limit at the time
    // of GC (due to GC at safe points only).
    double overshoot_percent_sum;        // running sum of overshoot in percent
    double overshoot_percent_avg;        // average overshoot at last collection

    // Cumulative counters for the number of objects allocated of a particular kind.
    //
    // The annotations:
    //  'Object' tracing means standard property lists
    //  '+Foreign' tracing means it also references a host object
    //  'Foreign' destruction means the host object must be destroyed but the object itself need not
    //  'Custom' tracing or destruction means ad-hoc; requires tag dispatch
    //
    //                                            Tracing                Destruction
    // Derived from ES_Object
    unsigned long cnt_ES_Object;                //    Object                No
    unsigned long cnt_ES_Array;                    //  Object                No
    unsigned long cnt_ES_Function;                //    Custom (fixable)    No
    unsigned long cnt_ES_Builtin_Function;        //    Object                No
    unsigned long cnt_ES_Foreign_Object;        //    Object+Foreign        Foreign
    unsigned long cnt_ES_Foreign_Function;        //    Object+Foreign        Foreign
    unsigned long cnt_ES_Global_Object;            //    Custom (fixable)    Foreign
    unsigned long cnt_ES_RegExp;                //  Object                Custom (not fixable)
    unsigned long cnt_ES_XML_Object;            //  Object                Custom

    // Not derived from ES_Object
    unsigned long cnt_ES_Compiled_Code;            //    Custom                Custom (fixable)
    unsigned long cnt_ES_Function_Descriptor;    //    Custom                No
    unsigned long cnt_ES_Boxed_Array;            //    Custom                No
    unsigned long cnt_JString;                    //    Custom                No
    unsigned long cnt_JStringStorage;            //    No                    No

    // new in Carakan.
    unsigned long cnt_ES_Box;
    unsigned long cnt_ES_Class;
    unsigned long cnt_ES_Identifier_Hash_Table; //    Custom                No
    unsigned long cnt_ES_Properties;
    unsigned long cnt_ES_Property_Table;
    unsigned long cnt_ES_ProgramCode;
    unsigned long cnt_ES_FunctionCode;
    unsigned long cnt_JStringSegmented;
    unsigned long cnt_ES_Identifier_Array;
    unsigned long cnt_ES_IdentifierCell_Array;
    unsigned long cnt_ES_Identifier_List;
    unsigned long cnt_ES_Identifier_Boxed_Hash_Table;
    unsigned long cnt_ES_Arguments_Object;
    unsigned long cnt_ES_Class_Data;
    unsigned long cnt_ES_Date_Object;
    unsigned long cnt_ES_Boolean_Object;
    unsigned long cnt_ES_Error;
    unsigned long cnt_ES_RegExp_Object;
    unsigned long cnt_ES_RegExp_Constructor;
    unsigned long cnt_ES_Host_Object;
    unsigned long cnt_ES_Host_Function;
    unsigned long cnt_ES_String_Object;
    unsigned long cnt_ES_Object_Extra;
    unsigned long cnt_ES_Number_Object;
    unsigned long cnt_ES_Property_Value_Table;
    unsigned long cnt_ES_Compact_Indexed_Properties;
    unsigned long cnt_ES_Sparse_Indexed_Properties;
    unsigned long cnt_ES_Byte_Array_Indexed;
    unsigned long cnt_ES_Type_Array_Indexed;
    unsigned long cnt_ES_Special_Mutable_Access;
    unsigned long cnt_ES_Special_Aliased;
    unsigned long cnt_ES_Special_Global_Variable;
    unsigned long cnt_ES_Special_Function_Arguments;
    unsigned long cnt_ES_Special_Function_Caller;
    unsigned long cnt_ES_Special_RegExp_Capture;
    unsigned long cnt_ES_Special_Error_StackTrace;
    unsigned long cnt_ES_Boxed_List;

    // Demographics for objects of variable size, counters at integer log2 intervals
    unsigned long demo_JStringStorage[32];
    unsigned long demo_ES_Lexical_Rib[32];
    unsigned long demo_ES_Boxed_Array[32];

#endif // ES_DBG_GC
};

#define GC_DDATA            RT_DATA.esmmd


/****************************************************************************
 *
 * Roots management
 */

/* The dynamic roots table is a bag that may be large and have a lot of
 * dynamic removals.  Removals in the linear list implementation involve
 * searching and contracting, so here is a hash-table based implementation
 * that is faster.
 */
class ESMM_DynamicRootsEntry
{
public:
    ES_Boxed *obj;
    ESMM_DynamicRootsEntry *next;
};

class ESMM_DynamicRootsSegment
{
public:
    ESMM_DynamicRootsEntry entries[ES_PARM_DYNAMIC_ROOTS_SEGMENT_SIZE];
    ESMM_DynamicRootsSegment *next;
};

class ESMM_DynamicRoots
{
private:
    ESMM_DynamicRootsSegment *segments; ///< points to newest segment, chained thru 'next'
    ESMM_DynamicRootsEntry *free;       ///< free list of entries, every free node is on this list
    ESMM_DynamicRootsEntry **table;     ///< hash table
    unsigned allocated;                 ///< number of entries allocated
    unsigned population;                ///< number of live entries
    unsigned tablesize;                 ///< length of hash table

private:
    unsigned Hash(ES_Boxed *obj, unsigned m);

public:
    ESMM_DynamicRoots();
    ~ESMM_DynamicRoots();

    void ConstructL();
    void Push(ES_Boxed *obj);
    void Remove(ES_Boxed *obj);

    unsigned Size() { return population; }
    ESMM_DynamicRootsSegment *Segments() { return segments; }
};


class ES_CollectorLock
{
public:
    ES_CollectorLock(ES_Context *context, BOOL gc_on_destruction = FALSE);
    ~ES_CollectorLock();
    void Release();
    void Acquire();

protected:
    ES_Context *context;
    BOOL released, gc_on_destruction;
};

#define GCLOCK(var, args) \
    ES_Allocation_Context var args; \
    ES_CollectorLock var##_gclock(&var);

/* Implemented in ecmascript_runtime.cpp. */
class ES_RuntimeReaper
    : public EcmaScript_Object
{
private:
    friend class ES_Runtime;
    ES_Runtime *runtime;

public:
    ES_RuntimeReaper(ES_Runtime *rt);
    virtual ~ES_RuntimeReaper();

    OP_STATUS Initialize(ES_Context *context);

    virtual void GCTrace();
};

#endif // ES_COLLECTOR_H
