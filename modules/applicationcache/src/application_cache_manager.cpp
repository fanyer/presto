/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef APPLICATION_CACHE_SUPPORT

#include "modules/doc/frm_doc.h"
#include "modules/dom/domenvironment.h"

#include "modules/applicationcache/src/application_cache_common.h"
#include "modules/applicationcache/application_cache_manager.h"
#include "modules/applicationcache/src/manifest_parser.h"
#include "modules/applicationcache/manifest.h"

#include "modules/url/url2.h"
#include "modules/url/url_man.h"

#include "modules/url/url_rep.h"
#include "modules/cache/cache_int.h"
#include "modules/auth/auth_digest.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/applicationcache/application_cache_group.h"
#include "modules/applicationcache/application_cache.h"
#include "modules/applicationcache/src/application_cache_common.h"
#include "modules/applicationcache/manifest.h"

#include "modules/prefs/prefs_module.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/selftest/selftest_module.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"

class ExternalAPIAppCacheEntry : public OpWindowCommanderManager::OpApplicationCacheEntry
{
public:
	ExternalAPIAppCacheEntry() : m_disk_quota(0), m_disk_space_used(0) {}
	~ExternalAPIAppCacheEntry() {}

	const uni_char* GetApplicationCacheDomain() const { return m_domain.CStr(); }
	const uni_char* GetApplicationCacheManifestURL() const { return m_url.CStr(); }
	OpFileLength GetApplicationCacheDiskQuota() const { return m_disk_quota; }
	OpFileLength GetApplicationCacheCurrentDiskUse() const { return m_disk_space_used; }

	OP_STATUS SetDomain(const uni_char* domain) { return m_domain.Set(domain); }
	OP_STATUS SetDomain(URL& url) { return url.GetAttribute(URL::KHostName, m_domain); }
	OP_STATUS SetManifestURL(const uni_char* url) { return m_url.Set(url); }
	void SetApplicationCacheDiskQuota(OpFileLength quota) { m_disk_quota = quota; }
	void SetApplicationCacheDiskSpaceUsed(OpFileLength usage) { m_disk_space_used = usage; }

private:
	OpString m_domain, m_url;
	OpFileLength m_disk_quota, m_disk_space_used;
};


class InstallAppCacheCallbackContext : public OpApplicationCacheListener::InstallAppCacheCallback, public Link
{
public:

	enum InstallRequest
	{
		INSTALL_REQUEST,
		CHECK_FOR_UPDATE_REQUEST,
		INSTALL_UPDATE_REQUEST
	};

	InstallAppCacheCallbackContext(InstallRequest request, DOM_Environment *cache_host,
			const URL &manifest_url, URL &master_document_url, URL_ID id,
			ApplicationCacheGroup* cache_group = NULL,
			BOOL pass_cache_host_to_update_process = FALSE)
		 : m_request(request),
		   m_cache_host(cache_host),
		   m_manifest_url(manifest_url),
		   m_master_document_url(master_document_url),
		   m_id(id),
		   m_cache_group(cache_group),
		   m_pass_cache_host_to_update_process(pass_cache_host_to_update_process)
		   {}

	virtual ~InstallAppCacheCallbackContext() { Out(); }

	virtual void OnDownloadAppCacheReply(BOOL allow_install);
	virtual void OnCheckForNewAppCacheVersionReply(BOOL allow_checking_for_update);
	virtual void OnDownloadNewAppCacheVersionReply(BOOL allow_installing_update);

	void CancelDialogs();

	InstallRequest m_request;
	DOM_Environment *m_cache_host;
	const URL m_manifest_url;
	URL m_master_document_url;

	/* The id of the manifest url for this application */
	URL_ID m_id;

	ApplicationCacheGroup *m_cache_group;
	BOOL m_pass_cache_host_to_update_process;
};

class QuotaCallbackContext : public OpApplicationCacheListener::QuotaCallback, public Link
{
public:

	QuotaCallbackContext(DOM_Environment *cache_host, const URL &manifest_url, const URL &current_download_entry, ApplicationCache *cache, BOOL is_copying_from_previous_cache, URL_ID id)
		: m_cache_host(cache_host),
		  m_manifest_url(manifest_url),
		  m_current_download_entry(current_download_entry),
		  m_cache(cache),
		  m_is_copying_from_previous_cache(is_copying_from_previous_cache),
		  m_id(id)
		  {}

	virtual ~QuotaCallbackContext() { Out(); }

	/** Called by the UI when a decision has been made about increasing the quota.
	 *  @param allow_increase TRUE if the quota is allowed to increase.
	 *	@param new_quota_size Size to increase to. Ignored if not allowed to increase, 0 if
	 *                        the quota should be unlimited. */
	virtual void OnQuotaReply(BOOL allow_increase, OpFileLength new_quota_size);
	void CacheHostDestroyed(DOM_Environment* host) { if (host == m_cache_host) m_cache_host = NULL; }
	DOM_Environment *m_cache_host;
	const URL m_manifest_url;
	URL m_current_download_entry;
	ApplicationCache *m_cache;
	const BOOL m_is_copying_from_previous_cache;

	/* The id of the manifest url for this cache */
	URL_ID m_id;

};

void CancelDialogForInstallContext(WindowCommander *wc, InstallAppCacheCallbackContext *callback_context)
{
	if (wc && wc->GetApplicationCacheListener())
	{
		URL_ID id = callback_context->m_id;
		switch (callback_context->m_request)
		{
			case InstallAppCacheCallbackContext::INSTALL_REQUEST:
				wc->GetApplicationCacheListener()->CancelDownloadAppCache(wc, id);
				break;

			case InstallAppCacheCallbackContext::CHECK_FOR_UPDATE_REQUEST:
				wc->GetApplicationCacheListener()->CancelCheckForNewAppCacheVersion(wc, id);
				break;

			case InstallAppCacheCallbackContext::INSTALL_UPDATE_REQUEST:
				wc->GetApplicationCacheListener()->CancelDownloadNewAppCacheVersion(wc, id);
				break;
			default:
				OP_ASSERT(!"UNKNOWN UI REQUEST TYPE");
				break;
		}
	}
}

void InstallAppCacheCallbackContext::OnDownloadAppCacheReply(BOOL allow_install)
{
	OP_ASSERT(m_request == INSTALL_REQUEST);

	OP_ASSERT(m_cache_group == NULL);
	OP_ASSERT(m_pass_cache_host_to_update_process == TRUE);

	if (allow_install && g_application_cache_manager && m_cache_host && m_request == INSTALL_REQUEST)
		RAISE_IF_ERROR(g_application_cache_manager->SetCacheHostManifest(m_cache_host, m_manifest_url, m_master_document_url));

	Out();  /* don't send cancel to this dialog */
	CancelDialogs();
	OP_DELETE(this);
}

void InstallAppCacheCallbackContext::OnCheckForNewAppCacheVersionReply(BOOL allow_checking_for_update)
{
	OP_ASSERT(m_request ==  CHECK_FOR_UPDATE_REQUEST);

	OP_ASSERT(m_cache_group != NULL);

	if (allow_checking_for_update && m_request == CHECK_FOR_UPDATE_REQUEST && g_application_cache_manager)
	{
		ApplicationCacheGroup* group = g_application_cache_manager->GetApplicationCacheGroupFromCacheHost(m_cache_host);

		OP_ASSERT(group == NULL || group == m_cache_group);
		if (g_application_cache_manager->CacheHostIsAlive(m_cache_host) && group)
			RAISE_IF_ERROR(g_application_cache_manager->ApplicationCacheDownloadProcess(m_pass_cache_host_to_update_process ? m_cache_host : NULL, m_manifest_url, m_master_document_url, m_cache_group));
	}
	Out();  /* don't send cancel to this dialog */
	CancelDialogs();
	OP_DELETE(this);
}

void InstallAppCacheCallbackContext::OnDownloadNewAppCacheVersionReply(BOOL allow_installing_update)
{
	OP_ASSERT(m_request ==  INSTALL_UPDATE_REQUEST);
	BOOL started_update = FALSE;

	if (allow_installing_update && m_request ==  INSTALL_UPDATE_REQUEST && g_application_cache_manager)
	{
		ApplicationCacheGroup* group = g_application_cache_manager->GetApplicationCacheGroupFromCacheHost(m_cache_host);

		if (g_application_cache_manager->CacheHostIsAlive(m_cache_host) && group)
		{
			OP_STATUS error = OpStatus::OK;
			if (OpStatus::IsSuccess(group->InitLoadingExplicitEntries()))
				started_update = TRUE;
			RAISE_IF_ERROR(error);
		}
}

	if (!started_update)
	{
		/* The cache was partially created, however, we didn't finish. We delete the cache, since it was not supposed to be installed. */
		ApplicationCacheGroup* group = g_application_cache_manager->GetApplicationCacheGroupFromCacheHost(m_cache_host);
		ApplicationCache *cache = group ? group->GetMostRecentCache() : NULL;
		if (cache && cache->IsComplete() == FALSE)
		{
			group->AbortUpdateProcess();
		}
	}
	Out();  /* don't send cancel to this dialog */
	CancelDialogs();
	OP_DELETE(this);
}

