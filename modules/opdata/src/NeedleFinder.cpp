/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/opdata/src/NeedleFinder.h"
#include "modules/opdata/UniString.h" // Need access to IsUniCharAligned()


/* virtual */ size_t
UniCharFinder::FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool /* last */)
{
	OP_ASSERT(UniString::IsUniCharAligned(buf_index));
	OP_ASSERT(UniString::IsUniCharAligned(buf));
	OP_ASSERT(UniString::IsUniCharAligned(buf_len));
	*needle_len = UNICODE_SIZE(1);
	const uni_char *p = uni_memchr(
		reinterpret_cast<const uni_char *>(buf), m_needle, UNICODE_DOWNSIZE(buf_len));
	OP_ASSERT(UniString::IsUniCharAligned(p));
	return p ? buf_index + (reinterpret_cast<const char *>(p) - buf) : OpDataNotFound;
}

/* virtual */ size_t
ByteSequenceFinder::FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool last)
{
	OP_ASSERT(buf_len > 0 && buf_len % m_unit_size == 0);

	if (m_partial) // Partial match from end of last fragment
	{
		switch (ContinueMatch(buf_index, buf, buf_len, needle_len))
		{
		case FULL_MATCH: // Full match, needle_len and buf_index are updated.
			OP_ASSERT(buf_index >= *needle_len);
			return buf_index - *needle_len;
		case PARTIAL_MATCH: // Partial match, reached end of buffer.
			OP_ASSERT(buf_len == 0);
			return OpDataNotFound;
		case FAILED_MATCH: // Failed match, keep going
			break;
		}
	}

	OP_ASSERT(!m_partial);

	if (m_needle_len == 0) // Split after every unit but the last
	{
		*needle_len = 0;
		return last && buf_len == m_unit_size ? OpDataNotFound : buf_index + m_unit_size;
	}
	OP_ASSERT(m_needle_len >= m_unit_size);

	while (true)
	{
		// Look for first character in needle.
		const char *p = StartMatch(buf, buf_len);
		if (!p)
			return OpDataNotFound;
		OP_ASSERT(p >= buf);
		OP_ASSERT(p + m_unit_size <= buf + buf_len);

		// See if the rest of the needle matches at the given position.
		size_t advance = p + m_unit_size - buf;
		buf_index += advance;
		buf += advance;
		buf_len -= advance;
		m_partial = m_needle + m_unit_size;
		switch (ContinueMatch(buf_index, buf, buf_len, needle_len))
		{
		case FULL_MATCH: // Full match, needle_len and buf_index are updated.
			OP_ASSERT(buf_index >= *needle_len);
			return buf_index - *needle_len;
		case PARTIAL_MATCH: // Partial match, reached end of buffer.
			OP_ASSERT(buf_len == 0);
			return OpDataNotFound;
		case FAILED_MATCH: // Failed match, keep going
			break;
		}
	}
}

