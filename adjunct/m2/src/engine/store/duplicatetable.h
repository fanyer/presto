/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr)
 */

#ifndef STORE_DUPLICATE_TABLE_H
#define STORE_DUPLICATE_TABLE_H

#include "modules/util/OpHashTable.h"
#include "adjunct/desktop_util/mempool/mempool.h"
#include "adjunct/m2/src/include/defs.h"

struct DuplicateMessage
{
	DuplicateMessage(message_gid_t id) : m2_id(id), next(NULL) {}

	message_gid_t		m2_id;
	DuplicateMessage*	next;
};

class DuplicateTable
{
public:
							/** Constructor
							  */
							DuplicateTable() : m_mempool() {}

							/** After creation of store, set the estimated amount of messages to preallocate a reasonable sized buffer for all duplicates
							  * @param message_count - total amount of messages in store
							  */
	void					SetTotalMessageCount(UINT32 message_count) { m_mempool.SetInitialUnits(message_count/2) ; }

							/** Gets the list of duplicates (which should not be deleted or changed)
							  * @param master_id - duplicates of this master m2_id
							  * @return DuplicateMessage* when found, NULL otherwise
							  */
	const DuplicateMessage* GetDuplicates(message_gid_t master_id);
							
							/** Adds a duplicate from the table
							  * @param master_id - the m2_id to use for the master
							  * @param duplicate_id - the m2_id to add
							  * @param OK or ERR_NO_MEMORY when OOM
							  */
	OP_STATUS				AddDuplicate(message_gid_t master_id, message_gid_t duplicate_id);

							/** Removes and deletes a duplicate from the table
							  * @param master_id - where to find the duplicate to remove
							  * @param id_to_remove - the m2_id to remove (it can be the master_id)
							  * @param new_master_id - out parameter: 0 if all duplicates are removed, master_id if nothing has changed or something else if it's updated
							  * @param OK or ERR_NO_MEMORY if OOM when moving to a new master_id
							  */
	OP_STATUS				RemoveDuplicate(message_gid_t master_id, message_gid_t id_to_remove, message_gid_t &new_master_id);
							
							/** Delete all duplicates
							  */
	void					Clear();

private:

	DuplicateMessage*		GetById(message_gid_t m2_id);

	OpINT32HashTable<DuplicateMessage>	m_duplicate_table;	///< table of all the duplicates
	MemPool<DuplicateMessage>			m_mempool;			///< Mempool for duplicates
};

#endif // STORE_DUPLICATE_TABLE_H
