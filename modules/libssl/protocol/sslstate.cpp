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

#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/protocol/op_ssl.h"
#include "modules/libssl/keyex/sslkeyex.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/hardcore/mem/mem_man.h"

//#include "modules/libssl/ui/sslcctx.h"
#include "modules/url/url_sn.h"

#include "modules/libssl/debug/tstdump2.h"

#define COMM_CONNECTION_FAILURE 0x02

#ifdef _DEBUG
#ifdef YNP_WORK
//#define _DEBUGSSL_HANDHASH
#define _DEBUG_ERR
#endif
#endif

//extern MessHandler *mess;

//SSL_SessionStateRecordHead SSL::sessioncache;

#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define SSL_ERRSTR(p,x) Str::##p##_##x
#else
#define SSL_ERRSTR(p,x) x
#endif

const SSL_statesdescription_record handlerecordstates[] = {
	{SSL_PREHANDSHAKE,       SSL_AlertMessage,     SSL_HANDLE_ALERT,             SSL_PREHANDSHAKE       },
	{SSL_PREHANDSHAKE,       SSL_Handshake,        SSL_HANDLE_HANDSHAKE,         SSL_PREHANDSHAKE       },
	{SSL_PREHANDSHAKE,       SSL_Application,      SSL_HANDLE_APPLICATION2,	     SSL_PREHANDSHAKE   },
	{SSL_WAITING_FOR_SESSION,SSL_AlertMessage,     SSL_HANDLE_ALERT,             SSL_PREHANDSHAKE       },
	{SSL_WAITING_FOR_SESSION,SSL_Handshake,        SSL_HANDLE_HANDSHAKE,         SSL_PREHANDSHAKE       },
	{SSL_SENT_CLIENT_HELLO,  SSL_AlertMessage,     SSL_HANDLE_ALERT,             SSL_SENT_CLIENT_HELLO  },
	{SSL_SENT_CLIENT_HELLO,  SSL_Handshake,        SSL_HANDLE_HANDSHAKE,         SSL_SENT_CLIENT_HELLO  },
	{SSL_SENT_CLIENT_HELLO,  SSL_Application,      SSL_HANDLE_APPLICATION2,      SSL_SENT_CLIENT_HELLO   },
	{SSL_NEGOTIATING,        SSL_ChangeCipherSpec, SSL_CHANGE_CIPHER,			 SSL_NEGOTIATING   },
	{SSL_NEGOTIATING,        SSL_AlertMessage,     SSL_HANDLE_ALERT,             SSL_NEGOTIATING       },
	{SSL_NEGOTIATING,        SSL_Handshake,        SSL_HANDLE_HANDSHAKE,         SSL_NEGOTIATING       },
	{SSL_NEGOTIATING,        SSL_Application,      SSL_HANDLE_APPLICATION2,      SSL_NEGOTIATING   },
	{SSL_CONNECTED,          SSL_ChangeCipherSpec, SSL_ERROR_UNEXPECTED_MESSAGE, SSL_CLOSE_CONNECTION   },
	{SSL_CONNECTED,          SSL_AlertMessage,     SSL_HANDLE_ALERT,             SSL_CONNECTED          },
	{SSL_CONNECTED,          SSL_Handshake,        SSL_HANDLE_HANDSHAKE,         SSL_PREHANDSHAKE       },
	{SSL_CONNECTED,          SSL_Application,      SSL_HANDLE_APPLICATION,       SSL_CONNECTED          },
	{SSL_CLOSE_CONNECTION,   SSL_AlertMessage,     SSL_HANDLE_ALERT,             SSL_CLOSE_CONNECTION   },
	{SSL_CLOSE_CONNECTION,   SSL_Handshake,        SSL_IGNORE,                   SSL_CLOSE_CONNECTION   },
	{SSL_CLOSE_CONNECTION,   SSL_Application,      SSL_HANDLE_APPLICATION,       SSL_CLOSE_CONNECTION   },
	{SSL_CLOSE_CONNECTION2,  SSL_AlertMessage,     SSL_HANDLE_ALERT,             SSL_CLOSE_CONNECTION2  },
	{SSL_CLOSE_CONNECTION2,  SSL_Handshake,        SSL_IGNORE,                   SSL_CLOSE_CONNECTION2  },
	{SSL_CLOSE_CONNECTION2,  SSL_Application,      SSL_HANDLE_APPLICATION,       SSL_CLOSE_CONNECTION2  },
	{SSL_NOT_CONNECTED,      SSL_Application,      SSL_HANDLE_APPLICATION,       SSL_NOT_CONNECTED      }, // HACK to avoid latemessages
	{SSL_NOT_CONNECTED,      SSL_AlertMessage,     SSL_HANDLE_ALERT,             SSL_NOT_CONNECTED      }, // HACK to avoid latemessages
	{SSL_NOSTATE,            SSL_Application,      SSL_ERROR_UNEXPECTED_MESSAGE, SSL_CLOSE_CONNECTION   } // Failsafe
};

