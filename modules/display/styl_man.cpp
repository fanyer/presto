/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/layout/box/box.h"
#include "modules/display/styl_man.h"
#include "modules/display/fontcache.h"
#include "modules/display/sfnt_base.h"
#include "modules/pi/OpSystemInfo.h"

#include "modules/encodings/tablemanager/optablemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/debug/debug.h"

#ifdef DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
#include "modules/display/display_module.h"
#endif // DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES

#ifdef EPOC
#include <w32std.h>
#include <eikenv.h>
#ifdef FONTSWITCHING
#include "EpocFont.h"
#endif
#endif // EPOC

#if defined TEXTSHAPER_SUPPORT && defined USE_TEXTSHAPER_INTERNALLY
#include "modules/display/textshaper.h"
#endif // TEXTSHAPER_SUPPORT && USE_TEXTSHAPER_INTERNALLY

// static
StyleManager::GenericFont StyleManager::GetGenericFontType(const OpFontInfo* base)
{
	const BOOL monospace = base->Monospace();
	GenericFont generic;
	OpFontInfo::FontSerifs serifs = monospace ? OpFontInfo::UnknownSerifs : base->GetSerifs();
	switch (serifs)
	{
	case OpFontInfo::WithSerifs:
		generic = SERIF;
		break;
	case OpFontInfo::WithoutSerifs:
		generic = SANS_SERIF;
		break;
	default:
		generic = monospace ? MONOSPACE : SERIF;
	}
	return generic;
}

// static
StyleManager::GenericFont StyleManager::GetGenericFont(const uni_char* fontName)
{
	if (uni_stri_eq(fontName, "SERIF"))
		return SERIF;
	if (uni_stri_eq(fontName, "SANS-SERIF"))
		return SANS_SERIF;
	if (uni_stri_eq(fontName, "FANTASY"))
		return FANTASY;
	if (uni_stri_eq(fontName, "CURSIVE"))
		return CURSIVE;
	if (uni_stri_eq(fontName, "MONOSPACE"))
		return MONOSPACE;

	return UNKNOWN;
}
// static
StyleManager::GenericFont StyleManager::GetGenericFont(::GenericFont font)
{
	switch (font)
	{
	case GENERIC_FONT_SANSSERIF:
		return StyleManager::SANS_SERIF;
		break;
	case GENERIC_FONT_CURSIVE:
		return StyleManager::CURSIVE;
		break;
	case GENERIC_FONT_FANTASY:
		return StyleManager::FANTASY;
		break;
	case GENERIC_FONT_MONOSPACE:
		return StyleManager::MONOSPACE;
		break;
	case GENERIC_FONT_SERIF:
		return StyleManager::SERIF;
		break;
	}
	return UNKNOWN;
}

StyleManager::StyleManager()
	: fontface_hash(FALSE)
	, m_alphabetical_font_list(0)
	, m_ahem_font_number(-1)
{
    int i;
	for (i=0; i<NUMBER_OF_STYLES; i++)
        style[i] = 0;

    for (i=0; i<STYLE_EX_SIZE; i++)
        style_ex[i] = 0;

#ifdef FONTSWITCHING
	block_table = NULL;
	block_table_internal = NULL;
	block_table_internal_count = 0;
	block_table_present = TRUE;
	locale_script = WritingSystem::Unknown;

//	styleManager = this; //Sorry, this is ugly, but otherwise we crash... - AM
#endif // FONTSWITCHING

	fonts_in_hash = 0;

	for (i = 0; i < WritingSystem::NumScripts; ++i)
		for (int j = 0; j < UNKNOWN; ++j)
			generic_font_numbers[i][j] = -1;
}

OP_STATUS StyleManager::Construct()
{
	TRAPD(err, locale_script = WritingSystem::FromEncoding(g_op_system_info->GetSystemEncodingL()));

	OP_STATUS status = OpFontManager::Create(&fontManager);

	if (OpStatus::IsError(status))
	{
		if (status == OpStatus::ERR_NO_MEMORY)
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		fontDatabase = NULL;
		return status;
	}

	fontDatabase = OP_NEW(OpFontDatabase, ());

	if (fontDatabase == NULL)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		OP_DELETE(fontManager);
		fontManager = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	status = fontDatabase->Construct(fontManager);
	if (OpStatus::IsError(status))
	{
		if (status == OpStatus::ERR_NO_MEMORY)
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		OP_DELETE(fontDatabase);
		fontDatabase = NULL;
		OP_DELETE(fontManager);
		fontManager = NULL;
		return status;
	}

	const int numfonts = fontDatabase->GetNumSystemFonts();
	m_alphabetical_font_list = OP_NEWA(short, numfonts);
	if (!m_alphabetical_font_list)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return OpStatus::ERR_NO_MEMORY;
	}
	for (INT32 i = 0; i < numfonts; ++i)
	{
		m_alphabetical_font_list[i] = i;

		OpFontInfo *fontinfo = GetFontInfo(i);
		if (uni_stricmp(fontinfo->GetFace(), UNI_L("ahem")) == 0)
			m_ahem_font_number = i;
	}

	// HACK: font_name_comp uses styleManager to get font name
	StyleManager* old = g_opera->display_module.style_manager;
	g_opera->display_module.style_manager = this;
	BuildAlphabeticalList();

	fontManager->BeforeStyleInit(this);

#ifdef _GLYPHTESTING_SUPPORT_
	for (UINT32 i = 0; i < fontDatabase->GetNumSystemFonts(); ++ i)
		SetupFontScriptsFromGlyphs(GetFontInfo(i));
#endif // _GLYPHTESTING_SUPPORT_

#ifdef PERSCRIPT_GENERIC_FONT
	InitGenericFont(SERIF);
	InitGenericFont(SANS_SERIF);
	InitGenericFont(CURSIVE);
	InitGenericFont(FANTASY);
	InitGenericFont(MONOSPACE);
#else
	SetGenericFont(SERIF, fontManager->GetGenericFontName(GENERIC_FONT_SERIF));
	SetGenericFont(SANS_SERIF, fontManager->GetGenericFontName(GENERIC_FONT_SANSSERIF));
	SetGenericFont(CURSIVE, fontManager->GetGenericFontName(GENERIC_FONT_CURSIVE));
	SetGenericFont(FANTASY, fontManager->GetGenericFontName(GENERIC_FONT_FANTASY));
	SetGenericFont(MONOSPACE, fontManager->GetGenericFontName(GENERIC_FONT_MONOSPACE));
#endif

	g_opera->display_module.style_manager = old;

	return OpStatus::OK;
}

/* static */
StyleManager* StyleManager::Create()
{
	StyleManager* style_manager = OP_NEW(StyleManager, ());
	if (!style_manager)
		return NULL;
	OP_STATUS status = style_manager->Construct();
	if (OpStatus::IsError(status))
	{
		OP_DELETE(style_manager);
		return NULL;
	}
	return style_manager;
}

/** Destructor. */
StyleManager::~StyleManager()
{
	int i;
	for (i=0; i<NUMBER_OF_STYLES; i++)
	{
	    if (style[i] && style[i] != NullStyle)
	        OP_DELETE(style[i]);
	}
	for (i=0; i<STYLE_EX_SIZE; i++)
	{
	    if (style_ex[i] && style_ex[i] != NullStyle)
	        OP_DELETE(style_ex[i]);
	}

	preferredFonts.Clear();

	OP_DELETE(fontDatabase);
	OP_DELETE(fontManager);

	OP_DELETEA(m_alphabetical_font_list);
#ifdef FONTSWITCHING
	OP_DELETEA(block_table_internal);
#endif // FONTSWITCHING
}

