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
#include "modules/libssl/protocol/sslver.h"
#include "modules/libssl/protocol/sslplainrec.h"
#include "modules/libssl/protocol/sslcipherrec.h"
#include "modules/libssl/methods/sslhash.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#ifdef LIBSSL_ENABLE_SSL_FALSE_START
#include "modules/libssl/protocol/ssl_false_start_manager.h"
#endif // LIBSSL_ENABLE_SSL_FALSE_START

#ifdef YNP_WORK
#include "modules/libssl/debug/tstdump2.h"
#define SSL_VERIFIER
#endif

struct Handshake_actions_st : public Link
{
	int id;
	SSL_HandShakeType msg_type;
	SSL_Handshake_Status msg_status;
	SSL_Handshake_Action action;

	Handshake_actions_st(int _id, SSL_HandShakeType mtyp, SSL_Handshake_Status _stat, SSL_Handshake_Action actn):id(_id), msg_type(mtyp), msg_status(_stat), action(actn){};
};

SSL_Version_Dependent::SSL_Version_Dependent(uint8 ver_major, uint8 ver_minor)
 : version(ver_major, ver_minor), cipher_desc(NULL)
{
	Init();
}

void SSL_Version_Dependent::Init()
{
	conn_state = NULL;
	if (final_hash.Ptr())
		final_hash->ForwardTo(this);
	AddHandshakeAction(Handshake_Receive, SSL_V3_Recv_Hello_Request, SSL_Hello_Request, Handshake_Ignore, Handshake_No_Action);
	AddHandshakeAction(Handshake_Receive, SSL_V3_Recv_Server_Hello, SSL_Server_Hello, Handshake_Expected, Handshake_Handle_Message);
}

SSL_Version_Dependent::~SSL_Version_Dependent()
{
	Send_Queue.Clear();
	Receive_Queue.Clear();
}

void SSL_Version_Dependent::SetCipher(const SSL_CipherDescriptions *desc)
{
	cipher_desc = desc;
}


BOOL SSL_Version_Dependent::SendClosure() const
{
	return TRUE;
}

void SSL_Version_Dependent::AddHandshakeHash(SSL_secure_varvector32 *source)
{
	if(!source)
		return;

	SSL_secure_varvector32 *temp = OP_NEW(SSL_secure_varvector32, ());

	if(temp)
	{
		temp->ForwardTo(this);
		temp->Set(source);
		if(!temp->Error())
			temp->Into(&handshake_queue);
		else
			OP_DELETE(temp);
	}
	else
		RaiseAlert(SSL_Internal,SSL_Allocation_Failure);

	if(final_hash->HashID() != SSL_NoHash)
		final_hash->CalculateHash(*source);

#if defined _DEBUG && defined YNP_WORK
	handshake.Append(*source);
#endif
}

void SSL_Version_Dependent::GetHandshakeHash(SSL_SignatureAlgorithm alg, 
											 SSL_secure_varvector32 &target)
{
	SSL_Hash_Pointer hasher;

	hasher = SignAlgToHash(alg);

	GetHandshakeHash(hasher);

	hasher->ExtractHash(target);
}

void SSL_Version_Dependent::GetHandshakeHash(SSL_Hash_Pointer &hasher)
{
	hasher->InitHash();
	SSL_secure_varvector32 *temp= handshake_queue.First();
	while(temp)
	{
		hasher->CalculateHash(*temp);
		temp = (SSL_secure_varvector32 *) temp->Suc();
	}
}

void SSL_Version_Dependent::ClearHandshakeActions(Handshake_Queue action_queue)
{
	if(action_queue == Handshake_Receive)
		Receive_Queue.Clear();
	else
		Send_Queue.Clear();
}

void SSL_Version_Dependent::AddHandshakeAction(Handshake_Queue action_queue, SSL_V3_handshake_id id,
		SSL_HandShakeType mtyp, SSL_Handshake_Status _mstat, SSL_Handshake_Action actn,
		Handshake_Add_Point add_policy, SSL_V3_handshake_id add_id)
{
	Head *target = (action_queue == Handshake_Receive ? &Receive_Queue : &Send_Queue);
	Handshake_actions_st *item = OP_NEW(Handshake_actions_st, (id, mtyp, _mstat, actn));

	if(item == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
		return;
	}
	
	switch(add_policy)
	{
	case Handshake_Add_In_Front:
		item->IntoStart(target);
		break;
	case Handshake_Add_Before_ID:
	case Handshake_Add_After_ID:
		{
			Handshake_actions_st *action = GetHandshakeItem( action_queue, add_id);

			if(action)
			{
				if(add_policy == Handshake_Add_Before_ID)
					item->Precede(action);
				else
					item->Follow(action);
				break;
			}
			// else use default action
		}
	case Handshake_Add_At_End:
	default:
		item->Into(target);
		break;
	}
}

