/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef APPLICATION_CACHE_GROUP_H_
#define APPLICATION_CACHE_GROUP_H_

#ifdef APPLICATION_CACHE_SUPPORT
struct UpdateAlgorithmArguments;
class ManifestParser;

#include "modules/url/url2.h"
#include "modules/util/opfile/opfile.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/url/url_loading.h"

/**
 * An application cache group control the cache for one specific manifest given in <html manifest="/some_manifest.mst">.
 *
 * Each manifest is associated with exactly one application cache group. A cache group controls
 * application caches defined by class ApplicationCache.  Each cache is one specific version
 * of the cache. Every time the manifest changes on the server, the new cache is created and added
 * to the cache list in the application cache group. All the urls specified in the manifest are downloaded
 * and stored in the cache.
 *
 * The ApplicationCacheGroup also takes care of most of the application cache update algorithm as specified in HTML5.
 */

class ApplicationCacheGroup : public MessageObject, public OpTimerListener
{
public:
	virtual ~ApplicationCacheGroup();

	/**
	 * Create the cache group.
	 *
	 * @param application_cache_group 	(out)The created group
	 * @param manifest_url				The manifest_url that identifies this group
	 * @return OpStatus::ERR_NO_MEMORY or OpStatus::OK
	 */
	static OP_STATUS Make(ApplicationCacheGroup *&application_cache_group, const URL &manifest_url);

	/**
	 * The cache group states as specified in HTML5
	 */
	enum CacheGroupState
	{
		IDLE, 			// The application cache is not currently in the process of being updated.
		CHECKING, 		// The manifest is being fetched and checked for updates.
		DOWNLOADING 	// Resources are being downloaded to be added to the cache, due to a changed resource manifest.
	};

	/** Add a new cache to the group. Should not
	 * be called by other than ApplicationCacheManager
	 * or ApplicationCacheGroup.
	 *
	 * The added cache will be the last version of the caches
	 * this cache group.
	 *
	 * @param cache The cache to add
	 * @return OK, ERR_NO_MEMORY or ERR if the cache has already been added.*/
	OP_STATUS AddCache(ApplicationCache *cache);

	/** Remove cache from memory under shutdown.
	 *  Will not effect the cache already on disk
	 *
	 * @param cache The cache to add
	 * @return OK or ERR if cache did not exist
	 * */
	OP_STATUS RemoveCache(ApplicationCache *cache);

	/** Remove cache from both memory and disk
	 *
	 * @param cache The cache to delete
	 */
	void DeleteCache(ApplicationCache *cache);

	/** Remove this cache group from both memory and disk
	 *
	 * Only the content in the caches will be affected.
	 *
	 * Can only return OpStatus::ERR_NO_MEMORY if
	 * update_application_cache_index_file == TRUE
	 *
	 * @param update_application_cache_index_file
	 * @param keep_newest_complete_cache If true, do not delete the newest
	 * 									 cache it is complete
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if update_application_cache_index_file and and OOM happens
	 */
	OP_STATUS DeleteAllCachesInGroup(BOOL update_application_cache_index_file = TRUE, BOOL keep_newest_complete_cache = FALSE);

	/** Get the most recent cache in cache group that
	 *  is that is complete if must_be_complete == TRUE and
	 *  has master url == match_master_document.
	 *
	 * @param must_be_complete			Ignored if false. If true, return the most recent cache that is
	 * 									complete and matches match_master_document.
	 * @param match_master_document  	Ignored if NULL, otherwise the returned cache must
	 * 									contain match_master_document and matches must_be_complete.
	 *
	 * @return 							The most recent cache which satisfies must_be_complete and matches match_master_document.
	 */
	ApplicationCache *GetMostRecentCache(BOOL must_be_complete = FALSE, const uni_char *match_master_document = NULL) const;

	/** Get the host of this ApplicationCache while updating.
	 *
	 * @return 							The current DOM_Environment owning this ApplicationCache while being updated, NULL if not updating.
	 */
	DOM_Environment *GetCurrentUpdateHost() const;

	/** Sets the update arguments, and start the asynchronous loading of the manifest
	 *
	 * @param cache_host			The cache_host that started the update
	 * @param update_arguments		The arguments for the cache update.
	 * 								In case the manifest changes on the server
	 * 								in the middle of an update, the algorithm
	 * 								must be restarted using these arguments.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS StartAnsyncCacheUpdate(DOM_Environment *cache_host, UpdateAlgorithmArguments *update_arguments);

	/** Add pending master to the update.
	 *
	 * A pending master is an object that contains a cache host and a master url.
	 *
	 * The pending masters will be associated and notified when
	 * then update algorithm is done.
	 *
	 * @param master_document_url The master document using the cache
	 * @param cache_host		  The cache host loading the master
	 * @return ERR_NO_MEMORY or OK
	 */
	OP_STATUS AddPendingMasterEntry(const URL& master_document_url, DOM_Environment* cache_host);

