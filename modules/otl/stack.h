/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_OTL_STACK_H
#define MODULES_OTL_STACK_H

#include "modules/otl/vector.h"

/**
 * A stack is a last in, first out (LIFO) data type.
 *
 * A stack is a restriced data structure with two fundamental operators:
 *
 * - Push() adds an item on top of the stack. If the stack is empty, it is
 *   first initialized before the item is added.
 * - Pop() removes the top item. Popping an empty stack is undefined
 *   behaviour (so don't do it).
 *
 * The top item is the only item that is accessible (using Top()).
 *
 * Basic usage:
 *
 * @code
 *  OtlStack<int> stack;
 *  RETURN_IF_ERROR(stack.Push(1));
 *  RETURN_IF_ERROR(stack.Push(2));
 *  RETURN_IF_ERROR(stack.Push(3));
 *  bool test1 = (stack.Top() == 3); // true
 *  RETURN_IF_ERROR(stack.Pop());
 *  bool test2 = (stack.Top() == 2); // true
 * @endcode
 */
template<typename T>
class OtlStack : private OtlVector<T>
{
public:
	/**
	 * Construct a new stack.
	 *
	 * You may choose the allocation growth factor during construction of the
	 * stack. The growth factor controls how much larger a chunk of memory
	 * the stack allocates every time it runs out of space.
	 *
	 * Do not hold references or pointers to individual elements on the stack;
	 * they will be invalidated whenever the stack needs to grow.
	 *
	 * No allocation takes place until something is pushed onto the stack.
	 * However, if you know the number of elements beforehand, you can
	 * Reserve() the amount you need and avoid excessive reallocations.
	 *
	 * @see Reserve
	 * @see SetGrowthFactor
	 *
	 * @param growth How much to increase the total capacity of the stack when
	 *               the stack needs to grow. The default value of 1.5f means
	 *               that the stack will increase its capacity by 50%.
	 */
	inline OtlStack(float growth = 1.5f);

	/**
	 * Pop all elements off the stack.
	 *
	 * This calls the destructor for all elements in the stack, and sets the
	 * size of the stack to zero. The destructors are called in order from
	 * top to bottom, so calling this function is equivalent to repeatedly
	 * call Pop() until the stack is empty.
	 *
	 * This may be used on an empty stack.
	 *
	 * The destructors of the elements are called when they are erased.
	 * However, note that the underlying memory consumed by the elements is not
	 * automatically released (it is kept for reuse later). Use Trim() to
	 * release surplus capacity when required.
	 *
	 * @see Trim
	 */
	void Clear() { OtlVector<T>::Clear(); }

	/**
	 * Get the capacity of the stack.
	 *
	 * The capacity is the number of elements the stack can hold without
	 * reallocating the elements.
	 *
	 * The capacity includes any elements that are already on the stack, and
	 * indicates the total capacity of the stack, not the "free space".
	 *
	 * Immediately after construction, before elements are added, capacity is
	 * always zero.
	 *
	 * @return The capacity of the stack.
	 */
	size_t GetCapacity() const { return OtlVector<T>::GetCapacity(); }

	/**
	 * Get the growth factor of the stack.
	 *
	 * The growth factor was previously set during construction, or using
	 * SetGrowthFactor().
	 *
	 * @see GetGrowthFactor
	 *
	 * @return The growth factor of the stack.
	 */
	float GetGrowthFactor() const { return OtlVector<T>::GetGrowthFactor(); }

	/**
	 * Get the size of the stack.
	 *
	 * The size of the stack is the number of elements on the stack. The size
	 * of the stack must not be confused with its capacity; the capacity is
	 * the maximum size of the stack before a reallocation is required.
	 *
	 * @return The number of elements on the stack.
	 */
	size_t GetSize() const { return OtlVector<T>::GetSize(); }

	/**
	 * Check whether the stack is empty.
	 *
	 * A stack is empty if its size is zero (capacity is irrelevant).
	 *
	 * @see GetSize
	 *
	 * @return @c true if the stack is empty.
	 */
	bool IsEmpty() const { return OtlVector<T>::IsEmpty(); }

