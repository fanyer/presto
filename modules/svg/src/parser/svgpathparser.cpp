/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/parser/svgpathparser.h"

OP_STATUS
SVGPathParser::ParsePath(const uni_char *input_string, unsigned str_len, OpBpath *path)
{
	tokenizer.Reset(input_string, str_len);
	this->path = path;
	this->status = OpStatus::OK;

    SVGPathSeg seg;
    SVGPathSeg::SVGPathSegType prev_type = SVGPathSeg::SVGP_UNKNOWN;
    while (GetNextSegment(seg, prev_type))
    {
		prev_type = (SVGPathSeg::SVGPathSegType)seg.info.type;
		status = path->AddCopy(seg);
    }

	return tokenizer.ReturnStatus(status);
}

BOOL
SVGPathParser::GetNextSegment(SVGPathSeg &seg, SVGPathSeg::SVGPathSegType prev_path_seg_type)
{
	tokenizer.EatWsp();

    if (tokenizer.IsEnd())
		return FALSE;

    BOOL implicit;
    SVGPathSeg::SVGPathSegType seg_type;
    if (!GetNextCmdType(seg_type, implicit))
		return FALSE;

    if (implicit)
    {
		if (prev_path_seg_type == SVGPathSeg::SVGP_UNKNOWN ||
			prev_path_seg_type == SVGPathSeg::SVGP_CLOSE)
		{
			status = OpStatus::ERR;
			return FALSE;
		}
		else if (prev_path_seg_type == SVGPathSeg::SVGP_MOVETO_ABS)
		{
			seg_type = SVGPathSeg::SVGP_LINETO_ABS;
		}
		else if (prev_path_seg_type == SVGPathSeg::SVGP_MOVETO_REL)
		{
			seg_type = SVGPathSeg::SVGP_LINETO_REL;
		}
		else
		{
			seg_type = prev_path_seg_type;
		}

		seg.info.type = seg_type;
		seg.info.is_explicit = 0;
    }
    else
    {
		seg.info.type = seg_type;
		seg.MakeExplicit();
    }

    OP_ASSERT (seg_type != SVGPathSeg::SVGP_UNKNOWN);

	/* start with clean slate */
	seg.x = 0.0;
	seg.y = 0.0;
	seg.x1 = 0.0;
	seg.y1 = 0.0;
	seg.x2 = 0.0;
	seg.y2 = 0.0;

	tokenizer.EatWsp();

    switch(seg.info.type)
    {
    case SVGPathSeg::SVGP_CURVETO_CUBIC_ABS:
    case SVGPathSeg::SVGP_CURVETO_CUBIC_REL:
		seg.x1 = NextPathNumber();
		seg.y1 = NextPathNumber();
		/* fall-through */
    case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
    case SVGPathSeg::SVGP_CURVETO_QUADRATIC_REL:
    case SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_ABS:
    case SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_REL:
		seg.x2 = NextPathNumber();
		seg.y2 = NextPathNumber();
		/* fall-through */
    case SVGPathSeg::SVGP_LINETO_ABS:
    case SVGPathSeg::SVGP_LINETO_REL:
    case SVGPathSeg::SVGP_MOVETO_ABS:
    case SVGPathSeg::SVGP_MOVETO_REL:
    case SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_ABS:
    case SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_REL:
		seg.x = NextPathNumber();
		seg.y = NextPathNumber();
		break;

    case SVGPathSeg::SVGP_LINETO_VERTICAL_ABS:
    case SVGPathSeg::SVGP_LINETO_VERTICAL_REL:
    case SVGPathSeg::SVGP_LINETO_HORIZONTAL_ABS:
    case SVGPathSeg::SVGP_LINETO_HORIZONTAL_REL:
		seg.y = NextPathNumber();
		break;

    case SVGPathSeg::SVGP_ARC_REL:
    case SVGPathSeg::SVGP_ARC_ABS:
		seg.x1 = NextPathNumber();
		seg.y1 = NextPathNumber();
		seg.x2 = NextPathNumber();
		seg.info.large = GetArcFlag() ? 1 : 0;
		seg.info.sweep = GetArcFlag() ? 1 : 0;
		seg.x = NextPathNumber();
		seg.y = NextPathNumber();
		break;
    case SVGPathSeg::SVGP_CLOSE:
    default:
		break;
    }

    // Aditional switch because that lets us to fall-throughs in the
    // first. This second switch is to convert the path segment into
    // the format AddCopy assumes. The switch could be removed if
    // OpBpath was changed to understand the first format instead.
    switch(seg.info.type)
    {
    case SVGPathSeg::SVGP_LINETO_HORIZONTAL_ABS:
    case SVGPathSeg::SVGP_LINETO_HORIZONTAL_REL:
		seg.x = seg.y;
		seg.y = 0.0;
		break;
    case SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS:
    case SVGPathSeg::SVGP_CURVETO_QUADRATIC_REL:
		seg.x1 = seg.x2;
		seg.y1 = seg.y2;
		seg.x2 = 0.0;
		seg.y2 = 0.0;
		break;
#ifndef SVG_SUPPORT_ELLIPTICAL_ARCS
    case SVGPathSeg::SVGP_ARC_REL:
    case SVGPathSeg::SVGP_ARC_ABS:
		seg.info.type = (seg.info.type == SVGPathSeg::SVGP_ARC_REL) ?
			SVGPathSeg::SVGP_LINETO_REL : SVGPathSeg::SVGP_LINETO_ABS;
		seg.x1 = 0.0;
		seg.y1 = 0.0;
		seg.x2 = 0.0;
		seg.y2 = 0.0;
		break;
#endif // !SVG_SUPPORT_ELLIPTICAL_ARCS
    }

	tokenizer.EatWspSeparatorWsp(',');

    if (OpStatus::IsSuccess(status))
		return TRUE;
    else
		return FALSE;
}

