/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef MODULES_UNICODE_UNICODE_H
#define MODULES_UNICODE_UNICODE_H

#ifndef UNICODE_BUILD_TABLES
# include "modules/debug/debug.h" // OP_ASSERT
#endif

/**
 * @file unicode.h
 * Various utilities using information from the Unicode character data base.
 */

#ifndef PURE_FUNCTION
# define PURE_FUNCTION
#endif

#ifdef USE_UNICODE_LINEBREAK
/**
 * Unicode linebreaking classes, from the Unicode standard.
 */
enum LinebreakClass
{
	LB_XX,		/**< Unknown */

	LB_BK,		/**< Mandatory Break */
	LB_CR,		/**< Carriage Return */
	LB_LF,		/**< Line Feed */
	LB_CM,		/**< Combining Marks */
	LB_NL,		/**< Next Line */
	LB_SG,		/**< Surrogate */
	LB_WJ,		/**< Word Joiner */
	LB_ZW,		/**< Zero Width Space */
	LB_GL,		/**< Non-breaking ("Glue") */
	LB_SP,		/**< Space */

	LB_B2,		/**< Break Opportunity Before and After */
	LB_BA,		/**< Break After */
	LB_BB,		/**< Break Before */
	LB_HY,		/**< Hyphen */
	LB_CB,		/**< Contingent Break Opportunity */

	LB_CL,		/**< Close Punctuation */
	LB_CP,		/**< Close Parenthesis */
	LB_EX,		/**< Exclamation/Interrogation */
	LB_IN,		/**< Inseparable */
	LB_NS,		/**< Nonstarter */
	LB_OP,		/**< Open Punctuation */
	LB_QU,		/**< Ambiguous Quotation */

	LB_IS,		/**< Numeric Separator */
	LB_NU,		/**< Numeric */
	LB_PO,		/**< Postfix Numeric */
	LB_PR,		/**< Prefix Numeric */
	LB_SY,		/**< Symbols Allowing Break After */

	LB_AI,		/**< Ambiguous (Alphabetic or Ideographic) */
	LB_AL,		/**< Alphabetic */
	LB_CJ,		/**< Conditional Japanese Starter */
	LB_H2,		/**< Hangul LV Syllable */
	LB_H3,		/**< Hangul LVT Syllable */
	LB_HL,		/**< Hebrew Letter */
	LB_ID,		/**< Ideographic */
	LB_JL,		/**< Hangul L Jamo */
	LB_JV,		/**< Hangul V Jamo*/
	LB_JT,		/**< Hangul T Jamo */
	LB_SA,		/**< Complex Context Dependent (South East Asian) */

	LB_Last = LB_SA
};

/**
 * Return values from the Unicode::IsLinebreakOpportunity() method.
 */
enum LinebreakOpportunity
{
	LB_NO = 0,		/**< No line-break opportunity exists between the characters. */
	LB_YES = 1,		/**< A line-break opportunity exists between the characters. */

	LB_MAYBE = 2	/**< Line-breaking properties are unknown, or an error occurred. */
};

# ifndef UNICODE_BUILD_TABLES
#  include "modules/unicode/tables/linebreak.inl"
# endif
#endif // USE_UNICODE_LINEBREAK

/**
 * The character classes, from the Unicode standard.
 */
enum CharacterClass
{
	CC_Unknown = 0,	/**< Unknown character */
	CC_Cc,			/**< Other, Control */
	CC_Cf,			/**< Other, Format */
	CC_Co,			/**< Other, Private Use */
	CC_Cs,			/**< Other, Surrogate */

	CC_Ll,			/**< Letter, Lowercase */
	CC_Lm,			/**< Letter, Modifier */
	CC_Lo,			/**< Letter, Other */
	CC_Lt,			/**< Letter, Titlecase */
	CC_Lu,			/**< Letter, Uppercase */

	CC_Mc,			/**< Mark, Spacing Combining */
	CC_Me,			/**< Mark, Enclosing */
	CC_Mn,			/**< Mark, Nonspacing */

	CC_Nd,			/**< Number, Decimal Digit */
	CC_Nl,			/**< Number, Letter */
	CC_No,			/**< Number, Other */

	CC_Pe,			/**< Punctuation, Close */
	CC_Po,			/**< Punctuation, Other */
	CC_Ps,			/**< Punctuation, Open */
	CC_Pc,			/**< Punctuation, Connector */
	CC_Pd,			/**< Punctuation, Dash */
	CC_Pf,			/**< Punctuation, Final quote (may behave like Ps or Pe depending on usage) */
	CC_Pi,			/**< Punctuation, Initial quote (may behave like Ps or Pe depending on usage) */

	CC_Sc,			/**< Symbol, Currency */
	CC_Sk,			/**< Symbol, Modifier */
	CC_Sm,			/**< Symbol, Math */
	CC_So,			/**< Symbol, Other */

