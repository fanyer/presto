/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"
#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/libssl/sslbase.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/util/str.h"
#include "modules/url/url_sn.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/protocol/op_ssl.h"
#include "modules/libssl/ui/sslcctx.h"
#include "modules/libssl/keyex/sslkeyex.h"
#include "modules/libssl/method_disable.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/console/opconsoleengine.h"


SSL_STATE SSL::Handle_HandShake(SSL_STATE a_pref_next_state)
{
	OP_MEMORY_VAR SSL_STATE pref_next_state = a_pref_next_state;
	SSL_HandShakeType recordtype;
	SSL_Handshake_Action action_type;

	if(pending_connstate->key_exchange == NULL || pending_connstate->version_specific == NULL)
	{
		pending_connstate->SetVersion(connstate->version);
		if(ErrorRaisedFlag)
			return SSL_PRE_CLOSE_CONNECTION;
	}

	DataStream *source = GetRecord();

	OP_ASSERT(source);

	pref_next_state = SSL_NEGOTIATING;
	while (source->MoreData())
	{
		OP_ASSERT(pending_connstate->version_specific);
		loading_handshake.SetCommState(pending_connstate);

		OP_MEMORY_VAR OP_STATUS op_err = OpRecStatus::OK;
		SSL_TRAP_AND_RAISE_ERROR_THIS(op_err = loading_handshake.ReadRecordFromStreamL(source));

		if(ErrorRaisedFlag)	
		{
			loading_handshake.SetMessage(SSL_NONE);
			loading_handshake.ResetRecord();
			return SSL_PRE_CLOSE_CONNECTION;
		}

		if(op_err != OpRecStatus::FINISHED)
			continue;

		if(loading_handshake.GetMessage() != SSL_Hello_Request)
		{
			SSL_secure_varvector32 temp_target;

			loading_handshake.WriteRecordL(&temp_target);
			CalculateHandshakeHash(&temp_target);
		}

		{
			SSL_Alert msg;

			if(!loading_handshake.Valid(&msg))
			{
				msg.SetLevel(SSL_Fatal);
				RaiseAlert(msg);
				return SSL_PRE_CLOSE_CONNECTION;
			}
		}

		recordtype = loading_handshake.GetMessage();

		action_type = pending_connstate->version_specific->HandleHandshakeMessage(recordtype);
		switch(action_type)
		{
		case Handshake_Handle_Error:
			OpStatus::Ignore(loading_handshake.SetMessage(SSL_NONE));
			loading_handshake.ResetRecord();
			return SSL_PRE_CLOSE_CONNECTION;

		case Handshake_Restart:
			OpStatus::Ignore(loading_handshake.SetMessage(SSL_NONE));
			loading_handshake.ResetRecord();
			return StartNewHandshake(pref_next_state);

		case Handshake_Handle_Message:
			pref_next_state = Process_Handshake_Message(pref_next_state);
			break;
		}

		OpStatus::Ignore(loading_handshake.SetMessage(SSL_NONE));
		loading_handshake.ResetRecord();

		if(pref_next_state >= SSL_Wait_ForAction_Start && pref_next_state <= SSL_Wait_ForAction_End)
			break;
	}

	return pref_next_state;
}

