/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Luca Venturi
**
*/

#include "core/pch.h"

#include "modules/url/url_man.h"
#include "modules/cache/cache_ctxman_disk.h"
#include "modules/cache/url_cs.h"
#include "modules/cache/cache_debug.h"
#include "modules/cache/cache_common.h"
#include "modules/cache/cache_propagate.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_stor.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/stdlib/util/opdate.h"

#ifdef APPLICATION_CACHE_SUPPORT
#include "modules/applicationcache/application_cache_manager.h"
#endif // APPLICATION_CACHE_SUPPORT


Context_Manager_Disk::~Context_Manager_Disk()
{
#ifdef DISK_CACHE_SUPPORT
	urlManager->DeleteFilesInDeleteListContext(context_id);

#if CACHE_CONTAINERS_ENTRIES>0
	if(cache_written_before_shutdown)
		GarbageCollectMarkedContainers();
#endif // CACHE_CONTAINERS_ENTRIES>0

	if(!cache_written_before_shutdown)
	{
		TRAPD(op_err, WriteCacheIndexesL(TRUE, TRUE));
		OpStatus::Ignore(op_err);
		cache_written_before_shutdown = TRUE;

		// This is a precaution, as all the files should have already been deleted with the first call
		urlManager->DeleteFilesInDeleteListContext(context_id);
	}
#endif //DISK_CACHE_SUPPORT

#if CACHE_CONTAINERS_ENTRIES>0
	// This list is supposed to be cleaned before the manager is destroyed, or the index can be in an inconsistent state
	OP_ASSERT(cnt_marked.GetCount()==0);

	for(int cnt=0; cnt<CACHE_CONTAINERS_BUFFERS; cnt++)
	{
		OP_DELETE(cnt_buffers[cnt]);
		cnt_buffers[cnt]=0;
	}
#endif

}

Context_Manager_Disk::Context_Manager_Disk(URL_CONTEXT_ID a_id
#if defined DISK_CACHE_SUPPORT
			, OpFileFolder a_vlink_loc
			, OpFileFolder a_cache_loc
				): Context_Manager(a_id, a_vlink_loc, a_cache_loc)
#else
				): Context_Manager(a_id)
#endif // DISK_CACHE_SUPPORT

{
#if CACHE_CONTAINERS_ENTRIES>0
	for(int cnt=0; cnt<CACHE_CONTAINERS_BUFFERS; cnt++)
		cnt_buffers[cnt]=NULL;
#endif
}

void Context_Manager_Disk::InitL()
{
	Context_Manager::InitL();

#if CACHE_CONTAINERS_ENTRIES>0
	for(int cnt=0; cnt<CACHE_CONTAINERS_BUFFERS; cnt++)
	{
		LEAVE_IF_NULL(cnt_buffers[cnt]=OP_NEW(CacheContainer, ()));
	}

	enable_containers = TRUE;
#endif
}

#if CACHE_CONTAINERS_ENTRIES>0
OP_STATUS Context_Manager_Disk::RetrieveCacheContainer(Cache_Storage *storage, CacheContainer *&cont)
{
	OP_ASSERT(context_id==storage->Url()->GetContextId());
	
	cont=NULL;
	
	OP_ASSERT(storage && storage->GetContainerID());
	
	if(!storage->GetContainerID())
		return OpStatus::ERR_OUT_OF_RANGE;
		
	int i;
	OpFileFolder folder;
	OpStringC filename=storage->FileName(folder);
	
	OP_ASSERT(!filename.IsEmpty());
	
	if(filename.IsEmpty())
		return OpStatus::ERR_OUT_OF_RANGE;
	
	int available_cont=-1;
	
	// Look in the current buffers, to see if the file is already available
	// FIXME: read buffers different than write ones... well, both of them has to be used...
	for(i=0; !cont && i<CACHE_CONTAINERS_BUFFERS; i++)
	{
		if(cnt_buffers[i]->GetFileName() && !cnt_buffers[i]->GetFileName()->Compare(filename.CStr()))
			cont=cnt_buffers[i];
		else if(available_cont<0 && !cnt_buffers[i]->GetNumEntries())
			available_cont=i;
	}
	
	// TODO: check also write buffers
	
	// Create a new one
	if(!cont)
	{
		SimpleFileReader reader(GetStreamBuffer());
		CacheContainer *temp;
		
		if(available_cont<0)   // No buffers available
		{
			// Flush and reset the first one
			RETURN_IF_ERROR(FlushContainer(cnt_buffers[0], TRUE));
				
			temp=cnt_buffers[0];
			
			// Rotate the buffers and put this one at the end
			for(i=1; i<CACHE_CONTAINERS_BUFFERS; i++)
				cnt_buffers[i-1]=cnt_buffers[i];
			
			cnt_buffers[CACHE_CONTAINERS_BUFFERS-1]=temp;
		}
		else
		{
			temp=cnt_buffers[available_cont];
			temp->Reset();
		}
		
		RETURN_IF_ERROR(reader.Construct(filename.CStr(), folder));
		
		OP_STATUS ops=temp->ReadFromStream(&reader);
		
		reader.Close();
		
		RETURN_IF_ERROR(ops);
		
		cont=temp;
		cont->SetFileName(folder, filename.CStr());
	}
	
	if(!cont)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
		
	return OpStatus::OK;
}

unsigned char * Context_Manager_Disk::GetStreamBuffer()
{
	if(g_memory_manager->GetTempBuf2kLen()>=STREAM_BUF_SIZE)
		return (unsigned char *)g_memory_manager->GetTempBuf2k();
	
	return NULL;
}

