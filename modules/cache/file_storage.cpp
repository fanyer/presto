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

#include "core/pch.h"

#include "modules/util/opfile/opfile.h"
#include "modules/url/url2.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/cache/url_cs.h"
#include "modules/cache/cache_debug.h"
#include "modules/cache/cache_int.h"
#include "modules/cache/cache_common.h"
#include "modules/cache/cache_ctxman_disk.h"
#include "modules/url/url_ds.h"

#include "modules/olddebug/timing.h"

#ifdef _DEBUG
#ifdef YNP_WORK
#include "modules/olddebug/tstdump.h"
#define _DEBUG_DD
#define _DEBUG_DD1
#include "modules/url/tools/url_debug.h"
#endif
#endif

unsigned long GetFileError(OP_STATUS op_err, URL_Rep *url, const uni_char *operation);

#ifdef UNUSED_CODE
File_Storage::File_Storage(Cache_Storage *source, URLCacheType ctyp, BOOL frc_save, BOOL kp_open_file)
:Cache_Storage(source)
{ 
	cachetype = ctyp;
	cache_file = NULL;
	info.force_save = frc_save;
	info.keep_open_file = kp_open_file;
	info.dual = FALSE;
	info.completed = FALSE;
	info.computed = FALSE;
#ifdef SEARCH_ENGINE_CACHE
	info.modified = FALSE;
#endif


	file = NULL;
	file_count = 0;
	folder = OPFILE_ABSOLUTE_FOLDER;

#ifdef URL_ENABLE_ASSOCIATED_FILES
	purge_assoc_files = 0;
#endif
}
#endif

File_Storage::File_Storage(URL_Rep *url_rep, URLCacheType ctyp, BOOL frc_save, BOOL kp_open_file)
:Cache_Storage(url_rep)
{
	cachetype = ctyp;
	cache_file = NULL;
	file_count = 0;
	info.force_save = frc_save;
	info.keep_open_file = kp_open_file;
	info.dual = FALSE;
	info.memory_only_storage = FALSE;
#ifdef SEARCH_ENGINE_CACHE
	info.modified = FALSE;
#endif

	folder = OPFILE_ABSOLUTE_FOLDER;
	info.completed = FALSE;
	info.computed = FALSE;
	content_size = 0;
	stored_size = 0;

#ifdef URL_ENABLE_ASSOCIATED_FILES
	purge_assoc_files = 0;
#endif
}

File_Storage::~File_Storage()
{
	Purge();
}

OP_STATUS File_Storage::Construct(Cache_Storage *source, const OpStringC &target)
{
	OP_ASSERT(source);
	RETURN_IF_ERROR(filename.Set(target));
	if(GetCacheType() == URL_CACHE_DISK || GetCacheType() == URL_CACHE_TEMP )
	{
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		if(url->GetAttribute(URL::KNeverFlush))
			folder = OPFILE_OCACHE_FOLDER;
		else
#endif
			folder = OPFILE_CACHE_FOLDER;
	}
	else
		folder = OPFILE_ABSOLUTE_FOLDER;

	BOOL source_finished = source->GetFinished();
	source->SetFinished();
	if(filename.HasContent() || info.force_save)
	{
		OP_ASSERT(cache_content.GetLength() || filename.HasContent());

		if(cache_content.GetLength())
			RETURN_IF_ERROR(CheckFilename());

		if(filename.HasContent())
		{
			OpFileFolder folder1 = OPFILE_ABSOLUTE_FOLDER;
			OpStringC src_name(source->FileName(folder1));

			if(src_name.HasContent())
			{
#ifndef RAMCACHE_ONLY
				OP_STATUS op_err = CopyCacheFile(src_name, folder1, NULL, OPFILE_ABSOLUTE_FOLDER, TRUE, FALSE, FILE_LENGTH_NONE);
				if(OpStatus::IsError(op_err))
#endif
					SetFinished();

			}
			if(cache_content.GetLength() != 0)
			{
				info.dual = TRUE;
				if(Head::Empty())
				{
					info.dual = FALSE;
					FlushMemory();
				}
			}
		}

		OP_DELETE(source);
	}
	if(source_finished)
		SetFinished();
	return OpStatus::OK;
}

