/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#ifndef APPLICATION_CACHE_MANAGER_H_
#define APPLICATION_CACHE_MANAGER_H_

#ifdef APPLICATION_CACHE_SUPPORT
#include "modules/url/url2.h"
#include "modules/util/opfile/opfile.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/windowcommander/OpWindowCommander.h"

#include "modules/applicationcache/src/application_cache_common.h"
#include "modules/applicationcache/application_cache.h"
#include "modules/applicationcache/application_cache_group.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/url/url_name.h"

#define FALLBACK_URLS_MAXIMUM_CONNECT_TIMEOUT 20
#define NETTWORK_URLS_MAXIMUM_CONNECT_TIMEOUT 30

class ApplicationCacheManager;
class FramesDocument;
class DOM_Environment;
class ManifestParser;
class Manifest;
class OpApplicationCacheListener;

/**
 * Manages the caches and cache groups
 *
 */
class ApplicationCacheManager : public MessageObject
{
public:
	ApplicationCacheManager();

	/**
	 * Constructs the application cache manager
	 *
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS Construct();

	/**
	 * Deletes the application cache manager.
	 *
	 * Use this instead of OP_DELETE
	 */
	void Destroy();
	
	/**
	 * The result of CheckApplicationCacheNetworkModel(..)
	 */
	enum ApplicationCacheNetworkStatus
	{
		LOAD_FAIL,									// The url is not defined in the manifest, and loading should fail
		LOAD_FROM_APPLICATION_CACHE,				// The url was a implicit or explicit entry in the manifest. Load from the application cache
		LOAD_FROM_NETWORK_OBEY_NORMAL_CACHE_RULES,	// The url is in the network section in the manifest. Thus it is whitelisted, and should load normally from network
		RELOAD_FROM_NETWORK,						// The url must be reloaded
		NOT_APPLICATION_CACHE						// The url is not associated with an application cache. Load normally
	};
	
	/**
	 * Handle the cache manifest, if set in the <html> tag in a document.
	 * 
	 * @note Should only be called from doc module 
	 * @note The function is asynchronous
	 * 	
	 * @note Checks if a application cache already has been installed for this manifest_url
	 * If the application cache is not installed, the user is asked to install the application 
	 * cache. f the user accepts, the SetCacheHostManifest is called with the same arguments 
	 * as this function, and the installation of the application is started. 
	 *
	 * @param cache_host 			The cache host of this manifest.
	 * @param manifest_url			The cache manifest.
	 * @param master_document_url	The cache host url, that is, the html document with html manifest tag
	 * @return 						OpStatus::OK if success, OpStatus::Err if manifest is already set
	 * 								for this host, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS HandleCacheManifest(DOM_Environment *cache_host, const URL &manifest_url, URL &master_document_url);

	/**
	 * This method implements 6.9.5 "The application cache selection algorithm" in html5. 
	 * 
	 * @param cache_host 			The cache host of this manifest.
	 * @param manifest_url			The cache manifest. Might be empty.
	 * @param master_document_url	The cache host url, that is, the html document with html manifest tag. Might be empty.*
	 * @return 						OpStatus::OK if success, OpStatus::Err if manifest is already set
	 * 								for this host, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS SetCacheHostManifest(DOM_Environment *cache_host, const URL &manifest_url, URL &master_document_url);

	/**
	 * Request the user to start updating the cache
	 * 
	 * Both manifest_url and cache_group cannot be empty/NULL at the same time  
	 * 
	 * If user accepts,  ApplicationCacheDownloadProcess is called using the same arguments
	 * 
	 * !Do not change the input of cache_host, manifest_url master_document_url or  cache_group
	 * without reading the html5 offline application spec, as the update algorithm behavior depends 
	 * heavily on which arguments are given!
	 * 
	 * @param cache_host			 The cache host. Might be NULL.
	 * @param manifest_url			 The cache manifest. Might be empty.
	 * @param master_document_url	 The cache host url, that is, the html document with html manifest tag. Might be empty.*
	 * @param cache_group			 The cache group. Might be NULL.
	 * 
	 * @return 						OpStatus::OK if success, OpStatus::ERR_NO_MEMORY on OOM.
	 * 
	 */
	OP_STATUS RequestApplicationCacheDownloadProcess(DOM_Environment *cache_host, const URL &manifest_url, URL &master_document_url, ApplicationCacheGroup *cache_group);
	
