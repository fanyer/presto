/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_OPDATA_OPDATAFRAGMENTITERATOR_H
#define MODULES_OPDATA_OPDATAFRAGMENTITERATOR_H

#include "modules/opdata/OpData.h"
#include "modules/opdata/src/OpDataFragment.h"


/**
 * Helper class for iterating over fragments in an OpData buffer.
 *
 * Use this class when you need access to the data fragments that make up an
 * OpData buffer. This can be useful e.g. when submitting an OpData object
 * across a socket/pipe, as you can write portions of the OpData sequentially
 * instead of calling Data(), which reallocates all the fragments into a single
 * buffer (this is expensive, and may fail because of OOM).
 *
 * The fragment iterator keeps its own reference to the OpData object, so any
 * manipulation of the OpData object passed to the constructor will not affect
 * (or be reflected in) the data returned from the iterator.
 *
 * How to use this class:
 *
 *  -# Call OpDataFragmentIterator(const OpData&) to retrieve an iterator object
 *     over a given OpData object.
 *  -# Call GetData() to get a data pointer to the first fragment.
 *  -# Call GetLength() to get the length of the first fragment.
 *  -# Call Next() to proceed to the next fragment.
 *  -# Call GetData() to get a data pointer to the second fragment.
 *  -# Call GetLength() to get the length of the second fragment.
 *  -# Repeat steps #4 to #6 to process all the fragments.
 *  -# When you proceed PAST the last fragment, Next() will return
 *     false. Subsequently, any GetData() call will return NULL, and
 *     any GetLength() call will return 0.
 *
 * When iterating a buffer in embedded mode (including an empty buffer), the
 * first fragment returned is the only fragment; the first call to Next() will
 * return false. Hence, an empty buffer will return the empty string from
 * GetData() before Next() is called, and NULL after Next() is called.
 */
class OpDataFragmentIterator
{
public:
	/**
	 * Create an empty fragment iterator.
	 */
	OpDataFragmentIterator()
		: m_current(NULL), m_valid(false)
	{
	}

	/** Create a fragment iterator over the given OpData buffer.
	 * @param data is the OpData to iterate.
	 * @param beginning specifies if the iterator shall start at the data's
	 *        first fragment (beginning == true) or at the last fragment
	 *        (beginning == false). */
	OpDataFragmentIterator(const OpData& data, bool beginning=true)
		: m_data(data)
		, m_current(data.IsEmbedded() ? NULL : (beginning ? data.n.first : data.n.last))
		, m_valid(true)
	{
	}

	/**
	 * Check if the iterator is still valid.
	 *
	 * @return \c true if the iterator is valid, \c false otherwise.
	 */
	bool IsValid() const { return m_valid; }

	/**
	 * Return pointer to data stored in the current fragment.
	 *
	 * The data returned is NOT '\0'-terminated. Use GetLength()
	 * to find the corresponding length of the returned data.
	 *
	 * @return Pointer to data stored in this fragment. NULL iff
	 *         the iterator has hit the end of the buffer.
	 */
	const char *GetData(void) const
	{
		if (!m_valid)
			return NULL;
		if (m_data.IsEmbedded())
			return m_data.e.data;

		OP_ASSERT(m_current);
		size_t offset = (m_current == m_data.n.first)
			? m_data.n.first_offset
			: 0;
		return m_current->Data() + offset;
	}

	/**
	 * Return length of the current fragment.
	 *
	 * @return The number of bytes stored in the current fragment.
	 *         0 iff the iterator has hit the end of the buffer.
	 */
	size_t GetLength(void) const
	{
		if (!m_valid)
			return 0;
		if (m_data.IsEmbedded())
			return m_data.Length();

		OP_ASSERT(m_current);
		size_t offset = (m_current == m_data.n.first)
			? m_data.n.first_offset
			: 0;
		size_t length = (m_current == m_data.n.last)
			? m_data.n.last_length
			: m_current->Length();
		OP_ASSERT(offset < length);
		return length - offset;
	}

	/** Returns true if the iterator can proceed to a next fragment. */
	bool HasNext() const
	{
		return IsValid() && !m_data.IsEmbedded() && m_current != m_data.n.last;
	}

	/**
	 * Proceed to the next fragment.
	 *
	 * @return \c true if the iterator has proceeded to a valid
	 *         fragment, \c false if we hit the end of the buffer.
	 */
	bool Next(void)
	{
		if (!IsValid())
			return false;
		if (m_data.IsEmbedded() || m_current == m_data.n.last)
		{
			m_valid = false;
			return false;
		}
		OP_ASSERT(m_current && m_current->Next());
		m_current = m_current->Next();
		return true;
	}

	/** Returns true if the iterator can proceed to a previous fragment. */
	bool HasPrev() const
	{
		return IsValid() && !m_data.IsEmbedded() && m_current != m_data.n.first;
	}

