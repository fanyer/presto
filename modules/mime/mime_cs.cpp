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

#ifdef _MIME_SUPPORT_

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/encodings/utility/charsetnames.h"

#include "modules/url/url2.h"

//#include "url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_pd.h"
#include "modules/cache/url_cs.h"
#include "modules/cache/url_i18n.h"
#include "modules/mime/mimedec2.h"
#include "modules/mime/mime_cs.h"

#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
#include "modules/url/url_man.h"
#endif

#include "modules/olddebug/timing.h"

#ifdef _DEBUG
#ifdef YNP_WORK

#include "modules/olddebug/tstdump.h"

#define _DEBUG_DD
#define _DEBUG_DD1
#endif
#endif
#include "modules/util/opfile/opfile.h"

#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define MIME_ERRSTR(p,x) Str::##p##_##x
#else
#define MIME_ERRSTR(p,x) x
#endif



BOOL URL_Rep::GetAttachment(int i, URL &url)
{
	if(storage)
		return storage->GetAttachment(i,url);

	url = URL();
	return FALSE;
}

BOOL URL_DataStorage::GetAttachment(int i, URL &url)
{
	if(storage)
		return storage->GetAttachment(i,url);

	url = URL();
	return FALSE;
}

BOOL URL_Rep::IsMHTML() const
{
	return storage ? storage->IsMHTML() : FALSE;
}

BOOL URL_DataStorage::IsMHTML() const
{
	return storage ? storage->IsMHTML() : FALSE;
}

BOOL DecodedMIME_Storage::IsMHTML() const
{
	return decoder && decoder->GetContentTypeID() == MIME_Multipart_Related;
}

URL	URL_Rep::GetMHTMLRootPart()
{
	return storage ? storage->GetMHTMLRootPart() : URL();
}

URL	URL_DataStorage::GetMHTMLRootPart()
{
	return storage ? storage->GetMHTMLRootPart() : URL();
}

URL DecodedMIME_Storage::GetMHTMLRootPart()
{
	return decoder ? decoder->GetRelatedRootPart() : URL();
}

MIME_attach_element_url::MIME_attach_element_url(URL &associate_url, BOOL displd, BOOL isicon)
 : attachment(associate_url), displayed(displd), is_icon(isicon)
{
}

MIME_attach_element_url::~MIME_attach_element_url()
{
	if(attachment->GetAttribute(URL::KIsAlias))
	{
		URL temp;
		TRAPD(op_err, attachment->SetAttributeL(URL::KAliasURL, temp));
		OpStatus::Ignore(op_err); // No problem, unloading URL and resetting alias can't fail
	}
}

Decode_Storage::Decode_Storage(URL_Rep *url_rep) 
#ifndef RAMCACHE_ONLY
: Persistent_Storage(url_rep), 
#else
: Memory_Only_Storage(url_rep),
#endif // !RAMCACHE_ONLY
  script_embed_policy(MIME_EMail_ScriptEmbed_Restrictions)
{
	
	switch((URLType) url->GetAttribute(URL::KType))
	{
	case URL_HTTP:
	case URL_HTTPS:
	case URL_FTP:
		script_embed_policy = MIME_Remote_ScriptEmbed_Restrictions;
		break;
	case URL_FILE:
		script_embed_policy = MIME_Local_ScriptEmbed_Restrictions;
		break;
	}
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	valid_mhtml_archive = MIME_Decoder::IsValidMHTMLArchiveURL(url); 
	if (valid_mhtml_archive)
		script_embed_policy = MIME_No_ScriptEmbed_Restrictions;
#endif
	url->SetAttribute(URL::KSuppressScriptAndEmbeds, script_embed_policy);
}

#ifndef RAMCACHE_ONLY

OP_STATUS Decode_Storage::Construct(URL_Rep *url_rep)
{
	return Persistent_Storage::Construct();
}

