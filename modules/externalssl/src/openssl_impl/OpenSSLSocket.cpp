/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OpenSSLSocket.cpp
 *
 * SSL-capable socket implementation.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#include "core/pch.h"

#ifdef EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

#include "modules/externalssl/src/openssl_impl/OpenSSLSocket.h"
#include "modules/externalssl/src/openssl_impl/OpenSSLCertificate.h"
#include "modules/externalssl/src/openssl_impl/OpenSSLLibrary.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/err.h"

#include "modules/url/url_socket_wrapper.h"

OpenSSLSocket::OpenSSLSocket(OpSocketListener* listener, BOOL secure)
	: m_socket(0)
	, m_listener(listener)
	, m_secure(secure)
	, m_ciphers(0)
	, m_cipher_count(0)
	, m_ssl_cert_errors(CERT_BAD_CERTIFICATE)
	, m_ssl_ctx(0)
	, m_rbio(0)
	, m_wbio(0)
	, m_ssl(0)
	, m_peer_cert(0)
	, m_ssl_state(SSL_STATE_NOSSL)
	, m_socket_data_ready(false)
	, m_rbuf_pos(m_rbuf)
	, m_wbuf_pos(m_wbuf)
	, m_rbuf_len(0)
	, m_wbuf_len(0)
{
}


OP_STATUS OpenSSLSocket::Init()
{
	// Create the platform socket.
	OP_STATUS status = SocketWrapper::CreateTCPSocket(&m_socket, this, SocketWrapper::ALLOW_ALL_WRAPPERS);
	RETURN_IF_ERROR(status);

	// Set message callbacks.
	g_main_message_handler->SetCallBack(this, MSG_OPENSSL_HANDLE_BIOS, Id());
	g_main_message_handler->SetCallBack(this, MSG_OPENSSL_HANDLE_NONBLOCKING, Id());

	// Get library.
	OP_ASSERT(g_opera);
	void* global_data = g_opera->externalssl_module.GetGlobalData();
	OP_ASSERT(global_data);
	OpenSSLLibrary* library = reinterpret_cast <OpenSSLLibrary*> (global_data);
	OP_ASSERT(library);

	// Get CTX.
	m_ssl_ctx = library->Get_SSL_CTX();
	OP_ASSERT(m_ssl_ctx);

	return OpStatus::OK;
}


OpenSSLSocket::~OpenSSLSocket()
{
	g_main_message_handler->UnsetCallBacks(this);

	if (m_ssl)
	{
		// These asserts must be changed to ifs if they become able to be triggered

		// If part of the expression starting with "||" is true -
		// it's probably a bug in OpenSSL.
		OP_ASSERT(SSL_get_wbio(m_ssl) == m_wbio || (m_ssl->bbio && m_ssl->bbio->next_bio == m_wbio));
		m_wbio = 0;

		OP_ASSERT(SSL_get_rbio(m_ssl) == m_rbio);
		m_rbio = 0;

		// Should free BIOs as well
		SSL_free(m_ssl);
		m_ssl = 0;
	}

	// Deallocation if m_ssl failed to be created
	if (m_wbio)
	{
		BIO_free(m_wbio);
		m_wbio = 0;
	}
	if (m_rbio)
	{
		BIO_free(m_rbio);
		m_rbio = 0;
	}

	// Will deallocate m_peer_cert.
	UpdatePeerCert();

	if (m_socket)
	{
		delete m_socket;
		m_socket = 0;
	}
}


OP_STATUS OpenSSLSocket::Connect(OpSocketAddress* socket_address)
{
	OP_ASSERT(m_socket);
	return m_socket->Connect(socket_address);
}


OP_STATUS OpenSSLSocket::Send(const void* data, UINT length)
{
	OP_ASSERT(m_socket);

	if (!m_secure)
		return m_socket->Send(data, length);

	if (!data)
		return OpStatus::ERR;

	OP_ASSERT(m_ssl);
	OP_ASSERT(m_listener);

	// No graceful handling so far when in other states.
	// Also prevents reentrancy.
	if (m_ssl_state != SSL_STATE_CONNECTED)
	{
		Unimplemented();
		return OpStatus::ERR;
	}

	m_ssl_state = SSL_STATE_WRITING;

	// SSL_write() takes signed int. Let's check for int overflow.
	// When calling SSL_write() with 0 bytes to be sent, the behaviour is undefined.
	OP_ASSERT((int)length > 0);
	
	ERR_clear_error();
	int status = SSL_write(m_ssl, data, length);
	OP_STATUS op_status = HandleOpenSSLReturnCode(status);

	if (status > 0)
	{
		OP_ASSERT(status == (int)length);
		m_listener->OnSocketDataSent(this, length);
	}

	return op_status;
}


