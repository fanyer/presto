/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#ifndef SSLCONNSESS_H
#define SSLCONNSESS_H

#if defined _NATIVE_SSL_SUPPORT_
# ifdef SELFTEST
/* Control potentially huge (and time-consuming) hex-dump.
 *
 * SSL::ApplicationHandleFatal() can, if desired, emit a hex-dump (along with
 * its usual progress information) of all the data from the pending_connstate's
 * selftest_sequence.  This is only meaningful when running selftests (hence the
 * member name), as the relevant code is conditioned on active selftests.
 * Enable the #define LIBSSL_HANDSHAKE_HEXDUMP turns this on.
 */
#  ifdef YNP_WORK
#define LIBSSL_HANDSHAKE_HEXDUMP
#  endif // Yngve always wants it.
# endif // SELFTEST

class SSL_Version_Dependent;
class SSL_CipherDescriptions;
class SSL_CipherSpec;
class SSL_PublicKeyCipher;

#include "modules/libssl/protocol/sslver.h"
#include "modules/libssl/base/sslvarl1.h"
#include "modules/url/protocols/next_protocol.h"

class SSL_ConnectionState : public SSL_Error_Status
{
private:
	SSL_ConnectionState();  //Non-existent
	SSL_ConnectionState(SSL_ConnectionState *); //Non-existent
	SSL_ConnectionState(SSL_ConnectionState &); //Non-existent

public:
	BOOL anonymous_connection;
	SSL_SignatureAlgorithm sigalg;

	SSL_Port_Sessions_Pointer server_info;

	SSL_SessionStateRecordList *session;
	SSL_Version_Dependent *version_specific;
	SSL_KeyExchange *key_exchange;

	SSL_CertificateHandler *server_cert_handler;

	SSL_ProtocolVersion version;
	SSL_varvector16 client_random, server_random;
	SSL_varvector24 prepared_server_finished;

	SSL_varvector32 last_client_finished;
	SSL_varvector32 last_server_finished;

	struct direction_cipher{
		SSL_CipherSpec *cipher;
		SSL_ProtocolVersion version;
	} read,write;

	SSL_ProtocolVersion sent_version;
	SSL_CipherSuites sent_ciphers;
	SSL_varvectorCompress sent_compression;

	SSL_NextProtocols next_protocols_available;
	NextProtocol selected_next_protocol;
	BOOL next_protocol_extension_supported;

	SSL_DataQueueHead handshake_queue;

	URL	ActiveURL;
	OpWindow *window;
	BOOL user_interaction_blocked;

#ifdef LIBSSL_HANDSHAKE_HEXDUMP
	SSL_DataQueueHead &selftest_sequence;
#endif

public:
	SSL_ConnectionState(SSL_Port_Sessions *serv_info
#ifdef LIBSSL_HANDSHAKE_HEXDUMP
			, SSL_DataQueueHead &a_selftest_sequence
#endif
		);
	virtual ~SSL_ConnectionState();

private:
	void InternalInit();
	void InternalDestruct();

public:

	SSL_SessionStateRecordList *FindSessionRecord();
	void AddSessionRecord(SSL_SessionStateRecordList *target);
	//void RemoveSessionRecord(SSL_SessionStateRecordList *target);

	void RemoveSession();
	void OpenNewSession();

	void SetVersion(const SSL_ProtocolVersion &ver);

	int SetupStartCiphers();
	void CalculateMasterSecret();
	void SetUpCiphers();
	SSL_CipherDescriptions *Find_CipherDescription(const SSL_CipherID &);
	void PrepareCipher(SSL_CipherDirection dir, SSL_CipherSpec* cipher, SSL_BulkCipherType method,
		SSL_HashAlgorithmType hash, const SSL_Version_Dependent *version_specific);

	void DetermineSecurityStrength(SSL_PublicKeyCipher *RSA_premasterkey);
	BOOL Resume_Session();

	SSL_CipherSpec *Prepare_cipher_spec(BOOL writecipher);

	/* This session of this connection is negotiated. Signal to all
	 * associated  SSL connections that the handshakes can continue.
	 * 
	 * @param success If true the negotiation succeeded,
	 *                If false the negotiation failed.
	 */
	void BroadCastSessionNegotiatedEvent(BOOL success);

};

#endif
#endif // SSLCONNSESS_H
