/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGMotionPath.h"
#include "modules/svg/src/SVGMatrix.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/SVGKeySpline.h"
#include "modules/libvega/vegapath.h"
#include "modules/libvega/vegatransform.h"

//#define SVG_DEBUG_DONT_WARP

struct FlattenedPathIndex
{
	FlattenedPathIndex() { Reset(); }
	void Reset()
	{
		accumulated_length = 0;
		index = 0;
	}

	unsigned int index;
	VEGA_FIX accumulated_length;
};

SVGMotionPath::PositionDescriptor::PositionDescriptor() :
	where(0),
	next_where(-1),
	calcMode(SVGCALCMODE_PACED),
	keyTimes(NULL),
	keyPoints(NULL),
	keySplines(NULL),
	rotate(NULL)
{
}

SVGMotionPath::SVGMotionPath() :
	m_path(NULL),
	m_vega_path(NULL),
	m_vega_path_length(0),
	m_vega_line_segments(0),
	m_vega_segment_indexes(NULL),
	m_vega_segment_lengths(NULL),
	m_flatness(0.05),
	m_path_length(-1)
{
}

SVGMotionPath::~SVGMotionPath()
{
	Reset();
}

OP_STATUS SVGMotionPath::Set(OpBpath* path, SVGNumber path_length, BOOL account_moveto)
{
	OP_ASSERT(path);

	Reset();

	SVGObject::IncRef(path);
	m_path = path;
	m_vega_path = ConstructVegaPath(account_moveto);
	if (!m_vega_path)
		return OpStatus::ERR_NO_MEMORY;

	m_vega_path_length = GetSubLength(m_vega_path, 0, m_vega_path->getNumLines());

	if (path_length > 0)
		m_path_length = path_length;
	else
		m_path_length = m_vega_path_length;

	return OpStatus::OK;
}

void
SVGMotionPath::Reset()
{
	// Cast away const for a moment to do reference counting
	SVGObject::DecRef(m_path);
	if (m_vega_path)
	{
		OP_DELETE(m_vega_path);
		m_vega_path = NULL;
	}
	m_vega_path_length = 0;
	if (m_vega_segment_lengths)
	{
		OP_DELETEA(m_vega_segment_lengths);
		m_vega_segment_lengths = NULL;
	}
	if (m_vega_segment_indexes)
	{
		OP_DELETEA(m_vega_segment_indexes);
		m_vega_segment_indexes = NULL;
	}
}

SVGNumber
SVGMotionPath::GetSubLength(VEGAPath* path,
							unsigned int start_idx, unsigned int end_idx)
{
	OP_ASSERT(path);

	VEGA_FIX sum = VEGA_INTTOFIX(0);
	for (unsigned int i = start_idx; i < end_idx; i++)
	{
		if (!path->isLineVisible(i) ||
			path->isLineWarp(i))
		{
			continue;
		}

		sum += path->getLineLength(i);
	}
	SVGNumber path_length;
	path_length.SetVegaNumber(sum);
	return path_length;
}

SVGNumber SVGMotionPath::GetSegmentLength(unsigned int idx)
{
	if (m_vega_path && idx < m_vega_line_segments)
	{
		if (m_vega_segment_lengths[idx].Equal(0))
		{
			m_vega_segment_lengths[idx] =
				GetSubLength(m_vega_path,
							 idx == 0 ? 0 : m_vega_segment_indexes[idx - 1],
							 m_vega_segment_indexes[idx]);
		}
		return m_vega_segment_lengths[idx];
	}
	return 0;
}

SVGNumber SVGMotionPath::GetAccumulatedSegmentLength(unsigned int idx)
{
	if (m_vega_path &&
		m_vega_line_segments > 0)
	{
		if (idx > m_vega_line_segments)
			idx = m_vega_line_segments;

		SVGNumber sum(0);
		for (unsigned int i = 0; i < idx; i++)
		{
			sum += GetSegmentLength(i);
		}
		return sum;
	}
	return 0;
}

unsigned int SVGMotionPath::GetSegmentIndexAtLength(SVGNumber len)
{
	// FIXME: Return what index if input length > path length || input length < 0 ?
	// Returning last/first for now
	if (len < 0)
		return 0;

	if (m_vega_path && m_vega_line_segments > 0)
	{
		SVGNumber length_sum;
		unsigned int i = 0;
		length_sum = GetSegmentLength(i);
		while (len > length_sum && i < m_vega_line_segments)
		{
			i++;
			length_sum += GetSegmentLength(i);
		}
		if (i >= m_vega_line_segments)
			i = m_vega_line_segments-1;
		return i;
	}
	return 0;
}

