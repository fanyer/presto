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
#include "modules/pi/OpSystemInfo.h"

#include "modules/url/url2.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_cs.h"
#include "modules/cache/url_stor.h"
#include "modules/cache/cache_common.h"
#include "modules/cache/cache_utils.h"
#include "modules/cache/cache_propagate.h"

#ifdef _DEBUG
#ifdef YNP_WORK
#include "modules/olddebug/tstdump.h"
#define DEBUG_CACHE
#include "modules/url/tools/url_debug.h"
#endif
#endif

void Context_Manager::DoCheckCache(int &r_temp, int &d_temp)
{
	int r_prop=r_temp, d_prop=d_temp;

	CACHE_PROPAGATE_ALWAYS_VOID(DoCheckCache(r_prop, d_prop));

	int temp_delay;
	
	if(do_check_ram_cache)
	{
		temp_delay = CheckRamCacheSize();
		if(temp_delay && (temp_delay < r_temp || !r_temp))
			r_temp = temp_delay;
	}
#if defined(DISK_CACHE_SUPPORT) && !defined(SEARCH_ENGINE_CACHE)
	if(do_check_disk_cache)
	{
		temp_delay = CheckDCacheSize();
		if(temp_delay && (temp_delay < d_temp || !d_temp))
			d_temp = temp_delay;
	}
#endif
	
	do_check_ram_cache = (r_temp != 0 ? TRUE : FALSE);
	
	// Just to please Coverity...
	#if defined(DISK_CACHE_SUPPORT) && !defined(SEARCH_ENGINE_CACHE)
		do_check_disk_cache= (d_temp != 0 ? TRUE : FALSE);
	#else
		OP_ASSERT(!d_temp);
		
		do_check_disk_cache=FALSE;
	#endif
}

#ifdef DISK_CACHE_SUPPORT
#ifndef SEARCH_ENGINE_CACHE
void Context_Manager::StartCheckDCache()
{
	/// Avoid posting multiple messages...
	if(this!=urlManager->GetMainContext())
	{
		urlManager->StartCheckDCache();
		return;
	}

	if(!g_main_message_handler)
	{
		CheckDCacheSize();
		
		CACHE_PROPAGATE_ALWAYS_VOID(CheckDCacheSize());

		return;
	}
	if(!do_check_disk_cache && !do_check_ram_cache && g_main_message_handler)
		g_main_message_handler->PostDelayedMessage(MSG_FLUSH_CACHE,0,0,300);
	do_check_disk_cache = TRUE;
}
#endif
#endif // DISK_CACHE_SUPPORT

void Context_Manager::StartCheckRamCache()
{
	/// Avoid posting multiple messages...
	if(this!=urlManager->GetMainContext())
	{
		if(urlManager->GetMainContext())
		{
			OP_ASSERT(urlManager->GetMainContext()->context_id==0);
		
			urlManager->GetMainContext()->StartCheckRamCache();
		}

		return;
	}

	if(!g_main_message_handler)
	{
		CheckRamCacheSize();

		CACHE_PROPAGATE_ALWAYS_VOID(CheckRamCacheSize());

		return;
	}
	if(!do_check_disk_cache && !do_check_ram_cache && g_main_message_handler)
		g_main_message_handler->PostDelayedMessage(MSG_FLUSH_CACHE,0,0,300);
	do_check_ram_cache = TRUE;
}

void Context_Manager::SetCacheSize(OpFileLength new_size)
{
#ifdef SEARCH_ENGINE_CACHE
	OpFileLength old_size = GetCacheSize();
#endif

	size_disk.SetSize(new_size);

#ifdef SEARCH_ENGINE_CACHE
	if(GetCacheSize() < old_size)
	{
		double endt = g_op_time_info->GetWallClockMS() + 1000;  // one second maximum

		while (url_store->m_disk_index.DeleteOld(GetCacheSize()) == OpBoolean::IS_FALSE)  // error is ignored
			if(g_op_time_info->GetWallClockMS() > endt)
				break;  // if the time limit was reached, the resize will continue later with insertions
	}
#endif

#ifdef DISK_CACHE_SUPPORT
#ifndef SEARCH_ENGINE_CACHE
	StartCheckDCache();
#endif
#endif
}

