/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"


#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/util/opfile/opsafefile.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/datastream/opdatastreamsafefile.h"
#include "modules/stdlib/util/opdate.h"

#include "modules/url/url2.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_cs.h"
#include "modules/formats/url_dfr.h"

#include "modules/cache/url_stor.h"
#include "modules/cache/cache_common.h"
#include "modules/cache/cache_man.h"
#include "modules/cache/cache_utils.h"
#include "modules/cache/cache_propagate.h"

#ifdef CACHE_FAST_INDEX
	#include "modules/cache/simple_stream.h"
#endif

void Context_Manager::AutoSaveCacheL()
{
	if(urlManager->updated_cache)
	{
#ifdef DISK_CACHE_SUPPORT
		WriteCacheIndexesL(FALSE, FALSE);
#endif

		urlManager->updated_cache = FALSE;

#if defined(CACHE_MAX_VISITED_URL_COUNT) && CACHE_MAX_VISITED_URL_COUNT>0
		if(url_store->URL_RepCount() > CACHE_MAX_VISITED_URL_COUNT)
		{
			Head collect_list;
			Head sort_list[256];

			int i;
			time_t index;
			int num_selected=0;

			// Count the number of affected URLs, to avoid all the operations if it is not required
			URL_Rep* url_rep = url_store->GetFirstURL_Rep();

			while (url_rep)
			{
				if(url_rep->GetRefCount() == 1 && (!url_rep->GetDataStorage() || !url_rep->GetDataStorage()->GetCacheStorage()))
					num_selected++;

				url_rep = url_store->GetNextURL_Rep();
			}

			// Remove the URLs exceeding CACHE_MAX_VISITED_URL_COUNT, after sorting them
			if(num_selected > CACHE_MAX_VISITED_URL_COUNT)
			{
				url_rep = url_store->GetFirstURL_Rep();
				
				// First radix sort path (it sorts only based ont the first digit);
				while (url_rep)
				{
					if(url_rep->GetRefCount() == 1 && (!url_rep->GetDataStorage() || !url_rep->GetDataStorage()->GetCacheStorage()))
					{
						url_store->RemoveURL_Rep(url_rep);
						index = 0;
						url_rep->GetAttribute(URL::KVLocalTimeVisited, &index);
						url_rep->Into(&sort_list[ index & 0xff]);
					}
					url_rep = url_store->GetNextURL_Rep();
				}

				for(i=0;i< 256;i++)
					collect_list.Append(&sort_list[i]);
				
				/* At this stage, the URLs are sorted only by the first digit, but the list is complete */

				int shift=0;

				// Finishes the radix sort
				while(shift<24)
				{
					shift += 8;

					while((url_rep = (URL_Rep *) collect_list.First()) != NULL)
					{
						url_rep->Out();
						index = 0;
						url_rep->GetAttribute(URL::KVLocalTimeVisited, &index);
						url_rep->Into(&sort_list[ (index >> shift) & 0xff]);
					}

					for(i=0;i< 256;i++)
						collect_list.Append(&sort_list[i]);
				}

				// Move the first CACHE_MAX_VISITED_URL_COUNT URLs to url_store, to preserve them
				int i=0;

				while(i < CACHE_MAX_VISITED_URL_COUNT && !collect_list.Empty())
				{
					url_rep = (URL_Rep *) collect_list.Last();
					url_rep->Out();
					url_store->AddURL_Rep(url_rep);
					i++;
				}

				// Delete the remaining URLs, to avoid OOM on some devices ( CORE-36314 )
				while(!collect_list.Empty())
				{
					url_rep = (URL_Rep *) collect_list.First();
					url_rep->Out();
					url_rep->DecRef();
					OP_DELETE(url_rep);
				}
			}
		}
#endif // defined(CACHE_MAX_VISITED_URL_COUNT) && CACHE_MAX_VISITED_URL_COUNT>0
	}

	CACHE_PROPAGATE_ALWAYS_VOID(AutoSaveCacheL());
}

