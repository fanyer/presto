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

#ifndef CONTEXT_MANAGER_H_
#define CONTEXT_MANAGER_H_


enum delete_cache_mode {
	EMPTY_NORMAL_CACHE,
	EMPTY_OPERATOR_CACHE,
	EMPTY_NORMAL_AND_OPERATOR_CACHE
};

class FileName_Store;
class URL_Store;
class CacheIndex;
class DomainSummary;
class Cache_Manager;
class Context_Manager;
class CacheTester;
class CacheListWriter;
class File_Storage;

#ifdef SELFTEST
	class DebugFreeUnusedResources;
#endif

#ifdef SUPPORT_RIM_MDS_CACHE_INFO
#include "modules/pi/OpCacheInfo.h"
#endif // SUPPORT_RIM_MDS_CACHE_INFO

#include "modules/cache/cache_utils.h"
#include "modules/cache/cache_list.h"
#include "modules/url/url_name.h"
#include "modules/util/opfile/opfile.h"
#include "modules/cache/url_stor.h"
#include "modules/url/url2.h"

class URLLinkHead : public AutoDeleteHead
{
public:
	URLLink *First(){return (URLLink *)AutoDeleteHead::First();}
	URLLink *Last(){return (URLLink *)AutoDeleteHead::Last();}
};

/// Class able iterate an URLLinkHead list. NOTE' the list of course cannot be deleted during the iteration!
class URLLinkHeadIterator: public ListIteratorDelete<URL_DataStorage>
{
private:
	/// Value returned when Next() will be called
	URLLink *next;
	/// Last value used to return Next()
	URLLink *last;
public:
	// Constructor
	URLLinkHeadIterator(URLLinkHead &head) { next=head.First(); last=NULL; }

	virtual URL_DataStorage* Next();
	virtual OP_STATUS DeleteLast();
};

/// Class able iterate a list of URL_DataStorage. NOTE' the list of course cannot be deleted during the iteration!
class URL_DataStorageIterator: public ListIteratorDelete<URL_DataStorage>
{
private:
	/// Value returned when Next() will be called
	URL_DataStorage *next;
	/// Last value used to return Next()
	URL_DataStorage *last;
	/// Context Manager associated to the list
	Context_Manager *man;
public:
	// Constructor
	URL_DataStorageIterator(Context_Manager *ctx, URL_DataStorage *ds) { next=ds; last=NULL; man=ctx; OP_ASSERT(man); }

	virtual URL_DataStorage* Next();
	virtual OP_STATUS DeleteLast();
};

/// Class alternative to OpHashIterator, that iterate on the values; the OpHashIterator will be deleted in the destructor
template <class T>
class HashDataIterator: public ListIterator<T>
{
private:
	/// Real iterator, owned by this object
	OpHashIterator *iter;
	/// TRUE if first has already been called
	BOOL first_called;

public:
	HashDataIterator(OpHashIterator *iterator) { iter=iterator; OP_ASSERT(iter); first_called=FALSE; }
	~HashDataIterator() { OP_DELETE(iter); }

	virtual T* Next()
	{
		if(!iter)
			return NULL;

		OP_STATUS ops=(first_called) ? iter->Next() : iter->First();

		RETURN_VALUE_IF_ERROR(ops, NULL);

		first_called=TRUE;

		return (T*)(iter->GetData());
	}
};

// Various modes that can be used to synchronize the cache index with the disk
#define	CACHE_SYNC_NONE 0		// No synchronization
#define CACHE_SYNC_FULL 1		// Normal synchronization
#define CACHE_SYNC_ON_CHANGE 2  // Fast synchronization when non changes are detected (a flag file will
								// provide this information), else full sync
// Name of the file used to signal activity
#define CACHE_ACTIVITY_FILE UNI_L("activity.opr")

#ifdef URL_FILTER
	class URLFilter;
#endif

#ifdef CACHE_STATS
/// Class used to contain the statistics of a Coontext
class CacheStatistics
{
private:
	friend class Context_Manager;
	friend class CacheListWriter;
	friend class Context_Manager_Disk;

	int index_read_time;		// Time required to read the index file
	int index_num_read;			// Number of times that the index has been read
	int max_index_read_time;	// Maximum time required to read the index file
	int index_init;				// Time required to initialize index file
	int urls;					// Number of URLS at the beginning
	int req;					// Number of request to the cache
	int container_files;		// Number of file put on containers
	int sync_time1;				// Time required for the 1st part index synchronization (folder list)
	int sync_time2;				// Time required for the 2nd part index synchronization (delete start)

	// Create the HTML list of URLs
	// PROPAGATE: NEVER
	void CreateStatsURLList(OpAutoVector<OpString> *list, OpString &out);
	// Reset cache statistics
	// PROPAGATE: NEVER
	void StatsReset(BOOL reset_index);
};
#endif // CACHE_STATS

#ifdef SELFTEST
/// Used to keep track of URL_DataDescriptor statistics
class URLDD_Stats
{
private:
	// Data descriptor
	URL_DataDescriptor *dd;
	// Bytes moved to compact the buffer
	unsigned long bytes_memmoved;

public:
	URLDD_Stats(URL_DataDescriptor *url_dd) { OP_ASSERT(url_dd); dd=url_dd; bytes_memmoved=0; }

	// Get the data descriptor
	URL_DataDescriptor *GetDescriptor() { return dd; }
	// Get the number of mytes moved to compact the buffer (to verify the efficiency)
	unsigned long GetBytesMemMoved() { return bytes_memmoved; }
	// Increase the number of mytes moved to compact the buffer (to verify the efficiency);
	// this number is propagated to the Context manager, for better checking the behaviour
	void AddBytesMemMoved(unsigned long bytes);
};
#endif

/// Class that manages a cache size, with some customization flags
class CacheLimit
{
private:
	friend class Cache_Storage;
	friend class Context_Manager_Base;
	/// Size of the cache currently in use
	OpFileLength used_size;
	/// Size allowed for the cache
	OpFileLength max_size;
	/// TRUE if URL_Rep::GetURLObjectSize() needs to be added to the total size (which otherwise would only consider the size of the content of the URL)
	BOOL add_url_size;

#ifdef SELFTEST
	/// Struct used to keep track of the size of agiven url, for debugging purposes
	struct URLLength
	{
		/// Size of the content
		OpFileLength content_size;
		/// Estimantion of the size of the URL; this value is saved also when add_url_size is false
		OpFileLength url_size;
	};

	/// List of values used for AddToSize(); they are checked to be sure that calls to SubFromSize() matches
	OpPointerHashTable<URL_Rep, URLLength> url_sizes;
	/// Suppress some of the asserts after an OOM
	BOOL oom;

