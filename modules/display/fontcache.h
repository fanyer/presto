/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file fontcache.h
 *
 * FontCache class prototype.
 *
 * @author Markus Johansson
 *
 */

#ifndef _FONTCACHE_H_
#define _FONTCACHE_H_

# include "modules/util/simset.h"

#include "modules/pi/OpFont.h"

#include "modules/display/FontAtt.h"

class FramesDocument;
class Window;

/**
 * FontCacheElement's are stored in the font cache.
 * Every element has a reference counter and a timestamp for the last use.
 */
class FontCacheElement : public Link
{
public:
	FontCacheElement(OpFont* font, const FontAtt &fontatt, UINT32 time, UINT32 scale, Window* window, FramesDocument* doc) :
		font(font),
		last_use(time),
		ref_count(1),
		packed_init(0),
		doc(doc)
#ifdef FONTCACHE_PER_WINDOW
		,window(window)
#endif // FONTCACHE_PER_WINDOW
	{
		this->fontatt = fontatt;
		packed.scale = scale;
	}

#ifdef SVG_SUPPORT
	~FontCacheElement();
#else
	~FontCacheElement() { OP_DELETE(font); }
#endif // SVG_SUPPORT

	OpFont* font;
	FontAtt fontatt;
	UINT32 last_use;
	UINT32 ref_count;

	union
	{
		struct
		{
			unsigned int scale:31;
			unsigned int is_marked_for_purge:1;
		} packed;
		unsigned int packed_init;
	};
	FramesDocument* doc;
#ifdef FONTCACHE_PER_WINDOW
	Window* window;
#endif // FONTCACHE_PER_WINDOW
};

/**
 * FontCache. This is a cache of created OpFont-objects.
 * The size of the cache is tweakable, and it can also be tweaked so there is one cache per window,
 * instead of one global cache.
 */
class FontCache
{
public:
	/**
	 * Constructs a new font cache.
	 * The size will be set to FONTCACHESIZE.
	 */
	FontCache() : time(0), num_cached_fonts(0) {}

	/**
	 * Destroys the font cache.
	 */
	~FontCache();

	/**
	 * Clears out all unused fonts from the cache.
	 */
	void Clear();

	/**
	* Clears out all unused svgfonts from the cache.
	*/
	void ClearSVGFonts();

	/**
	 * Gets a font from the font cache.
	 * If the font is not in the cache it will be
	 * created. When creating a font and there is
	 * no space left in the cache, the font which was used
	 * least recently will be thrown out.
	 * When you are done using the font returned from
	 * this function, invoke ReleaseFont().
	 * @param fontatt Attributes for requested font.
	 * @param scale Scale used to create this font.
	 * @param doc The current FramesDocument (only used for creating SVGFonts).
	 * @return An OpFont object.
	 */
	OpFont* GetFont(FontAtt &fontatt, UINT32 scale, FramesDocument* doc = NULL);

	/**
	   increases the ref count of font (assumed to be in cache) by
	   one. this function serves as an alternative to GetFont, taking
	   an already fetched OpFont instead of a FontAtt, and is needed
	   by the printing code since it's passed an OpFont that might be
	   accessed later. releasing the font is done by invoking
	   ReleaseFont.

	   @param font the font for which to increase the reference count - should be in cache
	 */
	void ReferenceFont(OpFont* font);

	/**
	 * Releases a font.
	 * Call this method when you are not going to use
	 * the font anymore.
	 * @param font Font to release.
	 */
	void ReleaseFont(OpFont* font, BOOL force_total_removal = FALSE);

	/**
	 * Query the cache for any existing font instances with the specified fontnumber
	 *
	 * @param font_num Font number to query
	 * @return TRUE if instance with said fontnumber exists, otherwise FALSE
	 */
	BOOL HasCachedInstance(UINT32 font_num);

	/**
	 * Purge the instance(s) of a webfont with the specified fontnumber
	 *
	 * @param font_num Font number to query
	 * @ return TRUE if all matching entries were removed, FALSE otherwise
	 */
	BOOL PurgeWebFont(UINT32 font_num);

#ifdef FONTCACHE_PER_WINDOW
	/**
	 * Clears out all unreferenced fonts associated with a given window.
	 * @param window The Window for which fonts are to be cleared.
	 */
	void ClearForWindow(Window *window);
#endif // FONTCACHE_PER_WINDOW

private:
	UINT32 time;
	Head cached_fonts;
	unsigned int num_cached_fonts;
	void DeleteFontCacheElement(FontCacheElement* font_elm);
};

#endif // _FONTCACHE_H_