OP_STATUS Context_Manager_Disk::RetrieveCacheItemFromContainerAndStoreIt(Cache_Storage* storage, BOOL &found, URL_DataDescriptor *desc)
{
	OP_ASSERT(storage && storage->GetContainerID() && desc);
	
	found=FALSE;
	
	if(!desc)
		return OpStatus::ERR_NULL_POINTER;
		
	CacheContainer *cont=NULL;
	
	RETURN_IF_ERROR(RetrieveCacheContainer(storage, cont));
		
	// Retrieve the file and store it in cache_content
	const unsigned char *ptr;
	UINT16 size;
	
	if(cont->GetEntryByID(storage->GetContainerID(), ptr, size))
	{
		/*OP_STATUS ops=storage->Cache_Storage::StoreData((const unsigned char *)ptr, size);
		
		if(OpStatus::IsSuccess(ops))
		{
			desc->AddContentL(storage->cache_content, 
			found=TRUE;
		}*/
		
		OpFileLength next_position=desc->GetPosition()+desc->GetBufSize();
		
		if(next_position>=size)
		{
			found=TRUE;
			
			return OpStatus::OK;
		}
		
		if (desc->Grow(static_cast<unsigned long>(size - next_position)) == 0)
			return OpStatus::ERR_NO_MEMORY;

		UINT32 len = 0;
		OP_ASSERT(((size - next_position) <= USHRT_MAX) && "CACHE_CONTAINERS_FILE_LIMIT is too big!");
		RETURN_IF_LEAVE(len = desc->AddContentL(NULL, ptr + next_position, static_cast<UINT16>(size - next_position)));

		if (len < (size - next_position))
			return OpStatus::ERR_OUT_OF_RANGE;
		
		/*OP_ASSERT(len==size);
		
		if(len!=size)
			return OpStatus::ERR;*/
			
		found=TRUE;
		
		return OpStatus::OK;
	}
	
	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

OP_STATUS Context_Manager_Disk::RetrieveCacheItemFromContainerAndStoreIt(Cache_Storage *storage, BOOL &found, const OpStringC *file)
{
	OP_ASSERT(storage && storage->GetContainerID() && file);
	
	found=FALSE;
	
	if(!file)
		return OpStatus::ERR_NULL_POINTER;
		
	CacheContainer *cont=NULL;
	
	RETURN_IF_ERROR(RetrieveCacheContainer(storage, cont));
		
	// Retrieve the file and store it in cache_content
	const unsigned char *ptr;
	UINT16 size;
	
	if(cont->GetEntryByID(storage->GetContainerID(), ptr, size))
	{
		OpFile f;
		
		RETURN_IF_ERROR(f.Construct(file->CStr()));
		RETURN_IF_ERROR(f.Open(OPFILE_WRITE));
		RETURN_IF_ERROR(f.Write(ptr, size));
		RETURN_IF_ERROR(f.Close());
		
		found=TRUE;
		
		return OpStatus::OK;
	}
	
	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

OP_STATUS Context_Manager_Disk::MarkContainerForDelete(File_Storage *storage)
{
	OP_ASSERT(storage && storage->GetContainerID());
	
	OpFileFolder folder;
	const uni_char *file_name=storage->FileName(folder).CStr();
	if (!file_name)
	{
		OP_ASSERT(file_name);
		return OpStatus::ERR;
	}
	MarkedContainer *ptr=NULL;

	if(OpStatus::IsError(cnt_marked.GetData(file_name, &ptr))) // If the file is already present, no actions required
	{
		OP_NEW_DBG("Context_Manager_Disk::MarkContainerForDelete", "cache");
		OP_DBG((UNI_L("*** Cache File %s marked for delete"), file_name));

		OP_STATUS ops=OpStatus::ERR_NO_MEMORY;
		ptr=OP_NEW(MarkedContainer, ()); // The delete count is not incremented, so the file will be checked against delete later
		
		if(!ptr || OpStatus::IsError(ops=ptr->SetFileName(GetCacheLocationForFilesCorrected(), file_name, &cnt_marked)) || OpStatus::IsError(ops=cnt_marked.Add(ptr->GetFileNameOnly(), ptr)))
		{
			OP_DELETE(ptr);

			return ops;
		}
	}
	
	return OpStatus::OK;
}

OP_STATUS Context_Manager_Disk::AddCacheItemToContainer(File_Storage *storage, BOOL &added)
{
	OP_ASSERT(storage && storage->ContentLoaded()<=CACHE_CONTAINERS_FILE_LIMIT);
	//OP_ASSERT(storage->Url()->GetDataStorage()->GetCacheStorage()); /* Triggered by CORE-28766. Check http://testsuites.oslo.opera.com/core/bts/crashers/visual/CORE-28766/001.html */
	
	added=FALSE;
	
	//OP_ASSERT(!storage->GetContainerID());
	// TODO: check for existence...
	if(storage->GetContainerID()) // It is supposed to have been already saved...
	{
		added=TRUE;
		
		return OpStatus::OK;
	}
	
	storage->SetContainerID(0);
		
	// FIXME: Non persistent storage can be managed, but we need two containers...
	// FIXME: Pages that have a short expiration time, should be stored in separated containers
	// Check for the situation that block the URL from being add to a container
	if(!enable_containers ||			// Containers disabled
		!storage->IsPersistent() ||		// Not persistent: it will be deleted soon
		!storage->Url()->GetDataStorage()|| !storage->Url()->GetDataStorage()->GetCacheStorage() ||  // CORE-28766: There is not a cache storage (a crash will follow soon...)
		(storage->Url() && storage->Url()->Expired(FALSE, FALSE)) ||    // URL expired: to be deleted soon
		storage->Url()->GetAttribute(URL::KUnique) ||    // URL Unique, it is dangerous to put it in a container
		storage->Url()->GetAttribute(URL::KMultimedia))  // Multimedia files have dedicated storage
		return OpStatus::OK;

	#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		 // Files in the operator cache should not be put on containers, as they can be deleted by URLs in the normal cache. Check also DFL-1110
		if(storage->Url()->GetAttribute(URL::KNeverFlush))
			return OpStatus::OK;
	#endif

	OP_ASSERT(!storage->Url()->GetAttribute(URL::KUnique));
		
	UINT32 len=(UINT32)storage->ContentLoaded();
	
	if(len>CACHE_CONTAINERS_FILE_LIMIT)
		return OpStatus::OK;
	
	CacheContainer *cont=NULL;
	UINT8 id=0;
	ServerName *server_name = (ServerName *) storage->Url()->GetAttribute(URL::KServerName, NULL);
	int num_used_buffers=0;
	int i;
	
	// Try to allocate a slot on a used container
	for(i=0; !cont && i<CACHE_CONTAINERS_BUFFERS; i++)
	{
		OP_ASSERT(cnt_buffers[i]->GetNumEntries()<=CACHE_CONTAINERS_ENTRIES);
		
		if(cnt_buffers[i]->GetNumEntries())
		{
			num_used_buffers++;
		
			if(cnt_buffers[i]->AddEntry(len, id, server_name))
				cont=cnt_buffers[i];
		}
	}
	
	// Try to allocate a slot in an empty container
	for(i=0; !cont && i<CACHE_CONTAINERS_BUFFERS; i++)
	{
		OP_ASSERT(cnt_buffers[i]->GetNumEntries()<=CACHE_CONTAINERS_ENTRIES);
		
		if(!cnt_buffers[i]->GetNumEntries() && 
			cnt_buffers[i]->AddEntry(len, id, server_name))
			cont=cnt_buffers[i];
	}
	
	// FIXME: Entries should be read from the temporary memory cache, sorted and saved, with just one buffer
	
	// If all the buffers are in use, try to free the first one, then add the new one at the end
	if(!cont && num_used_buffers>=CACHE_CONTAINERS_BUFFERS)
	{
		RETURN_IF_ERROR(FlushContainer(cnt_buffers[0], TRUE));
		
		CacheContainer *temp=cnt_buffers[0];
		
		OP_ASSERT(temp->GetNumEntries()==0);
		
		for(int cnt=1; cnt<CACHE_CONTAINERS_BUFFERS; cnt++)
			cnt_buffers[cnt-1]=cnt_buffers[cnt];
			
		cnt_buffers[CACHE_CONTAINERS_BUFFERS-1]=temp;
			
		// Try again
		if(cnt_buffers[CACHE_CONTAINERS_BUFFERS-1]->AddEntry(len, id, server_name))
			cont=cnt_buffers[CACHE_CONTAINERS_BUFFERS-1];
	}
	
	if(cont)
	{
		OP_STATUS ops;
		
		if(OpStatus::IsSuccess(ops=storage->StoreInContainer(cont, id)))
		{
			OP_ASSERT((cont->GetNumEntries()==1 && cont->GetFileName()->IsEmpty()) ||
					  (cont->GetNumEntries()>1 && !cont->GetFileName()->IsEmpty()));
					  
			// Get the name to use for the container
			if(cont->GetNumEntries()==1)
			{
				OpString container_name;
				
				OpFileFolder folder=GetCacheLocationForFilesCorrected();
				
				OP_STATUS op_err = urlManager->GetNewFileNameCopy(container_name, UNI_L("tmp") //(ext.HasContent() ? ext.CStr() : UNI_L("tmp"))
				, folder
				, FALSE
		#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
				, !!storage->Url()->GetAttribute(URL::KNeverFlush)
		#endif
				, storage->Url()->GetContextId());
					
				RETURN_IF_ERROR(op_err);
				
				RETURN_IF_ERROR(cont->SetFileName(folder, container_name.CStr()));
			}
			
			OP_ASSERT(!cont->GetFileName()->IsEmpty());
			
			added=TRUE;
			RETURN_IF_ERROR(storage->SetFileName(cont->GetFileName()->CStr(), cont->GetFolder()));
			storage->SetContainerID(id);
			storage->SetFinished(TRUE);
			
			OP_NEW_DBG("Context_Manager_Disk::AddCacheItemToContainer", "cache");
			OP_DBG((UNI_L("Container %s add file %s\n"), cont->GetFileName()->CStr(), storage->Url()->GetAttribute(URL::KUniName).CStr()?storage->Url()->GetAttribute(URL::KUniName).CStr():UNI_L("")));
			
			// If the buffer is now full, write it on disk and clear it
			if(cont &&cont->GetNumEntries()>=CACHE_CONTAINERS_ENTRIES)
			{
				RETURN_IF_ERROR(FlushContainer(cont, TRUE));
			}
		}
		else
		{
			cont->RemoveEntryByID(id);
			return ops;
		}
	}
		
	return OpStatus::OK;
}

OP_STATUS Context_Manager_Disk::TraceMarkedContainers(ListIteratorDelete<URL_DataStorage> *iter_ds, BOOL full, const uni_char *descr)
{
	OP_ASSERT(iter_ds);

	if(!iter_ds)
		return OpStatus::ERR_NULL_POINTER;

	if(!cnt_marked.GetCount())
		return OpStatus::OK;

	// "Garbage collection" is performed only if there are at least 3 new marked containers, unless a full GC is requested
	if(!full && cnt_marked.GetCount()<3)
		return OpStatus::OK;

	OP_NEW_DBG("Context_Manager_Disk::TraceMarkedContainers()", "cache.del");
	OP_DBG((UNI_L("Start containers Trace() for %d files - cause: %s \n"), cnt_marked.GetCount(), descr));

	// Reset the status of the marked containers, calling ResetInUse()
	GCPhaseReset gc_reset(this);

	cnt_marked.ForEach(&gc_reset);

	// Trace and Delete: Go through all the URLs, and check which containers are in use;
	// For now:
	// - URLs not in use that are in a marked container (in use or not), are unloaded and deleted
	// - URLs in use set the container as in use, preventing it from being deleted
	URL_DataStorage *ds;

	while((ds=iter_ds->Next()) != NULL)
	{
		// If it is in a container, check if it has been deleted, and remove from the index
		Cache_Storage *cs=(ds) ? ds->GetCacheStorage() : NULL;

	#ifdef SELFTEST
		if(cs) cs->checking_container=TRUE;
	#endif

		if(cs && cs->GetContainerID())
		{
			OpFileFolder folder;
			OpStringC name=cs->FileName(folder);

			MarkedContainer *ptr;

			if(name.HasContent() && OpStatus::IsSuccess(cnt_marked.GetData(name.CStr(), &ptr)) && ptr) // Container already deleted
			{
				OP_ASSERT(ptr->GetFolder()==folder);
				OP_ASSERT(ptr->GetFolder()==GetCacheLocationForFiles());

				if(ds->Rep()->GetUsedCount())
					ptr->IncUsed();		// URLs in use prevent the container from being deleted
				else
				{
					iter_ds->DeleteLast();	// URLs not in use are deleted
					cs = NULL;
				}
			}
		}

	#ifdef SELFTEST
		if(cs) cs->checking_container=FALSE;
	#endif
	}

	return OpStatus::OK;
}

void Context_Manager_Disk::GCPhaseDeleteFiles::HandleContainer(const uni_char *file_name, MarkedContainer *cnt)
{
	if(!cnt->GetUsed()) // The element can be deleted
	{
		int index;
		BOOL modified;

		// Reset the container if it is in RAM
		if(mng->IsContainerInRam(cnt->GetFileNameOnly(), &index, &modified))
			mng->cnt_buffers[index]->Reset();

		cnt->AutoDelete();
	}
}

OP_STATUS Context_Manager_Disk::GarbageCollectMarkedContainers()
{
	GCPhaseDeleteFiles gc_delete(this);

	cnt_marked.ForEach(&gc_delete);

	cnt_marked.DeleteAll();

	CheckInvariants();

	return OpStatus::OK;
}

#ifdef DISK_CACHE_SUPPORT
int Context_Manager_Disk::CheckDCacheSize(BOOL all)
{
	int n=Context_Manager::CheckDCacheSize(all);

	URL_DataStorageIterator iter(this, (URL_DataStorage *)LRU_list.First());

	TraceMarkedContainers(&iter, TRUE, UNI_L("CheckDCacheSize"));
	GarbageCollectMarkedContainers();

	return n;
}
#endif // DISK_CACHE_SUPPORT

#ifdef SELFTEST
BOOL Context_Manager_Disk::CheckInvariants()
{
	// Used to simplify the debugging, as it prevents Context_Manager_Disk::CheckCacheSize() from refusing to
	// resize the cache 
	last_disk_cache_flush=0;

	if(!check_invariants)
		return TRUE;

#ifdef DEBUGGING
DEBUG_CODE_BEGIN("cache.dbg")
	URL_Rep *url_rep ;

	url_rep = (url_store)?url_store->GetFirstURL_Rep():NULL;

	// Populate the dcache_list
	while(url_rep)
	{
		URL_DataStorage *storage=url_rep->GetDataStorage();
		if(storage && storage->GetCacheStorage())
		{
			OP_ASSERT(storage->GetCacheStorage()->checking_container==FALSE);
			// A storage should either be in a container or embedded, not both
			OP_ASSERT(! (storage->GetCacheStorage()->container_id && storage->GetCacheStorage()->embedded));
			
			storage->GetCacheStorage()->checking_container=FALSE;
			
			if(!storage->GetCacheStorage()->CheckContainer())
				return FALSE;
				
			OP_ASSERT(storage->GetCacheStorage()->checking_container==FALSE);
		}
		
		url_rep = url_store->GetNextURL_Rep();
	}
DEBUG_CODE_END
#endif

	return TRUE;
}

BOOL Context_Manager_Disk::CheckCacheStorage(Cache_Storage *cs)
{
	if(!cs->container_id)
		return TRUE;

	// A storage should either be in a container or embedded, not both
	OP_ASSERT(!cs->embedded);

	OpStringC path;
	OpFileFolder folder;
	BOOL ok1;
	BOOL ok2;
	
	path=cs->FileName(folder);

	ok1=path.HasContent();

	if(IsDebugDeepEnabled())
		ok2=IsContainerPresent(cs);
	else
		ok2=TRUE;
	
	if(!ok1 || !ok2)
	{
		OpString full_path;

		g_folder_manager->GetFolderPath(folder, full_path);

		OP_NEW_DBG("Context_Manager_Disk::CheckCacheStorage", "cache.dbg");
		OP_DBG((UNI_L("!! Container %s not present for URL %s n folder %d: %s!!\n"), path.CStr(), cs->Url()->GetAttribute(URL::KUniName).CStr(), folder, full_path.CStr()));
	}

	OP_ASSERT(ok1);
	OP_ASSERT(ok2);

	return ok1 && ok2;
}
#endif // SELFTEST

BOOL Context_Manager_Disk::IsContainerPresent(Cache_Storage *storage)
{
	OP_ASSERT(storage && storage->GetContainerID());
	
	OpFileFolder folder1 = OPFILE_ABSOLUTE_FOLDER;
	OpStringC file_name=storage->FileName(folder1); //storage->Url()->GetAttribute(URL::KFileName);
	OP_ASSERT(file_name.HasContent());
	
	// Check containers in RAM
	for(int i=0; i<CACHE_CONTAINERS_BUFFERS; i++)
	{
		if(cnt_buffers[i]->GetFileName() && !file_name.Compare(cnt_buffers[i]->GetFileName()->CStr()))
			return TRUE;
	}
	
	// Ok, the feature should be disabled if DISK_CACHE_SUPPORT is not enabled
#ifdef DISK_CACHE_SUPPORT
	// Check on disk
	OpFile f;
	
	if(OpStatus::IsSuccess(f.Construct(file_name.CStr(), cache_loc)))
	{
		BOOL b;
		
		if(OpStatus::IsError(f.Exists(b)))
			return FALSE;
		
		if(!b)
		{
			OP_NEW_DBG("Context_Manager_Disk::IsContainerPresent", "cache");
			OP_DBG((UNI_L("!! Container %s not present !!\n"), file_name.CStr()));
			
			// Add the file to the list of containers known to have been deleted
			MarkedContainer *cont_to_add=OP_NEW(MarkedContainer, ());
					
			if(!cont_to_add || OpStatus::IsError(cont_to_add->SetFileName(GetCacheLocationForFilesCorrected(), file_name.CStr(), &cnt_marked)) || OpStatus::IsError(cnt_marked.Add(cont_to_add->GetFileNameOnly(), cont_to_add)))
				OP_DELETE(cont_to_add);
			else
				cont_to_add->SetDeleted(TRUE); // The file has already been deleted somewhere...
		}
		
		return b;
	}
#endif // DISK_CACHE_SUPPORT
	
		
	return FALSE;
}

/// Check if the container is present in RAM. If modified!=NULL, it will store the modified status of the container
/// PROPAGATE: NEVER
BOOL Context_Manager_Disk::IsContainerInRam(const uni_char *file_name, int *index, BOOL *modified)
{
	// Check containers in RAM
	for(int i=0; i<CACHE_CONTAINERS_BUFFERS; i++)
	{
		if(cnt_buffers[i]->GetFileName() &&	!cnt_buffers[i]->GetFileName()->Compare(file_name))
		{
			if(modified)
				*modified=cnt_buffers[i]->IsModified();
			if(index)
				*index=i;

			return TRUE;
		}
	}

	return FALSE;
}

OP_STATUS Context_Manager_Disk::FlushContainer(CacheContainer *cont, BOOL reset)
{
	OP_NEW_DBG("Context_Manager_Disk::FlushContainer", "cache");
		
	// The name needs to be already allocated
	OP_ASSERT(cont && cont->GetFileName() && (!cont->GetNumEntries() || !cont->GetFileName()->IsEmpty()));
	
	if(!cont || !cont->GetFileName())
		return OpStatus::ERR_NULL_POINTER;
		
	if(!cont->GetNumEntries()) // It is supposed to already have been reset...
		return OpStatus::OK;		
		
	if(cont->GetFileName()->IsEmpty())
		return OpStatus::ERR_OUT_OF_RANGE;

	if(cont->IsModified())
	{
		SimpleFileWriter file_stream(GetStreamBuffer());
		
		RETURN_IF_ERROR(file_stream.Construct(cont->GetFileName()->CStr(), GetCacheLocationForFilesCorrected(), FALSE)); 
#ifdef SELFTEST
		if (urlManager->emulateOutOfDisk != 0)
		{
			if(reset)
				cont->Reset();
			return OpStatus::ERR_NO_DISK;
		}
#endif //SELFTEST

		// If the container cannot be written (usually because of an Out Of Disk Error), delete the file to avoid problems
		OP_STATUS ops=cont->WriteToStream(&file_stream);

		if(OpStatus::IsError(ops))
		{
			OpFile file_to_del;

			file_stream.Close(FALSE);

			if(OpStatus::IsSuccess(file_to_del.Construct(cont->GetFileName()->CStr(), GetCacheLocationForFilesCorrected())))
				OpStatus::Ignore(file_to_del.Delete(FALSE));

			return ops;
		}

		RETURN_IF_ERROR(file_stream.Close());
		
		OP_DBG((UNI_L("Container %s flushing with %d entries...\n"), cont->GetFileName() && cont->GetFileName()->CStr()?cont->GetFileName()->CStr():UNI_L(" ??? "), cont->GetNumEntries()));
	}
	
	if(reset)
		cont->Reset();
	
	return OpStatus::OK;
}

OP_STATUS Context_Manager_Disk::FlushContainers(BOOL reset)
{
	OP_STATUS ops=OpStatus::OK;
	
	#ifdef SELFTEST
		OpString str;
	#endif

	// Current manager
	for(int i=0; i<CACHE_CONTAINERS_BUFFERS; i++)
	{
		if(cnt_buffers[i])
		{
		#ifdef SELFTEST
			if(cnt_buffers[i]->GetNumEntries())
				str.AppendFormat(UNI_L("Slot %d: %s, %d entries\n"), i, cnt_buffers[i]->GetFileName()->CStr(), cnt_buffers[i]->GetNumEntries());
		#endif
			// FIXME: containers should not be reset after flushing, but of course this has to be verified for regressions
			OP_STATUS temp=FlushContainer(cnt_buffers[i], reset);

			if(!OpStatus::IsSuccess(temp) && OpStatus::IsSuccess(ops))
				ops=temp;
		}
	}

#ifdef SELFTEST
	if(!str.IsEmpty())
	{
		OP_NEW_DBG("Context_Manager_Disk::FlushContainers()", "cache");
		OP_DBG((str.CStr()));
	}
#endif
		
	return ops;
}

BOOL Context_Manager_Disk::BypassStorageSave(Cache_Storage *storage, const OpStringC &filename, OP_STATUS &ops)
{
	OP_ASSERT(storage);

	if(storage->container_id)
	{
		BOOL found;
		
		ops=RetrieveCacheItemFromContainerAndStoreIt(storage, found, &filename);

		return TRUE;
	}

	return FALSE;
}

BOOL Context_Manager_Disk::BypassStorageFlush(File_Storage *storage)
{
	BOOL added=FALSE;
		
	// If possible, add it to a container, then continue
	if(!storage->plain_file && storage->cache_content.GetLength()<=CACHE_CONTAINERS_FILE_LIMIT &&
			(
					storage->cachetype==URL_CACHE_DISK
				#ifdef SELFTEST
					|| embed_all_types  /* Used to let HTTPS join containers */
				#endif
			))
		OpStatus::Ignore(AddCacheItemToContainer(storage, added));

#ifdef SELFTEST
	BOOL b=(added && storage->container_id && storage->filename.HasContent()) || (!added && !storage->container_id);

	if(!b)
	{
		OP_NEW_DBG("Context_Manager_Disk::BypassStorageFlush", "cache");
		OP_DBG((UNI_L("*** CRITICAL ERRORL: Added: %d - container_id: %d - File Name: %s"), added, storage->container_id, storage->filename.CStr()));
	}

	OP_ASSERT(b);
#endif
		
	return added;
}

BOOL Context_Manager_Disk::BypassStorageRetrieveData(Cache_Storage *storage, URL_DataDescriptor *desc, BOOL &more, unsigned long &ret, OP_STATUS &ops)
{
	OP_ASSERT(storage);

	// Load the content from the container
	if(storage->container_id)
	{
		BOOL found=FALSE;

		ops=RetrieveCacheItemFromContainerAndStoreIt(storage, found, desc);
		
		// This is a very bad moment to notice this problem...
		// It means that the cache container has been deleted even if some URLs is still using it. It should not happen under any circumstance,
		// so in a way or in another, it's a bug.
		// To investigate the problem, please enable "cache.dbg" in "debug.txt". If properly done, this will enable more (and slow) debugging code
		// in the cache, which should help understand when this is happening.
		// To understand if the debugging code is running, put a breakpoint in Context_Manager_Disk::CheckInvariants(), in the
		// section that checks "cache.dbg".
		// Please also verify that all the cache selftests can pass without triggering an assert, when "cache'dbg" is enabled.
		// Some of them can disable the assertions, because they create on purpose a wrong situation (for example to simuate a restart)
		//
		// In case of OOM, this assertion should not trigger
		OP_ASSERT((found || OpStatus::IsMemoryError(ops)) && "Cache Container missing");
			
		if(found)
		{
			if(!desc->PostedMessage())
				desc->PostMessage(MSG_URL_DATA_LOADED, storage->Url()->GetID(), 0);
				
			ret = desc->GetBufSize();
			ops = OpStatus::OK;
		}
		else
		{
		#ifdef SELFTEST
			OpFileFolder folder;
			OpStringC filename=storage->FileName(folder);


			OP_NEW_DBG("Context_Manager_Disk::BypassStorageRetrieveData", "cache");
			OP_DBG((UNI_L("*** Cache Container missing - URL %s is now broken and not present in %s\n"), storage->Url()->GetAttribute(URL::KUniName).CStr() ? storage->Url()->GetAttribute(URL::KUniName).CStr() : UNI_L(""), filename.CStr()));
		#endif
			
			// Container deleted or other problems
			ret = storage->Cache_Storage::RetrieveData(desc, more);
		}

		return TRUE;
	}
	
	return FALSE;
}

BOOL Context_Manager_Disk::BypassStorageTruncateAndReset(File_Storage *storage)
{
	if(!storage->container_id)
		return FALSE;

	MarkContainerForDelete(storage);

	return TRUE;
}

#endif // CACHE_CONTAINERS_ENTRIES>0

int Context_Manager_Disk::CheckCacheSize(URL_DataStorage *start_item, URL_DataStorage *end_item,
								BOOL &flush_flag, time_t &last_flush, int period,
								CacheLimit *cache_limit, OpFileLength size_limit, OpFileLength force_size,
								BOOL check_persistent
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT
								, BOOL flush_never_flush_too
#endif
								)
{
	CheckInvariants();

	if(flush_flag)
		return 0;
	if(g_main_message_handler &&
		(long) force_size < 0 &&
		last_flush  && last_flush + period> g_timecache->CurrentTime())
	{
		return (int) (period - (g_timecache->CurrentTime() - last_flush))*1000;
	}
	last_flush = g_timecache->CurrentTime();
	flush_flag = TRUE;
	int wait_period =0;

#ifdef DEBUG_CACHE
	PrintfTofile("cache.txt", "Starting cache flush - Limit:%d - Used: %d - Force: %d - Persist: %d - Time: %d\n", (UINT32)cache_limit->GetUsed(), (UINT32)size_limit, (UINT32)force_size, (UINT32)check_persistent, (UINT32)(g_op_time_info->GetTimeUTC()/1000));
#endif

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && !defined __OEM_OPERATOR_CACHE_MANAGEMENT
	time_t never_flush_expiration_date = g_timecache->CurrentTime() - g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NeverFlushExpirationTimeDays)*24*60*60;
#endif

//#if CACHE_CONTAINERS_ENTRIES>0
//	int initial_count=cnt_marked.GetCount();
//#endif

	if (cache_limit->GetUsed() > size_limit
#if defined STRICT_CACHE_LIMIT2
		*0.80
#endif
		)
	{
		// remove any URLs not belonging in cache - or url with lowest access time
		//time_t oldest_time = LONG_MAX;
		URL_DataStorage* next_url_ds = start_item;
		URL_DataStorage* cur_url_ds;
		URL_Rep* url_rep;
		
		time_t treshold_time = (time_t) (g_op_time_info->GetTimeUTC()/1000.0 - URL_CACHE_FLUSH_THRESHOLD_LIMIT*60);
		if ((int) force_size >= 0)
			treshold_time =  (time_t) (g_op_time_info->GetTimeUTC()/1000.0 + 1);     // nuke anything that is not from the future

		if(next_url_ds == NULL)
		{
			flush_flag = FALSE;
			return 0;
		}

		Head temp_LRU;
		Head temp_LRU1;

		// Move all the URL's to temp_LRU
		while (next_url_ds && next_url_ds != end_item)
		{
			cur_url_ds = next_url_ds;
			next_url_ds = (URL_DataStorage *) next_url_ds->Suc();

			RemoveLRU_Item(cur_url_ds);
			cur_url_ds->Into(&temp_LRU);
		}

		double start_time_wall = g_op_time_info->GetWallClockMS();
		double start_time_process = g_op_time_info->GetRuntimeMS();

		// Unload URLs that qualify for removing
		while ((next_url_ds = (URL_DataStorage *) temp_LRU.First()) != NULL)
		{
			// While the cache should not spend too much time cleaning, we accept a longer pause when the size
			// as above twice the limit
			if(trial_count_cache_flush < 20 && cache_limit->GetUsed() < size_limit*2 && (long) force_size < 0)
			{
				double current_time_wall = g_op_time_info->GetWallClockMS();
				double current_time_process = g_op_time_info->GetRuntimeMS();
				
				if(current_time_process - start_time_process > 500.0 || 
					current_time_wall - start_time_wall > 2000.0)
				{
					wait_period = period*1000;
					break; // don't spend too long, unless too many attempts has been made
				}
			}


			cur_url_ds = next_url_ds;
			url_rep = GetURLRep(next_url_ds);

			cur_url_ds->Out();
			cur_url_ds->Into(&temp_LRU1);
			
			// User files and URL in LOADING are kept on the cache
			if(!url_rep ||
				(URLStatus) url_rep->GetAttribute(URL::KLoadStatus, TRUE) == URL_LOADING ||
				((URLCacheType) url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_USERFILE) )
				continue;
			
#ifdef DEBUG_CACHE
			OpFileLength registered_len=0;
			url_rep->GetAttribute(URL::KContentLoaded, &registered_len);
			if((URLCacheType) url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_DISK ||
				(URLCacheType) url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_TEMP)
				PrintfTofile("cache.txt", "Checking (%u %u %lu/%lu) %s\n",
				url_rep->GetUsedCount(), (url_rep->GetAttribute(URL::KLoadStatus) != URL_LOADING ? 1 : 0),
				(unsigned long) registered_len, (unsigned long) size_disk.GetUsed(), DebugGetURLstring(url_rep));
#endif
			
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
			if(!flush_never_flush_too && url_rep->GetAttribute(URL::KNeverFlush))
				continue; // Do not delete never-flush elements while just cleaning normal cache.
#if !defined __OEM_OPERATOR_CACHE_MANAGEMENT
			if(flush_never_flush_too && url_rep->GetAttribute(URL::KNeverFlush))
			{
				time_t visited = 0;

				url_rep->GetAttribute(URL::KVLocalTimeVisited, &visited);

				if(visited >= never_flush_expiration_date)
					continue;
			}
#else
			if(flush_never_flush_too && (!url_rep->GetAttribute(URL::KNeverFlush) || 
				cache_limit->GetUsed() < (size_limit
#if defined STRICT_CACHE_LIMIT2
				*0.80
#endif
				))) 
			{
				continue;// Don't delete operator cache URLs if the operator cache is not filled up
				// And also don't delete non-operator cache URLs during operator cache check mode
			}
#endif
#endif
				time_t time_loaded = 0;

				url_rep->GetAttribute(URL::KVLocalTimeVisited, &time_loaded);

#ifdef _DEBUG
			//if (debug_free_unused)
			//	treshold_time = time_loaded + 1;
#endif //_DEBUG

			// Used and recently visited URLs (<5 minutes) are kept in cache
			if (url_rep->GetUsedCount() == 0 &&
				time_loaded < treshold_time)
			{
				BOOL removable = FALSE;
				BOOL is_application_cache_url = FALSE;

#ifdef APPLICATION_CACHE_SUPPORT
				is_application_cache_url = !!url_rep->GetAttribute(URL::KIsApplicationCacheURL);
#endif // APPLICATION_CACHE_SUPPORT

				if(check_persistent && !is_application_cache_url && ( // persistent URLs check
					(URLCacheType) url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_DISK ||
					(URLCacheType) url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_TEMP ||
					!url_rep->GetAttribute(URL::KCachePersistent) ||
					((URLType) url_rep->GetAttribute(URL::KType) == URL_HTTPS &&
					!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheHTTPSAfterSessions) &&
					!is_application_cache_url
					)
					) )
					removable = TRUE;
				else
				{
					Cache_Storage *cs=(url_rep->GetDataStorage()) ? url_rep->GetDataStorage()->GetCacheStorage() : NULL;

					if( !check_persistent && !is_application_cache_url && // RAM
						cs && cs->Empty() && cs->GetFinished())
						removable = TRUE;
				}
				
				cur_url_ds->Out();
#ifdef DISK_CACHE_SUPPORT
				if(!check_persistent && removable)
				{
					if(url_rep->DumpSourceToDisk())
					{
						if (cache_limit->GetUsed() < size_limit)
						{
							SetLRU_Item(cur_url_ds);
							break;
						}
						removable = FALSE;
					}
					/*

					else if(!((long) force_size >= 0
# ifdef ALWAYS_FLUSH_NOSTORE_URLS
						|| url_rep->GetAttribute(URL::KCachePolicy_NoStore)
# endif // ALWAYS_FLUSH_NOSTORE_URLS
						))
						removable = FALSE;
					*/
				}
#endif  // DISK_CACHE_SUPPORT
				
				if(removable)
				{
#ifdef APPLICATION_CACHE_SUPPORT
					OP_ASSERT(url_rep->GetAttribute(URL::KIsApplicationCacheURL) == FALSE);
#endif // APPLICATION_CACHE_SUPPORT
					#ifdef DEBUG_CACHE
						if((URLCacheType) url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_DISK ||
							(URLCacheType) url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_TEMP)
							PrintfTofile("cache.txt", "REMOVED (%u %u %lu/%lu) %s\n",
							url_rep->GetUsedCount(), (url_rep->GetAttribute(URL::KLoadStatus) != URL_LOADING ? 1 : 0),
							(unsigned long) registered_len, (unsigned long) size_disk.GetUsed(), DebugGetURLstring(url_rep));
					#endif
					
					url_store->RemoveURL_Rep(url_rep);
					url_rep->SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
					
				#if CACHE_CONTAINERS_ENTRIES>0
					// If it is in a container, mark it for delete
					Cache_Storage *cs=(url_rep && url_rep->GetDataStorage() && url_rep->GetDataStorage()->GetCacheStorage())?url_rep->GetDataStorage()->GetCacheStorage():NULL;

					if(cs && cs->GetContainerID() && cs->IsA(STORAGE_TYPE_FILE))
						OpStatus::Ignore(MarkContainerForDelete((File_Storage *)cs));
				#endif

					// Warning: cache_limit will be updated
					url_rep->Unload();
					if(!url_rep->InList())
					{
						if (!url_rep->GetAttribute(URL::KUnique))
							url_store->AddURL_Rep(url_rep);
						else
						{
							if(url_rep->GetRefCount()<1)
								OP_DELETE(url_rep);
						}
					}

					// Warning: cache_limit->GetUsed() changed because of Unload()
					if (cache_limit->GetUsed() < size_limit
#if defined STRICT_CACHE_LIMIT2
						*0.80
#endif
						)
					{
#if defined DEBUG_CACHE && defined DISK_CACHE_SUPPORT
						PrintfTofile("cache.txt", "Ending cache flush (B) %lu/%lu\n",size_disk.GetUsed(),GetCacheSize());
#endif
						break;
					}
				} // end if removable
				else
					SetLRU_Item(cur_url_ds);
			}
		} // end of while (temp_LRU)

		if(next_url_ds && cache_limit->GetUsed() > size_limit)
			trial_count_cache_flush ++; // another incomplete flush
		else
			trial_count_cache_flush =0;

		temp_LRU1.Append(&temp_LRU);

		//#if CACHE_CONTAINERS_ENTRIES>0
		//	ManageMarkedContainers((URL_DataStorage *)temp_LRU1.First());
		//#endif
	
		next_url_ds = (URL_DataStorage *) temp_LRU1.First();
		while (next_url_ds)
		{
			cur_url_ds = next_url_ds;
			next_url_ds = (URL_DataStorage *) next_url_ds->Suc();

			cur_url_ds->Out();
			SetLRU_Item(cur_url_ds);
		}
	} // end if (limit check)
	else
	{
	//#if CACHE_CONTAINERS_ENTRIES>0
	//	if(initial_count>0)
	//		ManageMarkedContainers(start_item);
	//#endif
	}

#ifdef DEBUG_CACHE
	PrintfTofile("cache.txt", "Ending cache flush (A) %lu/%lu\n",cache_limit->GetUsed(),size_limit);
#endif

	flush_flag = FALSE;

	CheckInvariants();
	
	return wait_period;
}

