/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/util/str.h"
#include "modules/url/url_sn.h"
#include "modules/libssl/protocol/op_ssl.h"
#include "modules/libssl/ui/sslcctx.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/libssl/debug/tstdump2.h"


#define COMM_CONNECTION_FAILURE 0x02
//#define _DEBUGSSL_HANDHASH
#ifdef _DEBUG
#ifdef YNP_WORK
#define _DEBUG_ERR
//#define TST_DUMP
#ifdef TST_DUMP
#endif
#endif
#endif

#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define SSL_ERRSTR(p,x) Str::##p##_##x
#else
#define SSL_ERRSTR(p,x) x
#endif

SSL_STATE SSL::Handle_Start_Handshake(SSL_HandShakeMessage &,SSL_STATE pref_next_state, BOOL ignore_any_ongoing_session_negotiation)
{
	buffered_amount_application_data = 0;
	buffered_amount_raw_data = 0;

	SSL_HandShakeMessage sent_client_hello;
	SSL_ProtocolVersion record_protocol;
	SSL_ProtocolVersion send_ver;

	SSL_SessionStateRecordList *session;

	session = pending_connstate->session;

	// Remove any existing timeout message.
	mh->RemoveDelayedMessages(MSG_SSL_WAITING_SESSION_TMEOUT, Id());

	if(session != NULL)
	{
		UINT8 major, minor;
		session->used_version.Get(major, minor);
		if (g_securityManager->IsEnabled(major, minor) == FALSE)
		{
			// The protocol for this session has been disabled. Restart session.
			pending_connstate->session->is_resumable = FALSE;
			if(connstate->session)
			{
				connstate->session->is_resumable = FALSE;
			}
			FlushBuffers();
			current_state = Close_Connection(SSL_PRE_CLOSE_CONNECTION);
			trying_to_reconnect = TRUE;
			if(!mh->HasCallBack(this, MSG_DO_TLS_FALLBACK, Id()))
				mh->SetCallBack(this, MSG_DO_TLS_FALLBACK, Id());
			mh->PostMessage(MSG_DO_TLS_FALLBACK,Id(),1);
			return current_state;
		}

		if(session->is_resumable && ignore_any_ongoing_session_negotiation && !session->session_negotiated)
			session->is_resumable = FALSE;

		if(!session->is_resumable)
		{
			session = pending_connstate->FindSessionRecord();
			if (session != NULL)
			{
				OP_ASSERT( session->is_resumable);
				session->connections++;
			}
			else
			{
				flags.allow_auto_disable_tls = TRUE;
			}
		}

		if(session && !session->session_negotiated)
		{

			if (OpStatus::IsError(server_info->AddNegotiationListener(this)))
				return SSL_CLOSE_CONNECTION;

			// 20 seconds timeout on waiting for session. After that this
			// connection will start a new session negotiation on its own.
			mh->PostDelayedMessage(MSG_SSL_WAITING_SESSION_TMEOUT, Id(), 0, 20000);

			return SSL_WAITING_FOR_SESSION;
		}

		if(session != NULL)
		{
			flags.allow_auto_disable_tls = !server_info->TLSDeactivated();
		}
	}

	SetProgressInformation(START_SECURE_CONNECTION,0, (void *) servername->UniName());
	SetRequestMsgMode(START_SECURE_CONNECTION);

	if(session == NULL)
	{
		OP_MEMORY_VAR TLS_Feature_Test_Status feature_status = server_info->GetFeatureStatus();

		// In case the protocol has been turned off since the previous test.
		if (g_securityManager->IsTestFeatureAvailable(feature_status) == FALSE)
			feature_status = TLS_Untested;

		if (feature_status == TLS_Untested)
		{
			// Start in TLS 1.2 mode; only the highest enabled version will be indicated to the server anaway
			feature_status = TLS_TestMaxVersion;
		}

		OP_ASSERT(feature_status != TLS_Untested);

		server_info->SetFeatureStatus(feature_status);

		OP_STATUS status = send_ver.SetFromFeatureStatus(feature_status); // Max enabled version, based on feature status
		if(OpStatus::IsError(status))
		{
			RaiseAlert(status);
			return SSL_PREHANDSHAKE_WAITING;
		}

		// Sets qualified guess for record protocol
		record_protocol = server_info->GetKnownToSupportVersion();
		if (record_protocol > send_ver)
			record_protocol = send_ver;
	}
	else // session != NULL
	{
		uint8 major, minor;
		session->used_version.Get(major, minor);
		OP_ASSERT(g_securityManager->IsEnabled(major, minor));

		record_protocol = server_info->GetKnownToSupportVersion();
		send_ver.Set(major,minor);

		if(!security_profile->ciphers.Contain(session->used_cipher))
		{
			session = NULL;
		}

		if(session == NULL)
			pending_connstate->RemoveSession();
	}

	if(session == NULL)
	{
		pending_connstate->OpenNewSession();
		if(ErrorRaisedFlag)
			return SSL_PREHANDSHAKE_WAITING;

		session = pending_connstate->session;
		if(connstate->session && connstate->session->session_negotiated)
			session->renegotiation_extension_supported = connstate->session->renegotiation_extension_supported;
	}

	pending_connstate->AddSessionRecord(session);

	if(pending_connstate->server_info->UseReducedDHE())
		pending_connstate->sent_ciphers.CopyCommon(g_securityManager->DHE_Reduced_CipherSuite, security_profile->ciphers, TRUE);
	else
		pending_connstate->sent_ciphers = security_profile->ciphers;
	pending_connstate->sent_compression = security_profile->compressions;


	connstate->SetVersion(record_protocol);
	if(ErrorRaisedFlag)
		return SSL_PREHANDSHAKE_WAITING;

	pending_connstate->sent_version = send_ver;
	sent_client_hello.SetCommState(connstate);
	TRAPD(op_err, sent_client_hello.SetUpHandShakeMessageL(SSL_Client_Hello, pending_connstate));
	if(OpStatus::IsError(op_err))
	{
		RaiseAlert(op_err);
		return SSL_PREHANDSHAKE_WAITING;
	}
	if(sent_client_hello.Error())
	{
		RaiseAlert(sent_client_hello);
		return SSL_PREHANDSHAKE_WAITING;
	}

	switch(server_info->GetFeatureStatus())
	{
	case TLS_Test_1_2_Extensions:
	//case TLS_Test_1_1_Extensions:
	case TLS_Test_1_0:
	case TLS_Test_1_0_Extensions:
	case TLS_Test_SSLv3_only:
		if(OpStatus::IsSuccess(g_main_message_handler->SetCallBack(this, MSG_COMM_LOADING_FAILED, (MH_PARAM_1) this)))
		{
			g_main_message_handler->PostDelayedMessage(MSG_COMM_LOADING_FAILED, (MH_PARAM_1) this, SSL_ERRSTR(SI,ERR_HTTP_TIMEOUT), 7*1000);
		}
		break;
	}

	return SendMessageRecord(sent_client_hello,pref_next_state);
}

