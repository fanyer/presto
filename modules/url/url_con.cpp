/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#include "modules/probetools/probepoints.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/util/timecache.h"
#include "modules/url/url2.h"

#include "modules/pi/OpSystemInfo.h"
#include "modules/url/uamanager/ua.h"

#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_rel.h"

#include "modules/url/url_man.h"
#include "modules/formats/url_dfr.h"
// Url_con.cpp

#include "modules/img/image.h"
#ifdef URL_USE_HISTORY_VLINKS
# include "modules/history/OpHistoryModel.h"
# include "modules/history/OpHistoryPage.h"
#endif

#ifdef WEBSERVER_SUPPORT	
# include "modules/webserver/webserver-api.h"
#endif

#ifdef WEB_TURBO_MODE
# include "modules/dochand/winman.h"
#endif

#include "modules/olddebug/tstdump.h"

#include "modules/cache/url_cs.h"

#ifdef APPLICATION_CACHE_SUPPORT
# include "modules/applicationcache/application_cache_manager.h"
# include "modules/applicationcache/application_cache.h"
# include"modules/applicationcache/manifest.h"
#endif // APPLICATION_CACHE_SUPPORT

class URL_Rep_Context : public URL_Rep
{
private:
	URL_CONTEXT_ID context_id;

public:
	URL_Rep_Context(URL_CONTEXT_ID id): context_id(id)
	{
		urlManager->IncrementContextReference(context_id);
#ifdef WEB_TURBO_MODE
		if (g_windowManager)
		{
			URL_CONTEXT_ID turbo_context = g_windowManager->GetTurboModeContextId();
			if( turbo_context && turbo_context == context_id )
				SetAttribute(URL::KUsesTurbo,TRUE);
		}
#endif
	}
	virtual ~URL_Rep_Context()
	{
		PreDestruct();
		if (urlManager)
			urlManager->DecrementContextReference(context_id);
	}

	virtual URL_CONTEXT_ID GetContextId() const {return context_id;}
};

// URL_Rep constructors

#ifdef _OPERA_DEBUG_DOC_
void URL_Rep::GetMemUsed(DebugUrlMemory &debug)
{
	if(storage)
	{
		debug.number_loaded++;
		storage->GetMemUsed(debug);
		debug.memory_loaded += sizeof(*this) + name.GetMemUsed();
/*		if(relative_names)
			debug.memory_loaded+= relative_names->GetMemUsed();*/
	}
	else
	{
		debug.number_visited++;
		debug.memory_visited += sizeof(*this) + name.GetMemUsed();
/*		if(relative_names)
			debug.memory_visited += relative_names->GetMemUsed();*/
	}
}

unsigned long URL_RelRep::GetMemUsed()
{
	return sizeof(*this) + name.Capacity();
}

/*
unsigned long relurl_sort_st::GetMemUsed()
{
	unsigned long len = sizeof(*this);

	if(url[0])
		len += url[0]->GetMemUsed();
	if(url[1])
		len += url[1]->GetMemUsed();
	if(next[0])
		len += next[0]->GetMemUsed();
	if(next[1])
		len += next[1]->GetMemUsed();
	if(next[2])
		len += next[2]->GetMemUsed();

	return len;
}*/
#endif

URL_Rep::URL_Rep()
{
#if defined URL_USE_UNIQUE_ID
	url_id = ++g_url_id_counter;
	if(url_id == 0)
		url_id = ++g_url_id_counter;
	OP_ASSERT(url_id != 0);
#endif
	reference_count = 1;
	last_visited = 0;
	info.visited_checked = FALSE;
	info.content_type = FROM_RANGED_ENUM(URLContentType, URL_UNDETERMINED_CONTENT);
	info.status = FROM_RANGED_ENUM(URLStatus, URL_UNLOADED); // Offset enum
	info.unique = FALSE;
	info.reload = FALSE;
	info.is_forms_request = FALSE;
	info.have_form_password = FALSE;
	info.have_authentication = FALSE;
	info.bypass_proxy = FALSE;
#ifdef WEB_TURBO_MODE
	info.uses_turbo = FALSE;
#endif

	used = 0;
	storage = NULL;

	auth_interface = NULL;

#ifdef WEBSOCKETS_SUPPORT
	websocket = NULL;
#endif //WEBSOCKETS_SUPPORT

#ifdef CACHE_STATS
	access_disk=0;
	access_other=0;
#endif
}

OP_STATUS URL_Rep::Construct(URL_Name_Components_st &url)
{
	return name.Set_Name(url); // This always allocates even for NULL URLs
}

