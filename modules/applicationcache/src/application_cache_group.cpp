/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"


#ifdef APPLICATION_CACHE_SUPPORT

#include "modules/applicationcache/application_cache.h"
#include "modules/applicationcache/application_cache_manager.h"

#include "modules/applicationcache/src/application_cache_common.h"
#include "modules/applicationcache/application_cache_group.h"

#include "modules/dom/domenvironment.h"
#include "modules/doc/frm_doc.h"

#include "modules/url/url_man.h"
#include "modules/applicationcache/src/manifest_parser.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/util/adt/opvector.h"

#include "modules/prefs/prefs_module.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"


ApplicationCacheGroup::ApplicationCacheGroup()
	: m_attempt(CACHE_ATTEMPT)
	, m_cache_group_state(IDLE)
	, m_is_obsolete(FALSE)
	, m_cache_disk_quota(0)
	, m_manifest_parser(NULL)
	, m_urldd(NULL)
	, m_number_of_downloaded_url_pending_master_entries(0)
	, m_number_of_failed_pending_master_entries(0)
	, m_is_waiting_for_pending_masters(FALSE)
	, m_running_subroutine_while_waiting_pending_masters(NO_SUBROUTINE_RUNNING)
	, m_manifest_url_list_iterator(NULL)
	, m_current_donwload_size(0)
	, m_number_of_downloaded_urls(0)
	, m_timeout_ms(APPLICATION_CACHE_LOAD_TIMEOUT)
	, m_restart_arguments(NULL)
	, m_is_beeing_deleted(FALSE)
{
}

ApplicationCacheGroup::~ApplicationCacheGroup()
{
	OP_DELETE(m_manifest_parser);
	OP_DELETE(m_urldd);
	OP_DELETE(m_manifest_url_list_iterator);
	m_manifest_url_list_iterator = NULL;

	if (m_restart_arguments)
	{
		m_restart_arguments->m_owner = NULL;
		m_restart_arguments->m_cache_group = NULL;
		if (!m_restart_arguments->m_schedueled_for_start)
		{

			OP_ASSERT(m_restart_arguments->m_cache_group == NULL || m_restart_arguments->m_cache_group == this);
			m_restart_arguments->m_cache_group = NULL;

			m_restart_arguments->Out();
			OP_DELETE(m_restart_arguments);
			m_restart_arguments = NULL;
		}

	}

	UINT32 count = m_cache_list.GetCount();

	for (UINT32 index = 0; index < count; index++)
	{
		m_cache_list.Get(index)->RemoveAllMasterURLs();
		m_cache_list.Get(index)->m_application_cache_group = NULL; // Make sure that the application caches doesn't try to remove itself from this cache group when deleted.
	}

	g_main_message_handler->UnsetCallBacks(this);
}

/* static */ OP_STATUS ApplicationCacheGroup::Make(ApplicationCacheGroup *&application_cache_group, const URL &manifest_url)
{
	application_cache_group = NULL;

	OpAutoPtr<ApplicationCacheGroup> temp_cache_group(OP_NEW(ApplicationCacheGroup, ()));
	if (!temp_cache_group.get())
		return OpStatus::ERR_NO_MEMORY;

	OpString manifest_url_str;
	RETURN_IF_ERROR(manifest_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, manifest_url_str));

	if (OpStatus::IsError(temp_cache_group->m_manifest_url_str.Set(manifest_url_str)))
		return OpStatus::ERR_NO_MEMORY;

	temp_cache_group->m_manifest_url = manifest_url;

	temp_cache_group->m_timeout_handler.SetTimerListener(temp_cache_group.get());

	temp_cache_group->m_cache_disk_quota = g_pccore->GetIntegerPref(PrefsCollectionCore::DefaultApplicationCacheQuota);

	application_cache_group = temp_cache_group.release();

	return OpStatus::OK;

}

OP_STATUS ApplicationCacheGroup::AddCache(ApplicationCache *cache)
{
	OP_ASSERT(g_application_cache_manager->GetCacheFromContextId(cache->GetURLContextId()) == NULL);
	OP_ASSERT(m_cache_list.Find(cache) == (INT32)-1);

	if (g_application_cache_manager)
		RETURN_IF_ERROR(g_application_cache_manager->m_cache_table_context_id.Add(cache->GetURLContextId(), cache));

	OP_STATUS status;
	if (OpStatus::IsError(status = m_cache_list.Add(cache)) && g_application_cache_manager)
	{
		OpStatus::Ignore(g_application_cache_manager->m_cache_table_context_id.Remove(cache->GetURLContextId(), &cache));
	}
	cache->m_application_cache_group = this;

	return status;
}

OP_STATUS ApplicationCacheGroup::RemoveCache(ApplicationCache *cache)
{
	if (g_application_cache_manager && cache)
	{
		ApplicationCache *removed_cache;
		OpStatus::Ignore(g_application_cache_manager->m_cache_table_context_id.Remove(cache->GetURLContextId(), &removed_cache));
		OP_ASSERT(removed_cache == cache || removed_cache == NULL);
	}

	OP_STATUS status = OpStatus::OK;
	if (OpStatus::IsSuccess(status = m_cache_list.RemoveByItem(cache)))
	{
		cache->m_application_cache_group = NULL;
	}
	else
		return OpStatus::ERR;

	return status;
}

void ApplicationCacheGroup::DeleteCache(ApplicationCache *cache)
{
	if (cache)
	{
		cache->RemoveAllCacheHostAssociations();
		cache->RemoveAllMasterURLs();
		OpStatus::Ignore(RemoveCache(cache));
		cache->SetObsolete(); // Will cause the cache to be deleted in ~ApplicationCache()
		OP_DELETE(cache);
	}
}

OP_STATUS ApplicationCacheGroup::DeleteAllCachesInGroup(BOOL update_application_cache_index_file /* = TRUE */ , BOOL keep_newest_complete_cache /* = FALSE */)
{
	UINT32 number_of_caches_to_delete = m_cache_list.GetCount();
	if (number_of_caches_to_delete > 0)
	{

		if (keep_newest_complete_cache && m_cache_list.Get(number_of_caches_to_delete -1)->IsComplete())
			number_of_caches_to_delete--;

		while (number_of_caches_to_delete > 0)
		{
			DeleteCache(m_cache_list.Get(0)); // DeleteCache will also remove cache from  m_cache_list
			number_of_caches_to_delete--;
		}

		OP_ASSERT(
					(!keep_newest_complete_cache && m_cache_list.GetCount() == 0) ||
					(keep_newest_complete_cache && m_cache_list.Get(0)->IsComplete() && m_cache_list.GetCount() == 1 ) ||
					(keep_newest_complete_cache && !m_cache_list.Get(0)->IsComplete() && m_cache_list.GetCount() == 0 )
				 );
	}
	return g_application_cache_manager && update_application_cache_index_file
			?
			g_application_cache_manager->SaveCacheState()
			:
			OpStatus::OK;
}

ApplicationCache *ApplicationCacheGroup::GetMostRecentCache(BOOL must_be_complete, const uni_char *match_master_document) const
{
	UINT32 count = m_cache_list.GetCount();

	if (must_be_complete == FALSE && count > 0 && match_master_document == NULL && !m_cache_list.Get(count - 1)->IsObsolete())
	{
		return m_cache_list.Get(count - 1);
	}
	else
	{
		while (count > 0)
		{
			ApplicationCache *cache = m_cache_list.Get(count - 1);


			if (	!cache->IsObsolete() &&
					((!must_be_complete || cache->IsComplete()) &&
					(match_master_document == NULL || cache->HasMasterURL(match_master_document)))
				)
				return cache;

			--count;
		}
	}

	return NULL;
}

DOM_Environment *ApplicationCacheGroup::GetCurrentUpdateHost() const
{
	if (m_restart_arguments)
		return m_restart_arguments->m_cache_host;
	return NULL;
}

OP_STATUS ApplicationCacheGroup::StartAnsyncCacheUpdate(DOM_Environment *cache_host,  UpdateAlgorithmArguments *update_arguments)
{
	if (m_restart_arguments)
	{
		m_restart_arguments->Out();
		OP_DELETE(m_restart_arguments);
	}
	m_restart_arguments = update_arguments;
	m_restart_arguments->m_owner = this;

	ApplicationCache *cache = GetMostRecentCache(TRUE);

	if (cache)
		m_attempt = ApplicationCacheGroup::UPGRADE_ATTEMPT;
	else
		m_attempt = ApplicationCacheGroup::CACHE_ATTEMPT;

	/* Step 3 */
	if (m_attempt == ApplicationCacheGroup::CACHE_ATTEMPT)
	{
		/* If this is a cache attempt, then this algorithm was invoked with a cache host; */
		OP_ASSERT(cache_host);
		SendDomEvent(APPCACHECHECKING, cache_host);
	}

	// Try loading manifest and check if it's different
	UnRegisterAllLoadingHandlers();
	URL manifest_url = g_url_api->GetURL(m_manifest_url_str.CStr(), g_application_cache_manager->GetCommonManifestContextId());
	m_loading_manifest_url.SetURL(manifest_url);
	m_loading_manifest_url->Unload();

	UnRegisterLoadingHandlers(m_loading_manifest_url);

	const ServerName* server = static_cast<const ServerName*>(GetManifestUrl().GetAttribute(URL::KServerName, (void *)NULL,  URL::KNoRedirect));

	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode) && (!server || !server->GetIsLocalHost()))
	{
		LoadingManifestFailed(m_loading_manifest_url);
		return OpStatus::OK;
	}

	OP_STATUS status = OpStatus::OK;
	if (m_loading_manifest_url->IsEmpty() || OpStatus::IsError(status = RegisterLoadingHandlers(m_loading_manifest_url)))
	{
		RETURN_IF_MEMORY_ERROR(status);
		LoadingManifestFailed(m_loading_manifest_url);
		return OpStatus::OK;
	}
	URL referrer_url;

	URL_LoadPolicy loadpolicy(FALSE, URL_Reload_Full, FALSE);

	RETURN_IF_ERROR(m_loading_manifest_url->SetAttribute(URL::KTimeoutMaxTCPConnectionEstablished, NETTWORK_URLS_MAXIMUM_CONNECT_TIMEOUT));

	CommState state = m_loading_manifest_url->LoadDocument(g_main_message_handler, referrer_url, loadpolicy);
	m_manifest_loading_lock.SetURL(m_loading_manifest_url);
	m_timeout_handler.Start(m_timeout_ms);

	if (state != COMM_LOADING)
	{
		LoadingManifestFailed(m_loading_manifest_url);
		return OpStatus::OK;
	}
	/* The algorithm continues in ApplicationCacheGroup::handleCallback */
	return OpStatus::OK;
}

