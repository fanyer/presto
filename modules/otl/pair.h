/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_OTL_PAIR_H
#define MODULES_OTL_PAIR_H

/// A simple, lightweight pair template, not unlike std::pair.
template<typename T1, typename T2>
struct OtlPair
{
	T1 first;
	T2 second;

	OtlPair()
		: first(T1())
		, second(T2())
	{}

	OtlPair(const T1& x, const T2& y)
		: first(x)
		, second(y)
	{}

	template <class U1, class U2>
	OtlPair(const OtlPair<U1, U2>& other)
		: first(other.first)
		, second(other.second)
	{}
}; // OtlPair

template<typename T1, typename T2>
OtlPair<T1, T2> OtlMakePair(T1 x, T2 y)
{
	return OtlPair<T1, T2>(x, y);
}

#endif // MODULES_OTL_PAIR_H