OP_STATUS OpenSSLSocket::Recv(void* buffer, UINT length, UINT* bytes_received)
{
	OP_ASSERT(m_socket);

	if (!m_secure)
		return m_socket->Recv(buffer, length, bytes_received);

	if (!buffer || !bytes_received)
		return OpStatus::ERR;

	OP_ASSERT(m_ssl);

	// No graceful handling so far when in other states.
	// Also prevents reentrancy.
	if (m_ssl_state != SSL_STATE_CONNECTED)
	{
		Unimplemented();
		return OpStatus::ERR;
	}

	m_ssl_state = SSL_STATE_READING;

	// SSL_read() takes signed int. Let's check for int overflow.
	// When calling SSL_read() with 0 buffer size, the behaviour is undefined.
	OP_ASSERT((int)length > 0);
	
	ERR_clear_error();
	int status = SSL_read(m_ssl, buffer, length);
	OP_STATUS op_status = HandleOpenSSLReturnCode(status);

	if (status > 0)
		*bytes_received = status;
	else
		*bytes_received = 0;

	return op_status;
}


void OpenSSLSocket::Close()
{
	OP_ASSERT(m_socket);

	if (!m_secure)
	{
		m_socket->Close();
		return;
	}

	OP_ASSERT(m_ssl);

	m_ssl_state = SSL_STATE_SHUTTING_DOWN;
	g_main_message_handler->PostMessage(MSG_OPENSSL_HANDLE_NONBLOCKING, Id(), 0);
}


#ifdef OPSOCKET_PAUSE_DOWNLOAD
OP_STATUS OpenSSLSocket::PauseRecv()
{
	OP_ASSERT(m_socket);
	return m_socket->PauseRecv();
}


OP_STATUS OpenSSLSocket::ContinueRecv()
{
	OP_ASSERT(m_socket);
	return m_socket->ContinueRecv();
}
#endif // OPSOCKET_PAUSE_DOWNLOAD


OP_STATUS OpenSSLSocket::GetSocketAddress(OpSocketAddress* socket_address)
{
	OP_ASSERT(m_socket);
	return m_socket->GetSocketAddress(socket_address);
}


#ifdef OPSOCKET_OPTIONS
OP_STATUS OpenSSLSocket::SetSocketOption(OpSocketBoolOption option, BOOL value)
{
	OP_ASSERT(m_socket);
	return m_socket->SetSocketOption(option, value);
}


OP_STATUS OpenSSLSocket::GetSocketOption(OpSocketBoolOption option, BOOL& out_value)
{
	OP_ASSERT(m_socket);
	return m_socket->GetSocketOption(option, out_value);
}
#endif // OPSOCKET_OPTIONS


OP_STATUS OpenSSLSocket::UpgradeToSecure()
{
	OP_ASSERT(m_ssl_ctx);

	m_secure = TRUE;

	// RBIO
	if (!m_rbio)
		m_rbio = BIO_new(BIO_s_mem());
	if (!m_rbio)
		return OpStatus::ERR;

	// WBIO
	if (!m_wbio)
		m_wbio = BIO_new(BIO_s_mem());
	if (!m_wbio)
		return OpStatus::ERR;

	// SSL
	if (!m_ssl)
		m_ssl = SSL_new(m_ssl_ctx);
	if (!m_ssl)
		return OpStatus::ERR;

	// Clear state if the socket is reused, no-op otherwise
	ClearSSL();

	// Connect objects to each other
	SSL_set_bio(m_ssl, m_rbio, m_wbio);

	// Handshaking
	SSL_set_connect_state(m_ssl);
	m_ssl_state = SSL_STATE_HANDSHAKING;
	g_main_message_handler->PostMessage(MSG_OPENSSL_HANDLE_NONBLOCKING, Id(), 0);

	return OpStatus::OK;
}