static int font_name_comp(const void* ap, const void* bp)
{
	const short a = *(const short*)ap;
	const short b = *(const short*)bp;
	OP_ASSERT(styleManager);
	const uni_char* aname = styleManager->GetFontFace(a);
	const uni_char* bname = styleManager->GetFontFace(b);
	OP_ASSERT(aname && bname);
	return uni_strcmp(aname, bname);
}
void StyleManager::BuildAlphabeticalList()
{
	const int numfonts = fontDatabase->GetNumSystemFonts();
	op_qsort(m_alphabetical_font_list, numfonts, sizeof(short), font_name_comp);

	for (int i = 0; i < numfonts - 1; ++i)
		OP_ASSERT(uni_strcmp(GetFontFace(m_alphabetical_font_list[i]), GetFontFace(m_alphabetical_font_list[i+1])) < 0);
}

#ifdef _GLYPHTESTING_SUPPORT_
void StyleManager::SetupFontScriptsFromGlyphs(OpFontInfo* fontinfo)
{
	OP_ASSERT(fontinfo);
	// OS/2 table doesn't discern between Arabic and Persian, but
	// Persian uses glyphs not commonly present in Arabic fonts
	if (fontinfo->HasScript(WritingSystem::Arabic))
	{
		// check for the most common Persian glyphs, and if present
		// mark the font as supporting Persian
		uni_char persianGlyphs[] = {
		    0x067e, // Peh
		    0x0686, // Tcheh
		    0x0698, // Jeh
		    0x06a9, // Keheh
		    0x06af, // Gaf
		    0x06cc  // Yeh
		};
		size_t j;
		for (j = 0; j < ARRAY_SIZE(persianGlyphs); ++j)
			if (!fontinfo->HasGlyph(persianGlyphs[j]))
				break;
		if (j == ARRAY_SIZE(persianGlyphs))
			fontinfo->SetScript(WritingSystem::Persian);
	}
}
#endif // _GLYPHTESTING_SUPPORT_

Style* StyleManager::GetStyle(HTML_ElementType helm) const
{
    if (style[helm])
        return style[helm];
    else
        return NullStyle;
}

OP_STATUS StyleManager::SetStyle(HTML_ElementType helm, Style *s)
{
    Style* old_style = GetStyle(helm);
    if (old_style && old_style != NullStyle)
        OP_DELETE(old_style);

	// If we try to set a non existing font, choose an existing font.
	const PresentationAttr& pattr = s->GetPresentationAttr();
	for (int i = 0; i < (int)WritingSystem::NumScripts; ++i)
	{
		WritingSystem::Script script = (WritingSystem::Script)i;
		const PresentationAttr::PresentationFont& fattr = pattr.GetPresentationFont(script);
		if (fattr.FontNumber < 0)
		{
			GenericFont id = helm == HE_PRE ? MONOSPACE : SANS_SERIF;
			int gen_num = GetGenericFontNumber(id, script);
			if (fattr.Font)
			{
				fattr.Font->SetFontNumber(gen_num);
				OP_STATUS status = s->SetFont(fattr.Font, script);
				// should always succeed, since fattr.Font exists
				OP_ASSERT(OpStatus::IsSuccess(status));
				OpStatus::Ignore(status);
			}
			else
			{
				FontAtt att;
				// "inherit" from HE_DOC_ROOT
				FontAtt* docroot = GetStyle(HE_DOC_ROOT)->GetPresentationAttr().GetPresentationFont(script).Font;
				if (docroot)
					att = *docroot;
				att.SetFontNumber(gen_num);
				RETURN_IF_ERROR(s->SetFont(&att, script));
			}
		}
	}
    style[helm] = s;
	return OpStatus::OK;
}

Style* StyleManager::GetStyleEx(int id) const
{
    if (id >= 0 && id < STYLE_EX_SIZE && style_ex[id])
        return style_ex[id];
    else
        return NullStyle;
}

void StyleManager::SetStyleEx(int id, Style *s)
{
	if (id >= 0 && id < STYLE_EX_SIZE)
	{
		Style* old_style = GetStyleEx(id);
		if (old_style && old_style != NullStyle)
			OP_DELETE(old_style);
		style_ex[id] = s;
	}
}

#ifdef DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
BOOL FontNameNeedsTranslation(const uni_char* name)
{
	return *name > 0xff;
}
const uni_char* TranslateFontName(const uni_char* name)
{
	const uni_char* western;
	if (FontNameNeedsTranslation(name) && (western = g_display_module.GetWesternFontName(name)))
		return western;
	return name;
}
#endif // DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES

short StyleManager::LookupFontNumber(HLDocProfile* hld_profile, const uni_char* family_name, CSS_MediaType media_type, BOOL set_timestamp)
{
	short fnum = -1;
	// Find and start loading web-font
	CSSCollection* coll = hld_profile->GetCSSCollection();

	OP_NEW_DBG("LookupFontNumber", "webfonts");
	BOOL is_web_font = FALSE;
	BOOL all_webfonts_loaded = TRUE;
	double start_time = g_op_time_info->GetRuntimeMS();
	double end_time = start_time;

	int font_timeout = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::WaitForWebFonts, hld_profile->GetFramesDocument()->GetHostName());

	if (font_timeout == 0)
		set_timestamp = FALSE;

	if (coll && g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableWebfonts, *hld_profile->GetURL()))
	{
		CSS_WebFont* web_font = coll->GetWebFont(family_name, media_type);
		if (web_font)
			is_web_font = TRUE;

		// If we have some webfonts with this family name, then start loading them
		while(web_font)
		{
			if (web_font->GetLoadStatus() == CSS_WebFont::WEBFONT_NOTLOADED)
			{
				OP_STATUS stat = web_font->Load(hld_profile->GetFramesDocument());
				if (OpStatus::IsMemoryError(stat))
					hld_profile->SetIsOutOfMemory(TRUE);
#ifdef _DEBUG
				OpString str;
				OpStatus::Ignore(web_font->GetSrcURL().GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI, str, TRUE));
				OP_DBG((UNI_L("Load (%d): %s (%s)"),
						GetFontNumber(family_name),
						family_name,
						str.CStr()
						));
#endif // _DEBUG
			}

			OP_ASSERT(web_font->GetLoadStatus() != CSS_WebFont::WEBFONT_NOTLOADED);

			if (web_font->GetLoadStatus() != CSS_WebFont::WEBFONT_LOADED && web_font->GetLoadStatus() != CSS_WebFont::WEBFONT_NOTFOUND)
			{
				if (all_webfonts_loaded)
				{
#ifdef _DEBUG
					int fn_tmp = GetFontNumber(family_name);
					OP_DBG(("Found %d loaded. Status: %d. IsLocal: %s. Style: %d. Weight: %d. Format: %d",
							fn_tmp,
							web_font->GetLoadStatus(),
							web_font->IsLocalFont() ? "yes" : "no",
							web_font->GetStyle(),
							web_font->GetWeight(),
							web_font->GetFormat()));
#endif // _DEBUG
					all_webfonts_loaded = FALSE;
				}

				if (set_timestamp)
				{
					double t = web_font->GetTimeStamp();
					if (t == 0)
					{
						t = start_time + font_timeout;
						web_font->SetTimeStamp(t);
					}
					if (end_time < t)
						end_time = t;
				}

			}

			web_font = static_cast<CSS_WebFont*>(web_font->Suc());
		}
	}

	// if we didn't find any webfonts => select a system font
	// if we're loading a webfont => use default font for now
	if (is_web_font && !all_webfonts_loaded)
	{
		if (start_time == end_time)
			return -1;

		coll->RegisterWebFontTimeout((int) (end_time - start_time));

		return -2;
	}

	fnum = GetFontNumber(family_name);

	// The GetFontNumber method can return disallowed font id:s since it operates on name alone.
	if (!g_webfont_manager->AllowedInDocument(fnum, hld_profile->GetFramesDocument()))
		fnum = -1;

	return fnum;
}

