/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_SOCKS_SOCKET_H
#define OP_SOCKS_SOCKET_H
# ifdef SOCKS_SUPPORT

#define g_socks_IsSocksEnabled	g_opera->socks_module.IsSocksEnabled()

#include "modules/url/url_socket_wrapper.h"

class OpSocketAddress;
class OpSocketListener;
class OpSocketHostResolver;
class OpSocketHostResolverListener;

/**
 * OpSocksSocket -- an OpSocket wrapper which provides for transparent SOCKS proxy support.
 *
 * Instances of this class are normally created by calling SocketWrapper::CreateTCPSocket with 
 * the allowSOCKS argument set to TRUE.  Alternatively, you can create a regular OpSocket 
 * and then feed it to OpSocksSocket::Create, if you want to specify a particular SOCKS proxy.
 *
 * Socks v5 is to be implemented according to http://www.faqs.org/rfcs/rfc1928.html
 * With the following restrictions:
 * 1. The supported authentication methods are <no_auth> and <username,password> (no GSS API!)
 * 2. No UDP connections, no TCP-server connections! Only TCP-client connections are supported.
 * 3. No IPv6 addresses are supported.
 *
 * TODO: as janvidar noted (outgoing) UDP connections should probably be
 *       supported, as otherwise DNS lookups won't work if the DNS server is
 *       only reachable through the SOCKS proxy.  But, for that, we need to
 *       implement OpUdpSocket, not OpSocket (OpSocket::Create's UDP as type
 *       parameter is entirely bogus).
 */
class OpSocksSocket : public OpSocket, public OpSocketListener, private OpHostResolverListener
{
		/** Construct an OpSocksSocket which wraps an inner OpSocket.  This should only 
		 * be called externally by the SocketWrapper class.  
		 *
		 * @param listener The listener of the socket (see OpSocketListener).
		 * @param secure TRUE if the socket is to be secure, meaning that an SSL connection is 
		 * to be established when Connect() is called.
		 * @param inner_socket The underlying OpSocket that will be wrapped.
		 */
		OpSocksSocket(OpSocketListener* listener, BOOL secure, OpSocket &inner_socket);

		friend class SocketWrapper;

public:
		/** Create an OpSocksSocket which wraps inner_socket, using the SOCKS server 
		 * located at socks_server_name:socks_server_port
		 *
		 * @param wrapped_socket (output) The Resulting OpSocksSocket will be put here.
		 * @param inner_socket An underlying OpSocket that the OpSocksSocket will wrap.
		 * @param listener The listener of the socket (see OpSocketListener).
		 * @param socks_server_name The hostname (or IP address) of the SOCKS server to use.
		 * @param socks_server_port The port number of the SOCKS server to use.
		 */
	static OP_STATUS Create(OpSocket **wrapped_socket, OpSocket& inner_socket, OpSocketListener* listener, BOOL secure, ServerName* socks_server_name, UINT socks_server_port);

	virtual ~OpSocksSocket();


	virtual OP_STATUS Connect(OpSocketAddress* socket_address);

	// This method allows target_host to be (unresolved) hostname
	virtual OP_STATUS Connect(const uni_char* target_host, signed short target_port);

	virtual OP_STATUS Send(const void* data, UINT length);

	virtual OP_STATUS Recv(void* buffer, UINT length, UINT* bytes_received);

	virtual void Close() { if (m_socks_socket) m_socks_socket->Close(); }

	virtual OP_STATUS GetSocketAddress(OpSocketAddress* socket_address)
		{ return m_socks_socket ? m_socks_socket->GetSocketAddress(socket_address) : OpStatus::ERR; }

#ifdef OPSOCKET_GETLOCALSOCKETADDR
	/** Get the address of the local end of the socket connection.
	 *
	 * @param socket_address a socket address to be filled in
	 */
	virtual OP_STATUS GetLocalSocketAddress(OpSocketAddress* socket_address)
		{ return m_socks_socket ? m_socks_socket->GetLocalSocketAddress(socket_address) : OpStatus::ERR; }
#endif // OPSOCKET_GETLOCALSOCKETADDR