void InstallAppCacheCallbackContext::CancelDialogs()
{

	/* Here we must cancel all the other requests for the same manifest */

	g_application_cache_manager->CancelInstallDialogsForManifest(m_id);
}

void QuotaCallbackContext::OnQuotaReply(BOOL allow_increase, OpFileLength new_quota_size)
{
	int new_quota_size_kb = static_cast<int>(new_quota_size/1024);
	if (static_cast<OpFileLength>(new_quota_size_kb) * 1024 != new_quota_size) // an ad-hoc ceil function
	{
		new_quota_size_kb++;
	}

	OP_ASSERT((m_is_copying_from_previous_cache == FALSE && m_current_download_entry.IsValid()) || (m_is_copying_from_previous_cache == TRUE && m_current_download_entry.IsEmpty()));
	if (g_application_cache_manager->CacheHostIsAlive(m_cache_host))
	{
		if (allow_increase)
			m_cache->GetCacheGrup()->SetDiskQuotaAndContinueUpdate(new_quota_size_kb, m_is_copying_from_previous_cache, m_current_download_entry);
		else
			m_cache->GetCacheGrup()->CancelUpdateAlgorithm(m_current_download_entry); /* we need to cancel the update here */
	}

	Out();  // don't send cancel to this dialog
	g_application_cache_manager->CancelQuotaDialogsForManifest(m_id);

	OP_DELETE(this);
}

OP_STATUS ApplicationCacheManager::HandleCacheManifest(DOM_Environment *cache_host, const URL &manifest_url, URL &master_document_url)
{
	if (!cache_host || manifest_url.IsEmpty() || master_document_url.IsEmpty())
		return OpStatus::OK;

	/* Don't run application cache for privacy mode */
	FramesDocument *frames_document = cache_host->GetFramesDocument();

	if (frames_document->GetWindow()->GetPrivacyMode())
		return OpStatus::OK;

#ifdef WEB_TURBO_MODE
	/* For now ignore application cache for turbo-mode, as that needs to be integrated explicitly */
	if (master_document_url.GetAttribute(URL::KUsesTurbo))
	{
		return OpStatus::OK;
	}
#endif // WEB_TURBO_MODE

	/* If we don't have cache to disk, there is no point in using offline cache. */
	if (!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheToDisk))
	{
		return OpStatus::OK;
	}

	ApplicationCacheGroup *check_group = cache_host ? GetApplicationCacheGroupFromManifest(manifest_url) : NULL;
	if (check_group && check_group->IsPendingCacheHost(cache_host))
	{
		// For some reason HandleCacheManifest() was called twice for this cache host, and the update is already in progress. We can safely return.
		// Todo: Check why doc code call this function twice
		return OpStatus::OK;

	}


	/* Step 1. Optionally, wait until the permission to start the application cache download process has been obtained from the user */
	WindowCommander *wc = GetWindowCommanderFromCacheHost(cache_host);

	if (wc && wc->GetApplicationCacheListener() && !GetApplicationCacheGroupFromManifest(manifest_url))
	{
		// We haven't already installed this app.  Ask if we should.
		InstallAppCacheCallbackContext* context = OP_NEW(InstallAppCacheCallbackContext, (InstallAppCacheCallbackContext::INSTALL_REQUEST, cache_host, manifest_url, master_document_url, manifest_url.Id(), NULL, TRUE));
		if (!context)
			return OpStatus::ERR_NO_MEMORY;
		context->Into(&m_pending_install_callbacks);

		// TODO: use sof's new way of getting window commander
		wc->GetApplicationCacheListener()->OnDownloadAppCache(
				wc,
				static_cast<UINTPTR>(context->m_id),
				context);
	}
	else  // it's already installed, or we have no listener, so continue
	{
		RETURN_IF_ERROR(SetCacheHostManifest(cache_host, manifest_url, master_document_url));
	}


	return OpStatus::OK;
}

OP_STATUS ApplicationCacheManager::SetCacheHostManifest(DOM_Environment *cache_host, const URL &manifest_url, URL &master_document_url)
{
	/* 6.9.5 The application cache selection algorithm */

	OP_ASSERT(m_application_cache_folder != OPFILE_TEMP_FOLDER && "ApplicationCacheManager is not constructed properly");
	// Check if cache_host already has a manifest, and or cache.

	if (GetApplicationCacheFromCacheHost(cache_host))
		return OpStatus::ERR;


	ApplicationCache *doc_loaded_from_app_cache = GetCacheFromContextId(master_document_url.GetContextId());

	OpString manifest_str;
	RETURN_IF_ERROR(manifest_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, manifest_str));

	/* ->If there is a manifest URL, and document was loaded from an application cache,
	 * and the URL of the manifest of that cache's application cache group is not the same as manifest URL
	 */
	if (manifest_url.IsValid() && doc_loaded_from_app_cache && (!uni_str_eq(manifest_str.CStr(), doc_loaded_from_app_cache->GetCacheGrup()->GetManifestUrlStr())))
	{
		/* Mark the entry for the resource from which document was taken in the application cache from which it was loaded as foreign. */
		doc_loaded_from_app_cache->MarkAsForegin(master_document_url, TRUE);

		FramesDocument *frm_doc = cache_host->GetFramesDocument();
		Window *window = frm_doc ? frm_doc->GetWindow() : NULL;

		if (window)
		{
			window->Reload(window->GetURLCameFromAddressField() ? WasEnteredByUser: NotEnteredByUser, FALSE, FALSE);
			return OpStatus::OK;
		}

	/*
		 Restart the current navigation from the top of the navigation algorithm, undoing any changes that were made as
		 part of the initial load (changes can be avoided by ensuring that the step to update the session history
		 with the new page is only ever completed after this application cache selection algorithm is run, though this is not required).

		 Note: The navigation will not result in the same resource being loaded, because "foreign" entries are never picked during navigation.

		 ToDo:(DragonFly) User agents may notify the user of the inconsistency between the cache manifest and the document's own metadata,
		 to aid in application development. */
		return OpStatus::ERR;
	}
	/* -> If document was loaded from an application cache */
	else if (doc_loaded_from_app_cache)
	{
		/* Associate document with the application cache from which it was loaded.
		 * Invoke the application cache download process for that application cache's
		 * application cache group, with document as the cache host. */

		RETURN_IF_ERROR(doc_loaded_from_app_cache->AddCacheHostAssociation(cache_host));
		URL empty_url;

		return RequestApplicationCacheDownloadProcess(cache_host, empty_url, empty_url, doc_loaded_from_app_cache->GetCacheGrup());

	}
	/*  ->If document was loaded using HTTP GET or equivalent, and, there is a manifest URL, and manifest URL has the same origin as document */
	else if (
			master_document_url.GetAttribute(URL::KHTTP_Method, FALSE) == HTTP_METHOD_GET &&
			manifest_url.GetAttribute(URL::KServerName, NULL) == master_document_url.GetAttribute(URL::KServerName, NULL) &&
			manifest_url.GetAttribute(URL::KServerPort) == master_document_url.GetAttribute(URL::KServerPort) &&
			manifest_url.GetAttribute(URL::KType) == master_document_url.GetAttribute(URL::KType))
	{
		/* Invoke the application cache download process for manifest URL, with document
		 * as the cache host and with the resource from which document was parsed as the master resource.*/

		return ApplicationCacheDownloadProcess(cache_host, manifest_url, master_document_url, NULL);
	}
	/* ->Otherwise */
	else
	{
		/* ToDo:(DragonFly):(from spec) If there was a manifest URL, the user agent may report to the user that it was ignored, to aid in application development. */

		return OpStatus::OK;
	}
}

OP_STATUS ApplicationCacheManager::RequestApplicationCacheDownloadProcess(DOM_Environment *cache_host, const URL &manifest_url, URL &master_document_url, ApplicationCacheGroup *application_cache_group)
{
	/* 6.9.4 Downloading or updating an application cache */

	// If we have already installed the app, ask if we should update

	BOOL sent_event = FALSE;
	if (application_cache_group
#ifdef SELFTEST
		&& m_send_ui_events
#endif // SELFTEST
	)
	{
		// Ask if we should update this app
		sent_event = SendUpdateEventToAssociatedWindows(application_cache_group, FALSE, cache_host ? TRUE : FALSE);
	}

	if (sent_event)
		return OpStatus::OK;
	else
		return ApplicationCacheDownloadProcess(cache_host, manifest_url, master_document_url, application_cache_group);
}


