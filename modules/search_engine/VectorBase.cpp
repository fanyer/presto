/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef SEARCH_ENGINE

#include "modules/search_engine/VectorBase.h"

#define ALLOC_STEP_SIZE 12
#define MAX_STEP_SIZE   1024

// the size doubles for small numbers and increases with a constant for high numbers
#define alloc_size(current_size) ((current_size) << 1) / (1 + (current_size) / MAX_STEP_SIZE) + ((current_size) + MAX_STEP_SIZE) / (1 + MAX_STEP_SIZE / ((current_size) + 1))




VectorBase::VectorBase(const TypeDescriptor &allocator) : m_allocator(allocator)
{
	m_items = NULL;
	m_size = 0;
	m_count = 0;
}

OP_STATUS VectorBase::DuplicateOf(const VectorBase& vec)
{
	char* new_items;
	unsigned i;
	
	if (m_count > 0 || m_size < vec.m_size)
	{
		RETURN_OOM_IF_NULL(new_items = OP_NEWA(char, vec.m_size * m_allocator.size));
		Clear();
	}
	else  // Reserve() can be used before
		new_items = m_items;

	for (i = 0; i  < vec.m_count; ++i)
		RETURN_IF_ERROR(m_allocator.Assign(new_items + i * m_allocator.size, vec.m_items + i * vec.m_allocator.size));

	m_items = new_items;
	m_size = vec.m_size;
	m_count = vec.m_count;

	return OpStatus::OK;
}

void VectorBase::TakeOver(VectorBase &vec)
{
	Clear();

	m_items = vec.m_items;
	vec.m_items = NULL;

	m_count = vec.m_count;
	vec.m_count = 0;

	m_size = vec.m_size;
	vec.m_size = 0;
}

void VectorBase::Clear(void)
{

	if (m_items != NULL)
	{
		unsigned i;
		for (i = 0; i < m_count; ++i)
			m_allocator.Destruct(m_items + i * m_allocator.size);

		OP_DELETEA(m_items);
		m_items = NULL;
	}
	m_size = 0;
	m_count = 0;
}

OP_STATUS VectorBase::Reserve(UINT32 size)
{
	if (m_size >= size)
		return OpStatus::OK;

	return Grow(size, m_count);
}

OP_STATUS VectorBase::SetCount(UINT32 count)
{
	RETURN_IF_ERROR(Reserve(count));

	if ((m_count = count) == 0)
		Clear();

	return OpStatus::OK;
}

OP_STATUS VectorBase::Replace(UINT32 idx, const void *item)
{
	m_allocator.Destruct(m_items + idx * m_allocator.size);
	return m_allocator.Assign(m_items + idx * m_allocator.size, item);
}

OP_STATUS VectorBase::Insert(UINT32 idx, const void *item)
{
	if (m_count >= m_size)
	{
		RETURN_IF_ERROR(Grow(m_size == 0 ? ALLOC_STEP_SIZE : alloc_size(m_size), idx));
	}
	else if (idx < m_count)
		op_memmove(m_items + (idx + 1) * m_allocator.size, m_items + idx * m_allocator.size, (m_count - idx) * m_allocator.size);

	RETURN_IF_ERROR(m_allocator.Assign(m_items +idx * m_allocator.size, item));
	++m_count;

	return OpStatus::OK;
}

OP_STATUS VectorBase::Insert(const void *item)
{
	UINT32 idx;

	if (m_count == 0)
		return Insert(0, item);

	idx = Search(item);

	if (idx >= m_count || m_allocator.Compare(item, m_items + idx * m_allocator.size))  // not in the vector yet
		return Insert(idx, item);

	return OpStatus::OK;  // this item is already inserted
}

BOOL VectorBase::RemoveByItem(const void *item)
{
	INT32 idx;
	
	if ((idx = Find(item)) == -1)
		return FALSE;

	Remove(NULL, idx);
	return TRUE;
}

void VectorBase::Remove(void *item, INT32 idx)
{
	UINT32 new_count;
	BOOL shrink_ok = FALSE;

	if (idx < 0)
		idx = m_count + idx;

	if (item != NULL)
		op_memcpy(item, m_items + idx * m_allocator.size, m_allocator.size);
	new_count = m_count - 1;

	if (alloc_size(new_count) < m_size && new_count >= ALLOC_STEP_SIZE)
		shrink_ok = OpStatus::IsSuccess(Shrink(new_count, idx));

	if (new_count == 0)
	{
		OP_DELETEA(m_items);
		m_items = NULL;
		m_size = 0;
	}
	else if (!shrink_ok && (UINT32)idx < new_count)
		op_memmove(m_items + idx * m_allocator.size, m_items + (idx + 1) * m_allocator.size, (new_count - idx) * m_allocator.size);

	m_count = new_count;
}

