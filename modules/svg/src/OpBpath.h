/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef OPBPATH_H
#define OPBPATH_H

#ifdef SVG_SUPPORT

#define FLAT_SEGMENT_STORAGE
#define COMPRESS_PATH_STRINGS
#define KEEP_LAST_MOVETO_INDEX // Optimization for normalized pathseglists

#include "modules/svg/svg_path.h"
#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/SVGNumberPair.h"
#include "modules/svg/src/SVGRect.h"

// This define enables expensive checks before and after modification
// of m_segments in SynchronizedPathSegLists. The check consists of
// looping through all segments and checking if the pointers and
// indexes are correct. By doing this early and often, the source of
// the problem can be find more easily. When disabled, it should
// compile to nothing.
//
// In the code, the macro CHECK_SYNC calls the sync-checking function,
// when enabled.
#if 0
#define SVG_PATH_EXPENSIVE_SYNC_TESTS
#endif

// Integrate functionality of the motion path into OpBpath directly
// Requirements:
// - Transform ()
// - Support the PositionDescriptor interface (basically what
//   parameters animateMotion has) for getting a transform
// - Calculate keyspline
// - Get the (unnormalized) path segment at a certain length
// - Calculate transform at path distance (for textPath)
// - Calculate rotation at path distance (for textPath)
// - GetLength
// - Do magic warping (for textPath)

class SVGCompoundSegment;

class SVGPathSeg
{
public:
	union {
		struct {
			unsigned int type:5;
			unsigned int is_explicit:1;
			// To differentiate explicit moveto:s from implicit

			unsigned int sweep:1;
			unsigned int large:1;
			/* 8 bits */
		} info;
		unsigned char m_seg_info_packed_init;
	};

	SVGPathSeg() :
		m_seg_info_packed_init(0) { }
		/* => type(SVGP_UNKNOWN), is_explicit(0), sweep(0), large(0) */

	void MakeExplicit()
	{
		info.is_explicit = (info.type == SVGP_MOVETO_ABS || info.type == SVGP_MOVETO_REL) ? 1 : 0;
	}

	BOOL operator==(const SVGPathSeg& other) const;

	/* 		SVGP_UNKNOWN
// 		SVGP_CLOSE							= 1,
// 		SVGP_MOVETO_ABS						= 2,
// 		SVGP_MOVETO_REL						= 3,
// 		SVGP_LINETO_ABS						= 4,
// 		SVGP_LINETO_REL						= 5,
// 		SVGP_CURVETO_CUBIC_ABS				= 6,
// 		SVGP_CURVETO_CUBIC_REL				= 7,
// 		SVGP_CURVETO_QUADRATIC_ABS			= 8,
// 		SVGP_CURVETO_QUADRATIC_REL			= 9,
// 		SVGP_ARC_ABS						= 10,
// 		SVGP_ARC_REL						= 11,
// 		SVGP_LINETO_HORIZONTAL_ABS			= 12,
// 		SVGP_LINETO_HORIZONTAL_REL			= 13,
// 		SVGP_LINETO_VERTICAL_ABS			= 14,
// 		SVGP_LINETO_VERTICAL_REL			= 15,
// 		SVGP_CURVETO_CUBIC_SMOOTH_ABS		= 16,
// 		SVGP_CURVETO_CUBIC_SMOOTH_REL		= 17,
// 		SVGP_CURVETO_QUADRATIC_SMOOTH_ABS	= 18,
// 		SVGP_CURVETO_QUADRATIC_SMOOTH_REL	= 19
*/

	SVGNumber x;	///< End x
	SVGNumber y;	///< End y
	// control points for curves
	SVGNumber x1;	///< rx for arcs, ignored for type = {SVGP_LINETO}
	SVGNumber y1;	///< ry for arcs, Ignored for type = {SVGP_LINETO}
	SVGNumber x2;	///< xrot for arcs, ignored for types = {SVGP_LINETO, SVGP_CURVETO_QUADRATIC}
	SVGNumber y2;	///< Ignored for types = {SVGP_ARC, SVGP_LINETO, SVGP_CURVETO_QUADRATIC}

	OP_STATUS Clone(SVGPathSeg** outcopy) const;

