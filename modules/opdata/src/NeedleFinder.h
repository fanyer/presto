/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_OPDATA_NEEDLEFINDER_H
#define MODULES_OPDATA_NEEDLEFINDER_H

/** @file
 *
 * @brief NeedleFinder and derived classes - finding needles in haystacks.
 *
 * This file is an internal/private part of the opdata module. Please do not
 * \#include this file from outside modules/opdata.
 *
 * This file contains the NeedleFinder base class, used by OpData and UniString
 * when some kind of needle needs to be found in a haystack consisting of a
 * linked list of fragments. The implementations of NeedleFinder (also found in
 * this file) are instantiated by various Find*() and other methods in OpData
 * and UniString.
 *
 * See OpData.h and UniString.h for more extensive design notes on OpData and
 * UniString, respectively.
 */


/**
 * Helper base class for finding needles in a succession of OpDataFragments.
 *
 * Implement #FindNeedle() in a subclass, and pass an instance of that subclass
 * to OpData::Find() or OpData::Split(). The instance's #FindNeedle() will be
 * invoked one or more times for each fragment, to find needles within each
 * fragment.
 *
 * Note that a (multi-byte) needle may straddle fragment boundaries, so
 * implementations deriving from this class should store the state needed to
 * handle such scenarios. Also, a needle finder searching for the \e last
 * occurrence of a needle will need to keep track of a \e potential last
 * occurrence in a previous fragment.
 */
class NeedleFinder
{
public:
	/**
	 * Find a needle within the given fragment data.
	 *
	 * The details of what kind of needle is found, or whether the first or
	 * last occurrence of the needle is found, is left to the implementation
	 * derived from this base class.
	 *
	 * This method will be repeatedly invoked with subsequent fragments of
	 * data (\c buf, \c buf_len) from a fragment list; the accompanying
	 * \c buf_index specifies the global index of \c buf in the fragment
	 * list. The \c buf_index values passed on subsequent calls correspond
	 * to each other in that the \c buf_index passed in a given call equals
	 * \c buf_index + \c buf_len from the previous call. Note that the
	 * \c buf_index passed on the initial call is not necessarily 0.
	 *
	 * This method shall return the global index of the \e start of the
	 * found needle. The returned index shall be within the range of
	 * \c buf_index + \c buf_len values passed this or a previous invocation
	 * of this method. The width of the found needle (in number of bytes)
	 * shall be stored in \c needle_len. If no needle is found (so far),
	 * this method shall return #OpDataNotFound.
	 *
	 * More formally, the \c buf_index and \c buf_len values passed to each
	 * invocation of this method define a search area from the initial index
	 * i (= the \c buf_index value passed to the initial method call) to the
	 * current length l (= \c buf_index + \c buf_len values passed to the
	 * current method call). If not #OpDataNotFound, the return value r from
	 * this function (and \c needle_len) must satisfy:
	 *   i <= r <= r + \c needle_len <= l
	 *
	 * The number of bytes occupied by the needle should be stored in
	 * \c needle_len. Even when the needle straddles one or more fragment
	 * boundaries, the actual number of bytes occupied by the needle (not
	 * just the portion in the current fragment) should be stored in
	 * \c needle_len.
	 *
	 * Examples:
	 *
	 * Given the following fragments (consult OpData::DebugView() for
	 * formatting details):
	 *
	 *   [17+'egg sp'] -> ['am bacon'] -> [' and spam']
	 *
	 * resulting in following method calls:
	 *
	 * 1. FindNeedle(\c buf_index = 17, \c buf = "egg sp", \c buf_len = 6,
	 *                \c needle_len, \c last = false)
	 * 2. FindNeedle(\c buf_index = 23, \c buf = "am bacon", \c buf_len = 8,
	 *                \c needle_len, \c last = false)
	 * 3. FindNeedle(\c buf_index = 31, \c buf = " and spam", \c buf_len = 9,
	 *                \c needle_len, \c last = true)
	 *
	 * A needle finder searching for the first occurrence of "egg" can
	 * immediately return 17 (and store 3 in \c needle_len). The second and
	 * third method calls would not occur in this scenario, as we already
	 * found what we're looking for.
	 *
	 * A needle finder searching for first occurrence of "bacon" must return
	 * #OpDataNotFound from the first call, and then return 26 (the global
	 * index of the first byte of the needle) from the second call (and
	 * store 5 into \c needle_len). The third call would not occur.
	 *
	 * A needle finder searching for the first occurrence of "spam" should
	 * return #OpDataNotFound from the first call (since the full needle has
	 * not yet been found). The second call should return 21 (the global
	 * index of the first byte of the needle) and store 4 (the total number
	 * of bytes occupied by the needle) into \c needle_len. Again, the third
	 * method call will not occur, since we already found a result.
	 *
	 * However, a needle finder searching for the \e last occurrence of
	 * "spam" would have to return #OpDataNotFound from the first two calls
	 * (but still record the match at global index 21 as a \e potential last
	 * match). Only on the third method call (when \c last is true) would
	 * the needle finder know that there is no more data to be searched. At
	 * this point, it would find the match in the third fragment and return
	 * global index 36 (and store 4 in \c needle_len).
	 *
	 * As a final example, calling Split("spam") would initiate a splitting
	 * process where we repeatedly search for occurrences of "spam". The
	 * method call sequence would look something like this (only the first
	 * two parameters included for brevity):
	 *
	 *   1. FindNeedle(17, "egg sp") returns OpDataNotFound.
	 *   2. FindNeedle(23, "am bacon") returns 21 and \c needle_len == 4.
	 *   (we now restart the search immediately after the found needle:)
	 *   3. FindNeedle(25, " bacon") returns OpDataNotFound.
	 *   4. FindNeedle(31, " and spam") returns 36 and \c needle_len == 4.
	 *
	 * @param buf_index The global index of the first byte in \c buf.
	 * @param buf The current fragment data in which to search for a needle.
	 * @param buf_len The number of bytes in \c buf to search.
	 * @param[out] needle_len Store the length of the needle here.
	 * @param last true if the given \c buf & \c buf_len includes the end of
	 *        the search area (haystack); false if there is more data in the
	 *        fragment list beyond the end of the given \c buf. If \c last
	 *        is true, and this method returns NULL, the caller will
	 *        consider the search area exhausted.
	 * @returns The global index of the start of the found needle, or
	 *          #OpDataNotFound if a needle is yet to be found.
	 */
	virtual size_t FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool last) = 0;
};