#ifndef RAMCACHE_ONLY
OpFileLength Context_Manager_Disk::SortURLListByDateAndLimit(URLLinkHead &list, URLLinkHead &rejects, OpFileLength size_limit, BOOL destroy_urls)
{
	URLLinkHead collect_list;
	URLLinkHead sort_list[256];
	URLLink *item;
	unsigned int shift;
	unsigned int i;
	time_t index;

#ifdef APPLICATION_CACHE_SUPPORT
	if (is_used_by_application_cache)
	{
		item = static_cast<URLLink *>(list.First());
		while (item)
		{
			Link *next_item = item->Suc();
			if (item->url.GetAttribute(URL::KIsApplicationCacheURL) == FALSE)
			{
				item->Out();
				item->Into(&collect_list);
			}

			item = static_cast<URLLink *>(next_item);
		}
	}
	else
		collect_list.Append(&list);
#else 
	collect_list.Append(&list);
#endif // APPLICATION_CACHE_SUPPORT

	// First radix sort path / shunt non-cachables into url_storage;
	shift = 0;

	while(shift<(sizeof(index)*8))
	{
		while((item = collect_list.First()) != NULL)
		{
			item->Out();

			index = 0;
			item->url.GetAttribute(URL::KVLocalTimeVisited, &index);
			if(!index)
				item->url.GetAttribute(URL::KVLocalTimeLoaded, &index);

			item->Into(&sort_list[ (index >> shift) & 0xff]);
		}

		for(i=0;i< 256;i++)
		{
			collect_list.Append(&sort_list[i]);
		}

		shift += 8;
	}

	OpFileLength cache_list_size = 0;
	OpFileLength cache_list_limit = size_limit + size_limit/20;// allow 5% extra
	OpFileLength size_of_element;

	// Try to keep as much files as possible, in a suboptimal but faster way
	while(cache_list_size < size_limit &&(item = (URLLink *) collect_list.Last()) != NULL)
	{
		item->Out();

		item->url.GetAttribute(URL::KContentLoaded, &size_of_element);
		if (cache_list_size + size_of_element < cache_list_limit)
		{
			cache_list_size += size_of_element;
			item->Into(&list);
		}
		else
		{
			if(item->url.GetAttribute(URL::KIsFollowed))
				item->Into(&rejects);

			if(destroy_urls)
			{
				#if CACHE_CONTAINERS_ENTRIES>0
					// If it is in a container, add the file to the list of deleted containers
					Cache_Storage *cs=(item->url.GetRep() && item->url.GetRep()->GetDataStorage() && item->url.GetRep()->GetDataStorage()->GetCacheStorage())?item->url.GetRep()->GetDataStorage()->GetCacheStorage():NULL;

					if(cs && cs->GetContainerID())
					{
						OpFileFolder folder;
						OpStringC name=cs->FileName(folder);
						
						OP_ASSERT(name.HasContent());
						MarkedContainer *ptr;

						if(name.HasContent() && OpStatus::IsError(cnt_marked.GetData(name.CStr(), &ptr))) // If the container has not been already added to the list
						{
							MarkedContainer *cont_to_add=OP_NEW(MarkedContainer, ());
						
							if(!cont_to_add || OpStatus::IsError(cont_to_add->SetFileName(GetCacheLocationForFilesCorrected(),name.CStr(), &cnt_marked)) || OpStatus::IsError(cnt_marked.Add(cont_to_add->GetFileNameOnly(), cont_to_add)))
								OP_DELETE(cont_to_add);
						}
					}
				#endif
				
				// Previously the URL was not transformed in temporary, but it should be the right thing to do
				item->url.SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
				
				if(!item->InList())
					DestroyURL(item->url);
				else
					item->url.Unload();
			}
			if(!item->InList())
				OP_DELETE(item);
		}
	}

	// Remove the remaining URLs
	while((item = (URLLink *) collect_list.First()) != NULL)
	{
		item->Out();
		if(item->url.GetAttribute(URL::KIsFollowed))
			item->Into(&rejects);

		if(destroy_urls)
		{
			#if CACHE_CONTAINERS_ENTRIES>0
				// If it is in a container, add the file to the list of deleted containers
				Cache_Storage *cs=(item->url.GetRep() && item->url.GetRep()->GetDataStorage() && item->url.GetRep()->GetDataStorage()->GetCacheStorage())?item->url.GetRep()->GetDataStorage()->GetCacheStorage():NULL;
				UINT8 cnt_id=(cs)?cs->GetContainerID():0;
				
				if(cs && cnt_id)
				{
					OpFileFolder folder;
					OpStringC name=cs->FileName(folder);
					
					OP_ASSERT(name.HasContent());
					

				MarkedContainer *ptr;

					if(name.HasContent() && OpStatus::IsError(cnt_marked.GetData(name.CStr(), &ptr))) // If the container has not been already added to the list
					{
						MarkedContainer *cont_to_add=OP_NEW(MarkedContainer, ());
					
						if(!cont_to_add || OpStatus::IsError(cont_to_add->SetFileName(GetCacheLocationForFilesCorrected(), name.CStr(), &cnt_marked)) || OpStatus::IsError(cnt_marked.Add(cont_to_add->GetFileNameOnly(), cont_to_add)))
							OP_DELETE(cont_to_add);
					}
				}
			#endif
			
			item->url.SetAttribute(URL::KCacheType, URL_CACHE_TEMP);

			if(!item->InList())
				DestroyURL(item->url);
			else
				item->url.Unload();
		}
		if(!item->InList())
			OP_DELETE(item);
	}
	
	//#if CACHE_CONTAINERS_ENTRIES>0
	//	ManageMarkedContainers(list, destroy_urls);
	//#endif

	return cache_list_size;
}