OP_STATUS ApplicationCacheManager::RequestIncreaseAppCacheQuota(ApplicationCache *cache, const BOOL is_copying_from_previous_cache, const URL &current_download_entry)
{
	OP_ASSERT((is_copying_from_previous_cache == FALSE && current_download_entry.IsValid()) || (is_copying_from_previous_cache == TRUE && current_download_entry.IsEmpty()));
	/* Ask if we should increase the quota */

	/* create one cache host for each context */
	const OpVector<DOM_Environment> &cache_hosts = cache->GetCacheHosts();


	UINT32 count = cache_hosts.GetCount();
	UINT32 index;
	BOOL sent_event = FALSE;
	m_callback_received = FALSE;
	for (index = 0; index < count; index++)
	{
		if (m_callback_received)
			break;

		DOM_Environment *cache_host = cache_hosts.Get(index);
		WindowCommander *wc = GetWindowCommanderFromCacheHost(cache_host);
		if (!cache_host)
			continue;

		URL manifest_url = cache->GetCacheGrup()->GetManifestUrl();
		QuotaCallbackContext* context = OP_NEW(QuotaCallbackContext, (cache_host, manifest_url, current_download_entry, cache, is_copying_from_previous_cache, manifest_url.Id()));
		if (!context)
			return OpStatus::ERR_NO_MEMORY;

		context->Into(&g_application_cache_manager->m_pending_quota_callbacks);

		const ServerName *server = static_cast<const ServerName*>(manifest_url.GetAttribute(URL::KServerName, (void *) NULL,  URL::KNoRedirect));
		if (server)
		{
			if (wc && wc->GetApplicationCacheListener())
			{
				wc->GetApplicationCacheListener()->OnIncreaseAppCacheQuota(
						wc,
						context->m_id,
						server->UniName(),
						static_cast<OpFileLength>(cache->GetCacheGrup()->GetDiskQuota()) * 1024,
						context);
				sent_event = TRUE;
			}
		}
		else
			return OpStatus::ERR;
	}

	if (!sent_event)
	{
		OP_ASSERT(!"No listener to ask to increase quota");
	}

	return OpStatus::OK;
}

OP_STATUS ApplicationCacheManager::ApplicationCacheDownloadProcess(DOM_Environment *cache_host, const URL &manifest_url_identifier, URL &master_document_url, ApplicationCacheGroup *application_cache_group)
{
	OpAutoPtr<UpdateAlgorithmArguments> restart_arguments(OP_NEW(UpdateAlgorithmArguments, (cache_host, manifest_url_identifier, master_document_url, application_cache_group)));
	if (!restart_arguments.get())
		return OpStatus::ERR_NO_MEMORY;

	ApplicationCacheGroup *cache_group = NULL;
	URL manifest_url;

//	OP_ASSERT((master_document_url.IsValid() && cache_host) || (master_document_url.IsEmpty() && !cache_host));

	/* 6.9.4 Downloading or updating an application cache */

	/* Step 2. Atomically, so as to avoid race conditions, perform the following substeps:*/

	/* 2.1 Pick the appropriate substeps: */

	/* ->If these steps were invoked with an absolute URL purported to identify a manifest */
	if (manifest_url_identifier.IsValid())
	{
		/* Let manifest URL be that absolute URL. */
		manifest_url = manifest_url_identifier;

		OP_ASSERT(application_cache_group == NULL);

		/* If there is no application cache group identified by manifest URL, then create
		 * a new application cache group identified by manifest URL.
		 * Initially, it has no application caches. One will be created later in this algorithm. */
		if ((application_cache_group = GetApplicationCacheGroupFromManifest(manifest_url)) == NULL)
		{
			// No cache group for this manifest exists. We need to create a cache group
			RETURN_IF_ERROR(CreateApplicationCacheGroup(application_cache_group, manifest_url));
		}

	}
	/* ->If these steps were invoked with an application cache group */
	else if (application_cache_group)
	{
		/* Let manifest URL be the absolute URL of the manifest used to identify the application cache group to be updated. */
		manifest_url = application_cache_group->GetManifestUrl();
	}
	else
	{
		OP_ASSERT(!"Algorithm must be invoked using manifest_url or application_cache_group");
		return OpStatus::OK;
	}

	OP_ASSERT(application_cache_group->IsObsolete() == FALSE);

	/*2.2  "Let cache group be the application cache group identified by manifest URL." */
	cache_group = application_cache_group;

	/* Step 2.3 "If these steps were invoked with a master resource, then add the resource,
	 * along with the resource's Document, to cache group's list of pending master entries."*/
 	if (master_document_url.IsValid() && cache_host)
 	{
 		if (!cache_group->IsPendingMasterEntry(master_document_url))
 			RETURN_IF_ERROR(cache_group->AddPendingMasterEntry(master_document_url, cache_host));

 	}

	ApplicationCacheGroup::CacheGroupState update_state = cache_group->GetUpdateState();
	if (update_state == ApplicationCacheGroup::CHECKING || update_state == ApplicationCacheGroup::DOWNLOADING)
	{
		if (cache_host)
		{
			/* step 2.4*/
			cache_group->SendDomEvent(APPCACHECHECKING, cache_host);

			/* step 2.5*/
			if (update_state == ApplicationCacheGroup::DOWNLOADING)
			{
				cache_group->SendDomEvent(APPCACHEDOWNLOADING, cache_host);
			}
		}

		/* step 2.6
		 * The state is checking or downloading. Abort the cache update algorithm
		 */
		return OpStatus::OK;
	}

	/* Continuing on step 2.7. */
	cache_group->SetUpdateState(ApplicationCacheGroup::CHECKING);

	/* Step 2.8. */
	cache_group->SendDomEventToHostsInCacheGroup(APPCACHECHECKING);

	restart_arguments->Into(&m_update_algorithm_restart_arguments);

	/* The remainder of the steps run asynchronously */
	return cache_group->StartAnsyncCacheUpdate(cache_host, restart_arguments.release());
}

OP_STATUS ApplicationCacheManager::RemoveCacheHostAssociation(DOM_Environment *cache_host)
{
	if (cache_host)
	{
		ApplicationCache *cache = GetApplicationCacheFromCacheHost(cache_host);

		if (cache)
		{
			return cache->RemoveCacheHostAssociation(cache_host);
		}
	}
	return OpStatus::ERR;
}

URL_CONTEXT_ID ApplicationCacheManager::GetContextIdFromMasterDocument(const URL &master_document)
{
	if (master_document.IsEmpty())
		return 0;

	OpString master_str;
	if (OpStatus::IsError(master_document.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, master_str)))
		return 0;
	return GetContextIdFromMasterDocument(master_str.CStr());

}

URL_CONTEXT_ID ApplicationCacheManager::GetContextIdFromMasterDocument(const uni_char *master_document)
{
	ApplicationCache *cache = NULL;
	ApplicationCacheGroup *cache_group = NULL;

	URL_CONTEXT_ID context_id = 0;
	if (
			OpStatus::IsSuccess(GetApplicationCacheGroupMasterTable(master_document, &cache_group)) &&
			(cache = cache_group->GetMostRecentCache()) &&
			(context_id = cache->GetURLContextId())
		)
	{
		return context_id;
	}

	return 0;
}