	/**
	 * Proceed to the previous fragment.
	 *
	 * @return \c true if the iterator has proceeded to a valid
	 *         fragment, \c false if we hit the end of the buffer.
	 */
	bool Prev(void)
	{
		if (!IsValid())
			return false;
		if (m_data.IsEmbedded() || m_current == m_data.n.first)
		{
			m_valid = false;
			return false;
		}
		OpDataFragment* prev = m_data.n.first;
		OP_ASSERT(prev && prev->Next());
		while (prev->Next() != m_current)
		{
			OP_ASSERT(prev != m_data.n.last);
			prev = prev->Next();
			OP_ASSERT(prev && prev->Next());
		}
		m_current = prev;
		return true;
	}

	/** Resets the fragment iterator so its current position is at the first
	 * (resp. last) OpDataFragment of the associated OpData.
	 *
	 * @param beginning Specifies whether the iterator shall be reset to the
	 *        first data fragment (beginning == true) or to the last data
	 *        fragment (beginning == false). */
	void Reset(bool beginning=true)
	{
		if (m_data.IsEmbedded())
			m_current = NULL;
		else
			m_current = beginning ? m_data.n.first : m_data.n.last;
		m_valid = true;
	}

	/**
	 * Operator overloading syntactic sugar.
	 *
	 * @{
	 */
	bool operator++(void) { return Next(); }
	bool operator--(void) { return Prev(); }
	operator bool(void) const { return IsValid(); }
	/** @} */

private:
	OpData m_data;
	OpDataFragment *m_current;
	bool m_valid;
};

/** Helper class for iterating over characters in OpData.
 *
 * Use this class when you need to iterate over all characters in an OpData
 * instance.
 * The iterator keeps its own reference of the OpData contents, so any
 * manipulation of the OpData instance passed to the constructor will not affect
 * (or be reflected in) the data returned from the iterator.
 *
 * The iterator can be used to iterate in forward direction (using Next() or
 * operator++()) or in backwards direction (using Previous() or
 * operator--()).
 *
 * Valid indices for the iterator are 0..OpData::Length(), where IsAtBeginning()
 * and IsAtEnd() check for the endpoints.
 *
 * @note The last index (OpData::Length()) acts as an end sentinel and is not a
 *       valid index of the OpData buffer. If OpData contains a string with
 *       OpData::Length() being the string-length, then the end sentinel
 *       corresponds to the terminating null-character.
 *
 * The iterator can be initialised to start at any index inside the string.
 *
 * Example to iterate from the start of the OpData buffer to its end:
 * \code
 * OpData data = ...;
 * for (OpDataCharIterator itr(data); itr; ++itr)
 * {
 *     char c = *itr;
 *     ...
 * }
 * \endcode
 *
 * Example to iterate from the end of the OpData buffer to its beginning:
 * \code
 * OpData data = ...;
 * OpDataCharIterator itr(data, data.Length());
 * while (--itr)
 * {
 *     char c = *itr;
 *     ...
 * }
 * \endcode
 */
class OpDataCharIterator {
public:
	/** Create an empty character iterator. */
	OpDataCharIterator()
		: m_current_data(NULL), m_current_length(0)
		, m_current_index(0), m_index(0) {}

	/** Create a character iterator over the given OpData buffer. */
	OpDataCharIterator(const OpData& data)
		: m_itr(data)
		, m_current_data(m_itr.GetData())
		, m_current_length(m_itr.GetLength())
		, m_current_index(0)
		, m_index(0) {}

	/** Create a character iterator over the specified OpData buffer and start
	 * the iterator at the specified index.
	 * @param data is the OpData buffer to iterate over.
	 * @param index is the index at which to start. If the index is greater than
	 *        the length of the data (e.g. OpDataUnknownLength), the iterator
	 *        starts at the end of the string. */
	OpDataCharIterator(const OpData& data, size_t index)
		: m_itr(data, index < data.Length())
		, m_current_data(m_itr.GetData())
		, m_current_length(m_itr.GetLength())
		, m_current_index(0)
		, m_index(0)
	{
		if (index >= data.Length())
		{	/* No need to iterate to the end, m_itr already points to the end,
			 * so we only need to adjust the m_index and m_current_index. */
			m_current_index = m_current_length;
			m_index = data.Length();
		}
		else if (0 < index)
			MoveToIndex(index);
	}

	/** Returns true if the iterator is at the beginning (index 0) of the
	 * data buffer, otherwise false. */
	op_force_inline bool IsAtBeginning() const { return m_index == 0; }

	/** Returns true if the iterator is at the end sentinel (index is past the
	 * last valid index in the data buffer), otherwise false. */
	op_force_inline bool IsAtEnd() const
		{ return m_current_index == m_current_length; }

