/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef BT_Filecache_H
#define BT_Filecache_H

// ----------------------------------------------------

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"
# include "modules/util/simset.h"

#include "bt-util.h"
#include "bt-benode.h"
#include "bt-tracker.h"
#include "bt-globals.h"

#define CHECK_INTERVAL_BT_FILECACHE	 5000	// check if the cache needs to be trimmed down every 5 seconds

class BTFileCacheEntry : public Link
{
public:
	BTFileCacheEntry()
		: m_offset(0)
		, m_length(0)
		, m_blocknumber(0)
		, m_buffer(NULL)
		, m_creationTime(op_time(NULL))
		, m_commited(FALSE)
	{
		BT_RESOURCE_ADD("BTFileCacheEntry", this);
	}
	~BTFileCacheEntry()
	{
		BT_RESOURCE_REMOVE(this);
		OP_DELETEA(m_buffer);
	}
	UINT64	m_offset;		// virtual offset in torrent
	UINT32	m_length;		// length of the buffer
	UINT32	m_blocknumber;	// the block number in the torrent
	byte*	m_buffer;		// the data
	time_t	m_creationTime;	// time the cache entry was created
	BOOL	m_commited;	// has this buffer been written?
};

class OpBTFileCacheListener
{
public:
    virtual ~OpBTFileCacheListener() {} // gcc 4 gets unhappy without this virtual destructor

	// asks the listener to write cache data
	virtual OP_STATUS	WriteCacheData(byte *buffer, UINT64 offset, UINT32 buflen) = 0;

	// if a fatal error occurs, eg. during write, the listener will be notified that a fatal error
	// has occured and can act accordingly
	virtual void		CacheDisabled(BOOL disabled) = 0;
};

class BTFileCacheEntryCollection : private Head
{
public:
	BTFileCacheEntryCollection()
	{
	}

	BTFileCacheEntry*	Last() const { return (BTFileCacheEntry*)Head::Last(); }
	BTFileCacheEntry*	First() const { return (BTFileCacheEntry*)Head::First(); }
	UINT GetCount() { return Head::Cardinal(); }
	BOOL IsEmpty() { return Head::Empty(); }
	void Clear() { Head::Clear(); }
	void AddItem(BTFileCacheEntry* item) { item->Into(this); }
	void AddFirst(BTFileCacheEntry* item) { item->IntoStart(this); }
	void Remove(BTFileCacheEntry* item) 
	{ 
		OP_ASSERT(HasLink(item));

		if(HasLink(item))
		{
			item->Out();
		}
	}

private:
};

class BTFileCacheAllocator
{
public:
	BTFileCacheAllocator() :
		m_total_size(0)
	{
		BT_RESOURCE_ADD("BTFileCacheAllocator", this);
	}
	virtual ~BTFileCacheAllocator()
	{
		BT_RESOURCE_REMOVE(this);
	}
	BTFileCacheEntry* New(UINT64 size)
	{
		BTFileCacheEntry* newentry = OP_NEW(BTFileCacheEntry, ());
		if(newentry)
		{
			newentry->m_buffer = OP_NEWA(byte, (int)size);

			if(newentry->m_buffer == NULL)
			{
				OP_DELETE(newentry);
				return NULL;
			}
			m_total_size += newentry->m_length;
		}
		return newentry;
	}
	void Delete(BTFileCacheEntry* entry)
	{
		m_total_size -= entry->m_length;
		entry->Out();
		OP_DELETE(entry);
	}

private:
	UINT64 m_total_size;
};

class BTFileCache : public OpTimerListener
{
public:
	BTFileCache();
	virtual ~BTFileCache();

	// cache read/miss methods
	UINT32 GetCacheReadHits() { return m_read_hits; }
	UINT32 GetCacheReadMisses() { return m_read_misses; }
	double GetCacheReadHitRatio() { return m_read_misses / (m_read_hits + m_read_misses); }
	UINT32 GetTotalReadAccesses() { return m_read_hits + m_read_misses; }
//	BOOL IsCompleteBlockCacheDataAvailable(UINT32 blocknumber);

	// cache write methods
	OP_STATUS AddCacheEntry(const BYTE *buffer, UINT64 buf_len, UINT64 offset, UINT32 blocknumber, BOOL commited);	// create and add cache entry with provided data
	OP_STATUS WriteCache();
	OP_STATUS TrimCache();
	OP_STATUS InvalidateBlock(UINT32 block);
	OP_STATUS InvalidateRange(UINT64 offset, UINT64 length); // invalidate and write a certain range of the cache to disk

	// cache read methods

	// copy buffer data into argument "buffer", max len is "buf_len" starting at offset.
	// method will return FALSE if not enough data is available to fill the buffer with the requested data
	BOOL GetCacheData(BYTE *buffer, UINT64& buf_len, UINT64 offset);

	// OpTimerListener
	void OnTimeOut(OpTimer* timer);

	void SetListener(OpBTFileCacheListener *listener) { m_listener = listener; m_cache_disabled = FALSE; }
	void Init(UINT32 blocksize) { m_block_size = blocksize; }
	UINT32 GetBlocksize() { return m_block_size; }
	INT32 IncRef() { return ++m_refcount; }
	INT32 DecRef();
	void ClearCache();

	OpString m_hashkey;

private:

	BTFileCacheEntryCollection m_filecache;
	UINT32 m_read_hits;
	UINT32 m_read_misses;
	UINT32 m_block_size;	// BT block size
	UINT64 m_cachesize;		// current total cache size in bytes
	UINT64 m_maxcachesize;	// maximum cache size in bytes
	BOOL   m_cache_disabled;
	OpTimer m_check_timer;
	OpBTFileCacheListener *m_listener;
	INT32	m_refcount;
	time_t m_last_commit;
	BTFileCacheAllocator m_allocator;

private:
	BOOL IsCompleteCacheDataAvailable(UINT64 offset, UINT64 length);	// does the cache contain all the data to fill this cache request
	BTFileCacheEntry* FindOldestBlockCacheEntry();		// find the oldest block in the cache set
	BTFileCacheEntry* FindMatchingEntry(UINT64 offset, UINT64 length);

};

#endif // BT_Filecache_H
