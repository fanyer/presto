/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_cs.h"
#include "modules/cache/url_stor.h"
#include "modules/cache/cache_common.h"
#include "modules/cache/cache_utils.h"
#include "modules/cache/cache_propagate.h"
#include "modules/cache/cache_int.h"
#include "modules/cache/cache_selftest.h"
#include "modules/cache/context_man.h"

#ifdef WEB_TURBO_MODE
# include "modules/dochand/winman.h"
#endif // WEB_TURBO_MODE

URL_DataStorage* URLLinkHeadIterator::Next()
{
	URLLink *cur=next;

	while(cur && (!cur->url.GetRep() || !cur->url.GetRep()->GetDataStorage()))  // Look for a record with a data storage
		cur=(URLLink *)cur->Suc();

	OP_ASSERT(!cur || (cur->url.GetRep() && cur->url.GetRep()->GetDataStorage()) );


	next=(cur) ? ((URLLink *)cur->Suc()) : NULL;
	last=cur;

	return (cur) ? cur->url.GetRep()->GetDataStorage(): NULL;
}

OP_STATUS URLLinkHeadIterator::DeleteLast()
{
	OP_ASSERT(last);

	if(!last)
		return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(last->InList());

	last->Out();

	last->url.SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
#if CACHE_CONTAINERS_ENTRIES>0
	last->url.GetRep()->GetDataStorage()->GetCacheStorage()->PreventContainerFileDelete();
#endif // CACHE_CONTAINERS_ENTRIES>0

	last->url.Unload();  // The URLLink is not freed
	OP_DELETE(last);	// The URLLink has to be deleted

	last=NULL;

	return OpStatus::OK;
}

URL_DataStorage* URL_DataStorageIterator::Next()
{
	last=next;
	next=(next) ? (URL_DataStorage *)next->Suc() : NULL;

	return last;
}

OP_STATUS URL_DataStorageIterator::DeleteLast()
{
	OP_ASSERT(last);

	if(!last)
		return OpStatus::ERR_NULL_POINTER;

	BOOL in_list=last->InList();

	last->SetAttribute(URL::KCacheType, URL_CACHE_TEMP);

#if CACHE_CONTAINERS_ENTRIES>0
	last->GetCacheStorage()->PreventContainerFileDelete();
#endif // CACHE_CONTAINERS_ENTRIES>0

	if(in_list)
		man->UnloadURL(last);        // Also delete cur_item, OP_DELETE() would crash
		//cur_item->url->Unload();	// Also delete cur_item, OP_DELETE() would crash
	else
	{
	#ifdef DISK_CACHE_SUPPORT
		man->DestroyURL(man->GetURLRep(last));
	#endif // DISK_CACHE_SUPPORT

		OP_DELETE(last);
	}

	return OpStatus::OK;
}

CacheLimit::CacheLimit(BOOL a_add_url_size)
{
	used_size = 0;
	max_size = 0;
	add_url_size = a_add_url_size;
#ifdef SELFTEST
	oom=FALSE;
#endif // SELFTEST
}

#ifdef SELFTEST
void CacheLimit::DeleteDataFromHashtable(const void *key, void *data)
{
	OP_DELETE((URLLength *)data);
}
#endif // SELFTEST

CacheLimit::~CacheLimit()
{
#ifdef SELFTEST
	url_sizes.ForEach(DeleteDataFromHashtable);
#endif // SELFTEST
}

#ifdef SELFTEST
void CacheLimit::DebugVerifyURLLength(URL_Rep *url, OpFileLength content_size, OpFileLength url_size)
{
	OpFileLength prev_content=0;
	OpFileLength prev_url=0;
	URLLength *prev_length=NULL;
	
	if(OpStatus::IsSuccess(url_sizes.GetData(url, &prev_length)) && prev_length)
	{
		prev_content=prev_length->content_size;
		prev_url=prev_length->url_size;
	}

	// In case of OOM, the check is kept if the previous value was !=0 (this because Remove are always supposed to be executed)
    // Unfortunately we don't know if Opera had an OOM elsewhere, which could prevent a proper balance of calls
	if(!oom || prev_content)
		OP_ASSERT(prev_content==content_size);
	if(!oom || prev_url)
		OP_ASSERT(prev_url==url_size);
}

void CacheLimit::DebugRemoveURLLength(URL_Rep *url)
{
	URLLength *prev_length=NULL;
	
	OpStatus::Ignore(url_sizes.Remove(url, &prev_length));

	if(prev_length)
		OP_DELETE(prev_length);
}