BOOL SSL::EvaluateHSTSPassCondition()
{
	// If server is HSTS, we continue if security is at least full.
	if (!servername->SupportsStrictTransportSecurity() || connstate->session->security_rating >= SECURITY_STATE_FULL)
		return TRUE;

	/* Server is http strict transport server (HSTS) and we do not have full security. */
	if (connstate->session->security_rating <= SECURITY_STATE_LOW)
		return FALSE;

	/**
	 * Server is http strict transport server (HSTS) and we do not have full security.
	 *
	 * According to spec, we are not allowed to have nothing but full security for
	 * HSTS servers. However, OCSP/CRL url retrieval can sometimes  fail which would
	 * block the website. To be compatible with other browsers, we need to let the
	 * connection continue even for failing OCSP/CRL urls.
	 *
	 * Thus we let failing ocsp/crl urls be an exception for this.
	 *
	 * Padlock will not show, as with non-HSTS connections.
	 *
	 * Revoked certificates will of course still block the connection.
	 */
	unsigned int ocsp_crl_low_reason_flags =
			SECURITY_LOW_REASON_UNABLE_TO_OCSP_VALIDATE |
			SECURITY_LOW_REASON_OCSP_FAILED             |
			SECURITY_LOW_REASON_UNABLE_TO_CRL_VALIDATE  |
			SECURITY_LOW_REASON_CRL_FAILED;

	if ((connstate->session->low_security_reason & (~ocsp_crl_low_reason_flags)) == 0)
	{
		// No other warning flags than the OCSP or CRL ones. Thus, we let the connection continue with these warnings.
		return TRUE;
	}

	// Security was lowered, and the connection should be blocked since this is a HSTS server.
	return FALSE;
}

void SSL::PostConsoleMessage(Str::LocaleString str, int errorcode)
{
#ifdef OPERA_CONSOLE
	if (g_console /*&& g_console->IsLogging()*/)
	{
		OpConsoleEngine::Message cmessage(OpConsoleEngine::Network, OpConsoleEngine::Information);
		cmessage.error_code = errorcode;

		URL url;
		SetProgressInformation(GET_APPLICATIONINFO, 0, &url);
		if(url.IsValid())
			url.GetAttribute(URL::KUniName, cmessage.url);

		SetLangString(str, cmessage.message);

		TRAPD(op_err, g_console->PostMessageL(&cmessage));
		OpStatus::Ignore(op_err);
	}
#endif // OPERA_CONSOLE
}


SSL_STATE SSL::StartNewHandshake(SSL_STATE nextstate)
{
	if(!connstate->session->renegotiation_extension_supported)
	{
		if((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & 
		(CRYPTO_METHOD_RENEG_DISABLE_UNPATCHED_RENEGO | CRYPTO_METHOD_RENEG_WARN | CRYPTO_METHOD_RENEG_REFUSE)) != 0)
		{
			// In warn mode about no renego protection do not permit such servers to renegotiate
			PostConsoleMessage(Str::S_SSL_SERVER_ATTEMPTED_UNSECURE_RENEGOTIATION, (int) SSL_No_Renegotiation);

			SSL_Alert warning(SSL_Warning,SSL_No_Renegotiation);
			return SendMessageRecord(warning, SSL_CONNECTED);
		}
		else 
			PostConsoleMessage(Str::S_SSL_SERVER_PERFORMED_UNSECURE_RENEGOTIATION);
	}

	nextstate = SSL_PREHANDSHAKE;

	g_main_message_handler->UnsetCallBack(this, MSG_SSL_COMPLETEDVERIFICATION);
	OP_DELETE(pending_connstate->key_exchange);
	pending_connstate->key_exchange = NULL;
	OP_DELETE(pending_connstate->version_specific);
	pending_connstate->version_specific= NULL;
	pending_connstate->handshake_queue.Clear();
	PauseApplicationData();

	SSL_SessionStateRecordList *tempsession = pending_connstate->FindSessionRecord();
	if (tempsession != NULL)
	{
		tempsession->connections++;
	}

	// Probably the same already, but make sure.
	pending_connstate->last_client_finished = connstate->last_client_finished;
	pending_connstate->last_server_finished = connstate->last_server_finished;

	nextstate = Handle_Start_Handshake(loading_handshake, nextstate, FALSE);
	flags.allow_auto_disable_tls = FALSE;

	return nextstate;
}

SSL_STATE SSL::Process_Handshake_Message(SSL_STATE nextstate)
{
	SSL_KEA_ACTION kea_action;

	kea_action = loading_handshake.ProcessMessage(pending_connstate);
	return Process_KeyExchangeActions(nextstate, kea_action);
}


