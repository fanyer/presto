/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef IRC_SUPPORT

#include "adjunct/m2/src/backend/irc/dcc.h"
#include "modules/hardcore/mh/constant.h"
#include "modules/util/filefun.h"
#include "adjunct/m2/src/util/misc.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/util/socketserver.h"

#include "modules/url/url_man.h"

OpString* GetSystemIp(OpString* pIp);

//********************************************************************
//
//	DCCBase
//
//********************************************************************

DCCBase::DCCBase(DCCObserver& observer)
:	m_observer(observer),
	m_glue(0),
	m_status(DCC_STATUS_NONE),
	m_port_number(0),
	m_id(0)
{
	// Init glue pointer.
	m_glue = MessageEngine::GetInstance()->GetGlueFactory();
	OP_ASSERT(m_glue != 0);
}


DCCBase::~DCCBase()
{
}


OP_STATUS DCCBase::Init(const OpStringC& other_party, UINT id)
{
	RETURN_IF_ERROR(m_other_party.Set(other_party));

	if (id != 0)
		m_id = id;
	else
		m_id = (UINTPTR)this; // m_id should probably be a UINTPTR if you want to use it like this !

	return OpStatus::OK;
}


//********************************************************************
//
//	DCCFile
//
//********************************************************************

DCCFile::DCCFile(DCCObserver &observer)
:	Autodeletable(),
	DCCBase(observer),
	m_file_size(0)
{
}


OP_STATUS DCCFile::Init(const OpStringC& other_party,
	const OpStringC& filename, OpFileLength file_size, UINT id)
{
	RETURN_IF_ERROR(DCCBase::Init(other_party, id));

	if (filename.IsEmpty())
		return OpStatus::ERR;

	RETURN_IF_ERROR(m_filename.Set(filename));
	m_file_size = file_size;

	SetStatus(DCC_STATUS_STARTING);
	return OpStatus::OK;
}


OP_STATUS DCCFile::GetOtherParty(OpString& other_party) const
{
	RETURN_IF_ERROR(other_party.Set(DCCBase::OtherParty()));
	return OpStatus::OK;
}


OP_STATUS DCCFile::GetFileName(OpString& name) const
{
	RETURN_IF_ERROR(name.Set(m_filename));
	return OpStatus::OK;
}


OP_STATUS DCCFile::GetProtocolName(OpString& name) const
{
	RETURN_IF_ERROR(name.Set("DCC"));
	return OpStatus::OK;
}


//********************************************************************
//
//	DCCClientConnection
//
//********************************************************************

class DCCClientConnection
:	public DCCConnection,
	public OpSocketListener
{
public:
	// Construction / destruction.
	DCCClientConnection(DCCConnectionListener& listener, GlueFactory& glue_factory, BOOL active);
	virtual ~DCCClientConnection();

	OP_STATUS Init(const OpStringC& longip, UINT port);

private:
	// DCCConnection.
	virtual OP_STATUS StartTransfer();
	virtual OP_STATUS ReadData(void *buffer, UINT buffer_size, UINT &bytes_read);
	virtual OP_STATUS SendData(void *buffer, UINT buffer_size);

	// OpSocketListener.
	virtual void OnSocketConnected(OpSocket* socket);
	virtual void OnSocketDataReady(OpSocket* socket);
	virtual void OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent);
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

	// Members.
	OpSocket *m_socket;
	OpSocketAddress *m_socket_address;

	OpByteBuffer m_send_buffer;
};


DCCClientConnection::DCCClientConnection(DCCConnectionListener& listener,
	GlueFactory& glue_factory, BOOL active)
:	DCCConnection(listener, glue_factory, active),
	m_socket(0),
	m_socket_address(0)
{
}


DCCClientConnection::~DCCClientConnection()
{
	if (m_socket != 0)
		Glue().DestroySocket(m_socket);

	if (m_socket_address != 0)
		Glue().DestroySocketAddress(m_socket_address);
}


