/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_OTL_DEQUE_H
#define MODULES_OTL_DEQUE_H

/**
 * Template for double-ended queues.
 *
 * A deque is a kind of sequence container similar to OtlVector, but with fast
 * insertion and removal at both ends, not only at the back.
 *
 * This implementation of a double-ended queue, like OtlVector, uses a single
 * array of elements for storing the elements. However, @e unlike OtlVector,
 * the elements are not guaranteed to be contiguous. The memory reserved
 * by the deque is used to store elements in a cirular fashion; when one
 * end is full, and we attempt to push a new element to that same end,
 * we wrap around to the opposite end of the array, and store the element
 * there instead (assuming that end isn't full too).
 *
 * Like OtlVector, objects added to the deque are @e copied. Please make sure
 * that the type you want to store is copy-constructible. See OtlVector for
 * more information.
 *
 * Items in the deque are accessed using the array operator, with indices
 * from '0' (front of the queue) to 'GetSize() - 1' (back of the queue).
 *
 * Basic usage:
 *
 * @code
 *  OtlDeque<int> deque;
 *  RETURN_IF_ERROR(deque.PushBack(37));  // Add an item at the end.
 *  RETURN_IF_ERROR(deque.PushFront(42));  // Add an item in front.
 *  RETURN_IF_ERROR(deque.PushFront(13));  // Add another item in front.
 *  deque.Erase(0); // Erase item at index '0' (erase front).
 *  int answer = deque[0]; // Accessing item at index '0' (42).
 * @endcode
 *
 * Looping through items:
 *
 * @code
 *  for (size_t i = 0; i < deque.GetSize(); ++i)
 *    deque[i].DoSomething();
 * @endcode
 *
 * @see OtlVector
 */
template<typename T>
class OtlDeque
{
public:
	/**
	 * Construct a new deque.
	 *
	 * You may choose the allocation growth factor during construction of the
	 * deque. The growth factor controls how much larger a chunk of memory
	 * the deque allocates every time it runs out of space.
	 *
	 * Do not hold references or pointers to individual elements in the deque;
	 * they will be invalidated whenever the deque needs to grow.
	 *
	 * No allocation takes place until something is added to the deque using
	 * PushBack(), PushFront() or Insert(), regardless of growth factor.
	 * However, if you know the number of elements beforehand, you can
	 * Reserve() the amount you need and avoid excessive reallocations.
	 *
	 * @see Reserve
	 * @see SetGrowthFactor
	 *
	 * @param growth How much to increase the total capacity of the deque when
	 *               the deque needs to grow. The default value of 1.5f means
	 *               that the deque will increase its capacity by 50%.
	 */
	OtlDeque(float growth = 1.5f);

	/**
	 * Destructor.
	 *
	 * If the deque still contains elements, the destructor will be called on
	 * each element, and the underlying memory will be released.
	 */
	~OtlDeque();

	/**
	 * Get the last element in the deque.
	 *
	 * Must not be called on empty deques.
	 *
	 * @return The last element in the deque.
	 */
	inline T& Back() const;

	/**
	 * Erase all elements in the deque.
	 *
	 * This calls the destructor for all elements in the deque, and sets the
	 * size of the deque to zero.
	 *
	 * This may be used on an empty deque.
	 *
	 * The destructors of the elements are called when they are erased.
	 * However, note that the underlying memory consumed by the elements is not
	 * automatically released (it is kept for reuse later). Use Trim() to
	 * release surplus capacity when required.
	 *
	 * @see Trim
	 */
	inline void Clear();

	/**
	 * Erase an element at the specified index.
	 *
	 * When an element is erased, the destructor of that element is called,
	 * and remaining elements are shifted to close the gap.
	 *
	 * This function may be used to erase end elements, and no memory will be
	 * moved in this case.
	 *
	 * Erasing an index that does not exist will cause undefined behaviour.
	 *
	 * @see EraseByValue
	 * @see PopBack
	 * @see PopFront
	 *
	 * @param index The index of the element to erase.
	 */
	void Erase(size_t index);

