/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(SVG_SUPPORT) && defined(SVG_DOM) && defined(SVG_FULL_11)

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/svgdom/svgdompathsegimpl.h"

SVGDOMPathSegImpl::SVGDOMPathSegImpl(SVGPathSegObject* seg) :
	m_pathseg(seg)
{
	SVGObject::IncRef(m_pathseg);
}

/* virtual */
SVGDOMPathSegImpl::~SVGDOMPathSegImpl()
{
	SVGObject::DecRef(m_pathseg);
}

/* virtual */ const char*
SVGDOMPathSegImpl::GetDOMName()
{
	switch (GetSegType())
	{
	default:
	case SVG_DOM_PATHSEG_UNKNOWN:
		break;

	case SVG_DOM_PATHSEG_CLOSEPATH:
		return "SVGPathSegClosePath";
	case SVG_DOM_PATHSEG_MOVETO_ABS:
		return "SVGPathSegMovetoAbs";
	case SVG_DOM_PATHSEG_MOVETO_REL:
		return "SVGPathSegMovetoRel";
	case SVG_DOM_PATHSEG_LINETO_ABS:
		return "SVGPathSegLinetoAbs";
	case SVG_DOM_PATHSEG_LINETO_REL:
		return "SVGPathSegLinetoRel";
	case SVG_DOM_PATHSEG_CURVETO_CUBIC_ABS:
		return "SVGPathSegCurvetoCubicAbs";
	case SVG_DOM_PATHSEG_CURVETO_CUBIC_REL:
		return "SVGPathSegCurvetoCubicRel";
	case SVG_DOM_PATHSEG_CURVETO_QUADRATIC_ABS:
		return "SVGPathSegCurvetoQuadraticAbs";
	case SVG_DOM_PATHSEG_CURVETO_QUADRATIC_REL:
		return "SVGPathSegCurvetoQuadraticRel";
	case SVG_DOM_PATHSEG_ARC_ABS:
		return "SVGPathSegArcAbs";
	case SVG_DOM_PATHSEG_ARC_REL:
		return "SVGPathSegArcRel";
	case SVG_DOM_PATHSEG_LINETO_HORIZONTAL_ABS:
		return "SVGPathSegLinetoHorizontalAbs";
	case SVG_DOM_PATHSEG_LINETO_HORIZONTAL_REL:
		return "SVGPathSegLinetoHorizontalRel";
	case SVG_DOM_PATHSEG_LINETO_VERTICAL_ABS:
		return "SVGPathSegLinetoVerticalAbs";
	case SVG_DOM_PATHSEG_LINETO_VERTICAL_REL:
		return "SVGPathSegLinetoVerticalRel";
	case SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
		return "SVGPathSegCurvetoCubicSmoothAbs";
	case SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
		return "SVGPathSegCurvetoCubicSmoothRel";
	case SVG_DOM_PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
		return "SVGPathSegCurvetoQuadraticSmoothAbs";
	case SVG_DOM_PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
		return "SVGPathSegCurvetoQuadraticSmoothRel";
	}
	// Fallback default
	return "SVGPathSeg";
}

/* virtual */ BOOL
SVGDOMPathSegImpl::IsValid(UINT32 idx)
{
	return m_pathseg->IsValid(idx);
}

int
SVGDOMPathSegImpl::GetSegType() const
{
	return m_pathseg->seg.info.type;
}

uni_char
SVGDOMPathSegImpl::GetSegTypeAsLetter() const
{
	return m_pathseg->seg.GetSegTypeAsChar();
}

double
SVGDOMPathSegImpl::GetX()
{
	return m_pathseg->seg.x.GetFloatValue();
}

double
SVGDOMPathSegImpl::GetY()
{
	return m_pathseg->seg.y.GetFloatValue();
}

double
SVGDOMPathSegImpl::GetX1()
{
	return m_pathseg->seg.x1.GetFloatValue();
}

double
SVGDOMPathSegImpl::GetY1()
{
	return m_pathseg->seg.y1.GetFloatValue();
}

double
SVGDOMPathSegImpl::GetX2()
{
	return m_pathseg->seg.x2.GetFloatValue();
}

double
SVGDOMPathSegImpl::GetY2()
{
	return m_pathseg->seg.y2.GetFloatValue();
}

BOOL
SVGDOMPathSegImpl::GetLargeArcFlag()
{
	return m_pathseg->seg.info.large;
}

BOOL
SVGDOMPathSegImpl::GetSweepFlag()
{
	return m_pathseg->seg.info.sweep;
}

OP_BOOLEAN
SVGDOMPathSegImpl::SetX(double val)
{
	RETURN_FALSE_IF(m_pathseg->seg.x == SVGNumber(val));
	m_pathseg->seg.x = val;
	m_pathseg->Sync();
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN
SVGDOMPathSegImpl::SetY(double val)
{
	RETURN_FALSE_IF(m_pathseg->seg.y == SVGNumber(val));
	m_pathseg->seg.y = val;
	m_pathseg->Sync();
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN
SVGDOMPathSegImpl::SetX1(double val)
{
	RETURN_FALSE_IF(m_pathseg->seg.x1 == SVGNumber(val));
	m_pathseg->seg.x1 = val;
	m_pathseg->Sync();
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN
SVGDOMPathSegImpl::SetY1(double val)
{
	RETURN_FALSE_IF(m_pathseg->seg.y1 == SVGNumber(val));
	m_pathseg->seg.y1 = val;
	m_pathseg->Sync();
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN
SVGDOMPathSegImpl::SetX2(double val)
{
	RETURN_FALSE_IF(m_pathseg->seg.x2 == SVGNumber(val));
	m_pathseg->seg.x2 = val;
	m_pathseg->Sync();
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN
SVGDOMPathSegImpl::SetY2(double val)
{
	RETURN_FALSE_IF(m_pathseg->seg.y2 == SVGNumber(val));
	m_pathseg->seg.y2 = val;
	m_pathseg->Sync();
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN
SVGDOMPathSegImpl::SetLargeArcFlag(BOOL large)
{
	RETURN_FALSE_IF(m_pathseg->seg.info.large == (unsigned int)(large ? 1 : 0));
	m_pathseg->seg.info.large = large ? 1 : 0;
	m_pathseg->Sync();
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN
SVGDOMPathSegImpl::SetSweepFlag(BOOL sweep)
{
	RETURN_FALSE_IF(m_pathseg->seg.info.sweep == (unsigned int)(sweep ? 1 : 0));
	m_pathseg->seg.info.sweep = sweep ? 1 : 0;
	m_pathseg->Sync();
	return OpBoolean::IS_TRUE;
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11