	char GetSegTypeAsChar() const;
public:
	enum SVGPathSegType {
		SVGP_UNKNOWN						= 0,
		SVGP_CLOSE							= 1,
		SVGP_MOVETO_ABS						= 2,
		SVGP_MOVETO_REL						= 3,
		SVGP_LINETO_ABS						= 4,
		SVGP_LINETO_REL						= 5,
		SVGP_CURVETO_CUBIC_ABS				= 6,
		SVGP_CURVETO_CUBIC_REL				= 7,
		SVGP_CURVETO_QUADRATIC_ABS			= 8,
		SVGP_CURVETO_QUADRATIC_REL			= 9,
		SVGP_ARC_ABS						= 10,
		SVGP_ARC_REL						= 11,
		SVGP_LINETO_HORIZONTAL_ABS			= 12,
		SVGP_LINETO_HORIZONTAL_REL			= 13,
		SVGP_LINETO_VERTICAL_ABS			= 14,
		SVGP_LINETO_VERTICAL_REL			= 15,
		SVGP_CURVETO_CUBIC_SMOOTH_ABS		= 16,
		SVGP_CURVETO_CUBIC_SMOOTH_REL		= 17,
		SVGP_CURVETO_QUADRATIC_SMOOTH_ABS	= 18,
		SVGP_CURVETO_QUADRATIC_SMOOTH_REL	= 19
	};
};

#ifdef SVG_FULL_11
class SVGDOMPathSegListImpl;

class SVGPathSegObject : public SVGObject
{
public:
	SVGPathSegObject() : SVGObject(SVGOBJECT_PATHSEG) {}
	SVGPathSegObject(const SVGPathSeg& s) : SVGObject(SVGOBJECT_PATHSEG), seg(s) {}

	virtual BOOL IsEqual(const SVGObject &other) const;
	virtual SVGObject *Clone() const { return OP_NEW(SVGPathSegObject, (seg)); }
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const { OP_ASSERT(0); return OpStatus::OK; }

	void Copy(const SVGPathSegObject& s) { seg = s.seg; }
	void Copy(const SVGPathSeg& s) { seg = s; }
	static SVGPathSeg* p(SVGPathSegObject* obj) { return obj ? &obj->seg : NULL; }
	static const SVGPathSeg* p(const SVGPathSegObject* obj) { return obj ? &obj->seg : NULL; }
	SVGPathSeg seg;

	OP_STATUS Sync();
	BOOL IsMember() { return member.compound != NULL; }
	UINT32 Idx() { return member.idx; }

	void SetInList(SVGDOMPathSegListImpl* l) { member.list = l; }
	SVGDOMPathSegListImpl *GetInList() { return member.list; }

	static void Release(SVGPathSegObject* obj);

	BOOL IsValid(UINT32 idx);

	struct Membership
	{
		Membership() : compound(NULL), idx(0), list(NULL) {}
		SVGCompoundSegment* compound; ///< The compound segment this segment is part of, if any.

		// Index in the normalized list of the compound
		// list. (UINT32)-1 has a special meaning, It means that it is
		// a unnormalized path seg of a compound segment
		UINT32 idx;

		// A pointer to the dom list this pathseg is a part of, if
		// any.
		SVGDOMPathSegListImpl* list;
	} member;
};
#endif // SVG_FULL_11

class NormalizedPathSegListInterface
{
public:
	virtual ~NormalizedPathSegListInterface() {} /* To keep the noise down */

	virtual OP_STATUS AddNormalizedCopy(const SVGPathSeg& item) = 0;
};

typedef OpAutoVector<SVGPathSeg> SVGPathSegList;

#ifdef SVG_FULL_11
/*
 * This list is a regular OpVector, but contains reference counted
 * objects. This means that a successful insertion/add should be
 * followed by a SVGObject::IncRef on the inserted object. Like-wise
 * should removal of objects in the list trigger SVGObject::DecRef on
 * the removed objects.
 */
typedef OpVector<SVGPathSegObject> SVGPathSegObjectList;

class SynchronizedPathSegList;