void CacheLimit::DebugAddURLLength(URL_Rep *url, OpFileLength content_size, OpFileLength url_size)
{
	URLLength *prev_length=NULL;
	BOOL created=FALSE;

	OP_NEW_DBG("CacheLimit::DebugAddURLLength()", "cache.limit");
	
	if(OpStatus::IsError(url_sizes.GetData(url, &prev_length)) || !prev_length)
	{
		prev_length=OP_NEW(URLLength, ());

		if(!prev_length)
		{
			oom=TRUE;
			return;
		}

		created = TRUE;

		OP_DBG((UNI_L("URL: %s - %x - Previous values: %d+%d - ctx id: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) prev_length->content_size, (UINT32) prev_length->url_size, url->GetContextId()));
	}

	prev_length->content_size+=content_size;
	prev_length->url_size+=url_size;

	OP_DBG((UNI_L("URL: %s - %x - New values: %d+%d - ctx id: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) prev_length->content_size, (UINT32) prev_length->url_size, url->GetContextId()));
	
	if(created && OpStatus::IsError(url_sizes.Add(url, prev_length)))
	{
		oom=TRUE;
		OP_DELETE(prev_length);
	}
}

void CacheLimit::DebugSubURLLength(URL_Rep *url, OpFileLength content_size, OpFileLength url_size)
{
	URLLength *prev_length=NULL;

	OP_NEW_DBG("CacheLimit::DebugSubURLLength()", "cache.limit");
	
	if(OpStatus::IsSuccess(url_sizes.GetData(url, &prev_length)))
	{
		OP_ASSERT(prev_length && "No previous size recorded");

		if(prev_length)
		{
			OP_ASSERT(prev_length->content_size >= content_size);
			OP_ASSERT(prev_length->url_size >= url_size);

			prev_length->content_size-=content_size;
			prev_length->url_size-=url_size;

			OP_DBG((UNI_L("URL: %s - %x - New values: %d+%d - ctx id: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) prev_length->content_size, (UINT32) prev_length->url_size, url->GetContextId()));
		}
	
	}
	else
		OP_ASSERT(!"URL not found!");
}

void Context_Manager_Base::VerifyURLSizeNotCounted(URL_Rep *url, BOOL check_memory, BOOL check_disk)
{
	if(check_memory)
		size_ram.DebugVerifyURLLength(url, 0, 0);

	if(check_disk)
		size_disk.DebugVerifyURLLength(url, 0, 0);
}
#endif // SELFTEST

void CacheLimit::AddToSize(OpFileLength content_size, URL_Rep *url, BOOL add_url_size_estimantion)
{
	OP_ASSERT(url);
	OpFileLength add_used = content_size;
	OpFileLength url_size_estimation = (add_url_size && add_url_size_estimantion) ? url->GetURLObjectSize(): 0;

#ifdef SELFTEST
    //DebugVerifyURLLength(url, 0, 0);
	// DebugRemoveURLLength(url);
	DebugAddURLLength(url, content_size, url_size_estimation);
#endif // SELFTEST
    
	add_used += url_size_estimation;

	if((OpFileLength) (LONG_MAX - used_size) > add_used)
		used_size += add_used;
	else
		used_size = LONG_MAX;
}

void CacheLimit::SubFromSize(OpFileLength content_size, URL_Rep *url, BOOL enable_checks)
{
	OpFileLength sub_used=content_size;

#ifdef SELFTEST
	if(enable_checks)
	{
		DebugVerifyURLLength(url, content_size, url->GetURLObjectSize());
		DebugRemoveURLLength(url);
		DebugVerifyURLLength(url, 0, 0);
	}
	else
		DebugSubURLLength(url, content_size, (add_url_size) ? url->GetURLObjectSize() : 0);
#endif // SELFTEST
	
	if(add_url_size)
		sub_used += url->GetURLObjectSize();

	OP_ASSERT(sub_used<=used_size && "AddToSize() / SubFromSize() calls mismatch. The cache is reporting a wrong size.");

	if(used_size > sub_used)
		used_size -= sub_used;
	else
		used_size = 0;
}

BOOL CacheLimit::IsLimitExceeded(OpFileLength limit, OpFileLength add) const
{
#ifdef SEARCH_ENGINE_CACHE
	return FALSE;
#else
	OpFileLength check_limit = 
	#if defined STRICT_CACHE_LIMIT2
		limit - limit / 20;  // We check before reaching the limit
	#else
		limit + limit / 20;  // Extra 5% allowed
	#endif // STRICT_CACHE_LIMIT2

	if((OpFileLength) (LONG_MAX - used_size) <add)
		return TRUE; // overflow protection

	return used_size + add > check_limit;
#endif // SEARCH_ENGINE_CACHE
}

Context_Manager_Base::Context_Manager_Base() :
	size_disk(TRUE),
	size_ram(TRUE)
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	, size_oem(TRUE)
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
#ifdef STRICT_CACHE_LIMIT2
	, size_predicted(TRUE)
#endif // STRICT_CACHE_LIMIT2
{
#ifdef SELFTEST
	//debug_free_unused = FALSE;
	debug_deep = FALSE;
	embed_all_types = FALSE;
#endif //SELFTEST

#ifdef URL_FILTER
	empty_dcache_filter=NULL;
#endif

#if defined(MHTML_ARCHIVE_REDIRECT_SUPPORT)
	offline = FALSE;
#endif
}


Context_Manager::Context_Manager(URL_CONTEXT_ID a_id
#if defined DISK_CACHE_SUPPORT
								 ,OpFileFolder a_vlink_loc,
								 OpFileFolder a_cache_loc
#endif
) :
#ifdef APPLICATION_CACHE_SUPPORT
	is_used_by_application_cache(FALSE),
#endif // APPLICATION_CACHE_SUPPORT

#ifdef DISK_CACHE_SUPPORT
	cache_loc(a_cache_loc), vlink_loc(a_vlink_loc),
#endif
	context_id(a_id)
{
	InternalInit();
#ifdef CACHE_STATS
	stats.StatsReset(TRUE);
#endif

#ifdef CACHE_SYNC
	activity_done=FALSE;
#endif
}

#ifdef CACHE_STATS
void CacheStatistics::StatsReset(BOOL reset_index)
{
	if(reset_index)
	{
		index_read_time=0;
		index_num_read=0;
		max_index_read_time=0;
		index_init=0;
		urls=0;
		req=0;
		container_files=0;
		sync_time1=0;
		sync_time2=0;
	}

}
#endif

void Context_Manager::InternalInit()
{
	url_store = NULL;
	next_manager = NULL;
	RAM_only_cache = FALSE;
	is_writing_cache = FALSE;
#ifdef URL_ENABLE_ASSOCIATED_FILES
	assoc_file_counter = 0;
#endif // URL_ENABLE_ASSOCIATED_FILES
	flushing_ram_cache = FALSE;
	flushing_disk_cache = FALSE;
	do_check_ram_cache = FALSE;
	do_check_disk_cache = FALSE;
#ifdef CACHE_MULTIPLE_FOLDERS
	enable_directories = TRUE;
#endif
	last_disk_cache_flush = 0;
	last_ram_cache_flush = 0;
	trial_count_cache_flush = 0;
	LRU_ram = NULL;
	LRU_temp = NULL;
	LRU_disk = NULL;
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	size_oem.SetSize((OpFileLength) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OperatorDiskCacheSize) * 1024);
#endif
	// Context spec
	references_count = 1;
	predestruct_done = FALSE;
#ifdef DISK_CACHE_SUPPORT
	cache_written_before_shutdown = FALSE;
#endif // DISK_CACHE_SUPPORT
	cache_size_pref = -1;
#ifdef STRICT_CACHE_LIMIT
    ramcache_used_extra = 0;
#endif
#ifdef NEED_URL_VISIBLE_LINKS
	visible_links = TRUE;
#endif // NEED_URL_VISIBLE_LINKS

#ifdef RAMCACHE_ONLY
	size_disk.SetSize((OpFileLength) -1);
	size_ram.SetSize((OpFileLength) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheSize) * 1024);
#else
	size_disk.SetSize((OpFileLength) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheSize) * 1024);
	size_ram.SetSize((OpFileLength ) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheBufferSize) * 1024);

	// If CacheToDisk is off use DiskCacheSize as RAM size as if RAMCACHE_ONLY was used
	if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheToDisk) == 0)
	{
		size_disk.SetSize((OpFileLength) -1);
		size_ram.SetSize((OpFileLength) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheSize) * 1024);
	}
#endif

#ifndef SEARCH_ENGINE_CACHE
	uni_strcpy(file_str, UNI_L("opr00000.tmp"));
#endif

#ifdef STRICT_CACHE_LIMIT
    memory_limit = size_ram.GetSize();
 
    memory_limit_margin = (int) (memory_limit * 0.1);
    if(memory_limit_margin> 200*1024) memory_limit_margin=200 * 1024;
#endif
#ifdef SUPPORT_RIM_MDS_CACHE_INFO
	m_cacheInfoSession = NULL;
	m_cacheInfoSessionId = -1;
#endif // SUPPORT_RIM_MDS_CACHE_INFO

#ifdef SELFTEST
	dd_bytes_moved = 0;
	dd_total_num = 0;
	dd_peak_num = 0;
	dd_memory = 0;
	check_invariants = TRUE;
#endif //SELFTEST

}

void Context_Manager::InitL()
{
#if defined(CACHE_PURGE) && !defined(DISK_CACHE_SUPPORT)
	if(!context_id)
		CleanDCacheDirL(); // Clean the temporary files created by the ns4plugins (see CORE-26186), but only at startup, when the main context is created
#endif

	OP_STATUS op_err = URL_Store::Create(&url_store, URL_STORE_SIZE
#if defined DISK_CACHE_SUPPORT
		, cache_loc
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		, context_id == 0 ? OPFILE_OCACHE_FOLDER : OPFILE_ABSOLUTE_FOLDER
#endif
#endif // DISK_CACHE_SUPPORT
		);
	LEAVE_IF_ERROR(op_err);
}

Context_Manager::~Context_Manager()
{
	InternalDestruct();
}

void Context_Manager::InternalDestruct()
{
#ifdef SUPPORT_RIM_MDS_CACHE_INFO
	if(m_cacheInfoSession )
	{
		OP_DELETE(m_cacheInfoSession);
		m_cacheInfoSession = NULL;
	}
#endif // SUPPORT_RIM_MDS_CACHE_INFO

	if(!predestruct_done)
		PreDestructStep(FALSE);
	if(InList()) Out();

	if(next_manager)
	{
		OP_DELETE(next_manager);

		next_manager=NULL;
	}

	/// Note for dd_stats: URL Data Descriptors are supposed to be delete, and they will take care 
	// of freeing the object in dd_stats, by calling RemoveDataDescriptorForStats()
}

void Context_Manager::PreDestructStep(BOOL from_predestruct)
{
	if(predestruct_done)
		return;

	OP_NEW_DBG("Context_Manager::PreDestructStep", "cache.del");
	OP_DBG((UNI_L("*** Destructing Context %d"), context_id));

	IncReferencesCount();
	predestruct_done = TRUE;

	URL_DataStorage *url_ds; 
	while((url_ds = (URL_DataStorage *)LRU_list.First()) != NULL)
	{
		OP_DBG((UNI_L("*** Data storage: %x - url: %x"), url_ds, url_ds->url));

		OP_ASSERT(url_ds->url->GetContextId()==context_id);

		url_ds->url->Unload();
	}
	LRU_ram = LRU_disk = LRU_temp = NULL;

	OP_DELETE(url_store);
	url_store = NULL;

	DecReferencesCount();
}

BOOL Context_Manager::GetIsRAM_Only() const
{
#ifdef DISK_CACHE_SUPPORT
	if (context_id)
		return RAM_only_cache;
	else
		return !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheToDisk);
#else
	return TRUE;
#endif
}

void Context_Manager::AddURL_Rep(URL_Rep *new_rep)
{
	// If someone tries to add new URL to a context already frozen, probably there is a bug somewhere in the code, as it means that
	// a part of Opera wants to remove it, while a part is still using it
	OP_ASSERT(!urlManager->IsContextManagerFrozen(this) && "Context frozen");

	OP_ASSERT(new_rep);
	
	if(new_rep)
		url_store->AddURL_Rep(new_rep);
}

URL_Rep *Context_Manager::GetResolvedURL(URL_Name_Components_st &resolved_name, BOOL create)
{
	URL_Rep * OP_MEMORY_VAR rep = NULL;
	OP_STATUS op_err= OpStatus::ERR;
	BOOL propagate=next_manager && !next_manager->IsRemoteManager();
	
	rep = url_store->GetURL_Rep(op_err, resolved_name, !propagate && create, context_id);
	RAISE_IF_ERROR(op_err);

	if(!rep && propagate)
	{
		//CACHE_PROPAGATE_NO_REMOTE_VOID(GetResolvedURL(resolved_name, FALSE), NULL);
		rep=next_manager->GetResolvedURL(resolved_name, FALSE);
		
		if(!rep && create)
		{
			rep = url_store->GetURL_Rep(op_err, resolved_name, !next_manager && create, context_id);
			RAISE_IF_ERROR(op_err);
		}
	}
	
	#ifdef CACHE_STATS
		stats.req++;
	#endif

	return rep;
}

URL_Rep *Context_Manager::LocateURL(URL_ID id)
{
	URL_Rep *rep = url_store->GetFirstURL_Rep();
	
	while(rep)
	{
		if(rep->GetID() == id)
			return rep;

		rep = url_store->GetNextURL_Rep();
	}

	CACHE_PROPAGATE_NO_REMOTE_RETURN(LocateURL(id), NULL);
}

void Context_Manager::MakeAllURLsUnique()
{
	URL_Rep *rep = url_store->GetFirstURL_Rep();

	while(rep)
	{
		URL_Rep *new_rep=url_store->GetNextURL_Rep();

		// Make sure every remaining URL gets deleted ASAP
		MakeUnique(rep);
		if(rep->GetRefCount() == 0)
			OP_DELETE(rep);
		
		rep = new_rep;
	}
}

void Context_Manager::MakeUnique(URL_Rep *url)
{
	if(!url || url->GetAttribute(URL::KUnique))
		return;

	if(url->InList())
	{
		url->DecRef();
		url_store->RemoveURL_Rep(url);
	}

	url->SetAttribute(URL::KUnique,TRUE);
}

void Context_Manager::SetLRU_Item(URL_DataStorage *url_ds)
{
	/// Check if the LRU pointers are in list
	OP_ASSERT(!LRU_temp || LRU_temp->InList());
	OP_ASSERT(!LRU_disk || LRU_disk->InList());
	OP_ASSERT(!LRU_ram || LRU_ram->InList());

	if(url_ds == NULL)
		return;

	RemoveLRU_Item(url_ds);

	if(url_ds->storage == NULL)
		return;

	URL_DataStorage *infront = NULL;
	URL_DataStorage **frontlist = NULL;
	URLCacheType ctype = (URLCacheType) url_ds->GetAttribute(URL::KCacheType);
	URLType type = (URLType) url_ds->GetAttribute(URL::KType);

	if(url_ds->storage->UsingMemory())
	{
		infront = (LRU_temp ? LRU_temp : LRU_disk);
		frontlist = &LRU_ram;
	}
	else if(ctype == URL_CACHE_DISK ||
		(ctype == URL_CACHE_USERFILE && (type == URL_HTTP || type == URL_FTP)))
	{
		frontlist = &LRU_disk;
	}
	else if(ctype != URL_CACHE_USERFILE)
	{
		infront = LRU_disk;
		frontlist = &LRU_temp;
	}

	if(infront)
	{
		if(infront->InList())
			url_ds->Precede(infront);
	}
	else
		url_ds->Into(&LRU_list);

	if(frontlist && (*frontlist) == NULL)
		*frontlist = url_ds;

	/// Check if the LRU pointers are in list
	OP_ASSERT(!LRU_temp || LRU_temp->InList());
	OP_ASSERT(!LRU_disk || LRU_disk->InList());
	OP_ASSERT(!LRU_ram || LRU_ram->InList());
}

void Context_Manager::RemoveLRU_Item(URL_DataStorage *url_ds)
{
	/// Check if the LRU pointers are in list
	OP_ASSERT(!LRU_temp || LRU_temp->InList());
	OP_ASSERT(!LRU_disk || LRU_disk->InList());
	OP_ASSERT(!LRU_ram || LRU_ram->InList());

	if(!url_ds || !url_ds->InList())
		return;

	URL_DataStorage *next = (URL_DataStorage *) url_ds->Suc();

	// If it is a LRU item, the LRU is moved to the next URL, or set to NULL if the next element is the head of another LRU list
	if(url_ds == LRU_ram)
		LRU_ram = ((LRU_temp ? LRU_temp : LRU_disk) == next ? NULL : next);
	else if(url_ds == LRU_temp)
		LRU_temp = (LRU_disk == next ? NULL : next);
	else if(url_ds == LRU_disk)
		LRU_disk = next;

	url_ds->Out();

	/// Check if the LRU pointers are in list
	OP_ASSERT(!LRU_temp || LRU_temp->InList());
	OP_ASSERT(!LRU_disk || LRU_disk->InList());
	OP_ASSERT(!LRU_ram || LRU_ram->InList());
}

#if defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)
void Context_Manager::DeleteFiles(FileName_Store &filenames)
{
	if(filenames.GetFirstFileName())
	{
		if(urlManager)
			urlManager->AddFilesToDeleteList(context_id, filenames);
	}
}
#endif // defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)

#ifdef NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS
void Context_Manager::RestartAllConnections()
{
	// LMTODO2: check implementation!
# ifdef DYNAMIC_PROXY_UPDATE
	if(url_store)
	{
		URL_Rep* url_rep = url_store->GetFirstURL_Rep();
		while (url_rep)
		{
			url_rep->StopAndRestartLoading();
			url_rep = url_store->GetNextURL_Rep();
		}
	}
	if(!context_id)
		urlManager->SetPauseStartLoading(FALSE);

	CACHE_PROPAGATE_ALWAYS_VOID(RestartAllConnections());
# endif // DYNAMIC_PROXY_UPDATE
}

void Context_Manager::StopAllLoadingURLs()
{
	if(url_store)
	{
		URL_Rep* url_rep = url_store->GetFirstURL_Rep();
		while (url_rep)
		{
		if(url_rep->GetAttribute(URL::KLoadStatus,FALSE) ==	URL_LOADING)
            {
				url_rep->HandleError(ERR_SSL_ERROR_HANDLED);
				url_rep->SetAttribute(URL::KLoadStatus,	URL_LOADING_ABORTED);
            }
			url_rep = url_store->GetNextURL_Rep();
		}
	}
	if(!context_id)
		urlManager->SetPauseStartLoading(FALSE);

	CACHE_PROPAGATE_ALWAYS_VOID(StopAllLoadingURLs());
}
#endif // NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS


OpFileLength Context_Manager_Base::GetCacheSize(void)
{
#if defined DISK_CACHE_SUPPORT && defined CACHE_SINGLE_CACHE_LIMIT
// 10% of total cache size is reserved for widgets with maximum 3.75% for one widget and minimum of 10KB
#define WIDGET_CACHE_RATIO_DIV 10
#define WIDGET_CACHE_RATIO_MUL 1
#define MAX_WIDGET_RATIO_DIV 80
#define MAX_WIDGET_RATIO_MUL 3
#define MIN_WIDGET_SIZE 10240

	OpFileLength ccaches;
	OpFileLength max_size;

	ccaches = ((Context_Manager *)(urlManager))->context_list.Cardinal();

	if(ccaches == 0)
		return size_disk.GetSize();

	max_size = ccaches * MAX_WIDGET_RATIO_MUL * size_disk.GetSize() / MAX_WIDGET_RATIO_DIV;
	if(max_size > WIDGET_CACHE_RATIO_MUL * size_disk.GetSize() / WIDGET_CACHE_RATIO_DIV)
		max_size =  WIDGET_CACHE_RATIO_MUL * size_disk.GetSize() / WIDGET_CACHE_RATIO_DIV;
	if(max_size < ccaches * MIN_WIDGET_SIZE)
		max_size = ccaches * MIN_WIDGET_SIZE;

	if(this == urlManager)
		return size_disk.GetSize() - max_size;

	return max_size / ccaches;
#else
	return size_disk.GetSize();
#endif  // DISK_CACHE_SUPPORT
}




#if CACHE_SYNC == CACHE_SYNC_ON_CHANGE
void Context_Manager::SignalCacheActivity()
{
#ifdef DISK_CACHE_SUPPORT
	OpFile flag_file;
	
	if(!activity_done &&
		OpStatus::IsSuccess(flag_file.Construct(CACHE_ACTIVITY_FILE, GetCacheLocationForFilesCorrected())) && 
		OpStatus::IsSuccess(flag_file.Open(OPFILE_WRITE)))
	{
		OpStatus::Ignore(flag_file.Close());
		activity_done=TRUE;
	}
#endif
}

void Context_Manager::SignalCacheSynchronized(BOOL check_delete_queue)
{
#ifdef DISK_CACHE_SUPPORT
	if(check_delete_queue && !urlManager->AllFilesDeletedInContext(context_id))
		return;

	// The index has been written (== synchronized): delete the flag file
	OpFile flag_file;

	if(OpStatus::IsSuccess(flag_file.Construct(CACHE_ACTIVITY_FILE, GetCacheLocationForFilesCorrected())))
	{
		flag_file.Delete(FALSE);
	}
	activity_done=FALSE;
#endif

}
#endif // #if CACHE_SYNC == CACHE_SYNC_ON_CHANGE
		

#ifdef _OPERA_DEBUG_DOC_
void Context_Manager::WriteDebugInfo(URL &url)
{
#ifdef DISK_CACHE_SUPPORT 
	/*double a = size_disk.GetUsed() / 1024;
	int b = a;
	double aa = size_disk.GetSize() / 1024;
	int bb = aa;*/
	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr>    <td>Disk Cache</td>      <td>%d kB</td>    <td>%d kB</td>    <td>%d</td>         </tr>\r\n"),
		int(double(size_disk.GetUsed() / 1024)),
		int(double(size_disk.GetSize() / 1024)),
		size_disk.GetSize() ? (int) (100.0 * size_disk.GetUsed() / size_disk.GetSize()) : 0);
#endif
	url.WriteDocumentDataUniSprintf(
	UNI_L("<tr>    <td>RAM Cache</td>   <td>%d kB</td>    <td>%d kB</td>    <td>%d</td>         </tr>\r\n</table>\r\n"),
		int(double(size_ram.GetUsed() / 1024)),
		int(double(size_ram.GetSize() / 1024)),
		size_ram.GetSize() ? (int) (100.0 * size_ram.GetUsed() / size_ram.GetSize()) : 0);

	url.WriteDocumentDataUniSprintf(UNI_L("<h1>Cache Manager info</h1>\r\n"));

#ifdef SELFTEST
	/// Info related to URL Data Descriptors
	OpVector<URLDD_Stats> dd_stats;

	unsigned long dd_bytes_used=0;
	unsigned long dd_num=0;

	if(OpStatus::IsSuccess(GetDataDescriptorsListForStats(dd_stats)))
	{
		for(int i=0, len=dd_stats.GetCount(); i<len; i++)
		{
			URLDD_Stats *stats = dd_stats.Get(i);
			URL_DataDescriptor *dd = (stats) ? stats->GetDescriptor() : NULL;

			if(dd)
			{
				dd_num++;
				dd_bytes_used+=dd->GetBufMaxLength();
			}
		}
	}

	url.WriteDocumentDataUniSprintf(UNI_L("<h5>URL Data Descriptors summary</h5>\r\n<ul>"));
	url.WriteDocumentDataUniSprintf(
		UNI_L("<table><tr>    <td>Pending Descriptors: %d</td>   <td>Descr. created so far: %d</td>    <td>Peak pending Desc.: %d </td></tr>\r\n"),
		dd_num, dd_total_num, dd_peak_num);
	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr> <td>Memory in use: %dkB</td>  <td>Memory used and freed: %d kB</td>  <td>Data moved: %d kB</td>  </tr>\r\n"),
		(int)((GetTotalMemoryAllocated(TRUE, FALSE)+1023) / 1024), (int)((GetTotalMemoryAllocated(FALSE, TRUE)+1023) / 1024), (int)((StatsGetBytesMemMoved() + 1023) / 1024));

	// List of URL Data Descriptors in use (it should ideally be 0 when the documents are loaded) 
	url.WriteDocumentDataUniSprintf(UNI_L("</table><h5>URL Data Descriptors alive</h5>\r\n<ul>"));

	for(int i=0, len=dd_stats.GetCount(); i<len; i++)
	{
		URLDD_Stats *stats = dd_stats.Get(i);
		URL_DataDescriptor *dd = (stats) ? stats->GetDescriptor() : NULL;

		if(dd)
		{
			URL url_dd=dd->GetURL();

			OpFileLength content_len=0;

			url_dd.GetAttribute(URL::KContentSize, &content_len);

			url.WriteDocumentDataUniSprintf(UNI_L("<li>Allocated: %d KB, remaining KB: %d, Cont. Len: %d KB, Moved: %d KB, url: "), (dd->GetBufMaxLength()+1023)/1024, (dd->GetBufSize()+1023)/1024, (int)((content_len+1023) / 1024), (int)stats->GetBytesMemMoved());
			url.WriteDocumentData(URL::KHTMLify, url_dd.GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI).CStr());
			url.WriteDocumentData(URL::KNormal, UNI_L("</li>\r\n"));
		}
	}