#endif // RAMCACHE_ONLY

#ifdef DISK_CACHE_SUPPORT
OP_STATUS Context_Manager_Disk::DeleteCacheIndexes()
{
	OpFileFolder folder=GetCacheLocationForFiles();

	if(folder==OPFILE_ABSOLUTE_FOLDER || vlink_loc==OPFILE_ABSOLUTE_FOLDER)
		return OpStatus::OK;

	OpFile idx;
	OpFile idx2;
		
	RETURN_IF_ERROR(idx.Construct(DiskCacheFile, folder));
	RETURN_IF_ERROR(idx.Delete(FALSE));

	RETURN_IF_ERROR(idx2.Construct(VlinkFile, vlink_loc));
	RETURN_IF_ERROR(idx2.Delete(FALSE));

	return OpStatus::OK;
}

void Context_Manager_Disk::WriteCacheIndexesL(BOOL sort_and_limit, BOOL destroy_urls)
{
	if(urlManager->IsContextManagerFrozen(this))
		return;

	BOOL disk_cache_enabled = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheToDisk);

	if (!disk_cache_enabled)
		return;

	CACHE_PROPAGATE_ALWAYS_VOID(WriteCacheIndexesL(sort_and_limit, destroy_urls));

#ifdef SEARCH_ENGINE_CACHE
	OP_STATUS err;
