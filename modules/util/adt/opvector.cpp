/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

// ---------------------------------------------------------------------------------

#include "core/pch.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/opswap.h"

// ---------------------------------------------------------------------------------

OpGenericVector::OpGenericVector(UINT32 stepsize)
		: m_size(0),
		  m_items(NULL),
		  m_count(0),
		  m_step(stepsize),
		  m_min_step(stepsize)
{
}

OpGenericVector::~OpGenericVector()
{
	OP_DELETEA(m_items);
}

void OpGenericVector::Clear()
{
	OP_DELETEA(m_items);
	m_items = NULL;
	m_size = 0;
	m_count = 0;
}

void OpGenericVector::Empty()
{
	m_count = 0;
}

OP_STATUS
OpGenericVector::DuplicateOf(const OpGenericVector& vec)
{
	void** new_items = NULL;
	if (vec.m_size > 0)
	{
		new_items = OP_NEWA(void*, vec.m_size);

		if (new_items == NULL)
			return OpStatus::ERR_NO_MEMORY;

		op_memcpy(new_items, vec.m_items, vec.m_size * sizeof(void*));
	}

	OP_DELETEA(m_items);

	m_items = new_items;
	m_size = vec.m_size;
	m_count = vec.m_count;
	m_step = vec.m_step;

	return OpStatus::OK;
}

OP_STATUS
OpGenericVector::Replace(UINT32 idx, void* item)
{
	OP_ASSERT(idx < m_count);

	if (idx >= m_count)
	{
		return OpStatus::ERR;
	}

	m_items[idx] = item;

	return OpStatus::OK;
}

OP_STATUS
OpGenericVector::Insert(UINT32 idx, void* item)
{
	if (idx > m_count)
	{
		idx = m_count;
	}

	if (m_items == NULL)
	{
		RETURN_IF_ERROR(Init());
	}

	if (m_count >= m_size)
	{
		return GrowInsert(idx, item);
	}

	NormalInsert(idx, item);

	return OpStatus::OK;
}

OP_STATUS
OpGenericVector::Add(void* item)
{
	return Insert(m_count, item);
}

OP_STATUS
OpGenericVector::RemoveByItem(void* item)
{
	INT32 idx = Find(item);

	if (idx == -1)
	{
		return OpStatus::ERR;
	}

	Remove(idx);

	return OpStatus::OK;
}

void*
OpGenericVector::Remove(UINT32 idx, UINT32 count)
{
	OP_ASSERT(idx + count <= m_count);

	if (idx >= m_count)
	{
		return NULL;
	}

	if (m_size > m_step && m_count + m_step - count < m_size)
	{
		return ShrinkRemove(idx, count);
	}
	else
	{
		return NormalRemove(idx, count);
	}
}

INT32
OpGenericVector::Find(void* item) const
{
	if (m_items)
	{
		for (UINT32 idx = 0; idx < m_count; idx++)
		{
			if (m_items[idx] == item)
			{
				return idx;
			}
		}
	}

	return -1;
}

void*
OpGenericVector::Get(UINT32 idx) const
{
	if (idx >= m_count)
	{
		return NULL;
	}

	return m_items[idx];
}