	/// Verifies if the URL length is as expected
	void DebugVerifyURLLength(URL_Rep *url, OpFileLength content_size, OpFileLength url_size);
	/// Removes the URL from url_sizes
	void DebugRemoveURLLength(URL_Rep *url);
	/// Increases the URL length recorded in the history, for debugging purposed; it does not affect the cache size
	void DebugAddURLLength(URL_Rep *url, OpFileLength content_size, OpFileLength url_size);
	/// Decreases the URL length recorded in the history, for debugging purposed; it does not affect the cache size
	void DebugSubURLLength(URL_Rep *url, OpFileLength content_size, OpFileLength url_size);

	static void DeleteDataFromHashtable(const void *key, void *data);
#endif

public:
	CacheLimit(BOOL add_url_size);
	~CacheLimit();

	/** Increment the size of the used cache
		@param content_size size of the content of the URL
		@param url URL, used to call GetURLLength() and compute the estimated size of the URL
		@param add_url_size_estimantion TRUE if we want the estimation of the URL size to be added; this is only a hint,
		       as add_url_size needs to be TRUE anyway for the URL size to be added to the computation
	*/
	void AddToSize(OpFileLength content_size, URL_Rep *url, BOOL add_url_size_estimantion);
	/** Decrement the size of the used cache
		@param content_size size of the content of the URL
		@enable_checks if FALSE, it disables consistency checks; basically it should always been TRUE, except for UnsetFinished()
			     (this only affect the code if SEFLTEST is on)
		@param url URL, used to call GetURLLength() and compute the estimated size of the URL
		WARNING: for a given URL, the sizes must match with the ones provided, during the call to AddToSize().
	*/
	void SubFromSize(OpFileLength content_size, URL_Rep *url, BOOL enable_checks);
	/**
		@param limit Limit to check
		@param add bytes to add during the check, for estimating the future
		@return TRUE if the limit has been exceeded (with STRICT_CACHE_LIMIT2 some further limitations can apply )
	*/
	BOOL IsLimitExceeded(OpFileLength limit, OpFileLength add = 0) const;
	/// Set the cache size
	void SetSize(OpFileLength size) { max_size=size; }
	/// Returns the cache size
	OpFileLength GetSize() const { return max_size; }
	/// Returns the bytes used by the cache
	OpFileLength GetUsed() const { return used_size; }
};

/// Base class of a Context_Manager. This is a candidate for refactoring...
class Context_Manager_Base
{
friend class Cache_manager;
friend class CacheListWriter;

protected:
	/// Default constructor
	Context_Manager_Base();

	/// Virtual destructor
	virtual ~Context_Manager_Base() { } 

	/// Next Manager in the chain, NULL indicates the end of the chain
	Context_Manager *next_manager;
	/// Size of the disk cache
	CacheLimit size_disk;
	/// Size of the RAM cache
	CacheLimit size_ram;
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	/// Size of the OEM cache
	CacheLimit size_oem;
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
#if defined STRICT_CACHE_LIMIT2
	/// Size of the OEM cache
	CacheLimit size_predicted;
#endif // STRICT_CACHE_LIMIT2

	/// Return the cache size. When TWEAK_CACHE_SINGLE_CACHE_LIMIT is enabled, it will be diffrent than max_size
	/// PROPAGATE: NEVER
	OpFileLength	GetCacheSize(void);


////////////// STRICT_CACHE_LIMIT2 ///////////////////
#if defined STRICT_CACHE_LIMIT2
protected:
	/// Increase the predicted cache usage
	/// @param content_size size of the content of the URL
	/// @param param url URL, used to call GetURLLength() and compute the estimated size of the URL
	/// @param add_url_size_estimantion TRUE if we want the estimation of the URL size to be added; this is only an hint
	/// PROPAGATE: NEVER.
	void			AddToPredictedCacheSize(OpFileLength content_size, URL_Rep *url, BOOL add_url_size_estimantion = TRUE);
	/// TRUE if the predicted cache size will be bigger than the mazimum size allowed
	/// @param content_size size of the content of the URL
	/// @param param url URL, used to call GetURLLength() and compute the estimated size of the URL
	/// PROPAGATE: NEVER.
	BOOL			PredictedWillExceedCacheSize(OpFileLength content_size, URL_Rep *url);
#endif

////////////// Chaining functionalities //////////
public:
	/// Set the next Context Manager in this chain, and returns the previous one
	/// PROPAGATE: NEVER
	Context_Manager *SetNextChainedManager(Context_Manager *manager);
	/// Return the next manager chained (NULL means end of the chain)
	/// PROPAGATE: NEVER
	Context_Manager *GetNextChainedManager() { return next_manager; }

////////////// URL filtering on EmptyDCache //////////
#ifdef URL_FILTER
protected:
    // URLFilter used to decide what URLs to delete from the cache; it is supposed to be used by the operator cache, for privacy concerns (e.g.: delete SiteCheck files)
    // NULL means that every URL must be deleted (default)
	URLFilter * empty_dcache_filter;

	// Set the URLFilter used to decide what URLs to delete from the cache; it is supposed to be used by the operator cache, for privacy concerns (e.g.: delete SiteCheck files)
	// The filter select which URLs to delete
	/// PROPAGATE: CONDITIONAL (no remote)
	OP_STATUS SetEmptyDCacheFilter(URLFilter *filter);
#endif


////////////// MHTML Support
#if defined(MHTML_ARCHIVE_REDIRECT_SUPPORT)
private:
	BOOL		offline;	/// TRUE if the context is offline

public:
	/// Return TRUE if this context is offline
	// PROPAGATE: NEVER
	BOOL			GetIsOffline() const {return offline;}
	/// Set if this context is offline
	// PROPAGATE: NEVER
	void			SetIsOffline(BOOL is_offline){offline = is_offline;}
#endif

////////////// DEBUG functionalities /////////////////
#ifdef SELFTEST
protected:
	// TRUE if a deeper test can be performed
	BOOL debug_deep;
	// TRUE if any type of storage can be embedded, in particular the temporary storage,
	// used by HTTPS; This also affects containers
	BOOL embed_all_types;

public:
	/// TRUE if a deeper test can be performed
	BOOL IsDebugDeepEnabled() { return debug_deep; }
	/// Set if all the cache type can be embedded, primarily to enable embedding of HTTPS
	/// This setting also affect containers
	void SetEmbedAllCacheTypes(BOOL b) { embed_all_types=b; }
	/// Return TRUE if all the cache types can be embedded and join a container
	BOOL GetEmbedAllCacheTypes() { return embed_all_types; }
	/** Verifies that the URL content size has not been added to the cache size
	  @param url THe URL to check
	  @param check_memory verify the RAM cache
	  @param check_disk veryfy the disk cache
	*/
	void VerifyURLSizeNotCounted(URL_Rep *url, BOOL check_memory, BOOL check_disk);
#endif //SELFTEST

////////////// Statistics /////////////////////////////
protected:
#ifdef CACHE_STATS
	CacheStatistics stats;		// Object used to keep track of the statistics
#endif
};