	/**
	 * Find an element in the deque and erase it.
	 *
	 * This function will look for an element with the specified value and
	 * erase it (if found).
	 *
	 * @see Erase
	 * @see Find
	 *
	 * @param element The element to find and destroy.
	 * @param offset Start searching from this offset. Default: 0.
	 *
	 * @return See OpStatus.
	 * @retval OpStatus::OK if an element was found and erased successfully.
	 * @retval OpStatus::ERR if no element was found (or erased).
	 */
	OP_STATUS EraseByValue(const T& element, size_t offset = 0);

	/**
	 * Find the index of an element.
	 *
	 * This function goes through the array of elements, and compares each
	 * each element with the provided element, using the equality operator
	 * (operator==).
	 *
	 * If the value appears repeatedly, the first index at which it appears
	 * is returned.
	 *
	 * @param element The element to look for.
	 * @param offset  Start searching from this offset. Default: 0.
	 *
	 * @return The index of the element, or -1 if the element was not found.
	 */
	int Find(const T& element, size_t offset = 0) const;

	/**
	 * Get the first element in the deque.
	 *
	 * Must not be called on an empty deque.
	 *
	 * @return The first element in the deque.
	 */
	inline T& Front() const;

	/**
	 * Get the capacity of the deque.
	 *
	 * The capacity is the number of elements the deque can hold without
	 * reallocating the elements.
	 *
	 * The capacity includes any elements that are already in the deque, and
	 * indicates the total capacity of the deque, not the "free space".
	 *
	 * Immediately after construction, before elements are added, capacity is
	 * always zero.
	 *
	 * @return The capacity of the deque.
	 */
	size_t GetCapacity() const { return m_capacity; }

	/**
	 * Get the growth factor of the deque.
	 *
	 * The growth factor was previously set during construction, or using
	 * SetGrowthFactor().
	 *
	 * @see SetGrowthFactor
	 *
	 * @return The growth factor of the deque.
	 */
	float GetGrowthFactor() const { return m_growth; }

	/**
	 * Get the size of the deque.
	 *
	 * The size of the deque is the number of elements in the deque. The size
	 * of the deque must not be confused with its capacity; the capacity is
	 * the maximum size of the deque before a reallocation is required.
	 *
	 * @return The number of elements in the deque.
	 */
	size_t GetSize() const { return m_size; }

	/**
	 * Insert an element at an arbitrary position in the deque.
	 *
	 * The element will be copied into the deque using the copy constructor
	 * of the element.
	 *
	 * When an element is inserted, elements are shifted in order to free the
	 * slot at @p index. The specified element is then copied into this slot.
	 *
	 * This function may be used to insert an element at the end of the deque
	 * (i.e. at the "next" index which currently does not exist), and memory
	 * will not be moved in this case. Inserting an item beyond the end, or
	 * with a negative index, is prohibited and may cause undefined behaviour.
	 *
	 * Reallocation will occur if the deque is full. However, if the capacity
	 * is larger than the size, reallocation is guaranteed to @e not occur.
	 *
	 * @see PushBack
	 * @see PushFront
	 *
	 * @param index   The point of insertion. Valid range: [0, GetSize()].
	 * @param element The element to insert.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if reallocation fails.
	 */
	OP_STATUS Insert(size_t index, const T& element);

	/**
	 * Check whether the deque is empty.
	 *
	 * A deque is empty if GetSize() == 0 (capacity is irrelevant).
	 *
	 * @see GetSize
	 *
	 * @return @c true if the deque is empty.
	 */
	bool IsEmpty() const { return m_size == 0; }

	/**
	 * Erase the last element in the deque.
	 *
	 * It is not allowed to call this function on an empty deque, so please
	 * make sure that this is not the case before calling this function.
	 *
	 * The destructor of the element is called when it is erased. However,
	 * note that the underlying memory consumed by the element is not
	 * automatically released (it is kept for reuse later). Use Trim() to
	 * release surplus capacity when required.
	 *
	 * @see Clear
	 * @see Erase
	 * @see Trim
	 */
	void PopBack();

	/**
	 * Erase the first element in the deque.
	 *
	 * It is not allowed to call this function on an empty deque, so please
	 * make sure that this is not the case before calling this function.
	 *
	 * The destructor of the element is called when it is erased. However,
	 * note that the underlying memory consumed by the element is not
	 * automatically released (it is kept for reuse later). Use Trim() to
	 * release surplus capacity when required.
	 *
	 * @see Clear
	 * @see Erase
	 * @see Trim
	 */
	void PopFront();

