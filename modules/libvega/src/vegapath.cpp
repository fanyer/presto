/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef VEGA_SUPPORT
#include "modules/libvega/vegaconfig.h"

#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegarenderer.h"
#include "modules/libvega/vegatransform.h"
#include "modules/libvega/src/vegasweepline.h"

#ifdef VEGA_3DDEVICE
# include "modules/libvega/src/vegatriangulate.h"
#endif // VEGA_3DDEVICE

#if VEGA_LINE_ALLOCATION_SIZE == 32
#define VEGA_LINE_ALLOCATION_SIZE_SHIFT 5
#define VEGA_LINE_ALLOCATION_SIZE_MASK 0x1f
#elif VEGA_LINE_ALLOCATION_SIZE == 64
#define VEGA_LINE_ALLOCATION_SIZE_SHIFT 6
#define VEGA_LINE_ALLOCATION_SIZE_MASK 0x3f
#elif VEGA_LINE_ALLOCATION_SIZE == 128
#define VEGA_LINE_ALLOCATION_SIZE_SHIFT 7
#define VEGA_LINE_ALLOCATION_SIZE_MASK 0x7f
#elif VEGA_LINE_ALLOCATION_SIZE == 256
#define VEGA_LINE_ALLOCATION_SIZE_SHIFT 8
#define VEGA_LINE_ALLOCATION_SIZE_MASK 0xff
#elif VEGA_LINE_ALLOCATION_SIZE == 512
#define VEGA_LINE_ALLOCATION_SIZE_SHIFT 9
#define VEGA_LINE_ALLOCATION_SIZE_MASK 0x1ff
#endif

// IMPORTANT: The VEGA_LINE_DATA macro is only safe to use if only the end point of the line is required
#ifdef VEGA_LINE_ALLOCATION_SIZE_SHIFT
#define VEGA_LINE_ATTRIBUTE(x) (this->lineData[(x)>>VEGA_LINE_ALLOCATION_SIZE_SHIFT]->lineAttributes[(x)&VEGA_LINE_ALLOCATION_SIZE_MASK])
#define VEGA_LINE_DATA(x) (this->lineData[(x)>>VEGA_LINE_ALLOCATION_SIZE_SHIFT]->lines+(((x)&VEGA_LINE_ALLOCATION_SIZE_MASK)<<1))
#else
#define VEGA_LINE_ATTRIBUTE(x) (this->lineData[(x)/VEGA_LINE_ALLOCATION_SIZE]->lineAttributes[(x)%VEGA_LINE_ALLOCATION_SIZE])
#define VEGA_LINE_DATA(x) (this->lineData[(x)/VEGA_LINE_ALLOCATION_SIZE]->lines+(((x)%VEGA_LINE_ALLOCATION_SIZE)<<1))
#endif

VEGAPath::VEGAPath() : lastSubPathStart(0), numLines(0),
			lineData(NULL), allocatedLineBlocks(0),
			started(false), multipleSubPaths(false),
#if defined(VEGA_3DDEVICE)
			triangulationData(NULL),
#endif // VEGA_3DDEVICE
			lineJoin(VEGA_LINEJOIN_BEVEL), lineCap(VEGA_LINECAP_BUTT),
			miterLimit(0), lineWidth(VEGA_INTTOFIX(1))
#ifdef VEGA_3DDEVICE
			, m_category(UNDETERMINED), cannotBeConverted(false)
#endif // VEGA_3DDEVICE
            , m_bbox_valid(false)
{
}

VEGAPath::~VEGAPath()
{
	OP_ASSERT(lineData || allocatedLineBlocks == 0);
	for (unsigned int i = 0; i < allocatedLineBlocks; ++i)
		freeLineData(lineData[i]);
	OP_DELETEA(lineData);
#if defined(VEGA_3DDEVICE)
	OP_DELETE(triangulationData);
#endif // VEGA_3DDEVICE
}

OP_STATUS VEGAPath::buildConstrainedPath(VEGAPath& constrained, const VEGATransform& transform, bool on_x, bool on_y)
{
	OP_ASSERT(isClosed());
#ifdef VEGA_3DDEVICE
#ifdef DEBUG_ENABLE_OPASSERT
	if (m_category != UNDETERMINED || OpStatus::IsSuccess(determineCategory()))
		OP_ASSERT(m_category == CONVEX);
#endif // DEBUG_ENABLE_OPASSERT
#endif // VEGA_3DDEVICE
	OP_ASSERT(on_x || on_y);

	VEGAPath t1;
	if (on_x && on_y)
	{
		VEGAPath t2, t3;
		RETURN_IF_ERROR(constrain(t1, transform, true,  VEGA_INTTOFIX(0), false));
		RETURN_IF_ERROR(t1.constrain(t2, transform, true,  VEGA_INTTOFIX(1), true));
		RETURN_IF_ERROR(t2.constrain(t3, transform, false, VEGA_INTTOFIX(0), false));
		return t3.constrain(constrained, transform, false, VEGA_INTTOFIX(1), true);
	}
	RETURN_IF_ERROR(constrain(t1, transform, on_x, VEGA_INTTOFIX(0), false));
	return t1.constrain(constrained, transform, on_x, VEGA_INTTOFIX(1), true);
}

static
bool checkIntersectLine(const VEGATransform& transform, size_t idx,
                        VEGA_FIX boundary, bool upper,
                        bool was_inside,
                        VEGA_FIX* pl, VEGA_FIX* l,
                        VEGA_FIX& pv, VEGA_FIX& px, VEGA_FIX& py)
{
	VEGA_FIX va[2] = { l[VEGALINE_STARTX], l[VEGALINE_STARTY] };
	transform.apply(va[0], va[1]);
	VEGA_FIX v = va[idx];
	const bool inside = upper ? (v <= boundary) : (v >= boundary);
	const bool intersects = (inside != was_inside);

	if (intersects)
	{
		// intersect line with boundary
		const VEGA_FIX k = (boundary - pv) / (v - pv);
		OP_ASSERT(k >= VEGA_INTTOFIX(0) && k <= VEGA_INTTOFIX(1));

		px = pl[VEGALINE_STARTX] + k*(l[VEGALINE_STARTX] - pl[VEGALINE_STARTX]);
		py = pl[VEGALINE_STARTY] + k*(l[VEGALINE_STARTY] - pl[VEGALINE_STARTY]);
	}

	pv = v;
	return intersects;
}

OP_STATUS VEGAPath::constrain(VEGAPath& constrained, const VEGATransform& transform, bool on_x,
                              VEGA_FIX boundary, bool upper)
{
	const size_t idx = on_x ? 0 : 1;
	OP_ASSERT(idx == 0 || idx == 1);

	if (!numLines) // can happen if a previous call cut away entire path
		return OpStatus::OK;
	OP_ASSERT(isClosed());
#ifdef VEGA_3DDEVICE
#ifdef DEBUG_ENABLE_OPASSERT
	if (m_category != UNDETERMINED || OpStatus::IsSuccess(determineCategory()))
		OP_ASSERT(m_category == CONVEX);
#endif // DEBUG_ENABLE_OPASSERT
#endif // VEGA_3DDEVICE

	OP_ASSERT(!constrained.getNumLines());
	RETURN_IF_ERROR(constrained.prepare(2*numLines)); // should be enough most of the time

	VEGA_FIX* pl   = getLine(0);
	VEGA_FIX pva[2] = { pl[VEGALINE_STARTX], pl[VEGALINE_STARTY] };
	transform.apply(pva[0], pva[1]);
	VEGA_FIX pv = pva[idx];

	unsigned int points = 0;
	bool inside = upper ? (pv <= boundary) : (pv >= boundary);
	if (inside)
	{
		constrained.moveTo(pl[VEGALINE_STARTX], pl[VEGALINE_STARTY]);
		++ points;
	}

	VEGA_FIX px, py;
	for (unsigned int i = 1; i < numLines; ++i)
	{
		VEGA_FIX* l = getLine(i);
		if (checkIntersectLine(transform, idx, boundary, upper, inside, pl, l, pv, px, py))
		{
			inside = !inside;
			if (points)
				RETURN_IF_ERROR(constrained.lineTo(px, py));
			else
				RETURN_IF_ERROR(constrained.moveTo(px, py));
			++ points;
		}

		if (inside)
		{
			px = l[VEGALINE_STARTX];
			py = l[VEGALINE_STARTY];

			if (points)
				RETURN_IF_ERROR(constrained.lineTo(px, py));
			else
				RETURN_IF_ERROR(constrained.moveTo(px, py));
			++ points;
		}

		pl = l;
	}

	if (!constrained.getNumLines())
		return OpStatus::OK;

	// must go back to first line of path
	if (checkIntersectLine(transform, idx, boundary, upper, inside, pl, getLine(0), pv, px, py))
		RETURN_IF_ERROR(constrained.lineTo(px, py));

	RETURN_IF_ERROR(constrained.close(true));

	// constrained path will always be convex
#ifdef VEGA_3DDEVICE
#ifdef DEBUG_ENABLE_OPASSERT
	if (m_category != UNDETERMINED || OpStatus::IsSuccess(determineCategory()))
		OP_ASSERT(m_category == CONVEX);
#endif // DEBUG_ENABLE_OPASSERT
	forceConvex();
#endif // VEGA_3DDEVICE
	return OpStatus::OK;
}

OP_STATUS VEGAPath::copy(const VEGAPath *other)
{
	unsigned int i;
	for (i = 0; i < allocatedLineBlocks; ++i)
		freeLineData(lineData[i]);
	OP_DELETEA(lineData);

	OP_STATUS status = OpStatus::OK;
#if defined(VEGA_3DDEVICE)
	OP_DELETE(triangulationData);
	triangulationData = NULL;

	if (other->triangulationData)
	{
		triangulationData = OP_NEW(VEGATriangulationData, ());
		// FIXME: report OOM? cloning triangles is not crucial...
		if (triangulationData &&
			OpStatus::IsError(triangulationData->copy(other->triangulationData)))
		{
			OP_DELETE(triangulationData);
			triangulationData = NULL;
		}
	}
#endif // VEGA_3DDEVICE

	lastSubPathStart = other->lastSubPathStart;

	lineData = OP_NEWA(LineData*, other->allocatedLineBlocks);
	if (lineData)
	{
		for (i = 0; i < other->allocatedLineBlocks; ++i)
		{
			lineData[i] = allocLineData();
			if (!lineData[i])
				status = OpStatus::ERR_NO_MEMORY;
		}
	}

	// Check for OOM errors from allocations above
	if (!lineData || OpStatus::IsError(status))
	{
		if (lineData)
			for (i = 0; i < other->allocatedLineBlocks; ++i)
				freeLineData(lineData[i]);
		OP_DELETEA(lineData);
		lineData = NULL;
		numLines = allocatedLineBlocks = 0;
#ifdef VEGA_3DDEVICE
		m_category = UNDETERMINED;
#endif // VEGA_3DDEVICE

#if defined(VEGA_3DDEVICE)
		OP_DELETE(triangulationData);
		triangulationData = NULL;
#endif // VEGA_3DDEVICE
		return OpStatus::ERR_NO_MEMORY;
	}

	numLines = other->numLines;
#ifdef VEGA_3DDEVICE
	m_category = other->m_category;
	cannotBeConverted = other->cannotBeConverted;
#endif // VEGA_3DDEVICE
	allocatedLineBlocks = other->allocatedLineBlocks;

	if (other->lineData)
	{
		for (unsigned int i = 0; i < allocatedLineBlocks; ++i)
			op_memcpy(lineData[i], other->lineData[i], sizeof(LineData));
	}

	started = other->started;
	multipleSubPaths = other->multipleSubPaths;

	lineJoin = other->lineJoin;
	lineCap = other->lineCap;
	miterLimit = other->miterLimit;
	lineWidth = other->lineWidth;

	m_bbox_valid = other->m_bbox_valid;
	if (m_bbox_valid)
	{
		m_bbox_x = other->m_bbox_x;
		m_bbox_y = other->m_bbox_y;
		m_bbox_w = other->m_bbox_w;
		m_bbox_h = other->m_bbox_h;
	}

	return OpStatus::OK;
}

OP_STATUS VEGAPath::prepare(unsigned int numLines)
{
	started = false;
	this->numLines = 0;
	multipleSubPaths = false;

#if defined(VEGA_3DDEVICE)
	m_category = UNDETERMINED;
	cannotBeConverted = false;
	markSubPathInfoInvalid();
	markTriangulationInfoInvalid();
#endif // VEGA_3DDEVICE

	lastSubPathStart = 0;
	m_bbox_valid = false;

	for (unsigned int i = 0; i < allocatedLineBlocks; ++i)
		lineData[i]->numCachedLineLens = 0;

	while (numLines > allocatedLineBlocks*VEGA_LINE_ALLOCATION_SIZE)
	{
		RETURN_IF_ERROR(increaseLineAllocation());
	}
	return OpStatus::OK;
}

bool VEGAPath::isPointInside(VEGA_FIX x, VEGA_FIX y, bool xorFill)
{
	if (!numLines)
		return false;
	return isPointInside(x, y, xorFill, 0, numLines-1);
}
bool VEGAPath::isPointInside(VEGA_FIX x, VEGA_FIX y, bool xorFill,
							 unsigned startIdx, unsigned endIdx)
{
	int numintpos = 0;
	int numintneg = 0;

	for (unsigned int cl = startIdx; cl <= endIdx; ++cl)
	{
		if (VEGA_LINE_ATTRIBUTE(cl) & VEGA_LINE_ATTR_WARP)
			continue;

		// Only check lines which start above and end on or below the vertex to avoid problems
		// with lines which intersect at the y coordinate of the vertex.
		VEGA_FIX* clline = getLine(cl);
		if ((clline[VEGALINE_STARTY] < y && clline[VEGALINE_ENDY] >= y) ||
			(clline[VEGALINE_STARTY] >= y && clline[VEGALINE_ENDY] < y))
		{
			VEGA_FIX fix_dx, fix_dy, fix_ldx, fix_ldy;
			bool isUp = clline[VEGALINE_STARTY] > clline[VEGALINE_ENDY];
			if (isUp)
			{
				fix_dy = clline[VEGALINE_ENDY] - clline[VEGALINE_STARTY];
				fix_dx = clline[VEGALINE_ENDX] - clline[VEGALINE_STARTX];
				fix_ldy = y - clline[VEGALINE_STARTY];
				fix_ldx = x - clline[VEGALINE_STARTX];
			}
			else
			{
				fix_dy = clline[VEGALINE_STARTY] - clline[VEGALINE_ENDY];
				fix_dx = clline[VEGALINE_STARTX] - clline[VEGALINE_ENDX];
				fix_ldy = y - clline[VEGALINE_ENDY];
				fix_ldx = x - clline[VEGALINE_ENDX];
			}

			VEGA_FIX sign = VEGA_VECDOTSIGN(-fix_dy, fix_dx, fix_ldx, fix_ldy);

			// On boundary - considered inside.
			if (sign == 0)
				return true;

			if (sign < 0)
			{
				if (isUp)
					++numintpos;
				else
					++numintneg;
			}
		}
		// Is the point sitting on the (possibly extended) horizontal line?
		else if (clline[VEGALINE_STARTY] == y && clline[VEGALINE_ENDY] == y)
		{
			VEGA_FIX sx = clline[VEGALINE_STARTX];
			VEGA_FIX ex = clline[VEGALINE_ENDX];
			VEGA_FIX fix_dx = ex - sx;
			VEGA_FIX fix_ldx = x - sx;

			// Within line segment?
			// 0 <= (x - sx) * dx / dx * dx <= 1 ->
			// 0 <= sign(dx) * (x - sx) * |dx| <= |dx|^2
			if (fix_dx < 0)
				fix_ldx = -fix_ldx, fix_dx = -fix_dx;

			if (fix_ldx >= 0 && fix_ldx <= fix_dx)
				return true;
		}
	}

	if (xorFill)
		return ((numintpos+numintneg)&1) != 0;
	else
		return (numintpos != numintneg);
}