URL_CONTEXT_ID ApplicationCacheManager::GetMostAppropriateCache(URL_Name_Components_st &resolved_name, const URL &parent_url)
{
	/*
	 * Implements "Get most appropriate cache" specified in
	 * http://www.whatwg.org/specs/web-apps/current-work/multipage/offline.html#concept-appcache-selection
	 *
	 * Multiple application caches in different application cache groups can contain the same resource,
	 * e.g. if the manifests all reference that resource. If the user agent is to select an application
	 * cache from a list of relevant application caches that contain a resource, the user agent must use
	 * the application cache that the user most likely wants to see the resource from, taking into
	 * account the following:
	 *
	 * -which application cache was most recently updated,
	 * -which application cache was being used to display the resource from which the user decided to look at the new resource
	 * -which application cache the user prefers.
	 *
	 * Which is called from  step 6.5.1 "Navigating across documents" in html5 specification.
	 */

	URLType url_type = resolved_name.scheme_spec ? resolved_name.scheme_spec->real_urltype : URL_UNKNOWN;

	if (url_type != URL_HTTP && url_type != URL_HTTPS)
		return 0;

	OpString entry_url_str;
	OP_STATUS status = GetURLStringFromURLNameComponent(resolved_name, entry_url_str);
	if (OpStatus::IsError(status))
	{
		g_memory_manager->RaiseCondition(status);
		return 0;
	}

	ApplicationCache *cache = GetApplicationCacheFromMasterOrManifest(entry_url_str);

	URL_CONTEXT_ID parent_id = parent_url.GetContextId();
	if (!cache && parent_id != 0)
		cache = GetCacheFromContextId(parent_id); // Check out the parent url. We may inherit the cache from the parent

	if (!cache)
	{
		ApplicationCache *cache_candiate = 0;
		// Check if there are any caches that contain this url.
		OpStatus::Ignore(m_cache_table_cached_url.GetData(entry_url_str.CStr(), &cache_candiate));
		if (cache_candiate)
		{
			// check origin. It must be the same as the manifest
			URL manifest_url = cache_candiate->GetManifest()->GetManifestUrl();
			if (
				cache_candiate->GetCacheGrup()->IsObsolete() == FALSE &&
				uni_strcmp(static_cast<const ServerName*>(manifest_url.GetAttribute(URL::KServerName, NULL))->UniName(), resolved_name.server_name->UniName()) == 0 &&
				manifest_url.GetAttribute(URL::KServerPort) == resolved_name.port &&
				static_cast<URLType>(manifest_url.GetAttribute(URL::KType)) == url_type)
			{
				cache = cache_candiate;
			}
		}
	}

	if (cache)
	{
		OP_ASSERT(cache->GetCacheGrup()->IsObsolete() == FALSE);
		if (cache->IsCached(entry_url_str.CStr()) == FALSE && cache->HasMasterURL(entry_url_str))
		{
			OP_ASSERT(!"Application cache url does not exist in cache. ");
			cache->SetObsolete();
		}
		else
		{
			URL_CONTEXT_ID context_id = cache->GetURLContextId();
			if (context_id != 0)
			{
				URL entry_url = g_url_api->GetURL(resolved_name.full_url, context_id); // Get the url from the cache to check if it's foreign

				if (entry_url.GetAttribute(URL::KIsApplicationCacheForeign) == FALSE)
					return context_id;
			}
		}
	}

	return 0;
}

URL_CONTEXT_ID ApplicationCacheManager::GetMostAppropriateCache(const uni_char *entry_url_str, const URL &parent_url)
{
	URL_Name_Components_st resolved_name;
	if (OpStatus::IsError(urlManager->GetResolvedNameRep(resolved_name, static_cast<URL>(parent_url).GetRep(), entry_url_str)))
		return 0;

	return GetMostAppropriateCache(resolved_name, parent_url);
}

ApplicationCache *ApplicationCacheManager::GetApplicationCacheFromMasterOrManifest(URL_Name_Components_st &entry_url)
{
	OpString entry_url_string;
	OP_STATUS status = GetURLStringFromURLNameComponent(entry_url, entry_url_string);
	if (OpStatus::IsError(status))
	{
		g_memory_manager->RaiseCondition(status);
		return 0;
	}
	return GetApplicationCacheFromMasterOrManifest(entry_url_string.CStr());
}

ApplicationCache *ApplicationCacheManager::GetApplicationCacheFromMasterOrManifest(const uni_char *entry_url)
{
	ApplicationCache *cache = GetApplicationCacheFromMasterDocument(entry_url, TRUE);

	if (!cache)
		cache = GetApplicationCacheFromManifest(entry_url); // See if this is a manifest url

	return cache;
}

ApplicationCache *ApplicationCacheManager::GetApplicationCacheFromManifest(const URL &manifest_url, BOOL must_be_complete)
{
	if (manifest_url.IsEmpty())
		return NULL;

	OpString manifest_str;
	if (OpStatus::IsError(manifest_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, manifest_str)))
		return NULL;

	return GetApplicationCacheFromManifest(manifest_str.CStr(), must_be_complete);
}

ApplicationCache *ApplicationCacheManager::GetApplicationCacheFromManifest(const uni_char *manifest_url, BOOL must_be_complete)
{
	ApplicationCacheGroup *cache_group = NULL;
	OpStatus::Ignore(m_cache_group_table_manifest_document.GetData(manifest_url, &cache_group));

	if (cache_group)
		return cache_group->GetMostRecentCache(must_be_complete);

	return NULL;
}

ApplicationCache *ApplicationCacheManager::GetApplicationCacheFromMasterDocument(const URL &master_document, BOOL must_be_complete)
{
	if (master_document.IsEmpty())
		return NULL;

	OpString master_str;
	if (OpStatus::IsError(master_document.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, master_str)))
		return NULL;

	return GetApplicationCacheFromMasterDocument(master_str.CStr(), must_be_complete);
}

ApplicationCache *ApplicationCacheManager::GetApplicationCacheFromMasterDocument(const uni_char *master_document, BOOL must_be_complete)
{
	ApplicationCacheGroup *cache_group = NULL;

	if (OpStatus::IsSuccess(GetApplicationCacheGroupMasterTable(master_document, &cache_group)))
		return cache_group->GetMostRecentCache(must_be_complete, master_document);
	return NULL;
}

ApplicationCache *ApplicationCacheManager::GetApplicationCacheFromCacheHost(DOM_Environment *cache_host) const
{
	ApplicationCache *cache = NULL;
	if (cache_host)
		OpStatus::Ignore(m_cache_table_frm.GetData(cache_host, &cache));
	return cache;
}

ApplicationCacheGroup *ApplicationCacheManager::GetApplicationCacheGroupFromCacheHost(DOM_Environment *cache_host)
{
	ApplicationCache *cache = NULL;
	if (cache_host)
	{
		if (OpStatus::IsSuccess(m_cache_table_frm.GetData(cache_host, &cache)))
		{
			return cache->GetCacheGrup();
		}
		else
		{
			/* go trough all cachegroups and find if any group has it as pending master */
			OpAutoPtr<OpHashIterator> iterator(m_cache_group_table_manifest_document.GetIterator());
			if (iterator.get() && OpStatus::IsSuccess(iterator->First()))
			{
				ApplicationCacheGroup *cache_group = NULL;
				do
				{
					cache_group = static_cast<ApplicationCacheGroup*>(iterator->GetData());
					if (cache_group->GetPendingCacheHost(cache_host))
						return cache_group;

				} while (OpStatus::IsSuccess(iterator->Next()));
			}

		}
	}
	return NULL;
}

ApplicationCache *ApplicationCacheManager::GetCacheFromContextId(URL_CONTEXT_ID cache_context_id) const
{
	if (cache_context_id == 0)
		return NULL;

	ApplicationCache *cache = NULL;
	OpStatus::Ignore(m_cache_table_context_id.GetData(cache_context_id, &cache));
	return cache;
}

ApplicationCacheGroup *ApplicationCacheManager::GetApplicationCacheGroupFromManifest(const URL &manifest_url)
{
	OpString manifest_str;
	if (OpStatus::IsError(manifest_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, manifest_str)))
		return NULL;

	return GetApplicationCacheGroupFromManifest(manifest_str.CStr());

}

ApplicationCacheGroup *ApplicationCacheManager::GetApplicationCacheGroupFromManifest(const uni_char *manifest_url)
{
	ApplicationCacheGroup *cache_group = NULL;
	if (OpStatus::IsSuccess(m_cache_group_table_manifest_document.GetData(manifest_url, &cache_group)))
		return cache_group;
	return NULL;
}

ApplicationCacheGroup *ApplicationCacheManager::GetApplicationCacheGroupMasterDocument(const URL &master_document)
{
	OpString master_str;
	if (OpStatus::IsError(master_document.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, master_str)))
		return NULL;

	return GetApplicationCacheGroupMasterDocument(master_str.CStr());
}

ApplicationCacheGroup *ApplicationCacheManager::GetApplicationCacheGroupMasterDocument(const uni_char *master_document)
{
	ApplicationCacheGroup *cache_group = NULL;
	if (OpStatus::IsSuccess(GetApplicationCacheGroupMasterTable(master_document, &cache_group)))
	{
		return cache_group;
	}

	return NULL;
}

OP_STATUS ApplicationCacheManager::DeleteApplicationCacheGroup(const uni_char *manifest_url)
{
	ApplicationCacheGroup *application_cache_group = GetApplicationCacheGroupFromManifest(manifest_url);
	if (!application_cache_group)
		return OpStatus::ERR;

	RETURN_IF_ERROR(application_cache_group->DeleteAllCachesInGroup());

	OP_STATUS status = m_cache_group_table_manifest_document.Remove(manifest_url, &application_cache_group);
	OP_DELETE(application_cache_group);
	RETURN_IF_ERROR(status);

	return SaveCacheState();
}

