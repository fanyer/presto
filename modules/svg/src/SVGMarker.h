/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_MARKER_H
#define SVG_MARKER_H

#if defined(SVG_SUPPORT) && defined(SVG_SUPPORT_MARKERS)

#include "modules/svg/svg_number.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/SVGNumberPair.h"
#include "modules/svg/src/SVGPoint.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGValue.h"

#include "modules/util/simset.h"

class OpBpath;
class PathSegListIterator;
class SVGPathSeg;
class SVGVector;
class LayoutProperties;
class HTML_Element;

/**
 * Iterate through the marker positions on a (graphic) element
 */
class SVGMarkerPosIterator
{
public:
	virtual ~SVGMarkerPosIterator() {}

	/**
	 * Move to first marker position
	 * @return OpStatus::OK if successful, OpStatus::ERR_* as appropriate otherwise
	 */
	virtual OP_STATUS First() = 0;

	/**
	 * Move to the next marker position
	 * @return TRUE if next marker found, FALSE otherwise
	 */
	virtual BOOL Next() = 0;

	/**
	 * Indicates if the iterator is at the 'start' marker
	 * (i.e. the first marker)
	 */
	virtual BOOL IsStart() = 0;

	/**
	 * Indicates if the iterator is at the 'end' marker
	 * (i.e. the last marker)
	 */
	virtual BOOL IsEnd() = 0;

	/**
	 * Get the position of the current marker
	 */
	virtual SVGNumberPair GetCurrentPosition() const = 0;

	/**
	 * Get the slope of the current marker
	 */
	virtual SVGNumber GetCurrentSlope() = 0;

protected:
	// Shared utility method
	static SVGNumber VectorToAngle(const SVGNumberPair& v)
	{
		return v.x.atan2(v.y) * 180 / SVGNumber::pi();
	}
};

/**
 * Marker position iterator for paths
 */
class SVGMarkerPathPosIterator : public SVGMarkerPosIterator
{
public:
	SVGMarkerPathPosIterator(OpBpath* p)
		: m_prev_seg(NULL), m_next_seg(NULL),
		  m_path(p), m_path_iter(NULL) {}

	~SVGMarkerPathPosIterator() { OP_DELETE(m_path_iter); }

	OP_STATUS First();
	BOOL Next();

	BOOL IsStart()
	{
		return (m_prev_seg == NULL);
	}

	BOOL IsEnd()
	{
		return (m_next_seg == NULL);
	}

	SVGNumberPair GetCurrentPosition() const
	{
		SVGNumberPair ret;
		if (m_curr_seg)
		{
			ret.x = m_curr_seg->x;
			ret.y = m_curr_seg->y;
		}
		return ret;
	}

	SVGNumber GetCurrentSlope();

private:
	void GetNextSeg();
	static BOOL IsConvertedArc(const SVGPathSeg* seg)
	{
		return
			seg->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_ABS &&
			(seg->info.large == 1 && seg->info.sweep == 1);
	}

	SVGNumberPair m_first_subpath_seg_v;

	const SVGPathSeg* m_prev_seg;
	const SVGPathSeg* m_curr_seg;
	const SVGPathSeg* m_next_seg;

	OpBpath* m_path;
	PathSegListIterator* m_path_iter;
};

/**
 * Marker position iterator for point lists
 */
class SVGMarkerPointListPosIterator : public SVGMarkerPosIterator
{
public:
	SVGMarkerPointListPosIterator(SVGVector* l, BOOL is_closed)
		: m_list(l), m_current_list_pos(0), m_is_closed(is_closed) {}

	SVGNumberPair GetCurrentPosition() const
	{
		return m_curr_pos;
	}

	SVGNumber GetCurrentSlope();

	BOOL IsStart()
	{
		return (m_current_list_pos == 0);
	}

	BOOL IsEnd()
	{
		return
			(m_is_closed && m_current_list_pos == m_list->GetCount()) ||
			(!m_is_closed && m_current_list_pos == m_list->GetCount() - 1);
	}

	OP_STATUS First();
	BOOL Next();

private:
	SVGNumberPair m_curr_pos;

	SVGVector* m_list;

	unsigned int m_current_list_pos;
	BOOL m_is_closed;
};

#endif // SVG_SUPPORT && SVG_SUPPORT_MARKERS
#endif // SVG_MARKER_H
