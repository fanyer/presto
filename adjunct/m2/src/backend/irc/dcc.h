/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef IRC_DCC_H
#define IRC_DCC_H

#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpSocketAddress.h"

#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/buffer.h"

#include "chat-filetransfer.h"

class GlueFactory;
class OpFile;

class DCCChat;
class DCCFileReceiver;
class DCCFileSender;

//****************************************************************************
//
//	DCCObserver
//
//****************************************************************************

class DCCObserver
{
public:
	virtual ~DCCObserver() {}
	// Receive.
	virtual void OnDCCReceiveBegin(DCCFileReceiver* receiver) = 0;
	virtual void OnDCCDataReceived(DCCFileReceiver* receiver, unsigned char* received, UINT bytes_received) = 0;
	virtual void OnDCCReceiveComplete(DCCFileReceiver* receiver) = 0;
	virtual void OnDCCReceiveFailed(DCCFileReceiver* receiver) = 0;

	// Send.
	virtual void OnDCCSendBegin(DCCFileSender* sender) = 0;
	virtual void OnDCCDataSent(DCCFileSender* sender, UINT bytes_sent) = 0;
	virtual void OnDCCSendComplete(DCCFileSender* sender) = 0;
	virtual void OnDCCSendFailed(DCCFileSender* sender) = 0;

	// Chat.
	virtual void OnDCCChatBegin(DCCChat* chat) = 0;
	virtual void OnDCCChatDataSent(DCCChat* chat, const OpStringC8& data) = 0;
	virtual void OnDCCChatDataReceived(DCCChat* chat, const OpStringC8& data) = 0;
	virtual void OnDCCChatClosed(DCCChat* chat) = 0;
};


//****************************************************************************
//
//	DCCConnection
//
//****************************************************************************

class DCCConnectionListener
{
public:
	virtual ~DCCConnectionListener() {}
	// Virtual methods.
	virtual OP_STATUS DCCConnectionConnected() { return OpStatus::OK; }
	virtual OP_STATUS DCCConnectionDataReady() { return OpStatus::OK; }
	virtual OP_STATUS DCCConnectionDataSent(UINT32 bytes_sent) { return OpStatus::OK; }
	virtual OP_STATUS DCCConnectionClosed() { return OpStatus::OK; }

protected:
	// Construction.
	DCCConnectionListener() { }
};


/*! Class encapsulating the dcc connection. This way the sender and receiver
	classes doesn't have to care about whether they really are a server or a
	client (depending on passive or active dcc).
*/

class DCCConnection
{
public:
	// Desctruction.
	virtual ~DCCConnection() { }

	// Virtual methods.
	virtual OP_STATUS StartTransfer() { OP_ASSERT(0); return OpStatus::ERR; }
	virtual OP_STATUS ReadData(void *buffer, UINT buffer_size, UINT &bytes_read) { OP_ASSERT(0); return OpStatus::ERR; }
	virtual OP_STATUS SendData(void *buffer, UINT buffer_size) { OP_ASSERT(0); return OpStatus::ERR; }

	// Methods.
	BOOL IsActive() const { return m_active; }
	BOOL IsPassive() const { return !m_active; }

protected:
	// Construction.
	DCCConnection(DCCConnectionListener& listener, GlueFactory &glue_factory, BOOL active)
	:	m_listener(listener),
		m_glue_factory(glue_factory),
		m_active(active)
	{
	}

	// Methods.
	DCCConnectionListener& GetListener() { return m_listener; }

	GlueFactory& Glue() { return m_glue_factory; }
	const GlueFactory& Glue() const { return m_glue_factory; }

private:
	// No copy or assignment.
	DCCConnection(const DCCConnection& other);
	DCCConnection& operator=(const DCCConnection& other);

	// Members.
	DCCConnectionListener& m_listener;
	GlueFactory &m_glue_factory;
	BOOL m_active;
};


//****************************************************************************
//
//	DCCBase
//
//****************************************************************************

/*! General base class containing stuff that is more or less common for all
	kinds of DCC (send, chat).
*/

class DCCBase
{
public:
	// Enums.
	enum DCCStatus
	{
		DCC_STATUS_NONE,
		DCC_STATUS_STARTING,
		DCC_STATUS_CONNECTED,
		DCC_STATUS_SENDING,
		DCC_STATUS_RECEIVING
	};

