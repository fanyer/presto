/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/unicode/unicode.h"
#include "modules/util/handy.h"

#ifndef _NO_GLOBALS_
// Do not call this directly, always call GetCharacterClass()
CharacterClass Unicode::GetCharacterClassInternalWithCache(UnicodePoint c)
{
	// Simple 2 entry cache.
	// It's common for this method to be called multiple times for the same 2 unicode points
	// Typically prev and next character during layout

	UnicodeModule::CC_CACHE_ENTRY *cache = g_opera->unicode_module.m_cc_cache;
	int &cache_old = g_opera->unicode_module.m_cc_cache_old;

	if (cache[!cache_old].c == c)
		return cache[!cache_old].cls;
	else if (cache[cache_old].c == c)
	{
		cache_old = !cache_old;
		return cache[!cache_old].cls;
	}

	CharacterClass cls = GetCharacterClassInternal(c);

	cache[cache_old].c = c;
	cache[cache_old].cls = cls;
	cache_old = !cache_old;
	return cls;
}
#endif // !_NO_GLOBALS_

// Do not call this directly, always call GetCharacterClass()
CharacterClass Unicode::GetCharacterClassInternal(UnicodePoint c)
{
	int plane = GetUnicodePlane(c);
	
	// Illegal/empty plane
	if (plane > 0xE || cls_planes[plane][0] == -1)
		return CC_Unknown;

	uni_char cp = static_cast<uni_char>(c);
 
	// Sentinel
	if (cp == 0xffff)
		return CC_Unknown;
	
	// Binary search the run-length compressed table.
	size_t high = cls_planes[plane][1], low = cls_planes[plane][0], middle;
	while (1)
	{
		middle = (high + low) >> 1;
		if (cls_chars[middle] <= cp)
		{
			if (cls_chars[middle + 1] > cp)
			{
				// Codepoint range found, return the data.
				return static_cast<CharacterClass>(cls_data[middle]);
			}
			else
			{
				low = middle;
			}
		}
		else
		{
			high = middle;
		}
	}
	/* NOT REACHED */
#ifdef _DEBUG
	OP_ASSERT(!"Quite impossible\n");
	return CC_Unknown;
#endif
}

BOOL Unicode::CheckPropertyValue(UnicodePoint c, UnicodeProperties prop)
{
	const UnicodePoint *table;
	int size;
	if (FindTableForProperty(prop, &table, &size))
	{
		size_t low = 0;
		size_t high = size - 1;
		size_t middle;
		do
		{
			middle = (high + low) >> 1;
			if (table[middle] <= c)
			{
				if (table[middle + 1] > c)
				{
					// The property tables are constructed in this way that each element with even index is the begin of codepoints' range 
					// for which the value is TRUE and each odd element defines codepoint that starts the range with FALSE value.
					return (middle % 2) ? FALSE : TRUE;
				}
				low = middle;
			}
			else
			{
				if (!high)
					return FALSE;
				high = middle;
			}
		}
		while (1);
	}
	else
		return FALSE;
}

BOOL Unicode::CheckDerivedPropertyValue(UnicodePoint c, UnicodeDerivedProperties prop)
{
	switch (prop)
	{
	case DERPROP_Default_Ignorable_Code_Point:
		return (CheckPropertyValue(c, PROP_Other_Default_Ignorable_Code_Point) 
				|| CheckPropertyValue(c, PROP_Variation_Selector)
				|| GetCharacterClass(c) == CC_Cf)
				&& !CheckPropertyValue(c, PROP_White_Space)
				&& (c < 0xFFF9 || c > 0xFFFB)
				&& (c < 0x0600 || c > 0x0603)
				&& c != 0x06DD
				&& c != 0x070F;
	default:
		return FALSE;
	}
}

UnicodeBlockType Unicode::GetUnicodeBlockType(UnicodePoint c)
{
	size_t low = 0;
	size_t high = ARRAY_SIZE(table_blocks_ranges) - 1;
	size_t middle;
	do
	{
		middle = (high + low) >> 1;
		if (table_blocks_ranges[middle] <= c)
		{
			if (table_blocks_ranges[middle + 1] > c)
				return UnicodeBlockType(table_blocks_values[middle]);
			low = middle;
		}
		else
			high = middle;
	}
	while (1);
}