	/** Associate the master in the pending master list with a specific cache in the cache group.
	 *
	 * @param cache The cache in the cache group the pending master entries should be associated with.
	 * @param must_have_finished_loading If TRUE, only associate entries that have finished loading.
	 * @return OK, ERR_NO_MEMORY or ERROR if the association already has happened
	 */
	OP_STATUS AssociatePendingMasterEntries(ApplicationCache *cache, BOOL must_have_finished_loading);

	/** Check if a pending master containing cache_host exists.
	 *
	 * @param cache_host The cache_host to check
	 * @return TRUE if it is a pending cache host.
	 */
	BOOL IsPendingCacheHost(const DOM_Environment* cache_host) const ;

	/** Tell the cache that the cache_host cannot be guaranteed to
	 * be alive. For example if the page is closed, or a reload happens.
	 *
	 * @param 	cache_host the cache_host that is destructed.
	 * @return 	OpStatus::OK, err OpStatus::ERR if cache_host did
	 * 			exist.
	 */
	OP_STATUS CacheHostDestructed(const DOM_Environment* cache_host);

	/** Remove the pending master which is loaded from a given cache_host
	 *
	 * @param cache_host The cache_host to be removed from the pending master
	 * @return
	 */
	OP_STATUS DeletePendingCacheHost(const DOM_Environment* cache_host);

	/** Check if a pending master containing master url exists.
	 *
	 * @param master_entry The master url to check
	 *
	 * @return TRUE if it exists
	 */
	BOOL IsPendingMasterEntry(const URL &master_entry) const;

	/** Get the manifest url for this cache group as string
	 *
	 * @return  The manifest url
	 */
	const uni_char *GetManifestUrlStr() const { return m_manifest_url_str.CStr(); }

	/** Get the manifest url for this cache group
	 *
	 * @return  The manifest url
	 */
	URL GetManifestUrl() const { return m_manifest_url; }

	/**
	 * Return a unique ID for this cache group
	 *
	 * @return return The unique id for this group
	 */
	INTPTR GetApplicationCacheID(){ return (INTPTR)m_manifest_url.Id(); }

	/** Run the update cache algorithm.
	 *
	 * Is only supposed to be triggered by javascript.
	 *
	 * @param cache_host The cache host who trigger the update
	 *
	 * @return OK, ERR_NO_MEMORY or ERR for genering error.
	 */
	OP_STATUS UpdateCache(DOM_Environment *cache_host);

	/** Swap a cache_host to the latest complete cache in cache group.
	 *
	 *
	 * @param cache_host The cache host who triggers the swap.
	 * @return OK, ERR_NO_MEMORY or ERR for genering error.
	 */
	OP_STATUS SwapCache(DOM_Environment *cache_host);

	/**
	 * Set Or remove the arguments needed to restart the update algorithm,
	 * in case the manifest is updated i the middle of the cache
	 * update algorithm.
	 *
	 * @param restart_arguments The arguments to use for restarting the algorithm
	 */
	void SetRestartArguments(UpdateAlgorithmArguments *restart_arguments){ m_restart_arguments = restart_arguments;}

	/**
	 * Get the state of the cache
	 *
	 * @return the state The current state of the cache group
	 */
	CacheGroupState GetUpdateState() const { return m_cache_group_state; }

	/**
	 * Check if the cache group is obsolete. In that case it
	 * will be deleted as soon as it's not used.
	 *
	 * @return TRUE if obsolete
	 */
	BOOL IsObsolete() { return m_is_obsolete; }

	/** return the current quota for this application cache in KBytes.
	 *
	 * @return The disk quota for this cache in KBytes. */
	int GetDiskQuota(){ return m_cache_disk_quota; }

	/** Cancel the current running update.
	 *
	 * @param current_download_entry The current url downloaded
	 */
	void CancelUpdateAlgorithm(URL &current_download_entry);

	/**Set the disk quota and continue the update algorithm.
	 *
	 * @param new_disk_quota			The new disk quota in kilobytes
	 * @param current_download_entry	Continue the algorithm with this url
	 */
	void SetDiskQuotaAndContinueUpdate(int new_disk_quota, BOOL is_copying_from_previous_cache, URL &current_download_entry);