OP_STATUS DCCClientConnection::Init(const OpStringC& longip, UINT port)
{
	if (longip.IsEmpty())
		return OpStatus::ERR;

	// Create a socket address to connect to.
	m_socket_address = Glue().CreateSocketAddress();
	if (m_socket_address == 0)
		return OpStatus::ERR_NO_MEMORY;

	m_socket_address->FromString(longip.CStr());
	if (!m_socket_address->IsValid())
		return OpStatus::ERR;

	m_socket_address->SetPort(port);

	// Create a socket.
	m_socket = Glue().CreateSocket(*this);
	if (m_socket == 0)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}


OP_STATUS DCCClientConnection::StartTransfer()
{
	OP_ASSERT(m_socket_address != 0);
	if (m_socket_address == 0)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_socket->Connect(m_socket_address));

	Glue().DestroySocketAddress(m_socket_address);
	m_socket_address = 0;

	return OpStatus::OK;
}


OP_STATUS DCCClientConnection::ReadData(void *buffer, UINT buffer_size,
	UINT &bytes_read)
{
	if (m_socket == 0)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_socket->Recv(buffer, buffer_size, &bytes_read));

	return OpStatus::OK;
}


OP_STATUS DCCClientConnection::SendData(void *buffer, UINT buffer_size)
{
	if (m_socket == 0)
		return OpStatus::ERR_NULL_POINTER;

	const UINT old_send_buffer_size = m_send_buffer.DataSize();
	RETURN_IF_ERROR(m_send_buffer.Append((unsigned char *)(buffer), buffer_size));

	// Only send the data we have just appended; the rest of the data is
	// allready sent and will be removed when OnSocketDataSent is called.
	RETURN_IF_ERROR(m_socket->Send(m_send_buffer.Buffer() + old_send_buffer_size, buffer_size));
	return OpStatus::OK;
}


void DCCClientConnection::OnSocketConnected(OpSocket* socket)
{
	GetListener().DCCConnectionConnected();
}


void DCCClientConnection::OnSocketDataReady(OpSocket* socket)
{
	GetListener().DCCConnectionDataReady();
}

void DCCClientConnection::OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent)
{
	OP_ASSERT(m_send_buffer.DataSize() >= bytes_sent);
	m_send_buffer.Remove(bytes_sent);

	GetListener().DCCConnectionDataSent(bytes_sent);
}


void DCCClientConnection::OnSocketClosed(OpSocket* socket)
{
	GetListener().DCCConnectionClosed();
}


void DCCClientConnection::OnSocketConnectError(OpSocket* socket, OpSocket::Error socket_error)
{
	GetListener().DCCConnectionClosed();
}


void DCCClientConnection::OnSocketReceiveError(OpSocket* socket, OpSocket::Error socket_error)
{
	if (socket_error != OpSocket::SOCKET_BLOCKING)
	{
		GetListener().DCCConnectionClosed();
	}
}


//********************************************************************
//
//	DCCServerConnection
//
//********************************************************************

class DCCServerConnection
:	public DCCConnection,
	public SocketServer::SocketServerListener
{
public:
	// Construction / destruction.
	DCCServerConnection(DCCConnectionListener& listener, GlueFactory& glue_factory, BOOL active);
	virtual ~DCCServerConnection();

	OP_STATUS Init(UINT start_port, UINT end_port, UINT& port_used);

private:
	// DCCConnection.
	virtual OP_STATUS StartTransfer() { return OpStatus::OK; }
	virtual OP_STATUS ReadData(void *buffer, UINT buffer_size, UINT &bytes_read);
	virtual OP_STATUS SendData(void *buffer, UINT buffer_size);

	// SocketServer::SocketServerListener.
	virtual void OnSocketConnected(const OpStringC &connected_to) { GetListener().DCCConnectionConnected(); }
	virtual void OnSocketClosed() { GetListener().DCCConnectionClosed(); }
	virtual void OnSocketDataAvailable() { GetListener().DCCConnectionDataReady(); }
	virtual void OnSocketDataSent(UINT bytes_sent) { GetListener().DCCConnectionDataSent(bytes_sent); }
	virtual void OnSocketError() { GetListener().DCCConnectionClosed(); }

	// Members.
	OpAutoPtr<SocketServer> m_socket_server;
};


