/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */

#ifndef CACHE_MANAGER_H_
#define CACHE_MANAGER_H_

#include "modules/cache/context_man.h"
#include "modules/cache/cache_int.h"

class File_Storage;
class Context_Manager_Disk;

#ifdef SELFTEST
	class DebugFreeUnusedResources;
#endif

#if defined (DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)
/// List of files to delete, in a given context
class Delete_List: public ListElement<Delete_List>
{
private:
	/// Context id of the Context Manager that requested the files to delete
	URL_CONTEXT_ID context_id;
    /// List of files to delete
	AutoDeleteList<FileNameElement> file_list;

public:
	/// Constructor that associates the list to a context id
	Delete_List(URL_CONTEXT_ID context_id) { this->context_id=context_id; }
	
	/// Add a file name to the list
	void AddFileName(FileNameElement *name);
	/// @return the context ID
	URL_CONTEXT_ID GetContextID() { return context_id; }
	/// Delete the number of files specified
	/// @param num Number of files to delete
	/// @return Number of files deleted
	UINT32 DeleteFiles(UINT32 num);
	/// @return TRUE if there are still files to delete
	BOOL HasFilesInList() { return file_list.First()!=NULL; }
	/// @return the number of files in the list
	UINT GetCount() { return file_list.Cardinal(); }
};
#endif // defined (DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)

/// Class that manage the cache, delegating the main functionalities to the correct Context_Manager
class Cache_Manager
{
private:
	friend class Context_Manager;
	friend class Context_Manager_Disk;
	friend class CacheTester;
	
	/// Files waiting to be deleted
#if defined (DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)
	AutoDeleteList<Delete_List> delete_file_list;
#endif // defined (DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)
	
	#if CACHE_SMALL_FILES_SIZE>0
		// Total size of the files embedded in the cache index
		UINT32 embedded_size;
		// Total number of files embedded in the cache index
		UINT32 embedded_files;
	#endif
	
	//----- Context_Managers list object ------
	/// List of the Context_Managers
	List<Context_Manager> context_list;
	/// Context_Managers that are marked for being deleted when possible; this can happen as a consequence of
	/// calling Context_Manager->DecReferencesCount(), or because FreeUnusedResources() detects it
	List<Context_Manager> frozen_contexts;
	
	/// Specification of the main context, to simplify the refactoring
	Context_Manager *ctx_main;
	/// TRUE if the cache is currently loading
	BOOL		updated_cache;
	/// TRUE if the content of the cache is in the process of being destroyed, because Opera is exiting
	BOOL 		exiting;
#if !defined(NO_URL_OPERA)
	/// opera:cache generator
	CacheListWriter clw;
#endif

#ifdef SELFTEST
private:
	/// TRUE to simulate a bug that used to crash Opera during a RemvoeContext() operation
	BOOL debug_simulate_remove_context_bug;
	/// TRUE to not check for duplicated contexts
	BOOL debug_accept_overlapping_contexts;
	/// TRUE to simulate an OOM during AddContextL();
	BOOL debug_oom_on_context_creation;
	
	friend class CacheHelpers;
	
public:
	/// Set the flag to simulate a bug that used to crash Opera during a RemvoeContext() operation
	void Debug_SimulateRemoveContextBug(BOOL b) { debug_simulate_remove_context_bug = b; }
	/// Get the flag to simulate a bug that used to crash Opera during a RemvoeContext() operation
	BOOL Debug_GetSimulateRemoveContextBug() { return debug_simulate_remove_context_bug; }
	/// Enable one overlapping context (the first AddContextL() will set the flag to FALSE ) 
	void Debug_AcceptOverlappingContexts() { debug_accept_overlapping_contexts = TRUE; }
	/// Set the flag to simulate a bug that used to crash Opera during a RemvoeContext() operation
	void Debug_SimulateOOMOnContextCreation(BOOL b) { debug_oom_on_context_creation = b; }
#endif // SELFTEST

private:
	// Put the context in the freeze list, to avoid dangerous operations and to clean it when possible
	// This method does not allow to freeze the main context, as it does not really make sense and it is not currently supported by several methods
	// This also imply that the main context cannot be removed.
	void			FreezeContextManager(Context_Manager *ctx) { if(ctx) { if(ctx==ctx_main) return; else ctx->Out(); ctx->Into(&frozen_contexts); } }
	// Returns TRUE if the context is in the freeze list
	BOOL			IsContextManagerFrozen(Context_Manager *ctx) { OP_ASSERT(ctx); if(!ctx) return FALSE; return ctx->GetList()==&frozen_contexts; }
	
public:
	/// Default constructor
	Cache_Manager();