OP_STATUS ApplicationCacheGroup::InititateListenPendingMasterResources(UpdateAlgorithmSubRoutine subroutine)
{
	if (subroutine == RUNNING_CACHE_FAILURE_ALGORITHM)
	{
		StopLoading();
	}

	OP_ASSERT(m_running_subroutine_while_waiting_pending_masters == NO_SUBROUTINE_RUNNING);
	m_running_subroutine_while_waiting_pending_masters = subroutine;
	/* We must take care of the resources that was downloaded
	   before we started listening to the downloads If not start
	   listening on the downloads. */

	/* This function is run both for Step 7.3 and 22 */

	m_number_of_downloaded_url_pending_master_entries = 0;
	m_number_of_failed_pending_master_entries = 0;

	m_is_waiting_for_pending_masters = TRUE;
	if (m_pending_master_entries.Cardinal() > 0)
	{
		PendingMasterEntry *next_entry = NULL;
		PendingMasterEntry* entry = static_cast<PendingMasterEntry*>(m_pending_master_entries.First());
		m_timeout_handler.Start(m_timeout_ms);

		while (entry && !m_pending_master_entries.Empty())
		{
			/* entry might be deleted, so we get the next entry here */
			next_entry =  static_cast<PendingMasterEntry*>(entry->Suc());

			URLStatus status = entry->m_master_entry.Status(TRUE);
			if (status == URL_LOADING || status == URL_LOADING_FROM_CACHE)
			{
				/* We listen to the download of this entry */
				RegisterLoadingHandlers(entry->m_master_entry);
			}
			else
			{
				/* it's already loaded */
				RETURN_IF_ERROR(WaitingPendingMasterResources(entry));
			}

			entry = next_entry;
		}

		if (subroutine ==  RUNNING_MANIFEST_NOT_CHANGED_ALGORITHM && m_attempt == UPGRADE_ATTEMPT)
		{
			/* We associate the masters that already has been downloaded (last part of Step 7.3) */
			/* Loop through all pending masters and check if they are downloaded. */

			AssociatePendingMasterEntries(GetMostRecentCache(), TRUE);
		}
	}
	else
		return CheckLoadingPendingMasterResourcesDone();
	return OpStatus::OK;

}

OP_STATUS ApplicationCacheGroup::WaitingPendingMasterResources(PendingMasterEntry *current_pending_master_entry)
{
	URLStatus status = current_pending_master_entry->m_master_entry.Status(TRUE);

	/* Step 7.1  */
	ApplicationCache *new_cache = GetMostRecentCache();

	switch (status) {
		case URL_UNLOADED:
		case URL_LOADING_FAILURE:
		case URL_LOADING_ABORTED:
		{
			return LoadingPendingMasterFailed(current_pending_master_entry, new_cache);
		}
		case URL_LOADED:
		{
			OP_ASSERT(m_number_of_downloaded_url_pending_master_entries + m_number_of_failed_pending_master_entries < m_pending_master_entries.Cardinal());
			m_number_of_downloaded_url_pending_master_entries++;

			/* Step 7.3 */
			if (m_running_subroutine_while_waiting_pending_masters == RUNNING_MANIFEST_NOT_CHANGED_ALGORITHM)
			{
				OP_ASSERT(m_attempt == UPGRADE_ATTEMPT);
				OpString url_string;
				RETURN_IF_ERROR(current_pending_master_entry->m_master_entry.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, url_string));

				URL url_in_correct_cache;
				if (new_cache->GetURLContextId() != current_pending_master_entry->m_master_entry.GetContextId())
				{
					/* The pending master was downloaded to default cache or the previous cache. We must copy it to the new cache */
					RETURN_IF_ERROR(urlManager->CopyUrlToContext(current_pending_master_entry->m_master_entry.GetRep(), new_cache->GetURLContextId()));
					url_in_correct_cache = g_url_api->GetURL(url_string.CStr(), new_cache->GetURLContextId());

					FramesDocument *doc = current_pending_master_entry->m_cache_host->GetFramesDocument();
					if (doc && doc->GetURL().GetContextId() == 0)
					{
						// To make sure that the new master url is loaded from the correct cache on reload,
						// we force the document url over to the cache
						OP_ASSERT(current_pending_master_entry->m_master_entry.GetContextId() == 0);
						FramesDocument *doc = current_pending_master_entry->m_cache_host->GetFramesDocument();
						if (doc)
							OpStatus::Ignore(doc->SetNewUrl(url_in_correct_cache));
					}
				}
				else
				{
					url_in_correct_cache = current_pending_master_entry->m_master_entry;
				}

				url_in_correct_cache.SetAttribute(URL::KIsApplicationCacheURL, TRUE);

				OP_STATUS temp_status;
				OP_STATUS status = OpStatus::OK;

				if (url_in_correct_cache.Status(TRUE) == URL_LOADED && current_pending_master_entry->m_associated == FALSE)
				{
					if (
							OpStatus::IsError(temp_status = url_in_correct_cache.SetAttribute(URL::KCachePersistent, TRUE)) ||
							OpStatus::IsError(temp_status = new_cache->AddCacheHostAssociation(current_pending_master_entry->m_cache_host))  ||
							OpStatus::IsError(temp_status = new_cache->AddMasterURL(url_string.CStr()))
						)
					{
						if (OpStatus::IsMemoryError(status))
							status = temp_status;
						OpStatus::Ignore(new_cache->RemoveCacheHostAssociation(current_pending_master_entry->m_cache_host));
						if (url_string.HasContent())
							OpStatus::Ignore(new_cache->RemoveMasterURL(url_string.CStr()));
					}
					current_pending_master_entry->m_associated = TRUE;
					current_pending_master_entry->Out();
					OP_DELETE(current_pending_master_entry); current_pending_master_entry = NULL;

					RETURN_IF_ERROR(status);
				}
			}
			else if (m_running_subroutine_while_waiting_pending_masters == RUNNING_WAITING_PENDING_MASTERS)
			{
				OP_ASSERT(new_cache->IsComplete() == FALSE);
				/* Step 22. Success: Store the resource for this entry in new cache, if it isn't already there, and categorize its entry as a master entry. */
				UINT32 response_code = current_pending_master_entry->m_master_entry.GetAttribute(URL::KHTTP_Response_Code, FALSE);
				if (response_code == 200)
				{

					OP_ASSERT(current_pending_master_entry->m_master_entry.Status(FALSE) == URL_LOADED);
					OP_ASSERT(current_pending_master_entry->m_master_entry.GetAttribute(URL::KDataPresent));

					OpString pending_master_string;
					RETURN_IF_ERROR(current_pending_master_entry->m_master_entry.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, pending_master_string));

					/* The pending master was downloaded to default cache or the previous cache. We must copy it to the new cache */
					OP_STATUS status;
					RETURN_IF_MEMORY_ERROR(status = urlManager->CopyUrlToContext(current_pending_master_entry->m_master_entry.GetRep(), new_cache->GetURLContextId()));

					OP_ASSERT(OpStatus::IsSuccess(status) && "FATAL: something went wrong when copying a url from previous cache");

					URL master_url_in_new_cache = g_url_api->GetURL(pending_master_string, new_cache->GetURLContextId());
					master_url_in_new_cache.SetAttribute(URL::KIsApplicationCacheURL, TRUE);
					RETURN_IF_MEMORY_ERROR(master_url_in_new_cache.SetAttribute(URL::KCachePersistent, TRUE));

					OP_ASSERT(master_url_in_new_cache.Status(FALSE) == URL_LOADED);
					OP_ASSERT(master_url_in_new_cache.GetAttribute(URL::KDataPresent));
				}
			}

			return CheckLoadingPendingMasterResourcesDone();
		}

		default:
		case URL_LOADING_WAITING:
		case URL_LOADING:
		case URL_LOADING_FROM_CACHE:
			/* Nothing new happened return */
			return OpStatus::OK;
	}
}

OP_STATUS ApplicationCacheGroup::LoadingPendingMasterFailed(PendingMasterEntry *current_pending_master_entry, ApplicationCache *cache)
{
	OP_DELETE(m_manifest_url_list_iterator);
	m_manifest_url_list_iterator = NULL;

	OP_ASSERT(m_number_of_downloaded_url_pending_master_entries + m_number_of_failed_pending_master_entries < m_pending_master_entries.Cardinal());

	m_number_of_failed_pending_master_entries++;

	current_pending_master_entry->m_master_entry.Unload();

	if (m_running_subroutine_while_waiting_pending_masters == RUNNING_WAITING_PENDING_MASTERS)
	{
		/* Step 22, fails*/

		/* Step 22.1 */
		cache->RemoveCacheHostAssociation(current_pending_master_entry->m_cache_host);

		/* Step 22.2 no need to create a task list. We know in step 22.9 what to fire.*/

		/* Step 22.3 :If this is a cache attempt and this entry is the last
		 * entry in cache group's list of pending master entries, then run
		 * these further substeps: */
		if (m_attempt == CACHE_ATTEMPT && current_pending_master_entry->Suc() == NULL)
		{
			/* Step 22.3.1 Discard this cache group */
			m_cache_group_state = IDLE;

			SafeSelfDelete();

			/* Step 22.3.2, If appropriate, remove any user interface indicating that an update for this cache is in progress.*/
			/* FIXME: Remove any user interface */

			/* Step 22.3.3, Abort the application cache download process.*/
			AbortUpdateProcess();
			return OpStatus::OK;
		}
		else
		{
			/* 22.4, Otherwise, remove this entry from cache group's list of pending master entries. */
			current_pending_master_entry->Out();
			OP_DELETE(current_pending_master_entry);
		}
	}
	else if (m_running_subroutine_while_waiting_pending_masters == RUNNING_CACHE_FAILURE_ALGORITHM)
	{
		/* The events in cache failure steps 2.3 will be fired when all the documents has been loaded*/
	}

	return CheckLoadingPendingMasterResourcesDone();
}


