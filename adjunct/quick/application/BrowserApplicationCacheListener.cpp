/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/application/BrowserApplicationCacheListener.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/widgets/OpLocalStorageBar.h"
#include "adjunct/quick/dialogs/WebStorageQuotaDialog.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/url/url_man.h"

#define APPLICATION_CACHE_QUOTA_STEP 20*1024*1024

void BrowserApplicationCacheListener::OnDownloadAppCache(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback)
{
	ApplicationCacheStrategy strategy = GetStrategy(commander);
	DocumentDesktopWindow* win = g_application->GetDesktopWindowCollection().GetDocumentWindowFromCommander(commander);
	if (!win)
	{
		if (IsDragonFlyWindow(commander) || strategy == Accept)
		{
			callback->OnDownloadAppCacheReply(TRUE);
		}
		else
		{
			OP_ASSERT(FALSE);
			callback->OnDownloadAppCacheReply(FALSE);
		}
		return;
	}

	if (strategy == Ask)
	{
		OpLocalStorageBar* bar = win->GetLocalStorageBar();
		if (bar->GetRequestID() != id)
			bar->Show(callback, Download, id, commander->GetCurrentURL(FALSE));
	}
	else if (strategy == Accept)
	{
		callback->OnDownloadAppCacheReply(TRUE);
	}
	else
	{
		callback->OnDownloadAppCacheReply(FALSE);
	}
}

void BrowserApplicationCacheListener::CancelDownloadAppCache(OpWindowCommander* commander, UINTPTR id)
{
	CancelToolbar(commander, id);
}

void BrowserApplicationCacheListener::OnCheckForNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback)
{
	ApplicationCacheStrategy strategy = GetStrategy(commander);
	DocumentDesktopWindow* win = g_application->GetDesktopWindowCollection().GetDocumentWindowFromCommander(commander);
	if (!win)
	{
		if (IsDragonFlyWindow(commander) || strategy == Accept)
		{
			callback->OnCheckForNewAppCacheVersionReply(TRUE);
		}
		else
		{
			OP_ASSERT(FALSE);
			callback->OnCheckForNewAppCacheVersionReply(FALSE);
		}
		return;
	}

	if (strategy == Ask)
	{
		OpLocalStorageBar* bar = win->GetLocalStorageBar();
		if (bar->GetRequestID() != id)
			bar->Show(callback, CheckForUpdate, id, commander->GetCurrentURL(FALSE));
	}
	else if (strategy == Accept)
	{
		callback->OnCheckForNewAppCacheVersionReply(TRUE);
	}
	else
	{
		callback->OnCheckForNewAppCacheVersionReply(FALSE);
	}
}

void BrowserApplicationCacheListener::CancelCheckForNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id)
{
	CancelToolbar(commander, id);
}

void BrowserApplicationCacheListener::OnDownloadNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id, InstallAppCacheCallback* callback)
{
	ApplicationCacheStrategy strategy = GetStrategy(commander);
	DocumentDesktopWindow* win = g_application->GetDesktopWindowCollection().GetDocumentWindowFromCommander(commander);
	if (!win)
	{
		if (IsDragonFlyWindow(commander) || strategy == Accept)
		{
			callback->OnDownloadNewAppCacheVersionReply(TRUE);
		}
		else
		{
			OP_ASSERT(FALSE);
			callback->OnDownloadNewAppCacheVersionReply(FALSE);
		}
		return;
	}

	if (strategy == Ask)
	{
		OpLocalStorageBar* bar = win->GetLocalStorageBar();
		if (bar->GetRequestID() != id)
			bar->Show(callback, UpdateCache, id, commander->GetCurrentURL(FALSE));
	}
	else if (strategy == Accept)
	{
		callback->OnDownloadNewAppCacheVersionReply(TRUE);
	}
	else
	{
		callback->OnDownloadNewAppCacheVersionReply(FALSE);
	}
}