const SSL_statesdescription_alert handlealertstates[] = {
	{SSL_SENT_CLIENT_HELLO,   SSL_Close_Notify,           SSL_HANDLE_AS_HANDSHAKE_FAIL, SSL_CLOSE_CONNECTION  },
	{SSL_NEGOTIATING,         SSL_Close_Notify,           SSL_HANDLE_AS_HANDSHAKE_FAIL, SSL_CLOSE_CONNECTION  },
	{SSL_ANYSTATE,            SSL_Close_Notify,           SSL_HANDLE_CLOSE,             SSL_CLOSE_CONNECTION  },
	{SSL_ANYSTATE,            SSL_InternalError,          SSL_HANDLE_INTERNAL_ERRROR,   SSL_CLOSE_CONNECTION  },
	{SSL_ANYSTATE,            SSL_Illegal_Parameter,      SSL_HANDLE_ILLEGAL_PARAMETER, SSL_CLOSE_CONNECTION  },
	{SSL_ANYSTATE,            SSL_Unexpected_Message,     SSL_ERROR_UNEXPECTED_MESSAGE, SSL_CLOSE_CONNECTION  },
	{SSL_ANYSTATE,            SSL_Decompression_Failure,  SSL_HANDLE_DECOMPRESSION_FAIL,SSL_CLOSE_CONNECTION  },
	{SSL_ANYSTATE,            SSL_Bad_Record_MAC,         SSL_HANDLE_BAD_MAC,           SSL_CLOSE_CONNECTION  },
	{SSL_PREHANDSHAKE,        SSL_Handshake_Failure,      SSL_HANDLE_HANDSHAKE_FAIL,    SSL_CLOSE_CONNECTION  },
	{SSL_SENT_CLIENT_HELLO,   SSL_Handshake_Failure,      SSL_HANDLE_HANDSHAKE_FAIL,    SSL_CLOSE_CONNECTION  },
	{SSL_SENT_CLIENT_HELLO,   SSL_Unrecognized_Name, 	  SSL_IGNORE,					SSL_SENT_CLIENT_HELLO},
	{SSL_NEGOTIATING,         SSL_Bad_Certificate,        SSL_HANDLE_BAD_CERT,          SSL_CLOSE_CONNECTION  },
	{SSL_NEGOTIATING,         SSL_Unsupported_Certificate,SSL_HANDLE_BAD_CERT,          SSL_CLOSE_CONNECTION  },
	{SSL_NEGOTIATING,         SSL_Certificate_Revoked,    SSL_HANDLE_BAD_CERT,          SSL_CLOSE_CONNECTION  },
	{SSL_NEGOTIATING,         SSL_Unsupported_Certificate,SSL_HANDLE_BAD_CERT,          SSL_CLOSE_CONNECTION  },
	{SSL_NEGOTIATING,         SSL_Certificate_Expired,    SSL_HANDLE_BAD_CERT,          SSL_CLOSE_CONNECTION  },
	{SSL_NEGOTIATING,         SSL_Certificate_Not_Yet_Valid, SSL_HANDLE_BAD_CERT,       SSL_CLOSE_CONNECTION  },
	{SSL_NEGOTIATING,         SSL_Certificate_Unknown,    SSL_HANDLE_BAD_CERT,          SSL_CLOSE_CONNECTION  },
	{SSL_NEGOTIATING,		  SSL_Handshake_Failure,      SSL_HANDLE_HANDSHAKE_FAIL,    SSL_CLOSE_CONNECTION  },
	{SSL_NEGOTIATING,         SSL_No_Certificate,         SSL_HANDLE_NO_CLIENT_CERT,    SSL_NEGOTIATING      },
	{SSL_NOSTATE,             SSL_No_Description,         SSL_ERROR_UNEXPECTED_ERROR,   SSL_CLOSE_CONNECTION  } // Failsafe
};