OP_STATUS ApplicationCacheGroup::CheckLoadingPendingMasterResourcesDone(BOOL force)
{
	if (force == FALSE && m_number_of_downloaded_url_pending_master_entries + m_number_of_failed_pending_master_entries < m_pending_master_entries.Cardinal())
		return OpStatus::OK;

	m_timeout_handler.Stop();

	m_is_waiting_for_pending_masters = FALSE;

	UpdateAlgorithmSubRoutine subroutine_done = m_running_subroutine_while_waiting_pending_masters;
	m_running_subroutine_while_waiting_pending_masters = NO_SUBROUTINE_RUNNING;

	if (subroutine_done == RUNNING_MANIFEST_NOT_CHANGED_ALGORITHM && m_attempt == UPGRADE_ATTEMPT)
	{
		/* Step 7.3 to 7.7 */

		for (PendingMasterEntry* entry = static_cast<PendingMasterEntry*>(m_pending_master_entries.First());
			 entry;
			 entry = static_cast<PendingMasterEntry*>(entry->Suc()))
		{
			URLStatus status = entry->m_master_entry.Status(TRUE);
			if (status != URL_LOADED)
			{
				SendDomEvent(ONERROR, entry->m_cache_host);
			}
		}

		/* 7.4 */
		/* 	Then event is fired at step 7.8 */

		/* 7.5 */
		m_pending_master_entries.Clear();
		m_number_of_downloaded_url_pending_master_entries = 0;
		m_number_of_failed_pending_master_entries = 0;

		/* 7.6 */
		/* ToDo: remove user interface */

		/* 7.7 */
		m_cache_group_state = IDLE;

		/* Step 7.8 */
		SendDomEventToHostsInCacheGroup(APPCACHENOUPDATE);

		/* 7.9: abort the update process */
		AbortUpdateProcess();
		return OpStatus::OK;
	}
	else if (subroutine_done == RUNNING_WAITING_PENDING_MASTERS)
	{
		/* 22.9 : "For each task in task list, queue that task."
		 * No need to queue the task list. We simply creates and send the events here. */

		for (PendingMasterEntry* entry = static_cast<PendingMasterEntry*>(m_pending_master_entries.First());
			 entry;
			 entry = static_cast<PendingMasterEntry*>(entry->Suc()))
		{
			URLStatus status = entry->m_master_entry.Status(TRUE);
			if (status != URL_LOADED)
			{
				SendDomEvent(ONERROR, entry->m_cache_host);
			}
		}

		/* 23. Fetch the resource from manifest URL again, and let second manifest be that resource.  */

		ApplicationCache *cache = GetMostRecentCache();

		OP_ASSERT(cache && cache->IsComplete() == FALSE);

		URL manifest_url = g_url_api->GetURL(m_manifest_url_str.CStr(), cache ? cache->GetURLContextId() : g_application_cache_manager->GetCommonManifestContextId());
		m_loading_manifest_url.SetURL(manifest_url);
		UnRegisterAllLoadingHandlers();

		if (m_loading_manifest_url->IsEmpty() || OpStatus::IsError(RegisterLoadingHandlers(m_loading_manifest_url)))
		{
			LoadingManifestFailed(m_loading_manifest_url);
			return OpStatus::OK;
		}
		URL referrer_url;// = g_url_api->GetURL(m_master_document_url.CStr());

		URL_LoadPolicy loadpolicy(FALSE, URL_Reload_Conditional, FALSE);
		CommState state = m_loading_manifest_url->LoadDocument(g_main_message_handler, referrer_url, loadpolicy);
		m_manifest_loading_lock.SetURL(m_loading_manifest_url);

		m_timeout_handler.Start(m_timeout_ms);

		if (state != COMM_LOADING)
		{
			LoadingManifestFailed(m_loading_manifest_url);
			return OpStatus::OK;
		}
	}
	else if (subroutine_done == RUNNING_CACHE_FAILURE_ALGORITHM)
	{
		CacheFailure();
		if (m_restart_arguments && m_restart_arguments->m_schedueled_for_start)
		{
			ApplicationCacheManager *man = g_application_cache_manager;
			g_main_message_handler->SetCallBack(man, MSG_APPLICATION_CACHEGROUP_RESTART, reinterpret_cast<MH_PARAM_1>(m_restart_arguments));
			g_main_message_handler->PostDelayedMessage(MSG_APPLICATION_CACHEGROUP_RESTART, reinterpret_cast<MH_PARAM_1>(m_restart_arguments), reinterpret_cast<MH_PARAM_2>(m_restart_arguments->m_cache_host), 1000);
		}

	}

	return OpStatus::OK;
}

void ApplicationCacheGroup::LoadingManifestFailed(URL &manifest_url)
{
	/* Point 5 in 6.9.4-'Updating an application cache', in html5 */
	/* Check return code here */
	m_timeout_handler.Stop();
	UINT32 response_code = manifest_url.GetAttribute(URL::KHTTP_Response_Code);

	if (response_code == 404 || response_code == 410)
	{
		manifest_url.Unload();


		/* Step 5.2 : Let task list be an empty list of tasks. (We do not have to do that, since such a list is created in DOM  module, and fired on next message loop.)*/

		/* step 5.3 : For each cache host associated with an application cache in cache group, create a task to fire a simple event named obsolete. */
		SendDomEventToHostsInCacheGroup(APPCACHEOBSOLETE);

		/* step 5.4: For each entry in cache group's list of pending master entries, create a task to fire a simple event that is cancelable named error. */
		SendDomEventToPendingMasters(ONERROR);

		/* Step 5.1 */
		/* We set it to obsolete after sending the events, so that the events
		 * last will be sent. (we don't send events to obsolete cache groups)
		 */
		RAISE_IF_MEMORY_ERROR(SetObsolete()); // Fixme:handle error //

		/* step 5.5 */
		ApplicationCache *cache;
		UINT32 count = m_cache_list.GetCount();
		for (UINT32 index = 0; index < count; index++)
		{
			cache = m_cache_list.Get(index);

			if (cache->IsComplete() == FALSE)
			{
				DeleteCache(cache);
				index--;
				count--;
			}
		}

		/* step 5.6: to be implemented */
		/* FixMe: Send signal to UI to remove any cache update progress */

		/* Step 5.7 */
		m_cache_group_state = IDLE;

		/* Step 5. 8 : For each task in task list, queue that task.*/
		/* This is handled by the DOM/Ecmascript modules */

		/* Step 5.9 the update process is aborted */
		AbortUpdateProcess();
		return;
	}
	else
	{
		/* Step 6 : Otherwise, if fetching the manifest fails in some other way [..] then run the cache failure steps.*/
		LoadingManifestDone();
		RAISE_IF_MEMORY_ERROR(InititateListenPendingMasterResources(RUNNING_CACHE_FAILURE_ALGORITHM));
		return;
	}
}

void ApplicationCacheGroup::LoadingManifestDone()
{
	if (m_loading_manifest_url)
		m_loading_manifest_url->SetAttribute(URL::KIsApplicationCacheURL, TRUE);

	OP_DELETE(m_manifest_parser); m_manifest_parser = NULL;
	OP_DELETE(m_urldd); m_urldd = NULL;

	StopLoading();
}

OP_STATUS ApplicationCacheGroup::CreateNewCacheAndAssociatePendingMasterEntries(Manifest *manifest)
{
	/* This is a cache attempt, or the manifest has changed. We need to make a new cache in the group */

	/* Create a location for the cache */

	OpAutoPtr<Manifest> manifest_anchor(manifest);

	UINT8 random_string[CACHE_FOLDER_NAME_LENGTH * 2 + 2]; // ARRAY OK 2009-10-20 haavardm
	g_libcrypto_random_generator->GetRandom(random_string, CACHE_FOLDER_NAME_LENGTH * 2);

	for (int i = 0; i < CACHE_FOLDER_NAME_LENGTH * 2; i+=2)
	{
		random_string[i+1] = 0;
		random_string[i] &= 15;
		random_string[i] += random_string[i] < 10 ? '0' : 'a' - 10;
	}
	random_string[CACHE_FOLDER_NAME_LENGTH * 2] = 0;
	random_string[CACHE_FOLDER_NAME_LENGTH * 2 + 1] = 0;

	RETURN_IF_ERROR(m_current_cache_location.Set(reinterpret_cast<uni_char*>(random_string)));

	ApplicationCache *new_cache;
	RETURN_IF_ERROR(ApplicationCache::Make(new_cache, m_current_cache_location.CStr()));

	OP_STATUS status;
	if (OpStatus::IsError(status = AddCache(new_cache)))
	{
		OP_DELETE(new_cache);
		return status;
	}

	new_cache->TakeOverManifest(manifest_anchor.release());

	/* Step 8. */
	new_cache->SetCompletenessFlag(FALSE);

	/* Step 9. */
	return AssociatePendingMasterEntries(new_cache, FALSE);
}

void ApplicationCacheGroup::CacheFailure()
{
	m_timeout_handler.Stop();
	LoadingManifestDone();

	/* cache failure steps are as follows: */

	/* We skip Step 1. "Let task list be an empty list of tasks.". It's not needed, as we call this function after all entries has been loaded. */

	/* Step 2.1 :"Wait for the resource for this entry to have either completely downloaded or failed." : At this point all master entries has been loaded. */

	ApplicationCache *cache;
	for (PendingMasterEntry* entry = static_cast<PendingMasterEntry*>(m_pending_master_entries.First());
		 entry;
		 entry = static_cast<PendingMasterEntry*>(entry->Suc()))
	{
		OP_ASSERT(m_is_obsolete == FALSE);
		DOM_Environment* cache_host = entry->m_cache_host;

		/* Step 2.2 */
		if ((cache = GetMostRecentCache()))
			OpStatus::Ignore(cache->RemoveCacheHostAssociation(entry->m_cache_host)); // Deletes the entry

		/* Step 2.3 */
		SendDomEvent(ONERROR, cache_host);

	}

	/* step 3. */
	SendDomEventToHostsInCacheGroup(ONERROR);

	/* step 4: Empty cache group's list of pending master entries. */
	m_pending_master_entries.Clear();

	/* step 4. */
	/* To avoid putting this in the list we do this further down */

	/* step 5. If cache group has an application cache whose completeness flag is incomplete, then discard that application cache. */

	UINT32 count = m_cache_list.GetCount();
	for (UINT32 index = 0; index < count; index++)
	{
		cache = m_cache_list.Get(index);

		if (cache->IsComplete() == FALSE)
		{
			OP_DELETE(cache); // ~ApplicationCache() will remove the cache from all lists
			index--;
			count--;
		}
	}

	/* step 6 : If appropriate, remove any user interface indicating that an update for this cache is in progress. */
	/* FixMe */

	/* step 7 */
	m_cache_group_state = IDLE;


	/* step 8*/

	if (m_attempt == CACHE_ATTEMPT)
	{
		SafeSelfDelete();
		return;
	}

	/* Step 9.*/
	/* The events was fired above. This is ok though, since they will not actually be sent in this run of the message loop.*/

	/* Step 10. abort */
	AbortUpdateProcess();
}

