//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// @author Arjan van Leeuwen (arjanl)
//

#ifndef MESSAGE_SET_COMMAND_H
#define MESSAGE_SET_COMMAND_H

#include "adjunct/m2/src/backend/imap/commands/ImapCommandItem.h"
#include "adjunct/m2/src/backend/imap/commands/MessageSet.h"
#include "adjunct/m2/src/backend/imap/commands/MailboxCommand.h"
#include "adjunct/m2/src/backend/imap/imap-folder.h"

namespace ImapCommands
{
	enum MessageSetFlags
	{
		USE_UID			= 1 << 16
	};

	/** @brief Superclass for all commands that require a message set / sequence to operate
	 */
	class MessageSetCommand : public MailboxCommand
	{
	public:
		/** Constructor
		  * @param mailbox Mailbox to apply action on
		  * @param id_from ID to start at
		  * @param id_to   ID to end at (inclusive)
		  * @param flags   Flags for command
		  */
		MessageSetCommand(ImapFolder* mailbox, unsigned id_from, unsigned id_to, int flags)
			: MailboxCommand(mailbox), m_message_set(id_from, id_to), m_flags(flags) {}

		BOOL		IsUnnecessary(const IMAP4& protocol) const
			{ return m_message_set.IsInvalid(); }

		BOOL		CanExtendWith(const MessageSetCommand* command) const;

		OP_STATUS	ExtendWith(const MessageSetCommand* command, IMAP4& protocol);

		OP_STATUS	ExtendSequenceByRange(UINT32 range_start, UINT32 range_end)
			{ return m_message_set.InsertRange(range_start, range_end); }

		OP_STATUS	OnExpunge(unsigned expunged_server_id)
			{ return !(m_flags & USE_UID) ? m_message_set.Expunge(expunged_server_id) : OpStatus::OK; }

		unsigned	GetProgressTotalCount() const
			{ return m_message_set.Count(); }

		ImapFolder* NeedsSelectedMailbox() const
			{ return m_mailbox; }

		MessageSet& GetMessageSet()
			{ return m_message_set; }

	protected:
		enum ExtendableCommandType
		{
			NOT_EXTENDABLE,
			FETCH,
			STORE,
			COPY,
			MOVE,
			MESSAGE_DELETE,
			MESSAGE_UNDELETE,
			MESSAGE_SPAM,
			MESSAGE_NOT_SPAM
		};

		virtual ExtendableCommandType GetExtendableType() const { return NOT_EXTENDABLE; }
		OP_STATUS   PrepareSetForOffline(OpINT32Vector& message_ids);

		MessageSet   m_message_set;
		int			 m_flags;
	};


	/** @brief COPY command, always uses UID
	  */
	class Copy : public MessageSetCommand
	{
	public:
		Copy(ImapFolder* from_mailbox, ImapFolder* to_mailbox, unsigned id_from, unsigned id_to)
			: MessageSetCommand(from_mailbox, id_from, id_to, USE_UID), m_to_mailbox(to_mailbox) {}

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol);

		BOOL		IsSaveable() const
			{ return TRUE; }