void BrowserApplicationCacheListener::CancelDownloadNewAppCacheVersion(OpWindowCommander* commander, UINTPTR id)
{
	CancelToolbar(commander, id);
}

void BrowserApplicationCacheListener::OnIncreaseAppCacheQuota(OpWindowCommander* commander, UINTPTR id, const uni_char* cache_domain, OpFileLength current_quota_size, QuotaCallback *callback)
{
	DocumentDesktopWindow* win = g_application->GetDesktopWindowCollection().GetDocumentWindowFromCommander(commander);
	if (!win)
	{
		if (IsDragonFlyWindow(commander))
		{
			callback->OnQuotaReply(TRUE, current_quota_size + APPLICATION_CACHE_QUOTA_STEP);
		}
		else
		{
			OP_ASSERT(FALSE);
			callback->OnQuotaReply(FALSE, current_quota_size);
		}
		return;
	}

	// Always ask even if the strategy is accept
	ApplicationCacheStrategy strategy = GetStrategy(commander);
	if (strategy == Reject)
	{
		OP_ASSERT(FALSE);
		callback->OnQuotaReply(FALSE, current_quota_size);
		return;
	}
	//else if (strategy == Accept)
	//{
	//	// silently increase the quota
	//	callback->OnQuotaReply(TRUE, current_quota_size + APPLICATION_CACHE_QUOTA_STEP);
	//}
	else
	{
		// Ask
		WebStorageQuotaDialog* dialog = OP_NEW(WebStorageQuotaDialog,(commander->GetCurrentURL(FALSE), current_quota_size, current_quota_size + APPLICATION_CACHE_QUOTA_STEP, id));
		if (!dialog)
		{
			callback->OnQuotaReply(FALSE, current_quota_size);
			return;
		}

		dialog->SetApplicationCacheCallback(callback);
		dialog->Init(win);
	}
}

void BrowserApplicationCacheListener::CancelIncreaseAppCacheQuota(OpWindowCommander* commander, UINTPTR id)
{
	OpVector<DesktopWindow> windows;
	g_application->GetDesktopWindowCollection().GetDesktopWindows(windows);
	for(UINT32 i=0; i<windows.GetCount(); i++)
	{
		DesktopWindow* win = windows.Get(i);
		for (INT32 j=0; j<win->GetDialogCount(); j++)
		{
			Dialog* dialog = win->GetDialog(j);
			if (dialog->GetType() == OpTypedObject::DIALOG_TYPE_WEBSTORAGE_QUOTA_DIALOG)
			{
				if (((WebStorageQuotaDialog*)dialog)->GetApplicationCacheCallbackID() == id)
					dialog->CloseDialog(FALSE); // Don't call cancel, the callback the dialog hold is already invalid
			}
		}
	}
}


void BrowserApplicationCacheListener::CancelToolbar(OpWindowCommander* commander, UINTPTR id)
{
	DocumentDesktopWindow* win = g_application->GetDesktopWindowCollection().GetDocumentWindowFromCommander(commander);
	if (win)
	{
		OpLocalStorageBar* bar = win->GetLocalStorageBar();
		if (bar->GetRequestID() == id)
			bar->Cancel();
	}
}

ApplicationCacheStrategy BrowserApplicationCacheListener::GetStrategy(OpWindowCommander* commander)
{
	const uni_char* url = commander->GetCurrentURL(FALSE);
	URL tmp = urlManager->GetURL(url);
	if (tmp.GetServerName())
	{
		int strategy = g_pcui->GetIntegerPref(PrefsCollectionUI::StrategyOnApplicationCache, tmp.GetServerName()->UniName());
		switch (strategy)
		{
		case 0:
			return Ask;
		case 1:
			return Accept;
		case 2:
		default:
			break;
		}
	}

	return Reject;
}

BOOL BrowserApplicationCacheListener::IsDragonFlyWindow(OpWindowCommander* commander)
{
	DesktopWindow* win = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(commander->GetOpWindow());
	return win && win->GetType() == OpTypedObject::WINDOW_TYPE_DEVTOOLS;
}