OP_STATUS URL_Rep::Create(URL_Rep **url_rep, URL_Name_Components_st &url, URL_CONTEXT_ID context_id)
{
	*url_rep = NULL;

	OpAutoPtr<URL_Rep> temp_url(NULL);

	if(context_id)
		temp_url.reset(OP_NEW(URL_Rep_Context, (context_id)));
	else
		temp_url.reset(OP_NEW(URL_Rep, ()));

	if(temp_url.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(temp_url->Construct(url));

	*url_rep  = temp_url.release();

	return OpStatus::OK;
}

#ifdef DISK_CACHE_SUPPORT
void URL_Rep::CreateL(URL_Rep **url_rep, DataFile_Record *rec,
#ifndef SEARCH_ENGINE_CACHE
				FileName_Store &filenames,
#endif
				OpFileFolder folder
					, URL_CONTEXT_ID context_id
					)
{

	OpStringS8 tempname;
	ANCHOR(OpStringS8, tempname);
	OpStringS tempname2;
	ANCHOR(OpStringS, tempname2);
	URL_Name_Components_st url_name;
	ANCHOR(URL_Name_Components_st, url_name);

	*url_rep = NULL;

	rec->GetIndexedRecordStringL(TAG_URL_NAME,tempname);

	tempname2.SetL(tempname);

	LEAVE_IF_ERROR(urlManager->GetResolvedNameRep(url_name, NULL, tempname2.CStr()));

	OpStackAutoPtr<URL_Rep> temp_url(NULL);

	if(context_id)
		temp_url.reset(OP_NEW_L(URL_Rep_Context, (context_id)));
	else
		temp_url.reset(OP_NEW_L(URL_Rep, ()));

	temp_url->ConstructL(url_name, rec,
#ifndef SEARCH_ENGINE_CACHE
		filenames,
#endif
		folder);

	*url_rep = temp_url.release();
}

#ifdef CACHE_FAST_INDEX
#include "modules/cache/simple_stream.h"

void URL_Rep::CreateL(URL_Rep **url_rep, DiskCacheEntry *entry,
#ifndef SEARCH_ENGINE_CACHE
				FileName_Store &filenames,
#endif
				OpFileFolder folder
					, URL_CONTEXT_ID context_id
					)
{

	OpStringS tempname2;
	ANCHOR(OpStringS, tempname2);
	URL_Name_Components_st url_name;
	ANCHOR(URL_Name_Components_st, url_name);

	*url_rep = NULL;

	tempname2.SetL(*entry->GetURL());

	LEAVE_IF_ERROR(urlManager->GetResolvedNameRep(url_name, NULL, tempname2.CStr()));

	OpStackAutoPtr<URL_Rep> temp_url(NULL);

	if(context_id)
		temp_url.reset(OP_NEW_L(URL_Rep_Context, (context_id)));
	else
		temp_url.reset(OP_NEW_L(URL_Rep, ()));

	temp_url->ConstructL(url_name, entry,
#ifndef SEARCH_ENGINE_CACHE
		filenames,
#endif
		folder);

	*url_rep = temp_url.release();
}
#endif // CACHE_FAST_INDEX
#endif // DISK_CACHE_SUPPORT

URL_Rep::~URL_Rep()
{
	PreDestruct();
}


void URL_Rep::PreDestruct()
{
	URL_DataStorage *temp = storage;
	storage = NULL;
	OP_DELETE(temp);
	if(InList())
	{
		if(urlManager)
			urlManager->RemoveFromStorage(this);
		if(InList())
			Out();
	}
}

const char* URL_Rep::LinkId()
{
	return name.Name(URL::KName_Username_Password_Escaped_NOT_FOR_UI, NULL, (info.unique ? URL_NAME_LinkID_Unique : URL_NAME_LinkID));
}


BOOL URL_Rep::HasBeenVisited()
{
	return ((IsFollowed() || !relative_names.Empty()) ? TRUE : FALSE);
}

#ifdef APPLICATION_CACHE_SUPPORT	
OP_STATUS URL_Rep::CheckAndSetApplicationCacheFallback(BOOL &had_fallback)
{
	had_fallback = FALSE;
	ApplicationCache *application_cache = g_application_cache_manager->GetCacheFromContextId(GetContextId());
	
	if (application_cache == NULL)
	{
		return OpStatus::OK;
	}
 
	Manifest *manifest = application_cache->GetManifest();
	
	const uni_char *fallback_url_str;
	
	OpString8 name_string;
	
	RETURN_IF_ERROR(GetAttribute(URL::KName, name_string));
	
	OpString name_string_uni;
	
	RETURN_IF_ERROR(name_string_uni.SetFromUTF8(name_string.CStr()));
	
	if (manifest && manifest->MatchFallback(name_string_uni.CStr(), fallback_url_str))
	{
		URL fallback_url= g_url_api->GetURL(fallback_url_str,  application_cache->GetURLContextId());				

		URLStatus status = fallback_url.Status(FALSE);

		if (fallback_url.IsValid() && status == URL_LOADED)
		{
			// Removes the request from queue
			storage->DeleteLoading();

			RETURN_IF_ERROR(SetAttribute(URL::KFallbackURL, fallback_url));
			
			RETURN_IF_ERROR(SetAttribute(URL::KLoadStatus, URL_LOADED));
			RETURN_IF_ERROR(SetAttribute(URL::KHeaderLoaded, TRUE));
			RETURN_IF_ERROR(SetAttribute(URL::KHTTP_Response_Code, fallback_url.GetAttribute(URL::KHTTP_Response_Code, TRUE)));

			storage->GetMessageHandlerList()->BroadcastMessage(MSG_HEADER_LOADED, GetID(), 0, MH_LIST_ALL);
			storage->GetMessageHandlerList()->BroadcastMessage(MSG_URL_DATA_LOADED, GetID(), 0, MH_LIST_ALL);
			
			had_fallback = TRUE;
		}
	}
	return OpStatus::OK;
}
#endif // APPLICATION_CACHE_SUPPORT

URL &URL::operator=(const URL& url)
{
	TRACER_RELEASE();

	if (url.rep)
		url.rep->IncRef();
	if (url.rel_rep)
		url.rel_rep->IncRef();

	if (rel_rep && rel_rep->DecRef() == 0 && rel_rep != EmptyURL_RelRep && !rel_rep->IsVisited())
		rep->RemoveRelativeId(rel_rep->Name());
	if (rep && rep->DecRef() == 0 && rep != url.rep && rep != EmptyURL_Rep)
		OP_DELETE(rep);
	rep = url.rep;
	rel_rep = url.rel_rep;

	TRACER_SETTRACKER(rep);
	return *this;
}

URL::URL(URL_Rep* url_rep, const char* rel)
{
#ifdef MEMTEST
    OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
	OP_ASSERT(EmptyURL_Rep);
	OP_ASSERT(EmptyURL_RelRep);

	if (url_rep)
		rep = url_rep;
	else
		rep = EmptyURL_Rep;
	if (rel)
		rel_rep = rep->GetRelativeId(rel);
	else
	{
		rel_rep = EmptyURL_RelRep;
		if (rel_rep)
			rel_rep->IncRef();
	}

	if (rep)
		rep->IncRef();

	TRACER_SETTRACKER(rep);
}

URL::URL(const URL& url, const uni_char* rel)
{
#ifdef MEMTEST
    OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
	OP_ASSERT(EmptyURL_Rep);
	OP_ASSERT(EmptyURL_RelRep);

	rep = url.rep;
	if (rel && rep)
	{
		OpString8 rel2;

		OpStatus::Ignore(rel2.SetUTF8FromUTF16(rel)); // Cannot indicate OOM; anyway, there is not that much damage if an anchor URL fails.

		rel_rep = rep->GetRelativeId(rel2);
	}
	else
	{
		rel_rep = EmptyURL_RelRep;
		if (rel_rep)
			rel_rep->IncRef();
	}

	if (rep)
		rep->IncRef();

	TRACER_SETTRACKER(rep);
}

URL::URL()
{
#ifdef MEMTEST
    OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
	OP_ASSERT(EmptyURL_Rep);
	OP_ASSERT(EmptyURL_RelRep);

	rep = EmptyURL_Rep;
	rel_rep = EmptyURL_RelRep;
	if (rep)
		rep->IncRef();
	if(rel_rep)
		rel_rep->IncRef();
}

URL::URL(const URL& url)
{
#ifdef MEMTEST
    OOM_REPORT_UNANCHORED_OBJECT();
#endif // MEMTEST
	rep = url.rep;
	if (rep)
		rep->IncRef();
	rel_rep = url.rel_rep;
	if (rel_rep)
		rel_rep->IncRef();
	TRACER_SETTRACKER(rep);
}

URL::~URL()
{
#ifdef MEMTEST
	oom_delete_unanchored_object(this);
#endif // MEMTEST
	if (rep && rel_rep && rel_rep->DecRef() == 0 && rel_rep != EmptyURL_RelRep &&
		!rel_rep->IsVisited())
	{
		rep->RemoveRelativeId(rel_rep->Name());
	}

	if (rep)
	{
		rep->DecRef();
		if (rep != EmptyURL_Rep)
		{
			if (rep->GetRefCount() == 0)
			{
				if(rep->GetUsedCount() == 0)
					OP_DELETE(rep);
			}
			else if(rep->GetRefCount() == 1 && rep->InList() &&
				rep->storage == NULL && !rep->HasBeenVisited())
			{
				OP_DELETE(rep);
			}
		}
	}
	// Make sure deleted objects are useless.
	rel_rep = 0;
}

#if defined URL_USE_UNIQUE_ID
URL_ID	URL::Id() const
{
	return rep->GetID();
}
#endif


void URL::Access(BOOL set_visited)
{
	rep->Access(set_visited);
	if(set_visited)
		rel_rep->SetFollowed(TRUE);
}

BOOL URL::IsEmpty() const
{
	return (rep == EmptyURL_Rep);
}

BOOL URL::IsValid() const
{
	return (rep && rep != EmptyURL_Rep);
}

void URL_Rep::SetIsFollowed(BOOL is_followed)
{
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	if(storage && storage->GetNeverFlush())
		is_followed = TRUE;
#endif
	SetHasCheckedVisited(FALSE);
	SetLocalTimeVisited(is_followed ? (time_t) (g_op_time_info->GetTimeUTC()/1000.0) : 0);
	if(is_followed)
		urlManager->SetLRU_Item(storage);
}

#ifdef DISK_CACHE_SUPPORT
void URL_Rep::WriteCacheDataL(DataFile_Record *rec)
{
	OP_PROBE7_L(OP_PROBE_URL_REP_WRITECACHEDATA_L);

#ifndef SEARCH_ENGINE_CACHE
	rec->AddRecordL(TAG_URL_NAME, GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr()); // Most password URLs are remvoed before they get here, might be useful for downloads
#endif
	rec->AddRecordL(TAG_LAST_VISITED, (uint32) last_visited);

	if(GetAttribute(URL::KHTTPIsFormsRequest))
		rec->AddRecordL(0, TAG_FORM_REQUEST);

	relative_names.WriteCacheDataL(rec);

	if(storage && (rec->GetTag() == TAG_CACHE_FILE_ENTRY || rec->GetTag() == TAG_DOWNLOAD_RESCUE_FILE_ENTRY))
		storage->WriteCacheDataL(rec);
}

#ifdef CACHE_FAST_INDEX
void URL_Rep::WriteCacheDataL(DiskCacheEntry *entry)
{
	OP_PROBE7_L(OP_PROBE_URL_REP_WRITECACHEDATA_L);

#ifndef SEARCH_ENGINE_CACHE
	OpStringC8 url_name = GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI);
	entry->SetURL(&url_name);
#endif
	entry->SetLastVisited(last_visited);

	if(GetAttribute(URL::KHTTPIsFormsRequest))
		entry->SetFormQuery(TRUE);
		
	if(GetAttribute(URL::KMultimedia))
		entry->SetMultimedia(TRUE);

	if(GetAttribute(URL::KMIME_Type_Was_NULL))
		entry->SetContentTypeWasNULL(TRUE);

	OpStringC8 server_content_type = GetAttribute(URL::KServerMIME_Type);
	entry->SetServerContentType(&server_content_type);

	relative_names.WriteCacheDataL(entry);

	if(storage && (entry->GetTag() == TAG_CACHE_APPLICATION_CACHE_ENTRY || entry->GetTag() == TAG_CACHE_FILE_ENTRY || entry->GetTag() == TAG_DOWNLOAD_RESCUE_FILE_ENTRY || entry->GetTag() == TAG_CACHE_PARAMETERS_ONLY))
	#if CACHE_SMALL_FILES_SIZE>0
		storage->WriteCacheDataL(entry, storage->GetCacheStorage()->IsEmbedded());
	#else
		storage->WriteCacheDataL(entry, FALSE);
	#endif
}
#endif // CACHE_FAST_INDEX
#endif // DISK_CACHE_SUPPORT

