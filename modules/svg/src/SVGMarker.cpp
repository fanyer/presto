/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/svg/src/svgpch.h"

#if defined(SVG_SUPPORT) && defined(SVG_SUPPORT_MARKERS)

#include "modules/svg/src/SVGMarker.h"

#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/SVGCanvas.h"
#include "modules/svg/src/SVGCanvasState.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGVector.h"

#include "modules/logdoc/htm_elm.h"

#include "modules/layout/cascade.h"

OP_STATUS SVGMarkerPathPosIterator::First()
{
	OP_DELETE(m_path_iter);
	m_path_iter = m_path->GetPathIterator(TRUE /* Normalized */);
	if (!m_path_iter)
		return OpStatus::ERR_NO_MEMORY;

	// Initialize segments
	m_prev_seg = NULL;
	m_next_seg = NULL;

	m_curr_seg = m_path_iter->GetNext();
	if (!m_curr_seg || m_curr_seg->info.type != SVGPathSeg::SVGP_MOVETO_ABS)
		return OpStatus::ERR;

	// Fill in m_next_seg
	GetNextSeg();

	return OpStatus::OK;
}

BOOL SVGMarkerPathPosIterator::Next()
{
	m_prev_seg = m_curr_seg;
	m_curr_seg = m_next_seg;

	if (m_curr_seg == NULL)
		return FALSE;

	// Update m_next_seg
	GetNextSeg();

	return TRUE;
}

// Do some magic to get to the next segment (of interest)
void SVGMarkerPathPosIterator::GetNextSeg()
{
	m_next_seg = m_path_iter->GetNext();

	// Check for specially crafted arc segment,
	// i.e. we are 'inside' a converted/normalized arc
	if (m_next_seg && IsConvertedArc(m_next_seg))
	{
		unsigned int cnt = 0;
		const SVGPathSeg* arc_segs[5];
		arc_segs[cnt++] = m_next_seg;

		while (cnt <= 4)
		{
			arc_segs[cnt] = m_path_iter->GetNext();

			if (arc_segs[cnt] && IsConvertedArc(arc_segs[cnt]))
				cnt++;
			else
				break;
		}

		OP_ASSERT(cnt <= 4 && !(arc_segs[cnt] && IsConvertedArc(arc_segs[cnt])));
		const SVGPathSeg* saved_next = arc_segs[--cnt];

		m_next_seg = arc_segs[cnt];
		while (cnt)
		{
			cnt--;
			if (arc_segs[cnt]->x == m_next_seg->x && arc_segs[cnt]->y == m_next_seg->y &&
				m_next_seg->x == m_next_seg->x1 && m_next_seg->x1 == m_next_seg->x2 &&
				m_next_seg->y == m_next_seg->y1 && m_next_seg->y1 == m_next_seg->y2)
				m_next_seg = arc_segs[cnt];
			else
				break;
		}

		// Reset the current segment
		// This works because when current is bezier,
		// we don't care about the previous segment
		m_curr_seg = m_next_seg;
		m_next_seg = saved_next;
	}
#if 0
	// NOTE: This does not handle non-zero sized arc-parts
	// since there is no way to know when one arc started/ended
	// and thus something like 'M 0,0 A ... A ...' cannot be handled 
	do
	{
		// Check for specially crafted arc segment
		if (m_next_seg &&
			m_next_seg->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_ABS &&
			IsConvertedArc(m_next_seg))
		{
			if (m_curr_seg->x == m_next_seg->x && m_curr_seg->y == m_next_seg->y &&
				m_next_seg->x == m_next_seg->x1 && m_next_seg->x1 == m_next_seg->x2 &&
				m_next_seg->y == m_next_seg->y1 && m_next_seg->y1 == m_next_seg->y2)
			{
				// This segment does not have anything to add to the curve
				// so just skip it
				m_next_seg = m_path_iter->GetNext();
				continue;
			}
		}
		break;
	} while (TRUE);
#endif // 0
}

