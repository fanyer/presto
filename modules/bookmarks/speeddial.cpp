/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef CORE_SPEED_DIAL_SUPPORT

#include "modules/bookmarks/bookmark_manager.h"
#include "modules/bookmarks/bookmark_item.h"
#include "modules/bookmarks/speeddial.h"
#include "modules/bookmarks/speeddial_manager.h"
#include "modules/url/url_api.h"

SpeedDial::SpeedDial(unsigned int index)
	: m_index(index), m_loading(FALSE), m_reload_timer(NULL), m_storage(NULL)
{
}

SpeedDial::~SpeedDial()
{
	StopTimer();
	if (m_storage)
		m_storage->SetListener(NULL);
}

void SpeedDial::OnBookmarkDeleted()
{
	m_storage = NULL;
}

OP_STATUS SpeedDial::SetSpeedDial(const uni_char* url
#ifdef SUPPORT_DATA_SYNC
								  , BOOL from_sync /* = FALSE */
#endif // SUPPORT_DATA_SYNC
	)
{
#ifdef SUPPORT_GENERATE_THUMBNAILS
	// Stop loading the previous thumbnail
	StopLoading();
#endif

	URL old_core_url = GetCoreURL();

	if (url && *url)
	{
		RETURN_IF_ERROR(SetCoreURL(url));
	}
	else
	{
		EmptyCoreURL();
		m_storage->ClearAttributes();
#ifdef CORE_THUMBNAIL_SUPPORT
		m_thumbnail_url.Empty();
#endif
	}

#ifdef SUPPORT_DATA_SYNC
	RETURN_IF_ERROR(SpeedDialModified(old_core_url, TRUE, FALSE, from_sync));
#else
	RETURN_IF_ERROR(SpeedDialModified(old_core_url, TRUE, FALSE));
#endif // SUPPORT_DATA_SYNC
#ifdef SUPPORT_GENERATE_THUMBNAILS
	RETURN_IF_ERROR(StartLoading(FALSE));
#endif

	return OpStatus::OK;
}

void SpeedDial::SetBookmarkStorage(BookmarkItem* storage)
{
	OP_ASSERT(storage);
	m_storage = storage;
	m_storage->SetListener(this);

	/**
	 * If the storage is changed then we need the core URL to follow the change.
	 * This is the moment when the core URLs are created with SetCoreURL() while
	 * the speeddial thumbnails addresses are loaded on startup.
	 */
	URL empty_url;
	OpString storage_url_string;

	RETURN_VOID_IF_ERROR(GetStorageURLString(storage_url_string));
	RETURN_VOID_IF_ERROR(SetCoreURL(storage_url_string));
	OpStatus::Ignore(SpeedDialModified(empty_url, FALSE, FALSE));
}

OP_STATUS SpeedDial::SpeedDialModified(URL old_core_url, BOOL check_if_expired, BOOL reload
#ifdef SUPPORT_DATA_SYNC
									   , BOOL from_sync
#endif // SUPPORT_DATA_SYNC
	)
{
	URL current_url = GetCoreURL();

	// Check if the URL was really modified
	if (current_url.Id() == old_core_url.Id())
		return OpStatus::OK;

	BOOL to_empty = current_url.IsEmpty();

#ifdef SUPPORT_GENERATE_THUMBNAILS
	if (!old_core_url.IsEmpty())
	{
		g_thumbnail_manager->DelThumbnailRef(old_core_url);
	}
	if (!to_empty)
	{
		g_thumbnail_manager->AddThumbnailRef(current_url);
	}
	else
		RETURN_IF_ERROR(NotifySpeedDialRemoved());
#else // !SUPPORT_GENERATE_THUMBNAILS
	if (to_empty)
		NotifySpeedDialRemoved();
	else
		NotifySpeedDialChanged();
#endif // SUPPORT_GENERATE_THUMBNAILS

#ifdef SUPPORT_DATA_SYNC
	if (!from_sync)
	{
		if (to_empty)
		{
			m_storage->ClearSyncFlags();
			m_storage->SetDeleted(TRUE);
		}
		else if (old_core_url.IsEmpty() && m_storage->IsModified())
		{
			m_storage->SetModified(FALSE);
			m_storage->SetAdded(TRUE);
		}
		g_bookmark_manager->NotifyBookmarkChanged(m_storage);
		m_storage->ClearSyncFlags();
	}
#endif

	// Disable autoreload.
	RETURN_IF_ERROR(SetReload(FALSE, 0));
	
	// Trigger a bookmark save.
	g_bookmark_manager->SetSaveTimer();

	return OpStatus::OK;
}