/**
    Looks up and retreives the font number of a certain font face.
    @param fontface the name of the font to retreive the number for
    @return -1 if font does not exist on this system
*/

short StyleManager::GetFontNumber(const uni_char* orig_fontface)
{
	if (NULL == orig_fontface || 0 == *orig_fontface)
		return -1;

#ifdef DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
	const uni_char* fontface = TranslateFontName(orig_fontface);
#else
	const uni_char* fontface = orig_fontface;
#endif // DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES

	uint32 numFonts = fontDatabase->GetNumFonts();

	if (fonts_in_hash != numFonts)
	{
		// If more than 50 fonts, update the hash.
		fonts_in_hash = numFonts;
		fontface_hash.RemoveAll();
		if (numFonts > 50)
		{
			OpHashIterator* it = fontDatabase->GetIterator();
			if (it)
			{
				for (OP_STATUS err = it->First(); OpStatus::IsSuccess(err); err = it->Next())
				{
					OpFontInfo* fontInfo = (OpFontInfo*)it->GetData();
					OP_ASSERT(fontInfo->GetFace());
					if (OpStatus::IsError(fontface_hash.Add(fontInfo->GetFace(), (void*)(INTPTR)fontInfo->GetFontNumber())))
					{
						// Didn't have memory. Just bail out with a clean list. Everything will work anyway.
						// fonts_in_hash will still be numFonts so we won't try this every time we get here.
						fontface_hash.RemoveAll();
						break;
					}
				}
				OP_DELETE(it);
			}
		}
	}

	if (fontface_hash.GetCount())
	{
		void *data;
		if (OpStatus::IsSuccess(fontface_hash.GetData(fontface, &data)))
		{
			return (INTPTR) data;
		}
	}
	else
	{
		OpHashIterator* it = fontDatabase->GetIterator();
		if (it)
		{
			for (OP_STATUS err = it->First(); OpStatus::IsSuccess(err); err = it->Next())
			{
				OpFontInfo* fontInfo = (OpFontInfo*)it->GetData();
				if (fontInfo && fontInfo->GetFace())
				{
					if (uni_stricmp(fontInfo->GetFace(), fontface) == 0)
					{
						OP_DELETE(it);
						return fontInfo->GetFontNumber();
					}
				}
			}
			OP_DELETE(it);
		}
	}

#ifdef HAS_FONT_FOUNDRY
	OpString fullname;
	OP_STATUS res = fontManager->FindBestFontName(fontface, fullname);
	// FIXME: OOM
	if (OpStatus::IsSuccess(res))
	{
		fontface = fullname.CStr();
		for (uint32 i = 0; i < fontDatabase->GetNumSystemFonts(); i++)
		{
			OpFontInfo* fontInfo = fontDatabase->GetFontInfo(i);
			if (fontInfo && fontInfo->GetFace())
			{
				if (uni_stricmp(fontInfo->GetFace(), fullname.CStr()) == 0)
					return fontInfo->GetFontNumber();
			}
		}
	}
#endif // HAS_FONT_FOUNDRY

	return -1;
}

const uni_char*	StyleManager::GetFontFace(int font_number)
{
	OpFontInfo* fontInfo = GetFontInfo(font_number);

	if (fontInfo)
		return fontInfo->GetFace();
	else
		return NULL;
}

BOOL StyleManager::HasFont(int font_number)
{
	return GetFontInfo(font_number) != NULL;
}

//rg check what we expect from the opera side: what scale of weights. as of
// now we have some inconsistencies, as the font api weights are from 0 to 9.
int	StyleManager::GetNextFontWeight(int weight, BOOL bolder)
{
	if (bolder)
		if (weight < 4)
			return 4;
		else if (weight < 6)
			return 7;
		else
			return 9;
	else
		if (weight < 6)
			return 1;
		else if (weight < 8)
			return 4;
		else
			return 7;
}

OpFontInfo* StyleManager::GetFontInfo(int font_number)
{
	return fontDatabase->GetFontInfo((uint32) font_number);
}

short StyleManager::FindFallback(GenericFont id)
{
	const int numFonts = fontDatabase->GetNumSystemFonts();
	if (numFonts <= 0)
		return -1;

	OpFontInfo::FontSerifs wantedSerifs;
	if (id == SERIF)
		wantedSerifs = OpFontInfo::WithSerifs;
	else if (id == SANS_SERIF)
		wantedSerifs = OpFontInfo::WithoutSerifs;
	else
		wantedSerifs = OpFontInfo::UnknownSerifs;

	// kludge - ahem is not a good fallback font
	short ahem = GetFontNumber(UNI_L("ahem"));
	short cand_idx = -1;
	for (short i = 0; i < numFonts; ++i)
	{
		short idx = m_alphabetical_font_list[i];
		if (idx == ahem)
			continue;
		OpFontInfo* candidate = fontDatabase->GetFontInfo(idx);

		if (candidate->Vertical())
			continue;

		if (id == MONOSPACE)
		{
			if (candidate->Monospace())
				return idx;
		}
		else if (wantedSerifs != OpFontInfo::UnknownSerifs)
		{
			if (candidate->GetSerifs() == wantedSerifs)
				if (!candidate->Monospace())
					return idx;
				else
					cand_idx = idx;
		}
		// not much to do about fantasy and cursive
		else
		{
			if (!candidate->Monospace())
				return idx;
		}
	}

	if (cand_idx >= 0)
		return cand_idx;
	// mono/sans/serif mismatch: use first by alphabet
	return m_alphabetical_font_list[(ahem == 0 && numFonts > 1) ? 1 : 0];
}

#ifdef PERSCRIPT_GENERIC_FONT
void StyleManager::SetGenericFont(GenericFont id, const uni_char* font_name, WritingSystem::Script script)
#else
void StyleManager::SetGenericFont(GenericFont id, const uni_char* font_name)
#endif
{
	if (id == UNKNOWN)
		return;

#ifdef PERSCRIPT_GENERIC_FONT
	WritingSystem::Script locale = WritingSystem::Unknown;

	// Since currently css generic font pref can not be set per script, it should
	// just apply to all scripts but only if it's different with the default font.
	if (script == WritingSystem::NumScripts)
	{
		TRAPD(err, locale = WritingSystem::FromEncoding(g_op_system_info->GetSystemEncodingL()));
		const uni_char* default_font_name = fontManager->GetGenericFontName((::GenericFont)id, locale);
		if (font_name && default_font_name && uni_strcmp(font_name, default_font_name) == 0)
		{
			script = locale;
		}
	}
#endif // PERSCRIPT_GENERIC_FONT

	int i;
#ifdef HAS_FONT_FOUNDRY
	short original_number = GetFontNumber(font_name);
#else
	short original_number = -1;
	for (i = 0; (UINT32)i < fontDatabase->GetNumSystemFonts(); i++)
	{
		OpFontInfo* fontInfo = fontDatabase->GetFontInfo(i);
		if (fontInfo->GetFace() && font_name && uni_stricmp(fontInfo->GetFace(), font_name) == 0)
		{
			original_number = i;
			break;
		}
	}
#endif // HAS_FONT_FOUNDRY

#ifdef FONTSWITCHING
	BOOL force_fontswitch = FALSE;
#endif

	// if no prior default font is set try to find a fallback, but
	// don't overwrite what was set earlier
	if (original_number < 0 && generic_font_numbers[WritingSystem::Unknown][id] < 0)
	{
		original_number = FindFallback(id);
#ifdef FONTSWITCHING
		force_fontswitch = TRUE; // FindFallback doesn't choose a optimal font
#endif
	}
	// no such font or no fallback found
	if (original_number < 0)
		return;

#ifdef FONTSWITCHING
	OpFontInfo* original_font = fontDatabase->GetFontInfo(original_number);
#endif

#ifdef PERSCRIPT_GENERIC_FONT
	if (script != WritingSystem::NumScripts)
	{
# ifdef FONTSWITCHING
		OpFontInfo* font = GetFontForScript(original_font, script, force_fontswitch);
		if (font)
			SetGenericFont(id, font->GetFontNumber(), script);
		else
# endif // FONTSWITCHING
		SetGenericFont(id, original_number, script);
	}
	else
#endif // PERSCRIPT_GENERIC_FONT
	{
		for (i = 0; i < (int)WritingSystem::NumScripts; ++i)
		{
			const WritingSystem::Script script = (WritingSystem::Script)i;

#ifdef FONTSWITCHING
			OpFontInfo* font = GetFontForScript(original_font, script, force_fontswitch);
			if (font)
				SetGenericFont(id, font->GetFontNumber(), script);
			else
#endif // FONTSWITCHING
			SetGenericFont(id, original_number, script);

		}
	}
}

