/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _IRC_PROTOCOL_H_
#define _IRC_PROTOCOL_H_

#include "dcc.h"
#include "irc-message.h"
#include "identd.h"

#include "adjunct/m2/src/backend/compat/crcomm.h"
#include "modules/hardcore/unicode/unicode.h"
#include "modules/util/simset.h"
#include "modules/util/opautoptr.h"
#include "modules/util/OpHashTable.h"
#include "modules/windowcommander/OpTransferManager.h"

#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/util/str/m2tokenizer.h"

class IRCBackend;
class IRCProtocol;
class IRCUser;
class LocalInfo;
class ModeInfo;

class GlueFactory;
class OpFile;

class ChatFileTransfer;
class ChatFileTransferManager;

//****************************************************************************
//
//	IRCProtocolOwner
//
//****************************************************************************

class IRCProtocolOwner
{
public:
    virtual ~IRCProtocolOwner() {}
	// Notifications from IRCProtocol to the owner.
	virtual void OnConnectionLost(BOOL retry) = 0;
	virtual void OnRestartRequested() = 0;
	virtual void OnProtocolReady() = 0;

	virtual void OnInitialPresence(const OpStringC& nick) = 0;
	virtual void OnPresenceUpdate(const OpStringC& nick, const OpStringC& away_reason = OpStringC()) = 0;
	virtual void OnPresenceOffline(const OpStringC& nick) = 0;

	virtual void OnIdentdConnectionEstablished(const OpStringC &connection_to) = 0;
	virtual void OnIdentdIdentConfirmed(const OpStringC8 &request, const OpStringC8 &response) = 0;
	virtual void OnRawMessageReceived(const OpStringC8& message) = 0;
	virtual void OnRawMessageSent(const OpStringC8& message) = 0;

	virtual void OnNicknameInUse(const OpStringC& nick) = 0;
	virtual void OnChannelPasswordRequired(const ChatInfo& chat_info) = 0;

	virtual void OnCTCPPrivateAction(const ChatInfo& chat_info, const OpStringC& action, BOOL self_action) = 0;
	virtual void OnCTCPChannelAction(const ChatInfo& chat_info, const OpStringC& action, const OpStringC& from) = 0;
	virtual void OnDCCSendRequest(const OpStringC& sender, const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id) = 0;

	virtual void OnServerMessage(const OpStringC& message) = 0;
	virtual void OnServerInformation(const OpStringC& information) = 0;
	virtual void OnPrivateMessage(const ChatInfo& chat_info, const OpStringC& message, BOOL self_message) = 0;
	virtual void OnChannelMessage(const ChatInfo& chat_info, const OpStringC& message, const OpStringC& from) = 0;
	virtual void OnJoinedChannel(const ChatInfo& chat_info, const OpStringC &nick) = 0;
	virtual void OnJoinedChannelPassword(const ChatInfo&, const OpStringC &password) = 0;
	virtual void OnLeftChannel(const ChatInfo& chat_info, const OpStringC &nick, const OpStringC& leave_reason) = 0;
	virtual void OnQuit(const OpStringC& nick, const OpStringC& quit_message) = 0;
	virtual void OnKickedFromChannel(const ChatInfo& chat_info, const OpStringC& nick, const OpStringC& kicker, const OpStringC &kick_reason) = 0;
	virtual void OnTopicChanged(const ChatInfo& chat_info, const OpStringC& nick, const OpStringC& topic) = 0;
	virtual void OnNickChanged(const OpStringC& old_nick, const OpStringC& new_nick) = 0;
	virtual void OnWhoisReply(const OpStringC& nick, const OpStringC& user_id, const OpStringC& host,
		const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message,
		const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& rooms) = 0;
	virtual void OnInvite(const OpStringC& nick, const ChatInfo& chat_info) = 0;
	virtual void OnNickAway(const OpStringC& nick, const OpStringC& away_reason) = 0;

