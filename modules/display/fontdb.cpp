/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef _GLYPHTESTING_SUPPORT_
#include "modules/display/styl_man.h"
#endif // _GLYPHTESTING_SUPPORT_

#include "modules/display/fontdb.h"

#include "modules/pi/OpFont.h"

#ifdef DISPLAY_FONT_NAME_OVERRIDE
# include "modules/prefsfile/prefsfile.h"
#endif // DISPLAY_FONT_NAME_OVERRIDE

// number of entries in glyph table - UINT32 array with 32 glyphs per entry -> 2048 entries per plane
// currently contains BMP, SMP and SIP
#define GLYPHTAB_SIZE (3*2048)

OpFontDatabase::OpFontDatabase()
{
	numSystemFonts = 0;
}

#ifdef DISPLAY_FONT_NAME_OVERRIDE
BOOL OpFontDatabase::LoadFontOverrideFile(PrefsFile* prefs_obj)
{
	OP_NEW_DBG("LoadFontOverrideFile", "fontdb");
	OpFile overrides_file;

	if (OpStatus::IsError(overrides_file.Construct(UNI_L("font_overrides.ini"), OPFILE_HOME_FOLDER)))
		return FALSE;

	BOOL f_exists = FALSE;
	if (OpStatus::IsError(overrides_file.Exists(f_exists)) || !f_exists)
	{
		OP_DBG(UNI_L("No font_overrides.ini file found: No font name overrides loaded"));
		return FALSE;
	}

	TRAPD(status,
		  prefs_obj->ConstructL();
		  prefs_obj->SetFileL(&overrides_file);
		  prefs_obj->LoadAllL();
		);

	if (OpStatus::IsError(status)) {
		OP_DBG(UNI_L("Error loading font_overrides.ini"));
		return FALSE;
	}

	return TRUE;
}
#endif // DISPLAY_FONT_NAME_OVERRIDE


OP_STATUS OpFontDatabase::Construct(OpFontManager* fontmanager)
{
#ifdef DISPLAY_FONT_NAME_OVERRIDE
	PrefsFile prefs_obj(PREFS_INI);
	BOOL font_overrides_loaded = LoadFontOverrideFile(&prefs_obj);
#endif // DISPLAY_FONT_NAME_OVERRIDE

	RETURN_IF_ERROR(fontmanager->BeginEnumeration());

	numSystemFonts = fontmanager->CountFonts();
	m_font_list.SetMinimumCount(numSystemFonts);

	for (UINT32 i = 0; i < numSystemFonts; i++)
	{
		OpFontInfo* fi = OP_NEW(OpFontInfo, ());
		if(!fi)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS err = fontmanager->GetFontInfo(i, fi);
		if(OpStatus::IsSuccess(err))
		{
#ifdef DISPLAY_FONT_NAME_OVERRIDE
			if (font_overrides_loaded)
			{
				OpString fontname;
				TRAPD(status, prefs_obj.ReadStringL(UNI_L("Font Name Overrides"), fi->GetFace(), fontname, NULL));
				if (OpStatus::IsSuccess(status) && fontname.Length() != 0) {
					status = fi->SetFace(fontname);
				}
			}
#endif // DISPLAY_FONT_NAME_OVERRIDE

			err = m_font_list.Add(fi->GetFontNumber(), fi);
		}

		if(OpStatus::IsError(err))
		{
			OP_DELETE(fi);
			return err;
		}
	}

	RETURN_IF_ERROR(fontmanager->EndEnumeration());

	return OpStatus::OK;
}


OpFontDatabase::~OpFontDatabase()
{
}


OpFontInfo* OpFontDatabase::GetFontInfo(UINT32 fontnr)
{
	OpFontInfo* fi = NULL;
	OpStatus::Ignore(m_font_list.GetData(fontnr, &fi));
	return fi;
}

OP_STATUS OpFontDatabase::AddFontInfo(OpFontInfo* fi)
{
	OP_ASSERT(fi);
	OP_ASSERT(!m_font_list.Contains(fi->GetFontNumber()));
	OP_STATUS res = m_font_list.Add(fi->GetFontNumber(), fi);
	return res;
}

OP_STATUS OpFontDatabase::RemoveFontInfo(OpFontInfo* fi)
{
	OP_NEW_DBG("RemoveFontInfo", "webfonts");

	OP_ASSERT(fi);
	UINT32 n = fi->GetFontNumber();

	// never remove/release locally installed fontinfos/fontnumbers
	if (n < numSystemFonts)
	{
		return OpStatus::OK;
	}

	OP_ASSERT(m_font_list.Contains(n));

	OP_DBG(("Removing OpFontInfo %p (%d)"
			, fi
			, n
			));


	OpFontInfo* out_fi = NULL;
	RETURN_IF_ERROR(m_font_list.Remove(n, &out_fi));

	OP_ASSERT(!m_font_list.Contains(n));

	return OpStatus::OK;
}