/* region codes for the below method */
#define L_EDGE 0x1
#define R_EDGE 0x2
#define T_EDGE 0x4
#define B_EDGE 0x8

/** test whether the path intersects the given rectangle */
bool VEGAPath::intersects(VEGA_FIX minx, VEGA_FIX miny,
						  VEGA_FIX maxx, VEGA_FIX maxy, bool xorFill)
{
	for (unsigned int cl = 0; cl < numLines; ++cl)
	{
		if (VEGA_LINE_ATTRIBUTE(cl) & VEGA_LINE_ATTR_WARP)
			continue;

		VEGA_FIX* clline = getLine(cl);
		VEGA_FIX fix_x1 = clline[VEGALINE_STARTX];
		VEGA_FIX fix_y1 = clline[VEGALINE_STARTY];

		VEGA_FIX fix_x2 = clline[VEGALINE_ENDX];
		VEGA_FIX fix_y2 = clline[VEGALINE_ENDY];

		unsigned char region1 = 0, region2 = 0;

		if (fix_x1 < minx)
			region1 |= L_EDGE;
		else if (fix_x1 > maxx)
			region1 |= R_EDGE;
		if (fix_y1 < miny)
			region1 |= T_EDGE;
		else if (fix_y1 > maxy)
			region1 |= B_EDGE;

		if (fix_x2 < minx)
			region2 |= L_EDGE;
		else if (fix_x2 > maxx)
			region2 |= R_EDGE;
		if (fix_y2 < miny)
			region2 |= T_EDGE;
		else if (fix_y2 > maxy)
			region2 |= B_EDGE;

		/* the two points lie on the same side of one of the halfplanes */
		if (region1 & region2)
			continue;

		/* (atleast) one the points are inside the rect */
		if (region1 == 0 || region2 == 0)
			return true;

		VEGA_FIX t1, t2;
		VEGA_FIX div;

		if ((region1 | region2) & (L_EDGE | R_EDGE))
		{
			// Case with dx == 0
			div = VEGA_FIXMUL(maxy - miny, fix_x2 - fix_x1);
			if (div != 0) // if div is 0, the lines are parallel
			{
				t2 = VEGA_FIXMUL(fix_x2 - fix_x1, fix_y1 - miny);
				if ((region1 | region2) & L_EDGE)
				{
					t1 = VEGA_FIXMUL(miny - maxy, fix_x1 - minx);
					t2 -= VEGA_FIXMUL(fix_y2 - fix_y1, fix_x1 - minx);
				}
				else
				{
					t1 = VEGA_FIXMUL(miny - maxy, fix_x1 - maxx);
					t2 -= VEGA_FIXMUL(fix_y2 - fix_y1, fix_x1 - maxx);
				}

				if (div < 0)
				{
					t1 = -t1;
					t2 = -t2;
					div = -div;
				}
				if (t1 > VEGA_INTTOFIX(0) && t1 < div &&
					t2 > VEGA_INTTOFIX(0) && t2 < div)
					return true;
			}
		}
		if ((region1 | region2) & (T_EDGE | B_EDGE))
		{
			// Case with dy == 0
			div = VEGA_FIXMUL(minx - maxx, fix_y2 - fix_y1);
			if (div != 0) // if div is 0, the lines are parallel
			{
				if ((region1 | region2) & T_EDGE)
				{
					t1 = VEGA_FIXMUL(maxx - minx, fix_y1 - miny);
					t2 = VEGA_FIXMUL(fix_x2 - fix_x1, fix_y1 - miny);
				}
				else
				{
					t1 = VEGA_FIXMUL(maxx - minx, fix_y1 - maxy);
					t2 = VEGA_FIXMUL(fix_x2 - fix_x1, fix_y1 - maxy);
				}
				t2 -= VEGA_FIXMUL(fix_y2 - fix_y1, fix_x1 - minx);

				if (div < 0)
				{
					t1 = -t1;
					t2 = -t2;
					div = -div;
				}
				if (t1 > VEGA_INTTOFIX(0) && t1 < div &&
					t2 > VEGA_INTTOFIX(0) && t2 < div)
					return true;
			}
		}
	}

	/* test for the case where the path encloses the rectangle */
	if (isPointInside(minx, miny, xorFill))
		return true;
#if 0
	// FIXME: do we need more than one test?
	if (isPointInside(minx, maxy, xorFill))
		return true;
	if (isPointInside(maxx, miny, xorFill))
		return true;
	if (isPointInside(maxx, maxy, xorFill))
		return true;
#endif

	return false;
}

/** test whether the path is enclosed by the given rectangle */
bool VEGAPath::isEnclosed(VEGA_FIX minx, VEGA_FIX miny,
						  VEGA_FIX maxx, VEGA_FIX maxy)
{
	for (unsigned int cl = 0; cl < numLines; ++cl)
	{
		if (VEGA_LINE_ATTRIBUTE(cl) & VEGA_LINE_ATTR_WARP)
			continue;

		VEGA_FIX* clline = getLine(cl);
		VEGA_FIX fix_x1 = clline[VEGALINE_STARTX];
		VEGA_FIX fix_y1 = clline[VEGALINE_STARTY];

		VEGA_FIX fix_x2 = clline[VEGALINE_ENDX];
		VEGA_FIX fix_y2 = clline[VEGALINE_ENDY];

		/* the line crosses / is outside the boundrary */
		if (fix_x1 < minx || fix_x1 > maxx ||
			fix_y1 < miny || fix_y1 > maxy)
			return false;

		if (fix_x2 < minx || fix_x2 > maxx ||
			fix_y2 < miny || fix_y2 > maxy)
			return false;
	}
	return true;
}

/** add a move to. returns ERR_NO_MEMORY on OOM. */
OP_STATUS VEGAPath::moveTo(VEGA_FIX x, VEGA_FIX y)
{
	m_bbox_valid = false;
#ifdef VEGA_3DDEVICE
	markSubPathInfoInvalid();
#endif // VEGA_3DDEVICE

	if (!started)
	{
		OP_ASSERT(!multipleSubPaths);
		if (!allocatedLineBlocks)
		{
			RETURN_IF_ERROR(increaseLineAllocation());
		}
		lineData[0]->lines[0] = x;
		lineData[0]->lines[1] = y;
		started = true;

		return OpStatus::OK;
	}
	if (!lineData)
	{
		// this is not oom
		return OpStatus::ERR;
	}

	// This will start a new sub-path and leave the old one open
	RETURN_IF_ERROR(close(false));
	RETURN_IF_ERROR(warpTo(x, y));
	lastSubPathStart = numLines;
	multipleSubPaths = true;
#ifdef VEGA_3DDEVICE
	m_category = COMPLEX;
#endif // VEGA_3DDEVICE

	return OpStatus::OK;
}

/** add a line to. returns false on OOM. */
OP_STATUS VEGAPath::lineTo(VEGA_FIX x, VEGA_FIX y)
{
	if (!lineData || !started)
	{
		// this is not oom
		return OpStatus::ERR;
	}
	if (numLines == 0)
	{
		if (x == lineData[0]->lines[0] && y == lineData[0]->lines[1])
			return OpStatus::OK;
	}
	else
	{
		VEGA_FIX* lastLine = VEGA_LINE_DATA(numLines-1);
		if (x == lastLine[VEGALINE_ENDX] && y == lastLine[VEGALINE_ENDY])
			return OpStatus::OK;
	}

	m_bbox_valid = false;

#if defined(VEGA_3DDEVICE)
	markSubPathInfoInvalid();
	markTriangulationInfoInvalid();
#endif // VEGA_3DDEVICE

	while (numLines >= allocatedLineBlocks*VEGA_LINE_ALLOCATION_SIZE)
	{
		RETURN_IF_ERROR(increaseLineAllocation());
	}
	VEGA_FIX* newLine = VEGA_LINE_DATA(numLines);
	newLine[VEGALINE_ENDX] = x;
	newLine[VEGALINE_ENDY] = y;
	VEGA_LINE_ATTRIBUTE(numLines) = 0;
#ifdef VEGA_3DDEVICE
	m_category = UNDETERMINED;
#endif // VEGA_3DDEVICE
	++numLines;

	return OpStatus::OK;
}

/** add a line to. returns false on OOM. */
OP_STATUS VEGAPath::warpTo(VEGA_FIX x, VEGA_FIX y)
{
	if (!lineData || !started)
	{
		// this is not oom
		return OpStatus::ERR;
	}
	VEGA_FIX* lastLine = NULL;
	if (numLines == 0)
	{
		if (x == lineData[0]->lines[0] && y == lineData[0]->lines[1])
			return OpStatus::OK;
	}
	else
	{
		lastLine = VEGA_LINE_DATA(numLines-1);
		if (x == lastLine[VEGALINE_ENDX] && y == lastLine[VEGALINE_ENDY])
			return OpStatus::OK;
	}

	m_bbox_valid = false;

#if defined(VEGA_3DDEVICE)
	// Remove all triangles and intersections
	markTriangulationInfoInvalid();
	markSubPathInfoInvalid();
#endif // VEGA_3DDEVICE

	if (numLines > 0 && (VEGA_LINE_ATTRIBUTE(numLines-1)&VEGA_LINE_ATTR_WARP))
	{
		lastLine[VEGALINE_ENDX] = x;
		lastLine[VEGALINE_ENDY] = y;
		return OpStatus::OK;
	}

	while (numLines >= allocatedLineBlocks*VEGA_LINE_ALLOCATION_SIZE)
	{
		RETURN_IF_ERROR(increaseLineAllocation());
	}
	VEGA_FIX* newLine = VEGA_LINE_DATA(numLines);
	newLine[VEGALINE_ENDX] = x;
	newLine[VEGALINE_ENDY] = y;
	VEGA_LINE_ATTRIBUTE(numLines) = VEGA_LINE_ATTR_WARP | VEGA_LINE_ATTR_NOT_VISIBLE;
#ifdef VEGA_3DDEVICE
	m_category = UNDETERMINED;
#endif // VEGA_3DDEVICE
	++numLines;
	return OpStatus::OK;
}

#ifdef VEGA_USE_PRECISE_ITERATIVE_FLATTENING

#ifdef VEGA_USE_FLOATS
// This is currently the only supported configuration
static inline VEGA_FIX VEGA_FIXCBRT(VEGA_FIX v)
{
	// Here is a version of cbrtf that uses existing Opera
	// 'infrastructure' (which forces the use of doubles)
	double x = (double)v;

	int e;
	double m = op_frexp(x, &e);

	// Compute cube root exponent
	int se = e % 3;
	if (se > 0)
		se -= 3;

	e = (e - se) / 3;
	m = op_ldexp(m, se);

	// Initial guess
	m = (-0.46946116 * m + 1.072302) * m + 0.3812513;
	m = op_ldexp(m, e);

	// Two Newton iterations
	m = (2 * m + x / (m * m)) / 3;
	m = (2 * m + x / (m * m)) / 3;

	return (VEGA_FIX)m;
}
#else
// FIXME: I think the precision for this approximation is not quite up
// to the task
static VEGA_FIX VEGA_FIXCBRT(VEGA_FIX val)
{
	if (val == 0)
		return 0;
	VEGA_FIX ans = VEGA_INTTOFIX(1);

	for ( int i = 0; i < 10; ++i )
	{
		VEGA_FIX newans;
		newans = VEGA_FIXMUL(ans, ans);
		newans = VEGA_FIXDIV(VEGA_FIXMUL(newans, ans)-val, 3*newans);
		newans = ans - newans;
		if (ans == newans)
			return newans;
		ans = newans;
	}
	return ans;
}
#endif // VEGA_USE_FLOATS

/*
 * Below are alternative implementations of flattening for curve segments.
 * The one for cubic beziers are based on the document:
 *
 *    'Precise Flattening of Cubic Bezier Segments', Hain et al.
 *
 * and the quadratic version is derived in a similar manner.
 *
 * These 'flatteners' are supposed to generate less line segment for a
 * curve segment than the recursive (which really needn't be [code]
 * recursive), and should thus amount to less work for the rasterizer.
 *
 * There has been no evaluation of this fact though, and very little
 * testing overall of these versions. The fixed-point version of the
 * cubic flattener was broken last I tried it.
 *
 * /fs
 */

struct VEGACubicBezierSegment
{
	VEGA_FIX sx, sy;
	VEGA_FIX c1x, c1y;
	VEGA_FIX c2x, c2y;
	VEGA_FIX ex, ey;
};

// Split a cubic bezier at parameter <t> into a 'left' [0 t] and a
// 'right' [t 1] curve
static inline void splitCubicBezierAt(const VEGACubicBezierSegment& s, VEGA_FIX t,
									  VEGACubicBezierSegment& l, VEGACubicBezierSegment& r)
{
	l.sx = s.sx;
	l.sy = s.sy;

	l.c1x = s.sx + VEGA_FIXMUL(s.c1x - s.sx, t);
	l.c1y = s.sy + VEGA_FIXMUL(s.c1y - s.sy, t);

	VEGA_FIX m12x = s.c1x + VEGA_FIXMUL(s.c2x - s.c1x, t);
	VEGA_FIX m12y = s.c1y + VEGA_FIXMUL(s.c2y - s.c1y, t);

	l.c2x = l.c1x + VEGA_FIXMUL(m12x - l.c1x, t);
	l.c2y = l.c1y + VEGA_FIXMUL(m12y - l.c1y, t);

	r.c2x = s.c2x + VEGA_FIXMUL(s.ex - s.c2x, t);
	r.c2y = s.c2y + VEGA_FIXMUL(s.ey - s.c2y, t);

	r.c1x = m12x + VEGA_FIXMUL(r.c2x - m12x, t);
	r.c1y = m12y + VEGA_FIXMUL(r.c2y - m12y, t);

	l.ex = r.sx = l.c2x + VEGA_FIXMUL(r.c1x - l.c2x, t);
	l.ey = r.sy = l.c2y + VEGA_FIXMUL(r.c1y - l.c2y, t);

	r.ex = s.ex;
	r.ey = s.ey;
}