SSL_STATE SSL::Process_KeyExchangeActions(SSL_STATE nextstate, SSL_KEA_ACTION kea_action)
{
	SSL_KeyExchange *key_exchange;
	SSL_Alert msg;

	key_exchange = pending_connstate->key_exchange;

	while(kea_action != SSL_KEA_No_Action)
	{
		switch(kea_action)
		{
		case SSL_KEA_Application_Delay_Certificate_Request:
			flags.delayed_client_certificate_request = TRUE;
			kea_action = SSL_KEA_No_Action;
			break;
		case SSL_KEA_Application_Process_Certificate_Request :

			kea_action = SSL_KEA_No_Action;
			break;
		case SSL_KEA_Wait_For_KeyExchange:
			if(!g_main_message_handler->HasCallBack(this, MSG_SSL_COMPLETEDVERIFICATION, pending_connstate->key_exchange->Id()))
				g_main_message_handler->SetCallBack(this, MSG_SSL_COMPLETEDVERIFICATION, pending_connstate->key_exchange->Id());

			return SSL_WAIT_KeyExchange;

		case SSL_KEA_Wait_For_User:
			return nextstate; // Return as no futher actions may be taken.

		case SSL_KEA_Resume_Session:
			kea_action= Resume_Session();
			break;

		case SSL_KEA_Prepare_Premaster:
#ifdef LIBSSL_HANDSHAKE_HEXDUMP
			if(g_selftest.running)
			{
				SSL_secure_varvector32 *data = OP_NEW(SSL_secure_varvector32, ());
				if(data)
				{
					data->SetTag(4); // pre master

					*data = pending_connstate->key_exchange->PreMasterSecret();
					data->Into(&pending_connstate->selftest_sequence);
				}
			}
#endif // LIBSSL_HANDSHAKE_HEXDUMP
			pending_connstate->CalculateMasterSecret();
#ifdef LIBSSL_HANDSHAKE_HEXDUMP
			if(g_selftest.running)
			{
				SSL_secure_varvector32 *data = OP_NEW(SSL_secure_varvector32, ());
				if(data)
				{
					data->SetTag(5); // master

					*data = pending_connstate->session->mastersecret;
					data->Into(&pending_connstate->selftest_sequence);
				}
			}
#endif // LIBSSL_HANDSHAKE_HEXDUMP
			kea_action = SSL_KEA_Prepare_Keys;
			break;

		case SSL_KEA_Prepare_Keys:
			pending_connstate->SetUpCiphers();
#if 0 //def LIBSSL_HANDSHAKE_HEXDUMP
			if(g_selftest.running)
			{
				SSL_secure_varvector32 *data = new SSL_secure_varvector32;
				if(data)
				{
					data->SetTag(3); // keyblock

					*data = pending_connstate->version_specific->keyblock;
					data->Into(&selftest_sequence);
				}
			}
#endif // LIBSSL_HANDSHAKE_HEXDUMP
			kea_action = SSL_KEA_Commit_Session;
			break;

		case SSL_KEA_Commit_Session:
			kea_action = Commit_Session();
			break;

		case SSL_KEA_FullConnectionRestart:
			// TODO: Move this into a better location
			pending_connstate->BroadCastSessionNegotiatedEvent(TRUE);
			pending_connstate->session->is_resumable = FALSE;
			if(connstate->session)
			{
				connstate->BroadCastSessionNegotiatedEvent(TRUE);
				connstate->session->is_resumable = FALSE;
			}
			FlushBuffers();
			current_state = Close_Connection(SSL_PRE_CLOSE_CONNECTION);
			trying_to_reconnect = TRUE;
			if(!mh->HasCallBack(this, MSG_DO_TLS_FALLBACK, Id()))
				mh->SetCallBack(this, MSG_DO_TLS_FALLBACK, Id());
			mh->PostMessage(MSG_DO_TLS_FALLBACK,Id(),1);
			return current_state;
		default:
			OP_ASSERT(0);
			RaiseAlert(SSL_Internal, SSL_InternalError);
			// fall-through

		case SSL_KEA_Handle_Errors :
			if(ErrorRaisedFlag)
			{
				if(pending_connstate->session && pending_connstate->session->UserConfirmed == USER_REJECTED)
					flags.allow_auto_disable_tls = FALSE;
				nextstate = SSL_PRE_CLOSE_CONNECTION;
				if(key_exchange)
					key_exchange->ResetError();
				return nextstate;
			}
			kea_action = SSL_KEA_No_Action;
			break;
		}

		if(kea_action != SSL_KEA_Handle_Errors && ErrorRaisedFlag)
			kea_action = SSL_KEA_Handle_Errors;
	}

	return Process_HandshakeActions(nextstate);
}

