/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef ENGINE_H
#define ENGINE_H

#ifdef M2_SUPPORT

#define g_m2_engine MessageEngine::GetInstance()

// ----------------------------------------------------

#include "adjunct/desktop_util/adt/opsortedvector.h"
#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/m2/src/MessageDatabase/MessageDatabase.h"
#include "adjunct/m2/src/util/inputconvertermanager.h"

// ----------------------------------------------------

#include "adjunct/m2/src/glue/factory.h"
#include "modules/hardcore/timer/optimer.h"

#include "adjunct/desktop_util/settings/SettingsListener.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"

// ----------------------------------------------------

class ModuleManager;
class AutoDelete;

// ----------------------------------------------------

typedef UINT32 backend_id_t;
typedef UINT32 folder_id_t;

// ----------------------------------------------------

class Account;
class AccountManager;
class ChatNetworkManager;
class ChatFileTransferManager;
class ChatRoomsModel;
class ChattersModel;
class IndexModel;
class AccountsModel;
class GroupsModel;
class ChatRoom;
class Store;
class Index;
class Indexer;
class IndexGroup;
class Message;
class ProgressInfo;
class SEMessageDatabase;
#ifdef M2_MAPI_SUPPORT
class MapiMessageListener;
#endif //M2_MAPI_SUPPORT

class MessageEngine
	: public IndexerListener
	, public MessageLoop::Target
	, public SettingsListener
{
	struct EngineAsyncPasswordContainer
	{
		const Account*	m_account;
		BOOL			m_incoming;
	};

/* Observer-functions: */
public:
    OP_STATUS Sent(Account* account, message_gid_t uid, OP_STATUS transmit_status);
	OP_STATUS SignalStartSession(BOOL incoming);
	OP_STATUS SignalEndSession(BOOL incoming, int message_count, BOOL report_session);

	OP_STATUS IndexAdded(Indexer *indexer, UINT32 index_id) { return OpStatus::OK; }
	OP_STATUS IndexRemoved(Indexer *indexer, UINT32 index_id);
	OP_STATUS IndexChanged(Indexer *indexer, UINT32 index_id) { OnIndexChanged(index_id); return OpStatus::OK; }
	OP_STATUS IndexNameChanged(Indexer *indexer, UINT32 index_id) { return OpStatus::OK; }
	OP_STATUS IndexIconChanged(Indexer* indexer, UINT32 index_id) { return OpStatus::OK; }
	OP_STATUS IndexVisibleChanged(Indexer *indexer, UINT32 index_id) { return OpStatus::OK; }
	OP_STATUS IndexKeywordChanged(Indexer *indexer, UINT32 index_id, const OpStringC8& old_keyword, const OpStringC8& new_keyword);
	OP_STATUS KeywordAdded(Indexer* indexer, message_gid_t message_id, const OpStringC8& keyword);
	OP_STATUS KeywordRemoved(Indexer* indexer, message_gid_t message_id, const OpStringC8& keyword);
	OP_STATUS CategoryAdded(Indexer* indexer, UINT32 index_id) { return OpStatus::OK; }
	OP_STATUS CategoryRemoved(Indexer* indexer, UINT32 index_id) { return OpStatus::OK; }
	OP_STATUS CategoryUnreadChanged(Indexer* indexer, UINT32 index_id, UINT32 unread_messages) { return OpStatus::OK; }

// ----------------------------------------------------
// functions for telling engine about events. engine will then tell all listeners

	void OnImporterProgressChanged(const Importer* importer, const OpStringC& infoMsg, OpFileLength current, OpFileLength total, BOOL simple = FALSE);
	void OnImporterFinished(const Importer* importer, const OpStringC& infoMsg);
	void OnIndexChanged(UINT32 index_id);
	void OnActiveAccountChanged();
	void OnReindexingProgressChanged(INT32 progress, INT32 total);

	void OnSettingsChanged(DesktopSettings* settings);

	void OnAccountAdded(UINT16 account_id);
	void OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type);
	void OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable);
	void OnFolderRemoved(UINT16 account_id, const OpStringC& path);
	void OnFolderRenamed(UINT16 account_id, const OpStringC& old_path, const OpStringC& new_path);
	void OnFolderLoadingCompleted(UINT16 account_id);

    void OnMessageBodyChanged(StoreMessage& message);
	void OnMessageChanged(message_gid_t message_id);
	void OnMessageMadeAvailable(message_gid_t message_id, BOOL read);
	void OnAllMessagesAvailable();

	void OnMessageHidden(message_gid_t message_id, BOOL hidden);
	void OnMessagesRead(const OpINT32Vector& message_ids, BOOL read);
	void OnMessageReplied(message_gid_t message_id);
	void OnMessageLabelChanged(message_gid_t message_id, UINT32 old_label, UINT32 new_label);

	BOOL PerformingMultipleMessageChanges() { return m_multiple_message_changes > 0; }

	void OnChangeNickRequired(UINT16 account_id, const OpStringC& old_nick);
	void OnRoomPasswordRequired(UINT16 account_id, const OpStringC& room);

	void OnYesNoInputRequired(UINT16 account_id, EngineTypes::YesNoQuestionType type, OpString* sender=NULL, OpString* param=NULL);
	void OnError(UINT16 account_id, const OpStringC& errormessage, const OpStringC& context, EngineTypes::ErrorSeverity severity = EngineTypes::GENERIC_ERROR);
