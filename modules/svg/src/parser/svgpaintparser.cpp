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

#include "modules/logdoc/htm_lex.h"
#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/parser/svgpaintparser.h"
#include "modules/svg/src/parser/svgstringparser.h"

#include "modules/logdoc/logdoc_util.h"

OP_STATUS
SVGPaintParser::ParseColor(const uni_char *input_string, unsigned input_string_length, SVGColor& color)
{
    status = OpStatus::OK;
    tokenizer.Reset(input_string, input_string_length);
    tokenizer.EatWsp();
    status = ScanColor(color) ? OpStatus::OK : OpStatus::ERR;

    return tokenizer.ReturnStatus(status);
}

COLORREF SVGPaintParser::ScanHexColor()
{
	const uni_char *color_string = tokenizer.CurrentString();
	unsigned c = tokenizer.CurrentCharacter();
	unsigned n = 0;

	while (c >= 'a' && c <= 'f' ||
		   c >= 'A' && c <= 'F' ||
		   c >= '0' && c <= '9')
	{
		c = tokenizer.Shift(n);
	}

	if (n != 3 && n != 6)
		return USE_DEFAULT_COLOR;

	n /= 3; // Length per component

	unsigned r = HexToInt(color_string, n);
	unsigned g = HexToInt(color_string + n, n);
	unsigned b = HexToInt(color_string + 2 * n, n);

	if (n == 1)
		r *= 0x11, g *= 0x11, b *= 0x11;

	return OP_RGB(r, g, b);
}

BOOL
SVGPaintParser::ScanColor(SVGColor &color)
{
	BOOL rgb = tokenizer.Scan("rgb");
	if (rgb || tokenizer.Scan("hsl"))
    {
		BOOL has_alpha = tokenizer.Scan('a');

		if (!tokenizer.Scan('('))
			return FALSE;

		COLORREF cref;
		if (rgb)
		{
			unsigned int r,g,b;

			ColorChannel(r);
			ColorChannel(g);
			ColorChannel(b);

			if (has_alpha)
			{
				UINT8 a = AChannel();
				cref = OP_RGBA(r, g, b, a);
			}
			else
				cref = OP_RGB(r,g,b);
		}
		else // hsl
		{
			unsigned int h,s,l;

			if (!ColorChannel(h, 360, ALLOW_NUMBER) ||
				!ColorChannel(s, 100, ALLOW_PERCENTAGE) ||
				!ColorChannel(l, 100, ALLOW_PERCENTAGE))
				return FALSE;

			if (has_alpha)
			{
				UINT8 a = AChannel();
				cref = HSLA_TO_RGBA((float)h, (float)s, (float)l, (a+0.5f)/255.0f);
			}
			else
				cref = HSLA_TO_RGBA((float)h, (float)s, (float)l);
		}

		color.SetColorType(SVGColor::SVGCOLOR_RGBCOLOR);
		color.SetColorRef(cref);
		return tokenizer.Scan(')');
    }
    else
    {
		COLORREF cref = USE_DEFAULT_COLOR;

		// Hex-form or color name (eg. '#cacaca' or 'black'). No
		// color names contains other letters than alphabetic one
		// (lowercase). Hex-form contains digits and some lower or
		// upper case
		if (tokenizer.Scan('#'))
		{
			cref = ScanHexColor();
		}
		else
		{
			SVGTokenizer::State saved_state = tokenizer.state;

			const uni_char *color_string = tokenizer.CurrentString();
			unsigned c = tokenizer.CurrentCharacter();
			unsigned n = 0;

			while (c >= 'a' && c <= 'z' ||
				   c >= 'A' && c <= 'Z')
			{
				c = tokenizer.Shift(n);
			}

			if (n > 0)
			{
				cref = HTM_Lex::GetColIdxByName(color_string, n);
				if(cref == USE_DEFAULT_COLOR)
				{
					short keyword = CSS_GetKeyword(color_string, n);
					if(CSS_is_ui_color_val(keyword))
					{
						cref = keyword | CSS_COLOR_KEYWORD_TYPE_ui_color;
					}
				}
			}

			if (cref == USE_DEFAULT_COLOR)
			{
				// Rewind and try to match as a (hash-less) hex-color
				tokenizer.state = saved_state;

				cref = ScanHexColor();
			}
		}

		if (cref == USE_DEFAULT_COLOR)
			return FALSE;

		color.SetColorType(SVGColor::SVGCOLOR_RGBCOLOR);
		color.SetColorRef(cref);
		return TRUE;
    }
}

BOOL
SVGPaintParser::ColorChannel(unsigned int& ret, unsigned int maxValue, int flags /* = (ALLOW_PERCENTAGE | ALLOW_NUMBER) */)
{
    double num;
    tokenizer.EatWsp();
    BOOL num_p = tokenizer.ScanNumber(num);
    if (num_p)
    {
		if (tokenizer.Scan('%'))
		{
			if(!(flags & ALLOW_PERCENTAGE))
				return FALSE;
			else
				num = maxValue * num/100.0;
		}
		else if(!(flags & ALLOW_NUMBER))
			return FALSE;

		ret = num > maxValue ? maxValue : (num < 0) ? 0 : (unsigned int)num;
    }
    tokenizer.EatWspSeparatorWsp(',');
    return TRUE;
}

UINT8
SVGPaintParser::AChannel()
{
    UINT8 ret = 0;
    double num;
    tokenizer.EatWsp();
    BOOL num_p = tokenizer.ScanNumber(num);
    if (num_p)
    {
		ret = (num > 1) ? 255 : (num < 0) ? 0 : (UINT8)(num * 255);
    }
    return ret;
}

OP_STATUS
SVGPaintParser::ParsePaint(const uni_char *input_string, unsigned input_string_length,
						   SVGPaint& paint)
{
    if (input_string_length == 4 && uni_strncmp(input_string, "none", 4) == 0)
    {
		paint.SetPaintType(SVGPaint::NONE);
		return OpStatus::OK;
    }
    else if (input_string_length == 12 && uni_strncmp(input_string, "currentColor", 12) == 0)
    {
		paint.SetPaintType(SVGPaint::CURRENT_COLOR);
		return OpStatus::OK;
    }
    else
    {
		SVGColor color;
		const uni_char *url_string;
		unsigned url_string_length;
		status = OpStatus::OK;

		tokenizer.Reset(input_string, input_string_length);
		if (tokenizer.ScanURL(url_string, url_string_length))
		{
			paint.SetPaintType(SVGPaint::URI);
			paint.SetURI(url_string, url_string_length);

			tokenizer.EatWsp();
			if (!tokenizer.IsEnd())
				ScanBackupPaint(paint);
		}
		else if (ScanColor(color))
		{
			paint.SetPaintType(SVGPaint::RGBCOLOR);
			paint.SetColorRef(color.GetColorRef());
		}
		else
		{
			status = OpStatus::ERR;
		}

		return tokenizer.ReturnStatus(status);
    }
}

void
SVGPaintParser::ScanBackupPaint(SVGPaint &paint)
{
    SVGColor color;
    if (tokenizer.Scan("none"))
    {
		paint.SetPaintType(SVGPaint::URI_NONE);
    }
    else if (tokenizer.Scan("currentColor"))
    {
		paint.SetPaintType(SVGPaint::URI_CURRENT_COLOR);
    }
    else if (ScanColor(color))
    {
		paint.SetPaintType(SVGPaint::URI_RGBCOLOR);
		paint.SetColorRef(color.GetColorRef());
    }
    else
    {
		status = OpStatus::ERR;
    }
}

#endif // SVG_SUPPORT
