//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// @author Arjan van Leeuwen (arjanl)
//

#ifndef MAILBOX_COMMAND_H
#define MAILBOX_COMMAND_H

#include "adjunct/m2/src/backend/imap/commands/ImapCommandItem.h"
#include "adjunct/m2/src/backend/imap/commands/MiscCommands.h"
#include "adjunct/m2/src/backend/imap/imap-folder.h"
#include "adjunct/m2/src/backend/imap/imap-protocol.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/offlinelog.h"

class IMAP4;

namespace ImapCommands
{
	/** @brief Superclass for all commands that require one mailbox to operate on
	 */
	class MailboxCommand : public ImapCommandItem
	{
	public:
		/** Constructor
		  * @param mailbox Mailbox to apply action on
		  */
		MailboxCommand(ImapFolder* mailbox) : m_mailbox(mailbox) {}

		ImapFolder* GetMailbox() const { return m_mailbox; }

	protected:
		ImapFolder* m_mailbox;
	};


	/** @brief CREATE command (creating a mailbox)
	 */
	class Create : public MailboxCommand
	{
	public:
		Create(ImapFolder* mailbox) : MailboxCommand(mailbox) {}

		OP_STATUS	OnFailed(IMAP4& protocol, const OpStringC8& failed_msg);

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::CREATING_FOLDER; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.AppendFormat("CREATE %s", m_mailbox->GetQuotedName().CStr()); }
	};


	/** @brief All steps needed to safely delete a mailbox. Use this instead of using Delete directly
	 */
	class DeleteMailbox : public MailboxCommand
	{
	public:
		DeleteMailbox(ImapFolder* mailbox) : MailboxCommand(mailbox) {}

		BOOL		IsMetaCommand(IMAP4& protocol) const { return TRUE; }

		ImapCommandItem* GetExpandedQueue(IMAP4& protocol);
	};


	/** @brief DELETE command (deleting a mailbox)
	 */
	class Delete : public MailboxCommand
	{
	public:
		Delete(ImapFolder* mailbox) : MailboxCommand(mailbox) {}

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::DELETING_FOLDER; }

		ImapFolder* NeedsUnselectedMailbox() const
			{ return m_mailbox; }

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol)
			{ return protocol.GetBackend().RemoveFolder(m_mailbox); }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.AppendFormat("DELETE %s", m_mailbox->GetQuotedName().CStr()); }
	};


	/** @brief EXPUNGE command
	 */
	class Expunge : public MailboxCommand
	{
	public:
		Expunge(ImapFolder* mailbox) : MailboxCommand(mailbox) {}

		BOOL		IsSaveable() const
			{ return TRUE; }

		OP_STATUS	WriteToOfflineLog(OfflineLog& offline_log)
			{ return offline_log.EmptyTrash(FALSE); }

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::EMPTYING_TRASH; }

		ImapFolder* NeedsSelectedMailbox() const
			{ return m_mailbox; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol) { return command.Append("EXPUNGE"); }
	};


	/** @brief RENAME command (renaming a mailbox)
	 */
	class Rename : public MailboxCommand
	{
	public:
		Rename(ImapFolder* mailbox) : MailboxCommand(mailbox) {}

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::RENAMING_FOLDER; }

		ImapFolder* NeedsUnselectedMailbox() const
			{ return m_mailbox; }

		OP_STATUS OnSuccessfulComplete(IMAP4& protocol);

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.AppendFormat("RENAME %s %s", m_mailbox->GetOldName().CStr(), m_mailbox->GetNewName().CStr()); }
	private:
		OP_STATUS	ReSubscribeMailbox(ImapFolder* mailbox, IMAP4& protocol);
	};


	/** @brief SELECT command (creating a mailbox)
	 */
	class Select : public MailboxCommand
	{
	public:
		Select(ImapFolder* mailbox) : MailboxCommand(mailbox), m_is_quick_resync(FALSE) {}

		OP_STATUS	OnFailed(IMAP4& protocol, const OpStringC8& failed_msg);

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::CHECKING_FOLDER; }

		OP_STATUS	PrepareToSend(IMAP4& protocol);

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol);

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);

		BOOL		m_is_quick_resync;
	};


	/** @brief Like Select, but used when synchronizing a mailbox
	  */
	class SelectSync : public Select
	{
	public:
		SelectSync(ImapFolder* mailbox) : Select(mailbox) { m_mailbox->IncreaseSyncCount(); }
		~SelectSync() { m_mailbox->DecreaseSyncCount(); }
	};


	/** @brief EXAMINE command, like Select but read-only
	  */
	class Examine : public Select
	{
	public:
		Examine(ImapFolder* mailbox) : Select(mailbox) {}

		OP_STATUS OnSuccessfulComplete(IMAP4& protocol)
			{ return OpStatus::OK; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.AppendFormat("EXAMINE %s", m_mailbox->GetQuotedName().CStr()); }
	};


	/** @brief UNSELECT command, unselects a mailbox.
	  * Is replaced with suitable other commands if UNSELECT is not available.
	  */
	class Unselect : public MailboxCommand
	{
	public:
		Unselect(ImapFolder* mailbox) : MailboxCommand(mailbox) {}

		BOOL		IsUnnecessary(const IMAP4& protocol) const
			{ return protocol.GetCurrentFolder() != m_mailbox; }

		OP_STATUS	OnFailed(IMAP4& protocol, const OpStringC8& failed_msg)
			{ protocol.SetCurrentFolder(0); return OpStatus::OK; }

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol)
			{ protocol.SetCurrentFolder(0); return OpStatus::OK; }

		BOOL		IsMetaCommand(IMAP4& protocol) const
			{ return !(protocol.GetCapabilities() & ImapFlags::CAP_UNSELECT); }

		ImapCommandItem* GetExpandedQueue(IMAP4& protocol);

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.Append("UNSELECT"); }
	};


	/** @brief CLOSE command, closes a mailbox
	  */
	class Close : public MailboxCommand
	{
	public:
		Close(ImapFolder* mailbox) : MailboxCommand(mailbox) {}

		BOOL		IsUnnecessary(const IMAP4& protocol) const
			{ return protocol.GetCurrentFolder() != m_mailbox; }

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol)
			{ protocol.SetCurrentFolder(0); return OpStatus::OK; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.Append("CLOSE"); }
	};


	/** @brief meta-command for synchronizing a mailbox
	 */
	class Sync : public MailboxCommand
	{
	public:
		Sync(ImapFolder* mailbox) : MailboxCommand(mailbox) { m_mailbox->IncreaseSyncCount(); }
		~Sync() { m_mailbox->DecreaseSyncCount(); }

		BOOL		IsUnnecessary(const IMAP4& protocol) const { return m_mailbox->IsScheduledForMultipleSync(); }
		BOOL		IsMetaCommand(IMAP4& protocol) const { return TRUE; }
		ImapCommandItem* GetExpandedQueue(IMAP4& protocol);
	};


	/** @brief STATUS command (can be used to synchronize mailbox)
	 */
	class StatusSync : public MailboxCommand
	{
	public:
		StatusSync(ImapFolder* mailbox) : MailboxCommand(mailbox) { mailbox->IncreaseSyncCount(); }
		~StatusSync() { m_mailbox->DecreaseSyncCount(); }

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol);

		OP_STATUS	OnFailed(IMAP4& protocol, const OpStringC8& failed_msg)
			{ return m_mailbox->MarkUnselectable(); }

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::CHECKING_FOLDER; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return (protocol.GetCapabilities() & ImapFlags::CAP_QRESYNC)
					 ? command.AppendFormat("STATUS %s (MESSAGES RECENT UIDNEXT UIDVALIDITY UNSEEN HIGHESTMODSEQ)", m_mailbox->GetQuotedName().CStr())
					 : command.AppendFormat("STATUS %s (MESSAGES RECENT UIDNEXT UIDVALIDITY UNSEEN)", m_mailbox->GetQuotedName().CStr()); }
	};


	/** @brief SUBSCRIBE command (subscribing to a mailbox)
	 */
	class Subscribe : public MailboxCommand
	{
	public:
		Subscribe(ImapFolder* mailbox) : MailboxCommand(mailbox) {}

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol)
			{ return AddDependency(OP_NEW(ImapCommands::Lsub, ()), protocol); }

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::FOLDER_SUBSCRIBE; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.AppendFormat("SUBSCRIBE %s", m_mailbox->GetQuotedName().CStr()); }
	};


	/** @brief UNSUBSCRIBE command (unsubscribing from mailbox)
	 */
	class Unsubscribe : public MailboxCommand
	{
	public:
		Unsubscribe(ImapFolder* mailbox) : MailboxCommand(mailbox) {}

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol)
			{ return AddDependency(OP_NEW(ImapCommands::Lsub, ()), protocol); }

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::FOLDER_UNSUBSCRIBE; }

		ImapFolder* NeedsUnselectedMailbox() const
			{ return m_mailbox; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.AppendFormat("UNSUBSCRIBE %s", m_mailbox->GetQuotedName().CStr()); }
	};

	/** @brief special SUBSCRIBE command used when renaming a mailbox
	 */
	class SubscribeRenamed : public Subscribe
	{
	public:
		SubscribeRenamed(ImapFolder* mailbox) : Subscribe(mailbox) {}		

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol) { return OpStatus::OK; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
			{ return command.AppendFormat("SUBSCRIBE %s", m_mailbox->GetNewName().CStr()); }
	};

	
	/** @brief special UNSUBSCRIBE command used when renaming a mailbox
	 */
	class UnsubscribeRenamed : public Unsubscribe
	{
	public:
		UnsubscribeRenamed(ImapFolder* mailbox) : Unsubscribe(mailbox) {}

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol) { return m_mailbox->ApplyNewName(); }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol)
		{ return command.AppendFormat("UNSUBSCRIBE %s", m_mailbox->GetOldName().CStr()); }
	};

	/** @brief NOOP command (can be used to synchronize mailbox)
	 */
	class NoopSync : public MailboxCommand
	{
	public:
		NoopSync(ImapFolder* mailbox) : MailboxCommand(mailbox) { m_mailbox->IncreaseSyncCount(); }
		~NoopSync() { m_mailbox->DecreaseSyncCount(); }

		BOOL		IsUnnecessary(const IMAP4& protocol) const
			{ return Suc() != NULL; }

		ImapFolder* NeedsSelectedMailbox() const
			{ return m_mailbox; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol) { return command.Append("NOOP"); }
	};


	/** @brief APPEND command (to append a single message)
	 */
	class Append : public MailboxCommand
	{
	public:
		/** Constructor
		  * @param message_id Message to append
		  */
		Append(ImapFolder* mailbox, message_gid_t message_id)
			: MailboxCommand(mailbox), m_message_id(message_id), m_continuation(FALSE) {}

		OP_STATUS	PrepareContinuation(IMAP4& protocol, const OpStringC8& response_text)
			{ m_continuation = TRUE; return OpStatus::OK; }

		BOOL		IsSaveable() const
			{ return TRUE; }

		OP_STATUS	WriteToOfflineLog(OfflineLog& offline_log)
			{ return offline_log.InsertMessage(m_message_id, m_mailbox->GetIndexId()); }

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::APPENDING_MESSAGE; }

		BOOL		RemoveIfCausesDisconnect(IMAP4& protocol) const
			{ return TRUE; }

		BOOL		IsContinuation() const
			{ return m_continuation; }

		void		ResetContinuation()
			{ m_continuation = FALSE; }

		ImapFolder* NeedsUnselectedMailbox() const
			{ return m_mailbox; }

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol);

		OP_STATUS	OnAppendUid(IMAP4& protocol, unsigned uid_validity, unsigned uid);

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);

		OpString8	  m_raw_message;
		message_gid_t m_message_id;
		BOOL		  m_continuation;
	};


	/** @brief Append a message to the sent messages
	  */
	class AppendSent : public Append
	{
	public:
		AppendSent(ImapFolder* mailbox, message_gid_t message_id)
			: Append(mailbox, message_id), m_got_uid(FALSE) {}

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol);

		OP_STATUS	OnAppendUid(IMAP4& protocol, unsigned uid_validity, unsigned uid);

	protected:
		BOOL		m_got_uid;
	};
};

#endif // MAILBOX_COMMAND_H
