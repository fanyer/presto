/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/pi/OpSystemInfo.h"

#include "modules/libssl/sslbase.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/keyex/sslkeyex.h"
#include "modules/libssl/handshake/rsaparam.h"
#include "modules/libssl/handshake/dhparam.h"
#include "modules/libssl/handshake/keyex_server.h"

#include "modules/util/cleanse.h"

SSL_Server_Key_Exchange_st::SSL_Server_Key_Exchange_st()
{
	InternalInit();
}

void SSL_Server_Key_Exchange_st::InternalInit()
{
	ForwardToThis(sign,sighasher);
    alg = SSL_NULL_KEA;
	params = &dummy;
	sigcipher = NULL;
	connstate = NULL;
	rsa = NULL;
	AddItem(params);
	AddItem(sign);
}

SSL_Server_Key_Exchange_st::~SSL_Server_Key_Exchange_st()
{
	if(params != NULL)
	{
		LoadAndWritableList *temp = rsa;
		params = NULL;

		OP_DELETE(temp);
	}
}

void SSL_Server_Key_Exchange_st::SetCommState(SSL_ConnectionState *item)
{
	connstate = item;
	if(connstate != NULL && connstate->session != NULL && connstate->session->cipherdescription != NULL)
	{
		SSL_CipherSpec *cipher;
		
		SetKEA(connstate->session->cipherdescription->kea_alg);
		SetSignatureAlgorithm(connstate->sigalg);
		
		cipher = connstate->read.cipher/*(connstate->client ? connstate->read.cipher : connstate->write.cipher)*/;
		SetCipher(cipher->SignCipherMethod);
#ifdef _SUPPORT_TLS_1_2
		if(connstate->version.Compare(3,3) >= 0)
			sign.SetTLS12_Mode();
#endif
	}
}

SSL_KEA_ACTION SSL_Server_Key_Exchange_st::ProcessMessage(SSL_ConnectionState *pending)
{
	OP_ASSERT(pending && pending->session && pending->key_exchange);
	SSL_Alert msg;
	SSL_CipherSpec *cipher = pending->read.cipher;
	
	if(cipher == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return  SSL_KEA_Handle_Errors;
	}

	SetCipher(cipher->SignCipherMethod);

	if(!Verify(pending->client_random, pending->server_random , &msg))
	{
		if(!pending->server_info->UseReducedDHE())
		{
			pending->server_info->SetUseReducedDHE();
			RaiseAlert(SSL_Fatal, SSL_Insufficient_Security1);
		}
		else
			RaiseAlert(msg);

		return SSL_KEA_Handle_Errors;
	}

	return pending->key_exchange->ReceivedServerKeys(this);
}

BOOL SSL_Server_Key_Exchange_st::Valid(SSL_Alert *msg) const  
{
	if(!SSL_Handshake_Message_Base::Valid(msg))
		return FALSE;

	switch(alg)
	{
    case SSL_RSA_EXPORT_KEA :
    case SSL_RSA_KEA :  
		if(rsa == NULL || params != rsa)
		{
			if(msg != NULL)
				msg->Set(SSL_Internal, SSL_InternalError);
			return FALSE;
		}
		if(!rsa->Valid(msg))
			return FALSE;
		break;
#ifdef SSL_DH_SUPPORT
    case SSL_Anonymous_Diffie_Hellman_KEA :
    case SSL_Ephemeral_Diffie_Hellman_KEA :
    case SSL_Diffie_Hellman_KEA :
		if(dh == NULL)
		{
			if(msg != NULL)
				msg->Set(SSL_Internal, SSL_InternalError);
			return FALSE;
		}
		if(!dh->Valid(msg))
			return FALSE;
		break;
#endif // SSL_DH_SUPPORT
    case SSL_NULL_KEA : break;
		
    case SSL_Illegal_KEA:
    default           :
		if(msg != NULL)
			msg->Set(SSL_Internal, SSL_InternalError);
		return FALSE;
	}
	
	return TRUE;  
}