void SSL::SessionNegotiatedContinueHandshake(BOOL success)
{
	if(current_state == SSL_WAITING_FOR_SESSION)
	{
		if (success)
			current_state = Handle_Start_Handshake(loading_handshake, SSL_SENT_CLIENT_HELLO, FALSE);
		else
			current_state = Close_Connection(SSL_NOT_CONNECTED);
	}
}

BOOL SSL::SafeToDelete() const
{
	return ( !Handling_callback && SSL_Record_Layer::SafeToDelete() );
}


void SSL::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	Handling_callback++;
	switch(msg)
	{
	case MSG_COMM_LOADING_FINISHED:
	case MSG_COMM_LOADING_FAILED:
		if(is_connected)
		{
			//if(!flags.received_closure_alert && !server_info->IsIIS4Server() ) // IIS 4 servers apparently does not tell when they close the connection
			{
				/*
				if(connstate!= NULL && connstate->session != NULL)
					connstate->session->is_resumable = FALSE;*/
				if(pending_connstate != NULL && pending_connstate->session != NULL && 
					(connstate == NULL || pending_connstate->session != connstate->session))
					pending_connstate->session->is_resumable = FALSE;
			}
			Stop();
		}
		if((!is_connected || (current_state != SSL_RETRY
           && current_state != SSL_CLOSE_CONNECTION
           && current_state != SSL_CLOSE_CONNECTION2)) && !trying_to_reconnect)
		{
			mh->PostMessage(msg, Id(), par2);
		}
		else if(current_state == SSL_RECONNECTING)
		{
			mh->PostMessage(msg, Id(), par2);
		}
		break;

	case  MSG_SSL_WAITING_SESSION_TMEOUT :
		if(current_state == SSL_WAITING_FOR_SESSION)
			current_state = Handle_Start_Handshake(loading_handshake,SSL_SENT_CLIENT_HELLO, TRUE);
		break;

	case  MSG_SSL_WAIT_EMPTY_BUFFER :
		if(current_state == SSL_CLOSE_CONNECTION ||
			current_state == SSL_CLOSE_CONNECTION2 ||
			current_state == SSL_NOT_CONNECTED)
		{
			if((par2&0xff)==1)
			{
				current_state = Close_Connection(current_state);
			}
			else
			{
				Perform_ProcessReceivedData();
				BOOL fin = EmptyBuffers(TRUE);
				mh->PostDelayedMessage(MSG_SSL_WAIT_EMPTY_BUFFER,Id(),fin, (fin ? 512 : 128));
#ifdef TST_DUMP
				if(fin)
					PrintfTofile("winsck.txt",
                                 "SSL::HandleMessage sending last MSG_SSL_WAIT_EMPTY_BUFFER signal for id : %d\n",
                                 Id());
#endif
			}
		}
		break;

	case MSG_DO_TLS_FALLBACK:
		if(mh && trying_to_reconnect &&
			current_state != SSL_RECONNECTING)
		{
			if(par2 || ProtocolComm::Closed())
			{
				BOOL allowed = FALSE;
				SetProgressInformation(RESTART_LOADING,0, &allowed);
				if(allowed)
				{
					ResetConnection();
					Stop();
					SSL_Record_Layer::Stop();
					FlushBuffers(TRUE);
					ResetError(); // Forget any errors during the shutdown phase
					current_state = SSL_RECONNECTING;
					already_tried_to_reconnect = TRUE;
					Load();
				}
				else
				{
					mh->PostMessage(MSG_SSL_RESTART_CONNECTION, Id(), 0);
					mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), Str::S_MSG_SSL_NonFatalError_Retry);
				}
			}
			else
			{
				mh->PostMessage(MSG_DO_TLS_FALLBACK,Id(),0);
				break;
			}
		}
		if(mh)
			mh->UnsetCallBack(this, MSG_DO_TLS_FALLBACK, Id());
		break;
	case MSG_SSL_COMPLETEDVERIFICATION:
		if(pending_connstate->key_exchange && (MH_PARAM_1)pending_connstate->key_exchange->Id() == par1){
			g_main_message_handler->UnsetCallBack(this, MSG_SSL_COMPLETEDVERIFICATION);
			SSL_STATE next_state = Process_KeyExchangeActions(SSL_NEGOTIATING,  pending_connstate->key_exchange->GetPostVerifyAction());

			if(!(next_state > SSL_Wait_ForAction_Start && next_state < SSL_Wait_ForAction_End))
				next_state = ProgressHandshake(next_state);

			current_state = next_state;
		}
		break;
	default:
		SSL_Record_Layer::HandleCallback(msg,par1,par2);
	}

	if(ErrorRaisedFlag)
		current_state = Handle_Raised_Error(current_state);
	Handling_callback--;
}