	CC_Zl,			/**< Separator, Line */
	CC_Zp,			/**< Separator, Paragraph */
	CC_Zs			/**< Separator, Space */
};

#ifndef UNICODE_BUILD_TABLES
# include "modules/unicode/tables/charclass.inl"
#endif

#ifdef SUPPORT_TEXT_DIRECTION

/**
 * The directional categories, from the Unicode standard.
 */
enum BidiCategory
{
	BIDI_UNDEFINED = 0,		/**< No directional class defined. */
	BIDI_L = 1,				/**< Strong, Left-to-Right */
	BIDI_R = 2,				/**< Strong, Right-to-Left */
	BIDI_AL = 3,			/**< Strong, Right-to-Left Arabic */
	BIDI_LRE = 4,			/**< Strong, Left-to-Right Embedding */
	BIDI_LRO = 5,			/**< Strong, Left-to-Right Override*/
	BIDI_RLE = 6,			/**< Strong, Right-to-Left Embedding */
	BIDI_RLO = 7,			/**< Strong, Right-to-Left Override*/
	BIDI_PDF = 8,			/**< Weak, Pop Directional Format*/
	BIDI_EN = 9,			/**< Weak, European Number */
	BIDI_ES = 10,			/**< Weak, European Number Separator */
	BIDI_ET = 11,			/**< Weak, European Number Terminator */
	BIDI_AN = 12,			/**< Weak, Arabic Number */
	BIDI_CS = 13,			/**< Weak, Common Number Separator */
	BIDI_NSM = 14,			/**< Weak, Nonspacing Mark */
	BIDI_BN = 15,			/**< Weak, Boundary Neutral */
	BIDI_B = 16,			/**< Neutral, Paragraph Separator */
	BIDI_S = 17,			/**< Neutral, Segment Separator */
	BIDI_WS = 18,			/**< Neutral, Whitespace*/
	BIDI_ON = 19			/**< Neutral, Other Neutrals */
};
#endif

#ifdef USE_UNICODE_HANGUL_SYLLABLE
enum HangulSyllableType
{
	Hangul_L,				/**< Leading consonant (choseong) */
	Hangul_V,				/**< Vowel (jungseong) */
	Hangul_T,				/**< Trailing consonant (jongseong) */
	Hangul_LV,				/**< Block of L + V */
	Hangul_LVT,				/**< Block of L + V + T */
	Hangul_NA				/**< Non-Hangul character */
};
#endif

#ifdef USE_UNICODE_SEGMENTATION
enum WordBreakType
{
	WB_Other,		/**< Other characters */
	WB_CR,			/**< Carriage return (U+000D) */
	WB_LF,			/**< Linefeed (U+000A) */
	WB_Newline,		/**< Paragraph separators */
	WB_Extend,		/**< Grapheme extenders and spacing marks */
	WB_Format,		/**< Formatting characters (most Cf) */
	WB_Katakana,	/**< Katakana and related characters */
	WB_ALetter,		/**< Alphabetic characters, not ideographs or katakana */
	WB_MidNumLet,	/**< Symbols appearing in the middle of words or numbers */
	WB_MidLetter,	/**< Symbols appearing in the middle of words */
	WB_MidNum,		/**< Symbols appearing in the middle of numbers */
	WB_Numeric,		/**< Numeric symbols */
	WB_ExtendNumLet	/**< Connector punctuation class */
};

enum SentenceBreakType
{
	SB_Other,		/**< Other characters */
	SB_CR,			/**< Carriage return (U+000D) */
	SB_LF,			/**< Linefeed (U+000A) */
	SB_Extend,		/**< Grapheme extenders and spacing marks */
	SB_Sep,			/**< Paragraph separators */
	SB_Format,		/**< Formatting characters (most Cf) */
	SB_Sp,			/**< Breaking whitespace */
	SB_Lower,		/**< Lowercase letters */
	SB_Upper,		/**< Upper- and titlecase letters */
	SB_OLetter,		/**< Other letters */
	SB_Numeric,		/**< Numeric symbols */
	SB_ATerm,		/**< Sentence terminators (full stop, etc.) */
	SB_SContinue,	/**< Sentence continuators (comma, etc.) */
	SB_STerm,		/**< STerm characters */
	SB_Close		/**< Opening and closing punctuation, quotation marks */
};
#endif

#ifdef USE_UNICODE_SCRIPT
enum ScriptType
{
	SC_Unknown,
	SC_Arabic,
	SC_Bengali,
	SC_Bopomofo,
	SC_Cherokee,
	SC_Cyrillic,
	SC_Devanagari,
	SC_Greek,
	SC_Gujarati,
	SC_Gurmukhi,
	SC_Han,
	SC_Hangul,
	SC_Hebrew,
	SC_Hiragana,
	SC_Kannada,
	SC_Katakana,
	SC_Lao,
	SC_Latin,
	SC_Malayalam,
	SC_Myanmar,
	SC_Oriya,
	SC_Sinhala,
	SC_Tamil,
	SC_Telugu,
	SC_Thai,
	SC_Tibetan,
	SC_NumberOfScripts
};
#endif