#ifdef DISK_CACHE_SUPPORT
#ifndef SEARCH_ENGINE_CACHE
void URL_Rep::ConstructL(URL_Name_Components_st &url, DataFile_Record *rec, FileName_Store &filenames, OpFileFolder folder)
#else
void URL_Rep::ConstructL(URL_Name_Components_st &url, DataFile_Record *rec, OpFileFolder folder)
#endif
{
	LEAVE_IF_ERROR(name.Set_Name(url));

	last_visited = rec->GetIndexedRecordUIntL(TAG_LAST_VISITED);
	SetAttributeL(URL::KHTTPIsFormsRequest, rec->GetIndexedRecordBOOL(TAG_FORM_REQUEST));

	DataFile_Record *rec1 = rec->GetIndexedRecord(TAG_RELATIVE_ENTRY);
	while(rec1)
	{
		rec1->SetRecordSpec(rec->GetRecordSpec());
		rec1->IndexRecordsL();

		URL_RelRep *relrep;
	    URL_RelRep::CreateL(&relrep, rec1);

		if(!relrep->IsVisited())
		{
			OP_DELETE(relrep);
		}
		else
		{
			relative_names.FindOrAddRep(relrep);
		}

		rec1 = rec->GetIndexedRecord(rec1,TAG_RELATIVE_ENTRY);
	}

	if(rec->GetTag() == TAG_CACHE_FILE_ENTRY || rec->GetTag() == TAG_DOWNLOAD_RESCUE_FILE_ENTRY)
	{
		OpStackAutoPtr<URL_DataStorage> temp_storage(OP_NEW_L(URL_DataStorage, (this)));

#ifndef SEARCH_ENGINE_CACHE
		temp_storage->InitL(rec, filenames, folder);
#else
		temp_storage->InitL(rec, folder);
#endif

		storage = temp_storage.release();
	}
}

#ifdef CACHE_FAST_INDEX
	#ifndef SEARCH_ENGINE_CACHE
	void URL_Rep::ConstructL(URL_Name_Components_st &url, DiskCacheEntry *entry, FileName_Store &filenames, OpFileFolder folder)
	#else
	void URL_Rep::ConstructL(URL_Name_Components_st &url, DiskCacheEntry *entry, OpFileFolder folder)
	#endif
	{
		LEAVE_IF_ERROR(name.Set_Name(url));

		last_visited = entry->GetLastVisited();
		SetAttributeL(URL::KHTTPIsFormsRequest, entry->GetFormQuery());
		if(entry->GetMultimedia())
			SetAttributeL(URL::KMultimedia, TRUE);
		if(entry->GetContentTypeWasNULL())
			SetAttributeL(URL::KMIME_Type_Was_NULL, TRUE);

		SetAttributeL(URL::KServerMIME_Type, *entry->GetServerContentType());

		int num_rel=entry->GetRelativeLinksCount();

		for(int i=0; i<num_rel; i++)
		{
			RelativeEntry *rel=entry->GetRelativeLink(i);
			URL_RelRep *relrep;
			URL_RelRep::CreateL(&relrep, *rel->GetName());

			if(!relrep->IsVisited())
			{
				OP_DELETE(relrep);
			}
			else
			{
				relative_names.FindOrAddRep(relrep);
			}
		}

		if (entry->GetTag() == TAG_CACHE_APPLICATION_CACHE_ENTRY || entry->GetTag() == TAG_CACHE_FILE_ENTRY || entry->GetTag() == TAG_DOWNLOAD_RESCUE_FILE_ENTRY)
		{
			if(storage)
			{
		#ifndef SEARCH_ENGINE_CACHE
				storage->InitL(entry, filenames, folder);
		#else
				storage->InitL(entry, folder);
		#endif
			}
			else
			{
				OpStackAutoPtr<URL_DataStorage> temp_storage(OP_NEW_L(URL_DataStorage, (this)));

		#ifndef SEARCH_ENGINE_CACHE
				temp_storage->InitL(entry, filenames, folder);
		#else
				temp_storage->InitL(entry, folder);
		#endif

				OP_ASSERT(storage == NULL); // Or we'll leak memory.
				storage = temp_storage.release();
			}
		}
	}