void SSL::Handle_Record(SSL_ContentType recordtype)
{
	const SSL_statesdescription_record *staterecord;
	SSL_STATE nextstate,tempstate;

	if(current_state == SSL_NOT_CONNECTED)
		return;

	if(::Valid(recordtype) && !flags.closing_connection)
	{
		if(current_state == SSL_SENT_CLIENT_HELLO)
		{
			switch(pending_connstate->server_info->GetFeatureStatus())
			{
			case TLS_Test_1_2_Extensions:
			//case TLS_Test_1_1_Extensions:
			case TLS_Test_1_0:
			case TLS_Test_1_0_Extensions:
			case TLS_Test_SSLv3_only:
				// Disable featuretest timeout
				g_main_message_handler->UnsetCallBack(this, MSG_COMM_LOADING_FAILED, (MH_PARAM_1) this);
				g_main_message_handler->RemoveDelayedMessage(MSG_COMM_LOADING_FAILED, (MH_PARAM_1) this, SSL_ERRSTR(SI,ERR_HTTP_TIMEOUT));
				break;
			}
		}

		staterecord = handlerecordstates;
		while ((tempstate =staterecord->present_state) != SSL_NOSTATE)
		{
			if((tempstate == SSL_ANYSTATE || tempstate == current_state) &&
				staterecord->recordtype == recordtype)
				break;
			staterecord++;
		}

		nextstate = staterecord->default_nextstate;
		switch (staterecord->action)
		{
		case SSL_IGNORE        :
			nextstate = current_state;
			break;
		case SSL_CHANGE_CIPHER :
			nextstate = Handle_Change_Cipher(nextstate);
			break;
		case SSL_HANDLE_ALERT  :
			nextstate = Handle_Received_Alert(nextstate);
			break;
		case SSL_HANDLE_V2_RECORD :
		case SSL_HANDLE_HANDSHAKE :
			StartingToSetUpRecord(TRUE);
			WriteRecordsToBuffer(TRUE); // Make sure we collect the records without sending on network. This is to make the tcp packaging more efficient.
			nextstate = Handle_HandShake(nextstate);
			StartingToSetUpRecord(FALSE);
			WriteRecordsToBuffer(FALSE); // Allow sending on network
			StartEncrypt();	// Make sure every record in the record list is encrypted and sent
			Flush(); // flush in case anything is left in the buffer

			break;
		case SSL_HANDLE_APPLICATION :
		case SSL_HANDLE_APPLICATION2 :
			if(flags.application_records_permitted)
				nextstate = Handle_Application(nextstate);
			else
				RaiseAlert(SSL_Fatal,SSL_Unexpected_Message);
			break;
		case SSL_ERROR_UNEXPECTED_MESSAGE :
			if(!flags.closed_with_error)
				RaiseAlert(SSL_Fatal,SSL_Unexpected_Message);
			break;
        default:
            break;
		}

		current_state = nextstate;

		if(ErrorRaisedFlag)
		{
			current_state = Handle_Raised_Error(nextstate);
		}

	}
	if(!(current_state >= SSL_Wait_ForAction_Start && current_state <= SSL_Wait_ForAction_End))
		RemoveRecord();
}

void SSL::SetProgressInformation(ProgressState progress_level,
								   unsigned long progress_info1,
								   const void *progress_info2)
{
	if (progress_level == UPLOADING_PROGRESS)
	{
		/**
		 * Since SSL layer adds protocol overhead, the progress info from socket will
		 * will report more bytes than the application layer sent to SSL layer.
		 *
		 * Until all data has been sent, the report to application layer will be
		 * approximately correct.
		 *
		 * When last byte has been reported from comm, the rest of application
		 * data will be reported. Thus, the application layer will receive
		 * correct amount of progress, at the moment all data has been sent.
		 */

		double persent_progress = progress_info1 / buffered_amount_raw_data;
		OP_ASSERT(persent_progress <= 1);

		buffered_amount_raw_data -= progress_info1;

		unsigned long amount_application_data_to_report;

		if (buffered_amount_raw_data > 0)
		{
			// We report the approximate amount of data sent.
			amount_application_data_to_report = static_cast<unsigned long>(op_floor(persent_progress * buffered_amount_application_data));
			buffered_amount_application_data -= amount_application_data_to_report;
		}
		else
		{
			// All data has been sent, we report the rest of the application data.
			amount_application_data_to_report = buffered_amount_application_data;
			buffered_amount_application_data = 0;
		}

		if (amount_application_data_to_report)
			SComm::SetProgressInformation(progress_level, amount_application_data_to_report, progress_info2);
	}
	else
		SComm::SetProgressInformation(progress_level, progress_info1, progress_info2);
}

