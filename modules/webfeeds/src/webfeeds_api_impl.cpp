// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.


#include "core/pch.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/src/webfeeds_api_impl.h"
#include "modules/webfeeds/src/webfeed.h"
#include "modules/webfeeds/src/webfeedstorage.h"
#include "modules/webfeeds/src/webfeedutil.h"
#include "modules/webfeeds/src/webfeed_load_manager.h"
#include "modules/webfeeds/src/webfeed_reader_manager.h"

#include "modules/about/operafeeds.h"
#include "modules/doc/frm_doc.h"           // finding form elements
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/forms/src/formiterator.h"
#include "modules/hardcore/mh/constant.h"  // Message constants
#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/logdoc.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/url/url_man.h"

#ifdef SKIN_SUPPORT
# include "modules/skin/OpSkinManager.h"
#endif

//#define DEBUG_FEEDS_UPDATING
#define WEBFEEDS_UPDATE_FEEDS

const UINT msPerMin = 60000;


/* static */ BOOL OpFeed::IsFailureStatus(FeedStatus status)
{
	switch (status)
	{
	case STATUS_OK:
	case STATUS_REFRESH_POSTPONED:
	case STATUS_NOT_MODIFIED:
		return FALSE;
	default:
		return TRUE;
	}
}

/*******************************
* WebFeedFormIterator
*******************************/

OP_STATUS WebFeedFormIterator::Init(FramesDocument* frm_doc, const uni_char* id)
{
	if (!frm_doc)
		return OpStatus::ERR_NULL_POINTER;

	LogicalDocument* logdoc = frm_doc->GetLogicalDocument();
	if (!logdoc)
		return OpStatus::ERR_NULL_POINTER;

	HTML_Element* root = logdoc->GetDocRoot();
	if (!root)
		return OpStatus::ERR_NULL_POINTER;

	// Get form
	NamedElementsIterator it;
	logdoc->SearchNamedElements(it, root, id, TRUE, FALSE);
	HTML_Element* form = it.GetNextElement();
	if (!form)
		return OpStatus::ERR;

	m_form_iterator = OP_NEW(FormIterator, (frm_doc, form));
	if (!m_form_iterator.get())
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}


HTML_Element* WebFeedFormIterator::GetNext()
{
	return m_form_iterator->GetNext();
}

/*******************************
* WebFeedsAPI_impl
*******************************/

WebFeedsAPI_impl::WebFeedsAPI_impl() : m_next_free_feedid(1),
#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	m_store_space_factor(0.9), m_disk_space_used(0),
#endif
	m_number_updating_feeds(0), m_max_size(0), m_max_age(0), m_max_entries(0),
	m_update_interval(60), m_do_automatic_updates(FALSE), m_next_update(0.0),
	m_show_images(TRUE), m_skin_feed_icon_exists(FALSE), m_feeds(NULL)
{
#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	for (UINT i=0; i < ARRAY_SIZE(m_feeds_memory_cache); i++)
		m_feeds_memory_cache[i] = NULL;
#endif // WEBFEEDS_SAVED_STORE_SUPPORT
}

void WebFeedsAPI_impl::HashDelete(const void*, void* data)
{
	WebFeedStub* stub = (WebFeedStub*)data;
	if (stub)
		stub->StoreDestroyed();
}

WebFeedsAPI_impl::~WebFeedsAPI_impl()
{
#ifdef WEBFEEDS_AUTOSAVE
	// ignore status, if we can't save at this point there really isn't
	// much we can do about it
	TRAPD(status, GetStorage()->SaveStoreL(this));
#endif

	UnRegisterCallbacks();
	m_feeds->ForEach(HashDelete);

	OP_DELETE(m_feeds);
}

OP_STATUS WebFeedsAPI_impl::Init()
{
	mh = g_main_message_handler;
	OP_ASSERT(mh);
	RegisterCallbacks();
	// TODO: should we post a delayed update of all feeds at startup?
//	mh->PostDelayedMessage(MSG_WEBFEEDS_UPDATE_FEEDS, 0, 0, 1000);

	OP_ASSERT(!m_feeds);
	m_feeds = OP_NEW(OpHashTable, ());
	if (!m_feeds)
		return OpStatus::ERR_NO_MEMORY;

	m_load_manager = OP_NEW(WebFeedLoadManager, ());
	if (!m_load_manager.get())
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(m_load_manager->Init());

#ifdef WEBFEEDS_EXTERNAL_READERS
	m_external_reader_manager = OP_NEW(WebFeedReaderManager, ());
	if (!m_external_reader_manager.get())
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(m_external_reader_manager->Init());
#endif // WEBFEEDS_EXTERNAL_READERS

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	m_storage = OP_NEW(WebFeedStorage, ());
	if (!m_storage.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_MEMORY_ERROR(m_storage->LoadStore(this));

	RETURN_IF_MEMORY_ERROR(DeleteUnusedFiles());
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

	// What icon to use for feeds without their own icon:
#ifdef SKIN_SUPPORT
	if (g_skin_manager->GetSkinElement("RSS"))
		m_skin_feed_icon_exists = TRUE;
#endif

	return OpStatus::OK;
}

OP_STATUS WebFeedsAPI_impl::RegisterCallbacks()
{
	OP_ASSERT(mh);
#ifdef WEBFEEDS_UPDATE_FEEDS
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_WEBFEEDS_UPDATE_FEEDS, 0));
#endif
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_WEBFEEDS_FEED_LOADING_POSTPONED, 0));

	return OpStatus::OK;
}

void WebFeedsAPI_impl::UnRegisterCallbacks()
{
	OP_ASSERT(mh);
	mh->UnsetCallBacks(this);
}

OP_STATUS WebFeedsAPI_impl::LoadFeed(URL& feed_url, OpFeedListener* listener, BOOL return_unchanged_feeds, time_t last_modified, const char* etag)
{
	WebFeed* feed;

	feed = return_unchanged_feeds ? (WebFeed*)GetFeedByUrl(feed_url) : NULL;
	return LoadFeed(feed, feed_url, listener, return_unchanged_feeds, last_modified, etag);
}