SSL_STATE SSL::Close_Connection(SSL_STATE pref_next_state)
{
	BOOL dont_send= FALSE;

	if (flags.is_closing_connection)
		return pref_next_state;

	flags.is_closing_connection = TRUE;
	g_main_message_handler->UnsetCallBack(this, MSG_SSL_COMPLETEDVERIFICATION);


	connstate->BroadCastSessionNegotiatedEvent(FALSE);

	/**
	 * For speeding up HTTPS closing we skip graceful close (sending/receiving close alert messages) except
	 * when last socket operation was write. When closing after write we send close alert to signal to server
	 * that it has received all data. Note that this rarely happens.
	 *
	 * This optimization only have effect if the server closes connection after every request response. In these cases,
	 * removing graceful close removes several TCP round trips and it enables us to send the next request quicker.
	 * Synthetic tests show up to 70% load improvement. However, in real page loads we expect that number to be lower,
	 * as many other factors counts in.
	 *
	 * Testing has not seen any issues, and implementation practice is to ignore the gracefully close, or not act when
	 * the graceful close is missing.
	 */
	if(!flags.closing_connection && m_last_socket_operation == LAST_SOCKET_OPERATION_WRITE)
	{
		// Close gracefully, by sending and receiving close alert messages
		PauseApplicationData();
		if(ErrorRaisedFlag)
		{
			Handle_Raised_Error(pref_next_state);
			flags.closed_with_error = (flags.internal_error_message_set ? 2 : 1);
		}

		if(!ProtocolComm::Closed()
           && connstate != NULL
           && connstate->version_specific != NULL
           && connstate->version_specific->SendClosure())
		{
			// activate send queue, but remove queued records
			StartingToSetUpRecord(FALSE, TRUE);
			SSL_Alert alert(SSL_Warning,SSL_Close_Notify);
			pref_next_state = SendMessageRecord( alert,pref_next_state);
			ForceFlushPrioritySendQueue();
		}
		flags.closing_connection = TRUE;
		if(!flags.closed_with_error && !EmptyBuffers(TRUE) && mh != NULL)
		{
			mh->PostMessage(MSG_SSL_WAIT_EMPTY_BUFFER,Id(),2);

			flags.is_closing_connection = FALSE;
			return (pref_next_state != SSL_CONNECTED ? SSL_CLOSE_CONNECTION2 : SSL_CLOSE_CONNECTION);
		}
	}

	if(!flags.closed_with_error && !EmptyBuffers(TRUE))
	{
		flags.is_closing_connection = FALSE;

		return (pref_next_state != SSL_CLOSE_CONNECTION ?
			(current_state == SSL_NOT_CONNECTED ?SSL_NOT_CONNECTED : SSL_CLOSE_CONNECTION2)
			: SSL_CLOSE_CONNECTION);
	}

	if(current_state != SSL_CLOSE_CONNECTION && current_state != SSL_NOT_CONNECTED && current_state != SSL_CONNECTED)
	{
		if(!((current_state == SSL_SENT_CLIENT_HELLO ||
			current_state == SSL_NEGOTIATING  ||
			((flags.allow_auto_disable_tls &&
				!server_info->TLSDeactivated() &&
				pending_connstate->sent_version.Compare(3,0) > 0) ||
				(pending_connstate->version.Compare(3,0) > 0 &&
				pending_connstate->session->used_correct_tls_no_cert))
			))
			)
		{
			// Will not block
			current_state =Handle_Local_Error(SSL_Fatal,SSL_Handshake_Failure,SSL_CLOSE_CONNECTION2,FALSE);
		}
		else if(!(trying_to_reconnect && server_info->TLSUseSSLv3NoCert()) &&
			(g_securityManager && !g_securityManager->Enable_SSL_V3_0))
		{
			// Will not block
			current_state =Handle_Local_Error(SSL_Fatal,SSL_Handshake_Failure,SSL_CLOSE_CONNECTION2,FALSE);
		}
		else if(trying_to_reconnect)
			dont_send= TRUE;
	}

	if(ProtocolComm::Closed()
       && connstate != NULL
       && connstate->version_specific != NULL
       && !connstate->version_specific->SendClosure())
		current_state = SSL_NOT_CONNECTED;

	FlushBuffers(dont_send);
	Stop();
	if(mh != NULL && !dont_send && !trying_to_reconnect)
	{
		if(!flags.closed_with_error)
		{
			mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
		}
/*		else if(flags.closed_with_error == 3)
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), ERR_SSL_CONN_RESTART_SUGGESTED);*/
		else
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), (flags.closed_with_error > 1 ? (int)Str::S_SSL_FATAL_ERROR_MESSAGE : (int)ERR_SSL_ERROR_HANDLED));
		current_state = SSL_NOT_CONNECTED;
	}
	flags.is_closing_connection = FALSE;

	return current_state;
}

