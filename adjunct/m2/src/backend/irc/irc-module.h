// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef IRC_MODULE_H
#define IRC_MODULE_H

#ifdef IRC_SUPPORT
// ----------------------------------------------------

#include "irc-protocol.h"

#include "adjunct/m2/src/backend/ProtocolBackend.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/SleepManager.h"
#include "modules/util/adt/opvector.h"
#include "chat-filetransfer.h"

class IRCCommandInterpreter;
class PrefsFile;

//********************************************************************
//
//	IRCBackend
//
//********************************************************************

class IRCBackend
:	public ProtocolBackend,
	public IRCProtocolOwner,
	public HotlistModelListener,
	public SleepListener,
	public MessageObject
{
public:
	// Constructor / destructor.
	IRCBackend(MessageDatabase& database);
	virtual ~IRCBackend();

	// ProtocolBackend:
    OP_STATUS Init(Account* account);
	AccountTypes::AccountType GetType() const {return AccountTypes::IRC;}
    OP_STATUS SettingsChanged(BOOL startup=FALSE);
	BOOL	  IsBusy() const {return FALSE;}
    UINT16  GetDefaultPort(BOOL secure) const { return secure ? 194 : 194; } //Use 6667 as default instead?
	UINT32  GetAuthenticationSupported() {return 1<<AccountTypes::NONE;}
    OP_STATUS PrepareToDie() { return OpStatus::ERR; }

    OP_STATUS FetchHeaders(BOOL enable_signalling) {return OpStatus::ERR;}
    OP_STATUS FetchHeaders(const OpStringC8& internet_location, message_index_t index=0) {return OpStatus::ERR;}
    OP_STATUS FetchMessages(BOOL enable_signalling) {return OpStatus::ERR;}
    OP_STATUS FetchMessage(const OpStringC8& internet_location, message_index_t index=0) {return OpStatus::ERR;}
	OP_STATUS FetchMessage(message_index_t idx, BOOL user_request, BOOL force_complete) {return OpStatus::ERR;}

	OP_STATUS SelectFolder(UINT32 index_id, const OpStringC16& folder) { return OpStatus::ERR;}

	OP_STATUS StopFetchingMessages() {return OpStatus::ERR;}
	OP_STATUS StopSendingMessage() {return OpStatus::ERR;}

	OP_STATUS CloseConnection();

	OP_STATUS JoinChatRoom(const OpStringC& room, const OpStringC& password, BOOL no_lookup = FALSE);
	OP_STATUS LeaveChatRoom(const ChatInfo& chat_info);
	OP_STATUS SendChatMessage(EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& chat_info, const OpStringC& chatter);
	OP_STATUS ChangeNick(OpString& new_nick) { return m_protocol.get() != 0 ? m_protocol->SetNick(new_nick) : OpStatus::OK; };

	OP_STATUS SetChatProperty(const ChatInfo& chat_info, const OpStringC& chatter, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value);
	OP_STATUS SendFile(const OpStringC& chatter, const OpStringC& filename);

	BOOL		IsChatStatusAvailable(AccountTypes::ChatStatus chat_status);
	OP_STATUS	SetChatStatus(AccountTypes::ChatStatus chat_status);
	AccountTypes::ChatStatus	GetChatStatus(BOOL& is_connecting);

    OP_STATUS FetchNewGroups() {return OpStatus::ERR;}

	OP_STATUS InsertExternalMessage(Message& message) { return OpStatus::ERR; }

	// Chatroom handling
	UINT32    GetSubscribedFolderCount();
	OP_STATUS GetSubscribedFolderName(UINT32 index, OpString& name);
	OP_STATUS AddSubscribedFolder(OpString& name)
	{
	    OpString empty;
	    return AddSubscribedFolder(name, empty);
	}
	OP_STATUS AddSubscribedFolder(OpString& name, OpString& password);
	OP_STATUS RemoveSubscribedFolder(UINT32 index);
	OP_STATUS RemoveSubscribedFolder(const OpStringC& path);

	void      GetAllFolders();
	void      StopFolderLoading();

	OP_STATUS ServerCleanUp() {return OpStatus::ERR;}

	OP_STATUS AcceptReceiveRequest(UINT port_number, UINT id);
	UINT GetStartOfDccPool() const { return m_start_dcc_pool; }
	UINT GetEndOfDccPool() const { return m_end_dcc_pool; }
	void SetStartOfDccPool(UINT port) { m_start_dcc_pool = port; }
	void SetEndOfDccPool(UINT port) { m_end_dcc_pool = port; }
	void SetCanAcceptIncomingConnections(BOOL can_accept) { m_can_accept_incoming_connections = can_accept; }
	BOOL GetCanAcceptIncomingConnections() const { return m_can_accept_incoming_connections; }
	void SetOpenPrivateChatsInBackground(BOOL open_in_background) { m_open_private_chats_in_background = open_in_background; }
	BOOL GetOpenPrivateChatsInBackground() const { return m_open_private_chats_in_background; }
	OP_STATUS GetPerformWhenConnected(OpString& commands) const;
	OP_STATUS SetPerformWhenConnected(const OpStringC& commands);

	OP_STATUS RequestConnect();
    OP_STATUS Connect();
    OP_STATUS Disconnect();

	// SleepListener:
	void OnSleep();
	void OnWakeUp();

	// MessageObject:
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// Methods.
	OP_STATUS			ReadSettings();
	OP_STATUS			WriteSettings();

private:
	// No copy or assignment.
	IRCBackend(const IRCBackend&);
	IRCBackend& operator =(const IRCBackend&);

	// Internal classes.
	class Room
	{
	public:
		OpString m_room_name;
		OpString m_password;
	};

	// IRCProtocolOwner.
	virtual void OnConnectionLost(BOOL retry = TRUE);
	virtual void OnRestartRequested();
	virtual void OnProtocolReady();
	virtual void OnInitialPresence(const OpStringC& nick);
	virtual void OnPresenceUpdate(const OpStringC& nick, const OpStringC& away_reason = UNI_L(""));
	virtual void OnPresenceOffline(const OpStringC& nick);
	virtual void OnIdentdConnectionEstablished(const OpStringC &connection_to);
	virtual void OnIdentdIdentConfirmed(const OpStringC8 &request, const OpStringC8 &response);
	virtual void OnRawMessageReceived(const OpStringC8& message);
	virtual void OnRawMessageSent(const OpStringC8& message);

	virtual void OnNicknameInUse(const OpStringC& nick);
	virtual void OnChannelPasswordRequired(const ChatInfo& chat_room);

	virtual void OnCTCPPrivateAction(const ChatInfo& chat_info, const OpStringC& action, BOOL self_action);
	virtual void OnCTCPChannelAction(const ChatInfo& chat_info, const OpStringC& action, const OpStringC& from);
	virtual void OnDCCSendRequest(const OpStringC& sender, const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id);

	virtual void OnServerMessage(const OpStringC& message);
	virtual void OnServerInformation(const OpStringC& information);
	virtual void OnPrivateMessage(const ChatInfo& chat_info, const OpStringC& message, BOOL self_message);
	virtual void OnChannelMessage(const ChatInfo& chat_info, const OpStringC& message, const OpStringC& from);
	virtual void OnJoinedChannel(const ChatInfo& chat_room, const OpStringC &nick);
	virtual void OnJoinedChannelPassword(const ChatInfo& chat_room, const OpStringC &password);
	virtual void OnLeftChannel(const ChatInfo& chat_room, const OpStringC &nick, const OpStringC& leave_reason);
	virtual void OnQuit(const OpStringC& nick, const OpStringC& quit_message);
	virtual void OnKickedFromChannel(const ChatInfo& chat_room, const OpStringC& nick, const OpStringC& kicker, const OpStringC &kick_reason);
	virtual void OnTopicChanged(const ChatInfo& chat_room, const OpStringC& nick, const OpStringC& topic);
	virtual void OnNickChanged(const OpStringC& old_nick, const OpStringC& new_nick);
	virtual void OnWhoisReply(const OpStringC& nick, const OpStringC& user_id, const OpStringC& host,
		const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message,
		const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& rooms);
	virtual void OnInvite(const OpStringC& nick, const ChatInfo& chat_room);
	virtual void OnNickAway(const OpStringC& nick, const OpStringC& away_reason);

	virtual void OnChannelModeChanged(const ChatInfo& chat_room, const OpStringC& nick, uni_char mode, BOOL set,
		const OpStringC& parameter, BOOL is_user_mode, BOOL is_operator_or_better, BOOL is_voice_or_better);

	virtual void OnChannelListReply(const ChatInfo& chat_room, UINT visible_users, const OpStringC& topic);
	virtual void OnChannelListDone();
	virtual void OnChannelUserReply(const ChatInfo& chat_room, const OpStringC& nick, BOOL is_operator, BOOL has_voice, const OpStringC& prefix);
	virtual void OnChannelUserReplyDone(const ChatInfo& chat_room);

	virtual void OnFileSendInitializing(ChatFileTransfer& chat_transfer);
	virtual void OnFileSendBegin(const ChatFileTransfer& chat_transfer, BOOL& transfer_failed);
	virtual void OnFileSendProgress(const ChatFileTransfer& chat_transfer, UINT32 bytes_sent, BOOL &transfer_failed);
	virtual void OnFileSendCompleted(const ChatFileTransfer& chat_transfer);
	virtual void OnFileSendFailed(const ChatFileTransfer& chat_transfer);

	virtual void OnFileReceiveInitializing(ChatFileTransfer& chat_transfer, ChatFileTransferManager::FileReceiveInitListener * listener);
	virtual void OnFileReceiveBegin(const ChatFileTransfer& chat_transfer, BOOL& transfer_failed);
	virtual void OnFileReceiveProgress(const ChatFileTransfer& chat_transfer, unsigned char* received, UINT32 bytes_received, BOOL &transfer_failed);
	virtual void OnFileReceiveCompleted(const ChatFileTransfer& chat_transfer);
	virtual void OnFileReceiveFailed(const ChatFileTransfer& chat_transfer);

	virtual void GetCurrentCharset(OpString8 &charset);
	virtual void GetTransferPortRange(UINT &start_port, UINT &end_port);
	virtual void GetCTCPLimitations(UINT& max_ctcp_requests, UINT& max_ctcp_request_interval);
	virtual time_t GetCurrentTime();
	virtual ProgressInfo& GetProgress() { return m_progress; }

	// HotlistManager::ContactListener.
    virtual void OnHotlistItemAdded(HotlistModelItem* item);
    virtual void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flags = HotlistModel::FLAG_UNKNOWN);
    virtual void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child);
    virtual void OnHotlistItemTrashed(HotlistModelItem* item) {}
	virtual void OnHotlistItemUnTrashed(HotlistModelItem* item) {}
	virtual void OnHotlistItemMovedFrom(HotlistModelItem* item) {}


	BOOL				ReconnectOnDisconnection() const { return FALSE; }
	void				Cleanup(BOOL resumable = FALSE);
	void				ChatStatusAsText(AccountTypes::ChatStatus status, OpString &out);
	INT32				FindSubscribedFolder(const OpStringC& path);
	BOOL				IsConnected();
	OP_STATUS			InterpretAndSend(const OpStringC& raw_command, const ChatInfo& chat_info, const OpStringC& chatter, BOOL notify_owner);
	void				UpdateChatStatus(AccountTypes::ChatStatus new_status);

	OP_STATUS			GetPerformWhenConnected(OpAutoVector<OpString> &commands) const;

	// Static members.
	static const UINT m_presence_interval;
	static const long m_connection_delay = 5000;
	static const unsigned m_max_connect_retries = 20;

	// Members.
    OpAutoPtr<IRCProtocol>					m_protocol;
	OpAutoPtr<IRCCommandInterpreter>		m_command_interpreter;
	OpAutoPtr<OpTimer>						m_presence_timer;
	BOOL									m_should_list_rooms;
	AccountTypes::ChatStatus				m_chat_status;
	OpAutoVector<Room>						m_connecting_rooms;
	OpAutoVector<Room>						m_rooms;
	UINT									m_server_connect_index;
	unsigned								m_connect_count;

	UINT									m_start_dcc_pool;
	UINT									m_end_dcc_pool;
	UINT									m_max_ctcp_requests;
	UINT									m_max_ctcp_request_interval;
	BOOL									m_can_accept_incoming_connections;
	BOOL									m_open_private_chats_in_background;
	OpString								m_perform_when_connected;
};

#endif // IRC_SUPPORT

#endif // IRC_MODULE_H
