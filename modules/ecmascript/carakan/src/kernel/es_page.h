/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright Opera Software ASA 2009
 *
 * @author Daniel Spang
 */

#ifndef ES_PAGE_H
#define ES_PAGE_H


class ES_Chunk;
class ES_ChunkAllocator;

class ES_HeapHandle;
class ES_Heap;

class ES_PageHeader;

#ifdef ES_USE_ALIGNED_SEGMENTS
# include "modules/pi/system/OpMemory.h"
#endif // ES_USE_ALIGNED_SEGMENTS

/**
 * Small object pages are allocated from chunks.
 *
 * Note: If not using ES_USE_VIRTUAL_SEGMENTS, pages are aligned to the small
 * object page size and there will be some unused memory at the beginning and/or
 * end of the chunk storage.
 */
class ES_Chunk : public Link
{
public:
    ES_PageHeader *GetPage();

    ES_Chunk *ReturnPage(ES_PageHeader *page);

    BOOL AllFree() { return free_pages_count == PAGES_PER_CHUNK; }

    void CheckIntegrity();

    enum
    {
        PAGES_PER_CHUNK = ES_PARM_PAGES_PER_CHUNK,
        CHUNK_SIZE = ES_PARM_SMALL_PAGE_SIZE * PAGES_PER_CHUNK,
#ifdef ES_USE_ALIGNED_SEGMENTS
        TOTAL_SIZE = CHUNK_SIZE
#else // ES_USE_ALIGNED_SEGMENTS
        TOTAL_SIZE = CHUNK_SIZE + ES_PARM_SMALL_PAGE_SIZE - 1
#endif // ES_USE_ALIGNED_SEGMENTS
    };

#ifdef ES_HEAP_DEBUGGER
    unsigned CountFreePages() { return free_pages_count; }
#endif // ES_HEAP_DEBUGGER

private:
    friend class ES_ChunkAllocator;

    static ES_Chunk *Make();

    ES_Chunk();
    ~ES_Chunk();

    ES_PageHeader *free_pages;
    unsigned free_pages_count;

#ifdef ES_USE_ALIGNED_SEGMENTS
    const OpMemory::OpMemSegment *segment;
    char *storage;
#else // ES_USE_ALIGNED_SEGMENTS
    char storage[TOTAL_SIZE];
#endif // ES_USE_ALIGNED_SEGMENTS
};


/**
 * Header of page where garbage collected objects are allocated.
 *
 * A page consists of a page header, a card table (bit-field) representing the
 * remembered set, an object storage where the actual objects are stored and -
 * for large object pages - a footer with the excess card table.
 *
 * | header | card table | objects | excess card table |
 *
 * Both small and large object pages are aligned to the small object page size
 * to easily be able to find the beginning of the page.  Since there is only one
 * large object in large object pages we can use the same method to find the
 * page start for those.
 *
 * The card table is a bit-field where each bit represents a possible position
 * of a pointer in the object storage which is part of the remembered set.
 *
 * The card table logically starts at the very beginning of the page.  But since
 * the first pointer, that actually can be part of the remembered set, is
 * located after the card table, the first bits in the table are never set.  We
 * use this space to store the header.
 *
 * Large object pages, which can vary in size, uses the card table in the header
 * for the first part of the object store.  Pointers further up the object uses
 * the card table in the footer, since the card table in the header is fixed
 * sized.
 */
class ES_PageHeader
{
public:
    void Initialize(ES_Boxed *limit, ES_Chunk *owner = NULL, ES_PageHeader *next = NULL);

    static UINTPTR HeaderSize();
    /**< Size of header (including possible card table) counting from the beginning of the page. */

    static UINTPTR TotalSize(unsigned nbytes);
    /**< Total allocation size for page with 'nbytes' of useful payload. */

    ES_Boxed *GetFirst() const;
    /**< First object stored. */

    unsigned GetSize() const;
    /**< Size of the object storage. */

    ES_Chunk *GetChunk() const { return owner; }
    /**< The chunk this page belongs to. NULL if not allocated on chunk, i.e.,
       large object pages. */

    BOOL GetHasMarkedObjects() const { return flags.has_marked_objects; }
    /**< Page has one or more marked objects. */

    BOOL GetHasMustDestroyObjects() const { return flags.has_must_destroy_objects; }
    /**< Page has one or more objects that needs destroy. */

    void SetHasMarkedObjects(BOOL value) { flags.has_marked_objects = value; }
    void SetHasMustDestroyObjects(BOOL value) { flags.has_must_destroy_objects = value; if (value) flags.has_marked_objects = value; }

