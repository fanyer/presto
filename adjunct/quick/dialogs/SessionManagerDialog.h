/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SESSIONMANAGERDIALOG_H
#define SESSIONMANAGERDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

/***********************************************************************************
**
**	SessionManagerDialog
**
***********************************************************************************/

class ClassicApplication;

class SessionManagerDialog : public Dialog
{
	public:
								SessionManagerDialog(ClassicApplication& application)
									: m_application(&application) {}

		virtual					~SessionManagerDialog() {}

		DialogType				GetDialogType()			{return TYPE_CLOSE;}
		Type					GetType()				{return DIALOG_TYPE_STARTUP;}
		const char*				GetWindowName()			{return "Session Manager Dialog";}

		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);

		virtual void			OnInit();
		virtual UINT32			OnOk();
		virtual void			OnCancel();

		BOOL					OnInputAction(OpInputAction* action);

	private:
		ClassicApplication*		m_application;
		SimpleTreeModel			m_sessions_model;
};

#endif