OP_STATUS ApplicationCacheManager::DeleteAllApplicationCacheGroups(BOOL keep_newest_caches_in_each_group)
{
	m_obsoleted_cache_groups.DeleteAll();

	OP_STATUS err = OpStatus::OK;

	/* save caches on disk -before- clearing the list if we want to keep the latest caches on disk*/
	if (keep_newest_caches_in_each_group)
		err = SaveCacheState(TRUE);

	m_cache_group_table_manifest_document.DeleteAll();

	/* save caches on disk -after- clearing the list if we -don't- want to keep the latest caches on disk*/
	if (!keep_newest_caches_in_each_group)
	{
		err = SaveCacheState(TRUE);
	}

	OP_ASSERT(m_cache_table_frm.GetCount() == 0);
	OP_ASSERT((!keep_newest_caches_in_each_group && m_cache_group_table_master_document.GetCount() == 0) || keep_newest_caches_in_each_group);
	OP_ASSERT(m_cache_table_context_id.GetCount() == 0);
	OP_ASSERT(m_cache_table_cached_url.GetCount() == 0);
	return err;
}

WindowCommander* ApplicationCacheManager::GetWindowCommanderFromCacheHost(DOM_Environment *cache_host) const
{
#ifdef SELFTEST
	if (!m_send_ui_events)
		return NULL;
#endif // SELFTEST

	if (!cache_host)
		return NULL;
	// TODO: use sof's new way of getting window commander
	FramesDocument *frames_doc = cache_host->GetFramesDocument();

	Window *window = frames_doc ? frames_doc->GetWindow() : NULL;
	WindowCommander *window_commander = window ? window->GetWindowCommander() : NULL;

	return window_commander;
}


OP_STATUS ApplicationCacheManager::GetAllApplicationCacheEntries(OpVector<OpWindowCommanderManager::OpApplicationCacheEntry>& all_app_caches)
{
	OpAutoPtr<OpHashIterator> iterator(m_cache_group_table_manifest_document.GetIterator());
	if (!iterator.get())
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS iterator_status;
	for (iterator_status = iterator->First(); OpStatus::IsSuccess(iterator_status); iterator_status = iterator->Next())
	{
		ApplicationCacheGroup *application_cache_group = static_cast<ApplicationCacheGroup*>(iterator->GetData());
		ApplicationCache *most_recent_complete_cache;
		if ((most_recent_complete_cache = application_cache_group->GetMostRecentCache(TRUE)) == NULL)
			continue;

		ExternalAPIAppCacheEntry* next_entry = OP_NEW(ExternalAPIAppCacheEntry, ());
		if (!next_entry)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS status = all_app_caches.Add(next_entry);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(next_entry);
			return status;
		}

		RETURN_IF_ERROR(next_entry->SetManifestURL(application_cache_group->GetManifestUrlStr()));
		URL manifest_url = application_cache_group->GetManifestUrl();
		RETURN_IF_ERROR(next_entry->SetDomain(manifest_url));


		UINT most_recent_cache_size_kb = most_recent_complete_cache->GetCurrentCacheSize();
		int current_disk_quota_kb = application_cache_group->GetDiskQuota();

		next_entry->SetApplicationCacheDiskQuota(static_cast<OpFileLength>(current_disk_quota_kb) * 1024);
		next_entry->SetApplicationCacheDiskSpaceUsed(static_cast<OpFileLength>(most_recent_cache_size_kb) * 1024);
	}
	return OpStatus::OK;
}

BOOL ApplicationCacheManager::CacheHostIsAlive(DOM_Environment *cache_host)
{
	if (GetApplicationCacheFromCacheHost(cache_host) || GetApplicationCacheGroupFromCacheHost(cache_host))
		return TRUE;
	else
		return FALSE;
}

void ApplicationCacheManager::CacheHostDestructed(DOM_Environment *cache_host)
{
	/* Remove all references to cache_host in any application cache */

	ApplicationCacheGroup *cache_group = GetApplicationCacheGroupFromCacheHost(cache_host);
	if (cache_group)
	{
		OpStatus::Ignore(cache_group->CacheHostDestructed(cache_host));
		OP_ASSERT(cache_group->GetPendingCacheHost(cache_host) == NULL);
	}

	int count = m_update_algorithm_restart_arguments.Cardinal();
	UpdateAlgorithmArguments* next_args = static_cast<UpdateAlgorithmArguments*>(m_update_algorithm_restart_arguments.First());

	while (next_args && count > 0)
	{
		UpdateAlgorithmArguments* args = next_args;
		next_args = static_cast<UpdateAlgorithmArguments*>(args->Suc());
		if (args->m_cache_host == cache_host)
		{
			args->Out();
			OP_DELETE(args);
			count--;
		}
	}
	InstallAppCacheCallbackContext* callback_context = static_cast<InstallAppCacheCallbackContext*>(m_pending_install_callbacks.First());
	while (callback_context)
	{
		InstallAppCacheCallbackContext* next_context = static_cast<InstallAppCacheCallbackContext*>(callback_context->Suc());
		if (callback_context->m_cache_host == cache_host)
		{
			WindowCommander *wc = GetWindowCommanderFromCacheHost(cache_host);
			CancelDialogForInstallContext(wc, callback_context);
			callback_context->Out();
			OP_DELETE(callback_context);
		}

		callback_context = next_context;
	}

	QuotaCallbackContext* quota_callback_context = static_cast<QuotaCallbackContext*>(m_pending_quota_callbacks.First());

	while(quota_callback_context)
	{
		QuotaCallbackContext *next_context = static_cast<QuotaCallbackContext*>(quota_callback_context->Suc());

		if (quota_callback_context->m_cache_host == cache_host)
		{
			WindowCommander *wc = GetWindowCommanderFromCacheHost(cache_host);
			wc->GetApplicationCacheListener()->CancelIncreaseAppCacheQuota(wc, quota_callback_context->m_id);
			quota_callback_context->Out();
			OP_DELETE(quota_callback_context);
		}
		quota_callback_context = next_context;
	}

	/* will also remove cache hosts from any caches in the obsolete cache groups, since the obosolete caches still are in m_cache_table_frm */
	OpStatus::Ignore(RemoveCacheHostAssociation(cache_host));
}

ApplicationCacheManager::ApplicationCacheNetworkStatus ApplicationCacheManager::CheckApplicationCacheNetworkModel(URL_Rep *url)
{
	/* Check if this resource should be loaded from the application cache given by the context id ( url->GetContextId() );
	 *
	 * The following algorithm follows the html5 specification 6.9.7 'changes to network model'.
	 * http://www.whatwg.org/specs/web-apps/current-work/multipage/offline.html#changesToNetworkingModel
	 * */
	/* When a cache host is associated with an application cache whose
	 * completeness flag is complete, any and all loads for resources related to
	 * that cache host other than those for child browsing contexts must go
	 * through the following steps instead of immediately invoking the
	 * mechanisms appropriate to that resource's scheme: */

	OP_ASSERT(url);

	if (!url)
		return NOT_APPLICATION_CACHE;

	URL_CONTEXT_ID context_id = url->GetContextId();

	ApplicationCache *cache = GetCacheFromContextId(context_id);

	if (!cache)
		return NOT_APPLICATION_CACHE;

	if (!cache->IsComplete())
		return LOAD_FROM_NETWORK_OBEY_NORMAL_CACHE_RULES;

	OpString url_name_str;

	if (OpStatus::IsMemoryError(url->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, URL::KNoRedirect, url_name_str)))
		return LOAD_FROM_APPLICATION_CACHE;

	const uni_char *url_name = url_name_str.CStr();
	/* Step 1. in the html5 specification 6.9.6 'changes to network model'
	 *
	 *Currently we only support HTTP here, although the spec opens up for other schemes
	 */
	if (url->GetAttribute(URL::KHTTP_Method) != HTTP_METHOD_GET || url->GetAttribute(URL::KType) != cache->GetCacheGrup()->GetManifestUrl().GetAttribute(URL::KType))
		return LOAD_FROM_NETWORK_OBEY_NORMAL_CACHE_RULES;

	/* Step 2. */
	if (cache->IsCached(url_name)) /* The url is in the manifest, and exists in the cache */
	{
		if (OpStatus::IsMemoryError(url->SetAttribute(URL::KHeaderLoaded, TRUE)))
			return LOAD_FROM_APPLICATION_CACHE;

		OP_ASSERT((URLStatus) url->GetAttribute(URL::KLoadStatus, URL::KNoRedirect) == URL_LOADED);
		OP_ASSERT(url->GetAttribute(URL::KDataPresent) == 1);

		return LOAD_FROM_APPLICATION_CACHE;
	}

	/* step 3. */
	if (cache->IsWithelisted(url_name))
	{
		OpStatus::Ignore(url->SetAttribute(URL::KTimeoutMaxTCPConnectionEstablished, NETTWORK_URLS_MAXIMUM_CONNECT_TIMEOUT));

		 /* The resource is whitelisted. We fetch the resource normally.*/
		return LOAD_FROM_NETWORK_OBEY_NORMAL_CACHE_RULES;
	}

	/* Step 4. */
	if (cache->MatchFallback(url_name))
	{
		URL fallback_url = url->GetAttribute(URL::KFallbackURL);
		if (fallback_url.IsValid())
		{
			// The url already has a fallback url. This means this is the second try. We load from cache.
			// We remove the fallback url, so that next time, the url will again be loaded from network

			URL empty_fallback;
			if (OpStatus::IsMemoryError(url->SetAttribute(URL::KFallbackURL, empty_fallback)))
				return LOAD_FROM_APPLICATION_CACHE;
		}

		OpStatus::Ignore(url->SetAttribute(URL::KTimeoutMaxTCPConnectionEstablished, FALLBACK_URLS_MAXIMUM_CONNECT_TIMEOUT));

		/* This url has a  fallback in case it fails. We fetch the resource normally on net first.
		 * If that fails, we fetch it from the fallback url (see above) */
		return RELOAD_FROM_NETWORK;
	}

	/* step 5. */
	if (cache->GetManifest()->IsOnlineOpen())
	{
		 /* This online whitelist flag is open. We fetch the resource normally. */
		return LOAD_FROM_NETWORK_OBEY_NORMAL_CACHE_RULES;
	}


	/* Step 6. :  Fail the resource load. */
	return LOAD_FAIL;
}

