/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"
#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/keyex/sslkeyex.h"
#include "modules/libssl/protocol/sslver30.h"

#include "modules/libssl/protocol/tlsver10.h"
#include "modules/libssl/protocol/tlsver12.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/libssl/method_disable.h"
#include "modules/libssl/methods/sslcipher.h"
#include "modules/libssl/methods/sslpubkey.h"

SSL_ConnectionState::SSL_ConnectionState(SSL_Port_Sessions *serv_info
#ifdef LIBSSL_HANDSHAKE_HEXDUMP
			, SSL_DataQueueHead &a_selftest_sequence
#endif
		 )
#ifdef LIBSSL_HANDSHAKE_HEXDUMP
	: selftest_sequence(a_selftest_sequence)
#endif
{
	server_info = serv_info;
	InternalInit();
}

void SSL_ConnectionState::InternalInit()
{
	session = NULL;
	sigalg = SSL_Anonymous_sign;
	anonymous_connection = FALSE;
	server_cert_handler= NULL;
	read.cipher = NULL;
	write.cipher = NULL; 
	Prepare_cipher_spec(FALSE);
	Prepare_cipher_spec(TRUE);
	version_specific = NULL;
	key_exchange = NULL;
	window = NULL;
	user_interaction_blocked = FALSE;
	selected_next_protocol = NP_NOT_NEGOTIATED;
	next_protocol_extension_supported = FALSE;
}


SSL_ConnectionState::~SSL_ConnectionState()
{
	InternalDestruct();
}

void SSL_ConnectionState::InternalDestruct()
{
	if(session != NULL)
	{
		session->connections--;
		if(session->connections == 0 && !session->is_resumable)
		{
			if(session->InList())
			{
				session->Out();
			}
			
			OP_DELETE(session);
			session = NULL;
		}
	}
	OP_DELETE(server_cert_handler);
	server_cert_handler = NULL;
	OP_DELETE(read.cipher);
	read.cipher = NULL;
	OP_DELETE(write.cipher);
	write.cipher = NULL;
	OP_DELETE(version_specific);
	version_specific = NULL;
	OP_DELETE(key_exchange);
	key_exchange = NULL;
}



SSL_CipherSpec *SSL_ConnectionState::Prepare_cipher_spec(BOOL writecipher)
{
	SSL_CipherSpec *cipher;
	
	cipher = OP_NEW(SSL_CipherSpec, ());
	if(cipher == NULL)
	{
		RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
		return NULL;
	}
	
	if(writecipher)
	{
		OP_DELETE(write.cipher);
		write.cipher = cipher;
	}
	else
	{
		OP_DELETE(read.cipher);
		read.cipher = cipher;
	}

	return cipher;
}


void SSL_ConnectionState::PrepareCipher(SSL_CipherDirection dir,SSL_CipherSpec* cipher, SSL_BulkCipherType method,
						SSL_HashAlgorithmType hash, const SSL_Version_Dependent *version_specific)
{
	SSL_GeneralCipher *method1=NULL;
	SSL_MAC *mac;

	cipher->Sequence_number = 0;
	OP_STATUS op_err = OpStatus::OK;
	method1 = cipher->Method = g_ssl_api->CreateSymmetricCipher(method, op_err);
	if(OpStatus::IsError(op_err))
	{
		RaiseAlert(op_err);
		return;
	}
	method1->SetCipherDirection(dir);
	mac =cipher->MAC = version_specific->GetMAC();
	if(mac == NULL)
	{
		RaiseAlert(SSL_Fatal, SSL_Allocation_Failure);
		return;
	}
	ForwardToThis(*method1, *mac);
	mac->SetHash(hash);  
}

int SSL_ConnectionState::SetupStartCiphers()
{
	SSL_CipherDescriptions * OP_MEMORY_VAR cipherdesc = NULL;
	
	cipherdesc = Find_CipherDescription(session->used_cipher);

	if(cipherdesc == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return 1;
	}

	session->cipherdescription = cipherdesc;
	version_specific->SetCipher(cipherdesc);
	if(!session->session_negotiated)
	{
		session->security_rating = (int) session->cipherdescription->security_rating;
		session->low_security_reason = (int) session->cipherdescription->low_reason;
	}
	
	PrepareCipher(SSL_Decrypt, read.cipher, cipherdesc->method, cipherdesc->hash, version_specific);
	PrepareCipher(SSL_Encrypt, write.cipher, cipherdesc->method, cipherdesc->hash, version_specific);
	if (ErrorRaisedFlag)
		return 1;
	
	sigalg =  cipherdesc->sigalg;

#ifdef _SUPPORT_TLS_1_2
	if(version < SSL_ProtocolVersion(3,3) && sigalg != SSL_DSA_sign && sigalg != SSL_Anonymous_sign)
		sigalg = SSL_RSA_MD5_SHA_sign;
#endif
	
	OP_DELETE(key_exchange);
	key_exchange = NULL;

	TRAPD(op_err, key_exchange = CreateKeyExchangeL(cipherdesc->kea_alg, cipherdesc->sigalg));
	if(OpStatus::IsError(op_err))
	{
		RaiseAlert(op_err);
		return 1;
	}
	else
	{
		OP_ASSERT(key_exchange != NULL);

		key_exchange->SetCommState(this);
		anonymous_connection = key_exchange->GetIsAnonymous();
	}

	return 0;
}

