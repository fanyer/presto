/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef UIDL_H
#define UIDL_H

#include "adjunct/m2/src/backend/pop/commands/PopCommand.h"

namespace PopCommands
{
	class Retr;
	class Top;
	class Dele;
	class List;

	/** @brief UIDL command
	  */
	class Uidl : public PopCommand
	{
	public:
		Uidl();

		OP_STATUS GetPopCommand(OpString8& command) { return command.Set("UIDL\r\n"); }

		ProgressInfo::ProgressAction GetProgressAction() const { return ProgressInfo::SYNCHRONIZING; }

		int		  RequiresState() const;

		BOOL	  HasMultilineResponse() const { return TRUE; }

		BOOL	  MessageExpired( const OpStringC8& uidl, POP3& connection );

		OP_STATUS OnUIDL(int server_id, const OpStringC8& uidl, POP3& connection);

	private:
		template<class T> OP_STATUS AddMessageToCommand(T*& command, int server_id, const OpStringC8& uidl)
		{
			if (!command)
			{
				command = OP_NEW(T, ());
				RETURN_IF_ERROR(SetNextCommand(command));
			}
			return command->AddMessage(server_id, uidl);
		}

		Retr* m_retr_command;
		Top*  m_top_command;
		Dele* m_dele_command;
		List* m_list_command;
	};
};

#endif // UIDL_H
