/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_OTL_SET_H
#define MODULES_OTL_SET_H

#include "modules/otl/src/red_black_tree.h"

/**
 * An implementation of a set.
 *
 * The set doesn't allow duplicates. Insertion, searching and removing is
 * guaranteed to have O(log(n)) time complexity.
 *
 * @tparam T          Type of stored object. T must be copy-constructible.
 *                    Doesn't have to be assignable.
 * @tparam Comparator Comparison functor. Takes two arguments of @a T type and
 *                    returns a boolean: true if the first argument is to be
 *                    placed at an earlier position than the second in a strict
 *                    weak ordering operation, false otherwise. This defaults
 *                    to the Less comparator which calls @a T's < operator.
 *
 * @see OtlRedBlackTree
 */
template<typename T, typename Comparator = Less<T> >
class OtlSet : public OtlRedBlackTree<T, Comparator>
{
public:
	typedef typename OtlRedBlackTree<T, Comparator>::Iterator Iterator;
	typedef typename OtlRedBlackTree<T, Comparator>::ConstIterator ConstIterator;
	typedef typename OtlRedBlackTree<T, Comparator>::ReverseIterator ReverseIterator;
	typedef typename OtlRedBlackTree<T, Comparator>::ReverseConstIterator ReverseConstIterator;
};

#endif // MODULES_OTL_SET_H