	/**
	 * Ask the user for more disk space for this application. 
	 * 
	 * Called by ApplicationCacheGroup during update if
	 * the current quota has been exceeded.
	 * 
	 * !Do not change the input of cache_host, manifest_url master_document_url or  cache_group
	 * without reading the html5 offline application spec, as the update algorithm behavior depends 
	 * heavily on which arguments are given!
	 * 
	 * 
	 * @param cache						  The cache that needs higher quota.
	 * @param copying_from_previous_cache TRUE, if we are in the process of copying urls
	 * 									  from previous cache. FALSE, if we are downloading
	 * 									  url into cache.
	 * @param current_download_entry 	  The url that was is process of loading when
	 * 									  the quota was exceeded. Should only be valid url
	 * 									  if copying_from_previous_cache == FALSE.
	 * 
	 * @return OpStatus::OK if success, OpStatus::ERR_NO_MEMORY on OOM, OpStatus:ERR on generic internal error
	 */
	OP_STATUS RequestIncreaseAppCacheQuota(ApplicationCache *cache, const BOOL is_copying_from_previous_cache, const URL &current_download_entry = URL());
	
	/** 
	 * This method starts up 6.9.4 "Downloading or updating an application cache" 
	 * (also called "application cache download process") in html5 
	 * 
	 * Both manifest_url and cache_group cannot be empty/NULL at the same time  
	 * 
	 * @param cache_host			The cache host. Might be NULL.
	 * @param manifest_url			The cache manifest. Might be empty.
	 * @param master_document_url	The cache host url, that is, the html document with html manifest tag. Might be empty.*
	 * @param cache_group			The cache group. Might be NULL.
	 * 
	 * @return 						OpStatus::OK if success, OpStatus::ERR_NO_MEMORY on OOM.
	 */	
	OP_STATUS ApplicationCacheDownloadProcess(DOM_Environment *cache_host, const URL &manifest_url, URL &master_document_url, ApplicationCacheGroup *cache_group = NULL);
	
	/**
	 * Remove association with a an application cache
	 * 
	 * @param cache_host	The cache host to be un-associated
	 * 
	 * @return 	OpStatus::OK i success, OpStatus::ERR if the cache
	 * 		 	was not associated in the first place.
	 * 
	 */
	OP_STATUS RemoveCacheHostAssociation(DOM_Environment *cache_host);

	/**
	 * Get the url context for the cache identified by a master document
	 * 
	 * The url context identifies the real cache in the cache module where
	 * the urls are stored 
	 * 
	 * @param master_document  The master url in normalized form.
	 *
	 * @return The url context id which points to the cache
	 * 				 identified by  master_document
	 */
	URL_CONTEXT_ID GetContextIdFromMasterDocument(const URL &master_document);
	URL_CONTEXT_ID GetContextIdFromMasterDocument(const uni_char *master_document);

	/** 
	 * Implements the heuristics to find the correct application 
	 * cache for a given url entry
	 * 
	 * @param entry_url 	the url that should be checked, does not need to have been normalized.
	 * @param parent_url 	the parent (referrer) of the url that should be checked
	 * 
	 * @return context 	The context id for the appropriate application cache, 
	 * 					or the default context (0) if none where found. */

	URL_CONTEXT_ID GetMostAppropriateCache(URL_Name_Components_st &entry_url, const URL &parent_url = URL());
	URL_CONTEXT_ID GetMostAppropriateCache(const uni_char *entry_url, const URL &parent_url = URL());

	/**
	 * Get an application cache from manifest or master url
	 *
	 * @param entry_url The entry_url in normalized form.
	 *
	 * @return the applicationCache or NULL if it doesn't exist
	 */
	ApplicationCache *GetApplicationCacheFromMasterOrManifest(URL_Name_Components_st &entry_url);
	ApplicationCache *GetApplicationCacheFromMasterOrManifest(const uni_char *entry_url);