OP_STATUS OpenSSLSocket::SetSecureCiphers(const cipherentry_st* ciphers, UINT cipher_count)
{
	m_ciphers = ciphers;
	m_cipher_count = cipher_count;
	return OpStatus::OK;
}

OP_STATUS OpenSSLSocket::AcceptInsecureCertificate(OpCertificate* cert)
{
	//Force hitting the breakpoint.
	Unimplemented();
	
	if (m_ssl_state != SSL_STATE_ASKING_USER || !cert || !m_peer_cert)
		return OpStatus::ERR;

	unsigned int length;
	const char* cert_hash = cert->GetCertificateHash(length);
	OP_ASSERT(cert_hash);
	OP_ASSERT(length == SHA_DIGEST_LENGTH);

	// Compare hashes.
	int res = op_memcmp(cert_hash, m_peer_cert->sha1_hash, SHA_DIGEST_LENGTH);

	// Main decision.
	if (res == 0)
	{
		m_ssl_state = SSL_STATE_CONNECTED;
		return OpStatus::OK;
	}
	else
	{
		m_ssl_cert_errors = CERT_DIFFERENT_CERTIFICATE;
		return OpStatus::ERR;
	}
}


OP_STATUS OpenSSLSocket::GetCurrentCipher(cipherentry_st* used_cipher, BOOL* tls_v1_used, UINT32* pk_keysize)
{
	if (!used_cipher || !tls_v1_used || !pk_keysize || !m_ciphers)
		return OpStatus::ERR_NULL_POINTER;

	if (m_ssl_state != SSL_STATE_CONNECTED)
		return OpStatus::ERR;

	OP_ASSERT(m_cipher_count > 0);
	OP_ASSERT(m_secure);
	OP_ASSERT(m_ssl);

	const SSL_CIPHER* ssl_cipher = SSL_get_current_cipher(m_ssl);
	OP_ASSERT(ssl_cipher);
	// m_ssl owns ssl_cipher.

	// Get 2-byte cipher id
	cipherentry_st id;
	id.id[0] = (ssl_cipher->id >> 8) & 0xFF;
	id.id[1] =  ssl_cipher->id       & 0xFF;

	// Search for the used cipher
	UINT cipher_num = 0;
	for (; cipher_num < m_cipher_count; cipher_num++)
	{
		const cipherentry_st& cipher = m_ciphers[cipher_num];
		if (cipher.id[0] == id.id[0] && cipher.id[1] == id.id[1])
		{
			// Found
			used_cipher->id[0] = id.id[0];
			used_cipher->id[1] = id.id[1];
			break;
		}
	}

	// Cipher is unknown - set used_cipher to zeros
	if (cipher_num == m_cipher_count)
	{
		used_cipher->id[0] = 0;
		used_cipher->id[1] = 0;
	}

	// Detect if TLSv1 is used
	const char* ssl_protocol_version = SSL_get_version(m_ssl);
	OP_ASSERT(ssl_protocol_version);
	// ssl_protocol_version is not owned, it's a pointer to a string literal.
	
	if (!op_strcmp(ssl_protocol_version, "TLSv1"))
		*tls_v1_used = TRUE;
	else
		*tls_v1_used = FALSE;

	// Detect key size
	*pk_keysize = 0;

	// m_peer_cert should be filled now because UpdatePeerCert()
	// should have been called just after the handshake.
	if (m_peer_cert)
	{
		EVP_PKEY* peer_pubkey = X509_get_pubkey(m_peer_cert);
		if (peer_pubkey)
		{
			// We own peer_pubkey.
			// Get public key size in bits.
			*pk_keysize = EVP_PKEY_bits(peer_pubkey);
			EVP_PKEY_free(peer_pubkey);
		}
	}

	return OpStatus::OK;
}


OP_STATUS OpenSSLSocket::SetServerName(const uni_char* server_name)
{
	OP_STATUS status = m_server_name.Set(server_name);
	return status;
}


OpCertificate* OpenSSLSocket::ExtractCertificate()
{
	// Will gracefully fail and return 0 if m_peer_cert == 0.
	OpCertificate* op_cert = OpenSSLCertificate::Create(m_peer_cert);
	
	if (!op_cert)
		m_ssl_cert_errors = CERT_BAD_CERTIFICATE;

	return op_cert;
}


