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
#include "modules/libssl/protocol/tlsver10.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/debug/tstdump2.h"

#define HMAC_BLOCKSIZE 64
#define SSL_RANDOM_LENGTH 32

#ifdef _DEBUG
#ifdef YNP_WORK
#define SSL_VERIFIER
#endif
#endif

TLS_Version_1_0::TLS_Version_1_0(uint8 minor_ver)
 : TLS_Version_1_Dependent(minor_ver)
{
	final_hash= SSL_MD5_SHA;
}

TLS_Version_1_0::~TLS_Version_1_0()
{
}

#ifndef TLS_NO_CERTSTATUS_EXTENSION
void TLS_Version_1_0::SessionUpdate(SSL_SessionUpdate state)
{
	switch(state)
	{
	case Session_TLS_ExpectCertificateStatus:
		AddHandshakeAction(Handshake_Receive, TLS_Recv_CertificateStatus, TLS_CertificateStatus, Handshake_MustReceive, Handshake_Handle_Message, Handshake_Add_After_ID, SSL_V3_Recv_Certificate);
		break;
	case Session_Server_Done_Received:
		TLS_Version_1_Dependent::SessionUpdate(state);
		if (conn_state->next_protocol_extension_supported && conn_state->selected_next_protocol == NP_NOT_NEGOTIATED)
			AddHandshakeAction(Handshake_Send, TLS_Send_Next_Protocol, TLS_NextProtocol, Handshake_Will_Send, Handshake_Send_Message,Handshake_Add_Before_ID,SSL_V3_Send_Client_Finished);
		break;
	case Session_Finished_Confirmed:
		TLS_Version_1_Dependent::SessionUpdate(state);
		if (conn_state->next_protocol_extension_supported && conn_state->selected_next_protocol == NP_NOT_NEGOTIATED
			&& GetHandshakeStatus(Handshake_Send, SSL_V3_Send_Client_Finished) != Handshake_Sent)
			AddHandshakeAction(Handshake_Send, TLS_Send_Next_Protocol, TLS_NextProtocol, Handshake_Will_Send, Handshake_Send_Message,Handshake_Add_Before_ID,SSL_V3_Send_Client_Finished);
		break;
	default:
		TLS_Version_1_Dependent::SessionUpdate(state);
	}
}
#endif

BOOL TLS_Version_1_0::PRF(SSL_varvector32 &result, uint32 len, 
						   const SSL_varvector32 &secret_seed,
						   const char *label,  //Null teminated string
						   const SSL_varvector32 &data_seed) const
{
	SSL_secure_varvector32 result1;
	SSL_varvector32 secret_seed_1, secret_seed_2;
	uint32 secret_len_1,secret_len_2;
	
	result.SetSecure(TRUE);

	secret_len_1 = secret_seed.GetLength();
	secret_len_2 = (secret_len_1+1) /2;
	secret_seed_1.SetExternalPayload((byte *) secret_seed.GetDirect(), FALSE, secret_len_2);
	secret_seed_2.SetExternalPayload((byte *) secret_seed.GetDirect()+ (secret_len_1 - secret_len_2), FALSE, secret_len_2);
	
	if(!P_hash(result, len, secret_seed_1, label, data_seed, SSL_MD5) ||
		!P_hash(result1, len, secret_seed_2, label, data_seed, SSL_SHA))
	{
		result.Resize(0);
		return FALSE;
	}
	
	{
		uint32 len1=len;
		byte *src = result1.GetDirect();
		byte *trg = result.GetDirect();

		for(;len1>0;len1--)
			*(trg++) ^= *(src++);  
	}
	
#ifdef SSL_VERIFIER               
    OpString8 text;
		
	text.AppendFormat("TLS 1.0 PRF  outputdata string.");
    DumpTofile(result,len,text,"sslkeys.txt");
#endif

	return TRUE;
}


#endif
