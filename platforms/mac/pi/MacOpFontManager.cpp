/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/FontInfo.h"
#include "platforms/mac/pi/MacOpFont.h"
#include "platforms/mac/pi/MacOpFontManager.h"
#include "platforms/mac/pi/MacOpSystemInfo.h"
#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/bundledefines.h"
#include "modules/display/fontdb.h"
#include "modules/display/styl_man.h"
#include "platforms/mac/model/MacFontIntern.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "modules/util/handy.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/style/css_webfont.h"
#include "modules/mdefont/processedstring.h"

extern BOOL gInternalFontSwitching;

BOOL SetOpString(OpString& dest, CFStringRef source);
OP_STATUS OpCopyStringRelease(uni_char*& dest, CFStringRef source);

OP_STATUS OpFontManager::Create(OpFontManager **newObj)
{
	*newObj = OP_NEW(MacOpFontManager, ());
	return *newObj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

MacOpFontManager::MacOpFontManager() : 
                                       m_antialisingThreshhold(0),
                                       m_intern(NULL),
                                       m_fonts(NULL)
{
#ifndef PERSCRIPT_GENERIC_FONT
	mGenericSerifNameScore = 0;
	mGenericSansSerifNameScore = 0;
	mGenericCursiveNameScore = 0;
	mGenericFantasyNameScore = 0;
	mGenericMonospaceNameScore = 0;
#endif // PERSCRIPT_GENERIC_FONT
	m_sysLanguage = GetSystemLanguage();

	// Set the antialising threshold (Note: This is just one setting for Opera or Opera Next)
	CFPropertyListRef aaThreshhold = CFPreferencesCopyAppValue(CFSTR("AppleAntiAliasingThreshold"), CFSTR(OPERA_BUNDLE_ID_STRING));
	
	if (aaThreshhold)
	{
		if (CFGetTypeID(aaThreshhold) == CFStringGetTypeID())
			m_antialisingThreshhold = CFStringGetIntValue((CFStringRef)aaThreshhold);
		else if (CFGetTypeID(aaThreshhold) == CFNumberGetTypeID())
			CFNumberGetValue((CFNumberRef)aaThreshhold, kCFNumberIntType, &m_antialisingThreshhold);
		
		CFRelease(aaThreshhold);
	}

}

MacOpFontManager::~MacOpFontManager()
{
	if (m_fonts)
		OP_DELETEA(m_fonts);
	if (m_intern)
		OP_DELETE(m_intern);
}

VEGAFont* MacOpFontManager::GetVegaFont(OpWebFontRef webfont, UINT32 size, INT32 blur_radius)
{
	WebFontContainer* font = (WebFontContainer*)webfont;
	return OP_NEW(MacOpFont, (this, font->GetFontRef(), font->GetCTFontRef(), (SInt16)size, 0, OpFontInfo::PLATFORM_WEBFONT, blur_radius));
}

VEGAFont* MacOpFontManager::GetVegaFont(OpWebFontRef webfont, UINT8 preferWeight, BOOL preferItalic, UINT32 size, INT32 blur_radius)
{
	WebFontContainer* font = (WebFontContainer*)webfont;
	UINT8 style = (preferWeight>5?bold:0)|(preferItalic?italic:0);
	return OP_NEW(MacOpFont, (this, font->GetFontRef(), font->GetCTFontRef(), (SInt16)size, style, OpFontInfo::PLATFORM_WEBFONT, blur_radius));
}

VEGAFont* MacOpFontManager::GetVegaFont(const uni_char* face, UINT32 size, UINT8 preferWeight, BOOL preferItalic, BOOL must_have_getoutline, INT32 blur_radius)
{
	if (face)
	{
		// Hiragino  Kaku Gothic, regular
		if ((uni_stricmp(face, UNI_L("\x30D2\x30E9\x30AE\x30CE\x89D2\x30B4 Pro W3")) == 0) && (preferWeight > 5)) {
			// Hiragino  Kaku Gothic, bold
			face = UNI_L("\x30D2\x30E9\x30AE\x30CE\x89D2\x30B4 Pro W6");
			preferWeight = 4;
		}
		// Hiragino Minchu, regular
		if ((uni_stricmp(face, UNI_L("\x30D2\x30E9\x30AE\x30CE\x660E\x671D Pro W3")) == 0) && (preferWeight > 5)) {
			// Hiragino Minchu, bold
			face = UNI_L("\x30D2\x30E9\x30AE\x30CE\x660E\x671D Pro W6");
			preferWeight = 4;
		}
		if ((uni_stricmp(face, UNI_L("Geeza Pro")) == 0) && (preferWeight > 5)) {
			face = UNI_L("Geeza Pro Bold");
			preferWeight = 4;
		}

		for(UINT32 i=0; i < m_intern->Count(); i++)
		{
			if(!uni_strcmp(m_intern->GetFontName(i), face))
			{
				ATSFontRef font = m_intern->GetFontIdentifier(i);
				CTFontRef fontRef = m_intern->GetFontRef(i);
				SInt16 style = 0;

				if (preferWeight > 5)
					style |= bold;

				if (preferItalic)
					style |= italic;

				MacOpFont *newFont = OP_NEW(MacOpFont, (this, font, fontRef, (SInt16)size, style, OpFontInfo::PLATFORM, blur_radius));

#ifdef PI_CAP_HAS_CREATEFONT_WITH_OUTLINES
				UINT32 dummypos;
				SVGNumber dummyadvance;
				if(newFont && !OpStatus::IsSuccess(newFont->GetOutline(NULL, 0, dummypos, 0, TRUE, dummyadvance, NULL)))
				{
					// The font didn't support GetOutline, so we must return NULL unless we can get another one.
					// It's not a huge problem, but we should return another GetOutline-capable font instead of NULL.
					OP_DELETE(newFont);
					newFont =  NULL;
				}
#endif // PI_CAP_HAS_CREATEFONT_WITH_OUTLINES
				
				return newFont;
			}
		}
	}
	
	return NULL;

}

ATSFontRef MacOpFontManager::GetFontIdentifier(UINT32 fontnr)
{
	return m_intern->GetFontIdentifier(fontnr);
}

UINT32 MacOpFontManager::CountFonts()
{
	return m_intern->Count();
}

#ifdef PLATFORM_FONTSWITCHING
void MacOpFontManager::SetPreferredFont(uni_char ch, BOOL monospace, const uni_char *font, OpFontInfo::CodePage codepage)
{
}
#endif

OP_STATUS MacOpFontManager::BeginEnumeration()
{
	m_intern = OP_NEW(MacFontIntern, ());
	RETURN_OOM_IF_NULL(m_intern);

	OP_NEW_DBG("MacFontManager::BeginEnumeration()", "Fonts");
	m_intern->SortFonts();

	return OpStatus::OK;
}

OP_STATUS MacOpFontManager::EndEnumeration()
{
	return OpStatus::OK;
}

#ifdef PERSCRIPT_GENERIC_FONT
void MacOpFontManager::ConsiderGenericFont(GenericFont generic_font, WritingSystem::Script script, const uni_char* name, int score)
{
	OpAutoVector<GenericFontInfo>* generic = NULL;
	unsigned int i;
	GenericFontInfo* info = NULL;
	switch (generic_font)
	{
		case GENERIC_FONT_SERIF:
			generic = &mGenericSerifFonts;
			break;
		case GENERIC_FONT_SANSSERIF:
			generic = &mGenericSansSerifFonts;
			break;
		case GENERIC_FONT_CURSIVE:
			generic = &mGenericCursiveFonts;
			break;
		case GENERIC_FONT_FANTASY:
			generic = &mGenericFantasyFonts;
			break;
		case GENERIC_FONT_MONOSPACE:
			generic = &mGenericMonospaceFonts;
			break;
	}
	if (generic) {
		for (i = 0; i<generic->GetCount(); i++) {
			info = generic->Get(i);
			if (info->script == script) {
				if (score > info->score) {
					info->name.Set(name);
					info->score = score;
				}
				return;
			}
		}
		info = OP_NEW(GenericFontInfo, ());
		info->name.Set(name);
		info->score = score;
		info->script = script;
		generic->Add(info);
	}
}

void MacOpFontManager::ConsiderGenericFont(GenericFont generic_font, const uni_char* name, int score)
{
	ConsiderGenericFont(generic_font, WritingSystem::Unknown, name, score);
}
#else
void MacOpFontManager::ConsiderGenericFont(GenericFont generic_font, const uni_char* name, int score)
{
	
	OP_NEW_DBG("MacOpFontManager::ConsiderGenericFont", "Fonts");
	
	switch (generic_font)
	{
		case GENERIC_FONT_SERIF:
			if (score > mGenericSerifNameScore) {
				mGenericSerifName.Set(name);
				mGenericSerifNameScore = score;
				OP_DBG((UNI_L("Generic font serif: %s - (score %d)"), name, score));
			}
			break;
		case GENERIC_FONT_SANSSERIF:
			if (score > mGenericSansSerifNameScore) {
				mGenericSansSerifName.Set(name);
				mGenericSansSerifNameScore = score;
				OP_DBG((UNI_L("Generic font sans serif: %s - (score %d)"), name, score));
			}
			break;
		case GENERIC_FONT_CURSIVE:
			if (score > mGenericCursiveNameScore) {
				mGenericCursiveName.Set(name);
				mGenericCursiveNameScore = score;
				OP_DBG((UNI_L("Generic font cursive: %s - (score %d)"), name, score));
			}
			break;
		case GENERIC_FONT_FANTASY:
			if (score > mGenericFantasyNameScore) {
				mGenericFantasyName.Set(name);
				mGenericFantasyNameScore = score;
				OP_DBG((UNI_L("Generic font fantasy: %s - (score %d)"), name, score));
			}
			break;
		case GENERIC_FONT_MONOSPACE:
			if (score > mGenericMonospaceNameScore) {
				mGenericMonospaceName.Set(name);
				mGenericMonospaceNameScore = score;
				OP_DBG((UNI_L("Generic font monospace: %s - (score %d)"), name, score));
			}
			break;
	}
}
#endif

#ifdef PERSCRIPT_GENERIC_FONT
const uni_char* MacOpFontManager::GetGenericFontName(GenericFont generic_font, WritingSystem::Script script)
{
	OpAutoVector<GenericFontInfo>* generic = NULL;
	unsigned int i;
	GenericFontInfo* info;
	switch (generic_font)
	{
		case GENERIC_FONT_SERIF:
			generic = &mGenericSerifFonts;
			break;
		case GENERIC_FONT_SANSSERIF:
			generic = &mGenericSansSerifFonts;
			break;
		case GENERIC_FONT_CURSIVE:
			generic = &mGenericCursiveFonts;
			break;
		case GENERIC_FONT_FANTASY:
			generic = &mGenericFantasyFonts;
			break;
		case GENERIC_FONT_MONOSPACE:
			generic = &mGenericMonospaceFonts;
			break;
	}
	if (generic) {
retry:
		for (i = 0; i<generic->GetCount(); i++) {
			info = generic->Get(i);
			if (info->script == script)
				return info->name.CStr();
		}
		if (script == WritingSystem::ChineseUnknown) {
			script = (m_sysLanguage==kSystemLanguageSimplifiedChinese ? WritingSystem::ChineseSimplified : WritingSystem::ChineseTraditional);
			goto retry;
		}
		if (script != WritingSystem::Unknown) {
			script = WritingSystem::Unknown;
			goto retry;
		}
	}
	return (UNI_L("Helvetica"));
}
#else // !PERSCRIPT_GENERIC_FONT
const uni_char* MacOpFontManager::GetGenericFontName(GenericFont generic_font)
{
	switch (generic_font)
	{
		case GENERIC_FONT_SERIF:
			if (mGenericSerifName.HasContent())
				return mGenericSerifName.CStr();
			break;
		case GENERIC_FONT_SANSSERIF:
			if (mGenericSansSerifName.HasContent())
				return mGenericSansSerifName.CStr();
			break;
		case GENERIC_FONT_CURSIVE:
			if (mGenericCursiveName.HasContent())
				return mGenericCursiveName.CStr();
			break;
		case GENERIC_FONT_FANTASY:
			if (mGenericFantasyName.HasContent())
				return mGenericFantasyName.CStr();
			break;
		case GENERIC_FONT_MONOSPACE:
			if (mGenericMonospaceName.HasContent())
				return mGenericMonospaceName.CStr();
			break;
	}
	return (UNI_L("Helvetica"));
}
#endif // PERSCRIPT_GENERIC_FONT

OP_STATUS MacOpFontManager::GetWebFontInfo(OpWebFontRef webfont, OpFontInfo* fontinfo)
{
	WebFontContainer* font = (WebFontContainer*) webfont;
	if (font)
	{
		CFStringRef str = NULL;
		OSStatus err = noErr;
		OpString name;
		if (font->GetCTFontRef())
			str = CTFontCopyName(font->GetCTFontRef(), kCTFontFullNameKey);
		else
			err = ATSFontGetName(font->GetFontRef(), 0, &str);
		if (str && err == noErr)
		{
			SetOpString(name, str);
			fontinfo->SetFace(name);
			CFRelease(str);
		}

		fontinfo->SetFontNumber(0);
		
		// Dummy Info.
		fontinfo->SetMonospace(FALSE);
		fontinfo->SetItalic(FALSE);
		fontinfo->SetOblique(FALSE);
		fontinfo->SetNormal(TRUE);
		fontinfo->SetSmallcaps(FALSE);
		fontinfo->SetSerifs(font->GetSerifs());
		fontinfo->SetVertical(FALSE);
		fontinfo->SetWeight(4, TRUE);
		fontinfo->SetWeight(7, TRUE);
		
		const FONTSIGNATURE *signatures = font->GetFontSignatures();
		
		for (int i=0; i<126; i++)
			if (signatures->fsUsb[i / 32] & (1 << (i % 32)))
				fontinfo->SetBlock(i, TRUE);
			else
				fontinfo->SetBlock(i, FALSE);
		
		fontinfo->SetBlock(126,FALSE);
		fontinfo->SetBlock(127,FALSE);
		
		const uint32* characterMap = font->GetCharacterMap();
		if (characterMap) {
			for (int i = 0; i < 2048; i++) {
				for (int j = 0; j < 32; j++) {
					if (characterMap[i] & (1 << j)) {
						fontinfo->SetGlyph(i*32+j);
					}
				}
			}
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

OP_STATUS MacOpFontManager::GetFontInfo(UINT32 fontnr, OpFontInfo* fontinfo)
{
#ifdef WEBFONTS_SUPPORT
	WebFontContainer* font = NULL;
# ifdef PI_CAP_PLATFORM_WEBFONT_HANDLES
	UINTPTR webfont = fontinfo->GetWebFont();
	if (webfont)
		font = (WebFontContainer*) webfont;
# else
	int count = mInstalledWebFonts.GetCount();
	for (int i = 0; i<count; i++)
	{
		if (mInstalledWebFonts.Get(i)->GetFontNumber() == fontnr)
		{
			font = mInstalledWebFonts.Get(i);
			break;
		}
	}
# endif
	if (font)
	{
		CFStringRef str = NULL;
		OSStatus err = noErr;
		OpString name;
		if (font->GetCTFontRef())
			str = CTFontCopyName(font->GetCTFontRef(), kCTFontFullNameKey);
		else
			err = ATSFontGetName(font->GetFontRef(), 0, &str);
		if (str && err == noErr)
		{
			SetOpString(name, str);
			fontinfo->SetFace(name);
			CFRelease(str);
		}
		fontinfo->SetFontNumber(fontnr);

		// Dummy Info.
		fontinfo->SetMonospace(FALSE);
		fontinfo->SetItalic(FALSE);
		fontinfo->SetOblique(FALSE);
		fontinfo->SetNormal(TRUE);
		fontinfo->SetSmallcaps(FALSE);
		fontinfo->SetSerifs(font->GetSerifs());
		fontinfo->SetVertical(FALSE);
		fontinfo->SetWeight(4, TRUE);
		fontinfo->SetWeight(7, TRUE);

		const FONTSIGNATURE *signatures = font->GetFontSignatures();

		for (int i=0; i<126; i++)
			if (signatures->fsUsb[i / 32] & (1 << (i % 32)))
				fontinfo->SetBlock(i, TRUE);
			else
				fontinfo->SetBlock(i, FALSE);

		fontinfo->SetBlock(126,FALSE);
		fontinfo->SetBlock(127,FALSE);

		const uint32* characterMap = font->GetCharacterMap();
		if (characterMap) {
			for (int i = 0; i < 2048; i++) {
				for (int j = 0; j < 32; j++) {
					if (characterMap[i] & (1 << j)) {
						fontinfo->SetGlyph(i*32+j);
					}
				}
			}
			return OpStatus::OK;
		}
	}
#endif // WEBFONTS_SUPPORT
	{
		fontinfo->SetFace(m_intern->GetFontName(fontnr));
		fontinfo->SetFontNumber(fontnr);
		fontinfo->SetMonospace(m_intern->GetIsMonospace(fontnr));
		fontinfo->SetItalic(m_intern->GetSupportItalic(fontnr));
		fontinfo->SetOblique(m_intern->GetSupportOblique(fontnr));
		fontinfo->SetNormal(m_intern->GetSupportNormal(fontnr));
		fontinfo->SetSmallcaps(m_intern->GetSupportSmallCaps(fontnr));
#ifndef LOAD_SERIFS_ON_DEMAND
		fontinfo->SetSerifs(m_intern->GetHasSerifs(fontnr));
#endif
		fontinfo->SetVertical(FALSE);
		fontinfo->SetWeight(4, TRUE);
		fontinfo->SetWeight(7, TRUE);

		for(int i=0; i<128; i++)
			fontinfo->SetBlock(i, FALSE);

#ifndef DELAYED_CODEPAGE_CALCULATION
		FONTSIGNATURE *signatures = m_intern->GetFontSignatures(fontnr);

		for(int i=0; i<126; i++)
			if(signatures->fsUsb[i / 32] & (1 << (i % 32)))
				fontinfo->SetBlock(i, TRUE);

#ifdef REMOVE_BAD_CODEPAGES
		fontinfo->SetBlock(37, FALSE);	// Only Symbol, STKaiti or STHeiti should do this.
		fontinfo->SetBlock(39, FALSE);	// Trust STKaiti, STHeiti and Lucida on these.
#endif

#ifdef _GLYPHTESTING_SUPPORT_
		uint32* characterMap = m_intern->GetCharacterMap(fontnr);
		if (characterMap) {
			for (int i = 0; i < 2048; i++) {
				for (int j = 0; j < 32; j++) {
					if (characterMap[i] & (1 << j)) {
						fontinfo->SetGlyph(i*32+j);
					}
				}
			}
		}
#endif

		if (!signatures->fsUsb[0] &&
			!signatures->fsUsb[1] &&
			!signatures->fsUsb[2] &&
			!signatures->fsUsb[3] &&
			!signatures->fsCsb[0])
		{
			fontinfo->SetBlock(0,TRUE);
			fontinfo->SetBlock(1,TRUE);
		}
#endif // DELAYED_CODEPAGE_CALCULATION
		InternalFontID id = m_intern->GetInternalFontID(fontnr);
		fontinfo->SetScript(WritingSystem::LatinWestern);
		fontinfo->SetScript(WritingSystem::LatinEastern);

			/********* JAPANESE FONTS: Default for all unknown content on Japanese systems, default for japanese content on other systems *********/
			// "Hiragino Kaku Gothic Pro W3"
		if (kInternalFontIDHiraginoKakuGothic == id) {
			fontinfo->SetScript(WritingSystem::Japanese);
			fontinfo->SetScriptPreference(WritingSystem::Japanese, 3);
			if (m_sysLanguage == kSystemLanguageJapanese) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 3);
				ConsiderGenericFont(GENERIC_FONT_SANSSERIF, fontinfo->GetFace(), 4);
				ConsiderGenericFont(GENERIC_FONT_MONOSPACE, fontinfo->GetFace(), 5);
				ConsiderGenericFont(GENERIC_FONT_CURSIVE, fontinfo->GetFace(), 4);
				ConsiderGenericFont(GENERIC_FONT_FANTASY, fontinfo->GetFace(), 4);
			} else {
#ifndef DELAYED_CODEPAGE_CALCULATION
				fontinfo->SetBlock(13, FALSE);	// Do not use for Arabic
				fontinfo->SetBlock(56, FALSE);	// Do not use for Hangul
#endif
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, WritingSystem::Japanese, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, WritingSystem::Japanese, fontinfo->GetFace(), 5);
			ConsiderGenericFont(GENERIC_FONT_CURSIVE, WritingSystem::Japanese, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_FANTASY, WritingSystem::Japanese, fontinfo->GetFace(), 4);
#endif
		}
			// "Hiragino Mincho Pro W3"
		else if (kInternalFontIDHiraginoMincho == id) {
			fontinfo->SetScript(WritingSystem::Japanese);
			fontinfo->SetScriptPreference(WritingSystem::Japanese, 3);
			if (m_sysLanguage == kSystemLanguageJapanese) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 3);
				ConsiderGenericFont(GENERIC_FONT_SERIF, fontinfo->GetFace(), 4);
			} else {
#ifndef DELAYED_CODEPAGE_CALCULATION
					// Long story short: If we disable the scrips that Times doesn't have, they will go to Lucida, instead of this Serif font.
				fontinfo->SetBlock(9, FALSE);	// Not Cyrillic
				fontinfo->SetBlock(10, FALSE);	// Not Armenian
				fontinfo->SetBlock(11, FALSE);	// Not Hebrew
				fontinfo->SetBlock(13, FALSE);	// Do not use for Arabic
				fontinfo->SetBlock(56, FALSE);	// And not Hangul
#endif
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SERIF, WritingSystem::Japanese, fontinfo->GetFace(), 4);
#endif
		}

		// "Hiragino Maru Gothic Pro W4"
		else if (kInternalFontIDHiraginoMaruGothicProW4 == id) {
			fontinfo->SetScript(WritingSystem::Japanese);
			fontinfo->SetScriptPreference(WritingSystem::Japanese, 3);
			if (m_sysLanguage == kSystemLanguageJapanese) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 3);
				ConsiderGenericFont(GENERIC_FONT_CURSIVE, fontinfo->GetFace(), 5);
				ConsiderGenericFont(GENERIC_FONT_FANTASY, fontinfo->GetFace(), 5);
			} else {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_CURSIVE, WritingSystem::Japanese, fontinfo->GetFace(), 5);
			ConsiderGenericFont(GENERIC_FONT_FANTASY, WritingSystem::Japanese, fontinfo->GetFace(), 5);
#endif
		}

			/********* KOREAN FONTS: Default for all unknown content on Korean systems, default for korean content on other systems *********/
		else if (kInternalFontIDAppleGothic == id) {
			fontinfo->SetScript(WritingSystem::Korean);
			fontinfo->SetScriptPreference(WritingSystem::Korean, 3);
			if (m_sysLanguage == kSystemLanguageKorean) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 3);
				ConsiderGenericFont(GENERIC_FONT_SANSSERIF, fontinfo->GetFace(), 4);
				ConsiderGenericFont(GENERIC_FONT_MONOSPACE, fontinfo->GetFace(), 5);
			} else {
#ifndef DELAYED_CODEPAGE_CALCULATION
				fontinfo->SetBlock(13, FALSE);	// Do not use for Arabic
				fontinfo->SetBlock(49, FALSE);	// Attempt not to use this for Japanese
				fontinfo->SetBlock(50, FALSE);
#endif
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, WritingSystem::Korean, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, WritingSystem::Korean, fontinfo->GetFace(), 5);
#endif
		}
		else if (kInternalFontIDAppleMyungjo == id) {
			fontinfo->SetScript(WritingSystem::Korean);
			fontinfo->SetScriptPreference(WritingSystem::Korean, 3);
			if (m_sysLanguage == kSystemLanguageKorean) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 3);
				ConsiderGenericFont(GENERIC_FONT_SERIF, fontinfo->GetFace(), 4);
				ConsiderGenericFont(GENERIC_FONT_CURSIVE, fontinfo->GetFace(), 4);
				ConsiderGenericFont(GENERIC_FONT_FANTASY, fontinfo->GetFace(), 4);
			} else {
#ifndef DELAYED_CODEPAGE_CALCULATION
					// Long story short: If we disable the scrips that Times doesn't have, they will go to Lucida, instead of this Serif font.
				fontinfo->SetBlock(9, FALSE);	// Not Cyrillic
				fontinfo->SetBlock(10, FALSE);	// Not Armenian
				fontinfo->SetBlock(11, FALSE);	// Not Hebrew
				fontinfo->SetBlock(13, FALSE);	// Do not use for Arabic
				fontinfo->SetBlock(49, FALSE);	// Attempt not to use this for Japanese
				fontinfo->SetBlock(50, FALSE);
#endif
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SERIF, WritingSystem::Korean, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_CURSIVE, WritingSystem::Korean, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_FANTASY, WritingSystem::Korean, fontinfo->GetFace(), 4);
#endif
		}

			/********* SIMPLIFIED CHINESE FONTS *********/

		// "Heiti SC Light", sans serif
		else if (kInternalFontIDHeitiSCLight == id) {
			fontinfo->SetScript(WritingSystem::ChineseSimplified);
			fontinfo->SetScriptPreference(WritingSystem::ChineseSimplified, 4);
			fontinfo->SetScriptPreference(WritingSystem::LatinWestern, 1);
			if (m_sysLanguage == kSystemLanguageSimplifiedChinese) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 3);
				ConsiderGenericFont(GENERIC_FONT_SANSSERIF, fontinfo->GetFace(), 4);
				ConsiderGenericFont(GENERIC_FONT_MONOSPACE, fontinfo->GetFace(), 4);
			} else if (m_sysLanguage != kSystemLanguageTraditionalChinese) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, WritingSystem::ChineseSimplified, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, WritingSystem::ChineseSimplified, fontinfo->GetFace(), 4);
