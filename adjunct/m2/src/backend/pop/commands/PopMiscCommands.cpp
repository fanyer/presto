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

#include "adjunct/m2/src/backend/pop/commands/PopMiscCommands.h"

#define YY_HEADER_EXPORT_START_CONDITIONS
#define YYSTYPE void
#define YY_NO_UNISTD_H
#include "adjunct/m2/src/backend/pop/pop3-tokenizer.h"


/***********************************************************************************
 **
 **
 ** PopCommands::Stls::OnSuccessfulComplete
 ***********************************************************************************/
OP_STATUS PopCommands::Stls::OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection)
{
	RETURN_IF_ERROR(connection.UpgradeToTLS());

	// After TLS, we need a new capability check - might have changed
	RETURN_IF_ERROR(SetNextCommand(OP_NEW(PopCommands::Capa, ())));

	return PopCommand::OnSuccessfulComplete(success_msg, connection);
}


/***********************************************************************************
 **
 **
 ** PopCommands::List::OnSuccessfulComplete
 ***********************************************************************************/
int PopCommands::List::RequiresState() const
{
	return LIST;
}

#endif // M2_SUPPORT
