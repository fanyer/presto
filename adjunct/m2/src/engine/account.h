// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef ACCOUNT_H
#define ACCOUNT_H

#ifdef M2_SUPPORT

#include "modules/locale/locale-enum.h"
#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/m2/src/engine/backend.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/offlinelog.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/util/misc.h"

class PrivacyManagerCallback;
class SSLSecurtityPasswordCallbackImpl;
class MessageDatabase;

class Account : public MessageBackend, public Link, public OpTimerListener, public MessageObject
{
/* Provider-functions: */
public:
    AccountTypes::AccountType GetType() const {return AccountTypes::UNDEFINED;}
    OP_STATUS SettingsChanged(BOOL startup=FALSE);
	UINT32  GetAuthenticationSupported() {return 0;}
	BOOL	HasContinuousConnection() const { return m_incoming_interface ? m_incoming_interface->HasContinuousConnection() : FALSE; }
	BOOL	IsScheduledForFetch(message_gid_t id) const { return m_incoming_interface ? m_incoming_interface->IsScheduledForFetch(id) : FALSE; }

    OP_STATUS FetchMessages(BOOL enable_signalling);
	OP_STATUS SelectFolder(UINT32 index_id);
	OP_STATUS RefreshFolder(UINT32 index_id);
	OP_STATUS RefreshAll();

	OP_STATUS StopFetchingMessages();

	OP_STATUS KeywordAdded(message_gid_t message_id, const OpStringC8& keyword);
	OP_STATUS KeywordRemoved(message_gid_t message_id, const OpStringC8& keyword);

	OP_STATUS CloseAllConnections();

	OP_STATUS FetchMessage(message_index_t index, BOOL user_request, BOOL force_complete);

	OP_STATUS SendMessages();
	OP_STATUS PrepareToSendMessage(message_gid_t id, BOOL anynymous, Message& rfr_message);
    OP_STATUS SendMessage(Message& message, BOOL anonymous);
	OP_STATUS StopSendingMessage();

	OP_STATUS Connect() { return Connect(TRUE); }
	OP_STATUS Connect(BOOL incoming) { return (incoming ?
												(m_incoming_interface ? m_incoming_interface->Connect() : OpStatus::OK) :
												(m_outgoing_interface ? m_outgoing_interface->Connect() : OpStatus::OK)); }
	OP_STATUS Disconnect() { return Disconnect(TRUE); }
	OP_STATUS Disconnect(BOOL incoming) { return (incoming ?
													(m_incoming_interface ? m_incoming_interface->Disconnect() : OpStatus::OK) :
													(m_outgoing_interface ? m_outgoing_interface->Disconnect() : OpStatus::OK)); }

	OP_STATUS AddIntroductionMail() { return AddOperaGeneratedMail(WELCOME_MESSAGE_FILENAME, Str::S_WELCOME_TO_M2_TITLE); }
	
	OP_STATUS AddOperaGeneratedMail(const uni_char* filename, Str::LocaleString subject_string);

	virtual void ResetRetrievedPassword();

	/** Check if an account backend already has a certain message
	  * @param message The message to check for
	  * @return TRUE if the backend has the message, FALSE if we don't know
	  */
	BOOL	  HasExternalMessage(Message& message) { return m_incoming_interface ? m_incoming_interface->HasExternalMessage(message) : FALSE; }

	/** Insert an external message into the account backend (if the backend keeps information on received messages), used for importing
	  * @param message Message to insert
	  */
	OP_STATUS InsertExternalMessage(Message& message) { return m_incoming_interface ? m_incoming_interface->InsertExternalMessage(message) : OpStatus::ERR; }

	/** Gets the index where a message was originally stored (override if backend uses its own folders)
	  * @param message_id A message to check
	  * @return Index where message was originally stored, or 0 if unknown
	  */
	index_gid_t GetMessageOrigin(message_gid_t message_id) { return m_incoming_interface ? m_incoming_interface->GetMessageOrigin(message_id) : 0; }

	OP_STATUS ToOnlineMode() { return m_offline_log.ReplayLog(); }

	BOOL	  IsOfflineLogBusy() const { return m_offline_log.IsBusy(); }

	void	  OnAccountAdded();

	/** Inserts an existing message into this account as if new
	  * @param message Message to copy - will reflect the new message on completion!
	  */
	OP_STATUS InternalCopyMessage(Message& message);

	/* Update old formats */
	OP_STATUS UpdateStore(int old_version);

    /* IRC */
	OP_STATUS JoinChatRoom(const OpStringC& room, const OpStringC& password, BOOL no_lookup = FALSE);
	OP_STATUS LeaveChatRoom(const ChatInfo& room);
	OP_STATUS SendChatMessage(EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& room, const OpStringC& chatter);
	OP_STATUS ChangeNick(OpString& new_nick);
	OP_STATUS SetChatProperty(const ChatInfo& room, const OpStringC& chatter, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value);
	OP_STATUS SendFile(const OpStringC& chatter, const OpStringC& filename);
	void	  ResetNickFlux(){ m_nick_in_flux = FALSE; }

    /* NNTP */
    OP_STATUS FetchNewGroups();

	BOOL		IsChatStatusAvailable(AccountTypes::ChatStatus chat_status);
	OP_STATUS	SetChatStatus(AccountTypes::ChatStatus chat_status) { return m_incoming_interface ? m_incoming_interface->SetChatStatus(chat_status) : OpStatus::OK; }
	AccountTypes::ChatStatus GetChatStatus(BOOL& is_connecting);