OP_STATUS WebFeedsAPI_impl::LoadFeed(WebFeed* existing_feed, URL& feed_url, OpFeedListener* listener, BOOL return_unchanged_feeds, time_t last_modified, const char* etag)
{
	WebFeedStub* stub = NULL;

    // use redirected URL if we know it's a 301
	if (feed_url.GetAttribute(URL::KHTTP_Response_Code, FALSE) == HTTP_MOVED)
		feed_url = feed_url.GetAttribute(URL::KMovedToURL, TRUE);

	// If already loaded, and time since last update not over minimum then return
	// existing feed:
	if (existing_feed)
	{
		stub = existing_feed->GetStub();
	}
	else
		stub = GetStubByUrl(feed_url);

	if (stub)
	{
		double current_time = OpDate::GetCurrentUTCTime();
		if (current_time - stub->GetUpdateTime() < (stub->GetMinUpdateInterval() * 60000))  // 60000 ms per minute
		{
			if (listener)
			{
				WebFeed *feed = existing_feed;
				if (!feed && return_unchanged_feeds)
					feed = stub->GetFeed();

				RETURN_IF_ERROR(m_postponed_listeners.Add(listener));
				mh->PostMessage(MSG_WEBFEEDS_FEED_LOADING_POSTPONED, (MH_PARAM_1)listener, (feed ? (MH_PARAM_2)feed->GetId() : 0));
			}

			return OpStatus::OK;
		}
	}


	if (last_modified || etag)
	{
		if (!stub)
		{
			// can't expect to get back a feed if there is no existing feed,
			// and doesn't want to load if not modified
			OP_ASSERT(!return_unchanged_feeds);
			return_unchanged_feeds = FALSE;
		}
	}
	else if (stub)
	{
		last_modified = stub->GetHttpLastModified();
		etag = stub->GetHttpETag();
	}

	return m_load_manager->LoadFeed(existing_feed, feed_url, listener, return_unchanged_feeds, last_modified, etag);
}

OP_STATUS WebFeedsAPI_impl::AbortLoading(URL& feed_url)
{
	return m_load_manager->AbortLoading(feed_url);
}

OP_STATUS WebFeedsAPI_impl::AbortAllFeedLoading(BOOL updating_feeds_only)
{
	return m_load_manager->AbortAllFeedLoading(updating_feeds_only);
}

UINT WebFeedsAPI_impl::GetMaxSize() { return m_max_size; }
void WebFeedsAPI_impl::SetMaxSize(UINT size) { m_max_size = size; }
UINT WebFeedsAPI_impl::GetDefMaxAge() { return m_max_age; }
void WebFeedsAPI_impl::SetDefMaxAge(UINT age) { m_max_age = age; }
UINT WebFeedsAPI_impl::GetDefMaxEntries() { return m_max_entries; }
void WebFeedsAPI_impl::SetDefMaxEntries(UINT entries) { m_max_entries = entries; }
UINT WebFeedsAPI_impl::GetDefUpdateInterval() { return m_update_interval; }
void WebFeedsAPI_impl::SetDefUpdateInterval(UINT interval)
{
	m_update_interval = interval;
#ifdef WEBFEEDS_AUTO_UPDATES_SUPPORT
	ScheduleNextUpdate();
#endif
}
BOOL WebFeedsAPI_impl::GetDefShowImages() { return m_show_images; }
void WebFeedsAPI_impl::SetDefShowImages(BOOL show) { m_show_images = show; }

void WebFeedsAPI_impl::SetNextFreeFeedId(OpFeed::FeedId id) { m_next_free_feedid = id; }

OP_STATUS WebFeedsAPI_impl::AddUpdateListener(OpFeedListener* listener)
{
	if (!listener || (m_update_listeners.Find(listener) != -1)) // No listener or listener already registered
		return OpStatus::OK;

	return m_update_listeners.Add(listener);
}

OP_STATUS WebFeedsAPI_impl::AddOmniscientListener(OpFeedListener* listener)
{
	if (!listener || (m_omniscient_listeners.Find(listener) != -1)) // No listener or listener already registered
		return OpStatus::OK;

	return m_omniscient_listeners.Add(listener);
}

OpVector<OpFeedListener>* WebFeedsAPI_impl::GetAllOmniscientListeners()
{
	return &m_omniscient_listeners;
}

static void RemoveAllOfItem(OpVector<OpFeedListener>& vector, OpFeedListener* item)
{
	for (int i = vector.GetCount() - 1; i >= 0; i--)
		if (vector.Get(i) == item)
			vector.Remove(i);
}

OP_STATUS WebFeedsAPI_impl::RemoveListener(OpFeedListener* listener)
{
	RemoveAllOfItem(m_update_listeners, listener);
	RemoveAllOfItem(m_omniscient_listeners, listener);
	RemoveAllOfItem(m_postponed_listeners, listener);
	RETURN_IF_MEMORY_ERROR(m_load_manager->RemoveListener(listener));

	return OpStatus::OK;
}

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
UINT WebFeedsAPI_impl::GetFeedDiskSpaceQuota()
{

	if (g_webfeeds_api_impl->GetMaxSize() > 0)  // 0 == unlimited
	{
		// Sanity check:
		if (g_webfeeds_api_impl->GetMaxSize() < g_webfeeds_api_impl->GetDiskSpaceUsed())
		{
			OP_ASSERT(!"All allowable disk space used by store file by itself");
			return 1;
		}

		return (UINT)((g_webfeeds_api_impl->GetMaxSize() - g_webfeeds_api_impl->GetDiskSpaceUsed())
			/ g_webfeeds_api_impl->GetNumberOfFeeds()
			* g_webfeeds_api_impl->GetMaxSizeSpaceFactor());
	}
	else
		return 0;
}

