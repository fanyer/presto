/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SYNC_OLD_PASSWORD_MISSING_DIALOG_H
#define SYNC_OLD_PASSWORD_MISSING_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "modules/hardcore/mh/mh.h"

class SyncManager;

class SyncOldPasswordMissingDialog : public Dialog
{
	class AreYouSureDialog : public SimpleDialog
	{
		SyncOldPasswordMissingDialog* m_parent;
	public:
		AreYouSureDialog(SyncOldPasswordMissingDialog* parent): m_parent(parent) {}
		OP_STATUS Init();
		UINT32 OnOk();
	};
public:
	
	SyncOldPasswordMissingDialog(SyncManager & sync_manager);
	~SyncOldPasswordMissingDialog();

	/**
	 * @see MessageObject
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/**
	 * @see OpInputContext
	 */
	virtual Type GetType() { return DIALOG_TYPE_SYNC_OLD_PASSWORD_MISSING; }
	virtual BOOL OnInputAction(OpInputAction* action);
	
	/**
	 * @see Dialog
	 */
	virtual void OnInit();
	virtual DialogType GetDialogType() { return TYPE_OK_CANCEL; }
	virtual DialogImage	GetDialogImageByEnum() { return IMAGE_WARNING; }
	virtual	const char*	GetHelpAnchor() {return "link.html";}
	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse);
	
	/**
	 * @see DesktopWindow
	 */
	virtual const char*	GetWindowName() { return "Sync Old Password Missing Dialog"; }
	
private:

	void OkClicked();
	
	void ShowError();
	void HideError();
	void ShowProgressInfo(Str::LocaleString info_id);
	void HideProgressInfo();
	void SetInProgress(BOOL in_progress);

	SyncManager& m_sync_manager;
	BOOL m_in_progress;
};

#endif // SYNC_OLD_PASSWORD_MISSING_DIALOG_H