	/**
	 * Add an element at the end of the deque, after the current last element.
	 *
	 * The element will be copied into the deque using the copy constructor
	 * of the element.
	 *
	 * Reallocation will occur if the deque is full. However, if the capacity
	 * is larger than the size, reallocation is guaranteed to @e not occur.
	 *
	 * @see GetCapacity
	 * @see GetSize
	 * @see Reserve
	 *
	 * @param element The element to add.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if reallocation fails.
	 */
	OP_STATUS PushBack(const T& element);

	/**
	 * Add an element at the beginning of the deque, before the current first
	 * element.
	 *
	 * The element will be copied into the deque using the copy constructor
	 * of the element.
	 *
	 * Reallocation will occur if the deque is full. However, if the capacity
	 * is larger than the size, reallocation is guaranteed to @e not occur.
	 *
	 * @see GetCapacity
	 * @see GetSize
	 * @see Reserve
	 *
	 * @param element The element to add.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if reallocation fails.
	 */
	OP_STATUS PushFront(const T& element);

	/**
	 * Ensure sufficient capacity for anticipated use.
	 *
	 * If the specified capacity is larger than the current capacity, the
	 * deque will grow to the specified capacity.
	 *
	 * Please note that 'capacity' refers to @e total capacity, and includes any
	 * elements that may already be present in the deque. 'Capacity' does not
	 * mean "free space".
	 *
	 * As long as the capacity is larger than the current size, calls to
	 * PushBack(), PushFront() and Insert() can not fail. It is therefore
	 * beneficial to reserve an adequate amount of capacity in advance if we
	 * know that we shortly will be adding many elements to the deque. By doing
	 * that, we prevent unnecessary reallocations, and we do not have to check
	 * the return value of PushBack(), PushFront() and Insert().
	 *
	 * @see PushBack
	 * @see PushFront
	 * @see Insert
	 *
	 * @param capacity The desired capacity of the deque.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if allocation fails.
	 */
	OP_STATUS Reserve(int capacity);

	/**
	 * Set the growth factor of the deque.
	 *
	 * The growth factor defines how much to expand the total capacity of the
	 * deque when the deque needs to grow. Because moving large memory blocks
	 * is expensive, it can be beneficial to allocate space for a certain
	 * number of elements in advance, so that memory copying does not have to
	 * take place all the time.
	 *
	 * When the deque needs to grow, the new capacity is calculated by
	 * multiplying the old capacity by the growth factor. However, if this
	 * calculation did not result in a capacity increase (for instance if the
	 * current capacity is zero), then the deque will grow by one element
	 * instead.
	 *
	 * @see Reserve
	 *
	 * @param growth How much to increase the total capacity of the deque
	 *               when the deque needs to grow. 1.5f is a good value, which
	 *               means the capacity will increase by 50%.
	 */
	inline void SetGrowthFactor(float growth);

	/**
	 * Release surplus capacity.
	 *
	 * If the current capacity is larger than the current size, calling
	 * this function will reduce the capacity to the current size. This
	 * yields the smallest possible deque memory footprint.
	 *
	 * Unless elements are erased or capacity is reserved, the first call to
	 * PushBack(), PushFront() or Insert(), after calling this function, @e will
	 * trigger a reallocation.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if reallocation fails.
	 */
	OP_STATUS Trim();

	/**
	 * Access the element at a certain index.
	 *
	 * It is not allowed to access an index which does not exist. You must
	 * always make sure that the the index is at least zero and less than the
	 * current size.
	 *
	 * A nice, safe way to iterate through elements (from first to last
	 * element in the queue) is:
	 *
	 * @code
	 *  for (size_t i = 0; i < deque.GetSize(); ++i)
	 *    deque[i].DoSomething();
	 * @endcode
	 */
	inline T& operator[](size_t index) const;

private:
	/**
	 * Return a pointer to the first invalid element slot past the end
	 * of the (non-circular) array.
	 *
	 * @return A pointer to the end of the array.
	 */
	T* End() const { return m_elements + m_capacity; }

