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

#include "modules/url/url2.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_cs.h"


Download_Storage::Download_Storage(Cache_Storage *source)
	: File_Storage(source->Url(), URL_CACHE_USERFILE, TRUE, TRUE)
{
	constructed = FALSE;
	decided_decoding = FALSE;
	need_to_decode_all = FALSE;
	decoder = NULL;
	temp_storage = NULL;
}

Download_Storage::Download_Storage(URL_Rep *url)
	: File_Storage(url, URL_CACHE_USERFILE, TRUE, TRUE)
{
	constructed = FALSE;
	decided_decoding = FALSE;
	need_to_decode_all = FALSE;
	decoder = NULL;
	temp_storage = NULL;
}

OP_STATUS Download_Storage::Construct(Cache_Storage *source, const OpStringC &afilename)
{
	RETURN_IF_ERROR(File_Storage::Construct(afilename, NULL));

	TruncateAndReset();
		
	temp_storage = source;	

	if(!temp_storage)
	{
		if((URLStatus) url->GetAttribute(URL::KLoadStatus) != URL_LOADING)
			decided_decoding = TRUE;
		return OpStatus::OK;
	}

	OP_STATUS op_err;
	op_err = StoreData(NULL, 0);
	if(OpStatus::IsError(op_err))
	{
		temp_storage = NULL; // Will continue to use the old cache element, this object can no longer own it, return to previous owner
		return op_err;
	}

	constructed = TRUE;

	if(decided_decoding && !need_to_decode_all)
	{
		OP_DELETE(decoder);
		decoder = NULL;

		OP_DELETE(temp_storage);
		temp_storage = NULL;
	}

	return OpStatus::OK;
}

OP_STATUS Download_Storage::Construct(URL_Rep *url, const OpStringC &afilename, BOOL finished)
{
	RETURN_IF_ERROR(File_Storage::Construct(afilename,NULL));
	if(!finished)
		this->ResetForLoading();
	else
		decided_decoding = TRUE;
	constructed = TRUE;
	return OpStatus::OK;
}

Download_Storage* Download_Storage::Create(Cache_Storage *source, const OpStringC &afilename)
{
	Download_Storage* ds = OP_NEW(Download_Storage, (source));
	if (!ds)
		return NULL;
	if (OpStatus::IsError(ds->Construct(source, afilename)))
	{
		OP_DELETE(ds);
		return NULL;
	}
	return ds;
}

Download_Storage* Download_Storage::Create(URL_Rep *url, const OpStringC &afilename, BOOL finished)
{
	Download_Storage* ds = OP_NEW(Download_Storage, (url));
	if (!ds)
		return NULL;
	if (OpStatus::IsError(ds->Construct(url, afilename, finished)))
	{
		OP_DELETE(ds);
		return NULL;
	}
	return ds;
}


OP_STATUS Download_Storage::StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position)
{
	OP_NEW_DBG("Download_Storage::StoreData()", "cache.storage");
	OP_DBG((UNI_L("URL: %s - need_to_decode_all: %d - Temp Storage: %d - Len: %d - URL points to me: %d\n"), url->GetAttribute(URL::KUniName).CStr(), need_to_decode_all, temp_storage?1:0, (int) buf_len, url->GetDataStorage() && url->GetDataStorage()->GetCacheStorage()==this));

	if(!decided_decoding)
	{
		OP_STATUS op_err = OpStatus::OK;
		SetDescriptor(op_err);
		RETURN_IF_ERROR(op_err);
	}

	if(!temp_storage)
	{
		if(!buffer)
			return OpStatus::OK;

		return File_Storage::StoreData(buffer, buf_len);
	}

	if(buffer)
		RETURN_IF_ERROR(temp_storage->StoreData(buffer, buf_len));

	if(!decoder)
		return OpStatus::OK;

	BOOL more = TRUE;
	while(more)
	{
		unsigned long tbuf_len = decoder->RetrieveData(more);
		if(tbuf_len)
		{
			//DumpTofile((unsigned char *) decoder->GetBuffer(), tbuf_len, "Decoded content", "dwload.txt");
			File_Storage::StoreData((unsigned char *) decoder->GetBuffer(), tbuf_len);
			// File_Storage::StoreData may call SetFinished() in case of failure, deleting the decoder
			if (!decoder)
				break;

			decoder->ConsumeData(tbuf_len);
		}
		else
			break;
	}
	return OpStatus::OK;
}