#endif

	url.WriteDocumentDataUniSprintf(UNI_L("</ul><h5>Currently referenced or used urls stored on disk</h5>\r\n<ul>"));
	URL_Rep* url_rep = url_store->GetFirstURL_Rep();
//    url.WriteDocumentDataUniSprintf(UNI_L("<ul>"));
	while (url_rep)
	{
		if(url_rep->GetDataStorage() && url_rep->GetAttribute(URL::KLoadStatus,FALSE) == URL_LOADING && url_rep->GetDataStorage()->storage && url_rep->GetDataStorage()->storage->UsingMemory())
		{
			url.WriteDocumentData(URL::KNormal, UNI_L("<li>loading url: "));
			url.WriteDocumentData(URL::KHTMLify, url_rep->GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI).CStr());
			url.WriteDocumentData(URL::KNormal, UNI_L("</li>\r\n"));
		}
		else if(url_rep->GetDataStorage() && url_rep->GetDataStorage()->storage && url_rep->GetDataStorage()->storage->UsingMemory())
		{
			if((url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_DISK ||
				url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_TEMP) &&
				(url_rep->GetUsedCount() || url_rep->GetRefCount())&&
				url_rep->GetDataStorage()->storage && url_rep->GetDataStorage()->storage->IsPersistent())
			{
			    url.WriteDocumentDataUniSprintf(UNI_L("<li>Ref %d, used %d url: "), url_rep->GetRefCount(), url_rep->GetUsedCount());
				url.WriteDocumentData(URL::KHTMLify, url_rep->GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI).CStr());
				url.WriteDocumentData(URL::KNormal, UNI_L("</li>\r\n"));
			}
		}
		url_rep = url_store->GetNextURL_Rep();
	}
    url.WriteDocumentDataUniSprintf(UNI_L("</ul><h5>Currently referenced or used urls stored in memory</h5>\r\n<ul>"));

	url_rep = url_store->GetFirstURL_Rep();
	while (url_rep)
	{
		if(url_rep->GetRefCount() == 1 && !url_rep->IsFollowed() && url_rep->GetDataStorage() && url_rep->GetDataStorage()->storage && url_rep->GetDataStorage()->storage->UsingMemory())
		{
		    url.WriteDocumentDataUniSprintf(UNI_L("<li>Ref %d, used %d url: %s</li>\r\n"), url_rep->GetRefCount(), url_rep->GetUsedCount(), url_rep->UniName(PASSWORD_SHOW));
		}
		url_rep = url_store->GetNextURL_Rep();
	}
    url.WriteDocumentDataUniSprintf(UNI_L("</ul>"));
}
#endif

