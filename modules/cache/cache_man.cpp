/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/cache/cache_man.h"
#include "modules/cache/cache_ctxman_disk.h"
#include "modules/cache/url_cs.h"
#include "modules/cache/cache_int.h"
#include "modules/cache/cache_ctxman_disk.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_man.h"
#include "modules/cache/url_stor.h"
#include "modules/url/url_ds.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/dochand/winman.h"
#include "modules/pi/OpSystemInfo.h"

// Call the Context_Manager with the correct context_id
#define CONTAINERS_CONTEXT_MACRO(METHOD_CALL, ERR_NULL_STORAGE, ERR_NO_RESOURCE)	\
	OP_ASSERT(storage);										\
	if(!storage) return ERR_NULL_STORAGE;					\
	Context_Manager *cman = FindContextManager(storage->Url()->GetContextId());	\
															\
	if(cman)												\
		return cman->METHOD_CALL;							\
															\
	OP_ASSERT(FALSE);										\
	return ERR_NO_RESOURCE;

Cache_Manager::Cache_Manager()
{
#ifdef SELFTEST
	emulateOutOfDisk = FALSE;
#endif //SELFTEST

#if CACHE_SMALL_FILES_SIZE>0
	embedded_size = 0;
	embedded_files = 0;
#endif

#ifdef SELFTEST
	debug_simulate_remove_context_bug = FALSE;
	debug_accept_overlapping_contexts = FALSE;
	debug_oom_on_context_creation = FALSE;
#endif // SELFTEST

	ctx_main = NULL;
	updated_cache = FALSE;
	exiting = FALSE;
}

void Cache_Manager::MakeUnique(URL_Rep *url)
{
	if(!url || url->GetAttribute(URL::KUnique))
		return;

	CACHE_CTX_FIND_ID_VOID(url->GetContextId(), MakeUnique(url));
}

Context_Manager *Cache_Manager::FindContextManager(URL_CONTEXT_ID id)
{
	if(GetMainContext() && GetMainContext()->context_id==id)
		return ctx_main;

	// Check normal contexts
	Context_Manager *item = (Context_Manager *) context_list.First();

	while(item)
	{
		if(item->context_id == id)
			return item;

		item = (Context_Manager *) item->Suc();
	}

	// Check frozen contexts
	item = (Context_Manager *) frozen_contexts.First();

	while(item)
	{
		if(item->context_id == id)
			return item;

		item = (Context_Manager *) item->Suc();
	}

	return NULL;
}


URL_Rep *Cache_Manager::LocateURL(URL_ID id)
{
	CACHE_CTX_WHILE_BEGIN_REF
		URL_Rep *rep = manager->LocateURL(id);
		if(rep)
		{
			manager->DecReferencesCount();

			return rep;
		}
	CACHE_CTX_WHILE_END_REF
	
	OP_ASSERT(FALSE);
	
	return NULL;
}

void Cache_Manager::CacheCleanUp(BOOL ignore_downloads)
{
	CACHE_CTX_CALL_ALL(CacheCleanUp(ignore_downloads));
}

BOOL Cache_Manager::FindContextIdByParentURL(URL &parentURL, URL_CONTEXT_ID &id)
{
	CACHE_CTX_WHILE_BEGIN_REF		
		if (manager->parentURL.Id() == parentURL.Id())
		{
			id = manager->context_id;
			return TRUE;
		}	
	CACHE_CTX_WHILE_END_REF	
	return FALSE;
}

BOOL Cache_Manager::FreeUnusedResources(BOOL all SELFTEST_PARAM_COMMA_BEFORE(DebugFreeUnusedResources *dfur)) // return TRUE if used more than 100 ms
{
	double start = g_op_time_info->GetRuntimeMS();
	double current = start;
	
	CACHE_CTX_WHILE_BEGIN_REF_FROZEN
		manager->FreeUnusedResources(all SELFTEST_PARAM_COMMA_BEFORE(dfur));

		manager->DecReferencesCount();
	
		if(manager->GetReferencesCount() == 0)
		{
			manager->Out();
			OP_DELETE(manager);
		}

		if(!all)
		{
			current = g_op_time_info->GetRuntimeMS();
			if (current - start > 100.0)
				return TRUE;
		}
	CACHE_CTX_WHILE_END_NOREF_FROZEN
	
	return FALSE;
}

