/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Lars Thomas Hansen
*/

#ifndef OP_BITVECTOR_H
#define OP_BITVECTOR_H

/** A bitvector holds single-bit values from 0 to INT_MAX.

    We represent it as a linear array of bits up to the highest
	bit set.  Watch it!  */

class OpBitvector
{
public:
	OpBitvector();
	~OpBitvector();

	/** Get bit at position n */
	int GetBit(int n);

	/** Set bit at position n to 0 or 1 */
	OP_STATUS SetBit(int n, int v);

	/** Set bits at positions first through last to 0 or 1 */
	OP_STATUS SetBits(int first, int last, int v);

private:
	/* Grow the data structure to allow n to be non-zero*/
	OP_STATUS Grow(int n);

	int high;			// highest+1 index represented by map
	UINT32 *map;		// pointer to heap-allocated data
};

inline int OpBitvector::GetBit(int n)
{
	if (n >= high)
		return 0;
	else
		return (map[n >> 5] >> (n & 31)) & 1;
}

/* Observe that v is often known at compile-time, so when inlined this
   function will generally be much smaller.  */

inline OP_STATUS OpBitvector::SetBit(int n, int v)
{
	if (v == 0)
		if (n >= high)
			return OpStatus::OK;
		else
			map[n >> 5] &= ~(1 << (n & 31));
	else
	{
		if (n >= high)
		{
			if (OpStatus::IsError(Grow(n)))
				return OpStatus::ERR_NO_MEMORY;
		}
		map[n >> 5] |= (1 << (n & 31));
	}
	return OpStatus::OK;
}

#endif // OP_BITVECTOR_H