OP_STATUS OpFontDatabase::DeleteFontInfo(OpFontInfo* fi, BOOL release_fontnumber/* = TRUE*/)
{
	OP_NEW_DBG("DeleteFontInfo", "webfonts");

	OP_ASSERT(fi);
	UINT32 n = fi->GetFontNumber();

	// never remove/release locally installed fontinfos/fontnumbers
	if (n < numSystemFonts)
	{
		return OpStatus::OK;
	}

	RemoveFontInfo(fi);

	OP_DBG(("Deleting OpFontInfo %p (%d). Release_fontnumber: %s"
			, fi
			, n
			, release_fontnumber ? "yes" : "no"
			));

	OP_ASSERT(!release_fontnumber || (release_fontnumber && !g_font_cache->HasCachedInstance(n)));
	if(release_fontnumber && m_webfont_list.Find(n) != -1)
	{
		ReleaseFontNumber(n);
	}

	OP_DELETE(fi);

	return OpStatus::OK;
}

UINT32 OpFontDatabase::AllocateFontNumber()
{
	// FIXME: Make more efficient
	UINT32 num = GetNumSystemFonts();
	while(m_webfont_list.Find(num) != -1)
      num++;
	m_webfont_list.Add(num);
	return num;
}

void OpFontDatabase::ReleaseFontNumber(UINT32 fontnumber)
{
	OP_NEW_DBG("ReleaseFontNumber", "webfonts");

	// ignore releases that are not for webfonts (happens on fontcache clearing out cached elements)
	if (fontnumber < numSystemFonts)
	{
		return;
	}

	OP_DBG(("Releasing fontnumber %d. Was allocated: %s."
			, fontnumber
			, (m_webfont_list.Find(fontnumber) != -1) ? "yes" : "no"
			));
	m_webfont_list.RemoveByItem(fontnumber);
}

UINT32 OpFontDatabase::GetNumFonts()
{
	return m_font_list.GetCount();
}
UINT32 OpFontDatabase::GetNumSystemFonts()
{
	return numSystemFonts;
}

BOOL OpFontInfo::HasWeight(UINT8 weight)
{
#ifdef FONTMAN_HASWEIGHT
	if (Type() == PLATFORM && 0 == ((has_weight >> (weight + 10)) & 0x1))
		SetWeight(weight, styleManager->GetFontManager()->HasWeight(font_number, weight));
#endif // FONTMAN_HASWEIGHT
	return 0 != ((has_weight >> weight) & 0x1);
}

OP_STATUS OpFontInfo::MergeFontInfo(const OpFontInfo& other)
{
	blocks_available[0] |= other.blocks_available[0];
	blocks_available[1] |= other.blocks_available[1];
	blocks_available[2] |= other.blocks_available[2];
	blocks_available[3] |= other.blocks_available[3];

	if (!packed.has_ligatures && other.packed.has_ligatures)
		packed.has_ligatures = 1;

#ifdef _GLYPHTESTING_SUPPORT_
	if (other.m_glyphtab == NULL)
		return OpStatus::OK;

	if (m_glyphtab == NULL)
	{
		RETURN_IF_ERROR(CreateGlyphTable());
		for(int i=0;i<GLYPHTAB_SIZE;i++)
			m_glyphtab[i] = other.m_glyphtab[i];
	}
	else
	{
		for(int i=0;i<GLYPHTAB_SIZE;i++)
			m_glyphtab[i] |= other.m_glyphtab[i];
	}
#endif // _GLYPHTESTING_SUPPORT_

	return OpStatus::OK;
}

#ifdef _GLYPHTESTING_SUPPORT_

void OpFontInfo::UpdateGlyphTableIfNeeded()
{
	if (!packed.has_scanned_for_glyphs)
	{
		packed.has_scanned_for_glyphs = 1;
		if (Type() != SVG_WEBFONT)
			styleManager->GetFontManager()->UpdateGlyphMask(this);
	}
}

BOOL OpFontInfo::HasGlyph(const UnicodePoint uc)
{
	UpdateGlyphTableIfNeeded();

    UINT32 x = (UINT32)uc;
	if (x >= 0x30000)
		return FALSE;

    if(m_glyphtab)
        return (( m_glyphtab[x >> 5] &  (1 << (x & 31)) ) != 0);

    return TRUE;
}


OP_STATUS OpFontInfo::SetGlyph(const UnicodePoint uc)
{
    UINT32 x = (UINT32)uc;
	if (x >= 0x30000)
		return OpStatus::ERR;

    if(m_glyphtab == NULL)
    {
		RETURN_IF_ERROR(CreateGlyphTable());
		ClearGlyphTable();
    }

    m_glyphtab[x >> 5] |= (1 << (x & 31));

    return OpStatus::OK;
}

OP_STATUS OpFontInfo::CreateGlyphTable()
{
	OP_ASSERT (m_glyphtab == NULL);
	m_glyphtab = OP_NEWA(UINT32, GLYPHTAB_SIZE);
	return (m_glyphtab == NULL) ? OpStatus::ERR_NO_MEMORY : OpStatus::OK;
}