OP_STATUS VEGAPath::parabolicApprox(const VEGACubicBezierSegment& in_c, VEGA_FIX flatness)
{
	VEGACubicBezierSegment c = in_c;

	if (numLines == 0)
	{
		c.sx = lineData[0]->lines[0];
		c.sy = lineData[0]->lines[1];
	}
	else
	{
		VEGA_FIX* lastLine = VEGA_LINE_DATA(numLines-1);
		c.sx = lastLine[VEGALINE_ENDX];
		c.sy = lastLine[VEGALINE_ENDY];
	}

	unsigned iterations = 0;
	while (iterations++ < (1 << MAX_LINES_PER_SEGMENT_SHIFT))
	{
		VEGA_FIX v_sc1x = c.c1x - c.sx;
		VEGA_FIX v_sc1y = c.c1y - c.sy;
		VEGA_FIX v_sc2x = c.c2x - c.sx;
		VEGA_FIX v_sc2y = c.c2y - c.sy;

		bool is_degenerate = false;
		VEGA_FIX v1x, v1y, v2x, v2y;
		if (!VEGA_EQ(v_sc1x, 0) || !VEGA_EQ(v_sc1y, 0))
		{
			v1x = v_sc1x;
			v1y = v_sc1y;

			v2x = v_sc2x;
			v2y = v_sc2y;
		}
		// The vector P0 -> P1 is of zero length
		else if (!VEGA_EQ(v_sc2x, 0) || !VEGA_EQ(v_sc2y, 0))
		{
			v1x = v_sc2x;
			v1y = v_sc2y;

			v2x = c.ex - c.sx;
			v2y = c.ey - c.sy;
		}
		// The vector P0 -> P2 is also of zero length
		else
		{
			is_degenerate = true;
		}

		VEGA_FIX t_prim = VEGA_INTTOFIX(1);

		if (!is_degenerate)
		{
			VEGA_FIX s3 = VEGA_FIXMUL(v1x, v2y) - VEGA_FIXMUL(v2x, v1y);
			VEGA_FIX v_len2 = VEGA_FIXMUL(v1x, v1x) + VEGA_FIXMUL(v1y, v1y);

			s3 = VEGA_FIXDIV(s3, VEGA_FIXSQRT(v_len2));
			if (s3 < 0)
				s3 = -s3;

			if (s3 > 0)
			{
				t_prim = VEGA_FIXMUL2(VEGA_FIXSQRT(VEGA_FIXDIV(flatness, 3*s3)));
			}
		}

		if (t_prim >= VEGA_INTTOFIX(1))
			break;

		VEGACubicBezierSegment left, right;

		// Subdivide curve at t_prim
		splitCubicBezierAt(c, t_prim, left, right);

		RETURN_IF_ERROR(lineTo(left.ex, left.ey));

		c = right;
	}

	return lineTo(c.ex, c.ey);
}

static inline void insertSegmentValue(VEGA_FIX* t_vals, char* t_cmds, int i, VEGA_FIX new_t, char new_cmd)
{
	while (i > 0 && t_vals[i-1] > new_t)
	{
		t_vals[i] = t_vals[i-1];
		t_cmds[i] = t_cmds[i-1];

		i--;
	}

	t_vals[i] = new_t;
	t_cmds[i] = new_cmd;
}

static VEGA_FIX calcInflectionFlatnessBound(const VEGACubicBezierSegment& c,
											VEGA_FIX t, VEGA_FIX flatness)
{
	VEGACubicBezierSegment left, right;
	splitCubicBezierAt(c, t, left, right);

	VEGA_FIX ax = -right.sx + 3*(right.c1x - right.c2x) + right.ex;
	VEGA_FIX ay = -right.sy + 3*(right.c1y - right.c2y) + right.ey;

	VEGA_FIX c12x = 3 * (right.c1x - right.c2x);
	VEGA_FIX c12y = 3 * (right.c1y - right.c2y);
	VEGA_FIX c12_len2 = VEGA_FIXSQR(c12x) + VEGA_FIXSQR(c12y);

	VEGA_FIX s4 = VEGA_FIXMUL(c12y, ax) - VEGA_FIXMUL(c12x, ay);
	if (s4 < 0)
		s4 = -s4;

	return VEGA_FIXCBRT(VEGA_FIXDIV(9 * VEGA_FIXMUL(flatness, VEGA_FIXSQRT(c12_len2)),
									6 * s4));
}

OP_STATUS VEGAPath::cubicBezierTo(VEGA_FIX x, VEGA_FIX y, VEGA_FIX c1x, VEGA_FIX c1y, VEGA_FIX c2x, VEGA_FIX c2y, VEGA_FIX flatness)
{
	m_bbox_valid = false;
#ifdef VEGA_3DDEVICE
	markTriangulationInfoInvalid();
	markSubPathInfoInvalid();
#endif // VEGA_3DDEVICE

	VEGA_FIX sx, sy;
	if (numLines == 0)
	{
		sx = lineData[0]->lines[0];
		sy = lineData[0]->lines[1];
	}
	else
	{
		VEGA_FIX* lastLine = VEGA_LINE_DATA(numLines-1);
		sx = lastLine[VEGALINE_ENDX];
		sy = lastLine[VEGALINE_ENDY];
	}

	VEGA_FIX ax = -sx + 3*c1x - 3*c2x + x;
	VEGA_FIX ay = -sy + 3*c1y - 3*c2y + y;
	VEGA_FIX bx = 3*sx - 6*c1x + 3*c2x;
	VEGA_FIX by = 3*sy - 6*c1y + 3*c2y;
	VEGA_FIX cx = -3*sx + 3*c1x;
	VEGA_FIX cy = -3*sy + 3*c1y;

	VEGA_FIX denom = VEGA_FIXMUL(ay, bx) - VEGA_FIXMUL(ax, by);
	VEGA_FIX rcp_denom = VEGA_FIXDIV(VEGA_INTTOFIX(1), denom);

	// t_cusp = -1/2 * (ay * cx - ax * cy) / (ay * bx - ax * by)
	VEGA_FIX t_cusp = VEGA_FIXMUL(VEGA_FIXMUL(ay, cx) - VEGA_FIXMUL(ax, cy), rcp_denom);
	t_cusp = -VEGA_FIXDIV2(t_cusp);

	// t1 = t_cusp - (t_cusp^2 - 1/3 * (by * cx - bx * cy) / (ay * bx - ax * by)) ^ 1/2
	VEGA_FIX num2 = VEGA_FIXMUL(by, cx) - VEGA_FIXMUL(bx, cy);
	VEGA_FIX disc = VEGA_FIXSQR(t_cusp) - VEGA_FIXMUL(num2, rcp_denom) / 3;

	VEGA_FIX s3 = VEGA_FIXMUL(c2x - sx, c1y - sy) - VEGA_FIXMUL(c2y - sy, c1x - sx);
	VEGA_FIX v_sc1_len2 = VEGA_FIXSQR(c1x - sx) + VEGA_FIXSQR(c1y - sy);
	VEGA_FIX rcp_norm = VEGA_FIXDIV(VEGA_INTTOFIX(1), VEGA_FIXSQRT(v_sc1_len2));

	s3 = VEGA_FIXMUL(s3, rcp_norm);
	if (s3 < 0)
		s3 = -s3;

	VEGA_FIX t1_minus, t1_plus;
	VEGA_FIX t2_minus, t2_plus;

	VEGACubicBezierSegment c;

	c.sx = sx;
	c.sy = sy;
	c.c1x = c1x;
	c.c1y = c1y;
	c.c2x = c2x;
	c.c2y = c2y;
	c.ex = x;
	c.ey = y;

	if (disc >= 0)
	{
		disc = VEGA_FIXSQRT(disc);

		VEGA_FIX t1 = t_cusp - disc;
		{
			VEGA_FIX tf = calcInflectionFlatnessBound(c, t1, flatness);

			// tn- = tn - tf * (1 - tn)
			// tn+ = tn + tf * (1 - tn)
			t1_minus = t1 - VEGA_FIXMUL(tf, VEGA_INTTOFIX(1) - t1);
			t1_plus = t1 + VEGA_FIXMUL(tf, VEGA_INTTOFIX(1) - t1);

			if (t1_minus > t1_plus)
			{
				VEGA_FIX tmp = t1_minus;
				t1_minus = t1_plus;
				t1_plus = tmp;
			}
		}

		if (t1_plus <= 0)
			t1_minus = t1_plus = VEGA_INTTOFIX(-1);
		else if (t1_minus >= VEGA_INTTOFIX(1))
			t1_minus = t1_plus = VEGA_INTTOFIX(2);

		VEGA_FIX t2 = t_cusp + disc;
		{
			VEGA_FIX tf = calcInflectionFlatnessBound(c, t2, flatness);

			t2_minus = t2 - VEGA_FIXMUL(tf, VEGA_INTTOFIX(1) - t2);
			t2_plus = t2 + VEGA_FIXMUL(tf, VEGA_INTTOFIX(1) - t2);

			if (t2_minus > t2_plus)
			{
				VEGA_FIX tmp = t2_minus;
				t2_minus = t2_plus;
				t2_plus = tmp;
			}
		}

		if (t2_plus <= 0)
			t2_minus = t2_plus = VEGA_INTTOFIX(-1);
		else if (t2_minus >= VEGA_INTTOFIX(1))
			t2_minus = t2_plus = VEGA_INTTOFIX(2);
	}
	else
	{
		t1_minus = VEGA_INTTOFIX(-1);
		t1_plus = VEGA_INTTOFIX(-1);
		t2_minus = VEGA_INTTOFIX(-1);
		t2_plus = VEGA_INTTOFIX(-1);
	}

	if (VEGA_EQ(s3, 0))
		return lineTo(x, y);

	VEGA_FIX t_vals[6]; /* ARRAY OK 2010-05-27 fs */
	char t_cmds[6]; /* ARRAY OK 2010-05-27 fs */
	int cnt = 2;

	t_vals[0] = 0;
	t_cmds[0] = 0;

	// Even if there is no inflection points in [0 1], we want a curve
	// from t=0 to t=1
	t_vals[1] = VEGA_INTTOFIX(1);
	t_cmds[1] = 2;

	if (t2_plus >= 0)
	{
		insertSegmentValue(t_vals, t_cmds, cnt++, t1_minus, 2);
		if (t2_minus <= t1_plus || VEGA_EQ(t2_minus, t1_plus))
		{
			// Insert cusp instead of t1+ and t2-
			insertSegmentValue(t_vals, t_cmds, cnt++, t_cusp, 1);
		}
		else
		{
			insertSegmentValue(t_vals, t_cmds, cnt++, t1_plus, 1);
			insertSegmentValue(t_vals, t_cmds, cnt++, t2_minus, 2);
		}
		insertSegmentValue(t_vals, t_cmds, cnt++, t2_plus, 1);
	}

	int start = 0;

	// Find start point
	while (t_cmds[start] != 0)
		start++;

	// Reduce tail
	int actually_inserted = cnt;
	while (t_vals[cnt-1] != VEGA_INTTOFIX(1))
		cnt--;

	// If there is an inflection interval that contains t=1, propagate
	// the command from the following t value
	if (cnt != actually_inserted && t_cmds[cnt] != 2)
		t_cmds[cnt-1] = t_cmds[cnt];

	// FIXME: Prune short segments such as [0 eps] or [1-eps 1] et.c

	VEGA_FIX remaining_len = VEGA_INTTOFIX(1);
	for (start++ ; start < cnt; ++start)
	{
		VEGACubicBezierSegment left, right;
		VEGA_FIX local_t = (t_vals[start] - t_vals[start-1]) / remaining_len;

		// Split curve
		splitCubicBezierAt(c, local_t, left, right);

		c = right;

		OP_STATUS status = OpStatus::OK;
		switch (t_cmds[start])
		{
		case 1: // Linear segment
			status = lineTo(left.ex, left.ey);
			break;

		case 2: // Circular segment
			status = parabolicApprox(left, flatness);
			break;

		default:
			OP_ASSERT(!"Unexpected segment value");
		}

		RETURN_IF_ERROR(status);

		remaining_len = VEGA_INTTOFIX(1) - t_vals[start];

		if (remaining_len <= 0)
			break;
	}
	return OpStatus::OK;
}

OP_STATUS VEGAPath::quadraticBezierTo(VEGA_FIX x, VEGA_FIX y, VEGA_FIX cx, VEGA_FIX cy, VEGA_FIX flatness)
{
	m_bbox_valid = false;
#ifdef VEGA_3DDEVICE
	markTriangulationInfoInvalid();
	markSubPathInfoInvalid();
#endif // VEGA_3DDEVICE

	VEGA_FIX sx, sy;
	if (numLines == 0)
	{
		sx = lineData[0]->lines[0];
		sy = lineData[0]->lines[1];
	}
	else
	{
		VEGA_FIX* lastLine = VEGA_LINE_DATA(numLines-1);
		sx = lastLine[VEGALINE_ENDX];
		sy = lastLine[VEGALINE_ENDY];
	}

	unsigned iterations = 0;
	while (iterations++ < (1 << MAX_LINES_PER_SEGMENT_SHIFT))
	{
		// Calculate a segment [t_last, t'] with appropriate flatness
		// t' = (flatness / s2)^(1/2)
		// s2 = ((x - sx) * (cy - sy) - (y - sy) * (cx - sx)) / ((cx - sx)^2 + (cy - sy)^2)^1/2
		VEGA_FIX s2_d = VEGA_FIXSQR(cx - sx) + VEGA_FIXSQR(cy - sy);
		if (s2_d <= 0)
			break;

		VEGA_FIX s2_n = VEGA_FIXMUL(x - sx, cy - sy) - VEGA_FIXMUL(y - sy, cx - sx);
		VEGA_FIX s2 = VEGA_FIXDIV(s2_n, VEGA_FIXSQRT(s2_d));
		if (s2 < 0)
			s2 = -s2;

		if (s2 <= flatness)
			break;

		VEGA_FIX t_prim = VEGA_FIXSQRT(VEGA_FIXDIV(flatness, s2));

		// Subdivide at t_prim:
		//  left: [ P0, P0*(1-t') + P1*t', P0*(1-t')^2 + 2*P1*t'*(1-t') + P2*t'^2 ]
		// right: [ P0*(1-t')^2 + 2*P1*t'*(1-t') + P2*t'^2, P1*(1-t') + P2*t', P2 ]
		VEGA_FIX m01x = sx + VEGA_FIXMUL(cx - sx, t_prim);
		VEGA_FIX m01y = sy + VEGA_FIXMUL(cy - sy, t_prim);

		// New control point
		cx = cx + VEGA_FIXMUL(x - cx, t_prim);
		cy = cy + VEGA_FIXMUL(y - cy, t_prim);

		// New start point
		sx = m01x + VEGA_FIXMUL(cx - m01x, t_prim);
		sy = m01y + VEGA_FIXMUL(cy - m01y, t_prim);

		// Line to next start
		RETURN_IF_ERROR(lineTo(sx, sy));
	}

	return lineTo(x, y);
}

