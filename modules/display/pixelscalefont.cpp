/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#if defined(PIXEL_SCALE_RENDERING_SUPPORT) && defined(VEGA_NO_FONTMANAGER_CREATE) && !defined(VEGA_NATIVE_FONT_SUPPORT)

#include "modules/display/pixelscalefont.h"


PixelScaleFont::PixelScaleFont(VEGAFont* fnt, OpFontInfo::FontType type)
: VEGAOpFont(fnt, type), curscale(100)
, size(-1), outline(TRUE)
, webfont(OpFontManager::OpWebFontRef(0))
, weight((UINT8)~0u)
{

}

PixelScaleFont::~PixelScaleFont()
{
	Item* item = fontlist.First();
	while (item)
	{
		Item* tmp = item;
		item = item->Suc();

		tmp->Out();
		OP_DELETE(tmp->vegafont);
		OP_DELETE(tmp);
	}
}

VEGAFont* PixelScaleFont::getVegaFont()
{
	if (curscale == 100)
		return fontImpl;
	else if (!fontlist.Empty() && fontlist.First()->scale == curscale)
		return fontlist.First()->vegafont;
	else
		return NULL;
}

int PixelScaleFont::SetGlyphScale(int scale)
{
	int oldscale = curscale;

	if (curscale == scale)
	{
		return curscale;
	}
	else if (scale == 100)
	{
		curscale = scale;
		return oldscale;
	}

	// Search in the font list
	Item* fontitem = fontlist.First();
	while (fontitem)
	{
		if (fontitem->scale == scale)
			break;

		fontitem = fontitem->Suc();
	}

	// Hit, make it the head of the list
	if (fontitem)
	{
		fontitem->Out();
	}
	else
	{
		// No luck, create a new one
		fontitem = OP_NEW(Item, ());
		if (!fontitem)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		VEGAFont* font = CreateScaledFont(scale);
		if (!font)
		{
			OP_DELETE(fontitem);
			return OpStatus::ERR_NO_MEMORY;
		}

		fontitem->vegafont = font;
		fontitem->scale = scale;
	}

	// Add to cache as the first element
	curscale = scale;
	fontitem->IntoStart(&fontlist);

	return oldscale;
}

VEGAFont* PixelScaleFont::CreateScaledFont(int scale)
{
	const uni_char* face = fontImpl->getFontName();
	int sz = size * scale / 100;
	int blur_radius = fontImpl->getBlurRadius()* scale / 100;
	BOOL italic = fontImpl->isItalic();

	VEGAOpFontManager* fontmgr = static_cast<VEGAOpFontManager*>(styleManager->GetFontManager());
	OP_ASSERT(fontmgr);

	VEGAFont* vegafont = NULL;
	if (webfont)
	{
		if (weight != (UINT8)~0u)
		{
			vegafont = static_cast<VEGAFont*>(fontmgr->GetVegaFont(webfont, weight, italic, sz, blur_radius));
		}
		else
		{
			vegafont = static_cast<VEGAFont*>(fontmgr->GetVegaFont(webfont, sz, blur_radius));
		}
	}
	else
	{
		vegafont = static_cast<VEGAFont*>(fontmgr->GetVegaFont(face, sz, weight, italic, outline, blur_radius));
	}

	return vegafont;
}


OpFont* PixelScaleOpFontMgr::CreateFont(const uni_char* face, UINT32 size, UINT8 weight,
										BOOL italic, BOOL must_have_getoutline, INT32 blur_radius)
{
	VEGAFont* fnt = GetVegaFont(face, size, weight, italic, must_have_getoutline, blur_radius);
	if (!fnt)
		return NULL;

	PixelScaleFont* opfnt  = OP_NEW(PixelScaleFont, (fnt));
	if (!opfnt)
	{
		OP_DELETE(fnt);
		return NULL;
	}

	opfnt->size = size;
	opfnt->outline = must_have_getoutline;
	opfnt->weight = weight;

	return opfnt;
}

OpFont* PixelScaleOpFontMgr::CreateFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius)
{
	VEGAFont* fnt = GetVegaFont(webfont, size, blur_radius);
	if (!fnt)
		return NULL;

	OpFontInfo::FontType type = GetWebFontType(webfont);

	PixelScaleFont* opfnt  = OP_NEW(PixelScaleFont, (fnt, type));
	if (!opfnt)
	{
		OP_DELETE(fnt);
		return NULL;
	}

	opfnt->size = size;
	opfnt->webfont = webfont;

	return opfnt;
}

OpFont* PixelScaleOpFontMgr::CreateFont( OpWebFontRef webfont, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius )
{
	VEGAFont* fnt = GetVegaFont(webfont, weight, italic, size, blur_radius);
	if (!fnt)
		return NULL;

	OpFontInfo::FontType type = GetWebFontType(webfont);

	PixelScaleFont* opfnt  = OP_NEW(PixelScaleFont, (fnt, type));
	if (!opfnt)
	{
		OP_DELETE(fnt);
		return NULL;
	}

	opfnt->size = size;
	opfnt->webfont = webfont;
	opfnt->weight = weight;

	return opfnt;
}

#endif // PIXEL_SCALE_RENDERING_SUPPORT && VEGA_NO_FONTMANAGER_CREATE && !VEGA_NATIVE_FONT_SUPPORT
