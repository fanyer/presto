/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; coding: iso-8859-1 -*-
**
** Copyright (C) 2007-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#ifdef USE_UNICODE_SEGMENTATION

#include "modules/unicode/unicode.h"
#include "modules/unicode/unicode_segmenter.h"

#ifdef MDF_OPENTYPE_SUPPORT
#include "modules/mdefont/mdf_ot_indic.h"
#endif // MDF_OPENTYPE_SUPPORT

/**
 * Check if character is in the "Control" class as defined in UAX#29 X3.1.
 * Also returns true for CR and LF.
 */
/*static*/ inline bool UnicodeSegmenter::IsControl(UnicodePoint ch, CharacterClass cl)
{
	switch (cl)
	{
	case CC_Cf:
		if (ch == 0x200C || ch == 0x200D)
			return false;
		/* fall through */
	case CC_Zl:
	case CC_Zp:
	case CC_Cc:
		return true;
	}
	return false;
}

/**
 * Check if character is in the "Extend" class as defined in UAX#29 X3.1.
 */
/*static*/ inline bool UnicodeSegmenter::IsExtend(UnicodePoint /*ch*/, CharacterClass cl)
{
	if (cl == CC_Me || cl == CC_Mn)
		return true;

	// FIXME: Evaluate whether to include Other_Grapheme_Extend in the
	// Grapheme_Extend check; this class contains various repetition
	// and stretch characters. Needs a special lookup table.

	return false;
}

/*static*/ BOOL UnicodeSegmenter::IsGraphemeClusterBoundary(UnicodePoint ch1, CharacterClass cl1, UnicodePoint ch2, CharacterClass cl2)
{
//	OP_ASSERT(!Unicode::IsSurrogate(ch1) && !Unicode::IsSurrogate(ch2) || !"Invalid call to UnicodeSegmenter::IsGraphemeClusterBoundary()");
	
	// UAX#29, X3.1
	// GB1: sot +
	// GB2: + eot
	// GB1 is disabled so that we do not report a boundary for the case where
	// start of text is followed by a combining mark (which is an error
	// situation).
	if (/*!ch1 || */!ch2)
		return TRUE;

	// GB3: CR × LF
	if (ch1 == 0x0D && ch2 == 0x0A)
		return FALSE;

	// GB4: (Control | CR | LF) +
	// GB5: + (Control | CR | LF)
	if (IsControl(ch1, cl1) || IsControl(ch2, cl2))
		return TRUE;

#ifdef MDF_OPENTYPE_SUPPORT
	if (is_indic(ch1) && is_indic(ch2))
	{
		return g_indic_scripts->IsGraphemeClusterBoundary(ch1, ch2);
	}
#endif // MDF_OPENTYPE_SUPPORT

	// Look up Hangul syllable type
	HangulSyllableType h1, h2;
	if ((h1 = Unicode::GetHangulSyllableType(ch1)) != Hangul_NA &&
		(h2 = Unicode::GetHangulSyllableType(ch2)) != Hangul_NA)
	{
		// GB6: L × (L | V | LV | LVT)
		if (h1 == Hangul_L)
			return FALSE; // Also catches L T (invalid)

		// GB7: (LV | V) × (V | T)
		if ((h1 == Hangul_LV || h1 == Hangul_V) && h2 == Hangul_V)
			return FALSE; // T final handled below

		// GB8: (LVT | T) × T
		if (h2 == Hangul_T)
			return FALSE;
	}

	// GB9: × Extend
	// GB9a: × SpacingMark  } These two are not supported, as they are only
	// GB9b: Prepend ×      } used in "extended grapheme clusters".
	// GB10: Any + Any
	return !IsExtend(ch2, cl2);
}


/* static */ BOOL UnicodeSegmenter::MightBeFirstHalfOfGraphemeCluster(UnicodePoint c)
{
	// UAX#29, X3.1
	// GB1: sot +
	// GB2: + eot
	// GB1 is disabled so that we do not report a boundary for the case where
	// start of text is followed by a combining mark (which is an error
	// situation).

	// GB3: CR × LF
	if (c == 0x0D)
		return TRUE;

	// GB4 and GB5 is implicitly handled. Will return FALSE at the end of the
	// function.
	
	// Since we are working with UTF-16, we must also avoid breaking
	// inside a surrogate pair
	if (Unicode::IsHighSurrogate(c))
		return TRUE;
	
	// Look up Hangul syllable type
	// Covers GB6, GB7 and GB8
	if (Unicode::GetHangulSyllableType(c) != Hangul_NA)
		return TRUE;	

	return FALSE;
}

