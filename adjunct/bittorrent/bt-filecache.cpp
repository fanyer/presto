/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
#include "modules/util/gen_math.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"

#include "bt-util.h"
#include "bt-filecache.h"
#include "bt-globals.h"
#include "p2p-fileutils.h"

BTFileCache::BTFileCache()
	: m_read_hits(0)
	, m_read_misses(0)
	, m_block_size(0)
	, m_cachesize(0)
	, m_maxcachesize(DEFAULT_BT_CACHE_SIZE)
	, m_cache_disabled(FALSE)
	, m_listener(NULL)
	, m_refcount(0)
	, m_last_commit(op_time(NULL))
{
	m_check_timer.SetTimerListener(this);
	m_check_timer.Start(CHECK_INTERVAL_BT_FILECACHE);

	BT_RESOURCE_ADD("BTFileCache", this);
}

BTFileCache::~BTFileCache()
{
	WriteCache();

	BT_RESOURCE_REMOVE(this);
}

void BTFileCache::ClearCache()
{
	BTFileCacheEntry *entry, *next;

	entry = m_filecache.First();
	while(entry)
	{
		next = (BTFileCacheEntry *)entry->Suc();

		m_allocator.Delete(entry);

		entry = next;
	}
	m_filecache.Clear();
}

INT32 BTFileCache::DecRef()
{
	if(--m_refcount == 0)
	{
		g_P2PFiles->Remove(this);
		OP_DELETE(this);
		return 0;
	}
	return m_refcount;
}
/*
Will check if the given block is complete in the cache. Requires that the cache
has been initialized with the BT block size
*/
/*
BOOL BTFileCache::IsCompleteBlockCacheDataAvailable(UINT32 blocknumber)
{
	UINT32 size = 0;
	BTFileCacheEntry *entry;

	OP_ASSERT(m_block_size != 0);

	entry = m_filecache.First();
	while(entry)
	{
		if(entry->m_blocknumber)
		{
			size += entry->m_length;
		}
		if(size >= m_block_size)
		{
			break;
		}
		entry = (BTFileCacheEntry *)entry->Suc();
	}
	if(size == m_block_size)
	{
		DEBUGTRACE_DISKCACHE(UNI_L("IsCompleteBlockCacheDataAvailable is TRUE for block: %ld\n"), blocknumber);
	}
	return size == m_block_size;
}
*/
/*
This method assumes that cache data is requested on bittorrent block boundaries, which is the normal
use case for this code.  Any other request is incorrect anyway in the BT context.

FALSE is the return code if the cache is unable to fill the buffer completely in which case
the callee should read from disk and add the data to the buffer.

buf_len will contain the read number of bytes copied IF the method returns TRUE for success
*/

BOOL BTFileCache::GetCacheData(BYTE *buffer, UINT64& buf_len, UINT64 offset)
{
	UINT32 filled_count = 0;
	BTFileCacheEntry *fragment;

	if(!IsCompleteCacheDataAvailable(offset, buf_len))
	{
		return FALSE;
	}
	fragment = m_filecache.First();
	while(fragment)
	{
		// this assumes you will always request data on a fragment boundary. If you don't, something is wrong
		if(offset == fragment->m_offset)
		{
			UINT32 maxlen = min(fragment->m_length, (UINT32)buf_len-filled_count);

			op_memcpy(&buffer[filled_count], fragment->m_buffer, maxlen);

			filled_count += maxlen;;
			offset += maxlen;
			if(filled_count >= buf_len)
			{
				break;
			}
		}
		fragment = (BTFileCacheEntry *)fragment->Suc();
	}
	if(filled_count < buf_len)
	{
		return FALSE;
	}
	buf_len = filled_count;

	DEBUGTRACE_DISKCACHE(UNI_L("*** GetCacheData: offset: %ld, "), offset);
	DEBUGTRACE_DISKCACHE(UNI_L("length: %ld, "), buf_len);
	DEBUGTRACE_DISKCACHE(UNI_L("block: %ld\n"), (UINT32)(offset / m_block_size));

	return TRUE;
}