ApplicationCacheManager::ApplicationCacheNetworkStatus ApplicationCacheManager::CheckApplicationCacheNetworkModel(URL &url)
{
	return  CheckApplicationCacheNetworkModel(url.GetRep());
}

OP_STATUS ApplicationCacheManager::SaveCacheState(BOOL remove_cache_unused_contexts)
{
	OpFolderLister * tmp_folder
		= OpFile::GetFolderLister(m_application_cache_folder, UNI_L("*"));
	// NB: Brew's ADS compiler barfs on the above expression as folder_list's init :-(
	OpAutoPtr<OpFolderLister> folder_list(tmp_folder);
	if (!folder_list.get())
		return OpStatus::ERR_NO_MEMORY;

	OpAutoStringHashTable<OpString> cache_folders_to_delete;

	if (remove_cache_unused_contexts)
    {
		if (folder_list.get())
		{
			while (folder_list->Next())
			{
				/* put all folders in application cache folder into a data structure */
				if (folder_list->IsFolder())
				{
					const uni_char *folder = folder_list->GetFileName();
					OP_ASSERT(uni_strrchr(folder, '/') == NULL);

					/* folder should only be an hex string of 32 letters, such as
					 * '059c839d42e7e5458b4ff363a60b9df6', and contain no slashes. */

					if (uni_strlen(folder) != CACHE_FOLDER_NAME_LENGTH) /* To avoid the folders '..' and '.' */
						continue;

					if (uni_strrchr(folder, '/')) /* Needed, so that we don't risk deleting wrong folder in case of some serious bug */
						return OpStatus::ERR;

					OpString *folder_string;
					if (
							!(folder_string = OP_NEW(OpString, ())) ||
							OpStatus::IsError(folder_string->Set(folder_list->GetFileName())) ||
							OpStatus::IsError(cache_folders_to_delete.Add(folder_string->CStr(), folder_string))
						)
					{
						OP_DELETE(folder_string);
						return OpStatus::ERR_NO_MEMORY;
					}
				}
			}
			/* Below we will remove the folders from the cache_folder table, as soon as we find them in the cache state file.
			 * The remaining folders will be removed */
		}
		else
			return OpStatus::ERR_NO_MEMORY;
    }

	/* First we save the new caches that was used during this session */
	OpAutoPtr<OpHashIterator> manifest_iterator(m_cache_group_table_manifest_document.GetIterator());


	if (!manifest_iterator.get())
		return OpStatus::ERR_NO_MEMORY;

	XMLFragment cache_group_xml;
	RETURN_IF_ERROR(cache_group_xml.OpenElement(UNI_L("manifests"))); /* <manifests> */

	if (OpStatus::IsSuccess(manifest_iterator->First()))
	{
		ApplicationCacheGroup *cache_group;
		ApplicationCache* cache;
		Manifest *manifest;
		do
		{
			cache_group = static_cast<ApplicationCacheGroup*>(manifest_iterator->GetData());

			/* Only save the most recent cache that is complete*/
			if (!(cache = cache_group->GetMostRecentCache(TRUE)) || !(manifest = cache->GetManifest()))
			{
				continue;
			}

			OpAutoPtr<OpHashIterator> master_iterator(cache->GetMastersIterator());

			if (!master_iterator.get() || OpStatus::IsError(master_iterator->First()))
			{
				/* don't save caches with no masters */
				continue;
			}

			RETURN_IF_ERROR(cache_group_xml.OpenElement(UNI_L("manifest"))); /* <manifest> */

			RETURN_IF_ERROR(cache_group_xml.SetAttribute(UNI_L("url"), cache_group->GetManifestUrlStr())); /* url attribute */

			const uni_char *location = cache->GetApplicationLocation();
			OP_ASSERT(location);

			RETURN_IF_ERROR(cache_group_xml.SetAttribute(UNI_L("location"), location)); /* location  attribute*/

			RETURN_IF_ERROR(cache_group_xml.SetAttributeFormat(UNI_L("size_kb"), UNI_L("%u"), cache->GetCurrentCacheSize())); /* size_kb attribute*/

			if (cache_group->GetDiskQuota() >  g_pccore->GetIntegerPref(PrefsCollectionCore::DefaultApplicationCacheQuota))
			{
				/* only set the attribute if it differ from default */
				RETURN_IF_ERROR(cache_group_xml.SetAttributeFormat(UNI_L("diskquota_kb"), UNI_L("%d"), cache_group->GetDiskQuota())); /* diskquota_kb attribute*/
			}

			RETURN_IF_ERROR(cache_group_xml.OpenElement(UNI_L("masters"))); /* <masters> */

			if (OpStatus::IsSuccess(master_iterator->First()))
				do
				{
					RETURN_IF_ERROR(cache_group_xml.OpenElement(UNI_L("master"))); /* <master> */
					RETURN_IF_ERROR(cache_group_xml.SetAttribute(UNI_L("url"), static_cast<OpString*>(master_iterator->GetData())->CStr())); /* url attribute */
					cache_group_xml.CloseElement(); /* </master> */

				} while (OpStatus::IsSuccess(master_iterator->Next()));


			if (cache_folders_to_delete.Contains(location))
			{
				/* The cache existed in memory, so we delete them from the delete list */
				OpString *location_str = NULL;
				if (OpStatus::IsSuccess(cache_folders_to_delete.Remove(location, &location_str)))
					OP_DELETE(location_str);
			}

			cache_group_xml.CloseElement(); /* </masters> */
			cache_group_xml.CloseElement(); /* </group> */
		} while (OpStatus::IsSuccess(manifest_iterator->Next()));
	}
	cache_group_xml.CloseElement(); /* </manifests> */

	/* We save the xml to disk */
	TempBuffer buffer;
	RETURN_IF_ERROR(cache_group_xml.GetXML(buffer, TRUE, NULL, TRUE));

	OpFile file;
	RETURN_IF_ERROR(file.Construct(APPLICATION_CACHE_XML_FILE_NAME, GetCacheFolder()));
    RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
    RETURN_IF_ERROR(file.WriteUTF8Line(buffer.GetStorage()));
    RETURN_IF_ERROR(file.Close());


    /* We delete the folders with no references, that is: the folders left in cache_folders_to_delete. */
    if (remove_cache_unused_contexts)
    {
		folder_list.reset(OpFile::GetFolderLister(m_application_cache_folder, UNI_L("*")));
		if (folder_list.get())
		{
			while (folder_list->Next())
			{
				if (folder_list->IsFolder())
				{
					if (cache_folders_to_delete.Contains(folder_list->GetFileName()))
					{
						OpFile folder_to_delete;
						if (OpStatus::IsSuccess(folder_to_delete.Construct(folder_list->GetFileName(), m_application_cache_folder)))
						{
							OpStatus::Ignore(folder_to_delete.Delete(TRUE));
							OpStatus::Ignore(folder_to_delete.Close());
						}
					}
				}
			}
		}
		else
			return OpStatus::ERR_NO_MEMORY;
    }

    return OpStatus::OK;
}

