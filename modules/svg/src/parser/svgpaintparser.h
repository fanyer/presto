/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_PAINT_PARSER_H
#define SVG_PAINT_PARSER_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGPaint.h"
#include "modules/svg/src/parser/svgtokenizer.h"

class SVGPaintParser
{
public:
	enum {
		ALLOW_PERCENTAGE	= 0x01,
		ALLOW_NUMBER		= 0x02
	};
	OP_STATUS ParseColor(const uni_char *input_string, unsigned input_string_length, SVGColor& color);
	OP_STATUS ParsePaint(const uni_char *input_string, unsigned input_string_length, SVGPaint& paint);

private:
	BOOL	ColorChannel(unsigned int& color, unsigned int maxValue = 255, int flags = (ALLOW_PERCENTAGE | ALLOW_NUMBER));
	UINT8	AChannel();
	void	ScanURL(const uni_char *&uri, unsigned &uri_length);
	void	ScanString(const uni_char *&uri, unsigned &uri_length);
	BOOL	ScanColor(SVGColor &color);
	void	ScanBackupPaint(SVGPaint &paint);
	COLORREF ScanHexColor();

	SVGTokenizer tokenizer;
	OP_STATUS status;
};

#endif // SVG_SUPPORT
#endif // !SVG_PAINT_PARSER_H


