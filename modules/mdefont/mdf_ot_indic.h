/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** wonko
*/

#ifndef MDF_OT_INDIC_H
#define MDF_OT_INDIC_H

#ifdef MDEFONT_MODULE
#ifdef MDF_OPENTYPE_SUPPORT

// the fifferent indic scripts
// #define IS_DEVANAGARI(ch)   ((ch) >= 0x0900 && (ch) <= 0x097f)
// #define IS_BENGALI(ch)      ((ch) >= 0x0980 && (ch) <= 0x09ff)
// #define IF_GURMUKHI(ch)     ((ch) >= 0x0a00 && (ch) <= 0x0a7f)
// #define IS_GUJARATI(ch)     ((ch) >= 0x0a80 && (ch) <= 0x0aff)
// #define IS_ORIYA(ch)        ((ch) >= 0x0b00 && (ch) <= 0x0b7f)
// #define IS_TAMIL(ch)        ((ch) >= 0x0b80 && (ch) <= 0x0bff)
// #define IS_TELUGU(ch)       ((ch) >= 0x0c00 && (ch) <= 0x0c7f)
// #define IS_KANNADA(ch)      ((ch) >= 0x0c80 && (ch) <= 0x0cff)
// #define IS_MALAYALAM(ch)    ((ch) >= 0x0d00 && (ch) <= 0x0d7f)
// #define IS_SINHALA(ch)      ((ch) >= 0x0d80 && (ch) <= 0x0dff)
// #define IS_LIMBU(ch)        ((ch) >= 0x1900 && (ch) <= 0x194f)
// #define IS_SYLOTI_NAGRI(ch) ((ch) >= 0xa800 && (ch) <= 0xa82f)

inline
BOOL is_indic(const uni_char ch)
{
	return
		// devanagari, bengali, gurmukhi, gujarati, oriya, tamil, telugu, kannada, malayalam, sinhala
		(ch >= 0x0900 && ch <= 0x0dff) ||
		// limbu
		(ch >= 0x1900 && ch <= 0x194f) ||
		// syloti nagari
		(ch >= 0xa800 && ch <= 0xa82f);
}

/*
  utility functionality to render indic text. libgogi creates an
  instance of IndicGlyphClass, that in turn hold IndicScriptRange
  instances for different indic scripts. to measure and render indic
  text properly the following steps need be performed:

  * find syllable boundaries (indic_find_syllable)
  * detect the base character for each syllable (indic_find_base)
  * reorder the syllables (indic_reorder_syllable)
  * apply font-specific shaping (e.g. opentype gsub, or whatever the
    font has - some glyphs needed to correctly display indic text are
    not in the unicode range)

  the OT_handler class uses this functionality.
 */

/**
   a class used to keep track of various features in different indic scripts.
 */
struct IndicScriptRange
{
	enum Flags 
	{
		CONSONANT      = 0x01,
		INDEP_VOWEL    = 0x02,
		DEP_VOWEL      = 0x04,
		DIGIT          = 0x08,
		// break - these are "secondary" flags
		POST_BASE      = 0x10,
		BELOW_BASE     = 0x20,
		REORDRANT      = 0x40,
	};

	struct Ranges
	{
		struct RangeInfo
		{
			BOOL InRange(const uni_char ch) const { return ch >= start && ch <= end; }
			uni_char start, end;
			Flags flag;
		};
		Flags GetFlags(const uni_char ch) const
		{
			Flags flags = (Flags)0;
			for (int i = 0; i < count; ++i)
				if (ranges[i].InRange(ch))
					flags = (Flags)(flags | ranges[i].flag);
			return flags;

		}
		RangeInfo* ranges;
		UINT8 count;
	};

	IndicScriptRange(const uni_char ra, const uni_char nukta, const uni_char virama,
					 const Ranges* const ranges,
					 const uni_char tab_start, const uni_char tab_end);

	Flags GetFlags(const uni_char ch); ///< fetches flags for script
	BOOL InRange(const uni_char ch); ///< true if ch is in range of the script

	const uni_char m_ra, m_nukta, m_virama;
	const Ranges* const m_ranges;
	const uni_char m_tab_start, m_tab_end;

};

struct IndicGlyphClass
{
	enum Script
	{
		DEVANAGARI = 0,
// 		BENGALI,
// 		GURMUKHI,
 		GUJARATI,
// 		ORIYA,
// 		TAMIL,
// 		TELUGU,
		KANNADA,
// 		MALAYALAM,
// 		LIMBU,
// 		SYLOTI_NAGRI,
		UNKNOWN // last, used to get number of scripts
	};

	IndicGlyphClass();
	~IndicGlyphClass();
	OP_STATUS Initialize();
	BOOL IsIndic(const uni_char ch);
	Script GetScript(const uni_char ch);
	const IndicScriptRange& GetScriptRange(const Script s);
	IndicScriptRange::Flags GetFlags(const uni_char ch,  Script s = (Script)UNKNOWN);
	BOOL IsGraphemeClusterBoundary(const uni_char ch1, const uni_char ch2);

	IndicScriptRange* m_scripts[UNKNOWN];
	IndicScriptRange::Ranges m_script_ranges[UNKNOWN];
};

/**
   reorders a syllable
   @param script the script used in the syllable
   @param sstr the syllable
   @param slen the length of the syllable
   @param base (in-out) offset to the base glyph - updated if base glyph is moved durnig reordering
   @param hasReph (out) set to TRUE if the syllable contains a reph, FALSE otherwise
 */
void indic_reorder_syllable(IndicGlyphClass::Script script,
							uni_char* sstr, const UINT32 slen, UINT32& base, BOOL& hasReph);
/**
   tries to find the base glyph in a syllable
   @param script the script used in the syllable
   @param sstr the syllable
   @param slen the length of the syllable
   @param base (out) the offset to the base character
   @return TRUE if a base character was found, FALSE otherwise
 */
BOOL indic_find_base(IndicGlyphClass::Script script,
					 const uni_char* sstr, const UINT32 slen, UINT32& base);
/**
   tries to find the first syllable in the text.
   @param script the script used in the string
   @param str the text - must point to start of a syllable
   @param len the length of the text
   @param slen (out) the length of the syllable
   @return TRUE if a syllable was detected, FALSE otherwise
 */
BOOL indic_find_syllable(IndicGlyphClass::Script script,
						 const uni_char* str, const UINT32 len, UINT32& slen);

#endif // MDF_OPENTYPE_SUPPORT
#endif // MDEFONT_MODULE

#endif // MDF_OT_INDIC_H