#endif
		}
		// "STXihei", sans serif
		else if (kInternalFontIDSTXihei == id) {
			fontinfo->SetScript(WritingSystem::ChineseSimplified);
			fontinfo->SetScriptPreference(WritingSystem::ChineseSimplified, 4);
			if (m_sysLanguage == kSystemLanguageSimplifiedChinese) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 3);
				ConsiderGenericFont(GENERIC_FONT_SANSSERIF, fontinfo->GetFace(), 4);
				ConsiderGenericFont(GENERIC_FONT_MONOSPACE, fontinfo->GetFace(), 4);
			} else if (m_sysLanguage != kSystemLanguageTraditionalChinese) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, WritingSystem::ChineseSimplified, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, WritingSystem::ChineseSimplified, fontinfo->GetFace(), 4);
#endif
		}
			// "STKaiti", serif
		else if (kInternalFontIDSTKaiti == id) {
			fontinfo->SetScript(WritingSystem::ChineseSimplified);
			fontinfo->SetScriptPreference(WritingSystem::ChineseSimplified, 4);
			if (m_sysLanguage == kSystemLanguageSimplifiedChinese) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 3);
				ConsiderGenericFont(GENERIC_FONT_CURSIVE, fontinfo->GetFace(), 5);
				ConsiderGenericFont(GENERIC_FONT_FANTASY, fontinfo->GetFace(), 5);
			} else if (m_sysLanguage != kSystemLanguageTraditionalChinese) {
#ifndef DELAYED_CODEPAGE_CALCULATION
					// Long story short: If we disable the scrips that Times doesn't have, they will go to Lucida, instead of this Serif font.
				fontinfo->SetBlock(9, FALSE);	// Not Cyrillic
				fontinfo->SetBlock(10, FALSE);	// Not Armenian
				fontinfo->SetBlock(11, FALSE);	// Not Hebrew
				fontinfo->SetBlock(13, FALSE);	// Do not use for Arabic
				fontinfo->SetBlock(49, FALSE);	// Attempt not to use this for Japanese
				fontinfo->SetBlock(50, FALSE);
				fontinfo->SetBlock(56, FALSE);	// Do not use for Hangul
#endif
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_CURSIVE, WritingSystem::ChineseSimplified, fontinfo->GetFace(), 5);
			ConsiderGenericFont(GENERIC_FONT_FANTASY, WritingSystem::ChineseSimplified, fontinfo->GetFace(), 5);
