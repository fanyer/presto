/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MAILINDEXPROPERTIES_H
#define MAILINDEXPROPERTIES_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

class MailViewDesktopWindow;

/***********************************************************************************
**
**	SearchMailDialog
**
***********************************************************************************/

class MailIndexPropertiesDialog : public Dialog
{
	public:

								MailIndexPropertiesDialog(BOOL is_first_newsfeed_edit = FALSE);

		void					Init(DesktopWindow* parent_window, int index_id, OpTreeModel* groups_model = NULL, int item_id = 0);

		DialogType				GetDialogType()			{return TYPE_OK_CANCEL;}
		Type					GetType()				{return DIALOG_TYPE_MAIL_INDEX_PROPERTIES;}
		const char*				GetWindowName()			{return "Mail Index Properties Dialog";}
		BOOL					GetModality()			{return TRUE;}
		const char*				GetHelpAnchor()			{return "mail.html";}

		virtual void			OnInit();
		virtual void			OnInitVisibility();
		virtual UINT32			OnOk();
		virtual void			OnCancel();
		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);

	private:
		int						m_index_id;
		SimpleTreeModel			m_filter_model;

		OpTreeModel*			m_groups_model;
		int						m_item_id;

		BOOL					m_is_first_newsfeed_edit;
};

#endif //MAILINDEXPROPERTIES_H