DCCServerConnection::DCCServerConnection(DCCConnectionListener& listener,
	GlueFactory& glue_factory, BOOL active)
:	DCCConnection(listener, glue_factory, active)
{
}


DCCServerConnection::~DCCServerConnection()
{
}


OP_STATUS DCCServerConnection::Init(UINT start_port, UINT end_port,
	UINT& port_used)
{
	SocketServer *sock_serv = OP_NEW(SocketServer, (*this));
	m_socket_server = sock_serv;
	if (!sock_serv)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(sock_serv->Init(start_port, end_port, port_used, 60));
	return OpStatus::OK;
}


OP_STATUS DCCServerConnection::ReadData(void *buffer, UINT buffer_size,
	UINT &bytes_read)
{
	RETURN_IF_ERROR(m_socket_server->Read(buffer, buffer_size, bytes_read));
	return OpStatus::OK;
}


OP_STATUS DCCServerConnection::SendData(void *buffer, UINT buffer_size)
{
	RETURN_IF_ERROR(m_socket_server->Send(buffer, buffer_size));
	return OpStatus::OK;
}


//********************************************************************
//
//	DCCFileReceiver
//
//********************************************************************

DCCFileReceiver::DCCFileReceiver(DCCObserver &observer)
:	DCCFile(observer),
	m_bytes_received(0)
{
}


DCCFileReceiver::~DCCFileReceiver()
{
}


OP_STATUS DCCFileReceiver::InitCommon(const OpStringC& sender,
	const OpStringC& filename, OpFileLength file_size, UINT id)
{
	RETURN_IF_ERROR(DCCFile::Init(sender, filename, file_size, id));
	return OpStatus::OK;
}


OP_STATUS DCCFileReceiver::InitClient(const OpStringC& longip, UINT port)
{
	DCCClientConnection *client_conn = OP_NEW(DCCClientConnection, (*this, Glue(), FALSE));
	if (!client_conn)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<DCCClientConnection> client_connection(client_conn);

	RETURN_IF_ERROR(client_conn->Init(longip, port));
	SetConnectionImpl(client_connection.release());

	SetPortNumber(port);
	return OpStatus::OK;
}


OP_STATUS DCCFileReceiver::InitServer(UINT start_port, UINT end_port)
{
	DCCServerConnection *server_conn = OP_NEW(DCCServerConnection, (*this, Glue(), TRUE));
	if (!server_conn)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<DCCServerConnection> server_connection(server_conn);

	UINT port_used = 0;
	RETURN_IF_ERROR(server_conn->Init(start_port, end_port, port_used));
	SetConnectionImpl(server_connection.release());

	SetPortNumber(port_used);
	return OpStatus::OK;
}


OP_STATUS DCCFileReceiver::StartTransfer()
{
	RETURN_IF_ERROR(ConnectionImpl().StartTransfer());
	return OpStatus::OK;
}


OP_STATUS DCCFileReceiver::DCCConnectionConnected()
{
	SetStatus(DCC_STATUS_RECEIVING);
	Observer().OnDCCReceiveBegin(this);

	return OpStatus::OK;
}


OP_STATUS DCCFileReceiver::DCCConnectionDataReady()
{
	unsigned int bytes_received = 0;
	OpStatus::Ignore(ConnectionImpl().ReadData(m_read_buffer, 4096, bytes_received));

	while (int(bytes_received) > 0)
	{
		m_bytes_received += bytes_received;

		// Notify observer.
		Observer().OnDCCDataReceived(this, m_read_buffer, bytes_received);

		// Send acknowledgement.
		const UINT32 ack = op_htonl((UINT32)m_bytes_received);
		OpStatus::Ignore(ConnectionImpl().SendData((void *)&ack, 4));

		// Read more data, if any.
		bytes_received = 0;
		OpStatus::Ignore(ConnectionImpl().ReadData(m_read_buffer, 4096, bytes_received));
	}

	return OpStatus::OK;
}