#endif
#if !defined(DELAYED_CODEPAGE_CALCULATION) && defined(REMOVE_BAD_CODEPAGES)
			fontinfo->SetBlock(37, TRUE);
			fontinfo->SetBlock(39, TRUE);
#endif
		}
			// "STSong", serif
		else if (kInternalFontIDSTSong == id) {
			fontinfo->SetScript(WritingSystem::ChineseSimplified);
			fontinfo->SetScriptPreference(WritingSystem::ChineseSimplified, 3);
			if (m_sysLanguage == kSystemLanguageSimplifiedChinese) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 3);
				ConsiderGenericFont(GENERIC_FONT_SERIF, fontinfo->GetFace(), 4);
			} else if (m_sysLanguage != kSystemLanguageTraditionalChinese) {
#ifndef DELAYED_CODEPAGE_CALCULATION
					// Long story short: If we disable the scrips that Times doesn't have, they will go to Lucida, instead of this Serif font.
				fontinfo->SetBlock(9, FALSE);	// Not Cyrillic
				fontinfo->SetBlock(10, FALSE);	// Not Armenian
				fontinfo->SetBlock(11, FALSE);	// Not Hebrew
				fontinfo->SetBlock(13, FALSE);	// Do not use for Arabic
				fontinfo->SetBlock(49, FALSE);	// Attempt not to use this for Japanese
				fontinfo->SetBlock(50, FALSE);
				fontinfo->SetBlock(56, FALSE);	// Do not use for Hangul
#endif
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SERIF, WritingSystem::ChineseSimplified, fontinfo->GetFace(), 4);
#endif
#if !defined(DELAYED_CODEPAGE_CALCULATION) && defined(REMOVE_BAD_CODEPAGES)
			fontinfo->SetBlock(37, TRUE);
			fontinfo->SetBlock(39, TRUE);