void Cache_Manager::AutoSaveCacheL()
{
	CACHE_CTX_CALL_ALL(AutoSaveCacheL());
}

void Cache_Manager::PreDestructStep(BOOL from_predestruct)
{
	exiting = TRUE;

	// We call the PreDestruct() step to avoid situations where the context are not reacheable
	// (because the item is removed from the list before deleting it, by the destructor )
	CACHE_CTX_WHILE_BEGIN_REF_FROZEN
		manager->PreDestructStep(TRUE);
		manager->DecReferencesCount();
		manager->Out();
		manager->SignalCacheSynchronized(TRUE);
		if(manager==ctx_main)
			ctx_main=NULL; // Just a precaution
		OP_DELETE(manager);
	CACHE_CTX_WHILE_END_NOREF_FROZEN

	exiting = FALSE; // Just a precaution
}

void Cache_Manager::RemoveSensitiveCacheData()
{
	CACHE_CTX_CALL_ALL(RemoveSensitiveCacheData());

#ifdef _NATIVE_SSL_SUPPORT_
	EmptyDCache(g_revocation_context);
#endif
}

void Cache_Manager::PrefChanged(int pref, int newvalue)
{
#ifndef CACHE_SINGLE_CACHE_LIMIT
	if (pref == PrefsCollectionNetwork::DiskCacheSize)
	{
	// This function could support a faster version of the macro, without references and temporary variable
	CACHE_CTX_WHILE_BEGIN_REF
		OpFileLength base_size = newvalue;

		if(manager==ctx_main)
			manager->SetCacheSize(base_size * 1024);
		else if(manager->cache_size_pref<0)
		{
			base_size /= 20; // 5%
			if(base_size < 50)
				base_size = 50; // 50KB
			manager->SetCacheSize(base_size * 1024);
		}
	CACHE_CTX_WHILE_END_REF
	}
#endif
#ifndef RAMCACHE_ONLY
	if (pref == PrefsCollectionNetwork::CacheToDisk && newvalue == 0)
	{
	CACHE_CTX_WHILE_BEGIN_REF
		manager->size_ram.SetSize((OpFileLength) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheSize) * 1024);
		manager->size_disk.SetSize(static_cast<OpFileLength>(-1));
	CACHE_CTX_WHILE_END_REF
	}
	else if (pref == PrefsCollectionNetwork::CacheToDisk && newvalue == 1)
	{
	CACHE_CTX_WHILE_BEGIN_REF
		manager->size_disk.SetSize( (OpFileLength) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheSize) * 1024);
		manager->size_ram.SetSize((OpFileLength ) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheBufferSize) * 1024);
	CACHE_CTX_WHILE_END_REF
	}
#endif
}

#ifdef NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS
void Cache_Manager::RestartAllConnections()
{
	// LMTODO2: check implementation!
# ifdef DYNAMIC_PROXY_UPDATE
	// Restart all connections after a proxy change
	urlManager->SetPauseStartLoading(TRUE);
	
	CACHE_CTX_CALL_ALL(RestartAllConnections());
	
	urlManager->SetPauseStartLoading(FALSE);
# endif // DYNAMIC_PROXY_UPDATE
}

void Cache_Manager::StopAllLoadingURLs()
{
	urlManager->SetPauseStartLoading(TRUE);

	CACHE_CTX_CALL_ALL(StopAllLoadingURLs());

	urlManager->SetPauseStartLoading(FALSE);
}

#endif // NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS

#ifdef _OPERA_DEBUG_DOC_
void Cache_Manager::WriteDebugInfo(URL &url)
{
	CACHE_CTX_FIND_ID_VOID(url.GetContextId(), WriteDebugInfo(url));
}
#endif

void Cache_Manager::AddURL_Rep(URL_Rep *new_rep)
{
	OP_ASSERT(new_rep);
	
	if(!new_rep)
		return;

	CACHE_CTX_FIND_ID_VOID(new_rep->GetContextId(), AddURL_Rep(new_rep));	
}

URL_Rep * Cache_Manager::GetResolvedURL(URL_Name_Components_st &resolved_name, URL_CONTEXT_ID context_id)
{
	CACHE_CTX_FIND_ID_RETURN(context_id, GetResolvedURL(resolved_name), NULL);
}


