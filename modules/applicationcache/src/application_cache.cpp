/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef APPLICATION_CACHE_SUPPORT


#include "modules/applicationcache/application_cache_manager.h"
#include "modules/applicationcache/application_cache.h"
#include "modules/applicationcache/application_cache_group.h"
#include "modules/applicationcache/src/application_cache_common.h"

#include "modules/applicationcache/manifest.h"
#include "modules/url/url2.h"
#include "modules/url/url_id.h"
#include "modules/applicationcache/src/manifest_parser.h"
#include "modules/url/url_man.h"
#include "modules/dom/domenvironment.h"
#include "modules/prefs/prefs_module.h"

#include "modules/prefs/prefsmanager/collections/pc_core.h"

ApplicationCache::ApplicationCache()
	: 	m_url_context_id((UINT32)-1)
	,	m_context_created(FALSE)
	,	m_complete(FALSE) // Todo: Check why is this complete by default
	,	m_application_cache_group(NULL)
	,	m_manifest(NULL)
	,	m_cache_disk_size(0)
	, 	m_is_obsolete(FALSE)
{
}

/**
 * This class must only be used in the ~ApplicationCache()
 */
struct UrlRemover : public OpHashTableForEachListener
{
	ApplicationCache *m_cache;
	ApplicationCacheManager *m_application_cache_manager;

	UrlRemover(ApplicationCache *cache,ApplicationCacheManager *application_cache_manager)
	: m_cache(cache),
	  m_application_cache_manager(application_cache_manager)
	  {}

	virtual ~UrlRemover(){};

	void HandleKeyData(const void* key, void* data)
	{
		ApplicationCache *cache;
		if (OpStatus::IsSuccess(m_application_cache_manager->m_cache_table_cached_url.GetData(static_cast<const uni_char*>(key), &cache)) && cache == m_cache)
		{
			m_application_cache_manager->m_cache_table_cached_url.Remove(static_cast<const uni_char*>(key), &cache);
			OP_ASSERT(cache == m_cache);
		}
	}
};

ApplicationCache::~ApplicationCache()
{
	/* This should be called to remove the cache urls from memory. however, the effect
	 * of the call is that the cached urls are deleted from disk too. This must be fixed in
	 * the cache module. So currently will be only do this when cache is obsolete.
	 */
	if (m_is_obsolete && urlManager)
		urlManager->RemoveContext(m_url_context_id, TRUE);

	RemoveAllCacheHostAssociations();

	ApplicationCacheManager *app_man = g_application_cache_manager;
	if (app_man)
	{
		ApplicationCache *cache;
		OpStatus::Ignore(app_man->m_cache_table_context_id.Remove(m_url_context_id, &cache));

		if (m_manifest && m_complete)
		{
			UrlRemover url_remover(this, app_man);
			m_manifest->GetEntryTable().ForEach(&url_remover);
		}
	}

	RemoveAllMasterURLs();


	if (m_application_cache_group)
		OpStatus::Ignore(m_application_cache_group->RemoveCache(this));

	OP_ASSERT(m_url_context_id != 0);

	OP_DELETE(m_manifest);
}

/* static */ OP_STATUS ApplicationCache::Make(ApplicationCache *&application_cache, const uni_char *cache_location, DOM_Environment *cache_host)
{
	URL_CONTEXT_ID ctx_id = urlManager->GetNewContextID();

	if (!ctx_id)
		return OpStatus::ERR;

	application_cache = NULL;

	OpAutoPtr<ApplicationCache> temp_cache(OP_NEW(ApplicationCache, ()));
	if (!temp_cache.get())
		return OpStatus::ERR_NO_MEMORY;

	if (cache_host)
		RETURN_IF_ERROR(temp_cache->AddCacheHostAssociation(cache_host));

	temp_cache->m_url_context_id = ctx_id;

	RETURN_IF_ERROR(temp_cache->CreateURLContext(cache_location));

	application_cache = temp_cache.release();
	return OpStatus::OK;
}