#endif
		}

			/********* TRADITIONAL CHINESE FONTS *********/
		
		else if (kInternalFontIDHeitiTCLight == id) {
			fontinfo->SetScript(WritingSystem::ChineseTraditional);
			fontinfo->SetScriptPreference(WritingSystem::ChineseTraditional, 4);
			if (m_sysLanguage == kSystemLanguageTraditionalChinese) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 4);
				ConsiderGenericFont(GENERIC_FONT_SANSSERIF, fontinfo->GetFace(), 1);
				ConsiderGenericFont(GENERIC_FONT_MONOSPACE, fontinfo->GetFace(), 1);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, WritingSystem::ChineseTraditional, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, WritingSystem::ChineseTraditional, fontinfo->GetFace(), 4);
#endif
		}
		else if (kInternalFontIDLiHei == id) {
			fontinfo->SetScript(WritingSystem::ChineseTraditional);
			fontinfo->SetScriptPreference(WritingSystem::ChineseTraditional, 4);
			if (m_sysLanguage == kSystemLanguageTraditionalChinese) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 4);
				ConsiderGenericFont(GENERIC_FONT_SANSSERIF, fontinfo->GetFace(), 1);
				ConsiderGenericFont(GENERIC_FONT_MONOSPACE, fontinfo->GetFace(), 1);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, WritingSystem::ChineseTraditional, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, WritingSystem::ChineseTraditional, fontinfo->GetFace(), 4);