SSL_STATE SSL::Process_HandshakeActions(SSL_STATE a_nextstate)
{
	SSL_Alert msg;
	OP_MEMORY_VAR SSL_STATE nextstate = a_nextstate;
	OP_MEMORY_VAR SSL_Handshake_Action hand_action;

	do{
		SSL_HandShakeType msg_type = SSL_NONE;

		hand_action = pending_connstate->version_specific->NextHandshakeAction(msg_type);

		switch(hand_action)
		{
		case Handshake_Send_No_Certificate:
			{
				msg_type = SSL_Certificate;
				SSL_ProtocolVersion ver = pending_connstate->version;

				if(ver.Major() <3 || (ver.Major() == 3 && (ver.Minor() == 0 ||
						(ver.Minor() == 1 && server_info->TLSUseSSLv3NoCert()))))
				{
					nextstate = Handle_Raised_Error(SSL_Warning, SSL_No_Certificate, nextstate,TRUE);
					ResetError();
					break;
				}
			}
			// Send an empty certificate Handshake, as per TLS 1.0+
		case Handshake_Send_Message:
			{
				SSL_HandShakeMessage next_message;

				if(msg_type == SSL_Certificate_Verify && server_info->IsIIS5Server())
					ProtocolComm::SetProgressInformation(STOP_FURTHER_REQUESTS);

				next_message.SetCommState(connstate);
				TRAPD(op_err, next_message.SetUpHandShakeMessageL(msg_type, pending_connstate));
				if(OpStatus::IsError(op_err))
				{
					RaiseAlert(op_err);
					nextstate = SSL_PRE_CLOSE_CONNECTION;
					break;
				}
				if(next_message.Error(&msg))
				{
					RaiseAlert(msg);
					if(msg.GetDescription() == SSL_No_Certificate)
					{
						nextstate = Handle_Raised_Error(nextstate,TRUE);
						ResetError();
					}
					else
						nextstate = SSL_PRE_CLOSE_CONNECTION;
					break;
				}
				nextstate = SendMessageRecord(next_message, nextstate);
				break;
			}
		case Handshake_ChangeCipher2:
			{
				Do_ChangeCipher(FALSE);
				SendRecord(SSL_PerformChangeCipher, NULL, 0);
				pending_connstate->version_specific->SessionUpdate(Session_Changed_Server_Cipher);
				break;
			}
		case Handshake_ChangeCipher:
			{
				nextstate = Handle_SendChangeCipher(nextstate);
				break;
			}
		case Handshake_PrepareFinished:
			pending_connstate->version_specific->GetFinishedMessage(FALSE, pending_connstate->prepared_server_finished);
			break;

		case Handshake_Completed:
			{
				TLS_Feature_Test_Status feature_status = connstate->server_info->GetFeatureStatus();
				BOOL do_reconnect = FALSE;

				connstate->last_client_finished = pending_connstate->last_client_finished;
				connstate->last_server_finished = pending_connstate->last_server_finished;

				switch(feature_status)
				{
					case TLS_Test_1_0:
						connstate->server_info->SetPreviousSuccesfullFeatureTest(TLS_1_0_only);
						feature_status = TLS_1_0_only;
						do_reconnect = flags.delayed_client_certificate_request;
						break;
#ifdef _SUPPORT_TLS_1_2
					case TLS_Test_1_2_Extensions:
						connstate->server_info->SetPreviousSuccesfullFeatureTest(TLS_1_2_and_Extensions);
						feature_status = TLS_1_2_and_Extensions;
						do_reconnect = flags.delayed_client_certificate_request;
						break;
#endif

						/*
					case TLS_Test_1_1_Extensions:
						connstate->server_info->SetPreviousSuccesfullFeatureTest(TLS_1_1_and_Extensions);
						feature_status = TLS_1_1_and_Extensions;
							break;
						*/

					case TLS_Test_1_0_Extensions:
						connstate->server_info->SetPreviousSuccesfullFeatureTest(TLS_1_0_and_Extensions);
						feature_status = TLS_1_0_and_Extensions;
						do_reconnect = flags.delayed_client_certificate_request;
						break;
					case TLS_Test_SSLv3_only:
						connstate->server_info->SetPreviousSuccesfullFeatureTest(TLS_SSL_v3_only);
						feature_status = TLS_SSL_v3_only;
						do_reconnect = flags.delayed_client_certificate_request;
					default:
						break;
				}

				connstate->server_info->SetFeatureStatus(feature_status);

				if (feature_status == TLS_Version_not_supported)
				{
					nextstate = Handle_Raised_Error(SSL_Fatal, SSL_Protocol_Version_Alert, nextstate, FALSE);
					ResetError();
					server_info->SetValidTo(g_timecache->CurrentTime());
					return nextstate;
				}

				if (!EvaluateHSTSPassCondition())
				{
					nextstate = Handle_Raised_Error(SSL_Fatal, SSL_Access_Denied, SSL_CLOSE_CONNECTION, FALSE);
					ResetError();
					server_info->SetValidTo(g_timecache->CurrentTime());
					return nextstate;
				}

				if(do_reconnect)
				{
					connstate->BroadCastSessionNegotiatedEvent(FALSE);
					connstate->session->is_resumable = FALSE;

					nextstate =  Close_Connection(SSL_PRE_CLOSE_CONNECTION);
					trying_to_reconnect = TRUE;
					if(!mh->HasCallBack(this, MSG_DO_TLS_FALLBACK, Id()))
						mh->SetCallBack(this, MSG_DO_TLS_FALLBACK, Id());
					mh->PostMessage(MSG_DO_TLS_FALLBACK,Id(),1);
					return nextstate;
				}

				if(!connstate->session->renegotiation_extension_supported &&
					(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & 
					(CRYPTO_METHOD_RENEG_DISABLE_PADLOCK | CRYPTO_METHOD_RENEG_WARN | CRYPTO_METHOD_RENEG_REFUSE)) == CRYPTO_METHOD_RENEG_DISABLE_PADLOCK)
				{
					connstate->session->security_rating = SECURITY_STATE_HALF;
					connstate->session->low_security_reason |= SECURITY_LOW_REASON_WEAK_PROTOCOL ;
				}
#ifdef _SECURE_INFO_SUPPORT
				if(connstate->session->session_information == NULL)
				{
					connstate->session->SetUpSessionInformation(connstate->server_info);
				}
#endif
				UpdateSecurityInformation();

				SetProgressInformation(START_REQUEST,0, servername->UniName());

				SetRequestMsgMode(NO_STATE);
#ifdef SSL_ENABLE_URL_HANDSHAKE_STATUS
				if(pending_connstate->ActiveURL.IsValid())
					pending_connstate->ActiveURL.SetAttribute(g_KSSLHandshakeCompleted,TRUE);
#endif

				ResumeApplicationData();
				flags.application_records_permitted = TRUE;
				if (pending_connstate->selected_next_protocol != NP_NOT_NEGOTIATED)
					ProtocolComm::SetProgressInformation(SET_NEXTPROTOCOL, pending_connstate->selected_next_protocol, NULL);
				ProtocolComm::ConnectionEstablished();
				ProtocolComm::RequestMoreData();

				nextstate = SSL_CONNECTED;
				hand_action = Handshake_No_Action;

				connstate->BroadCastSessionNegotiatedEvent(TRUE);
				break;
			}
		case Handshake_Handle_Error:
			{
				nextstate = Handle_Raised_Error(nextstate,TRUE);
				break;
			}
#ifdef LIBSSL_ENABLE_SSL_FALSE_START
		case Handshake_False_Start_Send_Application_Data:
			{
				if (pending_connstate->selected_next_protocol != NP_NOT_NEGOTIATED)
					ProtocolComm::SetProgressInformation(SET_NEXTPROTOCOL, pending_connstate->selected_next_protocol, NULL);
				ProtocolComm::ConnectionEstablished();
				ProtocolComm::RequestMoreData();
				SendRecord(SSL_Handshake_False_Start_Application, NULL, 0);
				break;
			}
#endif // LIBSSL_ENABLE_SSL_FALSE_START;
		}
	}while(!ErrorRaisedFlag && hand_action != Handshake_No_Action);

	Flush();

	return nextstate;
}