OP_STATUS File_Storage::Construct(const OpStringC &file_name, FileName_Store *filenames, OpFileFolder afolder, OpFileLength file_len)
{
	RETURN_IF_ERROR(filename.Set(file_name));
	if(GetCacheType() == URL_CACHE_DISK || GetCacheType() == URL_CACHE_TEMP )
	{
		folder = afolder;
	}
	else
		folder = OPFILE_ABSOLUTE_FOLDER;

	info.completed = (filename.HasContent() ? TRUE : FALSE);

#if !defined(RAMCACHE_ONLY) && !defined(SEARCH_ENGINE_CACHE)
	if(HasContentBeenSavedToDisk() && IsDiskContentAvailable())
	{
		if(!content_size)
			content_size=file_len;

		if(!content_size && filenames && filename.HasContent())
		{
		#ifndef CACHE_MULTIPLE_FOLDERS
			FileNameElement *elm = filenames->RetrieveFileName(filename, NULL);
			if(elm)
				content_size = elm->GetFileSize();
//			else
//				return OpStatus::ERR_NO_MEMORY;
		#endif
		}

		if(!content_size)
			content_size = FileLength();

		OP_NEW_DBG("File_Storage::Construct(OpStringC, FileName_Store, OpFileFolder, OpFileLength)", "cache.limit");
		OP_DBG((UNI_L("URL: %s - %x - content_size: %d - ctx id: %d - read_only: %d - dual: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, url->GetContextId(), read_only, info.dual));

		OP_ASSERT(!IsRAMContentComplete());
		AddToCacheUsage(url, FALSE, TRUE, TRUE);

#if defined STRICT_CACHE_LIMIT2
		GetContextManager()->AddToPredictedCacheSize(content_size, url);
#endif
	}
#endif  // !RAMCACHE_ONLY && !SEARCH_ENGINE_CACHE

#ifdef SEARCH_ENGINE_CACHE
	if(info.completed && (GetCacheType() == URL_CACHE_DISK || GetCacheType() == URL_CACHE_TEMP ))
	{
		OpFile temp_file;
		BOOL exists;

		if(OpStatus::IsSuccess(temp_file.Construct(filename, folder)))
		{
			temp_file.GetFileLength(content_size);
			if(temp_file.Exists(exists) == OpStatus::OK)
				if(exists)
					SetCacheType(URL_CACHE_DISK);
		}
	}
#endif

	return OpStatus::OK;
}

OP_STATUS File_Storage::CheckFilename()
{
	if(filename.HasContent())
		return OpStatus::OK;

	OpString ext;
	OP_STATUS op_err = OpStatus::OK;
	
	TRAP(op_err, url->GetAttributeL(URL::KSuggestedFileNameExtension_L, ext));

	if(OpStatus::IsSuccess(op_err))
	{
		BOOL session_only=!IsPersistent() || GetCacheType() == URL_CACHE_TEMP;

		op_err = urlManager->GetNewFileNameCopy(filename, (ext.HasContent() ? ext.CStr() : UNI_L("tmp")), folder, session_only
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
			, !!url->GetAttribute(URL::KNeverFlush)
#endif
			, url->GetContextId());

		// Fix a bug that when [Disk Cache]Cache Other==0 prevented session files from being deleted
		if(session_only && GetCacheType()==URL_CACHE_DISK)
			SetCacheType(URL_CACHE_TEMP);

		OP_NEW_DBG("File_Storage::CheckFilename()", "cache.name");
		OP_DBG((UNI_L("New cache file name:%s [%d] - Session only: %d - URL: %s\n"), filename.CStr(), GetFolder(), !IsPersistent() || GetCacheType() == URL_CACHE_TEMP, url->GetAttribute(URL::KUniName).CStr()));
	}

	if(OpStatus::IsMemoryError(op_err))
		g_memory_manager->RaiseCondition(op_err);
		
	OP_ASSERT(OpStatus::IsError(op_err) || filename.HasContent());
	
	return op_err;
}

#ifdef UNUSED_CODE
File_Storage* File_Storage::Create(Cache_Storage *source, const OpStringC &target, URLCacheType ctyp, BOOL frc_save, BOOL kp_open_file)
{
	File_Storage* fs = OP_NEW(File_Storage, (source, ctyp, frc_save, kp_open_file));
	if(!fs)
		return NULL;
	if(OpStatus::IsError(fs->Construct(source, target)))
	{
		OP_DELETE(fs);
		return NULL;
	}
	return fs;
}
#endif

#ifdef CACHE_FILE_STORAGE_CREATE
File_Storage* File_Storage::Create(URL_Rep *url_rep,  const OpStringC &file_name, FileName_Store *filenames, OpFileFolder folder, URLCacheType ctyp, BOOL frc_save, BOOL kp_open_file)
{
	File_Storage* fs = OP_NEW(File_Storage, (url_rep, ctyp, frc_save, kp_open_file));
	if(!fs)
		return NULL;
	if(OpStatus::IsError(fs->Construct(file_name,filenames, folder)))
	{
		OP_DELETE(fs);
		return NULL;
	}
	return fs;
}
#endif

void File_Storage::SetCacheType(URLCacheType ct)
{
#ifdef SEARCH_ENGINE_CACHE
	if((folder == OPFILE_CACHE_FOLDER
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		|| folder == OPFILE_OCACHE_FOLDER
#endif
		) && cachetype == URL_CACHE_DISK && ct != URL_CACHE_DISK)
	{
		urlManager->MakeFileTemporary(url);
	}
#endif

	cachetype = ct;
}

const OpStringC File_Storage::FileName(OpFileFolder &fldr, BOOL get_original_body) const
{
	fldr = folder;
	return filename;
}

void File_Storage::SetFinished(BOOL force)
{
	OP_NEW_DBG("File_Storage::SetFinished()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - read_only: %d - info.completed: %d ==> TRUE - IsDiskContentAvailable(): %d \n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, (UINT32) cache_content.GetLength(), url->GetContextId(), read_only, info.completed, IsDiskContentAvailable()));

	if(cache_file)
	{
		OP_DELETE(cache_file);
		cache_file = NULL;
		if(file_count > 1)
		{
			OP_STATUS op_err = OpStatus::OK;
			cache_file = OpenFile(OPFILE_READ, op_err);
			if(OpStatus::IsError(op_err))
				url->HandleError(GetFileError(op_err, url,UNI_L("write")));
		}

		DecFileCount();
	}

	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - read_only: %d - info.completed: %d - IsDiskContentAvailable(): %d \n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, (UINT32) cache_content.GetLength(), url->GetContextId(), read_only, info.completed, IsDiskContentAvailable()));

	Cache_Storage::SetFinished();

#ifdef SUPPORT_RIM_MDS_CACHE_INFO
	if( !info.completed )
		urlManager->ReportCacheItem(url,TRUE);
#endif // SUPPORT_RIM_MDS_CACHE_INFO

	info.completed = TRUE;
}

BOOL File_Storage::IsDiskContentAvailable()
{
	return filename.HasContent()
#ifdef SEARCH_ENGINE_CACHE
		&& !info.modified
#endif
		&& (GetCacheType() == URL_CACHE_DISK || GetCacheType() == URL_CACHE_TEMP);
}


void File_Storage::UnsetFinished()
{
	OP_NEW_DBG("File_Storage::UnsetFinished()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - GetDiskContentComputed(): %d - read_only: %d - info.completed: %d ==> FALSE - IsDiskContentAvailable(): %d \n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, (UINT32) cache_content.GetLength(), url->GetContextId(), GetDiskContentComputed(), read_only, info.completed, IsDiskContentAvailable()));

	Cache_Storage::UnsetFinished();

	if(GetDiskContentComputed())
		SubFromDiskOrOEMSize(0, url, FALSE); // Removes the estimated URL size

	if(cache_file)
	{
		OpFileLength pos = 0;

		// TODO: Multimedia: Read Pos
		if(OpStatus::IsError(cache_file->GetFilePos(pos)))
			pos = 0;
		OP_DELETE(cache_file);
		
		OP_STATUS op_err = OpStatus::OK;
		cache_file = OpenFile((OpFileOpenMode) (OPFILE_APPEND| OPFILE_READ), op_err);
		OpStatus::Ignore(op_err);
		
		if(cache_file && pos)
			cache_file->SetFilePos(pos);
	}
	info.completed = FALSE;
}

void File_Storage::ResetForLoading()
{
	OP_NEW_DBG("File_Storage::ResetForLoading", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - read_only: %d - dual: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, cache_content.GetLength(), url->GetContextId(), read_only, info.dual));

	Cache_Storage::ResetForLoading();
	info.completed = FALSE;
}

void File_Storage::DecFileCount()
{
	if(cache_file)
	{
		if(file_count > 0)
			file_count --;
		else
			file_count = 0;

		if(file_count==0 && cache_file)
		{
			OP_DELETE(cache_file);
			cache_file = NULL;
		}
	}
	else
		file_count = 0;
}

OP_STATUS File_Storage::StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position)
{
	if(!buffer)
	{
		OP_ASSERT(!buf_len);

		return buf_len ? OpStatus::ERR_NULL_POINTER : OpStatus::OK;
	}

	if(GetFinished())
		return OpStatus::OK;

	OP_NEW_DBG("File_Storage::StoreData()", "cache.storage");
	OP_DBG((UNI_L("URL: %s - %x - bytes stored: %d - URL points to me: %d - dual: %d - completed: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, buf_len, url->GetDataStorage() && url->GetDataStorage()->GetCacheStorage()==this, info.dual, info.completed));

#ifdef _DEBUG_DD1
	OpFileLength temp_len = ContentLoaded();
#endif
	urlManager->SetLRU_Item(url);
	if(info.dual || !info.force_save)
	{
		RETURN_IF_ERROR(Cache_Storage::StoreData(buffer, buf_len));
#ifdef _DEBUG_DD1
		PrintfTofile("urldd1.txt","\nFS Store Data- %s - Stored to memory ( %lu:%lu)\n",
			DebugGetURLstring(url), buf_len, (unsigned long) ContentLoaded()
			);
#endif
	}

	if((filename.HasContent()
#ifdef SEARCH_ENGINE_CACHE
		&& !info.modified
#endif
		) || info.force_save || 
		(cache_content.GetLength() >= MIN_FORCE_DISK_STORAGE && !GetMemoryOnlyStorage()
#ifdef NEED_URL_CACHE_TRIPLE_LIMIT_FORCE_DISK
		&& (Cardinal() == 0 || cache_content.GetLength() >= (3*MAX_MEMORY_CONTENT_SIZE)) // EPOC only: Don't force to disk, unless larger than 768KB
#endif
		))
	{
		OP_MEMORY_VAR BOOL saved = FALSE;

		if(filename.IsEmpty())
		{
			RETURN_IF_ERROR(CheckFilename());

			if(cache_content.GetLength())
			{
#ifndef RAMCACHE_ONLY
				if(OpStatus::IsError(CopyCacheFile(NULL, OPFILE_ABSOLUTE_FOLDER, NULL, OPFILE_ABSOLUTE_FOLDER, TRUE, FALSE, start_position)))
#endif
					SetFinished();
#ifdef _DEBUG_DD1
				PrintfTofile("urldd1.txt","\nFS Store Data- %s - Saved to File (%lu)\n",
					DebugGetURLstring(url), (unsigned long) ContentLoaded()
					);
#endif
				saved = TRUE;
				info.dual = TRUE;
			}

			if(!info.dual && Cardinal() == 0 && !info.force_save)
			{
#ifdef _DEBUG_DD1
				PrintfTofile("urldd1.txt","\nFS Store Data- %s - Flushing Directly to File (%lu)\n",
					DebugGetURLstring(url), (unsigned long) ContentLoaded()
					);
#endif
				FlushMemory();
				info.force_save = TRUE;
			}
		}

		BOOL orig_dual = info.dual;
		if(info.dual && cache_content.GetLength() > MAX_MEMORY_CONTENT_SIZE)
		{
#ifdef _DEBUG_DD1
		PrintfTofile("urldd1.txt","\nFS Store Data- %s - Flushing to File %lu\n",
			DebugGetURLstring(url), (unsigned long) ContentLoaded()
			);
#endif
			OP_NEW_DBG("File_Storage::StoreData - cache_content exceeded MAX_MEMORY_CONTENT_SIZE", "cache.limit");
			OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - read_only: %d - dual: %d ==> FALSE\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, cache_content.GetLength(), url->GetContextId(), read_only, info.dual));

			// If it's being flushed to disk we need to update the used cache size.
			OP_ASSERT(IsDiskContentAvailable());

			if(IsDiskContentAvailable())
				AddToDiskOrOEMSize(cache_content.GetLength(), url, FALSE);

			FlushMemory(TRUE);
			info.dual = FALSE;
			info.force_save = TRUE;
			info.keep_open_file = TRUE;
		}
		
		if(!saved)
		{
			OP_STATUS op_err = OpStatus::OK;

			if(!cache_file)
			{
				cache_file = OpenFile((OpFileOpenMode) (OPFILE_APPEND | OPFILE_READ), op_err);
				if(cache_file)
					file_count ++;
			}

			if(cache_file)
			{
				__DO_START_TIMING(TIMING_FILE_OPERATION);
				// TODO: Multimedia: Where to get the position in the content? Pos?
				if(start_position != FILE_LENGTH_NONE)
					op_err=SetWritePosition(cache_file, start_position);

				if(OpStatus::IsSuccess(op_err))
				{
					if(OpStatus::IsSuccess(op_err = cache_file->Write(buffer, buf_len)))
					{
						// if info.dual was set, the content is both kept on RAM and saved on the disk
						if(!orig_dual && IsDiskContentAvailable())
							AddToDiskOrOEMSize(buf_len, url, FALSE);
					}
				}

				if(op_err == OpStatus::ERR_NOT_SUPPORTED)  // We are trying to write content already saved...
				{
					// To simplify things, a successful operation is simulated, but NOTHING is written on disk
				#ifdef SELFTEST
					OP_ASSERT(bypass_asserts || (FALSE && "Operation not supported and probably wasted bandwidth"));
				#endif

					op_err=OpStatus::OK;
				}

				if(OpStatus::IsError(op_err))
				{
								__DO_STOP_TIMING(TIMING_FILE_OPERATION);
					url->HandleError(GetFileError(op_err, url,UNI_L("write")));
					OP_DELETE(cache_file);
					cache_file = NULL;
					SetFinished(TRUE); // Make sure it doesn't try to flush
					return op_err;
				}
				// else cache_file->Flush();

#ifdef _DEBUG_DD1
				PrintfTofile("urldd1.txt","\nFS Store Data- %s - Stored to file ( %lu:%lu)\n",
				DebugGetURLstring(url), buf_len, (unsigned long) ContentLoaded()
				);
#endif
				if(!orig_dual && filename.IsEmpty())
				{
					content_size += buf_len;
					stored_size -= buf_len; // Count only once
				}

				if(!info.keep_open_file)
				{
					DecFileCount();
				}
								__DO_ADD_TIMING_PROCESSED(TIMING_FILE_OPERATION,buf_len);
								__DO_STOP_TIMING(TIMING_FILE_OPERATION);
			}
		}
	}
#ifdef _DEBUG_DD1
	PrintfTofile("urldd1.txt","\nFS Store Data- %s - Added content %lu:%lu (%lu)\n",
		DebugGetURLstring(url), buf_len, (unsigned long) ContentLoaded(), temp_len+buf_len
		);
	//if(temp_len+buf_len != ContentLoaded())
	//	int test = 1;
#endif

	return OpStatus::OK;
}

OpFileLength File_Storage::EstimateContentAvailable()
{
	OpFileLength file_len=0;

	if(OpStatus::IsSuccess(cache_file->GetFileLength(file_len)))
		return file_len;

	return 0;
}

unsigned long File_Storage::RetrieveData(URL_DataDescriptor *desc,BOOL &more)
{
	OP_NEW_DBG("File_Storage::RetrieveData()", "cache.storage");
	OP_DBG((UNI_L("URL: %s"), url->GetAttribute(URL::KUniName).CStr()));

	#ifdef SELFTEST
		num_disk_read++;
	#endif

	if(desc == NULL)
		return 0;

	//info.force_save = FALSE;
	more = FALSE;
	
#ifdef CACHE_STATS
	retrieved_from_disk=FALSE;
#endif // CACHE_STATS


	urlManager->SetLRU_Item(url);
	
	if(!info.dual && filename.HasContent()
#ifdef SEARCH_ENGINE_CACHE
		&& !info.modified
#endif
		)
	{
		Context_Manager *ctx=GetContextManager();
		unsigned long ret;
		OP_STATUS ops;

		OP_ASSERT(ctx);

		/// Retrieve from Containers, for example
		if(ctx->BypassStorageRetrieveData(this, desc, more, ret, ops))
		{
			if(OpStatus::IsSuccess(ops)) // Retrieved
			{
				// FIXME: check if the file can be read in a single operation...	
				info.completed=TRUE;  /// Warning: thread safe?

			#ifdef CACHE_STATS
				retrieved_from_disk=TRUE;
			#endif
			}
			else // Not retrieved
			{
			#ifdef CACHE_STATS
				retrieved_from_disk=FALSE;
			#endif
			
			if(filename.CStr())
				filename.CStr()[0]=0;
			}

			OP_NEW_DBG("File_Storage::RetrieveData()", "cache.storage");
			OP_DBG((UNI_L("(bypassed --> containers) URL: %s - more: %d from %s\n"), url->GetAttribute(URL::KUniName).CStr(), more, filename.CStr()));

			return ret;
		}
		
		// Load the content from the file
		if(!cache_file)
		{
			OP_STATUS op_err = OpStatus::OK;

			cache_file = OpenFile(info.completed ? OPFILE_READ : (OpFileOpenMode) (OPFILE_APPEND | OPFILE_READ), op_err);
		}
		
		more = FALSE;
		if(!cache_file)
		{
			OP_NEW_DBG("File_Storage::RetrieveData()", "cache.storage");
			OP_DBG((UNI_L("URL: %s - File NOT opened!\n"), url->GetAttribute(URL::KUniName).CStr()));

			return desc->GetBufSize();
		}
		if(!desc->GetUsingFile())
		{
			file_count ++;
			desc->SetUsingFile(TRUE);
		}
		
		OpFileLength len_estimation = 0;
		
		if(!content_size)
			len_estimation=EstimateContentAvailable();
		OP_MEMORY_VAR OpFileLength bread=desc->AddContentL(cache_file, content_size ? content_size : len_estimation, more);
		
		if(!more)
		{
			// TODO: Multimedia: check if real EOF or if related to the segment
			if(cache_file && !cache_file->Eof())
			{
				if(bread != 0 || (!GetFinished() && (URLStatus) url->GetAttribute(URL::KLoadStatus) == URL_LOADING)) 
					more = TRUE;
				 if(!desc->PostedMessage() && (bread != 0 || (URLStatus) url->GetAttribute(URL::KLoadStatus) != URL_LOADING))
					desc->PostMessage(MSG_URL_DATA_LOADED, url->GetID(), 0);
			}
			else if(!GetFinished() && (URLStatus) url->GetAttribute(URL::KLoadStatus) == URL_LOADING && desc->HasMessageHandler())
				more = TRUE;
		}
		else if(!desc->PostedMessage())
			desc->PostMessage(MSG_URL_DATA_LOADED, url->GetID(), 0);

		OP_NEW_DBG("File_Storage::RetrieveData()", "cache.storage");

	#if CACHE_CONTAINERS_ENTRIES>0
		OP_DBG((UNI_L("URL: %s - more: %d from %s - Container id: %d - URL ID: %p - Position: %d - Descriptor address: %p - Bytes read: %d"), url->GetAttribute(URL::KUniName).CStr(), more, filename.CStr(), container_id, url->GetID(), (int)desc->GetPosition(), desc, (int)bread));
	#else
		OP_DBG((UNI_L("URL: %s - more: %d from %s - Container DISABLED - URL ID: %p - Position: %d - Descriptor address: %p - Bytes read: %d"), url->GetAttribute(URL::KUniName).CStr(), more, filename.CStr(), url->GetID(), (int)desc->GetPosition(), desc, (int)bread));
	#endif

	#ifdef DEBUGGING
	DEBUG_CODE_BEGIN("cache.storage")
		OpStringC8 mime=url->GetAttribute(URL::KMIME_Type);
		OpString mime16;
		URL moved_to = url->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);

		mime16.Set(mime.CStr());

		OP_DBG((UNI_L("MIME Type: %s - Moved to: %s"), mime16.CStr(), moved_to.GetAttribute(URL::KUniName).CStr()));
		if(desc->GetBufSize()>=10)
			OP_DBG((UNI_L("Buf size: %d - content: %d %d %d %d %d %d %d %d %d %d"), desc->GetBufSize(), desc->GetBuffer()[0], desc->GetBuffer()[1], desc->GetBuffer()[2], desc->GetBuffer()[3], desc->GetBuffer()[4], desc->GetBuffer()[5], desc->GetBuffer()[6], desc->GetBuffer()[7], desc->GetBuffer()[8], desc->GetBuffer()[9]));

	DEBUG_CODE_END
	#endif
			
#ifdef CACHE_STATS
		retrieved_from_disk=TRUE;
#endif
		
#ifdef _DEBUG_DD1
		PrintfTofile("urldd1.txt","\nFS Retrieve Data- %s - %u %s - %s - %s\n",
			DebugGetURLstring(url),
			desc->GetPosition(),		
			(url->GetAttribute(URL::KLoadStatus) == URL_LOADING ? "Not Loaded" : "Loaded"),
			(GetFinished() ? "Finished" : "Not Finished"),
			(desc->HasMessageHandler() ? "MH" : "not MH")
			);
#endif
		return desc->GetBufSize();
	}

#ifdef _DEBUG_DD1
	PrintfTofile("urldd1.txt","\nFS Retrieve Data- %s - from memory\n", DebugGetURLstring(url));
#endif

	return Cache_Storage::RetrieveData(desc, more);
}

#ifdef CACHE_RESOURCE_USED
OpFileLength File_Storage::ResourcesUsed(ResourceID resource)
{
	if(resource == MEMORY_RESOURCE)
		return Cache_Storage::ResourcesUsed(resource);

	return 0;
}
#endif

OpFileLength File_Storage::FileLength()
{
	if(cache_file)
	{
		OpFileLength length;
		// TODO: Multimedia: Real length? Content length? Segment length?
		cache_file->GetFileLength(length);
		return length;
	}


	if(filename.HasContent()
#ifdef SEARCH_ENGINE_CACHE
		&& !info.modified
#endif
		)
	{
		OpFileLength file_size = 0;
		OpFile temp_file;

		if(OpStatus::IsSuccess(temp_file.Construct(filename.CStr(), folder, OPFILE_FLAGS_NONE)))
		{
			temp_file.GetFileLength(file_size);
			content_size = file_size;
		}
	}

	return content_size;
}

BOOL File_Storage::DataPresent()
{
	#if CACHE_CONTAINERS_ENTRIES>0
	if(container_id)
		// FIXME: cast is bad! Expose a function on Context_manager
		return ((Context_Manager_Disk *)GetContextManager())->IsContainerPresent(this); // Container file deleted
	#endif


	if(!info.dual && filename.HasContent())
	{
#if CACHE_CONTAINERS_ENTRIES>0
		if(container_id != 0)
			return TRUE;
#endif //CACHE_CONTAINERS_ENTRIES>0

		BOOL exists = FALSE;
		if(cache_file)
		{
			cache_file->Exists(exists);
			return exists;
		}

		OpFile temp_file;

		if(OpStatus::IsSuccess(temp_file.Construct(filename.CStr(), folder)))
			temp_file.Exists(exists);

		return exists;
	}

	return Cache_Storage::DataPresent();
}

void File_Storage::CloseFile()
{
	OP_DELETE(cache_file);
	cache_file = NULL;
	file_count=0;
}

#ifdef URL_ENABLE_ASSOCIATED_FILES

OP_STATUS File_Storage::AssocFileName(OpString &fname, URL::AssociatedFileType type, OpFileFolder &folder_assoc, BOOL allow_storage_change)
{
	int dot;
	int bit;

	if(allow_storage_change)
	{
		if(IsEmbedded())
			RollBackEmbedding();

		OP_ASSERT(!IsEmbedded());

	#if (CACHE_SMALL_FILES_SIZE>0 || CACHE_CONTAINERS_ENTRIES>0)
		if(!IsEmbedded() && !GetContainerID())
			plain_file=TRUE;			// URL with associated files are not supposed to join container or being embedded
	#endif // (CACHE_SMALL_FILES_SIZE>0 || CACHE_CONTAINERS_ENTRIES>0)

		if(!filename.HasContent() && GetCacheType() == URL_CACHE_DISK)
		{
			RETURN_IF_ERROR(Flush());
		}
	}

	if(!filename.HasContent() || folder==OPFILE_ABSOLUTE_FOLDER) // Local file storage are special...
	{
		// Switch to temporary cache files
		Context_Manager *man=GetContextManager();

		OP_ASSERT(man);

		if(man)
		{
			OpString *file_name;

			RETURN_IF_ERROR(man->CreateTempAssociatedFileName(url, type, file_name, TRUE));

			if(file_name)
			{
				fname.Set(file_name->CStr());
				folder_assoc = man->GetCacheLocationForFiles();

				return OpStatus::OK;
			}
		}

		return OpStatus::ERR_NOT_SUPPORTED;
	}

//#ifndef SEARCH_ENGINE_CACHE
//	RETURN_IF_ERROR(fname.Set(filename));

//	if((dot = fname.FindLastOf('.')) != -1)
//		fname.Delete(dot);
//#else
	RETURN_IF_ERROR(fname.Set("assoc"));

	bit = 0;
	while ((type & 1) == 0)
	{
		++bit;
		type = (URL::AssociatedFileType)((UINT32)type >> 1);
	}

	// Filenames are like: "assoc000/g_0001/opr00001.000"
	// Format: assocTT0/g_GGGG/oprXXXXX.CCC:
	//		TTT: Type of the associated file (AssociatedFileType: Thumbnail, CompiledECMAScript... maximum allowed: 12 types...)
	//		GGGG: cache generation (the same as the main cache file)
	//		XXXXX: cache file name (the same as the main cache file)
	//      CCC: container ID (this should be enough to cover containers with till 4K of URLs)
	RETURN_IF_ERROR(fname.AppendFormat(UNI_L("%.03X%c%s"), bit, PATHSEPCHAR, filename.CStr()));

	if((dot = fname.FindLastOf('.')) > 9)  // 9 == "assocXXX/"
		fname.Delete(dot);

	RETURN_IF_ERROR(fname.AppendFormat(UNI_L(".%03X"), GetContainerID()));
//#endif

	folder_assoc = folder;

	return OpStatus::OK;
}

#endif  // URL_ENABLE_ASSOCIATED_FILES

BOOL File_Storage::Flush()
{
	OP_ASSERT(urlManager);
	OP_ASSERT(url);
	
#ifdef CACHE_STATS
	stats_flushed++;
#endif

	if(!GetFinished())
		return FALSE;
		
#if CACHE_SMALL_FILES_SIZE>0
	// If possible, add it to the index
	if(!plain_file && ManageEmbedding())
		return TRUE;
#endif

	if((filename.IsEmpty()
#ifdef SEARCH_ENGINE_CACHE
		|| info.modified
#endif
		) && !info.dual && info.completed && cache_content.GetLength() && !GetMemoryOnlyStorage())
	{
		Context_Manager *ctx=GetContextManager();

		if(ctx && !ctx->BypassStorageFlush(this))
		{
	#ifdef SEARCH_ENGINE_CACHE
			if(!info.modified)
				RETURN_VALUE_IF_ERROR(CheckFilename(), FALSE);
			else
				info.modified = FALSE;
	#else  // SEARCH_ENGINE_CACHE
				RETURN_VALUE_IF_ERROR(CheckFilename(), FALSE);
	#endif // SEARCH_ENGINE_CACHE

			// Will save it with the filename contained in this->filename
			if(Cache_Storage::SaveToFile(NULL) != 0 )
				return FALSE;
		}

		
		FlushMemory();
		info.dual = FALSE;
		info.force_save = TRUE; // FROM_EPOC
		urlManager->SetLRU_Item(url);

		return TRUE;
	}
	else if(cache_content.GetLength() && !GetMemoryOnlyStorage())
	{
		FlushMemory();
		info.dual = FALSE;
		urlManager->SetLRU_Item(url);

		return TRUE;
	}

	// Return FALSE if we didn't do anything, unless we already have
	// done something. Previously this returned TRUE but then the error
	// was lost if neither of the previous branches were taken.
	// lth / 2002-01-28 - peter / 2002-02-21
	return filename.HasContent() ? TRUE : FALSE;
}


#if defined(_DEBUG) && defined(SEARCH_ENGINE_CACHE) && !defined(_NO_GLOBALS_)
#include "modules/cache/CacheIndex.h"
extern CacheIndex *g_disk_index;
#endif

BOOL File_Storage::Purge()
{
	OP_NEW_DBG("File_Storage::Purge", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - read_only: %d - dual: %d - info.completed: %d - IsDiskContentAvailable(): %d \n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, cache_content.GetLength(), url->GetContextId(), read_only, info.dual, info.completed, IsDiskContentAvailable()));

	OP_DELETE(cache_file);
	cache_file = NULL;

#if defined(_DEBUG) && defined(SEARCH_ENGINE_CACHE) && !defined(_NO_GLOBALS_)
	OpString tmp_storage;
	const uni_char * dbg_cache_folder = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_CACHE_FOLDER, tmp_storage);
	if(g_disk_index != NULL && filename.HasContent() &&
		uni_strncmp(filename, dbg_cache_folder, uni_strlen(dbg_cache_folder)) == 0 &&
		BlockStorage::FileExists(filename, OPFILE_CACHE_FOLDER) == OpBoolean::IS_TRUE)
	{
		if(!g_disk_index->SearchFile(filename))
		{
			OpString8 ufname;
			const char *cufname;

			ufname.SetUTF8FromUTF16(filename);
			cufname = ufname.CStr();
			file = NULL;
		}
	}
