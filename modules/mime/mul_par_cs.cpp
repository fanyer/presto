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

#include "modules/hardcore/mh/messages.h"
#include "modules/viewers/viewers.h"

#include "modules/url/url2.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/formats/argsplit.h"
#include "modules/mime/mul_par_cs.h"
#include "modules/mime/mimeutil.h"

#include "modules/encodings/detector/charsetdetector.h"
#include "modules/encodings/utility/charsetnames.h"

#include "modules/olddebug/tstdump.h"

//#define DEBUG_MULPART

Multipart_CacheStorage::Multipart_CacheStorage(URL_Rep *url)
 : StreamCache_Storage(url)
{
#ifdef DEBUG_MULPART
	PrintfTofile("mulpart1.txt", "\nStarting: %s\n", url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
#endif
	loading_item = NULL;
	desc = NULL;
	first_bodypart_created = FALSE;
	bodypart_status = Multipart_NotStarted;
}

Multipart_CacheStorage::~Multipart_CacheStorage()
{
	loading_item = NULL;
	OP_DELETE(desc);
	desc = NULL;
}

void Multipart_CacheStorage::ConstructL(Cache_Storage * initial_storage, OpStringS8 &content_encoding)
{
	StreamCache_Storage::ConstructL(initial_storage, content_encoding);

	desc = StreamCache_Storage::GetDescriptor(NULL, TRUE, FALSE, NULL);
	if(GetContentEncoding().HasContent())
	{
		OpStackAutoPtr<URL_DataDescriptor> desc1(OP_NEW_L(URL_DataDescriptor, (url, NULL)));
		LEAVE_IF_ERROR(desc1->Init(TRUE));
#ifdef _HTTP_COMPRESS
		desc1->SetupContentDecodingL(GetContentEncoding().CStr());
#endif
		
		if(desc1->GetFirstDecoder() != NULL)
		{
			desc1->SetSubDescriptor(desc);
			desc = desc1.release();
			AddDescriptor(desc);
		}
		// Auto cleanup
	}

	if(desc == NULL)
		LEAVE(OpStatus::ERR_NULL_POINTER);
}

void Multipart_CacheStorage::CreateNextElementL(URL new_target, const OpStringC8 &content_type_str, OpStringS8 &content_encoding, BOOL no_store)
{
	loading_item = NULL;
	if(new_target.IsEmpty())
	{
		ParameterList type(KeywordIndex_HTTP_General_Parameters);
		ANCHOR(ParameterList, type);
		URLContentType new_content_type = URL_UNDETERMINED_CONTENT;
		unsigned short new_charset_id = 0;
		
		type.SetValueL(content_type_str.CStr(),PARAM_SEP_SEMICOLON|PARAM_STRIP_ARG_QUOTES);
		
		Parameters *element = type.First();
		Parameters *content_type_elm = NULL;
		if(element)
		{
			do{
				element->SetNameID(HTTP_General_Tag_Unknown);
				
				ANCHORD(OpStringS8, stripped_name);
				BOOL illegal_name=FALSE;
				
				if(element->Name())
				{
					stripped_name.SetL(element->Name());
					int i;
					for(i=0;((unsigned char) stripped_name[i])>=32 && ((unsigned char) stripped_name[i])<=127;i++)
						;
					if (stripped_name[i]!=0)
						illegal_name=TRUE;
				}
				
				if(illegal_name)
				{
					new_content_type = URL_UNKNOWN_CONTENT;
					break;
				}

				OpStringC8 ctype(element->Name());
				content_type_elm = element;

				if (Viewer *v = g_viewers->FindViewerByMimeType(ctype))
					new_content_type = v->GetContentType();
				else
					new_content_type = URL_UNKNOWN_CONTENT;

				if(!ctype.IsEmpty() && 
					(strni_eq(ctype.CStr(), "TEXT/", 5) ||
					 strni_eq(ctype.CStr(), "APPLICATION/XML", 15) ||
					 strni_eq(ctype.CStr(), "APPLICATION/XHTML", 17)))
					element = type.GetParameterByID(HTTP_General_Tag_Charset, PARAMETER_ASSIGNED_ONLY);
				else
					element = NULL;
				
				if(element)
				{
					OpStringC8 cset(element->Value());
					new_charset_id = (cset.HasContent() ? g_charsetManager->GetCharsetIDL(cset.CStr()) : 0);
				}
			}while(0);
		}

		// Use cache elements
		OpStackAutoPtr<Cache_Storage> new_item(NULL);

#ifndef RAMCACHE_ONLY
		if(no_store)
#endif
			new_item.reset(OP_NEW_L(Memory_Only_Storage, (url)));
#ifndef RAMCACHE_ONLY
		else
			new_item.reset(OP_NEW_L(Persistent_Storage, (url)));
#endif

		new_item->TakeOverContentEncoding(content_encoding);
		if(content_type_elm)
			new_item->SetMIME_TypeL(content_type_elm->Name());
		new_item->SetContentType(new_content_type);
		new_item->SetCharsetID(new_charset_id);

		loading_item = OP_NEW_L(MultipartStorage_Item, (new_item.get()));
#ifdef DEBUG_MULPART
		PrintfTofile("mulpart.txt", "\nCreated new bodypart for %s\n", url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
		PrintfTofile("mulpart1.txt", "\nCreated new bodypart for %s\n", url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
		//PrintfTofile("msg.txt", "New bodypart %08x\n", url->GetID());
#endif

		loading_item->Into(&unused_cache_items);
		new_item.release();

		if((used_cache_items.Empty() || bodypart_status == Multipart_BoundaryRead) && loading_item->Pred() == NULL)
		{
			SetMultipartStatus(Multipart_HeaderLoaded);
			loading_item->header_loaded_sent = TRUE;
		}

		first_bodypart_created = TRUE;
	}
#if defined(MIME_ALLOW_MULTIPART_CACHE_FILL) || defined(WBMULTIPART_MIXED_SUPPORT)
	else
	{
		current_target = new_target;
		URL_DataStorage *ct_ds = current_target->GetRep()->GetDataStorage();
		if (ct_ds)
		{
			ct_ds->ResetCache();
			ct_ds->BroadcastMessage(MSG_HEADER_LOADED, current_target->Id(), current_target->GetAttribute(URL::KIsFollowed) ? 0 : 1, MH_LIST_NOT_INLINE_ONLY | MH_LIST_NOT_LOAD_SILENT_ONLY);
		}
		current_target->SetAttributeL(URL::KHTTP_Response_Code, HTTP_OK);
		current_target->SetAttributeL(URL::KCachePolicy_NoStore, no_store);
		current_target->SetAttributeL(URL::KHTTPEncoding, content_encoding);
		current_target->SetAttributeL(URL::KMIME_ForceContentType, content_type_str);
		current_target->SetAttributeL(URL::KLoadStatus, URL_LOADING);
		
		InheritExpirationDataL(current_target, url);
	}
#endif

}

void Multipart_CacheStorage::WriteDocumentDataL(const unsigned char *buffer, unsigned long buf_len, BOOL more)
{
#if defined(MIME_ALLOW_MULTIPART_CACHE_FILL) || defined(WBMULTIPART_MIXED_SUPPORT)
	if(!current_target->IsEmpty())
	{
#ifdef DEBUG_MULPART
		{
			OpString8 name1;
			current_target->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, name1);
			PrintfTofile("mulpart.txt", "\nMultipart writing data for %s to %s \n", url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr(),   name1);
			DumpTofile(buffer,buf_len,"Bodypart data","mulpart.txt");
		}
#endif
		current_target->WriteDocumentData(URL::KNormal, (const char *) buffer, buf_len);
		if(!more)
		{
			current_target->WriteDocumentDataFinished();
			current_target.UnsetURL();
		}
	}
	else
#endif
		if(loading_item && loading_item->item)
	{
		if(buf_len || !more)
		{
#ifdef DEBUG_MULPART
			//PrintfTofile("msg.txt", "New Data %08x %s\n", url->GetID(), (more ? "more" : "finished"));
			PrintfTofile("mulpart.txt", "\nMultipart writing data to element for %s\n", url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
			DumpTofile(buffer,buf_len,"Bodypart data","mulpart.txt");
			PrintfTofile("mulpart1.txt", "\nMultipart writing data to element for %s\n", url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
#endif
			LEAVE_IF_ERROR(loading_item->item->StoreData(buffer, buf_len));
			if(((used_cache_items.Empty() || bodypart_status == Multipart_HeaderLoaded) && loading_item->Pred() == NULL) || used_cache_items.HasLink(loading_item))
				SetMultipartStatus(Multipart_BodyContent);
		}

		if(!more)
		{
			loading_item->item->SetFinished(TRUE);
#ifdef DEBUG_MULPART
			PrintfTofile("mulpart.txt", "\nSignaling ready %s\n", url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
			PrintfTofile("mulpart1.txt", "\nSignaling ready %s\n", url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
#endif
			if(((used_cache_items.Empty() || bodypart_status == Multipart_BodyContent) && loading_item->Pred() == NULL) || 
						used_cache_items.HasLink(loading_item))
				SetMultipartStatus(Multipart_BoundaryRead);
			loading_item = NULL;
		}
	}
}

OP_STATUS Multipart_CacheStorage::StoreData(const unsigned char *buffer, unsigned long buf_len, OpFileLength start_position)
{
#ifdef DEBUG_MULPART
	PrintfTofile("mulpart.txt", "\nMultipart Adding data for %s\n", url->GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI).CStr());
	DumpTofile(buffer,buf_len,"Bodypart data","mulpart.txt");
#endif
	OP_STATUS op_err;
	
	op_err = StreamCache_Storage::StoreData(buffer, buf_len);
	if(OpStatus::IsSuccess(op_err))
		TRAP(op_err, ProcessDataL());
	return op_err;
}


URL_DataDescriptor *Multipart_CacheStorage::GetDescriptor(MessageHandler *mh, BOOL get_raw_data, BOOL get_decoded_data, Window *window,
														  URLContentType override_content_type, unsigned short override_charset_id, 
														  BOOL get_original_content, unsigned short parent_charset_id)
{
#ifdef DEBUG_MULPART
	//PrintfTofile("msg.txt", "New descriptor %08x\n", url->GetID());
#endif
	MultipartStorage_Item *item = used_cache_items.First();

	while(item)
	{
		MultipartStorage_Item *current_item = item;
		item = item->Suc();
		
		if(!current_item->item || current_item->item->Empty())
		{
			if(current_item == loading_item || (item == NULL && unused_cache_items.Empty()/* && !GetFinished()*/))
			{
				item = current_item;
				break;
			}
			OP_DELETE(current_item);
		}
	}

	if(!item)
	{
		item = unused_cache_items.First();

		if(!item)
			return NULL;

		item->Out();
		item->Into(&used_cache_items);
		if(unused_cache_items.Empty() && GetFinished())
		{
			url->StopLoading(NULL);
			url->SetAttribute(URL::KLoadStatus, URL_LOADED);
		}
	}
#ifdef DEBUG_MULPART
	PrintfTofile("mulpart1.txt", "\nGetting Descriptor %s\n", url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
#endif
	URL_DataDescriptor *new_desc = item->item->GetDescriptor(mh, get_raw_data, get_decoded_data, window, override_content_type, override_charset_id, get_original_content, parent_charset_id);

	if(new_desc)
		new_desc->SetMultipartIterable();

	return new_desc;
}

OpFileLength Multipart_CacheStorage::ContentLoaded(BOOL force)
{
	MultipartStorage_Item *item = used_cache_items.Last();
	
	return (item && item->item ? item->item->ContentLoaded(force) : 0);
}

BOOL	Multipart_CacheStorage::Flush()
{
	MultipartStorage_Item *item = loading_item;
	loading_item = NULL;

	if(item && item->item)
		return item->item->Flush();

	return TRUE;
}

BOOL	Multipart_CacheStorage::Purge()
{
	used_cache_items.Clear();
	unused_cache_items.Clear();
#if defined(MIME_ALLOW_MULTIPART_CACHE_FILL) || defined(WBMULTIPART_MIXED_SUPPORT)
	current_target.UnsetURL();
#endif
	StreamCache_Storage::Purge();

	return TRUE;
}

void	Multipart_CacheStorage::TruncateAndReset()
{
	Purge(); 
	ResetForLoading();
}

void	Multipart_CacheStorage::SetFinished(BOOL force )
{
	if(!GetFinished())
	{
		StreamCache_Storage::SetFinished(force);
		ProcessDataL();
		if(loading_item && loading_item->item)
			loading_item->item->SetFinished(TRUE);

		if(unused_cache_items.Empty())
		{
			url->GetDataStorage()->BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
			url->SetAttribute(URL::KLoadStatus, URL_LOADED);
		}
	}
}

void	Multipart_CacheStorage::MultipartSetFinished(BOOL force )
{
#if defined(MIME_ALLOW_MULTIPART_CACHE_FILL) || defined(WBMULTIPART_MIXED_SUPPORT)
	if(!current_target->IsEmpty())
	{
		current_target->WriteDocumentDataFinished();
	}
	else
#endif
		if(loading_item && loading_item->item)
	{
		loading_item->item->SetFinished(force);
		loading_item = NULL;
#ifdef DEBUG_MULPART
		PrintfTofile("mulpart.txt", "\nSignaling ready %s\n", url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
#endif
	}
	if(unused_cache_items.Cardinal() <= 1 && GetFinished() && 
				(used_cache_items.Empty() || 
					!(used_cache_items.Last())->item ||
					(used_cache_items.Last())->item->Empty()))
	{
		url->GetDataStorage()->BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
		url->StopLoading(NULL);
		url->SetAttribute(URL::KLoadStatus, URL_LOADED);
	}
}

BOOL Multipart_CacheStorage::IsLoadingPart()
{
#if defined(MIME_ALLOW_MULTIPART_CACHE_FILL) || defined(WBMULTIPART_MIXED_SUPPORT)
	if(!current_target->IsEmpty())
		return TRUE;
#endif
	return loading_item != NULL;
}

URLContentType	Multipart_CacheStorage::GetContentType() const
{
	if(first_bodypart_created)
	{
		MultipartStorage_Item  *storage = used_cache_items.Last();
		
		if(!storage || !storage->item || (storage->item->Cardinal() == 0 && !unused_cache_items.Empty()) )
			storage = unused_cache_items.First();
		
		
		return (storage && storage->item? storage->item->GetContentType() : URL_UNDETERMINED_CONTENT);
	}
	return StreamCache_Storage::GetContentType();
}

const OpStringC8 Multipart_CacheStorage::GetMIME_Type() const
{
	if(first_bodypart_created)
	{
		MultipartStorage_Item  *storage = used_cache_items.Last();
		
		if(!storage || !storage->item)
			storage = unused_cache_items.First();
		
		OpStringC8 def_ret;
		OpStringC8 ret(storage && storage->item? storage->item->GetMIME_Type() : def_ret);
		
		return ret;
	}

	OpStringC8 ret1(StreamCache_Storage::GetMIME_Type());
	return ret1;
}

Multipart_Status Multipart_CacheStorage::GetMultipartStatus()
{
	return bodypart_status;
}


void Multipart_CacheStorage::SetMultipartStatus(Multipart_Status status)
{
	OpMessage message = MSG_NO_MESSAGE;
#ifdef DEBUG_MULPART
	const char *string;
#define SETDEBUG_STRING(x) string= #x;
#else
#define SETDEBUG_STRING(x)
#endif
	if(status == Multipart_NextBodypart)
	{
		MultipartStorage_Item *next_item = unused_cache_items.First();

		if (next_item && next_item->header_loaded_sent)
			return;

		if(bodypart_status != Multipart_BoundaryRead && (next_item || !GetFinished()))
			SetMultipartStatus(Multipart_BoundaryRead);

		if(next_item && next_item->item)
		{
			SetMultipartStatus(Multipart_HeaderLoaded);
			next_item->header_loaded_sent = TRUE;
			if(next_item->item->ContentLoaded())
				SetMultipartStatus(Multipart_BodyContent);

			if(next_item->item->GetFinished())
				SetMultipartStatus(Multipart_BoundaryRead);
		}
		return;
	}

	switch(bodypart_status)
	{
	case Multipart_NotStarted:
		if(status != Multipart_HeaderLoaded)
			break;
	case Multipart_BoundaryRead:
		if(status == Multipart_HeaderLoaded)
		{
			message = MSG_HEADER_LOADED;
			SETDEBUG_STRING(MSG_HEADER_LOADED);
			bodypart_status = status;
		}
		break;
	case Multipart_HeaderLoaded:
		if(status == Multipart_BodyContent)
		{
			message = MSG_URL_DATA_LOADED;
			SETDEBUG_STRING(MSG_URL_DATA_LOADED);
			bodypart_status = status;
		}
		break;
	case Multipart_BodyContent:
		if(status == Multipart_BoundaryRead)
		{
			message = MSG_MULTIPART_RELOAD;
			SETDEBUG_STRING(MSG_MULTIPART_RELOAD);
			bodypart_status = status;
		}
		break;
	}

	if(message != MSG_NO_MESSAGE)
	{
		URL_DataStorage *url_ds = url->GetDataStorage();
		URL_ID id = url->GetID();

#ifdef DEBUG_MULPART
		PrintfTofile("mulpart.txt", "\nPosting %s #2(%d): %s\n", string, first_bodypart_created, url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
		PrintfTofile("mulpart1.txt", "\nPosting %s #2a(%d): %s\n", string, first_bodypart_created, url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
#endif
		url_ds->BroadcastMessage(message, id, 0,MH_LIST_ALL);
	}
}

