/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
x *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#if !defined(ADS12) || defined(ADS12_TEMPLATING) // see end of streamtype.cpp

#include "modules/libssl/sslbase.h"
#include "modules/libssl/keyex/sslkeyex.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/handshake/certreq.h"
#include "modules/libssl/options/certitem.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/libssl/methods/sslpubkey.h"
#include "modules/libssl/method_disable.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/libssl/smartcard/smc_man.h"
#include "modules/libssl/smartcard/smckey.h"

#include "modules/libssl/debug/tstdump2.h"
#include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"


SSL_KeyExchange::SSL_KeyExchange()
	:  commstate(NULL), 
	client_cert_list(NULL),
	post_verify_action(SSL_KEA_No_Action)
{
    if (g_keyex_id==0)
        g_keyex_id = 1;
	id = g_keyex_id++;

	ForwardToThis(pre_master_secret,	pre_master_secret_clear);
	pre_master_secret_encrypted.ForwardTo(this);

	receive_certificate = Handshake_Undecided;
	receive_server_keys = Handshake_Undecided;
	receive_certificate_request = Handshake_Undecided;
}

SSL_KeyExchange::~SSL_KeyExchange()
{
	InternalDestruct();
}

void SSL_KeyExchange::InternalDestruct()
{
	OP_DELETE(client_cert_list);
	client_cert_list = NULL;
	commstate = NULL;
}

BOOL SSL_KeyExchange::RecvServerKey(uint32 signkey_length)
{
	return FALSE;
}

void SSL_KeyExchange::SetCommState(SSL_ConnectionState *cstate)
{
	ForwardTo(cstate);
	commstate = cstate;
	OP_ASSERT(commstate);
	SetServerName(commstate && commstate->server_info != NULL ? commstate->server_info->GetServerName() : NULL);
	if(commstate)
	{
		SetShowDialog(!commstate->user_interaction_blocked // Don't show dialogs if interaction is disabled
#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
						&& !g_ssl_disable_internal_repository // Don't show dialogs if the internal repository is disabled
#endif
						);
		SetDisplayWindow(commstate->window);
		SetDisplayURL(commstate->ActiveURL);
	}
}

SSL_ASN1Cert_list &SSL_KeyExchange::AccessServerCertificateList()
{
	if(commstate != NULL && commstate->session != NULL)
		return commstate->session->Site_Certificate;
	else
	{
		OP_ASSERT(!"This should never happen");

        if (g_cert_def1==NULL)
            g_cert_def1 = OP_NEW(SSL_ASN1Cert_list, ());

		if(g_cert_def1 != NULL)
			g_cert_def1->Resize(0);
		else
			RaiseAlert(SSL_Internal, SSL_Allocation_Failure);

		return *g_cert_def1;
	}
}

SSL_CertificateHandler_ListHead *SSL_KeyExchange::ClientCertificateCandidates()
{
	SSL_CertificateHandler_ListHead *temp;

	temp = client_cert_list;
	client_cert_list = NULL;
	return temp;
}


static const SSL_SignatureAlgorithm preferred_list_of_signing_alg[] = {
	SSL_RSA_SHA_512,
	SSL_RSA_SHA_256,
	SSL_RSA_SHA_384,
	SSL_RSA_SHA_224,
	SSL_RSA_SHA,
	SSL_DSA_sign,
	SSL_RSA_MD5
};

