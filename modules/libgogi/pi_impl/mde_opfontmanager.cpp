/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/libgogi/pi_impl/mde_opfontmanager.h"
#include "modules/libgogi/pi_impl/mde_opfont.h"
#include "modules/display/fontdb.h"
#include "modules/display/styl_man.h"
#include "modules/libgogi/mde.h"

#ifdef MDEFONT_MODULE
#include "modules/mdefont/mdefont.h"
#endif // MDEFONT_MODULE

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaopfont.h"
#endif

#include "modules/style/css_webfont.h"

MDE_OpFontManager::MDE_OpFontManager()
		: m_fontcount(0),
		  m_fontNames(NULL),
		  m_isInEnumeration(FALSE)
{
}

MDE_OpFontManager::~MDE_OpFontManager()
{
	Clear();
}

void
MDE_OpFontManager::Clear()
{
	if (m_fontNames)
	{
		for (UINT32 i = 0 ; i < m_fontcount ; i++)
		{
			op_free(m_fontNames[i]);
		}
		OP_DELETEA(m_fontNames);
		m_fontNames = NULL;
	}
}

#if !defined(VEGA_OPPAINTER_SUPPORT) && !defined(MDE_PLATFORM_IMPLEMENTS_OPFONTMANAGER)
/*static*/
OP_STATUS
OpFontManager::Create(OpFontManager** new_fontmanager)
{
	OP_ASSERT(new_fontmanager);
	*new_fontmanager = OP_NEW(MDE_OpFontManager, ());
	if (!(*new_fontmanager))
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	// TODO: some kind of init?

	return OpStatus::OK;
}
#endif // !VEGA_OPPAINTER_SUPPORT && !MDE_PLATFORM_IMPLEMENTS_OPFONTMANAGER

OpFont* MDE_OpFontManager::CreateFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius)
{
	OP_ASSERT(!blur_radius && "blur radius not supported by MDE_OpFontManager, set TWEAK_DISPLAY_PER_GLYPH_SHADOW_LIMIT to 0");

	OP_NEW_DBG("MDE_OpFontManager::CreateFont2", "mdefont");
#if defined(HAS_FONT_FOUNDRY) || defined(PLATFORM_FONTSWITCHING)
	OP_ASSERT(FALSE);
	return NULL;
#else // !HAS_FONT_FOUNDRY && !PLATFORM_FONTSWITCHING
#ifdef MDEFONT_MODULE
	MDE_FONT* mdefont = MDF_GetWebFont((MDF_WebFontBase*)webfont, size);
	if (mdefont == NULL)
	{
		return NULL;
	}
#endif // MDEFONT_MODULE

	MDE_OpFont* font = OP_NEW(MDE_OpFont, ());
	if (font == NULL)
	{
#ifdef MDEFONT_MODULE
		MDF_ReleaseFont(mdefont);
#endif // MDEFONT_MODULE
		return NULL;
	}

#ifdef MDEFONT_MODULE
	font->Init(mdefont); // always succeeds
#endif // MDEFONT_MODULE
	font->m_type = OpFontInfo::PLATFORM_WEBFONT;
	return font;
#endif // !HAS_FONT_FOUNDRY && !PLATFORM_FONTSWITCHING
}

OpFont* MDE_OpFontManager::CreateFont(OpWebFontRef webfont, UINT8 weight, BOOL italic, UINT32 size, INT32 blur_radius)
{
	OP_ASSERT(!blur_radius && "blur radius not supported by MDE_OpFontManager, set TWEAK_DISPLAY_PER_GLYPH_SHADOW_LIMIT to 0");

	OP_NEW_DBG("MDE_OpFontManager::CreateFont2", "mdefont");
#if defined(HAS_FONT_FOUNDRY) || defined(PLATFORM_FONTSWITCHING)
	OP_ASSERT(FALSE);
	return NULL;
#else // !HAS_FONT_FOUNDRY && !PLATFORM_FONTSWITCHING
# ifdef MDEFONT_MODULE
	MDE_FONT* mdefont = MDF_GetWebFont((MDF_WebFontBase*)webfont, weight, italic, size);
	if (mdefont == NULL)
	{
		return NULL;
	}
# endif // MDEFONT_MODULE

	MDE_OpFont* font = OP_NEW(MDE_OpFont, ());
	if (font == NULL)
	{
# ifdef MDEFONT_MODULE
		MDF_ReleaseFont(mdefont);
# endif // MDEFONT_MODULE
		return NULL;
	}

# ifdef MDEFONT_MODULE
	font->Init(mdefont); // always succeeds
# endif // MDEFONT_MODULE
	font->m_type = OpFontInfo::PLATFORM_WEBFONT;
	return font;
#endif // !HAS_FONT_FOUNDRY && !PLATFORM_FONTSWITCHING
}