#ifndef UNICODE_BUILD_TABLES // called from script

/** Representation of a unicode point */
typedef UINT32 UnicodePoint;

/** Maximum legal value of a UnicodePoint */
#define MAX_UNICODE_POINT 0x10FFFF

/* Forward declarations. */
struct UniBuffer;

#include "modules/unicode/tables/proplist.inl"
#include "modules/unicode/tables/blocks.inl"
#include "modules/unicode/tables/joiningtype.inl"

enum UnicodeDerivedProperties 
{
	/**
	 * Derived Property: Default_Ignorable_Code_Point
	 * + Cf (Format characters)
	 * + Variation_Selector
	 * - White_Space
	 * - FFF9..FFFB (Annotation Characters)
	 * - 0600..0603, 06DD, 070F (exceptional Cf characters that should be visible)
	 */
	DERPROP_Default_Ignorable_Code_Point
};


/**
 * Unicode support functions.
 */
class Unicode
{
public:
#ifdef SUPPORT_UNICODE_NORMALIZE
	// unicode_normalize.cpp

	/**
	 * Normalize a Unicode string. If compose is TRUE, do compositing, if
	 * compat is TRUE, decompose compatibility characters (such as U+00BD)
	 * before composing.
	 *
	 * Only available if the UNICODE_NORMALIZE feature is on.
	 *
	 * - Normalization form D  = compat=FALSE, compose=FALSE
	 * - Normalization form KD = compat=TRUE,  compose=FALSE
	 * - Normalization form KC = compat=TRUE,  compose=TRUE
	 * - Normalization form C  = compat=FALSE, compose=TRUE
	 *
	 * @param x       The string to be composed
	 * @param len     Length of the string (or -1 if x is nul-terminated)
	 * @param compose TRUE if compositing should be done, FALSE otherwise
	 * @param compat  TRUE if compat decompositing should be done, FALSE otherwise
	 * @return The new string, or 0 if an OOM error occured.
	 *         The return value should be freed using OP_DELETEA()
	 *
	 */
	static uni_char *Normalize(const uni_char * x, int len, BOOL compose,
							   BOOL compat);

	/**
	 * Maps all fullwidth and halfwidth characters (those defined with
	 * Decomposition Types \<wide\> and \<narrow\>) to their decompositions.
	 * \<narrow\>/\<wide\> decomposition doesn't change the length of string so
	 * the destination buffer must be at least of the same size as src buffer.
	 */
	static void DecomposeNarrowWide(const uni_char *src, int source_len, uni_char *dst);
#endif

	/** Get the plane a UnicodePoint reside in */
	static inline int GetUnicodePlane(UnicodePoint c)
	{
		return c >> 16;	// Handle illegal planes here?
	}
	
	/**
	 * Get the first unicode point from a string.
	 *
	 * @param str The source string.
	 * @param len Length of the source string.
	 * @param[out] consumed Number of uni_chars consumed by this operation.
	 */
	static inline UnicodePoint GetUnicodePoint(const uni_char* str, int len, int &consumed)
	{
		UnicodePoint cp;

		OP_ASSERT(str && len);
		if (IsHighSurrogate(*str) && len >= 2 && IsLowSurrogate(*(str + 1)))
		{
			cp = DecodeSurrogate(*str, *(str + 1));
			consumed = 2;
		}
		else
		{
			cp = *str;
			consumed = 1;
		}

		return cp;
	}

	/**
	 * Writes a Unicode code-point to a UTF-16 encoded string. No null termination is output.
	 *
	 * @param str The destination string.
	 * @param cp Unicode point to write.
	 * @return Number of characters written to the destination string (1 or 2).
	 */
	static inline int WriteUnicodePoint(uni_char* str, UnicodePoint cp)
	{
		OP_ASSERT(str);
		if (cp > 0xffff)
		{
			MakeSurrogate(cp, str[0], str[1]);
			return 2;
		}
		else
		{
			str[0] = cp;
			return 1;
		}
	}

	/**
	 * Get the first unicode point from a string.
	 *
	 * @param str The source string.
	 * @param len Length of the source string.
	 */
	static inline UnicodePoint GetUnicodePoint(const uni_char* str, int len)
	{
		UnicodePoint cp;

		OP_ASSERT(str && len);
		if (IsHighSurrogate(*str) && len >= 2 && IsLowSurrogate(*(str + 1)))
		{
			cp = DecodeSurrogate(*str, *(str + 1));
		}
		else
		{
			cp = *str;
		}

		return cp;
	}