SSL_STATE SSL::Handle_Local_Error(SSL_AlertLevel lev,SSL_AlertDescription des,
                                  SSL_STATE pref_nex_state,BOOL usrknow)
{
	SSL_Alert err(lev,des);

	return Handle_Local_Error(err,pref_nex_state,usrknow);
}

SSL_STATE SSL::Handle_Raised_Error(SSL_AlertLevel lev,SSL_AlertDescription des,
                                   SSL_STATE pref_nex_state,BOOL usrknow)
{
	SSL_Alert msg(lev,des);

	Error(&msg);
	ResetError();

	return Handle_Local_Error(msg,pref_nex_state,usrknow);
}

SSL_STATE SSL::Handle_Raised_Error(SSL_STATE pref_nex_state,BOOL usrknow)
{
	SSL_Alert msg;

	Error(&msg);
	ResetError();

	return Handle_Local_Error(msg,pref_nex_state,usrknow);
}

SSL_STATE SSL::Handle_Local_Error(SSL_Alert &msg, SSL_STATE pref_nex_state,BOOL usrknow)
{
	SSL_Alert msg2;// = msg;
	BOOL send;
	BOOL cont;

	msg2 = msg;
	cont = FALSE;

	SSL_AlertDescription descr = msg.GetDescription();

#ifdef _DEBUG_ERR
	PrintfTofile("ssltrans.txt"," ID %d : Local Error %d (%x)\r\n\r\n",Id(),((int) msg.GetDescription())&0xff,((int) msg.GetDescription()));
#endif
	if(descr != SSL_Authentication_Only_Warning && // Do not use TLS fallback for authenitcation only warning
		(current_state == SSL_SENT_CLIENT_HELLO ||
		current_state == SSL_NEGOTIATING ||
		descr == SSL_Insufficient_Security1 ) &&
		flags.allow_auto_disable_tls &&
		!server_info->TLSDeactivated() &&
		pending_connstate->sent_version.Compare(3,0) > 0
		)
	{
		trying_to_reconnect = TRUE;
		if(pending_connstate->version < SSL_ProtocolVersion(3,1) && !pending_connstate->session->renegotiation_extension_supported)
		{
			server_info->SetTLSDeactivated(TRUE);
			server_info->SetFeatureStatus(TLS_SSL_v3_only);
			server_info->SetPreviousSuccesfullFeatureTest(TLS_SSL_v3_only);
			server_info->SetValidTo(g_timecache->CurrentTime() + SSL_VERSION_TEST_VALID_TIME );
		}
		else if(descr == SSL_Insufficient_Security1)
		{
			SendMessageRecord( msg,pref_nex_state);
			// Reconnect, no need to do anything more.
		}
		else
		{
			switch(server_info->GetFeatureStatus())
			{
#ifdef _SUPPORT_TLS_1_2
			case TLS_Test_1_2_Extensions:
				server_info->SetValidTo(g_timecache->CurrentTime() + SSL_VERSION_TEST_VALID_TIME );
				server_info->SetFeatureStatus(((current_state == SSL_SENT_CLIENT_HELLO || current_state == SSL_NEGOTIATING) && 
								pending_connstate->version.Compare(3,3) >= 0) ? 
								TLS_1_2_and_Extensions : TLS_Test_1_0_Extensions);
				break;
#endif
				/*
			case TLS_Test_1_1_Extensions:
				server_info->SetValidTo(g_timecache->CurrentTime() + SSL_VERSION_TEST_VALID_TIME );
				server_info->SetFeatureStatus(g_securityManager->Enable_TLS_V1_0  &&
									(current_state == SSL_SENT_CLIENT_HELLO || current_state == SSL_NEGOTIATING) && 
									pending_connstate->version.Compare(3,2) < 0 ? TLS_Test_1_0_Extensions : TLS_1_1_and_Extensions);
				break;
				*/
			case TLS_Test_1_0_Extensions:
				// Failure -> Test 1.0 without extensions
				server_info->SetFeatureStatus(TLS_Test_1_0);
				break;
			case TLS_Test_1_0:
				server_info->SetValidTo(g_timecache->CurrentTime() + SSL_VERSION_TEST_VALID_TIME );
				server_info->SetFeatureStatus(((current_state == SSL_SENT_CLIENT_HELLO || current_state == SSL_NEGOTIATING) && 
								pending_connstate->version.Compare(3,1) >= 0) ? 
								TLS_1_0_only : TLS_Test_SSLv3_only);
				break;
			default:
				// Already decided protocol type, no need to reconnect
				trying_to_reconnect = FALSE;
				goto dont_reconnect;

			}
		}
		return Close_Connection(pref_nex_state);
	}
dont_reconnect:;

	if(!flags.closed_with_error && (current_state != SSL_CONNECTED || ProtocolComm::IsActiveConnection()))
	{
		if(msg2.GetLevel() == SSL_Warning){
			if (!usrknow)
				ApplicationHandleWarning(msg2,FALSE,cont);
			else
				cont = TRUE;
		}
		else
		{
			if (!usrknow)
				ApplicationHandleFatal(msg2,FALSE);
			cont = FALSE;
		}
	}

	if(current_state != SSL_CLOSE_CONNECTION && current_state != SSL_CLOSE_CONNECTION2 && current_state != SSL_NOT_CONNECTED && current_state != SSL_PRE_CLOSE_CONNECTION)
	{
		send = (connstate != NULL && connstate->version_specific != NULL && connstate->version_specific->SendAlert(msg2, cont));

		if(send && (msg2.GetLevel() == SSL_Warning || msg2.GetLevel() == SSL_Fatal ))
			pref_nex_state = SendMessageRecord(msg2,pref_nex_state);
	}
	if(msg2.GetLevel() != SSL_Warning || !cont)
	{
		if(connstate!= NULL && connstate->session != NULL)
			connstate->session->is_resumable = FALSE;
		if(pending_connstate != NULL && pending_connstate->session != NULL)
			pending_connstate->session->is_resumable = FALSE;
		/*if(!trying_to_reconnect && msg2.GetDescription() == SSL_Insufficient_Security1)
			flags.closed_with_error = 3;
		else*/
			flags.closed_with_error = (flags.internal_error_message_set ? 2 : 1);
		pref_nex_state = Close_Connection(pref_nex_state);
	}
	return pref_nex_state;
}