#ifdef URL_FILTER
OP_STATUS Context_Manager_Base::SetEmptyDCacheFilter(URLFilter *filter)
{
	empty_dcache_filter=filter;

	CACHE_PROPAGATE_NO_REMOTE_VOID(SetEmptyDCacheFilter(filter));
	
	return OpStatus::OK;
}
#endif

void Context_Manager::ConstructPrefL(int a_cache_size, BOOL load_indexes)
{
	cache_size_pref = a_cache_size;

	InitL();
#ifdef DISK_CACHE_SUPPORT
#ifndef SEARCH_ENGINE_CACHE
	if(load_indexes)
	{
		ReadDCacheFileL();
		ReadVisitedFileL();
	}
#endif
#endif
	PrefsCollectionNetwork::integerpref t_cache_size_pref = (cache_size_pref>= 0 ? 
						(PrefsCollectionNetwork::integerpref) cache_size_pref : 
						PrefsCollectionNetwork::DummyLastIntegerPref);

	if(t_cache_size_pref != PrefsCollectionNetwork::DummyLastIntegerPref)
		SetCacheSize( ((OpFileLength) g_pcnet->GetIntegerPref(t_cache_size_pref)) * 1024); // Get the size from a preference, in KB
	else
	{
		// 5% of the default, but at least 50KB
		OpFileLength base_size = (OpFileLength) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheSize) * 1024;
		base_size /= 20; // 5%
		if(base_size < 50*1024)
			base_size = 50*1024; // 50KB

		SetCacheSize(base_size);
	}
}

