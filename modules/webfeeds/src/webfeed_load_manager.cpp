// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/src/webfeed_load_manager.h"
#include "modules/webfeeds/src/webfeeds_api_impl.h"
#include "modules/webfeeds/src/webfeedutil.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/url/tools/url_util.h"  // for custom URL headers

#ifdef DOC_PREFETCH_API
# include "modules/doc/prefetch.h"
#endif // DOC_PREFETCH_API

#define g_feed_load_manager (((WebFeedsAPI_impl*)g_webfeeds_api)->GetLoadManager())

#ifndef MSG_FEED_LOADING_TIMEOUT
# define MSG_FEED_LOADING_TIMEOUT MSG_COMM_TIMEOUT
#endif

const int FEED_LOADING_TIMEOUT = 60000;
#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define WF_ERRSTR(p,x) Str::##p##_##x
#else
#define WF_ERRSTR(p,x) x
#endif


WebFeedLoadManager::LoadingFeed::LoadingFeed()
	: m_feed(NULL), m_parser(NULL), m_feed_id(0), m_last_modified(0), m_return_unchanged_feeds(FALSE)
{
}

WebFeedLoadManager::LoadingFeed::LoadingFeed(URL& feed_url, WebFeed* feed, OpFeedListener* listener)
	: m_feed_url(feed_url), m_feed(feed), m_parser(NULL), m_feed_id(0), m_last_modified(0), m_return_unchanged_feeds(FALSE)
{
	if (m_feed)
	{
		m_feed->IncRef();
		m_feed_id = m_feed->GetId();
	}
	AddListener(listener);
}

WebFeedLoadManager::LoadingFeed::LoadingFeed(URL& url, OpFeed::FeedId id, OpFeedListener* listener) :
	m_feed_url(url), m_feed(NULL), m_parser(NULL), m_feed_id(id), m_return_unchanged_feeds(FALSE)
{
	AddListener(listener);
}

OP_STATUS WebFeedLoadManager::LoadingFeed::Init(BOOL return_unchanged_feeds, time_t last_modified, const char* etag)
{
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_FEED_LOADING_TIMEOUT, 0)); 

	m_return_unchanged_feeds = return_unchanged_feeds;
	m_last_modified = last_modified;

	if (etag)
		RETURN_IF_ERROR(m_etag.Set(etag));

	return OpStatus::OK;
}

WebFeedLoadManager::LoadingFeed::~LoadingFeed()
{
	if (m_feed)
		m_feed->DecRef();
	UnRegisterCallbacks();
	
	OP_DELETE(m_parser);
}

WebFeed* WebFeedLoadManager::LoadingFeed::GetFeed()
{
	if (m_feed)
		return m_feed;

	// try loading feed from disk if we don't have it yet
	if (m_feed_id)
	{
		m_feed = (WebFeed*)g_webfeeds_api->GetFeedById(m_feed_id);
		if (m_feed)
			m_feed->IncRef();
	}
	
	return m_feed;
}
void WebFeedLoadManager::LoadingFeed::ParsingStarted()
{
	g_main_message_handler->UnsetCallBack(this, MSG_FEED_LOADING_TIMEOUT);
}

void WebFeedLoadManager::LoadingFeed::ParsingDone(WebFeed* feed, OpFeed::FeedStatus status)
{
	m_parser = NULL;  // parser deletes itself

	if (feed && feed != m_feed)
	{
		if (m_feed)
			m_feed->DecRef();
		m_feed = feed;
		m_feed->IncRef();

		((WebFeedsAPI_impl*)g_webfeeds_api)->AddFeed(m_feed);
	}

	if (status == OpFeed::STATUS_OK)
		if (feed)
			feed->SetLastUpdated(OpDate::GetCurrentUTCTime());
		else  // If we didn't get a feed then parsing didn't really go ok, so change status
			status = OpFeed::STATUS_PARSING_ERROR;

	if (feed)
	{
		time_t modified =0;
		m_feed_url.GetAttribute(URL::KVLastModified, &modified, TRUE);
		feed->SetHttpLastModified(modified);
		OpString8 tag;
		if (OpStatus::IsSuccess(m_feed_url.GetAttribute(URL::KHTTP_EntityTag, tag, TRUE)))
			OpStatus::Ignore(feed->SetHttpETag(tag.CStr()));
	}
	
	// Not really a parsing error if we get a HTTP error, so return LOADING_FAILED instead
	if (status == OpFeed::STATUS_PARSING_ERROR
		&& (m_feed_url.Type() == URL_HTTP || m_feed_url.Type() == URL_HTTPS))
	{
		int http_response = m_feed_url.GetAttribute(URL::KHTTP_Response_Code, TRUE);
		if (http_response != HTTP_OK && http_response != HTTP_NOT_MODIFIED)
			status = OpFeed::STATUS_LOADING_ERROR;
	}

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT // only load favicons if we have disk to save them to
	if (status != OpFeed::STATUS_OK || m_feed->GetStub()->HasInternalFeedIcon() || !LoadIcon(feed)) // we're done with this feed
#endif
	{
		NotifyListeners(feed, status);
		LoadingFinished();
	}
}