#endif // CACHE_FAST_INDEX
#endif // DISK_CACHE_SUPPORT

uint32 URL::GetAttribute(URL::URL_Uint32Attribute attr, URL::URL_Redirect follow_ref) const
{
	if(attr == URL::KIsFollowed)
	{
		if(follow_ref)
		{
			URL moved_to = ((URL *) this)->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);

			if(moved_to.IsValid())
				return moved_to.GetAttribute(attr, URL::KNoRedirect);
		}

		if(rel_rep != EmptyURL_RelRep)
		{
			if(rel_rep->IsVisited())
				return TRUE;
		}
		else if(rep->GetAttribute(attr,URL::KNoRedirect))
			return TRUE;

#ifdef URL_USE_HISTORY_VLINKS
		if (g_globalHistory == NULL)
			return FALSE;

		// if we have  checked it earlier and visited was set, then GetIsFollowed() above would have returned TRUE. If it's not visited, we only need to check
		// it again if it hasn't been checked already.
		if ((rel_rep && !rel_rep->GetHasCheckedVisited()) || (!rel_rep && rep && !rep->GetHasCheckedVisited()))
		{
			// Make sure to use the same url string that is inserted into history when doing lookups
			OpStringC url_str = rep->GetAttribute(URL::KUniName_With_Fragment_Username, URL::KNoRedirect, rel_rep);
			if(!url_str.HasContent())
			{
				return FALSE;
			}
			HistoryPage *history_item;

			if(rel_rep)
			{
				rel_rep->SetHasCheckedVisited(TRUE);
			}
			else
			{
				rep->SetHasCheckedVisited(TRUE);
			}

			if(OpStatus::IsSuccess(g_globalHistory->GetItem(url_str, history_item)))
			{
				if(history_item->IsVisited())
				{
					if(rel_rep)
						rel_rep->SetLastVisited(history_item->GetAccessed());
					else
						rep->SetLocalTimeVisited(history_item->GetAccessed());
					return TRUE;
				}
			}
		}
		return FALSE;
#else
		return FALSE;
#endif
	}
	return rep->GetAttribute(attr,follow_ref);
}

void URL::GetAttributeL(URL::URL_StringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref) const
{
	LEAVE_IF_ERROR(rep->GetAttribute(attr, val, follow_ref, rel_rep));
}

void URL::GetAttributeL(URL::URL_UniStringAttribute attr, OpString &val, URL::URL_Redirect follow_ref) const
{
	LEAVE_IF_ERROR(rep->GetAttribute(attr, val, follow_ref, rel_rep));
}

void URL::GetAttributeL(URL::URL_NameStringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref) const
{
	LEAVE_IF_ERROR(rep->GetAttribute(attr, val, follow_ref, rel_rep));
}

void URL::GetAttributeL(URL::URL_UniNameStringAttribute attr, unsigned short charsetID, OpString &val, URL::URL_Redirect follow_ref) const
{
	LEAVE_IF_ERROR(rep->GetAttribute(attr, charsetID, val, follow_ref, rel_rep));
}

void URL::SetAttributeL(URL::URL_Uint32Attribute attr, uint32 value)
{
	LEAVE_IF_ERROR(rep->SetAttribute(attr,value));
}

void URL::SetAttributeL(URL::URL_StringAttribute attr, const OpStringC8 &string)
{
	LEAVE_IF_ERROR(rep->SetAttribute(attr,string));
}

void URL::SetAttributeL(URL::URL_UniStringAttribute attr, const OpStringC &string)
{
	LEAVE_IF_ERROR(rep->SetAttribute(attr,string));
}

void URL::SetAttributeL(URL::URL_VoidPAttribute  attr, const void *param)
{
	LEAVE_IF_ERROR(rep->SetAttribute(attr,param));
}

uint32 URL_Rep::GetAttribute(URL::URL_Uint32Attribute attr, URL::URL_Redirect follow_ref) const
{
	if(follow_ref != URL::KNoRedirect)
	{
		URL url = ((URL_Rep *)this)->GetAttribute(URL::KMovedToURL,URL::KFollowRedirect);

		if(url.IsValid())
			return url.GetAttribute(attr, URL::KNoRedirect);
	}

	if(attr > URL::KUInt_Start_Name &&  attr < URL::KUInt_End_Name)
		return name.GetAttribute(attr);
	if((attr > URL::KUInt_Start_Storage &&  attr < URL::KUInt_End_Storage) || attr > URL::KStartUintDynamicAttributeList)
		return storage ? storage->GetAttribute(attr) : (attr == URL::KHTTP_Refresh_Interval ? (uint32) (-1) : (attr ==  URL::KLoadedFromNetType ? NETTYPE_UNDETERMINED : 0));

	switch(attr)
	{
	case URL::KContentType:
		if(storage)
		{
			uint32 cnt_typ = storage->GetAttribute(URL::K_INTERNAL_ContentType);
			if((URLContentType) cnt_typ != URL_UNDETERMINED_CONTENT)
			{
				return cnt_typ;
			}
		}
		// Fall through to original content type
	case URL::KOriginalContentType:
		return GET_RANGED_ENUM(URLContentType, info.content_type); // offset enum
	case URL::KHaveAuthentication:
		return info.have_authentication;
	case URL::KHavePassword:
		return info.have_form_password;
	case URL::KHTTPIsFormsRequest:
		return info.is_forms_request;
	case URL::KIsImage:
		return imgManager->IsImage((URLContentType) GetAttribute(URL::KContentType));
	case URL::KIsFollowed:
		return ((URL_Rep *) this)->IsFollowed();
	case URL::KLoadStatus:
		return GET_RANGED_ENUM(URLStatus, info.status); // Offset enum
	case URL::KReloading:
		return info.reload;
	case URL::KUnique:
		return info.unique;
	case URL::KTimeoutPollIdle:
		if (storage)
			return storage->GetAttribute(URL::KTimeoutPollIdle);
		break;

	case URL::KTimeoutMaxTCPConnectionEstablished:
		if (storage)
			return storage->GetAttribute(URL::KTimeoutMaxTCPConnectionEstablished);
		break;

	case URL::KUntrustedContent:
		{
			if(storage &&
				storage->GetAttribute(URL::KUntrustedContent))
				return TRUE;

			URL redirect = ((URL_Rep *) this)->GetAttribute(URL::KMovedToURL,URL::KNoRedirect);
			if(redirect.IsValid())
				return redirect.GetAttribute(URL::KUntrustedContent);
		}
		break;
#ifdef WEBSERVER_SUPPORT
	case URL::KIsUniteServiceAdminURL:
		{
		const ServerName* server_name = static_cast<const ServerName*>(name.GetAttribute(URL::KServerName));
		if (server_name && g_webserver)
			return g_webserver->MatchServerAdmin(server_name, name.GetAttribute(URL::KResolvedPort));

		return FALSE;
		}
	case URL::KIsLocalUniteService:
		{
		const ServerName* server_name = static_cast<const ServerName*>(name.GetAttribute(URL::KServerName));
		if (server_name && g_webserver)
			return g_webserver->MatchServer(server_name, name.GetAttribute(URL::KResolvedPort));

		return FALSE;
		}
#endif
	case URL::KBypassProxy:
		return info.bypass_proxy;
#ifdef WEB_TURBO_MODE
	case URL::KUsesTurbo:
		return info.uses_turbo;
#endif
	}

	return 0;
}