#ifdef IRC_SUPPORT
	void OnChatStatusChanged(UINT16 account_id);
	void OnChatMessageReceived(UINT16 account_id, EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& chat_info, const OpStringC& chatter, BOOL is_private_chat);
	void OnChatServerInformation(UINT16 account_id, const OpStringC& server, const OpStringC& information);
	void OnChatRoomJoined(UINT16 account_id, const ChatInfo& room);
	void OnChatRoomLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason);
	void OnChatterJoined(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial);
	void OnChatterLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker);
	void OnChatPropertyChanged(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& changed_by, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value);
	void OnWhoisReply(UINT16 account_id, const OpStringC& nick, const OpStringC& user_id, const OpStringC& host,
					  const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message,
					  const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& channels);
	void OnInvite(UINT16 account_id, const OpStringC& nick, const ChatInfo& room);
	void OnFileReceiveRequest(UINT16 account_id, const OpStringC& sender, const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id);
	void OnUnattendedChatCountChanged(OpWindow* op_window, UINT32 unread);
	void OnChatReconnecting(UINT16 account_id, const ChatInfo& room);
#endif // IRC_SUPPORT
// ----------------------------------------------------

	OP_STATUS FetchMessages(BOOL enable_signalling, BOOL full_sync = FALSE) const;
	OP_STATUS FetchMessages(UINT16 account_id, BOOL enable_signalling) const;
	OP_STATUS SelectFolder(UINT32 index_id);
    OP_STATUS RefreshFolder(UINT32 index_id);
	OP_STATUS SendMessages(UINT16 only_from_account_id=0);
	OP_STATUS SendMessage(Message& message, BOOL anonymous);
	OP_STATUS StopSendingMessages();
	OP_STATUS StopSendingMessages(UINT16 account_id);
	OP_STATUS SaveDraft(Message* message);
	OP_STATUS StopFetchingMessages() const;
	OP_STATUS StopFetchingMessages(UINT16 account_id) const;
	OP_STATUS DisconnectAccount(UINT16 account_id) const;
	OP_STATUS DisconnectAccounts() const;

public:
	/** Instantiate this class if you are going to change multiple messages at a time,
	  * and the engine shouldn't do various updates in between. 
	  *
	  * Those blocked updates are: OnIndexChanged, OnMessageChanged, OnMessageHidden, 
	  * GetMessage. 
	  * Engine::m_multiple_message_changes is used to block them
	  */
	class MultipleMessageChanger : public MessageDatabase::MessageChangeLock
	{
		public:
			/** Constructor
			  * @param tochange How many messages caller plans on changing, 0 for unknown
			  */
			MultipleMessageChanger(unsigned tochange = 0);

			~MultipleMessageChanger();

		private:
			BOOL m_docount;
	};

    MessageEngine();
    virtual ~MessageEngine();

	OP_STATUS Init(OpString8& status);
	OP_STATUS InitMessageDatabaseIfNeeded();
	OP_STATUS InitEnginePart1(OpString8& status);
	OP_STATUS InitEnginePart2(OpString8& status);
	void	  FlushAutodeleteQueue();
	OP_STATUS InitDefaultAccounts();

	/** Check if M2 should be initialized on startup
	  * @return Whether M2 should be initialized
	  */
	static BOOL InitM2OnStartup();

// ----------------------------------------------------

	void SetFactories(GlueFactory* glue_factory)
	 				  { m_glue_factory = glue_factory; }

    GlueFactory* GetGlueFactory() const {return m_glue_factory;}


