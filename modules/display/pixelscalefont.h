/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef PIXELSCALEFONT_H
#define PIXELSCALEFONT_H

#if defined(PIXEL_SCALE_RENDERING_SUPPORT) && defined(VEGA_NO_FONTMANAGER_CREATE) && !defined(VEGA_NATIVE_FONT_SUPPORT)

#include "modules/libvega/src/oppainter/vegaopfont.h"
#include "modules/libvega/src/oppainter/vegamdefont.h"


/**
 * PixelScaleFont is a wrapper of VEGAOpFont, provides
 * the proper VEGAMDEFont based on the scale set via SetGlyphScale().
 */
class PixelScaleFont: public VEGAOpFont
{
	friend class PixelScaleOpFontMgr;

private:
	// current scale, will affect glyph
	int curscale;

	// needed to create the scaled font
	int size;
	BOOL outline;
	OpFontManager::OpWebFontRef webfont;
	UINT8 weight;

	class Item: public ListElement<Item>
	{
	public:
		int scale;
		VEGAFont* vegafont;
		Item() : scale(100), vegafont(NULL) {}
	};
	List<Item> fontlist;

public:
	PixelScaleFont(VEGAFont* fnt, OpFontInfo::FontType type = OpFontInfo::PLATFORM);
	virtual ~PixelScaleFont();

	virtual VEGAFont* getVegaFont();

	/**
	 * Set the scale of glyph for rendering, getVegaFont() will then
	 * return the best fit.
	 *
	 * @param scale the glyph scale, in percentage
	 * @return the old scale value
	 */
	int SetGlyphScale(int scale);

protected:
	VEGAFont* CreateScaledFont(int scale);
};


/**
 * PixelScaleOpFontMgr is a wrapper of VEGAMDFOpFontManager used to
 * create PixelScaleFont.
 *
 * VEGA_NO_FONTMANAGER_CREATE must be defined to use this, also conflict
 * with VEGA_NATIVE_FONT_SUPPORT.
 */
class PixelScaleOpFontMgr: public VEGAMDFOpFontManager
{
public:
	OpFont* CreateFont(const uni_char* face, UINT32 size, UINT8 weight,
					   BOOL italic, BOOL must_have_getoutline, INT32 blur_radius);
	OpFont* CreateFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius);
	OpFont* CreateFont(OpWebFontRef webfont, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius);
};

#endif // PIXEL_SCALE_RENDERING_SUPPORT && VEGA_NO_FONTMANAGER_CREATE && !VEGA_NATIVE_FONT_SUPPORT

#endif // PIXELSCALEFONT_H