class SVGCompoundSegment : public NormalizedPathSegListInterface
{
public:
	SVGCompoundSegment() : m_seg(NULL), packed_init(0) {}
	~SVGCompoundSegment();

	OP_STATUS Reset(SVGPathSegObject* newseg, UINT32 idx, BOOL normalized,
					const SVGPathSegObject* prevSeg,
					const SVGPathSegObject* lastMovetoNormSeg,
					const SVGPathSegObject* prevNormSeg, 
					const SVGPathSegObject* prevPrevNormSeg);

	UINT32 GetCount() { return (packed.seg_invalid ? m_normalized_seg.GetCount() : 1); }
	UINT32 GetNormalizedCount() { return m_normalized_seg.GetCount(); }

#ifdef _DEBUG
	SVGPathSegObject* GetSeg() { return m_seg; }
#endif // _DEBUG
	SVGPathSegObject* Get(UINT32 idx) { return (packed.seg_invalid ? GetNormalized(idx) : m_seg); }
	SVGPathSegObject* GetNormalized(UINT32 idx) { return m_normalized_seg.Get(idx); }

	virtual OP_STATUS AddNormalizedCopy(const SVGPathSeg& item);
	
	OP_STATUS InsertNormalized(UINT32 idx, SVGPathSegObject* seg);

	OP_STATUS DeleteNormalized(UINT32 idx);

	OP_STATUS Split(OpVector<SVGCompoundSegment>& outlist);

	OP_STATUS Copy(SVGCompoundSegment* from);

	OP_STATUS Sync(SVGPathSegObject* obj);

	void InvalidateSeg();

	struct Membership
	{
		Membership() : list(NULL), idx(0), normidx(0) {}
		SynchronizedPathSegList* list; ///< The path-segment list this compound segment is part of, if any
		UINT32 idx; ///< The index in the unnormalized list
		UINT32 normidx; ///< The index in the normalized list
	} member;

	void UpdateMembership();

protected:
	void SetSegment(SVGPathSegObject* seg) { m_seg = seg; SVGObject::IncRef(m_seg); }
	static void EmptyPathSegObjectList(SVGPathSegObjectList& lst);

private:
	SVGPathSegObject*			m_seg;
	SVGPathSegObjectList		m_normalized_seg;
	union
	{
		struct
		{
			unsigned int seg_invalid:1;
		} packed;
		unsigned int packed_init;
	};
};
#endif // SVG_FULL_11

class NormalizedPathSegList;
class SynchronizedPathSegList;
class PathSegList;

class PathSegListIterator
{
public:
	virtual ~PathSegListIterator() {} /* To keep the noise down */

	virtual SVGPathSeg* GetNext() = 0;
#ifdef SVG_FULL_11
	virtual SVGPathSegObject* GetNextObject() = 0;
#endif // SVG_FULL_11
	virtual void Reset() = 0;
};

class PathSegList
{
public:
	virtual ~PathSegList() {}

	virtual NormalizedPathSegList* GetAsNormalizedPathSegList() { return NULL; }
	virtual SynchronizedPathSegList* GetAsSynchronizedPathSegList() { return NULL; }
	virtual OP_STATUS Copy(PathSegList* plist) = 0;

	virtual OP_STATUS AddCopy(const SVGPathSeg& item) = 0;

#ifdef SVG_FULL_11
	virtual OP_STATUS Add(SVGPathSegObject* seg) = 0;
	virtual OP_STATUS Insert(UINT32 idx, SVGPathSegObject* newval, BOOL normalized) = 0;
	virtual OP_STATUS Replace(UINT32 idx, SVGPathSegObject* newval, BOOL normalized) = 0;
	virtual OP_STATUS Delete(UINT32 idx, BOOL normalized) = 0;
	virtual SVGPathSegObject* Get(UINT32 idx, BOOL normalized = TRUE) const = 0;
#endif // SVG_FULL_11

	virtual void Clear() = 0;
	virtual UINT32 GetCount(BOOL normalized = TRUE) const = 0;
	virtual PathSegListIterator* GetIterator(BOOL normalized = TRUE) const = 0;
	const SVGBoundingBox& GetBoundingBox() const { return m_bbox; }
	virtual void CompactPathList() {}
	void RebuildBoundingBox();

protected:
	SVGBoundingBox m_bbox;
};