OP_STATUS DCCFileReceiver::DCCConnectionClosed()
{
	if (m_bytes_received == GetFileSize())
		Observer().OnDCCReceiveComplete(this);
	else
		Observer().OnDCCReceiveFailed(this);

	return OpStatus::OK;
}


//********************************************************************
//
//	DCCFileSender
//
//********************************************************************

DCCFileSender::DCCFileSender(DCCObserver &observer)
:	DCCFile(observer),
	m_file(0),
	m_send_ahead(TRUE),
	m_bytes_sent(0)
{
}


DCCFileSender::~DCCFileSender()
{
	if (m_file != 0)
		Glue().DeleteOpFile(m_file);
}


OP_STATUS DCCFileSender::InitCommon(const OpStringC& receiver,
	const OpStringC& filename, UINT id)
{
	// Open the file we should send.
	m_file = Glue().CreateOpFile(filename);
	if (m_file == 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(m_file->Open(OPFILE_READ|OPFILE_SHAREDENYWRITE));

	OpFileLength file_size;
	RETURN_IF_ERROR(m_file->GetFileLength(file_size));

	// Initialize the base class.
	RETURN_IF_ERROR(DCCFile::Init(receiver, filename, file_size, id));

	return OpStatus::OK;
}


OP_STATUS DCCFileSender::InitClient(const OpStringC& longip, UINT port)
{
	// Initialize the connection.
	DCCClientConnection *client_conn = OP_NEW(DCCClientConnection, (*this, Glue(), FALSE));
	if (!client_conn)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<DCCClientConnection> client_connection(client_conn);

	RETURN_IF_ERROR(client_conn->Init(longip, port));
	m_connection_impl = client_connection.release();

	SetPortNumber(port);
	return OpStatus::OK;
}


OP_STATUS DCCFileSender::InitServer(UINT start_port, UINT end_port)
{
	// Initialize the connection.
	DCCServerConnection *serv_conn = OP_NEW(DCCServerConnection, (*this, Glue(), TRUE));
	if (!serv_conn)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<DCCServerConnection> server_connection(serv_conn);

	UINT port_used = 0;
	RETURN_IF_ERROR(serv_conn->Init(start_port, end_port, port_used));

	m_connection_impl = server_connection.release();
	SetPortNumber(port_used);

	return OpStatus::OK;
}


OP_STATUS DCCFileSender::StartTransfer()
{
	RETURN_IF_ERROR(m_connection_impl->StartTransfer());
	return OpStatus::OK;
}


OP_STATUS DCCFileSender::SetResumePosition(OpFileLength position)
{
	if (!m_file ||
		CurrentStatus() != DCC_STATUS_STARTING)
	{
		return OpStatus::ERR;
	}

	OP_STATUS ret;
	if ((ret=m_file->SetFilePos(position)) != OpStatus::OK)
		return ret;

	m_bytes_sent = position;
	return OpStatus::OK;
}


OP_STATUS DCCFileSender::DCCConnectionConnected()
{
	SetStatus(DCC_STATUS_SENDING);
	Observer().OnDCCSendBegin(this);

	// Send our first chunk.
	if (OpStatus::IsError(SendChunk()))
		Observer().OnDCCSendFailed(this);

	return OpStatus::OK;
}


OP_STATUS DCCFileSender::DCCConnectionDataReady()
{
	unsigned char buffer[4];
	unsigned int bytes_read = 0;

	OpStatus::Ignore(m_connection_impl->ReadData(buffer, 4, bytes_read));
	while (int(bytes_read) > 0)
	{
		OpStatus::Ignore(m_receive_buffer.Append(buffer, bytes_read));

		// Did we get a complete ack?
		if (m_receive_buffer.DataSize() == 4)
		{
			m_receive_buffer.ShrinkToFit();

			const UINT32 ack = OpMisc::CharsToLong(m_receive_buffer.Buffer());
			m_receive_buffer.Empty();

			// Check if we're done or if we have failed miserably.
			if (ack == GetFileSize())
			{
				Observer().OnDCCSendComplete(this);
				return OpStatus::OK;
			}
			else if (ack > GetFileSize()) // Should not happen.
			{
				Observer().OnDCCSendFailed(this);
				return OpStatus::OK;
			}
			else if (ack < GetFileSize())
			{
				if (!m_send_ahead)
				{
					// If we have disable send ahead, we should only send if
					// we have received a positive ack for the number of bytes
					// we have sent out.
					if (ack == m_bytes_sent)
					{
						if (OpStatus::IsError(SendChunk()))
						{
							Observer().OnDCCSendFailed(this);
							return OpStatus::OK;
						}
					}
				}
			}
		}

		bytes_read = 0;
		OpStatus::Ignore(m_connection_impl->ReadData(buffer, 4, bytes_read));
	}

	return OpStatus::OK;
}


OP_STATUS DCCFileSender::DCCConnectionDataSent(UINT32 bytes_sent)
{
	// Update count and notify observer.
	m_bytes_sent += bytes_sent;
	Observer().OnDCCDataSent(this, (UINT32)m_bytes_sent);

	// If we have enabled send ahead, just continue sending.
	if (m_send_ahead)
	{
		if (OpStatus::IsError(SendChunk()))
			Observer().OnDCCSendFailed(this);
	}

	return OpStatus::OK;
}


OP_STATUS DCCFileSender::DCCConnectionClosed()
{
	Observer().OnDCCSendFailed(this);
	return OpStatus::OK;
}


OP_STATUS DCCFileSender::SendChunk()
{
	OP_ASSERT(m_file != 0);

	const UINT remaining_bytes = (UINT)GetFileSize() - (UINT)m_bytes_sent;
	const UINT bytes_to_read = min(remaining_bytes, 4096U);

	if (remaining_bytes == 0)
		return OpStatus::OK;

	OpByteBuffer read_buffer;
	RETURN_IF_ERROR(read_buffer.Resize(bytes_to_read));
	OpFileLength bytes_read = 0;
	if (m_file->Read(read_buffer.Buffer(), bytes_to_read, &bytes_read) != OpStatus::OK)
		return OpStatus::ERR;

	// Attempt to send the data.
	if (OpStatus::IsError(m_connection_impl->SendData(read_buffer.Buffer(), read_buffer.BufferSize())))
	{
		OpFileLength pos;
		if (m_file->GetFilePos(pos)!=OpStatus::OK ||
			pos < bytes_to_read ||
			m_file->SetFilePos(pos - bytes_to_read)!=OpStatus::OK)
		{
			return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}


//****************************************************************************
//
//	DCCChat
//
//****************************************************************************

DCCChat::DCCChat(DCCObserver& observer)
:	Autodeletable(),
	DCCBase(observer)
{
}


OP_STATUS DCCChat::Init(const OpStringC& other_party)
{
	RETURN_IF_ERROR(DCCBase::Init(other_party));
	return OpStatus::OK;
}


OP_STATUS DCCChat::SendText(const OpStringC& text, const OpStringC8& encoding)
{
	OpString8 charset;
	RETURN_IF_ERROR(charset.Set(encoding));

	// Convert text from ucs2 to the given encoding.
	OpString8 converted;
	RETURN_IF_ERROR(
		MessageEngine::GetInstance()->ConvertToBestChar8(charset, TRUE, text,
		converted));

	RETURN_IF_ERROR(SendText(converted));
	return OpStatus::OK;
}


//****************************************************************************
//
//	DCCChatClient
//
//****************************************************************************

DCCChatClient::DCCChatClient(DCCObserver &observer)
:	DCCChat(observer),
	m_socket(0),
	m_socket_address(0)
{
}


DCCChatClient::~DCCChatClient()
{
	if (m_socket != 0)
		Glue().DestroySocket(m_socket);

	if (m_socket_address != 0)
		Glue().DestroySocketAddress(m_socket_address);
}


OP_STATUS DCCChatClient::Init(const OpStringC& other_party,
	const OpStringC& longip, UINT port)
{
	RETURN_IF_ERROR(DCCChat::Init(other_party));

	// Check validity of parameters.
	if (longip.IsEmpty())
		return OpStatus::ERR;
	else if (port == 0)
		return OpStatus::ERR;

	// Create a socket address to connect to.
	m_socket_address = Glue().CreateSocketAddress();
	if (m_socket_address == 0)
		return OpStatus::ERR_NO_MEMORY;

	m_socket_address->FromString(longip.CStr());
	if (!m_socket_address->IsValid())
		return OpStatus::ERR;

	m_socket_address->SetPort(port);
	SetPortNumber(port);

	// Create a socket.
	m_socket = Glue().CreateSocket(*this);
	if (m_socket == 0)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_socket->Connect(m_socket_address));
	Glue().DestroySocketAddress(m_socket_address);
	m_socket_address = 0;

	SetStatus(DCC_STATUS_STARTING);
	return OpStatus::OK;
}


OP_STATUS DCCChatClient::SendText(const OpStringC8& text)
{
	OP_ASSERT(CurrentStatus() == DCC_STATUS_CONNECTED);

	// A DCC Send buffer must end with a newline.
	RETURN_IF_ERROR(m_send_buffer.Set(text));
	RETURN_IF_ERROR(m_send_buffer.Append("\n"));

	RETURN_IF_ERROR(m_socket->Send(m_send_buffer.CStr(), m_send_buffer.Length()));

	Observer().OnDCCChatDataSent(this, m_send_buffer);
	return OpStatus::OK;
}


void DCCChatClient::OnSocketConnected(OpSocket* socket)
{
	SetStatus(DCC_STATUS_CONNECTED);
	Observer().OnDCCChatBegin(this);
}


void DCCChatClient::OnSocketConnectError(OpSocket* socket, OpSocket::Error error)
{
	Observer().OnDCCChatClosed(this);
}


void DCCChatClient::OnSocketDataReady(OpSocket* socket)
{
	OP_ASSERT(socket == m_socket);

	char read_buffer[4096];
	unsigned int bytes_received = 0;

	m_socket->Recv(read_buffer, 4096, &bytes_received);

	while (int(bytes_received) > 0)
	{
		m_receive_buffer.Append(read_buffer, bytes_received);
		HandleDataReceived();

		// Read more data, if any.
		bytes_received = 0;

		m_socket->Recv(read_buffer, 4096, &bytes_received);
	}
}


void DCCChatClient::OnSocketClosed(OpSocket* socket)
{
	Observer().OnDCCChatClosed(this);
}


void DCCChatClient::OnSocketReceiveError(OpSocket* socket, OpSocket::Error error)
{
	if (error != OpSocket::SOCKET_BLOCKING)
	{
		Observer().OnDCCChatClosed(this);
	}
}


OP_STATUS DCCChatClient::HandleDataReceived()
{
	// These messages will be ended by \n, as far as I can tell.
	int newline_pos = m_receive_buffer.Find("\n");
	while (newline_pos != KNotFound)
	{
		OpString8 line;
		RETURN_IF_ERROR(line.Set(m_receive_buffer.CStr(), newline_pos));

		Observer().OnDCCChatDataReceived(this, line);

		m_receive_buffer.Delete(0, newline_pos + 1);
		newline_pos = m_receive_buffer.Find("\n");
	}

	return OpStatus::OK;
}


//****************************************************************************
//
//	DCCCollection
//
//****************************************************************************

DCCCollection::DCCCollection()
{
}


DCCCollection::~DCCCollection()
{
	m_active_file_receivers.DeleteAll();
	m_active_file_senders.DeleteAll();
}


OP_STATUS DCCCollection::AddActiveFileReceiver(DCCFileReceiver* receiver)
{
	if (receiver == 0)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_active_file_receivers.Add(receiver->PortNumber(), receiver));
	return OpStatus::OK;
}

OP_STATUS DCCCollection::AddPassiveFileReceiver(DCCFileReceiver* receiver)
{
	if (receiver == 0)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_passive_file_receivers.Add(receiver->Id(), receiver));
	return OpStatus::OK;
}