/// Find the first occurrence of the given byte. The simplest needle finder.
class ByteFinder : public NeedleFinder
{
public:
	ByteFinder(char needle) : m_needle(needle) {}

	virtual size_t FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool /* last */)
	{
		*needle_len = 1;
		const char *p = static_cast<const char *>(op_memchr(buf, m_needle, buf_len));
		return p ? buf_index + (p - buf) : OpDataNotFound;
	}

private:
	char m_needle; ///< The byte we're looking for.
};

/// Find the first occurrence of the given uni_char.
class UniCharFinder : public NeedleFinder
{
public:
	UniCharFinder(uni_char needle) : m_needle(needle) {}

	virtual size_t FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool /* last */);

private:
	uni_char m_needle; ///< The uni_char we're looking for.
};

/**
 * The needle finder used by the multi-byte (const char *) versions of OpData's
 * FindFirst(), Remove(), and Split() methods.
 */
class ByteSequenceFinder : public NeedleFinder
{
public:
	ByteSequenceFinder(const char *needle, size_t length, size_t unit_size = 1)
		: m_needle(needle)
		, m_needle_len(length)
		, m_partial(NULL)
		, m_unit_size(unit_size)
	{
		OP_ASSERT(m_needle_len % m_unit_size == 0);
	}

	virtual size_t FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool last);

