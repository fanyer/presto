/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file fontcache.cpp
 *
 * FontCache class implementation.
 *
 * @author Markus Johansson
 *
 */

#include "core/pch.h"

#include "modules/display/fontcache.h"

#include "modules/hardcore/mem/mem_man.h"

#include "modules/pi/OpFont.h"

#include "modules/display/styl_man.h"
#include "modules/doc/frm_doc.h"

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
#endif // SVG_SUPPORT

FontCache::~FontCache()
{
	Clear();

	// We shouldn't have any cached fonts now.

	OP_ASSERT(cached_fonts.Empty());

	// Release cached fonts. Happens when the assert above fails.
	cached_fonts.Clear();
}

void
FontCache::Clear()
{
	FontCacheElement* font_elm = (FontCacheElement*)cached_fonts.First();
	FontCacheElement* next = NULL;
	while (font_elm)
	{
		next = (FontCacheElement*)font_elm->Suc();

		if (font_elm->ref_count == 0)
		{
			DeleteFontCacheElement(font_elm);
		}

		font_elm = next;
	}
}

void
FontCache::ClearSVGFonts()
{
#ifdef SVG_SUPPORT
	FontCacheElement* font_elm = (FontCacheElement*)cached_fonts.First();
	FontCacheElement* next = NULL;
	while (font_elm)
	{
		next = (FontCacheElement*)font_elm->Suc();

		if(font_elm->font->Type() == OpFontInfo::SVG_WEBFONT)
		{
			if (font_elm->ref_count == 0)
			{
				DeleteFontCacheElement(font_elm);
			}
		}
		font_elm = next;
	}
#endif // SVG_SUPPORT
}

void FontCache::DeleteFontCacheElement(FontCacheElement* font_elm)
{
	OP_ASSERT(font_elm->ref_count == 0);
	font_elm->Out();

	// If this was the last cached instance of a font with this
	// fontnumber, and the webfont manager no longer has a registered
	// font with this number, the font number can be released.
	if (styleManager && g_webfont_manager)
	{
		UINT32 fontnum = font_elm->fontatt.GetFontNumber();
		if (!g_webfont_manager->HasFont(fontnum) && !HasCachedInstance(fontnum))
		{
			styleManager->ReleaseFontNumber(fontnum);
		}
	}

	OP_DELETE(font_elm);
	num_cached_fonts--;
	OP_ASSERT(num_cached_fonts == cached_fonts.Cardinal()); ///< Should be synchronized
}

OpFont*
FontCache::GetFont(FontAtt &fontatt, UINT32 scale, FramesDocument* doc /* = NULL */)
{
	FontCacheElement* font_elm = (FontCacheElement*)cached_fonts.First();
	FontCacheElement* oldest_unused_font_elm = NULL;
	Window* window = doc ? doc->GetWindow() : NULL;

	while (font_elm)
	{
		if (!font_elm->packed.is_marked_for_purge && font_elm->fontatt.IsFontSpecificAttEqual(fontatt) && ((font_elm->packed.scale == scale))
#ifdef FONTCACHE_PER_WINDOW
			&& font_elm->window == window
#endif // FONTCACHE_PER_WINDOW
			&& (font_elm->font->Type() == OpFontInfo::PLATFORM ||
				font_elm->doc == doc)) // only allow the document that instantiated a webfont to access it
		{
			// Cache hit
			++font_elm->ref_count;
			font_elm->last_use = ++time;
			if (font_elm != cached_fonts.First())
			{
				font_elm->Out();
				font_elm->IntoStart(&cached_fonts);
			}

#if defined(_DEBUG)
			if(!styleManager->IsSystemFont(font_elm->fontatt.GetFontNumber()))
			{
				OP_NEW_DBG("FontCache::GetFont", "webfonts");
				OP_DBG((UNI_L("Found %s match: %s (fontnumber: %d type: %d) OpFont: %p Doc: %p CurrentDoc: %p"),
						font_elm->font->Type() == OpFontInfo::SVG_WEBFONT ? UNI_L("svgfont") : UNI_L("platformfont"),
						font_elm->fontatt.GetFaceName() ? font_elm->fontatt.GetFaceName() : UNI_L("(null)"),
						font_elm->fontatt.GetFontNumber(),
						font_elm->font->Type(),
						font_elm->font,
						font_elm->doc,
						doc
						));
			}
#endif // _DEBUG

			return font_elm->font;
		}

		if (font_elm->ref_count == 0)
			if (!oldest_unused_font_elm || font_elm->last_use < oldest_unused_font_elm->last_use)
				oldest_unused_font_elm = font_elm;

		font_elm = (FontCacheElement*)font_elm->Suc();
	}

	// Delete the oldest unused font, if we hit FONTCACHESIZE.
	if (num_cached_fonts >= FONTCACHESIZE && oldest_unused_font_elm)
	{
		DeleteFontCacheElement(oldest_unused_font_elm);
	}

	// Not found in cache. Create font.

	OpFont* font = NULL;

	if(font == NULL)
	{
		font = g_webfont_manager->CreateFont((uni_char*)fontatt.GetFaceName(),
											   fontatt.GetSize() * scale / 100,
											   (UINT8)fontatt.GetWeight(),
											   fontatt.GetItalic(),
											   fontatt.GetHasOutline(),
											   fontatt.GetBlurRadius(),
											   doc);
	}

	if (font == NULL)
		return NULL;

	font_elm = OP_NEW(FontCacheElement, (font, fontatt, ++time, scale, window, doc));

	if (font_elm)
	{
		num_cached_fonts++;
		font_elm->IntoStart(&cached_fonts);
		OP_ASSERT(num_cached_fonts == cached_fonts.Cardinal()); ///< Should be synchronized
		return font;
	}

	g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	OP_DELETE(font);
	return NULL;
}