	/**
	 * Get an application cache from manifest url
	 * @param manifest_url The manifest url which points to a cache in normalized form.
	 * @param must_be_complete The cache must be complete
	 *
	 * @return the applicationCache or NULL if it doesn't exist
	 */
	ApplicationCache *GetApplicationCacheFromManifest(const URL &manifest_url, BOOL must_be_complete = FALSE);
	ApplicationCache *GetApplicationCacheFromManifest(const uni_char *manifest_url, BOOL must_be_complete = FALSE);

	/**
	 * Get an application cache from a master_document
	 * @param master_document The master document url which points to a cache in normalized form.
	 * @param must_be_complete The cache must be complete
	 *
	 * @return the applicationCache or NULL if it doesn't exist
	 */
	ApplicationCache *GetApplicationCacheFromMasterDocument(const URL &master_document, BOOL must_be_complete = FALSE);
	ApplicationCache *GetApplicationCacheFromMasterDocument(const uni_char *master_document, BOOL must_be_complete = FALSE);

	/** 
	 * Return the application cache that is associated with the cache_host
	 * 
	 * This function can be used to check if a cache_host should load
	 * from a given cache. 
	 * 
	 * @param cache_host the cache host to check
	 * 
	 * @return The associated application cache.  
	 */
	ApplicationCache *GetApplicationCacheFromCacheHost(DOM_Environment *cache_host) const;
	
	/**
	 * Get the application cache group from cache_host
	 * that uses the cache group.
	 *
	 * @param cache_host The cache host.
	 * @return the application cache group or NULL if it doesn't exist, or if cache host is NULL.
	 */
	ApplicationCacheGroup *GetApplicationCacheGroupFromCacheHost(DOM_Environment *cache_host);

	/**
	 * Get the cache has a given cache context id.
	 *
	 * @param cache_context_id the context id.
	 *@return The cache that uses that context id
	 */
	ApplicationCache *GetCacheFromContextId(URL_CONTEXT_ID cache_context_id) const;

	/**
	 * Get the application cache group identified by a manifest
	 *
	 * @param manifest_url The manifest url
	 * @return the application cache group or NULL if it doesn't exist
	 */
	ApplicationCacheGroup *GetApplicationCacheGroupFromManifest(const URL &manifest_url);
	ApplicationCacheGroup *GetApplicationCacheGroupFromManifest(const uni_char *manifest_url);

	/**
	 * Get the application cache group for the cache identified by a master document
	 *
	 * @param master_document
	 * @param return The url context id which points to the cache
	 * 				 identified by  master_document
	 */
	ApplicationCacheGroup *GetApplicationCacheGroupMasterDocument(const URL &master_document);
	ApplicationCacheGroup *GetApplicationCacheGroupMasterDocument(const uni_char *master_document);

	/** 
	 * Create application cache group. Group is owned by ApplicationCacheManager.
	 * 
	 * @param[out]  application_cache_group The returned cache group
	 * @param		manifest_url 			The manifest url that identifes this group
	 * return 	OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS CreateApplicationCacheGroup(ApplicationCacheGroup *&application_cache_group, const URL &manifest_url);

	/**
	 * Delete application cache group identified
	 * by manifest_url. 
	 * 
	 * Also maintains the cache_groups.xml file on disk.
	 * Use therefore DeleteAllApplicationCacheGroups to delete all groups,
	 * to avoid heavy disk access.
	 * 
	 * @param manifest_url The unique manifest url for this group
	 * @return OpStatus::OK or ERR if group did not exist, or OpStatus::ERR_NO_MEMORY 
	 */
	OP_STATUS DeleteApplicationCacheGroup(const uni_char *manifest_url);
	
	/**
	 * Deletes all the application cache groups. Will
	 * also maintain the cache_groups.xml
	 * 
	 * @param keep_newest_caches_in_each_group Will keep the last cache version in each group.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS DeleteAllApplicationCacheGroups(BOOL keep_newest_caches_in_each_group = FALSE);

	/**
	 * Check application cache manager knows about the cache host,
	 * and that the cache host is not deleted
	 * 
	 * Note that to return TRUE the cache host must at some time
	 * have been associated or added as a pending cache to some group.
	 * 
	 * @return TRUE if the cache_host is still alive
	 */
	BOOL CacheHostIsAlive(DOM_Environment *cache_host);
	