protected:
	/**
	 * Method used to find the start of #m_needle in the given \c buf.
	 *
	 * Operates in units of #m_unit_size. I.e. when the pointer returned is
	 * non-NULL, it points to a location in \c buf that matches the first
	 * #m_unit_size bytes of #m_needle.
	 *
	 * The default implementation works for byte sequences (where
	 * #m_unit_size == 1). This method must be overridden in subclasses
	 * that use m_unit_size > 1.
	 *
	 * @param buf The buffer to search for the start of #m_needle.
	 * @param buf_len The number of bytes available in \c buf. This is
	 *          always a multiple of #m_unit_size (or 0).
	 * @returns Pointer to the first correctly-aligned occurrence of the
	 *          #m_unit_size first bytes of #m_needle in \c buf, or NULL if
	 *          no such occurrence is found.
	 */
	virtual const char *StartMatch(const char *buf, size_t buf_len)
	{
		OP_ASSERT(m_unit_size == 1);
		return static_cast<const char *>(
			op_memchr(buf, m_needle[0], buf_len));
	}

	/**
	 * Possible return values from ContinueMatch().
	 *
	 * @see ContinueMatch() for further details of each value.
	 */
	enum MatchResult
	{
		FULL_MATCH,
		PARTIAL_MATCH,
		FAILED_MATCH
	};

	/**
	 * Try to complete the match of #m_partial against \c buf.
	 *
	 * Preconditions: A potential needle match has been found.
	 *  - #m_partial points to the part of the #m_needle that has yet to be
	 *    matched.
	 *  - \c buf points to the corresponding part of the buffer that must be
	 *    matched against #m_partial.
	 *  - \c buf_len indicates how many bytes are available from \c buf.
	 *  - \c buf_index holds the global index of \c buf.
	 *
	 * This method has one of three outcomes:
	 *
	 * - The needle was fully matched at the given buffer position.
	 *    - #m_partial is reset to NULL (we're no longer in a partial match
	 *      situation).
	 *    - the needle length is stored in \c sep_len.
	 *    - \c buf is advanced to the byte following the match, and
	 *      \c buf_index and \c buf_len are incremented/decremented to match.
	 *    - FULL_MATCH is returned.
	 *
	 * - Matching continued through \c buf, but was still partial at its
	 *    end (i.e. we need more buffer data to complete the match).
	 *    - #m_partial is advanced past the partially matched bytes.
	 *    - \c sep_len is untouched.
	 *    - \c buf is advanced by \c buf_len (to the byte following the new
	 *      partial match), and \c buf_index is incremented to match.
	 *      \c buf_len is set to zero.
	 *    - PARTIAL_MATCH is returned.
	 *    - caller should continue the matching process with the next buffer
	 *      fragment.
	 *
	 * - The partial match failed in \c buf.
	 *    - #m_partial is reset to NULL (we're no longer in a partial match
	 *      situation).
	 *    - \c sep_len is untouched.
	 *    - \c buf, \c buf_index and \c buf_len are untouched.
	 *    - FAILED_MATCH is returned.
	 *    - caller should resume search within \c buf at needle's start.
	 *
	 * @param buf_index The global index of \c buf.
	 * @param buf Where to start to match the needle.
	 * @param buf_len The number of bytes left in this fragment, starting
	 *        from \c buf.
	 * @param sep_len Upon a successful match, store the length of the
	 *        matched separator here.
	 * @return FULL_MATCH, PARTIAL_MATCH or FAILED_MATCH, according to the
	 *         above description.
	 */
	MatchResult ContinueMatch(size_t& buf_index, const char *& buf, size_t& buf_len, size_t *sep_len);

	const char *m_needle; ///< The byte sequence we're looking for.
	size_t m_needle_len; ///< The number of bytes in \c m_needle.
	const char *m_partial; ///< Partial match from last fragment.
	size_t m_unit_size; ///< \#bytes per unit. char = 1, uni_char = 2, etc.
};

/**
 * The needle finder used by the multi-character (const uni_char *) versions of
 * UniString's FindFirst(), Remove(), and Split() methods.
 */