UnicodeJoiningType Unicode::GetUnicodeJoiningType(UnicodePoint c)
{
	int low = 0;
	int high = ARRAY_SIZE(table_joiningtype_codepoints) - 1;
	int middle;
	while (low<=high)
	{
		middle = (high + low) >> 1;
		if (table_joiningtype_codepoints[middle] == c)
			return table_joiningtype_values[middle];
		if (table_joiningtype_codepoints[middle] < c)
			low = middle + 1;
		else
			high = middle - 1;
	}
	CharacterClass cc = GetCharacterClass(c);
	return (cc==CC_Mn || cc==CC_Me || cc==CC_Cf) ? JT_T : JT_U;
}

#ifdef USE_UNICODE_LINEBREAK

#ifndef _NO_GLOBALS_
// Do not call this directly, always call GetLineBreakClass()
LinebreakClass Unicode::GetLineBreakClassInternalWithCache(UnicodePoint c)
{
	// Simple 2 entry cache.
	// It's common for this method to be called multiple times for the same 2 unicode points
	// Typically prev and next character during layout
	UnicodeModule::LBC_CACHE_ENTRY *cache = g_opera->unicode_module.m_lbc_cache;
	int &cache_old = g_opera->unicode_module.m_lbc_cache_old;

	if (cache[!cache_old].c == c)
		return cache[!cache_old].lbc;
	else if (cache[cache_old].c == c)
	{
		cache_old = !cache_old;
		return cache[!cache_old].lbc;
	}

	LinebreakClass lbc = GetLineBreakClassInternal(c);

	cache[cache_old].c = c;
	cache[cache_old].lbc = lbc;
	cache_old = !cache_old;
	return lbc;
}
#endif // !_NO_GLOBALS_

// Do not call this directly, always call GetLineBreakClass()
LinebreakClass Unicode::GetLineBreakClassInternal(UnicodePoint c)
{
//	OP_ASSERT(!IsSurrogate(c));

	int plane = GetUnicodePlane(c);

	// Only handle BMP, SMP and SIP
	if (plane > 2)
	{
		return LB_XX;
	}

	uni_char cp = static_cast<uni_char>(c);

	// Sentinels
	if (cp == 0xFFFF)
	{
		return LB_XX;
	}

	// Binary search the run-length compressed table.
	size_t high = line_break_planes_new[plane][1], low = line_break_planes_new[plane][0];
	size_t middle;

	while (1)
	{
		middle = (high + low) >> 1;
		if (line_break_chars_new[middle] <= cp)
		{
			if (line_break_chars_new[middle + 1] > cp)
			{
				// Codepoint range found, return the data.
				LinebreakClass retval = static_cast<LinebreakClass>(line_break_data_new[middle]);
				if (retval == LB_H2)
				{
					// Table compaction for Hangul
					if (GetHangulSyllableType(cp) == Hangul_LVT)
						retval = LB_H3;
				}
				return retval;
			}
			else
			{
				low = middle;
			}
		}
		else
		{
			high = middle;
		}
	}
	/* NOT REACHED */
#ifdef _DEBUG
	OP_ASSERT(!"Quite impossible\n");
	return LB_XX;
#endif
}
#endif

#ifdef USE_UNICODE_HANGUL_SYLLABLE
HangulSyllableType Unicode::GetHangulSyllableType(UnicodePoint c)
{
//	OP_ASSERT(!IsSurrogate(c) || !"Invalid call to Unicode::GetHangulSyllableType()");
	
	if (c >= 0x1100 && c <= 0x11FF)
	{
		// Hangul Jamo
		if (c >= 0x11A8)
		{
			// Hangul_Syllable_Type=Trailing_Jamo
			return Hangul_T;
		}
		else if (c >= 0x1160)
		{
			// Hangul_Syllable_Type=Vowel_Jamo
			return Hangul_V;
		}
		else
		{
			// Hangul_Syllable_Type=Leading_Jamo
			return Hangul_L;
		}
	}
	else if (c >= 0xA960 && c <= 0xA97C)
	{
		return Hangul_L;
	}
	else if (c >= 0xAC00 && c <= 0xD7A3)
	{
		// Hangul Syllables
		if (((c - 0xAC00) % 0x1C) == 0)
		{
			// Hangul_Syllable_Type=LV_Syllable
			return Hangul_LV;
		}
		else
		{
			// Hangul_Syllable_Type=LVT_Syllable
			return Hangul_LVT;
		}
	}
	else if (c >= 0xD7B0 && c <= 0xD7FB)
	{
		if (c >= 0xD7CB)
		{
			// Hangul_Syllable_Type=Trailing_Jamo
			return Hangul_T;
		}
		else if (c <= 0xD7C6)
		{
			// Hangul_Syllable_Type=Vowel_Jamo
			return Hangul_V;
		}
	}

	// ..U+10FF, U+1120..U+A95F, U+A980..U+ABFF, U+D7A4..U+D7CF,
	// U+D7C7..U+D7CA, U+D7FC..
	return Hangul_NA;
}
#endif

