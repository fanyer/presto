/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef DEBUGGING_GLYPHS_H
#define DEBUGGING_GLYPHS_H

enum DebuggingGlyphFlags
{
	// shape
	DEBUGGING_GLYPH_FLAG_BOX=0x800, // box or line
	DEBUGGING_GLYPH_FLAG_FILLED=0x400, // filled box or cross-box

	// position
	DEBUGGING_GLYPH_FLAG_TOP=0x80,
	DEBUGGING_GLYPH_FLAG_X_HEIGHT=0x40,
	DEBUGGING_GLYPH_FLAG_HALF_X_HEIGHT=0x20,
	DEBUGGING_GLYPH_FLAG_ASCENT_FIRST=0x10,
	DEBUGGING_GLYPH_FLAG_DESCENT_FIRST=0x8,
	DEBUGGING_GLYPH_FLAG_BOTTOM=0x4,

	// misc
	DEBUGGING_GLYPH_FLAG_PADDING_1=0x2
};

class DebuggingGlyphRenderer
{
public:
	static int Draw(
		class OpPainter *painter, int ascent, int descent, int x, int y,
		uint32_t ch);
	static int GetWidth(
		int ascent, int descent, uint32_t ch);
	static int IsDebuggingGlyph(uint32_t ch) {
		return ch >= 0xe000 && ch <= 0xefff;
	}
};

#endif // DEBUGGING_GLYPHS_H
