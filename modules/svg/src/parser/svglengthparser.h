/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_LENGTH_PARSER_H
#define SVG_LENGTH_PARSER_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGLength.h"
#include "modules/svg/src/parser/svgtokenizer.h"
#include "modules/svg/src/SVGValue.h"

class SVGVector;
class SVGFontSizeObject;

class SVGLengthParser
{
public:
	OP_STATUS ParseLength(const uni_char *input, unsigned input_length,
						  SVGLengthObject *&length);
	OP_STATUS ParseLengthList(const uni_char *input, unsigned input_length,
							  SVGVector &vector);
	OP_STATUS ParseBaselineShift(const uni_char *input, unsigned input_length,
								 SVGBaselineShiftObject *&bls_object);
	OP_STATUS ParseFontsize(const uni_char *input, unsigned input_length,
							SVGFontSizeObject *&fontsize_object);
	OP_STATUS ParseCoordinatePair(const uni_char *input, unsigned input_length,
								  SVGLength &coordinate_x, SVGLength &coordinate_y);
	OP_STATUS ParseClipShape(const uni_char *input, unsigned input_length,
							 SVGRectObject*& shape);

private:
	BOOL ScanLength(SVGLengthObject *&length);
	BOOL ScanLength(SVGLength &length);
	void ScanCoordinatePair(SVGLength &coordinate_x, SVGLength &coordinate_y);

	SVGTokenizer tokenizer;
    OP_STATUS status;
};

#endif // SVG_SUPPORT
#endif // !SVG_LENGTH_PARSER_H