OP_STATUS WebFeedsAPI_impl::DeleteUnusedFiles()
{
	OpStackAutoPtr<OpFolderLister> lister(NULL);
	lister.reset(OpFile::GetFolderLister(OPFILE_WEBFEEDS_FOLDER, UNI_L("*")));
	if (!lister.get())
		return OpStatus::ERR;

	while (lister->Next())
	{
		const uni_char* filename = lister->GetFileName();

		OpFeed::FeedId id = 0;
		if (uni_sscanf(filename, UNI_L("%08x"), &id) != 1)
			continue;

		if (!uni_strcmp(filename, UNI_L("feedreaders.ini")))
			continue;

		if (GetFeedById(id) || lister->IsFolder())
			continue;

		OpFile unused_file;
		OP_STATUS status = unused_file.Construct(lister->GetFullPath());

		if (OpStatus::IsMemoryError(status))
			return status;

		if (status != OpStatus::OK)
			continue;

		RETURN_IF_MEMORY_ERROR(unused_file.Delete());
	}

	return OpStatus::OK;
}

UINT WebFeedsAPI_impl::SaveL(WriteBuffer &buf)
{
	UINT pre_space_used = buf.GetBytesWritten();
	OP_ASSERT(m_feeds);

	buf.Append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	buf.Append("<!DOCTYPE store SYSTEM \"storage.dtd\" [\n");
	buf.Append("  <!ELEMENT store (defaults | feed*)>\n");
	buf.Append("  <!ATTLIST store\n");
	buf.Append("            default-update-interval NMTOKEN #REQUIRED\n");
	buf.Append("            space-factor CDATA \"0.9\">]>\n");

	buf.Append("<store default-update-interval=\"");
	buf.Append(m_update_interval);
	buf.Append("\" space-factor=\"");
	buf.Append((UINT)(m_store_space_factor * 100.0));
	buf.Append("\">\n");

	buf.Append("<defaults>\n");
	WebFeed::WriteSettingNum(buf, "max-size", m_max_size);
	WebFeed::WriteSettingNum(buf, "max-age", m_max_age);
	WebFeed::WriteSettingNum(buf, "max-entries", m_max_entries);
	WebFeed::WriteSettingBool(buf, "show-images", m_show_images);
	buf.Append("</defaults>\n");

	UINT total_disk_space_used = 0;
	BOOL is_space_limited = FALSE;
	OpStackAutoPtr<OpHashIterator> iter(m_feeds->GetIterator());
	if (!iter.get())
		LEAVE(OpStatus::ERR_NO_MEMORY);

	for (OP_STATUS status = iter->First(); status != OpStatus::ERR; status = iter->Next())
	{
		WebFeedStub* stub = (WebFeedStub*)iter->GetData();
		OP_ASSERT(stub);

		if (stub->IsSubscribed())
		{
			stub->SaveL(buf, TRUE);
			if (stub->FeedNeedSaving())
				m_storage->SaveFeedL(stub->GetFeed());

			if (stub->HasReachedDiskSpaceLimit())
				is_space_limited = TRUE;
		}

		total_disk_space_used += stub->GetDiskSpaceUsed();
	}

	buf.Append("</store>\n");

	m_disk_space_used = buf.GetBytesWritten() - pre_space_used;
	total_disk_space_used += m_disk_space_used;

	UINT max_size = GetMaxSize();
	if (max_size > 0)
	{
		double excess_ratio = (double(max_size) - total_disk_space_used) / max_size;
		if (excess_ratio < 0 || is_space_limited)  // don't increase limit if no one needs more space
		{
			m_store_space_factor += excess_ratio;

			if (m_store_space_factor > 2.0)
				m_store_space_factor = 2.0;
			else if (m_store_space_factor < 0.2)
				m_store_space_factor = 0.2;
		}
	}

	return m_disk_space_used;
}
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

OP_STATUS WebFeedsAPI_impl::AddFeed(WebFeed* feed)
{
	if (!feed->GetTitle()->IsBinary() && !feed->GetStub()->HasTitle())
		feed->GetStub()->SetTitle(feed->GetTitle()->Data());

	return AddFeed(feed->GetStub());
}

OP_STATUS WebFeedsAPI_impl::AddFeed(WebFeedStub* stub) //, OpFeed::FeedId id)
{
	OP_ASSERT(stub);

	OpFeed::FeedId id = stub->GetId();
	if (id == 0)
	{
		id = m_next_free_feedid++;
		stub->SetId(id);
	}
	else
	{
		void* old_stub = NULL;
		if (m_feeds->GetData(reinterpret_cast<void*>(id), &old_stub) == OpStatus::OK)  // We already have a stub for this feed
		{
			if (old_stub == stub)
			{
#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
				// Check if this feed is in memory cache already
				for (UINT i=0; i<ARRAY_SIZE(m_feeds_memory_cache); i++)
				{
					if (m_feeds_memory_cache[i] == NULL)
					{
						m_feeds_memory_cache[i] = stub;
						return OpStatus::OK;
					}
					else if (m_feeds_memory_cache[i] == stub)
					{
						// Is already in cache, move it to the end so it lives longer
						for (; i<ARRAY_SIZE(m_feeds_memory_cache)-1 && m_feeds_memory_cache[i+1]; i++)
							m_feeds_memory_cache[i] = m_feeds_memory_cache[i+1];
						m_feeds_memory_cache[i] = stub;

						return OpStatus::OK;
					}
				}
				// No room in cache, throw some old feed out and insert this
				ReplaceOldestInCache(stub);
#else
				// feed already in memory
				return OpStatus::OK;
#endif // WEBFEEDS_SAVED_STORE_SUPPORT
			}
			else if (m_feeds->Remove(reinterpret_cast<void*>(id), &old_stub) == OpStatus::OK)
			{
				OP_ASSERT(!"Who was a bad boy and created a new stub for an existing feed?");
				WebFeedStub* to_delete = (WebFeedStub*)old_stub;
				OP_DELETE(to_delete);
			}
		}
	}

	RETURN_IF_ERROR(m_feeds->Add(reinterpret_cast<void*>(id), (void*)stub));
#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	AddToInternalFeedCache(stub);
#endif // WEBFEEDS_SAVED_STORE_SUPPORT
	stub->IncRef();

	return OpStatus::OK;
}