ByteSequenceFinder::MatchResult
ByteSequenceFinder::ContinueMatch(size_t& buf_index, const char *& buf, size_t& buf_len, size_t *needle_len)
{
	OP_ASSERT(m_partial > m_needle);
	OP_ASSERT(m_partial <= m_needle + m_needle_len);
	size_t remaining_len = m_needle + m_needle_len - m_partial;
	size_t search_len = MIN(buf_len, remaining_len);
	if (op_memcmp(buf, m_partial, search_len) == 0)
	{
		m_partial += search_len;
		buf_index += search_len;
		buf += search_len;
		buf_len -= search_len;
		remaining_len -= search_len;
		if (!remaining_len)
		{
			// Case 0: Completed match of m_needle.
			OP_ASSERT(m_partial == m_needle + m_needle_len);
			m_partial = NULL;
			*needle_len = m_needle_len;
			return FULL_MATCH;
		}

		// Case 1: Advanced partial match of m_needle.
		OP_ASSERT(m_partial < m_needle + m_needle_len);
		return PARTIAL_MATCH;
	}

	// Case 2: Failed match
	/*
	 * Handle case where m_needle may possibly match itself at some
	 * offset < m_partial. E.g. if m_needle is "ababc", and the OpData
	 * object is ['ab'] -> ['ab'] -> ['ab'] -> ['c'] there is a partial
	 * match from index 0 to 4, and when we discover that it fails at
	 * index 4, we want to restart the search from index 1 (not index 4)
	 * so that we don't lose the full match starting at index 2.
	 */
	size_t restart_end = m_partial - m_needle;
	OP_ASSERT(m_unit_size <= restart_end);
	OP_ASSERT(restart_end < m_needle_len);
	const char *restart = StartMatch(m_needle + m_unit_size, restart_end - m_unit_size);
	if (restart)
	{
		/*
		 * The next restart point is stored in restart
		 * (m_needle < restart < m_partial). We know that
		 * restart[0] == m_needle[0], i.e. there may be a partial
		 * match from restart to m_partial.
		 *
		 * Recursively call ContinueMatch() to determine whether this
		 * potential partial match in fact exists. If it does (case
		 * PARTIAL_MATCH), m_partial will have been reset to work from
		 * that match instead of the original (now failed) match.
		 *
		 * The recursion of ContinueMatch() will take care of looking
		 * for more restart points and partial matches further into
		 * m_needle, so in case FAILED_MATCH, we can propagate that
		 * directly to our caller (i.e. there are no more partial self-
		 * matches to explore).
		 *
		 * Finally, a full match (case FULL_MATCH) is impossible here,
		 * since the recursive call works on a buf that is necessarily
		 * shorter than the current partial match.
		 */
		OP_ASSERT(restart < m_partial);
		/*
		 * We already know restart[0] == m_needle[0], so start
		 * searching m_unit_size after restart.
		 */
		const char *search_buf = restart + m_unit_size;
		OP_ASSERT(search_buf <= m_partial);
		/*
		 * Only search until the current partial match begins. From
		 * there we can resume the search in our original buffer (see
		 * case PARTIAL_MATCH below).
		 */
		size_t search_len = m_partial - search_buf;
		// Reset our partial match to beginning of m_needle.
		m_partial = m_needle + m_unit_size;

		size_t ignored = 0;
		MatchResult r = ContinueMatch(ignored, search_buf, search_len, needle_len);
		switch (r)
		{
		case FULL_MATCH:
			OP_ASSERT(!"Something is wrong with the universe.");
			break;
		case PARTIAL_MATCH:
			OP_ASSERT(search_len == 0);
			return ContinueMatch(buf_index, buf, buf_len, needle_len);
		case FAILED_MATCH:
			break; // No more partial matches in m_needle.
		}
	}
	m_partial = NULL;
	return FAILED_MATCH;
}

/* static */ const char *
UniCharSequenceFinder::FindSequence(const char *needle, size_t needle_len, const char *buf, size_t buf_len)
{
	OP_ASSERT(UniString::IsUniCharAligned(buf));
	OP_ASSERT(UniString::IsUniCharAligned(buf_len));
	OP_ASSERT(UniString::IsUniCharAligned(needle));
	OP_ASSERT(needle_len && UniString::IsUniCharAligned(needle_len));
	return reinterpret_cast<const char *>(uni_memchr(
		reinterpret_cast<const uni_char *>(buf),
		reinterpret_cast<const uni_char *>(needle)[0],
		UNICODE_DOWNSIZE(buf_len)));
}

/* virtual */ size_t
UniCharSetFinder::FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool /* last */)
{
	OP_ASSERT(UniString::IsUniCharAligned(buf_index));
	OP_ASSERT(UniString::IsUniCharAligned(buf));
	OP_ASSERT(UniString::IsUniCharAligned(buf_len));
	*needle_len = UNICODE_SIZE(1);
	const uni_char *ubuf = reinterpret_cast<const uni_char *>(buf);
	for (size_t i = 0; i < UNICODE_DOWNSIZE(buf_len); i++)
		if (Contains(ubuf[i]))
			return buf_index + UNICODE_SIZE(i);
	return OpDataNotFound;
}

/* virtual */ size_t
LastUniCharFinder::FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool last)
{
	OP_ASSERT(UniString::IsUniCharAligned(buf_index));
	OP_ASSERT(UniString::IsUniCharAligned(buf));
	OP_ASSERT(UniString::IsUniCharAligned(buf_len));
	const uni_char *ubuf = reinterpret_cast<const uni_char *>(buf);
	size_t ubuf_len = UNICODE_DOWNSIZE(buf_len);
	while (ubuf_len-- > 0)
		if (ubuf[ubuf_len] == m_needle)
		{
			m_found = buf_index + UNICODE_SIZE(ubuf_len);
			*needle_len = UNICODE_SIZE(1);
			break;
		}
	return last ? m_found : OpDataNotFound;
}

