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
#include "modules/libssl/keyex/sslkersa.h"
#include "modules/libssl/methods/sslpubkey.h"
#include "modules/libssl/certs/certhandler.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/handshake/premaster.h"

#include "modules/olddebug/tstdump.h"

SSL_RSA_KeyExchange::SSL_RSA_KeyExchange()
{
	InitFlags();
	RSA_premasterkey = NULL;
}

void SSL_RSA_KeyExchange::InitFlags()
{
	receive_certificate = Handshake_Expected;
	receive_server_keys = Handshake_Expected;
	receive_certificate_request = Handshake_Expected;
}

SSL_RSA_KeyExchange::~SSL_RSA_KeyExchange()
{
	OP_DELETE(RSA_premasterkey);
}

int SSL_RSA_KeyExchange::SetupServerCertificateCiphers()
{
	SSL_CipherSpec *cipherspec;
	SSL_CertificateHandler *certs;
	
	cipherspec = AccessCipher(FALSE);
	if(cipherspec == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return 1;
	}
	
	OP_DELETE(cipherspec->SignCipherMethod);
	cipherspec->SignCipherMethod = NULL; 

	OP_DELETE(RSA_premasterkey);
	RSA_premasterkey = NULL; 
	
	certs = AccessServerCertificateHandler();  
	if(certs == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return 1;
	}
	
	RSA_premasterkey =  certs->GetCipher(0);
	if(RSA_premasterkey == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
		return 1;
	}          
	RSA_premasterkey->ForwardTo(this);
	// TODO: Must make able to handle DSA/other certificates
	if(RSA_premasterkey->CipherID() != SSL_RSA)
	{
		RaiseAlert(SSL_Fatal, SSL_Illegal_Parameter);
		return 1;
	}          
	
	SetSigAlgorithm( 
#ifdef _SUPPORT_TLS_1_2
				AccessConnectionState()->version >= SSL_ProtocolVersion(3,3)  ? AccessConnectionState()->sigalg :  
#endif
				SSL_RSA_MD5_SHA_sign);
	if(RecvServerKey(RSA_premasterkey->KeyBitsLength()) )
		receive_server_keys = Handshake_Expected;

	cipherspec->SignCipherMethod = RSA_premasterkey->Fork();
	
	if(cipherspec->SignCipherMethod == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
		return 1;
	}          
	cipherspec->SignCipherMethod->ForwardTo(AccessConnectionState());
	
	return 0;
}

void SSL_RSA_KeyExchange::PreparePremaster()
{
	if(receive_certificate != Handshake_Received && receive_server_keys != Handshake_Expected)
	{
		RaiseAlert(SSL_Fatal, SSL_Handshake_Failure);
		return;
	}

	SSL_secure_varvector16 buffer;

	buffer.ForwardTo(this);
	CreatePremasterSecretToSend(buffer);
	if(Error())
		return;

	RSA_premasterkey->SetUsePrivate(FALSE);
#if 0
	DumpTofile(buffer,buffer.GetLength(),"RSA KEA Premaster","sslkeys.txt");
#endif
	RSA_premasterkey->EncryptVector(buffer, pre_master_secret_encrypted);
#if 0
	DumpTofile(pre_master_secret_encrypted,pre_master_secret_encrypted.GetLength(),"RSA KEA encrypted Premaster","sslkeys.txt");
#endif

	AccessConnectionState()->DetermineSecurityStrength(RSA_premasterkey);
}

void SSL_RSA_KeyExchange::CreatePremasterSecretToSend(SSL_secure_varvector16 &buffer)
{
	SSL_PreMasterSecret RSApremaster;
	
	RSApremaster.Generate();                
	RSApremaster.SetVersion(GetSentProtocolVersion());
	SSL_TRAP_AND_RAISE_ERROR_THIS(RSApremaster.WriteRecordL(&pre_master_secret));
	buffer = pre_master_secret;
}

#endif