	/// Destructor
	~Cache_Manager();
	/// Initialize the main context, which will automatically get the context_id 0
	void InitL(
		#ifdef DISK_CACHE_SUPPORT
			OpFileFolder a_vlink_loc = OPFILE_CACHE_FOLDER, OpFileFolder a_cache_loc = OPFILE_CACHE_FOLDER
		#endif
	);
	void DoCheckCache();
	// Find a context Manager
	Context_Manager *FindContextManager(URL_CONTEXT_ID id);
	/// Return the main context (equivalent to FindContextManager(0))
	Context_Manager *GetMainContext() { return ctx_main; }
	/** Retrieve an URL_Rep using the URL_ID (this operation is slow, as the list need to be checked)
		It checks EVERY context!
	*/
	URL_Rep *LocateURL(URL_ID id);
	
#if CACHE_SMALL_FILES_SIZE>0
	// Increase the total size of the files embedded in the cache, and add 1 to the number of files
	void IncEmbeddedSize(UINT32 size) { OP_ASSERT(size<=CACHE_SMALL_FILES_SIZE); embedded_size+=size; embedded_files++; }
	// Decrease the total size of the files embedded in the cahce, and subtract 1 to the number of files
	void DecEmbeddedSize(UINT32 size)
	{
		OP_ASSERT(size<=CACHE_SMALL_FILES_SIZE && size<=embedded_size && embedded_files>0);
		
		if(size<=embedded_size)
			{ embedded_size-=size; embedded_files--; }
		else
			{ embedded_size=embedded_files=0; } ; 
	}
	// Get the size of the files embedded
	UINT32 GetEmbeddedSize() { return embedded_size; }
	// Get the number of the files embedded
	UINT32 GetEmbeddedFiles() { return embedded_files; }
#endif


#ifdef NEED_CACHE_EMPTY_INFO
	/** checks if there are any items in cache */
	/// PROPAGATE: NEVER. Verify...
	BOOL			IsCacheEmpty();
#endif // NEED_CACHE_EMPTY_INFO

		
	/// Make an URL unique
	void MakeUnique(URL_Rep *url);
	
	//--------------- Function added for the Context_Manager itself...-----
	/// Stop loading all the URLs
	void CacheCleanUp(BOOL ignore_downloads);
	/// Remove URLs that can be discarded, and (try to) force the cache to fit in the limits
	BOOL FreeUnusedResources(BOOL all = TRUE SELFTEST_PARAM_COMMA_BEFORE(DebugFreeUnusedResources *dfur = NULL) );
	/// Save all the contexts
	void AutoSaveCacheL();
	/// Unload URLs and delete url_store, on each context
	void PreDestructStep(BOOL from_predestruct=FALSE);
	/// Remove sensitive URLs (for example HTTPS or with passwords)
	void RemoveSensitiveCacheData();
#if defined(NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS)
	// Restart all connections after a proxy change
    void RestartAllConnections();
    void StopAllLoadingURLs();
#endif // NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS
	/// Set the cahce size (in KB), if pref==PrefsCollectionNetwork::DiskCacheSize
	void PrefChanged(int pref, int newvalue);
#ifdef _OPERA_DEBUG_DOC_
	/// Write debug informations inside an URL (relative to its context)
	void WriteDebugInfo(URL &url);
#endif

