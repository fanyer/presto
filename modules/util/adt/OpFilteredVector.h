/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * The idea for this class is arjanl's (arjanl@opera.com), faithfully executed
 * by psmaas (psmaas@opera.com) and later cleaned up by Eddy.
 */
#ifndef MODULES_UTIL_ADT_OP_FILTERED_VECTOR_H
#define MODULES_UTIL_ADT_OP_FILTERED_VECTOR_H
# ifdef UTIL_OP_FILTERED_VECTOR

#include "modules/util/adt/opvector.h"

template<class T>
class OpFilteredVectorItem;

/** Indexing mode.
 *
 * Where OpFilteredVector's methods take or return an index into the vector, it
 * is always given relative to one of these modes, each of which selects a
 * subset of the vector's members.  The index of an entry in the array, with
 * respect to a given indexing mode, is the number of earlier entries in the
 * array that are included in that indexing mode's subset.
 */
enum FilteredVectorIndex
{
	CompleteVector, //< All entries are included
	VisibleVector, //< Only the visible entries are counted; used by default
	InvisibleVector //< Only the invisible entries are counted
};

/** A vector with entries selectively visible.
 *
 * The OpFilteredVector class creates a layer of abstraction over the vector
 * containing the items to be able to efficiently look up and insert into the
 * vector.  This layer of abstraction is necessary to support a concept of
 * "hidden" or filtered items. The presence of these filtered items can be
 * transparent by the user having a virtual index into the vector.
 *
 * This is done by keeping an external sorted vector with the indexes of the
 * filtered items.
 *
 * This external vector makes it possible to calculate the real index of items
 * which makes lookups and inserts more efficient.
 *
 * @tparam T Type of entries in the vector: must have OpFilteredVectorItem<T> as
 * one of its public bases.
 */
template<class T>
class OpFilteredVector
{
public:

	/** Add item to (the end of) a vector.
	 *
	 * @param item to be added
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS Add(T* item) { return Insert(GetCount(CompleteVector), item, CompleteVector); }

	/** Insert item at specified index in vector.
	 *
	 * @param index Where item should be inserted.
	 * @param item What to insert.
	 * @param mode Indexing mode; see FilteredVectorIndex.
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS Insert(UINT32 index, T* item, FilteredVectorIndex mode = VisibleVector)
	{
		if (!item)
			return OpStatus::ERR;

		// Make sure the index is into the complete vector
		index = GetIndex(index, mode, CompleteVector);

		// Update invisible items
		UINT32 invisible_count = m_filtered_indices.GetCount();
		for (UINT32 i = m_filtered_indices.Search(index); i < invisible_count; i++)
		{
			INT32 value = m_filtered_indices.Get(i);
			if (value >= static_cast<INT32>(index))
				m_filtered_indices.Replace(i, value + 1);
		}

		if (item->IsFiltered())
		{
			// Add index to m_filtered_indices
			UINT32 idx = m_filtered_indices.Search(index);

			if (idx == m_filtered_indices.GetCount() ||  // Insert last
				static_cast<INT32>(index) != m_filtered_indices.Get(idx))
				// Not already there
				m_filtered_indices.Insert(idx, index);

			else
				OP_ASSERT(!"Either an item was readded or I failed to insert it");
		}

		return m_items.Insert(index, item);
	}

	/** Remove item at specified index from the vector.
	 *
	 * @param index Index of the item to be removed.
	 * @param mode Indexing mode; see FilteredVectorIndex.
	 * @return Pointer to the item removed.
	 */
	T* Remove(UINT32 index, FilteredVectorIndex mode = VisibleVector)
	{
		// Make sure the index is into the complete vector
		index = GetIndex(index, mode, CompleteVector);

		// Update invisible items
		for (UINT32 i = m_filtered_indices.GetCount();
			 m_filtered_indices.Get(--i) >= (int) index + 1; )
			m_filtered_indices.Replace(i, m_filtered_indices.Get(i) - 1);

		T* item = m_items.Remove(index);
		if (item && item->IsFiltered())
		{
			// Remove index from m_filtered_indices
			UINT32 idx = m_filtered_indices.Search(index);

			if (idx < m_filtered_indices.GetCount() && // in range
				(int) index == m_filtered_indices.Get(idx))  // present
				m_filtered_indices.Remove(idx);

			else
				OP_ASSERT(!"Item was not there");
		}

		return item;
	}