void Download_Storage::SetDescriptor(OP_STATUS &op_err)
{
	op_err = OpStatus::OK;

	if(decided_decoding)
		return;

	decided_decoding = TRUE;
	need_to_decode_all = FALSE;
	BOOL decode_data = TRUE;

	OpStringS8 *encoding = url->GetHTTPEncoding(); 
	need_to_decode_all = (encoding != NULL && encoding->Length() != 0);

	/* tar.gz workaround */
	OpStringC8 mime_type(url->GetAttribute(URL::KMIME_Type));

	if(need_to_decode_all && (mime_type.IsEmpty() || 
		mime_type.CompareI("application/x-tar") == 0 || 
		mime_type.CompareI("application/x-gzip") == 0 ||
		mime_type.CompareI("application/x-tar-gz") == 0 || 
		mime_type.CompareI("application/unix-tar") == 0 || 
		mime_type.CompareI("application/x-gtar") == 0 || 
		mime_type.CompareI("application/x-gunzip") == 0 || 
		mime_type.CompareI("application/x-ustar") == 0 || 
		mime_type.CompareI("application/x-tar-gz") == 0 ||
		mime_type.CompareI("application/gzip") == 0))
	{
		need_to_decode_all = FALSE;
		decode_data = FALSE;
	}
	/* end of tar.gz workaround */

	if(temp_storage || need_to_decode_all)
	{
		if(!temp_storage)
		{
#ifndef RAMCACHE_ONLY	// added by lth
			temp_storage = Session_Only_Storage::Create(url);
#else
			temp_storage = OP_NEW(Memory_Only_Storage, (url));
#endif
			if(temp_storage == NULL)
			{
				g_memory_manager->RaiseCondition(op_err = OpStatus::ERR_NO_MEMORY);
				return;
			}
		
			if(encoding)
			{
				temp_storage->TakeOverContentEncoding(*encoding);
				if(decode_data)
					url->SetAttribute(URL::KResumeSupported,Not_Resumable); 
			}
		}

		if(need_to_decode_all)
		{
			OpFileLength content_len=0; // Length is unpredictable when we are using a decoder.
			url->SetAttribute(URL::KContentSize, &content_len);
		}

		decoder = temp_storage->GetDescriptor(NULL, TRUE, decode_data);
		if(decoder == NULL)
		{
			g_memory_manager->RaiseCondition(op_err = OpStatus::ERR_NO_MEMORY);
			decided_decoding = FALSE;
			return;
		}
		if(!need_to_decode_all)
			temp_storage->SetFinished();
		//if(encoding)
		//	encoding->Empty();
	}
}

Download_Storage::~Download_Storage()
{
	SetFinished();
}

void Download_Storage::SetFinished(BOOL force)
{
	OP_NEW_DBG("Download_Storage::SetFinished()", "cache.storage");
	OP_DBG((UNI_L("URL: %s - Temp Storage: %d - URL points to me: %d\n"), url->GetAttribute(URL::KUniName).CStr(), temp_storage?1:0, url->GetDataStorage() && url->GetDataStorage()->GetCacheStorage()==this));

	if(!constructed)
		temp_storage = NULL; // In case of a construction failure (e.g. disk full), error handling will already have finished the original cache element

	if(temp_storage)
	{
		temp_storage->SetFinished(force);

		if (!force) 
			OpStatus::Ignore(StoreData(NULL,0));

		Cache_Storage *t = temp_storage; // In case of disk full SetFinished() can be called again.
		temp_storage = NULL;
		OP_DELETE(t);
	}

	if(decoder)
	{
		OP_DELETE(decoder);
		decoder = NULL;
	}

	decided_decoding = TRUE;

	File_Storage::SetFinished(force);
}

void Download_Storage::TruncateAndReset()
{
	File_Storage::TruncateAndReset();

	decided_decoding = FALSE    ;

	OP_DELETE(decoder);
	decoder = NULL;
}

