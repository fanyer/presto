/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_OTL_RANGE_H
#define MODULES_OTL_RANGE_H

/**
 * A generic range class that is based on iterators.
 *
 * A range can be used to iterate over a range [begin, end) of elements.
 *
 * Example:
 *
 * @code
 * OtlList<int> list = ...;
 * OtlRange< OtlList<int>::ConstIterator > range;
 * for (range(list.Begin(), list.End()); range; ++range)
 *     print("value: %d\n", *range);
 * @endcode
 *
 * @note A range is not a copy of the original data, it is only a view onto the
 *       data. If the code using a range modifies the current item (taken that
 *       \c _Iterator allows to modify the current item), then the item is
 *       modified in the original collection.
 *
 * @note It is possible to remove the current beginning of the range while
 *       iterating the range.
 */
template<typename _Iterator, typename _ConstIterator=_Iterator>
class OtlRange
{
public:
	/// The iterator type that is used for the beginning of the range.
	typedef _Iterator Iterator;

	/**
	 * The iterator type that is used for the end of the range.
	 *
	 * This iterator type must be compatible with _Iterator for comparison.
	 */
	typedef _ConstIterator ConstIterator;

	/// A reference of the type the \c Iterator iterates over.
	typedef typename _Iterator::Reference Reference;

	/**
	 * A const reference of the type the \c Iterator iterates over.
	 *
	 * If the iterator's Reference type is <code>T&</code>, then the iterator's
	 * ConstReference type should be defined as <code>const T&</code>.
	 */
	typedef typename _Iterator::ConstReference ConstReference;

	/// A pointer to the type the \c Iterator iterates over.
	typedef typename _Iterator::Pointer Pointer;

	/// The default constructor creates an empty range.
	OtlRange() { }

	/**
	 * Creates a range from the specified \c begin until (and excluding) the
	 * specified \c end.
	 *
	 * @note The caller must ensure that both \c begin and \c end are in the
	 *       same collection, i.e. that after some finite number of calls to
	 *       the ++ operator on \c begin, \c begin equals \c end.
	 *
	 * @note This condition also means that the specified \c end shall not be
	 *       removed from its collection.
	 *
	 * @param begin is the iterator that points to the first element of the
	 *              range.
	 * @param end   is the iterator that points past the last element of the
	 *              range. The end is excluded from the range.
	 */
	OtlRange(const _Iterator& begin, const _ConstIterator& end)
		: m_current(begin)
		, m_end(end)
	{ }

	/// Returns true if the range is empty.
	bool IsEmpty() const { return m_current == m_end; }

	/// Returns true if the range is not empty.
	operator bool() const { return !IsEmpty(); }

	/// Returns the current beginning of the range.
	Iterator Current() { return m_current; }
	const Iterator& Current() const { return m_current; }

	/// Returns the end of the range.
	const ConstIterator& End() const { return m_end; }

	/**
	 * The dereference operator returns a reference of the item at the
	 * beginning of the range.
	 *
	 * @note This method must only be called if the range is not empty.
	 */
	Reference operator*() const { OP_ASSERT(!IsEmpty()); return *m_current; }

	/**
	 * The pointer operator returns a pointer to the item at the beginning of
	 * the range.
	 *
	 * @note This method must only be called if the range is not empty.
	 */
	Pointer operator->() const { OP_ASSERT(!IsEmpty()); return &(operator*()); }

	/**
	 * The increment operator advances the Current() iterator by one.
	 *
	 * @note This method must only be called if the range is not empty.
	 * @return A reference to this range. Note that the range may be empty after
	 *         this operator was called.
	 */
	OtlRange& operator++() { OP_ASSERT(!IsEmpty()); ++m_current; return *this; }

	/**
	 * Search for the specified item in this range.
	 *
	 * If the item is found, true is returned and the Begin() of the range
	 * points to the found item. If the item is not found, false is returned
	 * and the range will be empty.
	 *
	 * @note This method uses O(n). Depending on the underlying collection it
	 *       may be better to use the collection's find method. E.g. a tree or
	 *       a hash table implement a search algorithm that is significantly
	 *       faster.
	 *
	 * @param item is the value to find.
	 * @return true if the specified item was found and false if it was not
	 *              found.
	 */
	bool FindItem(ConstReference item)
	{
		for (; !IsEmpty() && *(*this) != item; ++(*this)) {}
		return !IsEmpty();
	}

private:
	/// Points to the beginning of the range.
	Iterator m_current;

	/// Points to the end of the range.
	ConstIterator m_end;
}; // OtlRange

#endif // MODULES_OTL_RANGE_H