/* static */ void ApplicationCacheGroup::StopLoadingUrl(const void *key, void *data)
{
	ApplicationCacheGroup::ManifestEntry* entry = static_cast<ApplicationCacheGroup::ManifestEntry*>(data);
	entry->StopLoading();
}

void ApplicationCacheGroup::StopLoading()
{
	m_cache_group_state = IDLE;
	UnRegisterAllLoadingHandlers();
	m_loading_manifest_url->StopLoading(g_main_message_handler);
	m_manifest_loading_lock.UnsetURL();

	m_timeout_handler.Stop();

	m_manifest_url_list.ForEach(&StopLoadingUrl);

	m_manifest_url_list.DeleteAll();
	OP_DELETE(m_manifest_url_list_iterator);
	m_manifest_url_list_iterator = NULL;


	m_running_subroutine_while_waiting_pending_masters = NO_SUBROUTINE_RUNNING;
}

void ApplicationCacheGroup::SafeSelfDelete()
{
	if (!m_is_beeing_deleted)
	{
		m_is_beeing_deleted = TRUE;
		StopLoading();
		g_main_message_handler->SetCallBack(this, MSG_APPLICATION_CACHEGROUP_DELETE, reinterpret_cast<MH_PARAM_1>(this));
		g_main_message_handler->PostMessage(MSG_APPLICATION_CACHEGROUP_DELETE, reinterpret_cast<MH_PARAM_1>(this), 0);
	}
}

void ApplicationCacheGroup::AbortUpdateProcess()
{
	if (m_restart_arguments && !m_restart_arguments->m_schedueled_for_start)
	{
		m_restart_arguments->Out();
		OP_DELETE(m_restart_arguments);
		m_restart_arguments = NULL;
	}
	m_timeout_handler.Stop();

	ManifestEntry *entry;
	if (m_manifest_url_list_iterator && (entry = static_cast<ManifestEntry*>(m_manifest_url_list_iterator->GetData())))
	{
		entry->StopLoading();
		OP_DELETE(m_manifest_url_list_iterator);
		m_manifest_url_list_iterator = NULL;
	}

	m_manifest_url_list.DeleteAll();

	ApplicationCache *cache = GetMostRecentCache();

	if (cache && !cache->IsComplete())
		DeleteCache(cache);

	if (m_cache_list.GetCount() == 0)
	{
		SafeSelfDelete();
	}

	m_cache_group_state = IDLE;
	LoadingManifestDone();
}

OP_STATUS ApplicationCacheGroup::AddPendingMasterEntry(const URL& master_document_url, DOM_Environment* cache_host)
{
	OP_ASSERT(GetPendingCacheHost(cache_host) == NULL);
	OP_ASSERT(GetPendingMasterDocument(master_document_url) == NULL);

	PendingMasterEntry* master_entry = OP_NEW(PendingMasterEntry, (master_document_url, cache_host));

	if (!master_entry)
		return OpStatus::ERR_NO_MEMORY;

	master_entry->Into(&m_pending_master_entries);

	return OpStatus::OK;
}

OP_STATUS ApplicationCacheGroup::AssociatePendingMasterEntries(ApplicationCache *cache, BOOL only_associate_loaded_entries)
{
	OP_ASSERT(cache);
	if (!cache)
		return OpStatus::ERR;

	OpString url_string;
	OP_STATUS status = OpStatus::OK;
	for (PendingMasterEntry* entry = static_cast<PendingMasterEntry*>(m_pending_master_entries.First());
		 entry;
		 entry = static_cast<PendingMasterEntry*>(entry->Suc()))
	{
		OP_STATUS temp_status = OpStatus::OK;

		if ((!only_associate_loaded_entries || entry->m_master_entry.Status(TRUE) == URL_LOADED) && entry->m_associated == FALSE)
		{
			if (
					(
						OpStatus::IsError(entry->m_master_entry.SetAttribute(URL::KCachePersistent, TRUE)) ||
						OpStatus::IsError(temp_status = cache->AddCacheHostAssociation(entry->m_cache_host))  ||
						OpStatus::IsError(temp_status = entry->m_master_entry.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, url_string)) ||
						OpStatus::IsError(temp_status = cache->AddMasterURL(url_string.CStr()))
					)
				)
			{
				if (OpStatus::IsMemoryError(status))
					status = temp_status;
				OpStatus::Ignore(cache->RemoveCacheHostAssociation(entry->m_cache_host));
				if (url_string.HasContent())
					OpStatus::Ignore(cache->RemoveMasterURL(url_string.CStr()));
			}
			else
			{
				entry->m_associated = TRUE;
			}
		}
	}
	return status;
}

BOOL ApplicationCacheGroup::IsPendingCacheHost(const DOM_Environment* cache_host) const
{
	return GetPendingCacheHost(cache_host) != NULL ? TRUE : FALSE;
}

