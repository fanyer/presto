/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef POP_COMMAND_H
#define POP_COMMAND_H

#include "adjunct/m2/src/backend/pop/pop3-protocol.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "modules/util/simset.h"

class PopCommand : public Link
{
public:
	/** Get the POP command associated with this command
	  * @param command String where the command should be saved
	  */
	virtual OP_STATUS GetPopCommand(OpString8& command) = 0;

	/** Retrieve progress information for this command
	  * @return Progress action corresponding to this command
	  */
	virtual ProgressInfo::ProgressAction GetProgressAction() const = 0;

	/** Get the amount of finished progress units for this command
	  */
	virtual unsigned  GetProgressCount() const { return 0; }

	/** Get the total amount of finished progress units for this command
	  */
	virtual unsigned  GetProgressTotalCount() const { return 1; }

	/** @return State the parser should switch to
	  */
	virtual int		  RequiresState() const { return 0; }

	/** @return Whether this command uses passwords
	  */
	virtual BOOL	  UsesPassword() const { return FALSE; }

	/** @return Whether this command solicits a multi-line response
	  */
	virtual BOOL	  HasMultilineResponse() const { return FALSE; }

	/** Action to take before sending this command
	  */
	virtual OP_STATUS PrepareToSend(POP3& connection) { return OpStatus::OK; }

	/** Action to take when command completes successfully
	  * @param success_msg Message on success
	  * @param connection POP3 connection
	  */
	virtual OP_STATUS OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection) { return connection.ProceedToNextCommand(); }

	/** Action to take when command fails
	  * @param error_msg Message on error
	  * @param connection POP3 connection
	  */
	virtual OP_STATUS OnFailedComplete(const OpStringC8& error_msg, POP3& connection);

	/** Action to take when we received a UIDL in response to this command
	  * @param server_id Server ID received with UIDL
	  * @param uidl UIDL received
	  */
	virtual OP_STATUS OnUIDL(int server_id, const OpStringC8& uidl, POP3& connection) { return OpStatus::OK; }

	/** Action to take when we received a list response to this command
	  * @param server_id Server ID received with this list response
	  * @param size Size of message with server_id
	  */
	virtual OP_STATUS OnList(int server_id, int size, POP3& connection) { return OpStatus::OK; }

	/** Action to take when we received a continuation request
	  * @param text Text received with continuation request
	  */
	virtual OP_STATUS OnContinueReq(const OpStringC8& text, POP3& connection) { return OpStatus::OK; }

protected:
	OP_STATUS SetNextCommand(PopCommand* command)
	{
		if (!command)
			return OpStatus::ERR_NO_MEMORY;
		command->Follow(this);
		return OpStatus::OK;
	}
};

#endif // POP_COMMAND_H
