/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_OPDATA_UNISTRINGFRAGMENTITERATOR_H
#define MODULES_OPDATA_UNISTRINGFRAGMENTITERATOR_H

#include "modules/opdata/UniString.h"
#include "modules/opdata/OpDataFragmentIterator.h"


/**
 * Helper class for iterating over fragments in a Unicode string.
 *
 * Use this class when you need access to the data fragments that make up a
 * UniString object. This can be useful e.g. when analyzing the contents of a
 * UniString object, as you can look at individual uni_chars without calling
 * Data(), which reallocates all the fragments into a single buffer (this is
 * expensive, and may fail because of OOM).
 *
 * The fragment iterator keeps its own reference to the UniString object, so
 * any manipulation of the UniString object passed to the constructor will not
 * affect (or be reflected in) the data returned from the iterator.
 *
 * How to use this class:
 *
 *  -# Call UniStringFragmentIterator(string) to retrieve an iterator object
 *     over a given UniString object.
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
 * When iterating a string in embedded mode (including an empty string), the
 * first fragment returned is the only fragment; the first call to Next() will
 * return false. Hence, an empty string will return the empty string from
 * GetData() before Next() is called, and NULL after Next() is called.
 */
class UniStringFragmentIterator
{
public:
	/**
	 * Create an empty fragment iterator.
	 */
	UniStringFragmentIterator() : m_it() {}

	/** Create a fragment iterator over the given UniString object.
	 * @param data is the UniString to iterate.
	 * @param beginning specifies if the iterator shall start at the data's
	 *        first fragment (beginning == true) or at the last fragment
	 *        (beginning == false). */
	UniStringFragmentIterator(const UniString& data, bool beginning=true)
		: m_it(data.m_data, beginning) {}

	/**
	 * Check if the iterator is still valid.
	 *
	 * @return \c true if the iterator is valid, \c false otherwise.
	 */
	bool IsValid() const { return m_it.IsValid(); }

	/**
	 * Return pointer to string data stored in the current fragment.
	 *
	 * The data returned is NOT '\0'-terminated. Use GetLength()
	 * to find the corresponding length of the returned data.
	 *
	 * @return Pointer to string data stored in this fragment. NULL iff
	 *         the iterator has hit the end of the string.
	 */
	const uni_char *GetData(void) const
	{
		return reinterpret_cast<const uni_char *>(m_it.GetData());
	}

	/**
	 * Return length of the current fragment.
	 *
	 * @return The number of uni_chars stored in the current fragment.
	 *         0 iff the iterator has hit the end of the string.
	 */
	size_t GetLength(void) const
	{
		return UNICODE_DOWNSIZE(m_it.GetLength());
	}

	/** Returns true if the iterator can proceed to a next fragment. */
	bool HasNext() const { return m_it.HasNext(); }

	/**
	 * Proceed to the next fragment.
	 *
	 * @return true if the iterator has proceeded to a valid
	 *         fragment, false if we hit the end of the string.
	 */
	bool Next(void) { return m_it.Next(); }

	/** Returns true if the iterator can proceed to a previous fragment. */
	bool HasPrev() const { return m_it.HasPrev(); }

	/**
	 * Proceed to the previous fragment.
	 *
	 * @return \c true if the iterator has proceeded to a valid
	 *         fragment, \c false if we hit the end of the buffer.
	 */
	bool Prev(void) { return m_it.Prev(); }

	/** Resets the fragment iterator so its current position is at the first
	 * (resp. last) OpDataFragment of the associated UniString.
	 *
	 * @param beginning Specifies whether the iterator shall be reset to the
	 *        first data fragment (beginning == true) or to the last data
	 *        fragment (beginning == false). */
	void Reset(bool beginning=true) { m_it.Reset(beginning); }

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
	OpDataFragmentIterator m_it;
};

#endif /* MODULES_OPDATA_UNISTRINGFRAGMENTITERATOR_H */