class NormalizedPathSegList : public PathSegList, public NormalizedPathSegListInterface
{
public:
	NormalizedPathSegList()
#ifdef FLAT_SEGMENT_STORAGE
		: m_segment_array(NULL),
		m_segment_array_size(0),
		m_segment_array_used(0)
#endif // FLAT_SEGMENT_STORAGE
#ifdef KEEP_LAST_MOVETO_INDEX
# ifdef FLAT_SEGMENT_STORAGE
		,
# else
		:
#endif // FLAT_SEGMENT_STORAGE
		m_last_explicit_moveto_index(-1)
#endif // KEEP_LAST_MOVETO_INDEX
		{}
	virtual ~NormalizedPathSegList() 
	{
#ifdef FLAT_SEGMENT_STORAGE
		OP_DELETEA(m_segment_array);
#endif // FLAT_SEGMENT_STORAGE
	}

	virtual NormalizedPathSegList* GetAsNormalizedPathSegList() { return this; }
	
	virtual OP_STATUS Copy(PathSegList* plist);
	virtual OP_STATUS AddCopy(const SVGPathSeg& item);
	virtual OP_STATUS AddNormalizedCopy(const SVGPathSeg& item);
	virtual UINT32 GetCount(BOOL normalized = TRUE) const;

#ifdef SVG_FULL_11
	virtual OP_STATUS Add(SVGPathSegObject* newval);
	virtual OP_STATUS Insert(UINT32 idx, SVGPathSegObject* newval, BOOL normalized);
	virtual OP_STATUS Replace(UINT32 idx, SVGPathSegObject* newval, BOOL normalized);
	virtual OP_STATUS Delete(UINT32 idx, BOOL normalized);
	virtual SVGPathSegObject* Get(UINT32 idx, BOOL normalized = TRUE) const;
#endif // SVG_FULL_11
	
#ifdef SVG_TINY_12
	SVGPathSeg* GetPathSeg(UINT32 idx) const 
	{
#ifdef FLAT_SEGMENT_STORAGE
		if(m_segment_array && idx < m_segment_array_used)
		{
			return &m_segment_array[idx];
		}
		return NULL;
#else
		return m_segments.Get(idx);
#endif // FLAT_SEGMENT_STORAGE
	}
#endif // SVG_TINY_12

	virtual void Clear();
	virtual void CompactPathList() 
	{
		if (m_segment_array_used != m_segment_array_size)
		{
			OpStatus::Ignore(SetArraySize(m_segment_array_used));
			// else ignore the OOM (hmprf, failing to save memory because of an OOM)
		}
	}

#ifdef FLAT_SEGMENT_STORAGE
	void SetAllocationStepSize(UINT32 step) { 
		if (!m_segment_array && step > 0)
		{
			m_segment_array = OP_NEWA(SVGPathSeg, step);
			if (m_segment_array)
			{
				// If OOM we'll just allocate it again when needed and 
				// then we can propagate the OOM
				m_segment_array_size = step;
			}
		}
	}
#else
	void SetAllocationStepSize(UINT32 step) { m_segments.SetAllocationStepSize(step); }
#endif

	class Iterator : public PathSegListIterator
	{
	public:
		virtual SVGPathSeg* GetNext();
#ifdef SVG_FULL_11
		virtual SVGPathSegObject* GetNextObject() { return NULL; }
#endif // SVG_FULL_11
		virtual void Reset() { m_index = 0; }

	private:
		friend class NormalizedPathSegList;

		Iterator(const NormalizedPathSegList* path, INT32 idx) : m_path(path), m_index(idx) {}
		const NormalizedPathSegList* m_path;
		INT32 m_index;
	};

	virtual PathSegListIterator* GetIterator(BOOL normalized = TRUE) const
	{
		//OP_ASSERT(normalized == TRUE); // Otherwise we get unexpected results
		return OP_NEW(NormalizedPathSegList::Iterator, (this, 0));
	}

private:
	friend class Iterator;
	NormalizedPathSegList(const NormalizedPathSegList& other); // No implementation - this is not copyable
	void operator=(const NormalizedPathSegList& rhs); // No implementation - this is not copyable
	OP_STATUS SetArraySize(UINT32 new_size);

