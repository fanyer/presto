/** -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#ifndef UNICODE_STRINGITERATOR_H
#define UNICODE_STRINGITERATOR_H

#include "modules/opdata/UniString.h"
#include "modules/opdata/UniStringFragmentIterator.h"

/** @short Iterator for unicode strings.
 *
 * A read-only bidirectional iterator for unicode strings (uni_char*) that
 * returns Unicode points, automatically decoding surrogate pairs. Supports
 * skipping over combining characters.
 *
 * The iterator has deliberately been kept very simple to avoid potential
 * confusion over what constitutes the "current" position and whether Next()
 * and Previous() return the character at the new or the old position.
 *
 * Valid indices for the iterator are 0..uni_strlen(str), where IsAtBeginning()
 * and IsAtEnd() check for the endpoints. Note that the last index acts as a
 * end sentinel and is not a valid index of the string.
 *
 * Surrogate code units appearing in invalid configurations (not high-low
 * pairs) will be returned as-is.
 */
class UnicodeStringIterator
{
public:

	/** @short Constructor.
	 *
	 * @param str String to be iterated.
	 * @param start_position Start position of the iterator. Pass KAll to start
	 *        at the end sentinel. Start positions past the end of the string will be
	 *        clamped to the end sentinel.
	 * @param length Length of the string in code units (uni_char's). If KAll,
	 *        the length of the string will be determined using
	 *        uni_strlen().
	 */
	explicit UnicodeStringIterator(const uni_char* str, int start_position = 0, int length = KAll);

	/** Create a character iterator over the given UniString.
	 * @param string is the UniString to iterate over.
	 * @param start_position is the index of the character to start at. If the
	 *        value is greater than string.Length(), e.g. OpDataUnknownLength,
	 *        the iterator is started at the end sentinel. */
	explicit UnicodeStringIterator(const UniString& string, size_t start_position = 0);

	/**
	 * Moves the iterator to the next Unicode point in the string, or to the
	 * end sentinel. If the iterator is already at the end sentinel, this is a
	 * no-op.
	 * @retval true if the iterator has proceeded to a valid character.
	 * @retval false if the iterator has hit the end of the string.
	 */
	bool Next();

	/** Returns true if Next() can proceed to a valid character. */
	op_force_inline bool HasNext(void) const
		{ return (m_pos + MAX(0, -m_pos_delta) < m_length) || m_itr.HasNext(); }

	/**
	 * Moves the iterator to the previous Unicode point in the string. If the
	 * iterator is already at the beginning, this is a no-op.
	 * @retval true if the iterator has proceeded to a valid character.
	 * @retval false if the iterator was already at the beginning.
	 */
	bool Previous();

	/** Returns true if Previous() can proceed to a valid character. */
	op_force_inline bool HasPrevious(void) const { return m_index > 0; }

#ifdef SUPPORT_UNICODE_NORMALIZE
	/**
	 * Moves the iterator to the next non-combining (combining class 0) Unicode
	 * point in the string, or to the end sentinel if none appear. If the
	 * iterator is already at the end, this is a no-op.
	 */
	bool NextBaseCharacter()
	{
		while (Next())
			if (SafeGetCombiningClass(At()) == 0)
				return true;
		return false;
	}

	/**
	 * Moves the iterator to the previous non-combining (combining class 0)
	 * Unicode point in the string, or to the beginning of the string if none
	 * appear. If the iterator is already at the beginning, this is a no-op.
	 * @retval true if the iterator has proceeded to a valid character.
	 * @retval false if the iterator was already at the beginning.
	 */
	bool PreviousBaseCharacter()
	{
		while (Previous())
			if (SafeGetCombiningClass(At()) == 0)
				return true;
		return false;
	}
#endif // SUPPORT_UNICODE_NORMALIZE

	/**
	 * Return true if the iterator is at the beginning (index 0) of the string,
	 * otherwise false.
	 */
	bool IsAtBeginning() const { return m_index == 0; }

	/**
	 * Returns true if the iterator is at the end sentinel (index one past the
	 * last valid index in the string), otherwise false.
	 */
	bool IsAtEnd() const { return m_pos_delta == 0; }

	/**
	 * Returns the Unicode point at the current position of the iterator, or 0
	 * if the iterator is at the end sentinel.
	 */
	UnicodePoint At() const { return m_current; }

	/**
	 * Returns the current index of the iterator in the string. For non-BMP
	 * characters this is the index of the first (high) surrogate.
	 */
	size_t Index() const { return m_index; }

	/**
	 * Resets the current position to the first character in the string.
	 */
	void Reset();

	/**
	 * Returns an iterator pointing to the beginning of the string.
	 */
	UnicodeStringIterator FromBeginning() const;

