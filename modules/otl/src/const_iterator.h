/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_OTL_CONSTITERATOR_H
#define MODULES_OTL_CONSTITERATOR_H
#include "modules/otl/src/reverse_iterator.h"

/**
 * Template for creating const iterators from normal iterators.
 *
 * The base iterator must define Pointer and Reference types. Usually, for
 * Iterator<T>, these would be T* and T&. The const iterator defines its own
 * ConstPointer and ConstReference types by prepending a const qualifier and
 * uses them as return types for operator-> and operator*, instead of Pointer
 * and Reference. Otherwise, the const iterator behaves as a normal iterator
 * and is comparable to one.
 */
template<typename Iterator>
class OtlConstIterator
{
public:
	typedef typename Iterator::ConstPointer Pointer;
	typedef typename Iterator::ConstReference Reference;
	typedef typename Iterator::ConstPointer ConstPointer;
	typedef typename Iterator::ConstReference ConstReference;

	OtlConstIterator() {}
	OtlConstIterator(const Iterator& base) : m_baseIter(base) {}

	OtlConstIterator operator++(int a) { return m_baseIter.operator ++(a); }
	OtlConstIterator& operator++() { ++m_baseIter; return *this; }
	OtlConstIterator operator--(int a) { return m_baseIter.operator --(a); }
	OtlConstIterator& operator--() { --m_baseIter; return *this; }

	ConstReference operator*() const { return m_baseIter.operator *(); }
	ConstPointer operator->() const { return m_baseIter.operator ->(); }


	bool operator==(const Iterator& other) const
	{
		return m_baseIter == other;
	}

	bool operator!=(const Iterator& other) const
	{
		return m_baseIter != other;
	}

	bool operator==(const OtlConstIterator& other) const
	{
		return m_baseIter == other.m_baseIter;
	}

	bool operator!=(const OtlConstIterator& other) const
	{
		return m_baseIter != other.m_baseIter;
	}

	template<typename CompatibleIterator>
	bool operator==(const OtlReverseIterator<CompatibleIterator>& other) const
	{
		return *this == other.Base();
	}

	template<typename CompatibleIterator>
	bool operator!=(const OtlReverseIterator<CompatibleIterator>& other) const
	{
		return *this != other.Base();
	}
private:
	Iterator m_baseIter;
}; // OtlConstIterator

#endif // MODULES_OTL_CONSTITERATOR_H
