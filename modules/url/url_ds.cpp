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

#include "modules/probetools/probepoints.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/viewers/viewers.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/dochand/docman.h"

#include "modules/url/url2.h"
#include "modules/url/protocols/http1.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/util/handy.h"

#include "modules/encodings/utility/charsetnames.h"

#include "modules/util/htmlify.h"

#include "modules/pi/network/OpSocketAddress.h"

#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
#include "modules/url/loadhandler/url_eph.h"
#endif // EXTERNAL_PROTOCOL_SCHEME_SUPPORT

#include "modules/url/url_rep.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_man.h"

#include "modules/olddebug/timing.h"

#include "modules/url/url_pd.h"
#include "modules/formats/url_dfr.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/uri_escape.h"
#include "modules/mime/mime_cs.h"
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
#include "modules/mime/mime_module.h"
#endif

#if defined SUPPORT_AUTO_PROXY_CONFIGURATION
# include "modules/autoproxy/autoproxy_public.h"
#endif

#include "modules/url/url_link.h"

#include "modules/upload/upload.h" // Upload_Element_Type
#if defined(M2_SUPPORT)
#include "adjunct/m2/src/engine/engine.h"
#endif // M2_SUPPORT

#ifdef URL_FILTER
#include "modules/content_filter/content_filter.h"
#endif // URL_FILTER

#include "modules/util/cleanse.h"

#include "modules/olddebug/tstdump.h"

#ifdef CACHE_FAST_INDEX
#include "modules/cache/simple_stream.h"
#endif

#ifdef WEBSERVER_SUPPORT
#include "modules/webserver/webserver-api.h"
#endif // WEBSERVER_SUPPORT

#include "modules/security_manager/include/security_manager.h"

#include "modules/url/url_dynattr_int.h"

#ifdef WEB_TURBO_MODE
#include "modules/obml_comm/obml_config.h"
#endif // WEB_TURBO_MODE

#include "modules/cache/url_cs.h"
#include "modules/url/tools/content_detector.h"

#ifdef SCOPE_RESOURCE_MANAGER
#include "modules/scope/scope_network_listener.h"
#endif // SCOPE_RESOURCE_MANAGER

// Url_ds.cpp

// URL Data Storage Handlers
#define MAX_LOAD_BUF_SIZE SHRT_MAX
#define SEND_REFERER 1


#ifdef _DEBUG
#ifdef YNP_WORK
#define _DEBUG_DS
#define _DEBUG_DS1
#endif
#endif

#ifdef _DEBUG
#ifdef YNP_WORK
//#define _DEBUG_RED
#endif
#endif

extern BOOL Is_Restricted_Port(ServerName *server, unsigned short, URLType);
#ifdef NEED_URL_NETWORK_ACTIVE_CHECK
extern BOOL NetworkActive();
#endif
#ifdef NEED_URL_HANDLE_UNRECOGNIZED_SCHEME
extern void HandleUnrecognisedURLTypeL(const OpStringC& aUrlName);
#endif
BOOL CheckIsDotComDomain(const OpStringC8 &domain);


static const OpMessage ds_loading_messages[] = {
    MSG_COMM_LOADING_FINISHED,
    MSG_COMM_LOADING_FAILED,
    MSG_COMM_DATA_READY
};

#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/src/WindowCommander.h"

#ifdef APPLICATION_CACHE_SUPPORT
#include "modules/applicationcache/application_cache_manager.h"
#include "modules/applicationcache/manifest.h"
#endif // APPLICATION_CACHE_SUPPORT

class URL_Dialogs : public OpDocumentListener::DialogCallback
{
private:
	OpMessage msg;
	BOOL finished;
	Reply dialog_result;
	OpDocumentListener *listener;
	WindowCommander *wic;

public:
	URL_Dialogs(OpMessage a_msg): msg(a_msg), finished(FALSE), dialog_result(REPLY_CANCEL),listener(NULL), wic(NULL) {};
	virtual ~URL_Dialogs(){
		if(listener && !finished)
			listener->OnCancelDialog(wic, this);
	}
	BOOL Configure(MsgHndlList *list);
	MH_PARAM_1 Id() const {return (MH_PARAM_1) this;}
	BOOL Finished() const {return finished;}
	Reply Result() const {return dialog_result;}
	virtual void OnDialogReply(Reply reply);

	OpDocumentListener *GetListener(){OP_ASSERT(listener); return listener;}
	WindowCommander *GetWindowCommander() const{return wic;}
};

OpDocumentListener *MhGetDocumentListener(MessageHandler *mh, WindowCommander *&wic_target)
{
	wic_target = NULL;
	if(!mh)
		return NULL;

	Window *win = mh->GetWindow();
	if(!win)
		return NULL;

	WindowCommander *wic = win->GetWindowCommander();
	if(!wic)
		return NULL;

	OpDocumentListener *listener = wic->GetDocumentListener();
	if(listener)
	{
		wic_target = wic;
		return listener;
	}
	return NULL;
}

OpDocumentListener *MhListGetFirstListener(MsgHndlList *list, WindowCommander *&wic_target)
{
	if (list)
	{
		MsgHndlList::Iterator itr;
		for(itr = list->Begin(); itr != list->End(); ++itr)
		{
			OpDocumentListener *listener = MhGetDocumentListener((*itr)->GetMessageHandler(), wic_target);
			if(listener)
				return listener;
		}
	}
	wic_target = NULL;
	return NULL;
}

BOOL URL_Dialogs::Configure(MsgHndlList *list)
{
	listener = MhListGetFirstListener(list, wic);

	OP_ASSERT(wic);
	OP_ASSERT(listener);

	return (wic != NULL && listener != NULL);
}

void URL_Dialogs::OnDialogReply(Reply reply)
{
	if(finished)
		return;

	finished = TRUE;
	dialog_result = reply;
	MH_PARAM_2 result;
	switch(reply)
	{
	case REPLY_YES:
		result = IDYES;
		break;
	case REPLY_NO:
		result = IDNO;
		break;
	case REPLY_CANCEL:
	default:
		result = IDCANCEL;
	}
	if(msg != MSG_NO_MESSAGE)
		g_main_message_handler->PostMessage(msg, Id(), result);
}

const URL_DataStorage::URL_UintAttributeEntry DataStorage_default_list[] =
{
	{URL::KCachePolicy_AlwaysVerify, FALSE},
	//{URL::KCachePolicy_MustRevalidate, FALSE},
	{URL::KCachePolicy_NoStore, FALSE},
#ifdef URL_ENABLE_ACTIVE_COMPRESS_CACHE
	{URL::KCompressCacheContent, FALSE},
#endif
	{URL::KContentType, URL_UNDETERMINED_CONTENT},
	{URL::KDisableProcessCookies, FALSE},
	{URL::KForceCacheKeepOpenFile, FALSE},
	{URL::KHeaderLoaded, FALSE},
	{URL::KIsResuming, FALSE},
	{URL::KIsThirdParty, FALSE},
	{URL::KIsThirdPartyReach, FALSE},
	{URL::KUseGenericAuthenticationHandling, FALSE},
	{URL::KLoadStatus, URL_UNLOADED},
	{URL::KReloadSameTarget, FALSE},
	{URL::KUntrustedContent, FALSE},
#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
	{URL::K_INTERNAL_Override_Reload_Protection, FALSE},
#endif
	{URL::KSecurityStatus, SECURITY_STATE_UNKNOWN},
	{URL::KMIME_CharSetId, 0},
	{URL::KAutodetectCharSetID, 0},
	{URL::KLimitNetType, NETTYPE_UNDETERMINED},
	{URL::KLoadedFromNetType, NETTYPE_UNDETERMINED},
#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
	{URL::KUsedForFileDownload, FALSE},
#endif
	{URL::KIsUserInitiated, FALSE},
	{URL::KNoInt, 0} // Termination
};


URL_DataStorage::URL_DataStorage(URL_Rep *rep)
{
	url = rep;
	InternalInit();
}

void URL_DataStorage::InternalInit()
{
	op_memset(&info, 0, sizeof(info));

	info.was_inline = TRUE;

	mh_list = NULL;
	current_dialog = NULL;

	local_time_loaded = 0;
	content_size = 0;
#ifdef WEB_TURBO_MODE
	turbo_transferred_bytes = 0;
	turbo_orig_transferred_bytes = 0;
#endif // WEB_TURBO_MODE

#ifdef USE_SPDY
	loaded_with_spdy = FALSE;
#endif // USE_SPDY
	loading = NULL;
	old_storage = NULL;
	storage = NULL;
	op_request = NULL;

	http_data = NULL;
	protocol_data.prot_data = NULL;

	local_time_loaded = 0;

#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
	load_count = 0;
#endif
#ifdef URL_ENABLE_ASSOCIATED_FILES
	assoc_files = 0;
#endif

	mime_charset = 0;
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	SetLoadDirect(FALSE);
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef _EMBEDDED_BYTEMOBILE
	predicted_depth = 0;
#endif //_EMBEDDED_BYTEMOBILE
	OP_ASSERT(((int)NETTYPE_UNDETERMINED) < 4); // Increase size of info.use_nettype; it is too small

	range_offset = 0;
	content_type_deterministic = TRUE;
	timeout_poll_idle = 0;
	timeout_max_tcp_connection_established = 0;
	info.content_type_was_null = FALSE;
}

OP_STATUS URL_DataStorage::Init()
{
	RETURN_IF_ERROR(SetAttribute(DataStorage_default_list));

	URLType type = (URLType) url->GetAttribute(URL::KType);
	switch (type)
	{
	case URL_HTTPS:
		// Leave the default UNKNOWN
		break;
	case URL_OPERA:
		RETURN_IF_ERROR(SetSecurityStatus(SECURITY_STATE_FULL));
		break;
	default:
		RETURN_IF_ERROR(SetSecurityStatus(SECURITY_STATE_NONE));
	}

	RETURN_IF_ERROR(SetAttribute(URL::KResumeSupported, (type == URL_HTTP || type == URL_HTTPS || type == URL_FTP ? Resumable_Unknown : Not_Resumable)));

	if(!mh_list)
		mh_list = OP_NEW(MsgHndlList, ());
	return (mh_list ? OpStatus::OK : OpStatus::ERR_NO_MEMORY);
}

#ifndef RAMCACHE_ONLY

const URL_DataStorage::URL_UintAttributeEntry DataStorage_rec_list[] = {
	{URL::KLoadStatus, TAG_STATUS},
	{URL::KCachePolicy_AlwaysVerify, TAG_CACHE_ALWAYS_VERIFY},
	{URL::KCachePolicy_MustRevalidate, TAG_CACHE_MUST_VALIDATE},
	{URL::KLoadedFromNetType, TAG_CACHE_LOADED_FROM_NETTYPE},
	{URL::KNoInt, 0} // Termination
};

const URL_DataStorage::URL_StringAttributeRecEntry DataStorage_rec_list2[] = {
	{URL::KMIME_Type, TAG_CONTENT_TYPE},
	{URL::KMIME_CharSet, TAG_CHARSET},
	{URL::KNoStr, 0} // Termination
};

const URL_DataStorage::URL_UintAttributeEntry DataStorage_http_rec_list[] = {
	{URL::KHTTP_Response_Code, TAG_HTTP_RESPONSE},
	{URL::KHTTP_Age, TAG_HTTP_AGE},
	{URL::KUsedUserAgentId, TAG_USERAGENT_ID},
	{URL::KUsedUserAgentVersion, TAG_USERAGENT_VER_ID},
	{URL::KNoInt, 0} // Termination
};

const URL_DataStorage::URL_StringAttributeRecEntry DataStorage_http_rec_list2[] = {
	{URL::KHTTP_Date, TAG_HTTP_KEEP_LOAD_DATE},
	{URL::KHTTP_LastModified, TAG_HTTP_KEEP_LAST_MODIFIED},
	{URL::KHTTPEncoding, TAG_HTTP_KEEP_ENCODING},
	{URL::KHTTP_EntityTag, TAG_HTTP_KEEP_ENTITY_TAG},
	{URL::KHTTP_MovedToURL_Name, TAG_HTTP_MOVED_TO_URL},
	{URL::KHTTPResponseText, TAG_HTTP_RESPONSE_TEXT},
	{URL::KHTTPRefreshUrlName, TAG_HTTP_REFRESH_URL},
	{URL::KHTTPContentLocation, TAG_HTTP_CONTENT_LOCATION},
	{URL::KHTTPContentLanguage, TAG_HTTP_CONTENT_LANGUAGE},
	{URL::KNoStr, 0} // Termination
};

#ifndef SEARCH_ENGINE_CACHE
void URL_DataStorage::InitL(DataFile_Record *rec, FileName_Store &filenames, OpFileFolder folder)
#else
void URL_DataStorage::InitL(DataFile_Record *rec, OpFileFolder folder)
#endif
{
	LEAVE_IF_ERROR(Init());

	SetAttributeL(DataStorage_rec_list, rec);
	SetAttributeL(DataStorage_rec_list2, rec);

	content_size = rec->GetIndexedRecordUInt64L(TAG_CONTENT_SIZE);
	local_time_loaded = (time_t) rec->GetIndexedRecordUInt64L(TAG_LOCALTIME_LOADED);

	URLType url_type = (URLType) url->GetAttribute(URL::KType);
	if (url_type == URL_HTTPS)
		SetAttributeL(URL::KSecurityStatus, SECURITY_STATE_LOW);
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	SetAttribute(URL::KNeverFlush, folder == OPFILE_OCACHE_FOLDER ? TRUE : FALSE);

	if(rec->GetIndexedRecordBOOL(TAG_CACHE_NEVER_FLUSH) &&
		url->GetAttribute(URL::KServerName, NULL) && ((ServerName *)url->GetAttribute(URL::KServerName, NULL))->TestAndSetNeverFlushTrust())
	{
		SetNeverFlush(TRUE);
	}
#endif

#ifdef URL_ENABLE_ASSOCIATED_FILES
	assoc_files = rec->GetIndexedRecordUIntL(TAG_ASSOCIATED_FILES);
#endif

	DataFile_Record *httprec = rec->GetIndexedRecord(TAG_HTTP_PROTOCOL_DATA);
	if(httprec != NULL && GetHTTPProtocolDataL())
	{
		httprec->IndexRecordsL();

		SetAttributeL(DataStorage_http_rec_list, httprec);
		SetAttributeL(DataStorage_http_rec_list2, httprec);

		http_data->keepinfo.expires = (time_t) rec->GetIndexedRecordUInt64L(TAG_HTTP_KEEP_EXPIRES);

		if(GetAttribute(URL::KHTTPRefreshUrlName).HasContent())
		{
			SetAttributeL(URL::KHTTP_Refresh_Interval, httprec->GetIndexedRecordIntL(TAG_HTTP_REFRESH_INT));
		}
		{
			OpStringS8 tmp;
			ANCHOR(OpStringS8,tmp);

			httprec->GetIndexedRecordStringL(TAG_HTTP_SUGGESTED_NAME, tmp);
			http_data->recvinfo.suggested_filename.SetFromUTF8L(tmp.CStr());
		}
		{
			DataFile_Record *http_link = httprec->GetIndexedRecord(TAG_HTTP_LINK_RELATION);
			while(http_link)
			{
				OpStackAutoPtr<HTTP_Link_Relations> link(NULL);

				link.reset(OP_NEW_L(HTTP_Link_Relations, ()));

				link->InitL(http_link);

				link->Into(&http_data->recvinfo.link_relations);
				link.release();

				http_link = httprec->GetIndexedRecord(http_link);
			}
		}

		DataFile_Record *httpheader_rec= httprec->GetIndexedRecord(TAG_HTTP_RESPONSE_HEADERS);
		if(httpheader_rec)
		{
			OpStackAutoPtr<HeaderList> headers(OP_NEW_L(HeaderList, ()));

			OpStackAutoPtr<DataFile_Record> httpheader_entry(httpheader_rec->GetNextRecordL());
			while(httpheader_entry.get() != NULL)
			{
				if(httpheader_entry->GetTag() == TAG_HTTP_RESPONSE_ENTRY)
				{
					OpString8 name;
					ANCHOR(OpString8, name);
					OpString8 val;
					ANCHOR(OpString8, val);

					httpheader_entry->IndexRecordsL();
					httpheader_entry->GetIndexedRecordStringL(TAG_HTTP_RESPONSE_HEADER_NAME, name);
					httpheader_entry->GetIndexedRecordStringL(TAG_HTTP_RESPONSE_HEADER_VALUE, val);

					if(name.HasContent())
					{
						OpStackAutoPtr<HeaderEntry> hdr(OP_NEW_L(HeaderEntry, ()));

						hdr->ConstructFromNameAndValueL(*headers, name.CStr(),val.CStr(), val.Length());
						hdr.release();
					}
				}
				httpheader_entry.reset(httpheader_rec->GetNextRecordL());
			}

			if(!headers->Empty())
				http_data->recvinfo.all_headers = headers.release();
		}
	}
	URLContentType tmp_url_ct;

	OpString temp_path;
	ANCHOR(OpString, temp_path);

	LEAVE_IF_ERROR(url->GetAttribute(URL::KUniPath, 0, temp_path));

	LEAVE_IF_ERROR(FindContentType(tmp_url_ct, content_type_string.CStr(), 0, temp_path.CStr()));
	SetAttributeL(URL::KContentType, tmp_url_ct);

#ifndef RAMCACHE_ONLY
	OpString tempname;
	ANCHOR(OpString, tempname);

	OpString8 tempname1;
	ANCHOR(OpString8, tempname1);
	rec->GetIndexedRecordStringL(TAG_CACHE_FILENAME, tempname1);

	tempname.SetFromUTF8L(tempname1);

	if(tempname.HasContent())
	{
        if(rec->GetIndexedRecord(TAG_CACHE_DOWNLOAD_FILE))
		{
			OpFile temp_file;
			ANCHOR(OpFile, temp_file);
			LEAVE_IF_ERROR(temp_file.Construct(tempname));

			BOOL exists = FALSE;

			OpStatus::Ignore(temp_file.Exists(exists));
			if(exists)
			{
				storage = Download_Storage::Create(url, tempname.CStr());
				LEAVE_IF_NULL(storage);
			}

			DataFile_Record *resumerec = rec->GetIndexedRecord(TAG_DOWNLOAD_RESUMABLE_STATUS);
			SetAttributeL(URL::KResumeSupported,( resumerec ? (URL_Resumable_Status) resumerec->GetUInt32L() : Resumable_Unknown));

			if(url_type == URL_FTP)
			{
				OpStringS8 temp_mdtm;
				ANCHOR(OpStringS8, temp_mdtm);

				rec->GetIndexedRecordStringL(TAG_DOWNLOAD_FTP_MDTM_DATE, temp_mdtm);

				if(temp_mdtm.HasContent())
				{
					URL_FTP_ProtocolData *ftp_ptr = GetFTPProtocolDataL();
					if(ftp_ptr)
					{
						ftp_ptr->MDTM_date.TakeOver(temp_mdtm);
					}
				}
			}
		}
		else
		{
		#ifdef MULTIMEDIA_CACHE_SUPPORT
			if(url->GetAttribute(URL::KMultimedia))
			{
				#ifndef SEARCH_ENGINE_CACHE
					storage = Multimedia_Storage::Create(url, filenames, folder, tempname.CStr(), content_size);
				#else
					storage = Multimedia_Storage::Create(url, folder, tempname.CStr(), content_size);
				#endif
			}
			else
		#endif // MULTIMEDIA_CACHE_SUPPORT
			{
				#ifndef SEARCH_ENGINE_CACHE
					storage = Persistent_Storage::Create(url, filenames, folder, tempname.CStr(), content_size);
				#else
					storage = Persistent_Storage::Create(url, folder, tempname.CStr(), content_size);
				#endif
			}

			LEAVE_IF_NULL(storage);

#ifdef URL_ENABLE_ASSOCIATED_FILES
			storage->SetPurgeAssociatedFiles(assoc_files);
#endif

			if(http_data)
			{
				storage->SetHTTPResponseCode(http_data->recvinfo.response);
				storage->TakeOverContentEncoding(http_data->keepinfo.encoding);
			}
		}
	}

	if(storage)
	{
		urlManager->SetLRU_Item(this);
	}
#endif
	if((URLContentType) GetAttribute(URL::KContentType) == URL_UNDETERMINED_CONTENT)
	{
		ContentDetector content_detector(url, TRUE, TRUE);
		LEAVE_IF_ERROR(content_detector.DetectContentType());
	}


	uint32 dyn_attr_item;
	for (dyn_attr_item = TAG_CACHE_DYNATTR_FLAG_ITEM; dyn_attr_item <= TAG_CACHE_DYNATTR_LAST_ITEM; dyn_attr_item++)
	{
		DataFile_Record *attr_item = rec->GetIndexedRecord(dyn_attr_item);

		while(attr_item)
		{
			uint32 mod_id = ((dyn_attr_item & 0x01) != 0 ? attr_item->GetUInt16L() /* long form */ : attr_item->GetUInt8L() /* short form*/);
			uint32 tag_id = ((dyn_attr_item & 0x01) != 0 ? attr_item->GetUInt16L() /* long form */ : attr_item->GetUInt8L() /* short form*/);;
			BOOL is_url_string = FALSE;

			switch(dyn_attr_item)
			{
			case TAG_CACHE_DYNATTR_FLAG_ITEM:
			case TAG_CACHE_DYNATTR_FLAG_ITEM_Long:
				AddDynamicAttributeFlagL(mod_id, tag_id);
				break;

			case TAG_CACHE_DYNATTR_INT_ITEM:
			case TAG_CACHE_DYNATTR_INT_ITEM_Long:
				AddDynamicAttributeIntL(mod_id, tag_id, attr_item->GetUInt32L(TRUE));
				break;

			case TAG_CACHE_DYNATTR_URL_ITEM:
			case TAG_CACHE_DYNATTR_URL_ITEM_Long:
				is_url_string = TRUE;
				// Fall through to string handling
			case TAG_CACHE_DYNATTR_STRING_ITEM:
			case TAG_CACHE_DYNATTR_STRING_ITEM_Long:
				{
					OpString8 val;
					ANCHOR(OpString8, val);

					attr_item->GetStringL(val);

					if(is_url_string)
						AddDynamicAttributeURL_L(mod_id, tag_id, val);
					else
						AddDynamicAttributeStringL(mod_id, tag_id, val);
				}
				break;
			}
			attr_item = rec->GetIndexedRecord(attr_item);
		}
	}

}

