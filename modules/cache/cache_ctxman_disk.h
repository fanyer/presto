/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Luca Venturi
**
** This class is the implementation of a context manager that store the cache on disk. This is the default on desktop.
** At some point, the manage of the RAM part should be moved to a dedicated class
*/

#ifndef CACHE_CTX_MAN_DISK_H
#define CACHE_CTX_MAN_DISK_H

#include "modules/cache/context_man.h"

#if CACHE_CONTAINERS_ENTRIES>0
	#include "modules/cache/cache_container.h"
	class File_Storage;

	/// Used to store the names of the container files that are marked because a URL has to be deleted.
	/// If a MarkedContainer is present in cnt_marked, it means that at least one of the URLs associated with it has to be deleted.
	// The current logic is that the whole container (and its associated URLs) need to be deleted at some point, but this could change in the future
	class MarkedContainer
	{
	private:
		BOOL deleted;			// True if the file has already been deleted
		OpString name;			// Name of the file
		OpFileFolder folder;	// Directory
		OpStringHashTable<MarkedContainer> *ht; // Hash table containing the entry
		UINT32 in_use;				// Number of URLs that are using it (this is used during the "Garbage Collection", temporarily, and the important thing is when it is >0 )

	public:
		/// Default constructor
		MarkedContainer() { deleted=FALSE; folder=OPFILE_ABSOLUTE_FOLDER; ht=NULL; ResetInUse(); }
		/// Set the name of the file
		OP_STATUS SetFileName(OpFileFolder file_folder, const uni_char * file_name, OpStringHashTable<MarkedContainer> *table) { folder=file_folder; OP_ASSERT(folder>=0); ht=table; return name.Set(file_name); }
		/// Get only the folder
		OpFileFolder GetFolder() { return folder; }
		/// Remove the entry from the Hash Table
		OP_STATUS Remove() { if(!ht) return OpStatus::ERR_NULL_POINTER; MarkedContainer *data; return ht->Remove(name.CStr(), &data); }
		/// Get the file name
		const uni_char *GetFileName(OpFileFolder &file_folder) { OP_ASSERT(folder>=0); file_folder=folder; return name.CStr();}
		/// Get the file name
		const uni_char *GetFileNameOnly() { return name.CStr();}
		/// Delete the file only if it has not already been deleted (deleted==FALSE)
		OP_STATUS AutoDelete();
		/// Manually set the value of deleted. Use carefully...
		void SetDeleted(BOOL b=TRUE) { deleted=b; }
		/// TRUE if the container file has been deleted
		BOOL IsDeleted() { return deleted; }
		/// Container no longer In Use
		void ResetInUse() { in_use=0; }
		/// Add one user
		void IncUsed() { in_use++; }
		/// Return the number of users
		int GetUsed() { return in_use; }
	};
#endif

/**
  Class that store the cache on the disk. This is a RAM + Disk implementation
*/
class Context_Manager_Disk: public Context_Manager
{
private:
	friend class GarbageCollectMarkedContainersListeners;
	friend class File_Storage;
	friend class CacheTester;
	class GCPhaseDeleteFiles;
	friend class GCPhaseDeleteFiles;

#if CACHE_CONTAINERS_ENTRIES>0
	/// Listener used to avoid calling GetIterator(), as it is crash prone
	class GCPhaseBase: public OpHashTableForEachListener
	{
	protected:
		Context_Manager_Disk *mng;  ///< Context Manager calling the listener
		OP_STATUS ops;				///< Return status of the operation

	public:
		GCPhaseBase(Context_Manager_Disk *manager) { mng=manager; OP_ASSERT(mng); ops=OpStatus::OK; }
		virtual void HandleKeyData(const void* key, void* data) { if(data && OpStatus::IsSuccess(ops)) HandleContainer((const uni_char *) key, (MarkedContainer *) data); }

		/// Function called for each container; the pointer is guaranteed to be not NULL, and an error (registered on ops) will stop the iteration
		virtual void HandleContainer(const uni_char *file_name, MarkedContainer *cnt) = 0;

		/// Return the status 
		OP_STATUS GetStatus() { return ops; }
	};

	/// Listener used to reset the containers
	class GCPhaseReset: public GCPhaseBase
	{
	public:
		GCPhaseReset(Context_Manager_Disk *manager): GCPhaseBase(manager) { }

