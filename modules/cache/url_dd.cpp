/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/hardcore/mh/messages.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/encodings/utility/createinputconverter.h"
#include "modules/url/url2.h"

#ifdef WBXML_SUPPORT
#include "modules/logdoc/wbxml_decoder.h"
#endif // WBXML_SUPPORT

#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_cs.h"
#include "modules/url/protocols/http_te.h"
#include "modules/cache/url_i18n.h"
#include "modules/url/url_man.h"

#include "modules/util/cleanse.h"

#include "modules/olddebug/timing.h"

#include "modules/olddebug/tstdump.h"
#ifdef _DEBUG
#ifdef YNP_WORK
#define _DEBUG_DD
#define _DEBUG_DD1
#endif
#endif

#if defined _DEBUG_DD || defined _DEBUG_DD1 || defined DEBUG_LOAD_STATUS
#include "modules/url/tools/url_debug.h"
#endif

#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define CACHE_ERRSTR(p,x) Str::##p##_##x
#else
#define CACHE_ERRSTR(p,x) x
#endif

// Url_dd.cpp

// URL Data Descriptor

#ifdef DEBUG_LOAD_STATUS
uint32 dd_counter = 0;

void URL_DataDescriptor::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
	case MSG_LOAD_DEBUG_STATUS:
				PrintfTofile("dd_loading.txt","     (%u active) URL request %s \n", dd_counter,DebugGetURLstring(url));
		break;
	}
}
#endif


URL_DataDescriptor::URL_DataDescriptor(URL_Rep *url_rep, MessageHandler *pmh, URLContentType override_content_type, unsigned short override_charset_id, unsigned short parent_charset_id)
{
#ifdef DEBUG_LOAD_STATUS
	dd_counter ++;
	g_main_message_handler->SetCallBack(this, MSG_LOAD_DEBUG_STATUS, 0);
#endif

	url = URL(url_rep, (char*)NULL);

	parent_charset = parent_charset_id;
	g_charsetManager->IncrementCharsetIDReference(parent_charset);
	mh = pmh;
	position = 0;

	buffer = NULL;
	buffer_len = 0;
	buffer_used = 0;
	
#ifdef _DEBUG_DD1
	PrintfTofile("urldd1.txt","\nDD Constructor- %s - %s\n",DebugGetURLstring(url),
		(url.GetAttribute(URL::KLoadStatus) == URL_LOADING ? "Not Loaded" : "Loaded"));
#endif
	storage = NULL;
	is_using_file = FALSE;
	posted_message = FALSE;
	posted_delayed_message = FALSE;
	need_refresh = FALSE;
	mulpart_iterable =  FALSE;

	sub_desc = NULL;

#ifdef _DEBUG
	is_utf_16_content= FALSE;
#endif
	content_type = override_content_type;
	charset_id = override_charset_id;
	g_charsetManager->IncrementCharsetIDReference(charset_id);

#ifdef SELFTEST
	Context_Manager *mng=urlManager->FindContextManager(url.GetContextId());

	if(mng)
		mng->AddDataDescriptorForStats(this);
#endif // SELFTEST
}