CommState SSL::ConnectionEstablished()
{
	buffered_amount_application_data = 0;
	buffered_amount_raw_data = 0;
	SSL_SessionStateRecordList *tempsession;

	if(server_info == (SSL_Port_Sessions *) NULL)
	{
		// Will not block
		current_state = Handle_Local_Error(SSL_Internal, SSL_Allocation_Failure,current_state);
		return COMM_REQUEST_FAILED;
	}

	SetDoNotReconnect(TRUE);
	flags.delayed_client_certificate_request = FALSE;
	current_state = SSL_PREHANDSHAKE;

	RETURN_VALUE_IF_ERROR(loading_handshake.SetMessage(SSL_NONE),COMM_REQUEST_FAILED);
	loading_handshake.ResetRecord();

	//SSL_CipherSpec *read, *write;

	trying_to_reconnect = FALSE;
    if(g_securityManager == NULL || !g_securityManager->SecurityEnabled)
	{
		// Will not block
        current_state = Handle_Local_Error(SSL_Internal, SSL_Security_Disabled, current_state);
        return COMM_REQUEST_FAILED;
    }
    if(!(g_securityManager->Enable_SSL_V3_0 || g_securityManager->Enable_TLS_V1_0
		|| g_securityManager->Enable_TLS_V1_1 || g_securityManager->Enable_TLS_V1_2
		)|| g_securityManager->SystemCipherSuite.Count() == 0
		)
	{
		// Will not block
        current_state = Handle_Local_Error(SSL_Internal, SSL_No_Cipher_Selected, current_state);
        return COMM_REQUEST_FAILED;
    }

	OP_DELETE(connstate->version_specific);
	connstate->version_specific = NULL;

	OP_DELETE(pending_connstate->version_specific);
	pending_connstate->version_specific = NULL;

	g_main_message_handler->UnsetCallBack(this, MSG_SSL_COMPLETEDVERIFICATION);
	OP_DELETE(pending_connstate->key_exchange);
	pending_connstate->key_exchange = NULL;

	connstate->last_client_finished.Resize(0);
	connstate->last_server_finished.Resize(0);
	pending_connstate->last_client_finished.Resize(0);
	pending_connstate->last_server_finished.Resize(0);

	flags.closing_connection =  flags.closed_with_error =  flags.received_closure_alert = FALSE;

	if(!mh->HasCallBack(this, MSG_SSL_WAITING_SESSION_TMEOUT, Id()))
		RAISE_AND_RETURN_VALUE_IF_ERROR(mh->SetCallBack(this, MSG_SSL_WAITING_SESSION_TMEOUT, Id()),COMM_REQUEST_FAILED);
	if(!mh->HasCallBack(this, MSG_SSL_WAIT_EMPTY_BUFFER, Id()))
		RAISE_AND_RETURN_VALUE_IF_ERROR(mh->SetCallBack(this, MSG_SSL_WAIT_EMPTY_BUFFER, Id()),COMM_REQUEST_FAILED);
	
	pending_connstate->RemoveSession();

	/*read = */ pending_connstate->Prepare_cipher_spec(FALSE);
	/*write = */ pending_connstate->Prepare_cipher_spec(TRUE);

	if(connstate->write.cipher != NULL)
		connstate->write.cipher->Sequence_number = 0;

	if(connstate->read.cipher != NULL)
		connstate->read.cipher->Sequence_number = 0;

	if(ErrorRaisedFlag)
	{
		// Will not block
		current_state = Handle_Raised_Error(current_state);
		return COMM_REQUEST_FAILED;
	}
	pending_connstate->sigalg = SSL_Anonymous_sign;

	pending_connstate->handshake_queue.Clear();

	security_profile = g_securityManager->FindSecurityProfile(SSL_Record_Layer::servername,SSL_Record_Layer::port);
	if(security_profile == NULL)
		return COMM_REQUEST_FAILED;

	tempsession = pending_connstate->FindSessionRecord();
    if (tempsession != NULL)
	{
		tempsession->connections++;
		flags.allow_auto_disable_tls = !server_info->TLSDeactivated();
	}
	SetProgressInformation(GET_APPLICATIONINFO, 0, &pending_connstate->ActiveURL);
	SetProgressInformation(GET_ORIGINATING_WINDOW, 0, &pending_connstate->window);
#ifdef URL_DISABLE_INTERACTION
	pending_connstate->user_interaction_blocked = GetUserInteractionBlocked();
#endif

	if (!Init() ||  SSL_Record_Layer::ConnectionEstablished() == COMM_CONNECTION_FAILURE)
		return COMM_REQUEST_FAILED;

	current_state = Handle_Start_Handshake(loading_handshake, SSL_SENT_CLIENT_HELLO, FALSE);

	if(ErrorRaisedFlag)
	{
		current_state = Handle_Raised_Error(current_state);
	}

	if(current_state == SSL_NOT_CONNECTED)
		return COMM_REQUEST_FAILED;

#ifdef SSL_ENABLE_URL_HANDSHAKE_STATUS
	if(pending_connstate->ActiveURL.IsValid())
		pending_connstate->ActiveURL.SetAttribute(g_KSSLHandshakeSent,TRUE);
#endif

	return COMM_LOADING;
}

