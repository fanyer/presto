/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/cache/url_cs.h"

unsigned long GetFileError(OP_STATUS op_err, URL_Rep *url, const uni_char *operation);

Local_File_Storage::Local_File_Storage(URL_Rep *url_rep, int flags)
	: File_Storage(url_rep, URL_CACHE_USERFILE)
	, m_flags(flags)
#if LOCAL_STORAGE_CACHE_LIMIT>0
	, buffer(NULL), buffer_len(0), time_cached(0)
#endif
{

}

Local_File_Storage::~Local_File_Storage()
{
#if LOCAL_STORAGE_CACHE_LIMIT>0
	if(buffer)
	{
		GetContextManager()->SubFromRamCacheSize(buffer_len, url);
		OP_DELETEA(buffer);
		buffer = NULL;
		read_only = FALSE;
	}
#endif
}


OP_STATUS Local_File_Storage::Construct(const uni_char *afilename, OpFileFolder folder)
{
	return File_Storage::Construct(afilename, NULL, folder);
}

Local_File_Storage* Local_File_Storage::Create(URL_Rep *url_rep, const OpStringC &afilename, OpFileFolder folder, int flags)
{
	Local_File_Storage* lfs = OP_NEW(Local_File_Storage, (url_rep, flags));
	if (!lfs)
		return NULL;
	if (OpStatus::IsError(lfs->Construct(afilename.CStr(), folder)))
	{
		OP_DELETE(lfs);
		return NULL;
	}
	return lfs;	
}

OP_STATUS Local_File_Storage::StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position)
{
	return OpStatus::OK;
}

BOOL Local_File_Storage::Flush()
{
	if(!GetFinished())
		return FALSE;

#if LOCAL_STORAGE_CACHE_LIMIT>0
	if (buffer)
	{
		GetContextManager()->SubFromRamCacheSize(buffer_len, url);
		buffer_len = 0;
		OP_DELETEA(buffer);
		buffer = NULL;
		read_only = FALSE;
	}
#endif

	return TRUE;
}

OpFileDescriptor* Local_File_Storage::OpenFile(OpFileOpenMode mode, OP_STATUS &op_err, int /*flags*/)
{
	return File_Storage::OpenFile(mode, op_err, m_flags);
}

OpFileDescriptor* Local_File_Storage::CreateAndOpenFile(const OpStringC &filename, OpFileFolder folder, OpFileOpenMode mode, OP_STATUS &op_err, int /*flags*/)
{
	return File_Storage::CreateAndOpenFile(filename, folder, mode, op_err, m_flags);
}

#if !defined NO_SAVE_SUPPORT || !defined RAMCACHE_ONLY
DataStream_GenericFile*	Local_File_Storage::OpenDataFile(const OpStringC &name, OpFileFolder folder,OpFileOpenMode mode, OP_STATUS &op_err, OpFileLength start_position, int /*flags*/)
{
	return File_Storage::OpenDataFile(name, folder, mode, op_err, start_position, m_flags);
}
#endif


#if LOCAL_STORAGE_CACHE_LIMIT>0

OpFileLength Local_File_Storage::ContentLoaded(BOOL force /*=FALSE*/)
{
	if (buffer)
		return buffer_len;
	else
		return File_Storage::ContentLoaded(force);
}

unsigned long Local_File_Storage::RetrieveDataAndCache(URL_DataDescriptor *desc,BOOL &more,BOOL cache_file_content)
{
	if (cache_file_content && filename.HasContent() && ((URLType)url->GetAttribute(URL::KType)) != URL_WIDGET )
	{
		OP_STATUS op_err = OpStatus::OK;

		more = FALSE;
		urlManager->SetLRU_Item(url);

		// Check if the local file has been modified.
		// Problem: cache_file will often be null. We don't open the file to try to speed up the most
		// common case (file unchanged)
		if (buffer)
		{
			time_t modified = 0;
			OP_STATUS ops;

			if(cache_file)  // we are lucky, it is still already available
				ops = static_cast<OpFile *>(cache_file)->GetLastModified(modified);
			else // Just construct a file and get the date
			{
				OpFile f;
				OpFileFolder folder1 = OPFILE_ABSOLUTE_FOLDER;
				OpStringC name(FileName(folder1,FALSE));

				ops = f.Construct(name.CStr(), folder1, m_flags);

				if(OpStatus::IsSuccess(ops))
					ops = f.GetLastModified(modified);
			}

			if(modified && OpStatus::IsSuccess(ops) && modified!=time_cached)
			{
				time_cached = modified;  // Save the time
				OP_DELETEA(buffer);		 // Force a re-buffering of the file
				buffer = NULL;

				GetContextManager()->SubFromRamCacheSize(buffer_len, url);
				read_only = FALSE;
			}
		}

		if (!buffer) // Cache the content of the file
		{
		#ifdef SELFTEST
			num_disk_read++;
		#endif

			if(!cache_file)
				cache_file = OpenFile(OPFILE_READ, op_err);

			if(!cache_file)
				return desc->GetBufSize();
			
			OpFileLength file_len = 0, possible_buffer_len = 0;

			if (!content_size)
				cache_file->GetFileLength(file_len);
			possible_buffer_len = content_size ? content_size : file_len;

			if (possible_buffer_len>LOCAL_STORAGE_CACHE_LIMIT)
			{
				unsigned long ret = File_Storage::RetrieveData(desc, more);
				if (!more)
					CloseFile();
				return ret;
			}

			buffer_len = static_cast<unsigned int>(possible_buffer_len);
			buffer = OP_NEWA(unsigned char,buffer_len);
			if (!buffer)
				return desc->GetBufSize();

			OpFileLength bread = 0;
			if(OpStatus::IsError(op_err = cache_file->Read(buffer, buffer_len, &bread)))
			{
				OP_DELETEA(buffer);
				buffer = 0;
				url->HandleError(GetFileError(op_err, url,UNI_L("read")));
				return desc->GetBufSize();
			}

			GetContextManager()->AddToRamCacheSize(buffer_len, url, TRUE);
			read_only = TRUE;

			// Note: in this case we are sure that cache_file is an OpFile (in particular a CacheFile), as we simply are on a storage where
			// only files are accessed, so the cast is supposed to be safe; alternatively, time_cached should
			// be set by OpenFile() and, at the last stage, by CreateAndOpenFile()
			if(OpStatus::IsError(((OpFile *)cache_file)->GetLastModified(time_cached)))
				time_cached = 0;

			CloseFile();
		}

		OpFileLength pos = desc->GetPosition() + desc->GetBufSize();
		if(pos<buffer_len && pos+desc->AddContentL(NULL, buffer + static_cast<unsigned int>(pos), buffer_len - static_cast<unsigned int>(pos), FALSE)<buffer_len)
			more = TRUE;
		else if(!desc->PostedMessage())
				desc->PostMessage(MSG_URL_DATA_LOADED, url->GetID(), 0);

		return desc->GetBufSize();
	}
	return File_Storage::RetrieveData(desc, more);
}
#endif

unsigned long Local_File_Storage::RetrieveData(URL_DataDescriptor *desc,BOOL &more)
{
#if LOCAL_STORAGE_CACHE_LIMIT>0
	return RetrieveDataAndCache(desc, more, TRUE);
#else
	return File_Storage::RetrieveData(desc, more);
#endif
}

