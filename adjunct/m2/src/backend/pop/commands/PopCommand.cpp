/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/backend/pop/commands/PopCommand.h"
#include "adjunct/m2/src/backend/pop/popmodule.h"
#include "modules/locale/oplanguagemanager.h"


/***********************************************************************************
 ** Action to take when command fails - default: display error message and end
 ** session
 **
 ** PopCommand::OnFailedComplete
 ** @param error_msg Message on error
 ** @param connection POP3 connection
 ***********************************************************************************/
OP_STATUS PopCommand::OnFailedComplete(const OpStringC8& error_msg, POP3& connection)
{
	// Display an error message to the user
	OpString dialog_string;

	OpStatus::Ignore(g_languageManager->GetString(Str::S_SERVER_RESPONSE, dialog_string));
	OpStatus::Ignore(dialog_string.Append(" "));
	OpStatus::Ignore(dialog_string.Append(error_msg));

	connection.GetBackend().OnError(dialog_string);

	// End the session
	OpStatus::Ignore(connection.GetBackend().Log("POP OUT: Disconnect", ""));
	return connection.Cancel();
}

#endif // M2_SUPPORT
