/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _FIND_TEXT_DIALOG_H
#define _FIND_TEXT_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class FindTextManager;

class FindTextDialog : public Dialog
{
public:
						FindTextDialog(const FindTextManager* manager);
	virtual				~FindTextDialog() {};

	Type				GetType()				{return DIALOG_TYPE_FIND_TEXT;}
	const char*			GetWindowName()			{return "Find Text Dialog";}
	BOOL				GetModality()			{return FALSE;}
	DialogType			GetDialogType()			{return TYPE_CLOSE;}

protected:
	virtual BOOL		HasDefaultButton()		{return TRUE;}
	virtual void		OnInit();
	virtual void		OnClick(OpWidget *widget, UINT32 id);
	virtual void		OnClose(BOOL user_initiated);

private:
	FindTextManager*	m_manager;
};

#endif //_FIND_TEXT_DIALOG_H