void Cache_Manager::AddContextL(URL_CONTEXT_ID id, OpFileFolder vlink_loc,
								OpFileFolder cache_loc, int cache_size_pref, URL &parentURL)
{
#ifdef DISK_CACHE_SUPPORT
	OpString full_path;
	ANCHOR(OpString, full_path);

	g_folder_manager->GetFolderPath(cache_loc, full_path);

	OP_NEW_DBG("Cache_Manager::AddContextL()", "cache.ctx");
	OP_DBG((UNI_L("*** Context %d mapped to %d: %s"), (int)id, (int)cache_loc, full_path.CStr()));
#endif

#if defined(SELFTEST) && defined(DISK_CACHE_SUPPORT)
	if(!debug_accept_overlapping_contexts && cache_loc!=OPFILE_ABSOLUTE_FOLDER)
	{   /// Check if there are contexts already using the same directory
		CACHE_CTX_WHILE_BEGIN_REF
			BOOL overlap=manager->cache_loc==cache_loc || manager->vlink_loc==vlink_loc;

			OP_ASSERT(!overlap);
			if(overlap)
				break;  // No point in checking more
		CACHE_CTX_WHILE_END_REF
	}
	debug_accept_overlapping_contexts = FALSE; // Bypass lasts for one context
#endif // SELFTEST && DISK_CACHE_SUPPORT

	Context_Manager *ctx = FindContextManager(id);
	if(ctx)
		LEAVE(OpStatus::ERR_NOT_SUPPORTED);   // We no longer support this situation. context_id must be unique

	Context_Manager * OP_MEMORY_VAR cm = NULL;

#ifdef DISK_CACHE_SUPPORT
	cm=OP_NEW_L(Context_Manager_Disk, (id, vlink_loc, cache_loc));
#else
	cm=OP_NEW_L(Context_Manager_Disk, (id));
#endif

#ifdef SELFTEST
	if(debug_oom_on_context_creation)
	{
		OP_DELETE(cm);
		cm = NULL;
	}
#endif // SELFTEST

	if(!cm)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	cm->parentURL = parentURL;
	OpStackAutoPtr<Context_Manager> item(cm);
	
	item->Into(&context_list);
	TRAPD(op_err, item->ConstructPrefL(cache_size_pref, TRUE));

	if(OpStatus::IsError(op_err))
	{
		item->Out();
		item.reset();
		LEAVE(op_err);
	}
	else
		cm->SignalCacheActivity();
	
	item.release();
}

OP_STATUS Cache_Manager::AddContextManager(Context_Manager *ctx)
{
	OP_ASSERT(ctx);

	if(!ctx)
		return OpStatus::ERR_NULL_POINTER;

	Context_Manager *old = FindContextManager(ctx->context_id);
	
	OP_ASSERT(!old);
	
	if(old)
		return OpStatus::ERR_NOT_SUPPORTED;

	ctx->Into(&context_list);

	return OpStatus::OK;
}

void Cache_Manager::RemoveContext(URL_CONTEXT_ID id, BOOL empty_context)
{
	OP_ASSERT(id);

	// The main context cannot be removed, because we don't support freezing it.
	// This is not extected to be a problem, as Opera does not remove it.
	// If it is rquired in the future, the CACHE_CTX_... macros and some other functions
	// need to be changed to support ctx_main in the frozen list, plus CacheTester::FreezeTests()
	// needs also to be updated.
	if(!id)
		return;

	Context_Manager * OP_MEMORY_VAR ctx = FindContextManager(id);

	if(ctx)
	{
		OP_NEW_DBG("Cache_Manager::RemoveContext()", "cache.ctx");
		OP_DBG((UNI_L("*** Removing Context %d - Files deleted: %s"), (int)id, empty_context ? UNI_L("yes") : UNI_L("no") ));

		// Remove context should absolutely be called only once
		// We will still try to free resources, but this is definitely a bug
		OP_ASSERT(!IsContextManagerFrozen(ctx) && "Multiple calls to RemoveContext()");

		ctx->FreeUnusedResources(TRUE);
		
		ctx->CacheCleanUp(FALSE);
#ifndef RAMCACHE_ONLY
		BOOL destroy_url=TRUE;
		
	#ifdef SELFTEST
		if(urlManager->Debug_GetSimulateRemoveContextBug())
			destroy_url=FALSE;
	#endif // SELFTEST
	
		if(empty_context)
			OpStatus::Ignore(ctx->DeleteCacheIndexes());
		else				// Not Empty ==> Index saved
		{
			TRAPD(op_err2, ctx->WriteCacheIndexesL(TRUE, destroy_url));
	
			OpStatus::Ignore(op_err2);
		}

		ctx->cache_written_before_shutdown = TRUE;
#endif
#ifdef WEB_TURBO_MODE
		if(!g_windowManager || g_windowManager->GetTurboModeContextId() != id)
#endif // WEB_TURBO_MODE
		{
			if(empty_context && !IsContextManagerFrozen(ctx))
				ctx->EmptyDCache(TRUE);
		}
		
		OP_ASSERT(ctx->url_store);
		
	#ifdef SELFTEST
		if(urlManager->Debug_GetSimulateRemoveContextBug())
			ctx->DebugSetReferencesCount(1);
	#endif // SELFTEST
		ctx->MakeAllURLsUnique();

		OP_DBG((UNI_L("*** Context %d removed"), (int)id ));
	}

	BOOL deleted=DecrementContextReference(id, TRUE);

	/// As something prevented the context from being removed, we need to freeze it
	/// While exiting, we should avoid freezing anyway, also because it would have effects on the list processed by PreDestructStep()
	if(ctx && !deleted && !IsContextManagerFrozen(ctx) && !exiting)
		FreezeContextManager(ctx);
}

