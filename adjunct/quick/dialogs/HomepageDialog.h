/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef HOMPAGEDIALOG_H
#define HOMPAGEDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class HomepageDialog : public Dialog
{
	OpWindowCommander*					m_current_windowcommander;
	public:

		Type				GetType()				{return DIALOG_TYPE_SET_HOMEPAGE;}
		const char*			GetWindowName()			{return "Set Homepage Dialog";}

		DEPRECATED(void Init(DesktopWindow* parent_window, Window* current_window));
		void				Init(DesktopWindow* parent_window, OpWindowCommander* current_windowcommander);
		UINT32				OnOk();
		void				OnChange(OpWidget *widget, BOOL changed_by_mouse);
		void				OnClick(OpWidget *widget, UINT32 id);
		void				OnInit();
};


#endif //this file
