/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef DIALOG_LISTENER_H
#define DIALOG_LISTENER_H

class Dialog;

class DialogListener
{
public:
	virtual ~DialogListener() {}
	
	/* listener gets this if dialog was ended with OK. result is that dialog's OnOkl() function returned */
	
	virtual void	OnOk(Dialog* dialog, UINT32 result) {}
	
	/* listener gets this if dialog was ended with Cancel. */
	
	virtual void	OnCancel(Dialog* dialog) {}
	
	/* listener gets this if dialog was ended with Cancel. */
	
	virtual void	OnYNCCancel(Dialog* dialog) {}
	
	/* listener gets this when dialog is being closed / destroyed */
	
	virtual void	OnClose(Dialog* dialog) {}
};

#endif // DIALOG_LISTENER_H