OpFont* MDE_OpFontManager::CreateFont(UINT32 fontnr, UINT32 size, UINT8 weight, BOOL italic, BOOL must_have_getoutline)
{
    OP_NEW_DBG("MDE_OpFontManager::CreateFont2", "mdefont");
#if defined(HAS_FONT_FOUNDRY) || defined(PLATFORM_FONTSWITCHING)
    OP_ASSERT(FALSE);
    return NULL;
#else // !HAS_FONT_FOUNDRY && !PLATFORM_FONTSWITCHING
#ifdef MDEFONT_MODULE
    MDE_FONT* mdefont = MDF_GetFont(fontnr, size, weight > 5, italic ? true : false);
#endif // MDEFONT_MODULE

# ifdef MDEFONT_MODULE
    if(must_have_getoutline && mdefont)
    {
        if (!MDF_IsScalable(mdefont))
        {
            MDF_ReleaseFont(mdefont);
            return NULL;
        }
    }
#endif // MDEFONT_MODULE

#ifdef MDEFONT_MODULE
    if (mdefont == NULL)
    {
        OP_DBG(("MDE_GetFont failed, trying fallback"));
        mdefont = MDF_GetFont(fontnr, size, false, false);
        if(must_have_getoutline && mdefont)
        {
            if (!MDF_IsScalable(mdefont))
            {
                MDF_ReleaseFont(mdefont);
                return NULL;
            }
        }

        if (mdefont == NULL)
        {
            return NULL;
        }
    }
#endif // MDEFONT_MODULE

    MDE_OpFont* font = OP_NEW(MDE_OpFont, ());
    if (font == NULL)
    {
#ifdef MDEFONT_MODULE
        MDF_ReleaseFont(mdefont);
#endif // MDEFONT_MODULE
        return NULL;
    }

#ifdef MDEFONT_MODULE
    font->Init(mdefont); // always succeeds
#endif // MDEFONT_MODULE
    return font;
#endif // !HAS_FONT_FOUNDRY && !PLATFORM_FONTSWITCHING
}

OpFont*
MDE_OpFontManager::CreateFont(const uni_char* face, UINT32 size, UINT8 weight,
							   BOOL italic, BOOL must_have_getoutline, INT32 blur_radius)
{
	OP_ASSERT(!blur_radius && "blur radius not supported by MDE_OpFontManager, set TWEAK_DISPLAY_PER_GLYPH_SHADOW_LIMIT to 0");

	OP_NEW_DBG("MDE_OpFontManager::CreateFont", "mde_font");

	if (face == NULL)
	{
		OP_DBG(("got face == NULL"));
		face = GetGenericFontName(GENERIC_FONT_SANSSERIF);
		// return NULL;
	}

	OP_DBG((UNI_L("face = %s, size = %d, weight = %d, italic = %d"), face, size, weight, italic));

	UINT32 fontnr = 0;
	for (fontnr = 0 ; fontnr < m_fontcount ; fontnr++)
	{
		OP_DBG((UNI_L("singlebyte face: \"%s\" compares to \"%s\""), face, m_fontNames[fontnr]));
		if (face && m_fontNames[fontnr] && uni_strcmp(face, m_fontNames[fontnr]) == 0)
		{
			OP_DBG(("face match, breaking"));
			break;
		}
	}

	if (fontnr == m_fontcount)
	{
		OP_DBG(("no face match, bailing out"));
		return NULL;
	}

	OP_DBG(("trying MDE_GetFont(fontnr = %d, size = %d, weight = %d, italic = %d)", fontnr, size, weight > 5, italic));
	return CreateFont(fontnr, size, weight, italic, must_have_getoutline);
}

