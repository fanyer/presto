/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_OTL_VECTOR_H
#define MODULES_OTL_VECTOR_H

/**
 * Template for sequence containers.
 *
 * Vectors store objects in a strict linear sequence, expanding the storage
 * space as the current capacity is exhausted.
 *
 * This class is different from OpVector, because it may be used to store
 * a sequence of any type, e.g. @c int, @c double, @c uni_char* and of
 * course, classes and structures. Unlike OpVector, which always stores an
 * array of pointers, OtlVector allocates a (configurable) memory chunk based
 * on the size of the type you want to store (not a @e pointer to the type
 * you want to store).
 *
 * Beware: when objects are added to the vector, the objects are @e copied
 * (using the copy constructor of the object), which may potentially be an
 * expensive operation. If you intend to store non-lightweight objects, you
 * should consider allocating the objects on the heap, and merely store
 * pointers to the objects instead.
 *
 * Also, make sure that you define a copy constructor if the class you wish
 * to store does not qualify as a "plain old data structure". In other words,
 * if you do not define a copy constructor, make sure the compiler generated
 * copy constructor copies the object adequately.
 *
 * @see http://en.wikipedia.org/wiki/Plain_old_data_structure
 *
 * Basic usage:
 *
 * @code
 *  OtlVector<int> vector;
 *  RETURN_IF_ERROR(vector.PushBack(42));  // Add an item.
 *  int answer = vector[0]; // Accessing item at index '0'.
 *  vector.Erase(0); // Erase item at index '0'.
 * @endcode
 *
 * Looping through items:
 *
 * @code
 *  for (size_t i = 0; i < vector.GetSize(); ++i)
 *    vector[i].DoSomething();
 * @endcode
 */
template<typename T>
class OtlVector
{
public:
	/**
	 * Construct a new vector.
	 *
	 * You may choose the allocation growth factor during construction of the
	 * vector. The growth factor controls how much larger a chunk of memory
	 * the vector allocates every time it runs out of space.
	 *
	 * Do not hold references or pointers to individual elements in the vector;
	 * they will be invalidated whenever the vector needs to grow.
	 *
	 * No allocation takes place until something is added to the vector using
	 * PushBack() or Insert(), regardless of growth factor. However, if you
	 * know the number of elements beforehand, you can Reserve() the amount
	 * you need and avoid excessive reallocations.
	 *
	 * @see Reserve
	 * @see SetGrowthFactor
	 *
	 * @param growth How much to increase the total capacity of the vector when
	 *               the vector needs to grow. The default value of 1.5f means
	 *               that the vector will increase its capacity by 50%.
	 */
	OtlVector(float growth = 1.5f);

	/**
	 * Destructor.
	 *
	 * If the vector still contains elements, the destructor will be called on
	 * each element, and the underlying memory will be released. Elements are
	 * destroyed in reverse order (largest index first).
	 */
	~OtlVector();

	/**
	 * Erase all elements in the vector.
	 *
	 * This calls the destructor for all elements in the vector, and sets the
	 * size of the vector to zero. The destructors are called in reverse order
	 * (largest index first).
	 *
	 * This may be used on an empty vector.
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
	 * and then all elements with an index greater than @c index are shifted
	 * left (closing the gap).
	 *
	 * This function may be used to erase the last element, and no memory
	 * will be moved in this case.
	 *
	 * It is not allowed to erase an index which does not exist.
	 *
	 * @see PopBack
	 * @see EraseByValue
	 *
	 * @param index The index of the element to erase.
	 */
	void Erase(size_t index);

	/**
	 * Find an element in the vector and erase it.
	 *
	 * This function will look for an element with the specified value and
	 * erase it (if found).
	 *
	 * @see Find
	 * @see Erase
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
	 * Get the capacity of the vector.
	 *
	 * The capacity is the number of elements the vector can hold without
	 * reallocating the elements.
	 *
	 * The capacity includes any elements that are already in the vector, and
	 * indicates the total capacity of the vector, not the "free space".
	 *
	 * Immediately after construction, before elements are added, capacity is
	 * always zero.
	 *
	 * @return The capacity of the vector.
	 */
	size_t GetCapacity() const { return m_capacity; }

	/**
	 * Get the growth factor of the vector.
	 *
	 * The growth factor was previously set during construction, or using
	 * SetGrowthFactor().
	 *
	 * @see GetGrowthFactor
	 *
	 * @return The growth factor of the vector.
	 */
	float GetGrowthFactor() const { return m_growth; }

	/**
	 * Get the size of the vector.
	 *
	 * The size of the vector is the number of elements in the vector. The size
	 * of the vector must not be confused with its capacity; the capacity is
	 * the maximum size of the vector before a reallocation is required.
	 *
	 * @return The number of elements in the vector.
	 */
	size_t GetSize() const { return m_size; }

