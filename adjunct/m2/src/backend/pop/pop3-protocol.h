/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef POP3_PROTOCOL_H
#define POP3_PROTOCOL_H

#include "adjunct/m2/src/backend/compat/crcomm.h"

class PopBackend;
class OfflineLog;
class GenericIncomingParser;
class PopCommand;
template<class T> class StreamBuffer;

/**
 * @brief POP3 communication class
 *
 * Handles all sending and receiving of POP3 commands
 */
class POP3 : public ClientRemoteComm
{
public:
	enum POPState
	{
		DISCONNECTED,
		CONNECTING,
		CONNECTED,
		SECURE,
		AUTHENTICATED
	};

	enum POPCapability
	{
		CAPA_SASL		= 1 << 0,
		CAPA_STARTTLS	= 1 << 1,
		CAPA_TOP		= 1 << 2,
		CAPA_UIDL		= 1 << 3,
		CAPA_USER		= 1 << 4,
		CAPA_CRAMMD5	= 1 << 5,
		CAPA_PLAIN		= 1 << 6
	};

	/** Constructor
	  */
	POP3(PopBackend& backend);

	/** Destructor
	  */
	~POP3();

	/** Initialize the backend. Call this and check result before calling other functions
	  */
	OP_STATUS Init();

	/** Connect to the server and execute pending tasks
	  */
	OP_STATUS Connect();

	/** Cancel any open connections and/or tasks
	  */
	OP_STATUS Cancel(BOOL notify = TRUE);

	/** Fetch a message by UIDL
	  * @param uidl UIDL of message to fetch
	  */
	OP_STATUS FetchMessage(const OpStringC8& uidl);

	/** Remove a message by UIDL
	  * @param uidl UIDL of message to remove
	  */
	OP_STATUS RemoveMessage(const OpStringC8& uidl);

	/** Write pending tasks to offline log
	  * @param offline_log Log to write to
	  */
	OP_STATUS WriteToOfflineLog(OfflineLog& offline_log);

	/** Set the state of the connection
	  */
	void	   SetState(POPState state) { m_state = state; }

	/** Greeting has been received
	  */
	OP_STATUS  OnGreeting();

	/** Set the timestamp of the connection
	  */
	OP_STATUS  OnTimestamp(const OpStringC8& timestamp) { return m_timestamp.Set(timestamp); }

	/** Current command was successful
	  * @param success_msg Message delivered with success
	  */
	OP_STATUS  OnSuccess(const OpStringC8& success_msg);

	/** Current command was not successful
	  * @param error_msg Message delivered with error
	  */
	OP_STATUS  OnError(const OpStringC8& error_msg);

	/** Current multi-line command was completed
	  */
	OP_STATUS  OnMultiLineComplete();

	/** Received a continuation request
	  * @param text Text in request
	  */
	OP_STATUS  OnContinueReq(const OpStringC8& text);

	/** A capability was received
	  */
	void	   AddCapability(POPCapability capability) { m_capabilities |= capability; }

	/** Reset capability knowledge
	  */
	void	   ResetCapabilities() { m_capabilities = 0; }

	/** Received a UIDL
	  */
	OP_STATUS  OnUIDL(int server_id, const OpStringC8& uidl);

	/** Received a LIST response
	  */
	OP_STATUS  OnList(int server_id, int size);

	/** Got a parse error
	  */
	void	   OnParseError(const OpStringC8& error);

	PopBackend& GetBackend() { return m_backend; }

	/** Upgrade connection to TLS
	  */
	OP_STATUS  UpgradeToTLS();

	/** Remove the current command and send the next command
	  */
	OP_STATUS  ProceedToNextCommand();

	/** Get the capabilities of this connection
	  */
	int		   GetCapabilities() const { return m_capabilities; }

	/** Add a command to the end of the command queue\
	  * @param command Command to add
	  */
	OP_STATUS AddCommand(PopCommand* command);

	/** Get the buffer used by the parser
	  */
	StreamBuffer<char>* GetParseBuffer();

	/** Get the state of the connection
	  */
	POPState GetState() const { return m_state; }

	/** Whether a message should be removed
	  * @param uidl UIDL of message
	  */
	BOOL ShouldRemoveUIDL(const OpStringC8& uidl) { return m_uidl_to_remove.Contains(uidl.CStr()); }

	/** Whether a message should be fetched
	  * @param uidl UIDL of message
	  */
	BOOL ShouldFetchUIDL(const OpStringC8& uidl) { return m_uidl_to_fetch.Contains(uidl.CStr()); }

	/** Get the timestamp associated with this connection
	  */
	const OpStringC8& GetTimestamp() const { return m_timestamp; }

	/** UIDL has been handled (message fetched or removed)
	  * @param uidl UIDL that has been handled
	  */
	OP_STATUS OnUIDLHandled(const OpStringC8& uidl);

	/** Send the next command in the queue
	  */
	OP_STATUS SendNextCommand();

protected:
	// From ClientRemoteComm
	void RequestMoreData() {}

	/** Called when data is received over the connection
	  */
	OP_STATUS ProcessReceivedData();

	/** Takes care of the messages MSG_COMM_LOADING_FINISHED and
	  * MSG_COMM_LOADING_FAILED. Is called from the comm system and
	  * also from a function in this class. MSG_COMM_LOADING_FINISHED rc == OpStatus::OK
	  * MSG_COMM_LOADING_FAILED rc == OpStatus::ERR;
	  */
	void OnClose(OP_STATUS rc);

	/** Connection requests restart
	  */
	void OnRestartRequested();

private:
	OP_STATUS UpdateProgress();

	Head		m_command_queue;						///< Commands to execute on server
	POPState	m_state;								///< State of the connection
	int			m_capabilities;							///< Capabilities of this connection
	OpString8	m_timestamp;							///< Timestamp from server for use in APOP

	OpAutoString8HashTable<OpString8> m_uidl_to_fetch;	///< Messages that should be fetched
	OpAutoString8HashTable<OpString8> m_uidl_to_remove;	///< Messages that should be removed

	PopBackend&		m_backend;

	GenericIncomingParser* m_parser;

	static const int IdleTimeout = 120;
};

#endif // POP3_PROTOCOL_H
