/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#ifdef _SUPPORT_TLS_1_2

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/cipher_description.h"
#include "modules/libssl/protocol/tlsver10.h"
#include "modules/libssl/protocol/tlsver12.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/debug/tstdump2.h"

#ifdef _DEBUG
#ifdef YNP_WORK
#define SSL_VERIFIER
#endif
#endif

TLS_Version_1_2::TLS_Version_1_2(uint8 minor_ver)
 : TLS_Version_1_Dependent(minor_ver)
{
}

TLS_Version_1_2::~TLS_Version_1_2()
{
}

void TLS_Version_1_2::SetCipher(const SSL_CipherDescriptions *desc)
{
	TLS_Version_1_Dependent::SetCipher(desc);
	OP_ASSERT(cipher_desc);

	switch(cipher_desc->prf_fun)
	{
	default:
		OP_ASSERT(!"Unknown PRF functiontion");
		// Ignoring error
	case TLS_PRF_default:
		final_hash = SSL_SHA_256;
		break;
	}
	GetHandshakeHash(final_hash);
}

#ifndef TLS_NO_CERTSTATUS_EXTENSION
void TLS_Version_1_2::SessionUpdate(SSL_SessionUpdate state)
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

BOOL TLS_Version_1_2::PRF(SSL_varvector32 &result, uint32 len, 
						   const SSL_varvector32 &secret_seed,
						   const char *label,  //Null teminated string
						   const SSL_varvector32 &data_seed) const
{
	result.SetSecure(TRUE);
	switch(cipher_desc->prf_fun)
	{
	case TLS_PRF_default:
		if(!P_hash(result, len,secret_seed, label,data_seed,SSL_SHA_256))
		{
			result.Resize(0);
			return FALSE;
		}
	
		break;
	default:
		OP_ASSERT(!"Unknown PRF functiontion");
		result.Resize(0);
		return FALSE;
		break;
	}

#ifdef SSL_VERIFIER               
    OpString8 text;
		
	text.AppendFormat("TLS 1.2 PRF  outputdata string.");
    DumpTofile(result,len,text,"sslkeys.txt");
#endif

	return TRUE;
}
#endif // _SUPPORT_TLS_1_2
#endif
