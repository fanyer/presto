/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef RETR_H
#define RETR_H

#include "adjunct/m2/src/backend/pop/commands/PopCommand.h"
#include "adjunct/desktop_util/adt/opsortedvector.h"

namespace PopCommands
{
	/** @brief Generic base class for commands that affect multiple messages
	  */
	class MessageCommand : public PopCommand
	{
	public:
		/** Constructor
		  */
		MessageCommand();

		virtual OP_STATUS GetPopCommand(OpString8& command);

		virtual unsigned  GetProgressCount() const { return m_current_message; }

		virtual unsigned  GetProgressTotalCount() const { return m_messages_to_do.GetCount(); }

		virtual OP_STATUS OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection);

		/** Add a message to retrieve to this command
		  * @param server_id server ID of message to retrieve
		  * @param uidl UIDL of message to retrieve
		  */
		OP_STATUS AddMessage(int server_id, const OpStringC8& uidl);

	protected:
		struct PopMessage
		{
			PopMessage(int p_server_id, int p_size = 0) : server_id(p_server_id), size(p_size) {}

			int       server_id;
			OpString8 uidl;
			int       size;

			BOOL operator<(const PopMessage& rhs) const { return server_id < rhs.server_id; }
		};

		virtual const char* GetCommand() const = 0;

		OpAutoSortedVector<PopMessage> m_messages_to_do;
		unsigned					   m_current_message;
	};


	/** @brief Retrieve full messages (RETR)
	  */
	class Retr : public MessageCommand
	{
	public:
		virtual ProgressInfo::ProgressAction GetProgressAction() const { return ProgressInfo::FETCHING_MESSAGES; }

		virtual int		  RequiresState() const;

		virtual BOOL	  HasMultilineResponse() const { return TRUE; }

		virtual OP_STATUS PrepareToSend(POP3& connection);

		virtual OP_STATUS OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection);

		virtual OP_STATUS OnFailedComplete(const OpStringC8& error_msg, POP3& connection);

		virtual OP_STATUS OnList(int server_id, int size, POP3& connection);

	protected:
		virtual const char* GetCommand() const { return "RETR %d\r\n"; }
		virtual BOOL HasHeadersOnly() const { return FALSE; }

		OP_STATUS CreateMessage(BOOL can_be_partial, POP3& connection);
	};


	/** @brief Retrieve message headers (TOP)
	  */
	class Top : public Retr
	{
	public:
		Top() : m_lines(0) {}

		void    SetLinesToFetch(unsigned lines_to_fetch) { m_lines = lines_to_fetch; }

		virtual ProgressInfo::ProgressAction GetProgressAction() const
			{ return m_lines > 0 ? ProgressInfo::FETCHING_MESSAGES : ProgressInfo::FETCHING_HEADERS; }

		virtual OP_STATUS GetPopCommand(OpString8& command);

		virtual OP_STATUS OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection);

	protected:
		virtual BOOL HasHeadersOnly() const { return m_lines == 0; }

		unsigned m_lines;
	};


	/** @brief Remove messages (DELE)
	  */
	class Dele : public MessageCommand
	{
	public:
		Dele() : m_flushed(FALSE) {}

		virtual ProgressInfo::ProgressAction GetProgressAction() const { return ProgressInfo::DELETING_MESSAGE; }

		virtual OP_STATUS PrepareToSend(POP3& connection);

		virtual OP_STATUS OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection);

	protected:
		virtual const char* GetCommand() const { return "DELE %d\r\n"; }

		BOOL m_flushed;
	};
};

#endif // RETR_H
