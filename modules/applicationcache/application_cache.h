/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#ifndef APPLICATION_CACHE_H_
#define APPLICATION_CACHE_H_

#ifdef APPLICATION_CACHE_SUPPORT
struct UnloadedDiskCache;

class Manifest;
class DOM_Environment;
class ApplicationCacheGroup;
class OpWindowCommander;

/**
 * An application cache is one version of the an application cache identified by
 * manifest given in <html manifest="/some_manifest.mst">.
 *
 * The different versions of the application cache is stored in the ApplicationCacheGroup.
 * An application cache that has been created and updated will never change. If
 * the cache is updated, a new cache is created.
 *
 * The ApplicationCache object knows which urls are handled by this cache, and how they
 * should be handled in according to the manifest document that created the ApplicationCacheGroup
 * which owns the ApplicationCache objects.
 *
 * Each cache is mapped to a url context in the cache module. The handling and storing
 * of the urls in cache is taken care of by the cache module.
 */

class ApplicationCache
{
public:

	virtual ~ApplicationCache();

	/**
	 * Creates a new application cache.
	 *
	 * application_cache 	The created cache (out)
	 * cache_group 			The associated cache group
	 * cache_host 			If not NULL, the cache_host will be associated with this cache.
	 */
	static OP_STATUS Make(ApplicationCache *&application_cache, const uni_char *cache_location, DOM_Environment *cache_host = NULL);

	/**
	 * Restores a cache from disk
	 *
	 * @application_cache 		The created cache (out)
	 * @unloaded_cache 			Small data structure that holds the information needed to set up the cache.
	 * @application_cache_group The cache group this cache belongs to
	 * @cache_host 				If not NULL, the cache_host will be associated with this cache.
	 */
	static OP_STATUS MakeFromDisk(ApplicationCache *&application_cache, const UnloadedDiskCache *unloaded_cache, ApplicationCacheGroup *application_cache_group, DOM_Environment *cache_host = NULL);

	/**
	 * The cache state as specified in HTML5
	 */
	enum CacheState
	{
		UNCACHED = 0, 		// A special value that indicates that an application cache object is not fully initialized.
		IDLE  = 1,  		// The application cache is not currently in the process of being updated.
		CHECKING = 2,		// The manifest is being fetched and checked for updates.
		DOWNLOADING = 3, 	// Resources are being downloaded to be added to the cache, due to a changed resource manifest.
		UPDATEREADY = 4, 	// There is a new version of the application cache available.  There is a corresponding update every event, which is fired instead of the cached event when a new update has been downloaded but not yet activated using the swapCache() method.
		OBSOLETE = 5, 		// The application cache group is now obsolete.

		UNKNOWN,
	};

	/**
	 * Check if the manifest for this cache handles a url
	 * and that the url actually is downloaded and stored.
	 *
	 * NOTE: If the url has not finished loading, this function
	 * will return FALSE.
	 *
	 * @param url 				The url to check.
	 * @return 					TRUE if the url is in this cache.
	 * */
	BOOL IsCached(const URL &url) const;
	BOOL IsCached(const uni_char *url) const;

	/**
	 * Check if the manifest for this cache handles a url.
	 *
	 * The url does not have to be loaded.
	 *
	 * @param url				The url to check.
	 * @return 					TRUE if the cache handles
	 * 							this url.
	 */
	BOOL IsHandledByCache(const uni_char *url) const;


	/**
	 * Check if a url has fallback
	 *
	 * @param url The url to check
	 * @return TRUE if the url a
	 */
	BOOL MatchFallback(const URL &url) const;
	BOOL MatchFallback(const uni_char *url) const;

	/**
	 * Get the fallback for a given url
	 * @param url
	 * @return
	 */
	const uni_char *GetFallback(const URL &url) const;
	const uni_char *GetFallback(const uni_char *url) const;

	/**
	 * Check if a url is on the whitelist
	 *
	 * If it is on the whitelist, it is aload
	 * to be loaded from network.
	 *
	 * @param url
	 * @return
	 */
	BOOL IsWithelisted(const URL &url) const;
	BOOL IsWithelisted(const uni_char *url) const;

	/**
	 * Get the cache group that owns this cache
	 *
	 * @return The cache group
	 */
	ApplicationCacheGroup *GetCacheGrup() const { return m_application_cache_group; }

	/**
	 * Returns the state for a given host
	 *
	 * @return 	UNCACHED	(numeric value 0) The ApplicationCache object's cache host is
	 * 						not associated with an application cache at this time.
	 * 			IDLE 		(numeric value 1) The ApplicationCache object's cache host
	 * 						is associated with an application cache whose application cache group's update status is idle,
	 * 						and that application cache is the newest cache in its application cache group,
	 * 						and the application cache group is not marked as obsolete.
	 * 			CHECKING 	(numeric value 2) The ApplicationCache object's cache host
	 * 						is associated with an application cache whose application cache group's update status is checking.
	 *			DOWNLOADING (numeric value 3) The ApplicationCache object's cache host
	 *						is associated with an application cache whose application cache group's update status is downloading.
	 * 			UPDATEREADY (numeric value 4) The ApplicationCache object's cache host
	 * 						is associated with an application cache whose application cache group's update status is idle,
	 * 						and whose application cache group is not marked as obsolete, but that application cache is not the newest cache in its group.
	 * 			OBSOLETE 	(numeric value 5) The ApplicationCache object's cache host
	 * 						is associated with an application cache whose application cache group is marked as obsolete.
	 */
	CacheState GetCacheState();

	/**
	 *
	 * Get the cache context (in the cache module) where this cache is stored
	 *
	 * @return The url context id of this cache
	 */
	URL_CONTEXT_ID GetURLContextId() const { return m_url_context_id; }