	SVGPathSeg					m_last_added_seg;

#ifdef FLAT_SEGMENT_STORAGE
	SVGPathSeg*  m_segment_array;
	UINT32 m_segment_array_size;
	UINT32 m_segment_array_used;
#else
	OpAutoVector<SVGPathSeg>	m_segments;
#endif // FLAT_SEGMENT_STORAGE

#ifdef KEEP_LAST_MOVETO_INDEX
	int							m_last_explicit_moveto_index;
#endif // KEEP_LAST_MOVETO_INDEX
};

#ifdef SVG_FULL_11
/**
 * This class acts as a backing store for OpBpath,
 * handling syncing of normalized/unnormalized segments
 */

class SynchronizedPathSegList : public PathSegList
{
public:
	SynchronizedPathSegList();
	virtual ~SynchronizedPathSegList() {}

	virtual SynchronizedPathSegList* GetAsSynchronizedPathSegList() { return this; }

	virtual OP_STATUS Copy(PathSegList* plist);

	virtual OP_STATUS AddCopy(const SVGPathSeg& item);

	virtual OP_STATUS Add(SVGPathSegObject* newval);
	virtual OP_STATUS Insert(UINT32 idx, SVGPathSegObject* newval, BOOL normalized);
	virtual OP_STATUS Replace(UINT32 idx, SVGPathSegObject* newval, BOOL normalized);
	virtual OP_STATUS Delete(UINT32 idx, BOOL normalized);

	virtual void Clear();

	virtual UINT32 GetCount(BOOL normalized = TRUE) const;
	virtual SVGPathSegObject* Get(UINT32 idx, BOOL normalized = TRUE) const;

	class Iterator : public PathSegListIterator
	{
	public:
		virtual SVGPathSeg* GetNext();
		virtual SVGPathSegObject* GetNextObject();

		virtual void Reset() { compidx = 0; segidx = 0; }

	private:
		friend class SynchronizedPathSegList;

			Iterator(const SynchronizedPathSegList* path, BOOL normalized) :
					path(path), compidx(0), segidx(0), normalized(normalized) {}

		const SynchronizedPathSegList* path;
		INT32 compidx;
		INT32 segidx;
		BOOL normalized;
	};

	virtual PathSegListIterator* GetIterator(BOOL normalized = TRUE) const
	{
		return OP_NEW(SynchronizedPathSegList::Iterator, (this, normalized));
	}

	OP_STATUS Sync(UINT32 comp_idx, SVGPathSegObject* obj);

protected:

	OP_STATUS SetupNewSegment(SVGCompoundSegment* newseg, SVGPathSegObject* newval, INT32 compidx, INT32 segidx, BOOL normalized);

	INT32 CompoundIndex(UINT32 idx, BOOL normalized, INT32& segindex) const;
	const SVGPathSegObject* GetPrevSeg(INT32 compidx, INT32 segidx);
	void UpdateMembership(UINT32 start_segment_index = 0);

	void PrevNormIdx(INT32& compidx, INT32& segidx);
	const SVGPathSegObject* FindLastMoveTo(INT32 idx, INT32 segidx);
	SVGPathSegObject* GetNormSeg(INT32 compidx, INT32 segidx) const;
	SVGPathSegObject* GetSeg(INT32 compidx, INT32 segidx) const;

private:
	friend class Iterator;

#ifdef SVG_PATH_EXPENSIVE_SYNC_TESTS
	void CheckSync() const;
#endif // _DEBUG

	OpAutoVector<SVGCompoundSegment>	m_segments;
	UINT32								m_normalized_count;
	UINT32								m_count;
};
#endif // SVG_FULL_11

class SVGBoundingBox;

/**
 * This class defines a curve that may contain the following
 * operations: moveto, lineto, {quadratic,cubic}curveto and arcto.
 */
class OpBpath : public SVGObject, public SVGPath
{
private:
	OpBpath();

public:
	static OP_STATUS Make(OpBpath*& outpath, BOOL used_by_dom = TRUE, UINT32 number_of_segments_hint = 0);