#ifdef CACHE_FAST_INDEX
void URL_DataStorage::InitL(DiskCacheEntry *entry,
#ifndef SEARCH_ENGINE_CACHE
							FileName_Store &filenames,
#endif
							OpFileFolder folder)
{
	LEAVE_IF_ERROR(Init());

	SetAttributeL(DataStorage_rec_list, entry);
	SetAttributeL(DataStorage_rec_list2, entry);

	content_size = entry->GetContentSize();
	local_time_loaded = entry->GetLocalTime();

	URLType url_type = (URLType) url->GetAttribute(URL::KType);
	if (url_type == URL_HTTPS)
		SetAttributeL(URL::KSecurityStatus, SECURITY_STATE_LOW);
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	SetAttribute(URL::KNeverFlush, folder == OPFILE_OCACHE_FOLDER ? TRUE : FALSE);

	if(entry->GetNeverFlush() &&
		url->GetAttribute(URL::KServerName, NULL) && ((ServerName *)url->GetAttribute(URL::KServerName, NULL))->TestAndSetNeverFlushTrust())
	{
		SetNeverFlush(TRUE);
	}
#endif

#ifdef URL_ENABLE_ASSOCIATED_FILES
	assoc_files = entry->GetAssociatedFiles();
#endif

	//DataFile_Record *httprec = rec->GetIndexedRecord(TAG_HTTP_PROTOCOL_DATA);
	HTTPCacheEntry *http=entry->GetHTTP();
	if(http && http->IsDataAvailable() && GetHTTPProtocolDataL())
	{
		SetAttributeL(DataStorage_http_rec_list, http);
		SetAttributeL(DataStorage_http_rec_list2, http);

		http_data->keepinfo.expires = (time_t) entry->GetExpiry();

		if(GetAttribute(URL::KHTTPRefreshUrlName).HasContent())
		{
			SetAttributeL(URL::KHTTP_Refresh_Interval, http->GetRefreshInterval());
		}
		{
			OpStringS8 *tmp=http->GetSuggestedName();

			if(tmp)
				http_data->recvinfo.suggested_filename.SetFromUTF8L(tmp->CStr());
		}
		{
			int num_relations=http->GetLinkrelationsCount();
			int cur=0;
			OpString8 *rel;

			while(cur<num_relations && (rel=http->GetLinkrelation(cur))!=NULL)
			{
				OpStackAutoPtr<HTTP_Link_Relations> link(NULL);

				link.reset(OP_NEW_L(HTTP_Link_Relations, ()));

				cur++;

				OP_ASSERT(rel);
				if(rel)
					link->InitL(*rel);

				link->Into(&http_data->recvinfo.link_relations);
				link.release();
			}
		}

		if(http && http->GetHeadersCount())
		{
			OpStackAutoPtr<HeaderList> headers(OP_NEW_L(HeaderList, ()));
			//OpStackAutoPtr<DataFile_Record> httpheader_entry(httpheader_rec->GetNextRecordL());

			for(int i=0, num_headers=http->GetHeadersCount(); i<num_headers; i++)
			{
				HTTPHeaderEntry *header=http->GetHeader(i);

				OP_ASSERT(header);
				if(header)
				{
					// NOTABENE: Original code checked against TAG_HTTP_RESPONSE_ENTRY, now it is the only possibility...
					{
						OpString8 *name;
						OpString8 *val;

						name=header->GetName();
						val=header->GetValue();

						if(name->HasContent())
						{
							OpStackAutoPtr<HeaderEntry> hdr(OP_NEW_L(HeaderEntry, ()));

							hdr->ConstructFromNameAndValueL(*headers, name->CStr(),val->CStr(), val->Length());
							hdr.release();
						}
					}

					//httpheader_entry.reset(httpheader_rec->GetNextRecordL());
				}
			}

			if(!headers->Empty())
				http_data->recvinfo.all_headers = headers.release();
		}
	}

	URLContentType tmp_url_ct;

	OpString temp_path;
	ANCHOR(OpString, temp_path);

	LEAVE_IF_ERROR(url->GetAttribute(URL::KUniPath, 0, temp_path));

	LEAVE_IF_ERROR(FindContentType(tmp_url_ct, content_type_string.CStr(), 0, temp_path.CStr()));
	SetAttributeL(URL::KContentType, tmp_url_ct);

	SetAttributeL(URL::KServerMIME_Type, *entry->GetServerContentType());

#ifndef RAMCACHE_ONLY
	OpString tempname;
	ANCHOR(OpString, tempname);

	OpString8 *tempname1;
	tempname1=entry->GetFileName();

	tempname.SetFromUTF8L(tempname1->CStr());

	if(entry->GetTag() != TAG_CACHE_APPLICATION_CACHE_ENTRY && (tempname.HasContent() || entry->GetEmbeddedContentSize()))
	{
		// TODO: check...
		//if(rec->GetIndexedRecord(TAG_CACHE_DOWNLOAD_FILE))
		if(entry->GetDownload())  // The file is a download...
		{
			OpFile temp_file;
			ANCHOR(OpFile, temp_file);
			LEAVE_IF_ERROR(temp_file.Construct(tempname));

			BOOL exists = FALSE;

			OpStatus::Ignore(temp_file.Exists(exists));
			if(exists)
			{
				storage = Download_Storage::Create(url, tempname.CStr());
				LEAVE_IF_NULL(storage);
			}

			SetAttributeL(URL::KResumeSupported,entry->GetResumable());

			if(url_type == URL_FTP)
			{
				OpStringS8 *temp_mdtm;

				temp_mdtm=entry->GetFTPModifiedDateTime();

				if(temp_mdtm && temp_mdtm->HasContent())
				{
					URL_FTP_ProtocolData *ftp_ptr = GetFTPProtocolDataL();
					if(ftp_ptr)
					{
						ftp_ptr->MDTM_date.TakeOver(*temp_mdtm);
					}
				}
			}
		}
		else
		{
			// BOOL force_disk=FALSE;	// TRUE to put the file on disk;
			// this "experimental code" is not working properly, and it
			// slows down Opera becasue of the need to store the files.
			// So the embedded files exceeding the memory limit will be dropped.
			// This is not supposed to be a real case scenario
			BOOL used=FALSE;

#if CACHE_SMALL_FILES_SIZE>0
			// The file is embedded: check if it can be embedded or the memory limit has ebeen reached
			// This situation is not supposed to happen, because CACHE_SMALL_FILES_LIMIT is a TWEAK
			if(!tempname.HasContent() && (urlManager->GetEmbeddedSize()+entry->GetEmbeddedContentSize()>CACHE_SMALL_FILES_LIMIT))
			{
				// The file cannot be embedded: push it to the disk!
				//force_disk=TRUE;
				entry->SetEmbeddedContentSize(0);   // Drop the file from the cache
			}
#endif

			if(entry->GetTag() != TAG_CACHE_APPLICATION_CACHE_ENTRY && (tempname.HasContent() || /*force_disk ||*/ entry->GetEmbeddedContentSize()))
			{
				if(tempname.HasContent()/* || force_disk*/)  // Store on disk
				{
					used=TRUE;
				#ifdef MULTIMEDIA_CACHE_SUPPORT
					if(url->GetAttribute(URL::KMultimedia))
					{
						#ifndef SEARCH_ENGINE_CACHE
							storage = Multimedia_Storage::Create(url, filenames, folder, tempname.CStr(), content_size);
						#else
							storage = Multimedia_Storage::Create(url, folder, tempname.CStr(), content_size);
						#endif
					}
					else
				#endif // MULTIMEDIA_CACHE_SUPPORT
					{
						#ifndef SEARCH_ENGINE_CACHE
							storage = Persistent_Storage::Create(url, filenames, folder, tempname.CStr(), content_size);
						#else
							storage = Persistent_Storage::Create(url, folder, tempname.CStr(), content_size);
						#endif

					}

					/*if(force_disk)
					{
					LEAVE_IF_ERROR(storage->StoreData(entry->GetEmbeddedContent(), entry->GetEmbeddedContentSize()));
					storage->SetFinished(TRUE);
					}*/
				}
#if CACHE_SMALL_FILES_SIZE>0
				else // Embedded file
				{
					// Create a persistent storage even if the content is kept in memory
					OP_ASSERT(entry->GetEmbeddedContentSize());

					used=TRUE;
					storage=OP_NEW_L(Embedded_Files_Storage, (url));
					//storage=OP_NEW(Memory_Only_Storage, (url));

					LEAVE_IF_ERROR(storage->StoreEmbeddedContent(entry));
				}
#endif

				if(used) // Common operations
				{
					LEAVE_IF_NULL(storage);

#ifdef URL_ENABLE_ASSOCIATED_FILES
					storage->SetPurgeAssociatedFiles(assoc_files);
#endif

					if(http_data)
					{
						storage->SetHTTPResponseCode(http_data->recvinfo.response);
						storage->TakeOverContentEncoding(http_data->keepinfo.encoding);
					}

#if CACHE_CONTAINERS_ENTRIES>0
					if(entry->GetContainerID())
						storage->SetContainerID(entry->GetContainerID());
#endif

					/*if(force_disk)
					storage->Flush();  // Save the memory content on the disk*/
				}
			}
		}
	}

	if(storage)
	{
		urlManager->SetLRU_Item(this);
	}
#endif // RAMCACHE_ONLY
	if((URLContentType) GetAttribute(URL::KContentType) == URL_UNDETERMINED_CONTENT)
	{
		ContentDetector content_detector(url, TRUE, TRUE);
		LEAVE_IF_ERROR(content_detector.DetectContentType());
	}

	if(entry->GetMultimedia())
		info.multimedia=TRUE;
	if(entry->GetContentTypeWasNULL())
		info.content_type_was_null=TRUE;
		
	// Dynamic attributes
	for(int att_idx=0, dyn_cnt=entry->GetDynamicAttributesCount(); att_idx<dyn_cnt; att_idx++)
	{
		StreamDynamicAttribute *att=entry->GetDynamicAttribute(att_idx);

		if(att)
		{
			UINT32 tag=att->GetTag();
			UINT32 mod_id=att->GetModuleID();
			UINT32 tag_id=att->GetTagID();

			switch(tag)
			{
			case TAG_CACHE_DYNATTR_FLAG_ITEM:
			case TAG_CACHE_DYNATTR_FLAG_ITEM_Long:
				AddDynamicAttributeFlagL(mod_id, tag_id);
				break;

			case TAG_CACHE_DYNATTR_INT_ITEM:
			case TAG_CACHE_DYNATTR_INT_ITEM_Long:
				AddDynamicAttributeIntL(mod_id, tag_id, ((StreamDynamicAttributeInt *)att)->GetValue());
				break;

			case TAG_CACHE_DYNATTR_STRING_ITEM:
			case TAG_CACHE_DYNATTR_STRING_ITEM_Long:
				AddDynamicAttributeStringL(mod_id, tag_id, ((StreamDynamicAttributeString *)att)->GetValue());
				break;

			case TAG_CACHE_DYNATTR_URL_ITEM:
			case TAG_CACHE_DYNATTR_URL_ITEM_Long:
				AddDynamicAttributeURL_L(mod_id, tag_id, ((StreamDynamicAttributeString *)att)->GetValue());
				break;
			}
		}
	}
}
#endif // CACHE_FAST_INDEX
#endif // RAMCACHE_ONLY

URL_DataStorage::~URL_DataStorage()
{
	InternalDestruct();
}

void URL_DataStorage::InternalDestruct()
{
	if(current_dialog)
	{
		OP_DELETE(current_dialog);
		current_dialog = NULL;
	}

	if(g_main_message_handler)
	{
		g_main_message_handler->UnsetCallBacks(this);
		g_main_message_handler->RemoveDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 0);
		g_main_message_handler->RemoveDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 1);
	}

	//delete [] buf;
	DeleteLoading();

	if (mh_list)
	{
		MessageHandler* mh = mh_list->FirstMsgHandler();

		while(mh != NULL)
		{
			mh_list->Remove(mh);
			mh->UnsetCallBacks(this);
			mh = mh_list->FirstMsgHandler();
		}
		OP_DELETE(mh_list);
	}

	DataStream_Conditional_Sensitive_ByteArray(cache_destruct, GetAttribute(URL::KCachePolicy_NoStore));

	// If required, manually add the Object size estimation to the RAM cache, as there is no cache storage
	if(info.cache_size_added)
	{
		OP_ASSERT(!storage);
		
		Context_Manager *ctx = urlManager->FindContextManager(url->GetContextId());

		if(ctx)
		{
			ctx->SubFromRamCacheSize(0, url);
#ifdef SELFTEST
			ctx->VerifyURLSizeNotCounted(url, TRUE, TRUE);
#endif // SELFTEST
		}

	}

	OP_DELETE(http_data);
	OP_DELETE(protocol_data.prot_data);
	OP_DELETE(old_storage);
	OP_DELETE(storage);

	old_storage=NULL;
	storage=NULL;

	SetAttribute(URL::KLoadStatus, URL_UNLOADED);
	if(InList())
		urlManager->RemoveLRU_Item(this);

	if(g_charsetManager)
	{
		g_charsetManager->DecrementCharsetIDReference(mime_charset);
	}
#ifdef SCOPE_RESOURCE_MANAGER
	if (OpScopeResourceListener::IsEnabled())
		OpScopeResourceListener::OnUrlUnload(url);
#endif // SCOPE_RESOURCE_MANAGER
}

// FIXME: Make inline

void URL_DataStorage::OnLoadStarted(MessageHandler *mh)
{
	OP_ASSERT(url);
	url->OnLoadStarted(mh);
}

void URL_DataStorage::OnLoadFinished(URLLoadResult result, MessageHandler* mh)
{
	OP_ASSERT(url);
	url->OnLoadFinished(result, mh);
}

void URL_DataStorage::OnLoadFailed()
{
	UnsetListCallbacks();
	mh_list->Clean();
	OP_ASSERT(url);
	url->OnLoadFinished(URL_LOAD_FAILED);
}

CommState URL_DataStorage::Reload(
		MessageHandler* msg_handler, const URL& p_referer_url,
		BOOL only_if_modified, BOOL proxy_no_cache,
		BOOL user_initiated, BOOL thirdparty_determined, EnteredByUser entered_by_user, BOOL inline_loading, BOOL* allow_if_modified)
{
	URL referer_url = p_referer_url;
	URLType type = (URLType) url->GetAttribute(URL::KType);

#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
	URLStatus status = (URLStatus) GetAttribute(URL::KLoadStatus);
	if(load_count && status != URL_LOADING && status != URL_UNLOADED)
	{
		msg_handler->PostMessage(MSG_HEADER_LOADED, url->GetID(), GetAttribute(URL::KIsFollowed) ? 0 : 1);
		msg_handler->PostMessage(MSG_URL_DATA_LOADED, url->GetID(), FALSE);

		return COMM_LOADING;
	}
#endif

	if (type == URL_HTTP || type == URL_HTTPS)
	{
		if (allow_if_modified)
			*allow_if_modified = FALSE;
		if(http_data || only_if_modified || proxy_no_cache)
		{
			if(only_if_modified && (URLCacheType) GetAttribute(URL::KCacheType) == URL_CACHE_USERFILE)
				only_if_modified = FALSE;
			if(GetHTTPProtocolData())
			{
				http_data->flags.only_if_modified = only_if_modified;
				http_data->flags.proxy_no_cache = proxy_no_cache;
				info.proxy_no_cache  = proxy_no_cache;
			}
		}

		/*
		// Disabled 24112004 YNP. May no longer server any real purpose except for restart(and resume) transfer
		// and do cause problems for some purposes (e.g bug #157759)
		if(entered_by_user == NotEnteredByUser && referer_url.IsEmpty())
			referer_url = GetAttribute(URL::KReferrerURL);
			*/
	}
	else if(type == URL_FTP)
		info.proxy_no_cache  = proxy_no_cache;


	SetAttribute(URL::KReloading,TRUE);

	CommState stat = Load_Stage1(msg_handler, referer_url, user_initiated,thirdparty_determined, inline_loading);
	if (stat != COMM_REQUEST_FAILED && allow_if_modified && only_if_modified && (type == URL_HTTP || type == URL_HTTPS))
	{
		if(GetHTTPProtocolData())
			*allow_if_modified = http_data->flags.added_conditional_headers;
	}
	return stat;
}

CommState URL_DataStorage::ResumeLoad(
		MessageHandler* msg_handler, const URL& referer_url)
{
	URLType type = (URLType) url->GetAttribute(URL::KType);

	if(type != URL_HTTP && type != URL_HTTPS
#if defined FTP_RESUME && !defined NO_FTP_SUPPORT
		&& type != URL_FTP
#endif
		)
		return Reload(msg_handler, referer_url,TRUE, FALSE, TRUE, FALSE, NotEnteredByUser, TRUE);

#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
	URLStatus status = (URLStatus) GetAttribute(URL::KLoadStatus);
	if(load_count && status != URL_LOADING && status != URL_UNLOADED)
	{
		msg_handler->PostMessage(MSG_HEADER_LOADED, url->GetID(), GetAttribute(URL::KIsFollowed) ? 0 : 1);
		msg_handler->PostMessage(MSG_URL_DATA_LOADED, url->GetID(), FALSE);

		return COMM_LOADING;
	}
#endif
	HTTP_Method method = (HTTP_Method) GetAttribute(URL::KHTTP_Method);
	if(method == HTTP_METHOD_POST || method == HTTP_METHOD_PUT)
		return COMM_REQUEST_FAILED;

	SetAttribute(URL::KIsResuming,TRUE);
	SetAttribute(URL::KReloadSameTarget,TRUE);

	return Load(msg_handler, referer_url,  TRUE, FALSE, TRUE);
}

BOOL URL_DataStorage::QuickLoad(BOOL guess_content_type)
{
	OP_ASSERT((URLType) url->GetAttribute(URL::KType) == URL_FILE || (URLType) url->GetAttribute(URL::KType) == URL_DATA);

	OnLoadStarted();

	URLStatus status = (URLStatus) GetAttribute(URL::KLoadStatus);
	if(status == URL_LOADING
#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
		|| (load_count && status != URL_UNLOADED)
#endif
		)
		return TRUE;

#ifdef SUPPORT_DATA_URL
	if((URLType) url->GetAttribute(URL::KType) == URL_DATA)
	{
		URL empty_url;
		StartLoadingDataURL(empty_url);
		return TRUE;
	}
#endif

#ifdef _LOCALHOST_SUPPORT_
	SetAttribute(URL::KHeaderLoaded,FALSE);
	SetAttribute(URL::KLoadStatus,URL_LOADING);
	if(loading)
	{
		OP_ASSERT(0); // Should never happen
		DeleteLoading();
	}

	MoveCacheToOld();
#ifdef DOM_GADGET_FILE_API_SUPPORT
	URLType urlType = (URLType)url->GetAttribute(URL::KType);

	if (urlType == URL_MOUNTPOINT)
	{
		StartLoadingMountPoint(guess_content_type);
	}
	else
#endif // DOM_GADGET_FILE_API_SUPPORT
		StartLoadingFile(guess_content_type, FALSE);

	if(loading)
	{
		DeleteLoading();
		return FALSE;
	}

	return (storage ? TRUE : FALSE);
#else
	return FALSE;
#endif // _LOCALHOST_SUPPORT_
}

CommState URL_DataStorage::Load(MessageHandler* msg_handler, const URL& referer_url,
								BOOL user_initiated, BOOL thirdparty_determined, BOOL inline_item)
{
#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
	URLStatus status = (URLStatus) GetAttribute(URL::KLoadStatus);
	if(load_count && status != URL_LOADING && status != URL_UNLOADED &&
		(!info.overide_load_protection
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
		|| url->GetAttribute(g_mime_module.GetInternalRedirectAttribute())
#endif
		))
	{
		msg_handler->PostMessage(MSG_HEADER_LOADED, url->GetID(), GetAttribute(URL::KIsFollowed) ? 0 : 1);
		msg_handler->PostMessage(MSG_URL_DATA_LOADED, url->GetID(), FALSE);

		return COMM_LOADING;
	}
	info.overide_load_protection = FALSE;
#endif

	if(http_data && (URLStatus) GetAttribute(URL::KLoadStatus) != URL_LOADING)
	{
		http_data->flags.only_if_modified = FALSE;
		http_data->flags.proxy_no_cache = FALSE;
		info.proxy_no_cache = FALSE;
	}

	return Load_Stage1(msg_handler, referer_url, user_initiated, thirdparty_determined, inline_item);
}

CommState URL_DataStorage::Load_Stage1(MessageHandler* msg_handler, const URL& referer_url,
								BOOL user_initiated, BOOL thirdparty_determined, BOOL inline_item)
{
	if(msg_handler == NULL)
		return COMM_REQUEST_FAILED;

#ifdef APPLICATION_CACHE_SUPPORT
	ApplicationCacheManager::ApplicationCacheNetworkStatus appcache_load_status = g_application_cache_manager->CheckApplicationCacheNetworkModel(url);
	switch (appcache_load_status) {
	case ApplicationCacheManager::NOT_APPLICATION_CACHE:
	case ApplicationCacheManager::LOAD_FROM_NETWORK_OBEY_NORMAL_CACHE_RULES:
		{
			break;
		}
	case ApplicationCacheManager::LOAD_FROM_APPLICATION_CACHE:
		{
			OpStatus::Ignore(SetAttribute(URL::KReloading,FALSE));
			OP_ASSERT(GetAttribute(URL::KUnique) == FALSE);
			OP_ASSERT((URLStatus) url->GetAttribute(URL::KLoadStatus, URL::KNoRedirect) == URL_LOADED);
//			SetAttribute(URL::KHeaderLoaded, TRUE);
			msg_handler->PostMessage(MSG_HEADER_LOADED, url->GetID(), TRUE);
			msg_handler->PostMessage(MSG_URL_DATA_LOADED, url->GetID(), FALSE);
			return COMM_LOADING;
		}

	case ApplicationCacheManager::LOAD_FAIL:
		{
			OP_ASSERT(GetAttribute(URL::KUnique) == FALSE);
			SetAttribute(URL::KLoadStatus, URL_LOADING_FAILURE);
			msg_handler->PostMessage(MSG_URL_LOADING_FAILED, url->GetID(), URL_ERRSTR(SI, ERR_COMM_CONNECT_FAILED));
			return COMM_REQUEST_FAILED;
			break;
		}
	case ApplicationCacheManager::RELOAD_FROM_NETWORK:
		{
			http_data->flags.only_if_modified = FALSE;
			break;
		}

	default:
			break;
	}
#endif // APPLICATION_CACHE_SUPPORT

	URLType type = (URLType) url->GetAttribute(URL::KType);
    URLStatus status = (URLStatus) GetAttribute(URL::KLoadStatus);
#ifdef OPERA_PERFORMANCE
	if ((type == URL_HTTP || type == URL_HTTPS) && GetHTTPProtocolData())
		GetHTTPProtocolData()->keepinfo.time_request_created.timestamp_now();
#endif // OPERA_PERFORMANCE

#ifdef GOGI_URL_FILTER
	OpStatus::Ignore(url->SetAttribute(URL::KAllowed, 0));
#endif // GOGI_URL_FILTER

#ifdef _DEBUG_DS1
	OpString8 tempname;
	url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, tempname);
	PrintfTofile("urlds1.txt","\nDS Load- %s\n", tempname.CStr());
#endif

	if (status != URL_LOADING)
		if (!mh_list->IsEmpty())
			mh_list->Clean(); // this should not be necessary !!!

	if(Is_Restricted_Port((ServerName *) url->GetAttribute(URL::KServerName, NULL), (unsigned short) url->GetAttribute(URL::KServerPort), type))
	{
		SetAttribute(URL::KLoadStatus, URL_LOADING_ABORTED);

		SendMessages(msg_handler, FALSE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_ILLEGAL_PORT));
		return COMM_REQUEST_FAILED;
	}

	switch (type)
	{
#ifdef _MIME_SUPPORT_
	case URL_NEWSATTACHMENT:
	case URL_SNEWSATTACHMENT:
		SendMessages(msg_handler, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_ILLEGAL_URL));
		return COMM_LOADING;
	case URL_ATTACHMENT:
	case URL_CONTENT_ID:
		if(status == URL_UNLOADED)
		{
			SendMessages(msg_handler, FALSE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_NO_CONTENT));
			return COMM_LOADING;
		}
		if (status == URL_LOADING)
		{
			BOOL has_mh = mh_list->HasMsgHandler(msg_handler, FALSE, FALSE, FALSE);

			if (!has_mh)
				RAISE_AND_RETURN_VALUE_IF_ERROR(AddMessageHandler(msg_handler), COMM_REQUEST_FAILED);
		}
		if(GetAttribute(URL::KHeaderLoaded))
		{
			msg_handler->PostMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1);
			msg_handler->PostMessage(MSG_URL_DATA_LOADED, url->GetID(), 0);
		}
		if ((URLContentType) GetAttribute(URL::KContentType) == URL_UNDETERMINED_CONTENT)
		{
			ContentDetector content_detector(url, TRUE, TRUE);
			if(OpStatus::IsError(content_detector.DetectContentType()))
			{
				SendMessages(msg_handler, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				return COMM_LOADING;
			}
		}
		return COMM_LOADING;
#else
	case URL_NEWS:
	case URL_SNEWS:
	case URL_NEWSATTACHMENT:
	case URL_SNEWSATTACHMENT:
	case URL_ATTACHMENT:
	case URL_CONTENT_ID:
	case URL_MAILTO:
		// FIXME: NEW API
		SendMessages(msg_handler, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_ILLEGAL_URL));
		return COMM_LOADING;
#endif
	default:
		break;
	}

	OpStringC8 hostname = url->GetAttribute(URL::KHostName);
	URL tmpUrl(url, (char*)0);

	if(status != URL_LOADING &&
		urlManager->IsOffline(tmpUrl) &&
		!urlManager->OfflineLoadable(type) &&

		!(hostname.Compare("localhost") == 0 || hostname.Compare("127.0.0.1") == 0)

		)
	{
		if(urlManager->IsOffline(tmpUrl))
        {
			SendMessages(msg_handler, FALSE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_COMM_HOST_NOT_FOUND));
		    return COMM_LOADING;
        }
#ifdef NEED_URL_NETWORK_ACTIVE_CHECK
        else if(!user_initiated
# ifdef NEED_URL_ACTIVE_CONNECTIONS
				&& !urlManager->ActiveConnections()
# endif
				&& !NetworkActive())
        {
			SendMessages(msg_handler, TRUE, MSG_URL_LOADING_FAILED, ERR_SSL_ERROR_HANDLED);
		    return COMM_LOADING;
        }
#endif // URL_NEED_NETWORK_ACTIVE_CHECK
	}

#ifdef DEBUG_LOAD_STATUS
	if(!g_main_message_handler->HasCallBack(this,MSG_LOAD_DEBUG_STATUS,0))
		g_main_message_handler->SetCallBack(this, MSG_LOAD_DEBUG_STATUS,0);