void URL_Rep::SetAttributeL(URL::URL_Uint32Attribute attr, uint32 value)
{
	LEAVE_IF_ERROR(SetAttribute(attr,value));
}

OP_STATUS URL_Rep::SetAttribute(URL::URL_Uint32Attribute attr, uint32 value)
{
	/* Set Attribute of name is disabled since no current flags can be set in the name
	if(attr > URL::KUInt_Start_Name &&  attr < URL::KUInt_End_Name)
	{
		name.SetAttribute(attr, value);
		return;
	}
	*/
	if((attr > URL::KUInt_Start_Storage &&  attr < URL::KUInt_End_Storage) || attr > URL::KStartUintDynamicAttributeList )
	{
#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
		if(!storage && attr == URL::KPauseDownload)
			return OpStatus::OK;
#endif
		OP_STATUS op_err = OpStatus::OK;
		if(CheckStorage(op_err))
			op_err = storage->SetAttribute(attr, value);
		return op_err;
	}

	BOOL flag = (value ? TRUE : FALSE);

	switch(attr)
	{
	case URL::KForceContentType:
		{
			URL url = GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);

			if(url.IsValid())
			{
				return url.SetAttribute(URL::KContentType, value);
			}
		}
		// Continue to URL::KContentType
	case URL::KContentType:
		if(storage)
			RETURN_IF_ERROR(storage->SetAttribute(URL::K_INTERNAL_ContentType, value));
		CHECK_RANGED_ENUM(URLContentType, value);
		info.content_type = FROM_RANGED_ENUM(URLContentType, value); // Offset enum
		break;
	case URL::KHaveAuthentication:
		info.have_authentication = flag;
		break;
	case URL::KHavePassword:
		info.have_form_password = flag;
		break;
	case URL::KHTTPIsFormsRequest:
		info.is_forms_request = flag;
		break;
	case URL::KLoadStatus:
		if (GetAttribute(URL::KCacheType) == URL_CACHE_DISK &&
			(value == URL_EMPTY || value == URL_UNLOADED || value == URL_LOADING_ABORTED || value == URL_LOADING_FAILURE || value == URL_LOADING_WAITING))
			{
				// Multimedia files must persist even after a stop loading, becuase is a common operation for them
				if(value!=URL_LOADING_ABORTED || !storage->GetAttribute(URL::KMultimedia))
					RETURN_IF_ERROR(SetAttribute(URL::KCacheType, URL_CACHE_TEMP));
			}
		CHECK_RANGED_ENUM(URLStatus, value); // Offset enum
		info.status = FROM_RANGED_ENUM(URLStatus, value); // Offset enum
		break;
	case URL::KReloading:
		info.reload = (value ? 1 : 0);
		break;
	case URL::KUnique:
		info.unique = (value ? 1 : 0);
		break;
	case URL::KTimeoutPollIdle:
		if (storage)
			storage->SetAttribute(URL::KTimeoutPollIdle, value);
		break;
	case URL::KTimeoutMaxTCPConnectionEstablished:
		OP_STATUS op_err;
		if(CheckStorage(op_err))
			storage->SetAttribute(URL::KTimeoutMaxTCPConnectionEstablished, value);
		return op_err;
	case URL::KUntrustedContent:
		{
			if(storage)
				storage->SetAttributeL(URL::KUntrustedContent,value);

			URL redirect = GetAttribute(URL::KMovedToURL);
			if(redirect.IsValid())
				redirect.SetAttributeL(URL::KUntrustedContent,value);
		}
		break;
	case URL::KBypassProxy:
		info.bypass_proxy = flag;
		break;
#ifdef WEB_TURBO_MODE
	case URL::KUsesTurbo:
		info.uses_turbo = flag;
		break;
#endif
	}

	return OpStatus::OK;
}

const OpStringC8 URL_Rep::GetAttribute(URL::URL_StringAttribute attr, URL::URL_Redirect follow_ref, URL_RelRep* rel_rep) const
{
	if(follow_ref != URL::KNoRedirect)
	{
		URL url = ((URL_Rep *)this)->GetAttribute(URL::KMovedToURL,URL::KFollowRedirect);

		if(url.IsValid())
		{
			OpStringC8 ret(url.GetAttribute(attr, URL::KNoRedirect));

			return ret;
		}
	}

	if(attr == URL::KFragment_Name)
	{
		OpStringC8 ret(rel_rep->Name());

		return ret;
	}

	if(attr > URL::KStr_Start_Name &&  attr < URL::KStr_End_Name)
	{
		OpStringC8 ret(name.GetAttribute(attr, rel_rep));

		return ret;
	}

	if(storage)
	{
		OpStringC8 ret(storage->GetAttribute(attr));

		return ret;
	}

	OpStringC8 def_ret;
	return def_ret;
}

void URL_Rep::GetAttributeL(URL::URL_StringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref, URL_RelRep* rel_rep) const
{
	LEAVE_IF_ERROR(GetAttribute(attr, val, follow_ref, rel_rep));
}

OP_STATUS URL_Rep::GetAttribute(URL::URL_StringAttribute attr, OpString &val, URL::URL_Redirect follow_ref, URL_RelRep* rel_rep) const
{
	OpString8 temp;

	RETURN_IF_ERROR(GetAttribute(attr, temp, follow_ref, rel_rep));

	return val.Set(temp);
}

OP_STATUS URL_Rep::GetAttribute(URL::URL_StringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref, URL_RelRep* rel_rep) const
{
	if(follow_ref != URL::KNoRedirect)
	{
		URL url = ((URL_Rep *)this)->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);

		if(url.IsValid())
		{
			return url.GetAttribute(attr, val, URL::KNoRedirect);
		}
	}

	if(attr == URL::KFragment_Name)
	{
		return val.Set(rel_rep->Name());
	}

	if(attr > URL::KStr_Start_Name &&  attr < URL::KStr_End_Name)
	{
		return name.GetAttribute(attr,val, rel_rep);
	}

	if(storage)
	{
		return storage->GetAttribute(attr, val);
	}

	val.Empty();
	return OpStatus::OK;
}



const OpStringC URL_Rep::GetAttribute(URL::URL_UniStringAttribute attr, URL::URL_Redirect follow_ref, URL_RelRep* rel_rep) const
{
	if(follow_ref != URL::KNoRedirect)
	{
		URL url = ((URL_Rep *)this)->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);

		if(url.IsValid())
		{
			OpStringC ret(url.GetAttribute(attr, URL::KNoRedirect));
			return ret;
		}
	}

	if(attr == URL::KUniFragment_Name)
	{
		OpStringC ret(rel_rep->UniName());
		return ret;
	}

	if(attr > URL::KUStr_Start_Name &&  attr < URL::KUStr_End_Name)
	{
		OpStringC ret(name.GetAttribute(attr, rel_rep));
		return ret;
	}

	if(attr > URL::KUStr_Start_Storage &&  attr < URL::KUStr_End_Storage)
	{
		if(storage)
		{
			OpStringC ret(storage->GetAttribute(attr));
			return ret;
		}
	}
	OpStringC def_ret;
	return def_ret;
}