Decode_Storage* Decode_Storage::Create(URL_Rep *url_rep)
{
	Decode_Storage* ds = OP_NEW(Decode_Storage, (url_rep));
	if (!ds)
		return NULL;
	if (OpStatus::IsError(ds->Construct(url_rep)))
	{
		OP_DELETE(ds);
		return NULL;
	}
	return ds;	
}

#endif //!RAMCACHE_ONLY

DecodedMIME_Storage::DecodedMIME_Storage(URL_Rep *url_rep) 
: Decode_Storage(url_rep),decoder(NULL),writing_to_self(FALSE), prefer_plain(FALSE),ignore_warnings(FALSE), body_part_count(0), decode_only(FALSE)
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
,context_id(0)
#endif

{
	data.SetIsFIFO();
	SetMemoryOnlyStorage(!!url->GetAttribute(URL::KCachePolicy_NoStore));
}


DecodedMIME_Storage::~DecodedMIME_Storage()
{
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
	if(context_id)
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
		if (!valid_mhtml_archive)
#endif
			urlManager->RemoveContext(context_id, TRUE);
#endif

	OP_DELETE(decoder);
}

OP_STATUS DecodedMIME_Storage::Construct(URL_Rep *url_rep)
{
#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
	URL parentURL(url_rep, (char*)NULL);
	if (urlManager->FindContextIdByParentURL(parentURL, context_id) == FALSE)
	{
		context_id = urlManager->GetNewContextID();
		TRAPD(op_err, urlManager->AddContextL(context_id, OPFILE_ABSOLUTE_FOLDER, OPFILE_ABSOLUTE_FOLDER, OPFILE_ABSOLUTE_FOLDER, OPFILE_ABSOLUTE_FOLDER, parentURL, TRUE));
		if(OpStatus::IsError(op_err))
			context_id = 0;
	}
	else
	{
		Context_Manager *cm = urlManager->FindContextManager(context_id);

		if(cm)
			cm->FreeUnusedResources(TRUE);
	}

	URL base_alias = urlManager->GetURL(url_rep->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr(), context_id);
	TRAPD(op_err, url_rep->SetAttributeL(URL::KBaseAliasURL, base_alias));
#endif

#ifdef RAMCACHE_ONLY
	return OpStatus::OK;
#else
	return Decode_Storage::Construct(url_rep);
#endif
}	

URL_DataDescriptor *DecodedMIME_Storage::GetDescriptor(MessageHandler *mh, BOOL get_raw_data, BOOL get_decoded_data, Window *window, URLContentType override_content_type, unsigned short override_charset_id, BOOL get_original_content, unsigned short parent_charset_id)
{
	return Decode_Storage::GetDescriptor(mh, TRUE, TRUE, NULL, override_content_type); // No encoding is used, content is always UTF-16
}

void DecodedMIME_Storage::CreateDecoder(const unsigned char *src, unsigned long src_len)
{
	if(src == NULL || src_len == 0)
		return;

	OP_MEMORY_VAR URLType type;

	switch((URLType) url->GetAttribute(URL::KType))
	{
	case URL_SNEWS:
		type = URL_SNEWSATTACHMENT;
		break;
	case URL_NEWS:
		type = URL_NEWSATTACHMENT;
		break;
	default:
		type = URL_ATTACHMENT;
		break;
	}

#ifdef URL_SEPARATE_MIME_URL_NAMESPACE
	TRAPD(op_err, decoder = MIME_Decoder::CreateDecoderL(NULL, src, src_len, type, url, context_id));
#else
	TRAPD(op_err, decoder = MIME_Decoder::CreateDecoderL(NULL, src, src_len, type, url));
#endif
	OpStatus::Ignore(op_err); // handled below
		
	if(!decoder)
	{
		OP_STATUS st;
		TRAP(st, data.ResizeL(0));
		url->HandleError(MIME_ERRSTR(SI,ERR_CACHE_INTERNAL_ERROR));
		return;
	}

	decoder->SetScriptEmbed_Restrictions(script_embed_policy);
	decoder->SetUseNoStoreFlag(GetMemoryOnlyStorage());
	decoder->SetPreferPlaintext(prefer_plain);
	decoder->SetIgnoreWarnings(ignore_warnings);
}