void Context_Manager::ConstructSizeL(int a_cache_size, BOOL load_indexes)
{
	cache_size_pref = -1;

	InitL();
#ifdef DISK_CACHE_SUPPORT
#ifndef SEARCH_ENGINE_CACHE
	if(load_indexes)
	{
		ReadDCacheFileL();
		ReadVisitedFileL();
	}
#endif
#endif
	
	/// Size in KB
	SetCacheSize((OpFileLength) (a_cache_size * 1024));
}

Context_Manager * Context_Manager_Base::SetNextChainedManager(Context_Manager *manager)
{
	OP_ASSERT((!manager && next_manager) || (manager && !next_manager));   // It could be removed, it is just for the initial development phase

	Context_Manager *old_manager=next_manager;

	next_manager=manager;

	return old_manager;
}

void Context_Manager::UnloadURL(URL_DataStorage *ds)
{
	OP_ASSERT(ds && ds->url);
	
	ds->url->Unload();
}

URL_Rep *Context_Manager::GetURLRep(URL_DataStorage *ds)
{
	OP_ASSERT(ds && ds->url);

	return ds->url;
}

#ifdef NEED_CACHE_EMPTY_INFO
BOOL Context_Manager::IsCacheEmpty()
{
	URL_Rep* url_rep = url_store->GetFirstURL_Rep();
    while (url_rep)
    {
		if((URLType) url_rep->GetAttribute(URL::KType) == URL_HTTP &&
				!url_rep->GetAttribute(URL::KHavePassword) &&
				!url_rep->GetAttribute(URL::KHaveAuthentication))
		{
			const URL_HTTP_ProtocolData* http_url = (url_rep->storage ? url_rep->storage->http_data : NULL);

			OpFileLength registered_len=0;
			url_rep->GetAttribute(URL::KContentLoaded, FALSE, &registered_len);

			if(http_url && registered_len && 
				(URLCacheType) url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_DISK &&
				(URLStatus) url_rep->GetAttribute(URL::KLoadStatus) != URL_UNLOADED)
			{
				return FALSE; // we have at least one valid cache item
			}
		}
		url_rep = url_store->GetNextURL_Rep();
	}

	CACHE_PROPAGATE_NO_REMOTE_RETURN(IsCacheEmpty(), TRUE);
}
#endif // NEED_CACHE_EMPTY_INFO