#endif

	if (url_store == NULL)
		LEAVE(OpStatus::ERR_NULL_POINTER);

	if (is_writing_cache)
		return;
	
	if(cache_loc == OPFILE_ABSOLUTE_FOLDER || vlink_loc == OPFILE_ABSOLUTE_FOLDER)
		return;

	URLLinkHead dcache_list, vlink_list;
	ANCHOR(URLLinkHead, dcache_list);
	ANCHOR(URLLinkHead, vlink_list);
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT
	URLLinkHead ocache_list;
	ANCHOR(URLLinkHead, ocache_list);
#if !defined __OEM_OPERATOR_CACHE_MANAGEMENT
	time_t never_flush_expiration_date = g_timecache->CurrentTime() - g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NeverFlushExpirationTimeDays)*24*60*60;
#endif
#endif

	URL_Rep *url_rep ;

#if defined(_DEBUG) && defined(SEARCH_ENGINE_CACHE)
	err = url_store->m_disk_index.CheckConsistency();
	OP_ASSERT(err == OpStatus::OK);  // see ConsistencyStatus in CacheIndex.h for errors
#endif

	url_rep = url_store->GetFirstURL_Rep();

	// Populate the lists (above all dcache_list, used to check the size of the disk cache)
	while(url_rep)
	{
		AutoDeleteHead *add_list = NULL;

		URLType type = (URLType) url_rep->GetAttribute(URL::KType);
#if (defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT) || defined SEARCH_ENGINE_CACHE
		time_t visited=0;
		url_rep->GetAttribute(URL::KVLocalTimeVisited, &visited);
#endif

		URL_DataStorage *storage=url_rep->GetDataStorage();
		BOOL embedded=storage && storage->GetCacheStorage() && storage->GetCacheStorage()->IsEmbedded();
		URLStatus status =(URLStatus) url_rep->GetAttribute(URL::KLoadStatus);

		if ( (status == URL_LOADED || (status == URL_LOADING_ABORTED && url_rep->GetAttribute(URL::KMultimedia))) &&
			(url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_DISK || embedded))
		{
			if (url_rep->CheckStorage()	&&
					(url_rep->GetDataStorage()->GetCacheStorage()->GetCacheType() == URL_CACHE_DISK	|| embedded)
					&&	!url_rep->GetDataStorage()->GetCacheStorage()->GetIsMultipart())
			{
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT
				if (context_id == 0 && url_rep->GetAttribute(URL::KNeverFlush))
				{
#if !defined __OEM_OPERATOR_CACHE_MANAGEMENT
					if (visited >= never_flush_expiration_date)
						add_list = &vlink_list;
					else
#endif // __OEM_OPERATOR_CACHE_MANAGEMENT
#ifdef SEARCH_ENGINE_CACHE
					if (!IndexFileL(url_store->m_operator_index, url_rep, destroy_urls, size_oem.GetSize()))
						add_list = &vlink_list;
#else
						add_list = &ocache_list;
#endif
				} else
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT

#ifdef SEARCH_ENGINE_CACHE
				if (!IndexFileL(url_store->m_disk_index, url_rep, destroy_urls, GetDCacheSize()))
					add_list = &vlink_list;
#else
				add_list = &dcache_list;
#endif
			}
		}
		// URLs that do not qualify for the disk cache are still added to the visited links list
		else if(type == URL_HTTP || type == URL_FTP || type == URL_Gopher || type == URL_WAIS ||
			type == URL_FILE || type == URL_NEWS || type == URL_HTTPS || type == URL_SNEWS)
		{
			if (!url_rep->IsFollowed())
			{
				if (url_rep->GetRefCount() == 1 &&
					(URLStatus) url_rep->GetAttribute(URL::KLoadStatus) == URL_UNLOADED )
				{
					goto next_url;
				}
			}
			else
				add_list = &vlink_list;
		}

		{
		#ifdef SELFTEST
			OP_NEW_DBG("Context_Manager_Disk::WriteCacheIndexesL()", "cache.index");

			if(add_list)
				OP_DBG((UNI_L("*** URL %s indexed in context %d"), url_rep->GetAttribute(URL::KUniName).CStr(), context_id));
			else
				OP_DBG((UNI_L("*** URL %s NOT indexed in context %d - emb:%d - status:%d - type:%d - cache_type:%d"), url_rep->GetAttribute(URL::KUniName).CStr(), context_id, embedded, status, type, url_rep->GetAttribute(URL::KCacheType)));
		#endif // SELFTEST

			URL url(url_rep, (const char *) NULL);
			if(add_list)
			{
				URLLink *item = OP_NEW(URLLink, (url));
				if (item)
				{
					OP_ASSERT(item->url.GetAttribute(URL::KLoadStatus) == URL_UNLOADED || item->url.GetRep()->GetDataStorage());
					
					item->Into(add_list);
				}
			}
		}
next_url:;
		url_rep = url_store->GetNextURL_Rep();
	}