#endif
		}
		else if (kInternalFontIDLiSong == id) {
			fontinfo->SetScript(WritingSystem::ChineseTraditional);
			fontinfo->SetScriptPreference(WritingSystem::ChineseTraditional, 4);
			if (m_sysLanguage == kSystemLanguageTraditionalChinese) {
//				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 3);
				ConsiderGenericFont(GENERIC_FONT_SERIF, fontinfo->GetFace(), 1);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SERIF, WritingSystem::ChineseTraditional, fontinfo->GetFace(), 4);
#endif
		}
		else if (kInternalFontIDBiauKai == id) {
			fontinfo->SetScript(WritingSystem::ChineseTraditional);
			fontinfo->SetScriptPreference(WritingSystem::ChineseTraditional, 4);
			if (m_sysLanguage == kSystemLanguageTraditionalChinese) {
				ConsiderGenericFont(GENERIC_FONT_CURSIVE, fontinfo->GetFace(), 4);
				ConsiderGenericFont(GENERIC_FONT_FANTASY, fontinfo->GetFace(), 4);
			}
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_CURSIVE, WritingSystem::ChineseTraditional, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_FANTASY, WritingSystem::ChineseTraditional, fontinfo->GetFace(), 4);
#endif
		}

			/********* THAI FONTS *********/

		else if (kInternalFontIDAyuthaya == id) {
			fontinfo->SetScript(WritingSystem::Thai);
			fontinfo->SetScriptPreference(WritingSystem::Thai, 1);
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, WritingSystem::Thai, fontinfo->GetFace(), 4);
#endif
		}
		else if (kInternalFontIDThonburi == id) {
			fontinfo->SetScript(WritingSystem::Thai);
			fontinfo->SetScriptPreference(WritingSystem::Thai, 3);
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, WritingSystem::Thai, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_SERIF, WritingSystem::Thai, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_CURSIVE, WritingSystem::Thai, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_FANTASY, WritingSystem::Thai, fontinfo->GetFace(), 4);
#endif
		}

			/********* WESTERN FONTS: Default for all unknown content on non-CJK systems *********/
		else if (kInternalFontIDLucidaGrande == id) {
			fontinfo->SetScriptPreference(WritingSystem::LatinWestern, 1);
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, fontinfo->GetFace(), 3);
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, WritingSystem::Hebrew, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_SERIF, WritingSystem::Hebrew, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_FANTASY, WritingSystem::Cyrillic, fontinfo->GetFace(), 3);
			ConsiderGenericFont(GENERIC_FONT_FANTASY, WritingSystem::Hebrew, fontinfo->GetFace(), 3);
#endif
#if !defined(DELAYED_CODEPAGE_CALCULATION) && defined(REMOVE_BAD_CODEPAGES)
			fontinfo->SetBlock(37, TRUE);
			fontinfo->SetBlock(39, TRUE);
#endif
		}
		else if (kInternalFontIDTimes == id) {
			if (m_sysLanguage == kSystemLanguageUnknown) {
				fontinfo->SetScriptPreference(WritingSystem::LatinWestern, 3);
			} else {
				fontinfo->SetScriptPreference(WritingSystem::LatinWestern, 1);
			}
			ConsiderGenericFont(GENERIC_FONT_SERIF, fontinfo->GetFace(), 3);
		}

			/********* OTHER SCRIPTS *********/
		else if (kInternalFontIDGeezaPro == id) {
			fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, WritingSystem::Arabic, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_SERIF, WritingSystem::Arabic, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_CURSIVE, WritingSystem::Arabic, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_FANTASY, WritingSystem::Arabic, fontinfo->GetFace(), 4);
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, WritingSystem::Arabic, fontinfo->GetFace(), 4);
#endif
		}
		else if (kInternalFontIDMshtakan == id) {
			fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
		}
		else if (kInternalFontIDDevanagariMT == id) {
			fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
		}
		else if (kInternalFontIDGurmukhiMT == id) {
			fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
		}
		else if (kInternalFontIDGujaratiMT == id) {
			fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
		}
		else if (kInternalFontIDPlantagenetCherokee == id) {
			fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
		}
		else if (kInternalFontIDInaiMathi == id) {
			fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
		}
		else if (kInternalFontIDEuphemiaUCAS == id) {
			fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
		}

			/****** Other fonts worthy of consideration ******/
				/* Serif */
		else if (kInternalFontIDTimesNewRoman == id) {
			ConsiderGenericFont(GENERIC_FONT_SERIF, fontinfo->GetFace(), 2);
		}
		else if (kInternalFontIDNewYork == id) {
			ConsiderGenericFont(GENERIC_FONT_SERIF, fontinfo->GetFace(), 1);
		}
				/* Sans-Serif */
		else if (kInternalFontIDArial == id) {
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, fontinfo->GetFace(), 2);
		}
		else if (kInternalFontIDGeneva == id) {
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, fontinfo->GetFace(), 1);
		}
		else if (kInternalFontIDHelvetica == id) {
			if (m_sysLanguage == kSystemLanguageUnknown) {
				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 3);
			} else {
				fontinfo->SetScriptPreference(OpFontInfo::ScriptFromCodePage(OpFontInfo::OP_DEFAULT_CODEPAGE), 1);
			}
			ConsiderGenericFont(GENERIC_FONT_SANSSERIF, fontinfo->GetFace(), 4);
		}
				/* Cursive */
		else if (kInternalFontIDAppleChancery == id) {
			ConsiderGenericFont(GENERIC_FONT_CURSIVE, fontinfo->GetFace(), 3);
		}
		else if (kInternalFontIDSand == id) {
			ConsiderGenericFont(GENERIC_FONT_CURSIVE, fontinfo->GetFace(), 1);
		}
				/* Monospace */
		else if (kInternalFontIDCourierNew == id) {
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, fontinfo->GetFace(), 1);
		}
		else if (kInternalFontIDMonaco == id) {
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, fontinfo->GetFace(), 3);
#ifdef PERSCRIPT_GENERIC_FONT
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, WritingSystem::Cyrillic, fontinfo->GetFace(), 2);
#endif
		}
		else if (kInternalFontIDCourier == id) {
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, fontinfo->GetFace(), 2);
		}
		// Apple Menlo is default monospace font on Mac OS X 10.6
		else if (kInternalFontIDAppleMenlo == id) {
			ConsiderGenericFont(GENERIC_FONT_MONOSPACE, fontinfo->GetFace(), 4);
		}
				/* Fantacy */
		else if (kInternalFontIDComicSansMS == id) {
			ConsiderGenericFont(GENERIC_FONT_FANTASY, fontinfo->GetFace(), 2);
		}
		else if (kInternalFontIDPapyrus == id) {
			ConsiderGenericFont(GENERIC_FONT_FANTASY, fontinfo->GetFace(), 3);
		}

