// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton and Karianne Ekern
//

#ifndef __SYNCSHUTDOWNPROGRESSDIALOG_H__
#define __SYNCSHUTDOWNPROGRESSDIALOG_H__

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/sync/sync_coordinator.h"

class SyncShutdownProgressDialog : public Dialog,
#ifdef SY_CAP_SYNCLISTENER_CHANGED
								   public OpSyncUIListener
#else
								   public OpSyncListener
#endif
{
public:
	SyncShutdownProgressDialog();
	~SyncShutdownProgressDialog();
  
	virtual void				OnInit();

	virtual BOOL				GetModality()			{ return TRUE; }
	virtual BOOL				GetIsBlocking() 		{ return FALSE; }
	virtual Type				GetType()				{return DIALOG_TYPE_SYNC_SHUTDOWN_PROGRESS;}
	virtual const char*			GetWindowName()			{return "Sync Shutdown Progress Dialog";}

	virtual DialogType			GetDialogType()			{return TYPE_OK;}

	virtual void				OnClose(BOOL user_initiated);

	virtual	const uni_char*		GetOkText()			{ return m_ok_text.CStr();}				


#ifdef SY_CAP_SYNCLISTENER_CHANGED
	// OpSyncUIListener
	virtual void OnSyncFinished(OpSyncState& sync_state) {}
	virtual void OnSyncError(OpSyncError error, const OpStringC& error_msg);
	virtual void OnSyncStarted(BOOL items_sent) {}
	virtual void OnSyncReencryptEncryptionKeyFailed(ReencryptEncryptionKeyContext* context) { /* Empty. */ }
	virtual void OnSyncReencryptEncryptionKeyCancel(ReencryptEncryptionKeyContext* context) { /* Empty. */ }
#else
	// OpSyncListener
	void OnSyncError(OpSyncError error, OpString& error_msg);
	void OnSyncStarted(BOOL items_sending) {}
	void OnSyncCompleted(OpSyncCollection *new_items, OpSyncState& sync_state, OpSyncServerInformation& server_info) {}
	void OnSyncItemAdded() {}
#endif

private:
	OpString	m_ok_text;
	BOOL		m_dont_exit;
};

#endif // SUPPORT_DATA_SYNC
#endif // __SYNCSHUTDOWNPROGRESSDIALOG_H__
