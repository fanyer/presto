/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef JABBER_BACKEND_H
#define JABBER_BACKEND_H

#include "adjunct/m2/src/backend/ProtocolBackend.h"
#include "jabber-client.h"

// ***************************************************************************
//
//	JabberBackend
//
// ***************************************************************************

/**
 * This is Opera's interface to the Jabber client.
 * The functions of this class are called from the Account class.
 */
class JabberBackend
:	public ProtocolBackend
{
public:
	// Construction / destruction.
	JabberBackend();
	~JabberBackend();

	// Methods.
	/**
	 * Turns the Opera chat status into a Jabber presence and a Jabber status message.
	 * @param status One of the Opera chat statuses.
	 * @param presence The corresponding Jabber presence.
	 * @param status_message
	 */
	void GetPresenceAndStatusMessage(Account::ChatStatus status, OpString8& presence, OpString8& status_message);

	// ProtocolBackend.
	virtual OP_STATUS Init(Account* account);
	virtual Account::AccountType GetType() const { return Account::JABBER; }
	virtual const char *GetName() const { return "JABBER"; }
	virtual OP_STATUS SettingsChanged(BOOL startup) { return OpStatus::OK; }
	virtual UINT32 GetAuthenticationSupported() { return 1 << Account::NONE; }
	virtual OP_STATUS PrepareToDie() { return OpStatus::OK; }
	virtual OP_STATUS Connect();
	virtual OP_STATUS Disconnect();
	virtual OP_STATUS GetProgress(Account::ProgressInfo& progress) const;
	virtual OP_STATUS SendChatMessage(MessageEngine::ChatMessageType type, const OpStringC& message, const ChatInfo& room, const OpStringC& chatter);
	virtual BOOL IsChatStatusAvailable(Account::ChatStatus chat_status);
	virtual OP_STATUS SetChatStatus(Account::ChatStatus chat_status);
	virtual Account::ChatStatus GetChatStatus(BOOL& is_connecting) { return m_current_chat_status; }
	virtual OP_STATUS FetchHeaders(const OpStringC8& internet_location, message_index_t index=0) { return OpStatus::ERR; }
	virtual OP_STATUS FetchHeaders(BOOL enable_signalling) { return OpStatus::ERR; }
	virtual OP_STATUS FetchMessage(const OpStringC8& internet_location, message_index_t index=0) { return OpStatus::ERR; }
	virtual OP_STATUS FetchMessage(message_index_t idx, BOOL user_request) { return OpStatus::ERR; }
	virtual OP_STATUS FetchMessages(BOOL enable_signalling) { return OpStatus::ERR; }
	virtual OP_STATUS SelectFolder(UINT32 index_id, const OpStringC16& folder) { return OpStatus::ERR; }
	virtual OP_STATUS StopFetchingMessages() { return OpStatus::ERR; }
	virtual OP_STATUS DeleteMessage(message_gid_t message_id) { return OpStatus::ERR; }
	virtual OP_STATUS StopSendingMessage() { return OpStatus::ERR; }
	virtual OP_STATUS JoinChatRoom(const OpStringC& room, const OpStringC& password) { return OpStatus::ERR; }
	virtual OP_STATUS LeaveChatRoom(const ChatInfo& room) { return OpStatus::ERR; }
	virtual OP_STATUS ChangeNick(OpString& new_nick) { return OpStatus::ERR; }
	virtual OP_STATUS FetchNewGroups() { return OpStatus::ERR; }
	virtual OP_STATUS ServerCleanUp() { return OpStatus::ERR; }
	virtual OP_STATUS InsertExternalMessage(Message& message) { return OpStatus::ERR; }

	// Jabber-specific, added to MessageBackend for this class
	/**
	 * Requests or stops a subscription to the presence of a Jabber-user.
	 * @param jabber_id The JID of the user to subscribe or not subscribe to.
	 * @param subscribe Whether or not to subscribe.
	 */
	OP_STATUS SubscribeToPresence(const OpStringC& jabber_id, BOOL subscribe);
	/**
	 * Allows or denies a Jabber user to subscribe to ones own presence.
	 * @param jabber_id The JID that is allowed or denied subscription.
	 * @param allow Whether to allow or deny jabber_id subscription.
	 */
	OP_STATUS AllowPresenceSubscription(const OpStringC& jabber_id, BOOL allow);

private:
	// Members.
	JabberClient* m_client;
	Account::ChatStatus m_current_chat_status;
};

#endif // JABBER_BACKEND_H