#else // VEGA_USE_PRECISE_ITERATIVE_FLATTENING

OP_STATUS VEGAPath::cubicBezierTo(VEGA_FIX x, VEGA_FIX y, VEGA_FIX c1x, VEGA_FIX c1y, VEGA_FIX c2x, VEGA_FIX c2y, VEGA_FIX flatness)
{
	m_bbox_valid = false;
#ifdef VEGA_3DDEVICE
	markTriangulationInfoInvalid();
	markSubPathInfoInvalid();
#endif // VEGA_3DDEVICE

	return cubicBezierToHelper(x, y, c1x, c1y, c2x, c2y, flatness, 0);
}

OP_STATUS VEGAPath::cubicBezierToHelper(VEGA_FIX x, VEGA_FIX y, VEGA_FIX c1x, VEGA_FIX c1y, VEGA_FIX c2x, VEGA_FIX c2y, VEGA_FIX flatness, int rec_count)
{
	// FIXME: there is a much better way to do this, but it requires a third-root // timj
	VEGA_FIX sx, sy;
	if (numLines == 0)
	{
		sx = lineData[0]->lines[0];
		sy = lineData[0]->lines[1];
	}
	else
	{
		VEGA_FIX* lastLine = VEGA_LINE_DATA(numLines-1);
		sx = lastLine[VEGALINE_ENDX];
		sy = lastLine[VEGALINE_ENDY];
	}

	VEGA_FIX ux, uy;
	VEGA_FIX vx, vy;
	ux = 3*c1x - 2*sx - x;
	ux = VEGA_ABS(ux / 4);
	uy = 3*c1y - 2*sy - y;
	uy = VEGA_ABS(uy / 4);
	vx = 3*c2x - 2*x - sx;
	vx = VEGA_ABS(vx / 4);
	vy = 3*c2y - 2*y - sy;
	vy = VEGA_ABS(vy / 4);

	if (ux < vx)
		ux = vx;
	if (uy < vy)
		uy = vy;

#if 0 // flatness hack
	if (lineWidth > VEGA_INTTOFIX(10))
	{
		// If the lineWidth is too big, increase the length
		// used to calculate flatness in case it is stroked.
		// The worst case is a full circle, which will
		// increase it's diameter by lineWidth*2 and thus
		// increase it's line length by
		// pi*d - pi*(d+2*lineWidth) = pi*2*lineWidth
		VEGA_FIX exp = VEGA_FIXMUL(lineWidth, VEGA_FIX_2PI);
		exp = VEGA_FIXSQR(exp);
		ux = VEGA_FIXMUL(ux, exp);
		uy = VEGA_FIXMUL(uy, exp);
	}
#endif

	if (ux+uy <= flatness || flatness < VEGA_EPSILON || rec_count == MAX_LINES_PER_SEGMENT_SHIFT)
	{
		RETURN_IF_ERROR(lineTo(x, y));
	}
	else
	{
		VEGA_FIX left_x, left_y, left_c1x, left_c1y, left_c2x, left_c2y;
		VEGA_FIX right_c1x, right_c1y, right_c2x, right_c2y;
		//subdivide to l and r;
		VEGA_FIX mx = VEGA_FIXDIV2(c1x+c2x);
		VEGA_FIX my = VEGA_FIXDIV2(c1y+c2y);
		left_c1x = VEGA_FIXDIV2(sx+c1x);
		left_c1y = VEGA_FIXDIV2(sy+c1y);
		right_c2x = VEGA_FIXDIV2(x+c2x);
		right_c2y = VEGA_FIXDIV2(y+c2y);
		left_c2x = VEGA_FIXDIV2(left_c1x+mx);
		left_c2y = VEGA_FIXDIV2(left_c1y+my);
		right_c1x = VEGA_FIXDIV2(right_c2x+mx);
		right_c1y = VEGA_FIXDIV2(right_c2y+my);
		left_x = VEGA_FIXDIV2(left_c2x+right_c1x);
		left_y = VEGA_FIXDIV2(left_c2y+right_c1y);
		if (rec_count > 2)
		{
			VEGA_FIX dx = left_x - VEGA_FIXDIV2(x + sx);
			VEGA_FIX dy = left_y - VEGA_FIXDIV2(y + sy);
			if (VEGA_ABS(dx) + VEGA_ABS(dy) < flatness)
			{
				return lineTo(x, y);
			}
		}
		RETURN_IF_ERROR(cubicBezierToHelper(left_x, left_y, left_c1x, left_c1y, left_c2x, left_c2y, flatness, rec_count+1));
		RETURN_IF_ERROR(cubicBezierToHelper(x, y, right_c1x, right_c1y, right_c2x, right_c2y, flatness, rec_count+1));
	}
	return OpStatus::OK;
}

OP_STATUS VEGAPath::quadraticBezierTo(VEGA_FIX x, VEGA_FIX y, VEGA_FIX cx, VEGA_FIX cy, VEGA_FIX flatness)
{
	m_bbox_valid = false;
#ifdef VEGA_3DDEVICE
	markTriangulationInfoInvalid();
	markSubPathInfoInvalid();
#endif // VEGA_3DDEVICE

	return quadraticBezierToHelper(x, y, cx, cy, flatness, 0);
}

OP_STATUS VEGAPath::quadraticBezierToHelper(VEGA_FIX x, VEGA_FIX y, VEGA_FIX cx, VEGA_FIX cy, VEGA_FIX flatness, int rec_count)
{
	VEGA_FIX sx, sy;
	if (numLines == 0)
	{
		sx = lineData[0]->lines[0];
		sy = lineData[0]->lines[1];
	}
	else
	{
		VEGA_FIX* lastLine = VEGA_LINE_DATA(numLines-1);
		sx = lastLine[VEGALINE_ENDX];
		sy = lastLine[VEGALINE_ENDY];
	}

	VEGA_FIX lcx = VEGA_FIXDIV2(sx+cx);
	VEGA_FIX lcy = VEGA_FIXDIV2(sy+cy);
	VEGA_FIX rcx = VEGA_FIXDIV2(x+cx);
	VEGA_FIX rcy = VEGA_FIXDIV2(y+cy);
	VEGA_FIX mx = VEGA_FIXDIV2(lcx+rcx);
	VEGA_FIX my = VEGA_FIXDIV2(lcy+rcy);

	if (rec_count == MAX_LINES_PER_SEGMENT_SHIFT ||
		VEGA_ABS(mx - VEGA_FIXDIV2(sx + x)) <= flatness &&
		VEGA_ABS(my - VEGA_FIXDIV2(sy + y)) <= flatness)
		return lineTo(x, y);

	RETURN_IF_ERROR(quadraticBezierToHelper(mx, my, lcx, lcy, flatness, rec_count+1));
	return quadraticBezierToHelper(x, y, rcx, rcy, flatness, rec_count+1);
}
#endif // VEGA_USE_PRECISE_ITERATIVE_FLATTENING

OP_STATUS VEGAPath::arcTo(VEGA_FIX x, VEGA_FIX y, VEGA_FIX rx, VEGA_FIX ry, VEGA_FIX xrot,
		bool large, bool sweep, VEGA_FIX flatness)
{
	m_bbox_valid = false;
#ifdef VEGA_3DDEVICE
	markTriangulationInfoInvalid();
	markSubPathInfoInvalid();
#endif // VEGA_3DDEVICE

	// Fix the radius
	VEGA_FIX rxsq, rysq;
	rxsq = VEGA_FIXSQR(rx);
	rysq = VEGA_FIXSQR(ry);
	if ( rxsq == 0 || rysq == 0 )
		return lineTo(x, y);
	if (rx < 0)
		rx = -rx;
	if (ry < 0)
		ry = -ry;

	VEGA_FIX sx, sy;
	if (numLines == 0)
	{
		sx = lineData[0]->lines[0];
		sy = lineData[0]->lines[1];
	}
	else
	{
		VEGA_FIX* lastLine = VEGA_LINE_DATA(numLines-1);
		sx = lastLine[VEGALINE_ENDX];
		sy = lastLine[VEGALINE_ENDY];
	}

	VEGA_FIX dx, dy;
	dx = sx-x;
	dy = sy-y;
	if (dx == 0 && dy == 0)
		return OpStatus::OK;
	dx = VEGA_FIXDIV2(dx);
	dy = VEGA_FIXDIV2(dy);

	//
	// Convert from arc on form:
	// {sx, sy, x, y, rx, ry, xrot, large, sweep}
	// to form
	// {cx, cy, rx, ry, startangle, deltaangle}
	//
	// Translate to midpoint of line segment (sx, sy) -> (x, y) and align coordinate axes:
	// [x'] = [cos(xrot)   sin(xrot)] * [(sx - x) / 2]
	// [y']   [-sin(xrot)  cos(xrot)]   [(sy - y) / 2]
	//
	// Solve for center of ellipse:
	// clen = sqrt([rx^2 * ry^2 - (rx^2 * y'^2  ry^2 * x'^2)] / [rx^2 * y'^2 + ry^2 * x'^2])
	// clen = {-clen iff large == sweep; clen iff large != sweep}
	// [cx'] = clen * [rx * y' / ry]
	// [cy']          [ry * x' / rx]
	//
	// Translate center back to original coordinate system:
	// [cx] = [cos(xrot) -sin(xrot)] * [cx'] + [(sx + x) / 2]
	// [cy]   [sin(xrot)  cos(xrot)]   [cy']   [(sy + y) / 2]
	//
	// Compute starting angle:
	// u = [(x' - cx') / rx]
	//     [(y' - cy') / ry]
	// startangle = arccos({[1; 0] dot u} / |u|)
	// startangle = {-startangle iff [1; 0] perp u < 0}
	//
	// Compute delta angle:
	// v = [(-x' - cx') / rx]
	//     [(-y' - cy') / ry]
	// deltaangle = arccos({u dot v} / {|u|*|v|}) mod 360
	// deltaangle = {-deltaangle iff u perp v < 0}
	//
	//              / deltaangle + 360 iff not sweep and deltaangle < 0
	// deltaangle = | deltaangle - 360 iff sweep and deltaangle > 0
	//              \ deltaangle
	//

	//
	// Radius check:
	// Lambda = x'^2 / rx^2 + y'^2 / ry^2
	//
	// Radius correction (iff Lambda > 1):
	// rx = sqrt(Lambda) * rx, ry = sqrt(Lambda) * ry
	//

	VEGA_FIX xprim;
	VEGA_FIX yprim;
	VEGA_FIX cosfi, sinfi;
	cosfi = VEGA_FIXCOS(xrot);
	sinfi = VEGA_FIXSIN(xrot);
	xprim = VEGA_FIXMUL(cosfi, dx) + VEGA_FIXMUL(sinfi, dy);
	yprim = VEGA_FIXMUL(cosfi, dy) - VEGA_FIXMUL(sinfi, dx);

	VEGA_FIX radcheck = VEGA_FIXMULDIV(xprim, xprim, rxsq);
	radcheck += VEGA_FIXMULDIV(yprim, yprim, rysq);
	if (radcheck > VEGA_INTTOFIX(1))
	{
		radcheck = VEGA_FIXSQRT(radcheck);
		rx = VEGA_FIXMUL(radcheck, rx);
		ry = VEGA_FIXMUL(radcheck, ry);
		rxsq = VEGA_FIXSQR(rx);
		rysq = VEGA_FIXSQR(ry);
	}

	// Now we know the radius is valid
	// Find the center of the ellipse
	VEGA_DBLFIX clen_dbl;
	clen_dbl = VEGA_FIXMUL_DBL(rxsq, VEGA_FIXSQR(yprim));
	clen_dbl = VEGA_DBLFIXADD(clen_dbl, VEGA_FIXMUL_DBL(rysq, VEGA_FIXSQR(xprim)));
	if (VEGA_DBLFIXSIGN(clen_dbl) == 0)
		return lineTo(x, y);
	// (rxsq * rysq - clen) / clen
	clen_dbl = VEGA_DBLFIXDIV(VEGA_DBLFIXSUB(VEGA_FIXMUL_DBL(rxsq, rysq), clen_dbl), clen_dbl);
	if (VEGA_DBLFIXLT(clen_dbl, VEGA_INTTODBLFIX(0)))
		clen_dbl = VEGA_DBLFIXNEG(clen_dbl);
		//return lineTo(x, y);
	clen_dbl = VEGA_DBLFIXSQRT(clen_dbl);
	if ( large == sweep )
		clen_dbl = VEGA_DBLFIXNEG(clen_dbl);

	VEGA_FIX clen = VEGA_DBLFIXTOFIX(clen_dbl);

	VEGA_FIX cxprim, cyprim;
	cxprim = VEGA_FIXMULDIV(rx, yprim, ry);
	cyprim = -VEGA_FIXMULDIV(ry, xprim, rx);
	cxprim = VEGA_FIXMUL(cxprim, clen);
	cyprim = VEGA_FIXMUL(cyprim, clen);

	VEGA_FIX cx, cy;
	cx = VEGA_FIXMUL(cosfi, cxprim);
	cx -= VEGA_FIXMUL(sinfi, cyprim);
	cy = VEGA_FIXMUL(sinfi, cxprim);
	cy += VEGA_FIXMUL(cosfi, cyprim);
	cx += VEGA_FIXDIV2(sx+x);
	cy += VEGA_FIXDIV2(sy+y);

	// Find start and stop angle
	VEGA_FIX vx, vy, vlen;
	// start angle
	vx = VEGA_FIXDIV(xprim-cxprim, rx);
	vy = VEGA_FIXDIV(yprim-cyprim, ry);
	vlen = VEGA_VECLENGTH(vx, vy);

	VEGA_FIX startangle;
	startangle = VEGA_FIXDIV(vx, vlen);
	if (startangle < VEGA_INTTOFIX(-1))
		startangle = VEGA_INTTOFIX(-1);
	if (startangle > VEGA_INTTOFIX(1))
		startangle = VEGA_INTTOFIX(1);
	startangle = VEGA_FIXACOS(startangle);
	if (vy < 0)
		startangle = -startangle;

	// delta angle
	VEGA_FIX ux, uy, ulen;
	ux = vx;
	uy = vy;
	ulen = vlen;

	vx = VEGA_FIXDIV(-xprim-cxprim, rx);
	vy = VEGA_FIXDIV(-yprim-cyprim, ry);

	vlen = VEGA_VECLENGTH(vx, vy);

	VEGA_FIX angle;
	angle = VEGA_FIXDIV(VEGA_FIXMUL(ux, vx)+VEGA_FIXMUL(uy, vy), VEGA_FIXMUL(ulen,vlen));
	if (angle < VEGA_INTTOFIX(-1))
		angle = VEGA_INTTOFIX(-1);
	if (angle > VEGA_INTTOFIX(1))
		angle = VEGA_INTTOFIX(1);
	angle = VEGA_FIXACOS(angle);
	if (VEGA_FIXMUL(ux, vy)-VEGA_FIXMUL(uy,vx) > 0)
		angle = -angle;
	if (!sweep && angle < 0)
	{
		angle += VEGA_INTTOFIX(360);
	}

	if (sweep && angle > 0)
	{
		angle -= VEGA_INTTOFIX(360);
	}

	// Some value used to make sure we don't get a div by zero
	while (angle > VEGA_INTTOFIX(1) ||angle < VEGA_INTTOFIX(-1))
	{
		VEGA_FIX curangle, currot;
		bool swap = false;
		VEGA_FIX x0, y0, x1, y1, x2, y2, x3, y3;
		if (angle > VEGA_INTTOFIX(90))
		{
			curangle = VEGA_INTTOFIX(90);
			angle -= VEGA_INTTOFIX(90);
			currot = startangle - VEGA_FIXDIV2(curangle);
			startangle -= VEGA_INTTOFIX(90);
		}
		else if (angle < VEGA_INTTOFIX(-90))
		{
			curangle = VEGA_INTTOFIX(90);
			angle += VEGA_INTTOFIX(90);
			currot = startangle + VEGA_FIXDIV2(curangle);
			swap = true;
			startangle += VEGA_INTTOFIX(90);
		}
		else if (angle > 0)
		{
			curangle = angle;
			angle = 0;
			currot = startangle - VEGA_FIXDIV2(curangle);
		}
		else
		{
			curangle = -angle;
			angle = 0;
			currot = startangle + VEGA_FIXDIV2(curangle);
			swap = true;
		}
		// This is the controll points of the bezier curve from curangle/2 to -curangle/2
		x0 = VEGA_FIXCOS(VEGA_FIXDIV2(curangle));
		y0 = VEGA_FIXSIN(VEGA_FIXDIV2(curangle));
		x1 = (VEGA_INTTOFIX(4)-x0)/3;
		y1 = VEGA_FIXMUL(VEGA_INTTOFIX(1)-x0, VEGA_INTTOFIX(3)-x0);
		y1 = VEGA_FIXDIV(y1, 3*y0);
		x2 = x1;
		y2 = -y1;
		x3 = x0;
		y3 = -y0;

		// rotate
		VEGATransform trans;
		trans.loadRotate(currot);
		trans.apply(x0, y0);
		trans.apply(x1, y1);
		trans.apply(x2, y2);
		trans.apply(x3, y3);
		// stretch
		x0 = VEGA_FIXMUL(x0, rx);
		y0 = VEGA_FIXMUL(y0, ry);
		x1 = VEGA_FIXMUL(x1, rx);
		y1 = VEGA_FIXMUL(y1, ry);
		x2 = VEGA_FIXMUL(x2, rx);
		y2 = VEGA_FIXMUL(y2, ry);
		x3 = VEGA_FIXMUL(x3, rx);
		y3 = VEGA_FIXMUL(y3, ry);
		// rotate
		trans.loadRotate(xrot);
		trans.apply(x0, y0);
		trans.apply(x1, y1);
		trans.apply(x2, y2);
		trans.apply(x3, y3);
		// translate
		x0 += cx;
		y0 += cy;
		x1 += cx;
		y1 += cy;
		x2 += cx;
		y2 += cy;
		x3 += cx;
		y3 += cy;
		if (swap)
		{
			VEGA_FIX tx = x0, ty = y0;
			x0 = x3;
			y0 = y3;
			x3 = tx;
			y3 = ty;
			tx = x1;
			ty = y1;
			x1 = x2;
			y1 = y2;
			x2 = tx;
			y2 = ty;
		}
		if (angle == 0)
		{
			x3 = x;
			y3 = y;
		}
		// bezierTo
		RETURN_IF_ERROR(cubicBezierTo(x3, y3, x1, y1, x2, y2, flatness));
	}
	// lineTo does nothing if the last point is same as the point to line to, so the check is redundant
	return lineTo(x, y);
}

