/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "modules/url/protocols/commcleaner.h"
#include "modules/hardcore/mh/mh.h"

void CommCleaner::ConstructL()
{
	posted_message = FALSE;
	OP_ASSERT(NULL != g_main_message_handler);
	LEAVE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_COMM_CLEANER, 0));
	//postL();
}

CommCleaner::~CommCleaner()
{
	OP_ASSERT(NULL != g_main_message_handler);
	g_main_message_handler->UnsetCallBack(this, MSG_COMM_CLEANER, 0);
}


void CommCleaner::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	// We may now post a new message
	posted_message = FALSE;

	// Remove any Finished, but not deleted Communication objects
	Comm::CommRemoveDeletedComm();

	// Remove any Finished, but not deleted Communication objects
	Comm::TryLoadBlkWaitingComm();
	Comm::TryLoadWaitingComm(NULL);
}

void CommCleaner::SignalWaitElementActivity()
{
	TRAPD(err, postL()); 
	if (OpStatus::IsError(err)) // Keep the debugging OP_STATUS code quiet
	{
		// Restarts next time instead, As long as a failure does not repeat it is not really _that_important that it happens every tenth of a second.
		return;
	}
}

void CommCleaner::postL() 
{
	if(posted_message)
		return;

	if (!g_main_message_handler->PostDelayedMessage(MSG_COMM_CLEANER,	0, 0, 1000/10 ))
		LEAVE(OpStatus::ERR_NO_MEMORY);

	posted_message = TRUE;
}