	/** Deletes the path including all path commands and subpaths. */
	virtual ~OpBpath();

	virtual BOOL IsEqual(const SVGObject& obj) const;
	OP_STATUS Interpolate(const OpBpath &p1, const OpBpath& p2, SVG_ANIMATION_INTERVAL_POSITION interval_position);
	OP_STATUS Copy(const OpBpath &p1);

	virtual SVGObject *Clone() const;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue& animation_value);

	/**
	 * Move to the specified coordinate. Doing this will create a new
	 * path segment and the old segment (if any) will be left open.  If
	 * you want the old path segment to be closed, then you need to
	 * call OpBpath::Close() before doing the moveto.  If you call
	 * MoveTo several times in a row without anything else in between
	 * the first MoveTo will be interpreted as a MoveTo and the
	 * remaining ones will be interpreted as LineTo.
	 *
	 * @param x The x position
	 * @param y The y position
	 * @param relative TRUE if the given (x,y) are relative to the current position, FALSE if the values are absolute
	 */
#ifdef MoveTo
#define oldmoveto MoveTo
#undef MoveTo
#endif
	virtual OP_STATUS MoveTo(SVGNumber x, SVGNumber y, BOOL relative);

	/**
	 * Move to the specified coordinate. Doing this will create a new
	 * path segment and the old segment (if any) will be left open.
	 * If you want the old path segment to be closed, then you need to
	 * call OpBpath::Close() before doing the moveto.
	 *
	 * This is a convinience function
	 *
	 * @param x The x position
	 * @param y The y position
	 * @param relative TRUE if the given (x,y) are relative to the current position, FALSE if the values are absolute
	 */
	OP_STATUS MoveTo(const SVGNumberPair& coord, BOOL relative, BOOL is_explicit = TRUE);