#if !defined(DELAYED_CODEPAGE_CALCULATION) && defined(REMOVE_BAD_CODEPAGES)
		else if (kInternalFontIDSymbol == id) {
			fontinfo->SetBlock(37, TRUE);
			fontinfo->SetBlock(39, TRUE);
		}
#endif

#ifndef DELAYED_CODEPAGE_CALCULATION
		BOOL has_punctuation = FALSE;
		BOOL has_chinese_punctuation = FALSE;
		BOOL has_currency = FALSE;
		BOOL has_geometric_shapes = FALSE;
		BOOL has_mathematical_operators = FALSE;

		for(int i=0; i<22; i++)
		{
			if(signatures->fsCsb[0] & (1 << i))
			{
				switch(i)
				{
					case 0:
					case 1:
						has_punctuation = TRUE;
						has_currency = TRUE;
						break;

					case 17:
					case 18:
					case 20:
						has_punctuation = TRUE;
						has_chinese_punctuation = TRUE;
						has_geometric_shapes = TRUE;
						has_mathematical_operators = TRUE;
						break;
				}
			}
		}

		if(has_punctuation)
			fontinfo->SetBlock(31, TRUE);

		if(has_chinese_punctuation)
		{
			fontinfo->SetBlock(48, TRUE);
			fontinfo->SetBlock(68, TRUE);
		}

		if(has_currency)
			fontinfo->SetBlock(33, TRUE);

		if(has_geometric_shapes)
			fontinfo->SetBlock(45, TRUE);

		if(has_mathematical_operators)
			fontinfo->SetBlock(38, TRUE);

		fontinfo->SetCodepage(OpFontInfo::OP_DEFAULT_CODEPAGE);

//		if(signatures->fsCsb[0] & UNICODE_CODEPAGE_MASK_JAPANESE)
		if ((signatures->fsUsb[1] & UNICODE_RANGE_MASK_HIRAGANA) &&
			(signatures->fsUsb[1] & UNICODE_RANGE_MASK_KATAKANA))
			fontinfo->SetCodepage(OpFontInfo::OP_JAPANESE_CODEPAGE);
			//Should also check for non-korean

		if(signatures->fsCsb[0] & UNICODE_CODEPAGE_MASK_SIMPLIFIEDCHINESE)
			fontinfo->SetCodepage(OpFontInfo::OP_SIMPLIFIED_CHINESE_CODEPAGE);

		if(signatures->fsCsb[0] & UNICODE_CODEPAGE_MASK_TRADITIONALCHINESE)
			fontinfo->SetCodepage(OpFontInfo::OP_TRADITIONAL_CHINESE_CODEPAGE);

		if(signatures->fsCsb[0] & UNICODE_CODEPAGE_MASK_KOREAN)
			fontinfo->SetCodepage(OpFontInfo::OP_KOREAN_CODEPAGE);

		if(signatures->fsCsb[0] & UNICODE_CODEPAGE_MASK_CYRILLIC)
			fontinfo->SetCodepage(OpFontInfo::OP_CYRILLIC_CODEPAGE);
#endif // DELAYED_CODEPAGE_CALCULATION

		return OpStatus::OK;
	}
}

