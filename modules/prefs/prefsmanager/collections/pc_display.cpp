/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/erasettings.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefs/prefsmanager/hostoverride.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/display/styl_man.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"
#include "modules/windowcommander/WritingSystem.h"

/** Pack two chars in an int. */
#define STR2INT16(x,y) ((x << 8) | y)

#include "modules/prefs/prefsmanager/collections/pc_display_c.inl"

PrefsCollectionDisplay *PrefsCollectionDisplay::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcdisplay)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcdisplay = OP_NEW_L(PrefsCollectionDisplay, (reader));
	return g_opera->prefs_module.m_pcdisplay;
}

PrefsCollectionDisplay::~PrefsCollectionDisplay()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCDISPLAY_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCDISPLAY_NUMBEROFINTEGERPREFS);
#endif

	if (m_forceEncoding)
		g_charsetManager->DecrementCharsetIDReference(m_forceEncoding);
	if (m_defaultHTMLEncoding)
		g_charsetManager->DecrementCharsetIDReference(m_defaultHTMLEncoding);

	g_opera->prefs_module.m_pcdisplay = NULL;
}

const uni_char *PrefsCollectionDisplay::GetDefaultStringPref(stringpref which) const
{
	// Need to handle special cases here

	enum GenericFont font;

	switch (which)
	{
	case CSSFamilySerif:
		font = GENERIC_FONT_SERIF;
		break;

	case CSSFamilySansserif:
		font = GENERIC_FONT_SANSSERIF;
		break;

	case CSSFamilyCursive:
		font = GENERIC_FONT_CURSIVE;
		break;

	case CSSFamilyFantasy:
		font = GENERIC_FONT_FANTASY;
		break;

	case CSSFamilyMonospace:
		font = GENERIC_FONT_MONOSPACE;
		break;

	case ForceEncoding:
		return m_default_forceEncoding.CStr();

	case DefaultEncoding:
		return m_default_fallback_encoding.CStr();

	default:
		return m_stringprefdefault[which].defval;
	}

	// Retrieve platform default for this kind of font
	OpFontManager *fontManager = styleManager->GetFontManager();
	return fontManager->GetGenericFontName(font);
}

#if defined PREFS_HAVE_STRING_API || defined PREFS_WRITE
const uni_char *PrefsCollectionDisplay::GetDefaultStringInternal(int which, const struct stringprefdefault *)
{
	return GetDefaultStringPref(stringpref(which));
}
#endif // PREFS_HAVE_STRING_API

void PrefsCollectionDisplay::ReadAllPrefsL(PrefsModule::PrefsInitInfo *info)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCDISPLAY_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCDISPLAY_NUMBEROFINTEGERPREFS);

	// Fill in default values for CSS families if none were loaded
	// (We don't know the actual order of the stringpref enums)
	for (int i = 0; i <= int(DummyLastStringPref); ++ i)
	{
		switch (i) {
			case CSSFamilySerif:
			case CSSFamilySansserif:
			case CSSFamilyCursive:
			case CSSFamilyFantasy:
			case CSSFamilyMonospace:
				if (m_stringprefs[i].IsEmpty())
				{
					m_stringprefs[i].SetL(GetDefaultStringPref(stringpref(i)));
				}
				break;
		}
	}

	const uni_char* user_languages = info->m_user_languages.CStr();

	WritingSystem::Script user_script;
	if (user_languages)
	{
		user_script = WritingSystem::FromLanguageCode(user_languages);

		// Set default autodetection mode from the appropriate user language
		// settings
		m_default_forceEncoding.SetL(CharsetDetector::AutoDetectStringFromId(CharsetDetector::AutoDetectIdFromWritingSystem(user_script)));
	}
	else
	{
		user_script = WritingSystem::Unknown;
	}

	// Use the default value if none was set in ini file.
#ifdef PREFS_READ
	if (!m_reader->IsKey(SectionNumberToStringInternal(m_stringprefdefault[ForceEncoding].section),
	                     m_stringprefdefault[ForceEncoding].key))
#endif
	{
		m_stringprefs[ForceEncoding].SetL(m_default_forceEncoding);
	}

	// Set default fallback encoding from the appropriate user language
	// settings
	const char* fallback_encoding;
	switch (user_script)
	{
	case WritingSystem::Cyrillic:
		fallback_encoding = "windows-1251";
		break;

	case WritingSystem::ChineseUnknown:
	case WritingSystem::ChineseSimplified:
		fallback_encoding = "gbk";
		break;

	case WritingSystem::ChineseTraditional:
		fallback_encoding = "big5";
		break;

	case WritingSystem::Japanese:
		fallback_encoding = "shift_jis";
		break;

	case WritingSystem::Korean:
		fallback_encoding = "euc-kr";
		break;

	case WritingSystem::LatinEastern:
		OP_ASSERT(user_languages);

		switch (STR2INT16(uni_tolower(user_languages[0]), uni_tolower(user_languages[1])))
		{
		case STR2INT16('h', 'u'): // Hungarian
		case STR2INT16('p', 'l'): // Polish
			fallback_encoding = "iso-8859-2";
			break;
		default:
			fallback_encoding = "windows-1250";
			break;
		}
		break;

	case WritingSystem::Turkish:
		fallback_encoding = "iso-8859-9";
		break;

	default:
		fallback_encoding = "iso-8859-1";
	}

	m_default_fallback_encoding.SetL(fallback_encoding);

	// Use the default value if none was set in ini file.
