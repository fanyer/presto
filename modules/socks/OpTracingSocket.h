/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/* This source file is used to generate equest-response sequences for OpSocksSocket.ot;
 * It is not supposed to be compiled and linked into any opera executable.
 */
#ifndef OP_TRACING_SOCKET_H
#define OP_TRACING_SOCKET_H

#ifdef SOCKS_SUPPORT

#include "modules/pi/network/OpSocket.h"

class OpSocketAddress;
class OpSocketListener;

/**
 * OpTracingSocket -- wrap-delegates the socket passed to its constructor and traces all the in- and out- bound traffic
 */
class OpTracingSocket : public OpSocket
{
public:
	enum state_t {
		CREATED,
		CONNECTED,
		CLOSED
	};

private:
	OpSocket*   socket;
	state_t     state;
	BOOL        do_trace;

	void  Trace(const char* method, const void* data, UINT length);

	void  Trace(const char* method, const char* str) { Trace(method, str, op_strlen(str)); }

public:

	OpTracingSocket(OpSocket* socket) : socket(socket), state(CREATED), do_trace(TRUE) {;}

	virtual ~OpTracingSocket() { OP_DELETE(socket); socket = NULL; }


	virtual OP_STATUS Connect(OpSocketAddress* socket_address)
	{
		OP_STATUS status = socket->Connect(socket_address);
		if (OpStatus::IsSuccess(status))
			state = CONNECTED;
		return status;
	}

	virtual OP_STATUS Send(const void* data, UINT length);

	virtual OP_STATUS Recv(void* buffer, UINT length, UINT* bytes_received);
	
	virtual void Close()
	{
		socket->Close();
		state = CLOSED;
	}

	virtual Type SocketType() { return socket->SocketType(); }

	virtual OP_STATUS GetSocketAddress(OpSocketAddress* socket_address) { return socket->GetSocketAddress(socket_address); }

#ifdef OPSOCKET_GETLOCALSOCKETADDR
	/** Get the address of the local end of the socket connection.
	 *
	 * @param socket_address a socket address to be filled in
	 */
	virtual OP_STATUS GetLocalSocketAddress(OpSocketAddress* socket_address) { return socket->GetLocalSocketAddress(socket_address); }
#endif // OPSOCKET_GETLOCALSOCKETADDR

	/** Set or change the listener for this socket.
	 *
	 * @param listener New listener to set.
	 */
	virtual void SetListener(OpSocketListener* listener) { socket->SetListener(listener); }

#if defined(SOCKET_LISTEN_SUPPORT)
	virtual OP_STATUS Listen(OpSocketAddress* socket_address, int queue_size) { return socket->Listen(socket_address, queue_size); }

	virtual OP_STATUS Accept(OpSocket* socket) { return socket->Accept(socket); }
#endif // SOCKET_LISTEN_SUPPORT

#ifdef _EXTERNAL_SSL_SUPPORT_
#error Oops!
	virtual OP_STATUS UpgradeToSecure();

	virtual OP_STATUS SetSecureCiphers(const cipherentry_st* ciphers, UINT cipher_count);

	virtual OP_STATUS AcceptInsecureCertificate(OpCertificate *certificate);

	virtual OP_STATUS GetCurrentCipher(cipherentry_st* used_cipher, BOOL* tls_v1_used, UINT32* pk_keysize);

	virtual OP_STATUS SetServerName(const uni_char* server_name);

	virtual OpCertificate *ExtractCertificate();

	virtual OpAutoVector<OpCertificate> *ExtractCertificateChain();

	virtual int GetSSLConnectErrors();
#endif // _EXTERNAL_SSL_SUPPORT_

#ifdef SOCKET_NEED_ABORTIVE_CLOSE
#error Oops!
	/** Inform the socket layer that an abortive close is required.
	 *
	 * To avoid unwanted re-transmissions in the TCP layer after closing a
	 * socket for e.g. a timed out HTTP connection, it is necessary to signal
	 * that the socket close should happen abortively. If this is signalled to
	 * the socket implementation the platform layer should do its best to shut
	 * down the socket immediately, resetting the socket connection if
	 * necessary.
	 */
	virtual void SetAbortiveClose();
#endif // SOCKET_NEED_ABORTIVE_CLOSE

#ifdef SOCKET_SUPPORTS_TIMER
#error Oops!
	virtual void SetTimeOutInterval(unsigned int seconds, BOOL restore_default = FALSE);
#endif // SOCKET_SUPPORTS_TIMER

#ifdef OPSOCKET_PAUSE_DOWNLOAD
#error Oops!
	virtual OP_STATUS PauseRecv();

	virtual OP_STATUS ContinueRecv();
#endif // OPSOCKET_PAUSE_DOWNLOAD

};

#endif //SOCKS_SUPPORT

#endif //OP_TRACING_SOCKET_H
