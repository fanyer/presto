/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef OP_TIGHT_VECTOR_H
#define OP_TIGHT_VECTOR_H

/** @brief A vector suitable for small numbers of items (uses less memory)
  * @param T Type to store in vector, can be a pointer
  * @param StepMultiplier If added items do not fit into existing memory, multiply
  *        the used memory by this number
  *
  * Examples:
  *   OpTightVector<int> vector // Vector of integers
  *   OpTightVector<OpRect> vector // Vector of OpRects
  *   OpTightVector<OpRect*> vector // Vector of pointers to OpRect
  *
  * This is a controlled (in memory terms) vector that will use a minimum of
  * sizeof(T) * N + 8 + sizeof(void*) bytes and a maximum of
  * sizeof(T) * N * StepMultiplier + 8 + sizeof(void*) bytes, where N is the number of items
  * in the vector.
  *
  * At any time, the memory use can be brought back to the minimum by calling Trim().
  */
template<typename T, unsigned StepMultiplier = 2>
class OpTightVector
{
public:
	OpTightVector() : m_array(0), m_count(0), m_size(0) {}
	~OpTightVector() { op_free(m_array); }

	/** Remove all items */
	inline void Clear();

	/** Access an item in this vector
	  * @param pos position in vector, behavior undefined if pos >= GetCount()
	  * @note This can be used to get the underlying C-style array
	  * @example
	  *   OpTightVector<int> ints;
	  *   ints.Add(1);
	  *   int firstint = ints[0]; // firstint == 1
	  *   int* intarray = &ints[0]; // intarray[0] == 1
	  */
	const T& operator[](UINT32 pos) const { return m_array[pos]; }
	T& operator[](UINT32 pos) { return m_array[pos]; }

	/** Insert an item into the vector at a specified position
	  * @param pos Where to insert item
	  * @param item Item to insert
	  */
	inline OP_STATUS Insert(UINT32 pos, const T& item);

	/** Add an item to the end of the vector
	  * @param item Item to add
	  */
	OP_STATUS Add(const T& item) { return Insert(m_count, item); }

	/** Remove items from the vector
	  * @param pos Which item to remove
	  * @param count Number of items to remove starting at @a pos
	  */
	inline void Remove(UINT32 pos, UINT32 count = 1);

	/** Move items in the vector in range [from .. from+count) to [to .. to+count)
	  * @param from Move items starting at this position
	  * @param to Move items to this position
	  * @param count Number of items to move starting at @a from
	  * @note No items are lost in this operation. When moving to a lower
	  * position, existing items will move to a higher position and vice versa.
	  * @note The from and to ranges may overlap.
	  */
	OP_STATUS Move(UINT32 from, UINT32 to, UINT32 count = 1);

	/** Find an item in the vector using operator==()
	  * @param item the item to look for
	  * @return index of @a item in the vector, or @c -1 if it is not found
	  */
	INT32 Find(const T& item) const;

	/** @return Number of items in the vector. */
	UINT32 GetCount() const { return m_count; }

	/** Minimalize the memory usage to the number of items in the vector */
	OP_STATUS Trim() { return Resize(m_count); }

	/** Resize the vector to a new number of items
	  * @param new_size New number of items to resize to
	  * @note after this action, all items and only those items with
	  * index in [0,new_size) can be accessed
	  */
	inline OP_STATUS Resize(UINT32 new_size);

	/** Prepare vector for new items, if number is known in advance this can speed up adding many items
	  * @param growsize How many new items to prepare for (in addition to GetCount())
	  * @note this allocates space but doesn't change the actual number of items in the vector
	  */
	OP_STATUS Reserve(UINT32 growsize) { return m_count + growsize > m_size ? ReserveInternal(m_count + growsize) : OpStatus::OK; }

protected:
	inline OP_STATUS ReserveInternal(UINT32 new_size);

	OP_STATUS PrepareForNewItem() { return m_count == m_size ? ReserveInternal(MAX(m_count + 1, m_count * StepMultiplier)) : OpStatus::OK; }

	T* m_array;
	UINT32 m_count;
	UINT32 m_size;

private:
	OpTightVector(const OpTightVector&);
	OpTightVector& operator=(const OpTightVector&);
};

/** @brief Like OpTightVector, but owns pointers given to it
  * Will deallocate pointers when removed from vector (including on destruction)
  */
