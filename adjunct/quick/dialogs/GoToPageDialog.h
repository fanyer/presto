/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef GOTOPAGEDIALOG_H
#define GOTOPAGEDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

/***********************************************************************************
**
**  GoToPageDialog
**
***********************************************************************************/

class GoToPageDialog : public Dialog
{
public:
						GoToPageDialog(BOOL nick);
						~GoToPageDialog() {}

	Type				GetType()				{return DIALOG_TYPE_GO_TO_PAGE;}
	const char*			GetWindowName()			{return "Go To Page Dialog";}
	
	virtual UINT32		OnOk();
	virtual void		OnCancel() {};
	virtual void		OnInit();
	
	void				OnChange(OpWidget *widget, BOOL changed_by_mouse);
	void 				GoToPage(const OpString &candidate, OpInputAction* action);
	BOOL 				OpenNickname(const OpString& candidate,BOOL exact);

	virtual BOOL		OnInputAction(OpInputAction* action);

private:
	BOOL				m_is_nick;
};

#endif //SIMPLEDIALOG_H