void SSL::CalculateHandshakeHash(SSL_secure_varvector32 *source)
{
	SSL_Version_Dependent *version_specific;
	SSL_secure_varvector32 *temp;

	version_specific = pending_connstate->version_specific;
	if(version_specific != NULL)
	{
		while(!pending_connstate->handshake_queue.Empty())
		{
			temp = pending_connstate->handshake_queue.First();
			version_specific->AddHandshakeHash(temp);
			OP_DELETE(temp);
		}
		if(source!= NULL)
			version_specific->AddHandshakeHash(source);
	}
	else if(source!= NULL)
	{
		temp = OP_NEW(SSL_secure_varvector32, ());

		if(temp)
		{
			temp->Set(source);
			if(!temp->Error())
				temp->Into(&pending_connstate->handshake_queue);
			else
				OP_DELETE(temp);
		}
		else
			RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
	}
}

SSL_STATE SSL::SendMessageRecord(SSL_ContentType type, DataStream &item, BOOL hashmessage, SSL_STATE a_pref_next_state)
{
	OP_MEMORY_VAR  SSL_STATE pref_next_state = a_pref_next_state;
	OpAutoPtr<SSL_secure_varvector32> target(OP_NEW(SSL_secure_varvector32, ()));

	if(target.get() == NULL)
	{
		RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
		return SSL_PRE_CLOSE_CONNECTION;
	}

	target->SetTag(type);

	SSL_TRAP_AND_RAISE_ERROR_THIS(item.WriteRecordL(target.get()));
	if(!ErrorRaisedFlag)
	{
		if(hashmessage)
			CalculateHandshakeHash(target.get());

		if(!flags.closing_connection)
			Send(target.release());
	}
	else
		pref_next_state = SSL_PRE_CLOSE_CONNECTION;

	//target.reset();

	return pref_next_state;

}