#ifdef USE_UNICODE_SEGMENTATION
#include "modules/unicode/tables/wordbreak.inl"
WordBreakType Unicode::GetWordBreakType(UnicodePoint c)
{
//	OP_ASSERT(!IsSurrogate(c) || !"Unicode::GetWordBreakType()");
	
	if (c >= 65532)
	{
		// Treat SMB as ALetter.
		if (GetUnicodePlane(c) == 1)
			return WB_ALetter;

		// The upper parts of BMP and plane 2+ is unknown.
		return WB_Other;
	}
	else
	{
		// Binary search the run-length compressed table.
		size_t high = ARRAY_SIZE(word_break_chars), low = 0, middle;
		while (1)
		{
			middle = (high + low) >> 1;
			if (word_break_chars[middle] <= c)
			{
				if (word_break_chars[middle + 1] > c)
				{
					// Codepoint range found, return the data.
					return static_cast<WordBreakType>(word_break_data[middle]);
				}
				else
				{
					low = middle;
				}
			}
			else
			{
				high = middle;
			}
		}
		/* NOT REACHED */
	}
	
#ifdef _DEBUG
	OP_ASSERT(!"Quite impossible\n");
	return WB_Other;
#endif
}

#include "modules/unicode/tables/sentencebreak.inl"
SentenceBreakType Unicode::GetSentenceBreakType(UnicodePoint c)
{
	if (c >= 65532)
	{				
		// Treat SMB as OLetter.
		if (GetUnicodePlane(c) == 1)
			return SB_OLetter;

		// Only handle BMP
		return SB_Other;
	}
	else
	{
		// Binary search the run-length compressed table.
		size_t high = ARRAY_SIZE(sentence_break_chars), low = 0, middle;
		while (1)
		{
			middle = (high + low) >> 1;
			if (sentence_break_chars[middle] <= c)
			{
				if (sentence_break_chars[middle + 1] > c)
				{
					// Codepoint range found, return the data.
					return static_cast<SentenceBreakType>(sentence_break_data[middle]);
				}
				else
				{
					low = middle;
				}
			}
			else
			{
				high = middle;
			}
		}
		/* NOT REACHED */
	}
	
#ifdef _DEBUG
	OP_ASSERT(!"Quite impossible\n");
	return SB_Other;
#endif
}
#endif

#ifdef USE_UNICODE_SCRIPT
#include "modules/unicode/tables/scripts.inl"
ScriptType Unicode::GetScriptType(UnicodePoint c)
{
	if (c >= 65501)
	{				
		// Only handle BMP
		return SC_Unknown;
	}
	else
	{
		// Binary search the run-length compressed table.
		size_t high = ARRAY_SIZE(script_chars), low = 0, middle;
		while (1)
		{
			middle = (high + low) >> 1;
			if (script_chars[middle] <= c)
			{
				if (script_chars[middle + 1] > c)
				{
					// Codepoint range found, return the data.
					return static_cast<ScriptType>(script_data[middle]);
				}
				else
				{
					low = middle;
				}
			}
			else
			{
				high = middle;
			}
		}
		/* NOT REACHED */
	}
#ifdef _DEBUG
	OP_ASSERT(!"Quite impossible\n");
	return SC_Unknown;
#endif
}
#endif // USE_UNICODE_SCRIPT
