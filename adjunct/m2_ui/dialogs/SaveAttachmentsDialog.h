/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SAVE_ATTACHMENTS_DIALOG_H
#define SAVE_ATTACHMENTS_DIALOG_H

#ifdef M2_SUPPORT

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

/***********************************************************************************
**
**	SaveAttachmentsDialog
**
***********************************************************************************/

class SaveAttachmentsDialog : public Dialog
{
	public:
								SaveAttachmentsDialog(OpAutoVector<URL>*);
		virtual					~SaveAttachmentsDialog();

		void					Init(DesktopWindow* parent_window);

		Type					GetType()				{return DIALOG_TYPE_SAVE_ATTACHMENTS;}
		const char*				GetWindowName()			{return "Save Attachments Dialog";}

		virtual void			OnInit();
		virtual UINT32			OnOk();
		virtual void			OnCancel();

		OP_STATUS				PopulateAttachmentList();

	private:
		SimpleTreeModel			m_attachments_model;
		OpTreeView*				m_tree_view;
		OpAutoVector<URL>*		m_attachments;

		// flag to know whether the user is saving attachments for the first time this opera session
		static BOOL				m_new_save_session;
		void                    OnOkL();
};
#endif // SAVE_ATTACHMENTS_DIALOG_H
#endif //SAVE_ATTACHMENTS_DIALOG_H