OP_STATUS ApplicationCacheManager::LoadCacheState()
{
	OP_ASSERT(m_cache_state_loaded == FALSE);

	if (m_cache_state_loaded)
		return OpStatus::ERR;

	//	/* read XML-file */

	OpFile file;
	RETURN_IF_ERROR(file.Construct(APPLICATION_CACHE_XML_FILE_NAME, GetCacheFolder()));

	BOOL found;
	RETURN_IF_ERROR(file.Exists(found));
	if (found == FALSE)
	{
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	XMLFragment cache_group_xml;
	OP_STATUS status;
	int default_minimum_disk_quota = g_pccore->GetIntegerPref(PrefsCollectionCore::DefaultApplicationCacheQuota);

	if (OpStatus::IsSuccess(status = cache_group_xml.Parse(static_cast<OpFileDescriptor*>(&file))))
	{
		if (cache_group_xml.EnterElement(UNI_L("manifests")))
		{
			while (cache_group_xml.HasMoreElements())
			{
				if (cache_group_xml.EnterElement(UNI_L("manifest")))
				{
					const uni_char *manifest = cache_group_xml.GetAttribute(UNI_L("url"));
					const uni_char *location = cache_group_xml.GetAttribute(UNI_L("location"));
					const uni_char *disk_quota_string = cache_group_xml.GetAttribute(UNI_L("diskquota_kb"));
					const uni_char *size_kb_string = cache_group_xml.GetAttribute(UNI_L("size_kb"));

					int disk_quota = 0;
					if (disk_quota_string)
					{
						disk_quota = uni_atoi(disk_quota_string);
					}

					int cache_size = 0;
					if (size_kb_string)
					{
						cache_size = uni_atoi(size_kb_string);
					}

					if (manifest && location && cache_group_xml.EnterElement(UNI_L("masters")))
					{
						// FixMe: check manifest, master and location for illegal characters
						OpAutoPtr<UnloadedDiskCache> unloaded_disk_cache(OP_NEW(UnloadedDiskCache,()));
						if (!unloaded_disk_cache.get())
							return OpStatus::ERR_NO_MEMORY;

						if (disk_quota > default_minimum_disk_quota )
							unloaded_disk_cache->m_cache_disk_quota = disk_quota;
						else
							unloaded_disk_cache->m_cache_disk_quota = default_minimum_disk_quota;

						unloaded_disk_cache->m_cache_disk_size = cache_size;

						RETURN_IF_ERROR(unloaded_disk_cache->m_location.Set(location));
						RETURN_IF_ERROR(unloaded_disk_cache->m_manifest_url.Set(manifest));

						unloaded_disk_cache->m_manifest = g_url_api->GetURL(manifest, GetCommonManifestContextId());

						while (cache_group_xml.HasMoreElements())
						{
							if (cache_group_xml.EnterElement(UNI_L("master")))
							{
								const uni_char *master = cache_group_xml.GetAttribute(UNI_L("url"));
								if (master)
								{
									OpString *master_str = OP_NEW(OpString,());
									if (master_str == NULL)
										return OpStatus::ERR_NO_MEMORY;

									if (OpStatus::IsError(status = master_str->Set(master)))
									{
										OP_DELETE(master_str);
										return status;
									}
									if (OpStatus::IsError(status = unloaded_disk_cache->m_master_urls.Add(master_str)))
									{
										OpStatus::Ignore(unloaded_disk_cache->m_master_urls.RemoveByItem(master_str));
										OP_DELETE(master_str);
										return status;
									}
								}
							}
							else
							{
								cache_group_xml.EnterAnyElement();
							}
							cache_group_xml.LeaveElement();
						}

						RETURN_IF_MEMORY_ERROR(CreateApplicationCacheGroupUnloaded(unloaded_disk_cache.get()));
					}
					else
					{
						cache_group_xml.EnterAnyElement();
					}
					cache_group_xml.LeaveElement();
				}
				else
				{
					cache_group_xml.EnterAnyElement();
				}
				cache_group_xml.LeaveElement();
			}
		}
		else
		{
			cache_group_xml.EnterAnyElement();
		}
		cache_group_xml.LeaveElement();
	}
	else
	{
		RETURN_IF_MEMORY_ERROR(status);
	}
	m_cache_state_loaded = TRUE;
	return file.Close();
}

void ApplicationCacheManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_APPLICATION_CACHEGROUP_RESTART);

	if (msg == MSG_APPLICATION_CACHEGROUP_RESTART)
	{
		UpdateAlgorithmArguments *posted_arguments = reinterpret_cast<UpdateAlgorithmArguments*>(par1);
		DOM_Environment *posted_cache_host = reinterpret_cast<DOM_Environment*>(par2);

		/* Check that argument is still alive' */
		for (UpdateAlgorithmArguments* args = static_cast<UpdateAlgorithmArguments*>(m_update_algorithm_restart_arguments.First());
			args;
			args = static_cast<UpdateAlgorithmArguments*>(args->Suc()))
		{
			if (posted_arguments == args && posted_cache_host == args->m_cache_host )
			{
				ApplicationCacheGroup *cache_group = args->m_cache_group;

				if (cache_group)
					cache_group->SetRestartArguments(NULL);

				args->Out();

				if (args->m_owner)
					args->m_owner->SetRestartArguments(NULL);
				RAISE_IF_MEMORY_ERROR(ApplicationCacheDownloadProcess(args->m_cache_host, args->m_manifest_url, args->m_master_url, args->m_cache_group));
				g_main_message_handler->UnsetCallBack(this, MSG_APPLICATION_CACHEGROUP_RESTART, reinterpret_cast<MH_PARAM_1>(args));

				/* Important: delete the args after call to ApplicationCacheDownloadProcess, since ApplicationCacheDownloadProcess changes
				 * the content of m_update_algorithm_restart_arguments.
				 */
				OP_DELETE(args);

				break;
			}
		}
	}
}

void ApplicationCacheManager::CancelInstallDialogsForManifest(URL_ID id)
{
	m_callback_received = TRUE;
	InstallAppCacheCallbackContext* callback_context = static_cast<InstallAppCacheCallbackContext*>(m_pending_install_callbacks.First());

	while(callback_context)
	{
		InstallAppCacheCallbackContext* next_callback_context = static_cast<InstallAppCacheCallbackContext*>(callback_context->Suc());

		if (callback_context->m_id == id)
		{
			WindowCommander *wc = GetWindowCommanderFromCacheHost(callback_context->m_cache_host);
			CancelDialogForInstallContext(wc, callback_context);
			callback_context->Out();
			OP_DELETE(callback_context);
		}
		callback_context = next_callback_context;
	}
}

void ApplicationCacheManager::CancelQuotaDialogsForManifest(URL_ID id)
{
	m_callback_received = TRUE;
	QuotaCallbackContext* callback_context = static_cast<QuotaCallbackContext*>(m_pending_quota_callbacks.First());

	while(callback_context)
	{
		QuotaCallbackContext* next_callback_context = static_cast<QuotaCallbackContext*>(callback_context->Suc());

		if (callback_context->m_id == id)
		{
			WindowCommander *wc = GetWindowCommanderFromCacheHost(callback_context->m_cache_host);
			if (wc && wc->GetApplicationCacheListener())
			{
				wc->GetApplicationCacheListener()->CancelIncreaseAppCacheQuota(wc, callback_context->m_id);
			}
			callback_context->Out();
			OP_DELETE(callback_context);
		}
		callback_context = next_callback_context;
	}
}

void ApplicationCacheManager::CancelAllDialogsForWindowCommander(WindowCommander *wc)
{
	InstallAppCacheCallbackContext* callback_context = static_cast<InstallAppCacheCallbackContext*>(m_pending_install_callbacks.First());

	while(callback_context)
	{
		InstallAppCacheCallbackContext* next_callback_context = static_cast<InstallAppCacheCallbackContext*>(callback_context->Suc());
		WindowCommander *context_wc = GetWindowCommanderFromCacheHost(callback_context->m_cache_host);
		if (context_wc == wc)
		{
			CancelDialogForInstallContext(wc, callback_context);
			callback_context->Out();
			OP_DELETE(callback_context);
		}
		callback_context = next_callback_context;
	}

	QuotaCallbackContext* quota_callback_context = static_cast<QuotaCallbackContext*>(m_pending_quota_callbacks.First());
	while(quota_callback_context)
	{
		QuotaCallbackContext* next_quota_callback_context = static_cast<QuotaCallbackContext*>(quota_callback_context->Suc());
		WindowCommander *context_wc = GetWindowCommanderFromCacheHost(quota_callback_context->m_cache_host);
		if (context_wc && wc == context_wc && context_wc->GetApplicationCacheListener())
		{
			context_wc->GetApplicationCacheListener()->CancelIncreaseAppCacheQuota(wc, quota_callback_context->m_id);
			quota_callback_context->Out();
			OP_DELETE(quota_callback_context);
		}
		quota_callback_context = next_quota_callback_context;
	}
}


