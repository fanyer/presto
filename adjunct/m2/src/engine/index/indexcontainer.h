/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef INDEX_CONTAINER_H
#define INDEX_CONTAINER_H

#include "modules/util/adt/opvector.h"
#include "adjunct/m2/src/include/enums.h"
#include "adjunct/m2/src/engine/index.h"

/** @brief Owns Index* pointers and matches them to index ids
  */
class IndexContainer
{
public:
	IndexContainer() : m_count(0) {}

	/** Get an index
	  * @param id ID of index to get
	  * @return the index if it could be found, NULL otherwise
	  */
	Index* GetIndex(index_gid_t id) const { return id < IndexTypes::LAST ? m_index_types[GetType(id)].Get(GetPos(id)) : 0; }

	/** Add an index to this container
	  * @param id ID of index to add
	  * @param index Index to add
	  */
	OP_STATUS AddIndex(index_gid_t id, Index* index);

	/** Delete the index with the specified ID (will deallocate index)
	  * @param id ID of index to delete
	  */
	void DeleteIndex(index_gid_t id);

	/**  Get a pointer to an index in range [range_start, range_end)
	  *  Returns the index from the iterator (first index if iterator is -1) in the range, or NULL if
	  *  such an index can't be found
	  *
	  *  Ex. usage:
	  *   INT32 it = -1;
	  *   while (index = GetRange(FIRST_SEARCH, LAST_SEARCH, it))
	  *      // Do something with index
	  *
	  * @param range_start Start of the range to search
	  * @param range_end End of the range to search
	  * @param iterator a previously received iterator from this function, or -1 to start
	  * @return the index if it could be found, NULL otherwise
	  */
	Index* GetRange(UINT32 range_start, UINT32 range_end, INT32& iterator);

	/** @return the number of indexes in this container
	  */
	unsigned GetCount() const { return m_count; }

private:
	static size_t GetType(index_gid_t id) { return id / IndexTypes::ID_SIZE; }
	static size_t GetPos(index_gid_t id) { return id % IndexTypes::ID_SIZE; }

	/* m_index_types contains a bucket for each index type, where an index type
	 * is a range of size ID_SIZE, for example [FIRST_CONTACT .. LAST_CONTACT].
	 * This uses the fact that within such a range, the index ids that are taken
	 * are usually contiguous (except where indexes have been deleted) and that
	 * all ranges have a size of ID_SIZE. Lookup is now O(1) (position in the
	 * table can be derived from the ID) and memory usage grows with the highest
	 * ID taken by an index in each range. Sort of like a manual hash table,
	 * but still ordered in a way so that we can efficiently implement GetRange().
	 *
	 * To find an index that belongs to a certain id in m_index_types, you
	 * need to know the 'type' of the index (see GetType() above) which is the
	 * bucket in m_index_types that contains the index, and the position (see
	 * GetPos() above) which is the position of the index in the bucket.
	 *
	 * --------------------------------------------
	 * | bucket | contains indexes with IDs       |
	 * | 0      | [0 .. ID_SIZE)                  |
	 * | 1      | [FIRST_CONTACT .. LAST_CONTACT) |
	 * | 2      | [FIRST_FOLDER .. LAST_FOLDER)   |
	 * | ...    | ...                             |
	 * | Last   | [LAST - ID_SIZE .. LAST)        |
	 * --------------------------------------------
	 */
	OpAutoVector<Index> m_index_types[IndexTypes::LAST / IndexTypes::ID_SIZE];

	unsigned m_count; ///< Number of indexes stored in this container
};

#endif // INDEX_CONTAINER_H