#ifdef DISK_CACHE_SUPPORT
#ifdef SELFTEST
BOOL Context_Manager::CheckDoubleURLs()
{
	// Get up to 10 URLs for double check
	URL_Rep* url_rep = url_store->GetFirstURL_Rep();
	URL_Rep* urls_to_check[10];

	int url_total=0;

	while (url_rep && url_total<10)
	{
		if(!CacheTester::IsURLRepIllegal(url_rep))  // Illegal URLs looks duplicated
			urls_to_check[url_total++]=url_rep;
		url_rep = url_store->GetNextURL_Rep();
	}

	// Double check
	for(int i=0; i<url_total; i++)
	{
		URL_Rep* url_main = urls_to_check[i];
		URL_Rep* url_check;
		OpString8 name_main;
		OpString8 name_check;
		int num_matches = 0;  // 1 is the only acceptable value

		url_main->GetAttribute(URL::KName_With_Fragment_Username_Password_Hidden, name_main, URL::KNoRedirect);

		url_check = url_store->GetFirstURL_Rep();

		while (url_check)
		{
			url_check->GetAttribute(URL::KName_With_Fragment_Username_Password_Hidden, name_check, URL::KNoRedirect);

			if(url_check==url_main || !name_check.Compare(name_main))
				num_matches++;

			url_check = url_store->GetNextURL_Rep();
		}

		if(num_matches!=1)
			return FALSE;
	}

	return TRUE;
}
#endif // SELFTEST
#endif // DISK_CACHE_SUPPORT

