/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SAVESESSIONDIALOG_H
#define SAVESESSIONDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

/***********************************************************************************
**
**	SaveSessionDialog
**
***********************************************************************************/

class SaveSessionDialog : public Dialog
{

	public:
								SaveSessionDialog();
		virtual					~SaveSessionDialog() {};

		Type					GetType()				{return DIALOG_TYPE_SAVE_SESSION;}
		const char*				GetWindowName()			{return "Save Session Dialog";}
		const char*				GetHelpAnchor()			{return "sessions.html";}

		UINT32					OnOk();
		void					OnInit();

		BOOL					OnInputAction(OpInputAction* action);
};

#endif // this file