#ifdef SEARCH_ENGINE_CACHE
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	if (url_store->m_operator_index.IsOpen() && OpStatus::IsError((err = url_store->m_operator_index.Commit(FALSE))))
	{
		url_store->m_operator_index.Abort();
		LEAVE(err);
	}
#endif

	if (OpStatus::IsError((err = url_store->m_disk_index.Commit(FALSE))))
	{
		url_store->m_disk_index.Abort();
		LEAVE(err);
	}
#endif

	// Limit the size of the disk (and operator) cache
	if(sort_and_limit)
	{
		OpFileLength dcache_list_limit = GetCacheSize();
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT
#if defined __OEM_OPERATOR_CACHE_MANAGEMENT
		OpFileLength ocache_list_limit = size_oem.GetSize();
#else
		OpFileLength ocache_list_limit = GetCacheSize();
#endif

#if !defined __OEM_OPERATOR_CACHE_MANAGEMENT
		OpFileLength ocache_size_used =
#endif
			SortURLListByDateAndLimit(ocache_list, vlink_list, ocache_list_limit, destroy_urls);
#if !defined __OEM_OPERATOR_CACHE_MANAGEMENT
		if(GetCacheSize() > ocache_size_used)
			dcache_list_limit -= ocache_size_used;
		else
			dcache_list_limit = 0;
#endif
#endif  //  __OEM_EXTENDED_CACHE_MANAGEMENT
		SortURLListByDateAndLimit(dcache_list, vlink_list, dcache_list_limit, destroy_urls);
	}

