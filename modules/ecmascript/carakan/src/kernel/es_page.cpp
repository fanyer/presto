/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright Opera Software ASA 2009
 *
 * @author Daniel Spang
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

/* static */ ES_Chunk *
ES_Chunk::Make()
{
    ES_Chunk *chunk = OP_NEW(ES_Chunk, ());

    if (!chunk)
        return NULL;

#ifdef ES_USE_ALIGNED_SEGMENTS
    chunk->segment = OpMemory::CreateAlignedSegment(CHUNK_SIZE, ES_PARM_SMALL_PAGE_SIZE);

    if (!chunk->segment)
    {
        OP_DELETE(chunk);
        return NULL;
    }

    chunk->storage = static_cast<char *>(chunk->segment->address);

    /* Verify that we actually got aligned memory. */
    OP_ASSERT((reinterpret_cast<UINTPTR>(chunk->storage) + ES_PARM_SMALL_PAGE_SIZE - 1 & ~(ES_PARM_SMALL_PAGE_SIZE - 1)) == reinterpret_cast<UINTPTR>(chunk->storage));

    ES_PageHeader *page = reinterpret_cast<ES_PageHeader *>(chunk->storage);
#else // ES_USE_ALIGNED_SEGMENTS
    ES_PageHeader *page = reinterpret_cast<ES_PageHeader *>(reinterpret_cast<UINTPTR>(chunk->storage) + ES_PARM_SMALL_PAGE_SIZE - 1 & ~(ES_PARM_SMALL_PAGE_SIZE - 1));
#endif // ES_USE_ALIGNED_SEGMENTS

    chunk->free_pages = NULL;
    for (chunk->free_pages_count = 0; chunk->free_pages_count < PAGES_PER_CHUNK; chunk->free_pages_count++)
    {
        ES_PageHeader *next_page = reinterpret_cast<ES_PageHeader *>(reinterpret_cast<UINTPTR>(page) + ES_PARM_SMALL_PAGE_SIZE);
        page->Initialize(reinterpret_cast<ES_Free *>(next_page) - 1, chunk, chunk->free_pages); // Leave space for the dummy end-of-page object.
        page->next = chunk->free_pages;
        chunk->free_pages = page;
        page = next_page;
    }

    return chunk;
}

ES_Chunk::ES_Chunk()
    : free_pages(NULL)
    , free_pages_count(0)
#ifdef ES_USE_ALIGNED_SEGMENTS
    , segment(NULL)
    , storage(NULL)
#endif // ES_USE_ALIGNED_SEGMENTS
{
}

ES_Chunk::~ES_Chunk()
{
#ifdef ES_USE_ALIGNED_SEGMENTS
    if (segment)
        OpMemory::DestroySegment(segment);
#endif // ES_USE_ALIGNED_SEGMENTS
}

ES_Chunk *
ES_ChunkAllocator::AllocateChunk()
{
    ES_Chunk *chunk = ES_Chunk::Make();

    if (!chunk)
        return NULL;

    stats.allocated++;
    stats.max_allocated = MAX(stats.allocated, stats.max_allocated);

#ifndef _STANDALONE
    MemoryManager::IncDocMemoryCount(sizeof(ES_Chunk), FALSE);
#endif // !_STANDALONE

    return chunk;
}

void
ES_ChunkAllocator::FreeChunk(ES_Chunk *chunk)
{
    chunk->CheckIntegrity();

    OP_ASSERT(chunk->AllFree());
    OP_DELETE(chunk);

    stats.allocated--;

#ifndef _STANDALONE
    MemoryManager::DecDocMemoryCount(sizeof(ES_Chunk));
#endif // !_STANDALONE
}

ES_PageAllocator::ES_PageAllocator(ES_ChunkAllocator *chunk_allocator)
    : chunk_allocator(chunk_allocator),
      reference_count(1)
{
#ifdef ES_HEAP_DEBUGGER
    Into(g_ecmaPageAllocatorList);
#endif // ES_HEAP_DEBUGGER
}