SVGNumber
SVGMarkerPathPosIterator::GetCurrentSlope()
{
	BOOL needs_in = FALSE;
	BOOL needs_out = FALSE;
	BOOL set_subpath = FALSE;

	if (!m_curr_seg)
		return 0;

	SVGNumberPair out_v, in_v;
	unsigned int segs = 0;

	switch (m_curr_seg->info.type)
	{
	case SVGPathSeg::SVGP_MOVETO_ABS:
		// New sub-path, only outgoing
		needs_out = TRUE;
		set_subpath = TRUE;
		break;
	case SVGPathSeg::SVGP_LINETO_ABS:
	case SVGPathSeg::SVGP_CLOSE:
		needs_in = needs_out = TRUE;
		break;
#ifdef SVG_KEEP_QUADRATIC_CURVES
	case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
		needs_out = TRUE;
		in_v.x = m_curr_seg->x - m_curr_seg->x1;
		in_v.y = m_curr_seg->y - m_curr_seg->y1;
		in_v = in_v.Normalize();
		segs++;
		break;
#endif // SVG_KEEP_QUADRATIC_CURVES
	case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
		// Incoming direction can be determined without previous segment
		needs_out = TRUE;
		in_v.x = m_curr_seg->x - m_curr_seg->x2;
		in_v.y = m_curr_seg->y - m_curr_seg->y2;
		in_v = in_v.Normalize();
		segs++;
		break;
#ifdef _DEBUG
	default:
		OP_ASSERT(!"Unhandled pathseg type");
		break;
#endif // _DEBUG
	}

	if (needs_out && m_next_seg)
	{
		// 'close' followed by other than 'moveto' should have
		// outgoing correspond to first segment of subsequent subpath

		switch (m_next_seg->info.type)
		{
		case SVGPathSeg::SVGP_MOVETO_ABS:
			// No outgoing (but see above)
			if (m_curr_seg->info.type == SVGPathSeg::SVGP_CLOSE)
			{
				// Closed subpaths should orient between
				// incoming and _outgoing_of_first_segment_ in the subpath
				out_v = m_first_subpath_seg_v;
				segs++;
			}
			break;
		case SVGPathSeg::SVGP_LINETO_ABS:
		case SVGPathSeg::SVGP_CLOSE:
		case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
#ifdef SVG_KEEP_QUADRATIC_CURVES
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
#endif // SVG_KEEP_QUADRATIC_CURVES
			{
			out_v.x = m_curr_seg->x;
			out_v.y = m_curr_seg->y;

			SVGNumberPair p2;
			if (m_next_seg->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_ABS)
			{
				if (m_next_seg->x1.Close(out_v.x) && m_next_seg->y1.Close(out_v.y))
				{
					if (m_next_seg->x2.Close(out_v.x) && m_next_seg->y2.Close(out_v.y))
					{
						// Current position coincides with CP1 and CP2 => use endpoint
						p2 = SVGNumberPair(m_next_seg->x, m_next_seg->y);
					}
					else
					{
						// Current position coincides with CP1 => use CP2
						p2 = SVGNumberPair(m_next_seg->x2, m_next_seg->y2);
					}
				}
				else
				{
					// Use CP1
					p2 = SVGNumberPair(m_next_seg->x1, m_next_seg->y1);
				}
			}
#ifdef SVG_KEEP_QUADRATIC_CURVES
			else if (m_next_seg->info.type == SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS)
			{
				if (m_next_seg->x1.Close(out_v.x) && m_next_seg->y1.Close(out_v.y))
				{
					// Current position coincides with CP1 => use endpoint
					p2 = SVGNumberPair(m_next_seg->x, m_next_seg->y);
				}
				else
				{
					// Use CP1
					p2 = SVGNumberPair(m_next_seg->x1, m_next_seg->y1);
				}
			}
#endif // SVG_KEEP_QUADRATIC_CURVES
			else
			{
				p2 = SVGNumberPair(m_next_seg->x, m_next_seg->y);
			}
			out_v.x = p2.x - out_v.x;
			out_v.y = p2.y - out_v.y;
			out_v = out_v.Normalize();
			segs++;
			}
			break;
#ifdef _DEBUG
		default:
			OP_ASSERT(!"Unhandled pathseg type");
			break;
#endif // _DEBUG
		}

		if (set_subpath)
			m_first_subpath_seg_v = out_v;
	}

	if (needs_in && m_prev_seg)
	{
		switch (m_prev_seg->info.type)
		{
		case SVGPathSeg::SVGP_MOVETO_ABS:
		case SVGPathSeg::SVGP_LINETO_ABS:
		case SVGPathSeg::SVGP_CLOSE:
#ifdef SVG_KEEP_QUADRATIC_CURVES
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
#endif // SVG_KEEP_QUADRATIC_CURVES
		case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
			in_v.x = m_curr_seg->x - m_prev_seg->x;
			in_v.y = m_curr_seg->y - m_prev_seg->y;
			in_v = in_v.Normalize();
			segs++;
			break;
#ifdef _DEBUG
		default:
			OP_ASSERT(!"Unhandled pathseg type");
			break;
#endif // _DEBUG
		}		
	}

	OP_ASSERT(segs <= 2);

	// segs : 0 => orientation cannot be determined => angle = 0
	// segs : 1 => one segment, use angle of vector
	// segs : 2 => two segments, use angle bisector
	SVGNumber angle; 
	if (segs)
		angle = VectorToAngle(SVGNumberPair(in_v.x+out_v.x, in_v.y+out_v.y));

	return angle;
}