#endif

	if(cachetype != URL_CACHE_DISK && cachetype != URL_CACHE_USERFILE)
	{
		if(filename.HasContent())
		{
			if(GetCacheType() == URL_CACHE_TEMP)
				SubFromCacheUsage(url, FALSE, IsDiskContentAvailable() && HasContentBeenSavedToDisk());  // Cache_Storage::Purge() only subtract the ram usage
#if CACHE_CONTAINERS_ENTRIES>0
			if(!prevent_delete) // Delete the file, unless it is in a container already deleted
#endif
			{
				OpFile f;
				if(OpStatus::IsSuccess(f.Construct(filename.CStr(), folder)))
				{
	#if defined(_DEBUG) && defined(SEARCH_ENGINE_CACHE) && !defined(_NO_GLOBALS_)
					if(g_disk_index != NULL && folder == OPFILE_CACHE_FOLDER)
					{
						OP_ASSERT(!g_disk_index->SearchFile(filename));
					}
	#endif
					// Containers (or future special cases) marked for delete
					Context_Manager *ctx=GetContextManager();

					if(!ctx->BypassStorageTruncateAndReset(this))
					{
					#if defined(_DEBUG) && CACHE_CONTAINERS_ENTRIES>0
						OP_NEW_DBG("File_Storage::Purge", "cache.del");

						if(container_id)
							OP_DBG((UNI_L("*** Cache File cont id %d in %s deleted directly (strange... it should not happen...)"), container_id, filename.CStr() ));
					#endif

					#if defined(USE_ASYNC_FILEMAN) && defined(UTIL_ASYNC_FILE_OP)
						f.DeleteAsync();
					#else
						f.Delete(FALSE);
					#endif
					}
	#ifdef SUPPORT_RIM_MDS_CACHE_INFO
					urlManager->ReportCacheItem(url,FALSE);
	#endif // SUPPORT_RIM_MDS_CACHE_INFO
				}
			}

#ifdef URL_ENABLE_ASSOCIATED_FILES
			if(purge_assoc_files) // // Delete associated files
				OpStatus::Ignore(PurgeAssociatedFiles(TRUE));
#endif // URL_ENABLE_ASSOCIATED_FILES
		}
	#ifdef URL_ENABLE_ASSOCIATED_FILES
		else  // No file name: only temporary associated files are possible
		{
			if(purge_assoc_files) // // Delete associated files
				OpStatus::Ignore(PurgeTemporaryAssociatedFiles());
		}
	#endif // URL_ENABLE_ASSOCIATED_FILES
	}
	else
	{
		SubFromCacheUsage(url, FALSE, IsDiskContentAvailable() && HasContentBeenSavedToDisk()); // Cache_Storage::Purge() only subtract the ram usage
		Flush();

		#ifdef URL_ENABLE_ASSOCIATED_FILES
			if(purge_assoc_files) // // Delete associated files
				OpStatus::Ignore(PurgeTemporaryAssociatedFiles());
		#endif // URL_ENABLE_ASSOCIATED_FILES
	}


