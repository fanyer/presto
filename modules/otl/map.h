/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_OTL_MAP_H
#define MODULES_OTL_MAP_H

#include "modules/otl/pair.h"
#include "modules/otl/src/red_black_tree.h"

/**
 * Comparator for OtlMap that takes only the key into consideration.
 *
 * This comparator ensures uniqueness and proper ordering of keys within the
 * map.
 *
 * @tparam KeyValuePair  Pair of key and value types. Key is first, value is
 *                       second.
 * @tparam KeyComparator Comparison functor. Takes two arguments of key's type
 *                       and returns a boolean: true if the first argument is to
 *                       be placed at an earlier position than the second in a
 *                       strict weak ordering operation, false otherwise. This
 *                       defaults to the Less comparator which calls the key's
 *                       < operator.
 */
template<typename KeyValuePair, typename KeyComparator>
struct OtlMapComparator
{
	OtlMapComparator(const KeyComparator& keyComparator)
		: keyComparator(keyComparator) {}

	bool operator()(const KeyValuePair& o1, const KeyValuePair& o2) const
	{
		return keyComparator(o1.first, o2.first);
	}
private:
	KeyComparator keyComparator;
}; // OtlMapComparator

/**
 * Associative map.
 *
 * Associative mapping of keys to values. Each key must be unique and have
 * a defined ordering relation (by default operator <). Guarantees O(log(n))
 * time complexity for insertion, removal and search operations.
 *
 * @tparam Key           Type of the key. Must be copy-constructible.
 * @tparam Value         Type of the value. Must be copy-constructible and
 *                       default-constructible.
 * @tparam KeyComparator Comparison functor. Takes two arguments of type
 *                       const Key& and returns a boolean: true if the first
 *                       argument is to be placed at an earlier position than
 *                       the second in a strict weak ordering operation, false
 *                       otherwise. This defaults to the Less comparator which
 *                       calls the key's < operator.
 */
template<typename Key, typename Value, typename KeyComparator = Less<const Key> >
class OtlMap
	: public OtlRedBlackTree< OtlPair<const Key, Value>
	                        , OtlMapComparator<OtlPair<const Key, Value>, KeyComparator> >
{
public:
	typedef OtlPair<const Key, Value> KeyValuePair;
	typedef OtlMapComparator<KeyValuePair, KeyComparator> Comparator;
	typedef OtlRedBlackTree<KeyValuePair, Comparator> RBTree;
	typedef typename RBTree::Iterator Iterator;
	typedef typename RBTree::ConstIterator ConstIterator;
	typedef typename RBTree::ReverseIterator ReverseIterator;
	typedef typename RBTree::ReverseConstIterator ReverseConstIterator;

	OtlMap(const KeyComparator& keyComparator = KeyComparator())
		: RBTree(Comparator(keyComparator)) {}

	Iterator Find(const Key& key) { return RBTree::Find(fromKey(key)); }
	ConstIterator Find(const Key& key) const { return RBTree::Find(fromKey(key)); }

	OP_STATUS Insert(const Key& key,
	                 const Value& value,
	                 Iterator* insertionPositionOut = NULL)
	{
		return RBTree::Insert(OtlMakePair(key, value), insertionPositionOut);
	}

	size_t Erase(const Key& key) { return RBTree::Erase(fromKey(key)); }
	size_t Count(const Key& key) const { return RBTree::Count(fromKey(key)); }

	using RBTree::Count;
	using RBTree::Insert;
	using RBTree::Erase;

private:
	static KeyValuePair fromKey(const Key& key)
	{
		return OtlMakePair(key, Value());
	}
}; // OtlMap

#endif // MODULES_OTL_MAP_H
