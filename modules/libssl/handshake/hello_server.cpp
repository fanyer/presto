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
#include "modules/libssl/handshake/sslhand3.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/handshake/hello_server.h"
#include "modules/libssl/handshake/tls_ext.h"
#include "modules/libssl/method_disable.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/util/cleanse.h"

SSL_Server_Hello_st::SSL_Server_Hello_st()
: compression_method(SSL_Null_Compression)
{
	AddItem(server_version);
	AddItem(random);
	AddItem(session_id);
	AddItem(cipherid);
	AddItem(compression_method);
	AddItem(extensions);
}

SSL_KEA_ACTION SSL_Server_Hello_st::ProcessMessage(SSL_ConnectionState *pending)
{
	SSL_SessionStateRecordList *session;
	SSL_Version_Dependent *version_specific;
	SSL_Alert msg;

	if(!pending)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return SSL_KEA_Handle_Errors;
	}

	OP_MEMORY_VAR TLS_Feature_Test_Status feature_status = pending->server_info->GetFeatureStatus();
	OP_MEMORY_VAR BOOL renegotiate = FALSE;

	switch(feature_status)
	{
	case TLS_Test_1_0:
	case TLS_Test_1_2_Extensions:
	//case TLS_Test_1_1_Extensions:
	case TLS_Test_1_0_Extensions:
		// if we are testing TLS 1.0 or higher and server responds with SSL 3.0, then assume server has correct version response.
		// Downgrade to SSL 3.0 or show error if SSL 3.0 is not supported
		if(server_version < SSL_ProtocolVersion(3,1))
		{
			feature_status = securityManager->IsTestFeatureAvailable(TLS_Test_SSLv3_only) ? TLS_Test_SSLv3_only : TLS_Version_not_supported;
			renegotiate = TRUE;
		}
		break;
	}

	if(feature_status == TLS_Version_not_supported || (server_version == SSL_ProtocolVersion(3,0) && !securityManager->IsSSLv30Enabled()))
	{
		RaiseAlert(SSL_Fatal, SSL_Protocol_Version_Alert);
		return SSL_KEA_Handle_Errors;
	}
	pending->server_info->SetKnownToSupportVersion(server_version);

	pending->server_info->SetFeatureStatus(feature_status);
	pending->server_info->SetValidTo(g_timecache->CurrentTime() + SSL_VERSION_TEST_VALID_TIME );

#ifndef TLS_NO_CERTSTATUS_EXTENSION
	OP_MEMORY_VAR BOOL seen_cert_status = FALSE;
#endif
	BOOL seen_renego_extension = FALSE;
	if(extensions.GetLength() > 0)
	{
		TLS_ExtensionList ext_list;
		ANCHOR(SSL_LoadAndWritable_list, ext_list);
		TLS_ServerNameList namelist;
		ANCHOR(SSL_LoadAndWritable_list, namelist);

		extensions.ResetRead();
		OP_MEMORY_VAR OP_STATUS op_err2 = OpStatus::OK;
		TRAPD(op_err, op_err2 = ext_list.ReadRecordFromStreamL(&extensions));
		if(OpStatus::IsError(op_err) || op_err2 != OpRecStatus::FINISHED)
		{
			RaiseAlert(SSL_Fatal, SSL_Decode_Error);
			return SSL_KEA_Handle_Errors;
		}

		OP_MEMORY_VAR BOOL seen_servername = FALSE;
		for(OP_MEMORY_VAR uint16 i=0; i< ext_list.Count(); i++)
		{
			switch(ext_list[i].extension_id)
			{
#ifndef TLS_NO_CERTSTATUS_EXTENSION
			case TLS_Ext_StatusRequest:
				if(ext_list[i].extension_data.GetLength() != 0)
				{
					RaiseAlert(SSL_Fatal, SSL_Decode_Error);
					return SSL_KEA_Handle_Errors;
				}

				seen_cert_status = TRUE;
				break;
#endif
				
			case TLS_Ext_RenegotiationInfo:
				if(seen_renego_extension)
					goto ext_decode_error;
				{
					TLS_RenegotiationInfo reneg_info;

					ext_list[i].extension_data.ResetRead();
					TRAPD(op_err, reneg_info.ReadRecordFromStreamL(&ext_list[i].extension_data));

					if(OpStatus::IsError(op_err))
					{
						RaiseAlert(op_err);
						return SSL_KEA_Handle_Errors;
					}
					if(ext_list[i].extension_data.MoreData()) // If there is more data after the extension then there is an error
					{
						RaiseAlert(SSL_Fatal, SSL_Decode_Error);
						return SSL_KEA_Handle_Errors;
					}

					SSL_varvector32 test_info;

					test_info.Concat(pending->last_client_finished, pending->last_server_finished);
					if( (!pending->session->renegotiation_extension_supported && test_info.GetLength() >0) || // Changing to RI in the middle of a connection is not allowed, presume an attack
						test_info != reneg_info.renegotiated_connection)
					{
						RaiseAlert(SSL_Fatal, SSL_Handshake_Failure);
						return SSL_KEA_Handle_Errors;
					}
					seen_renego_extension = TRUE;
				}
				break;

			case TLS_Ext_ServerName:
				if(!seen_servername)
				{
					seen_servername = TRUE;
					if(ext_list[i].extension_data.GetLength() == 0)
						break;
				}
				// error if multiple occurences or non-zero length;
				RaiseAlert(SSL_Fatal, SSL_Decode_Error);
				return SSL_KEA_Handle_Errors;

			case TLS_Ext_NextProtocol:
				{
					TLS_NextProtocols next_protocols;

					ext_list[i].extension_data.ResetRead();
					TRAPD(op_err, next_protocols.ReadRecordFromStreamL(&ext_list[i].extension_data));

					if(OpStatus::IsError(op_err))
					{
						RaiseAlert(op_err);
						return SSL_KEA_Handle_Errors;
					}
					if(ext_list[i].extension_data.MoreData())
					{
						RaiseAlert(SSL_Fatal, SSL_Decode_Error);
						return SSL_KEA_Handle_Errors;
					}
					pending->next_protocols_available.Copy(next_protocols.protocols);
					pending->next_protocol_extension_supported = TRUE;
				}
				break;

			default:
ext_decode_error:;
				RaiseAlert(SSL_Fatal, SSL_Decode_Error);
				return SSL_KEA_Handle_Errors;
			}
		}

	}