	/// Add an URL_Rep to the list of URLs stored in the context
	void			AddURL_Rep(URL_Rep *r);
	/// Get the URL from the list stored in the context
	URL_Rep *		GetResolvedURL(URL_Name_Components_st &url, URL_CONTEXT_ID context_id = 0);
	/** Add a new context (basically a separated cache on a dedicated directory).
		WARNING: when the context is no longer in use, possibly when Opera shuts down, it has to be removed using RemoveContext().
		         Failing to remove it, will require a synchronization the next time the context is read.
				 This factor is critical for the main context, as it can impact the startup time
		@param id Context id, usually retrieved using urlManager->GetNewContextID()
		@param vlink_loc Visited Link file
		@param cache_loc Folder that will contain the cache
		@param cache_size_pref Preference used to get the size in KB; <0 means default size (5% of the main cache, set by PrefsCollectionNetwork::DiskCacheSize)
	*/
	void			AddContextL(URL_CONTEXT_ID id, OpFileFolder vlink_loc,
								OpFileFolder cache_loc, int cache_size_pref, URL &parentURL);
	/// Add a context using directly the Context Manager. The Context ID of the manager must be available!
	/// ConstructPrefL() or ConstructSizeL() must already have been called
	OP_STATUS		AddContextManager(Context_Manager *ctx);
	/** Remove a context
	 * @param id Context ID
	 * @param empty_context TRUE to call EmptyDCache(), FALSE to let the cache persist across reboot
	 */
	void			RemoveContext(URL_CONTEXT_ID id, BOOL empty_context);
	// Check if the context exists
	BOOL			ContextExists(URL_CONTEXT_ID id);
	/** Finds context with given parentURL. Return TRUE if context exists, FALSE in other case */
	BOOL			FindContextIdByParentURL(URL &parentURL, URL_CONTEXT_ID &id);
	// Increment the context reference count
	void			IncrementContextReference(URL_CONTEXT_ID id);
	/// Decrement the context reference count
	/// @param id Context ID
	/// @param enable_context_delete TRUE if the context can be deleted as a result fo this call. This parameter
	///		should be set to TRUE only be RemoveContext().
	///		FreeUnusedResources() can also remove the context, but without calling this method
	/// @return returns TRUE if the context as been deleted
	BOOL			DecrementContextReference(URL_CONTEXT_ID id, BOOL enable_context_delete=FALSE);

	/// Force the RAM cache to fit into its size (or into force_size if it is >=0)
	/// It calls this method in all the contexts, as this operation is supposed to be performed in low memory situations
	void CheckRamCacheSize(OpFileLength force_size= (OpFileLength) -1);

	/** Gets the combined size of all the caches managed by the cache manager.
	 *  @return The size.
	 */
	OpFileLength GetRamCacheSize();

#if defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)
	// Add some files to the list of files to delete for the given context id
	OP_STATUS AddFilesToDeleteList(URL_CONTEXT_ID ctx_id, FileName_Store &filenames);
	// Delete some files in the list (max CACHE_NUM_FILES_PER_DELETELIST_PASS), and continue later if required
	// When the list of a given context is depleted, and if the context is no longer existing, activity.opr will be deleted
	void DeleteFilesInDeleteList(BOOL all=FALSE);
	// Delete ALL files in the list of a given context; this is intended for tests and operations that require synchronization with the disk
	// Normally DeleteFilesInDeleteList(FALSE), which acts on every context, and does not block Opera too much time, is better.
	void DeleteFilesInDeleteListContext(URL_CONTEXT_ID ctx_id);
	/// @return TRUE if all the files of the given context have been deleted
	BOOL AllFilesDeletedInContext(URL_CONTEXT_ID ctx_id);
	/// @return the number of files in delete list
	UINT GetNumFilesToDelete();
	/// Returns TRUE if all the files have been deleted
	BOOL AllFilesDeleted();
#endif // defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)

#ifdef DISK_CACHE_SUPPORT
#ifndef SEARCH_ENGINE_CACHE	
	/// Request to check the disk cache size
	void StartCheckDCache() { if(GetMainContext()) GetMainContext()->StartCheckDCache(); }
	/**
		Dump the files on disk and save the index of the cache
		@param sort_and_limit TRUE to force the cache into its limit
		@param destroy_urls TRUE to destroy URLs after saving (Opera exit)
		@param write_contexts TRUE to save all the contexts
	*/
	void WriteCacheIndexesL(BOOL sort_and_limit, BOOL destroy_urls, BOOL write_contexts = TRUE);
	/// Load the index of the visited links
	void ReadVisitedFileL();
	/// Load the cache index of the visited links
	void ReadDCacheFileL();
#endif // SEARCH_ENGINE_CACHE
#endif // DISK_CACHE_SUPPORT

#ifdef APPLICATION_CACHE_SUPPORT
	OP_STATUS CopyUrlToContext(URL_Rep *url, URL_CONTEXT_ID desitnation_context);
#endif // APPLICATION_CACHE_SUPPORT

	void SetLRU_Item(URL_DataStorage *url);
	void RemoveLRU_Item(URL_DataStorage *url_ds);
	void SetLRU_Item(URL_Rep *url);

#ifdef NEED_URL_VISIBLE_LINKS
	BOOL			GetVisLinks() { return  GetMainContext()->GetVisLinks(); }
	void			SetVisLinks(BOOL val=TRUE) { GetMainContext()->SetVisLinks(val); }
#endif // NEED_URL_VISIBLE_LINKS