BOOL VectorBase::DeleteByItem(void *item)
{
	INT32 idx;
	
	if ((idx = Find(item)) == -1)
		return FALSE;

	Delete(idx);
	return TRUE;
}

void VectorBase::Delete(INT32 idx, UINT32 count)
{
	int i;
	UINT32 new_count;
	BOOL shrink_ok = FALSE;

	if (idx < 0)
		idx = m_count + idx;

	for (i = idx; i < idx + (INT32)count; ++i)
		m_allocator.Destruct(m_items + i * m_allocator.size);

	new_count = m_count - count;

	if (alloc_size(new_count) < m_size && new_count >= ALLOC_STEP_SIZE)
		shrink_ok = OpStatus::IsSuccess(Shrink(new_count, idx, count));

	if (new_count == 0)
	{
		OP_DELETEA(m_items);
		m_items = NULL;
		m_size = 0;
	}
	else if (!shrink_ok && idx < (INT32)m_count)
		op_memmove(m_items + idx * m_allocator.size, m_items + (idx + count) * m_allocator.size, (new_count - idx) * m_allocator.size);

	m_count = new_count;
}

INT32 VectorBase::Find(const void *item) const
{
	UINT32 i;

	for (i = 0; i < m_count; ++i)
	{
		if (!m_allocator.Compare(m_items + i * m_allocator.size, item) && !m_allocator.Compare(item, m_items + i * m_allocator.size))
			return i;
	}

	return -1;
}

OP_STATUS VectorBase::Sort(void)
{
	unsigned count;
	unsigned i;
	char *temp_array;

	if (m_count <= 1)
		return OpStatus::OK;

	temp_array = OP_NEWA(char, m_count * m_allocator.size);
	if (temp_array == NULL)
	{  // fallback for out of memory - bubble sort
		unsigned j, k;
		char tmp_T;

		i = 1;
		while (i < m_count)
		{
			j = 1;
			while (j <= m_count - i)
			{
				if (m_allocator.Compare(m_items + j * m_allocator.size, m_items + (j - 1) * m_allocator.size))
				{
					for (k = 0; k < m_allocator.size; ++k)
					{
						tmp_T = (m_items + (j - 1) * m_allocator.size)[k];
						(m_items + (j - 1) * m_allocator.size)[k] = (m_items + j * m_allocator.size)[k];
						(m_items + j * m_allocator.size)[k] = tmp_T;
					}
				}
				else if (!m_allocator.Compare(m_items + (j - 1) * m_allocator.size, m_items + j * m_allocator.size))  // items are equal
				{
					Delete(j);
					--j;
				}
				++j;
			}
			++i;
		}

		return OpStatus::OK;
	}

	op_memcpy(temp_array, m_items, m_count * m_allocator.size);

	count = MergeSort(temp_array, m_items, 0, m_count - 1);

	if (alloc_size(count) < m_size && count >= ALLOC_STEP_SIZE)
		RETURN_IF_ERROR(Shrink(count, count, m_count - count));

	m_count = count;

	OP_DELETEA(temp_array);
	
	return OpStatus::OK;
}

INT32 VectorBase::Search(const void *item, UINT32 start, UINT32 end) const
{
	int n2;

	while (end > start)
	{
		n2 = (end - start) / 2;
		if (m_allocator.Compare(m_items + (start + n2) * m_allocator.size, item))
			start = start + n2 + 1;
		else
			end = start + n2;
	}

	return start;
}

OP_STATUS VectorBase::Grow(UINT32 new_size, UINT32 hole)
{
	char* new_items;
	
	RETURN_OOM_IF_NULL(new_items = OP_NEWA(char, new_size * m_allocator.size));

	if (m_size > 0)
	{
		// Copy elements before item.
		op_memcpy(new_items, m_items, hole * m_allocator.size);
		// Copy elements after item.
		if (hole < m_count)
			op_memcpy(new_items + (hole + 1) * m_allocator.size, m_items + hole * m_allocator.size, (m_count - hole) * m_allocator.size);
	}

	OP_DELETEA(m_items);

	m_size = new_size;
	m_items = new_items;

	return OpStatus::OK;
}

