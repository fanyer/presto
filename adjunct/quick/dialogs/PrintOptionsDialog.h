/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _PRINT_OPTIONS_DIALOG_H
#define _PRINT_OPTIONS_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class PrintOptionsDialog : public Dialog
{
public:
	Type				GetType()				{return DIALOG_TYPE_PRINT_OPTIONS;}
	const char*			GetWindowName()			{return "Print Options Dialog";}

protected:
	virtual void		OnInitVisibility();
	virtual UINT32		OnOk();
	virtual void		OnChange(OpWidget *widget, BOOL changed_by_mouse);

private:
	void SavePrintOptions();
};

#endif //_PRINT_OPTIONS_DIALOG_H