void SSL_Version_Dependent::RemoveHandshakeAction(Handshake_Queue action_queue, int id, Handshake_Remove_Point remove_policy)
{
	Handshake_actions_st *action = GetHandshakeItem( action_queue, id);

	if(action)
	{
		if(remove_policy == Handshake_Remove_Only_ID)
		{
			action->Out();
			OP_DELETE(action);
		}
		else
		{
			Handshake_actions_st *current_action;
			
			do{
				current_action = action;
				action = (Handshake_actions_st *) current_action->Suc();
				current_action->Out();
				OP_DELETE(current_action);
			}while(action);
		}
	}
}


SSL_Handshake_Status SSL_Version_Dependent::GetHandshakeStatus(Handshake_Queue action_queue, int id)
{
	Handshake_actions_st *action = GetHandshakeItem( action_queue, id);

	return (action ? action->msg_status : Handshake_Undecided);
}

Handshake_actions_st *SSL_Version_Dependent::GetHandshakeItem(Handshake_Queue action_queue, int id)
{
	Handshake_actions_st *action = (Handshake_actions_st *) (action_queue == Handshake_Receive ? Receive_Queue.First() : Send_Queue.First());

	while(action && action->id != id)
	{
		action = (Handshake_actions_st *) action->Suc();
	}

	return action ;
}


SSL_Handshake_Action SSL_Version_Dependent::HandleHandshakeMessage(SSL_HandShakeType mtyp)
{
	Handshake_actions_st *action = (Handshake_actions_st *) Receive_Queue.First();

	while(action)
	{
		if(action->msg_type == mtyp)
		{
			if(action->msg_status == Handshake_Expected || action->msg_status == Handshake_MustReceive)
			{
				action->msg_status = Handshake_Received;
				return action->action;
			}
			else if(action->msg_status == Handshake_Ignore)
				return action->action; // Dont bother, but return action
			break; // else unexpected message
		}
		else if(action->msg_status == Handshake_MustReceive)
			break; // unexpected message

		action = (Handshake_actions_st *) action->Suc();
	}

	RaiseAlert(SSL_Fatal, SSL_Unexpected_Message);
	return Handshake_Handle_Error;
}

SSL_Handshake_Action SSL_Version_Dependent::NextHandshakeAction(SSL_HandShakeType &mtyp)
{
	Handshake_actions_st *action = (Handshake_actions_st *) Send_Queue.First();

	while(action)
	{
		if(action->msg_status == Handshake_Block_Messages)
			break;
		if(action->msg_status == Handshake_Will_Send)
		{
			SSL_Handshake_Action actn = action->action;
			action->msg_status = Handshake_Sent;
			mtyp = action->msg_type;
			
			if(actn == Handshake_Completed)
			{
				//action = NULL; // implicit
				ClearHandshakeActions(Handshake_Send);
				ClearHandshakeActions(Handshake_Receive);
				// Preparing for renegotiation of the session
				AddHandshakeAction(Handshake_Receive, SSL_V3_Recv_Hello_Request, SSL_Hello_Request, Handshake_Expected, Handshake_Restart);
			}

			return actn;
		}
		action = (Handshake_actions_st *) action->Suc();
	}

	mtyp = SSL_NONE;
	return Handshake_No_Action;
}

SSL_Record_Base *SSL_Version_Dependent::GetRecord(SSL_ENCRYPTMODE mode) const
{
	SSL_PlainText *rec = NULL;
	switch (mode)
	{
    case SSL_RECORD_ENCRYPTED_COMPRESSED:
    case SSL_RECORD_ENCRYPTED_UNCOMPRESSED: 
		rec = OP_NEW(SSL_CipherText, ()); 
		break;
		/* Compression Disabled */
		//  case SSL_RECORD_COMPRESSED :  return new SSL_Compressed;
    case SSL_RECORD_PLAIN :        
    default: 
		rec = OP_NEW(SSL_PlainText, ());
		break;
	}

	if(rec)
		rec->SetVersion(Version());

	return rec;
}


