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

#include "modules/encodings/utility/charsetnames.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/util/opfile/opfile.h"

#include "modules/url/url2.h"
#include "modules/url/url_ds.h"

#include "modules/url/url_rep.h"
#include "modules/url/url_man.h"
#include "modules/cache/url_cs.h"
#include "modules/cache/cache_int.h"
#include "modules/cache/multimedia_cache.h"

#ifdef PI_ASYNC_FILE_OP
#include "modules/cache/cache_exporter.h"
#endif // PI_ASYNC_FILE_OP

#include "modules/olddebug/timing.h"

#if CACHE_SMALL_FILES_SIZE>0
	#include "modules/cache/simple_stream.h"
#endif

#if CACHE_CONTAINERS_ENTRIES>0
	#include "modules/cache/cache_container.h"
#endif

#ifdef _DEBUG
#ifdef YNP_WORK
#include "modules/olddebug/tstdump.h"
#include "modules/url/tools/url_debug.h"
#define _DEBUG_DD
#define _DEBUG_DD1
#endif
#endif

/// Create a read only file descriptor based on a buffer; it's a simplified implementation
class ReadOnlyBufferFileDescriptor: public OpFileDescriptor
{
private:
	/// Buffer that contains the data; it is owned by the creator, not by this object
	UINT8 *buf;
	/// Size of the buffer
	UINT32 size;
	/// Current position
	UINT32 pos;

public:
	/**
		Constructor

		@param buffer Buffer that contains the data. The buffer is not copied, so it cannot be deleted while this object is alive
		@param length Length of the buffer
	*/
	ReadOnlyBufferFileDescriptor(void *buffer, UINT32 length) { OP_ASSERT(buffer); buf=(UINT8 *)buffer; size=length; pos=0; }
	virtual OpFileType Type() const {
		return OPFILE;
	}

	// The following methods are equivalent to those in OpLowLevelFile.
	// See modules/pi/system/OpLowLevelFile.h for further documentation.

	virtual BOOL IsWritable() const { return FALSE; }
	virtual OP_STATUS Open(int mode) { if(mode!=OPFILE_READ) return OpStatus::ERR_NOT_SUPPORTED; return OpStatus::OK; }
	virtual BOOL IsOpen() const { return TRUE; }
	virtual OP_STATUS Close() { return OpStatus::OK; }
	virtual BOOL Eof() const { return pos>=size; }
	virtual OP_STATUS Exists(BOOL& exists) const { exists=TRUE; return OpStatus::OK; }
	virtual OP_STATUS GetFilePos(OpFileLength& position) const { position=pos; return OpStatus::OK; };
	virtual OP_STATUS SetFilePos(OpFileLength position, OpSeekMode seek_mode=SEEK_FROM_START) { OP_ASSERT(position<size); OP_ASSERT(seek_mode==SEEK_FROM_START); if(seek_mode!=SEEK_FROM_START) return OpStatus::ERR_NOT_SUPPORTED; pos=(UINT32)position; return OpStatus::OK; } ;
	virtual OP_STATUS GetFileLength(OpFileLength& length) const { length=size; return OpStatus::OK; }
	virtual OP_STATUS Write(const void* data, OpFileLength len) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read=NULL)
	{
		if(bytes_read)
			*bytes_read=0;

		if(pos==size)
			return OpStatus::OK;

		if(pos>size)
			return OpStatus::ERR_OUT_OF_RANGE;

		UINT32 bytes=(UINT32) (pos+len>size?size-pos:len);

		op_memcpy(data, buf+pos, bytes);

		pos+=bytes;

		if(bytes_read)
			*bytes_read=bytes;

		return OpStatus::OK;
	}
	virtual OP_STATUS ReadLine(OpString8& str) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OpFileDescriptor* CreateCopy() const { return OP_NEW(ReadOnlyBufferFileDescriptor, (buf, size)); }
};

unsigned long GetFileError(OP_STATUS op_err, URL_Rep *url, const uni_char *operation);
OpFileDescriptor* Cache_Storage::CreateAndOpenFile(const OpStringC &filename, OpFileFolder folder, OpFileOpenMode mode, OP_STATUS &op_err, int flags)
{
	op_err = OpStatus::OK;
	if(filename.HasContent())
	{
									__DO_START_TIMING(TIMING_FILE_OPERATION);
		CacheFile *fil = OP_NEW(CacheFile, ());
		if(!fil)
			op_err = OpStatus::ERR_NO_MEMORY;

		if(OpStatus::IsSuccess(op_err))
		{
			op_err = fil->Construct(filename.CStr(), folder, flags);
			if(OpStatus::IsSuccess(op_err))
			{
				op_err = fil->Open(mode);
			}
		}
									__DO_STOP_TIMING(TIMING_FILE_OPERATION);
		if(OpStatus::IsError(op_err))
		{
			OP_DELETE(fil);
			fil = NULL;
			//url->HandleError(GetFileError(op_err, url,UNI_L("open")));
		}
		return fil;
	}
	else
	{
		op_err=OpStatus::ERR_OUT_OF_RANGE;
		
		return NULL;
	}
}

Cache_Storage::Cache_Storage(URL_Rep *url_rep)
{
	InternalInit(url_rep, NULL);
}

Cache_Storage::Cache_Storage(Cache_Storage *old)
{
	InternalInit(NULL, old);
}

