/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#if !defined UNICODE_SEGMENTER_H && defined USE_UNICODE_SEGMENTATION
#define UNICODE_SEGMENTER_H

#include "modules/unicode/unicode.h"

/**
 * Identify segments in Unicode text. Implementation of Unicode Standard
 * Annex (UAX) 29, Text Boundaries.
 *
 * State machine for calculating word boundaries:
 * @dotfile wordbreak.dot "Word boundary detection; green for boundary, red for non-boundary"
 *
 * State machine for calculating sentence boundaries:
 * @dotfile sentencebreak.dot "Sentence boundary detection; green for boundary, red for non-boundary"
 */
class UnicodeSegmenter
{
public:
	/**
	 * Determine whether there is a grapheme cluster boundary between
	 * the two Unicode UTF-16 code units. If TRUE, consider these
	 * two code units as belonging to different grapheme clusters
	 * ("characters"). If FALSE, the two code units are part of the
	 * same grapheme cluster (which may consist of more code units).
	 *
	 * Since we are using UTF-16 encoded characters, we will be unable
	 * to detect combining marks outside of the basic multi-lingual
	 * plane.
	 *
	 * Indicate start-of-text or end-of-text by inputting NUL characters.
	 *
	 * Tailoring: If the first character is a NUL and the second character
	 * is a combining mark, no boundary will be reported, considering
	 * the NUL a base character.
	 *
	 * @param ch1 First UTF-16 code unit.
	 * @param ch2 Second UTF-16 code unit.
	 * @return
	 *    Whether the two code units belong to different grapheme clusters.
	 */
	static inline BOOL IsGraphemeClusterBoundary(UnicodePoint ch1, UnicodePoint ch2)
	{ return IsGraphemeClusterBoundary(ch1, Unicode::GetCharacterClass(ch1), ch2, Unicode::GetCharacterClass(ch2)); }

	/**
	 * Determine whether the characters might be the first half of
	 * a pair so that they should not be split.
	 *
	 * Should only be used if the following character is unknown. If the
	 * next character is known IsGraphemeClusterBoundary should be used
	 * instead.
	 *
	 * @param c Unicode code point.
	 * @return Whether the code units indicates that it should be kept along
	 *     with a following (not yet known) character.
	 *
	 * @see IsGraphemeClusterBoundary
	 */
	static BOOL MightBeFirstHalfOfGraphemeCluster(UnicodePoint c);

	/** Type of segments that are searchable. */
	enum Type
	{
		Grapheme,		/**< Grapheme cluster (character) boundary. */
		Word,			/**< Word boundary. */
		Sentence		/**< Sentence boundary. */
	};

	/**
	 * Create a Unicode segmenter object. When creating the object, you
	 * will need to determine which type of boundary you want FindBoundary()
	 * to find.
	 *
	 * @param type The type of boundary to find.
	 */
	explicit UnicodeSegmenter(enum Type type) : m_type(type) { Reset(); }

	/**
	 * Locate the next boundary of the selected type. This will search
	 * through the text and return the index to BEFORE the character at
	 * which there is a boundary.
	 *
	 * The state will be retained between calls to FindBoundary().
	 * Subsequent calls to FindBoundary() must start with "text" pointing
	 * to the first character after the boundary (text + boundary),
	 * and adjust the text_len parameter (text_len - boundary):
	 *
	 * <pre>
	 *  UnicodeSegmenter segmenter(UnicodeSegmenter::Word);
	 *  boundary = segmenter.FindBoundary(text_p, text_len);
	 *  text_p += boundary;
	 *  text_len -= boundary;
	 * </pre>
	 *
	 * To indicate that the text ends after the provided text segment,
	 * DO include the trailing NUL character in the character count.
	 * This is the only way of determining whether there is a boundary
	 * after the last character in a text string, due to the way
	 * combining characters are encoded in Unicode (following the
	 * base character).
	 *
	 * - To add an end-of-text token, call with "text_len" = 1 and
	 *   "text" = NULL or pointing a string only consisting of a NUL
	 *   character.
	 * - To reset the state (add a start-of-text token), call Reset().
	 *
	 * @param text The text fragment to search for a boundary.
	 * @param text_len
	 *    Length of the text fragment. A NUL character is treated as
	 *    end-of-text.
	 *
	 * @return
	 *   Index to the character before which there is a boundary of type
	 *   "type".
	 *   <br>
	 *   0 is returned if there is a boundary before the first character
	 *   in the string. The internal state is updated so that the next
	 *   value returned will be > 0.
	 *   <br>
	 *   text_len is returned if the algorithm could not find a boundary
	 *   within the fragment. All of "text" was consumed. There might
	 *   still be a boundary at the end of this fragment, which then
	 *   will be reported by returning 0 on the next call to FindBoundary()
	 *   which has text_len > 0.
	 */
	int FindBoundary(const uni_char *text, int text_len);