	/**
	 * Callback from the cache_host that the cache_host might be deleted,
	 * and should be un-associated from all caches
	 */
	void CacheHostDestructed(DOM_Environment *cache_host);

	/**
	 *  Implements 6.9.6 Changes to the networking model
	 *  
	 *  Check where and how to load a url
	 *  
	 *  @param url	The url to check
	 *  @return 	Where to load the url from
	 */
	ApplicationCacheNetworkStatus CheckApplicationCacheNetworkModel(URL_Rep *url);
	ApplicationCacheNetworkStatus CheckApplicationCacheNetworkModel(URL &url);
	
	/** 
	 * Saves the information of where the latest complete caches for each cache manifest are stored.
	 * The information is stored in cache_groups.xml 
	 * 
	 * Under shutdown this function is called with is_shutdown == TRUE, so that 
	 * the cache context directories with old caches will be deleted.
	 * 
	 * 
	 * @param remove_cache_unused_contexts 	If true, all the old caches context directories are deleted
	 * @return 								OpStatus::OK, OpStatus:ERR generic, OpStatus::ERR_NO_MEMORY 
	 */
	OP_STATUS SaveCacheState(BOOL remove_cache_unused_contexts = FALSE);

	/**
	 * Loads the application cache state from disk after shutdown.
	 *
	 * Note that this function does not recreate the actual cache context.
	 * It just create a small data structure, so that we can do quick
	 * lookups for master urls.
	 *
	 * When a stored application cache is accessed, the actual cash context
	 * is created.
	 *
	 * @return 	OpStatus::OK or
	 * 			OpStatus::ERR_NO_MEMORY for OOM.
	 *
	 */
	OP_STATUS LoadCacheState();
	
	/**
	 * Get the folder where all the application caches are stored
	 * @return The application cache folder
	 */
	OpFileFolder GetCacheFolder() { return m_application_cache_folder; }
	
	/**
	 * callback for internal use
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	
	/**
	 * Get the WindowCommander from cache host
	 *
	 * Used to send UI events to the correct WindowCommander
	 * @return the window commander
	 */
	WindowCommander* GetWindowCommanderFromCacheHost(DOM_Environment *cache_host) const;
		
	/**
	 * Return a list of all caches. Only
	 * the most recent complete cache in each cache group is return
	 *
	 * @param all_app_caches [out]The list of caches
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS GetAllApplicationCacheEntries(OpVector<OpWindowCommanderManager::OpApplicationCacheEntry>& all_app_caches);
	

	/**
	 * Internal calls for canceling dialogs
	 * This typically happens if several tabs has
	 * been sent the same event, and one tab has answered.
	 */
	void CancelInstallDialogsForManifest(URL_ID id);
	void CancelQuotaDialogsForManifest(URL_ID id);

	/**
	 * To be called before the WindowCommander is deleted.
	 *
	 * @param The window commander that is deleted
	 */
	void CancelAllDialogsForWindowCommander(WindowCommander *wc);

#ifdef SELFTEST
	void SetSendUIEvents(BOOL send_events) { m_send_ui_events = send_events; }
#endif // SELFTEST	
	
	/**
	 * The first time a manifest is loaded it's loaded into
	 * The this context, since the new cache isn't created yet.
	 *
	 * The second time the manifest will be loaded
	 * into the correct cache and stored there
	 *
	 * @return The manifest cache context id
	 */
	URL_CONTEXT_ID GetCommonManifestContextId(){ return m_common_manifest_context_id; }
private:
	virtual ~ApplicationCacheManager(){ OP_ASSERT(g_application_cache_manager == NULL); }

	/**
	 * Since each group can be stored several 
	 * times in the hash table with different
	 * keys, it's inconvenient to store the keys
	 * in ApplicationCacheGroup. 
	 */
	struct ApplicationCacheGroupKeyBundle
	{
		OpString m_master_doucument_url_key;

