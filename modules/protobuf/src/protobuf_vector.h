/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_PROTOBUF_VECTOR_H
#define OP_PROTOBUF_VECTOR_H

#ifdef PROTOBUF_SUPPORT

#include "modules/util/adt/opvector.h"

/**
 * Implements a vector that operates on values instead of pointers
 * (like OpVector does) and uses the copy-constructor and assignment
 * operator of the chosen type T.
 *
 * This vector is used for complex objects (such as UniString and Opdata)
 * assigned using values/const ref.
 *
 * T must have a default constructor (no arguments) and an assignment operator.
 *
 * @note It currently wraps around OpVector but could be extend to
 *       use a proper value based vector at some point. For instance
 *       fixing OpValueVector to not use memmove operations.
 *
 * @note This should only be used internally in the protobuf module.
*/
template<typename T>
class OpProtobufValueVector
{
public:
	/**
	 * Creates a vector, with a specified allocation step size.
	 */
	OpProtobufValueVector(UINT32 stepsize = 10);
	~OpProtobufValueVector();

	/**
	 * Clear vector by removing (and free'ing) all items in the vector.
	 */
	void Clear();

	/**
	 * Adds @a item to the end of the vector.
	 *
	 * @return OpStatus::OK if the item was added,
	 *         OpStatus::ERR_NO_MEMORY if it failed to allocate space for new items.
	 */
	OP_STATUS Add(const T &item);

	/**
	 * Replace the item at @a index with new @a item.
	 *
	 * @return OpStatus::OK if the item was replaced.
	 *         OpStatus::ERR otherwise.
	 */
	OP_STATUS Replace(UINT32 index, const T &item);

	/**
	 * Insert and add @a item at given @a index. The index must be in the range
	 * the current vector, or added to the end.
	 *
	 * @return OpStatus::OK if the item was added,
	 *         OpStatus::ERR_NO_MEMORY if it failed to allocate space for new items.
	 */
	OP_STATUS Insert(UINT32 index, const T &item);

	/**
	 * Removes @a item if found in vector.
	 *
	 * @note Requires comparison operator on T.
	 *
	 * @return OpStatus::OK.
	 */
	OP_STATUS RemoveByItem(const T &item);

	/**
	 * Removes items from the vector starting at @a index. It removes n number of
	 * items equal to or lower than @a count.
	 *
	 * @return OpStatus::OK.
	 */
	OP_STATUS Remove(UINT32 index, UINT32 count = 1);

	/**
	 * Searches for item matching @a item.
	 * @return The index of the matched item, or @c -1 if items was not found.
	 *
	 * @note Requires comparison operator on T.
	 */
	INT32 Find(const T &item) const;

	/**
	 * @return Item at @a index.
	 * @note Make sure @a index is within range of the vector.
	 */
	const T &Get(UINT32 index) const;

	/**
	 * @return The number of items in the vector.
	 */
	UINT32 GetCount() const;

private:
	OpVector<T> vector;
};

template<typename T>
OpProtobufValueVector<T>::OpProtobufValueVector(UINT32 stepsize)
	: vector(stepsize)
{
}

template<typename T>
OpProtobufValueVector<T>::~OpProtobufValueVector()
{
	Clear();
}

template<typename T>
void OpProtobufValueVector<T>::Clear()
{
	UINT32 count = GetCount();
	for (UINT32 i = 0; i < count; ++i)
		OP_DELETE(vector.Get(i));
	vector.Clear();
}

template<typename T>
OP_STATUS OpProtobufValueVector<T>::Add(const T &item)
{
	OpAutoPtr<T> tmp(OP_NEW(T, (item)));
	RETURN_OOM_IF_NULL(tmp.get());

	RETURN_IF_ERROR(vector.Add(tmp.get()));

	tmp.release();
	return OpStatus::OK;
}

template<typename T>
OP_STATUS OpProtobufValueVector<T>::Replace(UINT32 idx, const T &item)
{
	OP_ASSERT(idx < GetCount());

	T *vector_item = vector.Get(idx);
	OP_ASSERT(vector_item);

	*vector_item = item;
	return OpStatus::OK;
}

template<typename T>
OP_STATUS OpProtobufValueVector<T>::Insert(UINT32 idx, const T &item)
{
	OpAutoPtr<T> tmp(OP_NEW(T, (item)));
	RETURN_OOM_IF_NULL(tmp.get());

	RETURN_IF_ERROR(vector.Insert(idx, tmp.get()));

	tmp.release();
	return OpStatus::OK;
}

template<typename T>
OP_STATUS OpProtobufValueVector<T>::RemoveByItem(const T &item)
{
	INT32 idx = Find(item);
	if (idx == -1)
		return OpStatus::ERR;
	T *tmp = vector.Remove(idx);
	if (!tmp)
		return OpStatus::ERR;
	OP_DELETE(tmp);
	return OpStatus::OK;
}

template<typename T>
OP_STATUS OpProtobufValueVector<T>::Remove(UINT32 idx, UINT32 count)
{
	UINT32 end = idx + count;
	for (UINT32 i = idx; i < end; ++i)
		OP_DELETE(vector.Get(i));
	vector.Remove(idx, count);
	return OpStatus::OK;
}

template<typename T>
INT32 OpProtobufValueVector<T>::Find(const T &item) const
{
	UINT32 count = GetCount();
	for (UINT32 idx = 0; idx < count; ++idx)
	{
		T *vector_item = vector.Get(idx);
		if (*vector_item == item)
			return idx;
	}
	return -1;
}

template<typename T>
const T &OpProtobufValueVector<T>::Get(UINT32 idx) const
{
	return *vector.Get(idx);
}

template<typename T>
UINT32 OpProtobufValueVector<T>::GetCount() const
{
	return vector.GetCount();
}

#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_VECTOR_H