# ifdef PREFS_READ
	if (!m_reader->IsKey(SectionNumberToStringInternal(m_stringprefdefault[DefaultEncoding].section),
	                     m_stringprefdefault[DefaultEncoding].key))
# endif
	{
		m_stringprefs[DefaultEncoding].SetL(m_default_fallback_encoding);
	}

	// Remember ID of the encoding strings, so that we can return single-byte
	// representations of them
	if (!m_stringprefs[ForceEncoding].IsEmpty())
	{
		OpString8 forceEncoding;
		forceEncoding.SetL(m_stringprefs[ForceEncoding].CStr());
		m_forceEncoding = g_charsetManager->GetCharsetIDL(forceEncoding.CStr());
		g_charsetManager->IncrementCharsetIDReference(m_forceEncoding);
	}
	else
		m_forceEncoding = 0;

	if (!m_stringprefs[DefaultEncoding].IsEmpty())
	{
		OpString8 defaultHTMLEncoding;
		defaultHTMLEncoding.SetL(m_stringprefs[DefaultEncoding].CStr());
		m_defaultHTMLEncoding = g_charsetManager->GetCharsetIDL(defaultHTMLEncoding.CStr());
		g_charsetManager->IncrementCharsetIDReference(m_defaultHTMLEncoding);
	}
	else
		m_defaultHTMLEncoding = 0;
}