BOOL Cache_Manager::ContextExists(URL_CONTEXT_ID id)
{
	return (FindContextManager(id) != NULL ? TRUE : FALSE);
}

void Cache_Manager::IncrementContextReference(URL_CONTEXT_ID a_context_id)
{
	Context_Manager *ctx = FindContextManager(a_context_id);

	if(ctx)
		ctx->IncReferencesCount();
}

BOOL Cache_Manager::DecrementContextReference(URL_CONTEXT_ID a_context_id, BOOL enable_context_delete)
{
	Context_Manager *ctx = FindContextManager(a_context_id);

	if(ctx && !exiting) // Avoid to delete the context if Opera is shutting down, as this will be performed in a different way. Basically it prevents a race condition (CORE-33012)
	{
		OP_ASSERT(ctx->GetReferencesCount());

		if(ctx->GetReferencesCount())
			ctx->DecReferencesCount();

		// The context is now delete only when the reference count reach 0 and enable_context_delete is set tot TRUE.
		// This prevents normal operations involving references to delete the context (possibly because of an unbalanced call) in a way
		// that can crash Opera.
		// The only method that is supposed to call this function with enable_context_delete set to TRUE is RemoveContext().
		// Please Note that FreeUnusedResources() can delete the context as well, though not calling this method.
		if(!ctx->GetReferencesCount() && enable_context_delete)
		{
			ctx->IncReferencesCount();
			ctx->PreDestructStep(FALSE);
			ctx->DecReferencesCount();
			ctx->Out();
			ctx->SignalCacheSynchronized(TRUE);
			OP_DELETE(ctx);

			return TRUE;
		}
	}

	return FALSE;
}

void Cache_Manager::CheckRamCacheSize(OpFileLength force_size)
{
	CACHE_CTX_CALL_ALL(CheckRamCacheSize(force_size));
}

OpFileLength Cache_Manager::GetRamCacheSize()
{
	OpFileLength res = 0;
	CACHE_CTX_WHILE_BEGIN_REF
		res += manager->GetRamCacheUsed();
	CACHE_CTX_WHILE_END_REF
	return res;
}

#if defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)
OP_STATUS
Cache_Manager::AddFilesToDeleteList(URL_CONTEXT_ID ctx_id, FileName_Store& filenames)
{
	Delete_List *ctx_cur_del_list=delete_file_list.First();
	Delete_List *ctx_del_list=NULL;

	while(ctx_cur_del_list && !ctx_del_list)
	{
		if(ctx_cur_del_list->GetContextID()==ctx_id)
			ctx_del_list=ctx_cur_del_list;
		ctx_cur_del_list=ctx_cur_del_list->Suc();
	}

	if(!ctx_del_list)
	{
		ctx_del_list=OP_NEW(Delete_List, (ctx_id));

		RETURN_OOM_IF_NULL(ctx_del_list);

		ctx_del_list->Into(&delete_file_list);
	}

	int added=0;

	OP_NEW_DBG("Cache_Manager::DeleteFiles()", "cache.del");
	OP_DBG((UNI_L("*** Delete List start")));

	for (FileNameElement* name = filenames.GetFirstFileName(); name; name = filenames.GetNextFileName())
	{
		OP_DBG((UNI_L("*** File: %s [%d]"), name->FileName().CStr(), name->Folder()));
		filenames.RemoveFileName(name);
		ctx_del_list->AddFileName(name);
		added++;
	}

	OP_DBG((UNI_L("*** Delete List end")));

	if(added || !AllFilesDeleted() /*&& !g_main_message_handler */)
	{
		g_main_message_handler->RemoveDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES, 0, 0); // Make sure there is only one message
		g_main_message_handler->PostDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES, 0, 0, 1000); // every second
	}

	return OpStatus::OK;
}