void StyleManager::SetGenericFont(GenericFont id, short font_number, WritingSystem::Script script)
{
	if (id == UNKNOWN || font_number < 0)
	{
		OP_ASSERT(!"SetGenericFont passed invalid data");
		return;
	}
	generic_font_numbers[script][id] = font_number;
}
short StyleManager::GetGenericFontNumber(const uni_char* fontface, WritingSystem::Script script) const
{
	return GetGenericFontNumber(GetGenericFont(fontface), script);
}
short StyleManager::GetGenericFontNumber(GenericFont id, WritingSystem::Script script) const
{
	if (id == UNKNOWN)
		return -1;
	short font_number = generic_font_numbers[script][id];
	return font_number;
}

OP_STATUS StyleManager::CreateFontNumber(const uni_char* family, int& fontnumber)
{
	// It's forbidden to overload the generic fontnames
	if (GetGenericFont(family) != UNKNOWN)
		return OpStatus::ERR;

	int n = fontDatabase->AllocateFontNumber();

	// There are only 13 bits available in the WordInfo struct,
	// so we need to limit here otherwise we'll get random fonts when it overflows
	if(n < (1 << 13))
	{
		fontnumber = n;
		return OpStatus::OK;
	}

	fontDatabase->ReleaseFontNumber(n);
	return OpStatus::ERR;
}

void StyleManager::ReleaseFontNumber(int fontnumber)
{
	fontDatabase->ReleaseFontNumber(fontnumber);
}

OP_STATUS StyleManager::AddWebFontInfo(OpFontInfo* fi)
{
	OP_STATUS stat = fontDatabase->AddFontInfo(fi);
	if (OpStatus::IsSuccess(stat))
	{
		fonts_in_hash = 0; // needs a refresh
	}
	return stat;
}

OP_STATUS StyleManager::RemoveWebFontInfo(OpFontInfo* fi)
{
	OP_STATUS stat = fontDatabase->RemoveFontInfo(fi);
	if (OpStatus::IsSuccess(stat))
	{
		fonts_in_hash = 0; // needs a refresh
	}
	return stat;
}

OP_STATUS StyleManager::DeleteWebFontInfo(OpFontInfo* fi, BOOL release_fontnumber)
{
	OP_STATUS stat = fontDatabase->DeleteFontInfo(fi, release_fontnumber);
	if (OpStatus::IsSuccess(stat))
	{
		fonts_in_hash = 0; // needs a refresh
	}
	return stat;
}

const int font_size_table[] = { 9, 10, 13, 16, 18, 24, 32, 48 };

int StyleManager::GetFontSize(const FontAtt* font, BYTE fsize) const
{
	if (!font)
	{
		OP_ASSERT(!"functionality changed - font need always be passed");
	}

	if (font)
	{
		int base_size = op_abs((int)font->GetHeight());

		if (fsize > 7)
			fsize = 7;

		int font_size = font_size_table[fsize];

		if (base_size != font_size_table[3])
		{
			// original: font_size = (int) (((0.8+font_size) * base_size) / font_size_table[3]);
			font_size = ((8 + font_size * 10) * base_size) / (font_size_table[3] * 10);
		}

		if (font_size < g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MinFontSize))
			font_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MinFontSize);
		else if (font_size > g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MaxFontSize))
			font_size = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::MaxFontSize);

		return font_size;
	}

	return -1;
}

int StyleManager::GetNextFontSize(const FontAtt* font, int current_size, BOOL smaller) const
{
	if (font)
	{
		int cur_size = current_size;
		int base_size = op_abs((int)font->GetHeight());
		if (base_size != font_size_table[3])
			cur_size = (int) (-0.8 + (cur_size * font_size_table[3]) / base_size);

		int fsize = 1;
		while (font_size_table[fsize] < cur_size && fsize < 7)
			fsize++;

		if (smaller && fsize > 1)
			fsize--;
		else if (!smaller && fsize < 7)
			fsize++;

		int font_size = font_size_table[fsize];

		if (base_size != font_size_table[3])
			font_size = (int) (((0.8+font_size) * base_size) / font_size_table[3]);

		if (font_size < 6)
			font_size = 6;

		return font_size;
	}

	return -1;
}

#ifdef FONTSWITCHING
short StyleManager::GetBestFont(const uni_char* str, int length, short font_number, const WritingSystem::Script script)
{
# ifndef PLATFORM_FONTSWITCHING
	FontSupportInfo font_info(font_number);
	int consumed;
#  if defined TEXTSHAPER_SUPPORT && defined USE_TEXTSHAPER_INTERNALLY
	TextShaper::ResetState();
#  endif // TEXTSHAPER_SUPPORT && USE_TEXTSHAPER_INTERNALLY
	// Use the same SwitchFont function as the layoutengine.
	BOOL SwitchFont(const uni_char* str, int length, int& consumed, FontSupportInfo& font_info, const WritingSystem::Script script);
	BOOL ret = SwitchFont(str, length, consumed, font_info, script);
	if (ret)
		return font_info.current_font->GetFontNumber();
# endif // PLATFORM_FONTSWITCHING
	return font_number;
}

static int unicode_block_cmp(const void* ap, const void* bp)
{
	const StyleManager::UnicodePointBlock *a = static_cast<const StyleManager::UnicodePointBlock*>(ap);
	const StyleManager::UnicodePointBlock *b = static_cast<const StyleManager::UnicodePointBlock*>(bp);
	return a->block_lowest - b->block_lowest;
}

