/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef UNICODE_FALLBACK_THAI_LINEBREAK
#include "modules/unicode/unicode_linebreak_thai_fallback.h"
#include "modules/unicode/unicode_segmenter.h"

// Define Wordlist, LeadingChars, FollowingChars and EOWChars arrays and their *_len companions.
#include "modules/unicode/tables/unicode_linebreak_thai_fallback.inl"

/* static */
int FallbackThaiUnicodeLinebreaker::MatchInWordList(const uni_char *buf, int len, const uni_char *words, const unsigned short *indices, size_t indices_len)
{
	// If the initial part of buf matches a word in the given (sorted) list
	// of words, the length of the matching word is returned. Otherwise
	// (no match), 0 is returned.
	size_t i, j;
	const uni_char *w;
	int w_len, cmp;
	// TODO: Rewrite linear search to binary search, if needed
	for (i = 0; i < indices_len; i++) {
		j = i ? indices[i - 1] : 0; // current word's index in words[]
		w = words + j;
		w_len = indices[i] - j;
		if (len < w_len)
			continue;
		cmp = uni_strncmp(buf, w, w_len);
		if (cmp == 0)
			return w_len;
		else if (cmp < 0) // we have passed buf's location in the array
			break;
	}
	return 0;
}

/* static */
BOOL FallbackThaiUnicodeLinebreaker::MatchInCharList(const uni_char c, const uni_char *list, size_t list_len)
{
	// If the given uni_char exists in the given (sorted) list of
	// uni_chars, return TRUE, otherwise return FALSE.
	size_t i;
	// TODO: Rewrite linear search to binary search, if needed
	for (i = 0; i < list_len && (c < list[i]); i++) {
		if (c == list[i])
			return TRUE;
	}
	return FALSE;
}

/* static */
LinebreakOpportunity FallbackThaiUnicodeLinebreaker::IsLinebreakOpportunity(UnicodePoint prev, const uni_char *buf, int len)
{
	OP_ASSERT(prev && buf && len);
	int w;
	UnicodePoint cur = Unicode::GetUnicodePoint(buf, len, w);
	OP_ASSERT(w > 0 && IsValidChar(cur));

	// Grapheme clusters should already have been filtered by the caller
	OP_ASSERT(UnicodeSegmenter::IsGraphemeClusterBoundary(prev, cur));

	// Following char followed by leading char
	if (MatchInCharList(prev, FollowingChars, FollowingChars_len) &&
	    MatchInCharList(cur, LeadingChars, LeadingChars_len))
		return LB_YES;

	// Thai followed by non-thai
	// (already handled by our caller - we're only called when cur is Thai)

	// Non-thai followed by thai
	if (!IsValidChar(prev))
		return LB_YES;

	// Non-leading char followed by known word
	if (!MatchInCharList(prev, LeadingChars, LeadingChars_len) &&
	    MatchInWordList(buf, len, Wordlist, Wordlist_indices, Wordlist_len))
		return LB_YES;

	// Known word followed by non-following chars
	// (not be tested here, requires holding state between invocations)

	// End-of-word symbols
	if (MatchInCharList(prev, EOWChars, EOWChars_len))
		return LB_YES;

	return LB_NO;
}

#endif // UNICODE_FALLBACK_THAI_LINEBREAK