	#ifdef CACHE_GENERATE_ARRAY
		/// Generate some arrays of data used by opera:cache
		OP_STATUS GenerateCacheArray(uni_char** &filename, uni_char** &location, int* &size, int &num_items); { GetMainContext()->GenerateCacheArray(filename, location, size, num_items); }
	#endif
	#if !defined(NO_URL_OPERA) 
		/// Write the cache list inside the URL (this is the output of opera:cache)
		OP_STATUS GenerateCacheList(URL& url);
	#endif
	
	/// Remove the Mesage handler from every contexts
	void RemoveMessageHandler(MessageHandler* mh);
	/// Remove the URL from the list of stored URLs
	void RemoveFromStorage(URL_Rep *url);
	
	/// Delete the URLs that have a reference count <=1
	void DeleteVisitedLinks();
	/// Notify that the cache is loading
	void			HasStartedLoading(){}

#ifdef SELFTEST
	/** Check invariants in all the Context Managers;
		@return FALSE if there is an error
	*/
	BOOL CheckInvariants();
#endif //SELFTEST
	
	BOOL GetContextIsRAM_Only(URL_CONTEXT_ID id);
	void SetContextIsRAM_Only(URL_CONTEXT_ID id, BOOL ram_only);
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	BOOL GetContextIsOffline(URL_CONTEXT_ID id);
	void SetContextIsOffline(URL_CONTEXT_ID id, BOOL is_offline);
#endif

#ifdef URL_FILTER
	// Set the URLFilter used to decide what URLs to delete from the cache; it is supposed to be used by the operator cache, for privacy concerns (e.g.: delete SiteCheck files)
	// The filter select which URLs to delete
	OP_STATUS SetEmptyDCacheFilter(URLFilter *filter, URL_CONTEXT_ID ctx_id);
#endif

	/** Delete objects from the disk cache, from all the contexts
	 * @param mode The cache is separated into normal cache and operator cache.
	 * This argument specifies on which to operate. It is also possible to
	 * operate on both at the same time.
	 * @param cache_operation_target URL or URL prefix. See @ref prefix_match.
	 * @param prefix_match Specifies whether the @ref prefix_match parameter is
	 * a URL or a URL prefix. If it is a prefix, every object matching this
	 * prefix in the cache will be deleted. Otherwise, only the specified URL
	 * will be deleted.
	 * @param origin_time Only delete objects older than this timestamp. The
	 * value 0 means that no timestamp comparison should be done.
	 */
	void			EmptyDCache(
		OEM_EXT_OPER_CACHE_MNG(delete_cache_mode mode = EMPTY_NORMAL_CACHE)    OPER_CACHE_COMMA()
		OEM_CACHE_OPER_AFTER(const char *cache_operation_target=NULL)
		OEM_CACHE_OPER_AFTER(int flags=0)
		OEM_CACHE_OPER(time_t origin_time=0));
	
	/** 
	 * Version of EmptyDCache that works on only one context
	 *
	 * Delete objects from the disk cache
	 * @param mode The cache is separated into normal cache and operator cache.
	 * This argument specifies on which to operate. It is also possible to
	 * operate on both at the same time.
	 * @param cache_operation_target URL or URL prefix. See @ref prefix_match.
	 * @param prefix_match Specifies whether the @ref prefix_match parameter is
	 * a URL or a URL prefix. If it is a prefix, every object matching this
	 * prefix in the cache will be deleted. Otherwise, only the specified URL
	 * will be deleted.
	 * @param origin_time Only delete objects older than this timestamp. The
	 * value 0 means that no timestamp comparison should be done.
	 */
	void EmptyDCache(
		URL_CONTEXT_ID ctx_id
		OEM_EXT_OPER_CACHE_MNG_BEFORE(delete_cache_mode mode = EMPTY_NORMAL_CACHE)    OPER_CACHE_COMMA()
		OEM_CACHE_OPER_AFTER(const char *cache_operation_target=NULL)
		OEM_CACHE_OPER_AFTER(int flags=0)
		OEM_CACHE_OPER(time_t origin_time=0));
	
	
	OP_STATUS		GetNewFileNameCopy(OpStringS &name, const uni_char* ext, OpFileFolder &folder, BOOL session_only
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
					,BOOL useOperatorCache=FALSE
#endif
	, URL_CONTEXT_ID context_id = 0);		

#ifdef SELFTEST
	BOOL emulateOutOfDisk;
#endif //SELFTEST
};

#endif // CACHE_MANAGER_H_