BOOL
SVGPathParser::GetNextCmdType(SVGPathSeg::SVGPathSegType &path_seg_type, BOOL& implicit)
{
	tokenizer.EatWsp();
    unsigned cmd_char = tokenizer.CurrentCharacter();

    switch(cmd_char)
    {
    case 'm': path_seg_type = SVGPathSeg::SVGP_MOVETO_REL; break;
    case 'M': path_seg_type = SVGPathSeg::SVGP_MOVETO_ABS; break;
    case 'l': path_seg_type = SVGPathSeg::SVGP_LINETO_REL; break;
    case 'L': path_seg_type = SVGPathSeg::SVGP_LINETO_ABS; break;
    case 'c': path_seg_type = SVGPathSeg::SVGP_CURVETO_CUBIC_REL; break;
    case 'C': path_seg_type = SVGPathSeg::SVGP_CURVETO_CUBIC_ABS; break;
    case 's': path_seg_type = SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_REL; break;
    case 'S': path_seg_type = SVGPathSeg::SVGP_CURVETO_CUBIC_SMOOTH_ABS; break;
    case 'q': path_seg_type = SVGPathSeg::SVGP_CURVETO_QUADRATIC_REL; break;
    case 'Q': path_seg_type = SVGPathSeg::SVGP_CURVETO_QUADRATIC_ABS; break;
    case 't': path_seg_type = SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_REL; break;
    case 'T': path_seg_type = SVGPathSeg::SVGP_CURVETO_QUADRATIC_SMOOTH_ABS; break;
    case 'z': path_seg_type = SVGPathSeg::SVGP_CLOSE; break;
    case 'Z': path_seg_type = SVGPathSeg::SVGP_CLOSE; break;
    case 'h': path_seg_type = SVGPathSeg::SVGP_LINETO_HORIZONTAL_REL; break;
    case 'H': path_seg_type = SVGPathSeg::SVGP_LINETO_HORIZONTAL_ABS; break;
    case 'v': path_seg_type = SVGPathSeg::SVGP_LINETO_VERTICAL_REL; break;
    case 'V': path_seg_type = SVGPathSeg::SVGP_LINETO_VERTICAL_ABS; break;
    case 'a': path_seg_type = SVGPathSeg::SVGP_ARC_REL; break;
    case 'A': path_seg_type = SVGPathSeg::SVGP_ARC_ABS; break;
    default:
		path_seg_type = SVGPathSeg::SVGP_UNKNOWN;
		implicit = ((cmd_char >= '0' && cmd_char <= '9') ||
					cmd_char == '-' || cmd_char == '.' || cmd_char == '+');
    }

    if (path_seg_type == SVGPathSeg::SVGP_UNKNOWN)
    {
		return implicit;
    }
    else
    {
		tokenizer.Shift();
		implicit = FALSE;
		return TRUE;
    }
}

BOOL
SVGPathParser::GetArcFlag()
{
    if (tokenizer.IsEnd())
	{
		status = OpStatus::ERR;
		return FALSE;
	}

	BOOL flag = FALSE;
	unsigned c = tokenizer.CurrentCharacter();
	if (c == '1')
	{
		tokenizer.Shift();
		flag = TRUE;
	}
	else if (c == '0')
	{
		tokenizer.Shift();
	}
	else
	{
		status = OpStatus::ERR;
	}

	tokenizer.EatWspSeparatorWsp(',');

    return flag;
}

double
SVGPathParser::NextPathNumber()
{
	double d;
	if (tokenizer.ScanNumber(d))
	{
 		tokenizer.EatWspSeparatorWsp(',');
		return d;
	}
	else
	{
		status = OpStatus::ERR;
		return 0.0;
	}
}

#endif // SVG_SUPPORT
