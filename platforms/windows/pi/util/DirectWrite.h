/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Øyvind Østlund
**
*/

#ifndef DIRECT_WRITE_H
#define DIRECT_WRITE_H

#include <dwrite.h>

/*
 *  Convert an Opera font weight to a DirectWrite font weight.
 * 
 *  @weight A number from 1 (thin) to 9 (bold), where 4 is normal.
 *          Anything outside that is not a valid font weight.
 *	@return The corresponding DirectWrite weight.
 */

const DWRITE_FONT_WEIGHT ToDWriteWeight(const UINT8 weight);

/*
 *  Convert a DirectWrite font weight to an Opera font weight.
 * 
 *  @dw_weight A number from 100 (thin) to 950 (bold), where 400 is normal.
 *             Anything outside that is not a valid font weight.
 *	@return    The corresponding value Opera use for the same weight.
 */

const UINT8 ToFontWeight(const DWRITE_FONT_WEIGHT dw_weight);


//
// DirectWrite helper classes/structs
//

struct DWriteGlyphRun
{
	UINT					len;		// The length of the current glyph run.
	UINT					size;		// The max size that indices/advances/offsets can hold.
	UINT16*					indices;	// The runs glyph indices.
	FLOAT*					advances;	// The runs glyph advances.
	DWRITE_GLYPH_OFFSET*	offsets;	// The runs glyph offsets.
	float					width;		// The calculated string width.

	void Delete();

	OP_STATUS Append(DWriteGlyphRun* run);
	OP_STATUS Resize(UINT new_size, bool preserve);
};


/*
 * DWriteString
 * 
 * - The str part should not always be deleted (Be carefull).
 * - The run part should most often be delteed. It has it's own delete funcion (Be carefull).
 *
 */

struct DWriteString
{
	uni_char*				str;		// The string that made this glyph run.
	UINT					len;		// The length of the currently stored DWRITE_GLYPH_RUN.
	UINT					size;		// The max size str can hold.
	DWriteGlyphRun			run;		// The indices, advances and offsets of a glyph run.

	OP_STATUS SetString(const uni_char* string, UINT len);
};


/*
 * DWriteOpFontInfo
 * 
 * - Enumerates font family info.
 * - Enumerates font face info.
 * - Can make a OpFontInfo copy of the contained font info.
 *
 */

class DWriteOpFontInfo
{
public:
	DWriteOpFontInfo() : m_weight(0) {}
	~DWriteOpFontInfo() {}

	OP_STATUS InitFontFamilyInfo(IDWriteFontFamily* dw_font_family, IDWriteFont* dw_font);
	OP_STATUS InitFontFaceInfo(IDWriteFontFace* dw_font_face);

	OP_STATUS CreateCopy(OpFontInfo* font_info);

	const uni_char* GetFontName() const;

private:

	OP_STATUS InitFontFamilyName(IDWriteFontFamily* dw_font_family);
	void InitFontStyleInfo(IDWriteFont* dw_font);
	void InitFontClassification(const UINT8* os2_tab, const UINT32 os2_tab_size);
	void InitScriptAndCodepage(const UINT8* os2_tab, const UINT32 os2_tab_size);

	UINT8 m_weight;						// The default font weight for this font.
	OpFontInfo m_font_info;				// All the enumerated font info.
};

#endif DIRECT_WRITE_H