	/**
	 * Return all windows associated with any cache in the cachegroup.
	 * Should be used by UI callbacks to send events
	 *
	 * Vector must be freed by caller
	 *
	 * @return A vector of the windows associated with any cache in the group, or NULL if memory error;
	 */

	OpVector<Window> *GetAllWindowsAssociatedWithCacheGroup();

	/**
	 * When a new manifest has been parsed, we need to copy all the file in the
	 * manifest normal cache, to this cache.
	 */
	OP_STATUS InitLoadingExplicitEntries();

	/**
	 * Abort the current update process.
	 *
	 */
	void AbortUpdateProcess();

private:
	/**
	 * Used by m_manifest_url_list.ForEach()
	 */
	static void StopLoadingUrl(const void *key, void *data);

	void SetUpdateState(CacheGroupState state) { m_cache_group_state = state; }

	enum CacheAttempt
	{
		NO_ATTEMPT,
		CACHE_ATTEMPT,
		UPGRADE_ATTEMPT
	};

	enum UpdateAlgorithmSubRoutine
	{
		NO_SUBROUTINE_RUNNING,
		RUNNING_CACHE_FAILURE_ALGORITHM, /* cache failure */
		RUNNING_MANIFEST_NOT_CHANGED_ALGORITHM, /* Step .7, when manifest has not changed during upgrade*/
		RUNNING_WAITING_PENDING_MASTERS
	};

	struct PendingMasterEntry : public Link
	{
		URL m_master_entry;
		DOM_Environment* m_cache_host;
		BOOL m_associated;
		PendingMasterEntry(const URL& url, DOM_Environment* host)
			: m_master_entry(url), m_cache_host(host), m_associated(FALSE) {}
	};

	struct ManifestEntry
	{
		BOOL explicit_or_fallback_entry;
		BOOL master_entry;
		BOOL manifest_entry;
		IAmLoadingThisURL m_entry_lock;
		URL_InUse m_entry;
		ApplicationCacheGroup *m_group;

		void StopLoading()
		{
			if (m_entry->Status(TRUE) != URL_LOADED)
				m_entry->StopLoading(g_main_message_handler);
			m_entry_lock.UnsetURL();
			m_group->UnRegisterLoadingHandlers(m_entry);
		}

		OP_STATUS Load()
		{
			URL referrer_url;
			URL_LoadPolicy loadpolicy(FALSE, URL_Reload_Conditional, FALSE);
			if (m_entry->LoadDocument(g_main_message_handler, referrer_url, loadpolicy) != COMM_LOADING)
				return OpStatus::ERR;

			return m_entry_lock.SetURL(m_entry);
		}

		ManifestEntry(URL &url, ApplicationCacheGroup *group)
			: explicit_or_fallback_entry(FALSE)
			, master_entry(FALSE)
			, manifest_entry(FALSE)
		{
			m_entry.SetURL(url);
		}

		~ManifestEntry() { StopLoading(); m_group->UnRegisterLoadingHandlers(m_entry); }

	};

	/**
	 * Set the obsolete flag of this group.
	 *
	 * If this flag is set, the group will be deleted
	 * as soon as no cache_host is using it.
	 *
	 * @param return OK or ERR_NO_MEMORY on oom.
	 */
	OP_STATUS SetObsolete();

	/**
	 * Send events to the applicationCache javascript object
	 *
	 * lengthComputable, loaded and total is only used by progress event
	 *
	 * @param event_type		The type of event which can be
	 * 								APPCACHECHECKING,
									APPCACHENOUPDATE,
									APPCACHEDOWNLOADING,
									APPCACHEUPDATEREADY,
									APPCACHECACHED,
									APPCACHEOBSOLETE,
									ONERROR,
									ONPROGRESS
	 * @param cache_host			The dom environment who will receive the event
	 * @param lengthComputable		TRUE, if loaded is possible to calculate. ONLY USED BY ONPROGRESS
	 * @param loaded				Number of urls currently loaded. ONLY USED BY ONPROGRESS
	 * @param total					Total urls that will be loaded. ONLY USED BY ONPROGRESS
	 */
	void SendDomEvent(DOM_EventType event_type, DOM_Environment* cache_host, BOOL lengthComputable = FALSE, OpFileLength loaded = 0, OpFileLength total = 0);

