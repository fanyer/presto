/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/util/OpRegion.h"

// == OpRegion ==========================================================

OpRegion::OpRegion()
	: m_rects(NULL)
	, m_num_rects(0)
	, m_max_rects(0)
#ifdef _DEBUG
	, m_debug_allowed_to_conflict(-1)
#endif // _DEBUG
{
}

OpRegion::~OpRegion()
{
	OP_DELETEA(m_rects);
}

void OpRegion::Empty()
{
#ifdef _DEBUG
	for(int i = 0; i < m_num_rects; i++)
	{
		for(int j = i+1; j < m_num_rects; j++)
		{
			OP_ASSERT(!m_rects[i].Intersecting(m_rects[j]));
		}
	}
#endif
	OP_DELETEA(m_rects);
	m_rects = NULL;
	m_max_rects = 0;
	m_num_rects = 0;
}

BOOL OpRegion::Resize(unsigned int new_capacity)
{
	OP_ASSERT(new_capacity >= (unsigned int)m_num_rects);

	// This constructor runs all the constructors, but they should
	// be light weight
	OpRect *new_rects = OP_NEWA(OpRect, new_capacity);
	if (!new_rects)
		return FALSE;

	if (m_rects)
		op_memcpy(new_rects, m_rects, sizeof(OpRect) * m_num_rects);

	OP_DELETEA(m_rects);
	m_rects = new_rects;
	m_max_rects = new_capacity;

	return TRUE;
}

BOOL OpRegion::TryShrink()
{
	// Hardcoded policy: maximum headroom = m_num_rects / 2
	unsigned int headroom = m_max_rects - m_num_rects;
	if (headroom <= (unsigned int)m_num_rects / 2)
		return FALSE;

	// Attempt to shrink
	unsigned int new_capacity = m_num_rects + m_num_rects / 2;

	return Resize(new_capacity);
}

#ifdef SVG_EXTENDED_OP_REGION_API
OP_STATUS OpRegion::Duplicate(OpRegion* &newRegion) const
{
	newRegion = NULL;
	OpRegion* r = OP_NEW(OpRegion, ());
	if (!r)
		return OpStatus::ERR_NO_MEMORY;

	r->m_num_rects = m_num_rects;
	r->m_max_rects = m_max_rects;
	if (m_rects)
	{
		r->m_rects = OP_NEWA(OpRect, r->m_max_rects);
		if (!r->m_rects)
		{
			OP_DELETE(r);
			return OpStatus::ERR_NO_MEMORY;
		}
		op_memcpy(r->m_rects, m_rects, sizeof(OpRect) * m_max_rects);
	}
	newRegion = r;
	return OpStatus::OK;
}
#endif // SVG_EXTENDED_OP_REGION_API

void OpRegion::IntersectWith(const OpRect& r)
{
	int i;
	for (i = 0; i < m_num_rects; i++)
	{
		m_rects[i].IntersectWith(r);
		if (m_rects[i].IsEmpty())
			RemoveRect(i--);
	}
}

BOOL OpRegion::Expand(unsigned int extra_capacity)
{
	unsigned int headroom = m_max_rects - m_num_rects;
	if (headroom >= extra_capacity)
		return TRUE;

	unsigned int new_max_rects = m_max_rects + (extra_capacity - headroom + 3) & ~3;

	return Resize(new_max_rects);
}

BOOL OpRegion::AddRect(const OpRect& rect)
{
	OP_ASSERT(!rect.IsEmpty());

#ifdef _DEBUG
	for (int i = 0; i <m_num_rects; i++)
	{
		// It must not overlap with any other rect. You should have
		// used ExcludeRect before
		OP_ASSERT(!m_rects[i].Intersecting(rect) || i == m_debug_allowed_to_conflict);
	}
#endif // _DEBUG
	if (!GrowIfNeeded())
		return FALSE;
	m_rects[m_num_rects++] = rect;
/*#ifdef _DEBUG
	static maxcount = 0;
	if (num_rects > maxcount)
	{
		maxcount = num_rects;
		P_DEBUG_PRINTF("%d\n", maxcount);
	}
#endif*/
	return TRUE;
}