void UnicodeSegmenter::Reset()
{
	m_prev = 0;	// Start of text
	m_prev_class = CC_Cc;
	m_last_found_boundary = false;
	m_wb_state = WBS_Start;
	m_sb_state = SBS_Start;
}

int UnicodeSegmenter::FindBoundary(const uni_char *text, int text_len)
{
	static const uni_char nul = 0;
	const uni_char *run = text, *end = text + text_len;

	// Check that we have a string
	if (!text_len)
	{
		return 0;
	}

	// If we are called with a NULL parameter, we add a NUL to the stream.
	if (!text)
	{
		OP_ASSERT(text_len == 1 || !"Invalid call to UnicodeSegmenter::FindBoundary()");
		text_len = 1;
		text = &nul;
	}

	// Skip the first character if we found a boundary last time, since we
	// then will have consumed the first character.
	if (m_last_found_boundary)
	{
		int consumed;
		Unicode::GetUnicodePoint(run, text_len, consumed);
		run += consumed;
		m_last_found_boundary = false;
	}

	// Drive the state machine
	BOOL cont = TRUE;

	// We only ever need one breaking type at once, keep the variable
	// outside the loop for efficiency reasons.
	union
	{
		WordBreakType word;
		SentenceBreakType sentence;
	} breaktype;

	while (run < end && cont)
	{
		int consumed;
		register UnicodePoint cur = Unicode::GetUnicodePoint(run, text_len, consumed);
		CharacterClass cur_class = Unicode::GetCharacterClass(cur);

		// Using the Ignore rule, we skip extending codepoints when
		// driving the state machines.
		if (IsGraphemeClusterBoundary(m_prev, m_prev_class, cur, cur_class))
		{
			switch (m_type)
			{
			case Grapheme:
				// The grapheme-boundary state machine terminates whenever
				// IsGraphemeClusterBoundary() returns TRUE.
				cont = FALSE;
				break;

			case Word:
				// Run the word-boundary state machine.
				breaktype.word = Unicode::GetWordBreakType(cur);
				if (breaktype.word == WB_CR)
				{
					m_wb_state = WBS_CR;
					cont = FALSE;
				}
				else if (breaktype.word == WB_LF && m_wb_state == WBS_CR)
				{
					// WB3: CR x LF
					m_wb_state = WBS_Start;
				}
				else if (breaktype.word == WB_CR || breaktype.word == WB_LF ||
				         breaktype.word == WB_Newline)
				{
					// WB3a: (Newline | CR | LF) +
					// WB3b: + (Newline | CR | LF)
					m_wb_state = WBS_Newline;
					cont = FALSE;
				}
				else if (breaktype.word == WB_Format ||
				         breaktype.word == WB_Extend)
				{
					// WB4: X (Extend | Format)* -> X
					cont = TRUE;
				}
				else if (breaktype.word == WB_ALetter)
				{
					if (m_wb_state != WBS_ALetter && // WB5: ALetter x ALetter
						m_wb_state != WBS_ALetter_MidLetter && // WB7: ALetter (MidLetter | MidNumLet) x ALetter
						m_wb_state != WBS_Numeric && // WB10: Numeric x ALetter
						m_wb_state != WBS_ExtendNumLet) // WB13b: ExtendNumLet x ALetter
						cont = FALSE; // WB14: Any + Any
					m_wb_state = WBS_ALetter;
				}
				else if ((breaktype.word == WB_MidLetter || breaktype.word == WB_MidNumLet)
				         && m_wb_state == WBS_ALetter)
				{
					// NB! Simplification: We only use one character
					// lookahead, so there could be a boundary here
					// according to the specification.
					// WB6: ALetter x (MidLetter | MidNumLet) ALetter
					m_wb_state = WBS_ALetter_MidLetter;
				}
				else if (breaktype.word == WB_Numeric)
				{
					if (m_wb_state != WBS_ALetter && // WB9: ALetter x Numeric
						m_wb_state != WBS_Numeric_MidNum && // WB11: Numeric (MidNum | MidNumLet) x Numeric
						m_wb_state != WBS_Numeric && // WB8: Numeric x Numeric
						m_wb_state != WBS_ExtendNumLet) // WB13b: ExtendNumLet x Numeric
						cont = FALSE; // WB14: Any + Any
					m_wb_state = WBS_Numeric;
				}
				else if ((breaktype.word == WB_MidNum || breaktype.word == WB_MidNumLet)
				         && m_wb_state == WBS_Numeric)
				{
					// NB! Simplification: We only use one character
					// lookahead, so there could be a boundary here
					// according to the specification.
					// WB12: Numeric x (MidNum | MidNumLet) Numeric
					m_wb_state = WBS_Numeric_MidNum;
				}
				else if (breaktype.word == WB_Katakana)
				{
					cont =
						m_wb_state == WBS_Katakana || // WB13: Katakana x Katakana
						m_wb_state == WBS_ExtendNumLet; // WB13b: ExtendNumLet x Katakana
					m_wb_state = WBS_Katakana;
				}
				else if (breaktype.word == WB_ExtendNumLet)
				{
					if (m_wb_state != WBS_ALetter && // WB13a: ALetter x ExtendNumLet
						m_wb_state != WBS_Numeric && // WB13a: Numeric x ExtendNumLet
						m_wb_state != WBS_Katakana && // WB13a: Katakana x ExtendNumLet
						m_wb_state != WBS_ExtendNumLet) // WB13a: ExtendNumLet x ExtendNumLet
						cont = FALSE; // WB14: Any + Any
					m_wb_state = WBS_ExtendNumLet;
				}
				else
				{
					cont = FALSE; // WB14: Any + Any
					m_wb_state = WBS_Start;
				}
				break;

			case Sentence:
				// Run the sentence-boundary state machine.
				breaktype.sentence = Unicode::GetSentenceBreakType(cur);
				// SB4: Sep | CR | LF +
				// SB11:(STerm | ATerm) Close* Sp* (Sep | CR | LF)? +
				cont =
					m_sb_state != SBS_Sep &&
					m_sb_state != SBS_STerm &&
					m_sb_state != SBS_STerm_Close_Sp &&
					m_sb_state != SBS_ATerm &&
					m_sb_state != SBS_ATerm_Close_Sp;
				if (breaktype.sentence == SB_CR)
				{
					m_sb_state = SBS_CR;
					cont = FALSE;
				}
				else if (breaktype.sentence == SB_LF && m_sb_state == SBS_CR)
				{
					// SB3: CR x LF
					m_sb_state = SBS_Start;
				}
				else if (breaktype.sentence == SB_Format ||
				         breaktype.sentence == SB_Extend)
				{
					// SB5: X (Extend | Format)* -> X
					cont = TRUE;
				}
				else if (breaktype.sentence == SB_Sep ||
				         breaktype.sentence == SB_CR ||
				         breaktype.sentence == SB_LF)
				{
					if (m_sb_state == SBS_STerm_Close_Sp)
					{
						// SB11: ( STerm | ATerm ) Close* Sp* Sep? ( Sep | CR | LF )? +
						cont = TRUE;
						m_sb_state = SBS_STerm_Close_Sp_Sep;
					}
					else
					{
						// SB4: see above
						// SB9: (STerm | ATerm) Close* x (Close | Sp | Sep)
						// SB10:(STerm | ATerm) Close* Sp* x (Sp | Sep | CR | LF)
						m_sb_state = SBS_Sep;
					}
				}
				else if (breaktype.sentence == SB_ATerm)
				{
					if (m_sb_state == SBS_Upper)
					{
						// SB7: Upper ATerm x ATerm (first part)
						m_sb_state = SBS_Upper_ATerm;
					}
					else
					{
						if (m_sb_state == SBS_ATerm ||
							m_sb_state == SBS_ATerm_Close_Sp ||
							m_sb_state == SBS_STerm ||
							m_sb_state == SBS_STerm_Close_Sp ||
							m_sb_state == SBS_STerm_Close_Sp_Sep)
						{
							// SB8a: (STerm | ATerm) Close* Sp* x (SContinue | STerm | ATerm)
							cont = TRUE;
						}
						m_sb_state = SBS_ATerm;
					}
				}
				else if (breaktype.sentence == SB_Close || breaktype.sentence == SB_Sp)
				{
					switch (m_sb_state)
					{
					case SBS_ATerm_Close_Sp_Misc:
					case SBS_ATerm_Close_Sp:
					case SBS_ATerm:
						cont = TRUE;
						// SB8: ATerm Close* Sp* x (!(OLetter | Upper | Lower | Sep | CR | LF | ATerm | STerm))* Lower
						m_sb_state = SBS_ATerm_Close_Sp;
						break;

					case SBS_STerm_Close_Sp:
					case SBS_STerm:
						cont = TRUE;
						// SB9: (STerm | ATerm) Close* x (Close | Sp | Sep)
						// SB10:(STerm | ATerm) Close* Sp* x (Sp | Sep | CR | LF)
						// SB11:(STerm | ATerm) Close* Sp* (Sep | CR | LF)? +
						m_sb_state = SBS_STerm_Close_Sp;
						break;

					default:
						m_sb_state = SBS_Start;
					}
				}
				else if (breaktype.sentence == SB_Lower)
				{
					if (m_sb_state == SBS_ATerm ||
						m_sb_state == SBS_ATerm_Close_Sp ||
						m_sb_state == SBS_ATerm_Close_Sp_Misc)
					{
						// SB8: ATerm Close* Sp* x (!(OLetter | Upper | Lower | Sep | CR | LF | ATerm | STerm))* Lower
						cont = TRUE;
					}
					m_sb_state = SBS_Start;
				}
				else if (breaktype.sentence == SB_Upper)
				{
					if (m_sb_state == SBS_ATerm)
					{
						// SB7: Upper ATerm x Upper
						cont = TRUE;
					}
					m_sb_state = SBS_Upper;
				}
				else if (breaktype.sentence == SB_Numeric)
				{
					if (m_sb_state == SBS_ATerm || // SB6: ATerm x Numeric
						m_sb_state == SBS_ATerm_Close_Sp ||
						m_sb_state == SBS_ATerm_Close_Sp_Misc)
					{
						// SB8: ATerm Close* Sp* x (!(OLetter | Upper | Lower | Sep | CR | LF | ATerm | STerm))* Lower
						cont = TRUE;
						m_sb_state = SBS_ATerm_Close_Sp_Misc;
					}
					else
					{
						m_sb_state = SBS_Start;
					}
				}
				else if (breaktype.sentence == SB_STerm)
				{
					if (m_sb_state == SBS_ATerm ||
						m_sb_state == SBS_ATerm_Close_Sp ||
						m_sb_state == SBS_STerm ||
						m_sb_state == SBS_STerm_Close_Sp)
					{
						// SB8a: (STerm | ATerm) Close* Sp* x (SContinue | STerm | ATerm)
						cont = TRUE;
					}

					m_sb_state = SBS_STerm;
				}
				else if (breaktype.sentence == SB_OLetter)
				{
					m_sb_state = SBS_Start;
				}
				else if (breaktype.sentence == SB_SContinue)
				{
					if (m_sb_state == SBS_ATerm ||
						m_sb_state == SBS_ATerm_Close_Sp ||
						m_sb_state == SBS_STerm ||
						m_sb_state == SBS_STerm_Close_Sp)
					{
						// SB8a: (STerm | ATerm) Close* Sp* x (SContinue | STerm | ATerm)
						cont = TRUE;
					}

					m_sb_state = SBS_Start;
				}
				else if (cur == 0)
				{
					// SB2: + eot
					m_sb_state = SBS_Start;
					cont = FALSE;
				}
				else //if (breaktype.sentence == SBS_Other)
				{
					OP_ASSERT(breaktype.sentence == SB_Other || !"Internal error");
					if (m_sb_state == SBS_ATerm ||
						m_sb_state == SBS_ATerm_Close_Sp ||
						m_sb_state == SBS_ATerm_Close_Sp_Misc)
					{
						// SB8: ATerm Close* Sp* x (!(OLetter | Upper | Lower | Sep | CR | LF | ATerm | STerm))* Lower
						cont = TRUE;
						m_sb_state = SBS_ATerm_Close_Sp_Misc;
					}
					m_sb_state = SBS_Start;
				}
				break;
			}
		}

		// Always return 0 the first call (*m_prev == NUL).
		if (cont && m_prev)
		{
			run += consumed;
		}
		else
		{
			cont = FALSE;
		}

		// Remember last character
		m_prev = cur;
		m_prev_class = cur_class;
	}

	// Skip the first character the next time, unless we consume all input.
	m_last_found_boundary = run < end;

	return run - text;
}

#endif
