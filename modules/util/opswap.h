/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_UTIL_OP_SWAP_H
#define MODULES_UTIL_OP_SWAP_H

/**
 * Swaps values of two variables.
 *
 * Assigns the contents of @a x to @a y and the contents of @a y to @a x.
 *
 * This is an equivalent of std::swap().
 *
 * @tparam T variable type.  Unless there is a specialization for a given type
 * 		@a T, it must be assignable.
 * @param x a variable
 * @param y another variable
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
template <typename T>
inline void op_swap(T& x, T& y)
{
	T tmp(x);
	x = y;
	y = tmp;
}

#endif // MODULES_UTIL_OP_SWAP_H