#endif

									__DO_START_TIMING(TIMING_COMM_LOAD_DS);
	BOOL has_mh = mh_list->HasMsgHandler(msg_handler, FALSE, FALSE, FALSE);

	if (!has_mh)
	{
		if(OpStatus::IsError(AddMessageHandler(msg_handler)))
		{
			SendMessages(msg_handler, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			return COMM_LOADING;
		}
	}
	else
		if (status == URL_LOADING)
			return COMM_LOADING;
#ifdef _DEBUG_RED
	if(!inline_loading)
	{
		PrintfTofile("redir1.txt","%s [%p] Load\n", url->Name(),msg_handler);
	}

	if((URLStatus) GetAttribute(URL::KLoadStatus) != URL_LOADING && (URLStatus) url->GetAttribute(URL::KLoadStatus, TRUE) == URL_LOADING)
	{
		int test = 1;
	}
#endif

#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	if(status != URL_LOADING && GetAttribute(URL::KIsOutOfCoverageFile))
	{
		// remove Out of coverage file
		DataStream_Conditional_Sensitive_ByteArray(cache_destruct, GetAttribute(URL::KCachePolicy_NoStore));
		OP_DELETE(old_storage);
		OP_DELETE(storage);
		old_storage = NULL;
		storage = NULL;
		SetAttribute(URL::KIsOutOfCoverageFile, FALSE);

		// resetting resume and cache validation flags
		SetAttribute(URL::KReloadSameTarget, FALSE);
		if(http_data)
			http_data->flags.only_if_modified = FALSE;
	}
#endif

#ifdef URL_ENABLE_HTTP_RANGE_SPEC
	if(http_data && (http_data->sendinfo.range_start!=FILE_LENGTH_NONE || http_data->sendinfo.range_end!=FILE_LENGTH_NONE))
	{
		SetAttribute(URL::KReloadSameTarget, TRUE);
	}
#endif

	if (status == URL_LOADING)
	{
		if (!has_mh)
		{
			if(type != URL_HTTPS)
			{
				int security_status = (type == URL_OPERA ? SECURITY_STATE_FULL : SECURITY_STATE_NONE);

				if(OpStatus::IsError(SetSecurityStatus(security_status)))
				{
					SendMessages(msg_handler, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
					return COMM_LOADING;
				}
				Window *win = msg_handler->GetWindow();
				if(win)
					win->SetSecurityState(security_status, FALSE, NULL);
			}
			if (GetAttribute(URL::KHeaderLoaded) &&
				(!http_data || http_data->flags.header_loaded_sent))
			{
				msg_handler->PostMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1);
				msg_handler->PostMessage(MSG_URL_DATA_LOADED, url->GetID(), 0);
			}

			if (!has_mh)
			{
				OpFileLength loaded_size = 0;
				GetAttribute(URL::KContentLoaded, &loaded_size);

				if (loaded_size > 0)
				{
					Window *win = msg_handler->GetWindow();
					if(win)
						win->HandleDataProgress((long) loaded_size, FALSE, NULL, NULL, NULL);
				}
			}
		}

									__DO_STOP_TIMING(TIMING_COMM_LOAD_DS);

		return COMM_LOADING;
	}
	else if(!GetAttribute(URL::KIsResuming))
	{
		if(user_initiated || !thirdparty_determined)
		{
			SetAttribute(URL::KDisableProcessCookies,FALSE);
			SetAttribute(URL::KIsThirdParty,FALSE);
			SetAttribute(URL::KIsThirdPartyReach,FALSE);
		}
		if(http_data && (!storage || (http_data->flags.only_if_modified && !storage->DataPresent())))
			http_data->flags.only_if_modified = FALSE;

		if(!GetAttribute(URL::KReloadSameTarget))
		{
			DataStream_Conditional_Sensitive_ByteArray(cache_destruct, GetAttribute(URL::KCachePolicy_NoStore));
			OP_DELETE(old_storage);
			old_storage = storage;
			storage = NULL;
		}
	}

	internal_error_message.Empty();

	if(OpStatus::IsError(msg_handler->SetCallBack(this, MSG_REFRESH_PROGRESS, 0)))
	{
		SendMessages(msg_handler, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		return COMM_LOADING;
	}

	TRAPD(op_err, g_url_api->CheckCookiesReadL());
	// Third party URL?
#if defined _ASK_COOKIE
	ServerName *server = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
	COOKIE_MODES cmode_site = (server ? server->GetAcceptCookies(TRUE) : COOKIE_DEFAULT);
#elif defined PUBSUFFIX_ENABLED
	COOKIE_MODES cmode_site = COOKIE_DEFAULT;
#endif // _ASK_COOKIE or PUBSUFFIX_ENABLED
	COOKIE_MODES cmode_global = (COOKIE_MODES) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled);
	COOKIE_MODES cmode;

#if defined PUBSUFFIX_ENABLED
	BOOL wait_for_data = FALSE;
#endif
#if defined _ASK_COOKIE || defined PUBSUFFIX_ENABLED
	cmode = cmode_site;

	if(cmode == COOKIE_DEFAULT)
#endif
		cmode = cmode_global;

	info.was_inline = inline_item;

	if (!info.was_inline)
	{
		DocumentManager *docman = msg_handler->GetDocumentManager();
		if (docman && docman->GetFrame())
			info.was_inline = TRUE;
	}

	if(cmode != COOKIE_NONE &&
		((!user_initiated && !thirdparty_determined)) &&
		info.was_inline &&
		((URLType) url->GetAttribute(URL::KType) == URL_HTTP || (URLType) url->GetAttribute(URL::KType) == URL_HTTPS))
	{
		if(!DetermineThirdParty(referer_url))
		{
#if defined PUBSUFFIX_ENABLED
			g_main_message_handler->SetCallBack(this, MSG_PUBSUF_FINISHED_AUTO_UPDATE_ACTION, 0);
			SetAttribute(URL::KReferrerURL, referer_url);
			wait_for_data = TRUE;
#endif
		}
	}

	if(url->GetAttribute(URL::KSpecialRedirectRestriction))
		SetAttribute(URL::KDisableProcessCookies,TRUE);

	if(!user_initiated)
	{
		if(info.use_nettype == NETTYPE_UNDETERMINED)
		{
			// Set minimum level accessible
			info.use_nettype = referer_url.GetAttribute(URL::KLoadedFromNetType);
			if (info.use_nettype == NETTYPE_UNDETERMINED)
			{
				ServerName *ref_sn = referer_url.GetServerName();
				if(ref_sn)
					info.use_nettype = ref_sn->GetNetType();
			}
		}
	}

	SetAttribute(URL::KHeaderLoaded,FALSE);
	SetAttribute(URL::KLoadStatus,URL_LOADING);
	if(loading)
		DeleteLoading();
	info.determined_proxy = FALSE;
	info.use_proxy = FALSE;
	if(http_data)
	{
		http_data->sendinfo.proxyname.Empty();
		http_data->flags.checking_redirect = 0;
	}

#if defined PUBSUFFIX_ENABLED
	if(wait_for_data)
		return COMM_LOADING;
#endif

#if defined(WEB_TURBO_MODE) || defined(URL_PER_SITE_PROXY_POLICY)
	Window* origin_window = msg_handler->GetWindow();
#endif // WEB_TURBO_MODE || URL_PER_SITE_PROXY_POLICY

#ifdef WEB_TURBO_MODE
	// Make sure all new main documents are loaded in the correct mode
	BOOL should_use_turbo = origin_window && origin_window->GetTurboMode();
	if ((type == URL_HTTP || type == URL_HTTPS) &&
		!!url->GetAttribute(URL::KUsesTurbo) != should_use_turbo &&
		url->GetAttribute(URL::KHTTP_ContentUsageIndication) == HTTP_UsageIndication_MainDocument)
	{
		URL_CONTEXT_ID ctx = should_use_turbo ? g_windowManager->GetTurboModeContextId() : 0;

		return InternalRedirect(url->GetAttribute(URL::KName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI, URL::KNoRedirect),
								!!url->GetAttribute(URL::KUnique),
								ctx);
	}
#endif // WEB_TURBO_MODE

#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	if (type == URL_HTTP)
	{
		ServerName *current_server = (ServerName*)url->GetAttribute(URL::KServerName, NULL);
		if (current_server && current_server->SupportsStrictTransportSecurity())
		{
			const char *path = url->GetAttribute(URL::KName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI, URL::KNoRedirect);

			OpString8 https_path;
			if (path && op_strlen(path) > 7)
			{
				https_path.AppendFormat("https://%s", path + 7);

				return InternalRedirect(https_path.CStr(),
									!!url->GetAttribute(URL::KUnique),
									url->GetContextId());
			}
		}
	}
#endif // LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY

#ifdef URL_PER_SITE_PROXY_POLICY
	// Make sure this (inline and plugin) url share the same proxy policy with main document
    // if PrefsCollectionNetwork::EnableProxy isn't overriden for this host
	if (origin_window && (type == URL_HTTP || type == URL_HTTPS)
        && !g_pcnet->IsPreferenceOverridden(PrefsCollectionNetwork::EnableProxy,url->GetAttribute(URL::KUniHostName)))
	{
		info.use_proxy = origin_window->GetCurrentLoadingURL().GetAttribute(URL::KUseProxy);
	}
#endif

	return Load_Stage2(msg_handler, referer_url);
}

#ifdef GOGI_URL_FILTER
OP_STATUS URL_DataStorage::CheckURLAllowed(URL_Rep *url, BOOL& wait)
{
	OpMessage msg = MSG_CHECK_URL_ALLOWED;
	OP_STATUS retval = g_urlfilter->CheckURLAsync(URL(url,(char *)NULL), msg, wait);

	if (OpStatus::IsSuccess(retval) && wait) {
		g_main_message_handler->SetCallBack(this, msg, url->GetID());
	}
	return retval;
}
#endif // GOGI_URL_FILTER

CommState URL_DataStorage::InternalRedirect(const char *new_url, BOOL unique, URL_CONTEXT_ID new_ctx)
{
	URL dummy;
	URL redirect_url = urlManager->GetURL(dummy, /* Empty parent */
										  new_url,
										  unique,
										  new_ctx /* Use new context */);
	UINT32 old_response = GetAttribute(URL::KHTTP_Response_Code);
	TRAPD(op_err,
		SetAttributeL(URL::KMovedToURL, redirect_url);
		SetAttributeL(URL::KHTTP_Response_Code, HTTP_TEMPORARY_REDIRECT));

	if(OpStatus::IsSuccess(op_err))
	{
		if(OpStatus::IsError(ExecuteRedirect_Stage2(TRUE)))
			HandleFailed(ERR_COMM_INTERNAL_ERROR);
		else if((URLStatus) url->GetAttribute(URL::KLoadStatus) == URL_LOADING)
			HandleFinished();
	}
	SetAttribute(URL::KHTTP_Response_Code, old_response);
	return COMM_LOADING;
}

CommState URL_DataStorage::AskAboutURLWithUserName(const URL& referer_url)
{
	OP_MEMORY_VAR CommState status = COMM_REQUEST_FAILED;
	TRAPD(op_err, status = AskAboutURLWithUserNameL(referer_url));

	if(OpStatus::IsError(op_err) || status != COMM_LOADING)
	{
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, (OpStatus::IsError(op_err) ? URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR ): ERR_SSL_ERROR_HANDLED));
		return COMM_LOADING;
	}

	return status;
}

CommState URL_DataStorage::AskAboutURLWithUserNameL(const URL& referer_url)
{
	OpString tmp_str; ANCHOR(OpString, tmp_str);
	OpString username; ANCHOR(OpString, username);
	OpString server; ANCHOR(OpString, server);

	LEAVE_IF_ERROR(url->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, 0, tmp_str));
	username.SetL(url->GetAttribute(URL::KUserName));
	url->GetAttributeL(URL::KUniHostName,server);

	UriUnescape::ReplaceChars(username.CStr(), UriUnescape::LocalfileUtf8);

	OpStackAutoPtr<URL_Dialogs> current_dialog_base(OP_NEW_L(URL_Dialogs, (MSG_ASKED_USERNAME_PERMISSION)));

	if(!current_dialog_base->Configure(mh_list))
		LEAVE(OpStatus::ERR);

	current_dialog_base->GetListener()->OnAskAboutUrlWithUserName(current_dialog_base->GetWindowCommander(),
		tmp_str.CStr(),
		server.CStr(),
		username.CStr(),
		current_dialog_base.get());

	current_dialog = current_dialog_base.release();

	LEAVE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_ASKED_USERNAME_PERMISSION, current_dialog->Id()));

	if(((URLType) url->GetAttribute(URL::KType) == URL_HTTP || (URLType) url->GetAttribute(URL::KType) == URL_HTTPS))
	{
		SetAttributeL(URL::KReferrerURL, referer_url);
	}

	return COMM_LOADING;
}

CommState URL_DataStorage::Load_Stage2(MessageHandler* msg_handler, const URL& referer_url)
{

#ifdef GOGI_URL_FILTER
	if (!url->GetAttribute(URL::KAllowed)) {
		BOOL wait;
		OP_STATUS r = CheckURLAllowed(url, wait);
		if (OpStatus::IsError(r)) {
			SetAttribute(URL::KLoadStatus, URL_LOADING_ABORTED);
			SendMessages(msg_handler, FALSE, MSG_URL_LOADING_FAILED, ERR_HTTP_NO_CONTENT);
			return COMM_REQUEST_FAILED;
		}
		if (wait) {
			if(((URLType) url->GetAttribute(URL::KType) == URL_HTTP ||
				(URLType) url->GetAttribute(URL::KType) == URL_HTTPS)) {
				SetAttributeL(URL::KReferrerURL, referer_url);
			}
			return COMM_LOADING;
		}
		else
			OpStatus::Ignore(url->SetAttribute(URL::KAllowed, 1));
	}
#endif // GOGI_URL_FILTER

	URLType type = (URLType) url->GetAttribute(URL::KType);

	if (type == URL_HTTP || type == URL_HTTPS || type == URL_FTP || type == URL_FILE)
	{
		const char * OP_MEMORY_VAR name = url->GetAttribute(URL::KUserName).CStr();
		if (name == NULL && url->GetAttribute(URL::KPassWord).HasContent())
			name = "";

		if (name != NULL && !((ServerName *) url->GetAttribute(URL::KServerName, NULL))->GetPassUserNameURLsAutomatically(name))
		{
#ifdef URL_DISABLE_INTERACTION
			if(url->GetAttribute(URL::KBlockUserInteraction))
			{
				SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_ILLEGAL_URL));
				OnLoadFinished(URL_LOAD_FAILED, msg_handler);
				return COMM_LOADING;
			}
#endif

			return AskAboutURLWithUserName(referer_url);
		}
	}
#ifdef ADD_PERMUTE_NAME_PARTS
	if((type == URL_HTTP || type == URL_HTTPS)

		&& g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckPermutedHostNames)
		)
	{
		ServerName *server = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
		const char *name = server->Name();

	BOOL addressSet=(server->SocketAddress() && server->SocketAddress()->IsValid());

	if(!addressSet && name && op_strchr(name,'.') == NULL &&
#ifdef SUPPORT_LITERAL_IPV6_URL
		*name != '[' &&
#endif //SUPPORT_LITERAL_IPV6_URL
	   UriUnescape::stricmp(name, "localhost", UriUnescape::All) != 0 &&
	   op_strspn(name,"0123456789") != op_strlen(name) &&
	   GetAttribute(URL::KHTTP_Method) != HTTP_METHOD_HEAD &&
	   !url->GetAttribute(URL::KUnique)
	   )
		return StartNameCompletion(referer_url);
	}
#endif // ADD_PERMUTE_NAME_PARTS

	return StartLoading(msg_handler, referer_url);
}


CommState URL_DataStorage::StartLoading(MessageHandler* msg_handler, const URL& referer_url)
{
	if(msg_handler == NULL)
	{
									__DO_STOP_TIMING(TIMING_COMM_LOAD_DS);
		return COMM_REQUEST_FAILED;
	}

	OnLoadStarted(msg_handler);

	URLType type = (URLType) url->GetAttribute(URL::KType);

#ifdef URL_NETWORK_LISTENER_SUPPORT

#ifdef OBSERVE_GADGET_NETWORK_EVENTS
	MessageHandler* tmp_msg_handler = mh_list->FirstMsgHandler();
	Window *window = tmp_msg_handler ? tmp_msg_handler->GetWindow() : NULL;
	if (((type == URL_HTTP) ||
		(type == URL_HTTPS)) &&
		window && window->GetGadget())
		urlManager->NetworkEvent(URL_GADGET_LOADING_STARTED, url->GetID());
#else
	urlManager->NetworkEvent(URL_LOADING_STARTED, url->GetID());
#endif // OBSERVE_GADGET_NETWORK_EVENTS
#endif

#ifdef URL_BLOCK_FOR_MULTIPART
	if((type == URL_HTTP || type == URL_HTTPS) &&
		((ServerName *) url->GetAttribute(URL::KServerName, NULL))->GetBlockedByMultipartCount(url->GetAttribute(URL::KResolvedPort)))
	{
		if(!GetAttribute(URL::KWaitingForBlockingMultipart))
		{
			g_main_message_handler->SetCallBack(this, MSG_URL_MULTIPART_UNBLOCKED,0);
			SetAttribute(URL::KWaitingForBlockingMultipart, TRUE);
		}
		return COMM_LOADING;
	}
#endif

#ifdef _EMBEDDED_BYTEMOBILE
	if (type == URL_HTTP && (GetAttribute(URL::KHTTP_Method) == HTTP_METHOD_POST || GetAttribute(URL::KHTTP_Method) == HTTP_METHOD_PUT || GetAttribute(URL::KHTTP_Method) == HTTP_METHOD_PUT || GetAttribute(URL::KHTTP_Method) == HTTP_METHOD_CONNECT || GetAttribute(URL::KHTTP_Method) == HTTP_METHOD_HEAD))
	{
		SetLoadDirect(TRUE);
	}
#endif

	if(	(type == URL_HTTP) ||
		(type == URL_HTTPS) ||
#ifdef GOPHER_SUPPORT
		(type == URL_Gopher) ||
#endif
#ifdef WAIS_SUPPORT
		(type == URL_WAIS) ||
#endif
		(type == URL_FTP)
		)
	{
		if(!info.determined_proxy)
		{
			CommState stat = DetermineProxy(msg_handler,referer_url);
			if(stat != COMM_REQUEST_FINISHED)
			{
				__DO_STOP_TIMING(TIMING_COMM_LOAD_DS);
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
				if(loading)
					goto start_loading;
#endif
				return stat;
			}
		}
	}

	switch(info.use_proxy ? URL_HTTP : type)
		{
		case URL_HTTP:
		case URL_HTTPS:
#ifdef GOPHER_SUPPORT
		case URL_Gopher:
#endif
#ifdef WAIS_SUPPORT
		case URL_WAIS:
#endif
			{
				if(!GetHTTPProtocolData())
					break;

				TRAPD(op_err, SetAttributeL(URL::KReferrerURL, referer_url));
				OpStatus::Ignore(op_err);

#ifdef __THUNDERBALL__
                if(!loading && prefsManager->GetIntegerPrefDirect(PrefsManager::UseHTTPAbstraction))
				{
					TRAPD(err, loading = URL_Thunderball_LoadHandler::NewL(url, g_main_message_handler));

				}
#endif
				if(!loading)
				{
					URL_HTTP_LoadHandler *http_loading = OP_NEW(URL_HTTP_LoadHandler, (url, g_main_message_handler));

#ifdef WEBSERVER_SUPPORT
				if (http_loading && url->GetAttribute(URL::KIsUniteServiceAdminURL))
				{
					http_loading->SetUniteConnection();
				}
#endif // WEBSERVER_SUPPORT
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
					if(http_loading)
					{
						if (GetLoadDirect())
							http_loading->SetLoadDirect();
#ifdef _EMBEDDED_BYTEMOBILE
						if (GetPredicted())
						{
							http_loading->SetPredicted(predicted_depth);
							SetPredicted(FALSE, predicted_depth);
						}
#endif // _EMBEDDED_BYTEMOBILE
					}
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
					loading = http_loading;
				}

				if( !loading )
				{
					g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
#ifdef DEBUG_LOAD_STATUS
				    g_main_message_handler->UnsetCallBack(this, MSG_LOAD_DEBUG_STATUS);
#endif
					return COMM_REQUEST_FAILED;
				}

                break;
			}
#ifdef FTP_SUPPORT
		case URL_FTP:
			{
				loading = OP_NEW(URL_FTP_LoadHandler, (url, g_main_message_handler));//FIXME:OOM
				break;
			}
#endif // FTP_SUPPORT
#ifdef _MIME_SUPPORT_
		case URL_EMAIL:
        case URL_NEWS:
        case URL_SNEWS:
#if defined(M2_SUPPORT)
			if(g_m2_engine)
			{
				URL mailcommand_url(url, (char*)NULL);
				g_m2_engine->MailCommand(mailcommand_url);
			}
#endif // M2_SUPPORT
			storage = old_storage;
			old_storage = NULL;
			BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);

			SetAttribute(URL::KLoadStatus,URL_LOADING);
			SetAttribute(URL::KReloading,FALSE);
			SetAttribute(URL::KIsResuming, FALSE);
			SetAttribute(URL::KReloadSameTarget,FALSE);
			return COMM_LOADING;
			break;
#endif
#ifdef _LOCALHOST_SUPPORT_
		case URL_FILE:
			{
				TRAPD(op_err, SetAttributeL(URL::KReferrerURL, referer_url));
				StartLoadingFile(TRUE);
				break;
			}
#endif // _LOCALHOST_SUPPORT_
#ifdef DOM_GADGET_FILE_API_SUPPORT
		case URL_MOUNTPOINT:
			{
				TRAPD(op_err, SetAttributeL(URL::KReferrerURL, referer_url));
				StartLoadingMountPoint(TRUE);
				break;
			}
#endif // WEBSERVER_SUPPORT

#if defined(GADGET_SUPPORT) && defined(_LOCALHOST_SUPPORT_)
		case URL_WIDGET:
			{
				TRAPD(op_err, SetAttributeL(URL::KReferrerURL, referer_url));
				StartLoadingWidget(msg_handler, TRUE);
				break;
			}
#endif /* GADGET_SUPPORT && _LOCALHOST_SUPPORT_ */

#if defined(OPERA_URL_SUPPORT) || defined(HAS_OPERABLANK)
		case URL_OPERA:
			StartLoadingOperaSpecialDocument(msg_handler);
			if(loading)
				break; // We have a loadhandler
#ifdef DEBUG_LOAD_STATUS
			g_main_message_handler->UnsetCallBack(this, MSG_LOAD_DEBUG_STATUS);
#endif
			return COMM_LOADING;
#endif // OPERA_URL_SUPPORT || HAS_OPERABLANK

#ifdef HAS_ATVEF_SUPPORT
		case URL_TV:
			StartLoadingTvDocument();
			break;
#endif /* HAS_ATVEF_SUPPORT */

#ifdef SUPPORT_DATA_URL
		case URL_DATA:
			StartLoadingDataURL(referer_url);
			return COMM_LOADING;
#endif

		default:
#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
			if (urlManager->IsAnExternalProtocolHandler(type))
			{
				loading = OP_NEW(ExternalProtocolLoadHandler, (url, g_main_message_handler, type));//FIXME:OOM
				if (!loading)
				{
					g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
#ifdef DEBUG_LOAD_STATUS
					g_main_message_handler->UnsetCallBack(this, MSG_LOAD_DEBUG_STATUS);
#endif
					return COMM_REQUEST_FAILED;
				}
				break;
			}
#endif // EXTERNAL_PROTOCOL_SCHEME_SUPPORT
			SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_ILLEGAL_URL));
			break;


		}


#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
start_loading:; // for automatic proxy configuration
#endif
	if(type != URL_HTTPS)
	{
		int security_status = url->GetAttribute(URL::KSecurityStatus, TRUE);
		if (type == URL_OPERA)
			security_status = SECURITY_STATE_FULL;

		OpStatus::Ignore(SetSecurityStatus(security_status));
		if (msg_handler->GetWindow())
			msg_handler->GetWindow()->SetSecurityState(security_status, FALSE, NULL);
	}
#ifdef DEBUG_LOAD_STATUS
	started_tick =(unsigned long) g_op_time_info->GetWallClockMS();
	OpString8 tempname;
	url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, tempname);
	PrintfTofile("loading.txt","     URL request Id %d : Starting tick=%lu: %s\n",
		(loading ? loading->Id() : 0), started_tick, tempname.CStr());
	// print time stamp
#endif

#ifdef HTTP_BENCHMARK
	BenchMarkLoading();
#endif

#ifdef URL_PER_SITE_PROXY_POLICY
	if (loading)
		loading->SetForceSOCKSProxy(info.use_proxy);
#endif

	CommState l;
	if ((!storage || GetAttribute(URL::KReloadSameTarget)) &&
		(!loading || ((l = loading->Load()) != COMM_LOADING && l != COMM_REQUEST_FINISHED && l != COMM_REQUEST_WAITING && l != COMM_WAITING)))
	{
		BOOL url_has_application_cache_fallback = HandleFailed(URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
									__DO_STOP_TIMING(TIMING_COMM_LOAD_DS);
#ifdef DEBUG_LOAD_STATUS
		g_main_message_handler->UnsetCallBack(this, MSG_LOAD_DEBUG_STATUS);
#endif

		if (url_has_application_cache_fallback)
			return COMM_LOADING;

		return COMM_REQUEST_FAILED;
	}

	if(loading)
	{
		g_main_message_handler->UnsetCallBack(this, MSG_COMM_LOADING_FINISHED);
		g_main_message_handler->UnsetCallBack(this, MSG_COMM_LOADING_FAILED);
		g_main_message_handler->UnsetCallBack(this, MSG_COMM_DATA_READY);

		RAISE_AND_RETURN_VALUE_IF_ERROR(g_main_message_handler->SetCallBackList(this, loading->Id(), ds_loading_messages, ARRAY_SIZE(ds_loading_messages)), COMM_REQUEST_FAILED);

		if(!GetAttribute(URL::KTimeoutStartsAtRequestSend) && !GetAttribute(URL::KTimeoutStartsAtConnectionStart))
		{
			StartTimeout();
		}
	}
#ifdef DEBUG_LOAD_STATUS
    else
    {
	    g_main_message_handler->UnsetCallBack(this, MSG_LOAD_DEBUG_STATUS);
    }
#endif

							__DO_STOP_TIMING(TIMING_COMM_LOAD_DS);
	return COMM_LOADING;
}