	// unicode_case.cpp

	/**
	 * Lowercase a whole string. Stops when a null-character is encountered.
	 *
	 * @param data  The string to convert.
	 * @param in_place If TRUE no new storage is allocated, the
	 *                 lowercase transform is done in-place.
	 *
	 * @return If in_place is FALSE, a string allocated using uni_strdup
	 *         or NULL if out of memory.
	 *         Otherwise the string passed as an argument.
	 */
	static uni_char *Lower(uni_char * data, BOOL in_place = FALSE);

	/**
	 * Uppercase a whole string. Stops when a null-character is encountered.
	 *
	 * @param data  The string to convert.
	 * @param in_place If TRUE no new storage is allocated, the
	 *                 uppercase transform is done in-place.
	 *
	 * @return If in_place is FALSE, a string allocated using uni_strdup
	 *         or NULL if out of memory.
	 *         Otherwise the string passed as an argument.
	 */
	static uni_char *Upper(uni_char * data, BOOL in_place = FALSE);

	/**
	 * Convert a single character to its lower-case form.
	 */
	static inline UnicodePoint ToLower(UnicodePoint c)
	{
		// Hard-code behaviour for ASCII characters
		if (c < 128)
			return (c >= 'A' && c <= 'Z') ? c | 0x20 : c;

		UnicodePoint res(c);
		ToLowerInternal(&res);
		return res;
	}

	/**
	 * Convert a single character to its upper-case form.
	 */
	static inline UnicodePoint ToUpper(UnicodePoint c)
	{
		// Hard-code behaviour for ASCII characters
		if (c < 128)
			return (c >= 'a' && c <= 'z') ? c & ~0x20 : c;

		UnicodePoint res(c);
		ToUpperInternal(&res);
		return res;
	}

	/** 
	 * Performs full case folding of every character in the specified
	 * UTF-16 encoded unicode string. Case folding can be an important part
	 * of normalizing text for comparison purposes. The main difference between
	 * simple case folding and full case folding is that the first doesn't change
	 * the string length while the latter can. Note that where they can be supported,
	 * the full case foldings are superior: for example, they allow "MASSE" and "Ma<U+00DF>e" to match.
	 *
	 * For more information about case folding please read section 3.13 "Default Case Algorithms" in The Unicode Standard.
	 *
	 * @param s String to perform full case foding on
	 * @return The new string, or 0 if an OOM error occured.
	 *         The return value should be freed using OP_DELETEA()
	 */
	static uni_char *ToCaseFoldFull(const uni_char *s);

	/** 
	 * Performs simple case folding on the specified unicode point.
	 * Simple case folding maps single codepoint to itself or to another single codepoint.
	 *
	 * For more information about case folding please read section 3.13 "Default Case Algorithms" in The Unicode Standard.
	 *
	 * @param c Codepoint to perform simple case foding on
	 * @return The mapping for given codepoint
	 */
	static UnicodePoint ToCaseFoldSimple(UnicodePoint c);

	/** 
	 * Performs full case folding for the specified single Unicode codepoint. 
	 * The output buffer must be at least of 3 UnicodePoints long. No nul-termination is output.
	 *
	 * For more information about case folding please read section 3.13 "Default Case Algorithms" in The Unicode Standard.
	 *
	 * @param c The Unicode codepoint to perform full case folding on.
	 * @param output The resulting sequence of Unicode codepoints.
	 * @return Number of Unicode codepoints written to the output buffer.
	 */
	static int ToCaseFoldFull(UnicodePoint c, UnicodePoint *output);

	// unicode_properties.cpp

#ifdef USE_UNICODE_LINEBREAK
	/**
	 * Return the linebreak class for the specified character.
	 */
	static LinebreakClass GetLineBreakClass(UnicodePoint c) PURE_FUNCTION;

	/**
	 * Check if there is a linebreaking opportunity between two
	 * characters. Before using this function, you must perform
	 * whitespace compacting, by removing all instances of whitespace
	 * characters (U+0020 SPACE, and whatever characters are considered
	 * whitespace in the current context), and find grapheme clusters
	 * so that the two code units tested both are starters.
	 *
	 * This function is UNABLE to find line break opportunities in
	 * "Complex Context Dependent" (South East Asian) text. See the
	 * function below for more details.
	 *
	 * Use the UnicodeSegmenter::IsGraphemeClusterBoundary() or
	 * UnicodeSegmenter::FindBoundary() APIs to detect the grapheme
	 * cluster boundaries.
	 *
	 * @param first
	 *   The first code unit to test. May not be a space (class SP
	 *   (LB_SP)) or combining mark (class CM (LB_CM)). Use the value
	 *   0 to indicate start-of-text.
	 * @param second
	 *   The second code unit to test. May not be a space (class SP
	 *   (LB_SP)) or combining mark (class CM (LB_CM)).
	 *   Use the value 0 to indicate end-of-text.
	 * @param space_separated
	 *   TRUE if there is whitespace between first and second.
	 * @return
	 *   LB_YES if there is a break opportunity. <br>
	 *   LB_NO if there is no break opportunity. <br>
	 *   LB_MAYBE if first or second was of class SP or CM.
	 */
	static LinebreakOpportunity IsLinebreakOpportunity(UnicodePoint first, UnicodePoint second, BOOL space_separated) PURE_FUNCTION;