SSL_STATE SSL::SendMessageRecord(SSL_ChangeCipherSpec_st &item,SSL_STATE pref_next_state)
{
	return SendMessageRecord(SSL_ChangeCipherSpec, item, FALSE, pref_next_state);
}

SSL_STATE SSL::SendMessageRecord(SSL_HandShakeMessage &item,SSL_STATE pref_next_state)
{
	SSL_ContentType type = SSL_Handshake;
	return SendMessageRecord(type, item, (item.GetMessage() != SSL_Hello_Request), pref_next_state);
}

SSL_STATE SSL::SendMessageRecord(SSL_Alert &item,SSL_STATE pref_next_state)
{
	return SendMessageRecord((item.GetLevel() == SSL_Warning ? SSL_Warning_Alert_Message : SSL_AlertMessage), item, FALSE, pref_next_state);
}

void SSL::SendRecord(SSL_ContentType type,byte *source,uint32 blen)
{
	SSL_secure_varvector32 *tempbuffer = OP_NEW(SSL_secure_varvector32, ());
	if(tempbuffer == NULL)
	{
		OP_DELETEA(source); // clean up constructor does not delete
		RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
		return;
	}

	tempbuffer->SetTag(type);
	tempbuffer->SetExternalPayload(source, TRUE, blen);

	Send(tempbuffer);
}

void SSL::SendData(char *source,unsigned blen)
{
	buffered_amount_application_data += blen;

	OP_ASSERT(EvaluateHSTSPassCondition() || !"If server is strict transport security enabled, the security state must be FULL or higher");

	SendRecord(SSL_Application,(byte *)source, (uint32) blen);
}

