/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef SYNC_STATUS_DIALOG_H
#define SYNC_STATUS_DIALOG_H

#ifdef SUPPORT_DATA_SYNC

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class SyncManager;

struct SyncStatusContext
{
public:
	enum SuggestedAction
	{
		ActionNone,
		ActionSyncNow,
		ActionChangeUser
	};

	OpString8			m_status_icon_name;
	OpString			m_status_text;
	SuggestedAction		m_suggested_action;
};

/***********************************************************************************
**  @class	SyncStatusDialog
**	@brief	Dialog to show the state of Link (running/error etc)
************************************************************************************/
class SyncStatusDialog : public Dialog
{
public:
	SyncStatusDialog(SyncManager & sync_manager, const SyncStatusContext & context);

	//===== OpInputContext implementations =====
	virtual Type			GetType() { return DIALOG_TYPE_SYNC_STATUS; }
	virtual BOOL			OnInputAction(OpInputAction* action);

	//===== Dialog implementations =====
	virtual void			OnInit();
	virtual DialogType		GetDialogType() {return TYPE_OK;}

	//===== DesktopWindow implementations =====
	virtual const char*		GetWindowName() { return "Sync Status Dialog"; }

private:
	SyncManager &		m_sync_manager;
	SyncStatusContext	m_context;
};

#endif // SUPPORT_DATA_SYNC

#endif // SYNC_STATUS_DIALOG_H