	/* NNTP newsgroups, IMAP folders, IRC chatrooms */
	UINT32    GetSubscribedFolderCount() { return (m_incoming_interface ? m_incoming_interface->GetSubscribedFolderCount() : 0); }
	OP_STATUS GetSubscribedFolderName(UINT32 index, OpString& name) { return (m_incoming_interface ? m_incoming_interface->GetSubscribedFolderName(index, name)  : OpStatus::ERR); }
	OP_STATUS AddSubscribedFolder(OpString& path) { return (m_incoming_interface ? m_incoming_interface->AddSubscribedFolder(path) : OpStatus::ERR); }
	OP_STATUS RemoveSubscribedFolder(const OpStringC& path) { return (m_incoming_interface ? m_incoming_interface->RemoveSubscribedFolder(path) : OpStatus::ERR); }
	OP_STATUS RemoveSubscribedFolder(UINT32 index) { return (m_incoming_interface ? m_incoming_interface->RemoveSubscribedFolder(index) : OpStatus::ERR); }
	OP_STATUS CommitSubscribedFolders() { return (m_incoming_interface ? m_incoming_interface->CommitSubscribedFolders() : OpStatus::ERR); }

	/* IMAP specific, create, rename and delete on server */
	OP_STATUS CreateFolder(OpString& completeFolderPath, BOOL subscribed) { return (m_incoming_interface ? m_incoming_interface->CreateFolder(completeFolderPath, subscribed) : OpStatus::ERR); }
	OP_STATUS DeleteFolder(OpString& completeFolderPath) { return  (m_incoming_interface ? m_incoming_interface->DeleteFolder(completeFolderPath) : OpStatus::ERR); }
	OP_STATUS RenameFolder(OpString& oldCompleteFolderPath, OpString& newCompleteFolderPath);

	/* POP3 specifics */
	OP_STATUS ServerCleanUp() { return (m_incoming_interface ? m_incoming_interface->ServerCleanUp() : OpStatus::ERR); }

	/* Chat specifics */
	OP_STATUS AcceptReceiveRequest(UINT port_number, UINT id) { return m_incoming_interface->AcceptReceiveRequest(port_number, id); }

	void      GetAllFolders() { if (m_incoming_interface) m_incoming_interface->GetAllFolders(); }
	void      StopFolderLoading() { if (m_incoming_interface) m_incoming_interface->StopFolderLoading(); }

	/* Jabber specifics */ // added by dvs
#ifdef JABBER_SUPPORT
	OP_STATUS SubscribeToPresence(const OpStringC& jabber_id, BOOL subscribe) { return m_incoming_interface->SubscribeToPresence(jabber_id, subscribe); }
	OP_STATUS AllowPresenceSubscription(const OpStringC& jabber_id, BOOL allow) { return m_incoming_interface->AllowPresenceSubscription(jabber_id, allow); }
#endif // JABBER_SUPPORT

	/* News and IMAP messages */
	OP_STATUS InsertMessage(message_gid_t message_id, index_gid_t destination_index);
	OP_STATUS RemoveMessage(message_gid_t message_id, BOOL permanently);
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

/* Observer-functions: */
public:
    OP_STATUS SignalStartSession(const MessageBackend* backend) {return MessageEngine::GetInstance()->SignalStartSession(IsIncomingBackend(backend));}
    OP_STATUS SignalEndSession(const MessageBackend* backend, int message_count, BOOL report_session) {return MessageEngine::GetInstance()->SignalEndSession(IsIncomingBackend(backend), message_count, report_session);}
	OP_STATUS Fetched(Message& message, BOOL headers_only = FALSE, BOOL register_as_new = TRUE);
    OP_STATUS Sent(message_gid_t uid, OP_STATUS transmit_status);
	BOOL      ResetNeedSignal() { BOOL needs = m_need_signal; m_need_signal = FALSE; return needs && (m_incoming_interface ? m_incoming_interface->Verbose() : FALSE); }

// ----------------------------------------------------
// functions for accounts telling engine about events

	void OnFolderAdded(const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable = TRUE) {MessageEngine::GetInstance()->OnFolderAdded(m_account_id, name, path, subscribedLocally, subscribedOnServer, editable);}
	void OnFolderRemoved(const OpStringC& path) {MessageEngine::GetInstance()->OnFolderRemoved(m_account_id, path);}
	void OnFolderRenamed(const OpStringC& old_path, const OpStringC& new_path) {MessageEngine::GetInstance()->OnFolderRenamed(m_account_id, old_path, new_path);}
	void OnFolderLoadingCompleted() { MessageEngine::GetInstance()->OnFolderLoadingCompleted(m_account_id); }

	void OnChangeNickRequired(const OpStringC& old_nick) {if (m_nick_in_flux) return; else m_nick_in_flux = TRUE; MessageEngine::GetInstance()->OnChangeNickRequired(m_account_id, old_nick);}
	void OnRoomPasswordRequired(const OpStringC& room) {MessageEngine::GetInstance()->OnRoomPasswordRequired(m_account_id, room);}
	void OnError(const OpStringC& errormessage, EngineTypes::ErrorSeverity severity=EngineTypes::GENERIC_ERROR);
	void OnError(Str::LocaleString errormessage, EngineTypes::ErrorSeverity severity=EngineTypes::GENERIC_ERROR);

#ifdef IRC_SUPPORT
	void OnChatMessageReceived(EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& chat_info, const OpStringC& chatter, BOOL is_private_chat) { MessageEngine::GetInstance()->OnChatMessageReceived(m_account_id, type, message, chat_info, chatter, is_private_chat); }
	void OnChatServerInformation(const OpStringC& server, const OpStringC& information) { MessageEngine::GetInstance()->OnChatServerInformation(m_account_id, server, information); }
	void OnChatRoomJoined(const ChatInfo& room) {MessageEngine::GetInstance()->OnChatRoomJoined(m_account_id, room);}
	void OnChatRoomLeft(const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason) {MessageEngine::GetInstance()->OnChatRoomLeft(m_account_id, room, kicker, kick_reason);}
	void OnChatterJoined(const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial) {MessageEngine::GetInstance()->OnChatterJoined(m_account_id, room, chatter, is_operator, is_voiced, prefix, initial);}
	void OnChatterLeft(const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker) {MessageEngine::GetInstance()->OnChatterLeft(m_account_id, room, chatter, message, kicker);}
	void OnChatPropertyChanged(const ChatInfo& room, const OpStringC& chatter, const OpStringC& changed_by, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value);
	void OnChatReconnecting(const ChatInfo& room) {MessageEngine::GetInstance()->OnChatReconnecting(m_account_id, room);}
	void OnWhoisReply(const OpStringC& nick, const OpStringC& user_id, const OpStringC& host,
					  const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message,
					  const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& channels) { MessageEngine::GetInstance()->OnWhoisReply(m_account_id, nick, user_id, host, real_name, server, server_info, away_message, logged_in_as, is_irc_operator, seconds_idle, signed_on, channels); }
	void OnInvite(const OpStringC& nick, const ChatInfo& room) { MessageEngine::GetInstance()->OnInvite(m_account_id, nick, room); }
	void OnFileReceiveRequest(const OpStringC& sender, const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id) { MessageEngine::GetInstance()->OnFileReceiveRequest(m_account_id, sender, filename, file_size, port_number, id); }
#endif // IRC_SUPPORT
	void OnTimeOut(OpTimer* timer);

private:
    UINT16                   m_account_id; //Unique number

#ifdef _DEBUG
	BOOL	m_is_initiated;
#endif