void URL_Rep::GetAttributeL(URL::URL_UniStringAttribute attr, OpString &val, URL::URL_Redirect follow_ref, URL_RelRep* rel_rep) const
{
	LEAVE_IF_ERROR(GetAttribute(attr, val, follow_ref, rel_rep));
}

OP_STATUS URL_Rep::GetAttribute(URL::URL_UniStringAttribute attr, OpString &val, URL::URL_Redirect follow_ref, URL_RelRep* rel_rep) const
{
	if(follow_ref)
	{
		URL url = ((URL_Rep *)this)->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);

		if(url.IsValid())
		{
			return url.GetAttribute(attr, val, URL::KNoRedirect);
		}
	}

	if(attr == URL::KUniFragment_Name)
	{
		return val.Set(rel_rep->UniName());
	}

	if(attr > URL::KUStr_Start_Name &&  attr < URL::KUStr_End_Name)
	{
		return name.GetAttribute(attr,val, rel_rep);
	}

	if(attr > URL::KUStr_Start_Storage &&  attr < URL::KUStr_End_Storage)
	{
		if(storage)
		{
			return storage->GetAttribute(attr, val);
		}
	}

	switch(attr)
	{
	case URL::KSuggestedFileName_L:
		return GetSuggestedFileName(val);
		break;
	case URL::KSuggestedFileNameExtension_L:
		return GetSuggestedFileNameExtension(val);
		break;
	default:
		val.Empty();
		break;
	}
	return OpStatus::OK;
}

const OpStringC8 URL_Rep::GetAttribute(URL::URL_NameStringAttribute attr, URL::URL_Redirect follow_ref, URL_RelRep* rel_rep) const
{
	if(follow_ref != URL::KNoRedirect)
	{
		URL url = ((URL_Rep *)this)->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);

		if(url.IsValid())
		{
			OpStringC8 ret(url.GetRep()->GetAttribute(attr, URL::KNoRedirect));
			return ret;
		}
	}

	OpStringC8 ret(name.GetAttribute(attr, rel_rep));
	return ret;
}

const OpStringC URL_Rep::GetAttribute(URL::URL_UniNameStringAttribute attr, URL::URL_Redirect follow_ref, URL_RelRep* rel_rep) const
{
	if(follow_ref != URL::KNoRedirect)
	{
		URL url = ((URL_Rep *)this)->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);

		if(url.IsValid())
		{
			OpStringC ret(url.GetRep()->GetAttribute(attr, URL::KNoRedirect));
			return ret;
		}
	}

	OpStringC ret(name.GetAttribute(attr, rel_rep));
	return ret;
}

OP_STATUS URL_Rep::GetAttribute(URL::URL_NameStringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref, URL_RelRep* rel_rep) const
{
	if(follow_ref != URL::KNoRedirect)
	{
		URL url = ((URL_Rep *)this)->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);

		if(url.IsValid())
		{
			return url.GetAttribute(attr, val, URL::KNoRedirect);
		}
	}

	return name.GetAttribute(attr,val, rel_rep);
}

OP_STATUS URL_Rep::GetAttribute(URL::URL_UniNameStringAttribute attr, unsigned short charsetID, OpString &val, URL::URL_Redirect follow_ref, URL_RelRep* rel_rep) const
{
	if(follow_ref != URL::KNoRedirect)
	{
		URL url = ((URL_Rep *)this)->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);

		if(url.IsValid())
		{
			return url.GetAttribute(attr, charsetID, val, URL::KNoRedirect);
		}
	}

	return name.GetAttribute(attr, charsetID, val, rel_rep);
}

const void *URL_Rep::GetAttribute(URL::URL_VoidPAttribute  attr, const void *param, URL::URL_Redirect follow_ref) const
{
	if(follow_ref != URL::KNoRedirect)
	{
		URL url = ((URL_Rep *)this)->GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);

		if(url.IsValid())
			return url.GetAttribute(attr, param, URL::KNoRedirect);
	}

	if(attr > URL::KVoip_Start_Name &&  attr < URL::KVoip_End_Name)
	{
		return name.GetAttribute(attr);
	}

	switch(attr)
	{
	case URL::K_ID:
		OP_ASSERT(param); // Parameter MUST be provided
		if(param)
			*((URL_ID *) param) = GetID();
		return param;
	case URL::KVLocalTimeVisited:
		OP_ASSERT(param); // Parameter MUST be provided
		if(param)
			*((time_t *) param) = last_visited;
		return param;
	/** DO NOT PUT new cases below this line, unless they are part of the same logic */
	case URL::KContentLoaded:
	case URL::KContentSize:
		OP_ASSERT(param); // Parameter MUST be provided
		if(param)
			*((OpFileLength *) param) = 0;
		// Fall through to default;
	/** DO NOT PUT new cases below this line, unless they are part of the same logic */
	default:
		if(storage)
			return storage->GetAttribute(attr, param);
		break;
	}

	return NULL;
}

URL URL_Rep::GetAttribute(URL::URL_URLAttribute  attr, URL::URL_Redirect follow_ref)
{
	if(follow_ref != URL::KNoRedirect)
	{
		if(attr != URL::KMovedToURL)
		{
			URL url = GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);

			if(url.IsValid())
				return url.GetAttribute(attr, URL::KNoRedirect);
		}
		else if(storage)
		{
			URL ret = storage->GetAttribute(URL::KMovedToURL);

			if(ret.IsValid())
			{
				URL url = ret.GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);
				if(url.IsValid())
					ret = url;
			}

			OP_ASSERT(ret.GetRep() != this);

			return ret;
		}
	}

	if(storage)
		return storage->GetAttribute(attr);

	return URL();
}

void URL_Rep::SetAttributeL(URL::URL_URLAttribute  attr, const URL &param)
{
	LEAVE_IF_ERROR(SetAttribute(attr,param));
}

OP_STATUS URL_Rep::SetAttribute(URL::URL_URLAttribute  attr, const URL &param)
{
	switch(attr)
	{
	case URL::KBaseAliasURL:
		if(param.IsEmpty())
			return OpStatus::OK; // Do nothing more (have reset the URL to non-alias status)
		break;
	case URL::KAliasURL:
		if((URLStatus) GetAttribute(URL::KLoadStatus) != URL_UNLOADED)
			return OpStatus::ERR;
		Unload(); // Quick and clean removal of earlier alias.

		if(param.IsEmpty())
			return OpStatus::OK; // Do nothing more (have reset the URL to non-alias status)
		break;
	}

	OP_STATUS op_err = OpStatus::OK;
	if(CheckStorage(op_err))
		op_err = storage->SetAttribute(attr, param);

	return op_err;
}

