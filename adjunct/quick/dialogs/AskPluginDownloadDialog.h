/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ASKPLUGINDOWNLOADDIALOG_H
#define ASKPLUGINDOWNLOADDIALOG_H

#include "adjunct/quick/dialogs/SimpleDialog.h"

class AskPluginDownloadDialog : public SimpleDialog
{
	public:

		OP_STATUS			Init(DesktopWindow* parent_window, const uni_char* plugin, 
				const uni_char* downloadurl, Str::LocaleString message_format, BOOL is_widget = FALSE);

		virtual	BOOL		GetDoNotShowAgain()		{ return TRUE; }

		UINT32				OnOk();
		void				OnCancel();

	private:

		BOOL				m_is_widget;
		OpString			m_askplugindownloadurl;
};

#endif // ASKPLUGINDOWNLOADDIALOG_H
