/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA 2009
 *
 * @author Daniel Spang
 */

#ifndef ES_PAGE_INLINES_H
#define ES_PAGE_INLINES_H

inline void
ES_PageHeader::Initialize(ES_Boxed *limit, ES_Chunk *owner/* = NULL*/, ES_PageHeader *next/* = NULL*/)
{
    this->limit = limit;
    this->owner = owner;
    this->next = next;
    raw = 0;

    /* Set up dummy end-of-page header. */
    this->limit->ClearHeader();
}

/* static */ inline UINTPTR
ES_PageHeader::HeaderSize()
{
    return sizeof(ES_PageHeader) + (ES_LIM_ARENA_ALIGN - 1) & ~(ES_LIM_ARENA_ALIGN - 1);
}

/* static */ inline UINTPTR
ES_PageHeader::TotalSize(unsigned nbytes)
{
    unsigned extra = sizeof(ES_Header); // Used for dummy end-of-page zero size object.

    OP_ASSERT(nbytes == GC_ALIGN(nbytes));
    OP_ASSERT(ES_PageHeader::HeaderSize() == GC_ALIGN(ES_PageHeader::HeaderSize()));

    return ES_PageHeader::HeaderSize() + nbytes + extra;
}

inline ES_Boxed *
ES_PageHeader::GetFirst() const
{
    return reinterpret_cast<ES_Boxed *>(reinterpret_cast<UINTPTR>(this) + HeaderSize());
}

inline unsigned
ES_PageHeader::GetSize() const
{
    return reinterpret_cast<UINTPTR>(limit) - reinterpret_cast<UINTPTR>(GetFirst());
}

inline ES_Chunk *
ES_PageHeader::ReturnToChunk()
{
    return GetChunk()->ReturnPage(this);
}


inline ES_PageHeader *
ES_Chunk::GetPage()
{
    CheckIntegrity();

    ES_PageHeader *page = free_pages;

    if (page)
    {
        free_pages = page->next;
        page->next = NULL;
        free_pages_count--;
        OP_ASSERT(page->owner == this);
    }

    return page;
}


inline ES_Chunk *
ES_Chunk::ReturnPage(ES_PageHeader *page)
{
    OP_ASSERT(page->owner == this);
    page->next = free_pages;
    free_pages = page;
    free_pages_count++;

    CheckIntegrity();

    if (AllFree())
        return this;
    else
        return NULL;
}


inline void
ES_Chunk::CheckIntegrity()
{
#ifdef DEBUG_ENABLE_OPASSERT
    unsigned free = 0;
    ES_PageHeader *free_page = free_pages;

    while (free_page)
    {
        free_page = free_page->next;
        free++;
    }

    OP_ASSERT(free == free_pages_count);
#endif // DEBUG_ENABLE_OPASSERT
}


#endif // ES_PAGE_INLINES_H
