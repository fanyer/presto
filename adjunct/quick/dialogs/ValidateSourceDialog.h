/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VALIDATESOURCEDIALOG_H
#define VALIDATESOURCEDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

extern void UploadFileForValidation(OpWindowCommander* win_comm);

class ValidateSourceDialog : public Dialog
{
	OpWindowCommander*	m_window_commander;
	int					m_settingFlags;
public:

	Type				GetType()				{return DIALOG_TYPE_VALIDATE_SOURCE;}
	const char*			GetWindowName()			{return "Validate Source Dialog";}
	BOOL				GetDoNotShowAgain()		{return TRUE;}

	void				Init(DesktopWindow* parent_window, OpWindowCommander* win_comm);
	UINT32				OnOk();
	void				OnClose(BOOL user_initiated);
	void				OnInit();
};

#endif //VALIDATESOURCEDIALOG