#ifdef oldmoveto
#define MoveTo oldmoveto
#undef oldmoveto
#endif

	OP_STATUS CommonMoveTo(SVGNumber x, SVGNumber y, BOOL relative, BOOL is_explicit = TRUE);

	/**
	 * Line to the specified coordinate.
	 *
	 * This is a convinence function
	 *
	 * @param x The end x position
	 * @param y The end y position
	 * @param relative TRUE if the given (x,y) are relative to the current position, FALSE if the values are absolute
	 */
	virtual OP_STATUS LineTo(const SVGNumberPair& coord, BOOL relative);

	/**
	 * Line to the specified coordinate.
	 *
	 * @param x The end x position
	 * @param y The end y position
	 * @param relative TRUE if the given (x,y) are relative to the current position, FALSE if the values are absolute
	 */
	virtual OP_STATUS LineTo(SVGNumber x, SVGNumber y, BOOL relative);
	
	/**
	 * Horizontal straight line to the specified x coordinate.
	 *
	 * @param x The x position
	 * @param relative TRUE if the given x is relative to the current position, FALSE if the value is absolute
	 */
	virtual OP_STATUS HLineTo(SVGNumber x, BOOL relative);
	
	/**
	 * Vertical straight line to the specified y coordinate.
	 *
	 * @param y The y position
	 * @param relative TRUE if the given y is relative to the current position, FALSE if the value is absolute
	 */
	virtual OP_STATUS VLineTo(SVGNumber y, BOOL relative);
	
	/**
	 * Cubic Bezier curve to.
	 * 
	 * If you pass smooth=TRUE then the first control point is assumed
	 * to be the reflection of the second control point on the previous
	 * command relative to the current point. If there is no previous
	 * command or if the previous command was not a cubic curve, assume
	 * the first control point is coincident with the current point.
	 * 
	 * @param cp1x The first controlpoint x coordinate
	 * @param cp1y The first controlpoint y coordinate
	 * @param cp2x The second controlpoint x coordinate
	 * @param cp2y The second controlpoint y coordinate
	 * @param endx The end x coordinate
	 * @param endy The end y coordinate
	 * @param smooth If TRUE then the first controlpoint will be calculated for you, you only need to pass the second controlpoint and endpoint
	 * @param relative TRUE if the given coordinates are relative to the current position, FALSE if the values are absolute
	 */
	virtual OP_STATUS CubicCurveTo(	SVGNumber cp1x, SVGNumber cp1y,
									SVGNumber cp2x, SVGNumber cp2y, 
									SVGNumber endx, SVGNumber endy, 
									BOOL smooth, BOOL relative);

	/**
	 * Quadratic Bezier curve to.
	 * 
	 * If you pass smooth=TRUE then the control point is assumed to be
	 * the reflection of the control point on the previous command
	 * relative to the current point. If there is no previous command
	 * or if the previous command was not a quadratic curve, assume the
	 * control point is coincident with the current point.
	 * 
	 * @param cp1x The first controlpoint x coordinate
	 * @param cp1y The first controlpoint y coordinate
	 * @param endx The end x coordinate
	 * @param endy The end y coordinate
	 * @param smooth If TRUE then the first controlpoint will be calculated for you, you only need to pass the second controlpoint and endpoint
	 * @param relative TRUE if the given coordinates are relative to the current position, FALSE if the values are absolute
	 */		
	virtual OP_STATUS QuadraticCurveTo( SVGNumber cp1x, SVGNumber cp1y, 
										SVGNumber endx, SVGNumber endy, 
										BOOL smooth, BOOL relative);
	
	/**
	 * Arc to.
	 *
	 * Add an elliptical arc from the current point to (x, y). The
	 * size and orientation of the ellipsi are defined by two radii
	 * (rx, ry) and an x-axis rotation.
	 *
	 * @param rx The x radius
	 * @param ry The y radius
	 * @param xrot The rotation in degrees around the x-axis
	 * @param large If TRUE then choose the largest arc
	 * @param sweep If TRUE arc will be drawn in a "positive-angle" direction
	 * @param x The end x coordinate
	 * @param y The end y coordinate
	 */
	virtual OP_STATUS ArcTo(SVGNumber rx, SVGNumber ry,
							SVGNumber xrot,
							BOOL large,
							BOOL sweep,
							SVGNumber x,
							SVGNumber y, BOOL relative);

	/**
	 * Close this path segment. You can still add other commands after
	 * doing this, it just means this segment is closed.  Paths are not
	 * automatically closed for you, the default is to create open
	 * paths.
	 */
	virtual OP_STATUS Close();

	/**
	 * Reduces memory usage for the path, but potentially makes
	 * changes to it slower.
	 */
	void CompactPath() { m_pathlist->CompactPathList(); }

#ifdef SVG_FULL_11
	// Slow random access methods
	SVGPathSegObject* Get(UINT32 idx, BOOL normalized = TRUE) const
	{
		return m_pathlist->Get(idx, normalized);
	}

	OP_STATUS Set(UINT32 idx, const SVGPathSeg& val, BOOL normalized = TRUE)
	{
		SVGPathSegObject* copy = NULL;
		copy = OP_NEW(SVGPathSegObject, (val));
		if (!copy)
			return OpStatus::ERR_NO_MEMORY;

		return m_pathlist->Replace(idx, copy, normalized);
	}

	OP_STATUS Add(SVGPathSegObject* newval)
	{
		return m_pathlist->Add(newval);
	}

	OP_STATUS Insert(UINT32 idx, SVGPathSegObject* newval, BOOL normalized)
	{
		return m_pathlist->Insert(idx, newval, normalized);
	}

	OP_STATUS Replace(UINT32 idx, SVGPathSegObject* newval, BOOL normalized)
	{
		if (!newval)
			return OpStatus::ERR_NULL_POINTER;

		return m_pathlist->Replace(idx, newval, normalized);
	}

	OP_STATUS Delete(UINT32 idx, BOOL normalized)
	{
		return m_pathlist->Delete(idx, normalized);
	}
#endif // SVG_FULL_11

#ifdef SVG_TINY_12
	/** Gets the SVGPathSeg at specified index in the normalized list. */
	SVGPathSeg* GetPathSeg(UINT32 idx) const
	{
		NormalizedPathSegList* nl = m_pathlist->GetAsNormalizedPathSegList();
		if(nl)
			return nl->GetPathSeg(idx);
		
		return NULL;
	}
