/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef OPSORTEDVECTOR_H
#define OPSORTEDVECTOR_H

#include "modules/util/adt/opvector.h"

template<class T>
struct GenericLessThan
{
	BOOL operator()(const T* lhs, const T* rhs) const { return *lhs < *rhs; }
};

template<class T>
struct PointerLessThan
{
	BOOL operator()(const T* lhs, const T* rhs) const { return lhs < rhs; }
};

template<class T>
struct IntegerLessThan
{
	BOOL operator()(const T* lhs, const T* rhs) const { return reinterpret_cast<INTPTR>(lhs) < reinterpret_cast<INTPTR>(rhs); }
};


/** @brief A generic sorted vector
  * A vector that will only retrieve items in sorted order. The Find* functions
  * of this vector use a binary search.
  *
  * If necessary, a custom compare function can be specified by using a
  * function object for LT. The function object has to implement the function
  *     BOOL operator()(const T* lhs, const T* rhs) const
  * See GenericLessThan for an example.
  */
template<class T, class LT = GenericLessThan<T>, bool AllowDuplicates = true>
class OpSortedVector : private OpGenericVector
{
public:
	/** Find an item in the vector
	  * @param item Item to search for
	  * @return Position of the item in the vector, or -1 if not found
	  */
	int		  Find(const T* item) const
		{ return Find(item, FALSE); }

	/** Find the first item in the vector conforming to the search key
	  * @param item The item to find
	  * @return Position of the first conforming item in the vector, or -1 if not found
	  */
	int		  FindFirst(const T* item) const;

	/** Find the position where an item would be inserted into the vector
	  * @param item Item to find an insert position for
	  * @return Position where item would be inserted
	  */
	unsigned  FindInsertPosition(const T* item) const
		{ return Find(item, TRUE); }

	/** Find an item in the vector - like Find, but returns the actual item found
	  * @param item Item to search for
	  * @return The found item in the vector, or NULL if not found
	  */
	T*		  FindSimilar(const T* item) const;

	/** Insert an item into the vector
	  * @param item Item to insert
	  * @param got_index If non-null, this will be set to the index that was assigned to the item
	  */
	OP_STATUS Insert(T* item);

	/** Remove an item from the vector
	  * @param item Item to remove from the vector
	  * @return OpStatus::OK if item is no longer in this vector after operation
	  */
	void	  Remove(const T* item);

	/** Check whether the vector contains a certain item
	  * @param item Item to check for
	  * @return Whether the vector contains item
	  */
	BOOL	  Contains(const T* item) const
		{ return Find(item) != -1; }

	/** Get an item by its index
	  * @param index Index to fetch item for
	  * @return Item found at index
	  */
	T*		  GetByIndex(unsigned index) const
		{ return static_cast<T*>(OpGenericVector::Get(index)); }

	/** Remove one or more items by their indices
	  * @param index Index of first item to remove
	  * @param count How many items to remove
	  * @return The first removed item
	  */
	T*		  RemoveByIndex(unsigned index, unsigned count = 1)
		{ return static_cast<T*>(OpGenericVector::Remove(index, count)); }

	/** Replace an item in the vector by its index
	  * @param index Index to replace
	  * @param replace_with What to replace the item at index with
	  */
	OP_STATUS ReplaceByIndex(unsigned index, T* replace_with);

	/** @return The amount of items contained in this vector
	  */
	unsigned  GetCount() const
		{ return OpGenericVector::GetCount(); }

	/** Remove all items from the vector
	  */
	void	  Clear()
		{ return OpGenericVector::Clear(); }

	/** Remove and deallocate an item
	  * @param item Item to remove and deallocate
	  */
	void	  Delete(T* item)
		{ Remove(item); OP_DELETE(item); }

	/** Remove and deallocate all items in the vector
	  */
	void	  DeleteAll();

private:
	int Find(const T* item, BOOL insert_position) const;
	inline static BOOL Equal(const T* lhs, const T* rhs)
		{ LT lt; return !lt(lhs, rhs) && !lt(rhs, lhs); }
};


/** @brief A sorted vector that deallocates its contents when destroyed
  */
template<class T, class LT = GenericLessThan<T> >
class OpAutoSortedVector : public OpSortedVector<T, LT>
{
public:
	/** Destructor
	  */
	~OpAutoSortedVector()
		{ OpSortedVector<T, LT>::DeleteAll(); }
};


/** @brief Sorted vector for integers
  * For function documentation, see OpSortedVector
  *
  * Two types for integers are defined:
  *  OpINTSortedVector - for sorting a list of integers
  *  OpINTSet - likewise, but without duplicate elements
  */
template<bool AllowDuplicates> class OpGenericINTSortedVector;
typedef OpGenericINTSortedVector<true> OpINTSortedVector;
typedef OpGenericINTSortedVector<false> OpINTSet;