void DecodedMIME_Storage::WriteToDecoder(const unsigned char *source, unsigned long source_len)
{
	OP_STATUS op_err;
	const unsigned char * OP_MEMORY_VAR src = source;
	OP_MEMORY_VAR unsigned long src_len = source_len;

	if(!decoder)
	{
		TRAP(op_err, data.WriteDataL(src, src_len));

		src = data.GetDirectPayload();
		src_len = data.GetLength();

		OP_MEMORY_VAR unsigned long header_len = 0;
		const unsigned char *current = src;
		BOOL found_end_of_header = FALSE;

		while(header_len < src_len)
		{
			header_len++;
			if(*(current++) == '\n')
			{
				if(header_len < src_len)
				{
					if(*current == '\r')
					{
						current ++;
						if(header_len >= src_len)
							break;
						header_len ++;
					}
					if(*current == '\n')
					{
						header_len++;
						found_end_of_header = TRUE;
						break;
					}
				}
			}
		}

		if(!found_end_of_header )
			return;

		CreateDecoder(src, header_len);
		if(!decoder)
			return;

		decoder->SetPreferPlaintext(prefer_plain);
		decoder->SetIgnoreWarnings(ignore_warnings);
		decoder->SetUseNoStoreFlag(!!url->GetAttribute(URL::KCachePolicy_NoStore));

		TRAP(op_err, decoder->LoadDataL(src+ header_len,src_len - header_len));
		if(OpStatus::IsError(op_err))
		{
			url->HandleError(MIME_ERRSTR(SI,ERR_CACHE_INTERNAL_ERROR));
			return;
		}
		TRAP(op_err, data.CommitSampledBytesL(src_len));
		if(OpStatus::IsError(op_err))
		{
			url->HandleError(MIME_ERRSTR(SI,ERR_CACHE_INTERNAL_ERROR));
			return;
		}
		return;
	}

	decoder->SetForceCharset(charset_id);
	TRAP(op_err, decoder->LoadDataL(src,src_len));

	if(!decode_only)
	{
		writing_to_self = TRUE;
		URL tmp(url, (char *) NULL);
		TRAP(op_err, decoder->RetrieveDataL(tmp, this));
		writing_to_self = FALSE;
		if((URLType) url->GetAttribute(URL::KType) == URL_EMAIL && url->GetDataStorage())
		{
			URL_DataStorage *url_ds = url->GetDataStorage();
			if(!url_ds->GetAttribute(URL::KHeaderLoaded))
			{
				url_ds->BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), 0, MH_LIST_ALL);
				url_ds->SetAttribute(URL::KHeaderLoaded,TRUE);
			}

			url_ds->BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
		}
	}
}

void DecodedMIME_Storage::SetBigAttachmentIcons(BOOL isbig)
{
	if (decoder)
		decoder->SetBigAttachmentIcons(isbig);
}

void DecodedMIME_Storage::SetFinished(BOOL force)
{
	OP_STATUS op_err;
	if(!writing_to_self && !decoder && data.GetLength() != 0)
	{
		CreateDecoder(data.GetDirectPayload(),data.GetLength());
		TRAP(op_err, data.CommitSampledBytesL(data.GetLength()));
	}

	if(writing_to_self || !decoder)
	{
		Decode_Storage::SetFinished(force);
		if((URLStatus) url->GetAttribute(URL::KLoadStatus) == URL_LOADING)
			url->SetAttribute(URL::KLoadStatus, URL_LOADED);
	}
	else if(decoder)
	{
		TRAP(op_err, decoder->FinishedLoadingL());
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
		valid_mhtml_archive = decoder->IsValidMHTMLArchive(); 
#endif

		if(!decode_only)
		{
			writing_to_self = TRUE;

			URL tmp(url, (char *) NULL);

			TRAP(op_err, decoder->RetrieveDataL(tmp, this));
			writing_to_self = FALSE;
		}
		else
		{
			decoder->RetrieveAttachementList(this);
		}
		Decode_Storage::SetFinished(force);
	}
}