    MessageBackend*				m_incoming_interface;
    MessageBackend*				m_outgoing_interface;
    BOOL						m_is_temporary;
    OpString					m_account_category;
	BOOL						m_save_incoming_password;
	BOOL						m_save_outgoing_password;
	BOOL						m_need_signal;
	BOOL						m_nick_in_flux;

                                           //Example-values:
    AccountTypes::AccountType m_incoming_protocol; // "POP"  //Pop-server
	AccountTypes::AccountType m_outgoing_protocol; // "SMTP"  //SMTP-server
    UINT16  m_incoming_portnumber;         // 110  //default port for insecure pop
    UINT16  m_outgoing_portnumber;         // 25  //default port for insecure smtp
    BOOL      m_use_secure_connection_in;    // FALSE  //don't use POP over SSL
    BOOL      m_use_secure_connection_out;   // FALSE  //don't use SMTP over SSL
    BOOL      m_use_secure_authentication;   // FALSE  //Send passwords in plain text
	AccountTypes::AuthenticationType m_incoming_authentication_method; //CRAM-MD5 //Authenticate incoming using CRAM-MD5
	AccountTypes::AuthenticationType m_outgoing_authentication_method; //NONE		//Don't authenticate outgoing
	AccountTypes::AuthenticationType m_current_incoming_auth_method; //Last used authentication method. Not saved.
	AccountTypes::AuthenticationType m_current_outgoing_auth_method; //Last used authentication method. Not saved.

    INT16   m_incoming_timeout;            // 60  //1-minute timeout
    INT16   m_outgoing_timeout;            // 90  //90-second timeout

	BOOL	  m_delayed_remove_from_server;		 // FALSE // Delete messages on server after X days
	UINT32	  m_remove_from_server_delay; // 604 800 seconds (7 days) // The delay that a message should stay on the server before removed (in seconds)
	BOOL	  m_remove_from_server_only_if_read;	// FALSE // Remove the message on server after X days only if the message is marked as read
	BOOL      m_remove_from_server_only_if_complete_message; // TRUE // Remove the message on server after X days only if we have a full local copy
    OpTimer*  m_fetch_timer;
    UINT32    m_initial_poll_delay;          // 0  //Delay from account initialization to first automatic poll
    UINT32    m_poll_interval;               // 0  //Don't poll automatically
    UINT16    m_max_downloadsize;            // 1024  //Down't download messages larger than 1024Kb
    INT16     m_purge_age;                   // -1  //Never purge on age (no matter how many days old the messages are)
    INT32     m_purge_size;                  // -1  //Never purge on size (no matter how many Kb all the messages are)
    BOOL      m_download_bodies;             // FALSE  //First fetch headers. Then bodies when needed
    BOOL      m_download_attachments;        // FALSE  //Fetch attachments only when needed
    BOOL      m_leave_on_server;             // FALSE  //Delete mail on server after having downloaded them
	BOOL	  m_server_has_spam_filter;		 // FALSE  // Avoid using m2 spam filter for accounts that have spam filters on the server (eg Gmail IMAP)
	BOOL	  m_imap_bodystructure;			 // TRUE   //Server can handle BODYSTRUCTURE commands (IMAP)
	BOOL	  m_imap_idle;					 // TRUE   //Server can handle IDLE commands (IMAP)
	BOOL      m_permanent_delete;            // TRUE   //Delete mail on server when it is removed from trash (POP)
    BOOL      m_queue_outgoing;              // FALSE  //Send immediately
	BOOL	  m_send_after_checking;		 // FALSE  //Don't call SMTP after POP
	BOOL	  m_read_if_seen;				 // Mark message as read if already downloaded with other mailer
	BOOL	  m_sound_enabled;				 // enable the sound file.
	BOOL	  m_manual_check_enabled;		 // fetch messages when clicking "Get Mail"
	BOOL      m_add_contact_when_sending;    // automagically add a contact when you send an e-mail
	BOOL	  m_warn_if_empty_subject;		 // Warn if subject is empty when sending a message
	BOOL	  m_HTML_signature;				 // Tells wether the signature is in HTML format
	BOOL	  m_low_bandwidth;				 // Whether account is in low bandwidth mode
	unsigned  m_fetch_max_lines;			 // How many lines to fetch of each message body
	BOOL      m_fetch_only_text;			 // Whether to fetch onlny the text part of a message initially
	BOOL	  m_force_single_connection;	 // TRUE if server prefare single connection
	BOOL	  m_qresync;					 // TRUE if server handles QRESYNC in right way

    OpString  m_accountname;                 // UNI_L("FrodeGill@home pop-account")

