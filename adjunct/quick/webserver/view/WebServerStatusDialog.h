/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef WEBSERVER_STATUS_DIALOG_H
#define WEBSERVER_STATUS_DIALOG_H

#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/hardcore/timer/optimer.h"

class WebServerController;

/***********************************************************************************
**  @class	WebServerStatusDialog
**	@brief	Dialog to show the state of Unite (running/error etc)
************************************************************************************/
class WebServerStatusDialog
	: public Dialog
{
public:
	static const INT32 S_NUM_MIN = 5; // show stats for the last 5 minutes

	WebServerStatusDialog(WebServerController & controller);
	virtual				~WebServerStatusDialog();

	//===== OpInputContext implementations =====
	virtual Type		GetType() { return DIALOG_TYPE_WEBSERVER_STATUS; }
	virtual BOOL		OnInputAction(OpInputAction* action);

	//===== Dialog implementations =====
	virtual void		OnInit();
	virtual DialogType	GetDialogType() {return TYPE_OK;}

	//===== DesktopWindow implementations =====
	virtual const char*	GetWindowName() { return "Webserver Status Dialog"; }

	//===== OpTimerListener implementations =====
	virtual void		OnTimeOut(OpTimer* timer);

private:
	void				UpdateStatus();

private:
	WebServerController &	m_controller;
	OpTimer *				m_timer;
};

#endif // WEBSERVER_SUPPORT

#endif // WEBSERVER_STATUS_DIALOG_H