#if CACHE_CONTAINERS_ENTRIES>0
	#if defined __OEM_EXTENDED_CACHE_MANAGEMENT
		URLLinkHeadIterator ocache_iter(ocache_list);

		TraceMarkedContainers(&ocache_iter, TRUE, UNI_L("WriteCacheIndexesL() for OEM cache"));
	#endif // defined __OEM_EXTENDED_CACHE_MANAGEMENT

	URLLinkHeadIterator dcache_iter(dcache_list);

	TraceMarkedContainers(&dcache_iter, TRUE, UNI_L("WriteCacheIndexesL() for OEM cache"));
	// Collect garbage both from ocache_list and dcache_list, to prevent corruption
	GarbageCollectMarkedContainers();
#endif // CACHE_CONTAINERS_ENTRIES>0

	url_rep = url_store->GetFirstURL_Rep();
	
	// Dump files to disk and remove unvisited URLs
	if(sort_and_limit)
		while(url_rep)
		{
			AutoDeleteHead *add_list = NULL;

			URLType type = (URLType) url_rep->GetAttribute(URL::KType);
	#if (defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT) || defined SEARCH_ENGINE_CACHE
			time_t visited=0;
			url_rep->GetAttribute(URL::KVLocalTimeVisited, &visited);
	#endif

			URL_DataStorage *storage=url_rep->GetDataStorage();
			BOOL embedded=storage && storage->GetCacheStorage() && storage->GetCacheStorage()->IsEmbedded();
			URLStatus status =(URLStatus) url_rep->GetAttribute(URL::KLoadStatus);

			if ( (status == URL_LOADED || (status == URL_LOADING_ABORTED && url_rep->GetAttribute(URL::KMultimedia))) &&
				((url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_DISK) || embedded))
			{
				if (url_rep->DumpSourceToDisk(destroy_urls))
				{
	#if defined __OEM_EXTENDED_CACHE_MANAGEMENT
					if (context_id == 0 && url_rep->GetAttribute(URL::KNeverFlush))
					{
	#if !defined __OEM_OPERATOR_CACHE_MANAGEMENT
						if (visited >= never_flush_expiration_date)
							add_list = &vlink_list;
						else
	#endif // __OEM_OPERATOR_CACHE_MANAGEMENT
	#ifdef SEARCH_ENGINE_CACHE
						if (!IndexFileL(url_store->m_operator_index, url_rep, destroy_urls, size_oem.GetSize()))
							add_list = &vlink_list;
	#else
							add_list = &ocache_list;
	#endif
					} else
	#endif // __OEM_EXTENDED_CACHE_MANAGEMENT

	#ifdef SEARCH_ENGINE_CACHE
					if (!IndexFileL(url_store->m_disk_index, url_rep, destroy_urls, GetCacheSize()))
						add_list = &vlink_list;
	#else
					add_list = &dcache_list;
	#endif
				}
				else if(type == URL_HTTP || type == URL_FTP || type == URL_Gopher || type == URL_WAIS ||
					type == URL_FILE || type == URL_NEWS || type == URL_HTTPS || type == URL_SNEWS)
				{
					if (!url_rep->IsFollowed())
					{
						if (url_rep->GetRefCount() == 1 &&
							(URLStatus) url_rep->GetAttribute(URL::KLoadStatus) == URL_UNLOADED )
						{
							// Flush unvisited URLs
							url_rep->DecRef();
							url_store->RemoveURL_Rep(url_rep);
							is_writing_cache = TRUE;
							OP_DELETE(url_rep);
							is_writing_cache = FALSE;
							goto next_url2;
						}
					}
					else
						add_list = &vlink_list;
				}
			}

			{
				if(!add_list &&destroy_urls
	#ifdef SEARCH_ENGINE_CACHE
					|| (visited + 600 < g_timecache->CurrentTime() && url_rep->GetUsedCount() <= 0 && url_rep->GetRefCount() <= 1)
	#endif
					)
				{
					URL url(url_rep, (const char *) NULL);
					DestroyURL(url);
				}
			}
	next_url2:;
			url_rep = url_store->GetNextURL_Rep();
		} // end of 'if(sort_and_limit) while(url_rep)'

	if(context_id)
	{
	#ifdef CACHE_STATS
		long start=(long)g_op_time_info->GetRuntimeMS();
	#endif
#ifndef SEARCH_ENGINE_CACHE
		#ifdef CACHE_FAST_INDEX
			WriteIndexFileSimpleL(dcache_list, TAG_CACHE_FILE_ENTRY, DiskCacheFile, cache_loc, TRUE, destroy_urls);
		#else
			WriteIndexFileL(dcache_list, TAG_CACHE_FILE_ENTRY, DiskCacheFile, cache_loc, TRUE, destroy_urls);
		#endif
		WriteIndexFileL(vlink_list, TAG_VISITED_FILE_ENTRY, VlinkFile, cache_loc, FALSE, destroy_urls);
#endif

	#if CACHE_CONTAINERS_ENTRIES>0
		OpStatus::Ignore(FlushContainers());
	#endif

	#ifdef CACHE_STATS
		long end=(long)g_op_time_info->GetRuntimeMS();
		OpString ops;
		OpFile f;
		OpString prefix;
		double date=OpDate::GetCurrentUTCTime();
		
		prefix.AppendFormat(UNI_L("%d/%d/%d "), OpDate::YearFromTime(date), OpDate::MonthFromTime(date)+1, OpDate::DateFromTime(date)+1);
		prefix.AppendFormat(UNI_L("%d:%d:%d - "), OpDate::HourFromTime(date), OpDate::MinFromTime(date), OpDate::SecFromTime(date));
		
		#ifndef SEARCH_ENGINE_CACHE
			#ifdef CACHE_FAST_INDEX
				ops.AppendFormat(UNI_L("%s - Fast Cache 1 Write Time (%s): %d"), prefix.CStr(), DiskCacheFile, end-start);
			#else
				ops.AppendFormat(UNI_L("%s - Normal Cache 1 Write Time (%s): %d"), prefix.CStr(), DiskCacheFile, end-start);
			#endif
		#endif
		
		#if CACHE_SMALL_FILES_SIZE>0
			ops.AppendFormat(UNI_L("Embedded files - max size: %u - files embedded: %u\n"), CACHE_SMALL_FILES_SIZE, urlManager->GetEmbeddedFiles());
			ops.AppendFormat(UNI_L("Embedded files - mem used: %u KB - mem max: %u KB\n"), urlManager->GetEmbeddedSize()/1024, CACHE_SMALL_FILES_LIMIT/1024);
		#endif 
		
		f.Construct(UNI_L("cache_stats.txt"), OPFILE_CACHE_FOLDER);
		f.Open(OPFILE_APPEND);
		f.WriteUTF8Line(ops);
		f.Close();
	#endif
		return;
	}

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT
#if defined __OEM_OPERATOR_CACHE_MANAGEMENT
#ifndef SEARCH_ENGINE_CACHE
  #ifdef CACHE_FAST_INDEX
    WriteIndexFileSimpleL(ocache_list, TAG_CACHE_FILE_ENTRY, DiskCacheFile, OPFILE_OCACHE_FOLDER, TRUE, destroy_urls);
  #else
	WriteIndexFileL(ocache_list, TAG_CACHE_FILE_ENTRY, DiskCacheFile, OPFILE_OCACHE_FOLDER, TRUE, destroy_urls);
  #endif
