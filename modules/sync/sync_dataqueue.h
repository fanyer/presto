/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _SYNC_DATAQUEUE_H_INCLUDED_
#define _SYNC_DATAQUEUE_H_INCLUDED_

#include "modules/util/simset.h"
#include "modules/sync/sync_datacollection.h"
#include "modules/sync/sync_state.h"

class OpSyncDataItem;
class OpSyncFactory;
class OpSyncParser;
class OpInternalSyncListener;

class OpSyncDataQueue
#ifdef SYNC_QUEUE_ON_DISK
	: public MessageObject
#endif // SYNC_QUEUE_ON_DISK
{
	friend class OpSyncFactory;

private:
	/**
	 * Constructor is private, only certain classes are allowed to
	 * create an item!
	 *
	 * @param factory The class creation factory to use.
	 */
	OpSyncDataQueue(OpSyncFactory* factory, OpInternalSyncListener* listener);

public:

	enum SyncQueueType
	{
		SYNCQUEUE_ACTIVE,			///< The currently active queue.
		/**
		 * Items that will be compared with the items received from the server,
		 * and differences will be sent off to the server.
		 * OpSyncDataQueue::Add() adds an item to this queue if the argument
		 * dirty_item is true.
		 */
		SYNCQUEUE_DIRTY_ITEMS,
		SYNCQUEUE_OUTGOING,			///< The outgoing queue.
		SYNCQUEUE_RECEIVED_ITEMS	///< Items just received.
	};

	virtual ~OpSyncDataQueue();

	/**
	 * Init the data queue class.
	 *
	 * @param use_disk_queue TRUE if the disk queue is to be used.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS Init(BOOL use_disk_queue);

	void SetUseDiskQueue(BOOL value) { m_use_disk_queue = value; }
	BOOL UseDiskQueue() const { return m_use_disk_queue; }

	/**
	 * Add a single item to the SYNCQUEUE_ACTIVE or SYNCQUEUE_DIRTY_ITEMS queue.
	 * If dirty_item is TRUE, the item will be added to SYNCQUEUE_DIRTY_ITEMS.
	 *
	 * @param item_to_queue The item to add to the queue. The item is assumed to
	 *  be new and need synchronisation.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS Add(OpSyncDataItem* item_to_queue, BOOL dirty_item = FALSE, BOOL keep_ordered = FALSE);

	/**
	 * Add multiple items to the queue.
	 *
	 * @param items_to_queue The items to add to the queue. The items are
	 * assumed to be new and need synchronisation.
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS Add(OpSyncDataCollection& items_to_queue);

	/**
	 * Clears the SYNCQUEUE_OUTGOING queue and updates the persistent data
	 * files, that contain the SYNCQUEUE_ACTIVE and SYNCQUEUE_OUTGOING items (so
	 * the file for the SYNCQUEUE_OUTGOING will be empty).
	 *
	 * This method should usually be called after the client successfully sent
	 * its SYNCQUEUE_OUTGOING items to the server.
	 */
	OP_STATUS ClearItemsToSend();
	OP_STATUS ClearReceivedItems();
	void RemoveQueuedItems(OpSyncDataItem::DataItemType type);

	/**
	 * Returns true if more items are queued to be sent to the server. Items
	 * might be added to a separate queue if the clients adds items to be
	 * synchronized while a synchronization is already in progress.
	 *
	 * @note The items to be sent to the server are in the queue
	 *  SYNCQUEUE_ACTIVE and SYNCQUEUE_OUTGOING (unless exclude_outgoing is set).
	 *
	 * @param exclude_outgoing If TRUE, do not include items ready to be sent to the server
	 * @return TRUE if items are queued to be sent
	 *
	 * @see ClearItemsToSend()
	 * @see Add()
	 */
	BOOL HasQueuedItems(BOOL exclude_outgoing = FALSE) const;

	/**
	 * Returns true if more items of the specified supports type are queued to
	 * be sent to the server. Items might be added to a separate queue if the
	 * clients adds items to be synchronised while a synchronisation is already
	 * in progress. SYNCQUEUE_OUTGOING is not checked in this call.
	 *
	 * @note The items to be sent to the server are in the queue
	 *  SYNCQUEUE_ACTIVE.
	 *
	 * @return true if items are queued to be sent
	 *
	 * @see Add()
	 */
	bool HasQueuedItems(SyncSupportsState supports) const;

	/**
	 * Returns true if the queue SYNCQUEUE_DIRTY_ITEMS is not empty.
	 */
	BOOL HasDirtyItems() const { return m_dirty_items.HasItems(); }

	/**
	 * This method adds an item to the active sync collection and ensures that
	 * it's added with ordering that is correct.  If this item has a parent, it
	 * ensures it gets added after the parent.
	 * Likewise, if this item has a previous, it will ensure that the item is
	 * added after the previous referenced item.
	 *
	 * This method is important to fix issues like:
	 *
	 * - Add a bookmark
	 * - Add a folder
	 * - Move added bookmark into folder
	 *
	 * In the outgoing queue, the bookmark shall not be sent before the folder
	 * to the server, but have the folder as parent. Otherwise this triggered a
	 * dirty sync from the server and drop the bookmark to root.
	 *
	 * This method ensures that the bookmark is moved after the folder in the
	 * queue so the ordering is maintained.
	 *
	 * @param item New or existing item with updated data. The content of this
	 *  item may have been moved into an existing item, so this item may be
	 *  empty on return.
	 * @param merged Is set to TRUE if the item was merged with an existing item
	 *  before being ordered.
	 * @return OpStatus::OK on success
	 */
	OP_STATUS AddItemOrdered(OpSyncDataItem* item, BOOL& merged);

	/**
	 * Returns the OpSyncCollection of the specified queue type.
	 *
	 * @param type The SyncQueueType type of the collection to return
	 */
	OpSyncCollection* GetSyncCollection(SyncQueueType type);
	OpSyncDataCollection* GetSyncDataCollection(SyncQueueType type);

	void PopulateOutgoingItems(SyncSupportsState supports);
	OP_STATUS AddAsReceivedItems(OpSyncCollection& items);

	void SetMaxItems(UINT32 max_items) { m_max_items_to_send = max_items; }
	UINT32 GetMaxItems() const { return m_max_items_to_send; }

	/**
	 * Marks the queue as dirty, i.e. some item was added to the queue and the
	 * queue needs to be written to disk.
	 *
	 * This method also starts a timer to actually write the queue file to
	 * disk.
	 *
	 * @note This "dirty" is an attribute of the queue and a dirty queue has not
	 *  yet been written to disk. This is different to HasDirtyItems() which
	 *  returns whether the SYNCQUEUE_DIRTY_ITEMS has some elements.
	 *
	 * @see StartTimeout(), WriteQueue()
	 */
	OP_STATUS SetDirty();

	OpSyncDataItem* FindItem(const uni_char* id);
	void Shutdown();

private:
	void ClearCollection(OpSyncDataCollection& collection);
	void ClearCollection(OpSyncCollection& collection);

#ifdef SYNC_QUEUE_ON_DISK
	OP_STATUS StartTimeout();
	void StopTimeout();

	/**
	 * @name Implementation of MessageObject
	 * @{
	 */

	/**
	 * Handles the message MSG_SYNC_WRITE_DELAY by writing the queues to disk
	 * (see WriteQueue()).
	 *
	 * @param msg is expected to be MSG_SYNC_WRITE_DELAY
	 * @param par1, par2 are ignored.
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	/** @} */ // Implementation of MessageObject

	OP_STATUS ReadQueue(const OpStringC& filename);

	/**
	 * Writes the current active queue SYNCQUEUE_ACTIVE into the file
	 * [OPFILE_USERPREFS_FOLDER]/link_queue_myopera.dat and the current outgoing
	 * queue SYNCQUEUE_OUTGOING into the file
	 * [OPFILE_USERPREFS_FOLDER]/link_queue_out_myopera.dat.
	 */
	OP_STATUS WriteQueue();

	/**
	 * Writes the data-queue of the specified queue-type into the file with the
	 * specified name.
	 * @param filename is the name if the file (relative to the folder
	 *  OPFILE_USERPREFS_FOLDER).
	 */
	OP_STATUS WriteQueue(const OpStringC& filename, SyncQueueType dest_queue_type);
#endif // SYNC_QUEUE_ON_DISK

	OpSyncFactory* m_factory;
	OpSyncParser* m_parser;
	OpInternalSyncListener* m_listener;

	/**
	 * Contains the number of items that are being sent at any one time, but not
	 * more than the maximum. */
	OpSyncDataCollection m_items_to_send;

	/** Contains all items that do not fit in the main queue. */
	OpSyncDataCollection m_item_queue;

	/**
	 * Contains all items that are compared to the server items as part of a
	 * dirty sync. */
	OpSyncDataHashedCollection m_dirty_items;

	/** Contains the items that has been received from the server. */
	OpSyncCollection m_received_items;

	/** Maximum number of items to queue for sending. */
	UINT32 m_max_items_to_send;

	/** If TRUE, the queue is dirty and needs to be written to disk. */
	BOOL m_dirty;

	/** Should be queue items to disk? */
	BOOL m_use_disk_queue;

	/** Should the timer be set for write? */
	BOOL m_write_timer_enabled;
};

#endif //_SYNC_DATAQUEUE_H_INCLUDED_