	/** Proceed to the next character.
	 * If the iterator is already at the end sentinel, the iterator is not
	 * changed and this method returns false.
	 *
	 * @note When you call this method on an iterator for which IsAtEnd() was
	 *       false before calling the method and Next() returns false, then
	 *       IsAtEnd() is true after the call.
	 *
	 * @retval true if the iterator has proceeded to a valid character of the
	 *         OpData buffer.
	 * @retval false if the iterator has hit the end of the string.
	 */
	op_force_inline bool Next(void)
	{
		OP_ASSERT(m_current_index+1 > 0 || !"Integer overflow!");
		if (m_current_index+1 < m_current_length)
		{
			++m_current_index;
			++m_index;
			return true;
		}
		else
			return NextWithFragment();
	}

	/** Returns true if Next() can proceed to a valid character. */
	bool HasNext(void) const
	{
		OP_ASSERT(m_current_index+1 > 0 || !"Integer overflow!");
		return (m_current_index+1 < m_current_length) || m_itr.HasNext();
	}

	/** Proceed to the previous character.
	 *
	 * @note If the iterator is already at the beginning (IsAtBeginning() is
	 *       true when calling this method), then the iterator's position is not
	 *       changed and this method returns false.
	 *
	 * @retval true if the iterator has proceeded to a valid character.
	 * @retval false if the iterator was already at the beginning.
	 */
	bool Previous(void)
	{
		if (m_current_index > 0)
		{
			OP_ASSERT(IsAtEnd() || IsValid());
			OP_ASSERT(m_index > 0);
			--m_current_index;
			--m_index;
			OP_ASSERT(IsValid());
			return true;
		}
		else
			return PreviousWithFragment();
	}

	/** Returns true if Previous() can proceed to a valid character. */
	op_force_inline bool HasPrevious(void) const { return m_index > 0; }

	/** Return the character the iterator currently points at.
	 *
	 * @note If the iterator is no longer valid, then this method returns '\0',
	 *  but an OpData may contain valid '\0' characters. Therefore callers
	 *  should check IsValid() instead of assuming that a '\0' means
	 *  end-of-data.
	 */
	char At() const
	{
		OP_ASSERT(IsAtEnd() || IsValid());
		return IsAtEnd() ? '\0' : m_current_data[m_current_index];
	}

	/** Returns the current index of the iterator in the string.
	 *
	 * @note If the iterator is at the end sentinel, i.e., IsAtEnd() is true,
	 *  then the returned index is equal to OpData::Length() of the associated
	 *  data buffer.
	 * @note If the iterator is before the beginning of the string, (e.g., after
	 *  we called Previous() on an iterator that pointed to the first character
	 *  of a string) then the returned index is OpDataUnknownLength.
	 */
	size_t Index() const { return m_index; }

	/** Operator overloading syntactic sugar.
	 *
	 * @{
	 */
	bool operator++(void) { return Next(); }
	bool operator--(void) { return Previous(); }
	operator bool(void) const { return !IsAtEnd(); }
	char operator*(void) const { return At(); }
	/** @} */

private:
#ifdef DEBUG_ENABLE_OPASSERT
	/** Check if the iterator is still valid. The iterator is valid when it
	 * points to some character within the associated OpData buffer.
	 *
	 * @note If the iterator is at the beginning (IsAtBeginning()) and the
	 *  associated data is not empty, then the iterator is valid. If the
	 *  iterator is at the end sentinel, i.e., past the last valid index in the
	 *  string (IsAtEnd()), then the iterator is not valid.
	 *
	 * @return \c true if the iterator is valid, \c false otherwise. */
	op_force_inline bool IsValid() const
		{ return m_current_index < m_current_length && m_itr.IsValid(); }
#endif // DEBUG_ENABLE_OPASSERT

	/** Move the iterator forward to the specified index. The current index is
	 * expected to be lower than the specified index. */
	void MoveToIndex(size_t index);

	/** Called by Next() when the iterator is not moved within the current
	 * fragment. */
	bool NextWithFragment();

	/** Moves the internal iterator to the next fragment and sets
	 * m_current_length, m_current_data to the next fragment and m_current_index
	 * to 0. */
	void StepToNextFragment();

	/** Called by Previous() when the iterator currently points to the first
	 * character inside the current fragment.
	 * If there is any previous character (m_index > 0), then the internal
	 * iterator is moved to the previous fragment and m_current_length,
	 * m_current_data are set to that fragment's data and m_current_index to the
	 * last char in that fragment.
	 * If there is no previous character (m_index == 0), then nothing is changed
	 * and false is returned.
	 * @return true if the iterator was moved to a previous character.
	 */
	bool PreviousWithFragment();

	OpDataFragmentIterator m_itr;
	const char* m_current_data;	///< Data in the current fragment
	size_t m_current_length;	///< Length of the current fragment
	/** Current position relative to the current fragment. */
	size_t m_current_index;
	/** Current position relative to the complete buffer. */
	size_t m_index;
};

#endif /* MODULES_OPDATA_OPDATAFRAGMENTITERATOR_H */