    OpString  m_incoming_servername;         // UNI_L("mail.chello.no")
    OpString  m_incoming_username;           // UNI_L("frode.gill")

	OpMisc::SafeString<OpString> m_incoming_temp_login;
	OpMisc::SafeString<OpString> m_outgoing_temp_login;

    OpString  m_outgoing_servername;         // UNI_L("smtp.chello.no")
    OpString  m_outgoing_username;           // UNI_L("frode.gill")

    OpString8 m_charset;                     // "iso-8859-1"
	BOOL	  m_force_charset;				 // Set to TRUE if the outgoing message MUST use m_charset as its encoding
    BOOL      m_allow_8bit_header;
    BOOL      m_allow_8bit_transfer;
	BOOL	  m_allow_incoming_quotedstring_qp;	//Set to TRUE if "=?iso-8859-1?Q?dummy?=" should decode to "dummy", in violation of RFC2047 �5.3
    Message::TextpartSetting m_prefer_HTML_composing; // whether this accounts should be used for HTML or not
	AccountTypes::TextDirection m_default_direction;	// DIR_AUTOMATIC, DIR_LEFT_TO_RIGHT or DIR_RIGHT_TO_LEFT

    OpString  m_realname;                    // UNI_L("Frode Gill (private)")
    OpString  m_email;                       // UNI_L("frode.gill@chello.no")
    OpString  m_replyto;                     // UNI_L("frodegill_spam@chello.no")
    OpString  m_organization;                // UNI_L("")
    OpString8 m_xface;                       // "*0JG%XJ4#I_cjcG+^}7'M&l'[Z%*0&,u%;4#tOpl|CA12[c_A%p6R`MK~m7Of{{k;z]"/5~1H7]oCf`S>R/==bd`5)cKxQM!vQ"Z'j~d2OMh^V?tu?K,>s<%:u0i|g"*Rk<V;^6v5A})<U{xvYW]BF16*4mz]4O3lp_wi*<yX~1Q79fQ33Iy{}]v),]8GYKtH6q^l53qaNcwLvo>~B_mej"
    OpString  m_signature_file;              // UNI_L("C:\\signature.txt")
    OpString  m_fqdn;                        // UNI_L("mail.opera.com")  //Fully-qualified Domain Name
	OpString  m_original_fqdn;               // UNI_L("mail.opera.com")  //Fully-qualified Domain Name
    OpString8 m_personalization;             // "fgoc"  //personalization-string used in message-id (use blank for increased privacy, at the cost of some scoring-functionality)
	OpString  m_sound_file;
//    PGP*      m_pgp_key;
    OpString  m_reply_string;                // UNI_L("On %:Date:, you wrote in %g")
    OpString  m_followup_string;             // UNI_L("%f wrote:")
    OpString  m_forward_string;              // UNI_L("Found this %:Message-id: in %g:")

    INT8    m_linelength;                  // 76  //Wrap at column 76
	BOOL	  m_remove_signature_on_reply;
	INT16	  m_max_quotedepth_on_reply;
	INT16	  m_max_quotedepth_on_quickreply;

	BOOL	m_HTML_toolbar_displayed;

    OpString  m_incoming_optionsfile;         // UNI_L("FrodeGill@home pop-account.rc");
    OpString  m_outgoing_optionsfile;         // UNI_L("");

    OpString  m_incoming_logfile;             // UNI_L(""); //No logging

	OpFile*   m_incoming_logfile_ptr;
    OpString  m_outgoing_logfile;             // UNI_L(""); //No logging
    OpFile*   m_outgoing_logfile_ptr;

	OpString	m_auto_cc;
	OpString	m_auto_bcc;

	OfflineLog  m_offline_log;

	unsigned	m_sent_messages;

	int			m_default_store;

public:
    Account(MessageDatabase& database);
    ~Account();

    OP_STATUS Init(UINT16 account_id, OpString8& status);
	OP_STATUS Init() { OpString8 dummy; return Init(m_account_id, dummy); }
	OP_STATUS InitBackends();
    OP_STATUS PrepareToDie();

    /* If account_id==0, it will be given an id and put into the AccountManager list */
    OP_STATUS SaveSettings(BOOL force_flush=TRUE);
    OP_STATUS SetDefaults();
    UINT16  GetDefaultIncomingPort(BOOL secure) const {return m_incoming_interface ? m_incoming_interface->GetDefaultPort(secure) : 0; }
    UINT16  GetDefaultOutgoingPort(BOOL secure) const {return m_outgoing_interface ? m_outgoing_interface->GetDefaultPort(secure) : 0; }
	IndexTypes::Type GetIncomingIndexType() const {return m_incoming_interface ? m_incoming_interface->GetIndexType() : IndexTypes::FOLDER_INDEX;}
	IndexTypes::Type GetOutgoingIndexType() const {return m_outgoing_interface ? m_outgoing_interface->GetIndexType() : IndexTypes::FOLDER_INDEX;}

//    OP_STATUS AppendSignature(IN OpString& mail_body, OUT BOOL& signature_appended, IN BOOL reflow) const;
//    OP_STATUS ReplaceSignature(IN OpString& mail_body, OUT BOOL& signature_changed, IN BOOL reflow=FALSE, IN UINT16 old_signature_account_id=0) const;


public:
	unsigned  GetSentMessagesCount();

    BOOL      GetIsTemporary() const {return m_is_temporary;}
    OP_STATUS GetAccountCategory(OpString& category) const {return category.Set(m_account_category);}

	void	  SetSaveIncomingPassword(BOOL save_incoming_password) {m_save_incoming_password = save_incoming_password;}
	void	  SetSaveOutgoingPassword(BOOL save_outgoing_password) {m_save_outgoing_password = save_outgoing_password;}