	/**
	 * Insert an element at an arbitrary position in the vector.
	 *
	 * When an element is inserted, all elements with an index equal to or
	 * larger than @c index are shifted right, and the specified element is
	 * then copied into the now free slot at @c index.
	 *
	 * This function may be used to insert an element at the end of the vector
	 * (i.e. at the "next" index which currently does not exist), and memory
	 * will not be moved in this case. Inserting an item beyond the end is
	 * prohibited, and may cause undefined behaviour.
	 *
	 * Reallocation will occur if the vector is full. However, if the capacity
	 * is larger than the size, reallocation is guaranteed to @e not occur.
	 *
	 * @see PushBack
	 *
	 * @param index   The point of insertion.
	 * @param element The element to insert.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if reallocation fails.
	 */
	OP_STATUS Insert(size_t index, const T& element);

	/**
	 * Check whether the vector is empty.
	 *
	 * A vector is empty if its size is zero (capacity is irrelevant).
	 *
	 * @see GetSize
	 *
	 * @return @c true if the vector is empty.
	 */
	bool IsEmpty() const { return m_size == 0; }

	/**
	 * Erase the element at the end of the vector.
	 *
	 * It is not allowed to call this function on an empty vector, so please
	 * make sure that this is not the case before calling this function.
	 *
	 * The destructor of the element is called when it is erased. However,
	 * note that the underlying memory consumed by the element is not
	 * automatically released (it is kept for reuse later). Use Trim() to
	 * release surplus capacity when required.
	 *
	 * @see Trim
	 * @see Erase
	 * @see Clear
	 */
	void PopBack();

	/**
	 * Add an element at the end of the vector.
	 *
	 * The element will be copied into the vector using the copy constructor
	 * of the element.
	 *
	 * Reallocation will occur if the vector is full. However, if the capacity
	 * is larger than the size, reallocation is guaranteed to @e not occur.
	 *
	 * @see Reserve
	 * @see GetCapacity
	 * @see GetSize
	 *
	 * @param element The element to add.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if reallocation fails.
	 */
	OP_STATUS PushBack(const T& element);

	/**
	 * Ensure sufficient capacity for anticipated use.
	 *
	 * If the specified capacity is larger than the current capacity, the
	 * vector will grow to the specified capacity.
	 *
	 * Please note that @c capacity refers to @e total capacity, and includes any
	 * elements that may already be present in the vector. @c Capacity does not
	 * mean "free space".
	 *
	 * As long as the capacity is larger than the current size, calls to
	 * PushBack() and Insert() can not fail. It is therefore beneficial to
	 * reserve an adequate amount of capacity in advance if we know that we
	 * shortly will be adding many elements to the vector. By doing that, we
	 * prevent unnecessary reallocations, and we do not have to check the
	 * return value of PushBack() and Insert().
	 *
	 * @see PushBack
	 * @see Insert
	 *
	 * @param capacity The desired capacity of the vector.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if allocation fails.
	 */
	OP_STATUS Reserve(int capacity);

	/**
	 * Set the growth factor of the vector.
	 *
	 * The growth factor defines how much to expand the total capacity of the
	 * vector when the vector needs to grow. Because moving large memory blocks
	 * is expensive, it can be beneficial to allocate space for a certain
	 * number of elements in advance, so that memory copying does not have to
	 * take place all the time.
	 *
	 * When the vector needs to grow, the new capacity is calculated by
	 * multiplying the old capacity by the growth factor. However, if this
	 * calculation did not result in a capacity increase (for instance if the
	 * current capacity is zero), then the vector will grow by one element
	 * instead.
	 *
	 * @see Reserve
	 *
	 * @param growth How much to increase the total capacity of the vector
	 *               when the vector needs to grow. 1.5f is a good value, which
	 *               means the capacity will increase by 50%.
	 */
	inline void SetGrowthFactor(float growth);

	/**
	 * Release surplus capacity.
	 *
	 * If the current capacity is larger than the current size, calling
	 * this function will reduce the capacity to the current size. This
	 * yields the smallest possible vector memory footprint.
	 *
	 * Unless elements are erased or capacity is reserved, any subsequent
	 * calls to PushBack() or Insert() @e will trigger reallocation after
	 * calling this function.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if reallocation fails.
	 */
	OP_STATUS Trim();

	/**
	 * Access the element at a certain index.
	 *
	 * It is not allowed to access an index which does not exist. You must
	 * always make sure that the vector is not empty, that the index is
	 * larger than zero and less than the current size.
	 *
	 * A nice, safe way to iterate through elements is:
	 *
	 * @code
	 *  for (size_t i = 0; i < vector.GetSize(); ++i)
	 *    vector[i].DoSomething();
	 * @endcode
	 */
	inline T& operator[](size_t index) const;

private:
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
	 * array, i.e. it is not allowed to call this function with @c capacity
	 * set to a value less than @c m_size.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if reallocation fails.
	 */
	OP_STATUS Reallocate(size_t capacity);

