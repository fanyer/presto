/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Roar Lauritzsen
**
*/

#ifndef ZLIB_TRANSCEIVE_SOCKET_H
#define ZLIB_TRANSCEIVE_SOCKET_H

#ifdef _EMBEDDED_BYTEMOBILE

#include "modules/pi/network/OpSocket.h"
#include "modules/hardcore/mh/messobj.h"

#if !defined(USE_ZLIB)
#error "For EBO compression to work, USE_ZLIB must be enabled"
#endif
#include "modules/zlib/zlib.h"
#include "modules/zlib/zconf.h"

#define ZTS_SECURE
#define ZTS_SECURE2

/** Zlib transceiving wrapper class for OpSocket.
 * 
 * This class can be wrapped around an OpSocket to perform zlib compression and
 * decompression on socket traffic.
 */
class ZlibTransceiveSocket : public OpSocket, public OpSocketListener, public MessageObject
{
public:
	/** Create a ZlibTransceiveSocket object wrapping an existing OpSocket.
	 *
	 * @param new_socket (out) Set to the new OpSocket object. May be aliased
	 *     to &wrapped_socket. In case of error, the value is unchanged.
	 * @param wrapped_socket The normal OpSocket that will handle actual I/O.
	 * @param listener The listener of wrapped_socket. Socket listening will
	 *     instead pass through the new ZlibTransceiveSocket. The listener
	 *     will receive the new ZlibTransceiveSocket as the "socket" parameter
	 *     of the callbacks.
	 * @param buffer_size The size to allocate for the receive decoding buffer
	 * @param transfer_ownership If TRUE, on success of this call the
	 *     ownership of wrapped_socket is transferred to the new
	 *     ZlibTransceiveSocket, which becomes responsible for deallocating
	 *     it in the destructor.
	 * @return OK, ERR or ERR_NO_MEMORY.
	 */
	static OP_STATUS Create(OpSocket** new_socket,
							OpSocket* wrapped_socket,
							OpSocketListener* listener,
							unsigned buffer_size,
							BOOL tranfer_ownership = TRUE);

	~ZlibTransceiveSocket();

	OP_STATUS Send(const void* data, UINT length    ZTS_SECURE);
	OP_STATUS Recv(void* buffer, UINT length, UINT* bytes_received    ZTS_SECURE);