    virtual UINT16  GetAccountId() const {return m_account_id;}
    MessageBackend* GetIncomingBackend() const {return m_incoming_interface;}
    MessageBackend* GetOutgoingBackend() const {return m_outgoing_interface;}
    BOOL      IsIncomingBackend(const MessageBackend* backend) const {return backend==m_incoming_interface;}
    BOOL      IsOutgoingBackend(const MessageBackend* backend) const {return backend==m_outgoing_interface;}

	ProgressInfo& GetProgress(BOOL incoming);
	ProgressInfo& GetProgress() { return GetProgress(TRUE); }

	static OpStringC8 GetProtocolName(AccountTypes::AccountType protocol);
	static AccountTypes::AccountType GetProtocolFromName(const OpStringC8& name);
	static AccountTypes::AccountType GetOutgoingForIncoming(AccountTypes::AccountType incoming);

    AccountTypes::AccountType GetIncomingProtocol() const {return m_incoming_protocol;}
    AccountTypes::AccountType GetOutgoingProtocol() const {return m_outgoing_protocol;}
    UINT16  GetIncomingPort() const {return m_incoming_portnumber;}
    UINT16  GetOutgoingPort() const {return m_outgoing_portnumber;}
    BOOL      GetUseSecureConnectionIn() const {return m_use_secure_connection_in;}
    BOOL      GetUseSecureConnectionOut() const {return m_use_secure_connection_out;}
    BOOL      GetUseSecureAuthentication() const {return m_use_secure_authentication;}
	AccountTypes::AuthenticationType GetIncomingAuthenticationMethod() const {return m_incoming_authentication_method;}
	AccountTypes::AuthenticationType GetOutgoingAuthenticationMethod() const {return m_outgoing_authentication_method;}
	AccountTypes::AuthenticationType GetCurrentIncomingAuthMethod() const;
	AccountTypes::AuthenticationType GetCurrentOutgoingAuthMethod() const;
	UINT32  GetIncomingAuthenticationSupported() const; //bit-flags of all methods supported by incoming backend
	UINT32  GetOutgoingAuthenticationSupported() const; //bit-flags of all methods supported by outgoing backend
    INT16   GetIncomingTimeout() const {return m_incoming_timeout;}
    INT16   GetOutgoingTimeout() const {return m_outgoing_timeout;}
	BOOL	IsDelayedRemoveFromServerEnabled() const { return m_delayed_remove_from_server; }
	UINT32	GetRemoveFromServerDelay() const { return m_remove_from_server_delay; }
	BOOL	IsRemoveFromServerOnlyIfCompleteMessage() const { return m_remove_from_server_only_if_complete_message; }
	BOOL	IsRemoveFromServerOnlyIfMarkedAsRead() const { return m_remove_from_server_only_if_read; }
    UINT32  GetInitialPollDelay() const {return m_initial_poll_delay;}
    UINT32  GetPollInterval() const {return m_poll_interval;}
    UINT16  GetMaxDownloadSize() const {return m_max_downloadsize;}
    INT16   GetPurgeAge() const {return m_purge_age;}
    INT32   GetPurgeSize() const {return m_purge_size;}
	BOOL      GetDownloadBodies(BOOL get_setting = FALSE) const {return get_setting ? m_download_bodies : (m_download_bodies && !GetLowBandwidthMode());}
    BOOL      GetDownloadAttachments() const {return m_download_attachments;}
	BOOL      GetLeaveOnServer(BOOL get_setting = FALSE) const {return get_setting ? m_leave_on_server : (m_leave_on_server || GetLowBandwidthMode());}
	BOOL      GetServerHasSpamFilter() { return m_server_has_spam_filter;}
	BOOL	  GetImapBodyStructure() const {return m_imap_bodystructure;}
	BOOL	  GetImapIdle() const {return m_imap_idle;}
	BOOL	  GetPermanentDelete() const {return m_permanent_delete;}
    BOOL      GetQueueOutgoing() const {return m_queue_outgoing;}
	BOOL	  GetSendQueuedAfterChecking() const {return m_send_after_checking;}
    BOOL      GetMarkReadIfSeen() const {return m_read_if_seen;}
    const OpStringC& GetAccountName() const {return m_accountname;}
    const OpStringC& GetIncomingServername() const {return m_incoming_servername;}
    OP_STATUS GetIncomingServername(OpString8& servername) const;
	OP_STATUS GetIncomingServernames(OpAutoVector<OpString> &servernames) const;
	const OpStringC& GetIncomingUsername() const {return m_incoming_username;}
    OP_STATUS GetIncomingUsername(OpString8& username) const;
#ifdef WAND_CAP_GANDALF_3_API
    OP_STATUS GetIncomingPassword(PrivacyManagerCallback* callback) const;
#else
	OP_STATUS GetIncomingPassword(OpString& password) const;
#endif // WAND_CAP_GANDALF_3_API
    const OpStringC& GetOutgoingServername() const {return m_outgoing_servername;}
    OP_STATUS GetOutgoingServername(OpString8& servername) const;
	const OpStringC& GetOutgoingUsername() const {return m_outgoing_username;}
    OP_STATUS GetOutgoingUsername(OpString8& username) const;
#ifdef WAND_CAP_GANDALF_3_API
    OP_STATUS GetOutgoingPassword(PrivacyManagerCallback* callback) const;
#else
	OP_STATUS GetOutgoingPassword(OpString& password) const;
#endif // WAND_CAP_GANDALF_3_API
    OP_STATUS GetCharset(OpString8& charset) const {return charset.Set(m_charset);}
	AccountTypes::TextDirection GetDefaultDirection() const { return m_default_direction; }
	BOOL	  GetForceCharset() const {return m_force_charset;}
    BOOL      GetAllow8BitHeader() const {return m_allow_8bit_header;}
    BOOL      GetAllow8BitTransfer() const {return m_allow_8bit_transfer;}
	BOOL	  GetAllowIncomingQuotedstringQP() const {return m_allow_incoming_quotedstring_qp;}
    Message::TextpartSetting PreferHTMLComposing() const {return m_prefer_HTML_composing;}
    OP_STATUS GetRealname(OpString& realname) const {return realname.Set(m_realname);}
    OP_STATUS GetEmail(OpString& email) const {return email.Set(m_email);}
    OP_STATUS GetEmail(OpString8& email) const; //Returns ERR_PARSING_FAILED if address is invalid
    OP_STATUS GetFormattedEmail(OpString16& email, BOOL do_quote_pair_encode) const;
    OP_STATUS GetReplyTo(OpString& replyto) const {return replyto.Set(m_replyto);}
    OP_STATUS GetOrganization(OpString& organization) const {return organization.Set(m_organization);}
    OP_STATUS GetXFace(OpString8& xface) const {return xface.Set(m_xface);}
    OP_STATUS GetSignatureFile(OpString& signature_file) const {return signature_file.Set(m_signature_file);}
    OP_STATUS GetSignature(OpString& signature, BOOL& isHTML) const;
    OP_STATUS GetFQDN(OpString& fqdn);
    OP_STATUS GetPersonalizationString(OpString8& personalization) const {return personalization.Set(m_personalization);}
	OP_STATUS GetSoundFile(OpString& path) const {return path.Set(m_sound_file);}
	BOOL	  GetSoundEnabled() const {return m_sound_enabled;}
	BOOL	  GetManualCheckEnabled() {return m_manual_check_enabled;}
//    PGP*      m_pgp_key;
    OP_STATUS GetReplyString(OpString& reply_string) const {return reply_string.Set(m_reply_string);}
    OP_STATUS GetFollowupString(OpString& followup_string) const {return followup_string.Set(m_followup_string);}
    OP_STATUS GetForwardString(OpString& forward_string) const {return forward_string.Set(m_forward_string);}
    INT8    GetLinelength() const {return m_linelength;}
	BOOL	  GetRemoveSignatureOnReply() const {return m_remove_signature_on_reply;}
	INT16	  GetMaxQuoteDepthOnReply() const {return m_max_quotedepth_on_reply;}
	INT16	  GetMaxQuoteDepthOnQuickReply() const {return m_max_quotedepth_on_quickreply;}
    OP_STATUS GetIncomingOptionsFile(OpString& options_file) const {return options_file.Set(m_incoming_optionsfile);}
    OP_STATUS GetOutgoingOptionsFile(OpString& options_file) const {return options_file.Set(m_outgoing_optionsfile);}
    OP_STATUS GetIncomingLogFile(OpString& log_file) const {return log_file.Set(m_incoming_logfile);}
    OP_STATUS GetOutgoingLogFile(OpString& log_file) const {return log_file.Set(m_outgoing_logfile);}
	BOOL	  BackendHasLogging(const MessageBackend* backend) const { return (backend == m_incoming_interface && m_incoming_logfile.HasContent()) || (backend == m_outgoing_interface && m_outgoing_logfile.HasContent()); }
	OP_STATUS GetFolderPath(AccountTypes::FolderPathType type, OpString& folder_path) const {return m_incoming_interface ? m_incoming_interface->GetFolderPath(type, folder_path) : OpStatus::OK;}
	BOOL      GetAddContactWhenSending() const {return m_add_contact_when_sending;}
	BOOL      GetWarnIfEmptySubject() const {return m_warn_if_empty_subject;}
    OP_STATUS GetAutoCC(OpString& auto_cc) const {return auto_cc.Set(m_auto_cc);}
    OP_STATUS GetAutoBCC(OpString& auto_bcc) const {return auto_bcc.Set(m_auto_bcc);}
	int		  GetDefaultStore(BOOL get_setting = FALSE) const;
	BOOL	  GetLowBandwidthMode() const { return m_low_bandwidth; }
	unsigned  GetFetchMaxLines() const { return m_fetch_max_lines; }
	BOOL	  GetFetchOnlyText(BOOL get_setting = FALSE) const { return get_setting ? m_fetch_only_text : (GetLowBandwidthMode() || m_fetch_only_text); }

