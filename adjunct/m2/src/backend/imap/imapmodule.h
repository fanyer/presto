//
// Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)
//

#ifndef IMAP_MODULE_H
#define IMAP_MODULE_H

#include "adjunct/m2/src/backend/ProtocolBackend.h"

#include "adjunct/m2/src/backend/imap/imap-protocol.h"
#include "adjunct/m2/src/backend/imap/imap-folder.h"
#include "adjunct/m2/src/backend/imap/imap-namespace.h"
#include "adjunct/m2/src/backend/imap/imap-uid-manager.h"

#include "adjunct/desktop_util/adt/opsortedvector.h"
#include "adjunct/quick/managers/SleepManager.h"

#include "modules/util/OpHashTable.h"

class PrefsFile;

class ImapBackend
  : public ProtocolBackend
  , public SleepListener
  , public MessageObject
{
public:
	ImapBackend(MessageDatabase& database);
	virtual ~ImapBackend();

	// From ProtocolBackend
	OP_STATUS Init(Account* account);
	OP_STATUS PrepareToDie();

	/** Fetch a complete message by store index
	  * @param id Store index of the message to fetch
	  * @param user_request Whether this action was initiated by the UI
	  */
	OP_STATUS FetchMessage(message_index_t, BOOL user_request, BOOL force_complete);

	/**
	  * Remove the message from the list of messages scheduled for fetch
	  * @param id Message id for the message that is just received
	  */
	void	  MessageReceived(message_gid_t id);

	/** Returns whether a certain message is scheduled to be fetched by this backend
	*/
	BOOL	  IsScheduledForFetch(message_gid_t id) const;

	/** Fetch a message by server UID
	  * @param folder Folder on server where message is located
	  * @param uid Server UID of the message to fetch
	  * @param force_full_message If set, always fetch the complete message, otherwise, get headers only or body according to preferences
	  * @param high_priority Whether the fetch should be executed with high priority
	  */
	OP_STATUS FetchMessageByUID(ImapFolder* folder, unsigned uid, BOOL force_full_message, BOOL high_priority = FALSE);

	/** Fetch new messages (check for new messages) according to the settings of the account
	  */
	OP_STATUS FetchMessages(BOOL enable_signalling);

	/** Select a folder identified by index, called when user selects a folder in UI
	  * @param index_id The index of the folder
	  */
	OP_STATUS SelectFolder(UINT32 index_id);

	/** Refresh a folder identified by index, called when user activates Reload
	 * @param index_id The index of the folder
	 */
	OP_STATUS RefreshFolder(UINT32 index_id);

	/** Refresh all (force a total update), called when user activates Reload or Resynchronize all messages
		  */
	OP_STATUS RefreshAll();

	/** Stop all downloading of messages
	  */
	OP_STATUS StopFetchingMessages();

	/** A keyword has been added to a message
	  * @param message_id Message to which a keyword has been added
	  * @param keyword Keyword that was added to the message
	  */
	OP_STATUS KeywordAdded(message_gid_t message_id, const OpStringC8& keyword);

	/** A keyword has been removed from a message
	  * @param message_id Message from which a keyword has been removed
	  * @param keyword Keyword that was removed from the message
	  */
	OP_STATUS KeywordRemoved(message_gid_t message_id, const OpStringC8& keyword);

	/** Gets the index where a message was originally stored (override if backend uses its own folders)
	  * @param message_id A message to check
	  * @return Index where message was originally stored, or 0 if unknown
	  */
	index_gid_t GetMessageOrigin(message_gid_t message_id);

	OP_STATUS Connect();
	OP_STATUS Disconnect() { return Disconnect("MessageBackend::Disconnect"); }
	OP_STATUS Disconnect(const OpStringC8& why);
	OP_STATUS RequestDisconnect();

	UINT32	  GetSubscribedFolderCount();
	OP_STATUS GetSubscribedFolderName(UINT32 index, OpString& name);
	OP_STATUS AddSubscribedFolder(OpString& path);
	OP_STATUS RemoveSubscribedFolder(UINT32 index);
	OP_STATUS RemoveSubscribedFolder(const OpStringC& path);
	OP_STATUS CommitSubscribedFolders()				{ return OpStatus::OK; }

	void	  GetAllFolders();
	void	  StopFolderLoading() {}

	void	  OnMessageSent(message_gid_t message_id);
	OP_STATUS CreateFolder(OpString& completeFolderPath, BOOL subscribed);
	OP_STATUS DeleteFolder(OpString& completeFolderPath);
	OP_STATUS RenameFolder(OpString& oldCompleteFolderPath, OpString& newCompleteFolderPath);

	BOOL	  HasExternalMessage(Message& message);
	OP_STATUS InsertExternalMessage(Message& message);
	OP_STATUS WriteToOfflineLog(OfflineLog& offline_log);

	OP_STATUS InsertMessage(message_gid_t message_id, index_gid_t destination_index);
	OP_STATUS RemoveMessages(const OpINT32Vector& message_ids, BOOL permanently);
	OP_STATUS RemoveMessage(const OpStringC8& internet_location);
	OP_STATUS MoveMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id);
	OP_STATUS CopyMessages(const OpINT32Vector& message_ids, index_gid_t source_index_id, index_gid_t destination_index_id);
	OP_STATUS ReadMessages(const OpINT32Vector& message_ids, BOOL read);
	OP_STATUS ReplyToMessage(message_gid_t message_id);
	OP_STATUS FlaggedMessage(message_gid_t message_id, BOOL flagged);

	OP_STATUS EmptyTrash(BOOL done_removing_messages);
	OP_STATUS MessagesMovedFromTrash(const OpINT32Vector& message_ids);

	OP_STATUS MarkMessagesAsSpam(const OpINT32Vector& message_ids, BOOL is_spam, BOOL imap_move);

	OP_STATUS GetFolderPath(AccountTypes::FolderPathType type, OpString& folder_path) const;
	OP_STATUS SetFolderPath(AccountTypes::FolderPathType type, const OpStringC& folder_path);

	void	  OnAccountAdded();

	OP_STATUS SettingsChanged(BOOL startup = FALSE);

	UINT16	  GetDefaultPort(BOOL secure) const 	{ return 143; }
	BOOL	  GetUseSSL() const						{ UINT16 port; return OpStatus::IsSuccess(GetPort(port)) && port == 993; }
	UINT32	  GetAuthenticationSupported()			{ return (UINT32)1 << AccountTypes::NONE |
															 (UINT32)1 << AccountTypes::PLAINTEXT |
															 (UINT32)1 << AccountTypes::PLAIN |
															 (UINT32)1 << AccountTypes::LOGIN |
															 (UINT32)1 << AccountTypes::CRAM_MD5 |
															 (UINT32)1 << AccountTypes::AUTOSELECT; }
	BOOL	  IsBusy() const						{ return m_default_connection.IsBusy() || m_extra_connection.IsBusy(); }
	IndexTypes::Type GetIndexType() const 			{ return IndexTypes::IMAP_INDEX; }
	AccountTypes::AccountType GetType() const 		{ return AccountTypes::IMAP; }
	BOOL	  SupportsStorageType(AccountTypes::MboxType type)
													{ return type < AccountTypes::MBOX_TYPE_COUNT; }
	BOOL	  HasContinuousConnection() const		{ return TRUE; }

	int		  GetConnectionCount()					{ return m_extra_connection.IsConnected() ? 2 : 1; }

	/** Synchronize (check for new messages) in a folder
	  * @param folder Which folder to synchronize
	  */
	OP_STATUS SyncFolder(ImapFolder* folder);

	/** Update information we have about a folder, or create a new entry if we didn't have it yet
	  * @param name Name of the folder
	  * @param delimiter Delimiter used
	  * @param list_flags LIST Flags for the folder
	  * @param is_lsub If TRUE, we are subscribed to this folder
	  * @param no_sync If TRUE, don't start a synchronization of this folder
	  */
	OP_STATUS UpdateFolder(const OpStringC8& name, char delimiter, int list_flags, BOOL is_lsub, BOOL no_sync = FALSE);

	/** Remove a folder from all possible local storage - M2 will forget about this folder
	  * @param folder Folder to remove
	  */
	OP_STATUS RemoveFolder(ImapFolder* folder);

	/** Remove a folder from the IMAP backend, will not delete it or notify listeners
	  * @param folder Folder to remove
	  */
	OP_STATUS RemoveFolderFromIMAPBackend(ImapFolder* folder);

	/** The connection should call this function when unexpectantly disconnected
	  * @param connection The connection that was disconnected
	  */
	OP_STATUS OnUnexpectedDisconnect(IMAP4& connection);

	/** If folder is selected by one of the connections, return that connection
	  * @param folder The folder to check
	  * @return Connection that has folder selected, or NULL if none
	  */
	IMAP4*	  SelectedByConnection(ImapFolder* folder);

	/** Generate a new tag for a command sent over this account
	  * @param new_tag Where to save the tag
	  */
	OP_STATUS GenerateNewTag(OpString8& new_tag);

	/** Add a folder to this IMAP account's internal list of folders
	  * @param folder The folder to add
	  */
	OP_STATUS AddFolder(ImapFolder* folder);

	/** Remove all namespace data that we have
	  */
	OP_STATUS ResetNamespaces();

	/** Add namespace for a specific namespace type
	  * @param type Namespace type
	  * @param prefix Namespace prefix
	  * @param delimiter Delimiter used for this namespace
	  */
	OP_STATUS AddNamespace(ImapNamespace::NamespaceType type, const OpStringC8& prefix, char delimiter);

	/** Get the user-selected folder
	  */
	ImapFolder* GetSelectedFolder() const { return m_selected_folder; }

	/** Get the folder where undeleted messages or messages marked as not spam should be put (\AllMail folder or INBOX)
	  */
	ImapFolder* GetRestoreFolder();

	/** Get options for this connection as defined in ImapFlags::Options
	  */
	int		  GetOptions() const { return m_options; }

	/** Get the connection that is responsible for folder
	  * @param folder Folder to find
	  */
	IMAP4&	  GetConnection(ImapFolder* folder);

	/** Check whether a UID map should be maintained for a certain folder
	  * @param folder Folder to check
	  */
	BOOL	  ShouldMaintainUIDMap(ImapFolder* folder);

	/** Lock this connection (no further communication until unlocked)
	  */
	void	  Lock() { m_options |= ImapFlags::OPTION_LOCKED; }

	/** @return UID manager for this account
	  */
	ImapUidManager& GetUIDManager() { return m_uid_manager; }

	/** @return configuration file for this account
	  */
	PrefsFile*		GetCurrentPrefsFile();

	/** Get an IMAP folder by its type
	  * @param type of the folder: INBOX, Sent, Spam, Trash, Drafts, etc
	  * @return folder if found, NULL otherwise
	  */
	ImapFolder*		GetFolderByType(AccountTypes::FolderPathType type);

	/** Returns the folder prefix
	  * @return The folder prefix
	  */
	const OpStringC8& GetFolderPrefix() const { return m_folder_prefix; }

	/** Get an IMAP folder by its name
	  * @param name Name of folder to find
	  * @param folder Where to store result
	  * @return OpStatus::OK if found, OpStatus::ERR otherwise
	  */
	OP_STATUS GetFolderByName(const OpStringC8& name, ImapFolder*& folder) const;
	OP_STATUS GetFolderByName(const OpStringC& name, ImapFolder*& folder) const;

	/** Get an IMAP folder by its index
	  * @param index_id Index of folder to find
	  * @param folder Where to store result
	  * @return OpStatus::OK if found, OpStatus::ERR otherwise
	  */
	OP_STATUS GetFolderByIndex(index_gid_t index_id, ImapFolder*& folder) const;

	unsigned  GetConnectRetries() const    { return m_connect_retries; }
	unsigned  GetMaxConnectRetries() const { return MaxConnectRetries; }

	/** Call when a message has been copied to a different mailbox and received
	  * a new UID (to ensure appearance in correct indexes)
	  * @param source_message Message that was copied
	  * @param dest_folder Folder that message was copied to
	  * @param dest_uid UID that message received in dest_folder
	  * @param replace Whether to replace the source message with the copied message
	  */
	OP_STATUS OnMessageCopied(message_gid_t source_message, ImapFolder* dest_folder, unsigned dest_uid, BOOL replace);

	/** Remove folders that are not in the confirmed list
	  */
	OP_STATUS ProcessConfirmedFolderList();

	/** Add a progress group item
	  */
	void NewProgress(ProgressInfo::ProgressAction group, int total_increase);

	/** Increase progress total without changing users
	  */
	void IncreaseProgressTotal(ProgressInfo::ProgressAction group, int total_increase);

	/** Update a progress group
	  */
	void UpdateProgressGroup(ProgressInfo::ProgressAction group, int done_increase, BOOL propagate);

	/** Remove a progress group item
	  */
	void ProgressDone(ProgressInfo::ProgressAction group);

	/** Reset progress
	  */
	void ResetProgress();

	// From SleepListener
	/** Called when system goes into sleep mode
	 */
	void OnSleep();

	/** Called when system comes out of sleep mode
	 */
	void OnWakeUp();

	// From MessageObject
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// Fix Gmails duplicate mail issues by using the \AllMail IMAP folder (found with XLIST)
	OP_STATUS SetAllMailFolder(ImapFolder* allmail_folder);