	virtual void OnChannelModeChanged(const ChatInfo& chat_info, const OpStringC& nick, uni_char mode, BOOL set,
		const OpStringC& parameter, BOOL is_user_mode, BOOL is_operator_or_better, BOOL is_voice_or_better) = 0;

	virtual void OnChannelListReply(const ChatInfo& chat_info, UINT visible_users, const OpStringC& topic) = 0;
	virtual void OnChannelListDone() = 0;
	virtual void OnChannelUserReply(const ChatInfo& chat_info, const OpStringC& nick, BOOL is_operator, BOOL has_voice, const OpStringC& prefix) = 0;
	virtual void OnChannelUserReplyDone(const ChatInfo& chat_info) = 0;

	virtual void OnFileSendInitializing(ChatFileTransfer& chat_transfer) = 0;
	virtual void OnFileSendBegin(const ChatFileTransfer& chat_transfer, BOOL& transfer_failed) = 0;
	virtual void OnFileSendProgress(const ChatFileTransfer& chat_transfer, UINT32 bytes_sent, BOOL &transfer_failed) = 0;
	virtual void OnFileSendCompleted(const ChatFileTransfer& chat_transfer) = 0;
	virtual void OnFileSendFailed(const ChatFileTransfer& chat_transfer) = 0;

	virtual void OnFileReceiveInitializing(ChatFileTransfer& chat_transfer, ChatFileTransferManager::FileReceiveInitListener * listener) = 0;
	virtual void OnFileReceiveBegin(const ChatFileTransfer& chat_transfer, BOOL& transfer_failed) = 0;
	virtual void OnFileReceiveProgress(const ChatFileTransfer& chat_transfer, unsigned char* received, UINT32 bytes_received, BOOL &transfer_failed) = 0;
	virtual void OnFileReceiveCompleted(const ChatFileTransfer& chat_transfer) = 0;
	virtual void OnFileReceiveFailed(const ChatFileTransfer& chat_transfer) = 0;

	// Queries from the IRCProtocol to the owner.
	virtual void GetCurrentCharset(OpString8& charset) = 0;
	virtual void GetTransferPortRange(UINT& start_port, UINT& end_port) = 0;
	virtual void GetCTCPLimitations(UINT& max_ctcp_requests, UINT& max_ctcp_request_interval) = 0;
	virtual time_t GetCurrentTime() = 0;
	virtual ProgressInfo& GetProgress() = 0;
};


//****************************************************************************
//
//	IRCIncomingHandler
//
//****************************************************************************

class IRCIncomingHandler
:	public OpTimerListener
{
public:
	// Destructor.
	virtual ~IRCIncomingHandler();

	// Methods.
	BOOL IsDone() const { return m_done; }

	void FirstMessageSent();
	void SetRawMessageCount(UINT count) { m_raw_message_count = count; }

	// Overridables.
	virtual BOOL HandleReply(const IRCMessage& reply, UINT numeric_reply) = 0;
	virtual BOOL HandleMessage(const IRCMessage& message, const OpStringC& command) = 0;

#ifdef _DEBUG
	virtual const char* Name() const = 0;
#endif
	virtual BOOL IsReplyHandler() const { return FALSE; }
	virtual BOOL IsMessageHandler() const { return FALSE; }

protected:
	// Constructor.
	IRCIncomingHandler(IRCProtocol& protocol, UINT seconds_timeout = 0);

	// Virtual functions.
	virtual void HandleTimeout() { }

	// Methods.
	IRCProtocol &Protocol() { return m_protocol; }
	const IRCProtocol& Protocol() const { return m_protocol; }

	void CompleteMessageProcessed();
	void HandlerDone() { m_done = TRUE; }

private:
	// No copy or assignment.
	IRCIncomingHandler(const IRCIncomingHandler& other);
	IRCIncomingHandler& operator=(const IRCIncomingHandler& rhs);

	// OpTimerListener.
	virtual void OnTimeOut(OpTimer* timer);

	// Members.
	IRCProtocol &m_protocol;
	OpAutoPtr<OpTimer> m_timer;

	BOOL m_done;
	UINT m_raw_message_count;
	UINT m_timeout_secs;
};