OP_STATUS
MDE_OpFontManager::AddWebFont(OpWebFontRef& webfont, const uni_char* full_path_of_file)
{
#ifdef MDEFONT_MODULE
	MDF_WebFontBase* ref;
	RETURN_IF_ERROR(MDF_AddFontFile(full_path_of_file, ref));
	webfont = (OpWebFontRef)ref;
	return OpStatus::OK;
#else
	return OpStatus::OK;
#endif // MDEFONT_MODULE
}

OP_STATUS
MDE_OpFontManager::RemoveWebFont(OpWebFontRef webfont)
{
# ifdef MDEFONT_MODULE
    return MDF_RemoveFont((MDF_WebFontBase*)webfont);
#else
	OP_ASSERT(!"MDE_Font module required");
	return OpStatus::ERR_NOT_SUPPORTED;
# endif // MDEFONT_MODULE
}

OP_STATUS
MDE_OpFontManager::GetWebFontInfo(OpWebFontRef webfont, OpFontInfo* fontinfo)
{
# ifdef MDEFONT_MODULE
	MDF_FONTINFO mdfinfo;
	RETURN_IF_ERROR(MDF_GetWebFontInfo((MDF_WebFontBase*)webfont, &mdfinfo));
	return GetFontInfoInternal(mdfinfo, fontinfo);
# else
	OP_ASSERT(!"MDE_Font module required");
	return OpStatus::ERR_NOT_SUPPORTED;
# endif // MDEFONT_MODULE
}

OP_STATUS
MDE_OpFontManager::GetLocalFont(OpWebFontRef& localfont, const uni_char* facename)
{
# ifdef MDEFONT_MODULE
	MDF_WebFontBase* local = 0;
	RETURN_IF_ERROR(MDF_GetLocalFont(local, facename));
	localfont = (OpWebFontRef)local;
	return OpStatus::OK;
# else
	OP_ASSERT(!"MDE_Font module required");
	return OpStatus::ERR_NOT_SUPPORTED;
# endif // MDEFONT_MODULE
}

BOOL
MDE_OpFontManager::SupportsFormat(int f)
{
# ifdef MDEFONT_MODULE
	const CSS_WebFont::Format format = static_cast<CSS_WebFont::Format>(f);
	return (format & (CSS_WebFont::FORMAT_TRUETYPE | CSS_WebFont::FORMAT_TRUETYPE_AAT | CSS_WebFont::FORMAT_OPENTYPE)) != 0;
# else
	OP_ASSERT(!"MDE_Font module required");
	return 0;
# endif // MDEFONT_MODULE
}

UINT32
MDE_OpFontManager::CountFonts()
{
	return m_fontcount;
}

#include "modules/util/simset.h"

OP_STATUS MDE_OpFontManager::InitFonts()
{
	m_fontNames = OP_NEWA(uni_char*, m_fontcount);

	if (m_fontNames == NULL)
		return OpStatus::ERR_NO_MEMORY;

	for (UINT32 fontnr = 0; fontnr < m_fontcount; ++fontnr)
	{
#ifdef MDEFONT_MODULE
		MDF_FONTINFO info;
		RETURN_IF_ERROR(MDF_GetFontInfo(fontnr, &info));
		if (info.font_name == NULL)
			info.font_name = UNI_L("unknown");
		m_fontNames[fontnr] = uni_strdup(info.font_name ? info.font_name : UNI_L("unknown"));

		if (m_fontNames[fontnr] == NULL)
			return OpStatus::ERR_NO_MEMORY;
#endif // MDEFONT_MODULE
	}
	return OpStatus::OK;
}

#ifdef MDF_CODEPAGES_FROM_OS2_TABLE
#endif // MDF_CODEPAGES_FROM_OS2_TABLE

OP_STATUS
MDE_OpFontManager::GetFontInfo(UINT32 fontnr, OpFontInfo* fontinfo)
{
#ifdef MDEFONT_MODULE
	MDF_FONTINFO info;
	RETURN_IF_ERROR(MDF_GetFontInfo(fontnr, &info));
	if (info.font_name == NULL)
		info.font_name = UNI_L("unknown");
	fontinfo->SetFontNumber(fontnr);
	return GetFontInfoInternal(info, fontinfo);
#else
	OP_ASSERT(fontinfo);

	// fixme wait for mde block info api or implement using the glyph api
	int i;
	for (i = 0 ; i < 128 ; i++)
	{
		fontinfo->SetBlock(i, FALSE);
	}
	return OpStatus::OK;
#endif // MDEFONT_MODULE
}