	/** Operator overloading syntactic sugar.
	 *
	 * @{
	 */
	bool operator++(void) { return Next(); }
	bool operator--(void) { return Previous(); }
	UnicodePoint operator*(void) const { return At(); }
	/** @} */

private:
#ifdef SUPPORT_UNICODE_NORMALIZE
	static unsigned char SafeGetCombiningClass(UnicodePoint c)
	{	/* For malformed UTF16 strings c might represent a standalone surrogate,
		 * which we treat as a non-combining character. */
		if (Unicode::IsSurrogate(c))
			return 0;
		return Unicode::GetCombiningClassFromCodePoint(c);
	}
#endif // SUPPORT_UNICODE_NORMALIZE

	/** Sets the unicode-point m_current according to the current string
	 * position. */
	void SetNextUnicodePoint();

	/** Moves the internal iterator to the next fragment and sets m_str,
	 * m_length to the next fragment and m_pos to 0. */
	void StepToNextFragment();

	/** Moves the internal iterator to the previous fragment and sets m_str,
	 * m_length to the previous fragment data and m_pos to the last char in
	 * that fragment. */
	void StepToPrevFragment();

	/** Return the uni_char at m_str[m_pos] and increase m_pos and m_pos_delta,
	 * so m_pos is the index of the character after the returned char. */
	uni_char NextUniChar();

	/** Decrease m_pos and m_pos_delta and return the uni_char at m_str[m_pos],
	 * so m_pos is the index of the returned character. */
	uni_char PrevUniChar();

	/** Are we iterating over a UniString or a uni_char*?
	 *
	 * When iterating a UniString, care is taken to ensure m_itr.IsValid()
	 * remains true at all times (even when this Unicode iterator is invalid).
	 * When iterating a raw uni_char*, m_itr is always invalid.
	 */
	bool IsIteratingUniString() { return m_itr.IsValid(); }

	/** The fragment-iterator is used when iterating over a UniString. In that
	 * case the pair (m_str,m_length) is set to the current fragment of the
	 * UniString (m_itr.GetData(), m_itr.GetLength()). When iterating over a
	 * const uni_char*, the fragment-iterator remains empty and thus it returns
	 * false to HasNext() and HasPrev(), which is consistent with iterating over
	 * a UniString. */
	UniStringFragmentIterator m_itr;

	const uni_char* m_str;	///< The current string fragment.
	size_t m_length;		///< Length of the current string fragment.

	/** Points to the index of the last uni_char (relative to m_str) which was
	 * used to construct the current UnicodePoint. Depending on the sign of
	 * m_pos_delta, it is the index of the first uni_char after the current
	 * UnicodePoint (m_pos_delta >= 0) or the index of the first uni_char of
	 * the current UnicodePoint (m_pos_delta < 0).
	 * The valid range is 0 <= m_pos <= m_length.
	 *
	 * @note We store the index (together with m_pos_delta) in this way to be
	 *   able to handle a UnicodePoint that is distributed over two
	 *   string-fragments. The first uni_char may be at the end of the first
	 *   fragment, the second uni_char may be the first uni_char of the next
	 *   fragment. If the iterator is at the first fragment before that
	 *   UnicodePoint and we use Next() to iterate to it, then we first take the
	 *   last uni_char of the first fragment, detect that we need another
	 *   uni_char to complete the UnicodePoint, advance the fragment-iterator to
	 *   the second fragment, take the first uni_char from it at let m_pos point
	 *   to the index of the first uni_char in the second fragment which is not
	 *   part of the current UnicodePoint. For the reverse operation, i.e. if
	 *   the iterator points just after such a UnicodePoint and we use
	 *   Previous() to iterate to it, then we first take the first character of
	 *   the second fragment, detect that we need another uni_char, advance the
	 *   fragment-iterator to the first fragment, take the last uni_char from it
	 *   and let m_pos point to the index of the uni_char that we used.
	 *   Though we are in both cases at the same UnicodePoint, the
	 *   fragment-iterator points to different fragments, depending on the order
	 *   of reading the uni_chars. That is reflected in the values of m_pos and
	 *   m_pos_delta. */
	size_t m_pos;

	/** The absolute value specifies the number of uni_chars from the string
	 * that were used to construct the current UnicodePoint.
	 * - If m_pos_delta == 0, then the iterator is at the end of the string and
	 *   m_current == 0.
	 * - If m_pos_delta < 0, then the iterator was moved backwards with a call
	 *   to Previous().
	 * - If m_pos_delta > 0, then the iterator was moved just initialised or
	 *   moved forward with a call to Next(). */
	int m_pos_delta;

	/** The index of the current UnicodePoint relative to the complete string.
	 * If the UnicodePoint is constructed from multiple uni_char, then it is
	 * the index of the first uni_char. */
	size_t m_index;

	UnicodePoint m_current;	///< The current UnicodePoint.
};

#endif // UNICODE_STRINGITERATOR_H