OP_STATUS DCCCollection::AddActiveFileSender(DCCFileSender* sender)
{
	if (sender == 0)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_active_file_senders.Add(sender->PortNumber(), sender));
	return OpStatus::OK;
}


OP_STATUS DCCCollection::AddPassiveFileSender(DCCFileSender* sender)
{
	if (sender == 0)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_passive_file_senders.Add(sender->Id(), sender));
	return OpStatus::OK;
}


OP_STATUS DCCCollection::RemoveFileReceiver(DCCFileReceiver* receiver)
{
	if (receiver == 0)
		return OpStatus::ERR_NULL_POINTER;

	if (m_passive_file_receivers.Contains(receiver->Id()))
		OpStatus::Ignore(m_passive_file_receivers.Remove(receiver->Id(), &receiver));
	else
		OpStatus::Ignore(m_active_file_receivers.Remove(receiver->PortNumber(), &receiver));

	autodelete(receiver);
	return OpStatus::OK;
}


OP_STATUS DCCCollection::RemoveFileSender(DCCFileSender* sender)
{
	if (sender == 0)
		return OpStatus::ERR_NULL_POINTER;

	if (m_passive_file_senders.Contains(sender->Id()))
		OpStatus::Ignore(m_passive_file_senders.Remove(sender->Id(), &sender));
	else
		OpStatus::Ignore(m_active_file_senders.Remove(sender->PortNumber(), &sender));

	autodelete(sender);
	return OpStatus::OK;
}