OP_STATUS VectorBase::Shrink(UINT32 new_size, UINT32 hole, UINT32 count)
{
	char* new_items;
	
	if (new_size > 0)
		RETURN_OOM_IF_NULL(new_items = OP_NEWA(char, new_size * m_allocator.size));
	else new_items = NULL;

	if (new_size > 0)
	{
		// Copy elements before item.
		op_memcpy(new_items, m_items, hole * m_allocator.size);
		// Copy elements after item.
		if (hole < m_count)
			op_memcpy(new_items + hole * m_allocator.size, m_items + (hole + count) * m_allocator.size, (m_count - hole - count) * m_allocator.size);
	}

	OP_DELETEA(m_items);

	m_size = new_size;
	m_items = new_items;

	return OpStatus::OK;
}

INT32 VectorBase::MergeSort(char* array, char* temp_array, INT32 left, INT32 right)
{
	INT32 left_end;
	INT32 temp_pos;
	INT32 center = (left + right) / 2;
	INT32 last_pos;  // to remove duplicates
	INT32 shift;

	if (center > left)
	{
		if ((shift = MergeSort(temp_array, array, left, center)) <= center)
		{
			op_memmove(temp_array + shift * m_allocator.size, temp_array + (center + 1) * m_allocator.size, (right - center) * m_allocator.size);
			op_memmove(array + shift * m_allocator.size, array + (center + 1) * m_allocator.size, (right - center) * m_allocator.size);
			right -= center + 1 - shift;
			center = shift - 1;
		}
		right = MergeSort(temp_array, array, center + 1, right) - 1;
	}

	left_end = center;
	temp_pos = left;

	++center;

	if (left <= left_end && center <= right)  // first item, cannot check for duplicates
	{
		if (m_allocator.Compare(array + left * m_allocator.size, array + center * m_allocator.size))
			op_memcpy(temp_array + temp_pos++ * m_allocator.size, array + (last_pos = left++) * m_allocator.size, m_allocator.size);
		else
			op_memcpy(temp_array + temp_pos++ * m_allocator.size, array + (last_pos = center++) * m_allocator.size, m_allocator.size);
	}
	else {
		if (left <= left_end)
			op_memcpy(temp_array + temp_pos++ * m_allocator.size, array + left * m_allocator.size, m_allocator.size);
		else
			op_memcpy(temp_array + temp_pos++ * m_allocator.size, array + center * m_allocator.size, m_allocator.size);

		return temp_pos;  // array sizes cannot differ by more than 1
	}

	while (left <= left_end && center <= right)
	{
		if (m_allocator.Compare(array + left * m_allocator.size, array + center * m_allocator.size))
		{
			if (m_allocator.Compare(array + last_pos * m_allocator.size, array + left * m_allocator.size))
				op_memcpy(temp_array + temp_pos++ * m_allocator.size, array + (last_pos = left++) * m_allocator.size, m_allocator.size);
			else
				m_allocator.Destruct(array + left++ * m_allocator.size);
		}
		else {
			if (m_allocator.Compare(array + last_pos * m_allocator.size, array + center * m_allocator.size))
				op_memcpy(temp_array + temp_pos++ * m_allocator.size, array + (last_pos = center++) * m_allocator.size, m_allocator.size);
			else 
				m_allocator.Destruct(array + center++ * m_allocator.size);
		}
	}
	
	while (left <= left_end)
	{
		if (m_allocator.Compare(array + last_pos * m_allocator.size, array + left * m_allocator.size))
			op_memcpy(temp_array + temp_pos++ * m_allocator.size, array + (last_pos = left++) * m_allocator.size, m_allocator.size);
		else
			m_allocator.Destruct(array + left++ * m_allocator.size);
	}

	while (center <= right)
	{
		if (m_allocator.Compare(array + last_pos * m_allocator.size, array + center * m_allocator.size))
			op_memcpy(temp_array + temp_pos++ * m_allocator.size, array + (last_pos = center++) * m_allocator.size, m_allocator.size);
		else
			m_allocator.Destruct(array + center++ * m_allocator.size);
	}

	return temp_pos;
}