OP_STATUS VEGAPath::close(bool visible)
{
	if (numLines == lastSubPathStart)
		return OpStatus::OK;
	VEGA_FIX* lastLine = VEGA_LINE_DATA(numLines-1);
	VEGA_FIX* firstLine = getLine(lastSubPathStart);
	if (firstLine[VEGALINE_STARTX] == lastLine[VEGALINE_ENDX] &&
		firstLine[VEGALINE_STARTY] == lastLine[VEGALINE_ENDY])
	{
		// If close(false) is run and the current position is the same
		// as the start position we need an invisible line of length 0
		// to make sure the linecaps are not connected for this close.
		// This happens in the polyline testcase from w3c.
		if (!visible && /*!(lineAttributes[numLines-1] & VEGA_LINE_ATTR_CLOSE)*/!(VEGA_LINE_ATTRIBUTE(numLines-1)&VEGA_LINE_ATTR_CLOSE))
		{
			// FIXME: this is a pretty ugly hack to make sure a line
			// doesn't have a visible close when it should not
			RETURN_IF_ERROR(lineTo(firstLine[VEGALINE_STARTX]+VEGA_INTTOFIX(1), firstLine[VEGALINE_STARTY]+VEGA_INTTOFIX(1)));
			lastLine = VEGA_LINE_DATA(numLines-1);
			lastLine[VEGALINE_ENDX] = firstLine[VEGALINE_STARTX];
			lastLine[VEGALINE_ENDY] = firstLine[VEGALINE_STARTY];
			VEGA_LINE_ATTRIBUTE(numLines-1) |= VEGA_LINE_ATTR_NOT_VISIBLE;
		}
		VEGA_LINE_ATTRIBUTE(numLines-1) |= VEGA_LINE_ATTR_CLOSE;
		return OpStatus::OK;
	}
	RETURN_IF_ERROR(lineTo(firstLine[VEGALINE_STARTX], firstLine[VEGALINE_STARTY]));
	if (!visible)
		VEGA_LINE_ATTRIBUTE(numLines-1) |= VEGA_LINE_ATTR_NOT_VISIBLE;
	VEGA_LINE_ATTRIBUTE(numLines-1) |= VEGA_LINE_ATTR_CLOSE;
	return OpStatus::OK;
}

bool VEGAPath::isClosed() const
{
	if (numLines == lastSubPathStart)
		return true;
	VEGA_FIX* firstLine = getLine(lastSubPathStart);
	VEGA_FIX* lastLine = VEGA_LINE_DATA(numLines-1);
	return firstLine[VEGALINE_STARTX] == lastLine[VEGALINE_ENDX] && firstLine[VEGALINE_STARTY] == lastLine[VEGALINE_ENDY];
}

bool VEGAPath::removeLastLine()
{
	m_bbox_valid = false;
#ifdef VEGA_3DDEVICE
	markSubPathInfoInvalid();
#endif // VEGA_3DDEVICE

	if (numLines > lastSubPathStart)
	{
		--numLines;
		return true;
	}
	return false;
}

OP_STATUS VEGAPath::appendPrimitive(VEGAPrimitive* prim)
{
	VEGA_FIX vf_flat = prim->flatness;

	if (!allocatedLineBlocks)
		RETURN_IF_ERROR(increaseLineAllocation());

	switch (prim->type)
	{
	case VEGAPrimitive::RECTANGLE:
	{
		// Simple rect
		VEGA_FIX x = prim->data.rect.x;
		VEGA_FIX y = prim->data.rect.y;
		VEGA_FIX w = prim->data.rect.width;
		VEGA_FIX h = prim->data.rect.height;

		RETURN_IF_ERROR(moveTo(x, y));
		RETURN_IF_ERROR(lineTo(x + w, y));
		RETURN_IF_ERROR(lineTo(x + w, y + h));
		RETURN_IF_ERROR(lineTo(x, y + h));
		RETURN_IF_ERROR(close(true));
	}
	break;

	case VEGAPrimitive::ROUNDED_RECT_UNIFORM:
	{
		// Rounded rect
		VEGA_FIX x = prim->data.rrect.x;
		VEGA_FIX y = prim->data.rrect.y;
		VEGA_FIX w = prim->data.rrect.width;
		VEGA_FIX h = prim->data.rrect.height;
		VEGA_FIX rx = prim->data.rrect.corners[0];
		VEGA_FIX ry = prim->data.rrect.corners[1];

		// Quadratic bezier - 2 per corner
		// P00 = (rx, 0)
		// P01 = (rx, ry * [2*cos(pi/2) - 1])
		// P02 = (rx * cos(pi/2), ry * sin(pi/2))
		// P10 = P02
		// P11 = (rx * [2*cos(pi/2) - 1], ry)
		// P12 = (0, ry)

		RETURN_IF_ERROR(moveTo(x + rx, y));

		VEGA_FIX curr_x = x + w - rx;
		VEGA_FIX curr_y = y;

		RETURN_IF_ERROR(lineTo(curr_x, curr_y));

		VEGA_FIX cospi2 = VEGA_FIXCOS(VEGA_INTTOFIX(45));
		VEGA_FIX rxf1 = VEGA_FIXMUL(rx, cospi2);
		VEGA_FIX rxf2 = VEGA_FIXMUL(rx, 2 * cospi2 - VEGA_INTTOFIX(1));
		VEGA_FIX ryf1 = VEGA_FIXMUL(ry, cospi2);
		VEGA_FIX ryf2 = VEGA_FIXMUL(ry, 2 * cospi2 - VEGA_INTTOFIX(1));

		// Top-Right corner ([x + w - rx, y] -> [x + w, y + ry])
		// -> [rxf2, 0] -> [rxf1, ry - ryf1] -> [rx, ry - ryf2] -> [rx, ry]
		RETURN_IF_ERROR(appendCorner(curr_x + rxf2, curr_y,
									 curr_x + rxf1, curr_y + ry - ryf1,
									 curr_x + rx, curr_y + ry - ryf2,
									 curr_x + rx, curr_y + ry,
									 vf_flat));

		curr_x = x + w;
		curr_y = y + h - ry;

		RETURN_IF_ERROR(lineTo(curr_x, curr_y));

		// Bottom-Right corner ([x + w, y + h - ry] -> [x + w - rx, y + h])
		// -> [0, ryf2] -> [-rx + rxf1, ryf1] -> [-rx + rxf2, ry] -> [-rx, ry]
		RETURN_IF_ERROR(appendCorner(curr_x, curr_y + ryf2,
									 curr_x - rx + rxf1, curr_y + ryf1,
									 curr_x - rx + rxf2, curr_y + ry,
									 curr_x - rx, curr_y + ry,
									 vf_flat));

		curr_x = x + rx;
		curr_y = y + h;

		RETURN_IF_ERROR(lineTo(curr_x, curr_y));

		// Bottom-Left corner ([x + rx, y + h] -> [x, y + h - ry])
		// -> [-rxf2, 0] -> [-rxf1, -ry + ryf1] -> [-rx, -ry + ryf2] -> [-rx, -ry]
		RETURN_IF_ERROR(appendCorner(curr_x - rxf2, curr_y,
									 curr_x - rxf1, curr_y - ry + ryf1,
									 curr_x - rx, curr_y - ry + ryf2,
									 curr_x - rx, curr_y - ry,
									 vf_flat));

		curr_x = x;
		curr_y = y + ry;

		RETURN_IF_ERROR(lineTo(curr_x, curr_y));

		// Top-Left corner ([x, y + ry] -> [x + rx, y])
		// -> [0, -ryf2] -> [rx - rxf1, -ryf1] -> [rx - rxf2, -ry] -> [rx, -ry]
		RETURN_IF_ERROR(appendCorner(curr_x, curr_y - ryf2,
									 curr_x + rx - rxf1, curr_y - ryf1,
									 curr_x + rx - rxf2, curr_y - ry,
									 curr_x + rx, curr_y - ry,
									 vf_flat));

		RETURN_IF_ERROR(close(true));
	}
	break;

	default:
		OP_ASSERT(!"Unknown primitive");
	}

	return OpStatus::OK;
}

VEGA_FIX *VEGAPath::getLine(unsigned int lnum) const
{
#ifdef VEGA_LINE_ALLOCATION_SIZE_SHIFT
	unsigned int block = lnum>>VEGA_LINE_ALLOCATION_SIZE_SHIFT;
	unsigned int line = lnum&VEGA_LINE_ALLOCATION_SIZE_MASK;
#else
	unsigned int block = lnum/VEGA_LINE_ALLOCATION_SIZE;
	unsigned int line = lnum%VEGA_LINE_ALLOCATION_SIZE;
#endif
	if (line == 0 && block)
	{
		lineData[block]->lines[0] = lineData[block-1]->lines[VEGA_LINE_ALLOCATION_SIZE*2];
		lineData[block]->lines[1] = lineData[block-1]->lines[VEGA_LINE_ALLOCATION_SIZE*2+1];
	}
	return lineData[block]->lines+2*line;
}

VEGA_FIX* VEGAPath::getNonWarpLine(unsigned int lnum) const
{
	if (VEGA_LINE_ATTRIBUTE(lnum) & VEGA_LINE_ATTR_WARP)
		return NULL;
#ifdef VEGA_LINE_ALLOCATION_SIZE_SHIFT
	unsigned int block = lnum>>VEGA_LINE_ALLOCATION_SIZE_SHIFT;
	unsigned int line = lnum&VEGA_LINE_ALLOCATION_SIZE_MASK;
#else
	unsigned int block = lnum/VEGA_LINE_ALLOCATION_SIZE;
	unsigned int line = lnum%VEGA_LINE_ALLOCATION_SIZE;
#endif
	if (line == 0 && block)
	{
		lineData[block]->lines[0] = lineData[block-1]->lines[VEGA_LINE_ALLOCATION_SIZE*2];
		lineData[block]->lines[1] = lineData[block-1]->lines[VEGA_LINE_ALLOCATION_SIZE*2+1];
	}
	return lineData[block]->lines+2*line;
}

VEGA_FIX VEGAPath::getLineLength(unsigned int lnum)
{
#ifdef VEGA_LINE_ALLOCATION_SIZE_SHIFT
	unsigned int block = lnum>>VEGA_LINE_ALLOCATION_SIZE_SHIFT;
	unsigned int line = lnum&VEGA_LINE_ALLOCATION_SIZE_MASK;
#else
	unsigned int block = lnum/VEGA_LINE_ALLOCATION_SIZE;
	unsigned int line = lnum%VEGA_LINE_ALLOCATION_SIZE;
#endif
	if (lineData[block]->numCachedLineLens <= line)
	{
		if (block && lineData[block]->numCachedLineLens == 0)
		{
			lineData[block]->lines[0] = lineData[block-1]->lines[VEGA_LINE_ALLOCATION_SIZE*2];
			lineData[block]->lines[1] = lineData[block-1]->lines[VEGA_LINE_ALLOCATION_SIZE*2+1];
		}
		for (unsigned int i = lineData[block]->numCachedLineLens; i <= line; ++i)
		{
			VEGA_FIX* l = lineData[block]->lines+(i<<1);
			lineData[block]->linelens[i] = VEGA_VECLENGTH(l[VEGALINE_ENDX] - l[VEGALINE_STARTX],
														  l[VEGALINE_ENDY] - l[VEGALINE_STARTY]);
		}
		lineData[block]->numCachedLineLens = line+1;
	}
	return lineData[block]->linelens[line];
}