void SSL_KeyExchange::SelectClientCertificate(SSL_CertificateHandler_List *clientcert, SSL_PublicKeyCipher *clientkey)
{
	SSL_Certificate_indirect_list *item;
	uint24 i,n;

	if(clientcert == NULL)
		return;
	commstate->write.cipher->SignCipherMethod = clientkey;

#ifdef _SUPPORT_TLS_1_2
	if(supported_sig_algs.Count() > 0)
	{
		OP_ASSERT(clientcert->certitem);
		OP_ASSERT(clientcert->certitem->handler);

		if(clientcert->certitem->GetCertificateHandler())
		{
			SSL_SignatureAlgorithm kalg = clientcert->certitem->handler->CertificateSigningKeyAlg(0);

			size_t i;
			for(i= 0; i < ARRAY_SIZE(preferred_list_of_signing_alg); i++)
			{
				SSL_SignatureAlgorithm talg = preferred_list_of_signing_alg[i];
				TLS_SigAndHash alg = talg;
				if(SignAlgToBasicSignAlg(talg) == kalg &&
					supported_sig_algs.Contain(alg))
				{
					SetSigAlgorithm(alg);
					break;
				}
			}
		}
	}
#endif

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	if(clientcert->external_key_item)
	{
		commstate->session->Client_Certificate = clientcert->external_key_item->DERCertificate();
	}
	else
#endif
	{
		n = clientcert->ca_list.Cardinal() +1;
		commstate->session->Client_Certificate.Resize(n);
		commstate->session->Client_Certificate[0] = clientcert->certitem->certificate;

		item = clientcert->ca_list.First();
		i = 1;
		while(item != NULL)
		{
			commstate->session->Client_Certificate[i++] = item->cert_item->certificate;
			item = item->Suc();
		}
	}
	OP_DELETE(clientcert);

}

uint32 SSL_KeyExchange::ProcessClientCertRequest(SSL_CertificateRequest_st &req)
{
	SSL_ClientCertificateType ctype;
	SSL_CertificateHandler_List *candidateitem;
	SSL_CertificateItem *certitem;
	uint16 i,n;
	int status;
	SSL_Alert msg;

	g_securityManager->Init(SSL_LOAD_CLIENT_STORE);

	if(client_cert_list != NULL)
		client_cert_list->Clear();
	else
		client_cert_list = OP_NEW(SSL_CertificateHandler_ListHead, ());

	if(commstate == NULL || client_cert_list == NULL)
		return 0;

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	OP_STATUS op_err;
	op_err = g_external_clientkey_manager->GetAvailableKeysAndCertificates(client_cert_list, req.Authorities, servername, port);
	if(OpStatus::IsError(op_err))
		RaiseAlert(op_err);

	// TODO: Remove this when PINcode and Login support is enabled
	candidateitem = client_cert_list->First();
	while(candidateitem)
	{
		SSL_CertificateHandler_List *current = candidateitem;
		candidateitem = candidateitem->Suc();

		if(current->external_key_item && current->external_key_item->AccessKey() &&  
			(current->external_key_item->AccessKey()->Need_Login() ||
				current->external_key_item->AccessKey()->Need_PIN()))
		{
			// Can't currently handle Login or PIN
			current->Out();
			OP_DELETE(current);
		}
	}
#endif

	if(g_securityManager->System_ClientCerts.Empty())
		return client_cert_list->Cardinal();

	candidateitem = NULL;
	n = req.GetCtypeCount();
	certitem = g_securityManager->System_ClientCerts.First();
	while(certitem != NULL)
	{
		ctype = certitem->certificatetype;
		for(i=0;i < n;i ++)
		{
			if(req.CtypeItem((uint8) i) == ctype)
				break;
		}

		if(i<n)
		{
			if(candidateitem == NULL)
				candidateitem = OP_NEW(SSL_CertificateHandler_List, ());
			else
			{
				candidateitem->ca_list.Clear();
				candidateitem->certitem = NULL;
			}
			if(candidateitem == NULL)
				break;

			msg.ResetError();
			status = g_securityManager->BuildChain(candidateitem, certitem, req.Authorities, msg);

			if(msg.ErrorRaisedFlag)
				RaiseAlert(msg);
			else if(status >= 0 && candidateitem->certitem && candidateitem->certitem->handler != NULL)
			{
				candidateitem->Into(client_cert_list);
				candidateitem = NULL;
			}
		}

		certitem = certitem->Suc();
	}

	if(candidateitem != NULL)
		OP_DELETE(candidateitem);

#ifdef _SUPPORT_TLS_1_2
	if(AccessConnectionState()->version >= SSL_ProtocolVersion(3,3))
	{
		supported_sig_algs = req.supported_sig_algs;

		if(supported_sig_algs.Count() == 0)
		{
			client_cert_list->Clear(); // No algs, no certs ...
		}
		else
		{
			SSL_CertificateHandler_List *next_candidateitem = client_cert_list->First();
			while(next_candidateitem)
			{
				candidateitem = next_candidateitem;
				next_candidateitem = next_candidateitem->Suc();

				OP_ASSERT(candidateitem->certitem);
				OP_ASSERT(candidateitem->certitem->handler);

				if(candidateitem->certitem->GetCertificateHandler())
				{
					BOOL acceptable = FALSE;
					TLS_SigAndHash alg = candidateitem->certitem->handler->CertificateSignatureAlg(0);

					if(supported_sig_algs.Contain(alg))
					{
						SSL_SignatureAlgorithm kalg = candidateitem->certitem->handler->CertificateSigningKeyAlg(0);

						size_t i;
						for(i= 0; i < ARRAY_SIZE(preferred_list_of_signing_alg); i++)
						{
							SSL_SignatureAlgorithm talg = preferred_list_of_signing_alg[i];
							alg = talg;
							if(SignAlgToBasicSignAlg(talg) == kalg &&
								supported_sig_algs.Contain(alg))
							{
								acceptable = TRUE;
								break;
							}
						}
					}

					if(!acceptable)
					{
						candidateitem->Out();
						OP_DELETE(candidateitem);
					}
				}
			}

		}
	}
#endif

	return client_cert_list->Cardinal();
}