#ifdef SUPPORT_GENERATE_THUMBNAILS
OP_STATUS SpeedDial::StartLoading(BOOL reload)
{
	return g_thumbnail_manager->RequestThumbnail(GetCoreURL(), reload, reload, OpThumbnail::ViewPort, SPEEDDIAL_THUMBNAIL_WIDTH, SPEEDDIAL_THUMBNAIL_HEIGHT);
}

OP_STATUS SpeedDial::StopLoading()
{
	return g_thumbnail_manager->CancelThumbnail(GetCoreURL());
}
#endif // SUPPORT_GENERATE_THUMBNAILS

OP_STATUS SpeedDial::Reload()
{
#ifdef SUPPORT_GENERATE_THUMBNAILS
	RETURN_IF_ERROR(StartLoading(TRUE));
#endif
	return OpStatus::OK;
}

BOOL SpeedDial::IsEmpty() const
{
	if (!m_storage)
		return TRUE;

	if (GetCoreURL().IsEmpty())
		return TRUE;

	return FALSE;
}

void SpeedDial::Clear(
#ifdef SUPPORT_DATA_SYNC
	BOOL from_sync /* = FALSE */
#endif // SUPPORT_DATA_SYNC
	)
{ 
	if (!IsEmpty())
#ifdef SUPPORT_DATA_SYNC
		OpStatus::Ignore(SetSpeedDial(NULL, from_sync));
#else
		OpStatus::Ignore(SetSpeedDial(NULL));
#endif // SUPPORT_DATA_SYNC
}

BOOL SpeedDial::MatchingDocument(const URL &document_url) const
{
	if (IsEmpty())
		return FALSE;

	if (GetCoreURL().Id() == document_url.Id())
		return TRUE;

	return FALSE;
}

OP_STATUS SpeedDial::SetStorageURLString(const OpStringC& url_string)
{
	BookmarkAttribute url_attr;
	RETURN_IF_ERROR(url_attr.SetTextValue(url_string));
	RETURN_IF_ERROR(m_storage->SetAttribute(BOOKMARK_URL, &url_attr));

	return OpStatus::OK;
}
	
OP_STATUS SpeedDial::GetStorageURLString(OpString &url) const
{
	BookmarkAttribute url_attr;

	if (m_storage)
	{
		RETURN_IF_ERROR(m_storage->GetAttribute(BOOKMARK_URL, &url_attr));
		RETURN_IF_ERROR(url_attr.GetTextValue(url));
	}
	return OpStatus::OK;
}

OP_STATUS SpeedDial::SetTitle(const uni_char* title) 
{
	BookmarkAttribute title_attr;
	RETURN_IF_ERROR(title_attr.SetTextValue(title));
	RETURN_IF_ERROR(m_storage->SetAttribute(BOOKMARK_TITLE, &title_attr));

	return OpStatus::OK;
}

OP_STATUS SpeedDial::GetTitle(OpString &title) const
{
	BookmarkAttribute title_attr;

	if (m_storage)
	{
		RETURN_IF_ERROR(m_storage->GetAttribute(BOOKMARK_TITLE, &title_attr));
		RETURN_IF_ERROR(title_attr.GetTextValue(title));
	}
	return OpStatus::OK;
}

OP_STATUS SpeedDial::SetReload(BOOL enable, int timeout)
{
	m_reload_timeout = timeout * 1000;
	
	if (enable)
		RETURN_IF_ERROR(StartTimer(m_reload_timeout));
	else
		StopTimer();

	return OpStatus::OK;
}

OP_STATUS SpeedDial::StartTimer(int timeout)
{
	if (!m_reload_timer)
	{
		m_reload_timer = OP_NEW(OpTimer, ());
		if (!m_reload_timer)
			return OpStatus::ERR_NO_MEMORY;

		m_reload_timer->SetTimerListener(this);
	}

	m_reload_timer->Start(timeout);

	return OpStatus::OK;
}

void SpeedDial::StopTimer()
{
	if (m_reload_timer)
	{
		m_reload_timer->Stop();
		OP_DELETE(m_reload_timer);
		m_reload_timer = NULL;
	}
}


void SpeedDial::OnTimeOut(OpTimer* timer)
{
#ifdef SUPPORT_GENERATE_THUMBNAILS
	if (timer == m_reload_timer) {
		if (OpStatus::IsError(StartLoading(TRUE)))
			return;

		OpStatus::Ignore(StartTimer(m_reload_timeout));
	}
#endif
}