		OP_STATUS	WriteToOfflineLog(OfflineLog& offline_log);

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::APPENDING_MESSAGE; }

		OP_STATUS	OnCopyUid(IMAP4& protocol, unsigned uid_validity, OpINT32Vector& source_set, OpINT32Vector& dest_set);

	protected:
		ExtendableCommandType GetExtendableType() const { return COPY; }
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);

		ImapFolder* m_to_mailbox;
		OpAutoINT32HashTable<message_gid_t> m_uid_m2id_map;
	};

	/** @brief meta-command for moving message (there's no MOVE command in IMAP), always uses UID
	  */
	class Move : public MessageSetCommand
	{
	public:
		Move(ImapFolder* from_mailbox, ImapFolder* to_mailbox, unsigned id_from, unsigned id_to)
			: MessageSetCommand(from_mailbox, id_from, id_to, USE_UID), m_to_mailbox(to_mailbox) {}

		BOOL		IsMetaCommand(IMAP4& protocol) const { return TRUE; }

		BOOL		IsSaveable() const { return TRUE; }

		OP_STATUS	WriteToOfflineLog(OfflineLog& offline_log);

		ImapCommandItem* GetExpandedQueue(IMAP4& protocol);

	protected:
		ExtendableCommandType GetExtendableType() const { return MOVE; }

		ImapFolder* m_to_mailbox;
	};

	
	/** @brief MoveCopy, special Copy command used for Move 
	  * It moves the message locally when the copy is done on the server
	  */
	class MoveCopy : public Copy
	{
	public:
		MoveCopy(ImapFolder* from_mailbox, ImapFolder* to_mailbox, unsigned id_from, unsigned id_to)
			: Copy(from_mailbox, to_mailbox, id_from, id_to) {}

		OP_STATUS	OnCopyUid(IMAP4& protocol, unsigned uid_validity, OpINT32Vector& source_set, OpINT32Vector& dest_set);
	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);

	};


	/** @brief FETCH command
	  */
	class Fetch : public MessageSetCommand
	{
	public:
		enum FetchFlags
		{
			BODYSTRUCTURE	= 1 << 0,	///< Fetches the BODYSTRUCTURE (MIME structure) of a message
			ALL_HEADERS		= 1 << 1,	///< Fetches all headers of a message
			HEADERS			= 1 << 2,	///< Fetches headers needed to display / filter message
			BODY			= 1 << 3,	///< Fetches body of a message (everything except headers)
			TEXTPART		= 1 << 4,	///< Fetches text/plain part of the body of a message
			COMPLETE		= 1 << 5,	///< Fetches the whole message (headers and body)
			CHANGED			= 1 << 6	///< Fetches only items changed since last known (QRESYNC only)
		};

		Fetch(ImapFolder* mailbox, unsigned id_from, unsigned id_to, int flags)
			: MessageSetCommand(mailbox, id_from, id_to, flags) {}

		OP_STATUS	PrepareToSend(IMAP4& protocol);

		BOOL		IsUnnecessary(const IMAP4& protocol) const
			{ return m_mailbox->GetExists() == 0 || MessageSetCommand::IsUnnecessary(protocol); }

		int			GetFetchFlags() const
			{ return m_flags; }

		unsigned	GetProgressTotalCount() const
			{ return IsSent() ? 0 : m_message_set.Count(); }

		ProgressInfo::ProgressAction GetProgressAction() const;

	protected:
		virtual OP_STATUS GetFetchCommands(OpString8& commands) const;

		virtual const char* GetDefaultHeaders() const;
	
		virtual Fetch* CreateNewCopy() const;

		ExtendableCommandType GetExtendableType() const { return FETCH; }

		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);
	};

	/** @brief Base class for fetch-based sync commands
	  */
	class FetchSync : public Fetch
	{
	public:
		FetchSync(ImapFolder* mailbox, unsigned id_from, unsigned id_to, int flags)
			: Fetch(mailbox, id_from, id_to, flags) { m_mailbox->IncreaseSyncCount(); }

		~FetchSync()
			{ m_mailbox->DecreaseSyncCount(); }

		OP_STATUS	OnSuccessfulComplete(IMAP4& protocol)
			{ return m_mailbox->DoneFullSync(); }

		BOOL		IsUnnecessary(const IMAP4& protocol) const
			{ return !m_mailbox->NeedsFullSync() || m_mailbox->GetExists() == 0; }
	};

	/** @brief Full sync of a folder (fetches flags for all messages)
	  */
	class FullSync : public FetchSync
	{
	public:
		FullSync(ImapFolder* mailbox)
			: FetchSync(mailbox, 1, 0, 0) {}

		unsigned	GetProgressTotalCount() const
			{ return m_mailbox->GetExists(); }
	};

	/** @brief Forced Full sync of a folder (fetches flags for all messages)
	  */
	class ForcedFullSync : public FullSync
	{
	public:
		ForcedFullSync(ImapFolder* mailbox)
			: FullSync(mailbox) {}

		BOOL		IsUnnecessary(const IMAP4& protocol) const
			{ return m_mailbox->GetExists() == 0; }
	};

	/** @brief Low bandwidth sync (only checks for new messages)
	  */
	class LowBandwidthSync : public FetchSync
	{
	public:
		LowBandwidthSync(ImapFolder* mailbox)
		    : FetchSync(mailbox, mailbox->GetHighestUid() + 1, 0, USE_UID) {}
	};

	/** @brief Full sync for servers with QRESYNC support
	  */
	class QuickResync : public FetchSync
	{
	public:
		QuickResync(ImapFolder* mailbox)
			: FetchSync(mailbox, 1, 0, USE_UID | CHANGED) {}
	};

	/** @brief Fetch headers of messages
	 */
	class HeaderFetch : public Fetch
	{
	public:
		HeaderFetch(ImapFolder* mailbox, unsigned id_from, unsigned id_to, BOOL uid, BOOL bodystructure)
			: Fetch(mailbox, id_from, id_to, HEADERS | (uid ? USE_UID : 0) | (bodystructure ? BODYSTRUCTURE : 0)) {}
	};

	/** @brief Fetch bodies of messages
	 */
	class BodyFetch : public Fetch
	{
	public:
		BodyFetch(ImapFolder* mailbox, unsigned id_from, unsigned id_to, BOOL uid)
			: Fetch(mailbox, id_from, id_to, BODY | (uid ? USE_UID : 0)) {}
	};

	/** @brief Fetch complete messages
	 */
	class CompleteFetch : public Fetch
	{
	public:
		CompleteFetch(ImapFolder* mailbox, unsigned id_from, unsigned id_to, BOOL uid)
			: Fetch(mailbox, id_from, id_to, COMPLETE | (uid ? USE_UID : 0)) {}
	};
	
	/** @brief Fetch the text part of a message
	 * HOW THIS WORKS: the command will first request necessary headers and bodystructure. From that,
	 * it will derive which part is the text body, and it will create another command to download that.
	 */
	class TextPartFetch : public Fetch
	{
	public:
		TextPartFetch(ImapFolder* mailbox, unsigned id_from, unsigned id_to, BOOL uid)
			: Fetch(mailbox, id_from, id_to, ALL_HEADERS | BODYSTRUCTURE | TEXTPART | (uid ? USE_UID : 0)) {}

		OP_STATUS	OnTextPartDepth(unsigned uid, unsigned textpart_depth, IMAP4& protocol);
	
	protected:
		const char* GetDefaultHeaders() const;
		
		Fetch*		CreateNewCopy() const;

	private:
		class TextPartBodyFetch : public Fetch
		{
		public:
			TextPartBodyFetch(ImapFolder* mailbox, unsigned uid, unsigned textpart_depth)
				: Fetch(mailbox, uid, uid, TEXTPART | USE_UID), m_textpart_depth(textpart_depth) {}

			BOOL		CanExtendWith(const MessageSetCommand* command) const
				{ return FALSE; }

		protected:
			OP_STATUS	GetFetchCommands(OpString8& commands) const;
	
			unsigned m_textpart_depth;
		};
	};


	/** @brief SEARCH command
	  */
	class Search : public MessageSetCommand
	{
	public:
		enum SearchFlags
		{
			FOR_UID	= 1 << 0,
			BY_UID	= 1 << 1,
			NOT		= 1 << 2,
			DELETED	= 1 << 3
		};

		Search(ImapFolder* mailbox, unsigned id_from, unsigned id_to, int flags)
			: MessageSetCommand(mailbox, id_from, id_to, flags) {}

		OP_STATUS	OnSearchResults(OpINT32Vector& search_results);

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);
	};


	/** @brief STORE command
	  */
	class Store : public MessageSetCommand
	{
	public:
		enum StoreFlags
		{
			ADD_FLAGS		= 1 << 0,
			REMOVE_FLAGS	= 1 << 1,
			SILENT			= 1 << 2,
			FLAG_ANSWERED	= 1 << 3,
			FLAG_FLAGGED	= 1 << 4,
			FLAG_SEEN		= 1 << 5,
			FLAG_DELETED	= 1 << 6,
			KEYWORDS		= 1 << 7
		};

		Store(ImapFolder* mailbox, unsigned id_from, unsigned id_to, int flags, const OpStringC8& keyword = NULL)
			: MessageSetCommand(mailbox, id_from, id_to, flags) { m_keyword.Set(keyword); }

		BOOL		IsUnnecessary(const IMAP4& protocol) const;

		OP_STATUS	PrepareToSend(IMAP4& protocol);

		BOOL		CanExtendWith(const MessageSetCommand* command) const;

		BOOL		IsSaveable() const
			{ return TRUE; }

		void		MakeSaveable()
			{ m_flags &= ~SILENT; ImapCommandItem::MakeSaveable(); }

		BOOL		Changes(ImapFolder* folder, unsigned uid) const
			{ return m_mailbox == folder && m_message_set.Contains(uid); }

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::STORING_FLAG; }

		OP_STATUS	WriteToOfflineLog(OfflineLog& offline_log);

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);
		const char* GetStoreFlags(OpString8& tmp_string);
		ExtendableCommandType GetExtendableType() const { return STORE; }

		OpString8 m_keyword;
	};


	/** @brief Command that executes store on found messages, used in message delete
	 */
	class StoreResults : public Store
	{
	public:
		StoreResults(ImapFolder* mailbox, int flags)
			: Store(mailbox, 0, 0, flags) {}

		OP_STATUS	OnSearchResults(OpINT32Vector& search_results)
			{ return m_message_set.InsertSequence(search_results); }
	};

	/** @brief meta-command for permanently deleting messages, always deletes by UID
	  * if you specify a trash folder in the constructor, it will move to the trash folder before expunging
	  */
	class MessageDelete : public MessageSetCommand
	{
	public:
		MessageDelete(ImapFolder* mailbox, unsigned id_from, unsigned id_to, ImapFolder* trash_folder = NULL)
			: MessageSetCommand(mailbox, id_from, id_to, USE_UID), m_trash_folder(trash_folder) {}

		BOOL		Changes(ImapFolder* folder, unsigned uid) const
			{ return m_mailbox == folder && m_message_set.Contains(uid); }

		BOOL		IsMetaCommand(IMAP4& protocol) const
			{ return TRUE; }

		BOOL		IsSaveable() const
			{ return TRUE; }

		OP_STATUS	WriteToOfflineLog(OfflineLog& offline_log);

		ImapCommandItem* GetExpandedQueue(IMAP4& protocol);

	protected:
		ExtendableCommandType GetExtendableType() const { return MESSAGE_DELETE; }
		ImapFolder* m_trash_folder;
	};


	/** @brief UID EXPUNGE command
	  */
	class UidExpunge : public MessageSetCommand
	{
	public:
		UidExpunge(ImapFolder* mailbox, unsigned id_from, unsigned id_to)
			: MessageSetCommand(mailbox, id_from, id_to, USE_UID) {}

		ProgressInfo::ProgressAction GetProgressAction() const
			{ return ProgressInfo::DELETING_MESSAGE; }

	protected:
		OP_STATUS	AppendCommand(OpString8& command, IMAP4& protocol);
	};

	/** @brief SpecialExpungeCopy command, used in MessageDelete
	  * It also expunges the messages moved to the trash folder
	  */
	class SpecialExpungeCopy : public MoveCopy
	{
	public:
		SpecialExpungeCopy(ImapFolder* from_mailbox, ImapFolder* to_mailbox, unsigned id_from, unsigned id_to)
			: MoveCopy(from_mailbox, to_mailbox, id_from, id_to) {}

		OP_STATUS	OnCopyUid(IMAP4& protocol, unsigned uid_validity, OpINT32Vector& source_set, OpINT32Vector& dest_set);
	};

	/** @brief meta-command for marking messages as deleted, or moving messages to the trash folder
	  * if you specify a trash folder in the constructor, it will move to the trash folder before expunging
	  */
	class MessageMarkAsDeleted : public MessageSetCommand
	{
	public:
		MessageMarkAsDeleted(ImapFolder* mailbox, ImapFolder* trash_folder, unsigned id_from, unsigned id_to)
			: MessageSetCommand(mailbox, id_from, id_to, USE_UID), m_trash_folder(trash_folder) {}

		BOOL		Changes(ImapFolder* folder, unsigned uid) const
			{ return m_mailbox == folder && m_message_set.Contains(uid); }

		BOOL		IsMetaCommand(IMAP4& protocol) const
			{ return TRUE; }

		BOOL		IsSaveable() const
			{ return TRUE; }

		OP_STATUS	WriteToOfflineLog(OfflineLog& offline_log);

		ImapCommandItem* GetExpandedQueue(IMAP4& protocol);

	protected:
		ExtendableCommandType GetExtendableType() const { return MESSAGE_DELETE; }
		ImapFolder* m_trash_folder;
	};

	/** @brief SpecialMarkAsDeleteCopy command, used in MessageDelete
	  */
	class SpecialMarkAsDeleteCopy : public MoveCopy
	{
	public:
		SpecialMarkAsDeleteCopy(ImapFolder* from_mailbox, ImapFolder* to_mailbox, unsigned id_from, unsigned id_to)
			: MoveCopy(from_mailbox, to_mailbox, id_from, id_to) {}

		OP_STATUS	OnCopyUid(IMAP4& protocol, unsigned uid_validity, OpINT32Vector& source_set, OpINT32Vector& dest_set);
	};

	/** @brief meta-command for marking messages as spam or not, 
	  * It will also move messages to the spam folder (or back to a specified) if a move_to_folder is specified
	  */
	class MessageMarkAsSpam : public MessageSetCommand
	{
	public:
		MessageMarkAsSpam(ImapFolder* mailbox, ImapFolder* move_to_folder, BOOL mark_as_spam, unsigned id_from, unsigned id_to)
			: MessageSetCommand(mailbox, id_from, id_to, USE_UID), m_to_folder(move_to_folder), m_mark_as_spam(mark_as_spam) {}

		BOOL		Changes(ImapFolder* folder, unsigned uid) const
			{ return m_mailbox == folder && m_message_set.Contains(uid); }

		BOOL		IsMetaCommand(IMAP4& protocol) const
			{ return TRUE; }

		BOOL		IsSaveable() const
			{ return TRUE; }

		OP_STATUS	WriteToOfflineLog(OfflineLog& offline_log);

		ImapCommandItem* GetExpandedQueue(IMAP4& protocol);

	protected:
		ExtendableCommandType GetExtendableType() const { return m_mark_as_spam ? MESSAGE_SPAM : MESSAGE_NOT_SPAM; }
		ImapFolder* m_to_folder;
		BOOL		m_mark_as_spam;
	};


	/** @brief meta-command for un-deleting messages
	  *  First remove the \deleted flag, then move the messages from trash to to_folder if there is a trash folder
	  */
	class MessageUndelete : public MessageSetCommand
	{
	public:
		MessageUndelete(ImapFolder* mailbox, ImapFolder* to_folder, unsigned id_from, unsigned id_to)
			: MessageSetCommand(mailbox, id_from, id_to, USE_UID), m_to_folder(to_folder) {}

		BOOL		IsMetaCommand(IMAP4& protocol) const
			{ return TRUE; }

		BOOL		IsSaveable() const
			{ return TRUE; }
		
		OP_STATUS	WriteToOfflineLog(OfflineLog& offline_log);

		ImapCommandItem* GetExpandedQueue(IMAP4& protocol);

	protected:
		ExtendableCommandType GetExtendableType() const { return MESSAGE_UNDELETE; }
		ImapFolder* m_to_folder;
	};

};

#endif // MESSAGE_SET_COMMAND_H