SSL_STATE SSL::Handle_Change_Cipher(SSL_STATE pref_next_state_arg)
{
    OP_MEMORY_VAR SSL_STATE pref_next_state = pref_next_state_arg;
	SSL_Alert msg;
	DataStream *source = GetRecord();

	OP_ASSERT(source);

	while (source->MoreData())
	{
		OP_MEMORY_VAR OP_STATUS op_err = OpRecStatus::OK;
		SSL_TRAP_AND_RAISE_ERROR_THIS(op_err = loading_chcipherspec.ReadRecordFromStreamL(source));

		if(ErrorRaisedFlag)
			return SSL_PRE_CLOSE_CONNECTION;

		if(op_err != OpRecStatus::FINISHED)
			continue;

		if(!loading_chcipherspec.Finished())
			continue;

		if(!loading_chcipherspec.Valid(&msg))
		{
			msg.SetLevel(SSL_Fatal);
			RaiseAlert(msg);
			return SSL_PRE_CLOSE_CONNECTION;
		}

		if(!pending_connstate->version_specific->ExpectingCipherChange())
		{
			RaiseAlert(SSL_Fatal,SSL_Unexpected_Message);
			return SSL_PRE_CLOSE_CONNECTION;
		}

		Do_ChangeCipher(FALSE);
		if(source->MoreData())
		{
			RaiseAlert(SSL_Fatal,SSL_Unexpected_Message);
			return SSL_PRE_CLOSE_CONNECTION;
		}
		pending_connstate->version_specific->SessionUpdate(Session_Changed_Server_Cipher);
		pending_connstate->version_specific->GetFinishedMessage(/*!pending_connstate->client*/ FALSE, pending_connstate->prepared_server_finished);
	}

	return pref_next_state;
}