OpAutoVector<OpCertificate>* OpenSSLSocket::ExtractCertificateChain()
{
	if (!m_ssl)
		return 0;

	// Get certificate chain.
	STACK_OF(X509)* x509_stack = SSL_get_peer_cert_chain(m_ssl);
	if (!x509_stack)
		return 0;
	// m_ssl owns x509_stack.

	int x509_count = sk_X509_num(x509_stack);
	OP_ASSERT(x509_count >= 0);

	OpAutoVector<OpCertificate>* chain = OP_NEW(OpAutoVector<OpCertificate>, (x509_count));
	if (!chain)
		return 0;

	for (int i = 0; i < x509_count; i++)
	{
		X509* x509 = sk_X509_value(x509_stack, i);
		OP_ASSERT(x509);
		// x509_stack owns x509.

		OpCertificate* op_cert = OpenSSLCertificate::Create(x509);
		if (!op_cert)
		{
			OP_DELETE(chain);
			return 0;
		}

		OP_STATUS status = chain->Add(op_cert);
		if (status != OpStatus::OK)
		{
			OP_DELETE(op_cert);
			OP_DELETE(chain);
			return 0;
		}
	}

	return chain;
}


void OpenSSLSocket::OnSocketConnected(OpSocket* socket)
{
	OP_ASSERT(socket == m_socket);
	OP_ASSERT(m_listener);

	if (!m_secure)
	{
		m_listener->OnSocketConnected(this);
		return;
	}

	UpgradeToSecure();
}


void OpenSSLSocket::OnSocketDataReady(OpSocket* socket)
{
	OP_ASSERT(socket == m_socket);
	OP_ASSERT(m_listener);

	if (!m_secure)
	{
		m_listener->OnSocketDataReady(this);
		return;
	}

	m_socket_data_ready = true;
	g_main_message_handler->PostMessage(MSG_OPENSSL_HANDLE_BIOS, Id(), 0);
}


void OpenSSLSocket::OnSocketDataSent(OpSocket* socket, UINT bytes_sent)
{
	OP_ASSERT(socket == m_socket);
	OP_ASSERT(m_listener);

	if (!m_secure)
	{
		m_listener->OnSocketDataSent(this, bytes_sent);
		return;
	}
	
	switch (m_ssl_state)
	{
		case SSL_STATE_HANDSHAKING:
		case SSL_STATE_SHUTTING_DOWN:
			// Do nothing
			break;

		case SSL_STATE_CONNECTED:
			// 0 because the amount of data is already accounted
			// in OpenSSLSocket::Send()
			m_listener->OnSocketDataSent(this, 0);
			g_main_message_handler->PostMessage(MSG_OPENSSL_HANDLE_BIOS, Id(), 0);
			break;

		default:
			// Shouldn't happen
			Unimplemented();
	}

	return;
}


void OpenSSLSocket::OnSocketClosed(OpSocket* socket)
{
	OP_ASSERT(socket == m_socket);
	OP_ASSERT(m_listener);

	if (m_secure)
		ClearSSL();

	m_listener->OnSocketClosed(this);
}


void OpenSSLSocket::OnSocketConnectError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(socket == m_socket);
	OP_ASSERT(m_listener);
	OP_ASSERT(m_ssl_state == SSL_STATE_NOSSL);

	m_listener->OnSocketConnectError(this, error);
}


void OpenSSLSocket::OnSocketReceiveError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(socket == m_socket);
	OP_ASSERT(m_listener);

	m_listener->OnSocketReceiveError(this, error);
}


void OpenSSLSocket::OnSocketSendError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(socket == m_socket);
	OP_ASSERT(m_listener);

	m_listener->OnSocketSendError(this, error);
}


void OpenSSLSocket::OnSocketCloseError(OpSocket* socket, OpSocket::Error error)
{
	OP_ASSERT(socket == m_socket);
	OP_ASSERT(m_listener);

	m_listener->OnSocketCloseError(this, error);
}


void OpenSSLSocket::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
		case MSG_OPENSSL_HANDLE_BIOS:
			HandleOpenSSLBIOs();
			break;

		case MSG_OPENSSL_HANDLE_NONBLOCKING:
			HandleOpenSSLNonBlocking();
			break;

		default:
			Unimplemented();
	}
}