	// Delegating the rest to m_socket
	OP_STATUS Connect(OpSocketAddress* socket_address    ZTS_SECURE) { return m_socket->Connect(socket_address    ZTS_SECURE2); }
	void Close() { m_socket->Close(); }
	OP_STATUS GetSocketAddress(OpSocketAddress* socket_address) { return m_socket->GetSocketAddress(socket_address); }
#ifdef OPSOCKET_GETLOCALSOCKETADDR
	OP_STATUS GetLocalSocketAddress(OpSocketAddress* socket_address) { return m_socket->GetLocalSocketAddress(socket_address); }
#endif // OPSOCKET_GETLOCALSOCKETADDR
	void SetListener(OpSocketListener* listener) { OP_ASSERT(listener); m_listener = listener; }
#ifdef SOCKET_LISTEN_SUPPORT
	OP_STATUS Listen(OpSocketAddress* socket_address, int queue_size) { return m_socket->Listen(socket_address, queue_size); }
	OP_STATUS Accept(OpSocket* socket    ZTS_SECURE) { return m_socket->Accept(socket    ZTS_SECURE2); }
#endif // SOCKET_LISTEN_SUPPORT
#ifdef _EXTERNAL_SSL_SUPPORT_
	OP_STATUS UpgradeToSecure() { return m_socket->UpgradeToSecure(); }
	OP_STATUS SetSecureCiphers(const cipherentry_st* ciphers, UINT cipher_count) { return m_socket->SetSecureCiphers(ciphers, cipher_count); }
	OP_STATUS AcceptInsecureCertificate(OpCertificate *certificate) { return m_socket->AcceptInsecureCertificate(certificate); }
	OP_STATUS GetCurrentCipher(cipherentry_st* used_cipher, BOOL* tls_v1_used, UINT32* pk_keysize) { return m_socket->GetCurrentCipher(used_cipher, tls_v1_used, pk_keysize); }
	OP_STATUS SetServerName(const uni_char* server_name) { return m_socket->SetServerName(server_name); }
	OpCertificate* ExtractCertificate() { return m_socket->ExtractCertificate(); }
	OpAutoVector<OpCertificate>* ExtractCertificateChain() { return m_socket->ExtractCertificateChain(); }
	int GetSSLConnectErrors() { return m_socket->GetSSLConnectErrors(); }
#endif // _EXTERNAL_SSL_SUPPORT_
#ifdef SOCKET_NEED_ABORTIVE_CLOSE
	void SetAbortiveClose() { m_socket->SetAbortiveClose(); }
#endif // SOCKET_NEED_ABORTIVE_CLOSE
#ifdef SOCKET_SUPPORTS_TIMER
	void SetTimeOutInterval(unsigned int seconds, BOOL restore_default = FALSE) { m_socket->SetTimeOutInterval(seconds, restore_default); }
#endif // SOCKET_SUPPORTS_TIMER
#ifdef OPSOCKET_PAUSE_DOWNLOAD
	OP_STATUS PauseRecv() { return m_socket->PauseRecv(); }
	OP_STATUS ContinueRecv() { return m_socket->ContinueRecv(); }
#endif // OPSOCKET_PAUSE_DOWNLOAD

#ifdef OPSOCKET_OPTIONS
	virtual OP_STATUS SetSocketOption(OpSocketBoolOption option, BOOL value) { return m_socket ? m_socket->SetSocketOption(option, value) : OpStatus::ERR; }
	virtual OP_STATUS GetSocketOption(OpSocketBoolOption option, BOOL& out_value) { return m_socket ? m_socket->GetSocketOption(option, out_value) : OpStatus::ERR; }
#endif // OPSOCKET_OPTIONS

#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
	void SetIsFileDownload(BOOL is_download) { m_socket->SetIsFileDownload(is_download); }
#endif

private:
	ZlibTransceiveSocket();
	ZlibTransceiveSocket(OpSocket*, OpSocketListener*, char*, unsigned, z_stream&, z_stream&, BOOL);

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void OnSocketClosed(OpSocket* socket);
	void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);

	// Delegating the rest to m_listener
	void OnSocketConnected(OpSocket* socket) { m_listener->OnSocketConnected(this); }
	void OnSocketDataReady(OpSocket* socket) { m_listener->OnSocketDataReady(this); }
	void OnSocketDataSent(OpSocket* socket, UINT bytes_sent) { m_listener->OnSocketDataSent(this, bytes_sent); }
#ifdef SOCKET_LISTEN_SUPPORT
	void OnSocketConnectionRequest(OpSocket* socket) { m_listener->OnSocketConnectionRequest(this); }
	void OnSocketListenError(OpSocket* socket, OpSocket::Error error) { m_listener->OnSocketListenError(this, error); }
#endif // SOCKET_LISTEN_SUPPORT
	void OnSocketConnectError(OpSocket* socket, OpSocket::Error error) { m_listener->OnSocketConnectError(this, error); }
	void OnSocketSendError(OpSocket* socket, OpSocket::Error error) { m_listener->OnSocketSendError(this, error); }
	void OnSocketCloseError(OpSocket* socket, OpSocket::Error error) { m_listener->OnSocketCloseError(this, error); }
#ifndef URL_CAP_TRUST_ONSOCKETDATASENT
	void OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent) { m_listener->OnSocketDataSendProgressUpdate(this, bytes_sent); }
#endif // !URL_CAP_TRUST_ONSOCKETDATASENT
	
	OP_STATUS DoInflate(int& inflate_status);

	OpSocket*			m_socket;
	OpSocketListener*	m_listener;
	const BOOL			m_owns_socket;
	char*				m_recv_buffer;
	const unsigned		m_recv_buffer_size;
	char*				m_recv_buffer_rest;
	unsigned			m_recv_buffer_rest_len;
	int					m_recv_buffer_read_number;
	z_stream			m_recv_stream;
	z_stream			m_send_stream;
	BOOL				m_socket_blocking;

	BOOL				m_pending_closed;
	OpSocket::Error		m_pending_receive_error;
	BOOL				m_pending_on_socket_data_ready;
	BOOL				m_has_pending_data;
	Bytef				m_pending_data;
};

#undef ZTS_SECURE
#undef ZTS_SECURE2

#endif // _EMBEDDED_BYTEMOBILE
#endif // _ZLIB_TRANSCEIVE_SOCKET