#ifndef TLS_NO_CERTSTATUS_EXTENSION
	if(!seen_cert_status)
	{
		pending->session->sent_ocsp_extensions.Resize(0);
		pending->session->ocsp_extensions_sent = FALSE;
	}
#endif

	if (feature_status >= TLS_Final_stage_start && feature_status <= TLS_Final_stage_end)
	{
		if(!seen_renego_extension)
		{
			if((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_RENEG_REFUSE) != 0)
			{
				RaiseAlert(SSL_Fatal, SSL_NoRenegExtSupport);
				return SSL_KEA_Handle_Errors;
			}
		}
	}
	else
	{
		/* If we are in test modus, and we have seen a renegotiation extension we go direct to TLS 1.2 mode*/
		if(seen_renego_extension)
		{

			OP_ASSERT(pending->last_client_finished.GetLength() == 0); // Should only occur for intial handshakes
			pending->session->renegotiation_extension_supported = seen_renego_extension;

			pending->server_info->SetFeatureStatus(TLS_1_2_and_Extensions);
			if (feature_status != TLS_Test_1_2_Extensions)
			{
				return SSL_KEA_FullConnectionRestart;
			}
		}
	}

	pending->session->renegotiation_extension_supported = seen_renego_extension;

	if(renegotiate)
		return SSL_KEA_FullConnectionRestart;

	pending->server_random = random;
	if(!pending->server_random.Valid(&msg))
	{
		RaiseAlert(msg);
		return SSL_KEA_Handle_Errors;
	}
	
	if(!pending->sent_ciphers.Contain(cipherid) ||
		!pending->sent_compression.Contain(compression_method))
	{
		RaiseAlert(SSL_Fatal,SSL_Handshake_Failure);
		return SSL_KEA_Handle_Errors;
    }

	OP_ASSERT(pending->version_specific);
	// Version specific already set before server hello is parsed (needed by the state engine)

	version_specific = pending->version_specific;
	
	session = pending->session;
	if(session->session_negotiated)
	{
		if(session_id == session->sessionID)
		{
			if (cipherid != session->used_cipher ||
				compression_method != session->used_compression)
			{
				RaiseAlert(SSL_Fatal,SSL_Illegal_Parameter);
				return SSL_KEA_Handle_Errors;
			}

			version_specific->SessionUpdate(Session_Resume_Session);
			return SSL_KEA_Resume_Session;
		}
		else
		{
			session->is_resumable = FALSE;
			session->connections++; // To keep the record until after.
			pending->OpenNewSession();
			if (pending->ErrorRaisedFlag)
				return SSL_KEA_Handle_Errors;
			
#ifndef TLS_NO_CERTSTATUS_EXTENSION
			pending->session->sent_ocsp_extensions = session->sent_ocsp_extensions;
			pending->session->ocsp_extensions_sent = session->ocsp_extensions_sent;
#endif
			pending->session->renegotiation_extension_supported = session->renegotiation_extension_supported;

			session->connections--; // Will be deleted automatically later.
			pending->server_info->RemoveSessionRecord(session);

			session = pending->session;
			pending->AddSessionRecord(session);
		}  
	}
	
	session->sessionID= session_id;
	session->last_used = g_timecache->CurrentTime();
	session->used_cipher = cipherid;
	session->used_compression = (SSL_CompressionMethod) compression_method;
	session->is_resumable = (session_id.GetLength() >0);
	session->used_version = pending->version;
	
	if(pending->SetupStartCiphers())
	{
		RaiseAlert(SSL_Fatal,SSL_Handshake_Failure);
		return SSL_KEA_Handle_Errors;
	}

	version_specific->SessionUpdate(pending->anonymous_connection ?  Session_New_Anonymous_Session : Session_New_Session);
#ifndef TLS_NO_CERTSTATUS_EXTENSION
	if(seen_cert_status)
		version_specific->SessionUpdate(Session_TLS_ExpectCertificateStatus);
#endif

	return SSL_KEA_No_Action;
}
#endif // relevant support
