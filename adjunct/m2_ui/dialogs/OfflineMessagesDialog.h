/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef OFFLINE_MESSAGES_DIALOG_H
#define OFFLINE_MESSAGES_DIALOG_H

#include "adjunct/quick/dialogs/SimpleDialog.h"


/** @brief Dialog to ask user whether he'd like to make existing message available offline
  */
class OfflineMessagesDialog : public SimpleDialog
{
	public:
		/** Initialization
		  * @param parent_window
		  * @param account_id ID of account that requires messages available offline
		  */
		OP_STATUS		Init(DesktopWindow* parent_window,
							 UINT16 account_id);

		// From SimpleDialog
		virtual UINT32	OnOk();

	private:
		UINT16	 m_account_id;
};

#endif // OFFLINE_MESSAGES_DIALOG_H