	/** Set or change the listener for this socket.
	 *
	 * @param listener New listener to set.
	 */
	virtual void SetListener(OpSocketListener* listener) { m_listener = listener; }

#if defined(SOCKET_LISTEN_SUPPORT)
	virtual OP_STATUS Listen(OpSocketAddress* socket_address, int queue_size);

	virtual OP_STATUS Accept(OpSocket* socket);
#endif // SOCKET_LISTEN_SUPPORT

#ifdef _EXTERNAL_SSL_SUPPORT_
	virtual OP_STATUS UpgradeToSecure()
		{ return m_socks_socket ? m_socks_socket->UpgradeToSecure() : OpStatus::ERR; }

	virtual OP_STATUS SetSecureCiphers(const cipherentry_st* ciphers, UINT cipher_count)
		{ return m_socks_socket ? m_socks_socket->SetSecureCiphers(ciphers, cipher_count) : OpStatus::ERR; }

	virtual OP_STATUS AcceptInsecureCertificate(OpCertificate *certificate)
		{ return m_socks_socket ? m_socks_socket->AcceptInsecureCertificate(certificate) : OpStatus::ERR; }

	virtual OP_STATUS GetCurrentCipher(cipherentry_st* used_cipher, BOOL* tls_v1_used, UINT32* pk_keysize)
		{ return m_socks_socket ? m_socks_socket->GetCurrentCipher(used_cipher, tls_v1_used, pk_keysize) : OpStatus::ERR; }

	virtual OP_STATUS SetServerName(const uni_char* server_name)
		{ return m_socks_socket ? m_socks_socket->SetServerName(server_name) : OpStatus::ERR; }

	virtual OpCertificate *ExtractCertificate()
		{ return m_socks_socket ? m_socks_socket->ExtractCertificate() : 0; }

	virtual OpAutoVector<OpCertificate> *ExtractCertificateChain()
		{ return m_socks_socket ? m_socks_socket->ExtractCertificateChain() : 0; }

	virtual int GetSSLConnectErrors()
		{ return m_socks_socket ? m_socks_socket->GetSSLConnectErrors() : 0; }
#endif // _EXTERNAL_SSL_SUPPORT_

#ifdef SOCKET_NEED_ABORTIVE_CLOSE
	/** Inform the socket layer that an abortive close is required.
	 *
	 * To avoid unwanted re-transmissions in the TCP layer after closing a
	 * socket for e.g. a timed out HTTP connection, it is necessary to signal
	 * that the socket close should happen abortively. If this is signalled to
	 * the socket implementation the platform layer should do its best to shut
	 * down the socket immediately, resetting the socket connection if
	 * necessary.
	 */
	virtual void SetAbortiveClose()
		{ if (m_socks_socket) m_socks_socket->SetAbortiveClose(); }
#endif // SOCKET_NEED_ABORTIVE_CLOSE

#ifdef SOCKET_SUPPORTS_TIMER
	virtual void SetTimeOutInterval(unsigned int seconds, BOOL restore_default = FALSE)
		{ if (m_socks_socket) m_socks_socket->SetTimeOutInterval(seconds, restore_default); }
#endif // SOCKET_SUPPORTS_TIMER

#ifdef OPSOCKET_PAUSE_DOWNLOAD
	virtual OP_STATUS PauseRecv()
		{ return m_socks_socket ? m_socks_socket->PauseRecv() : OpStatus::ERR; }

	virtual OP_STATUS ContinueRecv()
		{ return m_socks_socket ? m_socks_socket->ContinueRecv() : OpStatus::ERR; }
#endif // OPSOCKET_PAUSE_DOWNLOAD

#ifdef OPSOCKET_OPTIONS
	virtual OP_STATUS SetSocketOption(OpSocketBoolOption option, BOOL value) { return m_socks_socket ? m_socks_socket->SetSocketOption(option, value) : OpStatus::ERR; }
	virtual OP_STATUS GetSocketOption(OpSocketBoolOption option, BOOL& out_value) { return m_socks_socket ? m_socks_socket->GetSocketOption(option, out_value) : OpStatus::ERR; }
#endif // OPSOCKET_OPTIONS

public:
	/** OpSocketListener impl. */
	/** The socket has connected to a remote host successfully. */
	virtual void OnSocketConnected(OpSocket* socket);

