/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OpenSSLSocket.h
 *
 * SSL-capable socket.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef OPENSSL_SOCKET_H
#define OPENSSL_SOCKET_H

#ifdef EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

#include "modules/externalssl/src/OpSSLSocket.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/ssl.h"

#define OPENSSL_SOCKET_BUFFER_SIZE (16 * 1024)


class OpenSSLSocket: public OpSSLSocket, public OpSocketListener, public MessageObject
{
public:
	OpenSSLSocket(OpSocketListener* listener, BOOL secure = FALSE);
	OP_STATUS Init();
	virtual ~OpenSSLSocket();

public:
	// OpSocket methods
	virtual OP_STATUS Connect(OpSocketAddress* socket_address);
	virtual OP_STATUS Send(const void* data, UINT length);
	virtual OP_STATUS Recv(void* buffer, UINT length, UINT* bytes_received);
	virtual void Close();
	virtual OP_STATUS GetSocketAddress(OpSocketAddress* socket_address);
	virtual void SetListener(OpSocketListener* listener) { m_listener = listener; }

#ifdef OPSOCKET_PAUSE_DOWNLOAD
	// OpSocket pause/resume methods
	virtual OP_STATUS PauseRecv();
	virtual OP_STATUS ContinueRecv();
#endif // OPSOCKET_PAUSE_DOWNLOAD

#ifdef OPSOCKET_OPTIONS
	// OpSocket option methods
	virtual OP_STATUS SetSocketOption(OpSocketBoolOption option, BOOL value);
	virtual OP_STATUS GetSocketOption(OpSocketBoolOption option, BOOL& out_value);
#endif // OPSOCKET_OPTIONS

#ifdef SOCKET_LISTEN_SUPPORT
	// OpSocket server methods
	virtual OP_STATUS Listen(OpSocketAddress* socket_address, int queue_size) { Unimplemented(); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS Accept(OpSocket* socket) { Unimplemented(); return OpStatus::ERR_NOT_SUPPORTED; }
#endif // SOCKET_LISTEN_SUPPORT

	// OpSocket external SSL methods
	virtual OP_STATUS UpgradeToSecure();
	virtual OP_STATUS SetSecureCiphers(const cipherentry_st* ciphers, UINT cipher_count);
	virtual OP_STATUS AcceptInsecureCertificate(OpCertificate* cert);
	virtual OP_STATUS GetCurrentCipher(cipherentry_st* used_cipher, BOOL* tls_v1_used, UINT32* pk_keysize);
	virtual OP_STATUS SetServerName(const uni_char* server_name);
	virtual OpCertificate* ExtractCertificate();
	virtual OpAutoVector<OpCertificate>* ExtractCertificateChain();
	virtual int GetSSLConnectErrors() { return m_ssl_cert_errors; }

public:
	// OpSocketListener methods
	virtual void OnSocketConnected(OpSocket* socket);
	virtual void OnSocketDataReady(OpSocket* socket);
	virtual void OnSocketDataSent(OpSocket* socket, UINT bytes_sent);
	virtual void OnSocketClosed(OpSocket* socket);
	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error error);
#ifdef SOCKET_LISTEN_SUPPORT
	virtual void OnSocketConnectionRequest(OpSocket* socket) { Unimplemented(); }
	virtual void OnSocketListenError(OpSocket* socket, OpSocket::Error error) { Unimplemented(); }
#endif // SOCKET_LISTEN_SUPPORT

public:
	// MessageObject methods
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	MH_PARAM_1 Id() const { return reinterpret_cast <MH_PARAM_1> (this); }
	OP_STATUS HandleOpenSSLReturnCode(int status);
	OP_STATUS HandleOpenSSLPositiveReturnCode(int status);
	OP_STATUS HandleOpenSSLNegativeReturnCode(int status, int err);
	void HandleOpenSSLBIOs();
	static void PrepareBuffer(char*  buf,     char*  buf_pos,      int  buf_len,
	                          char*& buf_end, char*& buf_free_pos, int& buf_free_len);
	void HandleOpenSSLRBIO();
	void HandleOpenSSLWBIO();
	void HandleOpenSSLNonBlocking();
	void UpdatePeerCert();
	void UpdatePeerCertVerification();
	void ClearSSL();

private:
	// Platform socket, listener
	OpSocket*         m_socket;
	OpSocketListener* m_listener;

	// Opera SSL related stuff
	bool                  m_secure;
	const cipherentry_st* m_ciphers;
	UINT                  m_cipher_count;
	OpString              m_server_name;
	int                   m_ssl_cert_errors;

	// OpenSSL related stuff
	// One SSL_CTX instance per program is sufficient. Should be moved out of the class.
	SSL_CTX* m_ssl_ctx;
	BIO*     m_rbio;
	BIO*     m_wbio;
	SSL*     m_ssl;
	X509*    m_peer_cert;

private:
	// States
	enum SSLState
	{
		SSL_STATE_NOSSL,
		SSL_STATE_HANDSHAKING,
		SSL_STATE_ASKING_USER,
		SSL_STATE_CONNECTED,
		SSL_STATE_READING,
		SSL_STATE_WRITING,
		SSL_STATE_SHUTTING_DOWN,
		// Unused state
		SSL_STATE_LAST
	};
	enum SSLState m_ssl_state;

	bool m_socket_data_ready;

	// Socket buffers
	// Buffer for reading from m_socket and writing to m_wbio
	char  m_rbuf[OPENSSL_SOCKET_BUFFER_SIZE]; /* ARRAY OK 2009-09-04 alexeik */
	// Buffer for writing to m_socket and reading from m_wbio
	char  m_wbuf[OPENSSL_SOCKET_BUFFER_SIZE]; /* ARRAY OK 2009-09-04 alexeik */
	// Current positions of useful data in buffers
	char* m_rbuf_pos;
	char* m_wbuf_pos;
	// Current lengths of useful data in buffers
	int   m_rbuf_len;
	int   m_wbuf_len;

private:
	void Unimplemented();
};

#endif // EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

#endif // OPENSSL_SOCKET_H