OP_STATUS URL_DataDescriptor::Init(BOOL get_raw_data, Window *window)
{
	// caller, for some reason, wanted raw URL data, so we do no conversion.
	// this could be if if we are a sub-descriptor.  if we are a sub-descriptior
	// we are being created because the real descriptor had a decoder and so 
	// creating a new one will cause boundless recursion.
	// alternatively, this could be an applet wanting to do its own conversion.
	if (get_raw_data)
	{
#if defined(_DEBUG) && defined(_MIME_SUPPORT_)
		is_utf_16_content= (url.GetAttribute(URL::KMIME_CharSet).CompareI("utf-16") == 0 || (storage && storage->IsProcessed()));
#endif
		return OpStatus::OK;
	}

#ifdef WBXML_SUPPORT
	const char *mime_type = url.GetAttribute(URL::KMIME_Type).CStr();

	BOOL is_wbxml = FALSE;
	if ( mime_type != NULL )
	{
	    is_wbxml = (op_stricmp(mime_type, "application/vnd.wap.wbxml") == 0
					|| op_stricmp(mime_type, "application/vnd.wap.wmlc") == 0);
	}
#endif // WBXML_SUPPORT

	CE_decoding.Clear();

	// setting up character conversion
	URLContentType content = (content_type != URL_UNDETERMINED_CONTENT ? content_type : 
									(URLContentType) url.GetAttribute(URL::KContentType));

	if (
#ifdef WBXML_SUPPORT
		! is_wbxml &&
#endif // WBXML_SUPPORT
		needsUnicodeConversion(content))
	{
		// ok, it's a type of content we should convert
		// set up converter

		CharacterDecoder* decoder = OP_NEW(CharacterDecoder, (parent_charset, window));
		if(decoder == NULL || OpStatus::IsMemoryError(decoder->Construct()))
		{
			OP_DELETE(decoder);
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return OpStatus::ERR_NO_MEMORY;
		}
		decoder->Into(&CE_decoding);
#ifdef _DEBUG
		is_utf_16_content= TRUE;
#endif
	}

#ifdef WBXML_SUPPORT
	if (is_wbxml)
	{
		WBXML_Decoder *decoder = OP_NEW(WBXML_Decoder, ());
		if (decoder == NULL)
			return OpStatus::ERR_NO_MEMORY;

		decoder->Into(&CE_decoding);
	}
#endif // WBXML_SUPPORT


#if defined(SVG_SUPPORT) && defined(_HTTP_COMPRESS)
	if(content == URL_SVG_CONTENT)
	{
		// FIXME: This may waste memory since most svg content is non-gzipped. 
		//        Only add the decoder if a gzip magic header is found.

		// To be able to open *.svgz images (gzipped SVG)
		HTTP_Transfer_Decoding* unzipDecoder =  HTTP_Transfer_Decoding::Create(HTTP_Gzip);
		if(unzipDecoder == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		unzipDecoder->Into(&CE_decoding);
	}
#endif
	return OpStatus::OK;
}

URL_DataDescriptor::~URL_DataDescriptor()
{
#ifdef DEBUG_LOAD_STATUS
	g_main_message_handler->UnsetCallBacks(this);
#endif
#ifdef _DEBUG_DD1
	/*
	if(position != url.ContentLoaded() || url.Status(FALSE) == URL_LOADING)
		int stop = 1;
		*/
#endif
#ifdef _DEBUG_DD1
	OpFileLength registered_len=0;
	url.GetAttribute(URL::KContentLoaded, &registered_len);
	PrintfTofile("urldd1.txt","\nDD Destructor- %s - %s - %lu:%lu (%u:%u)\n",
		DebugGetURLstring(url), (url.GetAttribute(URL::KLoadStatus) == URL_LOADING ? "Not Loaded" : "Loaded"),
		(unsigned long) position, (unsigned long) registered_len, buffer_used, buffer_len
		);
#endif
	OP_DELETE(sub_desc);

	if(is_using_file && storage)
		storage->DecFileCount();

	if(InList())
		Out();

	if(buffer)
	{
		if(url.GetAttribute(URL::KCachePolicy_NoStore))
			OPERA_cleanse_heap(buffer, buffer_len);
		else
			op_memset(buffer, 0, buffer_len);

		OP_DELETEA(buffer);
	}
	g_charsetManager->DecrementCharsetIDReference(charset_id);
	g_charsetManager->DecrementCharsetIDReference(parent_charset);
#ifdef DEBUG_LOAD_STATUS
	OP_ASSERT(dd_counter >0);
	dd_counter --;
#endif

#ifdef SELFTEST
	Context_Manager *mng=urlManager->FindContextManager(url.GetContextId());

	if(mng)
		mng->RemoveDataDescriptorForStats(this);
#endif // SELFTEST

}

void URL_DataDescriptor::SetStorage(Cache_Storage *strage)
{
	storage = strage;
#if defined(_DEBUG) && defined(_MIME_SUPPORT_)
	is_utf_16_content= is_utf_16_content || (url.GetAttribute(URL::KMIME_CharSet).CompareI("utf-16") == 0 || (storage && storage->IsProcessed()));
#endif
	if(strage && content_type == URL_UNDETERMINED_CONTENT)
	{
		content_type = strage->GetContentType();
#ifdef _CACHE_DESCRIPTOR_MIMETYPE
		content_type_string.Set(strage->GetMIME_Type());
#endif
	}
	if(!charset_id)
	{
		if(strage && content_type != URL_UNDETERMINED_CONTENT)
			charset_id = strage->GetCharsetID();
		else
		{
#ifdef _CACHE_DESCRIPTOR_MIMETYPE
			content_type_string.Set(url.GetAttribute(URL::KMIME_Type));
#endif
			content_type = (URLContentType) url.GetAttribute(URL::KContentType);
			charset_id =  (unsigned short) url.GetAttribute(URL::KMIME_CharSetId);
		}
		g_charsetManager->IncrementCharsetIDReference(charset_id);
	}
}

void URL_DataDescriptor::SetCharsetID(unsigned short id)
{
	g_charsetManager->IncrementCharsetIDReference(id);
	g_charsetManager->DecrementCharsetIDReference(charset_id);
	charset_id=id;
}


unsigned long URL_DataDescriptor::RetrieveDataL(BOOL &more)
{
	ClearPostedMessage();

	// If nothing changed since last RetrieveDataL, more messages will cause spinning
	// In that case we want to block sending MSG_URL_DATA_LOADED while we are in this call
	messageTracker.MaybeStartBlocking(storage && storage->MoreData());

	unsigned long num_bytes = RetrieveDataInternalL(more);
#ifdef DEBUG_ENABLE_OPASSERT
	if (num_bytes < 1024 && more) // We got relatively little on the first try, check if we can get more right away
	{
#if (CACHE_SMALL_FILES_SIZE>0 || CACHE_CONTAINERS_ENTRIES>0)
		if (storage && storage->IsPlainFile())
			return num_bytes; // We don't want to check this case -> Typically user has done "View Source" and might edit the cache file directly
#endif
		unsigned long num_bytes2 = RetrieveDataInternalL(more);
		if (num_bytes == 0 && num_bytes2 > 0)
			OP_ASSERT(!"RetrieveDataL returned 0 bytes but could have returned more data right away. This can be unexpected for the caller, and a performance issue. Please file bug if reproducible.");
		else if (num_bytes2 != num_bytes)
			OP_ASSERT(!"RetrieveDataL could have returned more data right away. This can be a performance issue. Please file bug if reproducible.");
		num_bytes = num_bytes2;
	}
#endif

	messageTracker.StopBlocking();

	return num_bytes;
}

unsigned long URL_DataDescriptor::RetrieveDataInternalL(BOOL &more)
{
	more = FALSE;
	if (need_refresh)
	{
		more = TRUE;
		return 0;
	}

	if(!storage)
	{
		return buffer_used;
	}

	unsigned long ret;
	unsigned long old_buffer_used = buffer_used;
	if(!CE_decoding.Empty())
	{
		Data_Decoder *decoder = (Data_Decoder *) CE_decoding.First();

		if(!buffer)
		{
			// NOTE: it should be very nice to compute the final size of the encoded content, and allocate only the required memory.
			// Unfortunately, the Content Length is not enough, as the content is encoded, so for now we decided to postpone
			// this optimization as it has shown regressions in Desktop
			// Look at CORE-27718 for details.
			buffer_len = DATADESC_BUFFER_SIZE;
			OP_ASSERT(buffer_len<=DATADESC_BUFFER_SIZE);

			buffer = OP_NEWA_L(char, buffer_len);
			buffer_used = 0;
		}
		if(!sub_desc)
		{
			sub_desc = storage->GetDescriptor(mh, TRUE, FALSE, NULL, content_type, charset_id);
			if(sub_desc == NULL)
			{
				return 0;
			}
		}

		URL_ID url_id = url.Id(FALSE);

		if(buffer_len == buffer_used)
		{
			more = TRUE;
			if (!PostedMessage())
				PostMessage(MSG_URL_DATA_LOADED, url_id, 0);
			return buffer_used;
		}

		BOOL read_storage = FALSE;
		unsigned ret1 = decoder->ReadData(buffer + buffer_used , buffer_len - buffer_used,
									sub_desc, read_storage, more);
		buffer_used += ret1;
		ret = buffer_used;

		messageTracker.DataConsumedOrAdded(ret1);

		if(decoder->Error())
		{
			more = FALSE;
			storage = NULL;

			if (!decoder->CanErrorBeHidden())
			{
				if((URLStatus) url.GetAttribute(URL::KLoadStatus) == URL_LOADING)
					url.GetRep()->HandleError(CACHE_ERRSTR(SI,ERR_HTTP_DECODING_FAILURE));
				else
					PostMessage(MSG_URL_LOADING_FAILED,url_id,CACHE_ERRSTR(SI,ERR_HTTP_DECODING_FAILURE));
				return 0;
			}
			return ret;
		}

		if (!more)
		{
			// Check charset detection
			if (url.GetAttribute(URL::KMIME_CharSet).IsEmpty())
			{
				// Character set is not set in the HTTP header or by a
				// <meta> tag.

				// Actual character set used by decoder
				const char *used_charset = decoder->GetCharacterSet();

				// Character set guessed
				const char *guessed_charset = decoder->GetDetectedCharacterSet();

				Window *window = GetDocumentWindow();

				if (used_charset && guessed_charset &&
					op_stricmp(used_charset, guessed_charset) != 0)
					// Made this test case insensitive because
					// of error comparing big5 & Big5
				{
					// Set our new guess as if it was a HTTP header, this
					// keep the next run from detecting again.
					need_refresh = true;
					RAISE_IF_ERROR(url.SetAttribute(URL::KMIME_CharSet, guessed_charset));

					// We also store the name of the force encoding setting
					// here, so that we can re-evaluate if we change it
					// later.

					const char *force_charset = NULL;
					if (window)
					{
						force_charset = window->GetForceEncoding();
					}
					else
					{
						force_charset = g_pcdisplay->GetForceEncoding();
					}
					if (!force_charset || !*force_charset)
					{
						force_charset = "AUTODETECT";
					}
					RAISE_IF_ERROR(url.SetAttribute(URL::KAutodetectCharSet, force_charset));

					//PrintfTofile("urldd.txt", "DD Restart Operation %s\n",	(url.Name() ? url.Name() : ""));
					// Force a refresh from cache
					if (mh.get() != ((MessageHandler *) NULL))
					{
						mh->PostMessage(MSG_MULTIPART_RELOAD, url_id, 1);
						mh->PostMessage(MSG_INLINE_REPLACE, url_id, 0);
						mh->PostMessage(MSG_HEADER_LOADED, url_id, url.GetRep()->GetIsFollowed() ? 0 : 1);
					}

					//return ret;
				}
			}
		}

		if((old_buffer_used != buffer_used) && more && (mh.get() != ((MessageHandler *) NULL)) && !PostedMessage() && !PostedDelayedMessage())
		{
			posted_message = FALSE;
			posted_delayed_message = TRUE;
			mh->PostDelayedMessage(MSG_URL_DATA_LOADED, url_id, 0, 50);
		}

	}
	else
		ret = storage->RetrieveData(this, more);

	if (!more && storage && mulpart_iterable)
	{
		url.GetRep()->IterateMultipart();
		mulpart_iterable = FALSE;
	}

#ifdef _DEBUG_DD1
	OpFileLength registered_len=0;
	url.GetAttribute(URL::KContentLoaded, &registered_len);
	PrintfTofile("urldd1.txt","\nDD RetrieveData- %s - %s - %s- %lu:%lu (%u:%u) %lu\n",
		DebugGetURLstring(url), (url.GetAttribute(URL::KLoadStatus) == URL_LOADING ? "Not Loaded" : "Loaded"),
		(more ? "MORE" : "NOT MORE"), (unsigned long) position, (unsigned long) registered_len, buffer_used, buffer_len, ret
		);
#endif

	return ret;
}

void URL_DataDescriptor::ConsumeData(unsigned len)
{
#ifdef _DEBUG_DD1
	PrintfTofile("urldd1.txt","\nDD Consume Data- %d bytes: %s\n",len,DebugGetURLstring(url));
#endif

#ifdef _DEBUG_DD
	PrintfTofile("urldd.txt","\nDD Consume Data- %d bytes: %s\n",len, DebugGetURLstring(url));
	DumpTofile((const unsigned char *) buffer, (unsigned long) len,"","urldd.txt");

	if(url.GetAttribute(URL::KContentType) == URL_HTML_CONTENT || url.GetAttribute(URL::KContentType) == URL_TEXT_CONTENT ||
		url.GetAttribute(URL::KContentType) == URL_XML_CONTENT || url.GetAttribute(URL::KContentType) == URL_X_JAVASCRIPT ||
		url.GetAttribute(URL::KContentType) == URL_CSS_CONTENT || url.GetAttribute(URL::KContentType) == URL_PAC_CONTENT)
	{
		const char *rdata = buffer;
		unsigned long len2 = len;
		OpString8 encoded;

		if(is_utf_16_content)
		{
			if(OpStatus::IsSuccess(encoded.SetUTF8FromUTF16((const uni_char *)buffer, UNICODE_DOWNSIZE(len))))
			{
				rdata = encoded.CStr();
				len2 = encoded.Length();
			}
		}

		PrintfTofile("urldd.txt","\n-----\n%.*s\n------\n",len2,rdata);
	}
#endif

	if(buffer_used > len)
	{
		op_memmove(buffer, buffer+len, buffer_used-len);
		buffer_used -= len;

#ifdef SELFTEST
		// Add stats for memory movement
		Context_Manager *mng=urlManager->FindContextManager(url.GetContextId());

		if(mng)
		{
			URLDD_Stats *stats=mng->GetDataDescriptorForStats(this);

			if(stats)
				stats->AddBytesMemMoved(buffer_used-len);
		}
#endif // SELFTEST
	}
	else
	{
		OP_ASSERT(len == buffer_used);
		buffer_used = 0;
	}
	position += len;

	if (messageTracker.DataConsumedOrAdded(len))
		messageTracker.PostMessageIfBlocked(this); // It was wrong to previously block the MSG_URL_DATA_LOADED since the client consumed something
}

unsigned int URL_DataDescriptor::Grow(unsigned long requested_size, GrowPolicy policy)
{
	// Black magic from 2005. If there are 13 or more free bytes
	// in the buffer - don't grow the buffer, just return its length.
	const int fudge_factor = 6;	// Boy, don't you wish you knew.
    if(!requested_size && buffer_used + sizeof(uni_char)*fudge_factor < buffer_len )
        return(buffer_len);

	// No requested_size and buffer is almost full. Let's do something default,
	// for example grow the buffer by default increment.
	if(!requested_size)
		requested_size = buffer_len + DATADESC_BUFFER_INCREMENT;

	if (policy == GROW_BIG)
	{
		// Do not shrink the buffer on GROW_BIG if smaller size is requested.
		// And do not grow it either.
		if (requested_size <= buffer_len)
			return buffer_len;

		// If growing policy is GROW_BIG - make sure that the buffer will be
		// no shorter than DATADESC_BUFFER_SIZE and aligned by DATADESC_BUFFER_INCREMENT.
		// The code below works best if DATADESC_BUFFER_SIZE itself is aligned
		// by DATADESC_BUFFER_INCREMENT. Usually it is so.
		if(requested_size < DATADESC_BUFFER_SIZE)
			requested_size = DATADESC_BUFFER_SIZE;
		else
		{
			unsigned long mod = requested_size % DATADESC_BUFFER_INCREMENT;
			if (mod != 0)
				requested_size += DATADESC_BUFFER_INCREMENT-mod; // resize to multiple of increment
		}

		OP_ASSERT(requested_size >= DATADESC_BUFFER_SIZE);
#if DATADESC_BUFFER_SIZE % DATADESC_BUFFER_INCREMENT == 0
		OP_ASSERT(requested_size % DATADESC_BUFFER_INCREMENT == 0);
#endif
	}
	else
	{
		OP_ASSERT(policy == GROW_SMALL);

		// Don't allow growing above DATADESC_BUFFER_SIZE on GROW_SMALL.
		// NB! Care should be taken that the buffer size will still not
		// go below buffer_used.
		if (requested_size > DATADESC_BUFFER_SIZE)
			requested_size = DATADESC_BUFFER_SIZE;

		// Never go below buffer_used!
		if (requested_size < buffer_used)
			requested_size = buffer_used;

		// No point to reallocate the buffer if the final size is the same.
		if (requested_size == buffer_len)
			return buffer_len;

		// Do not align buffer size on GROW_SMALL, unlike on GROW_BIG.
	}

	// We are sure that we won't corrupt memory
	// or reallocate the buffer uselessly without changing its size.
	OP_ASSERT(buffer_used <= buffer_len);
	OP_ASSERT(buffer_used <= requested_size);
	OP_ASSERT(requested_size != buffer_len);
	OP_ASSERT(policy == GROW_SMALL || requested_size > buffer_len);

    char *tmp_buffer = OP_NEWA(char, requested_size);
    if( tmp_buffer )
    {
		if(buffer) // Just a small optimization
		{
			op_memmove(tmp_buffer, buffer, buffer_used);

			if(url.GetAttribute(URL::KCachePolicy_NoStore))
				OPERA_cleanse_heap(buffer, buffer_len);
			else
				op_memset(buffer, 0, buffer_len);
			OP_DELETEA(buffer);
		}

		buffer = tmp_buffer;
        buffer_len = requested_size;
    	return(buffer_len);
    }
	else
		return 0;
}

BOOL URL_DataDescriptor::FinishedLoading()
{
	if(sub_desc)
	{
		if(sub_desc->storage && !sub_desc->storage->GetFinished() && (URLStatus) sub_desc->url.GetAttribute(URL::KLoadStatus) == URL_LOADING)
			return FALSE;

		if(CE_decoding.First())
		{
			Data_Decoder *decoder = (Data_Decoder *) CE_decoding.First();

			if(!decoder->Finished())
				return FALSE;
		}
	}
	else if(storage)
	{
		if(!storage->GetFinished() && (URLStatus) url.GetAttribute(URL::KLoadStatus) == URL_LOADING)
			return FALSE;
	}
	return TRUE;
}


BOOL URL_DataDescriptor::RoomForContent() const
{
	return (buffer == NULL || (buffer != NULL && buffer_used < buffer_len));
}

unsigned long URL_DataDescriptor::AddContentL(DataStream *src, const unsigned char *buf, unsigned long len
#if LOCAL_STORAGE_CACHE_LIMIT>0
											  , BOOL more /*= TRUE*/
#endif
											  )
{
	if(!buffer)
	{
		buffer_len =
#if LOCAL_STORAGE_CACHE_LIMIT>0
			(!more && len<DATADESC_BUFFER_SIZE) ? len :
#endif
			DATADESC_BUFFER_SIZE;

		buffer = OP_NEWA_L(char, buffer_len);
		buffer_used = 0;
	}
#ifdef _DEBUG_DD1
	unsigned long want_len = len;
#endif

	if(buffer_used == buffer_len)
		return 0;

	if (src || len > buffer_len - buffer_used)
		len = buffer_len - buffer_used;
	
#ifdef _DEBUG_DD1
	PrintfTofile("urldd1.txt","\nDD Adding Data- %d bytes: %s. Wanted %lu\n",len,DebugGetURLstring(url), want_len);
#endif

	if(src)
	{
		len = src->ReadDataL((unsigned char *) buffer + buffer_used, len);
#ifdef _DEBUG_DD1
		PrintfTofile("urldd1.txt","\nDD Added Data- %d bytes: %s. %lu bytes\n",len,DebugGetURLstring(url), len);
#endif
	}
	else
	{
		op_memcpy(buffer + buffer_used, buf, len);
	}
	buffer_used += len;

	messageTracker.DataConsumedOrAdded(len);

	return len;
}
unsigned long GetFileError(OP_STATUS op_err, URL_Rep *url, const uni_char *operation);

OpFileLength URL_DataDescriptor::AddContentL(OpFileDescriptor *file, OpFileLength len, BOOL &more)
{
	file->SetFilePos(GetPosition() + GetBufSize());
	if(!buffer)
	{
		buffer_len = (unsigned long) (len && len < DATADESC_BUFFER_SIZE ? len + 1 : DATADESC_BUFFER_SIZE);
		buffer = OP_NEWA_L(char, buffer_len);
		buffer_used = 0;
	}
	OP_STATUS op_err = OpStatus::OK;
	OP_MEMORY_VAR OpFileLength bread=0;
	if(OpStatus::IsError(op_err = file->Read(buffer + buffer_used, buffer_len - buffer_used, (OpFileLength *) &bread)))
	{
		url.GetRep()->HandleError(GetFileError(op_err, url.GetRep(),UNI_L("read")));
		return GetBufSize();
	}
	buffer_used += (unsigned long)bread;

	messageTracker.DataConsumedOrAdded((unsigned long)bread);

	// Multimedia files with bread == 0 must have more == FALSE
	if ((bread || !url.GetAttribute(URL::KMultimedia)) && !file->Eof())
		more = TRUE;
	return bread;
}

const char *URL_DataDescriptor::GetCharacterSet()
{
	// The CharacterDecoder is always the first decoder (the last one added)
	Data_Decoder *firstdecoder = (Data_Decoder *) CE_decoding.First();
	return firstdecoder ? firstdecoder->GetCharacterSet() : NULL;
}

void URL_DataDescriptor::StopGuessingCharset()
{
	// The CharacterDecoder is always the first decoder (the last one added)
	Data_Decoder *firstdecoder = (Data_Decoder *) CE_decoding.First();
	if (firstdecoder) firstdecoder->StopGuessingCharset();
}

#ifdef _CACHE_DESCRIPTOR_MIMETYPE
const OpStringC8 URL_DataDescriptor::GetMIME_Type() const
{
	return (content_type_string.HasContent() ? content_type_string : url.GetAttribute(URL::KMIME_Type));
}
#endif

int URL_DataDescriptor::GetFirstInvalidCharacterOffset()
{
	if (Data_Decoder* decoder = GetFirstDecoder())
		if (decoder->IsA(UNI_L("CharacterDecoder")))
			return static_cast<CharacterDecoder *>(decoder)->GetFirstInvalidCharacterOffset();

	return -1;
}

Data_Decoder*	URL_DataDescriptor::GetFirstDecoder()
{
	return (Data_Decoder*) CE_decoding.First();
}

OP_STATUS URL_DataDescriptor::SetPosition(OpFileLength pos)
{
	// If the position is within our buffer, consume the bytes leading up to it
	if (pos > position && pos < (OpFileLength)(position + buffer_used))
	{
		ConsumeData((unsigned) (pos - position));
		return OpStatus::OK;
	}

	if (storage && storage->GetCacheType() == URL_CACHE_STREAM)
	{
		url.StopLoading(mh);
#ifdef URL_ENABLE_HTTP_RANGE_SPEC
		url.SetAttribute(URL::KHTTPRangeStart, (void *) &pos);
#endif
		url.Load(mh, url.GetAttribute(URL::KReferrerURL), FALSE, FALSE, FALSE);
		position = pos;
		buffer_used = 0;

		return OpStatus::OK;
	}
	else if (storage)
	{
		// Check if the position is available
		BOOL available;
		OpFileLength available_size;

		storage->GetPartialCoverage(pos, available, available_size, FALSE);
		if (available)
		{
			position = pos;
			buffer_used = 0;

			return OpStatus::OK;
		}
		else
			return OpStatus::ERR_OUT_OF_RANGE;
	}
	else
	{
		OpFileLength registered_len = 0;
		url.GetAttribute(URL::KContentLoaded, &registered_len);
		if (position != pos && registered_len >=pos)
		{
			position = pos;
			buffer_used = 0;

			return OpStatus::OK;
		}
		else
			return OpStatus::ERR_OUT_OF_RANGE;
	}
}

OP_STATUS URL_DataDescriptor::GetSortedCoverage(OpAutoVector<StorageSegment> &out_segments, OpFileLength start, OpFileLength len, BOOL merge)
{
	return (storage) ? storage->GetSortedCoverage(out_segments, start, len, merge) : OpStatus::ERR_NULL_POINTER;
}

MessageTracker::MessageTracker()
{
	nothing_consumed_or_added = FALSE;
	block_msg_data_loaded = FALSE;
	msg_data_loaded_was_blocked = FALSE;
	msg_data_loaded_delay = 0;
}

void MessageTracker::MaybeStartBlocking(BOOL storage_has_more)
{
	block_msg_data_loaded = nothing_consumed_or_added && !storage_has_more;
	nothing_consumed_or_added = TRUE;
	msg_data_loaded_was_blocked = FALSE;
}

void MessageTracker::StopBlocking()
{
	block_msg_data_loaded = FALSE;
}

void MessageTracker::ClearBlockingState()
{
	block_msg_data_loaded = FALSE;
	msg_data_loaded_was_blocked = FALSE;
}

BOOL MessageTracker::BlockMessage(OpMessage Msg, unsigned long delay)
{
	if (Msg == MSG_URL_DATA_LOADED && block_msg_data_loaded)
	{
		msg_data_loaded_was_blocked = TRUE;
		msg_data_loaded_delay = delay;
		return TRUE;
	}
	return FALSE;
}

BOOL MessageTracker::DataConsumedOrAdded(unsigned long amount)
{
	if (amount != 0)
	{
		nothing_consumed_or_added = FALSE;
		return TRUE;
	}
	return FALSE;
}

void MessageTracker::PostMessageIfBlocked(URL_DataDescriptor *dd)
{
	if (msg_data_loaded_was_blocked)
	{
		msg_data_loaded_was_blocked = FALSE;
		block_msg_data_loaded = FALSE;
		if (msg_data_loaded_delay == 0)
		{
			if (!dd->PostedMessage()) // ...as long as we didn't already post one
				dd->PostMessage(MSG_URL_DATA_LOADED, dd->GetURL().Id(FALSE), 0);
		}
		else
		{
			if (!dd->PostedMessage() && !dd->PostedDelayedMessage())
				dd->PostDelayedMessage(MSG_URL_DATA_LOADED, dd->GetURL().Id(FALSE), 0, msg_data_loaded_delay);
		}
	}
}