template<typename T, unsigned StepMultiplier = 2>
class OpAutoTightVector : public OpTightVector<T, StepMultiplier>
{
};

template<typename T, unsigned StepMultiplier>
class OpAutoTightVector<T*, StepMultiplier> : public OpTightVector<T*, StepMultiplier>
{
public:
	typedef OpTightVector<T*, StepMultiplier> Base;

	~OpAutoTightVector() { Deallocate(0, Base::m_count); }

	void Clear() { Deallocate(0, Base::m_count); Base::Clear(); }

	void Remove(UINT32 pos, UINT32 count = 1) { Deallocate(pos, count); Base::Remove(pos, count); }

	T* Release(UINT32 pos) { T* item = Base::m_array[pos]; Base::Remove(pos); return item; }

private:
	void Deallocate(UINT32 pos, UINT32 count)
	{
		for (UINT32 i = pos; i < pos + count; i++)
			OP_DELETE(Base::m_array[i]);
	}
};


// Implementations
template<typename T, unsigned StepMultiplier>
void OpTightVector<T, StepMultiplier>::Clear()
{
	for (UINT32 i = 0; i < m_count; i++)
		m_array[i].~T(); // remember to call the destructor
	op_free(m_array);
	m_array = 0;
	m_size = 0;
	m_count = 0;
}

template<typename T, unsigned StepMultiplier>
OP_STATUS OpTightVector<T, StepMultiplier>::Insert(UINT32 pos, const T& item)
{
	RETURN_IF_ERROR(PrepareForNewItem());
	if (pos < m_count)
		op_memmove(m_array + pos + 1, m_array + pos, (m_count - pos) * sizeof(T));
	// Call the constructor via placement new (doesn't allocate new memory)
	new (&m_array[pos]) T(item);
	m_count++;
	return OpStatus::OK;
}

template <typename T, unsigned StepMultiplier>
void OpTightVector<T, StepMultiplier>::Remove(UINT32 pos, UINT32 count)
{
	for (UINT32 i = pos; i < pos + count; i++)
		m_array[i].~T();
	if (pos + count < m_count)
		op_memmove(m_array + pos, m_array + pos + count, (m_count - pos - count) * sizeof(T));
	m_count -= count;
}

template <typename T, unsigned StepMultiplier>
OP_STATUS OpTightVector<T, StepMultiplier>::Move(UINT32 from, UINT32 to, UINT32 count)
{
	if (from == to)
		return OpStatus::OK;

	OP_ASSERT(from + count <= m_count && to + count <= m_count);
	T* tmp = static_cast<T*>(op_malloc(count * sizeof(T)));
	RETURN_OOM_IF_NULL(tmp);
	op_memcpy(tmp, m_array + from, count * sizeof(T));

	if (from < to)
		op_memmove(m_array + from, m_array + from + count, (to - from) * sizeof(T));
	else
		op_memmove(m_array + to + count, m_array + to, (from - to) * sizeof(T));

	op_memcpy(m_array + to, tmp, count * sizeof(T));
	op_free(tmp);
	return OpStatus::OK;
}

template <typename T, unsigned StepMultiplier>
INT32 OpTightVector<T, StepMultiplier>::Find(const T& item) const
{
	for (UINT32 i = 0; i < m_count; ++i)
		if (m_array[i] == item)
			return i;
	return -1;
}

template <typename T, unsigned StepMultiplier>
OP_STATUS OpTightVector<T, StepMultiplier>::Resize(UINT32 new_size)
{
	for (UINT32 pos = new_size; pos < m_count; pos++)
		m_array[pos].~T();
	RETURN_IF_ERROR(ReserveInternal(new_size));
	for (UINT32 pos = m_count; pos < new_size; pos++)
		new (&m_array[pos]) T;
	m_count = new_size;
	return OpStatus::OK;
}

template <typename T, unsigned StepMultiplier>
OP_STATUS OpTightVector<T, StepMultiplier>::ReserveInternal(UINT32 new_size)
{
	T* new_array = static_cast<T*>(op_realloc(m_array, new_size * sizeof(T)));
	if (!new_array)
		return OpStatus::ERR_NO_MEMORY;
	m_array = new_array;
	m_size = new_size;
	return OpStatus::OK;
}

#endif