void StyleManager::BuildOptimizedBlockTable()
{
	// Count unicode how many unicode blocks we have in the table
	int blocks_in_table_count = 0;
	for(;;)
	{
		UnicodePointBlock tmp_block;
		ReadUnicodeBlockLowestHighest(blocks_in_table_count, tmp_block);
		if (!tmp_block.block_highest) {
			// We found the table terminator
			break;
		}
		blocks_in_table_count++;
	};

	// Copy all unicode blocks to a new array for optimized lookup
	block_table_internal = OP_NEWA(UnicodePointBlock, blocks_in_table_count);
	if (!block_table_internal)
		return;

	for(block_table_internal_count = 0; block_table_internal_count < blocks_in_table_count; block_table_internal_count++)
		ReadUnicodeBlockLowestHighest(block_table_internal_count, block_table_internal[block_table_internal_count]);

	// Sort array after range so we can binary search in it
	op_qsort(block_table_internal, block_table_internal_count, sizeof(UnicodePointBlock), unicode_block_cmp);

	// Merge adjacent ranges with the same block number
	for(int i = 1; i < block_table_internal_count; i++)
	{
		if (block_table_internal[i - 1].block_number == block_table_internal[i].block_number &&
			block_table_internal[i - 1].block_highest + 1 == block_table_internal[i].block_lowest)
		{
			block_table_internal[i - 1].block_highest = block_table_internal[i].block_highest;
			op_memmove(&block_table_internal[i], &block_table_internal[i + 1], sizeof(UnicodePointBlock) * (block_table_internal_count - i - 1));
			block_table_internal_count--;
			i--;
		}
	}
}

void StyleManager::ReadUnicodeBlockLowestHighest(int table_index, UnicodePointBlock &block)
{
	UINT8 *index = block_table;
	index += table_index * 6;

	UINT8 plane = index[1];
	block.block_number = index[0];
#if defined OPERA_BIG_ENDIAN
	block.block_lowest = (index[2] << 8) | index[3];
	block.block_highest = (index[4] << 8) | index[5];
#else
	block.block_lowest = (index[3] << 8) | index[2];
	block.block_highest = (index[5] << 8) | index[4];
#endif // OPERA_BIG_ENDIAN

	block.block_lowest |= (plane << 16);
	block.block_highest |= (plane << 16);
}

void StyleManager::GetUnicodeBlockInfo(UnicodePoint ch, int &block_no, UnicodePoint &block_lowest, UnicodePoint &block_highest) {
	// don't report a block for control characters and such - see discussion in CORE-6634
	if (ch <= 0x20 ||
		ch == 0x7f ||
		(ch >= 0x80 && ch <= 0x9f) ||
		(ch >= 0xfe00 && ch <= 0xfe0f) ||
		(ch >= 0xfff0 && ch <= 0xffff)) // upper bound included since we might represent ch as a larger type eventually
	{
		block_no = UNKNOWN_BLOCK_NUMBER;
		return;
	}

	// Quick path for Basic Latin
	if (ch <= 0x007F)
	{
		block_lowest = 0;
		block_highest = 0x007F;
		block_no = 0;
		return;
	}

	if (block_table == NULL && block_table_present) {
		long size;
#if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined USE_OPENTYPE_DATA
		block_table = (UINT8*) g_table_manager->Get("uniblocks", size);
#elif defined EPOC // ENCODINGS_HAVE_TABLE_DRIVEN && USE_OPENTYPE_DATA
		OP_STATUS ret = ((CEpocFontManager*)fontManager)->GetUnicodeBlockTable(block_table);
		if (OpStatus::IsError(ret))
		{
			if (ret == OpStatus::ERR_NO_MEMORY)
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				OP_DELETE(block_table);
				block_table = NULL;
				return;
			}
		}
#else
# pragma PRAGMAMSG("No block_table available")
#endif // ENCODINGS_HAVE_TABLE_DRIVEN

		if (block_table == NULL)
		{
			block_table_present = FALSE;
 			block_no = UNKNOWN_BLOCK_NUMBER;
			block_lowest = 0;
			block_highest = MAX_UNICODE_POINT;
			return;
		}
		BuildOptimizedBlockTable();
    }

	if (block_table_internal != NULL) {
		// Binary search in block_table_internal
		UINT16 min = 0, max = block_table_internal_count;
		while (min < max)
		{
			UINT16 mid = (min + max) >> 1;

			UnicodePointBlock *block = &block_table_internal[mid];

			if (ch < block->block_lowest)
				max = mid;
			else if (ch > block->block_highest)
				min = mid + 1;
			else
			{
				block_no = block->block_number;
				block_lowest = block->block_lowest;
				block_highest = block->block_highest;
				return;
			}
		}
	}
	else if (block_table != NULL) {
		// Linear search over block_table, should only happen if OOM occured in BuildOptimizedBlockTable()
		UnicodeBlock block;
		UINT8 *index = block_table;
		do
		{
			block.block_number = index[0];
			UINT8 plane = index[1];
#if defined OPERA_BIG_ENDIAN
			block_lowest = (index[2] << 8) | index[3];
			block_highest = (index[4] << 8) | index[5];
#else
			block_lowest = (index[3] << 8) | index[2];
			block_highest = (index[5] << 8) | index[4];
#endif // OPERA_BIG_ENDIAN
			index+=6;

			block_lowest |= (plane << 16);
			block_highest |= (plane << 16);

			OP_ASSERT( (plane != 0) == (block_lowest   > 0xffff) );
			OP_ASSERT( (plane != 0) == (block_highest  > 0xffff) );

			if (ch >= block_lowest && ch <= block_highest)
			{
				block_no = block.block_number;
				return;
			}
		} while (block_highest);

	}

 	block_no = UNKNOWN_BLOCK_NUMBER;
	block_lowest = ch;
	block_highest = ch;
}

OP_STATUS StyleManager::SetPreferredFontForScript(UINT8 block_nr, BOOL monospace, const uni_char* font_name, BOOL replace)
{
	OP_STATUS status = OpStatus::OK;
	WritingSystem::Script script;
	switch (block_nr)
	{
	case 49: // Hiragana
	case 50: // Katakana
		script = WritingSystem::Japanese;
		break;

	case 55: // In the prefsfile this is treated as: Chinese simplifed
		script = WritingSystem::ChineseSimplified;
		status = styleManager->SetPreferredFont(59, monospace, font_name, script, replace);
		break;

	case 59: // In the prefsfile this is treated as: Chinese traditional
		script = WritingSystem::ChineseTraditional;
		status = styleManager->SetPreferredFont(55, monospace, font_name, script, replace);
		break;

	case 61: // In the prefsfile this is treated as: Kanji
		script = WritingSystem::Japanese;
		status = SetPreferredFont(54, monospace, font_name, script, replace);
		if (OpStatus::IsSuccess(status))
			status = SetPreferredFont(55, monospace, font_name, script, replace);
		if (OpStatus::IsSuccess(status))
			status = SetPreferredFont(59, monospace, font_name, script, replace);
		break;

	case 13: // Arabic, needs presentation forms as well
		script = WritingSystem::Arabic;
		status = SetPreferredFont(63, monospace, font_name, script, replace);
		if (OpStatus::IsSuccess(status))
			status = SetPreferredFont(67, monospace, font_name, script, replace);
		break;

	default:
		script = ScriptForBlock(block_nr);
		break;
	}
	if (OpStatus::IsSuccess(status))
		status = SetPreferredFont(block_nr, monospace, font_name, script, replace);
	return status;
}

/**
    Sets up the font switcher to override the system font for a specific unicode block.
    @param block_nr the block for which to override with the specified font
    @param monospace whether this font should be used for monospace or not
    @param font_name the name of the font
    @return status
	DOCUMENTME
*/

