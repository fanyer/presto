//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// @author Arjan van Leeuwen (arjanl)
//

#ifndef AUTHENTICATION_COMMAND_H
#define AUTHENTICATION_COMMAND_H

#include "adjunct/m2/src/backend/imap/commands/ImapCommandItem.h"
#include "adjunct/m2/src/backend/imap/imap-protocol.h"

namespace ImapCommands
{
	/** @brief Authentication commands
	 */
	class Authentication : public ImapCommandItem
	{
	public:
		Authentication() : m_continuation(0) {}

		OP_STATUS	PrepareContinuation(IMAP4& protocol, const OpStringC8& response_text)
			{ m_continuation++; return OpStatus::OK; }

		BOOL		UsesPassword() const
			{ return TRUE; }

		BOOL		IsUnnecessary(const IMAP4& protocol) const
			{ return protocol.GetState() & ImapFlags::STATE_AUTHENTICATED; }

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol)
			{ protocol.AddState(ImapFlags::STATE_AUTHENTICATED); return OpStatus::OK; }

		int			NeedsState(BOOL secure, IMAP4& protocol) const
			{ return GetDefaultFlags(secure, protocol) & ~(ImapFlags::STATE_AUTHENTICATED | ImapFlags::STATE_ENABLED_QRESYNC | ImapFlags::STATE_COMPRESSED); }

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::AUTHENTICATING; }

		BOOL		IsContinuation() const
			{ return m_continuation > 0; }

		OP_STATUS	PrepareToSend(IMAP4& protocol)
			{ protocol.RemoveState(ImapFlags::STATE_RECEIVED_CAPS); return OpStatus::OK; }

		OP_STATUS	OnFailed(IMAP4& protocol, const OpStringC8& failed_msg);

		BOOL		HandleBye(IMAP4& protocol) { OpStatus::Ignore(OnFailed(protocol, "")); return TRUE; }

	protected:

		unsigned	m_continuation;
	};


	/** @brief Authenticate meta-command, use if you want to authenticate
	 */
	class Authenticate : public ImapCommandItem
	{
	public:
		BOOL		IsMetaCommand(IMAP4& protocol) const { return TRUE; }
		ImapCommandItem* GetExpandedQueue(IMAP4& protocol);
	};


	/** @brief AUTHENTICATE CRAM-MD5 command
	 */
	class AuthenticateCramMD5 : public Authentication
	{
	public:
		OP_STATUS	PrepareContinuation(IMAP4& protocol, const OpStringC8& response_text);

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);

		OpString8	m_md5_response;
	};


	/** @brief AUTHENTICATE LOGIN command
	 */
	class AuthenticateLogin : public Authentication
	{
	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);
	};


	/** @brief AUTHENTICATE PLAIN command
	 */
	class AuthenticatePlain : public Authentication
	{
	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);
	};


	/** @brief LOGIN command (plaintext login)
	 */
	class Login : public Authentication
	{
	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);
	};
};

#endif // AUTHENTICATION_COMMAND_H
