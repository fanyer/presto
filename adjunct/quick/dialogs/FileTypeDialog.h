/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FILETYPEDIALOG_H
#define FILETYPEDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class Viewer;

/***********************************************************************************
**
**  FiletypeDialog
**
***********************************************************************************/


class FiletypeDialog : public Dialog
{
		Viewer*						m_viewer;
		BOOL						m_is_new_entry;
		BOOL 						m_webhandler_deleted;

		void						RefreshControls();			//updates controltext dependant on others

	public:

									FiletypeDialog(Viewer* viewer, BOOL new_entry);
									~FiletypeDialog();
		virtual void				OnInit();
		virtual void				OnChange(OpWidget *widget, BOOL changed_by_mouse);
		virtual void				OnCancel();
		virtual UINT32				OnOk();
		virtual void				OnClose(BOOL user_initiated);

		virtual Type				GetType()				{return DIALOG_TYPE_FILETYPE;}
		virtual const char*			GetWindowName()			{return "Filetype Dialog";}
		virtual const char*			GetHelpAnchor()			{return "downloads.html";}

		BOOL						OnInputAction(OpInputAction* action);
};

#endif //FILETYPEDIALOG_H