#ifdef CACHE_PURGE
void Context_Manager::CleanDCacheDirL()
{
	CACHE_PROPAGATE_ALWAYS_VOID(CleanDCacheDirL());

    FileName_Store filenames(1024);
    OP_STATUS op_err = filenames.Construct();
    RAISE_AND_RETURN_VOID_IF_ERROR(op_err);

    if(GetCacheLocationForFiles() != OPFILE_ABSOLUTE_FOLDER)
    {
        ReadDCacheDir(filenames, filenames, GetCacheLocationForFiles(), TRUE, FALSE, NULL);
        DeleteFiles(filenames);
        urlManager->DeleteFilesInDeleteListContext(context_id);
    }
}
#endif // CACHE_PURGE

#ifdef SELFTEST
OP_STATUS Context_Manager::GetDataDescriptorsListForStats(OpVector<URLDD_Stats> &vector)
{
	// Bad casting, but it should actually be safe as OpVector extends OpGenericVector without adding fields
	return dd_stats.CopyAllToVector(* ((OpGenericVector *)&vector));
}

OP_STATUS Context_Manager::AddDataDescriptorForStats(URL_DataDescriptor *dd)
{
	dd_total_num++; // The data descriptor has been created even if we experience an OOM in the next lines

	URLDD_Stats *stats=OP_NEW(URLDD_Stats, (dd));

	if(!stats)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ops=dd_stats.Add(dd, stats);

	if(OpStatus::IsError(ops))
		OP_DELETE(stats);

	if((unsigned long) dd_stats.GetCount() > dd_peak_num)
		dd_peak_num = dd_stats.GetCount();

	return ops;
}

URLDD_Stats *Context_Manager::GetDataDescriptorForStats(URL_DataDescriptor *dd)
{
	void *stats=NULL;

	OP_STATUS ops=dd_stats.GetData(dd, &stats);

	if(OpStatus::IsError(ops))
		return NULL;

	return (URLDD_Stats *)stats;
}

BOOL Context_Manager::RemoveDataDescriptorForStats(URL_DataDescriptor *dd)
{
	void *stats=NULL;

	OP_STATUS ops=dd_stats.Remove(dd, &stats);

	if(OpStatus::IsError(ops))
		return FALSE;

	OP_DELETE((URLDD_Stats *)stats);

	if(dd->GetBuffer() && dd->GetBufMaxLength()>0)
		dd_memory += dd->GetBufMaxLength();

	return TRUE;
}

unsigned long Context_Manager::GetTotalMemoryAllocated(BOOL add_descriptors_alive, BOOL add_descriptors_deleted)
{
	OP_ASSERT(add_descriptors_deleted || add_descriptors_alive); // Else it's pretty useless...

	unsigned long tot = 0;

	if(add_descriptors_deleted)
		tot += dd_memory;

	if(add_descriptors_alive)
	{
		OpVector<URLDD_Stats> dd_stats;

		if(OpStatus::IsSuccess(GetDataDescriptorsListForStats(dd_stats)))
		{
			for(int i=0, len=dd_stats.GetCount(); i<len; i++)
			{
				URLDD_Stats *stats = dd_stats.Get(i);
				URL_DataDescriptor *dd = (stats) ? stats->GetDescriptor() : NULL;

				if(dd)
					tot+=dd->GetBufMaxLength();
			}
		}
	}

	return tot;
}

void URLDD_Stats::AddBytesMemMoved(unsigned long bytes)
{
	bytes_memmoved+=bytes;
	
	Context_Manager *mng=urlManager->FindContextManager(dd->GetURL().GetContextId());

	if(mng)
		mng->StatsAddBytesMemMoved(bytes);
}