SSL_KEA_ACTION SSL::Resume_Session()
{
	if(!pending_connstate->Resume_Session())
		return SSL_KEA_Handle_Errors;

	return SSL_KEA_Commit_Session;
}

SSL_KEA_ACTION SSL::Commit_Session()
{
	connstate->RemoveSession();
	connstate->session = pending_connstate->session;
	connstate->session->connections ++;

	return SSL_KEA_No_Action;
}

SSL_STATE SSL::HandleDialogFinished(MH_PARAM_2 par2)
{
	if(par2 != IDOK && par2 != IDYES)
		return Close_Connection(SSL_CLOSE_CONNECTION);

	OP_MEMORY_VAR SSL_STATE	pref_next_state = current_state;
	switch(current_state)
	{
	case SSL_PREHANDSHAKE_WAITING:
	case SSL_SENT_CLIENT_HELLO_WAITING:
		pref_next_state = Handle_Start_Handshake(loading_handshake, SSL_SENT_CLIENT_HELLO, FALSE) ;
		break;
	case SSL_WAIT_CERT_2:
		break;
#ifdef USE_SSL_CERTINSTALLER
	case SSL_WAIT_PASSWORD:
		break;
#endif
	case SSL_WAIT_KeyExchange:
		pref_next_state = current_state = SSL_NEGOTIATING;
		break;

	default:
		pref_next_state = Close_Connection(current_state);
	}

	return ProgressHandshake(pref_next_state);
}