// ----------------------------------------------------

    // singleton class, note that only classes within the engine-module may/can use this
    // a reference to the engine singleton has to be passed down to each backend anyway

    static MessageEngine* GetInstance() { return s_instance; }
	static void CreateInstance() { s_instance = OP_NEW(MessageEngine, ()); }
	static void DestroyInstance() { OP_DELETE(s_instance); s_instance = 0; }

// ----------------------------------------------------
    OP_STATUS CreateMessage(Message& new_message, UINT16 account_id, message_gid_t old_message_id = 0, MessageTypes::CreateType type=MessageTypes::NEW) const;
    Account* CreateAccount();
	OP_STATUS SetOfflineMode(BOOL offline_mode);
	OP_STATUS GetMessage(Message& message, message_gid_t id, BOOL full = FALSE, BOOL timeout_request=FALSE);
	OP_STATUS GetMessage(Message& message, const OpStringC8& message_id, BOOL full);
	OP_STATUS FetchCompleteMessage(message_gid_t message_id);
	OP_STATUS HasMessageDownloadedBody(message_gid_t id, BOOL& has_downloaded_body);
	OP_STATUS OnDeleted(message_gid_t message_is, BOOL permanently);
	OP_STATUS RemoveMessages(OpINT32Vector& message_ids, BOOL permanently);
	OP_STATUS RemoveMessages(OpINT32Vector& message_ids, Account* account, BOOL permanently);
	OP_STATUS UndeleteMessages(const OpINT32Vector& message_ids);
	OP_STATUS EmptyTrash();
	BOOL      IsEmptyingTrash() { return m_messages_to_delete.GetCount() > 0; }
	OP_STATUS CancelMessage(message_gid_t message);
	OP_STATUS UpdateMessage(Message& message);
	OP_STATUS IndexRead(index_gid_t index_id);
	OP_STATUS MessageRead(Message& message, BOOL read);
	OP_STATUS MessagesRead(OpINT32Vector& message_ids, BOOL read);
	OP_STATUS MessageFlagged(message_gid_t message_id, BOOL flagged);
	OP_STATUS MessageReplied(message_gid_t message, BOOL read);
	OP_STATUS MarkAsSpam(const OpINT32Vector& message_ids, BOOL is_spam, BOOL propagate_call = TRUE, BOOL imap_move = TRUE);
	void	  SetSpamLevel(INT32 level);
	INT32	  GetSpamLevel();
	void      DelayedSpamFilter(message_gid_t message_id) { }

	void	  SetUserWantsToDoRecovery (BOOL will_do_recover) { m_will_do_recover_check_on_exit = will_do_recover; }
	void	  SetAskedAboutDoingMailRecovery (BOOL has_asked) { m_has_asked_user_about_recovery = has_asked; }
	BOOL	  NeedToAskAboutDoingMailRecovery ();

	OP_STATUS AddToContacts(message_gid_t message, int parent_folder_id);
	OP_STATUS GetFromAddress(message_gid_t message_id, OpString& sender, BOOL do_quote_pair_encode=FALSE);


// ----------------------------------------------------

	virtual AccountManager* GetAccountManager() const {return m_account_manager;}
#ifdef IRC_SUPPORT
	virtual ChatRoomsModel* GetChatRoomsModel() const {return m_chatrooms_model;}
#endif
	virtual AccountsModel* GetAccountsModel() const {return m_accounts_model;}
	virtual ProgressInfo& GetMasterProgress() {return *m_master_progress;}

	virtual Account* GetAccountById(UINT16 account_id);			// helper function

#ifdef IRC_SUPPORT
	virtual ChatRoom* GetChatRoom(UINT32 room_id);
	virtual ChatRoom* GetChatRoom(UINT16 account_id, OpString& room);
	virtual ChatRoom* GetChatter(UINT32 chatter_id);
#endif

	virtual UINT32 GetIndexIdFromFolder(OpTreeModelItem* item);

	virtual Store* GetStore() { return m_store; }
	virtual Indexer* GetIndexer() { return m_indexer; }

	virtual OP_STATUS GetIndexModel(OpTreeModel** index_model, Index* index, INT32& start_pos);

	OP_STATUS ReinitAllIndexModels();

#ifdef IRC_SUPPORT
	virtual ChatNetworkManager* GetChatNetworks();
	virtual ChatFileTransferManager* GetChatFileTransferManager();