CommState URL_DataStorage::DetermineProxy(MessageHandler* msg_handler, const URL& referer_url)
{
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	// Check with autoproxy script (unless this *is* the autoproxy script)
	if(url->GetAttribute(URL::KIsPACFile))
	{
		info.determined_proxy = TRUE;
		return COMM_REQUEST_FINISHED;
	}

	if(url->GetAttribute(URL::KBypassProxy))
	{
		info.determined_proxy = TRUE;
		return COMM_REQUEST_FINISHED;
	}

#ifdef AUTOPROXY_PER_CONTEXT
	URL_CONTEXT_ID id = url->GetContextId();
	extern BOOL UseAutoProxyForContext(URL_CONTEXT_ID);
#endif

	if (
#ifdef AUTOPROXY_PER_CONTEXT
		UseAutoProxyForContext(id) &&
#endif
		g_pcnet->IsAutomaticProxyOn()
//		&& prefsManager->GetIntegerPrefDirect(PrefsManager::EcmaScriptEnabled)
		)
	{
		OpStatus::Ignore(SetAttribute(URL::KReferrerURL, referer_url));

		loading = CreateAutoProxyLoadHandler(url, g_main_message_handler); // FIXME: OOM
		if(loading != NULL)
		{
			static const OpMessage messages[] =
			{
				MSG_COMM_PROXY_DETERMINED,
				MSG_COMM_LOADING_FAILED
			};

			g_main_message_handler->SetCallBackList(this, loading->Id(), messages, ARRAY_SIZE(messages));
			return COMM_LOADING;
		}
	}
#endif

	//reset httpdata proxyname if this is called for a second time.
	if (http_data)
		http_data->sendinfo.proxyname.Empty();

	// Check with preferences
	URLType type = (URLType) url->GetAttribute(URL::KType);
	(void)type; // silence "unused" warning
	ServerName *sn = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
	if(sn == NULL)
		return COMM_REQUEST_FAILED;

#ifdef EXTERNAL_PROXY_DETERMINATION
	// Deprecated. Use TWEAK_URL_EXTERNAL_PROXY_DETERMINATION_BY_URL instead
	DEPRECATED(extern const uni_char* GetExternalProxy(URL_CONTEXT_ID id, const uni_char* host));
	const uni_char* proxy = GetExternalProxy(url->GetContextId(), url->GetAttribute(URL::KUniHostName));
#elif defined EXTERNAL_PROXY_DETERMINATION_BY_URL
	extern const uni_char* GetExternalProxy(URL &url);

	URL this_url(url, (char *) NULL);
	const uni_char* proxy = GetExternalProxy(this_url);
#else // EXTERNAL_PROXY_DETERMINATION_BY_URL
	const uni_char *proxy = urlManager->GetProxy(sn, type);
#endif // EXTERNAL_PROXY_DETERMINATION_BY_URL

#ifdef _EMBEDDED_BYTEMOBILE
	// Should this be inside the #else of #ifdef EXTERNAL_PROXY_DETERMINATION above?
	if (type == URL_HTTP && urlManager->GetEmbeddedBmOpt() && !GetLoadDirect())
	{
		proxy = urlManager->GetEBOServer().CStr();
	}
	if (type == URL_HTTPS && urlManager->GetEmbeddedBmOpt() && !GetLoadDirect())
		SetLoadDirect(TRUE);
#endif // _EMBEDDED_BYTEMOBILE
#ifdef WEB_TURBO_MODE
	BOOL use_turbo = !!url->GetAttribute(URL::KUsesTurbo);

	if (use_turbo && !GetLoadDirect())
	{
		const uni_char* hostname = sn->UniName();
		if( !sn->IsHostResolved() && hostname && *hostname )
		{
			UINT32 host_len = uni_strlen(hostname);

			if( uni_strspn(hostname,UNI_L(".0123456789")) == static_cast<size_t>(host_len)
# ifdef SUPPORT_LITERAL_IPV6_URL
			|| hostname[0] == '['
# endif //SUPPORT_LITERAL_IPV6_URL
			  )
			{
				OP_STATUS op_err = OpStatus::OK;
# ifdef SUPPORT_LITERAL_IPV6_URL
				if( hostname[0] == '[' && hostname[host_len-1] == ']')
				{
					OpString host;
					if( OpStatus::IsSuccess(op_err = host.Set(hostname+1, host_len-1)) )
					{
						op_err = sn->SetHostAddressFromString(host.CStr());
					}
				}
				else
# endif //SUPPORT_LITERAL_IPV6_URL
				{
					op_err = sn->SetHostAddressFromString(hostname);
				}
				if(OpStatus::IsError(op_err))
				{
					if(op_err == OpStatus::ERR_NO_MEMORY)
						g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
					return COMM_REQUEST_FAILED;
				}
			}
		}

		// HTTPS and local traffic does not use Turbo Proxy
		if( !GetLoadDirect() && (type == URL_HTTPS || sn->GetNetType() < NETTYPE_PUBLIC
#  ifdef WEBSERVER_SUPPORT
			|| url->GetAttribute(URL::KIsLocalUniteService)
#  endif // WEBSERVER_SUPPORT
			) )
		{
			SetLoadDirect(TRUE);
		}
	}

	BOOL proxy_is_turbo_proxy = FALSE;
	if( use_turbo && urlManager->GetWebTurboAvailable() && !GetLoadDirect() )
	{
		if( !proxy && type == URL_HTTP )
		{
			proxy = g_obml_config->GetTurboProxyName();
			proxy_is_turbo_proxy = TRUE;
		}
	}
#endif // WEB_TURBO_MODE

	if( (proxy
#  ifdef URL_PER_SITE_PROXY_POLICY
		|| urlManager->GetProxy(sn, URL_SOCKS)
#  endif
		) &&
#  ifdef WEBSERVER_SUPPORT
		!url->GetAttribute(URL::KIsLocalUniteService) &&
#  endif // WEBSERVER_SUPPORT
		(info.use_proxy || urlManager->UseProxyOnServer(sn, (unsigned short) url->GetAttribute(URL::KResolvedPort))) &&
		GetHTTPProtocolData())
	{
		OP_STATUS op_err = http_data->sendinfo.proxyname.Set(proxy);
		if(OpStatus::IsError(op_err))
		{
			if(op_err == OpStatus::ERR_NO_MEMORY)
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return COMM_REQUEST_FAILED;
		}
		info.use_proxy = TRUE;
#ifdef WEB_TURBO_MODE
		if( use_turbo && !proxy_is_turbo_proxy
			&& type == URL_HTTP	&& !GetLoadDirect()
#ifdef URL_SIGNAL_PROTOCOL_TUNNEL
			&& !url->GetAttribute(URL::KUsedAsTunnel)
#endif // URL_SIGNAL_PROTOCOL_TUNNEL
			&& urlManager->GetWebTurboAvailable() )
		{
			http_data->sendinfo.use_proxy_passthrough = TRUE;
		}
		else
		{
			http_data->sendinfo.use_proxy_passthrough = FALSE;
		}
#endif // WEB_TURBO_MODE
	}
	else
		info.use_proxy = FALSE;

	info.determined_proxy = TRUE;
	return COMM_REQUEST_FINISHED;
}

/**
 * Returns a data descriptor from which the URL data can be read.
 * @param get_raw_data Turns off character conversion. Used by applets,
 * which do their own conversion.
 */

URL_DataDescriptor *URL_DataStorage::GetDescriptor(MessageHandler *mh, BOOL get_raw_data, BOOL get_decoded_data, Window *window,
												   URLContentType override_content_type, unsigned short override_charset_id, BOOL get_original_content, unsigned short parent_charset_id)
{
	return (storage ? storage->GetDescriptor(mh, get_raw_data, get_decoded_data, window, override_content_type, override_charset_id, get_original_content, parent_charset_id) : NULL);
}

/*
void URL_DataStorage::UpdateDescriptor(URL_DataDescriptor *desc)
{
	//OP_ASSERT(storage);

	if(storage)
	{
		storage->AddDescriptor(desc);
	}
}
*/

void URL_DataStorage::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
									__DO_START_TIMING(TIMING_COMM_LOAD_DS1);


	switch(msg)
	{
		case MSG_REFRESH_PROGRESS:
			if(loading)
				loading->RefreshProgressInformation();
			break;
		case MSG_URL_TIMEOUT:
			if(par2 == 0 || !GetAttribute(URL::KTimeoutSeenDataSinceLastPoll_INTERNAL))
			{
#ifdef SELFTEST
				g_languageManager->GetString(ConvertUrlStatusToLocaleString(par2== 0 ? URL_ERRSTR(SI, ERR_URL_TIMEOUT) : URL_ERRSTR(SI, ERR_URL_IDLE_TIMEOUT)) , internal_error_message);
#endif
				HandleFailed(par2== 0 ? URL_ERRSTR(SI, ERR_URL_TIMEOUT) : URL_ERRSTR(SI, ERR_URL_IDLE_TIMEOUT));
				break;
			}
			OP_ASSERT(GetAttribute(URL::KTimeoutPollIdle) != 0);
			SetAttribute(URL::KTimeoutSeenDataSinceLastPoll_INTERNAL, FALSE);
			g_main_message_handler->PostDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 1, GetAttribute(URL::KTimeoutPollIdle)*1000);
			break;
		case MSG_COMM_DATA_READY:
			ReceiveData();

			if(GetAttribute(URL::KHeaderLoaded) && (!http_data || http_data->flags.header_loaded_sent))
			{
				BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
			}
			break;

		case MSG_COMM_LOADING_FINISHED:
			HandleFinished();
			break;

		case MSG_COMM_LOADING_FAILED:
			HandleFailed(par2);
			break;
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		case MSG_COMM_PROXY_DETERMINED:
				info.determined_proxy = TRUE;
                info.use_proxy = (http_data->sendinfo.proxyname.FindI("PROXY") != KNotFound);
				if(!info.use_proxy && GetAttribute(URL::KSendOnlyIfProxy))
				{
					HandleFailed(MSG_NOT_USING_PROXY);
					break;
				}
#endif
		case MSG_NAME_COMPLETED:
			{
				if(loading)
				{
					g_main_message_handler->RemoveCallBacks(this, loading->Id());
					DeleteLoading();
				}
				URL referrer = GetAttribute(URL::KReferrerURL);
				if(
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
					(msg != MSG_COMM_PROXY_DETERMINED || par2 == 0) &&
#endif
						(HTTP_Method) GetAttribute(URL::KHTTP_Method) == HTTP_METHOD_GET
					)
				{
					// The IP address could have changed, so we have to
					// reset the proxy information.
					info.determined_proxy = FALSE;
					info.use_proxy = FALSE;
					if(http_data)
						http_data->sendinfo.proxyname.Empty();
				}
				StartLoading(mh_list->FirstMsgHandler(), referrer);
			}
			break;

#ifdef URL_BLOCK_FOR_MULTIPART
		case MSG_URL_MULTIPART_UNBLOCKED:
			{
				OP_ASSERT(GetAttribute(URL::KWaitingForBlockingMultipart));
				SetAttributeL(URL::KWaitingForBlockingMultipart, FALSE);
				g_main_message_handler->UnsetCallBack(this, MSG_URL_MULTIPART_UNBLOCKED);
				URL referer = (http_data ? http_data->sendinfo.referer_url : URL());
				StartLoading(mh_list->FirstMsgHandler(), referer);
			}
			break;
#endif
#ifdef GOGI_URL_FILTER
		case MSG_CHECK_URL_ALLOWED:
			{
				g_main_message_handler->UnsetCallBack(this, MSG_CHECK_URL_ALLOWED);
				if(par2)
				{
					OpStatus::Ignore(url->SetAttribute(URL::KAllowed, 1));
					Load_Stage2(GetFirstMessageHandler(), GetAttribute(URL::KReferrerURL));
				}
				else
				{
					HandleFailed(ERR_HTTP_NO_CONTENT);
				}
			}
			break;
#endif // GOGI_URL_FILTER
		case MSG_ASKED_USERNAME_PERMISSION:
			{
				if(current_dialog)
				{
					OP_DELETE(current_dialog);
					current_dialog = NULL;
				}
				g_main_message_handler->UnsetCallBack(this, MSG_ASKED_USERNAME_PERMISSION);
				if(par2 != IDOK && par2 != IDYES)	// allow MB_OK and MB_YESNO positive messages
				{
					HandleFailed(ERR_SSL_ERROR_HANDLED);
					break;
				}
				((ServerName *) url->GetAttribute(URL::KServerName, NULL))->SetPassUserNameURLsAutomatically(url->GetAttribute(URL::KUserName));//FIXME:OOM - OP_STATUS

				Load_Stage2(GetFirstMessageHandler(), GetAttribute(URL::KReferrerURL));
			}
			break;

		case MSG_HANDLED_POST_REDIRECT_QUESTION:
			{
				g_main_message_handler->UnsetCallBack(this, MSG_HANDLED_POST_REDIRECT_QUESTION);
				if(current_dialog)
				{
					OP_DELETE(current_dialog);
					current_dialog = NULL;
				}
				if(!http_data || http_data->flags.checking_redirect == 0)
					return;

				int res = par2;

				if(res == IDNO)
					http_data->flags.checking_redirect = 2;
				else if(res == IDCANCEL)
				{
					OpStatus::Ignore(SetAttribute(URL::KHTTP_MovedToURL_Name, NULL)); // reset, cannot fail
					http_data->flags.checking_redirect = 0;

					if((URLStatus) url->GetAttribute(URL::KLoadStatus) != URL_LOADING)
						HandleFinished();

					return;
				}
				else
					http_data->flags.checking_redirect = 3;

				if(OpStatus::IsError(ExecuteRedirect_Stage2()))
					HandleFailed(URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				else if((URLStatus) url->GetAttribute(URL::KLoadStatus) != URL_LOADING)
					HandleFinished();
				break;
			}
#if defined PUBSUFFIX_ENABLED
		case MSG_PUBSUF_FINISHED_AUTO_UPDATE_ACTION:
			if(!info.header_loaded && !DetermineThirdParty(GetAttribute(URL::KReferrerURL))) // Only for LoadStage2
				return;

			g_main_message_handler->UnsetCallBack(this, MSG_PUBSUF_FINISHED_AUTO_UPDATE_ACTION,0);
			if(!info.header_loaded)
				Load_Stage2(GetFirstMessageHandler(), GetAttribute(URL::KReferrerURL));
			else
				ExecuteRedirect_Stage2();
			break;
#endif
#ifdef DEBUG_LOAD_STATUS
		case MSG_LOAD_DEBUG_STATUS:
			{
				OpString8 tempname;
				url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, tempname);
				PrintfTofile("loading.txt","     URL request Id %d : %s : %s\n",
					(loading ? loading->Id() : 0),
					(url->GetAttribute(URL::KLoadStatus) == URL_LOADING ? "LOADING" : "NOT LOADING"),
					tempname.CStr());
				break;
			}
#endif
#ifdef _DEBUG
			/*
		default:
			{
				int debug = 1;
			}
			*/
#endif
	}
									__DO_STOP_TIMING(TIMING_COMM_LOAD_DS1);

}

void URL_DataStorage::HandleFinished()
{

#ifdef DEBUG_LOAD_STATUS
	{
		OpString8 tempname;
		url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, tempname);
		PrintfTofile("loading.txt","     URL request Id %d : Finished tick=%lu/%lu: %s\n",
			(loading ? loading->Id() : 0), (unsigned long) g_op_time_info->GetWallClockMS(),
			(unsigned long) g_op_time_info->GetWallClockMS()-started_tick, tempname.CStr());
		started_tick = 0;
		g_main_message_handler->UnsetCallBack(this, MSG_LOAD_DEBUG_STATUS);
	}
#endif

#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
	SetAttribute(URL::KUsedForFileDownload, FALSE);
#endif

	if(loading)
	{
		g_main_message_handler->RemoveCallBacks(this, loading->Id());
		DeleteLoading();
	}

	g_main_message_handler->UnsetCallBack(this, MSG_URL_TIMEOUT);
	g_main_message_handler->RemoveDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 0);
	g_main_message_handler->RemoveDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 1);

	//FreeLoadBuf();
#ifdef _DEBUG_DS1
	{
		OpString8 tempname;
		url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, tempname);
		PrintfTofile("urlds1.txt","\nDS HandleFinished- %s\n", tempname.CStr());
		PrintfTofile("urldd1.txt","\nDS HandleFinished- %s\n", tempname.CStr());
	}
#endif
	info.was_inline = TRUE;
	info.proxy_no_cache = FALSE;
	if(!storage || !storage->GetIsMultipart())
		SetAttribute(URL::KLoadStatus, URL_LOADED);
	SetAttribute(URL::KIsResuming,FALSE);
	SetAttribute(URL::KReloadSameTarget,FALSE);
	url->SetAttribute(URL::KOverrideRedirectDisabled, FALSE);
	SetAttribute(URL::KSendOnlyIfProxy, FALSE);
	info.use_nettype = NETTYPE_UNDETERMINED;

	OP_ASSERT(!info.cache_size_added);

	if(storage)
		storage->SetFinished();
	else if(!info.cache_size_added)
	{
		// Manually add the Object size estimation to the RAM cache, as there is no cache storage
		Context_Manager *ctx = urlManager->FindContextManager(url->GetContextId());

		if(ctx)
		{
			ctx->AddToRamCacheSize(0, url, TRUE);
			info.cache_size_added = TRUE;
		}
	}
	if(old_storage)
	{
		DataStream_Conditional_Sensitive_ByteArray(cache_destruct, GetAttribute(URL::KCachePolicy_NoStore));
		OP_DELETE(old_storage);
		old_storage = NULL;
	}
	//OP_ASSERT(!storage || (storage->GetFinished() && storage->Cache_Storage::GetFinished()));

#ifdef _DEBUG_DS
	{
		OpString8 temp_debug_name;
		url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, temp_debug_name);
		PrintfTofile("urlds.txt","\nReceived Finished: %s\n",temp_debug_name.CStr());
		PrintfTofile("redir.txt","\nDS Received Finished: %s\n",temp_debug_name.CStr());
	}
#endif

	if (http_data)
	{
		http_data->flags.redirect_blocked = FALSE;
	}

	URLType type = (URLType) url->GetAttribute(URL::KType);
	if ((type == URL_HTTP || type == URL_HTTPS) &&
		http_data/* && not_auth*/)
	{
		http_data->flags.only_if_modified = FALSE;
		http_data->flags.proxy_no_cache = FALSE;

#ifdef _DEBUG_DS
		{
			OpString8 temp_debug_name;
			url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, temp_debug_name);

			PrintfTofile("redir.txt","Moved-to of %s (reponse %d) is\n",temp_debug_name.CStr(), http_data->recvinfo.response);

			http_data->recvinfo.moved_to_url.GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, temp_debug_name);
			PrintfTofile("redir.txt","            %s (%s) \n",http_data->recvinfo.moved_to_url_name.CStr(), (http_data->recvinfo.moved_to_url.IsValid() ? temp_debug_name.CStr() : ""));
		}
#endif
		uint32 response = GetAttribute(URL::KHTTP_Response_Code);

		if(http_data->flags.connect_auth)
		{
			// A non-200 response to a CONNECT request is treated as unsecure
			SetSecurityStatus(SECURITY_STATE_NONE, NULL);
			URL this_url(url, (char *) NULL);
			GetMessageHandlerList()->SetProgressInformation(SET_SECURITYLEVEL,SECURITY_STATE_NONE, NULL, &this_url);
			// documents served from the CONNECT response must not execute scripts
			url->SetAttribute(URL::KSuppressScriptAndEmbeds, MIME_Remote_Script_Disable);
		}
		if (
		    g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableClientPull, static_cast<const ServerName *>(url->GetAttribute(URL::KServerName, NULL))) &&
			!url->GetAttribute(URL::KSpecialRedirectRestriction) &&
			GetAttribute(URL::KMovedToURL).IsValid() &&
			(IsRedirectResponse(response) ||
			http_data->flags.checking_redirect == 1))
		{
			return;
		}
	}

	if(http_data)
		http_data->flags.checking_redirect = 0;

//loading_is_finished:;
	uint32 response = GetAttribute(URL::KHTTP_Response_Code);
	if(!GetAttribute(URL::KHeaderLoaded) ||
		(URLContentType) GetAttribute(URL::KContentType) == URL_UNDETERMINED_CONTENT
#ifdef _MIME_SUPPORT_
		|| (URLContentType) GetAttribute(URL::KContentType) == URL_MIME_CONTENT
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
		|| (URLContentType) GetAttribute(URL::KContentType) == URL_MHTML_ARCHIVE
#endif
#endif
		|| (response== HTTP_UNAUTHORIZED && !url->GetAttribute(URL::KSpecialRedirectRestriction)) ||
			response== HTTP_PROXY_UNAUTHORIZED
			)
	{
		if (!http_data || !(response == HTTP_NO_CONTENT ||
			(IsRedirectResponse(response) &&
			((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableClientPull, static_cast<const ServerName *>(url->GetAttribute(URL::KServerName, NULL))) ||
				url->GetAttribute(URL::KOverrideRedirectDisabled))
				&& !url->GetAttribute(URL::KSpecialRedirectRestriction))) || // If redirect, but redirect is disabled (and no override is set), continue as if this was an ordinary page
			(response == HTTP_UNAUTHORIZED && url->GetAttribute(URL::KSpecialRedirectRestriction)) ))
		{
			ContentDetector content_detector(url, TRUE, FALSE);
			if(OpStatus::IsError(content_detector.DetectContentType()))
			{
				SetAttribute(URL::KSpecialRedirectRestriction, FALSE);
				SetAttribute(URL::KLoadStatus,URL_LOADING_FAILURE); // Yngve, er denne  ok?
				BroadcastMessage(MSG_URL_LOADING_FAILED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
				OnLoadFinished(URL_LOAD_FAILED);
				return;
			}
		}

#ifdef _MIME_SUPPORT_
		/*
		if(GetContentType() == URL_MIME_CONTENT)
		{
			DecodeMimeData();
		}
		else
		*/
#endif
		{
			BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
			SetAttribute(URL::KHeaderLoaded,TRUE);
		}
	}

	SetAttribute(URL::KSpecialRedirectRestriction, FALSE);

	if(storage && storage->GetIsMultipart())
		return;

	range_offset = 0;

	// the loading status of url object has changed so make sure client code will get new MSG_URL_DATA_LOADED message (CORE-42411)
	for (MsgHndlList::Iterator it = mh_list->Begin(); it != mh_list->End(); ++it)
	{
		MessageHandler *mh = (*it)->GetMessageHandler();
		for (URL_DataDescriptor *dd = storage ? storage->First() : NULL; dd; dd = dd->Suc())
		{
			if (dd->GetMessageHandler() == mh)
			{
				mh->RemoveDelayedMessage(MSG_URL_DATA_LOADED, url->GetID(), 0);
				dd->ClearPostedMessage();
				break;
			}
		}
	}
	BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);

#ifdef _DEBUG_DS1
	OpString8 temp_debug_name;
	url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, temp_debug_name);
	PrintfTofile("urldd1.txt","\nDS HandleFinished 2- %s\n",temp_debug_name.CStr());
#endif
	{
		MessageHandler* mh = mh_list->FirstMsgHandler();

		while(mh != NULL)
		{
			mh_list->Remove(mh);
			mh->UnsetCallBacks(this);
			mh = mh_list->FirstMsgHandler();
		}
	}
#ifdef DEBUG_LOAD_STATUS
	{
		OpString8 temp_debug_name;
		url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, temp_debug_name);
		PrintfTofile("loading.txt","     URL request Id %d : Finished #1 tick=%lu/%lu: %s\n",
			(loading ? loading->Id() : 0), (unsigned long) g_op_time_info->GetWallClockMS(),
			(unsigned long) g_op_time_info->GetWallClockMS()-started_tick, temp_debug_name.CStr());
		started_tick = 0;
		g_main_message_handler->UnsetCallBack(this, MSG_LOAD_DEBUG_STATUS);
	}
#endif

	OnLoadFinished(URL_LOAD_FINISHED);
}

OP_STATUS URL_DataStorage::ExecuteRedirect()
{
	if (!http_data)
		return OpStatus::OK;

	URL this_url(url, (char *) NULL);
	URL_InUse url_blocker(this_url);

	http_data->flags.redirect_blocked = FALSE;

	URLType url_type = (URLType) url->GetAttribute(URL::KType);
	if (url_type != URL_HTTP && url_type != URL_HTTPS)
		return OpStatus::OK;

	uint32 response = GetAttribute(URL::KHTTP_Response_Code);
	OpStringC8 location(GetAttribute(URL::KHTTP_Location));

	if( location.IsEmpty() ||
		!IsRedirectResponse(response) ||
		http_data->sendinfo.redirect_count>=20)
	{
		OpStatus::Ignore(SetAttribute(URL::KHTTP_MovedToURL_Name, NULL)); // reset, cannot fail
		BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
		url->Access(FALSE);
		http_data->flags.header_loaded_sent = TRUE;
		return OpStatus::OK;
	}

#ifdef _DEBUG_DS
	{
		OpString8 temp_name;
		url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, temp_name);
		PrintfTofile("redir.txt","Moved-to of %s (reponse %d) is\n",temp_name.CStr(), response);
		GetAttribute(URL::KHTTP_MovedToURL_Name, temp_name);
		PrintfTofile("redir.txt","            %s\n",temp_name.CStr());
	}
#endif

#ifdef _DEBUG_DS
	{
		OpString8 temp_name;
		url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI,temp_name);
		PrintfTofile("redir.txt","Preparing to Redirect %s to\n",temp_name.CStr());
		PrintfTofile("redir.txt","            %s \n",location.CStr());
	}
