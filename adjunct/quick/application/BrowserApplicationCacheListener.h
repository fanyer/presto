/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef BROWSER_APPLICATION_CACHE_LISTENER
#define BROWSER_APPLICATION_CACHE_LISTENER

#include "modules/windowcommander/OpWindowCommander.h"

enum ApplicationCacheRequest
{
	Download,
	CheckForUpdate,
	UpdateCache,
	IncreaseQuota
};

enum ApplicationCacheStrategy
{
	Ask,
	Accept,
	Reject
};

class BrowserApplicationCacheListener : public OpApplicationCacheListener
{
public:
	/* events that must be answered. We trigger OP_ASSERTs if null listener is set when getting callbacks. */
	virtual void OnDownloadAppCache(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback);
	virtual void CancelDownloadAppCache(OpWindowCommander* commander, UINTPTR id);
	virtual void OnCheckForNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback);
	virtual void CancelCheckForNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id);
	virtual void OnDownloadNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback);
	virtual void CancelDownloadNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id);
	virtual void OnIncreaseAppCacheQuota(OpWindowCommander* commander, UINTPTR id, const uni_char* cache_domain, OpFileLength current_quota_size, QuotaCallback *callback);
	virtual void CancelIncreaseAppCacheQuota(OpWindowCommander* commander, UINTPTR id);

	/* notification events */
	virtual void OnAppCacheChecking(OpWindowCommander* commander){}
	virtual void OnAppCacheError(OpWindowCommander* commander){}
	virtual void OnAppCacheNoUpdate(OpWindowCommander* commander){}
	virtual void OnAppCacheDownloading(OpWindowCommander* commander){}
	virtual void OnAppCacheProgress(OpWindowCommander* commander){}
	virtual void OnAppCacheUpdateReady(OpWindowCommander* commander){}
	virtual void OnAppCacheCached(OpWindowCommander* commander){}
	virtual void OnAppCacheObsolete(OpWindowCommander* commander){}

private:
	// If a request has been answered cancel all the other toolbars of the same request
	void CancelToolbar(OpWindowCommander* commander, UINTPTR id);
	
	BOOL IsDragonFlyWindow(OpWindowCommander* commander);

	ApplicationCacheStrategy GetStrategy(OpWindowCommander* commander);
};

#endif
