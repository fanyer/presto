// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef IBACKEND_H
#define IBACKEND_H

#include "adjunct/m2/src/include/defs.h"

class Message;
class ChatInfo;
class OfflineLog;
class ProgressInfo;
class MessageDatabase;

class MessageBackend
{
	public:
		MessageBackend(MessageDatabase& database) : m_database(database) {}
		virtual ~MessageBackend() {}

        virtual OP_STATUS SettingsChanged(BOOL startup=FALSE) = 0;
        virtual OP_STATUS PrepareToDie() = 0;
		virtual UINT16    GetDefaultPort(BOOL secure) const {return 0;}
		virtual UINT32    GetAuthenticationSupported() = 0;
		virtual IndexTypes::Type GetIndexType() const { return IndexTypes::FOLDER_INDEX; }

		virtual const char* GetIcon(BOOL progress_icon) { return progress_icon ? NULL : "Mail Accounts"; }

		virtual OP_STATUS GetFolderPath(AccountTypes::FolderPathType type, OpString& folder_path) const {return OpStatus::OK;}
		virtual OP_STATUS SetFolderPath(AccountTypes::FolderPathType type, const OpStringC& folder_path) {return OpStatus::OK;}

		virtual BOOL	  SupportsStorageType(AccountTypes::MboxType type) { return 0 < type && type < AccountTypes::MBOX_TYPE_COUNT; }
		
		/** Returns whether backend as a continuous connection
		  */
		virtual BOOL	  HasContinuousConnection() const { return FALSE; }
		
		/** Returns whether a certain message is scheduled to be fetched by this backend
		  */
		virtual BOOL	  IsScheduledForFetch(message_gid_t id) const { return FALSE; }

		/** Called after this account has been added
		  */
		virtual void	  OnAccountAdded() {}

		/** Whether this backend is verbose (i.e. wants notifications even if no messages received)
		  */
		virtual BOOL	  Verbose() { return FALSE; }

		/** Update any file formats kept for this backend
		  * @param old_version Version for current file formats
		  */
		virtual OP_STATUS UpdateStore(int old_version) { return OpStatus::OK; }

		//API
		/** Fetch a message by store index
		  * @param id Store index of the message to fetch
		  * @param user_request Whether this action was initiated by the UI
		  * @param force_complete When TRUE, will always fetch the complete message. Might
		  *                       fetch partial message otherwise.
		  */
		virtual OP_STATUS FetchMessage(message_index_t id, BOOL user_request, BOOL force_complete) = 0;

		/** Fetch new messages (check for new messages) according to the settings of the account
		  * @param enable_signalling TODO What
		  */
        virtual OP_STATUS FetchMessages(BOOL enable_signalling) = 0;

		/** Select a folder identified by index, called when user selects a folder in UI
		  * @param index_id The index of the folder
		  */
		virtual OP_STATUS SelectFolder(UINT32 index_id) { return OpStatus::OK; }

		/** Refresh a folder identified by index, called when user activates Reload
		  * @param index_id The index of the folder
		  */
		virtual OP_STATUS RefreshFolder(UINT32 index_id) { return OpStatus::OK; }

		/** Refresh all (force a total update), called when user activates Reload
		  */
		virtual OP_STATUS RefreshAll() { Disconnect(); return FetchMessages(TRUE); }

		/** Stop all downloading of messages
		  */
		virtual OP_STATUS StopFetchingMessages() = 0;

		/** A keyword has been added to a message
		  *  Implement if your protocol supports keywords (tags) on messages
		  * @param message_id Message to which a keyword has been added
		  * @param keyword Keyword that was added to the message
		  */
		virtual OP_STATUS KeywordAdded(message_gid_t message_id, const OpStringC8& keyword) { return OpStatus::OK; }

		/** A keyword has been removed from a message
		  *  Implement if your protocol supports keywords (tags) on messages
		  * @param message_id Message from which a keyword has been removed
		  * @param keyword Keyword that was removed from the message
		  */
		virtual OP_STATUS KeywordRemoved(message_gid_t message_id, const OpStringC8& keyword) { return OpStatus::OK; }

