/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "modules/cache/simple_stream.h"
#include "modules/cache/cache_utils.h"





#ifndef CACHE_CONTAINER_H
#define CACHE_CONTAINER_H

#if CACHE_CONTAINERS_ENTRIES>0

/// Sign used to identify the container (just to allow new containers in the future)
#define CACHE_CONTAINERS_BYTE_SIGN 90


/** Single file entry in a container file*/
class ContainerEntry
{
private:
	friend class CacheContainer;
	UINT8 entry_id;		// File ID, unique in the same container; 0 means deleted
	UINT16 size;	// Being tought for small files, 64KB is enough
	unsigned char *ext_pointer; // Pointer supplied to the container
	
	/**
	  Set the new pointer;
	  @param ptr Pointer. The ownership is taken by the Cache Container
	  @param new_size Size
	*/
	void SetPointer(const unsigned char *ptr, UINT16 new_size)
	{
		OP_DELETEA(ext_pointer);
		ext_pointer=(unsigned char *)ptr;
		size=new_size;
		CheckInvariants();
	}
	
	/**
	  Set the whole record;
	  @param new_id ID of the entry
	  @param ptr Pointer. The ownership is taken by the Cache Container
	  @param new_size Size
	*/
	void SetRecord(UINT8 new_id, const unsigned char *ptr, UINT16 new_size)
	{
		entry_id=new_id;
		SetPointer(ptr, new_size);
	}
	
	ContainerEntry(): entry_id(0), size(0), ext_pointer(NULL) { }
	
	// Invariants check via assertions
	void CheckInvariants() { OP_ASSERT(entry_id>0 || (entry_id==0 && size==0 && ext_pointer==NULL)); }
	// This function is call during the header loading, to set the ID and the size of the entry
	//void TemporarySetIDAndSize(UINT8 new_id, UINT16 new_size) { OP_ASSERT(!entry_id && !size && !pointer && !owned); entry_id=new_id; size=new_size; }
	
public:
	/** Reset the entry. The pointer will be deleted
	*/
	void Reset()
	{
		OP_DELETEA(ext_pointer);
		ext_pointer=NULL;
			
		entry_id=0;
		size=0;
	}
	
	// Return the size
	UINT16 GetSize() { return size; }
	UINT8 GetID() { return entry_id; }
	const unsigned char *GetPointer() { return ext_pointer; }
	~ContainerEntry() { Reset(); }
};

/**
	Interface to store several small files in a single bigger file.
	The container does not save the file name, because the cache already store it.
	Note that the container support delete, but the cache is not supposed to support it.
	The ID (unique during the time, in the same container) is used because in this way an entry can be
	deleted without causing major problems to an index even if it is not synchronized.
	This means that reached the ID 255, no more entries can be stored, even if the container is empty
	
	File format:
		- 1 byte: CACHE_CONTAINERS_BYTE_SIGN  (sign)
		- 1 byte: Number of entries
		*** For each entry:
			- 1 byte: ID
			- 2 bytes: size
		- Data: each data entry
*/
class CacheContainer
{
private:
	/// Each entry represent a single file
	ContainerEntry entries[CACHE_CONTAINERS_ENTRIES];
	/// Number of entries allocated
	UINT32 num_entries;
	/// Next ID that can be used
	UINT32 next_id;
	/// Total size allocated
	UINT32 total_size;
	/// Domain of the entries (unique pointer, like ServerName)
	void *domain_pointer;
	/// Name of the file that has to store the data
	OpString file_name;
	/// TRUE if the file has been modified; after a Reset(), it is not modified...
	BOOL modified;
	/// Folder where the container file is put
	OpFileFolder folder;
	
	// Invariants check via assertions
	inline void CheckInvariants();
	
	/**
		Retrieve the entry index using the ID
		@param id ID of the resurce
		@return -1 if the entry was not found, the index itself if it was
	*/
	int GetEntryIndexByID(UINT8 id);
	
public:
	CacheContainer(): num_entries(0), next_id(1), total_size(0), domain_pointer(NULL), modified(FALSE), folder(OPFILE_CACHE_FOLDER) { CheckInvariants(); } 
	~CacheContainer() { Reset(); }