#ifdef PREFS_HOSTOVERRIDE
void PrefsCollectionDisplay::ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user)
{
	ReadOverridesInternalL(host, section, active, from_user,
	                       m_integerprefdefault, m_stringprefdefault);
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_VALIDATE
void PrefsCollectionDisplay::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	case Scale:
		*value = (*value / 10) * 10;
		if (*value < 20)
			*value = 20;
		else if (*value > 1000)
			*value = 1000;
		break;

	case FlexRootMaxWidth:
	case FlexRootMinWidth:
		if (*value < 0)
			*value = 0;
		break;

	case RM1_MinimumImageWidth: // Percentage
	case RM2_MinimumImageWidth:
	case RM3_MinimumImageWidth:
	case RM4_MinimumImageWidth:
#ifdef TV_RENDERING
	case RM5_MinimumImageWidth:
#endif
		if (*value < 0)
			*value = 0;
		else if (*value > 100)
			*value = 100;
		break;

	case RM1_RespondToMediaType: // [0,4]
	case RM1_ApplyModeSpecificTricks:
	case RM2_RespondToMediaType:
	case RM2_ApplyModeSpecificTricks:
	case RM3_RespondToMediaType:
	case RM3_ApplyModeSpecificTricks:
	case RM4_RespondToMediaType:
	case RM4_ApplyModeSpecificTricks:
#ifdef TV_RENDERING
	case RM5_RespondToMediaType:
	case RM5_ApplyModeSpecificTricks:
#endif
		if (*value < 0 || *value > 4)
			*value = 0;
		break;

	case RM1_FlexibleFonts: // [0,3]
	case RM1_ShowImages:
	case RM1_ShowIFrames:
	case RM2_FlexibleFonts:
	case RM2_ShowImages:
	case RM2_ShowIFrames:
	case RM3_FlexibleFonts:
	case RM3_ShowImages:
	case RM3_ShowIFrames:
	case RM4_FlexibleFonts:
	case RM4_ShowImages:
	case RM4_ShowIFrames:
#ifdef TV_RENDERING
	case RM5_FlexibleFonts:
	case RM5_ShowImages:
	case RM5_ShowIFrames:
#endif
		if (*value < 0 || *value > 3)
			*value = 0;
		break;

	case RM1_FlexibleImageSizes: // [0,2]
	case RM1_HonorNowrap:
	case RM1_DownloadAndApplyDocumentStyleSheets:
	case RM1_ColumnStretch:
	case RM1_TableStrategy:
	case RM1_RemoveOrnamentalImages:
	case RM1_FramesPolicy:
	case RM1_AbsolutePositioning:
	case RM1_RemoveLargeImages:
	case RM1_UseAltForCertainImages:
	case RM1_DownloadImagesAsap:
	case RM1_ShowBackgroundImages:
	case RM1_SplitHideLongWords:
	case RM1_AbsolutelyPositionedElements:
	case RM1_HonorHidden:
	case RM1_MediaStyleHandling:
	case RM2_FlexibleImageSizes:
	case RM2_HonorNowrap:
	case RM2_DownloadAndApplyDocumentStyleSheets:
	case RM2_ColumnStretch:
	case RM2_TableStrategy:
	case RM2_RemoveOrnamentalImages:
	case RM2_FramesPolicy:
	case RM2_AbsolutePositioning:
	case RM2_RemoveLargeImages:
	case RM2_UseAltForCertainImages:
	case RM2_DownloadImagesAsap:
	case RM2_ShowBackgroundImages:
	case RM2_SplitHideLongWords:
	case RM2_AbsolutelyPositionedElements:
	case RM2_HonorHidden:
	case RM2_MediaStyleHandling:
	case RM3_FlexibleImageSizes:
	case RM3_HonorNowrap:
	case RM3_DownloadAndApplyDocumentStyleSheets:
	case RM3_ColumnStretch:
	case RM3_TableStrategy:
	case RM3_RemoveOrnamentalImages:
	case RM3_FramesPolicy:
	case RM3_AbsolutePositioning:
	case RM3_RemoveLargeImages:
	case RM3_UseAltForCertainImages:
	case RM3_DownloadImagesAsap:
	case RM3_ShowBackgroundImages:
	case RM3_SplitHideLongWords:
	case RM3_AbsolutelyPositionedElements:
	case RM3_HonorHidden:
	case RM3_MediaStyleHandling:
	case RM4_FlexibleImageSizes:
	case RM4_HonorNowrap:
	case RM4_DownloadAndApplyDocumentStyleSheets:
	case RM4_ColumnStretch:
	case RM4_TableStrategy:
	case RM4_RemoveOrnamentalImages:
	case RM4_FramesPolicy:
	case RM4_AbsolutePositioning:
	case RM4_RemoveLargeImages:
	case RM4_UseAltForCertainImages:
	case RM4_DownloadImagesAsap:
	case RM4_ShowBackgroundImages:
	case RM4_SplitHideLongWords:
	case RM4_AbsolutelyPositionedElements:
	case RM4_HonorHidden:
	case RM4_MediaStyleHandling:
#ifdef TV_RENDERING
	case RM5_FlexibleImageSizes:
	case RM5_HonorNowrap:
	case RM5_DownloadAndApplyDocumentStyleSheets:
	case RM5_ColumnStretch:
	case RM5_TableStrategy:
	case RM5_RemoveOrnamentalImages:
	case RM5_FramesPolicy:
	case RM5_AbsolutePositioning:
	case RM5_RemoveLargeImages:
	case RM5_UseAltForCertainImages:
	case RM5_DownloadImagesAsap:
	case RM5_ShowBackgroundImages:
	case RM5_SplitHideLongWords:
	case RM5_AbsolutelyPositionedElements:
	case RM5_HonorHidden:
	case RM5_MediaStyleHandling:
#endif
		if (*value < 0 || *value > 2)
			*value = 0;
		break;

	case RM1_MinimumTextColorContrast: // [0,255]
	case RM2_MinimumTextColorContrast:
	case RM3_MinimumTextColorContrast:
	case RM4_MinimumTextColorContrast:
#ifdef TV_RENDERING
	case RM5_MinimumTextColorContrast:
#endif
		if (*value < 0)
			*value = 0;
		else if (*value > 255)
			*value = 255;
		break;

#ifdef SVG_SUPPORT
	case SVGRenderingQuality:
		if (*value < 0)
			*value = DEFAULT_SVG_RENDERING_QUALITY;
		break;
	case SVGTargetFrameRate:
		if (*value < 1)
			*value = DEFAULT_SVG_TARGET_FRAMERATE;
		break;

#endif

#ifdef ENCODING_DRIVEN_LINE_HEIGHT
	case DefaultLineHeightGeneral:
	case DefaultLineHeightTraditionalChinese:
	case DefaultLineHeightSimplifiedChinese:
	case DefaultLineHeightJapanese:
	case DefaultLineHeightKorean:
	case DefaultLineHeightCyrillic:
		if (*value < -1)
			*value = -1;
		break;
#endif // ENCODING_DRIVEN_LINE_HEIGHT

	case MinFontSize:
	case MaxFontSize:
#ifdef PREFS_HAVE_SCRIPT_ALLOW
	case AllowScriptToResizeWindow:
	case AllowScriptToMoveWindow:
	case AllowScriptToRaiseWindow:
	case AllowScriptToLowerWindow:
	case AllowScriptToChangeStatus:
#endif
	case AllowScriptToReceiveRightClicks:
	case AllowScriptToHideURL:
	case FirstUpdateDelay:
	case StyledFirstUpdateTimeout:
	case UpdateDelay:
	case DocumentMode:
	case ShowActiveFrame:
#ifdef GRAB_AND_SCROLL
	case ScrollIsPan:
#endif
	case MaximumMarqueeLoops:
	case LimitParagraphWidth:
#ifdef LAYOUT_YIELD_REFLOW
	case InitialYieldReflowTime:
	case YieldReflowTimeIncreaseRate:
#endif
#ifdef WIDGETS_LIMIT_TEXTAREA_SIZE
	case MaximumBytesTextarea:
#endif
	case FramesEnabled:
	case IFramesEnabled:
#ifdef _PLUGIN_SUPPORT_
	case PluginsEnabled:
#endif
#ifdef USE_FLASH_PREF
	case FlashEnabled:
#endif
#ifdef CANVAS_SUPPORT
	case CanvasEnabled:
#endif
	case LinkHasColor:
	case LinkHasUnderline:
	case LinkHasStrikeThrough:
	case LinkHasFrame:
	case VisitedLinkHasColor:
	case VisitedLinkHasUnderline:
	case VisitedLinkHasStrikeThrough:
	case UM_AuthorCSS:
	case UM_AuthorFonts:
	case UM_UserCSS:
	case UM_UserFonts:
	case UM_UserLinks:
	case DM_AuthorCSS:
	case DM_AuthorFonts:
	case DM_UserCSS:
	case DM_UserFonts:
	case DM_UserLinks:
#ifdef PREFS_HAVE_INTERPOLATE_IMAGES
	case InterpolateImages:
#endif
#ifdef EMBEDDED_ICC_SUPPORT
	case ImagesUseICC:
#endif
#ifdef PREFS_HAVE_HOTCLICK
	case AutomaticSelectMenu:
#endif
	case SmoothScrolling:
#ifdef _SUPPORT_SMOOTH_DISPLAY_
	case SmoothDisplay:
#endif
#ifdef CSS_SCROLLBARS_SUPPORT
	case EnableScrollbarColors:
#endif
#ifdef _DIRECT_URL_WINDOW_
	case AutoDropDown:
#endif
	case EnableStylingOnForms:
#ifdef SESSION_SUPPORT
	case SavePasswordProtectedPages:
#endif
#ifdef LAYOUT_USE_SSR_MAX_WIDTH_PREF
	case SSRMaxWidth:
#endif
	case RM1_Small:
	case RM1_Medium:
	case RM1_Large:
	case RM1_XLarge:
	case RM1_XXLarge:
	case RM1_FlexibleColumns:
	case RM1_IgnoreRowspanWhenRestructuring:
	case RM1_TableMagic:
	case RM1_AllowScrollbarsInIFrame:
	case RM1_Float:
	case RM1_ConvertNbspIntoNormalSpace:
	case RM1_AllowAggressiveWordBreaking:
	case RM1_TextColorLight:
	case RM1_TextColorDark:
	case RM1_HighlightBlocks:
	case RM1_AvoidInterlaceFlicker:
	case RM1_CrossoverSize:
	case RM1_AllowHorizontalScrollbar:
	case RM2_Small:
	case RM2_Medium:
	case RM2_Large:
	case RM2_XLarge:
	case RM2_XXLarge:
	case RM2_FlexibleColumns:
	case RM2_IgnoreRowspanWhenRestructuring:
	case RM2_TableMagic:
	case RM2_AllowScrollbarsInIFrame:
	case RM2_Float:
	case RM2_ConvertNbspIntoNormalSpace:
	case RM2_AllowAggressiveWordBreaking:
	case RM2_TextColorLight:
	case RM2_TextColorDark:
	case RM2_HighlightBlocks:
	case RM2_AvoidInterlaceFlicker:
	case RM2_CrossoverSize:
	case RM2_AllowHorizontalScrollbar:
	case RM3_Small:
	case RM3_Medium:
	case RM3_Large:
	case RM3_XLarge:
	case RM3_XXLarge:
	case RM3_FlexibleColumns:
	case RM3_IgnoreRowspanWhenRestructuring:
	case RM3_TableMagic:
	case RM3_AllowScrollbarsInIFrame:
	case RM3_Float:
	case RM3_ConvertNbspIntoNormalSpace:
	case RM3_AllowAggressiveWordBreaking:
	case RM3_TextColorLight:
	case RM3_TextColorDark:
	case RM3_HighlightBlocks:
	case RM3_AvoidInterlaceFlicker:
	case RM3_CrossoverSize:
	case RM3_AllowHorizontalScrollbar:
	case RM4_Small:
	case RM4_Medium:
	case RM4_Large:
	case RM4_XLarge:
	case RM4_XXLarge:
	case RM4_FlexibleColumns:
	case RM4_IgnoreRowspanWhenRestructuring:
	case RM4_TableMagic:
	case RM4_AllowScrollbarsInIFrame:
	case RM4_Float:
	case RM4_ConvertNbspIntoNormalSpace:
	case RM4_AllowAggressiveWordBreaking:
	case RM4_TextColorLight:
	case RM4_TextColorDark:
	case RM4_HighlightBlocks:
	case RM4_AvoidInterlaceFlicker:
	case RM4_CrossoverSize:
	case RM4_AllowHorizontalScrollbar:
#ifdef TV_RENDERING
	case RM5_Small:
	case RM5_Medium:
	case RM5_Large:
	case RM5_XLarge:
	case RM5_XXLarge:
	case RM5_FlexibleColumns:
	case RM5_IgnoreRowspanWhenRestructuring:
	case RM5_TableMagic:
	case RM5_AllowScrollbarsInIFrame:
	case RM5_Float:
	case RM5_ConvertNbspIntoNormalSpace:
	case RM5_AllowAggressiveWordBreaking:
	case RM5_TextColorLight:
	case RM5_TextColorDark:
	case RM5_HighlightBlocks:
	case RM5_AvoidInterlaceFlicker:
	case RM5_CrossoverSize:
	case RM5_AllowHorizontalScrollbar:
#endif
#ifdef SUPPORT_TEXT_DIRECTION
	case RTLFlipsUI:
#endif // SUPPORT_TEXT_DIRECTION
	case LeftHandedUI:
#ifdef SUPPORT_IME_PASSWORD_INPUT
	case ShowIMEPassword:
#endif // SUPPORT_IME_PASSWORD_INPUT
#ifdef ON_DEMAND_PLUGIN
	case EnableOnDemandPlugin:
	case EnableOnDemandPluginPlaceholder:
#endif // ON_DEMAND_PLUGIN
	case EnableWebfonts:
#ifdef FORMAT_UNSTYLED_XML
	case FormatUnstyledXML:
#endif //FORMAT_UNSTYLED_XML
	case AltImageBorderEnabled:
	case ReparseFailedXHTML:
#ifdef SKIN_HIGHLIGHTED_ELEMENT
	case DisableHighlightUponOutline:
#endif // SKIN_HIGHLIGHTED_ELEMENT
		break; // Nothing to do.

	case VisitedLinksState:
		if (*value < 0)
			*value = 0;
		else if (*value > 2)
			*value = 2;
		break;

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionDisplay::CheckConditionsL(int which,
                                              const OpStringC &invalue,
                                              OpString **outvalue,
                                              const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
	case ForceEncoding:
	case DefaultEncoding:
	{
		// Make sure the charset name is valid
		char sb[64]; /* ARRAY OK 2009-02-26 adame */
		const char *realname = NULL;
		if (!invalue.IsEmpty())
		{
			uni_cstrlcpy(sb, invalue.CStr(), ARRAY_SIZE(sb));
			realname = g_charsetManager->GetCanonicalCharsetName(sb);
		}
		else
		{
			*sb = 0;
		}
		if (realname)
		{
			OpStackAutoPtr<OpString> newvalue(OP_NEW_L(OpString, ()));
			newvalue->SetL(realname);
			*outvalue = newvalue.release();
			return TRUE;
		}
		else
		{
			// Charset is not in canonical name list. If we are writing the
			// forced encoding, it can also be an autodetect identifier.
			if (ForceEncoding == stringpref(which) &&
			    CharsetDetector::AutoDetectIdFromString(sb) != CharsetDetector::autodetect_none)
			{
				break;
			}

			// The current version of Opera does not understand the setting,
			// which means that it should be discarded.
			OpStackAutoPtr<OpString> newvalue(OP_NEW_L(OpString, ()));
			if (DefaultEncoding == stringpref(which))
			{
				newvalue->SetL(m_default_fallback_encoding);
			}
			*outvalue = newvalue.release();
			return TRUE;
		}
		break;
	}

	case CSSFamilySerif:
	case CSSFamilySansserif:
	case CSSFamilyCursive:
	case CSSFamilyFantasy:
	case CSSFamilyMonospace:
		// Bug#172879: Make sure the font is known
		if (-1 == styleManager->GetFontNumber(invalue.CStr()))
		{
			// The font is not recognized, use the default value instead
			OpStackAutoPtr<OpString> newvalue(OP_NEW_L(OpString, ()));
			newvalue->SetL(GetDefaultStringPref(stringpref(which)));
			*outvalue = newvalue.release();
			return TRUE;
		}
		break;

	case PasswordCharacterPlaceholder:
	{
		OpStackAutoPtr<OpString> newvalue(OP_NEW_L(OpString, ()));
		BOOL use_fallback = FALSE;

		for (const uni_char* p = invalue.CStr(); *p; ++p)
		{
			if (Unicode::IsCntrl(*p))
			{
				use_fallback = TRUE;
				break;
			}
		}

		UnicodePoint fallback_character;
		if (use_fallback)
		{
			fallback_character = Unicode::GetUnicodePoint(WIDGETS_PASSWORD_CHAR, uni_strlen(WIDGETS_PASSWORD_CHAR));
			OP_ASSERT(Unicode::IsCntrl(fallback_character) && "Non-printable fallback character?");
		}
		else
		{
			fallback_character = Unicode::GetUnicodePoint(invalue.CStr(), invalue.Length());
		}

		newvalue->ReserveL(3);
		int unicode_point_length = Unicode::WriteUnicodePoint(newvalue->CStr(), fallback_character);
		newvalue->CStr()[unicode_point_length] = 0;

		*outvalue = newvalue.release();
		return TRUE;
	}

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}

	// When FALSE is returned, no OpString is created for outvalue
	return FALSE;
}
#endif // PREFS_VALIDATE