/* static */ OP_STATUS ApplicationCache::MakeFromDisk(ApplicationCache *&application_cache, const UnloadedDiskCache *unloaded_cache, ApplicationCacheGroup *application_cache_group, DOM_Environment *cache_host)
{
	application_cache = NULL;
	OpAutoPtr<ApplicationCache> temp_cache(OP_NEW(ApplicationCache, ()));
	if (!temp_cache.get())
		return OpStatus::ERR_NO_MEMORY;

	if (cache_host)
		RETURN_IF_ERROR(temp_cache->AddCacheHostAssociation(cache_host));

	temp_cache->m_url_context_id = urlManager->GetNewContextID();
	if (temp_cache->m_url_context_id == 0)
		return OpStatus::ERR;

	temp_cache->m_cache_disk_size = unloaded_cache->m_cache_disk_size;

	RETURN_IF_ERROR(temp_cache->CreateURLContext(unloaded_cache->m_location.CStr()));

	OpFile manifest_file;
 	OpString manifest_file_name;
 	RETURN_IF_ERROR(manifest_file_name.Set(temp_cache->m_application_cache_location.CStr()));
 	RETURN_IF_ERROR(manifest_file_name.Append("/"));
 	RETURN_IF_ERROR(manifest_file_name.Append(MANIFEST_FILENAME));

 	RETURN_IF_ERROR(Manifest::MakeManifestFromDisk(temp_cache->m_manifest, unloaded_cache->m_manifest, manifest_file_name, g_application_cache_manager->GetCacheFolder()));

 	RETURN_IF_ERROR(application_cache_group->AddCache(temp_cache.get()));

	UINT32 count = unloaded_cache->m_master_urls.GetCount();
	for (UINT32 index = 0; index < count; index ++)
	{
		OpStatus::Ignore(temp_cache->AddMasterURL(unloaded_cache->m_master_urls.Get(index)->CStr()));
	}

	temp_cache->SetCompletenessFlag(TRUE);

	//data_descriptor
 	application_cache = temp_cache.release();

 	return OpStatus::OK;
}

BOOL ApplicationCache::IsCached(const URL &url) const
{
	if (m_manifest)
	{
		OpString url_str;
		OpStatus::Ignore(url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, url_str));
		return IsCached(url_str.CStr());
	}
	return FALSE;
}

BOOL ApplicationCache::IsCached(const uni_char *url) const
{
	URL is_cached_url = g_url_api->GetURL(url, GetURLContextId());
	OP_ASSERT(m_application_cache_group);

	if (is_cached_url.GetAttribute(URL::KLoadStatus, URL::KNoRedirect) != URL_LOADED)
		return FALSE;

	return IsHandledByCache(url);
}

BOOL ApplicationCache::IsHandledByCache(const uni_char *url) const
{
	// manifest url is always cached
	if (m_application_cache_group && uni_str_eq(m_application_cache_group->GetManifestUrlStr(), url))
		return TRUE;

	// Step 2 in 6.9.7 Changes to the networking model in html5 application cache specification.
	if (
			m_manifest && url && m_application_cache_group &&
			(
					m_manifest->CheckCacheUrl(url) ||
					HasMasterURL(url))
		)
 		return TRUE;

	return FALSE;
}

BOOL ApplicationCache::MatchFallback(const URL &url) const
{
	if (m_manifest)
	{
		OpString url_str;
		if (OpStatus::IsError(url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, url_str)))
		{
			return FALSE;
		}
		return MatchFallback(url_str.CStr());
	}
	return FALSE;
}

BOOL ApplicationCache::MatchFallback(const uni_char *url) const
{
	if (m_manifest && url)
	{
		const uni_char *dummy_url;
		return m_manifest->MatchFallback(url, dummy_url);
	}
	return FALSE;
}

const uni_char *ApplicationCache::GetFallback(const URL &url) const
{
	if (m_manifest)
	{
		OpString url_str;
		if (OpStatus::IsError(url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, url_str)))
			return NULL;
		return GetFallback(url_str.CStr());
	}
	return NULL;
}

const uni_char *ApplicationCache::GetFallback(const uni_char *url) const
{
	// MatchFallback changes url to the fallback url
	const uni_char *fallback_url;
	if (m_manifest && url && m_manifest->MatchFallback(url, fallback_url))
		return url;
	return NULL;

}

BOOL ApplicationCache::IsWithelisted(const URL &url) const
{
	if (m_manifest)
	{
		OpString url_str;
		OpStatus::Ignore(url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, url_str));
		return IsWithelisted(url_str.CStr());
	}

	return FALSE;
}

BOOL ApplicationCache::IsWithelisted(const uni_char *url) const
{
	if (m_manifest && url)
		return m_manifest->MatchNetworkUrl(url);
	return FALSE;
}

OP_STATUS ApplicationCache::CreateURLContext(const uni_char *location)
{
	if (!m_context_created)
	{
		RETURN_IF_ERROR(m_application_cache_location.Set(location));

		OpFileFolder application_cache_folder;
		g_folder_manager->AddFolder(g_application_cache_manager->GetCacheFolder(), m_application_cache_location.CStr(), &application_cache_folder);

		RETURN_IF_LEAVE(urlManager->AddContextL(m_url_context_id
			, g_application_cache_manager->GetCacheFolder(), application_cache_folder, application_cache_folder, application_cache_folder
			, TRUE)); // We need to share cookies between all applications on the same server. Otherwise the user will
					  // have to log in over again each time the cache is updated.

		Context_Manager *ctx_manager = urlManager->FindContextManager(m_url_context_id);
		OP_ASSERT(ctx_manager);

		if (!ctx_manager)
			return OpStatus::ERR;

		ctx_manager->SetIsUsedByApplicationCache(TRUE);

		m_context_created = TRUE;
	}
	return OpStatus::OK;
}