OP_STATUS OpenSSLSocket::HandleOpenSSLReturnCode(int status)
{
	OP_ASSERT(m_ssl);

	int err = SSL_get_error(m_ssl, status);
	OP_STATUS opst = OpStatus::ERR;

	if (status > 0)
	{
		// Something good
		OP_ASSERT(err == SSL_ERROR_NONE);
		opst = HandleOpenSSLPositiveReturnCode(status);
	}
	else
	{
		// Something bad or operation in progress
		OP_ASSERT(err != SSL_ERROR_NONE);
		opst = HandleOpenSSLNegativeReturnCode(status, err);
	}

	ERR_clear_error();
	return opst;
}


OP_STATUS OpenSSLSocket::HandleOpenSSLPositiveReturnCode(int status)
{
	OP_ASSERT(status > 0);
	OP_ASSERT(g_main_message_handler);
	OP_ASSERT(m_listener);
	OP_ASSERT(m_socket);
	OP_ASSERT(m_ssl);

	switch (m_ssl_state)
	{
		case SSL_STATE_HANDSHAKING:
			OP_ASSERT(status == 1);
			// It will also get the verification info.
			UpdatePeerCert();

			// Based on the verification info, decide if handshaking
			// can be finished.
			{
				if (m_ssl_cert_errors == SSL_NO_ERROR)
				{
					m_ssl_state = SSL_STATE_CONNECTED;
					m_listener->OnSocketConnected(this);
				}
				else
				{
					m_ssl_state = SSL_STATE_ASKING_USER;
					m_listener->OnSocketConnectError(this, SECURE_CONNECTION_FAILED);
				}
			}
			break;

		case SSL_STATE_READING:
		case SSL_STATE_WRITING:
			g_main_message_handler->PostMessage(MSG_OPENSSL_HANDLE_BIOS, Id(), 0);
			m_ssl_state = SSL_STATE_CONNECTED;
			break;

		case SSL_STATE_SHUTTING_DOWN:
			OP_ASSERT(status == 1);
			m_ssl_state = SSL_STATE_NOSSL;
			UpdatePeerCert();
			m_socket->Close();
			break;

		default:
			// Shouldn't happen
			Unimplemented();
	}

	return OpStatus::OK;
}


OP_STATUS OpenSSLSocket::HandleOpenSSLNegativeReturnCode(int status, int err)
{
	OP_ASSERT(status <= 0);
	OP_ASSERT(g_main_message_handler);
	OP_ASSERT(m_listener);
	OP_ASSERT(m_socket);
	OP_ASSERT(m_ssl);

	switch (err)
	{
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		{
			// Need an operation on a non-blocking socket
			g_main_message_handler->PostMessage(MSG_OPENSSL_HANDLE_BIOS, Id(), 0);

			switch (m_ssl_state)
			{
				case SSL_STATE_HANDSHAKING:
				case SSL_STATE_SHUTTING_DOWN:
					// Do nothing, let BIOs be handled
					break;

				case SSL_STATE_READING:
					m_listener->OnSocketReceiveError(this, SOCKET_BLOCKING);
					m_ssl_state = SSL_STATE_CONNECTED;
					break;

				case SSL_STATE_WRITING:
					m_listener->OnSocketSendError(this, SOCKET_BLOCKING);
					m_ssl_state = SSL_STATE_CONNECTED;
					break;

				default:
					// Shouldn't happen
					Unimplemented();
			}
			break;
		}

		// Graceful SSL shutdown
		case SSL_ERROR_ZERO_RETURN:
		{
			OP_ASSERT(status == 0);

			switch (m_ssl_state)
			{
				case SSL_STATE_HANDSHAKING:
					m_listener->OnSocketConnectError(this, SECURE_CONNECTION_FAILED);
					break;

				case SSL_STATE_READING:
					m_listener->OnSocketReceiveError(this, SOCKET_BLOCKING);
					break;

				default:
					// Shouldn't happen
					Unimplemented();
			}
			m_ssl_state = SSL_STATE_SHUTTING_DOWN;
			g_main_message_handler->PostMessage(MSG_OPENSSL_HANDLE_NONBLOCKING, Id(), 0);
			break;
		}

		case SSL_ERROR_SSL:
		case SSL_ERROR_SYSCALL:
		{
			switch (m_ssl_state)
			{
				case SSL_STATE_HANDSHAKING:
					m_listener->OnSocketConnectError(this, SECURE_CONNECTION_FAILED);
					break;

				case SSL_STATE_READING:
					m_listener->OnSocketReceiveError(this, NETWORK_ERROR);
					break;

				case SSL_STATE_WRITING:
					m_listener->OnSocketSendError(this, NETWORK_ERROR);
					break;

				case SSL_STATE_SHUTTING_DOWN:
					// Do nothing, ignore unsuccessful SSL shutdown
					break;

				default:
					// Shouldn't happen
					Unimplemented();
			}

			// Do not leak resources - do abortive close
			m_socket->Close();
			break;
		}

		default:
		{
			// An unhandled error occured
			Unimplemented();

			switch (m_ssl_state)
			{
				// Do not leak resources - do abortive close
				case SSL_STATE_SHUTTING_DOWN:
					m_socket->Close();
					break;
			}
		}
	}

	return OpStatus::ERR;
}


