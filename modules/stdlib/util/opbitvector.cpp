/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Lars Thomas Hansen
*/

#include "core/pch.h"
#include "modules/stdlib/util/opbitvector.h"

OpBitvector::OpBitvector()
	: high(-1)
	, map(NULL)
{
}

OpBitvector::~OpBitvector()
{
	delete [] map;
}

OP_STATUS OpBitvector::Grow(int n)
{
	int h = (n + 32) & ~31;	// h represents a number of bits
	if (high < 0)
		high = 0;
	if (h > high)
	{
		UINT32 *newmap = new UINT32[h / 32];
		if (newmap == NULL)
			return OpStatus::ERR_NO_MEMORY;
		if (map != NULL)
			op_memcpy(newmap, map, high / 8);
		op_memset(newmap + high / 32, 0, (h - high) / 8);
		delete [] map;
		map = newmap;
		high = h;
	}
	return OpStatus::OK;
}

OP_STATUS OpBitvector::SetBits(int first, int last, int v)
{
	if (last >= high)
	{
		if (OpStatus::IsError(Grow(last)))
			return OpStatus::ERR_NO_MEMORY;
	}

	int j = first;
	while (j <= last && (j & 31) != 0)
	{
		SetBit(j, v);
		j++;
	}

	UINT32 bits = v ? ~0 : 0;
	while (j+31 <= last)
	{
		OP_ASSERT((j & 31) == 0);
		map[j >> 5] = bits;
		j += 32;
	}

	while (j <= last)
	{
		SetBit(j, v);
		j++;
	}

	return OpStatus::OK;
}