#ifndef SVG_OPTIMIZE_RENDER_MULTI_PASS
OpRect OpRegion::GetUnionOfIntersectingRects(const OpRect& rect) const
{
	OP_ASSERT(!rect.IsEmpty());
	OpRect result;
	for (int i = 0; i < m_num_rects; i++)
	{
		OpRect intersect = rect;
		intersect.IntersectWith(m_rects[i]);
		result.UnionWith(intersect);
	}
	OP_ASSERT(result.IsEmpty() || rect.Contains(result));
	return result;
}
#endif // !SVG_OPTIMIZE_RENDER_MULTI_PASS

OpRect OpRegion::GetUnionOfRects() const
{
	OpRect result;
	for (int i = 0; i < m_num_rects; i++)
	{
		result.UnionWith(m_rects[i]);
	}
	return result;
}

OP_STATUS OpRegion::GetArrayOfIntersectingRects(const OpRect& rect, OpVector<OpRect>& rect_list) const
{
	OP_ASSERT(!rect.IsEmpty());
	OP_ASSERT(rect_list.GetCount() == 0);

	for (int i = 0; i < m_num_rects; i++)
	{
		OpRect intersect = rect;
		intersect.IntersectWith(m_rects[i]);
		if (!intersect.IsEmpty())
		{
			OpRect* res = OP_NEW(OpRect, (intersect));
			if (!res || OpStatus::IsError(rect_list.Add(res)))
			{
				OP_DELETE(res);
				rect_list.DeleteAll();
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	return OpStatus::OK;
}

void OpRegion::RemoveRect(int index)
{
	// This mustn't change the order of the rects since it's used by
	// RemoveRect while iterating over some rects.
	OP_ASSERT(index >= 0 && index < m_num_rects);
	for(int i = index; i < m_num_rects - 1; i++)
	{
		m_rects[i] = m_rects[i + 1];
	}
	m_num_rects--;
}

BOOL OpRegion::RemoveRect(const OpRect& rect_to_remove)
{
	OP_ASSERT(!rect_to_remove.IsEmpty());

	unsigned int grow_with = 0;
	unsigned int isect_start;
	if (!Partition(rect_to_remove, isect_start, grow_with))
	{
		// rect_to_remove is contained entirely by one of the rects.
		// Move it to the last index and fixup for next stage
		OpRect tmp = m_rects[isect_start];
		m_rects[isect_start] = m_rects[m_num_rects-1];
		m_rects[m_num_rects-1] = tmp;

		isect_start = m_num_rects-1;
		grow_with = 3;
	}

	if (!Expand(grow_with))
		return FALSE;

	if (!RemoveOverlap(rect_to_remove, isect_start))
		return FALSE;

	CoalesceRects(); // Brrr
	return TRUE;
}

BOOL OpRegion::Includes(int x, int y) const
{
	OpPoint p(x, y);
	for (int i = 0; i < m_num_rects; i++)
	{
		if (m_rects[i].Contains(p))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL OpRegion::Partition(const OpRect& rect, unsigned int& isect_start, unsigned int& grow_with)
{
	//
	// Three sets of rectangles are used:
	// non-overlapping (N), remaining (R), overlapping (O)
	//
	// The m_rects array will be partitioned like this during this pass:
	// |N...|R...|O...|
	//
	// swap_p points at the first rect in the O-set while i points at
	// the first element in the R-set.
	//
	isect_start = m_num_rects;
	unsigned int i = 0;

	while (isect_start > i)
	{
		if (!rect.Intersecting(m_rects[i]))
		{
			i++;
			continue;
		}

		if (m_rects[i].Contains(rect))
		{
			// We can exit; signal position if caller wants to know
			isect_start = i;
			return FALSE;
		}

		if (rect.Contains(m_rects[i]))
		{
			// Move R[last] (if exists) into N[i], and O[last] (if
			// exists) into the position previously occupied by
			// R[last]
			isect_start--;

			if (isect_start > i)
				m_rects[i] = m_rects[isect_start];

			m_num_rects--;

			if (isect_start != (unsigned int)m_num_rects)
				m_rects[isect_start] = m_rects[m_num_rects];
		}
		else /* if (rect.Intersecting(m_rects[i])) */
		{
			grow_with += 2; // Overly pessimistic, improve

			isect_start--;

			if (isect_start > i)
			{
				OpRect tmp = m_rects[i];
				m_rects[i] = m_rects[isect_start];
				m_rects[isect_start] = tmp;
			}
			// else, no need to swap, and we're done
		}
	}
	return TRUE;
}

BOOL OpRegion::RemoveOverlap(const OpRect& rect, unsigned int isect_start)
{
	unsigned int i = isect_start; // Rectangles that intersect start at isect_start
	unsigned int intersecting_rects = m_num_rects - isect_start;
	OpRegion r_split;
	while (intersecting_rects > 0)
	{
		if (!r_split.ExcludeRect(m_rects[i], rect))
			return FALSE;

		if (!r_split.IsEmpty())
		{
			// The first rect go in the old slot
			m_rects[i] = r_split.m_rects[0];

			// Any possible other rects go at the tail
			for (int j = 1; j < r_split.m_num_rects; j++)
			{
				if (!AddRect(r_split.m_rects[j]))
					return FALSE;
			}

			// Quicker than calling Empty(), and avoids allocating memory
			// in a possible next iteration
			r_split.m_num_rects = 0;

			i++;
		}
		else
		{
			// Remove this rect entirely
			m_rects[i] = m_rects[--m_num_rects];
		}

		intersecting_rects--;
	}
	return TRUE;
}

BOOL OpRegion::IncludeRect(const OpRect& rect)
{
	OP_ASSERT(!rect.IsEmpty());

	// Pass 1: Partition the rectangles into overlapping / non-overlapping
	unsigned int grow_with = 1;
	unsigned int isect_start;

	if (!Partition(rect, isect_start, grow_with))
		return TRUE; // rect is already entirely contained in the region

	// Pass 2: Go through the rects that intersected, and remove overlap
	if (!Expand(grow_with))
		return FALSE;

	if (!RemoveOverlap(rect, isect_start))
		return FALSE;

	return AddRect(rect);
}

BOOL OpRegion::IncludeRegion(const OpRegion& region)
{
	BOOL ret = TRUE;
	OpRegionIterator it(&region);

	if (it.First())
	{
		do
			ret = IncludeRect(it.GetRect());
		while (ret && it.Next());
	}

	return ret;
}

BOOL OpRegion::ExcludeRect(const OpRect& rect, const OpRect& c_remove)
{
	OP_ASSERT(rect.Intersecting(c_remove));
#ifdef _DEBUG
	for (int debug_no = 0; debug_no < m_num_rects; debug_no++)
	{
		OP_ASSERT(&rect != m_rects+debug_no);
		// OP_ASSERT(&c_remove != m_rects+debug_no);
	}
#endif // _DEBUG

	OpRect remove = c_remove;
	remove.IntersectWith(rect);

	// Top
	if (remove.y > rect.y)
	{
		if (!AddRect(OpRect(rect.x, rect.y, rect.width, remove.y - rect.y)))
		{
			return FALSE;
		}
	}

	// Left
	if (remove.x > rect.x)
	{
		if (!AddRect(OpRect(rect.x, remove.y, remove.x - rect.x, remove.height)))
		{
			return FALSE;
		}
	}

	// Right
	if (remove.x + remove.width < rect.x + rect.width)
	{
		if (!AddRect(OpRect(remove.x + remove.width, remove.y, rect.x + rect.width - (remove.x + remove.width), remove.height)))
		{
			return FALSE;
		}
	}

	// Bottom
	if (remove.y + remove.height < rect.y + rect.height)
	{
		if (!AddRect(OpRect(rect.x, remove.y + remove.height, rect.width, rect.y + rect.height - (remove.y + remove.height))))
		{
			return FALSE;
		}
	}

	return TRUE;
}

void OpRegion::CoalesceRects()
{
	// Melt together rectangles
	for(int i = 0; i < m_num_rects; i++)
	{
		for(int j = 0; j < m_num_rects; j++)
		{
			if (i == j)
			{
				continue;
			}
			if (i > m_num_rects - 1)
			{
				// m_num_rects might have changed
				break;
			}
			// Only melt together rects that are above themselves
			// since those are the ones that are created in excess by
			// the algorithm in ExcludeRect
			if (m_rects[i].x == m_rects[j].x &&
				m_rects[i].width == m_rects[j].width &&
				((m_rects[i].y + m_rects[i].height == m_rects[j].y) ||
				 (m_rects[j].y + m_rects[j].height == m_rects[i].y)))
			{
				m_rects[i].UnionWith(m_rects[j]);
				RemoveRect(j);
				j--;
			}
		}
	}
	TryShrink();
}

void OpRegion::Translate(int tx, int ty)
{
	OpPoint ofs(tx, ty);
	for (int i = 0; i < m_num_rects; i++)
		m_rects[i].OffsetBy(ofs);
}
