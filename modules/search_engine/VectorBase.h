/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef VECTORBASE_H
#define VECTORBASE_H

#include "modules/search_engine/TypeDescriptor.h"

/**
 * @brief General-pupose container for any types or pointers to types,
 *   capable to replace OpVector, OpAutoVector, OpGenericVector and OpINT32Vector while providing more functionality.
 * @author Pavel Studeny <pavels@opera.com>
 */
class VectorBase : public NonCopyable
{
public:
	/**
	 *  @param allocator description of basic oparations on the type; use predefined PtrDescriptor<T> for pointers
	 */
	VectorBase(const TypeDescriptor &allocator);

	/**
	 * destruct all data left in the Vector
	 */
	~VectorBase(void) {Clear();}

	/**
	 * direct access to the items; can be useful together with SetCount(...) to fill the Vector with block of data from the disk
	 */
	void *Ptr(void) const {return m_items;}

	/**
	 * @param vec Vector to copy from
	 */
	CHECK_RESULT(OP_STATUS DuplicateOf(const VectorBase& vec));

	/**
	 * move all data into itself, comparison and destruction functions aren't changed
	 * @param vec will be empty after the operation
	 */
	void TakeOver(VectorBase &vec);

	/**
	 * destruct all data and delete them from the Vector
	 */
	void Clear(void);

	/**
	 * @param size number of items to reserve space for in advance
	 */
	CHECK_RESULT(OP_STATUS Reserve(UINT32 size));

	/**
	 * change the number of items; doesn't initialize nor destruct any new items/left overs
	 * @param count new number of items the Vector will contain, can be both lower and higher than the current count
	 */
	CHECK_RESULT(OP_STATUS SetCount(UINT32 count));

	/**
	 * destruct the item on the given place and set a new one instead
	 */
	CHECK_RESULT(OP_STATUS Replace(UINT32 idx, const void *item));

	/**
	 * insert an item to the given position, already present items from this position on will be moved up
	 */
	CHECK_RESULT(OP_STATUS Insert(UINT32 idx, const void *item));

	/**
	 * insert an item into a sorted data on its position
	 */
	CHECK_RESULT(OP_STATUS Insert(const void *item));

	/**
	 * insert an item to the end of the vector
	 */
	CHECK_RESULT(OP_STATUS Add(const void *item)) {return Insert(m_count, item);}

	/**
	 * remove item from the vector if it exists; item isn't destructed.
	 * It is assumed that you have taken over the contents of item by
	 * copying the bits, not by using the TypeDescriptor assignment function.
	 */
	BOOL RemoveByItem(const void *item);

	/**
	 * remove item at the given position. Does not call destructor.
	 * The removed data will be placed in item.
	 * If item==NULL, it is assumed that you have already taken over the
	 * contents at the given index by copying the bits, not by using the
	 * TypeDescriptor assignment function.
	 */
	void Remove(void *item, INT32 idx);

	/**
	 * remove item from the vector if it exists; item (in the vector) is destructed
	 */
	BOOL DeleteByItem(void *item);

	/**
	 * remove a range of items from the vector; items are destructed
	 */
	void Delete(INT32 idx, UINT32 count = 1);

	/**
	 * find an item in an unsorted Vector. Returns -1 if not found.
	 */
	INT32 Find(const void *item) const;

	/**
	 * @return item at the given position
	 */
	const void *Get(UINT32 idx) const {	return m_items + idx * m_allocator.size; }

	/**
	 * @return number of items in the Vector
	 */
	UINT32 GetCount(void) const { return m_count; }

	/**
	 * @return maximum number of items the Vector can hold with the currently allocated memory
	 */
	UINT32 GetSize(void) const { return m_size; }

	/**
	 * sort the items in the Vector into an ascending order and remove duplicates
	 */
	CHECK_RESULT(OP_STATUS Sort(void));

	/**
	 * find an item in a sorted vector
	 * @return position of the item or a position where the item would be if it was in the Vector
	 */
	INT32 Search(const void *item) const {return Search(item, 0, m_count);}

	/**
	 * find an item in a sorted vector in the given range
	 */
	INT32 Search(const void *item, UINT32 start, UINT32 end) const;

	/**
	 * @return a reference to the item at given position
	 */
	void *operator[](UINT32 idx) const { return m_items + idx * m_allocator.size; }

	/**
	 * Add the elements to a sorted vector which aren't present already. Uses the assignment function in type descriptor.
	 */
	CHECK_RESULT(OP_STATUS Unite(const VectorBase &vec));
	CHECK_RESULT(static OP_STATUS Unite(VectorBase &result, const VectorBase &vec1, const VectorBase &vec2));

	/**
	 * Remove all the elements from a sorted vector present in vec. Removed items are destructed.
	 */
	void Differ(const VectorBase &vec);
	CHECK_RESULT(static OP_STATUS Differ(VectorBase &result, const VectorBase &vec1, const VectorBase &vec2));

	/**
	 * Keep just the elements present in both. Others are destructed.
	 */
	CHECK_RESULT(OP_STATUS Intersect(const VectorBase &vec));
	CHECK_RESULT(static OP_STATUS Intersect(VectorBase &result, const VectorBase &vec1, const VectorBase &vec2));

	/**
	 * Keep just the elements for which "matches" return TRUE. Others are destructed.
	 */
	void Filter(BOOL (*matches)(const void* item, void* custom_data), void* custom_data);

	/**
	 * @return an estimate of the memory used by this data structure
	 */
#ifdef ESTIMATE_MEMORY_USED_AVAILABLE	
	size_t EstimateMemoryUsed() const;
#endif

protected:
	CHECK_RESULT(OP_STATUS Grow(UINT32 new_size, UINT32 hole));

	CHECK_RESULT(OP_STATUS Shrink(UINT32 new_size, UINT32 hole, UINT32 count = 1));

	INT32 MergeSort(char* array, char* temp_array, INT32 left, INT32 right);

	char *m_items;
	TypeDescriptor m_allocator;
	UINT32 m_size;
	UINT32 m_count;
};

#endif  // VECTORBASE_H