void
Cache_Manager::DeleteFilesInDeleteList(BOOL all)
{
	UINT32 files_to_del = CACHE_NUM_FILES_PER_DELETELIST_PASS;
	OP_NEW_DBG("Cache_Manager::DeleteFilesInDeleteList()", "cache.del");

	Delete_List *ctx_cur_del_list=delete_file_list.First();
	Delete_List *next_ctx_cur_del_list=NULL;

	while((files_to_del>0 || all) && ctx_cur_del_list)
	{
		next_ctx_cur_del_list=ctx_cur_del_list->Suc();
		files_to_del-=ctx_cur_del_list->DeleteFiles(files_to_del);
		
		if(!ctx_cur_del_list->HasFilesInList())
		{
			ctx_cur_del_list->Out();
			OP_DELETE(ctx_cur_del_list);
		}

		ctx_cur_del_list=next_ctx_cur_del_list;
	}

	if (!AllFilesDeleted())
	{
		OP_ASSERT(g_main_message_handler);
		g_main_message_handler->RemoveDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES, 0, 0); // Make sure there is only one message
		g_main_message_handler->PostDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES, 0, 0, 1000); // every second
	}
}

void
Cache_Manager::DeleteFilesInDeleteListContext(URL_CONTEXT_ID ctx_id)
{
	OP_NEW_DBG("Cache_Manager::DeleteFilesInDeleteListContext()", "cache.del");
	Delete_List *ctx_cur_del_list=delete_file_list.First();
	Delete_List *next_ctx_cur_del_list=NULL;

	while(ctx_cur_del_list)
	{
		next_ctx_cur_del_list=ctx_cur_del_list->Suc();

		if(ctx_cur_del_list->GetContextID()==ctx_id)
		{
			ctx_cur_del_list->DeleteFiles(1024*1024);
		
			OP_ASSERT(!ctx_cur_del_list->HasFilesInList());

			if(!ctx_cur_del_list->HasFilesInList())
			{
				ctx_cur_del_list->Out();
				OP_DELETE(ctx_cur_del_list);
			}

			break;
		}

		ctx_cur_del_list=next_ctx_cur_del_list;
	}
}

UINT32 Delete_List::DeleteFiles(UINT32 num)
{
	FileNameElement* name = NULL;
	UINT32 i=0;
	OP_NEW_DBG("Delete_List::DeleteFiles()", "cache.del");

	OpFile file;

	while(i < num && (name = file_list.First()) != NULL)
	{
		OpFile file;

		i++;

		if (OpStatus::IsSuccess(file.Construct(name->FileName().CStr(), name->Folder())))
		{
			OP_DBG((UNI_L("*** Cache File %s [%d] deleted because in list"), name->FileName().CStr(), name->Folder()));
			file.Delete(FALSE);
		}
		name->Out();
		OP_DELETE(name);
	}

	OP_ASSERT(i<=num); // Else we deleted too many files... and we caused an overflow...

	return i;
}

void Delete_List::AddFileName(FileNameElement *name)
{
	if(name)
		name->Into(&file_list);
}

BOOL Cache_Manager::AllFilesDeletedInContext(URL_CONTEXT_ID ctx_id)
{
	Delete_List *ctx_cur_del_list=delete_file_list.First();

	while(ctx_cur_del_list)
	{
		if(ctx_cur_del_list->GetContextID()==ctx_id && ctx_cur_del_list->HasFilesInList())
			return FALSE;

		ctx_cur_del_list=ctx_cur_del_list->Suc();
	}

	return TRUE;
}