void OpenSSLSocket::HandleOpenSSLBIOs()
{
	HandleOpenSSLRBIO();
	HandleOpenSSLWBIO();

	OP_ASSERT(m_rbio);
	int rbio_pending = BIO_pending(m_rbio);

	if (m_ssl_state == SSL_STATE_CONNECTED && rbio_pending > 0)
		m_listener->OnSocketDataReady(this);
}


void OpenSSLSocket::PrepareBuffer(char*  buf,     char*  buf_pos,      int  buf_len,
                                  char*& buf_end, char*& buf_free_pos, int& buf_free_len)
{
	OP_ASSERT(buf);
	OP_ASSERT(buf_pos);
	OP_ASSERT(buf_pos >= buf);
	OP_ASSERT(buf_len >= 0);
	
	// Pointer to the first byte after the buffer
	buf_end = buf + OPENSSL_SOCKET_BUFFER_SIZE;
	OP_ASSERT(buf_pos <= buf_end);

	// Pointer to the first "free" byte of the buffer
	buf_free_pos = buf_pos + buf_len;
	OP_ASSERT(buf_free_pos <= buf_end);

	// Length of "free" buffer space
	buf_free_len = buf_end - buf_free_pos;
	OP_ASSERT(buf_free_len >= 0);
}


void OpenSSLSocket::HandleOpenSSLRBIO()
{
	OP_ASSERT(m_rbio);
	OP_ASSERT(m_socket);

	if (m_socket_data_ready)
	{
		// Prepare buffer
		char* rbuf_end = 0;
		char* rbuf_free_pos = 0;
		int rbuf_free_len = 0;
		PrepareBuffer(m_rbuf, m_rbuf_pos, m_rbuf_len, rbuf_end, rbuf_free_pos, rbuf_free_len);

		if (rbuf_free_len > 0)
		{
			UINT bytes_read = 0;
			// Read from socket
			OP_STATUS status = m_socket->Recv(rbuf_free_pos, rbuf_free_len, &bytes_read);
			if (status == OpStatus::OK)
			{
				if (bytes_read > 0)
					m_rbuf_len += bytes_read;
				else
					m_socket_data_ready = false;
			}
			OP_ASSERT(m_rbuf_pos + m_rbuf_len == rbuf_free_pos + bytes_read);
		}
		OP_ASSERT(m_rbuf_pos + m_rbuf_len <= rbuf_end);

		if (m_rbuf_len > 0)
		{
			// Buffer space available
			// Write to BIO
			int bytes_written = BIO_write(m_rbio, m_rbuf_pos, m_rbuf_len);
			OP_ASSERT(bytes_written <= m_rbuf_len);
			
			if (bytes_written > 0)
			{
				m_rbuf_pos += bytes_written;
				m_rbuf_len -= bytes_written;
				OP_ASSERT(m_rbuf_len >= 0);
				OP_ASSERT(m_rbuf_pos + m_rbuf_len <= rbuf_end);

				if (m_rbuf_len == 0)
					// Buffer is empty
					m_rbuf_pos = m_rbuf;

				g_main_message_handler->PostMessage(
					MSG_OPENSSL_HANDLE_NONBLOCKING, Id(), 0);

				m_listener->OnSocketDataReady(this);
			}
		}
	}
}


