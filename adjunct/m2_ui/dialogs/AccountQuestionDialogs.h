/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ACCOUNTQUESTIONDIALOGS_H
#define ACCOUNTQUESTIONDIALOGS_H

#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"

#include "adjunct/m2/src/engine/chatinfo.h"
// ***************************************************************************
//
//	AskServerCleanupController
//
// ***************************************************************************

class AskServerCleanupController : public SimpleDialogController
{
public:
	AskServerCleanupController(UINT16 accountid);
	virtual void OnOk();
private:
	INT32					m_account_id;
};


// ***************************************************************************
//
//	AskKickReasonDialog
//
// ***************************************************************************

class AskKickReasonDialog : public Dialog
{
public:
	// Construction / destruction.
	AskKickReasonDialog(UINT16 account_id, ChatInfo& chat_info);

	OP_STATUS Init(DesktopWindow* parent_window, const OpStringC& nick);

private:
	// Dialog overrides.
	virtual Type GetType() { return DIALOG_TYPE_KICK_CHAT_USER; }
	virtual const char* GetWindowName() { return "Kick Chat User Dialog"; }

	virtual void OnInit();
	virtual UINT32 OnOk();
	virtual void OnCancel();

	virtual BOOL GetIsBlocking() { return TRUE; }

	//  Members.
	UINT16 m_account_id;
	ChatInfo& m_chat_info;

	OpString m_nick_to_kick;
};

#endif //ACCOUNTQUESTIONDIALOGS_H