	// Methods.
	DCCStatus CurrentStatus() const { return m_status; }
	const OpStringC& OtherParty() const { return m_other_party; }
	void SetPortNumber(UINT port_number) { m_port_number = port_number; }
	UINT PortNumber() const { return m_port_number; }
	UINT Id() const { return m_id; }

protected:
	// Constructor / destructor.
	DCCBase(DCCObserver& observer);
	virtual ~DCCBase();

	OP_STATUS Init(const OpStringC& other_party, UINT id = 0);

	// Methods.
	DCCObserver &Observer() { return m_observer; }
	const DCCObserver &Observer() const { return m_observer; }

	GlueFactory &Glue() { return *m_glue; }
	GlueFactory const &Glue() const { return *m_glue; }

	void SetStatus(DCCStatus new_status) { m_status = new_status; }

private:
	// Members.
	DCCObserver&	m_observer;
	GlueFactory*	m_glue;

	DCCStatus		m_status;
	OpString		m_other_party;
	UINT			m_port_number;
	UINT			m_id;
};


//****************************************************************************
//
//	DCCFile
//
//****************************************************************************

/*! Base class for sending / receiving files through DCC. */

class DCCFile
:	public Autodeletable,
	public ChatFileTransfer,
	public DCCBase
{

public:
	// Destruction.
	virtual ~DCCFile() { }

	// Methods.
	const OpStringC& FileName() const { return m_filename; }
	BOOL IsActive() const { return m_connection_impl->IsActive(); }
	BOOL IsPassive() const { return m_connection_impl->IsPassive(); }

	// ChatFileTransfer.
	virtual OP_STATUS GetOtherParty(OpString& other_party) const;
	virtual OP_STATUS GetFileName(OpString& name) const;
	virtual OpFileLength GetFileSize() const { return m_file_size; }
	virtual OP_STATUS GetProtocolName(OpString& name) const;

protected:
	// Construction.
	DCCFile(DCCObserver &observer);
	OP_STATUS Init(const OpStringC& other_party, const OpStringC& filename, OpFileLength file_size, UINT id = 0);

	// Methods.
	DCCConnection& ConnectionImpl() { return *m_connection_impl; }
	const DCCConnection& ConnectionImpl() const { return *m_connection_impl; }
	void SetConnectionImpl(DCCConnection* connection_impl) { m_connection_impl = connection_impl; }

private:
	// Members.
	OpAutoPtr<DCCConnection> m_connection_impl;

	OpString	m_filename;
	OpFileLength	m_file_size;
};


//****************************************************************************
//
//	DCCFileReceiver
//
//****************************************************************************

class DCCFileReceiver
:	public DCCFile,
	public DCCConnectionListener
{
public:
	// Constructor / destructor.
	DCCFileReceiver(DCCObserver &observer);
	virtual ~DCCFileReceiver();

	OP_STATUS InitCommon(const OpStringC& sender, const OpStringC& filename, OpFileLength file_size, UINT id = 0);
	OP_STATUS InitClient(const OpStringC& longip, UINT port);
	OP_STATUS InitServer(UINT start_port, UINT end_port);

	// Methods.
	OP_STATUS StartTransfer();
	void SetResumePosition(OpFileLength position) { m_bytes_received = position; }

private:
	// DCCConnectionListener.
	virtual OP_STATUS DCCConnectionConnected();
	virtual OP_STATUS DCCConnectionDataReady();
	virtual OP_STATUS DCCConnectionClosed();

	// Members.
	OpFileLength m_bytes_received;
	unsigned char m_read_buffer[4096];
};


//****************************************************************************
//
//	DCCFileSender
//
//****************************************************************************

class DCCFileSender
:	public DCCFile,
	public DCCConnectionListener
{
public:
	// Construction / destruction.
	DCCFileSender(DCCObserver &observer);
	virtual ~DCCFileSender();

	OP_STATUS InitCommon(const OpStringC& receiver, const OpStringC& filename, UINT id = 0);

	OP_STATUS InitClient(const OpStringC& longip, UINT port);
	OP_STATUS InitServer(UINT start_port, UINT end_port);

	// Methods.
	OP_STATUS StartTransfer();
	virtual OP_STATUS SetResumePosition(OpFileLength position);

private:
	// DCCConnectionListener.
	virtual OP_STATUS DCCConnectionConnected();
	virtual OP_STATUS DCCConnectionDataReady();
	virtual OP_STATUS DCCConnectionDataSent(UINT32 bytes_sent);
	virtual OP_STATUS DCCConnectionClosed();

	// Methods.
	OP_STATUS SendChunk();

	// Members.
	OpAutoPtr<DCCConnection> m_connection_impl;

	OpFile* m_file;
	BOOL m_send_ahead; // Don't wait for ack before sending if this is set.
	OpByteBuffer m_receive_buffer;
	OpFileLength m_bytes_sent;
};