ES_PageAllocator::~ES_PageAllocator()
{
#ifdef ES_HEAP_DEBUGGER
    Out();
#endif // ES_HEAP_DEBUGGER

    OP_ASSERT(!current_chunks.First());
    OP_ASSERT(!full_chunks.First());

#if 0
    while (ES_Chunk *current_chunk = static_cast<ES_Chunk *>(current_chunks.First()))
    {
        current_chunk->Out();
        chunk_allocator->FreeChunk(current_chunk);
    }
#endif // 0
}

class ES_SuspendedAllocateChunk
    : public ES_SuspendedCall
{
public:
    ES_SuspendedAllocateChunk(ES_ChunkAllocator *chunk_allocator)
        : chunk_allocator(chunk_allocator)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        chunk = chunk_allocator->AllocateChunk();
    }

    ES_Chunk *chunk;

private:
    ES_ChunkAllocator *chunk_allocator;
};

ES_PageHeader *
ES_PageAllocator::AllocateFixed(ES_Context *context)
{
    ES_PageHeader *page = NULL;
    ES_Chunk *current_chunk = reinterpret_cast<ES_Chunk *>(current_chunks.First());

    while (current_chunk && !(page = current_chunk->GetPage()))
    {
        current_chunk->Out();
        current_chunk->Into(&full_chunks);

        current_chunk = reinterpret_cast<ES_Chunk *>(current_chunks.First());
    }

    if (!page)
    {
        OP_ASSERT(current_chunks.Empty());

        if (context->IsUsingStandardStack())
            current_chunk = chunk_allocator->AllocateChunk();
        else
        {
            ES_SuspendedAllocateChunk call(chunk_allocator);
            static_cast<ES_Execution_Context *>(context)->SuspendedCall(&call);
            current_chunk = call.chunk;
        }

        if (!current_chunk)
            return NULL;

        current_chunk->Into(&current_chunks);
        page = current_chunk->GetPage();
        OP_ASSERT(page);
        OP_ASSERT(page->limit->Bits() == 0);
        OP_ASSERT(ObjectSize(page->limit) == 0);
    }

#ifdef ES_HARDCORE_GC_MODE
    if (g_hardcode_gc_mode)
        op_memset(page->GetFirst(), 0xfe, page->limit - page->GetFirst());
#endif // ES_HARDCORE_GC_MODE

    /* Check that dummy object is in place. */
    OP_ASSERT(page->limit->Bits() == 0);
    OP_ASSERT(ObjectSize(page->limit) == 0);

    return page;
}

/* static */ ES_PageHeader *
ES_PageAllocator::AllocateLarge(ES_Context *context, unsigned nbytes)
{
    unsigned total_bytes = ES_PageHeader::TotalSize(nbytes) + ES_LIM_ARENA_ALIGN;

    void *storage;

    if (!context || context->IsUsingStandardStack())
        storage = op_malloc(total_bytes);
    else
    {
        ES_SuspendedMalloc call(total_bytes);
        static_cast<ES_Execution_Context *>(context)->SuspendedCall(&call);
        storage = call.storage;
    }

    if (storage == NULL)
        return NULL;

#ifndef _STANDALONE
    MemoryManager::IncDocMemoryCount(total_bytes, FALSE);
#endif // !_STANDALONE

    ES_PageHeader *page = reinterpret_cast<ES_PageHeader *>(reinterpret_cast<UINTPTR>(storage) + ES_LIM_ARENA_ALIGN & ~(ES_LIM_ARENA_ALIGN-1));
    UINTPTR offset = reinterpret_cast<UINTPTR>(page) - reinterpret_cast<UINTPTR>(storage);
    OP_ASSERT(0 < offset && offset < 256);
    reinterpret_cast<UINT8 *>(page)[-1] = offset;

    page->Initialize(reinterpret_cast<ES_Boxed *>((reinterpret_cast<UINTPTR>(page->GetFirst()) + nbytes)));

    OP_ASSERT(page->GetSize() == nbytes);
    OP_ASSERT(ES_IS_POINTER_PAGE_ALIGNED(page->GetFirst()));

#ifdef ES_HARDCORE_GC_MODE
    if (g_hardcode_gc_mode)
        op_memset(page->GetFirst(), 0xfe, reinterpret_cast<UINTPTR>(page->limit) - reinterpret_cast<UINTPTR>(page->GetFirst()));
#endif // ES_HARDCORE_GC_MODE

    return page;
}