int SSL_KeyExchange::SetupServerKeys(SSL_Server_Key_Exchange_st *serverkeys)
{
	return 0;
}

SSL_KEA_ACTION SSL_KeyExchange::ReceivedCertificate() // Certificate in commstate
{
	if(receive_certificate != Handshake_Expected)
	{
		  RaiseAlert(SSL_Fatal, SSL_Unexpected_Message);
		  return SSL_KEA_Handle_Errors;
	}

	receive_certificate = Handshake_Received;


#ifdef _DEBUG
#ifdef YNP_WORK
	TruncateDebugFile("crt1.der");
	TruncateDebugFile("crt2.der");
	TruncateDebugFile("crt3.der");
	BinaryDumpTofile(commstate->session->Site_Certificate[0],"crt1.der");
	if(commstate->session->Site_Certificate.Count() > 1)
	{
		BinaryDumpTofile(commstate->session->Site_Certificate[1],"crt2.der");
	}
	if(commstate->session->Site_Certificate.Count() > 2)
	{
		BinaryDumpTofile(commstate->session->Site_Certificate[2],"crt3.der");
	}
#endif
#endif

	SSL_Alert errormsg;

	SetCertificate(commstate->session->Site_Certificate);
	SetCipherDescription(commstate->session->cipherdescription);
	SetProtocolVersion(commstate->session->used_version);
	SetCheckServerName(TRUE);
	SetHostName(commstate->server_info);
#ifndef TLS_NO_CERTSTATUS_EXTENSION
	SetSentOCSPExtensions(commstate->session->ocsp_extensions_sent,
			commstate->session->sent_ocsp_extensions,commstate->session->received_ocsp_response);
#endif
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	if(!commstate->session->renegotiation_extension_supported && 
		(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & (CRYPTO_METHOD_RENEG_DISABLE_EV | CRYPTO_METHOD_RENEG_WARN)) != 0)
	{
		SetCheckExtendedValidation(FALSE);
	}
#endif
	if(Error())
		return SSL_KEA_Handle_Errors;

	SSL_CertificateVerifier::VerifyStatus verify_status = PerformVerifySiteCertificate(&errormsg);

	if(Error())
		return SSL_KEA_Handle_Errors;

	if(verify_status == SSL_CertificateVerifier::Verify_Failed)
	{
		VerifyFailedStep(errormsg);
		return SSL_KEA_Handle_Errors;
	}

	if(verify_status == SSL_CertificateVerifier::Verify_Started)
		return SSL_KEA_Wait_For_KeyExchange;

	VerifySucceededStep();

	if(Error())
		return SSL_KEA_Handle_Errors;

	return SSL_KEA_No_Action;

}