	/**
	 * Check if there is a linebreaking opportunity between the 'prev' code
	 * point and the first code point in the given text buffer.
	 *
	 * Before using this function, you must perform whitespace compacting
	 * between 'prev' and the start of 'buf', by removing all instances of
	 * whitespace characters (U+0020 SPACE, and whatever characters are
	 * considered whitespace in the current context), and find grapheme
	 * clusters so that the two code units tested both are starters.
	 *
	 * When analyzing "Complex Context Dependent" (South East Asian) text,
	 * this function will have to look further into the given buffer to
	 * determine whether the start of the buffer constitutes a line break
	 * opportunity.
	 *
	 * Use the UnicodeSegmenter::IsGraphemeClusterBoundary() or
	 * UnicodeSegmenter::FindBoundary() APIs to detect the grapheme
	 * cluster boundaries.
	 *
	 * @param prev
	 *   The code point preceding the current position in the text.
	 *   May not be a space (class SP (LB_SP)) or combining mark (class CM
	 *   (LB_CM)). Use the value 0 to indicate start-of-text.
	 * @param buf
	 *   The text buffer starting at the current position. The initial code
	 *   point in this buffer will used together with 'prev' to determine
	 *   whether there is a line break opportunity immediately preceding
	 *   this buffer (Exception: in case of LB_SA code points, analysis
	 *   might look further into 'buf').
	 *   The code point at the start of this buffer, may not be a space
	 *   (class SP (LB_SP)) or combining mark (class CM (LB_CM)). Use NULL
	 *   or the empty string to indicate end-of-text.
	 * @param len
	 *   Length of text in above buffer.
	 * @param space_separated
	 *   TRUE if there is whitespace between prev and buf.
	 * @return
	 *   LB_YES if there is a break opportunity. <br>
	 *   LB_NO if there is no break opportunity. <br>
	 *   LB_MAYBE if prev or buf[0] is of class SP or CM.
	 */
	static LinebreakOpportunity IsLinebreakOpportunity(UnicodePoint prev, const uni_char *buf, int len, BOOL space_separated) PURE_FUNCTION;
#endif

#ifdef USE_UNICODE_SCRIPT
	/**
	 * Return the script property for the specified character.
	 */
	static ScriptType GetScriptType(UnicodePoint c) PURE_FUNCTION;
#endif

	/**
	 * Returns TRUE if the argument is a CSS punctuation character (full
	 * stop, ",", "?" etc). Equivalent to checking if the class is one of
	 * Ps, Pe, Pi, Pf or Po.
	 */
	static inline BOOL IsCSSPunctuation(UnicodePoint c)
	{
		switch (GetCharacterClass(c))
		{
		case CC_Pe:
		case CC_Po:
		case CC_Ps:
		case CC_Pf:
		case CC_Pi:
			return TRUE;
		}
		return FALSE;
	}

	/**
	 * Returns TRUE if the character is a uppercase character.
	 * Equivalent to checking if its character class is Lu.
	 */
	static inline BOOL IsUpper(UnicodePoint x)
	{
		return GetCharacterClass(x) == CC_Lu;
	}

	/**
	 * Returns TRUE if the character is a lowercase character.
	 * Equivalent to checking if its character class is Ll.
	 */
	static inline BOOL IsLower(UnicodePoint x)
	{
		return GetCharacterClass(x) == CC_Ll;
	}

	/**
	 * Returns TRUE if the character is a control character.
	 */
	static inline BOOL IsCntrl(UnicodePoint x)
	{
		return (x < 32) || (GetCharacterClass(x) == CC_Cc);
	}

	/**
	 * Returns TRUE if the character is a space character.
	 * Equivalent to checking if GetCharacterClass(x) is CC_Z*
	 * (since we handle the control-area spaces as Zs in our table).
	 */
	static inline BOOL IsSpace(UnicodePoint x)
	{
		// This code is much smaller than it looks to be since the CC_* below are in sequence.
		switch (GetCharacterClass(x))
		{
		case CC_Zl:
		case CC_Zp:
		case CC_Zs:
			return TRUE;
		default:
			return FALSE;
		}
	}

