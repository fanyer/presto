/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen 
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#include "modules/libssl/sslbase.h"

#ifdef __SSL_WITH_DH_DSA__

#include "modules/libssl/ssl_api.h"
#include "modules/libssl/keyex/sslkedh.h"
#include "modules/libssl/methods/sslpubkey.h"
#include "modules/libssl/certs/certhandler.h"
#include "modules/libssl/handshake/dhparam.h"
#include "modules/libssl/handshake/keyex_server.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"


SSL_DH_KeyExchange::SSL_DH_KeyExchange(BOOL ephermal,BOOL anonymous,
									   SSL_SignatureAlgorithm signertype)
{
	InitFlags();
	flags.ephermal = ephermal;
	flags.anonymous = anonymous;
	flags.signertype = signertype;
}

/*
SSL_DH_KeyExchange::SSL_DH_KeyExchange(const SSL_DH_KeyExchange *other)
: SSL_KeyExchange(other)
{
	if(other != NULL)
		flags = other->flags;
	else
		InitFlags();

	if(flags.anonymous)
		flags.will_recv_key = TRUE;
}
*/

void SSL_DH_KeyExchange::InitFlags()
{
	receive_certificate = Handshake_Expected;
	receive_server_keys = Handshake_Expected;
	receive_certificate_request = Handshake_Expected;

	DH_premasterkey = NULL;
}

SSL_DH_KeyExchange::~SSL_DH_KeyExchange()
{
	OP_DELETE(DH_premasterkey);
}

/*
SSL_KeyExchange *SSL_DH_KeyExchange::Fork() const
{
return new SSL_DH_KeyExchange(this);
}*/


int SSL_DH_KeyExchange::SetupServerCertificateCiphers()
{
	SSL_CipherSpec *cipherspec;
	SSL_PublicKeyCipher * OP_MEMORY_VAR SignCipher;
	SSL_CertificateHandler *certs;
	
	cipherspec = AccessCipher(FALSE);
	if(cipherspec == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return 1;
	}
	
	SignCipher = cipherspec->SignCipherMethod;
	if(SignCipher != NULL)
	{
		OP_DELETE(SignCipher);
		cipherspec->SignCipherMethod = NULL; 
	}

	certs = AccessServerCertificateHandler();  
	if(certs == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return 1;
	}
	
	SignCipher = NULL;
	if(!flags.anonymous)
	{
		SSL_ClientCertificateType cert_type = certs->CertificateType(0);
	
#ifdef _SUPPORT_TLS_1_2
		if(AccessConnectionState()->version < SSL_ProtocolVersion(3,3))
			flags.signertype = (cert_type == SSL_rsa_sign ? SSL_RSA_MD5_SHA_sign : SSL_DSA_sign);
#endif

		if( flags.ephermal ||
			(flags.signertype == SSL_RSA_MD5_SHA_sign && cert_type == SSL_rsa_sign) ||
			(flags.signertype == SSL_DSA_sign && cert_type == SSL_dss_sign))
		{
			SignCipher =  certs->GetCipher(0);
			if(SignCipher == NULL)
			{
				RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
				return 1;
			}          
			if(SignCipher->CipherID() != SignAlgToCipher(flags.signertype))
			{
				RaiseAlert(SSL_Fatal, SSL_Illegal_Parameter);
				return 1;
			}
		}
		else
		{
			if(!((flags.signertype == SSL_RSA_MD5_SHA_sign && cert_type != SSL_rsa_fixed_dh) ||
				(flags.signertype == SSL_DSA_sign && cert_type == SSL_dss_fixed_dh)))
			{
				RaiseAlert(SSL_Fatal, SSL_Illegal_Parameter);
				return 1;
			}
		}
	}
	
	SetSigAlgorithm(flags.signertype);
	
	receive_server_keys = ((flags.anonymous || flags.ephermal) && SignCipher != NULL ? Handshake_Expected : Handshake_Will_Not_Receive) ;

	if(receive_server_keys == Handshake_Expected)
	{ 
		cipherspec->SignCipherMethod = SignCipher;
		OP_STATUS op_err = OpStatus::OK;
		DH_premasterkey = g_ssl_api->CreatePublicKeyCipher(SSL_DH, op_err);
		OpStatus::Ignore(op_err);
	}
	else
	{
		OP_DELETE(SignCipher);
		SignCipher = NULL;
		DH_premasterkey = certs->GetCipher(0);
		if(DH_premasterkey == NULL || DH_premasterkey->CipherID() != SSL_DH)
		{
			RaiseAlert(SSL_Fatal, SSL_Illegal_Parameter);
			return 1;
		}
	}
	
	if(DH_premasterkey == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
		return 1;
	}          
	if(SignCipher != NULL)
		SignCipher->ForwardTo(this);
	DH_premasterkey->ForwardTo(this);

	return 0;
}