#ifdef MDEFONT_MODULE
OP_STATUS
MDE_OpFontManager::GetFontInfoInternal(const MDF_FONTINFO& info, OpFontInfo* fontinfo)
{
	OP_ASSERT(fontinfo);

	OP_STATUS status = fontinfo->SetFace(info.font_name);
	fontinfo->SetWeight(4, info.has_normal);
	fontinfo->SetWeight(7, info.has_bold);
	fontinfo->SetNormal(info.has_normal);
	fontinfo->SetItalic(info.has_italic);
	fontinfo->SetMonospace(info.is_monospace);

	switch (info.has_serif)
	{
		case MDF_SERIF_SANS:
			fontinfo->SetSerifs(OpFontInfo::WithoutSerifs);
			break;
		case MDF_SERIF_SERIF:
			fontinfo->SetSerifs(OpFontInfo::WithSerifs);
			break;
		case MDF_SERIF_UNDEFINED:
			fontinfo->SetSerifs(OpFontInfo::UnknownSerifs);
			break;
		default:
			OP_ASSERT(FALSE); // something broken may not be entirely correct
	}

	// fixme wait for mde block info api or implement using the glyph api
	int i;
	for (i = 0 ; i < 128 ; i++)
	{
		fontinfo->SetBlock(i, FALSE);
	}

	for (i = 0 ; i < 4 ; i++)
	{
		for (int j = 0 ; j < 32 ; j++)
		{
			fontinfo->SetBlock(i * 32 + j, (info.ranges[i] & (1<<j)) == 0 ? FALSE : TRUE);
		}
	}

#ifdef MDF_CODEPAGES_FROM_OS2_TABLE
	fontinfo->SetScriptsFromOS2CodePageRanges(info.m_codepages, 1);
#endif // MDF_CODEPAGES_FROM_OS2_TABLE

	return status;
}
#endif // MDEFONT_MODULE

OP_STATUS
MDE_OpFontManager::BeginEnumeration()
{
	if (m_isInEnumeration)
		return OpStatus::ERR;

	Clear();
#ifdef MDEFONT_MODULE
	RETURN_IF_ERROR(MDF_BeginFontEnumeration());
	m_fontcount = MDF_CountFonts();
#endif // MDEFONT_MODULE
	m_isInEnumeration = TRUE;

	return InitFonts();
}

OP_STATUS
MDE_OpFontManager::EndEnumeration()
{
	if (!m_isInEnumeration)
		return OpStatus::ERR;

#ifdef MDEFONT_MODULE
	MDF_EndFontEnumeration();
#endif // MDEFONT_MODULE

    MDE_GenericFonts dfonts;
    if( !OpStatus::IsSuccess(FindDefaultFonts(&dfonts)) )
        return OpStatus::ERR;

	SetDefaultFonts(&dfonts);

	m_isInEnumeration = FALSE;
	return OpStatus::OK;
}

OP_STATUS
MDE_OpFontManager::FindDefaultFonts(MDE_GenericFonts* fonts)
{
    fonts->default_font    = "Bitstream Vera Sans";
    fonts->cursive_font    = "Bitstream Vera Sans";
    fonts->fantasy_font    = "Bitstream Vera Sans";
    fonts->monospace_font  = "Bitstream Vera Sans Mono";
    fonts->sans_serif_font = "Bitstream Vera Sans";
    fonts->serif_font      = "Bitstream Vera Serif";

    return OpStatus::OK;
}

const uni_char*
MDE_OpFontManager::GetGenericFontName(GenericFont generic_font)
{
	const uni_char* name;
	switch(generic_font)
	{
	case GENERIC_FONT_SERIF:
		name = m_default_fonts.serif_font;
		break;
	case GENERIC_FONT_SANSSERIF:
		name = m_default_fonts.sans_serif_font;
		break;
	case GENERIC_FONT_CURSIVE:
		name = m_default_fonts.cursive_font;
		break;
	case GENERIC_FONT_FANTASY:
		name = m_default_fonts.fantasy_font;
		break;
	case GENERIC_FONT_MONOSPACE:
		name = m_default_fonts.monospace_font;
		break;
	default:
		name = m_default_fonts.default_font;
	}
	// font doesn't exist - get fallback from StyleManager
	if (styleManager && styleManager->GetFontNumber(name) < 0)
	{
		StyleManager::GenericFont id = StyleManager::GetGenericFont(generic_font);
		if (id == StyleManager::UNKNOWN) // what does default mean? i guess serif's as good as anything ...
			id = StyleManager::SERIF;
		short num = styleManager->GetGenericFontNumber(id, WritingSystem::Unknown);
		OP_ASSERT(num >= 0);
		name = styleManager->GetFontFace(num);
	}
	return name;
}

