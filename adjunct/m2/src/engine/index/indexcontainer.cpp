/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/index/indexcontainer.h"

OP_STATUS IndexContainer::AddIndex(index_gid_t id, Index* index)
{
	OpAutoVector<Index>& indexes = m_index_types[GetType(id)];
	const size_t pos = GetPos(id);

	// Make sure there is place for new index
	for (unsigned i = indexes.GetCount(); i <= pos; i++)
		RETURN_IF_ERROR(indexes.Add(0));

	// Now insert the index
	Index* existing_index = indexes.Get(pos);
	if (existing_index == index)
		return OpStatus::OK;

	if (existing_index)
	{
		OP_DELETE(existing_index);
		OP_ASSERT(0);
	}
	else
		m_count++;

	return indexes.Replace(pos, index);
}

void IndexContainer::DeleteIndex(index_gid_t id)
{
	Index* existing_index = GetIndex(id);
	if (!existing_index)
		return;

	OP_DELETE(existing_index);
	OpStatus::Ignore(m_index_types[GetType(id)].Replace(GetPos(id), 0));
	m_count--;
}

Index* IndexContainer::GetRange(UINT32 range_start, UINT32 range_end, INT32& iterator)
{
	if (iterator < 0)
		iterator = range_start;

	while ((unsigned)iterator < range_end)
	{
		size_t type = GetType(iterator);
		size_t pos = GetPos(iterator);
		size_t endpos;
		Index* index = 0;
		OpAutoVector<Index>& indexes = m_index_types[type];

		// if end of range is in current type, don't search past end of range
		if (GetType(range_end) == type)
			endpos = min(GetPos(range_end), indexes.GetCount());
		else
			endpos = indexes.GetCount();

		// Search for index within current type
		while (pos < endpos && !index)
		{
			index = indexes.Get(pos);
			pos++;
		}

		// Update iterator so that it can be used next time
		iterator = pos + type * IndexTypes::ID_SIZE;

		if (index)
			return index;

		// Progress to next type if within range
		if (type + 1 <= GetType(range_end))
			iterator = (type + 1) * IndexTypes::ID_SIZE;
		else
			break;
	}

	return 0;
}

#endif // M2_SUPPORT