OP_STATUS
OpGenericVector::Init()
{
	m_items = OP_NEWA(void*, m_step);
	if (m_items == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	m_size = m_step;
	return OpStatus::OK;
}

OP_STATUS
OpGenericVector::GrowInsert(UINT32 insertidx, void* item)
{
	OP_ASSERT(insertidx <= m_count);

	// auto-increase m_step
	UINT32 new_step = m_step * 2;

	size_t new_size = m_size + new_step;
	void** new_items = OP_NEWA(void*, new_size);
	if (new_items == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	m_step = new_step;
	m_size = new_size;

	// Copy elements before item.
	op_memcpy(&new_items[0], &m_items[0], insertidx * sizeof(void*));
	// Insert item.
	new_items[insertidx] = item;
	// Copy elements after item.
	op_memcpy(&new_items[insertidx + 1], &m_items[insertidx], (m_count - insertidx) * sizeof(void*));

	OP_DELETEA(m_items);
	m_count++;

	m_items = new_items;

	return OpStatus::OK;
}

void
OpGenericVector::NormalInsert(UINT32 insertidx, void* item)
{
	OP_ASSERT(insertidx <= m_count);

	if (insertidx < m_count)
	{
		// Make room for insert.
		op_memmove(&m_items[insertidx + 1], &m_items[insertidx], (m_count - insertidx) * sizeof(void*));
	}

	m_items[insertidx] = item;
	m_count++;
}

void*
OpGenericVector::ShrinkRemove(UINT32 removeidx, UINT32 count)
{
	OP_ASSERT(removeidx + count <= m_count);

	void* remove_item = m_items[removeidx];

	size_t new_size = ((m_count + m_step - count) / m_step) * m_step;

	OP_ASSERT(new_size > 0);

	void** new_items = OP_NEWA(void*, new_size);

	if (new_items == NULL)
	{
		// We do not need to shrink things, if we fail, just use the old vector.
		return NormalRemove(removeidx, count);
	}

	m_count -= count;

	// Copy all items before removeidx.

	op_memcpy(&new_items[0], &m_items[0], removeidx * sizeof(void*));

	// Copy all items after removeidx.
	op_memcpy(&new_items[removeidx], &m_items[removeidx + count], (m_count - removeidx) * sizeof(void*));

	OP_DELETEA(m_items);

	m_size = new_size;
	m_items = new_items;

	// auto-decrease m_step
	if (m_step > m_min_step)
	{
		m_step /= 2;
	}

	return remove_item;
}

void*
OpGenericVector::NormalRemove(UINT32 removeidx, UINT32 count)
{
	OP_ASSERT(removeidx + count <= m_count);

	void* remove_item = m_items[removeidx];

	m_count -= count;

	if (removeidx < m_count)
	{
		op_memmove(&m_items[removeidx], &m_items[removeidx + count], (m_count - removeidx) * sizeof(void*));
	}

	return remove_item;
}

void
OpGenericVector::QSort(int (*cmp)(const void **, const void **))
{
	op_qsort(m_items, GetCount(), sizeof(void **),
		reinterpret_cast<int (*)(const void *, const void *)>(cmp));
}

void
OpGenericVector::Swap(OpGenericVector* v)
{
	op_swap(this->m_size, v->m_size);
	op_swap(this->m_items, v->m_items);
	op_swap(this->m_count, v->m_count);
}

#if defined ADVANCED_OPINT32VECTOR || defined ADVANCED_OPVECTOR

static INT32 MergeSort(void** array, void** temp_array, INT32 left, INT32 right)
{
	INT32 left_end;
	INT32 temp_pos;
	INT32 center = (left + right) / 2;
	INTPTR last;
	INT32 shift;

	if (center > left)
	{
		if ((shift = MergeSort(temp_array, array, left, center)) <= center)
		{
			op_memmove(temp_array + shift, temp_array + center + 1, (right - center) * sizeof(void *));
			op_memmove(array + shift, array + center + 1, (right - center) * sizeof(void *));
			right -= center + 1 - shift;
			center = shift - 1;
		}
		right = MergeSort(temp_array, array, center + 1, right) - 1;
	}

	left_end = center;
	temp_pos = left;

	++center;

	if (left > 0)
		last = (INTPTR)(array[left - 1]);
	else {
		if ((INTPTR)(array[left]) == (INTPTR)(array[center]))  // just init to some value
			last = (INTPTR)(array[left]) - 1;
		else last = (INTPTR)(array[left]) < (INTPTR)(array[center]) ? (INTPTR)(array[center]) : (INTPTR)(array[left]);
	}

	while (left <= left_end && center <= right)
	{
		if ((INTPTR)(array[left]) < (INTPTR)(array[center]))
		{
			if ((INTPTR)(array[left]) != last)
				last = INTPTR(temp_array[temp_pos++] = array[left++]);
			else ++left;
		}
		else {
			if ((INTPTR)(array[center]) != last)
				last = INTPTR(temp_array[temp_pos++] = array[center++]);
			else ++center;
		}
	}

	while (left <= left_end)
	{
		if ((INTPTR)(array[left]) != last)
			last = INTPTR(temp_array[temp_pos++] = array[left++]);
		else ++left;
	}

	while (center <= right)
	{
		if ((INTPTR)(array[center]) != last)
			last = INTPTR(temp_array[temp_pos++] = array[center++]);
		else ++center;
	}

	return temp_pos;
}

// based on johan's OpGenericBinaryTree with some optimizations
OP_STATUS OpINT32Vector::Sort()
{
	int count;

	if (m_count <= 1)
		return OpStatus::OK;

	void** temp_array = OP_NEWA(void*, m_count);
	if (!temp_array)
       return OpStatus::ERR_NO_MEMORY;

	op_memcpy(temp_array, m_items, m_count * sizeof(void *));

	count = MergeSort(temp_array, m_items, 0, m_count - 1);

	Remove(count, m_count - count);

	OP_DELETEA(temp_array);
	return OpStatus::OK;
}

// returns either a position of item, if found, or position where to insert
UINT32 OpINT32Vector::Search(INT32 item, UINT32 start, UINT32 end) const
{
	int n2;

	while (end > start)
	{
		n2 = (end - start) / 2;
		if (Get(start + n2) < item)
			start = start + n2 + 1;
		else
			end = start + n2;
	}

	return start;
}

OP_STATUS OpINT32Vector::Add(const OpINT32Vector &vec)
{
	void **dst;
	UINT32 i, j, pos, len;

	pos = 0;
	if (m_size > m_count + vec.m_count)
	{
		i = 0;
		while (i < vec.m_count)
		{
			pos = Search((INTPTR)(vec.m_items[i]), pos, m_count);

			while (pos < m_count && (INTPTR)(m_items[pos]) == (INTPTR)(vec.m_items[i]))  // remove duplicities
			{
				if (++i >= vec.m_count)
					return OpStatus::OK;

				++pos;
			}

			if (pos >= m_count)
				break;

			if ((INTPTR)(m_items[pos]) < (INTPTR)(vec.m_items[i]))
				continue;

			len = 1;
			while (i + len < vec.m_count && (INTPTR)(m_items[pos]) > (INTPTR)(vec.m_items[i + len]))
				++len;

			op_memmove(m_items + pos + len, m_items + pos, (m_count - pos) * sizeof(void *));
			op_memcpy(m_items + pos, vec.m_items + i, len * sizeof(void *));

			m_count += len;
			i += len;
			pos += len;
		}

		if (i < vec.m_count && (INTPTR)(vec.m_items[i]) == (INTPTR)(m_items[m_count - 1]))
			++i;

		if (i < vec.m_count)
		{
			op_memcpy(m_items + m_count, vec.m_items + i, (vec.m_count - i) * sizeof(void *));
			m_count += (vec.m_count - i);
		}

		return OpStatus::OK;
	}

	if ((dst = OP_NEWA(void *, m_count + vec.m_count)) == NULL)
		return OpStatus::ERR_NO_MEMORY;

	m_size = m_count + vec.m_count;

	i = 0;
	j = 0;
	pos = 0;

	while (i < m_count && j < vec.m_count)
	{
		if ((INTPTR)(m_items[i]) == (INTPTR)(vec.m_items[j]))
		{
			dst[pos++] = m_items[i++];
			++j;
		}
		else if ((INTPTR)(m_items[i]) < (INTPTR)(vec.m_items[j]))
			dst[pos++] = m_items[i++];
		else dst[pos++] = vec.m_items[j++];
	}

	while (i < m_count)
		dst[pos++] = m_items[i++];

	while (j < vec.m_count)
		dst[pos++] = vec.m_items[j++];

	if (m_items != NULL)
		OP_DELETEA(m_items);

	m_items = dst;
	m_count = pos;

	return OpStatus::OK;
}

OP_STATUS OpINT32Vector::Add(OpINT32Vector &result, const OpINT32Vector &vec1, const OpINT32Vector &vec2)
{
	UINT32 i,j;

	if (result.m_size < vec1.m_size + vec2.m_size)
	{
		void **dst;

		if ((dst = OP_NEWA(void *, vec1.m_count + vec2.m_count)) == NULL)
			return OpStatus::ERR_NO_MEMORY;

		result.m_size = vec1.m_count + vec2.m_count;

		if (result.m_items != NULL)
			OP_DELETEA(result.m_items);

		result.m_items = dst;
	}

	i = 0;
	j = 0;
	result.m_count = 0;

	while (i < vec1.m_count && j < vec2.m_count)
	{
		if ((INTPTR)(vec1.m_items[i]) == (INTPTR)(vec2.m_items[j]))
		{
			result.m_items[result.m_count++] = vec1.m_items[i++];
			++j;
		}
		else if ((INTPTR)(vec1.m_items[i]) < (INTPTR)(vec2.m_items[j]))
			result.m_items[result.m_count++] = vec1.m_items[i++];
		else result.m_items[result.m_count++] = vec2.m_items[j++];
	}

	while (i < vec1.m_count)
		result.m_items[result.m_count++] = vec1.m_items[i++];

	while (j < vec2.m_count)
		result.m_items[result.m_count++] = vec2.m_items[j++];

	return OpStatus::OK;
}

#ifdef ADVANCED_OPINT32VECTOR
OP_STATUS OpINT32Vector::Subtract(const OpINT32Vector &vec)
#else
OP_STATUS OpINT32Vector::Substract(const OpINT32Vector &vec)
#endif
{
	UINT32 i, dpos1, dpos2, pos, orig_count;

	if (m_count == 0 || vec.m_count == 0)
		return OpStatus::OK;

	dpos1 = 0;
	i = (UINT32)-1;
	do {
		if (++i >= vec.m_count)
			return OpStatus::OK;  // nothing to delete

		dpos1 = Search((INTPTR)(vec.m_items[i]), dpos1, m_count);
	} while (dpos1 >= m_count || (INTPTR)(m_items[dpos1]) != (INTPTR)(vec.m_items[i]));

	pos = dpos1;
	orig_count = m_count;

	UINT32 dlen;
	for (;;)
	{
		dlen = 1;
		while (dpos1 + dlen < orig_count && i + 1 < vec.m_count &&
			(INTPTR)(m_items[dpos1 + dlen]) == (INTPTR)(vec.m_items[i + 1]))
		{
			++i;
			++dlen;
		}

		if (dpos1 + dlen >= orig_count)
		{
			m_count = pos;
			Refit();
			return OpStatus::OK;
		}

		if (i >= vec.m_count)
			break;

		dpos2 = dpos1 + dlen;
		do {
			if (++i >= vec.m_count)
				break;

			dpos2 = Search((INTPTR)(vec.m_items[i]), dpos2, orig_count);
		} while (dpos2 >= m_count || (INTPTR)(m_items[dpos2]) != (INTPTR)(vec.m_items[i]));

		if (i >= vec.m_count)
			break;

		if ((dpos2 - dpos1 - dlen) > 0)
		{
			op_memmove(m_items + pos, m_items + dpos1 + dlen, (dpos2 - dpos1 - dlen) * sizeof(void *));
			pos += (dpos2 - dpos1 - dlen);
		}
		dpos1 = dpos2;

		m_count -= dlen;
	}

	op_memmove(m_items + pos, m_items + dpos1 + dlen, (orig_count - dpos1 - dlen) * sizeof(void *));
	m_count -= dlen;

	Refit();

	return OpStatus::OK;
}

#ifdef ADVANCED_OPINT32VECTOR
OP_STATUS OpINT32Vector::Subtract(OpINT32Vector &result, const OpINT32Vector &vec1, const OpINT32Vector &vec2)
#else
OP_STATUS OpINT32Vector::Substract(OpINT32Vector &result, const OpINT32Vector &vec1, const OpINT32Vector &vec2)
#endif
{
	UINT32 i,j;

	if (result.m_size < vec1.m_size)
	{
		void **dst;

		if ((dst = OP_NEWA(void *, vec1.m_count)) == NULL)
			return OpStatus::ERR_NO_MEMORY;

		result.m_size = vec1.m_count;

		if (result.m_items != NULL)
			OP_DELETEA(result.m_items);

		result.m_items = dst;
	}

	i = 0;
	j = 0;
	result.m_count = 0;

	while (i < vec1.m_count && j < vec2.m_count)
	{
		if ((INTPTR)(vec1.m_items[i]) == (INTPTR)(vec2.m_items[j]))
		{
			++i;
			++j;
		}
		else if ((INTPTR)(vec1.m_items[i]) < (INTPTR)(vec2.m_items[j]))
			result.m_items[result.m_count++] = vec1.m_items[i++];
		else ++j;
	}

	while (i < vec1.m_count)
		result.m_items[result.m_count++] = vec1.m_items[i++];

	result.Refit();

	return OpStatus::OK;
}

OP_STATUS OpINT32Vector::Intersect(const OpINT32Vector &vec)
{
	UINT32 i, dpos1, dpos2, dlen, pos, orig_count;

	if (m_count == 0 || vec.m_count == 0)
	{
		Clear();
		return OpStatus::OK;
	}

	dpos1 = (UINT32)-1;
	i = 0;
	do {
		if (++dpos1 >= m_count)
			return OpStatus::OK;
		i = vec.Search((INTPTR)(m_items[dpos1]), i, vec.m_count);
	} while (i < vec.m_count && (INTPTR)(m_items[dpos1]) == (INTPTR)(vec.m_items[i]));

	pos = dpos1;
	orig_count = m_count;

	for (;;)
	{
		dlen = 0;
		do {
			if (dpos1 + ++dlen >= orig_count)
				break;
			i = vec.Search((INTPTR)(m_items[dpos1 + dlen]), i, vec.m_count);
		} while (i < vec.m_count && (INTPTR)(m_items[dpos1 + dlen]) != (INTPTR)(vec.m_items[i]));

		if (dpos1 + dlen >= orig_count || i >= vec.m_count)
		{
			m_count = pos;
			Refit();
			return OpStatus::OK;
		}

		dpos2 = dpos1 + dlen;
		do {
			if (++dpos2 >= orig_count)
				break;
			i = vec.Search((INTPTR)(m_items[dpos2]), i, vec.m_count);
		} while (i < vec.m_count && (INTPTR)(m_items[dpos2]) == (INTPTR)(vec.m_items[i]));

		if (dpos2 >= orig_count)
			break;

		op_memmove(m_items + pos, m_items + dpos1 + dlen, (dpos2 - dpos1 - dlen) * sizeof(void *));
		pos += (dpos2 - dpos1 - dlen);
		dpos1 = dpos2;

		m_count -= dlen;
	}

	op_memmove(m_items + pos, m_items + dpos1 + dlen, (orig_count - dpos1 - dlen) * sizeof(void *));
	m_count -= dlen;

	Refit();

	return OpStatus::OK;
}

OP_STATUS OpINT32Vector::Intersect(OpINT32Vector &result, const OpINT32Vector &vec1, const OpINT32Vector &vec2)
{
	UINT32 i,j;

	if (result.m_size < vec1.m_size)
	{
		void **dst;

		if ((dst = OP_NEWA(void *, vec1.m_count)) == NULL)
			return OpStatus::ERR_NO_MEMORY;

		result.m_size = vec1.m_count;

		if (result.m_items != NULL)
			OP_DELETEA(result.m_items);

		result.m_items = dst;
	}

	i = 0;
	j = 0;
	result.m_count = 0;

	while (i < vec1.m_count && j < vec2.m_count)
	{
		if ((INTPTR)(vec1.m_items[i]) == (INTPTR)(vec2.m_items[j]))
		{
			result.m_items[result.m_count++] = vec1.m_items[i++];
			++j;
		}
		else if ((INTPTR)(vec1.m_items[i]) < (INTPTR)(vec2.m_items[j]))
			++i;
		else ++j;
	}

	result.Refit();

	return OpStatus::OK;
}

// algorithm taken from ShrinkRemove;
// doesn't seem to work precisely even there if you removed a significant number of elements,
// but it doesn't matter
void OpINT32Vector::Refit(void)
{
	size_t new_size;
	void** new_items;

	if (m_size <= m_step || m_count + m_step >= m_size)
		return;

	if (m_count == 0)
	{
		Clear();
		return;
	}

	new_size = ((m_count + m_step) / m_step) * m_step;
	if ((new_items = OP_NEWA(void*, new_size)) == NULL)
		return;

	op_memcpy(new_items, m_items, m_count * sizeof(void *));

	OP_DELETEA(m_items);

	m_size = new_size;
	m_items = new_items;

	// auto-decrease m_step
	if (m_step > m_min_step)
	{
		m_step /= 2;
	}
}

#endif  // ADVANCED_OPVECTOR

// The following is needed because .ot files apparently cannot have static functions
#ifdef SELFTEST
extern "C" {
	int util_test_OpVector_QSort_intcmp(const int **a, const int **b)
	{
		return **a - **b;
	}
} // extern "C"
#endif // SELFTEST
