/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url2.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/cache/cache_int.h"
#include "modules/cache/cache_utils.h"
#include "modules/cache/url_cs.h"
#include "modules/cache/url_stor.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/cache/cache_propagate.h"
#include "modules/cache/cache_debug.h"

#ifdef URL_FILTER
	#include "modules/content_filter/content_filter.h"
#endif

void Context_Manager::CacheCleanUp(BOOL ignore_downloads)
{
	/// FIXME: if the context manager cannot initiate a download, the clean up is useless
	if (url_store)
	{
		URL_Rep* url_rep = url_store->GetFirstURL_Rep();
		while (url_rep)
		{
			if(!ignore_downloads || (URLCacheType) url_rep->GetAttribute(URL::KCacheType) != URL_CACHE_USERFILE)
				url_rep->StopLoading(NULL);
			url_rep = url_store->GetNextURL_Rep();
		}
	}

	CACHE_PROPAGATE_ALWAYS_VOID(CacheCleanUp(ignore_downloads));
}

void Context_Manager::RemoveFromStorage(URL_Rep *url)
{
	OP_ASSERT(url);
	
	if(!url)
		return;

	// Unique URLs are not in url_store
	if(!url->GetAttribute(URL::KUnique) && url_store)
		url_store->RemoveURL_Rep(url);

	CACHE_PROPAGATE_NO_REMOTE_VOID(RemoveFromStorage(url));
}

BOOL Context_Manager::FreeUnusedResources(BOOL all SELFTEST_PARAM_COMMA_BEFORE(DebugFreeUnusedResources *dfur))
{
	#define MS_CACHED_ITERATIONS 20		// Number of URLs to check before updating the real time

	int ms_remaining=MS_CACHED_ITERATIONS;		// Number of URLs remaining to check with the cached time
#ifndef RAMCACHE_ONLY
	double system_time_sec = g_op_time_info->GetTimeUTC()/1000.0; // For this value, it should be acceptable to just cache the value without updating it
#endif // RAMCACHE_ONLY
	double threshold = 100.0; // Max. ms to work
	double start = g_op_time_info->GetRuntimeMS();
	double current = start;

#ifdef SELFTEST
	if(dfur) // On "cache_free_unused.ot", just process URLs for 10 ms, to be able to better debug problems
	{
		dfur->mng_processed++;
		threshold = 10.0;
	}

	CheckInvariants();
#endif // SELFTEST

	URL_Rep* url_rep;

	// Skip resources that have been already freed; this avoid stalling
	if(!free_bm.IsReset())
		url_rep = url_store->JumpToURL_RepBookmark(free_bm);
	else
		url_rep = url_store->GetFirstURL_Rep(free_bm);

	#ifdef SELFTEST
		if(dfur) // Collect statistics
		{
			dfur->url_skipped = free_bm.GetAbsolutePosition();
			dfur->time_skip += g_op_time_info->GetRuntimeMS()-start;
		}
	#endif  // SELFTEST

	/// If the end is reached, then restart
	if(!url_rep)
	{
		url_rep=url_store->GetFirstURL_Rep(free_bm);

		#ifdef SELFTEST
			if(dfur /* && skipped*/)	// Collect statistics... possibly wrong...
				dfur->url_restart ++;
		#endif  // SELFTEST
	}

	while (url_rep)
	{
		ms_remaining--;

#ifndef RAMCACHE_ONLY
			url_rep->FreeUnusedResources(system_time_sec);
#endif // RAMCACHE_ONLY

#ifdef SELFTEST
			if(dfur)	// Collect statistics
				dfur->url_processed++;
#endif  // SELFTEST

			if ( url_rep->GetRefCount() == 1 && (
	#if defined(DISK_CACHE_SUPPORT)
						cache_loc == OPFILE_ABSOLUTE_FOLDER ||
	#endif // DISK_CACHE_SUPPORT
						(	url_rep->InList() && 
							url_rep->GetDataStorage() == NULL && !url_rep->HasBeenVisited()) ))
			{
				url_store->RemoveURL_Rep(url_rep);
				OP_DELETE(url_rep);

#ifdef SELFTEST
				if(dfur)	// Collect statistics
					dfur->url_deleted++;
#endif
			}

			// Update the time
			if(!all && !ms_remaining)
			{
				ms_remaining = MS_CACHED_ITERATIONS;
				
				current = g_op_time_info->GetRuntimeMS();

				if (current - start > threshold)
				{
#ifdef SELFTEST
					if(dfur)	// Collect statistics
						dfur->time += current-start;
#endif

					return TRUE;
				}
			}

		url_rep = url_store->GetNextURL_Rep(free_bm);

		// Technically speaking, if url_rep is NULL it could restart, but it means that we should stop before reprocessing the same URLs
	}

	free_bm.Reset();  // The end of the list has been reached: next time check from the beginning

#ifdef SELFTEST
	CheckInvariants();
#endif

#ifdef DISK_CACHE_SUPPORT
	if(all)
		CheckRamCacheSize(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheToDisk) ? 0 : GetCacheSize()); // Force as much as possible out