unsigned long DecodedMIME_Storage::RetrieveData(URL_DataDescriptor *target,BOOL &more)
{
	more = FALSE;
	
	if(target == NULL)
		return 0;
	
	if(!GetFinished())
		ProcessData();

	unsigned long ret = Decode_Storage::RetrieveData(target, more);
	if(!GetFinished())
	{
		// if(!more && (url->Status(FALSE) == URL_LOADING || (decoder &&  decoder->IsFinished())))
		if((decoder &&  !decoder->IsFinished()) || more || (URLStatus) url->GetAttribute(URL::KLoadStatus) == URL_LOADING)
		{
			if(!target->PostedMessage())
			{
					target->PostDelayedMessage(MSG_URL_DATA_LOADED, url->GetID(), 0,100);
			}
			more = TRUE;
		}
#if 0
		else
			int debug = 1;
#endif
	}

	return ret;
}

BOOL DecodedMIME_Storage::Purge()
{
	attached_urls.Clear();

	OP_DELETE(decoder);
	decoder = NULL;

	Decode_Storage::Purge();

	return TRUE;
}

BOOL DecodedMIME_Storage::Flush()
{
	OP_DELETE(decoder);
	decoder = NULL;

	return Decode_Storage::Flush();
}

void DecodedMIME_Storage::AddMIMEAttachment(URL &associate_url, BOOL displayed, BOOL isicon, BOOL isbodypart)
{
	if(associate_url.IsEmpty())
		return;

	MIME_attach_element_url *attach = attached_urls.First();
	while(attach)
	{
		if(attach->attachment->Id() == associate_url.Id())
		{
			attach->displayed = displayed;
			break;
		}
		attach = attach->Suc();
	}

	if(!attach)
	{
		MIME_attach_element_url *attach = OP_NEW(MIME_attach_element_url, (associate_url, displayed, isicon));
		if(attach)
			attach->Into(&attached_urls);
		else
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
	
	if(isbodypart)
		body_part_count++;
}

BOOL DecodedMIME_Storage::GetAttachment(int i, URL &url)
{
	MIME_attach_element_url *attach = attached_urls.First();
	while(attach)
	{
		if(!attach->is_icon)
		{
			if(!i)
				break;
			i--;
		}

		attach = attach->Suc();
	}

	url = (attach ? attach->attachment.GetURL() : URL());
	return attach && attach->displayed;
}


MIME_DecodeCache_Storage::MIME_DecodeCache_Storage(URL_Rep *url_rep, Cache_Storage *src)
: DecodedMIME_Storage(url_rep)
{
	SetContentType(URL_XML_CONTENT);
	source = NULL;
	desc = NULL;

	// Do not take over the source cache element unless initialization was successful
#ifdef RAMCACHE_ONLY
	source = src;
#endif
}

MIME_DecodeCache_Storage::~MIME_DecodeCache_Storage()
{
	OP_DELETE(source);
	OP_DELETE(desc);
}

OP_STATUS MIME_DecodeCache_Storage::Construct(URL_Rep *url_rep, Cache_Storage *src)
{
	RETURN_IF_ERROR(DecodedMIME_Storage::Construct(url_rep));
	source = src;
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	BOOL prev_decode_only = GetDecodeOnly();
	SetDecodeOnly(TRUE);
	ProcessData(); // Process data early to make the redirect while we have a MessageHandler
	SetDecodeOnly(prev_decode_only);
#endif
	return OpStatus::OK;
}

URL_DataDescriptor *MIME_DecodeCache_Storage::GetDescriptor(MessageHandler *mh, BOOL get_raw_data, BOOL get_decoded_data, Window *window, URLContentType override_content_type, unsigned short override_charset_id, BOOL get_original_content, unsigned short parent_charset_id)
{
	if(get_original_content && source)
	{
		unsigned short temp_charset = 0; 
		OpStatus::Ignore(g_charsetManager->GetCharsetID("iso-8859-1", &temp_charset));
		g_charsetManager->IncrementCharsetIDReference(temp_charset);
		URL_DataDescriptor *retval = source->GetDescriptor(mh, FALSE, TRUE, window, URL_TEXT_CONTENT, temp_charset, TRUE, parent_charset_id);
		g_charsetManager->DecrementCharsetIDReference(temp_charset);
		return retval;
	}

	return DecodedMIME_Storage::GetDescriptor(mh, TRUE, TRUE, NULL, override_content_type, override_charset_id); // No encoding is used, content is always UTF-16
}


/*
MIME_DecodeCache_Storage* MIME_DecodeCache_Storage::Create(URL_Rep *url_rep, Cache_Storage *src)
{
	MIME_DecodeCache_Storage* mdcs = new MIME_DecodeCache_Storage(url_rep, src);
	if (!mdcs)
		return NULL;
	if (OpStatus::IsError(mdcs->Construct(url_rep, src)))
	{
		delete mdcs;
		return NULL;
	}
	return mdcs;	
}
*/

OP_STATUS MIME_DecodeCache_Storage::StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position)
{
	 OP_ASSERT(start_position == FILE_LENGTH_NONE);

	if(GetWritingToSelf())
		RETURN_IF_ERROR(DecodedMIME_Storage::StoreData(buffer, buf_len));
	else if(source)
	{
		RETURN_IF_ERROR(source->StoreData(buffer, buf_len));
		ProcessData();
	}
	else
		WriteToDecoder(buffer, buf_len);

	return OpStatus::OK;
}

