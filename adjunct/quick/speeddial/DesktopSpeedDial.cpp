// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//
#include "core/pch.h"

#ifdef SUPPORT_SPEED_DIAL

#include "adjunct/quick/speeddial/DesktopSpeedDial.h"

#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/url/url_man.h"

#if defined(_MACINTOSH_) && defined(PIXEL_SCALE_RENDERING_SUPPORT)
#include "platforms/mac/util/CocoaPlatformUtils.h"
#endif // _MACINTOSH_ && PIXEL_SCALE_RENDERING_SUPPORT

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SpeedDial
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

DesktopSpeedDial::DesktopSpeedDial()	:	
	m_reload_timer(NULL),
	m_data_notification_timer_running(false),
	m_extension_data_updated(false),
	m_last_data_notification(0.0),
	m_is_loading(FALSE),
	m_is_reloading(FALSE),
	m_need_reload(FALSE)
{
	OpStatus::Ignore(g_thumbnail_manager->AddListener(this));
	m_data_notification_timer.SetTimerListener(this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

DesktopSpeedDial::~DesktopSpeedDial() 
{
	// Kill the timer on shut down if it exists
	StopReloadTimer();
	StopDataNotificationTimer();

	if (!m_core_url.IsEmpty())
		g_thumbnail_manager->DelThumbnailRef(m_core_url);

	// Kill the listener
	OpStatus::Ignore(g_thumbnail_manager->RemoveListener(this));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DesktopSpeedDial::SetTitle(const OpStringC& title, BOOL custom)
{
	if (m_title == title && m_is_custom_title == custom)
		return OpStatus::OK;

	OpString copy_of_title;
	RETURN_IF_ERROR(copy_of_title.Set(title));
	// Get rid of characters that will mess up the formatting
	RETURN_IF_ERROR(copy_of_title.ReplaceAll(UNI_L("\\t"), UNI_L("")));
	RETURN_IF_ERROR(copy_of_title.ReplaceAll(UNI_L("\\r"), UNI_L("")));
	RETURN_IF_ERROR(copy_of_title.ReplaceAll(UNI_L("\\n"), UNI_L("")));

	RETURN_IF_ERROR(SpeedDialData::SetTitle(copy_of_title, custom));

	g_speeddial_manager->PostSaveRequest();
	NotifySpeedDialDataChanged();
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DesktopSpeedDial::SetURL(const OpStringC& url)
{
	if (m_url == url)
		return OpStatus::OK;

	StopLoadingSpeedDial();
	if (!m_core_url.IsEmpty())
		g_thumbnail_manager->DelThumbnailRef(m_core_url);

	RETURN_IF_ERROR(SpeedDialData::SetURL(url));
	if (!m_core_url.IsEmpty())
		g_thumbnail_manager->AddThumbnailRef(m_core_url);

	g_speeddial_manager->PostSaveRequest();
	NotifySpeedDialDataChanged();
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DesktopSpeedDial::SetDisplayURL(const OpStringC& display_url)
{
	if (m_display_url == display_url)
		return OpStatus::OK;

	SpeedDialData::SetDisplayURL(display_url);

	g_speeddial_manager->PostSaveRequest();
	NotifySpeedDialUIChanged();
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DesktopSpeedDial::Set(const SpeedDialData& original, bool retain_uid)
{
	RETURN_IF_ERROR(SpeedDialData::Set(original, retain_uid));
	// Restart or stop timer
	SetReload(original.GetReloadPolicy(), original.GetReloadTimeout(), original.GetReloadOnlyIfExpired());
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::OnAdded()
{
	if (GetExtensionWUID().HasContent() && g_desktop_extensions_manager)
		OpStatus::Ignore(g_desktop_extensions_manager->SetExtensionDataListener(
					GetExtensionWUID(), this));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::OnRemoving()
{
	StopDataNotificationTimer();

	if (GetExtensionWUID().HasContent() && g_desktop_extensions_manager)
		OpStatus::Ignore(g_desktop_extensions_manager->SetExtensionDataListener(
					GetExtensionWUID(), NULL));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::OnShutDown()
{
	if (m_data_notification_timer_running)
	{
		// It is too late to notify listeners (there shouldn't be any at this point), so just
		// notify manager that speed dials need to be saved.
		g_speeddial_manager->PostSaveRequest();
		StopDataNotificationTimer();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::SetReload(const ReloadPolicy policy, const int timeout, const BOOL only_if_expired)
{
	ReloadPolicy pol = policy;

	if (timeout > 0 && (pol == SpeedDialData::Reload_PageDefined || pol == SpeedDialData::Reload_UserDefined))
	{
		// If synching over link does not send us policy information (because that is a new property in 11.10) then
		// the timeout can be SPEED_DIAL_RELOAD_INTERVAL_DEFAULT with a policy of SpeedDialData::Reload_UserDefined
		// which means user selected "Never" in menu.
		if (timeout == SPEED_DIAL_RELOAD_INTERVAL_DEFAULT)
		{
			StopReloadTimer();
			pol = SpeedDialData::Reload_NeverHard;
		}
		else
		{
			StartReloadTimer(timeout, only_if_expired);
		}
	}
	else
	{
		StopReloadTimer();
	}
	//save new reload information
	SpeedDialData::SetReload(pol, timeout, only_if_expired);
	g_speeddial_manager->PostSaveRequest();

	// Only ever sends a changed since the SetSpeedDial will be the thing that
	// will do the add/delete calls, this is extra information for an 
	// active speed dial
	NotifySpeedDialDataChanged();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::StartReloadTimer(int timeout, BOOL only_if_expired)
{
	// extensions should not be reloaded by the timer (DSK-369019)
	if (GetExtensionWUID().HasContent())
		return;

	// Make sure we have a timeout
	if (timeout <= 0)
		timeout = SPEED_DIAL_ENABLED_RELOAD_INTERVAL_DEFAULT;

	m_timeout = timeout;
	m_only_if_expired = only_if_expired;
	
	if (!m_reload_timer)
	{
		if (!(m_reload_timer = OP_NEW(OpTimer, ())))
			return;
			
		m_reload_timer->SetTimerListener(this);
	}
	// This is in ms
	m_reload_timer->Start(m_timeout * 1000);
	
	// Set the timer running flag
	m_reload_timer_running = TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::RestartReloadTimer(const URL &document_url)
{
	// Check that we are restarting the timer for the matching URL and that there is a timer
	if (m_reload_timer && !m_reload_timer_running && IsMatchingCall(document_url))
	{
		// If there are no Speed dials showing, postpone reloading until it is shown again
		if (g_speeddial_manager->GetSpeedDialTabsCount())
		{
			int time_since_firing = max(m_reload_timer->TimeSinceFiring(), 0);
			int time_to_wait = m_timeout * 1000 - time_since_firing;
			m_reload_timer->Start(max(time_to_wait, 0));
		}
		else
		{
			m_need_reload = TRUE;
		}

		// Set the timer running flag
		m_reload_timer_running = TRUE;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::StopReloadTimer()
{
	if (m_reload_timer)
	{
		m_reload_timer->Stop();
		OP_DELETE(m_reload_timer);
		m_reload_timer = NULL;
	}

	// Set the timer running flag
	m_reload_timer_running = FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::StartDataNotificationTimer(UINT32 timeout_in_ms)
{
	m_data_notification_timer.Start(timeout_in_ms);
	m_data_notification_timer_running = true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::StopDataNotificationTimer()
{
	if (m_data_notification_timer_running)
	{
		m_data_notification_timer.Stop();
		m_data_notification_timer_running = false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::OnTimeOut(OpTimer* timer)
{
	if (m_reload_timer == timer)
	{
		m_reload_timer_running = FALSE;
		SetExpired();
	}
	else if (&m_data_notification_timer == timer)
	{
		g_speeddial_manager->PostSaveRequest();
		NotifySpeedDialDataChanged();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DesktopSpeedDial::LoadL(PrefsFile& file, const OpStringC8& section_name)
{
	BOOL custom = file.ReadIntL(section_name, SPEEDDIAL_KEY_CUSTOM_TITLE, FALSE);
	RETURN_IF_ERROR(SetTitle(file.ReadStringL(section_name, SPEEDDIAL_KEY_TITLE), custom));
	RETURN_IF_ERROR(SetURL(file.ReadStringL(section_name, SPEEDDIAL_KEY_URL)));
	RETURN_IF_ERROR(SetDisplayURL(file.ReadStringL(section_name, SPEEDDIAL_KEY_DISPLAY_URL)));
	RETURN_IF_ERROR(SetExtensionID(file.ReadStringL(section_name, SPEEDDIAL_KEY_EXTENSION_ID)));
	RETURN_IF_ERROR(m_partner_id.Set(file.ReadStringL(section_name, SPEEDDIAL_KEY_PARTNER_ID)));
	/* Read the auto reload params */
	SpeedDialData::ReloadPolicy reload_policy = SpeedDialData::Reload_NeverSoft;
	if (file.IsKey(section_name, SPEEDDIAL_KEY_RELOAD))
	{
		// Upgrade. SPEEDDIAL_KEY_RELOAD will not be written to file again. It can be 0 or 1
		int flag;
		flag = file.ReadIntL(section_name, SPEEDDIAL_KEY_RELOAD, SPEED_DIAL_RELOAD_ENABLED_DEFAULT);
		if (flag == 1)
		{
			int timeout;
			timeout = file.ReadIntL(section_name, SPEEDDIAL_KEY_RELOAD_INTERVAL, SPEED_DIAL_RELOAD_INTERVAL_DEFAULT);
			if (timeout == SPEED_DIAL_RELOAD_INTERVAL_DEFAULT)
				reload_policy = SpeedDialData::Reload_NeverHard;
			else
				reload_policy = SpeedDialData::Reload_UserDefined;
		}
		else
			reload_policy = SpeedDialData::Reload_NeverSoft;
	}
	else
	{
		int mode;
		mode = file.ReadIntL(section_name, SPEEDDIAL_KEY_RELOAD_POLICY, SpeedDialData::Reload_NeverSoft);
		switch(mode)
		{
			case Reload_NeverSoft:
			case Reload_UserDefined:
			case Reload_PageDefined:
			case Reload_NeverHard:
				reload_policy = (SpeedDialData::ReloadPolicy)mode;
		}
	}

	SetReload(reload_policy,
			  file.ReadIntL(section_name, SPEEDDIAL_KEY_RELOAD_INTERVAL, SPEED_DIAL_RELOAD_INTERVAL_DEFAULT),
			  file.ReadIntL(section_name, SPEEDDIAL_KEY_RELOAD_ONLY_IF_EXPIRED, SPEED_DIAL_RELOAD_ONLY_IF_EXPIRED_DEFAULT));
	RETURN_IF_ERROR(m_unique_id.Set(file.ReadStringL(section_name, SPEEDDIAL_KEY_ID).CStr()));

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::SaveL(PrefsFile& file, const OpStringC8& section_name) const
{
	file.WriteStringL(section_name, SPEEDDIAL_KEY_TITLE, m_title.CStr());
	file.WriteIntL(section_name, SPEEDDIAL_KEY_CUSTOM_TITLE, m_is_custom_title);
	file.WriteStringL(section_name, SPEEDDIAL_KEY_URL, m_url.CStr());
	if (m_display_url.HasContent())
	{
		file.WriteStringL(section_name, SPEEDDIAL_KEY_DISPLAY_URL, m_display_url.CStr());
	}
	if (m_extension_id.HasContent())
	{
		file.WriteStringL(section_name,SPEEDDIAL_KEY_EXTENSION_ID,m_extension_id.CStr());
	}
	if (m_partner_id.HasContent())
	{
		file.WriteStringL(section_name, SPEEDDIAL_KEY_PARTNER_ID, m_partner_id.CStr());
	}
	file.WriteIntL(section_name, SPEEDDIAL_KEY_RELOAD_POLICY, GetReloadPolicy());
	if (m_unique_id.HasContent())
	{
		file.WriteStringL(section_name, SPEEDDIAL_KEY_ID, m_unique_id.CStr());
	}
	file.WriteIntL(section_name, SPEEDDIAL_KEY_RELOAD_INTERVAL, GetReloadTimeout());
	file.WriteIntL(section_name, SPEEDDIAL_KEY_RELOAD_ONLY_IF_EXPIRED, GetReloadOnlyIfExpired());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::NotifySpeedDialDataChanged()
{
	StopDataNotificationTimer();
	m_last_data_notification = g_op_time_info->GetRuntimeMS();

	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnSpeedDialDataChanged(*this);

	// If the data changed, then we always notify the UI as well.
	NotifySpeedDialUIChanged();
}

void DesktopSpeedDial::NotifySpeedDialUIChanged()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnSpeedDialUIChanged(*this);
}

void DesktopSpeedDial::NotifySpeedDialExpired()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnSpeedDialExpired();
}

void DesktopSpeedDial::NotifySpeedDialZoomRequest()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnSpeedDialEntryScaleChanged();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::ReAddThumnailRef()
{
	// This function should only ever be called after a Purge of the Thumbnail Manager

	// If this speeddial has a url then readd the thumbnail ref.
	if (!m_core_url.IsEmpty())
		g_thumbnail_manager->AddThumbnailRef(m_core_url);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DesktopSpeedDial::StartLoadingSpeedDial(BOOL reload, INT32 width, INT32 height) const
{
	if (width <= 0 || height <= 0)
		return OpStatus::ERR;

	BOOL do_reload = reload;

	// StopLoadingSpeedDial() will cancel an ongoing reload
	if (m_is_reloading)
		do_reload = TRUE;

	RETURN_IF_ERROR(StopLoadingSpeedDial());

	OpThumbnail::ThumbnailMode mode = OpThumbnail::SpeedDial;
	ReloadPolicy reload_policy = GetReloadPolicy();

	if (Reload_UserDefined == reload_policy || Reload_PageDefined == reload_policy)
		mode = OpThumbnail::SpeedDialRefreshing;

	// If the timer stopped while all the speed dial tabs were closed and
	// this speed dial has reload every enabled, and this wasn't a forced
	// reload then we need to force a reload now
	if (m_need_reload && GetReloadEnabled())
		do_reload = TRUE;

	// This is needed so that we can collect Preview-Refresh data from http header. 
	// The value, if any, is used to automatically set reload interval.

	// In case the user has changed the aspect ratio, reconfigure the thumbnail manager.
	g_speeddial_manager->ConfigureThumbnailManager();

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	INT32 scale = static_cast<DesktopOpSystemInfo*>(g_op_system_info)->GetMaximumHiresScaleFactor();
	PixelScaler scaler(scale);
	width = TO_DEVICE_PIXEL(scaler, width);
	height = TO_DEVICE_PIXEL(scaler, height);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	RETURN_IF_ERROR(g_thumbnail_manager->RequestThumbnail(m_core_url,
			do_reload,
			do_reload,
			mode,
			width,
			height
			)
	);

	// Always make sure this is reset after a load is called
	m_need_reload = FALSE;

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DesktopSpeedDial::StopLoadingSpeedDial() const
{
	if (!m_is_loading)
		return OpStatus::OK;

	RETURN_IF_ERROR(g_thumbnail_manager->CancelThumbnail(m_core_url));
	
	m_is_loading = FALSE;
	m_is_reloading = FALSE;

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DesktopSpeedDial::HasInternalExtensionURL() const
{
	return GetExtensionID().HasContent() && (GetURL().Find("widget://") != KNotFound ) ? true : false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::OnThumbnailRequestStarted(const URL &document_url, BOOL reload) 
{	
	// Only process the request for matching calls
	if (!IsMatchingCall(document_url))
		return;

	m_is_loading = TRUE;
	m_is_reloading = reload;
	NotifySpeedDialUIChanged();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::OnThumbnailReady(const URL &document_url, const Image &thumbnail, const uni_char *title, long preview_refresh) 
{ 
	// Only process the request for matching calls
	if (!IsMatchingCall(document_url))
		return;

	// these must be set before NotifySpeedDial*Changed() is called
	m_is_loading = FALSE;
	m_is_reloading = FALSE;

	// If the title has changed set the data and save to the file
	// This is need for any typed in entries because the title isn't retieved until the url is downloaded
	if (GetTitle() != title && !m_is_custom_title)
	{
		SetTitle(title, m_is_custom_title); // notifies of data change (and thus UI change)
		OpStatus::Ignore(g_speeddial_manager->Save());
	}
	else
	{
		// no data changed, but still notify UI change
		NotifySpeedDialUIChanged();
	}

	// The Preview-refresh value handling must be updated if the value changes
	m_preview_refresh = (int)preview_refresh;
	if (m_preview_refresh > 0 && m_reload_policy == SpeedDialData::Reload_NeverSoft && !GetReloadEnabled())
	{
		SetReload(SpeedDialData::Reload_PageDefined, m_preview_refresh, GetReloadOnlyIfExpired());
		OpStatus::Ignore(g_speeddial_manager->Save());
	}
	else if (m_preview_refresh <= 0 && m_reload_policy == SpeedDialData::Reload_PageDefined && GetReloadEnabled())
	{
		SetReload(SpeedDialData::Reload_NeverSoft, GetReloadTimeout(), GetReloadOnlyIfExpired());
		OpStatus::Ignore(g_speeddial_manager->Save());
	}
	else
	{
		RestartReloadTimer(document_url);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::OnThumbnailFailed(const URL &document_url, OpLoadingListener::LoadingFinishStatus status) 
{ 
	// Only process the request for matching calls
	if (!IsMatchingCall(document_url))
		return;

	m_is_loading = FALSE;
	m_is_reloading = FALSE;
	NotifySpeedDialUIChanged();

	RestartReloadTimer(document_url); 
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopSpeedDial::OnExtensionDataUpdated(const OpStringC& title, const OpStringC& url)
{
	bool changed = false;
	bool force_notification = !m_extension_data_updated;
	if (title != m_title)
	{
		if (m_title.IsEmpty())
		{
			force_notification = true;
		}
		if (!m_is_custom_title && OpStatus::IsSuccess(m_title.Set(title)))
		{
			changed = true;
		}
	}
	else if (url != m_url)
	{
		if (m_url.IsEmpty())
		{
			force_notification = true;
		}
		if (OpStatus::IsSuccess(m_url.Set(url)))
		{
			changed = true;
		}
	}

	if (changed)
	{
		// Extension can change its title and/or fallback-url very frequently, which
		// is not very useful and can lead to race conditions on the Link server
		// (if server receives updated data for extension that doesn't exist then
		// it will add that extension, which makes it difficult to delete extension
		// that constantly updates itself on some other instance of Opera).
		// To mitigage this OnSpeedDialDataChanged is delayed if:
		// - less than EXTENSION_UPDATE_INTERVAL seconds passed since last notification
		// - and at least EXTENSION_UPDATE_NO_DELAY_PERIOD passed since last notification.
		// The second condition is to ensure that if extension changes url and title at
		// the same time, which will result in two calls to this function, then either
		// both notifications are sent immediately or both are delayed.
		// UI notifications are always sent immediately.
 
		if (force_notification)
		{
			StopDataNotificationTimer();
		}
		else if (!m_data_notification_timer_running)
		{
			double now = g_op_time_info->GetRuntimeMS();
			if (now >= m_last_data_notification + EXTENSION_UPDATE_NO_DELAY_PERIOD &&
				now - m_last_data_notification <= EXTENSION_UPDATE_INTERVAL)
			{
				StartDataNotificationTimer(EXTENSION_UPDATE_INTERVAL - (now - m_last_data_notification));
			}
		}
		if (m_data_notification_timer_running)
		{
			NotifySpeedDialUIChanged();
		}
		else
		{
			g_speeddial_manager->PostSaveRequest();
			NotifySpeedDialDataChanged();
		}
		m_extension_data_updated = true;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL DesktopSpeedDial::IsMatchingCall(const URL &document_url)
{
	// Only process the request for matching calls
	return !m_core_url.IsEmpty() && (m_core_url.Id() == document_url.Id());
}

#endif // SUPPORT_SPEED_DIAL