void Context_Manager::AddToCacheSize(OpFileLength content_size, URL_Rep *url, BOOL add_url_size_estimantion)
{
	urlManager->updated_cache=TRUE;

	OP_NEW_DBG("Context_Manager::AddToCacheSize()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - estimation: %d - add estimation: %d - ctx id: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, (UINT32) url->GetURLObjectSize(), add_url_size_estimantion, url->GetContextId()));

	// Let's not waste time without any reason
	if(!content_size && !add_url_size_estimantion)
		return;

	size_disk.AddToSize(content_size, url, add_url_size_estimantion);

	//PrintfTofile("cs.txt", "Add\t%6lu\t%9lu\t%9lu\n", add_used + CACHE_URL_OBJECT_SIZE_ESTIMATION, size_disk.GetUsed(), old);

#ifdef DISK_CACHE_SUPPORT
	if(size_disk.IsLimitExceeded(GetCacheSize()))
		StartCheckDCache();
#endif // DISK_CACHE_SUPPORT
}

#if defined STRICT_CACHE_LIMIT2
void Context_Manager_Base::AddToPredictedCacheSize(OpFileLength content_size, URL_Rep *url, BOOL add_url_size_estimantion)
{
	OP_NEW_DBG("Context_Manager::AddToPredictedCacheSize()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - ctx id: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, url->GetContextId()));
	size_predicted.AddToSize(content_size, url, add_url_size_estimantion);
}

BOOL Context_Manager_Base::PredictedWillExceedCacheSize(OpFileLength content_size, URL_Rep *url)
{
	OP_ASSERT(url);

	return size_predicted.IsLimitExceeded(GetCacheSize(), content_size + url->GetURLObjectSize());
}
#endif

void Context_Manager::SubFromCacheSize(OpFileLength content_size, URL_Rep *url, BOOL enable_checks)
{
	urlManager->updated_cache=TRUE;

	OP_NEW_DBG("Context_Manager::SubFromCacheSize()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - ctx id: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, url->GetContextId()));

	size_disk.SubFromSize(content_size, url, enable_checks);

#if defined STRICT_CACHE_LIMIT2
	size_predicted.SubFromSize(content_size, url, enable_checks);
#endif // STRICT_CACHE_LIMIT2

#ifdef SELFTEST
	VerifyCacheSize();
#endif // SELFTEST
}

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
void Context_Manager::AddToOCacheSize(OpFileLength content_size, URL_Rep *url, BOOL add_url_size_estimantion)
{
	OP_NEW_DBG("Context_Manager::AddToOCacheSize()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - ctx id: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, url->GetContextId()));
	size_oem.AddToSize(content_size, url, add_url_size_estimantion);

#ifdef DISK_CACHE_SUPPORT
	if(size_oem.IsLimitExceeded(GetCacheSize()))
		StartCheckDCache();
#endif // DISK_CACHE_SUPPORT
}

void Context_Manager::SubFromOCacheSize(OpFileLength content_size, URL_Rep *url, BOOL enable_checks)
{
	OP_NEW_DBG("Context_Manager::SubFromOCacheSize()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - ctx id: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, url->GetContextId()));
	size_oem.SubFromSize(content_size, url, enable_checks);

#ifdef SELFTEST
	VerifyCacheSize();
#endif // SELFTEST
}
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT

void Context_Manager::AddToRamCacheSize(OpFileLength content_size, URL_Rep *url, BOOL add_url_size_estimantion)
{
	OP_NEW_DBG("Context_Manager::AddToRamCacheSize()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - ctx id: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, url->GetContextId()));

	size_ram.AddToSize(content_size, url, add_url_size_estimantion);

	StartCheckRamCache();
}

void Context_Manager::SubFromRamCacheSize(OpFileLength content_size, URL_Rep *url)
{
	OP_NEW_DBG("Context_Manager::SubFromRamCacheSize()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - ctx id: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, url->GetContextId()));
	size_ram.SubFromSize(content_size, url, TRUE);

#ifdef SELFTEST
	VerifyCacheSize();
#endif // SELFTEST
}

#ifdef DISK_CACHE_SUPPORT
#ifndef SEARCH_ENGINE_CACHE
int Context_Manager::CheckDCacheSize(BOOL all)
{
	CACHE_PROPAGATE_ALWAYS_VOID(CheckDCacheSize());

	if(flushing_disk_cache)
		return FALSE;

	CheckRamCacheSize(all ? size_ram.GetSize() : (OpFileLength) -1);
	int ret_val = CheckCacheSize((URL_DataStorage *) LRU_list.First(), NULL, 
					flushing_disk_cache, last_disk_cache_flush, 60, 
					&size_disk, GetCacheSize(), (all ? GetCacheSize() : (OpFileLength) -1), TRUE);
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT
	if(!ret_val)
	{
		time_t temp_last_flush = 0; // Just need one temporarily, last_disk_cache_flush was already updated and would block this operation.
		ret_val = CheckCacheSize((URL_DataStorage *) LRU_list.First(), NULL, 
					flushing_disk_cache, temp_last_flush, 60, 
#if	!defined __OEM_OPERATOR_CACHE_MANAGEMENT
					&size_disk , GetCacheSize(),
					(all ? GetCacheSize() : (OpFileLength) -1 ),
#else
					&size_oem , size_oem.GetSize(),
					(all ? size_oem.GetSize() : (OpFileLength) -1 ),
#endif
					TRUE, TRUE
					);
	}
#endif

	return ret_val;
}
#endif
#endif // DISK_CACHE_SUPPORT

int Context_Manager::CheckRamCacheSize(OpFileLength force_size)	// force_size >= 0 means force to this size
{
	if(flushing_ram_cache)
		return 0;

	int wait_period = 0;

	if(LRU_temp)
		wait_period += CheckCacheSize(LRU_temp, NULL, 
					flushing_ram_cache, last_ram_cache_flush, 10, 
					&size_ram,  ((long) force_size >= 0 ? force_size : size_ram.GetSize()), force_size, FALSE);

	wait_period += CheckCacheSize(LRU_ram, NULL, 
					flushing_ram_cache, last_ram_cache_flush, 10, 
					&size_ram,  ((long) force_size >= 0 ? force_size : size_ram.GetSize()), force_size, FALSE);

	return wait_period;
}


#ifdef STRICT_CACHE_LIMIT
void Context_Manager::MakeRoomUnderMemoryLimit(int size)
{
	int freemem = memory_limit - GetUsedUnderMemoryLimit();
	if(freemem >= size)
		return;

//	printf("MRUML: lim=%d, used=%d, asked for=%d", memory_limit, GetUsedUnderMemoryLimit(), size);
	// This tries to force size_ram.GetUsed() to be smaller than the argument
	CheckRamCacheSize(size_ram.GetUsed() - (size - freemem));
//	printf("  usedafter=%d, u+s=%d\n", GetUsedUnderMemoryLimit(), GetUsedUnderMemoryLimit() + size);
};
#endif

#ifdef _OPERA_DEBUG_DOC_
void Context_Manager::GetCacheMemUsed(DebugUrlMemory &debug)
{
	URL_Rep *rep;

	debug.registered_diskcache = (unsigned long) size_disk.GetUsed();
	debug.registered_ramcache = (unsigned long) size_ram.GetUsed();

	rep = url_store->GetFirstURL_Rep();
	while(rep)
	{
		rep->GetMemUsed(debug);
		rep = url_store->GetNextURL_Rep();
	}
}
#endif


#ifdef SUPPORT_RIM_MDS_CACHE_INFO
#include "modules/pi/OpCacheInfo.h"
#include "modules/url/url_pd.h"

void Context_Manager::StartReconcile(int session_id)
{
	if( !m_cacheInfoSession || !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RIMMDSBrowserMode) )
		return;

	if( m_cacheInfoSessionId < 0 )
		m_cacheInfoSessionId = session_id;
	else if( m_cacheInfoSessionId != session_id )
		return;

	URL_Rep* url_rep = url_store->GetFirstURL_Rep();
	while (url_rep)
	{
		if(url_rep->GetAttribute(URL::KType) == URL_HTTP &&
			!url_rep->GetAttribute(URL::KHavePassword) &&
			!url_rep->GetAttribute(URL::KHaveAuthentication) &&
			url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_DISK )
		{
			URL url(url_rep, (const char *) NULL);
			URLLink *link_item = OP_NEW(URLLink, (url));
			if( !link_item )
			{
				m_cacheInfoList.Clear();
				m_cacheInfoSession->ReconcileFailed(session_id);
				return;
			}
			link_item->Into(&m_cacheInfoList);
		}
		url_rep = url_store->GetNextURL_Rep();
	}   

	if( !g_main_message_handler->PostMessage(MSG_CACHEMAN_REPORT_SOME_ITEMS, session_id, 0) )
	{
		m_cacheInfoSession->ReconcileFailed(session_id);
	}
}

void Context_Manager::CancelReconcile(int session_id)
{
	if( session_id != m_cacheInfoSessionId )
		return;

	m_cacheInfoList.Clear();
}

OP_STATUS Context_Manager::ReportCacheItem(URL_Rep* url, BOOL added)
{
	if( !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RIMMDSBrowserMode) ||
		!m_cacheInfoSession || m_cacheInfoSessionId < 0 )
		return;

	URLCacheType ctype = (URLCacheType)url->GetAttribute(URL::KCacheType);
	URLType utype = (URLType)url->GetAttribute(URL::KType);

	if( !m_cacheInfoSession ||
		(utype != URL_HTTP && (utype != URL_HTTPS && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheHTTPS))) ||
		(ctype != URL_CACHE_MEMORY && ctype != URL_CACHE_DISK) ||
		url->GetAttribute(URL::KHavePassword) ||
		url->GetAttribute(URL::KHaveAuthentication) )
	{
		return OpStatus::ERR;
	}

	if( added )
	{
		cache_info_st cacheItem;
		URL_DataStorage *url_ds = 0;
		URL_HTTP_ProtocolData *hptr = 0;

		if( (url_ds = url->GetDataStorage()) &&
			(hptr = url_ds->GetHTTPProtocolData()) )
		{
			cacheItem.url = url->GetAttribute(URL::KName);
			cacheItem.eTag = hptr->keepinfo.entitytag.CStr();
			url_ds->GetAttribute(URL::KContentLoaded,&(cacheItem.contentLength));
			url_ds->GetAttribute(URL::KVLastModified,&(cacheItem.lastModified));
			cacheItem.expires = hptr->keepinfo.expires;

			m_cacheInfoSession->CacheEntryAdded(m_cacheInfoSessionId, cacheItem);
		}
		else
			return OpStatus::ERR;
	}
	else
	{
		m_cacheInfoSession->CacheEntryRemoved(m_cacheInfoSessionId, url->Name(PASSWORD_NOTHING));
	}
	return OpStatus::OK;
}

#if CACHE_TRIM_MAX_NUM_CACHEITEM_REPORT < 1
#error "CACHE_TRIM_MAX_NUM_CACHEITEM_REPORT must be at least one."
#endif

OP_STATUS Context_Manager::ReportSomeCacheItems(int session_id)
{
	if( session_id != m_cacheInfoSessionId )
	{
		m_cacheInfoSession->ReconcileFailed(session_id);
		return OpStatus::ERR;
	}
	
	cache_info_st cacheItem;
	URL_DataStorage *url_ds = 0;
	URL_HTTP_ProtocolData *hptr = 0;
	int counter = 0;

	URLLink *list_elem = (URLLink*)m_cacheInfoList.First();
	while( list_elem && counter < CACHE_TRIM_MAX_NUM_CACHEITEM_REPORT )
	{
		URL_Rep* item = list_elem->GetURL().GetRep();
		if( (url_ds = item->GetDataStorage()) &&
			(hptr = url_ds->GetHTTPProtocolData()) )
		{
			cacheItem.url = item->GetAttribute(URL::KName);
			cacheItem.eTag = hptr->keepinfo.entitytag.CStr();
			url_ds->GetAttribute(URL::KContentLoaded,&(cacheItem.contentLength));
			url_ds->GetAttribute(URL::KVLastModified,&(cacheItem.lastModified));
			cacheItem.expires = hptr->keepinfo.expires;

			m_cacheInfoSession->ReconcileNextCacheEntry(session_id, cacheItem);
		}

		list_elem->Out();
		OP_DELETE(list_elem);
		list_elem = (URLLink*)m_cacheInfoList.First();
		counter++;
	}

	if( m_cacheInfoList.Empty() )
	{
		m_cacheInfoSession->ReconcileCompleted(session_id);
	}
	else if( !g_main_message_handler->PostMessage(MSG_CACHEMAN_REPORT_SOME_ITEMS, session_id, 0) )
	{
		CancelReconcile(session_id);
		m_cacheInfoSession->ReconcileFailed(session_id);
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

#endif // SUPPORT_RIM_MDS_CACHE_INFO