// Eddy/2002/Apr: would someone who knows what this is for please document it ...
SSL_STATE SSL::Handle_Received_Alert(SSL_STATE pref_next_state)
{
	OP_MEMORY_VAR SSL_STATE nextstate = SSL_ANYSTATE; // ... and give this a suitable initialiser.
	BOOL cont = FALSE;
	DataStream *source = GetRecord();

	OP_ASSERT(source);

	while (source->MoreData())
	{
		SSL_TRAP_AND_RAISE_ERROR_THIS(loading_alert.ReadRecordFromStreamL(source));
		if(ErrorRaisedFlag)
			return SSL_PRE_CLOSE_CONNECTION;

		if(!loading_alert.Finished())
			continue;

		if(flags.closing_connection)
			continue;

        SSL_AlertDescription alerttype = loading_alert.GetDescription();
#ifdef _DEBUG_ERR
		PrintfTofile("ssltrans.txt", " ID %d : Remote Error %d (%x)\r\n\r\n",
                     Id(), ((int) alerttype)&0xff, ((int) alerttype));
#endif

        const SSL_statesdescription_alert *staterecord = handlealertstates;
        SSL_STATE tempstate;
		while ((tempstate =staterecord->present_state) != SSL_NOSTATE)
		{
			if((tempstate == SSL_ANYSTATE || tempstate == current_state) &&
				staterecord->alerttype == alerttype)
				break;
			staterecord++;
		}

		if(staterecord->action == SSL_HANDLE_AS_HANDSHAKE_FAIL)
			loading_alert.Set(SSL_Fatal, SSL_Handshake_Failure);

		nextstate =staterecord->fatal_nextstate;
		BOOL display_warning = TRUE;

		if(!flags.closed_with_error &&
			loading_alert.GetLevel() == SSL_Fatal &&
			current_state != SSL_CONNECTED &&
			connstate &&
			pending_connstate && pending_connstate->sent_version.Compare(3,0) > 0
			)
		{
			// Try to restart using SSL v3.0 instead
			cont = FALSE;
			if(connstate->session != NULL)
				connstate->session->is_resumable = FALSE;
			if(pending_connstate->session != NULL)
				pending_connstate->session->is_resumable = FALSE;
			trying_to_reconnect = TRUE;
			display_warning = FALSE;
			switch(server_info->GetFeatureStatus())
			{
			case TLS_Test_SSLv3_only:

				if (g_securityManager->Enable_SSL_V3_0)
				{
					server_info->SetTLSDeactivated(TRUE);
					server_info->SetFeatureStatus(TLS_SSL_v3_only);
					server_info->SetPreviousSuccesfullFeatureTest(TLS_SSL_v3_only);

					server_info->SetValidTo(g_timecache->CurrentTime() + SSL_VERSION_TEST_VALID_TIME );
					trying_to_reconnect = TRUE;
				}
				break;

			case TLS_Test_1_0:
				server_info->SetValidTo(g_timecache->CurrentTime() + SSL_VERSION_TEST_VALID_TIME );
				if(connstate->version < SSL_ProtocolVersion(3,1))
				{
					if (g_securityManager->Enable_SSL_V3_0)
					{
						server_info->SetTLSDeactivated(TRUE);
						server_info->SetFeatureStatus(TLS_SSL_v3_only);
						trying_to_reconnect = TRUE;
					}
					break;
				}
				server_info->SetFeatureStatus(TLS_1_0_only);
				trying_to_reconnect = FALSE;
				break;
#ifdef _SUPPORT_TLS_1_2
			case TLS_Test_1_2_Extensions:
				server_info->SetFeatureStatus((current_state != SSL_SENT_CLIENT_HELLO && current_state != SSL_NEGOTIATING) || 
					connstate->version.Compare(3,3) >= 0 ? TLS_1_2_and_Extensions : TLS_Test_1_0_Extensions);
				server_info->SetValidTo(g_timecache->CurrentTime() + SSL_VERSION_TEST_VALID_TIME );
				break;
#endif
				/*
			case TLS_Test_1_1_Extensions:
				server_info->SetFeatureStatus(g_securityManager->Enable_TLS_V1_0 &&
									(current_state == SSL_SENT_CLIENT_HELLO || current_state == SSL_NEGOTIATING) && 
									connstate->version.Compare(3,2) < 0 ? TLS_Test_1_0_Extensions : TLS_1_1_and_Extensions);
				server_info->SetValidTo(g_timecache->CurrentTime() + SSL_VERSION_TEST_VALID_TIME );
				break;
				*/
			case TLS_Test_1_0_Extensions:
				server_info->SetFeatureStatus(TLS_Test_1_0);
				break;
			default:
				if(connstate->version > SSL_ProtocolVersion(3,0))
				{
					trying_to_reconnect = FALSE;
				}
				break;
			}
			if(!trying_to_reconnect)
			{
				display_warning = TRUE;
			}
		}

		if(display_warning && (current_state != SSL_CONNECTED || ProtocolComm::IsActiveConnection()))
		{
			if(loading_alert.GetLevel() == SSL_Warning)
				ApplicationHandleWarning(loading_alert,TRUE,cont);
			else
			{
				ApplicationHandleFatal(loading_alert,TRUE);
				cont = FALSE;
			}
		}

		if(!cont )
		{
			if(!trying_to_reconnect && alerttype != SSL_Close_Notify)
			{
				flags.closed_with_error = (flags.internal_error_message_set ? 2 : 1);
				if(connstate!= NULL && connstate->session != NULL)
					connstate->session->is_resumable = FALSE;
				if(pending_connstate != NULL && pending_connstate->session != NULL)
					pending_connstate->session->is_resumable = FALSE;
			}
			nextstate = Close_Connection(nextstate);
		}


		if(alerttype == SSL_Close_Notify)
		{
			flags.received_closure_alert = TRUE;
			SetProcessingInputData(FALSE);
		}

		current_state = nextstate;

	}
	return nextstate;
}