#ifdef URL_ENABLE_ASSOCIATED_FILES
	/// Strict used to store a ticket for a temporary associated file
	struct TempAssociateFileTicket
	{
		/// Ticket used to retrieve the file
		OpString ticket;
		/// File_name with the associated file
		OpString file_name;
	};
#endif // URL_ENABLE_ASSOCIATED_FILES


/** Class that will manage only a single context
	Every context has its own index, and it's supposed to be stored in its own directory

	A Context_Manager can be chained with other Context_Managers (via next_manager). This allow multiple levels of cache, and it is a
	prerequisite of the solution chosed to enable Multi threading / processing support in the network code.

	Being a chain, some method calls need to be propagated to the other managers of the chain, so each method needs
	to have a PROPAGATE note, to explain how it is supposed to behave.

	REQUIREMENTS:
	* All Context_Manager of a chain must be in the same thread
	* In the future, it will fine if the last Context_Manager communicate with another thread (for example via sockets)
	
	PROPAGATE can be of thess types:
	- NEVER: the method call is local, and it will never be propagated
	- ALWAYS: the method call is propagated every time, even if the manager is remote (for example it uses sockets)
	- CONDITIONAL: the method call is propagated sometimes. A brief note should explain when it happens.
			For example: CONDITIONAL (no remote)  ==> It is propagated, unlese the following manager is remote

	*** Notes:
		- opera:cache needs to be modified to show meaningful informations
*/
class Context_Manager: public ListElement<Context_Manager>, public Context_Manager_Base
#ifdef SUPPORT_RIM_MDS_CACHE_INFO
	: public OpCacheInfoReconcileHandler