void SSL_Server_Key_Exchange_st::SetKEA(SSL_KeyExchangeAlgorithm kea)
{ 
	SSL_CipherSpec *cipher;
	
	if(alg != kea)
	{
		LoadAndWritableList *temp = rsa;
		params = rsa=NULL;

		OP_DELETE(temp);
		
		alg=kea;
		switch(kea)
		{
		case SSL_RSA_EXPORT_KEA :
		case SSL_RSA_KEA :  
			rsa = OP_NEW(SSL_ServerRSAParams, ());
			if(rsa == NULL)
			{
				RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
				alg = SSL_NULL_KEA;
				return;
			}
			params = rsa;
			if(connstate != NULL)
			{
				SetSignatureAlgorithm(connstate->sigalg);
				
				cipher = connstate->read.cipher;
				SetCipher(cipher->SignCipherMethod);
			}                           
			break;
#ifdef SSL_DH_SUPPORT
		case SSL_Anonymous_Diffie_Hellman_KEA:
			sign.SetEnableRecord(FALSE); // Anonymous does not have signature field
			// Falling through
		case SSL_Ephemeral_Diffie_Hellman_KEA :
		case SSL_Diffie_Hellman_KEA : 
			alg=kea;
			dh = OP_NEW(SSL_ServerDHParams, ());
			if(dh == NULL)
			{
				RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
				kea = SSL_NULL_KEA;
				return;
			}
			params = dh;
			if(connstate != NULL)
			{
				SetSignatureAlgorithm(connstate->sigalg);
				
				cipher = connstate->read.cipher; /*(connstate->client ? connstate->read.cipher : connstate->write.cipher);*/
				SetCipher(cipher->SignCipherMethod);
			}
			break;
#endif // SSL_DH_SUPPORT
		case SSL_Illegal_KEA:
		case SSL_NULL_KEA :
			alg=kea;
			rsa=NULL;
			params = &dummy;
			break;
		default           : 
			alg= SSL_Illegal_KEA;
			rsa=NULL;
			params = &dummy;
			break;
		}
	}
}

void SSL_Server_Key_Exchange_st::SetSignatureAlgorithm(SSL_SignatureAlgorithm item)
{
	sign.SetSignatureAlgorithm(item);
}

/* 
SSL_SignatureAlgorithm SSL_Server_Key_Exchange_st::GetSignatureAlgorithm() const
{
	return sign.GetSignatureAlgorithm();
}
*/

/* 
const SSL_Signature &SSL_Server_Key_Exchange_st::GetSignature() const
{
	return sign;
}
*/

void SSL_Server_Key_Exchange_st::GetParam(SSL_ServerRSAParams &item)  
{
	if (alg == SSL_RSA_KEA || alg == SSL_RSA_EXPORT_KEA)
	{
		item = *rsa;
	}
}

#ifdef SSL_DH_SUPPORT
void SSL_Server_Key_Exchange_st::GetParam(SSL_ServerDHParams &item)
{
	if (alg == SSL_Diffie_Hellman_KEA || alg == SSL_Ephemeral_Diffie_Hellman_KEA || alg == SSL_Anonymous_Diffie_Hellman_KEA)
	{
		item = *dh;
	}
}
#endif // SSL_DH_SUPPORT

void SSL_Server_Key_Exchange_st::SetCipher(SSL_PublicKeyCipher *cipher)
{
	sigcipher = cipher;
	
	sign.SetCipher(sigcipher);
}

BOOL SSL_Server_Key_Exchange_st::Verify(SSL_varvector16 &client, SSL_varvector16 &server, SSL_Alert *msg)
{
	SSL_varvector16 buf;
	SSL_Signature testsig(NULL);

	if(testsig.ErrorRaisedFlag)
	{
		testsig.Error(msg);
		return FALSE;
	}
	testsig.ForwardTo(this);
    
	testsig.SetSignatureAlgorithm(sign.GetSignatureAlgorithm());
	testsig.InitSigning();
	testsig.Sign(client);
	testsig.Sign(server);
	
	if(params != NULL)
		SSL_TRAP_AND_RAISE_ERROR_THIS(params->WriteRecordL(&buf));
	
	testsig.Sign(buf);
	testsig.FinishSigning();

	if(ErrorRaisedFlag)
	{
		Error(msg);
		return FALSE;
	}
	
	return sign.Verify(testsig,msg);
}

#endif // relevant support