BOOL Cache_Manager::AllFilesDeleted()
{
	Delete_List *ctx_cur_del_list=delete_file_list.First();

	while(ctx_cur_del_list)
	{
		if(ctx_cur_del_list->HasFilesInList())
			return FALSE;

		ctx_cur_del_list=ctx_cur_del_list->Suc();
	}

	return TRUE;
}

UINT Cache_Manager::GetNumFilesToDelete()
{
	Delete_List *ctx_cur_del_list=delete_file_list.First();
	UINT num = 0;

	while(ctx_cur_del_list)
	{
		num += ctx_cur_del_list->GetCount();

		ctx_cur_del_list=ctx_cur_del_list->Suc();
	}

	return num;
}
#endif // defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)

#ifdef DISK_CACHE_SUPPORT
#ifndef SEARCH_ENGINE_CACHE	
void Cache_Manager::WriteCacheIndexesL(BOOL sort_and_limit, BOOL destroy_urls, BOOL write_contexts)
{
	if(write_contexts)
	{
		CACHE_CTX_CALL_ALL(WriteCacheIndexesL(sort_and_limit, destroy_urls));
	}
	else if(GetMainContext())
		GetMainContext()->WriteCacheIndexesL(sort_and_limit, destroy_urls);
}

void Cache_Manager::ReadVisitedFileL()
{
	CACHE_CTX_CALL_ALL(ReadVisitedFileL());
}

void Cache_Manager::ReadDCacheFileL()
{
	CACHE_CTX_CALL_ALL(ReadDCacheFileL());
}
#endif // SEARCH_ENGINE_CACHE
#endif // DISK_CACHE_SUPPORT

#ifdef APPLICATION_CACHE_SUPPORT
OP_STATUS Cache_Manager::CopyUrlToContext(URL_Rep *url_rep, URL_CONTEXT_ID copy_to_context)
{
	if (url_rep->GetAttribute(URL::KLoadStatus, URL::KNoRedirect) != URL_LOADED)
	{
		return OpStatus::ERR;
	}

	UINT32 http_status = url_rep->GetAttribute(URL::KHTTP_Response_Code, FALSE);
	if (http_status == 410 || http_status == 404 || http_status == 0)
		return OpStatus::OK;

	Context_Manager *context = FindContextManager(copy_to_context);

	if (!context)
		return OpStatus::ERR;

	URL_Store* ctx_url_store = context->url_store;

	/* Don't copy if an url with the same name is already there */
	if (ctx_url_store->GetLinkObject(url_rep->LinkId()))
		return OpStatus::OK;

	url_rep->DumpSourceToDisk(TRUE);

	DiskCacheEntry cache_entry;

	cache_entry.Reset();
	cache_entry.SetTag(TAG_CACHE_APPLICATION_CACHE_ENTRY);
	RETURN_IF_LEAVE(url_rep->WriteCacheDataL(&cache_entry));


	FileName_Store filenames(0);
	RETURN_IF_ERROR(filenames.Construct());
	OpFileFolder folder = OPFILE_TEMP_FOLDER; /* Should be ignored */

	URL_Rep *copy_rep = NULL;
	cache_entry.SetTag(TAG_CACHE_APPLICATION_CACHE_ENTRY);
	cache_entry.SetDataAvailable(TRUE);
	RETURN_IF_LEAVE(URL_Rep::CreateL(&copy_rep, &cache_entry, filenames, folder, copy_to_context));
	OpAutoPtr<URL_Rep> copy_rep_container(copy_rep);

	OpAutoPtr<URL_DataDescriptor>  descriptor(url_rep->GetDescriptor(NULL, URL::KNoRedirect, TRUE, FALSE, NULL, URL_UNDETERMINED_CONTENT, 0, TRUE));

	if (!descriptor.get() || OpStatus::IsError(copy_rep->WriteDocumentData(URL::KNormal, descriptor.get())))
	{
		copy_rep->Unload();
		return OpStatus::ERR;
	}

	OP_ASSERT(!url_rep->GetAttribute(URL::KMIME_CharSet).Compare(copy_rep->GetAttribute(URL::KMIME_CharSet)));
	OP_ASSERT(!url_rep->GetAttribute(URL::KHTTPEncoding).Compare(copy_rep->GetAttribute(URL::KHTTPEncoding)));
	OP_ASSERT(!url_rep->GetAttribute(URL::KMIME_ForceContentType).Compare(copy_rep->GetAttribute(URL::KMIME_ForceContentType)));

	OP_ASSERT(url_rep->GetAttribute(URL::KContentType) == copy_rep->GetAttribute(URL::KContentType));

	URL_DataStorage *data_storage = copy_rep->GetDataStorage();
	Cache_Storage *cache_storage = data_storage ? data_storage->GetCacheStorage() : NULL;
	OP_ASSERT(cache_storage);

	// Todo: We set to plain file. This should not be needed, but the resource
	// tend to not be stored in the index if we don't ( if the resource is
	// small enough to stay in ram-cache).
	// The reason seem to be that the DumpSourceToDisk doesn't write it to disk,
	// when plain file is FALSE.
	// If the resource is not written to disk right away, there seem to be a bug
	// that causes it not to be written into the index either.
	if (cache_storage)
		cache_storage->SetPlainFile(TRUE);

	copy_rep->WriteDocumentDataFinished();
	copy_rep->DumpSourceToDisk(TRUE);

	ctx_url_store->AddURL_Rep(copy_rep_container.release());

	OP_ASSERT(ctx_url_store->GetLinkObject(url_rep->LinkId()));

	OP_ASSERT(copy_rep->GetContextId() == copy_to_context);

	return OpStatus::OK;
}
#endif // APPLICATION_CACHE_SUPPORT


