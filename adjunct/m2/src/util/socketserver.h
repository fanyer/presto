/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef UTIL_SOCKETSERVER_H
#define UTIL_SOCKETSERVER_H

#include "modules/pi/network/OpSocket.h"
#include "modules/hardcore/timer/optimer.h"
#include "adjunct/m2/src/util/buffer.h"

//****************************************************************************
//
//	SocketServer
//
//****************************************************************************

/*! A class encapsulating the mysteries of creating a socket server. Will set
	up a listening socket and accept a connection to it, as well as managing
	the send buffer for you, thus enabling you to just call
	SocketServer::Send() and forget about the fact that you need to hold on to
	the buffer. This class will also allow you to set a timeout to decide how
	long to wait before shutting the server down.
*/

class SocketServer
:	public OpSocketListener,
	public OpTimerListener
{
public:
	// Listener.
	class SocketServerListener
	{
	public:
		virtual void OnSocketConnected(const OpStringC &connected_to) { }
		virtual void OnSocketClosed() { }
		virtual void OnSocketDataAvailable() { }
		virtual void OnSocketDataSent(UINT bytes_sent) { }
		virtual void OnSocketError() { }

	protected:
		SocketServerListener() { }
		virtual ~SocketServerListener() { }
	};

	// Construction / destruction.
	SocketServer(SocketServerListener& listener);
	~SocketServer();

	OP_STATUS Init(UINT listen_port_start, UINT listen_port_end,
		UINT &port_used, UINT timeout_seconds = 0);

	// Methods.
	OP_STATUS Read(void *buffer, UINT buffer_size, UINT &bytes_read);
	OP_STATUS Send(void *buffer, UINT buffer_size);

private:
	// No copy or assignment.
	SocketServer(SocketServer const &other);
	SocketServer&operator=(SocketServer const &rhs);

	// Methods.
	void StopListening();

	// OpSocketObserver.
	virtual void OnSocketConnected(OpSocket* socket) { }
	virtual void OnSocketDataReady(OpSocket* socket);
	virtual void OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent);
	virtual void OnSocketClosed(OpSocket* socket);
	virtual void OnSocketConnectionRequest(OpSocket* socket);
	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error error) { }
#ifdef PI_CAP_SOCKET_LISTEN_CLEANUP
	virtual void OnSocketListenError(OpSocket* socket, OpSocket::Error error) {}
#endif
	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error error) { }
	virtual void OnSocketDataSendProgressUpdate(OpSocket* socket, unsigned int bytes_sent) { }

	// OpTimerListener.
	virtual void OnTimeOut(OpTimer* timer);

	// Members.
	SocketServerListener& m_listener;

	OpAutoPtr<OpSocket> m_listening_socket;
	OpAutoPtr<OpSocket> m_connected_socket;

	OpByteBuffer m_send_buffer;
	OpAutoPtr<OpTimer> m_timer;
};

#endif