class UniCharSequenceFinder : public ByteSequenceFinder
{
public:
	UniCharSequenceFinder(const uni_char *needle, size_t length)
		: ByteSequenceFinder(
			reinterpret_cast<const char *>(needle),
			UNICODE_SIZE(length),
			UNICODE_SIZE(1))
	{}

	static const char *FindSequence(const char *needle, size_t needle_len, const char *buf, size_t buf_len);

protected:
	virtual const char *StartMatch(const char *buf, size_t buf_len)
	{
		OP_ASSERT(m_unit_size == UNICODE_SIZE(1));
		return FindSequence(m_needle, m_needle_len, buf, buf_len);
	}
};

/**
 * Find the first byte that is a member of the given byte set.
 *
 * Used by OpData::FindFirstOf().
 */
class ByteSetFinder : public NeedleFinder
{
public:
	ByteSetFinder(void) { Clear(); }

	ByteSetFinder(const char *accept, size_t length)
	{
		Clear();
		for (size_t i = 0; i < length; i++)
			Add(accept[i]);
	}

	void Clear(void) { op_memset(m_table, 0, TABLE_SIZE); }

	void Add(unsigned char c)
	{
		// Set the bit corresponding to the given byte.
		m_table[c / CHAR_BIT] |= 1 << (c % CHAR_BIT);
	}

	virtual bool Contains(unsigned char c) const
	{
		return (m_table[c / CHAR_BIT] & (1 << (c % CHAR_BIT))) != 0;
	}

	virtual size_t FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool /* last */)
	{
		*needle_len = 1;
		for (size_t i = 0; i < buf_len; i++)
			if (Contains(buf[i]))
				return buf_index + i;
		return OpDataNotFound;
	}

private:
	/// Size of table holding one bit per byte value.
	static const size_t TABLE_SIZE = (1 << CHAR_BIT) / CHAR_BIT;

	/** Bit table containing set members. */
	unsigned char m_table[TABLE_SIZE];	// ARRAY OK 2011-10-11 markuso
};

/**
 * Find the first byte that is \e not a member of the given byte set.
 *
 * Used by OpData::FindEndOf().
 */
class ByteSetComplementFinder : public ByteSetFinder
{
public:
	ByteSetComplementFinder(const char *reject, size_t length) : ByteSetFinder(reject, length) {}

	virtual bool Contains(unsigned char c) const { return !ByteSetFinder::Contains(c); }
};

/**
 * Find the first uni_char that is a member of the given uni_char set.
 *
 * Used by UniString::FindFirstOf().
 */
class UniCharSetFinder : public NeedleFinder
{
public:
	UniCharSetFinder(const uni_char *accept, size_t length)
		: m_accept(accept), m_length(length)
	{
		for (size_t i = 0; i < length; i++)
			Add(accept[i]);
	}

	void Clear(void)
	{
		m_low_chars.Clear();
		m_length = 0;
	}

	void Add (uni_char c)
	{
		if ((c >> CHAR_BIT) == 0) // < 256
			m_low_chars.Add(c & 0xff);
	}

	virtual bool Contains(uni_char c) const
	{
		if ((c >> CHAR_BIT) == 0) // < 256
			return m_low_chars.Contains(c & 0xff);

		// Outside byte range
		return uni_memchr(m_accept, c, m_length) != NULL;
	}

	virtual size_t FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool /* last */);

private:
	ByteSetFinder m_low_chars; ///< Holds uni_char members whose value < 256.

	/*
	 * TODO: Research the following possible optimization: Replace m_accept
	 * by a map containing members whose value >= 256, or some other data
	 * structure deemed more optimal. However, if that data structure goes
	 * OOM, we'll have to revert to something similar to the current code...
	 */
	const uni_char *m_accept; ///< The original string of members.
	size_t m_length; ///< The number of uni_chars in m_accept.
};

/**
 * Find the first uni_char that is \e not a member of the given uni_char set.
 *
 * Used by UniString::FindEndOf().
 */
class UniCharSetComplementFinder : public UniCharSetFinder
{
public:
	UniCharSetComplementFinder(const uni_char *reject, size_t length) : UniCharSetFinder(reject, length) {}