/*
This method assumes that cache data is requested on bittorrent block boundaries, which is the normal
use case for this code.  Any other request is incorrect anyway in the BT context.
*/
BOOL BTFileCache::IsCompleteCacheDataAvailable(UINT64 offset, UINT64 length)
{
	OP_ASSERT(length != 0);

	UINT64 total_data_available = 0;
	BTFileCacheEntry *fragment;

	fragment = m_filecache.First();
	while(fragment)
	{
		// this assumes you will always request data on a fragment boundary. If you don't, something is wrong
		if(offset == fragment->m_offset)
		{
			total_data_available += fragment->m_length;
			offset += fragment->m_length;
		}
		if(total_data_available >= length)
		{
			break;
		}
		fragment = (BTFileCacheEntry *)fragment->Suc();
	}
	// we record this as a hit or miss on the cache
	if(total_data_available >= length)
	{
		m_read_hits++;
	}
	else
	{
		m_read_misses++;
	}
	return total_data_available >= length;
}

BTFileCacheEntry* BTFileCache::FindMatchingEntry(UINT64 offset, UINT64 length)
{
	BTFileCacheEntry *fragment;

	fragment = m_filecache.First();
	while(fragment)
	{
		if(offset >= fragment->m_offset)		// above offset
		{
			if(offset <= fragment->m_offset + fragment->m_length - 1)	// but below threshold
			{
				return fragment;
			}
		}
		fragment = (BTFileCacheEntry *)fragment->Suc();
	}
	return NULL;
}

// invalidate and write a certain range of the cache to disk
OP_STATUS BTFileCache::InvalidateBlock(UINT32 block)
{
	BTFileCacheEntry *entry;

	entry= m_filecache.First();
	while(entry)
	{
		if(block == entry->m_blocknumber)
		{
			if(m_listener)
			{
				if(entry->m_commited)
				{
					DEBUGTRACE_DISKCACHE(UNI_L("*** NOT WRITING CACHE ENTRY (InvalidateBlock): offset: %ld, "), entry->m_offset);
					DEBUGTRACE_DISKCACHE(UNI_L("length: %ld, "), entry->m_length);
					DEBUGTRACE_DISKCACHE(UNI_L("block: %ld\n"), entry->m_blocknumber);
				}
				else
				{
					DEBUGTRACE_DISKCACHE(UNI_L("*** InvalidateBlock - WRITING CACHE: offset: %ld, "), entry->m_offset);
					DEBUGTRACE_DISKCACHE(UNI_L("length: %ld, "), entry->m_length);
					DEBUGTRACE_DISKCACHE(UNI_L("block: %ld\n"), entry->m_blocknumber);
				}
				if(entry->m_commited || OpStatus::IsSuccess(m_listener->WriteCacheData(entry->m_buffer, entry->m_offset, entry->m_length)))
				{
					entry->m_commited = TRUE;
				}
				else
				{
					m_listener->CacheDisabled(TRUE);
					m_cache_disabled = TRUE;
				}
			}
			else
			{
				m_cache_disabled = TRUE;
			}
		}
		entry= (BTFileCacheEntry *)entry->Suc();
	}
	return OpStatus::OK;
}

OP_STATUS BTFileCache::InvalidateRange(UINT64 offset, UINT64 length)
{
	BTFileCacheEntry* entry = FindMatchingEntry(offset, length);
	BTFileCacheEntry* new_entry = NULL;

	while(length > 0 && entry != NULL)
	{
		// max of what this buffer can provide
		UINT64 readlen = min(length, entry->m_length);

		if(m_listener)
		{
			if(entry->m_commited)
			{
				DEBUGTRACE_DISKCACHE(UNI_L("*** NOT WRITING CACHE ENTRY (InvalidateRange): offset: %ld, "), entry->m_offset);
				DEBUGTRACE_DISKCACHE(UNI_L("length: %ld, "), entry->m_length);
				DEBUGTRACE_DISKCACHE(UNI_L("block: %ld\n"), entry->m_blocknumber);
			}
			else
			{
				DEBUGTRACE_DISKCACHE(UNI_L("*** InvalidateRange - WRITING CACHE: offset: %ld, "), entry->m_offset);
				DEBUGTRACE_DISKCACHE(UNI_L("length: %ld, "), entry->m_length);
				DEBUGTRACE_DISKCACHE(UNI_L("block: %ld\n"), entry->m_blocknumber);
			}
			if(entry->m_commited || OpStatus::IsSuccess(m_listener->WriteCacheData(entry->m_buffer, entry->m_offset, entry->m_length)))
			{
				entry->m_commited = TRUE;
				length -= readlen;
				offset += readlen;
			}
			else
			{
				m_listener->CacheDisabled(TRUE);
				m_cache_disabled = TRUE;
				break;
			}
		}
		else
		{
			m_cache_disabled = TRUE;
			break;
		}
		new_entry = FindMatchingEntry(offset, length);
		if(new_entry != entry)
		{
			entry = new_entry;
		}
		else
		{
			break;
		}
	}
	return OpStatus::OK;
}

