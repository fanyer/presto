/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_OTL_REVERSEITERATOR_H
#define MODULES_OTL_REVERSEITERATOR_H

/**
 * A general reverse iterator.
 *
 * This template class can be used to define a reverse iterator based on another
 * normal @c Iterator class. While the normal @c Iterator iterates from the
 * first element of a collection until the end, the @c OtlReverseIterator
 * iterates from the last element of a collection backwards to its beginning.
 *
 * The @c Iterator class on which the OtlReverseIterator is based must have the
 * following properties:
 *
 * - Define a default constructor, a copy constructor, an assignment operator.
 * - Define a typename @c Iterator::Reference, which is the reference of the
 *   type the @c Iterator iterates over. This is used as return type of the
 *   operator*(). If the @c Iterator iterates over a collection of elements of
 *   type @c T, then the @c Iterator::Reference is expected to be defined as
 *   @code
 *   class Iterator {
 *   public:
 *       typedef T& Reference;
 *   ...
 *   };
 *   @endcode
 * - Define a typename @c Iterator::ConstReference, which is the const reference
 *   of the type the @c Iterator iterates over. If the @c Iterator iterates over
 *   a collection of elements of type @c T, then the @c Iterator::ConstReference
 *   is expected to be defined as <code>typedef const T& ConstReference;</code>.
 * - Define a <code>Reference Iterator::operator*() const</code> which returns a
 *   reference of the current item.
 * - Define a typename @c Iterator::Pointer, which is the pointer to the type
 *   the @c Iterator iterates over. This is used as return type of the
 *   @c operator->(). If the @c Iterator iterates over a collection of elements
 *   of type @c T, then the @c Iterator::Pointer is expected to be defined as
 *   @code
 *   class Iterator {
 *   public:
 *       typedef T* Pointer;
 *   ...
 *   };
 *   @endcode
 * - Define a <code>Pointer Iterator::operator->() const</code> which returns
 *   the pointer to the current item.
 * - Define an equality operator <code>bool operator==(const Iterator&)
 *   const</code>. Which compares two @c Iterator instances.
 * - Define an pre-increment <code>Iterator::operator++()</code>. The
 *   pre-decrement @c operator--() of the OtlReverseIterator uses that operator.
 * - Define an pre-decrement <code>Iterator::operator--()</code>. The
 *   pre-increment @c operator++() of the OtlReverseIterator uses that operator.
 * - Define an post-increment <code>Iterator::operator++(int)</code>. The
 *   post-decrement @c operator--(int) of the OtlReverseIterator uses that
 *   operator.
 * - Define an post-decrement <code>Iterator::operator--(int)</code>. The
 *   post-increment @c operator++(int) of the OtlReverseIterator uses that
 *   operator.
 *
 * A collection using the reverse iterator should define two methods @c RBegin()
 * and @c REnd() which return an OtlReverseIterator pointing to the last element
 * of the collection and to the beginning of the collection.
 *
 * Example: Assume that a collection defines a normal @c Iterator and a
 * corresponding @c ConstIterator which can iterate from the first element of
 * the collection to the end of the collections. Assume further on that the
 * end-iterator is defined such that incrementing the iterator pointing to the
 * last element reaches the end and decrementing the iterator pointing to the
 * first element reaches the end (i.e. the end is the connector node of a
 * circular list). Then the collection class can use this reverse iterator like
 * @code
 * class Collection {
 * public:
 *     class ConstIterator;
 *
 *     class Iterator {
 *     public:
 *         typedef T& Reference;
 *         typedef T* Pointer;
 *         Iterator() { ... }
 *         Iterator(const Iterator& other) { ... }
 *         Iterator(... element) { ... }
 *         const Iterator& operator=(const Iterator& other) { ... }
 *         Reference Iterator::operator*() const { ... }
 *         Pointer Iterator::operator->() const { ... }
 *         bool operator==(const Iterator& other) const { ... }
 *         bool operator!=(const Iterator& other) const { ... }
 *         bool operator==(const ConstIterator& other) const { ... }
 *         bool operator!=(const ConstIterator& other) const { ... }
 *         Iterator& operator++() { ... }
 *         Iterator& operator--() { ... }
 *         Iterator operator++(int) { ... }
 *         Iterator operator--(int) { ... }
 *     };
 *
 *     class ConstIterator {
 *     public:
 *         typedef const T& Reference;
 *         typedef const T* Pointer;
 *         ConstIterator() { ... }
 *         ConstIterator(... element) { ... }
 *         ConstIterator(const ConstIterator& other) { ... }
 *         ConstIterator(const Iterator& other) { ... }
 *         const ConstIterator& operator=(const ConstIterator& other) { ... }
 *         const ConstIterator& operator=(const Iterator& other) { ... }
 *         Reference ConstIterator::operator*() const { ... }
 *         Pointer ConstIterator::operator->() const { ... }
 *         bool operator==(const ConstIterator& other) const { ... }
 *         bool operator!=(const ConstIterator& other) const { ... }
 *         bool operator==(const Iterator& other) const { ... }
 *         bool operator!=(const Iterator& other) const { ... }
 *         ConstIterator& operator++() { ... }
 *         ConstIterator& operator--() { ... }
 *         ConstIterator operator++(int) { ... }
 *         ConstIterator operator--(int) { ... }
 *     };
 *
 *     ConstIterator Begin() const { return ConstIterator(FirstElement()); }
 *     Iterator Begin() { return Iterator(FirstElement()); }
 *     ConstIterator End() const { ... }
 *
 *     typedef OtlReverseIterator<Iterator> ReverseIterator;
 *     typedef OtlReverseIterator<ConstIterator> ReverseConstIterator;
 *
 *     ReverseConstIterator RBegin() const { return ReverseConstIterator(LastElement()); }
 *     ReverseIterator RBegin() { return ReverseConstIterator(LastElement()); }
 *     ReverseConstIterator REnd() const { return ReverseConstIterator(End()); }
 *
 * };
 * @endcode
 * Then you can use the @c Iterator and @c ConstIterator to move forward in the
 * collection from the first element to the last element and you can use the
 * @c OtlReverseIterator<Iterator> and @c OtlReverseIterator<ConstIterator> to
 * move backwards in the collection from the last element to the first element:
 * @code
 * Collection my_collection = ...;
 *
 * Collection::Iterator itr;
 * for (itr = my_collection.Begin(); itr != my_collection.End(); ++itr)
 *    ...;
 *
 * Collection::ReverseIterator itr;
 * for (itr = my_collection.RBegin(); itr != my_collection.REnd(); ++itr)
 *    ...;
 * @endcode
 */