OP_STATUS WebFeedsAPI_impl::CheckAllFeedsForNumEntries()
{
	OpStackAutoPtr<OpHashIterator> iter(m_feeds->GetIterator());
	if (!iter.get())
		return OpStatus::ERR_NO_MEMORY;

	for (OP_STATUS status = iter->First(); status != OpStatus::ERR; status = iter->Next())
		((WebFeedStub*)iter->GetData())->GetFeed()->CheckFeedForNumEntries(0);

	return OpStatus::OK;
}

OP_STATUS WebFeedsAPI_impl::CheckAllFeedsForExpiredEntries()
{
	OpStackAutoPtr<OpHashIterator> iter(m_feeds->GetIterator());
	if (!iter.get())
		return OpStatus::ERR_NO_MEMORY;

	for (OP_STATUS status = iter->First(); status != OpStatus::ERR; status = iter->Next())
		((WebFeedStub*)iter->GetData())->GetFeed()->CheckFeedForExpiredEntries();

	return OpStatus::OK;
}

#ifdef WEBFEEDS_EXTERNAL_READERS
unsigned WebFeedsAPI_impl::GetExternalReaderCount() const
{
	return m_external_reader_manager->GetCount();
}

unsigned WebFeedsAPI_impl::GetExternalReaderIdByIndex(unsigned index) const
{
	return m_external_reader_manager->GetIdByIndex(index);
}

OP_STATUS WebFeedsAPI_impl::GetExternalReaderName(unsigned id, OpString& name) const
{
	return m_external_reader_manager->GetName(id, name);
}

OP_STATUS WebFeedsAPI_impl::GetExternalReaderTargetURL(unsigned id, const URL& feed_url, URL& target_url) const
{
	return m_external_reader_manager->GetTargetURL(id, feed_url, target_url);
}

#ifdef WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
OP_STATUS WebFeedsAPI_impl::GetExternalReaderCanEdit(unsigned id, BOOL& can_edit) const
{
	WebFeedReaderManager::ReaderSource source;
	RETURN_IF_ERROR(m_external_reader_manager->GetSource(id, source));
	can_edit = source == WebFeedReaderManager::READER_CUSTOM;
	return OpStatus::OK;
}

unsigned WebFeedsAPI_impl::GetExternalReaderFreeId() const
{
	return m_external_reader_manager->GetFreeId();
}

OP_STATUS WebFeedsAPI_impl::AddExternalReader(unsigned id, const uni_char *feed_url, const uni_char *feed_name) const
{
	return m_external_reader_manager->AddReader(id, feed_url, feed_name);
}

BOOL WebFeedsAPI_impl::HasExternalReader(const uni_char *feed_url, unsigned *id /* = NULL */) const
{
	return m_external_reader_manager->HasReader(feed_url, id);
}

OP_STATUS WebFeedsAPI_impl::DeleteExternalReader(unsigned id) const
{
	return m_external_reader_manager->DeleteReader(id);
}
#endif // WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
#endif // WEBFEEDS_EXTERNAL_READERS

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
void WebFeedsAPI_impl::AddToInternalFeedCache(WebFeedStub* stub)
{
	if (!stub)
		return;

	UINT ncached = 0;  // ncached should also point to first free index in the array
	UINT max_cached = ARRAY_SIZE(m_feeds_memory_cache);
	for (ncached=0; ncached<max_cached && m_feeds_memory_cache[ncached]; ncached++)
		/* empty body */ ;

#ifdef _DEBUG  // Make sure we don't have holes in cache. No one else should call
    // RemoveFeedFromMemory, and since we always remove the first and move all the others
    // one step up, there should never be any feeds after first NULL
	for (UINT rest=ncached; rest<max_cached; rest++)
		OP_ASSERT(m_feeds_memory_cache[rest] == NULL);
#endif

	OP_ASSERT(ncached <= max_cached);
	if (ncached >= max_cached)
		ReplaceOldestInCache(stub);
	else
		m_feeds_memory_cache[ncached] = stub;
}

void WebFeedsAPI_impl::ReplaceOldestInCache(WebFeedStub* stub)
{
	if (stub == m_feeds_memory_cache[0])
		return;

	// Have to throw out one feed from memory, choose oldest one
	m_feeds_memory_cache[0]->RemoveFeedFromMemory();

	// copy all the others one step forward in array
	for (UINT i=1; i<ARRAY_SIZE(m_feeds_memory_cache); i++)
		m_feeds_memory_cache[i-1] = m_feeds_memory_cache[i];

	m_feeds_memory_cache[ARRAY_SIZE(m_feeds_memory_cache)-1] = stub;
}
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

#ifdef WEBFEEDS_AUTO_UPDATES_SUPPORT
void WebFeedsAPI_impl::DoAutomaticUpdates(BOOL auto_update)
{
	m_do_automatic_updates = auto_update;

	if (auto_update)
		ScheduleNextUpdate();
#ifdef WEBFEEDS_UPDATE_FEEDS
	else
		mh->RemoveDelayedMessage(MSG_WEBFEEDS_UPDATE_FEEDS, 0, 0);
#endif
}
#endif // WEBFEEDS_AUTO_UPDATES_SUPPORT

