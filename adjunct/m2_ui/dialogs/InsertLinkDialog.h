/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/

#ifndef INSERTLINKDIALOG_H
#define INSERTLINKDIALOG_H

#include "adjunct/m2_ui/widgets/RichTextEditor.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"

/***********************************************************************************
**
**	InsertLinkDialog
**
***********************************************************************************/

class InsertLinkDialog : public Dialog
{
public:

		void					Init(DesktopWindow * parent_window, RichTextEditor* rich_text_editor, OpString title);
		virtual					~InsertLinkDialog() {};
		
		virtual void			OnInit();
		virtual void			OnClick(OpWidget *widget, UINT32 id = 0);

		virtual DialogType				GetDialogType()			{return TYPE_OK_CANCEL;}
		virtual Type					GetType()				{return DIALOG_TYPE_INSERT_LINK;}
		virtual const char*				GetWindowName()			{return "Mail Insert Link Dialog";}
		virtual const char*				GetHelpAnchor()			{return "mail.html";}

		virtual UINT32			OnOk();

private: 
		RichTextEditor*			m_rich_text_editor;
		OpString				m_title;
};

#endif //INSERTLINKDIALOG_H