OP_STATUS VectorBase::Unite(const VectorBase &vec)
{
	char *dst;
	UINT32 i, j, pos, len;

	if (vec.m_count == 0)
		return OpStatus::OK;
	if (m_count == 0)
		return DuplicateOf(vec);

	pos = 0;
	if (m_size >= m_count + vec.m_count)
	{
		i = 0;
		while (i < vec.m_count)
		{
			pos = Search(vec.Get(i), pos, m_count);

			while (pos < m_count && !m_allocator.Compare(Get(pos), vec.Get(i)) && !m_allocator.Compare(vec.Get(i), Get(pos)))  // remove duplicates
			{
				if (++i >= vec.m_count)
					return OpStatus::OK;

				++pos;
			}

			if (pos >= m_count)
				break;

			if (m_allocator.Compare(Get(pos), vec.Get(i)))
				continue;

			len = 1;
			while (i + len < vec.m_count && m_allocator.Compare(vec.Get(i + len), Get(pos)))
				++len;

			op_memmove(m_items + (pos + len) * m_allocator.size, m_items + pos * m_allocator.size, (m_count - pos) * m_allocator.size);
			for (j = 0; j < len; ++j)
			{
				RETURN_IF_ERROR(m_allocator.Assign(m_items + (pos + j) * m_allocator.size, vec.Get(i + j)));
			}

			m_count += len;
			i += len;
			pos += len;
		}

		if (i < vec.m_count && !m_allocator.Compare(Get(m_count - 1), vec.Get(i)) && !m_allocator.Compare(vec.Get(i), Get(m_count - 1)))
			++i;

		while (i < vec.m_count)
		{
			RETURN_IF_ERROR(m_allocator.Assign(m_items + m_count++ * m_allocator.size, vec.Get(i++)));
		}

		return OpStatus::OK;
	}

	RETURN_OOM_IF_NULL(dst = OP_NEWA(char, (m_count + vec.m_count) * m_allocator.size));

	m_size = m_count + vec.m_count;

	i = 0;
	j = 0;
	pos = 0;

	while (i < m_count && j < vec.m_count)
	{
		if (m_allocator.Compare(Get(i), vec.Get(j)))
		{
			RETURN_IF_ERROR(m_allocator.Assign(dst + pos++ * m_allocator.size, Get(i++)));
		}
		else if (m_allocator.Compare(vec.Get(j), Get(i)))
		{
			RETURN_IF_ERROR(m_allocator.Assign(dst + pos++ * m_allocator.size, vec.Get(j++)));
		}
		else {
			RETURN_IF_ERROR(m_allocator.Assign(dst + pos++ * m_allocator.size, Get(i)));
			++i;
			++j;
		}
	}

	while (i < m_count)
	{
		RETURN_IF_ERROR(m_allocator.Assign(dst + pos++ * m_allocator.size, Get(i++)));
	}

	while (j < vec.m_count)
	{
		RETURN_IF_ERROR(m_allocator.Assign(dst + pos++ * m_allocator.size, vec.Get(j++)));
	}

	if (m_items != NULL)
		OP_DELETEA(m_items);

	m_items = dst;
	m_count = pos;

	return OpStatus::OK;
}

OP_STATUS VectorBase::Unite(VectorBase &result, const VectorBase &vec1, const VectorBase &vec2)
{
	UINT32 i,j;

	if (result.m_count > 0)
		result.Clear();
	RETURN_IF_ERROR(result.Reserve(vec1.m_count + vec2.m_count));

	i = 0;
	j = 0;
	result.m_count = 0;

	while (i < vec1.m_count && j < vec2.m_count)
	{
		if (result.m_allocator.Compare(vec1.Get(i), vec2.Get(j)))
		{
			RETURN_IF_ERROR(result.m_allocator.Assign(result.m_items + (result.m_count++) * result.m_allocator.size, vec1.Get(i++)));
		}
		else if (result.m_allocator.Compare(vec2.Get(j), vec1.Get(i)))
		{
			RETURN_IF_ERROR(result.m_allocator.Assign(result.m_items + (result.m_count++) * result.m_allocator.size, vec2.Get(j++)));
		}
		else {
			RETURN_IF_ERROR(result.m_allocator.Assign(result.m_items + (result.m_count++) * result.m_allocator.size, vec1.Get(i)));
			++i;
			++j;
		}
	}

	while (i < vec1.m_count)
	{
		RETURN_IF_ERROR(result.m_allocator.Assign(result.m_items + (result.m_count++) * result.m_allocator.size, vec1.Get(i++)));
	}

	while (j < vec2.m_count)
	{
		RETURN_IF_ERROR(result.m_allocator.Assign(result.m_items + (result.m_count++) * result.m_allocator.size, vec2.Get(j++)));
	}

	return OpStatus::OK;
}


