/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UNICODE_UNICODE_LINEBREAK_THAI_FALLBACK_H
#define MODULES_UNICODE_UNICODE_LINEBREAK_THAI_FALLBACK_H

#ifdef UNICODE_FALLBACK_THAI_LINEBREAK
#include "modules/unicode/unicode.h"

/**
 * Simple fallback Thai language line breaking algorithm.
 *
 * This class implements a simplistic line breaking algorithm for strings in
 * the Thai language. It is based on the UserJS Thai line breaking
 * implementation available at [1]. The algorithm defines a fairly short list
 * of known words, and combined with lists of "leading", "following" and
 * "end-of-word" characters, makes a semi-qualified guess at appropriate line
 * break opportunities. See the implementation of NextLinebreakOpportunity()
 * for more details.
 *
 * The class assumes that all Thai characters only occupy a single uni_char.
 * This should hold as long as uni_char is 16-bit (UCS-2), and Thai characters
 * only live inside Unicode plane 0.
 *
 * [1]: http://userjs.org/scripts/browser/enhancements/thai-wrap
 */
class FallbackThaiUnicodeLinebreaker {
protected:
	/* statics imported from tables/unicode_linebreak_thai_fallback.inl */
	static const uni_char Wordlist[];
	static const unsigned short Wordlist_indices[];
	static const size_t Wordlist_len;
	static const uni_char LeadingChars[];
	static const size_t LeadingChars_len;
	static const uni_char FollowingChars[];
	static const size_t FollowingChars_len;
	static const uni_char EOWChars[];
	static const size_t EOWChars_len;

	static int MatchInWordList(const uni_char *buf, int len, const uni_char *word_list, const unsigned short *word_indices, size_t num_indices);
	static BOOL MatchInCharList(const uni_char c, const uni_char *list, size_t list_len);

public:
	/**
	 * Check if the given character is "appropriate" for this line breaker.
	 *
	 * Return TRUE if the given character indicates that the following
	 * string is appropriate input for this line breaking algorithm.
	 *
	 * Simply returns TRUE if the given codepoint is within the Thai area
	 * of Unicode.
	 *
	 * @param c The codepoint to check
	 * @return TRUE if valid input for this line breaking algorithm
	 */
	static inline BOOL IsValidChar(UnicodePoint c) {
		return c > 0x0E00 && c < 0x0E60;
	}

	/**
	 * Check if there is a linebreaking opportunity between 'prev' and
	 * the first character in the given buffer.
	 *
	 * See Unicode::IsLinebreakOpportunity() for more details on this
	 * method's preconditions.
	 *
	 * @param prev
	 *   The first code unit to test. May not be a space (class SP
	 *   (LB_SP)) or combining mark (class CM (LB_CM)). Use the value
	 *   0 to indicate start-of-text.
	 * @param buf
	 *   The second code unit to test must be at the start of this buffer,
	 *   and it may not be a space (class SP (LB_SP)) or combining mark
	 *   (class CM (LB_CM)). Use NULL or the empty string to indicate
	 *   end-of-text.
	 * @param len
	 *   Length of text in above buffer.
	 * @return
	 *   LB_YES if there is a break opportunity. <br>
	 *   LB_NO if there is no break opportunity. <br>
	 *   LB_MAYBE if prev or buf[0] is of class SP or CM.
	 */
	static LinebreakOpportunity IsLinebreakOpportunity(UnicodePoint prev, const uni_char *buf, int len) PURE_FUNCTION;
};

#endif // UNICODE_FALLBACK_THAI_LINEBREAK

#endif // !MODULES_UNICODE_UNICODE_LINEBREAK_THAI_FALLBACK_H