	/**
		Add an entry to the container.
		@param ptr Pointer to the resource itself (usually an URL or a buffer)
		@param size Size of the data to store
		@param id ID of the resurce
		@param take_ownership TRUE if the class has to manage the life of the pointer
		@param host_name Is the host name of the URL cached (URL of different hosts are refused)
			   it could make sense also to use the host of the Tab  (so that ADs and statistics can be mixed)
		@return TRUE if it was possible to add the file to the container, FALSE if it is full
			   When there are no entries, the first one will set the domain
	*/
	BOOL AddEntry(UINT32 size, UINT8 &id, void *host_pointer);
	/**
		Remove the entry using the ID
		@param id ID of the resurce
		@return TRUE if the entry was found
	*/
	BOOL RemoveEntryByID(UINT8 id);
	/**
		Update the pointer using the ID (in any case, the preferred way is to delete the old ID, and create a new one)
		@param id ID of the resurce
		@param new_ptr New pointer
		@param new_size New size
		@param take_ownership TRUE if the class has to manage the life of the pointer
		@return TRUE if the entry was found and updated (it could fail because of a file too big)
	*/
	BOOL UpdatePointerByID(UINT8 id, const unsigned char *new_ptr, UINT16 new_size);
	/**
		Retrieve an entry using the ID
		@param id ID of the resurce
		@param ptr Pointer to the resource itself (usually an URL or a buffer)
		@param size Size of the data to store
		@return OpStatus::OK if it was possible to add the file to the container
	*/
	BOOL GetEntryByID(UINT8 id, const unsigned char *&new_ptr, UINT16 &size);
	/// Read all the data from the stream
	OP_STATUS ReadFromStream(SimpleStreamReader *reader);
	/// Wrtie all the data to the stream
	OP_STATUS WriteToStream(SimpleStreamWriter *writer);
	/// Return the numer of entries (mainly for testing)
	int GetNumEntries() { return num_entries; }
	/// Return the total size of the data, without the header (mainly for testing)
	int GetSize() { return total_size; }
	/** Reset the container. Pointers owned will be deleted
	*/
	void Reset();
	/// Set the file name (this is for the cache, the container does not really use it)
	OP_STATUS SetFileName(OpFileFolder cont_folder, const uni_char *name) { OP_ASSERT(name); if(!name) return OpStatus::ERR_NULL_POINTER; folder=cont_folder; return file_name.Set(name); }
	/// Return the folder
	OpFileFolder GetFolder() { return folder; }
	/// Get the file name
	OpStringC16 *GetFileName() { return &file_name; }
	/// TRUE if the container has been modified (== un update, an add or a remove has been done)
	BOOL IsModified() { return modified; }
};

void CacheContainer::CheckInvariants()
{
	#ifdef _DEBUG
		UINT32 counted_entries=0;
		OP_ASSERT(next_id>0 && next_id<=256);
		OP_ASSERT(num_entries<=CACHE_CONTAINERS_ENTRIES && num_entries<256);
		UINT32 counted_size=0;
		
		for(int i=0; i<CACHE_CONTAINERS_ENTRIES; i++)
		{
			entries[i].CheckInvariants();
			
			if(entries[i].GetSize()>0)
				OP_ASSERT(entries[i].GetID()>0);
			if(entries[i].GetID()>0)
				counted_entries++;
			else
			{
				OP_ASSERT(entries[i].GetSize()==0 && entries[i].GetPointer()==NULL);
			}
			counted_size+=entries[i].GetSize();
			
			for(int j=i+1; j<CACHE_CONTAINERS_ENTRIES; j++)
			{
				if(entries[i].GetID())
					OP_ASSERT(entries[i].GetID()!=entries[j].GetID());
			}
		}
		
		OP_ASSERT(counted_entries==num_entries);
		OP_ASSERT(counted_size<=CACHE_CONTAINERS_CONTAINER_LIMIT);
		OP_ASSERT(counted_size==total_size);
	#endif
}



#endif // CACHE_CONTAINERS_ENTRIES
#endif // CACHE_CONTAINER_H