void SSL_Version_Dependent::SessionUpdate(SSL_SessionUpdate state)
{
	switch(state)
	{
	case Session_Resume_Session:
		AddHandshakeAction(Handshake_Receive, SSL_V3_Recv_Server_Cipher_Change, SSL_NONE, Handshake_Expected, Handshake_No_Action);
		break;
	case Session_New_Session:
	//case Session_New_Export_Session:
		AddHandshakeAction(Handshake_Receive, SSL_V3_Recv_Certificate, SSL_Certificate, Handshake_MustReceive, Handshake_Handle_Message);
		// fall-through
	case Session_New_Anonymous_Session:
		AddHandshakeAction(Handshake_Receive, SSL_V3_Recv_Server_keys, SSL_Server_Key_Exchange, Handshake_Expected, Handshake_Handle_Message);
		// Anonymous Sessions Do not use 
		if(state != Session_New_Anonymous_Session)
			AddHandshakeAction(Handshake_Receive, SSL_V3_Recv_Certificate_Request, SSL_CertificateRequest, Handshake_Expected, Handshake_Handle_Message);
		AddHandshakeAction(Handshake_Receive, SSL_V3_Recv_Server_Done, SSL_Server_Hello_Done, Handshake_Expected, Handshake_Handle_Message);
		AddHandshakeAction(Handshake_Send, SSL_V3_Send_Pause, SSL_NONE, Handshake_Block_Messages, Handshake_No_Action);
		break;
	case Session_Server_Done_Received:
		RemoveHandshakeAction(Handshake_Receive, SSL_V3_Recv_Server_Hello, Handshake_Remove_From_ID);
		RemoveHandshakeAction(Handshake_Send, SSL_V3_Send_Pause, Handshake_Remove_Only_ID);
		AddHandshakeAction(Handshake_Send, SSL_V3_Send_PreMaster_Key, SSL_Client_Key_Exchange, Handshake_Will_Send, Handshake_Send_Message,Handshake_Add_Before_ID,SSL_V3_Pre_Certficate_Verify);
		AddHandshakeAction(Handshake_Send, SSL_V3_Send_Change_Cipher, SSL_NONE, Handshake_Will_Send, Handshake_ChangeCipher, Handshake_Add_After_ID, SSL_V3_Post_Certficate);
		AddHandshakeAction(Handshake_Send, SSL_V3_Send_Client_Finished, SSL_Finished, Handshake_Will_Send, Handshake_Send_Message);

#ifdef LIBSSL_ENABLE_SSL_FALSE_START
		if (g_ssl_false_start_manager->ConnectionApprovedForSSLFalseStart(conn_state))
			AddHandshakeAction(Handshake_Send, SSL_V3_Send_False_Start_Application_Data, SSL_NONE, Handshake_Will_Send, Handshake_False_Start_Send_Application_Data);
#endif // LIBSSL_ENABLE_SSL_FALSE_START

		AddHandshakeAction(Handshake_Receive, SSL_V3_Recv_Server_Cipher_Change, SSL_NONE, Handshake_Expected, Handshake_No_Action);
		break;
	case Session_Certificate_Configured:
		AddHandshakeAction(Handshake_Send, SSL_V3_Send_Certficate, SSL_Certificate, Handshake_Will_Send, Handshake_Send_Message);
		AddHandshakeAction(Handshake_Send, SSL_V3_Pre_Certficate_Verify, SSL_NONE, Handshake_Ignore, Handshake_No_Action);
		AddHandshakeAction(Handshake_Send, SSL_V3_Send_Certficate_Verify, SSL_Certificate_Verify, Handshake_Will_Send, Handshake_Send_Message);
		AddHandshakeAction(Handshake_Send, SSL_V3_Post_Certficate, SSL_NONE, Handshake_Ignore, Handshake_No_Action);
		break;
	case Session_No_Certificate:
		AddHandshakeAction(Handshake_Send, SSL_V3_Send_No_Certficate, SSL_NONE, Handshake_Will_Send, Handshake_Send_No_Certificate);
		break;
	case Session_Changed_Server_Cipher:
		AddHandshakeAction(Handshake_Receive, SSL_V3_Recv_Server_Finished, SSL_Finished, Handshake_Expected, Handshake_Handle_Message);
		break;
	case Session_Finished_Confirmed:
		if(GetHandshakeStatus(Handshake_Send, SSL_V3_Send_Change_Cipher) != Handshake_Sent)
		{
			AddHandshakeAction(Handshake_Send, SSL_V3_Send_Change_Cipher, SSL_NONE, Handshake_Will_Send, Handshake_ChangeCipher);
			AddHandshakeAction(Handshake_Send, SSL_V3_Send_Client_Finished, SSL_Finished, Handshake_Will_Send, Handshake_Send_Message);
		}
		AddHandshakeAction(Handshake_Send, SSL_V3_Send_Handshake_Complete, SSL_NONE, Handshake_Will_Send, Handshake_Completed);
		break;
	}
}

BOOL SSL_Version_Dependent::ExpectingCipherChange()
{
	if(GetHandshakeStatus(Handshake_Receive, SSL_V3_Recv_Server_Cipher_Change) != Handshake_Expected)
		return FALSE;

	RemoveHandshakeAction(Handshake_Receive, SSL_V3_Recv_Server_Cipher_Change);
	return TRUE;
}

#endif
