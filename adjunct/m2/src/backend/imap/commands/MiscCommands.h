//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// @author Arjan van Leeuwen (arjanl)
//

#ifndef MISC_COMMANDS_H
#define MISC_COMMANDS_H

#include "adjunct/m2/src/backend/imap/commands/ImapCommandItem.h"
#include "adjunct/m2/src/backend/imap/imap-flags.h"
#include "adjunct/m2/src/backend/imap/imap-protocol.h"
#include "adjunct/m2/src/backend/imap/imapmodule.h"

namespace ImapCommands
{
	/** @brief CAPABILITY command
	 */
	class Capability : public ImapCommandItem
	{
	public:
		int			NeedsState(BOOL secure, IMAP4& protocol) const { return ImapFlags::STATE_CONNECTED; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol) { return command.Append("CAPABILITY"); }
	};

	/** @brief ID command
	 */
	class ID : public ImapCommandItem
	{
	public:
		int			NeedsState(BOOL secure, IMAP4& protocol) const { return ImapFlags::STATE_RECEIVED_CAPS; }
	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);
	};

	/** @brief LIST command
	  */
	class List : public ImapCommandItem
	{
	public:
		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol)
			{ return protocol.GetBackend().ProcessConfirmedFolderList(); }

	protected:
		virtual OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.AppendFormat("LIST %s \"*\"", protocol.GetBackend().GetFolderPrefix().CStr()); }

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::FETCHING_FOLDER_LIST; }
	};

	/** @brief SpecialUseList command is either XLIST or LIST (SPECIAL-USE)
	  */
	class SpecialUseList : public List
	{
	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);

	};


	/** @brief LOGOUT command
	  */
	class Logout : public ImapCommandItem
	{
	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol) { return command.Append("LOGOUT"); }
	};


	/** @brief LSUB command
	  */
	class Lsub : public ImapCommandItem
	{
	public:
		OP_STATUS	PrepareToSend(IMAP4& protocol);

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol)
			{ return protocol.ProcessSubscribedFolderList(); }

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::FETCHING_FOLDER_LIST; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.AppendFormat("LSUB %s \"*\"", protocol.GetBackend().GetFolderPrefix().CStr()); }
	};


	/** @brief NAMESPACE command
	  */
	class Namespace : public ImapCommandItem
	{
	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol) { return command.Append("NAMESPACE"); }
	};


	/** @brief STARTTLS command
	  */
	class Starttls : public ImapCommandItem
	{
	public:
		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol)
			{ return protocol.UpgradeToTLS(); }

		int			NeedsState(BOOL secure, IMAP4& protocol) const
			{ return ImapFlags::STATE_CONNECTED | ImapFlags::STATE_RECEIVED_CAPS; }

		OP_STATUS	OnFailed(IMAP4& protocol, const OpStringC8& failed_msg)
			{ protocol.SetCapabilities(protocol.GetCapabilities() & ~ImapFlags::CAP_STARTTLS); return OpStatus::OK; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol) { return command.Append("STARTTLS"); }
	};


	/** @brief IDLE command
	 */
	class Idle : public ImapCommandItem
	{
	public:
		Idle() : m_continuation(FALSE) {}

		BOOL		IsUnnecessary(const IMAP4& protocol) const
			{ return !m_continuation && Suc() != NULL; }

		BOOL		WaitForNext() const
			{ return m_continuation; }

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol)
			{ return protocol.StopIdleTimer(); }

		OP_STATUS	PrepareContinuation(IMAP4& protocol, const OpStringC8& response_text)
			{ m_continuation = TRUE; return protocol.StartIdleTimer(); }

		BOOL		IsContinuation() const
			{ return m_continuation; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.Append(m_continuation ? "DONE" : "IDLE"); }

		BOOL		m_continuation;
	};
	
	/** @brief ENABLE command
	 */
	class EnableQResync : public ImapCommandItem
	{
	public:
		int			NeedsState(BOOL secure, IMAP4& protocol) const
			{ return GetDefaultFlags(secure, protocol) & ~ImapFlags::STATE_ENABLED_QRESYNC; }
		
		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol);
		
		OP_STATUS	OnFailed(IMAP4& protocol, const OpStringC8& failed_msg)
			{ protocol.SetCapabilities(protocol.GetCapabilities() & ~ImapFlags::CAP_QRESYNC); return OpStatus::OK; }
	
	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.Append("ENABLE QRESYNC"); }
	};

	/** @brief COMPRESS command
	  */
	class Compress : public ImapCommandItem
	{
	public:
		int			NeedsState(BOOL secure, IMAP4& protocol) const
			{ return GetDefaultFlags(secure, protocol) & ~ImapFlags::STATE_COMPRESSED; }

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol)
			{ protocol.AddState(ImapFlags::STATE_COMPRESSED); return OpStatus::OK; }

		OP_STATUS	OnFailed(IMAP4& protocol, const OpStringC8& failed_msg)
			{ protocol.SetCapabilities(protocol.GetCapabilities() & ~ImapFlags::CAP_COMPRESS_DEFLATE); return OpStatus::OK; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.Append("COMPRESS DEFLATE"); }
	};
};

#endif // MISC_COMMANDS_H