template<typename _Iterator>
class OtlReverseIterator
{
public:
	typedef _Iterator Iterator;

private:
	/**
	 * Iterating through this collection is referred to this instance in the
	 * opposite direction.
	 */
	Iterator m_iterator;

public:
	/// A reference of the type the @c Iterator iterates over.
	typedef typename _Iterator::Reference Reference;

	/**
	 * A const reference of the type the @c Iterator iterates over.
	 *
	 * If the iterator's Reference type is <code>T&</code>, then the iterator's
	 * ConstReference type should be defined as <code>const T&</code>
	 */
	typedef typename _Iterator::ConstReference ConstReference;

	/// A pointer to the type the @c Iterator iterates over.
	typedef typename _Iterator::Pointer Pointer;

	/**
	 * The default constructor uses the default constructor on the underlying
	 * @c Iterator.
	 */
	OtlReverseIterator() : m_iterator() {}

	/**
	 * This constructor creates a reverse iterator that points to the same item
	 * as the specified iterator.
	 */
	explicit OtlReverseIterator(const Iterator& i) : m_iterator(i) {}

	/**
	 * The copy constructor copies the Iterator from the specified other
	 * reverse iterator.
	 */
	OtlReverseIterator(const OtlReverseIterator& other)
		: m_iterator(other.m_iterator)
	{}

	/**
	 * The assignment operator copies the Iterator from the specified other
	 * reverse iterator.
	 */
	const OtlReverseIterator& operator=(const OtlReverseIterator& other)
	{
		m_iterator = other.m_iterator; return *this;
	}

	/**
	* This constructor is used to construct a reverse iterator from another
	 * compatible reverse iterator class. E.g. construct a reverse
	 * const-iterator from the corresponding reverse non-const-iterator.
	 *
	 * To use this constructor, the template type @c Iterator of this class must
	 * have a copy constructor which accepts the @c _CompatibleIterator as
	 * argument.
	 */
	template<typename _CompatibleIterator>
	OtlReverseIterator(const OtlReverseIterator<_CompatibleIterator>& other)
		: m_iterator(other.Base())
	{}

