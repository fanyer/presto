/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Manuela Hutter (manuelah)
 *
 */

#ifndef __ST_AUTO_UPDATE_MANAGER_H__
#define __ST_AUTO_UPDATE_MANAGER_H__

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/quick/managers/AutoUpdateManager.h"
#include "modules/selftest/src/testutils.h"

/*************************************************************************
 **
 ** ST_AutoUpdateManager
 ** NOTE: not a subclass of the autoupdate manager, but a friend of it.
 ** (hooks into its private/protected members)
 **
 ** AutoUpdateManager needs to stay in place, as it is hooked up with its
 ** listeners, and the listeners are accessing it using the public static
 ** instance of it.
 **
 **************************************************************************/

class ST_AutoUpdateManager
{

public:
	ST_AutoUpdateManager() :
		  m_state(AutoUpdater::AUSUpToDate)
	{}

	virtual ~ST_AutoUpdateManager()
	{}

	virtual void OnUpToDate(BOOL silent = FALSE)
	{
		g_autoupdate_manager->OnUpToDate(silent);
	}

	virtual void OnChecking(BOOL silent = FALSE)
	{
		m_state = AutoUpdater::AUSChecking;
		g_autoupdate_manager->OnChecking(silent);
	}

	virtual void OnUpdateAvailable(UpdatableResource::UpdatableResourceType type, OpFileLength update_size, const uni_char* update_info_url, BOOL silent = FALSE)
	{
		m_state = AutoUpdater::AUSUpdateAvailable;
		g_autoupdate_manager->OnUpdateAvailable(type, update_size, update_info_url, silent);
	}

	virtual void OnDownloading(UpdatableResource::UpdatableResourceType type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate, BOOL silent = FALSE)
	{
		m_state = AutoUpdater::AUSDownloading;
		g_autoupdate_manager->OnDownloading(type, total_size, downloaded_size, kbps, time_estimate, silent);
	}

	virtual void OnReadyToInstallNewVersion(const OpString& version, BOOL silent = FALSE)
	{
		m_state = AutoUpdater::AUSReadyToInstall;
		g_autoupdate_manager->OnReadyToInstallNewVersion(version, silent);
	}

	virtual void OnError(AutoUpdateError error, BOOL silent = FALSE)
	{
		m_state = AutoUpdater::AUSError;
		g_autoupdate_manager->OnError(error, silent);
	}
	
	void SetUpdateMinimized(BOOL minimized = TRUE)
	{
		g_autoupdate_manager->SetUpdateMinimized(minimized);
	}

private:
	AutoUpdater::AutoUpdateState m_state;
};

#endif // AUTO_UPDATE_SUPPORT
#endif // __ST_AUTO_UPDATE_MANAGER_H__