//****************************************************************************
//
//	IRCReplyHandler
//
//****************************************************************************

class IRCReplyHandler
:	public IRCIncomingHandler
{
public:
	// Static methods.
	static BOOL NeedsDummyHandler(const IRCMessage &message);
	static void RepliesNeeded(const IRCMessage &message, OpINT32Set &replies);

protected:
	// Constructor.
	IRCReplyHandler(IRCProtocol &protocol, UINT seconds_timeout = 0);

private:
	// IRCIncomingHandler.
	virtual BOOL IsReplyHandler() const { return TRUE; }
	virtual BOOL HandleMessage(const IRCMessage& message, const OpStringC& command) { return FALSE; }
};


//****************************************************************************
//
//	IRCMessageHandler
//
//****************************************************************************

class IRCMessageHandler
:	public IRCIncomingHandler
{
protected:
	// Constructor.
	IRCMessageHandler(IRCProtocol &protocol, UINT seconds_timeout = 0);

private:
	// IRCIncomingHandler.
	virtual BOOL IsMessageHandler() const { return TRUE; }
	virtual BOOL HandleReply(const IRCMessage& reply, UINT numeric_reply) { return FALSE; }
};


//****************************************************************************
//
//	IRCContacts
//
//****************************************************************************

class IRCContacts
{
public:
	// Construction.
	IRCContacts() { }

	// Internal classes.
	class IRCContact
	{
	public:
		// Construction.
		IRCContact() : m_online(FALSE), m_awaiting_online_reply(FALSE) { }
		OP_STATUS Init(const OpStringC& nick) { return m_nick.Set(nick); }

		// Methods.
		const OpStringC& Nick() const { return m_nick; }
		const OpStringC& AwayMessage() const { return m_away_message; }

		BOOL IsAway() const { return m_away_message.HasContent(); }
		OP_STATUS SetAwayMessage(const OpStringC& away_message) { return m_away_message.Set(away_message); }

		BOOL IsOnline() const { return m_online; }
		BOOL IsOffline() const { return !m_online; }
		void SetOnline(BOOL online) { m_online = online; }

		BOOL IsAwaitingOnlineReply() const { return m_awaiting_online_reply; }
		void SetAwaitingOnlineReply(BOOL waiting) { m_awaiting_online_reply = waiting; }

	private:
		// No copy or assignment.
		IRCContact(const IRCContact& other);
		IRCContact& operator=(const IRCContact& other);

		// Members.
		OpString m_nick;
		OpString m_away_message;

		BOOL m_online;
		BOOL m_awaiting_online_reply;
	};

	// Methods.
	OP_STATUS AddContacts(INT32 id, const OpStringC& nicknames);
	OP_STATUS RemoveContacts(INT32 id);
	OP_STATUS ContactList(INT32 id, OpAutoVector<OpString> &nicknames) const;

	BOOL HasContact(const OpStringC& nick) const;
	OP_STATUS Contact(const OpStringC& nick, IRCContact*& contact) const;

	INT32 ContactCount() const { return m_contacts_by_nick.GetCount(); }

	OP_STATUS PrepareNicknameList(OpVector<OpStringC>& nicknames);
	OP_STATUS OfflineContacts(OpVector<OpStringC>& nicknames);

private:
	// No copy or assignment.
	IRCContacts(const IRCContacts& other);
	IRCContacts& operator=(const IRCContacts& other);

	// Members.
	OpAutoStringHashTable<IRCContact> m_contacts_by_nick;
	OpAutoINT32HashTable<OpVector<IRCContact> > m_contacts_by_id;
};

//****************************************************************************
//
//	IRCMessageSender
//
//****************************************************************************

