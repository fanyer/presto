/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "socketserver.h"

#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/glue/factory.h"

#include "modules/pi/network/OpSocketAddress.h"

//****************************************************************************
//
//	SocketServer
//
//****************************************************************************

SocketServer::SocketServer(SocketServerListener& listener)
:	m_listener(listener),
	m_timer(0)
{
}


SocketServer::~SocketServer()
{
}


OP_STATUS SocketServer::Init(UINT listen_port_start, UINT listen_port_end,
	UINT &port_used, UINT timeout_seconds)
{
	// Create a socket and start to listen at the given port.
	m_listening_socket = MessageEngine::GetInstance()->GetGlueFactory()->CreateSocket(*this);
	if (m_listening_socket.get() == 0)
		return OpStatus::ERR_NO_MEMORY;

	port_used = 0;
	OpSocketAddress *address;
	if (OpStatus::IsError(OpSocketAddress::Create(&address)))
		return OpStatus::ERR_NO_MEMORY;
	for (UINT port = listen_port_start; port <= listen_port_end; ++port)
	{
		address->SetPort(port);
		if (OpStatus::IsSuccess(m_listening_socket->Listen(address, 1)))
		{
			port_used = port;
			break;
		}
	}
	OP_DELETE(address);

	if (port_used == 0)
		return OpStatus::ERR;

	// Create a timer if we selected a timeout different than 0.
	if (timeout_seconds > 0)
	{
		OpTimer* timer = OP_NEW(OpTimer, ());
		if (timer == 0)
			return OpStatus::ERR_NO_MEMORY;

		m_timer = timer;
		m_timer->SetTimerListener(this);
		m_timer->Start(timeout_seconds * 1000);
	}

	return OpStatus::OK;
}


OP_STATUS SocketServer::Read(void *buffer, UINT buffer_size, UINT &bytes_read)
{
	bytes_read = 0;
	m_connected_socket->Recv(buffer, buffer_size, &bytes_read);

	return OpStatus::OK;
}


OP_STATUS SocketServer::Send(void *buffer, UINT buffer_size)
{
	OP_ASSERT(m_connected_socket.get() != 0);

	const UINT old_send_buffer_size = m_send_buffer.DataSize();
	RETURN_IF_ERROR(m_send_buffer.Append((unsigned char *)(buffer), buffer_size));

	// Only send the data we have just appended; the rest of the data is
	// allready sent and will be removed when OnSocketDataSent is called.
	m_connected_socket->Send(m_send_buffer.Buffer() + old_send_buffer_size, buffer_size);
	return OpStatus::OK;
}


void SocketServer::StopListening()
{
	// Shutting down the listening socket before the accepted socket is shut
	// down seems to be creating weird connection errors for the next listen
	// socket that is created. Thus we don't do anything here, but just let
	// the listening socket die together with the accept socket; in the
	// SocketServer destructor.
}


void SocketServer::OnSocketDataReady(OpSocket* socket)
{
	m_listener.OnSocketDataAvailable();
}


void SocketServer::OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent)
{
	// Remove the bytes that we have sent from the buffer.
	OP_ASSERT(m_send_buffer.DataSize() >= bytes_sent);
	m_send_buffer.Remove(bytes_sent);

	// Notify the observer.
	m_listener.OnSocketDataSent(bytes_sent);
}


void SocketServer::OnSocketClosed(OpSocket* socket)
{
	m_listener.OnSocketClosed();
}


void SocketServer::OnSocketConnectionRequest(OpSocket* socket)
{
	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();

	// We have an incoming connection request.
	m_connected_socket = glue_factory->CreateSocket(*this);
	if (m_connected_socket.get() != 0)
	{
		m_listening_socket->Accept(m_connected_socket.get());
		StopListening();

		// Is there a timer running? If so, kill it.
		m_timer = 0;

		// Retrieve the ip address of the host connecting.
		OpString connecting_host;

		{
			OpSocketAddress *socket_address = glue_factory->CreateSocketAddress();
			m_connected_socket->GetSocketAddress(socket_address);

			socket_address->ToString(&connecting_host);
			glue_factory->DestroySocketAddress(socket_address);
		}

		m_listener.OnSocketConnected(connecting_host);
	}
}


void SocketServer::OnSocketReceiveError(OpSocket* socket, OpSocket::Error error)
{
	if (error != OpSocket::SOCKET_BLOCKING)
		m_listener.OnSocketError();
}


void SocketServer::OnSocketSendError(OpSocket* socket, OpSocket::Error error)
{
	if (error != OpSocket::SOCKET_BLOCKING)
		m_listener.OnSocketError();
}


void SocketServer::OnTimeOut(OpTimer* timer)
{
	// Ok, don't listen anymore.
	StopListening();

	// Send a notification that the socket has closed.
	m_listener.OnSocketClosed();
}

#endif //M2_SUPPORT
