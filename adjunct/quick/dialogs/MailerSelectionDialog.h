// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef __MAILERSELECTIONDIALOG_H__
#define __MAILERSELECTIONDIALOG_H__

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/mail/mailto.h"

class MailerSelectionDialog : public Dialog
{
	public:
		enum MailerSelection
		{
			NO_MAILER,
			SYSTEM_MAILER,    // Use system wide default mail client
			INTERNAL_MAILER,  // Use M2
			WEBMAIL_MAILER    // Use a web based mail client
		};

public:
	MailerSelectionDialog();

	OP_STATUS					Init(DesktopWindow* parent_window, const MailTo& mailto, BOOL force_background, BOOL new_window, const OpStringC* attachment);

	virtual void				OnInit();

	virtual BOOL				GetDoNotShowAgain() 		{return TRUE;}
	virtual Str::LocaleString	GetDoNotShowAgainTextID();
	virtual Type				GetType()					{return DIALOG_TYPE_MAILER_SELECTION;}
	virtual const char*			GetWindowName()				{return "Mailer Selection Dialog";}

	INT32						GetButtonCount()			{ return 2; };
	virtual void				OnChange(OpWidget *widget, BOOL changed_by_mouse);
	virtual UINT32				OnOk();

private:
	int							m_selection;
	unsigned					m_webmail_id;
	BOOL						m_force_background;
	BOOL						m_new_window;
	MailTo						m_mailto;
	OpString					m_attachment;
};

#endif // __MAILERSELECTIONDIALOG_H__
