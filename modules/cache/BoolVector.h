// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef BOOLVECTOR_H
#define BOOLVECTOR_H

#include "modules/search_engine/VectorBase.h"

/**
 * @brief bit vector holding BOOL variables
 * @author Pavel Studeny <pavels@opera.com>
 */
class BoolVector : private VectorBase
{
public:
	/**
	 * no destructor for T, comparison uses a default operator<
	 */
	BoolVector(void) : VectorBase(DefDescriptor<UINT32>()) {}

	/**
	 * @param size number of items to reserve space for in advance
	 */
	OP_STATUS Reserve(UINT32 size)
	{
		UINT32 prev_size = VectorBase::GetSize();
		RETURN_IF_ERROR(VectorBase::Reserve((size + 0x1F) >> 5));
		if (size <= prev_size)
			return OpStatus::OK;
		op_memset(((UINT32 *)VectorBase::Ptr()) + prev_size, 0, (VectorBase::GetSize() - prev_size) * sizeof(UINT32));
		VectorBase::SetCount(VectorBase::GetSize());
		return OpStatus::OK;
	}

	/**
	 * destruct all data and delete them from the Vector
	 */
	void Clear(void) {VectorBase::Clear();}

	/**
	 * delete a range of items from the beginning, shift the rest
	 */
	void Delete(UINT32 count)
	{
		int shift, i, size;

		if (count == 0)
			return;

		size = VectorBase::GetSize();
		if ((int)(count >> 5) >= size)
		{
			Clear();
			return;
		}

		shift = count & 0x1F;
		if (shift == 0)
		{
			count >>= 5;
			for (i = 0; i < (int)(size - count); ++i)
				*(UINT32 *)VectorBase::Get(i) = *(UINT32 *)VectorBase::Get(i + count);
			return;
		}

		--size;
		count >>= 5;
		for (i = 0; i < (int)(size - count); ++i)
			*(UINT32 *)VectorBase::Get(i) = (*(UINT32 *)VectorBase::Get(i + count) >> shift) | (*(UINT32 *)VectorBase::Get(i + count + 1) << (32 - shift));
		*(UINT32 *)VectorBase::Get(i) = *(UINT32 *)VectorBase::Get(i + count) >> shift;
	}

	/**
	 * Set value on the given position
	 */
	void Set(UINT32 idx, BOOL item)
	{
		if ((idx >> 5) >= VectorBase::GetSize())
			return;
		UINT32 ui = ((*(UINT32 *)VectorBase::Get(idx >> 5)) & ~(1 << (idx & 0x1F))) | ((item == 1) << (idx & 0x1F));
		VectorBase::Replace(idx >> 5, &ui);
	}

	/**
	 * Get value on the given position
	 */
	BOOL Get(UINT32 idx) const {return (idx >> 5) >= VectorBase::GetSize() ? FALSE : ((*(UINT32 *)VectorBase::Get(idx >> 5)) >> (idx & 0x1F)) & 1;}

	/**
	 * @return position of the first TRUE/FALSE or <count of values> + 1 if not found
	 */
	INT32 FindFirst(BOOL item) const
	{
		UINT32 i = 0, j = 0;
		UINT32 key = item ? 0 : (UINT32)-1;

		if (VectorBase::GetSize() == 0)
			return 0;
		while (i < VectorBase::GetSize() && *(UINT32 *)VectorBase::Get(i) == key)
			++i;
		if (i >= VectorBase::GetSize())
			return (i << 5) + 1;
		item = (item != FALSE);  // just for case
		key = *(UINT32 *)VectorBase::Get(i);
		while ((BOOL)(key & 1) != item)
		{
			++j;
			key >>= 1;
			OP_ASSERT(j <= 0x1F);  // infinite loop?
		}
		return (i << 5) | j;
	}

	BOOL operator[](UINT32 idx) const {return Get(idx);}
};

#endif  // BOOLVECTOR_H