    void      SetAccountId(UINT16 account_id) {m_account_id=account_id;}
    void      SetIncomingProtocol(AccountTypes::AccountType protocol) {m_incoming_protocol = protocol;} //Use with care, backend should be updated too
	void      SetOutgoingProtocol(AccountTypes::AccountType protocol) {m_outgoing_protocol = protocol;} //Use with care, backend should be updated too
    void      SetIsTemporary(BOOL temporary);
    OP_STATUS SetAccountCategory(const OpStringC& category) {return m_account_category.Set(category);}
    void      SetIncomingPort(UINT16 port) {m_incoming_portnumber=port;}
	void      SetOutgoingPort(UINT16 port) {m_outgoing_portnumber=port;}
    void      SetUseSecureConnectionIn(BOOL use_secure_connection) {m_use_secure_connection_in=use_secure_connection;}
    void      SetUseSecureConnectionOut(BOOL use_secure_connection) {m_use_secure_connection_out=use_secure_connection;}
    void      SetUseSecureAuthentication(BOOL use_secure_connection) {m_use_secure_authentication=use_secure_connection;}
	AccountTypes::AuthenticationType SetAuthenticationMethod(BOOL incoming, AccountTypes::AuthenticationType method); //Will return the method actually set - if backend don't support the requested one the best one will be set instead
	AccountTypes::AuthenticationType SetIncomingAuthenticationMethod(AccountTypes::AuthenticationType method) {return SetAuthenticationMethod(TRUE, method);}
	AccountTypes::AuthenticationType SetOutgoingAuthenticationMethod(AccountTypes::AuthenticationType method) {return SetAuthenticationMethod(FALSE, method);}
	void	  SetCurrentIncomingAuthMethod(AccountTypes::AuthenticationType auth_method) {m_current_incoming_auth_method=auth_method;}
	void	  SetCurrentOutgoingAuthMethod(AccountTypes::AuthenticationType auth_method) {m_current_outgoing_auth_method=auth_method;}
    void      SetIncomingTimeout(INT16 incoming_timeout) {m_incoming_timeout=incoming_timeout;}
    void      SetOutgoingTimeout(INT16 outgoing_timeout) {m_outgoing_timeout=outgoing_timeout;}
	void	  SetDelayedRemoveFromServerEnabled(BOOL delayed_remove_from_server)  {m_delayed_remove_from_server = delayed_remove_from_server; }
	void 	  SetRemoveFromServerDelay(UINT32 remove_from_server_delay) { m_remove_from_server_delay = remove_from_server_delay; }
	void	  SetRemoveFromServerOnlyIfCompleteMessage(BOOL remove_from_server_only_if_complete_message) { m_remove_from_server_only_if_complete_message = remove_from_server_only_if_complete_message; }
	void	  SetRemoveFromServerOnlyIfMarkedAsRead(BOOL remove_from_server_only_if_read) { m_remove_from_server_only_if_read = remove_from_server_only_if_read; }
    void      SetInitialPollDelay(UINT32 poll_delay) {m_initial_poll_delay=poll_delay;}
    void      SetPollInterval(UINT32 poll_interval) {m_poll_interval=poll_interval;}
	void	  MinimumLoginDelay(UINT32 login_delay) {if (m_poll_interval>0 && m_poll_interval<login_delay) m_poll_interval=login_delay;}
    void      SetMaxDownloadSize(UINT16 max_downloadsize) {m_max_downloadsize=max_downloadsize;}
    void      SetPurgeAge(INT16 purge_age) {m_purge_age=purge_age;}
    void      SetPurgeSize(INT32 purge_size) {m_purge_size=purge_size;}
    void      SetDownloadBodies(BOOL download_bodies) {m_download_bodies=download_bodies;}
    void      SetDownloadAttachments(BOOL download_attachments) {m_download_attachments=download_attachments;}
    void      SetLeaveOnServer(BOOL leave_on_server) {m_leave_on_server=leave_on_server;}
	void      SetServerHasSpamFilter(BOOL server_has_spam_filter) {m_server_has_spam_filter=server_has_spam_filter;}
	void	  SetImapBodyStructure(BOOL bodystructure) {m_imap_bodystructure = bodystructure;}
	void	  SetImapIdle(BOOL idle) {m_imap_idle = idle;}
	void	  SetPermanentDelete(BOOL permanent_delete) { m_permanent_delete = permanent_delete; }
    void      SetQueueOutgoing(BOOL queue_outgoing) {m_queue_outgoing=queue_outgoing;}
	void	  SetSendQueuedAfterChecking(BOOL send_after_checking) {m_send_after_checking = send_after_checking;}
    void      SetMarkReadIfSeen(BOOL read_if_seen) {m_read_if_seen=read_if_seen;}
    OP_STATUS SetAccountName(const OpStringC& accountname);
    OP_STATUS SetIncomingServername(const OpStringC16& servername) {OP_STATUS ret = m_incoming_servername.Set(servername); m_incoming_servername.Strip(); return ret;}
    OP_STATUS SetIncomingUsername(const OpStringC16& username) {return m_incoming_username.Set(username);}
    OP_STATUS SetIncomingPassword(const OpStringC& password, BOOL remember = TRUE);
    OP_STATUS SetOutgoingServername(const OpStringC16& servername) {OP_STATUS ret = m_outgoing_servername.Set(servername); m_outgoing_servername.Strip(); return ret;}
    OP_STATUS SetOutgoingUsername(const OpStringC16& username) {return m_outgoing_username.Set(username);}
    OP_STATUS SetOutgoingPassword(const OpStringC& password, BOOL remember = TRUE);
    OP_STATUS SetCharset(const OpStringC8& charset) {return m_charset.Set(charset);}
	void	  SetDefaultDirection(AccountTypes::TextDirection direction) { m_default_direction = direction; }
	void      SetForceCharset(BOOL force_charset) {m_force_charset=force_charset;}
    void      SetAllow8BitHeader(BOOL allow_8bit_header) {m_allow_8bit_header=allow_8bit_header;}
    void      SetAllow8BitTransfer(BOOL allow_8bit_transfer) {m_allow_8bit_transfer=allow_8bit_transfer;}
	void	  SetAllowIncomingQuotedstringQP(BOOL allow_incoming_quotedstring_qp) {m_allow_incoming_quotedstring_qp=allow_incoming_quotedstring_qp;}
    void      SetPreferHTMLComposing(Message::TextpartSetting setting) {m_prefer_HTML_composing=setting;}
    OP_STATUS SetRealname(const OpStringC& realname) {return m_realname.Set(realname);}
    OP_STATUS SetEmail(const OpStringC& email) {OP_STATUS ret = m_email.Set(email); m_email.Strip(); return ret;}
    OP_STATUS SetReplyTo(const OpStringC& replyto) {OP_STATUS ret = m_replyto.Set(replyto); m_replyto.Strip(); return ret;}
    OP_STATUS SetOrganization(const OpStringC& organization) {return m_organization.Set(organization);}
    OP_STATUS SetXFace(const OpStringC8& xface) {return m_xface.Set(xface);}
    OP_STATUS SetSignatureFile(const OpStringC& signature_file) {return m_signature_file.Set(signature_file);}
    OP_STATUS SetSignature(const OpStringC& signature, BOOL isHTML, BOOL overwrite_existing=TRUE);
//    OP_STATUS SetFQDN(const OpStringC& fqdn) {return m_fqdn.Set(fqdn);}
    OP_STATUS SetPersonalizationString(const OpStringC8& personalization) {return m_personalization.Set(personalization);}
	OP_STATUS SetSoundFile(OpString& path) {return m_sound_file.Set(path);}
	void	  SetSoundEnabled(BOOL enabled) { m_sound_enabled = enabled;}
	void	  SetManualCheckEnabled(BOOL enabled) { m_manual_check_enabled = enabled;}
//    PGP*      m_pgp_key;
    OP_STATUS SetReplyString(const OpStringC& reply_string) {return m_reply_string.Set(reply_string);}
    OP_STATUS SetFollowupString(const OpStringC& followup_string) {return m_followup_string.Set(followup_string);}
    OP_STATUS SetForwardString(const OpStringC& forward_string) {return m_forward_string.Set(forward_string);}
    void      SetLinelength(INT8 linelength) {m_linelength=linelength;}
	void	  SetRemoveSignatureOnReply(BOOL remove_signature_on_reply) {m_remove_signature_on_reply=remove_signature_on_reply;}
	void	  SetMaxQuoteDepthOnReply(INT16 max_quotedepth_on_reply) {m_max_quotedepth_on_reply=max_quotedepth_on_reply;}
	void	  SetMaxQuoteDepthOnQuickReply(INT16 max_quotedepth_on_quickreply) {m_max_quotedepth_on_quickreply=max_quotedepth_on_quickreply;}
    OP_STATUS SetIncomingOptionsFile(const OpStringC& options_file) {return m_incoming_optionsfile.Set(options_file);}
    OP_STATUS SetOutgoingOptionsFile(const OpStringC& options_file) {return m_outgoing_optionsfile.Set(options_file);}
    OP_STATUS SetIncomingLogFile(const OpStringC& log_file) {return m_incoming_logfile.Set(log_file);}
    OP_STATUS SetOutgoingLogFile(const OpStringC& log_file) {return m_outgoing_logfile.Set(log_file);}
	OP_STATUS SetFolderPath(AccountTypes::FolderPathType type, const OpStringC& folder_path) {return m_incoming_interface ? m_incoming_interface->SetFolderPath(type, folder_path) : OpStatus::OK;}
	const char* GetIcon(BOOL progress_icon = FALSE);
	void      SetAddContactWhenSending(BOOL add_contact_when_sending) {m_add_contact_when_sending = add_contact_when_sending;}
	void      SetWarnIfEmptySubject(BOOL warn_if_empty_subject) {m_warn_if_empty_subject = warn_if_empty_subject;}
    OP_STATUS SetAutoCC(const OpStringC& auto_cc) {OP_STATUS ret = m_auto_cc.Set(auto_cc); m_auto_cc.Strip(); return ret;}
    OP_STATUS SetAutoBCC(const OpStringC& auto_bcc) {OP_STATUS ret = m_auto_bcc.Set(auto_bcc); m_auto_bcc.Strip(); return ret;}
	OP_STATUS SetDefaultStore(int default_store);
	void	  SetFetchOnlyText(BOOL fetch_only_text) { m_fetch_only_text = fetch_only_text; }
	void	  SetLowBandwidthMode(BOOL low_bandwidth) { m_low_bandwidth = low_bandwidth;  GetProgress().OnProgressChanged(GetProgress()); }