void
ES_PageAllocator::FreeFixed(ES_PageHeader *page)
{
#ifdef ES_HARDCORE_GC_MODE
    if (g_hardcode_gc_mode)
        op_memset(page->GetFirst(), 0xfe, page->limit - page->GetFirst());
#endif // ES_HARDCORE_GC_MODE

    ES_Chunk *all_free_chunk = page->ReturnToChunk();

    if (all_free_chunk)
    {
        all_free_chunk->Out();
        chunk_allocator->FreeChunk(all_free_chunk);
    }
    else if (full_chunks.HasLink(page->GetChunk()))
    {
        page->GetChunk()->Out();
        page->GetChunk()->Into(&current_chunks);
    }
}

/* static */ void
ES_PageAllocator::FreeLarge(ES_Context *context, ES_PageHeader *page)
{
    OP_ASSERT(!context || context->IsUsingStandardStack());

    UINTPTR offset = reinterpret_cast<UINT8 *>(page)[-1];
    OP_ASSERT(0 < offset && offset < 256);
    void *storage = reinterpret_cast<UINT8 *>(page) - offset;

#ifndef _STANDALONE
    unsigned total_bytes = ES_PageHeader::TotalSize(page->GetSize()) + ES_LIM_ARENA_ALIGN;
    MemoryManager::DecDocMemoryCount(total_bytes);
#endif // !_STANDALONE

    OP_ASSERT(!context || context->IsUsingStandardStack());
    op_free(storage);
}

void ES_PageAllocator::MergeWith(ES_Heap *other_heap)
{
    ES_PageAllocator *other = other_heap->GetPageAllocator();

    if (this == other)
        return;

    current_chunks.Append(&other->current_chunks);
    full_chunks.Append(&other->full_chunks);

    // Go through all heaps using 'other' and update their page allocator to 'this'.
    ES_HeapHandle *heaph = static_cast<ES_HeapHandle *>(other->heaps.First());
    while (heaph)
    {
        ES_HeapHandle *next = static_cast<ES_HeapHandle *>(heaph->Suc());
        if (heaph->heap != other_heap)
            heaph->heap->SetPageAllocator(this);
        heaph = next;
    }
}

void ES_PageAllocator::AddHeap(ES_HeapHandle *heaph)
{
    OP_ASSERT(!heaph->InList());
    heaph->Into(&heaps);
}

void ES_PageAllocator::RemoveHeap(ES_HeapHandle *heaph)
{
    OP_ASSERT(heaps.HasLink(heaph));
    heaph->Out();
    if (heaps.Empty())
        OP_DELETE(this);
}

#ifdef ES_HEAP_DEBUGGER

unsigned ES_PageAllocator::CountPages()
{
    unsigned pages = 0;
    ES_Chunk *chunk;

    for (chunk = static_cast<ES_Chunk *>(current_chunks.First()); chunk; chunk = static_cast<ES_Chunk *>(chunk->Suc()))
        pages += ES_Chunk::PAGES_PER_CHUNK - chunk->CountFreePages();

    for (chunk = static_cast<ES_Chunk *>(full_chunks.First()); chunk; chunk = static_cast<ES_Chunk *>(chunk->Suc()))
        pages += ES_Chunk::PAGES_PER_CHUNK;

    return pages;
}

ES_HeapHandle *ES_PageAllocator::GetFirstHeapHandle()
{
    return static_cast<ES_HeapHandle *>(heaps.First());
}

#endif // ES_HEAP_DEBUGGER