ApplicationCache::CacheState ApplicationCache::GetCacheState()
{
	if (!m_application_cache_group)
		return UNCACHED;

	if (m_application_cache_group->IsObsolete())
		return OBSOLETE;

	ApplicationCacheGroup::CacheGroupState  group_state = m_application_cache_group->GetUpdateState();


	switch (group_state)
	{
	case ApplicationCacheGroup::IDLE:
	{
		OP_ASSERT(m_application_cache_group->GetMostRecentCache()->IsComplete() == TRUE && "The latest cache must be complete if the state of the group is IDLE");

		if (m_application_cache_group->GetMostRecentCache() == this)
			return IDLE;
		else
			return UPDATEREADY;
	}
	case ApplicationCacheGroup::CHECKING:
		return CHECKING;

	case ApplicationCacheGroup::DOWNLOADING:
		return DOWNLOADING;

	default:
		OP_ASSERT(!"Should not be here");
		return IDLE;
	}
}

OP_STATUS ApplicationCache::AddCacheHostAssociation(DOM_Environment *cache_host)
{
	if (cache_host && (g_application_cache_manager->GetApplicationCacheFromCacheHost(cache_host) == NULL))
	{
		OpStatus::Ignore(g_application_cache_manager->m_cache_table_frm.Add(cache_host, this));
		return m_cache_hosts.Add(cache_host);
	}

	return OpStatus::OK;
}

OP_STATUS ApplicationCache::RemoveCacheHostAssociation(DOM_Environment *cache_host)
{
	OP_ASSERT(cache_host);
	ApplicationCache *cache = NULL;
#ifdef DEBUG_ENABLE_OPASSERT
	OP_STATUS removed_manager = OpStatus::OK;
#endif
	if (g_application_cache_manager)
	{
#ifdef DEBUG_ENABLE_OPASSERT
		removed_manager =
#endif
			g_application_cache_manager->m_cache_table_frm.Remove(cache_host, &cache);
	}

	OP_STATUS removed_cache = m_cache_hosts.RemoveByItem(cache_host);

	OP_ASSERT("Cache manager and cache is out of sync!" && (!g_application_cache_manager || (g_application_cache_manager && removed_manager == removed_cache)));

	return removed_cache;
}

void ApplicationCache::RemoveAllCacheHostAssociations()
{
	while (m_cache_hosts.GetCount() > 0)
	{
		DOM_Environment *cache_host = m_cache_hosts.Get(0);
		OpStatus::Ignore(RemoveCacheHostAssociation(cache_host));
		if (m_application_cache_group)
			OpStatus::Ignore(m_application_cache_group->DeletePendingCacheHost(cache_host));

	}
}

void ApplicationCache::TakeOverManifest(Manifest* manifest)
{
	OP_DELETE(m_manifest);
	m_manifest = manifest;
}