//****************************************************************************
//
//	DCCChat
//
//****************************************************************************

/*! Base class for chatting through DCC. */

class DCCChat
:	public Autodeletable,
	public DCCBase
{
public:
	// Destructor.
	virtual ~DCCChat() { }

	// Methods.
	virtual OP_STATUS SendText(const OpStringC& text, const OpStringC8& encoding);
	virtual OP_STATUS SendText(const OpStringC8& text) = 0;

protected:
	// Constructor.
	DCCChat(DCCObserver& observer);

	OP_STATUS Init(const OpStringC& other_party);
};


//****************************************************************************
//
//	DCCChatClient
//
//****************************************************************************

class DCCChatClient
:	public DCCChat,
	public OpSocketListener
{
public:
	// Constructor / destructor.
	DCCChatClient(DCCObserver &observer);
	virtual ~DCCChatClient();

	OP_STATUS Init(const OpStringC& other_party, const OpStringC& longip,
		UINT port);

	// DCCChat.
	virtual OP_STATUS SendText(const OpStringC8& text);

private:
	// OpSocketListener.
	virtual void OnSocketConnected(OpSocket* socket);
	virtual void OnSocketDataReady(OpSocket* socket);
	virtual void OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent) { }
	virtual void OnSocketClosed(OpSocket* socket);
	virtual void OnSocketConnectionRequest(OpSocket* socket) { }
	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error error);
#ifdef PI_CAP_SOCKET_LISTEN_CLEANUP
	virtual void OnSocketListenError(OpSocket* socket, OpSocket::Error error){}
#endif
	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error error) { }
	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error error) { }
	virtual void OnSocketDataSendProgressUpdate(OpSocket* socket, unsigned int bytes_sent) { }

	// Methods.
	OP_STATUS HandleDataReceived();

	// Members.
	OpSocket *m_socket;
	OpSocketAddress *m_socket_address;

	OpString8 m_send_buffer;
	OpString8 m_receive_buffer;
};


//****************************************************************************
//
//	DCCCollection
//
//****************************************************************************

/*! Collection class for managing currently active DCC sessions. */

class DCCCollection
{
public:
	// Construction / destruction.
	DCCCollection();
	~DCCCollection();

	// Methods.
	OP_STATUS AddActiveFileReceiver(DCCFileReceiver* receiver);
	OP_STATUS AddPassiveFileReceiver(DCCFileReceiver* receiver);

	OP_STATUS AddActiveFileSender(DCCFileSender* sender);
	OP_STATUS AddPassiveFileSender(DCCFileSender* sender);

	OP_STATUS RemoveFileReceiver(DCCFileReceiver* receiver);
	OP_STATUS RemoveFileSender(DCCFileSender* sender);

	OP_STATUS FileReceiver(UINT port_number, UINT id, DCCFileReceiver*& receiver) const;
	OP_STATUS FileReceiverFromPort(UINT port_number, DCCFileReceiver*& receiver) const;
	OP_STATUS FileReceiverFromId(UINT id, DCCFileReceiver*& receiver) const;

	OP_STATUS FileSender(UINT port_number, UINT id, DCCFileSender*& sender) const;
	OP_STATUS FileSenderFromPort(UINT port_number, DCCFileSender*& sender) const;
	OP_STATUS FileSenderFromId(UINT id, DCCFileSender*& sender) const;

private:
	// No copy or assignment.
	DCCCollection(const DCCCollection& other);
	DCCCollection& operator=(const DCCCollection& other);

	// Methods.
	DCCCollection& Collection() const { return (DCCCollection &)(*this); }

	// Members.
	OpINT32HashTable<DCCFileReceiver> m_active_file_receivers;
	OpINT32HashTable<DCCFileReceiver> m_passive_file_receivers;
	OpINT32HashTable<DCCFileSender> m_active_file_senders;
	OpINT32HashTable<DCCFileSender> m_passive_file_senders;
};

#endif
