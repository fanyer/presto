/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/

#ifndef INSERTHTMLDIALOG_H
#define INSERTHTMLDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/m2_ui/widgets/RichTextEditor.h"


/***********************************************************************************
**
**	InsertLinkDialog
**
***********************************************************************************/

class InsertHTMLDialog : public Dialog
{
public:

		void					Init(DesktopWindow* parent_window, RichTextEditor* rich_text_editor);
		virtual void			OnInit();
		virtual					~InsertHTMLDialog() {};

		virtual DialogType		GetDialogType()			{return TYPE_OK_CANCEL;}
		virtual Type			GetType()				{return DIALOG_TYPE_INSERT_HTML;}
		virtual const char*		GetWindowName()			{return "Mail Insert HTML Dialog";}
		virtual const char*		GetHelpAnchor()			{return "mail.html";}

		virtual UINT32			OnOk();

private: 
		RichTextEditor*			m_rich_text_editor;
};

#endif //INSERTHTMLDIALOG_H