/*
Add the provided buffer to the cache.
*/
OP_STATUS BTFileCache::AddCacheEntry(const BYTE *buffer, UINT64 buf_len, UINT64 offset, UINT32 blocknumber, BOOL commited)
{
	if(m_cache_disabled)
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	if(!buffer || buf_len == 0)
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR_NULL_POINTER;
	}
	if(m_cachesize >= DEFAULT_BT_CACHE_SIZE)
	{
		OP_STATUS s = TrimCache();
		if(OpStatus::IsError(s))
		{
			return s;
		}
	}
	BTFileCacheEntry* defwrite = FindMatchingEntry(offset, buf_len);
	if(defwrite)
	{
		if(offset == defwrite->m_offset &&
			buf_len == defwrite->m_length)
		{
			DEBUGTRACE_DISKCACHE(UNI_L("*** Duplicate entry (not added): offset: %ld, "), defwrite->m_offset);
			DEBUGTRACE_DISKCACHE(UNI_L("length: %ld, "), defwrite->m_length);
			DEBUGTRACE_DISKCACHE(UNI_L("block: %ld\n"), defwrite->m_offset / m_block_size);

			return OpStatus::OK;
		}
	}
	defwrite = m_allocator.New(buf_len);
	if(defwrite == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	defwrite->m_offset	= offset;
	defwrite->m_length	= (UINT32)buf_len;
	defwrite->m_blocknumber = blocknumber;
	defwrite->m_commited = commited;

	if(defwrite->m_buffer == NULL)
	{
		m_allocator.Delete(defwrite);
		return OpStatus::ERR_NO_MEMORY;
	}

	memcpy(defwrite->m_buffer, buffer, (UINT32)buf_len);

	m_cachesize += buf_len;

	// add the item in sorted order by offset
	BTFileCacheEntry *entry;

	entry = m_filecache.First();
	while(entry)
	{
		if(offset < entry->m_offset)
		{
			defwrite->Precede(entry);

			DEBUGTRACE_DISKCACHE(UNI_L("*** AddCacheEntry: offset: %ld, "), offset);
			DEBUGTRACE_DISKCACHE(UNI_L("length: %ld, "), buf_len);
			DEBUGTRACE_DISKCACHE(UNI_L("block: %ld\n"), blocknumber);

			return OpStatus::OK;
		}
		entry = (BTFileCacheEntry *)entry->Suc();
	}
	// first entry in the cache
	defwrite->Into((Head *)&m_filecache);

	return OpStatus::OK;
}

/*
This method will find the oldest block in the cache, or the block containing the oldest pieces
*/

BTFileCacheEntry* BTFileCache::FindOldestBlockCacheEntry()
{
	BTFileCacheEntry* oldest_block = 0;
	time_t oldest_time = op_time(NULL);

	BTFileCacheEntry *entry;

	entry = m_filecache.First();
	while(entry)
	{
		if(entry && entry->m_blocknumber)
		{
			if(entry->m_creationTime < oldest_time)
			{
				oldest_time = entry->m_creationTime;
				oldest_block = entry;
			}
		}
		entry = (BTFileCacheEntry *)entry->Suc();
	}
	return oldest_block;
}