#endif

// ----------------------------------------------------

	// implementing the MessageEngine interface

	OP_STATUS AddEngineListener(EngineListener* engine_listener) { return m_engine_listeners.Add(engine_listener); }
	OP_STATUS AddAccountListener(AccountListener* account_listener) { return m_account_listeners.Add(account_listener); }
	OP_STATUS AddMessageListener(MessageListener* message_listener) { return m_message_listeners.Add(message_listener); }
	OP_STATUS AddInteractionListener(InteractionListener* interaction_listener) { return m_interaction_listeners.Add(interaction_listener); }
#ifdef IRC_SUPPORT
	OP_STATUS AddChatListener(ChatListener* chat_listener) { return m_chat_listeners.Add(chat_listener); }
#endif // IRC_SUPPORT

	OP_STATUS RemoveEngineListener(EngineListener* engine_listener) { return m_engine_listeners.RemoveByItem(engine_listener); }
	OP_STATUS RemoveAccountListener(AccountListener* account_listener) { return m_account_listeners.RemoveByItem(account_listener); }
	OP_STATUS RemoveMessageListener(MessageListener* message_listener) { return m_message_listeners.RemoveByItem(message_listener); }
	OP_STATUS RemoveInteractionListener(InteractionListener* interaction_listener) { return m_interaction_listeners.RemoveByItem(interaction_listener); }
#ifdef IRC_SUPPORT
	OP_STATUS RemoveChatListener(ChatListener* chat_listener) { return m_chat_listeners.RemoveByItem(chat_listener); }
#endif // IRC_SUPPORT

	OP_STATUS GetAccountsModel(OpTreeModel** accounts_model) { *accounts_model = (OpTreeModel*) GetAccountsModel(); return OpStatus::OK; }
	OP_STATUS GetGroupsModel(OpTreeModel** accounts_model, UINT16 account_id, BOOL read_only);
	OP_STATUS ReleaseGroupsModel(OpTreeModel* index_model, BOOL commit);

	OP_STATUS SetIndexModelType(index_gid_t index_id, IndexTypes::ModelType type, IndexTypes::ModelAge age, INT32 flags, IndexTypes::ModelSort sort, BOOL ascending, IndexTypes::GroupingMethods grouping_method, message_gid_t selected_message);
	OP_STATUS GetIndexModelType(index_gid_t index_id, IndexTypes::ModelType& type, IndexTypes::ModelAge& age, INT32& flags, IndexTypes::ModelSort& sort, BOOL& ascending, IndexTypes::GroupingMethods& grouping_method, message_gid_t& selected_message);
	index_gid_t GetIndexIDByAddress(OpString& address);

	OP_STATUS GetIndexModel(OpTreeModel** index_model, index_gid_t index_id, INT32& start_pos);
	OP_STATUS ReleaseIndexModel(OpTreeModel* index_model);
#ifdef IRC_SUPPORT
	OP_STATUS GetChatRoomsModel(OpTreeModel** rooms_model) { *rooms_model = (OpTreeModel*) GetChatRoomsModel(); return OpStatus::OK; }
	OP_STATUS GetChattersModel(OpTreeModel** chatters_model, UINT16 account_id, const OpStringC& room);
	OP_STATUS DeleteChatRoom(UINT32 room_id);