bool VEGAPath::getScaledLineNormal(VEGA_FIX* lineData, VEGA_FIX lineLength, VEGA_FIX dblnormlen, VEGA_FIX &nx, VEGA_FIX &ny)
{
	VEGA_FIX len;
	nx = lineData[VEGALINE_ENDY] - lineData[VEGALINE_STARTY];
	ny = lineData[VEGALINE_STARTX] - lineData[VEGALINE_ENDX];
	len = VEGA_FIXMUL2(lineLength);
	if (len == 0)
	{
		return false;
	}
	nx = VEGA_FIXMULDIV(nx, dblnormlen, len);
	ny = VEGA_FIXMULDIV(ny, dblnormlen, len);
	return true;
}
bool VEGAPath::getScaledLineDirection(VEGA_FIX* lineData, VEGA_FIX lineLength, VEGA_FIX dbllen, VEGA_FIX &nx, VEGA_FIX &ny)
{
	VEGA_FIX len;
	nx = lineData[VEGALINE_ENDX] - lineData[VEGALINE_STARTX];
	ny = lineData[VEGALINE_ENDY] - lineData[VEGALINE_STARTY];
	len = VEGA_FIXMUL2(lineLength);
	if (len == 0)
	{
		return false;
	}
	nx = VEGA_FIXMULDIV(nx, dbllen, len);
	ny = VEGA_FIXMULDIV(ny, dbllen, len);
	return true;
}

void VEGAPath::transform(VEGATransform *trans)
{
	unsigned int i;
	if (!started)
		return;

	m_bbox_valid = false;
	unsigned int remLines = numLines;
	for (i = 0; i < allocatedLineBlocks && remLines; ++i)
	{
		// initial start line
		lineData[i]->numCachedLineLens = 0;
		trans->apply(lineData[i]->lines[0], lineData[i]->lines[1]);
		for (unsigned int j = 0; j < VEGA_LINE_ALLOCATION_SIZE && remLines; ++j)
		{
			trans->apply(lineData[i]->lines[j*2+2], lineData[i]->lines[j*2+3]);
			--remLines;
		}
	}
#if defined(VEGA_3DDEVICE)
	markSubPathInfoInvalid();
#endif // VEGA_3DDEVICE
}

#ifdef VEGA_USE_LINEDATA_POOL
/* static */
VEGAPath::LineData* VEGAPath::allocLineData()
{
	if (LineData* linedata = reinterpret_cast<LineData*>(g_vegaGlobals.linedata_pool))
	{
		g_vegaGlobals.linedata_pool = NULL;
		return linedata;
	}
	else
	{
		return OP_NEW(LineData, ());
	}
}

/* static */
void VEGAPath::freeLineData(LineData* linedata)
{
	if (g_vegaGlobals.linedata_pool == NULL)
		g_vegaGlobals.linedata_pool = reinterpret_cast<void*>(linedata);
	else
		OP_DELETE(linedata);
}

/* static */
void VEGAPath::clearPool()
{
	LineData* linedata = reinterpret_cast<LineData*>(g_vegaGlobals.linedata_pool);
	OP_DELETE(linedata);
}
#endif // VEGA_USE_LINEDATA_POOL

OP_STATUS VEGAPath::increaseLineAllocation()
{
	LineData** nld = OP_NEWA(LineData*, allocatedLineBlocks+1);
	if (!nld)
		return OpStatus::ERR_NO_MEMORY;
	nld[allocatedLineBlocks] = allocLineData();
	if (!nld[allocatedLineBlocks])
	{
		OP_DELETEA(nld);
		return OpStatus::ERR_NO_MEMORY;
	}
	for (unsigned int i = 0; i < allocatedLineBlocks; ++i)
	{
		nld[i] = lineData[i];
	}
	if (allocatedLineBlocks)
	{
		nld[allocatedLineBlocks]->lines[0] = nld[allocatedLineBlocks-1]->lines[VEGA_LINE_ALLOCATION_SIZE*2];
		nld[allocatedLineBlocks]->lines[1] = nld[allocatedLineBlocks-1]->lines[VEGA_LINE_ALLOCATION_SIZE*2+1];
	}
	nld[allocatedLineBlocks]->numCachedLineLens = 0;
	OP_DELETEA(lineData);
	lineData = nld;
	++allocatedLineBlocks;
	return OpStatus::OK;
}

bool VEGAPath::isLineVisible(unsigned int lnum) const
{
	return (VEGA_LINE_ATTRIBUTE(lnum) & VEGA_LINE_ATTR_NOT_VISIBLE)==0;
}

bool VEGAPath::isLineWarp(unsigned int lnum) const
{
	return (VEGA_LINE_ATTRIBUTE(lnum) & VEGA_LINE_ATTR_WARP)!=0;
}

bool VEGAPath::isLineClose(unsigned int lnum) const
{
	return (VEGA_LINE_ATTRIBUTE(lnum) & VEGA_LINE_ATTR_CLOSE)!=0;
}

void VEGAPath::getBoundingBox(VEGA_FIX& x, VEGA_FIX& y, VEGA_FIX& w, VEGA_FIX& h)
{
	if (m_bbox_valid)
	{
		x = m_bbox_x;
		y = m_bbox_y;
		w = m_bbox_w;
		h = m_bbox_h;
		return;
	}

	x = VEGA_INFINITY;
	y = VEGA_INFINITY;
	VEGA_FIX x2 = -VEGA_INFINITY;
	VEGA_FIX y2 = -VEGA_INFINITY;

	for (unsigned int cl = 0; cl < numLines; ++cl)
	{
		if (!(VEGA_LINE_ATTRIBUTE(cl) & VEGA_LINE_ATTR_WARP))
		{
			const VEGA_FIX* clline = getLine(cl);
			if (clline[VEGALINE_STARTX] < clline[VEGALINE_ENDX])
			{
				x = MIN(x, clline[VEGALINE_STARTX]);
				x2 = MAX(x2, clline[VEGALINE_ENDX]);
			}
			else
			{
				x = MIN(x, clline[VEGALINE_ENDX]);
				x2 = MAX(x2, clline[VEGALINE_STARTX]);
			}

			if (clline[VEGALINE_STARTY] < clline[VEGALINE_ENDY])
			{
				y = MIN(y, clline[VEGALINE_STARTY]);
				y2 = MAX(y2, clline[VEGALINE_ENDY]);
			}
			else
			{
				y = MIN(y, clline[VEGALINE_ENDY]);
				y2 = MAX(y2, clline[VEGALINE_STARTY]);
			}
		}
	}

	w = x2 - x;
	h = y2 - y;

	m_bbox_valid = true;
	m_bbox_x = x;
	m_bbox_y = y;
	m_bbox_w = w;
	m_bbox_h = h;
}

// static
bool VEGAPath::line_intersection(VEGA_FIX x1, VEGA_FIX y1, VEGA_FIX x2, VEGA_FIX y2,
		VEGA_FIX x3, VEGA_FIX y3, VEGA_FIX x4, VEGA_FIX y4,
		VEGA_FIX &x, VEGA_FIX &y, bool infinite)
{
	// if line 1 is from v1 to v2 and line 2 is from v3 to v4, we have
	// p1 = v1 + t1*(v2 - v1)
	// p2 = v3 + t2*(v4 - v3)

	// when the lines collide we must have p1 == p2, hence:
	// x1 + t1*(x2-x1) == x3 + t2*(x4-x3)
	// and:
	// y1 + t1*(y2-y1) == y3 + t2*(y4-y3)

	// solving this equation will give:
	// t1 = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / ((y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1))
	// t2 = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / ((y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1))

	// if the line segments are connected they can not cross eachother
	if (x2 == x3 && y2 == y3)
		return false;
	if (x1 == x4 && y1 == y4)
		return false;
	if (x1 == x3 && y1 == y3)
		return false;
	if (x2 == x4 && y2 == y4)
		return false;

	VEGA_DBLFIX div = VEGA_DBLFIXSUB(VEGA_FIXMUL_DBL(y4 - y3, x2 - x1),
									 VEGA_FIXMUL_DBL(x4 - x3, y2 - y1));

	if (VEGA_DBLFIXSIGN(div) == 0) // if div is 0, line segments are parallell
		return false;

	// if both t1 and t2 is >=0 and <=1, the lines intersect
	VEGA_DBLFIX t1 = VEGA_DBLFIXSUB(VEGA_FIXMUL_DBL(x4 - x3, y1 - y3),
									VEGA_FIXMUL_DBL(y4 - y3, x1 - x3));
	VEGA_DBLFIX t2 = VEGA_DBLFIXSUB(VEGA_FIXMUL_DBL(x2 - x1, y1 - y3),
									VEGA_FIXMUL_DBL(y2 - y1, x1 - x3));
	if (!infinite)
	{
		if (VEGA_DBLFIXSIGN(div) < 0)
		{
			t1 = VEGA_DBLFIXNEG(t1);
			t2 = VEGA_DBLFIXNEG(t2);
			div = VEGA_DBLFIXNEG(div);
		}
		if (VEGA_DBLFIXLT(t1, VEGA_INTTODBLFIX(0)) || VEGA_DBLFIXGT(t1, div) ||
			VEGA_DBLFIXLT(t2, VEGA_INTTODBLFIX(0)) || VEGA_DBLFIXGT(t2, div))
			return false;
	}
	VEGA_DBLFIX t1_h = VEGA_DBLFIXDIV(t1, div);

	// the lines intersect..
	x = x1 + VEGA_FIXMUL(VEGA_DBLFIXTOFIX(t1_h), x2-x1);
	y = y1 + VEGA_FIXMUL(VEGA_DBLFIXTOFIX(t1_h), y2-y1);
	return true;
}

// these are divided in two since it is possible that neither inside, not outside will need a join
static inline bool needs_join_on_outside(VEGA_FIX* line, VEGA_FIX* prevline)
{
	VEGA_FIX nx, ny;
	nx = line[VEGALINE_ENDY] - line[VEGALINE_STARTY];
	ny = line[VEGALINE_STARTX] - line[VEGALINE_ENDX];
	return VEGA_VECDOTSIGN(nx, ny, prevline[VEGALINE_STARTX] - line[VEGALINE_STARTX],
						   prevline[VEGALINE_STARTY] - line[VEGALINE_STARTY]) <= 0;
}
static inline bool needs_join_on_inside(VEGA_FIX* line, VEGA_FIX* prevline)
{
	VEGA_FIX nx, ny;
	nx = line[VEGALINE_ENDY] - line[VEGALINE_STARTY];
	ny = line[VEGALINE_STARTX] - line[VEGALINE_ENDX];
	return VEGA_VECDOTSIGN(nx, ny, prevline[VEGALINE_ENDX] - line[VEGALINE_ENDX],
						   prevline[VEGALINE_ENDY] - line[VEGALINE_ENDY]) > 0;
}

OP_STATUS VEGAPath::do_miter_join(VEGAPath *outline, VEGA_FIX* line, VEGA_FIX* nextline,
									  VEGA_FIX dx, VEGA_FIX dy, VEGA_FIX ndx, VEGA_FIX ndy)
{
	// render join
	VEGA_FIX x, y;
	if (line_intersection(line[VEGALINE_STARTX]+dx,
						line[VEGALINE_STARTY]+dy,
						line[VEGALINE_ENDX]+dx,
						line[VEGALINE_ENDY]+dy,
						nextline[VEGALINE_STARTX]+ndx,
						nextline[VEGALINE_STARTY]+ndy,
						nextline[VEGALINE_ENDX]+ndx,
						nextline[VEGALINE_ENDY]+ndy,
						x, y, true))
	{
		// 4 * (ldx^2 + ldy^2) / lineWidth^2 <= miterLimit^2
		VEGA_FIX ldx = x - line[VEGALINE_ENDX];
		VEGA_FIX ldy = y - line[VEGALINE_ENDY];
		VEGA_DBLFIX len = VEGA_DBLFIXADD(VEGA_FIXMUL_DBL(ldx, ldx),
										 VEGA_FIXMUL_DBL(ldy, ldy));
		VEGA_FIX mllw = VEGA_FIXMUL(miterLimit, lineWidth) / 2;
		if (VEGA_DBLFIXLTEQ(len, VEGA_FIXMUL_DBL(mllw, mllw)))
		{
			return outline->lineTo(x, y);
		}
	}
	return OpStatus::ERR;
}