		/** Send a message
		  *  Implement if your protocol supports sending messages
		  * @param message Message to sent
		  * @param anonymous Whether the message should be sent anonymously
		  */
		virtual OP_STATUS SendMessage(Message& message, BOOL anonymous) { return OpStatus::ERR; }

		/** Stops all sending of messages
		  *  Implement if your protocol supports sending messages
		  */
		virtual OP_STATUS StopSendingMessage() { return OpStatus::ERR; }

		/** Connect to the server
		  */
		virtual OP_STATUS Connect() = 0;

		/** Disconnect from the server
		  */
        virtual OP_STATUS Disconnect() = 0;

		/** Clear passwords that are fetched in the backend (e.g. to trigger a fetch of a new password or when it's cleared from delete private data)
		 */
		virtual void ResetRetrievedPassword() = 0;

		/** Check if a backend already has received a certain message
		  * @param message The message to check for
		  * @return TRUE if the backend has the message, FALSE if we don't know
		  */
		virtual BOOL      HasExternalMessage(Message& message) { return FALSE; }

		/** Do all actions necessary to insert an 'external' message (e.g. from import) into this backend
		  * @param message Message to insert
		  */
		virtual OP_STATUS InsertExternalMessage(Message& message) = 0;

		/** Gets the index where a message was originally stored (override if backend uses its own folders)
		  * @param message_id A message to check
		  * @return Index where message was originally stored, or 0 if unknown
		  */
		virtual index_gid_t GetMessageOrigin(message_gid_t message_id) { return 0; }

		/** If your backend can hold commands, write all commands that it's currently holding into the offline log
		  * This will be called when the backend is destroyed, makes it possible to resume after startup
		  * @param offline_log Log to write to
		  */
		virtual OP_STATUS WriteToOfflineLog(OfflineLog& offline_log) { return OpStatus::OK; }

		/** Gets progress for this backend
		  */
		virtual ProgressInfo& GetProgress() = 0;

		/* NNTP newsgroups, IMAP folders, IRC chatrooms, Implement if needed. */
		virtual UINT32    GetSubscribedFolderCount() { return 0; }
		virtual OP_STATUS GetSubscribedFolderName(UINT32 index, OpString& name) { return OpStatus::ERR; }
		virtual OP_STATUS AddSubscribedFolder(OpString& path) { return OpStatus::ERR; }
		virtual OP_STATUS RemoveSubscribedFolder(const OpStringC& path) { return OpStatus::ERR; }
		virtual OP_STATUS RemoveSubscribedFolder(UINT32 index) { return OpStatus::ERR; }
		virtual OP_STATUS CommitSubscribedFolders() {return OpStatus::OK; }

		virtual void      GetAllFolders() {}
		virtual void      StopFolderLoading() {}

        /* IRC */
		virtual OP_STATUS JoinChatRoom(const OpStringC& room, const OpStringC& password, BOOL no_lookup = FALSE) { return OpStatus::ERR; }
		virtual OP_STATUS LeaveChatRoom(const ChatInfo& room) { return OpStatus::ERR; }
		virtual OP_STATUS SendChatMessage(EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& room, const OpStringC& chatter) { return OpStatus::ERR; }
		virtual OP_STATUS ChangeNick(OpString& new_nick) { return OpStatus::ERR; }

		virtual OP_STATUS SetChatProperty(const ChatInfo& room, const OpStringC& chatter, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value) { return OpStatus::ERR; }
		virtual OP_STATUS SendFile(const OpStringC& chatter, const OpStringC& filename) { return OpStatus::ERR; }

		virtual	BOOL		IsChatStatusAvailable(AccountTypes::ChatStatus chat_status) { return FALSE; }
		virtual	OP_STATUS	SetChatStatus(AccountTypes::ChatStatus chat_status) { return OpStatus::ERR; }
		virtual AccountTypes::ChatStatus	GetChatStatus(BOOL& is_connecting) { is_connecting = FALSE; return AccountTypes::OFFLINE; }