OP_STATUS ApplicationCacheManager::CreateApplicationCacheGroup(ApplicationCacheGroup *&application_cache_group, const URL &manifest_url)
{
	application_cache_group = NULL;
	ApplicationCacheGroup *temp_application_cache_group;

	RETURN_IF_ERROR(ApplicationCacheGroup::Make(temp_application_cache_group, manifest_url));
	OP_STATUS status;
	if (OpStatus::IsError(status = m_cache_group_table_manifest_document.Add(temp_application_cache_group->GetManifestUrlStr(), temp_application_cache_group)))
	{
		OP_DELETE(temp_application_cache_group);
		application_cache_group = NULL;
		return status;
	}

	application_cache_group = temp_application_cache_group;
	return OpStatus::OK;
}

OP_STATUS ApplicationCacheManager::CreateApplicationCacheGroupUnloaded(UnloadedDiskCache *unloaded_disk_cache)
{
	ApplicationCache *application_cache = NULL;
	ApplicationCacheGroup *application_cache_group = NULL;

	OP_STATUS status;
	if (
			OpStatus::IsError(status = CreateApplicationCacheGroup(application_cache_group, unloaded_disk_cache->m_manifest)) ||
			OpStatus::IsError(status = ApplicationCache::MakeFromDisk(application_cache, unloaded_disk_cache, application_cache_group))
		)
	{
		if (application_cache_group)
		{
			ApplicationCacheGroup *removed_group;
			OpStatus::Ignore(m_cache_group_table_manifest_document.Remove(application_cache_group->GetManifestUrlStr(), &removed_group));
			OP_ASSERT(removed_group == application_cache_group);
		}
		if (application_cache && application_cache_group)
		{
			OpStatus::Ignore(application_cache_group->RemoveCache(application_cache));
		}

		OP_DELETE(application_cache_group);

		OP_DELETE(application_cache);

		return status;
	}

	/* This overloads the default/preference disk quota set in ApplicationCache::Make(). */
	application_cache_group->m_cache_disk_quota = unloaded_disk_cache->m_cache_disk_quota;

	return OpStatus::OK;
}

ApplicationCacheManager::ApplicationCacheManager()
	: m_application_cache_folder(OPFILE_APP_CACHE_FOLDER)
	, m_cache_state_loaded(FALSE)
	, m_callback_received(FALSE)
	, m_common_manifest_context_id(0)
#ifdef SELFTEST
	, m_send_ui_events(TRUE)
#endif // SELFTEST


{
	OP_ASSERT(g_application_cache_manager == NULL);

}

OP_STATUS ApplicationCacheManager::Construct()
{
	OpFileFolder application_cache_folder;
	g_folder_manager->AddFolder(m_application_cache_folder, UNI_L("mcache"), &application_cache_folder);

	m_common_manifest_context_id = urlManager->GetNewContextID();

	RETURN_IF_LEAVE(urlManager->AddContextL(m_common_manifest_context_id
		, m_application_cache_folder, application_cache_folder, application_cache_folder, application_cache_folder
		, TRUE)); // We need to share cookies between all applications on the same server. Otherwise the user will
				  // have to log in over again each time the cache is updated.

	RETURN_IF_ERROR(LoadCacheState());
	return OpStatus::OK;
}

void ApplicationCacheManager::Destroy()
{
	g_application_cache_manager = NULL;
	g_main_message_handler->UnsetCallBacks(this);
	OP_DELETE(this);
}

BOOL ApplicationCacheManager::CacheGroupMasterTableContains(const uni_char *master_url_key)
{
	return m_cache_group_table_master_document.Contains(master_url_key);
}

OP_STATUS ApplicationCacheManager::GetApplicationCacheGroupMasterTable(const uni_char *master_url, ApplicationCacheGroup **group)
{
	*group = NULL;
	ApplicationCacheGroupKeyBundle *bundle;
	RETURN_IF_ERROR(m_cache_group_table_master_document.GetData(master_url, &bundle));
	*group = bundle->m_group;
	return OpStatus::OK;
}

OP_STATUS ApplicationCacheManager::AddApplicationCacheGroupMasterTable(const uni_char *master_url, ApplicationCacheGroup *group)
{
	OP_ASSERT(group);
	OpAutoPtr<ApplicationCacheGroupKeyBundle> bundle(OP_NEW(ApplicationCacheGroupKeyBundle, ()));
	if (!bundle.get())
		return OpStatus::ERR_NO_MEMORY;
	bundle->m_master_doucument_url_key.Set(master_url);
	bundle->m_group = group;

	RETURN_IF_ERROR(m_cache_group_table_master_document.Add(bundle->m_master_doucument_url_key.CStr(), bundle.get()));
	bundle.release();
	return OpStatus::OK;
}

OP_STATUS ApplicationCacheManager::RemoveApplicationCacheGroupMasterTable(const uni_char *master_url, ApplicationCacheGroup **group)
{
	*group = NULL;
	ApplicationCacheGroupKeyBundle *bundle;
	RETURN_IF_ERROR(m_cache_group_table_master_document.Remove(master_url, &bundle));
	*group = bundle->m_group;

	OP_DELETE(bundle);
	return OpStatus::OK;
}

BOOL ApplicationCacheManager::SendUpdateEventToAssociatedWindows(ApplicationCacheGroup* app_cache_group, BOOL request_continue_after_update_check, BOOL pass_cache_host_to_update_process)
{
#ifdef SELFTEST
	OP_ASSERT(m_send_ui_events == TRUE);
#endif
	URL empty_url;
	OpAutoPtr< OpVector<Window> > associated_windows(app_cache_group->GetAllWindowsAssociatedWithCacheGroup());

	if (!associated_windows.get())
		return FALSE;

	UINT32 windows_count = associated_windows->GetCount();

	BOOL sent_event = TRUE;
	m_callback_received  = FALSE;

	for (UINT index = 0; index < windows_count; index++)
	{

		/* If the event handler answers directly, m_callback_received will change
		 * to TRUE. That means we  must stop sending events, since we already
		 * have an answer.
		 */

		if (m_callback_received)
			break;

		Window *window = associated_windows->Get(index);
		FramesDocument *frm_doc = window->GetCurrentDoc();
		DOM_Environment *cache_host = frm_doc ? frm_doc->GetDOMEnvironment() : NULL;
		WindowCommander *wc = cache_host ? GetWindowCommanderFromCacheHost(cache_host) : NULL;

		URL empty_url;
		if (wc && !WindowHasReceivedEvent(cache_host))
		{

			InstallAppCacheCallbackContext::InstallRequest request;
			if (request_continue_after_update_check)
				request = InstallAppCacheCallbackContext::INSTALL_UPDATE_REQUEST;
			else
				request = InstallAppCacheCallbackContext::CHECK_FOR_UPDATE_REQUEST;

			InstallAppCacheCallbackContext* context = OP_NEW(InstallAppCacheCallbackContext, (request, cache_host, empty_url, empty_url, app_cache_group->GetManifestUrl().Id(), app_cache_group, pass_cache_host_to_update_process));
			if (!context)
			{
				CancelInstallDialogsForManifest(app_cache_group->GetManifestUrl().Id());
				return FALSE;
			}

			context->Into(&m_pending_install_callbacks);

			if (wc->GetApplicationCacheListener())
			{

				if (!request_continue_after_update_check)
				{
					wc->GetApplicationCacheListener()->OnCheckForNewAppCacheVersion(
						wc,
						static_cast<UINTPTR>(context->m_id),
						context);

				}
				else
				{
					wc->GetApplicationCacheListener()->OnDownloadNewAppCacheVersion(
						wc,
						static_cast<UINTPTR>(context->m_id),
						context);
				}

				sent_event = TRUE;
			}
		}
	}

	return sent_event;
}

BOOL ApplicationCacheManager::WindowHasReceivedEvent(const DOM_Environment *cache_host)
{
	Link *ch = m_pending_install_callbacks.First();

	if (ch) do
	{
		if (cache_host == static_cast<InstallAppCacheCallbackContext*>(ch)->m_cache_host)
			return TRUE;

	} while ((ch = ch->Suc()) != NULL);

	return FALSE;
}

BOOL ApplicationCacheManager::IsInline(const URL &parent_url)
{
	if (parent_url.IsValid())
	{
		URLType parent_url_type = (URLType) parent_url.GetAttribute(URL::KType);

		// If parent url is URL_OPERA or URL_NULL_TYPE then this is not an inline
		if (parent_url_type == URL_OPERA ||  parent_url_type == URL_NULL_TYPE)
			return FALSE;

		return TRUE;
	}

	return FALSE;
}

OP_STATUS ApplicationCacheManager::GetURLStringFromURLNameComponent(URL_Name_Components_st &resolved_name, OpString &url_string)
{
	int url_max_len = op_strlen(resolved_name.full_url) + 1;
	RETURN_OOM_IF_NULL(url_string.Reserve(url_max_len));
	resolved_name.OutputString(URL::KName_Escaped, url_string.CStr(), url_max_len);
	return OpStatus::OK;
}
#endif // APPLICATION_CACHE_SUPPORT