OP_STATUS StyleManager::SetPreferredFont(UINT8 block_nr, BOOL monospace, const uni_char* font_name, const WritingSystem::Script script, BOOL replace)
{
#ifdef HAS_FONT_FOUNDRY
	OpString fullname;
	if (font_name)
	{
		OP_STATUS result = fontManager->FindBestFontName(font_name, fullname);
		if (OpStatus::IsError(result))
		{
			if (result != OpStatus::ERR)
				return result;
		}
		else
		{
			if (fullname.Length() > 0)
				font_name = fullname.CStr();
		}
	}
#endif // HAS_FONT_FOUNDRY

#ifdef PLATFORM_FONTSWITCHING
	if (!block_table)
	{
		long size;
# if defined ENCODINGS_HAVE_TABLE_DRIVEN && defined USE_OPENTYPE_DATA
		block_table = (UINT8*)
			(g_table_manager ? g_table_manager->Get("uniblocks", size) : NULL);
# endif
	}
	if (block_table)
	{
# ifdef ENCODINGS_HAVE_TABLE_DRIVEN
		UnicodePoint ch = 0;
		UINT8 *b=block_table;
		while (TRUE)
		{
#  ifdef OPERA_BIG_ENDIAN
			int block_lowest = (b[2] << 8) | b[3];
			int block_highest = (b[4] << 8) | b[5];
#  else
			int block_lowest = (b[3] << 8) | b[2];
			int block_highest = (b[5] << 8) | b[4];
#  endif // OPERA_BIG_ENDIAN

			if (!block_highest)
				break;

			if (b[0] == block_nr)
			{
				ch = block_lowest;
				break;
			}
			b += 6;
		}
		if (ch)
			fontManager->SetPreferredFont(ch, monospace, font_name, OpFontInfo::CodePageFromScript(script));
# else // ENCODINGS_HAVE_TABLE_DRIVEN
		UnicodePoint ch = 0;
		for (UnicodeBlock *b=block_table; b->highest; b++)
		{
			if (b->block_number == block_nr)
			{
				ch = b->lowest;
				break;
			}
		}
		if (ch)
			fontManager->SetPreferredFont(ch, monospace, font_name, OpFontInfo::CodePageFromScript(script));
# endif // ENCODINGS_HAVE_TABLE_DRIVEN
	}
#endif // PLATFORM_FONTSWITCHING

	// Get preferred font entry in preferred fonts list.
	PreferredFont* pfont = (PreferredFont*)preferredFonts.First();
	while (pfont && !(pfont->block_nr == block_nr && pfont->script == script))
		pfont = (PreferredFont*)pfont->Suc();

	// if we're not to replace and there's already an entry, bail
	if (pfont && !replace && (monospace ? pfont->monospace_font_info : pfont->font_info))
		return OpStatus::OK;

	if (!font_name)
	{
		if (pfont)
		{
			if (monospace)
				pfont->monospace_font_info = NULL;
			else
				pfont->font_info = NULL;

			if (!pfont->font_info && !pfont->monospace_font_info)
			{
				pfont->Out();
				OP_DELETE(pfont);
			}
		}
		return OpStatus::OK;
	}

	// Find font info for the preferred font.
	for (UINT32 i = 0; i < fontDatabase->GetNumSystemFonts(); i++)
	{
		OpFontInfo* fontInfo = fontDatabase->GetFontInfo(i);
		BOOL add_font;
		switch (script)
		{
		case WritingSystem::ChineseSimplified:
		case WritingSystem::ChineseTraditional:
			add_font = fontInfo->HasScript(script) ||
					fontInfo->HasBlock(55) ||
					fontInfo->HasBlock(59);
			break;
		default:
			add_font = fontInfo->HasBlock(block_nr); break;
		}

		if (add_font && !fontInfo->Vertical() && fontInfo->GetFace() && (uni_stricmp(fontInfo->GetFace(), font_name) == 0))
		{
			if (!pfont)
			{
				pfont = OP_NEW(PreferredFont, (block_nr, script));
				if (!pfont)
					return OpStatus::ERR_NO_MEMORY;
				pfont->Into(&preferredFonts);
			}

			if (monospace)
				pfont->monospace_font_info = fontInfo;
			else
				pfont->font_info = fontInfo;
			break;
		}
	}
	return OpStatus::OK;
}

OpFontInfo* StyleManager::GetPreferredFont(UINT8 block_nr, BOOL monospace, const WritingSystem::Script script)
{
	PreferredFont* current = (PreferredFont*)preferredFonts.First();
	PreferredFont* fallback = NULL;

	while (current)
	{
		if (current->block_nr == block_nr)
		{
			if (current->script == script)
				return (monospace ? current->monospace_font_info : current->font_info);
			else if (current->script == WritingSystem::Unknown)
				fallback = current;
		}

		current = (PreferredFont*)current->Suc();
	}

	if (fallback)
		return (monospace ? fallback->monospace_font_info : fallback->font_info);

	return NULL;
}

#ifndef PLATFORM_FONTSWITCHING

// DEPRECATED
BOOL StyleManager::IsCodepageRelevant(UINT8 block_nr)
{
    switch (block_nr)
    {
    case 31: // general punctuation
    case 59: // CJK Unified Ideographs
    case 55: // CJK Compatibility
    case 48:
    case 49: // hiragana
    case 50: // katakana
        return TRUE;
    default:
        return FALSE;
    }
}

struct FontSwitchInfo
{
	FontSwitchInfo(const OpFontInfo* specified,
				   const WritingSystem::Script script, const int block, const UnicodePoint ch,
				   const int font_number)
		: specified(specified),
		  script(script), block(block), ch(ch),
		  font_number(font_number),
		  serifs(specified->GetSerifs()), monospace(specified->Monospace()),
		  match(0), match_script(0)
	{}

	/**
	   Gets total font score for fontCandidate, by calling
	   GetFontCoverageScore, GetFontScriptScore and GetFontStyleScore,
	   and applying some magic heuristic. match and match_script will
	   be updated with the total scores.
	 */
	BOOL         GetFontScore(OpFontInfo* fontCandidate);

	/**
	   Gets coverage score for fontCandidate. coverage score is a
	   value supposed to correspond to how apt the font is for
	   displaying ch and other glyphs in block.
	 */
	int GetFontCoverageScore(OpFontInfo* fontCandidate);
	/**
	   Gets style score for fontCandidate. style score corresponds to
	   how close fontCandidate is to specified, stylewise.
	 */
	int GetFontStyleScore(OpFontInfo* fontCandidate);

	/**
	   Sortof mimics script parts of scoring heuristic in GetFontScore.
	 */
	int GetFontScriptScore(OpFontInfo* fontCandidate);

	const OpFontInfo* const specified; ///< the specified font
	const WritingSystem::Script script; ///< the prefered script (based on page encoding, lang tag, etc)
	const int block; ///< corresponds to the ulUnicodeRange* entries in the OS/2 table (opentype spec)
	const UnicodePoint ch; ///< unicode point that's to be drawn
	const int font_number; ///< font number for doc root, to break ties

	const OpFontInfo::FontSerifs serifs; ///< serif type of the specified font
	const BOOL monospace; ///< whether the specified font is monospace

	// these should be set to the scores of the best match prior to
	// calling GetFontScore, and will be updated to the scores of
	// fontCandidate if GetFontScore returns true
	int match;
	int match_script;
};

inline
int FontSwitchInfo::GetFontCoverageScore(OpFontInfo* fontCandidate)
{
	OP_ASSERT(fontCandidate);
	if (fontCandidate->HasBlock(block))
	{
#ifdef _GLYPHTESTING_SUPPORT_
		if(ch && fontCandidate->HasGlyphTable(block))
		{
			if(fontCandidate->HasGlyph(ch))
				return 8;
		}
		else if (ch)
			// this can happen on eg OOM - a font we _know_ contain
			// the glyph's probably better than one we know _might_
			return 7;
		else
#endif // _GLYPHTESTING_SUPPORT_
			return 8;
	}
	return 0;
}