#if defined(DISK_CACHE_SUPPORT) && !defined(SEARCH_ENGINE_CACHE)
#ifdef SELFTEST
void Context_Manager::TestWriteIndexFileL(OpStringC filename, OpFileFolder folder, BOOL fast)
{
	URLLinkHead dcache_list;
	ANCHOR(URLLinkHead, dcache_list);

	URL_Rep *url_rep ;

	url_rep = url_store->GetFirstURL_Rep();

	// Populate the dcache_list
	while(url_rep)
	{
		URL_DataStorage *storage=url_rep->GetDataStorage();
		BOOL embedded=storage && storage->GetCacheStorage() && storage->GetCacheStorage()->IsEmbedded();
		
		if((URLStatus) url_rep->GetAttribute(URL::KLoadStatus) == URL_LOADED && 
			(url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_DISK || embedded))
		{
			if(url_rep->CheckStorage()	&&
					(url_rep->GetDataStorage()->GetCacheStorage()->GetCacheType() == URL_CACHE_DISK	|| embedded)
					&&	!url_rep->GetDataStorage()->GetCacheStorage()->GetIsMultipart())
			{
				URL url(url_rep, (const char *) NULL);
				
				URLLink *item = OP_NEW(URLLink, (url));
				if(item)
					item->Into(&dcache_list);
			}
		}
		url_rep = url_store->GetNextURL_Rep();
	}
	
	#ifdef CACHE_FAST_INDEX
		if(fast)
			WriteIndexFileSimpleL(dcache_list, TAG_CACHE_FILE_ENTRY, filename, folder, TRUE, FALSE);
		else
			WriteIndexFileL(dcache_list, TAG_CACHE_FILE_ENTRY, filename, folder, TRUE, FALSE);
	#else
		OP_ASSERT(!fast);
		
		WriteIndexFileL(dcache_list, TAG_CACHE_FILE_ENTRY, filename, folder, FALSE, FALSE);
	#endif
}
#endif // SELFTEST

void Context_Manager::WriteIndexFileL(URLLinkHead &list, uint32 tag, OpStringC filename, OpFileFolder folder, BOOL write_lastentry, BOOL destroy_urls)
{
	DataStream_NonSensitive_ByteArrayL(write_cache_index);

	OpStackAutoPtr<DataFile> listfile(NULL);
	OpStackAutoPtr<OpDataStreamSafeFile> base_listfile(OP_NEW_L(OpDataStreamSafeFile, ()));
	LEAVE_IF_ERROR(base_listfile->Construct(filename.CStr(), folder));
	OpStackAutoPtr<DataFile_Record> url_rec(NULL);
	OpStackAutoPtr<URLLink> item(NULL);
	URLLink *item2;

	LEAVE_IF_ERROR(base_listfile->Open(OPFILE_WRITE));
	listfile.reset(OP_NEW_L(DataFile, (base_listfile.get(), CURRENT_CACHE_VERSION, 1, 2)));
	base_listfile.release();

	listfile->InitL();

	while((item2 = list.First()) != NULL && listfile->Opened())
	{
		item.reset(item2);
		item2->Out();

		if(tag == TAG_VISITED_FILE_ENTRY || !item->url.GetAttribute(URL::KFileName).IsEmpty())
		{
			url_rec.reset(OP_NEW_L(DataFile_Record, (tag)));
			url_rec->SetRecordSpec(listfile->GetRecordSpec());
			item->url.WriteCacheDataL(url_rec.get());
			url_rec->WriteRecordL(listfile.get());
			url_rec.reset();
		}

		if(destroy_urls)
		{
			DestroyURL(item2->url);
		}
		item.reset();
	}

#ifndef SEARCH_ENGINE_CACHE
	if(write_lastentry && listfile->Opened())
	{
		url_rec.reset(OP_NEW_L(DataFile_Record, (TAG_CACHE_LASTFILENAME_ENTRY)));

		OpStringC tempstring(file_str+3);
		ANCHOR(OpStringC,tempstring);


		OpString8 tempstring1;
		ANCHOR(OpString8,tempstring1);

		tempstring1.SetUTF8FromUTF16L(tempstring.CStr());
		tempstring1.Delete(5);

		url_rec->SetRecordSpec(listfile->GetRecordSpec());
		url_rec->AddContentL(tempstring1);
		url_rec->WriteRecordL(listfile.get());
		url_rec.reset();
	}
#endif
	if(!listfile->Opened())
		LEAVE(OpStatus::ERR);

	listfile->Close();
	listfile.reset();
}