#ifdef _GLYPHTESTING_SUPPORT_
void MacOpFontManager::UpdateGlyphMask(OpFontInfo *fontinfo)
{
#ifdef DELAYED_CODEPAGE_CALCULATION
	UINT32 fontnr = fontinfo->GetFontNumber();
#ifdef WEBFONTS_SUPPORT
# ifdef PI_CAP_PLATFORM_WEBFONT_HANDLES
	UINTPTR webfont = fontinfo->GetWebFont();
	if (webfont)
	{
		WebFontContainer* font = (WebFontContainer*) webfont;
		const FONTSIGNATURE *signatures = font->GetFontSignatures();

		for (int i=0; i<126; i++)
			if (signatures->fsUsb[i / 32] & (1 << (i % 32)))
				fontinfo->SetBlock(i, TRUE);
			else
				fontinfo->SetBlock(i, FALSE);

		fontinfo->SetBlock(126,FALSE);
		fontinfo->SetBlock(127,FALSE);

		const uint32* characterMap = font->GetCharacterMap();
		if (characterMap) {
			for (int i = 0; i < 2048; i++) {
				for (int j = 0; j < 32; j++) {
					if (characterMap[i] & (1 << j)) {
						fontinfo->SetGlyph(i*32+j);
					}
				}
			}
		}
	}
#else
	int count = mInstalledWebFonts.GetCount();
	for (int i = 0; i<count; i++)
	{
		WebFontContainer* font = mInstalledWebFonts.Get(i);
		if (font->GetFontNumber() == fontnr)
		{
			// Don't update this font!
			return;
		}
	}
# endif
#endif // WEBFONTS_SUPPORT
	uint32* characterMap = m_intern->GetCharacterMap(fontnr);
	if (!characterMap)
		return;

#ifdef _FONT_MASK_DEBUG
	static int dbg_glyph_upd = 0;
	DecOnScreen(0, 0, kOnScreenBlack, kOnScreenWhite, ++dbg_glyph_upd);
#endif

	for(int i=0; i<128; i++)
		fontinfo->SetBlock(i, FALSE);
//		fontinfo->SetBlock(i, TRUE);

	FONTSIGNATURE *signatures = m_intern->GetFontSignatures(fontnr);

	for(int i=0; i<126; i++)
		if(signatures->fsUsb[i / 32] & (1 << (i % 32)))
			fontinfo->SetBlock(i, TRUE);

#ifdef REMOVE_BAD_CODEPAGES
	fontinfo->SetBlock(37, FALSE);	// Only Symbol should do this. Really.
	fontinfo->SetBlock(39, FALSE);
#endif

	InternalFontID id = m_intern->GetInternalFontID(fontnr);

	bool non_arabic_font = false;
	if (kInternalFontIDArial == id ||
		kInternalFontIDCourierNew == id ||
		kInternalFontIDTahoma == id ||
		kInternalFontIDTimesNewRoman == id ||
		kInternalFontIDArialUnicodeMS == id ||
		kInternalFontIDMicrosoftSansSerif == id) {
		non_arabic_font = true;
		fontinfo->SetBlock(13, FALSE);
	}

#ifdef _GLYPHTESTING_SUPPORT_
	if (characterMap) {
		for (int i = 0; i < 2048; i++) {
			if (non_arabic_font && (i>= 48 && i<=55) || i==59) continue;	//U+0600-U+06FF, U+0760-U+077F
			for (int j = 0; j < 32; j++) {
				if (non_arabic_font && i==58 && j>=16) continue;			//U+075x
				if (characterMap[i] & (1 << j)) {
					fontinfo->SetGlyph(i*32+j);
				}
			}
		}
	}
#endif

	if (!signatures->fsUsb[0] &&
		!signatures->fsUsb[1] &&
		!signatures->fsUsb[2] &&
		!signatures->fsUsb[3] &&
		!signatures->fsCsb[0])
	{
		fontinfo->SetBlock(0,TRUE);
		fontinfo->SetBlock(1,TRUE);
	}


		/********* JAPANESE FONTS: Default for all unknown content on Japanese systems, default for japanese content on other systems *********/
		// "Hiragino Kaku Gothic Pro W3", Standard on Jaguar (Safari 1.0)
	if (kInternalFontIDHiraginoKakuGothic == id) {
		if (m_sysLanguage != kSystemLanguageJapanese) {
			fontinfo->SetBlock(13, FALSE);	// Do not use for Arabic
			fontinfo->SetBlock(56, FALSE);	// Do not use for Hangul
		}
	}
		// "Hiragino Mincho Pro W3", Standard on Panther (Safari 1.2)
	else if (kInternalFontIDHiraginoMincho == id) {
		if (m_sysLanguage != kSystemLanguageJapanese) {
				// Long story short: If we disable the scrips that Times doesn't have, they will go to Lucida, instead of this Serif font.
			fontinfo->SetBlock(9, FALSE);	// Not Cyrillic
			fontinfo->SetBlock(10, FALSE);	// Not Armenian
			fontinfo->SetBlock(11, FALSE);	// Not Hebrew
			fontinfo->SetBlock(13, FALSE);	// Do not use for Arabic
			fontinfo->SetBlock(56, FALSE);	// And not Hangul
		}
	}

		/********* KOREAN FONTS: Default for all unknown content on Korean systems, default for korean content on other systems *********/
	else if (kInternalFontIDAppleGothic == id) {
		if (m_sysLanguage != kSystemLanguageKorean) {
			fontinfo->SetBlock(13, FALSE);	// Do not use for Arabic
			fontinfo->SetBlock(49, FALSE);	// Attempt not to use this for Japanese
			fontinfo->SetBlock(50, FALSE);
		}
	}
	else if (kInternalFontIDAppleMyungjo == id) {
		if (m_sysLanguage != kSystemLanguageKorean) {
				// Long story short: If we disable the scrips that Times doesn't have, they will go to Lucida, instead of this Serif font.
			fontinfo->SetBlock(9, FALSE);	// Not Cyrillic
			fontinfo->SetBlock(10, FALSE);	// Not Armenian
			fontinfo->SetBlock(11, FALSE);	// Not Hebrew
			fontinfo->SetBlock(13, FALSE);	// Do not use for Arabic
			fontinfo->SetBlock(49, FALSE);	// Attempt not to use this for Japanese
			fontinfo->SetBlock(50, FALSE);
		}
	}

		/********* SIMPLIFIED CHINESE FONTS *********/

		// "STHeiti", sans serif
	else if (kInternalFontIDSTHeiti == id) {
		if (m_sysLanguage != kSystemLanguageTraditionalChinese) {
			fontinfo->SetBlock(13, FALSE);	// Do not use for Arabic
			fontinfo->SetBlock(49, FALSE);	// Attempt not to use this for Japanese
			fontinfo->SetBlock(50, FALSE);
			fontinfo->SetBlock(56, FALSE);	// Do not use for Hangul
		}
#ifdef REMOVE_BAD_CODEPAGES
		fontinfo->SetBlock(37, TRUE);
		fontinfo->SetBlock(39, TRUE);
#endif
	}
		// "STKaiti", serif
	else if (kInternalFontIDSTKaiti == id) {
		if (m_sysLanguage != kSystemLanguageTraditionalChinese) {
				// Long story short: If we disable the scrips that Times doesn't have, they will go to Lucida, instead of this Serif font.
			fontinfo->SetBlock(9, FALSE);	// Not Cyrillic
			fontinfo->SetBlock(10, FALSE);	// Not Armenian
			fontinfo->SetBlock(11, FALSE);	// Not Hebrew
			fontinfo->SetBlock(13, FALSE);	// Do not use for Arabic
			fontinfo->SetBlock(49, FALSE);	// Attempt not to use this for Japanese
			fontinfo->SetBlock(50, FALSE);
			fontinfo->SetBlock(56, FALSE);	// Do not use for Hangul
		}
#ifdef REMOVE_BAD_CODEPAGES
		fontinfo->SetBlock(37, TRUE);
		fontinfo->SetBlock(39, TRUE);
#endif
	}

#ifdef REMOVE_BAD_CODEPAGES
	else if (kInternalFontIDLucidaGrande == id) {
		fontinfo->SetBlock(37, TRUE);
		fontinfo->SetBlock(39, TRUE);
	}
	else if (kInternalFontIDSymbol == id) {
		fontinfo->SetBlock(37, TRUE);
		fontinfo->SetBlock(39, TRUE);
	}
#endif

	BOOL has_punctuation = FALSE;
	BOOL has_chinese_punctuation = FALSE;
	BOOL has_currency = FALSE;
	BOOL has_geometric_shapes = FALSE;
	BOOL has_mathematical_operators = FALSE;

	for(int i=0; i<22; i++)
	{
		if(signatures->fsCsb[0] & (1 << i))
		{
			switch(i)
			{
				case 0:
				case 1:
					has_punctuation = TRUE;
					has_currency = TRUE;
					break;

				case 17:
				case 18:
				case 20:
					has_punctuation = TRUE;
					has_chinese_punctuation = TRUE;
					has_geometric_shapes = TRUE;
					has_mathematical_operators = TRUE;
					break;
			}
		}
	}

	if(has_punctuation)
		fontinfo->SetBlock(31, TRUE);

	if(has_chinese_punctuation)
	{
		fontinfo->SetBlock(48, TRUE);
		fontinfo->SetBlock(68, TRUE);
	}

	if(has_currency)
		fontinfo->SetBlock(33, TRUE);

	if(has_geometric_shapes)
		fontinfo->SetBlock(45, TRUE);

	if(has_mathematical_operators)
		fontinfo->SetBlock(38, TRUE);

	fontinfo->SetScript(WritingSystem::Unknown);

	if ((signatures->fsUsb[1] & UNICODE_RANGE_MASK_HIRAGANA) &&
		(signatures->fsUsb[1] & UNICODE_RANGE_MASK_KATAKANA))
		fontinfo->SetScript(WritingSystem::Japanese);
		//Should also check for non-korean

	if(signatures->fsCsb[0] & UNICODE_CODEPAGE_MASK_SIMPLIFIEDCHINESE)
		fontinfo->SetScript(WritingSystem::ChineseSimplified);

	if(signatures->fsCsb[0] & UNICODE_CODEPAGE_MASK_TRADITIONALCHINESE)
		fontinfo->SetScript(WritingSystem::ChineseTraditional);

	if(signatures->fsCsb[0] & UNICODE_CODEPAGE_MASK_KOREAN)
		fontinfo->SetScript(WritingSystem::Korean);

	if(signatures->fsCsb[0] & UNICODE_CODEPAGE_MASK_CYRILLIC)
		fontinfo->SetScript(WritingSystem::Cyrillic);
#endif // DELAYED_CODEPAGE_CALCULATION
}
#endif // _GLYPHTESTING_SUPPORT_

#ifdef PI_UPDATE_SERIFS
#ifdef LOAD_SERIFS_ON_DEMAND
void MacOpFontManager::UpdateSerifness(OpFontInfo *fontinfo)
{
	fontinfo->SetSerifs(m_intern->GetHasSerifs(fontinfo->GetFontNumber()));
}
#endif // LOAD_SERIFS_ON_DEMAND
#endif // PI_UPDATE_SERIFS

int MacOpFontManager::GetAntialisingTreshhold()
{
	return m_antialisingThreshhold;	
}

OP_STATUS MacOpFontManager::AddWebFont(OpWebFontRef& webfont, const uni_char* full_path_of_file)
{
	WebFontContainer* font = OP_NEW(WebFontContainer, (full_path_of_file, 0));
	if (!font || (!font->GetFontRef() && !font->GetCTFontRef()))
	{
		OP_DELETE(font);
		webfont = NULL;
		return OpStatus::ERR;
	}
	mInstalledWebFonts.Add(font);
	webfont = (OpWebFontRef)font;
	return OpStatus::OK;
}

