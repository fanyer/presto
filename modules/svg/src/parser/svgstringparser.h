/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_STRING_PARSER_H
#define SVG_STRING_PARSER_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/parser/svgtokenizer.h"

class SVGVector;

class SVGStringParser
{
public:
	OP_STATUS	ParseStringCommaList(const uni_char *input, unsigned str_len, SVGVector &vector);
	/**< In a comma separated list, the commas is always optional. One
	   or more unquoted whitespace suffices as separator */

	OP_STATUS	ParseStringSemicolonList(const uni_char *input, unsigned str_len, SVGVector &vector);

	OP_STATUS	ParseFontFamilyList(const uni_char *input, unsigned str_len, SVGVector &vector);
	/**< A font family list supports quouting of items, and the
	  quoting should be removed in the result. We support the single
	  quotes (') as quotation */
#if 0
	OP_STATUS	ParseQuotedString(const uni_char *input, unsigned input_length, const uni_char *&string, unsigned& string_length);
	/**< Parse a string with support for quotes (') */
#endif // 0

private:

	OP_STATUS	ParseStringList(const uni_char *input, unsigned str_len, SVGTokenizer::StringRules &rules, SVGVector &vector);

	OP_STATUS status;
	SVGTokenizer tokenizer;
};

#endif // SVG_SUPPORT
#endif // !SVG_STRING_PARSER_H