#endif

	OpStatus::Ignore(SetAttribute(URL::KHTTP_MovedToURL_Name, NULL));

	char *OP_MEMORY_VAR tmp = (char *) location.CStr();
	while (*tmp != '\0' && *tmp != '#')
		tmp++;

	if (*tmp == '#')
		*tmp = '\0';
	else
		tmp = 0;

	OpStringC8 href_rel(tmp ? tmp +1 : NULL);

	BOOL unique_url = FALSE;
	HTTP_Method method = (HTTP_Method) GetAttribute(URL::KHTTP_Method);

	if( !GetMethodIsSafe(method) &&
		(response == HTTP_MOVED || response == HTTP_TEMPORARY_REDIRECT))
		unique_url = TRUE;


	URL moved_to_url;
	{
		// Escape the location
		int escapes = 0;
		char *this_name = (char *) location.CStr();
		OpString8 escaped_name;

		if (this_name &&
			op_strnicmp(this_name, "javascript:", 11) != 0 &&
			op_strnicmp(this_name, "news:", 5) != 0 &&
			op_strnicmp(this_name, "newspost:", 9) != 0 &&
			op_strnicmp(this_name, "snews:", 6) != 0 &&
			op_strnicmp(this_name, "snewspost:", 10) != 0
			) // don't escape a javascript url !!!
				escapes = UriEscape::CountEscapes(this_name, UriEscape::StandardUnsafe);

		if (escapes)
		{
			if(escaped_name.Reserve(op_strlen(this_name) + escapes*2+1) == NULL)
			{
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				HandleFailed(URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				return OpStatus::ERR_NO_MEMORY;
			}
			UriEscape::Escape(escaped_name.DataPtr(), this_name, UriEscape::StandardUnsafe);
			this_name = escaped_name.CStr();
		}

		URL tmp_url(url, (char *) NULL);
		URL tmp_moved(urlManager->GetURL(tmp_url,this_name, href_rel,unique_url));

		OP_STATUS op_err;
		URL next_url = tmp_moved;
		next_url = next_url.GetAttribute(URL::KMovedToURL);
		if(next_url.IsValid())
		{
			urlManager->MakeUnique(tmp_moved.GetRep());
			URL tmp_moved2(urlManager->GetURL(tmp_url,this_name, href_rel,unique_url));
			TRAP(op_err, SetAttributeL(URL::KMovedToURL, tmp_moved2));
		}
		else
			TRAP(op_err, SetAttributeL(URL::KMovedToURL, tmp_moved));
		moved_to_url = GetAttribute(URL::KMovedToURL);
		if(OpStatus::IsError(op_err) || moved_to_url.IsEmpty())
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			HandleFailed(URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	if (tmp)
		*tmp = '#';
	tmp = NULL;

#ifdef URL_DISABLE_INTERACTION
	if(GetAttribute(URL::KBlockUserInteraction))
		http_data->recvinfo.moved_to_url.SetAttribute(URL::KBlockUserInteraction, TRUE);
#endif

#ifdef ABSTRACT_CERTIFICATES
	if( GetAttribute(URL::KCertificateRequested) ||
		http_data->recvinfo.moved_to_url.GetAttribute(URL::KType) == URL_HTTPS)
	{
		// this might prove uncessary as DocumentManager::OpenURL will be called by Window::OpenURL anyway
		http_data->recvinfo.moved_to_url.SetAttributeL(URL::KCertificateRequested,TRUE);
	}
#endif

	BOOL permitted = TRUE;

#ifdef URL_FILTER
	if (url->GetAttribute(URL::KSkipContentBlocker))
		RETURN_IF_MEMORY_ERROR(moved_to_url.SetAttribute(URL::KSkipContentBlocker, TRUE));
	else
	{
		MessageHandler* msg_handler = mh_list->FirstMsgHandler();
		Window *window = msg_handler ? msg_handler->GetWindow() : NULL;
		OpString temp_url;
		BOOL allowed = FALSE;
		OP_STATUS status;
		ServerName * sn =  window ? window->GetCurrentShownURL().GetServerName() : NULL;
		BOOL widget = window && window->GetType() == WIN_TYPE_GADGET;
		HTMLLoadContext ctx(RESOURCE_UNKNOWN, sn, NULL, widget);

		if (OpStatus::IsSuccess(status = moved_to_url.GetAttribute(URL::KUniName_Escaped, 0, temp_url)) &&
			OpStatus::IsSuccess(status = g_urlfilter->CheckURL(temp_url.CStr(), allowed, window, &ctx)) &&
			!allowed)
		{
			// Blocked urls are always marked as failed, so a top level
			// error page is rendered if applicable.
			url->SetAttribute(URL::KLoadStatus, URL_LOADING_FAILURE);
			BroadcastMessage(MSG_URL_LOADING_FAILED, url->GetID(), URL_ERRSTR(SI, ERR_COMM_BLOCKED_URL), MH_LIST_ALL);

			return OpStatus::OK;
		}
		else
			RETURN_IF_MEMORY_ERROR(status);
	}
#endif // URL_FILTER
	if(GetAttribute(URL::KSpecialRedirectRestriction))
	{
		// protocol and servername must match
		permitted = (URLType) moved_to_url.GetAttribute(URL::KType) == (URLType) url->GetAttribute(URL::KType) &&
						((ServerName *) moved_to_url.GetAttribute(URL::KServerName, (const void*) 0)) == ((ServerName *) url->GetAttribute(URL::KServerName, (const void*) 0));

		unsigned short port1 = (unsigned short) moved_to_url.GetAttribute(URL::KServerPort);
		unsigned short port2 = (unsigned short) url->GetAttribute(URL::KServerPort);

		// ports must match too
		if(permitted && port1 != port2 )
		{
			unsigned short def_port = 0;
			switch((URLType) url->GetAttribute(URL::KType))
			{
			case URL_HTTP:
				def_port = 80;
				break;
			case URL_HTTPS:
				def_port = 443;
				break;
			case URL_FTP:
				def_port = 21;
				break;
			}

			if((port1 != 0 && port2 != 0) ||
				(port1 == 0 && port2 != def_port) ||
				(port2 == 0 && port1 != def_port))
				permitted = FALSE;
		}
	}
	else if((URLType)moved_to_url.GetAttribute(URL::KType) == URL_FILE)
		permitted = FALSE;

#ifdef WEBSERVER_SUPPORT
	if (moved_to_url.GetAttribute(URL::KIsUniteServiceAdminURL) && !url->GetAttribute(URL::KIsUniteServiceAdminURL))
		permitted = FALSE;
#endif // WEBSERVER_SUPPORT


	if(!permitted)
	{
		// This redirect is not permitted, remove redirect URL and act as if it is a normal document.
		OpStatus::Ignore(SetAttribute(URL::KHTTP_MovedToURL_Name, NULL)); // reset, cannot fail
		moved_to_url.Unload();

		BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
		url->Access(FALSE);
		http_data->flags.header_loaded_sent = TRUE;

		return OpStatus::OK;
	}

	return ExecuteRedirect_Stage2();
}

OP_STATUS URL_DataStorage::ExecuteRedirect_Stage2(BOOL mode_switch)
{
	URL moved_to_url = GetAttribute(URL::KMovedToURL);
	if(moved_to_url.IsEmpty())
		return OpStatus::ERR;

	// Disallow all redirects over the following schemes:
	//  - javascript: (CORE-31866)
	//  - opera:, about: (CORE-47452)
	//  - data: (CORE-48941)
	URLType moved_to_type = (URLType) moved_to_url.GetAttribute(URL::KType);
	if (moved_to_type == URL_JAVASCRIPT ||
	    moved_to_type == URL_OPERA ||
	    moved_to_type == URL_ABOUT ||
	    moved_to_type == URL_DATA)
	{
		TRAPD(op_err, SetAttributeL(URL::KMovedToURL, URL()));
		SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_REDIRECT_FAILED));
		return op_err;
	}

#ifdef CORS_SUPPORT
	if (g_secman_instance && loading)
		if (GetAttribute(URL::KFollowCORSRedirectRules))
		{
			URL this_url(url, (char *) NULL);
			OP_BOOLEAN result = g_secman_instance->VerifyRedirect(this_url, moved_to_url);
			RETURN_IF_ERROR(result);
			if (result == OpBoolean::IS_FALSE)
			{
				TRAPD(op_err, SetAttributeL(URL::KMovedToURL, URL()));
				SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_REDIRECT_FAILED));
				return op_err;
			}
		}
#endif // CORS_SUPPORT

#ifdef WEB_TURBO_MODE
	OpStatus::Ignore(moved_to_url.SetAttribute(URL::KTurboBypassed, url->GetAttribute(URL::KTurboBypassed)));
#endif
	OpStatus::Ignore(moved_to_url.SetAttribute(URL::KTimeoutMaximum, GetAttribute(URL::KTimeoutMaximum)));
	OpStatus::Ignore(moved_to_url.SetAttribute(URL::KTimeoutPollIdle, GetAttribute(URL::KTimeoutPollIdle)));
	OpStatus::Ignore(moved_to_url.SetAttribute(URL::KTimeoutMinimumBeforeIdleCheck, GetAttribute(URL::KTimeoutMinimumBeforeIdleCheck)));
	OpStatus::Ignore(moved_to_url.SetAttribute(URL::KTimeoutStartsAtRequestSend, GetAttribute(URL::KTimeoutStartsAtRequestSend)));

	OpStatus::Ignore(moved_to_url.SetAttribute(URL::KHTTP_Priority, GetAttribute(URL::KHTTP_Priority)));

#ifdef URL_ENABLE_HTTP_RANGE_SPEC
	OpFileLength range_start = 0;
	OpFileLength range_end = 0;
	url->GetAttribute(URL::KHTTPRangeStart, &range_start);
	url->GetAttribute(URL::KHTTPRangeEnd, &range_end);

	OpStatus::Ignore(moved_to_url.SetAttribute(URL::KHTTPRangeEnd, &range_end));
	OpStatus::Ignore(moved_to_url.SetAttribute(URL::KHTTPRangeStart, &range_start));
#endif // URL_ENABLE_HTTP_RANGE_SPEC

	OpStatus::Ignore(moved_to_url.SetAttribute(URL::KMultimedia, url->GetAttribute(URL::KMultimedia)));

#ifdef HTTP_CONTENT_USAGE_INDICATION
	OpStatus::Ignore(moved_to_url.SetAttribute(URL::KHTTP_ContentUsageIndication, GetAttribute(URL::KHTTP_ContentUsageIndication)));
#endif // HTTP_CONTENT_USAGE_INDICATION

	MsgHndlList* old_mh_list = mh_list;
	mh_list = OP_NEW(MsgHndlList, ());//FIXME:OOM
	if(mh_list == NULL)
	{
		mh_list = old_mh_list; //??
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);//
		return OpStatus::ERR_NO_MEMORY;
	}

	OpStatus::Ignore(moved_to_url.SetAttribute(URL::KSendAcceptEncoding, GetAttribute(URL::KSendAcceptEncoding)));

	HTTP_Method method = (HTTP_Method) GetAttribute(URL::KHTTP_Method);
	uint32 response = GetAttribute(URL::KHTTP_Response_Code);

#ifdef URL_FILTER
#ifdef FILTER_REDIRECT_RULES
	BOOL ret = FALSE;
	if (g_urlfilter)
	{
		OpString url_name;
		OP_STATUS op_err = url->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, 0, url_name);
		if(OpStatus::IsError(op_err))
		{
			OpStatus::Ignore(SetAttribute(URL::KHTTP_MovedToURL_Name, NULL)); // reset, cannot fail

			delete mh_list;
			mh_list = old_mh_list;

			moved_to_url.Unload();

			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return op_err;
		}

		OpStatus::Ignore(g_urlfilter->CheckRedirectURL((uni_char *)url_name.CStr(), ret));
	}
	if(ret)
	{
#ifdef URL_NETWORK_LISTENER_SUPPORT
		urlManager->NetworkEvent(URL_LOADING_REDIRECT_FILTERED, url->GetID());
#endif
		OpStatus::Ignore(SetAttribute(URL::KHTTP_MovedToURL_Name, NULL)); // reset, cannot fail
		moved_to_url.Unload();
		//BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
		//url->Access(FALSE);
		//http_data->flags.header_loaded_sent = TRUE;
		return OpStatus::OK;
	}
#endif // FILTER_REDIRECT_RULES
#endif // URL_FILTER

	if( (moved_to_type == URL_HTTP || moved_to_type== URL_HTTPS) &&
		GetMethodHasRequestBody(method) &&
		(/*response == HTTP_MOVED ||*/ response == HTTP_TEMPORARY_REDIRECT))
	{
		BOOL use_data = TRUE;

#ifdef WEB_TURBO_MODE
		if(mode_switch)
			http_data->flags.checking_redirect = 3;
#endif // WEB_TURBO_MODE

		if(http_data->flags.checking_redirect == 0)
		{
			OpString this_name;
			OpString movedto_name;
			OP_STATUS op_err=OpStatus::OK;

#ifdef URL_DISABLE_INTERACTION
			if(url->GetAttribute(URL::KBlockUserInteraction))
			{
				op_err = OpStatus::ERR;
			}
#endif

			if(OpStatus::IsSuccess(op_err))
				op_err = url->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, 0, this_name, URL::KNoRedirect);
			if(OpStatus::IsSuccess(op_err))
				op_err = moved_to_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, 0, movedto_name, URL::KNoRedirect);


			if(OpStatus::IsSuccess(op_err))
				do {
					OpAutoPtr<URL_Dialogs> temp_current_dialog(OP_NEW(URL_Dialogs, (MSG_HANDLED_POST_REDIRECT_QUESTION)));
					if(!temp_current_dialog.get())
					{
						op_err = OpStatus::ERR_NO_MEMORY;
						break;
					}

					if(!temp_current_dialog->Configure(old_mh_list))
					{
						op_err = OpStatus::ERR;
						break;
					}

					temp_current_dialog->GetListener()->OnAskAboutFormRedirect(temp_current_dialog->GetWindowCommander(),this_name.CStr(), movedto_name.CStr(), temp_current_dialog.get());

					op_err = g_main_message_handler->SetCallBack(this, MSG_HANDLED_POST_REDIRECT_QUESTION, temp_current_dialog->Id());
					current_dialog = temp_current_dialog.release();
				}while(0);

			if(OpStatus::IsError(op_err))
			{
				OpStatus::Ignore(SetAttribute(URL::KHTTP_MovedToURL_Name, NULL)); // reset, cannot fail
				moved_to_url.Unload();
			}
			else
				http_data->flags.checking_redirect = 1;


			OP_DELETE(mh_list);
			mh_list = old_mh_list;
			return OpStatus::OK;

		}
		else if(http_data->flags.checking_redirect == 1)
			return OpStatus::OK;


		use_data = (http_data->flags.checking_redirect == 3);
		http_data->flags.checking_redirect = 0;

		// Move Postdata
		if(use_data)
		{
			OP_STATUS op_err;
			op_err = moved_to_url.SetAttribute(URL::KHTTP_ContentType, GetAttribute(URL::KHTTP_ContentType));
			if(OpStatus::IsSuccess(op_err))
				op_err = moved_to_url.SetHTTP_Data(http_data->sendinfo.http_data.CStr(),TRUE);
			if(OpStatus::IsSuccess(op_err) && method == HTTP_METHOD_String)
			{
				OpString8 method_string;
				op_err = GetAttribute(URL::KHTTPSpecialMethodStr, method_string);
				if(OpStatus::IsSuccess(op_err))
					op_err = moved_to_url.SetAttribute(URL::KHTTPSpecialMethodStr,method_string);
			}
			if(OpStatus::IsError(op_err))
			{
				OpStatus::Ignore(SetAttribute(URL::KHTTP_MovedToURL_Name, NULL)); // reset, cannot fail

				OP_DELETE(mh_list);
				mh_list = old_mh_list;

				moved_to_url.Unload();

				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return op_err;
			}
#ifdef HAS_SET_HTTP_DATA
			moved_to_url.SetHTTP_Data(http_data->sendinfo.upload_data,TRUE);
			http_data->sendinfo.upload_data = NULL;
#endif
			OpStatus::Ignore(moved_to_url.SetAttribute(URL::KHTTP_Method, GetAttribute(URL::KHTTP_Method)));
		}

		OpStatus::Ignore(moved_to_url.SetAttribute(URL::KHavePassword, url->GetAttribute(URL::KHavePassword)));
	}
	else if(response == HTTP_TEMPORARY_REDIRECT)
	{
		OpStatus::Ignore(moved_to_url.SetAttribute(URL::KHTTP_Method, GetAttribute(URL::KHTTP_Method)));
		if(method == HTTP_METHOD_String)
		{
			OpString8 method_string;

			if(OpStatus::IsError(GetAttribute(URL::KHTTPSpecialMethodStr, method_string)) ||
				OpStatus::IsError(moved_to_url.SetAttribute(URL::KHTTPSpecialMethodStr,method_string)))
			{
				OpStatus::Ignore(SetAttribute(URL::KHTTP_MovedToURL_Name, NULL)); // reset, cannot fail

				OP_DELETE(mh_list);
				mh_list = old_mh_list;

				moved_to_url.Unload();

				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	// check that url.http->moved_to_url does not points to this URL
	URL next_url = moved_to_url;
	while(next_url.IsValid())
	{
		if (next_url.GetRep() == url)
			break;
		next_url = next_url.GetAttribute(URL::KMovedToURL);
	}
	if(next_url.IsValid())
	{
		// avoid a circular list of moved_to_url
		moved_to_url.SetAttribute(URL::KHTTP_MovedToURL_Name, NULL);
	}

	if(moved_to_url.GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) == URL_LOADING)
	{
		if(next_url.IsValid())
		{
			// Stop loading of the redirected URL if we have a circular list
			// does not affect this Url, message handlers are in safekeping
			moved_to_url.StopLoading(NULL);
		}
		else
		{
			// Skip to the last URL in the chain
			URL last_moved_to_url  = moved_to_url.GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);
			if (last_moved_to_url.IsValid())
				moved_to_url = last_moved_to_url;
		}
	}

#ifdef DEBUG_URL_COMM
	FILE *fptemp = fopen("c:\\klient\\url.txt","a");
	fprintf(fptemp,"[%d] URL: %s - Redirection: load \"%s\"\n", id, name, url.http->moved_to_url.Name());
	fclose(fptemp);
#endif
	old_mh_list->BroadcastMessage(MSG_URL_MOVED, url->GetID(), moved_to_url.Id(/*FALSE*/), MH_LIST_ALL);

	URLType mtype = (URLType) moved_to_url.GetAttribute(URL::KType);

	if (mtype == URL_TELNET || mtype == URL_MAILTO
#if defined(IMODE_EXTENSIONS)
        || mtype == URL_TEL
#endif
		|| mtype == URL_UNKNOWN || mtype > URL_NULL_TYPE)
	{
		OP_DELETE(mh_list);
		mh_list = old_mh_list;

		MessageHandler* msg_handler = mh_list->FirstMsgHandler();
		Window *win = msg_handler ? msg_handler->GetWindow() : NULL;

		if(win)
			win->EndProgressDisplay();
        URL ref_url =  GetAttribute(URL::KReferrerURL);
#ifdef NEED_URL_HANDLE_UNRECOGNIZED_SCHEME
		if( mtype == URL_UNKNOWN )
		{
			TRAPD(op_err,HandleUnrecognisedURLTypeL(moved_to_url.GetAttribute(URL::KUniName).CStr()));
			return op_err;
		}
		else
#endif

			return (win ? win->OpenURL(moved_to_url, DocumentReferrer(ref_url),TRUE,FALSE,-1, FALSE) : OpStatus::OK);
	}



	ServerName *document_servername = NULL;
	{
		MessageHandler* mh = old_mh_list->FirstMsgHandler();
		Window *window = mh ? mh->GetWindow() : NULL;
		DocumentManager *docman = window ? window->DocManager() : NULL;
		URL document_url;
		if (docman)
		{
			document_url = docman->GetCurrentURL();
			document_servername = (ServerName *)document_url.GetAttribute(URL::KServerName, NULL);
		}
	}

	if(url->GetAttribute(URL::KOverrideRedirectDisabled) && next_url.IsEmpty() &&
		(URLStatus) moved_to_url.GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) == URL_LOADED)
	{
		if(!moved_to_url.Expired(FALSE, FALSE))
		{
			old_mh_list->BroadcastMessage(MSG_HEADER_LOADED, moved_to_url.Id(), (moved_to_url.GetRep()->GetIsFollowed() ? 0 : 1), MH_LIST_ALL);
			old_mh_list->BroadcastMessage(MSG_URL_DATA_LOADED, moved_to_url.Id(), 0, MH_LIST_ALL);
			OP_DELETE(mh_list);
			mh_list = old_mh_list;

			return OpStatus::OK;
		}
	}
	moved_to_url.SetAttribute(URL::KOverrideRedirectDisabled, url->GetAttribute(URL::KOverrideRedirectDisabled));

	ServerName *this_servername = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
#ifdef _ASK_COOKIE
	COOKIE_MODES cmode_site = (this_servername ? this_servername->GetAcceptCookies(TRUE) : COOKIE_DEFAULT);
#endif
	COOKIE_MODES cmode_global = (COOKIE_MODES) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled);
	COOKIE_MODES cmode;

#ifdef _ASK_COOKIE
	cmode = cmode_site;

	if(cmode == COOKIE_DEFAULT)
#endif
		cmode = cmode_global;

	if(cmode != COOKIE_NONE &&
		(cmode != COOKIE_SEND_NOT_ACCEPT_3P && document_servername != (ServerName *) moved_to_url.GetAttribute(URL::KServerName, NULL)) &&
		((URLType) url->GetAttribute(URL::KType) == URL_HTTP || (URLType) url->GetAttribute(URL::KType) == URL_HTTPS))
	{
        URL temp(url, (char *) NULL);

		moved_to_url.SetAttribute(URL::KDisableProcessCookies,FALSE);
		moved_to_url.SetAttribute(URL::KIsThirdParty, FALSE);
		if(!moved_to_url.DetermineThirdParty(temp))
		{
#if defined PUBSUFFIX_ENABLED
			g_main_message_handler->SetCallBack(this, MSG_PUBSUF_FINISHED_AUTO_UPDATE_ACTION, 0);
			return COMM_LOADING;
#endif
		}
	}
	{
		// find referer
		URL ref_url;
		if (SEND_REFERER )
			ref_url = GetAttribute(URL::KReferrerURL);

#ifdef _DEBUG_DS
		{
			OpString8 temp_debug_name;
			url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, temp_debug_name);

			PrintfTofile("redir.txt","Redirecting %s to\n",temp_debug_name.CStr());

			moved_to_url.GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, temp_debug_name);
			PrintfTofile("redir.txt","            %s \n",temp_debug_name.CStr());
		}
#endif

		if (loading)
			OpStatus::Ignore(loading->CopyExternalHeadersToUrl(moved_to_url));

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
        moved_to_url.SetAttribute(URL::KIsPACFile, url->GetAttribute(URL::KIsPACFile));
#endif
		if(mtype == URL_HTTP || mtype == URL_HTTPS)
			moved_to_url.SetAttribute(URL::KCachePolicy_AlwaysCheckNeverExpiredQueryURLs, GetAttribute(URL::KCachePolicy_AlwaysCheckNeverExpiredQueryURLs));

		OpSocketAddressNetType current_nettype = this_servername ? this_servername->GetNetType() : NETTYPE_UNDETERMINED;

		if(current_nettype != NETTYPE_UNDETERMINED &&
			((OpSocketAddressNetType) info.use_nettype == NETTYPE_UNDETERMINED ||
			(OpSocketAddressNetType) info.use_nettype < current_nettype))
			moved_to_url.SetAttribute(URL::KLimitNetType, current_nettype); // If this server is in a more public net, use that as a network restriction

		BOOL succeded = FALSE;
		int next_redir_count = http_data->sendinfo.redirect_count + 1;
		MsgHndlList::Iterator itr = old_mh_list->Begin();
		url->OnLoadRedirect(moved_to_url.GetRep());
		for (; itr != old_mh_list->End(); ++itr)
		{
			CommState stat;
#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
			moved_to_url.SetAttribute(URL::K_INTERNAL_Override_Reload_Protection, TRUE);
#endif
			if ((*itr)->GetMessageHandler() && ((moved_to_url.GetAttribute(URL::KLoadStatus) == URL_LOADED &&
				http_data->flags.only_if_modified)))
				stat = moved_to_url.Reload((*itr)->GetMessageHandler(), ref_url, TRUE, info.proxy_no_cache, info.was_inline, FALSE, FALSE,FALSE,TRUE);
			else
				stat = moved_to_url.Load((*itr)->GetMessageHandler(), ref_url, info.was_inline, FALSE, FALSE, FALSE);

			if(stat == COMM_LOADING || stat == COMM_WAITING)
				succeded = TRUE;
#ifdef _DEBUG_DS
			PrintfTofile("redir.txt","     done\n");
#endif

			if((*itr)->GetMessageHandler())
				(*itr)->GetMessageHandler()->UnsetCallBacks(this);

			if((*itr)->GetMessageHandler() == g_main_message_handler && loading)
				RAISE_AND_RETURN_VALUE_IF_ERROR(g_main_message_handler->SetCallBackList(this, loading->Id(), ds_loading_messages, ARRAY_SIZE(ds_loading_messages)), COMM_REQUEST_FAILED);
		}

		if(!succeded)
		{
			old_mh_list->BroadcastMessage(MSG_URL_LOADING_FAILED,moved_to_url.Id(/*FALSE*/),URL_ERRSTR(SI, ERR_REDIRECT_FAILED),MH_LIST_ALL);
			//old_mh_list->BroadcastMessage(MSG_URL_INLINE_LOADING, moved_to_url.Id(/*FALSE*/), MAKELONG(moved_to_url.GetAttribute(URL::KLoadStatus), 0), MH_LIST_ALL);
		}
		OP_DELETE(old_mh_list);

		//set redirect count
		moved_to_url.SetAttribute(URL::KRedirectCount, next_redir_count);
	}

	return OpStatus::OK;
}

BOOL URL_DataStorage::HandleFailed(MH_PARAM_2 par2)
{
#ifdef DEBUG_LOAD_STATUS
	{
		OpString8 tempname;
		url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, tempname);
		PrintfTofile("loading.txt","     URL request Id %d : Failed tick=%lu/%lu: %s\n",
			(loading ? loading->Id() : 0), (unsigned long) g_op_time_info->GetWallClockMS(),
			(unsigned long) g_op_time_info->GetWallClockMS()-started_tick, tempname.CStr());
		started_tick = 0;
		g_main_message_handler->UnsetCallBack(this, MSG_LOAD_DEBUG_STATUS);
	}
#endif

#ifdef APPLICATION_CACHE_SUPPORT
	BOOL url_has_application_cache_fallback = FALSE;

	/* Check if this url is loaded from an application cache, and check if it had a fallback url*/
	LEAVE_IF_ERROR(url->CheckAndSetApplicationCacheFallback(url_has_application_cache_fallback));

	/* If it had an fallback url, CheckAndSetApplicationCacheFallback sets fallback url and broadcasts URL_LOADED message. */
	if (url_has_application_cache_fallback)
		return TRUE;
#endif // APPLICATION_CACHE_SUPPORT

	g_main_message_handler->UnsetCallBack(this, MSG_URL_TIMEOUT);
	g_main_message_handler->RemoveDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 0);
	g_main_message_handler->RemoveDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 1);

	if(par2 == URL_ERRSTR(SI, ERR_COMM_CONNECTION_CLOSED) && storage && storage->GetIsMultipart())
	{
		HandleFinished();
		return FALSE;
	}

#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
	SetAttribute(URL::KUsedForFileDownload, FALSE);
#endif

	if (http_data)
	{
		http_data->flags.redirect_blocked = FALSE;
		SetAttribute(URL::KSpecialRedirectRestriction, FALSE);
	}
	if(par2 == URL_ERRSTR(SI, ERR_AUTO_PROXY_CONFIG_FAILED)
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
		|| par2 == ERR_COMM_RELOAD_DIRECT
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
	)
	{
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
		if (par2 == ERR_COMM_RELOAD_DIRECT)
		{
			SetLoadDirect(TRUE);
			SetAttribute(URL::KReloadSameTarget, TRUE);
		}
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
		g_main_message_handler->UnsetCallBacks(this);
		SetAttribute(URL::KHeaderLoaded,FALSE);
		SetAttribute(URL::KLoadStatus, URL_LOADING);
		info.determined_proxy = FALSE;
		info.use_proxy = FALSE;
		if(http_data)
			http_data->sendinfo.proxyname.Empty();

		MessageHandler* mh = mh_list->FirstMsgHandler();

		if(!mh)
			mh = g_main_message_handler;
		URL dummy;
		if (loading)
			DeleteLoading();
		StartLoading(mh, (http_data ? http_data->sendinfo.referer_url : dummy));
		return FALSE;
	}

	// Str::SI_ERR_HTTP_100CONTINUE_ERROR is Non - fatal error (Hardcoded)
	if(par2 == URL_ERRSTR(SI, ERR_HTTP_100CONTINUE_ERROR))
	{
#ifdef URL_DISABLE_INTERACTION
		if(url->GetAttribute(URL::KBlockUserInteraction))
		{
			return FALSE;
		}
#endif

		WindowCommander *wic = NULL;
		OpDocumentListener *listener = MhListGetFirstListener(mh_list, wic);
		if(listener)
			listener->OnFormRequestTimeout(wic, url->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped).CStr());
		return FALSE;
	}

	if(loading)
	{
		loading->EndLoading(TRUE);
		g_main_message_handler->RemoveCallBacks(this, loading->Id());
		DeleteLoading();
	}
	//FreeLoadBuf();