void OpenSSLSocket::HandleOpenSSLWBIO()
{
	OP_ASSERT(m_wbio);
	OP_ASSERT(m_socket);
	
	int wbio_pending = BIO_pending(m_wbio);
	if (wbio_pending > 0)
	{
		// Prepare buffer
		char* wbuf_end = 0;
		char* wbuf_free_pos = 0;
		int wbuf_free_len = 0;
		PrepareBuffer(m_wbuf, m_wbuf_pos, m_wbuf_len, wbuf_end, wbuf_free_pos, wbuf_free_len);

		if (wbuf_free_len > 0)
		{
			// Buffer space available
			if (wbio_pending > wbuf_free_len)
				wbio_pending = wbuf_free_len;

			// Read from BIO
			int bytes_read = BIO_read(m_wbio, wbuf_free_pos, wbio_pending);
			OP_ASSERT(bytes_read <= wbio_pending);
			
			if (bytes_read > 0)
				m_wbuf_len += bytes_read;

			OP_ASSERT(m_wbuf_pos + m_wbuf_len == wbuf_free_pos + bytes_read);
		}
		OP_ASSERT(m_wbuf_pos + m_wbuf_len <= wbuf_end);

		if (m_wbuf_len)
		{
			// Write to socket
			OP_STATUS status = m_socket->Send(m_wbuf_pos, m_wbuf_len);
			if (status == OpStatus::OK)
			{
				// Buffer is empty
				m_wbuf_pos = m_wbuf;
				m_wbuf_len = 0;

				g_main_message_handler->PostMessage(
					MSG_OPENSSL_HANDLE_NONBLOCKING, Id(), 0);
			}
		}
	}
}


void OpenSSLSocket::HandleOpenSSLNonBlocking()
{
	OP_ASSERT(m_ssl);

	switch (m_ssl_state)
	{

		case SSL_STATE_HANDSHAKING:
		{
			ERR_clear_error();
			int status = SSL_do_handshake(m_ssl);
			HandleOpenSSLReturnCode(status);
			break;
		}

		case SSL_STATE_NOSSL:
		case SSL_STATE_CONNECTED:
		case SSL_STATE_READING:
		case SSL_STATE_WRITING:
			// Do nothing
			break;

		case SSL_STATE_SHUTTING_DOWN:
		{
			ERR_clear_error();
			int status = SSL_shutdown(m_ssl);
			HandleOpenSSLReturnCode(status);
			break;
		}

		default:
			Unimplemented();
	}
}


void OpenSSLSocket::UpdatePeerCert()
{
	// This function relies on the fact that the peer certificate pointer
	// inside m_ssl remains unchanged for the whole time of SSL/TLS session.
	
	X509* peer_cert = 0;
	if(m_ssl)
		peer_cert = SSL_get_peer_certificate(m_ssl);

	if (peer_cert == m_peer_cert)
		// Already up-to-date, including the case when both pointers are 0.
		return;

	if (m_peer_cert)
		// m_peer_cert is a leftover from the previous SSL session.
		X509_free(m_peer_cert);

	// Updating the pointer. Correct behaviour in (peer_cert == 0) case.
	m_peer_cert = peer_cert;
	// Set to CERT_BAD_CERTIFICATE until the opposite is proven.
	m_ssl_cert_errors = CERT_BAD_CERTIFICATE;
	
	if (!m_peer_cert)
		return;

	// Update certificate hash.
	unsigned int len = 0;
	int res = X509_digest(m_peer_cert, EVP_sha1(), m_peer_cert->sha1_hash, &len);
	if (res == 0)
	{
		// Hash calculation failure. It's probably an OOM problem.
		// In this case it will probably not be enough memory
		// for normal further work. Cert comparison can't work.
		// This case is very rare.
		m_peer_cert = 0;
		return;
	}
	// Hash calculation successful.
	OP_ASSERT(res == 1);
	OP_ASSERT(len == SHA_DIGEST_LENGTH);

	UpdatePeerCertVerification();
}


