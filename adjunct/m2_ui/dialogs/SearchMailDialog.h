/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SEARCHMAIL_H
#define SEARCHMAIL_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class MailDesktopWindow;

/***********************************************************************************
**
**	SearchMailDialog
**
***********************************************************************************/

class SearchMailDialog : public Dialog
{
	public:

		virtual					~SearchMailDialog();

		void					Init(MailDesktopWindow* parent_window, int index_id, int search_index_id = 0);

		Type					GetType()				{return DIALOG_TYPE_SEARCH_MAIL;}
		const char*				GetWindowName()			{return "Search Mail Dialog";}
		const char*				GetHelpAnchor()			{return "mail.html#search";}

		virtual void			OnInit();
		virtual UINT32			OnOk();
		virtual void			OnCancel();
		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);

	private:

		MailDesktopWindow*		m_parent_window;
		int						m_index_id;
		int						m_search_index_id;
};

#endif //SEARCHMAIL_H