#ifdef _DEBUG_DS1
	{
		OpString8 tempname;
		url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, tempname);
		PrintfTofile("urlds1.txt","\nDS HandleFailed- %s\n",
			(tempname.HasContent() ? tempname.CStr() : ""));
		PrintfTofile("urldd1.txt","\nDS HandleFailed- %s\n",
			(tempname.HasContent() ? tempname.CStr() : ""));
	}
#endif
	SetAttribute(URL::KLoadStatus,URL_LOADING_FAILURE);
	SetAttribute(URL::KIsResuming,FALSE);
	SetAttribute(URL::KReloadSameTarget,FALSE);
	url->SetAttribute(URL::KOverrideRedirectDisabled,FALSE);
	SetAttribute(URL::KSendOnlyIfProxy, FALSE);
	info.use_nettype = NETTYPE_UNDETERMINED;
	info.was_inline = TRUE;

	if(http_data)
	{
		http_data->flags.only_if_modified = FALSE;
		http_data->flags.proxy_no_cache = FALSE;

		if(http_data->flags.connect_auth)
		{
			// A non-200 response to a CONNECT request is treated as unsecure
			SetSecurityStatus(SECURITY_STATE_NONE, NULL);
			URL this_url(url, (char *) NULL);
			GetMessageHandlerList()->SetProgressInformation(SET_SECURITYLEVEL,SECURITY_STATE_NONE, NULL, &this_url);
			// documents served from the CONNECT response must not execute scripts
			url->SetAttribute(URL::KSuppressScriptAndEmbeds, MIME_Remote_Script_Disable);
		}
	}

	// force finish, don't try to save data otherwise we might get another failure callback
	if(storage)
		storage->SetFinished(TRUE);

	if(old_storage)
	{
		DataStream_Conditional_Sensitive_ByteArray(cache_destruct, GetAttribute(URL::KCachePolicy_NoStore));

		// We have non-empty old_storage. As we are in HandleFailed() function,
		// the new storage is not necessary better. It can be empty.
		// In this case we may want to use the old storage, if it contains
		// some useful content or generate an error if it doesn't.
		// See also bug CORE-31350.

		// Most probably we'll want to use the new storage,
		// thus let our initial decision be like this.
		BOOL use_old_storage = FALSE;

		// Making of the decision is organized as do-while(0) loop here,
		// because it is more readable than a set of complex nested
		// if-else code blocks.
		do
		{
			// If the new storage is not empty - we'll use it
			// instead of the old one.
			if (storage)
			{
				OP_ASSERT(use_old_storage == FALSE);
				break;
			}

			// New storage is empty.

			// If the user has initiated the loading - he probably wants
			// some reaction, reflecting the current situation. If the browser
			// will just continue showing the old content - the user will
			// think, that it [the browser] ignores his actions, like hitting
			// Reload button. That's why we'll not use the old content, but
			// accept the absence of content during the current loading attempt
			// and generate an error later.
			if (info.user_initiated)
			{
				OP_ASSERT(use_old_storage == FALSE);
				break;
			}

			// "Forcing" rules didn't match. Let's estimate usefulness
			// of the old content then. We will only be able to do it for
			// HTTP/HTTPS URLs.
			URLType type = (URLType) url->GetAttribute(URL::KType);
			BOOL is_http = (type == URL_HTTP || type == URL_HTTPS);

			// If the protocol is not HTTP/HTTPS - we can't find out
			// whether the old content is useful or not. It's probably
			// useful, because it could be loaded and put in cache.
			// It also matches the original behaviour.
			if (!is_http)
			{
				use_old_storage = TRUE;
				break;
			}

			// If old content was loaded with 2xx code - it's useful.
			// Otherwise it contains an old error message (4xx, 5xx)
			// or a "trace" of an incomplete load (1xx, 3xx).
			unsigned short old_response_code = old_storage->GetHTTPResponseCode();
			if (old_response_code >= 200 && old_response_code < 300)
				use_old_storage = TRUE;
			else
				OP_ASSERT(use_old_storage == FALSE);
		}
		while (0);

		if (use_old_storage)
		{
			// No memory is leaked on the subsequent assignment.
			OP_ASSERT(!storage);

			storage = old_storage;
		}
		else
			OP_DELETE(old_storage);
		old_storage = NULL;
	}

#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	BOOL broadcast = TRUE;
	if(par2 == URL_ERRSTR(SI, ERR_OUT_OF_COVERAGE))
	{
		if(storage)
			par2 = ERR_SSL_ERROR_HANDLED;
		else

		{
			par2 = URL_ERRSTR(SI, ERR_COMM_NETWORK_UNREACHABLE);

			OpStringC filename(g_pcnet->GetStringPref(PrefsCollectionNetwork::OutOfCoverageFileName));
			if(filename.Length() >0 )
			{
				storage = Local_File_Storage::Create(url, filename);

				if(storage)
				{
					SetAttribute(URL::KIsOutOfCoverageFile, TRUE);
					// Assumes that the out of coverage file is a HTML file and that
					// is has no specific charset, or is defined in the file
					SetAttribute(URL::KMIME_Type, "text/html");
					SetAttribute(URL::KContentType, URL_HTML_CONTENT);
					SetAttribute(URL::KMIME_CharSet, NULL); // Use Default

					// Act as if this is a regular document loading, not a load failure
					BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
					SetAttribute(URL::KHeaderLoaded, TRUE);
					url->SetAttribute(URL::KIsFollowed,TRUE);
					SetAttribute(URL::KLoadStatus, URL_LOADED);

					BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
					broadcast = FALSE;
				}
				else
					g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);

			}
		}
	}
#endif

	//OP_ASSERT(!storage || (storage->GetFinished() && storage->Cache_Storage::GetFinished()));
	//mh_list->PostMessage(MSG_URL_LOADING_FAILED, ERR_NO_CONTENT, id, MH_LIST_ALL);
#ifdef _DEBUG_DS
	{
		OpString8 tempname;
		url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, tempname);
		PrintfTofile("urlds.txt","\nReceived Failed %lx: %s\n",(DWORD) par2, (tempname.HasContent() ? tempname.CStr() : ""));
	}
#endif
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	if(broadcast)
#endif
		SendMessages(NULL, FALSE, MSG_URL_LOADING_FAILED, par2);

	{
		MessageHandler* mh = mh_list->FirstMsgHandler();

		while(mh != NULL)
		{
			mh_list->Remove(mh);
			mh->UnsetCallBacks(this);
			mh = mh_list->FirstMsgHandler();
		}
	}

	if (par2 == URL_ERRSTR(SI, ERR_DISK_IS_FULL))
	{
		// Not much we can do about it but report
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_DISK);
	}

	OnLoadFinished(URL_LOAD_FAILED);

	return FALSE;
}

void URL_DataStorage::MoveCacheToOld(BOOL conditionally)
{
#ifdef _DEBUG_DS
	{
		OpString8 tempname;
		url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, tempname);
		PrintfTofile("urlds.txt","\nDS Moving cache to Old: %s\n",(tempname.HasContent() ? tempname.CStr() : ""));
	}
#endif
	if(conditionally && storage == NULL)
		return;

	DataStream_Conditional_Sensitive_ByteArray(cache_destruct, GetAttribute(URL::KCachePolicy_NoStore));

	if (old_storage )
		OP_DELETE(old_storage);
	old_storage = storage;
	storage = NULL;
	if(old_storage)
		old_storage->SetFinished();
}

void URL_DataStorage::ResetCache()
{
	if(storage)
		storage->TruncateAndReset();
}

BOOL URL_DataStorage::CreateCacheL()
{
	OP_STATUS op_err = CreateCache();
	if(OpStatus::IsMemoryError(op_err))
		LEAVE(op_err);

	return (storage != NULL ? TRUE : FALSE);
}

OP_STATUS URL_DataStorage::CreateCache()
{
	if(storage)
		return OpStatus::OK;

	SetAttribute(URL::KIsGenerated, FALSE);
	SetAttribute(URL::KIsGeneratedByOpera, FALSE);

	URLType type = (URLType) url->GetAttribute(URL::KType);
	if(type != URL_HTTP && type != URL_HTTPS) // HTTP(S) initialization of this flag is handled elsewhere
		url->SetAttribute(URL::KSuppressScriptAndEmbeds, MIME_No_ScriptEmbed_Restrictions);

	OP_STATUS op_err = OpStatus::OK;
	storage = CreateNewCache(op_err);
	RETURN_IF_ERROR(op_err);

	if(old_storage)
	{
		DataStream_Conditional_Sensitive_ByteArray(cache_destruct, GetAttribute(URL::KCachePolicy_NoStore));
#ifdef SEARCH_ENGINE_CACHE
		if ((storage->GetCacheType() == URL_CACHE_DISK || storage->GetCacheType() == URL_CACHE_TEMP) && old_storage->GetCacheType() == URL_CACHE_DISK)
			if (((File_Storage *)old_storage)->GetFileName() != NULL &&
				((File_Storage *)old_storage)->DataPresent())
		{
			OP_STATUS err;

			if (storage->GetCacheType() == URL_CACHE_TEMP)
				storage->SetCacheType(URL_CACHE_DISK);

			if ((err = ((File_Storage *)storage)->SetFileName(
				((File_Storage *)old_storage)->GetFileName(),
				((File_Storage *)old_storage)->GetFolder())) != OpStatus::OK)
			{
				OP_DELETE(storage);
				storage = NULL;

				return err;
			}
		}
#endif
		if (old_storage->GetCacheType() == URL_CACHE_DISK || old_storage->GetCacheType() == URL_CACHE_TEMP)
			old_storage->TruncateAndReset();
		OP_DELETE(old_storage);
		old_storage = NULL;
	}

	if(http_data)
	{
		storage->SetHTTPResponseCode(http_data->recvinfo.response);
		storage->TakeOverContentEncoding(http_data->keepinfo.encoding);
	}

	urlManager->SetLRU_Item(this);

	return (storage != NULL) ? OpStatus::OK : OpStatus::ERR;
}


#ifndef RAMCACHE_ONLY
BOOL URL_DataStorage::CachePersistent(void)
{
	URLType utype;

#ifdef APPLICATION_CACHE_SUPPORT
	// Urls is always persistent if the url is in a
	// application cache context, -and- the url is set to be cached.

	//Todo: should ask the cache manager here if the cache persistent.
	// This feature does not yet exist, and the cache manager is in
	// a process of major rewrite. Fix when multimedia cache is done.
	ApplicationCacheManager *man = g_application_cache_manager;
	ApplicationCache *cache = NULL;
	if (man && (cache = man->GetCacheFromContextId(url->GetContextId())))
	{
		const OpStringC url_string = url->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped);
		if (cache->IsHandledByCache(url_string.CStr()))
			return TRUE;
	}

#endif // APPLICATION_CACHE_SUPPORT

	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EmptyCacheOnExit) ||
		!((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheSize)
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
		&& !GetNeverFlush()
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT && __OEM_OPERATOR_CACHE_MANAGEMENT
		)
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	|| (GetNeverFlush() && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OperatorDiskCacheSize))
#endif // __OEM_EXTENDED_CACHE_MANAGEMENT && __OEM_OPERATOR_CACHE_MANAGEMENT
		) ||
		(!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheDocs) &&
		!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheFigs) &&
		!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheOther)))
		return FALSE;

	if (url->GetAttribute(URL::KUnique) || url->GetAttribute(URL::KHavePassword) || url->GetAttribute(URL::KHaveAuthentication)
#ifdef _MIME_SUPPORT_
		|| url->GetAttribute(URL::KIsAlias)
#endif
		)
		return FALSE;

	utype = (URLType)url->GetAttribute(URL::KType);

	if ((utype == URL_HTTP || (utype == URL_HTTPS &&
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
		(GetAttribute(URL::KNeverFlush) ||
		g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheHTTPS) )
#else
		g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheHTTPS)
#endif
				)) &&
		//!url->GetAttribute(URL::KSpecialRedirectRestriction) &&
		(	url->GetAttribute(URL::KHTTP_Response_Code) < HTTP_MULTIPLE_CHOICES /* 300*/ ||
			url->GetAttribute(URL::KHTTP_Response_Code) == HTTP_MOVED /* 301 - Permanently moved deserve to be persistent */
		))
		return TRUE;

	return FALSE;
}
#endif // !RAMCACHE_ONLY

Cache_Storage *URL_DataStorage::CreateNewCache(OP_STATUS &op_err, BOOL force_to_file)
{
	op_err = OpStatus::OK;
	OpAutoPtr<Cache_Storage> new_storage(NULL);

#ifdef _DEBUG_DS
	OpString8 tempname;
	url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, tempname);
	PrintfTofile("urlds.txt","\nDS Creating Cache: %s\n",(tempname.CStr() ? tempname.CStr() : ""));
#endif
	local_time_loaded = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);

#ifdef _MIME_SUPPORT_
	if((URLType) url->GetAttribute(URL::KType) != URL_EMAIL)
#endif
	{

#ifdef RAMCACHE_ONLY
		if(!force_to_file)
			new_storage.reset(OP_NEW(Memory_Only_Storage, (url)));//FIXME:OOM
		else
		{
			File_Storage *fnew_storage = File_Storage::Create(url, (uni_char *) NULL, NULL, OPFILE_CACHE_FOLDER,
										  URL_CACHE_TEMP, TRUE, TRUE);
			new_storage.reset(fnew_storage);
		}
#else // not RAMCACHE_ONLY

		if(!force_to_file && (GetAttribute(URL::KCachePolicy_NoStore)
# ifdef DISK_CACHE_SUPPORT
                || !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheToDisk) // pref have  priority over context setting
# endif // DISK_CACHE_SUPPORT

				|| urlManager->GetContextIsRAM_Only(url->GetContextId())

# if defined STRICT_CACHE_LIMIT2
				|| urlManager->FindContextManager(url->GetContextId())->PredictedWillExceedCacheSize(content_size, url)
# endif // defined STRICT_CACHE_LIMIT2

			))
		{
		#ifdef MULTIMEDIA_CACHE_SUPPORT
			if(url->GetAttribute(URL::KMultimedia))
			{
				Multimedia_Storage* temp_store = Multimedia_Storage::Create(url, NULL, TRUE);  // Force in RAM
				new_storage.reset(temp_store);
			}
			else
		#endif //MULTIMEDIA_CACHE_SUPPORT
				new_storage.reset(OP_NEW(Memory_Only_Storage, (url)));//FIXME:OOM
		}
		else
		{
# if defined STRICT_CACHE_LIMIT2
			urlManager->FindContextManager(url->GetContextId())->AddToPredictedCacheSize(content_size, url);
# endif // STRICT_CACHE_LIMIT2

		#ifdef MULTIMEDIA_CACHE_SUPPORT
			if(url->GetAttribute(URL::KMultimedia))
			{
				Multimedia_Storage* temp_store = Multimedia_Storage::Create(url);
				new_storage.reset(temp_store);
			}
			else
		#endif //MULTIMEDIA_CACHE_SUPPORT
			if(CachePersistent()
# ifdef DISK_CACHE_SUPPORT
                && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheToDisk)
# endif // DISK_CACHE_SUPPORT
				)
			{
				Persistent_Storage* temp_store = Persistent_Storage::Create(url);
				new_storage.reset(temp_store);
			}
			else
			{
				Session_Only_Storage* temp_store = Session_Only_Storage::Create(url);
				new_storage.reset(temp_store);
			}
		}
#endif // not RAMCACHE_ONLY

		if(new_storage.get() == NULL)
		{
			op_err = OpStatus::ERR_NO_MEMORY;
			return NULL;
		}
	}

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	if(new_storage.get() != NULL && GetNeverFlush())
		new_storage->SetNeverFlush();
#endif

#if defined URL_ENABLE_ACTIVE_COMPRESS_CACHE
	if(new_storage.get() != NULL && info.compress_cache)
	{
		if(!http_data || http_data->keepinfo.encoding.IsEmpty())
			new_storage->ConfigureEncode();
	}
#endif

#ifdef _MIME_SUPPORT_
	if((URLType) url->GetAttribute(URL::KType) == URL_EMAIL ||
		((
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
		  (URLContentType) url->GetAttribute(URL::KContentType) == URL_MHTML_ARCHIVE ||
#endif
		  (URLContentType) url->GetAttribute(URL::KContentType) == URL_MIME_CONTENT) &&
#ifdef CONTENT_DISPOSITION_ATTACHMENT_FLAG
		!url->GetAttribute(URL::KUsedContentDispositionAttachment) &&
#endif
		new_storage.get()))
	{

		OpAutoPtr<MIME_DecodeCache_Storage> processor(OP_NEW(MIME_DecodeCache_Storage, (url,NULL )));//FIXME:OOM The MIME_DecodeCache_Storage constructor should be rewritten.

		if(processor.get() == NULL)
		{
			op_err = OpStatus::ERR_NO_MEMORY;
			return NULL;
		}

		RETURN_VALUE_IF_ERROR(op_err = processor->Construct(url, new_storage.release()), NULL);
		processor->SetDecodeOnly(!!GetAttribute(URL::KMIME_Decodeonly));

		new_storage.reset(processor.release());
		RETURN_VALUE_IF_ERROR(op_err = SetAttribute(URL::KContentType, URL_XML_CONTENT), NULL);
		BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
	}
#endif

	if(	new_storage.get() && info.forceCacheKeepOpenFile)
		new_storage->ForceKeepOpen();

	op_err = OpStatus::OK;
	return new_storage.release();
}

OP_STATUS URL_DataStorage::CreateStreamCache()
{
	OpAutoPtr<StreamCache_Storage> temp_storage(OP_NEW(StreamCache_Storage, (url)));
	if(temp_storage.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;

	OpStringS8 content_encoding;

	if(http_data)
		content_encoding.TakeOver(http_data->keepinfo.encoding);

	TRAP_AND_RETURN(op_err, temp_storage->ConstructL(storage, content_encoding));

	urlManager->MakeUnique(url);

	if (storage)
		OP_DELETE(storage);

	storage = temp_storage.release();

	if(old_storage)
	{
		DataStream_Conditional_Sensitive_ByteArray(cache_destruct, GetAttribute(URL::KCachePolicy_NoStore));
		OP_DELETE(old_storage);
		old_storage = NULL;
	}

	urlManager->SetLRU_Item(this);

	return OpStatus::OK;
}

//FIXME:OOM Check callers
unsigned long URL_DataStorage::PrepareForViewing(BOOL get_raw_data, BOOL get_decoded_data, BOOL force_to_file)
{
	if((URLStatus) GetAttribute(URL::KLoadStatus) == URL_LOADING && (!http_data ||
		!storage || !storage->GetFinished() || storage->GetIsMultipart()
		))
		return 0;

	if((URLType) url->GetAttribute(URL::KType) == URL_FILE ||
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
		(URLContentType) GetAttribute(URL::KContentType) == URL_MHTML_ARCHIVE ||
#endif
		(URLContentType) GetAttribute(URL::KContentType) == URL_MIME_CONTENT)
		return 0; // never do anything with FILE URLs or MIME decoded files (it would replace the file)

	BOOL recreate = FALSE;
#ifndef RAMCACHE_ONLY
	if(GetAttribute(URL::KCachePolicy_NoStore))
#endif
	{
		recreate = TRUE;
		RETURN_VALUE_IF_ERROR(SetAttribute(URL::KCachePolicy_NoStore, FALSE), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
	}

#ifdef _HTTP_COMPRESS
	if(!recreate && storage && storage->GetContentEncoding().HasContent() && get_decoded_data)
		recreate = TRUE;
#endif
	if(!recreate && force_to_file && storage && storage->GetCacheType() == URL_CACHE_MEMORY)
		recreate = TRUE;

	// Embedded files are kept in RAM
	#if CACHE_SMALL_FILES_SIZE>0
		if(storage && storage->IsEmbedded())
			recreate = TRUE;
	#endif

	// Container files are not readable
	#if CACHE_CONTAINERS_ENTRIES>0
		if(storage && storage->GetContainerID()>0)
			recreate = TRUE;
	#endif

	if(recreate && storage)
	{
		content_size = 0; // length is unpredictable, and will most likely not be the same.

		OpAutoPtr<URL_DataDescriptor> desc(GetDescriptor(NULL, get_raw_data, get_decoded_data));
		if(!desc.get())
			return URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR);
		OP_STATUS op_err = OpStatus::OK;
		OpAutoPtr<Cache_Storage> new_storage(CreateNewCache(op_err,TRUE));
		RETURN_VALUE_IF_ERROR(op_err, URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));

		if(!new_storage.get())
		{
			desc.reset();
			return URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR);
		}

		BOOL more = TRUE;
		while(more)
		{
			unsigned int len = desc->RetrieveData(more);

			if(len)
			{
				RETURN_VALUE_IF_ERROR(new_storage->StoreData((unsigned char *) desc->GetBuffer(), len), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				if(new_storage->GetFinished())
				{
					desc.reset();
					new_storage.reset();
					return URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR);
				}
				desc->ConsumeData(len);
			}
		}

		// Please the checks of the cache, keeping calls to AddToRamCacheSize() and SubFromRamCacheSize() balanced
		OP_DELETE(storage);
		storage = NULL;
		new_storage->SetFinished();

		desc.reset();
		storage = new_storage.release();
		urlManager->SetLRU_Item(this);

		if(http_data && get_decoded_data)
		{
			RETURN_VALUE_IF_ERROR(SetAttribute(URL::KHTTPEncoding, NULL), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		}
	}

	if(storage)
	{
#if (CACHE_SMALL_FILES_SIZE>0 || CACHE_CONTAINERS_ENTRIES>0)
		storage->SetPlainFile(TRUE);
#endif

		storage->Flush();
		storage->CloseFile();
	}

	return 0;
}

void URL_DataStorage::EndLoading(BOOL force)
{
	if(loading)
	{
		loading->EndLoading(force);
		DeleteLoading();
	}

	if(storage == NULL && old_storage != NULL)
	{
		storage = old_storage;
		old_storage = NULL;
	}
	else if(storage)
		storage->SetFinished();

	urlManager->SetLRU_Item(this);
}

void URL_DataStorage::RemoveMessageHandler(MessageHandler *old_mh)
{
	if (mh_list->IsEmpty())
		return;

	old_mh->UnsetCallBacks(this);
	mh_list->Remove(old_mh);
	if(mh_list->IsEmpty())
		StopLoading(NULL);
}

void URL_DataStorage::StopLoading(MessageHandler* msg_handler)
{
#ifdef DEBUG_LOAD_STATUS
	OpString8 tempname;
	url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, tempname);
	PrintfTofile("loading.txt","     URL request Id %d : StopLoading tick=%lu/%lu: %s\n",
		(loading ? loading->Id() : 0), (unsigned long) g_op_time_info->GetWallClockMS(),
		(unsigned long) g_op_time_info->GetWallClockMS()-started_tick, tempname.CStr());
	started_tick = 0;
#endif

	OP_ASSERT(url->reference_count>0); // Else SetAttribute(URL::KOverrideRedirectDisabled, FALSE) will crash...

	URL moved_to_url = GetAttribute(URL::KMovedToURL);
	if(moved_to_url.IsValid())
		moved_to_url.StopLoading(msg_handler);
	//if (GetStatus() == URL_LOADING)
	if(msg_handler)
	{
		//MessageHandler* first_mh = mh_list->FirstMsgHandler();
		mh_list->Remove(msg_handler);
		msg_handler->UnsetCallBacks(this);
		if (!mh_list->IsEmpty())
		{
#if 0
			if (first_mh == msg_handler)
			{
				// msg_handler (== first_mh) is the one receiving messages from HTTP
				// need to transfer this to new first in list:
				first_mh = mh_list->FirstMsgHandler();
				/*
				msg_handler->UnsetCallBacks(url);
				if(loading)
				{
					loading->ChangeMsgHandler(first_mh);
					loading->SetCallbacks(url,url);
				}
				*/
			}
#endif
			return;
		}
	}
	else
	{
		MessageHandler* mh = mh_list->FirstMsgHandler();

		while(mh != NULL)
		{
			mh_list->Remove(mh);
			mh->UnsetCallBacks(this);
			mh = mh_list->FirstMsgHandler();
		}
	}

#ifdef GOGI_URL_FILTER
	if (g_urlfilter) {
		OpStatus::Ignore(g_urlfilter->CancelCheckURLAsync(URL(url, (char *) NULL)));
	}
#endif // GOGI_URL_FILTER

#ifdef DEBUG_LOAD_STATUS
		if (g_main_message_handler)
			g_main_message_handler->UnsetCallBack(this, MSG_LOAD_DEBUG_STATUS);
#endif

	if(storage)
		storage->SetFinished();
	else
		UseOldCache();

	if(loading)
	{
		loading->EndLoading(TRUE);
		DeleteLoading();
	}

	SetAttribute(URL::KIsResuming,FALSE);
	SetAttribute(URL::KReloadSameTarget,FALSE);

	url->SetAttribute(URL::KOverrideRedirectDisabled,FALSE);
	SetAttribute(URL::KSendOnlyIfProxy, FALSE);
	info.was_inline = TRUE;

	//FreeLoadBuf();
	if (g_main_message_handler)
	{
		g_main_message_handler->UnsetCallBacks(this);
		g_main_message_handler->RemoveDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 0);
		g_main_message_handler->RemoveDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 1);
	}

	if ((URLStatus) GetAttribute(URL::KLoadStatus) == URL_LOADING)
	{
		SetAttribute(URL::KLoadStatus,URL_LOADING_ABORTED);
		OnLoadFinished(URL_LOAD_STOPPED);
	}

	range_offset = 0;
}