# ifndef SEARCH_ENGINE_CACHE
	if (size_disk.GetUsed() > GetCacheSize() + GetCacheSize()/20) // allow 5% extra before we check size
	{
		if(all)
			CheckDCacheSize(all);
		else
			StartCheckDCache();
	}
# endif // SEARCH_ENGINE_CACHE
#else
	if(all)
		CheckRamCacheSize(0); // Force as much as possible out
	StartCheckRamCache();
#endif // DISK_CACHE_SUPPORT

	CACHE_PROPAGATE_NO_REMOTE_VOID(FreeUnusedResources(all));

#ifdef SELFTEST
	if(dfur) // Collect statistics
	{
		dfur->mng_finished++;
		dfur->time += current-start;
	}
#endif // SELFTEST

	return FALSE;
}

#ifdef UNUSED_CODE
void Context_Manager::PurgeVisitedLinks( time_t maxage /*seconds*/ )
{
	// lth added this function; is it right?
	// Warning: some cargo cult programming ahead.
	URL_Rep* url_rep = url_store->GetFirstURL_Rep();
	time_t time_now = g_timecache->CurrentTime();
	while (url_rep)
	{
		if (url_rep->GetRefCount() <= 1
		    && url_rep->Status(FALSE) != URL_LOADING
			&& time_now - url_rep->last_visited > maxage)
		{
			url_store->RemoveURL_Rep(url_rep);
			OP_DELETE(url_rep);
		}
		url_rep = url_store->GetNextURL_Rep();
	}
}
#endif // UNUSED_CODE

void Context_Manager::EmptyDCache(BOOL delete_app_cache
OEM_EXT_OPER_CACHE_MNG_BEFORE(delete_cache_mode mode)
OPER_CACHE_COMMA()
OEM_CACHE_OPER_AFTER(const char *cache_operation_target)
OEM_CACHE_OPER_AFTER(int flags)
OEM_CACHE_OPER(time_t origin_time)
)
#ifdef SEARCH_ENGINE_CACHE

{
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	if (mode == EMPTY_NORMAL_CACHE || mode == EMPTY_NORMAL_AND_OPERATOR_CACHE)
#endif
	{
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_CACHE_OPERATION
		url_store->m_disk_index.Delete(cache_operation_target, (flags & COFLAG_PREFIX_MATCH) != 0, origin_time);
#else
		url_store->m_disk_index.DeleteOld(0);
#endif
	}
}

#else  // SEARCH_ENGINE_CACHE