void URL::SetAttributeL(URL::URL_URLAttribute  attr, const URL &param)
{
	LEAVE_IF_ERROR(rep->SetAttribute(attr, param));
}

OP_STATUS URL::SetAttribute(URL::URL_URLAttribute  attr, const URL &param)
{
	return rep->SetAttribute(attr, param);
}

void URL_Rep::SetAttributeL(URL::URL_StringAttribute attr, const OpStringC8 &string)
{
	LEAVE_IF_ERROR(SetAttribute(attr,string));
}

OP_STATUS URL_Rep::SetAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string)
{
	OP_ASSERT(!(attr > URL::KStr_Start_Name &&  attr < URL::KStr_End_Name)); // In this range -> Illegal operation

	OP_STATUS op_err = OpStatus::OK;
	if(CheckStorage(op_err))
		op_err = storage->SetAttribute(attr, string);

	return op_err;
}

void URL_Rep::SetAttributeL(URL::URL_UniStringAttribute attr, const OpStringC &string)
{
	LEAVE_IF_ERROR(SetAttribute(attr,string));
}

OP_STATUS URL_Rep::SetAttribute(URL::URL_UniStringAttribute attr, const OpStringC &string)
{
	OP_ASSERT(!(attr > URL::KUStr_Start_Name &&  attr < URL::KUStr_End_Name)); // In this range -> Illegal operation

	OP_STATUS op_err = OpStatus::OK;
	if(CheckStorage(op_err))
		op_err = storage->SetAttribute(attr, string);

	return op_err;
}

void URL_Rep::SetAttributeL(URL::URL_VoidPAttribute  attr, const void *param)
{
	LEAVE_IF_ERROR(SetAttribute(attr,param));
}

OP_STATUS URL_Rep::SetAttribute(URL::URL_VoidPAttribute  attr, const void *param)
{
	if(attr > URL::KVoip_Start_Storage && attr < URL::KVoip_End_Storage)
	{
		OP_STATUS op_err = OpStatus::OK;
		if(CheckStorage(op_err))
			op_err = storage->SetAttribute(attr, param);
		return op_err;
	}

	switch(attr)
	{
	case URL::KVLocalTimeVisited:
		OP_ASSERT(param); // Parameter MUST be provided
		SetHasCheckedVisited(FALSE);
		if(param)
			last_visited = *((time_t *) param) = last_visited;
		break;
	}

	return OpStatus::OK;
}

BOOL URL::SameServer(const URL &other, Server_Check include_port) const
{
	ServerName *sn = (ServerName *) GetAttribute(URL::KServerName,NULL);
	ServerName *other_sn = (ServerName *) other.GetAttribute(URL::KServerName,NULL);

	if(!sn || sn != other_sn)
		return FALSE;

	switch(include_port)
	{
	case URL::KCheckPort:
		return (GetAttribute(URL::KServerPort) == other.GetAttribute(URL::KServerPort));
	case URL::KCheckResolvedPort:
		return (GetAttribute(URL::KResolvedPort) == other.GetAttribute(URL::KResolvedPort));
	}
	return TRUE;
}

URLLink::~URLLink()
{
}

#ifdef URL_ACTIVATE_OLD_NAME_FUNCS
static URL::URL_NameStringAttribute ConvertPasswordHideToName(Password_Status password_hide)
{
	switch(password_hide)
	{
	case PASSWORD_SHOW:
	case PASSWORD_SHOW_VERBATIM:
		return URL::KName_Username_Password_Escaped_NOT_FOR_UI;
	case PASSWORD_HIDDEN:
	case PASSWORD_HIDDEN_VERBATIM:
		return URL::KName_Username_Password_Hidden_Escaped;
	case PASSWORD_ONLYUSER:
	case PASSWORD_ONLYUSER_VERBATIM:
		return URL::KName_Username_Escaped;
	case PASSWORD_NOTHING:
	case PASSWORD_NOTHING_VERBATIM:
		return URL::KName_Escaped;
	case PASSWORD_SHOW_UNESCAPE_URL:
		return URL::KName_Username_Password_NOT_FOR_UI;
	case PASSWORD_HIDDEN_UNESCAPE_URL:
		return URL::KName_Username_Password_Hidden;
	case PASSWORD_ONLYUSER_UNESCAPE_URL:
		return URL::KName_Username;
	case PASSWORD_NOTHING_UNESCAPE_URL:
		return URL::KName;
	}
	return URL::KName_Escaped;
}

static URL::URL_UniNameStringAttribute ConvertPasswordHideToUniName(Password_Status password_hide)
{
	switch(password_hide)
	{
	case PASSWORD_SHOW:
	case PASSWORD_SHOW_UNESCAPE_URL:
		return URL::KUniName_Username_Password_NOT_FOR_UI;
	case PASSWORD_HIDDEN:
	case PASSWORD_HIDDEN_UNESCAPE_URL:
		return URL::KUniName_Username_Password_Hidden;
	case PASSWORD_ONLYUSER:
	case PASSWORD_ONLYUSER_UNESCAPE_URL:
		return URL::KUniName_Username;
	case PASSWORD_NOTHING:
	case PASSWORD_NOTHING_UNESCAPE_URL:
		return URL::KUniName;
	case PASSWORD_SHOW_VERBATIM:
		return URL::KUniName_Username_Password_Escaped_NOT_FOR_UI;
	case PASSWORD_HIDDEN_VERBATIM:
		return URL::KUniName_Username_Password_Hidden_Escaped;
	case PASSWORD_NOTHING_VERBATIM:
		return URL::KUniName_Escaped;
	case PASSWORD_ONLYUSER_VERBATIM:
		return URL::KUniName_Username_Escaped;
	}
	return URL::KUniName;
}

static URL::URL_UniNameStringAttribute ConvertPasswordHideToUniNameFragment(Password_Status password_hide)
{
	switch(password_hide)
	{
	case PASSWORD_SHOW:
	case PASSWORD_SHOW_UNESCAPE_URL:
		return URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI;
	case PASSWORD_HIDDEN:
	case PASSWORD_HIDDEN_UNESCAPE_URL:
		return URL::KUniName_With_Fragment_Username_Password_Hidden;
	case PASSWORD_ONLYUSER:
	case PASSWORD_ONLYUSER_UNESCAPE_URL:
		return URL::KUniName_With_Fragment_Username;
	case PASSWORD_NOTHING:
	case PASSWORD_NOTHING_UNESCAPE_URL:
		return URL::KUniName_With_Fragment;
	case PASSWORD_SHOW_VERBATIM:
		return URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI;
	case PASSWORD_HIDDEN_VERBATIM:
		return URL::KUniName_With_Fragment_Username_Password_Hidden_Escaped;
	case PASSWORD_NOTHING_VERBATIM:
		return URL::KUniName_With_Fragment_Escaped;
	case PASSWORD_ONLYUSER_VERBATIM:
		return URL::KUniName_With_Fragment_Username_Escaped;
	}
	return URL::KUniName;
}

const uni_char *URL::UniName(Password_Status password_hide) const
{
	return rep->GetAttribute(ConvertPasswordHideToUniName(password_hide)).CStr();
}