#endif // IRC_SUPPORT

	OP_STATUS SubscribeFolder(OpTreeModel* groups_model, UINT32 item_id, BOOL subscribe);
	OP_STATUS CreateFolder(OpTreeModel* groups_model, UINT32* item_id);
	OP_STATUS UpdateFolder(OpTreeModel* groups_model, UINT32 item_id, OpStringC& path, OpStringC& name);
	OP_STATUS DeleteFolder(OpTreeModel* groups_model, UINT32 item_id);
	OP_STATUS UpdateNewsGroupIndexList();

	OP_STATUS Connect(UINT16 account_id);
	OP_STATUS JoinChatRoom(UINT16 account_id, const OpStringC& room, const OpStringC& password);
	OP_STATUS LeaveChatRoom(UINT16 account_id, const ChatInfo& room);
	OP_STATUS SendChatMessage(UINT16 account_id, EngineTypes::ChatMessageType type, const OpString& message, const ChatInfo& room, const OpStringC& chatter);

	OP_STATUS SetChatProperty(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value);
	OP_STATUS SendFile(UINT16 account_id, const OpStringC& chatter, const OpStringC& filename);

	OP_STATUS ImportMessage(Message* message, OpString& original_folder_name, BOOL move_to_sent);
	OP_STATUS ImportContact(OpString& full_name, OpString& email_address);

	BOOL	  ImportInProgress() const;
	void	  SetImportInProgress(BOOL status = TRUE);
	EngineTypes::ImporterId GetDefaultImporterId();

	OP_STATUS ImportOPML(const OpStringC& filename);
	OP_STATUS ExportOPML(const OpStringC& filename);

	OP_STATUS ExportToMbox(const index_gid_t index_id, const OpStringC& mbox_filename);

	OP_STATUS GetLexiconWords(uni_char **result, const uni_char *start_of_word, INT32 &count);

	UINT32 GetIndexCount();
	virtual Index* GetIndexById(index_gid_t id);
	Index* GetIndexRange(UINT32 range_start, UINT32 range_end, INT32& iterator);
	OP_STATUS RemoveIndex(index_gid_t id);
	Index* CreateIndex(index_gid_t parent_id, BOOL visible);

	OP_STATUS UpdateIndex(index_gid_t id);
	BOOL IsIndexMailingList(index_gid_t id);
	BOOL IsIndexNewsfeed(index_gid_t id);

	OP_STATUS ToClipboard(const OpINT32Vector& message_ids, index_gid_t source_index_id, BOOL cut);
	OP_STATUS PasteFromClipboard(index_gid_t dest_index_id);

	/** Returns index where items on clipboard come from
	  * @return Index of items on clipboard, or 0 if there are no items on the clipboard
	  */
	index_gid_t GetClipboardSource();

	/** Checks if all items in an index have bodies downloaded
	  * @param index_id Which index to check
	  * @return Whether all bodies are available
	  */
	BOOL	  HasAllBodiesDownloaded(index_gid_t index_id);

	/** Ensure that all items in an index have bodies downloaded
	  * @param index_id Which index to check
	  * @return Whether all bodies are available, will start body fetch otherwise
	  */
	BOOL	  EnsureAllBodiesDownloaded(index_gid_t index_id);

	/** Call when starting to edit a message
	  * @param message_id Message being edited
	  */
	OP_STATUS OnStartEditingMessage(message_gid_t message_id) { return m_messages_being_edited.Insert(message_id); }

	/** Call when stopping with editing a message
	  * @param message_id Message that is no longer being edited
	  */
	OP_STATUS OnStopEditingMessage(message_gid_t message_id) { m_messages_being_edited.Remove(message_id); return OpStatus::OK; }

	INT32     GetUnreadCount();

	OP_STATUS ReportAuthenticationDialogResult(const OpStringC& user_name, const OpStringC& password, BOOL cancel, BOOL remember);
	OP_STATUS ReportChangeNickDialogResult(UINT16 account_id, OpString& new_nick);
	OP_STATUS ReportRoomPasswordResult(UINT16 account_id, OpString& room, OpString& password);

	AutoDelete* GetAutoDelete() { return m_autodelete; }

	/** Load an RSS feed based on a URL (string or URL type)
	  * @param url URL to download
	  * @param downloaded_url Already downloaded URL
	  * @param manual_create Whether this was the result of user action
	  */
	OP_STATUS LoadRSSFeed(const OpStringC& url, URL& downloaded_url, BOOL manual_create = TRUE, BOOL open_panel = TRUE);

	OP_STATUS MailCommand(URL& url);

// ----------------------------------------------------

	// utility methods for character conversion
	// should be moved later.

	static OP_STATUS ConvertToBestChar8(IO OpString8& preferred_charset, BOOL force_charset, IN const OpStringC16& source, OUT OpString8& dest, int max_dst_length = -1, int* converted = NULL);
	static OP_STATUS ConvertToChar8(IO OpString8& charset, IN const OpStringC16& source, OUT OpString8& dest, OUT int& invalid_count, OUT OpString16& invalid_chars, int max_dst_length = -1, int* converted = NULL);
	static OP_STATUS ConvertToChar8(OutputConverter* converter, IN const OpStringC16& source, OUT OpString8& dest, int max_dst_length = -1, int* converted = NULL);

	InputConverterManager& GetInputConverter() { return m_input_converter_manager; }

	static OP_STATUS ConvertToChar16(IN const OpStringC8& charset, IN const OpStringC8& source, OUT OpString16& dest, BOOL accept_illegal_characters=FALSE, BOOL append = FALSE, int length = KAll);
	static OP_STATUS ConvertToChar16(InputConverter* converter, IN const OpStringC8& source, OUT OpString16& dest, BOOL append = FALSE, int length = KAll);

	// From Target:
	OP_STATUS Receive(OpMessage message);

	/**
	 * Check if M2 is the default mailer and ask user to set up it as the default mail client if needed
	 */
