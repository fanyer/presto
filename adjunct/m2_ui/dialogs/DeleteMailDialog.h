/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DELETEMAILDIALOG_H
#define DELETEMAILDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

/***********************************************************************************
**
**  DeleteMailDialog
**
***********************************************************************************/

class DeleteMailDialog : public Dialog
{
	public:
		virtual OP_STATUS	Init(UINT32 index_id, DesktopWindow* parent_window);

		DialogType			GetDialogType()			{return TYPE_OK_CANCEL;}
		Type				GetType()				{return DIALOG_TYPE_DELETE_MAIL;}
		const char*			GetWindowName()			{return "Delete Mail Dialog";}
		const char*			GetHelpAnchor()			{return "mail.html";}
		virtual DialogImage	GetDialogImageByEnum()	{return IMAGE_WARNING;}
		BOOL				GetProtectAgainstDoubleClick() {return FALSE;}

		virtual UINT32		OnOk();
		virtual void		OnCancel() {};
		virtual BOOL		GetDoNotShowAgain() {return TRUE;}

};

#endif //SIMPLEDIALOG_H
