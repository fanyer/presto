/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Adam Minchinton and Karianne Ekern
 */
#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/dialogs/SyncShutdownProgressDialog.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "modules/locale/oplanguagemanager.h"

//////////////////////////////////////////////////////////////////////////////////////////////////

SyncShutdownProgressDialog::SyncShutdownProgressDialog() :
	m_dont_exit(FALSE)
{
	// Set the cancel button text
	g_languageManager->GetString(Str::D_SYNC_SHUTDOWN_CANCEL, m_ok_text);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

SyncShutdownProgressDialog::~SyncShutdownProgressDialog()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void SyncShutdownProgressDialog::OnInit()
{
	OpLabel* label_text = (OpLabel*)GetWidgetByName("label_text");
	if (label_text) 
		label_text->SetWrap(TRUE);

	// Grab the progress control and hide it to start
	OpProgressBar *progress = (OpProgressBar*)GetWidgetByName("server_progress_bar");
	if (progress)
	{
		progress->SetType(OpProgressBar::Spinner);
		progress->RunIndeterminateBar();
	}

#ifdef SY_CAP_SYNCLISTENER_CHANGED
	if (g_sync_coordinator)
		g_sync_coordinator->SetSyncUIListener(this);
#else
	if (g_sync_coordinator)
		g_sync_coordinator->SetSyncListener(this);
#endif
}


//////////////////////////////////////////////////////////////////////////////////////////////////

void SyncShutdownProgressDialog::OnClose(BOOL user_initiated)
{
	// Test if we should exit, no exiting if this was after an error
	if (!m_dont_exit)
	{
		// Logout so that the EXIT will work this time (this won't effect the autologin as it's not saved to the preference file)
		g_sync_manager->SetLinkEnabled(FALSE, FALSE, FALSE);
	}

	//SetLinkEnabled could cause sync error (m_dont_exit will be TRUE then), which will later cause crash, DSK-353327
	if (!m_dont_exit)
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_EXIT, 1);
	}

#ifdef SY_CAP_SYNCLISTENER_CHANGED
	if (g_sync_coordinator)
		g_sync_coordinator->RemoveSyncUIListener(this);
#else
	if (g_sync_coordinator)
		g_sync_coordinator->RemoveSyncListener(this);
#endif

	Dialog::OnClose(user_initiated);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void SyncShutdownProgressDialog::OnSyncError(OpSyncError error, const OpStringC& error_msg)
{
	m_dont_exit = TRUE;
	
	// Shut this box down if an error happens since something else will probably show up
	// Or it will tell you it had an error in the corner
	CloseDialog(FALSE, FALSE, TRUE);
}

#endif // SUPPORT_DATA_SYNC