SSL_STATE SSL::Handle_SendChangeCipher(SSL_STATE pref_next_state)
{
	SSL_ChangeCipherSpec_st temp;
	pref_next_state = SendMessageRecord(temp,pref_next_state);
	SendRecord(SSL_PerformChangeCipher, NULL, 0);
	return pref_next_state;
}

SSL::SSL(MessageHandler* msg_handler, ServerName* hostname, unsigned short portnumber, BOOL do_record_splitting)
		 : SSL_Record_Layer(msg_handler,hostname,(portnumber ? portnumber : 443), do_record_splitting)
{
	InternalConstruct();
}

void SSL::InternalConstruct()
{
	buffered_amount_application_data = 0;
	buffered_amount_raw_data = 0;
	security_profile = NULL;
	flags.closing_connection =  flags.closed_with_error = flags.use_sslv3_handshake =
	flags.is_closing_connection = flags.application_records_permitted =
	flags.delayed_client_certificate_request  = flags.internal_error_message_set =
	flags.received_closure_alert = flags.asking_password = FALSE;

	current_state = SSL_NOT_CONNECTED;
	flags.allow_auto_disable_tls = TRUE;

	certificate_ctx = NULL;
}

void SSL::Stop()
{
	SSL_STATE actual_current_state= current_state;

	if(!ProcessingFinished() ||
		(current_state != SSL_NOT_CONNECTED &&
		current_state != SSL_CLOSE_CONNECTION2 &&
		current_state != SSL_CLOSE_CONNECTION))
	{
		/*
		if(flags.closing_connection && flags.closed_with_error && !server_info->IsIIS4Server() ) // IIS 4 servers apparently does not tell when they close the connection
		{
			if(connstate!= NULL && connstate->session != NULL)
				connstate->session->is_resumable = FALSE;
			if(pending_connstate != NULL && pending_connstate->session != NULL)
				pending_connstate->session->is_resumable = FALSE;
		}
		*/
		if(!EmptyBuffers(FALSE))
		{
			current_state = Close_Connection(current_state);
		}
	}
    if(pending_connstate != NULL && pending_connstate->session != NULL &&
		!pending_connstate->session->session_negotiated)
		pending_connstate->session->is_resumable = FALSE;

	if(current_state != SSL_NOT_CONNECTED && !flags.closing_connection)
		current_state = Close_Connection(current_state);
	if(current_state != SSL_RETRY)
		current_state = SSL_NOT_CONNECTED;

	SSL_Record_Layer::Stop();

	if(((trying_to_reconnect && server_info->TLSUseSSLv3NoCert()) ||
		((actual_current_state == SSL_SENT_CLIENT_HELLO ||
		actual_current_state == SSL_NEGOTIATING) &&
		pending_connstate != NULL &&
		pending_connstate->version.Compare(3,0) > 0 &&
		pending_connstate->session->used_correct_tls_no_cert)
		)
		&& ProtocolComm::Closed())
	{

		if(!trying_to_reconnect)
		{
			BOOL allowed = FALSE;
			SetProgressInformation(RESTART_LOADING,0, &allowed);
			if(allowed)
			{
				pending_connstate->session->use_correct_tls_no_cert = FALSE;
				server_info->SetTLSUseSSLv3NoCert(TRUE);
				trying_to_reconnect = TRUE;
				flags.allow_auto_disable_tls = FALSE;
				flags.closing_connection =  FALSE;
				flags.closed_with_error = 0;
				current_state = SSL_RETRY;
				mh->SetCallBack(this, MSG_DO_TLS_FALLBACK, Id());
				mh->PostMessage(MSG_DO_TLS_FALLBACK,Id(),0);
			}
			else
			{
				trying_to_reconnect = FALSE;
				RaiseAlert(SSL_Fatal, SSL_Handshake_Failure);
			}
		}
	}
	else if((trying_to_reconnect || ((actual_current_state == SSL_SENT_CLIENT_HELLO ||
		actual_current_state == SSL_NEGOTIATING) &&
		flags.allow_auto_disable_tls &&
		(!server_info->TLSDeactivated() || trying_to_reconnect) &&
		pending_connstate != NULL &&
		(pending_connstate->sent_version.Major()>3 	||
		(/*pending_connstate->sent_version.Compare(3,0)>0  &&*/
		(actual_current_state == SSL_SENT_CLIENT_HELLO ||
		pending_connstate->version != pending_connstate->sent_version
		) )) ))
		&& ProtocolComm::Closed())
	{
		{
			if(g_securityManager->Enable_SSL_V3_0 && pending_connstate->version < SSL_ProtocolVersion(3,1) && !pending_connstate->session->renegotiation_extension_supported)
			{
				server_info->SetTLSDeactivated(TRUE);
				server_info->SetValidTo(g_timecache->CurrentTime() + SSL_VERSION_TEST_VALID_TIME );
				server_info->SetFeatureStatus(TLS_SSL_v3_only);
			}
			else if(!trying_to_reconnect)
			{
				SSL_Options *security_manager = g_securityManager;
				TLS_Feature_Test_Status feature_status = server_info->GetFeatureStatus();

				if (security_manager->IsTestFeatureAvailable(server_info->GetPreviousSuccesfullFeatureTest()))
				{
					feature_status = server_info->GetPreviousSuccesfullFeatureTest();
				}
				else
				{
					switch(feature_status)
					{
					case TLS_Test_1_2_Extensions:
						if (security_manager->IsTestFeatureAvailable(TLS_Test_1_0_Extensions))
						{
							feature_status = TLS_Test_1_0_Extensions; // Could try plain TLS 1.2, but assume that a server that do not accept 1.2 with extensions won't support plain TLS 1.2

							break;
						} // Fall through
						/*
					case TLS_Test_1_1_Extensions:
						if (security_manager->IsEnabled(TLS_Test_1_0_Extensions))
						{
							feature_status = TLS_Test_1_0_Extensions; // Could try TLS 1.1, but assume that a server that do not accept 1.1 with extensions won't support plain TLS 1.1
							break;
						} // Fall through
						*/
					case TLS_Test_1_0_Extensions:
						if (security_manager->IsTestFeatureAvailable(TLS_Test_1_0))
						{
							feature_status = TLS_Test_1_0;
							break;
						} // Fall through
					case TLS_Test_1_0:
						if (security_manager->IsTestFeatureAvailable(TLS_Test_SSLv3_only))
						{
							if(actual_current_state == SSL_SENT_CLIENT_HELLO)
							{
								feature_status = TLS_Test_SSLv3_only;
								break;
							}
							else
							{
								server_info->SetFeatureStatus(TLS_1_0_only);
								trying_to_reconnect = FALSE;
								RaiseAlert(SSL_Fatal, SSL_Handshake_Failure);
								return;
							}
						} // Fall through
					case TLS_Test_SSLv3_only:

						if(actual_current_state == SSL_SENT_CLIENT_HELLO)
						{
							trying_to_reconnect = FALSE;
							RaiseAlert(SSL_Fatal, SSL_Not_TLS_Server);
							return;
						}
						else if (security_manager->IsTestFeatureAvailable(TLS_Test_SSLv3_only))
						{
							feature_status = TLS_SSL_v3_only;
							break;
						} // Fall through
					default:
						trying_to_reconnect = FALSE;
						RaiseAlert(SSL_Fatal, SSL_Handshake_Failure);
						return;
					}
				}
				server_info->SetFeatureStatus(feature_status);
				server_info->SetValidTo(g_timecache->CurrentTime() + SSL_VERSION_TEST_VALID_TIME );
			}

			trying_to_reconnect = TRUE;
			flags.closing_connection =  FALSE;
			flags.closed_with_error =  0;
			current_state = SSL_RETRY;
			mh->SetCallBack(this, MSG_DO_TLS_FALLBACK, Id());
			mh->PostMessage(MSG_DO_TLS_FALLBACK,Id(),0);
		}
	}
}

SSL::~SSL()
{
	InternalDestruct();
}

void SSL::InternalDestruct()
{
	if (server_info != NULL) // BREW compiler requires an explicit check with NULL for OpSmartPointer.
		server_info->RemoveNegotiationListener(this);
#ifdef API_LIBSSL_DECRYPT_WITH_SECURITY_PASSWD
	if(flags.asking_password)
		g_securityManager->EndSecurityPasswordSession();
#endif // API_LIBSSL_DECRYPT_WITH_SECURITY_PASSWD
	if(g_windowManager != NULL && mh != NULL)
		mh->UnsetCallBacks(this);
	current_state = SSL_NOT_CONNECTED;
	security_profile = NULL;

	OP_DELETE(certificate_ctx);
	certificate_ctx = NULL;
}

#endif