OP_STATUS VEGAPath::createOutline(VEGAPath *outline, VEGA_FIX flatness, int firstLine/* = 0*/)
{
	VEGA_FIX dx, dy;

	if ((int)getNumLines() <= firstLine)
		return OpStatus::ERR;

	if (getNumLines() > 1)
		RETURN_IF_ERROR(close(false));

	if (firstLine == 0)
	{
		// Will not reallocate since number of lines >= 0
		RETURN_IF_ERROR(outline->prepare(getNumLines()*2));
		outline->lineJoin = lineJoin;
		outline->lineCap = lineCap;
		outline->miterLimit = miterLimit;
		outline->lineWidth = lineWidth;
	}

	const VEGA_FIX lineWidth = outline->lineWidth;

	while ((firstLine < (int)getNumLines()))
	{
		while (firstLine < (int)getNumLines() && (VEGA_LINE_ATTRIBUTE(firstLine) & VEGA_LINE_ATTR_WARP))
			++firstLine;
		if ((int)getNumLines() <= firstLine)
			return OpStatus::OK;

		// determine the last line in the current subpath
		int lastLine = getNumLines();
		if (hasMultipleSubPaths())
		{
			lastLine = firstLine;
			while (lastLine < (int)getNumLines() && !(VEGA_LINE_ATTRIBUTE(lastLine) & VEGA_LINE_ATTR_WARP))
				++lastLine;
		}

		VEGA_FIX* firstLineData = getLine(firstLine);
		VEGA_FIX firstLineLength = getLineLength(firstLine);
		// handle the case of one line here so I don't have to care about it later
		if (lastLine == firstLine+1)
		{
			VEGA_FIX dirx, diry;
			dirx = 0;
			diry = 0;

			if (VEGA_LINE_ATTRIBUTE(firstLine) & VEGA_LINE_ATTR_NOT_VISIBLE)
			{
				firstLine = lastLine+1;
				continue;
			}

			if (!getScaledLineNormal(firstLineData, firstLineLength, lineWidth, dx, dy))
				return OpStatus::OK;
			if (lineCap == VEGA_LINECAP_SQUARE)
				getScaledLineDirection(firstLineData, firstLineLength, lineWidth, dirx, diry);

			RETURN_IF_ERROR(outline->moveTo(firstLineData[VEGALINE_STARTX]-dirx+dx, firstLineData[VEGALINE_STARTY]-diry+dy));
			RETURN_IF_ERROR(outline->lineTo(firstLineData[VEGALINE_ENDX]+dirx+dx, firstLineData[VEGALINE_ENDY]+diry+dy));
			if (lineCap == VEGA_LINECAP_ROUND)
			{
				RETURN_IF_ERROR(outline->arcTo(firstLineData[VEGALINE_ENDX]-dx,
								firstLineData[VEGALINE_ENDY]-dy,
								VEGA_FIXDIV2(lineWidth), VEGA_FIXDIV2(lineWidth), 0, true, true, flatness));
			}
			else
			{
				RETURN_IF_ERROR(outline->lineTo(firstLineData[VEGALINE_ENDX]+dirx-dx, firstLineData[VEGALINE_ENDY]+diry-dy));
			}
			RETURN_IF_ERROR(outline->lineTo(firstLineData[VEGALINE_STARTX]-dirx-dx, firstLineData[VEGALINE_STARTY]-diry-dy));
			if (lineCap == VEGA_LINECAP_ROUND)
			{
				RETURN_IF_ERROR(outline->arcTo(firstLineData[VEGALINE_STARTX]+dx,
								firstLineData[VEGALINE_STARTY]+dy,
								VEGA_FIXDIV2(lineWidth), VEGA_FIXDIV2(lineWidth), 0, true, true, flatness));
			}
			else
			{
				RETURN_IF_ERROR(outline->close(true));
			}
		}
		else
		{

			int lnum;


			// we can't start with a warp
			dx = 0;
			dy = 0;
			// don't care if this fails (but it shouold not happen
			getScaledLineNormal(firstLineData, firstLineLength, lineWidth, dx, dy);
			RETURN_IF_ERROR(outline->moveTo(firstLineData[VEGALINE_STARTX]+dx, firstLineData[VEGALINE_STARTY]+dy));

			unsigned int internal_start_line;
			internal_start_line = outline->getNumLines();
			// strictly less since last-line is a warp
			for ( lnum = firstLine; lnum < lastLine; ++lnum ){
				if (VEGA_LINE_ATTRIBUTE(lnum) & VEGA_LINE_ATTR_NOT_VISIBLE)
					continue;

				VEGA_FIX* lineData = getLine(lnum);
				VEGA_FIX lineLength = getLineLength(lnum);
				dx = 0;
				dy = 0;
				getScaledLineNormal(lineData, lineLength, lineWidth, dx, dy);

				int prevnum = lnum-1;
				if (prevnum < firstLine)
					prevnum = lastLine-1;
				int nextnum = lnum+1;
				if (nextnum >= lastLine)
					nextnum = firstLine;

				if (VEGA_LINE_ATTRIBUTE(prevnum) & VEGA_LINE_ATTR_NOT_VISIBLE)
				{
					// if the previous was not visible, just warp to the start point.
					// otherwise we are already at the startpoint
					RETURN_IF_ERROR(outline->warpTo(lineData[VEGALINE_STARTX]+dx, lineData[VEGALINE_STARTY]+dy));
				}
				// Now the current position is exaclty where the line should start
				if (!(VEGA_LINE_ATTRIBUTE(nextnum) & VEGA_LINE_ATTR_NOT_VISIBLE))
				{
					VEGA_FIX* nextLineData = getLine(nextnum);
					VEGA_FIX nextLineLength = getLineLength(nextnum);
					// if the next line is visible we have two cases, it neds a join or not :)
					if (needs_join_on_outside(nextLineData, lineData))
					{
						VEGA_FIX ndx = 0, ndy = 0;
						getScaledLineNormal(nextLineData, nextLineLength, lineWidth, ndx, ndy);
						// move to the end of the line, then render the join
						if (lineJoin == VEGA_LINEJOIN_MITER)
						{
							OP_STATUS ret_val = do_miter_join(outline, lineData, nextLineData, dx, dy, ndx, ndy);
							if (ret_val == OpStatus::ERR)
							{
								RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_ENDX]+dx, lineData[VEGALINE_ENDY]+dy));
								RETURN_IF_ERROR(outline->lineTo(nextLineData[VEGALINE_STARTX]+ndx,
												nextLineData[VEGALINE_STARTY]+ndy));
							}
							else if (OpStatus::IsError(ret_val))
							{
								return ret_val;
							}
						}
						else if (lineJoin == VEGA_LINEJOIN_ROUND)
						{
							RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_ENDX]+dx, lineData[VEGALINE_ENDY]+dy));

							RETURN_IF_ERROR(outline->arcTo(nextLineData[VEGALINE_STARTX]+ndx,
											nextLineData[VEGALINE_STARTY]+ndy,
											VEGA_FIXDIV2(lineWidth), VEGA_FIXDIV2(lineWidth), 0, false, true, flatness));
						}
						else
						{
							RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_ENDX]+dx, lineData[VEGALINE_ENDY]+dy));
							RETURN_IF_ERROR(outline->lineTo(nextLineData[VEGALINE_STARTX]+ndx,
											nextLineData[VEGALINE_STARTY]+ndy));
						}
					}
					else
					{
						VEGA_FIX ndx = 0, ndy = 0;
						VEGA_FIX intx, inty;
						getScaledLineNormal(nextLineData, nextLineLength, lineWidth, ndx, ndy);
						if (line_intersection(lineData[VEGALINE_STARTX]+dx,
											lineData[VEGALINE_STARTY]+dy,
											lineData[VEGALINE_ENDX]+dx,
											lineData[VEGALINE_ENDY]+dy,
											nextLineData[VEGALINE_STARTX]+ndx,
											nextLineData[VEGALINE_STARTY]+ndy,
											nextLineData[VEGALINE_ENDX]+ndx,
											nextLineData[VEGALINE_ENDY]+ndy,
											intx, inty, false))
						{
							// if the lines intersect, move to the intersection point
							RETURN_IF_ERROR(outline->lineTo(intx, inty));
						}
						else if (VEGA_ABS(VEGA_FIXMUL(dx, nextLineData[VEGALINE_ENDX] -
													  nextLineData[VEGALINE_STARTX]) +
										  VEGA_FIXMUL(dy, nextLineData[VEGALINE_ENDY] -
													  nextLineData[VEGALINE_STARTY])) <= 100*VEGA_EPSILON)
						{
							// Co-linear
							RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_ENDX]+dx, lineData[VEGALINE_ENDY]+dy));
							RETURN_IF_ERROR(outline->lineTo(nextLineData[VEGALINE_STARTX]+ndx, nextLineData[VEGALINE_STARTY]+ndy));
						}
						else
						{
							// else move to the endpoint of this line, or the startpoint of the next
							// depending on which line is longer
							if (line_intersection(lineData[VEGALINE_STARTX]+dx,
											lineData[VEGALINE_STARTY]+dy,
											lineData[VEGALINE_STARTX]-dx,
											lineData[VEGALINE_STARTY]-dy,
											nextLineData[VEGALINE_STARTX]+ndx,
											nextLineData[VEGALINE_STARTY]+ndy,
											nextLineData[VEGALINE_ENDX]+ndx,
											nextLineData[VEGALINE_ENDY]+ndy,
											intx, inty, false))
							{
								RETURN_IF_ERROR(outline->lineTo(intx, inty));
								RETURN_IF_ERROR(outline->lineTo(nextLineData[VEGALINE_STARTX]+ndx, nextLineData[VEGALINE_STARTY]+ndy));
							}
							else if (line_intersection(lineData[VEGALINE_STARTX]+dx,
											lineData[VEGALINE_STARTY]+dy,
											lineData[VEGALINE_ENDX]+dx,
											lineData[VEGALINE_ENDY]+dy,
											nextLineData[VEGALINE_ENDX]+ndx,
											nextLineData[VEGALINE_ENDY]+ndy,
											nextLineData[VEGALINE_ENDX]-ndx,
											nextLineData[VEGALINE_ENDY]-ndy,
											intx, inty, false))
							{
								RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_ENDX]+dx, lineData[VEGALINE_ENDY]+dy));
								RETURN_IF_ERROR(outline->lineTo(intx, inty));
							}
							else
							{
								RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_ENDX]+dx, lineData[VEGALINE_ENDY]+dy));
								RETURN_IF_ERROR(outline->lineTo(nextLineData[VEGALINE_STARTX]+ndx, nextLineData[VEGALINE_STARTY]+ndy));
							}
						}
					}
				}
				else
				{
					// next line is not visible so we will need a cap
					if (lineCap == VEGA_LINECAP_SQUARE)
					{
						VEGA_FIX dirx = 0, diry = 0;
						getScaledLineDirection(lineData, lineLength, lineWidth, dirx, diry);
						RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_ENDX]+dirx+dx, lineData[VEGALINE_ENDY]+diry+dy));
						RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_ENDX]+dirx-dx, lineData[VEGALINE_ENDY]+diry-dy));
						// FIXME: this small linesegment can be merged with the line back instead
						RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_ENDX]-dx, lineData[VEGALINE_ENDY]-dy));
					}
					else if (lineCap == VEGA_LINECAP_ROUND)
					{
						RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_ENDX]+dx, lineData[VEGALINE_ENDY]+dy));
						RETURN_IF_ERROR(outline->arcTo(lineData[VEGALINE_ENDX]-dx,
										lineData[VEGALINE_ENDY]-dy,
										VEGA_FIXDIV2(lineWidth), VEGA_FIXDIV2(lineWidth), 0, true, true, flatness));
					}
					else
					{
						RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_ENDX]+dx, lineData[VEGALINE_ENDY]+dy));
						RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_ENDX]-dx, lineData[VEGALINE_ENDY]-dy));
					}
				}
			}
			// warp down to the minus point
			// Initialize to get rid of a stupid compiler warning
			unsigned int internal_end_line = 0;
			if (!(VEGA_LINE_ATTRIBUTE(lastLine-1) & VEGA_LINE_ATTR_NOT_VISIBLE))
			{
				// if there was not a join between last and first line the first position should be updated
				// if there was a join, the positions will be identical
				if (!(VEGA_LINE_ATTRIBUTE(firstLine) & VEGA_LINE_ATTR_NOT_VISIBLE) && (outline->getNumLines() > internal_start_line))
				{
					firstLineData = outline->getLine(internal_start_line);
					VEGA_FIX* lastLineData = outline->getLine(outline->getNumLines()-1);
					firstLineData[VEGALINE_STARTX] = lastLineData[VEGALINE_ENDX];
					firstLineData[VEGALINE_STARTY] = lastLineData[VEGALINE_ENDY];
				}
				VEGA_FIX* lineData = getLine(lnum-1);
				RETURN_IF_ERROR(outline->warpTo(lineData[VEGALINE_ENDX]-dx, lineData[VEGALINE_ENDY]-dy));
				internal_end_line = outline->getNumLines();
			}
			for ( lnum = lastLine-1; lnum >= firstLine; --lnum ){

				if (VEGA_LINE_ATTRIBUTE(lnum) & VEGA_LINE_ATTR_NOT_VISIBLE)
					continue;

				VEGA_FIX* lineData = getLine(lnum);
				VEGA_FIX lineLength = getLineLength(lnum);
				dx = 0;
				dy = 0;
				getScaledLineNormal(lineData, lineLength, lineWidth, dx, dy);

				int nextnum = lnum-1;
				if (nextnum < firstLine)
					nextnum = lastLine-1;
				int prevnum = lnum+1;
				if (prevnum >= lastLine)
					prevnum = firstLine;

				if (VEGA_LINE_ATTRIBUTE(prevnum) & VEGA_LINE_ATTR_NOT_VISIBLE)
				{
					// if the previous was not visible, just warp to the start point.
					// otherwise we are already at the startpoint
					RETURN_IF_ERROR(outline->warpTo(lineData[VEGALINE_ENDX]-dx, lineData[VEGALINE_ENDY]-dy));
				}
				if (!(VEGA_LINE_ATTRIBUTE(nextnum) & VEGA_LINE_ATTR_NOT_VISIBLE))
				{
					VEGA_FIX* nextLineData = getLine(nextnum);
					VEGA_FIX nextLineLength = getLineLength(nextnum);
					if (needs_join_on_inside(nextLineData, lineData))
					{
						VEGA_FIX ndx = 0, ndy = 0;
						getScaledLineNormal(nextLineData, nextLineLength, lineWidth, ndx, ndy);
						// move to the end of the line, then render the join
						if (lineJoin == VEGA_LINEJOIN_MITER)
						{
							OP_STATUS ret_val = do_miter_join(outline, nextLineData, lineData, -ndx, -ndy, -dx, -dy);
							if (ret_val == OpStatus::ERR)
							{
								RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_STARTX]-dx, lineData[VEGALINE_STARTY]-dy));
								RETURN_IF_ERROR(outline->lineTo(nextLineData[VEGALINE_ENDX]-ndx,
																nextLineData[VEGALINE_ENDY]-ndy));
							}
							else if (OpStatus::IsError(ret_val))
							{
								return ret_val;
							}
						}
						else if (lineJoin == VEGA_LINEJOIN_ROUND)
						{
							RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_STARTX]-dx, lineData[VEGALINE_STARTY]-dy));

							RETURN_IF_ERROR(outline->arcTo(nextLineData[VEGALINE_ENDX]-ndx,
											nextLineData[VEGALINE_ENDY]-ndy,
											VEGA_FIXDIV2(lineWidth), VEGA_FIXDIV2(lineWidth), 0, false, true, flatness));
						}
						else
						{
							RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_STARTX]-dx, lineData[VEGALINE_STARTY]-dy));
							RETURN_IF_ERROR(outline->lineTo(nextLineData[VEGALINE_ENDX]-ndx,
															nextLineData[VEGALINE_ENDY]-ndy));
						}
					}
					else
					{
						VEGA_FIX ndx = 0, ndy = 0;
						VEGA_FIX intx, inty;
						getScaledLineNormal(nextLineData, nextLineLength, lineWidth, ndx, ndy);
						if (line_intersection(lineData[VEGALINE_STARTX]-dx,
											lineData[VEGALINE_STARTY]-dy,
											lineData[VEGALINE_ENDX]-dx,
											lineData[VEGALINE_ENDY]-dy,
											nextLineData[VEGALINE_STARTX]-ndx,
											nextLineData[VEGALINE_STARTY]-ndy,
											nextLineData[VEGALINE_ENDX]-ndx,
											nextLineData[VEGALINE_ENDY]-ndy,
											intx, inty, false))
						{
							// if the lines intersect, move to the intersection point
							RETURN_IF_ERROR(outline->lineTo(intx, inty));
						}
						else if (VEGA_ABS(VEGA_FIXMUL(dx, nextLineData[VEGALINE_ENDX] -
													  nextLineData[VEGALINE_STARTX]) +
										  VEGA_FIXMUL(dy, nextLineData[VEGALINE_ENDY] -
													  nextLineData[VEGALINE_STARTY])) <= 100*VEGA_EPSILON)
						{
							// Co-linear
							RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_STARTX]-dx, lineData[VEGALINE_STARTY]-dy));
							RETURN_IF_ERROR(outline->lineTo(nextLineData[VEGALINE_ENDX]-ndx, nextLineData[VEGALINE_ENDY]-ndy));
						}
						else
						{
							// else move to the endpoint of this line, or the startpoint of the next
							// depending on which line is longer
							if (line_intersection(lineData[VEGALINE_ENDX]+dx,
												  lineData[VEGALINE_ENDY]+dy,
												  lineData[VEGALINE_ENDX]-dx,
												  lineData[VEGALINE_ENDY]-dy,
												  nextLineData[VEGALINE_STARTX]-ndx,
												  nextLineData[VEGALINE_STARTY]-ndy,
												  nextLineData[VEGALINE_ENDX]-ndx,
												  nextLineData[VEGALINE_ENDY]-ndy,
												  intx, inty, false))
							{
								RETURN_IF_ERROR(outline->lineTo(intx, inty));
								RETURN_IF_ERROR(outline->lineTo(nextLineData[VEGALINE_ENDX]-ndx, nextLineData[VEGALINE_ENDY]-ndy));
							}
							else if (line_intersection(lineData[VEGALINE_STARTX]-dx,
													   lineData[VEGALINE_STARTY]-dy,
													   lineData[VEGALINE_ENDX]-dx,
													   lineData[VEGALINE_ENDY]-dy,
													   nextLineData[VEGALINE_STARTX]-ndx,
													   nextLineData[VEGALINE_STARTY]-ndy,
													   nextLineData[VEGALINE_STARTX]+ndx,
													   nextLineData[VEGALINE_STARTY]+ndy,
													   intx, inty, false))
							{
								RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_STARTX]-dx, lineData[VEGALINE_STARTY]-dy));
								RETURN_IF_ERROR(outline->lineTo(intx, inty));
							}
							else
							{
								RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_STARTX]-dx, lineData[VEGALINE_STARTY]-dy));
								RETURN_IF_ERROR(outline->lineTo(nextLineData[VEGALINE_ENDX]-ndx, nextLineData[VEGALINE_ENDY]-ndy));
							}
						}
					}
				}
				else
				{
					// next line is not visible so we will need a cap
					if (lineCap == VEGA_LINECAP_SQUARE)
					{
						VEGA_FIX dirx = 0, diry = 0;
						getScaledLineDirection(lineData, lineLength, lineWidth, dirx, diry);
						RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_STARTX]-dirx-dx, lineData[VEGALINE_STARTY]-diry-dy));
						RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_STARTX]-dirx+dx, lineData[VEGALINE_STARTY]-diry+dy));
						// FIXME: skip this last segment and add it when drawing in the other direction
						RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_STARTX]+dx, lineData[VEGALINE_STARTY]+dy));
					}
					else if (lineCap == VEGA_LINECAP_ROUND)
					{
						RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_STARTX]-dx, lineData[VEGALINE_STARTY]-dy));

						RETURN_IF_ERROR(outline->arcTo(lineData[VEGALINE_STARTX]+dx,
													   lineData[VEGALINE_STARTY]+dy,
													   VEGA_FIXDIV2(lineWidth), VEGA_FIXDIV2(lineWidth), 0, true, true, flatness));
					}
					else
					{
						RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_STARTX]-dx, lineData[VEGALINE_STARTY]-dy));
						RETURN_IF_ERROR(outline->lineTo(lineData[VEGALINE_STARTX]+dx, lineData[VEGALINE_STARTY]+dy));
					}
				}
			}
			if (!(VEGA_LINE_ATTRIBUTE(lastLine-1) & VEGA_LINE_ATTR_NOT_VISIBLE) && !(VEGA_LINE_ATTRIBUTE(firstLine) & VEGA_LINE_ATTR_NOT_VISIBLE) && (outline->getNumLines() > internal_start_line))
			{
				// if there was not a join between last and first line the first position should be updated
				// if there was a join, the positions will be identical
				VEGA_FIX* endLineData = outline->getLine(internal_end_line);
				VEGA_FIX* lastLineData = outline->getLine(outline->getNumLines()-1);
				endLineData[VEGALINE_STARTX] = lastLineData[VEGALINE_ENDX];
				endLineData[VEGALINE_STARTY] = lastLineData[VEGALINE_ENDY];
			}
			firstLineData = outline->getLine(internal_start_line);
			// warp to the startpos to avoid closing of the path
			RETURN_IF_ERROR(outline->warpTo(firstLineData[VEGALINE_STARTX], firstLineData[VEGALINE_STARTY]));
		}
		// +1 since the last line is the "warp" to the next sub path
		firstLine = lastLine+1;
	}
	return OpStatus::OK;
}

