/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANGLE_PARSER_H
#define SVG_ANGLE_PARSER_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/parser/svgtokenizer.h"

class SVGAngleParser
{
public:
    OP_STATUS ParseRotate(const uni_char *input_string, unsigned input_string_length,
						  SVGRotate *&rotate);
	/**< Parses a string for "a rotate" and allocates a SVGRotate to
	   match. The rotate syntax is described in:
	   http://www.w3.org/TR/SVG11/animate.html#RotateAttribute */

    OP_STATUS ParseOrient(const uni_char *input_string, unsigned input_string_length,
						  SVGOrient *&orient);
	/**< Parses a string for "a orient" and allocates a SVGOrient to
	   match. The orient syntax is described in:
	   http://www.w3.org/TR/SVG11/animate.html#RotateAttribute */


private:
	BOOL ScanAngle(double &number, SVGAngleType& unit);

	SVGTokenizer tokenizer;
	OP_STATUS status;
};

#endif // SVG_SUPPORT
#endif // !SVG_ANGLE_PARSER_H