OP_STATUS ApplicationCacheGroup::CacheHostDestructed(const DOM_Environment* cache_host)
{
	if (OpStatus::IsSuccess(DeletePendingCacheHost(cache_host)))
	{
		ApplicationCache *cache;
		if (m_cache_group_state != IDLE)
		{
			cache = GetMostRecentCache();
			int cache_host_count = 0;
			if (cache)
				cache_host_count = cache->GetCacheHosts().GetCount();

			if (
					cache &&
					m_pending_master_entries.Cardinal() == 0 &&
					// If there's no cache hosts associated, or the one associated cache host is descructed, abort update algorithm.
					(cache_host_count == 0 || (cache_host_count == 1 && cache_host == cache->GetCacheHosts().Get(0)))
				)
			{
				ManifestEntry *entry;
				if (m_manifest_url_list_iterator && (entry = static_cast<ManifestEntry*>(m_manifest_url_list_iterator->GetData())))
				{
					AbortUpdateProcess();
				}
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS ApplicationCacheGroup::DeletePendingCacheHost(const DOM_Environment* cache_host)
{
	PendingMasterEntry *entry;
	OP_STATUS status = OpStatus::ERR;
	while (cache_host && (entry = GetPendingCacheHost(cache_host)))
	{
		entry->Out();
		OP_DELETE(entry);
		status = OpStatus::OK;
	}
	return status;
}

BOOL ApplicationCacheGroup::IsPendingMasterEntry(const URL &master_entry) const
{
	return GetPendingMasterDocument(master_entry) != NULL ? TRUE : FALSE;
}

ApplicationCacheGroup::PendingMasterEntry *ApplicationCacheGroup::GetPendingMasterDocument(URL_ID url_id) const
{
	for (PendingMasterEntry* entry = static_cast<PendingMasterEntry*>(m_pending_master_entries.First());
		 entry;
		 entry = static_cast<PendingMasterEntry*>(entry->Suc()))
	{
		if (entry->m_master_entry.Id() == url_id)
			return entry;
	}

	return NULL;
}

ApplicationCacheGroup::PendingMasterEntry *ApplicationCacheGroup::GetPendingMasterDocument(const URL &master_entry) const
{
	for (PendingMasterEntry* entry = static_cast<PendingMasterEntry*>(m_pending_master_entries.First());
		 entry;
		 entry = static_cast<PendingMasterEntry*>(entry->Suc()))
	{
		if (entry->m_master_entry.Id() == master_entry.Id())
			return entry;
	}

	return NULL;
}

ApplicationCacheGroup::PendingMasterEntry *ApplicationCacheGroup::GetPendingCacheHost(const DOM_Environment* cache_host) const
{
	for (PendingMasterEntry* entry = static_cast<PendingMasterEntry*>(m_pending_master_entries.First());
		 entry;
		 entry = static_cast<PendingMasterEntry*>(entry->Suc()))
	{
		if (entry->m_cache_host == cache_host)
			return entry;
	}

	return NULL;
}


OP_STATUS ApplicationCacheGroup::LoadFromManifestURL(BOOL second_try)
{
	OP_STATUS status = OpStatus::OK;
	BOOL more;

	OP_ASSERT(!m_loading_manifest_url->IsEmpty());

	if (m_loading_manifest_url->Type() != URL_DATA && !m_loading_manifest_url->GetAttribute(URL::KHeaderLoaded, TRUE))
		return OpStatus::OK;

	// Get the datadescriptor and check if it contains data OK for parsing
	if (!m_urldd)
	{
		UINT32 http_response = 0;

		if (m_loading_manifest_url->Type() == URL_HTTP || m_loading_manifest_url->Type() == URL_HTTPS)
			http_response = m_loading_manifest_url->GetAttribute(URL::KHTTP_Response_Code, FALSE);
		else
			http_response = HTTP_OK;

		if (http_response == HTTP_NOT_MODIFIED)
		{
			/* continue on step 7.1 here */
			return InititateListenPendingMasterResources(RUNNING_MANIFEST_NOT_CHANGED_ALGORITHM);
		}


		if (http_response == 404 || http_response == 410)
		{
			LoadingManifestFailed(m_loading_manifest_url);
			return OpStatus::OK;
		}

		m_urldd = m_loading_manifest_url->GetDescriptor(g_main_message_handler, TRUE, FALSE, TRUE, NULL);

		if (!m_urldd)
		{
			LoadingManifestFailed(m_loading_manifest_url);
			return OpStatus::ERR_NO_MEMORY;
		}

		if (http_response != HTTP_OK && http_response != HTTP_NOT_MODIFIED || m_urldd->GetContentType() != URL_MANIFEST_CONTENT)
		{
			LoadingManifestFailed(m_loading_manifest_url);
			return OpStatus::OK;
		}

		if (second_try)
		{
			/* We save the manifest to a file on the second download */
			OpString manifest_file_name;
			RETURN_IF_ERROR(manifest_file_name.Set(m_current_cache_location.CStr()));
			RETURN_IF_ERROR(manifest_file_name.Append(UNI_L("/")));
			RETURN_IF_ERROR(manifest_file_name.Append(MANIFEST_FILENAME));

			RETURN_IF_ERROR(m_current_manifest_file.Construct(manifest_file_name.CStr(), g_application_cache_manager->GetCacheFolder()));
			RETURN_IF_ERROR(m_current_manifest_file.Open(OPFILE_WRITE));
		}
//
//		if (http_response == HTTP_NOT_MODIFIED)
//		{
//			return InititateListenPendingMasterResources();
//			// FIXME:
//		}
	}

	// Create the parser
	if (!m_manifest_parser)
	{
		if (OpStatus::IsError(status = ManifestParser::BuildManifestParser(m_loading_manifest_url, m_manifest_parser)))
		{
			RETURN_IF_MEMORY_ERROR(status);
			LoadingManifestFailed(m_loading_manifest_url);
			return OpStatus::OK;
		}
	}

	m_urldd->RetrieveDataL(more);

	if (second_try)
	{
		RETURN_IF_ERROR(m_current_manifest_file.Write(m_urldd->GetBuffer(), m_urldd->GetBufSize()));
	}

	unsigned consumed;
	if (OpStatus::IsError(status = m_manifest_parser->Parse((const uni_char *) m_urldd->GetBuffer(), m_urldd->GetBufSize() / sizeof(uni_char), !more, consumed)))
	{
		RETURN_IF_MEMORY_ERROR(status);
		LoadingManifestFailed(m_loading_manifest_url);
		return OpStatus::OK;
	}
	// TODO: what shall we do if `consumer' is less then the buffer size -- and this is quite possible case
	// as a matter of fact, a manifest parser consumes only complete lines (if it is not a last chunk of data)
	// we need to store a piece of data that was not parsed at all
	// it may be stored either of parser or on application cache size

	m_urldd->ConsumeData(consumed * sizeof(uni_char));

	if (!more && m_loading_manifest_url->Status(TRUE) == URL_LOADED)  // we're done
	{
		Manifest* tmp_manifest = NULL;
		if (OpStatus::IsError(status = m_manifest_parser->BuildManifest(tmp_manifest)))
		    return status;


		OpAutoPtr<Manifest> manifest (tmp_manifest);

		LoadingManifestDone();
		if (second_try == FALSE)
		{
			ApplicationCache* cache = GetMostRecentCache();
			OP_ASSERT((m_attempt == UPGRADE_ATTEMPT && cache) || (m_attempt == CACHE_ATTEMPT && !cache));
			Manifest *prev_manifest = cache ? cache->GetManifest() : NULL;

			/* Step 7. */
			if (
					m_attempt == UPGRADE_ATTEMPT &&
					!uni_strcmp(manifest->GetHash().CStr(), prev_manifest->GetHash().CStr())
				)
			{
				/* The newly downloaded manifest is the same as the previous one. Continues on 7.1 here */
				return InititateListenPendingMasterResources(RUNNING_MANIFEST_NOT_CHANGED_ALGORITHM);
			}
			else
			{
				/* This is a cache attempt, or the manifest has changed. We need to make a new cache in the group
				 * and associate the master entries.*/


				RETURN_IF_ERROR(CreateNewCacheAndAssociatePendingMasterEntries(manifest.release())); /* takes over ownership */

				ApplicationCache *cache = GetMostRecentCache();
				if (cache && m_loading_manifest_url->GetContextId() == g_application_cache_manager->GetCommonManifestContextId())
				{
					RETURN_IF_MEMORY_ERROR(urlManager->CopyUrlToContext(m_loading_manifest_url->GetRep(), cache->GetURLContextId()));
				}

				// Give callback to ask the user if he wants download the new version
				BOOL sent_events = FALSE;
				if (m_attempt == UPGRADE_ATTEMPT &&
#ifdef SELFTEST
					g_application_cache_manager->m_send_ui_events &&
#endif // SELFTEST
					g_application_cache_manager->SendUpdateEventToAssociatedWindows(this, TRUE, TRUE))
					sent_events = TRUE;

				if (!sent_events)
				{
					// Start downloading files from manifest (or copy to correct cache if we already have them)
					/* continue on step 10 here */
					return InitLoadingExplicitEntries();
				}
			}
		}
		else /* second_try == TRUE */
		{

			OpStatus::Ignore(m_current_manifest_file.Flush());
			OpStatus::Ignore(m_current_manifest_file.Close());

			OP_ASSERT(m_cache_list.GetCount() > 0);
			ApplicationCache* current_cache = GetMostRecentCache();
			OP_ASSERT(current_cache->IsComplete() == FALSE);
			Manifest *prev_manifest = NULL;
			if (current_cache && (prev_manifest = current_cache->GetManifest()))
			{
				if (!uni_strcmp(manifest->GetHash().CStr(), prev_manifest->GetHash().CStr())) // The manifest has not changed
				{
					current_cache->TakeOverManifest(manifest.release());
					RETURN_IF_ERROR(CacheUpdateFinished(current_cache));

				}
				else
				{
					/* 24. If the previous step failed for any reason,
					 * or if the fetching attempt involved a redirect,
					 * or if second manifest and manifest are not
					 * byte-for-byte identical, then schedule a rerun
					 * of the entire algorithm with the same parameters
					 * after a short delay, and run the cache failure steps. */

					if (m_restart_arguments)
						m_restart_arguments->m_schedueled_for_start = TRUE;

					/* m_schedueled_for_start = TRUE will ensure that the algorithm is restarted after
					 * cache failure steps have been run. */
					return InititateListenPendingMasterResources(RUNNING_CACHE_FAILURE_ALGORITHM);
				}
			}
		}
	}
	return OpStatus::OK;
}

OpFileLength ApplicationCacheGroup::CalculateApplicationCacheSize(OpHashIterator *entries_iterator, URL_CONTEXT_ID context_id)
{
	OpFileLength size = 0;
	URL_API *url_api = g_url_api;

	if (OpStatus::IsSuccess(entries_iterator->First())) do
	{
		OpString *url_string = static_cast<OpString*>(entries_iterator->GetData());
		if (!url_string || url_string->IsEmpty())
			continue;

		URL application_cached_url = url_api->GetURL(url_string->CStr(), context_id);
		size += application_cached_url.ContentLoaded();

	} while (OpStatus::IsSuccess(entries_iterator->Next()));

	return size;
}

/* Appart from the MSG_APPLICATION_CACHEGROUP_DELETE message, Parameter 1 MUST always be an url id. */

OP_STATUS ApplicationCacheGroup::OnDownloadingManifest(OpMessage msg)
{
	if (m_cache_group_state == CHECKING) // Downloading manifest first time
	{
		switch (msg)
		{
		case MSG_URL_LOADING_FAILED:
		case MSG_URL_MOVED:
			LoadingManifestFailed(m_loading_manifest_url);
			return OpStatus::OK;

		case MSG_NOT_MODIFIED:
		case MSG_URL_DATA_LOADED:
			{
				OP_STATUS status;
				RAISE_IF_MEMORY_ERROR(status = LoadFromManifestURL());

				if (OpStatus::IsError(status))
					LoadingManifestFailed(m_loading_manifest_url);
				break;
			}
		}
	}
	else if (m_cache_group_state == DOWNLOADING) // Downloading the manifest the second time (to check it hasn't changed while downloading entries)
	{
		switch (msg)
		{
		case MSG_URL_LOADING_FAILED:
		case MSG_URL_MOVED: /* redirect */
		{
			/* Step 24. */
			return InititateListenPendingMasterResources(RUNNING_CACHE_FAILURE_ALGORITHM);
			/* else: fall through to next case: */
		}

		case MSG_COMM_LOADING_FINISHED:
		case MSG_NOT_MODIFIED:
		case MSG_URL_DATA_LOADED:
		{
			OP_STATUS status;
			RAISE_IF_MEMORY_ERROR(status = LoadFromManifestURL(TRUE));

			if (OpStatus::IsError(status))
				LoadingManifestFailed(m_loading_manifest_url);

			break;
		}
		default:
			OP_ASSERT(!"unknown message");
			break;
		}


	}

	return OpStatus::OK;
}

OP_STATUS ApplicationCacheGroup::OnDownloadingManifestEntries(OpMessage msg, ManifestEntry *download_url, URLStatus loading_status)
{
	if (msg == MSG_URL_MOVED || (loading_status != URL_LOADING && loading_status != URL_LOADING_FROM_CACHE ))
	{
		m_number_of_downloaded_urls++;

		ApplicationCache *current_cache = GetMostRecentCache();
		OP_ASSERT(current_cache);
		UINT32 response_code = download_url->m_entry->GetAttribute(URL::KHTTP_Response_Code, FALSE);

		/* Step 17.4 */
		/* If the previous step fails (e.g. the server returns a 4xx or 5xx response
		 * or equivalent, or there is a DNS error, or the connection times out, or the
		 * user cancels the download), or if the server returned a redirect, then run
		 * the first appropriate step from the following list */
		if ((response_code >=400 && response_code < 600) || msg == MSG_URL_MOVED)
		{
			UnRegisterLoadingHandlers(download_url->m_entry);
			BOOL send_error_event = TRUE;
			/* If the URL being processed was flagged as an "explicit entry" or a "fallback entry" */
			if (download_url->explicit_or_fallback_entry)
			{
				return InititateListenPendingMasterResources(RUNNING_CACHE_FAILURE_ALGORITHM);

			}
			/* If the error was a 404 or 410 HTTP response or equivalent */
			else if (response_code == 404 || response_code == 410)
			{
				/* the resource is droppet from cache */
				download_url->m_entry->Unload();
			}
			else
			{
				/* Step 17.4 'otherwise' */
				/* Copy the resource and its metadata from the newest
				 * application cache in cache group whose completeness
				 * flag is complete, and act as if that was the fetched
				 * resource, ignoring the resource obtained from the
				 * network. */

				ApplicationCache *new_complete_cache = GetMostRecentCache(TRUE);
				OpString url_string;

				RETURN_IF_ERROR(download_url->m_entry->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, url_string));

				/* Remove it from the cache context */
				RETURN_IF_ERROR(download_url->m_entry->SetAttribute(URL::KUnique, TRUE));

				download_url->m_entry->Unload();
				OpStatus::Ignore(m_manifest_url_list.Remove(download_url->m_entry->GetRep(), &download_url));
				OP_DELETE(download_url);

				if (new_complete_cache && current_cache && url_string.HasContent())
				{
					URL prev_url = g_url_api->GetURL(url_string.CStr(), new_complete_cache->GetURLContextId());
					URLStatus prev_url_status = prev_url.Status(FALSE);

					if (prev_url_status == URL_LOADED)
					{
						/* Copy the url from previous complete cache */
						RETURN_IF_MEMORY_ERROR(urlManager->CopyUrlToContext(prev_url.GetRep(), current_cache->GetURLContextId()));
						send_error_event = FALSE;
						m_current_donwload_size += prev_url.ContentLoaded();
					}
				}

			}
			/* 17.2 : For each cache host associated with an application cache in cache group,
			 * queue a task to fire an event with the name progress... */
			/* In the spec the events are sent at start of loading for each file,
			 * and only 2 urls are started at the same time.
			 */
			if (send_error_event && download_url->explicit_or_fallback_entry)
				SendDomEventToHostsInCacheGroup(ONPROGRESS, TRUE, m_number_of_downloaded_urls, m_manifest_url_list.GetCount());

		}
		else if (loading_status == URL_LOADED)
		{
			m_current_donwload_size += download_url->m_entry->ContentLoaded();
			download_url->m_entry->SetAttribute(URL::KIsApplicationCacheURL, TRUE);

			download_url->m_entry->DumpSourceToDisk(TRUE); /* we force the url to cache disk */
			/* 17.2 */
			if (download_url->explicit_or_fallback_entry)
				SendDomEventToHostsInCacheGroup(ONPROGRESS, TRUE, m_number_of_downloaded_urls, m_manifest_url_list.GetCount());

			UnRegisterLoadingHandlers(download_url->m_entry);
		}
		else if (GetDiskQuota() != 0 && msg == MSG_URL_DATA_LOADED && m_current_donwload_size + download_url->m_entry->ContentLoaded() > static_cast<OpFileLength>(GetDiskQuota()) * 1024)
		{
			download_url->StopLoading();
			m_timeout_handler.Stop();
			OP_STATUS increase_status;
			if (OpStatus::IsError(increase_status = g_application_cache_manager->RequestIncreaseAppCacheQuota(current_cache, FALSE, download_url->m_entry)))
			{
				RETURN_IF_ERROR(InititateListenPendingMasterResources(RUNNING_CACHE_FAILURE_ALGORITHM));
				return increase_status;
			}
		}


		if (m_number_of_downloaded_urls >= m_manifest_url_list.GetCount())
		{
			/* Step 18.*/
			SendDomEventToHostsInCacheGroup(ONPROGRESS, TRUE, m_number_of_downloaded_urls, m_manifest_url_list.GetCount());

			/* The following steps are saved in the Manifest m_manifest object in the ApplicationCache *cache object. */
			/* 19. Store the URLs that form the new online whitelist in new cache.
			 * 21. Store the value of the new online whitelist wildcard flag in new cache.
			 */

			/* Step 22 in cache update algorithm: For each entry in
			 * cache group's list of pending master entries, wait for
			 * the resource for this entry to have either completely downloaded or failed. */
			return InititateListenPendingMasterResources(RUNNING_WAITING_PENDING_MASTERS);

		}
		else
		{
			/* There are more urls do load */
			if (OpStatus::IsError(LoadNextEntry(FALSE)))
				return InititateListenPendingMasterResources(RUNNING_CACHE_FAILURE_ALGORITHM);
			return OpStatus::OK;
		}
	}

	return OpStatus::OK;
}

/* Todo: split/refactor this function */
void ApplicationCacheGroup::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_APPLICATION_CACHEGROUP_DELETE && (INTPTR)this == (INTPTR)par1)
	{
		m_is_beeing_deleted = TRUE;
		OP_ASSERT(m_cache_group_state == IDLE);
		OpStatus::Ignore(DeleteAllCachesInGroup());
		ApplicationCacheManager *man = g_application_cache_manager;
		if (man)
			OpStatus::Ignore(man->SaveCacheState());

		AbortUpdateProcess();
		if (man)
		{
			ApplicationCacheGroup *group;
			if (IsObsolete())
				OpStatus::Ignore(g_application_cache_manager->m_obsoleted_cache_groups.Remove(m_manifest_url_str.CStr(), &group));
			else
				OpStatus::Ignore(g_application_cache_manager->m_cache_group_table_manifest_document.Remove(m_manifest_url_str.CStr(), &group));

			OP_ASSERT(group == this || group == NULL);
		}
		OP_DELETE(this);
		return;
	}
	OP_ASSERT(msg != MSG_APPLICATION_CACHEGROUP_DELETE);
	/* We restart the timeout */
	m_timeout_handler.Start(m_timeout_ms);

	BOOL is_manifest_file = FALSE;

	if ((URL_ID)par1 == m_loading_manifest_url->Id())
		is_manifest_file = TRUE;

	if (m_is_waiting_for_pending_masters)
	{
		OP_ASSERT(is_manifest_file == FALSE);
		PendingMasterEntry *entry = GetPendingMasterDocument((URL_ID)par1);
		if (entry)
		{
			/* Step 7.3 and step 22. */
			OP_STATUS err = WaitingPendingMasterResources(entry);
			if (OpStatus::IsError(err))
			{
				RAISE_IF_ERROR(err);
				AbortUpdateProcess();
			}
			return;
		}
	}

	if (is_manifest_file == TRUE) // Downloading manifest
	{

		OP_STATUS err = OnDownloadingManifest(msg);
		if (OpStatus::IsError(err))
		{
			RAISE_IF_ERROR(err);
			AbortUpdateProcess();
		}

		return;
	}

	ManifestEntry *download_url = NULL;
	URLStatus loading_status = URL_EMPTY;

	if (OpStatus::IsError(m_manifest_url_list.GetData(reinterpret_cast<URL_Rep*>(par1), &download_url)))
		return;
	loading_status =  download_url->m_entry->Status(FALSE);
	OP_ASSERT((URL_ID)par1 == download_url->m_entry->Id());

	if ((URL_ID)par1 != download_url->m_entry->Id())
		return;

	switch (msg)
	{
	case MSG_URL_LOADING_FAILED:
	case MSG_COMM_LOADING_FINISHED:
	case MSG_NOT_MODIFIED:
	case MSG_URL_DATA_LOADED:
	case MSG_URL_MOVED: /* redirect */
		{
			OP_STATUS err = OnDownloadingManifestEntries(msg, download_url, loading_status);
			if (OpStatus::IsError(err))
			{
				RAISE_IF_ERROR(err);
				AbortUpdateProcess();
			}
			break;
		}
	default:
		OP_ASSERT(!"unknown message");
		break;

	}
}

/* virtual */ void ApplicationCacheGroup::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer == &m_timeout_handler);
	if (m_cache_group_state == CHECKING)
	{
		OP_ASSERT(m_running_subroutine_while_waiting_pending_masters == NO_SUBROUTINE_RUNNING || m_running_subroutine_while_waiting_pending_masters == RUNNING_MANIFEST_NOT_CHANGED_ALGORITHM);
		/* Step 6. "Otherwise, if fetching the manifest fails in some
		 * ther way <...>  or the CONNECTION TIMES OUT.<..> then run the
		 * cache failure steps."*/
		m_running_subroutine_while_waiting_pending_masters = NO_SUBROUTINE_RUNNING;
		return CacheFailure();
	}
	else if (m_running_subroutine_while_waiting_pending_masters != NO_SUBROUTINE_RUNNING )
	{
		OP_ASSERT(m_cache_group_state == DOWNLOADING);
		/* No response from any the remaining pending masters for
		 * at least APPLICATION_CACHE_LOAD_TIMEOUT ms.
		 *
		 * We stop the remaing url, and run the appropriate failing algorithm
		 * according to  m_running_subroutine_while_waiting_pending_masters
		 * (handled by LoadingPendingMasterFailed).
		 */

		ApplicationCache *cache = GetMostRecentCache();

		OP_ASSERT(cache && cache->IsComplete() == FALSE);

		for (PendingMasterEntry* entry = static_cast<PendingMasterEntry*>(m_pending_master_entries.First());
			 entry;
			 entry = static_cast<PendingMasterEntry*>(entry->Suc()))
		{
			URL &url  = entry->m_master_entry;
			if (url.Status(TRUE) != URL_LOADED)
			{
				url.StopLoading(g_main_message_handler);
				/* This method handle choose and runs the correct failing steps
				 * depending on m_running_subroutine_while_waiting_pending_masters. */
				if (cache)
					RAISE_IF_MEMORY_ERROR(LoadingPendingMasterFailed(entry, cache));
			}
		}
		return;
	}

	CacheFailure();
	OP_ASSERT(!"NO REASON FOR TIMEOUT");
}

