/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/url2.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/cache/cache_int.h"
#include "modules/formats/url_dfr.h"
#include "modules/cache/url_stor.h"
#include "modules/cache/cache_common.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/cache/url_cs.h"
#include "modules/cache/cache_man.h"
#include "modules/cache/cache_propagate.h"

#ifdef CACHE_FAST_INDEX
	#include "modules/cache/simple_stream.h"
#endif



#if defined(DISK_CACHE_SUPPORT) && !defined(SEARCH_ENGINE_CACHE)
OP_STATUS Context_Manager::OpenIndexFile(OpFile **fp, const OpStringC &name, OpFileFolder folder, OpFileOpenMode mode)
{
	OP_ASSERT(fp);
	*fp = NULL;

	OP_STATUS op_err;
	OpFile *f = OP_NEW(OpFile, ());
	if (!f)
		return OpStatus::ERR_NO_MEMORY;

	op_err = f->Construct(name.CStr(), folder);
	if(OpStatus::IsError(op_err))
	{
		OP_DELETE(f);
		return op_err;
	}
	
	op_err = f->Open(mode);
	if (OpStatus::IsMemoryError(op_err)) // Only check for OOM, IsOpen is checked later on.
	{
		OP_DELETE(f);
		return op_err;
	}

	*fp=f;
	return OpStatus::OK;
}