void VectorBase::Differ(const VectorBase &vec)
{
	UINT32 i, dpos1, dlen;

	if (m_count == 0 || vec.m_count == 0)
		return;

	i = 0;
	dpos1 = Search(vec.Get(i));

	while (dpos1 < m_count)
	{
		if (!m_allocator.Compare(vec.Get(i++), Get(dpos1)))
		{
			dlen = 1;
			while (dpos1 + dlen < m_count && i < vec.m_count && !m_allocator.Compare(Get(dpos1 + dlen), vec.Get(i)) && !m_allocator.Compare(vec.Get(i), Get(dpos1 + dlen)))
			{
				++dlen;
				++i;
			}
			Delete(dpos1, dlen);
		}
		if (i >= vec.m_count)
			return;

		dpos1 = Search(vec.Get(i), dpos1, m_count);
	}

	return;
}

OP_STATUS VectorBase::Differ(VectorBase &result, const VectorBase &vec1, const VectorBase &vec2)
{
	UINT32 i,j;

	if (result.m_count > 0)
		result.Clear();
	RETURN_IF_ERROR(result.Reserve(vec1.m_count));

	i = 0;
	j = 0;
	result.m_count = 0;

	while (i < vec1.m_count && j < vec2.m_count)
	{
		if (result.m_allocator.Compare(vec1.Get(i), vec2.Get(j)))
		{
			RETURN_IF_ERROR(result.m_allocator.Assign(result.m_items + (result.m_count++) * result.m_allocator.size, vec1.Get(i++)));
		}
		else if (result.m_allocator.Compare(vec2.Get(j), vec1.Get(i)))
			++j;
		else {
			++i;
			++j;
		}
	}

	while (i < vec1.m_count)
	{
		RETURN_IF_ERROR(result.m_allocator.Assign(result.m_items + (result.m_count++) * result.m_allocator.size, vec1.Get(i++)));
	}

	return OpStatus::OK;
}


OP_STATUS VectorBase::Intersect(const VectorBase &vec)
{
	UINT32 vmin, i, dpos, dlen;

	if (m_count == 0 || vec.m_count == 0)
	{
		Clear();
		return OpStatus::OK;
	}

	vmin = 0;
	dpos = 0;
	dlen = 0;

	do {
		i = vec.Search(Get(dpos + dlen), vmin, vec.m_count);
		if (i < vec.m_count && !m_allocator.Compare(Get(dpos + dlen), vec.Get(i)))
		{
			vmin = i;

			if (dlen > 0)
				Delete(dpos, dlen);
			else
				++dpos;

			dlen = 0;
		}
		else
			++dlen;
	} while (dpos + dlen < m_count);

	if (dlen > 0)
		Delete(dpos, dlen);

	return OpStatus::OK;
}

OP_STATUS VectorBase::Intersect(VectorBase &result, const VectorBase &vec1, const VectorBase &vec2)
{
	UINT32 i,j;

	if (result.m_count > 0)
		result.Clear();

	if (vec1.m_count == 0 || vec2.m_count == 0)
		return OpStatus::OK;

	RETURN_IF_ERROR(result.Reserve(vec1.m_count));

	i = 0;
	j = 0;

	while (i < vec1.m_count && j < vec2.m_count)
	{
		if (result.m_allocator.Compare(vec1.Get(i), vec2.Get(j)))
			++i;
		else if (result.m_allocator.Compare(vec2.Get(j), vec1.Get(i)))
			++j;
		else {
			RETURN_IF_ERROR(result.m_allocator.Assign(result.m_items + (result.m_count++) * result.m_allocator.size, vec1.Get(i)));
			++i;
			++j;
		}
	}

	return OpStatus::OK;
}

void VectorBase::Filter(BOOL (*matches)(const void* item, void* custom_data), void* custom_data)
{
	if (!matches)
		return;

	/* This algorithm is O(n^2), repeatedly deleting from the middle and shifting down.
	 * It could be optimized, but that does not make sense as long as Intersect is not.
	 * It is probably waaay faster than a single disk access anyway. */
	for (int i = m_count-1; i >= 0; i--)
		if (!matches(Get(i), custom_data))
			Delete(i);
}

#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
size_t VectorBase::EstimateMemoryUsed() const
{
	size_t sum = 0;
	if (m_items)
	{
		unsigned i;
		for (i = 0; i < m_count; ++i)
			sum += m_allocator.EstimateMemoryUsed(m_items + i * m_allocator.size);
		
		sum += 2*sizeof(size_t);
	}
	return sum +
		sizeof(m_items) +
		sizeof(m_allocator) +
		sizeof(m_size) +
		sizeof(m_count);
}
#endif

#endif  // SEARCH_ENGINE