void WebFeedsAPI_impl::FeedFinished(OpFeed::FeedId id)
{
	// Bookkeeping for updating feeds.
	// UpdateFeeds sets the IsUpdating flag for each feed it starts an update, and counts
	// how many is updating. Here we remove the flag, and when no more feeds are updating
	// notify the updatelisteners.
	WebFeedStub* stub = GetStubById(id);
	if (stub && stub->IsUpdating())
	{
		stub->SetIsUpdating(FALSE);
		m_number_updating_feeds--;
		if (m_number_updating_feeds == 0)
			UpdateFinished();
	}
}

void WebFeedsAPI_impl::UpdateFinished()
{
	// Notify listeners and schedule next automatic update
	UINT i;
	for (i = 0; i < m_update_listeners.GetCount(); i++)
		m_update_listeners.Get(i)->OnUpdateFinished();
	for (i = 0; i < m_omniscient_listeners.GetCount(); i++)
		m_omniscient_listeners.Get(i)->OnUpdateFinished();

	m_update_listeners.Clear();

#ifdef WEBFEEDS_AUTO_UPDATES_SUPPORT
	ScheduleNextUpdate();
#endif

#ifdef WEBFEEDS_AUTOSAVE
	TRAPD(status, GetStorage()->SaveStoreL(this));
	if (OpStatus::IsMemoryError(status))
		g_memory_manager->RaiseCondition(status);
#endif
}

#ifdef WEBFEEDS_AUTO_UPDATES_SUPPORT
void WebFeedsAPI_impl::ScheduleNextUpdate()
{
	// Post a message so we will be called again in time for next automatic update
	double current_time = OpDate::GetCurrentUTCTime();
	double next_default_update = current_time + (m_update_interval * msPerMin);
	if (m_next_update == 0.0 || m_next_update > next_default_update)   // no time specified, use update interval
		m_next_update = next_default_update;

	OP_ASSERT(m_next_update - current_time >= -120000);  // two minutes - if the loading of a feed takes a long time to time out, and the update interval is short then the next update may be scheduled before the last has finished
	unsigned long time_until_next_update;
	if (m_next_update - current_time > msPerMin)
		time_until_next_update = (unsigned long)(m_next_update - current_time);
	else
		time_until_next_update = msPerMin;

#ifdef WEBFEEDS_UPDATE_FEEDS
#ifdef DEBUG_FEEDS_UPDATING
	printf("Next update in %d minutes %d seconds\n", (int)(time_until_next_update/msPerMin), (int)((time_until_next_update%msPerMin)/1000));
#endif
	if (GetAutoUpdate())
		mh->PostDelayedMessage(MSG_WEBFEEDS_UPDATE_FEEDS, 0, 0, time_until_next_update);
#endif
}
#endif // WEBFEEDS_AUTO_UPDATES_SUPPORT

OP_STATUS WebFeedsAPI_impl::UpdateSingleFeed(WebFeedStub* stub, OpFeedListener *listener, BOOL return_unchanged_feeds, BOOL force_update)
{
	const double fudge_factor = 0.5;   // Amount of update interval which must have passed before we do an update
                                       // This is so that we try to update feeds simultaniously

	// The timestamps are in milliseconds, while the intervals stored in feeds/store
	// is in minutes. Hence all the *60000 when adding interval to timestamp.
	double current_time = OpDate::GetCurrentUTCTime();

	OP_ASSERT(stub);
	OP_ASSERT(stub->GetUpdateTime() != 0.0);  // All feeds should get their time set when parsing

	double feed_next_update = stub->GetUpdateTime() + (stub->GetUpdateInterval() * msPerMin);
	const double fudged_interval = stub->GetUpdateInterval() * msPerMin * fudge_factor;
	if ((!force_update && (current_time + fudged_interval) < feed_next_update) ||  // check if update interval has passed (but allow feed to be updated before it should have been by amount of fudge factor
		(current_time - stub->GetUpdateTime()) < (stub->GetMinUpdateInterval() * msPerMin))  // Mimimum time must always have passed
	{
		if (listener)
		{
			if (return_unchanged_feeds)
				listener->OnFeedLoaded(stub->GetFeed(), OpFeed::STATUS_REFRESH_POSTPONED);
			else  // don't actually return the feed object for unchanged
			      // feeds, as that might require loading it from disk
				listener->OnFeedLoaded(NULL, OpFeed::STATUS_REFRESH_POSTPONED);
		}
	}
	else // safe to update feed
	{
		OP_ASSERT(stub->GetId() != 0);
		stub->SetIsUpdating(TRUE);
		m_number_updating_feeds++;

		if (m_load_manager->UpdateFeed(stub->GetURL(), stub->GetId(), listener, return_unchanged_feeds, stub->GetHttpLastModified(), stub->GetHttpETag()) == OpStatus::ERR_NO_MEMORY)
			return OpStatus::ERR_NO_MEMORY;
		feed_next_update = current_time + (stub->GetUpdateInterval() * msPerMin);
	}

	if (m_next_update == 0.0 || feed_next_update < m_next_update)
		m_next_update = feed_next_update;

	return OpStatus::OK;
}

OP_STATUS WebFeedsAPI_impl::UpdateFeeds(OpFeedListener *listener, BOOL return_unchanged_feeds, BOOL force_update)
{
	// Remove any messages telling us to update, in case there was an automatic update
	// scheduled, but we are updating manually
#ifdef WEBFEEDS_UPDATE_FEEDS
	mh->RemoveDelayedMessage(MSG_WEBFEEDS_UPDATE_FEEDS, 0, 0);
#endif

	m_next_update = 0.0;  // will use default update interval

	// If in offline mode, just return immediately
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
	{
		UpdateFinished();
		return OpStatus::OK;
	}

	if (m_number_updating_feeds > 0)  // We only do one update at a time, so finish the one we're doing
	{
		AddUpdateListener(listener);
		return OpStatus::OK;
	}

	OpStackAutoPtr<OpHashIterator> iter(m_feeds->GetIterator());
	if (!iter.get())
		return OpStatus::ERR_NO_MEMORY;

	for (OP_STATUS status = iter->First(); status != OpStatus::ERR; status = iter->Next())
		UpdateSingleFeed((WebFeedStub*)iter->GetData(), listener, return_unchanged_feeds, force_update);

	if (m_number_updating_feeds > 0)
		AddUpdateListener(listener);
	else  // no feeds needed updating
		UpdateFinished();

	return OpStatus::OK;
}