		virtual void HandleContainer(const uni_char *file_name, MarkedContainer *cnt) { cnt->ResetInUse(); }
	};
	
	/// Cache files belonging to containers no longer in use are deleted
	class GCPhaseDeleteFiles: public GCPhaseBase
	{
	public:
		GCPhaseDeleteFiles(Context_Manager_Disk *manager): GCPhaseBase(manager) { }

		virtual void HandleContainer(const uni_char *file_name, MarkedContainer *cnt);
	};
#endif // CACHE_CONTAINERS_ENTRIES>0

public:
	virtual void InitL();
	virtual ~Context_Manager_Disk();
	Context_Manager_Disk(URL_CONTEXT_ID a_id = 0
#if defined DISK_CACHE_SUPPORT
			, OpFileFolder a_vlink_loc = OPFILE_CACHE_FOLDER
			, OpFileFolder a_cache_loc = OPFILE_CACHE_FOLDER
				);
#else
				);
#endif // DISK_CACHE_SUPPORT

protected:
	/// Return a temporary buffer used for files, or NULL if it is not available
	/// PROPAGATE: NEVER
	unsigned char * GetStreamBuffer();

	/// Sort the URL list by last access date, and remove all the old URls that dall outside the size limit
	/// PROPAGATE: NEVER. Higher level methods should instead be propagated
	OpFileLength	SortURLListByDateAndLimit(URLLinkHead &list, URLLinkHead &rejects, OpFileLength size_limit, BOOL destroy_urls);

#if CACHE_CONTAINERS_ENTRIES>0
	/// Buffers used for the containers (only one site is allowed in a container)
	CacheContainer *cnt_buffers[CACHE_CONTAINERS_BUFFERS];
	/// Container files marked (at least one URLs need to be deleted); the key is the uni_char * pointer, data is the corresponding MarkedContainer, and OpAutoPointerHashTable will take care of delete the data, so memory leak should not happen
	OpAutoStringHashTable<MarkedContainer> cnt_marked;
	/// TRUE to enable containers creation; FALSE will never let a file qualify for containers
	BOOL enable_containers;

	/**
		Try to add a Cache_Storage to a container. It is normal to fail, for
		example because it is not suitable for it.
		If it is added, the file name is updated.
		The caller is responsible for checking that the size is suitable
		(this to avoid a useless method call), and an ASSERT will check it.
		@param storage Storage to try to add.
		@param added TRUE if it has been added
		PROPAGATE: NEVER
	*/
	OP_STATUS AddCacheItemToContainer(File_Storage *storage, BOOL &added);
	/**
		Try to retrieve the file from the container, and store it on cache_content
		The caller is responsible for checking that the storage has a Container ID
		(this to avoid a useless method call), and an ASSERT will check it.
		@param storage Storage to try to read.
		@param found TRUE if it has been found
		@param desc Where to store the content
		PROPAGATE: NEVER
	*/
	OP_STATUS RetrieveCacheItemFromContainerAndStoreIt(Cache_Storage *storage, BOOL &found, URL_DataDescriptor *desc);
	/**
		Try to retrieve the file from the container, and store it on a file
		The caller is responsible for checking that the storage has a Container ID
		(this to avoid a useless method call), and an ASSERT will check it.
		@param storage Storage to try to read.
		@param found TRUE if it has been found
		@param desc Where to store the content
		PROPAGATE: NEVER
	*/
	OP_STATUS RetrieveCacheItemFromContainerAndStoreIt(Cache_Storage *storage, BOOL &found, const OpStringC *file);
	/// Flush all the containers, writing them to disk
	/// @param reset if TRUE all the containers will be reset, so they will become empty
	/// PROPAGATE: NEVER
	OP_STATUS FlushContainers(BOOL reset = TRUE);
	/// Check if the container is present in RAM or disk. In this case, we suppose that the delete of one file delete the whole container (Core 2.3)
	/// PROPAGATE: NEVER
	BOOL IsContainerPresent(Cache_Storage *cs);
	/// Check if the container is present on RAM.
	/// PROPAGATE: NEVER
	BOOL IsContainerInRam(const uni_char *file_name, int *index, BOOL *modified);
	/**
		Mark the container for being deleted in a later stage; this is used, for example, when the files are updated
		@param storage Storage that has to be marked as deleted
		PROPAGATE: NEVER
	*/
	OP_STATUS MarkContainerForDelete(File_Storage *storage);

	/**
	Retrieve the cache container that store the required file.
	The caller is responsible for checking that the storage has a Container ID
	(this to avoid a useless method call), and an ASSERT will check it.
	@param storage Storage to try to read.
	@param container Container, NULL if not found
	PROPAGATE: NEVER
	*/
	OP_STATUS RetrieveCacheContainer(Cache_Storage *storage, CacheContainer *&cont);
	/// Flush a single container, writing it to disk
	/// @param reset if TRUE the container is reset, so it will become empty
	/// PROPAGATE: NEVER
	OP_STATUS FlushContainer(CacheContainer *cont, BOOL reset);

	/** Execute the first phase (Trace) of a "Garbage Collection" like operation: all the URLs are checked to see if they block the delete of a Marked Container.
		This operation is relatively expensive, as it is related to the number of URLs present in the cache, not to the number of containers marked for delete
		@param iter_ds Iterator with the URL_DataStorage objects to analyze (this is used to unify the two ManageMarkedContainers() cases)
		@param full TRUE execute the Tracing in any case, FALSE if it will be executed only if there is enough work to justify the Trace phase
		@param descr Description

		NOTE: This method does not alter the number of elements of cnt_marked, only their state.
		NOTE: This method will Delete URLs that belong to a marked container and that are not in use (even if the container in the end will not be deleted)
		NOTE: It is normal to call this method multiple times (with different lists) and only calling once GarbageCollectMarkedContainers(); it prevents corruption.
			  It is NOT safe to perform cache operations between multiple calls of TraceMarkedContainers() and GarbageCollectMarkedContainers()

		PROPAGATE: NEVER
	*/
	OP_STATUS TraceMarkedContainers(ListIteratorDelete<URL_DataStorage> *iter_ds, BOOL full, const uni_char *descr);


	/** Execute the second phase (Delete) of a "Garbage Collection" like operation: all the containers marked for deletion and not in use, will be deleted
		In this process, all the URLs that belong to a marked container and that are not in use, will be deleted. This can waste a bit of bandwidth,
		but it also reduces the CPU usage and simplify the delete.

		After a successful call of this method, cnt_marked will be empty.

		PROPAGATE: NEVER
	*/
	OP_STATUS GarbageCollectMarkedContainers();