#ifdef DYNAMIC_PROXY_UPDATE
void URL_DataStorage::StopAndRestartLoading()
{
	BOOL dont_restart = FALSE;
	URL moved_to_url = GetAttribute(URL::KMovedToURL);
	if(moved_to_url.IsValid())
	{
		moved_to_url.StopAndRestartLoading();
		dont_restart = TRUE;
	}

	if ((URLStatus) GetAttribute(URL::KLoadStatus) != URL_LOADING)
		return;

	if(dont_restart)
	{
		HandleFinished();
		return;
	}

	BroadcastMessage(MSG_MULTIPART_RELOAD, url->GetID(), 0, MH_LIST_ALL);
	BroadcastMessage(MSG_INLINE_REPLACE, url->GetID(), 0, MH_LIST_ALL);

	if(storage)
	{
		storage->SetFinished();
		if(!GetAttribute(URL::KReloadSameTarget))
			MoveCacheToOld();
	}
	//else
		//UseOldCache();

	if(loading)
	{
		loading->EndLoading(TRUE);
		DeleteLoading();
	}

//	SetStatus(URL_LOADING_ABORTED);

	MessageHandler* mh = mh_list->FirstMsgHandler();

	CommState state = COMM_REQUEST_FAILED;
	if(mh)
	{
		OnLoadFinished(URL_LOAD_STOPPED);
		OnLoadStarted();

		g_main_message_handler->UnsetCallBacks(this);
		SetAttribute(URL::KHeaderLoaded,FALSE);
		SetAttribute(URL::KLoadStatus, URL_LOADING);
		info.determined_proxy = FALSE;
		info.use_proxy = FALSE;
		if(http_data)
			http_data->sendinfo.proxyname.Empty();

		URL dummy;
		state = StartLoading(mh, (http_data ? http_data->sendinfo.referer_url : dummy));
	}

	if(state != COMM_LOADING)
		HandleFailed(ERR_SSL_ERROR_HANDLED); // Silently stop loading
		/*
	else
	{
		//BroadcastMessage(MSG_MULTIPART_RELOAD, url->GetID(), 0, MH_LIST_ALL);
		//BroadcastMessage(MSG_INLINE_REPLACE, url->GetID(), 0, MH_LIST_ALL);
	}*/
}
#endif // DYNAMIC_PROXY_UPDATE

unsigned char* URL_DataStorage::CheckLoadBuf(unsigned long buf_size)
{
	if(g_ds_buf && buf_size > g_ds_lbuf)
	{
		OP_DELETEA(g_ds_buf);
		g_ds_buf = 0;
		g_ds_lbuf = 0;
	}
	if (!g_ds_buf)
	{
		g_ds_lbuf = MAX( (unsigned long) DEFAULT_FILE_LOAD_BUF_SIZE, buf_size);
		g_ds_buf = OP_NEWA(unsigned char, g_ds_lbuf);
	}

	return g_ds_buf;
}

void URL_DataStorage::ReceiveDataL()
{
#ifdef _DEBUG_DS
	//PrintfTofile("winsck.txt","DS Receive Data entry: %s\n", (url->Name() ? url->Name() : ""));
#endif
	if(!loading)
		return;
	unsigned long bsize = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize)*1024;
	unsigned char *buf = CheckLoadBuf(bsize);
	if(buf == NULL)
	{
		EndLoading(TRUE);
		return;
	}

	//PrintfTofile("http.txt","\nDS Receive Data - Starting to read : %s\n", url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
	unsigned long read_data = loading->ReadData((char *) buf, bsize);
	//PrintfTofile("http.txt","\nDS Receive Data - read %d bytes: %s\n",read_data, url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr());
	if(read_data == 0)
		return;

	BOOL firstData = storage == NULL;
	if(firstData && OpStatus::IsError(CreateCache()))
	{
		op_memset(buf, 0, read_data);
		EndLoading(TRUE);
		return;
	}

#ifdef _DEBUG_DS
	OpString8 tempname;
	url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, tempname);
	PrintfTofile("urlds.txt","\nDS Receive Data::GetContent() - read %d bytes: %s\n",read_data, tempname.CStr());
	DumpTofile(buf, read_data,"","urlds.txt");

	URLContentType ctype = (URLContentType) GetAttribute(URL::KContentType);

	if(ctype == URL_HTML_CONTENT || ctype == URL_TEXT_CONTENT ||
		ctype == URL_XML_CONTENT || ctype == URL_X_JAVASCRIPT ||
		ctype == URL_CSS_CONTENT || ctype == URL_PAC_CONTENT)
	{
		const unsigned char *rdata = buf;
		unsigned long len = read_data;
		OpString8 encoded;

		if(GetAttribute(URL::KMIME_CharSet).CompareI("utf-16") == 0)
		{
			if(OpStatus::IsSuccess(encoded.SetUTF8FromUTF16((const uni_char *) buf, UNICODE_DOWNSIZE(read_data))))
			{
				rdata = (unsigned char *) encoded.CStr();
				len = encoded.Length();
			}
		}

		PrintfTofile("urlds.txt","\n-----\n%.*s\n------\n",len,rdata);
	}

#endif

	OP_STATUS op_err;
	OpFileLength start_byte = FILE_LENGTH_NONE;

#ifdef URL_ENABLE_HTTP_RANGE_SPEC
		if(
			http_data &&
				(
					http_data->sendinfo.range_start !=FILE_LENGTH_NONE ||
					url->GetAttribute(URL::KMultimedia)
				)
			)
			start_byte = HTTPRange::ConvertFileLengthNoneToZero(http_data->sendinfo.range_start) + range_offset;
#endif

	op_err=storage->StoreData(buf, read_data, start_byte);

	op_memset(buf, 0, read_data);

	RAISE_IF_MEMORY_ERROR(op_err);
	LEAVE_IF_ERROR(op_err);

	range_offset += read_data;

	if(storage->GetIsMultipart())
		return;

	uint32 response = GetAttribute(URL::KHTTP_Response_Code);
	if (firstData || !content_type_deterministic)
	{
		ContentDetector content_detector(url, FALSE, FALSE);
		content_detector.DetectContentType();
		content_type_deterministic = content_detector.WasTheCheckDeterministic();
		if((URLContentType) GetAttribute(URL::KContentType) != URL_UNDETERMINED_CONTENT &&
			(!http_data || !http_data->flags.header_loaded_sent && (
				(http_data->flags.auth_status == HTTP_AUTH_NOT || response != HTTP_UNAUTHORIZED) &&
				(http_data->flags.proxy_auth_status == HTTP_AUTH_NOT || response != HTTP_PROXY_UNAUTHORIZED) ) ))
		{
			BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
			if(http_data)
				http_data->flags.header_loaded_sent=TRUE;
			url->Access(FALSE);
		}
	}
	if(GetAttribute(URL::KHeaderLoaded) &&
		 (!http_data || (URLType) url->GetAttribute(URL::KType) == URL_FTP ||
			((response!= HTTP_UNAUTHORIZED || url->GetAttribute(URL::KSpecialRedirectRestriction)) &&
				response!= HTTP_PROXY_UNAUTHORIZED &&
				http_data->flags.header_loaded_sent)))
	{
		BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
	}
}

OP_STATUS URL_DataStorage::FindContentType(URLContentType &ret_ct,
										   const char* mimetype,
										   const uni_char* ext, const uni_char *name, BOOL set_mime_type)
{
	URLType type = (URLType) url->GetAttribute(URL::KType);
    BOOL use_mime = TRUE;

	// if mime is not set, must use extension
	if (mimetype==NULL || *mimetype == 0)
		use_mime = FALSE;
	// always use mime for mail/news
	else if( type==URL_SNEWSATTACHMENT || type==URL_NEWSATTACHMENT || type==URL_ATTACHMENT || type==URL_EMAIL || type==URL_CONTENT_ID)
		use_mime = TRUE;

	if( use_mime == FALSE )
	{
		const uni_char *fileext;

		if (ext && *ext)
			fileext = ext;
		else
			fileext = FindFileExtension(name);

		if (fileext)
		{
			Viewer* viewer = g_viewers->FindViewerByExtension(fileext);
			if (viewer)
			{
				mimetype = viewer->GetContentTypeString8();
			}
			else
			{
				ret_ct = URL_UNDETERMINED_CONTENT;
				return OpStatus::OK;
			}
		}
		else
		{
			ret_ct = URL_UNDETERMINED_CONTENT;
			return OpStatus::OK;
		}
	}

	if (mimetype)
	{
		OpString8 mimetype_buf;

		OP_STATUS op_err = mimetype_buf.Set(mimetype); // handled below

		if(set_mime_type /*&& content_type_string.IsEmpty()*/)
		{
			OpStatus::Ignore(content_type_string.Set(mimetype)); // not usually significant if there is an error
		}
		else
			set_mime_type = FALSE;


		if(OpStatus::IsError(op_err))
		{
			RAISE_IF_MEMORY_ERROR(op_err);
			ret_ct = URL_UNDETERMINED_CONTENT;
			return op_err;
		}

		if (Viewer *v = g_viewers->FindViewerByMimeType(mimetype_buf))
		{
			ret_ct = v->GetContentType();
			return OpStatus::OK;
		}

			/*
			else
			{
				if ((type == URL_HTTP || type == URL_HTTPS) &&
				    g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TrustServerTypes) == FALSE)
				{
					// for unknown mimetypes (no registred viewer), force probing of content later
					ret_ct = URL_UNDETERMINED_CONTENT;
					return OpStatus::OK;
				}
			}
			*/

		if (mimetype_buf.Compare("*/*") == 0 ||
			mimetype_buf.FindFirstOf('/') == KNotFound)
		{
			if(set_mime_type)
				content_type_string.Empty(); // Don't keep these

			ret_ct = URL_UNDETERMINED_CONTENT;
			return OpStatus::OK;
		}
		else
		{
			ret_ct = URL_UNKNOWN_CONTENT;
			return OpStatus::OK;
		}
	}

	ret_ct = URL_UNDETERMINED_CONTENT;
	return OpStatus::OK;
}