void OpFontInfo::ClearGlyphTable()
{
	for(int i=0;i<GLYPHTAB_SIZE;i++)
		m_glyphtab[i] = 0;
}

OP_STATUS OpFontInfo::ClearGlyph(const UnicodePoint uc)
{
    UINT32 x = (UINT32)uc;
	if (x >= 0x30000)
		return OpStatus::ERR;

	if (m_glyphtab == NULL)
	{
		RETURN_IF_ERROR(CreateGlyphTable());
		ClearGlyphTable();
	}

    m_glyphtab[x >> 5]  &= ~(1 << (x & 31));

    return OpStatus::OK;
}

void OpFontInfo::InvalidateGlyphTable()
{
	if (m_glyphtab)
		ClearGlyphTable();

	packed.has_scanned_for_glyphs = 0;
}

#endif

/*static */OpFontInfo::CodePage OpFontInfo::CodePageFromScript(WritingSystem::Script script)
{
	switch (script)
	{
		case WritingSystem::Cyrillic:
			return OpFontInfo::OP_CYRILLIC_CODEPAGE;
		case WritingSystem::ChineseSimplified:
			return OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE;
		case WritingSystem::ChineseTraditional:
			return OpFontInfo::OP_TRADITIONAL_CHINESE_CODEPAGE;
		case WritingSystem::ChineseUnknown:
			return OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE;
		case WritingSystem::Japanese:
			return OpFontInfo::OP_JAPANESE_CODEPAGE;
		case WritingSystem::Korean:
			return OpFontInfo::OP_KOREAN_CODEPAGE;
		default:
			return OpFontInfo::OP_DEFAULT_CODEPAGE;
	}
}

// DEPRECATED temporarily introduced while not all modules are multistyle-aware - don't use!
/*static */WritingSystem::Script OpFontInfo::ScriptFromCodePage(const OpFontInfo::CodePage codepage)
{
	switch (codepage)
	{
	case OP_CYRILLIC_CODEPAGE:
		return WritingSystem::Cyrillic;
		break;
	case OP_SIMPLIFIED_CHINESE_CODEPAGE:
		return WritingSystem::ChineseSimplified;
		break;
	case OP_TRADITIONAL_CHINESE_CODEPAGE:
		return WritingSystem::ChineseTraditional;
		break;
	case OP_JAPANESE_CODEPAGE:
		return WritingSystem::Japanese;
		break;
	case OP_KOREAN_CODEPAGE:
		return WritingSystem::Korean;
		break;
	default:
		return WritingSystem::Unknown;
	}
}

/*static */WritingSystem::Script OpFontInfo::ScriptFromOS2CodePageBit(const UINT8 bit)
{
	switch (bit)
	{
	case 0:
	case 50:
	case 62:
	case 63:
		return WritingSystem::LatinWestern;
		break;

	case 1:
	case 58:
		return WritingSystem::LatinEastern;
		break;

	case 2:
	case 49:
	case 57:
		return WritingSystem::Cyrillic;
		break;

	case 3:
	case 48:
	case 60:
		return WritingSystem::Greek;
		break;

	case 4:
	case 56:
		return WritingSystem::Turkish;
		break;

	case 5:
		return WritingSystem::Hebrew;
		break;

	case 6:
	case 61:
		return WritingSystem::Arabic;
		break;

	case 7:
	case 59:
		return WritingSystem::Baltic;
		break;

	case 8:
		return WritingSystem::Vietnamese;
		break;

	case 16:
		return WritingSystem::Thai;
		break;

	case 17:
		return WritingSystem::Japanese;
		break;

	case 18:
		return WritingSystem::ChineseSimplified;
		break;
	case 20:
		return WritingSystem::ChineseTraditional;
		break;

	case 19:
		// case 21: WONKO: removed - see CORE-17691
		return WritingSystem::Korean;
		break;

	case 51:
		return WritingSystem::Arabic;
		break;

	case 53:
		return WritingSystem::Hebrew;
		break;
	}

	return WritingSystem::Unknown;
}
void OpFontInfo::SetScriptsFromOS2CodePageRanges(const UINT32* codepages, const UINT8 match_score/* = 0*/)
{
	OP_ASSERT(codepages);
	UINT8 offs = 0;
	for (UINT8 p = 0; p < 2; ++p)
	{
		for (UINT8 b = 0; b < 32; ++b)
		{
			if (!(codepages[p] & (1 << b)))
				continue;
			const WritingSystem::Script script = ScriptFromOS2CodePageBit(offs + b);
			if (script == WritingSystem::Unknown)
				continue;
			m_scripts[script] = TRUE;
			if (match_score)
				m_script_preference[script] = match_score;
		}
		offs += 32;
	}

	// make fonts that claim to support LatinWestern also claim LatinUnknown
	if (m_scripts[WritingSystem::LatinWestern])
	{
		m_scripts[WritingSystem::LatinUnknown] = TRUE;
		if (match_score)
			m_script_preference[WritingSystem::LatinUnknown] = match_score;
	}
}