void WebFeedLoadManager::LoadingFeed::OnEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus status, BOOL new_entry)
{
#ifdef DOC_PREFETCH_API
	if (new_entry)
	{
		OpFeed* feed = entry->GetFeed();
		if (feed->PrefetchPrimaryWhenNewEntries())
		{
			RAISE_AND_RETURN_VOID_IF_ERROR(((WebFeedEntry*)entry)->PrefetchPrimaryLink());
		}
	}
#endif

	NotifyListeners(entry, status, new_entry);
}

void WebFeedLoadManager::LoadingFeed::NotifyListeners(WebFeed* feed, OpFeed::FeedStatus status)
{
	if (feed)
		feed->SetStatus(status);

#ifdef WEBFEEDS_REFRESH_FEED_WINDOWS
	g_webfeeds_api_impl->UpdateFeedWindow(m_feed);
#endif
	UINT i;
	OpVector<OpFeedListener>* omnis = g_webfeeds_api_impl->GetAllOmniscientListeners();
	for (i=0; i<omnis->GetCount(); i++)
		omnis->Get(i)->OnFeedLoaded(feed, status);
	for (i=0; i<m_listeners.GetCount(); i++)
		m_listeners.Get(i)->OnFeedLoaded(feed, status);
}

void WebFeedLoadManager::LoadingFeed::NotifyListeners(OpFeedEntry* entry, OpFeedEntry::EntryStatus status, BOOL new_entry)
{
#ifdef WEBFEEDS_REFRESH_FEED_WINDOWS
	g_webfeeds_api_impl->UpdateFeedWindow(m_feed);
#endif

	UINT i;
	OpVector<OpFeedListener>* omnis = g_webfeeds_api_impl->GetAllOmniscientListeners();
	for (i=0; i<omnis->GetCount(); i++)
	{
		omnis->Get(i)->OnEntryLoaded(entry, status);
		if (new_entry)
			omnis->Get(i)->OnNewEntryLoaded(entry, status);
	}
	for (i=0; i<m_listeners.GetCount(); i++)
	{
		m_listeners.Get(i)->OnEntryLoaded(entry, status);
		if (new_entry)
			m_listeners.Get(i)->OnNewEntryLoaded(entry, status);
	}
}

void WebFeedLoadManager::LoadingFeed::LoadingFinished()
{
	// Notify the others so they can clean up their data structures, we're done loading this feed now
	g_feed_load_manager->RemoveLoadingFeed(this);
	g_webfeeds_api_impl->FeedFinished(m_feed_id);
	OP_DELETE(this);
}

OP_STATUS WebFeedLoadManager::LoadingFeed::AddListener(OpFeedListener* listener)
{
	if (!listener || (m_listeners.Find(listener) != -1)) // No listener or listener already registered
		return OpStatus::OK;

	return m_listeners.Add(listener);
}

OP_STATUS WebFeedLoadManager::LoadingFeed::RemoveListener(OpFeedListener* listener)
{
	RETURN_IF_MEMORY_ERROR(m_listeners.RemoveByItem(listener));

	return OpStatus::OK;
}