void SpeedDial::AddListener(OpSpeedDialListener* l)
{
	m_listeners.Add(l);

	if (!IsEmpty())
		OpStatus::Ignore(NotifySpeedDialChanged());
}

void SpeedDial::RemoveListener(OpSpeedDialListener* l)
{
	m_listeners.Remove(l);
}

#ifdef SUPPORT_GENERATE_THUMBNAILS

void SpeedDial::OnThumbnailRequestStarted(const URL &document_url, BOOL reload)
{
	if (MatchingDocument(document_url))
	{
		m_loading = TRUE;
		NotifySpeedDialChanged();
	}
}

OP_STATUS SpeedDial::GetUrl(OpString &url) const
{
	return GetStorageURLString(url);
}

void SpeedDial::OnThumbnailReady(const URL &document_url, const Image &thumbnail, const uni_char *title, long)
{ 
	if (MatchingDocument(document_url))
	{
		ThumbnailManager::CacheEntry *cache_entry = g_thumbnail_manager->CacheLookup(document_url);

		SetThumbnail(cache_entry->filename.CStr());

#ifdef SUPPORT_DATA_SYNC
		OpString old_title;
		GetTitle(old_title);
		if (old_title.Compare(title) != 0)
		{
			SetTitle(title);
			m_storage->ClearSyncFlags();
			m_storage->SetModified(TRUE);
			m_storage->SetSyncAttribute(BOOKMARK_THUMBNAIL_FILE, TRUE);
			m_storage->SetSyncAttribute(BOOKMARK_TITLE, TRUE);
			g_bookmark_manager->NotifyBookmarkChanged(m_storage);
			m_storage->ClearSyncFlags();
		}
#else
		SetTitle(title);
#endif // SUPPORT_DATA_SYNC
		m_loading = FALSE;

		NotifySpeedDialChanged();
	}
}

OP_STATUS SpeedDial::SetCoreURL(const OpStringC& url_string)
{
	BOOL res = FALSE;
	OpString resolved_string;

	RETURN_IF_LEAVE(res = g_url_api->ResolveUrlNameL(url_string, resolved_string, TRUE));
	if (!res)
		return OpStatus::ERR;

	URL empty_url;
	m_core_url = g_url_api->GetURL(empty_url, resolved_string, TRUE);
	RETURN_IF_ERROR(SetStorageURLString(resolved_string));

	return OpStatus::OK;
}

URL SpeedDial::GetCoreURL() const
{
	return m_core_url;
}

void SpeedDial::EmptyCoreURL()
{
	URL empty_url;
	m_core_url = empty_url;
}

void SpeedDial::OnThumbnailFailed(const URL &document_url, OpLoadingListener::LoadingFinishStatus status)
{ 
	if (MatchingDocument(document_url))
	{
		m_loading = FALSE;

		OpStatus::Ignore(NotifySpeedDialChanged());
	}
}

#endif // SUPPORT_GENERATE_THUMBNAILS

OP_STATUS SpeedDial::NotifySpeedDialChanged()
{
	for (OpListenersIterator iter(m_listeners); m_listeners.HasNext(iter);)
	{
		RETURN_IF_ERROR(m_listeners.GetNext(iter)->OnSpeedDialChanged(this));
	}

	return OpStatus::OK;
}

OP_STATUS SpeedDial::NotifySpeedDialRemoved()
{
	for (OpListenersIterator iter(m_listeners); m_listeners.HasNext(iter);)
	{
		RETURN_IF_ERROR(m_listeners.GetNext(iter)->OnSpeedDialRemoved(this));
	}

	return OpStatus::OK;
}

OP_STATUS SpeedDial::NotifySpeedDialSwapped()
{
	for (OpListenersIterator iter(m_listeners); m_listeners.HasNext(iter);)
	{
		RETURN_IF_ERROR(m_listeners.GetNext(iter)->OnSpeedDialChanged(this));
	}

	return OpStatus::OK;
}

#ifdef SUPPORT_DATA_SYNC
OP_STATUS SpeedDial::SpeedDialSynced()
{
	URL empty_url;
	RETURN_IF_ERROR(SpeedDialModified(empty_url, FALSE, FALSE, TRUE));
#ifdef SUPPORT_GENERATE_THUMBNAILS
	if (g_speed_dial_manager->HasOpenPages())
		RETURN_IF_ERROR(StartLoading(FALSE));
#endif
	return OpStatus::OK;
}
#endif // SUPPORT_DATA_SYNC

#endif // CORE_SPEED_DIAL_SUPPORT