class IRCMessageSender
:	public OpTimerListener
{
public:
	// Owner.
	class Owner
	{
	public:
        virtual ~Owner() {}
		virtual OP_STATUS SendDataToConnection(char* data, UINT data_size) = 0;
		virtual OP_STATUS OnMessageSent(const OpStringC8& message, OpAutoPtr<IRCIncomingHandler> handler) = 0;

	protected:
		// Construction.
		Owner() { }
	};

	// Construction.
	IRCMessageSender(Owner& owner);

	// Priority type.
	enum Priority
	{
		HIGH_PRIORITY, // Sent immediately, no matter what.
		NORMAL_PRIORITY, // Sent as soon as possible (when we're not flooding).
		LOW_PRIORITY // Sent when no normal messages are waiting.
	};

	// Methods.
	OP_STATUS Send(const IRCMessage& message, const OpString8& raw_message, Priority priority, IRCIncomingHandler* handler, UINT delay_secs);

private:
	// No copy or assignment.
	IRCMessageSender(const IRCMessageSender& other);
	IRCMessageSender& operator=(const IRCMessageSender& other);

	// Internal classes.
	class QueuedIRCMessage
	{
	public:
		// Construction.
		QueuedIRCMessage(int weight, Priority priority, OpAutoPtr<IRCIncomingHandler> handler, UINT delay_secs)
		:	m_weight(weight),
			m_priority(priority),
			m_handler(handler),
			m_timeout(0)
		{
			if (delay_secs > 0)
				m_timeout = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->CurrentTime() + delay_secs;
		}

		OP_STATUS Init(const OpStringC8& raw_message)
		{
			RETURN_IF_ERROR(m_raw_message.Set(raw_message));
			return OpStatus::OK;
		}

		// Methods.
		Priority GetPriority() const { return m_priority; }
		time_t Timeout() const { return m_timeout; }
		UINT Weight() const { return m_weight; }
		const OpStringC8& RawMessage() const { return m_raw_message; }

		IRCIncomingHandler* ReleaseHandler() { return m_handler.release(); }

	private:
		// No copy or assignment.
		QueuedIRCMessage(const QueuedIRCMessage& other);
		QueuedIRCMessage& operator=(const QueuedIRCMessage& other);

		// Members.
		int m_weight;
		Priority m_priority;

		OpString8 m_raw_message;
		OpAutoPtr<IRCIncomingHandler> m_handler;
		time_t m_timeout;
	};

	// OpTimerListener.
	virtual void OnTimeOut(OpTimer* timer);

	// Methods.
	OP_STATUS AddToInternalQueue(QueuedIRCMessage* queued_message, BOOL delay_message);
	int CalculateWeight(const IRCMessage& message, int message_length) const;
	OP_STATUS InternalSend(const OpStringC8& raw_message, int weight, OpAutoPtr<IRCIncomingHandler> handler, int message_length = -1);
	BOOL SendAllowed() const { return m_penalty_count > 0; }

	// Constants.
	enum { MaxPenaltyCount = 10 };

	// Members.
	Owner& m_owner;

	OpAutoVector<QueuedIRCMessage> m_normal_priority_queue;
	OpAutoVector<QueuedIRCMessage> m_low_priority_queue;
	OpAutoVector<QueuedIRCMessage> m_delayed_messages_queue;

	int m_penalty_count;
	OpAutoPtr<OpTimer> m_timer;
};


//****************************************************************************
//
//	IRCProtocol
//
//****************************************************************************