	virtual bool Contains(uni_char c) const { return !UniCharSetFinder::Contains(c); }
};

/// Find the last occurrence of the given byte.
class LastByteFinder : public NeedleFinder
{
public:
	LastByteFinder(char needle) : m_needle(needle), m_found(OpDataNotFound) {}

	virtual size_t FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool last)
	{
		while (buf_len-- > 0)
			if (buf[buf_len] == m_needle)
			{
				m_found = buf_index + buf_len;
				*needle_len = 1;
				break;
			}
		return last ? m_found : OpDataNotFound;
	}

private:
	char m_needle; ///< The byte we're looking for.
	size_t m_found; ///< The last match found so far.
};

/// Find the last occurrence of the given uni_char.
class LastUniCharFinder : public NeedleFinder
{
public:
	LastUniCharFinder(uni_char needle) : m_needle(needle), m_found(OpDataNotFound) {}

	virtual size_t FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool last);

private:
	uni_char m_needle; ///< The uni_char we're looking for.
	size_t m_found; ///< The last match found so far.
};

/// Find the last occurrence of the given byte sequence.
class LastByteSequenceFinder : public ByteSequenceFinder
{
public:
	LastByteSequenceFinder(const char *needle, size_t length, size_t unit_size = 1)
		: ByteSequenceFinder(needle, length, unit_size)
		, m_last_match(OpDataNotFound)
	{}

	virtual size_t FindNeedle(size_t buf_index, const char *buf, size_t buf_len, size_t *needle_len, bool last);

protected:
	virtual const char *ReverseStartMatch(const char *buf, size_t buf_len)
	{
		OP_ASSERT(m_unit_size == 1);
		while (buf_len-- > 0)
			if (buf[buf_len] == m_needle[0])
				return buf + buf_len;
		return NULL;
	}

	/**
	 * Find the last complete match that ends in the given \c buf.
	 *
	 * Note that the start of this match may be in a previous fragment (in
	 * which case m_partial should be set to the portion of m_needle matched
	 * in previous fragment(s)).
	 *
	 * This method will not alter the value of m_partial.
	 *
	 * Returns the global index (derived from \c buf_index) of the start of
	 * the last full match of m_needle that ends in this fragment.
	 * Returns #OpDataNotFound if no such match is found.
	 */
	size_t FindLastFullMatch(size_t buf_index, const char * buf, size_t buf_len);

	/**
	 * Find the first partial match that ends \e after the present fragment.
	 *
	 * Note that the start of this partial match may be in a previous
	 * fragment (in which case m_partial should be set to the portion of
	 * m_needle matched in previous fragment(s)).
	 *
	 * If a partial match is found at the end of \c buf, m_partial will be
	 * updated to record the portion matched. Otherwise, m_partial will be
	 * reset to NULL.
	 *
	 * The given parameters may be rewritten as documented by
	 * ByteSequenceFinder::ContinueMatch().
	 */
	void FindFirstPartialMatch(size_t& buf_index, const char *& buf, size_t& buf_len, size_t *needle_len);

	size_t m_last_match; ///< Global index of start of last complete match.
};

/// Find the last occurrence of the given uni_char sequence.
class LastUniCharSequenceFinder : public LastByteSequenceFinder
{
public:
	LastUniCharSequenceFinder(const uni_char *needle, size_t length)
		: LastByteSequenceFinder(
			reinterpret_cast<const char *>(needle),
			UNICODE_SIZE(length),
			UNICODE_SIZE(1))
	{}

protected:
	virtual const char *StartMatch(const char *buf, size_t buf_len)
	{
		OP_ASSERT(m_unit_size == UNICODE_SIZE(1));
		return UniCharSequenceFinder::FindSequence(m_needle, m_needle_len, buf, buf_len);
	}

	virtual const char *ReverseStartMatch(const char *buf, size_t buf_len);
};

#endif /* MODULES_OPDATA_NEEDLEFINDER_H */