#ifdef PREFS_WRITE
void PrefsCollectionDisplay::BroadcastChange(int pref, const OpStringC &newvalue)
{
	// Need to copy the encoding strings down to single-byte representations
	switch (stringpref(pref))
	{
	case ForceEncoding:
		g_charsetManager->DecrementCharsetIDReference(m_forceEncoding);
		m_forceEncoding = 0;
		if (!newvalue.IsEmpty())
		{
			OpString8 forceEncoding;
			// Unable to return any error value here
			if (OpStatus::IsSuccess(forceEncoding.Set(newvalue.CStr())) &&
			    OpStatus::IsSuccess(g_charsetManager->GetCharsetID(forceEncoding.CStr(), &m_forceEncoding)))
				g_charsetManager->IncrementCharsetIDReference(m_forceEncoding);
		}
		break;

	case DefaultEncoding:
		g_charsetManager->DecrementCharsetIDReference(m_defaultHTMLEncoding);
		m_defaultHTMLEncoding = 0;
		if (!newvalue.IsEmpty())
		{
			OpString8 defaultHTMLEncoding;
			// Unable to return any error value here
			if (OpStatus::IsSuccess(defaultHTMLEncoding.Set(newvalue.CStr())) &&
			    OpStatus::IsSuccess(g_charsetManager->GetCharsetID(defaultHTMLEncoding.CStr(), &m_defaultHTMLEncoding)))
				g_charsetManager->IncrementCharsetIDReference(m_defaultHTMLEncoding);
		}
		break;
	}

	OpPrefsCollection::BroadcastChange(pref, newvalue);
}
#endif // PREFS_WRITE