#if defined(_DEBUG) && defined(SEARCH_ENGINE_CACHE) && !defined(_NO_GLOBALS_)
// disabled because it would also trigger on any CreateNewCache or whatever the method is called
/*
	OpString tmp_storage;
	const uni_char * dbg_cache_folder = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_CACHE_FOLDER, tmp_storage);
	if(g_disk_index != NULL && filename.HasContent() &&
		uni_strncmp(filename, dbg_cache_folder, uni_strlen(dbg_cache_folder)) == 0 &&
		BlockStorage::FileExists(filename, OPFILE_CACHE_FOLDER) == OpBoolean::IS_TRUE)
	{
		OP_ASSERT(g_disk_index->SearchFile(filename));
	}*/
#endif
	filename.Empty();
	// WOFF fonts (that use associated files) can detach the cache storage from the cache, with the result that
	// FlushMemory() does not execute because it checks that the storage is inside a list.
	// Probably other operations, like view source, can end-up in the same situation.
	// In this case VerifyURLSizeNotCounted() cannot be called to verify the imbalance, as it is imbalanced by definition.
	if(Cache_Storage::Purge())
	{
#ifdef SELFTEST
		Cache_Storage *cs=(url && url->GetDataStorage() && url->GetDataStorage()->GetCacheStorage()) ? url->GetDataStorage()->GetCacheStorage() : NULL;

		// In some cases, for example during Download_Storage::SetFinished(), a temporary cache storage is deleted,
		// and this check is not valid, as the URL has already been associated to a different storage
		if(!cs || cs==this)
			GetContextManager()->VerifyURLSizeNotCounted(url, TRUE, TRUE);
#endif // SELFTEST
	}

	OP_ASSERT(stored_size == 0);

	return TRUE;
}