void SSL_DH_KeyExchange::PreparePremaster()
{
	if(flags.ephermal && receive_server_keys != Handshake_Received)
	{
		RaiseAlert(SSL_Fatal, SSL_Handshake_Failure);
		return;
	}
	DH_premasterkey->ProduceGeneratedPublicKeys();
	DH_premasterkey->ProduceGeneratedPrivateKeys();
	DH_premasterkey->UnLoadPrivateKey(SSL_DH_private_common,pre_master_secret);
	DH_premasterkey->UnLoadPublicKey(SSL_DH_gen_public_key,pre_master_secret_encrypted);


#if 0
	DumpTofile(pre_master_secret_encrypted,pre_master_secret_encrypted.GetLength(),"DH KEA Premaster","sslkeys.txt");
#endif
	AccessConnectionState()->DetermineSecurityStrength(DH_premasterkey);
}

/*virtual */
SSL_KeyExchangeAlgorithm SSL_DH_KeyExchange::GetKeyExhangeAlgorithm()
{
	OP_ASSERT(!(flags.ephermal && flags.anonymous)); // Cannot be both ephermal and anonymous at once

	if (flags.ephermal)
		return SSL_Ephemeral_Diffie_Hellman_KEA;
	else if (flags.anonymous)
		return SSL_Anonymous_Diffie_Hellman_KEA;
	else return SSL_Diffie_Hellman_KEA;
}

int SSL_DH_KeyExchange::SetupServerKeys(SSL_Server_Key_Exchange_st *serverkeys)
{
	SSL_ServerDHParams params;

	if(!serverkeys)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return 1;
	}          
	
	if(flags.anonymous)
	{
		OP_STATUS op_err = OpStatus::OK;
		DH_premasterkey = g_ssl_api->CreatePublicKeyCipher(SSL_DH, op_err);
		OpStatus::Ignore(op_err);

		if(!DH_premasterkey)
		{
			RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
			return 1;
		}          
	}

	serverkeys->GetParam(params);
	
	DH_premasterkey->LoadPublicKey(SSL_DH_public_prime, params.GetDH_P());
	DH_premasterkey->LoadPublicKey(SSL_DH_public_generator, params.GetDH_G());
	DH_premasterkey->LoadPublicKey(SSL_DH_recv_public_key, params.GetDH_Ys());
	

	if(DH_premasterkey->ErrorRaisedFlag)
	{
		RaiseAlert(DH_premasterkey);
		return 1;
	}

	if(flags.ephermal &&
		AccessConnectionState()->session->security_rating == 3 && 
		!AccessConnectionState()->server_info->UseReducedDHE() )
	{
		uint32 sec_level_policy = (uint32) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MinimumSecurityLevel);

		uint16 keysize = DH_premasterkey->KeyBitsLength();

		if((sec_level_policy <2 && keysize <1024) ||
			(sec_level_policy >= 2 && keysize < 1100 + (sec_level_policy-2)*100))
		{
			AccessConnectionState()->server_info->SetUseReducedDHE();
			RaiseAlert(SSL_Fatal, SSL_Insufficient_Security1);
			return 1;
		}
	}

	AccessConnectionState()->DetermineSecurityStrength(DH_premasterkey);
	
	return 0;
}


BOOL SSL_DH_KeyExchange::GetIsAnonymous()
{
	return flags.anonymous;
}


#endif
#endif