SVGNumber SVGMotionPath::CalculateKeyTimes(PositionDescriptor& pos)
{
	if (pos.keyTimes == NULL ||
		pos.keyPoints == NULL ||
		pos.keyTimes->GetCount() != pos.keyPoints->GetCount())
	{
		return 0;
	}

	SVGVector& keyTimes = *pos.keyTimes;
	SVGVector& keyPoints = *pos.keyPoints;

	if (keyTimes.GetCount() > 1)
	{
		unsigned int endIdx;
		for (endIdx = 1;
			 endIdx < keyTimes.GetCount() && pos.where > ((SVGNumberObject *)keyTimes.Get(endIdx))->value;
			 endIdx++)
			;

		SVGNumber ktbegin = ((SVGNumberObject *)keyTimes.Get(endIdx-1))->value;
		SVGNumber kpbegin = ((SVGNumberObject *)keyPoints.Get(endIdx-1))->value;

		SVGNumber ktend, kpend;
		if (endIdx == keyTimes.GetCount())
		{
			ktend = 1;
			kpend = 1;
		}
		else
		{
			ktend = ((SVGNumberObject *)keyTimes.Get(endIdx))->value;
			kpend = ((SVGNumberObject *)keyPoints.Get(endIdx))->value;
		}

		SVGNumber q2;
		if (pos.calcMode != SVGCALCMODE_DISCRETE)
		{
			SVGNumber q1 = (pos.where - ktbegin) / (ktend - ktbegin); // the relative position in the time interval
			q1 = CalculateKeySplines(pos, endIdx - 1, q1);
			q2 = kpbegin + (kpend - kpbegin) * q1;    // the relative position in the total path
		}
		else
		{
			q2 = kpbegin;
		}
		return q2;
	}
	return 0;
}

SVGNumber
SVGMotionPath::CalculateKeySpline(SVGKeySpline* keyspline, SVGNumber flatness, SVGNumber q1)
{
	VEGAPath spath;
	// FIXME: Return something else than zero on error?
	RETURN_VALUE_IF_ERROR(spath.moveTo(VEGA_INTTOFIX(0), VEGA_INTTOFIX(0)), 0);
	RETURN_VALUE_IF_ERROR(spath.cubicBezierTo(VEGA_INTTOFIX(1), VEGA_INTTOFIX(1),
											  (keyspline->m_v[0]).GetVegaNumber(),
											  (keyspline->m_v[1]).GetVegaNumber(),
											  (keyspline->m_v[2]).GetVegaNumber(),
											  (keyspline->m_v[3]).GetVegaNumber(),
											  (SVGNumber(0.01).GetVegaNumber())), 0);

	for (unsigned int i=0; i<spath.getNumLines();i++)
	{
		VEGA_FIX* line = spath.getLine(i);
		if ((q1).GetVegaNumber() >= line[VEGALINE_STARTX] &&
			(q1).GetVegaNumber() < line[VEGALINE_ENDX])
		{
			VEGA_FIX lx = line[VEGALINE_ENDX] - line[VEGALINE_STARTX];
			VEGA_FIX ly = line[VEGALINE_ENDY] - line[VEGALINE_STARTY];
			VEGA_FIX result;
			if (lx > VEGA_INTTOFIX(0))
			{
				VEGA_FIX slope = VEGA_FIXDIV(ly, lx);
				VEGA_FIX dx = (q1).GetVegaNumber() - line[VEGALINE_STARTX];
				VEGA_FIX dy = VEGA_FIXMUL(dx, slope);
				result = line[VEGALINE_STARTY] + dy;
			}
			else
			{
				result = line[VEGALINE_STARTY];
			}

			q1.SetVegaNumber(result);
			break;
		}
	}

	return q1;
}

SVGNumber
SVGMotionPath::CalculateKeySplines(PositionDescriptor& pos, unsigned int whichInterval, SVGNumber& q1)
{
	if (pos.keySplines != NULL)
	{
		SVGVector* keySplines = pos.keySplines;
		if (keySplines &&
			whichInterval < keySplines->GetCount() &&
			keySplines->VectorType() == SVGOBJECT_KEYSPLINE)
		{
			SVGKeySpline* keyspline = (SVGKeySpline*)keySplines->Get(whichInterval);

			if (keyspline != NULL)
			{
				q1 = CalculateKeySpline(keyspline, m_flatness, q1);
			}
		}
	}
	return q1;
}