void Cache_Storage::InternalInit(URL_Rep *url_rep, Cache_Storage *old)
{
	OP_ASSERT((url_rep && !old) || (!url_rep && old));
	
	storage_id = g_url_storage_id_counter++;
	content_size = 0;
	read_only = FALSE;
	save_position=FILE_LENGTH_NONE;
	in_setfinished = FALSE;

	if(!url_rep && old)
		url = old->url;
	else
		url = url_rep;

	OP_ASSERT(url);

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	never_flush = FALSE;
#endif
	http_response_code = (unsigned short) url->GetAttribute(URL::KHTTP_Response_Code);
	content_type = (URLContentType) url->GetAttribute(URL::KContentType);
	charset_id = (unsigned short) url->GetAttribute(URL::KMIME_CharSetId);
	g_charsetManager->IncrementCharsetIDReference(charset_id);

	OP_MEMORY_VAR URLType type= (URLType) url_rep->GetAttribute(URL::KType);
	
	cache_content.SetIsSensitive(type == URL_HTTPS ? TRUE : FALSE);

#ifdef CACHE_ENABLE_LOCAL_ZIP
	encode_storage = NULL;
#endif
#ifdef CACHE_STATS
	retrieved_from_disk=FALSE; 
	stats_flushed=0;
#endif

#if CACHE_SMALL_FILES_SIZE>0
	embedded=FALSE;
#endif
#if CACHE_CONTAINERS_ENTRIES>0
	container_id=0;
	prevent_delete=FALSE;
	#ifdef SELFTEST
		checking_container=FALSE;
	#endif
#endif
#ifdef SELFTEST
	bypass_asserts=FALSE;
	debug_simulate_store_error=FALSE;
	num_disk_read=0;
#endif
#if (CACHE_SMALL_FILES_SIZE>0 || CACHE_CONTAINERS_ENTRIES>0)
	plain_file=FALSE;
#endif

	if(old)
	{
		url = old->url;
		old->cache_content.ResetRead();
		TRAPD(op_err, cache_content.AddContentL(&old->cache_content));
		OpStatus::Ignore(op_err); // FIXME: report errors;
		content_size= old->content_size;
		read_only = old->read_only;
		
		
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		never_flush = old->never_flush;
#endif
		
		URL_DataDescriptor *item;
		
		while((item = old->First()) != NULL)
		{
			item->Out();
			item->Into(this);
			item->SetStorage(this);
		}
	}
}

Cache_Storage::~Cache_Storage()
{
	InternalDestruct();
}

void Cache_Storage::TruncateAndReset()
{
	OP_NEW_DBG("Cache_Storage::TruncateAndReset", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - read_only: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, cache_content.GetLength(), url->GetContextId(), read_only));

#if CACHE_SMALL_FILES_SIZE>0
	if(IsEmbedded())
	{
		urlManager->DecEmbeddedSize(cache_content.GetLength());
		embedded=FALSE;
	}
#endif

	ResetForLoading();
	content_encoding.Empty();
}

void Cache_Storage::InternalDestruct()
{
	SubFromCacheUsage(url, IsRAMContentComplete(), IsDiskContentAvailable() && HasContentBeenSavedToDisk());

#ifdef SELFTEST
	GetContextManager()->VerifyURLSizeNotCounted(url, TRUE, TRUE);
#endif // SELFTEST

	url = NULL;
	content_size= 0;
	read_only = FALSE;
	g_charsetManager->DecrementCharsetIDReference(charset_id);

#ifdef CACHE_ENABLE_LOCAL_ZIP
	OP_DELETE(encode_storage);
	encode_storage = NULL;
#endif

	URL_DataDescriptor *desc;
	
	while((desc = First()) != NULL)
	{
		desc->SetStorage(NULL);
		desc->Out();
	}
}

BOOL Cache_Storage::Purge()
{
	return FlushMemory();
}

#if CACHE_SMALL_FILES_SIZE>0
BOOL Cache_Storage::ManageEmbedding()
{
	// "Plain" files are not embedded
	if(plain_file)
		return FALSE;
		
	// Small files are kept in memory and stored in the index
	if(IsEmbedded())
		return TRUE;
		
	// Check if it can be embedded
	if(IsEmbeddable())
	{
		UINT32 size=cache_content.GetLength();
		
	#ifdef CACHE_SMALL_FILES_LIMIT
		OP_ASSERT(CACHE_SMALL_FILES_LIMIT>0);  // Instead of putting a limit of 0, disable CACHE_SMALL_FILES_SIZE
		
		if(urlManager->GetEmbeddedSize()+size<=CACHE_SMALL_FILES_LIMIT)
	#else
		// This combination is not supposed to be enabled, because memory can grow too much...
		// An high limit should be fine
	
		OP_ASSERT(FALSE);
	#endif
		{
			embedded=TRUE;
			urlManager->IncEmbeddedSize(size);
		
			return TRUE;
		}
	}
	
	return FALSE;
}
void Cache_Storage::RollBackEmbedding()
{
	OP_ASSERT(IsEmbedded());

	if(IsEmbedded())
	{
		urlManager->DecEmbeddedSize(cache_content.GetLength());
		embedded=FALSE;
	}
}
#endif // CACHE_SMALL_FILES_SIZE

void Cache_Storage::SetCharsetID(unsigned short id)
{
	g_charsetManager->IncrementCharsetIDReference(id);
	g_charsetManager->DecrementCharsetIDReference(charset_id);
	charset_id=id;
}

void Cache_Storage::AddToDiskOrOEMSize(OpFileLength size_to_add, URL_Rep *url, BOOL add_url_size_estimantion)
{
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	if(never_flush)
		urlManager->GetMainContext()->AddToOCacheSize(size_to_add, url, add_url_size_estimantion);
#ifndef RAMCACHE_ONLY
		else
#endif // RAMCACHE_ONLY
#endif // defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
#ifndef RAMCACHE_ONLY
		GetContextManager()->AddToCacheSize(size_to_add, url, add_url_size_estimantion);
#endif // RAMCACHE_ONLY

	SetContentAlreadyStoredOnDisk(GetContentAlreadyStoredOnDisk() + size_to_add);
	SetDiskContentComputed(TRUE);
}

void Cache_Storage::SubFromDiskOrOEMSize(OpFileLength size_to_sub, URL_Rep *url, BOOL enable_checks)
{
	#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		if(never_flush)
			urlManager->GetMainContext()->SubFromOCacheSize(size_to_sub, url, enable_checks);
	#ifndef RAMCACHE_ONLY
			else
	#endif // RAMCACHE_ONLY
#endif // defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
#ifndef RAMCACHE_ONLY
		GetContextManager()->SubFromCacheSize(size_to_sub, url, enable_checks);
#endif // RAMCACHE_ONLY

		SetContentAlreadyStoredOnDisk(GetContentAlreadyStoredOnDisk() - size_to_sub);
}

