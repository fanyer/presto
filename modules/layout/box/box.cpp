/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

/** @file box.cpp
 *
 * Box class implementation for document layout.
 *
 * @author Geir Ivarsøy
 * @author Karl Anders Øygard
 *
 */

#include "core/pch.h"

#include "modules/layout/box/box.h"
#include "modules/layout/box/blockbox.h"
#include "modules/layout/box/flexitem.h"
#include "modules/layout/box/inline.h"
#include "modules/layout/box/tables.h"
#include "modules/layout/content/flexcontent.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/layout/cssprops.h"
#include "modules/probetools/probepoints.h"
#include "modules/url/url_api.h"
#include "modules/img/image.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/logdoc/logdocenum.h"
#include "modules/logdoc/selection.h"
#include "modules/style/css_media.h"
#include "modules/layout/frm.h"
#include "modules/layout/numbers.h"
#include "modules/dochand/win.h"
#include "modules/logdoc/src/textdata.h"
#include "modules/unicode/unicode_stringiterator.h"

#ifdef GADGET_SUPPORT
# include "modules/gadgets/OpGadget.h"
#endif

#include "modules/logdoc/urlimgcontprov.h"

#include "modules/pi/OpSystemInfo.h"

#include "modules/display/prn_dev.h"

#ifdef SKIN_SUPPORT
#include "modules/skin/OpSkinManager.h"
#endif

#include "modules/hardcore/mh/messages.h"
#include "modules/dochand/winman.h"
#include "modules/doc/html_doc.h"

#include "modules/prefs/prefsmanager/collections/pc_print.h"

#include "modules/layout/layout_workplace.h"

#ifdef MEDIA_HTML_SUPPORT
# include "modules/media/mediaelement.h"
#endif // MEDIA_HTML_SUPPORT

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
# include "modules/svg/svg_image.h"
#endif // SVG_SUPPORT

#include "modules/unicode/unicode.h"
#include "modules/unicode/unicode_segmenter.h"

#ifdef DOCUMENT_EDIT_SUPPORT
# include "modules/documentedit/OpDocumentEdit.h"
#endif // DOCUMENT_EDIT_SUPPORT

static inline BOOL HasPercentageBackgroundPosition(const HTMLayoutProperties &props)
{
	return props.bg_images.HasPercentagePositions();
}

static inline COLORREF BackgroundColor(const HTMLayoutProperties &props, const Box* box)
{
	COLORREF bg_color = props.bg_color;

	if (bg_color == USE_DEFAULT_COLOR && box && !props.GetBgIsTransparent())
		if (FormObject* fo = box->GetFormObject())
			bg_color = fo->GetDefaultBackgroundColor();

	return bg_color;
}

/** Check if the background of this element was already painted by the root element.

	@param doc The document
	@param html_element The element that we want to detect if the root element painted backgrounds for
	@return TRUE if the background for this element was already painted. */

static inline BOOL
CheckBackgroundOnRoot(FramesDocument *doc, HTML_Element *html_element)
{
	if (html_element->GetNsType() == NS_HTML)
	{
		HTML_ElementType elm_type = html_element->Type();

		if (elm_type == Markup::HTE_HTML ||
			elm_type == Markup::HTE_BODY &&
			html_element == doc->GetHLDocProfile()->GetBodyElm() &&
			doc->GetLayoutWorkplace()->GetDocRootProperties().GetBackgroundElementType() == Markup::HTE_BODY)
			if (html_element->GetInserted() != HE_INSERTED_BY_LAYOUT)
				// Background already drawn by root

				return TRUE;
	}

	return FALSE;
}

/** Return TRUE if there is no background or border. */

static inline BOOL
HasNoBgAndBorder(COLORREF bg_color, const BackgroundsAndBorders& bb, const HTMLayoutProperties& props)
{
	return bg_color == USE_DEFAULT_COLOR && bb.IsBorderAllTransparent() && props.bg_images.IsEmpty() && !props.box_shadows.Get();
}

#if defined(FONTSWITCHING) && !defined(PLATFORM_FONTSWITCHING)

#ifdef USE_TEXTSHAPER_INTERNALLY
#include "modules/display/textshaper.h"
#endif // USE_TEXTSHAPER_INTERNALLY

BOOL NeedSwitch(FontSupportInfo& font_info, const UnicodePoint uc)
{
    if(!font_info.current_font)
		return FALSE;

	/* no need to font switch for characters that should not be displayed.
	   Should probably be implemented in a more generic way, but that is
	   coming up for core-2 */

	if ((uc >= 0x2028 && uc <= 0x202e) ||
		(uc >= 0x200b && uc <= 0x200f) ||
		StyleManager::NoFontswitchNoncollapsingWhitespace(uc))
        return FALSE;

    BOOL left_block = (uc < font_info.block_lowest || uc > font_info.block_highest);

    if(left_block)
		return TRUE;

#ifdef _GLYPHTESTING_SUPPORT_
    if(font_info.current_font->HasGlyphTable(font_info.current_block))   // This block is not always completely supported
        return !font_info.current_font->HasGlyph(uc);          // Check if the font actually supports the character
#endif
	// The block check above only tells us that we are still in the same block. But we still *have* to check if the current font really has that block anyway.
	if (font_info.current_block != UNKNOWN_BLOCK_NUMBER)
		if (!font_info.current_font->HasBlock(font_info.current_block))
			return TRUE;

    return FALSE;
}

/** Switch to a font that supports given character.
	Update font_info and return TRUE if switch is done.
	Return FALSE if cannot switch. */


/* This function signature should eventually be removed in favour of the one below, taking a full string */

static BOOL
SwitchFont(const UnicodePoint uc, FontSupportInfo& font_info, WritingSystem::Script script)
{

	int old_block = font_info.current_block;
	styleManager->GetUnicodeBlockInfo(uc, font_info.current_block, font_info.block_lowest, font_info.block_highest);

	if (!NeedSwitch(font_info, uc))
		return FALSE;

	if (font_info.current_block != UNKNOWN_BLOCK_NUMBER)
	{
		if (font_info.current_font != font_info.desired_font
			&&	font_info.desired_font->HasBlock(font_info.current_block)
#ifdef _GLYPHTESTING_SUPPORT_
			&& (!font_info.desired_font->HasGlyphTable(font_info.current_block)
				|| (styleManager->ShouldHaveGlyph(uc) && font_info.desired_font->HasGlyph(uc)) )
#endif
			&& (font_info.desired_font->HasScript(script) && font_info.current_font != font_info.desired_font))
		{
			// Use desired font whenever possible

			font_info.current_font = font_info.desired_font;

			return TRUE;
		}
		else
			if ((font_info.current_block != old_block && !font_info.desired_font->HasBlock(font_info.current_block))  /* we have changed block, we should reprioritize (see bug 161664) */
				 || !font_info.current_font->HasBlock(font_info.current_block)
#ifdef _GLYPHTESTING_SUPPORT_
				 || (styleManager->ShouldHaveGlyph(uc) && font_info.current_font->HasGlyphTable(font_info.current_block) && !font_info.current_font->HasGlyph(uc))
#endif
				 || !font_info.current_font->HasScript(script))
			{
				// Current font does not support this character

				font_info.current_font = styleManager->GetRecommendedAlternativeFont(font_info.desired_font, font_info.current_block, script, uc);

				if (!font_info.current_font)
				{
					// No font support for this character so use desired font

					font_info.current_font = font_info.desired_font;
				}

				return TRUE;
			}
	}

	return FALSE;
}

BOOL
SwitchFont(const uni_char* str, int length, int& consumed, FontSupportInfo& font_info, WritingSystem::Script script)
{
	OP_ASSERT(length);
	UnicodePoint uc = Unicode::GetUnicodePoint(str, length, consumed);
#ifdef USE_TEXTSHAPER_INTERNALLY
	if (TextShaper::NeedsTransformation(uc) || TextShaper::GetJoiningClass(uc) != TextShaper::NON_JOINING)
	{
		UnicodePoint shaped = TextShaper::GetShapedChar(str, length, consumed);
		if (shaped)
			uc = shaped;
	}
#endif // USE_TEXTSHAPER_INTERNALLY

	OP_ASSERT(consumed <= length);

	return SwitchFont(uc, font_info, script);
}

/* Used in the obml module by the use of 'extern' */

BOOL SwitchFont(const uni_char* str, int length, int& consumed, FontSupportInfo& font_info, OpFontInfo::CodePage current_codepage)
{
	return SwitchFont(str, length, consumed, font_info, WritingSystem::Unknown);
}

#endif // FONTSWITCHING && !PLATFORM_FONTSWITCHING

static inline BOOL ERA_AggressiveWordBreakAfter(UnicodePoint ch)
{
	if (ch == '_' || ch == '$' || ch =='@' || ch == '%' || ch == '#' || ch == ')' || ch == '}' || ch == ']')
		return TRUE;
	return FALSE;
}

static inline BOOL ERA_AggressiveWordBreakBefore(UnicodePoint ch)
{
	if (ch == '{' || ch == '[' || ch == '(')
		return TRUE;
	return FALSE;
}

static inline BOOL layout_collapse_space(UnicodePoint ch, BOOL treat_nbsp_as_space)
{
	return uni_collapsing_sp(ch) || (uni_isnbsp(ch) && treat_nbsp_as_space);
}

/* Returns the UnicodePoint before the one (starting) at str+pos,
   taking surrogate pairs into consideration. */
UnicodePoint GetPreviousUnicodePoint(const uni_char* str, const size_t pos, int& consumed)
{
	if (!pos)
		return 0;
	size_t p = pos - 1;
	if (p && Unicode::IsLowSurrogate(str[p]) && Unicode::IsHighSurrogate(str[p-1]))
		--p;

	const UnicodePoint uc = Unicode::GetUnicodePoint(str + p, pos - p, consumed);
	OP_ASSERT(p + consumed == pos);
	return uc;
}

/** Get next text fragment from 'txt' which is updated to point to the
	start of next fragment and fill 'word_info' with the result.  Returns
	FALSE if no more characters.

	A text fragment is principally the next non-breakable block of text.

	 - For 'white_space' value "pre" the only breaking characters are linefeed
	   (U+000A), carriage return (U+000D) and FORCE_LINE_BREAK_CHAR. A fragment
	   will also be broken by a tab (U+0009). The breaking character is not
	   included in the fragment length.

	   If 'stop_at_whitespace_separated_words', which is only used for
	   white_space value "pre", is TRUE then each white-space
	   separated word will be a fragment. Trailing white-spaces will
	   be included in length.

	 - For 'white_space' value "normal" fragments can be broken between
	   characters with the appropriate breaking properties (like between
	   LINEBREAK_ALLOW_AFTER and LINEBREAK_ALLOW_BEFORE).  If the breaking
	   character is a white-space character then all following white-spaces
	   (except FORCE_LINE_BREAK_CHAR) are collapsed and 'txt' will be
	   updated to the next non-whitespace character, but the white-spaces
	   are not included in the fragment length.

	 - 'white_space' value "nowrap" behaves like "normal" except that
	   other breaking properties then white-spaces are ignored.

	 - 'white-space' value "pre-wrap" behaves like "pre" except that
	   breaking is allowed as for "normal".	 Trailing white-spaces (except
	   breaking characters) are included in the fragment length.

	For all cases fragments will be broken at characters that are not
	supported by given 'font'.

	word_info.font_number will be updated if font for returned fragment is different
	from current_font upon entry.  */

BOOL
GetNextTextFragment(const uni_char*& txt, int length,
					UnicodePoint& last_base_character,
					CharacterClass& last_base_character_class,
					BOOL leading_whitespace,
					WordInfo& word_info,
					CSSValue white_space,
					BOOL stop_at_whitespace_separated_words,
					BOOL treat_nbsp_as_space,
					FontSupportInfo& font_info,
					FramesDocument* doc,
					WritingSystem::Script script)
{
	if (length == 0)
		return FALSE;

	if (length > WORD_INFO_LENGTH_MAX)
		// The WordInfo structure cannot handle longer fragments than this.

		length = WORD_INFO_LENGTH_MAX;

	const uni_char* wstart = txt;
	const uni_char* wend = txt;

	BOOL has_baseline = TRUE;
	BOOL next_has_baseline = TRUE;

#if defined(FONTSWITCHING) && !defined(PLATFORM_FONTSWITCHING)
	if (script == WritingSystem::Unknown)
	{
		HLDocProfile* docprof = doc ? doc->GetHLDocProfile() : NULL;
		if (docprof)
			script = docprof->GetPreferredScript();
	}
#ifdef USE_TEXTSHAPER_INTERNALLY
	TextShaper::ResetState();
#endif // USE_TEXTSHAPER_INTERNALLY

#endif // FONTSWITCHING && !PLATFORM_FONTSWITCHING

	int consumed;
	UnicodePoint uc = 0;

	short awb = WORD_BREAK_NORMAL;

	if (doc)
		awb = doc->GetAggressiveWordBreaking();

	if (white_space == CSS_VALUE_pre)
	{
		UnicodePoint puc = 0;
#ifdef SUPPORT_TEXT_DIRECTION
		BidiCategory biditype = BIDI_UNDEFINED;
#endif

		// looping to find end of word

		while (length)
		{
			uc = Unicode::GetUnicodePoint(wend, length, consumed);
			next_has_baseline = has_baseline(uc);

			if (wend > wstart && has_baseline != next_has_baseline)
				break;

			if (uc == 173) // Soft hyphen
			{
				if (wend == wstart)
					wstart++;
				else
					break;
			}
			else
			{
				if (uc == '\t')
				{
					if (wend == wstart)
					{
						wend++;
						word_info.SetIsTabCharacter(TRUE);
					}
					break;
				}
				else
					if (uc == FORCE_LINE_BREAK_CHAR || uc == '\r' || uc == '\n')
					{
						word_info.SetHasEndingNewline(TRUE);
						break;
					}
					else
						if (stop_at_whitespace_separated_words &&
							wend > wstart &&
							!uni_isspace(uc) &&
							uni_isspace(puc) && (treat_nbsp_as_space || !uni_isnbsp(puc)))
						{
							word_info.SetHasTrailingWhitespace(TRUE);
							break;
						}
#if defined(FONTSWITCHING) && !defined(PLATFORM_FONTSWITCHING)

				/* Don't fontswitch some whitespace characters, but make sure to put them in their own word. A sequence of
				   non fontswitching whitespace chars is ok. */

				if (puc != 0 && StyleManager::NoFontswitchNoncollapsingWhitespace(puc) != StyleManager::NoFontswitchNoncollapsingWhitespace(uc))
					break;

				// Check font support

#ifdef DISPLAY_INTERNAL_DIACRITICS_SUPPORT
				/* Don't switch font for diacritics when using API_DISPLAY_INTERNAL_DIACRITICS_SUPPORT. VisualDevice takes care
				   of that when measuring/painting strings and prefer getting the letters and diacritics in the same run of text. */
				if (!VisualDevice::IsKnownDiacritic(uc))
#endif // DISPLAY_INTERNAL_DIACRITICS_SUPPORT
					if (SwitchFont(wend, length, consumed, font_info, script))
						if (wend == wstart)
							// Update font number

							word_info.SetFontNumber(font_info.current_font->GetFontNumber());
						else
							// Use what we've got so far

							break;
#endif // FONTSWITCHING && !PLATFORM_FONTSWITCHING

				if (awb == WORD_BREAK_AGGRESSIVE)
				{
					if (wend == wstart)
					{
						if (last_base_character && ERA_AggressiveWordBreakAfter(last_base_character))
							word_info.SetCanLinebreakBefore(TRUE);
					}
					else
						if (ERA_AggressiveWordBreakBefore(uc))
						{
							word_info.SetCanLinebreakBefore(TRUE);
							break;
						}
						else
							if (last_base_character && ERA_AggressiveWordBreakAfter(last_base_character))
								break;
				}

#ifdef SUPPORT_TEXT_DIRECTION
				/* Ensure that bidification does not reverse text direction inside
				   a word by breaking words when bidi category changes. */

				BidiCategory bidihere = Unicode::GetBidiCategory(uc);

				/* NSM chars should just be of the same type as the preceding character. This is to help diacritical placement
				   all the control chars, CS & ES should only be one in each word. BN chars should not affect the algorithm
				   (chap 4.1) so they will also be treated as a part of the preceding word. */

				if (bidihere != biditype ||
					biditype == BIDI_RLO ||
					biditype == BIDI_LRO ||
					biditype == BIDI_RLE ||
					biditype == BIDI_LRE ||
					biditype == BIDI_PDF ||
					biditype == BIDI_ES  ||
					biditype == BIDI_CS)
					if (biditype != BIDI_UNDEFINED && bidihere != BIDI_NSM && bidihere != BIDI_BN)
							 // type changed, or last char was a CS, ES or control char (see rule W4 in bidi spec), so divide word here
						break;
					else
						if (biditype == BIDI_UNDEFINED)
							biditype = bidihere;
#endif // SUPPORT_TEXT_DIRECTION

				has_baseline = next_has_baseline;
			}

			wend += consumed;
			length -= consumed;
			puc = uc;
		}

		// found end of word
		word_info.SetLength(wend - wstart);
		txt = wend;

		if (word_info.HasEndingNewline())
			txt++;

		/* Skip '\r' in CR/LF sequences. */
		if (word_info.HasEndingNewline() && *wend == '\r' && (length > 1 && *(wend + 1) == '\n'))
			txt++;

		if (stop_at_whitespace_separated_words)
		{
			if (word_info.HasEndingNewline())
				word_info.SetHasTrailingWhitespace(TRUE);
			else
				if (length && wend > wstart)
				{
					int dummy;

					uc = Unicode::GetUnicodePoint(wend, length);
					const size_t pos = wend - wstart;
					puc = GetPreviousUnicodePoint(wstart, pos, dummy);

					if (!uni_isspace(uc) && uni_isspace(puc) && (treat_nbsp_as_space || !uni_isnbsp(puc)))
						word_info.SetHasTrailingWhitespace(TRUE);
				}
		}
	}
	else
	{
#if defined(FONTSWITCHING) && !defined(PLATFORM_FONTSWITCHING)
		UnicodePoint puc = 0;
#endif // FONTSWITCHING && !PLATFORM_FONTSWITCHING

#ifdef SUPPORT_TEXT_DIRECTION
		BidiCategory biditype = BIDI_UNDEFINED;
#endif

		// scan all non-breakable characters

		while (length)
		{
			uc = Unicode::GetUnicodePoint(wend, length, consumed);
			if (layout_collapse_space(uc, treat_nbsp_as_space))
				break;

			CharacterClass character_class = Unicode::GetCharacterClass(uc);

			if (!UnicodeSegmenter::IsGraphemeClusterBoundary(last_base_character, last_base_character_class, uc, character_class))
			{
				wend += consumed;
				length -= consumed;
			}
			else
			{
				BOOL ignore_base_character = FALSE;

				next_has_baseline = has_baseline(uc);

				if (wend > wstart && has_baseline != next_has_baseline)
					break;

				if (uc == 173) // Soft hyphen
				{
					if (wend == wstart)
					{
						last_base_character = uc;
						leading_whitespace = FALSE;

						word_info.SetLength(1);
						txt = wend + 1;
						word_info.SetHasBaseline(has_baseline);

						return TRUE;
					}

					break;
				}
				else
					if ((character_class == CC_Cf && uc != 0x200b) ||
						character_class == CC_Cc)
						ignore_base_character = TRUE;
					else
					{
						if (white_space != CSS_VALUE_nowrap && Unicode::IsLinebreakOpportunity(last_base_character, wend, length, leading_whitespace) == LB_YES)
						{
							if (wend == wstart)
								word_info.SetCanLinebreakBefore(TRUE);
							else
								break;
						}
						else
							if (awb == WORD_BREAK_AGGRESSIVE)
								if (wend == wstart)
								{
									if (last_base_character && ERA_AggressiveWordBreakAfter(last_base_character))
										word_info.SetCanLinebreakBefore(TRUE);
								}
								else
									if (ERA_AggressiveWordBreakBefore(uc))
									{
										word_info.SetCanLinebreakBefore(TRUE);
										break;
									}
									else
										if (last_base_character && ERA_AggressiveWordBreakAfter(last_base_character))
											break;
					}

#if defined(FONTSWITCHING) && !defined(PLATFORM_FONTSWITCHING)

				/* Don't fontswitch some whitespace characters, but make sure to put them in their own word. A sequence of
				   non fontswitching whitespace chars is ok. */

				if (puc != 0 && StyleManager::NoFontswitchNoncollapsingWhitespace(puc) != StyleManager::NoFontswitchNoncollapsingWhitespace(uc))
					break;

				// Check font support

#ifdef DISPLAY_INTERNAL_DIACRITICS_SUPPORT
				/* Don't switch font for diacritics when using API_DISPLAY_INTERNAL_DIACRITICS_SUPPORT. VisualDevice takes care
				   of that when measuring/painting strings and prefer getting the letters and diacritics in the same run of text.
				   Checking the character_class first so we don't do unnecessary calls to it (optimization) */
				if (!(character_class == CC_Mn && VisualDevice::IsKnownDiacritic(uc)))
#endif // DISPLAY_INTERNAL_DIACRITICS_SUPPORT
					if (SwitchFont(wend, length, consumed, font_info, script))
						if (wend == wstart)
							// Update font number

							word_info.SetFontNumber(font_info.current_font->GetFontNumber());
						else
							// Use what we've got so far

							break;
#endif // FONTSWITCHING && !PLATFORM_FONTSWITCHING

#ifdef SUPPORT_TEXT_DIRECTION
				/* Ensure that bidification does not reverse text direction inside
				   a word by breaking words when bidi category changes. */

				BidiCategory bidihere = Unicode::GetBidiCategory(uc);

				/* NSM chars should just be of the same type as the preceding
				   character. This is to help diacritical placement all the
				   control chars, CS & ES should only be one in each word.
				   BN chars should not affect the algorithm (chap 4.1) so they will
				   also be treated as a part of the preceding word. */

				if (bidihere != biditype ||
					biditype == BIDI_RLO ||
					biditype == BIDI_LRO ||
					biditype == BIDI_RLE ||
					biditype == BIDI_LRE ||
					biditype == BIDI_PDF ||
					biditype == BIDI_ES  ||
					biditype == BIDI_CS)
					if (biditype != BIDI_UNDEFINED && bidihere != BIDI_NSM && bidihere != BIDI_BN)
						// type changed, or last char was a CS || ES (see rule W4 in bidi spec) so divide word here
						break;
					else
						if (biditype == BIDI_UNDEFINED) // NSM words are allowed if the NSMs are in the beginning of the word
							biditype = bidihere;
#endif // SUPPORT_TEXT_DIRECTION

				leading_whitespace = FALSE;
				has_baseline = next_has_baseline;

				if (!ignore_base_character)
				{
					last_base_character = uc;
					last_base_character_class = character_class;
				}

#if defined(FONTSWITCHING) && !defined(PLATFORM_FONTSWITCHING)
				puc = uc;
#endif // FONTSWITCHING && !PLATFORM_FONTSWITCHING

				wend += consumed;
				length -= consumed;
			}
		}

		// if we don't care about the white-spaces this is the end

		if (white_space != CSS_VALUE_pre_wrap)
 			word_info.SetLength(wend - wstart);

		// eat up the trailing white-spaces

		int skip_characters = 0;
#ifdef SUPPORT_TEXT_DIRECTION
		const uni_char* space_start = wend;
#endif

		while (length)
		{
			uc = Unicode::GetUnicodePoint(wend, length, consumed);
			if (!layout_collapse_space(uc, treat_nbsp_as_space))
				break;

			word_info.SetHasTrailingWhitespace(TRUE);

			if (uc == FORCE_LINE_BREAK_CHAR)
			{
				skip_characters = consumed;
				word_info.SetHasEndingNewline(TRUE);

				break;
			}
			else
				if (white_space == CSS_VALUE_pre_line || white_space == CSS_VALUE_pre_wrap)
					if (uc == '\r' || uc == '\n')
					{
						skip_characters = 1;
						word_info.SetHasEndingNewline(TRUE);

						if (uc == '\r' && (length > 1 && *(wend + 1) == '\n'))
							skip_characters++;

						break;
					}
					else
						if (white_space == CSS_VALUE_pre_wrap &&
							uc == '\t')
						{
							if (wend == wstart)
							{
								wend++;
								word_info.SetIsTabCharacter(TRUE);
							}
							break;
						}

			wend += consumed;
			length -= consumed;
		}

#ifdef SUPPORT_TEXT_DIRECTION
		if ((biditype != Unicode::GetBidiCategory(uc) && biditype != BIDI_UNDEFINED) ||
		    biditype == BIDI_EN || biditype == BIDI_AN ||
			(white_space == CSS_VALUE_pre_wrap && (biditype == BIDI_R || biditype == BIDI_AL)))
		{
			/* These whitespace characters are at a point where the text's bidi
			   direction changes. Therefore they must have a word of their own. */

			length += space_start - wend;
			wend = space_start;
			word_info.SetHasTrailingWhitespace(FALSE); // FIXME: also turn off rest
			word_info.SetHasEndingNewline(FALSE);
			skip_characters = 0;
		}
#endif

		// include trailing white-spaces in length if "pre-wrap"

		if (white_space == CSS_VALUE_pre_wrap)
			word_info.SetLength(wend - wstart);

		txt = wend + skip_characters;
	}

	word_info.SetHasBaseline(has_baseline);

	return TRUE;
}

BOOL
GetNextTextFragment(const uni_char*& txt, int length,
					UnicodePoint& last_base_character,
					BOOL leading_whitespace,
					WordInfo& word_info,
					CSSValue white_space,
					BOOL stop_at_whitespace_separated_words,
					BOOL treat_nbsp_as_space,
					FontSupportInfo& font_info,
					FramesDocument* doc,
					WritingSystem::Script script)
{
	CharacterClass last_base_character_class = Unicode::GetCharacterClass(last_base_character);

	return GetNextTextFragment(txt, length, last_base_character, last_base_character_class, leading_whitespace, word_info, white_space, stop_at_whitespace_separated_words, treat_nbsp_as_space, font_info, doc, script);
}

BOOL
GetNextTextFragment(const uni_char*& txt, int length,
					WordInfo& word_info,
					CSSValue white_space,
					BOOL stop_at_whitespace_separated_words,
					BOOL treat_nbsp_as_space,
					FontSupportInfo& font_info,
					FramesDocument* doc,
					WritingSystem::Script script)
{
	UnicodePoint last_base_character = 0;
	CharacterClass last_base_character_class = CC_Unknown;

	return GetNextTextFragment(txt, length, last_base_character, last_base_character_class, FALSE, word_info, white_space, stop_at_whitespace_separated_words, treat_nbsp_as_space, font_info, doc, script);
}

/** Scale minimum width of percentage width content to avoid overflow. */

static LayoutCoord
ScalePropagationWidthForPercent(const HTMLayoutProperties& props, LayoutCoord width, LayoutCoord nonpercent_horizontal_margin)
{
	/* Percentage in FlexRoot.

	   Scale up the minimum width width, so that we basically tell the
	   containing block: "You have to be this wide to prevent me from
	   overflowing". Horizontal margin (and, for box-sizing:content-box, border
	   and padding) will need to fit inside the unused percentage.

	   Calculate how wide the containing block needs to be for percentage width
	   resolution of this box not to overflow it. Calculate how wide it needs
	   to be for non-percentage portion of the box to fit beside that without
	   overflowing it. Propagate the larger of these two values. If percentage
	   is larger than 100, it is impossible to avoid overflow.

	   FIXME: Percentual values for margin and padding properties have already
	   been changed from resolved pixel values to 0 at this point (because
	   we're in a measure pass). What we really would need here are the
	   original percentage values for margin, padding, min-width and
	   max-width. However, they are resolved too early, when calculating the
	   cascade. See also bug 292324. */

	LayoutFixed percent = LayoutFixed(-props.content_width);
	LayoutFixed unused_percent = LAYOUT_FIXED_HUNDRED_PERCENT - percent;
	LayoutCoord percent_requirement;
	LayoutCoord nonpercent_requirement = LayoutCoord(0);

	if (props.box_sizing == CSS_VALUE_content_box)
	{
		LayoutCoord border_padding = LayoutCoord(props.GetNonPercentHorizontalBorderPadding());

		OP_ASSERT(border_padding <= width);
		percent_requirement = ReversePercentageToLayoutCoord(percent, width - border_padding);

		if (unused_percent > LayoutFixed(0))
			nonpercent_requirement = ReversePercentageToLayoutCoord(unused_percent, border_padding + nonpercent_horizontal_margin);
	}
	else
	{
		percent_requirement = ReversePercentageToLayoutCoord(percent, width);

		if (unused_percent > LayoutFixed(0))
			nonpercent_requirement = ReversePercentageToLayoutCoord(unused_percent, nonpercent_horizontal_margin);
	}

	return MAX(LayoutCoord(0), MAX(percent_requirement, nonpercent_requirement));
}

void
CssValuesToBreakPolicies(const LayoutInfo& info,
						 LayoutProperties* cascade,
						 BREAK_POLICY& page_break_before,
						 BREAK_POLICY& page_break_after,
						 BREAK_POLICY& column_break_before,
						 BREAK_POLICY& column_break_after)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	CssValueToBreakPolicy((CSSValue) props.page_props.break_before, page_break_before, column_break_before);
	CssValueToBreakPolicy((CSSValue) props.page_props.break_after, page_break_after, column_break_after);

	if (!cascade->FindMultiPaneContainer(FALSE))
		/* Not inside of a multi-column container, or there is a paged
		   container between us and an ancestor multicol container, and columns
		   cannot be broken in the middle of a paged container. */

		column_break_before = column_break_after = BREAK_ALLOW;

	if (
#ifdef PAGED_MEDIA_SUPPORT
		info.paged_media == PAGEBREAK_OFF &&
#endif // PAGED_MEDIA_SUPPORT
		!cascade->FindMultiPaneContainer(TRUE))
		/* Not inside a paged container, and not in paged media mode. There can
		   be no page breaks when we're not paginating anything. */

		page_break_before = page_break_after = BREAK_ALLOW;
}

#ifdef PAGED_MEDIA_SUPPORT

/** Find the paged container, if any, associated with the given box.

	This will normally simply be the paged container established by this box,
	but in the case of root and BODY elements, we may have to use the
	viewport's paged container instead.  */

static PagedContainer*
FindPagedContainer(Box* box)
{
	PagedContainer* paged_container = NULL;

	if (Container* inner_container = box->GetContainer())
		paged_container = inner_container->GetPagedContainer();

	if (!paged_container)
	{
		HTML_Element* elm = box->GetHtmlElement();

		if (elm->IsMatchingType(Markup::HTE_BODY, NS_HTML))
			elm = elm->Parent();

		if (elm->IsMatchingType(Markup::HTE_HTML, NS_HTML))
			elm = elm->Parent();

		if (elm->Type() == Markup::HTE_DOC_ROOT)
			if (Container* inner_container = elm->GetLayoutBox()->GetContainer())
				paged_container = inner_container->GetPagedContainer();
	}

	return paged_container;
}

#endif // PAGED_MEDIA_SUPPORT

LayoutInfo::LayoutInfo(LayoutWorkplace* wp)
  : workplace(wp),
#ifdef LAYOUT_YIELD_REFLOW
	force_layout_changed(FALSE),
#endif
#ifdef PAGED_MEDIA_SUPPORT
	paged_media(PAGEBREAK_OFF),
	pending_pagebreak_found(FALSE),
#endif // PAGED_MEDIA_SUPPORT
	stop_before(NULL),
	start_selection_done(FALSE),
	external_layout_change(FALSE),
	allow_word_split(FALSE),
	yield_count(0),
	translation_x(0),
	root_translation_x(0),
	inline_translation_x(0),
	nonpos_scroll_x(0),
	translation_y(0),
	root_translation_y(0),
	inline_translation_y(0),
	nonpos_scroll_y(0),
	quote_level(0)
{
	doc = wp->GetFramesDocument();
	if (doc)
	{
		PrefsCollectionDisplay::RenderingModes rendering_mode = PrefsCollectionDisplay::MSR;
		switch (doc->GetLayoutMode())
		{
		case LAYOUT_SSR:
			rendering_mode = PrefsCollectionDisplay::SSR;
			break;
		case LAYOUT_CSSR:
			rendering_mode = PrefsCollectionDisplay::CSSR;
			break;
		case LAYOUT_AMSR:
			rendering_mode = PrefsCollectionDisplay::AMSR;
			break;
#ifdef TV_RENDERING
		case LAYOUT_TVR:
			rendering_mode = PrefsCollectionDisplay::TVR;
			break;
#endif // TV_RENDERING
		}

		visual_device = doc->GetVisualDevice();
		hld_profile = doc->GetHLDocProfile();

		text_selection = doc->GetTextSelection();
		if (text_selection && text_selection->IsEmpty())
			text_selection = NULL;

		if (doc->GetLayoutMode() != LAYOUT_NORMAL /*&& !doc->GetMediaHandheldResponded()*/)
		{
			column_stretch = (ColumnStretch)g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::ColumnStretch));
			table_strategy = (TableStrategy)g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::TableStrategy));
			flexible_columns = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::FlexibleColumns)) == 1;
			flexible_width = (FlexibleReplacedWidth)g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::FlexibleImageSizes));
			treat_nbsp_as_space = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::ConvertNbspIntoNormalSpace)) == 1;
#ifdef CONTENT_MAGIC
			content_magic = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::TableMagic)) == 1 && !doc->IsInlineFrame();
#endif // CONTENT_MAGIC
			allow_word_split = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::SplitHideLongWords)) >= 1;
		}
		else
		{
			column_stretch = COLUMN_STRETCH_NORMAL;
			table_strategy = TABLE_STRATEGY_NORMAL;
			flexible_columns = FALSE;
			treat_nbsp_as_space = FALSE;
			allow_word_split = doc->GetHandheldEnabled();
#ifdef CONTENT_MAGIC
			content_magic = FALSE;
#endif // CONTENT_MAGIC
		}

		media_type = doc->GetMediaType();

#ifdef PAGED_MEDIA_SUPPORT
		if (CSS_IsPagedMedium(media_type))
			paged_media = PAGEBREAK_ON;
		else
			paged_media = PAGEBREAK_OFF;
#endif // PAGED_MEDIA_SUPPORT

		using_flexroot = wp->UsingFlexRoot();
	}
	else
	{
		// this might happen when you import Netscape bookmarks

		visual_device = NULL;
		media_type = CSS_MEDIA_TYPE_SCREEN;
		text_selection = NULL;
		column_stretch = COLUMN_STRETCH_NORMAL;
		table_strategy = TABLE_STRATEGY_NORMAL;
		flexible_columns = FALSE;
		treat_nbsp_as_space = FALSE;
#ifdef CONTENT_MAGIC
		content_magic = FALSE;
#endif // CONTENT_MAGIC
		using_flexroot = FALSE;
	}
#ifdef LAYOUT_YIELD_REFLOW
	force_layout_changed = wp->GetYieldForceLayoutChanged() == LayoutWorkplace::FORCE_ALL;
#endif

	fixed_pos_x = wp->GetLayoutViewX();
	fixed_pos_y = wp->GetLayoutViewY();
}

/** Union 'other' bounding box with this one. See declaration in header file for more information. */

void
RelativeBoundingBox::UnionWith(const AbsoluteBoundingBox& other, LayoutCoord box_width, LayoutCoord box_height, BOOL skip_content)
{
	if (other.GetX() == LAYOUT_COORD_MIN)
		left = LAYOUT_COORD_MAX;
	else
		if (left < -other.GetX())
			left = -other.GetX();

	if (other.GetY() == LAYOUT_COORD_MIN)
		top = LAYOUT_COORD_MAX;
	else
		if (top < -other.GetY())
			top = -other.GetY();

	if (other.GetWidth() == LAYOUT_COORD_MAX)
		right = LAYOUT_COORD_MAX;
	else
		if (box_width + right < other.GetX() + other.GetWidth())
			right = other.GetX() + other.GetWidth() - box_width;

	if (other.GetHeight() == LAYOUT_COORD_MAX)
		bottom = LAYOUT_COORD_MAX;
	else
		if (box_height + bottom < other.GetY() + other.GetHeight())
			bottom = other.GetY() + other.GetHeight() - box_height;

	if (!skip_content)
	{
		if (other.GetContentWidth() == LAYOUT_COORD_MAX)
			content_right = LAYOUT_COORD_MAX;
		else
			if (box_width + content_right < other.GetX() + other.GetContentWidth())
				content_right = other.GetX() + other.GetContentWidth() - box_width;

		if (other.GetContentHeight() == LAYOUT_COORD_MAX)
			content_bottom = LAYOUT_COORD_MAX;
		else
			if (box_height + content_bottom < other.GetY() + other.GetContentHeight())
				content_bottom = other.GetY() + other.GetContentHeight() - box_height;
	}
}

/** Reset the bounding box to a specific value. */

void
RelativeBoundingBox::Reset(LayoutCoord value, LayoutCoord content_value)
{
	left = value; right = value; top = value; bottom = value;
	content_right = content_value; content_bottom = content_value;
}

/** Reduce right and bottom part of relative bounding box. */

void
RelativeBoundingBox::Reduce(LayoutCoord b, LayoutCoord r, LayoutCoord min_value)
{
	// Negative bottom and right values are OK - will increase the bounding box then.

	if (bottom && bottom != LAYOUT_COORD_MAX)
	{
		if (b > bottom)
			bottom = LayoutCoord(0);
		else
			bottom -= b;

		if (bottom < min_value)
			bottom = min_value;
	}

	if (right && right != LAYOUT_COORD_MAX)
	{
		if (r > right)
			right = LayoutCoord(0);
		else
			right -= r;

		if (right < min_value)
			right = min_value;
	}

	if (content_bottom && content_bottom != LAYOUT_COORD_MAX)
	{
		if (b > content_bottom)
			content_bottom = LayoutCoord(0);
		else
			content_bottom -= b;

		if (content_bottom < 0)
			content_bottom = LayoutCoord(0);
	}
}

/** Calculate the absolute bounding box. */

void
AbsoluteBoundingBox::Set(const RelativeBoundingBox& box, LayoutCoord box_width, LayoutCoord box_height)
{
	LayoutCoord w = box.TotalWidth(box_width);
	LayoutCoord cw = box.TotalContentWidth(box_width);

	if (w < 0)
		w = LayoutCoord(0);

	if (cw < 0)
		cw = LayoutCoord(0);

	x = -box.Left();
	y = box.TopIsInfinite() ? LAYOUT_COORD_MIN/LayoutCoord(2) : -box.Top();

	width = w;
	content_width = cw;

	height = box.TotalHeight(box_height);
	content_height = box.TotalContentHeight(box_height);

	if (height < 0)
		height = LayoutCoord(0);

	if (content_height < 0)
		content_height = LayoutCoord(0);
}

/** Sets new bounding box from given parameters */

void
AbsoluteBoundingBox::Set(LayoutCoord new_x, LayoutCoord new_y, LayoutCoord new_width, LayoutCoord new_height)
{
	x = new_x;
	y = new_y;
	width = new_width;
	height = new_height;

	if (content_width > width)
		content_width = width;

	if (content_height > height)
		content_height = height;
}

/** Translates bounding box by given offset */

void
AbsoluteBoundingBox::Translate(LayoutCoord offset_x, LayoutCoord offset_y)
{
	x += offset_x;
	y += offset_y;
}

/** Grows bounding box to given max size. May grow content box as well */

void
AbsoluteBoundingBox::Grow(LayoutCoord max_width, LayoutCoord max_height)
{
	if (width < max_width)
		width = max_width;

	if (height < max_height)
		height = max_height;

	if (content_width < max_width)
		content_width = max_width;

	if (content_height < max_height)
		content_height = max_height;
}

/** Union this bounding box with another one. */

void
AbsoluteBoundingBox::UnionWith(const AbsoluteBoundingBox& other)
{
	if (x > other.x)
	{
		width += x - other.x;
		content_width += x - other.x;
		x = other.x;
	}

	if (y > other.y)
	{
		height += y - other.y;
		content_height += y - other.y;
		y = other.y;
	}

	if (x + width < other.x + other.width)
		width = other.x + other.width - x;

	if (y + height < other.y + other.height)
		height = other.y + other.height - y;

	if (x + content_width < other.x + other.content_width)
		content_width = other.x + other.content_width - x;

	if (y + content_height < other.y + other.content_height)
		content_height = other.y + other.content_height - y;
}

/** Intersect this bounding box with another one. */

void
AbsoluteBoundingBox::IntersectWith(const AbsoluteBoundingBox& other)
{
	LayoutCoord new_x = MAX(x, other.x);
	LayoutCoord new_y = MAX(y, other.y);
	LayoutCoord new_right = MIN(x + width, other.x + other.width);
	LayoutCoord new_bottom = MIN(y + height, other.y + other.height);
	LayoutCoord new_content_right = MIN(x + content_width, other.x + other.content_width);
	LayoutCoord new_content_bottom = MIN(y + content_height, other.y + other.content_height);

	x = new_x;
	y = new_y;

	if (new_right > new_x && new_bottom > new_y)
	{
		width = new_right - new_x;
		height = new_bottom - new_y;
	}
	else
		width = height = LayoutCoord(0);

	if (new_content_right > new_x && new_content_bottom > new_y)
	{
		content_width = new_content_right - new_x;
		content_height = new_content_bottom - new_y;
	}
	else
		content_width = content_height = LayoutCoord(0);
}

/** Clip bounding box to a clipping area (if set) */

void
AbsoluteBoundingBox::ClipTo(LayoutCoord clip_left, LayoutCoord clip_top, LayoutCoord clip_right, LayoutCoord clip_bottom)
{
	if (clip_left == CLIP_NOT_SET)
		return;

	LayoutCoord bbox_left = clip_left != CLIP_AUTO ? MAX(x, clip_left) : x;
	LayoutCoord bbox_top = clip_top != CLIP_AUTO ? MAX(y, clip_top) : y;

	LayoutCoord bbox_right = clip_right != CLIP_AUTO ? MIN(x + width, clip_right) : x + width;
	LayoutCoord bbox_bottom = clip_bottom != CLIP_AUTO ? MIN(y + height, clip_bottom) : y + height;

	LayoutCoord cbox_right = MIN(x + content_width, bbox_right);
	LayoutCoord cbox_bottom = MIN(y + content_height, bbox_bottom);

	x = bbox_left;
	y = bbox_top;
	width = bbox_right > bbox_left ? bbox_right - bbox_left : LayoutCoord(0);
	height = bbox_bottom > bbox_top ? bbox_bottom - bbox_top : LayoutCoord(0);

	content_width = cbox_right > bbox_left ? cbox_right - bbox_left : LayoutCoord(0);
	content_height = cbox_bottom > bbox_top ? cbox_bottom - bbox_top : LayoutCoord(0);
}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

/** Signal that the HTML_Element associated with this layout box about to be deleted. */

/* virtual */ void
Box::SignalHtmlElementDeleted(FramesDocument* doc)
{
	HTML_Element* containing_element = GetContainingElement();

	if (containing_element)
		if (Container* container = containing_element->GetLayoutBox()->GetContainer())
			container->SignalChildDeleted(doc, this);
}

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

BOOL
Box::GetRectList(FramesDocument* doc, BoxRectType type, RectList &rect_list)
{
	AffinePos ctm;
	RECT single_rect;
	BOOL result = GetRect(doc, type, ctm, single_rect, &rect_list, 0, -1);

	/* If the list is empty, we populate it with the single rectangle
	   we obtained from GetRect. This avoids having to populate the
	   list in a bunch of different places. In fact, the only place
	   the list is populated (except here) is when we have a inline
	   box on multiple lines. All other cases fall into the single
	   rect case. */
	if (result && rect_list.First() == NULL)
	{
		RectListItem *rect_item = OP_NEW(RectListItem, (single_rect));

		if (rect_item)
			rect_item->Into(&rect_list);
		else
			result = FALSE;
	}
 	else if (!result && rect_list.First())
 		rect_list.Clear();

#ifdef CSS_TRANSFORMS
	if (ctm.IsTransform())
		for (RectListItem *iter = rect_list.First();
			 iter != NULL;
			 iter = iter->Suc())
		{
			ctm.Apply(iter->rect);
		}
#endif // CSS_TRANSFORMS

	return result;
}

BOOL
Box::GetRect(FramesDocument* doc, BoxRectType type, RECT& rect, int element_text_start /* = 0 */, int element_text_end /* = -1 */)
{
	return GetBBox(doc, type, rect, NULL, element_text_start, element_text_end);
}

BOOL
Box::GetRect(FramesDocument* doc, BoxRectType type, AffinePos &ctm, RECT& rect, int element_text_start /* = 0 */, int element_text_end /* = -1 */)
{
	return GetRect(doc, type, ctm, rect, NULL, element_text_start, element_text_end);
}

BOOL
Box::GetBBox(FramesDocument* doc, BoxRectType type, RECT& rect, int element_text_start /* = 0 */, int element_text_end /* = -1 */)
{
	return GetBBox(doc, type, rect, NULL, element_text_start, element_text_end);
}

BOOL
Box::GetBBox(FramesDocument* doc, BoxRectType type, RECT& rect, RectList *rect_list, int element_text_start /* = 0 */, int element_text_end /* = -1 */)
{
	AffinePos ctm;
	if (!GetRect(doc, type, ctm, rect, rect_list, element_text_start, element_text_end))
		return FALSE;

	if (type != OFFSET_BOX)

		/* The offset box is relative to the containing element, don't
		   apply transform. */

		ctm.Apply(rect);

	return TRUE;
}

void
Box::GetContentEdges(const HTMLayoutProperties &props, OpRect& rect, OpPoint& offset) const
{
	OP_ASSERT(props.box_sizing != CSS_VALUE_padding_box);

	if (props.box_sizing == CSS_VALUE_content_box)
	{
		rect = GetPaddingEdges(props);
		rect.OffsetBy(props.padding_left, props.padding_top);
		rect.width -= props.padding_left + props.padding_right;
		rect.height -= props.padding_top + props.padding_bottom;
	}
	else
		rect = GetBorderEdges();

	if (ScrollableArea* sc = GetScrollable())
	{
		int sb_width = sc->GetVerticalScrollbarWidth();

		rect.width -= sb_width;
		rect.height -= sc->GetHorizontalScrollbarWidth();

		if (sc->LeftHandScrollbar())
			rect.x += sb_width;

		offset.x = sc->GetViewX();
		offset.y = sc->GetViewY();
	}
	else
		offset.x = offset.y = 0;
}

/* Private function of box that takes three result arguments, an
 * affine transform, a rect and a pointer to a list. If the rect is a
 * union, the sub-rects are added to the list (if the pointer is
 * non-NULL). The affine transform is empty unless there is a
 * transformed ancestor.
 */

BOOL
Box::GetRect(FramesDocument* doc, BoxRectType type, AffinePos &ctm, RECT &rect, RectList *rect_list, int element_text_start, int element_text_end)
{
	OP_ASSERT(doc);
	LayoutWorkplace* layout_workplace = doc->GetLogicalDocument()->GetLayoutWorkplace();

	/* If this method is called from Reflow, the result is unreliable.
	   If this method is called from Reflow, and it needs to do a partial
	   traversal to find the result, it's dangerous. Even creating cascade
	   elements is problematic. For instance, setting font in VisualDevice
	   when creating cascade elements will affect fonts used for the reflow. */
	OP_ASSERT(layout_workplace && !layout_workplace->IsReflowing());

	if (layout_workplace->IsReflowing())
		return FALSE;

	HTML_Element* element = GetHtmlElement();

    /* Getting splitted boxes into the rect_list is only supported for
	   the type 'content box' and 'border box'. This assumption is
	   used below when adjusting the rectangles with top_offset and
	   left_offset. */
	OP_ASSERT(!rect_list || type == BORDER_BOX || type == CONTENT_BOX || type == ENCLOSING_BOX);

	/* No sense in calling with text values on non-text elements. */
	OP_ASSERT(element->Type() == Markup::HTE_TEXT || (element_text_start == 0 && element_text_end == -1));

	/* Default is no transform */
	ctm.SetTranslation(0,0);

    if (element)
    {
		BOOL is_scroll_box = FALSE;

		rect.top = 0;
		rect.left = 0;
		rect.right = 0;
		rect.bottom = 0;

		switch (type)
		{
		case SCROLL_BOX:
			{
				/* The scroll box is used (only) from DOM to support the scroll{Left,Top,Width,Height}
				   properties on HTML elements. These properties are not properly defined yet,
				   although the CSSOM View spec is slowly progressing. There are currently
				   a number of bugs that instead dictate our behavior, as well as trying to follow the
				   Mozilla/IE precedent.

				   See bugs (and corresponding test cases):

				   - DSK-101532. This bug is the reason we use the documents width and height for
				                 right and bottom of body element in quirk mode.

				   - CORE-13.    This bug disallowed use from treating <html> and <body> the same.
				                 Some sites relied on one of <body>|<html>.scroll{Left,Top} being zero.

				   - CORE-8025.  This bug originates from #275660. This bug makes us use the size of
				                 this box (instead of viewport size) to compute right and bottom.

				   - CORE-10016. Overflow should be included for <html> in strict, so we use the bounding box.

				   - CORE-13815. Don't use the negative overflow for <html> in strict. We don't allow
				                 scrolling to that area anyway.

				   CORE-37472 was not specifically about these properties, but it modified scrollbar calculation
				   and updated calculation of these properties to match more closely Mozilla/IE as well as
				   the modified scrollbar code.
				*/

				if (doc->GetLogicalDocument()->GetRoot()->IsAncestorOf(element))
				{
					if (doc->GetHLDocProfile()->IsInStrictMode()
						&& element->IsMatchingType(Markup::HTE_HTML, NS_HTML)
						&& element->Parent()->Type() == Markup::HTE_DOC_ROOT)
					{
#ifdef PAGED_MEDIA_SUPPORT
						if (RootPagedContainer* root_pc = layout_workplace->GetRootPagedContainer())
						{
							rect.left = root_pc->GetViewX();
							rect.right = rect.left + root_pc->GetScrollPaddingBoxWidth();
							rect.top = root_pc->GetViewY();
							rect.bottom = rect.top + root_pc->GetScrollPaddingBoxHeight();

							return TRUE;
						}
#endif // PAGED_MEDIA_SUPPORT

						/* In strict (standards) mode, for the top-most html element in the tree, delegate to
						   the initial containing block for dimensions and the viewport for coordinates.
						   See also note at the public GetRect function regarding the two meanings of SCROLL_BOX. */

						rect.left = doc->GetVisualViewX();
						rect.top = doc->GetVisualViewY();

						AbsoluteBoundingBox parent_bounding_box;
						static_cast<BlockBox *>(element->Parent()->GetLayoutBox())->GetBoundingBox(parent_bounding_box, TRUE);

						rect.right = rect.left + parent_bounding_box.GetContentWidth();
						rect.bottom = rect.top + parent_bounding_box.GetContentHeight();

						if (parent_bounding_box.GetX() < 0)
							rect.right += parent_bounding_box.GetX();

						if (parent_bounding_box.GetY() < 0)
							rect.bottom += parent_bounding_box.GetY();

						return TRUE;
					}
					else if (!doc->GetHLDocProfile()->IsInStrictMode() &&
							 element->IsMatchingType(Markup::HTE_BODY, NS_HTML)
#ifdef PAGED_MEDIA_SUPPORT
							 && !layout_workplace->GetRootPagedContainer()
#endif // PAGED_MEDIA_SUPPORT
						)
					{
						/* In quirks mode, for (the) body element in the tree, return the scrolling position
						   in left and top. Use the width and height of the document to caluculate right and
						   bottom. */

						rect.left = doc->GetVisualViewX();
						rect.top = doc->GetVisualViewY();
						rect.right = rect.left + doc->Width();
						rect.bottom = rect.top + doc->Height();
						return TRUE;
					}
				}

				if (Content* content = GetContent())
					if (ScrollableArea* sc = content->GetScrollable())
					{
						rect.top = sc->GetViewY();
						rect.left = sc->GetViewX();
						rect.right = rect.left + sc->GetScrollPaddingBoxWidth();
						rect.bottom = rect.top + sc->GetScrollPaddingBoxHeight();
						return TRUE;
					}
#ifdef PAGED_MEDIA_SUPPORT
					else
						if (PagedContainer* pc = content->GetPagedContainer())
						{
							rect.left = pc->GetViewX();
							rect.right = rect.left + pc->GetScrollPaddingBoxWidth();
							rect.top = pc->GetViewY();
							rect.bottom = rect.top + pc->GetScrollPaddingBoxHeight();
							return TRUE;
						}
#endif // PAGED_MEDIA_SUPPORT

				if (IsBlockBox() || IsTableCell() || IsTableCaption())
                {
					rect.right = rect.left + GetWidth();
					rect.bottom = rect.top + GetHeight();
					return TRUE;
                }
			}
			is_scroll_box = TRUE;
            type = OFFSET_BOX; // Use offset_box values for width and height if this is not a block.

		case PADDING_BOX:
		case BORDER_BOX:
		case OFFSET_BOX:
		case BOUNDING_BOX:
		case ENCLOSING_BOX:
		case CONTENT_BOX:
			{
				Head props_list;

	            VisualDevice* visual_device = doc->GetVisualDevice();

				BOOL is_positioned = IsPositionedBox();
				int getpos_info = 0;
				BOOL box_found = FALSE;
				LayoutCoord left_offset(0);
				LayoutCoord top_offset(0);

				LayoutProperties *offset_cascade = NULL;
				HTML_Element *offset_parent = 0;

				if (type == OFFSET_BOX)
				{
					/*
					 * We define offsetLeft and offsetTop like this (attempting to copy MSIE's
					 * behavior as much as reasonable):
					 *
					 * The offsetLeft and offsetTop attributes of HTMLElement node A must be the number
					 * of pixels that the top border edge of A is to the right of and below the top
					 * border edge of A.offsetParent when A.offsetParent is not null and does not
					 * implement the HTMLBodyElement interface ([CSS21], section 8.1). Otherwise
					 * (A.offsetParent is null or implements the HTMLBodyElement interface), offsetLeft
					 * and offsetTop must be the number of pixels that the top border edge of node A is
					 * to the right of and below the origin (0,0) of the initial containing block.
					 *
					 * When node A implements the HTMLBodyElement interface, offsetLeft and offsetTop
					 * must be 0.
					 *
					 * An up-to-date version of the complete offset* spec can probably be found
					 * here: http://dev.w3.org/csswg/cssom-view/#offset-attributes
					 */

					if (!LayoutProperties::CreateCascade(element, props_list, doc->GetHLDocProfile()))
						return FALSE; // OOM

					offset_cascade = ((LayoutProperties *)props_list.Last())->FindOffsetParent(doc->GetHLDocProfile());
					if (offset_cascade)
					{
						offset_parent = offset_cascade->html_element;

						if (offset_parent &&
							offset_parent->IsMatchingType(Markup::HTE_BODY, NS_HTML) &&
							offset_cascade->GetProps()->position == CSS_VALUE_static)
						{
							/* Add offset from BODY to root now, which is contrary to what we
							   normally do (offset from border edge of node A to border edge of
							   A.offsetParent). */

							while (offset_cascade && offset_cascade->html_element && offset_cascade->html_element->Type() != Markup::HTE_DOC_ROOT)
								offset_cascade = offset_cascade->Pred();

							offset_parent = offset_cascade ? offset_cascade->html_element : 0;
						}
					}
				}

				HTML_Element *traverse_root = NULL; /* If we have to traverse, start traverse from this element */

				/* For table elements, list mode should return the table box
				   and caption box (if present). See bug CORE-11687. We must
				   traverse to find these. Currently this is implemented for
				   non-bounding box boxes.*/

				if ((IsBlockBox() && !IsFloatedPaneBox() && (!IsAbsolutePositionedBox() ||
															 !((AbsolutePositionedBox*) this)->IsHypotheticalHorizontalStaticInline() ||
															 !((AbsolutePositionedBox*) this)->TraverseInLine()) &&
					!(rect_list && GetTableContent() != NULL)) || IsTableCell() || IsTableCaption())
				{
					box_found = TRUE;

					if (type == OFFSET_BOX)
					{
						AbsoluteBoundingBox border_box;

						((VerticalBox*)this)->GetBoundingBox(border_box, FALSE, TRUE, FALSE);

						rect.left = border_box.GetX();
						rect.top = border_box.GetY();
						rect.right = rect.left + border_box.GetWidth();
						rect.bottom = rect.top + border_box.GetHeight();

						if (offset_parent || doc->GetHLDocProfile()->IsInStrictMode())

							/* Find the relative distance to the offset parent.  Do not set return
							   flags (getpos_info) from here, because they aren't relevant for the
							   offset properties */

							GetOffsetFromAncestor(left_offset, top_offset, offset_parent, GETPOS_IGNORE_SCROLL | GETPOS_OFFSET_PARENT); // FIXME move
					}
					else
					{
						if (type == BOUNDING_BOX || type == ENCLOSING_BOX)
						{
							AbsoluteBoundingBox bounding_box;

							((VerticalBox*)this)->GetBoundingBox(bounding_box, TRUE, TRUE);

							bounding_box.GetBoundingRect(rect);
						}
						else
						{
							// BORDER_BOX, PADDING_BOX or CONTENT_BOX

							rect.left = 0;
							rect.top = 0;
							rect.right = GetWidth();
							rect.bottom = GetHeight();

							if (type != BORDER_BOX)
							{
								if (!LayoutProperties::CreateCascade(element, props_list, doc->GetHLDocProfile()))
									return FALSE; // OOM

								LayoutProperties* layout_props = (LayoutProperties*)props_list.Last();
								const HTMLayoutProperties& props = *layout_props->GetProps();

								rect.left += props.border.left.width;
								rect.top += props.border.top.width;
								rect.right -= props.border.right.width;
								rect.bottom -= props.border.bottom.width;

								if (type == CONTENT_BOX)
								{
									rect.left += props.padding_left;
									rect.top += props.padding_top;
									rect.right -= props.padding_right;
									rect.bottom -= props.padding_bottom;
								}
							}
						}

						getpos_info = GetOffsetFromAncestor(left_offset, top_offset, NULL, Box::GETPOS_ABORT_ON_INLINE); // FIXME move

						if (getpos_info & (GETPOS_INLINE_FOUND
#ifdef CSS_TRANSFORMS
										   | GETPOS_TRANSFORM_FOUND
#endif
								))
						{
							left_offset = LayoutCoord(0);
							top_offset = LayoutCoord(0);

							traverse_root = GetContainingElement(element, IsAbsolutePositionedBox(), IsFixedPositionedBox(TRUE));
							box_found = FALSE;
							props_list.Clear();
						}
					}
				}

				if (!box_found)
				{
					LayoutProperties* layout_props = NULL;
					BOOL needs_target_ancestor = FALSE;

					/* Fallback to traverse with BoxEdgesObject to find the rect. */

					if (type == OFFSET_BOX)
					{
						/* For the offset box, the traverse root element is normally the
						   offsetParent.

						   There is a catch, however. If the offsetParent is an inline element,
						   BoxEdgesObject has a special mode for calculating both the offset to the
						   inline offsetParent and to the element, so that the offset can be
						   calculated relative to the inline offsetParent in the final result.

						   Also, if the elements are inside a multi-column container, we need to
						   start traversing at the outermost multi-column container. */

						traverse_root = offset_parent;
						layout_props = offset_cascade ? offset_cascade->Pred() : (LayoutProperties *)props_list.First();

						if (traverse_root)
						{
							OP_ASSERT(traverse_root->GetLayoutBox());

							if (traverse_root->GetLayoutBox()->TraverseInLine())
								needs_target_ancestor = TRUE;
							else
								needs_target_ancestor = traverse_root->GetLayoutBox()->IsColumnizable();
						}
					}

					if (type != OFFSET_BOX || needs_target_ancestor)
					{
						/* Find a suitable traverse root for BoxEdgesObject. This element must
						   be closer to root than any inline or transformed elements.  We need
						   to be able to use GetOffsetFromAncestor to compute the offset from
						   the traverse_root up to the document root. */

						if (!traverse_root)
							traverse_root = GetContainingElement(element);

						OP_ASSERT(traverse_root);

						while (TRUE)
						{
							for (; traverse_root; traverse_root = GetContainingElement(traverse_root))
							{
								Box* b = traverse_root->GetLayoutBox();

								if (b->IsBlockBox())
								{
									if (!b->TraverseInLine() && !((BlockBox*) b)->IsInMultiPaneContainer())
										break;
								}
								else
									if (type == OFFSET_BOX && !is_positioned && b->IsTableCell())
										break;
							}

							HTML_Element* candidate = traverse_root;

							while (traverse_root && !traverse_root->GetLayoutBox()->IsInlineBox()
#ifdef CSS_TRANSFORMS
								   && !traverse_root->GetLayoutBox()->HasTransformContext()
#endif
								)
								traverse_root = GetContainingElement(traverse_root);

							if (traverse_root)
								traverse_root = GetContainingElement(traverse_root);
							else
							{
								traverse_root = candidate;
								break;
							}
						}

						if (!traverse_root)
							return TRUE;

						if (IsAbsolutePositionedBox() && !IsFixedPositionedBox())
						{
							// We cannot partially traverse a tree for getting the position
							// of an absolute positioned element whose container is a PositionedInlineBox
							// because a PositionedInlineBox relies on the translation in the traversal
							// object to relative to the document root when finding the offset. In that case,
							// start traversing from the root element instead.
							HTML_Element* elm = element;
							while (elm && elm != traverse_root)
							{
								Box* box = elm->GetLayoutBox();
								if (box->IsInlineBox() && box->IsPositionedBox())
								{
									traverse_root = doc->GetLogicalDocument()->GetRoot();
									break;
								}
								elm = elm->Parent();
							}
						}

						layout_props = (LayoutProperties*)props_list.Last();

						if (layout_props)
						{
							// Find the cascade entry for traverse_root's parent

							OP_ASSERT(!layout_props->Pred() && !traverse_root->Parent() || layout_props->html_element);

							if (layout_props->html_element)
								while (layout_props->html_element != traverse_root->Parent())
									layout_props = layout_props->Pred();
						}
						else
							// Create a cascade all the way down to traverse_root's parent

							if (traverse_root->Parent())
							{
								if (!LayoutProperties::CreateCascade(traverse_root->Parent(), props_list, doc->GetHLDocProfile()))
									return FALSE; // OOM

								layout_props = (LayoutProperties*)props_list.Last();
							}
							else
							{
								layout_props = new LayoutProperties();

								if (!layout_props)
									return FALSE; // OOM

								layout_props->Into(&props_list);
								layout_props->GetProps()->Reset(NULL, doc->GetHLDocProfile());
							}
					}

					if (!traverse_root)
						traverse_root = doc->GetLogicalDocument()->GetRoot();

					Box* traverse_root_box = traverse_root->GetLayoutBox();

					BoxEdgesObject::AreaType area_type;

					switch (type)
					{
					default:
						area_type = BoxEdgesObject::BOUNDING_AREA;
						break;

					case CONTENT_BOX:
						area_type = BoxEdgesObject::CONTENT_AREA;
						break;

					case OFFSET_BOX:
						area_type = BoxEdgesObject::OFFSET_AREA;
						break;

					case BORDER_BOX:
						area_type = BoxEdgesObject::BORDER_AREA;
						break;

					case PADDING_BOX:
						area_type = BoxEdgesObject::PADDING_AREA;
						break;

					case ENCLOSING_BOX:
						area_type = BoxEdgesObject::ENCLOSING_AREA;
						break;
					}

					BoxEdgesObject box_edges(doc, element, traverse_root, area_type, element_text_start, element_text_end);
					box_edges.SetList(rect_list);

					if (needs_target_ancestor)
						box_edges.SetTargetAncestor(offset_parent);

					// Push visual device state before we Reset and traverse
					VDState old_vd_state = visual_device->PushState();
					visual_device->Reset();

					LayoutWorkplace *layout_workplace = doc->GetLogicalDocument()->GetLayoutWorkplace();

					box_edges.SetFixedPos(layout_workplace->GetLayoutViewX(),
										  layout_workplace->GetLayoutViewY());

					if (traverse_root_box->IsBlockBox())
					{
						BlockBox* block_box = (BlockBox*) traverse_root_box;
						box_edges.Translate(-block_box->GetX(), -block_box->GetY());
						block_box->Traverse(&box_edges, layout_props, block_box->GetContainingElement());
					}
					else
						if (traverse_root_box->IsTableBox())
						{
							LayoutProperties* table_lprops = layout_props;
							TableContent* table = NULL;

							for (;; table_lprops = table_lprops->Pred())
							{
								table = table_lprops->html_element->GetLayoutBox()->GetTableContent();

								if (table)
									break;
							}

							OP_ASSERT(table);

							if (traverse_root_box->IsTableCell())
							{
								TableCellBox* cell = (TableCellBox*) traverse_root_box;
								box_edges.Translate(-cell->GetPositionedX(), -cell->GetPositionedY());
								cell->TraverseCell(&box_edges, table_lprops, cell->GetTableRowBox());
							}
							else
								if (traverse_root_box->IsTableRow())
								{
									TableRowBox* row = (TableRowBox*) traverse_root_box;
									box_edges.Translate(-row->GetPositionedX(), -row->GetPositionedY());
									row->TraverseRow(&box_edges, table_lprops, table);
								}
								else
									if (traverse_root_box->IsTableRowGroup())
									{
										TableRowGroupBox* row_group = (TableRowGroupBox*) traverse_root_box;
										box_edges.Translate(-row_group->GetPositionedX(), -row_group->GetPositionedY());
										row_group->Traverse(&box_edges, table_lprops, table);
									}
									else
										if (traverse_root_box->IsTableCaption())
										{
											TableCaptionBox* caption = (TableCaptionBox*) traverse_root_box;
											box_edges.Translate(-caption->GetPositionedX(), -caption->GetPositionedY());
											caption->Traverse(&box_edges, table_lprops, table);
										}
						}
						else
							OP_ASSERT(!"Unexpected box type");

					// Restore visualdevices state
					visual_device->PopState(old_vd_state);

					box_found = box_edges.GetBoxFound();

					if (box_found)
					{
						rect = box_edges.GetBoxEdges();
						ctm = box_edges.GetBoxCTM();

						if (needs_target_ancestor)
							TranslateRects(rect, rect_list, - box_edges.GetTargetAncestorX(), - box_edges.GetTargetAncestorY());

						if (traverse_root_box->IsAbsolutePositionedBox())
						{
							if (((AbsolutePositionedBox*) traverse_root_box)->GetHorizontalOffset() != NO_HOFFSET)
								left_offset -= ((BlockBox*)traverse_root_box)->GetX();

							if (((AbsolutePositionedBox*) traverse_root_box)->GetVerticalOffset() != NO_VOFFSET)
								top_offset -= ((BlockBox*)traverse_root_box)->GetY();
						}

						if (type != OFFSET_BOX)
						{
							if (traverse_root_box->IsFixedPositionedBox())
							{
								// scroll position added by TraverseBox
								if (((AbsolutePositionedBox*) traverse_root_box)->GetVerticalOffset() != NO_VOFFSET)
									top_offset -= LayoutCoord(doc->GetLayoutViewY());
								if (((AbsolutePositionedBox*) traverse_root_box)->GetHorizontalOffset() != NO_HOFFSET)
									left_offset -= LayoutCoord(doc->GetLayoutViewX());
							}

							getpos_info = traverse_root_box->GetOffsetFromAncestor(left_offset, top_offset);

#ifdef CSS_TRANSFORMS
							if (ctm.IsTransform())
							{
								ctm.PrependTranslation(left_offset, top_offset);
								left_offset = LayoutCoord(0);
								top_offset = LayoutCoord(0);
							}
#endif

						}
					}
				}

				props_list.Clear();

				if (box_found)
				{
					if (type == OFFSET_BOX && element->IsMatchingType(Markup::HTE_BODY, NS_HTML) || is_scroll_box)
					{
						/* The offset is added to the offsetLeft/offsetTop
						   values of the element that has BODY as
						   offsetParent, so don't add them now. Also,
						   scrollLeft and scrollTop on inline boxes should
						   always be 0. */

						rect.right -= rect.left;
						rect.bottom -= rect.top;
						rect.left = 0;
						rect.top = 0;
					}
					else
					{
						/* For offset properties, we should not care
						   about what GetOffsetFromAncestor returns. */

						OP_ASSERT(getpos_info == 0 || type != OFFSET_BOX);

						if (getpos_info & GETPOS_FIXED_FOUND
#ifdef CSS_TRANSFORMS
							&& !(getpos_info & GETPOS_TRANSFORM_FOUND)
#endif
							)
						{
							top_offset += LayoutCoord(doc->GetLayoutViewY());
							left_offset += LayoutCoord(doc->GetLayoutViewX());
						}

						TranslateRects(rect, rect_list, left_offset, top_offset);
					}
				}

				return box_found;
			}

		case CLIENT_BOX:
			{
				/* Get clientHeight and clientWidth values. We define and implement it as follows:

				   The clientWidth and clientHeight of element A are the width and height of the
				   padding box, excluding any horizontal or vertical scrollbar generated by the
				   element.

				   In strict mode, clientWidth and clientHeight of the root element (HTML) are the
				   layout viewport width and height.

				   In quirks mode, clientWidth and clientHeight of the BODY element are the width
				   and height of the layout viewport.

				   What clientWidth and clientHeight are when A is an inline box is not defined.

				   See also the latest draft of the CSSOM View specification. */

				Head props_list;

				if (!LayoutProperties::CreateCascade(element, props_list, doc->GetHLDocProfile()))
					return FALSE; // OOM

				LayoutProperties* layout_props = (LayoutProperties*)props_list.Last();
				const HTMLayoutProperties& props = *layout_props->GetProps();
				BOOL strict_mode = doc->GetHLDocProfile()->IsInStrictMode();

				if (strict_mode && element->IsMatchingType(Markup::HTE_HTML, NS_HTML) ||
					!strict_mode && element->IsMatchingType(Markup::HTE_BODY, NS_HTML))
				{
					rect.right = doc->GetLayoutViewWidth();
					rect.bottom = doc->GetLayoutViewHeight();
				}
				else
				{
					rect.left = props.border.left.width;
					rect.right = GetWidth() - props.border.right.width;
					rect.top = props.border.top.width;
					rect.bottom = GetHeight() - props.border.bottom.width;

					if (ScrollableArea* sc = GetScrollable())
					{
						rect.right -= sc->GetVerticalScrollbarWidth();
						rect.bottom -= sc->GetHorizontalScrollbarWidth();
					}
				}

				props_list.Clear();

				return TRUE;
			}
		}
	}

	return FALSE;
}

/* static */ void
Box::TranslateRects(RECT &rect, RectList *rect_list, long x, long y)
{
	rect.top += y;
	rect.left += x;
	rect.right += x;
	rect.bottom += y;

	if (rect_list != NULL)
		for (RectListItem *iter = rect_list->First();
			 iter != NULL;
			 iter = iter->Suc())
		{
			RECT &r = iter->rect;
			r.top += y;
			r.left += x;
			r.right += x;
			r.bottom += y;
		}
}

BOOL
Box::CountWordsOfChildren(LineSegment& segment, int& words) const
{
	for (HTML_Element* html_element = GetHtmlElement()->FirstChild();
		 html_element;
		 html_element = html_element->Suc())
	{
		const Box* child_box = html_element->GetLayoutBox();
		if (child_box && !child_box->CountWords(segment, words))
			return FALSE;
	}

	return TRUE;
}

void
Box::AdjustVirtualPosOfChildren(LayoutCoord offset)
{
	for (HTML_Element* html_element = GetHtmlElement()->FirstChild();
		 html_element;
		 html_element = html_element->Suc())
	{
		Box* child_box = html_element->GetLayoutBox();
		if (child_box)
			child_box->AdjustVirtualPos(offset);
	}
}

/** Get the list item marker pseudo element of this box, or NULL if there is none. */

HTML_Element*
Box::GetListMarkerElement() const
{
	HTML_Element* potential_marker = GetHtmlElement()->FirstChild();
	return potential_marker && potential_marker->GetIsListMarkerPseudoElement() ? potential_marker : NULL;
}

/** Is this box (and potentially also its children) columnizable? */

/* virtual */ BOOL
Box::IsColumnizable(BOOL require_children_columnizable) const
{
	return GetContainingElement()->GetLayoutBox()->IsColumnizable(TRUE);
}

/** Get height, minimum line height and base line. */

/* virtual */ void
Box::GetVerticalAlignment(InlineVerticalAlignment* valign)
{
	*valign = InlineVerticalAlignment();
	valign->logical_above_baseline = LayoutCoord(0);
	valign->logical_below_baseline = LayoutCoord(0);
	valign->logical_above_baseline_nonpercent = LayoutCoord(0);
	valign->logical_below_baseline_nonpercent = LayoutCoord(0);
}

/** Reflow box and children. */

LAYST
Box::LayoutChildren(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	HTML_Element* html_element = cascade->html_element;
	HTML_Element* child = NULL;

	OP_ASSERT(!info.workplace->GetYieldElement());

#ifdef LAYOUT_YIELD_REFLOW

	++info.yield_count;

	/* Dont check too often... */

	if (info.workplace->CanYield() && !(info.yield_count & 0x7) && !html_element->HasBefore() && !html_element->HasAfter() && html_element->GetInserted() != HE_INSERTED_BY_LAYOUT)
	{
		/* Yield if we take longer than MS_BEFORE_YIELD ms */
		double delay_time = g_op_time_info->GetRuntimeMS() - info.workplace->GetReflowStart();

		if (delay_time > info.workplace->GetMaxReflowTime())
		{
			HTML_Element* achild = html_element->FirstChild();

			BOOL extra = FALSE;

			while (achild)
			{
				if (achild->IsExtraDirty())
				{
					extra = TRUE;
					break;
				}
				achild = achild->Suc();
			}

			info.workplace->SetYieldElement(html_element);
			info.workplace->SetYieldElementExtraDirty(extra);

			return LAYOUT_YIELD;
		}
	}
#endif // LAYOUT_YIELD_REFLOW

	OP_ASSERT(ShouldLayoutChildren());

	if (first_child &&
		(!first_child->GetLayoutBox() || !first_child->GetLayoutBox()->IsInlineRunInBox()))
		for (HTML_Element* run = first_child; run != html_element; run = run->Parent())
			child = run;

	if (!child)
		child = html_element->FirstChild();

	for (; child; child = child->Suc())
		if (child == info.stop_before)
			return LAYOUT_STOP;
		else
		{
			if (child->GetInserted() == HE_INSERTED_BY_LAYOUT && !child->IsExtraDirty() && child->GetLayoutBox() && child->GetLayoutBox()->IsAnonymousWrapper())
				/* The child is an anonymous wrapper box (table object or flex item). It is not
				   extra dirty. However, if there is no layout box between this anonymous wrapper
				   and the next sibling that is extra dirty, it may mean that the sibling is to be
				   moved inside this anonymous wrapper box. There's no way to tell for sure,
				   really, until we have generated the cascade for that sibling, but by that time,
				   it's too late to go back and reflow this wrapper. Therefore, if such a sibling
				   is found, mark the anonymous wrapper extra dirty, so that it is regenerated, to
				   possibly include this sibling (and further siblings as well). */

				for (HTML_Element* sibling = child->Suc(); sibling; sibling = sibling->Suc())
					if (sibling->IsExtraDirty() && !sibling->HasNoLayout())
					{
						/* The parent shouldn't be inserted by layout, since MarkExtraDirty()
						   inside a layout-inserted subtree will always cause all layout-inserted
						   parents to be marked extra dirty as well. Let's make sure, with an
						   assertion. Handling a situation like this should be possible, but it has
						   not been verified that we do it properly. */

						OP_ASSERT(html_element->GetInserted() != HE_INSERTED_BY_LAYOUT);

						BOOL regenerate_box = TRUE;

						for (HTML_Element* elm = child; elm && elm->GetInserted() == HE_INSERTED_BY_LAYOUT; elm = elm->LastChild())
							if (elm->GetIsPseudoElement() || elm->GetIsGeneratedContent())
							{
								/* An anonymous wrapper box generated because of a pseudo element
								   (typically a ::before element) or because of generated content
								   ('content' property on a DOM node) should not span siblings, so
								   leave it alone. */

								regenerate_box = FALSE;
								break;
							}

						if (regenerate_box)
							child->SetDirty(ELM_EXTRA_DIRTY);

						break;
					}
					else
						if (sibling->GetLayoutBox())
							break;

			BOOL extra_dirty = child->IsExtraDirty();

            if ((extra_dirty || !child->GetLayoutBox()) && !child->HasNoLayout())
            {
				if (extra_dirty)
				{
					BOOL gracetime = FALSE;
					if (Box* old_box = child->GetLayoutBox())
					{
						if (cascade->Suc())
							// Must finish previous layout boxes

							if (OpStatus::IsMemoryError(cascade->Suc()->Finish(&info)))
								return LAYOUT_OUT_OF_MEMORY;

						// Update area where box is, before deleting it

						old_box->Invalidate(cascade, info);
						gracetime = TRUE;
						imgManager->BeginGraceTime();
					}

					LayoutProperties::RemoveElementsInsertedByLayout(info, child);
					if (gracetime)
						imgManager->EndGraceTime();
				}

				/* Attempt to create a new layout box for this child, and lay it out. Recurse to children.

				   1: In some cases, this operation may cause a parent element to be inserted by
				   the layout engine (anonymous table elements), in which case we will attempt to
				   create a layout box for said inserted element instead. GOTO 1.
				   LayoutProperties::INSERTED_PARENT is returned from CreateChildLayoutBox() when
				   a parent element is inserted. */

				HTML_Element* walker_element = child;

				do
				{
					child = walker_element;

					if (extra_dirty)
						for (HTML_Element* grandchild = child->FirstChildActualStyle(); grandchild; grandchild = grandchild->SucActualStyle())
							grandchild->SetDirty(ELM_BOTH_DIRTY);

					LayoutProperties::LP_STATE status = cascade->CreateChildLayoutBox(info, child);

					switch (status)
					{
					case LayoutProperties::STOP:
						return LAYOUT_STOP;

					case LayoutProperties::OUT_OF_MEMORY:
						return LAYOUT_OUT_OF_MEMORY;

					case LayoutProperties::YIELD:
						return LAYOUT_YIELD;

					case LayoutProperties::ELEMENT_MOVED_UP:
						/* The child element and its successive siblings were moved up one level in
						   the tree (this happens when an element is found to be unsuitable as a
						   child of some layout-inserted anonymous table element), and they are
						   therefore no longer children of this element. Nothing more to do here
						   then. The parent Box's LayoutChildren() will take care of these elements
						   in due course. */

						return LAYOUT_CONTINUE;

					default:
						break;
					}

					walker_element = walker_element->Parent();
				}
				while (walker_element != html_element); // repeat as long as the layout engine inserts a parent element
            }
			else
				if (Box* child_box = child->GetLayoutBox())
					if (child_box->IsInlineRunInBox())
					{
						// Keep the run-in as inline - don't lay it out as block now.

						Container* container = cascade->html_element->GetLayoutBox()->GetContainer();

						if (!container)
							container = cascade->container;

						/* Commit pending line content now, since it would confuse the layout engine
						   (incorrectly making it convert the run-in to a block) if it were done when
						   adding the next sibling block to the container. */

						LAYST st = container->CommitLineContent(info, TRUE);

						if (st != LAYOUT_CONTINUE)
							return st;

						container->SetPendingRunIn((InlineRunInBox*) child_box, info, LayoutCoord(0));
					}
					else
					{
						LayoutProperties* child_cascade = cascade->GetChildCascade(info, child);

						if (!child_cascade)
							return LAYOUT_OUT_OF_MEMORY;

#ifdef DEBUG
						const HTMLayoutProperties& props = *child_cascade->GetProps();
						if (props.display_type == CSS_VALUE_list_item && props.HasListItemMarker())
						{
							HTML_Element* list_marker = child->FirstChild();
							/* If this triggers, it means something has removed
							   a list marker pseudo element without marking parent
							   list item element extra dirty. That is illegal. */
							OP_ASSERT(list_marker && list_marker->GetIsListMarkerPseudoElement());
						}
#endif // DEBUG

						if (child->NeedsUpdate())
							child_box->Invalidate(cascade, info);

						LAYST state = child_box->Layout(child_cascade, info, first_child, start_position);

						if (state != LAYOUT_CONTINUE)
							return state;
					}

			first_child = NULL;
			start_position = LAYOUT_COORD_MIN;
		}

	return LAYOUT_CONTINUE;
}

/** Inline-traverse children of this box. */

BOOL
Box::LineTraverseChildren(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline)
{
	HTML_Element* html_element = GetHtmlElement();
	HTML_Element* element = NULL;
	BOOL continue_traverse = TRUE;

	if (segment.start_element)
		for (HTML_Element* run = segment.start_element; run != html_element; run = run->Parent())
		{
			element = run;

			if (!run)
				break;
		}

	if (!element)
	{
		element = html_element->FirstChild();
		segment.start_element = NULL;
	}

	for (; element && continue_traverse; element = element->Suc())
	{
		if (Box* box = element->GetLayoutBox())
			continue_traverse = box->LineTraverseBox(traversal_object, parent_lprops, segment, baseline);

		segment.start_element = NULL;
	}

	return continue_traverse;
}

/* virtual */ BOOL
Box::SkipZElement(LayoutInfo& info)
{
	StackingContext* stacking_context = GetLocalStackingContext();
	if (stacking_context && !IsPositionedBox())
	{
		/* We form a new stacking context, but not a containing block for
		   absolute positioned elements. We need to continue to skip through
		   child stacking contexts to find absolute positioned elements which
		   may be affected by a change in the nearest containing block for
		   absolute positioned elements. */
		stacking_context->Restart();
		return stacking_context->SkipElement(GetHtmlElement(), info);
	}
	else
		return TRUE;
}

/** Get used border widths, including collapsing table borders */

/* virtual */ void
Box::GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const
{
	top = props.border.top.width;
	left = props.border.left.width;
	right = props.border.right.width;
	bottom = props.border.bottom.width;
}

/** Skip words because they belong to a different traversal pass. Needed
	when text-align is justify. */

void
Box::SkipJustifiedWords(LineSegment& segment)
{
	OP_ASSERT(segment.justify);

	int elm_words = 0;
	int segment_words = segment.line->GetNumberOfWords();
	LayoutCoord justified_space_total = segment.line->GetWidth() - segment.line->GetUsedSpace();

	CountWords(segment, elm_words);

	segment.justified_space_extra_accumulated +=
		LayoutCoord(justified_space_total * (segment.word_number + elm_words - 1) / (segment_words - 1) -
					justified_space_total * (segment.word_number - 1) / (segment_words - 1));

	segment.word_number += elm_words;
}

/** Should overflow clipping of this box affect clipping of the target box? */

BOOL
Box::GetClipAffectsTarget(HTML_Element* target_element, BOOL pane_clip)
{
	if (!target_element)
		return TRUE;

	Box* target_box = target_element->GetLayoutBox();

	OP_ASSERT(GetHtmlElement()->IsAncestorOf(target_element));

	if (target_box->IsFixedPositionedBox())
		/* Fixed positioned boxes are not affected by clipping, unless this is
		   the root. We then want to make sure that nothing is drawn on top of
		   the page control widget. */

		return GetHtmlElement()->Type() == Markup::HTE_DOC_ROOT && !pane_clip;

	HTML_Element* this_element = GetHtmlElement();

	BOOL last_was_absolute = FALSE;

	HTML_Element* iter = target_element;

	while (iter && (iter != this_element))
	{
		Box* desc_box = iter->GetLayoutBox();

		if (desc_box->IsPositionedBox())
		{
			last_was_absolute = desc_box->IsAbsolutePositionedBox();
			if (last_was_absolute && desc_box->IsFixedPositionedBox())
				return FALSE;
		}

		iter = iter->Parent();
	}

	return !pane_clip && IsPositionedBox() || !last_was_absolute;
}

OpRect
Box::GetPaddingEdges(const HTMLayoutProperties &props) const
{
	OpRect r = GetBorderEdges();
	r.OffsetBy(props.border.left.width, props.border.top.width);
	r.width -= props.border.left.width + props.border.right.width;
	r.height -= props.border.top.width + props.border.bottom.width;
	return r;
}

/** Lay out box. */

/* virtual */ LAYST
Contentless_Box::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
		return LAYOUT_OUT_OF_MEMORY;

	// Remove dirty mark

	cascade->html_element->MarkClean();

	if (first_child && first_child != cascade->html_element)
		return LayoutChildren(cascade, info, first_child, start_position);
	else
		return LayoutChildren(cascade, info, NULL, LayoutCoord(0));
}

/** Is some part of this content selected */

BOOL
Box::GetSelected() const
{
	return GetHtmlElement()->IsInSelection();
}

/** Set that some part of this content is selected */

void
Box::SetSelected(BOOL value)
{
	GetHtmlElement()->SetIsInSelection(value);
}

SpaceManagerReflowState::~SpaceManagerReflowState()
{
	while (FLink* flink = First())
	{
		flink->Out();
		flink->float_box->DeleteFloatReflowCache();
	}
}

/** Destructor. */

SpaceManager::~SpaceManager()
{
	while (FLink* flink = First())
	{
		flink->Out();
		flink->float_box->DeleteFloatReflowCache();
	}

	OP_DELETE(reflow_state);
}

/** Restart space manager - prepare it for reflow. */

BOOL
SpaceManager::Restart()
{
	delete reflow_state;
	reflow_state = new SpaceManagerReflowState;

	if (!reflow_state)
		return FALSE;

	/* Move all floats into the pending list.
	   Later they will be either deleted or put back when reflowed or skipped. */

	for (FLink* flink = First(); flink; flink = First())
	{
		flink->Out();
		flink->Into(reflow_state);

		if (!flink->float_box->InitialiseFloatReflowCache())
			return FALSE;
	}

	if (spaceman_packed.full_bfc_reflow)
		reflow_state->full_bfc_reflow = reflow_state->full_bfc_min_max_calculation = TRUE;

	return TRUE;
}

/** Reflow of the box associated with this space manager is finished. */

void
SpaceManager::FinishLayout()
{
	/* No reflow_state means that reflow of the owning box was skipped.
	   If there is a reflow_state, verify that all floats have been moved back into the space manager. */

	OP_ASSERT(!reflow_state || reflow_state->Empty());

	for (FLink* flink = First(); flink; flink = flink->Suc())
		flink->float_box->DeleteFloatReflowCache();

	spaceman_packed.full_bfc_reflow = 0;

	OP_DELETE(reflow_state);
	reflow_state = NULL;
}

/** Skip over an element. */

void
SpaceManager::SkipElement(LayoutInfo& info, HTML_Element* element)
{
	FLink* flink = reflow_state->First();

	// Move pending floats that are children of element back into the float list

	while (flink)
	{
		FLink* next = flink->Suc();
		HTML_Element* html_element = flink->float_box->GetHtmlElement();

		if (element->IsAncestorOf(html_element))
		{
			FloatingBox* float_box = flink->float_box;

			AddFloat(float_box);
			float_box->SkipLayout(info);
		}
		else
			break;

		flink = next;
	}
}

/** Propagate bottom margins of child floats of element. */

void
SpaceManager::PropagateBottomMargins(LayoutInfo& info, HTML_Element* element, LayoutCoord y_offset, LayoutCoord min_y_offset)
{
	FLink* flink = Last();

	while (flink)
	{
		FLink* pred = flink->Pred();
		HTML_Element* html_element = flink->float_box->GetHtmlElement();

		if (element->IsAncestorOf(html_element))
		{
			FloatingBox* float_box = flink->float_box;

			float_box->PropagateBottomMargins(info);

			if (y_offset)
				float_box->SetBfcY(float_box->GetBfcY() + y_offset);

			if (min_y_offset)
				float_box->SetBfcMinY(float_box->GetBfcMinY() + min_y_offset);
		}
		else
			break;

		flink = pred;
	}
}

/** Add floating block box to space manager. */

void
SpaceManager::AddFloat(FloatingBox* floating_box)
{
	if (floating_box->link.InList())
		floating_box->link.Out();

	floating_box->link.Into(this);
	floating_box->InvalidateFloatReflowCache();
}

/** Get new space from space manager. */

LayoutCoord
SpaceManager::GetSpace(LayoutCoord& bfc_y, LayoutCoord& bfc_x, LayoutCoord& width, LayoutCoord min_width, LayoutCoord min_height, VerticalLock vertical_lock) const
{
	FLink* flink = Last();

	if (flink)
		flink->float_box->UpdateFloatReflowCache();

	LayoutCoord left;
	LayoutCoord right;
	LayoutCoord left_height;
	LayoutCoord right_height;
	LayoutCoord last_bfc_y;

	do
	{
		left = bfc_x;
		right = left + width;
		LayoutCoord left_float_bottom = bfc_y;
		LayoutCoord right_float_bottom = bfc_y;
		LayoutCoord next_edge = LAYOUT_COORD_MAX;

		left_height = LAYOUT_COORD_MAX;
		right_height = LAYOUT_COORD_MAX;
		flink = Last();
		last_bfc_y = bfc_y;

		while (flink)
		{
			FloatingBox* floating_box = flink->float_box;

			LayoutCoord float_bfc_x = floating_box->GetBfcX();
			LayoutCoord float_bfc_y = floating_box->GetBfcY() - floating_box->GetMarginTop();
			LayoutCoord float_height_with_margins = LayoutCoord(floating_box->GetHeightAndMargins());

			// Don't bother if float is above y

			if (bfc_y < float_bfc_y + float_height_with_margins)
			{
				// find closest edge

				if (bfc_y < float_bfc_y && next_edge > float_bfc_y)
					next_edge = float_bfc_y;

				if (floating_box->IsLeftFloat())
				{
					float_bfc_x += floating_box->GetMarginToEdge();

					// Is float within the x range we are looking at?

					if (left < float_bfc_x)
						// Are we above the top of the float?

						if (min_height == LAYOUT_COORD_MAX || bfc_y + min_height >= float_bfc_y)
						{
							// Not enough room above the float, but we may fit beside it.

							if (bfc_y < float_bfc_y)
							{
								// y is above float

								if (left_height > float_bfc_y)
									if (right - float_bfc_x >= min_width || vertical_lock != VLOCK_OFF)
									{
										// There's enough horizontal space for us (or we don't care)

										if (vertical_lock != VLOCK_KEEP_LEFT_FIXED)
											left = float_bfc_x;
									}
									else
									{
										// Not enough horizontal space for us, see if we are too tall

										left_height = float_bfc_y;

										if (left_height - bfc_y < min_height)
										{
											// Space has been exhausted, keep searching further down

											bfc_y = next_edge;
											break;
										}
									}
							}
							else
							{
								// y is affected by float

								if (vertical_lock != VLOCK_KEEP_LEFT_FIXED)
									left = float_bfc_x;

								left_float_bottom = float_bfc_y + float_height_with_margins;

								if (right - left < min_width && vertical_lock == VLOCK_OFF)
								{
									LayoutCoord float_bottom;

									if (right_float_bottom != bfc_y && left_float_bottom > right_float_bottom)
										float_bottom = right_float_bottom;
									else
										float_bottom = left_float_bottom;

									// Prevent infinite loop

									if (bfc_y < float_bottom)
									{
										// Space has been exhausted, try searching further down

										bfc_y = float_bottom;
										break;
									}
								}
							}
						}
						else
							// There's enough room above the float.

							if (left_height > float_bfc_y)
								left_height = float_bfc_y;
				}
				else
				{
					float_bfc_x -= floating_box->GetMarginToEdge();

					// Is float within the x range we are looking at?

					if (right > float_bfc_x)
						// Are we above the top of the float?

						if (min_height == LAYOUT_COORD_MAX || bfc_y + min_height >= float_bfc_y)
						{
							// Not enough room above the float, but we may fit beside it.

							if (bfc_y < float_bfc_y)
							{
								// y is above float

								if (right_height > float_bfc_y)
									if (float_bfc_x - left >= min_width || vertical_lock != VLOCK_OFF)
									{
										// There's enough space for us (or we don't care)

										if (vertical_lock != VLOCK_KEEP_RIGHT_FIXED)
											right = float_bfc_x;
									}
									else
									{
										// Not enough horizontal space for us, see if we are too tall

										right_height = float_bfc_y;

										if (right_height - bfc_y < min_height)
										{
											// Space has been exhausted, keep searching further down

											bfc_y = next_edge;
											break;
										}
									}
							}
							else
							{
								// y is affected by float

								right_float_bottom = float_bfc_y + float_height_with_margins;

								if (vertical_lock != VLOCK_KEEP_RIGHT_FIXED)
									right = float_bfc_x;

								if (right - left < min_width && vertical_lock == VLOCK_OFF)
								{
									LayoutCoord float_bottom;

									if (left_float_bottom != bfc_y && right_float_bottom > left_float_bottom)
										float_bottom = left_float_bottom;
									else
										float_bottom = right_float_bottom;

									// Prevent infinite loop

									if (bfc_y < float_bottom)
									{
										// Space has been exhausted, keep searching further down

										bfc_y = float_bottom;
										break;
									}
								}
							}
						}
						else
							// There's enough room above the float.

							if (right_height > float_bfc_y)
								right_height = float_bfc_y;
				}
			}
			else
			{
				// Check if any of the preceding floats can affect us. If they cannot, we're done.

				LayoutCoord bottom = floating_box->GetLowestFloatBfcBottom(CSS_VALUE_both);

				if (bfc_y >= bottom)
					break;
			}

			flink = flink->Pred();
		}
	}
	while (last_bfc_y < bfc_y); // Repeat as long as we want to keep searching further down

	width = right - left;
	bfc_x = left;

	LayoutCoord max_height = MIN(left_height, right_height);

	if (max_height != LAYOUT_COORD_MAX)
		max_height -= bfc_y;

	return max_height;
}

/** Get maximum width of all floats at the specified minimum Y position, and change minimum Y if necessary. */

void
SpaceManager::GetFloatsMaxWidth(LayoutCoord& bfc_min_y, LayoutCoord min_height, LayoutCoord max_width, LayoutCoord bfc_left_edge_min_distance, LayoutCoord bfc_right_edge_min_distance, LayoutCoord container_width, LayoutCoord& left_floats_max_width, LayoutCoord& right_floats_max_width)
{
	LayoutCoord last_bfc_min_y;

	do
	{
		LayoutCoord left_min_bottom = LAYOUT_COORD_MAX;
		LayoutCoord right_min_bottom = LAYOUT_COORD_MAX;
		FLink* flink = Last();

		if (flink)
			flink->float_box->UpdateFloatReflowCache();

		last_bfc_min_y = bfc_min_y;
		left_floats_max_width = LayoutCoord(0);
		right_floats_max_width = LayoutCoord(0);

		while (flink)
		{
			FloatingBox* floating_box = flink->float_box;

			LayoutCoord float_bfc_min_y = floating_box->GetBfcMinY();
			LayoutCoord float_min_height_with_margins = floating_box->GetMinHeightAndMargins();

			float_bfc_min_y -= floating_box->GetMarginTop(TRUE);

			if (bfc_min_y + min_height >= float_bfc_min_y && bfc_min_y < float_bfc_min_y + float_min_height_with_margins)
			{
				// We are affected by this float.

				LayoutCoord acc_max_width = floating_box->GetAccumulatedMaxWidth();

				if (floating_box->IsLeftFloat())
				{
					if (acc_max_width > bfc_left_edge_min_distance)
					{
						/* The total maximum width of left floats is larger than the
						   minimal distance to the left edge of the BFC. */

						acc_max_width -= bfc_left_edge_min_distance;

						if (left_floats_max_width < acc_max_width)
							left_floats_max_width = acc_max_width;
					}

					left_min_bottom = float_bfc_min_y + float_min_height_with_margins;
				}
				else
				{
					if (acc_max_width > bfc_right_edge_min_distance)
					{
						/* The total maximum width of right floats is larger than the
						   minimal distance to the right edge of the BFC. */

						acc_max_width -= bfc_right_edge_min_distance;

						if (right_floats_max_width < acc_max_width)
							right_floats_max_width = acc_max_width;
					}

					right_min_bottom = float_bfc_min_y + float_min_height_with_margins;
				}

				if (container_width != LAYOUT_COORD_MAX)
					if (container_width - left_floats_max_width - right_floats_max_width < max_width)
					{
						// Not enough space. Try further down.

						bfc_min_y = MIN(left_min_bottom, right_min_bottom);
						break;
					}
			}
			else
			{
				/* Not affected by this float. Check if any of the preceding floats can affect us.
				   If they cannot, we're done. */

				LayoutCoord min_bfc_bottom = floating_box->GetLowestFloatBfcMinBottom(CSS_VALUE_both);

				if (bfc_min_y >= min_bfc_bottom)
					// All preceding floats are above (minimum-y-wise) the area we're looking at.

					break;
			}

			LayoutCoord max_left = floating_box->GetHighestAccumulatedMaxWidth(CSS_VALUE_left);
			LayoutCoord max_right = floating_box->GetHighestAccumulatedMaxWidth(CSS_VALUE_right);

			if (max_left <= left_floats_max_width && max_right <= right_floats_max_width)
				/* None of the preceding floats have higher accumulated maximum widths than
				   what we have already found. */

				break;

			flink = flink->Pred();
		}
	}
	while (last_bfc_min_y < bfc_min_y);
}

/** Find bottom of floats. */

LayoutCoord
SpaceManager::FindBfcBottom(CSSValue clear) const
{
	LayoutCoord bottom_edge = LAYOUT_COORD_MIN;
	FLink* flink = Last();

	if (flink)
	{
		FloatingBox* floating_box = flink->float_box;

		floating_box->UpdateFloatReflowCache();
		bottom_edge = floating_box->GetLowestFloatBfcBottom(clear);
	}

	return bottom_edge;
}

/** Find minimum bottom of floats. */

LayoutCoord
SpaceManager::FindBfcMinBottom(CSSValue clear) const
{
	LayoutCoord min_bottom_edge = LAYOUT_COORD_MIN;
	FLink* flink = Last();

	if (flink)
	{
		FloatingBox* floating_box = flink->float_box;

		floating_box->UpdateFloatReflowCache();
		min_bottom_edge = floating_box->GetLowestFloatBfcMinBottom(clear);
	}

	return min_bottom_edge;
}

/** Get the last float in this block formatting context. */

FloatingBox*
SpaceManager::GetLastFloat() const
{
	return Last() ? Last()->float_box : NULL;
}

/** Lay out box. */

/* virtual */ LAYST
Box::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	OP_ASSERT(!first_child || first_child == cascade->html_element);

	// Set blink

	if (props.text_decoration & TEXT_DECORATION_BLINK)
		for (FramesDocument* d = info.doc; d; d = d->GetDocManager()->GetParentDoc())
			d->SetBlink(TRUE);

	return LAYOUT_CONTINUE;
}

/** Invalidate fixed descendants of this Box. */

void
Box::InvalidateFixedDescendants(FramesDocument* doc)
{
	/* If the root stacking context is empty, there are no fixed elements. */
	if (doc->GetDocRoot()->GetLayoutBox()->GetLocalStackingContext()->IsEmpty())
		return;

	HTML_Element* ancestor = GetHtmlElement();
	StackingContext* context = NULL;

	/* Find closest stacking context. */
	for (HTML_Element* elm = ancestor; elm && !(context = elm->GetLayoutBox()->GetLocalStackingContext()); elm = elm->Parent())
		;

	OP_ASSERT(context);

	context->InvalidateFixedDescendants(doc, ancestor);
}

/** Return the TableContent for the closest table ascendant for this Box. */

TableContent*
Box::GetTable() const
{
	for (HTML_Element* parent = GetContainingElement(); parent; parent = GetContainingElement(parent))
	{
		TableContent* table = parent->GetLayoutBox()->GetTableContent();

		if (table)
			return table;
	}

	return NULL;
}

/* static */ HTML_Element*
Box::GetContainingElement(HTML_Element* element, BOOL absolutely_positioned, BOOL fixed)
{
	OP_PROBE4(OP_PROBE_BOX_GETCONTAININGELEMENT);

	OP_ASSERT(!fixed || absolutely_positioned);

	for (HTML_Element* parent = element->Parent(); parent; parent = parent->Parent())
	{
		Box* box = parent->GetLayoutBox();

		if (box)
		{
			if (box->IsContentReplaced())
			{
#ifdef SVG_SUPPORT
				if (parent->IsMatchingType(Markup::SVGE_SVG, NS_SVG) &&
					element->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG))
					/* Returning NULL because foreignObjects inside SVGContent
					   act as root elements and do not have containing elements. */
					return NULL;
				else
#endif // SVG_SUPPORT
					return parent;
			}

			if (fixed)
			{
#ifdef CSS_TRANSFORMS
				if (box->HasTransformContext())
					return parent;
#endif
				if (!parent->Parent())
					return parent;
			}
			else
				if (absolutely_positioned)
				{
					if (box->IsPositionedBox())
						return parent;
				}
				else
					if (box->IsContainingElement() || box->IsTableBox())
						return parent;
		}
	}

	return NULL;
}

/** Initialise reflow state. */

InlineBoxReflowState*
InlineBox::InitialiseReflowState()
{
	if (!(un.data & 1))
	{
		InlineBoxReflowState* state = new InlineBoxReflowState;

		if (state)
		{
			state->html_element = un.html_element;

#ifdef CSS_TRANSFORMS
			if (TransformContext *transform_context = GetTransformContext())
				if (!transform_context->InitialiseReflowState(state))
				{
					OP_DELETE(state);
					return NULL;
				}
#endif

			un.data = ((UINTPTR)state) | 1;
		}

		return state;
	}
	else
		return (InlineBoxReflowState*) (un.data & ~1);
}

/** Compute baseline. */

void
InlineBox::ComputeBaseline(LayoutInfo& info)
{
	InlineBoxReflowState* state = GetReflowState();
	LayoutProperties* cascade = state->cascade;
	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord baseline_offset(0);
	LayoutCoord ascent(0);
	LayoutCoord descent(0);
	LayoutCoord half_leading(0);
	LayoutCoord baseline_offset_nonpercent(0);
	LayoutCoord ascent_nonpercent(0);
	LayoutCoord descent_nonpercent(0);
	LayoutCoord half_leading_nonpercent(0);

	if (content->IsInlineContent())
	{
		state->valign.minimum_line_height = props.GetCalculatedLineHeight(info.visual_device);
		state->valign.minimum_line_height_nonpercent = state->valign.minimum_line_height;

		ascent = LayoutCoord(props.font_ascent);
		ascent_nonpercent = ascent;
		descent = LayoutCoord(props.font_descent);
		descent_nonpercent = descent;

		state->valign.box_above_baseline = ascent + props.padding_top + LayoutCoord(props.border.top.width);
		state->valign.box_below_baseline = descent + props.padding_bottom + LayoutCoord(props.border.bottom.width);
		half_leading = HTMLayoutProperties::GetHalfLeading(state->valign.minimum_line_height, ascent, descent);
		half_leading_nonpercent = HTMLayoutProperties::GetHalfLeading(state->valign.minimum_line_height_nonpercent, ascent_nonpercent, descent_nonpercent);
	}
	else
	{
		/* Replaced elements don't have a baseline. Inline-tables and
		   inline-blocks do. Margin, padding and border will be added to line
		   height. Padding and border are included in content height in our
		   implementation. */

		state->valign.minimum_line_height = content->GetHeight() + props.margin_top + props.margin_bottom;

		LayoutCoord min_line_height_nonpercent = content->GetMinHeight();

		if (!props.GetMarginTopIsPercent())
			min_line_height_nonpercent += props.margin_top;
		if (!props.GetMarginBottomIsPercent())
			min_line_height_nonpercent += props.margin_bottom;

		state->valign.minimum_line_height_nonpercent = min_line_height_nonpercent;

		if (content->IsReplaced() && !content->IsReplacedWithBaseline())
		{
			ascent = state->valign.minimum_line_height;
			ascent_nonpercent = state->valign.minimum_line_height_nonpercent;
		}
		else
		{
			// inline-table, inline-block or replaced content that has baseline.

			ascent = content->GetBaseline(props) + props.margin_top;
			descent = state->valign.minimum_line_height - ascent;

			ascent_nonpercent = content->GetMinBaseline(props);
			if (!props.GetMarginTopIsPercent())
				ascent_nonpercent += props.margin_top;
			descent_nonpercent = state->valign.minimum_line_height_nonpercent - ascent_nonpercent;
		}

		state->valign.box_above_baseline = ascent - props.margin_top;
		state->valign.box_below_baseline = descent - props.margin_bottom;
	}

	switch (props.vertical_align_type)
	{
	case CSS_VALUE_top:
	case CSS_VALUE_bottom:
		state->valign.loose_root = TRUE;
		state->valign.loose = TRUE;
		state->valign.top_aligned = (props.vertical_align_type == CSS_VALUE_top);
		break;

	default:
		baseline_offset += GetBaselineOffset(props, ascent + half_leading, state->valign.minimum_line_height - (ascent + half_leading));
		baseline_offset_nonpercent += GetBaselineOffset(props, ascent_nonpercent + half_leading_nonpercent, state->valign.minimum_line_height_nonpercent - (ascent_nonpercent + half_leading_nonpercent));

	case CSS_VALUE_baseline:
		break;
	}

	state->valign.logical_above_baseline = ascent - baseline_offset + half_leading;
	state->valign.logical_below_baseline = state->valign.minimum_line_height - state->valign.logical_above_baseline;
	state->valign.logical_above_baseline_nonpercent = ascent_nonpercent - baseline_offset_nonpercent + half_leading_nonpercent;
	state->valign.logical_below_baseline_nonpercent = state->valign.minimum_line_height_nonpercent - state->valign.logical_above_baseline_nonpercent;

	HTML_Element* parent = cascade->FindParent();
	if (parent)
	{
		// Adjust vertical alignment and bounds with parent
		// A loose subtree is not affected by the parent (but may still affect the bounding box of the parent)
		if (!state->valign.loose_root)
		{
			InlineVerticalAlignment valign;

			// Inherit parent properties

			parent->GetLayoutBox()->GetVerticalAlignment(&valign);

			state->valign.logical_above_baseline -= valign.baseline;
			state->valign.logical_below_baseline += valign.baseline;
			state->valign.logical_above_baseline_nonpercent -= valign.baseline_nonpercent;
			state->valign.logical_below_baseline_nonpercent += valign.baseline_nonpercent;
			state->valign.box_above_baseline -= valign.baseline;
			state->valign.box_below_baseline += valign.baseline;
			state->valign.baseline = valign.baseline + baseline_offset;
			state->valign.baseline_nonpercent = valign.baseline_nonpercent + baseline_offset_nonpercent;

			if (valign.loose)
				state->valign.loose = TRUE;
		}

		/* Propagate bounds (box_{above,below}_baseline, and also the vertical line space space required
		   (logical_{above,below}_baseline unless this is the root of a loose subtree. */

		parent->GetLayoutBox()->PropagateVerticalAlignment(state->valign, state->valign.loose_root);
	}
}

OP_STATUS
InlineBox::ListMarkerPreLayout(LayoutInfo& info, LayoutProperties* layout_props)
{
	OP_ASSERT(layout_props->html_element->GetIsListMarkerPseudoElement());

	/* Do some operations depending on the content type:
	   For all: calculate the width, process list numbering
	   For BulletContent: calculate height, set the dimensions
	   For numeration marker (InlineContent): set the numeration string. */

	const HTMLayoutProperties& props = *layout_props->GetProps();
	LayoutCoord width(0);
	BulletContent* bullet_content = content->GetBulletContent();
	Container* list_item = layout_props->container;
	LayoutProperties* list_item_cascade = list_item->GetPlaceholder()->GetCascade();
	int value;

	if (list_item_cascade->container)
		value = list_item_cascade->container->GetNewListItemValue(list_item_cascade->html_element);
	else
		if (list_item_cascade->flexbox)
			value = list_item_cascade->flexbox->GetNewListItemValue(list_item_cascade->html_element);
		else
			/* The list item has no containing element. This may happen when the list
			   item is the root node (and that's possible with SVG foreignobject). */

			value = 1;

	if (bullet_content)
	{
		int height = 0;
		Image img;
		BOOL calculated_from_image = FALSE;

#ifndef SVG_SUPPORT
		img = layout_props->GetListMarkerImage(info.doc);
#else // SVG_SUPPORT
		SVGImage* svg_image = NULL;
		img = layout_props->GetListMarkerImage(info.doc, svg_image);
		if (svg_image)
		{
			int width_resolved;
			if (OpStatus::IsSuccess(svg_image->GetResolvedSize(info.visual_device, layout_props,
															   props.font_size, props.font_size,
															   width_resolved, height)))
			{
				calculated_from_image = TRUE;
				width = LayoutCoord(width_resolved);
			}
		}
		else
#endif // SVG_SUPPORT
#ifdef CSS_GRADIENT_SUPPORT
			if (props.list_style_image_gradient)
			{
				calculated_from_image = TRUE;
				width = LayoutCoord(props.font_size);
				height = LayoutCoord(props.font_size);
			}
			else
#endif // CSS_GRADIENT_SUPPORT
				if (img.Width() && img.Height())
				{
					calculated_from_image = TRUE;
					width = LayoutCoord(img.Width());
					height = static_cast<int>(img.Height());
				}

		if (!calculated_from_image)
		{
			int font_size;

			if (info.doc->GetHandheldEnabled())
			{
				font_size = styleManager->GetStyle(Markup::HTE_DOC_ROOT)->GetPresentationAttr().GetPresentationFont(props.current_script).Font->GetSize();
				font_size = font_size * 2 / 3;
			}
			else
				font_size = props.font_size;

			width = LayoutCoord(MULT_BY_MARKER_WIDTH_RATIO(font_size));
			height = MULT_BY_MARKER_OFFSET_RATIO(font_size);
		}

		bullet_content->SetBulletSize(width, LayoutCoord(height));
	}
	else
	{
		OP_ASSERT(content->IsInlineContent());
		HTML_Element* text_node = layout_props->html_element->FirstChild();

		OP_ASSERT(text_node && text_node->Type() == Markup::HTE_TEXT);

		uni_char str[101]; /* ARRAY OK 2011-06-06 pdamek */
		int str_len;

#ifdef SUPPORT_TEXT_DIRECTION
		str_len = MakeListNumberStr(value, props.list_style_type, UNI_L("."), str, 100,
			props.direction == CSS_VALUE_rtl ? LIST_NUMBER_STR_MODE_RTL : LIST_NUMBER_STR_MODE_LTR);
		/* Marker element shouldn't override direction, so the list item's one
		   should be the same. */
		OP_ASSERT(props.direction == list_item->GetPlaceholder()->GetCascade()->GetProps()->direction);
		/* If this fails, it means that we might have incomplete number string.
		   In case of rtl it may mean that a direction override control
		   character has not been popped, which will result in some layout
		   mess up. */
		OP_ASSERT(str_len < 100);
#else // !SUPPORT_TEXT_DIRECTION
		str_len = MakeListNumberStr(value, props.list_style_type, UNI_L("."), str, 100);
#endif // SUPPORT_TEXT_DIRECTION

		/* format_option param can be zero in the below call, because we don't
		   allow any text-transform nor small caps on list markers. */
		width = LayoutCoord(info.visual_device->GetTxtExtentEx(str, str_len, 0));
		RETURN_IF_ERROR(text_node->SetTextOfNumerationListItemMarker(info.doc, str, str_len));
	}

	if (props.list_style_pos == CSS_VALUE_outside)
	{
		list_item->SetHasFlexibleMarker(TRUE);
		width += props.margin_right;

		InlineVerticalAlignment dummy;

		if (!list_item->AllocateNoncontentLineSpace(layout_props->html_element,
													-width,
													-width,
													LayoutCoord(0),
													dummy,
													width,
													LayoutCoord(0)))
			return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/** Get underline for elements on the specified line. */

/* virtual */ BOOL
InlineBox::GetUnderline(const Line* line, short& underline_position, short& underline_width) const
{
	for (HTML_Element* html_element = GetHtmlElement()->FirstChild();
		 html_element;
		 html_element = html_element->Suc())
	{
		if (html_element->GetLayoutBox())
			if (!html_element->GetLayoutBox()->GetUnderline(line, underline_position, underline_width))
				return FALSE;
	}

	return TRUE;
}

/** Get underline for elements on the specified line. */

/* virtual */ BOOL
Text_Box::GetUnderline(const Line* line, short& underline_position, short& underline_width) const
{
	if (line->GetEndPosition() <= x)
		return FALSE;

	if (line->GetStartPosition() < 0 || line->GetStartPosition() < x + width)
	{
		if (underline_position < packed2.underline_position)
			underline_position = packed2.underline_position;

		if (underline_width < packed2.underline_width)
			underline_width = packed2.underline_width;
	}

	return TRUE;
}

/** Reflow box and children. */

/* virtual */ LAYST
InlineBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	InlineBoxReflowState* state = InitialiseReflowState();

	if (!state)
		return LAYOUT_OUT_OF_MEMORY;
	else
	{
		HTML_Element* html_element = cascade->html_element;

		state->cascade = cascade;

		/* Compute baseline here for non-replaced inlines; is inherited by
		   children. */

		PushTranslations(info, state);

		if (content->IsInlineContent())
		{
#ifdef CONTENT_MAGIC
			if (info.content_magic && html_element->Type() == Markup::HTE_A)
				info.doc->CheckContentMagic();
#endif // CONTENT_MAGIC

			ComputeBaseline(info);

			if (start_position > x)
				state->pending_width = start_position - (x + content->GetWidth());
		}

#ifdef CSS_TRANSFORMS
		if (state->transform)
			if (!state->transform->transform_context->PushTransforms(info, state->transform->translation_state))
				return LAYOUT_OUT_OF_MEMORY;
#endif

		if (first_child && first_child != html_element)
			return content->Layout(cascade, info, first_child, start_position);
		else
		{
			int reflow_content = html_element->MarkClean();
			Container* container = cascade->container;

			if (html_element->GetIsListMarkerPseudoElement())
				if (OpStatus::IsMemoryError(ListMarkerPreLayout(info, cascade)))
					return LAYOUT_OUT_OF_MEMORY;

			if (Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
				return LAYOUT_OUT_OF_MEMORY;

			const HTMLayoutProperties& props = *cascade->GetProps();

#ifdef SUPPORT_TEXT_DIRECTION
			if (props.unicode_bidi != CSS_VALUE_normal)
				if (!container->PushBidiProperties(info, props.direction == CSS_VALUE_ltr,
												   (props.unicode_bidi == CSS_VALUE_bidi_override) ||
												   (info.workplace->GetBidiStatus() == VISUAL_ENCODING && props.direction == CSS_VALUE_ltr)))
					return LAYOUT_OUT_OF_MEMORY;
#endif

			if (!content->IsInlineContent())
			{
				// Make sure that content is initialized to get the correct height

				switch (content->ComputeSize(cascade, info))
				{
				case OpBoolean::IS_TRUE:
					reflow_content = ELM_DIRTY;

				case OpBoolean::IS_FALSE:
					break;

				default:
					return LAYOUT_OUT_OF_MEMORY;
				}

				if (!reflow_content)
					if (
#ifdef PAGED_MEDIA_SUPPORT
						info.paged_media == PAGEBREAK_ON ||
#endif
						cascade->multipane_container ||
						info.stop_before && html_element->IsAncestorOf(info.stop_before))
						reflow_content = ELM_DIRTY;

				x = container->GetLinePendingPosition();

				SpaceManager* local_space_manager = GetLocalSpaceManager();

				if (reflow_content)
				{
					RestartStackingContext();
					ResetRelativeBoundingBox();

					if (local_space_manager && !local_space_manager->Restart())
						return LAYOUT_OUT_OF_MEMORY;

					return content->Layout(cascade, info, NULL, start_position);
				}
				else
				{
					//get stored bounding box as new one will not be calculated
					GetRelativeBoundingBox(state->bounding_box);

					if (!cascade->SkipBranch(info, !local_space_manager, TRUE))
						return LAYOUT_OUT_OF_MEMORY;

					return LAYOUT_CONTINUE;
				}
			}
			else
			{
				if (start_position <= x)
				{
					/* Left non-content (margin, border, padding) has not been
					   added previously. This is the normal case, but with
					   ::first-line it may already have been added. */

					LayoutCoord left_border_padding = props.padding_left + LayoutCoord(props.border.left.width);
					LayoutCoord box_width = left_border_padding + props.margin_left;
					LayoutCoord min_width = LayoutCoord(props.border.left.width);

					if (!props.GetMarginLeftIsPercent())
						min_width += props.margin_left;

					if (!props.GetPaddingLeftIsPercent())
						min_width += props.padding_left;

					state->pending_width += left_border_padding;

					start_position = LAYOUT_COORD_MIN;

					// Allocate space for border and get start position for inline box

					x = container->GetLinePendingPosition();

					/* This is a waste of time if box_width is 0, but need to do it
					   anyway, since outline may be present. */

					if (!container->AllocateNoncontentLineSpace(html_element,
																box_width,
																min_width,
																props.DecorationExtent(info.doc),
																state->valign,
																MAX(LayoutCoord(0), -props.margin_left),
																LayoutCoord(0)))
						return LAYOUT_OUT_OF_MEMORY;
				}

				RestartStackingContext();

				return content->Layout(cascade, info, NULL, start_position);
			}
		}
	}
}

/** Update screen. */

/* virtual */ void
InlineBox::UpdateScreen(LayoutInfo& info)
{
	InlineBoxReflowState* state = GetReflowState();

	HTML_Element* parent = state->cascade->FindParent();

	if (content->IsInlineContent())
		content->GrowInlineWidth(state->pending_width);

	parent->GetLayoutBox()->GrowInlineWidth(state->pending_width);

	state->pending_width = LayoutCoord(0);
}

/** Finish reflowing box. */

/* virtual */ LAYST
InlineBox::FinishLayout(LayoutInfo& info)
{
	InlineBoxReflowState* state = GetReflowState();

	if (!state)
		/* Already finished by a ::first-line that is in a descendant block
		   of this element */
		return LAYOUT_CONTINUE;

	LayoutProperties* cascade = state->cascade;
	Container* container = cascade->container;
	const HTMLayoutProperties& props = *cascade->GetProps();
	const HTMLayoutProperties& parent_props = *cascade->Pred()->GetProps();

	switch (content->FinishLayout(info))
	{
	case LAYOUT_END_FIRST_LINE:
		return LAYOUT_END_FIRST_LINE;

	case LAYOUT_OUT_OF_MEMORY:
		return LAYOUT_OUT_OF_MEMORY;
	}

	if (SpaceManager* local_space_manager = GetLocalSpaceManager())
		local_space_manager->FinishLayout();

	if (StackingContext* local_stacking_context = GetLocalStackingContext())
		local_stacking_context->FinishLayout(cascade);

	LayoutCoord grow_broken_width(0);
	// Needn't append borders for replaced inlines

	if (content->IsInlineContent())
	{
		HTML_Element* html_element = cascade->html_element;

		LayoutCoord right_border_padding = props.padding_right + LayoutCoord(props.border.right.width);

		if (right_border_padding || props.margin_right)
		{
			LayoutCoord box_width = right_border_padding + props.margin_right;
			LayoutCoord min_width = LayoutCoord(props.border.right.width);

			if (!props.GetMarginRightIsPercent())
				min_width += props.margin_right;

			if (!props.GetPaddingRightIsPercent())
				min_width += props.padding_right;

			state->pending_width += right_border_padding;

			if (!container->AllocateNoncontentLineSpace(html_element,
														box_width,
														min_width,
														props.DecorationExtent(info.doc),
														state->valign,
														LayoutCoord(0),
														MAX(LayoutCoord(0), -props.margin_right)))
				return LAYOUT_OUT_OF_MEMORY;
		}

		if (html_element->GetIsListMarkerPseudoElement() && props.list_style_pos == CSS_VALUE_inside)
			// Need to explicitly override the last_base_character, as there is no space after an inside list item marker.
			container->SetLastBaseCharacterFromWhiteSpace(static_cast<const CSSValue>(parent_props.white_space));
		else if (props.white_space == CSS_VALUE_nowrap && parent_props.white_space == CSS_VALUE_normal)
			container->SetLastBaseCharacter(0x200B);

		content->GrowInlineWidth(state->pending_width);

		// The parent box needs to know about the horizontal margin
		state->pending_width += props.margin_left + props.margin_right;
	}
	else
		for (;;)
		{
			LayoutCoord width;
			LayoutCoord nonpercent_margin_left = props.GetMarginLeftIsPercent() ? LayoutCoord(0) : props.margin_left;
			LayoutCoord nonpercent_margin_right = props.GetMarginRightIsPercent() ? LayoutCoord(0) : props.margin_right;
			LayoutCoord min_width(0);
			LayoutCoord normal_min_width(0);
			LayoutCoord max_width(0);
			LinebreakOpportunity break_properties = LB_NO;
			LAYST continue_layout;
			BOOL keep_min_widths = FALSE;
			BOOL weak_non_normal = info.doc->GetLayoutMode() != LAYOUT_NORMAL && content->IsWeakContent();

			ComputeBaseline(info);

			width = content->GetWidth() + props.margin_right + props.margin_left;

			content->GetMinMaxWidth(min_width, normal_min_width, max_width);

			if (weak_non_normal)
			{
				if (nonpercent_margin_left > 2)
					min_width += LayoutCoord(2);
				else
					min_width += nonpercent_margin_left;

				if (nonpercent_margin_right > 2)
					min_width += LayoutCoord(2);
				else
					min_width += nonpercent_margin_right;

				// Modify margin if this replaced content is scaled.

				UINT32 unbreakable_width = (UINT32) cascade->html_element->GetSpecialNumAttr(Markup::LAYOUTA_UNBREAKABLE_WIDTH, SpecialNs::NS_LAYOUT);
				if (unbreakable_width)
				{
					LayoutCoord min_unbreakable_width(unbreakable_width & 0x0000ffff);
					LayoutCoord weak_unbreakable_width(unbreakable_width >> 16);

					if (props.max_width < min_unbreakable_width + weak_unbreakable_width)
					{
						if (min_unbreakable_width < props.max_width)
						{
							LayoutCoord margins = props.margin_left + props.margin_right;

							if (margins > 4)
							{
								margins = ((margins - LayoutCoord(4)) * (props.max_width - min_unbreakable_width)) / weak_unbreakable_width + LayoutCoord(4);
								width += margins - props.margin_left - props.margin_right;
							}
						}
					}
				}
			}
			else
				min_width += nonpercent_margin_right + nonpercent_margin_left;

			normal_min_width += nonpercent_margin_right + nonpercent_margin_left;
			max_width += nonpercent_margin_right + nonpercent_margin_left;

			{
				break_properties = container->GetLinebreakOpportunity(0xFFFC);

				container->SetLastBaseCharacterFromWhiteSpace(static_cast<const CSSValue>(parent_props.white_space));

				if (!info.hld_profile->IsInStrictMode() && cascade->html_element->Type() == Markup::HTE_IMG && !info.treat_nbsp_as_space)
				{
					/* Don't allow break before or after images inside tables,
					   unless there's a container in between with absolute specified width. */

					for (LayoutProperties* parent = cascade->Pred(); parent && parent->html_element; parent = parent->Pred())
					{
						Box* box = parent->html_element->GetLayoutBox();

						if (box)
							if (box->GetTableContent())
							{
								keep_min_widths = TRUE;
								break;
							}
							else
								if (box->GetContainer() && parent->GetProps()->content_width > 0)
									break;
					}
				}
			}

			LayoutCoord shadow = props.DecorationExtent(info.doc);

			const RelativeBoundingBox& bounding_box = state->bounding_box;
			InlineVerticalAlignment dummy;

			container->AllocateNoncontentLineSpace(cascade->html_element,
												   LayoutCoord(0),
												   LayoutCoord(0),
												   shadow,
												   dummy,
												   MAX(-props.margin_left, LayoutCoord(0)),
												   LayoutCoord(0));

			// Allocate space for this box on the line

			continue_layout = container->AllocateLineSpace(cascade->html_element, info,
														   width, min_width, max_width,
														   state->valign,
														   MAX(bounding_box.Top(), shadow), MAX(bounding_box.Bottom(), shadow),
														   MAX(bounding_box.Left(), shadow), MAX(bounding_box.Right(), shadow),
														   state->bounding_box.ContentRight(), state->bounding_box.ContentBottom(),
														   state->abspos_pending_bbox_right, state->abspos_pending_bbox_bottom,
														   0,
														   break_properties,
														   keep_min_widths);

			container->AllocateNoncontentLineSpace(cascade->html_element,
												   LayoutCoord(0),
												   LayoutCoord(0),
												   shadow,
												   dummy,
												   LayoutCoord(0),
												   MAX(-props.margin_right, LayoutCoord(0)));

			if (continue_layout == LAYOUT_END_FIRST_LINE)
			{
				// First-line finished

				if (!container->EndFirstLine(info, cascade))
					return LAYOUT_OUT_OF_MEMORY;

				return LAYOUT_END_FIRST_LINE;
			}
			else
				if (continue_layout == LAYOUT_CONTINUE)
				{
					state->pending_width += width;

					if (weak_non_normal)
						container->AllocateWeakContentSpace(cascade->html_element, normal_min_width - min_width, FALSE);

					break;
				}
				else
					// Out of memory

					return LAYOUT_OUT_OF_MEMORY;
		}

#ifdef SUPPORT_TEXT_DIRECTION
	if (props.unicode_bidi != CSS_VALUE_normal)
		if (!container->PopBidiProperties())
			return LAYOUT_OUT_OF_MEMORY;
#endif

#ifdef CSS_TRANSFORMS
	if (state->transform)
	{
		TransformContext *transform_context = state->transform->transform_context;
		transform_context->PopTransforms(info, state->transform->translation_state);

		LayoutCoord height(0);

		if (content->IsInlineContent())
			height = state->valign.box_above_baseline + state->valign.box_below_baseline;
		else
			height = GetHeight();

		if (transform_context->Finish(info, cascade, GetWidth(), height))

			/* Finish() calculated a new transform; reflow the inline
			   again so the proper transform is set when we allocate
			   space on the line.  The line needs to know the
			   transform when the line space is allocated so that it
			   can keep track of the bounding box.

			   As long as the transform does not not influence the
			   size of the box, this should be safe from reflow
			   loops. */

			content->ForceReflow(info);
	}
#endif

	if (IsPositionedBox())
	{
		StackingContext *stacking_context = GetLocalStackingContext();

		if (!stacking_context)
			stacking_context = cascade->stacking_context;

		OP_ASSERT(stacking_context);

		stacking_context->UpdateBottomAligned(info);

		PopTranslations(info, state);
	}

	if (info.doc->GetHandheldEnabled())
	{
		BOOL force_line_end = FALSE;

		switch (cascade->html_element->Type())
		{
		case Markup::HTE_A:
		case Markup::HTE_TD:
		case Markup::HTE_TH:
			force_line_end = container->GetContentWidth() && GetWidth() > container->GetContentWidth();
			break;

		case Markup::HTE_TR:
			{
				int cell_count = 0;

				for (HTML_Element* child_element = cascade->html_element->FirstChild(); child_element && cell_count < 2; child_element = child_element->Suc())
					if (child_element->Type() == Markup::HTE_TD || child_element->Type() == Markup::HTE_TH)
						cell_count++;

				force_line_end = cell_count > 1;
			}
			break;
		}

		if (force_line_end)
			container->ForceLineEnd(info, cascade->html_element, FALSE);
	}


	HTML_Element* parent = cascade->FindParent();

	parent->GetLayoutBox()->GrowInlineWidth(state->pending_width - grow_broken_width);

	SetRelativeBoundingBox(state->bounding_box);

	DeleteReflowState();

	return LAYOUT_CONTINUE;
}

/** Get the percent of normal width when page is shrunk in ERA. */

/* virtual */ int
Content_Box::GetPercentOfNormalWidth() const
{
	return content->GetPercentOfNormalWidth();
}

/** Clean out reflow states. */

/* virtual */ void
Content_Box::CleanReflowState()
{
	DeleteReflowState();
	content->DeleteReflowState();
}

/** Get width available for the margin box. */

/* virtual */ LayoutCoord
Content_Box::GetAvailableWidth(LayoutProperties* cascade)
{
	OP_ASSERT(!cascade->flexbox); // meaningless to ask a flex item about this

	return cascade->container ? cascade->container->GetContentWidth() : LayoutCoord(0);
}

/** Get width of box. */

/* virtual */ LayoutCoord
Content_Box::GetWidth() const
{
	return content->GetWidth();
}

/** Get height. */

/* virtual */ LayoutCoord
Content_Box::GetHeight() const
{
	return content->GetHeight();
}

/** Get container established by this box, if any. */

/* virtual */ Container*
Content_Box::GetContainer() const
{
	return content->GetContainer();
}

/** Get container of this box, if it has one. */

/* virtual */ TableContent*
Content_Box::GetTableContent() const
{
	return content->GetTableContent();
}

/** Get flex content (also known as flexbox or flex container) established by this box, if it has one. */

/* virtual */ FlexContent*
Content_Box::GetFlexContent() const
{
	return content->GetFlexContent();
}

/** Get selectcontent of this box, if it has one. */

/* virtual */ SelectContent*
Content_Box::GetSelectContent() const
{
	return content->GetSelectContent();
}

/** Get scrollable, if it is one. */

/* virtual */ ScrollableArea*
Content_Box::GetScrollable() const
{
	return content->GetScrollable();
}

/** Get button container of this box, if it has one. */

/* virtual */ ButtonContainer*
Content_Box::GetButtonContainer() const
{
	return content->GetButtonContainer();
}

#ifdef SVG_SUPPORT
/** Get svg content of this box, if it has one. */

/* virtual */ SVGContent*
Content_Box::GetSVGContent() const
{
	return content->GetSVGContent();
}
#endif // SVG_SUPPORT

/* virtual */ BulletContent*
Content_Box::GetBulletContent() const
{
	return content->GetBulletContent();
}

/** Does this box have inline content? */

/* virtual */ BOOL
Content_Box::IsInlineContent() const
{
	return content->IsInlineContent();
}

/** Is this box one of the table boxes or a box with table content? */

/* virtual */ BOOL
Content_Box::IsTableBox() const
{
	return content->GetTableContent() != NULL;
}

/** Is content of this box considered a containing element? */

/* virtual */ BOOL
Content_Box::IsContainingElement() const
{
	return content->IsContainingElement();
}

/** Is content of this box replaced content? */

/* virtual */ BOOL
Content_Box::IsContentReplaced() const
{
	return content->IsReplaced();
}

/** Is content of this box image content? */

/* virtual */ BOOL
Content_Box::IsContentImage() const
{
	return content->IsImage();
}

/** Is content of this box embed content? */

/* virtual */ BOOL
Content_Box::IsContentEmbed() const
{
	return content->IsEmbed();
}

/** Is there typically a CoreView associated with this box ?

	@see class CoreView */

/* virtual */ BOOL
Content_Box::HasCoreView() const
{
	return content->HasCoreView();
}

/** Create form, plugin and iframe objects */

/* virtual */ OP_STATUS
Content_Box::EnableContent(FramesDocument* doc)
{
	return content->Enable(doc);
}

/** Remove form, plugin and iframe objects */

/* virtual */ void
Content_Box::MarkDisabledContent(FramesDocument* doc)
{
    MarkDirty(doc);

	content->MarkDisabled();
}

/** Remove form, plugin and iframe objects */

/* virtual */ void
Content_Box::DisableContent(FramesDocument* doc)
{
	HTML_Element* html_element = GetHtmlElement();

	if (html_element->HasBgImage())
		html_element->UndisplayImage(doc, TRUE);

	if (doc)
	{
		LayoutWorkplace* workplace = doc->GetLogicalDocument()->GetLayoutWorkplace();

		if (workplace->StoreReplacedContentEnabled() && content->IsEmbed())
		{
			/* We assume that this box is about to be deleted, and that we're
			   reflowing. Store the ReplacedContent of this box and reuse it if
			   the box is recreated during this reflow pass. This is done to
			   keep the content's state from being reset.

			   This means that we are indeed NOT disabling content in this
			   case. And we're in a method called DisableContent()
			   now... hmm...

			   It should also be mentioned that the whole stored
			   ReplacedContent business is risky. We disconnect the Content_Box
			   from its ReplacedContent here. Content_Box suddenly cannot
			   assume that it has a valid Content pointer, and vice versa. */

			if (workplace->StoreReplacedContent(html_element, (ReplacedContent*) content))
				content->SetPlaceholder(NULL);
			else
			{
				// FIXME: OOM

				content->Disable(doc);
				OP_DELETE(content);
			}

			content = NULL;

			return;
		}

		content->Disable(doc);
	}
}

/** Stores the formobject in the HTML_Element (for backup during reflow)  */

void Content_Box::StoreFormObject()
{
	content->StoreFormObject();
}

/** Add option element to 'select' content */

/* virtual */ OP_STATUS
Content_Box::AddOption(HTML_Element* option_element, unsigned int &index)
{
	return content->AddOption(option_element, index);
}

/** Remove option element from 'select' content */

/* virtual */ void
Content_Box::RemoveOption(HTML_Element* option_element)
{
	content->RemoveOption(option_element);
}

/** Remove optiongroup element from 'select' content */

void
Content_Box::RemoveOptionGroup(HTML_Element* optiongroup_element)
{
	content->RemoveOptionGroup(optiongroup_element);
}

/** Begin optiongroup in 'select' content */

void
Content_Box::BeginOptionGroup(HTML_Element* optiongroup_element)
{
	content->BeginOptionGroup(optiongroup_element);
}

/** End optiongroup in 'select' content */

void
Content_Box::EndOptionGroup(HTML_Element* optiongroup_element)
{
	content->EndOptionGroup(optiongroup_element);
}

/** Layout option element in 'select' content */

/* virtual */ OP_STATUS
Content_Box::LayoutOption(LayoutWorkplace* workplace, unsigned int index)
{
	return content->LayoutOption(GetCascade(), workplace, index);
}

/** Lay out form content. */

/* virtual */ OP_STATUS
Content_Box::LayoutFormContent(LayoutWorkplace* workplace)
{
	return content->LayoutFormContent(GetCascade(), workplace);
}

/** Get form object. */

/* virtual */ FormObject*
Content_Box::GetFormObject() const
{
	return content->GetFormObject();
}

/** Clear min/max width. */

/* virtual */ void
Content_Box::ClearMinMaxWidth()
{
	content->ClearMinMaxWidth();
}

/** Delete reflow state. */

/* virtual */ void
Content_Box::DeleteReflowState()
{
	ReflowState* state = GetReflowState();

	if (state)
	{
		un.html_element = state->html_element;
		OP_DELETE(state);
	}
}

/* virtual */ BOOL
Content_Box::RemoveCachedInfo()
{
	return content->RemoveCachedInfo();
}

/** Should this box layout its children? */

/* virtual */ BOOL
Content_Box::ShouldLayoutChildren()
{
	return content->ShouldLayoutChildren();
}

/** Apply clipping */

void
Content_Box::PushClipRect(TraversalObject* traversal_object, LayoutProperties* cascade, TraverseInfo& info)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	OpRect rect = GetClippedRect(props, TRUE);
	traversal_object->PushClipRect(rect, info);
}

/** Remove clipping */

void
Content_Box::PopClipRect(TraversalObject* traversal_object, TraverseInfo& info)
{
	traversal_object->PopClipRect(info);
}

/** Get the distance between this box and an ancestor (number of pixels
	below and to the right of the ancestor, measured between top/left border
	edges). */

int
Box::GetOffsetFromAncestor(LayoutCoord &x, LayoutCoord &y, HTML_Element* ancestor, int flags)
{
	enum PosGetState {
		GET,
		GET_IF_POSITIONED,
		GET_IF_TRANSFORMED,
		NO_GET
	};

	HTML_Element *element = GetHtmlElement();

	if (ancestor == element)
		// no offset from ourselves
		return 0;

	Container* root_multipane_container = NULL; // root paged or multicol container that affects this element.
	PosGetState x_getstate = GET;
	PosGetState y_getstate = GET;
	int result = 0;
	BOOL is_offset_parent = !!(flags & GETPOS_OFFSET_PARENT);
	BOOL multipane_x_handled = FALSE;
	BOOL multipane_y_handled = FALSE;

	/* The ignore_scroll_offset_* variables are used for hypothetically static
	   positioned absolute positioned boxes, whose position is not affected
	   by scroll position of non positioned ancestors between this and the
	   containing element.

	   See also @ref TraversalObject::SyncRootScroll and @ref Box::GetOffsetTranslations. */

	BOOL ignore_scroll_offset_x = FALSE;
	BOOL ignore_scroll_offset_y = FALSE;

	do
	{
		Box *box = element->GetLayoutBox();
		HTML_Element *containing_element = GetContainingElement(element);

		if (box->IsPositionedBox())
		{
			if (x_getstate == GET_IF_POSITIONED)
				x_getstate = GET;

			if (y_getstate == GET_IF_POSITIONED)
				y_getstate = GET;
		}

#ifdef CSS_TRANSFORMS
		if (box->HasTransformContext())
		{
			if (x_getstate == GET_IF_TRANSFORMED)
				x_getstate = GET;

			if (y_getstate == GET_IF_TRANSFORMED)
				y_getstate = GET;

			result |= GETPOS_TRANSFORM_FOUND;
		}
#endif

		/* For offset parent, we must be careful to not add offsets to the containing element if the
		   containing element is an ancestor to the ancestor argument.

		   This is a hack to fix the case for when we have block-level elements inside positioned
		   inline-level elements. In this case the containing element should strictly speaking be
		   the positioned inline-level element.  That is not what GetContainingElement returns
		   though, it returns the element corresponding to the Container of both the inline-level
		   element and the block-level element. */

		BOOL include_container = !is_offset_parent || !ancestor || ancestor->IsAncestorOf(containing_element);
		BOOL in_multipane = FALSE;

		if (include_container)
			if (box->IsBlockBox())
			{
				BlockBox* block_box = (BlockBox*) box;

				if (x_getstate == GET)
					x += block_box->GetX();

				if (y_getstate == GET)
					y += block_box->GetY();

				in_multipane = block_box->IsInMultiPaneContainer();
			}
			else
				if (box->IsInlineBox())
				{
					if (box->IsPositionedBox())
					{
						PositionedInlineBox* pos_inline_box = (PositionedInlineBox*) box;

						if (x_getstate == GET)
							x += LayoutCoord(pos_inline_box->GetContainingBlockX() + pos_inline_box->GetAccumulatedLeftOffset());

						if (y_getstate == GET)
							y += LayoutCoord(pos_inline_box->GetContainingBlockY() + pos_inline_box->GetAccumulatedTopOffset());
					}
					else
					{
						result |= GETPOS_INLINE_FOUND;
						if (flags & GETPOS_ABORT_ON_INLINE)
							return result;

						if (x_getstate == GET || y_getstate == GET)
						{
							LayoutCoord virtual_pos = box->GetVirtualPosition();
							Line *line = containing_element->GetLayoutBox()->GetContainer()->GetLineAtVirtualPosition(virtual_pos);
							if (line)
							{
								if (x_getstate == GET)
									x += virtual_pos - line->GetStartPosition() + line->GetX(); // FIXME: text alignment/direction ignored

								if (y_getstate == GET)
									y += line->GetY();
							}
						}
					}
				}
				else
					if (box->IsTableCell())
					{
						TableCellBox* tb = (TableCellBox*)box;

						if (x_getstate == GET)
							x += tb->GetPositionedX();

						if (y_getstate == GET)
						{
							y += tb->GetPositionedY();

							if (tb->GetTableRowBox()->IsWrapped())
								y += LayoutCoord(tb->GetCellYOffset());
						}
					}
					else
						if (box->IsTableRow())
						{
							if (x_getstate == GET)
								x += ((TableRowBox*)box)->GetPositionedX();

							if (y_getstate == GET)
								y += ((TableRowBox*)box)->GetPositionedY();

							in_multipane = ((TableRowBox*) box)->IsInMultiPaneContainer();
						}
						else
							if (box->IsTableRowGroup())
							{
								if (x_getstate == GET)
									x += ((TableRowGroupBox*)box)->GetPositionedX();

								if (y_getstate == GET)
									y += ((TableRowGroupBox*)box)->GetPositionedY();

								in_multipane = ((TableRowGroupBox*) box)->IsInMultiPaneContainer();
							}
							else
								if (box->IsTableCaption())
								{
									if (x_getstate == GET)
										x += ((TableCaptionBox*)box)->GetPositionedX();

									if (y_getstate == GET)
										y += ((TableCaptionBox*)box)->GetPositionedY();

									in_multipane = ((TableCaptionBox*) box)->IsInMultiPaneContainer();
								}
								else
									if (box->IsFlexItemBox())
									{
										if (x_getstate == GET)
											x += ((FlexItemBox*) box)->GetX();

										if (y_getstate == GET)
											y += ((FlexItemBox*) box)->GetY();

										in_multipane = ((FlexItemBox*) box)->IsInMultiPaneContainer();
									}

		if (box->IsAbsolutePositionedBox())
		{
			BOOL is_fixed = box->IsFixedPositionedBox(TRUE);

			if (is_fixed)
				result |= GETPOS_FIXED_FOUND;

			BOOL is_fixed_and_transformed = is_fixed && !box->IsFixedPositionedBox();

			if (x_getstate != NO_GET)
				if (((AbsolutePositionedBox*) box)->GetHorizontalOffset() == NO_HOFFSET)
					ignore_scroll_offset_x = TRUE;
				else
				{
					if (is_fixed_and_transformed)
						x_getstate = GET_IF_TRANSFORMED;
					else if (is_fixed)
						x_getstate = NO_GET;
					else
						x_getstate = GET_IF_POSITIONED;
				}

			if (y_getstate != NO_GET)
				if(((AbsolutePositionedBox*) box)->GetVerticalOffset() == NO_VOFFSET)
					ignore_scroll_offset_y = TRUE;
				else
				{
					if (is_fixed_and_transformed)
						y_getstate = GET_IF_TRANSFORMED;
					else if (is_fixed)
						y_getstate = NO_GET;
					else
						y_getstate = GET_IF_POSITIONED;
				}
		}

		if (in_multipane)
		{
			/* We're inside a multicol or paged container. We may need to
			   compensate for the translation set on the column(s)/page(s) in
			   which this block box lives. Find the root pane container. */

			if (!root_multipane_container)
				root_multipane_container = Container::FindOuterMultiPaneContainerOf(element);

			OP_ASSERT(root_multipane_container);

			BOOL get_x = !multipane_x_handled &&
				(x_getstate == GET
#ifdef PAGED_MEDIA_SUPPORT
				 || root_multipane_container->GetPagedContainer() && x_getstate != NO_GET
#endif // PAGED_MEDIA_SUPPORT
					);

			BOOL get_y = !multipane_y_handled &&
				(y_getstate == GET
#ifdef PAGED_MEDIA_SUPPORT
				 || root_multipane_container->GetPagedContainer() && y_getstate != NO_GET
#endif // PAGED_MEDIA_SUPPORT
					);

			if (get_x || get_y)
			{
				LayoutCoord translation_x;
				LayoutCoord translation_y;

				root_multipane_container->GetAreaAndPaneTranslation(box, translation_x, translation_y);

				if (get_x)
				{
					x += translation_x;
					multipane_x_handled = TRUE;
				}

				if (get_y)
				{
					y += translation_y;
					multipane_y_handled = TRUE;
				}
			}
		}

		if (x_getstate == GET_IF_POSITIONED || y_getstate == GET_IF_POSITIONED)
			/* We're looking for a containing block for absolutely positioned
			   boxes, so let's to make sure that we don't jump over positioned
			   inline boxes. */

			for (HTML_Element *elm = element->Parent(); elm != containing_element; elm = elm->Parent())
			{
				Box *box = elm->GetLayoutBox();
				if (box && box->IsInlineBox() && box->IsPositionedBox())
				{
					containing_element = elm;
					break;
				}
			}

		if (containing_element)
		{
			Box* containing_box = containing_element->GetLayoutBox();

			if (containing_box->IsTableCell())
			{
				if (y_getstate == GET)
					y += ((TableCellBox*)containing_box)->ComputeCellY(FALSE);
			}

			if (containing_box->IsPositionedBox())
			{
				ignore_scroll_offset_x = FALSE;
				ignore_scroll_offset_y = FALSE;
			}

			if (Content* content = containing_box->GetContent())
			{
				if (!(flags & GETPOS_IGNORE_SCROLL))
					if (ScrollableArea* scrollable = content->GetScrollable())
					{
						if ((x_getstate == GET && !ignore_scroll_offset_x) || x_getstate == GET_IF_POSITIONED && containing_box->IsPositionedBox())
							x -= scrollable->GetViewX();

						if ((y_getstate == GET && !ignore_scroll_offset_y)|| y_getstate == GET_IF_POSITIONED && containing_box->IsPositionedBox())
							y -= scrollable->GetViewY();
					}
#ifdef PAGED_MEDIA_SUPPORT
					else
						if (PagedContainer* pc = content->GetPagedContainer())
						{
							if ((x_getstate == GET && !ignore_scroll_offset_x) || x_getstate == GET_IF_POSITIONED && containing_box->IsPositionedBox())
								x -= pc->GetViewX();

							if ((y_getstate == GET && !ignore_scroll_offset_y)|| y_getstate == GET_IF_POSITIONED && containing_box->IsPositionedBox())
								y -= pc->GetViewY();
						}
#endif // PAGED_MEDIA_SUPPORT

				if (Container* container = content->GetContainer())
					if (container == root_multipane_container)
					{
						multipane_x_handled = multipane_y_handled = FALSE;
						root_multipane_container = NULL;
					}
			}
		}

		element = containing_element;
	}
	while (element && element != ancestor && (x_getstate != NO_GET || y_getstate != NO_GET));

	if ((x_getstate == GET_IF_POSITIONED || y_getstate == GET_IF_POSITIONED) && ancestor && !ancestor->GetLayoutBox()->IsPositionedBox())
	{
		/* If we end up here, it means we have tried to find the offset of an
		   absolutely positioned box relative to a non-positioned ancestor. We have
		   stopped at the non-positioned ancestor meaning we have already added
		   the offset from the absolutely positioned box to its positioned containing
		   element. To get the correct offset, we get the offset of the ancestor
		   element relative to the positioned containing element and subtract that
		   offset from the current result.

		   Example:

		   <div id="abs" style="position: absolute;">
		     <div id="spacer" style="height: 200px;"></div>
		     <div id="ancestor">
			    <div id="box" style="position: absolute; top: 400px;">

		   If we're doing #box.getOffsetFromAncestor(#ancestor) here we will have
		   an y offset of 400px at this point. We then do
		   #ancestor.getOffsetFromAncestor(#abs) in the code below to subtract the
		   200px added by #spacer in the case above. */

		LayoutCoord cx(0);
		LayoutCoord cy(0);
		Box* ancestor_box = ancestor->GetLayoutBox();
		result |= ancestor_box->GetOffsetFromAncestor(cx, cy, GetContainingElement(ancestor, TRUE), flags);
		if (x_getstate == GET_IF_POSITIONED)
			x -= cx;
		if (y_getstate == GET_IF_POSITIONED)
			y -= cy;
	}

	if (root_multipane_container && ancestor && ancestor->GetLayoutBox() != root_multipane_container->GetPlaceholder())
	{
		/* We're still inside a multi-pane container. We need to compensate for
		   the translation set on the column(s)/page(s) in which the ancestor
		   block box lives. */

		LayoutCoord translation_x;
		LayoutCoord translation_y;

		root_multipane_container->GetAreaAndPaneTranslation(ancestor->GetLayoutBox(), translation_x, translation_y);

		if (multipane_x_handled)
			x -= translation_x;

		if (multipane_y_handled)
			y -= translation_y;
	}

	return result;
}

/** Mark all boxes with fixed (non-percentage, non-auto) width in this subtree dirty. */

void
Box::MarkBoxesWithAbsoluteWidthDirty(FramesDocument* doc)
{
	HTML_Element* html_element = GetHtmlElement();

	if (HasAbsoluteWidth())
		html_element->MarkDirty(doc, TRUE);

	for (HTML_Element* child = html_element->FirstChild(); child; child = child->Suc())
		if (Box* box = child->GetLayoutBox())
			box->MarkBoxesWithAbsoluteWidthDirty(doc);
}

/** Mark all containers in this subtree dirty. */

void
Box::MarkContainersDirty(FramesDocument* doc)
{
	HTML_Element* html_element = GetHtmlElement();

	if (GetContainer())
		html_element->MarkDirty(doc, FALSE);

	// possible optimization for LIMIT_PARAGRAPH_WIDTH: only mark wide enough containers dirty??

	for (HTML_Element* child = html_element->FirstChild(); child; child = child->Suc())
		if (Box* box = child->GetLayoutBox())
			box->MarkContainersDirty(doc);
}

/** Mark all content with relative height in this subtree dirty. */

void
Box::MarkContentWithPercentageHeightDirty(FramesDocument* doc)
{
	HTML_Element* html_element = GetHtmlElement();

	if (Content* content = GetContent())
		if (content->HeightIsRelative())
			html_element->MarkDirty(doc, FALSE);

	if (!IsContentReplaced() && !GetTableContent())
		for (HTML_Element* child = html_element->FirstChild(); child; child = child->Suc())
			if (Box* box = child->GetLayoutBox())
				box->MarkContentWithPercentageHeightDirty(doc);
}

/** Clear 'use old row heights' mark on all tables in this subtree. */

void
Box::ClearUseOldRowHeights()
{
	if (TableContent* table = GetTableContent())
		table->ClearUseOldRowHeights();
	else
		for (HTML_Element* child = GetHtmlElement()->FirstChild(); child; child = child->Suc())
			if (Box* box = child->GetLayoutBox())
				box->ClearUseOldRowHeights();
}

/** Get current page and number of pages. */

void
Box::GetPageInfo(unsigned int& current_page, unsigned int& page_count)
{
#ifdef PAGED_MEDIA_SUPPORT
	if (PagedContainer* paged_container = FindPagedContainer(this))
	{
		current_page = paged_container->GetCurrentPageNumber();
		page_count = paged_container->GetTotalPageCount();
	}
	else
#endif // PAGED_MEDIA_SUPPORT
	{
		current_page = 0;
		page_count = 1;
	}
}

/** Set current page number. */

void
Box::SetCurrentPageNumber(unsigned int current_page)
{
#ifdef PAGED_MEDIA_SUPPORT
	if (PagedContainer* paged_container = FindPagedContainer(this))
		paged_container->SetCurrentPageNumber(current_page, TRUE);
#endif // PAGED_MEDIA_SUPPORT
}

/** Return TRUE if this box has a list item marker pseudo element that has a box. */

BOOL
Box::HasListMarkerBox() const
{
	HTML_Element* marker_element = GetListMarkerElement();
	return marker_element && marker_element->GetLayoutBox();
}

/** Return TRUE if this box is an anonymous wrapper box inserted by layout. */

BOOL
Box::IsAnonymousWrapper() const
{
	HTML_Element* html_element = GetHtmlElement();

	if (html_element->GetInserted() == HE_INSERTED_BY_LAYOUT && !html_element->GetIsPseudoElement())
		return IsTableBox() || IsFlexItemBox();

	return FALSE;
}

BlockBox::BlockBox(HTML_Element* element, Content* content)
  : VerticalBox(element, content),
	packed_init(0),
	x(0),
	y(0),
	min_y(0),
	virtual_position(LAYOUT_COORD_MIN)
{
}

/** Expand container to make space for floating and absolute positioned boxes. See declaration in header file for more information. */

/* virtual */ void
BlockBox::PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	ReflowState* state = GetReflowState();
	Container* container = NULL;

	/** Update bounding boxes - if we're between an abspos element and its containing block
		no clipping can be done (bounding needs boxes to be updated). After we have passed containing element
		we may safely clip any overflow (by skipping bbox update for example) */

	BOOL is_positioned_box = IsPositionedBox();

	if (opts.type == PROPAGATE_ABSPOS_BOTTOM_ALIGNED)
	{
		StackingContext *context = GetLocalStackingContext();

		if (context)
			context->SetNeedsBottomAlignedUpdate();

		if (is_positioned_box)
		{
			if (!context)
				state->cascade->stacking_context->SetNeedsBottomAlignedUpdate();

			return;
		}
	}
	else if (opts.type != PROPAGATE_ABSPOS_SKIPBRANCH || IsOverflowVisible())
		UpdateBoundingBox(child_bounding_box, opts.type != PROPAGATE_FLOAT && !is_positioned_box);

	/* Propagating bboxes when skipping branches should be stopped on the nearest
	   abspos box. BBox propagation of such box will be sooner or later triggered
	   by another SkipBranch call so there is no need to do it now. */

	if (opts.type == PROPAGATE_ABSPOS_SKIPBRANCH && IsAbsolutePositionedBox())
		return;

	if (opts.type == PROPAGATE_ABSPOS && is_positioned_box || opts.type == PROPAGATE_ABSPOS_SKIPBRANCH)
	{
		/** We have either reached a containing element or we already passed it */

		if (state)
			/** If we're not in SkipBranch or we reached element that is still being reflowed
				we can safely stop bbox propagation here. Bounding box will be propagated
				when layout of this box is finished */

			return;

		/** We've already found a containing element but since it had no reflow state we must be
			skipping a branch. Switch propagation type (this will enable us to clip such bbox)
			and continue propagation until we reach first element that is being reflowed */
		opts.type = PROPAGATE_ABSPOS_SKIPBRANCH;
	}

	if (state)
		container = state->cascade->container;
	else
	{
		HTML_Element* containing_element = GetContainingElement();

		if (containing_element)
			container = containing_element->GetLayoutBox()->GetContainer();
	}

	if (container)
	{
		AbsoluteBoundingBox abs_bounding_box;

		if (opts.type == PROPAGATE_ABSPOS_SKIPBRANCH)
			GetBoundingBox(abs_bounding_box, IsOverflowVisible());
		else
			abs_bounding_box = child_bounding_box;

		abs_bounding_box.Translate(x, y);

		container->PropagateBottom(info, bottom + y, min_bottom + min_y, abs_bounding_box, opts);
	}
}

/** Propagate the right edge of absolute positioned boxes. */

/* virtual */ void
BlockBox::PropagateRightEdge(const LayoutInfo& info, LayoutCoord right, LayoutCoord noncontent, LayoutFixed percentage)
{
	ReflowState* state = GetReflowState();
	Container* container = NULL;

	if (IsPositionedBox())
	{
		/* The containing block of the child boxes being propagated is not the
		   root element. Stop propagation. FIXME: This isn't the correct thing
		   to do for fixed positioned boxes. */

		return;
	}

	if (state)
		container = state->cascade->container;
	else
	{
		HTML_Element* containing_element = GetContainingElement();

		if (containing_element)
			container = containing_element->GetLayoutBox()->GetContainer();
	}

	if (container)
		container->PropagateRightEdge(info, x + right, x + noncontent, percentage); // FIXME: x may depend on initial containing block
}

/** Propagate a break point caused by break properties or a spanned element. */

/* virtual */ BOOL
BlockBox::PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint)
{
	if (packed.column_spanned && break_type != PAGE_BREAK)
		return TRUE;
	else
		return GetCascade()->container->PropagateBreakpoint(virtual_y + y, break_type, breakpoint);
}

/** Get width available for the margin box. */

/* virtual */ LayoutCoord
BlockBox::GetAvailableWidth(LayoutProperties* cascade)
{
	if (packed.column_spanned)
		return ((MultiColumnContainer*) cascade->multipane_container)->GetOuterContentWidth();

	Container* container = cascade->container;
	LayoutCoord available_width = container->GetContentWidth();

	if (cascade->IsLayoutWidthAuto() &&
		GetLocalSpaceManager() &&
		(content->GetContainer() || content->GetFlexContent()) &&
		IsInStack())
	{
		/* Width of the block is auto, and it must not overlap floats. Find out
		   how much space is available at the block's position. */

		const HTMLayoutProperties& props = *cascade->GetProps();
		LayoutCoord y_in = y;
		LayoutCoord x_in = x + props.margin_left;
		LayoutCoord horizontal_margins = props.margin_left + props.margin_right;
		LayoutCoord border_padding = props.padding_left + props.padding_right +
			LayoutCoord(props.border.left.width + props.border.right.width);
		LayoutCoord minimum_width(0);

		if (props.box_sizing == CSS_VALUE_border_box)
			minimum_width += border_padding;

		minimum_width = props.CheckWidthBounds(minimum_width);

		if (props.box_sizing != CSS_VALUE_border_box)
			minimum_width += border_padding;

		available_width -= horizontal_margins;
		container->GetSpace(cascade->space_manager, y_in, x_in, available_width, minimum_width, LayoutCoord(0));
		available_width += horizontal_margins;
	}

	return available_width;
}

/** Recalculate top margins after new block box has been added. See declaration in header file for more information. */

/* virtual */ BOOL
BlockBox::RecalculateTopMargins(LayoutInfo& info, const VerticalMargin* top_margin, BOOL has_bottom_margin)
{
	LayoutProperties* cascade = GetCascade();
	Container* container = cascade->container;

	// If block box has containing box...

	if (container)
		// Propagate margin change to containing box

		return container->RecalculateTopMargins(info, top_margin, has_bottom_margin);
	else
	{
		LayoutCoord offset = -y;

		SetY(top_margin ? top_margin->GetHeight() : LayoutCoord(0));

		offset += y;

		if (offset)
			info.Translate(LayoutCoord(0), offset);

		return offset != 0;
	}
}

static inline BOOL
HasBorder(const BorderEdge &be)
{
	return be.style != BORDER_STYLE_NOT_SET && be.style != CSS_VALUE_none && be.width > 0 && be.color != CSS_COLOR_transparent;
}

/** Helper method for painting background and borders. */

BackgroundsAndBorders::BackgroundsAndBorders(FramesDocument *d, VisualDevice *vd, LayoutProperties *cascade, const Border *b) :
	doc(d),
	vis_dev(vd),
	expand_image_position(FALSE),
	inline_virtual_width(0),
	inline_virtual_position(0),
	extra_top_margin(0),
	top_gap_x(0),
	top_gap_width(0),
	has_positioning_area_override(FALSE),
	scroll_x(0),
	scroll_y(0),
	border_area_covered_by_background(FALSE),
	css_length_resolver(vd, FALSE, 0.0f, LayoutFixed(0), 0, d->RootFontSize())
{
	if (b)
		computed_border = *b;
	else
		computed_border.SetDefault();

	border_image.Reset();

	if (cascade)
		UseCascadedProperties(cascade);
	else
	{
		font_color = USE_DEFAULT_COLOR;
		padding_top = padding_left = padding_right = padding_bottom = LayoutCoord(0);
		box_decoration_break = CSS_VALUE_slice;
		image_rendering = CSS_VALUE_auto;
	}
}

BOOL
BackgroundsAndBorders::SkipBackground(HTML_Element *element)
{
	BOOL skip_bg = FALSE;

#ifdef _PRINT_SUPPORT_
	skip_bg = (doc->GetWindow()->GetPreviewMode() || vis_dev->IsPrinter()) && !g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrintBackground);
#endif // _PRINT_SUPPORT_

#ifdef SVG_SUPPORT
	if (element && element->Type() == Markup::HTE_DOC_ROOT && doc->IsInlineFrame())
	{
		// SVG in an iframe must not paint its background since that
		// would obscure the parent doc and SVGs can be transparent.
		HTML_Element* root = doc->GetLogicalDocument()->GetDocRoot();
		if(root && root->GetNsType() == NS_SVG && root->Type() == Markup::SVGE_SVG)
			skip_bg = TRUE;
	}
#endif // SVG_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
	/* A background for a cue should not apply to the cue root, but to an
	   anonymous 'background box' that is a (sole) child of the cue root. */

	if (element && element->IsMatchingType(Markup::CUEE_ROOT, NS_CUE))
		skip_bg = TRUE;
#endif // MEDIA_HTML_SUPPORT

	return skip_bg;
}

BOOL
BackgroundsAndBorders::SkipBackgroundColor(const BG_OUT_INFO &bg_info, const BgImages &bg_images, HTML_Element *element) const
{
	if (bg_images.IsEmpty() && element->Type() == Markup::HTE_INPUT)
		if (Content* content = element->GetLayoutBox()->GetContent())
			/* Only consider skipping background color if this is a widget. Never skip
			   background for images or INPUT elements that have been turned into
			   non-replaced content by using the 'content' property. */

			if (content->IsReplaced() && ((ReplacedContent*) content)->IsForm())
			{
				BOOL has_border = border_image.HasBorderImage() ||
					bg_info.has_border_bottom || bg_info.has_border_right ||
					bg_info.has_border_top || bg_info.has_border_left ||
					bg_info.border->top.HasRadius() || bg_info.border->bottom.HasRadius() ||
					bg_info.border->left.HasRadius() || bg_info.border->right.HasRadius();

				/* Skip background color for input elements unless layout
				   draws the border or inset box shadow. */

				return !has_border && !bg_info.has_inset_box_shadow;
			}

	return FALSE;
}

void
BackgroundsAndBorders::PaintBackgroundColor(COLORREF bg_color,
											const BgImages &bg_images,
											BG_OUT_INFO &bg_info,
											const OpRect &border_box)
{
	if (bg_color != USE_DEFAULT_COLOR)
	{
		OpRect draw_rect = border_box;
		BackgroundProperties bprops;

		/* Find last layer to find out background properties for that layer */
		bg_images.GetLayer(css_length_resolver, bg_images.GetLayerCount() - 1, bprops);
		ComputeDrawingArea(border_box, &bprops, bg_info.border, draw_rect);

		bg_info.background = &bprops;
		vis_dev->BgColorOut(&bg_info, draw_rect, border_box, bg_color);
		bg_info.background = NULL;

		if (bprops.bg_clip == CSS_VALUE_border_box)
			border_area_covered_by_background = TRUE;
	}
}

#ifdef CSS_GRADIENT_SUPPORT
void
BackgroundsAndBorders::PaintBackgroundGradient(HTML_Element *element,
                                               const BG_OUT_INFO &bg_info,
                                               COLORREF current_color,
                                               const OpRect &border_box,
                                               const CSS_Gradient &gradient)
{
	OpRect bpa; // background positioning area

	// background-origin
	ComputeBackgroundOrigin(border_box, bg_info.background, bg_info.border, /* out */ bpa);

	// background-attachment
	HandleBackgroundAttachment(bg_info.background, /* out */ bpa);

	// If b. p. a. is empty (e.g. an empty document), don't paint anything.
	if (bpa.IsEmpty())
		return;

	// background-clip
	OpRect clip_rect;
	ComputeDrawingArea(border_box,
					   bg_info.background,
					   bg_info.border,
					   clip_rect);

	// Mark fixed background
	if (bg_info.background->bg_attach == CSS_VALUE_fixed)
	{
		AffinePos doc_pos = vis_dev->GetCTM();
		doc_pos.AppendTranslation(bpa.x, bpa.y);

		OpRect fixed_rect = clip_rect;
		doc_pos.Apply(fixed_rect);

		doc->GetLogicalDocument()->GetLayoutWorkplace()->AddFixedContent(fixed_rect);
	}

	// background-size
	// A gradient is infinite, but for scaling purposes the dimensions of the background-origin are used,

	OpRect gradient_rect(bpa);

	double img_scale_x = 100,
		img_scale_y = 100;
	AdjustScaleForRenderingMode(element, img_scale_x, img_scale_y);
	ComputeImageScale(bg_info.background, bpa, gradient_rect.width, gradient_rect.height, img_scale_x, img_scale_y);
	if (img_scale_x <= 0 || img_scale_y <= 0)
		/* If background-size resolves to 0, or if the calculation in ComputeImageScale
		   has overflowed, skip painting. */
		return;

	gradient_rect.width = INT32(gradient_rect.width * img_scale_x / 100);
	gradient_rect.height = INT32(gradient_rect.height * img_scale_y / 100);

	// If background-size is set to a length and background-repeat is not 'round', use the background-size length.
	// This is a workaround for low precision (see CT-374)
	const BackgroundProperties &bg = *bg_info.background;
	if (bg.bg_xsize != BG_SIZE_X_AUTO && bg.bg_repeat_x != CSS_VALUE_round && !bg.packed.bg_xsize_per)
		gradient_rect.width = bg.bg_xsize;
	if (bg.bg_ysize != BG_SIZE_Y_AUTO && bg.bg_repeat_y != CSS_VALUE_round && !bg.packed.bg_ysize_per)
		gradient_rect.height = bg.bg_ysize;

	if (gradient_rect.width == 0 || gradient_rect.height == 0)
		// A 0-sized background-image should not be displayed (see 'background-size' spec)
		return;
	// End of workaround.

	// background-position
	OpPoint bg_pos;
	ComputeImagePosition(bg_info.background, bpa,
						 gradient_rect.width, gradient_rect.height,
						 100, 100, // already scaled
						 bg_pos);

	// background-repeat
	// (except 'round', already handled in ComputeImageScale)
	ClipDrawingArea(clip_rect,
					bg_pos,
					TRUE,
					bg_info.background,
					bg_info.border,
					gradient_rect.width, gradient_rect.height,
					100, 100, // already scaled
					clip_rect);

	OpRect space;
	CalculateRepeatSpacing(bg_info.background, bpa, /* out */ bg_pos, gradient_rect, space);
	gradient_rect.x = bg_pos.x; gradient_rect.y = bg_pos.y;

	vis_dev->BgGradientOut(&bg_info, current_color, border_box, clip_rect, gradient_rect, gradient, space);
}
#endif // CSS_GRADIENT_SUPPORT

void
BackgroundsAndBorders::HandleBoxShadows(const Shadows *shadows, const OpRect &border_box,
										BG_OUT_INFO *bg_info, BOOL with_inset)
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if (!shadows || !shadows->Get())
		return;

	const OpRect padding_box = GetPaddingBox(bg_info->border, border_box);

	Shadow box_shadow;

	const COLORREF& fallback_color = font_color;
	Shadows::Iterator iter(*shadows);
	iter.MoveToEnd();

	while (iter.GetPrev(css_length_resolver, box_shadow))
	{
		if (box_shadow.inset == with_inset)
			vis_dev->PaintBoxShadow(box_shadow, border_box, padding_box, fallback_color, bg_info->border);
		if (box_shadow.inset)
			bg_info->has_inset_box_shadow = TRUE;
	}
#endif // VEGA_OPPAINTER_SUPPORT
}

/* static */ void
BackgroundsAndBorders::ComputeImageScale(const BackgroundProperties *background,
										 const OpRect &positioning_rect,
										 int img_width, int img_height,
										 double &img_scale_x, double &img_scale_y)
{
	OP_ASSERT(img_width != 0 && img_height != 0);

	if (background->packed.bg_size_contain || background->packed.bg_size_cover)
	{
		double scale_factor;
		if (background->packed.bg_size_contain)
			scale_factor = MIN(positioning_rect.width * 100.0 / img_width, positioning_rect.height * 100.0 / img_height);
		else
			scale_factor = MAX(positioning_rect.width * 100.0 / img_width, positioning_rect.height * 100.0 / img_height);

		img_scale_x = img_scale_y = scale_factor;
	}
	else
	{
		if (background->bg_xsize != BG_SIZE_X_AUTO)
		{
			if (background->packed.bg_xsize_per)
				img_scale_x = (background->bg_xsize * static_cast<double>(positioning_rect.width)) / (LAYOUT_FIXED_HUNDRED_PERCENT * static_cast<double>(img_width) / img_scale_x);
			else
				img_scale_x = (img_scale_x * background->bg_xsize) / img_width;

			if (background->bg_ysize == BG_SIZE_Y_AUTO)
				img_scale_y = img_scale_x;

			// Workaround for rounding when near 0
			if (img_scale_x == 0 && background->bg_xsize != 0)
				img_scale_x = 1;
		}

		if (background->bg_ysize != BG_SIZE_Y_AUTO)
		{
			if (background->packed.bg_ysize_per)
				img_scale_y = (background->bg_ysize * static_cast<double>(positioning_rect.height)) / (LAYOUT_FIXED_HUNDRED_PERCENT * static_cast<double>(img_height) / img_scale_y);
			else
				img_scale_y = (img_scale_y * background->bg_ysize) / img_height;

			if (background->bg_xsize == BG_SIZE_X_AUTO)
				img_scale_x = img_scale_y;

			// Workaround for rounding when near 0
			if (img_scale_y == 0 && background->bg_ysize != 0)
				img_scale_y = 1;
		}
	}

	/* "If X != 0 is the width of the image after step one and
	    W is the width of the background positioning area,
		then the rounded width X' = W / round(W / X) where
		round() is a function that returns the nearest natural
		number (integer greater than zero). */
	if (img_scale_x != 0 && img_scale_y != 0)
	{
		if (background->bg_repeat_x == CSS_VALUE_round)
			if (op_fmod(positioning_rect.width, (img_width * img_scale_x / 100)) != 0)
			{
				int rounded = (int) (0.5 + ((double) positioning_rect.width * 100 / (img_width * img_scale_x)));
				double new_width = positioning_rect.width / (rounded > 0 ? rounded : 1);

				img_scale_x = new_width * 100 / img_width;
			}

		if (background->bg_repeat_y == CSS_VALUE_round)
			if (op_fmod(positioning_rect.height, (img_height * img_scale_y / 100)) != 0)
			{
				int rounded = (int) (0.5 + ((double) positioning_rect.height * 100 / (img_height * img_scale_y)));
				double new_height = positioning_rect.height / (rounded > 0 ? rounded : 1);

				img_scale_y = new_height * 100 / img_height;
			}
	}

	/* "If 'background-repeat' is 'round' for one direction only
	   and if 'background-size' is 'auto' for the other direction,
	   then there is a third step: that other direction is scaled
	   so that the original aspect ratio is restored." */

	if (background->bg_ysize == BG_SIZE_Y_AUTO && background->bg_repeat_x == CSS_VALUE_round && background->bg_repeat_y != CSS_VALUE_round)
		img_scale_y = img_scale_x;

	if (background->bg_xsize == BG_SIZE_X_AUTO && background->bg_repeat_y == CSS_VALUE_round && background->bg_repeat_x != CSS_VALUE_round)
		img_scale_x = img_scale_y;
}

void
BackgroundsAndBorders::HandleBackgroundAttachment(const BackgroundProperties *background, OpRect &positioning_rect)
{
	if (background->bg_attach == CSS_VALUE_fixed)
	{
		positioning_rect = doc->GetLogicalDocument()->GetLayoutWorkplace()->GetLayoutViewport();

		OpPoint p = vis_dev->ToDocument(OpPoint());
		positioning_rect.x -= p.x;
		positioning_rect.y -= p.y;
	}
	else
		if (background->bg_attach == CSS_VALUE_local)
		{
			positioning_rect.x -= scroll_x;
			positioning_rect.y -= scroll_y;
		}
}

/* static */ void
BackgroundsAndBorders::ComputeImagePosition(const BackgroundProperties *background,
											const OpRect &positioning_rect,
											int img_width, int img_height,
											int img_scale_x, int img_scale_y, OpPoint &background_pos)
{
	/* Calculate initial position of background image */

	background_pos.x = positioning_rect.x;
	background_pos.y = positioning_rect.y;

	int scaled_image_width = (img_width*img_scale_x)/100;
	int scaled_image_height = (img_height*img_scale_y)/100;

	if (background->bg_pos_xref == CSS_VALUE_left)
	{
		if (background->packed.bg_xpos_per)
			background_pos.x += LayoutFixedPercentageOfInt(LayoutFixed(background->bg_xpos), positioning_rect.width - scaled_image_width);
		else
			background_pos.x += background->bg_xpos;
	}
	else
	{
		OP_ASSERT(background->bg_pos_xref == CSS_VALUE_right);

		if (background->packed.bg_xpos_per)
			background_pos.x += (positioning_rect.width - scaled_image_width) - LayoutFixedPercentageOfInt(LayoutFixed(background->bg_xpos), positioning_rect.width - scaled_image_width);
		else
			background_pos.x += positioning_rect.width - scaled_image_width - background->bg_xpos;
	}

	if (background->bg_pos_yref == CSS_VALUE_top)
	{
		if (background->packed.bg_ypos_per)
			background_pos.y += LayoutFixedPercentageOfInt(LayoutFixed(background->bg_ypos), positioning_rect.height - scaled_image_height);
		else
			background_pos.y += background->bg_ypos;
	}
	else
	{
		OP_ASSERT(background->bg_pos_yref == CSS_VALUE_bottom);

		if (background->packed.bg_ypos_per)
			background_pos.y += (positioning_rect.height - scaled_image_height) - LayoutFixedPercentageOfInt(LayoutFixed(background->bg_ypos), positioning_rect.height - scaled_image_height);
		else
			background_pos.y += positioning_rect.height - scaled_image_height - background->bg_ypos;
	}
}

/* static */ OpRect
BackgroundsAndBorders::GetPaddingBox(const Border *border, OpRect border_box)
{
	OpRect padding_box = border_box;;

	if (border->top.width != BORDER_WIDTH_NOT_SET)
	{
		padding_box.x += border->left.width;
		padding_box.y += border->top.width;
		padding_box.width -= (border->right.width + border->left.width);
		padding_box.height -= (border->bottom.width + border->top.width);
	}

	return padding_box;
}

void
BackgroundsAndBorders::ComputeDrawingArea(const OpRect &border_box,
										  const BackgroundProperties *background,
										  const Border *border,
										  OpRect &clip_rect)
{
	/* Initially assume that we're going to draw to the area as defined by background-clip, which
	   may be content, padding, and (any transparent parts of) border. */

	clip_rect = border_box;

	if (background->bg_clip == CSS_VALUE_padding_box || background->bg_clip == CSS_VALUE_content_box)

		/* Remove border ... */
		clip_rect = GetPaddingBox(border, clip_rect);

	if (background->bg_clip == CSS_VALUE_content_box)
	{
		/* Remove padding ... */
		clip_rect.x += padding_left;
		clip_rect.y += padding_top;
		clip_rect.width -= padding_left + padding_right;
		clip_rect.height -= padding_top + padding_bottom;
	}
}

void
BackgroundsAndBorders::ClipDrawingArea(const OpRect &clip_rect,
									   const OpPoint &background_pos,
									   BOOL image_size_known,
									   const BackgroundProperties *background,
									   const Border *border,
									   int img_width, int img_height,
									   double img_scale_x, double img_scale_y,
									   OpRect &draw_rect)
{
	long draw_x_min = clip_rect.x;
	long draw_x_max = clip_rect.x + clip_rect.width;
	long draw_y_min = clip_rect.y;
	long draw_y_max = clip_rect.y + clip_rect.height;

	/* But if the image is not repeated, clip that box rectangle against the image's actual position
	   (taking background-position into account) - but not if image size is not known yet. Then we
	   are not sure where it will end up in the box and must go for the entire box so it is
	   repainted correctly when decoded. */

	if (image_size_known)
	{
		if (background->bg_repeat_x == CSS_VALUE_no_repeat)
		{
			if (draw_x_min < background_pos.x)
				draw_x_min = background_pos.x;

			long tmp_max = background_pos.x + long(img_width * img_scale_x / 100);
			if (draw_x_max > tmp_max)
				draw_x_max = tmp_max;
		}

		if (background->bg_repeat_y == CSS_VALUE_no_repeat)
		{
			if (draw_y_min < background_pos.y)
				draw_y_min = background_pos.y;

			long tmp_max = background_pos.y + long(img_height * img_scale_y / 100);
			if (draw_y_max > tmp_max)
				draw_y_max = tmp_max;
		}
	}

	/* Avoid negative widths  */

	long draw_width  = draw_x_max - draw_x_min;
	if (draw_width < 0) draw_width = 0;

	long draw_height = draw_y_max - draw_y_min;
	if (draw_height < 0) draw_height = 0;

	draw_rect.Set(draw_x_min, draw_y_min, draw_width, draw_height);
}

#ifdef SVG_SUPPORT
BOOL
BackgroundsAndBorders::GetSVGImage(URL bg_url, UrlImageContentProvider *conprov, const OpRect &border_box, Image &img, BOOL &invalidate_old_position, int &override_doc_width, int &override_doc_height)
{
	if (!conprov)
		conprov = UrlImageContentProvider::FindImageContentProvider(bg_url);

	if (!conprov)
		return FALSE;

	if (!conprov->GetSVGImageRef())
	{
		conprov->UpdateSVGImageRef(doc->GetLogicalDocument());

		if (!conprov->GetSVGImageRef())
			return FALSE;
	}

	SVGImage* svgimage = conprov->GetSVGImageRef()->GetSVGImage();
	if (!svgimage)
		return FALSE;

	int containing_width = border_box.width;
	int containing_height = border_box.height;
	int desired_width = containing_width;
	int desired_height = containing_height;

	/* FIXME: SVG would want to use the cascade to find out the em-base. Sadly, we don't always have
	   the corresponding cascade when painting backgrounds. Sending NULL makes it fall back to ask
	   the VisualDevice. */

	if (OpStatus::IsSuccess(svgimage->GetResolvedSize(vis_dev, NULL,
													  containing_width, containing_height,
													  desired_width, desired_height)))
	{
		override_doc_width = desired_width;
		override_doc_height = desired_height;

		/* Rescale to screen size to avoid ugly bitmap scaling */

		desired_width = vis_dev->ScaleToScreen(desired_width);
		desired_height = vis_dev->ScaleToScreen(desired_height);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		const PixelScaler& scaler = vis_dev->GetVPScale();
		desired_width = TO_DEVICE_PIXEL(scaler, desired_width);
		desired_height = TO_DEVICE_PIXEL(scaler, desired_height);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		if (img.IsFailed() ||
			img.Width() != (unsigned int) desired_width ||
			img.Height() != (unsigned int) desired_height)
		{
			OpBitmap* res;

			if (OpStatus::IsSuccess(svgimage->PaintToBuffer(res, 0, desired_width, desired_height)))
			{
				invalidate_old_position = TRUE;
				if(OpStatus::IsError(img.ReplaceBitmap(res)))
				{
					OP_DELETE(res);
					invalidate_old_position = FALSE; // use the old image
				}
			}
		}

		return TRUE;
	}
	else
		return FALSE;

}
#endif // SVG_SUPPORT

/* Wrapper method getting the HEListElm for a background or border image.

   For background images, the HEListElm is stored in two places. The
   first, normal, background image is stored as inline type ==
   BGIMAGE_INLINE.  The second up to n:th background image is stored
   in inline_Type == EXTRA_BGIMAGE_INLINE. */

static HEListElm *GetHEListElmForInline(FramesDocument *doc, URL base_url, HTML_Element *element, InlineResourceType inline_type, const CssURL &bg_img)
{
	HEListElm *helm = NULL;

	if (inline_type == EXTRA_BGIMAGE_INLINE)
	{
		URL bg_url = g_url_api->GetURL(base_url, bg_img);

		HTML_Element::BgImageIterator iter(element);
		HEListElm *iter_elm = iter.Next();
		while (iter_elm)
		{
			if (UrlImageContentProvider *prov = iter_elm->GetUrlContentProvider())
			{
				URL iter_bg_url = *prov->GetUrl();
				if (iter_bg_url == bg_url)
					return iter_elm;
			}

			iter_elm = iter.Next();
		}
	}
	else
		helm = element->GetHEListElmForInline(inline_type);

	return helm;
}

BOOL
BackgroundsAndBorders::GetImageInformation(HTML_Element *element, const CssURL &bg_img, const OpRect &border_box,
										   InlineResourceType inline_type, int bg_idx, ImageInfo &image_info)
{
	/* Each CssURL is resolved during style parsing and should be
	   absolute URLs. Therefore we should ideally use the empty URL as
	   base. Some information, like context id, doesn't survive
	   stringification so we use the document base url for the extra
	   information. There is also the exception for bgimage
	   attributes. Those CssURLs are not resolved to absolute URLs
	   during parsing for performance reasons. */

	URL base_url;

	if (doc->GetHLDocProfile()->BaseURL())
		base_url = *doc->GetHLDocProfile()->BaseURL();
	else
		base_url = doc->GetURL();

	URL bg_url;

	if (bg_idx == 0)

		/* Just get the first bginline */
		image_info.helm = element->GetHEListElmForInline(inline_type);
	else
	{
		inline_type = EXTRA_BGIMAGE_INLINE;
		image_info.helm = GetHEListElmForInline(doc, base_url, element, inline_type, bg_img);

		if (image_info.helm == NULL)

			/* For multiple backgrounds we need to reload all extra background images */
			return FALSE;
	}

	UrlImageContentProvider* conprov = NULL;

	if (image_info.helm && image_info.helm->GetUrlContentProvider())
	{
		/* Faster than getting the url via g_url_api->GetURL since it's
		   already stored in UrlImageContentProvider. */

		conprov = image_info.helm->GetUrlContentProvider();
		bg_url = *conprov->GetUrl();

		/* We can't trust that the bg_img is matching the url got from the
		   provider (script may have changed it)

		   Check if it matches the url object. If not, get the Url the
		   slow way. */

		OpStringC bg_url_str = bg_url.GetAttribute(URL::KUniName, FALSE);

		if (bg_url_str.CStr() && uni_strcmp(bg_url_str.CStr(), bg_img))
			conprov = NULL;
	}

	if (!conprov)
		bg_url = g_url_api->GetURL(base_url, bg_img);

	image_info.replaced_url = FALSE;

	if (!bg_url.IsEmpty())
	{
		if (conprov)
			image_info.img = conprov->GetImage();
		else
			image_info.img = UrlImageContentProvider::GetImageFromUrl(bg_url);

		URLStatus status = bg_url.Status(TRUE);

		if (!image_info.helm || !image_info.helm->GetLoadInlineElm() || !(*image_info.helm->GetLoadInlineElm()->GetUrl() == bg_url))
		{
			/* In some cases background images are not loaded until they need to be painted as
			   background (:first-line).  If url already is loaded by another document it still
			   needs to be registered by this. */

			if (bg_idx > 0)
				return FALSE;
			else
			{
				doc->LoadInline(&bg_url, element, inline_type, LoadInlineOptions().FromUserCSS(bg_img.IsUserDefined()));
				status = bg_url.Status(TRUE);
				image_info.helm = GetHEListElmForInline(doc, base_url, element, inline_type, bg_img);
			}
		}

		if (!image_info.helm)
			return FALSE;

		URL old_url = image_info.helm->GetOldUrl();

		if (status == URL_LOADED && !old_url.IsEmpty())
			image_info.replaced_url = TRUE;

		if (status == URL_LOADED || image_info.img.ImageDecoded())
		{
#ifdef SVG_SUPPORT
			if (bg_url.ContentType() == URL_SVG_CONTENT)
			{
				if (GetSVGImage(bg_url, conprov, border_box, image_info.img,
								image_info.invalidate_old_position,
								image_info.override_img_width, image_info.override_img_height))
					image_info.is_svg = TRUE;
			}
			else
#endif
			{
				image_info.invalidate_old_position = image_info.helm->UpdateImageUrlIfNeeded(&bg_url);
				image_info.img = image_info.helm->GetImage(); // The new image is ready. Use it.
			}
		}
		else
		{
			if (!old_url.IsEmpty())
				// The new image isn't loaded yet. Using the old one is a good idea.

				image_info.img = UrlImageContentProvider::GetImageFromUrl(old_url);
		}
	}

	return TRUE;
}

void
BackgroundsAndBorders::AdjustScaleForRenderingMode(HTML_Element *element, double &img_scale_x, double &img_scale_y)
{
	PrefsCollectionDisplay::RenderingModes rendering_mode = PrefsCollectionDisplay::MSR;

	/* FIXME: Do we have this look-up somewhere else? */
	switch (doc->GetLayoutMode())
	{
	case LAYOUT_SSR:
		rendering_mode = PrefsCollectionDisplay::SSR;
		break;
	case LAYOUT_CSSR:
		rendering_mode = PrefsCollectionDisplay::CSSR;
		break;
	case LAYOUT_AMSR:
		rendering_mode = PrefsCollectionDisplay::AMSR;
		break;
#ifdef TV_RENDERING
	case LAYOUT_TVR:
		rendering_mode = PrefsCollectionDisplay::TVR;
		break;
#endif // TV_RENDERING
	}

	if (doc->GetLayoutMode() != LAYOUT_NORMAL && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(rendering_mode, PrefsCollectionDisplay::ShowBackgroundImages)) == 1)
		img_scale_x = img_scale_y = element->GetLayoutBox()->GetPercentOfNormalWidth();
}

void
BackgroundsAndBorders::ComputeBackgroundOrigin(const OpRect &border_box, const BackgroundProperties* background, const Border *border,
											   OpRect &positioning_rect) const
{
	positioning_rect = border_box;

	if (background->bg_origin == CSS_VALUE_padding_box ||
		background->bg_origin == CSS_VALUE_content_box)
	{
		/* Remove border from border-box to get padding-box */

		positioning_rect = GetPaddingBox(border, positioning_rect);

		if (background->bg_origin == CSS_VALUE_content_box)
		{
			/* Remove border from padding-box to get content-box */

			positioning_rect.x += padding_left;
			positioning_rect.y += padding_top;
			positioning_rect.width -= padding_left + padding_right;
			positioning_rect.height -= padding_top + padding_bottom;
		}
	}

	if (box_decoration_break == CSS_VALUE_slice && inline_virtual_width && background->bg_attach != CSS_VALUE_fixed)
	{
		long virtual_width = inline_virtual_width;

		if (background->bg_origin == CSS_VALUE_padding_box ||
			background->bg_origin == CSS_VALUE_content_box)
		{
			/* Remove border from border-box to get padding-box */
			virtual_width -= (border->right.width + border->left.width);
			if (background->bg_origin == CSS_VALUE_content_box)
				virtual_width -= padding_left + padding_right;
		}

		positioning_rect.width = virtual_width;
		positioning_rect.x -= inline_virtual_position;
	}
	else
		if (has_positioning_area_override)
			/* FIXME: Should this rect be subject to background-origin
			   also? If so, we must make sure we have the right props to
			   do that. */

			positioning_rect = override_positioning_rect;
}

/* static */
BOOL BackgroundsAndBorders::CalculateRepeatSpacing(const BackgroundProperties *background, const OpRect &positioning, OpPoint &background_pos, const OpRect &img, OpRect &space)
{
	if (background->bg_repeat_x == CSS_VALUE_space)
	{
		OP_ASSERT(img.width != 0);

		int horizontal_spaces = (positioning.width / img.width) - 1;
		if (horizontal_spaces > 0)
		{
			space.width = (positioning.width % img.width) / horizontal_spaces;
			background_pos.x = positioning.x;
		}
		else if (horizontal_spaces == 0)
		{
			/* There is space for only one image, use
			   background-position in the x direction. */

			space.width = img.width;
		}
		else
		{
			/* There is not enough space for even one image. Don't draw. */
			return FALSE;
		}
	}

	if (background->bg_repeat_y == CSS_VALUE_space)
	{
		OP_ASSERT(img.height != 0);

		int vertical_spaces = (positioning.height / img.height) - 1;
		if (vertical_spaces > 0)
		{
			space.height = (positioning.height % img.height) / vertical_spaces;
			background_pos.y = positioning.y;
		}
		else if (vertical_spaces == 0)
		{
			/* There is space for only one image, use
			   background-position in the y direction. */

			space.height = img.height;
		}
		else
		{
			/* There is not enough space for even one image. Don't draw. */
			return FALSE;
		}
	}
	return TRUE;
}


void
BackgroundsAndBorders::SetDocumentPosition(HTML_Element* element, HEListElm *helm, const OpRect &image_rect) const
{
	AffinePos doc_pos = vis_dev->GetCTM();
	doc_pos.AppendTranslation(image_rect.x, image_rect.y);

	doc->SetImagePosition(element, doc_pos, image_rect.width, image_rect.height, TRUE, helm, expand_image_position);
}

void
BackgroundsAndBorders::PaintImage(Image img, HTML_Element *element, HEListElm *helm, BG_OUT_INFO &bg_info,
								  const OpRect &border_box, BOOL is_svg, int override_img_width, int override_img_height,
								  BOOL replaced_url)
{
	BOOL image_size_known = TRUE;

	int img_width = (is_svg && override_img_width > 0) ? override_img_width : img.Width();
	int img_height = (is_svg && override_img_height > 0) ? override_img_height : img.Height();

	double img_scale_x = 100;
	double img_scale_y = 100;

	OpRect positioning_rect;
	ComputeBackgroundOrigin(border_box, bg_info.background, bg_info.border, positioning_rect);

	OpPoint background_pos;
	if (!img_width || !img_height)
	{
		/* We don't know the image size yet but need an update
		   of whole background area when image is loaded. */

		img_width = border_box.width;
		img_height = border_box.height;
		image_size_known = FALSE;
	}
	else
	{
		/* Set scale using rendering mode */

		AdjustScaleForRenderingMode(element, img_scale_x, img_scale_y);

		/* Honour background-attach */

		HandleBackgroundAttachment(bg_info.background, positioning_rect);

		/* Honour background-size.

		   The background-size is relative to the background positioning
		   area, the virtual rect. */

		ComputeImageScale(bg_info.background, positioning_rect, img_width, img_height, img_scale_x, img_scale_y);
		if (img_scale_x <= 0 || img_scale_y <= 0)
			// If background-size resolves to 0, or if the calculation in ComputeImageScale
			// has overflowed, skip painting.
			return;

		/* Honour background-position */

		ComputeImagePosition(bg_info.background, positioning_rect,
							 img_width, img_height,
							 int(img_scale_x), int(img_scale_y),
							 background_pos);
	}

	if (!img_width || !img_height)
		return;

	int scaled_img_width  = int(img_width  * img_scale_x / 100),
		scaled_img_height = int(img_height * img_scale_y / 100);

	/* background-clip */

	OpRect draw_rect;
	ComputeDrawingArea(border_box,
					   bg_info.background,
					   bg_info.border,
					   draw_rect);

	/* Honour background-repeat (except 'round', handled in the
	   background-size handling). */

	ClipDrawingArea(draw_rect,
					background_pos,
					image_size_known,
					bg_info.background,
					bg_info.border,
					img_width, img_height,
					img_scale_x, img_scale_y,
					draw_rect);

	OpRect space;
	if (!CalculateRepeatSpacing(bg_info.background,
		positioning_rect, background_pos,
		OpRect(0, 0, scaled_img_width, scaled_img_height), space))
		return; // There is not enough space for even one image. Don't draw.

	/* Now we can tell the document that the image is painted and where */

	SetDocumentPosition(element, helm, draw_rect);

	/* Decode all imagedata in the new image now so we won't get a
	   glitch when replacing images.  If turbo mode is ON, always
	   decode image now to avoid flicker update */

	if (replaced_url)
		helm->LoadAll();

	if (bg_info.background->bg_attach == CSS_VALUE_fixed)
	{
		/* Add fixed background. */

		LayoutWorkplace* workplace = doc->GetLogicalDocument()->GetLayoutWorkplace();
		workplace->AddFixedContent(helm->GetImageBBox());
	}

	if (!image_size_known && img.ImageDecoded() && img.Width() && img.Height())
	{
		/* The image was decoded by the doc code when the image
		   position in the document was set. We can now compute the
		   correct image position. */

		PaintImage(img, element, helm, bg_info,
				   border_box, is_svg, override_img_width, override_img_height,
				   replaced_url);
		return;
	}

	/* FIXME: The background image was changed, so we invalidate the
	   new area to be sure it is repainted.  We may have got a new
	   area now e.g. by hover changing the repeat-property, and the
	   area won't be updated automatically if the image was already
	   decoded. */

	if ((img.GetLastDecodedLine() == img.Height() && (img.ImageDecoded() || (img.IsFailed() && img.Height() > 0)) && helm)
#ifdef HAS_ATVEF_SUPPORT
								|| img.IsAtvefImage()
#endif // HAS_ATVEF_SUPPORT
		)
	{
		/* Compute the offset of the background drawing area (as
		   defined by background-clip) compared to the background
		   positioning area (as defined by background-position). */

		OpPoint offset(draw_rect.x - background_pos.x,
					   draw_rect.y - background_pos.y);

		OpRect cover_rect = border_box;

#ifdef SVG_SUPPORT
		VDStateNoScale state_noscale;

		if (is_svg)
		{
			cover_rect = vis_dev->ToBBox(cover_rect);
			cover_rect = vis_dev->ScaleToScreen(cover_rect);

			state_noscale = vis_dev->BeginNoScalePainting(draw_rect);
			draw_rect = state_noscale.dst_rect;

			offset.x = offset.x * state_noscale.old_scale / 100;
			offset.y = offset.y * state_noscale.old_scale / 100;

			img_scale_x = img_scale_x * state_noscale.old_scale / 100;
			img_scale_y = img_scale_y * state_noscale.old_scale / 100;
		}
#endif // SVG_SUPPORT

		OP_ASSERT(draw_rect.width >= 0);
		OP_ASSERT(draw_rect.height >= 0);
		OP_ASSERT(bg_info.border != NULL);

		if (!draw_rect.IsEmpty())
		{
			vis_dev->BgImgOut(&bg_info, cover_rect, img, draw_rect, offset, helm, img_scale_x, img_scale_y, space.width, space.height);

			/* The following is not exactly the truth, since
			  background-repeat and background-clip may have shrunk
			  the area we draw.  However, the visual device has
			  special code for detecting this though (end of BgImgOut)
			  so that the 'avoid overdraw' optimization is handled
			  correct in those cases (we enter slow-case) */

			border_area_covered_by_background = TRUE;
		}

		/* Make into a helper function */
#ifdef SVG_SUPPORT
		if (is_svg)
			vis_dev->EndNoScalePainting(state_noscale);
#endif // SVG_SUPPORT

	}
}

BOOL
BackgroundsAndBorders::HandleSkinElement(const OpRect &border_box, const CssURL &bg_img)
{
#ifdef SKIN_SUPPORT
	if (bg_img[0] == 's' && bg_img[1] == ':')
	{
#ifdef DISPLAY_AVOID_OVERDRAW
		// Cover area but keep the hole! We need to flush it immediately.
		vis_dev->CoverBackground(border_box, TRUE, TRUE);
		vis_dev->FlushBackgrounds(border_box);
#endif
		char name8[120]; /* ARRAY OK 2008-02-05 mstensho */
		uni_cstrlcpy(name8, bg_img + 2, ARRAY_SIZE(name8));
		g_skin_manager->DrawElement(vis_dev, name8, border_box);
		return TRUE;
	}
#endif

	return FALSE;
}

void
BackgroundsAndBorders::PaintBackgroundImages(const BgImages& bg_images, BG_OUT_INFO &bg_info,
											 COLORREF current_color, const OpRect &border_box,
											 HTML_Element *element)
{
	if (bg_images.IsEmpty())
		return;

	BackgroundProperties bprops;
	const int background_layer_count = bg_images.GetLayerCount();

	for (int i = background_layer_count - 1; i >= 0; i--)
	{
		bg_images.GetLayer(css_length_resolver, i, bprops);

		if (bprops.bg_img)
		{
			if (HandleSkinElement(border_box, bprops.bg_img))
				continue;

			ImageInfo image_info;

			if (!GetImageInformation(element, bprops.bg_img, border_box, BGIMAGE_INLINE, bprops.bg_idx, image_info))
			{
				/* When image information isn't available, we call
				   LoadInline on all the extra background images and try
				   again.

				   The problem is that when the background property is
				   changed (e.g. by script), we redraw the area of the
				   background image, so we doesn't notice that we haven't
				   loaded the image(s) until we are about to paint them.

				   It is in the case of missing extra background images,
				   we end up here.  The first, "normal", background is
				   handled in GetImageInformation().

				   If the images are fetched from the network (not in
				   cache), the next GetImageInformation give us the old
				   background image.  But when the images are indeed
				   loaded, the area will be invalidated again and we will
				   finally paint the correct background image.

				   In order to fix this we would need a flag in
				   HTML_Element (or similar) that tells us to reload
				   inline resources on that element and we need to do that
				   prior to paint, during a reflow triggered by a change
				   of the background property. */

				bg_images.LoadImages(doc->GetMediaType(), doc, element, TRUE);
				GetImageInformation(element, bprops.bg_img, border_box, BGIMAGE_INLINE, bprops.bg_idx, image_info);
			}

			if (image_info.helm)
			{
				bg_info.background = &bprops;
				PaintImage(image_info.img, element, image_info.helm, bg_info,
						   border_box, image_info.is_svg, image_info.override_img_width,
						   image_info.override_img_height, image_info.replaced_url);
			}
		}
#ifdef CSS_GRADIENT_SUPPORT
		else
			if (bprops.bg_gradient)
			{
				bg_info.background = &bprops;
				PaintBackgroundGradient(element, bg_info, current_color, border_box, *bprops.bg_gradient);
				border_area_covered_by_background = (bprops.bg_clip == CSS_VALUE_border_box);
			}
#endif // CSS_GRADIENT_SUPPORT
		// else: background: none
	}
}

void
BackgroundsAndBorders::InitializeBgInfo(BG_OUT_INFO &bg_info, HTML_Element *element, Border &used_border)
{
	bg_info.element = element;
	bg_info.border = &used_border;
	bg_info.border_image = border_image.HasBorderImage() ? &border_image : NULL;
	bg_info.has_border_left = HasBorder(used_border.left);
	bg_info.has_border_top = HasBorder(used_border.top);
	bg_info.has_border_right = HasBorder(used_border.right);
	bg_info.has_border_bottom = HasBorder(used_border.bottom);
	bg_info.background = NULL;
	bg_info.has_inset_box_shadow = FALSE;
	bg_info.image_rendering = image_rendering;
}

void
BackgroundsAndBorders::UseCascadedProperties(LayoutProperties *cascade)
{
	/* This is how we want to draw borders around fieldsets with a legend.

	   +- This is a legend --------+
       |                           |
       +---------------------------+

	   To accomplish this we compute extra_top_margin, top_gap_x, top_gap_width and use them when
	   painting backgrounds and borders. */

	HTML_Element *element = cascade->html_element;
	if (element && element->Type() == Markup::HTE_FIELDSET && element->GetNsType() == NS_HTML)
	{
		HTML_Element* elm = element->FirstChild();

		while (elm && !elm->GetLayoutBox())
			elm = elm->Suc();

		if (elm && elm->Type() == Markup::HTE_LEGEND && elm->GetNsType() == NS_HTML)
		{
			LayoutProperties* legend_props = cascade->GetChildProperties(doc->GetHLDocProfile(), elm); // FIXME: OOM
			if (legend_props)
			{
				/* LEGEND is the first child of the FIELDSET. Need to check if the CSS properties
				   are acceptable before moving the FIELDSET top border. A similar check is done in
				   HTMLayoutProperties::GetCssProperties(), and we need to perform the exact same
				   check here. */

				Box *legend_box = elm->GetLayoutBox();
				if (legend_box && legend_box->IsBlockBox() && legend_props->GetProps()->position == CSS_VALUE_static && legend_props->GetProps()->float_type == CSS_VALUE_none)
				{
					// Move top-border down a bit for fieldsets with a legend. Also make a gap for the legend.
					extra_top_margin = LayoutCoord((((BlockBox *)legend_box)->GetHeight() + cascade->GetProps()->border.top.width) / 2);
					top_gap_x = ((BlockBox *)legend_box)->GetX();
					top_gap_width = legend_box->GetWidth();
				}
			}
		}
	}

	/* Save some properties for use later */

	const HTMLayoutProperties& props = *cascade->GetProps();
	font_color = props.font_color;

	padding_left = props.padding_left;
	padding_right = props.padding_right;
	padding_top = props.padding_top;
	padding_bottom = props.padding_bottom;

	css_length_resolver.SetFontSize(props.decimal_font_size_constrained);
	css_length_resolver.SetFontNumber(props.font_number);
	border_image = props.border_image;
	box_decoration_break = props.box_decoration_break;
	image_rendering = props.image_rendering;
}

void BackgroundsAndBorders::PaintBackgrounds(HTML_Element *element, COLORREF bg_color, COLORREF current_color,
											 const BgImages &bg_images, const Shadows *shadows,
											 const OpRect &border_box)
{
	if (!SkipBackground(element))
	{
		OpRect modified_border_box(border_box);
		modified_border_box.height -= extra_top_margin;

		BG_OUT_INFO bg_info;
		Border used_border = computed_border.GetUsedBorder(border_box);
		InitializeBgInfo(bg_info, element, used_border);

		vis_dev->Translate(0, extra_top_margin);

		HandleBoxShadows(shadows, modified_border_box, &bg_info, FALSE);

		if (!modified_border_box.IsEmpty())
		{
			if (!SkipBackgroundColor(bg_info, bg_images, element))
				PaintBackgroundColor(bg_color, bg_images, bg_info, modified_border_box);

			if (bg_images.IsEmpty())
				ResetBgImagesVisibility(doc, element);

			if (doc->GetShowImages())
				PaintBackgroundImages(bg_images, bg_info, current_color, modified_border_box, element);
		}

		HandleBoxShadows(shadows, modified_border_box, &bg_info, TRUE);

		vis_dev->Translate(0, -extra_top_margin);
	}
}

void BackgroundsAndBorders::PaintBorders(HTML_Element *element, const OpRect &border_box, COLORREF current_color)
{
	if (IsBorderAllTransparent())
		return;

	BG_OUT_INFO bg_info;
	Border used_border = computed_border.GetUsedBorder(border_box);
	InitializeBgInfo(bg_info, element, used_border);

	OpRect modified_border_box(border_box);
	modified_border_box.height -= extra_top_margin;

	LayoutCoord width(modified_border_box.width);
	LayoutCoord height(modified_border_box.height);

	vis_dev->Translate(0, extra_top_margin);

	if (!border_area_covered_by_background)
		vis_dev->BgHandleNoBackground(&bg_info, modified_border_box);

	if (border_image.border_img)
	{
		ImageInfo image_info;

		/* Sub-par SVG support here. */

		if (!GetImageInformation(element, border_image.border_img, modified_border_box, BORDER_IMAGE_INLINE, 0, image_info))
			return;

		if (image_info.helm)
		{
			SetDocumentPosition(element, image_info.helm, modified_border_box);
			OpRect draw_rect = modified_border_box;

#ifdef SVG_SUPPORT
			VDStateNoScale state_noscale;

			if(image_info.is_svg)
			{
				bg_info.image_rendering = CSS_VALUE_auto;

				state_noscale = vis_dev->BeginNoScalePainting(draw_rect);
				draw_rect = state_noscale.dst_rect;
			}
#endif // SVG_SUPPORT

			vis_dev->PaintBorderImage(image_info.img, image_info.helm, draw_rect, &used_border, &border_image, NULL, bg_info.image_rendering);

#ifdef SVG_SUPPORT
			if(image_info.is_svg)
				vis_dev->EndNoScalePainting(state_noscale);
#endif // SVG_SUPPORT
		}
	}
#ifdef CSS_GRADIENT_SUPPORT
	else
		if (border_image.border_gradient)
			vis_dev->PaintBorderImageGradient(modified_border_box, &used_border, &border_image, current_color);
#endif // CSS_GRADIENT_SUPPORT
		else
#ifdef VEGA_OPPAINTER_SUPPORT
			if (used_border.HasRadius())
				vis_dev->DrawBorderWithRadius(width, height, bg_info.border, top_gap_x, top_gap_width);
			else
#endif // VEGA_OPPAINTER_SUPPORT
				if (used_border.IsAllSolidAndSameColor() && !top_gap_width)
				{
					/* Calling LineOut() (see below) will result in drawing lines 1px
					   wide at a time, i.e. a lot of drawing operations if the borders
					   are thick. Since we have just realized that all border edges have
					   the same color and solid style, we can take a shortcut and save a
					   lot of time if the borders are thick. */

					if (used_border.top.color != CSS_COLOR_transparent)
					{
						COLORREF old_col = vis_dev->GetColor();
						int vertical_border_height = height - used_border.bottom.width - used_border.top.width;

						vis_dev->SetColor(used_border.top.color);

						if (vertical_border_height > 0)
						{
							if (used_border.left.width > 0)
								vis_dev->FillRect(OpRect(0, used_border.top.width, used_border.left.width, vertical_border_height));

							if (used_border.right.width > 0)
								vis_dev->FillRect(OpRect(width - used_border.right.width, used_border.top.width, used_border.right.width, vertical_border_height));
						}

						if (used_border.top.width > 0)
							vis_dev->FillRect(OpRect(0, 0, width, used_border.top.width));

						if (used_border.bottom.width > 0)
							vis_dev->FillRect(OpRect(0, height - used_border.bottom.width, width, used_border.bottom.width));

						vis_dev->SetColor(old_col);
					}
				}
				else
				{
					if (bg_info.has_border_bottom)
						vis_dev->LineOut(0, height - 1, width,
										 used_border.bottom.width, used_border.bottom.style,
										 used_border.bottom.color, TRUE, FALSE,
										 used_border.left.width, used_border.right.width);

					if (bg_info.has_border_right)
						vis_dev->LineOut(width - 1, 0,
										 height, used_border.right.width, used_border.right.style,
										 used_border.right.color, FALSE, FALSE,
										 used_border.top.width, used_border.bottom.width);

					if (bg_info.has_border_left)
						vis_dev->LineOut(0, 0,
										 height, used_border.left.width, used_border.left.style,
										 used_border.left.color, FALSE, TRUE, used_border.top.width, used_border.bottom.width);

					if (bg_info.has_border_top)
					{
						if (top_gap_width)
						{
							vis_dev->LineOut(0, 0, top_gap_x, used_border.top.width,
											 used_border.top.style, used_border.top.color,
											 TRUE, TRUE, used_border.left.width, 0);

							vis_dev->LineOut(top_gap_x + top_gap_width, 0, width - top_gap_x - top_gap_width, used_border.top.width,
											 used_border.top.style, used_border.top.color,
											 TRUE, TRUE, 0, used_border.right.width);
						}
						else
							vis_dev->LineOut(0, 0,
											 width, used_border.top.width,
											 used_border.top.style, used_border.top.color, TRUE, TRUE,
											 used_border.left.width, used_border.right.width);
					}
				}

	vis_dev->Translate(0, -extra_top_margin);
}

void
BackgroundsAndBorders::OverrideProperties(const DocRootProperties &doc_root_props)
{
	/* Border widths (only) are copied from the element that defines the
	   background for the viewport. To be compatible with other browsers
	   with regards to background image placement. */

	const Border& b = doc_root_props.GetBorder();
	computed_border.top.width = b.top.width;
	computed_border.left.width = b.left.width;
	computed_border.bottom.width = b.bottom.width;
	computed_border.right.width = b.right.width;

	font_color = doc_root_props.GetBodyFontColor();
	css_length_resolver.SetFontSize(doc_root_props.GetBodyFontSize());
	css_length_resolver.SetFontNumber(doc_root_props.GetBodyFontNumber());
}

/* static */ void
BackgroundsAndBorders::ResetBgImagesVisibility(FramesDocument *doc, HTML_Element *element)
{
	if (element->HasBgImage())
	{
		// Used to have a bgimage but no more, let everyone know it's gone
		element->UndisplayImage(doc, TRUE);
		element->SetHasBgImage(FALSE);
	}
}

/* static */ void
BackgroundsAndBorders::HandleRootBackgroundElementChanged(FramesDocument* doc, Markup::Type bg_element_type, HTML_Element* doc_root, HTML_Element* body_elm)
{
	if (bg_element_type == Markup::HTE_DOC_ROOT || bg_element_type == Markup::HTE_HTML)
	{
		if (doc_root && bg_element_type == Markup::HTE_DOC_ROOT)
		{
			/* No background on <html> */
			ResetBgImagesVisibility(doc, doc_root);
		}

		if (body_elm && doc_root != body_elm)
			/* No doc root background on <body>. If the body element still has a
			   background image, reponsibility for painting it is left to the actual
			   body element. */
			ResetBgImagesVisibility(doc, body_elm);
	}
}

/** Collapse this margin with the other margin and return difference. */

void
VerticalMargin::Collapse(const VerticalMargin& other, LayoutCoord* y_offset, LayoutCoord* y_offset_nonpercent)
{
	LayoutCoord yoffs(0);
	LayoutCoord yoffs_nonpercent(0);

	if (max_positive < other.max_positive)
	{
		yoffs = other.max_positive - max_positive;
		max_positive = other.max_positive;
	}

	if (max_negative < other.max_negative)
	{
		yoffs -= other.max_negative - max_negative;
		max_negative = other.max_negative;
	}

	if (max_positive_nonpercent < other.max_positive_nonpercent)
	{
		yoffs_nonpercent = other.max_positive_nonpercent - max_positive_nonpercent;
		max_positive_nonpercent = other.max_positive_nonpercent;
	}

	if (max_negative_nonpercent < other.max_negative_nonpercent)
	{
		yoffs_nonpercent -= other.max_negative_nonpercent - max_negative_nonpercent;
		max_negative_nonpercent = other.max_negative_nonpercent;
	}

	if (y_offset)
		*y_offset = yoffs;

	if (y_offset_nonpercent)
		*y_offset_nonpercent = yoffs_nonpercent;
}

/** Set margin based on bottom margin property. */

void
VerticalMargin::CollapseWithTopMargin(const HTMLayoutProperties& props, BOOL ignore_default)
{
	if (!ignore_default && !props.GetMarginTopSpecified())
	{
		if (max_default < props.margin_top)
			max_default = props.margin_top;

		if (max_default_nonpercent < props.margin_top && !props.GetMarginTopIsPercent())
			max_default_nonpercent = props.margin_top;
	}
	else
		if (props.margin_top < 0)
		{
			if (max_negative < -props.margin_top)
				max_negative = -props.margin_top;

			if (max_negative_nonpercent < -props.margin_top && !props.GetMarginTopIsPercent())
				max_negative_nonpercent = -props.margin_top;
		}
		else
		{
			if (max_positive < props.margin_top)
				max_positive = props.margin_top;

			if (max_positive_nonpercent < props.margin_top && !props.GetMarginTopIsPercent())
				max_positive_nonpercent = props.margin_top;
		}
}

/** Set margin based on the margin-bottom property. */

void
VerticalMargin::CollapseWithBottomMargin(const HTMLayoutProperties& props, BOOL ignore_default)
{
	if (!ignore_default && !props.GetMarginBottomSpecified())
	{
		if (max_default < props.margin_bottom)
			max_default = props.margin_bottom;

		if (max_default_nonpercent < props.margin_bottom && !props.GetMarginBottomIsPercent())
			max_default_nonpercent = props.margin_bottom;
	}
	else
		if (props.margin_bottom < 0)
		{
			if (max_negative < -props.margin_bottom)
				max_negative = -props.margin_bottom;

			if (max_negative_nonpercent < -props.margin_bottom && !props.GetMarginBottomIsPercent())
				max_negative_nonpercent = -props.margin_bottom;
		}
		else
		{
			if (max_positive < props.margin_bottom)
				max_positive = props.margin_bottom;

			if (max_positive_nonpercent < props.margin_bottom && !props.GetMarginBottomIsPercent())
				max_positive_nonpercent = props.margin_bottom;
		}
}

/** Apply default margin, so that it participates in margin collapsing. */

void
VerticalMargin::ApplyDefaultMargin()
{
	OP_ASSERT(max_default >= 0);
	OP_ASSERT(max_default_nonpercent >= 0);

	if (max_positive < max_default)
		max_positive = max_default;

	if (max_positive_nonpercent < max_default_nonpercent)
		max_positive_nonpercent = max_default_nonpercent;
}

/** Recalculate containing block size and schedule the specified descendant box for reflow if needed. */

/* virtual */ void
VerticalBox::RecalculateContainingBlock(FramesDocument* doc, AbsolutePositionedBox* box)
{
	// It has already been determined that this descendant needs reflow.

	box->MarkDirty(doc, FALSE);
}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

/** Signal that the HTML_Element associated with this layout box about to be deleted. */

/* virtual */ void
VerticalBox::SignalHtmlElementDeleted(FramesDocument* doc)
{
}

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

/** Go through absolutely positioned descendants and schedule them for reflow as needed.

	Check if the containing block established by this box changed in such a way that
	absolutely positioned descendants need to be reflowed. */

void
VerticalBox::CheckAbsPosDescendants(LayoutInfo& info)
{
	if (IsPositionedBox() && (HasContainingHeightChanged(TRUE) || HasContainingWidthChanged(TRUE)))
		/* At this point, the final height of the container is known, and thus
		   we can notify descendants that have this container as their
		   containing block about changes. */

		GetReflowState()->abs_pos_descendants.ContainingBlockChanged(info, this);
}

/** Initialise reflow state. */

VerticalBoxReflowState*
VerticalBox::InitialiseReflowState()
{
	if (!(un.data & 1))
	{
		VerticalBoxReflowState* state = new VerticalBoxReflowState;

		if (state)
		{
			state->html_element = un.html_element;

#ifdef CSS_TRANSFORMS
			if (TransformContext *transform_context = GetTransformContext())
				if (!transform_context->InitialiseReflowState(state))
				{
					OP_DELETE(state);
					return NULL;
				}
#endif

			un.data = ((UINTPTR)state) | 1;
		}

		return state;
	}
	else
		return (VerticalBoxReflowState*) (un.data & ~1);
}

/** Paint background and border of this box on screen. */

/* virtual */ void
VerticalBox::PaintBgAndBorder(LayoutProperties* cascade, FramesDocument* doc, VisualDevice* visual_device) const
{
	HTML_Element* html_element = cascade->html_element;
	const HTMLayoutProperties& props = *cascade->GetProps();
	BackgroundsAndBorders bb(doc, visual_device, cascade, &props.border);
	COLORREF bg_color = BackgroundColor(props, this);
	BOOL is_root = html_element->Type() == Markup::HTE_DOC_ROOT;

	if (!is_root)
	{
		if (doc->GetHandheldEnabled() && bg_color == USE_DEFAULT_COLOR)
		{
			// Inherit background color from parent inline element - if any

			LayoutProperties* parent_cascade = cascade->Pred();

			if (parent_cascade->html_element && parent_cascade->html_element->GetLayoutBox()->IsInlineBox())
				bg_color = parent_cascade->GetCascadingProperties().bg_color;
		}

		if (HasNoBgAndBorder(bg_color, bb, props))
			return;
	}

	LayoutCoord box_width = GetWidth();
	LayoutCoord box_height = GetHeight();
	BgImages bg_images = props.bg_images;
	BOOL background_drawn_by_root = FALSE;

	if (is_root)
	{
		LayoutCoord visible_width = LayoutCoord(visual_device->GetRenderingViewWidth());
		LayoutCoord visible_height = LayoutCoord(visual_device->GetRenderingViewHeight());

		box_width = LayoutCoord(MAX(doc->Width() + doc->NegativeOverflow(), visible_width));  // Use documentwidth for background or viswidth if bigger.

		box_height = MAX(LayoutCoord(doc->Height()), visible_height);  // Use documentheight for background or visheight if bigger.
		box_width = LayoutCoord(visual_device->ApplyScaleRoundingNearestUp(box_width - visible_width) + visible_width);
		box_height = LayoutCoord(visual_device->ApplyScaleRoundingNearestUp(box_height - visible_height) + visible_height);

		HTML_Element* bg_element = html_element;
		HLDocProfile* hld_profile = doc->GetHLDocProfile();
		DocRootProperties& doc_root_properties = hld_profile->GetLayoutWorkplace()->GetDocRootProperties();
		const Markup::Type bg_element_type = doc_root_properties.GetBackgroundElementType();

		HTML_Element* body_elm = hld_profile->GetBodyElm();
		LayoutCoord bg_x_pos(0);
		LayoutCoord bg_y_pos(0);
		LayoutCoord bg_box_width = box_width;
		LayoutCoord bg_box_height = box_height;

		if (!body_elm && hld_profile->IsXml())
			body_elm = hld_profile->GetDocRoot();

		if (body_elm && (body_elm->GetNsType() == NS_HTML || body_elm == hld_profile->GetDocRoot()))
		{
			bg_color = doc_root_properties.GetBackgroundColor();
			bg_images = doc_root_properties.GetBackgroundImages();

			if (bg_color != USE_DEFAULT_COLOR || !bg_images.IsEmpty())
			{
				if (bg_element_type == Markup::HTE_BODY || body_elm == hld_profile->GetDocRoot())
					bg_element = body_elm;
				else
					bg_element = body_elm->Parent();
			}

			if (bg_element != html_element)
			{
				HTML_Element* root_element = bg_element == body_elm ? body_elm->Parent() : bg_element;

				if (root_element != html_element && root_element->GetLayoutBox() && root_element->GetLayoutBox()->IsBlockBox())
				{
					BlockBox* box = (BlockBox*)root_element->GetLayoutBox();
					const Border& border = doc_root_properties.GetBorder();

					bg_x_pos = box->GetX();
					bg_y_pos = box->GetY();
					bg_box_width = box->GetWidth();
					bg_box_height = box->GetHeight();

					bg_x_pos += LayoutCoord(border.left.width);
					bg_y_pos += LayoutCoord(border.top.width);
					bg_box_width -= LayoutCoord(border.left.width + border.right.width);
					bg_box_height -= LayoutCoord(border.top.width + border.bottom.width);
				}
			}
		}

		if (bg_color == USE_DEFAULT_COLOR)
		{
			BOOL transparent = FALSE;

			Window* win = doc->GetWindow();

			if (doc->GetParentDoc() && !doc->GetParentDoc()->IsFrameDoc() ||
				doc_root_properties.HasTransparentBackground() && win->IsBackgroundTransparent())
			{
				// Don't draw background for iframes with transparent background.

				transparent = TRUE;
				doc->GetLogicalDocument()->GetLayoutWorkplace()->SetBgFixed();
			}

#ifdef GADGET_SUPPORT
			if (win->GetType() != WIN_TYPE_GADGET ||
				!win->GetGadget() ||
				win->GetGadget()->GetGadgetType() != GADGET_TYPE_CHROMELESS)
#endif
				if (!transparent)
					bg_color = win->GetDefaultBackgroundColor();
		}

#ifdef QUICK
		if (doc->GetLayoutMode() == LAYOUT_SSR ||
			doc->GetLayoutMode() == LAYOUT_CSSR)
			// We render the docroot gray in ssr mode on desktop to make the document width more visible.

			bg_color = OP_RGB(100,100,100);
#endif

		// Paint background only
		BackgroundsAndBorders bb(doc, visual_device, cascade, NULL);

		/* For the document root, the background positioning area and
		   the background paint area differs.

		   The background positioning area is the root's corresponding
		   box given by (bg_x_pos, bg_y_pos, bg_box_width,
		   bg_box_height).

		   The background paint area is the canvas, normally the area
		   (0, 0, box_width, box_height). We also extend this area to
		   take negative overflow into account. */

		bb.OverridePositioningRect(OpRect(bg_x_pos, bg_y_pos, bg_box_width, bg_box_height));
		bb.OverrideProperties(doc_root_properties);

		if (doc_root_properties.HasBackgroundElementChanged())
		{
			/* The background element used to be another element, the
			   html element or the body element. */

			bb.HandleRootBackgroundElementChanged(doc, bg_element_type, hld_profile->GetDocRoot(), body_elm);

			/* We have done our deed and can now reset the flag */
			doc_root_properties.ClearBackgroundElementChanged();
		}

		if (OP_GET_A_VALUE(bg_color) < 255)
		{
			Window* win = doc->GetWindow();

			if (doc->GetParentDoc() && !doc->GetParentDoc()->IsFrameDoc())
				doc->GetLogicalDocument()->GetLayoutWorkplace()->SetBgFixed();
			else if (!doc->GetParentDoc() && !win->IsBackgroundTransparent())
			{
#ifdef GADGET_SUPPORT
				if (win->GetType() != WIN_TYPE_GADGET ||
					win->GetGadget()->GetGadgetType() != GADGET_TYPE_CHROMELESS)
#endif
				{
					BgImages empty_bg_images;
					OP_ASSERT(empty_bg_images.IsEmpty());
					/* The wanted background color is transparent and we must paint a solid one.
					   Use the default background color as background filler first. */
					bb.PaintBackgrounds(bg_element, colorManager->GetBackgroundColor(), props.font_color, empty_bg_images, NULL,
										OpRect(-bounding_box.Left(), 0, box_width + bounding_box.Left(), box_height));
				}
			}
		}
		bb.PaintBackgrounds(bg_element, bg_color, props.font_color, bg_images, NULL,
							OpRect(-bounding_box.Left(), 0, box_width + bounding_box.Left(), box_height));

		box_height = LayoutCoord(doc->Height()); // Use document height for border on HTML.
		background_drawn_by_root = TRUE;
	}
	else
		background_drawn_by_root = CheckBackgroundOnRoot(doc, html_element);

	if (ScrollableArea* scrollable = GetScrollable())
		bb.SetScrollOffset(scrollable->GetViewX(), scrollable->GetViewY());

	OpRect border_box(0, 0, box_width, box_height);
	if (!background_drawn_by_root)
		bb.PaintBackgrounds(html_element, bg_color, props.font_color, bg_images, &props.box_shadows, border_box);

	bb.PaintBorders(html_element, border_box, props.font_color);
}

/** Traverse box with children. */

/* virtual */ void
PositionedBlockBox::Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element)
{
	HTML_Element* html_element = GetHtmlElement();
	TraverseType old_traverse_type = traversal_object->GetTraverseType();

	if (traversal_object->IsTarget(html_element) &&
		(old_traverse_type == TRAVERSE_ONE_PASS ||
		 old_traverse_type == TRAVERSE_CONTENT))
	{
		if (packed.on_line && !traversal_object->GetInlineFormattingContext() && traversal_object->TraverseInLogicalOrder())
			/* If we're traversing in logical order, this block must be
			   traversed during line traversal, which is not now. */

			return;

		RootTranslationState root_translation_state;
		LayoutProperties* layout_props = NULL;
		TraverseInfo traverse_info;
		BOOL traverse_descendant_target = FALSE;

#ifdef CSS_TRANSFORMS
		TransformContext *transform_context = GetTransformContext();
		TranslationState transforms_translation_state;
#endif

		traversal_object->Translate(x, y);

		/* Update root translation by computing the vector from the
		   current root translation to the current translation. Then
		   apply this vector to the root translation. */

		traversal_object->SyncRootScrollAndTranslation(root_translation_state);

#ifdef CSS_TRANSFORMS
		if (transform_context)
		{
			OP_BOOLEAN status = transform_context->PushTransforms(traversal_object, transforms_translation_state);
			switch (status)
			{
			case OpBoolean::ERR_NO_MEMORY:
				traversal_object->SetOutOfMemory();
			case OpBoolean::IS_FALSE:
				if (traversal_object->GetTarget())
					traversal_object->SwitchTarget(containing_element);
				traversal_object->RestoreRootScrollAndTranslation(root_translation_state);
				traversal_object->Translate(-x, -y);
				return;
			}
		}
#endif //CSS_TRANSFORMS

		HTML_Element* old_target = traversal_object->GetTarget();

		if (old_target == html_element)
			traversal_object->SetTarget(NULL);

		if (!traversal_object->GetTarget() && old_traverse_type != TRAVERSE_ONE_PASS)
			traversal_object->SetTraverseType(TRAVERSE_BACKGROUND);

		if (traversal_object->EnterVerticalBox(parent_lprops, layout_props, this, traverse_info))
		{
			PaneTraverseState old_pane_traverse_state;

			traversal_object->StorePaneTraverseState(old_pane_traverse_state);

#ifdef DISPLAY_AVOID_OVERDRAW
			if (HasNegativeZChildren() && traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
				traversal_object->FlushBackgrounds(layout_props, this);
#endif

			TraverseZChildren(traversal_object, layout_props, FALSE, FALSE);
			traversal_object->RestorePaneTraverseState(old_pane_traverse_state);

			// Traverse children in normal flow

			if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
			{
				content->Traverse(traversal_object, layout_props);

#ifdef DISPLAY_AVOID_OVERDRAW
				traversal_object->FlushBackgrounds(layout_props, this);
#endif

				traversal_object->SetTraverseType(TRAVERSE_CONTENT);
				traversal_object->RestorePaneTraverseState(old_pane_traverse_state);

				traversal_object->TraverseFloats(this, layout_props);
				traversal_object->RestorePaneTraverseState(old_pane_traverse_state);
			}

			content->Traverse(traversal_object, layout_props);

			PaneTraverseState new_pane_traverse_state;

			traversal_object->StorePaneTraverseState(new_pane_traverse_state);

			// Traverse positioned children with z_index >= 0 if bottom-up or z_index < 0 if top-down

			traversal_object->RestorePaneTraverseState(old_pane_traverse_state);
			TraverseZChildren(traversal_object, layout_props, TRUE, FALSE);
			traversal_object->RestorePaneTraverseState(new_pane_traverse_state);

			traversal_object->LeaveVerticalBox(layout_props, this, traverse_info);
		}
		else
			traversal_object->SetTraverseType(old_traverse_type);

#ifdef CSS_TRANSFORMS
		if (transform_context)
			transform_context->PopTransforms(traversal_object, transforms_translation_state);
#endif

		if (old_target == html_element)
		{
			traversal_object->SetTarget(old_target);
			if (traversal_object->SwitchTarget(containing_element) == TraversalObject::SWITCH_TARGET_NEXT_IS_DESCENDANT)
			{
				OP_ASSERT(!GetLocalStackingContext());
				traverse_descendant_target = TRUE;
			}
		}

		traversal_object->RestoreRootScrollAndTranslation(root_translation_state);
		traversal_object->Translate(-x, -y);

		if (traverse_descendant_target)
			/* This box was the target (whether it was entered or not is not interesting,
			   though). The next target is a descendant. We can proceed (no need to
			   return to the stacking context loop), but first we have to retraverse
			   this box with the new target set - most importantly because we have to
			   make sure that we enter it and that we do so in the right way. */

			Traverse(traversal_object, parent_lprops, containing_element);
	}
}

/** Reflow box and children. */

/* virtual */ LAYST
PositionedBlockBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
	{
		if (cascade->stacking_context)
			cascade->stacking_context->AddZElement(&z_element);

		LAYST status = cascade->container->CreateEmptyLine(info);
		if (status != LAYOUT_CONTINUE)
			return status;

		/* Initially in the layout process, we allocate space for the block
		   without paying attention to relative offsets (as per spec). Will add
		   relative offsets when this is finished. */

		left = LayoutCoord(0);
		top = LayoutCoord(0);
	}

	return BlockBox::Layout(cascade, info, first_child, start_position);
}

/** Propagate a break point caused by break properties or a spanned element. */

/* virtual */ BOOL
PositionedBlockBox::PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint)
{
	return GetCascade()->container->PropagateBreakpoint(virtual_y + y - top, break_type, breakpoint);
}

/** Reflow box and children. */

/* virtual */ LAYST
PositionedZRootBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			z_element.SetZIndex(cascade->GetProps()->z_index);

	return PositionedBlockBox::Layout(cascade, info, first_child, start_position);
}

/** Traverse box with children. */

/* virtual */ void
PositionedZRootBox::TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip)
{
	// Traverse children with z_index < 0 if bottom-up or z_index > 0 if top-down

	stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip);
}

/** Lay out box. */

/* virtual */ LAYST
BlockBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	HTML_Element* html_element = cascade->html_element;

	if (first_child && first_child != html_element)
	{
		return content->Layout(cascade, info, first_child, start_position);
	}
	else
	{
		VerticalBoxReflowState* state = InitialiseReflowState();

		if (!state)
			return LAYOUT_OUT_OF_MEMORY;

		Container* container = cascade->container;
		LAYST continue_layout;

		if (container->IsCssFirstLine())
		{
			continue_layout = container->CommitLineContent(info, TRUE);
			if (continue_layout != LAYOUT_CONTINUE)
				return continue_layout;

			container->ClearCssFirstLine(html_element);
			return LAYOUT_END_FIRST_LINE;
		}
		else
		{
			state->needs_update = html_element->NeedsUpdate();

			int reflow_content = html_element->MarkClean();
			const HTMLayoutProperties& props = *cascade->GetProps();
			BOOL is_container = !!content->GetContainer();
			BOOL is_positioned = IsPositionedBox();
			SpaceManager* local_space_manager = GetLocalSpaceManager();

			if (Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
				return LAYOUT_OUT_OF_MEMORY;

			state->cascade = cascade;
			state->old_x = x;
			state->old_y = y;
			state->old_width = content->GetWidth();
			state->old_height = content->GetHeight();
			state->old_bounding_box = bounding_box;

#ifdef CSS_TRANSFORMS
			if (state->transform)
				state->transform->old_transform = state->transform->transform_context->GetCurrentTransform();
#endif

			if (container->IsCalculatingMinMaxWidths() && !cascade->space_manager->IsFullBfcMinMaxCalculationEnabled())
			{
				/* The container is calculating min/max widths. We need to pay special attention to
				   changes then. If the maximum width or minimum height of this block changes during
				   reflow, we need to enable full block formatting context min/max calculation. */

				LayoutCoord dummy;

				content->GetMinMaxWidth(dummy, dummy, state->old_max_width);
				state->old_min_height = content->GetMinHeight();
				state->old_min_y = GetMinY();
			}

#ifdef LAYOUT_YIELD_REFLOW
			if (local_space_manager && info.force_layout_changed)
			{
				local_space_manager->EnableFullBfcReflow();
				reflow_content = ELM_DIRTY;
			}
#endif // LAYOUT_YIELD_REFLOW

			packed.has_absolute_width = props.content_width >= 0;
			packed.has_clearance = FALSE;

			if (cascade->multipane_container)
			{
				packed.column_spanned = props.column_span == COLUMN_SPAN_ALL &&
					cascade->multipane_container->GetMultiColumnContainer() &&
					cascade->space_manager == cascade->multipane_container->GetPlaceholder()->GetLocalSpaceManager();

				/* There's an ancestor multi-pane container, but that doesn't
				   have to mean that this block is to be columnized. */

				packed.in_multipane = cascade->container == cascade->multipane_container ||
					cascade->container->GetPlaceholder()->IsColumnizable(TRUE);
			}

			continue_layout = container->GetNewBlockStage1(info, cascade, this);

			if (continue_layout != LAYOUT_CONTINUE)
				return continue_layout;

			// Recalculate top margins

			info.Translate(x, y);

			if (props.margin_top)
			{
				VerticalMargin top_margin;

				top_margin.CollapseWithTopMargin(props);
				RecalculateTopMargins(info, &top_margin);
			}

			switch (content->ComputeSize(cascade, info))
			{
			case OpBoolean::IS_TRUE:
				reflow_content |= ELM_DIRTY;

			case OpBoolean::IS_FALSE:
				break;

			default:
				return LAYOUT_OUT_OF_MEMORY;
			}

			if (is_container)
			{
				if (container->HasFlexibleMarkers())
					/* If we have flexible markers, we must force reflow of the content
					   to ensure that the markers will be vertically aligned according
					   to the descendant content, just like after reflow with
					   layout structures building. Might be worth considering doing it better
					   to spare the full content reflow. */
#ifdef CSS_TRANSFORMS
					/* But not if the box is transformed, because in such case we don't allow its
					   container to influence flexible markers' positions. */
					if (!GetTransformContext())
#endif // CSS_TRANSFORMS
						reflow_content = ELM_DIRTY;

				if (!local_space_manager)
				{
					if (cascade->space_manager->IsFullBfcReflowEnabled()
#ifdef PAGED_MEDIA_SUPPORT
						&& !info.pending_pagebreak_found
#endif
						)
						/* Lines, tables, block replaced content, and blocks with non-visible overflow in
						   the block box may be affected by floats. */

						reflow_content = ELM_DIRTY;

					if (container->IsCalculatingMinMaxWidths() && cascade->space_manager->IsFullBfcMinMaxCalculationEnabled())
					{
						reflow_content = ELM_DIRTY;
						content->ClearMinMaxWidth();
					}
				}
			}

			if (!container->GetNewBlockStage2(info, cascade, this))
				return LAYOUT_OUT_OF_MEMORY;

			if (packed.column_spanned)
			{
				// Propagate the break caused by this spanned element to the nearest multicol container.

				MultiColBreakpoint* breakpoint = NULL;

				if (!container->PropagateBreakpoint(y - cascade->GetProps()->margin_top, SPANNED_ELEMENT, &breakpoint))
					return LAYOUT_OUT_OF_MEMORY;

				/* We have already confirmed that this is a suitable element for
				   spanning all columns, so it should REALLY result in a breakpoint. */

				OP_ASSERT(breakpoint);
				OP_ASSERT(cascade->multipane_container->GetMultiColumnContainer());

				if (!((MultiColumnContainer*) cascade->multipane_container)->AddPendingSpannedElm(cascade, breakpoint))
					return LAYOUT_OUT_OF_MEMORY;
			}

			if (is_positioned)
			{
				// Normal flow position has been set. Now add relative offsets.

				LayoutCoord left_offset;
				LayoutCoord top_offset;

				props.GetLeftAndTopOffset(left_offset, top_offset);

				SetRelativePosition(left_offset, top_offset);

				x += left_offset;
				y += top_offset;
				info.Translate(left_offset, top_offset);

				state->previous_nonpos_scroll_x = info.nonpos_scroll_x;
				state->previous_nonpos_scroll_y = info.nonpos_scroll_y;
				info.nonpos_scroll_x = LayoutCoord(0);
				info.nonpos_scroll_y = LayoutCoord(0);
			}

#ifdef MEDIA_HTML_SUPPORT
			if (html_element->IsMatchingType(Markup::CUEE_ROOT, NS_CUE))
				if (MediaElement* media_element = ReplacedContent::GetMediaElement(html_element))
				{
					OpPoint cue_pos = media_element->GetCuePosition(html_element);

					info.Translate(LayoutCoord(cue_pos.x) - x, LayoutCoord(cue_pos.y) - y);

					x = LayoutCoord(cue_pos.x);
					y = LayoutCoord(cue_pos.y);
				}
#endif // MEDIA_HTML_SUPPORT

			if (!reflow_content)
				if (
#ifdef PAGED_MEDIA_SUPPORT
					info.paged_media == PAGEBREAK_ON ||
#endif
					cascade->multipane_container ||
					info.stop_before && html_element->IsAncestorOf(info.stop_before) ||
					is_container && ((Container*) content)->GetInlineRunIn() ||
					container->HasPendingRunIn() ||
					props.display_type == CSS_VALUE_run_in || props.display_type == CSS_VALUE_compact ||
					props.display_type == CSS_VALUE_list_item && cascade->container->IsReversedList())
					reflow_content = ELM_DIRTY;

#ifdef CSS_TRANSFORMS
			if (state->transform)
				if (!state->transform->transform_context->PushTransforms(info, state->transform->translation_state))
					return LAYOUT_OUT_OF_MEMORY;
#endif

			if (!reflow_content)
			{
				// No need to reflow children, compute margins instead

				OP_BOOLEAN status;

				VerticalMargin top_margin;
				VerticalMargin bottom_margin;

				if (cascade->Suc())
					if (OpStatus::IsMemoryError(cascade->Suc()->Finish(&info)))
						return LAYOUT_OUT_OF_MEMORY;

				status = content->CalculateTopMargins(cascade, info, &top_margin, FALSE);
				RecalculateTopMargins(info, &top_margin);

				if (status == OpBoolean::IS_TRUE)
				{
					status = content->CalculateBottomMargins(cascade, info, &bottom_margin, FALSE);

					PropagateBottomMargins(info, &bottom_margin);
				}
				else
				{
					if (props.margin_bottom)
					{
						bottom_margin.CollapseWithBottomMargin(props);
						RecalculateTopMargins(info, &bottom_margin, TRUE);
					}

					if (packed.has_clearance)
						container->ClearanceStopsMarginCollapsing(TRUE);
				}

				if (status == OpStatus::ERR_NO_MEMORY)
					return LAYOUT_OUT_OF_MEMORY;

				if (is_container && !local_space_manager)
				{
					/* If the position of this block changes, its content may
					   be affected by floats, and we need to reflow and/or
					   recalculate min/max widths. */

					if (state->old_y != y || state->old_x != x)
					{
						cascade->space_manager->EnableFullBfcReflow();
						reflow_content = ELM_DIRTY;
					}

					if (container->IsCalculatingMinMaxWidths() && state->old_min_y != min_y)
					{
						cascade->space_manager->EnableFullBfcMinMaxCalculation();
						reflow_content = ELM_DIRTY;
						content->ClearMinMaxWidth();
					}
				}

				if (!reflow_content)
					if (status == OpBoolean::IS_FALSE)
						/* Need to propagate bounding box here, since it wasn't
						   propagated by PropagateBottomMargins. */

						PropagateBottomMargins(info, NULL, FALSE);
			}

			if (is_positioned)
			{
				info.GetRootTranslation(state->previous_root_x, state->previous_root_y);
				info.SetRootTranslation(info.GetTranslationX(), info.GetTranslationY());
				state->has_root_translation = TRUE;
			}

			if (reflow_content)
			{
				if (GetLocalZElement())
				{
					RestartStackingContext();

#ifdef PAGED_MEDIA_SUPPORT
					if (info.paged_media == PAGEBREAK_ON && !cascade->multipane_container)
						info.doc->RewindPage(0);
#endif // PAGED_MEDIA_SUPPORT
				}

				if (local_space_manager)
				{
					if (!local_space_manager->Restart())
						return LAYOUT_OUT_OF_MEMORY;
				}
				else
					if (is_container && container->IsCalculatingMinMaxWidths())
						content->ClearMinMaxWidth();

				bounding_box.Reset(props.DecorationExtent(info.doc), LayoutCoord(0));

				continue_layout = content->Layout(cascade, info);
			}
			else
				if (!cascade->SkipBranch(info, !local_space_manager, TRUE))
					return LAYOUT_OUT_OF_MEMORY;

			return continue_layout;
		}
	}
}

/** Get baseline of vertical layout.

	@param last_line TRUE if last line baseline search (inline-block case).	*/

/* virtual */ LayoutCoord
BlockBox::GetBaseline(BOOL last_line /*= FALSE*/) const
{
	return content->GetBaseline(last_line);
}

/** Do we need to calculate min/max widths of this box's content? */

/* virtual */ BOOL
Content_Box::NeedMinMaxWidthCalculation(LayoutProperties* cascade) const
{
	return cascade->container->IsCalculatingMinMaxWidths();
}

/** Propagate widths to container / table. */

/* virtual */ void
BlockBox::PropagateWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width)
{
	LayoutProperties* cascade = GetCascade();
	Container* container = cascade->container;

	if (!container->IsCalculatingMinMaxWidths())
		return;

	const HTMLayoutProperties& props = *cascade->GetProps();
	LayoutCoord margin(0);

	// Add non-percentage, horizontal margins.

	if (!props.GetMarginLeftIsPercent())
		margin += props.margin_left;

	if (!props.GetMarginRightIsPercent())
		margin += props.margin_right;

	if (container->AffectsFlexRoot() &&
		props.WidthIsPercent() &&
		!content->IsReplaced() &&
		(!content->GetTableContent() || ((TableContent*) content)->GetFixedLayout() != FIXED_LAYOUT_OFF))
	{
		/* Scale for percentage to prevent content overflow, unless it's
		   replaced content or a table with automatic layout. Width of such
		   tables will never be less than their minimum widths. Tables with
		   fixed layout, on the other hand, should perform this scaling. */

		min_width = ScalePropagationWidthForPercent(props, min_width, margin);
		normal_min_width = ScalePropagationWidthForPercent(props, normal_min_width, margin);
	}
	else
	{
		min_width = -margin < min_width ? min_width + margin : LayoutCoord(0);
		normal_min_width = -margin < normal_min_width ? normal_min_width + margin : LayoutCoord(0);
	}

	max_width = -margin < max_width ? max_width + margin : LayoutCoord(0);

	if (IsFloatingBox())
		container->PropagateFloatWidths(info, cascade, (FloatingBox*) this, min_width, normal_min_width, max_width);
	else
		container->PropagateBlockWidths(info, cascade, this, min_width, normal_min_width, max_width);
}

/** Set child's bottom margins in this block and propagate changes to parents. See declaration in header file for more information. */

/* virtual */ void
BlockBox::PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin, BOOL has_inflow_content)
{
	LayoutProperties* cascade = GetCascade();
	Container* container = cascade->container;
	AbsoluteBoundingBox abs_bounding_box;
	const HTMLayoutProperties& props = *cascade->GetProps();
	VerticalMargin new_bottom_margin;

	if (bottom_margin)
		new_bottom_margin = *bottom_margin;

	GetBoundingBox(abs_bounding_box, !IsPositionedBox() || props.overflow_x == CSS_VALUE_visible);

#ifdef CSS_TRANSFORMS
	if (TransformContext *transform_context = GetTransformContext())
		transform_context->ApplyTransform(abs_bounding_box);
#endif

	abs_bounding_box.Translate(x, y);

	if (has_inflow_content && props.margin_bottom)
		new_bottom_margin.CollapseWithBottomMargin(props);

	container->UpdateBottomMargins(info, &new_bottom_margin, &abs_bounding_box, has_inflow_content);
}

#ifdef PAGED_MEDIA_SUPPORT

/** Insert a page break. */

/* virtual */ BREAKST
BlockBox::InsertPageBreak(LayoutInfo& info, int strength)
{
	Container* container = GetCascade()->container;

	if (container)
	{
		BREAKST status;

		info.Translate(LayoutCoord(0), -y);
		status = container->InsertPageBreak(info, strength, ONLY_PREDECESSORS);
		info.Translate(LayoutCoord(0), y);

		return status;
	}

	return BREAK_NOT_FOUND;
}

/** Attempt to break page. */

BREAKST
BlockBox::AttemptPageBreak(LayoutInfo& info, int strength)
{
	BREAKST status;

	info.Translate(LayoutCoord(0), y);
	status = content->AttemptPageBreak(info, strength, HARD);
	info.Translate(LayoutCoord(0), -y);

	return status;
}

#endif // PAGED_MEDIA_SUPPORT

/** Helper function for calculating clipping area */

void
Content_Box::GetClippedBox(AbsoluteBoundingBox& bounding_box, const HTMLayoutProperties& props, BOOL clip_content) const
{
	LayoutCoord left = LAYOUT_COORD_MIN;
	LayoutCoord right = LAYOUT_COORD_MAX;
	LayoutCoord top = LAYOUT_COORD_MIN;
	LayoutCoord bottom = LAYOUT_COORD_MAX;

	BOOL clip_overflow = props.overflow_x != CSS_VALUE_visible;
	BOOL apply_clip = props.clip_left != CLIP_NOT_SET && IsAbsolutePositionedBox();

	if (clip_overflow || apply_clip && props.clip_left == CLIP_AUTO)
		left = LayoutCoord(0);
	if (clip_overflow || apply_clip && props.clip_right == CLIP_AUTO)
		right = GetWidth();
	if (clip_overflow || apply_clip && props.clip_top == CLIP_AUTO)
		top = LayoutCoord(0);
	if (clip_overflow || apply_clip && props.clip_bottom == CLIP_AUTO)
		bottom = GetHeight();

	/* For table content overflow_x being hidden is enough
	   - keeping other browser engines compatibility (Gecko, WebKit)
	   - in case of tables only overflow_x matters and it controls also
	   the visiblity of y overflow. */
	if (clip_content && props.overflow_x == CSS_VALUE_hidden &&
		(props.overflow_y == CSS_VALUE_hidden || GetTableContent()))
	{
		short border_top, border_left, border_right, border_bottom;

		GetBorderWidths(props, border_top, border_left, border_right, border_bottom, FALSE);

		left += LayoutCoord(border_left);
		top += LayoutCoord(border_top);
		right -= LayoutCoord(border_right);
		bottom -= LayoutCoord(border_bottom);
	}

	if (apply_clip)
	{
		if (props.clip_left != CLIP_AUTO && props.clip_left > left)
			left = props.clip_left;
		if (props.clip_right != CLIP_AUTO && props.clip_right < right)
			right = props.clip_right;
		if (props.clip_top != CLIP_AUTO && props.clip_top > top)
			top = props.clip_top;
		if (props.clip_bottom != CLIP_AUTO && props.clip_bottom < bottom)
			bottom = props.clip_bottom;
	}

	LayoutCoord width(0);
	LayoutCoord height(0);

	if (left < right)
	{
		if (right > 0 && left < right - LAYOUT_COORD_MAX)
		{
			width = LAYOUT_COORD_MAX/LayoutCoord(2);

			if ((left == LAYOUT_COORD_MIN) == (right == LAYOUT_COORD_MAX))
				left = LAYOUT_COORD_MIN/LayoutCoord(4);
			else if (right != LAYOUT_COORD_MAX)
				left = right - LAYOUT_COORD_MAX/LayoutCoord(2);
		}
		else
		{
			width = right - left;

			if (width > LAYOUT_COORD_MAX/2)
			{
				if (right != LAYOUT_COORD_MAX)
					left = right - LAYOUT_COORD_MAX/LayoutCoord(2);
				width = LAYOUT_COORD_MAX/LayoutCoord(2);
			}

		}
	}

	if (top < bottom)
	{
		/* Make sure we don't exceed INT_MAX/2 for height to get some slack on OpRects
		   which doesn't handle overflow for Bottom() (or Right()). If we need to adjust
		   the height, it's not an exact science which of the top or bottom values we
		   need to move, but we should at least reduce the auto (LONG_MAX) values first. */

		/* This condition checks if the height specified through top and bottom exceeds LONG_MAX. */
		if (bottom > 0 && top < bottom - LAYOUT_COORD_MAX)
		{
			height = LAYOUT_COORD_MAX/LayoutCoord(2);

			if ((top == LAYOUT_COORD_MIN) == (bottom == LAYOUT_COORD_MAX))
				top = LAYOUT_COORD_MIN/LayoutCoord(4);
			else if (bottom != LAYOUT_COORD_MAX)
				top = bottom - LAYOUT_COORD_MAX/LayoutCoord(2);
		}
		else
		{
			height = bottom - top;
			if (height > LAYOUT_COORD_MAX/2)
			{
				if (bottom != LAYOUT_COORD_MAX)
					top = bottom - LAYOUT_COORD_MAX/LayoutCoord(2);
				height = LAYOUT_COORD_MAX/LayoutCoord(2);
			}
		}
	}

	bounding_box.Set(left, top, LayoutCoord(width > 0 ? width : 0), height);
}

/** Update bounding box.

	Return FALSE if not changed. */

/* virtual */ void
VerticalBox::UpdateBoundingBox(const AbsoluteBoundingBox& box, BOOL skip_content)
{
	bounding_box.UnionWith(box, GetWidth(), content->GetHeight(), skip_content);
}

/** Reduce right and bottom part of relative bounding box. */

/* virtual */ void
VerticalBox::ReduceBoundingBox(LayoutCoord bottom, LayoutCoord right, LayoutCoord min_value)
{
	bounding_box.Reduce(bottom, right, min_value);
}

/** Get bounding box relative to top/left border edge of this box */

/* virtual */ void
VerticalBox::GetBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow /* = TRUE */, BOOL adjust_for_multipane /* = FALSE */, BOOL apply_clip /* = TRUE */) const
{
	OP_PROBE3(OP_PROBE_VERTICALBOX_GETBOUNDINGBOX);

	if (include_overflow)
		box.Set(bounding_box, GetWidth(), content->GetHeight());
	else
	{
		box.Set(LayoutCoord(0), LayoutCoord(0), GetWidth(), content->GetHeight());
		box.SetContentSize(box.GetWidth(), box.GetHeight());
	}

	if (adjust_for_multipane && IsColumnizable())
	{
		LayoutCoord new_x = box.GetX();
		LayoutCoord new_y = box.GetY();
		LayoutCoord new_width = box.GetWidth();
		LayoutCoord new_height = box.GetHeight() - content->GetHeight();

		OP_ASSERT(!IsTableCell());

		Container* outer_multipane_container = Container::FindOuterMultiPaneContainerOf(GetHtmlElement());
		LayoutCoord dummy_x;
		LayoutCoord dummy_y;
		RECT border_rect;

		outer_multipane_container->GetAreaAndPaneTranslation(this, dummy_x, dummy_y, &border_rect);

		if (border_rect.left < 0)
		{
			new_x += LayoutCoord(border_rect.left);
			new_width += LayoutCoord(-border_rect.left);
		}

		if (border_rect.right > 0)
			new_width += LayoutCoord(border_rect.right);

		if (border_rect.top < 0)
		{
			new_y += LayoutCoord(border_rect.top);
			new_height -= LayoutCoord(border_rect.top);
		}

		if (border_rect.bottom > 0)
			new_height += LayoutCoord(border_rect.bottom);

		box.Set(new_x, new_y, new_width, new_height);
	}
}

/** Did the width of the containing block established by this box change
 * during reflow? */

/* virtual */ BOOL
VerticalBox::HasContainingWidthChanged(BOOL include_changed_before)
{
	OP_ASSERT(IsPositionedBox());

	if (!include_changed_before)
		return FALSE; // The width never changes during reflow.

	VerticalBoxReflowState *state = GetReflowState();

	return state && state->old_width != GetWidth();
}

/** Did the height of the containing block established by this box change
 * during reflow? */

/* virtual */ BOOL
VerticalBox::HasContainingHeightChanged(BOOL include_changed_before)
{
	if (GetHtmlElement()->Type() == Markup::HTE_DOC_ROOT)
		/* For the document root, it is the initial containing block,
		   equal to the layout viewport, that establishes the
		   containing block.  The root container height is usually
		   unrelated to the viewport */

		return FALSE;

	VerticalBoxReflowState *state = GetReflowState();

	return state && (include_changed_before || state->has_auto_height) && state->old_height != GetHeight();
}

/** Get the value to use when calculating the containing block width for
	absolutely positioned children. The value will include border and
	padding. */

/* virtual */ LayoutCoord
VerticalBox::GetContainingBlockWidth()
{
	OP_ASSERT(IsPositionedBox());
	LayoutCoord width = GetWidth();

	if (ScrollableArea* scrollable = GetScrollable())
		width -= scrollable->GetVerticalScrollbarWidth();

	return width;
}

/** Get the value to use when calculating the containing block height for
	absolutely positioned children. The value will include border and
	padding. */

/* virtual */ LayoutCoord
VerticalBox::GetContainingBlockHeight()
{
	OP_ASSERT(IsPositionedBox());

	LayoutCoord height = LayoutCoord(0);
	Container *container = content->GetContainer();
	VerticalBoxReflowState *state = (VerticalBoxReflowState *)GetReflowState();

	if (!state || !state->has_auto_height || container && container->IsContainingBlockHeightCalculated())
		height = GetHeight();
	else
		height = state->old_height;

	if (ScrollableArea *sc = content->GetScrollable())
		height -= sc->GetHorizontalScrollbarWidth();
#ifdef PAGED_MEDIA_SUPPORT
	else
		if (PagedContainer* pc = content->GetPagedContainer())
			height -= pc->GetPageControlHeight();
#endif // PAGED_MEDIA_SUPPORT

	return height;
}

/** Update root translation state for this box. */

/* virtual */ BOOL
VerticalBox::UpdateRootTranslationState(BOOL changed_root_translation, LayoutCoord x, LayoutCoord y)
{
	VerticalBoxReflowState* state = GetReflowState();

	if (state && state->has_root_translation)
		if (!changed_root_translation)
			// Request move of root translation to match the new translation

			return TRUE;
		else
		{
			// Our containing block has moved, update previous root translation

			state->previous_root_x += x;
			state->previous_root_y += y;
		}

	return FALSE;
}

/** Calculate height and vertical offset for an absolute positioned
	non-replaced box */

static void
CalculateHeightAndOffset(LayoutCoord containing_content_height, LayoutCoord padding_border_height, LayoutCoord& content_height, LayoutCoord top, LayoutCoord bottom, LayoutCoord& margin_top, LayoutCoord& margin_bottom)
{
	if (content_height < 0 && content_height > CONTENT_HEIGHT_SPECIAL) //percentage value
		content_height = PercentageToLayoutCoord(LayoutFixed(-content_height), containing_content_height);

	if (top != VPOSITION_VALUE_AUTO && content_height > CONTENT_HEIGHT_SPECIAL && bottom != VPOSITION_VALUE_AUTO)
	{
		LayoutCoord margins = containing_content_height - content_height - padding_border_height - top - bottom;

		if (margin_top == MARGIN_VALUE_AUTO && margin_bottom == MARGIN_VALUE_AUTO)
		{
			margin_top = margins / LayoutCoord(2);
			margin_bottom = margins - margin_top;
		}
		else
			if (margin_top == MARGIN_VALUE_AUTO)
				margin_top = margins - margin_bottom;
			else
				if (margin_bottom == MARGIN_VALUE_AUTO)
					margin_bottom = margins - margin_top;
	}
	else
	{
		if (margin_top == MARGIN_VALUE_AUTO)
			margin_top = LayoutCoord(0);

		if (margin_bottom == MARGIN_VALUE_AUTO)
			margin_bottom = LayoutCoord(0);

		if (top != VPOSITION_VALUE_AUTO && content_height == CONTENT_HEIGHT_AUTO && bottom != VPOSITION_VALUE_AUTO)
		{
			content_height = containing_content_height - (padding_border_height + top + bottom + margin_top + margin_bottom);

			if (content_height < 0)
				content_height = LayoutCoord(0);
		}
	}
}

/** Get width available for the margin box. */

/* virtual */ LayoutCoord
AbsolutePositionedBox::GetAvailableWidth(LayoutProperties* cascade)
{
	return GetReflowState()->available_width;
}

/** Element was skipped, reposition. */

/* virtual */ BOOL
AbsolutePositionedBox::SkipZElement(LayoutInfo& info)
{
	LayoutCoord old_y = y;
	LayoutCoord old_x = x;

	/** Update root translation so that the root coordinate system has
		origin in this box.

		FIXME: Hypothetical static positioning is not supported
		properly and this may cause invalidation problems.  See
		AbsolutePositionedBox::Traverse() for which translations that
		would be necessary. */

	info.TranslateRoot(old_x, old_y);

	BOOL affected_by_containing_block = abs_packed2.containing_block_width_affects_layout || abs_packed2.containing_block_height_affects_layout || abs_packed2.containing_block_affects_position;

	if (affected_by_containing_block || abs_packed.right_aligned)
	{
		HTML_Element* html_element = GetHtmlElement();
		HTML_Element* containing_element = GetContainingElement(html_element, TRUE, IsFixedPositionedBox(TRUE));
		Box* containing_box = containing_element->GetLayoutBox();

		if (affected_by_containing_block && !CheckAffectedByContainingBlock(info, containing_box, TRUE))
			return FALSE;

		if (abs_packed.right_aligned && offset_horizontal != NO_HOFFSET)
		{
			// Right aligned boxes need to be adjusted

			x = containing_box->GetContainingBlockWidth() + offset_horizontal - content->GetWidth() - LayoutCoord(abs_packed.cached_right_border);

			if (old_x != x)
			{
				LayoutCoord width = content->GetWidth();
				LayoutCoord height = content->GetHeight();
				LayoutCoord root_x;
				LayoutCoord root_y;

				if (IsFixedPositionedBox())
				{
					root_x = info.workplace->GetLayoutViewX();
					root_y = info.workplace->GetLayoutViewY();

					if (offset_vertical == NO_VOFFSET)
					{
						LayoutCoord dummy(0);
						OP_ASSERT(html_element->Parent() && html_element->Parent()->GetLayoutBox());
						html_element->Parent()->GetLayoutBox()->GetOffsetFromAncestor(dummy, root_y, containing_element);
					}
				}
				else
					info.GetRootTranslation(root_x, root_y);

				bounding_box.Invalidate(info.visual_device, root_x, root_y, width, height);
				bounding_box.Invalidate(info.visual_device, root_x + x - old_x, root_y, width, height);
			}
		}
	}

	UpdatePosition(info);
	PropagateBottomMargins(info);

	info.TranslateRoot(- old_x, - old_y);

	return TRUE;
}

/** Do we need to calculate min/max widths of this box's content? */

/* virtual */ BOOL
AbsolutePositionedBox::NeedMinMaxWidthCalculation(LayoutProperties* cascade) const
{
	return cascade->container && cascade->container->AffectsFlexRoot();
}

/** Calculate width available for this box. */

void
AbsolutePositionedBox::CalculateAvailableWidth(const LayoutInfo& info)
{
	AbsolutePositionedBoxReflowState* state = GetReflowState();
	LayoutProperties* cascade = state->cascade;
	const HTMLayoutProperties& props = *cascade->GetProps();
	HTML_Element* containing_element = IsFixedPositionedBox() ? NULL : cascade->FindContainingElementForAbsPos(abs_packed.fixed);
	const HTMLayoutProperties* containing_props = 0;
	LayoutCoord containing_block_width;

	/* When calculating horizontal properties of an absolutely positioned
	   element, the viewport will be used as the containing block when at least
	   one of the following is true:
	   - position is fixed
	   - it is the root element

	   In all other cases we will use the nearest ancestor with position
	   different from static as the containing block. */

	if (containing_element)
	{
		Box* containing_box = containing_element->GetLayoutBox();
		short containing_border_left = 0;
		short containing_border_right = 0;
		short containing_border_top = 0;
		short containing_border_bottom = 0;

		containing_props = containing_box->GetHTMLayoutProperties();
		abs_packed2.containing_block_is_inline =
			containing_box->IsInlineBox() && !containing_box->IsInlineBlockBox()
#ifdef SVG_SUPPORT
			&& !cascade->html_element->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG)
#endif // SVG_SUPPORT
			;
		containing_box->GetBorderWidths(*containing_props, containing_border_top, containing_border_left, containing_border_right, containing_border_bottom, TRUE);
		containing_block_width = containing_box->GetContainingBlockWidth() - LayoutCoord(containing_border_left + containing_border_right);
		state->containing_box = containing_box;
		state->containing_border_left = containing_border_left;
		abs_packed.cached_right_border = containing_border_right;
	}
	else
	{
#ifndef SVG_SUPPORT
		OP_ASSERT(cascade->html_element->Type() == Markup::HTE_DOC_ROOT || abs_packed.fixed);
#else
		OP_ASSERT(cascade->html_element->Type() == Markup::HTE_DOC_ROOT ||
				  cascade->html_element->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG) ||
				  abs_packed.fixed);

		if (cascade->html_element->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG))
			containing_block_width = cascade->Pred()->GetProps()->content_width;
		else
#endif // SVG_SUPPORT
		{
			containing_block_width = info.workplace->GetLayoutViewMinWidth();

#ifdef _PRINT_SUPPORT_
			if (info.visual_device->IsPrinter())
				if (info.doc->IsInlineFrame())
					containing_block_width = LayoutCoord(info.doc->GetDocManager()->GetFrame()->GetWidth());
				else
					if (info.visual_device->GetWindow()->GetFramesPrintType() != PRINT_AS_SCREEN)
						containing_block_width = LayoutCoord(info.doc->GetCurrentPage()->GetPageWidth());
#endif // _PRINT_SUPPORT_
		}

		abs_packed.cached_right_border = 0;
	}

	if (Container* container = cascade->container)
	{
		LayoutCoord left_containing_exclude_width(containing_props ? containing_props->border.left.width : 0);

#if defined PAGED_MEDIA_SUPPORT || defined SUPPORT_TEXT_DIRECTION
		LayoutCoord right_containing_exclude_width(containing_props ? containing_props->border.right.width : 0);
#endif // PAGED_MEDIA_SUPPORT || SUPPORT_TEXT_DIRECTION

#ifdef PAGED_MEDIA_SUPPORT
		if (!containing_element || containing_element->Type() == Markup::HTE_DOC_ROOT)
			if (RootPagedContainer* root_paged_container = info.workplace->GetRootPagedContainer())
			{
				// Subtract @page margins (stored as padding)

				const HTMLayoutProperties& root_props = *root_paged_container->GetPlaceholder()->GetCascade()->GetProps();

				left_containing_exclude_width = root_props.padding_left;
				right_containing_exclude_width = root_props.padding_right;
				containing_block_width -= left_containing_exclude_width + right_containing_exclude_width;

				// And the containing block's position is offset by the @page margin as well.

				state->containing_border_left = left_containing_exclude_width;
			}
#endif // PAGED_MEDIA_SUPPORT

		state->containing_block_width = containing_block_width;

		if (props.left == HPOSITION_VALUE_AUTO && props.right == HPOSITION_VALUE_AUTO)
			// Left and right are auto. Calculate the "static position".

			state->static_position = container->GetStaticPosition(info,  GetHtmlElement(), props.GetWasInline());

		LayoutCoord available_width = containing_block_width;

		if (props.content_width == CONTENT_WIDTH_AUTO)
			// When width is auto, left and right properties may affect the used width.

			if (props.left == HPOSITION_VALUE_AUTO && props.right == HPOSITION_VALUE_AUTO)
			{
				/* Left, right and width are all auto. Subtract the distance between
				   container and containing block from available width. */

				Content_Box* container_box = container->GetPlaceholder();
				LayoutCoord dummy_y(0);
				LayoutCoord static_offset(0);

				container_box->GetOffsetFromAncestor(static_offset, dummy_y, containing_element);

#ifdef SUPPORT_TEXT_DIRECTION
				if (container_box->GetCascade()->GetProps()->direction == CSS_VALUE_rtl)
					available_width -= (containing_block_width + right_containing_exclude_width) - (static_offset + container->GetWidth()) + state->static_position;
				else
#endif // SUPPORT_TEXT_DIRECTION
					available_width -= static_offset + state->static_position - left_containing_exclude_width;
			}
			else
			{
				if (props.left != HPOSITION_VALUE_AUTO)
					available_width -= props.left;

				if (props.right != HPOSITION_VALUE_AUTO)
					available_width -= props.right;
			}

		state->available_width = MAX(available_width, LayoutCoord(0));
	}
	else
	{
		state->containing_block_width = containing_block_width;
		state->available_width = containing_block_width;
	}
}

/** Calculate X position of box. The width may be needed to calculate this. */

void
AbsolutePositionedBox::CalculateHorizontalPosition()
{
	AbsolutePositionedBoxReflowState* state = GetReflowState();
	LayoutProperties* cascade = state->cascade;

	if (cascade->container)
	{
		const HTMLayoutProperties& props = *cascade->GetProps();
		LayoutCoord margin_left = props.margin_left;
		LayoutCoord margin_right = props.margin_right;

		if (props.left != HPOSITION_VALUE_AUTO && props.right != HPOSITION_VALUE_AUTO &&
			(props.content_width != CONTENT_WIDTH_AUTO || props.max_width >= 0 || content->IsReplaced()))
		{
			// Left and right are non-auto. Width is constrained. Resolve auto horizontal margins.

			LayoutCoord margins = state->containing_block_width - GetWidth() - (props.left + props.right);

			/* If both margin-left and margin-right are auto, prevent them from
			   being resolved to something negative. If only one of them is
			   auto, on the other hand, it may be resolved to something
			   negative. So says the spec, at least the way I interpret it. */

			if (props.GetMarginLeftAuto() && props.GetMarginRightAuto())
			{
				if (margins > 0)
				{
					margin_left = margins / LayoutCoord(2);
					margin_right = margins - margin_left;
				}
			}
			else
				if (props.GetMarginLeftAuto())
					margin_left = margins - margin_right;
				else
					if (props.GetMarginRightAuto())
						margin_right = margins - margin_left;
		}

		if (props.left == HPOSITION_VALUE_AUTO && props.right == HPOSITION_VALUE_AUTO)
		{
#ifdef SUPPORT_TEXT_DIRECTION
			Container* container = cascade->container;

			if (container->GetPlaceholder()->GetCascade()->GetProps()->direction == CSS_VALUE_rtl)
			{
				/* Find the border-box size of this box's container, since the calculations
				   below all work on border-box values. */

				const HTMLayoutProperties& container_props = *container->GetPlaceholder()->GetCascade()->GetProps();
				LayoutCoord container_width = container->GetContentWidth() +
					container_props.padding_left + container_props.padding_right +
					LayoutCoord(container_props.border.left.width +	container_props.border.right.width);

				/* Set X position to the left border-edge of this box, relative to left
				   border-edge of its container. */

				x = container_width - state->static_position - margin_right - GetWidth();
				abs_packed.right_aligned = 1;
			}
			else
#endif // SUPPORT_TEXT_DIRECTION
				x = state->static_position + margin_left;

			offset_horizontal = NO_HOFFSET;
		}
		else
		{
			abs_packed.right_aligned = props.left == HPOSITION_VALUE_AUTO;

#ifdef SUPPORT_TEXT_DIRECTION
			const HTMLayoutProperties* containing_props = state->containing_box ? state->containing_box->GetHTMLayoutProperties() : 0;

			if (!abs_packed.right_aligned)
				abs_packed.right_aligned = props.right != HPOSITION_VALUE_AUTO && containing_props && containing_props->direction == CSS_VALUE_rtl;
#endif // SUPPORT_TEXT_DIRECTION

			if (abs_packed.right_aligned)
			{
				offset_horizontal = -(props.right + margin_right);
				x = offset_horizontal + state->containing_block_width - GetWidth();
			}
			else
			{
				offset_horizontal = props.left + margin_left;
				x = offset_horizontal;
			}

			x += LayoutCoord(state->containing_border_left);
		}
	}
}

LayoutCoord
AbsolutePositionedBox::GetContainingBoxHeightAndBorders(LayoutInfo& info, Container* container, short& containing_border_top, short& containing_border_bottom, BOOL& current_page_is_containing_block) const
{
	/* When calculating vertical properties of an absolutely positioned
	   element, the viewport will be used as the containing block when at least
	   one of the following is true:
	   - position is fixed and there isn't a transformed ancestor.
	   - the nearest ancestor with position different from static is the root
	     element, or there is no such ancestor
	   - in quirks mode: the nearest ancestor container is the BODY element,
	     and the nearest ancestor with position different from static is not
		 an inline element.

	   In all other cases we will use the nearest ancestor with
	   position different from static as the containing block, except
	   for when the position is fixed and there is a transformed
	   ancestor that establishes the containing block. */

	LayoutCoord containing_block_height(0);

	containing_border_top = 0;
	containing_border_bottom = 0;
	current_page_is_containing_block = FALSE;

	Box *containing_box = GetReflowState()->containing_box;
	const HTMLayoutProperties* containing_props = 0;

	if (containing_box)
	{
		if (containing_box->IsBlockBox() &&
			(containing_box->GetHtmlElement()->Type() == Markup::HTE_DOC_ROOT ||
			 (!info.hld_profile->IsInStrictMode() && container && container->GetHtmlElement()->Type() == Markup::HTE_BODY)))
			containing_box = 0;
	}

	if (containing_box)
	{
		short containing_border_left = 0;
		short containing_border_right = 0;

		containing_props = containing_box->GetHTMLayoutProperties();
		containing_box->GetBorderWidths(*containing_props, containing_border_top, containing_border_left, containing_border_right, containing_border_bottom, TRUE);
		containing_block_height = containing_box->GetContainingBlockHeight() - LayoutCoord(containing_border_top + containing_border_bottom);
	}
	else
	{
#ifdef _PRINT_SUPPORT_
		if (info.visual_device->IsPrinter() && info.visual_device->GetWindow()->GetFramesPrintType() != PRINT_AS_SCREEN && !info.doc->IsInlineFrame())
			containing_block_height = LayoutCoord(info.doc->GetCurrentPage()->GetPageHeight());
		else
#endif // _PRINT_SUPPORT_
		{
			containing_block_height = InitialContainingBlockHeight(info);

#ifdef PAGED_MEDIA_SUPPORT
			if (GetHtmlElement()->Type() == Markup::HTE_DOC_ROOT)
				containing_block_height += info.workplace->GetPageControlHeight();

			current_page_is_containing_block = info.paged_media != PAGEBREAK_OFF;
#endif
		}
	}

#ifdef PAGED_MEDIA_SUPPORT
	if (container && (!containing_box || containing_box->GetHtmlElement()->Type() == Markup::HTE_DOC_ROOT))
		if (RootPagedContainer* root_paged_container = info.workplace->GetRootPagedContainer())
		{
			// Subtract @page margins (stored as padding)

			containing_props = root_paged_container->GetPlaceholder()->GetCascade()->GetProps();
			containing_block_height -= containing_props->padding_top + containing_props->padding_bottom;
		}
#endif // PAGED_MEDIA_SUPPORT

	return containing_block_height;
}

void AbsolutePositionedBox::CalculateVerticalPosition(LayoutProperties* cascade, LayoutInfo& info)
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	Container* container = cascade->container;

	LayoutCoord top = props.top;
	LayoutCoord bottom = props.bottom;

	if (container)
	{
		if (top == VPOSITION_VALUE_AUTO && bottom == VPOSITION_VALUE_AUTO)
			top = cascade->container->GetReflowPosition(props.GetWasInline());
	}
	else
		top = bottom = LayoutCoord(0);

	LayoutCoord containing_block_height;

	short containing_border_top = 0;
	short containing_border_bottom = 0;
	BOOL current_page_is_containing_block = FALSE;

	containing_block_height = GetContainingBoxHeightAndBorders(info, container, containing_border_top, containing_border_bottom, current_page_is_containing_block);

#ifdef PAGED_MEDIA_SUPPORT
	if (current_page_is_containing_block && !abs_packed.fixed && props.top != VPOSITION_VALUE_AUTO)
		top += LayoutCoord(info.doc->GetPageTop());
#endif // PAGED_MEDIA_SUPPORT

	LayoutCoord margin_top, margin_bottom;
	LayoutCoord content_height = props.content_height; // not used here

	ResolveHeightAndVerticalMargins(cascade, content_height, margin_top, margin_bottom, info);

	// Now we are prepared, now set the hypothetical Y position
	if ((props.top == VPOSITION_VALUE_AUTO && props.bottom == VPOSITION_VALUE_AUTO) || !container)
	{
		offset_vertical = NO_VOFFSET;

		abs_packed.bottom_aligned = 0;

		y = top;

		if (container)
			y += container->CollapseHypotheticalMargins(margin_top);
		else
			y += margin_top;
	}
	else
	{
		abs_packed.bottom_aligned = top == VPOSITION_VALUE_AUTO;

		if (abs_packed.bottom_aligned)
		{
			abs_packed.cached_bottom_border = containing_border_bottom;
			offset_vertical = -(bottom + margin_bottom);

			y = containing_block_height - GetHeight() + offset_vertical - LayoutCoord(abs_packed.cached_bottom_border);

#ifdef PAGED_MEDIA_SUPPORT
			if (current_page_is_containing_block && !abs_packed.fixed)
				/* This is broken, should be positioned at bottom of page,
				   but bottom of page isn't known yet in OperaShow. */

				y += LayoutCoord(info.doc->GetPageTop());
#endif // PAGED_MEDIA_SUPPORT
		}
		else
		{
			offset_vertical = top + margin_top;
			y = offset_vertical + LayoutCoord(containing_border_top);
		}
	}

#ifdef PAGED_MEDIA_SUPPORT
	Box* containing_box = GetReflowState()->containing_box;

	if (offset_vertical != NO_VOFFSET && container &&
		(!containing_box || containing_box->GetHtmlElement()->Type() == Markup::HTE_DOC_ROOT))
		if (RootPagedContainer* root_paged_container = info.workplace->GetRootPagedContainer())
		{
			// Add @page top margin (stored as padding)

			const HTMLayoutProperties& containing_props = *root_paged_container->GetPlaceholder()->GetCascade()->GetProps();

			y += containing_props.padding_top;

			// And the containing block's position is offset by the @page margin as well.

			if (abs_packed.bottom_aligned)
				abs_packed.cached_bottom_border += containing_props.padding_bottom;
		}

	if (info.paged_media == PAGEBREAK_ON && !cascade->multipane_container)
		info.doc->RewindPage(0);
#endif // PAGED_MEDIA_SUPPORT

}

/** Calculate used vertical CSS properties (height and margins). */

/* virtual */ BOOL
AbsolutePositionedBox::ResolveHeightAndVerticalMargins(LayoutProperties* cascade, LayoutCoord& content_height, LayoutCoord& margin_top, LayoutCoord& margin_bottom, LayoutInfo& info) const
{
	const HTMLayoutProperties& props = *cascade->GetProps();
	Container* container = cascade->container;
	LayoutCoord padding_border_height(0);

	LayoutCoord top = props.top;
	LayoutCoord bottom = props.bottom;

	// Calculate height and vertical offset and margins, first try

	LayoutCoord containing_block_height;
	short containing_border_top = 0;
	short containing_border_bottom = 0;

	margin_top = props.GetMarginTopAuto() ? MARGIN_VALUE_AUTO : props.margin_top;
	margin_bottom = props.GetMarginBottomAuto() ? MARGIN_VALUE_AUTO : props.margin_bottom;

	BOOL current_page_is_containing_block = FALSE; // not used here

	if (props.box_sizing == CSS_VALUE_content_box)
		padding_border_height = props.padding_top + props.padding_bottom + LayoutCoord(props.border.top.width + props.border.bottom.width);

	containing_block_height = GetContainingBoxHeightAndBorders(info, container, containing_border_top, containing_border_bottom, current_page_is_containing_block);

	if (container)
	{
		if (top == VPOSITION_VALUE_AUTO && bottom == VPOSITION_VALUE_AUTO)
			top = cascade->container->GetReflowPosition(props.GetWasInline());
	}
	else
		top = bottom = LayoutCoord(0);

	if (content_height == CONTENT_HEIGHT_AUTO && content->IsReplaced())
		/* Set to the height resolved from intrinsic height, min-height or
		   max-height, so that auto vertical margins can be resolved. */

		content_height = content->GetHeight() - padding_border_height;

	CalculateHeightAndOffset(containing_block_height, padding_border_height, content_height, top, bottom, margin_top, margin_bottom);

	if (content_height > CONTENT_HEIGHT_SPECIAL)
		if (content_height < props.min_height || (props.max_height >= 0 && content_height > props.max_height))
		{
			// Height must be within the limits of min-height and max-height

			if (props.max_height >= 0 && content_height > props.max_height)
				content_height = props.max_height;

			if (content_height < props.min_height)
				content_height = props.min_height;

			// Recalculate offset and margins

			margin_top = props.GetMarginTopAuto() ? MARGIN_VALUE_AUTO : props.margin_top;
			margin_bottom = props.GetMarginBottomAuto() ? MARGIN_VALUE_AUTO : props.margin_bottom;

			CalculateHeightAndOffset(containing_block_height, padding_border_height, content_height, top, bottom, margin_top, margin_bottom);
		}

	return TRUE;
}

/** Update bottom aligned absolutepositioned boxes. */

/* virtual */ void
AbsolutePositionedBox::UpdateBottomAligned(LayoutInfo& info)
{
	if (abs_packed.bottom_aligned)
	{
		UpdatePosition(info);
		PropagateBottomMargins(info);
	}
}

/** Update the position. The Y position depends on the box height if it is
	bottom-aligned. */

void
AbsolutePositionedBox::UpdatePosition(LayoutInfo &info, BOOL translate)
{
	if (abs_packed.bottom_aligned)
	{
		// Bottom aligned boxes need to be adjusted

		HTML_Element* html_element = GetHtmlElement();
		HTML_Element* containing_element = GetContainingElement(html_element, TRUE, abs_packed.fixed);
		Box* containing_box = containing_element->GetLayoutBox();

		LayoutCoord old_y = y;

#ifdef PAGED_MEDIA_SUPPORT
		PageDescription* page = NULL;

		if (info.paged_media != PAGEBREAK_OFF && containing_element->Type() == Markup::HTE_DOC_ROOT)
			page = info.doc->GetPageDescription(old_y);

		if (page && page->GetPageBottom() != LONG_MAX >> 1)
			y = offset_vertical + LayoutCoord(page->GetPageBottom()) - GetHeight();
		else
#endif // PAGED_MEDIA_SUPPORT
		{
			LayoutCoord containing_content_height;

			if (containing_element->Type() == Markup::HTE_DOC_ROOT)
				containing_content_height = InitialContainingBlockHeight(info);
			else
				containing_content_height = containing_box->GetContainingBlockHeight();

			y = offset_vertical + containing_content_height - GetHeight() - LayoutCoord(abs_packed.cached_bottom_border);
		}

		if (old_y != y)
		{
			LayoutCoord width = content->GetWidth();
			LayoutCoord height = content->GetHeight();
			LayoutCoord root_x;
			LayoutCoord root_y;

			if (IsFixedPositionedBox())
			{
				root_x = info.workplace->GetLayoutViewX();
				root_y = info.workplace->GetLayoutViewY();

				if (offset_horizontal == NO_HOFFSET)
				{
					LayoutCoord dummy(0);
					OP_ASSERT(html_element->Parent() && html_element->Parent()->GetLayoutBox());
					html_element->Parent()->GetLayoutBox()->GetOffsetFromAncestor(root_x, dummy, containing_element);
				}
			}
			else
				info.GetRootTranslation(root_x, root_y);

			if (translate)
			{
				info.SetRootTranslation(root_x, y);
				info.Translate(LayoutCoord(0), y - old_y);
			}

			bounding_box.Invalidate(info.visual_device, root_x, root_y, width, height);
			bounding_box.Invalidate(info.visual_device, root_x, root_y + y - old_y, width, height);
		}
	}
}

/** Initialise reflow state. */

AbsolutePositionedBoxReflowState*
AbsolutePositionedBox::InitialiseReflowState()
{
	if (!(un.data & 1))
	{
		AbsolutePositionedBoxReflowState* state = new AbsolutePositionedBoxReflowState;

		if (state)
		{
			state->html_element = un.html_element;

#ifdef CSS_TRANSFORMS
			if (TransformContext *transform_context = GetTransformContext())
				if (!transform_context->InitialiseReflowState(state))
				{
					OP_DELETE(state);
					return NULL;
				}
#endif

			un.data = ((UINTPTR)state) | 1;
		}

		return state;
	}
	else
		return (AbsolutePositionedBoxReflowState*) (un.data & ~1);
}

/** Lay out box. */

/* virtual */ LAYST
AbsolutePositionedBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	HTML_Element* html_element = cascade->html_element;

	if (first_child && first_child != html_element)
	{
		return content->Layout(cascade, info, first_child, start_position);
	}
	else
	{
		AbsolutePositionedBoxReflowState* state = InitialiseReflowState();

		if (!state)
			return LAYOUT_OUT_OF_MEMORY;

		int reflow_content = html_element->MarkClean();
		LAYST continue_layout = LAYOUT_CONTINUE;
		Container* container = cascade->container;
		HTMLayoutProperties& props = *cascade->GetProps();

		SetClipRect(props.clip_left, props.clip_top, props.clip_right, props.clip_bottom);

		abs_packed.fixed = props.position == CSS_VALUE_fixed;
#ifdef CSS_TRANSFORMS
		abs_packed2.inside_transformed = props.IsInsideTransform();
#endif

		if (Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
			return LAYOUT_OUT_OF_MEMORY;

		state->cascade = cascade;
		state->old_x = x;
		state->old_y = y;
		state->old_width = content->GetWidth();
		state->old_height = content->GetHeight();
		state->old_bounding_box = bounding_box;

#ifdef CSS_TRANSFORMS
		if (state->transform)
			state->transform->old_transform = state->transform->transform_context->GetCurrentTransform();
#endif

		state->previous_x_translation = info.GetTranslationX();
		state->previous_y_translation = info.GetTranslationY();
		state->x_was_static = (offset_horizontal == NO_HOFFSET);
		state->y_was_static = (offset_vertical == NO_VOFFSET);

		if (props.IsInsidePositionFixed())
			info.Translate(- info.GetFixedPositionedX(), - info.GetFixedPositionedY());

#ifdef LAYOUT_YIELD_REFLOW
		if (info.force_layout_changed)
			space_manager.EnableFullBfcReflow();
#endif // LAYOUT_YIELD_REFLOW

		packed.has_absolute_width = props.content_width >= 0;

		if (cascade->multipane_container)
			/* There's an ancestor multi-pane container, but that doesn't have
			   to mean that this block is to be columnized. */

			packed.in_multipane = cascade->container == cascade->multipane_container ||
				cascade->container->GetPlaceholder()->IsColumnizable(TRUE);

		abs_packed2.fixed_left = !props.GetMarginLeftIsPercent() && !props.GetLeftIsPercent();
		abs_packed2.containing_block_width_affects_layout = props.content_width < 0 || props.GetHasHorizontalPropsWithPercent();
		abs_packed2.containing_block_height_affects_layout = props.content_height < 0 && (props.content_height != CONTENT_HEIGHT_AUTO || props.top != VPOSITION_VALUE_AUTO && props.bottom != VPOSITION_VALUE_AUTO) || props.GetHasVerticalPropsWithPercent();
		abs_packed2.containing_block_affects_position = props.left != HPOSITION_VALUE_AUTO || props.right != HPOSITION_VALUE_AUTO || props.top != VPOSITION_VALUE_AUTO || props.bottom != VPOSITION_VALUE_AUTO;
		abs_packed2.horizontal_static_inline = props.GetWasInline() && props.left == HPOSITION_VALUE_AUTO && props.right == HPOSITION_VALUE_AUTO;

		info.GetRootTranslation(state->previous_root_x, state->previous_root_y);

		state->previous_nonpos_scroll_x = info.nonpos_scroll_x;
		state->previous_nonpos_scroll_y = info.nonpos_scroll_y;
		info.nonpos_scroll_x = LayoutCoord(0);
		info.nonpos_scroll_y = LayoutCoord(0);

		if (container)
		{
			continue_layout = container->GetNewAbsolutePositionedBlock(info, cascade, this);

			if (continue_layout != LAYOUT_CONTINUE)
				return continue_layout;
		}

		CalculateAvailableWidth(info);

		switch (content->ComputeSize(cascade, info))
		{
		case OpBoolean::IS_TRUE:
			reflow_content = ELM_DIRTY;

		case OpBoolean::IS_FALSE:
			break;

		default:
			return LAYOUT_OUT_OF_MEMORY;
		}

		CalculateHorizontalPosition();
		CalculateVerticalPosition(cascade, info);

		if (abs_packed.fixed)
			info.Translate(info.GetFixedPositionedX(), info.GetFixedPositionedY());

		if (offset_horizontal == NO_HOFFSET)
			info.Translate(state->previous_nonpos_scroll_x, LayoutCoord(0));
		else
		{
			if (abs_packed.fixed)
				info.Translate(-state->previous_x_translation, LayoutCoord(0));
			else
				info.Translate(state->previous_root_x - state->previous_x_translation, LayoutCoord(0));
		}

		if (offset_vertical == NO_VOFFSET)
			info.Translate(LayoutCoord(0), state->previous_nonpos_scroll_y);
		else
			if (abs_packed.fixed)
				info.Translate(LayoutCoord(0), -state->previous_y_translation);
			else
				info.Translate(LayoutCoord(0), state->previous_root_y - state->previous_y_translation);

		if (!reflow_content)
			if (
#ifdef PAGED_MEDIA_SUPPORT
				info.paged_media == PAGEBREAK_ON ||
#endif
				info.stop_before && html_element->IsAncestorOf(info.stop_before) ||
				props.display_type == CSS_VALUE_list_item && container && container->IsReversedList())
				reflow_content = ELM_DIRTY;

		if (cascade->stacking_context)
		{
			z_element.SetZIndex(props.z_index);
			cascade->stacking_context->AddZElement(&z_element);
			if (abs_packed.fixed)
				cascade->stacking_context->SetHasFixedElement();
		}

		info.Translate(x, y);

#ifdef CSS_TRANSFORMS
		if (state->transform)
			if (!state->transform->transform_context->PushTransforms(info, state->transform->translation_state))
				return LAYOUT_OUT_OF_MEMORY;
#endif

		info.SetRootTranslation(info.GetTranslationX(), info.GetTranslationY());

		if (reflow_content)
		{
			if (!space_manager.Restart())
				return LAYOUT_OUT_OF_MEMORY;

			bounding_box.Reset(props.DecorationExtent(info.doc), LayoutCoord(0));

			RestartStackingContext();

			continue_layout = content->Layout(cascade, info);
		}
		else
			if (!cascade->SkipBranch(info, FALSE, TRUE))
				return LAYOUT_OUT_OF_MEMORY;

		return continue_layout;
	}
}

/** Update screen. */

/* virtual */ void
AbsolutePositionedBox::UpdateScreen(LayoutInfo& info)
{
	AbsolutePositionedBoxReflowState* state = GetReflowState();
	const HTMLayoutProperties& props = *state->cascade->GetProps();

	if (props.IsInsidePositionFixed())
		/* Untranslate what we did at the beginning of Layout(), or we'd pass
		   the wrong invalidation rectangles. */

		info.Translate(info.GetFixedPositionedX(), info.GetFixedPositionedY());

	BlockBox::UpdateScreen(info);

#ifdef DEBUG
	if (!state->html_element->Parent() && !info.doc->IsPrintDocument())
	{
		OP_ASSERT(!info.GetTranslationX());
		OP_ASSERT(!info.GetTranslationY());
	}
#endif // DEBUG

	info.Translate(state->previous_x_translation - info.GetTranslationX(),
				   state->previous_y_translation - info.GetTranslationY());
}

/** Signal that content may have changed. */

/* virtual */ void
AbsolutePositionedBox::SignalChange(FramesDocument* doc, BoxSignalReason reason /* = BOX_SIGNAL_REASON_UNKNOWN */)
{
	BlockBox::SignalChange(doc, reason);

	if (abs_packed2.pending_reflow_check)
	{
		abs_packed2.pending_reflow_check = 0;

		if (abs_packed.fixed)
			MarkDirty(doc, FALSE);
		else
			if (HTML_Element* containing_elm = GetContainingElement(TRUE, FALSE))
				containing_elm->GetLayoutBox()->RecalculateContainingBlock(doc, this);
	}
}

/** Set child's bottom margins in this block and propagate changes to parents. See declaration in header file for more information. */

/* virtual */ void
AbsolutePositionedBox::PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* /* bottom_margin */, BOOL /* has_inflow_content */)
{
	AbsolutePositionedBoxReflowState* state = GetReflowState();

#ifdef CSS_TRANSFORMS
	if (TransformContext *transform_context = GetTransformContext())
		if (state)
			transform_context->Finish(info, state->cascade, GetWidth(), GetHeight());
#endif

	if (IsFixedPositionedBox())
		return;

	if (state && abs_packed.bottom_aligned)
	{
		OP_ASSERT(state->cascade->container);

		state->cascade->container->PropagateBottom(info,
												   LayoutCoord(0),
												   LayoutCoord(0),
												   AbsoluteBoundingBox(),
												   PropagationOptions(PROPAGATE_ABSPOS_BOTTOM_ALIGNED, offset_horizontal == NO_HOFFSET, offset_vertical == NO_VOFFSET));
	}
	else
	{
		Container* container;
		LayoutCoord relative_x = x;
		LayoutCoord relative_y = y;

		if (state)
		{
			container = state->cascade->container;

			if (offset_horizontal != NO_HOFFSET)
				// 'relative_x' is currently relative to containing block,
				// but we need it relative to the container

				relative_x -= state->previous_x_translation - state->previous_root_x;

			if (offset_vertical != NO_VOFFSET)
				// 'relative_y' is currently relative to containing block,
				// but we need it relative to the container

				relative_y -= state->previous_y_translation - state->previous_root_y;
		}
		else
		{
			HTML_Element* html_element = GetHtmlElement();
			HTML_Element* containing_element = GetContainingElement(html_element);

			container = containing_element->GetLayoutBox()->GetContainer();

			if (offset_horizontal != NO_HOFFSET ||
				offset_vertical != NO_VOFFSET)
			{
				HTML_Element* abs_containing_element = GetContainingElement(html_element, TRUE);
				LayoutCoord root_x(0);
				LayoutCoord root_y(0);

				OP_ASSERT(html_element->Parent() && html_element->Parent()->GetLayoutBox());
				html_element->Parent()->GetLayoutBox()->GetOffsetFromAncestor(root_x, root_y, abs_containing_element);

				if (offset_horizontal != NO_HOFFSET)
					relative_x -= root_x;

				if (offset_vertical != NO_VOFFSET)
					relative_y -= root_y;
			}
		}

		/* FIXME: If we're inside a table cell, relative_y may be wrong. */

		if (container)
		{
			AbsoluteBoundingBox abs_bounding_box;

			if (!abs_packed.right_aligned && info.using_flexroot)
			{
				if (state)
				{
					// Calculate an ideal right edge that will prevent containing block overflow.

					const HTMLayoutProperties& props = *state->cascade->GetProps();

					if (props.WidthIsPercent())
						flexroot_width_required = props.content_width;
					else
					{
						LayoutCoord min_width;
						LayoutCoord dummy1;
						LayoutCoord dummy2;

						if (content->GetMinMaxWidth(min_width, dummy1, dummy2))
							flexroot_width_required = min_width;
					}

					if (props.box_sizing == CSS_VALUE_content_box)
						horizontal_border_padding = props.GetNonPercentHorizontalBorderPadding();
					else
						horizontal_border_padding = LayoutCoord(0);
				}

				LayoutCoord right(0);
				LayoutFixed percent = LayoutFixed(0);
				LayoutCoord border_padding = relative_x + horizontal_border_padding;

				if (flexroot_width_required >= 0)
					// Absolute or auto width

					right = relative_x + flexroot_width_required;
				else
					// Percentual width

					percent = LayoutFixed(-flexroot_width_required);

				if (right > 0 || percent > LayoutFixed(0))
					container->PropagateRightEdge(info, right, border_padding, percent);
			}

			GetBoundingBox(abs_bounding_box, IsOverflowVisible());

			if (abs_bounding_box.IsEmpty())
				return;

#ifdef CSS_TRANSFORMS
			if (TransformContext *transform_context = GetTransformContext())
				transform_context->ApplyTransform(abs_bounding_box);
#endif

			abs_bounding_box.Translate(relative_x, relative_y);

			/* Bottom aligned abspos box should not propagate its bounding box until
			   its containing block layout has been finished and final bottom
			   position can be determined. Containing block will ask bottom aligned
			   for bbox propagation just before finishing its own layout.

			   Special type of propagation 'PROPAGATE_ABSPOS_BOTTOM_ALIGNED' is used to
			   mark all stacking contexts between abspos and its containing element so
			   UpdateBottomAligned() can be triggered only on branches that require that. */

			container->PropagateBottom(info,
									   LayoutCoord(0),
									   LayoutCoord(0),
									   abs_bounding_box,
									   PropagationOptions(PROPAGATE_ABSPOS, offset_horizontal == NO_HOFFSET, offset_vertical == NO_VOFFSET));
		}
	}
}

/** Get bounding box relative to top/left border edge of this box. Overflow may include overflowing content as well as
absolutely positioned descendants. Returned box may be clipped to a rect defined by CSS 'clip' property. */

/* virtual */ void
AbsolutePositionedBox::GetBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow, BOOL adjust_for_multicol /* = FALSE */, BOOL apply_clip /* = TRUE */) const
{
	BlockBox::GetBoundingBox(box, include_overflow, adjust_for_multicol, apply_clip);

	if (apply_clip && clip_rect.left != CLIP_NOT_SET)
		box.ClipTo(LayoutCoord(clip_rect.left), LayoutCoord(clip_rect.top), LayoutCoord(clip_rect.right), LayoutCoord(clip_rect.bottom));
}

/** Propagate a break point caused by break properties or a spanned element. */

/* virtual */ BOOL
AbsolutePositionedBox::PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint)
{
	// Don't break inside absolutely positioned boxes.

	return TRUE;
}

/** Find the normal right edge of the rightmost absolute positioned box. */

/* virtual */ LayoutCoord
AbsolutePositionedBox::FindNormalRightAbsEdge(HLDocProfile* hld_profile, LayoutProperties* parent_cascade)
{
	LayoutCoord left_edge(0);

	if (abs_packed.right_aligned || !abs_packed2.fixed_left || !HasAbsoluteWidth() || content->IsShrinkToFit())
		return LayoutCoord(0);

	HTML_Element* html_element = GetHtmlElement();

	LayoutProperties* cascade = parent_cascade->GetCascadeAndLeftAbsEdge(hld_profile, html_element, left_edge, GetHorizontalOffset() == NO_HOFFSET);

	LayoutCoord right_edge(0);

	if (cascade)
	{
		right_edge = left_edge + cascade->GetProps()->content_width;

		if (GetHorizontalOffset() != NO_HOFFSET)
			right_edge += cascade->GetProps()->padding_left;
	}

	return right_edge;
}

/** Find the normal right edge of the rightmost absolute positioned box. */

/* virtual */ LayoutCoord
AbsoluteZRootBox::FindNormalRightAbsEdge(HLDocProfile* hld_profile, LayoutProperties* parent_cascade)
{
	LayoutCoord right_edge(0);
	BOOL is_root = GetHtmlElement()->Type() == Markup::HTE_DOC_ROOT;

	if (!is_root)
		right_edge = AbsolutePositionedBox::FindNormalRightAbsEdge(hld_profile, parent_cascade);

	if (!stacking_context.Empty())
	{
		LayoutCoord left_edge(0);
		HTML_Element* html_element = GetHtmlElement();

		if (is_root)
			parent_cascade = parent_cascade->GetCascadeAndLeftAbsEdge(hld_profile, html_element, left_edge, FALSE);

		while (parent_cascade && parent_cascade->html_element != html_element)
			parent_cascade = parent_cascade->Suc();

		if (parent_cascade)
		{
			LayoutCoord child_right_edge = stacking_context.FindNormalRightAbsEdge(hld_profile, parent_cascade);

			if (right_edge > child_right_edge)
				return right_edge;
			else
				return child_right_edge;
		}
	}

	return right_edge;
}

/** Is content of this inline box weak? */

/* virtual */ BOOL
InlineBox::IsWeakContent() const
{
	return content && content->IsWeakContent();
}

/* virtual */ BOOL
InlineBox::CountWords(LineSegment& segment, int& words) const
{
	return !IsInlineContent() || CountWordsOfChildren(segment, words);
}

/* virtual */ void
InlineBox::AdjustVirtualPos(LayoutCoord offset)
{
	x += offset;

	if (IsInlineContent())
		AdjustVirtualPosOfChildren(offset);
}

/** Get this box' baseline offset */

/* static */ LayoutCoord
InlineBox::GetBaselineOffset(const HTMLayoutProperties& props, LayoutCoord above_baseline, LayoutCoord below_baseline)
{
	switch (props.vertical_align_type)
	{
	case 0:
		return LayoutCoord(-props.vertical_align);

	case CSS_VALUE_middle:
		return (above_baseline + below_baseline) / LayoutCoord(2) - below_baseline - LayoutCoord(props.parent_font_ascent) / LayoutCoord(3);

	case CSS_VALUE_sub:
		return LayoutCoord(props.parent_font_ascent / 2);

	case CSS_VALUE_super:
		return LayoutCoord(-props.parent_font_ascent / 2);

	case CSS_VALUE_text_top:
		return above_baseline - LayoutCoord(props.parent_font_ascent); // top of this minus top of parent text

	case CSS_VALUE_text_bottom:
		return LayoutCoord(props.parent_font_descent - below_baseline + props.parent_font_internal_leading); // bottom of parent text minus bottom of this

	default:
		return LayoutCoord(0);
	}
}

/** Get height, minimum line height and base line. */

/* virtual */ void
InlineBox::GetVerticalAlignment(InlineVerticalAlignment* valign)
{
	InlineBoxReflowState* state = GetReflowState();

	OP_ASSERT(state);

	if (state)
		*valign = state->valign;
}

/** Propagate vertical alignment values up to nearest loose subtree root. */

/* virtual */ void
InlineBox::PropagateVerticalAlignment(const InlineVerticalAlignment& valign, BOOL only_bounds)
{
	InlineBoxReflowState* state = GetReflowState();

	OP_ASSERT(state);

	if (state)
	{
		BOOL modified = FALSE;

		if (state->valign.loose && !only_bounds)
		{
			if (state->valign.logical_above_baseline < valign.logical_above_baseline - state->valign.baseline)
			{
				state->valign.logical_above_baseline = valign.logical_above_baseline - state->valign.baseline;
				modified = TRUE;
			}

			if (state->valign.logical_below_baseline < valign.logical_below_baseline + state->valign.baseline)
			{
				state->valign.logical_below_baseline = valign.logical_below_baseline + state->valign.baseline;
				modified = TRUE;
			}

			if (state->valign.logical_above_baseline_nonpercent < valign.logical_above_baseline_nonpercent - state->valign.baseline_nonpercent)
			{
				state->valign.logical_above_baseline_nonpercent = valign.logical_above_baseline_nonpercent - state->valign.baseline_nonpercent;
				modified = TRUE;
			}

			if (state->valign.logical_below_baseline_nonpercent < valign.logical_below_baseline_nonpercent + state->valign.baseline_nonpercent)
			{
				state->valign.logical_below_baseline_nonpercent = valign.logical_below_baseline_nonpercent + state->valign.baseline_nonpercent;
				modified = TRUE;
			}
		}

		if (state->valign.box_above_baseline < valign.box_above_baseline - state->valign.baseline)
		{
			state->valign.box_above_baseline = valign.box_above_baseline - state->valign.baseline;
			modified = TRUE;
		}

		if (state->valign.box_below_baseline < valign.box_below_baseline + state->valign.baseline)
		{
			state->valign.box_below_baseline = valign.box_below_baseline + state->valign.baseline;
			modified = TRUE;
		}

		if (modified)
		{
			HTML_Element* parent = state->cascade->FindParent();

			OP_ASSERT(parent);

			if (state->valign.loose_root)
				only_bounds = TRUE;

			if (parent)
				parent->GetLayoutBox()->PropagateVerticalAlignment(state->valign, only_bounds);
		}
	}
}

/** Propagate a break point caused by break properties or a spanned element. */

/* virtual */ BOOL
InlineBox::PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint)
{
	OP_ASSERT(!GetContainer());
	OP_ASSERT(!GetTableContent());

	return GetCascade()->container->PropagateBreakpoint(virtual_y, break_type, breakpoint);
}

/** Update bounding box.

	Return FALSE if not changed. */

/* virtual */ void
InlineBox::UpdateBoundingBox(const AbsoluteBoundingBox& box, BOOL skip_content)
{
	/* If the state is NULL here, it might cause invalidation problems
	   because the bounding box is not updated correctly. That might happen
	   in SkipBranch for instance where an absolute positioned child is not
	   able to contribute to the Line's bounding box if the Container/Line is
	   not being reflowed. See CORE-25942. */

	InlineBoxReflowState* state = GetReflowState();
	if (state)
	{
		LayoutCoord box_width = content->GetWidth();
		LayoutCoord box_height = content->GetHeight();

		if (skip_content)
		{
			if (box.GetX() + box.GetWidth() > state->abspos_pending_bbox_right)
				state->abspos_pending_bbox_right = box.GetX() + box.GetWidth();

			if (box.GetY() + box.GetHeight() > state->abspos_pending_bbox_bottom)
				state->abspos_pending_bbox_bottom = box.GetY() + box.GetHeight();
		}

		state->bounding_box.UnionWith(box, box_width, box_height, skip_content);
	}
}

/** Reduce right and bottom part of relative bounding box. */

/* virtual */ void
InlineBox::ReduceBoundingBox(LayoutCoord bottom, LayoutCoord right, LayoutCoord min_value)
{
	/* Don't bother to check box type here, although reducing bounding box size
	   is only relevant for inline-block and inline-table. */

	InlineBoxReflowState* state = GetReflowState();

	state->bounding_box.Reduce(bottom, right, min_value);
}

/* virtual */
Content_Box::~Content_Box()
{
	GetHtmlElement()->SetIsInSelection(FALSE);
	OP_DELETE(content);
}

/** Update screen. */

/* virtual */ void
BlockBox::UpdateScreen(LayoutInfo& info)
{
	content->UpdateScreen(info);

	VerticalBoxReflowState* state = GetReflowState();

	if (!state)
		return;

	VisualDevice* visual_device = info.visual_device;
	LayoutCoord width = content->GetWidth();
	LayoutCoord height = content->GetHeight();
	BOOL width_changed = state->old_width != width;
	BOOL height_changed = state->old_height != height;
	BOOL dimension_changed = width_changed || height_changed;
	BOOL pos_changed = state->old_y != y || state->old_x != x;
	BOOL layout_changed = pos_changed || dimension_changed;
	BOOL any_changed = layout_changed;

#ifdef CSS_TRANSFORMS
	TransformContext *transform_context = state->transform ? state->transform->transform_context : NULL;
#endif

	if (!dimension_changed && IsOverflowVisible() && state->old_bounding_box != bounding_box)
		/* Changed bounding box means that the dimension has changed in terms
		   of invalidation, but not in terms of layout_changed. */

		dimension_changed = any_changed = TRUE;

	HTML_Element* html_element = state->html_element;
	LayoutProperties* cascade = state->cascade;

	if (layout_changed && cascade->space_manager && !cascade->space_manager->IsFullBfcReflowEnabled() && !IsAbsolutePositionedBox())
		/* Position or size of this block changed. We need to reflow all lines in the remaining
		   part of the block formatting context, because they may be affected by floats. */

		cascade->space_manager->EnableFullBfcReflow();

	if (cascade->container && cascade->container->IsCalculatingMinMaxWidths() &&
		!cascade->space_manager->IsFullBfcMinMaxCalculationEnabled() &&
		!IsAbsolutePositionedBox())
	{
		BOOL changed = state->old_min_y != GetMinY();

		if (!changed)
		{
			LayoutCoord dummy, max_width;

			if (content->GetMinMaxWidth(dummy, dummy, max_width))
				if (max_width != state->old_max_width || GetMinY() != state->old_min_y || content->GetMinHeight() != state->old_min_height)
					changed = TRUE;
		}

		if (changed)
			/* Minimum height, maximum width, or something that may affect maximum width in sibling
			   content, has changed. Need to recalculate min/max widths of all lines in the
			   remaining part of the block formatting context, because they may be affected by
			   floats. */

			cascade->space_manager->EnableFullBfcMinMaxCalculation();
	}

	CheckAbsPosDescendants(info);

#ifdef CSS_TRANSFORMS
	if (transform_context)
		transform_context->PopTransforms(info, state->transform->translation_state);
#endif

	info.Translate(-x, -y);

#ifdef CSS_TRANSFORMS
	BOOL transform_changed = FALSE;

	if (transform_context)
		transform_changed = state->transform->old_transform != transform_context->GetCurrentTransform();

	any_changed = any_changed || transform_changed;
	dimension_changed = dimension_changed || transform_changed;
#endif

	if (any_changed)
	{
		BOOL invalidation_handled = FALSE;

		if (html_element->Type() == Markup::HTE_DOC_ROOT)
		{
			const BgImages &bg_images = info.workplace->GetDocRootProperties().GetBackgroundImages();

			BOOL bg_percentage_x_pos, bg_percentage_y_pos;
			bg_images.HasPercentagePositions(bg_percentage_x_pos, bg_percentage_y_pos);

			BOOL has_background_image = !bg_images.IsEmpty();
			BOOL percentage_bg_affected = ((state->old_width != width && bg_percentage_x_pos) ||
										   (state->old_height != height && bg_percentage_y_pos));

			if (has_background_image && percentage_bg_affected)
			{
				// update all
				visual_device->UpdateRelative(x, y, width, height);
				invalidation_handled = TRUE;
			}
		}

		if (!invalidation_handled)
		{
			if (pos_changed || dimension_changed
#ifdef CSS_TRANSFORMS
				|| (transform_context != NULL)
#endif
				)
			{
				int old_x = state->old_x - state->old_bounding_box.Left();
				int old_y = state->old_y - state->old_bounding_box.Top();

				if (IsAbsolutePositionedBox())
				{
					/* If the new positioning for an absolute or fixed positioned box changed coordinate system
					   because it changed positioning to/from hypothetical static position. We need to adjust the
					   old invalidation rectangle according to the current translation. */

					AbsolutePositionedBox* abs_box = (AbsolutePositionedBox*) this;
					AbsolutePositionedBoxReflowState* abs_state = (AbsolutePositionedBoxReflowState*) state;

					if (abs_state->x_was_static != (abs_box->GetHorizontalOffset() == NO_HOFFSET))
					{
						LayoutCoord adjust_x = abs_state->previous_x_translation;

						if (!IsFixedPositionedBox())
							adjust_x -= abs_state->previous_root_x;

						if (abs_box->GetHorizontalOffset() == NO_HOFFSET)
							old_x -= adjust_x;
						else
							old_x += adjust_x;
					}
					if (abs_state->y_was_static != (abs_box->GetVerticalOffset() == NO_VOFFSET))
					{
						LayoutCoord adjust_y = abs_state->previous_y_translation;

						if (!IsFixedPositionedBox())
							adjust_y -= abs_state->previous_root_y;

						if (abs_box->GetVerticalOffset() == NO_VOFFSET)
							old_y -= adjust_y;
						else
							old_y += adjust_y;
					}
				}

#ifdef CSS_TRANSFORMS
				if (transform_context)
				{
					visual_device->Translate(old_x, old_y);
					visual_device->PushTransform(state->transform->old_transform);

					visual_device->UpdateRelative(LayoutCoord(0), LayoutCoord(0),
												  state->old_bounding_box.TotalWidth(state->old_width),
												  state->old_bounding_box.TotalHeight(state->old_height));

					visual_device->PopTransform();
					visual_device->Translate(x - old_x, y - old_y);

					visual_device->PushTransform(transform_context->GetCurrentTransform());

					bounding_box.Invalidate(visual_device, LayoutCoord(0), LayoutCoord(0), width, height);

					visual_device->PopTransform();
					visual_device->Translate(-x, -y);
				}
				else
#endif
				{
					visual_device->UpdateRelative(old_x, old_y,
												  state->old_bounding_box.TotalWidth(state->old_width),
												  state->old_bounding_box.TotalHeight(state->old_height));

					bounding_box.Invalidate(visual_device, x, y, width, height);
				}
			}
			else
			{
				LayoutCoord new_right = x + width;
				LayoutCoord new_bottom = y + height;

				LayoutCoord old_right = state->old_x + state->old_width;
				LayoutCoord old_bottom = state->old_y + state->old_height;

				if (new_bottom != old_bottom)
				{
					// Box changed in height; update area below box

					LayoutCoord update_height = old_bottom - new_bottom;
					LayoutCoord update_from;

					if (update_height < 0)
					{
						update_from = old_bottom;
						update_height = -update_height;
					}
					else
						update_from = new_bottom;

					visual_device->UpdateRelative(x,
												  update_from,
												  MAX(state->old_width, width),
												  update_height);
				}

				if (old_right != new_right)
				{
					// Box changed in width; update area to the right of box

					LayoutCoord update_width = old_right - new_right;
					LayoutCoord update_from;

					if (update_width < 0)
					{
						update_from = old_right;
						update_width = -update_width;
					}
					else
						update_from = new_right;

					visual_device->UpdateRelative(update_from,
												  y,
												  update_width,
												  MAX(state->old_height, height));
				}
			}
		}

		state->old_x = x;
		state->old_y = y;
		state->old_width = width;
		state->old_height = height;
		state->old_bounding_box = bounding_box;
	}
	else if (state->needs_update)
		/* Invalidation was skipped before layout so in case nothing
		   changed in box's layout call invalidate now */

		bounding_box.Invalidate(visual_device, x, y, width, height);
}

/** Invalidate the bounding box */

void
BlockBox::InvalidateBoundingBox(LayoutInfo &info, LayoutCoord offset_x, LayoutCoord offset_y) const
{
	info.Translate(x + offset_x, y + offset_y);

#ifdef CSS_TRANSFORMS
	TranslationState translation_state;
	const TransformContext *transform_context = GetTransformContext();

	if (transform_context)
		if (!transform_context->PushTransforms(info, translation_state))
		{
			info.Translate(- x - offset_x, - y - offset_y);
			return;
		}

#endif

	bounding_box.Invalidate(info.visual_device, LayoutCoord(0), LayoutCoord(0), content->GetWidth(), content->GetHeight());

#ifdef CSS_TRANSFORMS
	if (transform_context)
		transform_context->PopTransforms(info, translation_state);
#endif

	info.Translate(- x - offset_x, - y - offset_y);
}

/** Invalidate the screen area that the box uses. */

/* virtual */ void
BlockBox::Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const
{
	HTML_Element* element = GetHtmlElement();
	if (element->IsExtraDirty() || !element->IsDirty() || !element->NeedsUpdate())
		/* Element is dirty and also its area needs to be repainted. However,
		   at this stage element's position may be wrong due to future margins
		   that may alter position of any of box's ancestors. Therefore, while
		   box's coordinates (relative to its parent) remain unchanged by margins,
		   its screen position changes after margin propagation. Invalidating now
		   may cause repaint issues therefore we'll defer invalidation until
		   final box screen position is known. */

		InvalidateBoundingBox(info, LayoutCoord(0), LayoutCoord(0));
}

/** Invalidate the screen area that the box uses. */

/* virtual */ void
AbsolutePositionedBox::Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const
{
	LayoutCoord root_x;
	LayoutCoord root_y;

	if (IsFixedPositionedBox())
	{
		root_x = LayoutCoord(0);
		root_y = LayoutCoord(0);
	}
	else
		info.GetRootTranslation(root_x, root_y);

	if (offset_horizontal != NO_HOFFSET)
		root_x -= info.GetTranslationX();
	else
		root_x = LayoutCoord(0);

	if (offset_vertical != NO_VOFFSET)
		root_y -= info.GetTranslationY();
	else
		root_y = LayoutCoord(0);

	if (IsFixedPositionedBox())
	{
		root_x += info.workplace->GetLayoutViewX();
		root_y += info.workplace->GetLayoutViewY();
	}

	InvalidateBoundingBox(info, root_x, root_y);
}

/** The size or position of the containing block has changed, and this
	absolutely positioned box needs to know (and probably reflow). */

void
AbsolutePositionedBox::ContainingBlockChanged(LayoutInfo &info, BOOL width_changed, BOOL height_changed)
{
	if (width_changed && abs_packed2.containing_block_width_affects_layout ||
		height_changed && abs_packed2.containing_block_height_affects_layout)
	{
		abs_packed2.pending_reflow_check = 1;
		info.workplace->SetReflowElement(GetHtmlElement());
	}
}

/** Check if this box is affected by the size or position of its containing
	block. */

BOOL
AbsolutePositionedBox::CheckAffectedByContainingBlock(LayoutInfo &info, Box* containing_box, BOOL skipped)
{
	if (containing_box->IsBlockBox() || containing_box->IsTableCaption() || containing_box->IsFlexItemBox())
		/* For blocks, table-captions and flex items, we check for relevant changes
		   when the containing block has been laid out. A list of potentially affected
		   descendants is kept in the reflow state of the containing block box. */

		return ((VerticalBox*) containing_box)->AddAffectedAbsPosDescendant(this, skipped);
	else
	{
		OP_ASSERT(containing_box->IsInlineBox() || containing_box->IsTableRow() || containing_box->IsTableRowGroup() || containing_box->IsTableCell());

		/* If the containing block is an inline box, the size of the containing block
		   that it establishes is not known until its container has been laid out. If
		   the containing block is a table-cell, table-row or table-row-group, the
		   size of the containing block that is established is not known until its
		   table has been laid out (non-auto height tables may stretch rows when
		   finishing layout). So, check for relevant changes later, in
		   SignalChange(). */

		if (abs_packed2.containing_block_width_affects_layout || abs_packed2.containing_block_height_affects_layout || abs_packed2.containing_block_affects_position)
		{
			abs_packed2.pending_reflow_check = 1;
			info.workplace->SetReflowElement(GetHtmlElement());
		}
	}

	return TRUE;
}

/** Distribute content into columns. */

/* virtual */ BOOL
AbsolutePositionedBox::Columnize(Columnizer& columnizer, Container* container)
{
	return TRUE;
}

/** Figure out to which column(s) or spanned element a box belongs. */

/* virtual */ void
AbsolutePositionedBox::FindColumn(ColumnFinder& cf, Container* container)
{
	if (cf.GetTarget() == this)
	{
		/* Note that this isn't correct (but better than nothing). If this
		   absolutely positioned box is the start element of the next column,
		   we fail to catch that. And the coordinates will be off in any
		   case. */

		cf.SetPosition(LayoutCoord(0));
		cf.SetBoxStartFound();
		cf.SetPosition(GetLayoutHeight());
		cf.SetBoxEndFound();
	}
}

/** Get height of content. */

/* virtual */ LayoutCoord
BlockBox::GetLayoutHeight() const
{
	return VerticalBox::GetHeight();
}

/** Get height if maximum width is satisfied. */

/* virtual */ LayoutCoord
BlockBox::GetLayoutMinHeight() const
{
	return content->GetMinHeight();
}

/** Finish reflowing box. */

/* virtual */ LAYST
BlockBox::FinishLayout(LayoutInfo& info)
{
	VerticalBoxReflowState* state = GetReflowState();
	LayoutProperties *cascade = state->cascade;
	Container* container = cascade->container;
	const HTMLayoutProperties& props = *cascade->GetProps();

	if (content->IsRunIn())
	{
		/* This is a run-in or compact; mark as pending for later
		   block boxes. */

		container->SetPendingRunIn(this, info, props.margin_left + props.margin_right + props.padding_left + props.padding_right + LayoutCoord(props.border.left.width + props.border.right.width));
	}

	switch (content->FinishLayout(info))
	{
	case LAYOUT_END_FIRST_LINE:
		return LAYOUT_END_FIRST_LINE;

	case LAYOUT_OUT_OF_MEMORY:
		return LAYOUT_OUT_OF_MEMORY;
	}

	if (SpaceManager* local_space_manager = GetLocalSpaceManager())
		local_space_manager->FinishLayout();

	if (StackingContext* local_stacking_context = GetLocalStackingContext())
		local_stacking_context->FinishLayout(cascade);

	UpdateScreen(info);

	if (IsPositionedBox())
	{
		info.SetRootTranslation(state->previous_root_x, state->previous_root_y);

		if (IsAbsolutePositionedBox())
			if (HTML_Element* containing_element = GetContainingElement(cascade->html_element, TRUE, IsFixedPositionedBox(TRUE)))
				if (!((AbsolutePositionedBox *)this)->CheckAffectedByContainingBlock(info, containing_element->GetLayoutBox(), FALSE))
					return LAYOUT_OUT_OF_MEMORY;

		info.nonpos_scroll_x = state->previous_nonpos_scroll_x;
		info.nonpos_scroll_y = state->previous_nonpos_scroll_y;
	}

	if (packed.column_spanned)
		// Don't let the spanned box's bottom margin collapse with anything.

		container->CommitMargins();

#ifdef PAGED_MEDIA_SUPPORT
	if (info.paged_media == PAGEBREAK_ON && !cascade->multipane_container)
		if (IsFloatingBox() || IsAbsolutePositionedBox())
		{
			if (cascade->container)
				// Rewind to previous position of container

				info.doc->RewindPage(cascade->container->GetReflowPosition());
		}
		else
			if (content->GetTableContent())
				// Rewind to bottom of table

				info.doc->RewindPage(GetY() + GetHeight());
#endif // PAGED_MEDIA_SUPPORT

	if (props.display_type == CSS_VALUE_list_item && !HasListMarkerBox() && container)
		/* Need to process the list number, since if the list marker was not
		   displayed, the number was not processed during marker box layout,
		   but it should still affect numbering of later list items. */

		container->GetNewListItemValue(cascade->html_element);

#ifndef LAYOUT_YIELD_REFLOW
	OP_ASSERT(!info.stop_before || !cascade->html_element->IsAncestorOf(info.stop_before));
#endif

	DeleteReflowState();

	return LAYOUT_CONTINUE;
}

FloatingBox::~FloatingBox()
{
	/* Removing a float may affect the block formatting context in ways we will
	   be unable to detect during reflow, and reflow might then make incorrect
	   assumptions and skip layout of certain blocks that _seem to_ be
	   unchanged and unaffected. Therefore we will here make sure that all
	   blocks in the entire block formatting context are reflowed in the next
	   pass. */

	link.ForceFullBfcReflow();

	link.Out();
	DeleteFloatReflowCache();
}

#ifdef PAGED_MEDIA_SUPPORT

/** Insert a page break. */

/* virtual */ BREAKST
FloatingBox::InsertPageBreak(LayoutInfo& info, int strength)
{
	/* Stop looking for a page break here, just causes problems.

	Container* container = GetCascade()->container;

	if (container)
	{
		BREAKST status;

		info.Translate(0, -y);
		status = container->InsertPageBreak(info, strength, TRUE);
		info.Translate(0, y);

		return status;
	}*/

	return BREAK_NOT_FOUND;
}

#endif // PAGED_MEDIA_SUPPORT

/** Get the lowest bottom outer edge of this float and preceding floats. */

LayoutCoord
FloatingBox::GetLowestFloatBfcBottom(CSSValue float_types)
{
	LayoutCoord bottom = LAYOUT_COORD_MIN;

	OP_ASSERT(float_types == CSS_VALUE_left || float_types == CSS_VALUE_right || float_types == CSS_VALUE_both);
	OP_ASSERT(!GetFloatReflowCache()->invalid);

	switch (float_types)
	{
	case CSS_VALUE_both:
	case CSS_VALUE_left:
		bottom = float_reflow_cache->left_floats_bottom;
		if (float_types == CSS_VALUE_left)
			break;

		// fall-through

	case CSS_VALUE_right:
		if (bottom < float_reflow_cache->right_floats_bottom)
			bottom = float_reflow_cache->right_floats_bottom;
		break;
	};

	return bottom;
}

/** Get the lowest minimum bottom outer edge of this float and preceding floats. */

LayoutCoord
FloatingBox::GetLowestFloatBfcMinBottom(CSSValue float_types)
{
	LayoutCoord min_bottom = LAYOUT_COORD_MIN;

	OP_ASSERT(float_types == CSS_VALUE_left || float_types == CSS_VALUE_right || float_types == CSS_VALUE_both);
	OP_ASSERT(!GetFloatReflowCache()->invalid);

	switch (float_types)
	{
	case CSS_VALUE_both:
	case CSS_VALUE_left:
		min_bottom = float_reflow_cache->left_floats_min_bottom;
		if (float_types == CSS_VALUE_left)
			break;

		// fall-through

	case CSS_VALUE_right:
		if (min_bottom < float_reflow_cache->right_floats_min_bottom)
			min_bottom = float_reflow_cache->right_floats_min_bottom;
		break;
	};

	return min_bottom;
}

/** Get the highest maximum width among this float and preceding floats.

	@param float_type CSS_VALUE_left or CSS_VALUE_right */

LayoutCoord
FloatingBox::GetHighestAccumulatedMaxWidth(CSSValue float_type)
{
	FloatReflowCache* cache = GetFloatReflowCache();

	OP_ASSERT(float_type == CSS_VALUE_left || float_type == CSS_VALUE_right);
	OP_ASSERT(!cache->invalid);

	if (float_type == CSS_VALUE_left)
		return cache->highest_left_acc_max_width;
	else
		return cache->highest_right_acc_max_width;
}

/** Update the reflow cache of this float (and preceding floats with invalid cache, if any) if invalid. */

void
FloatingBox::UpdateFloatReflowCache()
{
	if (!GetFloatReflowCache()->invalid)
		return;

	FLink* flink = &link;

	while (flink->Pred() && flink->Pred()->float_box->GetFloatReflowCache()->invalid)
		flink = flink->Pred();

	do
	{
		FLink* pred = flink->Pred();
		FloatingBox* box = flink->float_box;
		FloatReflowCache* cache = box->GetFloatReflowCache();
		LayoutCoord bottom = box->GetBfcY() - box->GetMarginTop() + box->GetHeightAndMargins();
		LayoutCoord min_bottom = box->GetBfcMinY() - box->GetMarginTop(TRUE) + box->GetMinHeightAndMargins();

		if (pred)
		{
			FloatingBox* pred_box = pred->float_box;
			FloatReflowCache* pred_cache = pred_box->GetFloatReflowCache();

			cache->left_floats_bottom = pred_cache->left_floats_bottom;
			cache->right_floats_bottom = pred_cache->right_floats_bottom;
			cache->left_floats_min_bottom = pred_cache->left_floats_min_bottom;
			cache->right_floats_min_bottom = pred_cache->right_floats_min_bottom;
			cache->highest_left_acc_max_width = pred_cache->highest_left_acc_max_width;
			cache->highest_right_acc_max_width = pred_cache->highest_right_acc_max_width;
		}
		else
		{
			cache->left_floats_bottom = LAYOUT_COORD_MIN;
			cache->right_floats_bottom = LAYOUT_COORD_MIN;
			cache->left_floats_min_bottom = LAYOUT_COORD_MIN;
			cache->right_floats_min_bottom = LAYOUT_COORD_MIN;
			cache->highest_left_acc_max_width = LayoutCoord(0);
			cache->highest_right_acc_max_width = LayoutCoord(0);
		}

		LayoutCoord acc_max_width = GetAccumulatedMaxWidth();

		if (box->float_packed.left)
		{
			if (cache->left_floats_bottom < bottom)
				cache->left_floats_bottom = bottom;

			if (cache->left_floats_min_bottom < min_bottom)
				cache->left_floats_min_bottom = min_bottom;

			if (cache->highest_left_acc_max_width < acc_max_width)
				cache->highest_left_acc_max_width = acc_max_width;
		}
		else
		{
			if (cache->right_floats_bottom < bottom)
				cache->right_floats_bottom = bottom;

			if (cache->right_floats_min_bottom < min_bottom)
				cache->right_floats_min_bottom = min_bottom;

			if (cache->highest_right_acc_max_width < acc_max_width)
				cache->highest_right_acc_max_width = acc_max_width;
		}

		cache->invalid = FALSE;

		flink = flink->Suc();
	}
	while (flink && flink->Pred() != &link);
}

/** Initialise the reflow cache for this floating box. See FloatReflowCache. */

BOOL
FloatingBox::InitialiseFloatReflowCache()
{
	if (!float_packed.has_float_reflow_cache || !float_reflow_cache)
	{
		float_reflow_cache = new FloatReflowCache;
		if (!float_reflow_cache)
			return FALSE;

		float_packed.has_float_reflow_cache = 1;
	}

	float_reflow_cache->invalid = TRUE;

	return TRUE;
}

/** Delete the reflow state for this floating box. See FloatReflowCache. */

void
FloatingBox::DeleteFloatReflowCache()
{
	if (float_packed.has_float_reflow_cache)
	{
		OP_DELETE(float_reflow_cache);
		float_packed.has_float_reflow_cache = 0;

		next_float = NULL;
	}
}

/** Mark the reflow cache of this float, and all succeeding floats,
	invalid/dirty, so that it is updated before next time it needs to
	be accessed. */

void
FloatingBox::InvalidateFloatReflowCache()
{
	for (FLink* flink = &link; flink; flink = flink->Suc())
	{
		FloatReflowCache* cache = flink->float_box->GetFloatReflowCache();
		if (cache)
		{
			if (cache->invalid)
				break;

			cache->invalid = TRUE;
		}
	}
}

/** Get total maximum width of this float, plus all floats to the left (if
	this is a left-float) or to the right (if this is a right-float) of
	this float, if the maximum width of the block formatting context in
	which this float lives is satisfied. */

LayoutCoord
FloatingBox::GetAccumulatedMaxWidth() const
{
	LayoutCoord min_width;
	LayoutCoord normal_min_width;
	LayoutCoord max_width;
	LayoutCoord accumulated_max_width = prev_accumulated_max_width + nonpercent_horizontal_margin;

	if (content->GetMinMaxWidth(min_width, normal_min_width, max_width))
		accumulated_max_width += max_width;

	return MAX(LayoutCoord(0), accumulated_max_width);
}

/** Skip layout of this float. */

void
FloatingBox::SkipLayout(LayoutInfo& info)
{
	Container* container = GetContainingElement()->GetLayoutBox()->GetContainer();
	FloatReflowCache* cache = GetFloatReflowCache();
	LayoutCoord container_bfc_x(0);
	LayoutCoord container_bfc_y(0);
	LayoutCoord container_bfc_min_y(0);

	LayoutCoord left;
	LayoutCoord top;

	GetRelativeOffsets(left, top);

	container->GetBfcOffsets(container_bfc_x, container_bfc_y, container_bfc_min_y);

	cache->bfc_x = container_bfc_x + x - left;
	cache->bfc_y = container_bfc_y + y - top;
	cache->bfc_min_y = container_bfc_min_y + min_y;

	PropagateBottomMargins(info);
}

/** Lay out box. */

/* virtual */ LAYST
FloatingBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	HTML_Element* html_element = cascade->html_element;

	if (first_child && first_child != html_element)
	{
		return content->Layout(cascade, info, first_child, start_position);
	}
	else
	{
		VerticalBoxReflowState* state = InitialiseReflowState();

		if (!state || !InitialiseFloatReflowCache())
			return LAYOUT_OUT_OF_MEMORY;

		state->needs_update = html_element->NeedsUpdate();

		int reflow_content = html_element->MarkClean();
		Container* container = cascade->container;
		const HTMLayoutProperties& props = *cascade->GetProps();
		LAYST continue_layout;

		if (Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
			return LAYOUT_OUT_OF_MEMORY;

		state->cascade = cascade;
		state->old_x = x;
		state->old_y = y;
		state->old_width = content->GetWidth();
		state->old_height = content->GetHeight();
		state->old_bounding_box = bounding_box;

#ifdef CSS_TRANSFORMS
		if (state->transform)
			state->transform->old_transform = state->transform->transform_context->GetCurrentTransform();
#endif

		if (container->IsCalculatingMinMaxWidths() && !cascade->space_manager->IsFullBfcMinMaxCalculationEnabled())
		{
			/* The container is calculating min/max widths. We need to pay special attention to
			   changes then. If the maximum width or minimum height of this block changes during
			   reflow, we need to enable full block formatting context min/max calculation. */

			LayoutCoord dummy;

			content->GetMinMaxWidth(dummy, dummy, state->old_max_width);
			state->old_min_height = content->GetMinHeight();
			state->old_min_y = GetMinY();
		}

#ifdef LAYOUT_YIELD_REFLOW
		if (info.force_layout_changed)
			space_manager.EnableFullBfcReflow();
#endif // LAYOUT_YIELD_REFLOW

		OP_ASSERT(props.float_type == CSS_VALUE_left || props.float_type == CSS_VALUE_right);
		float_packed.left = props.float_type == CSS_VALUE_left;

		continue_layout = container->GetNewFloatStage1(info, cascade, this);

		if (continue_layout != LAYOUT_CONTINUE)
			return continue_layout;

		info.Translate(x, y);

		switch (content->ComputeSize(cascade, info))
		{
		case OpBoolean::IS_TRUE:
			reflow_content = ELM_DIRTY;

		case OpBoolean::IS_FALSE:
			break;

		default:
			return LAYOUT_OUT_OF_MEMORY;
		}

		if (cascade->multipane_container)
			/* There's an ancestor multi-pane container, but that doesn't have
			   to mean that this block is to be columnized. */

			packed.in_multipane = cascade->container == cascade->multipane_container ||
				cascade->container->GetPlaceholder()->IsColumnizable(TRUE);

		margin_top = props.margin_top;
		margin_bottom = props.margin_bottom;
		float_packed.margin_top_is_percent = props.GetMarginTopIsPercent();
		float_packed.margin_bottom_is_percent = props.GetMarginBottomIsPercent();

		float_edge = LayoutCoord(0);
		nonpercent_horizontal_margin =
			(props.GetMarginLeftIsPercent() ? LayoutCoord(0) : props.margin_left) +
			(props.GetMarginRightIsPercent() ? LayoutCoord(0) : props.margin_right);

		container->GetNewFloatStage2(info, cascade, this);

		FloatReflowCache* cache = GetFloatReflowCache();

		cache->margin_left = props.margin_left;
		cache->margin_right = props.margin_right;

		if (IsPositionedBox())
		{
			info.GetRootTranslation(state->previous_root_x, state->previous_root_y);
			info.SetRootTranslation(info.GetTranslationX(), info.GetTranslationY());
		}

#ifdef CSS_TRANSFORMS
		if (state->transform)
			if (!state->transform->transform_context->PushTransforms(info, state->transform->translation_state))
				return LAYOUT_OUT_OF_MEMORY;
#endif

		if (!reflow_content)
			if (
#ifdef PAGED_MEDIA_SUPPORT
				info.paged_media == PAGEBREAK_ON ||
#endif
				info.stop_before && html_element->IsAncestorOf(info.stop_before) ||
				props.display_type == CSS_VALUE_list_item && cascade->container->IsReversedList())
				reflow_content = ELM_DIRTY;

		if (reflow_content)
		{
			RestartStackingContext();

			if (!space_manager.Restart())
				return LAYOUT_OUT_OF_MEMORY;

			if (container->IsCalculatingMinMaxWidths() && content->IsShrinkToFit())
				content->ClearMinMaxWidth();

			bounding_box.Reset(props.DecorationExtent(info.doc), LayoutCoord(0));

			return content->Layout(cascade, info);
		}
		else
			if (!cascade->SkipBranch(info, FALSE, TRUE))
				return LAYOUT_OUT_OF_MEMORY;

		return LAYOUT_CONTINUE;
	}
}

/* virtual */ LAYST
PositionedFloatingBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
	{
		if (cascade->stacking_context)
			cascade->stacking_context->AddZElement(&z_element);

		cascade->GetProps()->GetLeftAndTopOffset(left_offset, top_offset);

		LAYST status = cascade->container->CreateEmptyLine(info);
		if (status != LAYOUT_CONTINUE)
			return status;
	}

	return FloatingBox::Layout(cascade, info, first_child, start_position);
}

/* virtual */ LAYST
PositionedZRootFloatingBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			z_element.SetZIndex(cascade->GetProps()->z_index);

	return PositionedFloatingBox::Layout(cascade, info, first_child, start_position);
}

/** Finish reflowing box. */

/* virtual */ LAYST
FloatingBox::FinishLayout(LayoutInfo& info)
{
	const HTMLayoutProperties& props = *GetCascade()->GetProps();

	LAYST st = BlockBox::FinishLayout(info);

	if (props.float_type == CSS_VALUE_right)
		float_edge += props.margin_left;
	else
		float_edge += props.margin_right + content->GetWidth();

	return st;
}

/** Set child's bottom margins in this block and propagate changes to parents. See declaration in header file for more information. */

/* virtual */ void
FloatingBox::PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* /* bottom_margin */, BOOL /* has_inflow_content */)
{
	AbsoluteBoundingBox abs_bounding_box;
	ReflowState* state = GetReflowState();
	Container* container = NULL;

	if (state)
		container = state->cascade->container;

	// Nasty hack here

	if (!container)
	{
		HTML_Element* containing_element = GetContainingElement();

		if (containing_element)
			container = containing_element->GetLayoutBox()->GetContainer();
	}

	OP_ASSERT(container);
	if (!container)
		return;

	GetBoundingBox(abs_bounding_box, IsOverflowVisible());

#ifdef CSS_TRANSFORMS
	if (TransformContext *transform_context = GetTransformContext())
		transform_context->ApplyTransform(abs_bounding_box);
#endif

	abs_bounding_box.Translate(x, y);

	LayoutCoord dummy;
	LayoutCoord top;

	GetRelativeOffsets(dummy, top);

	/* Let 'bottom' be the normal-flow bottom margin-edge of the float,
	   relative to the top border-edge of its container. */

	LayoutCoord bottom = y - margin_top - top + GetHeightAndMargins();
	LayoutCoord min_bottom = min_y - GetMarginTop(TRUE) + GetMinHeightAndMargins();

	container->PropagateBottom(info, bottom, min_bottom, abs_bounding_box, PROPAGATE_FLOAT);
}

/** Propagate a break point caused by break properties or a spanned element. */

/* virtual */ BOOL
FloatingBox::PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint)
{
	// Don't break inside floats.

	return TRUE;
}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

/** Signal that the HTML_Element associated with this layout box about to be deleted. */

/* virtual */ void
TableRowGroupBox::SignalHtmlElementDeleted(FramesDocument* doc)
{
}

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

/** Initialise reflow state. */

TableRowGroupBoxReflowState*
TableRowGroupBox::InitialiseReflowState()
{
	if (!(un.data & 1))
	{
		TableRowGroupBoxReflowState* state = new TableRowGroupBoxReflowState;

		if (state)
		{
			state->html_element = un.html_element;

			un.data = ((UINTPTR)state) | 1;
		}

		return state;
	}
	else
		return (TableRowGroupBoxReflowState*) (un.data & ~1);
}

/** Delete reflow state. */

void
TableRowGroupBox::DeleteReflowState()
{
	TableRowGroupBoxReflowState* state = GetReflowState();

	if (state)
	{
		un.html_element = state->html_element;
		OP_DELETE(state);
	}
}

/** Traverse box. */

/* virtual */ void
TableRowGroupBox::Traverse(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table)
{
	OP_PROBE6(OP_PROBE_TABLEROWGROUPBOX_TRAVERSE);

	HTML_Element* html_element = GetHtmlElement();
	TraverseType old_traverse_type = traversal_object->GetTraverseType();
	BOOL positioned = IsPositionedBox();
	LayoutCoord translate_x = GetPositionedX();
	LayoutCoord translate_y = GetPositionedY();

	traversal_object->Translate(translate_x, translate_y);

	HTML_Element* old_target = traversal_object->GetTarget();
	RootTranslationState translation_state;
	BOOL traverse_descendant_target = FALSE;

	if (positioned)
	{
		traversal_object->SyncRootScrollAndTranslation(translation_state);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(NULL);

			if (old_traverse_type != TRAVERSE_ONE_PASS)
				traversal_object->SetTraverseType(TRAVERSE_BACKGROUND);
		}
	}

	if (!IsVisibilityCollapsed() && traversal_object->EnterTableRowGroup(this))
	{
		PaneTraverseState old_pane_traverse_state;

		traversal_object->StorePaneTraverseState(old_pane_traverse_state);

		if (positioned)
			TraverseZChildren(traversal_object, table_lprops, FALSE, FALSE);

		TraverseAllRows(traversal_object, table_lprops, table);

		if (positioned)
		{
			if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
			{
				traversal_object->FlushBackgrounds(table_lprops, this);
				traversal_object->SetTraverseType(TRAVERSE_CONTENT);

				LayoutProperties* layout_props = table_lprops->GetChildProperties(traversal_object->GetDocument()->GetHLDocProfile(), GetHtmlElement());

				if (!layout_props)
				{
					traversal_object->SetOutOfMemory();
					return;
				}

				traversal_object->RestorePaneTraverseState(old_pane_traverse_state);
				traversal_object->TraverseFloats(this, layout_props);

				traversal_object->RestorePaneTraverseState(old_pane_traverse_state);
				TraverseAllRows(traversal_object, table_lprops, table);
			}

			PaneTraverseState new_pane_traverse_state;

			traversal_object->StorePaneTraverseState(new_pane_traverse_state);

			traversal_object->RestorePaneTraverseState(old_pane_traverse_state);
			TraverseZChildren(traversal_object, table_lprops, TRUE, FALSE);
			traversal_object->RestorePaneTraverseState(new_pane_traverse_state);
		}
	}
	else
		traversal_object->SetTraverseType(old_traverse_type);

	if (positioned)
	{
		traversal_object->RestoreRootScrollAndTranslation(translation_state);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(old_target);
			if (traversal_object->SwitchTarget(table_lprops->html_element) == TraversalObject::SWITCH_TARGET_NEXT_IS_DESCENDANT)
				traverse_descendant_target = TRUE;
		}
	}

	traversal_object->Translate(-translate_x, -translate_y);

	if (traverse_descendant_target)
		/* This box was the target (whether it was entered or not is not interesting,
		   though). The next target is a descendant. We can proceed (no need to
		   return to the stacking context loop), but first we have to retraverse
		   this box with the new target set - most importantly because we have to
		   make sure that we enter it and that we do so in the right way. */

		Traverse(traversal_object, table_lprops, table);
}

/** Traverse the content of this box, i.e. its children. */

/* virtual */ void
TableRowGroupBox::TraverseContent(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	LayoutProperties* table_lprops = layout_props->Pred();
	TableContent* table = table_lprops->html_element && table_lprops->html_element->GetLayoutBox() ? table_lprops->html_element->GetLayoutBox()->GetTableContent() : 0;
	OP_ASSERT(table);

	if (table)
		TraverseAllRows(traversal_object, table_lprops, table);
}

/** Traverse all rows in the table-row-group. */

void
TableRowGroupBox::TraverseAllRows(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table)
{
	Column* pane = traversal_object->GetCurrentPane();

	for (TableRowBox* row = GetFirstRow(); row && !traversal_object->IsOffTargetPath(); row = (TableRowBox*) row->Suc())
	{
		if (pane)
			if (!traversal_object->IsPaneStarted())
				/* We're looking for a column's or page's start element. Until
				   we find it, we shouldn't traverse anything. */

				if (row == pane->GetStartElement().GetTableRowBox())
					/* We found the start element. Traverse everything as
					   normally until we find the stop element. */

					traversal_object->SetPaneStarted(TRUE);
				else
					continue;
			else
				if (pane->ExcludeStopElement() && row == pane->GetStopElement().GetTableRowBox())
				{
					traversal_object->SetPaneDone(TRUE);
					break;
				}

		row->TraverseRow(traversal_object, table_lprops, table);

		if (pane)
			if (row == pane->GetStopElement().GetTableRowBox())
			{
				/* We found the stop element, which means that we're done with
				   a column or page. */

				traversal_object->SetPaneDone(TRUE);
				break;
			}
	}
}

/** Recalculate containing block size and schedule the specified descendant box for reflow if needed. */

/* virtual */ void
TableRowGroupBox::RecalculateContainingBlock(FramesDocument* doc, AbsolutePositionedBox* box)
{
	OP_ASSERT(IsPositionedBox());

	CalculateContainingBlockHeight();

	if (HasContainingBlockSizeChanged())
		box->MarkDirty(doc, FALSE);
}

/** Reflow box and children. */

/* virtual */ LAYST
TableRowGroupBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (first_child && first_child != cascade->html_element)
		return LayoutChildren(cascade, info, first_child, start_position);
	else
	{
		TableContent* table = cascade->table;
		PositionedElement* pos_elm = (PositionedElement*) GetLocalZElement();
		TableRowGroupBoxReflowState* state = InitialiseReflowState();

		if (!state || Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
			return LAYOUT_OUT_OF_MEMORY;

		// Cache border in case of collapsing border model

		const HTMLayoutProperties& props = *cascade->GetProps();

		if (props.border.top.style != CSS_VALUE_none ||
			props.border.left.style != CSS_VALUE_none ||
			props.border.right.style != CSS_VALUE_none ||
			props.border.bottom.style != CSS_VALUE_none)
			state->border = props.border;

		switch (props.display_type)
		{
		case CSS_VALUE_table_row_group:
			packed.type = TABLE_ROW_GROUP;
			break;
		case CSS_VALUE_table_header_group:
			packed.type = TABLE_HEADER_GROUP;
			break;
		case CSS_VALUE_table_footer_group:
			packed.type = TABLE_FOOTER_GROUP;
			break;
#ifdef _DEBUG
		default:
			OP_ASSERT(FALSE);
			break;
#endif
		}

		// Remove dirty mark

		cascade->html_element->MarkClean();

		if (table)
		{
			const HTMLayoutProperties& table_props = *table->GetPlaceholder()->GetCascade()->GetProps();

			// Cache border in case of collapsing border model

			state->rules_groups = table->GetRules() == TABLE_RULES_groups;

			if (table_props.border_collapse == CSS_VALUE_collapse)
			{
				state->border_collapsing = TRUE;
				state->border_spacing_vertical = LayoutCoord(0);
			}
			else
			{
				state->border_collapsing = FALSE;
				state->border_spacing_vertical = LayoutCoord(table_props.border_spacing_vertical);
			}

			state->cascade = cascade;
			state->reflow_position = LayoutCoord(0);
			state->old_x = GetPositionedX();
			state->old_y = GetPositionedY();
			state->old_height = GetHeight();
			state->minimum_height = LayoutCoord(0);

			if (cascade->multipane_container)
				/* There's an ancestor multi-pane container, but that doesn't
				   have to mean that this row group is to be columnized. */

				packed2.in_multipane = table->GetPlaceholder()->IsColumnizable(TRUE);

			if (pos_elm)
				pos_elm->Layout(cascade);

			CalculateContainingBlockWidth();

			table->GetNewRowGroup(info, this);

			if (!packed2.is_visibility_collapsed == (props.visibility == CSS_VALUE_collapse))
			{
				packed2.is_visibility_collapsed = props.visibility == CSS_VALUE_collapse;
				table->RequestReflow(info);
			}

			info.Translate(GetPositionedX(), GetPositionedY());

			if (pos_elm)
			{
				info.GetRootTranslation(state->previous_root_x, state->previous_root_y);
				info.SetRootTranslation(info.GetTranslationX(), info.GetTranslationY());
			}

			if (
#ifdef PAGED_MEDIA_SUPPORT
				info.paged_media != PAGEBREAK_OFF ||
#endif // PAGED_MEDIA_SUPPORT
				cascade->multipane_container)
			{
				packed2.avoid_page_break_inside = table->AvoidPageBreakInside();
				packed2.avoid_column_break_inside = table->AvoidColumnBreakInside();

#ifdef PAGED_MEDIA_SUPPORT
				if (info.paged_media != PAGEBREAK_OFF)
					if (props.page_props.break_inside == CSS_VALUE_avoid || props.page_props.break_inside == CSS_VALUE_avoid_page)
						packed2.avoid_page_break_inside = 1;
#endif // PAGED_MEDIA_SUPPORT

				if (props.page_props.break_inside == CSS_VALUE_avoid || props.page_props.break_inside == CSS_VALUE_avoid_column)
					packed2.avoid_column_break_inside = 1;

				BREAK_POLICY page_break_before;
				BREAK_POLICY column_break_before;
				BREAK_POLICY page_break_after;
				BREAK_POLICY column_break_after;

				CssValuesToBreakPolicies(info, cascade, page_break_before, page_break_after, column_break_before, column_break_after);

				packed2.page_break_before = page_break_before;
				packed2.column_break_before = column_break_before;
				packed2.page_break_after = page_break_after;
				packed2.column_break_after = column_break_after;

				/* The column break bitfields cannot hold 'left' and 'right' values
				   (but they are meaningless in a column context anyway): */

				OP_ASSERT(column_break_before != BREAK_LEFT && column_break_before != BREAK_RIGHT);
				OP_ASSERT(column_break_after != BREAK_LEFT && column_break_after != BREAK_RIGHT);
			}
		}

		// Clear list of rows in this group

		while (rows.First())
			rows.First()->Out();

		return LayoutChildren(cascade, info, NULL, LayoutCoord(0));
	}
}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

/** Signal that the HTML_Element associated with this layout box about to be deleted. */

/* virtual */ void
TableColGroupBox::SignalHtmlElementDeleted(FramesDocument* doc)
{
	TableContent* table = GetTable();

	table->SignalChildDeleted(doc, this);
}

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

/** Finish reflowing box. */

/* virtual */ LAYST
TableColGroupBox::FinishLayout(LayoutInfo& info)
{
	TableContent* table = GetTable();

	if (table)
	{
		HTML_Element *parent = html_element->Parent();

		if (col_packed.has_col_child)
		{
			/* Children have added columns to the table. This box won't then,
			   but perform border collapsing still. */

			OP_ASSERT(col_packed.is_group);

			if (border && table->GetCollapseBorders())
			{
				HTML_Element* child = html_element->LastChild();

				while (child)
				{
					if (child->GetLayoutBox())
						break;

					child = child->Pred();
				}

				if (child)
				{
					OP_ASSERT(child->GetLayoutBox()->IsTableColGroup());

					TableColGroupBox* col_box = (TableColGroupBox*) child->GetLayoutBox();
					const Border* child_border = col_box->GetBorder();

					if (child_border)
#ifdef SUPPORT_TEXT_DIRECTION
						if (table->IsRTL())
						{
							border->left.Collapse(child_border->left);
							col_box->CollapseLeftBorder(border->left);
						}
						else
#endif // SUPPORT_TEXT_DIRECTION
						{
							border->right.Collapse(child_border->right);
							col_box->CollapseRightBorder(border->right);
						}
				}
			}
		}
		else
		{
			// No children have added columns to the table, so this box will.

			int span = 1;

			if (html_element->Type() == Markup::HTE_COL || html_element->Type() == Markup::HTE_COLGROUP)
				span = (int)(INTPTR)html_element->GetAttr(Markup::HA_SPAN, ITEM_TYPE_NUM, (void*) 1);

			if (!table->AddColumns(info, span, desired_width, this))
				return LAYOUT_OUT_OF_MEMORY;
		}

		if (parent->GetLayoutBox()->IsTableColGroup())
			((TableColGroupBox*)parent->GetLayoutBox())->SetHasColChild();
	}

	return LAYOUT_CONTINUE;
}

BOOL
TableColGroupBox::IsFirstColInGroup()
{
	if (IsGroup())
		/* Only applies to table-columns */
		return FALSE;

	HTML_Element *iter = GetHtmlElement()->Pred();
	while (iter)
	{
		if (Box *box = iter->GetLayoutBox())
			if (box->IsTableColGroup())
				return FALSE;

		iter = iter->Pred();
	}

	return TRUE;
}

/** Reflow box and children. */

/* virtual */ LAYST
TableColGroupBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (first_child && first_child != html_element)
		return LayoutChildren(cascade, info, first_child, start_position);
	else
	{
		const HTMLayoutProperties& props = *cascade->GetProps();
		TableContent* table = cascade->table;

		if (Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
			return LAYOUT_OUT_OF_MEMORY;

		col_packed.is_group = props.display_type == CSS_VALUE_table_column_group;
		col_packed.has_col_child = 0;

		// Cache border in case of collapsing border model

		if (table->GetCollapseBorders())
		{
			const Border* parent_border = NULL;
			Box* parent_box = html_element->Parent()->GetLayoutBox();

			if (parent_box->IsTableColGroup())
				parent_border = ((TableColGroupBox*)parent_box)->GetBorder();

			if (parent_border ||
				props.border.top.style != CSS_VALUE_none ||
				props.border.left.style != CSS_VALUE_none ||
				props.border.right.style != CSS_VALUE_none ||
				props.border.bottom.style != CSS_VALUE_none)
			{
				if (!border)
				{
					border = OP_NEW(Border, ());

					if (!border)
						return LAYOUT_OUT_OF_MEMORY;
				}

				*border = props.border;

				if (parent_border)
				{
					border->top.Collapse(parent_border->top);
					border->bottom.Collapse(parent_border->bottom);

					if (IsFirstColInGroup())
#ifdef SUPPORT_TEXT_DIRECTION
						if (table->IsRTL())
							border->right.Collapse(parent_border->right);
						else
#endif // SUPPORT_TEXT_DIRECTION
							border->left.Collapse(parent_border->left);
				}
			}
		}

		if (!col_packed.is_visibility_collapsed == (props.visibility == CSS_VALUE_collapse))
		{
			col_packed.is_visibility_collapsed = props.visibility == CSS_VALUE_collapse;
			table->RequestReflow(info);
		}

		desired_width = info.table_strategy == TABLE_STRATEGY_NORMAL ? props.content_width : CONTENT_WIDTH_AUTO;

		// Remove dirty mark

		cascade->html_element->MarkClean();

		return LayoutChildren(cascade, info, NULL, LayoutCoord(0));
	}
}

/** Invalidate the screen area that the box uses. */

/* virtual */ void
TableColGroupBox::Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const
{
	TableContent* table = parent_cascade->html_element->GetLayoutBox()->GetTableContent();

	if (!table)
		table = parent_cascade->table;

	info.visual_device->UpdateRelative(x, 0, width, table->GetOldHeight());
}

/** Update properties cached by this class. */

void
TableColGroupBox::UpdateProps(const HTMLayoutProperties& props)
{
	font_color = props.font_color;
	bg_color = props.bg_color;
	bg_images = props.bg_images;
	box_shadows = props.box_shadows;
}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

/** Signal that the HTML_Element associated with this layout box about to be deleted. */

/* virtual */ void
TableRowBox::SignalHtmlElementDeleted(FramesDocument* doc)
{
}

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

TableRowBox::TableRowBox(HTML_Element* element)
	: height(0),
	  y_pos(0),
	  baseline(0)
{
	un.html_element = element;
	packed_init = 0;
	packed2_init = 0;
}

TableCollapsingBorderRowBox::TableCollapsingBorderRowBox(HTML_Element* element)
  : TableRowBox(element)
{
    border.Reset();
	bounding_box_top = 0;
	bounding_box_bottom = 0;
}

/** Initialise reflow state. */

TableRowBoxReflowState*
TableRowBox::InitialiseReflowState()
{
	if (!(un.data & 1))
	{
		TableRowBoxReflowState* state = new TableRowBoxReflowState;

		if (state)
		{
			state->html_element = un.html_element;

			un.data = ((UINTPTR)state) | 1;
		}

		return state;
	}
	else
		return (TableRowBoxReflowState*) (un.data & ~1);
}

/** Delete reflow state. */

void
TableRowBox::DeleteReflowState()
{
	ReflowState* state = GetReflowState();

	if (state)
	{
		un.html_element = state->html_element;
		OP_DELETE(state);
	}
}

/** Get cell spanning 'column' with rowspan at least as big as 'rowspan'. */

/* static */ TableCellBox*
TableRowBox::GetRowSpannedCell(TableRowBox* row, unsigned int column, int rowspan, BOOL table_is_wrapped, int* relative_row_number, TableRowBox** foreign_row)
{
	for (int distance = 0; row; distance++, rowspan++, row = (TableRowBox*) row->Pred())
	{
		if (TableCellBox* cell = row->GetCell(column))
		{
			int cell_row_span = table_is_wrapped ? 1 : cell->GetCellRowSpan();

			if (rowspan > 1 && cell_row_span < rowspan)
			{
				if (cell->GetColumn() == column)
					// The cell doesn't have enough rowspan to reach the row we started at.

					return NULL;
			}
			else
			{
				if (cell_row_span > 1 && foreign_row)
					*foreign_row = row;

				if (relative_row_number)
					*relative_row_number = cell_row_span - distance;

				return cell;
			}
		}

		if (table_is_wrapped)
			break;
	}

	return NULL;
}

/** Calculate baseline of row. */

LayoutCoord
TableRowBox::CalculateBaseline() const
{
	/* CSS 2.1:
	   The baseline of a table row is the baseline of the baseline-aligned
	   table cell in the row with the largest height above its baseline. If no
	   table cell in the row is aligned to the baseline, the baseline of the
	   table row is the bottom content edge of the lowest cell in the row. */

	LayoutCoord lowest_bottom_content_edge(0);
	BOOL has_baseline_alignment = FALSE;
	LayoutCoord baseline = LAYOUT_COORD_MIN;

	for (TableCellBox* cell = GetFirstCell(); cell; cell = (TableCellBox*) cell->Suc())
		if (cell->IsBaselineAligned())
		{
			has_baseline_alignment = TRUE;
			LayoutCoord cell_baseline = cell->GetBaseline();

			if (baseline < cell_baseline)
				baseline = cell_baseline;
		}
		else
			if (!has_baseline_alignment)
			{
				/* Is this too expensive? It is probably only needed on the first
				   row of inline-tables. Using only baseline-aligned cells may
				   give incorrect results, but should be sufficient in all other
				   cases. */

				LayoutCoord bottom_content_edge = cell->ComputeCellY(this) + cell->GetContent()->GetHeight();

				if (bottom_content_edge > lowest_bottom_content_edge)
					lowest_bottom_content_edge = bottom_content_edge;
			}

	if (!has_baseline_alignment)
		return lowest_bottom_content_edge;

	return baseline;
}

/** Traverse box with children. */

/* virtual */ void
TableRowBox::TraverseRow(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table)
{
	OP_PROBE6(OP_PROBE_TABLEROWBOX_TRAVERSEROW);

	HTML_Element* html_element = GetHtmlElement();
	TraverseType old_traverse_type = traversal_object->GetTraverseType();
	BOOL positioned = IsPositionedBox();
	LayoutCoord translate_x = GetPositionedX();
	LayoutCoord translate_y = GetPositionedY();

	traversal_object->Translate(translate_x, translate_y);

	HTML_Element* old_target = traversal_object->GetTarget();
	RootTranslationState translation_state;
	BOOL traverse_descendant_target = FALSE;

	if (positioned)
	{
		traversal_object->SyncRootScrollAndTranslation(translation_state);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(NULL);

			if (old_traverse_type != TRAVERSE_ONE_PASS)
				traversal_object->SetTraverseType(TRAVERSE_BACKGROUND);
		}
	}

	if (traversal_object->EnterTableRow(this))
	{
		if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
			traversal_object->HandleEmptyCells(this, table, table_lprops);

		if (positioned)
		{
			traversal_object->FlushBackgrounds(table_lprops, this);
			TraverseZChildren(traversal_object, table_lprops, FALSE, FALSE);
		}

		for (TableCellBox* cell = GetFirstCell(); cell && !traversal_object->IsOffTargetPath(); cell = (TableCellBox*) cell->Suc())
			cell->TraverseCell(traversal_object, table_lprops, this);

		if (positioned)
			if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
			{
				traversal_object->FlushBackgrounds(table_lprops, this);
				traversal_object->SetTraverseType(TRAVERSE_CONTENT);

				LayoutProperties* layout_props = table_lprops->GetChildProperties(traversal_object->GetDocument()->GetHLDocProfile(), GetHtmlElement());

				if (!layout_props)
				{
					traversal_object->SetOutOfMemory();
					return;
				}

				traversal_object->TraverseFloats(this, layout_props);

				OP_ASSERT(!traversal_object->GetTarget());

				for (TableCellBox* cell = GetFirstCell(); cell; cell = (TableCellBox*) cell->Suc())
					cell->TraverseCell(traversal_object, table_lprops, this);
			}

		if (positioned)
		{
			traversal_object->FlushBackgrounds(table_lprops, this);
			TraverseZChildren(traversal_object, table_lprops, TRUE, FALSE);
		}

		traversal_object->LeaveTableRow(this, table);
	}
	else
		traversal_object->SetTraverseType(old_traverse_type);

	if (positioned)
	{
		traversal_object->RestoreRootScrollAndTranslation(translation_state);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(old_target);
			if (traversal_object->SwitchTarget(table_lprops->html_element) == TraversalObject::SWITCH_TARGET_NEXT_IS_DESCENDANT)
				traverse_descendant_target = TRUE;
		}
	}

	traversal_object->Translate(-translate_x, -translate_y);

	if (traverse_descendant_target)
		/* This row was the target (whether it was entered or not is not interesting,
		   though). The next target is a descendant. We can proceed (no need to
		   return to the stacking context loop), but first we have to retraverse
		   this row with the new target set - most importantly because we have to
		   make sure that we enter it and that we do so in the right way. */

		TraverseRow(traversal_object, table_lprops, table);
}

/** Traverse the content of this box, i.e. its children. */

/* virtual */ void
TableRowBox::TraverseContent(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	LayoutProperties* table_lprops = layout_props->Pred()->Pred();

	for (TableCellBox* cell = GetFirstCell(); cell && !traversal_object->IsOffTargetPath(); cell = (TableCellBox*) cell->Suc())
		cell->TraverseCell(traversal_object, table_lprops, this);
}

/** Increase y position (or decrease, if using negative values). */

void
TableRowBox::IncreaseY(LayoutInfo& info, LayoutCoord increase)
{
	y_pos += increase;

	if (GetReflowState())
		// This box is currently being laid out, so we need to translate.

		info.Translate(LayoutCoord(0), increase);
}

/** Is table wrapped? (AMSR) */

BOOL
TableRowBox::IsTableWrapped() const
{
	if (IsWrapped())
		return TRUE;
	else
	{
		OP_ASSERT(GetHtmlElement()->Parent());

		HTML_Element* parent = GetHtmlElement()->Parent()->Parent();

		OP_ASSERT(parent);
		OP_ASSERT(parent->GetLayoutBox());
		OP_ASSERT(parent->GetLayoutBox()->GetTableContent());

		return parent->GetLayoutBox()->GetTableContent()->IsWrapped();
	}
}

/** Get cell spanning 'column'. */

TableCellBox*
TableRowBox::GetCell(unsigned short column) const
{
	for (TableCellBox* cell = GetLastCell(); cell; cell = (TableCellBox*) cell->Pred())
		if (cell->GetColumn() == column)
			return cell;
		else
			if (cell->GetColumn() < column)
				if (cell->GetColumn() + cell->GetCellColSpan() > column)
					return cell;
				else
					return NULL;

	return NULL;
}

/** Recalculate containing block size and schedule the specified descendant box for reflow if needed. */

/* virtual */ void
TableRowBox::RecalculateContainingBlock(FramesDocument* doc, AbsolutePositionedBox* box)
{
	OP_ASSERT(IsPositionedBox());

	CalculateContainingBlockHeight();

	if (HasContainingBlockSizeChanged())
		box->MarkDirty(doc, FALSE);
}

/** Is this box (and potentially also its children) columnizable? */

/* virtual */ BOOL
TableRowBox::IsColumnizable(BOOL require_children_columnizable) const
{
	return !require_children_columnizable && packed.in_multipane == 1;
}

/** Reflow box and children. */

/* virtual */ LAYST
TableRowBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (first_child && first_child != cascade->html_element)
		return LayoutChildren(cascade, info, first_child, start_position);
	else
	{
		TableContent* table = cascade->table;
		PositionedElement* pos_elm = (PositionedElement*) GetLocalZElement();
#ifdef PAGED_MEDIA_SUPPORT
		int reflow_content =
#endif
			cascade->html_element->MarkClean();
		TableRowBoxReflowState* state = InitialiseReflowState();

		if (!state || Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
			return LAYOUT_OUT_OF_MEMORY;

		state->cascade = cascade;
		state->old_x = GetPositionedX();
		state->old_y = GetPositionedY();
		state->old_baseline = GetBaseline();
		state->old_height = height;
		state->row_group_box = cascade->FindTableRowGroup();

		if (pos_elm)
			pos_elm->Layout(cascade);

		CalculateContainingBlockWidth();

		if (
#ifdef PAGED_MEDIA_SUPPORT
			info.paged_media != PAGEBREAK_OFF ||
#endif // PAGED_MEDIA_SUPPORT
			cascade->multipane_container)
		{
#ifdef PAGED_MEDIA_SUPPORT
			BOOL has_trailing_page_break = packed.has_trailing_page_break;
			BOOL pending_page_break = packed.pending_page_break;
			BOOL override_vertical_align = packed.override_vertical_align;
#endif // PAGED_MEDIA_SUPPORT

			packed_init = 0;

#ifdef PAGED_MEDIA_SUPPORT
			if (!cascade->multipane_container)
			{
				packed.has_trailing_page_break = has_trailing_page_break;
				packed.pending_page_break = pending_page_break;

				if (override_vertical_align)
					packed.override_vertical_align = TRUE;
				else
					if (info.paged_media == PAGEBREAK_FIND && !(reflow_content & ELM_MINMAX_DELETED))
						switch (SpansMultiplePages(info))
						{
						case OpBoolean::IS_TRUE:
							packed.override_vertical_align = TRUE;
							info.paged_media = PAGEBREAK_ON;
							break;
						case OpBoolean::IS_FALSE:
							break;
						default:
							return LAYOUT_OUT_OF_MEMORY;
						}
			}
#endif // PAGED_MEDIA_SUPPORT

			BREAK_POLICY page_break_before;
			BREAK_POLICY column_break_before;
			BREAK_POLICY page_break_after;
			BREAK_POLICY column_break_after;

			CssValuesToBreakPolicies(info, cascade, page_break_before, page_break_after, column_break_before, column_break_after);

			packed2.page_break_before = page_break_before;
			packed2.column_break_before = column_break_before;
			packed2.page_break_after = page_break_after;
			packed2.column_break_after = column_break_after;

			/* The column break bitfields cannot hold 'left' and 'right' values
			   (but they are meaningless in a column context anyway): */

			OP_ASSERT(column_break_before != BREAK_LEFT && column_break_before != BREAK_RIGHT);
			OP_ASSERT(column_break_after != BREAK_LEFT && column_break_after != BREAK_RIGHT);
		}
		else
			packed_init = 0;

		CollapseTopBorder();

		height = LayoutCoord(0);

		if (table)
		{
			TableRowGroupBox* row_group = state->row_group_box;

			state->next_column = 0;
			state->reflow_position = LayoutCoord(0);

			state->baseline = LayoutCoord(0);
			state->max_below_baseline = LayoutCoord(0);

			if (cascade->multipane_container)
				/* There's an ancestor multi-pane container, but that doesn't
				   have to mean that this row is to be columnized. */

				packed.in_multipane = table->GetPlaceholder()->IsColumnizable(TRUE);

			// Reset current formatting

			cells.RemoveAll();

			if (!row_group->GetNewRow(info, this))
				return LAYOUT_OUT_OF_MEMORY;

			if (table->UseOldRowHeights())
			{
				state->use_old_height = TRUE;
				row_group->ForceRowHeightIncrease(info, this, state->old_height);
			}

			const HTMLayoutProperties& props = *cascade->GetProps();
			LayoutCoord min_height = props.content_height;

			packed.has_spec_height = min_height != CONTENT_HEIGHT_AUTO && min_height != 0;

			packed.is_wrapped = 0;

			if (!packed2.is_visibility_collapsed == (props.visibility == CSS_VALUE_collapse))
			{
				packed2.is_visibility_collapsed = props.visibility == CSS_VALUE_collapse;
				table->RequestReflow(info);
			}

			if (packed.has_spec_height)
				GrowRow(info, min_height, LayoutCoord(0), TRUE);

			// First column can be rowspanned

			if (table->SkipRowSpannedColumns(info, state->next_column, this, &state->reflow_position))
				packed.rowspanned_from = TRUE;

			row_group->PropagateBottomMargins(info);

			info.Translate(GetPositionedX(), GetPositionedY());

			if (pos_elm)
			{
				info.GetRootTranslation(state->previous_root_x, state->previous_root_y);
				info.SetRootTranslation(info.GetTranslationX(), info.GetTranslationY());
			}
		}

		return LayoutChildren(cascade, info, NULL, LayoutCoord(0));
	}
}

/** Reflow box and children. */

/* virtual */ LAYST
TableCollapsingBorderRowBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
	{
		TableRowGroupBox* row_group = cascade->FindTableRowGroup();
		const HTMLayoutProperties* table_props = row_group->GetHTMLayoutProperties();

		border = cascade->GetProps()->border;

		if (!row_group->GetLastRow())
			// Collapse top border of row with top border of table

			border.top.Collapse(table_props->border.top);

		// Collapse left border of row with left border of table

		border.left.Collapse(table_props->border.left);

		// Collapse right border of row with left border of table

		border.right.Collapse(table_props->border.right);
	}

	return TableRowBox::Layout(cascade, info, first_child, start_position);
}

/** Collapse bottom border. */

void
TableCollapsingBorderRowBox::CollapseBottomBorder(const BorderEdge& bottom_border)
{
	border.bottom.Collapse(bottom_border);
}

/** Collapse top border (in collapsing border model). */

/* virtual */ void
TableCollapsingBorderRowBox::CollapseTopBorder()
{
	TableRowBoxReflowState* state = GetReflowState();
	if (!state)
		return;
	HTML_Element* html_element = state->cascade->html_element;
	TableRowGroupBox* row_group_box = state->row_group_box;

	// Check against tablerowgroup

	if (row_group_box->GetBorder())
	{
		HTML_Element* row_group_element = row_group_box->GetHtmlElement();
		HTML_Element* row_element = row_group_element->FirstChild();

		while (row_element && row_element->Type() == Markup::HTE_TEXT) // FIXME: This is wrong. Look for the first layout box instead.
			row_element = row_element->Suc();

		if (row_element == html_element)
			border.top.Collapse(row_group_box->GetBorder()->top);
	}

	TableRowBox* last_row = row_group_box->GetLastRow();

	if (last_row)
		border.top.Collapse(((TableCollapsingBorderRowBox*) last_row)->GetBorder().bottom);
}

/** Finish right border (in collapsing border model). In a rtl table this will instead be the left border in the leftmost column.*/

/* virtual */ void
TableCollapsingBorderRowBox::FinishLastBorder(LayoutInfo& info, int number_of_columns, const BorderEdge& table_last_border)
{
	TableCollapsingBorderCellBox* cell = (TableCollapsingBorderCellBox*) GetLastCell();

#ifdef SUPPORT_TEXT_DIRECTION
	if (GetTable()->IsRTL())
	{
		border.left.Collapse(table_last_border);

		if (cell && cell->GetColumn() + cell->GetCellColSpan() >= number_of_columns)
			cell->CollapseLeftBorder(info, border.left);
	}
	else
#endif // SUPPORT_TEXT_DIRECTION
	{
		border.right.Collapse(table_last_border);

		if (cell && cell->GetColumn() + cell->GetCellColSpan() >= number_of_columns)
			cell->CollapseRightBorder(info, border.right, TRUE, FALSE);
	}
}

/** Finish bottom border (in collapsing border model). */

/* virtual */ void
TableCollapsingBorderRowBox::FinishBottomBorder(LayoutInfo& info, const BorderEdge& bottom_border, TableRowGroupBox* row_group, TableContent* table)
{
	TableCollapsingBorderCellBox* cell;

	// Collapse bottom border of row with bottom border of table row group

	border.bottom.Collapse(bottom_border);

	for (unsigned int column = 0; column < table->GetLastColumn(); column++)
	{
		cell = (TableCollapsingBorderCellBox*) GetRowSpannedCell(this, column, 1, table->IsWrapped());

		if (cell)
		{
			BorderEdge cell_bottom_border = cell->GetBorder().bottom;

			TableColGroupBox* column_box = table->GetColumnBox(column);

			if (column_box && column_box->GetBorder())
				cell_bottom_border.Collapse(column_box->GetBorder()->bottom);

			cell_bottom_border.Collapse(border.bottom);

			LayoutCoord row_height = GetHeight();
			if (!table->IsWrapped() && cell->GetCellRowSpan() > 1)
				row_height += GetStaticPositionedY() - cell->GetTableRowBox()->GetStaticPositionedY();

			cell->CollapseBottomBorder(info, row_group, this, row_height, cell_bottom_border);

			column += cell->GetCellColSpan() - 1;
		}
	}
}

/** Update bounding box. */

void
TableCollapsingBorderRowBox::UpdateBoundingBox(short top, short bottom)
{
	if (bounding_box_top < top)
		bounding_box_top = top;

	if (bounding_box_bottom < bottom)
		bounding_box_bottom = bottom;
}

/** Get previous. */

TableCollapsingBorderRowBox*
TableCollapsingBorderRowBox::GetPrevious() const
{
	TableCollapsingBorderRowBox* prev_row_box = (TableCollapsingBorderRowBox*) Pred();
	if (!prev_row_box)
	{
		TableRowGroupBox* row_group = GetTableRowGroup(GetHtmlElement());
		if (row_group)
			prev_row_box = (TableCollapsingBorderRowBox*) row_group->GetLastRow();
	}

	return prev_row_box;
}

/** Get used border widths, including collapsing table borders */

/* virtual */ void
TableCollapsingBorderRowBox::GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const
{
	if (inner)
	{
		top = (border.top.width + 1) / 2;
		left = (border.left.width + 1) / 2;
		right = border.right.width / 2;
		bottom = border.bottom.width / 2;
	}
	else
	{
		top = border.top.width / 2;
		left = border.left.width / 2;
		right = (border.right.width + 1) / 2;
		bottom = (border.bottom.width + 1) / 2;
	}
}

/** Grow height. */

void
TableRowGroupBox::GrowRowGroup(LayoutInfo& info, LayoutCoord increment)
{
	TableContent* table = GetTable();

	OP_ASSERT(increment);

	if (table)
		table->MoveElements(info, Suc(), increment);
}

/** Force row height increase. */

void
TableRowGroupBox::ForceRowHeightIncrease(LayoutInfo& info, TableRowBox* row, LayoutCoord increment, BOOL clear_use_old_row_heights)
{
	if (increment)
	{
		row->GrowHeight(increment);
		row->UpdateBaseline();

		if (clear_use_old_row_heights)
			for (TableCellBox* cell = row->GetFirstCell(); cell; cell = (TableCellBox*) cell->Suc())
				if (cell->HasContentWithPercentageHeight())
					/* Table cell has content with percentage based height.
					   If the content is a table it needs to be reflowed without using old heights. */

					cell->ClearUseOldRowHeights();

		while (row->Suc())
		{
			row = (TableRowBox*) row->Suc();

			row->IncreaseY(info, increment);
		}

		GrowRowGroup(info, increment);
	}
}

#ifdef PAGED_MEDIA_SUPPORT

/** Insert a page break. */

/* virtual */ BREAKST
TableRowGroupBox::AttemptPageBreak(LayoutInfo& info, int strength)
{
	BREAKST status = BREAK_KEEP_LOOKING;
	TableRowBox* potential_break = NULL;

	info.Translate(LayoutCoord(0), GetPositionedY());

	for (TableRowBox* row = (TableRowBox*) rows.Last(); row && status == BREAK_KEEP_LOOKING; row = (TableRowBox*) row->Pred())
		status = row->AttemptPageBreak(info, strength, potential_break);

	info.Translate(LayoutCoord(0), -GetPositionedY());

	return status;
}

/** Insert a page break. */

BREAKST
TableRowGroupBox::InsertPageBreak(LayoutInfo& info, int strength)
{
	TableRowGroupBoxReflowState* state = GetReflowState();
	TableContent* table = state->cascade->table;

	if (table)
	{
		BREAKST status;

		info.Translate(LayoutCoord(0), -GetPositionedY());
		status = table->InsertPageBreak(info, strength);
		info.Translate(LayoutCoord(0), GetPositionedY());

		return status;
	}

	return BREAK_NOT_FOUND;
}

#endif // PAGED_MEDIA_SUPPORT

/** Add row to table. */

BOOL
TableRowGroupBox::GetNewRow(LayoutInfo& info, TableRowBox* row_box)
{
	TableRowGroupBoxReflowState* state = GetReflowState();
	if (!state)
		return FALSE;
	TableContent* table = state->cascade->table;

	CloseRow(state, info, FALSE);

	row_box->Into(&rows);
	row_box->SetStaticPositionedY(state->reflow_position);

	if (state->border_spacing_vertical)
		GrowRowGroup(info, LayoutCoord(state->border_spacing_vertical));

	table->ResetCollapsedBottomBorder();

	return TRUE;
}

/** Close rows. */

void
TableRowGroupBox::CloseRow(TableRowGroupBoxReflowState* reflow_state, LayoutInfo& info, BOOL group_finished)
{
	TableContent* table = reflow_state->cascade->table;
	TableRowBox* last_row = (TableRowBox*) rows.Last();

	if (last_row)
	{
		if (group_finished)
		{
			/* Rowspanned columns with too large rowspan value may
			   affect height on last row. */

			table->TerminateRowSpan(info, last_row);

			if (table->GetCollapseBorders())
				// Finish bottom border on all cells in last row

				last_row->FinishBottomBorder(info, GetBorder()->bottom, this, table);
		}

		// Advance reflow position

		LayoutCoord bottom = last_row->GetStaticPositionedY() + last_row->GetHeight();

		reflow_state->reflow_position = bottom + reflow_state->border_spacing_vertical;
		reflow_state->minimum_height += reflow_state->border_spacing_vertical;

#ifdef PAGED_MEDIA_SUPPORT
		if (info.paged_media != PAGEBREAK_OFF && !reflow_state->cascade->multipane_container)
			if (last_row->HasTrailingPagebreak())
			{
				info.doc->RewindPage(bottom);
				info.doc->AdvancePage(bottom);

				reflow_state->reflow_position = LayoutCoord(info.doc->GetRelativePageTop());

				if (last_row->HasPendingPagebreak())
				{
					OP_ASSERT(info.paged_media == PAGEBREAK_FIND);
					info.paged_media = PAGEBREAK_ON;
				}
			}
#endif // PAGED_MEDIA_SUPPORT
	}
}

/** Finish right border on every cell in rightmost column. In case of rtl table, left border in leftmost column */

void
TableRowGroupBox::FinishLastBorder(TableCollapsingBorderContent* table, LayoutInfo& info)
{
	const HTMLayoutProperties& props = *table->GetPlaceholder()->GetCascade()->GetProps();

	for (TableRowBox* row = (TableRowBox*) rows.First(); row; row = (TableRowBox*) row->Suc())
		row->FinishLastBorder(info,
							  table->GetColumnCount(),
#ifdef SUPPORT_TEXT_DIRECTION
							  table->IsRTL() ? props.border.left :
#endif // SUPPORT_TEXT_DIRECTION
							  props.border.right);
}

/** Is this box (and potentially also its children) columnizable? */

/* virtual */ BOOL
TableRowGroupBox::IsColumnizable(BOOL require_children_columnizable) const
{
	return packed2.in_multipane == 1;
}

/** Increase y position (or decrease, if using negative values). */

/* virtual */ void
TableRowGroupBox::IncreaseY(LayoutInfo& info, TableContent* table, LayoutCoord increase)
{
	packed.y_pos += increase;

	if (ReflowState* state = GetReflowState())
		// A TableRowGroupBox's reflow state is kept until table layout has finished.

		if (state->cascade)
			/* Its cascade, however, isn't. So if we get here, we're laying
			   out this box. Let's translate. */

			info.Translate(LayoutCoord(0), increase);
}

/** Get height. */

/* virtual */ LayoutCoord
TableRowGroupBox::GetHeight() const
{
	TableRowBox* bottom_row = (TableRowBox*) rows.Last();

	if (bottom_row)
		return bottom_row->GetStaticPositionedY() + bottom_row->GetHeight();
	else
		return LayoutCoord(0);
}

/** Get height of spanned rows including vertical cell spacing */

LayoutCoord
TableRowGroupBox::GetRowSpannedHeight(const TableRowBox* row, int rowspan)
{
	OP_PROBE3(OP_PROBE_TABLEROWGROUPBOX_GETROWSPANNEDHEIGHT);

	const TableRowBox* bottom_row = row;

	// Find bottom row

	while (--rowspan > 0)
	{
		TableRowBox* suc = (TableRowBox*) bottom_row->Suc();

		if (suc)
			bottom_row = suc;
		else
			if (GetReflowState())
			{
				// While reflowing old rows are taken out of list. We still need the old
				// height so use an alternative way of finding the rows ...

				HTML_Element* row_element = bottom_row->GetHtmlElement()->Suc();
				while (row_element && !row_element->GetLayoutBox())
					row_element = row_element->Suc();

				if (row_element && row_element->GetLayoutBox()->IsTableRow())
					bottom_row = (TableRowBox*) row_element->GetLayoutBox();
				else
					break;
			}
			else
				break;
	}

	return bottom_row->GetStaticPositionedY() + bottom_row->GetHeight() - row->GetStaticPositionedY();
}

/** Update screen. */

/* virtual */ void
TableRowGroupBox::UpdateScreen(LayoutInfo& info)
{
	TableRowGroupBoxReflowState* state = GetReflowState();

	state->old_height = GetHeight();

	info.Translate(-GetPositionedX(), -GetPositionedY());
}

/** Invalidate the screen area that the box uses. */

/* virtual */ void
TableRowGroupBox::Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const
{
	TableContent* table = parent_cascade->html_element->GetLayoutBox()->GetTableContent();

	// FIXME: no way to handle overflow here

	info.visual_device->UpdateRelative(0, GetPositionedY(), table->GetRowWidth(), GetHeight());
}

/** Finish reflowing box. */

/* virtual */ LAYST
TableRowGroupBox::FinishLayout(LayoutInfo& info)
{
	TableRowGroupBoxReflowState* state = GetReflowState();

	OP_ASSERT(state);

	CloseRow(state, info, TRUE);

	if (StackingContext* local_stacking_context = GetLocalStackingContext())
		local_stacking_context->FinishLayout(state->cascade);

	UpdateScreen(info);

	if (IsPositionedBox())
		info.SetRootTranslation(state->previous_root_x, state->previous_root_y);

	state->cascade->table->AddRowGroupMinHeight(state->minimum_height);

	/* Note that reflow state is kept until layout of our table is finished.
	   This is because table-row-groups may be moved vertically by
	   table-row-group or table-caption siblings following it. The cascade is
	   useless as soon as we continue to the next element, though, so let's
	   reset it. */

	state->cascade = NULL;

	return LAYOUT_CONTINUE;
}

/** Get used border widths, including collapsing table borders */

/* virtual */ void
TableRowGroupBox::GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const
{
	TableContent* table = GetTable();

	if (table->GetCollapseBorders())
		table->GetBorderWidths(props, top, left, right, bottom, inner); // Note that 'props' is for the table-row-group here, not the table.
	else
		// No table-row borders in separated borders table model.

		top = left = right = bottom = 0;
}

/** Distribute content into columns. */

/* virtual */ BOOL
TableRowGroupBox::Columnize(Columnizer& columnizer, Container* container)
{
	BREAK_POLICY prev_page_break_policy = BREAK_AVOID;
	BREAK_POLICY prev_column_break_policy = BREAK_AVOID;
	LayoutCoord stack_position = GetStaticPositionedY();

	columnizer.EnterChild(stack_position);

	for (TableRowBox* row = (TableRowBox*) rows.First(); row; row = (TableRowBox*) row->Suc())
	{
		if (row->IsVisibilityCollapsed())
			continue;

		LayoutCoord virtual_position = row->GetStaticPositionedY();
		BREAK_POLICY cur_column_break_policy = row->GetColumnBreakPolicyBefore();
		BREAK_POLICY column_break_policy = CombineBreakPolicies(prev_column_break_policy, cur_column_break_policy);
		BREAK_POLICY cur_page_break_policy = row->GetPageBreakPolicyBefore();
		BREAK_POLICY page_break_policy = CombineBreakPolicies(prev_page_break_policy, cur_page_break_policy);

		if (BreakForced(page_break_policy))
		{
			if (!columnizer.ExplicitlyBreakPage(row))
				return FALSE;
		}
		else
			if (BreakForced(column_break_policy))
				if (!columnizer.ExplicitlyBreakColumn(row))
					return FALSE;

		if (!packed2.avoid_column_break_inside && BreakAllowedBetween(prev_column_break_policy, cur_column_break_policy)
			&& (columnizer.GetColumnsLeft() > 0 ||
				!packed2.avoid_page_break_inside && BreakAllowedBetween(prev_page_break_policy, cur_page_break_policy)))
		{
			// We are allowed to move on to the next column/page, if necessary.

			/* Be sure to commit the vertical border spacing preceding this row
			   before adding this row.

			   Other than the fact that it makes it marginally easier to
			   implement, there's no strong reason why we don't let a preceding
			   border spacing join the row in the next column instead. The
			   specs are silent, but maybe one might argue that doing it this
			   way makes it more identical to how we treat margins across
			   column boundaries. */

			columnizer.AdvanceHead(virtual_position);

			if (!columnizer.CommitContent())
				return FALSE;
		}

		columnizer.AllocateContent(virtual_position, row);
		columnizer.AdvanceHead(virtual_position + row->GetHeight());

		prev_page_break_policy = row->GetPageBreakPolicyAfter();
		prev_column_break_policy = row->GetColumnBreakPolicyAfter();
	}

	columnizer.LeaveChild(stack_position);

	return TRUE;
}

/** Figure out to which column(s) or spanned element a box belongs. */

/* virtual */ void
TableRowGroupBox::FindColumn(ColumnFinder& cf)
{
	LayoutCoord stack_position = GetStaticPositionedY();

	cf.EnterChild(stack_position);

	if (cf.GetTarget() == this)
	{
		cf.SetBoxStartFound();
		cf.SetPosition(GetHeight());
		cf.SetBoxEndFound();
	}
	else
	{
		HTML_Element* target_elm = cf.GetTarget()->GetHtmlElement();

		if (GetHtmlElement()->IsAncestorOf(target_elm))
			for (TableRowBox* row = (TableRowBox*) rows.First();
				 row && !cf.IsBoxEndFound();
				 row = (TableRowBox*) row->Suc())
				if (!row->IsVisibilityCollapsed())
					if (row->GetHtmlElement()->IsAncestorOf(target_elm))
					{
						/* Found the row or row child that we were looking
						   for. Such an element always starts and ends in the
						   same column. */

						cf.SetPosition(row->GetStaticPositionedY());
						cf.SetBoxStartFound();
						cf.SetBoxEndFound();
					}
	}

	cf.LeaveChild(stack_position);
}

/** Update cell. */

void
TableCellBox::Update(LayoutInfo& info, TableRowBox* row, LayoutCoord height, BOOL updated_all)
{
	TableCellBoxReflowState* cell_state = GetReflowState();
	LayoutCoord new_cell_y = GetPositionedY();
	LayoutCoord new_position = new_cell_y + row->GetPositionedY();
	LayoutCoord width = GetWidth();
	LayoutCoord new_x = GetPositionedX();
	BOOL pos_changed = cell_state->old_x != new_x ||
		cell_state->old_position != new_position ||
		cell_state->old_y != new_cell_y + ComputeCellY(row);
	BOOL size_changed = cell_state->old_width != width || cell_state->old_height != height;

	if (pos_changed || size_changed)
	{
		if (!updated_all)
			if (pos_changed || cell_state->update_entire_cell)
			{
				// Update entire cell

				AbsoluteBoundingBox before;
				AbsoluteBoundingBox bbox;

				if (pos_changed)
					// Include overflow
					before.Set(cell_state->old_bounding_box, cell_state->old_width, cell_state->old_height);
				else
				{
					before.Set(LayoutCoord(0), LayoutCoord(0), cell_state->old_width, cell_state->old_height);
					before.SetContentSize(before.GetWidth(), before.GetHeight());
				}

				before.Translate(cell_state->old_x, cell_state->old_position);

				GetBoundingBox(bbox, pos_changed);
				bbox.Translate(new_x, new_position);
				bbox.UnionWith(before);

				info.visual_device->UpdateRelative(bbox.GetX(), bbox.GetY(), bbox.GetWidth(), bbox.GetHeight());
			}
			else
			{
				LayoutCoord new_right = new_x + width;
				long new_bottom = new_position + height;

				LayoutCoord old_right = cell_state->old_x + MAX(cell_state->old_width, LayoutCoord(0));
				long old_bottom = cell_state->old_y + MAX(cell_state->old_height, LayoutCoord(0)); // FIXME: shouldn't it be old_position instead of old_y?

				if (new_bottom != old_bottom)
				{
					// Box changed in height; update area below box

					long update_height = old_bottom - new_bottom;
					long update_from;

					if (update_height < 0)
					{
						update_from = old_bottom;
						update_height = -update_height;
					}
					else
						update_from = new_bottom;

					info.visual_device->UpdateRelative(new_x,
													   update_from,
													   MAX(cell_state->old_width, width),
													   update_height);
				}

				if (old_right < new_right)
				{
					// Box changed in width; update area to the right of box

					long update_width = old_right - new_right;
					long update_from;

					if (update_width < 0)
					{
						update_from = old_right;
						update_width = -update_width;
					}
					else
						update_from = new_right;

					info.visual_device->UpdateRelative(update_from,
													   new_position,
													   update_width,
													   MAX(cell_state->old_height, height));
				}
			}

		cell_state->old_bounding_box = bounding_box;
		cell_state->old_x = new_x;
		cell_state->old_height = height;
		cell_state->old_width = width;
		cell_state->old_position = new_position;
	}
}

/** Get baseline of cell. */

LayoutCoord
TableCellBox::GetBaseline() const
{
	LayoutCoord baseline = content->GetBaseline();

	if (baseline == LAYOUT_COORD_MIN)
		return content->IsEmpty() ? LayoutCoord(0) : content->GetHeight();
	else
		return baseline;
}

/** Update screen. */

/* virtual */ void
TableRowBox::UpdateScreen(LayoutInfo& info)
{
	TableRowBoxReflowState* state = GetReflowState();

	TableContent* table = state->cascade->table;
	TableRowGroupBox* row_group = state->row_group_box;
	LayoutCoord x = GetPositionedX();
	LayoutCoord y = GetPositionedY();
	BOOL pos_changed = state->old_x != x || state->old_y != y;
	BOOL updated_all = FALSE;

	baseline = CalculateBaseline();

	info.Translate(-x, -y);

	if (pos_changed || state->old_height != height)
	{
		const HTMLayoutProperties& props = *state->cascade->GetProps();
		SpaceManager* space_manager = state->cascade->space_manager;

		if (space_manager->HasPendingFloats())
			space_manager->EnableFullBfcReflow();

		if (pos_changed || HasBackground(props))
		{
			// If we have a background or border, we need to update all

			LayoutCoord row_width = table->GetRowWidth();

			LayoutCoord left = MIN(state->old_x, x);
			LayoutCoord right = MAX(state->old_x + row_width, x + row_width);
			LayoutCoord top = MIN(state->old_y, y);
			LayoutCoord bottom = MAX(state->old_y + state->old_height, y + height);

			info.visual_device->UpdateRelative(left, top, right - left, bottom - top);
			updated_all = TRUE;
		}
		else
			if (state->old_height > height)
				// Box shrunk in height; update area below box

				info.visual_device->UpdateRelative(0, y + height, table->GetWidth(), state->old_height - height);

		state->old_x = x;
		state->old_y = y;
		state->old_height = height;
	}

	for (int column = table->GetLastColumn() - 1; column >= 0; column--)
	{
		int rowspan = table->GetCurrentRowspan(column);

		if (rowspan)
		{
			TableRowBox* foreign_row = NULL;

			if (TableCellBox* cell = GetRowSpannedCell(this, (unsigned int) column, 1, table->IsWrapped(), NULL, &foreign_row))
				if (foreign_row)
					// Special case for row spanned cells

					cell->Update(info,
								 foreign_row,
								 cell->GetHeight(),
								 FALSE);
				else
					cell->Update(info,
								 this,
								 height,
								 updated_all);
		}
	}

	/* Now is a good time to check if and how the cell overflow affects the
	   row, and propagate the bounding box to the row-group. */

	AbsoluteBoundingBox row_bounding_box;
	row_bounding_box.Set(LayoutCoord(0), LayoutCoord(0), table->GetRowWidth(), height);
	row_bounding_box.SetContentSize(row_bounding_box.GetWidth(), row_bounding_box.GetHeight());

	if (packed2.overflowed)
	{
		for (TableCellBox* cell = (TableCellBox *)cells.First();
			 cell;
			 cell = (TableCellBox *)cell->Suc())
		{
			if (cell->HasContentOverflow() || cell->IsPositionedBox())
			{
				/* Note: assuming overflow:visible on the cell here. We would need its layout
				   properties to tell for sure. */

				AbsoluteBoundingBox cell_bounding_box;

				cell->GetBoundingBox(cell_bounding_box);
				cell_bounding_box.Translate(cell->GetPositionedX(), cell->GetPositionedY());

				row_bounding_box.UnionWith(cell_bounding_box);
			}
		}

		if (row_bounding_box.GetX() == 0 && row_bounding_box.GetY() == 0 && row_bounding_box.GetWidth() <= table->GetRowWidth() && row_bounding_box.GetHeight() <= height)
			packed2.overflowed = 0; // None of the cells with overflow actually overflowed their row.
	}

	row_bounding_box.Translate(GetPositionedX(), GetPositionedY());
	row_group->UpdateBoundingBox(row_bounding_box);
}

/** Invalidate the screen area that the box uses. */

/* virtual */ void
TableRowBox::Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const
{
	TableRowGroupBox* row_group = NULL;

	if (parent_cascade->html_element)
	{
		OP_ASSERT(parent_cascade->html_element->GetLayoutBox()->IsTableRowGroup());
		row_group = (TableRowGroupBox *)parent_cascade->html_element->GetLayoutBox();
	}

	if (!row_group)
		row_group = parent_cascade->FindTableRowGroup();

	TableContent* table = parent_cascade->table;
	LayoutCoord y = GetPositionedY();

	// FIXME: no way to handle overflow here

	info.Translate(LayoutCoord(0), y);
	info.visual_device->UpdateRelative(0, 0, table->GetRowWidth(), height);
	info.Translate(LayoutCoord(0), -y);
}

/** Finish reflowing box. */

/* virtual */ LAYST
TableRowBox::FinishLayout(LayoutInfo& info)
{
	TableRowBoxReflowState* state = GetReflowState();
	LayoutProperties* cascade = state->cascade;
	TableContent* table = cascade->table;
	unsigned short next_column = state->next_column;

	// Skip rowspanned columns

	while (next_column < table->GetColumnCount())
	{
		if (table->SkipRowSpannedColumns(info, next_column, this))
			packed.rowspanned_from = TRUE;

		next_column++;
	}

	if (packed.rowspanned_from)
	{
		// Avoid breaking between rows that share the same cell.

		packed2.page_break_after = BREAK_AVOID;
		packed2.column_break_after = BREAK_AVOID;
	}

#ifdef PAGED_MEDIA_SUPPORT
	if (info.paged_media == PAGEBREAK_ON && !state->cascade->multipane_container)
	{
		switch (SpansMultiplePages(info))
		{
		case OpBoolean::IS_TRUE:
			if (!packed.override_vertical_align)
				// Restart layout for row

				info.workplace->SetPageBreakElement(info, state->html_element);
			break;
		case OpBoolean::IS_FALSE:
			break;
		default:
			return LAYOUT_OUT_OF_MEMORY;
		}
	}
#endif // PAGED_MEDIA_SUPPORT

	if (StackingContext* local_stacking_context = GetLocalStackingContext())
		local_stacking_context->FinishLayout(cascade);

	UpdateScreen(info);

	if (IsPositionedBox())
		info.SetRootTranslation(state->previous_root_x, state->previous_root_y);

	TableRowGroupBox* row_group = state->row_group_box;

	row_group->AddRowMinHeight(state->minimum_height);

	if (IsTableCollapsingBorderRowBox())
	{
		// Add top border, unless it's the first row in the table

		if (Pred() || row_group->Pred() && row_group->Pred()->IsRowGroup())
			row_group->AddRowMinHeight(LayoutCoord(((TableCollapsingBorderRowBox*) this)->GetBorder().top.width));
	}

	for (int column = table->GetLastColumn() - 1; column >= 0; column--)
	{
		int rowspan = table->GetCurrentRowspan(column);

		if (rowspan)
		{
			int relative_row_number = 0;
			TableCellBox* cell = GetRowSpannedCell(this, (unsigned int) column, 1, table->IsWrapped(), &relative_row_number);

			if (cell && relative_row_number <= 1)
				cell->DelayedDeleteReflowState();
		}
	}

	table->RowCompleted();

	DeleteReflowState();

	return LAYOUT_CONTINUE;
}

/** Close any rowspanned cells that might not be expired. */

void
TableRowBox::CloseRowspannedCell(LayoutInfo& info, unsigned short column, TableContent* table)
{
	// Force rowspan to be ended and adjust row height if necessary

	table->SkipRowSpannedColumns(info, column, this, NULL, TRUE);

	if (TableCellBox* box = GetRowSpannedCell(this, column, 1, FALSE))
		box->DelayedDeleteReflowState();
}

/** Add new cell. Return FALSE on OOM, TRUE otherwise. */

BOOL
TableRowBox::AddCell(LayoutCoord* x, LayoutInfo& info, TableContent* table, TableCellBox* cell, int colspan, int rowspan, LayoutCoord desired_width, LayoutCoord* cell_width)
{
	TableRowBoxReflowState* state = GetReflowState();
	LayoutCoord collapsed_width(0);

	OP_ASSERT(state);
	OP_ASSERT(!cell->InList());
	OP_ASSERT(table);

	cell->Into(&cells);

	*x = state->reflow_position;

	if (table)
	{
		for (unsigned short next_column = state->next_column + 1; next_column < state->next_column + colspan; next_column++)
			if (table->SkipRowSpannedColumns(info, next_column, this))
				packed.rowspanned_from = TRUE;

		if (!table->AdjustNumberOfColumns(state->next_column, colspan, rowspan, TRUE, info.workplace))
			return FALSE;

		table->SetDesiredColumnWidth(info, state->next_column, colspan, desired_width, TRUE);

		*cell_width = table->GetCellWidth(state->next_column, info.table_strategy == TABLE_STRATEGY_SINGLE_COLUMN || (info.table_strategy == TABLE_STRATEGY_TRUE_TABLE && !table->IsTrueTable()) ? 1 : colspan, FALSE, &collapsed_width);

		// AMSR
		if (table->IsWrapped() && state->reflow_position + *cell_width > table->GetRowWidth())
		{
			BOOL wrap = TRUE;

			if (colspan > 1)
			{
				LayoutCoord min_width;
				LayoutCoord normal_min_width;
				LayoutCoord max_width;
				LayoutCoord first_col_width = table->GetCellWidth(state->next_column, 1, FALSE);

#ifdef DEBUG
				BOOL has_minmax =
#endif
					cell->GetMinMaxWidth(min_width, normal_min_width, max_width);

#ifdef DEBUG
				OP_ASSERT(has_minmax);
#endif

				if (min_width < first_col_width)
					min_width = first_col_width;

				if (min_width <= table->GetRowWidth() - state->reflow_position)
				{
					*cell_width = table->GetRowWidth() - state->reflow_position;
					wrap = FALSE;
				}
			}

			if (wrap)
			{
				packed.is_wrapped = 1;
				state->reflow_position = LayoutCoord(0);
				*x = state->reflow_position;
			}
		}
	}
	else
	{
		*cell_width = LayoutCoord(0);
		return FALSE;
	}

	state->reflow_position += *cell_width - collapsed_width;

	if (!table->IsColumnVisibilityCollapsed(state->next_column))
		state->reflow_position += table->GetHorizontalCellSpacing();

	state->next_column += colspan;

	if (table->SkipRowSpannedColumns(info, state->next_column, this, &state->reflow_position))
		packed.rowspanned_from = TRUE;

	if (rowspan > 1)
		packed.rowspanned_from = TRUE;

	return TRUE;
}

/** Grow row to min_height. */

void
TableRowBox::GrowRow(LayoutInfo& info, LayoutCoord min_height, LayoutCoord baseline, BOOL set_specified_height)
{
	TableRowBoxReflowState* state = GetReflowState();

	// Set the has_specified_height status

	if (set_specified_height)
		if (min_height < 0)
		{
			LayoutFixed spec_height_percentage = LayoutFixed(packed.spec_height_percentage);

			if (LayoutFixedAsInt(spec_height_percentage) < -min_height)
				spec_height_percentage = LayoutFixed(-min_height);

			if (spec_height_percentage > LayoutFixed(0x00ffffff)) // 2^24 - 1
				spec_height_percentage = LayoutFixed(0x00ffffff);

			packed.spec_height_percentage = (unsigned int)LayoutFixedAsInt(spec_height_percentage);
		}
		else
			if (min_height)
				packed.spec_height_pixels = TRUE;

	/* baseline==0 either means that this row growth request was not
	   made from a table cell, or that the table cell that requested
	   the row growth was not aligned at the baseline. */

	if (baseline)
	{
		OP_ASSERT(state);

		if (state->max_below_baseline < min_height - baseline)
			state->max_below_baseline = min_height - baseline;

		if (state->baseline < baseline)
			state->baseline = baseline;

		if (min_height < state->baseline + state->max_below_baseline)
			min_height = state->baseline + state->max_below_baseline;
	}

	if (height < min_height && (!state || !state->use_old_height))
	{
		TableRowGroupBox* row_group = GetTableRowGroup(GetHtmlElement());
		LayoutCoord increment = min_height - height;

		height = min_height;

		if (row_group)
			row_group->GrowRowGroup(info, increment);
	}
}

#ifdef PAGED_MEDIA_SUPPORT

/** Attempt to break page. */

BREAKST
TableRowBox::AttemptPageBreak(LayoutInfo& info, int strength, TableRowBox*& potential_break)
{
	long y = GetPositionedY();
	long page_top = info.doc->GetRelativePageTop();
	long page_bottom = info.doc->GetRelativePageBottom();

	if (y + GetHeight() < page_top)
		return BREAK_NOT_FOUND;

	/* Do not attempt to break inside the row here. If there are break
	   opportunities inside the row, they have already been considered. */

	if (y + GetHeight() <= page_bottom)
	{
		/* This row is above the current page's bottom, so we should consider
		   breaking. */

		if (potential_break && packed2.page_break_after != BREAK_AVOID && !packed.has_trailing_page_break)
		{
			/* It is allowed to break after this row, it is allowed to break
			   before the row below us (potential_break), and there's no break
			   point here already, so break the page after us. */

			info.workplace->SetPageBreakElement(info, GetHtmlElement());

			packed.has_trailing_page_break = TRUE;
			packed.pending_page_break = TRUE;

			return BREAK_FOUND;
		}
	}

	if (y < page_top)
		/* Row is off the top of the page, no further break points
		   to be found above this. */

		return BREAK_NOT_FOUND;

	if (packed2.page_break_before != BREAK_AVOID)
		// Try to break page before us

		potential_break = this;
	else
		potential_break = NULL;

	return BREAK_KEEP_LOOKING;
}

/** Insert a page break. */

BREAKST
TableRowBox::InsertPageBreak(LayoutInfo& info, int strength)
{
	TableRowBoxReflowState* state = GetReflowState();
	TableRowGroupBox* row_group = state->row_group_box;

	if (row_group)
	{
		BREAKST status;

		info.Translate(LayoutCoord(0), -GetPositionedY());
		status = row_group->InsertPageBreak(info, strength);
		info.Translate(LayoutCoord(0), GetPositionedY());

		return status;
	}

	return BREAK_NOT_FOUND;
}

/** Return IS_TRUE if this table-row crosses page boundaries. */

OP_BOOLEAN
TableRowBox::SpansMultiplePages(const LayoutInfo& info) const
{
	LayoutCoord y = GetPositionedY();

	PageDescription* p1 = info.doc->GetPageDescription(y, TRUE);
	PageDescription* p2 = info.doc->GetPageDescription(y + height, TRUE);
	if (!p1 || !p2)
		return OpBoolean::ERR_NO_MEMORY;

	return p1 != p2 ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

#endif // PAGED_MEDIA_SUPPORT

/** Propagate minimum and maximum table cell widths. */

void
TableRowBox::PropagateCellWidths(const LayoutInfo& info, int column, int colspan, LayoutCoord desired_width, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width)
{
	TableContent* table = GetTable();

	if (table)
		table->PropagateCellWidths(info, column, colspan, desired_width, min_width, normal_min_width, max_width);
}

/** Propagate the bounding box of cell's absolutely positioned descendant to the table. */

void
TableRowBox::PropagateCellBottom(const LayoutInfo& info, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	TableContent* table = GetTable();

	if (table)
	{
		TableRowGroupBox* rowgroup = (TableRowGroupBox*)GetHtmlElement()->Parent()->GetLayoutBox();
		AbsoluteBoundingBox bounding_box = child_bounding_box;

		bounding_box.Translate(GetPositionedX() + rowgroup->GetPositionedX(),
							   GetPositionedY() + rowgroup->GetPositionedY());
		table->PropagateBottom(info, bounding_box, opts);
	}
}

/** Get used border widths, including collapsing table borders */

/* virtual */ void
TableRowBox::GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const
{
	// No table-row borders in separated borders table model.

	top = left = right = bottom = 0;
}

TableCellBox::TableCellBox(HTML_Element* element, Content* content)
  : VerticalBox(element, content),
	desired_width(0),
	x_pos(0),
	content_width(CONTENT_WIDTH_AUTO)
{
	packed_init = 0;
}

TableCollapsingBorderCellBox::TableCollapsingBorderCellBox(HTML_Element* element, Content* content)
  : TableBorderCellBox(element, content),
	packed2_init(0),
	collapsed_left_border_width(0),
	collapsed_right_border_width(0)
{
}

/** Signal that content may have changed. */

/* virtual */ void
TableCellBox::SignalChange(FramesDocument* doc, BoxSignalReason reason /* = BOX_SIGNAL_REASON_UNKNOWN */)
{
	MarkContentWithPercentageHeightDirty(doc);
}

/** Recalculate containing block size and schedule the specified descendant box for reflow if needed. */

/* virtual */ void
TableCellBox::RecalculateContainingBlock(FramesDocument* doc, AbsolutePositionedBox* box)
{
	OP_ASSERT(IsPositionedBox());

	CalculateContainingBlockHeight();

	if (HasContainingBlockSizeChanged())
		box->MarkDirty(doc, FALSE);
}

/** Initialise reflow state. */

TableCellBoxReflowState*
TableCellBox::InitialiseReflowState()
{
	if (!(un.data & 1))
	{
		TableCellBoxReflowState* state = new TableCellBoxReflowState;

		if (state)
		{
			state->html_element = un.html_element;

			un.data = ((UINTPTR)state) | 1;
		}

		return state;
	}
	else
		return (TableCellBoxReflowState*) (un.data & ~1);
}

/** Increase desired width and min/max widths, and repropagate them. */

void
TableCellBox::IncreaseWidths(const LayoutInfo& info, LayoutCoord increment)
{
	// Update widths.

	if (desired_width > 0)
		desired_width += increment;

	content->IncreaseWidths(increment);

	// Repropagate them.

	LayoutCoord min_width;
	LayoutCoord normal_min_width;
	LayoutCoord max_width;

	if (content->GetMinMaxWidth(min_width, normal_min_width, max_width))
		PropagateWidths(info, min_width, normal_min_width, max_width);
}

/** Update screen. */

/* virtual */ void
TableCellBox::UpdateScreen(LayoutInfo& info)
{
	TableCellBoxReflowState* state = GetReflowState();

	content->UpdateScreen(info);

	info.Translate(-GetPositionedX(), -state->old_y);
}

/** Finish reflowing box. */

/* virtual */ LAYST
TableCellBox::FinishLayout(LayoutInfo& info)
{
	TableCellBoxReflowState* state = GetReflowState();

	OP_ASSERT(state);

	LayoutProperties* cascade = state->cascade;

	switch (content->FinishLayout(info))
	{
	case LAYOUT_END_FIRST_LINE:
		return LAYOUT_END_FIRST_LINE;

	case LAYOUT_OUT_OF_MEMORY:
		return LAYOUT_OUT_OF_MEMORY;
	}

	OP_ASSERT(cascade);

	space_manager.FinishLayout();

	if (StackingContext* local_stacking_context = GetLocalStackingContext())
		local_stacking_context->FinishLayout(cascade);

	if (info.table_strategy == TABLE_STRATEGY_TRUE_TABLE && GetCellColSpan() == 1)
		cascade->table->AddTrueTableCondition(GetColumn(), content->IsTrueTableCandidate(), info);

	UpdateScreen(info);

	GetTableRowBox()->PropagateMinHeight(content->GetMinHeight());

	if (IsPositionedBox())
		info.SetRootTranslation(state->previous_root_x, state->previous_root_y);

	state->cascade = NULL;

	return LAYOUT_CONTINUE;
}

/** Reflow box and children. */

/* virtual */ LAYST
TableCellBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	HTML_Element* html_element = cascade->html_element;

	if (first_child && first_child != html_element)
	{
		return content->Layout(cascade, info, first_child, start_position);
	}
	else
	{
		int reflow_content = html_element->MarkClean();
		HTMLayoutProperties& local_props = *cascade->GetProps();
		TableContent* table = cascade->table;
		TableRowBox* row = GetTableRowBox();
		TableRowGroupBox* row_group = row->GetTableRowGroupBox();
		PositionedElement* pos_elm = (PositionedElement*) GetLocalZElement();

		TableCellBoxReflowState* state = InitialiseReflowState();

		if (!state ||
			!cascade->WantToModifyProperties(TRUE) ||
			Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
			return LAYOUT_OUT_OF_MEMORY;

		state->cascade = cascade;

		LayoutCoord top_padding_border = local_props.padding_top + LayoutCoord(local_props.border.top.width);
		LayoutCoord bottom_padding_border = local_props.padding_bottom + LayoutCoord(local_props.border.bottom.width);
		LayoutCoord horizontal_padding_border;
		LayoutCoord new_desired_width = local_props.content_width;
		int colspan = GetCellColSpan();
		int rowspan = GetCellRowSpan();
		unsigned int column = row->GetNextColumn();
		LayoutCoord old_pos_y = GetPositionedY();

		ModifyBorderProps(info, cascade, row);

		horizontal_padding_border = local_props.padding_left + local_props.padding_right + LayoutCoord(local_props.border.left.width + local_props.border.right.width);

		if (info.table_strategy != TABLE_STRATEGY_NORMAL)
			new_desired_width = CONTENT_WIDTH_AUTO;
		else
			if (new_desired_width == CONTENT_WIDTH_AUTO && colspan == 1 && table->GetFixedLayout() == FIXED_LAYOUT_OFF)
				/* The cell has no desired width. Check if the cell's column box has one.

				   MSIE honors width set on a column/column-group box only if
				   the table column contains at least one non-colspanned cell
				   with auto width. Firefox honors it only if the width of all
				   non-colspanned cells in the column are auto. We follow the
				   MSIE way here. */

				if (TableColGroupBox* colgroup_box = table->GetColumnBox(column))
					new_desired_width = colgroup_box->GetDesiredWidth();

		// Ignore percentage min-width and max-width, mainly because it isn't implemented. :)

		if (local_props.GetMinWidthIsPercent())
			local_props.min_width = LayoutCoord(0);

		if (local_props.GetMaxWidthIsPercent())
			local_props.max_width = LayoutCoord(-1);

		if (new_desired_width > 0)
		{
			// Width must be within the limits of min-width and max-width

			new_desired_width = local_props.CheckWidthBounds(new_desired_width);

			if (local_props.box_sizing == CSS_VALUE_content_box)
				new_desired_width += horizontal_padding_border;
		}

		state->old_bounding_box = bounding_box;
		state->old_x = GetPositionedX();
		state->old_width = content_width;
		state->old_content_height = GetCellContentHeight();

		if (pos_elm)
			pos_elm->Layout(cascade);

		BOOL percentage_pos = HasBackgroundImage(local_props) && HasPercentageBackgroundPosition(local_props);

		state->update_entire_cell = percentage_pos || /* left or bottom placement needs attention here */
									local_props.border.left.width ||
									local_props.border.top.width ||
									local_props.border.right.width ||
									local_props.border.bottom.width;

#ifdef LAYOUT_YIELD_REFLOW
		if (info.force_layout_changed)
			space_manager.EnableFullBfcReflow();
#endif // LAYOUT_YIELD_REFLOW

		LayoutCoord static_x_pos;

		if (!row->AddCell(&static_x_pos, info, table, this, colspan, rowspan, new_desired_width, &content_width))
			return LAYOUT_OUT_OF_MEMORY;

#ifdef SUPPORT_TEXT_DIRECTION
		if (table->IsRTL())
		{
			LayoutCoord new_x = table->GetRowWidth() - content_width - static_x_pos;

			static_x_pos = MAX(new_x, LayoutCoord(0));
		}
#endif

		SetStaticPositionedX(static_x_pos);

		packed.column = column;
		desired_width = new_desired_width;

		if (table->IsWrapped())
			rowspan = 1;

#ifdef PAGED_MEDIA_SUPPORT
		if (row->OverrideVerticalAlign())
			packed.vertical_alignment = CELL_ALIGN_TOP;
		else
#endif
			switch (local_props.vertical_align_type)
			{
			default:
				packed.vertical_alignment = CELL_ALIGN_BASELINE;
				break;

			case CSS_VALUE_top:
				packed.vertical_alignment = CELL_ALIGN_TOP;
				break;

			case CSS_VALUE_middle:
				packed.vertical_alignment = CELL_ALIGN_MIDDLE;
				break;

			case CSS_VALUE_bottom:
				packed.vertical_alignment = CELL_ALIGN_BOTTOM;
				break;
			}

		{
			LayoutCoord row_height(0);
			LayoutCoord cell_y(0);

			if (rowspan > 1)
			{
				if (row_group)
					// Here is a potential problem

					row_height = row_group->GetRowSpannedHeight(row, rowspan);
			}
			else
				row_height = row->GetOldHeight();

			switch (packed.vertical_alignment)
			{
			case CELL_ALIGN_BASELINE:
				cell_y = row->GetOldBaseline() - GetBaseline();
				break;

			default:
				break;

			case CELL_ALIGN_MIDDLE:
			case CELL_ALIGN_BOTTOM:
				cell_y = row_height - GetCellContentHeight();

				/* Cell height can be less than content height for a rowspanned
				   cell that is not yet complete. */

				if (cell_y < 0)
				{
					cell_y = LayoutCoord(0);
					break;
				}

				if (packed.vertical_alignment == CELL_ALIGN_MIDDLE)
					cell_y /= 2;
			}

			// AMSR
			if (row->IsWrapped())
				cell_y += GetCellYOffset();

			// FIXME: If packed.vertical_alignment has changed, old_y and old_position may be bogus, which may cause redraw problems.

			state->old_y = old_pos_y + cell_y;
			state->old_position = old_pos_y + row->GetPositionedY();
			state->old_height = row_height;
		}

		/* We ignore min-height and max-height for table-cells. This is in line with what
		   other browsers do, and correct behavior according to CSS 2.0. Not so for CSS
		   2.1, though. Together with KHTML we do support min-width and max-width. */

		local_props.min_height = LayoutCoord(0);

		if (row->UseOldRowHeight())
			local_props.max_height = state->old_height - (top_padding_border + bottom_padding_border);
		else
			local_props.max_height = LayoutCoord(-1);

		state->desired_height = LayoutCoord(0);

		if (local_props.content_height != CONTENT_HEIGHT_AUTO)
			if (rowspan == 1)
			{
				// AMSR
				LayoutCoord cell_y(0);
				LayoutCoord cell_height = local_props.content_height;
				LayoutMode layout_mode = info.doc->GetLayoutMode();

				if (row->IsWrapped())
					cell_y = GetCellYOffset();

				if (layout_mode != LAYOUT_NORMAL)
				{
					LayoutCoord min_width;
					LayoutCoord normal_min_width;
					LayoutCoord max_width;

#ifdef DEBUG
					BOOL has_minmax =
#endif
						content->GetMinMaxWidth(min_width, normal_min_width, max_width);

#ifdef DEBUG
					OP_ASSERT(has_minmax);
#endif

					if (content_width < normal_min_width)
						cell_height = cell_height * content_width / normal_min_width;
				}

				// Should baseline be honoured here?

				row->SetHasSpecifiedHeight();
				row->GrowRow(info, cell_y + cell_height, LayoutCoord(0), TRUE);
			}
			else
				if (local_props.content_height > 0)
					state->desired_height = local_props.content_height;

		CalculateContainingBlockWidth();

		info.Translate(GetPositionedX(), state->old_y);

		if (pos_elm)
		{
			info.GetRootTranslation(state->previous_root_x, state->previous_root_y);
			info.SetRootTranslation(info.GetTranslationX(), info.GetTranslationY());
		}

		if (!reflow_content)
			if (content_width != state->old_width ||
#ifdef PAGED_MEDIA_SUPPORT
				info.paged_media == PAGEBREAK_ON ||
#endif
				cascade->multipane_container ||
				info.stop_before && html_element->IsAncestorOf(info.stop_before) ||
				table->CssHeightChanged() ||
				table->IsCalculatingMinMaxWidths() && !content->HasValidMinMaxWidths())
				reflow_content = ELM_DIRTY;

		if (reflow_content)
		{
			RestartStackingContext();

			if (!space_manager.Restart())
				return LAYOUT_OUT_OF_MEMORY;

			bounding_box.Reset(local_props.DecorationExtent(info.doc), LayoutCoord(0));

#ifdef PAGED_MEDIA_SUPPORT
			if (info.paged_media == PAGEBREAK_ON && !cascade->multipane_container)
				info.doc->RewindPage(0);
#endif // PAGED_MEDIA_SUPPORT

			return content->Layout(cascade, info);
		}
		else
			if (!cascade->SkipBranch(info, FALSE, TRUE))
				return LAYOUT_OUT_OF_MEMORY;
	}

	return LAYOUT_CONTINUE;
}

/* virtual */ void
TableCellBox::Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const
{
	TableRowBox* row = GetTableRowBox();
	TableRowGroupBox* row_group = row->GetTableRowGroupBox();

	OP_ASSERT(row);

	int rowspan = GetCellRowSpan();

	LayoutCoord old_row_height(0);

	if (rowspan > 1)
		old_row_height = row_group->GetRowSpannedHeight(row, rowspan);
	else
		old_row_height = row->GetOldHeight();

	bounding_box.Invalidate(info.visual_device, GetPositionedX(), GetPositionedY(), content_width, old_row_height);
}

/** Get bounding box relative to left border edge of this cell and top edge of the row. */

/* virtual */ void
TableCellBox::GetBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow /* = TRUE */, BOOL adjust_for_multipane /* = FALSE */, BOOL apply_clip /* = TRUE */) const
{
	/* No need to handle adjust_for_multipane here. Table cells are never split
	   over multiple pages or columns, and their position relative to their row
	   are therefore not affected by paged or multicol containers. */

	GetCellBoundingBox(box, include_overflow, NULL);
}

/** Get bounding box relative to left border edge of this cell and top edge of the row.
	Extends box to include the whole cell border when borders are collapsed (for See CSS2.1 - 17.6.2). */

void
TableCellBox::GetCellBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow, const Border* collapsed_borders) const
{
	OP_PROBE3(OP_PROBE_TABLECELLBOX_GETBOUNDINGBOX);

	AbsoluteBoundingBox rows_bounding_box;
	LayoutCoord cell_height = GetHeight();

	VerticalBox::GetBoundingBox(box, include_overflow);

	/* The bounding box in VerticalBox is relative to the content height, not the box height. For
	   most box types, these values are the same, but not for table-cell boxes. Content height is
	   the height of the border-box (just like for other boxes), while box height is normally the
	   same as the total height of the rows that the cell spans across. We need the full height, to
	   be able to draw the cell background. */

	box.Translate(LayoutCoord(0), ComputeCellY(GetTableRowBox(), TRUE, cell_height));

	LayoutCoord x(0);
	LayoutCoord y(0);
	LayoutCoord width = GetWidth();
	LayoutCoord height = cell_height;

	if (collapsed_borders && include_overflow)
	{
		x = LayoutCoord(-collapsed_borders->left.width / 2);
		y = LayoutCoord(-collapsed_borders->top.width / 2);
		width += LayoutCoord((collapsed_borders->right.width + 1) / 2 - x);
		height += LayoutCoord((collapsed_borders->bottom.width + 1) / 2 - y);
	}

	rows_bounding_box.Set(x, y, width, height);
	rows_bounding_box.SetContentSize(rows_bounding_box.GetWidth(), rows_bounding_box.GetHeight());

	box.UnionWith(rows_bounding_box);
}

/** Traverse box with children. */

/* virtual */ void
PositionedTableCellBox::TraverseCell(TraversalObject* traversal_object, LayoutProperties* table_lprops, const TableRowBox* row)
{
	if (traversal_object->IsTarget(GetHtmlElement()))
		TableCellBox::TraverseCell(traversal_object, table_lprops, row);
}

/** Reflow box and children. */

/* virtual */ LAYST
PositionedZRootTableCellBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			pos_elm.SetZIndex(cascade->GetProps()->z_index);

	return PositionedTableCellBox::Layout(cascade, info, first_child, start_position);
}

/** Traverse z children. */

/* virtual */ void
PositionedZRootTableCellBox::TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip)
{
	// Traverse children with z_index < 0 if bottom-up or z_index > 0 if top-down

	stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip);
}

/** Traverse box with children. */

/* virtual */ void
PositionedTableBorderCellBox::TraverseCell(TraversalObject* traversal_object, LayoutProperties* table_lprops, const TableRowBox* row)
{
	if (traversal_object->IsTarget(GetHtmlElement()))
		TableBorderCellBox::TraverseCell(traversal_object, table_lprops, row);
}

/** Reflow box and children. */

/* virtual */ LAYST
PositionedZRootTableBorderCellBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			pos_elm.SetZIndex(cascade->GetProps()->z_index);

	return PositionedTableBorderCellBox::Layout(cascade, info, first_child, start_position);
}

/** Traverse z children. */

/* virtual */ void
PositionedZRootTableBorderCellBox::TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip)
{
	// Traverse children with z_index < 0 if bottom-up or z_index > 0 if top-down

	stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip);
}

/** Traverse box with children. */

/* virtual */ void
PositionedTableCollapsingBorderCellBox::TraverseCell(TraversalObject* traversal_object, LayoutProperties* table_lprops, const TableRowBox* row)
{
	if (traversal_object->IsTarget(GetHtmlElement()))
		TableCollapsingBorderCellBox::TraverseCell(traversal_object, table_lprops, row);
}

/** Reflow box and children. */

/* virtual */ LAYST
PositionedZRootTableCollapsingBorderCellBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			pos_elm.SetZIndex(cascade->GetProps()->z_index);

	return PositionedTableCollapsingBorderCellBox::Layout(cascade, info, first_child, start_position);
}

/** Traverse z children. */

/* virtual */ void
PositionedZRootTableCollapsingBorderCellBox::TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip)
{
	// Traverse children with z_index < 0 if bottom-up or z_index > 0 if top-down

	stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip);
}

/** Modify properties for border cell box. */

/* virtual */ void
TableBorderCellBox::ModifyBorderProps(LayoutInfo& info, LayoutProperties* cascade, TableRowBox* table_row)
{
	HTMLayoutProperties& props = *cascade->GetProps();

	border = props.border;

	if (table_row)
	{
		TableContent* table = cascade->table;
		TableRowGroupBox* row_group = cascade->FindTableRowGroup();
		TableRowBox* row = row_group->GetLastRow();

		OP_ASSERT(row);
		OP_ASSERT(row == table_row);

		TableRowBox* prev_row = (TableRowBox*) row->Pred();
		int column = table_row->GetNextColumn();
		int colspan = GetCellColSpan();
		BOOL colgroup_starts_left = FALSE;
		BOOL colgroup_stops_right = FALSE;

		table->GetColGroupStartStop(column, colspan, colgroup_starts_left, colgroup_stops_right);

		unsigned int table_frame = table->GetFrame();
		unsigned int table_rules = table->GetRules();

		if ((!prev_row && (table_frame == TABLE_FRAME_border ||
			               table_frame == TABLE_FRAME_hsides ||
						   table_frame == TABLE_FRAME_above)) ||
			(prev_row && (table_rules == TABLE_RULES_all ||
			              table_rules == TABLE_RULES_rows)))
		{
			// Turn on top border

			border.top.width = 1;
			border.top.style = CSS_VALUE_inset;
			border.top.color = DEFAULT_BORDER_COLOR;
		}

		if ((column == 0 && (table_frame == TABLE_FRAME_border ||
							 table_frame == TABLE_FRAME_vsides ||
							 table_frame == TABLE_FRAME_lhs)) ||
			(column > 0 && (table_rules == TABLE_RULES_all ||
			                table_rules == TABLE_RULES_cols)) ||
			(colgroup_starts_left && table_rules == TABLE_RULES_groups))
		{
			// Turn on left border

			border.left.width = 1;
			border.left.style = CSS_VALUE_inset;
			border.left.color = DEFAULT_BORDER_COLOR;
		}

		if (colgroup_stops_right && table_rules == TABLE_RULES_groups)
		{
			// Turn on right border

			border.right.width = 1;
			border.right.style = CSS_VALUE_inset;
			border.right.color = DEFAULT_BORDER_COLOR;
		}

		if (!prev_row && table_rules == TABLE_RULES_groups)
		{
			// Turn on top border

			border.top.width = 1;
			border.top.style = CSS_VALUE_inset;
			border.top.color = DEFAULT_BORDER_COLOR;
		}
	}

	// Set modified border

	props.border = border;
}

/** Modify properties for collapsing cell box. */

/* virtual */ void
TableCollapsingBorderCellBox::ModifyBorderProps(LayoutInfo& info, LayoutProperties* cascade, TableRowBox* table_row)
{
	TableCollapsingBorderRowBox* row_box = (TableCollapsingBorderRowBox*) table_row;
	HTMLayoutProperties& props = *cascade->GetProps();
	TableCollapsingBorderContent* table = (TableCollapsingBorderContent*) cascade->table;

#ifdef SUPPORT_TEXT_DIRECTION
	BOOL rtl = table->IsRTL();
#endif

	SetTopLeftCorner(BORDER_CORNER_STACKING_NOT_SET);
	SetTopRightCorner(BORDER_CORNER_STACKING_NOT_SET);
	SetBottomLeftCorner(BORDER_CORNER_STACKING_NOT_SET);
	SetBottomRightCorner(BORDER_CORNER_STACKING_NOT_SET);

	// Need to cache border; will be modified later

	border = props.border;

	if (!packed2.left_border_collapsed)
		collapsed_left_border_width = border.left.width;
	else
		packed2.left_border_collapsed = 0;
	if (!packed2.right_border_collapsed)
		collapsed_right_border_width = border.right.width;
	else
		packed2.right_border_collapsed = 0;

	if (!packed2.bottom_border_collapsed)
		packed2.collapsed_bottom_border_width = border.bottom.width;
	else
		packed2.bottom_border_collapsed = 0;

	short collapsed_top_border_width = border.top.width;

	packed2.hidden = props.visibility == CSS_VALUE_hidden;

	if (row_box)
	{
		// Collapse top border with top of row border

		border.top.Collapse(row_box->GetBorder().top);

		TableRowGroupBox* row_group = cascade->FindTableRowGroup();
		TableRowBox* row = row_group->GetLastRow();

		OP_ASSERT(row);
		OP_ASSERT(row == row_box);

		TableRowBox* prev_row = (TableRowBox*) row->Pred();

		if (!prev_row)
			for (TableListElement* element = row_group->Pred(); element && !prev_row; element = element->Pred())
				if (element->IsRowGroup())
				{
					row_group = (TableRowGroupBox*) element;
					prev_row = row_group->GetLastRow();
				}

		unsigned short column = row_box->GetNextColumn();
		TableColGroupBox* column_box = table->GetColumnBox(column);
		int colspan = GetCellColSpan();

		if (column_box)
		{
			// Collapse borders with column(-group) box and sibling column(-group) boxes.

			const Border* column_border = column_box->GetBorder();

			if (column_border)
			{
				TableColGroupBox* prev_column_box = NULL;

				if (column > 0)
				{
					prev_column_box = table->GetColumnBox(column - 1);

					OP_ASSERT(prev_column_box);
				}

				if (column == 0 || prev_column_box != column_box)
#ifdef SUPPORT_TEXT_DIRECTION
					if (rtl)
						border.right.Collapse(column_border->right);
					else
#endif // SUPPORT_TEXT_DIRECTION
						border.left.Collapse(column_border->left);

				if (!prev_row)
					border.top.Collapse(column_border->top);

				if (column > 0 && (prev_column_box != column_box || !column_box->IsGroup()))
				{
					column_border = prev_column_box->GetBorder();

					if (column_border)
#ifdef SUPPORT_TEXT_DIRECTION
						if (rtl)
							border.right.Collapse(column_border->left);
						else
#endif // SUPPORT_TEXT_DIRECTION
							border.left.Collapse(column_border->right);
				}
			}

			TableColGroupBox* next_column_box = table->GetColumnBox(column + colspan);

			if (colspan > 1)
				column_box = table->GetColumnBox(column + colspan - 1);

			if (column_box && (column_box != next_column_box || !column_box->IsGroup()) && column_box->GetBorder())
#ifdef SUPPORT_TEXT_DIRECTION
				if (rtl)
					border.left.Collapse(column_box->GetBorder()->left);
				else
#endif // SUPPORT_TEXT_DIRECTION
					border.right.Collapse(column_box->GetBorder()->right);

			if (next_column_box && (column_box != next_column_box || !column_box->IsGroup()))
			{
				column_border = next_column_box->GetBorder();

				if (column_border)
#ifdef SUPPORT_TEXT_DIRECTION
					if (rtl)
						border.left.Collapse(column_border->right);
					else
#endif // SUPPORT_TEXT_DIRECTION
						border.right.Collapse(column_border->left);
			}
		}

#ifdef SUPPORT_TEXT_DIRECTION
		if (rtl)
		{
			if (column == 0)
			{
				OP_ASSERT(!row_box->GetLastCell());

				// Collapse with right row border

				border.right.Collapse(row_box->GetBorder().right);

				// Collapse with right table border

				table->CollapseRightBorder(info, border.right, 0);
			}
			else
			{
				TableCollapsingBorderCellBox* right_cell = (TableCollapsingBorderCellBox*) row_box->GetLastCell();

				if (right_cell && right_cell->GetColumn() + right_cell->GetCellColSpan() == column)
				{
					short right_cell_left_border_width = right_cell->CollapseLeftBorder(info, border.right /*, table->IsWrapped() ? 1 : right_cell->GetCellRowSpan() == GetCellRowSpan()*/);

					if (collapsed_right_border_width < right_cell_left_border_width)
						collapsed_right_border_width = right_cell_left_border_width;
				}
				else
				{
					// Cell at right edge is rowspanned.

					OP_ASSERT(prev_row);

					right_cell = (TableCollapsingBorderCellBox*) TableRowBox::GetRowSpannedCell(prev_row, column - 1, 1, table->IsWrapped());

					if (right_cell)
						collapsed_right_border_width = right_cell->CollapseLeftBorder(info, border.right /*, FALSE */);
				}
			}
		}
		else

#endif
		{
			if (column == 0)
			{
				OP_ASSERT(!row_box->GetLastCell());

				// Collapse with left row border

				border.left.Collapse(row_box->GetBorder().left);

				// Collapse with left table border

				table->CollapseLeftBorder(info, border.left, 0);
			}
			else
			{
				TableCollapsingBorderCellBox* left_cell = (TableCollapsingBorderCellBox*) row_box->GetLastCell();

				if (left_cell && left_cell->GetColumn() + left_cell->GetCellColSpan() == column)
				{
					short left_cell_right_border_width = left_cell->CollapseRightBorder(info, border.left, table->IsWrapped() || left_cell->GetCellRowSpan() == GetCellRowSpan());

					if (collapsed_left_border_width < left_cell_right_border_width)
						collapsed_left_border_width = left_cell_right_border_width;
				}
				else
				{
					// Cell at left edge is rowspanned.

					OP_ASSERT(prev_row);

					left_cell = (TableCollapsingBorderCellBox*) TableRowBox::GetRowSpannedCell(prev_row, column - 1, 1, table->IsWrapped());

					if (left_cell)
						collapsed_left_border_width = left_cell->CollapseRightBorder(info, border.left, FALSE);
				}
			}
		}

		if (prev_row)
		{
			// If this cell has colspan > 1 there can be more then one top cell,
			// we need to collapse with all of them.

			TableCollapsingBorderCellBox* top_cell = NULL;
			TableCollapsingBorderCellBox* last_top_cell = NULL;
			int top_cell_colspan = 0;
			unsigned short next_column = column;

			while (next_column < column + colspan)
			{
				top_cell = (TableCollapsingBorderCellBox*) TableRowBox::GetRowSpannedCell(prev_row, next_column, 1, table->IsWrapped());

				if (top_cell)
				{
					last_top_cell = top_cell;
					top_cell_colspan = top_cell->GetCellColSpan();
					next_column = top_cell->GetColumn() + top_cell_colspan;

					LayoutCoord row_height = prev_row->GetHeight();
					if (!table->IsWrapped() && top_cell->GetCellRowSpan() > 1)
						row_height += prev_row->GetStaticPositionedY() - top_cell->GetTableRowBox()->GetStaticPositionedY();

					short top_cell_bottom_border_width = top_cell->CollapseBottomBorder(info, row_group, prev_row, row_height, border.top, top_cell->GetColumn() >= column && next_column <= column + colspan);

					if (next_column >= column + colspan)
						border.top.Collapse(top_cell->GetBorder().bottom);

					if (collapsed_top_border_width < top_cell_bottom_border_width)
						collapsed_top_border_width = top_cell_bottom_border_width;
				}
				else
					next_column++;
			}

			// Look for rowspanned cell that will become right cell of this

			if (table->GetColumnCount() > column + colspan && table->GetCurrentRowspan(column + colspan) > 1)
			{
				TableCollapsingBorderCellBox* right_cell = last_top_cell ? (TableCollapsingBorderCellBox*) last_top_cell->Suc() : NULL;

				if (!right_cell)
				{
					OP_ASSERT(prev_row->Pred());

					right_cell = (TableCollapsingBorderCellBox*) TableRowBox::GetRowSpannedCell(prev_row, column + colspan, 1, table->IsWrapped());
				}

				if (right_cell)
				{
					collapsed_right_border_width = right_cell->CollapseLeftBorder(info, border.right);
					packed2.right_border_collapsed = 1;
				}
				else
				{
					OP_ASSERT(table->IsWrapped());
				}
			}
		}
		else
		{
			// Collapse with top table border

			table->CollapseTopBorder(info, border.top);
		}

		OP_ASSERT(column + colspan > 0);

		unsigned short end_column = column + colspan - 1;

		if (end_column >= table->GetColumnCount() - 1)
		{
#ifdef SUPPORT_TEXT_DIRECTION
			if (rtl)
			{
				BorderEdge left_edge = border.left;
				table->CollapseLeftBorder(info, left_edge, end_column);
			}
			else
#endif
			{
				BorderEdge right_edge = border.right;
				table->CollapseRightBorder(info, right_edge, end_column);
			}
		}

		BorderEdge bottom_edge = border.bottom;
		table->CollapseBottomBorder(info, bottom_edge);
	}

	if (border.top.style == CSS_VALUE_hidden)
		collapsed_top_border_width = 0;
	else
		if (collapsed_top_border_width < border.top.width)
			collapsed_top_border_width = border.top.width;

	if (border.left.style == CSS_VALUE_hidden)
		collapsed_left_border_width = 0;
	else
		if (collapsed_left_border_width < border.left.width)
			collapsed_left_border_width = border.left.width;

	if (border.right.style == CSS_VALUE_hidden)
		collapsed_right_border_width = 0;
	else
		if (collapsed_right_border_width < border.right.width)
			collapsed_right_border_width = border.right.width;

	if (border.bottom.style == CSS_VALUE_hidden)
		packed2.collapsed_bottom_border_width = 0;
	else
		if (packed2.collapsed_bottom_border_width < border.bottom.width)
			packed2.collapsed_bottom_border_width = border.bottom.width;

	// Set modified border

	props.border.left.width = (collapsed_left_border_width + 1) / 2;
	props.border.top.width = (collapsed_top_border_width + 1) / 2;
	props.border.right.width = collapsed_right_border_width / 2;
	props.border.bottom.width = packed2.collapsed_bottom_border_width / 2;

	if (row_box)
		row_box->UpdateBoundingBox(props.border.top.width, props.border.bottom.width);
}

/** Collapse left border. */

short
TableCollapsingBorderCellBox::CollapseLeftBorder(LayoutInfo& info, BorderEdge& left_cell_right_edge)
{
	LayoutCoord increment = LayoutCoord((left_cell_right_edge.width + 1) / 2 - (collapsed_left_border_width + 1) / 2);

	if (left_cell_right_edge.width > collapsed_left_border_width)
		collapsed_left_border_width = left_cell_right_edge.width;

	packed2.left_border_collapsed = 1;

	if (increment > 0)
		// Adjacent border edge increased the width of this cell (and potentially its column as well).

		IncreaseWidths(info, increment);

	return collapsed_left_border_width;
}

/** Collapse right border. */

short
TableCollapsingBorderCellBox::CollapseRightBorder(LayoutInfo& info, BorderEdge& right, BOOL collapse, BOOL right_is_cell)
{
	LayoutCoord old_width = LayoutCoord(collapsed_right_border_width);

	if (collapse)
	{
		if (right_is_cell)
		{
			right.Collapse(border.right);
			border.right = right;
		}
		else
			border.right.Collapse(right);

		collapsed_right_border_width = border.right.width;
	}
	else
		if (right.width > collapsed_right_border_width)
			collapsed_right_border_width = right.width;

	packed2.right_border_collapsed = 1;

	LayoutCoord increment = LayoutCoord(collapsed_right_border_width / 2 - old_width / 2);

	if (increment > 0)
		// Adjacent border edge increased the width of this cell (and potentially its column as well).

		IncreaseWidths(info, increment);

	return collapsed_right_border_width;
}

/** Set bottom border. */

short
TableCollapsingBorderCellBox::CollapseBottomBorder(LayoutInfo& info, TableRowGroupBox* row_group, TableRowBox* row, LayoutCoord row_height, const BorderEdge& bottom, BOOL collapse)
{
	short old_bottom_width = packed2.collapsed_bottom_border_width / 2;

	//OP_ASSERT(border.bottom.width <= bottom.width);

	if (collapse)
	{
		border.bottom.Collapse(bottom);

		packed2.collapsed_bottom_border_width = border.bottom.width;
	}
	else
		if (packed2.collapsed_bottom_border_width < bottom.width)
			packed2.collapsed_bottom_border_width = bottom.width;

	packed2.bottom_border_collapsed = 1;

	if (old_bottom_width != packed2.collapsed_bottom_border_width / 2)
	{
		content->GrowHeight(LayoutCoord(packed2.collapsed_bottom_border_width / 2 - old_bottom_width));

		LayoutCoord increment = GetCellContentHeight() - row_height;

		if (increment > 0)
		{
			TableContent* table = GetTable();

			if (!table->UseOldRowHeights())
				row_group->ForceRowHeightIncrease(info, row, increment);
		}
	}

	return packed2.collapsed_bottom_border_width;
}

/** Set bottom border. Return TRUE if size is changed. */

BOOL
TableBorderCellBox::SetBottomBorder(const BorderEdge& bottom)
{
	short old_bottom_width = border.bottom.width / 2;

	OP_ASSERT(border.bottom.width <= bottom.width);

	border.bottom = bottom;

	if (old_bottom_width != border.bottom.width / 2)
	{
		content->GrowHeight(LayoutCoord(border.bottom.width / 2 - old_bottom_width));
		return TRUE;
	}
	else
		return FALSE;
}

/** Get bounding box relative to x,y position of this box */

/* virtual */ void
TableCollapsingBorderCellBox::GetBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow /* = TRUE */, BOOL adjust_for_multipane /* = FALSE */, BOOL apply_clip /* = TRUE */) const
{
	GetCellBoundingBox(box, include_overflow, &border);
}

/** Get used border widths, including collapsing table borders */

/* virtual */ void
TableCollapsingBorderCellBox::GetBorderWidths(const HTMLayoutProperties& props, short& top, short& left, short& right, short& bottom, BOOL inner) const
{
	if (inner)
	{
		top = (border.top.width + 1) / 2;
		left = (border.left.width + 1) / 2;
		right = border.right.width / 2;
		bottom = border.bottom.width / 2;
	}
	else
	{
		top = border.top.width / 2;
		left = border.left.width / 2;
		right = (border.right.width + 1) / 2;
		bottom = (border.bottom.width + 1) / 2;
	}
}

/** Paint background and border. */

void
TableCellBox::PaintBg(LayoutProperties* cascade, FramesDocument* doc, VisualDevice* visual_device, const Border* border, const Border* collapsed_border) const
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	COLORREF bg_color = BackgroundColor(props, this);
	BgImages bg_images = props.bg_images;

	HTML_Element* html_element = cascade->html_element;
	HTML_Element* bg_img_element = html_element;

	long box_width = GetWidth();
	long box_height = GetHeight();

	BOOL background_drawn_by_root = CheckBackgroundOnRoot(doc, html_element);

	if (!doc->GetHLDocProfile()->IsInStrictMode())
	{
		BOOL has_default_background = (bg_color == USE_DEFAULT_COLOR || bg_images.IsEmpty());
		if (has_default_background)
		{
			LayoutProperties* row_cascade = cascade->Pred();

			if (bg_color == USE_DEFAULT_COLOR)
				bg_color = BackgroundColor(*row_cascade->GetProps());

			if (bg_images.IsEmpty())
			{
				bg_images = row_cascade->GetProps()->bg_images;

				if (!bg_images.IsEmpty())
					bg_img_element = row_cascade->html_element;
			}

			TableContent* table = GetTable();
			TableColGroupBox* column_box = table->GetColumnBox(GetColumn());

			if (column_box)
			{
				if (bg_color == USE_DEFAULT_COLOR)
					bg_color = column_box->GetBackgroundColor();

				if (bg_images.IsEmpty())
				{
					bg_images = column_box->GetBackgroundImages();

					if (!bg_images.IsEmpty())
						bg_img_element = column_box->GetHtmlElement();
				}

				if (bg_color == USE_DEFAULT_COLOR && bg_images.IsEmpty())
				{
					Box* parent_box = column_box->GetHtmlElement()->Parent()->GetLayoutBox();
					if (parent_box->IsTableColGroup())
					{
						bg_color = ((TableColGroupBox*)parent_box)->GetBackgroundColor();
						bg_images = ((TableColGroupBox*)parent_box)->GetBackgroundImages();

						if (!bg_images.IsEmpty())
							bg_img_element = parent_box->GetHtmlElement();
					}
				}
			}
		}
	}
	else if (bg_color == USE_DEFAULT_COLOR)
	{
		// FIX: optimize by checking of a higher layer has color!=default before painting unneccesary bg.
		// FIX: follow spec on empty cell
		TableContent* table = GetTable();

		TableColGroupBox* colgroupbox = NULL;
		TableColGroupBox* colbox = table->GetColumnBox(GetColumn());
		if (colbox)
		{
			Box* parent_box = colbox->GetHtmlElement()->Parent()->GetLayoutBox();
			if (parent_box->IsTableColGroup())
				colgroupbox = (TableColGroupBox*) parent_box;
		}

		LayoutProperties* row_cascade = cascade->Pred();
		LayoutProperties* rowgroup_cascade = row_cascade->Pred();
		if (!rowgroup_cascade->html_element->GetLayoutBox()->IsTableRowGroup())
			rowgroup_cascade = NULL;

		LayoutProperties* table_cascade = cascade->Pred();
		while(table_cascade && !table_cascade->html_element->GetLayoutBox()->GetTableContent())
			table_cascade = table_cascade->Pred();

		OP_ASSERT(table_cascade);

		if (table_cascade)
		{
			BOOL border_collapsed = !!collapsed_border;

			// Column group
			PaintColumnBg(cascade, table_cascade, doc, visual_device, colgroupbox, colbox, border_collapsed);

			// Column
			PaintColumnBg(cascade, table_cascade, doc, visual_device, colbox, NULL, border_collapsed);

			// Row group
			PaintRowBg(cascade, table_cascade, doc, visual_device, rowgroup_cascade, row_cascade, border_collapsed);

			// Row
			PaintRowBg(cascade, table_cascade, doc, visual_device, row_cascade, NULL, border_collapsed);
		}
	}

	long bg_x_pos = 0;
	long bg_y_pos = 0;
	long bg_box_width = box_width;
	long bg_box_height = box_height;

	if (collapsed_border)
	{
		bg_x_pos += (collapsed_border->left.width + 1) / 2;
		bg_y_pos += (collapsed_border->top.width + 1) / 2;
		bg_box_width -= (collapsed_border->left.width + collapsed_border->right.width + 1) / 2;
		bg_box_height -= (collapsed_border->top.width + collapsed_border->bottom.width + 1) / 2;
	}

	BOOL paint = !content->IsEmpty();

	if (!paint && props.empty_cells == CSS_VALUE_show)
	{
		paint = TRUE;

		if (props.GetSkipEmptyCellsBorder())
			border = 0;
	}

	if (paint)
	{
		BackgroundsAndBorders bb(doc, visual_device, cascade, border);

		if (ScrollableArea* scrollable = GetScrollable())
			bb.SetScrollOffset(scrollable->GetViewX(), scrollable->GetViewY());

		OpRect border_box(0, 0, box_width, box_height);
		OpRect positioning_area(bg_x_pos, bg_y_pos, bg_box_width, bg_box_height);

		bb.OverridePositioningRect(positioning_area);
		if (!background_drawn_by_root)
			bb.PaintBackgrounds(bg_img_element, bg_color, props.font_color, bg_images, &props.box_shadows, border_box);

		bb.PaintBorders(bg_img_element, border_box, props.font_color);
	}
}

/** Paint background on columns or columngroups in strict mode. */

void
TableCellBox::PaintColumnBg(LayoutProperties* cascade, LayoutProperties* table_lprops, FramesDocument* doc, VisualDevice* visual_device, TableColGroupBox* colgroupbox, TableColGroupBox* child_colbox, BOOL border_collapsed) const
{
	BOOL colgroupbox_has_background = (colgroupbox && (!colgroupbox->GetBackgroundImages().IsEmpty() || colgroupbox->GetBackgroundColor() != USE_DEFAULT_COLOR));

	if (colgroupbox_has_background)
	{
		TableContent* table = (TableContent*) ((Content_Box*) table_lprops->html_element->GetLayoutBox())->GetContent();
		TableRowGroupBox* rowgroup = cascade->FindTableRowGroup();
		TableRowBox* row = GetTableRowBox();
		LayoutCoord bg_box_width = colgroupbox->GetWidth();
		LayoutCoord bg_x_pos(0);

		if (child_colbox)
			// Position relative to group from child

			bg_x_pos = colgroupbox->GetX() - child_colbox->GetX();

		long bg_y_pos = table->GetRowsTop() - rowgroup->GetPositionedY() - row->GetPositionedY();
		long bg_box_height = table->GetRowsHeight();

		if (row->IsWrapped())
			bg_y_pos -= GetCellYOffset();

		/* No cascade and no border. We may miscalculate padding- and content-box from the border-box. */
		BackgroundsAndBorders bb(doc, visual_device, NULL, NULL);

		bb.OverridePositioningRect(OpRect(bg_x_pos, bg_y_pos, bg_box_width, bg_box_height));
		OpRect border_box(0, 0, GetWidth(), GetHeight());

		bb.PaintBackgrounds(colgroupbox->GetHtmlElement(),
							colgroupbox->GetBackgroundColor(),
							colgroupbox->GetFontColor(),
							colgroupbox->GetBackgroundImages(),
							colgroupbox->GetBoxShadows(),
							border_box);
	}
}

/** Paint background on rows or rowgroups in strict mode. */

void
TableCellBox::PaintRowBg(LayoutProperties* cascade, LayoutProperties* table_lprops, FramesDocument* doc, VisualDevice* visual_device, LayoutProperties* rowgroup_cascade, LayoutProperties* row_cascade, BOOL border_collapsed) const
{
	if (rowgroup_cascade && (HasBackground(*rowgroup_cascade->GetProps())))
	{
		const HTMLayoutProperties& table_props = *table_lprops->GetProps();
		TableContent* table = cascade->table;

		TableRowBox* row_box = GetTableRowBox();
		Box* box = rowgroup_cascade->html_element->GetLayoutBox();

		long bg_y_pos = 0;
		long bg_box_height = GetHeight();
		if (box->IsTableRowGroup())
		{
			TableRowGroupBox* rowgroupbox = (TableRowGroupBox*) box;
			long top_y = rowgroupbox->GetFirstRow()->GetPositionedY();
			long bottom_y = rowgroupbox->GetLastRow()->GetPositionedY() + rowgroupbox->GetLastRow()->GetHeight();

			bg_box_height = bottom_y - top_y;
			bg_y_pos = top_y - row_box->GetPositionedY();
		}

		const Border* tborder = &table_props.border;

		long bg_x_pos = -GetPositionedX();
		long bg_box_width = table->GetWidth();

		if (border_collapsed)
		{
			short top, left, right, bottom;
			table->GetBorderWidths(table_props, top, left, right, bottom, FALSE);
			bg_box_width -= left + right;
			bg_x_pos += left;
		}
		else
		{
			bg_box_width -= table_props.border_spacing_vertical * 2
							+ tborder->left.width + tborder->right.width;
			bg_x_pos += table_props.border_spacing_vertical + tborder->left.width;
		}

		BackgroundsAndBorders bb(doc, visual_device, rowgroup_cascade, NULL);
		OpRect border_box(0, 0, GetWidth(), GetHeight());
		bb.OverridePositioningRect(OpRect(bg_x_pos, bg_y_pos, bg_box_width, bg_box_height));

		if (packed.column)
			bb.SetExpandImagePosition();

		const HTMLayoutProperties &props = *rowgroup_cascade->GetProps();
		bb.PaintBackgrounds(rowgroup_cascade->html_element,
							props.bg_color,
							props.font_color,
							props.bg_images,
							&props.box_shadows,
							border_box);
	}
}

/** Paint background and border of this box on screen. */

/* virtual */ void
TableCellBox::PaintBgAndBorder(LayoutProperties* cascade, FramesDocument* doc, VisualDevice* visual_device) const
{
	PaintBg(cascade, doc, visual_device, &cascade->GetProps()->border, NULL);
}

/** Paint background and border of this box on screen. */

/* virtual */ void
TableBorderCellBox::PaintBgAndBorder(LayoutProperties* cascade, FramesDocument* doc, VisualDevice* visual_device) const
{
	PaintBg(cascade, doc, visual_device, &border, NULL);

	short border_spacing_horizontal = 0;
	short border_spacing_vertical = 0;

	for (LayoutProperties* search = cascade->Pred(); search; search = search->Pred())
	{
		Box* box = search->html_element->GetLayoutBox();

		if (box->GetTableContent())
		{
			border_spacing_horizontal = search->GetCascadingProperties().border_spacing_horizontal;
			border_spacing_vertical = search->GetCascadingProperties().border_spacing_vertical;

			break;
		}
	}

	if (border_spacing_horizontal > 0)
	{
		if (border.top.style != CSS_VALUE_none && border.top.width)
		{
			if (border.left.style == CSS_VALUE_none || border.left.width == 0)
				visual_device->LineOut(-border_spacing_horizontal, 0, border_spacing_horizontal, border.top.width, border.top.style, border.top.color, TRUE, TRUE, 0, 0);
			if (border.right.style == CSS_VALUE_none || border.right.width == 0)
				visual_device->LineOut(GetWidth(), 0, border_spacing_horizontal, border.top.width, border.top.style, border.top.color, TRUE, TRUE, 0, 0);
		}

		if (border.bottom.style != CSS_VALUE_none && border.bottom.width)
		{
			if (border.left.style == CSS_VALUE_none || border.left.width == 0)
				visual_device->LineOut(-border_spacing_horizontal, GetHeight() - 1, border_spacing_horizontal, border.bottom.width, border.bottom.style, border.bottom.color, TRUE, FALSE, 0, 0);
			if (border.right.style == CSS_VALUE_none || border.right.width == 0)
				visual_device->LineOut(GetWidth(), GetHeight() - 1, border_spacing_horizontal, border.bottom.width, border.bottom.style, border.bottom.color, TRUE, FALSE, 0, 0);
		}
	}

	if (border_spacing_vertical > 0)
	{
		if (border.left.style != CSS_VALUE_none && border.left.width)
		{
			if (border.top.style == CSS_VALUE_none || border.top.width == 0)
				visual_device->LineOut(0, -border_spacing_vertical, border_spacing_vertical, border.left.width, border.left.style, border.left.color, FALSE, TRUE, 0, 0);
			if (border.bottom.style == CSS_VALUE_none || border.bottom.width == 0)
				visual_device->LineOut(0, GetHeight(), border_spacing_vertical, border.left.width, border.left.style, border.left.color, FALSE, TRUE, 0, 0);
		}

		if (border.right.style != CSS_VALUE_none && border.right.width)
		{
			if (border.top.style == CSS_VALUE_none || border.top.width == 0)
				visual_device->LineOut(GetWidth() - 1, -border_spacing_vertical, border_spacing_vertical, border.right.width, border.right.style, border.right.color, FALSE, FALSE, 0, 0);
			if (border.bottom.style == CSS_VALUE_none || border.bottom.width == 0)
				visual_device->LineOut(GetWidth() - 1, GetHeight(), border_spacing_vertical, border.right.width, border.right.style, border.right.color, FALSE, FALSE, 0, 0);
		}
	}
}

/** Paint background and border of this box on screen. */

/* virtual */ void
TableCollapsingBorderCellBox::PaintBgAndBorder(LayoutProperties* cascade, FramesDocument* doc, VisualDevice* visual_device) const
{
	PaintBg(cascade, doc, visual_device, NULL, &border);
}

void
TableCollapsingBorderCellBox::PaintCollapsedBorders(PaintObject* paint_object,
													LayoutCoord width,
													LayoutCoord height,
												    TableCollapsingBorderCellBox* top_left_cell,
												    TableCollapsingBorderCellBox* left_cell,
												    TableCollapsingBorderCellBox* bottom_left_cell,
												    TableCollapsingBorderCellBox* top_cell,
												    TableCollapsingBorderCellBox* bottom_cell,
												    TableCollapsingBorderCellBox* top_right_cell,
												    TableCollapsingBorderCellBox* right_cell,
												    TableCollapsingBorderCellBox* bottom_right_cell)
{
	if (packed2.hidden)
		return;

	TableCellBorderCornerStacking corner;
	int start;
	int end;
	int top_border_width;
	int left_border_width;
	int right_border_width;
	int bottom_border_width;

	VisualDevice* visual_device = paint_object->GetVisualDevice();

	Border draw_border = border;

	if (draw_border.right.style == CSS_VALUE_outset)
		draw_border.right.style = CSS_VALUE_ridge;
	else
		if (draw_border.right.style == CSS_VALUE_inset)
			draw_border.right.style = CSS_VALUE_groove;

	if (draw_border.bottom.style == CSS_VALUE_outset)
		draw_border.bottom.style = CSS_VALUE_ridge;
	else
		if (draw_border.bottom.style == CSS_VALUE_inset)
			draw_border.bottom.style = CSS_VALUE_groove;

	// Top border

	if (top_cell != this)
	{
		if (top_cell)
			draw_border.top.Collapse(top_cell->GetBorder().bottom);

		if (draw_border.top.style != CSS_VALUE_none && draw_border.top.width)
		{
			start = 0;
			end = width;
			left_border_width = 0;
			right_border_width = 0;

			if (draw_border.top.style == CSS_VALUE_outset)
				draw_border.top.style = CSS_VALUE_ridge;
			else
				if (draw_border.top.style == CSS_VALUE_inset)
					draw_border.top.style = CSS_VALUE_groove;

			corner = BORDER_CORNER_STACKING_NOT_SET;

			if (left_cell != this)
				corner = GetTopLeftCorner();
			else
				if (top_cell && top_cell != top_left_cell)
					corner = top_cell->GetBottomLeftCorner();

			switch (corner)
			{
			case TOP_LEFT_ON_TOP:
				OP_ASSERT(top_left_cell);
				start = (top_left_cell->GetBorder().right.width + 1) / 2;
				break;

			case TOP_RIGHT_ON_TOP:
				OP_ASSERT(top_cell);
				left_border_width = -top_cell->GetBorder().left.width;
				start = (-left_border_width - 1) / 2;
				break;

			case BOTTOM_LEFT_ON_TOP:
				OP_ASSERT(left_cell);
				start = (left_cell->GetBorder().right.width + 1) / 2;
				break;

			case BOTTOM_RIGHT_ON_TOP:
				left_border_width = draw_border.left.width;
				start = -left_border_width / 2;
				break;
			}

			corner = BORDER_CORNER_STACKING_NOT_SET;

			if (right_cell != this)
				corner = GetTopRightCorner();
			else
				if (top_cell && top_cell != top_right_cell)
					corner = top_cell->GetBottomRightCorner();

			switch (corner)
			{
			case TOP_LEFT_ON_TOP:
				OP_ASSERT(top_cell);
				right_border_width = -top_cell->GetBorder().right.width;
				end += right_border_width / 2 + 1;
				break;

			case TOP_RIGHT_ON_TOP:
				OP_ASSERT(top_right_cell);
				end -= top_right_cell->GetBorder().left.width / 2;
				break;

			case BOTTOM_LEFT_ON_TOP:
				right_border_width = draw_border.right.width;
				end += (right_border_width + 1) / 2;
				break;

			case BOTTOM_RIGHT_ON_TOP:
				OP_ASSERT(right_cell);
				end -= right_cell->GetBorder().left.width / 2;
				break;
			}

			RECT local_rect = {
				MIN(start, start + left_border_width),
				- draw_border.top.width / 2,
				MAX(end, end - right_border_width),
				(draw_border.top.width + 1) / 2
			};

			if (paint_object->Intersects(local_rect))
				visual_device->LineOut(start, -draw_border.top.width / 2, end - start, draw_border.top.width, draw_border.top.style, draw_border.top.color, TRUE, TRUE, left_border_width, right_border_width);
		}
	}

	// Left border

	if (left_cell != this)
	{
		if (left_cell)
			draw_border.left.Collapse(left_cell->GetBorder().right);

		if (draw_border.left.style != CSS_VALUE_none && draw_border.left.width)
		{
			start = 0;
			end = height;
			top_border_width = 0;
			bottom_border_width = 0;

			if (draw_border.left.style == CSS_VALUE_outset)
				draw_border.left.style = CSS_VALUE_ridge;
			else
				if (draw_border.left.style == CSS_VALUE_inset)
					draw_border.left.style = CSS_VALUE_groove;

			corner = BORDER_CORNER_STACKING_NOT_SET;

			if (top_cell != this)
				corner = GetTopLeftCorner();
			else
				if (left_cell && left_cell != top_left_cell)
					corner = left_cell->GetTopRightCorner();
				else
					if (!left_cell && top_left_cell)
						corner = TOP_LEFT_ON_TOP;

			switch (corner)
			{
			case TOP_LEFT_ON_TOP:
				OP_ASSERT(top_left_cell);
				start = (top_left_cell->GetBorder().bottom.width + 1) / 2;
				break;

			case TOP_RIGHT_ON_TOP:
				OP_ASSERT(top_cell);
				start = (top_cell->GetBorder().bottom.width + 1) / 2;
				break;

			case BOTTOM_LEFT_ON_TOP:
				OP_ASSERT(left_cell);
				top_border_width = -left_cell->GetBorder().top.width;
				start = (-top_border_width - 1) / 2;
				break;

			case BOTTOM_RIGHT_ON_TOP:
				top_border_width = draw_border.top.width;
				start = -top_border_width / 2;
				break;
			}

			corner = BORDER_CORNER_STACKING_NOT_SET;

			if (bottom_cell != this)
				corner = GetBottomLeftCorner();
			else
				if (left_cell && left_cell != bottom_left_cell)
					corner = left_cell->GetBottomRightCorner();
				else
					if (!left_cell && bottom_left_cell)
						corner = BOTTOM_LEFT_ON_TOP;

			switch (corner)
			{
			case TOP_LEFT_ON_TOP:
				OP_ASSERT(left_cell);
				bottom_border_width = -left_cell->GetBorder().bottom.width;
				end += bottom_border_width / 2 + 1;
				//end += (bottom_border_width + 1) / 2 /*+ 1*/;
				break;

			case TOP_RIGHT_ON_TOP:
				bottom_border_width = draw_border.bottom.width;
				end += (bottom_border_width + 1) / 2;
				break;

			case BOTTOM_LEFT_ON_TOP:
				OP_ASSERT(bottom_left_cell);
				end -= bottom_left_cell->GetBorder().top.width / 2;
				break;

			case BOTTOM_RIGHT_ON_TOP:
				OP_ASSERT(bottom_cell);
				end -= bottom_cell->GetBorder().top.width / 2;
				break;
			}

			RECT local_rect = {
				- draw_border.left.width / 2,
				MIN(start, start + top_border_width),
				draw_border.left.width,
				MAX(end, end - bottom_border_width)
			};

			if (paint_object->Intersects(local_rect))
				visual_device->LineOut(-draw_border.left.width / 2, start, end - start, draw_border.left.width, draw_border.left.style, draw_border.left.color, FALSE, TRUE, top_border_width, bottom_border_width);
		}
	}

	// Right border (only if no right cell)

	if (!right_cell && draw_border.right.style != CSS_VALUE_none && draw_border.right.width)
	{
		start = 0;
		end = height;
		top_border_width = 0;
		bottom_border_width = 0;

		if (draw_border.right.style == CSS_VALUE_outset)
			draw_border.right.style = CSS_VALUE_ridge;
		else
			if (draw_border.right.style == CSS_VALUE_inset)
				draw_border.right.style = CSS_VALUE_groove;

		corner = BORDER_CORNER_STACKING_NOT_SET;

		if (top_cell != this)
			corner = GetTopRightCorner();
		else
			if (top_right_cell)
				corner = top_right_cell->GetBottomLeftCorner();

		switch (corner)
		{
		case TOP_LEFT_ON_TOP:
			OP_ASSERT(top_cell && top_cell != this);
			start = (top_cell->GetBorder().bottom.width + 1) / 2;
			break;

		case TOP_RIGHT_ON_TOP:
			OP_ASSERT(top_right_cell);
			start = (top_right_cell->GetBorder().bottom.width + 1) / 2;
			break;

		case BOTTOM_LEFT_ON_TOP:
			OP_ASSERT(top_cell != this);
			top_border_width = draw_border.top.width;
			start = -top_border_width / 2;
			break;

		case BOTTOM_RIGHT_ON_TOP:
			OP_ASSERT(FALSE);
			break;
		}

		corner = BORDER_CORNER_STACKING_NOT_SET;

		if (bottom_cell != this)
			corner = GetBottomRightCorner();
		else
			if (bottom_right_cell)
				corner = bottom_right_cell->GetTopLeftCorner();

		switch (corner)
		{
		case TOP_LEFT_ON_TOP:
			OP_ASSERT(bottom_cell != this);
			bottom_border_width = draw_border.bottom.width;
			end += (bottom_border_width + 1) / 2;
			break;

		case BOTTOM_LEFT_ON_TOP:
			OP_ASSERT(bottom_cell && bottom_cell != this);
			end -= bottom_cell->GetBorder().top.width / 2;
			break;

		case BOTTOM_RIGHT_ON_TOP:
			OP_ASSERT(bottom_right_cell);
			end -= bottom_right_cell->GetBorder().top.width / 2;
			break;

		case TOP_RIGHT_ON_TOP:
			OP_ASSERT(FALSE);
			break;
		}

		RECT local_rect = {
			width - (draw_border.right.width) / 2,
			start,
			width + draw_border.right.width,
			end
		};

		if (paint_object->Intersects(local_rect))
			visual_device->LineOut(width + (draw_border.right.width + 1) / 2 - 1, start, end - start, draw_border.right.width, draw_border.right.style, draw_border.right.color, FALSE, FALSE, top_border_width, bottom_border_width);
	}

	// Bottom border (only if no bottom cell)

	if (!bottom_cell && draw_border.bottom.style != CSS_VALUE_none && draw_border.bottom.width)
	{
		start = 0;
		end = width;
		left_border_width = 0;
		right_border_width = 0;

		if (draw_border.bottom.style == CSS_VALUE_outset)
			draw_border.bottom.style = CSS_VALUE_ridge;
		else
			if (draw_border.bottom.style == CSS_VALUE_inset)
				draw_border.bottom.style = CSS_VALUE_groove;

		corner = BORDER_CORNER_STACKING_NOT_SET;

		if (left_cell != this)
			corner = GetBottomLeftCorner();
		else
			if (bottom_left_cell)
				corner = bottom_left_cell->GetTopRightCorner();

		switch (corner)
		{
		case TOP_LEFT_ON_TOP:
			OP_ASSERT(left_cell && left_cell != this);
			start = (left_cell->GetBorder().right.width + 1) / 2;
			break;

		case TOP_RIGHT_ON_TOP:
			OP_ASSERT(left_cell != this);
			left_border_width = draw_border.left.width;
			start = -left_border_width / 2;
			break;

		case BOTTOM_LEFT_ON_TOP:
			OP_ASSERT(bottom_left_cell);
			start = (bottom_left_cell->GetBorder().right.width + 1) / 2;
			break;

		case BOTTOM_RIGHT_ON_TOP:
			OP_ASSERT(FALSE);
			break;
		}

		corner = BORDER_CORNER_STACKING_NOT_SET;

		if (right_cell != this)
			corner = GetBottomRightCorner();
		else
			if (bottom_right_cell)
				corner = bottom_right_cell->GetTopLeftCorner();

		switch (corner)
		{
		case TOP_LEFT_ON_TOP:
			OP_ASSERT(right_cell != this);
			right_border_width = draw_border.right.width;
			end += (right_border_width + 1) / 2;
			break;

		case TOP_RIGHT_ON_TOP:
			OP_ASSERT(right_cell && right_cell != this);
			end -= right_cell->GetBorder().left.width / 2;
			break;

		case BOTTOM_RIGHT_ON_TOP:
			OP_ASSERT(bottom_right_cell);
			end -= bottom_right_cell->GetBorder().left.width / 2;
			break;

		case BOTTOM_LEFT_ON_TOP:
			OP_ASSERT(FALSE);
			break;
		}

		RECT local_rect = {
			start,
			height - (draw_border.bottom.width + 1) / 2,
			end,
			height + draw_border.bottom.width
		};

		if (paint_object->Intersects(local_rect))
			visual_device->LineOut(start, height + (draw_border.bottom.width + 1) / 2 - 1, end - start, draw_border.bottom.width, draw_border.bottom.style, draw_border.bottom.color, TRUE, FALSE, left_border_width, right_border_width);
	}
}

/** Propagate bounding box for floating and absolute positioned boxes. See declaration in header file for more information. */

/* virtual */ void
TableCellBox::PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	// A table cell has its own space manager, so you never propagate a float past it:
	OP_ASSERT(opts.type != PROPAGATE_FLOAT);

	TableRowBox* row = GetTableRowBox();

	/* FIXME: Since we don't know the Y position of the table cell at this point, updating the
	   bounding box now is wrong *unless* the top and bottom properties of the absolutely
	   positioned box both are auto ("static" Y position in the bounding box). For non-static Y
	   position, we cannot update the bounding box before layout of the table is done (Sibling
	   cells affect the Y position of the cell. Captions and table row groups in the same table
	   later in the document may move the table row group to which this cell belongs further down).
	   See how the bounding box of the absolutely positioned box is set up in
	   AbsolutePositionedBox::PropagateBottomMargins() to understand why things are wrong.

	   Symptom: Redraw problems, unwanted or missing vertical scrollbar */

	/** Update bounding boxes - if we're between an abspos element and its containing block
		no clipping can be done (bounding needs boxes to be updated). After we have passed containing element
		we may safely clip any overflow (by skipping bbox update for example) */

	BOOL is_positioned_box = IsPositionedBox();

	if (opts.type == PROPAGATE_ABSPOS_BOTTOM_ALIGNED)
	{
		StackingContext *context = GetLocalStackingContext();

		if (context)
			context->SetNeedsBottomAlignedUpdate();

		if (is_positioned_box)
		{
			if (!context)
			{
				ReflowState* state = GetReflowState();
				state->cascade->stacking_context->SetNeedsBottomAlignedUpdate();
			}

			return;
		}
	}
	else if (opts.type != PROPAGATE_ABSPOS_SKIPBRANCH || IsOverflowVisible())
		UpdateBoundingBox(child_bounding_box, !is_positioned_box);

	if (is_positioned_box || opts.type == PROPAGATE_ABSPOS_SKIPBRANCH)
	{
		/** We either reached a containing element or we already passed it */

		if (GetReflowState())
			/** If we're not in SkipBranch or we reached element that is still being reflowed
				we can safely stop bbox propagation here. Bounding box will be propagated
				when layout of this box is finished */

			return;

		/** We've already found a containing element but since it had no reflow state we must be
			skipping a branch. Switch propagation type (this will enable us to clip such bbox)
			and continue propagation until we reach first element that is being reflowed */
		opts.type = PROPAGATE_ABSPOS_SKIPBRANCH;
	}

	if (row)
	{
		/* FIXME: Since we don't know the Y position of the table cell at this point, propagating
		   the bounding box now is wrong *if* the top and bottom properties of the absolutely
		   positioned box both are auto. See previous FIXME comment (but note that this is about
		   the opposite case (static vs. non-static position)).

		   Symptom: Unwanted or missing vertical scrollbar */

		AbsoluteBoundingBox box;

		if (opts.type == PROPAGATE_ABSPOS_SKIPBRANCH)
			GetBoundingBox(box, IsOverflowVisible());
		else
			box = child_bounding_box;

		box.Translate(GetPositionedX(), GetPositionedY());

		row->PropagateCellBottom(info, box, opts);
	}
}

/** Set child's bottom margins in this block and propagate changes to parents. See declaration in header file for more information. */

/* virtual */ void
TableCellBox::PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* /* bottom_margin */, BOOL /*has_inflow_content */)
{
	TableRowBox* row = GetTableRowBox();

	if (row)
	{
		if (row->IsTableWrapped() || GetCellRowSpan() == 1)
		{
			// AMSR
			LayoutCoord cell_y(0);
			if (row->IsWrapped())
				cell_y = GetCellYOffset();

			row->GrowRow(info, cell_y + content->GetHeight(), IsBaselineAligned() ? GetBaseline() : LayoutCoord(0));
		}

		if (HasContentOverflow() || IsPositionedBox())
			if (GetCascade()->GetProps()->overflow_x == CSS_VALUE_visible)
				/* This cell has visible overflow (or is positioned), but it is too early to check
				   if (or how) that affects the bounding-box of the table row. We don't know the
				   height of the row or the vertical position of this cell yet. Need to get back to
				   this when the table row has been laid out. */

				row->SetHasOverflowedCell();
	}
}

/** Do we need to calculate min/max widths of this box's content? */

/* virtual */ BOOL
TableCellBox::NeedMinMaxWidthCalculation(LayoutProperties* cascade) const
{
	return cascade->table->IsCalculatingMinMaxWidths();
}

/** Propagate widths to container / table. */

/* virtual */ void
TableCellBox::PropagateWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width)
{
	TableRowBox* row = GetTableRowBox();

	if (row)
		row->PropagateCellWidths(info, packed.column, GetCellColSpan(), desired_width, min_width, normal_min_width, max_width);
}

#ifdef PAGED_MEDIA_SUPPORT

/** Insert a page break. */

/* virtual */ BREAKST
TableCellBox::InsertPageBreak(LayoutInfo& info, int strength)
{
	TableRowBox* row = GetTableRowBox();

	if (row)
	{
		TableCellBoxReflowState* state = GetReflowState();
		BREAKST status;

		info.Translate(LayoutCoord(0), -state->old_y);
		status = row->InsertPageBreak(info, strength);
		info.Translate(LayoutCoord(0), state->old_y);

		return status;
	}

	return BREAK_NOT_FOUND;
}

#endif // PAGED_MEDIA_SUPPORT

/** Find table row box */

TableRowBox*
TableCellBox::GetTableRowBox() const
{
	HTML_Element* parent = GetHtmlElement()->Parent();

	OP_ASSERT(parent);
	OP_ASSERT(parent->GetLayoutBox());
	OP_ASSERT(parent->GetLayoutBox()->IsTableRow());

	return (TableRowBox*) parent->GetLayoutBox();
}

TableCaptionBox::TableCaptionBox(HTML_Element* element, Content* content)
  : VerticalBox(element, content),
	packed_init(0),
	content_width(LAYOUT_COORD_HALF_MAX),
	x_pos(0),
	y_pos(0)
{
}

/** Update screen. */

/* virtual */ void
TableCaptionBox::UpdateScreen(LayoutInfo& info)
{
	VerticalBoxReflowState* state = GetReflowState();
	/* if (!state) This is useless
		return; */
	LayoutProperties* cascade = state->cascade;
	LayoutCoord x = GetPositionedX();
	LayoutCoord y = GetPositionedY();
	BOOL pos_changed = state->old_x != x || state->old_y != y;
	LayoutCoord height;

	content->UpdateScreen(info);

	height = content->GetHeight();

	CheckAbsPosDescendants(info);

	info.Translate(-x, -y);

	if (pos_changed ||
		state->old_width != content_width ||
		state->old_height != height)
	{
		const HTMLayoutProperties& props = *cascade->GetProps();
		SpaceManager* space_manager = state->cascade->space_manager;

		if (space_manager->HasPendingFloats())
			space_manager->EnableFullBfcReflow();

		if (pos_changed)
		{
			AbsoluteBoundingBox before;
			AbsoluteBoundingBox bbox;

			before.Set(state->old_bounding_box, state->old_width, state->old_height);
			before.Translate(state->old_x, state->old_y);

			GetBoundingBox(bbox);
			bbox.Translate(x, y);

			bbox.UnionWith(before);

			info.visual_device->UpdateRelative(bbox.GetX(), bbox.GetY(), bbox.GetWidth(), bbox.GetHeight());
		}
		else
			if (HasBackground(props) ||
				props.border.left.width ||
				props.border.top.width ||
				props.border.right.width ||
				props.border.bottom.width)
			{
				// If we have a background or border, we need to update all

				LayoutCoord left = MIN(state->old_x, x);
				LayoutCoord right = MAX(state->old_x + state->old_width, x + content_width);
				LayoutCoord top = MIN(state->old_y, y);
				LayoutCoord bottom = MAX(state->old_y + state->old_height, y + height);

				info.visual_device->UpdateRelative(left, top, right - left, bottom - top);
			}
			else
			{
				if (state->old_height > height)
					// Box shrunk in height; update area below box

					info.visual_device->UpdateRelative(x,
													   y + height,
													   content_width,
													   state->old_height - height);

				if (state->old_width > content_width)
					// Box shrunk in width; update area to the left of box

					info.visual_device->UpdateRelative(state->old_width,
													   y,
													   state->old_width - content_width,
													   height);
			}

		state->old_bounding_box = bounding_box;
		state->old_x = x;
		state->old_y = y;
		state->old_width = content_width;
		state->old_height = height;
	}
	else if (state->needs_update)
		bounding_box.Invalidate(info.visual_device, GetPositionedX(), GetPositionedY(), content_width, content->GetHeight());
}

/** Increase y position (or decrease, if using negative values). */

/* virtual */ void
TableCaptionBox::IncreaseY(LayoutInfo& info, TableContent* table, LayoutCoord increase)
{
	y_pos += increase;

	if (GetReflowState())
		// This box is currently being laid out, so we need to translate.

		info.Translate(LayoutCoord(0), increase);
}

/** Invalidate the screen area that the box uses. */

/* virtual */ void
TableCaptionBox::Invalidate(LayoutProperties* parent_cascade, LayoutInfo& info) const
{
	HTML_Element* element = GetHtmlElement();
	if (element->IsExtraDirty() || !element->IsDirty() || !element->NeedsUpdate())
		/* Element is dirty and also its area needs to be repainted. However,
		   at this stage element's position may be wrong due to future margins
		   that may alter position of any of box's ancestors. Therefore, while
		   box's coordinates (relative to its parent) remain unchanged by margins,
		   its screen position changes after margin propagation. Invalidating now
		   may cause repaint issues therefore we'll defer invalidation untill
		   final box screen position is known. */

		bounding_box.Invalidate(info.visual_device, GetPositionedX(), GetPositionedY(), content_width, content->GetHeight());
}

/** Finish reflowing box. */

/* virtual */ LAYST
TableCaptionBox::FinishLayout(LayoutInfo& info)
{
	VerticalBoxReflowState* state = GetReflowState();

	switch (content->FinishLayout(info))
	{
	case LAYOUT_END_FIRST_LINE:
		return LAYOUT_END_FIRST_LINE;

	case LAYOUT_OUT_OF_MEMORY:
		return LAYOUT_OUT_OF_MEMORY;
	}

	space_manager.FinishLayout();

	if (StackingContext* local_stacking_context = GetLocalStackingContext())
		local_stacking_context->FinishLayout(state->cascade);

	UpdateScreen(info);

	if (IsPositionedBox())
		info.SetRootTranslation(state->previous_root_x, state->previous_root_y);

	state->cascade->table->AddCaptionMinHeight(content->GetMinHeight());

	DeleteReflowState();

	return LAYOUT_CONTINUE;
}

/** Recalculate top margins after new block box has been added. See declaration in header file for more information. */

/* virtual */ BOOL
TableCaptionBox::RecalculateTopMargins(LayoutInfo& info, const VerticalMargin* top_margin, BOOL /* has_bottom_margin */)
{
	VerticalMargin new_top_margin;

	if (top_margin)
	{
		new_top_margin = *top_margin;
		new_top_margin.max_default = LayoutCoord(0);
		new_top_margin.max_default_nonpercent = LayoutCoord(0);
	}

	return GetCascade()->table->RecalculateTopMargins(info, &new_top_margin);
}

/** Set child's bottom margins in this block and propagate changes to parents. See declaration in header file for more information. */

/* virtual */ void
TableCaptionBox::PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin, BOOL has_inflow_content)
{
	LayoutProperties* cascade = GetCascade();
	TableContent* table = cascade->table;

	if (table)
	{
		const HTMLayoutProperties& props = *cascade->GetProps();
		VerticalMargin new_bottom_margin;

		if (bottom_margin)
		{
			new_bottom_margin = *bottom_margin;
			new_bottom_margin.max_default = LayoutCoord(0);
			new_bottom_margin.max_default_nonpercent = LayoutCoord(0);
		}

		if (props.margin_bottom)
			new_bottom_margin.CollapseWithBottomMargin(props, TRUE);

		table->PropagateBottomMargins(info, &new_bottom_margin, has_inflow_content);
	}
}

/** Propagate bounding box for floating and absolute positioned boxes. See declaration in header file for more information. */

/* virtual */ void
TableCaptionBox::PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	// A table caption has its own space manager, so you never propagate a float past it:

	OP_ASSERT(opts.type != PROPAGATE_FLOAT);

	/** Update bounding boxes - if we're between an abspos element and its containing block
		no clipping can be done (bounding needs boxes to be updated). After we have passed containing element
		we may safely clip any overflow (by skipping bbox update for example) */

	BOOL is_positioned_box = IsPositionedBox();

	if (opts.type == PROPAGATE_ABSPOS_BOTTOM_ALIGNED)
	{
		StackingContext *context = GetLocalStackingContext();

		if (context)
			context->SetNeedsBottomAlignedUpdate();

		if (is_positioned_box)
		{
			if (!context)
			{
				ReflowState* state = GetReflowState();
				state->cascade->stacking_context->SetNeedsBottomAlignedUpdate();
			}

			return;
		}
	}
	else if (opts.type != PROPAGATE_ABSPOS_SKIPBRANCH || IsOverflowVisible())
		UpdateBoundingBox(child_bounding_box, !is_positioned_box);

	if (is_positioned_box || opts.type == PROPAGATE_ABSPOS_SKIPBRANCH)
	{
		/** We either reached a containing element or we already passed it */

		if (GetReflowState())
			/** If we're not in SkipBranch or we reached element that is still being reflowed
				we can safely stop bbox propagation here. Bounding box will be propagated
				when layout of this box is finished */

			return;

		/** We've already found a containing element but since it had no reflow state we must be
			skipping a branch. Switch propagation type (this will enable us to clip such bbox)
			and continue propagation until we reach first element that is being reflowed */
		opts.type = PROPAGATE_ABSPOS_SKIPBRANCH;
	}

	if (TableContent* table = GetTable())
	{
		AbsoluteBoundingBox abs_bounding_box;

		if (opts.type == PROPAGATE_ABSPOS_SKIPBRANCH)
			GetBoundingBox(abs_bounding_box, IsOverflowVisible());
		else
			abs_bounding_box = child_bounding_box;

		abs_bounding_box.Translate(GetPositionedX(), GetPositionedY());

		table->PropagateBottom(info, abs_bounding_box, opts);
	}
}

/** Do we need to calculate min/max widths of this box's content? */

/* virtual */ BOOL
TableCaptionBox::NeedMinMaxWidthCalculation(LayoutProperties* cascade) const
{
	return cascade->table->IsCalculatingMinMaxWidths();
}

/** Propagate widths to container / table. */

/* virtual */ void
TableCaptionBox::PropagateWidths(const LayoutInfo& info, LayoutCoord min_width, LayoutCoord normal_min_width, LayoutCoord max_width)
{
	TableContent* table = GetCascade()->table;

	if (table)
		table->PropagateCaptionWidths(info, min_width, max_width);
}

#ifdef PAGED_MEDIA_SUPPORT

/** Attempt to break page. */

/* virtual */ BREAKST
TableCaptionBox::AttemptPageBreak(LayoutInfo& info, int strength)
{
	if (GetPositionedY() + GetHeight() <= info.doc->GetRelativePageBottom())
	{
		BREAKST status;

		info.Translate(LayoutCoord(0), GetPositionedY());
		status = content->AttemptPageBreak(info, strength, HARD);
		info.Translate(LayoutCoord(0), -GetPositionedY());

		if (status != BREAK_KEEP_LOOKING)
			return status;
	}

	return BREAK_KEEP_LOOKING;
}

/** Insert a page break. */

/* virtual */ BREAKST
TableCaptionBox::InsertPageBreak(LayoutInfo& info, int strength)
{
	TableContent* table = GetCascade()->table;

	if (table)
	{
		BREAKST status;

		info.Translate(LayoutCoord(0), -GetPositionedY());
		status = table->InsertPageBreak(info, strength);
		info.Translate(LayoutCoord(0), GetPositionedY());

		return status;
	}

	return BREAK_NOT_FOUND;
}

#endif // PAGED_MEDIA_SUPPORT

/** Is this box (and potentially also its children) columnizable? */

/* virtual */ BOOL
TableCaptionBox::IsColumnizable(BOOL require_children_columnizable) const
{
	return packed.in_multipane == 1;
}

/** Reflow box and children. */

/* virtual */ LAYST
TableCaptionBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	HTML_Element* html_element = cascade->html_element;

	if (first_child && first_child != html_element)
	{
		return content->Layout(cascade, info, first_child, start_position);
	}
	else
	{
		VerticalBoxReflowState* state = InitialiseReflowState();

		state->needs_update = html_element->NeedsUpdate();

		int reflow_content = html_element->MarkClean();
		const HTMLayoutProperties& props = *cascade->GetProps();
		TableContent* table = cascade->table;

		if (!state || Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
			return LAYOUT_OUT_OF_MEMORY;

		state->cascade = cascade;

		if (table)
		{
			LayoutCoord border_padding = props.padding_left + props.padding_right +
				LayoutCoord(props.border.left.width + props.border.right.width);

			state->old_bounding_box = bounding_box;
			state->old_x = GetPositionedX();
			state->old_y = GetPositionedY();
			state->old_width = content_width;
			state->old_height = content->GetHeight();

			if (cascade->multipane_container)
				/* There's an ancestor multi-pane container, but that doesn't
				   have to mean that this caption is to be columnized. */

				packed.in_multipane = table->GetPlaceholder()->IsColumnizable(TRUE);

			SetStaticPositionedY(LayoutCoord(0));
			LayoutCoord new_width = table->GetWidth();

			if (props.content_width >= 0)
				new_width = props.content_width;
			else
				if (props.content_width == CONTENT_WIDTH_AUTO)
					new_width -= border_padding + (props.margin_left + props.margin_right);
				else
					new_width = props.ResolvePercentageWidth(new_width);

			// Width must be within the limits of min-width and max-width

			new_width = props.CheckWidthBounds(new_width);

			// ... and it should never be negative

			if (new_width < 0)
				content_width = LayoutCoord(0);
			else
				content_width = new_width;

			if (props.content_width == CONTENT_WIDTH_AUTO || props.box_sizing == CSS_VALUE_content_box)
				content_width += border_padding;

			table->GetNewCaption(info, this, props.caption_side);

			SetStaticPositionedX(props.margin_left);
			OP_ASSERT(!GetStaticPositionedY());

			packed.is_top_caption = props.caption_side == CSS_VALUE_top;

			if (
#ifdef PAGED_MEDIA_SUPPORT
				info.paged_media != PAGEBREAK_OFF ||
#endif // PAGED_MEDIA_SUPPORT
				cascade->multipane_container)
			{
				BREAK_POLICY page_break_before;
				BREAK_POLICY column_break_before;
				BREAK_POLICY page_break_after;
				BREAK_POLICY column_break_after;

				CssValuesToBreakPolicies(info, cascade, page_break_before, page_break_after, column_break_before, column_break_after);

				packed.page_break_before = page_break_before;
				packed.column_break_before = column_break_before;
				packed.page_break_after = page_break_after;
				packed.column_break_after = column_break_after;

				/* The column break bitfields cannot hold 'left' and 'right' values
				   (but they are meaningless in a column context anyway): */

				OP_ASSERT(column_break_before != BREAK_LEFT && column_break_before != BREAK_RIGHT);
				OP_ASSERT(column_break_after != BREAK_LEFT && column_break_after != BREAK_RIGHT);
			}

			if (IsPositionedBox())
				info.GetRootTranslation(state->previous_root_x, state->previous_root_y);

			info.Translate(GetPositionedX(), GetPositionedY());

			if (!reflow_content)
				if (content_width != state->old_width ||
#ifdef PAGED_MEDIA_SUPPORT
					info.paged_media == PAGEBREAK_ON ||
#endif
					cascade->multipane_container ||
					info.stop_before && html_element->IsAncestorOf(info.stop_before) ||
					table->IsCalculatingMinMaxWidths() && !content->HasValidMinMaxWidths())
					reflow_content = ELM_DIRTY;

			if (reflow_content)
			{
				if (!space_manager.Restart())
					return LAYOUT_OUT_OF_MEMORY;

				RestartStackingContext();

				bounding_box.Reset(props.DecorationExtent(info.doc), LayoutCoord(0));

				// Recalculate top margins

				VerticalMargin top_margin;

				if (props.margin_top)
					top_margin.CollapseWithTopMargin(props);

				RecalculateTopMargins(info, &top_margin);

				if (IsPositionedBox())
					// Caption position has just been calculated. Translate root.

					info.SetRootTranslation(info.GetTranslationX(), info.GetTranslationY());

				return content->Layout(cascade, info);
			}
			else
			{
				OP_BOOLEAN status;

				VerticalMargin top_margin;
				VerticalMargin bottom_margin;

				if (cascade->Suc())
					if (OpStatus::IsMemoryError(cascade->Suc()->Finish(&info)))
						return LAYOUT_OUT_OF_MEMORY;

				status = content->CalculateTopMargins(cascade, info, &top_margin, TRUE);
				RecalculateTopMargins(info, &top_margin);

				if (!cascade->SkipBranch(info, FALSE, TRUE))
					return LAYOUT_OUT_OF_MEMORY;

				if (status == OpBoolean::IS_TRUE)
				{
					status = content->CalculateBottomMargins(cascade, info, &bottom_margin);
					PropagateBottomMargins(info, &bottom_margin);
				}
				else
					if (props.margin_bottom)
					{
						bottom_margin.CollapseWithBottomMargin(props);
						RecalculateTopMargins(info, &bottom_margin, TRUE);
					}

				if (IsPositionedBox())
					// Caption position has just been calculated. Translate root.

					info.SetRootTranslation(info.GetTranslationX(), info.GetTranslationY());

				if (status == OpStatus::ERR_NO_MEMORY)
					return LAYOUT_OUT_OF_MEMORY;
			}
		}
	}

	return LAYOUT_CONTINUE;
}

/** Get cell height. */

/* virtual */ LayoutCoord
TableCellBox::GetHeight() const
{
	OP_PROBE3(OP_PROBE_TABLECELLBOX_GETHEIGHT);
	TableRowBox* row = GetTableRowBox();

	if (row)
	{
		HTML_Element* html_element = GetHtmlElement();
		int rowspan = row->IsTableWrapped() ? 1 : GetCellRowSpan();

		if (rowspan > 1)
		{
			TableRowGroupBox* row_group = GetTableRowGroup(html_element);

			if (row_group)
				return row_group->GetRowSpannedHeight(row, rowspan);
		}
		else
		{
			// AMSR
			if (row->IsWrapped())
			{
				// This is rewritten to only check the part which affects the cell since
				// SSR causes alot of wrapping. On BREW this reduced this function from
				// ~5-6 to ~1-2 % while loading www.dn.se in the emulator //timj 2004-09-02
				TableCellBox *cell;
				LayoutCoord max_h = GetCellContentHeight(); // this cell's content height
#ifdef SUPPORT_TEXT_DIRECTION
				BOOL is_rtl = GetTable()->IsRTL();
#endif // SUPPORT_TEXT_DIRECTION
				LayoutCoord prev_x = GetStaticPositionedX();
				LayoutCoord prev_width = GetWidth();

				for (cell = (TableCellBox*) Suc(); cell; cell = (TableCellBox*) cell->Suc())
				{
					LayoutCoord cell_x = cell->GetStaticPositionedX();
					LayoutCoord cell_width = cell->GetWidth();

#ifdef SUPPORT_TEXT_DIRECTION
					if (is_rtl)
					{
						if (prev_x + prev_width < cell_x + cell_width || prev_x + prev_width == cell_x + cell_width && cell_width > 0)
							break;
					}
					else
#endif // SUPPORT_TEXT_DIRECTION
						if (prev_x > cell_x || prev_x == cell_x && cell_width > 0)
							break;

					if (max_h < cell->GetCellContentHeight())
						max_h = cell->GetCellContentHeight();

					prev_x = cell_x;
					prev_width = cell_width;
				}

				prev_x = GetStaticPositionedX();
				prev_width = GetWidth();

				for (cell = (TableCellBox*) Pred(); cell; cell = (TableCellBox*) cell->Pred())
				{
					LayoutCoord cell_x = cell->GetStaticPositionedX();
					LayoutCoord cell_width = cell->GetWidth();

#ifdef SUPPORT_TEXT_DIRECTION
					if (is_rtl)
					{
						if (prev_x + prev_width > cell_x + cell_width || prev_x + prev_width == cell_x + cell_width && cell_width > 0)
							break;
					}
					else
#endif // SUPPORT_TEXT_DIRECTION
						if (prev_x < cell_x || prev_x == cell_x && cell_width > 0)
							break;

					if (max_h < cell->GetCellContentHeight())
						max_h = cell->GetCellContentHeight();

					prev_x = cell_x;
					prev_width = cell_width;
				}
				return max_h;
			}

			return row->GetHeight();
		}
	}

	return LayoutCoord(0);
}

/** Get cell height. */

LayoutCoord
TableCellBox::GetCellHeight() const
{
	LayoutCoord cell_height = content->GetHeight();
	TableCellBoxReflowState* cell_state = GetReflowState();

	if (cell_state)
	{
		LayoutCoord desired_height = cell_state->desired_height;

		return MAX(cell_height, desired_height);
	}
	else
		return cell_height;
}

/** Get cell colspan. */

unsigned short
TableCellBox::GetCellColSpan() const
{
	HTML_Element* html_element = GetHtmlElement();

	if (html_element->Type() == Markup::HTE_TH || html_element->Type() == Markup::HTE_TD)
	{
		unsigned short colspan = html_element->GetNumAttr(Markup::HA_COLSPAN, NS_IDX_HTML, 1);
		if (colspan)
			return colspan;
	}

	return 1;
}

/** Get cell rowspan. */

unsigned short
TableCellBox::GetCellRowSpan() const
{
	HTML_Element* html_element = GetHtmlElement();

	if (html_element->Type() == Markup::HTE_TH || html_element->Type() == Markup::HTE_TD)
	{
		unsigned short rowspan = html_element->GetNumAttr(Markup::HA_ROWSPAN, NS_IDX_HTML, 1);
		if (rowspan)
			return rowspan;
		else
			return SHRT_MAX;
	}

	return 1;
}

/** Is this box (and potentially also its children) columnizable? */

/* virtual */ BOOL
TableCellBox::IsColumnizable(BOOL require_children_columnizable) const
{
	return FALSE;
}

/** Allocate word_info_array, and update word_count. Precondition: empty array. */

BOOL
Text_Box::AllocateWordInfoArray(unsigned int new_word_count)
{
	OP_ASSERT(!word_info_array && !packed2.word_count);

	word_info_array = OP_NEWA(WordInfo, new_word_count);

	if (!word_info_array && new_word_count)
		return FALSE;

	packed2.word_count = new_word_count;

	REPORT_MEMMAN_INC(packed2.word_count * sizeof(WordInfo));

	return TRUE;
}

/** Delete word_info_array, and update word_count. */

void
Text_Box::DeleteWordInfoArray()
{
	OP_DELETEA(word_info_array);
	word_info_array = NULL;

	if (packed2.word_count)
	{
		REPORT_MEMMAN_DEC(packed2.word_count * sizeof(WordInfo));

		packed2.word_count = 0;
	}
}

/** Set text selection point to start of this text */

void
Text_Box::SetVisualStartOfSelection(SelectionBoundaryPoint* selection
#ifdef SUPPORT_TEXT_DIRECTION
							  , BOOL is_rtl_elm
#endif
							  )
{
	const uni_char* text_content = (uni_char*) html_element->TextContent();

	if (text_content && word_info_array && packed2.word_count)
	{
		int firstUseful = 0;

		while (word_info_array[firstUseful].GetLength() == 0 && firstUseful < packed2.word_count - 1)
			firstUseful++;

		const uni_char* word_start = text_content + word_info_array[firstUseful].GetStart();

#ifdef SUPPORT_TEXT_DIRECTION

		BidiCategory bidi = BIDI_BN;

		if (word_info_array[firstUseful].GetLength())
		{
			const UnicodePoint uc = Unicode::GetUnicodePoint(word_start, word_info_array[firstUseful].GetLength());
			bidi = Unicode::GetBidiCategory(uc);
		}

		if (bidi == BIDI_R || bidi == BIDI_AL ||
			(is_rtl_elm &&
			 (bidi == BIDI_ON || bidi == BIDI_BN || bidi == BIDI_WS || bidi == BIDI_B || bidi == BIDI_S || bidi == BIDI_UNDEFINED)))
		{
			LayoutCoord word_width = word_info_array[firstUseful].GetWidth();

			if (word_info_array[firstUseful].HasTrailingWhitespace())
				word_width += GetWordSpacing(FALSE);

			selection->SetLogicalPosition(html_element, word_start - text_content);
		}
		else
#endif
			selection->SetLogicalPosition(html_element, word_start - text_content);
	}
	else
		selection->SetLogicalPosition(html_element, 0);
}

/** Set text selection point to end of this text */

void
Text_Box::SetVisualEndOfSelection(SelectionBoundaryPoint* selection
#ifdef SUPPORT_TEXT_DIRECTION
							 , BOOL is_rtl_elm
#endif
)
{
	const uni_char* text_content = (uni_char*) html_element->TextContent();

	if (text_content && word_info_array && packed2.word_count)
	{
		const WordInfo& word_info = word_info_array[packed2.word_count - 1];

#ifdef SUPPORT_TEXT_DIRECTION
		BidiCategory bidi = Unicode::GetBidiCategory(*(text_content + word_info.GetStart()));

		if (bidi == BIDI_R || bidi == BIDI_AL || (is_rtl_elm && bidi == BIDI_ON))
			selection->SetLogicalPosition(html_element, word_info.GetStart() + word_info.GetLength());
		else
#endif
			selection->SetLogicalPosition(html_element, word_info.GetStart() + word_info.GetLength());
	}
	else
		selection->SetLogicalPosition(html_element, 0);
}

/** Helper function to get format options for text. */

int
GetTextFormat(const HTMLayoutProperties& props, const WordInfo& word_info)
{
	int text_format = 0;

	switch (props.text_transform)
	{
	case TEXT_TRANSFORM_CAPITALIZE:
		text_format |= TEXT_FORMAT_CAPITALIZE; break;
	case TEXT_TRANSFORM_UPPERCASE:
		text_format |= TEXT_FORMAT_UPPERCASE; break;
	case TEXT_TRANSFORM_LOWERCASE:
		text_format |= TEXT_FORMAT_LOWERCASE; break;
	}

	if (props.small_caps == CSS_VALUE_small_caps)
		text_format |= TEXT_FORMAT_SMALL_CAPS;

	if (word_info.IsStartOfWord())
		text_format |= TEXT_FORMAT_IS_START_OF_WORD;

	return text_format;
}

unsigned int GetFirstLetterLength(const uni_char* text, const int length)
{
	BOOL has_text = FALSE;

	UnicodePoint uc;
	UnicodeStringIterator iter(text, 0, length);

	// Consume spaces and leading punctuations
	while (!iter.IsAtEnd() && uni_isspace(iter.At()))
		iter.Next();

	while (!iter.IsAtEnd() && Unicode::IsCSSPunctuation(iter.At()))
	{
		has_text = TRUE;
		iter.Next();
	}

	switch (Unicode::GetCharacterClass(iter.At()))
	{
	case CC_Pc:
	case CC_Pd:
		// Starts with non-ignorable punctuation: there is no
		// first-letter (CORE-23426)
		return 0;
	}

	// Include first letter if present
	if (!iter.IsAtEnd() && !uni_isspace(iter.At()))
	{
		uc = iter.At();
		has_text = TRUE;

		// first letter might be a ligature or grapheme rather than a
		// single unicode codepoint
#if defined(FONTSWITCHING) && !defined(PLATFORM_FONTSWITCHING)
# if defined USE_TEXTSHAPER_INTERNALLY
		if (TextShaper::NeedsTransformation(uc))
		{
			TextShaper::ResetState();
			int consumed;
			TextShaper::GetShapedChar(text + iter.Index(), length - iter.Index(), consumed);
			// rather convoluted - at the time of writing text shaper
			// only deals with BMP text, but this may change
			consumed += iter.Index();
			if (consumed > 0)
				while (iter.Index() < (size_t)consumed)
					iter.Next();
			OP_ASSERT(consumed >= 0 && iter.Index() == (size_t)consumed);
		}
		else
# endif // USE_TEXTSHAPER_INTERNALLY
#endif // FONTSWITCHING && !PLATFORM_FONTSWITCHING
		{
			iter.Next();
			UnicodePoint base_character = uc;
			CharacterClass base_character_class = Unicode::GetCharacterClass(base_character);

			while (!iter.IsAtEnd())
			{
				uc = iter.At();

				if (UnicodeSegmenter::IsGraphemeClusterBoundary(base_character,
							                                    base_character_class,
																uc,
																Unicode::GetCharacterClass(uc)))
					break;

				iter.Next();
			}
		}
	}

	// Has a letter or just punctuation
	if (has_text)
	{
		// Consume trailing punctuations
		while (!iter.IsAtEnd() && Unicode::IsCSSPunctuation(iter.At()))
			iter.Next();

		return iter.Index();
	}

	return 0;
}

/** Count number of words in segment. */

/* virtual */ BOOL
Text_Box::CountWords(LineSegment& segment, int& words) const
{
	if (word_info_array)
	{
		LayoutCoord pos = x - segment.start;

		for (int i = 0; i < packed2.word_count; ++i)
		{
			const WordInfo& word_info = word_info_array[i];
			LayoutCoord word_width = word_info.GetWidth();

			if (segment.length < pos + word_width)
				return FALSE;

			if (word_info.IsCollapsed())
				// this word was collapsed; probably whitespace

				continue;

			if (segment.white_space != CSS_VALUE_pre_wrap && word_info.HasTrailingWhitespace())
				// Add trailing space

				word_width += GetWordSpacing(word_info.IsFirstLineWidth());

			LayoutCoord new_pos = pos + word_width;

			if (pos >= 0 && word_info.HasTrailingWhitespace())
				if (new_pos <= segment.length)
					words++;

			pos = new_pos;
		}
	}

	return TRUE;
}

#ifdef DOCUMENT_EDIT_SUPPORT

/**
 * This is a hack to figure out if the empty text html_element is one
 * inserted by document_edit to put the caret inside. Did I mention hack?
 */
static BOOL IsLoneEmptyTextElement(HTML_Element* html_element)
{
	OP_ASSERT(html_element);
	HTML_Element* non_inline_container = html_element;
	while (non_inline_container)
	{
		if (non_inline_container->IsText() ||
			non_inline_container->GetNsType() == NS_HTML &&
			(non_inline_container->Type() == Markup::HTE_B ||
			 non_inline_container->Type() == Markup::HTE_STRONG ||
			 non_inline_container->Type() == Markup::HTE_I ||
			 non_inline_container->Type() == Markup::HTE_EM ||
			 non_inline_container->Type() == Markup::HTE_SPAN ||
			 non_inline_container->Type() == Markup::HTE_U))
		{
			non_inline_container = non_inline_container->Parent();
		}
		else
		{
			break;
		}
	}

	OP_ASSERT(non_inline_container);

	if (!non_inline_container)
		return FALSE;

	HTML_Element* stop = non_inline_container->NextSibling();
	HTML_Element* it = non_inline_container->Next();

	while (it != stop)
	{
		if (it != html_element &&
			it->Type() == Markup::HTE_TEXT)
		{
			return FALSE;
		}
		it = it->Next();
	}

	return TRUE;
}

#endif // DOCUMENT_EDIT_SUPPORT

/** Reflow box and children. */

/* virtual */ LAYST
Text_Box::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	const uni_char* text_content = (uni_char*) html_element->TextContent();
	int text_content_length = html_element->GetTextContentLength();
	LayoutCoord grow_inline_width;

	OP_ASSERT(!first_child || first_child == html_element);

	// Reset current formatting

	if (start_position == LAYOUT_COORD_MIN)
	{
		width = LayoutCoord(0);
		start_position = LayoutCoord(0);
	}
	else
		width -= x - start_position;

	grow_inline_width = width;

	// Remove dirty mark

	html_element->MarkClean();

	if (text_content)
	{
		const HTMLayoutProperties& props = *cascade->GetProps();
		Container* container = cascade->container;

		TextSelection* ts = info.doc->GetTextSelection();

		if (ts && (html_element == ts->GetStartElement() || html_element == ts->GetEndElement()))
			ts->MarkDirty(NULL);

		if (!container->GetCollapseWhitespace() ||
			TextRequiresLayout(info.doc, text_content, (CSSValue) props.white_space))
		{
			BOOL first_line = container->IsCssFirstLine();
			HTML_Element* parent = cascade->FindParent();

			unsigned short new_word_count = 0;

			if (!word_info_array || packed.remove_word_info)
			{
#ifdef CONTENT_MAGIC
				int character_count = 0;
#endif // CONTENT_MAGIC
				WordInfo* tmp_word_info_array = g_opera->layout_module.tmp_word_info_array;

				if (text_content && text_content_length > WORD_INFO_MAX_SIZE)
				{
					/* Probably allocates way too much, but it's only temporary.
					   May be an issue in documents with lots of text; may
					   cause new/delete performance issues. */

					tmp_word_info_array = OP_NEWA(WordInfo, text_content_length);

					if (!tmp_word_info_array)
						return LAYOUT_OUT_OF_MEMORY;
				}

				// Prepare the word info array

				packed.remove_word_info = FALSE;

				// Split content into non-breakable fragments

				BOOL stop_at_whitespace_separated_words = props.white_space == CSS_VALUE_nowrap ||
														  props.white_space == CSS_VALUE_pre ||
														  props.small_caps == CSS_VALUE_small_caps ||
														  props.word_spacing_i != LayoutFixed(0);

				unsigned int first_letter_len = GetFirstLetterLength(text_content, text_content_length);
				BOOL first_letter_done = !html_element->Pred() || !html_element->Pred()->GetIsFirstLetterPseudoElement() || first_letter_len == 0;
				const uni_char* tmp = text_content;
				BOOL leading_whitespace = FALSE;
				UnicodePoint last_base_character = 0;
				CharacterClass last_base_character_class = CC_Unknown;

				for (;;)
				{
					FontSupportInfo font_info(props.font_number);
					const uni_char* word = tmp;

					WordInfo tmp_word_info;
					tmp_word_info.Reset();
					tmp_word_info.SetStart(short(word - text_content));
					tmp_word_info.SetFontNumber(font_info.current_font ? font_info.current_font->GetFontNumber() : props.font_number);

					if (!GetNextTextFragment(tmp, text_content_length - tmp_word_info.GetStart(),
											 last_base_character, last_base_character_class, leading_whitespace,
											 tmp_word_info, CSSValue(props.white_space), stop_at_whitespace_separated_words, info.treat_nbsp_as_space, font_info, info.doc, props.current_script))
						break;

					WordInfo& word_info = tmp_word_info_array[new_word_count];
					word_info = tmp_word_info;

					leading_whitespace = word_info.HasTrailingWhitespace();

					if (first_letter_done)
					{
						int dummy;
						const UnicodePoint puc = GetPreviousUnicodePoint(text_content, word - text_content, dummy);

						if ((new_word_count && word > text_content && uni_isspace(puc)) ||
							(!new_word_count && container->GetCollapseWhitespace()))
							word_info.SetIsStartOfWord(TRUE);

#ifdef CONTENT_MAGIC
						character_count += word_info.GetLength();
#endif // CONTENT_MAGIC

						word_info.SetWidth(0);
						new_word_count++;
					}
					else if (static_cast<unsigned int>(word - text_content) + word_info.GetLength() >= first_letter_len)
					{
						word_info.SetLength((word - text_content) + word_info.GetLength() - first_letter_len);
						word_info.SetHasEndingNewline(FALSE);
						word_info.SetHasTrailingWhitespace(FALSE);
						tmp = text_content + first_letter_len;
						first_letter_done = TRUE;
					}
				}

#ifdef DOCUMENT_EDIT_SUPPORT
				if (OpDocumentEdit* document_edit = info.doc->GetDocumentEdit())
				{
					if (!new_word_count)
					{
						/* Here be dragons. But we can avoid them if layout-inserted, or if
						   this element actually isn't in an editable part of the document. */

						if (html_element->GetInserted() != HE_INSERTED_BY_LAYOUT &&
							document_edit->GetEditableContainer(html_element))
						{
							/* Fake a word to not collapse empty textelements in editable mode.
							   (We must be able to edit empty textelements, so they must be traversed). */

							WordInfo& word_info = tmp_word_info_array[new_word_count];

							word_info.Reset();
							word_info.SetFontNumber(info.visual_device->GetCurrentFontNumber());
							word_info.SetIsStartOfWord(TRUE);
							// DocumentEdit inserts empty text blocks to position the caret and give
							// elements a height (so that the caret is visible). We convert that signal
							// to a "has_ending_newline" here to get the desired rendering. Yes it's a hack.
							// See bug 336017 and 316949 with duplicates
							BOOL force_height_to_text = IsLoneEmptyTextElement(html_element);
							word_info.SetHasEndingNewline(force_height_to_text); // This is a hack to force height to empty block elements
							new_word_count++;
						}
					}
# ifdef INTERNAL_SPELLCHECK_SUPPORT
					else
					{
						if (packed2.word_count)
						{
							// copy the old is_misspelling flags to prevent "flickering"

							if (info.doc->GetDocumentEdit()->GetSpellCheckerSession())
								for (int i = MIN(packed2.word_count, new_word_count)-1; i >= 0; i--)
									tmp_word_info_array[i].SetIsMisspelling(word_info_array[i].IsMisspelling());
						}
						else // new_word_count && !word_count
							info.doc->GetDocumentEdit()->OnTextElmGetsLayoutWords(html_element);
					}
# endif // INTERNAL_SPELLCHECK_SUPPORT
				}
#endif // DOCUMENT_EDIT_SUPPORT

				if (!word_info_array || packed2.word_count != new_word_count)
				{
					DeleteWordInfoArray();

					if (!AllocateWordInfoArray(new_word_count))
					{
						if (tmp_word_info_array != g_opera->layout_module.tmp_word_info_array)
							OP_DELETEA(tmp_word_info_array);

						return LAYOUT_OUT_OF_MEMORY;
					}
				}

				// Copy array

				if (word_info_array)
					for (int i = 0; i < new_word_count; ++i)
						word_info_array[i] = tmp_word_info_array[i];

				packed.first_line_word_spacing = packed.word_spacing = 0;
				packed2.underline_position = 0;
				packed2.underline_width = 0;

				if (tmp_word_info_array != g_opera->layout_module.tmp_word_info_array)
					OP_DELETEA(tmp_word_info_array);

#ifdef CONTENT_MAGIC
				if (info.content_magic && character_count && parent->Type() != Markup::HTE_A &&
					html_element->GetInserted() != HE_INSERTED_BY_LAYOUT)
				{
					info.doc->AddContentMagic(html_element, character_count);
				}
#endif // CONTENT_MAGIC
			}

			if (word_info_array)
			{
				InlineVerticalAlignment valign;
				BOOL drop_first_newline = start_position > 0;
				LayoutCoord current_word_spacing = GetWordSpacing(first_line);

				valign.minimum_line_height = LayoutCoord(-1);

				if (!current_word_spacing)
				{
					current_word_spacing = LayoutCoord(LayoutFixedToInt(props.word_spacing_i));

					if (props.white_space != CSS_VALUE_pre_wrap && props.white_space != CSS_VALUE_pre)
#ifdef _GLYPHTESTING_SUPPORT_
						current_word_spacing += LayoutCoord(info.visual_device->MeasureNonCollapsingSpaceWord(UNI_L(" "), 1, 0));
#else  // _GLYPHTESTING_SUPPORT_
						// Can not call MeasureNonCollapsingSpaceWord as it will always use estimate when glyph testing is not available.
						current_word_spacing += LayoutCoord(info.visual_device->GetTxtExtent(UNI_L(" "), 1));
#endif // _GLYPHTESTING_SUPPORT_

					/* (kilsmo) We need to clip current_word_spacing to not be larger than
					   what fits in packed.word_spacing (13 bits) and not smaller
					   than 0 (since packed.word_spacing is unsigned). The effect of this
					   is that we will have words without wordspacing between them. According
					   to the CSS 2 specification, what to do with big negative numbers in word-spacing,
					   is up to the user-agent to decide. */

					current_word_spacing = LayoutCoord(MAX(0, current_word_spacing));
					current_word_spacing = LayoutCoord(MIN(0x1FFF, current_word_spacing));

					if (first_line)
						packed.first_line_word_spacing = current_word_spacing;
					else
						packed.word_spacing = current_word_spacing;
				}

				if (start_position == 0)
					// Element hasn't been laid out yet
					x = container->GetLinePendingPosition();
				else
					start_position -= x;

				Box* parent_box = parent->GetLayoutBox();
				HTML_Element* container_elm = container->GetHtmlElement();
				CSSLengthResolver length_resolver(info.doc->GetVisualDevice(), FALSE, 0, props.decimal_font_size_constrained, props.font_number, info.doc->RootFontSize());
				LayoutCoord text_shadow_width = props.text_shadows.GetMaxDistance(length_resolver);
				BOOL split_word = FALSE;
				int char_count = 0;
				BOOL found_text = FALSE;
				BOOL has_baseline = TRUE;

				int new_word_count = packed2.word_count;

				for (short word_idx = 0; word_idx < new_word_count; ++word_idx)
				{
					WordInfo& word_info = word_info_array[word_idx];
					const uni_char* word = text_content + word_info.GetStart();
					int word_length = word_info.GetLength();
					LayoutCoord word_width;
					BOOL font_changed = FALSE;

					const UnicodePoint uc = word_length ? Unicode::GetUnicodePoint(word, word_length) : 0;

					if (word_info.IsTabCharacter())
					{
						word_width = LayoutCoord(0);
						word_info.SetWidth(0);
					}
					else
					{
						word_width = word_info.GetWidth();

						if (start_position <= 0)
						{
							// Switch font if necessary

							if (word_info.GetFontNumber() != info.visual_device->GetCurrentFontNumber())
							{
								cascade->ChangeFont(info.visual_device, word_info.GetFontNumber());

								LayoutCoord text_baseline = LayoutCoord(props.font_ascent);

								valign.minimum_line_height = props.GetCalculatedLineHeight(info.visual_device);
								valign.minimum_line_height_nonpercent = valign.minimum_line_height;

								LayoutCoord half_leading = HTMLayoutProperties::GetHalfLeading(valign.minimum_line_height, text_baseline, LayoutCoord(props.font_descent));
								LayoutCoord half_leading_nonpercent = HTMLayoutProperties::GetHalfLeading(valign.minimum_line_height_nonpercent, text_baseline, LayoutCoord(props.font_descent));

								valign.logical_above_baseline = text_baseline + half_leading;
								valign.logical_below_baseline = valign.minimum_line_height - valign.logical_above_baseline;
								valign.logical_above_baseline_nonpercent = text_baseline + half_leading_nonpercent;
								valign.logical_below_baseline_nonpercent = valign.minimum_line_height_nonpercent - valign.logical_above_baseline_nonpercent;
								valign.box_above_baseline = text_baseline;
								valign.box_below_baseline = LayoutCoord(props.font_descent);

								font_changed = TRUE;
							}

							if (!word_width || !first_line != !word_info.IsFirstLineWidth())
							{
								if (word_length > 0)
								{
									OP_ASSERT(start_position <= 0 || word_width);

									int text_format = GetTextFormat(props, word_info);

#ifdef SUPPORT_TEXT_DIRECTION
									BidiCategory bidi = Unicode::GetBidiCategory(text_content[word_info.GetStart()]); //FIXME visual encoding should ignore this

									if (bidi == BIDI_R || bidi == BIDI_AL)
										text_format |= TEXT_FORMAT_REVERSE_WORD;
#endif

									word_width = LayoutCoord(info.visual_device->GetTxtExtentEx(word, word_length, text_format));

									if (word_width < 0)
										word_width = LayoutCoord(0);

									if (word_width > WORD_INFO_WIDTH_MAX)
										// Doesn't really matter what we set. The word will be split later.

										word_info.SetWidth(WORD_INFO_WIDTH_MAX);
									else
										word_info.SetWidth(word_width);
								}
							}

							word_info.SetIsFirstLineWidth(first_line);
						}
					}

					if (valign.minimum_line_height < 0)
					{
						if (parent != container_elm)
							parent_box->GetVerticalAlignment(&valign);
						else
						{
							LayoutCoord text_baseline = LayoutCoord(props.font_ascent);

							valign.minimum_line_height = props.GetCalculatedLineHeight(info.visual_device);
							valign.minimum_line_height_nonpercent = valign.minimum_line_height;

							LayoutCoord half_leading = HTMLayoutProperties::GetHalfLeading(valign.minimum_line_height, text_baseline, LayoutCoord(props.font_descent));

							valign.logical_above_baseline = text_baseline + half_leading;
							valign.logical_above_baseline_nonpercent = valign.logical_above_baseline;
							valign.logical_below_baseline = valign.minimum_line_height - valign.logical_above_baseline;
							valign.logical_below_baseline_nonpercent = valign.logical_below_baseline;
							valign.box_above_baseline = text_baseline;
							valign.box_below_baseline = LayoutCoord(props.font_descent);
						}

						font_changed = TRUE;
					}

					if (font_changed || !has_baseline != !word_info.HasBaseline())
					{
						short underline_position = props.font_underline_position;

						has_baseline = word_info.HasBaseline();

						if (!has_baseline)
						{
							underline_position = props.font_descent - props.font_underline_width;

							if (underline_position < 2)
								underline_position++;
						}

						if (packed2.underline_position < underline_position)
							packed2.underline_position = underline_position;
						if (packed2.underline_width < props.font_underline_width)
							packed2.underline_width = props.font_underline_width;
					}

					if (word_width || !word_info.HasTrailingWhitespace())
					{
						if (start_position <= 0)
						{
							LinebreakOpportunity break_property = LB_NO;
							LAYST continue_layout;

							if (info.doc->GetHandheldEnabled() && char_count == 0 && word_width)
								if (uc == 0x00B7 || uc == 0x30FB || (uc >= 0x2460 && uc <= 0x27BF))
									container->ForceLineEnd(info, html_element, FALSE);

							if (uc == 173) // Soft hyphen
								;
							else
							{
								LayoutCoord cjk_extra_descender(0);

								if (!word_info.HasBaseline() && props.font_descent < 2)
									cjk_extra_descender = LayoutCoord(1);

								if (word_length)
									// Construct break property for word

									if (found_text)
										break_property = word_info.CanLinebreakBefore() ? LB_YES : LB_NO;
									else
										switch (props.white_space)
										{
										case CSS_VALUE_nowrap:
										case CSS_VALUE_pre:
											break_property = container->GetLinebreakOpportunity(0xFFFD);
											container->SetLastBaseCharacter(0);

											found_text = TRUE;

											break;

										case CSS_VALUE_pre_wrap:
											if (uni_isspace(uc))
												// This word is all whitespace

												break;

										default:
											{
												UnicodePoint combining_mark = 0;
												CharacterClass character_class = CC_Unknown;

												break_property = container->GetLinebreakOpportunity(uc);

												// A fair amount of code to find the last base character

												for (int i = text_content_length; i >= 0;)
												{
													UnicodePoint next_char;
													CharacterClass next_character_class;

													int consumed;
													if (i > 0)
													{
														next_char = GetPreviousUnicodePoint(text_content, i, consumed);
														next_character_class = Unicode::GetCharacterClass(next_char);
													}
													else
													{
														// beginning of text

														next_char = 0;
														next_character_class = CC_Unknown;
														consumed = 1;
													}

													if (combining_mark)
														if (UnicodeSegmenter::IsGraphemeClusterBoundary(next_char, next_character_class, combining_mark, character_class))
														{
															container->SetLastBaseCharacter(combining_mark);
															break;
														}

													if ((next_character_class != CC_Cf || next_char == 0x200b) &&
														next_character_class != CC_Cc)
													{
														// Don't look at formatting and control characters

														combining_mark = next_char;
														character_class = next_character_class;
													}
													i -= consumed;
												}

												found_text = TRUE;
											}

											break;
										}

								LayoutCoord min_width;

								if (word_width > WORD_INFO_WIDTH_MAX)
								{
									// The bitfield in WordInfo can't handle this width and must be split.
									switch (SplitWord(word_idx, LayoutCoord(WORD_INFO_WIDTH_MAX), info.visual_device, props, TRUE))
									{
									case OpStatus::ERR_NO_MEMORY:
										return LAYOUT_OUT_OF_MEMORY;

									case OpBoolean::IS_TRUE:
										// The second part of the splitted word may also be wider
										// than WORD_INFO_WIDTH_MAX and should be recalculated next round.
										word_info_array[word_idx + 1].SetWidth(0);
										word_idx--;
										new_word_count++;
										continue;

									case OpBoolean::IS_FALSE:
										break;
									}
								}

								if (split_word)
									/* Current word has been split, allow break at splitting point. */

									break_property = LB_YES;

								// Decide how this word contributes to min width

								if (!info.allow_word_split)
									min_width = word_width;
								else
									if (split_word)
										min_width = LayoutCoord(0);
									else
									{
										/* Only add min_width to breakable words, otherwise words like a1b will
										   get three times min_width as aaa. */

										if (container->GetLinePendingPosition() == 0 ||
											break_property == LB_YES)
										{
											min_width = LayoutCoord(props.font_average_width * 2);

											if (min_width > word_width)
												min_width = word_width;
										}
										else
											min_width = LayoutCoord(0);
									}

								if (info.allow_word_split || props.overflow_wrap == CSS_VALUE_break_word && props.white_space != CSS_VALUE_nowrap)
									// Split word when the word doesn't fit and word splitting is allowed

									if (!split_word)
									{
										if (container->GetMaxWordWidth(break_property) < word_width)
											switch (SplitWord(word_idx, container->GetMaxWordWidth(break_property), info.visual_device, props, container->IsStartOfNewLine()))
											{
											case OpStatus::ERR_NO_MEMORY:
												return LAYOUT_OUT_OF_MEMORY;

											case OpBoolean::IS_TRUE:
												split_word = TRUE;
												word_idx--;
												new_word_count++;
												continue;

											case OpBoolean::IS_FALSE:
												// Not enough space for first character - word needs to start on a new line

												if (break_property != LB_YES)
													split_word = TRUE;

												break_property = LB_YES;
												if (container->GetMaxWordWidth(break_property) < word_width)
													// word is too long and needs to be split

													switch (SplitWord(word_idx, container->GetMaxWordWidth(break_property), info.visual_device, props, TRUE))
													{
													case OpStatus::ERR_NO_MEMORY:
														return LAYOUT_OUT_OF_MEMORY;

													case OpBoolean::IS_TRUE:
														word_idx--;
														new_word_count++;
														continue;

													case OpBoolean::IS_FALSE:
														break;
													}

												break;
											}
									}

								width += word_width;

								if (info.doc->GetAggressiveWordBreaking() == WORD_BREAK_AGGRESSIVE && container->GetLastLetterAllowAggressiveBreakAfter())
								{
									container->SetLastLetterAllowAggressiveBreakAfter(FALSE);
									break_property = LB_YES;
								}

								LayoutCoord bbox_overflow_bottom = MAX(text_shadow_width, cjk_extra_descender);

								LayoutCoord line_overflow_bottom = valign.box_below_baseline - valign.logical_below_baseline;
								if (line_overflow_bottom > 0)
								{
									valign.box_below_baseline = valign.logical_below_baseline;
									bbox_overflow_bottom = MAX(bbox_overflow_bottom, line_overflow_bottom);
								}

								continue_layout = container->AllocateLineSpace(html_element, info,
																			   word_width, min_width, word_width,
																			   valign,
																			   text_shadow_width, bbox_overflow_bottom,
																			   text_shadow_width, text_shadow_width,
																			   LayoutCoord(0), cjk_extra_descender,
																			   LayoutCoord(0), LayoutCoord(0),
																			   uc,
																			   break_property,
																			   split_word);

								if (continue_layout != LAYOUT_CONTINUE)
									return continue_layout;

								if (info.allow_word_split)
									container->AllocateWeakContentSpace(html_element, word_width - min_width, info.doc->GetLayoutMode() == LAYOUT_NORMAL);

								if (info.doc->GetAggressiveWordBreaking() == WORD_BREAK_AGGRESSIVE && ERA_AggressiveWordBreakAfter(uc))
									container->SetLastLetterAllowAggressiveBreakAfter(TRUE);
							}
						}

						start_position -= word_width;
					}

					if (word_info.IsTabCharacter())
					{
						LayoutCoord tab_width = container->GetTabSpace();

						word_info.SetWidth(tab_width);

						if (start_position <= 0)
						{
							width += tab_width;

							OP_ASSERT(props.white_space == CSS_VALUE_pre_wrap || props.white_space == CSS_VALUE_pre);

							LAYST continue_layout = container->AllocateLineSpace(html_element, info,
																				 tab_width, tab_width, tab_width,
																				 valign,
																				 text_shadow_width, text_shadow_width,
																				 text_shadow_width, text_shadow_width,
																				 LayoutCoord(0), LayoutCoord(0),
																				 LayoutCoord(0), LayoutCoord(0),
																				 LayoutCoord(0),
																				 props.white_space == CSS_VALUE_pre_wrap ? LB_YES : LB_NO,
																				 FALSE);

							if (continue_layout != LAYOUT_CONTINUE)
								return continue_layout;
						}

						start_position -= tab_width;
					}
					else
						if (word_info.HasTrailingWhitespace())
							if (start_position <= 0)
							{
								// This is whitespace; how do we handle it?

								switch (props.white_space)
								{
								case CSS_VALUE_normal:
								case CSS_VALUE_nowrap:
								case CSS_VALUE_pre_line:
									if (!word_length && container->GetCollapseWhitespace())
										// mark word as collapsed

										word_info.SetCollapsed(TRUE);
									else
									{
										// whitespace isn't collapsed, allocate space for it

										word_info.SetCollapsed(FALSE);

										width += current_word_spacing;

										if (!container->AllocateLineWhitespace(current_word_spacing, html_element))
											return LAYOUT_OUT_OF_MEMORY;

										start_position -= current_word_spacing;
									}

									break;

								case CSS_VALUE_pre_wrap:
									word_info.SetCollapsed(FALSE);

									if (!container->AllocateLineWhitespace(LayoutCoord(0), html_element))
										return LAYOUT_OUT_OF_MEMORY;

									break;

								case CSS_VALUE_pre:
									// This is just to separate words on the line (for capitalise, etc.)

									if (!container->AllocateLineWhitespace(LayoutCoord(0), html_element))
										return LAYOUT_OUT_OF_MEMORY;

									if (current_word_spacing)
									{
										LAYST continue_layout = container->AllocateLineSpace(html_element, info,
																							 current_word_spacing, current_word_spacing, current_word_spacing,
																							 valign,
																							 text_shadow_width, text_shadow_width,
																							 text_shadow_width, text_shadow_width,
																							 LayoutCoord(0), LayoutCoord(0),
																							 LayoutCoord(0),
																							 LayoutCoord(0), LayoutCoord(0),
																							 LB_NO,
																							 FALSE);

										if (continue_layout != LAYOUT_CONTINUE)
											return continue_layout;

										start_position -= current_word_spacing;
									}

									break;
								}
							}
							else
								// If we're looking for start, use old info

								if (!word_info.IsCollapsed())
									if (word_info.IsFirstLineWidth())
										start_position -= LayoutCoord(packed.first_line_word_spacing);
									else
										start_position -= LayoutCoord(packed.word_spacing);

					if (start_position <= 0 && word_info.HasEndingNewline())
						if (drop_first_newline)
							drop_first_newline = FALSE;
						else
						{
							LAYST status = container->ForceLineEnd(info, html_element);

							if (status != LAYOUT_CONTINUE)
								return status;
						}

					split_word = FALSE;

					if (info.doc->GetHandheldEnabled() && !(word_length == 1 && uni_isidsp(uc)))
						char_count += word_length;
				}

				parent_box->GrowInlineWidth(width - grow_inline_width);
			}
		}
	}

	if (packed.remove_word_info)
	{
		DeleteWordInfoArray();
		width = LayoutCoord(0);
	}

	return LAYOUT_CONTINUE;
}

/** Split a word so that the first part is less or equal 'max_width'.
	Return FALSE if first character is wider then 'max_width' */

OP_BOOLEAN
Text_Box::SplitWord(short word_idx, LayoutCoord max_width, VisualDevice* vd, const HTMLayoutProperties& props, BOOL start_of_line)
{
	// NOTE: Text can be fragmented if container is reflowed with different widths (resizing window)
	//       but the intended use is for SSR and MSR mode which normally has a fixed width.
	//       (Not sure about tables in MSR though ...)

	const uni_char* text_content = (uni_char*) html_element->TextContent();
	WordInfo& word_info = word_info_array[word_idx];
	const uni_char* word = text_content + word_info.GetStart();
	int word_length = word_info.GetLength();

	int text_format = GetTextFormat(props, word_info);

#ifdef SUPPORT_TEXT_DIRECTION
	BidiCategory bidi = Unicode::GetBidiCategory(text_content[word_info.GetStart()]); //FIXME visual encoding should ignore this

	if (bidi == BIDI_R || bidi == BIDI_AL)
		text_format |= TEXT_FORMAT_REVERSE_WORD;
#endif

	// Switch font if necessary

	if (word_info.GetFontNumber() != vd->GetCurrentFontNumber())
		vd->SetFont(word_info.GetFontNumber());

	unsigned int result_width, result_char_count;
	vd->GetTxtExtentSplit(word, word_length, max_width, text_format, result_width, result_char_count);
	if (result_char_count > 0)
	{
		int new_word_count = packed2.word_count + 1;
		WordInfo* new_word_info_array = OP_NEWA(WordInfo, new_word_count);

		if (!new_word_info_array)
			return OpStatus::ERR_NO_MEMORY;

		// Copy array

		for (int new_i = 0, old_i = 0; new_i < new_word_count; ++new_i, ++old_i)
		{
			new_word_info_array[new_i] = word_info_array[old_i];
			if (new_i == word_idx)
			{
				int remaining_width = vd->GetTxtExtentEx(word + result_char_count, word_length - result_char_count, text_format);

				if (remaining_width > WORD_INFO_WIDTH_MAX)
					/* No worries. We're probably being called because we exceeded
					   WORD_INFO_WIDTH_MAX in the first place, and the calling code will
					   remeasure and split as needed. This solution could probably
					   benefit from redesign. */

					remaining_width = WORD_INFO_WIDTH_MAX;

				new_word_info_array[new_i].SetLength(result_char_count);
				new_word_info_array[new_i].SetWidth(result_width);
				new_word_info_array[new_i].SetHasEndingNewline(FALSE);
				new_word_info_array[new_i].SetHasTrailingWhitespace(FALSE);
				new_word_info_array[++new_i] = word_info_array[old_i];
				new_word_info_array[new_i].SetLength(word_length - result_char_count);
				new_word_info_array[new_i].SetStart(new_word_info_array[new_i].GetStart() + result_char_count);
				new_word_info_array[new_i].SetWidth(remaining_width);
				new_word_info_array[new_i].SetIsStartOfWord(FALSE);
			}
		}

		REPORT_MEMMAN_INC((new_word_count - packed2.word_count) * sizeof(WordInfo));

		OP_DELETEA(word_info_array);
		word_info_array = new_word_info_array;
		packed2.word_count = new_word_count;

		return OpBoolean::IS_TRUE;
	}

	return OpBoolean::IS_FALSE;
}

/** Get number of (non-whitespace) characters in this box. */

int
Text_Box::GetCharCount() const
{
	int char_count = 0;

	if (word_info_array)
		for (int i = 0; i < packed2.word_count; ++i)
			char_count += word_info_array[i].GetLength();

	return char_count;
}

FontSupportInfo::FontSupportInfo(int font_number)
{
#if defined(FONTSWITCHING)
	desired_font = styleManager->GetFontInfo(font_number);
	current_block = UNKNOWN_BLOCK_NUMBER;
#else
	desired_font = NULL;
	current_block = 0;
#endif
	current_font = desired_font;

	block_lowest = MAX_UNICODE_POINT;
	block_highest = 0x0000;
}

StackingContext::~StackingContext()
{
	while (First())
		First()->Out();

	while (pending_elements.First())
		pending_elements.First()->Out();
}

/** Add element to list. */

void
StackingContext::AddZElement(ZElement* element)
{
	ZElement* list_element;

	element->Remove();

	// The stacking context is sorted by 'z-index', secondarily by 'order'.

	for (list_element = (ZElement*) Last(); list_element; list_element = (ZElement*) list_element->Pred())
		if (list_element->GetZIndex() <= element->GetZIndex())
			if (list_element->GetOrder() <= element->GetOrder() ||
				list_element->GetZIndex() < element->GetZIndex())
				break;

	if (list_element)
		element->Follow(list_element);
	else
		element->IntoStart(this);

	element->SetLogicalPred(last);
	element->SetLogicalSuc(NULL);
	if (last)
		last->SetLogicalSuc(element);
	last = element;
}

/** Restart list. */

void
StackingContext::Restart()
{
	// Move all elements into the pending list, in logical order

	for (ZElement* element = last; element; element = element->LogicalPred())
	{
		element->Out();
		element->ResetSameClippingStack();
		element->IntoStart(&pending_elements);
	}

	has_fixed_element = FALSE;
	last = NULL;
}

/** Skip over an element. */

BOOL
StackingContext::SkipElement(HTML_Element* element, LayoutInfo& info)
{
	// Move pending positioned boxes and other stacking context entries that are children of element back into the stacking context.

	ZElement* pending_element = (ZElement*) pending_elements.First();

	BOOL success = TRUE;

	while (pending_element)
	{
		ZElement* next = (ZElement*) pending_element->Suc();
		HTML_Element* html_element = pending_element->GetHtmlElement();

		if (element->IsAncestorOf(html_element))
		{
			Box* box = html_element->GetLayoutBox();

			AddZElement(pending_element);

			if (!has_fixed_element)
				if (box->IsFixedPositionedBox())
					has_fixed_element = TRUE;
				else
				{
					StackingContext* local_stacking_context = box->GetLocalStackingContext();
					if (local_stacking_context && local_stacking_context->has_fixed_element)
						has_fixed_element = TRUE;
				}

			success = box->SkipZElement(info) && success;
		}
		else
			break;

		pending_element = next;
	}

	return success;
}

/** Translate children of element, because of collapsing margins on container. */

void
StackingContext::TranslateChildren(HTML_Element* element, LayoutCoord offset, LayoutInfo& info)
{
	for (ZElement* z_element = last; z_element; z_element = z_element->LogicalPred())
	{
		HTML_Element* html_element = z_element->GetHtmlElement();
		Box* box = html_element->GetLayoutBox();

		if (element->IsAncestorOf(html_element))
		{
			if (box->IsAbsolutePositionedBox())
			{
				AbsolutePositionedBox* abs_box = (AbsolutePositionedBox*) box;

				if (abs_box->GetVerticalOffset() == NO_VOFFSET)
				{
					/* The top property of 'abs_box' is auto. This means that 'abs_box' assumes a
					   hypothetical-static position in its normal flow context, but without being
					   affected by margin collapsing taking place later in the element tree. The
					   translation operation we're doing here is to compensate for such margin
					   collapsing. (This is similar to what MSIE does, while Firefox and Konqueror
					   let margin collapsing affect the boxes as normally) However, if a containing
					   block in the chain between 'abs_box' and 'element' is absolutely positioned,
					   'abs_pos' shall not be affected by this translation operation, since it is
					   not in the same static flow context. */

					BOOL different_flow_context = FALSE;

					for (HTML_Element* containing_elm = Box::GetContainingElement(html_element, TRUE);
						 element->IsAncestorOf(containing_elm);
						 containing_elm = Box::GetContainingElement(containing_elm, TRUE))
						if (containing_elm->GetLayoutBox()->IsAbsolutePositionedBox())
						{
							different_flow_context = TRUE;
							break;
						}

					if (!different_flow_context)
					{
						abs_box->SetY(abs_box->GetStackPosition() + offset);
						abs_box->PropagateBottomMargins(info);
					}
				}
			}
			else if (!box->IsPositionedBox())
			{
				StackingContext* stacking_context = box->GetLocalStackingContext();
				OP_ASSERT(stacking_context); // Assumption is that if it's not a *Positioned*Box, it's a ZRoot*Box.
				stacking_context->TranslateChildren(element, offset, info);
			}

			continue;
		}

		break;
	}
}

/** Remove element from list. */

void
StackingContext::Remove(ZElement* remove_element)
{
	if (last == remove_element)
		last = remove_element->LogicalPred();

	ZElement *suc = remove_element->LogicalSuc();
	if (suc != NULL)
		suc->SetLogicalPred(remove_element->LogicalPred());

	ZElement *pred = remove_element->LogicalPred();
	if (pred != NULL)
		pred->SetLogicalSuc(remove_element->LogicalSuc());

	remove_element->SetLogicalPred(NULL);
	remove_element->SetLogicalSuc(NULL);
}

/** Find the normal right edge of the rightmost absolute positioned box. */

LayoutCoord
StackingContext::FindNormalRightAbsEdge(HLDocProfile* hld_profile, LayoutProperties* parent_cascade)
{
	LayoutCoord right_edge(0);
	ZElement* list_element = (ZElement*) First();

	if (list_element)
	{
		for (; list_element; list_element = (ZElement*) list_element->Suc())
		{
			Box* box = list_element->GetHtmlElement()->GetLayoutBox();
			LayoutCoord box_right_edge = box->FindNormalRightAbsEdge(hld_profile, parent_cascade);

			if (right_edge < box_right_edge)
				right_edge = box_right_edge;
		}
	}

	return right_edge;
}

/** Are there any absolutely positioned elements in this stacking context? */

BOOL
StackingContext::HasAbsolutePositionedElements()
{
	for (ZElement* elm = (ZElement*)First(); elm; elm = (ZElement*)elm->Suc())
	{
		Box* box = elm->GetHtmlElement()->GetLayoutBox();

		if (box->IsAbsolutePositionedBox())
			return TRUE;

		StackingContext* stacking_context = box->GetLocalStackingContext();
		if (stacking_context && stacking_context->HasAbsolutePositionedElements())
			return TRUE;
	}

	return FALSE;
}

/** Has z children with negative z-index */

BOOL StackingContext::HasNegativeZChildren()
{
	ZElement* z_element = (ZElement*) First();

	if (!z_element)
		return FALSE;

	return z_element->GetZIndex() < 0;
}

/** Invalidate fixed descendants of a given HTML_Element */

void
StackingContext::InvalidateFixedDescendants(FramesDocument* doc, HTML_Element* ancestor)
{
	BOOL descendant_found = FALSE;

	for (ZElement* elm = (ZElement*)First(); elm; elm = (ZElement*)elm->Suc())
	{
		HTML_Element* htm_elm = elm->GetHtmlElement();

		if (!ancestor || ancestor->IsAncestorOf(htm_elm))
		{
			descendant_found = TRUE;

			Box* box = htm_elm->GetLayoutBox();

			if (box->IsFixedPositionedBox())
			{
				/* Invalidate fixed positioned */
				RECT rect;
				if (box->GetRect(doc, BOUNDING_BOX, rect))
					doc->GetVisualDevice()->Update(rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1, TRUE);
			}

			StackingContext* stacking_context = box->GetLocalStackingContext();
			if (stacking_context)
				stacking_context->InvalidateFixedDescendants(doc, NULL);
		}
		else
			if (descendant_found)
				/* We have found descendants of ancestor before. Since we ZElements are
				   in prefixed tree-order, and we've now found an element that's not a
				   descendant, we won't find any more descendants. */
				return;
	}
}

/** Finish this StackingContext. */

void
StackingContext::FinishLayout(LayoutProperties* cascade)
{
	if (has_fixed_element)
	{
		if (StackingContext* parent_stacking_context = cascade->stacking_context)
			parent_stacking_context->has_fixed_element = TRUE;
	}
}

/** Remove element from lists. */

void
ZElement::Remove()
{
	ZElementList* list = (ZElementList*) GetList();

	if (list && list->IsStackingContext())
		((StackingContext*) list)->Remove(this);

	Link::Out();
}

BOOL
ZElement::HasSameClippingStack() const
{
	OP_ASSERT(Pred() && static_cast<ZElement*>(Pred())->GetZIndex() == z_index);

	if (has_same_clipping_stack != MAYBE)
		return has_same_clipping_stack == YES;
	else
	{
		HTML_Element* prev_elm = static_cast<ZElement*>(Pred())->GetHtmlElement();

		OP_ASSERT(prev_elm);

		int this_elm_level = 0;
		int prev_elm_level = 0;

		HTML_Element* iter_this = element;
		HTML_Element* iter_prev = prev_elm;

		/* Find levels of both Z Elements (this one and the predecessor, prev)
		   from z root (this_ctx_elm). */

		do
		{
			iter_this = iter_this->Parent();
			this_elm_level++;

			/* We must encounter some element with local stacking context
			   before reaching the root (or the root itself). */
			OP_ASSERT(iter_this);
		}
		while (!iter_this->GetLayoutBox()->GetLocalStackingContext());

		HTML_Element* this_ctx_elm = iter_this;

		do
		{
			iter_prev = iter_prev->Parent();
			prev_elm_level++;

			OP_ASSERT(iter_prev);
		}
		while (iter_prev != this_ctx_elm);

		Box* this_elm_box = element->GetLayoutBox();
		Box* prev_elm_box = prev_elm->GetLayoutBox();

		/* The two below track the type of the last encountered positioned box type
		   before reaching the common ancestor for each of previous and this Z Element.
		   If there is no such box, the value remains with its initial value,
		   CSS_VALUE_relative, because for the clipping stack checking purpose, it
		   has the same meaning as if the last encountered positioned box was relative. */

		CSSValue this_elm_positioned_type = this_elm_box->IsAbsolutePositionedBox() ?
											(this_elm_box->IsFixedPositionedBox() ? CSS_VALUE_fixed : CSS_VALUE_absolute) : CSS_VALUE_relative;
		CSSValue prev_elm_positioned_type = prev_elm_box->IsAbsolutePositionedBox() ?
											(prev_elm_box->IsFixedPositionedBox() ? CSS_VALUE_fixed : CSS_VALUE_absolute) : CSS_VALUE_relative;

		iter_this = element;
		iter_prev = prev_elm;

		// Make both levels equal.

		while (this_elm_level > prev_elm_level)
		{
			this_elm_level--;
			iter_this = iter_this->Parent();

			if (z_index != 0)
			{
				if (iter_this != prev_elm)
				{
					/* There may be a positioned box on the path from this element to
					   the common ancestor. */

					Box* box = iter_this->GetLayoutBox();

					/* Such potential positioned box doesn't establish its stacking context
					   however. Otherwise this element wouldn't be on the same stacking context
					   as prev_elm. That also means the z-index of the potential positioned
					   box'es Z Element is zero (which is different than the one of this). */
					OP_ASSERT(!box->GetLocalStackingContext());

					if (box->IsPositionedBox() && this_elm_positioned_type != CSS_VALUE_fixed)
						this_elm_positioned_type = box->IsAbsolutePositionedBox() ?
							(box->IsFixedPositionedBox() ? CSS_VALUE_fixed : CSS_VALUE_absolute) : CSS_VALUE_relative;
				}
			}
			else
				/* Unless this Z Element is a decendant of the previous one and we just
				   reached the previous element with iter_new, we can't have any positioned
				   boxes on the path to the common ancestor.

				   Suppose we have an ancestor A that has a positioned box on the path to
				   the common ancestor. If A doesn't establish its own stacking context, it
				   has zero z-index, so this element is not a logical successor (with the
				   same z-index) of prev_elm. If A does establish its own stacking context,
				   this element is not on the same stacking context as prev_elm. In both
				   cases we have a contradiction with the expectations about the prev_elm
				   and this element. */

				OP_ASSERT(iter_this == prev_elm || !iter_this->GetLayoutBox()->IsPositionedBox());
		}

		while (prev_elm_level > this_elm_level)
		{
			prev_elm_level--;
			iter_prev = iter_prev->Parent();
			Box* box = iter_prev->GetLayoutBox();

			if (box->IsPositionedBox() && prev_elm_positioned_type != CSS_VALUE_fixed)
				prev_elm_positioned_type = box->IsAbsolutePositionedBox() ?
					(box->IsFixedPositionedBox() ? CSS_VALUE_fixed : CSS_VALUE_absolute) : CSS_VALUE_relative;
		}

		OP_ASSERT(this_elm_level > 0);

		if (iter_this != iter_prev)
		{
			// The two elements are not on the same path in the hierarchy.

			OP_ASSERT(!iter_prev->IsAncestorOf(iter_this) && !iter_this->IsAncestorOf(iter_prev));

			/* Iterate to the common ancestor simultaneously, store the positioned type
			   for the prev Z Element and this when needed. */

			while (TRUE)
			{
				this_elm_level--;
				iter_this = iter_this->Parent();

				iter_prev = iter_prev->Parent();
				Box* prev_box = iter_prev->GetLayoutBox();

				if (iter_this == iter_prev)
					break;
				else
				{
					if (z_index != 0)
					{
						/* There may be a positioned box on the path from this element to the
						   common ancestor. */

						Box* this_box = iter_this->GetLayoutBox();

						/* Such potential positioned box doesn't establish its stacking context
						   however. Otherwise this element wouldn't be on the same stacking context
						   as prev_elm. That also means the z-index of the potential positioned
						   box'es Z Element is zero (which is different than the one of this). */
						OP_ASSERT(!this_box->GetLocalStackingContext());

						if (this_box->IsPositionedBox() && this_elm_positioned_type != CSS_VALUE_fixed)
							this_elm_positioned_type = this_box->IsAbsolutePositionedBox() ?
								(this_box->IsFixedPositionedBox() ? CSS_VALUE_fixed : CSS_VALUE_absolute) : CSS_VALUE_relative;
					}
					else
						/* We can't have any positioned boxes on the path to the common ancestor.
						   Explanation why is above in the comment in one of
						   'Make both levels equal' loops. */

						OP_ASSERT(!iter_this->GetLayoutBox()->IsPositionedBox());

					if (prev_box->IsPositionedBox() && prev_elm_positioned_type != CSS_VALUE_fixed)
						prev_elm_positioned_type = prev_box->IsAbsolutePositionedBox() ?
							(prev_box->IsFixedPositionedBox() ? CSS_VALUE_fixed : CSS_VALUE_absolute) : CSS_VALUE_relative;
				}
			}
		}
		else
		{
			// This Z Element is a descendant of the previous one.

			if (this_elm_positioned_type == CSS_VALUE_fixed && prev_elm_positioned_type != CSS_VALUE_fixed)
			{
				/* This element may have a different clipping stack. We need to check
				   whether some clip rects apply to the previous one. However we can start
				   from the parent of the previous Z Element, since we will re-enter it
				   if this Z Element is a descendant. */

				this_elm_level--;
				iter_this = iter_this->Parent();
			}
			else
			{
				has_same_clipping_stack = YES;
				return TRUE;
			}
		}

		if (prev_elm_positioned_type == this_elm_positioned_type)
		{
			has_same_clipping_stack = YES;
			return TRUE;
		}

		/* Iterate from the common ancestor (or the parent of the previous Z Element
		   if this one is a descendant and we got here) to the z root and check
		   whether some element's clip rect applies only to one of the involved
		   Z elements. */

		BOOL positioned_box_encountered = FALSE;

		for ( ; this_elm_level >= 0; this_elm_level--, iter_this = iter_this->Parent())
		{
			Box* box = iter_this->GetLayoutBox();

			if (box->IsPositionedBox())
			{
				if (this_elm_positioned_type != CSS_VALUE_fixed && prev_elm_positioned_type != CSS_VALUE_fixed)
					// Makes all the further clip rects apply for both Z elements.

					break;

				positioned_box_encountered = TRUE;
			}

			if (box->GetScrollable())
				if (this_elm_positioned_type != CSS_VALUE_fixed && prev_elm_positioned_type != CSS_VALUE_fixed)
				{
					/* Not the same clipping stack. One element has current positioned type
					   CSS_VALUE_relative, the other CSS_VALUE_absolute and the clip rect from
					   that scrollable applies only to the relative one. */

					has_same_clipping_stack = NO;
					return FALSE;
				}
				else
					if (positioned_box_encountered ||
						this_elm_positioned_type == CSS_VALUE_relative || prev_elm_positioned_type == CSS_VALUE_relative)
					{
						// The clip rect applies to the non fixed positioned Z Element.

						has_same_clipping_stack = NO;
						return FALSE;
					}
		}

		has_same_clipping_stack = YES;
		return TRUE;
	}
}

/** Update bottom aligned elements. */

void
StackingContext::UpdateBottomAligned(LayoutInfo& info)
{
	if (!needs_bottom_aligned_update)
		return;

	needs_bottom_aligned_update = FALSE;

	for (ZElement* list_element = (ZElement*) First(); list_element; list_element = (ZElement*) list_element->Suc())
	{
		Box* box = list_element->GetHtmlElement()->GetLayoutBox();

		if (box->IsPositionedBox())
			box->UpdateBottomAligned(info);
		else
		{
			StackingContext* stacking_context = box->GetLocalStackingContext();
			OP_ASSERT(stacking_context); // Assumption is that if it's not a *Positioned*Box, it's a ZRoot*Box.

			stacking_context->UpdateBottomAligned(info);
		}
	}
}

/** Add a box that is affected by the size of its containing block. */

BOOL
AffectedAbsPosDescendants::AddAffectedBox(AbsolutePositionedBox *box, BOOL skipped)
{
	AffectedAbsPosElm *elm = OP_NEW(AffectedAbsPosElm, (box, last, skipped));
	if (!elm)
		return FALSE;

	last = elm;

	return TRUE;
}

/** The size of the containing block established by this element has changed. */

void
AffectedAbsPosDescendants::ContainingBlockChanged(LayoutInfo &info, Box *containing_block)
{
	while (last)
	{
		AffectedAbsPosElm *elm = last;
		AbsolutePositionedBox *box = elm->GetLayoutBox();
		BOOL skipped = elm->WasSkipped();

		box->ContainingBlockChanged(info, containing_block->HasContainingWidthChanged(skipped), containing_block->HasContainingHeightChanged(skipped));

		last = elm->Unlink();
		OP_DELETE(elm);
	}
}

/** Calculate and set offset caused by relative positioning. */

void
PositionedElement::Layout(LayoutProperties* cascade)
{
	poselm_packed.containing_block_width_changed = 0;
	poselm_packed.containing_block_height_calculated = 0;
	poselm_packed.containing_block_height_changed = 0;

	if (cascade->stacking_context)
		cascade->stacking_context->AddZElement(this);

	cascade->GetProps()->GetLeftAndTopOffset(x_offset, y_offset);
}

/** Calculate size and position of the containing block established by this box.

	Only to be called after the container of this box has been reflowed.

	@return FALSE on memory error, TRUE otherwise. */

BOOL
PositionedInlineBox::CalculateContainingBlock(FramesDocument *doc, ContainingBlockReason reason)
{
	OP_ASSERT(containing_block_calculated == CONTAINING_BLOCK_REASON_NOT_CALCULATED);

	LayoutCoord old_height = containing_block_height;
	LayoutCoord old_width = containing_block_width;

	containing_block_x = LayoutCoord(0);
	containing_block_y = LayoutCoord(0);
	containing_block_width = LayoutCoord(0);
	containing_block_height = LayoutCoord(0);

	HTML_Element* html_element = GetHtmlElement();
	HTML_Element* containing_element = GetContainingElement(html_element);
	HTML_Element* target_ancestor = NULL;

	/* Find containing element to start partial traversal from. Partial traversal
	   can be started from block boxes and table cells. */

	while (containing_element &&
		   (!containing_element->GetLayoutBox()->IsBlockBox() || containing_element->GetLayoutBox()->TraverseInLine()) &&
		   !containing_element->GetLayoutBox()->IsTableCell() &&
		   !containing_element->GetLayoutBox()->IsTableCaption())
	{
		/* We cannot start traversing from an inline containing element, so we store the closest
		   inline container in the traversal object so that we can store its position during
		   traversal and subtract it from the block or table cell container offset after the traversal. */

		if (!target_ancestor && containing_element->GetLayoutBox()->TraverseInLine())
		{
			/* Containing block which is inline (or to be traversed during line
			   traversal) is either (inline-)block, (inline-)table or
			   (inline-)flex. */

			OP_ASSERT(containing_element->GetLayoutBox()->IsContainingElement());

			target_ancestor = containing_element;
		}

		containing_element = GetContainingElement(containing_element);
	}

	OP_ASSERT(containing_element);

	/* Create cascade for the starting point of the partial traversal. */

	Head props_list;
	LayoutProperties* layout_props;

	if (!LayoutProperties::CreateCascade(containing_element->Parent(), props_list, doc->GetHLDocProfile()))
		return FALSE; // OOM

	layout_props = (LayoutProperties*)props_list.Last();

	/* Set up the traversal object. */

	BoxEdgesObject box_edges(doc, html_element, containing_element, BoxEdgesObject::CONTAINING_BLOCK_AREA);
	if (target_ancestor)
		box_edges.SetTargetAncestor(target_ancestor);

	/* We reset the visual_device to start translations from (0, 0) to be able
	   to use the translation values directly as containing block offsets. */

	VisualDevice* visual_device = doc->GetVisualDevice();
	VDState old_vd_state = visual_device->PushState();
	visual_device->Reset();

	Box* containing_box = containing_element->GetLayoutBox();

	/* Perform partial traversal. */

	if (containing_box->IsBlockBox() && !containing_box->IsFloatedPaneBox())
	{
		/* Start partial traversal from a block box. */

		BlockBox* block_box = (BlockBox*) containing_box;

		/* Do negative translation of the offsets translated in block_box->Traverse()
		   to make sure its content is traversed from (0, 0). */

		if (block_box->IsAbsolutePositionedBox())
		{
			LayoutCoord x, root_x;
			LayoutCoord y, root_y;

			((AbsolutePositionedBox*) block_box)->GetOffsetTranslations(&box_edges, *layout_props->GetProps(), LayoutCoord(0), LayoutCoord(0), x, y, root_x, root_y);

			OP_ASSERT(root_x == x && root_y == y);

			box_edges.Translate(-x, -y);
			box_edges.TranslateRoot(-root_x, -root_y);
			box_edges.SetFixedPos(-x, -y);
		}
		else
			box_edges.Translate(-block_box->GetX(), -block_box->GetY());

		block_box->Traverse(&box_edges, layout_props, block_box->GetContainingElement());
	}
	else
		if (containing_box->IsTableCell())
		{
			/* Start partial traversal from a table cell. */

			TableCellBox* cell = (TableCellBox*) containing_box;
			LayoutProperties* table_lprops = layout_props;

			/* Do negative translation of the offsets translated in TraverseCell to
			   make sure that the content is traversed from (0, 0). */

			box_edges.Translate(-cell->GetPositionedX(), -(cell->GetPositionedY() + cell->ComputeCellY()));

			while (!table_lprops->html_element->GetLayoutBox()->GetTableContent())
				table_lprops = table_lprops->Pred();

			cell->TraverseCell(&box_edges, table_lprops, cell->GetTableRowBox());
		}
		else
			if (containing_box->IsTableCaption())
			{
				/* Start partial traversal from a table caption box. */

				TableCaptionBox* caption = (TableCaptionBox*) containing_box;

				/* Do negative translation of the offsets translated in caption->Traverse() to
				   make sure that the content is traversed from (0, 0). */

				box_edges.Translate(-caption->GetPositionedX(), -caption->GetPositionedY());
				caption->Traverse(&box_edges, layout_props, layout_props->table);
			}

	visual_device->PopState(old_vd_state);

	props_list.Clear();

	if (box_edges.IsOutOfMemory())
		return FALSE;

	if (box_edges.GetBoxFound())
	{
		/* We found this PositionedInlineBox, get the resulting rect
		   and update the containing_block members. */

		RECT bounding_rect = box_edges.GetBoxEdges();
		RECT containing_block_rect = box_edges.GetContainingBlockRect();

		if (target_ancestor)
		{
			/* Cancel out the position of the target ancestor. */

			LayoutCoord target_x = box_edges.GetTargetAncestorX();
			LayoutCoord target_y = box_edges.GetTargetAncestorY();
			TranslateRects(bounding_rect, NULL, -target_x, -target_y);
			TranslateRects(containing_block_rect, NULL, -target_x, -target_y);
		}

		containing_block_x = LayoutCoord(containing_block_rect.left - accumulated_left_offset);
		containing_block_y = LayoutCoord(containing_block_rect.top - accumulated_top_offset);
		containing_block_width = LayoutCoord(containing_block_rect.right - containing_block_rect.left);
		containing_block_height = LayoutCoord(containing_block_rect.bottom - containing_block_rect.top);

		ScrollableArea *sc = GetScrollable();
		if (sc)
		{
			/* overflow applies to inline-block elements, but not inline
			   elements, so we will only end up here for PositionedInlineBlockBox
			   and containing_block_width may not be negative here. */

			containing_block_width -= sc->GetVerticalScrollbarWidth();
			containing_block_height -= sc->GetHorizontalScrollbarWidth();
		}

		AbsoluteBoundingBox abs_bbox;
		abs_bbox.Set(LayoutCoord(bounding_rect.left - containing_block_rect.left),
					 LayoutCoord(bounding_rect.top - containing_block_rect.top),
					 LayoutCoord(bounding_rect.right - bounding_rect.left),
					 LayoutCoord(bounding_rect.bottom - bounding_rect.top));

		bounding_box.UnionWith(abs_bbox, containing_block_width, containing_block_height, FALSE);
	}

	containing_block_calculated |= reason;

	/* It is here assumed that there is little point in differentiating between
	   the different types of containing block changes. */

	containing_block_changed = containing_block_width != old_width || containing_block_height != old_height;

	return TRUE;
}

/** Reflow box and children. */

/* virtual */ LAYST
PositionedInlineBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			cascade->stacking_context->AddZElement(&z_element);

	containing_block_changed = 0;
	containing_block_calculated = CONTAINING_BLOCK_REASON_NOT_CALCULATED;

	LayoutProperties* iter_cascade = cascade;
	LayoutProperties* end_cascade = cascade->container->GetPlaceholder()->GetCascade();

	do
	{
		if (iter_cascade->html_element->GetLayoutBox()->GetLocalStackingContext())
			break;

		iter_cascade = iter_cascade->Pred();
	}
	while (iter_cascade != end_cascade);

	if (iter_cascade == end_cascade)
		/* Need to revisit element to re-calculate containing block.
		   But not if we are on a stacking context of an inline placed element
		   that is us or our ancestor that lies within the same container.
		   In such case calculate containing block in order to be able to determine
		   a valid offset from some block level ancestor is pretty useless (e.g.
		   because the stacking context could belong to a transformed box) and
		   some code in AreaTraversalObject depends on the fact that the flag
		   CONTAINING_BLOCK_REASON_OFFSET_CALCULATION is disabled in such case.
		   The containing block may be calculated for the other reason regardless
		   though. */

		info.workplace->SetReflowElement(cascade->html_element);

	return InlineBox::Layout(cascade, info, first_child, start_position);
}

/** Translate positioned inline. */

/* virtual */ void
PositionedInlineBox::PushTranslations(LayoutInfo& info, InlineBoxReflowState* state)
{
	LayoutCoord left_offset;
	LayoutCoord top_offset;
	LayoutProperties* cascade = state->cascade;

	cascade->GetProps()->GetLeftAndTopOffset(left_offset, top_offset);

	/** Store the current inline translation. This is the inline translation
		without any top/left offsets included. */

	info.GetInlineTranslation(state->previous_inline_x, state->previous_inline_y);

	/** Set the current inline translation to the position of this inline
		without any top/left offsets included. */

	info.SetInlineTranslation(containing_block_x, containing_block_y);

	/** Translate the top/left offset for this positioned inline. */

	info.Translate(left_offset, top_offset);

	/* Store the relative position offsets in the container reflow state.
	   The offsets are used to adjust the position of the bounding box that
	   contributes to the bounding box of the Line on which this box lies. */

	cascade->container->SetRelativePosition(left_offset, top_offset);
	cascade->container->GetRelativePosition(accumulated_left_offset, accumulated_top_offset);

	/* Store previous and set current root translation. */

	info.GetRootTranslation(state->previous_root_x, state->previous_root_y);
	info.SetRootTranslation(info.GetTranslationX(), info.GetTranslationY());

#ifdef CSS_TRANSFORMS
	if (state->transform)
		cascade->container->SetTransform(state->transform->transform_context->GetCurrentTransform(),
										 state->transform->previous_transform);
#endif
}

/** Un-translate positioned inline. */

/* virtual */ void
PositionedInlineBox::PopTranslations(LayoutInfo& info, InlineBoxReflowState* state)
{
	LayoutCoord left_offset;
	LayoutCoord top_offset;
	LayoutProperties* cascade = state->cascade;

#ifdef CSS_TRANSFORMS
	if (HasTransformContext())
		cascade->container->ResetTransform(state->transform->previous_transform);
#endif

	/* Revert what we did with translations in PushTranslations. */

	cascade->GetProps()->GetLeftAndTopOffset(left_offset, top_offset);

	info.Translate(-left_offset, -top_offset);
	cascade->container->SetRelativePosition(-left_offset, -top_offset);

	info.SetInlineTranslation(state->previous_inline_x, state->previous_inline_y);
	info.SetRootTranslation(state->previous_root_x, state->previous_root_y);
}

/** Reflow box and children. */

/* virtual */ LAYST
InlineZRootBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			z_element.SetZIndex(cascade->GetProps()->z_index);

	return PositionedInlineBox::Layout(cascade, info, first_child, start_position);
}

/** Traverse box with children. */

/* virtual */ BOOL
PositionedInlineBox::LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline)
{
	HTML_Element* html_element = GetHtmlElement();

	if (traversal_object->IsTarget(html_element))
	{
		HTML_Element* old_target = traversal_object->GetTarget();

		if (old_target == html_element)
			traversal_object->SetTarget(NULL);

		BOOL continue_traverse = InlineBox::LineTraverseBox(traversal_object, parent_lprops, segment, baseline);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(old_target);

			if (!segment.stop_element || !html_element->IsAncestorOf(segment.stop_element))
				// no more lines for this element
			{
				if (traversal_object->SwitchTarget(segment.container_props->html_element) == TraversalObject::SWITCH_TARGET_NEXT_IS_DESCENDANT)
				{
					OP_ASSERT(!GetLocalStackingContext());

					/* We do not allow next target to be a descendant in inline boxes
					   that have an inline content. */
					OP_ASSERT(!content->IsInlineContent());

					/* This box was the target (whether it was entered or not is not interesting,
					   though). The next target is a descendant. We can proceed (no need to
					   return to the stacking context loop), but first we have to retraverse
					   this box with the new target set - most importantly because we have to
					   make sure that we enter it and that we do so in the right way. */

					return LineTraverseBox(traversal_object, parent_lprops, segment, baseline);
				}
			}
		}

		return continue_traverse;
	}
	else
	{
		if (segment.justify && content->IsInlineContent())
			SkipJustifiedWords(segment);

		return !segment.stop_element || !html_element->IsAncestorOf(segment.stop_element);
	}
}

/* virtual */ void
PositionedInlineBox::SignalChange(FramesDocument* doc, BoxSignalReason reason /* = BOX_SIGNAL_REASON_UNKNOWN */)
{
	InlineBox::SignalChange(doc, reason);

	if (containing_block_calculated == CONTAINING_BLOCK_REASON_NOT_CALCULATED)
		RecalculateContainingBlock(doc, NULL);
	else
		containing_block_calculated |= CONTAINING_BLOCK_REASON_OFFSET_CALCULATION;
}

/** Recalculate containing block size and schedule the specified descendant box for reflow if needed. */

/* virtual */ void
PositionedInlineBox::RecalculateContainingBlock(FramesDocument* doc, AbsolutePositionedBox* box)
{
	/* This is a suitable time to check if the size or position of the containing block
	   established by the positioned inline box changed. We couldn't do it during reflow
	   of the inline box, because at that point its container was still not reflowed, so
	   that we would get incorrect results, because inline content isn't commited to a
	   line immediately. */

	OP_ASSERT(!box || box->IsAffectedByContainingBlock());

#ifdef _DEBUG
	LayoutWorkplace* workplace = doc->GetLogicalDocument()->GetLayoutWorkplace();
	workplace->SetAllowDirtyChildPropsDebug(TRUE);
#endif // _DEBUG

	ContainingBlockReason reason = box ? CONTAINING_BLOCK_REASON_ABSOLUTE_DESCENDANT : CONTAINING_BLOCK_REASON_OFFSET_CALCULATION;

	if (containing_block_calculated == CONTAINING_BLOCK_REASON_NOT_CALCULATED)
		CalculateContainingBlock(doc, reason); // FIXME: OOM
	else
		containing_block_calculated |= reason;

#ifdef _DEBUG
	workplace->SetAllowDirtyChildPropsDebug(FALSE);
#endif // _DEBUG

	if (containing_block_changed && box && box->IsAffectedByContainingBlock())
		box->MarkDirty(doc, FALSE);
}

/** Get bounding box relative to top/left border edge of this box */

/* virtual */ void
PositionedInlineBox::GetBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow /* = TRUE */, BOOL adjust_for_multipane /* = FALSE */, BOOL apply_clip /* = TRUE */) const
{
	/* Check that the containing_block_* values have been calculated. */
	OP_ASSERT(containing_block_calculated != CONTAINING_BLOCK_REASON_NOT_CALCULATED);

	/* We don't know the actual size of the inline content spanning multiple lines
	   (negative containing_block_width/height/x/y does not cover the whole content
	   rectangle). I don't know of any code that would call this method with
	   include_overflow=FALSE. If that turns out to be necessary, the bounding box
	   for the actual content needs to be stored. */
	OP_ASSERT(include_overflow);

	box.Set(bounding_box, containing_block_width, containing_block_height);
}

OP_STATUS OptionBox::Construct(Box* select_box, LayoutWorkplace* workplace)
{
	OP_ASSERT(html_element);

	index = UINT_MAX;
	OP_STATUS status = select_box->AddOption(html_element, index);

	if (OpStatus::IsError(status))
	{
		workplace->GetFramesDocument()->GetHLDocProfile()->SetIsOutOfMemory(TRUE);
		return status;
	}
	else
		OP_ASSERT(index != UINT_MAX);

	HTML_Element* containing_element = GetContainingElement(html_element);

	if (containing_element)
	{
		/* LayoutOption if we have a child element already.

		   This happens sometimes when reflow disabled formobjects
		   and then doesn't layout them. */

		return containing_element->GetLayoutBox()->LayoutOption(workplace, index);
	}

	return OpStatus::OK;
}

/** Lay out box. */

/* virtual */ OP_STATUS
OptionBox::LayoutFormContent(LayoutWorkplace* workplace)
{
	HTML_Element* containing_element = GetContainingElement();

	if (containing_element && index != UINT_MAX)
		return containing_element->GetLayoutBox()->LayoutOption(workplace, index);

	return OpStatus::OK;
}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

/** Signal that the HTML_Element associated with this layout box about to be deleted. */

/* virtual */ void
OptionBox::SignalHtmlElementDeleted(FramesDocument* doc)
{
	HTML_Element* html_element = GetHtmlElement();

	for (HTML_Element *parent = html_element->Parent(); parent; parent = parent->Parent())
		if (parent->Type() == Markup::HTE_SELECT)
		{
			parent->GetLayoutBox()->RemoveOption(html_element);
			break;
		}
}

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

OptionGroupBox::OptionGroupBox(HTML_Element* element, Box* select_box)
  : html_element(element)
{
}

/** Finish reflowing box. */

/* virtual */ LAYST
OptionGroupBox::FinishLayout(LayoutInfo& info)
{
	return LAYOUT_CONTINUE;
}

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE

/** Signal that the HTML_Element associated with this layout box about to be deleted. */

/* virtual */ void
OptionGroupBox::SignalHtmlElementDeleted(FramesDocument* doc)
{
	HTML_Element* html_element = GetHtmlElement();

	for (HTML_Element *parent = html_element->Parent(); parent; parent = parent->Parent())
		if (parent->Type() == Markup::HTE_SELECT)
		{
			parent->GetLayoutBox()->RemoveOptionGroup(html_element);
			break;
		}
}

#endif // LAYOUT_TRAVERSE_DIRTY_TREE

/** Signal that content may have changed. */

/* virtual */ void
Content_Box::SignalChange(FramesDocument* doc, BoxSignalReason reason /* = BOX_SIGNAL_REASON_UNKNOWN */)
{
	content->SignalChange(doc);

	if (!content->IsReplaced())
	{
		HTML_Element* html_element = GetHtmlElement();

		if (html_element->HasAttr(Markup::HA_USEMAP) && (html_element->Type() == Markup::HTE_IMG ||
														 html_element->Type() == Markup::HTE_OBJECT ||
														 html_element->Type() == Markup::HTE_INPUT))
			if (html_element->HasRealSizeDependentCss() && CheckRealSizeDependentCSS(html_element, doc))
				// css properties changed, so we need to reflow

				html_element->MarkExtraDirty(doc);
			else
				if (!html_element->HasBeforeOrAfter() && !html_element->ShowAltText() && doc->GetWindow()->ShowImages())
					// Size can be determined; reflow

					html_element->MarkExtraDirty(doc);
	}

	if (content->GetReflowForced())
	{
		MarkDirty(doc, FALSE);
		content->SetReflowForced(FALSE);
	}
}

/** Get min/max width for box. */

BOOL
Content_Box::GetMinMaxWidth(LayoutCoord& min_width, LayoutCoord& normal_min_width, LayoutCoord& max_width) const
{
	return content->GetMinMaxWidth(min_width, normal_min_width, max_width);
}

/* virtual */ void
Content_Box::YieldLayout(LayoutInfo& info)
{
	content->YieldLayout(info);
}

/** Traverse the content of this box, i.e. its children. */

/* virtual */ void
Content_Box::TraverseContent(TraversalObject* traversal_object, LayoutProperties* layout_props)
{
	content->Traverse(traversal_object, layout_props);
}

/** Get bounding box relative to top/left border edge of this box */

/* virtual */ void
Content_Box::GetBoundingBox(AbsoluteBoundingBox& box, BOOL include_overflow /* = TRUE */, BOOL adjust_for_multipane /* = FALSE */, BOOL apply_clip /* = TRUE */) const
{
	/* This box type does not re-implement GetBoundingBox. This generic
	   implementation does not include any overflow. Calling GetBoundingBox
	   with include_overflow = TRUE will give you an incorrect result. */

	OP_ASSERT(!include_overflow);

	box.Set(LayoutCoord(0), LayoutCoord(0), GetWidth(), GetHeight());
	box.SetContentSize(box.GetWidth(), box.GetHeight());
}

/** Handle event that occurred on this element. */

/* virtual */ void
Content_Box::HandleEvent(FramesDocument* doc, DOM_EventType ev, int offset_x, int offset_y)
{
	content->HandleEvent(doc, ev, offset_x, offset_y);
}

/** Return TRUE if the actual value of 'overflow' is 'visible'. */

BOOL
Content_Box::IsOverflowVisible()
{
	return content->IsOverflowVisible();
}

/** Set scroll offset, if applicable for this type of box / content. */

/* virtual */ void
Content_Box::SetScrollOffset(int* x, int* y)
{
	content->SetScrollOffset(x, y);
}

/** Calculate and return bottom margins of layout element. See declaration in header file for more information. */

/* virtual */ OP_BOOLEAN
BlockBox::CalculateBottomMargins(LayoutProperties* parent_cascade, LayoutInfo& info, VerticalMargin* bottom_margin) const
{
	LayoutProperties* cascade = parent_cascade->GetChildCascade(info, GetHtmlElement());

	if (cascade)
	{
		VerticalMargin tmp_bottom_margin;
		OP_BOOLEAN state = content->CalculateBottomMargins(cascade, info, &tmp_bottom_margin);

		parent_cascade->CleanSuc();

		if (state == OpBoolean::IS_FALSE && packed.has_clearance)
			return OpBoolean::IS_TRUE;
		else
		{
			bottom_margin->Collapse(tmp_bottom_margin);
			return state;
		}
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/** Calculate and return top margins of layout element. See declaration in header file for more information. */

/* virtual */ OP_BOOLEAN
BlockBox::CalculateTopMargins(LayoutProperties* parent_cascade, LayoutInfo& info, VerticalMargin* top_margin) const
{
	LayoutProperties* cascade = parent_cascade->GetChildCascade(info, GetHtmlElement());

	if (cascade)
	{
		OP_BOOLEAN state = content->CalculateTopMargins(cascade, info, top_margin, TRUE);

		parent_cascade->CleanSuc();

		return state;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/** Set page and column breaking policies on this block box. */

void
BlockBox::SetBreakPolicies(BREAK_POLICY page_break_before, BREAK_POLICY column_break_before,
						   BREAK_POLICY page_break_after, BREAK_POLICY column_break_after)
{
	packed.page_break_before = page_break_before;
	packed.column_break_before = column_break_before;
	packed.page_break_after = page_break_after;
	packed.column_break_after = column_break_after;

	/* The column break bitfields cannot hold 'left' and 'right' values (and
	   they are meaningless in that context anyway): */

	OP_ASSERT(column_break_before != BREAK_LEFT && column_break_before != BREAK_RIGHT);
	OP_ASSERT(column_break_after != BREAK_LEFT && column_break_after != BREAK_RIGHT);
}

/** Distribute content into columns. */

/* virtual */ BOOL
BlockBox::Columnize(Columnizer& columnizer, Container* container)
{
	OP_ASSERT(!IsFloatingBox());

	if (content->IsColumnizable() && !IsEmptyListItemWithMarker())
	{
		LayoutCoord stack_position = GetStackPosition();

		columnizer.EnterChild(stack_position);

		if (!content->Columnize(columnizer, container))
			return FALSE;

		columnizer.LeaveChild(stack_position);
	}

	return TRUE;
}

/** Figure out to which column(s) or spanned element a box belongs. */

/* virtual */ void
BlockBox::FindColumn(ColumnFinder& cf, Container* container)
{
	LayoutCoord stack_position = GetStackPosition();

	cf.SetPosition(stack_position);
	cf.EnterChild(stack_position);

	if (cf.GetTarget() == this)
	{
		cf.SetBoxStartFound();
		cf.SetPosition(GetLayoutHeight());
		cf.SetBoxEndFound();
	}
	else
		if (GetHtmlElement()->IsAncestorOf(cf.GetTarget()->GetHtmlElement()))
			content->FindColumn(cf);

	cf.LeaveChild(stack_position);
}

/* virtual */ void
BlockBox::SignalChange(FramesDocument* doc, BoxSignalReason reason /* = BOX_SIGNAL_REASON_UNKNOWN */)
{
	VerticalBox::SignalChange(doc, reason);

	if (reason == BOX_SIGNAL_REASON_IMAGE_DATA_LOADED)
	{
		HTML_Element* marker = GetListMarkerElement();

		if (marker)
		{
			Box* marker_box = marker->GetLayoutBox();

			/* Mark extra dirty if there is no content or the content is inappropriate for
			   the image or svg image bullet. Just dirty otherwise. */

			if (!(marker_box && marker_box->GetBulletContent()))
				marker->MarkExtraDirty(doc);
			else
				marker->MarkDirty(doc);
		}
	}
}

/** Calculate and return bottom margins of layout element. See declaration in header file for more information. */

/* virtual */ OP_BOOLEAN
TableCaptionBox::CalculateBottomMargins(LayoutProperties* parent_cascade, LayoutInfo& info, VerticalMargin* bottom_margin) const
{
	LayoutProperties* cascade = parent_cascade->GetChildCascade(info, GetHtmlElement());

	if (cascade)
	{
		OP_BOOLEAN state = content->CalculateBottomMargins(cascade, info, bottom_margin);

		bottom_margin->max_default = LayoutCoord(0);
		bottom_margin->max_default_nonpercent = LayoutCoord(0);

		parent_cascade->CleanSuc();

		return state;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/** Calculate and return top margins of layout element. See declaration in header file for more information. */

/* virtual */ OP_BOOLEAN
TableCaptionBox::CalculateTopMargins(LayoutProperties* parent_cascade, LayoutInfo& info, VerticalMargin* top_margin) const
{
	LayoutProperties* cascade = parent_cascade->GetChildCascade(info, GetHtmlElement());

	if (cascade)
	{
		OP_BOOLEAN state = content->CalculateTopMargins(cascade, info, top_margin, TRUE);

		top_margin->max_default = LayoutCoord(0);
		top_margin->max_default_nonpercent = LayoutCoord(0);

		parent_cascade->CleanSuc();

		return state;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/** Is this box (and potentially also its children) columnizable? */

/* virtual */ BOOL
BlockBox::IsColumnizable(BOOL require_children_columnizable) const
{
	if (!packed.in_multipane)
		return FALSE;

	if (require_children_columnizable)
		return !IsAbsolutePositionedBox() && !IsFloatingBox() && !IsFloatedPaneBox() && content->IsColumnizable();
	else
		return TRUE;
}

/** Traverse box with children. */

/* virtual */ void
//FIXME:OOM Check callers
BlockBox::Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element)
{
	OP_PROBE6(OP_PROBE_BLOCKBOX_TRAVERSE);

	if (packed.on_line && !traversal_object->GetInlineFormattingContext() && traversal_object->TraverseInLogicalOrder())
		/* If we're traversing in logical order, this block must be traversed
		   during line traversal, which is not now. */

		return;

	TraverseInfo traverse_info;
	LayoutProperties* layout_props = NULL;

	traversal_object->Translate(x, y);

	if (traversal_object->EnterVerticalBox(parent_lprops, layout_props, this, traverse_info))
	{
		content->Traverse(traversal_object, layout_props);

		traversal_object->LeaveVerticalBox(layout_props, this, traverse_info);
	}

	traversal_object->Translate(-x, -y);
}

/** Traverse box with children. */

/* virtual */ BOOL
//FIXME:OOM Check callers
BlockBox::LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline)
{
	OP_ASSERT(traversal_object->GetTraverseType() == TRAVERSE_CONTENT ||
			  traversal_object->GetTraverseType() == TRAVERSE_ONE_PASS);
	OP_ASSERT(!segment.start_element);

	if (traversal_object->IsTarget(GetHtmlElement(), packed.on_line))
	{
		/* Either marked as "on a line" for logical traversal, or the target is
		   somewhere inside this block, and this may be our only opportunity to
		   catch it (if we're inside some inline zroot, for example). */

		LayoutCoord left_offset = segment.GetHorizontalOffset();
		LayoutCoord top_offset = segment.line->GetY();

		traversal_object->Translate(-left_offset, -top_offset);
		Traverse(traversal_object, parent_lprops, segment.container_props->html_element);
		traversal_object->Translate(left_offset, top_offset);
		traversal_object->SetInlineFormattingContext(TRUE);

		if (traversal_object->IsTargetDone())
			return FALSE;
	}

	return !!packed.on_line;
}

/** Traverse box with children. */

/* virtual */ void
FloatingBox::Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element)
{
	OP_PROBE3(OP_PROBE_FLOATINGBOX_TRAVERSE);

	HTML_Element* html_element = GetHtmlElement();
	TraverseType old_traverse_type = traversal_object->GetTraverseType();

	if (packed.on_line && !traversal_object->GetInlineFormattingContext() && traversal_object->TraverseInLogicalOrder())
		/* If we're traversing in logical order, this block must be traversed
		   during line traversal, which is not now. */

		return;

	if (GetLocalZElement())
	{
		if (old_traverse_type == TRAVERSE_BACKGROUND || !traversal_object->IsTarget(html_element))
			/* Positioned box or a box type that establishes a new stacking context.
			   Traversal will be done (or is already done) from StackingContext. */

			return;
	}
	else
		// Prevent content traversal unless this is the current float target.

		if (old_traverse_type == TRAVERSE_CONTENT && !traversal_object->IsTarget(html_element))
			return;

	LayoutProperties* layout_props = NULL;
	RootTranslationState root_translation_state;
	BOOL positioned = IsPositionedBox();
	BOOL traverse_descendant_target = FALSE;

	traversal_object->Translate(x, y);

	if (positioned)
		traversal_object->SyncRootScrollAndTranslation(root_translation_state);

	TraverseInfo traverse_info;
	HTML_Element* old_target = traversal_object->GetTarget();

	if (old_traverse_type == TRAVERSE_BACKGROUND)
		/* No traversal now; it is not the right pass (first background, _then_
		   floats, then content). Check if we need to traverse this floating
		   box at all. If so, add it to the float target traversal list. Actual
		   traverse will take place in the float traversal pass. */

		traverse_info.dry_run = TRUE;

#ifdef CSS_TRANSFORMS
	TransformContext *transform_context = GetTransformContext();
	TranslationState transforms_translation_state;

	if (transform_context)
	{
		OP_BOOLEAN status = transform_context->PushTransforms(traversal_object, transforms_translation_state);
		switch (status)
		{
		case OpBoolean::ERR_NO_MEMORY:
			traversal_object->SetOutOfMemory();
		case OpBoolean::IS_FALSE:
			if (traversal_object->GetTarget())
				traversal_object->SwitchTarget(containing_element);
			if (positioned)
				traversal_object->RestoreRootScrollAndTranslation(root_translation_state);
			traversal_object->Translate(-x, -y);
			return;
		}
	}
#endif // CSS_TRANSFORMS

	if (old_target == html_element)
		traversal_object->SetTarget(NULL);

	if (old_traverse_type == TRAVERSE_CONTENT && !traversal_object->GetTarget())
		traversal_object->SetTraverseType(TRAVERSE_BACKGROUND);

	if (traversal_object->EnterVerticalBox(parent_lprops, layout_props, this, traverse_info))
	{
		if (traverse_info.dry_run)
			traversal_object->AddFloat(this);
		else
		{
#ifdef DISPLAY_AVOID_OVERDRAW
			if (HasNegativeZChildren() && traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
				traversal_object->FlushBackgrounds(layout_props, this);
#endif

			TraverseZChildren(traversal_object, layout_props, FALSE, traverse_info.handle_overflow);

			TraverseInfo tmp_info;

			if (traverse_info.handle_overflow)
				PushClipRect(traversal_object, layout_props, tmp_info);

			if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
			{
				content->Traverse(traversal_object, layout_props);

#ifdef DISPLAY_AVOID_OVERDRAW
				traversal_object->FlushBackgrounds(layout_props, this);
#endif

				traversal_object->SetTraverseType(TRAVERSE_CONTENT);

				traversal_object->TraverseFloats(this, layout_props);
			}

			content->Traverse(traversal_object, layout_props);

			if (tmp_info.has_clipped)
				PopClipRect(traversal_object, tmp_info);

			TraverseZChildren(traversal_object, layout_props, TRUE, traverse_info.handle_overflow);

			traversal_object->LeaveVerticalBox(layout_props, this, traverse_info);
		}
	}
	else
		traversal_object->SetTraverseType(old_traverse_type);

#ifdef CSS_TRANSFORMS
	if (transform_context)
		transform_context->PopTransforms(traversal_object, transforms_translation_state);
#endif // CSS_TRANSFORMS

	if (old_target == html_element)
	{
		traversal_object->SetTarget(old_target);
		if (traversal_object->SwitchTarget(containing_element) == TraversalObject::SWITCH_TARGET_NEXT_IS_DESCENDANT)
		{
			OP_ASSERT(!GetLocalStackingContext());
			// SWITCH_TARGET_NEXT_IS_DESCENDANT only possible for Z element targets.
			OP_ASSERT(GetLocalZElement());
			traverse_descendant_target = TRUE;
		}
	}

	if (positioned)
		traversal_object->RestoreRootScrollAndTranslation(root_translation_state);

	traversal_object->Translate(-x, -y);

	if (traverse_descendant_target)
		/* This box was the target (whether it was entered or not is not interesting,
		   though). The next target is a descendant. We can proceed (no need to
		   return to the stacking context loop), but first we have to retraverse
		   this box with the new target set - most importantly because we have to
		   make sure that we enter it and that we do so in the right way. */

	   return Traverse(traversal_object, parent_lprops, containing_element);
}

/* virtual */ void
PositionedZRootFloatingBox::TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip)
{
	// Traverse children with z_index < 0 if bottom-up or z_index > 0 if top-down

	stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip);
}

/** Traverse box with children. */

/* virtual */ BOOL
FloatingBox::LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline)
{
	BlockBox::LineTraverseBox(traversal_object, parent_lprops, segment, baseline);

	/* BlockBox will do a fine job for us here, but we cannot just propagate
	   the return value. Even if a float is not marked as being "on a line", it
	   may end up being traversed as part of a line.

	   Consider this: <body><span><div style="float:left;"></div> x

	   Laying out the float doesn't cause a line to be created, since no inline
	   content has been allocated at that point. Therefore it will not be
	   marked as being "on a line". A line is created later, though, after the
	   'x' has been laid out. The start element of the line will be the
	   SPAN. In order to get from the start of the SPAN to the 'x', we need to
	   be able to pass through the float. */

	return !traversal_object->IsTargetDone();
}

LayoutCoord
AbsolutePositionedBox::InitialContainingBlockHeight(LayoutInfo& info) const
{
	LayoutCoord containing_block_height = info.workplace->GetLayoutViewHeight();

	FramesDocElm* containing_iframe = info.doc->GetDocManager()->GetFrame();
	if (containing_iframe &&
		containing_iframe->GetHtmlElement() &&
		containing_iframe->GetHtmlElement()->GetLayoutBox())
	{
		Box* b = containing_iframe->GetHtmlElement()->GetLayoutBox();
		if (b && b->GetContent() && b->GetContent()->IsIFrame())
		{
			IFrameContent* c = (IFrameContent*)b->GetContent();
			if (c->GetExpandIFrameHeight())
			{
				containing_block_height = LayoutCoord(0);
			}
		}
	}

	if (info.doc->GetLayoutMode() != LAYOUT_NORMAL && info.doc->IsInlineFrame() && GetHtmlElement()->Type() != Markup::HTE_DOC_ROOT)
	{
		FramesDocElm* fde = info.doc->GetDocManager()->GetFrame();
		if (info.flexible_width == FLEXIBLE_REPLACED_WIDTH_SCALE || (info.flexible_width == FLEXIBLE_REPLACED_WIDTH_SCALE_MIN && fde->GetFrameScrolling() == SCROLLING_NO))
			containing_block_height = LayoutCoord(fde->GetNormalHeight());
	}

	return containing_block_height;
}

void
AbsolutePositionedBox::GetOffsetTranslations(TraversalObject* traversal_object,
											 const HTMLayoutProperties& container_props,
											 LayoutCoord cancel_x_scroll,
											 LayoutCoord cancel_y_scroll,
											 LayoutCoord& translate_x,
											 LayoutCoord& translate_y,
											 LayoutCoord& translate_root_x,
											 LayoutCoord& translate_root_y)
{
	HTML_Element* containing_inline_element = 0;

	LayoutCoord new_x_translation = x;
	LayoutCoord new_y_translation = y;
	LayoutCoord orig_x_translation = traversal_object->GetTranslationX();
	LayoutCoord orig_y_translation = traversal_object->GetTranslationY();

	LayoutCoord orig_root_x_translation;
	LayoutCoord orig_root_y_translation;
	traversal_object->GetRootTranslation(orig_root_x_translation, orig_root_y_translation);

	LayoutCoord root_view_x(0);
	LayoutCoord root_view_y(0);

	if (abs_packed.fixed)
		traversal_object->GetViewportScroll(root_view_x, root_view_y);

	if (offset_horizontal == NO_HOFFSET)
	{
		new_x_translation += orig_x_translation + cancel_x_scroll;

		if (IsHypotheticalHorizontalStaticInline() && !TraverseInLine())
		{
			if (container_props.text_align == CSS_VALUE_center ||
#ifdef SUPPORT_TEXT_DIRECTION
				container_props.text_align == CSS_VALUE_right && container_props.direction == CSS_VALUE_ltr ||
				container_props.text_align == CSS_VALUE_left && container_props.direction == CSS_VALUE_rtl
#else
				container_props.text_align == CSS_VALUE_right
#endif
				)
			{
				/* This was an inline before it was converted to block by
				   the position property. Its horizontal position is auto.
				   Still it doesn't belong to a line. Find the line it
				   would have belonged to, had it not been absolutely
				   positioned, and adjust for text-align. */

				VerticalLayout* vertical_layout = Suc();

				while (vertical_layout && !vertical_layout->IsInStack())
					vertical_layout = vertical_layout->Suc();

				if (vertical_layout && vertical_layout->IsLine())
				{
					LayoutCoord xoffs = ((Line *)vertical_layout)->GetWidth() - ((Line *)vertical_layout)->GetUsedSpace();

					if (container_props.text_align == CSS_VALUE_center)
						xoffs /= 2;

					new_x_translation += xoffs;
				}

				/* FIXME: If vertical_layout is not a line, we probably
				   still want to adjust for text-align. */
			}
		}
	}
	else
		if (IsFixedPositionedBox())
			new_x_translation += root_view_x;
		else
		{
			if (abs_packed2.containing_block_is_inline)
			{
				/* Entering inline boxes does not change translation (and
				   we cannot even tell if we have entered the inline box at
				   this point). Add this translation now. */

				containing_inline_element = GetContainingElement(TRUE);

				new_x_translation += orig_x_translation + ((PositionedInlineBox *)containing_inline_element->GetLayoutBox())->GetContainingBlockX();
			}
			else
			{
#ifdef CSS_TRANSFORMS
				// Will only end up here for fixed positioned boxes if they are transformed.
				OP_ASSERT(!abs_packed.fixed || abs_packed2.inside_transformed);

				if (!abs_packed.fixed)
#endif
					new_x_translation += orig_root_x_translation;
			}

			new_x_translation += cancel_x_scroll;
		}

	if (offset_vertical == NO_VOFFSET)
		new_y_translation += orig_y_translation + cancel_y_scroll;
	else
		if (IsFixedPositionedBox())
			new_y_translation += root_view_y;
		else
		{
			if (abs_packed2.containing_block_is_inline)
			{
				/* Entering inline boxes does not change translation (and
				   we cannot even tell if we have entered the inline box at
				   this point). Add this translation now. */

				if (!containing_inline_element)
					containing_inline_element = GetContainingElement(TRUE);
				new_y_translation += orig_y_translation + ((PositionedInlineBox *)containing_inline_element->GetLayoutBox())->GetContainingBlockY();
			}
			else
			{
#ifdef CSS_TRANSFORMS
				// Will only end up here for fixed positioned boxes if they are transformed.
				OP_ASSERT(!abs_packed.fixed || abs_packed2.inside_transformed);

				if (!abs_packed.fixed)
#endif
					new_y_translation += orig_root_y_translation;
			}

			new_y_translation += cancel_y_scroll;
		}

	translate_x = new_x_translation - orig_x_translation;
	translate_y = new_y_translation - orig_y_translation;
	translate_root_x = new_x_translation - orig_root_x_translation;
	translate_root_y = new_y_translation - orig_root_y_translation;
}

/** Traverse box with children. */

//FIXME:OOM Check callers
/* virtual */ void
AbsolutePositionedBox::Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element)
{
	OP_PROBE6(OP_PROBE_ABSOLUTEPOSITIONEDBOX_TRAVERSE);

	HTML_Element* html_element = GetHtmlElement();

	if (html_element->Type() == Markup::HTE_DOC_ROOT ||
#ifdef SVG_SUPPORT
		html_element->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG) ||
#endif
		(traversal_object->IsTarget(html_element) &&
		 (traversal_object->GetTraverseType() == TRAVERSE_ONE_PASS ||
		  traversal_object->GetTraverseType() == TRAVERSE_CONTENT)))
	{
		LayoutProperties* layout_props = NULL;

		LayoutCoord synced_root_x_scroll;
		LayoutCoord synced_root_y_scroll;
		LayoutCoord cancel_x_scroll;
		LayoutCoord cancel_y_scroll;

		LayoutCoord old_x_scroll = traversal_object->GetScrollX();
		LayoutCoord old_y_scroll = traversal_object->GetScrollY();

		traversal_object->SyncRootScroll(synced_root_x_scroll, synced_root_y_scroll);

		if (IsFixedPositionedBox())
		{
			cancel_x_scroll = old_x_scroll;
			cancel_y_scroll = old_y_scroll;
		}
		else
		{
			cancel_x_scroll = synced_root_x_scroll;
			cancel_y_scroll = synced_root_y_scroll;
		}

		LayoutCoord translate_x;
		LayoutCoord translate_y;
		LayoutCoord translate_root_x;
		LayoutCoord translate_root_y;

		GetOffsetTranslations(traversal_object,
							  *parent_lprops->GetProps(),
							  cancel_x_scroll,
							  cancel_y_scroll,
							  translate_x,
							  translate_y,
							  translate_root_x,
							  translate_root_y);

		traversal_object->TranslateRootScroll(-cancel_x_scroll, -cancel_y_scroll);
		traversal_object->TranslateScroll(-cancel_x_scroll, -cancel_y_scroll);
		traversal_object->TranslateRoot(translate_root_x, translate_root_y);
		traversal_object->Translate(translate_x, translate_y);

#ifdef CSS_TRANSFORMS
		TransformContext *transform_context = GetTransformContext();
		TranslationState transforms_translation_state;

		if (transform_context)
		{
			OP_BOOLEAN status = transform_context->PushTransforms(traversal_object, transforms_translation_state);
			switch (status)
			{
			case OpBoolean::ERR_NO_MEMORY:
				traversal_object->SetOutOfMemory();
			case OpBoolean::IS_FALSE:
				if (traversal_object->GetTarget())
					traversal_object->SwitchTarget(containing_element);
				traversal_object->Translate(-translate_x, -translate_y);
				traversal_object->TranslateRoot(-translate_root_x, -translate_root_y);
				traversal_object->TranslateScroll(cancel_x_scroll, cancel_y_scroll);
				traversal_object->TranslateRootScroll(cancel_x_scroll - synced_root_x_scroll, cancel_y_scroll - synced_root_y_scroll);
				return;
			}
		}
#endif //CSS_TRANSFORMS

		HTML_Element* old_target = traversal_object->GetTarget();
		TraverseType old_traverse_type = traversal_object->GetTraverseType();
		TraverseInfo traverse_info;
		BOOL traverse_descendant_target = FALSE;

		if (old_target == html_element)
			traversal_object->SetTarget(NULL);

		if (!traversal_object->GetTarget() && old_traverse_type != TRAVERSE_ONE_PASS)
			traversal_object->SetTraverseType(TRAVERSE_BACKGROUND);

		if (traversal_object->EnterVerticalBox(parent_lprops, layout_props, this, traverse_info))
		{
#ifdef DISPLAY_AVOID_OVERDRAW
			if (HasNegativeZChildren() && traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
				traversal_object->FlushBackgrounds(layout_props, this);
#endif

			// Traverse positioned children with z_index < 0 if bottom-up or z_index >= 0 if top-down

			TraverseZChildren(traversal_object, layout_props, FALSE, traverse_info.handle_overflow);

			// Traverse children in normal flow

			TraverseInfo tmp_info;

			if (traverse_info.handle_overflow)
				PushClipRect(traversal_object, layout_props, tmp_info);

			if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
			{
				content->Traverse(traversal_object, layout_props);

#ifdef DISPLAY_AVOID_OVERDRAW
				traversal_object->FlushBackgrounds(layout_props, this);
#endif

				traversal_object->SetTraverseType(TRAVERSE_CONTENT);

				traversal_object->TraverseFloats(this, layout_props);
			}

			content->Traverse(traversal_object, layout_props);

			if (tmp_info.has_clipped)
				PopClipRect(traversal_object, tmp_info);

			// Traverse positioned children with z_index >= 0 if bottom-up or z_index < 0 if top-down

			TraverseZChildren(traversal_object, layout_props, TRUE, traverse_info.handle_overflow);

			traversal_object->LeaveVerticalBox(layout_props, this, traverse_info);
		}
		else
			traversal_object->SetTraverseType(old_traverse_type);

#ifdef CSS_TRANSFORMS
		if (transform_context)
			transform_context->PopTransforms(traversal_object, transforms_translation_state);
#endif // CSS_TRANSFORMS

		if (old_target == html_element)
		{
			traversal_object->SetTarget(old_target);
			if (traversal_object->SwitchTarget(containing_element) == TraversalObject::SWITCH_TARGET_NEXT_IS_DESCENDANT)
			{
				OP_ASSERT(!GetLocalStackingContext());
				traverse_descendant_target = TRUE;
			}
		}

		traversal_object->Translate(-translate_x, -translate_y);
		traversal_object->TranslateRoot(-translate_root_x, -translate_root_y);
		traversal_object->TranslateScroll(cancel_x_scroll, cancel_y_scroll);
		traversal_object->TranslateRootScroll(cancel_x_scroll - synced_root_x_scroll, cancel_y_scroll - synced_root_y_scroll);

#ifdef _DEBUG
		if (!html_element->Parent() && !traversal_object->GetDocument()->IsPrintDocument())
		{
			OP_ASSERT(!traversal_object->GetTranslationX());
			OP_ASSERT(!traversal_object->GetTranslationY());
		}
#endif

		if (traverse_descendant_target)
			/* This box was the target (whether it was entered or not is not interesting,
			   though). The next target is a descendant. We can proceed (no need to
			   return to the stacking context loop), but first we have to retraverse
			   this box with the new target set - most importantly because we have to
			   make sure that we enter it and that we do so in the right way. */

			Traverse(traversal_object, parent_lprops, containing_element);
	}
}

/** Traverse box with children. */

//FIXME:OOM Check callers
/* virtual */ BOOL
AbsolutePositionedBox::LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline)
{
	HTML_Element* html_element = GetHtmlElement();

	OP_ASSERT(traversal_object->GetTraverseType() == TRAVERSE_CONTENT ||
			  traversal_object->GetTraverseType() == TRAVERSE_ONE_PASS);
	OP_ASSERT(!segment.start_element);

	if (traversal_object->IsTarget(html_element, packed.on_line))
	{
		/* Either marked as "on a line" for logical traversal, or the target is
		   somewhere inside this block, and this may be our only opportunity to
		   catch it (if we're inside some inline zroot, for example). */

		LayoutProperties* layout_props = parent_lprops->GetChildProperties(traversal_object->GetDocument()->GetHLDocProfile(), html_element);
		if (!layout_props)
		{
			traversal_object->SetOutOfMemory();
			return FALSE;
		}

		/* Find the amount of translation that has been done from the top-left
		   corner of the border box of the closest container. Since the position
		   stored in this box is relative to that point for hypothetical static
		   positioning, we need to untranslate that translation to compensate
		   for the translation done in AbsolutePositionedBox::Traverse(). */

		LayoutCoord top_offset = segment.line->GetY();
		LayoutCoord left_offset(segment.x);

		if (IsHypotheticalHorizontalStaticInline())
		{
			/* The hypothetical horizontal static position we guess when laying out
			   absolutely positioned boxes does not include adjustments due to the
			   text-align property. For ltr content we skip untranslation of that
			   offset by setting left_offset to segment.line->GetX().

			   For rtl content the hypothetical horizontal position stored in this
			   box is stored relative to the container as if the line is right-aligned.
			   For that case, we'll do the untranslation of the left edge of the Line
			   and add the amount that the right edge of the line is translated
			   instead (by calling GetTextAlignOffset with TRUE for directional). */

#ifdef SUPPORT_TEXT_DIRECTION
			const HTMLayoutProperties& props = *parent_lprops->GetProps();

			if (props.direction == CSS_VALUE_rtl)
				left_offset -= segment.line->GetTextAlignOffset(props, FALSE, TRUE);
			else
#endif // SUPPORT_TEXT_DIRECTION
				left_offset = segment.line->GetX();

			if (segment.justify)
#ifdef SUPPORT_TEXT_DIRECTION
				if (props.direction == CSS_VALUE_rtl)
					left_offset += segment.justified_space_extra_accumulated;
				else
#endif // SUPPORT_TEXT_DIRECTION
					left_offset -= segment.justified_space_extra_accumulated;
		}

#ifdef SUPPORT_TEXT_DIRECTION
		/* Compensate for bidi segment translation. */
		left_offset += segment.offset;
#endif // SUPPORT_TEXT_DIRECTION

		traversal_object->Translate(-left_offset, -top_offset);
		Traverse(traversal_object, parent_lprops, segment.container_props->html_element);
		traversal_object->Translate(left_offset, top_offset);
		traversal_object->SetInlineFormattingContext(TRUE);

		if (traversal_object->IsTargetDone())
			return FALSE;
	}

	return TRUE;
}

/** Traverse box with children. */

/* virtual */ void
AbsoluteZRootBox::TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip)
{
	// Traverse children with z_index < 0 if bottom-up or z_index > 0 if top-down

	stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip);
}

/** Traverse box with children. */

//FIXME:OOM Check callers
/* virtual */ void
TableCaptionBox::Traverse(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table)
{
	HTML_Element* html_element = GetHtmlElement();
	LayoutProperties* layout_props = table_lprops->GetChildProperties(traversal_object->GetDocument()->GetHLDocProfile(), html_element);

	if (!layout_props)
	{
		traversal_object->SetOutOfMemory();
		return;
	}

	BOOL positioned = IsPositionedBox();
	LayoutCoord translate_x = GetPositionedX();
	LayoutCoord translate_y = GetPositionedY();

	traversal_object->Translate(translate_x, translate_y);

	TraverseType old_traverse_type = traversal_object->GetTraverseType();

	HTML_Element* old_target = traversal_object->GetTarget();
	TraverseInfo traverse_info;
	RootTranslationState root_translation_state;
	BOOL traverse_descendant_target = FALSE;

	if (positioned)
	{
		traversal_object->SyncRootScrollAndTranslation(root_translation_state);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(NULL);

			if (old_traverse_type != TRAVERSE_ONE_PASS)
				traversal_object->SetTraverseType(TRAVERSE_BACKGROUND);
		}
	}

	if (traversal_object->EnterVerticalBox(table_lprops, layout_props, this, traverse_info))
	{
		PaneTraverseState old_pane_traverse_state;

		traversal_object->StorePaneTraverseState(old_pane_traverse_state);

		if (positioned)
		{
			traversal_object->FlushBackgrounds(layout_props, this);
			TraverseZChildren(traversal_object, layout_props, FALSE, FALSE);
		}

		content->Traverse(traversal_object, layout_props);

		if (positioned)
		{
			if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
			{
				traversal_object->FlushBackgrounds(layout_props, this);
				traversal_object->SetTraverseType(TRAVERSE_CONTENT);
				traversal_object->RestorePaneTraverseState(old_pane_traverse_state);

				traversal_object->TraverseFloats(this, layout_props);

				traversal_object->RestorePaneTraverseState(old_pane_traverse_state);
				content->Traverse(traversal_object, layout_props);
			}

			PaneTraverseState new_pane_traverse_state;

			traversal_object->StorePaneTraverseState(new_pane_traverse_state);

			traversal_object->RestorePaneTraverseState(old_pane_traverse_state);
			TraverseZChildren(traversal_object, layout_props, TRUE, FALSE);
			traversal_object->RestorePaneTraverseState(new_pane_traverse_state);
		}

		traversal_object->LeaveVerticalBox(layout_props, this, traverse_info);
	}
	else
		traversal_object->SetTraverseType(old_traverse_type);

	if (positioned)
	{
		traversal_object->RestoreRootScrollAndTranslation(root_translation_state);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(old_target);
			if (traversal_object->SwitchTarget(table_lprops->html_element) == TraversalObject::SWITCH_TARGET_NEXT_IS_DESCENDANT)
				traverse_descendant_target = TRUE;
		}
	}

	traversal_object->Translate(-translate_x, -translate_y);

	if (traverse_descendant_target)
		/* This box was the target (whether it was entered or not is not interesting,
		   though). The next target is a descendant. We can proceed (no need to
		   return to the stacking context loop), but first we have to retraverse
		   this box with the new target set - most importantly because we have to
		   make sure that we enter it and that we do so in the right way. */

		Traverse(traversal_object, table_lprops, table);
}

/** Distribute content into columns. */

/* virtual */ BOOL
TableCaptionBox::Columnize(Columnizer& columnizer, Container* container)
{
	LayoutCoord stack_position = GetStaticPositionedY();

	columnizer.EnterChild(stack_position);

	if (!content->Columnize(columnizer, container))
		return FALSE;

	columnizer.LeaveChild(stack_position);

	return TRUE;
}

/** Figure out to which column(s) or spanned element a box belongs. */

/* virtual */ void
TableCaptionBox::FindColumn(ColumnFinder& cf)
{
	LayoutCoord stack_position = GetStaticPositionedY();

	cf.EnterChild(stack_position);

	if (cf.GetTarget() == this)
	{
		cf.SetBoxStartFound();
		cf.SetPosition(GetHeight());
		cf.SetBoxEndFound();
	}
	else
		if (GetHtmlElement()->IsAncestorOf(cf.GetTarget()->GetHtmlElement()))
			content->FindColumn(cf);

	cf.LeaveChild(stack_position);
}

/** Reflow box and children. */

/* virtual */ LAYST
PositionedTableCaptionBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
	{
		if (cascade->stacking_context)
			cascade->stacking_context->AddZElement(&z_element);

		cascade->GetProps()->GetLeftAndTopOffset(x_offset, y_offset);
	}

	return TableCaptionBox::Layout(cascade, info, first_child, start_position);
}

/** Traverse box with children.

	This method will traverse the contents of this box and its children, recursively. */

/* virtual */ void
PositionedTableCaptionBox::Traverse(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table)
{
	if (traversal_object->IsTarget(GetHtmlElement()))
		TableCaptionBox::Traverse(traversal_object, table_lprops, table);
}

/** Reflow box and children. */

/* virtual */ LAYST
PositionedZRootTableCaptionBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			z_element.SetZIndex(cascade->GetProps()->z_index);

	return PositionedTableCaptionBox::Layout(cascade, info, first_child, start_position);
}

/** Traverse z children. */

/* virtual */ void
PositionedZRootTableCaptionBox::TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip)
{
	// Traverse children with z_index < 0 if bottom-up or z_index > 0 if top-down

	stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip);
}

/** Compute y translation of table cell. */

LayoutCoord
TableCellBox::ComputeCellY(const TableRowBox* row, BOOL include_wrapped_offset, LayoutCoord cell_height) const
{
	OP_PROBE3(OP_PROBE_TABLECELLBOX_COMPUTECELLY);

	LayoutCoord cell_y(0);

	switch (packed.vertical_alignment)
	{
	case CELL_ALIGN_BASELINE:
		{
			cell_y = row->GetBaseline() - GetBaseline();

			if (cell_y + GetCellContentHeight() > cell_height)
				cell_y = cell_height - GetCellContentHeight();
		}
		break;

	case CELL_ALIGN_MIDDLE:
	case CELL_ALIGN_BOTTOM:
		{
			cell_y = cell_height - GetCellContentHeight();

			/* Cell height can be less than content height for a rowspanned
			   cell that is not yet complete. */

			if (cell_y < 0)
				cell_y = LayoutCoord(0);
			else
				if (packed.vertical_alignment == CELL_ALIGN_MIDDLE)
					cell_y = cell_y / LayoutCoord(2);
		}
		break;
	}

	if (include_wrapped_offset && row->IsWrapped())
		cell_y += GetCellYOffset();

	return cell_y;
}

/** Get vertical offset if cell is wrapped (AMSR) */

LayoutCoord
TableCellBox::GetCellYOffset() const
{
	OP_PROBE3(OP_PROBE_TABLECELLBOX_GETCELLYOFFSET);

	LayoutCoord y(0);
	LayoutCoord max_h(0);
	LayoutCoord prev_x = GetStaticPositionedX();
	BOOL wrapped = FALSE;
#ifdef SUPPORT_TEXT_DIRECTION
	short prev_width = GetWidth();
	BOOL is_rtl = GetTable()->IsRTL();
#endif // SUPPORT_TEXT_DIRECTION

	for (TableCellBox* cell = (TableCellBox*) Pred(); cell; cell = (TableCellBox*) cell->Pred())
	{
		LayoutCoord cell_x = cell->GetStaticPositionedX();
		LayoutCoord cell_width = cell->GetWidth();

		if (
#ifdef SUPPORT_TEXT_DIRECTION
			is_rtl && (prev_x + prev_width > cell_x + cell_width || prev_x + prev_width == cell_x + cell_width && cell_width > 0) ||
			!is_rtl &&
#endif // SUPPORT_TEXT_DIRECTION
			(prev_x < cell_x || prev_x == cell_x && cell_width > 0))
		{
			wrapped = TRUE;
			y += max_h;
			max_h = LayoutCoord(0);
		}

		if (wrapped && max_h < cell->GetCellContentHeight())
			max_h = cell->GetCellContentHeight();

		prev_x = cell_x;
#ifdef SUPPORT_TEXT_DIRECTION
		prev_width = cell_width;
#endif
	}

	return y + max_h;
}

/** Traverse box with children. */

/* virtual */ void
TableCellBox::TraverseCell(TraversalObject* traversal_object, LayoutProperties* table_lprops, const TableRowBox* row)
{
	OP_PROBE6(OP_PROBE_TABLECELLBOX_TRAVERSECELL);

	TraverseInfo traverse_info;
	RootTranslationState root_translation_state;
	LayoutProperties* layout_props = NULL;
	TableContent* table = table_lprops->html_element->GetLayoutBox()->GetTableContent();

	LayoutCoord wrapped_y(0);

	if (row->IsWrapped())
		wrapped_y += GetCellYOffset();

	if (table_lprops->GetProps()->visibility == CSS_VALUE_hidden)
		SetIsHidden();

	LayoutCoord x_translate = GetPositionedX();
	LayoutCoord y_translate = wrapped_y + GetPositionedY();

	traversal_object->Translate(x_translate, y_translate);

	OpRect clip_rect;
	LayoutCoord collapsed_translate_x(0);
	LayoutCoord collapsed_translate_y(0);
	BOOL collapsed = table && table->GetCollapsedCellRect(clip_rect, collapsed_translate_x, collapsed_translate_y, table_lprops, this, row);

	if (collapsed)
	{
		traversal_object->Translate(-collapsed_translate_x, -collapsed_translate_y);
		traversal_object->BeginCollapsedTableCellClipping(this, clip_rect, traverse_info);
	}

	HTML_Element* html_element = GetHtmlElement();
	HTML_Element* old_target = traversal_object->GetTarget();
	TraverseType old_traverse_type = traversal_object->GetTraverseType();
	BOOL positioned = IsPositionedBox();
	BOOL traverse_descendant_target = FALSE;

	if (positioned)
	{
		traversal_object->SyncRootScrollAndTranslation(root_translation_state);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(NULL);

			if (old_traverse_type != TRAVERSE_ONE_PASS)
				traversal_object->SetTraverseType(TRAVERSE_BACKGROUND);
		}
	}

	if (traversal_object->EnterVerticalBox(table_lprops, layout_props, this, traverse_info))
	{
		LayoutCoord cell_y = ComputeCellY(row, FALSE);

		traversal_object->Translate(LayoutCoord(0), cell_y);

		if (positioned)
		{
			// Flush backgrounds all the way up to the table element.

			traversal_object->FlushBackgrounds(table_lprops, this);

			TraverseZChildren(traversal_object, layout_props, FALSE, FALSE);
		}

		content->Traverse(traversal_object, layout_props);

		if (positioned)
		{
			if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
			{
				traversal_object->FlushBackgrounds(layout_props, this);
				traversal_object->SetTraverseType(TRAVERSE_CONTENT);

				traversal_object->TraverseFloats(this, layout_props);

				content->Traverse(traversal_object, layout_props);
			}

			TraverseZChildren(traversal_object, layout_props, TRUE, FALSE);
		}

		traversal_object->Translate(LayoutCoord(0), -cell_y);

		traversal_object->LeaveVerticalBox(layout_props, this, traverse_info);
	}
	else
		traversal_object->SetTraverseType(old_traverse_type);

	if (positioned)
	{
		traversal_object->RestoreRootScrollAndTranslation(root_translation_state);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(old_target);
			if (traversal_object->SwitchTarget(row->GetHtmlElement()) == TraversalObject::SWITCH_TARGET_NEXT_IS_DESCENDANT)
				traverse_descendant_target = TRUE;
		}
	}

	if (collapsed)
	{
		traversal_object->EndCollapsedTableCellClipping(this, traverse_info);
		traversal_object->Translate(collapsed_translate_x, collapsed_translate_y);
	}

	traversal_object->Translate(-x_translate, -y_translate);

	if (traverse_descendant_target)
		/* This box was the target (whether it was entered or not is not interesting,
		   though). The next target is a descendant. We can proceed (no need to
		   return to the stacking context loop), but first we have to retraverse
		   this box with the new target set - most importantly because we have to
		   make sure that we enter it and that we do so in the right way. */

		TraverseCell(traversal_object, table_lprops, row);
}

/** Get the logical top and bottom of a loose subtree of inline boxes limited by end_position.

	A subtree is defined as 'loose' if the root has vertical-alignment 'top' or 'bottom'
	and consist of all descendants that are not loose themselves. */

/* virtual */ BOOL
InlineBox::GetLooseSubtreeTopAndBottom(TraversalObject* traversal_object, LayoutProperties* cascade, LayoutCoord baseline, LayoutCoord end_position, HTML_Element* start_element, LayoutCoord& logical_top, LayoutCoord& logical_bottom)
{
	const HTMLayoutProperties& props = *cascade->GetProps();

	if (x >= end_position || props.vertical_align_type == CSS_VALUE_top || props.vertical_align_type == CSS_VALUE_bottom)
		return TRUE;

	LayoutCoord new_baseline;
	LayoutCoord height;
	LayoutCoord half_leading(0);

	if (content->IsInlineContent())
	{
		height = props.GetCalculatedLineHeight(traversal_object->GetDocument()->GetVisualDevice());
		new_baseline = LayoutCoord(props.font_ascent);
		half_leading = HTMLayoutProperties::GetHalfLeading(height, new_baseline, LayoutCoord(props.font_descent));
	}
	else
	{
		height = content->GetHeight() + (props.margin_top + props.margin_bottom);

		if (content->IsReplaced() && !content->IsReplacedWithBaseline())
			new_baseline = height;
		else
			new_baseline = content->GetBaseline(props) + props.margin_top;
	}

	if (props.vertical_align_type != CSS_VALUE_baseline)
	{
		LayoutCoord above_baseline = new_baseline + half_leading;
		LayoutCoord below_baseline = height - above_baseline;

		baseline += GetBaselineOffset(props, above_baseline, below_baseline);
	}

	LayoutCoord top = baseline - new_baseline - half_leading;
	LayoutCoord bottom = top + height;

	if (logical_top > top)
		logical_top = top;

	if (logical_bottom < bottom)
		logical_bottom = bottom;

	if (content->IsInlineContent())
		return content->GetLooseSubtreeTopAndBottom(traversal_object, cascade, baseline, end_position, start_element, logical_top, logical_bottom);
	else
		return TRUE;
}

/** Traverse box with children. */

//FIXME:OOM Check callers
/* virtual */ BOOL
InlineBox::LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline)
{
	HTML_Element* html_element = GetHtmlElement();

	OP_ASSERT(!segment.start_element || segment.start_element == html_element || !IsInlineBlockBox());

	if (segment.stop_element == html_element && x == segment.stop)
		// Traversal stops here

		return FALSE;

	if (traversal_object->GetTarget() && !traversal_object->IsTarget(html_element))
	{
		if (segment.justify && content->IsInlineContent())
			SkipJustifiedWords(segment);

		return !segment.stop_element || !html_element->IsAncestorOf(segment.stop_element);
	}

	if (IsInlineRunInBox() && parent_lprops->html_element != html_element->Parent())
	{
		AutoDeleteHead props_list;
		LayoutProperties* parent = parent_lprops;

		while (parent->html_element != html_element->Parent())
			parent = parent->Pred();

		if (LayoutProperties* cascade = parent->CloneCascade(props_list, traversal_object->GetDocument()->GetHLDocProfile()))
			return LineTraverseBox(traversal_object, cascade, segment, baseline);
		else
		{
			traversal_object->SetOutOfMemory();
			return FALSE;
		}
	}

	BOOL continue_traverse = TRUE;
	const Line* line = segment.line;
	LayoutProperties* layout_props = parent_lprops->GetChildProperties(traversal_object->GetDocument()->GetHLDocProfile(), html_element);

	if (!layout_props)
	{
		traversal_object->SetOutOfMemory();
		return FALSE;
	}

	HTMLayoutProperties& props = *layout_props->GetProps();

	// Calculate height and baseline

	LayoutCoord local_baseline;
	LayoutCoord height;
	BOOL is_inline_content = content->IsInlineContent();

	if (!is_inline_content)
	{
		if (traversal_object->GetDocument()->GetLayoutMode() != LAYOUT_NORMAL && content->IsWeakContent())
		{
			// Modify margin, padding and border if this replaced content is scaled.

			UINT32 unbreakable_width = (UINT32) html_element->GetSpecialNumAttr(Markup::LAYOUTA_UNBREAKABLE_WIDTH, SpecialNs::NS_LAYOUT);

			if (unbreakable_width)
			{
				LayoutCoord min_unbreakable_width(unbreakable_width & 0x0000ffff);
				LayoutCoord weak_unbreakable_width(unbreakable_width >> 16);

				if (props.max_width < min_unbreakable_width + weak_unbreakable_width)
					if (min_unbreakable_width <= props.max_width)
					{
						if (props.margin_left + props.margin_right > 4)
						{
							LayoutCoord margins = props.margin_left + props.margin_right;

							margins = ((margins - LayoutCoord(4)) * (props.max_width - min_unbreakable_width)) / weak_unbreakable_width + LayoutCoord(4);

							props.margin_right = margins * props.margin_right / (props.margin_left + props.margin_right);
							props.margin_left = margins - props.margin_right;
						}

						if (content->ScaleBorderAndPadding())
						{
							if (props.padding_left > 2)
								props.padding_left = ((props.padding_left - LayoutCoord(2)) * (props.max_width - min_unbreakable_width)) / weak_unbreakable_width + LayoutCoord(2);
							if (props.padding_right > 2)
								props.padding_right = ((props.padding_right - LayoutCoord(2)) * (props.max_width - min_unbreakable_width)) / weak_unbreakable_width + LayoutCoord(2);
							if (props.border.left.width > 1)
								props.border.left.width = ((props.border.left.width - 1) * (props.max_width - min_unbreakable_width)) / weak_unbreakable_width + 1;
							if (props.border.right.width > 1)
								props.border.right.width = ((props.border.right.width - 1) * (props.max_width - min_unbreakable_width)) / weak_unbreakable_width + 1;
						}
					}
			}
		}

		local_baseline = content->GetBaseline(props);
		if (content->IsReplaced() && !content->IsReplacedWithBaseline())
			/* Replaced elements do not have a baseline.
			   Fake a baseline to use when calculating 'top' position to draw. */
			local_baseline += props.margin_bottom;
		height = content->GetHeight();

		if (props.vertical_align_type != CSS_VALUE_baseline)
		{
			if (props.vertical_align_type == CSS_VALUE_top)
				baseline = local_baseline + props.margin_top;
			else
			{
				if (props.vertical_align_type == CSS_VALUE_bottom)
					baseline = local_baseline + line->GetLayoutHeight() - height - props.margin_bottom;
				else
					baseline += GetBaselineOffset(props,
												  local_baseline + props.margin_top,
												  height + props.margin_bottom - local_baseline);
			}
		}
	}
	else
	{
		local_baseline = LayoutCoord(props.font_ascent + props.padding_top + props.border.top.width);
		height = local_baseline + LayoutCoord(props.font_descent + props.padding_bottom + props.border.bottom.width);

		if (props.vertical_align_type != CSS_VALUE_baseline)
		{
			LayoutCoord half_leading = props.GetHalfLeading(traversal_object->GetDocument()->GetVisualDevice());

			if (props.vertical_align_type == CSS_VALUE_top ||
				props.vertical_align_type == CSS_VALUE_bottom)
			{
				LayoutCoord logical_top(0);
				LayoutCoord logical_bottom = height;

				if (!content->GetLooseSubtreeTopAndBottom(traversal_object,
														  layout_props,
														  local_baseline,
														  segment.stop,
														  segment.start_element,
														  logical_top,
														  logical_bottom))
				{
					traversal_object->SetOutOfMemory();
					return FALSE;
				}

				if (props.vertical_align_type == CSS_VALUE_top)
					baseline = local_baseline + half_leading - props.padding_top - LayoutCoord(props.border.top.width) - logical_top;
				else
					baseline = LayoutCoord(local_baseline + half_leading - props.padding_top - props.border.top.width +
										   line->GetLayoutHeight() - props.GetCalculatedLineHeight(traversal_object->GetDocument()->GetVisualDevice()) -
										   logical_bottom + height);
			}
			else
				baseline += GetBaselineOffset(props,
											  local_baseline + half_leading - props.padding_top - LayoutCoord(props.border.top.width),
											  LayoutCoord(props.font_descent) + half_leading);
		}

		layout_props->GetProps()->textdecoration_baseline = baseline;
	}

	BOOL draw_right = TRUE;
	BOOL draw_left = TRUE;
	BOOL draw_content = TRUE;
	LayoutCoord top = baseline - local_baseline;
	LayoutCoord left = x - segment.start + props.margin_left;
	LayoutCoord right = left + content->GetWidth();

	if (segment.start_element)
		// Traversal hasn't started yet

		if (x != segment.start && left < 0)
			/* This is a really shitty condition that needs a long explanation.
			   Normally if we traverse an inline box that span more than one
			   line segment, then if we enter the segment that starts later
			   than the box itself, we must set the left to zero, so that the box
			   will not include the part of this inline box that is not overlapping
			   with the current segment. But there is a special case with the boxes
			   that have an x pos set to negative value deliberately. We must
			   not enter the if block if we LineTraverse this box for the first time.
			   Really not sure why (perhaps ask mg).
			   However when we LineTraverse it again being in some further segment,
			   we should behave normally.
			   We can ensure that by the "segment.start != 0" part. */
			if (segment.start != 0 || !IsInlineRunInBox() && !html_element->GetIsListMarkerPseudoElement())
			{
				draw_left = FALSE;
				left = LayoutCoord(0);

				if (right == props.padding_right + props.border.right.width)
					draw_content = FALSE;
			}

	if (segment.stop_element)
	{
		/* The TRUE result of the below comparison tells us whether the box spans
		   more than one segment. If so, we should act accordingly by limiting this
		   fragment right edge and forcing segement switch after leaving this box in
		   current segment traverse (draw_right).

		   However we should not do it if this is inline block box. Inline block
		   can't be split across the segments (nor lines).
		   And the situation that the right will
		   be greater than the length of the segment can be achieved by setting
		   padding to the value bigger than the available space for the line.
		   Switching segment when there are no more on the line, would result in
		   leaving the line (CORE-41543).
		   On the other hand if such inline block is followed by a non inline block
		   and the new segment start with that following box, the segment switch
		   will occur later after realizing that we have gone past the former segment
		   already. */
		if (right > segment.length + segment.line->GetTrailingWhitespace() && !IsInlineBlockBox())
		{
			draw_right = FALSE;

			if (segment.justify)
				right = line->GetWidth();
			else
				right = segment.length;
		}
		else
			if (segment.line->GetTrailingWhitespace() && right == segment.length + segment.line->GetTrailingWhitespace())
				/* Need to check if trailing whitespace should be chopped off.
				   Look for content after this inline. */

				for (HTML_Element* sibling = html_element->NextSibling(); sibling; sibling = sibling->Next())
					if (sibling == segment.stop_element)
					{
						right -= segment.line->GetTrailingWhitespace();
						break;
					}
					else
					{
						Box* box = sibling->GetLayoutBox();

						if (box && box->GetWidth())
							break;
					}
	}
	else
		if (segment.line->GetTrailingWhitespace() && right == segment.length + segment.line->GetTrailingWhitespace())
		{
			/* Need to check if trailing whitespace should be chopped off.
			   Look for content after this inline.

			   Shittiest code ever. */

			HTML_Element* sibling = html_element;

			while (sibling)
			{
				HTML_Element* next = sibling->Suc();

				if (!next)
				{
					sibling = sibling->Parent();

					if (sibling->GetLayoutBox()->GetContainer())
					{
						// We're last in container so end here

						right -= segment.line->GetTrailingWhitespace();
						break;
					}
				}
				else
				{
					Box* box = next->GetLayoutBox();

					sibling = next;

					if (box)
					{
						if (!box->IsInlineBox() && box->IsTextBox())
						{
							right -= segment.line->GetTrailingWhitespace();
							break;
						}

						if (box->GetWidth())
							break;
					}
				}
			}
		}

	if (segment.justify && draw_right && is_inline_content)
	{
		short number_of_words = segment.line->GetNumberOfWords();
		int words = content->CountWords(segment);
		LayoutCoord justified_space_total = segment.line->GetWidth() - segment.line->GetUsedSpace();

		right += justified_space_total * LayoutCoord(segment.word_number + words - 1) / LayoutCoord(number_of_words - 1) -
				 justified_space_total * LayoutCoord(segment.word_number - 1) / LayoutCoord(number_of_words - 1);
	}

#ifdef SUPPORT_TEXT_DIRECTION
	// If the segment is rtl, the box position should be computed relative to its right edge instead of left.
	if (!segment.left_to_right)
	{
		LayoutCoord tmp = (segment.length) - left;

		left = (segment.length) - right;
		right = tmp;
	}
#endif // SUPPORT_TEXT_DIRECTION

	if (segment.justify && segment.word_number > 1)
	{
		// Add extra word-spacing
		left += segment.justified_space_extra_accumulated;
		if (draw_right)
			right += segment.justified_space_extra_accumulated;
	}

	short old_font_underline_position = props.font_underline_position;
	short old_font_underline_width = props.font_underline_width;

	if (props.text_decoration & TEXT_DECORATION_UNDERLINE)
		GetUnderline(segment.line, props.font_underline_position, props.font_underline_width);

	RECT box_area; // border edges of current inline box relative to the line

	box_area.top = top;
	box_area.left = left;
	box_area.right = right;
	box_area.bottom = box_area.top + height;

	TraverseInfo traverse_info;
	RootTranslationState root_translation_state;

	LayoutCoord left_offset(0);
	LayoutCoord top_offset(0);
	BOOL positioned = IsPositionedBox();
#ifdef CSS_TRANSFORMS
	TransformContext *transform_context = GetTransformContext();
	TranslationState transforms_translation_state;
#endif

	if (positioned)
	{
		props.GetLeftAndTopOffset(left_offset, top_offset);

		traversal_object->Translate(left_offset, top_offset);
	}

#ifdef CSS_TRANSFORMS
	if (transform_context)
	{
		OP_BOOLEAN status = transform_context->PushTransforms(traversal_object, left, top, transforms_translation_state);
		switch (status)
		{
		case OpBoolean::ERR_NO_MEMORY:
			traversal_object->SetOutOfMemory();
		case OpBoolean::IS_FALSE:
			if (positioned)
				traversal_object->Translate(-left_offset, -top_offset);
			return FALSE;
		}
	}
#endif //CSS_TRANSFORMS

	if (traversal_object->EnterInlineBox(layout_props, this, box_area, segment, draw_left, draw_right, baseline, traverse_info))
	{
		if (html_element->GetFormObject())
			traversal_object->HandleTextContent(layout_props, html_element->GetFormObject());

		if (!traverse_info.skip_this_inline && draw_content)
		{
			StackingContext* local_stacking_context = GetLocalStackingContext();

			if (content->IsContainingElement())
			{
				// inline-block, inline-table or inline-flex.

				LayoutCoord untranslate_y = baseline - content->GetBaseline(props);

				traversal_object->Translate(left, untranslate_y);

				if (positioned)
					traversal_object->SyncRootScrollAndTranslation(root_translation_state);

				if (local_stacking_context)
					local_stacking_context->Traverse(traversal_object, layout_props, this, FALSE, traverse_info.handle_overflow);

				TraverseInfo tmp_info;

				if (traverse_info.handle_overflow)
					PushClipRect(traversal_object, layout_props, tmp_info);

				if (!traversal_object->GetTarget() && traversal_object->GetTraverseType() != TRAVERSE_ONE_PASS)
				{
					traversal_object->SetTraverseType(TRAVERSE_BACKGROUND);

					content->Traverse(traversal_object, layout_props);//FIXME:OOM Check if the traversal_object is OOM

#ifdef DISPLAY_AVOID_OVERDRAW
					traversal_object->FlushBackgrounds(layout_props, this);
#endif

					traversal_object->SetTraverseType(TRAVERSE_CONTENT);
					traversal_object->TraverseFloats(this, layout_props);
				}

				content->Traverse(traversal_object, layout_props);//FIXME:OOM Check if the traversal_object is OOM

				if (tmp_info.has_clipped)
					PopClipRect(traversal_object, tmp_info);

				if (local_stacking_context)
					local_stacking_context->Traverse(traversal_object, layout_props, this, TRUE, traverse_info.handle_overflow);

				traversal_object->Translate(-left, -untranslate_y);

				if (positioned)
					traversal_object->RestoreRootScrollAndTranslation(root_translation_state);

				traversal_object->SetInlineFormattingContext(TRUE);
			}
			else
			{
				LineSegment initial_segment;

				if (local_stacking_context)
				{
					/* Make a copy. Stacking context traversal must not affect
					   regular traversal, and vice versa. */

					initial_segment = segment;

					// Traverse Z children with negative z-index.

					local_stacking_context->LineTraverse(traversal_object, layout_props, content, FALSE, initial_segment, baseline, x, traverse_info.handle_overflow);
				}

				TraverseInfo tmp_info;

				if (traverse_info.handle_overflow)
					PushClipRect(traversal_object, layout_props, tmp_info);

				// Traverse content and non-positioned children.

				continue_traverse = content->LineTraverse(traversal_object, layout_props, segment, baseline, left);//FIXME:OOM Check if the traversal_object is OOM

				if (tmp_info.has_clipped)
					PopClipRect(traversal_object, tmp_info);

				if (local_stacking_context)
					// Traverse Z children with non-negative z-index.

					local_stacking_context->LineTraverse(traversal_object, layout_props, content, TRUE, initial_segment, baseline, x, traverse_info.handle_overflow);
			}
		}
		else
			if (segment.start == segment.stop)
				continue_traverse = FALSE;

		traversal_object->LeaveInlineBox(layout_props, this, box_area, draw_left, draw_right, traverse_info);
	}

#ifdef CSS_TRANSFORMS
	if (transform_context)
		transform_context->PopTransforms(traversal_object, transforms_translation_state);
#endif

	if (positioned)
		traversal_object->Translate(-left_offset, -top_offset);

	props.font_underline_position = old_font_underline_position;
	props.font_underline_width = old_font_underline_width;

	/* Force FALSE in two cases:
	   - The box spans more than one segment (!draw_right). In such case
	   we should go to the next segment, before we go past this box during its
	   line traverse.
	   - The current segment has been already traversed completely
	   (segment.stop_element == html_element). */
	if (!draw_right || segment.stop_element == html_element)
		return FALSE;
	else
		return continue_traverse;
}

/** Absolute positioned descendants of PositionedInlineBlockBoxes contribute to the bounding box. */

/* virtual */ void
InlineBox::PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	/* We should not end up here with floats (and a bottom value) because the float propagation
	   should be stopped in Container::PropagateBottom because packed.stop_top_margin_collapsing
	   will always be TRUE for InlineBlockBoxes. */

	OP_ASSERT(opts.type != PROPAGATE_FLOAT);

	BOOL is_positioned_box = IsPositionedBox();

	if (opts.type == PROPAGATE_ABSPOS_BOTTOM_ALIGNED)
	{
		StackingContext *context = GetLocalStackingContext();

		if (context)
			context->SetNeedsBottomAlignedUpdate();

		if (is_positioned_box)
		{
			if (!context)
			{
				ReflowState* state = GetReflowState();
				state->cascade->stacking_context->SetNeedsBottomAlignedUpdate();
			}
		}

		return;
	}

	if (is_positioned_box && IsOverflowVisible() && IsInlineBlockBox())
		UpdateBoundingBox(child_bounding_box, FALSE);
}

/** Is this box (and potentially also its children) columnizable? */

/* virtual */ BOOL
InlineBlockBox::IsColumnizable(BOOL require_children_columnizable) const
{
	return FALSE;
}

/** Absolute positioned descendants of InlineBlockBoxes contribute to the bounding box. */

/* virtual */ void
InlineBlockBox::PropagateBottom(const LayoutInfo& info, LayoutCoord bottom, LayoutCoord min_bottom, const AbsoluteBoundingBox& child_bounding_box, PropagationOptions opts)
{
	/* We should not end up here with floats (and a bottom value) because the float propagation
	   should be stopped in Container::PropagateBottom because packed.stop_top_margin_collapsing
	   will always be TRUE for InlineBlockBoxes. */

	OP_ASSERT(opts.type != PROPAGATE_FLOAT);

	if (opts.type == PROPAGATE_ABSPOS_BOTTOM_ALIGNED)
	{
		StackingContext* context = GetLocalStackingContext();

		if (context)
			context->SetNeedsBottomAlignedUpdate();

		if (IsPositionedBox())
		{
			if (!context)
			{
				ReflowState* state = GetReflowState();
				state->cascade->stacking_context->SetNeedsBottomAlignedUpdate();
			}

			return;
		}
	}

	RECT static_rect;
	child_bounding_box.GetBoundingRect(static_rect);

	if (!opts.IsStatic())
	{
		/* Either horizontal or vertical abspos' position is non-static. This means that
		   adequate bbox position/extent will not contribute to inline's bbox (and it will no longer
		   depend on inline position) */

		RECT non_static_rect = static_rect;

		if (opts.has_static_h_position)
			non_static_rect.left = non_static_rect.right = 0;
		else
			static_rect.left = static_rect.right = 0;

		if (opts.has_static_v_position)
			non_static_rect.top = non_static_rect.bottom = 0;
		else
			static_rect.top = static_rect.bottom = 0;

		// Propagate non-static bbox directly to container

		ReflowState* state = GetReflowState();
		Container* container = NULL;

		if (state)
			container = state->cascade->container;
		else
		{
			HTML_Element* containing_element = GetContainingElement();

			if (containing_element)
				container = containing_element->GetLayoutBox()->GetContainer();
		}

		if (container)
		{
			AbsoluteBoundingBox non_static_box;

			non_static_box.Set(LayoutCoord(non_static_rect.left),
							   LayoutCoord(non_static_rect.top),
							   LayoutCoord(non_static_rect.right - non_static_rect.left),
							   LayoutCoord(non_static_rect.bottom - non_static_rect.top));
			non_static_box.SetContentSize(non_static_box.GetWidth(), non_static_box.GetHeight());

			container->PropagateBottom(info, bottom, min_bottom, non_static_box, opts);
		}
	}

	AbsoluteBoundingBox static_box;

	static_box.Set(LayoutCoord(static_rect.left),
				   LayoutCoord(static_rect.top),
				   LayoutCoord(static_rect.right - static_rect.left),
				   LayoutCoord(static_rect.bottom - static_rect.top));

	UpdateBoundingBox(static_box, TRUE);
}

/** Propagate a break point caused by break properties or a spanned element. */

/* virtual */ BOOL
InlineBlockBox::PropagateBreakpoint(LayoutCoord virtual_y, BREAK_TYPE break_type, MultiColBreakpoint** breakpoint)
{
	// Don't break inside inline-blocks and inline-tables

	return TRUE;
}

/** Traverse box. */

//FIXME:OOM Check callers
/* virtual */ BOOL
Text_Box::LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline)
{
	LayoutProperties* layout_props = parent_lprops->GetChildProperties(traversal_object->GetDocument()->GetHLDocProfile(), html_element);

	if (!layout_props)
	{
		traversal_object->SetOutOfMemory();
		return FALSE;
	}

	BOOL end_of_line = FALSE;

	if (traversal_object->GetTarget())
	{
		if (segment.justify)
			SkipJustifiedWords(segment);

		end_of_line = segment.stop_element == html_element;
	}
	else
	{
		const uni_char* text_content = (uni_char*) html_element->TextContent();

		if (traversal_object->GetTextElement() != html_element)
		{
			traversal_object->SetTextElement(html_element);
			traversal_object->SetWordIndex(0);
		}

		if (text_content && word_info_array)
		{
			HTMLayoutProperties& props = *layout_props->GetProps();
			const Line* line = segment.line;
			VisualDevice* visual_device = traversal_object->GetDocument()->GetVisualDevice();
			short number_of_words = line->GetNumberOfWords();
			LayoutCoord pos = traversal_object->GetWordIndex() ? traversal_object->GetWordPosition() : x;
			LayoutCoord start = pos;
			LayoutCoord end = LAYOUT_COORD_MAX;
			BOOL handle_text = props.visibility == CSS_VALUE_visible || traversal_object->GetEnterHidden();

			props.textdecoration_baseline = baseline;

			if (segment.start_element)
				// Traversal hasn't started yet

				start = segment.start;

			if (segment.stop_element == html_element)
			{
				// Traversal is terminated in this element

				end = segment.stop;

				end_of_line = TRUE;
			}

			if (handle_text)
			{
				props.SetDisplayProperties(visual_device);
				props.SetTextMetrics(visual_device);

				traversal_object->EnterTextBox(layout_props, this, segment);
			}

#ifdef DEBUG
			const uni_char* debug_end = text_content + html_element->GetTextContentLength();
#endif

			for (int i = traversal_object->GetWordIndex(); i < packed2.word_count; ++i)
			{
				const WordInfo& word_info = word_info_array[i];
				const uni_char* word = text_content + word_info.GetStart();
				int word_length = word_info.GetLength();
				LayoutCoord word_width = word_info.GetWidth();
				LayoutCoord space_width(0);
				LayoutCoord justified_space_extra(0);

				int consumed = 0;
				const UnicodePoint uc = word_length ? Unicode::GetUnicodePoint(word, word_length, consumed) : 0;

#ifdef DEBUG
				OP_ASSERT(word + word_length - 1 <= debug_end);
#endif

				if (word_info.HasTrailingWhitespace())
					space_width = GetWordSpacing(word_info.IsFirstLineWidth());

				if (uc == 173) // Soft hyphen
					word_width = LayoutCoord(0);

				if (!*word || word_info.IsCollapsed())
				{
					/* As an optimization we sometimes skip collapsed/empy space in the
					   traversals, but some traversal objects will be unhappy if words
					   are hidden from them, including everything handling selections,
					   so we leave the "skip words" optimization to traversal objects
					   that have said that they don't require a logical tree traversal. */

					if (traversal_object->TraverseInLogicalOrder()
#ifdef DOCUMENT_EDIT_SUPPORT
					 || traversal_object->GetDocument()->GetDocumentEdit()
#endif
					 )
					{
						if (word_info.IsCollapsed())
							space_width = LayoutCoord(0);

						if (!*word)
							/* It's an empty word so word_width is 1 to not collapse layout
							   but now we must use 0. */

							word_width = LayoutCoord(0);
					}
					else
						continue;
				}

				if ((word_width && end < pos + word_width) ||
					(!word_width && uc != 173 && end == pos))
				{
					OP_ASSERT(!segment.stop_element || end_of_line);
					break;
				}

				LayoutCoord new_pos = pos + word_width + space_width;

				if (pos >= start)
				{
					LayoutCoord out_x;

#ifdef SUPPORT_TEXT_DIRECTION
					if (!segment.left_to_right)
						out_x = segment.start + segment.length - (pos + word_width);
					else
#endif
						out_x = pos - segment.start;

					if (segment.justify && !word_info.IsCollapsed() && *word)
					{
						// Add extra word-spacing

						out_x += segment.justified_space_extra_accumulated;

						if (word_info.HasTrailingWhitespace())
						{
#ifdef SUPPORT_TEXT_DIRECTION
							if (!segment.left_to_right)
								--segment.word_number;
							else
#endif
								++segment.word_number;

							//if (new_pos < end) // Not the ending word
							{
								LayoutCoord justified_space = LayoutCoord(((line->GetWidth() - line->GetUsedSpace()) * (segment.word_number - 1)) /
																		  (number_of_words - 1));

								// Calculate the extra space which should be added to final selection
#ifdef SUPPORT_TEXT_DIRECTION
								if (!segment.left_to_right)
									justified_space_extra = segment.justified_space_extra_accumulated - justified_space;
								else
#endif
									justified_space_extra = justified_space - segment.justified_space_extra_accumulated;

								segment.justified_space_extra_accumulated = justified_space;
							}
						}
					}

					if (handle_text && (!traversal_object->GetTarget() || traversal_object->GetTarget() == html_element))
					{
						if (word_length)
						{
							if (uc == 173) // Soft hyphen
							{
								if (segment.stop > new_pos || (segment.stop == new_pos && (!line->Suc() || !line->Suc()->IsLine())))
									word_length = 0;
								else
									word_width = word_info.GetWidth();
							}

							// Switch font if necessary

							if (word_info.GetFontNumber() != visual_device->GetCurrentFontNumber())
								layout_props->ChangeFont(visual_device, word_info.GetFontNumber());
						}

						traversal_object->HandleTextContent(layout_props,
															this,
															word,
															word_length,
															word_width,
															segment.start + segment.length < new_pos ? LayoutCoord(0) : space_width,
															segment.start + segment.length < new_pos ? LayoutCoord(0) : justified_space_extra,
															word_info,
															out_x,
															pos,
															baseline,
															segment,
															line->GetLayoutHeight());
					}
				}

				pos = new_pos;

				traversal_object->SetWordIndex(i + 1);
			}

			traversal_object->SetWordPosition(pos);

			if (handle_text)
			{
				props.font_underline_position = MAX(props.font_underline_position, packed2.underline_position);
				props.font_underline_width = MAX(props.font_underline_width, packed2.underline_width);
				traversal_object->LeaveTextBox(layout_props, segment, baseline);
			}
		}
	}

	return !end_of_line;
}

/** Remove cached info */

/* virtual */ BOOL
Text_Box::RemoveCachedInfo()
{
	packed.remove_word_info = TRUE;

#ifdef LAYOUT_TRAVERSE_DIRTY_TREE
	DeleteWordInfoArray();
#endif // LAYOUT_TRAVERSE_DIRTY_TREE

	return TRUE;
}

void
InlineBox::PaintBgAndBorder(LayoutProperties* cascade, FramesDocument* doc, VisualDevice* visual_device, const RECT& box_area, LayoutCoord position, BOOL draw_left, BOOL draw_right) const
{
	HTML_Element* html_element = cascade->html_element;
	const HTMLayoutProperties& props = *cascade->GetProps();
	Border border = props.border;

	if (props.box_decoration_break == CSS_VALUE_slice)
	{
		if (!draw_left)
		{
			border.left.style = CSS_VALUE_none;
			border.left.width = 0;
			border.left.radius_start = 0;
			border.left.radius_end = 0;
		}

		if (!draw_right)
		{
			border.right.style = CSS_VALUE_none;
			border.right.width = 0;
			border.right.radius_start = 0;
			border.right.radius_end = 0;
		}
	}

	BackgroundsAndBorders bb(doc, visual_device, cascade, &border);
	COLORREF bg_color = BackgroundColor(props, this);

	if (HasNoBgAndBorder(bg_color, bb, props))
		return;

	OpRect border_box(0, 0, box_area.right - box_area.left, box_area.bottom - box_area.top);

	visual_device->Translate(box_area.left, box_area.top);

	if (ScrollableArea* scrollable = GetScrollable())
		bb.SetScrollOffset(scrollable->GetViewX(), scrollable->GetViewY());

	bb.SetInline(draw_left, content->GetWidth(), position > x ? position - x : LayoutCoord(0));
	bb.PaintBackgrounds(html_element, bg_color, props.font_color,
						props.bg_images, &props.box_shadows, border_box);

	bb.PaintBorders(html_element, border_box, props.font_color);

	visual_device->Translate(-box_area.left, -box_area.top);
}

/** Signal that content may have changed. */

/* virtual */ void
InlineRunInBox::SignalChange(FramesDocument* doc, BoxSignalReason reason /* = BOX_SIGNAL_REASON_UNKNOWN */)
{
	InlineBox::SignalChange(doc, reason);

	if (convert_to_block)
		GetHtmlElement()->MarkExtraDirty(doc);
}

/** Request or cancel request for block conversion.. */

void
InlineRunInBox::SetRunInBlockConversion(BOOL convert, LayoutInfo& info)
{
	if (convert_to_block != convert)
	{
		convert_to_block = convert;

		if (convert)
			info.workplace->SetReflowElement(GetHtmlElement(), TRUE);
	}
}

/** Traverse elements with z-index between given interval. */

void
StackingContext::Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, Box* parent_box, BOOL positive_z_index, BOOL handle_overflow_clip)
{
	if (traversal_object->TraverseInLogicalOrder())
		return;

	ZElement* z_element = (ZElement*) First();
	TraverseType old_traversal_type = traversal_object->GetTraverseType();
	PaneTraverseState old_pane_traverse_state;

	traversal_object->StorePaneTraverseState(old_pane_traverse_state);

	if (!z_element)
		return;

	if (positive_z_index)
	{
		while (z_element && z_element->GetZIndex() < 0)
			z_element = (ZElement*) z_element->Suc();
	}
	else
		if (z_element->GetZIndex() >= 0)
			z_element = NULL;

	if (old_traversal_type != TRAVERSE_ONE_PASS)
		traversal_object->SetTraverseType(TRAVERSE_CONTENT);

	TraverseInfo info;
	Content_Box *content_box = parent_box->IsContentBox() ? (Content_Box *)parent_box : NULL;

	OP_ASSERT(!traversal_object->GetTarget());

	while (z_element && positive_z_index ^ (z_element->GetZIndex() < 0))
	{
		HTML_Element* old_target = traversal_object->GetTarget();
		HTML_Element* html_element = z_element->GetHtmlElement();

		if ((!old_target || old_target == html_element) && traversal_object->TraversePositionedElement(html_element, parent_lprops->html_element))
		{
			HTML_Element* start_target = html_element;

			traversal_object->InitTargetTraverse(parent_lprops->html_element, start_target);

			if (handle_overflow_clip && content_box)
			{
				if (content_box->GetClipAffectsTarget(html_element))
				{
					if (!info.has_clipped)
						content_box->PushClipRect(traversal_object, parent_lprops, info);
				}
				else if (info.has_clipped)
					content_box->PopClipRect(traversal_object, info);
			}

			parent_box->TraverseContent(traversal_object, parent_lprops);

			html_element = traversal_object->GetTarget();
			z_element = html_element->GetLayoutBox()->GetLocalZElement();

			traversal_object->SetTarget(old_target);

			BOOL is_done = traversal_object->IsTargetDone();

			traversal_object->TargetFinished(parent_box);
			traversal_object->RestorePaneTraverseState(old_pane_traverse_state);

			if (!is_done)
				/* There's a target we didn't handle. This may happen when there are discrepancies
				   between logical element order and layout stack order, which may happen inside of
				   tables when we reorder table-row-groups and table-captions in the layout stack,
				   and SwitchTarget() tells us to continue to the next Z element (which we have
				   already traversed past, so we will miss it). */

				// Make sure that we handled at least one target, or we'll loop forever

				if (html_element != start_target)
					/* Traverse and search for this element from the beginning of the stacking
					   context, since we missed it. */

					continue;
		}

		z_element = (ZElement*) z_element->Suc();
	}

	/* If we for some reason didn't hit the target, reset "next container
	   element", so that our parents don't get confused. */

	traversal_object->SetNextContainerElement(NULL);
	OP_ASSERT(!traversal_object->GetTarget());

	if (info.has_clipped)
		content_box->PopClipRect(traversal_object, info);

	traversal_object->SetTraverseType(old_traversal_type);
}

/** Traverse elements with z-index between given interval. */

void
StackingContext::LineTraverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, Content* parent_content, BOOL positive_z_index, const LineSegment& segment, LayoutCoord baseline, LayoutCoord x, BOOL handle_overflow_clip)
{
	if (traversal_object->TraverseInLogicalOrder())
		return;

	ZElement* z_element = (ZElement*) First();

	if (!z_element)
		return;

	if (positive_z_index)
	{
		while (z_element && z_element->GetZIndex() < 0)
			z_element = (ZElement*) z_element->Suc();
	}
	else
		if (z_element->GetZIndex() >= 0)
			z_element = NULL;

	TraverseInfo tmp_info;
	Content_Box *parent_box = parent_content->GetPlaceholder();

	OP_ASSERT(!traversal_object->GetTarget());

	while (z_element && positive_z_index ^ (z_element->GetZIndex() < 0))
	{
		HTML_Element* old_target = traversal_object->GetTarget();
		HTML_Element* html_element = z_element->GetHtmlElement();

		if ((!old_target || old_target == html_element) &&
			(!parent_content->GetPlaceholder()->IsInlineBlockBox() || traversal_object->TraversePositionedElement(html_element, parent_lprops->html_element)))
		{
			/* Whatever target traversal does to line traversal state, it must
			   not affect the next target, nor regular non-target traversal. */

			LineSegment disposable_segment = segment;

			HTML_Element* start_target = html_element;

			if (traversal_object->GetTraverseType() != TRAVERSE_ONE_PASS)
				traversal_object->SetTraverseType(TRAVERSE_CONTENT);

			traversal_object->InitTargetTraverse(parent_lprops->html_element, start_target);

			if (handle_overflow_clip)
			{
				if (parent_box->GetClipAffectsTarget(html_element))
				{
					if (!tmp_info.has_clipped)
						parent_box->PushClipRect(traversal_object, parent_lprops, tmp_info);
				}
				else if (tmp_info.has_clipped)
					parent_box->PopClipRect(traversal_object, tmp_info);
			}

			parent_content->LineTraverse(traversal_object, parent_lprops, disposable_segment, baseline, x);

			html_element = traversal_object->GetTarget();
			z_element = html_element->GetLayoutBox()->GetLocalZElement();

			traversal_object->SetTarget(old_target);

			BOOL is_done = traversal_object->IsTargetDone();

			traversal_object->TargetFinished(parent_box);

			if (!is_done)
				/* There's a target we didn't handle. This may happen when there are discrepancies
				   between logical element order and layout stack order, which may happen inside of
				   tables when we reorder table-row-groups and table-captions in the layout stack,
				   and SwitchTarget() tells us to continue to the next Z element (which we have
				   already traversed past, so we will miss it). */

				// Make sure that we handled at least one target, or we'll loop forever

				if (html_element != start_target)
					/* Traverse and search for this element from the beginning of the stacking
					   context, since we missed it. */

					continue;
		}

		z_element = (ZElement*) z_element->Suc();
	}

	/* If we for some reason didn't hit the target, reset "next container
	   element", so that our parents don't get confused. */

	traversal_object->SetNextContainerElement(NULL);
	OP_ASSERT(!traversal_object->GetTarget());

	if (tmp_info.has_clipped)
		parent_box->PopClipRect(traversal_object, tmp_info);
}

/** Lay out box. */

/* virtual */ LAYST
BrBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	HTML_Element* html_element = cascade->html_element;

	OP_ASSERT(!first_child || first_child == html_element);

	if (Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
		return LAYOUT_OUT_OF_MEMORY;

	// Remove dirty mark

	html_element->MarkClean();

	LAYST status = cascade->container->GetNewBreak(info, cascade);

	if (status != LAYOUT_CONTINUE)
		return status;
	else
		return LayoutChildren(cascade, info, NULL, LayoutCoord(0));
}

/** Traverse box. */

/* virtual */ BOOL
BrBox::LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline)
{
	if (traversal_object->GetTraverseType() != TRAVERSE_BACKGROUND && traversal_object->EnterLayoutBreak(html_element))
	{
		LayoutProperties* layout_props = parent_lprops->GetChildProperties(traversal_object->GetDocument()->GetHLDocProfile(),
																		   html_element);

		if (!layout_props)
		{
			traversal_object->SetOutOfMemory();
			return FALSE;
		}

		/* If this is a left-to-right segment, we need to move to the end of the segment. */

		LayoutCoord x_offset = segment.length;

#ifdef SUPPORT_TEXT_DIRECTION
		if (!segment.left_to_right)
			x_offset = LayoutCoord(0);
#endif // SUPPORT_TEXT_DIRECTION

		traversal_object->Translate(x_offset, LayoutCoord(0));

		traversal_object->HandleLineBreak(layout_props, FALSE);
		traversal_object->HandleSelectableBox(layout_props);

		traversal_object->Translate(-x_offset, LayoutCoord(0));
	}

	return traversal_object->GetTarget() != NULL;
}

/** Lay out box. */

/* virtual */ LAYST
WBrBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	HTML_Element* html_element = cascade->html_element;

	if (Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
		return LAYOUT_OUT_OF_MEMORY;

	// Remove dirty mark

	html_element->MarkClean();

	return cascade->container->AllocateWbr(info, html_element);
}

/** Traverse box. */

//FIXME:OOM Check callers
/* virtual */ BOOL
NoDisplayBox::LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline)
{
	LayoutProperties* layout_props = parent_lprops->GetChildProperties(traversal_object->GetDocument()->GetHLDocProfile(), html_element);

	if (!layout_props)
	{
		traversal_object->SetOutOfMemory();
		return FALSE;
	}

	return segment.stop_element != html_element && LineTraverseChildren(traversal_object, layout_props, segment, baseline);
}

/** Calculate used vertical CSS properties (height and margins). */

/* virtual */ BOOL
TableCellBox::ResolveHeightAndVerticalMargins(LayoutProperties* cascade, LayoutCoord& content_height, LayoutCoord& margin_top, LayoutCoord& margin_bottom, LayoutInfo& info) const
{
	if (content_height > 0 && !info.hld_profile->IsInStrictMode())
	{
		const HTMLayoutProperties& props = *cascade->GetProps();
		margin_top = props.margin_top;
		margin_bottom = props.margin_bottom;
		content_height -= props.padding_top + props.padding_bottom + LayoutCoord(props.border.top.width + props.border.bottom.width);
		if (content_height < 0)
			content_height = CONTENT_HEIGHT_AUTO;

		return TRUE;
	}
	else
		return FALSE;
}

/* virtual */ LAYST
ZRootBlockBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
	{
		if (cascade->stacking_context)
			cascade->stacking_context->AddZElement(&z_element);

		LAYST status = cascade->container->CreateEmptyLine(info);
		if (status != LAYOUT_CONTINUE)
			return status;
	}

	return BlockBox::Layout(cascade, info, first_child, start_position);
}

/* virtual */ void
ZRootBlockBox::Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element)
{
	HTML_Element* html_element = GetHtmlElement();
	TraverseType old_traverse_type = traversal_object->GetTraverseType();

	if (traversal_object->IsTarget(html_element) &&
		(old_traverse_type == TRAVERSE_ONE_PASS ||
		 old_traverse_type == TRAVERSE_CONTENT))
	{
		if (packed.on_line && !traversal_object->GetInlineFormattingContext() && traversal_object->TraverseInLogicalOrder())
			/* If we're traversing in logical order, this block must be
			   traversed during line traversal, which is not now. */

			return;

		TraverseInfo traverse_info;
		LayoutProperties* layout_props = NULL;

		traversal_object->Translate(x, y);

		HTML_Element* old_target = traversal_object->GetTarget();

		if (old_target == html_element)
			traversal_object->SetTarget(NULL);

		if (!traversal_object->GetTarget() && old_traverse_type != TRAVERSE_ONE_PASS)
			traversal_object->SetTraverseType(TRAVERSE_BACKGROUND);

		if (traversal_object->EnterVerticalBox(parent_lprops, layout_props, this, traverse_info))
		{
			PaneTraverseState old_pane_traverse_state;

			traversal_object->StorePaneTraverseState(old_pane_traverse_state);

#ifdef DISPLAY_AVOID_OVERDRAW
			if (HasNegativeZChildren() && traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
				traversal_object->FlushBackgrounds(layout_props, this);
#endif

			TraverseZChildren(traversal_object, layout_props, FALSE, FALSE);
			traversal_object->RestorePaneTraverseState(old_pane_traverse_state);

			// Traverse children in normal flow

			if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
			{
				content->Traverse(traversal_object, layout_props);
				traversal_object->RestorePaneTraverseState(old_pane_traverse_state);

#ifdef DISPLAY_AVOID_OVERDRAW
				traversal_object->FlushBackgrounds(layout_props, this);
#endif

				traversal_object->SetTraverseType(TRAVERSE_CONTENT);

				traversal_object->TraverseFloats(this, layout_props);
				traversal_object->RestorePaneTraverseState(old_pane_traverse_state);
			}

			content->Traverse(traversal_object, layout_props);

			PaneTraverseState new_pane_traverse_state;

			traversal_object->StorePaneTraverseState(new_pane_traverse_state);

			// Traverse positioned children with z_index >= 0 if bottom-up or z_index < 0 if top-down

			traversal_object->RestorePaneTraverseState(old_pane_traverse_state);
			TraverseZChildren(traversal_object, layout_props, TRUE, FALSE);
			traversal_object->RestorePaneTraverseState(new_pane_traverse_state);

			traversal_object->LeaveVerticalBox(layout_props, this, traverse_info);
		}
		else
			traversal_object->SetTraverseType(old_traverse_type);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(old_target);
			traversal_object->SwitchTarget(containing_element);
		}

		traversal_object->Translate(-x, -y);
	}
}

/* virtual */ LAYST
ZRootFloatingBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if ((!first_child || first_child == cascade->html_element) && cascade->stacking_context)
		cascade->stacking_context->AddZElement(&z_element);

	return FloatingBox::Layout(cascade, info, first_child, start_position);
}

/* virtual */ LAYST
ZRootSpaceManagerBlockBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
	{
		if (cascade->stacking_context)
			cascade->stacking_context->AddZElement(&z_element);

		LAYST status = cascade->container->CreateEmptyLine(info);
		if (status != LAYOUT_CONTINUE)
			return status;
	}

	return BlockBox::Layout(cascade, info, first_child, start_position);
}

/* virtual */ void
ZRootSpaceManagerBlockBox::Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element)
{
	HTML_Element* html_element = GetHtmlElement();
	TraverseType old_traverse_type = traversal_object->GetTraverseType();

	if (traversal_object->IsTarget(html_element) &&
		(old_traverse_type == TRAVERSE_ONE_PASS ||
		 old_traverse_type == TRAVERSE_CONTENT))
	{
		if (packed.on_line && !traversal_object->GetInlineFormattingContext() && traversal_object->TraverseInLogicalOrder())
			/* If we're traversing in logical order, this block must be
			   traversed during line traversal, which is not now. */

			return;

		TraverseInfo traverse_info;
		LayoutProperties* layout_props = NULL;

		traversal_object->Translate(x, y);

		HTML_Element* old_target = traversal_object->GetTarget();

		if (old_target == html_element)
			traversal_object->SetTarget(NULL);

		if (!traversal_object->GetTarget() && old_traverse_type != TRAVERSE_ONE_PASS)
			traversal_object->SetTraverseType(TRAVERSE_BACKGROUND);

		if (traversal_object->EnterVerticalBox(parent_lprops, layout_props, this, traverse_info))
		{
			PaneTraverseState old_pane_traverse_state;

			traversal_object->StorePaneTraverseState(old_pane_traverse_state);

#ifdef DISPLAY_AVOID_OVERDRAW
			if (HasNegativeZChildren() && traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
				traversal_object->FlushBackgrounds(layout_props, this);
#endif

			TraverseZChildren(traversal_object, layout_props, FALSE, traverse_info.handle_overflow);
			traversal_object->RestorePaneTraverseState(old_pane_traverse_state);

			// Traverse children in normal flow

			TraverseInfo tmp_info;

			if (traverse_info.handle_overflow)
				PushClipRect(traversal_object, layout_props, tmp_info);

			if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
			{
				content->Traverse(traversal_object, layout_props);
				traversal_object->RestorePaneTraverseState(old_pane_traverse_state);

#ifdef DISPLAY_AVOID_OVERDRAW
				traversal_object->FlushBackgrounds(layout_props, this);
#endif

				traversal_object->SetTraverseType(TRAVERSE_CONTENT);

				traversal_object->TraverseFloats(this, layout_props);
				traversal_object->RestorePaneTraverseState(old_pane_traverse_state);
			}

			content->Traverse(traversal_object, layout_props);

			PaneTraverseState new_pane_traverse_state;

			traversal_object->StorePaneTraverseState(new_pane_traverse_state);

			if (tmp_info.has_clipped)
				PopClipRect(traversal_object, tmp_info);

			// Traverse positioned children with z_index >= 0 if bottom-up or z_index < 0 if top-down

			traversal_object->RestorePaneTraverseState(old_pane_traverse_state);
			TraverseZChildren(traversal_object, layout_props, TRUE, traverse_info.handle_overflow);
			traversal_object->RestorePaneTraverseState(new_pane_traverse_state);

			traversal_object->LeaveVerticalBox(layout_props, this, traverse_info);
		}
		else
			traversal_object->SetTraverseType(old_traverse_type);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(old_target);
			traversal_object->SwitchTarget(containing_element);
		}

		traversal_object->Translate(-x, -y);
	}
}

/* virtual */ LAYST
ZRootInlineBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if ((!first_child || first_child == cascade->html_element) && cascade->stacking_context)
		cascade->stacking_context->AddZElement(&z_element);

	return InlineBox::Layout(cascade, info, first_child, start_position);
}

/* virtual */ BOOL
ZRootInlineBox::LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline)
{
	HTML_Element* html_element = GetHtmlElement();

	if (traversal_object->IsTarget(html_element))
	{
		HTML_Element* old_target = traversal_object->GetTarget();
		BOOL continue_traverse;

		if (old_target == html_element)
			traversal_object->SetTarget(NULL);

		continue_traverse = InlineBox::LineTraverseBox(traversal_object, parent_lprops, segment, baseline);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(old_target);

			if (!segment.stop_element || !html_element->IsAncestorOf(segment.stop_element))
				// no more lines for this element
				traversal_object->SwitchTarget(segment.container_props->html_element);
		}

		return continue_traverse;
	}
	else
	{
		if (segment.justify && content->IsInlineContent())
			SkipJustifiedWords(segment);

		return !segment.stop_element || !html_element->IsAncestorOf(segment.stop_element);
	}
}

/* virtual */ LAYST
ZRootInlineBlockBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if ((!first_child || first_child == cascade->html_element) && cascade->stacking_context)
		cascade->stacking_context->AddZElement(&z_element);

	return InlineBox::Layout(cascade, info, first_child, start_position);
}

/* virtual */ BOOL
ZRootInlineBlockBox::LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline)
{
	HTML_Element* html_element = GetHtmlElement();

	if (traversal_object->IsTarget(html_element))
	{
		HTML_Element* old_target = traversal_object->GetTarget();
		BOOL continue_traverse;

		if (old_target == html_element)
			traversal_object->SetTarget(NULL);

		continue_traverse = InlineBox::LineTraverseBox(traversal_object, parent_lprops, segment, baseline);

		if (old_target == html_element)
		{
			traversal_object->SetTarget(old_target);

			if (!segment.stop_element || !html_element->IsAncestorOf(segment.stop_element))
				// no more lines for this element
				traversal_object->SwitchTarget(segment.container_props->html_element);
		}

		return continue_traverse;
	}
	else
	{
		if (segment.justify && content->IsInlineContent())
			SkipJustifiedWords(segment);

		return !segment.stop_element || !html_element->IsAncestorOf(segment.stop_element);
	}
}

/** Propagate bottom margins and bounding box to parents, and walk the dog. */

/* virtual */ void
FloatedPaneBox::PropagateBottomMargins(LayoutInfo& info, const VerticalMargin* bottom_margin, BOOL has_inflow_content)
{
	/* Nothing useful to do here. We don't know the final position of this box
	   yet, so its bounding box is not going to help us. As for margin
	   collapsing: pane-attached floats do not collapse its margins with
	   anything. */
}

/** Lay out box. */

/* virtual */ LAYST
FloatedPaneBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	HTML_Element* html_element = cascade->html_element;

	if (first_child && first_child != html_element)
		return content->Layout(cascade, info, first_child, start_position);

	VerticalBoxReflowState* state = InitialiseReflowState();

	if (!state)
		return LAYOUT_OUT_OF_MEMORY;

	Container* container = cascade->container;
	LAYST st;

	if (container->IsCssFirstLine())
	{
		st = container->CommitLineContent(info, TRUE);
		if (st != LAYOUT_CONTINUE)
			return st;

		container->ClearCssFirstLine(html_element);
		return LAYOUT_END_FIRST_LINE;
	}

	int reflow_content = html_element->MarkClean();
	const HTMLayoutProperties& props = *cascade->GetProps();
	BOOL is_positioned = IsPositionedBox();
	SpaceManager* local_space_manager = GetLocalSpaceManager();
	int column_span = props.column_span;

	OP_ASSERT(cascade->multipane_container);

	if (column_span > 1 && cascade->multipane_container->GetMultiColumnContainer())
	{
		int used_column_count = ((MultiColumnContainer*) cascade->multipane_container)->GetColumnCount();

		if (column_span > used_column_count)
			column_span = used_column_count;
	}
	else
		column_span = 1;

	if (Box::Layout(cascade, info) == LAYOUT_OUT_OF_MEMORY)
		return LAYOUT_OUT_OF_MEMORY;

	OP_ASSERT(local_space_manager);

	state->cascade = cascade;
	state->old_x = x;
	state->old_y = y;
	state->old_width = content->GetWidth();
	state->old_height = content->GetHeight();
	state->old_bounding_box = bounding_box;

#ifdef CSS_TRANSFORMS
	if (state->transform)
		state->transform->old_transform = state->transform->transform_context->GetCurrentTransform();
#endif

#ifdef LAYOUT_YIELD_REFLOW
	if (info.force_layout_changed)
	{
		local_space_manager->EnableFullBfcReflow();
		reflow_content = ELM_DIRTY;
	}
#endif // LAYOUT_YIELD_REFLOW

	x = LayoutCoord(0);
	y = LayoutCoord(0);
	packed.has_absolute_width = props.content_width >= 0;
	packed.in_multipane = 1;
	panebox_packed.is_top_float = CSS_is_top_gcpm_float_val(props.float_type);
	panebox_packed.is_far_corner_float = CSS_is_corner_gcpm_float_val(props.float_type) && cascade->multipane_container->GetMultiColumnContainer();

#ifdef PAGED_MEDIA_SUPPORT
	panebox_packed.put_on_next_page = CSS_is_next_gcpm_float_val(props.float_type);

	if (panebox_packed.put_on_next_page)
	{
		Container* multipane_container = cascade->multipane_container;

		while (multipane_container && !multipane_container->GetPagedContainer())
			multipane_container = multipane_container->GetPlaceholder()->GetCascade()->multipane_container;

		if (!multipane_container)
			// No paged container. So "next page" is a bit meaningless then.

			panebox_packed.put_on_next_page = 0;
	}
#endif // PAGED_MEDIA_SUPPORT

	panebox_packed.column_span = column_span;

	/* Set up page / column breaking policies. Only 'break-after' is supported,
	   and only forced breaking. Break prevention ('avoid*') is not
	   supported. Forcing a break after a pane-attached float means that
	   in-flow content cannot follow on the same page or in the same column(s)
	   as the float. */

#ifdef PAGED_MEDIA_SUPPORT
	if (info.paged_media != PAGEBREAK_OFF || cascade->FindMultiPaneContainer(TRUE))
		panebox_packed.force_page_break_after =
			props.page_props.break_after == CSS_VALUE_always ||
			props.page_props.break_after == CSS_VALUE_page ||
			props.page_props.break_after == CSS_VALUE_left ||
			props.page_props.break_after == CSS_VALUE_right;
#endif // PAGED_MEDIA_SUPPORT

	panebox_packed.force_column_break_after = props.page_props.break_after == CSS_VALUE_column;

	if (!is_positioned)
		cascade->multipane_container->AddStaticPaneFloat(this);

	switch (content->ComputeSize(cascade, info))
	{
	case OpBoolean::IS_TRUE:
		reflow_content |= ELM_DIRTY;

	case OpBoolean::IS_FALSE:
		break;

	default:
		return LAYOUT_OUT_OF_MEMORY;
	}

	st = container->GetNewPaneFloat(info, cascade, this);

	if (st != LAYOUT_CONTINUE)
		return st;

	margin_top = props.margin_top;
	margin_bottom = props.margin_bottom;

	if (is_positioned)
	{
		// Store relative offsets.

		LayoutCoord left_offset;
		LayoutCoord top_offset;

		props.GetLeftAndTopOffset(left_offset, top_offset);
		SetRelativePosition(left_offset, top_offset);
	}

#ifdef CSS_TRANSFORMS
	if (state->transform)
		if (!state->transform->transform_context->PushTransforms(info, state->transform->translation_state))
			return LAYOUT_OUT_OF_MEMORY;
#endif

	if (reflow_content)
	{
		if (GetLocalZElement())
			RestartStackingContext();

		if (!local_space_manager->Restart())
			return LAYOUT_OUT_OF_MEMORY;

		bounding_box.Reset(props.DecorationExtent(info.doc), LayoutCoord(0));

		st = content->Layout(cascade, info);
	}
	else
		if (!cascade->SkipBranch(info, !local_space_manager, TRUE))
			return LAYOUT_OUT_OF_MEMORY;

	return st;
}

/** Get width available for the margin box. */

/* virtual */ LayoutCoord
FloatedPaneBox::GetAvailableWidth(LayoutProperties* cascade)
{
	LayoutCoord pane_width = cascade->multipane_container->GetContentWidth();

	if (panebox_packed.column_span > 1)
	{
		const HTMLayoutProperties& flow_root_props = *cascade->multipane_container->GetPlaceholder()->GetCascade()->GetProps();

		return pane_width * LayoutCoord(panebox_packed.column_span) +
			LayoutCoord(panebox_packed.column_span - 1) * LayoutCoord(flow_root_props.column_gap);
	}
	else
		return pane_width;
}

/** Finish reflowing box. */

/* virtual */ LAYST
FloatedPaneBox::FinishLayout(LayoutInfo& info)
{
	/* Set translation. We didn't bother to do it during layout; position would
	   be wrong anyway. But FinishLayout() will attempt to untranslate... */

	info.Translate(x, y);

	LayoutProperties* cascade = GetCascade();
	LAYST st = SpaceManagerBlockBox::FinishLayout(info);

	if (st == LAYOUT_CONTINUE)
	{
		LayoutCoord margin_box_height = GetHeight() + margin_top + margin_bottom;

		cascade->multipane_container->PropagatePaneFloatHeight(margin_box_height * LayoutCoord(panebox_packed.column_span));
	}

	return st;
}

/** Traverse box with children. */

/* virtual */ void
FloatedPaneBox::Traverse(TraversalObject* traversal_object, LayoutProperties* parent_lprops, HTML_Element* containing_element)
{
	HTML_Element* html_element = GetHtmlElement();
	TraverseType old_traverse_type = traversal_object->GetTraverseType();

	if (!traversal_object->IsTarget(html_element))
		return;

	Column* pane = traversal_object->GetCurrentPane();

	if (!pane->HasFloat(this))
		// Wrong column / page

		return;

	HTML_Element* old_target = traversal_object->GetTarget();
	RootTranslationState root_translation_state;
	TraverseInfo traverse_info;
	LayoutProperties* layout_props = NULL;
	StackingContext* local_stacking_context = GetLocalStackingContext();
	LayoutCoord pane_container_x_tr;
	LayoutCoord pane_container_y_tr;

	traversal_object->GetPaneTranslation(pane_container_x_tr, pane_container_y_tr);

	LayoutCoord translate_x(pane_container_x_tr - traversal_object->GetTranslationX() + x);
	LayoutCoord translate_y(pane_container_y_tr - traversal_object->GetTranslationY() + y);

#ifdef SUPPORT_TEXT_DIRECTION
	if (panebox_packed.column_span > 1)
	{
		Container* multipane_container = NULL;

		if (Container* container = parent_lprops->html_element->GetLayoutBox()->GetContainer())
			if (container->IsMultiPaneContainer())
				multipane_container = container;

		if (!multipane_container)
			multipane_container = parent_lprops->multipane_container;

		OP_ASSERT(multipane_container->GetMultiColumnContainer());
		MultiColumnContainer* multicol = (MultiColumnContainer*) multipane_container;

		if (multicol->IsRTL())
			// The current column is the rightmost one. Get to the leftmost one.

			translate_x -= LayoutCoord(panebox_packed.column_span - 1) * (multicol->GetColumnWidth() + multicol->GetColumnGap());
	}
#endif // SUPPORT_TEXT_DIRECTION

	traversal_object->Translate(translate_x, translate_y);

	BOOL positioned = IsPositionedBox();

	if (positioned)
		traversal_object->SyncRootScrollAndTranslation(root_translation_state);

	if (old_target == html_element)
		traversal_object->SetTarget(NULL);

	if (old_traverse_type != TRAVERSE_ONE_PASS)
		traversal_object->SetTraverseType(TRAVERSE_BACKGROUND);

	// Need to mark column traversal as started while inside this float.

	PaneTraverseState old_pane_state;

	traversal_object->StorePaneTraverseState(old_pane_state);
	traversal_object->SetPaneStarted(TRUE);
	traversal_object->SetPaneStartOffset(LayoutCoord(0));

	if (traversal_object->EnterVerticalBox(parent_lprops, layout_props, this, traverse_info))
	{
		if (local_stacking_context)
		{
			traversal_object->FlushBackgrounds(layout_props, this);
			TraverseZChildren(traversal_object, layout_props, FALSE, FALSE);
		}

		if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND)
		{
			content->Traverse(traversal_object, layout_props);

			traversal_object->FlushBackgrounds(layout_props, this);
			traversal_object->SetTraverseType(TRAVERSE_CONTENT);

			traversal_object->TraverseFloats(this, layout_props);
		}

		content->Traverse(traversal_object, layout_props);

		if (local_stacking_context)
			TraverseZChildren(traversal_object, layout_props, TRUE, FALSE);

		traversal_object->LeaveVerticalBox(layout_props, this, traverse_info);
	}
	else
		traversal_object->SetTraverseType(old_traverse_type);

	traversal_object->RestorePaneTraverseState(old_pane_state);

	if (positioned)
		traversal_object->RestoreRootScrollAndTranslation(root_translation_state);

	if (old_target == html_element)
	{
		traversal_object->SetTarget(old_target);
		traversal_object->SwitchTarget(containing_element);
	}

	traversal_object->Translate(-translate_x, -translate_y);
}

/** Traverse box with children. */

/* virtual */ BOOL
FloatedPaneBox::LineTraverseBox(TraversalObject* traversal_object, LayoutProperties* parent_lprops, LineSegment& segment, LayoutCoord baseline)
{
	return TRUE;
}

/** Figure out to which column(s) or spanned element a box belongs. */

/* virtual */ void
FloatedPaneBox::FindColumn(ColumnFinder& cf, Container* container)
{
	/* Cannot find GCPM floats logically. They are instead discovered when
	   entering columns. Each column knows which GCPM floats start there. */
}

/** Insert this float into a float list. */

void
FloatedPaneBox::IntoPaneFloatList(PaneFloatList& float_list, PaneFloatEntry* insert_before)
{
	list_entry.Out();

	if (insert_before)
		list_entry.Precede(insert_before);
	else
		list_entry.Into(&float_list);
}

/** Insert this float into a float list. */

void
FloatedPaneBox::IntoPaneFloatList(PaneFloatList& float_list, int start_pane, BOOL in_next_row)
{
	list_entry.Out();
	this->start_pane = start_pane;
	panebox_packed.put_in_next_row = !!in_next_row;

	PaneFloatEntry* insert_before;

	for (insert_before = float_list.First(); insert_before; insert_before = insert_before->Suc())
	{
		FloatedPaneBox* box = insert_before->GetBox();

		if (!in_next_row || box->IsForNextRow())
			if (box->GetStartPane() > start_pane)
				break;
	}

	if (insert_before)
		list_entry.Precede(insert_before);
	else
		list_entry.Into(&float_list);
}

/** Move float to next row. */

void
FloatedPaneBox::MoveToNextRow()
{
	panebox_packed.put_in_next_row = 1;

	if (PaneFloatEntry* insert_before = list_entry.Suc())
	{
		PaneFloatEntry* last;

		do
		{
			last = insert_before;

			FloatedPaneBox* box = insert_before->GetBox();

			if (box->IsForNextRow())
				if (box->GetStartPane() > start_pane)
					break;

			insert_before = insert_before->Suc();
		}
		while (insert_before);

		list_entry.Out();

		if (insert_before)
			list_entry.Precede(insert_before);
		else
			list_entry.Follow(last);
	}
}

/** Lay out box. */

/* virtual */ LAYST
PositionedFloatedPaneBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			cascade->stacking_context->AddZElement(&z_element);

	LAYST st = FloatedPaneBox::Layout(cascade, info, first_child, start_position);

	x += left;

	return st;
}

/** Lay out box. */

/* virtual */ LAYST
PositionedZRootFloatedPaneBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			z_element.SetZIndex(cascade->GetProps()->z_index);

	return PositionedFloatedPaneBox::Layout(cascade, info, first_child, start_position);
}

/** Traverse z children. */

/* virtual */ void
PositionedZRootFloatedPaneBox::TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip)
{
	// Traverse children with z_index < 0 if bottom-up or z_index > 0 if top-down

	stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip);
}

/** Lay out box. */

/* virtual */ LAYST
ZRootFloatedPaneBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
		{
			cascade->stacking_context->AddZElement(&z_element);
			z_element.SetZIndex(cascade->GetProps()->z_index);
		}

	return FloatedPaneBox::Layout(cascade, info, first_child, start_position);
}

/** Traverse z children. */

/* virtual */ void
ZRootFloatedPaneBox::TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* layout_props, BOOL after, BOOL handle_overflow_clip)
{
	// Traverse children with z_index < 0 if bottom-up or z_index > 0 if top-down

	stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip);
}

#ifdef CSS_TRANSFORMS

void
TranslationState::Read(const TraversalObject *traversal_object)
{
	translation_x = traversal_object->GetTranslationX();
	translation_y = traversal_object->GetTranslationY();
	traversal_object->GetRootTranslation(root_translation_x, root_translation_y);
	traversal_object->GetFixedPos(fixed_pos_x, fixed_pos_y);
	old_transform_root = traversal_object->GetTransformRoot();
}

void
TranslationState::Write(TraversalObject *traversal_object) const
{
	traversal_object->SetTranslation(translation_x, translation_y);
	traversal_object->SetRootTranslation(root_translation_x, root_translation_y);
	traversal_object->SetFixedPos(fixed_pos_x, fixed_pos_y);
	traversal_object->SetTransformRoot(old_transform_root);
}

void
TranslationState::Read(const LayoutInfo &info)
{
	translation_x = info.GetTranslationX();
	translation_y = info.GetTranslationY();
	info.GetRootTranslation(root_translation_x, root_translation_y);
	info.GetFixedPos(fixed_pos_x, fixed_pos_y);
}

void
TranslationState::Write(LayoutInfo &info) const
{
	info.SetTranslation(translation_x, translation_y);
	info.SetRootTranslation(root_translation_x, root_translation_y);
	info.SetFixedPos(fixed_pos_x, fixed_pos_y);
}

OP_BOOLEAN
TransformContext::PushTransforms(TraversalObject *traversal_object, TranslationState &state) const
{
	AffineTransform transform;
	css_transforms.GetTransform(transform);
	return traversal_object->PushTransform(transform, state);
}

OP_BOOLEAN
TransformContext::PushTransforms(TraversalObject *traversal_object, LayoutCoord x, LayoutCoord y, TranslationState &state) const
{
	AffineTransform affine_transform;

	if (x != 0 || y != 0)
	{
		affine_transform.LoadTranslate(float(x), float(y));
		css_transforms.ApplyToAffineTransform(affine_transform);
		affine_transform.PostTranslate( - float(x), - float(y));
	}
	else
		css_transforms.GetTransform(affine_transform);

	return traversal_object->PushTransform(affine_transform, state);
}

void
TransformContext::PopTransforms(TraversalObject *traversal_object, const TranslationState &state) const
{
	traversal_object->PopTransform(state);
}

BOOL
TransformContext::PushTransforms(LayoutInfo &info, TranslationState &state) const
{
	VisualDevice *visual_device = info.visual_device;

	AffineTransform transform;
	css_transforms.GetTransform(transform);

	if (OpStatus::IsMemoryError(visual_device->PushTransform(transform)))
		return FALSE;

	state.Read(info);

	info.SetTranslation(LayoutCoord(0), LayoutCoord(0));
	info.SetRootTranslation(LayoutCoord(0), LayoutCoord(0));
	info.SetFixedPos(LayoutCoord(0), LayoutCoord(0));

	return TRUE;
}

void
TransformContext::PopTransforms(LayoutInfo &info, const TranslationState &state) const
{
	VisualDevice *visual_device = info.visual_device;
	visual_device->PopTransform();
	state.Write(info);
}

BOOL
TransformContext::Finish(const LayoutInfo &info, LayoutProperties *cascade, LayoutCoord box_width, LayoutCoord box_height)
{
	HTMLayoutProperties &props = *cascade->GetProps();
	return css_transforms.ComputeTransform(info.doc, props, box_width, box_height);
}

void
TransformContext::ApplyTransform(AbsoluteBoundingBox& bounding_box)
{
	OpRect bounding_rect, content_rect;

	bounding_box.GetBoundingRect(bounding_rect);
	bounding_box.GetContentRect(content_rect);

	AffineTransform at;
	at.LoadIdentity();
	css_transforms.ApplyToAffineTransform(at);

	bounding_rect = at.GetTransformedBBox(bounding_rect);
	content_rect = at.GetTransformedBBox(content_rect);

	bounding_box.Set(LayoutCoord(bounding_rect.x), LayoutCoord(bounding_rect.y), LayoutCoord(bounding_rect.width), LayoutCoord(bounding_rect.height));
	bounding_box.SetContentSize(LayoutCoord(content_rect.width), LayoutCoord(content_rect.height));
}

void
TransformContext::ApplyTransform(OpRect& rect)
{
	AffineTransform at;
	css_transforms.GetTransform(at);
	rect = at.GetTransformedBBox(rect);
}

AffineTransform
TransformContext::GetCurrentTransform() const
{
	AffineTransform transform;
	css_transforms.GetTransform(transform);
	return transform;
}

#endif // CSS_TRANSFORMS

/** Traverse box. */

/* virtual */ void
PositionedTableRowGroupBox::Traverse(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table)
{
	if (traversal_object->IsTarget(GetHtmlElement()))
		TableRowGroupBox::Traverse(traversal_object, table_lprops, table);
	else
		if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND && !traversal_object->GetTarget() && table->GetCollapseBorders())
		{
			// All border-collapsing is done in the same traversal pass as the table's background.

			Column* pane = traversal_object->GetCurrentPane();
			LayoutCoord x = GetStaticPositionedX();
			LayoutCoord y = GetStaticPositionedY();

			traversal_object->Translate(x, y);

			for (TableRowBox* row = GetFirstRow(); row; row = (TableRowBox*) row->Suc())
			{
				if (pane)
					if (!traversal_object->IsPaneStarted())
					{
						/* We're looking for a column's or page's start
						   element. Until we find it, we shouldn't traverse
						   anything. */

						if (row == pane->GetStartElement().GetTableRowBox())
							/* We found the start element. Traverse everything
							   as normally until we find the stop element. */

							traversal_object->SetPaneStarted(TRUE);
						else
							continue;
					}

				LayoutCoord row_y = row->GetStaticPositionedY();

				OP_ASSERT(row->IsTableCollapsingBorderRowBox());

				traversal_object->Translate(LayoutCoord(0), row_y);
				traversal_object->HandleCollapsedBorders((TableCollapsingBorderRowBox*) row, table);
				traversal_object->Translate(LayoutCoord(0), -row_y);

				if (pane)
					if (row == pane->GetStopElement().GetTableRowBox())
					{
						/* We found the stop element, which means that we're
						   done with a column or page. */

						traversal_object->SetPaneDone(TRUE);
						break;
					}
			}

			traversal_object->Translate(-x, -y);
		}
}

/** Reflow box and children. */

/* virtual */ LAYST
PositionedZRootTableRowGroupBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			pos_elm.SetZIndex(cascade->GetProps()->z_index);

	RestartStackingContext();

	return PositionedTableRowGroupBox::Layout(cascade, info, first_child, start_position);
}

/** Traverse z children. */

/* virtual */ void
PositionedZRootTableRowGroupBox::TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* table_lprops, BOOL after, BOOL handle_overflow_clip)
{
	// Traverse children with z_index < 0 if bottom-up or z_index > 0 if top-down

	LayoutProperties* layout_props = table_lprops->GetChildProperties(traversal_object->GetDocument()->GetHLDocProfile(), GetHtmlElement());

	if (!layout_props)
	{
		traversal_object->SetOutOfMemory();
		return;
	}

	stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip);
}

/** Traverse box with children.

	This method will traverse the contents of this box and its children, recursively. */

/* virtual */ void
PositionedTableRowBox::TraverseRow(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table)
{
	if (traversal_object->IsTarget(GetHtmlElement()))
		TableRowBox::TraverseRow(traversal_object, table_lprops, table);
}

/** Reflow box and children. */

/* virtual */ LAYST
PositionedZRootTableRowBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			pos_elm.SetZIndex(cascade->GetProps()->z_index);

	RestartStackingContext();

	return PositionedTableRowBox::Layout(cascade, info, first_child, start_position);
}

/** Traverse z children. */

/* virtual */ void
PositionedZRootTableRowBox::TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* table_lprops, BOOL after, BOOL handle_overflow_clip)
{
	// Traverse children with z_index < 0 if bottom-up or z_index > 0 if top-down

	LayoutProperties* layout_props = table_lprops->GetChildProperties(traversal_object->GetDocument()->GetHLDocProfile(), GetHtmlElement());

	if (!layout_props)
	{
		traversal_object->SetOutOfMemory();
		return;
	}

	stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip);
}

/** Traverse box with children.

	This method will traverse the contents of this box and its children, recursively. */

/* virtual */ void
PositionedTableCollapsingBorderRowBox::TraverseRow(TraversalObject* traversal_object, LayoutProperties* table_lprops, TableContent* table)
{
	if (traversal_object->IsTarget(GetHtmlElement()))
		TableCollapsingBorderRowBox::TraverseRow(traversal_object, table_lprops, table);
	else
		if (traversal_object->GetTraverseType() == TRAVERSE_BACKGROUND && !traversal_object->GetTarget())
		{
			// All border-collapsing is done in the same traversal pass as the table's background.

			LayoutCoord y = GetStaticPositionedY();

			traversal_object->Translate(LayoutCoord(0), y);
			traversal_object->HandleCollapsedBorders(this, table);
			traversal_object->Translate(LayoutCoord(0), -y);
		}
}

/** Reflow box and children. */

/* virtual */ LAYST
PositionedZRootTableCollapsingBorderRowBox::Layout(LayoutProperties* cascade, LayoutInfo& info, HTML_Element* first_child, LayoutCoord start_position)
{
	if (!first_child || first_child == cascade->html_element)
		if (cascade->stacking_context)
			pos_elm.SetZIndex(cascade->GetProps()->z_index);

	RestartStackingContext();

	return PositionedTableCollapsingBorderRowBox::Layout(cascade, info, first_child, start_position);
}

/** Traverse z children. */

/* virtual */ void
PositionedZRootTableCollapsingBorderRowBox::TraverseZChildren(TraversalObject* traversal_object, LayoutProperties* table_lprops, BOOL after, BOOL handle_overflow_clip)
{
	// Traverse children with z_index < 0 if bottom-up or z_index > 0 if top-down

	LayoutProperties* layout_props = table_lprops->GetChildProperties(traversal_object->GetDocument()->GetHLDocProfile(), GetHtmlElement());

	if (!layout_props)
	{
		traversal_object->SetOutOfMemory();
		return;
	}

	stacking_context.Traverse(traversal_object, layout_props, this, after, handle_overflow_clip);
}