#ifdef URL_ENABLE_ASSOCIATED_FILES
OP_STATUS File_Storage::PurgeAssociatedFiles(BOOL also_temporary)
{
	UINT32 i;
	OpString afname;
	OpFile f;
	OP_STATUS ops=OpStatus::OK;
	OP_STATUS ops_t=OpStatus::OK;

	for (i = 1; i != 0; i <<= 1)
	{
		if(OpStatus::IsSuccess(ops) && !OpStatus::IsSuccess(ops_t))
			ops=ops_t;

		ops_t=OpStatus::OK;

		if(i & purge_assoc_files) // Select only the files to purge
		{
			OpFileFolder folder_assoc;

			if(!OpStatus::IsSuccess(ops_t=AssocFileName(afname, (URL::AssociatedFileType)i, folder_assoc, FALSE)))
				continue;

			if(OpStatus::IsSuccess(ops_t=f.Construct(afname, folder_assoc)))
			{
				OP_NEW_DBG("File_Storage::Purge", "cache.del");
				OP_DBG((UNI_L("*** Cache Associated File %s deleted"), afname.CStr() ));

#if defined(USE_ASYNC_FILEMAN) && defined(UTIL_ASYNC_FILE_OP)
				f.DeleteAsync();
#else
				f.Delete(FALSE);
#endif
#ifdef SUPPORT_RIM_MDS_CACHE_INFO
				urlManager->ReportCacheItem(url,FALSE);
#endif // SUPPORT_RIM_MDS_CACHE_INFO
			}
		}
	}

	if(also_temporary)
		ops_t=PurgeTemporaryAssociatedFiles();

	if(OpStatus::IsSuccess(ops) && !OpStatus::IsSuccess(ops_t))
		ops=ops_t;

	return ops;
}