WebFeedsAPI_impl::WebFeedStub* WebFeedsAPI_impl::GetStubByUrl(const URL& feed_url)
{
	OpStackAutoPtr<OpHashIterator> iter(m_feeds->GetIterator());
	if (!iter.get())
		return NULL;

	for (OP_STATUS status = iter->First(); status != OpStatus::ERR; status = iter->Next())
	{
		WebFeedStub* stub = (WebFeedStub*)iter->GetData();
		OP_ASSERT(stub);

		if (stub->GetURL() == feed_url)
			return stub;
	}

	return NULL;
}

WebFeedsAPI_impl::WebFeedStub* WebFeedsAPI_impl::GetStubById(OpFeed::FeedId id)
{
	if (!id)
		return NULL;

	void* stub = NULL;
	if (m_feeds->GetData(reinterpret_cast<void*>(id), &stub) != OpStatus::OK)
		return NULL;

	return (WebFeedStub*)stub;
}

OpFeed* WebFeedsAPI_impl::GetFeedByUrl(const URL& feed_url)
{
	WebFeedStub* stub = GetStubByUrl(feed_url);
	if (stub)
		return stub->GetFeed();

	return NULL;
}

OpFeed*	WebFeedsAPI_impl::GetFeedById(OpFeed::FeedId id)
{
	WebFeedStub* stub = GetStubById(id);
	if (stub)
		return stub->GetFeed();  // should only return NULL on error
	return NULL;
}

UINT WebFeedsAPI_impl::GetNumberOfFeeds()
{
	return (UINT)m_feeds->GetCount();
}

UINT* WebFeedsAPI_impl::GetAllFeedIds(UINT& length, BOOL subscribed_only)
{
	length = 0;
	const UINT nfeeds = GetNumberOfFeeds();

	OpFeed::FeedId* ids = OP_NEWA(OpFeed::FeedId, nfeeds + 1);  // plus 0 guard at end
	if (!ids)
		return NULL;

	OpStackAutoPtr<OpHashIterator> iter(m_feeds->GetIterator());
	if (!iter.get())
	{
		OP_DELETEA(ids);
		return NULL;
	}

	UINT i = 0;
	for (OP_STATUS status = iter->First(); status != OpStatus::ERR; status = iter->Next())
	{
		OP_ASSERT(i < nfeeds);

		WebFeedStub* stub = (WebFeedStub*)iter->GetData();
		OP_ASSERT(stub && stub->GetId());

		if (!subscribed_only || stub->IsSubscribed())
		{
			ids[i] = stub->GetId();
			i++;
		}
	}
	length = i;
	ids[i] = 0;

	return ids;
}

#ifdef WEBFEEDS_DISPLAY_SUPPORT
OP_STATUS WebFeedsAPI_impl::GetFeedFromIdURL(const char* feed_id, OpFeed*& feed, OpFeedEntry*& entry, WebFeedsAPI_impl::FeedIdURLType& type)
{
	feed = NULL;
	entry = NULL;

	if (!feed_id)
		return OpStatus::ERR_NULL_POINTER;

	feed_id = op_strchr(feed_id, '/');

	if (!feed_id || !*(feed_id+1))
		return OpStatus::ERR;
	feed_id++; // skip leading slash

	if (op_strncmp(feed_id, "all-subscribed", 14) == 0)  // write list of subscribed feeds
	{
		type = SubscriptionListURL;
		return OpStatus::OK;
	}
	if (op_strncmp(feed_id, "settings", 8) == 0)  // settings page
	{
		type = GlobalSettingsURL;
		return OpStatus::OK;
	}

	UINT fid;
	char entry_str[9];  /* ARRAY OK 2009-04-27 arneh */
	UINT num_ids = op_sscanf(feed_id, "%8x-%8s", &fid, entry_str);

	if (num_ids == 0)
		return OpStatus::ERR;

	feed = (WebFeed*)GetFeedById(fid);
	if (!feed)
		return OpStatus::ERR;

	if (num_ids == 1)
	{
		type = FeedIdURL;
		return OpStatus::OK;
	}

	OP_ASSERT(num_ids == 2);

	if (op_strcmp(entry_str, "settings") == 0)
	{
		type = FeedSettingsURL;
		return OpStatus::OK;
	}

	UINT eid;
	if (op_sscanf(entry_str, "%8x", &eid) != 1)
		return OpStatus::ERR;

	entry = (WebFeedEntry*)feed->GetEntryById(eid);
	if (!entry)
		return OpStatus::ERR;
	type = EntryIdURL;
	return OpStatus::OK;
}

WebFeedsAPI::OpFeedWriteStatus
WebFeedsAPI_impl::WriteByFeedId(const char* feed_id, URL& out_url)
{
	OpFeed* feed = NULL;
	OpFeedEntry* entry = NULL;
	FeedIdURLType type;

	if (OpStatus::IsError(GetFeedFromIdURL(feed_id, feed, entry, type)))
		return WebFeedsAPI::IllegalId;

	OP_STATUS status = OpStatus::OK;
	switch (type)
	{
	case SubscriptionListURL:
		status = WriteSubscriptionList(out_url);
		break;
	case FeedIdURL:
#ifdef WEBFEEDS_WRITE_SUBSCRIBE_BOX
		status = feed->WriteFeed(out_url, TRUE, TRUE);
#else
		status = feed->WriteFeed(out_url, TRUE, FALSE);
#endif // WEBFEEDS_WRITE_SUBSCRIBE_BOX
		break;
	case EntryIdURL:
#ifdef OLD_FEED_DISPLAY
		status = entry->WriteEntry(out_url);
#endif // OLD_FEED_DISPLAY
		break;

#ifdef WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
	case GlobalSettingsURL:
		TRAP(status, WriteGlobalSettingsPageL(out_url));
		break;
	case FeedSettingsURL:
		TRAP(status, ((WebFeed*)feed)->WriteSettingsPageL(out_url));
		break;
#endif // WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
	default:
		return WebFeedsAPI::IllegalId;
	}

	if (OpStatus::IsError(status))
		return WebFeedsAPI::WriteError;
	else
		return WebFeedsAPI::Written;
}