	OP_STATUS ReadAccountSettings(PrefsFile * prefs_file, const char * ini_key, BOOL import = FALSE);
	/**
	 * Creates default name for incoming options file.
	 */
	OP_STATUS CreateIncomingOptionsFileName();
	OP_STATUS CreateOutgoingOptionsFileName();
	OP_STATUS CreateSignatureFileName();

    OP_STATUS LogIncoming(const OpStringC8& heading, const OpStringC8& text);
    OP_STATUS LogOutgoing(const OpStringC8& heading, const OpStringC8& text);

    UINT      GetStartOfDccPool() const;
    UINT      GetEndOfDccPool() const;
    void	  SetStartOfDccPool(UINT port);
    void      SetEndOfDccPool(UINT port);
	void      SetCanAcceptIncomingConnections(BOOL can_accept);
	BOOL      GetCanAcceptIncomingConnections() const;
	void	  SetOpenPrivateChatsInBackground(BOOL open_in_background);
	BOOL	  GetOpenPrivateChatsInBackground() const;
	OP_STATUS GetPerformWhenConnected(OpString& commands) const;
	OP_STATUS SetPerformWhenConnected(const OpStringC& commands);

	void	  SetForceSingleConnection(BOOL value) { m_force_single_connection = value; }
	BOOL	  GetForceSingleConnection() { return m_force_single_connection; }