inline
int FontSwitchInfo::GetFontStyleScore(OpFontInfo* fontCandidate)
{
	OP_ASSERT(fontCandidate);
	int this_match = 0;
	if (serifs == OpFontInfo::UnknownSerifs || serifs == fontCandidate->GetSerifs())
		this_match++;

	if (monospace == fontCandidate->Monospace())
		this_match ++; // give extra weight to monospace equality

	return this_match;
}

inline
int FontSwitchInfo::GetFontScriptScore(OpFontInfo* fontCandidate)
{
	OP_ASSERT(fontCandidate);
	int score = fontCandidate->GetScriptPreference(script);
	if (score < 0)
		return score;
	if (fontCandidate->HasScript(script))
	    score += 4;
	return score;
}

// remember to set match and match_script in info to best before calling
inline
BOOL FontSwitchInfo::GetFontScore(OpFontInfo* fontCandidate)
{
	OP_ASSERT(fontCandidate);

	int this_match = 0;
	int this_match_script = fontCandidate->GetScriptPreference(script); // 1 - 4 points

	/* If this font has a negative script preference we really want to avoid this
	   font as much as possible, disregarding serifs/monospace etc (bug 185062) */

	if (this_match_script < 0)
	{
		this_match += this_match_script;
		this_match_script = 0;
	}

	if (match == 14 && this_match_script <= 0 && (font_number != (INT32)fontCandidate->GetFontNumber() || this_match_script < match_script))
	{
		// This font can never attain the magic 15 score, and thus never become our fallback font.
		// Since HasBlock & HasGlyphTable are potentially very time-consuming,
		// we might as well skip immediately.
		// This is very noticable for people who have installed the Adobe font libraries.
		return FALSE;
	}

	this_match += GetFontCoverageScore(fontCandidate);

	// If script preference has any points we should increase the matchpoint with 1. The script preference
	// point is later used as secondary priority between matches with the same matchpoint.
	// This is needed because otherwise the script preference will override lookpreferences set by f.ex CSS (like serifs).
	if (this_match_script > 0)
		this_match++;

	this_match += GetFontStyleScore(fontCandidate);

	if (fontCandidate->HasScript(script))
		this_match += 4; // give extra weight to script

	match = this_match;
	match_script = this_match_script;
	return TRUE;
}

UINT32 StyleManager::GetTotalFontCount()
{
	UINT32 tot = fontDatabase->GetNumSystemFonts();
#if !defined(WEBFONTS_SUPPORT) && defined(SVG_SUPPORT)
	tot += useSVGFonts && svgFontDatabase ? svgFontDatabase->GetNumFonts() + 1 : 0;
# endif // !WEBFONTS_SUPPORT && SVG_SUPPORT
	return tot;
}

OpFontInfo* StyleManager::NextFontAlphabetical(UINT32 i)
{
	const UINT32 idx =
#if !defined(WEBFONTS_SUPPORT) && defined(SVG_SUPPORT)
		i >= fontDatabase->GetNumSystemFonts() ? i :
#endif // !WEBFONTS_SUPPORT && SVG_SUPPORT
		m_alphabetical_font_list[i];
	OpFontInfo* fontCandidate = GetFontInfo(idx);

	if (!fontCandidate)
		return 0;

	// horrible hack, but it's probably not a good idea to font switch to ahem. ever.
	if ((int)idx == m_ahem_font_number)
		return 0;

	if (fontCandidate->Vertical()) //rg only look at horizontal fonts for now, as we don't support vertical layout yet.
		return 0;

	return fontCandidate;
}

OpFontInfo* StyleManager::GetRecommendedAlternativeFont(const OpFontInfo* specified_font, int block, WritingSystem::Script script, const UnicodePoint uc, BOOL use_preferred)
{
	BOOL monospace = specified_font->Monospace();
	OpFontInfo* alternative = 0;

	// check preferred font for combination of block and script
	if (use_preferred)
	{
		alternative = GetPreferredFont(block, monospace, script);
		if (alternative)
		{
#ifdef _GLYPHTESTING_SUPPORT_
			if (uc && alternative->HasGlyphTable(block) && !alternative->HasGlyph(uc))
				alternative = 0;
			else
#endif // _GLYPHTESTING_SUPPORT_
			return alternative;
		}
	}

	if (uc)
	{
		WritingSystem::Script block_script = ScriptForBlock(block);
		if ( block_script != WritingSystem::Unknown && script != block_script)
		{
			// the script is wrong at least for this character
			script = block_script;
		}
		else if ((block == 59 || block == 55 || block == 61) 
			&& script != WritingSystem::ChineseSimplified 
			&& script != WritingSystem::ChineseTraditional 
			&& script != WritingSystem::Japanese
			&& script != WritingSystem::Korean
			&& (locale_script == WritingSystem::ChineseSimplified
			|| locale_script == WritingSystem::ChineseTraditional
			|| locale_script == WritingSystem::Japanese
			|| locale_script == WritingSystem::Korean))
		{
			// Han character misdetected as Latin or other scripts, assume system locale
			script = locale_script;
		}
	}

	const int break_score = 15; // score that's "good enough" - i.e. we won't do any further looking

	Style* style = GetStyle(HE_DOC_ROOT);
	PresentationAttr p_attr = style->GetPresentationAttr();
	const int font_number = p_attr.GetPresentationFont(script).FontNumber;

	FontSwitchInfo info(specified_font,
						script, block, uc,
						font_number);

	// try to make an early bail with the specified font, or use as best until something better turns up
	OpFontInfo* fontCandidate = GetFontInfo(specified_font->GetFontNumber());
	int best_match = 0, best_match_script = 0;
	if (info.GetFontScore(fontCandidate))
	{
		if (info.match >= break_score)
			return fontCandidate; // search no further than necessary
		best_match = info.match;
		best_match_script = info.match_script;
		alternative = fontCandidate;
	}

	// check if it's possible to fall back to a generic font (from prefs)
	if (uc)
	{
		const GenericFont generic = GetGenericFontType(specified_font);
		const int gnum = GetGenericFontNumber(generic, script);
		if (gnum >= 0 && static_cast<UINT32>(gnum) != specified_font->GetFontNumber())
		{
			OpFontInfo* generic_font = GetFontInfo(gnum);
			if (generic_font && info.GetFontCoverageScore(generic_font))
				return generic_font;
		}
	}

	const UINT32 count = GetTotalFontCount();
	for (UINT32 i = 0; i < count; ++i)
	{
		fontCandidate = NextFontAlphabetical(i);
		if (!fontCandidate)
			continue;

		info.match = best_match;
		info.match_script = best_match_script;
		if (!info.GetFontScore(fontCandidate))
			continue;

		if (info.match > best_match ||
			(info.match == best_match &&
			 (info.match_script > best_match_script ||
			  (info.font_number == (int)fontCandidate->GetFontNumber() && info.match_script >= best_match_script))))
		{
			best_match = info.match;
			best_match_script = info.match_script;
			if (info.match >= break_score)
				return fontCandidate; // search no further than necessary
			alternative = fontCandidate;
		}
	}
	if (best_match < 6 || alternative == specified_font)
		return NULL;
	else
		return alternative;
}

#endif // !PLATFORM_FONTSWITCHING

#ifdef _GLYPHTESTING_SUPPORT_

BOOL StyleManager::ShouldHaveGlyph(UnicodePoint ch)
{
	if (ch == 0 ||	// null (empty string)
		ch == 9 ||  // tab
		ch == 10||	// lf
		ch == 13)	// cr
	{
		return FALSE;
	}
    return TRUE;
}

#endif // _GLYPHTESTING_SUPPORT_

#endif // FONTSWITCHING