OP_STATUS
SVGMotionPath::CalculateCurrentDistanceAlongPath(PositionDescriptor& pos, SVGNumber& distance)
{
	switch(pos.calcMode)
	{
	case SVGCALCMODE_PACED:
		{
			distance = pos.where * m_path_length; // Paced calculation mode ignores keyTimes and keyPoints
			return OpStatus::OK;
		}
	case SVGCALCMODE_SPLINE:
	case SVGCALCMODE_LINEAR:
		{
			SVGNumber c;
			if (pos.keyTimes == NULL ||
				pos.keyPoints == NULL ||
				pos.keyTimes->GetCount() == 0 ||
				pos.keyPoints->GetCount() == 0)
			{
				unsigned int ic = m_path->GetCount(FALSE);        // how many subintervals
				if(ic > 1)
					ic--;

				// avoid division-by-zero
				if(ic == 0)
					return OpSVGStatus::INVALID_ANIMATION;

				SVGNumber il = SVGNumber(1) / (SVGNumber(ic)); // how long are each
				SVGNumber q;
				unsigned int a;
				if (pos.where.Equal(1)) // special-treatment
				{
					q = 1;
					a = ic - 1;
				}
				else
				{
					SVGNumber r = pos.where % il;                // how far along are we in the time subinterval
					q = r / il;                        // make that value relative (0-1)
					a = (unsigned int)(pos.where*ic).GetIntegerValue();  // which interval are we in
				}
				if (pos.calcMode == SVGCALCMODE_SPLINE)
				{
					q = CalculateKeySplines(pos, a, q);
				}
				SVGNumber p = GetSegmentLength(a);
				SVGNumber b = GetAccumulatedSegmentLength(a);
				c = b + q * p;                      // where are we in this interval
			}
			else
			{
				c = CalculateKeyTimes(pos) * m_vega_path_length;
			}
			distance = c / m_vega_path_length * m_path_length;
			return OpStatus::OK;
		}
	case SVGCALCMODE_DISCRETE:
		{
			unsigned int ic = m_path->GetCount(FALSE);           // how many subintervals
			// avoid division-by-zero
			if(ic == 0)
				return OpSVGStatus::INVALID_ANIMATION;

			unsigned int a = (unsigned int)(pos.where * ic).GetIntegerValue();
			SVGNumber b = GetAccumulatedSegmentLength(a); // how long are this interval relative to the total length

			SVGNumber c;
			if (pos.keyTimes == NULL ||
				pos.keyPoints == NULL ||
				pos.keyTimes->GetCount() == 0 ||
				pos.keyPoints->GetCount() == 0)
			{
				c = b;
			}
			else
			{
				c = CalculateKeyTimes(pos) * m_vega_path_length;
			}

			SVGNumber next_where = SVGNumber(a + 1) / SVGNumber(ic);
			if (next_where > SVGNumber(1))
				next_where = next_where - SVGNumber(1);
			pos.next_where = next_where;

			distance = c / m_vega_path_length * m_path_length;
			return OpStatus::OK;
		}
	default:
		break;
	}

	OP_ASSERT(!"Not reached");
	return OpStatus::ERR;
}