OP_STATUS File_Storage::PurgeTemporaryAssociatedFiles()
{
	if(!purge_assoc_files)
		return OpStatus::OK;

	UINT32 i;
	OpString afname;
	OpFile f;
	OP_STATUS ops=OpStatus::OK;
	OP_STATUS ops_t=OpStatus::OK;
	Context_Manager *man=GetContextManager();

	OP_ASSERT(man);

	if(!man)
		return OpStatus::ERR_NULL_POINTER;

	OpFileFolder folder_assoc = man->GetCacheLocationForFiles();

	for (i = 1; i != 0; i <<= 1)
	{
		if(OpStatus::IsSuccess(ops) && !OpStatus::IsSuccess(ops_t))
			ops=ops_t;

		if(i & purge_assoc_files) // Select only the files to purge
		{
			OpString *name=man->GetTempAssociatedFileName(url, (URL::AssociatedFileType)i);

			if(name)  // Temporary file actually existing
			{
				OpFile f;

				if(OpStatus::IsSuccess(ops_t=f.Construct(name->CStr(), folder_assoc)))
				{
				#if defined(USE_ASYNC_FILEMAN) && defined(UTIL_ASYNC_FILE_OP)
					f.DeleteAsync();
				#else
					f.Delete(FALSE);
				#endif

				#ifdef SUPPORT_RIM_MDS_CACHE_INFO
					urlManager->ReportCacheItem(url,FALSE);
				#endif // SUPPORT_RIM_MDS_CACHE_INFO
				}
			}
				
		}
	}

	return ops;
}
#endif // URL_ENABLE_ASSOCIATED_FILES