OP_STATUS WebFeedLoadManager::LoadingFeed::StartLoading()
{
	OP_ASSERT(!m_parser);

	OpStackAutoPtr<WebFeedParser> parser(OP_NEW(WebFeedParser, (this, TRUE)));

	if (!parser.get())
		return OpStatus::ERR_NO_MEMORY;
    {
        OpString dummy;
        RETURN_IF_ERROR(parser->Init(dummy));
    }

	WebFeed* feed = GetFeed();
	// Clean up feed for old entries
	if (feed)
		feed->CheckFeedForExpiredEntries();

	URL load_url = m_feed_url;

#if 0 // disabled until URL enables setting LastModified and EntityTag - see bug #237593
	if (m_last_modified != 0 || m_etag.HasContent())
	{
		URL_Custom_Header header_item;
		load_url = g_url_api->MakeUniqueCopy(m_feed_url);

		if (m_last_modified != 0)
			RETURN_IF_ERROR(load_url.SetAttribute(URL::KVLastModified, (void*)m_last_modified));

		if (m_etag.HasContent())
		{
			RETURN_IF_ERROR(load_url.SetAttribute(URL::KHTTP_EntityTag, m_etag));
/*			This code is implemented in URL, but not ideal
			RETURN_IF_ERROR(header_item.name.Set("If-None-Match"));
			RETURN_IF_ERROR(header_item.value.Set(m_etag));
			RETURN_IF_ERROR(load_url.SetAttribute(URL::KAddHTTPHeader, &header_item));
*/
		}
	}
#endif // 0
	
	m_parser = parser.release();

	g_feed_load_manager->AddLoadingFeed(this);
	OP_STATUS status = m_parser->LoadFeed(load_url, feed);
	if (OpStatus::IsError(status))
	{
		NotifyListeners(feed, OpFeed::STATUS_PARSING_ERROR);
		OP_DELETE(m_parser);
		m_parser = NULL;
		g_feed_load_manager->RemoveLoadingFeed(this);
	}
	else
	{
		// If we haven't received any parsing data within one minute, assume something is wrong and abort
		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_FEED_LOADING_TIMEOUT, m_icon_url.Id()));
		g_main_message_handler->PostDelayedMessage(MSG_FEED_LOADING_TIMEOUT, m_feed_url.Id(), 0, FEED_LOADING_TIMEOUT);
	}

	return status;
}

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
BOOL WebFeedLoadManager::LoadingFeed::LoadIcon(WebFeed* feed)
{
	if (!feed)
		return FALSE;

	m_icon_url = g_url_api->GetURL(feed->GetIcon());
	if (m_icon_url.Type() == URL_FILE || m_icon_url.Type() == URL_DATA)  // no need to download these
		return FALSE;

	if (m_icon_url.IsEmpty())
		m_icon_url = g_url_api->GetURL(feed->GetURL(), "/favicon.ico");
	if (m_icon_url.IsEmpty())
		return FALSE;		

	// Register URL callbacks and load URL
	OpMessage url_messages[] = { MSG_URL_DATA_LOADED, MSG_URL_LOADING_FAILED };
	OP_STATUS status = g_main_message_handler->SetCallBackList(this, m_icon_url.Id(), url_messages, ARRAY_SIZE(url_messages));
	if (status == OpStatus::ERR_NO_MEMORY)
	{
		g_memory_manager->RaiseCondition(status);
		return FALSE;
	}

	CommState state = m_icon_url.Load(g_main_message_handler, feed->GetURL());
	if (state == COMM_LOADING)
	{
		// If the favicon hasn't loaded in half a second, then skip it.  We don't care that much,
		// and it delays the rest of loading too much if we wait long for a icon which is slow loading.
		g_main_message_handler->PostDelayedMessage(MSG_URL_LOADING_FAILED, m_icon_url.Id(), WF_ERRSTR(SI,ERR_HTTP_TIMEOUT), 500);
		return TRUE; // Loading has started
	}

	return FALSE;
}
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