	/**
	 * Grow the size of the array according to growth factor.
	 *
	 * @param min Minimum number of new free slots.
	 * @return May return OpStatus::ERR_NO_MEMORY if reallocation fails.
	 */
	OP_STATUS Grow(size_t min);

	/**
	 * Change the capacity of the array.
	 *
	 * This may be used to increase the capacity of the array, or erase
	 * unused capacity. It may not be used to decrease the @e size of the
	 * array, i.e. it is not allowed to call this function with @p capacity
	 * set to a value less than @c m_size.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if reallocation fails.
	 */
	OP_STATUS Reallocate(size_t capacity);

	/// The max number of elements before the deque needs to grow.
	size_t m_capacity;

	/// The number of elements in the deque.
	size_t m_size;

	/// The factor to increase the capacity by when the deque needs to grow.
	float m_growth;

	/// Pointer to the last element in the deque.
	T* m_back;

	/// Pointer to the first element in the deque.
	T* m_front;

	/// Pointer to the array of elements.
	T* m_elements;

	/// Intentionally left undefined.
	OtlDeque(const OtlDeque&);

	/// Intentionally left undefined.
	OtlDeque& operator=(const OtlDeque&);
};

template<typename T>
OtlDeque<T>::OtlDeque(float growth)
	: m_capacity(0)
	, m_size(0)
	, m_growth(growth)
	, m_back(NULL)
	, m_front(NULL)
	, m_elements(NULL)
{
	OP_ASSERT(growth >= 1.0f || !"OtlDeque: Invalid growth factor");
}

template<typename T>
OtlDeque<T>::~OtlDeque()
{
	Clear();
	op_free(m_elements);
}

template<typename T>
T& OtlDeque<T>::Back() const
{
	OP_ASSERT(m_size > 0 || !"OtlDeque::Back: Buffer is empty");
	return *m_back;
}

template<typename T>
void OtlDeque<T>::Clear()
{
	for (size_t i = 0; i < m_size; ++i)
	{
		m_back->~T();
		if (m_back == m_elements)
			m_back = End();
		--m_back;
	}
	m_size = 0;
}

template<typename T>
void OtlDeque<T>::Erase(size_t index)
{
	OP_ASSERT(index < m_size || !"OtlDeque::Erase: Index out of bounds");

	T* const target = &(*this)[index];
	T* const last = End() - 1;

	target->~T();

	if (target <= m_back)
	{
		op_memmove(target, target + 1, (m_back - target) * sizeof(T));
		m_back = (m_back == m_elements) ? last : m_back - 1;
	}
	else if (target >= m_front)
	{
		op_memmove(m_front + 1, m_front, (target - m_front) * sizeof(T));
		m_front = (m_front == last) ? m_elements : m_front + 1;
	}

	--m_size;

	/**
	 * Removing the last element may leave 'm_front' and 'm_back' pointing
	 * to somewhere in the middle of the array, but it is not necessary to
	 * reset these pointers to initial values. As long as the pointers are
	 * correctly placed relative to each other, it does not matter where in
	 * the array we begin.
	 */
}

template<typename T>
OP_STATUS OtlDeque<T>::EraseByValue(const T& element, size_t offset)
{
	int index = Find(element, offset);

	if (index < 0)
		return OpStatus::ERR;

	Erase(index);

	return OpStatus::OK;
}

template<typename T>
T& OtlDeque<T>::Front() const
{
	OP_ASSERT(m_size > 0 || !"OtlDeque::Front: Buffer is empty");
	return *m_front;
}

template<typename T>
int OtlDeque<T>::Find(const T& element, size_t i) const
{
	for (; i < m_size; ++i)
		if ((*this)[i] == element)
			return i;
	return -1; // Not found.
}