#endif // SVG_TINY_12

	UINT32 GetCount(BOOL normalized = TRUE) const
	{
		return m_pathlist->GetCount(normalized);
	}

	OP_STATUS AddCopy(const SVGPathSeg& newval)
	{
		return m_pathlist->AddCopy(newval);
	}

	void Clear()
	{
		m_pathlist->Clear();
	}

	OP_STATUS SetUsedByDOM(BOOL val);

	/**
	 * Gets the boundingbox for the entire path.
	 * @return bbox The boundingbox (output)
	 */
	const SVGBoundingBox& GetBoundingBox() const
	{
		return m_pathlist->GetBoundingBox();
	}

	OP_STATUS Concat(const OpBpath &p1);

	/**
	 * Call this if you have manually changed values in 
	 * the path to make sure the bounding box is still valid.
	 */
	void RecalculateBoundingBox() { m_pathlist->RebuildBoundingBox(); }

	static void UpdateBoundingBox(const SVGNumberPair& curr_pt, const SVGPathSeg* cmd, SVGBoundingBox &bbox);

	/**
	 * Convert all segments in the path to cubic bezier curves.
	 * MoveTos are unaffected.
	 * @param newpath The new path will be returned here
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS Bezierize(OpBpath** newpath) const;

	/**
	 * Convert an arc segment into bezier and append to the outlist.
	 * @param arcseg The arc segment to convert
	 * @param outlist The result will be appended to this list
	 * @return OpStatus::OK if successful
	 */
	static OP_STATUS ConvertArcToBezier(const SVGPathSeg* arcseg, SVGNumberPair& ioCurrentPos, SVGPathSeg* outlist, UINT32& outsize);

	/**
	 * Iterator interface for OpBpath
	 * @param normalized TRUE if the iterator should iterate over the normalized path
	 * @return Iterator with reference to the first path segment
	 */
	PathSegListIterator* GetPathIterator(BOOL normalized) const
	{
		return m_pathlist->GetIterator(normalized);
	}

	static OP_STATUS NormalizeSegment(const SVGPathSeg* seg, 
									  const SVGPathSeg* prevSeg,
									  const SVGPathSeg* lastMovetoNormSeg,
									  const SVGPathSeg* prevNormSeg, 
									  const SVGPathSeg* prevPrevNormSeg,
									  NormalizedPathSegListInterface* outlist);

	static void		ConvertQuadraticToCubic(SVGNumber cp1x, SVGNumber cp1y, SVGNumber endx, SVGNumber endy, BOOL relative, SVGNumber curx, SVGNumber cury, SVGPathSeg& cubiccmd);

#ifdef COMPRESS_PATH_STRINGS
	OP_STATUS		SetString(const uni_char *str, UINT32 len);
#else
	OP_STATUS		SetString(const uni_char *str, UINT32 len) { return m_str_rep_utf8.SetUTF8FromUTF16(str, len); }
#endif // COMPRESS_PATH_STRINGS

protected:

	void            PrepareInterpolatedCmd(const SVGPathSeg* c1, SVGNumberPair current_point, SVGPathSeg& out);
	BOOL            GetInterpolatedCmd(const SVGPathSeg* c1, const SVGPathSeg* c2, SVGPathSeg& c3, SVGNumber where);
	BOOL            AddCmdSum(const SVGPathSeg& c1, const SVGPathSeg& c2);

	static OP_STATUS LowConvertArcToBezier(const SVGPathSeg* arcseg, SVGNumberPair& ioCurrentPos, SVGPathSeg* outlist, UINT32& outsize);

private:

	PathSegList* 		m_pathlist;
	SVGNumberPair		m_current_pos;

#ifdef COMPRESS_PATH_STRINGS
	/**
	 * Compressed string representation of the string, or NULL if
	 * there isn't any.
	 */
	UINT8*				m_compressed_str_rep;
	/**
	 * If m_compressed_str_rep_utf8 is != NULL, then this is the
	 * length of the buffer.
	 */
	int					m_compressed_str_rep_length;
#endif // COMPRESS_PATH_STRINGS
};

typedef OpAutoVector<OpBpath> OpBpathList;

#endif // SVG_SUPPORT
#endif // OPBPATH_H

