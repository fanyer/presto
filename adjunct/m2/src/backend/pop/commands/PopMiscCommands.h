/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef POP_MISC_COMMANDS_H
#define POP_MISC_COMMANDS_H

#include "adjunct/m2/src/backend/pop/commands/PopCommand.h"
#include "adjunct/m2/src/backend/pop/commands/Capa.h"

namespace PopCommands
{
	/** @brief Stop the connection
	  */
	class Quit : public PopCommand
	{
	public:
		OP_STATUS GetPopCommand(OpString8& command) { return command.Set("QUIT\r\n"); }

		ProgressInfo::ProgressAction GetProgressAction() const { return ProgressInfo::CONTACTING; }

		OP_STATUS OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection) { return OpStatus::OK; }
	};

	/** @brief Start a secure connection
	  */
	class Stls : public PopCommand
	{
	public:
		OP_STATUS GetPopCommand(OpString8& command) { return command.Set("STLS\r\n"); }

		ProgressInfo::ProgressAction GetProgressAction() const { return ProgressInfo::CONTACTING; }

		OP_STATUS OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection);
	};

	/** @brief Request list of message sizes
	  */
	class List : public PopCommand
	{
	public:
		OP_STATUS GetPopCommand(OpString8& command) { return command.Set("LIST\r\n"); }

		ProgressInfo::ProgressAction GetProgressAction() const { return ProgressInfo::SYNCHRONIZING; }

		BOOL	  HasMultilineResponse() const { return TRUE; }

		int		  RequiresState() const;
	};
};

#endif // POP_MISC_COMMANDS_H
