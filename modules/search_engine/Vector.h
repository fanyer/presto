/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef VECTOR_H
#define VECTOR_H

#include "modules/search_engine/VectorBase.h"
#include "modules/search_engine/ResultBase.h"

/**
 * @brief General-pupose container for any types or pointers to types,
 *   capable to replace OpVector, OpAutoVector, OpGenericVector and OpINT32Vector while providing more functionality.
 * @author Pavel Studeny <pavels@opera.com>
 */
template <typename T> class TVector : public VectorBase
{
public:
	/**
	 * no destructor for T, comparison uses a default operator<
	 */
	TVector(void) : VectorBase(DefDescriptor<T>()) {}

	/**
	 * @param allocator description of basic oparations on the type; use predefined PtrDescriptor<T> for pointers
	 */
	TVector(const TypeDescriptor &allocator) : VectorBase(allocator) {}

	/**
	 * @param compare BOOL function saying which item is less
	 * @param destruct code to be performed as destruction of T either on a request or interanlly in various methods,
	 *   use Comparison::DefaultDelete if T is a normal pointer
	 */
	TVector(TypeDescriptor::ComparePtr compare, TypeDescriptor::DestructPtr destruct = NULL) :
		VectorBase(TypeDescriptor(sizeof(T), &DefDescriptor<T>::Assign,
			compare, destruct == NULL ? &DefDescriptor<T>::Destruct : destruct
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
			, &DefDescriptor<T>::EstimateMemoryUsed
#endif
			)) {}

	/**
	 * @param destruct code to be performed as destruction of T either on a request or interanlly in various methods,
	 *   use Comparison::DefaultDelete if T is a normal pointer
	 */
	TVector(TypeDescriptor::DestructPtr destruct) :
		VectorBase(TypeDescriptor(sizeof(T), &DefDescriptor<T>::Assign, &DefDescriptor<T>::Compare, destruct
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE
			, &DefDescriptor<T>::EstimateMemoryUsed
#endif
			)) {}

	/**
	 * direct access to the items; can be useful together with SetCount(...) to fill the Vector with block of data from the disk
	 */
	T *Ptr(void) const {return (T*)VectorBase::Ptr();}

	/**
	 * destruct the item on the given place and set a new one instead
	 */
	CHECK_RESULT(OP_STATUS Replace(UINT32 idx, const T &item)) {return VectorBase::Replace(idx, &item);}

	/**
	 * insert an item to the given position, already present items from this position on will be moved up
	 */
	CHECK_RESULT(OP_STATUS Insert(UINT32 idx, const T &item)) {return VectorBase::Insert(idx, &item);}

	/**
	 * insert an item into a sorted data on its position
	 */
	CHECK_RESULT(OP_STATUS Insert(const T &item)) {return VectorBase::Insert(&item);}

	/**
	 * insert an item to the end of the vector
	 */
	CHECK_RESULT(OP_STATUS Add(const T &item)) {return VectorBase::Add(&item);}

	/**
	 * remove item from the vector if it exists; item isn't destructed.
	 * It is assumed that you have taken over the contents of item by
	 * copying the bits, not by using the TypeDescriptor assignment function.
	 */
	BOOL RemoveByItem(const T &item) {return VectorBase::RemoveByItem(&item);}

	/**
	 * remove item at the given position
	 * @return the removed item; destruction is the responsibility of the caller
	 */
	T Remove(INT32 idx) {T t; VectorBase::Remove(&t, idx); return t;}

	/**
	 * remove item from the vector if it exists; item (in the vector) is destructed
	 */
	BOOL DeleteByItem(T &item) {return VectorBase::DeleteByItem(&item);}

	/**
	 * find an item in an unsorted Vector
	 * @return position of the item or -1 if not found
	 */
	INT32 Find(const T &item) const {return VectorBase::Find(&item);}

	/**
	 * @return item at the given position
	 */
	const T &Get(UINT32 idx) const {return *(T *)VectorBase::Get(idx);}

	/**
	 * find an item in a sorted vector
	 * @return position of the item or a position where the item would be if it was in the Vector
	 */
	INT32 Search(const T &item) const {return VectorBase::Search(&item);}

	/**
	 * find an item in a sorted vector in the given range
	 */
	INT32 Search(const T &item, UINT32 start, UINT32 end) const {return VectorBase::Search(&item, start, end);}

	/**
	 * Remove all items that do not match the given filter
	 */
	void Filter(SearchFilter<T>& filter) {VectorBase::Filter(filter_matcher, &filter);}

	/**
	 * @return a reference to the item at given position
	 */
	T &operator[](UINT32 idx) const {return *(T *)VectorBase::operator[](idx);}

private:
	static BOOL filter_matcher(const void* item, void* filter)
	{
		return reinterpret_cast<SearchFilter<T>*>(filter)->Matches(*reinterpret_cast<const T*>(item));
	}
};

#endif  // VECTOR_H