void BTFileCache::OnTimeOut(OpTimer* timer)
{

	if(m_cachesize >= DEFAULT_BT_CACHE_SIZE)
	{
		// we need to trim down the cache
		DEBUGTRACE_DISKCACHE(UNI_L("*** TRIMMING CACHE: size: %lld, "), (UINT64)m_cachesize);
		DEBUGTRACE_DISKCACHE(UNI_L("hits: %ld, "), m_read_hits);
		DEBUGTRACE_DISKCACHE(UNI_L("misses: %ld\n"), m_read_misses);

		OpStatus::Ignore(TrimCache());

	}
	// commit data every 2 minutes
	time_t now = op_time(NULL);

	if((now - 120) > m_last_commit)
	{
		BTFileCacheEntry *entry;

		entry = m_filecache.First();
		while(entry)
		{
			if(!entry->m_commited)
			{
				// request that the listener writes out the data to disk
				if(m_listener)
				{
					DEBUGTRACE_DISKCACHE(UNI_L("*** WRITING CACHE ENTRY (OnTimeOut): offset: %ld, "), entry->m_offset);
					DEBUGTRACE_DISKCACHE(UNI_L("length: %ld, "), entry->m_length);
					DEBUGTRACE_DISKCACHE(UNI_L("block: %ld\n"), entry->m_blocknumber);

					if(OpStatus::IsSuccess(m_listener->WriteCacheData(entry->m_buffer, entry->m_offset, entry->m_length)))
					{
						entry->m_commited = TRUE;
					}
					else
					{
						m_listener->CacheDisabled(TRUE);
						m_cache_disabled = TRUE;
					}
				}
				else
				{
					m_cache_disabled = TRUE;
				}
			}
			entry = (BTFileCacheEntry *)entry->Suc();
		}
		m_last_commit = now;
	}
	DEBUGTRACE_DISKCACHE(UNI_L("*** DISKCACHE: size: %lld, "), (UINT64)m_cachesize);
	DEBUGTRACE_DISKCACHE(UNI_L("hits: %ld, "), m_read_hits);
	DEBUGTRACE_DISKCACHE(UNI_L("misses: %ld\n"), m_read_misses);

	m_check_timer.Start(CHECK_INTERVAL_BT_FILECACHE);
}

OP_STATUS BTFileCache::WriteCache()
{
	OP_STATUS s = OpStatus::OK;
	BTFileCacheEntry *entry;

	entry = m_filecache.First();
	while(entry)
	{
		if(!entry->m_commited)
		{
			// request that the listener writes out the data to disk
			if(m_listener)
			{
				DEBUGTRACE_DISKCACHE(UNI_L("*** WRITING CACHE ENTRY (WriteCache): offset: %ld, "), entry->m_offset);
				DEBUGTRACE_DISKCACHE(UNI_L("length: %ld, "), entry->m_length);
				DEBUGTRACE_DISKCACHE(UNI_L("block: %ld, "), entry->m_blocknumber);
				DEBUGTRACE_DISKCACHE(UNI_L("cache size: %ld: "), m_cachesize);

				s = m_listener->WriteCacheData(entry->m_buffer, entry->m_offset, entry->m_length);
				if(OpStatus::IsSuccess(s))
				{
					entry->m_commited = TRUE;
					m_cachesize -= entry->m_length;
					DEBUGTRACE_DISKCACHE(UNI_L("%s\n"), "SUCCESS");
				}
				else
				{
					DEBUGTRACE_DISKCACHE(UNI_L("%s\n"), "FAILURE");
				}
			}
		}
		entry = (BTFileCacheEntry *)entry->Suc();
	}
	ClearCache();
	m_cachesize = 0;
	return s;
}

OP_STATUS BTFileCache::TrimCache()
{
	do
	{
		// first, write out complete blocks until we are below the limit again
		BTFileCacheEntry* entry = FindOldestBlockCacheEntry();

		if(!entry)
			break;

		// request that the listener writes out the data to disk
		if(m_listener)
		{
			if(!entry->m_commited)
			{
				DEBUGTRACE_DISKCACHE(UNI_L("*** WRITING CACHE ENTRY (TrimCache): offset: %ld, "), entry->m_offset);
				DEBUGTRACE_DISKCACHE(UNI_L("length: %ld, "), entry->m_length);
				DEBUGTRACE_DISKCACHE(UNI_L("block: %ld\n"), entry->m_blocknumber);
			}
			if(entry->m_commited || OpStatus::IsSuccess(m_listener->WriteCacheData(entry->m_buffer, entry->m_offset, entry->m_length)))
			{
				m_cachesize -= entry->m_length;
				m_filecache.Remove(entry);
				m_allocator.Delete(entry);
			}
			else
			{
				m_listener->CacheDisabled(TRUE);
				m_cache_disabled = TRUE;
				return OpStatus::ERR;
			}
		}
		else
		{
			m_cache_disabled = TRUE;
			return OpStatus::ERR;
		}
	}
	while (m_cachesize >= (DEFAULT_BT_CACHE_SIZE - DEFAULT_BT_CACHE_HEADROOM));
	return OpStatus::OK;
}

#endif // _BITTORRENT_SUPPORT_
#endif //M2_SUPPORT