PrefsCollectionDisplay::integerpref
	PrefsCollectionDisplay::GetPrefFor(
		CSSMODE m, PrefsCollectionDisplay::csssetting s)
{
	// Ugly, but saves me from implementing a two-dimensional OpOverrideHost.
	switch (m)
	{
	case CSS_NONE:
		switch (s)
		{
		case EnableAuthorCSS:
			return UM_AuthorCSS;
		case EnableAuthorFontAndColors:
			return UM_AuthorFonts;
		case EnableUserCSS:
			return UM_UserCSS;
		case EnableUserFontAndColors:
			return UM_UserFonts;
		case EnableUserLinkSettings:
			return UM_UserLinks;
		}
		OP_ASSERT(!"Unrecognized parameters");
		break;

	case CSS_FULL:
		switch (s)
		{
		case EnableAuthorCSS:
			return DM_AuthorCSS;
		case EnableAuthorFontAndColors:
			return DM_AuthorFonts;
		case EnableUserCSS:
			return DM_UserCSS;
		case EnableUserFontAndColors:
			return DM_UserFonts;
		case EnableUserLinkSettings:
			return DM_UserLinks;
		}
		OP_ASSERT(!"Unrecognized parameters");
		break;

	default:
		OP_ASSERT(!"Unrecognized parameters");
	}

	return UM_AuthorCSS;
}