int  SSL_KeyExchange::VerifyCheckExtraWarnings(int &low_security_reason)
{
	int warn_flag = 0;
	if(commstate->session && !commstate->session->renegotiation_extension_supported && 
		(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_RENEG_WARN) != 0)
	{
		warn_flag |= SSL_NO_RENEG_EXTSUPPORT;
		commstate->session->security_rating = SECURITY_STATE_LOW;
		low_security_reason |= SECURITY_LOW_REASON_WEAK_PROTOCOL;
	}

	return warn_flag;
}

void SSL_KeyExchange::VerifyFailed(SSL_Alert &msg)
{
	VerifyFailedStep(msg);
	post_verify_action = SSL_KEA_Handle_Errors;

	g_main_message_handler->PostMessage(MSG_SSL_COMPLETEDVERIFICATION, Id(), FALSE);
}

void SSL_KeyExchange::VerifyFailedStep(SSL_Alert &msg)
{
	RaiseAlert(msg);
	if(GetConfirmedMode() == USER_REJECTED)
		commstate->session->UserConfirmed = USER_REJECTED;
	GetOCSPFailreason(commstate->session->ocsp_fail_reason);
}

void SSL_KeyExchange::VerifySucceeded(SSL_Alert &msg)
{
	VerifySucceededStep();

	g_main_message_handler->PostMessage(MSG_SSL_COMPLETEDVERIFICATION, Id(), (post_verify_action != SSL_KEA_Handle_Errors));
}

void SSL_KeyExchange::VerifySucceededStep()
{
	OP_DELETE(commstate->server_cert_handler);
	commstate->server_cert_handler = ExtractCertificateHandler();
	commstate->session->Validated_Site_Certificate = GetValidatedCertificate();
	commstate->session->Matched_name.Set(GetMatched_name());
	GetCertificateNames(commstate->session->CertificateNames);
	commstate->session->certificate_status = GetCertificateStatus();
	commstate->session->UserConfirmed = GetConfirmedMode();
	commstate->session->security_rating = GetSecurityRating();
	commstate->session->low_security_reason = GetLowSecurityReason();
	commstate->session->lowest_keyex.Update(GetLowKeySizes());
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	commstate->session->extended_validation = GetExtendedValidation();
#endif
	GetOCSPFailreason(commstate->session->ocsp_fail_reason);
	commstate->DetermineSecurityStrength(NULL);
	post_verify_action = SSL_KEA_No_Action;

	SetupServerCertificateCiphers();

	if(Error())
		post_verify_action = SSL_KEA_Handle_Errors;
}


SSL_KEA_ACTION SSL_KeyExchange::ReceivedServerKeys(SSL_Server_Key_Exchange_st *serverkeys) // Signature have been verified by caller
{
	if(!serverkeys)
		return SSL_KEA_No_Action;

	if(receive_server_keys != Handshake_Expected)
	{
		RaiseAlert(SSL_Fatal, SSL_Unexpected_Message);
		return SSL_KEA_Handle_Errors;
	}

	receive_server_keys = Handshake_Received;

	if(SetupServerKeys(serverkeys))
		return SSL_KEA_Handle_Errors;

	if(commstate->session->security_rating < SECURITY_STATE_HALF && 
		commstate->session->UserConfirmed != PERMANENTLY_CONFIRMED && 
		commstate->session->UserConfirmed != SESSION_CONFIRMED )
	{
		if(!show_dialog
#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
			// Do not allow user interaction for strict transport security connections.
			// This is to avoid click-through-security.
			|| servername->SupportsStrictTransportSecurity()
#endif // LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
		)
		{
			RaiseAlert(SSL_Fatal, SSL_Insufficient_Security);
			return SSL_KEA_Handle_Errors;
		}

		certificate_ctx.reset(OP_NEW(SSL_Certificate_DisplayContext, (IDM_SSL_LOW_ENCRYPTION_LEVEL)));
		if(certificate_ctx.get() == NULL)
		{
			RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
			return SSL_KEA_Handle_Errors;
		}

		certificate_ctx->SetCertificateList(commstate->session->Site_Certificate);

		if(!commstate->anonymous_connection)
			certificate_ctx->SetCertificateChain(commstate->server_cert_handler->Fork(), FALSE);

		certificate_ctx->SetURL(DisplayURL);
		certificate_ctx->SetServerInformation(commstate->server_info->GetServerName(), commstate->server_info->Port());
		certificate_ctx->SetCipherName(commstate->session->cipherdescription->fullname);
		certificate_ctx->SetCipherVersion(commstate->session->used_version);
		SSL_Alert alert;
		alert.Set(SSL_Fatal, SSL_Insufficient_Security);
		certificate_ctx->SetAlertMessage(alert);
		certificate_ctx->SetCompleteMessage(MSG_SSL_WEAK_KEY_WARNING_HANDLED, (UINTPTR) this);
		certificate_ctx->SetRemberAcceptAndRefuse(TRUE);

		if((commstate->session->low_security_reason & SECURITY_LOW_REASON_WEAK_KEY) != 0)
		{
			certificate_ctx->AddCertificateComment(Str::S_WEAK_CERTIFICATE_KEY, NULL, NULL);
		}
		if((commstate->session->low_security_reason & SECURITY_LOW_REASON_SLIGHTLY_WEAK_KEY) != 0)
		{
			certificate_ctx->AddCertificateComment(Str::S_SLIGHTLY_WEAK_CERTIFICATE_KEY, NULL, NULL);
		}

		certificate_ctx->SetWindow(window);

		if(OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_SSL_WEAK_KEY_WARNING_HANDLED, (UINTPTR) this)) ||
			!InitSecurityCertBrowsing(NULL, certificate_ctx.get()))
		{
			RaiseAlert(SSL_Internal, SSL_InternalError);
			certificate_ctx.reset();
			return SSL_KEA_Handle_Errors;
		}

		return SSL_KEA_Wait_For_KeyExchange;
	}
	
	return SSL_KEA_No_Action;
}