	/**
	 * Pop the top element off the stack.
	 *
	 * Popping an empty stack is not allowed, and will produce undefined
	 * behaviour. Please make sure that the stack has at least one element
	 * before calling this function.
	 *
	 * The destructor of the element is called when it is erased. However,
	 * note that the underlying memory consumed by the element is not
	 * automatically released (it is kept for reuse later). Use Trim() to
	 * release surplus capacity when required.
	 *
	 * @see Clear
	 * @see Trim
	 */
	void Pop() { OtlVector<T>::PopBack(); }

	/**
	 * Push an element onto the stack.
	 *
	 * The element will be copied onto the stack using the copy constructor
	 * of the element.
	 *
	 * Reallocation will occur if the stack is full. However, if the capacity
	 * is larger than the size, reallocation is guaranteed to @e not occur.
	 *
	 * @param element The element to add.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if reallocation fails.
	 */
	OP_STATUS Push(const T& element) { return OtlVector<T>::PushBack(element); }

	/**
	 * Ensure sufficient capacity for anticipated use.
	 *
	 * If the specified capacity is larger than the current capacity, the
	 * stack will grow to the specified capacity.
	 *
	 * Please note that @c capacity refers to @e total capacity, and includes any
	 * elements that may already be present in the stack. @c Capacity does not
	 * mean "free space".
	 *
	 * As long as the capacity is larger than the current size, calls to
	 * Pop() can not fail. It is therefore beneficial to reserve an adequate
	 * amount of capacity in advance if we know that we shortly will be adding
	 * many elements to the stack. By doing that, we prevent unnecessary
	 * reallocations, and we do not have to check the return value of Pop().
	 *
	 * @see Pop
	 *
	 * @param capacity The desired capacity of the stack.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if allocation fails.
	 */
	OP_STATUS Reserve(int capacity) { return OtlVector<T>::Reserve(capacity); }

	/**
	 * Set the growth factor of the stack.
	 *
	 * The growth factor defines how much to expand the total capacity of the
	 * stack when the stack needs to grow. Because moving large memory blocks
	 * is expensive, it can be beneficial to allocate space for a certain
	 * number of elements in advance, so that memory copying does not have to
	 * take place all the time.
	 *
	 * When the stack needs to grow, the new capacity is calculated by
	 * multiplying the old capacity with the growth factor. However, if this
	 * calculation did not result in a capacity increase (for instance if the
	 * current capacity is zero), then the stack will grow by one element
	 * instead.
	 *
	 * @see Reserve
	 *
	 * @param growth How much to increase the total capacity of the stack when
	 *               the stack needs to grow. 1.5f is a good value, which means
	 *               the capacity will increase by 50%.
	 */
	void SetGrowthFactor(float growth) { OtlVector<T>::SetGrowthFactor(growth); }

	/**
	 * Get the top element on the stack.
	 *
	 * Must not be called if the stack is empty.
	 *
	 * @return The top element on the stack.
	 */
	inline T& Top() const;

	/**
	 * Release surplus capacity.
	 *
	 * If the current capacity is larger than the current size, calling
	 * this function will reduce the capacity to the current size. This
	 * yields the smallest possible stack memory footprint.
	 *
	 * Unless elements are popped or capacity is reserved, any subsequent
	 * calls to Push() @e will trigger reallocation after calling this
	 * function.
	 *
	 * @return May return OpStatus::ERR_NO_MEMORY if reallocation fails.
	 */
	OP_STATUS Trim() { return OtlVector<T>::Trim(); }
};

template<typename T>
OtlStack<T>::OtlStack(float growth) : OtlVector<T>(growth) { }

template<typename T>
T& OtlStack<T>::Top() const
{
	OP_ASSERT(GetSize() > 0 || !"OtlStack::Top: OtlStack is empty");

	return OtlVector<T>::operator[](GetSize() - 1);
}

#endif // MODULES_OTL_STACK_H