	/**
	 * Returns TRUE if the character is a alphabetical character.
	 * Unicode classes Ll, Lm, Lo, Lt, Lu and Nl.
	 */
	static inline BOOL IsAlpha(UnicodePoint x)
	{
		// This code is smaller than it looks to be since the CC_* below are in sequence.
		switch (GetCharacterClass(x))
		{
		case CC_Ll:
		case CC_Lm:
		case CC_Lo:
		case CC_Lt:
		case CC_Lu:
		case CC_Nl:
			return TRUE;
		}
		return FALSE;
	}

	/**
	 * Returns TRUE if the character is a digit character.
	 *
	 * Note that the current code is not strictly correct as far as
	 * Unicode is concerned, it only returns TRUE for European (ASCII)
	 * digits.
	 *
	 * To check for Unicode digits, check if GetCharacterClass(x) is
	 * CC_Nd, CC_Nl or CC_No or use Unicode::IsUniDigit().
	 */
	static inline BOOL IsDigit(UnicodePoint x)
	{
		return x <= '9' && x >= '0';
	}

	/**
	 * Check if 'x' is a Unicode digit (i.e, character class is one of
	 * CC_Nd, CC_Nl or CC_No).
	 */
	static inline BOOL IsUniDigit(UnicodePoint x)
	{
		switch (GetCharacterClass(x))
		{
		case CC_Nd:
		case CC_Nl:
		case CC_No:
			return TRUE;
		}
		return FALSE;
	}


	/**
	 * Return TRUE if the character is either alphanumeric.
	 * Equivalent to IsAlpha(x) || IsUniDigit(x) (but faster), or
	 * checking CharacterClass for one of Nd, Nl, No, Ll, Lm,
	 * Lo, Lt and Lu.
	 */
	static inline BOOL IsAlphaOrDigit(UnicodePoint x)
	{
		switch (GetCharacterClass(x))
		{
		case CC_Nd:
		case CC_Nl:
		case CC_No:
		case CC_Ll:
		case CC_Lm:
		case CC_Lo:
		case CC_Lt:
		case CC_Lu:
			return TRUE;
		}
		return FALSE;
	}

	/**
	 * Return TRUE if the character is a surrogate character.
	 * Equivalent to IsHighSurrogate() || IsLowSurrogate() (but faster).
	 */
	static inline BOOL IsSurrogate(UINT32 x)
	{
		return (x & 0xFFFFF800) == 0xD800;
	}

	/**
	 * Return TRUE if the character is a high surrogate.
	 * The high surrogate is the first UTF-16 code unit in a surrogate
	 * pair.
	 */
	static inline BOOL IsHighSurrogate(UINT32 x)
	{
		return x >= 0xD800 && x <= 0xDBFF;
	}

	/**
	 * Return TRUE if the character is a low surrogate.
	 * The low surrogate is the second UTF-16 code unit in a surrogate
	 * pair.
	 */
	static inline BOOL IsLowSurrogate(UINT32 x)
	{
		return x >= 0xDC00 && x <= 0xDFFF;
	}

	/**
	 * Return the character class of the specified character.
	 *
	 * Undefined characters does not nessesarily have the CC_Unknown
	 * character class as it is now, due to table-size optmization.
	 * They generally speaking get the character class of the first
	 * defined character with a lower codepoint.
	 *
	 * - CC_Unknown  Unknown character
     * - CC_Lu	   Letter, Uppercase
     * - CC_Ll	   Letter, Lowercase
     * - CC_Lt	   Letter, Titlecase
     * - CC_Lm	   Letter, Modifier
     * - CC_Lo	   Letter, Other
     * - CC_Mn	   Mark, Nonspacing
     * - CC_Mc	   Mark, Spacing Combining
     * - CC_Me	   Mark, Enclosing
     * - CC_Nd	   Number, Decimal Digit
     * - CC_Nl	   Number, Letter
     * - CC_No	   Number, Other
     * - CC_Pc	   Punctuation, Connector
     * - CC_Pd	   Punctuation, Dash
     * - CC_Ps	   Punctuation, Open
     * - CC_Pe	   Punctuation, Close
     * - CC_Pi	   Punctuation, Initial quote (may behave like Ps or Pe depending on usage)
     * - CC_Pf	   Punctuation, Final quote (may behave like Ps or Pe depending on usage)
     * - CC_Po	   Punctuation, Other
     * - CC_Sm	   Symbol, Math
     * - CC_Sc	   Symbol, Currency
     * - CC_Sk	   Symbol, Modifier
     * - CC_So	   Symbol, Other
     * - CC_Zs	   Separator, Space
     * - CC_Zl	   Separator, Line
     * - CC_Zp	   Separator, Paragraph
     * - CC_Cc	   Other, Control
     * - CC_Cf	   Other, Format
     * - CC_Cs	   Other, Surrogate
     * - CC_Co	   Other, Private Use
	 */
	static CharacterClass GetCharacterClass(UnicodePoint c) PURE_FUNCTION;