VEGAPath*
SVGMotionPath::ConstructVegaPath(BOOL account_moveto)
{
	VEGAPath* vpath = OP_NEW(VEGAPath, ());
	if (!vpath)
		return NULL;

	// FIXME: OOM - What to do if we fail to create the path?
	PathSegListIterator* iter = m_path->GetPathIterator(TRUE); /* normalized */
	if (!iter)
	{
		OP_DELETE(vpath);
		return NULL;
	}

	OpINT32Vector tmp_line_indexes;

	OP_STATUS status = OpStatus::OK;
	unsigned int i;

	for (i = 0; i < m_path->GetCount(TRUE) && OpStatus::IsSuccess(status); i++)
	{
		const SVGPathSeg* cmd = iter->GetNext();
		if (i == 0 && cmd->info.type != SVGPathSeg::SVGP_MOVETO_ABS)
		{
			status = OpStatus::ERR;
			break;
		}

		switch (cmd->info.type)
		{
		case SVGPathSeg::SVGP_MOVETO_ABS:
			status = vpath->moveTo((cmd->x).GetVegaNumber(), (cmd->y).GetVegaNumber());
			break;
#ifdef SVG_KEEP_QUADRATIC_CURVES
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
			status = vpath->quadraticBezierTo((cmd->x).GetVegaNumber(), (cmd->y).GetVegaNumber(),
											  (cmd->x1).GetVegaNumber(), (cmd->y1).GetVegaNumber(),
											  (m_flatness).GetVegaNumber());
			break;
#endif // SVG_KEEP_QUADRATIC_CURVES
		case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
			status = vpath->cubicBezierTo((cmd->x).GetVegaNumber(),(cmd->y).GetVegaNumber(),
										  (cmd->x1).GetVegaNumber(), (cmd->y1).GetVegaNumber(),
										  (cmd->x2).GetVegaNumber(), (cmd->y2).GetVegaNumber(),
										  (m_flatness).GetVegaNumber());
			break;
		case SVGPathSeg::SVGP_LINETO_ABS:
			status = vpath->lineTo((cmd->x).GetVegaNumber(), (cmd->y).GetVegaNumber());
			break;
		case SVGPathSeg::SVGP_CLOSE:
			status = vpath->close(true);
			break;
#ifdef _DEBUG
		default:
			OP_ASSERT(!"Bad pathseg type, please normalize first!");
#endif // _DEBUG
		}

		if (account_moveto || cmd->info.type != SVGPathSeg::SVGP_MOVETO_ABS)
			tmp_line_indexes.Add(vpath->getNumLines());
	}
	OP_DELETE(iter);

	if (OpStatus::IsError(status))
	{
		OP_DELETE(vpath);
		return NULL;
	}

	if (!m_transform.IsIdentity())
	{
		VEGATransform pathxfrm;
		m_transform.CopyToVEGATransform(pathxfrm);
		vpath->transform(&pathxfrm);
	}

	m_vega_line_segments = tmp_line_indexes.GetCount();

	m_vega_segment_indexes = OP_NEWA(unsigned int, m_vega_line_segments);
	m_vega_segment_lengths = OP_NEWA(SVGNumber, m_vega_line_segments);

	if (!m_vega_segment_indexes || !m_vega_segment_lengths)
	{
		OP_DELETEA(m_vega_segment_indexes);
		m_vega_segment_indexes = NULL;

		OP_DELETEA(m_vega_segment_lengths);
		m_vega_segment_lengths = NULL;

		m_vega_line_segments = 0;

		OP_DELETE(vpath);

		return NULL;
	}

	for (i = 0; i < m_vega_line_segments; i++)
	{
		m_vega_segment_indexes[i] = tmp_line_indexes.Get(i);
		m_vega_segment_lengths[i] = 0;
	}
	return vpath;
}

static SVGNumber CalculateAngleOfLine(const VEGA_FIX* line)
{
	SVGNumber snx, sny;
	snx.SetVegaNumber(line[VEGALINE_ENDX] - line[VEGALINE_STARTX]);
	sny.SetVegaNumber(line[VEGALINE_ENDY] - line[VEGALINE_STARTY]);

	return snx.atan2(sny);
}

static SVGNumber CalculateAngleFromRotate(SVGRotate* rotate, const VEGA_FIX* line)
{
	if (rotate->GetRotateType() == SVGROTATE_ANGLE)
	{
		SVGAngle* angle = rotate->GetAngle();
		if (angle)
			return angle->GetAngleInUnits(SVGANGLE_RAD);
	}
	else if (rotate->GetRotateType() == SVGROTATE_AUTO ||
			 rotate->GetRotateType() == SVGROTATE_AUTOREVERSE)
	{
		SVGNumber theta = CalculateAngleOfLine(line);

		if (rotate->GetRotateType() == SVGROTATE_AUTOREVERSE)
		{
			theta += SVGNumber::pi();
			if (theta > (SVGNumber::pi() * 2))
				theta -= (SVGNumber::pi() * 2);
		}
		return theta;
	}
	return 0;
}

