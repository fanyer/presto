/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/parser/svgangleparser.h"
#include "modules/svg/src/SVGValue.h"

BOOL
SVGAngleParser::ScanAngle(double &number, SVGAngleType &unit)
{
    tokenizer.EatWsp();

    if (tokenizer.IsEnd())
		return FALSE;

    if (tokenizer.ScanNumber(number))
    {
		tokenizer.EatWsp();

		if(tokenizer.Scan("rad"))
		{
			unit = SVGANGLE_RAD;
		}
		else if(tokenizer.Scan("grad"))
		{
			unit = SVGANGLE_GRAD;
		}
		else if (tokenizer.Scan("deg"))
		{
			unit = SVGANGLE_DEG;
		}
		else
		{
			unit = SVGANGLE_UNSPECIFIED;
		}

		return TRUE;
    }

    return FALSE;
}

OP_STATUS
SVGAngleParser::ParseRotate(const uni_char *input_string, unsigned input_string_length,
							SVGRotate*& rotate)
{
    OP_STATUS status = OpStatus::OK;
    tokenizer.Reset(input_string, input_string_length);

    double number;
    SVGAngleType angle_type;
    SVGRotateType rotate_type = SVGROTATE_UNKNOWN;

    SVGAngle* angle = NULL;
    rotate = NULL;

    tokenizer.EatWsp();

    if (tokenizer.Scan("auto-reverse"))
    {
		rotate_type = SVGROTATE_AUTOREVERSE;
    }
    else if (tokenizer.Scan("auto"))
    {
		rotate_type = SVGROTATE_AUTO;
    }
    else if (ScanAngle(number, angle_type))
    {
		rotate_type = SVGROTATE_ANGLE;
		angle = OP_NEW(SVGAngle, (SVGNumber(number), angle_type));
		if (!angle_type)
			status = OpStatus::ERR_NO_MEMORY;
    }
    else
    {
		status = OpStatus::ERR;
    }

    if (OpStatus::IsSuccess(status))
    {
		OP_ASSERT(rotate_type != SVGROTATE_UNKNOWN);
		if (rotate_type == SVGROTATE_AUTO ||
			rotate_type == SVGROTATE_AUTOREVERSE)
		{
			rotate = OP_NEW(SVGRotate, (rotate_type));
		}
		else
		{
			OP_ASSERT(angle != NULL);
			rotate = OP_NEW(SVGRotate, (angle));
		}

		if (!rotate)
		{
			status = OpStatus::ERR_NO_MEMORY;
			OP_DELETE(angle); // We may have allocated an angle
		}
    }

    return tokenizer.ReturnStatus(status);
}

OP_STATUS
SVGAngleParser::ParseOrient(const uni_char *input_string, unsigned input_string_length,
							SVGOrient*& orient)
{
    OP_STATUS status = OpStatus::OK;
    tokenizer.Reset(input_string, input_string_length);

    double number;
    SVGAngleType angle_type;
    SVGOrientType orient_type = SVGORIENT_UNKNOWN;

    SVGAngle* angle = NULL;
    orient = NULL;

    tokenizer.EatWsp();

    if (tokenizer.Scan("auto"))
    {
		orient_type = SVGORIENT_AUTO;
    }
    else if (ScanAngle(number, angle_type))
    {
		orient_type = SVGORIENT_ANGLE;
		angle = OP_NEW(SVGAngle, (SVGNumber(number), angle_type));
		if (!angle_type)
			status = OpStatus::ERR_NO_MEMORY;
    }
    else
    {
		status = OpStatus::ERR;
    }

    if (OpStatus::IsSuccess(status))
    {
		OP_ASSERT(orient_type != SVGORIENT_UNKNOWN);
		OP_ASSERT((orient_type == SVGORIENT_AUTO && angle == NULL) ||
				  (orient_type == SVGORIENT_ANGLE && angle));

		SVGOrient::Make(orient, orient_type, angle);

		if (!orient)
		{
			status = OpStatus::ERR_NO_MEMORY;
			OP_DELETE(angle); // We may have allocated an angle
		}
    }

    return tokenizer.ReturnStatus(status);
}

#endif // SVG_SUPPORT