OP_STATUS DCCCollection::FileReceiver(UINT port_number, UINT id,
	DCCFileReceiver*& receiver) const
{
	if (port_number == 0)
		RETURN_IF_ERROR(FileReceiverFromId(id, receiver));
	else
		RETURN_IF_ERROR(FileReceiverFromPort(port_number, receiver));

	return OpStatus::OK;
}


OP_STATUS DCCCollection::FileReceiverFromPort(UINT port_number, DCCFileReceiver*& receiver) const
{
	RETURN_IF_ERROR(Collection().m_active_file_receivers.GetData(port_number, &receiver));
	return OpStatus::OK;
}


OP_STATUS DCCCollection::FileReceiverFromId(UINT id, DCCFileReceiver*& receiver) const
{
	RETURN_IF_ERROR(Collection().m_passive_file_receivers.GetData(id, &receiver));
	return OpStatus::OK;
}


OP_STATUS DCCCollection::FileSender(UINT port_number, UINT id, DCCFileSender*& sender) const
{
	if (port_number == 0)
		RETURN_IF_ERROR(FileSenderFromId(id, sender));
	else
		RETURN_IF_ERROR(FileSenderFromPort(port_number, sender));

	return OpStatus::OK;
}


OP_STATUS DCCCollection::FileSenderFromPort(UINT port_number, DCCFileSender*& sender) const
{
	RETURN_IF_ERROR(Collection().m_active_file_senders.GetData(port_number, &sender));
	return OpStatus::OK;
}


OP_STATUS DCCCollection::FileSenderFromId(UINT id, DCCFileSender*& sender) const
{
	RETURN_IF_ERROR(Collection().m_passive_file_senders.GetData(id, &sender));
	return OpStatus::OK;
}

#endif // IRC_SUPPORT