void Cache_Storage::AddToCacheUsage(URL_Rep *url, BOOL ram, BOOL disk_oem, BOOL add_url_size_estimantion)
{
	OP_NEW_DBG("Cache_Storage::AddToCacheUsage()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - ram: %d - disk_oem: %d - add_url_size_estimantion:%d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, cache_content.GetLength(), url->GetContextId(), ram, disk_oem, add_url_size_estimantion));

	if(ram)
	{
		if(!GetMemoryOnlyStorage())
			GetContextManager()->AddToRamCacheSize(cache_content.GetLength(), url, add_url_size_estimantion);
	}

	if(disk_oem && (content_size || add_url_size_estimantion))
		AddToDiskOrOEMSize(content_size, url, add_url_size_estimantion);
}

void Cache_Storage::SubFromCacheUsage(URL_Rep *url, BOOL ram, BOOL disk_oem)
{
	OP_NEW_DBG("Cache_Storage::SubFromCacheUsage()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - ram: %d - disk_oem: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, cache_content.GetLength(), url->GetContextId(), ram, disk_oem));

	if(ram)
	{
		if(!GetMemoryOnlyStorage())
			GetContextManager()->SubFromRamCacheSize(cache_content.GetLength(), url);
	}

	if(disk_oem && (GetContentAlreadyStoredOnDisk() || GetDiskContentComputed()))
	{
		// GetContentAlreadyStoredOnDisk() should be set only if GetDiskContentComputed() is TRUE
		OP_ASSERT(GetDiskContentComputed());

		SubFromDiskOrOEMSize(GetContentAlreadyStoredOnDisk(), url, TRUE);

		OP_ASSERT(GetContentAlreadyStoredOnDisk()==0);
		SetDiskContentComputed(FALSE);
	}

#ifdef SELFTEST
	GetContextManager()->VerifyURLSizeNotCounted(url, ram, disk_oem);
#endif // SELFTEST
}

void Cache_Storage::SetFinished(BOOL)
{
	OP_NEW_DBG("Cache_Storage::SetFinished()", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - read_only: %d ==> TRUE - IsDiskContentAvailable: %d - HasContentBeenSavedToDisk: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, cache_content.GetLength(), url->GetContextId(), read_only, IsDiskContentAvailable(), HasContentBeenSavedToDisk()));

	if (in_setfinished)
	{
		// Already in setfinished so we can't do anything here
		OP_ASSERT(1==0 && !"SetFinished entered recursively!");
		return;
	}
	in_setfinished = TRUE;

	if(!read_only)
	{
	#ifdef CACHE_ENABLE_LOCAL_ZIP
		if(encode_storage)
		{
			OpStatus::Ignore(encode_storage->FinishStorage(this)); // FIXME
			OP_DELETE(encode_storage);
			encode_storage = NULL;
		}
	#endif

		TRAPD(op_err, cache_content.FinishedAddingL());
		OpStatus::Ignore(op_err); // FIXME
		content_size += cache_content.GetLength();
	}

#ifdef SELFTEST
	GetContextManager()->VerifyURLSizeNotCounted(url, !IsRAMContentComplete(), FALSE /* Content on disk can already have been written during StoreData() */);
#endif // SELFTEST

	AddToCacheUsage(url, !IsRAMContentComplete(), IsDiskContentAvailable() && !HasContentBeenSavedToDisk(), TRUE);

	read_only = TRUE;
	in_setfinished = FALSE;
}

void Cache_Storage::UnsetFinished()
{
#ifdef SELFTEST
	if(!read_only)
		GetContextManager()->VerifyURLSizeNotCounted(url, TRUE, FALSE);
#endif // SELFTEST

	if(!read_only)
		return;

	SubFromCacheUsage(url, TRUE, FALSE);
	read_only = FALSE;
}

OP_STATUS	Cache_Storage::StoreDataEncode(const unsigned char *buffer, unsigned long buf_len)
{
#ifdef CACHE_ENABLE_LOCAL_ZIP
	if(encode_storage)
		return encode_storage->StoreData(this, buffer, buf_len);
#endif

	return StoreData(buffer, buf_len);
}

OP_STATUS Cache_Storage::StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position)
{
	OP_NEW_DBG("Cache_Storage::StoreData()", "cache.storage");
	OP_DBG((UNI_L("URL: %s - %x - Len: %d - URL points to me: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (int) buf_len, url->GetDataStorage() && url->GetDataStorage()->GetCacheStorage()==this));

	if(read_only || buffer == NULL || buf_len == 0)
		return OpStatus::OK;

	OP_STATUS op_err = OpStatus::OK;
	
#if CACHE_SMALL_FILES_SIZE>0
	OP_ASSERT(CACHE_SMALL_FILES_SIZE<=4*FL_MAX_SMALL_PAYLOAD);
#endif

	// that version will not assert on what we are doing
	if(cache_content.GetLength() == 0)
	{
		OpFileLength content_len=0;
		
		url->GetAttribute(URL::KContentSize, &content_len);
		if(content_len && content_len <= 4*FL_MAX_SMALL_PAYLOAD)
		{
			// Preallocate the expected length of content
			// As we have not specified fixed length or direct access
			// the object will naturally convert to large payload.
			TRAP(op_err, cache_content.ResizeL((uint32) content_len, TRUE));
		}
	}
	
	if(OpStatus::IsSuccess(op_err))
		TRAP(op_err, cache_content.AddContentL(buffer, buf_len));

#ifdef SELFTEST
	if(debug_simulate_store_error)
		op_err=OpStatus::ERR_NO_MEMORY;
#endif

	if(OpStatus::IsError(op_err))
	{
		if(OpStatus::IsMemoryError(op_err))
		{
			BOOL unsafe=url->GetDataStorage() && url->GetDataStorage()->GetCacheStorage() && url->GetDataStorage()->GetCacheStorage()->CircularReferenceDetected(this);

			url->HandleError(ERR_SSL_ERROR_HANDLED);
			g_memory_manager->RaiseCondition( op_err );

			if(unsafe)
				return op_err;
		}
		TRAP(op_err, cache_content.AddContentL(buffer, buf_len));
		if(OpStatus::IsError(op_err))
		{
			url->HandleError(ERR_SSL_ERROR_HANDLED);
			if(OpStatus::IsMemoryError(op_err))
				g_memory_manager->RaiseCondition( op_err );
		}
	}

	//OP_NEW_DBG("Cache_Storage::StoreData()", "cache.storage");
	OP_DBG((UNI_L("URL: %s - %x - bytes stored: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, buf_len));

	return op_err;
}

unsigned long Cache_Storage::RetrieveData(URL_DataDescriptor *desc,BOOL &more)
{
	OP_NEW_DBG("Cache_Storage::RetrieveData()", "cache.storage");
	OP_DBG((UNI_L("URL: %s - %x"), url->GetAttribute(URL::KUniName).CStr(), url));

	more = FALSE;
	if(	desc == NULL || !desc->RoomForContent() || cache_content.GetLength() == 0)
	{
		if(desc && cache_content.GetLength() > 0)
			more = TRUE;
		return (desc ? desc->GetBufSize() : 0);
	}

	cache_content.SetReadPos((uint32) desc->GetPosition()+desc->GetBufSize());

	// CORE-27718: optimization to reduce the allocation of memory when the content length is available
	if(!desc->GetBuffer())
	{
		OpFileLength content_len=0;

		url->GetAttribute(URL::KContentSize, &content_len);

		if(content_len)
			desc->Grow((unsigned long)content_len, URL_DataDescriptor::GROW_SMALL);
	}

	TRAPD(err, desc->AddContentL(&cache_content, NULL, 0));
	if(OpStatus::IsMemoryError(err))
	{
		g_memory_manager->RaiseCondition( OpStatus::ERR_NO_MEMORY );
		return (desc ? desc->GetBufSize() : 0);
	}

	if(cache_content.MoreData())
	{
		if(!desc->PostedMessage())
			desc->PostMessage(MSG_URL_DATA_LOADED, url->GetID(), 0);
		more = TRUE;
	}
	else
	{
		/**
			A multipart URL has a storage of type Multipart_CacheStorage,
			which when iterating over the parts creates a Cache_Storage for
			each. So, in that situation, GetIsMultipart() is FALSE but
			url->GetDataStorage()->GetCacheStorage()->GetIsMultipart() is TRUE.
			In that case the message cannot be posted because that is handled
			by the Multipart_CacheStorage.
		*/
		Cache_Storage *cs = url->GetDataStorage()->GetCacheStorage();
		if (!cs || !cs->GetIsMultipart())
			if (desc->GetBufSize() > 0 && !desc->PostedMessage())
				desc->PostMessage(MSG_URL_DATA_LOADED, url->GetID(), 0);

		//OP_ASSERT(desc->GetPosition()+desc->GetBufSize() == cache_content.GetLength() &&(!GetFinished() || (URLStatus) url->GetAttribute(URL::KLoadStatus) != URL_LOADING));
		if(!GetFinished() && (URLStatus) url->GetAttribute(URL::KLoadStatus) == URL_LOADING && desc->HasMessageHandler())
			more = TRUE;
	}

	OP_DBG((UNI_L("URL: %s - more: %d\n"), url->GetAttribute(URL::KUniName).CStr(), more));

#ifdef _DEBUG_DD1
	PrintfTofile("urldd1.txt","\nCS Retrieve Data2- %s - %u:%u %s - %s - %s\n",
		DebugGetURLstring(url),
		cache_content.GetReadPos(), cache_content.GetLength(),		
		(url->GetAttribute(URL::KLoadStatus) == URL_LOADING ? "Not Loaded" : "Loaded"),
		(GetFinished() ? "Finished" : "Not Finished"),
		(desc->HasMessageHandler() ? "MH" : "not MH")
				);
#endif

	return desc->GetBufSize();
}

#ifdef UNUSED_CODE
BOOL Cache_Storage::OOMForceFlush()
{
	SetFinished();
	if(!Flush())
		return FALSE;
	UnsetFinished();
	return TRUE;
}
#endif

#ifdef CACHE_RESOURCE_USED
OpFileLength Cache_Storage::ResourcesUsed(ResourceID resource)
{
	return (resource == MEMORY_RESOURCE ? cache_content.GetLength() : 0);
}
#endif

URL_DataDescriptor *Cache_Storage::GetDescriptor(MessageHandler *mh, BOOL get_raw_data, BOOL get_decoded_data, Window *window, 
												 URLContentType override_content_type, unsigned short override_charset_id, BOOL get_original_content, unsigned short parent_charset_id)
{
	URL_DataDescriptor * OP_MEMORY_VAR desc;

	if(override_content_type == URL_UNDETERMINED_CONTENT)
		override_content_type = content_type;
	if(override_charset_id == 0)
		override_charset_id = charset_id;

	desc = OP_NEW(URL_DataDescriptor, (url, mh, override_content_type, override_charset_id, parent_charset_id));
	if(desc == NULL)
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	else if(OpStatus::IsError(desc->Init(get_raw_data, window)))
	{
		OP_DELETE(desc);
		desc = NULL;
	}

	if(desc)
	{
#ifdef _HTTP_COMPRESS
		//OpStringS8 *encoding = url->GetHTTPEncoding();
	
		if(content_encoding.Length() && get_decoded_data
#ifdef _MIME_SUPPORT_
			&& !IsProcessed()
#endif // _MIME_SUPPORT_
			)
		{
			TRAPD(op_err,desc->SetupContentDecodingL(content_encoding.CStr()));
			
			if(OpStatus::IsError(op_err))
			{
				OP_DELETE(desc);
				desc = NULL;
				
				return NULL;
			}
		}
#endif // _HTTP_COMPRESS
		
		AddDescriptor(desc);
	}

	return desc;
}

void Cache_Storage::AddDescriptor(URL_DataDescriptor *desc)
{
	desc->SetStorage(this);
	desc->Into(this);
}

void Cache_Storage::CloseFile()
{
	// Nothing to do in this case
	// To be overloaded by storages that really use a file
}

OpFileDescriptor* Cache_Storage::AccessReadOnly(OP_STATUS &op_err)
{
	cache_content.SetNeedDirectAccess(TRUE);

	OpFileDescriptor *fd=OP_NEW(ReadOnlyBufferFileDescriptor, (cache_content.GetDirectPayload(), cache_content.GetLength()));

	op_err=fd ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;

	return fd;
}

OP_STATUS Cache_Storage::CheckExportAsFile(BOOL allow_embedded_and_containers)
{
	// These checks are a bit too much restrictive, but basically this feature is used to speed up the saving
	// of big images and video files (video tag)
	// Check CORE-31730 and DSK-292617
	if( (URLStatus)url->GetAttribute(URL::KLoadStatus) != URL_LOADED ||
	    !GetFinished() || GetIsMultipart())
		return OpStatus::ERR_NOT_SUPPORTED;

	URLType type=(URLType)url->GetAttribute(URL::KType);

	if( type != URL_HTTP && type!=URL_HTTPS)
		return OpStatus::ERR_NOT_SUPPORTED;

#ifndef RAMCACHE_ONLY
	if(url->GetAttribute(URL::KCachePolicy_NoStore))
		return OpStatus::ERR_NOT_SUPPORTED;
#endif

#ifdef MULTIMEDIA_CACHE_SUPPORT
	// Fails for incomplete Multimedia files
	OP_ASSERT(IsExportAllowed());

	if(!IsExportAllowed())
		return OpStatus::ERR_NOT_SUPPORTED;
#endif

// Embedded files are kept in RAM
	if(!allow_embedded_and_containers && IsEmbedded())
		return OpStatus::ERR_NOT_SUPPORTED;

// Container files are not readable
	if(!allow_embedded_and_containers && GetContainerID()>0)
		return OpStatus::ERR_NOT_SUPPORTED;

	//OpFileFolder folder;
	//OpStringC cache_file=FileName(folder);

	//// If the file name is not yet available, a part of the saving will be synchronous

	//if(!allow_embedded_and_containers && cache_file.IsEmpty())
		//return OpStatus::ERR_NOT_SUPPORTED;

	return OpStatus::OK;
}

OpFile *Cache_Storage::OpenCacheFileForExportAsReadOnly(OP_STATUS &ops)
{
	OpFile *file=NULL;

	// Storage object without name are not supposed to be saved with this function
	OpFileFolder folder;
	OpStringC name=FileName(folder);

	if(!name.HasContent())
		ops=OpStatus::ERR_NOT_SUPPORTED;
	else
	{
		// Open the file as read only
		file=OP_NEW(OpFile, ());

		if(!file)
			ops=OpStatus::ERR_NO_MEMORY;
		else if(OpStatus::IsSuccess(ops=file->Construct(name, folder)))
			ops=file->Open(OPFILE_READ);

		if(OpStatus::IsError(ops))
		{
			OP_DELETE(file);
			file=NULL;
		}
	}

	return file;
}

OP_STATUS Cache_Storage::ExportAsFile(const OpStringC &file_name)
{
	OP_STATUS ops=OpStatus::OK;
	OpFileDescriptor *desc=NULL;

	RETURN_IF_ERROR(CheckExportAsFile(TRUE)); // Allow embedded files and containers

	// Check if the Context Manager bypass the save
	if(GetContextManager()->BypassStorageSave(this, file_name, ops))
		return ops;

	desc=AccessReadOnly(ops);

	// Copy of the file
	OpFile file;

	if(	OpStatus::IsSuccess(ops) &&
		OpStatus::IsSuccess(ops=file.Construct(file_name)) &&
		desc &&
		OpStatus::IsSuccess(ops=file.Open(OPFILE_WRITE)))
	{
		UINT8 buf[STREAM_BUF_SIZE];   // ARRAY OK 2010-08-09 lucav
		OpFileLength bytes_read=0;
		OpFileLength total_bytes=0;

		while(	OpStatus::IsSuccess(ops) &&
				OpStatus::IsSuccess(ops=desc->Read(buf, STREAM_BUF_SIZE, &bytes_read)) &&
				bytes_read>0)
		{
			ops=file.Write(buf, bytes_read);

			if(bytes_read>0 && bytes_read!=FILE_LENGTH_NONE)
				total_bytes+=bytes_read;
		}

#ifdef SELFTEST
		OpFileLength len=0;

		desc->GetFileLength(len);

		OP_ASSERT(total_bytes==len);
#endif

		desc->Close();
	}

#if (CACHE_SMALL_FILES_SIZE>0 || CACHE_CONTAINERS_ENTRIES>0)
	if(embedded)
		cache_content.SetNeedDirectAccess(FALSE);
#endif

	OP_DELETE(desc);

	return ops;
}

#ifdef PI_ASYNC_FILE_OP
OP_STATUS Cache_Storage::ExportAsFileAsync(const OpStringC &file_name, AsyncExporterListener *listener, UINT32 param, BOOL delete_if_error, BOOL safe_fall_back)
{
	OP_ASSERT(listener);

	if(!listener)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS ops=CheckExportAsFile(FALSE);  // Embedded files and container need to be managed in the "safe" way
	BOOL safe=FALSE;

	if(OpStatus::IsError(ops))
	{
		if(!safe_fall_back)
			return ops;
		else
			safe=TRUE;
	}

	CacheAsyncExporter *exporter=OP_NEW(CacheAsyncExporter, (listener, url, safe, delete_if_error, param));

	RETURN_OOM_IF_NULL(exporter);

	// The object will delete itself when appropriate
	ops=exporter->StartExport(file_name);

	if(OpStatus::IsError(ops))
		OP_DELETE(exporter);

	return ops;
}
#endif // PI_ASYNC_FILE_OP

OpFileDescriptor *Cache_Storage::OpenFile(OpFileOpenMode mode, OP_STATUS &op_err, int flags)
{
									__DO_START_TIMING(TIMING_FILE_OPERATION);
	OpFileFolder folder1 = OPFILE_ABSOLUTE_FOLDER;
	OpStringC name(FileName(folder1,FALSE));
	OpAutoPtr<OpFileDescriptor> fil( CreateAndOpenFile(name, folder1, mode, op_err, flags) );
									__DO_STOP_TIMING(TIMING_FILE_OPERATION);

	if(OpStatus::IsError(op_err))
	{
		OP_NEW_DBG("Cache_Storage::OpenFile()", "cache.storage");
		OP_DBG((UNI_L("Cannot open file %s\n"), name.CStr()));

		url->HandleError(GetFileError(op_err, url,UNI_L("open")));
		return NULL;
	}
	return fil.release();
}

#if !defined NO_SAVE_SUPPORT || !defined RAMCACHE_ONLY
DataStream_GenericFile *Cache_Storage::OpenDataFile(const OpStringC &name, OpFileFolder folder,OpFileOpenMode mode, OP_STATUS &op_err, OpFileLength start_position, int flags)
{
	OpFileFolder folder1 = folder;
	OpStringC	name1(name.HasContent() ? name : FileName(folder1, ((mode & OPFILE_WRITE) != 0 ? FALSE: TRUE)));
	OpAutoPtr<OpFileDescriptor> fil0( CreateAndOpenFile(name1, folder1, mode, op_err, flags) );
									__DO_STOP_TIMING(TIMING_FILE_OPERATION);
	
	OpAutoPtr<DataStream_GenericFile> fil(NULL);

	if(OpStatus::IsSuccess(op_err))
	{
		if(start_position != FILE_LENGTH_NONE)
			SetWritePosition(fil0.get(), start_position);
		fil.reset(OP_NEW(DataStream_GenericFile, (fil0.get(), ((mode & OPFILE_READ) != 0 ? FALSE : TRUE))));
		if(fil.get() != NULL)
		{
			fil0.release();
			TRAP(op_err, fil->InitL());
		}
		else
			op_err = OpStatus::ERR_NO_MEMORY;
	}

	if(OpStatus::IsError(op_err))
	{
		url->HandleError(GetFileError(op_err, url,UNI_L("open")));
		return NULL;
	}
	return fil.release();
}
#endif

#if !defined NO_SAVE_SUPPORT || !defined RAMCACHE_ONLY
OP_STATUS Cache_Storage::CopyCacheFile(const OpStringC &source_name, OpFileFolder source_folder, 
										const OpStringC &target_name,  OpFileFolder target_folder, 
										BOOL source_is_memory, BOOL target_is_memory, OpFileLength start_position)
{
/*#if CACHE_SMALL_FILES_SIZE>0
	OP_ASSERT(  (source_is_memory && cache_content.GetLength()==content_size) ||
				(source_is_memory && cache_content.GetLength()==content_size)
	);
	
	
	if(source_is_memory && cache_content.GetLength()<=CACHE_SMALL_FILES_SIZE)
		target_is_memory=TRUE;
#endif*/
	
	if(!!source_is_memory == !!target_is_memory && source_name.Compare(target_name) == 0)
		// Source == target
		return OpStatus::OK;
		
	DataStream * OP_MEMORY_VAR source;
	OpAutoPtr<DataStream_GenericFile> source_file(NULL);
	OP_STATUS op_err = OpStatus::OK;

	if(source_name.HasContent() || !source_is_memory)
	{
		source_file.reset(OpenDataFile(source_name, source_folder, OPFILE_READ, op_err));
		RETURN_IF_ERROR(op_err);
		source = source_file.get();
	}
	else
	{
		cache_content.ResetRead();
		source = &cache_content;
	}
	
	BOOL target_has_content;
	
	/*#if CACHE_SMALL_FILES_SIZE>0
		if(content_size>0 && content_size<=CACHE_SMALL_FILES_SIZE)
		{
			target_is_memory=TRUE;
			target_has_content=FALSE;
		}
		else
	#endif*/
			target_has_content=target_name.HasContent();
	
	DataStream * OP_MEMORY_VAR target;
	OpAutoPtr<DataStream_GenericFile> target_file(NULL);

	if(target_has_content || !target_is_memory)
	{
		target_file.reset(OpenDataFile(target_name, target_folder, OPFILE_WRITE, op_err, start_position));
		RETURN_IF_ERROR(op_err);
		target = target_file.get();
	}
	else
	{
		cache_content.ResetRecord(); 
		target = &cache_content;
	}
	
	// target can be zero in OOM conditions!
	if(!target)
		return OpStatus::ERR_NO_MEMORY;
	
#ifdef SELFTEST
	if(urlManager->emulateOutOfDisk != 0)
		return OpStatus::ERR_NO_DISK;
#endif //SELFTEST
	
	TRAP(op_err, source->ExportDataL(target));

	RETURN_IF_ERROR(op_err);
	
	#ifdef DATASTREAM_ENABLE_FILE_OPENED
		// If the file for some reason was closed, it indicates an error.
		if(!target_file.get()->Opened())
			op_err = OpStatus::ERR;
	#endif

	//TRAP(op_err, target->AddContentL(source));
	return op_err;
}
#endif // !defined NO_SAVE_SUPPORT || !defined RAMCACHE_ONLY

unsigned long Cache_Storage::SaveToFile(const OpStringC &filename)
{
	OP_STATUS ops;

	if(GetContextManager()->BypassStorageSave(this, filename, ops))
		return ops;

	//#if CACHE_SMALL_FILES_SIZE>0
	//	OP_ASSERT(!ManageEmbedding());
	//#endif

#if defined NO_SAVE_SUPPORT && !defined DISK_CACHE_SUPPORT
	OP_STATUS op_err = OpStatus::ERR_NOT_SUPPORTED;
#else
	OpFileFolder folder1 = OPFILE_ABSOLUTE_FOLDER;
	OpStringC empty;
	OpStringC src_name(filename.HasContent() ? FileName(folder1) : empty);
									__DO_START_TIMING(TIMING_FILE_OPERATION);
	OP_STATUS op_err = CopyCacheFile(src_name, folder1, filename, OPFILE_ABSOLUTE_FOLDER, src_name.IsEmpty(), FALSE, save_position);
									__DO_STOP_TIMING(TIMING_FILE_OPERATION);
#endif

	if(OpStatus::IsError(op_err))
	{
		unsigned long err = url->HandleError(GetFileError(op_err, url,UNI_L("write")));
		if(OpStatus::IsMemoryError(op_err))
			g_memory_manager->RaiseCondition(op_err);
		return err;
	}
									__DO_ADD_TIMING_PROCESSED(TIMING_FILE_OPERATION,cache_content.GetLength());
	return 0;
}

void Cache_Storage::ResetForLoading()
{
	OP_NEW_DBG("Cache_Storage::ResetForLoading", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - read_only: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, cache_content.GetLength(), url->GetContextId(), read_only));

	SubFromCacheUsage(url, IsRAMContentComplete(), IsDiskContentAvailable() && HasContentBeenSavedToDisk());

#ifdef SELFTEST
	GetContextManager()->VerifyURLSizeNotCounted(url, IsRAMContentComplete(),  IsDiskContentAvailable() && HasContentBeenSavedToDisk());
#endif // SELFTEST

	cache_content.ResetRecord();
	content_size = 0;
	read_only = FALSE;
}

BOOL Cache_Storage::FlushMemory(BOOL force)
{
	OP_NEW_DBG("Cache_Storage::FlushMemory", "cache.limit");
	OP_DBG((UNI_L("URL: %s - %x - content_size: %d - cache_content: %d - ctx id: %d - read_only: %d\n"), url->GetAttribute(URL::KUniName).CStr(), url, (UINT32) content_size, cache_content.GetLength(), url->GetContextId(), read_only));

	if(!force && Cardinal())
		return FALSE;

#ifdef SELFTEST
	if(!read_only)
		GetContextManager()->VerifyURLSizeNotCounted(url, TRUE, FALSE);
#endif // SELFTEST

	if(read_only)
	{
		SubFromCacheUsage(url, TRUE, FALSE);

		read_only = FALSE;
	}
	cache_content.ResetRecord();

	return TRUE;
}

OP_STATUS Cache_Storage::FilePathName(OpString &name, BOOL get_original_body) const
{
	OpFileFolder folder1 = OPFILE_ABSOLUTE_FOLDER;
	OpStringC fname(FileName(folder1, get_original_body));

	if(folder1 == OPFILE_ABSOLUTE_FOLDER)
		return name.Set(fname.CStr());

	name.Empty();

	OpFile fil;
	RETURN_IF_ERROR(fil.Construct(fname.CStr(), folder1));

	return name.Set(fil.GetFullPath());
}

void Cache_Storage::DecFileCount()
{
}

void Cache_Storage::SetMemoryOnlyStorage(BOOL flag)
{
}

BOOL Cache_Storage::GetMemoryOnlyStorage()
{
	return FALSE;
}

BOOL Cache_Storage::IsPersistent()
{
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	// never-flush files are always persistent
	if(never_flush)
		return TRUE;
#endif

	// Failed/aborted loads are not persistent
	URLStatus stat = (URLStatus) url->GetAttribute(URL::KLoadStatus);
	if(stat != URL_LOADED && stat != URL_LOADING && stat != URL_UNLOADED)
		return FALSE;

// Only used by cache files. True if it is to be preserved for the next session; DEFAULT TRUE

	switch((URLContentType) url->GetAttribute(URL::KContentType))
	{
		// Document cache
	case URL_HTML_CONTENT:
	case URL_XML_CONTENT:
	case URL_TEXT_CONTENT:
		return g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheDocs);

		// Image cache
	case URL_GIF_CONTENT:
	case URL_JPG_CONTENT:
	case URL_XBM_CONTENT:
	case URL_BMP_CONTENT:
	case URL_WEBP_CONTENT:
	case URL_CPR_CONTENT:
#ifdef _PNG_SUPPORT_
	case URL_PNG_CONTENT:
#endif
#ifdef HAS_ATVEF_SUPPORT
	case URL_TV_CONTENT: // This is an "image" also.
#endif /* HAS_ATVEF_SUPPORT */
#ifdef _WML_SUPPORT_
	case URL_WBMP_CONTENT:
#endif
		return g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheFigs);

	default:
		break;
	}
	// Other cache
	return  g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheOther);
}

#ifdef CACHE_ENABLE_LOCAL_ZIP
OP_STATUS Cache_Storage::ConfigureEncode()
{
	if(encode_storage)
		return OpStatus::OK;

	encode_storage = OP_NEW(InternalEncoder, (DataStream_Deflate));
	if(encode_storage == NULL)
		return OpStatus::OK; // encoder is not critical (at this time; it may be for a future encryption mode)

	if(	OpStatus::IsError(encode_storage->Construct()) ||
		OpStatus::IsError(content_encoding.Set("deflate"))
		)
	{
		OP_DELETE(encode_storage);
		encode_storage = NULL;
		return OpStatus::OK; // encoder is not critical (at this time; it may be for a future encryption mode)
	}

	return OpStatus::OK;
}

Cache_Storage::InternalEncoder::InternalEncoder(DataStream_Compress_alg alg)
: compression_fifo(), compressor(alg, TRUE, &compression_fifo, FALSE)
{
}


OP_STATUS Cache_Storage::InternalEncoder::Construct()
{
	compression_fifo.SetIsFIFO();
	RETURN_IF_LEAVE(compressor.InitL());
	return OpStatus::OK;
}

OP_STATUS Cache_Storage::InternalEncoder::StoreData(Cache_Storage *target, const unsigned char *buffer, unsigned long buf_len)
{
	OP_ASSERT(target);
	OP_ASSERT(buffer);
	RETURN_IF_LEAVE(compressor.WriteDataL(buffer, buf_len));

	return WriteToStorage(target);
}

OP_STATUS Cache_Storage::InternalEncoder::FinishStorage(Cache_Storage *target)
{
	OP_ASSERT(target);
	RETURN_IF_LEAVE(compressor.FlushL());

	return WriteToStorage(target);
}

OP_STATUS Cache_Storage::InternalEncoder::WriteToStorage(Cache_Storage *target)
{
	unsigned long buf_len2 = compression_fifo.GetLength();
	if(buf_len2 && target)
	{
		RETURN_IF_ERROR(target->StoreData(compression_fifo.GetDirectPayload(), buf_len2));
		RETURN_IF_LEAVE(compression_fifo.CommitSampledBytesL(buf_len2));
	}
	return OpStatus::OK;
}
#endif

#ifdef _MIME_SUPPORT_
BOOL Cache_Storage::GetAttachment(int i, URL &url)
{
	url = URL();
	return FALSE;
}
#endif // _MIME_SUPPORT_


#if CACHE_SMALL_FILES_SIZE>0
OP_STATUS Cache_Storage::RetrieveEmbeddedContent(DiskCacheEntry *entry)
{
	// Check and resets
	OP_ASSERT(IsEmbedded());
	OP_ASSERT(entry);
	
	if(!entry)
		return OpStatus::ERR_NULL_POINTER;
		
	OP_ASSERT(entry->GetEmbeddedContentSize()==0);
	if(entry->GetEmbeddedContentSizeReserved()==0)
		RETURN_IF_ERROR(entry->ReserveSpaceForEmbeddedContent(CACHE_SMALL_FILES_SIZE));
	
	cache_content.ResetRead();
	
	UINT32 OP_MEMORY_VAR len=cache_content.GetLength();
	
	if(!len)
	{
		entry->SetEmbeddedContentSize(0);
		
		return OpStatus::OK;
	}
	
	UINT32 OP_MEMORY_VAR read_len=0;
	
	OP_ASSERT(len<=entry->GetEmbeddedContentSizeReserved());
	OP_ASSERT(len<=CACHE_SMALL_FILES_SIZE);
	
	if(len<=entry->GetEmbeddedContentSize())
		return OpStatus::ERR;
	
	// Embed the file
	RETURN_IF_LEAVE(read_len=cache_content.ReadDataL(entry->GetEmbeddedContent(), len, DataStream::KSampleOnly));
	
	OP_ASSERT(read_len==len);
	
	if(read_len!=len)
		return OpStatus::ERR;
	
	entry->SetEmbeddedContentSize(read_len);
	OP_ASSERT(entry->GetEmbeddedContentSize()==len);
	
	return OpStatus::OK;
}

OP_STATUS Cache_Storage::StoreEmbeddedContent(DiskCacheEntry *entry)
{
	OP_ASSERT(entry && entry->GetEmbeddedContentSize());
	OP_ASSERT(!embedded);
	
	embedded=FALSE;
	
	if(!entry)
		return OpStatus::ERR_NULL_POINTER;
		
	if(!entry->GetEmbeddedContentSize())
		return OpStatus::OK;
		
	OP_ASSERT(!cache_content.GetLength());
	
	RETURN_IF_LEAVE(cache_content.WriteDataL(entry->GetEmbeddedContent(), entry->GetEmbeddedContentSize()));
	
	UINT32 write_len=cache_content.GetLength();
	
	OP_ASSERT(write_len==entry->GetEmbeddedContentSize());
	
	if(write_len!=entry->GetEmbeddedContentSize())
		return OpStatus::ERR;
		
	embedded=TRUE;
	urlManager->IncEmbeddedSize(write_len);
	
	return OpStatus::OK;
}
#endif

#if CACHE_CONTAINERS_ENTRIES>0
OP_STATUS Cache_Storage::StoreInContainer(CacheContainer *cont, UINT id)
{
	// Some check
	OP_ASSERT(cont && id);
	
	if(!cont)
		return OpStatus::ERR_NULL_POINTER;
		
	if(!id)
		return OpStatus::ERR_OUT_OF_RANGE;
	
	int len=cache_content.GetLength();
	
	OP_ASSERT(len);
	
	if(!len)
		return OpStatus::ERR_OUT_OF_RANGE;
		
	OP_ASSERT(len<=CACHE_CONTAINERS_FILE_LIMIT);
	
	if(len>CACHE_CONTAINERS_FILE_LIMIT)
		return OpStatus::ERR_OUT_OF_RANGE;
		
	// Allocate a pointer and update the container
	unsigned char *ptr=OP_NEWA(unsigned char, len);
	
	if(!ptr)
		return OpStatus::ERR_NO_MEMORY;
		
	if(!cont->UpdatePointerByID(id, ptr, len))
	{
		OP_ASSERT(FALSE);
		
		OP_DELETEA(ptr);
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	}
	
	// Read the cache content
	int OP_MEMORY_VAR read_len=0;
	
	cache_content.ResetRead();
	
	RETURN_IF_LEAVE(read_len=cache_content.ReadDataL(ptr, len, DataStream::KSampleOnly));
	
	if(len!=read_len)
		return OpStatus::ERR;
		
	OP_ASSERT(CheckContainer());
	
	return OpStatus::OK;
}

#ifdef SELFTEST
BOOL Cache_Storage::CheckContainer()
{
	if(checking_container)
		return TRUE;

	Context_Manager *ctx=GetContextManager();

	checking_container=TRUE;
	BOOL b = !ctx || ctx->CheckCacheStorage(this);

	checking_container=FALSE;
	
	return b;
}
#endif // SELFTEST
#endif // CACHE_CONTAINERS_ENTRIES>0

Context_Manager *Cache_Storage::GetContextManager()
{
	Context_Manager *cman = urlManager->FindContextManager(Url()->GetContextId());

	OP_ASSERT(cman);
	
	return cman;
}

#ifdef _OPERA_DEBUG_DOC_
void Cache_Storage::GetMemUsed(DebugUrlMemory &debug)
{
	debug.memory_loaded += sizeof(*this) + cache_content.GetLength();
/*	if(small_content)
	{
		debug.number_ramcache++;
		debug.memory_ramcache += content_size;
	}
	else if(content)
	{
		debug.number_ramcache++;
		debug.memory_ramcache += content->GetMemSize();
	}*/
}
#endif

OP_STATUS Cache_Storage::GetUnsortedCoverage(OpAutoVector<StorageSegment> &out_segments, OpFileLength start, OpFileLength len)
{
	OpFileLength size = (content_size > cache_content.GetLength()) ? content_size : cache_content.GetLength();

	if(start>size)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	if(start==FILE_LENGTH_NONE)
		return OpStatus::ERR_OUT_OF_RANGE;

	StorageSegment *seg;
	RETURN_OOM_IF_NULL(seg=OP_NEW(StorageSegment, ()));

	OpFileLength end = (len<size) ? len : size;
	seg->content_start = start;
	seg->content_length = end-seg->content_start;

	OP_STATUS ops=out_segments.Add(seg);

	if(OpStatus::IsError(ops))
		OP_DELETE(seg);

	return ops;
}

OP_STATUS Cache_Storage::GetSortedCoverage(OpAutoVector<StorageSegment> &out_segments, OpFileLength start, OpFileLength len, BOOL merge)
{
	// One segment is always sorted...
	return GetUnsortedCoverage(out_segments, start, len);
}

void Cache_Storage::GetPartialCoverage(OpFileLength position, BOOL &available, OpFileLength &length, BOOL multiple_segments)
{
	OpFileLength loaded=ContentLoaded();
	if(position==FILE_LENGTH_NONE || position>=loaded)
	{
		available=FALSE;
		length=0;
	}
	else
	{
		available=TRUE;
		length=loaded-position;
	}
}

#ifdef CACHE_FAST_INDEX
	SimpleStreamReader *Cache_Storage::CreateStreamReader()
	{
		UINT32 size=cache_content.GetLength();
		
		if(size>0) // Stream buffer from datastream
		{
			cache_content.SetNeedDirectAccess(TRUE);
			SimpleBufferReader *reader=OP_NEW(SimpleBufferReader, (cache_content.GetDirectPayload(), size));
			cache_content.SetNeedDirectAccess(FALSE);
			
			if(reader)
			{
				if(OpStatus::IsError(reader->Construct()))
				{
					OP_DELETE(reader);
					reader=NULL;
				}
				
				return reader;
			}
		}
		
		return NULL;
	}
#endif