#endif
{
protected:
	friend class Cache_Manager;
	friend class CacheTester;
	friend class MultiContext;
	friend class CacheListWriter;
	friend class Context_Manager_Base;
	friend class URL_DataStorageIterator;
	friend class URL_DataDescriptor;
	friend class URL_DataStorage;

	/// Unload an URL; method provided to avoid declaring friends all the derived Context Managers
	static void UnloadURL(URL_DataStorage *ds);
	/// Get an URL_Rep from an URL_DataStorage ; method provided to avoid declaring friends all the derived Context Managers
	static URL_Rep *GetURLRep(URL_DataStorage *ds);
	
#ifdef APPLICATION_CACHE_SUPPORT
	BOOL is_used_by_application_cache;
#endif // APPLICATION_CACHE_SUPPORT

	/// TRUE if the context is RAM only
	BOOL		RAM_only_cache;

	BOOL		is_writing_cache;

	BOOL		flushing_disk_cache;
	BOOL		do_check_ram_cache;
	BOOL		do_check_disk_cache;
	
#ifdef CACHE_MULTIPLE_FOLDERS
	/// TRUE to enable multiple directories in the context (default_
	BOOL		enable_directories;
#endif

	/// TRUE if the RAM Cache is flushing
	BOOL flushing_ram_cache;

	time_t		last_disk_cache_flush;
	time_t		last_ram_cache_flush;
	int			trial_count_cache_flush;
#ifdef CACHE_SYNC
	BOOL		activity_done;  // TRUE if some activity on the disk cache has been done (so the index can be out of sync)
#endif

	/**
	 * List of LRU items (older come first). It contains the list of Loaded URLs, also Unique ones (Unloaded are stored in url_store).
	 * It is the usage history of the global loaded URL list, sorted by category and last used time.
	 * It is complementary to url_store.
	 */
	Head LRU_list;
	/**
	 * Pointers to the Last Recently Used items, mapped on LRU_List. Basically LRU_List is used like if it is 3 lists
	 * LRU_ram -> ... -> LRU_temp -> ... -> LRU_disk -> ... -> LRU_end
	 * LRU_temp and LRU_ram may be NULL
	 * LRU_disk may be NULL in which case a non-null LRU_end precedes a new LRU_disk item
	 * Any URL with a cache object is added to this list, download files and
	 *  persistent cache entries are added to the LRU_disk list, all other to the LRU_temp
	 *  When a cache item is accessed it is moved to the end of the respective list
	 */
	URL_DataStorage		*LRU_ram, *LRU_temp, *LRU_disk;
	/**
	 * This is the list of URLs currently stored in the cache. It also contains Unloaded URLs, but NOT Unique ones (stored in LRU_list).
	 * It is complementary to LRU_list.
	 */
	URL_Store*		url_store;
	/// <Bookmark used to keep track of the position of the resources freed
	LinkObjectBookmark free_bm;

#ifndef SEARCH_ENGINE_CACHE
    uni_char	file_str[13]; /* ARRAY OK 2009-04-23 lucav */
#endif

#ifdef NEED_URL_VISIBLE_LINKS
	BOOL		visible_links;
#endif // NEED_URL_VISIBLE_LINKS

#ifdef SELFTEST
	friend class URLDD_Stats;

	/// List of statistics related to URL Data Descriptors; the objects are of type URLDD_Stats
	OpHashTable dd_stats;
	/// Bytes moved (with op_memmove) in the Data Descriptors (to check if it makes sense to improve them).
	/// Note: this is only related to the implementation of URL_DataDescriptor::ConsumeData()
	unsigned long dd_bytes_moved;
	/// Total number of Data Descriptors created
	unsigned long dd_total_num;
	/// Maximum number of descriptors alive at the same time
	unsigned long dd_peak_num;
	/// Estimation of the memory consumption used by the previously allocated descriptors (descriptors still alive need to be manually added)
	unsigned long dd_memory;
	/// TRUE if the invariants can be checked (this can still be subject to other conditions)
	BOOL check_invariants;

	/// Add a Data descriptor to the list of Data Descriptors currently alive, to check leaks and various problems
	/// PROPAGATE: NEVER
	OP_STATUS AddDataDescriptorForStats(URL_DataDescriptor *dd);
	/// Add a Data descriptor to the list of Data Descriptors currently alive, to check leaks and various problems
	/// PROPAGATE: NEVER
	URLDD_Stats *GetDataDescriptorForStats(URL_DataDescriptor *dd);
	/// Remove the Data Descriptor from the list, delete the stats object. Return TRUE if the
	/// Descriptor has been found, otherwise returns NULL
	/// PROPAGATE: NEVER
	BOOL RemoveDataDescriptorForStats(URL_DataDescriptor *dd);
	/// Keep track of how many bytes we move in the Data Descriptors (to check if it makes sense to improve them)
	/// PROPAGATE: NEVER
	void StatsAddBytesMemMoved(unsigned long bytes) { dd_bytes_moved+=bytes; }
	/// Keep track of how many bytes have been moved by the Data Descriptors
	/// PROPAGATE: NEVER
	unsigned long StatsGetBytesMemMoved() { return dd_bytes_moved; }
	/// Return the total memory allocated by all the descriptors (not necesarily at the same moment)
	unsigned long GetTotalMemoryAllocated(BOOL add_descriptors_alive, BOOL add_descriptors_deleted);
	/// Verify that without URLs the cache size is zero
	void VerifyCacheSize();
#endif // SELFTEST

	/// Return the cache location used for ReadDCacheDir(); OPFILE_ABSOLUTE_FOLDER means OPFILE_CACHE_FOLDER
	/// PROPAGATE: NEVER
#ifdef DISK_CACHE_SUPPORT
	OpFileFolder GetCacheLocationForFilesCorrected() { return (cache_loc==OPFILE_ABSOLUTE_FOLDER) ? OPFILE_CACHE_FOLDER : cache_loc; }
#else
	OpFileFolder GetCacheLocationForFilesCorrected() { return OPFILE_CACHE_FOLDER; }
#endif // DISK_CACHE_SUPPORT

	/// Initialization
	/// PROPAGATE: NEVER
	virtual void InitL();

	/**** Context_Manager "specification" (ex context_spec class) */
	/// Reference count
	unsigned int references_count;  // Used to be references (references ++, references --), but now it is exposed via methods
	/// Flag used to manage the pre-destruction phase
	BOOL predestruct_done;
#ifdef DISK_CACHE_SUPPORT
	/// Flag used to avoid a double save with delete of the URLs, that would probably create an empty index...
	BOOL cache_written_before_shutdown;
#endif	//DISK_CACHE_SUPPORT
	/// Cache size preference, used to manage the cache size when there is a change in the preferences, because if widgets
	/// don't have an explicit value, the cache size will be changed based on the main cache size preference
	int	cache_size_pref;

	/// Parent URL, used e.g. when context is created for specific mhtml file
	URL parentURL;
#ifdef URL_ENABLE_ASSOCIATED_FILES
friend class File_Storage;

	/* Implementation of the "temporary associated files", for storages that don't have a file namo or that are not persisted.
	   The context Manager keeps an hash table that is able to associate a URL with the correct Associated File requested.
	   WARNING: this is only intended to support temporary associated files, for special cases. For the other cases, the file is
	   retrieved from the hame.
	*/


	/// This counter is used to create "tickets" to store temporary associated files when the cache storage is without file name
	UINT32 assoc_file_counter;

	/// Hash table used to store the name of the temporary associated files
	OpAutoStringHashTable<TempAssociateFileTicket> ht_temp_assoc_files;

	/// Get a ticket name (based on rep_id and type), which can be used to create a true file name
	void GetTempAssociatedTicketName(URL_Rep *rep, URL::AssociatedFileType type, OpString &ticket);

	/// Return the temporary associated file type, if there is one
	OpString *GetTempAssociatedFileName(URL_Rep *rep, URL::AssociatedFileType type);
	/** Create an associated file, unless there is already one
		@param rep URL_Rep that will get the association
		@param type Type of the associated file
		@param file Associated File created (NULL if error)
		@param memory TRUE if the file has to be created in memory
	*/
	OP_STATUS CreateTempAssociatedFileName(URL_Rep *rep, URL::AssociatedFileType type, OpString *&file_name, BOOL allow_duplicated);
#endif // URL_ENABLE_ASSOCIATED_FILES


public:
	/// Public constructor
	/// PROPAGATE: NEVER
	Context_Manager(URL_CONTEXT_ID a_id = 0
#if defined DISK_CACHE_SUPPORT
			, OpFileFolder a_vlink_loc = OPFILE_CACHE_FOLDER
			, OpFileFolder a_cache_loc = OPFILE_CACHE_FOLDER
#endif
		);
	/** Second phase constructor
		@param a_cache_size_pref: preference that contain the size in KB; to directly set the size, call SetCacheSize() after this method
		@param load_index: true to load data and vlink indexes. Must be FALSE berfor calling URL::AddContextL() to avoid problems with cookies.
							In this case, ReadDCacheFileL() and ReadVisitedFileL() should be called later
		PROPAGATE: NEVER
	*/
	void ConstructPrefL(int a_cache_size_pref, BOOL load_indexes = TRUE);
	/** Second phase constructor
		@param a_cache_size: size in KB
		@param load_index: true to load data and vlink indexes. Must be FALSE befor calling URL::AddContextL() to avoid problems with cookies.
							In this case, ReadDCacheFileL() and ReadVisitedFileL() should be called later
		PROPAGATE: NEVER
	*/
	void ConstructSizeL(int a_cache_size, BOOL load_indexes = TRUE);
	/// Operations required before destructing the object
	/// PROPAGATE: NEVER
	void PreDestructStep(BOOL from_predestruct);

	/// Destructor
	/// PROPAGATE: NEVER
	virtual ~Context_Manager();
	
#if CACHE_SYNC == CACHE_SYNC_ON_CHANGE
	/// Signal that there has been activity in the cache; it is used to create the flag file (CACHE_ACTIVITY_FILE)
	/// PROPAGATE: NEVER
	void SignalCacheActivity();
	/// Signal that the cache is fully synchronized; the flag file (CACHE_ACTIVITY_FILE) will be deleted
	/// @param check_delete_queue if True, this operation will require Cache_Manager::delete_file_list to be empty
	/// PROPAGATE: NEVER
	void SignalCacheSynchronized(BOOL check_delete_queue);
#else
	/// Dummy version of SignalCacheActivity()
	/// PROPAGATE: NEVER
	void SignalCacheActivity() { }
	/// Dummy version of SignalCacheSynchronized()
	/// PROPAGATE: NEVER
	void SignalCacheSynchronized(BOOL check_delete_queue) { }
#endif

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	/** Increment the size of the Operator Cache and start a cache check
	    @param content_size size of the content of the URL
	    @param param url URL, used to call GetURLLength() and compute the estimated size of the URL
	    @param add_url_size_estimantion TRUE if we want the estimation of the URL size to be added; this is only an hint
	    PROPAGATE: NEVER
	*/
	void AddToOCacheSize(OpFileLength content_size, URL_Rep *url, BOOL add_url_size_estimantion);
	/** Decrement the size of the Operator Cache
	    @param content_size size of the content of the URL
	    @param param url URL, used to call GetURLLength() and compute the estimated size of the URL
		@enable_checks if FALSE, it disables consistency checks; basically it should always been TRUE, except for UnsetFinished()
			     (this only affect the code if SEFLTEST is on)
	    PROPAGATE: NEVER
	*/
	void SubFromOCacheSize(OpFileLength content_size, URL_Rep *url, BOOL enable_checks);
#endif

#ifdef CACHE_MULTIPLE_FOLDERS
	// Change if the context support multiple directories
	void SetEnableDirectories(BOOL b) { enable_directories = b; }
#endif

private:
	/// Internal, private constructor
	/// PROPAGATE: NEVER
	void InternalInit();
	/// Internal, private destructor
	/// PROPAGATE: NEVER
	void InternalDestruct();

#ifdef CACHE_MULTIPLE_FOLDERS
	/// Get the full name of the file, with the directory ("sesn" or "g_XXXX")
	/// PROPAGATE: NEVER
	static OP_STATUS GetNameWithFolders(OpString &full_name, BOOL session_only, const uni_char *file_name);
#endif

protected:
#if defined(DISK_CACHE_SUPPORT) && !defined(SEARCH_ENGINE_CACHE)
	/// Write the cache index to the disk (legacy version)
	/// PROPAGATE: NEVER. Higher level methods should instead be propagated
	void			WriteIndexFileL(URLLinkHead &list, uint32 tag, OpStringC filename, OpFileFolder folder, BOOL write_lastentry, BOOL destroy_urls);
	/// Write the cache index to the disk (fast version)
	/// PROPAGATE: NEVER. Higher level methods should instead be propagated
	void			WriteIndexFileSimpleL(URLLinkHead &list, uint32 tag, OpStringC filename, OpFileFolder folder, BOOL write_lastentry, BOOL destroy_urls);
	/// Read a cache index; return the number of files written
	/// PROPAGATE: NEVER. Higher level methods should instead be propagated
	int				ReadCacheIndexFileL(FileName_Store &filenames, BOOL sync_cache, const OpStringC &name, const OpStringC &name_old, OpFileFolder folder, uint32 tag, BOOL read_last_entry, BOOL empty_disk_cache);
#endif // defined(DISK_CACHE_SUPPORT) && !defined(SEARCH_ENGINE_CACHE)
	/// Force the RAM cache to fit into its size (or into force_size if it is >=0) and return the waiting time before next check
	/// PROPAGATE: NEVER. Higher level methods should instead be propagated
	int				CheckRamCacheSize(OpFileLength force_size= (OpFileLength) -1);
	
protected:
	/// Add an URL_Rep to the URL list
	/// PROPAGATE: NEVER
	void			AddURL_Rep(URL_Rep *new_rep);
	/// Retrieve an URL_Rep using the URL_Name_Components_st
	/// PROPAGATE: CONDITIONAL: when not remote and URL not found (This will be a problem...)
	/// PROPAGATE_NOTE: It MUST become PROPAGATE: NEVER
	URL_Rep *		GetResolvedURL(URL_Name_Components_st &url, BOOL create = TRUE);
	/// Retrieve an URL_Rep using the URL_ID (this operation is slow, as the list need to be checked)
	/// PROPAGATE: CONDITIONAL: when not remote and URL not found (This will be a problem...)
	/// PROPAGATE_NOTE: It MUST become PROPAGATE: NEVER
	URL_Rep *		LocateURL(URL_ID id);
	
	/// Add the URL to the LRU list, in the right position (RAM, Temp or Disk)
	/// PROPAGATE: NEVER. But has to be verified...
	void			SetLRU_Item(URL_DataStorage *url);
	/// Remove the URL from the LRU list, updating the list itself
	/// PROPAGATE: NEVER. But has to be verified...
	void			RemoveLRU_Item(URL_DataStorage *url_ds);

	/// Return TRUE if the Context Manager is remote (e.g. sockets)
	/// PROPAGATE: NEVER.
	virtual BOOL IsRemoteManager() { return FALSE; }

	////// Information hiding of the reference count
	/// Return the referecnes count (introduced to fix DSK-301823)
	/// PROPAGATE: NEVER.
	unsigned int GetReferencesCount() { return references_count; }
#ifdef SELFTEST
	/// Debug: set the reference count
	void DebugSetReferencesCount(unsigned int num) { references_count=num; }
#endif
	/// Increment the reference count
	/// PROPAGATE: NEVER.
	void IncReferencesCount() { references_count ++; }
	/// Decrement the reference count
	/// PROPAGATE: NEVER.
	void DecReferencesCount() { references_count --; }

#if defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)
	/// Calls urlManager->DeleteFiles(), signaling also that there is some activity
	void DeleteFiles(FileName_Store &filenames);
#endif // defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE)

#if ( defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE) ) && !defined(SEARCH_ENGINE_CACHE)
#ifdef URL_ENABLE_ASSOCIATED_FILES
	/**
	Based on choose_present, build a third list with all the associated files that have (or don't have) a corresponding cache file
		WARNING: the filenames list MUST have already been synchronized with the disk (so basically ready for urlManager->DeleteFiles())
		The rational of the algorithm currently implemented is that:
		- Associated files are supposed to be a relatively small number (this could change if Carakan will ever start use them)
		- Associated files are only linked to URLs saved on a real file (could change for performances if if Carakan will ever start use them)
		- Normal files are supposed to be already checked against the index, so we are checking an already purged list, which is good
		@param filenames List of normal files in the disk cache
		@param associated List of associated files in the disk cache
		@param result Computed list
		@param choose_present if TRUE, result will contain all the associated file with a corresponding main file, else only the ones without
		/// PROPAGATE: NEVER. Higher level methods should instead be propagated
	*/
	static OP_STATUS CheckAssociatedFilesList(FileName_Store &filenames, FileName_Store &associated, FileName_Store &result, BOOL choose_present);
#endif // URL_ENABLE_ASSOCIATED_FILES
#endif  // ( defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE) ) && !defined(SEARCH_ENGINE_CACHE)

public:
#if ( defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE) ) && !defined(SEARCH_ENGINE_CACHE)
	/**
	  Read a directory cache and construct a list of the files presents
	  @param filenames List of cache files
	  @param associated List of files associated to the cache
	  @param specialFolder [OPFILE_CACHE_FOLDER] Folder of interest
	  @param check_generations TRUE if the function has to check also for the presences of the g_* directories; this is used only if the TWEAK is enabled, but the parameter is always present
	  @param check_associated_files TRUE if the function, will scan the directories with associated files
	  @param sub_folder optional sub folder to look into (this is supposed to be used internally by the function itself)
	  @param max_files >0 to put a limit on the number of files to add to the list
	  @param skip_directories TRUE to avoid deleting directories (that starts with "opr"), FALSE to delete them
	  @param check_sessions TRUE to check also all the files in the sesn directories (this is used by the selftests)
	  /// PROPAGATE: NEVER. Higher level methods should instead be propagated
	*/
	static void	ReadDCacheDir(FileName_Store &filenames, FileName_Store &associated, OpFileFolder specialFolder, BOOL check_generations, BOOL check_associated_files, const uni_char *sub_folder, int max_files=-1, BOOL skip_directories=TRUE, BOOL check_sessions=FALSE);
#endif  // ( defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE) ) && !defined(SEARCH_ENGINE_CACHE)

	/// Make an URL unique, and remove it from the cache list
	/// PROPAGATE: NEVER  (verify)
	void			MakeUnique(URL_Rep *url);
	/// Make all URLs unique, and remove them from the cache list
	/// PROPAGATE: NEVER  (verify)
	void			MakeAllURLsUnique();
	/// Unload and delete the URL (if possibile)
	/// PROPAGATE: NEVER
	void			DestroyURL(URL &url);
	/// Unload and delete the URL_Rep (if possibile)
	/// PROPAGATE: NEVER
	void			DestroyURL(URL_Rep *rep);

	/** Try to free as much memory as possible.
		It delete URLs that are no longer required, and also force the cache to adhere to its size.
		The URLs are also dumped to disk.

		@param all TRUE if all the URLs have to be checked, even if this mean to go over the threshold (100 ms)
		@return TRUE if not all the URLs have been checked, because the time threshold have been reached

		PROPAGATE: CONDITIONAL  (no remote)
	*/
	BOOL			FreeUnusedResources(BOOL all = TRUE SELFTEST_PARAM_COMMA_BEFORE(DebugFreeUnusedResources *dfur = NULL) );
	/// Delete all the URLs that have been visited
	/// PROPAGATE: CONDITIONAL  (no remote)
	void			DeleteVisitedLinks();
	/// Remove the message handler from all the URLs
	/// PROPAGATE: NEVER (verify)
	void			RemoveMessageHandler(MessageHandler* mh);
	/// Remove the URL from the list.
	/// PROPAGATE: CONDITIONAL? (no remote. verify)
	void			RemoveFromStorage(URL_Rep *url);
	/// Stop to load all the URLs
	/// PROPAGATE: ALWAYS (FIXME: in the future execution will depend on the context manager)
	void			CacheCleanUp(BOOL ignore_downloads);
#if defined (DISK_CACHE_SUPPORT) || defined (CACHE_PURGE)
    /// Delete all the files in the cache directory
	/// PROPAGATE: ALWAYS
	void            CleanDCacheDirL();
#endif
#ifdef _OPERA_DEBUG_DOC_
	// Write Debug informations on the URL
	// PROPAGATE: NEVER
	void			WriteDebugInfo(URL &url);
#endif
	/// Return TRUE id this context manager is RAM only
	// PROPAGATE: NEVER (this information cannot really be accurate without asking to all the local managers)
	BOOL			GetIsRAM_Only() const;
	/// Set if this context manager is RAM only
	// PROPAGATE: NEVER
	void			SetIsRAM_Only(BOOL ram_only){RAM_only_cache = ram_only;}

	/** Delete objects from the disk cache
		@param mode The cache is separated into normal cache and operator cache.
			This argument specifies on which to operate. It is also possible to
			operate on both at the same time.
		@param delete_app_cache TRUE if attributes with KIsApplicationCacheURL should be deleted as well
		@param cache_operation_target URL or URL prefix. See @ref prefix_match.
		@param prefix_match Specifies whether the @ref prefix_match parameter is
			a URL or a URL prefix. If it is a prefix, every object matching this
			prefix in the cache will be deleted. Otherwise, only the specified URL
			will be deleted.
		@param origin_time Only delete objects older than this timestamp. The
			value 0 means that no timestamp comparison should be done.
		PROPAGATE: ALWAYS
	 */
	void			EmptyDCache(BOOL delete_app_cache
		OEM_EXT_OPER_CACHE_MNG_BEFORE(delete_cache_mode mode = EMPTY_NORMAL_CACHE)    OPER_CACHE_COMMA()
		OEM_CACHE_OPER_AFTER(const char *cache_operation_target=NULL)
		OEM_CACHE_OPER_AFTER(int flags=0)
		OEM_CACHE_OPER(time_t origin_time=0));

#ifdef UNUSED_CODE
	/// Delete all URL visited too much time ago
	void			PurgeVisitedLinks( time_t maxage );
#endif

#ifndef SEARCH_ENGINE_CACHE
	/// Increment the number used for the next cache file
	/// PROPAGATE: NEVER
	void			IncFileStr();
#endif

	/// Save the cache, writing the index
	/// PROPAGATE: ALWAYS
	void AutoSaveCacheL();

	/// Get the name of the next cache file, including the subdirectory it should go into
	/// PROPAGATE: NEVER
	OP_STATUS		GetNewFileNameCopy(OpStringS &name, const uni_char* ext, OpFileFolder &folder, BOOL session_only
						OEM_EXT_OPER_CACHE_MNG_BEFORE(BOOL useOperatorCache=FALSE));

	/// Set the new cache size (in bytes), and also start to enforce it.
	/// PROPAGATE: NEVER. Note: Opera should probably be responsible to manage each Context, because it is impossible to define
	/// a proper behavior in a generic way.
	void			SetCacheSize(OpFileLength new_size);

	/// Increase the cache usage and start forcing the limit if required
	/// @param content_size size of the content of the URL
	/// @param param url URL, used to call GetURLLength() and compute the estimated size of the URL
	/// @param add_url_size_estimantion TRUE if we want the estimation of the URL size to be added; this is only an hint
	/// PROPAGATE: NEVER.
	void			AddToCacheSize(OpFileLength content_size, URL_Rep *url, BOOL add_url_size_estimantion);
	/// Decrease the cache usage
	/// @param content_size size of the content of the URL
	/// @param param url URL, used to call GetURLLength() and compute the estimated size of the URL
	/// @enable_checks if FALSE, it disables consistency checks; basically it should always been TRUE, except for UnsetFinished()
    ///	     (this only affect the code if SEFLTEST is on)
	/// PROPAGATE: NEVER.
	void			SubFromCacheSize(OpFileLength content_size, URL_Rep *url, BOOL enable_checks);

#ifdef APPLICATION_CACHE_SUPPORT
	void SetIsUsedByApplicationCache(BOOL used){ is_used_by_application_cache = used; }
#endif // APPLICATION_CACHE_SUPPORT


#ifdef DISK_CACHE_SUPPORT
#ifdef SEARCH_ENGINE_CACHE
	/// Adds an URLs to the index
	/// PROPAGATE: NEVER.
	BOOL			IndexFileL(CacheIndex &index, URL_Rep *url_rep, BOOL force_index, OpFileLength cache_size_limit);
	/// Mark the URL as temporary, becoming a session URL ("sesn" is also added to the name; it is probably worth checking if the file is also moved)
	/// PROPAGATE: NEVER.
	void			MakeFileTemporary(URL_Rep *url_rep);
#endif

#ifndef SEARCH_ENGINE_CACHE
	/// Posts a MSG_FLUSH_CACHE message; this will end-up calling DoCheckCache()
	/// PROPAGATE: CONDITIONAL (propagate CheckDCacheSize() when !g_main_message_handler)
	void			StartCheckDCache();
	/// Enforce the cache size
	/// PROPAGATE: ALWAYS. (Return value does not need to be propagated)
	/// Note: this method is slightly changed by Context_Manager_Disk, to trace and delete containers
	virtual int	CheckDCacheSize(BOOL all=FALSE);

	/// Read the index of the visited URLs
	/// PROPAGATE: ALWAYS.
	void			ReadVisitedFileL();
	/// Read the index of the cache dURLs
	/// PROPAGATE: ALWAYS.
	void			ReadDCacheFileL();
	OP_STATUS		OpenCacheIndexFile(OpFile **fp, const OpStringC &name, const OpStringC &name_old, OpFileFolder folder);
	OP_STATUS		OpenIndexFile(OpFile **fp, const OpStringC &name, OpFileFolder folder, OpFileOpenMode mode);
#endif  // !SEARCH_ENGINE_CACHE
#ifdef SELFTEST
	/// Write an index; it does not pretend to really work, it is just for benchmarks and tests
	/// PROPAGATE: NEVER.
	void			TestWriteIndexFileL(OpStringC filename, OpFileFolder folder, BOOL fast);
	// Read an index; it does not pretend to really work, it is just for benchmarks and tests. The next entry is deleted before the test
	/// PROPAGATE: NEVER.
	void			TestReadIndexFileL(OpStringC filename, OpFileFolder folder, int &num_entries, OpString &next_entry);
	/// Check up to 10 URLs to see if they are double; this is performed to check against double loading at startup
	BOOL CheckDoubleURLs();
#endif // SELFTEST
#endif  // DISK_CACHE_SUPPORT

	/// Function that enforce the cache size limit (both on RAM and disk)
	/// PROPAGATE: ALWAYS.
	void			DoCheckCache(int &r_temp, int &d_temp);
	/// Request a clean in the RAM cache
	/// PROPAGATE: CONDITIONAL (propagate CheckRamCacheSize() when !g_main_message_handler)
		void			StartCheckRamCache();

	/// Increment the size of the RAM cache
	/// @param content_size size of the content of the URL
	/// @param param url URL, used to call GetURLLength() and compute the estimated size of the URL
	/// @param add_url_size_estimantion TRUE if we want the estimation of the URL size to be added; this is only an hint
	/// PROPAGATE: NEVER.
	void AddToRamCacheSize(OpFileLength content_size, URL_Rep *url, BOOL add_url_size_estimantion);
	/// Decrement the size of the RAM cache
	/// @param content_size size of the content of the URL
	/// @param param url URL, used to call GetURLLength() and compute the estimated size of the URL
	/// PROPAGATE: NEVER.
	void SubFromRamCacheSize(OpFileLength content_size, URL_Rep *url);

#ifdef NEED_URL_VISIBLE_LINKS
	/// PROPAGATE: NEVER.
	BOOL			GetVisLinks() const{return visible_links;}
	/// PROPAGATE: NEVER.
	void			SetVisLinks(BOOL val=TRUE) {visible_links=val;}
#endif // NEED_URL_VISIBLE_LINKS

#ifdef NEED_CACHE_EMPTY_INFO
	/** checks if there are any items in cache */
	/// PROPAGATE: CONDITIONAL (no remote)
	BOOL			IsCacheEmpty();
#endif // NEED_CACHE_EMPTY_INFO

	/// Return the cache location used for ReadDCacheDir()
	/// PROPAGATE: NEVER
#ifdef DISK_CACHE_SUPPORT
	OpFileFolder GetCacheLocationForFiles() { return cache_loc; }
#else
	OpFileFolder GetCacheLocationForFiles() { return OPFILE_CACHE_FOLDER; }
#endif // DISK_CACHE_SUPPORT

#ifdef STRICT_CACHE_LIMIT
private:
    /** Maximum amount of memory (in bytes) we're supposed to use for various cacheing issues. */
    int memory_limit;
 
    /** Amount of cache memory (in bytes) used but not registered with ramcache_used.
     * Supposed to be used to adjust ramcache_used for comparison with
     * memory_limit, e.g. when deciding whether to abort a download or not.
     *
     * WARNINGS: There is no reason to believe that this value will be
     * accurate, neither is there any reason to believe that this value
     * is actually used.
     */
    int ramcache_used_extra;
 
    /** Safety margin for the memory limit (in bytes).
     * In other words, always try to leave this much memory free
     * in comparison to the memory_limit.
     */
    int memory_limit_margin;

public:
    /// Return the memory limit
	/// PROPAGATE: NEVER.
	int GetMemoryLimit() { return memory_limit; };
    /// Set the memory limit
	/// PROPAGATE: NEVER.
	void SetMemoryLimit(int new_limit) { memory_limit=new_limit; };
 
    /// Return the memory limit margin
	/// PROPAGATE: NEVER.
	int GetMemoryLimitMargin() { return memory_limit_margin; };
    /// Set the memory limit margin
	/// PROPAGATE: NEVER.
	void SetMemoryLimitMargin(int new_margin) { memory_limit_margin=new_margin; };
 
    /// Set the extra bytes used by the RAM cache
	/// PROPAGATE: NEVER.
	int GetRamCacheSizeExtra() { return ramcache_used_extra; };
	/// Increase the extra bytes used by the RAM cache
	/// PROPAGATE: NEVER.
    void AddToRamCacheSizeExtra(int more) { ramcache_used_extra+=more; };
	/// Decrease the extra bytes used by the RAM cache
	/// PROPAGATE: NEVER.
    void SubFromRamCacheSizeExtra(int less) { ramcache_used_extra-=less; };

    unsigned int GetUsedUnderMemoryLimit() { return memory_limit_margin + ramcache_used_extra + size_ram.GetUsed(); };
    void MakeRoomUnderMemoryLimit(int size);
#endif /* STRICT_CACHE_LIMIT */

    /// Get the number of bytes used by the RAM cache
	/// PROPAGATE: NEVER.
	OpFileLength GetRamCacheUsed() { return size_ram.GetUsed(); };

	/// Get the number of bytes used by the Disk cache
	/// PROPAGATE: NEVER.
	OpFileLength GetDiskCacheUsed() { return size_disk.GetUsed(); };

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	/// Get the number of bytes used by the OEM cache
	/// PROPAGATE: NEVER.
	OpFileLength GetOEMCacheUsed() { return size_oem.GetUsed(); };
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT

	/// Remove HTTPS URLs and URLs with password or authentication informations
	/// PROPAGATE: ALWAYS. This has to be verified
	void RemoveSensitiveCacheData();

#if defined(NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS)
	/// Stop and restart every URL in url_store
	/// PROPAGATE: ALWAYS.
	void			RestartAllConnections();
	/// Aborts every loading in the URLs of url_store
	/// PROPAGATE: ALWAYS.
	void			StopAllLoadingURLs();
#endif // NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS

#ifdef SELFTEST
	/// Add a Data descriptor to the list of Data Descriptors currently alive, to check leaks and various problems
	/// The caller is not allowed to touch or delete the objects
	/// PROPAGATE: NEVER.
	OP_STATUS GetDataDescriptorsListForStats(OpVector<URLDD_Stats> &vector);
#endif

protected:
	
#ifdef DISK_CACHE_SUPPORT
	// OPFILE_ABSOLUTE_FOLDER means do not store (cache directory is OPFILE_CACHE_FOLDER), visited link file also goes to this directory
	/// Directory of the cache (OPFILE_ABSOLUTE_FOLDER means do not store, with cache directory being OPFILE_CACHE_FOLDER)
	OpFileFolder cache_loc;
	/// Directory of the visited links
	OpFileFolder vlink_loc;
#endif // DISK_CACHE_SUPPORT
	/// ID of the current context. Each widget has a separated context, plus there are dedicated context for special
	/// operations, like the privacy mode.
	/// In the future, each thread / process will get a dedicated context
	/// Context ID are supposed to be unique by thread, it has to be seen if they will be unique across threads.
	URL_CONTEXT_ID context_id;

protected:
#ifdef _OPERA_DEBUG_DOC_
	/// Returns all the memory used by the URL_Rep in url_store
	/// PROPAGATE: NEVER
	void GetCacheMemUsed(DebugUrlMemory &debug);
#endif

#ifdef SUPPORT_RIM_MDS_CACHE_INFO
public: // OpCacheInfoReconcileHandler
	/// PROPAGATE: NEVER?
	void RegisterSession(OpCacheInfoSession* session) { if( m_cacheInfoSession ) OP_DELETE(m_cacheInfoSession); m_cacheInfoSession = session; }
	/// PROPAGATE: NEVER?
	void StartReconcile(int session_id);
	/// PROPAGATE: NEVER?
	void CancelReconcile(int sesStartCheckRamCacheion_id);
public: // new
	/// PROPAGATE: NEVER?
	OP_STATUS ReportCacheItem(URL_Rep* url, BOOL added);
	/// PROPAGATE: NEVER?
	OP_STATUS ReportSomeCacheItems(int session_id);
private:
	OpCacheInfoSession*	m_cacheInfoSession;
	int					m_cacheInfoSessionId;
	Head				m_cacheInfoList;
#endif // SUPPORT_RIM_MDS_CACHE_INFO

public:
	/////////////// Public interface - OPTIONAL methods /////////////////////////////////
	/**
		Enable a Context_Manager to save data in a "custom" way, for example using containers

		@param storage Cache_Storage to save
		@param filename optional file name
		@param ops Status of the operation (only if TRUE is returned)
		@return TRUE if the Manager saved the storage, so this operation bypassing the normal cache saving (so Cache_Storage::SaveToFile need to return immediately)

		PROPAGATE: NEVER
	*/
	virtual BOOL BypassStorageSave(Cache_Storage *storage, const OpStringC &filename, OP_STATUS &ops) { return FALSE; }
	/// Enable a Context Manager to bypass, for example, File_Storage::RetrieveData().
	/// Warning: implementation could call Cache_Storage::RetrieveData(), so be carefull with infinite recursion...
	/// PROPAGATE: NEVER
	virtual BOOL BypassStorageRetrieveData(Cache_Storage *storage, URL_DataDescriptor *desc, BOOL &more, unsigned long &ret, OP_STATUS &ops) { return FALSE; }
	/// Enable a Context Manager to bypass File_Storage::Flush().
	/// PROPAGATE: NEVER
	virtual BOOL BypassStorageFlush(File_Storage *storage) { return FALSE; }
	/// Enable a Context Manager to bypass File_Storage::TruncateAndReset().
	/// PROPAGATE: NEVER
	virtual BOOL BypassStorageTruncateAndReset(File_Storage *storage) { return FALSE; }

	/// Check if the containers are working correctly
	/// PROPAGATE: NEVER
	virtual BOOL CheckInvariants() { return TRUE; };

#ifdef SELFTEST
	/// Used to avoid checking invariants after destructive tests (like the ones that call BrutalDelete()
	/// PROPAGATE: NEVER
	void SetInvariantsCheck(BOOL b) { check_invariants=b; }
	/// Check if a Cache_Storage object is valid (mainly a check for containers)
	/// PROPAGATE: NEVER
	virtual BOOL CheckCacheStorage(Cache_Storage *cs) { return TRUE; };
#endif // SELFTEST

/////////////// Public interface - MANDATORY methods /////////////////////////////////
	/// Low level function that enforce the cache size limit. Returns the number of milliseconds to wait
	/// NOTE: cache_limit needs to be taken directly from the Context_Manager, as the values need to be updated during the call
	/// PROPAGATE: NEVER. Higher level methods should instead be propagated
	virtual int	CheckCacheSize(URL_DataStorage *start_item, URL_DataStorage *end_item, 
					BOOL &flush_flag, time_t &last_flush, int period,
					CacheLimit *cache_limit, OpFileLength size_limit, OpFileLength force_size,
					BOOL check_persistent
				#if defined __OEM_EXTENDED_CACHE_MANAGEMENT
					, BOOL flush_never_flush_too = FALSE
				#endif // __OEM_EXTENDED_CACHE_MANAGEMENT
				) = 0;

#ifdef DISK_CACHE_SUPPORT
	/**
		Write the visited links and the data cache indexes, saving the state of the cache
		@param sort_and_limit TRUE to sort the URLs in cache and limit the size used by the disk cache
		@param destroy_urls TRUE if all the URLs have to be destroyed, of course after saving them in the index. This operation is supposed
							to be performed only just before closing Opera
		/// PROPAGATE: ALWAYS.
	*/
	virtual void WriteCacheIndexesL(BOOL sort_and_limit, BOOL destroy_urls) = 0;

	/**
		Delete all the disk indexes associated with the context manager
	*/
	virtual OP_STATUS DeleteCacheIndexes() { return OpStatus::OK; }
#endif // DISK_CACHE_SUPPORT
};

#endif // CONTEXT_MANAGER_H_