PrefsCollectionDisplay::integerpref
	PrefsCollectionDisplay::GetPrefFor(
		PrefsCollectionDisplay::RenderingModes m,
		PrefsCollectionDisplay::RenderingModeSettings s)
{
	// Ugly, but saves me from implementing a two-dimensional OpOverrideHost.
	switch (m)
	{
	case SSR:
		switch (s)
		{
		case FlexibleFonts:
			return RM1_FlexibleFonts;
		case ShowIFrames:
			return RM1_ShowIFrames;
		case Small:
			return RM1_Small;
		case Medium:
			return RM1_Medium;
		case Large:
			return RM1_Large;
		case XLarge:
			return RM1_XLarge;
		case XXLarge:
			return RM1_XXLarge;
		case ColumnStretch:
			return RM1_ColumnStretch;
		case TableStrategy:
			return RM1_TableStrategy;
		case FlexibleColumns:
			return RM1_FlexibleColumns;
		case IgnoreRowspanWhenRestructuring:
			return RM1_IgnoreRowspanWhenRestructuring;
		case TableMagic:
			return RM1_TableMagic;
		case ShowImages:
			return RM1_ShowImages;
		case RemoveOrnamentalImages:
			return RM1_RemoveOrnamentalImages;
		case RemoveLargeImages:
			return RM1_RemoveLargeImages;
		case UseAltForCertainImages:
			return RM1_UseAltForCertainImages;
		case FlexibleImageSizes:
			return RM1_FlexibleImageSizes;
		case AllowScrollbarsInIFrame:
			return RM1_AllowScrollbarsInIFrame;
		case MinimumImageWidth:
			return RM1_MinimumImageWidth;
		case Float:
			return RM1_Float;
		case DownloadImagesAsap:
			return RM1_DownloadImagesAsap;
		case ShowBackgroundImages:
			return RM1_ShowBackgroundImages;
		case SplitHideLongWords:
			return RM1_SplitHideLongWords;
		case HonorNowrap:
			return RM1_HonorNowrap;
		case ConvertNbspIntoNormalSpace:
			return RM1_ConvertNbspIntoNormalSpace;
		case AllowAggressiveWordBreaking:
			return RM1_AllowAggressiveWordBreaking;
		case MinimumTextColorContrast:
			return RM1_MinimumTextColorContrast;
		case TextColorLight:
			return RM1_TextColorLight;
		case TextColorDark:
			return RM1_TextColorDark;
		case HighlightBlocks:
			return RM1_HighlightBlocks;
		case AvoidInterlaceFlicker:
			return RM1_AvoidInterlaceFlicker;
		case CrossoverSize:
			return RM1_CrossoverSize;
		case AllowHorizontalScrollbar:
			return RM1_AllowHorizontalScrollbar;
		case DownloadAndApplyDocumentStyleSheets:
			return RM1_DownloadAndApplyDocumentStyleSheets;
		case ApplyModeSpecificTricks:
			return RM1_ApplyModeSpecificTricks;
		case RespondToMediaType:
			return RM1_RespondToMediaType;
		case FramesPolicy:
			return RM1_FramesPolicy;
		case AbsolutelyPositionedElements:
			return RM1_AbsolutelyPositionedElements;
		case AbsolutePositioning:
			return RM1_AbsolutePositioning;
		case HonorHidden:
			return RM1_HonorHidden;
		case MediaStyleHandling:
			return RM1_MediaStyleHandling;
		}
		break;

	case CSSR:
		switch (s)
		{
		case FlexibleFonts:
			return RM2_FlexibleFonts;
		case ShowIFrames:
			return RM2_ShowIFrames;
		case Small:
			return RM2_Small;
		case Medium:
			return RM2_Medium;
		case Large:
			return RM2_Large;
		case XLarge:
			return RM2_XLarge;
		case XXLarge:
			return RM2_XXLarge;
		case ColumnStretch:
			return RM2_ColumnStretch;
		case TableStrategy:
			return RM2_TableStrategy;
		case FlexibleColumns:
			return RM2_FlexibleColumns;
		case IgnoreRowspanWhenRestructuring:
			return RM2_IgnoreRowspanWhenRestructuring;
		case TableMagic:
			return RM2_TableMagic;
		case ShowImages:
			return RM2_ShowImages;
		case RemoveOrnamentalImages:
			return RM2_RemoveOrnamentalImages;
		case RemoveLargeImages:
			return RM2_RemoveLargeImages;
		case UseAltForCertainImages:
			return RM2_UseAltForCertainImages;
		case FlexibleImageSizes:
			return RM2_FlexibleImageSizes;
		case AllowScrollbarsInIFrame:
			return RM2_AllowScrollbarsInIFrame;
		case MinimumImageWidth:
			return RM2_MinimumImageWidth;
		case Float:
			return RM2_Float;
		case DownloadImagesAsap:
			return RM2_DownloadImagesAsap;
		case ShowBackgroundImages:
			return RM2_ShowBackgroundImages;
		case SplitHideLongWords:
			return RM2_SplitHideLongWords;
		case HonorNowrap:
			return RM2_HonorNowrap;
		case ConvertNbspIntoNormalSpace:
			return RM2_ConvertNbspIntoNormalSpace;
		case AllowAggressiveWordBreaking:
			return RM2_AllowAggressiveWordBreaking;
		case MinimumTextColorContrast:
			return RM2_MinimumTextColorContrast;
		case TextColorLight:
			return RM2_TextColorLight;
		case TextColorDark:
			return RM2_TextColorDark;
		case HighlightBlocks:
			return RM2_HighlightBlocks;
		case AvoidInterlaceFlicker:
			return RM2_AvoidInterlaceFlicker;
		case CrossoverSize:
			return RM2_CrossoverSize;
		case AllowHorizontalScrollbar:
			return RM2_AllowHorizontalScrollbar;
		case DownloadAndApplyDocumentStyleSheets:
			return RM2_DownloadAndApplyDocumentStyleSheets;
		case ApplyModeSpecificTricks:
			return RM2_ApplyModeSpecificTricks;
		case RespondToMediaType:
			return RM2_RespondToMediaType;
		case FramesPolicy:
			return RM2_FramesPolicy;
		case AbsolutelyPositionedElements:
			return RM2_AbsolutelyPositionedElements;
		case AbsolutePositioning:
			return RM2_AbsolutePositioning;
		case HonorHidden:
			return RM2_HonorHidden;
		case MediaStyleHandling:
			return RM2_MediaStyleHandling;
		}
		break;

	case AMSR:
		switch (s)
		{
		case FlexibleFonts:
			return RM3_FlexibleFonts;
		case ShowIFrames:
			return RM3_ShowIFrames;
		case Small:
			return RM3_Small;
		case Medium:
			return RM3_Medium;
		case Large:
			return RM3_Large;
		case XLarge:
			return RM3_XLarge;
		case XXLarge:
			return RM3_XXLarge;
		case ColumnStretch:
			return RM3_ColumnStretch;
		case TableStrategy:
			return RM3_TableStrategy;
		case FlexibleColumns:
			return RM3_FlexibleColumns;
		case IgnoreRowspanWhenRestructuring:
			return RM3_IgnoreRowspanWhenRestructuring;
		case TableMagic:
			return RM3_TableMagic;
		case ShowImages:
			return RM3_ShowImages;
		case RemoveOrnamentalImages:
			return RM3_RemoveOrnamentalImages;
		case RemoveLargeImages:
			return RM3_RemoveLargeImages;
		case UseAltForCertainImages:
			return RM3_UseAltForCertainImages;
		case FlexibleImageSizes:
			return RM3_FlexibleImageSizes;
		case AllowScrollbarsInIFrame:
			return RM3_AllowScrollbarsInIFrame;
		case MinimumImageWidth:
			return RM3_MinimumImageWidth;
		case Float:
			return RM3_Float;
		case DownloadImagesAsap:
			return RM3_DownloadImagesAsap;
		case ShowBackgroundImages:
			return RM3_ShowBackgroundImages;
		case SplitHideLongWords:
			return RM3_SplitHideLongWords;
		case HonorNowrap:
			return RM3_HonorNowrap;
		case ConvertNbspIntoNormalSpace:
			return RM3_ConvertNbspIntoNormalSpace;
		case AllowAggressiveWordBreaking:
			return RM3_AllowAggressiveWordBreaking;
		case MinimumTextColorContrast:
			return RM3_MinimumTextColorContrast;
		case TextColorLight:
			return RM3_TextColorLight;
		case TextColorDark:
			return RM3_TextColorDark;
		case HighlightBlocks:
			return RM3_HighlightBlocks;
		case AvoidInterlaceFlicker:
			return RM3_AvoidInterlaceFlicker;
		case CrossoverSize:
			return RM3_CrossoverSize;
		case AllowHorizontalScrollbar:
			return RM3_AllowHorizontalScrollbar;
		case DownloadAndApplyDocumentStyleSheets:
			return RM3_DownloadAndApplyDocumentStyleSheets;
		case ApplyModeSpecificTricks:
			return RM3_ApplyModeSpecificTricks;
		case RespondToMediaType:
			return RM3_RespondToMediaType;
		case FramesPolicy:
			return RM3_FramesPolicy;
		case AbsolutelyPositionedElements:
			return RM3_AbsolutelyPositionedElements;
		case AbsolutePositioning:
			return RM3_AbsolutePositioning;
		case HonorHidden:
			return RM3_HonorHidden;
		case MediaStyleHandling:
			return RM3_MediaStyleHandling;
		}
		break;

	case MSR:
		switch (s)
		{
		case FlexibleFonts:
			return RM4_FlexibleFonts;
		case ShowIFrames:
			return RM4_ShowIFrames;
		case Small:
			return RM4_Small;
		case Medium:
			return RM4_Medium;
		case Large:
			return RM4_Large;
		case XLarge:
			return RM4_XLarge;
		case XXLarge:
			return RM4_XXLarge;
		case ColumnStretch:
			return RM4_ColumnStretch;
		case TableStrategy:
			return RM4_TableStrategy;
		case FlexibleColumns:
			return RM4_FlexibleColumns;
		case IgnoreRowspanWhenRestructuring:
			return RM4_IgnoreRowspanWhenRestructuring;
		case TableMagic:
			return RM4_TableMagic;
		case ShowImages:
			return RM4_ShowImages;
		case RemoveOrnamentalImages:
			return RM4_RemoveOrnamentalImages;
		case RemoveLargeImages:
			return RM4_RemoveLargeImages;
		case UseAltForCertainImages:
			return RM4_UseAltForCertainImages;
		case FlexibleImageSizes:
			return RM4_FlexibleImageSizes;
		case AllowScrollbarsInIFrame:
			return RM4_AllowScrollbarsInIFrame;
		case MinimumImageWidth:
			return RM4_MinimumImageWidth;
		case Float:
			return RM4_Float;
		case DownloadImagesAsap:
			return RM4_DownloadImagesAsap;
		case ShowBackgroundImages:
			return RM4_ShowBackgroundImages;
		case SplitHideLongWords:
			return RM4_SplitHideLongWords;
		case HonorNowrap:
			return RM4_HonorNowrap;
		case ConvertNbspIntoNormalSpace:
			return RM4_ConvertNbspIntoNormalSpace;
		case AllowAggressiveWordBreaking:
			return RM4_AllowAggressiveWordBreaking;
		case MinimumTextColorContrast:
			return RM4_MinimumTextColorContrast;
		case TextColorLight:
			return RM4_TextColorLight;
		case TextColorDark:
			return RM4_TextColorDark;
		case HighlightBlocks:
			return RM4_HighlightBlocks;
		case AvoidInterlaceFlicker:
			return RM4_AvoidInterlaceFlicker;
		case CrossoverSize:
			return RM4_CrossoverSize;
		case AllowHorizontalScrollbar:
			return RM4_AllowHorizontalScrollbar;
		case DownloadAndApplyDocumentStyleSheets:
			return RM4_DownloadAndApplyDocumentStyleSheets;
		case ApplyModeSpecificTricks:
			return RM4_ApplyModeSpecificTricks;
		case RespondToMediaType:
			return RM4_RespondToMediaType;
		case FramesPolicy:
			return RM4_FramesPolicy;
		case AbsolutelyPositionedElements:
			return RM4_AbsolutelyPositionedElements;
		case AbsolutePositioning:
			return RM4_AbsolutePositioning;
		case HonorHidden:
			return RM4_HonorHidden;
		case MediaStyleHandling:
			return RM4_MediaStyleHandling;
		}
		break;

#ifdef TV_RENDERING
	case TVR:
		switch (s)
		{
		case FlexibleFonts:
			return RM5_FlexibleFonts;
		case ShowIFrames:
			return RM5_ShowIFrames;
		case Small:
			return RM5_Small;
		case Medium:
			return RM5_Medium;
		case Large:
			return RM5_Large;
		case XLarge:
			return RM5_XLarge;
		case XXLarge:
			return RM5_XXLarge;
		case ColumnStretch:
			return RM5_ColumnStretch;
		case TableStrategy:
			return RM5_TableStrategy;
		case FlexibleColumns:
			return RM5_FlexibleColumns;
		case IgnoreRowspanWhenRestructuring:
			return RM5_IgnoreRowspanWhenRestructuring;
		case TableMagic:
			return RM5_TableMagic;
		case ShowImages:
			return RM5_ShowImages;
		case RemoveOrnamentalImages:
			return RM5_RemoveOrnamentalImages;
		case RemoveLargeImages:
			return RM5_RemoveLargeImages;
		case UseAltForCertainImages:
			return RM5_UseAltForCertainImages;
		case FlexibleImageSizes:
			return RM5_FlexibleImageSizes;
		case AllowScrollbarsInIFrame:
			return RM5_AllowScrollbarsInIFrame;
		case MinimumImageWidth:
			return RM5_MinimumImageWidth;
		case Float:
			return RM5_Float;
		case DownloadImagesAsap:
			return RM5_DownloadImagesAsap;
		case ShowBackgroundImages:
			return RM5_ShowBackgroundImages;
		case SplitHideLongWords:
			return RM5_SplitHideLongWords;
		case HonorNowrap:
			return RM5_HonorNowrap;
		case ConvertNbspIntoNormalSpace:
			return RM5_ConvertNbspIntoNormalSpace;
		case AllowAggressiveWordBreaking:
			return RM5_AllowAggressiveWordBreaking;
		case MinimumTextColorContrast:
			return RM5_MinimumTextColorContrast;
		case TextColorLight:
			return RM5_TextColorLight;
		case TextColorDark:
			return RM5_TextColorDark;
		case HighlightBlocks:
			return RM5_HighlightBlocks;
		case AvoidInterlaceFlicker:
			return RM5_AvoidInterlaceFlicker;
		case CrossoverSize:
			return RM5_CrossoverSize;
		case AllowHorizontalScrollbar:
			return RM5_AllowHorizontalScrollbar;
		case DownloadAndApplyDocumentStyleSheets:
			return RM5_DownloadAndApplyDocumentStyleSheets;
		case ApplyModeSpecificTricks:
			return RM5_ApplyModeSpecificTricks;
		case RespondToMediaType:
			return RM5_RespondToMediaType;
		case FramesPolicy:
			return RM5_FramesPolicy;
		case AbsolutelyPositionedElements:
			return RM5_AbsolutelyPositionedElements;
		case AbsolutePositioning:
			return RM5_AbsolutePositioning;
		case HonorHidden:
			return RM5_HonorHidden;
		case MediaStyleHandling:
			return RM5_MediaStyleHandling;
		}
		break;
#endif
	}

	OP_ASSERT(!"Unrecognized parameters");
	return RM1_FlexibleFonts;
}