	/** Send events to the applicationCache javascript object in
	 * all cache host loading pending masters.
	 *
	 * lengthComputable, loaded and total is only used by progress event
	 *
	 * @param event_type		The type of event which can be
	 * 								APPCACHECHECKING,
									APPCACHENOUPDATE,
									APPCACHEDOWNLOADING,
									APPCACHEUPDATEREADY,
									APPCACHECACHED,
									APPCACHEOBSOLETE,
									ONERROR,
									ONPROGRESS
	 * @param lengthComputable		TRUE, if loaded is possible to calculate. ONLY USED BY ONPROGRESS
	 * @param loaded				Number of urls currently loaded. ONLY USED BY ONPROGRESS
	 * @param total					Total urls that will be loaded. ONLY USED BY ONPROGRESS
	 */
	void SendDomEventToPendingMasters(DOM_EventType event_type, BOOL lengthComputable = FALSE, OpFileLength loaded = 0, OpFileLength total = 0);

	/** Send events to the applicationCache javascript object in
	 * all cache hosts in this group.
	 *
	 * lengthComputable, loaded and total is only used by progress event
	 *
	 * @param event_type		The type of event which can be
	 * 								APPCACHECHECKING,
									APPCACHENOUPDATE,
									APPCACHEDOWNLOADING,
									APPCACHEUPDATEREADY,
									APPCACHECACHED,
									APPCACHEOBSOLETE,
									ONERROR,
									ONPROGRESS
	 * @param lengthComputable		TRUE, if loaded is possible to calculate. ONLY USED BY ONPROGRESS
	 * @param loaded				Number of urls currently loaded. ONLY USED BY ONPROGRESS
	 * @param total					Total urls that will be loaded. ONLY USED BY ONPROGRESS
	 */

	void SendDomEventToHostsInCacheGroup(DOM_EventType event_type, BOOL lengthComputable = FALSE, OpFileLength loaded = 0, OpFileLength total = 0);

	/**
	 * Switches document to use the cache with the new ID for all subsequent loads
	 *
	 * @param doc			change the cache for this document
	 * @param context_id	The context id for the new cache
	 * @return OpStatus::OK, OpStatus::ERR for generic error, or OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS SwitchDocumentCache(FramesDocument* doc, URL_CONTEXT_ID context_id);

	OP_STATUS OnDownloadingManifest(OpMessage msg);
	OP_STATUS OnDownloadingManifestEntries(OpMessage msg, ManifestEntry *download_url, URLStatus loading_status);

	/* From OpTimerListener */
	virtual void OnTimeOut(OpTimer* timer);


	/* From MessageObject */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	OP_STATUS CacheUpdateFinished(ApplicationCache *cache);


	PendingMasterEntry *GetPendingMasterDocument(URL_ID url_id) const;
	PendingMasterEntry *GetPendingMasterDocument(const URL &master_entry) const;
	PendingMasterEntry *GetPendingCacheHost(const DOM_Environment* cache_host) const;

	ApplicationCacheGroup();

	/**
	 * Starts loading the url "url_string" into context given by new_context_id.
	 * If the url already exists in the contxt prev_context_id,
	 * it is first copied from there. */
	OP_STATUS AddAndMergeEntries(OpHashIterator *entries_iterator, URL_CONTEXT_ID prev_context_id, URL_CONTEXT_ID new_context_id, BOOL master_entries);

	/**
	 * Start loading the next url in the manifest
	 *
	 * @param first_entry True if this is the first entry in the list.
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if loading failed.
	 */
	OP_STATUS LoadNextEntry(BOOL first_entry = FALSE);

	/**
	 *  Load and parse the manifest
	 *
	 *  @param second_try 	If TRUE, this is the second time the
	 *  				  	manifest is downloaded. The manifest
	 *  					is downloaded again after all the manifest
	 *  					entries has been downloaded, to check
	 *  				 	if the manifest has changed.
	 *
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if loading failed.
	 */
	OP_STATUS LoadFromManifestURL(BOOL second_try = FALSE);

	/**
	 *  Calculate the sum of all application cache
	 *  urls in a given context_id
	 *
	 *  Note that only urls cached by application cache
	 *  are counted.
	 *
	 *  @param 	entries_iterator	The iterator of cached urls
	 *  @param 	context_id			The context of to check
	 *  @return size of cached urls
	 *
	 */
	OpFileLength CalculateApplicationCacheSize(OpHashIterator *entries_iterator, URL_CONTEXT_ID context_id);

	OP_STATUS RegisterLoadingHandlers(const URL &url);
	void UnRegisterLoadingHandlers(const URL &url);
	void UnRegisterAllLoadingHandlers();

