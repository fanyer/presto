/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Partially stolen from Emil's code in libgogi.
 */
#ifndef _OP_REGION_H
#define _OP_REGION_H
#include "modules/util/adt/opvector.h"

class OpRect;

class OpRegion;

class OpRegionIterator
{
public:
	OpRegionIterator(const OpRegion* region) :
		m_region(region), m_idx(UINT_MAX) {}

	BOOL First();
	BOOL Next();

	const OpRect& GetRect() const;

private:
	const OpRegion* m_region;
	unsigned int m_idx;
};

class OpRegion
{
	friend class OpRegionIterator;
public:
	OpRegion();
	~OpRegion();

	/**
	 * Remove a rectangle from the region.
	 */
	BOOL RemoveRect(const OpRect& rect);

	/**
	 * Add a rectangle to the region. If the rectangle is
	 * overlapping other rectangles in the region, rect will be
	 * sliced up so the region contains no overlapping
	 * rectangles. returns true if success.
	 *
	 * @param rect The rect to include in the region. Must not be
	 * empty.
	 */
	BOOL IncludeRect(const OpRect& rect);

	/**
	 * Make this region the union of itself and another region.
	 * The result will not contain overlapping rectangles.
	 *
	 * @param region The region to include.
	 *
	 * @return TRUE on success.
	 */
	BOOL IncludeRegion(const OpRegion& region);

	/**
	 * Checks if a point is a member of the region.
	 */
	BOOL Includes(int x, int y) const;

	/**
	 * Checks if the region is empty.
	 */
	BOOL IsEmpty() const { return m_num_rects == 0; }

	/**
	 * Get a count of non-overlapping rectangles.
	 *
	 * @return Count of non-overlapping rectangles.
	 */
	int GetRectCount() const { return m_num_rects; }

	/**
	 * Empties the region.
	 */
	void Empty();

#ifdef SVG_EXTENDED_OP_REGION_API
	/**
	 * Allocates a copy of the current region and assigns newRegion
	 * to this copy.
	 * @param newRegion A pointer that will point to the newly
	 *                  allocated OpRegion on return
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS Duplicate(OpRegion* &newRegion) const;
#endif // SVG_EXTENDED_OP_REGION_API

	/**
	 * Intersect each of the rectangles in the region with the given
	 * rectangle.
	 */
	void IntersectWith(const OpRect& r);

#ifndef SVG_OPTIMIZE_RENDER_MULTI_PASS
	/**
	 * This method intersects |rect| with every part of the region and
	 * then returns the union of the results. It can be compared to an
	 * |Intersect| method but does not have the guarantee that the
	 * returned rect will only contain points in both the opregion and
	 * the rect.
	 */
	OpRect GetUnionOfIntersectingRects(const OpRect& rect) const;
#endif // !SVG_OPTIMIZE_RENDER_MULTI_PASS

	/**
	 * This method returns the union of the all parts of the region.
	 * The result will be a rect enclosing the region.
	 */
	OpRect GetUnionOfRects() const;

	/** Return region's intersection with a rectangle.
	 *
	 * @param rect The rectangle with which to intersect this region.
	 *
	 * @param rect_list An empty vector to which to add OpRect pointers, which
	 * the caller shall be responsible for deleting on success (e.g. by using an
	 * OpAutoVector as rect_list); on failure, rect_list shall be emptied using
	 * DeleteAll().
	 *
	 * @return See OpStatus; may OOM.
	 */
	OP_STATUS GetArrayOfIntersectingRects(const OpRect& rect, OpVector<OpRect>& rect_list) const;

	/**
	 * Optimize the region's representation.
	 */
	void CoalesceRects();

	/**
	 * Translate the region.
	 */
	void Translate(int tx, int ty);

	/**
	 * Get an iterator able to iterate the rects making up the region.
	 * The rects can be in any order, but are guaranteed to be
	 * non-overlapping.
	 */
	OpRegionIterator GetIterator() const { return OpRegionIterator(this); }

private:
	OpRect *m_rects;
	int m_num_rects;
	int m_max_rects;

	/**
	 * Remove a rectangle from the region.
	 */
	void RemoveRect(int index);

	/**
	 * Resize the rect array to contain to specified amount
	 */
	BOOL Resize(unsigned int new_capacity);

	/**
	 * Increase capacity to hold atleast extra_capacity more rects
	 */
	BOOL Expand(unsigned int extra_capacity);

	/**
	 * Try to shrink the rect array
	 */
	BOOL TryShrink();

	/**
	 * Increase capacity to hold atleast one more rect
	 */
	BOOL GrowIfNeeded() { return (m_num_rects != m_max_rects) || Expand(m_max_rects == 0 ? 1 : 4); }

	/**
	 * Add a rectangle to the region. returns true if success.
	 */
	BOOL AddRect(const OpRect& rect);

	/**
	 * Will split up the existing rectangles into smaller
	 * rectangles to exclude remove. returns TRUE if success.
	 */
	BOOL ExcludeRect(const OpRect& rect, const OpRect& remove);

	/**
	 * Partition the rectangles into intersecting / non-intersecting
	 * based on input rect. The start index of the intersecting half
	 * and an estimated amount to grow the rectangle array is returned.
	 *
	 * @returns FALSE if the rect is contained by a rectangle in the region, otherwise TRUE
	 */
	BOOL Partition(const OpRect& rect, unsigned int& isect_start, unsigned int& grow_with);

	/**
	 * Remove the overlap between rect and the region. Assumes that
	 * data is partitioned, and that the overlapping rectangles start
	 * at isect_start
	 *
	 * @returns FALSE on OOM, otherwise TRUE
	 */
	BOOL RemoveOverlap(const OpRect& rect, unsigned int isect_start);

#ifdef _DEBUG
	int m_debug_allowed_to_conflict;
#endif // _DEBUG
};

inline BOOL OpRegionIterator::First()
{
	m_idx = 0;
	return !m_region->IsEmpty();
}

inline BOOL OpRegionIterator::Next()
{
	m_idx++;
	return m_idx < (unsigned int)m_region->m_num_rects;
}

inline const OpRect& OpRegionIterator::GetRect() const
{
	return m_region->m_rects[m_idx];
}

#endif // _OP_REGION_H