const char*	URL::Name(Password_Status password_hide) const
{
	return rep->GetAttribute(ConvertPasswordHideToName(password_hide)).CStr();
}

//const char* URL::GetNameWithRel(Password_Status password_hide) const { return rep->Name(rel_rep, password_hide); }
const uni_char*	URL::GetUniNameWithRel(Password_Status password_hide) const
{
	return rep->GetAttribute(ConvertPasswordHideToUniNameFragment(password_hide)).CStr();
}

const char*	URL::GetName(BOOL follow_ref,Password_Status password_hide)
{
	return rep->GetAttribute(ConvertPasswordHideToName(password_hide), (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect)).CStr();
}

const uni_char*	URL::GetUniName(BOOL follow_ref,Password_Status password_hide)
{
	return rep->GetAttribute(ConvertPasswordHideToUniName(password_hide), (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect)).CStr();
}
#endif

#ifdef _EMBEDDED_BYTEMOBILE
void URL::SetPredicted(BOOL val, int depth) { rep->SetPredicted(val, depth); }
void URL_Rep::SetPredicted(BOOL val, int depth) {if (CheckStorage()) storage->SetPredicted(val, depth); }
#endif // _EMBEDDED_BYTEMOBILE

BOOL URL_Rep::DumpSourceToDisk(BOOL force_file ) { if(force_file) CheckStorage(); return storage && storage->DumpSourceToDisk(force_file); }
BOOL URL_Rep::FreeUnusedResources(double cached_system_time_sec) { return storage && storage->FreeUnusedResources(cached_system_time_sec); }

#if defined(_DEBUG) && defined(SEARCH_ENGINE_CACHE) && !defined(_NO_GLOBALS_)
#include "modules/cache/CacheIndex.h"
#include "modules/cache/url_cs.h"
extern CacheIndex *g_disk_index;
#endif

void URL_Rep::Unload()
{
#if defined(_DEBUG) && defined(SEARCH_ENGINE_CACHE) && !defined(_NO_GLOBALS_)
	OpString ufname;
	OpString8 fname, url_str;
	BOOL cache_disk;

	TRAPD(op_err, GetAttributeL(URL::KFilePathName_L, ufname, TRUE));
	fname.SetUTF8FromUTF16(ufname);
	url_str.Set(LinkId());
	cache_disk = GetAttribute(URL::KCacheType) == URL_CACHE_DISK;
#endif

	IncRef();

	URL_DataStorage *temp = storage;

	storage = NULL;
	if (temp)
		OP_DELETE(temp);

	DecRef();

#if defined(_DEBUG) && defined(SEARCH_ENGINE_CACHE) && !defined(_NO_GLOBALS_)
	OpString tmp_storage;
	const uni_char * dbg_cache_folder = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_CACHE_FOLDER, tmp_storage);
	if (fname.HasContent() && g_disk_index != NULL &&
		uni_strncmp(ufname, dbg_cache_folder, uni_strlen(dbg_cache_folder)) == 0 &&
		(cache_disk	|| uni_strncmp(dbg_cache_folder, fname, uni_strlen(dbg_cache_folder)) == 0))
	{
		if (!g_disk_index->SearchUrl(url_str))
		{
			OP_ASSERT(BlockStorage::FileExists(ufname, OPFILE_ABSOLUTE_FOLDER) != OpBoolean::IS_TRUE);
		}
		else {
			OP_ASSERT(BlockStorage::FileExists(ufname, OPFILE_ABSOLUTE_FOLDER) != OpBoolean::IS_FALSE);
		}
	}
#endif
}

MessageHandler* URL_Rep::GetFirstMessageHandler() { return (storage ? storage->GetFirstMessageHandler() : NULL); }
void URL_Rep::ChangeMessageHandler(MessageHandler* old_mh, MessageHandler* new_mh) { if(storage) storage->ChangeMessageHandler(old_mh, new_mh); }

void URL_Rep::RemoveMessageHandler(MessageHandler *old_mh){if(storage) storage->RemoveMessageHandler(old_mh);}

BOOL URL_Rep::DetermineThirdParty(URL &referrer){ if(CheckStorage()) return storage->DetermineThirdParty(referrer); return TRUE;}

void URL_Rep::CopyAllHeadersL(HeaderList& header_list_copy) const { if (storage) storage->CopyAllHeadersL(header_list_copy); }

#if CACHE_SMALL_FILES_SIZE>0
	BOOL URL_Rep::IsEmbedded() { return storage && storage->GetCacheStorage() && storage->GetCacheStorage()->IsEmbedded(); }
#endif

OP_STATUS URL::GetSortedCoverage(OpAutoVector<StorageSegment> &segments)
{
	if (rep && rep->storage && rep->storage->GetCacheStorage())
		return rep->storage->GetCacheStorage()->GetSortedCoverage(segments);
	else
		return OpStatus::ERR_NULL_POINTER;
}

void URL::GetPartialCoverage(OpFileLength position, BOOL &available, OpFileLength &length, BOOL multiple_segments)
{
	if (rep && rep->storage && rep->storage->GetCacheStorage())
	{
		rep->storage->GetCacheStorage()->GetPartialCoverage(position, available, length, multiple_segments);
	}
	else
	{
		available = FALSE;
		length = 0;
	}
}

BOOL URL::UseStreamingCache()
{
	if (rep)
	{
		if (rep->storage && rep->storage->GetCacheStorage())
		{
			BOOL streaming, ram, embedded_storage;
			rep->storage->GetCacheStorage()->GetCacheInfo(streaming, ram, embedded_storage);
			return streaming;
		}
#ifdef MULTIMEDIA_CACHE_SUPPORT
		else
		{
			BOOL ram_stream;
			return Multimedia_Storage::IsStreamRequired(rep, ram_stream);
		}
#endif // MULTIMEDIA_CACHE_SUPPORT
	}
	return FALSE;
}

OP_STATUS URL::GetNextMissingCoverage(OpFileLength &start, OpFileLength &len)
{
	if (rep && rep->storage && rep->storage->GetCacheStorage())
	{
		OpAutoVector<StorageSegment> missing;
		OpFileLength size=0;

		GetAttribute(URL::KContentSize, &size);
		if(size==0)
			size=FILE_LENGTH_NONE;  // 0 sounds suspicious, so we will assume that if we have some content, the last byte already downloaded is the end of the file

		RETURN_IF_ERROR(rep->storage->GetCacheStorage()->GetMissingCoverage(missing, 0, size));

		if(missing.GetCount()==0 || !missing.Get(0))
			start=len=FILE_LENGTH_NONE;
		else
		{
			start=missing.Get(0)->content_start;
			len=missing.Get(0)->content_length;
		}

		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NULL_POINTER;
}

#ifdef URL_ENABLE_TRACER
void URL_Tracer::SetTracker()
{
	Release();
	tracer = OP_NEWA(char,1);
	if(tracer)
		*tracer = (((INTPTR) tracer) >> 19) & 0xff;
}

void URL_InUse_Tracer::SetTracker()
{
	Release();
	tracer = OP_NEWA(char,1);
	if(tracer)
		*tracer = (((INTPTR) tracer) >> 19) & 0xff;
}

#endif