#endif
#else
	{
		// Prepend the neverflush items before the normal cache items;
		URLLink *item;
		while((item = ocache_list.Last()) != NULL)
		{
			item->Out();
			item->IntoStart(&dcache_list);
		}
	}
#endif
#endif
#ifndef SEARCH_ENGINE_CACHE
	#ifdef CACHE_STATS
		long start=(long)g_op_time_info->GetRuntimeMS();
	#endif
	#ifdef CACHE_FAST_INDEX
		WriteIndexFileSimpleL(dcache_list, TAG_CACHE_FILE_ENTRY, DiskCacheFile, OPFILE_CACHE_FOLDER, TRUE, destroy_urls);
	#else
		WriteIndexFileL(dcache_list, TAG_CACHE_FILE_ENTRY, DiskCacheFile, OPFILE_CACHE_FOLDER, TRUE, destroy_urls);
	#endif
	
	#ifdef CACHE_STATS
		long end=(long)g_op_time_info->GetRuntimeMS();
		OpString ops;
		OpFile f;
		OpString prefix;
		double date=OpDate::GetCurrentUTCTime();
		
		prefix.Reserve(100);
		prefix.AppendFormat(UNI_L("%d/%d/%d "), OpDate::YearFromTime(date), OpDate::MonthFromTime(date)+1, OpDate::DateFromTime(date)+1);
		prefix.AppendFormat(UNI_L("%d:%d:%d - "), OpDate::HourFromTime(date), OpDate::MinFromTime(date), OpDate::SecFromTime(date));
		
		#ifdef CACHE_FAST_INDEX
			ops.AppendFormat(UNI_L("%s - Fast Cache 2 Write Time (%s): %d"), prefix.CStr(), DiskCacheFile, end-start);
		#else
			ops.AppendFormat(UNI_L("%s - Normal Cache 2 Write Time (%s): %d"), prefix.CStr(), DiskCacheFile, end-start);
		#endif
		
		f.Construct(UNI_L("cache_stats.txt"), OPFILE_CACHE_FOLDER);
		f.Open(OPFILE_APPEND);
		f.WriteUTF8Line(ops);
		f.Close();
	#endif // CACHE_STATS
	
	WriteIndexFileL(vlink_list, TAG_VISITED_FILE_ENTRY, VlinkFile, OPFILE_HOME_FOLDER, FALSE, destroy_urls);
#endif // SEARCH_ENGINE_CACHE

#if CACHE_CONTAINERS_ENTRIES>0
	OpStatus::Ignore(FlushContainers());
#endif

#ifdef SEARCH_ENGINE_CACHE
	if (destroy_urls && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EmptyCacheOnExit))
	{
		while (url_store->m_disk_index.DeleteOld(0) == OpBoolean::IS_FALSE)
			;

		url_store->m_disk_index.Commit();
	}
#endif
#if defined(_DEBUG) && defined(SEARCH_ENGINE_CACHE)
	err = url_store->m_disk_index.CheckConsistency();
	OP_ASSERT(err == OpStatus::OK);  // see ConsistencyStatus in CacheIndex.h for errors
#endif
}
#endif // DISK_CACHE_SUPPORT

#if CACHE_CONTAINERS_ENTRIES>0
	OP_STATUS MarkedContainer::AutoDelete()
	{
		if(!deleted)
		{
			OpFile f;

			RETURN_IF_ERROR(f.Construct(name.CStr(), folder));

		#ifdef SELFTEST
			BOOL b=FALSE;

			f.Exists(b);

			OP_NEW_DBG("MarkedContainer::AutoDelete", "cache.del");

			if(b)
				OP_DBG((UNI_L("Container %s auto-deleted in folder %d!\n"), name.CStr(), (int)folder));
			else
				OP_DBG((UNI_L("Container %s not present in folder %d!\n"), name.CStr(), (int)folder));

		#endif

		#if defined(USE_ASYNC_FILEMAN) && defined(UTIL_ASYNC_FILE_OP)
			RETURN_IF_ERROR(f.DeleteAsync(FALSE));
		#else
			RETURN_IF_ERROR(f.Delete(FALSE));
		#endif

			deleted=TRUE;
		}

		OP_ASSERT(deleted);

		return OpStatus::OK;
	}
#endif