void Cache_Manager::SetLRU_Item(URL_DataStorage *url_ds)
{
	if(url_ds == NULL)
		return;

	CACHE_CTX_FIND_ID_VOID(url_ds->url->GetContextId(), SetLRU_Item(url_ds));
}

void Cache_Manager::RemoveLRU_Item(URL_DataStorage *url_ds)
{
	if(!url_ds || !url_ds->InList())
		return;

	CACHE_CTX_FIND_ID_VOID(url_ds->url->GetContextId(), RemoveLRU_Item(url_ds));
}

void Cache_Manager::SetLRU_Item(URL_Rep *url)
{
	if(url && url->GetDataStorage())
		SetLRU_Item(url->GetDataStorage());
}

#if !defined(NO_URL_OPERA) 
OP_STATUS Cache_Manager::GenerateCacheList(URL& url)
{
	Context_Manager *man=FindContextManager(url.GetContextId());

	if(!man)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	clw.SetContextManager(man);

	return clw.GenerateCacheList(url);
}
#endif

void Cache_Manager::RemoveMessageHandler(MessageHandler* mh)
{
	CACHE_CTX_CALL_ALL(RemoveMessageHandler(mh));
}

void Cache_Manager::RemoveFromStorage(URL_Rep *url)
{
	CACHE_CTX_FIND_ID_VOID_REF(url->GetContextId(), RemoveFromStorage(url));
}

void Cache_Manager::DeleteVisitedLinks()
{
	CACHE_CTX_CALL_ALL(DeleteVisitedLinks());
}

BOOL Cache_Manager::GetContextIsRAM_Only(URL_CONTEXT_ID id)
{
	CACHE_CTX_FIND_ID_RETURN(id, GetIsRAM_Only(), FALSE);
}

void Cache_Manager::SetContextIsRAM_Only(URL_CONTEXT_ID id, BOOL ram_only)
{
	CACHE_CTX_FIND_ID_VOID(id, SetIsRAM_Only(ram_only));
}

#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
BOOL Cache_Manager::GetContextIsOffline(URL_CONTEXT_ID id)
{
	CACHE_CTX_FIND_ID_RETURN(id, GetIsOffline(), FALSE);
}

void Cache_Manager::SetContextIsOffline(URL_CONTEXT_ID id, BOOL is_offline)
{
	CACHE_CTX_FIND_ID_VOID(id, SetIsOffline(is_offline));
}
#endif // MHTML_ARCHIVE_REDIRECT_SUPPORT

#ifdef URL_FILTER
OP_STATUS Cache_Manager::SetEmptyDCacheFilter(URLFilter *filter, URL_CONTEXT_ID ctx_id)
{
	CACHE_CTX_FIND_ID_RETURN(ctx_id, SetEmptyDCacheFilter(filter), OpStatus::ERR_NO_SUCH_RESOURCE);
}
#endif

	OP_STATUS Cache_Manager::GetNewFileNameCopy(OpStringS &name, const uni_char* ext, OpFileFolder &folder, BOOL session_only
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
					,BOOL useOperatorCache
#endif
	, URL_CONTEXT_ID context_id)
{
	Context_Manager *ctx = FindContextManager(context_id);

	OP_ASSERT(ctx);
	
	if(!ctx)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
		
	return ctx->GetNewFileNameCopy(name, ext, folder, session_only
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
					,useOperatorCache
#endif
	);
		}