void SSL_ConnectionState::CalculateMasterSecret()
{
	key_exchange->PreparePremaster();
	version_specific->CalculateMasterSecret(session->mastersecret, key_exchange->PreMasterSecret());
	key_exchange->PreMasterUsed();
}


void SSL_ConnectionState::SetUpCiphers()
{
	version_specific->CalculateKeys( session->mastersecret,
		write.cipher, read.cipher);
}

SSL_CipherDescriptions *SSL_ConnectionState::Find_CipherDescription(const SSL_CipherID &id)
{
	SSL_CipherDescriptions *item;
	
	item = g_securityManager->SystemCiphers.First();
	while (item != NULL)
	{
		if(item->id == id)
			break;
		item = item->Suc();
	}
	
	return item;
}

void SSL_ConnectionState::SetVersion(const SSL_ProtocolVersion &ver)
{ 
	BOOL allocated = FALSE;
	version = ver;
	write.version = ver;
	read.version = ver;

	OP_DELETE(version_specific );
	version_specific = NULL;
	
	switch(ver.Major())
	{
	case 3 :
		switch(ver.Minor())
		{
		case 1 :  
		case 2:
			// Class TLS_Version_1_0 speaks both TLS 1.0 and tls 1.1
			if (
				securityManager->Enable_TLS_V1_0 || securityManager->Enable_TLS_V1_1 ||
				(securityManager->Enable_TLS_V1_2 && server_info->IsInTestModus())
				)
			{
				// when testing for 1.2, Allow tls 1.0 records for for hello requests.
				allocated = TRUE;
				version_specific = OP_NEW(TLS_Version_1_0, (ver.Minor()));
			}
			break;
#ifdef _SUPPORT_TLS_1_2
		case 3:
			if(securityManager->Enable_TLS_V1_2)
			{
				allocated = TRUE;
				version_specific = OP_NEW(TLS_Version_1_2, (ver.Minor()));
			}
			break;
#endif // _SUPPORT_TLS_1_2
		case 0 :  
			if(securityManager->Enable_SSL_V3_0)
			{
				allocated = TRUE;
				version_specific = OP_NEW(SSL_Version_3_0, ());
			}
			break;
		}
	}

	if(version_specific == NULL)
	{
		RaiseAlert(SSL_Fatal, (allocated ? SSL_Allocation_Failure : SSL_Handshake_Failure)) ;
	}
	else
	{
		version_specific->ForwardTo(this);
		version_specific->SetConnState(this);
	}
}


void SSL_ConnectionState::DetermineSecurityStrength(SSL_PublicKeyCipher *RSA_premasterkey)
{
	if(session == NULL)
		return;

	if(RSA_premasterkey)
		g_ssl_api->DetermineSecurityStrength(RSA_premasterkey, session->lowest_keyex, session->security_rating, session->low_security_reason);

	const char *prot = "SSL";
	unsigned major = session->used_version.Major();
	unsigned minor = session->used_version.Minor();
	
	if(major >3 || (major == 3 && minor > 0))
	{
		prot = "TLS";
		if(major == 3)
			minor --;
		major -= 2;
	}
	else
	{
		OP_ASSERT(major ==3);
#if !defined LIBSSL_ALWAYS_WARN_SSLv3
		if((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_WARN_SSLV3) != 0)
#endif
		{
			session->low_security_reason |= SECURITY_LOW_REASON_WEAK_PROTOCOL;
			session->security_rating = SECURITY_STATE_LOW;
		}
	}
	
	session->security_cipher_string.Empty();
	OpString8 temp_string;
	OP_STATUS result = 	temp_string.AppendFormat(
					"%s v%d.%d %s (%d bit %s/%s)", 
					prot, major, minor, 
					session->cipherdescription->EncryptName.CStr(),
					session->lowest_keyex.RSA_keysize,
					session->cipherdescription->KEAname.CStr(),
					session->cipherdescription->HashName.CStr()
			);

	if(OpStatus::IsSuccess(result))
		result = session->security_cipher_string.Set(temp_string);

	if (OpStatus::IsError(result)) 
    {
        RaiseAlert(result);
    }
			
}

BOOL SSL_ConnectionState::Resume_Session()
{
	if(session == NULL)
	{
		RaiseAlert(SSL_Internal,SSL_InternalError);
		return FALSE;
	}
	
	if(SetupStartCiphers())
	{
		RaiseAlert(SSL_Fatal,SSL_Handshake_Failure);
		return FALSE;
	}
	
	if(session->cipherdescription->KeySize == 0)
	{
		RaiseAlert(SSL_Warning,SSL_Authentication_Only_Warning);
		return FALSE;
	}

	SetUpCiphers();

	OP_ASSERT(session->security_cipher_string.HasContent());

	return !ErrorRaisedFlag;
}

void SSL_ConnectionState::BroadCastSessionNegotiatedEvent(BOOL success)
{
	if (session)
		session->session_negotiated = TRUE;
	if (server_info != NULL ) 	// BREW compiler requires an explicit check with NULL for OpSmartPointer.
		server_info->BroadCastSessionNegotiatedEvent(success);
}

#endif // _NATIVE_SSL_SUPPORT_