template<typename T>
OP_STATUS OtlDeque<T>::Insert(size_t index, const T& element)
{
	OP_ASSERT(index <= m_size || !"OtlDeque::Insert: Index out of bounds");

	if (index == 0)
		return PushFront(element);
	else if (index == m_size)
		return PushBack(element);

	if (m_size == m_capacity && OpStatus::IsError(Grow(1)))
		return OpStatus::ERR_NO_MEMORY;

	T* target = &(*this)[index];
	T* const last = End() - 1;

	if (target <= m_back && m_back < last)
		op_memmove(target + 1, target, (++m_back - target) * sizeof(T));
	else // We can safely assume that (m_front > m_elements).
	{
		--m_front;
		op_memmove(m_front, m_front + 1, ((--target) - m_front) * sizeof(T));
	}

	new (target) T(element);
	++m_size;

	return OpStatus::OK;
}

template<typename T>
void OtlDeque<T>::PopBack()
{
	OP_ASSERT(m_size > 0 || !"OtlDeque::PopBack: Empty deque");

	m_back->~T();

	if (m_back == m_elements)
		m_back = End();

	--m_back;
	--m_size;
}

template<typename T>
void OtlDeque<T>::PopFront()
{
	OP_ASSERT(m_size > 0 || !"OtlDeque::PopFront: Empty deque");

	m_front->~T();
	if (++m_front == End())
		m_front = m_elements;

	--m_size;
}

template<typename T>
OP_STATUS OtlDeque<T>::PushBack(const T& element)
{
	if (m_size == m_capacity && OpStatus::IsError(Grow(1)))
		return OpStatus::ERR_NO_MEMORY;

	// Wrap around if m_back is at the end.
	if (++m_back == End())
		m_back = m_elements;

	new (m_back) T(element);
	++m_size;
	return OpStatus::OK;
}

template<typename T>
OP_STATUS OtlDeque<T>::PushFront(const T& element)
{
	if (m_size == m_capacity && OpStatus::IsError(Grow(1)))
		return OpStatus::ERR_NO_MEMORY;

	// Wrap around if m_front is at the beginning.
	if (m_front == m_elements)
		m_front = End();

	new (--m_front) T(element);
	++m_size;
	return OpStatus::OK;
}

template<typename T>
OP_STATUS OtlDeque<T>::Reserve(int capacity)
{
	OP_ASSERT(capacity >= 0 || !"OtlDeque::Reserve: Invalid buffer size");

	if (capacity <= 0 || static_cast<size_t>(capacity) <= m_capacity)
		return OpStatus::OK;

	return Reallocate(capacity);
}

template<typename T>
void OtlDeque<T>::SetGrowthFactor(float growth)
{
	OP_ASSERT(growth > 1.0f || !"OtlDeque: Invalid growth factor");
	m_growth = growth;
}

template<typename T>
OP_STATUS OtlDeque<T>::Trim()
{
	if (m_size == m_capacity)
		return OpStatus::OK; // Nothing to trim.

	return Reallocate(m_size);
}

template<typename T>
T& OtlDeque<T>::operator[](size_t index) const
{
	OP_ASSERT(m_size > 0 || !"OtlDeque::operator[]: Buffer is empty");
	OP_ASSERT(index < m_size || !"OtlDeque::operator[]: Index out of bounds");

	return m_elements[((m_front - m_elements) + index) % m_capacity];
}

template<typename T>
OP_STATUS OtlDeque<T>::Grow(size_t min)
{
	size_t new_cap = static_cast<size_t>(m_capacity * m_growth);
	size_t min_cap = m_capacity + min;

	return Reallocate(new_cap > min_cap ? new_cap : min_cap);
}

template<typename T>
OP_STATUS OtlDeque<T>::Reallocate(size_t capacity)
{
	T* new_array = static_cast<T*>(op_malloc(capacity * sizeof(T)));
	if (!new_array)
		return OpStatus::ERR_NO_MEMORY;

	if (m_back < m_front)
	{
		const size_t size = End() - m_front;
		op_memcpy(new_array, m_front, size * sizeof(T));
		op_memcpy(new_array + size, m_elements, (m_size - size) * sizeof(T));
	}
	else
		op_memcpy(new_array, m_front, m_size * sizeof(T));
	op_free(m_elements);

	m_elements = new_array;
	m_capacity = capacity;

	m_front = m_elements;
	m_back = m_elements + (m_size ? m_size : m_capacity) - 1;

	return OpStatus::OK;
}

#endif // MODULES_OTL_DEQUE_H
