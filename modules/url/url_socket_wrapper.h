/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _URL_SOCKET_WRAPPER_H_
#define _URL_SOCKET_WRAPPER_H_

#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpUdpSocket.h"
#include "modules/pi/network/OpHostResolver.h"

class SocketWrapper : public OpSocket, public OpSocketListener
{
public:
	enum Flags {
		NO_WRAPPERS              = 0,      // no wrappers- a raw OpSocket
		USE_SSL                  = 1 << 0, // use SSL
		ALLOW_SOCKS_WRAPPER      = 1 << 1, // enable OpSocksSocket
		ALLOW_CONNECTION_WRAPPER = 1 << 2, // enable ConnectionSocketWrapper

			// convenience value
		ALLOW_ALL_WRAPPERS = ALLOW_SOCKS_WRAPPER | ALLOW_CONNECTION_WRAPPER
	};

		/** Create a possibly wrapped OpSocket.  
		 * 
		 * @param[out] socket The resulting OpSocket will be placed here.  
		 * @param listener The listener of the socket (see OpSocketListener).
		 * @param flags A bitwise ORed set of SocketWrapper::Flags values, that are used 
		 * when creating the socket.
		 */
	static OP_STATUS CreateTCPSocket(OpSocket **socket, OpSocketListener *listener, unsigned int flags);

#ifdef OPUDPSOCKET
		/** Create a possibly wrapped OpUdpSocket.  
		 * 
		 * @param[out] socket The resulting OpUdpSocket will be placed here.  
		 * @param listener The listener of the socket (see OpUdpSocketListener).
		 * @param flags A bitwise ORed set of SocketWrapper::Flags values, that are used 
		 * when creating the socket.
		 * @param local_address Local address that may be used to select which network 
		 * interface to use.
		 */
	static OP_STATUS CreateUDPSocket(OpUdpSocket **socket, OpUdpSocketListener *listener, 
			unsigned int flags, OpSocketAddress* local_address = NULL);
#endif // #ifdef OPUDPSOCKET

		/** Create a possibly wrapped OpHostResolver.
		 * 
		 * @param[out] host_resolver Set to the host resolver created.
		 * @param listener Listener for the resolver.
		 */
	static OP_STATUS CreateHostResolver(OpHostResolver** host_resolver, OpHostResolverListener* listener);


	SocketWrapper (OpSocket *socket) : m_socket(socket), m_listener(NULL) { socket->SetListener(this); }
	virtual ~SocketWrapper() { OP_DELETE(m_socket); }

	// One-to-one OpSocket wrappers
	virtual OP_STATUS Connect(OpSocketAddress *socket_address) { return m_socket->Connect(socket_address); }
	virtual OP_STATUS Send(const void *data, UINT length) { return m_socket->Send(data, length); }
	virtual OP_STATUS Recv(void *buffer, UINT length, UINT *bytes_received) { return m_socket->Recv(buffer, length, bytes_received); }
	virtual void Close() { m_socket->Close(); }
	virtual OP_STATUS GetSocketAddress(OpSocketAddress* socket_address) { return m_socket->GetSocketAddress(socket_address); }
#ifdef OPSOCKET_GETLOCALSOCKETADDR
	virtual OP_STATUS GetLocalSocketAddress(OpSocketAddress* socket_address) { return m_socket->GetLocalSocketAddress(socket_address); }
#endif
	virtual void SetListener(OpSocketListener *listener) { m_listener = listener; }
#ifdef SOCKET_LISTEN_SUPPORT
	virtual OP_STATUS Listen(OpSocketAddress* socket_address, int queue_size) { return m_socket->Listen(socket_address, queue_size); }
	virtual OP_STATUS Accept(OpSocket *socket) { return m_socket->Accept(socket); }
#endif
#ifdef _EXTERNAL_SSL_SUPPORT_
	virtual OP_STATUS UpgradeToSecure() { return m_socket->UpgradeToSecure(); }
	virtual OP_STATUS SetSecureCiphers(const cipherentry_st* ciphers, UINT cipher_count) { return m_socket->SetSecureCiphers(ciphers, cipher_count); }
	virtual OP_STATUS AcceptInsecureCertificate(OpCertificate *certificate) { return m_socket->AcceptInsecureCertificate(certificate); }
	virtual OP_STATUS GetCurrentCipher(cipherentry_st* used_cipher, BOOL* tls_v1_used, UINT32* pk_keysize) { return m_socket->GetCurrentCipher(used_cipher, tls_v1_used, pk_keysize); }
	virtual OP_STATUS SetServerName(const uni_char* server_name) { return m_socket->SetServerName(server_name); }
	virtual OpCertificate *ExtractCertificate() { return m_socket->ExtractCertificate(); }
	virtual OpAutoVector<OpCertificate> *ExtractCertificateChain() { return m_socket->ExtractCertificateChain(); }
	virtual int GetSSLConnectErrors() { return m_socket->GetSSLConnectErrors(); }
#endif
#ifdef SOCKET_NEED_ABORTIVE_CLOSE
	virtual void SetAbortiveClose() { m_socket->SetAbortiveClose(); }
#endif
#ifdef SOCKET_SUPPORTS_TIMER
	virtual void SetTimeOutInterval(unsigned int seconds, BOOL restore_default) { m_socket->SetTimeOutInterval(seconds, restore_default); }
#endif
#ifdef OPSOCKET_PAUSE_DOWNLOAD
	virtual OP_STATUS PauseRecv() { return m_socket->PauseRecv(); }
	virtual OP_STATUS ContinueRecv() { return m_socket->ContinueRecv(); }
#endif
#ifdef OPSOCKET_OPTIONS
	virtual OP_STATUS SetSocketOption(OpSocket::OpSocketBoolOption option, BOOL value) { return m_socket->SetSocketOption(option, value); }
	virtual OP_STATUS GetSocketOption(OpSocketBoolOption option, BOOL& out_value) { return m_socket->GetSocketOption(option, out_value); }
#endif // OPSOCKET_OPTIONS

	// One-to-one OpSocketListener wrappers
	virtual void OnSocketConnected(OpSocket* socket) { m_listener->OnSocketConnected(this); }
	virtual void OnSocketDataReady(OpSocket* socket) { m_listener->OnSocketDataReady(this); }
	virtual void OnSocketDataSent(OpSocket* socket, UINT bytes_sent) { m_listener->OnSocketDataSent(this, bytes_sent); }
	virtual void OnSocketClosed(OpSocket* socket) { m_listener->OnSocketClosed(this); }

#ifdef SOCKET_LISTEN_SUPPORT
	virtual void OnSocketConnectionRequest(OpSocket* socket) { m_listener->OnSocketConnectionRequest(this); }
	virtual void OnSocketListenError(OpSocket* socket, OpSocket::Error error) { m_listener->OnSocketListenError(this, error); }
#endif // SOCKET_LISTEN_SUPPORT

	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error error) { m_listener->OnSocketConnectError(this, error); }
	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error) { m_listener->OnSocketReceiveError(this, error); }
	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error error) { m_listener->OnSocketSendError(this, error); }
	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error error) { m_listener->OnSocketCloseError(this, error); }

#ifndef URL_CAP_TRUST_ONSOCKETDATASENT
	virtual void OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent) { m_listener->OnSocketDataSendProgressUpdate(this, bytes_sent); }
#endif // !URL_CAP_TRUST_ONSOCKETDATASENT

protected:
	OpSocket *m_socket;
	OpSocketListener *m_listener;
};

#endif // _URL_SOCKET_WRAPPER_H_
