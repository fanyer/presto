/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ASK_MAX_MESSAGES_DOWNLOAD_DIALOG_H
#define ASK_MAX_MESSAGES_DOWNLOAD_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class AskMaxMessagesDownloadDialog : public Dialog
{
public:
	AskMaxMessagesDownloadDialog(int& current_max_messages, BOOL& ask_again, int available_messages, const OpStringC8& group, BOOL& ok);
	Type					GetType()				{return DIALOG_TYPE_ASK_MAX_MESSAGES_DOWNLOAD;}
	const char*				GetWindowName()			{return "Ask Max Messages Download Dialog";}
	BOOL					GetIsBlocking()			{return TRUE;}
	DialogImage				GetDialogImageByEnum()	{return IMAGE_QUESTION;}

	void					OnInit();
	UINT32					OnOk();
	void					OnCancel();

private:
	int& m_max_messages;
	int m_available_messages;
	OpString m_group;
	BOOL& m_ask_again;
	BOOL& m_ok;
};

#endif // ASK_MAX_MESSAGES_DOWNLOAD_DIALOG_H
