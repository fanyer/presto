// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2006 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton, Huib Kleinhout
//

#ifndef __DESKTOP_SPEEDDIAL_H__
#define __DESKTOP_SPEEDDIAL_H__

#ifdef SUPPORT_SPEED_DIAL

#include "modules/hardcore/timer/optimer.h"
#include "adjunct/quick/extensions/ExtensionUIListener.h"
#include "adjunct/quick/speeddial/SpeedDialData.h"
#include "adjunct/quick/speeddial/SpeedDialListener.h"

#include "modules/thumbnails/thumbnailmanager.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DesktopSpeedDial is a class to hold and manipulate the Speed Dial information for a single speed dial 
// across all of the speed dial pages
//
//
class DesktopSpeedDial
		: public SpeedDialData
		, public OpTimerListener
		, public ThumbnailManagerListener
		, public ExtensionDataListener
{
public:
	DesktopSpeedDial();
	virtual ~DesktopSpeedDial();

	virtual OP_STATUS SetTitle(const OpStringC& title, BOOL custom);
	virtual OP_STATUS SetURL(const OpStringC& url);
	virtual OP_STATUS SetDisplayURL(const OpStringC& display_url);

	virtual OP_STATUS Set(const SpeedDialData& original, bool retain_uid = false);
	void SetReload(const ReloadPolicy policy, const int timeout = 0, const BOOL only_if_expired = FALSE);

	// Readds the thumbnail ref after a Purge of the Thumbnail Manager
	void ReAddThumnailRef();

	// Starting and Stopping of loading of a speeddial's thumbnail
	OP_STATUS StartLoadingSpeedDial(BOOL reload, INT32 width, INT32 height) const;
	OP_STATUS StopLoadingSpeedDial() const;

	// GetLoading function
	BOOL	GetLoading() const { return m_is_loading; }
	BOOL	GetReloading() const { return m_is_reloading; }

	void SetExpired() { NotifySpeedDialExpired(); }
	void Zoom() { NotifySpeedDialZoomRequest(); }

	void OnPositionChanged() { NotifySpeedDialUIChanged(); }

	/** Hook called when the dial has been added to the Speed Dial set. */
	void OnAdded();

	/** Hook called when the dial is being removed from the Speed Dial set. */
	void OnRemoving();

	/** Hook called when speed dial manager is being destroyed, before speed dials are saved to disk. */
	void OnShutDown();
	// OpTimerListener
	void	OnTimeOut(OpTimer* timer);

	OP_STATUS AddListener(SpeedDialEntryListener& listener) const { return m_listeners.Add(&listener); }
	OP_STATUS RemoveListener(SpeedDialEntryListener& listener) const { return m_listeners.Remove(&listener); }

	OP_STATUS LoadL(PrefsFile& file, const OpStringC8& section_name);
	void SaveL(PrefsFile& file, const OpStringC8& section_name) const;

	bool HasInternalExtensionURL() const;

	// ThumbnailManagerListener
	virtual void OnThumbnailRequestStarted(const URL &document_url, BOOL reload);
	virtual void OnThumbnailReady(const URL &document_url, const Image &thumbnail, const uni_char *title, long preview_refresh);
	virtual void OnThumbnailFailed(const URL &document_url, OpLoadingListener::LoadingFinishStatus status);
	virtual void OnInvalidateThumbnails() {}

	// ExtensionDataListener
	virtual void OnExtensionDataUpdated(const OpStringC& title, const OpStringC& url);

private:
	void NotifySpeedDialDataChanged();
	void NotifySpeedDialUIChanged();
	void NotifySpeedDialExpired();
	void NotifySpeedDialZoomRequest();

	// Functions to setup and start/stop timers
	void StartReloadTimer(int timeout, BOOL only_if_expired);
	void RestartReloadTimer(const URL &document_url);
	void StopReloadTimer();

	void StartDataNotificationTimer(UINT32 timeout_in_ms);
	void StopDataNotificationTimer();

	// Function to make sure that the ThumbnailManagerListeners are only called for the correct url's
	BOOL IsMatchingCall(const URL &document_url);

	// Be careful if accessing these directly as they depend on each other
	// Use of the timer functions is recommended
	OpTimer		*m_reload_timer;			// Timer to control the reload (NULL when there is no auto reload)

	OpTimer		m_data_notification_timer;			// timer for delayed OnSpeedDialDataChanged notification
	bool		m_data_notification_timer_running;	// true if there are pending OnSpeedDialDataChanged notifications
	bool		m_extension_data_updated;			// true if extension data was updated at least once in this session
	double		m_last_data_notification;			// time of last OnSpeedDialDataChanged notification (runtime milliseconds)
	static const UINT32 EXTENSION_UPDATE_INTERVAL = 10 * 60 * 1000; // time in miliseconds between two notifications about
																	// update of extension data
	static const UINT32 EXTENSION_UPDATE_NO_DELAY_PERIOD = 1000;	// period of time after OnSpeedDialDataChanged notification
																	// when new notifications are sent without delay (time in ms)

	// The following are mutable so that Start/StopLoading() can be const.
	// Starting and stopping the loading does not affect speed dial data.

	// Flag to say if the speed dial is currently loading
	mutable BOOL m_is_loading;
	mutable BOOL m_is_reloading;
	// Set to TRUE if all speed dials are closed and a "reload every" timer goes off
	mutable BOOL m_need_reload;

	// mutable so that Add/RemoveListener() can be const.  Starting and stopping
	// listening to changes does not affect speed dial data.
	mutable OpListeners<SpeedDialEntryListener> m_listeners;
};

#endif // SUPPORT_SPEED_DIAL
#endif // __DESKTOP_SPEEDDIAL_H__
