/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef OP_VALUEVECTOR_H
#define OP_VALUEVECTOR_H

#ifdef PROTOBUF_SUPPORT

template<typename T>
class OpValueVector
{
public:
	OpValueVector(UINT32 stepsize = 10);
	virtual ~OpValueVector();

	void Clear();

	void SetAllocationStepSize(UINT32 step) { m_step = step; m_min_step = step; }

	OP_STATUS DuplicateOf(const OpValueVector<T>& vec);

	OP_STATUS Add(T item);

	OP_STATUS Replace(UINT32 idx, T item);

	OP_STATUS Insert(UINT32 idx, T item);

	OP_STATUS RemoveByItem(T item);

	OP_STATUS Remove(UINT32 idx, UINT32 count = 1);

	INT32 Find(T item) const;

	T Get(UINT32 idx) const;

	UINT32 GetCount() const { return m_count; }

private:
	OP_STATUS GrowInsert(UINT32 insertidx, T item);
	void NormalInsert(UINT32 insertidx, T item);
	OP_STATUS ShrinkRemove(UINT32 removeidx, UINT32 count);
	OP_STATUS NormalRemove(UINT32 removeidx, UINT32 count);

	UINT32 m_size;
	T*     m_items;
	UINT32 m_count;
	UINT32 m_step;
	UINT32 m_min_step;

	OP_STATUS Init();
	OpValueVector(const OpValueVector&);
	OpValueVector& operator=(const OpValueVector&);
};

template<typename T>
OpValueVector<T>::OpValueVector(UINT32 stepsize)
		: m_size(0),
		  m_items(NULL),
		  m_count(0),
		  m_step(stepsize),
		  m_min_step(stepsize)
{
	// The step size must be larger than 0, if not all actions that grow the internal array will fail
	OP_ASSERT(m_step > 0);
	if (m_step == 0)
		m_step = 1;
}

template<typename T>
OpValueVector<T>::~OpValueVector()
{
	OP_DELETEA(m_items);
}

template<typename T>
void OpValueVector<T>::Clear()
{
	OP_DELETEA(m_items);
	m_items = NULL;
	m_size = 0;
	m_count = 0;
}

template<typename T>
OP_STATUS
OpValueVector<T>::DuplicateOf(const OpValueVector<T>& vec)
{
	T* new_items = NULL;
	if (vec.m_size > 0)
	{
		new_items = OP_NEWA(T, vec.m_size);

		if (new_items == NULL)
			return OpStatus::ERR_NO_MEMORY;

		op_memcpy(new_items, vec.m_items, vec.m_size * sizeof(T));
	}

	OP_DELETEA(m_items);

	m_items = new_items;
	m_size = vec.m_size;
	m_count = vec.m_count;
	m_step = vec.m_step;

	return OpStatus::OK;
}

template<typename T>
OP_STATUS
OpValueVector<T>::Replace(UINT32 idx, T item)
{
	OP_ASSERT(idx < m_count);

	if (idx >= m_count)
	{
		return OpStatus::ERR;
	}

	m_items[idx] = item;

	return OpStatus::OK;
}

template<typename T>
OP_STATUS
OpValueVector<T>::Insert(UINT32 idx, T item)
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

template<typename T>
OP_STATUS
OpValueVector<T>::Add(T item)
{
	return Insert(m_count, item);
}

template<typename T>
OP_STATUS
OpValueVector<T>::RemoveByItem(T item)
{
	INT32 idx = Find(item);

	if (idx == -1)
	{
		return OpStatus::ERR;
	}

	Remove(idx);

	return OpStatus::OK;
}

template<typename T>
OP_STATUS
OpValueVector<T>::Remove(UINT32 idx, UINT32 count)
{
	OP_ASSERT(idx + count <= m_count);

	if (idx >= m_count)
	{
		return OpStatus::ERR;
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

template<typename T>
INT32
OpValueVector<T>::Find(T item) const
{
	if (item && m_items)
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

template<typename T>
T
OpValueVector<T>::Get(UINT32 idx) const
{
	if (idx >= m_count)
	{
		return T();
	}

	return m_items[idx];
}

template<typename T>
OP_STATUS
OpValueVector<T>::Init()
{
	m_items = OP_NEWA(T, m_step);
	if (m_items == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	m_size = m_step;
	return OpStatus::OK;
}

template<typename T>
OP_STATUS
OpValueVector<T>::GrowInsert(UINT32 insertidx, T item)
{
	OP_ASSERT(insertidx <= m_count);

	// auto-increase m_step
	UINT32 new_step = m_step * 2;

	size_t new_size = m_size + new_step;
	T* new_items = OP_NEWA(T, new_size);
	if (new_items == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	m_step = new_step;
	m_size = new_size;

	// Copy elements before item.
	op_memcpy(&new_items[0], &m_items[0], insertidx * sizeof(T));
	// Insert item.
	new_items[insertidx] = item;
	// Copy elements after item.
	op_memcpy(&new_items[insertidx + 1], &m_items[insertidx], (m_count - insertidx) * sizeof(T));

	OP_DELETEA(m_items);
	m_count++;

	m_items = new_items;

	return OpStatus::OK;
}

template<typename T>
void
OpValueVector<T>::NormalInsert(UINT32 insertidx, T item)
{
	OP_ASSERT(insertidx <= m_count);

	if (insertidx < m_count)
	{
		// Make room for insert.
		op_memmove(&m_items[insertidx + 1], &m_items[insertidx], (m_count - insertidx) * sizeof(T));
	}

	m_items[insertidx] = item;
	m_count++;
}

template<typename T>
OP_STATUS
OpValueVector<T>::ShrinkRemove(UINT32 removeidx, UINT32 count)
{
	OP_ASSERT(removeidx + count <= m_count);

	size_t new_size = ((m_count + m_step - count) / m_step) * m_step;

	OP_ASSERT(new_size > 0);

	T* new_items = OP_NEWA(T, new_size);

	if (new_items == NULL)
	{
		// We do not need to shrink things, if we fail, just use the old vector.
		return NormalRemove(removeidx, count);
	}

	m_count -= count;

	// Copy all items before removeidx.

	op_memcpy(&new_items[0], &m_items[0], removeidx * sizeof(T));

	// Copy all items after removeidx.
	op_memcpy(&new_items[removeidx], &m_items[removeidx + count], (m_count - removeidx) * sizeof(T));

	OP_DELETEA(m_items);

	m_size = new_size;
	m_items = new_items;

	// auto-decrease m_step
	if (m_step > m_min_step)
	{
		m_step /= 2;
	}

	return OpStatus::OK;
}

template<typename T>
OP_STATUS
OpValueVector<T>::NormalRemove(UINT32 removeidx, UINT32 count)
{
	OP_ASSERT(removeidx + count <= m_count);

	m_count -= count;

	if (removeidx < m_count)
	{
		op_memmove(&m_items[removeidx], &m_items[removeidx + count], (m_count - removeidx) * sizeof(T));
	}

	return OpStatus::OK;
}

#endif // PROTOBUF_SUPPORT

#endif // OP_VALUEVECTOR_H