void Cache_Manager::EmptyDCache(
	OEM_EXT_OPER_CACHE_MNG(delete_cache_mode mode)    OPER_CACHE_COMMA()
	OEM_CACHE_OPER_AFTER(const char *cache_operation_target)
	OEM_CACHE_OPER_AFTER(int flags)
	OEM_CACHE_OPER(time_t origin_time))
{
	CACHE_CTX_CALL_ALL(EmptyDCache(FALSE
		OEM_EXT_OPER_CACHE_MNG_BEFORE(mode)    OPER_CACHE_COMMA()
		OEM_CACHE_OPER_AFTER(cache_operation_target)
		OEM_CACHE_OPER_AFTER(flags)
		OEM_CACHE_OPER(origin_time)));
}
	
void Cache_Manager::EmptyDCache(
	URL_CONTEXT_ID ctx_id
	OEM_EXT_OPER_CACHE_MNG_BEFORE(delete_cache_mode mode)    OPER_CACHE_COMMA()
	OEM_CACHE_OPER_AFTER(const char *cache_operation_target)
	OEM_CACHE_OPER_AFTER(int flags)
	OEM_CACHE_OPER(time_t origin_time))
{
	CACHE_CTX_FIND_ID_VOID(ctx_id, EmptyDCache(FALSE
		OEM_EXT_OPER_CACHE_MNG_BEFORE(mode)    OPER_CACHE_COMMA()
		OEM_CACHE_OPER_AFTER(cache_operation_target)
		OEM_CACHE_OPER_AFTER(flags)
		OEM_CACHE_OPER(origin_time)));
}


void Cache_Manager::DoCheckCache()
{
	int r_temp=0, d_temp=0;
	
	CACHE_CTX_CALL_ALL(DoCheckCache(r_temp, d_temp));
	
	if((r_temp || d_temp) && g_main_message_handler)
		g_main_message_handler->PostDelayedMessage(MSG_FLUSH_CACHE,0,0, (!d_temp || (r_temp && r_temp< d_temp) ? r_temp : d_temp) );
}

void Cache_Manager::InitL(
#ifdef DISK_CACHE_SUPPORT
		OpFileFolder a_vlink_loc, OpFileFolder a_cache_loc
#endif
)
{
	#if defined DISK_CACHE_SUPPORT
		ctx_main=OP_NEW(Context_Manager_Disk, (0, a_vlink_loc, a_cache_loc));
	#else
		ctx_main=OP_NEW(Context_Manager_Disk, (0));
	#endif

	if(!ctx_main)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	OP_ASSERT(urlManager);

	TRAPD(op_err, ctx_main->ConstructPrefL(PrefsCollectionNetwork::DiskCacheSize, FALSE));
	
	if(OpStatus::IsError(op_err))
	{
		OP_DELETE(ctx_main);
		ctx_main=NULL;
		LEAVE(op_err);
	}

	ctx_main->SignalCacheActivity();
}

Cache_Manager::~Cache_Manager()
{
	// Assure that all contexts have been already removed before by PreDestructStep
	OP_ASSERT(ctx_main == NULL);
	OP_ASSERT(context_list.Empty());
	OP_ASSERT(frozen_contexts.Empty());
}

#ifdef NEED_CACHE_EMPTY_INFO
BOOL Cache_Manager::IsCacheEmpty()
{
	CACHE_CTX_WHILE_BEGIN_REF
		if(!manager->IsCacheEmpty())
		{
			manager->DecReferencesCount();

			return FALSE;
		}
	CACHE_CTX_WHILE_END_REF
	
	return TRUE;
}
#endif // NEED_CACHE_EMPTY_INFO

#ifdef SELFTEST
BOOL Cache_Manager::CheckInvariants()
{
	CACHE_CTX_WHILE_BEGIN_REF
		if(!manager->CheckInvariants())
		{
			manager->DecReferencesCount();

			return FALSE;
		}
	CACHE_CTX_WHILE_END_REF

	return TRUE;
}
#endif // SELFTEST