	/** Remove an item, specified by its value, from the vector.
	 *
	 * @note Has the same limitation as Find().  In particular, if there is a
	 *  copy of the given item, at an index suppressed in the given mode, before
	 *  a later copy at an index visible in the given mode, the former shall
	 *  prevent the latter from being noticed, hence from being removed.
	 *
	 * @param item The item to remove (identified using Find(item, mode)).
	 * @param mode Indexing mode; item is only removed if visible in this mode.
	 * @return OpStatus::OK if successful, OpStatus::ERR on failure.
	 */
	OP_STATUS RemoveByItem(T* item, FilteredVectorIndex mode = VisibleVector)
	{
		INT32 index = Find(item, mode);
		return index >= 0 && Remove(index, mode) ? OpStatus::OK : OpStatus::ERR;
	}

	/** Look up item at a specified index in the vector.
	 *
	 * @param index of the desired item
	 * @param mode Indexing mode; see FilteredVectorIndex.
	 * @return pointer to the item if present else NULL
	 */
	T* Get(UINT32 index, FilteredVectorIndex mode = VisibleVector) const
	{
		return m_items.Get(mode == CompleteVector ? index :
						   static_cast<UINT32>(GetIndex(index, mode, CompleteVector)));
	}

	/**
	 * @param mode Indexing mode; see FilteredVectorIndex.
	 * @return The number of items visible in this mode.
	 */
	INT32 GetCount(FilteredVectorIndex mode = VisibleVector)
	{
		switch (mode)
		{
		case CompleteVector:  return m_items.GetCount();
		case VisibleVector:   return m_items.GetCount() - m_filtered_indices.GetCount();
		case InvisibleVector: return m_filtered_indices.GetCount();
		}

		OP_ASSERT(!"Bad indexing mode");
		return 0;
	}

	/** Clear the vector.
	 *
	 * Deletes all the items (including the filtered items) and
	 * clears the filtered index array.
	 */
	void DeleteAll() { m_items.DeleteAll(); m_filtered_indices.Clear(); }

	/** Finds the index of a given item.
	 *
	 * Locates the first instance of the given item in the vector; if this index
	 * is valid for the given indexing mode, its index in that mode is returned;
	 * else -1 is returned.
	 *
	 * @note If the vector includes a later instance of the item at an index
	 *  visible in the desired mode, this will be hidden by any earlier instance
	 *  at an index suppressed by the mode.
	 *
	 * @param item The item to find.
	 * @param mode Indexing mode; see FilteredVectorIndex.
	 * @return The item's index in the given mode, or -1.
	 */
	INT32 Find(T* item, FilteredVectorIndex mode = VisibleVector) const
	{
		INT32 index = m_items.Find(item);

		switch (mode)
		{
		case CompleteVector: return index;

		case VisibleVector:
		{
			UINT32 idx = m_filtered_indices.Search(index);
			return (idx < m_filtered_indices.GetCount() &&
					index == m_filtered_indices.Get(idx))
				? -1 : GetIndex(index, CompleteVector, mode);
		}

		case InvisibleVector:
		{
			UINT32 idx = m_filtered_indices.Search(index);
			return (idx < m_filtered_indices.GetCount() &&
					index == m_filtered_indices.Get(idx))
				? GetIndex(index, CompleteVector, mode) : -1;
		}
		}

		OP_ASSERT(!"Bad indexing mode");
		return 0;
	}

	/** Duplicate another filtered vector.
	 *
	 * @param vec to be copied
	 * @return See OpStatus.
	 */
	OP_STATUS DuplicateOf(OpFilteredVector& vec)
	{
		RETURN_IF_ERROR(m_items.DuplicateOf(vec.GetItems()));
		return m_filtered_indices.DuplicateOf(vec.GetFilteredIndexes());
	}