OP_STATUS URL_DataStorage::CheckHTTPProtocolData()
{
	if(!http_data)
	{
		http_data = OP_NEW(URL_HTTP_ProtocolData, ());
		if(http_data == NULL)
			return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

URL_HTTP_ProtocolData *URL_DataStorage::GetHTTPProtocolData()
{
	RAISE_IF_MEMORY_ERROR(CheckHTTPProtocolData());
	return http_data;
}


/*
URL_FTP_ProtocolData *URL_DataStorage::GetFTPProtocolData()
{
	if(!protocol_data.ftp_data)
	{
		protocol_data.ftp_data = new URL_FTP_ProtocolData();//FIXME:OOM
		if(protocol_data.ftp_data == NULL)
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
	return protocol_data.ftp_data;
}
*/

OP_STATUS URL_DataStorage::CheckMailtoProtocolData()
{
	if((URLType) url->GetAttribute(URL::KType) == URL_MAILTO && !protocol_data.mailto_data)
	{
		protocol_data.mailto_data = OP_NEW(URL_Mailto_ProtocolData, ());
		if(protocol_data.mailto_data == NULL)
			return OpStatus::ERR_NO_MEMORY;
	}
	return (protocol_data.mailto_data ? OpStatus::OK : OpStatus::ERR);
}

URL_Mailto_ProtocolData *URL_DataStorage::GetMailtoProtocolData()
{
	RAISE_IF_MEMORY_ERROR(CheckMailtoProtocolData());
	return protocol_data.mailto_data;
}

URL_HTTP_ProtocolData *URL_DataStorage::GetHTTPProtocolDataL()
{
	LEAVE_IF_ERROR(CheckHTTPProtocolData());
	return http_data;
}

OP_STATUS URL_DataStorage::CheckFTPProtocolData()
{
	if(!protocol_data.ftp_data)
	{
		protocol_data.ftp_data = OP_NEW(URL_FTP_ProtocolData, ());
		if(!protocol_data.ftp_data)
			return OpStatus::ERR_NO_MEMORY;

	}
	return OpStatus::OK;
}

URL_FTP_ProtocolData *URL_DataStorage::GetFTPProtocolDataL()
{
	LEAVE_IF_ERROR(CheckFTPProtocolData());
	return protocol_data.ftp_data;
}

URL_Mailto_ProtocolData *URL_DataStorage::GetMailtoProtocolDataL()
{
	LEAVE_IF_ERROR(CheckMailtoProtocolData());
	return protocol_data.mailto_data;
}

BOOL URL_DataStorage::DumpSourceToDisk(BOOL force_create)
{
	BOOL result;
	if(!storage && force_create)
	{
		OpStatus::Ignore(CreateCache());
	}
	if(storage && !storage->GetIsMultipart())
	{
		storage->SetSavePosition((http_data && (http_data->sendinfo.range_start!=FILE_LENGTH_NONE || http_data->sendinfo.range_end!=FILE_LENGTH_NONE)) ? http_data->sendinfo.range_start : FILE_LENGTH_NONE);
		result =storage->Flush();

		storage->SetSavePosition(FILE_LENGTH_NONE); // Just to be on the safe side...

		if(!(result))
			return FALSE;
		//urlManager->SetLRU_Item(this); // Not needed, updated in cache storage element, when needed;
		return result;
	}
	return FALSE;
}

#ifdef URL_ENABLE_ASSOCIATED_FILES
AssociatedFile *URL_DataStorage::CreateAssociatedFile(URL::AssociatedFileType type)
{
	AssociatedFile *f;

	if (storage == NULL)
		return NULL;

	BOOL streaming;
	BOOL ram;
	BOOL embedded_storage;

	storage->GetCacheInfo(streaming, ram, embedded_storage);

	if(embedded_storage) // in this case, the storage need to be recreated as File storage...
	{
		PrepareForViewing(TRUE, TRUE, TRUE);

	#ifdef SELFTEST
		storage->GetCacheInfo(streaming, ram, embedded_storage);

		OP_ASSERT(!embedded_storage);
		OP_ASSERT(!storage->IsEmbedded());
	#endif
	}

	if ((f = storage->CreateAssociatedFile(type)) != NULL)
		assoc_files |= type;
	return f;
}

AssociatedFile *URL_DataStorage::OpenAssociatedFile(URL::AssociatedFileType type)
{
	if (storage == NULL)
		return NULL;

	return storage->OpenAssociatedFile(type);
}

OP_BOOLEAN URL_DataStorage::GetFirstAssocFName(OpString &fname, URL::AssociatedFileType &type)
{
	OpString rel_name;
//	OpFileFolder folder;
	OpFileFolder folder_assoc;

	type = (URL::AssociatedFileType)1;

	while (type != 0 && (assoc_files & type) == 0)
		type = (URL::AssociatedFileType)((int)type << 1);

	if (type == 0)
		return OpBoolean::IS_FALSE;

	if (storage == NULL)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(storage->AssocFileName(rel_name, type, folder_assoc, FALSE));

	if (!rel_name.HasContent())
		return OpStatus::ERR_FILE_NOT_FOUND;

//	storage->FileName(folder, FALSE);
//	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(folder, fname));
	fname.Set("");

	RETURN_IF_ERROR(fname.Append(rel_name));

	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN URL_DataStorage::GetNextAssocFName(OpString &fname, URL::AssociatedFileType &type)
{
	OpString rel_name;
//	OpFileFolder folder;
	OpFileFolder folder_assoc;

	do type = (URL::AssociatedFileType)((int)type << 1);
	while (type != 0 && (assoc_files & type) == 0);

	if (type == 0)
		return OpBoolean::IS_FALSE;

	if (storage == NULL)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(storage->AssocFileName(rel_name, type, folder_assoc, FALSE));

	if (!rel_name.HasContent())
		return OpStatus::ERR_FILE_NOT_FOUND;

//	storage->FileName(folder, FALSE);
//	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(folder, fname));
	fname.Set("");

	RETURN_IF_ERROR(fname.Append(rel_name));

	return OpBoolean::IS_TRUE;
}

#endif


BOOL URL_DataStorage::FreeUnusedResources(double cached_system_time_sec)
{
	BOOL result=FALSE;
	if ((URLStatus) GetAttribute(URL::KLoadStatus) == URL_LOADING /* || url->GetUsedCount() != 0*/)
		return result;

	//UrlPrintDebug("URL_Rep::FreeUnusedResources", name, id, 0, 0, 0, 0, 0);
	double system_time=(cached_system_time_sec != 0.0) ? cached_system_time_sec : g_op_time_info->GetTimeUTC()/1000.0;

	{
#ifndef RAMCACHE_ONLY
		URLCacheType ctype = (URLCacheType) GetAttribute(URL::KCacheType);
		if((ctype == URL_CACHE_DISK ||
			ctype == URL_CACHE_TEMP) &&
			(storage->Empty() /* of descriptors */ ||
			local_time_loaded < (time_t) (system_time - 60) ) // Flush anyway if it is one minute old.
			)
		{
			if(DumpSourceToDisk() == TRUE)
				result = TRUE;
			//urlManager->SetLRU_Item(this); // Not needed, updated in the cache elements
		}
#endif
		if(local_time_loaded < (time_t) (system_time - 5*60))
		{
			if(url->GetUsedCount() == 0)
			{
				URLStatus stat = (URLStatus) url->GetAttribute(URL::KLoadStatus, URL::KFollowRedirect);
				if(stat != URL_LOADING && stat != URL_UNLOADED)
				{
					if(!storage || (!storage->IsPersistent() && storage->GetFinished()))
					{
						// Delete URL's that are not cacheable
						url->Unload();
						result = TRUE;
					}
				}
			}
		}
	}
	return result;
}

void URL_Rep::DecUsed(int i)
{
	used -= i;
	if (used < 0)
		used = 0;

	if(used == 0)
	{
		URLStatus stat = (URLStatus) GetAttribute(URL::KLoadStatus, URL::KFollowRedirect);
		if(stat != URL_UNLOADED  && stat != URL_LOADING)
		{
			BOOL unload = FALSE;
			time_t loaded_time = 0;

			GetAttribute(URL::KVLocalTimeLoaded, &loaded_time);

			// Deletes cached documents that are not to be cached according to the cache preferences.
			// Is not affected by type of cache object (memory only/session only/persistent storage)
			if(!GetAttribute(URL::KCachePersistent) &&
				loaded_time < (time_t) (g_op_time_info->GetTimeUTC()/1000.0 - 5*60)) // five minute delay
				unload = TRUE;

			URLType type = (URLType) GetAttribute(URL::KType);
			if (type == URL_HTTPS &&
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
				!GetAttribute(URL::KNeverFlush) &&
#endif
				!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheHTTPSAfterSessions)
#ifdef APPLICATION_CACHE_SUPPORT
				 && !GetAttribute(URL::KIsApplicationCacheURL)
#endif // APPLICATION_CACHE_SUPPORT
				)
				unload = TRUE;
			else if (/*type == URL_FILE ||*/ // File URLs are always reloaded, may just as well save the memory,
				type == URL_OPERA || // and Opera URLs and MIME decoded helper URLs are also thrown out as soon as they are no longer needed
				type == URL_EMAIL|| type == URL_CONTENT_ID  || type == URL_ATTACHMENT ||
				type == URL_SNEWSATTACHMENT ||type == URL_NEWSATTACHMENT)
				unload = TRUE;

			if(GetAttribute(URL::KUnique))
				unload = TRUE;

			if(unload)
				Unload();
		}
	}
}

#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
OP_STATUS URL_Rep::IncLoading()
{
	if(!CheckStorage())
		return OpStatus::ERR_NO_MEMORY;

	storage->IncLoading();
	IncUsed(1);

	return OpStatus::OK;
}

void URL_Rep::DecLoading()
{
	if(storage)
		storage->DecLoading();
	DecUsed(1);
}
#endif


void URL_DataStorage::ChangeMessageHandler(MessageHandler* old_mh, MessageHandler* new_mh)
{
	/*
	MsgHndlListElm*	mle = mh_list->FirstMsgHndlListElm();
	if (!mle)
		return;

	MessageHandler* first_mh = mle->GetMessageHandler();
	BOOL mh_is_first = (first_mh && old_mh == first_mh);

	MessageHandler* next_mh = 0;
	*/

	if (new_mh)
	{
		BOOL has_mh = mh_list->HasMsgHandler(new_mh, FALSE, FALSE, FALSE);
		if (!has_mh)
			RAISE_AND_RETURN_VOID_IF_ERROR(AddMessageHandler(new_mh, TRUE, TRUE));

		//next_mh = new_mh;
	}
	/*
	else if (mh_is_first)
	{
		if (mle)
			mle = mle->Suc();
		if (mle)
			next_mh = mle->GetMessageHandler();
	}
	*/
	if (old_mh)
	{
		old_mh->UnsetCallBacks(this);
		mh_list->Remove(old_mh);
	}

	/*
	if (mh_is_first && next_mh)
	{
#pragma PRAGMAMSG("Hvordan Passed dette inn")
#ifdef _PLUGIN_SUPPORT_
		if (first_mh->HasCallBack(this, MSG_URL_LOAD_PLUGIN_DATA, 0))
			next_mh->SetCallBack(this, MSG_URL_LOAD_PLUGIN_DATA, 0, 0);
#endif
	}
	*/

	if ((URLStatus) GetAttribute(URL::KLoadStatus) == URL_LOADING)
	{
		if (mh_list->IsEmpty())
		{
			if(loading)
			{
#ifdef DEBUG_LOAD_STATUS
	    g_main_message_handler->UnsetCallBack(this, MSG_LOAD_DEBUG_STATUS);
#endif
				loading->EndLoading(TRUE);
				DeleteLoading();
				SetAttribute(URL::KLoadStatus,URL_LOADING_ABORTED); // 08/02/99 YNP
			}
		}
		/*
		else
		{
			if (mh_is_first)
			{
				// msg_handler (== first_mh) is the one receiving messages from HTTP
				// need to transfer this to new first in list:
				//first_mh = mh_list->FirstMsgHandler();
				//loading->ChangeMsgHandler(first_mh);
				//loading->SetCallbacks(url,url);
			}
		}
		*/
	}

	//    Callbacks are removed before calling this routine ???
	//    mh->UnsetCallBacks(this);
}


#ifndef RAMCACHE_ONLY
void URL_DataStorage::WriteCacheDataL(DataFile_Record *rec)
{
	OP_PROBE7_L(OP_PROBE_URL_DATASTORAGE_WRITECACHEDATA_L);

	if(GetAttribute(URL::KFileName).IsEmpty())
		return;

	GetAttributeL(DataStorage_rec_list, rec);
	GetAttributeL(DataStorage_rec_list2, rec);

	rec->AddRecord64L(TAG_CONTENT_SIZE, content_size);
	rec->AddRecord64L(TAG_LOCALTIME_LOADED, local_time_loaded);

#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	if(GetNeverFlush())
		rec->AddRecordL(0, TAG_CACHE_NEVER_FLUSH);
#endif

#ifdef URL_ENABLE_ASSOCIATED_FILES
	if(assoc_files)
	{
		rec->AddRecordL(TAG_ASSOCIATED_FILES, assoc_files);
	}
#endif

	if(http_data)
	{
		DataFile_Record httprec(TAG_HTTP_PROTOCOL_DATA);
		ANCHOR(DataFile_Record, httprec);

		httprec.SetRecordSpec(rec->GetRecordSpec());

		httprec.AddRecordL(TAG_USERAGENT_ID,  GetAttribute(URL::KUsedUserAgentId) );
		httprec.AddRecordL(TAG_USERAGENT_VER_ID, GetAttribute(URL::KUsedUserAgentVersion));

		GetAttributeL(DataStorage_http_rec_list, &httprec);
		GetAttributeL(DataStorage_http_rec_list2, &httprec);

		rec->AddRecord64L(TAG_HTTP_KEEP_EXPIRES, http_data->keepinfo.expires);

		if(GetAttribute(URL::KHTTPRefreshUrlName).HasContent())
		{
			httprec.AddRecordL(TAG_HTTP_REFRESH_INT,GetAttribute(URL::KHTTP_Refresh_Interval));
		}

		OpStringC suggestedname(GetAttribute(URL::KHTTP_SuggestedFileName));

		if(suggestedname.HasContent())
		{
			OpString8 tempname;
			ANCHOR(OpString8, tempname);

			tempname.SetUTF8FromUTF16L(suggestedname.CStr());

			httprec.AddRecordL(TAG_HTTP_SUGGESTED_NAME, tempname);
		}

		HTTP_Link_Relations *relations = (HTTP_Link_Relations *) http_data->recvinfo.link_relations.First();
		while(relations)
		{
			if(relations->OriginalString().HasContent())
				httprec.AddRecordL(TAG_HTTP_LINK_RELATION, relations->OriginalString().CStr());
			relations = relations->Suc();
		}

		if(http_data->recvinfo.all_headers && !http_data->recvinfo.all_headers->Empty())
		{
			DataFile_Record httpheader_rec(TAG_HTTP_RESPONSE_HEADERS);
			ANCHOR(DataFile_Record, httpheader_rec);

			httpheader_rec.SetRecordSpec(rec->GetRecordSpec());

			HeaderEntry *hdr = http_data->recvinfo.all_headers->First();

			while(hdr)
			{
				if(hdr->Name())
				{
					DataFile_Record httpheader_entry(TAG_HTTP_RESPONSE_ENTRY);
					ANCHOR(DataFile_Record, httpheader_entry);

					httpheader_entry.SetRecordSpec(rec->GetRecordSpec());

					httpheader_entry.AddRecordL(TAG_HTTP_RESPONSE_HEADER_NAME, hdr->GetName());
					httpheader_entry.AddRecordL(TAG_HTTP_RESPONSE_HEADER_VALUE, hdr->GetValue());


					httpheader_entry.WriteRecordL(&httpheader_rec);
				}
				hdr = hdr->Suc();
			}

			httpheader_rec.WriteRecordL(&httprec);
		}

		httprec.WriteRecordL(rec);
	}

	if(storage->GetCacheType() == URL_CACHE_USERFILE)
	{
		rec->AddRecordL(TAG_CACHE_DOWNLOAD_FILE);

		rec->AddRecordL(TAG_DOWNLOAD_RESUMABLE_STATUS, GetAttribute(URL::KResumeSupported));

		if(GetAttribute(URL::KFTP_MDTM_Date).HasContent())
		{
			rec->AddRecordL(TAG_DOWNLOAD_FTP_MDTM_DATE, GetAttribute(URL::KFTP_MDTM_Date));
		}
	}

	{
		OpStringC tempname1(GetAttribute(URL::KFileName));
		OpString8 tempname;
		ANCHOR(OpString8, tempname);

		tempname.SetUTF8FromUTF16L(tempname1.CStr());

#ifndef SEARCH_ENGINE_CACHE
		rec->AddRecordL(TAG_CACHE_FILENAME, tempname);
#endif
	}

	{
		URL_DynamicUIntAttributeDescriptor *desc = NULL;
		urlManager->GetFirstAttributeDescriptor(&desc);

		while(desc)
		{
			if(desc->GetCachable() && desc->GetIsFlag())
			{
				if(GetAttribute(desc->GetAttributeID()))
				{
					uint32 mod_id = desc->GetModuleId();
					uint32 tag_id = desc->GetTagID();
					BOOL longform = (mod_id > 255 ||tag_id > 255);
					DataFile_Record attrrec(longform ? TAG_CACHE_DYNATTR_FLAG_ITEM_Long : TAG_CACHE_DYNATTR_FLAG_ITEM);
					ANCHOR(DataFile_Record, attrrec);

					attrrec.SetRecordSpec(rec->GetRecordSpec());

					if(longform)
					{
						attrrec.AddContentL((uint16) mod_id);
						attrrec.AddContentL((uint16) tag_id);
					}
					else
					{
						attrrec.AddContentL((uint8) mod_id);
						attrrec.AddContentL((uint8) tag_id);
					}
					attrrec.WriteRecordL(rec);
				}
			}
			desc = desc->Suc();
		}
	}

	{
		URL_DynIntAttributeElement *item = (URL_DynIntAttributeElement *) dynamic_int_attrs.First();
		while(item)
		{
			uint32 i;
			for (i=0;i< ARRAY_SIZE(item->attributes); i++)
			{
				if(item->attributes[i].desc && item->attributes[i].desc->GetCachable())
				{
					uint32 mod_id = item->attributes[i].desc->GetModuleId();
					uint32 tag_id = item->attributes[i].desc->GetTagID();
					BOOL longform = (mod_id > 255 ||tag_id > 255);
					DataFile_Record attrrec(longform ? TAG_CACHE_DYNATTR_INT_ITEM_Long : TAG_CACHE_DYNATTR_INT_ITEM);
					ANCHOR(DataFile_Record, attrrec);

					attrrec.SetRecordSpec(rec->GetRecordSpec());

					if(longform)
					{
						attrrec.AddContentL((uint16) mod_id);
						attrrec.AddContentL((uint16) tag_id);
					}
					else
					{
						attrrec.AddContentL((uint8) mod_id);
						attrrec.AddContentL((uint8) tag_id);
					}
					attrrec.AddContentL(item->attributes[i].value.value);

					attrrec.WriteRecordL(rec);
				}
			}

			item = item->Suc();
		}
	}


	{
		URL_DynStrAttributeElement *item = (URL_DynStrAttributeElement *) dynamic_string_attrs.First();
		while(item)
		{
			uint32 i;
			for (i=0;i< ARRAY_SIZE(item->attributes); i++)
			{
				if(item->attributes[i].desc && item->attributes[i].desc->GetCachable())
				{
					uint32 mod_id = item->attributes[i].desc->GetModuleId();
					uint32 tag_id = item->attributes[i].desc->GetTagID();
					BOOL longform = (mod_id > 255 ||tag_id > 255);
					DataFile_Record attrrec(longform ? TAG_CACHE_DYNATTR_STRING_ITEM_Long : TAG_CACHE_DYNATTR_STRING_ITEM);
					ANCHOR(DataFile_Record, attrrec);

					attrrec.SetRecordSpec(rec->GetRecordSpec());

					if(longform)
					{
						attrrec.AddContentL((uint16) mod_id);
						attrrec.AddContentL((uint16) tag_id);
					}
					else
					{
						attrrec.AddContentL((uint8) mod_id);
						attrrec.AddContentL((uint8) tag_id);
					}
					attrrec.AddContentL(item->attributes[i].value);

					attrrec.WriteRecordL(rec);
				}
			}

			item = item->Suc();
		}
	}

	{
		URL_DynURLAttributeElement *item = (URL_DynURLAttributeElement *) dynamic_url_attrs.First();
		while(item)
		{
			uint32 i;
			for (i=0;i< ARRAY_SIZE(item->attributes); i++)
			{
				if(item->attributes[i].desc && item->attributes[i].desc->GetCachable())
				{
					uint32 mod_id = item->attributes[i].desc->GetModuleId();
					uint32 tag_id = item->attributes[i].desc->GetTagID();
					BOOL longform = (mod_id > 255 ||tag_id > 255);
					DataFile_Record attrrec(longform ? TAG_CACHE_DYNATTR_URL_ITEM_Long : TAG_CACHE_DYNATTR_URL_ITEM);
					ANCHOR(DataFile_Record, attrrec);

					attrrec.SetRecordSpec(rec->GetRecordSpec());

					if(longform)
					{
						attrrec.AddContentL((uint16) mod_id);
						attrrec.AddContentL((uint16) tag_id);
					}
					else
					{
						attrrec.AddContentL((uint8) mod_id);
						attrrec.AddContentL((uint8) tag_id);
					}

					OpString8 val;
					ANCHOR(OpString8, val);

					if(item->attributes[i].value.value.IsValid())
					{
						OpString8 val;
						ANCHOR(OpString8, val);
						item->attributes[i].value.value.GetAttributeL(URL::KName_With_Fragment_Escaped, val);
						attrrec.AddContentL(val);
					}
					else
						attrrec.AddContentL(item->attributes[i].value.value_str);

					attrrec.WriteRecordL(rec);
				}
			}

			item = item->Suc();
		}
	}
}

#ifdef CACHE_FAST_INDEX
void URL_DataStorage::WriteCacheDataL(DiskCacheEntry *entry, BOOL embedded)
{
	OP_PROBE7_L(OP_PROBE_URL_DATASTORAGE_WRITECACHEDATA_L);

	OP_ASSERT(entry);
	if(!entry)
		return;

	if(entry->GetTag() != TAG_CACHE_APPLICATION_CACHE_ENTRY && !embedded && GetAttribute(URL::KFileName).IsEmpty())
		return;

	GetAttributeL(DataStorage_rec_list, entry);
	GetAttributeL(DataStorage_rec_list2, entry);

	entry->SetContentSize(content_size);
	entry->SetLocalTime(local_time_loaded);

#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	if(GetNeverFlush())
		entry->SetNeverFlush(TRUE);
#endif

#ifdef URL_ENABLE_ASSOCIATED_FILES
	if(assoc_files)
	{
		entry->SetAssociatedFiles(assoc_files);
	}
#endif

	if(http_data)
	{
		HTTPCacheEntry *http=entry->GetHTTP();

		http->SetAgentID(GetAttribute(URL::KUsedUserAgentId));
		http->SetAgentVersionID(GetAttribute(URL::KUsedUserAgentVersion));

		GetAttributeL(DataStorage_http_rec_list, http);
		GetAttributeL(DataStorage_http_rec_list2, http);

		entry->SetExpiry(http_data->keepinfo.expires);

		if(entry->GetHTTP()->GetRefreshString()->HasContent())
		{
			http->SetRefreshInterval(GetAttribute(URL::KHTTP_Refresh_Interval));
		}

		OpStringC suggestedname(GetAttribute(URL::KHTTP_SuggestedFileName));

		if(suggestedname.HasContent())
		{
			OpString8 tempname;
			ANCHOR(OpString8, tempname);

			tempname.SetUTF8FromUTF16L(suggestedname.CStr());
			LEAVE_IF_ERROR(http->SetSuggestedName(&tempname));
		}

		HTTP_Link_Relations *relations = (HTTP_Link_Relations *) http_data->recvinfo.link_relations.First();
		while(relations)
		{
			if(relations->OriginalString().HasContent())
				LEAVE_IF_ERROR(http->AddLinkRelation(&relations->OriginalString()));
			relations = relations->Suc();
		}

		if(http_data->recvinfo.all_headers && !http_data->recvinfo.all_headers->Empty())
		{
			HeaderEntry *hdr = http_data->recvinfo.all_headers->First();

			while(hdr)
			{
				if(hdr->Name())
				{
					const OpStringC8 name = hdr->GetName();
					const OpStringC8 value = hdr->GetValue();
					LEAVE_IF_ERROR(http->AddHeader(&name, &value));
				}
				hdr = hdr->Suc();
			}
		}
	}

	if(storage->GetCacheType() == URL_CACHE_USERFILE)
	{
		entry->SetDownload(TRUE);
		entry->SetResumable((URL_Resumable_Status)GetAttribute(URL::KResumeSupported));

		if(GetAttribute(URL::KFTP_MDTM_Date).HasContent())
		{
			OpStringC8 temp = GetAttribute(URL::KFTP_MDTM_Date);
			LEAVE_IF_ERROR(entry->SetFTPModifiedDateTime(&temp));
		}
	}

	{
		OpStringC tempname1(GetAttribute(URL::KFileName));
		OpString8 tempname;
		ANCHOR(OpString8, tempname);

		tempname.SetUTF8FromUTF16L(tempname1.CStr());

#ifndef SEARCH_ENGINE_CACHE
		//rec->AddRecordL(TAG_CACHE_FILENAME, tempname);
		LEAVE_IF_ERROR(entry->SetFileName(&tempname));
#endif
#if CACHE_SMALL_FILES_SIZE>0
		// Embed the content
		if(storage->IsEmbedded())
		{
			LEAVE_IF_ERROR(storage->RetrieveEmbeddedContent(entry));
		}
#endif
	}

	// CORE-33950: if moved_to_url_name is empty, check moved_to_url
	// It could affect multiple responses, for example: HTTP_MULTIPLE_CHOICES(300), HTTP_MOVED (301), HTTP_FOUND(202), HTTP_SEE_OTHER(303), HTTP_USE_PROXY(305), HTTP_TEMPORARY_REDIRECT(307),
	// so I dediced to only check that it's different from 200, to avoid wasting some cycles on normal responses.
	if(http_data && http_data->recvinfo.response != 200)
	{
		if(!http_data->recvinfo.moved_to_url_name.IsEmpty())
			LEAVE_IF_ERROR(entry->GetHTTP()->SetMovedTo(&http_data->recvinfo.moved_to_url_name));
		else
		{
			OpString8 location;

			http_data->recvinfo.moved_to_url.GetAttribute(URL::KName_With_Fragment_Escaped, location);

			if(!location.IsEmpty())
				LEAVE_IF_ERROR(entry->GetHTTP()->SetMovedTo(&location));
		}
	}

	// Save th Dynamic attributes
	{// Flags
		URL_DynamicUIntAttributeDescriptor *desc = NULL;
		urlManager->GetFirstAttributeDescriptor(&desc);

		while(desc)
		{
			if(desc->GetCachable() && desc->GetIsFlag() && GetAttribute(desc->GetAttributeID()))
				LEAVE_IF_ERROR(entry->AddDynamicAttributeFromDataStorage(desc->GetModuleId(), desc->GetTagID(), TAG_CACHE_DYNATTR_FLAG_ITEM, NULL, 0));

			desc = desc->Suc();
		}
	}

	{// Integers
		URL_DynIntAttributeElement *item = (URL_DynIntAttributeElement *) dynamic_int_attrs.First();
		while(item)
		{
			uint32 i;
			for (i=0;i< ARRAY_SIZE(item->attributes); i++)
			{
				if(item->attributes[i].desc && item->attributes[i].desc->GetCachable())
					LEAVE_IF_ERROR(entry->AddDynamicAttributeFromDataStorage(item->attributes[i].desc->GetModuleId(), item->attributes[i].desc->GetTagID(), TAG_CACHE_DYNATTR_INT_ITEM, &item->attributes[i].value.value, 4));
			}

			item = item->Suc();
		}
	}


	{// Strings
		URL_DynStrAttributeElement *item = (URL_DynStrAttributeElement *) dynamic_string_attrs.First();
		while(item)
		{
			uint32 i;
			for (i=0;i< ARRAY_SIZE(item->attributes); i++)
			{
				if(item->attributes[i].desc && item->attributes[i].desc->GetCachable())
				{
					StringElement *el=&(item->attributes[i].value);

					if(!el) // Not really expected to happen...
						LEAVE_IF_ERROR(entry->AddDynamicAttributeFromDataStorage(item->attributes[i].desc->GetModuleId(), item->attributes[i].desc->GetTagID(), TAG_CACHE_DYNATTR_STRING_ITEM, NULL, 0));
					else
					{
						OpString8 &str=(OpString8 &)*el;

						LEAVE_IF_ERROR(entry->AddDynamicAttributeFromDataStorage(item->attributes[i].desc->GetModuleId(), item->attributes[i].desc->GetTagID(), TAG_CACHE_DYNATTR_STRING_ITEM, str.CStr(), str.Length()));
					}
				}
			}

			item = item->Suc();
		}
	}

	{// URLs
		URL_DynURLAttributeElement *item = (URL_DynURLAttributeElement *) dynamic_url_attrs.First();
		while(item)
		{
			uint32 i;
			for (i=0;i< ARRAY_SIZE(item->attributes); i++)
			{
				if(item->attributes[i].desc && item->attributes[i].desc->GetCachable())
				{
					OpString8 val;
					ANCHOR(OpString8, val);

					if(item->attributes[i].value.value.IsValid())
					{
						OpString8 val;
						ANCHOR(OpString8, val);
						item->attributes[i].value.value.GetAttributeL(URL::KName_With_Fragment_Escaped, val);
						LEAVE_IF_ERROR(entry->AddDynamicAttributeFromDataStorage(item->attributes[i].desc->GetModuleId(), item->attributes[i].desc->GetTagID(), TAG_CACHE_DYNATTR_URL_ITEM, val.CStr(), val.Length()));
					}
					else
						LEAVE_IF_ERROR(entry->AddDynamicAttributeFromDataStorage(item->attributes[i].desc->GetModuleId(), item->attributes[i].desc->GetTagID(), TAG_CACHE_DYNATTR_URL_ITEM, item->attributes[i].value.value_str.CStr(), item->attributes[i].value.value_str.Length()));
				}
			}

			item = item->Suc();
		}
	}
}
#endif // CACHE_FAST_INDEX
#endif // RAMCACHE_ONLY

unsigned long URL_Rep::HandleError(unsigned long error)
{
	if(storage)
		storage->HandleCallback(MSG_COMM_LOADING_FAILED,0,error);
	return error;
}

BOOL  URL_DataStorage::DetermineThirdParty(const URL &referer)
{
	BOOL thirdparty = FALSE;
	BOOL disable_cookies = FALSE;
	URLType reftype = (URLType) referer.GetAttribute(URL::KType);

	TRAPD(op_err, g_url_api->CheckCookiesReadL());
	OpStatus::Ignore(op_err);

	ServerName *this_servername = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
	ServerName *ref_servername = (ServerName *) referer.GetAttribute(URL::KServerName, (const void*) 0);

#ifdef _ASK_COOKIE
	COOKIE_MODES cmode_site = (ref_servername ? ref_servername->GetAcceptThirdPartyCookies(TRUE) :COOKIE_DEFAULT);
#endif
	COOKIE_MODES cmode_global = (COOKIE_MODES) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled);
	COOKIE_MODES cmode;

#ifdef _ASK_COOKIE
	cmode = cmode_site;

	if(cmode == COOKIE_NONE)
		disable_cookies = TRUE;
	else if(cmode != COOKIE_NO_THIRD_PARTY)
#endif
		cmode = cmode_global;

	URL tmpUrl = referer;
	if(tmpUrl.GetAttribute(URL::KIsThirdParty) || !this_servername)
		thirdparty = TRUE;

	if(tmpUrl.GetAttribute(URL::KDisableProcessCookies))
		thirdparty=disable_cookies = TRUE;

	do{
	if(!thirdparty && referer.IsValid() &&
		(cmode == COOKIE_NO_THIRD_PARTY ||
		cmode == COOKIE_SEND_NOT_ACCEPT_3P ||
		cmode == COOKIE_WARN_THIRD_PARTY))
	{
		if(/*referer.IsValid() && */reftype != URL_HTTP && reftype != URL_HTTPS)
			thirdparty = TRUE;
		else if(this_servername != ref_servername)
		{
			OP_ASSERT(this_servername);
			OP_ASSERT(ref_servername);

			ServerName *this_parent = this_servername->GetParentDomain();
			ServerName *ref_parent = ref_servername->GetParentDomain();

			ServerName *common_parent = this_servername->GetCommonDomain(ref_servername);
			if(common_parent)
			{
				if(common_parent->GetName().Compare("local") == 0 || // no need to check .local
					common_parent->GetNameComponentCount() < 2 // must have at least one internal dot to be checked for common cookies
					//  || url->GetAttribute(URL::KResolvedPort) != tmpUrl.GetAttribute(URL::KResolvedPort) // Must match resolved port (and implicitly, scheme) // Nope, cause problems
					)
				{
					common_parent = NULL;
				}
#if defined PUBSUFFIX_ENABLED
				else
				{
					ServerName::DomainType dom_type = common_parent->GetDomainTypeASync();

					if(dom_type != ServerName::DOMAIN_UNKNOWN)
					{
						if(dom_type >= ServerName::DOMAIN_VALID_START &&
							dom_type <= ServerName::DOMAIN_VALID_END)
						{
							thirdparty = (dom_type != ServerName::DOMAIN_NORMAL);
							break;
						}
						else if(dom_type == ServerName::DOMAIN_WAIT_FOR_UPDATE)
						{
							return FALSE;
						}
					}
				}
#endif
			}

#ifndef PUBSUFFIX_ENABLED
			BOOL checked_pubdom = FALSE;
			BOOL had_enough_data = FALSE;
			if (cmode == COOKIE_SEND_NOT_ACCEPT_3P && common_parent)
			{
				BOOL is_public_domain = TRUE;
				checked_pubdom = TRUE;
				if(OpStatus::IsError(g_secman_instance->IsPublicDomain(common_parent->UniName(), is_public_domain, &had_enough_data)))
					thirdparty = TRUE;
				else if (is_public_domain && had_enough_data)
					thirdparty = TRUE; // Don't allow setting domain to com or co.uk or similar
			}

			if(checked_pubdom && had_enough_data)
			{
				// thirdparty decided
			}
			else if(common_parent)
			{
				OpString8 test_url_name;
				const char *temp_cookies = NULL;
				OpStringC8 temp_path = url->GetAttribute(URL::KPath);

				if(OpStatus::IsSuccess(test_url_name.SetConcat(url->GetAttribute(URL::KProtocolName), "://", common_parent->GetName())) &&
					OpStatus::IsSuccess(test_url_name.AppendFormat(":%d%s",url->GetAttribute(URL::KResolvedPort),
					(temp_path.HasContent() ? temp_path.CStr() : "/") )))
				{
					// Check prot:
					URL test_url = g_url_api->GetURL(test_url_name, url->GetContextId());
					int version = INT_MAX;
					int max_version = 0;
					BOOL have_pass = FALSE, have_auth = FALSE;

					if(test_url.IsValid())
						temp_cookies = g_url_api->GetCookiesL(test_url, version, max_version, FALSE, FALSE,have_pass, have_auth);

				}
				if(!temp_cookies || op_strlen(temp_cookies) == 0)
					common_parent = NULL;
			}

			if(common_parent)
			{
				// OK, since the servers share cookies, or already decided
			}
			else
#endif // PUBSUFFIX_ENABLED
				if(this_parent == ref_parent) // same parent (or no parent => ".local" domain)
			{
				if(this_parent && this_parent->GetParentDomain() == NULL)
					thirdparty = TRUE;

				// If the parent domain does not have a parent the referrer is a thirdparty
				// Question: What about example1.co.uk and example2.co.uk ? They will be able to bypass the thirdparty check.
			}
			else if(this_parent == NULL ||  ref_parent == NULL)
			{
				// One of the domains is in the ".local" sone
				thirdparty = TRUE;
			}
			else if(ref_parent == this_servername)
			{
				// this server is the parent domain of the referrer
				if(tmpUrl.GetAttribute(URL::KIsThirdPartyReach))
					thirdparty = TRUE;
				else
					SetAttribute(URL::KIsThirdPartyReach,TRUE);
			}
			else if(this_parent == ref_servername || this_parent->GetCommonDomain(ref_parent) == ref_parent)
			{
				// Thirdparty OK
			}
			else
			{
				thirdparty = TRUE;
			}
		}
	}
	}while(0);

	if(thirdparty)
	{
		if(this_servername == NULL || cmode == COOKIE_NO_THIRD_PARTY)
			disable_cookies = TRUE;
		else if(cmode == COOKIE_WARN_THIRD_PARTY)
		{
			COOKIE_MODES dmode = this_servername->GetAcceptThirdPartyCookies(TRUE);
			if(dmode == COOKIE_NO_THIRD_PARTY)
				disable_cookies = TRUE;
		}
	}


	SetAttribute(URL::KIsThirdParty,thirdparty);
	SetAttribute(URL::KDisableProcessCookies,disable_cookies);

	return TRUE;
}

void URL_DataStorage::UnsetListCallbacks()
{
	MsgHndlList::Iterator itr;
	for(itr = mh_list->Begin(); itr != mh_list->End(); ++itr)
	{
		MessageHandler* mh = (*itr)->GetMessageHandler();
		if(mh)
			mh->UnsetCallBacks(this);
	}
}

void URL_DataStorage::SendMessages(MessageHandler *mh, BOOL set_failure, OpMessage msg, MH_PARAM_2 par2)
{
	if(set_failure)
		SetAttribute(URL::KLoadStatus,URL_LOADING_FAILURE);

	if(mh)
	{
		mh->PostMessage(MSG_URL_LOADING_FAILED, url->GetID(), par2);
	}

	mh_list->BroadcastMessage(msg, url->GetID(), par2, MH_LIST_ALL);
}

/*
void URL_DataStorage::SetContentSize(unsigned long cs)
{
	content_size = cs;
}
*/

void URL_DataStorage::DeleteLoading()
{
	SComm *temp = loading;
	loading = NULL;
	SComm::SafeDestruction(temp);
}

void URL_DataStorage::CopyAllHeadersL(HeaderList& header_list_copy) const
{
	if (http_data && http_data->recvinfo.all_headers)
	{
		http_data->recvinfo.all_headers->DuplicateHeadersL(&header_list_copy, 0, NULL);
	}
}

#ifdef _OPERA_DEBUG_DOC_
void URL_DataStorage::GetMemUsed(DebugUrlMemory &debug)
{
	debug.memory_loaded += sizeof(*this);
	//debug.memory_loaded += this->autodetect_charset.Length();
	//debug.memory_loaded += mime_charset.Length();
	debug.memory_loaded += content_type_string.Length();
	debug.memory_loaded += UNICODE_SIZE(security_text.Length());
	debug.memory_loaded += UNICODE_SIZE(internal_error_message.Length());

	if(http_data)
		debug.memory_loaded += http_data->GetMemUsed();
	// Other protocol data // mailto not included


	/*
	if(loading)
		debug.memory_loaded += loading->GetMemUsed();
		*/
	if(old_storage)
		old_storage->GetMemUsed(debug);
	if(storage)
		storage->GetMemUsed(debug);
}
#endif

void URL_DataStorage::BroadcastMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, int flags)
{
	if(msg != MSG_URL_DATA_LOADED || !storage || storage->GetIsMultipart() || storage->Empty())
		mh_list->BroadcastMessage(msg, par1, par2, flags);
	else
	{
		MsgHndlList::Iterator itr;
		for(itr = mh_list->Begin(); itr != mh_list->End(); ++itr)
		{
			MessageHandler *mh = (*itr)->GetMessageHandler();

			OP_ASSERT(mh);
			if (!mh)
				continue;

			URL_DataDescriptor *dd = storage->First();
			while(dd)
			{
				if(dd->GetMessageHandler() == mh)
					break;

				dd = dd->Suc();
			}

			if(dd)
			{
				if(!dd->PostedMessage())
					dd->PostMessage(msg, par1, par2);
			}
			else
				mh->PostMessage(msg, par1, par2);
		}

	}
}

OpFileLength URL_DataStorage::GetNextRangeStart()
{
	OP_ASSERT(http_data);
	
	if(http_data)
		return HTTPRange::ConvertFileLengthNoneToZero(http_data->sendinfo.range_start) + range_offset;
	
	return 0;
}

OP_STATUS URL_DataStorage::AddMessageHandler(MessageHandler *mh, BOOL always_new, BOOL first)
{
	OP_ASSERT(mh);
	RETURN_IF_ERROR(mh_list->Add(mh, FALSE, FALSE, FALSE, always_new, first));

#ifdef _ENABLE_AUTHENTICATE
	if (!urlManager->OnMessageHandlerAdded(url))
		return OpStatus::ERR_NO_MEMORY;
#endif // _ENABLE_AUTHENTICATE

	return OpStatus::OK;
}

void URL_DataStorage::StartIdlePoll()
{
	uint32 timeout = GetAttribute(URL::KTimeoutPollIdle);
	if (timeout)
	{
		uint32 temp = GetAttribute(URL::KTimeoutMinimumBeforeIdleCheck);
		if(temp)
			timeout = temp;

		g_main_message_handler->RemoveDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 1);
		g_main_message_handler->PostDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 1, timeout*1000);
		if(!g_main_message_handler->HasCallBack(this, MSG_URL_TIMEOUT, url->GetID()))
			g_main_message_handler->SetCallBack(this, MSG_URL_TIMEOUT, url->GetID());
	}
}

void URL_DataStorage::StartTimeout()
{
	BOOL set_callback = FALSE;
	uint32 timeout = GetAttribute(URL::KTimeoutMaximum);
	if(timeout)
	{
		g_main_message_handler->RemoveDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 0);
		g_main_message_handler->PostDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 0, timeout*1000);
		set_callback = TRUE;
	}

	StartIdlePoll();

	if(set_callback && !g_main_message_handler->HasCallBack(this, MSG_URL_TIMEOUT, url->GetID()))
		g_main_message_handler->SetCallBack(this, MSG_URL_TIMEOUT, url->GetID());
	SetAttribute(URL::KTimeoutSeenDataSinceLastPoll_INTERNAL, FALSE);
}

void URL_DataStorage::StartTimeout(BOOL response_timeout)
{
	uint32 timeout = response_timeout ? GetAttribute(URL::KResponseReceiveIdleTimeoutValue) : GetAttribute(URL::KRequestSendTimeoutValue);
	if(timeout)
	{
		g_main_message_handler->RemoveDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 0);
		g_main_message_handler->PostDelayedMessage(MSG_URL_TIMEOUT, url->GetID(), 0, timeout*1000);
		if(!g_main_message_handler->HasCallBack(this, MSG_URL_TIMEOUT, url->GetID()))
			g_main_message_handler->SetCallBack(this, MSG_URL_TIMEOUT, url->GetID());
	}
	SetAttribute(URL::KTimeoutSeenDataSinceLastPoll_INTERNAL, FALSE);
}