#ifdef OLD_FEED_DISPLAY
inline void AddHTMLL(URL& out_url, const uni_char* data)
{
	LEAVE_IF_ERROR(out_url.WriteDocumentData(URL::KNormal, data));
}
#endif

void WebFeedsAPI_impl::WriteSubscriptionListL(URL& out_url, BOOL complete_document)
{
#ifndef OLD_FEED_DISPLAY
	out_url = urlManager->GetURL("opera:feed-id/all-subscribed");
	OperaFeeds feed_preview(out_url, NULL);
	LEAVE_IF_ERROR(feed_preview.GenerateData());
#else
	// Create a new URL to write generated data to:
	if (out_url.IsEmpty())
		out_url = urlManager->GetNewOperaURL();

	if (complete_document)
		WebFeedUtil::WriteGeneratedPageHeaderL(out_url, UNI_L("Subscribed feeds")); // TODO: localize

	// Write title of all feeds:
	out_url.WriteDocumentDataUniSprintf(UNI_L("<h1>%s</h1>"), UNI_L("Subscribed feeds"));   // TODO: localize text

	out_url.WriteDocumentDataUniSprintf(UNI_L("<a href=\"%s\">%s</a>"), UNI_L(WEBFEEDS_SETTINGS_URL), UNI_L("settings"));  // TODO: localize text

	AddHTMLL(out_url, UNI_L("<form id=\"feed-selection\" action=\"\">\n<table>\n"));
	OpStackAutoPtr<OpHashIterator> iter(m_feeds->GetIterator());
	if (!iter.get())
		LEAVE(OpStatus::ERR_NO_MEMORY);

	for (OP_STATUS status = iter->First(); status != OpStatus::ERR; status = iter->Next())
	{
		WebFeedStub* stub = (WebFeedStub*)iter->GetData();
		OP_ASSERT(stub);
		if (stub)
		{
			if (!stub->IsSubscribed() || !stub->GetTitle())
				continue;

			OpString title; ANCHOR(OpString, title);
			LEAVE_IF_ERROR(WebFeedUtil::StripTags(stub->GetTitle(), title));

			OpString id_url; ANCHOR(OpString, id_url);
			stub->GetFeedIdURL(id_url);

			UINT unread_count = stub->GetUnreadCount();
			if (unread_count > 0)
				AddHTMLL(out_url, UNI_L("<tr class=\"unread\">"));
			else
				AddHTMLL(out_url, UNI_L("<tr>"));

			AddHTMLL(out_url, UNI_L("<tr>"));
			out_url.WriteDocumentDataUniSprintf(UNI_L("<td><a href=\"%s\">"), id_url.CStr());

			OpString icon; ANCHOR(OpString, icon);
			LEAVE_IF_ERROR(stub->GetInternalFeedIcon(icon));
			if (!icon.IsEmpty())
			{
				AddHTMLL(out_url, UNI_L("<img src=\""));
				AddHTMLL(out_url, icon.CStr());
				AddHTMLL(out_url, UNI_L("\" alt=\"\"> "));
			}
			else
				if (m_skin_feed_icon_exists)
					AddHTMLL(out_url, UNI_L("<img src=\"\" class=\"noicon\" alt=\"\">"));
				else
					AddHTMLL(out_url, UNI_L("<img src=\"\" class=\"noicon noskin\" alt=\"\">"));

			AddHTMLL(out_url, title.CStr());

			AddHTMLL(out_url, UNI_L("</a></td><td>"));
			out_url.WriteDocumentDataUniSprintf(UNI_L("%d/%d</td>"), stub->GetUnreadCount(), stub->GetTotalCount());

			// Write status if not ok
			if (stub->GetStatus() != OpFeed::STATUS_OK && stub->GetStatus() != OpFeed::STATUS_REFRESH_POSTPONED)
			{
				const uni_char* status;
				if (stub->GetStatus() == OpFeed::STATUS_ABORTED)
					status = UNI_L("Aborted");
				else
					status = UNI_L("Failed");

				out_url.WriteDocumentDataUniSprintf(UNI_L("<td>[%s]</td>"), status); // TODO: localize string
			}
			else  // else write timestamp of last update
			{
				uni_char* short_time = WebFeedUtil::TimeToShortString(stub->GetUpdateTime(), TRUE);
				if (short_time)
					out_url.WriteDocumentDataUniSprintf(UNI_L("<td>[%s]</td>"), short_time);
				else
					AddHTMLL(out_url, UNI_L("<td></td>"));
				OP_DELETEA(short_time);
			}

#ifdef WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
			out_url.WriteDocumentDataUniSprintf(UNI_L("<td><input name=\"%x\" type=\"checkbox\"></td>"), stub->GetId());
			out_url.WriteDocumentDataUniSprintf(UNI_L("<td><a href=\"%s-settings\">&raquo;</a></td>"), id_url.CStr());
#endif // WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI

			AddHTMLL(out_url, UNI_L("</tr>\n"));
		}
	}
	AddHTMLL(out_url, UNI_L("</table>\n</form>\n"));

	if (complete_document)
		WebFeedUtil::WriteGeneratedPageFooterL(out_url);
#endif // OLD_FEED_DISPLAY
}

#ifdef OLD_FEED_DISPLAY
#ifdef WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
OP_STATUS WebFeedsAPI_impl::WriteGlobalSettingsPage(URL& out_url)
{
	TRAPD(status, WriteGlobalSettingsPageL(out_url));
	return status;
}

