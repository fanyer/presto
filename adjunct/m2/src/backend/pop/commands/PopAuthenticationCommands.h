/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef POP_AUTHENTICATION_COMMANDS_H
#define POP_AUTHENTICATION_COMMANDS_H

#include "adjunct/m2/src/backend/pop/commands/PopCommand.h"
#include "adjunct/m2/src/util/authenticate.h"

namespace PopCommands
{
	/** @brief Base class for authentication commands
	  */
	class AuthenticationCommand : public PopCommand
	{
	public:
		virtual OP_STATUS OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection);

		virtual OP_STATUS OnFailedComplete(const OpStringC8& error_msg, POP3& connection);

		virtual ProgressInfo::ProgressAction GetProgressAction() const { return ProgressInfo::AUTHENTICATING; }

	protected:
		OpAuthenticate m_info;
	};


	/** @brief AUTH CRAM-MD5 command
	  */
	class AuthenticateCramMD5 : public AuthenticationCommand
	{
	public:
		AuthenticateCramMD5() : m_second_stage(FALSE) {}

		virtual OP_STATUS GetPopCommand(OpString8& command);

		virtual OP_STATUS OnContinueReq(const OpStringC8& text, POP3& connection);

	private:
		BOOL m_second_stage;
	};


	/** @brief APOP command
	  */
	class AuthenticateApop : public AuthenticationCommand
	{
	public:
		virtual OP_STATUS PrepareToSend(POP3& connection);

		virtual OP_STATUS GetPopCommand(OpString8& command);
	};


	/** @brief USER/PASS command
	  */
	class AuthenticatePlaintext : public AuthenticationCommand
	{
	public:
		AuthenticatePlaintext() : m_second_stage(FALSE) {}

		virtual OP_STATUS PrepareToSend(POP3& connection);

		virtual OP_STATUS GetPopCommand(OpString8& command);

		virtual OP_STATUS OnSuccessfulComplete(const OpStringC8& success_msg, POP3& connection);

		virtual BOOL	  UsesPassword() const { return m_second_stage; }

	private:
		BOOL m_second_stage;
	};
};

#endif // POP_AUTHENTICATION_COMMANDS_H