/* virtual */ size_t
LastByteSequenceFinder::FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool last)
{
	OP_ASSERT(buf_len % m_unit_size == 0);

	/*
	 * Our base class - ByteSequenceFinder - only keeps track of up to one
	 * match: the current ongoing partial match that may continue in the
	 * next fragment. We need to keep track of one additional match: the
	 * last observed _complete_ match. We need this to be able to return a
	 * match seen in a previous fragment (if there is no match in the final
	 * fragment).
	 *
	 * Therefore, divide this problem in two: First, find the last complete
	 * match, and second, find the first partial match to be potentially
	 * completed in the next fragment(s)
	 */

	// Zeroth case: handle search for the empty needle.
	if (!m_needle_len)
	{
		/*
		 * This is a pessimal case, we need to traverse to the last
		 * fragment, and return a match before the last character.
		 */
		if (!last)
			return OpDataNotFound;
		OP_ASSERT(buf_len >= m_unit_size);
		*needle_len = 0;
		return buf_index + buf_len - m_unit_size;
	}
	OP_ASSERT(m_needle_len >= m_unit_size);

	// First problem: Find the last complete match
	size_t found_complete_match = FindLastFullMatch(buf_index, buf, buf_len);
	if (found_complete_match != OpDataNotFound)
	{
		OP_ASSERT(m_last_match == OpDataNotFound || m_last_match < found_complete_match);
		m_last_match = found_complete_match;
	}

	// Return our last complete match if this is the last fragment.
	if (last)
	{
		size_t ret = m_last_match;
		// Reset state.
		m_last_match = OpDataNotFound;
		m_partial = NULL;
		*needle_len = m_needle_len;
		return ret;
	}

	// Second problem: Find partial match to be completed in next fragment.
	FindFirstPartialMatch(buf_index, buf, buf_len, needle_len);

	return OpDataNotFound;
}

size_t
LastByteSequenceFinder::FindLastFullMatch(size_t buf_index, const char * buf, size_t buf_len)
{
	/*
	 * First, look for a complete match within this fragment, or failing
	 * that, try to complete a partial match from a previous fragment.
	 */
	const char *p = buf_len >= m_needle_len
		? ReverseStartMatch(buf, buf_len - (m_needle_len - m_unit_size))
		: NULL;
	while (p)
	{
		// Potential complete match starting at p
		OP_ASSERT(p >= buf);
		OP_ASSERT(p + m_needle_len <= buf + buf_len);
		if (op_memcmp(p + m_unit_size, m_needle + m_unit_size, m_needle_len - m_unit_size) == 0)
			return buf_index + (p - buf); // Complete match starting at p.
		// Continue search for complete match between buf and p
		p = ReverseStartMatch(buf, (p - buf));
	}

	if (!m_partial) // No partial match from previous fragment.
		return OpDataNotFound;

	/*
	 * We know that the prior fragments end with the bytes between m_needle
	 * and m_partial.
	 *
	 * See if we can find a full match starting at m_partial - 1 and
	 * continuing into buf. If not, continue trying from m_partial - 2, and
	 * so on, until we've tried from m_needle. Return the global index
	 * (derived from buf_index) corresponding to the start of the full
	 * match, of #OpDataNotFound if no full match is found.
	 */
	OP_ASSERT(m_partial > m_needle);
	OP_ASSERT(m_partial <= m_needle + m_needle_len);

	p = m_partial + (buf_len < m_needle_len ? buf_len + m_unit_size - m_needle_len : 0);
	while (p >= m_needle && (p = ReverseStartMatch(m_needle, p - m_needle)) != NULL)
	{
		OP_ASSERT(p < m_partial);
		if (p == m_needle ||
		    op_memcmp(p, m_needle, m_partial - p) == 0)
		{
			// Partial match from p to m_partial
			// Try to complete match in buf.
			size_t remaining = m_needle_len - (m_partial - p);
			OP_ASSERT(remaining <= buf_len);
			if (op_memcmp(buf, m_needle + (m_partial - p), remaining) == 0)
			{
				// Full match starting at p.
				size_t pre_len = m_partial - p;
				OP_ASSERT(buf_index >= pre_len);
				return buf_index - pre_len;
			}
		}
	}
	return OpDataNotFound;
}

