/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#include "core/pch.h"

#ifdef PRIVATE_AREA_DEBUGGING_GLYPHS

#include "debugging_glyphs.h"

#include "modules/pi/OpPainter.h"


int DebuggingGlyphRenderer::Draw(
	OpPainter *painter, int ascent, int descent, int x, int y, uint32_t ch)
{
	if (ch < 0xe000 || ch > 0xefff)
		return 0;

	ch -= 0xe000;

	int top = 0;
	int bottom = 0;
	int left = 0;
	int right = 0;

	if (ch & DEBUGGING_GLYPH_FLAG_PADDING_1)
	{
		top ++;
		left ++;
		bottom ++;
		right ++;
	}

	x += left;

	if (ch & DEBUGGING_GLYPH_FLAG_BOX)
	{
		// box

		if (ch & DEBUGGING_GLYPH_FLAG_TOP)
			top += 0;
		else if (ch & DEBUGGING_GLYPH_FLAG_X_HEIGHT)
			top += ascent - ascent * 2 / 3;
		else if (ch & DEBUGGING_GLYPH_FLAG_HALF_X_HEIGHT)
			top += ascent - ascent / 3;
		else if (ch & DEBUGGING_GLYPH_FLAG_ASCENT_FIRST)
			top += ascent - 1;
		else if (ch & DEBUGGING_GLYPH_FLAG_DESCENT_FIRST)
			top += ascent;
		else if (ch & DEBUGGING_GLYPH_FLAG_BOTTOM)
			top += ascent + descent - 1;
		else
			return ascent + descent;

		if (ch & DEBUGGING_GLYPH_FLAG_BOTTOM)
			bottom += 0;
		else if (ch & DEBUGGING_GLYPH_FLAG_DESCENT_FIRST)
			bottom += descent - 1;
		else if (ch & DEBUGGING_GLYPH_FLAG_ASCENT_FIRST)
			bottom += descent;
		else if (ch & DEBUGGING_GLYPH_FLAG_HALF_X_HEIGHT)
			bottom += descent + ascent / 3;
		else if (ch & DEBUGGING_GLYPH_FLAG_X_HEIGHT)
			bottom += descent + ascent * 2 / 3;
		else if (ch & DEBUGGING_GLYPH_FLAG_TOP)
			bottom += ascent + descent - 1;

		y += top;
		int height = ascent + descent - top - bottom;
		int width = ascent + descent - left - right;

		if (ch & DEBUGGING_GLYPH_FLAG_FILLED)
		{
			painter->FillRect(OpRect(x, y, width, height));
		}
		else
		{
			painter->DrawRect(OpRect(x, y, width, height));
			painter->DrawLine(OpPoint(x, y), OpPoint(x+width-1, y+height-1));
			painter->DrawLine(OpPoint(x+width-1, y), OpPoint(x, y+height-1));
		}
	}
	else
	{
		// line(s)

		int width = ascent + descent - left - right;

		if (ch & DEBUGGING_GLYPH_FLAG_TOP)
			painter->DrawLine(OpPoint(x, y), width, TRUE);
		if (ch & DEBUGGING_GLYPH_FLAG_X_HEIGHT)
			painter->DrawLine(OpPoint(x, y+ascent-ascent*2/3), width, TRUE);
		if (ch & DEBUGGING_GLYPH_FLAG_HALF_X_HEIGHT)
			painter->DrawLine(OpPoint(x, y+ascent-ascent/3), width, TRUE);
		if (ch & DEBUGGING_GLYPH_FLAG_ASCENT_FIRST)
			painter->DrawLine(OpPoint(x, y+ascent-1), width, TRUE);
		if (ch & DEBUGGING_GLYPH_FLAG_DESCENT_FIRST)
			painter->DrawLine(OpPoint(x, y+ascent), width, TRUE);
		if (ch & DEBUGGING_GLYPH_FLAG_BOTTOM)
			painter->DrawLine(OpPoint(x, y+ascent+descent-1), width, TRUE);
	}

	return ascent + descent;
}

int DebuggingGlyphRenderer::GetWidth(int ascent, int descent, uint32_t ch)
{
	return ch >= 0xe000 && ch <= 0xefff ? ascent + descent : 0;
}

#endif // PRIVATE_AREA_DEBUGGING_GLYPHS
