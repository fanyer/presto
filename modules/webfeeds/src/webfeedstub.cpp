// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/src/webfeeds_api_impl.h"
#include "modules/webfeeds/src/webfeed.h"
#include "modules/webfeeds/src/webfeedstorage.h"

/*******************************
* WebFeedStub
*******************************/

OP_STATUS WebFeedsAPI_impl::WebFeedStub::Init()
{
	OP_ASSERT(!m_feed);
	OP_ASSERT(g_webfeeds_api->GetDefUpdateInterval());

	m_update_interval = 0;

	return OpStatus::OK;
}

OP_STATUS WebFeedsAPI_impl::WebFeedStub::Init(const uni_char* title,
	double update_time, UINT update_interval, UINT min_update_interval,
	URL& feed_url, BOOL subscribed)
{
	OP_ASSERT(!m_feed); // Somebody set feed without initializing first. Bad boy.
	RETURN_IF_ERROR(m_title.Set(title));
	m_update_time = update_time;
	m_update_interval = update_interval;
	m_min_update_interval = min_update_interval;
	m_feed_url = feed_url;
	m_packed.is_subscribed = subscribed;

	return OpStatus::OK;
}

WebFeedsAPI_impl::WebFeedStub::WebFeedStub() 
	: m_feed(NULL), m_id(0), m_status(OpFeed::STATUS_OK), m_update_time(0.0),
    m_min_update_interval(0), m_http_last_modified(0), m_total_entries(0),
    m_unread_entries(0), 
#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
    m_disk_space_used(0), 
#endif // WEBFEEDS_SAVED_STORE_SUPPORT
    m_packed_init(0)
{
}

WebFeedsAPI_impl::WebFeedStub::~WebFeedStub()
{
}

void WebFeedsAPI_impl::WebFeedStub::IncRef()
{
	if (m_feed)
		m_feed->IncRef();
}

void WebFeedsAPI_impl::WebFeedStub::StoreDestroyed()
{
	if (m_feed)
	{
		m_feed->MarkForRemoval(FALSE);  // MarkForRemoval would have left the stub, 
		                                // now we want to remove the stub as well
		m_feed->DecRef();
	}
	else
		OP_DELETE(this);
}

OP_STATUS WebFeedsAPI_impl::WebFeedStub::GetTitle(OpString& title)
{
	if (!m_title.IsEmpty())
		return title.Set(m_title);

    return m_feed_url.GetAttribute(URL::KUniName_Username_Password_Hidden, title);
}

UINT WebFeedsAPI_impl::WebFeedStub::GetUpdateInterval()
{
	if (m_update_interval)
		return m_update_interval;
	return g_webfeeds_api->GetDefUpdateInterval();
}

void WebFeedsAPI_impl::WebFeedStub::SetUpdateInterval(UINT interval)
{
	if (interval == 0 || interval > GetMinUpdateInterval())
		m_update_interval = interval;
	else
		m_update_interval = GetMinUpdateInterval();
}

OP_STATUS WebFeedsAPI_impl::WebFeedStub::GetFeedIdURL(OpString& result_string)
{
	OP_ASSERT(sizeof(GetId()) == 4);
	
	result_string.Empty();
	RETURN_IF_ERROR(result_string.AppendFormat(UNI_L("opera:feed-id/%08x"), GetId()));
	
	return OpStatus::OK;	
}

OP_STATUS WebFeedsAPI_impl::WebFeedStub::GetInternalFeedIcon(OpString& icon)
{
	OpString folder;
	
	icon.Empty();
	
	if (m_feed_icon.IsEmpty())
		return OpStatus::OK;

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_WEBFEEDS_FOLDER, folder));
	
	OP_ASSERT(!folder.IsEmpty());	
	if (folder.IsEmpty())
		return OpStatus::ERR;

	RETURN_IF_ERROR(icon.Append(UNI_L("file://localhost/")));
	RETURN_IF_ERROR(icon.Append(folder));
	RETURN_IF_ERROR(icon.Append(m_feed_icon));
#endif // WEBFEEDS_SAVED_STORE_SUPPORT
	
	return OpStatus::OK;
}

WebFeed* WebFeedsAPI_impl::WebFeedStub::GetFeed()
{
	if (m_feed)
		return m_feed;

 	// We don't have feed in memory right now, load it from store
	WebFeed* feed = NULL;
#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
 	m_total_entries = m_unread_entries = 0;
	SetHasReachedDiskSpaceLimit(FALSE);

	if (OpStatus::IsError(g_webfeeds_api_impl->GetStorage()->LoadFeed(this, feed, g_webfeeds_api_impl)))
		return NULL;

	SetFeed(feed);
	// Add this to memory cache, throwing out oldest feed in memory
	g_webfeeds_api_impl->AddToInternalFeedCache(this);
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

	return feed;
}

void WebFeedsAPI_impl::WebFeedStub::SetFeed(WebFeed* feed, BOOL webfeed_init)
{
	if (feed == m_feed)
		return;

	if (m_feed)
		m_feed->DecRef();

	m_feed = feed;
	if (m_feed && !webfeed_init)
	{
		m_feed->IncRef();
		m_feed->SetStub(this);
	}
}