void File_Storage::TruncateAndReset()
{
	OP_NEW_DBG("File_Storage::TruncateAndReset", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - read_only: %d - dual: %d - completed: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, cache_content.GetLength(), url->GetContextId(), read_only, info.dual, info.completed));

	OP_DELETE(cache_file);
	cache_file = NULL;

	if(filename.HasContent())
	{
		if(GetCacheType() == URL_CACHE_DISK || GetCacheType() == URL_CACHE_TEMP)
		{
			//PrintfTofile("cs.txt", "[%s]\tpurge\t%d\t", filename, info.unsetsize);
			if(!content_size)
				content_size = FileLength();
		}
		Context_Manager *ctx=GetContextManager();

		if(!ctx->BypassStorageTruncateAndReset(this))
		{
			OpFile f;
			if(OpStatus::IsSuccess(f.Construct(filename.CStr(), folder) ))
			{
					OP_NEW_DBG("File_Storage::TruncateAndReset()", "cache.del");
					OP_DBG((UNI_L("*** Cache File %s deleted - Folder id: %d - Context id: %d"), filename.CStr(), folder, Url()->GetContextId() ));
				
					f.Delete(FALSE);
	#ifdef SUPPORT_RIM_MDS_CACHE_INFO
				urlManager->ReportCacheItem(url,FALSE);
	#endif // SUPPORT_RIM_MDS_CACHE_INFO
			}
		}
	}

	Cache_Storage::TruncateAndReset();
	ResetForLoading();
	content_size = 0;
	OP_ASSERT(stored_size==0);
	stored_size = 0;
}