SSL_KEA_ACTION SSL_KeyExchange::ReceivedCertificateRequest(SSL_CertificateRequest_st *certreq)
{
	if(!certreq)
		return SSL_KEA_No_Action;

	if(receive_certificate_request != Handshake_Expected)
	{
		RaiseAlert(SSL_Fatal, SSL_Unexpected_Message);
		return SSL_KEA_Handle_Errors;
	}

	UINT32 count = ProcessClientCertRequest(*certreq);

	switch(commstate->server_info->GetFeatureStatus())
	{
	case TLS_Test_1_0:
	//case TLS_Test_1_1_Extensions:
	case TLS_Test_1_2_Extensions:
	case TLS_Test_1_0_Extensions:
		commstate->version_specific->SessionUpdate(Session_No_Certificate);
		return  (ErrorRaisedFlag ? SSL_KEA_Handle_Errors : (count == 0 ? SSL_KEA_No_Action : SSL_KEA_Application_Delay_Certificate_Request));
	default:
		receive_certificate_request = Handshake_Received;
		break;
	}

	if(count>0)
	{
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
		SSL_CertificateHandler_List *candidateitem = client_cert_list->First();
		while(candidateitem)
		{
			if(candidateitem->external_key_item && candidateitem->external_key_item->AccessKey() &&  
				candidateitem->external_key_item->AccessKey()->UseAutomatically())
			{
				// Automatically select this certificate
				candidateitem->Out();
				server_info->SetCertificate(candidateitem);
				OP_DELETE(candidateitem);
				break;
			}

			candidateitem = candidateitem->Suc();
		}
#endif

		if(server_info->HasCertificate())
		{
			return SetupPrivateKey(FALSE);
		}

		do{

			if(commstate->user_interaction_blocked)
				break;
			certificate_ctx.reset(OP_NEW(SSL_Certificate_DisplayContext, (IDM_SELECT_USER_CERTIFICATE)));

			if(certificate_ctx.get() == NULL)
			{
				RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
				break;
			}

			certificate_ctx->SetExternalOptionsManager(g_securityManager);
			certificate_ctx->SetCertificateSelectList(client_cert_list);
			client_cert_list = NULL;

			if(commstate->session != NULL)
			{
				certificate_ctx->SetCipherVersion(commstate->session->used_version);
				certificate_ctx->SetCipherName(commstate->session->cipherdescription->fullname);

				certificate_ctx->SetURL(commstate->ActiveURL);
				certificate_ctx->SetServerInformation(servername, port);
			}

			certificate_ctx->SetWindow(commstate->window);

			certificate_ctx->SetCompleteMessage(MSG_SSL_CLIENT_CERT_SELECTED, (UINTPTR) this);
			if(OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_SSL_CLIENT_CERT_SELECTED, (UINTPTR) this)) ||
				!InitSecurityCertBrowsing(NULL, certificate_ctx.get()))
			{
				RaiseAlert(SSL_Internal, SSL_InternalError);
				return SSL_KEA_Handle_Errors;
			}
			return SSL_KEA_Wait_For_KeyExchange;

		}
		while(0);
	}

	certificate_ctx.reset();
	commstate->version_specific->SessionUpdate(Session_No_Certificate);

	return (ErrorRaisedFlag ? SSL_KEA_Handle_Errors : SSL_KEA_No_Action);
}