OP_STATUS WebFeedsAPI_impl::WebFeedStub::RemoveFeedFromMemory()
{
	if (!m_feed)  // Nothing to remove
		return OpStatus::OK;

	if (m_feed->RefCount() == 1) // We have the only reference
	{
		OP_ASSERT(g_webfeeds_api->GetFeedById(GetId()) == m_feed);  // If the feed is not in store, it should not be deleted.  Feeds only exist outside of store when being created, and then shouldn't be removed from memory

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
		// Save feed to file first, so that we can retrieve it later
		RETURN_IF_LEAVE(((WebFeedsAPI_impl*)g_webfeeds_api)->GetStorage()->SaveFeedL(m_feed));
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

		// Remove this reference to it from memory
		OP_DELETE(m_feed);
		m_feed = NULL;
	}
	else
		// Someone else has a reference to the feed, no use removing it yet.
		// But mark it for removal when the reference is gone.
		m_feed->MarkForRemoval(TRUE);

	return OpStatus::OK;
}

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
UINT WebFeedsAPI_impl::WebFeedStub::GetDiskSpaceUsed()
{
	UINT total_space = 0;

	// if we have already saved feed, check how big it was at last save
	if (m_disk_space_used)
		total_space += m_disk_space_used;
	else
	{
		// else (and this should only happen for feeds not yet loaded from disk,
		// or if having created the feed directly (i.e. not through Load))
		// check how big the file is on the disk
		if (!m_feed)
		{
			uni_char filename[16];  /* ARRAY OK 2009-02-03 arneh */
			OpFile feed_file;
			
			uni_snprintf(filename, ARRAY_SIZE(filename), UNI_L("%08x.feed"), GetId());
			filename[15] = '\0';

			OP_STATUS status = feed_file.Construct(filename, OPFILE_WEBFEEDS_FOLDER);

			if (!OpStatus::IsError(status))
			{
				OpFileLength file_length;
				if (feed_file.GetFileLength(file_length) != OpStatus::OK)
					file_length = 0;

				if (file_length >= UINT_MAX)
					return UINT_MAX;

				total_space += (UINT)file_length;
			}
			else
				OP_ASSERT(FALSE);
		}
	}

	if (HasInternalFeedIcon())
	{	
		if (m_packed.icon_disk_size)
			total_space += m_packed.icon_disk_size;
		else if (GetInternalFeedIconFilename())
		{
			OpFile icon_file;
			OP_STATUS status = icon_file.Construct(GetInternalFeedIconFilename(), OPFILE_WEBFEEDS_FOLDER);

			if (!OpStatus::IsError(status))
			{
				OpFileLength file_length;
				if (icon_file.GetFileLength(file_length) != OpStatus::OK)
					file_length = 0;

				if (file_length >= UINT_MAX)
					return UINT_MAX;

				total_space += (UINT)file_length;
			}
			else
				OP_ASSERT(FALSE);				
		}
	}	
	
	return total_space;
}

void WebFeedsAPI_impl::WebFeedStub::SaveL(WriteBuffer &buf, BOOL is_store)
{
	UINT pre_space_used = buf.GetBytesWritten();
	
	if (!is_store)
	{
		buf.Append("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		buf.Append("<!DOCTYPE feed SYSTEM \"storage.dtd\">\n");
	}

	buf.Append("<feed id=\"");
	buf.Append(GetId());

	if (!is_store)
	{
		if (!m_feed)
			GetFeed();
		if (!m_feed)
			LEAVE(OpStatus::ERR);

		buf.Append("\" next-free-entry-id=\"");
		buf.Append(m_feed->GetNextFreeEntryId());

		if (m_feed->PrefetchPrimaryWhenNewEntries())
			buf.Append("\" prefetch=\"true");
	}
	buf.Append("\"");

	if (GetUpdateInterval() != g_webfeeds_api->GetDefUpdateInterval())
	{
		buf.Append(" update-interval=\"");
		buf.Append(GetUpdateInterval());
		buf.Append("\"");		
	}

	if (GetMinUpdateInterval())
	{
		buf.Append(" min-update-interval=\"");
		buf.Append(GetMinUpdateInterval());
		buf.Append("\"");
	}
	
	if (is_store)
	{
		buf.Append(" total-entries=\"");
		buf.Append(GetTotalCount());
		buf.Append("\" unread-entries=\"");
		buf.Append(GetUnreadCount());
		buf.Append("\"");
	
		if (!m_feed_icon.IsEmpty())
		{
			buf.Append(" icon=\"");
			buf.AppendQuotedL(m_feed_icon.CStr());
			buf.Append("\"");
		}
	}
	
	buf.Append(">\n");

	if (is_store)
	{
		buf.Append("<title>");
		buf.AppendQuotedL(m_title.CStr());
		buf.Append("</title>\n");
	}
	else
	{
		buf.Append("<title>");
		((WebFeedContentElement*)m_feed->GetTitle())->SaveL(buf, FALSE);
		buf.Append("</title>\n");

		m_feed->SaveOverrideSettingsL(buf);
	}

	if (!GetURL().IsEmpty())
	{
		buf.Append("<url value=\"");
		OpString8 url;
		GetURL().GetAttributeL(URL::KName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI, url);
		buf.AppendQuotedL(url.CStr());
		buf.Append("\"/>\n");
	}

	if (GetUpdateTime() > 0.0)
	{
		buf.Append("<date value=\"");
		buf.AppendDate(GetUpdateTime());
		buf.Append("\"/>\n");
	}


	if (!is_store)
	{
		int space_available = g_webfeeds_api_impl->GetFeedDiskSpaceQuota();

		if (space_available)
		{
			space_available -= buf.GetBytesWritten() - pre_space_used;
			space_available -= GetIconDiskSpace();
		}

		m_feed->SaveEntriesL(buf, space_available);
		UINT space_used = buf.GetBytesWritten() - pre_space_used + 8;
		SetDiskSpaceUsed(space_used);
	}

	buf.Append("</feed>\n");
}
#endif // WEBFEEDS_SAVED_STORE_SUPPORT
#endif // WEBFEEDS_BACKEND_SUPPORT
