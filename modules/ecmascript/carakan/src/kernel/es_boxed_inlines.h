/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1999 - 2006
 *
 * Data types allocated on the garbage collected heap.
 *
 * @author Lars T Hansen
 */

#ifndef ES_BOXED_INLINES_H
#define ES_BOXED_INLINES_H

inline void
ES_Boxed::InitGCTag(GC_Tag n, BOOL need_destroy)
{
    OP_ASSERT(need_destroy);
    ChangeGCTag(n);
    SetNeedDestroy();
}

inline void
ES_Boxed::SetNeedDestroy()
{
    hdr.header |= ES_Header::MASK_NEED_DESTROY;
    GetPage()->SetHasMustDestroyObjects();
}

inline unsigned
ES_Boxed::GetPageOffset()
{
    if (!IsAllocatedOnLargePage())
        return reinterpret_cast<UINTPTR>(this) & (ES_PARM_SMALL_PAGE_SIZE - 1);
    else
        return ES_PageHeader::HeaderSize();
}

inline ES_PageHeader *
ES_Boxed::GetPage()
{
    if (!IsAllocatedOnLargePage())
        return reinterpret_cast<ES_PageHeader *>(reinterpret_cast<UINTPTR>(this) & ~(ES_PARM_SMALL_PAGE_SIZE - 1));
    else
        return reinterpret_cast<ES_PageHeader *>(reinterpret_cast<UINTPTR>(this) - ES_PageHeader::HeaderSize());
}

inline const ES_PageHeader *
ES_Boxed::GetPage() const
{
    if (!IsAllocatedOnLargePage())
        return reinterpret_cast<ES_PageHeader *>(reinterpret_cast<UINTPTR>(this) & ~(ES_PARM_SMALL_PAGE_SIZE - 1));
    else
        return reinterpret_cast<ES_PageHeader *>(reinterpret_cast<UINTPTR>(this) - ES_PageHeader::HeaderSize());
}

/** @return non-zero if the mark bit is set */
inline unsigned int
IsMarked(const ES_Boxed *b)
{
	return b->hdr.header & ES_Header::MASK_MARK;
}

/** Set the normal mark bit */
inline void
SetMarked(ES_Boxed *b)
{
    OP_ASSERT(b->GetPage()->GetFirst() <= b);
    OP_ASSERT(b < b->GetPage()->limit);

#ifdef ES_HEAP_DEBUGGER
    ++g_ecmaLiveObjects[b->GCTag()];
#endif // ES_HEAP_DEBUGGER

    b->hdr.header |= ES_Header::MASK_MARK;
    b->GetPage()->SetHasMarkedObjects();
}

inline void
ClearMarked(ES_Boxed *b)
{
	b->hdr.header &= ~(ES_Header::MASK_MARK | ES_Header::MASK_NO_FOREIGN_TRACE);
}

inline void
InvertMarked(ES_Boxed *b)
{
    b->hdr.header ^= ES_Header::MASK_MARK;
}

#ifdef _DEBUG
inline unsigned int
IsDebugMarked(const ES_Boxed *b)
{
	return IsMarked(b);
}

inline void
SetDebugMarked(ES_Boxed *b)
{
    SetMarked(b);
}

inline void
ClearDebugMarked(ES_Boxed *b)
{
	ClearMarked(b);
}

inline void
InvertDebugMarked(ES_Boxed *b)
{
    InvertMarked(b);
}
#endif // _DEBUG

#endif // ES_BOXED_INLINES_H