{
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_CACHE_OPERATION
	//need to delete files that match cache operation filename
	int tz = g_op_time_info->GetTimezone();
	URL matchurl;
	if (cache_operation_target)
		matchurl = GetURL(cache_operation_target);
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT && __OEM_CACHE_OPERATION

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && !defined __OEM_OPERATOR_CACHE_MANAGEMENT
	time_t never_flush_expiration_date = g_timecache->CurrentTime() - g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NeverFlushExpirationTimeDays)*24*60*60;
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT && __OEM_OPERATOR_CACHE_MANAGEMENT

	Head temp_LRU;
	URL_DataStorage* cur_url_ds;
	URL_DataStorage *next_url_ds = (URL_DataStorage *) LRU_list.First();

	while (next_url_ds)
	{
		cur_url_ds = next_url_ds;
		next_url_ds = (URL_DataStorage *) next_url_ds->Suc();

		RemoveLRU_Item(cur_url_ds);
		cur_url_ds->Into(&temp_LRU);
	}

	URL_Rep* url_rep;
	
	while ((next_url_ds = (URL_DataStorage *) temp_LRU.First()) != NULL)
	{
		url_rep = next_url_ds->url;
		next_url_ds->Out();
		SetLRU_Item(url_rep->GetDataStorage());

		// URLs with KIsApplicationCacheURL should not be deleted (for more information, check CORE-45105)
#ifdef APPLICATION_CACHE_SUPPORT
		if (delete_app_cache || !url_rep->GetAttribute(URL::KIsApplicationCacheURL))
#endif // APPLICATION_CACHE_SUPPORT
		{
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT 
#if !defined __OEM_OPERATOR_CACHE_MANAGEMENT
			time_t visited = 0;

			url_rep->GetAttribute(URL::KVLocalTimeVisited, &visited);
			if(url_rep->GetAttribute(URL::KNeverFlush) && visited >= never_flush_expiration_date)
			{
				// Do nothing
			}
			else
#else
			BOOL delete_item = TRUE;
			
			if(mode != EMPTY_NORMAL_AND_OPERATOR_CACHE)
			{
				BOOL is_never_flush = !!url_rep->GetAttribute(URL::KNeverFlush);
				
				delete_item = ((is_never_flush && mode == EMPTY_OPERATOR_CACHE) || (!is_never_flush && mode == EMPTY_NORMAL_CACHE));
			}
			if (delete_item)
#endif // !__OEM_OPERATOR_CACHE_MANAGEMENT
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT
			{

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_CACHE_OPERATION
				if (origin_time && url_rep->GetLocalTimeLoaded(FALSE) + tz > origin_time)
				{
					// The cached object is newer than the cache operation message, so we won't delete it then.
					delete_item = FALSE;
				}

				if (delete_item && cache_operation_target)
				{
					if (flags & COFLAG_PREFIX_MATCH)
					{
						delete_item = FALSE;
						// Match prefix as specified in WAP-175-CacheOp, chapter 6.4
						if (matchurl.Type() == url_rep->GetType() &&
							matchurl.GetServerName() == url_rep->GetServerName() &&
							matchurl.GetServerPort() == url_rep->GetServerPort())
						{
							const char *obj = url_rep->GetPath();
							const char *pfx = matchurl.GetPath();
							if (!pfx || !*pfx)
								delete_item = TRUE;
							else if (obj && *obj)
							{
								BOOL prev_sep = FALSE;
								while (*obj == *pfx && *pfx && *obj && *pfx != '?' && *pfx != '#')
								{
									prev_sep = *obj == '/';
									++obj;
									++pfx;
								}
								delete_item = (*pfx == '?' || *pfx == '#') && (!*obj || *obj == *pfx) ||
									!*pfx && (prev_sep || !*obj || *obj == '?' || *obj == '#' || *obj == '/');
							}
						}
					}
					else
					{
						delete_item = matchurl.GetRep() == url_rep;
					}
				}
				if (delete_item)
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT && __OEM_CACHE_OPERATION
				{
				#ifdef URL_FILTER
					// Check if the URL really need to be deleted
					if(empty_dcache_filter)
					{
						BOOL url_del=TRUE;
						
						OpStringC str=url_rep->GetAttribute(URL::KUniName_Escaped,URL::KFollowRedirect); // don't filter on username/passwords
						
						if(OpStatus::IsSuccess(empty_dcache_filter->CheckURL(str.CStr(), url_del, NULL)))
						{
							if(!url_del)
								continue;
						}
						#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
							else  // In case of error, check KNeverFlush to decide what to do
							{
								BOOL is_never_flush = !!url_rep->GetAttribute(URL::KNeverFlush);
								
								if(is_never_flush)
									continue;
							}
						#endif
					}
					
				#endif
					URLStatus status = (URLStatus) url_rep->GetAttribute(URL::KLoadStatus);
					if(!url_rep->GetUsedCount() && status != URL_LOADING && status != URL_UNLOADED &&
						(URLCacheType) url_rep->GetAttribute(URL::KCacheType) != URL_CACHE_USERFILE)
					{
						url_rep->SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
						url_rep->IncRef();
						url_rep->Unload();
						url_rep->DecRef();
						if(!url_rep->InList())
						{
							if (!url_rep->GetAttribute(URL::KUnique))
								url_store->AddURL_Rep(url_rep);
							else
							{
								//url_rep->DecRef();
								if(url_rep->GetRefCount()<1)
									OP_DELETE(url_rep);
							}
						}
					}
					else
					{
						if(url_rep->GetDataStorage())
						{
							url_rep->SetAttribute(URL::KCachePolicy_AlwaysVerify, TRUE);
							if(url_rep->GetDataStorage()->storage && url_rep->GetDataStorage()->storage->GetCacheType() != URL_CACHE_USERFILE)
								url_rep->GetDataStorage()->storage->SetCacheType(URL_CACHE_TEMP);
						}
					}
                } // last delete_item check
			} // delete_item check
		}
	} // While, temp_LRU loop


#ifdef DISK_CACHE_SUPPORT
	FileName_Store filenames(1024); // List of files to delete
	OP_STATUS op_err = filenames.Construct();
	RAISE_AND_RETURN_VOID_IF_ERROR(op_err);

	if(GetCacheLocationForFiles() != OPFILE_ABSOLUTE_FOLDER)  // OPFILE_ABSOLUTE_FOLDER means temporary files in the main directory, and no index
		ReadDCacheDir(filenames, filenames, GetCacheLocationForFiles(), TRUE, FALSE, NULL);

	// URLs in url_store are not meant to be deleted...
	url_rep = url_store->GetFirstURL_Rep();
	
	while (url_rep)
	{
		OpStringC temp_filename(url_rep->GetAttribute(URL::KFileName));

		if(temp_filename.HasContent())
		{
			FileNameElement *filename = filenames.RetrieveFileName(temp_filename, NULL);
			
			if(filename)
			{
				filenames.RemoveFileName(filename);
				OP_DELETE(filename);
			}
		}

		url_rep = url_store->GetNextURL_Rep();
	}
	
	DeleteFiles(filenames);