void
LastByteSequenceFinder::FindFirstPartialMatch(size_t& buf_index, const char *& buf, size_t& buf_len, size_t *needle_len)
{
	// Start searching within m_needle_len of fragment's end:
	int start = buf_len + m_unit_size - m_needle_len;
	const char *partial_start = NULL;
	if (start < 0)
	{
		/*
		 * The partial match we're looking for may start _before_ the
		 * current fragment, i.e. within the partial match from previous
		 * fragment(s).
		 */
		if (m_partial)
		{
			// Must start searching within current partial match.
			if (-start > m_partial - m_needle)
				start = m_needle - m_partial;
			OP_ASSERT(-start <= m_partial - m_needle);
			partial_start = StartMatch(m_partial + start, -start);
			while (partial_start)
			{
				OP_ASSERT(m_needle <= partial_start);
				OP_ASSERT(partial_start < m_partial);
				// Determine if partial match goes until buf_end.
				partial_start += m_unit_size;
				if (partial_start == m_needle + m_unit_size ||
				    op_memcmp(partial_start,
				              m_needle + m_unit_size,
				              m_partial - partial_start) == 0)
				{
					/*
					 * We have a partial match from
					 * partial_start to m_partial. Reset
					 * m_partial to reflect this new partial
					 * match, and attempt to extend this
					 * partial match towards buf_len.
					 */
					size_t len = m_partial - (partial_start - m_unit_size);
					m_partial = m_needle + len;
					switch (ContinueMatch(buf_index, buf, buf_len, needle_len))
					{
					case FULL_MATCH: // Impossible, buf_len is too small.
						OP_ASSERT(!"Should not happen!");
						// fall through
					case PARTIAL_MATCH:
						// Yes, there is a partial match to the end of buf.
						OP_ASSERT(buf_len == 0);
						return;
					case FAILED_MATCH:
						// No partial match from partial_start to end of buf.
						OP_ASSERT(!m_partial);
					}
					break;
				}
				partial_start = StartMatch(partial_start, m_partial - partial_start);
			}
		}
		start = 0; // Keep looking for partial match from 'buf'.
	}
	OP_ASSERT(start >= 0);

	/*
	 * No partial match originating in previous fragment(s) (or we would
	 * have returned above).
	 * Find start of partial match within current fragment.
	 */
	partial_start = buf + start;
	while ((partial_start = StartMatch(partial_start, buf_len - (partial_start - buf))) != NULL)
	{
		size_t advance = partial_start + m_unit_size - buf;
		buf_index += advance;
		buf += advance;
		buf_len -= advance;
		m_partial = m_needle + m_unit_size;

		// Try to extend partial match to end of buf.
		switch (ContinueMatch(buf_index, buf, buf_len, needle_len))
		{
		case FULL_MATCH: // Impossible, buf_len is too small.
			OP_ASSERT(!"Should not happen!");
			// fall through
		case PARTIAL_MATCH:
			// Yes, there is a partial match to the end of buf.
			OP_ASSERT(buf_len == 0);
			return;
		case FAILED_MATCH:
			// No partial match from partial_start. Keep searching.
			OP_ASSERT(!m_partial);
		}
		partial_start += m_unit_size;
	}
	// No partial match at all in buf.
	m_partial = NULL;
	return;
}

/* virtual */ const char *
LastUniCharSequenceFinder::ReverseStartMatch(const char *buf, size_t buf_len)
{
	OP_ASSERT(m_unit_size == UNICODE_SIZE(1));
	OP_ASSERT(UniString::IsUniCharAligned(buf));
	OP_ASSERT(UniString::IsUniCharAligned(buf_len));
	OP_ASSERT(UniString::IsUniCharAligned(m_needle));
	OP_ASSERT(m_needle_len && UniString::IsUniCharAligned(m_needle_len));
	const uni_char *ubuf = reinterpret_cast<const uni_char *>(buf);
	size_t ubuf_len = UNICODE_DOWNSIZE(buf_len);
	uni_char needle = reinterpret_cast<const uni_char *>(m_needle)[0];
	while (ubuf_len-- > 0)
		if (ubuf[ubuf_len] == needle)
			return reinterpret_cast<const char *>(ubuf + ubuf_len);
	return NULL;
}