void MIME_DecodeCache_Storage::SetFinished(BOOL force)
{
	if(source)
		source->SetFinished(force);
	//else
	DecodedMIME_Storage::SetFinished(force);
}

void  MIME_DecodeCache_Storage::ProcessData()
{
	if(!source)
		return;
	
	if(!desc)
	{
		desc = source->GetDescriptor(NULL, TRUE, TRUE);
		if(!desc)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			url->HandleError(MIME_ERRSTR(SI,ERR_CACHE_INTERNAL_ERROR));
			return;
		}
	}
	
	if(desc)
	{
		BOOL more1 = TRUE;
		while (more1 && (desc->RetrieveData(more1)))
		{
			WriteToDecoder((unsigned char *) desc->GetBuffer(), desc->GetBufSize());
			desc->ConsumeData(desc->GetBufSize());
		}

		if(!more1 && (URLStatus) url->GetAttribute(URL::KLoadStatus) != URL_LOADING)
			DecodedMIME_Storage::SetFinished(TRUE);
	}
}

BOOL MIME_DecodeCache_Storage::Purge()
{
	if(desc)
	{
		OP_DELETE(desc);
		desc = NULL;
	}

	if(source)
		source->Purge();
		
	DecodedMIME_Storage::Purge();

	return TRUE;
}

BOOL MIME_DecodeCache_Storage::Flush()
{
	if(!GetFinished())
		return FALSE;

	if(desc)
	{
		OP_DELETE(desc);
		desc = NULL;
	}

	BOOL flushed;

	flushed = DecodedMIME_Storage::Flush();

	if(source)
		flushed = source->Flush();

	return flushed;
}

const OpStringC MIME_DecodeCache_Storage::FileName(OpFileFolder &folder, BOOL get_original_body) const
{
	if(get_original_body && source)
	{
		OpStringC rv0(source->FileName(folder, get_original_body));

		return rv0;
	}

	OpStringC rv(DecodedMIME_Storage::FileName(folder, get_original_body));

	return rv;
}

OP_STATUS MIME_DecodeCache_Storage::FilePathName(OpString &name, BOOL get_original_body) const
{
	if(get_original_body && source)
		return source->FilePathName(name, get_original_body);

	return DecodedMIME_Storage::FilePathName(name, get_original_body);
}

unsigned long MIME_DecodeCache_Storage::SaveToFile(const OpStringC &out_filename)
{
	if(source)
		return source->SaveToFile(out_filename);

	return DecodedMIME_Storage::SaveToFile(out_filename);
}



#endif