#ifdef M2_MAPI_SUPPORT
	void CheckDefaultMailClient();
#endif //M2_MAPI_SUPPORT

private:

	static OP_STATUS WriteViewPrefs(IndexTypes::ModelSort sort, BOOL ascending, IndexTypes::ModelType type, IndexTypes::GroupingMethods grouping_method);

	friend class MultipleMessageChanger;

	// do not expose copy constructor and assignment operator
    MessageEngine(const MessageEngine&);
	MessageEngine& operator =(const MessageEngine&);

	// private methods
	OP_STATUS MailCommandHTTP(URL& url);
#ifdef IRC_SUPPORT
	OP_STATUS MailCommandIRC(URL& url);
	OP_STATUS MailCommandChatTransfer(URL& url);
#endif
	OP_STATUS MailCommandNews(URL& url);

	OP_STATUS PasteFromClipboardToSpecialIndex(index_gid_t dest_index_id);
	OP_STATUS PasteFromClipboardToFolderIndex(index_gid_t dest_index_id);

	/* Splits the message_ids vector in a number of different vectors identified by the account number of the message
	 */
	OP_STATUS SplitIntoAccounts(const OpINT32Vector& message_ids, OpINT32HashTable<OpINT32Vector>& hash_table);
	
	/** Initialize store, message database and indexer. Done on startup or when the first account is created
	 */ 
	OP_STATUS InitMessageDatabase(OpString8& status);

	/**	Get the custom default_accounts.ini file if it exists, returns FALSE if it's not there or OOM
	 */
	BOOL GetCustomDefaultAccountsFile(OpFile& file);

	// singleton

    static MessageEngine* s_instance;

	// factories / creators

	GlueFactory*			m_glue_factory;

	// message management

	Store*					m_store;
	Indexer*				m_indexer;
	SEMessageDatabase*		m_message_database;

	BOOL					m_clipboard_cut;
	index_gid_t				m_clipboard_source;

	// chat stuff
	ChatNetworkManager*			m_chat_networks;
	ChatFileTransferManager*	m_chat_file_transfer_manager;

	// update of several messages at once

	unsigned				m_multiple_message_changes;
    BOOL                    m_deleting_trash;
	OpINTSortedVector		m_index_changed;
	BOOL					m_posted_index_changed;
	OpINTSet				m_messages_being_edited;

	// listeners

	OpVector<EngineListener>		m_engine_listeners;
	OpVector<AccountListener>		m_account_listeners;
	OpVector<MessageListener>		m_message_listeners;
	OpVector<InteractionListener>	m_interaction_listeners;
#ifdef IRC_SUPPORT
	OpVector<ChatListener>			m_chat_listeners;
#endif // IRC_SUPPORT

	// index models

	OpVector<IndexModel>	m_index_models;

	// group models

	OpVector<GroupsModel>	m_groups_models;

#ifdef IRC_SUPPORT
	// chat management
	ChatRoomsModel*			m_chatrooms_model;
#endif

	// accounts management

    AccountManager*			m_account_manager;
	AccountsModel*			m_accounts_model;

	// Async Password pending list
	BOOL					m_is_busy_asking_for_security_password;
	OpAutoVector<EngineAsyncPasswordContainer> m_async_password_list;

	// global preferences

	BOOL					m_will_do_recover_check_on_exit;	// will be on by default if it's an upgrade
	BOOL					m_has_asked_user_about_recovery;	// whether we have warned the user about that it will take some time to exit
	AutoDelete*				m_autodelete;
	MessageLoop*			m_loop;
	OpINT32Vector			m_messages_to_delete;

	ProgressInfo*			m_master_progress;

	InputConverterManager	m_input_converter_manager;

	INT32	m_mail_import_in_progress;

	URL						m_command_url;

#ifdef M2_MAPI_SUPPORT
	BOOL					m_check_default_mailer;
	MapiMessageListener*	m_mapi_listener;
#endif //M2_MAPI_SUPPORT
// ----------------------------------------------------

};

// ----------------------------------------------------

#endif //M2_SUPPORT

#endif // ENGINE_H
