/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CONTACT_MANAGER_DIALOG_H
#define CONTACT_MANAGER_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class OpHotlistView;

/***********************************************************************************
**
**	ContactManagerDialog
**
***********************************************************************************/

class ContactManagerDialog : public Dialog
{
	public:
			
								ContactManagerDialog() : m_hotlist_view(NULL) {};
		virtual					~ContactManagerDialog();

		DialogType				GetDialogType()			{return TYPE_CLOSE;}
		Type					GetType()				{return DIALOG_TYPE_CONTACT_MANAGER;}
		const char*				GetWindowName()			{return "Contact Manager Dialog";}
		BOOL					GetModality()			{return FALSE;}
		const char*				GetHelpAnchor()			{return "panels.html#contacts";}

		virtual void			OnInit();

		virtual BOOL			OnInputAction(OpInputAction* action);

	private:

		OpHotlistView*			m_hotlist_view;
};

#endif //CONTACT_MANAGER_DIALOG_H