	/** There is data ready to be received on the socket. */
	virtual void OnSocketDataReady(OpSocket* socket);

	/** Data has been sent on the socket. */
	virtual void OnSocketDataSent(OpSocket* socket, UINT bytes_sent);

	/** The socket has closed. */
	virtual void OnSocketClosed(OpSocket* socket);

#ifdef SOCKET_LISTEN_SUPPORT
	/** A listening socket has received an incoming connection request. */
	virtual void OnSocketConnectionRequest(OpSocket* socket);

	/** An error has occured on the socket whilst setting it up for listening. */
	virtual void OnSocketListenError(OpSocket* socket, OpSocket::Error error);
#endif // SOCKET_LISTEN_SUPPORT

	/** An error has occured on the socket whilst connecting. */
	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error error);

	/** An error has occured on the socket whilst receiving. */
	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);

	/** An error has occured on the socket whilst sending. */
	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error error);

	/** An error has occured on the socket whilst closing. */
	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error error);

private:
	// invoked by diff varieties of Connect:
	OP_STATUS CommonConnect();


	OP_STATUS  SocksSendConnectRequest();


	BOOL              m_secure;
	OpSocket*         m_socks_socket;
	OpString          m_target_host;   // provided as an argument to the Connect() method
	UINT              m_target_port;   // provided as an argument to the Connect() method
	OpSocketAddress*  m_actual_target_address; // returned by the socks server upon establishing connection to the target address
	OpSocketListener* m_listener;
	UINT              m_nb_sent_ignore;  // num bytes sent to the proxy server as part of the session handshake (these should not be signaled to the listener)
	UINT              m_nb_recv_ignore;  // num bytes received from the proxy server as part of the session handshake (these should not be signaled to the listener)

	/** When OpSocksSocket is created with custom SOCKS server name specified, then a new HostResolver is created to resolve the server name; After this, it is deleted and NULL-ified
	 *  In all other cases this property is NULL
	 */
	OpHostResolver*   m_host_resolver;

	/** Used to store the SOCKS server address (that is IP:port) in case of a custom socks server */
	OpSocketAddress*  m_socks_address;

	/** Used to store the SOCKS ServerName pointer only in case it is not yet resolved and until it is resolved */
	ServerName_Pointer m_socks_server_name;

	enum State
	{
		SS_CONNECTION_FAILED = -1,
		SS_AUTH_FAILED = -2,
		SS_CONNECTION_CLOSED = -3,
			SS_SOCKS_RESOLVE_FAILED = -4,// (optional) socket created with custom socks proxy which failed to resolve

		SS_CONSTRUCTED = 0,          // this obj constructed
			SS_RESOLVING_SOCKS,          // (optional) socket created with custom socks proxy which is currently being resolved
			SS_SOCKS_RESOLVED,           // (optional) socket created with custom socks proxy which is already resolved (Connect() hasn't been invoked yet)
			SS_WAITING_RESOLVING_SOCKS,  // (optional) socket created with custom socks proxy which is currently being resolved *and* Connect() has been invoked, but had to yield in order for the resolve to finish)
		SS_SOCKET_TO_SOCKS_CREATED,     // m_socks_socket created &
		SS_SOCKS_CONNECTED,
		SS_RQ1_SENT,
		SS_USERNAME_PASSWORD_SENT,
		SS_TARGET_ADDR_SENT,
		SS_RECV_TO_SKIP,
		SS_TARGET_CONNECTED,
	};
	State             m_state;

	void       Error(State state);

	// Impl of OpHostResolverListener:
	virtual void OnHostResolved(OpHostResolver* host_resolver);
	virtual void OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error);

public:

	// Used for DNS-like searches by class OpSocksHostResolver
	OpSocketAddress*  RelinquishActualTargetAddress()
	{
		OpSocketAddress* res = m_actual_target_address;
		m_actual_target_address = NULL;
		return res;
	}
};

# endif // SOCKS_SUPPORT
#endif // OP_SOCKS_SOCKET_H