template<bool AllowDuplicates>
class OpGenericINTSortedVector : protected OpSortedVector<void, IntegerLessThan<void>, AllowDuplicates>
{
public:
	int		  Find(INTPTR item) const
		{ return Super::Find(reinterpret_cast<const void*>(item)); }

	int		  FindFirst(INTPTR item) const
		{ return Super::FindFirst(reinterpret_cast<const void*>(item)); }

	unsigned  FindInsertPosition(INTPTR item) const
		{ return Super::FindInsertPosition(reinterpret_cast<const void*>(item)); }

	OP_STATUS Insert(INTPTR item)
		{ return Super::Insert(reinterpret_cast<void*>(item)); }

	void	  Remove(INTPTR item)
		{ Super::Remove(reinterpret_cast<const void*>(item)); }

	BOOL	  Contains(INTPTR item) const
		{ return Super::Contains(reinterpret_cast<const void*>(item)); }

	INTPTR	  GetByIndex(unsigned index) const
		{ return reinterpret_cast<INTPTR>(Super::GetByIndex(index)); }

	INTPTR	  RemoveByIndex(unsigned index, unsigned count = 1)
		{ return reinterpret_cast<INTPTR>(Super::RemoveByIndex(index, count)); }

	OP_STATUS ReplaceByIndex(unsigned index, INTPTR replace_with)
		{ return Super::ReplaceByIndex(index, reinterpret_cast<void*>(replace_with)); }

	unsigned  GetCount() const
		{ return Super::GetCount(); }

	void	  Clear()
		{ return Super::Clear(); }

protected:
	typedef OpSortedVector<void, IntegerLessThan<void>, AllowDuplicates> Super;
};

//////// Implementations //////

template<class T, class LT, bool AllowDuplicates>
int OpSortedVector<T, LT, AllowDuplicates>::Find(const T* item, BOOL insert_position) const
{
	LT lt;

	// Check for out of range
	if (GetCount() == 0 || lt(GetByIndex(GetCount() - 1), item))
		return insert_position ? GetCount() : -1;

	// Binary search
	unsigned bottom = 0;
	unsigned top    = GetCount();

	while (bottom < top)
	{
		unsigned check = (bottom + top) / 2;

		if (lt(GetByIndex(check), item))
			bottom = check + 1;
		else
			top = check;
	}

	if (insert_position ||
		(bottom < GetCount() &&
		 !lt(item, GetByIndex(bottom)) &&
		 !lt(GetByIndex(bottom), item)))
	{
		return bottom;
	}
	else
	{
		return -1;
	}
}

template<class T, class LT, bool AllowDuplicates>
int OpSortedVector<T, LT, AllowDuplicates>::FindFirst(const T* item) const
{
	int pos = Find(item);
	while (pos > 0 && Equal(GetByIndex(pos - 1), item))
		pos--;
	return pos;
}

template<class T, class LT, bool AllowDuplicates>
T* OpSortedVector<T, LT, AllowDuplicates>::FindSimilar(const T* item) const
{
	int pos = Find(item);
	return pos >= 0 ? GetByIndex(pos) : NULL;
}

template<class T, class LT, bool AllowDuplicates>
OP_STATUS OpSortedVector<T, LT, AllowDuplicates>::Insert(T* item)
{
	int pos = Find(item, TRUE);

	// Check if this is a duplicate
	if (!AllowDuplicates && (unsigned)pos < GetCount() && Equal(GetByIndex(pos), item))
		return OpStatus::OK;

	return OpGenericVector::Insert(pos, item);
}

template<class T, class LT, bool AllowDuplicates>
void OpSortedVector<T, LT, AllowDuplicates>::Remove(const T* item)
{
	int pos = FindFirst(item);
	if (pos < 0)
		return;

	for (int i = pos; (unsigned)i < GetCount() && Equal(item, GetByIndex(i)); i++)
	{
		if (item == GetByIndex(i))
		{
			OpGenericVector::Remove(i, 1);
			i--;
		}
	}
}

template<class T, class LT, bool AllowDuplicates>
OP_STATUS OpSortedVector<T, LT, AllowDuplicates>::ReplaceByIndex(unsigned index, T* replace_with)
{
	LT lt;
	if ((index == 0 || lt(GetByIndex(index - 1), replace_with)) && (index >= GetCount() || lt(replace_with, GetByIndex(index + 1))))
		return Replace(index, replace_with);

	RemoveByIndex(index);
	return Insert(replace_with);
}

template<class T, class LT, bool AllowDuplicates>
void OpSortedVector<T, LT, AllowDuplicates>::DeleteAll()
{
	unsigned count = GetCount();

	for (unsigned i = 0; i < count; i++)
		OP_DELETE(GetByIndex(i));
	Clear();
}

#endif // OPSORTEDVECTOR_H