	/**
	 * Return the last character scanned. If the entire input string was
	 * consumed, this is the last character of the input string. If a
	 * boundary was reported, this is the character immediately following
	 * the boundary.
	 *
	 * Call LastClass() to get the corresponding character class (cached)
	 * of this character.
	 *
	 * The data is valid as long as the last call to FindBoundary() was
	 * with text_len > 0, i.e even if it returned 0.
	 *
	 * @return The last UTF-16 code unit scanned from the input.
	 */
	inline uni_char LastChar() { return m_prev; }

	/**
	 * Return the class of the last character scanned. If the entire input string was
	 * consumed, this is the last character of the input string. If a
	 * boundary was reported, this is the character immediately following
	 * the boundary.
	 *
	 * Call LastChar() to get the corresponding character (cached).
	 *
	 * The data is valid as long as the last call to FindBoundary() was
	 * with text_len > 0, i.e even if it returned 0.
	 *
	 * @return The last character class scanned from the input.
	 */
	inline CharacterClass LastClass() { return m_prev_class; }

	/**
	 * Reset the state of the segmenter to that of start-of-text.
	 */
	void Reset();

	/** @overload
	 *
	 * @param ch1 First UTF-16 code unit.
	 * @param cl1 Class of first code unit.
	 * @param ch2 Second UTF-16 code unit.
	 * @param cl2 Class of first code unit.
	 * @return
	 *    Whether the two code units belong to different grapheme clusters.
	 */
	static BOOL IsGraphemeClusterBoundary(UnicodePoint ch1, CharacterClass cl1, UnicodePoint ch2, CharacterClass cl2);
	
	/**
	 * Word boundry states
	 */
	enum WB_State {
		WBS_Start, WBS_CR, WBS_Newline, WBS_ALetter, WBS_ALetter_MidLetter,
		WBS_Numeric, WBS_Numeric_MidNum,
		WBS_Katakana, WBS_ExtendNumLet
	};

	/**
	 * Sentence boundry states
	 */
	enum SB_State {
		SBS_Start, SBS_CR, SBS_Sep,
		SBS_ATerm, SBS_ATerm_Close_Sp, SBS_ATerm_Close_Sp_Misc,
		SBS_STerm, SBS_STerm_Close_Sp, SBS_STerm_Close_Sp_Sep,
		SBS_Upper, SBS_Upper_ATerm
	};

private:
	// Internal helpers
	static inline bool IsControl(UnicodePoint ch, CharacterClass cl);
	static inline bool IsExtend(UnicodePoint ch, CharacterClass cl);

	// State keeping
	Type m_type;			///< Type of boundary we are looking for
	UnicodePoint m_prev;		///< Last character consumed
	CharacterClass m_prev_class;	///< Class of last character consumed
	bool m_last_found_boundary;		///< If we found a boundary last time

	// State machines (we only ever need one of these, which one is
	// determined by m_type)
	union
	{
		enum WB_State m_wb_state;		///< Word boundary state machine
		enum SB_State m_sb_state;		///< Sentence boundary state machine
	};
};

#endif