OP_STATUS MacOpFontManager::RemoveWebFont(OpWebFontRef webfont)
{
	WebFontContainer* font = (WebFontContainer*)webfont;
	if (font)
		mInstalledWebFonts.RemoveByItem(font);
	OP_DELETE(font);
	return OpStatus::OK;
}

OP_STATUS MacOpFontManager::GetLocalFont(OpWebFontRef& localfont, const uni_char* facename)
{
	if (facename)
	{
		for(UINT32 i=0; i < m_intern->Count(); i++)
		{
			if(!uni_strcmp(m_intern->GetFontName(i), facename))
			{
				ATSFontRef font = m_intern->GetFontIdentifier(i);
				CTFontRef ctfont = m_intern->GetFontRef(i);
				localfont = (OpWebFontRef)OP_NEW(WebFontContainer, (font, ctfont, 0));
				return localfont ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
			}
		}
	}
	return OpStatus::ERR;
}

BOOL MacOpFontManager::SupportsFormat(int format)
{
	return (format & (CSS_WebFont::FORMAT_TRUETYPE | CSS_WebFont::FORMAT_TRUETYPE_AAT | CSS_WebFont::FORMAT_OPENTYPE)) != 0;
}

/***********************************************************************************
 ** Class WebFontContainer
 **
 ** 
 ***********************************************************************************/
#ifdef WEBFONTS_SUPPORT

WebFontContainer::WebFontContainer(const uni_char* path, UINT32 fontno) : fontcontainer(0), 
																		  font(0), 
																		  ctFont(0), 
																		  fontnumber(fontno),
																	      data(NULL),
																		  characterMap(NULL),
																		  italic(-1.0)
{
	bool font_created = false;
	if (OpStatus::IsSuccess(file.Construct(path)))
	{
		OpFileLength len = 0;
		if (OpStatus::IsSuccess(file.GetFileLength(len)))
		{
			if (OpStatus::IsSuccess(file.Open(OPFILE_READ)))
			{
				data = malloc(len);
				OpFileLength dummy;
				if (OpStatus::IsSuccess(file.Read(data, len, &dummy)))
				{
					if (GetOSVersion() >= 0x1060)
					{
						CFStringRef fontFileName = CFStringCreateWithCharacters(kCFAllocatorDefault, (const UniChar*)path, uni_strlen(path));
						if (fontFileName)
						{
							CFURLRef fontURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, fontFileName, kCFURLPOSIXPathStyle, false);
							if (fontURL)
							{
								CFErrorRef regerr;
								CTFontManagerRegisterFontsForURL(fontURL, kCTFontManagerScopeProcess, &regerr);
#ifdef CORE_TEXT_WEBFONT_MANAGEMENT
								CFArrayRef fdarray = CTFontManagerCreateFontDescriptorsFromURL(fontURL);
								if (fdarray)
								{
									CTFontDescriptorRef fd = (CTFontDescriptorRef)CFArrayGetValueAtIndex(fdarray, 0);
									CTFontRef font = CTFontCreateWithFontDescriptor(fd, 0.0, NULL);
									CFStringRef str = CTFontCopyName(font, kCTFontFullNameKey);
									if (font && str && CFStringCompare(str, CFSTR("LastResort"), 0) != kCFCompareEqualTo)
									{
										BOOL isSerifs;
										if (MacFontIntern::ParseOS2Tag(font, isSerifs))
											serifs = isSerifs ? OpFontInfo::WithSerifs : OpFontInfo::WithoutSerifs;
										MacFontIntern::ParseCMAPTables(font, &characterMap);
										MacFontIntern::GenerateUsb(signature, characterMap);
										italic = CTFontGetSlantAngle(font);
										font_created = true;
										ctFont = font;
									}
									CFRelease(fdarray);
									if (str)
										CFRelease(str);
								}
#endif
								CFRelease(fontURL);
							}
							CFRelease(fontFileName);
						}
					}
					if (!font_created)
					{
						/* Fall back to ATSUI */
						if (ATSFontActivateFromMemory(data, len, 3, kATSFontFormatUnspecified, NULL, kATSOptionFlagsDefault, &fontcontainer) != noErr)
							fontcontainer = 0;
						if (fontcontainer)
						{
							ItemCount count;
							if ((ATSFontFindFromContainer(fontcontainer, kNilOptions, 1, &font, &count) != noErr) || (count < 1))
								font = 0;
							if (font)
							{
								BOOL isSerifs;
								if (MacFontIntern::ParseOS2Tag(font, isSerifs))
									serifs = isSerifs ? OpFontInfo::WithSerifs : OpFontInfo::WithoutSerifs;
								MacFontIntern::ParseCMAPTables(font, &characterMap);
								MacFontIntern::GenerateUsb(signature, characterMap);
								ATSFontMetrics metrics;
								if (ATSFontGetHorizontalMetrics(fontcontainer, kATSOptionFlagsDefault, &metrics) == noErr)
									italic = metrics.italicAngle;
							}
						}
					}
				}
			}
		}
	}
}

WebFontContainer::WebFontContainer(ATSFontRef font, CTFontRef ctFont, UINT32 fontno) : fontcontainer(0),
																	 font(font),
																	 ctFont(ctFont),
																	 fontnumber(fontno),
																	 data(NULL),
																	 characterMap(NULL),
																	 italic(-1.0)
{
	if (ctFont)
	{
		CFRetain(ctFont);
		BOOL isSerifs;
		if (MacFontIntern::ParseOS2Tag(ctFont, isSerifs))
		{
			serifs = isSerifs ? OpFontInfo::WithSerifs : OpFontInfo::WithoutSerifs;
		}
		MacFontIntern::ParseCMAPTables(ctFont, &characterMap);
		MacFontIntern::GenerateUsb(signature, characterMap);
		italic = CTFontGetSlantAngle(ctFont);
	}
	else if (font)
	{
		BOOL isSerifs;
		if (MacFontIntern::ParseOS2Tag(font, isSerifs))
		{
			serifs = isSerifs ? OpFontInfo::WithSerifs : OpFontInfo::WithoutSerifs;
		}
		MacFontIntern::ParseCMAPTables(font, &characterMap);
		MacFontIntern::GenerateUsb(signature, characterMap);
		ATSFontMetrics metrics;
		if (ATSFontGetHorizontalMetrics(fontcontainer, kATSOptionFlagsDefault, &metrics) == noErr)
			italic = metrics.italicAngle;
	}
}

WebFontContainer::~WebFontContainer()
{
	if (ctFont)
		CFRelease(ctFont);

	free(data);
}

const uint32* WebFontContainer::GetCharacterMap()
{
	if (!characterMap)
	{
		MacFontIntern::ParseCMAPTables(ctFont, &characterMap);
	}

	return characterMap;
}

#endif // WEBFONTS_SUPPORT

#if !defined(MDEFONT_MODULE)
OP_STATUS MDF_ProcessedGlyphBuffer::Grow(const size_t size, BOOL copy_contents/* = FALSE*/)
{
	if (m_size >= size)
		return OpStatus::OK;
	
	ProcessedGlyph* tmp = OP_NEWA(ProcessedGlyph, size);
	if (!tmp)
		return OpStatus::ERR_NO_MEMORY;
	
	if (copy_contents)
		op_memcpy(tmp, m_buffer, m_size * sizeof(*m_buffer));

	OP_DELETEA(m_buffer);
	m_size = size;
	m_buffer = tmp;
	return OpStatus::OK;
}

MDF_ProcessedGlyphBuffer::~MDF_ProcessedGlyphBuffer()
{
	OP_DELETEA(m_buffer);
}

#endif