	/**
	 * Return the value of the specified property for given character.
	 */
	static BOOL CheckPropertyValue(UnicodePoint c, UnicodeProperties prop) PURE_FUNCTION;

	/**
	 * Return the value of the specified derived property for given character.
	 * Derived properties are usually defined as a logical combination of CoreProperties and character classes 
	 * (defined in DerivedCoreProperties.txt).
	 */
	static BOOL CheckDerivedPropertyValue(UnicodePoint c, UnicodeDerivedProperties prop) PURE_FUNCTION;

	/**
	 * Return the blocktype for given character. Possible blocks are defined by enum UnicodeBlocks (generated from Blocks.txt)
	 */
	static UnicodeBlockType GetUnicodeBlockType(UnicodePoint c) PURE_FUNCTION;

	/**
	 * Return the joining type for given character. Possible joining types are defined by enum UnicodeJoiningType (generated from ArabicShaping.txt)
	 */
	static UnicodeJoiningType GetUnicodeJoiningType(UnicodePoint c) PURE_FUNCTION;

#ifdef SUPPORT_UNICODE_NORMALIZE
	/**
	 * Get the combining class for a combining character. The class
	 * is a value between 0 and 255, where 0 represent non-combining
	 * base characters.
	 *
	 * See section 4.2 of the Unicode standard.
	 *
	 * @param c Character to look up.
	 * @return The combining class for the character.
	 */
	static unsigned char GetCombiningClass(uni_char c) PURE_FUNCTION;

	/**
	 * @overload
	 *
	 * @param c1 The high (first) surrogate representing the character.
	 * @param c2 The low (second) surrogate representing the character.
	 */
	static unsigned char GetCombiningClass(uni_char c1, uni_char c2) PURE_FUNCTION;

	/**
	 * @overload
	 *
	 * Get the combining class for a unicode code point. The class is a value
	 * between 0 and 255, where 0 represent non-combining base characters. This
	 * function has a unique name to guard against overload resolution
	 * problems with GetCombiningClass().
	 *
	 * See section 4.2 of the Unicode standard.
	 *
	 * @param c The code point of the character.
	 */
	static unsigned char GetCombiningClassFromCodePoint(UnicodePoint c) PURE_FUNCTION;
#endif

#ifdef SUPPORT_TEXT_DIRECTION
	// unicode_bidi.cpp

	/**
	 * Return the bidi category of the specified character.
	 *
	 * @return One of BIDI_UNDEFINED, BIDI_L, BIDI_R, BIDI_AL, BIDI_LRE,
	 * BIDI_LRO, BIDI_RLE, BIDI_RLO, BIDI_PDF, BIDI_EN, BIDI_ES,
	 * BIDI_ET, BIDI_AN, BIDI_CS, BIDI_NSM, BIDI_BN, BIDI_B, BIDI_S, BIDI_WS
	 * and BIDI_ON
	 */
	static BidiCategory GetBidiCategory(UnicodePoint c) PURE_FUNCTION;

	/**
	 * Returns TRUE if the supplied character has a mirror form (as an
	 * example '(' is a mirror form of ')').
	 */
	static BOOL IsMirrored(UnicodePoint c) PURE_FUNCTION;

	/**
	 * Returns the mirror form of the specified character (as an
	 * example '(' is a mirror form of ')'). Returns 0 if there is no
	 * mirror character.
	 */
	static UnicodePoint GetMirrorChar(UnicodePoint c) PURE_FUNCTION;
#endif							// SUPPORT_TEXT_DIRECTION

#ifdef USE_UNICODE_HANGUL_SYLLABLE
	/**
	 * Returns the Hangul_Syllable_Type property for the specified character.
	 *
	 * @return
	 *   One of Hangul_L, Hangul_V, Hangul_T, Hangul_LV and Hangul_LVT for
	 *   Korean characters. <br>
	 *   Hangul_NA for non-Korean characters.
	 */
	static HangulSyllableType GetHangulSyllableType(UnicodePoint c) PURE_FUNCTION;
#endif

#ifdef USE_UNICODE_SEGMENTATION
	/**
	 * Returns the Word_Break property as defined in UAX#29 for the
	 * specified character.
	 *
	 * @return One of the WordBreakType types, WB_Other for undefined
	 *         (category "Any").
	 */
	static WordBreakType GetWordBreakType(UnicodePoint c) PURE_FUNCTION;

	/**
	 * Returns the Sentence_Break property as defined in UAX#29 for the
	 * specified character.
	 *
	 * @return One of the SentenceBreakType types, SB_Other for
	 *         undefined (category "Any").
	 */
	static SentenceBreakType GetSentenceBreakType(UnicodePoint c) PURE_FUNCTION;
#endif