SSL_KEA_ACTION SSL_KeyExchange::SetupPrivateKey(BOOL asked_for_password)
{
#ifndef USE_SSL_CERTINSTALLER
	commstate->version_specific->SessionUpdate(Session_No_Certificate);
	return SSL_KEA_No_Action;
#else
	SSL_CertificateHandler_List *cert = NULL;
	SSL_PublicKeyCipher *key=NULL;

	if(server_info->HasCertificate())
		cert = server_info->GetCertificate();

	if(!cert)
	{
		commstate->version_specific->SessionUpdate(Session_No_Certificate);
		return SSL_KEA_No_Action;
	}

	SSL_secure_varvector32 decryptedkey;
	SSL_Alert msg;
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	if(cert->external_key_item)
	{
		OP_ASSERT(cert->external_key_item->AccessKey());
		// TODO: Add UI handling for Login
		OP_ASSERT(!cert->external_key_item->AccessKey()->Need_Login());
		// TODO: Add handling for PIN
		OP_ASSERT(!cert->external_key_item->AccessKey()->Need_PIN());

		// Get the private key
		key = cert->external_key_item->GetKey();
		if(!key)
			msg.RaiseAlert(OpStatus::ERR_NO_MEMORY);
	}
	else
#endif
	{
		SSL_dialog_config config(commstate->window, g_main_message_handler, MSG_SSL_CLIENT_KEY_WAIT_FOR_PASSWORD, 0, commstate->ActiveURL);

		if(!asked_for_password)
			g_securityManager->StartSecurityPasswordSession();

		OP_STATUS op_err = OpStatus::OK;
		key = g_securityManager->FindPrivateKey(op_err, cert, decryptedkey, msg, config);

		if(op_err == InstallerStatus::ERR_PASSWORD_NEEDED)
		{
			OP_DELETE(cert);
			OP_DELETE(key);
			g_main_message_handler->SetCallBack(this, MSG_SSL_CLIENT_KEY_WAIT_FOR_PASSWORD, 0);
			return SSL_KEA_Wait_For_KeyExchange;
		}
		g_securityManager->EndSecurityPasswordSession();
	}

	if(key == NULL)
	{
		OP_DELETE(cert);
		RaiseAlert(msg);
		return SSL_KEA_Handle_Errors;
	}

	if (!g_securityManager->EncryptClientCertificates())
	{
		/* This makes sure the certificate will not be encrypted again, as long as the user
		 * prefers not to encrypt certificates. */
		if (cert && cert->certitem)
		{
			OP_ASSERT(decryptedkey.GetLength() > 0);
			cert->certitem->private_key.Set(decryptedkey);
			cert->certitem->private_key_salt.Resize(0);
		}
	}


	SelectClientCertificate(cert,key);

	commstate->version_specific->SessionUpdate(Session_Certificate_Configured);

	return SSL_KEA_No_Action;
#endif
}

BOOL SSL_KeyExchange::GetIsAnonymous()
{
	return FALSE;
}

