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
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/sslrand.h"

#include "modules/libssl/base/sslprotver.h"
#include "modules/libssl/handshake/chcipher.h"
#include "modules/libssl/handshake/randfield.h"
#include "modules/libssl/handshake/cipherid.h"
#include "modules/libssl/handshake/vectormess.h"
#include "modules/libssl/handshake/hello_client.h"
#include "modules/libssl/handshake/hello_server.h"
#include "modules/libssl/handshake/asn1certlist.h"
#include "modules/libssl/handshake/cert_message.h"
//#include "modules/libssl/handshake/rsaparam.h"
//#include "modules/libssl/handshake/dhparam.h"
#include "modules/libssl/handshake/tlssighash.h"
#include "modules/libssl/handshake/sslsig.h"
#include "modules/libssl/handshake/keyex_server.h"
#include "modules/libssl/handshake/certreq.h"
//#include "modules/libssl/handshake/premaster.h"
#include "modules/libssl/handshake/keyex_client.h"
#include "modules/libssl/handshake/verify_message.h"
#include "modules/libssl/handshake/fin_message.h"
#include "modules/libssl/handshake/hello_done.h"
#include "modules/libssl/handshake/cert_status.h"
#include "modules/libssl/handshake/next_protocol.h"

#include "modules/util/cleanse.h"

SSL_HandShakeMessage::SSL_HandShakeMessage()
:	msg(NULL), ServerKeys(NULL), connstate(NULL)
{
	AddItem(msg);
	AddItem(dummy);
	spec.enable_tag = TRUE;
	spec.idtag_len = 1;	
	spec.enable_length = TRUE;
	spec.length_len = 3;
}

SSL_HandShakeMessage::~SSL_HandShakeMessage()
{
	if(msg != NULL)
	{
		LoadAndWritableList *temp = msg;
		msg = NULL;

		OP_DELETE(temp);
	}
}


void SSL_HandShakeMessage::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	switch(action)
	{
	case DataStream::KReadAction:
	case DataStream::KWriteAction:
		{
			uint32 step = arg1;
			int record_item_id = (int) arg2;

			if(record_item_id == DataStream_SequenceBase::STRUCTURE_START)
			{
				if(action == DataStream::KWriteAction)
					spec.enable_length = (connstate->version.Major() >= 3);
				else
					length.SetEnableRecord(connstate->version.Major() >= 3);
				dummy.ResetRecord();
				dummy.SetEnableRecord(TRUE);
			}
			
			LoadAndWritableList::PerformActionL(action, step, record_item_id);
			
			if(action == DataStream::KReadAction)
			{
				if(record_item_id == RECORD_TAG)
				{
					if(connstate->version.Major() < 3)
						SetTag((SSL_HandShakeType)(((uint32) GetTag()) | 0x100)); // Convert 8 bit value into the SSL v2 message type
					
					LEAVE_IF_ERROR(SetMessage((SSL_HandShakeType) GetTag()));
				}
				else if(record_item_id == STRUCTURE_FINISHED)
				{
					if(dummy.GetLength() != 0 && GetTag() != SSL_Client_Hello)
						RaiseAlert(SSL_Fatal,SSL_Illegal_Parameter);
				}
			}
		}
		break;
	default: 
		LoadAndWritableList::PerformActionL(action, arg1, arg2);
	}
}

OP_STATUS SSL_HandShakeMessage::SetMessage( SSL_HandShakeType item)
{
	if ((SSL_HandShakeType) GetTag() != item || msg == NULL)
	{
		LoadAndWritableList *temp;
		if(msg != NULL)
		{
			temp = msg;
			msg = NULL;
			ServerKeys = NULL;
			OP_DELETE(temp);
		}

		temp = NULL;

		switch (item)
		{
		case SSL_Client_Hello  : 
			temp = OP_NEW(SSL_Client_Hello_st, ());
			break;
		case SSL_Server_Hello  :
			temp = OP_NEW(SSL_Server_Hello_st, ());
			break;
		case SSL_Certificate :   
			temp = OP_NEW(SSL_Certificate_st, ());
			break;
		case SSL_Server_Key_Exchange  : 
			temp = ServerKeys = OP_NEW(SSL_Server_Key_Exchange_st, ());
			break;
		case SSL_CertificateRequest  : 
			temp = CertRequest = OP_NEW(SSL_CertificateRequest_st, ());
			break;
		case SSL_Certificate_Verify   : 
			temp = OP_NEW(SSL_CertificateVerify_st, ());
			break;  
		case SSL_Client_Key_Exchange  : 
			temp = OP_NEW(SSL_Client_Key_Exchange_st, ());
			break;
		case SSL_Finished :      
			temp = OP_NEW(SSL_Finished_st, ());
			break;
		case SSL_Server_Hello_Done :      
			temp = OP_NEW(SSL_Hello_Done_st, ());
			break;
		case TLS_NextProtocol :
			temp = OP_NEW(SSL_NextProtocol_st, ());
			break;

#ifndef TLS_NO_CERTSTATUS_EXTENSION
		case TLS_CertificateStatus:
			temp = OP_NEW(TLS_CertificateStatusResponse, ());
			break;
#endif
        default : break;
		} 
		msg = temp;
		if(msg == NULL && (item != SSL_NONE && item != SSL_Server_Hello_Done  && 
			item != SSL_Hello_Request))
		{
			RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
			return OpStatus::ERR_NO_MEMORY;
		} 

		SetTag(item);
		if(connstate != NULL)
		{
			if(item == SSL_Server_Key_Exchange)
				ServerKeys->SetCommState(connstate);
			else if(item == SSL_CertificateRequest)
				CertRequest->SetCommState(connstate);
		}


		dummy.ResetRecord();
	}

	return OpStatus::OK;
}

	  
SSL_HandShakeType SSL_HandShakeMessage::GetMessage() const
{
	return ((SSL_HandShakeType) GetTag());
}

void SSL_HandShakeMessage::SetCommState(SSL_ConnectionState *item)
{
	connstate = item;
	switch (GetMessage())
	{
	case SSL_Server_Key_Exchange  : 
		ServerKeys->SetCommState(connstate);
		break;
	case SSL_CertificateRequest  : 
		CertRequest->SetCommState(connstate);
		break;
		/*    case SSL_Client_Key_Exchange  : ClientKeys->SetCommState(connstate);
		break;     */
    default : break;
	} 

	if(connstate->version.Major() < 3)
	{
		spec.enable_length = FALSE;
	}
}

SSL_KEA_ACTION SSL_HandShakeMessage::ProcessMessage(SSL_ConnectionState *pending)
{
	if(msg != NULL)
		return msg->ProcessMessage(pending);

	return SSL_KEA_No_Action;
}

void SSL_HandShakeMessage::SetUpHandShakeMessageL(SSL_HandShakeType msg_typ, SSL_ConnectionState *pending)
{
	LEAVE_IF_ERROR(SetMessage(msg_typ));

	if(msg != NULL)
		msg->SetUpMessageL(pending);
}

#endif // relevant support