	/**
	 * Associate a cache host with this cache.
	 *
	 * @param cache_host
	 * @return
	 */
	OP_STATUS AddCacheHostAssociation(DOM_Environment *cache_host);

	/**
	 * Remove a cache_host association from this cache.
	 *
	 * @param cache_host
	 * @return OpStatus::OK, if the cache host association existed,
	 * 		   OpStatus::ERR otherwise.
	 */
	OP_STATUS RemoveCacheHostAssociation(DOM_Environment *cache_host);

	/**
	 * Remove all cache_host associations
	 * from this cache.
	 */
	void RemoveAllCacheHostAssociations();

	/** Get manifest.
	 * 	Can be NULL, if not set.
	 *
	 * @return The manifest, or NULL of not set.
	 */
	Manifest* GetManifest() const { return m_manifest; }

	/** Sets and takes ownership of manifest
	 *
	 *	@param manifest The manifest object
	 */
	void TakeOverManifest(Manifest* manifest);

	/** Return a list over cache_hosts associated with this cache
	 *
	 *	@return List of cache hosts
	 */
	const OpVector<DOM_Environment> &GetCacheHosts() const { return m_cache_hosts;}

	/**
	 * Checks if this cache is complete.
	 * It is complete if the cache update algorithm
	 * is finished and the update was a success
	 * @return
	 */
	BOOL IsComplete() const { return m_complete; }

	/** Add a master url.
	 *
	 * Does nothing if the url is already added
	 *
	 * @return ERR_NO_MEMORY on OOM, OK otherwise.
	 */
	OP_STATUS AddMasterURL(const uni_char *master_url);

	/**
	 *  Remove a master url from this cache.
	 *
	 *  This means that this cache will not be used by the web application
	 *  loaded from that master url
	 *
	 * @param master_url The url to remove
	 * @return OpStatus::OK or OpStatus::ERR if the master did not exist.
	 */
	OP_STATUS RemoveMasterURL(const uni_char *master_url);

	/**
	 * Remove all master urls from this cache.
	 */
	void RemoveAllMasterURLs();

	/**
	 * Check if a web application is using this cache
	 *
	 * @param master_url The url of the application
	 * @return TRUE or FALSE
	 */
	BOOL HasMasterURL(const uni_char *master_url) const { return m_master_document_urls.Contains(master_url); }

	/**
	 * Get an iterator for the master urls using this cache.
	 *
	 * The iterator is owned by caller.
	 *
	 * @return the iterator or NULL if OOM.
	 */
	OpHashIterator *GetMastersIterator() { return m_master_document_urls.GetIterator(); }

	/**
	 * The location where this cache is stored.
	 *
	 * The full path is "<OPFILE_HOME_FOLDER>/applicationcache/" prepended
	 * to the return of this function.
	 *
	 * @return The cache storage location.
	 */
	const uni_char *GetApplicationLocation() const { return m_application_cache_location.CStr(); }

	/**
	 * Mark this url as foreign.
	 * Foreign means that the url is an explicit entry that have a manifest
	 * attribute but that it doesn't point at this cache's manifest.
	 *
	 * @param url		The url to set as foreign
	 * @param foreign   TRUE/FALSE
	 * @return
	 */
	OP_STATUS MarkAsForegin(URL &url, BOOL foreign);

	/**
	 * Returns the size of the cache in kilobytes
	 * It only counts the permanent stored urls.
	 *
	 * @return size in kilobytes
	 */
	UINT GetCurrentCacheSize() { return m_cache_disk_size; }

	/**
	 * Check if this cache is obsolete.
	 *
	 * @return TRUE if obsolete
	 */
	BOOL IsObsolete() { return m_is_obsolete; };

	/**
	 * Set cache to obsolete
	 *
	 * This application cache will be ignored
	 *
	 */
	void SetObsolete() { m_is_obsolete = TRUE;}

	/**
	 * Should only be called by application cache code
	 *
	 * Sets the complete flag for this cache. It will
	 * also register all cached urls in the
	 * application cache manager, and mark all urls
	 * with URL::KIsApplicationCacheURL so that
	 * they will not be removed from cache.
	 *
	 * @param complete If true, the cache is set to be complete
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 *
	 */
	OP_STATUS SetCompletenessFlag(BOOL complete);
private:

	ApplicationCache();

	/**
	 * Should only be called from ApplicationCacheGroup
	 */
	OP_STATUS RemoveOwnerCacheGroupFromManager();

	OP_STATUS CreateURLContext(const uni_char *location);

	// Maps the application cache to an URL_CONTEXT.
	URL_CONTEXT_ID m_url_context_id;
	BOOL m_context_created;

	// Completenes flag. Only the most recent cache in a group can be incomplete.
	BOOL m_complete;

	OpString m_application_cache_location;

	// Pointer back to the cache group owning this cache. Pointer is owned by ApplicationCacheManager.
	ApplicationCacheGroup *m_application_cache_group;

	 // Frames documents using this cache. Referred to as cache host in html5 specification.
	OpVector<DOM_Environment> m_cache_hosts;

	// Master documents using the manifest of cache. Master documents are cached,
	// even if they not mentioned in the manifest. */
	OpAutoStringHashTable<OpString> m_master_document_urls;

	// The parsed manifest object
	Manifest* m_manifest;

	// The amount KB this cache uses on dis
	UINT m_cache_disk_size;

	BOOL m_is_obsolete;

	/* obs: use with care: must be updated before used in destructor
	 * this variable is needed for UrlRemover
	 */

	friend class ApplicationCacheGroup;
};
#endif // APPLICATION_CACHE_SUPPORT
#endif /* APPLICATION_CACHE_H_ */