void SSL_KeyExchange::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
	case MSG_SSL_WEAK_KEY_WARNING_HANDLED:
		{
			g_main_message_handler->UnsetCallBack(this, MSG_SSL_WEAK_KEY_WARNING_HANDLED);
			post_verify_action = SSL_KEA_No_Action;
			BOOL res = FALSE;
			do{
				if(!certificate_ctx.get())
					break;

				BOOL result = (certificate_ctx->GetFinalAction() == SSL_CERT_ACCEPT ? TRUE : FALSE);
				if(result)
				{
					SSL_AcceptCert_Item *accept_item = OP_NEW(SSL_AcceptCert_Item, ());

					if(accept_item)
					{
						accept_item->certificate = commstate->session->Site_Certificate[0];
						accept_item->AcceptedAs.Set(commstate->session->Matched_name);
						accept_item->confirm_mode = (certificate_ctx->GetRemberAcceptAndRefuse() ? PERMANENTLY_CONFIRMED : SESSION_CONFIRMED);
						unsigned long i, count = commstate->session->CertificateNames.Count();
						if(OpStatus::IsSuccess(accept_item->Certificate_names.Resize(count)))
						{
							for(i = 0; i < count; i++)
								accept_item->Certificate_names[i].Set(commstate->session->CertificateNames[i]);
						}
						accept_item->certificate_status = commstate->session->certificate_status;

						commstate->server_info->AddAcceptedCertificate(accept_item);
					}
					commstate->session->UserConfirmed = (certificate_ctx->GetRemberAcceptAndRefuse() ? PERMANENTLY_CONFIRMED : SESSION_CONFIRMED);
					commstate->session->security_rating = SECURITY_STATE_LOW;
					commstate->session->low_security_reason |= SECURITY_LOW_REASON_CERTIFICATE_WARNING;
				}
				else
				{
					RaiseAlert(certificate_ctx->GetAlertMessage());
					post_verify_action = SSL_KEA_Handle_Errors;
				}
			}while(0);
			certificate_ctx.reset();
			g_main_message_handler->PostMessage(MSG_SSL_COMPLETEDVERIFICATION, Id(), res);
		}
		break;
	case MSG_SSL_CLIENT_CERT_SELECTED:
		g_main_message_handler->UnsetCallBack(this, MSG_SSL_CLIENT_CERT_SELECTED);
		do{
			SSL_CertificateHandler_List *cert = NULL ;
			if(certificate_ctx.get() == NULL ||
				certificate_ctx->GetFinalAction() != SSL_CERT_ACCEPT || 
				!certificate_ctx->GetCurrentCertificateNumber()	||
				!certificate_ctx->GetClientCertificateCandidates() ||
				(cert = certificate_ctx->GetClientCertificateCandidates()->Item(certificate_ctx->GetCurrentCertificateNumber()-1)) == NULL)
			{
				commstate->version_specific->SessionUpdate(Session_No_Certificate);
				post_verify_action = SSL_KEA_No_Action;
				break;
			}

			cert->Out();
			server_info->SetCertificate(cert);
			OP_DELETE(cert);

			post_verify_action = SetupPrivateKey(FALSE);
		}while(0);
		certificate_ctx.reset();
		if(post_verify_action == SSL_KEA_Wait_For_KeyExchange)
		{
			return; // password initiated
		}
		g_main_message_handler->PostMessage(MSG_SSL_COMPLETEDVERIFICATION, Id(), (post_verify_action != SSL_KEA_Handle_Errors));
		break;
	case MSG_SSL_CLIENT_KEY_WAIT_FOR_PASSWORD:
		g_main_message_handler->UnsetCallBack(this, MSG_SSL_CLIENT_KEY_WAIT_FOR_PASSWORD);
		if(par2 == IDOK)
		{
			post_verify_action = SetupPrivateKey(TRUE);
			if(post_verify_action == SSL_KEA_Wait_For_KeyExchange)
				return; // password initiated
		}
		else
		{
			commstate->version_specific->SessionUpdate(Session_No_Certificate);
			post_verify_action = SSL_KEA_No_Action;
		}
		g_main_message_handler->PostMessage(MSG_SSL_COMPLETEDVERIFICATION, Id(), (post_verify_action != SSL_KEA_Handle_Errors));
		break;
	default:
		SSL_CertificateVerifier::HandleCallback(msg, par1, par2);
		break;
	}
}

#endif // !ADS12 or ADS12_TEMPLATING
#endif