class IRCProtocol
:	public ClientRemoteComm,
	public DCCObserver,
	public IdentdServer::IdentdListener,
	public IRCMessageSender::Owner
{
	// ReplyHandler friends.
	friend class UserhostReplyHandler;
	friend class WhoisReplyHandler;
	friend class ListReplyHandler;
	friend class PresenceIsOnReplyHandler;
	friend class PresenceWhoReplyHandler;
	friend class PresenceWhoisReplyHandler;
	friend class DCCWhoReplyHandler;
	friend class InitialModeReplyHandler;
	friend class BanWhoReplyHandler;
	friend class CTCPRequestReplyHandler;

	// MessageHandler friends.
	friend class DCCResumeMessageHandler;
	friend class PassiveDCCSendMessageHandler;

public:
	enum IrcState
	{
		OFFLINE,
		CONNECTING,
		ONLINE
	};

	IRCProtocol(IRCProtocolOwner &owner);
	virtual		~IRCProtocol();

    OP_STATUS	Init(const OpStringC8& servername, UINT16 port, const OpStringC8& password, const OpStringC& nick, const OpStringC& user_name, const OpStringC& full_name, BOOL use_ssl);
    OP_STATUS	Cancel(const OpStringC& quit_message, BOOL nice = TRUE);

	OP_STATUS	JoinChatRoom(const OpStringC& room, const OpStringC& password);
	OP_STATUS	LeaveChatRoom(const ChatInfo& chat_info);
	OP_STATUS	ListChatRooms();
	OP_STATUS	SendChatMessage(EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& chat_info, const OpStringC& chatter);

	// For use by module.
	OP_STATUS	SendUserMessage(const IRCMessage& message, BOOL notify_owner = TRUE);

	OP_STATUS	SetChatProperty(const ChatInfo& chat_info, const OpStringC& chatter, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value);
	OP_STATUS	SetNick(const OpStringC& nick);
	OP_STATUS	SetAway(const OpStringC &message);

	IrcState	State() const { return m_state; }

	OP_STATUS	DccSend(const OpStringC& nick, const OpStringC& filename, BOOL active_send = TRUE);
	OP_STATUS	AcceptDCCRequest(UINT port_number, UINT id);

	OP_STATUS ServerName(OpString& name) const { return name.Set(m_servername); }
	const OpStringC& Nick() const { return m_nick; }

	OP_STATUS IRCContactAdded(INT32 contact_id, const OpStringC& nicknames);
	OP_STATUS IRCContactChanged(INT32 contact_id, const OpStringC& nicknames);
	OP_STATUS IRCContactRemoved(INT32 contact_id, const OpStringC& nicknames);

protected:
	// ProtocolConnection::Client.
	virtual OP_STATUS ProcessReceivedData();
	virtual void RequestMoreData();
	virtual void OnClose(OP_STATUS rc);
	virtual void OnRestartRequested();

	OP_STATUS ParseResponseLine(const char *line);

private:
	// No copy or assignment.
	IRCProtocol(const IRCProtocol& other);
	IRCProtocol& operator=(const IRCProtocol& rhs);

	// DCCObserver.
	virtual void OnDCCReceiveBegin(DCCFileReceiver* receiver);
	virtual void OnDCCDataReceived(DCCFileReceiver* receiver, unsigned char* received, UINT bytes_received);
	virtual void OnDCCReceiveComplete(DCCFileReceiver* receiver);
	virtual void OnDCCReceiveFailed(DCCFileReceiver* receiver);

	virtual void OnDCCSendBegin(DCCFileSender* sender);
	virtual void OnDCCDataSent(DCCFileSender* sender, UINT bytes_sent);
	virtual void OnDCCSendComplete(DCCFileSender* sender);
	virtual void OnDCCSendFailed(DCCFileSender* sender);

	virtual void OnDCCChatBegin(DCCChat* chat);
	virtual void OnDCCChatDataSent(DCCChat* chat, const OpStringC8& data);
	virtual void OnDCCChatDataReceived(DCCChat* chat, const OpStringC8& data);
	virtual void OnDCCChatClosed(DCCChat* chat);

	// IdentdServer::IdentdListener.
	virtual void OnConnectionEstablished(const OpStringC &connection_to);
	virtual void OnIdentConfirmed(const OpStringC8 &request, const OpStringC8 &response);

	// IRCMessageSender::Owner.
	virtual OP_STATUS SendDataToConnection(char* data, UINT data_size);
	virtual OP_STATUS OnMessageSent(const OpStringC8& message, OpAutoPtr<IRCIncomingHandler> handler);

	// Methods.
	OP_STATUS HandleIncomingMessage(const IRCMessage &message, const OpStringC& command);
	OP_STATUS HandleIncomingReply(const IRCMessage &reply, UINT numeric_reply);

	OP_STATUS CreateDCCAcceptMessage(const DCCFile& file, UINT32 resume_position, BOOL with_id, IRCMessage& message) const;
	OP_STATUS CreateDCCSendMessage(const OpStringC& nick, const OpStringC& filename, const OpStringC& to_userhost, UINT port_number, UINT32 file_size, UINT id, IRCMessage& message) const;

	OP_STATUS SendISONMessages(UINT delay_secs);

public:
	OP_STATUS DCCInitReceiveCheckResume(DCCFileReceiver* receiver, OpFileLength resume_position, BOOL initialization_failed);
private:
	OP_STATUS DCCHandleResumeRequest(const CTCPInformation& ctcp_info);
	OP_STATUS DCCHandleWhoReply(const OpStringC& userhost, UINT port, UINT id, BOOL is_sender);
	OP_STATUS DoReceiveResume(const CTCPInformation& ctcp_info);

	OP_STATUS HandlePresenceISONReply(const OpStringC& nick);
	OP_STATUS HandlePresenceWHOReply(const OpStringC& nick, const OpStringC& status);
	OP_STATUS HandlePresenceWHOISReply(const OpStringC& nick, const OpStringC& away_message);

	OP_STATUS CheckForOfflineContacts();

	void ProcessModeChanges(const IRCMessage &message, BOOL is_initial);
	void ProcessServerSupportMessage(const IRCMessage &message);

	UINT SendIRCMessage(const IRCMessage &message, IRCMessageSender::Priority priority, IRCIncomingHandler* handler = 0, BOOL notify_owner = TRUE, UINT delay_secs = 0);

	BOOL IsChannel(const OpStringC &channel) const;
	void StripChannelPrefix(const OpStringC &channel, OpString &out) const;
	void PrefixChannel(const OpStringC &channel, OpString &out) const;

	void StripNicknamePrefix(const OpStringC& nick, OpString &out) const;
	void NicknamePrefix(const OpStringC& nick, OpString &prefix) const;

	OP_STATUS CreateBanAddress(OpString& ban_address, const OpStringC& user, const OpStringC& hostmask) const;
	OP_STATUS RespondToCTCPRequest(const IRCMessage& message, const OpStringC& command, BOOL& requires_reply, BOOL& handled);

	BOOL ModeIsOpOrBetter(const OpStringC& mode) const;
	BOOL ModeIsVoiceOrBetter(const OpStringC& mode) const;
	BOOL PrefixIsOpOrBetter(const OpStringC& prefix) const;
	BOOL PrefixIsVoiceOrBetter(const OpStringC& prefix) const;

	// Members.
	IRCProtocolOwner&					m_owner;

    OpString8							m_servername;
    UINT16								m_port;
	OpString							m_password;
	OpString							m_nick;
	OpString							m_user_name;
	OpString							m_full_name;

	IrcState							m_state;

	OpAutoPtr<ModeInfo>					m_mode_info;
	OpAutoPtr<LocalInfo>				m_local_info;

	IRCMessageSender					m_message_sender;
	OpAutoVector<IRCIncomingHandler>	m_message_handlers;
	OpAutoVector<IRCIncomingHandler>	m_reply_handlers;

	OpAutoPtr<IdentdServer>				m_identd_server;
	DCCCollection						m_dcc_sessions;

	IRCContacts							m_contacts;

	OpString							m_allowed_channel_prefixes;
	OpString							m_allowed_user_mode_changes;
	OpString							m_allowed_user_prefixes;

	AccountTypes::ChatStatus			m_chat_status;
	OpString8							m_remaining_buffer;

	BOOL								m_is_connected;
	BOOL								m_use_extended_list_command;
};

#endif // _IRC_PROTOCOL_H_