void Context_Manager::VerifyCacheSize()
{
	if(url_store && url_store->URL_RepCount()==0 && LRU_list.Empty())
	{
		OP_ASSERT(size_disk.GetUsed()==0);
		OP_ASSERT(size_ram.GetUsed()==0);
	}

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	if(context_id!=0)
		OP_ASSERT(size_oem.GetUsed()==0); // No OEM cache computed in other contexts
#endif // defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
}
#endif // SELFTEST

#if ( defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE) ) && !defined(SEARCH_ENGINE_CACHE)
#ifdef URL_ENABLE_ASSOCIATED_FILES
OP_STATUS Context_Manager::CheckAssociatedFilesList(FileName_Store &filenames, FileName_Store &associated, FileName_Store &result, BOOL choose_present)
{
#ifdef DEBUG_ENABLE_OPASSERT
	unsigned long n=filenames.LinkObjectCount();
	unsigned long tot=associated.LinkObjectCount() + result.LinkObjectCount();
#endif

	FileNameElement* assoc_name = associated.GetFirstFileName();
	FileNameElement* next_name = NULL;

	while(assoc_name)
	{
		next_name = associated.GetNextFileName();

		// Filenames are like: "assoc000/g_0001/opr00001.000"
		int first_slash=assoc_name->FileName().FindFirstOf(PATHSEPCHAR);
		int last_dot=assoc_name->FileName().FindLastOf('.');

		OP_ASSERT(first_slash>=5 && last_dot>first_slash);

		if(first_slash>0 && last_dot>first_slash)
		{
			// Original cache name
			OpString cache_name;

			// Intended name like: "g_0001/opr00001.tmp"
			RETURN_IF_ERROR(cache_name.Set(assoc_name->FileName().CStr() + first_slash+1, last_dot-first_slash));
			RETURN_IF_ERROR(cache_name.AppendFormat(UNI_L("tmp")));

			FileNameElement* fn = filenames.RetrieveFileName(cache_name.CStr(), cache_name.CStr());

			if( (fn && choose_present) || 
				(!fn && !choose_present))
			{
				associated.RemoveFileName(assoc_name);
				result.AddFileName(assoc_name);
			}
		}

		assoc_name = next_name;
	}

#ifdef DEBUG_ENABLE_OPASSERT
	OP_ASSERT(n == filenames.LinkObjectCount()); // No changes in filenames
	OP_ASSERT(tot == associated.LinkObjectCount() + result.LinkObjectCount());  // Objects moved from "associated" to "result"
#endif

	return OpStatus::OK;
}
#endif // URL_ENABLE_ASSOCIATED_FILES
#endif  // ( defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE) ) && !defined(SEARCH_ENGINE_CACHE)

#ifdef URL_ENABLE_ASSOCIATED_FILES
void Context_Manager::GetTempAssociatedTicketName(URL_Rep *rep, URL::AssociatedFileType type, OpString &ticket)
{
	if(!rep)
		return;

	URL_ID id=rep->GetID();
	
	while(id)
	{
		ticket.AppendFormat(UNI_L("%.04X"), (int)(id & 0xFFFF));
		id>>=16;
	}

	ticket.AppendFormat(UNI_L(".%d"), (int)type);
}

OpString *Context_Manager::GetTempAssociatedFileName(URL_Rep *rep, URL::AssociatedFileType type)
{
	OP_ASSERT(rep);

	if(!rep)
		return NULL;

	OpString ticket;

	GetTempAssociatedTicketName(rep, type, ticket);

	if(ticket.IsEmpty())
		return NULL;

	TempAssociateFileTicket *ft;
	OP_STATUS ops=ht_temp_assoc_files.GetData(ticket.CStr(), &ft);

	if(OpStatus::IsError(ops))
		return NULL;

	return &(ft->file_name);
}

OP_STATUS Context_Manager::CreateTempAssociatedFileName(URL_Rep *rep, URL::AssociatedFileType type, OpString *&file_name, BOOL allow_duplicated)
{
	file_name=NULL;

	OpFileFolder folder=GetCacheLocationForFiles();

	if(folder==OPFILE_ABSOLUTE_FOLDER || !rep) // We don't want to spread files around the disk...
		return OpStatus::ERR_NOT_SUPPORTED;

	URLType url_type=(URLType) rep->GetAttribute(URL::KType);

	// HTTPS thumbnails could contain sensitive data, so they cannot be saved on disk
	if(url_type==URL_HTTPS && type==URL::Thumbnail)
		return OpStatus::ERR_NOT_SUPPORTED;

	OpString *t=GetTempAssociatedFileName(rep, type);

	if(t)
	{
		if(allow_duplicated) // Duplicated OK
		{
			file_name = t;

			return OpStatus::OK;
		}

		return OpStatus::ERR; // Duplicated NOT OK
	}

	TempAssociateFileTicket *ticket = OP_NEW(TempAssociateFileTicket, ());

	if(!ticket)
		return OpStatus::ERR_NO_MEMORY;

	GetTempAssociatedTicketName(rep, type, ticket->ticket);
	
	if(ticket->ticket.IsEmpty())
	{
		OP_DELETE(ticket);

		return OpStatus::ERR; // basically rep is NULL...;
	}

	UINT32 file_name_number=++assoc_file_counter; // Get a temp name

	// Identify the bit
	int bit = 0;
	while ((type & 1) == 0)
	{
		++bit;
		type = (URL::AssociatedFileType)((UINT32)type >> 1);
	}

	OP_STATUS ops=ticket->file_name.AppendFormat(UNI_L("assot%.03X%copr%.05X.tmp"), bit, PATHSEPCHAR, file_name_number);

	if(OpStatus::IsSuccess(ops))
		ops=ht_temp_assoc_files.Add(ticket->ticket.CStr(), ticket);

	if(OpStatus::IsError(ops))
	{
		OP_DELETE(ticket);
		file_name=NULL;

		return ops;
	}

	file_name=&(ticket->file_name);

	return OpStatus::OK;
}
#endif // URL_ENABLE_ASSOCIATED_FILES