OP_STATUS WebFeedLoadManager::LoadingFeed::HandleDataLoaded()
{
	OpStackAutoPtr<URL_DataDescriptor> url_dd(m_icon_url.GetDescriptor(NULL, TRUE));
	if (!url_dd.get())
		return OpStatus::ERR;

	if (!url_dd->FinishedLoading())
		return OpStatus::OK;

	int http_response = m_icon_url.GetAttribute(URL::KHTTP_Response_Code);
	OpFileLength content_size;
	m_icon_url.GetAttribute(URL::KContentSize, &content_size, TRUE);
	if (http_response != HTTP_OK && http_response != HTTP_NOT_MODIFIED)
		return OpStatus::ERR;

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	if (content_size <= UINT_MAX && content_size < (UINT)(g_webfeeds_api_impl->GetFeedDiskSpaceQuota() * 0.20))
	{
		// save icon as id.extension in feeds folder
		OpString filename, extension, file_url, full_path;

		RETURN_IF_LEAVE(m_icon_url.GetAttributeL(URL::KSuggestedFileNameExtension_L, extension, TRUE));
		if (extension.IsEmpty())
			RETURN_IF_ERROR(extension.Set("ico"));
		RETURN_IF_ERROR(filename.AppendFormat(UNI_L("%08x.%s"), m_feed->GetId(), extension.CStr()));
	
		OpString folder;
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_WEBFEEDS_FOLDER, folder));
		OP_ASSERT(folder.CStr());
		if (!folder.CStr())
			return OpStatus::ERR;

		RETURN_IF_ERROR(full_path.Set(folder.CStr()));
		RETURN_IF_ERROR(full_path.Append(filename));
		RETURN_IF_ERROR(m_feed->GetStub()->SetInternalFeedIconFilename(filename.CStr()));
	
		if (m_icon_url.PrepareForViewing(TRUE, TRUE, FALSE) != 0)
			return OpStatus::ERR;
		if (!m_icon_url.SaveAsFile(full_path.CStr()))
			return OpStatus::ERR;
	
		m_feed->GetStub()->SetIconDiskSpace((UINT)content_size);
	}
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

	NotifyListeners(m_feed, OpFeed::STATUS_OK);
	LoadingFinished();

	return OpStatus::OK;
}

void WebFeedLoadManager::LoadingFeed::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OpString8 last_modified;
	switch (msg)
	{
	case MSG_FEED_LOADING_TIMEOUT:
		NotifyListeners(m_feed, OpFeed::STATUS_SERVER_TIMEOUT);
		LoadingFinished();
		break;
	case MSG_URL_DATA_LOADED:
		{
			OP_ASSERT(m_icon_url.Id() == (URL_ID)par1);
			OP_STATUS status = HandleDataLoaded();
			if (status == OpStatus::ERR_NO_MEMORY)
			{
				NotifyListeners(m_feed, OpFeed::STATUS_OOM);
				LoadingFinished();
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			}
			else if (OpStatus::IsError(status))
			{
				NotifyListeners(m_feed, OpFeed::STATUS_OK);  // an icon not loading is no problem
				LoadingFinished();
			}
		}
		break;
	case MSG_URL_LOADING_FAILED:  // an icon not loading is no problem
		NotifyListeners(m_feed, OpFeed::STATUS_OK);
		LoadingFinished();
		break;
	default:
		OP_ASSERT(!"We didn't register for any more messages, something's wrong!");
	}
	
	return;
}


void WebFeedLoadManager::LoadingFeed::UnRegisterCallbacks()
{
	OP_ASSERT(g_main_message_handler);
	g_main_message_handler->UnsetCallBacks(this);
}


/*******************************************
 * WebFeedLoadManager
 ******************************************/

WebFeedLoadManager::WebFeedLoadManager() :
	m_max_loading_feeds(WEBFEEDS_MAX_SIMULTANEOUS_LOADS)
{
}

WebFeedLoadManager::~WebFeedLoadManager()
{
}

OP_STATUS WebFeedLoadManager::Init()
{
	return OpStatus::OK;
}

OP_STATUS WebFeedLoadManager::UpdateFeed(URL& url, OpFeed::FeedId id, OpFeedListener* listener, BOOL return_unchanged, time_t last_modified, const char* etag)
{
	LoadingFeed* loading = IsLoadingOrQueued(url);
	if (loading)   // we are already loading this feed, just attach the 
	               // listener and it will be notified when it's done
		return loading->AddListener(listener);

	// have to start loading ourselves
	loading = OP_NEW(LoadingFeed, (url, id, listener));
	if (!loading)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = loading->Init(return_unchanged, last_modified, etag);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(loading);
		return status;
	}

	return LoadFeed(loading);
}

OP_STATUS WebFeedLoadManager::LoadFeed(WebFeed* feed, URL& feed_url, OpFeedListener* listener, BOOL return_unchanged, time_t last_modified, const char* etag)
{
	LoadingFeed* loading = IsLoadingOrQueued(feed_url);
	if (loading)   // we are already loading this feed, just attach the 
	               // listener and it will be notified when it's done
		return loading->AddListener(listener);

	// have to start loading ourselves
	loading = OP_NEW(LoadingFeed, (feed_url, feed, listener));
	if (!loading)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = loading->Init(return_unchanged, last_modified, etag);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(loading);
		return status;
	}

	return LoadFeed(loading);
}