private:
	OP_STATUS GetFolderAndUID(const OpStringC8& internet_location, ImapFolder*& folder, unsigned& uid) const;
	OP_STATUS GetFolderAndUID(message_gid_t message_id, ImapFolder*& folder, unsigned& uid) const;
	OP_STATUS ReadConfiguration();
	OP_STATUS WriteConfiguration(BOOL commit);
	OP_STATUS UpdateStore(int old_version);
	BOOL	  IsExtraAvailable() { return !(m_options & (ImapFlags::OPTION_FORCE_SINGLE_CONNECTION | ImapFlags::OPTION_EXTRA_CONNECTION_FAILS)); }

	struct ProgressGroup
	{
		ProgressGroup() : done(0), total(0), user_count(0) {}

		int      done;
		unsigned total;
		unsigned user_count;
	};

	static const unsigned	 MaxConnectRetries = 5;    // Maximum number of reconnects when unexpectedly disconnected

	IMAP4 					 m_default_connection;	// Used to connect to the INBOX, or all if this is only connection
	IMAP4					 m_extra_connection;	// If possible, used to connect to folders other then the INBOX
	OpAutoStringHashTable<ImapFolder>
							 m_folders;
	OpSortedVector<ImapFolder, PointerLessThan<ImapFolder> >
							 m_confirmed_folders;
	OpINT32HashTable<ImapFolder>	 m_special_folders; // hash table of all the inbox, sent, trash, spam, etc folders we know of
	OpString8				 m_folder_prefix;		// Prefix to use for all folder names
	OpAutoVector<ImapNamespace> m_namespaces[3];	// Registered namespaces (index of NamespaceType)
	ImapFolder*				 m_selected_folder;
	ImapUidManager			 m_uid_manager;
	int						 m_old_version;
	PrefsFile*				 m_prefsfile;
	unsigned				 m_connect_retries;
	ProgressGroup			 m_progress_groups[ProgressInfo::_PROGRESS_ACTION_COUNT];
	bool					 m_connected_on_sleep;
	OpINTSortedVector		 m_scheduled_messages;

	unsigned				 m_tag_seed;

	int						 m_options;				// Flags as defined in ImapFlags::Options
};

#endif // IMAP_MODULE_H
