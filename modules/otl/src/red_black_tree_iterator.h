/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_OTL_REDBLACKTREEITERATOR_H
#define MODULES_OTL_REDBLACKTREEITERATOR_H
#include "modules/otl/src/red_black_tree_node.h"


template<typename T>
class OpConstRedBlackTreeIterator;

/**
 * Iterator for moving around in an OtlRedBlackTree.
 *
 * Increment the iterator to move in-order, and decrement to move in reverse-order
 * around the tree. The only way to obtain a valid OtlRedBlackTreeIterator is to
 * use an OtlRedBlackTree method that returns one.
 *
 * Note that modifying the object referenced by the iterator in a way that alters
 * ordering within the tree is likely to break the tree's integrity. The reference
 * is non-const to allow modifications in general. It is impossible for the iterator
 * to know which changes may alter the ordering.
 *
 * @tparam T Type of the object stored in the OtlRedBlackTree.
 */
template<typename T>
class OtlRedBlackTreeIterator
{
public:
	typedef OtlRedBlackTreeNode<T> Node;
	typedef T& Reference;
	typedef const T& ConstReference;
	typedef T* Pointer;
	typedef const T* ConstPointer;
	OtlRedBlackTreeIterator(Node* n = NULL);

	OtlRedBlackTreeIterator operator++(int);
	OtlRedBlackTreeIterator& operator++();
	OtlRedBlackTreeIterator operator--(int);
	OtlRedBlackTreeIterator& operator--();

	T& operator*() const;
	T* operator->() const;
	bool operator==(const OtlRedBlackTreeIterator& other) const;
	bool operator!=(const OtlRedBlackTreeIterator& other) const;
	template<typename CompatibleIterator>
	bool operator==(const CompatibleIterator& other) const
	{
		return other == *this;
	}

	template<typename CompatibleIterator>
	bool operator!=(const CompatibleIterator& other) const
	{
		return other != *this;
	}

#if defined(RBTREE_DEBUG) || defined(SELFTEST)
	Node* nodePtr() const;
#endif // defined(RBTREE_DEBUG) || defined(SELFTEST)
protected:
	Node* NextNode() const;
	Node* PreviousNode() const;
	/// Pointer to current element.
	Node* ptr;
}; // OtlRedBlackTreeIterator

// OtlRedBlackTreeIterator implementation

template<typename T> inline
OtlRedBlackTreeIterator<T>::OtlRedBlackTreeIterator(Node* n)
	: ptr(n)
{ }

template<typename T> inline
typename OtlRedBlackTreeIterator<T>::Node* OtlRedBlackTreeIterator<T>::NextNode() const
{
	return ptr ? ptr->Next() : NULL;
}

template<typename T> inline
typename OtlRedBlackTreeIterator<T>::Node* OtlRedBlackTreeIterator<T>::PreviousNode() const
{
	return ptr ? ptr->Previous() : NULL;
}

template<typename T> inline
OtlRedBlackTreeIterator<T> OtlRedBlackTreeIterator<T>::operator++(int)
{
	OtlRedBlackTreeIterator tmp = *this;
	*this = OtlRedBlackTreeIterator(NextNode());
	return tmp;
}

template<typename T> inline
OtlRedBlackTreeIterator<T>& OtlRedBlackTreeIterator<T>::operator++()
{
	ptr = NextNode();
	return *this;
}

template<typename T> inline
OtlRedBlackTreeIterator<T> OtlRedBlackTreeIterator<T>::operator--(int)
{
	OtlRedBlackTreeIterator tmp = *this;
	*this = OtlRedBlackTreeIterator(PreviousNode());
	return tmp;
}

template<typename T> inline
OtlRedBlackTreeIterator<T>& OtlRedBlackTreeIterator<T>::operator--()
{
	ptr = PreviousNode();
	return *this;
}

template<typename T> inline
T* OtlRedBlackTreeIterator<T>::operator->() const
{
	OP_ASSERT(ptr);
	return &(ptr->item);
}

template<typename T> inline
T& OtlRedBlackTreeIterator<T>::operator*() const
{
	OP_ASSERT(ptr);
	return ptr->item;
}

template<typename T> inline
bool OtlRedBlackTreeIterator<T>::operator==(const OtlRedBlackTreeIterator& other) const
{
	return other.ptr == ptr;
}

template<typename T> inline
bool OtlRedBlackTreeIterator<T>::operator!=(const OtlRedBlackTreeIterator& other) const
{
	return other.ptr != ptr;
}

#if defined(RBTREE_DEBUG) || defined(SELFTEST)
template<typename T> inline
typename OtlRedBlackTreeIterator<T>::Node* OtlRedBlackTreeIterator<T>::nodePtr() const
{
	return ptr;
}
#endif // defined(RBTREE_DEBUG) || defined(SELFTEST)

#endif // MODULES_OTL_REDBLACKTREEITERATOR_H