OpFileLength File_Storage::ContentLoaded(BOOL force /*=FALSE*/)
{
	if(!force && (info.dual || filename.IsEmpty() || content_size))
	{
		if(content_size && !info.dual)
			return content_size;
		else
			return cache_content.GetLength();
	}
	else
		return FileLength();
}

void File_Storage::SetMemoryOnlyStorage(BOOL flag)
{
	info.memory_only_storage = (flag ? TRUE : FALSE);
}

BOOL File_Storage::GetMemoryOnlyStorage()
{
	return (BOOL) info.memory_only_storage;
}


/*
BOOL File_Storage::CacheFile()
{
// true if this is a part of the cache ; Default FALSE
	return FALSE;
}

BOOL File_Storage::Persistent()
{
// Only used by cache files. True if it is to be preserved for the next session; DEFAULT TRUE
	return TRUE;
}
*/

#ifdef _OPERA_DEBUG_DOC_
void File_Storage::GetMemUsed(DebugUrlMemory &debug)
{
	debug.memory_loaded += sizeof(*this) - sizeof(Cache_Storage);
	debug.memory_loaded += UNICODE_SIZE(filename.Length()+1);

	Cache_Storage::GetMemUsed(debug);
}
#endif

#ifdef CACHE_FAST_INDEX
	SimpleStreamReader *File_Storage::CreateStreamReader()
	{
		SimpleStreamReader *reader=Cache_Storage::CreateStreamReader();

		if(NULL!=reader)
			return reader;

		// StreamBuffer from the file
		SimpleFileReader *sfr=OP_NEW(SimpleFileReader, ());

		if(!sfr)
			return NULL;

		if(OpStatus::IsError(sfr->Construct(filename.CStr(), folder)))
		{
			OP_DELETE(sfr);

			return NULL;
		}

	#if CACHE_CONTAINERS_ENTRIES>0
		if(container_id)
		{
			// TODO: It would be very nice to instruct the stream to give an error after the known size (size)
			// Get the beginning of the file
			UINT32 ch=sfr->Read8();

			if(ch==CACHE_CONTAINERS_BYTE_SIGN) // Opera container file: reads the header
			{
				UINT32 num=sfr->Read8();  // Number of containers
				UINT32 offset=0;
#ifdef DEBUG_ENABLE_OPASSERT
				UINT32 size=0;
#endif
				BOOL found=FALSE;

				for(UINT32 i=0; i<num; i++)  // Read the entries
				{
					UINT32 cnt_id=sfr->Read8();
					UINT32 cnt_size=sfr->Read16();

					if(cnt_id==container_id)
					{
						OP_ASSERT(!size);

						found=TRUE;
#ifdef DEBUG_ENABLE_OPASSERT
						size=cnt_size;
#endif
					}
					else if(!found)
						offset+=cnt_size;
				}

				if(!offset || OpStatus::IsSuccess(sfr->Skip(offset)))  // Position the stream on the correct point
					return sfr;
			}

			OP_DELETE(sfr);

			return NULL;
		}
	#endif

	return sfr;
	}
#endif


