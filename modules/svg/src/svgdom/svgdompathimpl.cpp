/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(SVG_SUPPORT) && defined(SVG_DOM) && defined(SVG_TINY_12)

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/svgdom/svgdompathimpl.h"

SVGDOMPathImpl::SVGDOMPathImpl(OpBpath* path) :
	m_path(path)
{
	SVGObject::IncRef(m_path);
}

/* virtual */
SVGDOMPathImpl::~SVGDOMPathImpl()
{
	SVGObject::DecRef(m_path);
}

/* virtual */ const char*
SVGDOMPathImpl::GetDOMName()
{
	return "SVGPath";
}

/* virtual */ OP_STATUS 
SVGDOMPathImpl::GetSegment(unsigned long cmdIndex, short& outsegment)
{
	SVGPathSeg* seg = m_path->GetPathSeg(cmdIndex);
	if(seg)
	{
		switch(seg->info.type)
		{
		case SVGPathSeg::SVGP_CLOSE:
			outsegment = SVGDOMPath::SVGPATH_CLOSE;
			return OpStatus::OK;
		case SVGPathSeg::SVGP_MOVETO_ABS:
			outsegment = SVGDOMPath::SVGPATH_MOVETO;
			return OpStatus::OK;
		case SVGPathSeg::SVGP_LINETO_ABS:
			outsegment = SVGDOMPath::SVGPATH_LINETO;
			return OpStatus::OK;
		case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
			outsegment = SVGDOMPath::SVGPATH_CURVETO;
			return OpStatus::OK;
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
			outsegment = SVGDOMPath::SVGPATH_QUADTO;
			return OpStatus::OK;
		default:
			OP_ASSERT(!"This segment type is not supported in normalized lists. Who put it here?");
			break;
		}
	}
	
	return OpStatus::ERR;
}

/* virtual */ OP_STATUS 
SVGDOMPathImpl::GetSegmentParam(unsigned long cmdIndex, unsigned long paramIndex, double& outparam)
{
	SVGPathSeg* seg = m_path->GetPathSeg(cmdIndex);
	if(seg)
	{
		switch(seg->info.type)
		{
		case SVGPathSeg::SVGP_LINETO_ABS:
		case SVGPathSeg::SVGP_MOVETO_ABS:
			if(paramIndex == 0)
			{
				outparam = seg->x.GetFloatValue();
				return OpStatus::OK;
			}
			else if(paramIndex == 1)
			{
				outparam = seg->y.GetFloatValue();
				return OpStatus::OK;
			}
			break;
		case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
			switch(paramIndex)
			{
				case 0:
					outparam = seg->x1.GetFloatValue();
					return OpStatus::OK;
				case 1:
					outparam = seg->y1.GetFloatValue();
					return OpStatus::OK;
				case 2:
					outparam = seg->x2.GetFloatValue();
					return OpStatus::OK;
				case 3:
					outparam = seg->y2.GetFloatValue();
					return OpStatus::OK;
				case 4:
					outparam = seg->x.GetFloatValue();
					return OpStatus::OK;
				case 5:
					outparam = seg->y.GetFloatValue();
					return OpStatus::OK;
				default:
					break;
			}
			break;
		case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
			switch(paramIndex)
			{
				case 0:
					outparam = seg->x1.GetFloatValue();
					return OpStatus::OK;
				case 1:
					outparam = seg->y1.GetFloatValue();
					return OpStatus::OK;
				case 2:
					outparam = seg->x.GetFloatValue();
					return OpStatus::OK;
				case 3:
					outparam = seg->y.GetFloatValue();
					return OpStatus::OK;
				default:
					break;
			}
			break;
		case SVGPathSeg::SVGP_CLOSE:
			break;
		default:
			OP_ASSERT(!"This segment type is not supported in normalized lists. Who put it here?");
			break;
		}
	}

	return OpStatus::ERR;
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_TINY_12
