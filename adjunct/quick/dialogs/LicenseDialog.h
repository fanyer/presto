/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 * 
 * Espen Sand
 * 
 */
#ifndef LICENSE_DIALOG_H
#define LICENSE_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class LicenseDialog : public Dialog
{
public:
								LicenseDialog()			{}
	virtual						~LicenseDialog()		{}
	
	virtual DialogType			GetDialogType()			{return TYPE_YES_NO;}
	Type						GetType()				{return DIALOG_TYPE_LICENSE;}
	const char*					GetWindowName()			{return "License Dialog";}
	virtual BOOL				HasCenteredButtons()    {return FALSE;}
	virtual BOOL				IsScalable()			{return TRUE;}

	virtual	Str::LocaleString	GetOkTextID();
	virtual	Str::LocaleString	GetCancelTextID();

	virtual void				OnInitVisibility();
	virtual UINT32				OnOk();
	virtual void				OnCancel();

private:
	void 						ReadFile( OpString& buffer );
	OP_STATUS					OpenLicenseFile(OpFileFolder folder, OpFile& file);

};

#endif