public:
	// Customisations made for containers
	virtual BOOL BypassStorageSave(Cache_Storage *storage, const OpStringC &filename, OP_STATUS &ops);
	virtual BOOL BypassStorageRetrieveData(Cache_Storage *storage, URL_DataDescriptor *desc, BOOL &more, unsigned long &ret, OP_STATUS &ops);
	virtual BOOL BypassStorageFlush(File_Storage *storage);
	virtual BOOL BypassStorageTruncateAndReset(File_Storage *storage);

	/// Enable or disable containers; this has an impact only on new URLs
	void SetEnableContainers(BOOL b) { enable_containers=b; }
	/// Return if the containers are enabled
	BOOL GetEnableContainers() { return enable_containers; }
#ifdef DISK_CACHE_SUPPORT
	virtual int	CheckDCacheSize(BOOL all=FALSE);
#endif //DISK_CACHE_SUPPORT

	#ifdef SELFTEST
		virtual BOOL CheckCacheStorage(Cache_Storage *cs);
		virtual BOOL CheckInvariants();
	#endif // SELFTEST
#endif // CACHE_CONTAINERS_ENTRIES>0

	/////////////// Public interface - MANDATORY methods /////////////////////////////////
	virtual int	CheckCacheSize(URL_DataStorage *start_item, URL_DataStorage *end_item,
			BOOL &flush_flag, time_t &last_flush, int period,
			CacheLimit *cache_limit, OpFileLength size_limit, OpFileLength force_size,
			BOOL check_persistent
		#if defined __OEM_EXTENDED_CACHE_MANAGEMENT
			, BOOL flush_never_flush_too = FALSE
		#endif // __OEM_EXTENDED_CACHE_MANAGEMENT
		);
	virtual void WriteCacheIndexesL(BOOL sort_and_limit, BOOL destroy_urls)
#ifdef DISK_CACHE_SUPPORT
		;  // Defined in cache_ctxman_disk.cpp
#else
	{ } // Do nothing for RAM
#endif

#ifdef DISK_CACHE_SUPPORT
	virtual OP_STATUS DeleteCacheIndexes();
#endif
};


#endif //CACHE_CTX_MAN_DISK_H