OP_STATUS Context_Manager::OpenCacheIndexFile(OpFile **fp, const OpStringC &name, const OpStringC &name_old, OpFileFolder folder)
{
	OP_ASSERT(fp);
	*fp = NULL;

	RETURN_IF_ERROR(OpenIndexFile(fp, name, folder, OPFILE_READ));

	if(!(*fp)->IsOpen())
	{
		OP_DELETE(*fp);
		*fp = NULL;

		RETURN_IF_ERROR(OpenIndexFile(fp, name_old, folder, OPFILE_READ));
		if(!(*fp)->IsOpen())
		{
			OP_DELETE(*fp);
			*fp = NULL;
			return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

#define CACHE_DIR_TAG_CONTENT "Signature: 8a477f597d28d172789f06886806bc55\n# This file is a cache directory tag created by Opera.\n# For information about cache directory tags, see:\n# http://www.brynosaurus.com/cachedir/"

#if defined(DISK_CACHE_SUPPORT) && !defined(SEARCH_ENGINE_CACHE)
#ifdef SELFTEST
void Context_Manager::TestReadIndexFileL(OpStringC filename, OpFileFolder folder, int &num_entries, OpString &next_entry)
{
	FileName_Store filenames(8191);
	ANCHOR(FileName_Store, filenames);
	OpString name_old;
	ANCHOR(OpString, name_old);
	OpString old;
	ANCHOR(OpString, old);
	
	next_entry.Empty();
	num_entries=0;
	LEAVE_IF_ERROR(old.Set(file_str));
	uni_strcpy(file_str, UNI_L("oprWRONG.tmp"));

	num_entries=ReadCacheIndexFileL(filenames, FALSE, filename, name_old, folder, TAG_CACHE_FILE_ENTRY, TRUE, FALSE);
	LEAVE_IF_ERROR(next_entry.Set(file_str));
	
	if(!uni_strcmp(file_str, "oprWRONG.tmp"))
		uni_strcpy(file_str, old.CStr());
}
#endif // SELFTEST
#endif

int Context_Manager::ReadCacheIndexFileL(FileName_Store &filenames, BOOL sync_cache, const OpStringC &name, const OpStringC &name_old, OpFileFolder folder, uint32 tag, BOOL read_last_entry, BOOL empty_disk_cache)
{
	int num_entries=0;
	
	#ifdef CACHE_STATS
		long time_start=(long)g_op_time_info->GetRuntimeMS();
	#endif
	
	// Manage CACHEDIR.TAG; the errors are ignored because this feature is "nice to have but optional"
#ifdef CACHE_GENERATE_CACHEDIR_FILE
	if(folder==OPFILE_CACHE_FOLDER)
	{
		OpFile tag_file;
		
		if(OpStatus::IsSuccess(tag_file.Construct(UNI_L("CACHEDIR.TAG"), folder)))
		{
			BOOL tag_file_present=TRUE;
			
			if(OpStatus::IsSuccess(tag_file.Exists(tag_file_present)) && !tag_file_present && OpStatus::IsSuccess(tag_file.Open(OPFILE_WRITE)))
			{
				OpStatus::Ignore(tag_file.Write(CACHE_DIR_TAG_CONTENT, op_strlen(CACHE_DIR_TAG_CONTENT)));
				OpStatus::Ignore(tag_file.Close());
			}
		}
	}
#endif
	
	// Manage the cache index file
	DataStream_NonSensitive_ByteArrayL(read_cache_index);

	OpFile *f=NULL;
	OpStackAutoPtr<URL_Rep> url_rep(0);	
	
#ifndef CACHE_FAST_INDEX
	OpStackAutoPtr<DataFile_Record> url_rec(0);
#endif

	OP_STATUS ops_open=OpenCacheIndexFile(&f,name,  name_old, folder);

	if(OpStatus::IsError(ops_open))	// No need to panic
	{
	#ifdef SELFTEST
			debug_deep = TRUE;    // Deep testing on the containers can only be performed if the index is initially empty. No file, no index... so it can be enabled
	#endif
	
		return 0;
	}

#ifdef CACHE_FAST_INDEX
	OpConfigFileReader scache;
	ANCHOR(OpConfigFileReader, scache);
	
	RETURN_VALUE_IF_ERROR(scache.Construct(f, TRUE), 0);  // No need to panic if the file is badly formed
#else
	DataFile dcachefile(f);
	ANCHOR(DataFile, dcachefile);

	RETURN_VALUE_IF_ERROR(dcachefile.InitL(), 0);  // No need to panic if the file is badly formed
#endif

#if 0
	{
		PrintfTofile("filelist.txt", "Starting DCache File list\n");
		FileNameElement *elm = filenames.GetFirstFileName();
		while(elm)
		{
			PrintfTofile("filelist.txt", "DCache File %s\n", elm->LinkId());
			elm = filenames.GetNextFileName();
		}
		PrintfTofile("filelist.txt", "end of DCache File list\n\n");
	}
#endif

	
#ifdef CACHE_STATS
	long time_init=(long)g_op_time_info->GetRuntimeMS();
#endif

#ifdef SELFTEST
	if(!url_store->GetFirstURL_Rep())
		debug_deep = TRUE;    // Deep testing on the containers can only be performed if the index is initially empty, or useless asserts can pop up
#endif

	FileName_Store container_files(8191);   // List of container files, which can compare multiple times in the index
	ANCHOR(FileName_Store, container_files);
	
	LEAVE_IF_ERROR(container_files.Construct());

#ifdef CACHE_FAST_INDEX
	UINT32 record_tag;
	int record_len;
	DiskCacheEntry entry;
	ANCHOR(DiskCacheEntry, entry);
	
	#if CACHE_SMALL_FILES_SIZE>0
		OpStatus::Ignore(entry.ReserveSpaceForEmbeddedContent(CACHE_SMALL_FILES_SIZE));
	#endif
	
	while(OpStatus::IsSuccess(scache.ReadNextRecord(record_tag, record_len)) && record_len>0)
#else
	url_rec.reset(dcachefile.GetNextRecordL());
	
	while(url_rec.get() != NULL)
#endif
	{
#ifdef CACHE_STATS
		stats.urls++;
#endif

#ifdef SELFTEST
		debug_deep = FALSE;	// Deep testing on the containers can only be performed if the index is initially empty, or useless asserts can pop up
#endif
		
#ifndef SEARCH_ENGINE_CACHE
		if(read_last_entry &&
			#ifdef CACHE_FAST_INDEX
				record_tag
			#else
				url_rec->GetTag()
			#endif
			 == TAG_CACHE_LASTFILENAME_ENTRY
		)
		{
			OpStringS8 filnam;
			ANCHOR(OpStringS8, filnam);
			
		#ifdef CACHE_FAST_INDEX
			scache.ReadString(&filnam, record_len);
		#else
			url_rec->GetStringL(filnam);
		#endif
			
			if(filnam.Length() == 5)
			{
				OpString tempfilnam;
				
				tempfilnam.SetFromUTF8L(filnam.CStr());
				uni_strncpy(file_str+3, tempfilnam.CStr(), 5);
				IncFileStr();
			}
		}
		else
#endif
#ifdef CACHE_FAST_INDEX
		if(record_tag==tag)
		{
			URL_Rep *url_rep_ptr;
			OP_STATUS ops=entry.ReadValues(&scache, record_tag);
			
			if(OpStatus::IsError(ops))
				break;
			num_entries++;
			
		#ifdef DEBUGGING
		DEBUG_CODE_BEGIN("cache.index")
			// As we check the existence of each file, this log can be relatively slow, and it is therefore protected by debug.txt
			OP_NEW_DBG("Context_Manager::ReadCacheIndexFileL()", "cache.index");
			OpString name16;
			OpString url16;
			OpFile f;
			BOOL exists=FALSE;

			url16.Set(entry.GetURL()->CStr());

			if(entry.GetFileName() && !entry.GetFileName()->IsEmpty())
			{
				name16.Set(entry.GetFileName()->CStr());
				f.Construct(name16.CStr(), folder);

				f.Exists(exists);
			}

			// Check if it is present in the file name list
			const uni_char *desc_file_name;
			FileNameElement *fn=filenames.RetrieveFileName(name16, NULL);

			if(fn)
				desc_file_name=UNI_L(" present");
			else
			{
				desc_file_name=UNI_L(" NOT present");

				FileNameElement *elm = filenames.GetFirstFileName();
				
				while(elm)
				{
					if(!elm->FileName().Compare(name16))
					{
						desc_file_name=UNI_L(" INCONSISTENCY ");
						OP_ASSERT(! "Inconsistency in FileName_Store - Check case in calls to GetHashIdx()");
					}

					elm = filenames.GetNextFileName();
				}
			}

			OP_DBG((UNI_L("*** URL %s: %s %s - %s in FileName_Store"), url16.CStr(), ( name16.IsEmpty()) ? UNI_L("(no file)") : name16.CStr(), ( !name16.IsEmpty()) ? (exists?UNI_L("EXISTS") : UNI_L("MISSING!")) : UNI_L(""), desc_file_name ));

			
		DEBUG_CODE_END
		#endif

			URL_Rep::CreateL(&url_rep_ptr, &entry,
#else
		if(url_rec->GetTag()==tag)
		{
			url_rec->SetRecordSpec(dcachefile.GetRecordSpec());
			url_rec->IndexRecordsL();
			
			URL_Rep *url_rep_ptr;
			URL_Rep::CreateL(&url_rep_ptr, url_rec.get(),
#endif
#ifndef SEARCH_ENGINE_CACHE
				 filenames, 
#endif
				 folder, context_id);

			// LinkObjectStore::AddLinkObject() no longer allows LinkId() to be NULL.
			if (!url_rep_ptr->LinkId())
			{
				OP_DELETE(url_rep_ptr);
				#ifndef CACHE_FAST_INDEX
					url_rec.reset();
				#endif
				continue;
			}
			url_rep.reset(url_rep_ptr);
			
			if(url_rep.get())
			{
#ifdef URL_ENABLE_ASSOCIATED_FILES
				OP_BOOLEAN assoc_file_exists;
				OpString afname;
				URL::AssociatedFileType aftype;

				#ifdef CACHE_FAST_INDEX
				if(entry.GetAssociatedFiles())
				#else
				if (url_rec->GetIndexedRecordUIntL(TAG_ASSOCIATED_FILES))
				#endif
				{
					FileNameElement *filename;

					OP_ASSERT(url_rep->GetDataStorage());

					// No point in leaving in case of error.
					assoc_file_exists = url_rep->GetDataStorage()->GetFirstAssocFName(afname, aftype);

					while (assoc_file_exists == OpBoolean::IS_TRUE)
					{
						filename = filenames.RetrieveFileName(afname.CStr(), NULL);
						if (filename)
						{
							filenames.RemoveFileName(filename);
							OP_DELETE(filename);
						}

						// No point in leaving in case of error.
						assoc_file_exists = url_rep->GetDataStorage()->GetNextAssocFName(afname, aftype);
					}
				}
#endif

				#if CACHE_SYNC!=CACHE_SYNC_NONE
					// Remove the URL for the list of files to delete, or unload it if required
					if((URLCacheType) url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_DISK)
					{
						if(sync_cache || empty_disk_cache) // If the cache appears to be in sync, there is nothing to do
						{
							URL_DataStorage *storage=url_rep->GetDataStorage();
							BOOL embedded=storage && storage->GetCacheStorage() && storage->GetCacheStorage()->IsEmbedded();

							if(!embedded)
							{
								if(empty_disk_cache) // Shortcut for Empty cache on exit
									url_rep->Unload();
								else
								{
									OpStringC temp_filename(url_rep->GetAttribute(URL::KFileName));

									FileNameElement *filename = filenames.RetrieveFileName(temp_filename.CStr(), NULL);

									if(filename)
									{
										filenames.RemoveFileName(filename);
				#ifdef _DEBUG
										FileNameElement *filename2 = filenames.RetrieveFileName(temp_filename.CStr(), NULL);

										OP_ASSERT(filename2 == NULL);
				#endif
										if(storage && storage->GetCacheStorage() && storage->GetCacheStorage()->GetContainerID())  // Containers need to be checked more than once
											container_files.AddFileName(filename);
										else
											OP_DELETE(filename);
									}
									else if(storage && storage->GetCacheStorage() && storage->GetCacheStorage()->GetContainerID())
									{
										// Before deleting, check if it is present in the containers list
										if(!container_files.RetrieveFileName(temp_filename.CStr(), NULL))
											url_rep->Unload();
									}
									else
										url_rep->Unload();
								}
							}
						}
					}
					else
				#endif
				if((URLCacheType) url_rep->GetAttribute(URL::KCacheType) == URL_CACHE_USERFILE && url_rep->GetAttribute(URL::KFileName).IsEmpty())
				{
					url_rep->Unload();
				}

				if(url_rep->InList())
				{
					url_store->RemoveURL_Rep(url_rep.get());
				}
				
				url_store->AddURL_Rep(url_rep.release());
			}
			
		}
		#ifndef CACHE_FAST_INDEX
			url_rec.reset(dcachefile.GetNextRecordL());
		#endif
	}
#ifdef CACHE_FAST_INDEX
	scache.Close();
#else
	dcachefile.Close();
#endif

    #ifdef CACHE_STATS
		long temp=(long)g_op_time_info->GetRuntimeMS()-time_start;
		
		stats.index_read_time+=temp;
		stats.index_num_read++;
		stats.index_init=time_init-time_start;
		if(temp>stats.max_index_read_time)
			stats.max_index_read_time=temp;
		
		OpString ops;
		OpFile flog;
		OpString prefix;
		double date=OpDate::GetCurrentUTCTime();
		
		prefix.Reserve(100);
		prefix.AppendFormat(UNI_L("%d/%d/%d "), OpDate::YearFromTime(date), OpDate::MonthFromTime(date)+1, OpDate::DateFromTime(date)+1);
		prefix.AppendFormat(UNI_L("%d:%d:%d - "), OpDate::HourFromTime(date), OpDate::MinFromTime(date), OpDate::SecFromTime(date));
		
		#ifdef CACHE_FAST_INDEX
			ops.AppendFormat(UNI_L("%s - Fast Cache Read Time (%s): %d"), prefix.CStr(), DiskCacheFile, temp);
		#else
			ops.AppendFormat(UNI_L("%s - Normal Cache Read Time (%s): %d"), prefix.CStr(), DiskCacheFile, temp);
		#endif
		
		flog.Construct(UNI_L("cache_stats.txt"), OPFILE_CACHE_FOLDER);
		flog.Open(OPFILE_APPEND);
		flog.WriteUTF8Line(ops);
		flog.Close();
	#endif

	OP_NEW_DBG("Context_Manager::ReadCacheIndexFileL()", "cache.ctx");
	OP_DBG((UNI_L("Read %d entries in %s for context %d in folder %d - Sync: %d - Empty disk cache: %d\n"), num_entries, name.CStr(), context_id, folder, sync_cache, empty_disk_cache));

	return num_entries;
}

#ifndef SEARCH_ENGINE_CACHE
void Context_Manager::ReadDCacheFileL()
{
	CACHE_PROPAGATE_ALWAYS_VOID(ReadDCacheFileL());

	if(cache_loc == OPFILE_ABSOLUTE_FOLDER)
		return;

	// Retrieve the list of cache files (and their associated files) in the disk
	FileName_Store filenames(8191);
	ANCHOR(FileName_Store, filenames);

	LEAVE_IF_ERROR(filenames.Construct());

#ifdef URL_ENABLE_ASSOCIATED_FILES
	FileName_Store associated(8191);
	FileName_Store associated_to_del(8191);
	ANCHOR(FileName_Store, associated);
	ANCHOR(FileName_Store, associated_to_del);
	
	LEAVE_IF_ERROR(associated.Construct());
	LEAVE_IF_ERROR(associated_to_del.Construct());
#endif // URL_ENABLE_ASSOCIATED_FILES
	
	BOOL empty_disk_cache = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EmptyCacheOnExit) ||
						!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheSize) ||
						(!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheDocs) &&
						 !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheFigs) &&
						 !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheOther));

	BOOL OP_MEMORY_VAR sync_cache=TRUE;
	
// Get the list of all the files on disk
#if CACHE_SYNC!=CACHE_SYNC_NONE
	#ifdef CACHE_STATS
		long sync1_start=(long)g_op_time_info->GetRuntimeMS();
	#endif

	#if CACHE_SYNC==CACHE_SYNC_ON_CHANGE
		// Check if the sync phase is required. No flag file, no sync required
		OpFile flag_file;
		BOOL flag_exists=TRUE;
		
		if (OpStatus::IsSuccess(flag_file.Construct(CACHE_ACTIVITY_FILE, GetCacheLocationForFilesCorrected())) &&
			OpStatus::IsSuccess(flag_file.Exists(flag_exists)))
		{
			if(!flag_exists)
				sync_cache=FALSE;  // No flag file, no sync
		}
	#endif

	// Always clean the "view source" directory
	ReadDCacheDir(filenames, filenames, GetCacheLocationForFiles(), FALSE, FALSE, CACHE_VIEW_SOURCE_FOLDER);

	// Always clean the directory used for session files.
	// Even if it probably makes sense to always do it (APART from when the folder of the directory is set to OPFILE_ABSOLUTE_FOLDER, or it will create a lot of problems to M2!),
	// for now this is limited to the main cache directory.
	// Up to 4096 files / directories (starting with "opr") are removed. This is also intended to clean the situation created by DSK-287066
	if(!context_id)
		ReadDCacheDir(filenames, filenames, GetCacheLocationForFiles(), FALSE, FALSE, CACHE_SESSION_FOLDER, 4096, FALSE);

	// View Source and session directory, must really be deleted, so they are just added to the list of files to delete, without slowing down
	// things in ReadCacheIndexFileL()
	DeleteFiles(filenames);

	OP_ASSERT(!filenames.LinkObjectCount());

	if(sync_cache || empty_disk_cache)
	{
		// Sync the main directory
	#ifdef URL_ENABLE_ASSOCIATED_FILES
		ReadDCacheDir(filenames, associated, GetCacheLocationForFiles(), TRUE, TRUE, NULL);
		// Mark for delete all the associated files WITHOUT a corresponding cache file	
		CheckAssociatedFilesList(filenames, associated, associated_to_del, FALSE);
	#else
		ReadDCacheDir(filenames, filenames, GetCacheLocationForFiles(), TRUE, FALSE, NULL);
	#endif // URL_ENABLE_ASSOCIATED_FILES
	}

	#ifdef CACHE_STATS
		stats.sync_time1+=(long)g_op_time_info->GetRuntimeMS()-sync1_start;
	#endif
#endif // CACHE_SYNC!=CACHE_SYNC_NONE
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	//read the operator cache files as well
	#if CACHE_SYNC!=CACHE_SYNC_NONE
		if(sync_cache && !context_id)
			ReadDCacheDir(filenames,filenames,OPFILE_OCACHE_FOLDER, TRUE, FALSE, NULL);
	#endif
#endif

	TRAPD(op_err, ReadCacheIndexFileL(filenames, sync_cache, DiskCacheFile,  DiskCacheFileOld, GetCacheLocationForFiles(), TAG_CACHE_FILE_ENTRY, TRUE, empty_disk_cache));

	if(OpStatus::IsMemoryError(op_err))
		LEAVE(op_err);
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	if (!context_id)
	{
		TRAP(op_err, ReadCacheIndexFileL(filenames, sync_cache, DiskCacheFile,  DiskCacheFileOld, OPFILE_OCACHE_FOLDER, TAG_CACHE_FILE_ENTRY, TRUE, FALSE));
		if(OpStatus::IsMemoryError(op_err))
			LEAVE(op_err);
	}
#endif

#if CACHE_SYNC!=CACHE_SYNC_NONE
	#ifdef CACHE_STATS
		long sync2_start=(long)g_op_time_info->GetRuntimeMS();
	#endif

	// Sync also associated files
    #ifdef URL_ENABLE_ASSOCIATED_FILES
		// Mark for delete all the associated files WITH a corresponding cache file (as now this list is only about files to delete)
		if(associated.GetFirstFileName())
			CheckAssociatedFilesList(filenames, associated, associated_to_del, TRUE);

		DeleteFiles(associated_to_del);
	#endif // URL_ENABLE_ASSOCIATED_FILES

	if(sync_cache || filenames.GetFirstFileName())
		DeleteFiles(filenames);
	
	#ifdef CACHE_STATS
		stats.sync_time2+=(long)g_op_time_info->GetRuntimeMS()-sync2_start;
	#endif
#endif

	{
		// Sort the list of URLs
		Head collect_list;
		Head sort_list[256];

		//OP_ASSERT(LRU_ram == NULL && LRU_temp == NULL);

		unsigned int shift;
		unsigned int i;
		time_t index;

		// First radix sort path / shunt non-cachables into url_storage;
		shift = 0;
		URL_DataStorage* url_ds = LRU_disk;
		while (url_ds)
		{
			URL_DataStorage* url_ds_next = (URL_DataStorage *) url_ds->Suc();

			url_ds->Out();

			index = 0;
			url_ds->url->GetAttribute(URL::KVLocalTimeVisited, &index);
			if(!index)
				url_ds->GetAttribute(URL::KVLocalTimeLoaded, &index);
			url_ds->Into(&sort_list[ ((index) >> shift) & 0xff]);
			url_ds = url_ds_next;
		}

		for(i=0;i< 256;i++)
		{
			collect_list.Append(&sort_list[i]);
		}

		while(shift<(sizeof(index)*8 -8))
		{
			shift += 8;

			while((url_ds = (URL_DataStorage *) collect_list.First()) != NULL)
			{
				url_ds->Out();
				index = 0;
				url_ds->url->GetAttribute(URL::KVLocalTimeVisited, &index);
				if(!index)
					url_ds->GetAttribute(URL::KVLocalTimeLoaded, &index);
				url_ds->Into(&sort_list[ (index >> shift) & 0xff]);
			}

			for(i=0;i< 256;i++)
			{
				collect_list.Append(&sort_list[i]);
			}
		}

		while((url_ds = (URL_DataStorage *) collect_list.First()) != NULL)
		{
			url_ds->Out();
			url_ds->Into(&LRU_list);
		}

		LRU_disk = (URL_DataStorage *) LRU_list.First();
	}
}
#endif  // !SEARCH_ENGINE_CACHE

void Context_Manager::ReadVisitedFileL()
{
	CACHE_PROPAGATE_ALWAYS_VOID(ReadVisitedFileL());

	if(cache_loc == OPFILE_ABSOLUTE_FOLDER)
		return;

	FileName_Store dummy(1);
	ANCHOR(FileName_Store, dummy);

	LEAVE_IF_ERROR(dummy.Construct());

	TRAPD(op_err, ReadCacheIndexFileL(dummy, TRUE, VlinkFile,  VlinkFileOld, context_id ? cache_loc : OPFILE_HOME_FOLDER, TAG_VISITED_FILE_ENTRY, FALSE, FALSE));

	if(OpStatus::IsMemoryError(op_err))
		LEAVE(op_err);
}
#endif // DISK_CACHE_SUPPORT
