/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef CHANGEMASTERPASSWORDDIALOG_H
#define CHANGEMASTERPASSWORDDIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class MasterPasswordListener
{
public:
	virtual ~MasterPasswordListener() {};
	virtual void OnMasterPasswordChanged() = 0;
};

class ChangeMasterPasswordDialog : public Dialog
{
public:

	ChangeMasterPasswordDialog();
	~ChangeMasterPasswordDialog();

	Type				GetType()				{return DIALOG_TYPE_CHANGE_MASTERPASSWORD;}
	const char*			GetWindowName()			{return "Change Masterpassword Dialog";}
	const char*			GetHelpAnchor()			{return "certificates.html#master-pwd";}

	void				OnInit();
	UINT32				OnOk();
	void				OnChange(OpWidget *widget, BOOL changed_by_mouse);

	BOOL				CheckPassword(OpString* password);			//< check if password is equal to current master password
	static BOOL			CheckPasswordPolicy(OpString* password);	//< checks the password "quality"

	BOOL				OnInputAction(OpInputAction* action);
	void                BroadcastMasterPasswordChanged() { if(m_listener) m_listener->OnMasterPasswordChanged(); }
	void                AddListener(MasterPasswordListener* listener) { m_listener = listener; }

private:
	BOOL				m_ok_enabled;
	MasterPasswordListener* m_listener; 
};

#endif //CHANGEMASTERPASSWORDDIALOG_H