#ifdef CACHE_FAST_INDEX
void Context_Manager::WriteIndexFileSimpleL(URLLinkHead &list, uint32 tag, OpStringC filename, OpFileFolder folder, BOOL write_lastentry, BOOL destroy_urls)
{
	OpConfigFileWriter cache;
	ANCHOR(OpConfigFileWriter, cache);
	DiskCacheEntry disk_entry;
	ANCHOR(DiskCacheEntry, disk_entry);
	
	OpStackAutoPtr<URLLink> item(NULL);
	URLLink *item2;

	#ifdef CACHE_SAFE_FILE
		LEAVE_IF_ERROR(cache.Construct(filename.CStr(), folder, 1, 2, TRUE));
	#else
		LEAVE_IF_ERROR(cache.Construct(filename.CStr(), folder, 1, 2, FALSE));
	#endif
	
#ifndef SEARCH_ENGINE_CACHE
	// The last entry is now the first entry, to prevent the cache file from being corrupted
	if(write_lastentry)
	{
		//url_rec.reset(OP_NEW_L(DataFile_Record, (TAG_CACHE_LASTFILENAME_ENTRY)));

		OpStringC tempstring(file_str+3);
		ANCHOR(OpStringC,tempstring);

		OpString8 tempstring1;
		ANCHOR(OpString8,tempstring1);

		tempstring1.SetUTF8FromUTF16L(tempstring.CStr());
		tempstring1.Delete(5);
		
		LEAVE_IF_ERROR(cache.WriteBufTag(TAG_CACHE_LASTFILENAME_ENTRY, tempstring1.CStr(), 5));
	}
#endif
	
	while((item2 = list.First()) != NULL)
	{
		item.reset(item2);
		
		item2->Out();
		
		const OpStringC cf_name=item->url.GetAttribute(URL::KFileName);
		
		if(
			tag == TAG_VISITED_FILE_ENTRY || !cf_name.IsEmpty()
			#if CACHE_SMALL_FILES_SIZE>0
				|| item->url.GetRep()->IsEmbedded()
			#endif
			)
		{
			disk_entry.Reset();
			disk_entry.SetTag(tag);
			
			OP_ASSERT(disk_entry.GetEmbeddedContentSize()==0);
			
			// Fill the disk_entry with all the data required
			item->url.GetRep()->WriteCacheDataL(&disk_entry);
			
			#if CACHE_CONTAINERS_ENTRIES>0
				if(item->url.GetRep()->GetDataStorage() && item->url.GetRep()->GetDataStorage()->GetCacheStorage())
					disk_entry.SetContainerID(item->url.GetRep()->GetDataStorage()->GetCacheStorage()->GetContainerID());
			#endif
			
			//url_rec->WriteRecordL(listfile.get());
			//url_rec.reset();
			
			// Write all the data on the disk
			
			BOOL embedded=
			#if CACHE_SMALL_FILES_SIZE>0
				item->url.GetRep()->IsEmbedded()
			#else
				FALSE
			#endif
			;

			// URLs can be discarded because above the maximum size allowed for the index (maybe because of big headers or
			// or becasue they are big data://)
			BOOL url_refused=FALSE;  

			LEAVE_IF_ERROR(disk_entry.WriteValues(&cache, TAG_CACHE_FILE_ENTRY, embedded, url_refused));

			// If the URL has been refused, we make it temporary, so it will stay around but it will be deleted when possible
			if(url_refused)
				item->url.GetRep()->SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
		}
		if(destroy_urls)
		{
			DestroyURL(item2->url);
		}
		item.reset();
	}

	LEAVE_IF_ERROR(cache.Close((char *)NULL));
}

#endif // CACHE_FAST_INDEX

#endif // DISK_CACHE_SUPPORT

#ifdef DISK_CACHE_SUPPORT
#ifdef SEARCH_ENGINE_CACHE
BOOL Context_Manager::IndexFileL(CacheIndex &index, URL_Rep *url_rep, BOOL force_index, OpFileLength cache_size_limit)
{
	OP_STATUS err;
	time_t visited = 0;
	url_rep->GetAttribute(URL::KVLocalTimeVisited, &visited);

	if(!force_index &&
		// older than 10 minutes
		visited + 600 >= g_timecache->CurrentTime())
		return TRUE;

	OP_ASSERT(index.IsOpen());

	if(index.IsOpen())
	{
		if(OpStatus::IsError(err = index.Insert(url_rep, cache_size_limit)))
		{
			index.Abort();
			LEAVE(err);
		}
		if(err == OpBoolean::IS_FALSE)
			url_rep->SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
	}
	else
		err = OpBoolean::IS_FALSE;

	return err == OpBoolean::IS_TRUE;
}

void Context_Manager::MakeFileTemporary(URL_Rep *url_rep)
{
	url_store->m_disk_index.MakeFileTemporary(url_rep);
}
#endif // SEARCH_ENGINE

#endif // DISK_CACHE_SUPPORT
