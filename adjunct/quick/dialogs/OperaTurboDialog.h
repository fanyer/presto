/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 * 
 */
#ifndef OPERA_TURBO_DIALOG_H
#define OPERA_TURBO_DIALOG_H

#ifdef WEB_TURBO_MODE

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class OperaTurboDialog : public Dialog
{
public:
	OperaTurboDialog(BOOL3 slow_network);
	virtual ~OperaTurboDialog() {}
	
	virtual DialogType	GetDialogType()			{ return TYPE_OK_CANCEL; }
	Type				GetType()				{ return DIALOG_TYPE_OPERA_TURBO; }
	const char*			GetWindowName()			{ return "Opera Turbo Dialog"; }
	virtual BOOL		HasCenteredButtons()    { return FALSE; }
	virtual BOOL		IsScalable()			{ return TRUE; }

	virtual void		OnInit();
	virtual UINT32		OnOk();
	virtual void		OnCancel();

private:
	BOOL3 m_slow_network;
};

#endif // WEB_TURBO_MODE

#endif // OPERA_TURBO_DIALOG_H