OP_STATUS VEGAPath::createDash(VEGAPath *dashed, VEGA_FIX dashOffset, unsigned int numDash, VEGA_FIX *dashArray, VEGA_FIX precompPathLength){
	RETURN_IF_ERROR(dashed->prepare(getNumLines()));
	dashed->lineJoin = lineJoin;
	dashed->lineCap = lineCap;
	dashed->miterLimit = miterLimit;
	dashed->lineWidth = lineWidth;
	if (getNumLines() == 0)
		return OpStatus::OK;

	VEGA_FIX* lineData = getLine(0);
	RETURN_IF_ERROR(dashed->moveTo(lineData[VEGALINE_STARTX], lineData[VEGALINE_STARTY]));

	if (precompPathLength == 0)
		return OpStatus::ERR; // Don't divide by zero
	else if (precompPathLength > 0)
	{
		VEGA_FIX compPathLength = 0;
		for (unsigned int lnum = 0; lnum < getNumLines(); ++lnum)
		{
			if (!(VEGA_LINE_ATTRIBUTE(lnum) & VEGA_LINE_ATTR_NOT_VISIBLE))
			{
				compPathLength += getLineLength(lnum);
			}
		}

		VEGA_FIX scaleFactor = VEGA_FIXDIV(compPathLength, precompPathLength);

		dashOffset = VEGA_FIXMUL(dashOffset, scaleFactor);
		for (unsigned int i = 0; i < numDash; ++i)
		{
			dashArray[i] = VEGA_FIXMUL(dashArray[i], scaleFactor);
		}
	}

#if 1
	// In order to make progress below, the total dash length cannot be zero
	VEGA_FIX total_dash_len = 0;
	for (unsigned int i = 0; i < numDash; ++i)
		total_dash_len += dashArray[i];

	VEGA_FIX local_dash_array[2];
	if (total_dash_len == 0)
	{
		// It was zero, so use something that isn't
		numDash = 2;
		local_dash_array[0] = VEGA_EPSILON;
		local_dash_array[1] = VEGA_EPSILON;
		dashArray = local_dash_array;
	}
#endif // 1

	int initial_dash = 0;
	bool initial_drawing = true;
	VEGA_FIX initial_len_left = dashArray[0];
	while (dashOffset < 0)
	{
		--initial_dash;
		if (initial_dash < 0)
			initial_dash = numDash-1;
		dashOffset += dashArray[initial_dash];
		initial_drawing = !initial_drawing;
		initial_len_left = dashArray[initial_dash];
	}
	while (dashOffset > 0)
	{
		if (dashOffset < initial_len_left)
		{
			initial_len_left -= dashOffset;
			dashOffset = 0;
		}
		else
		{
			dashOffset -= initial_len_left;
			++initial_dash;
			if (initial_dash >= (int)numDash)
				initial_dash = 0;
			initial_drawing = !initial_drawing;
			initial_len_left = dashArray[initial_dash];
		}
	}
	int cur_dash = initial_dash;
	bool cur_drawing = initial_drawing;
	VEGA_FIX cur_len_left = initial_len_left;

	for (unsigned int lnum = 0; lnum < getNumLines(); ++lnum)
	{
		lineData = getLine(lnum);
		if (!(VEGA_LINE_ATTRIBUTE(lnum) & VEGA_LINE_ATTR_NOT_VISIBLE))
		{
			VEGA_FIX lineLength = getLineLength(lnum);
			if (cur_len_left >= lineLength)
			{
				if (cur_drawing)
				{
					RETURN_IF_ERROR(dashed->lineTo(lineData[VEGALINE_ENDX], lineData[VEGALINE_ENDY]));
				}
				else
				{
					RETURN_IF_ERROR(dashed->warpTo(lineData[VEGALINE_ENDX], lineData[VEGALINE_ENDY]));
				}
				cur_len_left -= lineLength;
			}
			else
			{
				VEGA_FIX line_len = lineLength;
				VEGA_FIX line_len_passed = 0;
				while (line_len > 0)
				{
					VEGA_FIX ex, ey;
					if (cur_len_left >= line_len)
					{
						ex = lineData[VEGALINE_ENDX];
						ey = lineData[VEGALINE_ENDY];
						cur_len_left -= line_len;
						line_len = 0;
					}
					else
					{
						line_len -= cur_len_left;
						// find the point which is line_len_passed + cur_len_left from the start
						VEGA_FIX len = cur_len_left;
						len += line_len_passed;

						len = VEGA_FIXDIV(len, lineLength);

						ex = lineData[VEGALINE_ENDX]-lineData[VEGALINE_STARTX];
						ey = lineData[VEGALINE_ENDY]-lineData[VEGALINE_STARTY];
						ex = VEGA_FIXMUL(ex, len);
						ey = VEGA_FIXMUL(ey, len);
						ex += lineData[VEGALINE_STARTX];
						ey += lineData[VEGALINE_STARTY];

						line_len_passed += cur_len_left;
						cur_len_left = 0;
					}
					// line/move as long as possible, then toggle
					if (cur_drawing)
					{
						RETURN_IF_ERROR(dashed->lineTo(ex, ey));
					}
					else
					{
						RETURN_IF_ERROR(dashed->warpTo(ex, ey));
					}
					if (cur_len_left <= 0)
					{
						++cur_dash;
						if (cur_dash >= (int)numDash)
							cur_dash = 0;
						cur_drawing = !cur_drawing;
						cur_len_left = dashArray[cur_dash];
					}
				}
			}
		}
		else
		{
			cur_dash = initial_dash;
			cur_drawing = initial_drawing;
			cur_len_left = initial_len_left;
			RETURN_IF_ERROR(dashed->warpTo(lineData[VEGALINE_ENDX], lineData[VEGALINE_ENDY]));
		}
	}

	// If the path is closed, the dashed path should also be closed, otherwise linejoins won't work properly
	if (isClosed())
	{
		// Use invisible close if the first line is not visible since invisible
		// lines at the start are not taken into consideration when creating outlines
		RETURN_IF_ERROR(dashed->close(dashed->isLineVisible(0)));
	}

	return OpStatus::OK;
}

#ifdef VEGA_3DDEVICE
/** Count number of directional changes, to be able to prune complex
  * polygons (eg hexagram). */
struct DirectionCounter
{
	DirectionCounter() : lastDir(0), dirChanges(0) {}
	/**
	   Add vector from one point in path to next. This most be done
	   consecutively, for all vertices in the path.
	   @param dx,dy the vector from one point in the path to the next
	 */
	void Vector(VEGA_FIX dx, VEGA_FIX dy)
	{
		curDir = ((dx > 0) ? -1 :
		          (dx < 0) ?  1 :
		          (dy > 0) ? -1 :
		          (dy < 0) ?  1 :
		           0);
		if (curDir == -lastDir)
			++ dirChanges;
		lastDir = curDir;
	}
	/// @return TRUE if path is complex/self-intersecting, FALSE otherwise
	/// NOTE: only valid after all vectors have been added
	bool IsComplex() const { return dirChanges > 2; }

	int curDir, lastDir;
	size_t dirChanges;
};

// Gist: Walk path and check which way the turns are made (using perp
// dot product) - if all turns are the same way and path is simple
// it's also convex. Determining whether path is simple or not is done
// by keeping track of the number of direction changes. If path is not
// simple convex, determine whether it's simple or complex
// (self-intersecting).
// This method should not be called on paths with multiple sub paths.
OP_STATUS VEGAPath::determineCategory()
{
	OP_ASSERT(m_category == UNDETERMINED);
	OP_ASSERT(!hasMultipleSubPaths());
	OP_ASSERT(numLines);
	VEGA_FIX* lineData = getLine(0);

	VEGA_FIX nx = lineData[VEGALINE_ENDY]-lineData[VEGALINE_STARTY];
	VEGA_FIX ny = lineData[VEGALINE_STARTX]-lineData[VEGALINE_ENDX];
	DirectionCounter dir;
	VEGA_FIX dx = 0, dy = 0;

	unsigned int i = 0;
	int expected_sign = 0;
	for (; i < numLines && !expected_sign; ++i)
	{
		lineData = getLine(i);

		dx = lineData[VEGALINE_ENDX]-lineData[VEGALINE_STARTX];
		dy = lineData[VEGALINE_ENDY]-lineData[VEGALINE_STARTY];
		dir.Vector(dx, dy);

		VEGA_FIX dist = VEGA_FIXMUL(nx, dx) + VEGA_FIXMUL(ny, dy);
		expected_sign = (dist > 0) - (dist < 0);
	}

	bool singleConvex = true;
	for (; i < numLines && singleConvex; ++i)
	{
		nx = dy;
		ny = -dx;

		lineData = getLine(i);
		dx = lineData[VEGALINE_ENDX]-lineData[VEGALINE_STARTX];
		dy = lineData[VEGALINE_ENDY]-lineData[VEGALINE_STARTY];
		dir.Vector(dx, dy);

		VEGA_FIX dist = VEGA_FIXMUL(nx, dx) + VEGA_FIXMUL(ny, dy);
		int sign = (dist > 0) - (dist < 0);
		if (sign == -expected_sign)
			singleConvex = false;
		}
	if (singleConvex)
	{
		nx = dy;
		ny = -dx;
		lineData = getLine(0);

		dx = lineData[VEGALINE_ENDX]-lineData[VEGALINE_STARTX];
		dy = lineData[VEGALINE_ENDY]-lineData[VEGALINE_STARTY];
		dir.Vector(dx, dy);

		VEGA_FIX dist = VEGA_FIXMUL(nx, dx) + VEGA_FIXMUL(ny, dy);
		int sign = (dist > 0) - (dist < 0);
		if (sign == -expected_sign)
			singleConvex = false;
	}

	if (singleConvex && !dir.IsComplex())
	{
		m_category = CONVEX;
		return OpStatus::OK;
	}

	// Check if path is self-intersecting or not.
	const OP_BOOLEAN si = isSelfIntersecting();
	RETURN_IF_ERROR(si);

	// Self intersecting paths should hopefully be taken care
	// of in the future. Now we are just bailing.
	if (si == OpBoolean::IS_TRUE)
	{
		m_category = COMPLEX;
		cannotBeConverted = true;
	}
	else
		m_category = SIMPLE;
	return OpStatus::OK;
}

OP_BOOLEAN VEGAPath::isSelfIntersecting()
{
	OP_ASSERT(m_category == UNDETERMINED);
	OP_ASSERT(!hasMultipleSubPaths());

	if (numLines < 3)
		return OpBoolean::IS_FALSE;

	VEGASelfIntersect si(*this);
	return si.isSelfIntersecting();
}
#endif // VEGA_3DDEVICE

#endif // VEGA_SUPPORT
