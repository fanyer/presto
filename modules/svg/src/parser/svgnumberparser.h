/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_NUMBER_PARSER_H
#define SVG_NUMBER_PARSER_H

#ifdef SVG_SUPPORT

#include "modules/svg/svg_number.h"
#include "modules/svg/src/parser/svgtokenizer.h"
#include "modules/svg/src/SVGVector.h"

class SVGNumberParser
{
public:
	OP_STATUS ParseNumber(const uni_char *input, unsigned str_len, BOOL normalize_percentage, SVGNumber &num);
	OP_STATUS ParseNumber(const uni_char *input, unsigned str_len, SVGNumber &num) { return ParseNumber(input, str_len, FALSE, num); }
	OP_STATUS ParseNumberCommaList(const uni_char *input, unsigned str_len, SVGVector &vector);
	OP_STATUS ParseNumberSemicolonList(const uni_char *input, unsigned str_len, SVGVector &vector);

	OP_STATUS ParsePointList(const uni_char *input, unsigned str_len, SVGVector &vector);
	/**< A point list is a list with semi-colon separated coordinate
	 * pairs. The cooridinates are two colon and/or whitespace
	 * separated numbers.
	 */

	OP_STATUS ParseUnicodeRange(const uni_char* input, unsigned str_len, unsigned& uc_start, unsigned& uc_end);

private:
	BOOL ScanUnicodeRange(unsigned& uc_start, unsigned& uc_end);

	OP_STATUS ParseNumberList(const uni_char *input, unsigned str_len, uni_char separator, SVGVector &vector);

	SVGTokenizer tokenizer;
};

#endif // SVG_SUPPORT
#endif // !SVG_NUMBER_PARSER_H