	/**
	* This assignment operator is used to construct a reverse iterator from
	 * another compatible reverse iterator class. E.g. assign a reverse
	 * const-iterator from the corresponding reverse non-const-iterator.
	 *
	 * To use this assignment operator, the template type @c Iterator of this
	 * class must have an assignment operator which accepts the
	 * @c _CompatibleIterator as argument.
	 */
	template<typename _CompatibleIterator>
	const OtlReverseIterator& operator=(const OtlReverseIterator<_CompatibleIterator>& other)
	{
		m_iterator = other.Base(); return *this;
	}

	/**
	 * Returns the @c Iterator on which the reverse iterator is based. This is
	 * used to construct or assign or compare a reverse iterator with a
	 * compatible other reverse iterator class.
	 */
	const Iterator& Base() const { return m_iterator; }

	/**
	 * The equality operator compares this instance with the specified other
	 * iterator. The two iterators are equal if the @c Iterator is equal to the
	 * other @c Iterator (or @c _CompatibleIterator). This reverse-iterator can
	 * be compared with
	 * - another reverse-iterator (with the same @c Iterator type),
	 * - an iterator of type @c Iterator,
	 * - a @c _CompatibleIterator, if the @c Iterator provides an equality
	 *   operator that can compare it with a @c _CompatibleIterator.
	 * - an @c OtlReverseIterator of a @c _CompatibleIterator.
	 *
	 * @param other is the iterator to compare with.
	 * @return true if this iterator equals the specified other iterator.
	 * @{
	 */
	bool operator==(const OtlReverseIterator& other) const
	{
		return m_iterator == other.m_iterator;
	}

	bool operator==(const Iterator& other) const
	{
		return m_iterator == other;
	}

	template<typename _CompatibleIterator>
	bool operator==(const OtlReverseIterator<_CompatibleIterator>& other) const
	{
		return m_iterator == other.Base();
	}

	template<typename _CompatibleIterator>
	bool operator==(const _CompatibleIterator& other) const
	{
		return m_iterator == other;
	}

	/** @} */

	/**
	 * The inequality operator compares this instance with the specified other
	 * iterator. The two iterators are different if the @c Iterator is not equal
	 * to the other @c Iterator (or @c _CompatibleIterator). This
	 * reverse-iterator can be compared with
	 * - another reverse-iterator (with the same @c Iterator type),
	 * - an iterator of type @c Iterator,
	 * - a @c _CompatibleIterator, if the @c Iterator provides an equality
	 *   operator that can compare it with a @c _CompatibleIterator.
	 * - an @c OtlReverseIterator of a @c _CompatibleIterator.
	 *
	 * @param other is the iterator to compare with.
	 * @return true if this iterator is not equal to the specified other
	 *  iterator.
	 * @{
	 */
	bool operator!=(const OtlReverseIterator& other) const
	{
		return !(*this == other);
	}

	bool operator!=(const Iterator& i) const
	{
		return !(*this == i);
	}

	template<typename _CompatibleIterator>
	bool operator!=(const OtlReverseIterator<_CompatibleIterator>& other) const
	{
		return !(*this == other);
	}

	template<typename _CompatibleIterator>
	bool operator!=(const _CompatibleIterator& other) const
	{
		return !(m_iterator == other);
	}
	/** @} */

	/**
	 * The pre-increment operator decrements the underlying @c Iterator and
	 * returns a reference to this instance.
	 */
	OtlReverseIterator& operator++() { --m_iterator; return *this; }

	/**
	 * The pre-decrement operator increments the underlying @c Iterator and
	 * returns a reference to this instance.
	 */
	OtlReverseIterator& operator--() { ++m_iterator; return *this; }

	/**
	 * The pre-increment operator decrements the underlying @c Iterator and
	 * returns an OtlReverseIterator that points to the previous position (before
	 * incrementing this iterator).
	 */
	OtlReverseIterator operator++(int)
	{
		return OtlReverseIterator(m_iterator--);
	}

	/**
	 * The pre-decrement operator increments the underlying @c Iterator and
	 * returns an OtlReverseIterator that points to the previous position (before
	 * decrementing this iterator).
	 */
	OtlReverseIterator operator--(int)
	{
		return OtlReverseIterator(m_iterator++);
	}

	/**
	 * The dereference operator returns a reference of the item, the underlying
	 * @c Iterator points to.
	 */
	Reference operator*() const { return *m_iterator; }

	/**
	 * The pointer operator returns a pointer of the item, the underlying
	 * @c Iterator points to.
	 */
	Pointer operator->() const { return &(operator*()); }
}; // OtlReverseIterator

#endif // MODULES_OTL_REVERSEITERATOR_H