BOOL
SVGMotionPath::DistanceToLineIndex(SVGNumber distance, FlattenedPathIndex& fpi)
{
	OP_ASSERT(m_vega_path);
	SVGNumber to_pathlength_scalefactor = m_path_length / m_vega_path_length;
	VEGA_FIX to_pathlen_sf = to_pathlength_scalefactor.GetVegaNumber();

	while (fpi.index < m_vega_path->getNumLines())
	{
		if (!m_vega_path->isLineVisible(fpi.index) || m_vega_path->isLineWarp(fpi.index))
		{
			fpi.index++;
			continue;
		}

		VEGA_FIX segment_length = VEGA_FIXMUL(m_vega_path->getLineLength(fpi.index), to_pathlen_sf);

		if (fpi.accumulated_length + segment_length >= distance.GetVegaNumber())
			return TRUE;

		fpi.accumulated_length = fpi.accumulated_length + segment_length;
		fpi.index++;
	}
	return FALSE;
}

void
SVGMotionPath::CalculateTransformAtDistance(SVGNumber distance, SVGRotate *rotate, SVGMatrix& target)
{
	if (m_vega_path && m_vega_path_length > 0)
	{
		if (distance > GetLength())
			distance = GetLength();

		FlattenedPathIndex fpi;
		VEGA_FIX where = VEGA_INTTOFIX(0);

		if (!DistanceToLineIndex(distance, fpi))
		{
			fpi.index = m_vega_path->getNumLines() - 1;
			where = VEGA_INTTOFIX(1);
		}
		else
		{
			VEGA_FIX linelength = m_vega_path->getLineLength(fpi.index);
			SVGNumber to_pathlength_scalefactor = m_path_length / m_vega_path_length;
			if (to_pathlength_scalefactor.NotEqual(1))
			{
				VEGA_FIX to_pathlen_sf = to_pathlength_scalefactor.GetVegaNumber();
				linelength = VEGA_FIXMUL(linelength, to_pathlen_sf);
			}
			where = VEGA_FIXDIV(((distance).GetVegaNumber() - fpi.accumulated_length), linelength);
		}

		VEGA_FIX *line = m_vega_path->getLine(fpi.index);

		if (rotate)
			target.LoadRotation(CalculateAngleFromRotate(rotate, line) * 180 / SVGNumber::pi());
		else
			target.LoadIdentity();

		SVGNumber tx, ty;
		tx.SetVegaNumber(line[VEGALINE_STARTX] + VEGA_FIXMUL(where, (line[VEGALINE_ENDX] - line[VEGALINE_STARTX])));
		ty.SetVegaNumber(line[VEGALINE_STARTY] + VEGA_FIXMUL(where, (line[VEGALINE_ENDY] - line[VEGALINE_STARTY])));

		target.MultTranslation(tx, ty);
	}
	else if (m_path)
	{
		/* zero-length path */
		PathSegListIterator* iter = m_path->GetPathIterator(TRUE);
		const SVGPathSeg* seg = NULL;
		if (iter)
		{
			seg = iter->GetNext();
			OP_DELETE(iter);
		}
		if (seg)
		{
			target.LoadTranslation(seg->x, seg->y);
		}
		else
		{
			target.LoadIdentity();
		}
	}
	else
	{
		/* fall-back */
		target.LoadIdentity();
	}

#if 0
	// This code potentially be used for handling of transforms on 'mpath'
	if(m_transform && !m_pre_apply_transform)
	{
		SVGMatrix m;
		m_transform->GetMatrix(m);
		target.Multiply(m);
	}
#endif
}

OP_STATUS
SVGMotionPath::GetCurrentTransformationValue(PositionDescriptor& pos,
											 SVGMatrix &matrix)
{
	SVGNumber distance = 0;
	RETURN_IF_ERROR(CalculateCurrentDistanceAlongPath(pos, distance));
	CalculateTransformAtDistance(distance, pos.rotate, matrix);
	return OpStatus::OK;
}

SVGNumber
SVGMotionPath::CalculateRotationAtDistance(SVGNumber distance)
{
	if (!m_vega_path || m_vega_path_length <= 0)
		return 0;

	FlattenedPathIndex fpi;
	if (!DistanceToLineIndex(distance, fpi))
		return 0;

	VEGA_FIX *line = m_vega_path->getLine(fpi.index);
	return CalculateAngleOfLine(line);
}