OP_STATUS ApplicationCacheGroup::CacheUpdateFinished(ApplicationCache *cache)
{

	if (m_restart_arguments)
	{
		m_restart_arguments->Out();
		OP_DELETE(m_restart_arguments);
		m_restart_arguments = NULL;
	}

	m_pending_master_entries.Clear();
	m_number_of_downloaded_url_pending_master_entries = 0;
	m_number_of_failed_pending_master_entries = 0;


	/* Step 25. Store manifest in new cache, if it's not there already, and categorize its entry as the manifest.*/
	/* Done: The file was stored in the cache context directory as a normal file when it was downloaded. */

	/* Step 26. Set the completeness flag of new cache to complete. */
	OP_ASSERT(cache->IsComplete() == FALSE);

	RETURN_IF_ERROR(cache->SetCompletenessFlag(TRUE));

	/* Step 27. "Let task list be an empty list of tasks." */
	/* We don't need a task list for events since the events actually will
	 * be fired on the following message loop calls to ecmascript module. See step 30. */

	/* Step 28. */
	if (m_attempt == CACHE_ATTEMPT)
		SendDomEventToHostsInCacheGroup(APPCACHECACHED);
	else // upgrade attempt
	{
		SendDomEventToHostsInCacheGroup(APPCACHEUPDATEREADY);
	}
	UnRegisterAllLoadingHandlers();

	/* 28. ToDo: If appropriate, remove any user interface indicating that an update for this cache is in progress. */

	/* 29. Set the update status of cache group to idle.*/
	m_cache_group_state = IDLE;

	cache->m_cache_disk_size =  static_cast<int>(m_current_donwload_size / 1024);

	/* 30. "For each task in task list, queue that task."
	 * Will be fired automatically by the ecmascript/dom module in
	 * next message loop calls to ecmascript module. */

	/* we save the cache state, so that is can be brought up again if opera closes */
	if (g_application_cache_manager)
		RETURN_IF_ERROR(g_application_cache_manager->SaveCacheState());

	Context_Manager *ctx_manager = urlManager->FindContextManager(cache->GetURLContextId());
	if (!ctx_manager)
		return OpStatus::ERR;

	RETURN_IF_LEAVE(ctx_manager->WriteCacheIndexesL(FALSE, FALSE));

	const OpVector<DOM_Environment> &cache_hosts = cache->GetCacheHosts();

	UINT32 count = cache_hosts.GetCount();
	UINT32 index;

	URL_CONTEXT_ID newest_cache_id = cache->GetURLContextId();

	if (m_attempt == CACHE_ATTEMPT)
	{
		for (index = 0; index < count; index++)
		{
			/* If the current cache context id for the base url for a document is 0, then it's not in application cache.
			 * If it's not in application cache and this is an cache attempt event, we switch to the application cache. */
			FramesDocument *frames_doc = cache_hosts.Get(index)->GetFramesDocument();
			OP_ASSERT(frames_doc);
			HLDocProfile* hld_profile = frames_doc ? frames_doc->GetHLDocProfile() : NULL;
			if (hld_profile)
			{
				URL base_url = *hld_profile->BaseURL();
				if (base_url.GetContextId() == 0)
				{
					OpStatus::Ignore(SwitchDocumentCache(cache_hosts.Get(index)->GetFramesDocument(), newest_cache_id));
				}
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS ApplicationCacheGroup::UpdateCache(DOM_Environment *cache_host)
{
	URL empty_url;
	/* important: Do not change the input parameters. The update algorithm behaves differently with different arguments */

	/*From spec:
	 * If the update() method is invoked, the user agent must invoke the application cache update process,
	 * in the background, for the application cache with which the ApplicationCache object's cache host is associated,
	 *  but without giving that cache host to the algorithm. If there is no such application cache, or if it is marked as obsolete,
	 *   then the method must raise an INVALID_STATE_ERR exception instead.
	 */

	return g_application_cache_manager->RequestApplicationCacheDownloadProcess(NULL, empty_url, empty_url, this);
}

OP_STATUS ApplicationCacheGroup::SwapCache(DOM_Environment *cache_host)
{
	URL_CONTEXT_ID id = 0;
	ApplicationCache* new_cache = GetMostRecentCache(TRUE);
	ApplicationCache *old_cache = g_application_cache_manager->GetApplicationCacheFromCacheHost(cache_host);


	if (!new_cache || IsObsolete() || !new_cache->IsComplete())
		return OpStatus::ERR;

	if (old_cache != new_cache)
	{
		if (old_cache)
			RETURN_IF_ERROR(old_cache->RemoveCacheHostAssociation(cache_host));
		RETURN_IF_ERROR(new_cache->AddCacheHostAssociation(cache_host));
		id = new_cache->GetURLContextId();
	}
	else
		return OpStatus::ERR;

	FramesDocument *doc = cache_host->GetFramesDocument(); /* can return NULL for webworkers. That's ok though */

	if (doc && id)
		return SwitchDocumentCache(doc, id);

	return OpStatus::OK;
}

void ApplicationCacheGroup::CancelUpdateAlgorithm(URL &current_download_url)
{
	if (current_download_url.IsValid())
	{
		ManifestEntry *current_download_url_entry = NULL;

		OpStatus::Ignore(m_manifest_url_list.GetData(current_download_url.GetRep(), &current_download_url_entry));
		OP_ASSERT(current_download_url_entry);

		if (current_download_url_entry)
			current_download_url_entry->StopLoading();
		else
			current_download_url.StopLoading(g_main_message_handler);
	}

	OP_DELETE(m_manifest_url_list_iterator);
	m_manifest_url_list_iterator = NULL;

	if (OpStatus::IsError(InititateListenPendingMasterResources(RUNNING_CACHE_FAILURE_ALGORITHM)))
		CacheFailure();
		/* we treat this as a cache failure.
		This call will stop the algorithm and set
		the correct states and send error events. */
}

void ApplicationCacheGroup::SetDiskQuotaAndContinueUpdate(int new_disk_quota, BOOL is_copying_from_previous_cache, URL &current_download_entry)
{
	OP_ASSERT((is_copying_from_previous_cache == FALSE && current_download_entry.IsValid()) || (is_copying_from_previous_cache == TRUE && current_download_entry.IsEmpty()));

	OP_ASSERT(new_disk_quota > m_cache_disk_quota);
	m_cache_disk_quota = new_disk_quota;

	if (is_copying_from_previous_cache)
	{
		OP_STATUS status;
		if (OpStatus::IsError(status = InitLoadingExplicitEntries()))
		{
			CancelUpdateAlgorithm(current_download_entry);
			RAISE_IF_MEMORY_ERROR(status);
			return;
		}
	}
	else
	{
		OP_ASSERT(m_cache_group_state == ApplicationCacheGroup::DOWNLOADING);
		URL referrer_url;
		CommState state = current_download_entry.ResumeLoad(g_main_message_handler, referrer_url);
		if (state != COMM_LOADING && state != COMM_REQUEST_FINISHED)
		{
			URL_LoadPolicy loadpolicy(FALSE, URL_Reload_Conditional, FALSE);
			state = current_download_entry.LoadDocument(g_main_message_handler, referrer_url, loadpolicy);
		}

		if (state != COMM_LOADING && state != COMM_REQUEST_FINISHED)
		{
			OP_ASSERT(!"SHOULD BE LOADING HERE");
			CancelUpdateAlgorithm(current_download_entry);
			return;
		}
		m_timeout_handler.Start(m_timeout_ms);
		return;
	}
}

OpVector<Window> *ApplicationCacheGroup::GetAllWindowsAssociatedWithCacheGroup()
{
	OpAutoPtr< OpVector<Window> > associated_windows(OP_NEW(OpVector<Window>,()));

	if (associated_windows.get() == NULL)
		return NULL;

	UINT32 cache_count = m_cache_list.GetCount();

	for (UINT32 i = 0; i < cache_count; i++)
	{
		OpVector<DOM_Environment> m_cache_hosts;
		const OpVector<DOM_Environment> &cache_hosts = m_cache_list.Get(i)->GetCacheHosts();

		UINT32 cache_host_count = cache_hosts.GetCount();
		for (UINT32 j = 0; j< cache_host_count; j++)
		{
			DOM_Environment *inv = 	cache_hosts.Get(j);

			Window *window = inv->GetFramesDocument() ? inv->GetFramesDocument()->GetWindow() : NULL;

			if (window)
			{
				if (OpStatus::IsError(associated_windows->Add(window)))
					return NULL;
			}
		}
	}

	return associated_windows.release();
}

OP_STATUS ApplicationCacheGroup::InitLoadingExplicitEntries()
{
	m_number_of_downloaded_urls = 0;

	ApplicationCache *new_cache = GetMostRecentCache();
	OP_ASSERT(new_cache);
	Manifest *manifest = new_cache->GetManifest();
	OP_ASSERT(manifest != NULL);

	if (!new_cache || !manifest)
		return OpStatus::ERR;

	OpAutoPtr<OpHashIterator> urls_to_download_iterator(manifest->GetCacheIterator());
	URL_CONTEXT_ID context_id = new_cache->GetURLContextId();

	/* We need to check size urls already downloaded to the zero context
	 * (The normal cache, before it was discovered this was an applicationcache)
	 */

	if (m_attempt == CACHE_ATTEMPT)
	{
		OpFileLength size = CalculateApplicationCacheSize(urls_to_download_iterator.get(), 0);

		if (size > static_cast<OpFileLength>(GetDiskQuota()) * 1024)
		{
			return g_application_cache_manager->RequestIncreaseAppCacheQuota(new_cache, TRUE);
		}
	}

	// update algorithm step 10
	m_cache_group_state = DOWNLOADING;
	// update algorithm step 11
	SendDomEventToHostsInCacheGroup(APPCACHEDOWNLOADING);

	OP_ASSERT(context_id != 0 && context_id != (URL_CONTEXT_ID) -1);

	/* Step 12. in 6.9.4 Updating an application cache in HTML5. "Let file list be an empty list of URLs with flags." */
	m_manifest_url_list.DeleteAll();
	OP_ASSERT(m_manifest_url_list_iterator == NULL);
	OP_DELETE(m_manifest_url_list_iterator);
	m_manifest_url_list_iterator = NULL;

	ApplicationCache *newest_comple_cache = GetMostRecentCache(TRUE);

	URL_CONTEXT_ID prev_context_id =
			(m_attempt == UPGRADE_ATTEMPT) ?
			newest_comple_cache->GetURLContextId()
			:
			0;

	/* Step 13. and 14.
	 * 13. Add all the URLs in the list of explicit entries obtained by parsing manifest to file list, each flagged with "explicit entry".
	 * 14. Add all the URLs in the list of fallback entries obtained by parsing manifest to file list, each flagged with "fallback entry".  */
	if (urls_to_download_iterator.get())
	{
		/* 17. 1 and 17.2 */
		RETURN_IF_ERROR(AddAndMergeEntries(urls_to_download_iterator.get(), prev_context_id, context_id, FALSE));
	}

	/* Step 15 Add all the masters in the newest cache that is complete */
	/* Step 16. If any URL is in file list more than once, then merge the entries into one entry for that URL, that entry having all the flags that the original entries had."
	 * Step 15 and 16 happens in AddEntries */
	/* 17. 1 and 17.2 */
	if (newest_comple_cache && m_attempt == UPGRADE_ATTEMPT)
	{
		urls_to_download_iterator.reset(newest_comple_cache->GetMastersIterator());
		RETURN_IF_ERROR(AddAndMergeEntries(urls_to_download_iterator.get(), prev_context_id, context_id, TRUE));
	}

	/* Step 17. "For each URL in file list, run the following steps. These steps may be run in parallel for two or more of the URLs at a time."
	 *
	 * The url code will now take care of the loading of the urls.
	 * Step 17.3 to  17.9 is handled in ApplicationCacheGroup::handleCallback
	 * */
	if (m_manifest_url_list.GetCount() > 0)
	{
		m_timeout_handler.Start(m_timeout_ms);
		OP_ASSERT(m_manifest_url_list_iterator == NULL);
		m_manifest_url_list_iterator = m_manifest_url_list.GetIterator();

		if (m_manifest_url_list_iterator == NULL)
			return OpStatus::ERR_NO_MEMORY;

		/* We start loading the first url */
		RETURN_IF_ERROR(LoadNextEntry(TRUE)); // Todo: proper error_handling

	}
	else
	{
		/* no urls in the list, we continue the algorithm on step 22. */
		SendDomEventToHostsInCacheGroup(ONPROGRESS, TRUE, 0, 0);
		return InititateListenPendingMasterResources(RUNNING_WAITING_PENDING_MASTERS);

	}

	/* continues in ApplicationCacheGroup::handleCallback. */
	return OpStatus::OK;
}

OP_STATUS ApplicationCacheGroup::AddAndMergeEntries(OpHashIterator *entries_iterator, URL_CONTEXT_ID prev_context_id, URL_CONTEXT_ID new_context_id, BOOL master_entries)
{
	OP_ASSERT(new_context_id != prev_context_id);
	URL_API *url_api = g_url_api;

	if (OpStatus::IsSuccess(entries_iterator->First())) do
	{
		OpString *url_string = static_cast<OpString*>(entries_iterator->GetData());
		if (!url_string || url_string->IsEmpty())
			continue;

		URL prev_url = url_api->GetURL(url_string->CStr(), prev_context_id);
		UINT32 prev_url_http_status = prev_url.GetAttribute(URL::KHTTP_Response_Code, FALSE);
		if (prev_url_http_status == 0)
		{
			/* This seems to happen even if prev_url_status is URL_LOADED. Doesn't make sense*/
			/* OP_ASSERT(prev_url_http_status != URL_LOADED); */
			prev_url.Unload();
			prev_url = url_api->GetURL(url_string->CStr(), prev_context_id);
			prev_url_http_status = prev_url.GetAttribute(URL::KHTTP_Response_Code, FALSE);
		}
		URLStatus prev_url_status = prev_url.Status(FALSE);

		UINT32 prev_url_has_data = prev_url.GetAttribute(URL::KDataPresent);
		URL new_url;
		BOOL is_loaded = FALSE;
		if (prev_url_status == URL_LOADED && prev_url_has_data && prev_url_http_status != 404 && prev_url_http_status != 410 && prev_url_http_status != 0)
		{
			/* we copy urls from the previous caches, so that expire headers for these urls will be respected */
			OP_STATUS status;
			OP_ASSERT(prev_url.GetAttribute(URL::KDataPresent) == prev_url_has_data);

			RETURN_IF_MEMORY_ERROR(status = urlManager->CopyUrlToContext(prev_url.GetRep(), new_context_id));

			OP_ASSERT(prev_url.GetAttribute(URL::KDataPresent) == prev_url_has_data);

			new_url = url_api->GetURL(url_string->CStr(), new_context_id);

			if (OpStatus::IsError(status))
			{
				/* Something went wrong in the copying */
				prev_url_status = URL_UNLOADED;
				prev_url_has_data = FALSE;
				new_url.Unload();
			}
			else
				is_loaded = TRUE;
		}

		if (!is_loaded)
			new_url = url_api->GetURL(url_string->CStr(), new_context_id );

		if (is_loaded)
		{
			OP_ASSERT(new_url.GetAttribute(URL::KDataPresent) == prev_url.GetAttribute(URL::KDataPresent));
			OP_ASSERT(new_url.IsValid());
			OP_ASSERT(new_url.Status(FALSE) == URL_LOADED);
			OP_ASSERT(new_url.GetContextId() == new_context_id);
			OP_ASSERT(new_url.GetAttribute(URL::KDataPresent) == prev_url_has_data);
			/* don't add master entries that already are loaded into the default cache
			 * to the loading list, since we know these have just been loaded */
			if (master_entries && prev_context_id == 0)
			{
				m_current_donwload_size += new_url.ContentLoaded();
				continue;

			}
			/* Non master entries, and all entries loaded from a previous application cache on
			 *  the other hand might have been loaded a long time ago so these needs to be checked */
		}


		ManifestEntry *download_url = NULL;

		if (OpStatus::IsError(m_manifest_url_list.GetData(new_url.GetRep(), &download_url))) //We use url->URL_Rep as id;
		{
			download_url = OP_NEW(ManifestEntry, (new_url, this));
			if (!download_url || OpStatus::IsMemoryError(m_manifest_url_list.Add(new_url.GetRep(), download_url)))
			{
				OP_DELETE(download_url);
				return OpStatus::ERR_NO_MEMORY;
			}

			OpStatus::Ignore(new_url.SetAttribute(URL::KCachePersistent, TRUE));
		}

		ApplicationCache *new_cache = GetMostRecentCache();
		OP_ASSERT(new_cache);

		if (master_entries)
		{
			download_url->manifest_entry = TRUE;
			OP_STATUS err;
			RAISE_IF_MEMORY_ERROR(err = new_cache->AddMasterURL(url_string->CStr()));
			OP_ASSERT(err != OpStatus::ERR); // Should not have been added previously
		}
		else
			download_url->explicit_or_fallback_entry = TRUE;
	} while (OpStatus::IsSuccess(entries_iterator->Next()));

	return OpStatus::OK;
}

OP_STATUS ApplicationCacheGroup::LoadNextEntry(BOOL first_entry)
{
	OP_ASSERT(m_manifest_url_list_iterator);
	if (!m_manifest_url_list_iterator)
		AbortUpdateProcess();

	if (first_entry)
	{
		m_current_donwload_size = 0;
		RETURN_IF_ERROR(m_manifest_url_list_iterator->First());
	}
	else
		RETURN_IF_ERROR(m_manifest_url_list_iterator->Next());

	ManifestEntry *entry = static_cast<ManifestEntry*>(m_manifest_url_list_iterator->GetData());
	OP_STATUS status;
	if (OpStatus::IsError(status = RegisterLoadingHandlers(entry->m_entry)) || OpStatus::IsError(status = entry->Load()))
	{
		UnRegisterLoadingHandlers((entry->m_entry));
		return status;
	}

	m_timeout_handler.Start(m_timeout_ms);
	return OpStatus::OK;
}

OP_STATUS ApplicationCacheGroup::RegisterLoadingHandlers(const URL &url)
{
	static const OpMessage messages[] = {
		MSG_NOT_MODIFIED,
		MSG_URL_DATA_LOADED,
		MSG_URL_LOADING_FAILED,
		MSG_URL_MOVED,
	};

	return g_main_message_handler->SetCallBackList(this, url.Id(TRUE), messages, ARRAY_SIZE(messages));
}

void ApplicationCacheGroup::UnRegisterLoadingHandlers(const URL &url)
{
	g_main_message_handler->RemoveCallBacks(this, url.Id());
}

void ApplicationCacheGroup::UnRegisterAllLoadingHandlers()
{
	g_main_message_handler->UnsetCallBacks(this);
	g_main_message_handler->SetCallBack(this, MSG_APPLICATION_CACHEGROUP_DELETE, reinterpret_cast<MH_PARAM_1>(this));
}

OP_STATUS ApplicationCacheGroup::SetObsolete()
{
	m_is_obsolete = TRUE;
	ApplicationCacheManager *man = g_application_cache_manager;
	if (man)
	{
		if (m_cache_list.GetCount()  > 0)
			RETURN_IF_ERROR(man->m_obsoleted_cache_groups.Add(GetManifestUrlStr(), this));
		else
			SafeSelfDelete();

		/* Remove the cache group from the tables m_cache_group_table_master_document and
		 * m_cache_group_table_manifest_document, and put it into the m_obsoleted_cache_groups
		 * to die a certain death. */
		ApplicationCacheGroup *removed_cache;
		OP_STATUS removed;
		OpStatus::Ignore(removed = man->m_cache_group_table_manifest_document.Remove(GetManifestUrlStr(), &removed_cache));
		OP_ASSERT(this == removed_cache);
		OP_ASSERT(OpStatus::IsSuccess(removed));
	}

	UINT32 size = m_cache_list.GetCount();
	for (UINT32 index = 0; index < size; index++)
	{
		m_cache_list.Get(index)->SetObsolete();
		m_cache_list.Get(index)->RemoveOwnerCacheGroupFromManager();
	}

	return OpStatus::OK;
}


void ApplicationCacheGroup::SendDomEvent(DOM_EventType event_type, DOM_Environment* cache_host, BOOL lengthComputable, OpFileLength loaded, OpFileLength total)
{
	if (cache_host && !m_is_obsolete)
	{
		RAISE_IF_MEMORY_ERROR(cache_host->SendApplicationCacheEvent (event_type, lengthComputable, loaded, total));
	}
}

void ApplicationCacheGroup::SendDomEventToPendingMasters(DOM_EventType  event_type, BOOL lengthComputable, OpFileLength loaded, OpFileLength total)
{
	if (m_is_obsolete)
		return;

	for (PendingMasterEntry* entry = static_cast<PendingMasterEntry*>(m_pending_master_entries.First());
		 entry;
		 entry = static_cast<PendingMasterEntry*>(entry->Suc()))
	{
		SendDomEvent(event_type, entry->m_cache_host, lengthComputable, loaded, total);
	}
}

void ApplicationCacheGroup::SendDomEventToHostsInCacheGroup(DOM_EventType event_type, BOOL lengthComputable, OpFileLength loaded, OpFileLength total)
{
	if (m_is_obsolete)
		return;

	UINT32 size = m_cache_list.GetCount();

	for (UINT32 index = 0; index < size; index++)
	{
		const OpVector<DOM_Environment> &hosts = m_cache_list.Get(index)->GetCacheHosts();
		UINT32 hosts_size = hosts.GetCount();

		for (UINT32 host_index = 0; host_index < hosts_size; host_index++)
		{
		    SendDomEvent (event_type, hosts.Get(host_index), lengthComputable, loaded, total);
		}
	}
}

/* static */
OP_STATUS ApplicationCacheGroup::SwitchDocumentCache(FramesDocument* doc, URL_CONTEXT_ID new_context_id)
{
	URL& old_url = doc->GetURL();

	if (old_url.GetContextId() == new_context_id)
		return OpStatus::OK;

	// TODO should have something like this to make sure all attributes are copied
	//URL new_url = g_url_api->GetURL(old_url, context_id);

	OpString old_url_string;
	RETURN_IF_ERROR(old_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, old_url_string));
	URL new_url = g_url_api->GetURL(old_url_string.CStr(), new_context_id);
	new_url.SetAttribute(URL::KIsApplicationCacheURL, TRUE);

	// Switch URL for document
	RETURN_IF_ERROR(doc->SetNewUrl(new_url));

	if (HLDocProfile* hld_profile = doc->GetHLDocProfile())
	{
		// Switch URL for HLDocProfile
		hld_profile->SetURL(&new_url);

		// Switch base URL
		if (hld_profile->BaseURL())
		{
			OpString base_url;
			RETURN_IF_ERROR(hld_profile->BaseURL()->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, base_url));
			URL new_base_url = g_url_api->GetURL(base_url.CStr(), new_context_id);
			new_url.SetAttribute(URL::KIsApplicationCacheURL, TRUE);
			hld_profile->SetBaseURL(&new_base_url, hld_profile->BaseURLString());
		}
	}

	// delete cached URL objects in logical document
	OP_ASSERT(doc->GetLogicalDocument());
	if (LogicalDocument* logdoc = doc->GetLogicalDocument())
	{
		if (HTML_Element* root = logdoc->GetRoot())
			root->ClearResolvedUrls();
	}

	return OpStatus::OK;
}

#endif // APPLICATION_CACHE_SUPPORT
