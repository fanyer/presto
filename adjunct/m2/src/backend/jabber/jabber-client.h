/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef JABBER_CLIENT_H
#define JABBER_CLIENT_H

#include "adjunct/m2/src/backend/compat/crcomm.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/util/xmlparser_tracker.h"

class JabberBackend;

// ***************************************************************************
//
//	JabberClient
//
// ***************************************************************************

/**
 * This class handles the communication between Opera and the Jabber-server.
 */

class JabberClient
:	public ClientRemoteComm,
	public XMLParserTracker
{
public:
	// Construction / destruction.
	JabberClient(JabberBackend *jabber_backend);
	~JabberClient();

	OP_STATUS Init();

	// Methods.
	OP_STATUS GetProgress(Account::ProgressInfo& progress);
	OP_STATUS Connect();
	OP_STATUS Disconnect();

	/*
	 * @see JabberBackend::SubscribeToPresence
	 */
	OP_STATUS  SubscribeToPresence(const OpStringC& jabber_id, BOOL subscribe);

	/*
	 * @see JabberBackend::AllowPresenceSubscription
	 */
	OP_STATUS  AllowPresenceSubscription(const OpStringC& jabber_id, BOOL allow);

	/**
	 * Sends the users current presence information to the Jabber server.
	 * @param presence The current Jabber presence.
	 * @param status_message The message accompanying the Jabber presence.
	 */
	OP_STATUS  AnnouncePresence(const OpStringC8& presence, const OpStringC8& status_message);

	/**
	 * Sends a Jabber chat-message. Also encodes it into UTF8.
	 * @param message The message to send.
	 * @param chatter The name of the recipient.
	 * @param jabber_id The JID of the recipient.
	 */
	OP_STATUS  SendMessage(const OpStringC &message, const OpStringC &chatter, OpStringC jabber_id);

private:
	// XMLParserTracker.
	virtual OP_STATUS OnStartElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes);
	virtual OP_STATUS OnEndElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name);
	virtual OP_STATUS OnCharacterData(const OpStringC& data);

	// ProtocolConnection::Client.
	void RequestMoreData();
	OP_STATUS ProcessReceivedData();
	void OnClose(OP_STATUS rc);

	// Methods.
	/**
	 * Reserves a suitable amount of memory for m_reply_buf.
	 */
	OP_STATUS CreateReplyBuf();

	/**
	 * Sends data to the TCP-socket.
	 * @param buffer Contains the data to send. Everything sent should be
	 * well formed XML, encoded in UTF8.
	 */

	OP_STATUS SendToConnection(const OpStringC8& buffer);
	/**
	 * A helper function for Escape.
	 * Replaces all occurences of replace_what with replace_with in
	 * replace_in. Works even if replace_what is a subset of replace_with.
	 */

	OP_STATUS ReplaceAll(OpString8 &replace_in, const OpStringC8& replace_what, const OpStringC8& replace_with);
	/**
	 * Escapes the characters < > & ' and " so they don't mess up the
	 * XML-stream. Returns the string with escaped characters, or empties
	 * the string if memory ran out.
	 * @param to_escape The string to escape.
	 * @return A char-pointer to the data of to_escape.
	 */

	const char *Escape(OpString8 &to_escape);
	/**
	 * Updates the internal connection-status (m_progress) and signals
	 * to Opera if the status was changed.
	 * @param status The status to change to.
	 */

	void SetStatus(Account::AccountStatus status);
	/*
	 * Logs in to the server specified in the Jabber-account with the user
	 * name and password also specified there.
	 */

	OP_STATUS Authenticate();
	/*
	 * Updates the contact's icon so that it corresponds to the received
	 * presence information. Uses m_incoming_presence.
	 */
	void OnPresenceReceived();

	// Internal classes.
	class JabberID
	{
	public:
		// Construction.
		JabberID() { }
		OP_STATUS Init(const OpStringC& jabber_id);

		// Methods.
		const OpStringC &GetSimpleJID() const { return m_simple_jid; }
		const OpStringC &GetFullJID() const { return m_full_jid; }
		const OpStringC &GetUser() const { return m_user; }
		const OpStringC &GetDomain() const { return m_domain; }
		const OpStringC &GetResource() const { return m_resource; }

	private:
		// No copy or assignment.
		JabberID(const JabberID& other);
		JabberID& operator=(const JabberID& other);

		// Members.
		OpString m_user;
		OpString m_domain;
		OpString m_resource;
		OpString m_full_jid;
		OpString m_simple_jid;
	};

	class JabberMessage
	{
	public:
		// Construction.
		JabberMessage()
		:	m_is_awaiting_body(false),
			m_has_body(false),
			m_has_received_error(false),
			m_is_receiving(false)
		{
		}

		// Methods.
		OP_STATUS SetSender(const OpStringC& sender) { return m_sender.Set(sender); }
		const OpStringC& GetSender() const { return m_sender; }

		OP_STATUS AppendBody(const OpStringC& data) { return m_body.Append(data); }
		const OpStringC& GetBody() const { return m_body; }

		void SetIsReceiving(bool receive) { m_is_receiving = receive; }
		bool IsReceiving() const { return m_is_receiving; }

		void SetIsAwaitingBody(bool await) { m_is_awaiting_body = await; await ? m_has_body = false : m_has_body = true; }
		bool IsAwaitingBody() const { return m_is_awaiting_body; }
		bool HasBody() const { return m_has_body; }

		void SetHasReceivedError(bool has_received_error) { m_has_received_error = has_received_error; }
		bool HasReceivedError() const { return m_has_received_error; }

		void Reset() { m_sender.Empty(); m_body.Empty(); m_has_body = false; m_has_received_error = false; }

	private:
		// No copy or assignment.
		JabberMessage(const JabberMessage& other);
		JabberMessage& operator=(const JabberMessage& other);

		// Members.
		OpString m_sender;
		OpString m_body;
		bool m_is_awaiting_body;
		bool m_has_body;
		bool m_has_received_error;
		bool m_is_receiving;
	};

	class JabberPresence
	{
	public:
		// Construction.
		JabberPresence()
		:	m_is_awaiting_show(false),
			m_has_show(false),
			m_is_receiving(false)
		{
		}

		// Methods.
		OP_STATUS SetSender(const OpStringC& sender) { return m_sender.Set(sender); }
		const OpStringC& GetSender() const { return m_sender; }
		OP_STATUS SetShow(const OpStringC& show) { m_has_show = true; return m_show.Set(show); }
		const OpStringC& GetShow() const { return m_show; }

		void SetIsReceiving(bool receive) { m_is_receiving = receive; }
		bool IsReceiving() const { return m_is_receiving; }

		void SetIsAwaitingShow(bool await) { m_is_awaiting_show = await; }
		bool IsAwaitingShow() const { return m_is_awaiting_show; }
		bool HasShow() const { return m_has_show; }

		void Reset() { m_sender.Empty(); m_show.Empty(); m_has_show = false; }

	private:
		// No copy or assignment.
		JabberPresence(const JabberPresence& other);
		JabberPresence& operator=(const JabberPresence& other);

		// Members.
		OpString m_sender;
		OpString m_show;
		bool m_is_awaiting_show;
		bool m_has_show;
		bool m_is_receiving;
	};

	// Members.
	JabberBackend* m_jabber_backend;
	Account::ProgressInfo m_progress;

	JabberMessage m_incoming_message;
	JabberPresence m_incoming_presence;

	OpString8 m_reply_buf;
	bool m_has_incoming;
};

#endif // JABBER_CLIENT_H