	/// The factor to increase the capacity by when the vector needs to grow.
	float m_growth;

	/// The number of elements in the vector.
	size_t m_size;

	/// The max number of elements before the vector needs to grow.
	size_t m_capacity;

	/// Pointer to the array of elements.
	T* m_elements;

	/// Intentionally left undefined.
	OtlVector(const OtlVector<T>& vector);

	/// Intentionally left undefined.
	OtlVector<T>& operator=(const OtlVector<T>& vector);
};

template<typename T>
OtlVector<T>::OtlVector(float growth)
	: m_growth(growth)
	, m_size(0)
	, m_capacity(0)
	, m_elements(NULL)
{
	OP_ASSERT(m_growth >= 1.0 || !"OtlVector: Invalid growth factor");
}

template<typename T>
OtlVector<T>::~OtlVector()
{
	Clear();
	op_free(m_elements);
}

template<typename T>
void OtlVector<T>::Clear()
{
	for (size_t i = m_size; i-- > 0 ;)
		m_elements[i].~T();

	m_size = 0;
}

template<typename T>
void OtlVector<T>::Erase(size_t index)
{
	OP_ASSERT(index < m_size || !"OtlVector: Index out of bounds");

	// Destroy the element.
	m_elements[index].~T();

	// Shift all elements after the erased element.
	op_memmove(m_elements + index, m_elements + index + 1, ((--m_size) - index) * sizeof(T));
}

template<typename T>
OP_STATUS OtlVector<T>::EraseByValue(const T& element, size_t offset)
{
	int index = Find(element, offset);

	if (index < 0)
		return OpStatus::ERR;

	Erase(index);

	return OpStatus::OK;
}

template<typename T>
int OtlVector<T>::Find(const T& element, size_t i) const
{
	for (; i < m_size; ++i)
		if (m_elements[i] == element)
			return i;
	return -1; // Not found.
}

template<typename T>
OP_STATUS OtlVector<T>::Insert(size_t index, const T& element)
{
	OP_ASSERT(index <= m_size || !"OtlVector: Index out of bounds");

	if (m_size == m_capacity && OpStatus::IsError(Grow(1)))
		return OpStatus::ERR_NO_MEMORY;

	// Shift all elements at the insertion point.
	op_memmove(m_elements + index + 1, m_elements + index, (m_size++ - index) * sizeof(T));

	// Placement new.
	new (m_elements + index) T(element);

	return OpStatus::OK;
}

template<typename T>
void OtlVector<T>::PopBack()
{
	OP_ASSERT(m_size > 0 || !"OtlVector::PopBack: OtlVector is empty");

	m_elements[--m_size].~T();
}

template<typename T>
OP_STATUS OtlVector<T>::PushBack(const T& element)
{
	if (m_size == m_capacity)
	{
		if (OpStatus::IsError(Grow(1)))
			return OpStatus::ERR_NO_MEMORY;

		// Returning here produces faster code with GCC.
		new (m_elements + m_size++) T(element);
		return OpStatus::OK;
	}

	// Placement new.
	new (m_elements + m_size++) T(element);
	return OpStatus::OK;
}

template<typename T>
OP_STATUS OtlVector<T>::Reserve(int capacity)
{
	OP_ASSERT(capacity >= 0 || !"OtlVector: Invalid array size");

	if (static_cast<size_t>(capacity) <= m_capacity)
		return OpStatus::OK;

	return Reallocate(capacity);
}

template<typename T>
void OtlVector<T>::SetGrowthFactor(float growth)
{
	OP_ASSERT(growth >= 1 || !"OtlVector: Invalid growth factor");
	m_growth = growth;
}

template<typename T>
OP_STATUS OtlVector<T>::Trim()
{
	if (m_size == m_capacity)
		return OpStatus::OK; // Nothing to trim.

	return Reallocate(m_size);
}

template<typename T>
T& OtlVector<T>::operator[](size_t index) const
{
	OP_ASSERT(index < m_size || !"OtlVector: Index out of bounds");
	return m_elements[index];
}

template<typename T>
OP_STATUS OtlVector<T>::Grow(size_t min)
{
	size_t new_cap = static_cast<size_t>(m_capacity * m_growth);
	size_t min_cap = m_capacity + min;

	return Reallocate(new_cap > min_cap ? new_cap : min_cap);
}

template<typename T>
OP_STATUS OtlVector<T>::Reallocate(size_t capacity)
{
	OP_ASSERT(capacity >= m_size);

	T* new_array = static_cast<T*>(op_malloc(sizeof(T) * capacity));
	if (!new_array)
		return OpStatus::ERR_NO_MEMORY;

	op_memcpy(new_array, m_elements, m_size * sizeof(T));
	op_free(m_elements);

	m_elements = new_array;
	m_capacity = capacity;

	return OpStatus::OK;
}

#endif // MODULES_OTL_VECTOR_H
