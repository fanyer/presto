/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MAILSTOREUPDATEDIALOG_H
#define MAILSTOREUPDATEDIALOG_H

#ifdef M2_SUPPORT

#ifdef M2_MERLIN_COMPATIBILITY

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/m2/src/engine/store/storeupdater.h"

class ProgressInfo;

class MailStoreUpdateDialog : public Dialog, public StoreUpdateListener
{
public:
						MailStoreUpdateDialog();
						~MailStoreUpdateDialog();

	Type				GetType()				{return DIALOG_TYPE_REINDEX_MAIL;}
	const char*			GetWindowName()			{return "Mail Store Update Dialog";}
	BOOL				GetModality()			{return FALSE;}
	const uni_char*		GetOkText();

	void				OnInit();
	void				OnCancel();

	BOOL				OnInputAction(OpInputAction* action);

	// From StoreUpdateListener
	void				OnStoreUpdateProgressChanged(int progress, int total);

private:
	OpString			m_ok_text;
	BOOL				m_paused;
	double				m_start_tick;
	INT32				m_start_progress;
	StoreUpdater		m_store_updater;
};

#endif // M2_MERLIN_COMPATIBILITY
#endif // M2_SUPPORT
#endif // MAILSTOREUPDATEDIALOG_H
