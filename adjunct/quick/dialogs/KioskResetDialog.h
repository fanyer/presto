// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#ifndef __KIOSK_RESET_DIALOG_H__
#define __KIOSK_RESET_DIALOG_H__

#include "adjunct/quick_toolkit/widgets/Dialog.h"

#include "modules/hardcore/timer/optimer.h"

class KioskResetDialog : public Dialog
{
public:
	KioskResetDialog();
	~KioskResetDialog();

	Type				GetType()				{return DIALOG_TYPE_KIOSK_RESET;}
	const char*			GetWindowName()			{return "Kiosk Reset Dialog";}
	virtual BOOL		GetModality()			{return FALSE;}
	virtual BOOL		HasCenteredButtons()	{return TRUE;}
	virtual INT32		GetButtonCount()		{return 1;}
	
	virtual void		OnClose(BOOL user_initiated);
	virtual void		OnInit();
	void 				OnTimeOut(OpTimer* timer);
	void 				UpdateMessage();

private:
	INT32 m_secs_to_shutdown;
	OpTimer* m_timer;
};


#endif