	/**
	 * Convert a UTF-16 surrogate pair into a Unicode code point.
	 *
	 * @param high High surrogate for the Unicode code point
	 *             (first UTF-16 code unit).
	 * @param low  Low surrogate for the Unicode code point
	 *             (second UTF-16 code unit).
	 * @return The Unicode code point corresponding to the surrogate pair.
	 */
	static inline UnicodePoint DecodeSurrogate(uni_char high, uni_char low)
	{
		return 0x10000 + (((high & 0x03FF) << 10) | (low & 0x03FF));
	}

	/**
	 * Convert a Unicode code point into a UTF-16 surrogate pair.
	 *
	 * @param ucs  The Unicode code point to convert.
	 * @param high High surrogate for the Unicode code point (output)
	 *             (first UTF-16 code unit).
	 * @param low  Low surrogate for the Unicode code point (output)
	 *             (second UTF-16 code unit).
	 */
	static inline void MakeSurrogate(UnicodePoint ucs, uni_char &high, uni_char &low)
	{
		// Surrogates spread out the bits of the UCS value shifted down by 0x10000
		ucs -= 0x10000;
		high = 0xD800 | uni_char(ucs >> 10);	// 0xD800 -- 0xDBFF
		low  = 0xDC00 | uni_char(ucs & 0x03FF);	// 0xDC00 -- 0xDFFF
	}

private:
	// Internal helpers
	static unsigned short GetCaseInfo(UnicodePoint) PURE_FUNCTION;
	static int Decompose(uni_char, uni_char *, BOOL);
	static int Decompose(uni_char, uni_char, uni_char *, BOOL);
	static BOOL IsWideOrNarrowDecomposition(int decomposition_index) PURE_FUNCTION;
	static UniBuffer *Decompose(UniBuffer *, BOOL);
	static UINT32 Compose(UINT32, UINT32) PURE_FUNCTION;
	static UniBuffer *Compose(UniBuffer *);
#ifdef USE_UNICODE_LINEBREAK
	static LinebreakClass LineBreakRemap(LinebreakClass lbclass) PURE_FUNCTION;
#ifndef _NO_GLOBALS_
	static LinebreakClass GetLineBreakClassInternalWithCache(UnicodePoint c) PURE_FUNCTION;
#endif // !_NO_GLOBALS_
	static LinebreakClass GetLineBreakClassInternal(UnicodePoint c) PURE_FUNCTION;
	static LinebreakOpportunity Is_SA_LinebreakOpportunity(UnicodePoint prev, const uni_char *buf, int len) PURE_FUNCTION;
#endif // USE_UNICODE_LINEBREAK
	static CharacterClass GetCharacterClassInternal(UnicodePoint c) PURE_FUNCTION;
#ifndef _NO_GLOBALS_
	static CharacterClass GetCharacterClassInternalWithCache(UnicodePoint c) PURE_FUNCTION;
#endif // !_NO_GLOBALS_

	static void ToLowerInternal(UnicodePoint *x);
	static void ToUpperInternal(UnicodePoint *x);
};

inline CharacterClass Unicode::GetCharacterClass(UnicodePoint c)
{
	// make the most likely code path inline
	if (c < 256)
	{
		// Classes for U+0000 to U+00FF are stored in a flat structure
		// for speed
		return static_cast<CharacterClass>(cls_data_flat[c]);
	}

	// do the less likely non-inline
#ifndef _NO_GLOBALS_
	return Unicode::GetCharacterClassInternalWithCache(c);
#else
	return Unicode::GetCharacterClassInternal(c);
#endif
}

#ifdef USE_UNICODE_LINEBREAK
inline LinebreakClass Unicode::GetLineBreakClass(UnicodePoint c)
{
	// inline the most likely and faster path
	if (c < 256)
	{
		// Properties for U+0000 to U+00FF are stored in a flat structure
		// for speed
		return static_cast<LinebreakClass>(line_break_flat_new[c]);
	}
	else
	{
#ifndef _NO_GLOBALS_
		return GetLineBreakClassInternalWithCache(c);
#else
		return GetLineBreakClassInternal(c);
#endif
	}
}

inline LinebreakClass Unicode::LineBreakRemap(LinebreakClass lbclass)
{
	// Remap ambigous classes as alphabetical
	if (lbclass == LB_AI /* Ambiguous (Alphabetic or Ideographic) */ ||
		lbclass == LB_SA /* Complex Context Dependent (South East Asian) */ ||
		lbclass == LB_SG /* Surrogates */ ||
		lbclass == LB_XX /* Unknown */)
	{
		return LB_AL /* Ordinary Alphabetic and Symbol Character */;
	}
	else if (lbclass == LB_CB /* Contingent Break Opportunity */)
	{
		/* Treat object replacement character as ideographic */
		return LB_ID /* Ideographic */;
	}
	return lbclass;
}
#endif

#endif // UNICODE_BUILD_TABLES
#endif // !MODULES_UNICODE_UNICODE_H