void StyleManager::OnPrefsInitL()
{
	g_pcdisplay->RegisterListenerL(this);

	SetGenericFont(SERIF,      g_pcdisplay->GetStringPref(PrefsCollectionDisplay::CSSFamilySerif).CStr());
	SetGenericFont(SANS_SERIF, g_pcdisplay->GetStringPref(PrefsCollectionDisplay::CSSFamilySansserif).CStr());
	SetGenericFont(CURSIVE,    g_pcdisplay->GetStringPref(PrefsCollectionDisplay::CSSFamilyCursive).CStr());
	SetGenericFont(FANTASY,    g_pcdisplay->GetStringPref(PrefsCollectionDisplay::CSSFamilyFantasy).CStr());
	SetGenericFont(MONOSPACE,  g_pcdisplay->GetStringPref(PrefsCollectionDisplay::CSSFamilyMonospace).CStr());
}

/** Signals a change in an integer preference.
  *
  * @param id       Identity of the collection that has changed the value.
  * @param pref     Identity of the preference. This is the integer
  *                 representation of the enumeration value used by the
  *                 associated OpPrefsCollection.
  * @param newvalue The new value of the preference.
  */
void StyleManager::PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue)
{
	// NOP
}

/** Signals a change in a string preference.
  *
  * @param id       Identity of the collection that has changed the value.
  * @param pref     Identity of the preference. This is the integer
  *                 representation of the enumeration value used by the
  *                 associated OpPrefsCollection.
  * @param newvalue The new value of the preference.
  */
void StyleManager::PrefChanged(enum OpPrefsCollection::Collections id, int pref, const OpStringC &newvalue)
{
	if (id == OpPrefsCollection::Display)
	{
		switch(pref)
		{
		case PrefsCollectionDisplay::CSSFamilySerif:
			SetGenericFont(SERIF, newvalue.CStr());
			break;
		case PrefsCollectionDisplay::CSSFamilySansserif:
			SetGenericFont(SANS_SERIF, newvalue.CStr());
			break;
		case PrefsCollectionDisplay::CSSFamilyCursive:
			SetGenericFont(CURSIVE, newvalue.CStr());
			break;
		case PrefsCollectionDisplay::CSSFamilyFantasy:
			SetGenericFont(FANTASY, newvalue.CStr());
			break;
		case PrefsCollectionDisplay::CSSFamilyMonospace:
			SetGenericFont(MONOSPACE, newvalue.CStr());
			break;
		}
	}
}

#ifdef PREFS_HOSTOVERRIDE

void StyleManager::HostOverrideChanged(enum OpPrefsCollection::Collections id, const uni_char *hostname)
{
	// NOP
}
#endif // PREFS_HOSTOVERRIDE

#ifdef FONTSWITCHING
OpFontInfo* StyleManager::GetFontSupportingScript(OpFontInfo* specified_font, WritingSystem::Script script, BOOL force/* = FALSE*/)
{
	OP_ASSERT(specified_font);

	// check if specified_font cuts it
	if (!force && specified_font->HasScript(script))
		return specified_font;

	FontSwitchInfo info(specified_font, script, 0, 0, 0);

	// prefer generic fonts
	const GenericFont generic = GetGenericFontType(specified_font);
	const int gnum = GetGenericFontNumber(generic, script);
	if (gnum >= 0 && static_cast<UINT32>(gnum) != specified_font->GetFontNumber())
	{
		OpFontInfo* genericFont = GetFontInfo(gnum);
		if (genericFont && info.GetFontScriptScore(genericFont) > 0)
			return genericFont;
	}

	// loop over all system fonts and find the one closest to specified_font but supporting script
	OpFontInfo* result = specified_font;
	info.match        = info.GetFontStyleScore(result);
	info.match_script = info.GetFontScriptScore(result);
	const UINT32 fontCount = fontDatabase->GetNumSystemFonts();
	for (UINT32 i = 0; i < fontCount; ++i)
	{
		OpFontInfo* fontCandidate = NextFontAlphabetical(i);
		if (!fontCandidate)
			continue;

		const int script_score = info.GetFontScriptScore(fontCandidate);
		if (script_score < 0)
			continue;

		const int style_score = info.GetFontStyleScore(fontCandidate);

		if (script_score > info.match_script ||
		    (script_score == info.match_script && style_score > info.match))
		{
			result = fontCandidate;
			info.match_script = script_score;
			info.match = style_score;
		}
	}
	return result;
}

OpFontInfo* StyleManager::GetFontForScript(OpFontInfo* default_font, WritingSystem::Script script, BOOL force/* = FALSE*/)
{
	if (!force && default_font && default_font->HasScript(script))
		return default_font;

	// shouldn't change Unknown
	if (script != WritingSystem::Unknown)
	{
# ifndef PLATFORM_FONTSWITCHING
		if (default_font)
		{
			return GetFontSupportingScript(default_font, script);
		}
# endif // !PLATFORM_FONTSWITCHING
	}
	return 0;
}
#endif // FONTSWITCHING

#ifdef FONTSWITCHING
#ifndef PLATFORM_FONTSWITCHING
/** Declaration of deprecated function */
OpFontInfo* StyleManager::GetRecommendedAlternativeFont(const OpFontInfo* specified_font, int block,
											OpFontInfo::CodePage preferred_codepage, BOOL use_preferred)
{
	return GetRecommendedAlternativeFont(specified_font,block,preferred_codepage,(uni_char)0,use_preferred);
}
OpFontInfo* StyleManager::GetRecommendedAlternativeFont(const OpFontInfo* specified_font, int block, OpFontInfo::CodePage preferred_codepage, const uni_char uc, BOOL use_preferred)
{
	return GetRecommendedAlternativeFont(specified_font, block, OpFontInfo::ScriptFromCodePage(preferred_codepage), uc, use_preferred);
}
#endif // !PLATFORM_FONTSWITCHING
#endif // FONTSWITCHING

#ifdef PERSCRIPT_GENERIC_FONT
void StyleManager::InitGenericFont(GenericFont id)
{
	for (int i = 0; i < (int)WritingSystem::NumScripts; ++i)
	{
		const WritingSystem::Script script = (WritingSystem::Script)i;
		SetGenericFont(id, fontManager->GetGenericFontName((::GenericFont)id, script), script);
	}
}
#endif

WritingSystem::Script StyleManager::ScriptForBlock(UINT8 block_nr)
{
	switch(block_nr)
	{
	case 0:
	case 1:
	case 33:
		return WritingSystem::LatinWestern;
	case 2:
	case 3:
	case 29:
		return WritingSystem::LatinUnknown;
	case 7:
	case 30:
		return WritingSystem::Greek;
	case 9:
		return WritingSystem::Cyrillic;
	case 11:
		return WritingSystem::Hebrew;
	case 13:
		return WritingSystem::Arabic;
	case 15:
		return WritingSystem::IndicDevanagari;
	case 16:
		return WritingSystem::IndicBengali;
	case 17:
		return WritingSystem::IndicGurmukhi;
	case 18:
		return WritingSystem::IndicGujarati;
	case 19:
		return WritingSystem::IndicOriya;
	case 20:
		return WritingSystem::IndicTamil;
	case 21:
		return WritingSystem::IndicTelugu;
	case 22:
		return WritingSystem::IndicKannada;
	case 23:
		return WritingSystem::IndicMalayalam;
	case 24:
		return WritingSystem::Thai;
	case 25:
		return WritingSystem::IndicLao;
	case 49: // Hiragana
	case 50: // Katakana
		return WritingSystem::Japanese;
	case 56:
		return WritingSystem::Korean;
	case 73:
		return WritingSystem::IndicSinhala;
	case 74:
		return WritingSystem::IndicMyanmar;
	default:
		break;
	}
	return WritingSystem::Unknown;
}