        /*NNTP */
		virtual OP_STATUS FetchNewGroups() { return OpStatus::ERR; }
		virtual OP_STATUS FetchNewsHeaders(const OpStringC8& internet_location, message_index_t index=0) { return OpStatus::ERR; }
		virtual OP_STATUS FetchNewsMessage(const OpStringC8& internet_location, message_index_t index=0) { return OpStatus::ERR; }

		/* IMAP */
		virtual void OnMessageSent(message_gid_t message_id) {};
		virtual OP_STATUS CreateFolder(OpString& completeFolderPath, BOOL subscribed) { return OpStatus::ERR; }
		virtual OP_STATUS DeleteFolder(OpString& completeFolderPath) { return OpStatus::ERR; }
		virtual OP_STATUS RenameFolder(OpString& oldCompleteFolderPath, OpString& newCompleteFolderPath) { return OpStatus::ERR; }

		virtual OP_STATUS InsertMessage(message_gid_t message_id, index_gid_t destination_index) { return OpStatus::OK; }
		virtual OP_STATUS RemoveMessages(const OpINT32Vector& message_ids, BOOL permanently) { return OpStatus::OK; }

		/** Remove a message permanently by internet location, implement if your backend supports internet location
		  * @param internet_location Internet location of message to remove
		  */
		virtual OP_STATUS RemoveMessage(const OpStringC8& internet_location) { return OpStatus::OK; }

		virtual OP_STATUS MoveMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id) { return OpStatus::OK; }
		virtual OP_STATUS CopyMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id) { return OpStatus::OK; }
		virtual OP_STATUS ReadMessages(const OpINT32Vector& message_ids, BOOL read) { return OpStatus::OK; }
		virtual OP_STATUS ReplyToMessage(message_gid_t message_id) { return OpStatus::OK; }
		virtual OP_STATUS FlaggedMessage(message_gid_t message_id, BOOL flagged) { return OpStatus::OK; }

		virtual OP_STATUS EmptyTrash(BOOL done_removing_messages) {return OpStatus::ERR;}
		virtual OP_STATUS MessagesMovedFromTrash(const OpINT32Vector& message_ids) {return OpStatus::ERR;}

		/** Move messages to the IMAP junk folder if there is one
		 */
		virtual OP_STATUS MarkMessagesAsSpam(const OpINT32Vector& message_ids, BOOL is_spam, BOOL imap_move) { return OpStatus::OK; }

		/* POP3 */
		virtual OP_STATUS ServerCleanUp() { return OpStatus::ERR; }

		/* Chat */
		virtual OP_STATUS AcceptReceiveRequest(UINT port_number, UINT id) { return OpStatus::ERR; }
		virtual UINT GetStartOfDccPool() const { return 0; }
		virtual UINT GetEndOfDccPool() const { return 0; }
		virtual void SetStartOfDccPool(UINT port) { }
		virtual void SetEndOfDccPool(UINT port) { }
		virtual void SetCanAcceptIncomingConnections(BOOL can_accept) { }
		virtual BOOL GetCanAcceptIncomingConnections() const { return FALSE; }
		virtual void SetOpenPrivateChatsInBackground(BOOL open_in_background) { }
		virtual BOOL GetOpenPrivateChatsInBackground() const { return FALSE; }
		virtual OP_STATUS GetPerformWhenConnected(OpString& commands) const { return OpStatus::ERR; }
		virtual OP_STATUS SetPerformWhenConnected(const OpStringC& commands) { return OpStatus::ERR; }

		/* Jabber */ // added by dvs
#ifdef JABBER_SUPPORT
		virtual OP_STATUS SubscribeToPresence(const OpStringC& jabber_id, BOOL subscribe) { return OpStatus::ERR; }
		virtual OP_STATUS AllowPresenceSubscription(const OpStringC& jabber_id, BOOL allow) { return OpStatus::ERR; }
#endif // JABBER_SUPPORT

		virtual OP_STATUS WriteSettings() { return OpStatus::OK; };

	protected:
		virtual MessageDatabase& GetMessageDatabase() { return m_database; }

	private:
		MessageDatabase& m_database;
};

#endif // IBACKEND_H