SSL_STATE SSL::Handle_Application(SSL_STATE pref_next_state)
{
	MoveRecordToApplicationBuffer();
	return pref_next_state;
}


BOOL SSL::AcceptNewVersion(const SSL_ProtocolVersion &nver)
{
	if(current_state !=SSL_SENT_CLIENT_HELLO)
		return FALSE;

	connstate->SetVersion(nver);
	if(ErrorRaisedFlag)
	{
		current_state = SSL_PRE_CLOSE_CONNECTION;;
		return FALSE;
	}
	return TRUE;
}

void SSL::UpdateSecurityInformation()
{
	 /* This code does a final check and signals the security level to window layer.
	 *
	 * Note that the session security is modifed in many different places. This should be
	 * re-factored and moved into one place, preferable into a security class.
	 *
	 * From 2011 the key size requirement for EV is raised to 2048. The key size == 2048 for
	 * EV is correctly checked in VerifyCertificate_CheckKeySize and session->extended_validation
	 * is false if it does't pass.
	 *
	 * Todo: check the 1024 bit size. It's unclear if that check is the same as in VerifyCertificate_CheckKeySize
	 * or if it is another check.  Anyhow, the 2048 check happens, and it will not be EV as long as that check fails.
	 */

	if(connstate && connstate->session)
	{
		SSL_SessionStateRecordList *session = connstate->session;

#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
				if(session->extended_validation &&
					session->security_rating ==  SECURITY_STATE_FULL &&
					session->lowest_keyex.RSA_keysize >= 1024)
				{
					session->security_rating = SECURITY_STATE_FULL_EXTENDED;
				}
#endif

		SetProgressInformation(SET_SECURITYLEVEL,
			((int) (session->security_rating & SECURITY_STATE_MASK)) | (((int) session->low_security_reason) << SECURITY_STATE_MASK_BITS),
			session->security_cipher_string.CStr());
#ifdef _SECURE_INFO_SUPPORT
		SetProgressInformation(SET_SESSION_INFORMATION,0, session->session_information);
#endif
		if((connstate->session->low_security_reason & (
			SECURITY_LOW_REASON_UNABLE_TO_CRL_VALIDATE | SECURITY_LOW_REASON_UNABLE_TO_OCSP_VALIDATE |
			SECURITY_LOW_REASON_OCSP_FAILED | SECURITY_LOW_REASON_CRL_FAILED)) != 0)
		{
			// If we were unable to check revocation, do not resume this session, next connection will do full handshake
			// Also, don't allow more than one (HTTP) request on this connection.
			connstate->session->is_resumable = FALSE;
			SetProgressInformation(STOP_FURTHER_REQUESTS, STOP_REQUESTS_TC_NOT_FIRST_REQUEST);
		}


	}
}

int SSL::GetSecurityLevel() const
{
    return (connstate != NULL && connstate->session != NULL &&
		connstate->session->cipherdescription != NULL ?
		(int) connstate->session->security_rating : SECURITY_STATE_UNKNOWN);
}

/*
const uni_char *SSL::GetSecurityText() const
{
    return (connstate != NULL && connstate->session != NULL &&
		connstate->session->cipherdescription != NULL ?
		connstate->session->security_cipher_string.CStr() : NULL);
}
*/

void SSL::ResetConnection()
{
}

OP_STATUS SSL::SetCallbacks(MessageObject* master, MessageObject* parent)
{
	static const OpMessage messages[] =
    {
		MSG_COMM_LOADING_FINISHED,
		MSG_COMM_LOADING_FAILED
    };

    RETURN_IF_ERROR(mh->SetCallBackList((parent ? parent : master), Id(), messages, ARRAY_SIZE(messages)));

	return ProtocolComm::SetCallbacks(master, this);
}

BOOL SSL::HasId(unsigned int sid)
{
	return (Id() == sid || (!trying_to_reconnect && ProtocolComm::HasId(sid)));
}

#endif