    void SetHasMarkedObjects() { flags.has_marked_objects = TRUE; }
    void SetHasMustDestroyObjects() { flags.has_must_destroy_objects = flags.has_marked_objects = TRUE; }

    void ClearHasMarkedObjects() { flags.has_marked_objects = FALSE; }
    void ClearHasMustDestroyObjects() { flags.has_must_destroy_objects = FALSE; }

    ES_Chunk *ReturnToChunk();

    ES_Boxed* limit;
    /**< Address past last object in page. */

    ES_PageHeader *next;
    /**< Next page in the area. */

    ES_Chunk *owner;
    /**< The chunk this page belongs to. NULL if not allocated on chunk, i.e.,
       large object pages. */

private:
    union
    {
        unsigned raw;
        struct
        {
            unsigned has_marked_objects:1;
            unsigned has_must_destroy_objects:1;
        } flags;
    };
};


/**
 *  Allocates chunks. This allocator is shared between heaps.
 */
class ES_ChunkAllocator
{
public:
    ES_ChunkAllocator() : stats() {}

    ES_Chunk *AllocateChunk();

    void FreeChunk(ES_Chunk *chunk);

    void TrimChunks() {}

    class Statistics
    {
    public:
        Statistics() : allocated(0), max_allocated(0) {}
        unsigned allocated;
        unsigned max_allocated;
    };

    Statistics stats;
};

/** TRUE if 'pointer' is aligned on ES_LIM_ARENA_ALIGN. */
#define ES_IS_POINTER_PAGE_ALIGNED(pointer) ((reinterpret_cast<UINTPTR>(pointer) & (ES_LIM_ARENA_ALIGN - 1)) == 0)

/** TRUE if 'size' is an integer multiple of ES_LIM_ARENA_ALIGN. */
#define ES_IS_SIZE_PAGE_ALIGNED(size) (((size) & (ES_LIM_ARENA_ALIGN - 1)) == 0)

/**
 *  Allocates aligned pages from chunks. Shared between related runtimes, hence
 *  reference counted.
 */
class ES_PageAllocator
#ifdef ES_HEAP_DEBUGGER
    : public Link
#endif // ES_HEAP_DEBUGGER
{
public:
    ES_PageAllocator(ES_ChunkAllocator *chunk_allocator);
    ~ES_PageAllocator();

    ES_PageHeader *AllocateFixed(ES_Context *context);
    /**< Allocate a small fixed sized page that is aligned to the page size. */

    static ES_PageHeader *AllocateLarge(ES_Context *context, unsigned nbytes);
    /**< Allocate a large page that is aligned to ES_LIM_ARENA_ALIGN. */

    void FreeFixed(ES_PageHeader *page);
    /**< Returns a page to the chunk. Deallocates the chunk if all pages in it
       is free. */

    static void FreeLarge(ES_Context *context, ES_PageHeader *page);
    /**< Deallocates a large page. */

    void MergeWith(ES_Heap *other_heap);
    /**< Merge other allocator with this one. */

    void AddHeap(ES_HeapHandle *);
    void RemoveHeap(ES_HeapHandle *);

#ifdef ES_HEAP_DEBUGGER
    unsigned CountChunks() { return current_chunks.Cardinal() + full_chunks.Cardinal(); }
    unsigned ChunkSize() { return ES_Chunk::TOTAL_SIZE; }
    unsigned CountPages();
    unsigned PageSize() { return ES_PARM_SMALL_PAGE_SIZE; }

    ES_HeapHandle *GetFirstHeapHandle();
#endif // ES_HEAP_DEBUGGER

private:
    ES_PageHeader *AllocateChunk(ES_Context *context, unsigned nbytes, unsigned align, BOOL update_top_and_limit);
    /**< Allocates a new chunk. */

    ES_PageHeader *AllocateFixedSlow(ES_Context *context);
    /**< Allocates a new aligned page of fixed size from a chunk. */

    Head current_chunks, full_chunks;
    /**< List of chunks where the first one is the one we currently allocate
       fixed pages from. This is a list because there can be more than one
       current chunk after a merge. */

    Head heaps;
    /**< List of heaps using this allocator. */

    ES_ChunkAllocator *chunk_allocator;

    unsigned reference_count;
};


class ES_PageVisitor
{
public:
    virtual void Visit(ES_PageHeader *page) = 0;
};

#endif // ES_PAGE_H
