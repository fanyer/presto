/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef __DESKTOPPRINTDIALOG_H__
# define __DESKTOPPRINTDIALOG_H__

# ifdef DESKTOP_PRINTDIALOG
# include "modules/windowcommander/OpWindowCommander.h"

class DesktopPrintDialogListener
{
public:
	virtual ~DesktopPrintDialogListener(){}
	virtual void DesktopPrintDialogDone() = 0;
};

class DesktopPrintDialog
{
public:
	
	virtual				~DesktopPrintDialog(){}

	static	OP_STATUS	Create(DesktopPrintDialog ** dialog, OpWindow * opwindow, PrintCallback * callback, DesktopPrintDialogListener * listener);
};

# endif // DESKTOP_PRINTDIALOG
#endif // __DESKTOPPRINTDIALOG_H__