OP_STATUS ApplicationCache::AddMasterURL(const uni_char *master_url)
{
	OP_ASSERT(GetCacheGrup());
	OpAutoPtr<OpString> url_container(OP_NEW(OpString,()));

	if (!url_container.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(url_container->Set(master_url));
	OP_STATUS err;
	if (OpStatus::IsSuccess(err = m_master_document_urls.Add(url_container->CStr(), url_container.get())))
	{
		OpString *url = url_container.release();

		ApplicationCacheGroup *cache_group = NULL;
		if (
				/* Don't add if it is already there( will be the case if a older cache had the same master document */
				OpStatus::IsError(g_application_cache_manager->GetApplicationCacheGroupMasterTable(url->CStr(), &cache_group)) &&
				OpStatus::IsMemoryError(err = g_application_cache_manager->AddApplicationCacheGroupMasterTable(url->CStr(), GetCacheGrup()))
			)
		{
			//* Clean up only if AddApplicationCacheGroupMasterTable return memory problems.
			OpString *str;
			m_master_document_urls.Remove(master_url, &str);
			OP_DELETE(str);
			return err;
		}
	}

	RETURN_IF_MEMORY_ERROR(err);

	OP_ASSERT(g_application_cache_manager->CacheGroupMasterTableContains(master_url));
	return OpStatus::OK;
}

OP_STATUS ApplicationCache::RemoveMasterURL(const uni_char *master_url)
{
//	OP_ASSERT(g_application_cache_manager->CacheGroupMasterTableContains(master_url));
	OpString *removed_string = NULL;
	ApplicationCacheManager *man = g_application_cache_manager;
	if (OpStatus::IsSuccess(m_master_document_urls.Remove(master_url, &removed_string)))
	{
		/* If no other caches in the group has this master document, remove the master url
		 * from the ApplicationCacheManager. */
		ApplicationCacheGroup *removed_group;
		if (
				man &&
				m_application_cache_group &&
				m_application_cache_group->IsObsolete() == FALSE &&
				!m_application_cache_group->GetMostRecentCache(FALSE, removed_string->CStr())
			)
		{
			OP_STATUS err = OpStatus::OK;
			OpStatus::Ignore(err = man->RemoveApplicationCacheGroupMasterTable(removed_string->CStr(), &removed_group));
			OP_ASSERT(OpStatus::IsSuccess(err));
		}

		OP_DELETE(removed_string);
		removed_string = NULL;
	}
	else
		return OpStatus::ERR;

	return OpStatus::OK;
}

class RemoveMasterUrlListener : public OpHashTableForEachListener
{
public:
	ApplicationCache *m_cache;
	RemoveMasterUrlListener(ApplicationCache *cache): m_cache(cache){};

	virtual void HandleKeyData(const void* key, void* data)
	{
		OpString *master_url = static_cast<OpString*>(data);
		/* This function make sure that the master url also is removed from the  ApplicationCacheManager if no
		 * cache has the master anymore.
		 */
		OP_STATUS status;
		OpStatus::Ignore(status = m_cache->RemoveMasterURL(master_url->CStr()));
		OP_ASSERT(OpStatus::IsSuccess(status));
	}
};


void ApplicationCache::RemoveAllMasterURLs()
{
	RemoveMasterUrlListener listener(this);
	m_master_document_urls.ForEach(&listener);
}

OP_STATUS ApplicationCache::MarkAsForegin(URL &url, BOOL foreign)
{
	RETURN_IF_ERROR(url.SetAttribute(URL::KIsApplicationCacheForeign, foreign));
	return OpStatus::OK;
}

OP_STATUS ApplicationCache::SetCompletenessFlag(BOOL complete)
{
	ApplicationCacheManager *man = g_application_cache_manager;

	OP_ASSERT(man);

	URL_API *url_api = g_url_api;
	if (complete == TRUE && m_manifest && man)
	{
		OP_ASSERT(m_complete == FALSE);
		OpAutoPtr<OpHashIterator> iter(m_manifest->GetCacheIterator());
		if (!iter.get())
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsSuccess(iter->First())) do
		{
			uni_char *url_str = static_cast<OpString*>(iter->GetData())->CStr();

			URL url = url_api->GetURL(url_str, m_url_context_id);
			url.SetAttribute(URL::KIsApplicationCacheURL, TRUE);

			ApplicationCache *cache;
			/* Remove any previous entry, so we are sure that the table has the
			 * most recent and relevant cache.
			 */
			OpStatus::Ignore(man->m_cache_table_cached_url.Remove(url_str, &cache));

			RETURN_IF_ERROR(man->m_cache_table_cached_url.Add(url_str, this));

		} while (OpStatus::IsSuccess(iter->Next()));

		/* Mark all master urls as application cache urls */
		iter.reset(m_master_document_urls.GetIterator());
		if (!iter.get())
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsSuccess(iter->First())) do
		{
			uni_char *url_str = static_cast<OpString*>(iter->GetData())->CStr();
			URL url = url_api->GetURL(url_str, m_url_context_id);
			url.SetAttribute(URL::KIsApplicationCacheURL, TRUE);
		} while (OpStatus::IsSuccess(iter->Next()));

		/* Also mark the manifest url. We must get the url in the correct cache context.
		 * The one in the manifest is the in the manifest context.
		 */
		OpString manifest_url_str;
		RETURN_IF_ERROR(m_manifest->GetManifestUrl().GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, manifest_url_str));
		URL url = url_api->GetURL(manifest_url_str, m_url_context_id);
		url.SetAttribute(URL::KIsApplicationCacheURL, TRUE);
	}

	m_complete = complete;
	return OpStatus::OK;
}

OP_STATUS ApplicationCache::RemoveOwnerCacheGroupFromManager()
{
	ApplicationCacheManager *man = g_application_cache_manager;
	if (!man) // might happen on shutdown
		return OpStatus::OK;

	OpAutoPtr<OpHashIterator> iterator(m_master_document_urls.GetIterator());
	if (iterator.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(iterator->First()))
		return OpStatus::OK; // No urls in hash table

	ApplicationCacheGroup *removed_cache_group;
	do {

		OpStatus::Ignore(man->RemoveApplicationCacheGroupMasterTable(static_cast<OpString*>(iterator->GetData())->CStr(), &removed_cache_group));

	} while (OpStatus::IsSuccess(iterator->Next()));

	return OpStatus::OK;
}

#endif // APPLICATION_CACHE_SUPPORT