	void	  EnableQRESYNC(BOOL value) { m_qresync = value; }
	BOOL	  IsQRESYNCEnabled() { return m_qresync; }
public:

	// FIXME: move to some better place

	enum presence_t
	{
		PRESENCE_UNAVAILABLE,
		PRESENCE_AWAY,
		PRESENCE_CHAT,
		PRESENCE_DO_NOT_DISTURB,
		PRESENCE_INACTIVE,
		PRESENCE_AVAILABLE
	};

	// callback from presence-aware backends FIXME: move later

	OP_STATUS PresenceChanged(const uni_char* identity, presence_t presence);

	OP_STATUS GetAuthenticationString(AccountTypes::AuthenticationType type, OpString& string) const;

	OP_STATUS ReadPasswordsFromAccounts(PrefsFile* prefs_file, const OpStringC8& section, BOOL import = FALSE);
	void      HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
private:
	SSLSecurtityPasswordCallbackImpl *m_askpasswordcontext;
	OpString8				m_encrypted_incoming_password;
	OpString8				m_encrypted_outgoing_password;

private:
	OP_STATUS CreateBackendObject(AccountTypes::AccountType account_type, Account* account, MessageBackend*& backend);
    OP_STATUS Log(OpFile*& file, const OpStringC8& heading, const OpStringC8& text);
	OP_STATUS StartAutomaticFetchTimer(BOOL startup = FALSE);

    Account(const Account&);
    Account& operator =(const Account&);

	OP_STATUS DecryptPassword(const OpStringC8& encrypted_password, OpString& plaintext_password, const char * key);
};

#endif //M2_SUPPORT

#endif // ACCOUNT_H