	/**
	 *
	 * Start waiting for pending master documents to be loaded.
	 * Waiting for pending masters can be triggered several places
	 * in the update algorithm (see the 6.9.6 in html5).
	 * The actual 'subroutine' running while waiting for the documents
	 * is indicated with subroutine
	 *
	 *
	 * @param subroutine 	Where to continue the algorithm when all
	 * 						masters have been downloaded.
	 * @return	OpStatus::ERR, OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 */
	OP_STATUS InititateListenPendingMasterResources(UpdateAlgorithmSubRoutine subroutine);
	OP_STATUS WaitingPendingMasterResources(PendingMasterEntry *current_pending_master_entry);
	OP_STATUS LoadingPendingMasterFailed(PendingMasterEntry *current_pending_master_entry, ApplicationCache *cache);
	OP_STATUS CheckLoadingPendingMasterResourcesDone(BOOL force = FALSE);
	void LoadingManifestFailed(URL &manifest_url);
	void LoadingManifestDone();

	OP_STATUS CreateNewCacheAndAssociatePendingMasterEntries(Manifest *manifest);
	void CacheFailure();

	/* Stop loading all urls. */
	void StopLoading();

	void SafeSelfDelete();
	CacheAttempt m_attempt;

	/**
	 * The state of this cache group, as defined in the application cache spec
	 */
	CacheGroupState m_cache_group_state;

	/**
	 * If TRUE, the cache will be deleted the master using it closes.
	 */
	BOOL m_is_obsolete;

	/**
	 * The quota in kilobytes
	 *
	 * 0 means unlimited
	 */
	int m_cache_disk_quota;

	/**
	 * The manifest url as string
	 */
	OpString m_manifest_url_str;
	URL m_manifest_url;

	/**
	 * The manifest file. The manifest is not stored as
	 * a cache entry. It's stored as a normal file inside
	 * the cache context folder.
	 */
	OpFile m_current_manifest_file;

	/**
	 * A random hex string that is generated for each cach cache.
	 * This is string is eventually stored in the cache created for this update.
	 */
	OpString m_current_cache_location;

	/**
	 * The currently loading manifest
	 */
	IAmLoadingThisURL m_manifest_loading_lock;
	URL_InUse m_loading_manifest_url;

	/**
	 *  List of caches in this group. The most recent cache is last. This list
	 *  owns the Applications caches.
	 */
	OpAutoVector<ApplicationCache> m_cache_list;

	/**
	 * The parser for the master
	 */
	ManifestParser* m_manifest_parser;

	/**
	 * The data descriptor for the manifest
	 */
	URL_DataDescriptor* m_urldd;

	/**
	 * The list of pending masters. The objests are of type PendingMasterEntry
	 */
	AutoDeleteHead m_pending_master_entries;

	/**
	 * counter for the progress event
	 */
	UINT m_number_of_downloaded_url_pending_master_entries;
	/**
	 * Counter of number of failed entries
	 */
	UINT m_number_of_failed_pending_master_entries;

	/**
	 * This is true if we have to wait for all masters
	 * to be loaded before the asynchronous update algorithm
	 * can proceed. It's used together with
	 */
	BOOL m_is_waiting_for_pending_masters;

	/**
	 * The update algorithm has several asynchronical subroutines. We need to
	 * keep track which one is running when we wait for pending masters.
	 */
	UpdateAlgorithmSubRoutine m_running_subroutine_while_waiting_pending_masters;

	/**
	 * List of urls from the manifest that will be downloaded. As given in step 12 in 6.9.4 Updating an application cache in HTML5
	 * type is struct ManifestDownloadUrl
	 * to avoid 64/32 bit issues we do not use URL::Id() as key. We use URL_Rep directly.
	 */
	OpAutoPointerHashTable<URL_Rep, ManifestEntry> m_manifest_url_list;

	/**
	 * The iterator used to iterate m_manifest_url_list
	 */
	OpHashIterator *m_manifest_url_list_iterator;

	/**
	 * The current sum of bytes downloaded. Only the size of urls
	 * that has been fully downloaded are added.
	 * */
	OpFileLength m_current_donwload_size;

	INT32 m_number_of_downloaded_urls;

	/**
	 * If an url hangs to long, the update algorithm
	 * will abort
	 */
	OpTimer m_timeout_handler;

	/**
	 * The timout for OpTimer
	 */
	UINT32 m_timeout_ms;

	/**
	 * We store the arguments that starts up
	 * the update algorithm. This is
	 * needed for restarting the algorithm
	 * in case the manifest changes on the server
	 * in the midle of the algorithm.
	 */
	UpdateAlgorithmArguments *m_restart_arguments;

	BOOL m_is_beeing_deleted;

	friend struct ManifestEntry;
	friend class ApplicationCacheManager;
};
#endif // APPLICATION_CACHE_SUPPORT
#endif /* APPLICATION_CACHE_GROUP_H_ */
