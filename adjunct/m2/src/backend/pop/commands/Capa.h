/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef CAPA_H
#define CAPA_H

#include "adjunct/m2/src/backend/pop/commands/PopCommand.h"

namespace PopCommands
{
	/** @brief CAPA command that creates authentication and listing commands
	 */
	class Capa : public PopCommand
	{
	public:
		OP_STATUS GetPopCommand(OpString8& command) { return command.Set("CAPA\r\n"); }

		ProgressInfo::ProgressAction GetProgressAction() const { return ProgressInfo::CONTACTING; }

		int		  RequiresState() const;

		BOOL	  HasMultilineResponse() const { return TRUE; }

		OP_STATUS OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection);

		OP_STATUS OnFailedComplete(const OpStringC8& error_msg, POP3& connection);

	private:
		OP_STATUS CreateAuthenticationCommands(POP3& connection);

		OP_STATUS CreateListingCommand(POP3& connection);

		OP_STATUS MakeSecure(POP3& connection);
	};
};

#endif // CAPA_H
