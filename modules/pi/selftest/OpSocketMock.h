/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_PI_SELFTEST_OPSOCKETMOCK_H
#define MODULES_PI_SELFTEST_OPSOCKETMOCK_H
#ifdef SELFTEST
#include "modules/pi/network/OpSocket.h"
#include "modules/hardcore/base/opstatus.h"
#include "modules/opdata/UniString.h"


/** A mock implementation of OpSocket for testing classes that
 * use this interface.
 *
 * May be incomplete, feel free to implement more faked/mocked methods.
 **/
class OpSocketMock : public OpSocket
{
public:
	OpSocketMock() :
		connectCalls(0), connectSocketAddress(NULL), connectReturn(OpStatus::OK),
		closeCalls(0),
		sendCalls(0), sendLastData(), sendLastLen(0), sendReturn(OpStatus::OK)
	{}

	virtual OP_STATUS Connect(OpSocketAddress* socket_address)
	{
		connectCalls++;
		connectSocketAddress = socket_address;
		return connectReturn;
	}

	virtual void Close()
	{
		closeCalls++;
	}

	virtual OP_STATUS Send(const void* data, UINT length)
	{
		sendCalls++;
		RETURN_IF_ERROR(sendLastData.SetCopyData((uni_char*) data, length/sizeof(uni_char)));
		sendLastLen = length;
		return sendReturn;
	}

	virtual void SetListener(OpSocketListener* listenerPtr)
	{
		listener = listenerPtr;
	}

	// Implement these functions when needed.
	virtual OP_STATUS Recv(void* buffer, UINT length, UINT* bytes_received) { return OpStatus::OK; }
	virtual OP_STATUS GetSocketAddress(OpSocketAddress* socket_address) { return OpStatus::OK; }
#ifdef OPSOCKET_GETLOCALSOCKETADDR
	virtual OP_STATUS GetLocalSocketAddress(OpSocketAddress* socket_address) { return OpStatus::OK; }
#endif // OPSOCKET_GETLOCALSOCKETADDR
#ifdef SOCKET_LISTEN_SUPPORT
	virtual OP_STATUS Listen(OpSocketAddress* socket_address, int queue_size) { return OpStatus::OK; }
	virtual OP_STATUS Accept(OpSocket* socket) { return OpStatus::OK; }
#endif // SOCKET_LISTEN_SUPPORT
#ifdef _EXTERNAL_SSL_SUPPORT_
	virtual OP_STATUS UpgradeToSecure() { return OpStatus::OK; }
	virtual OP_STATUS SetSecureCiphers(const cipherentry_st* ciphers, UINT cipher_count) { return OpStatus::OK; }
	virtual OP_STATUS AcceptInsecureCertificate(OpCertificate *certificate) { return OpStatus::OK; }
	virtual OP_STATUS GetCurrentCipher(cipherentry_st* used_cipher, BOOL* tls_v1_used, UINT32* pk_keysize) { return OpStatus::OK; }
	virtual OP_STATUS SetServerName(const uni_char* server_name) { return OpStatus::OK; }
	virtual OpCertificate* ExtractCertificate() { return NULL }
	virtual OpAutoVector<OpCertificate>* ExtractCertificateChain() { return NULL; }
	virtual int GetSSLConnectErrors() { return 0; }
#endif // _EXTERNAL_SSL_SUPPORT_
#ifdef SOCKET_NEED_ABORTIVE_CLOSE
	virtual void SetAbortiveClose() {}
#endif // SOCKET_NEED_ABORTIVE_CLOSE
#ifdef SOCKET_SUPPORTS_TIMER
	virtual void SetTimeOutInterval(unsigned int seconds, BOOL restore_default = FALSE) {}
#endif // SOCKET_SUPPORTS_TIMER
#ifdef OPSOCKET_PAUSE_DOWNLOAD
	virtual OP_STATUS PauseRecv() { return OpStatus::OK; }
	virtual OP_STATUS ContinueRecv() { return OpStatus::OK; }
#endif // OPSOCKET_PAUSE_DOWNLOAD
#ifdef OPSOCKET_OPTIONS
	virtual OP_STATUS SetSocketOption(OpSocketBoolOption option, BOOL value) { return OpStatus::OK; }
	virtual OP_STATUS GetSocketOption(OpSocketBoolOption option, BOOL& out_value) { return OpStatus::OK; }
#endif // OPSOCKET_OPTIONS

	void reset()
	{
		connectCalls = 0;
		connectSocketAddress = NULL;
		connectReturn = OpStatus::OK;
		closeCalls = 0;
		sendCalls = 0;
		sendLastData.Clear();
		sendLastLen = 0;
		sendReturn = OpStatus::OK;
	}

	int connectCalls;
	OpSocketAddress* connectSocketAddress;
	OP_STATUS connectReturn;

	int closeCalls;

	int sendCalls;
	UniString sendLastData;
	UINT sendLastLen;
	OP_STATUS sendReturn;

	OpSocketListener* listener;
};

#endif // SELFTEST
#endif // MODULES_PI_SELFTEST_OPSOCKETMOCK_H