OP_STATUS WebFeedLoadManager::LoadFeed(LoadingFeed* loading)  // takes ownership of loading object
{
	// If we're already loading the maximum number of feeds, put this feed into the waiting list
	if (m_loading_feeds.Cardinal() >= m_max_loading_feeds)
	{
		loading->Into(&m_waiting_feeds);
		return OpStatus::OK;
	}

	OP_STATUS status = loading->StartLoading();

	if (OpStatus::IsError(status))
		OP_DELETE(loading);
	return status;
}

OP_STATUS WebFeedLoadManager::AbortLoading(URL& feed_url)
{
	// remove the waiting ones first, or they will be moved to
	// the loading list ones the feeds there are removed
	LoadingFeed* queues[] = { (LoadingFeed*)m_waiting_feeds.First(),
							  (LoadingFeed*)m_loading_feeds.First() };

	for (UINT i = 0; i < ARRAY_SIZE(queues); i++)
	{
		LoadingFeed* lf = queues[i];
		while (lf)
		{
			LoadingFeed* next = (LoadingFeed*)lf->Suc();
			if (lf->GetURL() == feed_url)
			{
				lf->NotifyListeners(NULL, OpFeed::STATUS_ABORTED);
				
				if (i == 0) // waiting feed
				{
					lf->Out();
					OP_DELETE(lf);
				}
				else // loading feed
					lf->LoadingFinished();
				break;
			}
			lf = next;
		}
	}
	
	return OpStatus::OK;
}

OP_STATUS WebFeedLoadManager::AbortAllFeedLoading(BOOL stop_updating_only)
{
	// remove the waiting ones first, or they will be moved to
	// the loading list ones the feeds there are removed
	LoadingFeed* queues[] = { (LoadingFeed*)m_waiting_feeds.First(),
							  (LoadingFeed*)m_loading_feeds.First() };

	for (UINT i = 0; i < ARRAY_SIZE(queues); i++)
	{
		LoadingFeed* lf = queues[i];
		while (lf)
		{
			LoadingFeed* next = (LoadingFeed*)lf->Suc();
			if (!stop_updating_only || lf->IsUpdatingFeed())
			{
				lf->NotifyListeners(NULL, OpFeed::STATUS_ABORTED);
				
				if (i == 0) // waiting feed
				{
					lf->Out();
					OP_DELETE(lf);
				}
				else // loading feed
					lf->LoadingFinished();
			}
			lf = next;
		}
	}
	
	return OpStatus::OK;	
}

/* Called by LoadingFeed when it starts loading */
void WebFeedLoadManager::AddLoadingFeed(LoadingFeed* lf)
{
	if (!m_loading_feeds.HasLink(lf))
	{
		OP_ASSERT(m_loading_feeds.Cardinal() < m_max_loading_feeds);
		lf->Into(&m_loading_feeds);
	}
}

/* Called by LoadingFeed when it has finished loading */
OP_STATUS WebFeedLoadManager::RemoveLoadingFeed(LoadingFeed* lf)
{
	lf->Out();
	
	OP_ASSERT(m_loading_feeds.Cardinal() < m_max_loading_feeds);

	// Get next feed in line waiting for load	
	LoadingFeed* next = (LoadingFeed*)m_waiting_feeds.First();
	if (!next)
		return OpStatus::OK;

	next->Out();
	return next->StartLoading();
}

OP_STATUS WebFeedLoadManager::RemoveListener(OpFeedListener* listener)
{
	LoadingFeed* lf = NULL;
	for (lf = (LoadingFeed*)m_loading_feeds.First(); lf; lf = (LoadingFeed*)lf->Suc())
		lf->RemoveListener(listener);
	for (lf = (LoadingFeed*)m_waiting_feeds.First(); lf; lf = (LoadingFeed*)lf->Suc())
		lf->RemoveListener(listener);

	return OpStatus::OK;
}

WebFeedLoadManager::LoadingFeed* WebFeedLoadManager::IsLoadingOrQueued(URL& feed_url)
{
	LoadingFeed* queues[] = { (LoadingFeed*)m_waiting_feeds.First(),
							  (LoadingFeed*)m_loading_feeds.First() };

	for (UINT i = 0; i < ARRAY_SIZE(queues); i++)
	{
		for (LoadingFeed* lf = queues[i]; lf; lf = (LoadingFeed*)lf->Suc())
			if (lf->GetURL() == feed_url)
				return lf;
	}
	
	return NULL;
}
#endif // WEBFEEDS_BACKEND_SUPPORT
