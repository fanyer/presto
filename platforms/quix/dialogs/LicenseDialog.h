// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#ifndef __LICENSE_DIALOG_H__
#define __LICENSE_DIALOG_H__

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class LicenseDialog : public Dialog
{
public:
    virtual void Init( INT32* result );

	Type				GetType()				{return DIALOG_TYPE_SIMPLE;}
	const char*			GetWindowName()			{return "License Dialog";}
	virtual DialogType	GetDialogType()			{return TYPE_YES_NO;}
	virtual BOOL		GetIsBlocking()			{return TRUE;}
	virtual BOOL		HasCenteredButtons()    {return FALSE;}
	virtual BOOL		IsScalable()			{return TRUE;}

	virtual	const uni_char*	GetOkText()         {return m_ok_text.CStr();}
	virtual	const uni_char*	GetCancelText()     {return m_cancel_text.CStr();}

	virtual void		OnInitVisibility();
	virtual UINT32		OnOk();

	static BOOL ShowLicense();

private:
	void 				ReadFile( OpString& buffer );

private:
	INT32*				m_result;
	OpString            m_ok_text;
	OpString            m_cancel_text;
};

#endif