void WebFeedsAPI_impl::WriteGlobalSettingsPageL(URL& out_url)
{
	const uni_char* title = UNI_L("Webfeeds settings");

	// Create a new URL to write generated data to:
	if (out_url.IsEmpty())
		out_url = urlManager->GetNewOperaURL();

	WebFeedUtil::WriteGeneratedPageHeaderL(out_url, title);

	out_url.WriteDocumentDataUniSprintf(UNI_L("<h1>%s</h1>\n"), title);
	AddHTMLL(out_url, UNI_L("<form id=\"global-feed-settings\" action=\"\">\n"));
	AddHTMLL(out_url, UNI_L("<dl>\n"));

	WriteUpdateAndExpireFormL(out_url, FALSE);

	// total disk space
	UINT space = GetMaxSize() / 1024;  // unit is bytes originally, but won't showing anything less than kB in UI
	UINT space_unit = 1;
	const char* space_options[] = {"MB", "kB"};
	if (space > 2048) // use MB as unit
	{
		space = (space + 512) / 1024;
		space_unit = 0;
	}
	OpString options_string; ANCHOR(OpString, options_string);

	// Iterate through and find if any feeds are selected
	WebFeedFormIterator form_iter;
	RETURN_IF_ERROR(form_iter.Init(frm_doc, UNI_L("feed-selection")));
	for (HTML_Element* helm = form_iter.GetNext(); helm; helm = form_iter.GetNext())
	{
		FormValueRadioCheck* form_value = FormValueRadioCheck::GetAs(helm->GetFormValue());
		if (form_value->IsChecked(helm))
		{
			const uni_char* id_string = helm->GetId();
			const UINT id = (UINT)uni_strtol(id_string, NULL, 16);

			if (id)
			{
				checked = TRUE;
				return OpStatus::OK;
			}
		}
	}

	return OpStatus::OK;
}


OP_STATUS WebFeedsAPI_impl::RunCommandOnChecked(FramesDocument* frm_doc, WebFeedsAPI::SubscriptionListCommand command)
{
	if (op_strcmp(frm_doc->GetURL().GetName(FALSE, PASSWORD_SHOW), WEBFEEDS_SUBSCRIBED_LIST_URL))
		return OpStatus::ERR;

	// Iterate through and find which feeds are selected
	WebFeedFormIterator form_iter;
	RETURN_IF_ERROR(form_iter.Init(frm_doc, UNI_L("feed-selection")));
	for (HTML_Element* helm = form_iter.GetNext(); helm; helm = form_iter.GetNext())
	{
		FormValueRadioCheck* form_value = FormValueRadioCheck::GetAs(helm->GetFormValue());
		if (form_value->IsChecked(helm))
		{
			const uni_char* id_string = helm->GetName();
			const UINT id = (UINT)uni_strtol(id_string, NULL, 16);

			WebFeed* feed = (WebFeed*)GetFeedById(id);
			if (!feed)
				continue;

			switch (command)
			{
			case UnsubscribeMarked:
				feed->UnSubscribe();
				break;
			case MarkAllRead:
				feed->MarkAllEntries(OpFeedEntry::STATUS_READ);
				break;
			default:
				OP_ASSERT(FALSE);
			}
		}
	}

	return OpStatus::OK;
}
#endif // WEBFEEDS_SUBSCRIPTION_LIST_FORM_UI
#endif // OLD_FEED_DISPLAY

#ifdef WEBFEEDS_REFRESH_FEED_WINDOWS
OP_STATUS WebFeedsAPI_impl::UpdateFeedWindow(WebFeed* feed)
{
	// TODO: this code only checks windows, not frames.  So it won't work
	// if a feed is shown inside a frame.  Should be rewritten to work with
	// frames also

	for (Window* win = g_window_manager->FirstWindow(); win; win = win->Suc())
	{
		const uni_char* win_url = ((URL)win->GetCurrentURL()).GetUniName(FALSE, PASSWORD_HIDDEN);
		if (!win_url)
			continue;

		if (feed)
		{
			OpString id_url;
			RETURN_IF_ERROR(feed->GetFeedIdURL(id_url));
			if (id_url.Compare(win_url, id_url.Length()) == 0)
				win->Reload();
		}
		if (uni_strncmp(win_url, UNI_L(WEBFEEDS_SUBSCRIBED_LIST_URL), uni_strlen(win_url)) == 0)
			win->Reload();
	}

	return OpStatus::OK;
}
#endif // WEBFEEDS_REFRESH_FEED_WINDOWS

#endif // WEBFEEDS_DISPLAY_SUPPORT


void WebFeedsAPI_impl::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
#ifdef WEBFEEDS_UPDATE_FEEDS
	case MSG_WEBFEEDS_UPDATE_FEEDS:
		RAISE_AND_RETURN_VOID_IF_ERROR(UpdateFeeds(NULL, FALSE, FALSE));
		break;
#endif
	case MSG_WEBFEEDS_FEED_LOADING_POSTPONED:
		{
			OpFeedListener* listener = (OpFeedListener*)par1;

			// check that listener pointer still is valid
			if (OpStatus::IsError(m_postponed_listeners.RemoveByItem(listener)))
				break;

			OpFeed* feed = GetFeedById((OpFeed::FeedId)par2);
			listener->OnFeedLoaded(feed, OpFeed::STATUS_REFRESH_POSTPONED);
			break;
		}
	default:
		OP_ASSERT(!"Unknown message");
	}
}

void OpFeedListener::OnUpdateFinished()
{
}

void OpFeedListener::OnFeedLoaded(OpFeed* feed, OpFeed::FeedStatus)
{
}

void OpFeedListener::OnEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus)
{
}

void OpFeedListener::OnNewEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus)
{
}

#endif // WEBFEEDS_BACKEND_SUPPORT
