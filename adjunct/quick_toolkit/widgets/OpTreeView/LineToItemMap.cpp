/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr)
 */

#include "core/pch.h"

#include "LineToItemMap.h"

OP_STATUS LineToItemMap::AddItem(INT32 new_value, UINT32 num_lines)
{
	// find the right place to insert it
	INT32 index = BinarySearch(new_value);

	for (UINT32 i = 0; i < num_lines; ++i)
	{
		m_lines.Insert(index + i, new_value - (index + i));
	}

	if (num_lines > 1)
	{
		for (UINT32 i = index + num_lines; i < GetCount(); ++i)
		{
			// several lines were added, need to correct 
			m_lines.Replace(i, m_lines.Get(i) - (num_lines - 1));
		}
	}

	return OpStatus::OK;
}

OP_STATUS LineToItemMap::InsertLines(INT32 new_value, UINT32 num_lines)
{
	// find the right place to insert it
	INT32 index = BinarySearch(new_value);

	if (new_value == GetByIndex(index))
	{
		// already inserted
		return OpStatus::OK;
	}
	
	// now we know where to insert, insert all the lines
	for (UINT32 j = 0; j < num_lines; ++j)
	{
		m_lines.Insert(index + j, new_value - (index + j));
	}

	// decrease all values in the lines after the inserted lines
	for (UINT32 k = index + num_lines; k < GetCount(); ++k)
	{
		m_lines.Replace(k, m_lines.Get(k) - num_lines);
	}
	return OpStatus::OK;
}


OP_STATUS LineToItemMap::InsertItem(INT32 new_value, UINT32 num_lines)
{
	if (!m_insertion_mode)
		return OpStatus::ERR;
	// find the right place to insert it
	UINT32 index = m_end_index;
	if (m_end_index == 0)
		index = BinarySearch(new_value);

	if (new_value == GetByIndex(index) - (index < GetCount()) ? m_num_lines : 0)
	{
		// already inserted
		return OpStatus::OK;
	}

	// now we know where to insert, insert all the lines
	for (UINT32 j = 0; j < num_lines; ++j)
	{
		m_lines.Insert(index + j, new_value - (index + j));
	}

	m_end_index = index + num_lines ;
	m_num_lines += num_lines;
	return OpStatus::OK;
}


void LineToItemMap::EndInsertion()
{
	if (!m_insertion_mode)
		return;
	// decrease all values in the lines after the inserted lines
	for (UINT32 k = m_end_index; k < GetCount(); ++k)
	{
		m_lines.Replace(k, m_lines.Get(k) - m_num_lines);
	}
	m_insertion_mode = false;
}



void LineToItemMap::RemoveLines(INT32 value_start, INT32 value_end, INT32 offset)
{
	OP_ASSERT(value_start <= value_end);
	INT32 index = BinarySearch(value_start);
	INT32 count = 0;

	while (index + count < (int)GetCount() && GetByIndex(index + count) <= value_end)
		count++;

	m_lines.Remove(index, count);

	if (count == offset)
		return;

	for (UINT32 i = index; i < GetCount(); i++)
		m_lines.Replace(i, m_lines.Get(i) + count - offset);
}


INT32 LineToItemMap::GetByIndex(UINT32 index) const 
{
	if (GetCount() > index)
		return m_lines.Get(index) + index; 
	else 
		return -1;
}


INT32 LineToItemMap::Find(INT32 item)
{
	// often the value of the item is at the index, try that first
	INT32 temp;
	if ((temp = GetByIndex(item)) == item)
	{
		while (temp > 0 && GetByIndex(temp-1) == item) --temp;
		return temp;
	}
	else
	{
		INT32 index = BinarySearch(item);

		if (item == GetByIndex(index))
			return index;
	}

	return -1;
}

INT32 LineToItemMap::BinarySearch(INT32 item_to_find)
{
	if (GetCount() == 0 || item_to_find > GetByIndex(GetCount() - 1)) 
		return GetCount();

	INT32 mid, max = GetCount()-1, min = 0;
	do
	{
		mid = (min + max) / 2;

		if ( item_to_find > GetByIndex(mid))
			min = mid + 1;
		else
			max = mid - 1;
	}
	while (min <= max);
	
	return min;
}