SVGNumber SVGMarkerPointListPosIterator::GetCurrentSlope()
{
	int next_pos = (int)m_current_list_pos + 1;
	int prev_pos = (int)m_current_list_pos - 1;

	if (m_is_closed)
	{
		int len = m_list->GetCount();
		if (next_pos >= len)
			next_pos -= len;
		if (prev_pos < 0)
			prev_pos += len;
	}

	SVGNumberPair v;

	const SVGPoint* prev_p = static_cast<SVGPoint*>(m_list->Get(prev_pos));
	if (prev_p)
	{
		v.x = m_curr_pos.x - prev_p->x;
		v.y = m_curr_pos.y - prev_p->y;
		v = v.Normalize();
	}

	const SVGPoint* next_p = static_cast<SVGPoint*>(m_list->Get(next_pos));
	if (next_p)
	{
		SVGNumberPair v_tmp(next_p->x - m_curr_pos.x,
							next_p->y - m_curr_pos.y);
		v_tmp = v_tmp.Normalize();
		v.x += v_tmp.x;
		v.y += v_tmp.y;
	}
	return VectorToAngle(v);
}

OP_STATUS SVGMarkerPointListPosIterator::First()
{
	m_current_list_pos = 0;
	const SVGPoint* curr_p = static_cast<SVGPoint*>(m_list->Get(m_current_list_pos));
	if (curr_p)
		m_curr_pos = SVGNumberPair(curr_p->x, curr_p->y);

	return OpStatus::OK;
}

BOOL SVGMarkerPointListPosIterator::Next()
{
	m_current_list_pos++;

	unsigned int pos = m_current_list_pos;

	if (m_is_closed && m_current_list_pos == m_list->GetCount())
		pos = 0; // Allow wrap

	const SVGPoint* curr_p = static_cast<SVGPoint*>(m_list->Get(pos));
	if (curr_p)
	{
		m_curr_pos = SVGNumberPair(curr_p->x, curr_p->y);
		return TRUE;
	}
	return FALSE;
}

#endif // SVG_SUPPORT && SVG_SUPPORT_MARKERS