void OpenSSLSocket::UpdatePeerCertVerification()
{
	// Hack: always allow the connection, because the dialog asking for
	// bad certificate confirmation is still not implemented.
	// Ifdef out these lines if you want to perform certificate verification.
#if 1
	m_ssl_cert_errors = SSL_NO_ERROR;
	return;
#endif

	// Called only from UpdatePeerCert(). Thus the following must be true:
	OP_ASSERT(m_ssl);
	OP_ASSERT(m_peer_cert);
	OP_ASSERT(m_ssl_cert_errors == CERT_BAD_CERTIFICATE);

	// Check cert name.
	// Get subject from cert.
	X509_NAME* x509_name = X509_get_subject_name(m_peer_cert);
	OP_ASSERT(x509_name);
	// m_peer_cert owns x509_name.

	// Max hostname length is 255, according to RFC 2181.
	const int MAX_HOSTNAME_LENGTH = 255;
	char data[MAX_HOSTNAME_LENGTH + 1]; /* ARRAY OK 2009-10-05 alexeik */

	// Get CN from subject.
	int res = X509_NAME_get_text_by_NID(x509_name, NID_commonName, data, MAX_HOSTNAME_LENGTH + 1);
	if (res <= 0)
		// No CN or empty CN.
		return;

	// Compare server name with CN.
	OP_ASSERT(m_server_name.HasContent());
	OP_ASSERT(res <= MAX_HOSTNAME_LENGTH);
	if (m_server_name.CompareI(data, res) != 0)
		// CN is not the same as the server name.
		return;

	// Get verification result and update m_ssl_cert_errors.
	long verify_result = SSL_get_verify_result(m_ssl);
	switch (verify_result)
	{
		case X509_V_OK:
		// The following 7 codes are unused according to man verify(1ssl).
		// These are CRL issues. They do not necessary invalidate the certificate.
		case X509_V_ERR_UNABLE_TO_GET_CRL:
		case X509_V_ERR_CRL_SIGNATURE_FAILURE:
		case X509_V_ERR_UNABLE_TO_DECRYPT_CRL_SIGNATURE:
		case X509_V_ERR_CRL_NOT_YET_VALID:
		case X509_V_ERR_CRL_HAS_EXPIRED:
		case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
		case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
			m_ssl_cert_errors = SSL_NO_ERROR;
			break;

		case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
		case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY:
		case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
		case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
		case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
		case X509_V_ERR_CERT_CHAIN_TOO_LONG:
		case X509_V_ERR_INVALID_CA:
		case X509_V_ERR_CERT_UNTRUSTED:
		case X509_V_ERR_CERT_REJECTED:
		case X509_V_ERR_SUBJECT_ISSUER_MISMATCH:
		case X509_V_ERR_AKID_SKID_MISMATCH:
		case X509_V_ERR_AKID_ISSUER_SERIAL_MISMATCH:
		case X509_V_ERR_KEYUSAGE_NO_CERTSIGN:
			m_ssl_cert_errors = CERT_INCOMPLETE_CHAIN;
			break;

		case X509_V_ERR_CERT_NOT_YET_VALID:
		case X509_V_ERR_CERT_HAS_EXPIRED:
		case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
		case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
			m_ssl_cert_errors = CERT_EXPIRED;
			break;

		case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE:
		case X509_V_ERR_CERT_SIGNATURE_FAILURE:
		case X509_V_ERR_OUT_OF_MEM:
		case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
		case X509_V_ERR_CERT_REVOKED:
		case X509_V_ERR_PATH_LENGTH_EXCEEDED:
		case X509_V_ERR_INVALID_PURPOSE:
		case X509_V_ERR_APPLICATION_VERIFICATION:
		default:
			// Already set.
			OP_ASSERT(m_ssl_cert_errors == CERT_BAD_CERTIFICATE);
			//m_ssl_cert_errors = CERT_BAD_CERTIFICATE;
			break;
	}
}


void OpenSSLSocket::ClearSSL()
{
	OP_ASSERT(m_secure);
	OP_ASSERT(m_ssl);
	OP_ASSERT(m_rbio);
	OP_ASSERT(m_wbio);

	int err = SSL_clear(m_ssl);
	OP_ASSERT(err == 1);
	err = BIO_reset(m_rbio);
	OP_ASSERT(err == 1);
	err = BIO_reset(m_wbio);
	OP_ASSERT(err == 1);
	// Suppress warning about unused variable.
	(void)err;

	m_rbuf_len = 0;
	m_wbuf_len = 0;
	m_ssl_state = SSL_STATE_NOSSL;
	UpdatePeerCert();
}


void OpenSSLSocket::Unimplemented()
{
	// Set breakpoint here
	OP_ASSERT(!"OpenSSLSocket::Unimplemented() called! Get the stack trace and see what's not implemented.");
}

#endif // EXTERNAL_SSL_OPENSSL_IMPLEMENTATION
