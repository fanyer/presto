// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton and Karianne Ekern
//

#ifndef __SYNCMAINUSERSWITCHDIALOG_H__
#define __SYNCMAINUSERSWITCHDIALOG_H__

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/sync/sync_coordinator.h"

class SyncMainUserSwitchDialog : public Dialog
{
public:
	SyncMainUserSwitchDialog(BOOL *ok);
	~SyncMainUserSwitchDialog();
  
	virtual void				OnInit();

	virtual BOOL				GetModality()			{ return TRUE; }
	virtual BOOL				GetIsBlocking() 		{ return TRUE; }
	virtual Type				GetType()				{ return DIALOG_TYPE_SYNC_MAINUSER_SWITCH; }
	virtual const char*			GetWindowName()			{ return "Sync Main User Switch Dialog"; }
	INT32						GetButtonCount()		{ return 2; };
	virtual DialogType			GetDialogType()			{return TYPE_NO_YES;}

	BOOL						OnInputAction(OpInputAction* action);
	virtual UINT32				OnOk();

private:
	BOOL	*m_ok;	// Pointer to a bool passed it that stores if OK was pressed
};

#endif // SUPPORT_DATA_SYNC
#endif // __SYNCMAINUSERSWITCHDIALOG_H__