#ifdef SUPPORT_RIM_MDS_CACHE_INFO
	if( m_cacheInfoSession )
		m_cacheInfoSession->CacheCleared(m_cacheInfoSessionId);
#endif // SUPPORT_RIM_MDS_CACHE_INFO

#endif // DISK_CACHE_SUPPORT

	CACHE_PROPAGATE_ALWAYS_VOID(EmptyDCache(delete_app_cache
		OEM_EXT_OPER_CACHE_MNG_BEFORE(mode)    OPER_CACHE_COMMA()
		OEM_CACHE_OPER_AFTER(cache_operation_target)
		OEM_CACHE_OPER_AFTER(flags)
		OEM_CACHE_OPER(origin_time)));
}
#endif  // SEARCH_ENGINE_CACHE

void Context_Manager::DeleteVisitedLinks()
{
	/* Just to be on the safe side */
	URL_Rep *url_rep = url_store->GetFirstURL_Rep();
	while (url_rep)
	{
		url_rep->SetIsFollowed(FALSE);	/// TODO: Can it be moved to the else branch?
		if(url_rep->GetRefCount() <= 1)
		{
			url_store->RemoveURL_Rep(url_rep);
			OP_DELETE(url_rep);
		}

		url_rep = url_store->GetNextURL_Rep();
	}

	CACHE_PROPAGATE_NO_REMOTE_VOID(DeleteVisitedLinks());
}

#ifdef _SSL_USE_SMARTCARD_
void Context_Manager::CleanSmartCardAuthenticatedDocuments(ServerName *server)
{
	URL_Rep* url_rep = url_store->GetFirstURL_Rep();
	while (url_rep)
	{
		if(url_rep->GetServerName() == server && url_rep->GetAttribute(URL::KHaveSmartCardAuthentication))
		{
			url_rep->Unload();
			if(url_rep->GetRefCount() <= 1)
			{
				url_store->RemoveURL_Rep(url_rep);
				OP_DELETE(url_rep);
			}
		}
		url_rep = url_store->GetNextURL_Rep();
	}
}

#endif

#ifdef DISK_CACHE_SUPPORT
void Context_Manager::DestroyURL(URL &url)
{
	OP_ASSERT(url.GetRep()->GetContextId()==context_id);
 
	url.Unload();
	
	URL_Rep *url_rep = url.GetRep();
	url = URL();
		
	if(url_rep->GetRefCount() == 1)
	{
		url_rep->DecRef();
		url_store->RemoveURL_Rep(url_rep);
		OP_DELETE(url_rep);
	}
}

void Context_Manager::DestroyURL(URL_Rep *rep)
{
	if(!rep)
		return;
		
	OP_ASSERT(rep->GetContextId()==context_id);
		
	rep->Unload();
	
	if(rep->GetRefCount() == 1)
	{
		rep->DecRef();
		url_store->RemoveURL_Rep(rep);
		OP_DELETE(rep);
	}
}
#endif


void Context_Manager::RemoveSensitiveCacheData()
{
	URL_Rep* url_rep = url_store->GetFirstURL_Rep();
	while (url_rep)
	{
		if ((URLStatus) url_rep->GetAttribute(URL::KLoadStatus) != URL_LOADING && 
			(	(URLType) url_rep->GetAttribute(URL::KType) == URL_HTTPS  || 
				url_rep->GetAttribute(URL::KHavePassword) || 
				url_rep->GetAttribute(URL::KHaveAuthentication)) )
		{
			url_rep->Unload();
			BOOL unique = !!url_rep->GetAttribute(URL::KUnique);

			if(!url_rep->InList() && !unique)
				url_store->AddURL_Rep(url_rep);
			else if(!url_rep->InList() && unique)
			{
				//url_rep->DecRef();
				if(url_rep->GetRefCount()<1)
					OP_DELETE(url_rep);
			}
		}
		url_rep = url_store->GetNextURL_Rep();
	}

	CACHE_PROPAGATE_ALWAYS_VOID(RemoveSensitiveCacheData());
}

void Context_Manager::RemoveMessageHandler(MessageHandler* mh)
{
	URL_Rep* url_rep;

    url_rep = url_store->GetFirstURL_Rep();
    while (url_rep)
	{
		url_rep->RemoveMessageHandler(mh);
		url_rep = url_store->GetNextURL_Rep();
	}
}