void
FontCache::ReferenceFont(OpFont* font)
{
	OP_ASSERT(font);

	// Try to find font in cache
	for (FontCacheElement* font_elm = (FontCacheElement*)cached_fonts.First();
		 font_elm;
		 font_elm = (FontCacheElement*)font_elm->Suc())
	{
		if (font_elm->font == font)
		{
			++ font_elm->ref_count;
			return;
		}
	}

	OP_ASSERT(!"FontCache::ReferenceFont passed an OpFont that is not in the cache");
}

void
FontCache::ReleaseFont(OpFont* font, BOOL force_removal)
{
	if (font == NULL)
		return;

	// Try to find font in cache
	FontCacheElement* font_elm = (FontCacheElement*)cached_fonts.First();
	while (font_elm)
	{
		if (font_elm->font == font)
		{
			if (font_elm->ref_count > 0)
				--font_elm->ref_count;

			// Delete the font if it is not referenced and we have more than FONTCACHESIZE fonts in the cache.

			OP_ASSERT(num_cached_fonts == cached_fonts.Cardinal()); ///< Should be synchronized
			if (font_elm->ref_count == 0 && (force_removal || num_cached_fonts > FONTCACHESIZE))
			{
				DeleteFontCacheElement(font_elm);
			}

			return;
		}

		font_elm = (FontCacheElement*)font_elm->Suc();
	}

	if(!force_removal)
	{
		// We shouldn't come here
		OP_ASSERT(FALSE);
	}
}

BOOL FontCache::HasCachedInstance(UINT32 font_num)
{
	// Try to find fontnumber in cache
	FontCacheElement* font_elm = (FontCacheElement*)cached_fonts.First();
	while (font_elm)
	{
		if (font_elm->fontatt.GetFontNumber() == font_num)
			return TRUE;

		font_elm = (FontCacheElement*)font_elm->Suc();
	}
	return FALSE;
}

BOOL FontCache::PurgeWebFont(UINT32 font_num)
{
	OP_NEW_DBG("PurgeWebFont", "webfonts");

	BOOL completely_purged = TRUE;
	// Try to find fontnumber in cache
	FontCacheElement* font_elm = (FontCacheElement*)cached_fonts.First();
	while (font_elm)
	{
		FontCacheElement* next = (FontCacheElement*)font_elm->Suc();

		if (font_elm->fontatt.GetFontNumber() == font_num)
		{
			if (font_elm->ref_count == 0)
			{
				DeleteFontCacheElement(font_elm);
				OP_DBG(("Purged (%d) from fontcache", font_num));
			}
			else
			{
				font_elm->packed.is_marked_for_purge = 1;
				completely_purged = FALSE;
#ifdef _DEBUG
				OP_DBG(("Failed to purge (%d) from fontcache", font_num));
#endif // _DEBUG
			}
		}

		font_elm = next;
	}
	return completely_purged;
}

#ifdef FONTCACHE_PER_WINDOW
void FontCache::ClearForWindow(Window *window)
{
	if (window == NULL)
		return;

	// Try to find font in cache
	FontCacheElement* font_elm = (FontCacheElement*)cached_fonts.First();
	FontCacheElement* next = NULL;
	while (font_elm)
	{
		next = (FontCacheElement*)font_elm->Suc();

		if (!font_elm->ref_count && font_elm->window == window)
		{
			DeleteFontCacheElement(font_elm);
		}

		font_elm = next;
	}
}
#endif // FONTCACHE_PER_WINDOW

#ifdef SVG_SUPPORT
FontCacheElement::~FontCacheElement()
{
	// SVG caches font related data and needs to
	// know when a font is no longer usable
	if (g_svg_manager) // is null during opera shutdown
	{
		g_svg_manager->HandleFontDeletion(font);
	}
	OP_DELETE(font);
}
#endif // SVG_SUPPORT