#ifdef HAS_FONT_FOUNDRY
OP_STATUS
MDE_OpFontManager::FindBestFontName(const uni_char *name, OpString &full_name)
{
	OP_ASSERT(0);
	return OpStatus::ERR;
}

OP_STATUS
MDE_OpFontManager::GetFoundry(const uni_char *font, OpString &foundry)
{
	OP_ASSERT(0);
	return OpStatus::ERR;
}

OP_STATUS
MDE_OpFontManager::SetPreferredFoundry(const uni_char *foundry)
{
	OP_ASSERT(0);
	return OpStatus::ERR;
}
#endif // HAS_FONT_FOUNDRY

#ifdef PLATFORM_FONTSWITCHING
void
MDE_OpFontManager::SetPreferredFont(uni_char ch, bool monospace, const uni_char *font,
								  OpFontInfo::CodePage codepage)
{
	OP_ASSERT(0);
}
#endif // PLATFORM_FONTSWITCHING

OP_STATUS
MDE_OpFontManager::SetDefaultFonts(const MDE_GenericFonts* fonts)
{
	if (fonts == NULL || fonts->default_font == NULL)
	{
		return OpStatus::ERR;
	}

	m_default_fonts.Clear();

	if (fonts->default_font)
	{
		m_default_fonts.default_font = uni_up_strdup(fonts->default_font);
		if (m_default_fonts.default_font == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
	{
		// it must exist
		return OpStatus::ERR;
	}

	if (fonts->serif_font)
	{
		m_default_fonts.serif_font = uni_up_strdup(fonts->serif_font);
		if (m_default_fonts.serif_font == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	if (fonts->sans_serif_font)
	{
		m_default_fonts.sans_serif_font = uni_up_strdup(fonts->sans_serif_font);
		if (m_default_fonts.sans_serif_font == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	if (fonts->cursive_font)
	{
		m_default_fonts.cursive_font = uni_up_strdup(fonts->cursive_font);
		if (m_default_fonts.cursive_font == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	if (fonts->fantasy_font)
	{
		m_default_fonts.fantasy_font = uni_up_strdup(fonts->fantasy_font);
		if (m_default_fonts.fantasy_font == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	if (fonts->monospace_font)
	{
		m_default_fonts.monospace_font = uni_up_strdup(fonts->monospace_font);
		if (m_default_fonts.monospace_font == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	// now check that all fonts exist:
	OpFont* font;
	font = CreateFont(m_default_fonts.default_font, 8, 4, FALSE, FALSE, 0);
	if (font == NULL)
	{
		// the default font must be valid, the others will be set to fallback
		return OpStatus::ERR;
	}
	OP_DELETE(font); font = NULL;

	if (m_default_fonts.serif_font != NULL)
	{
		font = CreateFont(m_default_fonts.serif_font, 8, 4, FALSE, FALSE, 0);
		// NULL here may indicate OOM. but since this is during startup
		// it is unlikely, so if all fonts get mapped to the default one
		// because of that, then so be it.
	}

	if (font == NULL)
	{
		op_free(m_default_fonts.serif_font);
		m_default_fonts.serif_font = uni_up_strdup(fonts->default_font);
		if (m_default_fonts.serif_font == NULL)
		{
			OP_DELETE(font);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	OP_DELETE(font);
	font = NULL;

	return OpStatus::OK;
}


#ifdef _GLYPHTESTING_SUPPORT_

void MDE_OpFontManager::UpdateGlyphMask(OpFontInfo *fontinfo)
{
#ifdef MDEFONT_MODULE
	OpStatus::Ignore(MDF_UpdateGlyphMask(fontinfo)); // FIXME: OOM
#endif // MDEFONT_MODULE
}
#endif // _GLYPHTESTING_SUPPORT_