		/** 
		 * The cache group is owned by m_cache_group_table_manifest_document 
		 */
		ApplicationCacheGroup *m_group;
	};

	/* Creates application cache group from unloaded manifests. If success, unloaded_disk_cache is removed from m_unloaded_disk_caches and deleted */
	OP_STATUS CreateApplicationCacheGroupUnloaded(UnloadedDiskCache *unloaded_disk_cache);

	/** 
	 * Cache group hash table. Keys are given by manifest urls. Owns the cache groups. 
	 */
	OpAutoStringHashTable<ApplicationCacheGroup> m_cache_group_table_manifest_document;

	/** 
	 * Cache group table, keys are master documents. This table is managed by the ApplicationCacheGroup. 
	 * A specific cache group can be added several times with different master documents as keys.
	 */
	OpAutoStringHashTable<ApplicationCacheGroupKeyBundle> m_cache_group_table_master_document;
	
	BOOL CacheGroupMasterTableContains(const uni_char *master_url_key);
	OP_STATUS GetApplicationCacheGroupMasterTable(const uni_char *master_url_key, ApplicationCacheGroup **group);
	OP_STATUS AddApplicationCacheGroupMasterTable(const uni_char *master_url_key, ApplicationCacheGroup *group);
	OP_STATUS RemoveApplicationCacheGroupMasterTable(const uni_char *master_url_key, ApplicationCacheGroup **group);	

	BOOL SendUpdateEventToAssociatedWindows(ApplicationCacheGroup* app_cache_group, BOOL request_continue_after_update_check, BOOL pass_cache_host_to_update_process = FALSE);
	BOOL WindowHasReceivedEvent(const DOM_Environment *cache_host);

	/**
	 * Used by GetMostAppropriateCache to check if the url with
	 * given parent url is allowed to be associated with application caches.
	 *
	 * @param parent_url
	 * @return TRUE if url with given parent url is a candidate for being
	 * 				associated with a application cache
	 */
	BOOL IsInline(const URL &parent_url);

	OP_STATUS GetURLStringFromURLNameComponent(URL_Name_Components_st &resolved_name, OpString &url_string);

	/**
	 *  ApplicationCache hash tables. The actual ApplicationCache objects are owned by ApplicationCacheGroup. 
	 */
	
	/**
	 * 
	 * Table for fast lookup of caches using DOM_Environment (the cache host identifier).
	 * 
	 * Uses  DOM_Environment pointers as key. This table is managed by ApplicationCache.
	 * If a cache is present in this table, the cache_host (DOM_Environment) it has been associated with the cache. 
	 * 
	 */
	OpPointerHashTable<DOM_Environment, ApplicationCache> m_cache_table_frm;  
	
	/**
	 * Table for fast lookup of caches using context id.
	 *  
	 * Uses context id as key. This table is managed by ApplicationCacheGroup.  
	 * All caches added here are also stored in a cache group.
	 */
	OpINT32HashTable<ApplicationCache> m_cache_table_context_id;

	/**
	 * This table keeps the most recent cache that contains a given
	 * url key.
	 */
	OpStringHashTable<ApplicationCache> m_cache_table_cached_url;
	
	/**
	 *  Keep obsolete caches in limbo mode here, just to keep them alive 
	 */
	OpAutoStringHashTable<ApplicationCacheGroup> m_obsoleted_cache_groups;
	
	OpFileFolder m_application_cache_folder;
	BOOL m_cache_state_loaded;
	
	AutoDeleteHead m_update_algorithm_restart_arguments;

	AutoDeleteHead m_pending_install_callbacks;
	AutoDeleteHead m_pending_quota_callbacks;
	BOOL m_callback_received;

	URL_CONTEXT_ID m_common_manifest_context_id;
#ifdef SELFTEST	
	BOOL m_send_ui_events;
#endif // SELFTEST
	
	friend class ApplicationCache;
	friend class ApplicationCacheGroup;
	friend struct UrlRemover;
};
#endif // APPLICATION_CACHE_SUPPORT
#endif /* APPLICATION_CACHE_MANAGER_H_ */

