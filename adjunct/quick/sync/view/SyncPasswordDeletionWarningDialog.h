/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SYNC_PASSWORD_DELETION_WARNING_DIALOG_H
#define SYNC_PASSWORD_DELETION_WARNING_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

#include "modules/locale/oplanguagemanager.h"

class SyncPasswordDeletionWarningDialog : public Dialog
{
public:

	enum Result
	{
		DONT_DELETE_ANYTHING,
		DELETE_ONLY_ON_THIS_COMPUTER,
		DELETE_ON_ALL_COMPUTERS
	};

	/**
	 * @see OpInputContext
	 */
	virtual Type GetType() { return DIALOG_TYPE_SYNC_PASSWORD_DELETION_WARNING; }
	virtual BOOL OnInputAction(OpInputAction* action);

	/**
	 * @see Dialog
	 */
	virtual void OnInit();
	virtual DialogType GetDialogType() { return TYPE_OK_CANCEL; }
	virtual UINT32 OnOk();
	virtual Str::LocaleString GetOkTextID();

	/**
	 * @see DesktopWindow
	 */
	virtual const char*	GetWindowName() { return "Sync Password Deletion Warning Dialog"; }
};

#endif // SYNC_PASSWORD_DELETION_WARNING_DIALOG_H