	/** Translates an index between modes.
	 *
	 * Note that translating from visible to invisible (and vice versa) does not
	 * make sense and will cause an assertion.  Cases where neither in or out
	 * mode is CompleteVector do not make sense and will return index.
	 *
	 * @param index The index to be translated
	 * @param in_mode Indexing mode in which to interpret index.
	 * @param out_mode Indexing mode to use for returned value.
	 * @return The specified output index.
	 */
	INT32 GetIndex(INT32 index,
				   FilteredVectorIndex in_mode,
				   FilteredVectorIndex out_mode) const
	{
		if (in_mode == out_mode) // pointless request - but harmless
			return index;

		if (out_mode == CompleteVector)
			return GetCompleteIndex(index, in_mode);

		if (in_mode == CompleteVector)
			return GetFilteredIndex(index, out_mode);

		OP_ASSERT(!"Incoherent index conversion request");
		return index;
	}

	/** Control visibility of a given index's contents.
	 *
	 * @param index Index of the entry whose visibility is to be modified.
	 * @param visible TRUE to make the entry visible, FALSE to make it invisible.
	 */
	void SetVisibility(INT32 index, BOOL visible)
	{
		if (index < 0)
			return;

		T* item = m_items.Get(index);
		if (item)
		{
			item->m_filtered = !visible;
			if (visible)
				m_filtered_indices.RemoveByItem(index);
			else
			{
				// Add index to m_filtered_indices
				UINT32 idx = m_filtered_indices.Search(index);

				if (idx == m_filtered_indices.GetCount() ||  // Insert last
					index != m_filtered_indices.Get(idx))    // Not already there
					m_filtered_indices.Insert(idx, index);
				}
			}
		}

private:

	/**
	 * @param index into the filtered vector
	 * @return the actual index into the vector
	 */
	INT32 GetCompleteIndex(INT32 index, FilteredVectorIndex out_index = VisibleVector) const
	{
		if (index < 0)
			return -1;

		INT32 invisible_count = m_filtered_indices.GetCount();
		switch (out_index)
		{
			// Included for completeness, despite making little sense:
		case CompleteVector: return index;

		case VisibleVector:
		{
			INT32 pos = index;
			for (INT32 i = 0; i < invisible_count && m_filtered_indices.Get(i) <= pos; i++)
				pos++;

			return pos;
		}

		case InvisibleVector:
			if (index >= invisible_count)
				return m_items.GetCount();

			return m_filtered_indices.Get(index);
		}

		OP_ASSERT(!"Bad indexing mode");
		return -1;
	}

	/**
	 * @param index into the actual vector
	 * @return the index among the filter items
	 */
	INT32 GetFilteredIndex(INT32 index, FilteredVectorIndex mode = VisibleVector) const
	{
		switch (mode)
		{
			// Included for completeness, despite making little sense:
		case CompleteVector: return index;

		case VisibleVector:
		{
			UINT32 nearest_index = m_filtered_indices.Search(index);

			// Check if they are all smaller than index:
			if (nearest_index == m_filtered_indices.GetCount())
				return index - m_filtered_indices.GetCount();

			INT32 nearest_item_pos = m_filtered_indices.Get(nearest_index);

			if (nearest_item_pos < index)
				return index - (nearest_index + 1);
			else if (nearest_item_pos > index)
				return index - nearest_index;
			else
				return -1; // Item is invisible
		}

		case InvisibleVector:
		{
			UINT32 idx = m_filtered_indices.Search(index);

			if (idx < m_filtered_indices.GetCount() && // in range
				index == m_filtered_indices.Get(idx))  // present
				return idx;

			return -1; // Item is visible
		}
		}

		OP_ASSERT(!"Bad indexing mode");
		return 0;
	}


	OpVector<T>&   GetItems()           { return m_items; }
	OpINT32Vector& GetFilteredIndexes() { return m_filtered_indices; }

	OpVector<T>   m_items;
	OpINT32Vector m_filtered_indices;
};

template<class T>
class OpFilteredVectorItem
{
public:
	virtual ~OpFilteredVectorItem() {} // we expect to be sub-classed
	friend class OpFilteredVector<T>; // SetVisibility modifies m_filtered
	OpFilteredVectorItem(BOOL filtered) : m_filtered(filtered) {}
	BOOL IsFiltered() { return m_filtered; }

private:
	BOOL m_filtered;
};

# endif // UTIL_OP_FILTERED_VECTOR
#endif // MODULES_UTIL_ADT_OP_FILTERED_VECTOR_H