SSL_STATE SSL::ProgressHandshake(SSL_STATE pref_next_state)
{
	WriteRecordsToBuffer(TRUE);
	if(pref_next_state == SSL_NOT_CONNECTED || pref_next_state == SSL_PRE_CLOSE_CONNECTION ||
		pref_next_state == SSL_CLOSE_CONNECTION || pref_next_state == SSL_CLOSE_CONNECTION2)
	{
		return pref_next_state;
	}

	if(GetRecordType() == SSL_Handshake)
	{
		OP_STATUS op_err = loading_handshake.SetMessage(SSL_NONE);
		if(OpStatus::IsError(op_err))
		{
			RaiseAlert(op_err);
		}
		loading_handshake.ResetRecord();
		if(ErrorRaisedFlag)
		{
			pref_next_state = current_state = SSL_SERVER_HELLO_WAITING;
		}
		else
		{
			pref_next_state  = current_state = Handle_HandShake(pref_next_state);
	
			if(!(pref_next_state >= SSL_Wait_ForAction_Start && pref_next_state <= SSL_Wait_ForAction_End))
				RemoveRecord();
		}
	}

	if(!(pref_next_state >= SSL_Wait_ForAction_Start && pref_next_state <= SSL_Wait_ForAction_End))
	{
		current_state = pref_next_state;
		ProcessReceivedData();
		pref_next_state = current_state;
	}
	WriteRecordsToBuffer(FALSE);
	StartEncrypt();
	Flush();
	return pref_next_state;
}
#endif // _NATIVE_SSL_SUPPORT_