#ifdef SVG_FULL_11
void
SVGMotionPath::CalculateWarpFactors(SVGNumber midpoint, SVGNumber half_width,
									SVGNumber& left_sf, SVGNumber& right_sf)
{
	if (!m_vega_path || m_vega_path_length <= 0)
		return;

	SVGNumber length = GetLength();
	SVGNumber left_pos = midpoint - half_width;
	SVGNumber right_pos = midpoint + half_width;
	if (right_pos > length)
		right_pos = length;
	if (left_pos < 0)
		left_pos = 0;

	OP_ASSERT(left_pos <= midpoint && midpoint <= right_pos);

	SVGNumber theta, theta_l, theta_r;

	FlattenedPathIndex fpi;

	// If one of these fail, all the following ones should also fail,
	// but since the left and right edges have been clipped to path
	// length (and the midpoint was checked further up the
	// call-chain), it would seem strange for any of them to fail...
	if (DistanceToLineIndex(left_pos, fpi))
	{
		VEGA_FIX *line = m_vega_path->getLine(fpi.index);

		theta_l = CalculateAngleOfLine(line);
	}
	if (DistanceToLineIndex(midpoint, fpi))
	{
		VEGA_FIX *line = m_vega_path->getLine(fpi.index);

		theta = CalculateAngleOfLine(line);
	}
	if (DistanceToLineIndex(right_pos, fpi))
	{
		VEGA_FIX *line = m_vega_path->getLine(fpi.index);

		theta_r = CalculateAngleOfLine(line);
	}

	theta_l -= theta;
	theta_r -= theta;

 	left_sf = theta_l.tan();
 	right_sf = theta_r.tan();
}

OP_STATUS SVGMotionPath::Warp(const OpBpath* inpath, OpBpath** outpath,
							  SVGNumber start_dist, SVGNumber ctpexp, SVGNumber half_width)
{
	OP_ASSERT(outpath);

	OpBpath* bpath;

#ifdef SVG_DEBUG_DONT_WARP
	RETURN_IF_ERROR(OpBpath::Make(bpath, FALSE));
	*outpath = bpath;
	return bpath->Copy(inpath);
#else // !SVG_DEBUG_DONT_WARP
	RETURN_IF_ERROR(inpath->Bezierize(&bpath));

 	SVGNumber left_sf;
 	SVGNumber right_sf;
	CalculateWarpFactors(start_dist, half_width, left_sf, right_sf);

	half_width /= ctpexp; // half_width is transformed from path coordinates to glyph coordinates
	SVGNumber half_width_recip = SVGNumber(1) / half_width;

	PathSegListIterator* iter = bpath->GetPathIterator(TRUE); /* normalized */
	if (!iter)
		return OpStatus::ERR_NO_MEMORY;

	unsigned int count = bpath->GetCount();
	for (unsigned int i = 0; i < count; i++)
	{
		const SVGPathSeg* cmd = iter->GetNext();
		if (!cmd)
			continue;

		SVGPathSeg new_cmd = *cmd;

		// if this assert triggers then it's time to fix Bezierize so that it returns proper pathsegs.
		OP_ASSERT(cmd->info.type == SVGPathSeg::SVGP_CLOSE ||
				  cmd->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_ABS ||
				  cmd->info.type == SVGPathSeg::SVGP_MOVETO_ABS);

		if (cmd->info.type == SVGPathSeg::SVGP_CURVETO_CUBIC_ABS ||
			cmd->info.type == SVGPathSeg::SVGP_MOVETO_ABS)
		{
			SVGNumber dx = (half_width - new_cmd.x).abs() * half_width_recip * new_cmd.y;
			if (new_cmd.x < half_width)
				dx *= left_sf;
			else
				dx *= right_sf;
			new_cmd.x = new_cmd.x + dx;

			if (cmd->info.type != SVGPathSeg::SVGP_MOVETO_ABS)
			{
				SVGNumber dx = (half_width - new_cmd.x1).abs() * half_width_recip * new_cmd.y1;
				if (new_cmd.x1 < half_width)
					dx *= left_sf;
				else
					dx *= right_sf;
				new_cmd.x1 = new_cmd.x1 + dx;

				dx = (half_width - new_cmd.x2).abs() * half_width_recip * new_cmd.y2;
				if (new_cmd.x2 < half_width)
					dx *= left_sf;
				else
					dx *= right_sf;
				new_cmd.x2 = new_cmd.x2 + dx;
			}
		}

		bpath->Set(i, new_cmd); // Should not incur length modifications
	}
	OP_DELETE(iter);

	*outpath = bpath;
	return OpStatus::OK;
#endif // !SVG_DEBUG_DONT_WARP
}
#endif // SVG_FULL_11
#endif // SVG_SUPPORT
