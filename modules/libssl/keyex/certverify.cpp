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
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/keyex/certverify.h"
#include "modules/libssl/certs/verifyinfo.h"
#include "modules/libssl/methods/sslpubkey.h"
#include "modules/libssl/certs/certinstaller.h"
#include "modules/libssl/certs/certhandler.h"
#include "modules/libssl/protocol/sslstat.h"
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
#include "modules/libssl/data/root_auto_retrieve.h"
#endif

#include "modules/prefs/prefsmanager/collections/pc_network.h"

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::PerformVerifySiteCertificate(SSL_Alert *msg)
{
	verify_msg = SSL_Alert();

	SSL_CertificateVerifier::VerifyStatus ret = IterateVerifyCertificate(&verify_msg);

	if(msg)
		*msg = verify_msg;

	return ret;
}

void SSL_CertificateVerifier::ProgressVerifySiteCertificate()
{
	if(verify_state == SSL_CertificateVerifier::Verify_Aborted)
		return;

	SSL_CertificateVerifier::VerifyStatus ret = IterateVerifyCertificate(&verify_msg);

	if(ret == SSL_CertificateVerifier::Verify_Failed)
	{
		VerifyFailed(verify_msg);
		return;
	}

	if(ret != SSL_CertificateVerifier::Verify_Completed)
		return;

	VerifySucceeded(verify_msg);
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::IterateVerifyCertificate(SSL_Alert *msg)
{
	SSL_CertificateVerifier::VerifyStatus ret;

	if(doing_iteration)
		return SSL_CertificateVerifier::Verify_Started;

	doing_iteration = TRUE;

	/*
	 * State machine for asynchronous certificate verification.
	 *
	 * This loop runs tasks given by the member variable verify_state.
	 *
	 * Each task set a next_state which is the task that will run when
	 * the current task is done.
	 *
	 * Each task returns a task status. The loop runs as long as the
	 * tasks returns Verify_TaskCompleted.
	 *
	 * If a task returns Verify_Started, the task must run asynchronously.
	 * The loop will be broken off and continue later when the asynchronous
	 * task has finished.
	 *
	 * If a task returns Verify_Failed the state machine is broken off, and
	 * verification has failed.
	 *
	 * If a task returns Verify_Completed the state machine is broken off, and
	 * verification is a success.
	 */

	SSL_CertificateVerifier::VerifyProgress next_state = verify_state;

	do{
		verify_state = next_state;

		switch(verify_state)
		{
		case SSL_CertificateVerifier::Verify_NotStarted:
			ret = SSL_CertificateVerifier::Verify_TaskCompleted;
			next_state = SSL_CertificateVerifier::Verify_Init;
			break;

		case SSL_CertificateVerifier::Verify_Init:
			ret = VerifyCertificate_Init(msg);
			next_state = SSL_CertificateVerifier::Verify_CheckingCertificate;
			break;

		case SSL_CertificateVerifier::Verify_CheckingCertificate:
			ret = VerifyCertificate_CheckCert(msg);
			next_state = SSL_CertificateVerifier::Verify_UpdateIntermediates;
			break;

		case SSL_CertificateVerifier::Verify_UpdateIntermediates:
			ret = VerifyCertificate_AddIntermediates(msg);
			next_state = SSL_CertificateVerifier::Verify_VerifyCertificate;
			break;

		case SSL_CertificateVerifier::Verify_VerifyCertificate:
			{
				SSL_Alert errormsg;

				ret = VerifyCertificate(&errormsg);

				SSL_AlertDescription verify_error = errormsg.GetDescription();

				SSL_CertificateVerifier::VerifyStatus ocsp_ret;
				// Only check if not checked before and when not loading anything else than CRLs
				if (!certificate_checked_for_ocsp && (verify_error == SSL_Unloaded_CRLs_Or_OCSP || ret == Verify_TaskCompleted))
				{
					ocsp_ret = CheckOCSP();
					if (ocsp_ret == Verify_Started)
					{
						ret = Verify_Failed;
						certificate_loading_ocsp = TRUE;
					}

					certificate_checked_for_ocsp = TRUE;
					verify_error = SSL_Unloaded_CRLs_Or_OCSP;
				}

				if(ret != SSL_CertificateVerifier::Verify_TaskCompleted || certificate_loading_ocsp)
				{
					switch(verify_error)
					{
					case SSL_Unloaded_CRLs_Or_OCSP:
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
						loaded_crls = FALSE;
#endif // LIBSSL_ENABLE_CRL_SUPPORT
						next_state = SSL_CertificateVerifier::Verify_LoadingCRLOrOCSP;
						ret = SSL_CertificateVerifier::Verify_TaskCompleted;
						break;
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
					case SSL_Unknown_CA_RequestingUpdate:
						next_state = SSL_CertificateVerifier::Verify_LoadingRepositoryCert;
						ret = SSL_CertificateVerifier::Verify_TaskCompleted;
						break;
#endif
#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
					case SSL_Unknown_CA_RequestingExternalUpdate:
						next_state = SSL_CertificateVerifier::Verify_LoadingExternalRepositoryCert;
						ret = SSL_CertificateVerifier::Verify_TaskCompleted;
						break;
#endif
					default:
						if(msg)
							*msg=errormsg;
						next_state = SSL_CertificateVerifier::Verify_CheckingUntrusted;
						break;
					}
				}
				else
				{
					if(msg)
						*msg=errormsg;
					next_state = SSL_CertificateVerifier::Verify_CheckingUntrusted;
				}
				break;
			}
		case SSL_CertificateVerifier::Verify_LoadingAIACert:
			ret = (StartPendingURL(this) ? SSL_CertificateVerifier::Verify_Started : SSL_CertificateVerifier::Verify_TaskCompleted);
			/** Specifying verify, just in case the load does not start because it has already been loaded */
			next_state = SSL_CertificateVerifier::Verify_VerifyCertificate;
			break;
		case SSL_CertificateVerifier::Verify_LoadingCRLOrOCSP:
		{
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
			BOOL loading_crl_started = StartLoadingCrls(this);
#else
			BOOL loading_crl_started = FALSE;
#endif // LIBSSL_ENABLE_CRL_SUPPORT
			BOOL loading_ocsp_started = StartPendingURL(this);

			if (loading_crl_started || loading_ocsp_started)
				ret = SSL_CertificateVerifier::Verify_Started;
			else
				ret = SSL_CertificateVerifier::Verify_TaskCompleted;

			/** Specifying verify, just in case the load does not start because it has already been loaded */
			next_state = SSL_CertificateVerifier::Verify_VerifyCertificate;
			break;
		}
		case SSL_CertificateVerifier::Verify_LoadingRepositoryCert:
			{
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
				MH_PARAM_1 fin_id=0;
				if(OpStatus::IsSuccess(StartRootRetrieval(fin_id)) &&
					(g_main_message_handler->HasCallBack(this, MSG_SSL_AUTOFETCH_FINISHED, fin_id) || 
					OpStatus::IsSuccess(g_main_message_handler->SetCallBack(this, MSG_SSL_AUTOFETCH_FINISHED, fin_id)) ))
					ret = SSL_CertificateVerifier::Verify_Started;
				else
#endif // LIBSSL_AUTO_UPDATE_ROOTS
					ret =  SSL_CertificateVerifier::Verify_TaskCompleted;
				/** Specifying verify, just in case the load does not start because it has already been loaded */
				next_state = SSL_CertificateVerifier::Verify_VerifyCertificate;
			}
			break;
		case SSL_CertificateVerifier::Verify_LoadingUntrustedCert:
			{
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
				MH_PARAM_1 fin_id=0;
				if(OpStatus::IsSuccess(StartUntrustedRetrieval(fin_id)) &&
					(g_main_message_handler->HasCallBack(this, MSG_SSL_AUTOFETCH_FINISHED, fin_id) || 
					OpStatus::IsSuccess(g_main_message_handler->SetCallBack(this, MSG_SSL_AUTOFETCH_FINISHED, fin_id)) ))
					ret = SSL_CertificateVerifier::Verify_Started;
				else
#endif // LIBSSL_AUTO_UPDATE_ROOTS
					ret =  SSL_CertificateVerifier::Verify_TaskCompleted;
				/** Specifying verify, just in case the load does not start because it has already been loaded */
				next_state = SSL_CertificateVerifier::Verify_VerifyCertificate;
			}
			/** Specifying verify, just in case the load does not start because it has already been loaded */
			next_state = SSL_CertificateVerifier::Verify_VerifyCertificate;
			break;
		case SSL_CertificateVerifier::Verify_WaitForRepository:
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
			if(OpStatus::IsSuccess(g_main_message_handler->SetCallBack(this, MSG_SSL_FINISHED_AUTOUPDATE_BATCH, 0)) )
				ret = SSL_CertificateVerifier::Verify_Started;
			else
#endif // LIBSSL_AUTO_UPDATE_ROOTS
				ret = SSL_CertificateVerifier::Verify_TaskCompleted;

			/** Specifying verify, just in case the load does not start because it has already been loaded */
			next_state = SSL_CertificateVerifier::Verify_VerifyCertificate;
			break;

			/*	The following will always trigger a new verification attempt, even if they failed.
			 *	Failure may affect the security level, if relevant.
			 */
#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
		case SSL_CertificateVerifier::Verify_LoadingExternalRepositoryCert:
			if(!g_main_message_handler->HasCallBack(this, MSG_SSL_EXTERNAL_REPOSITORY_LOADED, 0))
				g_main_message_handler->SetCallBack(this, MSG_SSL_EXTERNAL_REPOSITORY_LOADED, 0);

			ret = SSL_CertificateVerifier::Verify_Started;
			break;
		case SSL_CertificateVerifier::Verify_LoadingExternalRepositoryCertCompleted:
			next_state = SSL_CertificateVerifier::Verify_VerifyCertificate;
			ret = SSL_CertificateVerifier::Verify_TaskCompleted;
			break;
#endif // SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
		case SSL_CertificateVerifier::Verify_LoadingAIACertCompleted:
            next_state = SSL_CertificateVerifier::Verify_UpdateIntermediates;
             ret = SSL_CertificateVerifier::Verify_TaskCompleted;
             break;
		case SSL_CertificateVerifier::Verify_LoadingCRLCompleted:
		case SSL_CertificateVerifier::Verify_LoadingRepositoryCertCompleted:
		case SSL_CertificateVerifier::Verify_WaitForRepositoryCompleted:
			next_state = SSL_CertificateVerifier::Verify_VerifyCertificate;
			ret = SSL_CertificateVerifier::Verify_TaskCompleted;
			break;
		case SSL_CertificateVerifier::Verify_LoadingUntrustedCertCompleted:
			next_state = SSL_CertificateVerifier::Verify_CheckingUntrusted;
			ret = SSL_CertificateVerifier::Verify_TaskCompleted;
			break;

		case SSL_CertificateVerifier::Verify_CheckingUntrusted:
			{
				SSL_Alert errormsg;
				ret = VerifyCertificate_CheckUntrusted(&errormsg);
				if(ret != SSL_CertificateVerifier::Verify_TaskCompleted)
				{
					switch(errormsg.GetDescription())
					{
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
					case SSL_Untrusted_Cert_RequestingUpdate:
						next_state = SSL_CertificateVerifier::Verify_LoadingUntrustedCert;
						ret = SSL_CertificateVerifier::Verify_TaskCompleted;
						break;
					case SSL_WaitForRepository:
						next_state = SSL_CertificateVerifier::Verify_WaitForRepository;
						ret = SSL_CertificateVerifier::Verify_TaskCompleted;
						break;
#endif // LIBSSL_AUTO_UPDATE_ROOTS
					default:
						if(msg)
							*msg=errormsg;
						next_state = SSL_CertificateVerifier::Verify_CheckingKeysize;
						break;
					}
				}
				else
				{
					if(msg)
						*msg=errormsg;
					next_state = SSL_CertificateVerifier::Verify_CheckingKeysize;
				}
				break;
			}

		case SSL_CertificateVerifier::Verify_CheckingKeysize:
			ret = VerifyCertificate_CheckKeySize(msg);
			next_state = SSL_CertificateVerifier::Verify_ExtractNames;
			break;

		case SSL_CertificateVerifier::Verify_ExtractNames:
			ret = VerifyCertificate_ExtractNames(msg);
			next_state = SSL_CertificateVerifier::Verify_ExtractVerifyStatus;
			break;

		case SSL_CertificateVerifier::Verify_ExtractVerifyStatus:
			ret = VerifyCertificate_ExtractVerificationStatus(msg);
			next_state = SSL_CertificateVerifier::Verify_CheckingMissingCertificate;
			break;
		case SSL_CertificateVerifier::Verify_CheckingMissingCertificate:
			{
				SSL_Alert errormsg;
				ret = VerifyCertificate_CheckMissingCerts(&errormsg);
				if(ret != SSL_CertificateVerifier::Verify_TaskCompleted)
				{
					switch(errormsg.GetDescription())
					{
					case SSL_Unknown_CA_WithAIA_URL:
						next_state = SSL_CertificateVerifier::Verify_LoadingAIACert;
						ret = SSL_CertificateVerifier::Verify_TaskCompleted;
						break;
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
					case SSL_Unknown_CA_RequestingUpdate:
						next_state = SSL_CertificateVerifier::Verify_LoadingRepositoryCert;
						ret = SSL_CertificateVerifier::Verify_TaskCompleted;
						break;
					case SSL_WaitForRepository:
						next_state = SSL_CertificateVerifier::Verify_WaitForRepository;
						ret = SSL_CertificateVerifier::Verify_TaskCompleted;
						break;
#endif // LIBSSL_AUTO_UPDATE_ROOTS
					default:
						if(msg)
							*msg=errormsg;
						next_state = SSL_CertificateVerifier::Verify_CheckingWarnings;
						break;
					}
				}
				else
				{
					if(msg)
						*msg=errormsg;
					next_state = SSL_CertificateVerifier::Verify_CheckingWarnings;
				}
				break;
			}

		case SSL_CertificateVerifier::Verify_CheckingWarnings:
			ret = VerifyCertificate_CheckWarnings(msg);
			next_state = SSL_CertificateVerifier::Verify_HandleDownloadedIntermediates;
			break;

		case SSL_CertificateVerifier::Verify_HandleDownloadedIntermediates:
			ret = VerifyCertificate_HandleDownloadedIntermediates(msg);
			next_state = SSL_CertificateVerifier::Verify_CheckHostName;
			break;

		case SSL_CertificateVerifier::Verify_CheckHostName:
			ret = VerifyCertificate_CheckName(msg);
			next_state = SSL_CertificateVerifier::Verify_CheckTrustedCertForHost;
			break;

		case SSL_CertificateVerifier::Verify_CheckTrustedCertForHost:
			ret = VerifyCertificate_CheckTrusted(msg);
			next_state = SSL_CertificateVerifier::Verify_CheckAskUser;
			break;

		case SSL_CertificateVerifier::Verify_CheckAskUser:
			ret = VerifyCertificate_CheckInteractionNeeded(msg);
			if(ret == SSL_CertificateVerifier::Verify_Started)
			{
				ret = SSL_CertificateVerifier::Verify_TaskCompleted;
				next_state = SSL_CertificateVerifier::Verify_AskingUser;
			}
			else
				next_state = SSL_CertificateVerifier::Verify_FinishedSuccessfully;
			break;

		case SSL_CertificateVerifier::Verify_AskingUser:
			ret = SSL_CertificateVerifier::Verify_Started;
			break;

		case Verify_AskingUserCompleted:
			ret = VerifyCertificate_CheckInteractionCompleted(msg);
			next_state = SSL_CertificateVerifier::Verify_FinishedSuccessfully;
			break;

		case Verify_AskingUserServerKeyCompleted:
			ret = VerifyCertificate_CheckInteractionCompleted(msg);
			next_state = SSL_CertificateVerifier::Verify_FinishedSuccessfully;

			break;

		case SSL_CertificateVerifier::Verify_Aborted:
			ret = SSL_CertificateVerifier::Verify_Started;
			break;

		default:
			ret = SSL_CertificateVerifier::Verify_Failed;
			break;
		}

		if(ret != SSL_CertificateVerifier::Verify_TaskCompleted)
		{
			doing_iteration = FALSE;
			return ret;
		}
	}while(next_state != Verify_FinishedSuccessfully);

	doing_iteration = FALSE;
	return SSL_CertificateVerifier::Verify_Completed;
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate(SSL_Alert *msg)
{
	cert_handler->ResetError();
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	CRL_List *crl_list_enabled = NULL;
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseCRLValidationForSSL))
	{
		crl_list_enabled =  &crl_list;
	}
#endif // LIBSSL_ENABLE_CRL_SUPPORT

	if(!cert_handler->VerifySignatures(certificate_purpose, msg
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
										, crl_list_enabled
#endif // LIBSSL_ENABLE_CRL_SUPPORT
		))
	{
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
		if(msg->GetDescription() == SSL_Unknown_CA_RequestingUpdate)
		{
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
			crl_list.Clear();
#endif // LIBSSL_ENABLE_CRL_SUPPORT
			cert_handler->GetPendingIssuerId(pending_issuer_id);
		}
#endif // LIBSSL_AUTO_UPDATE_ROOTS
		return SSL_CertificateVerifier::Verify_Failed;
	}

	cert_handler->GetValidatedCertificateChain(Validated_Certificate);

	cert_handler_validated.reset(g_ssl_api->CreateCertificateHandler());
	if(cert_handler_validated.get() == NULL)
	{
		if(msg != NULL)
			msg->Set(SSL_Internal, SSL_Allocation_Failure);
		return SSL_CertificateVerifier::Verify_Failed;
	}
	cert_handler_validated->LoadCertificate(Validated_Certificate);
	if(cert_handler_validated->Error(msg))
		return SSL_CertificateVerifier::Verify_Failed;

	val_certificate_count = cert_handler_validated->CertificateCount();

	if(!g_revoked_certificates.Empty())
	{
		for(uint24 i = 1; i< val_certificate_count; i++)
		{
			if (g_revoked_certificates.CheckForRevokedCert(i, cert_handler_validated.get(),  Validated_Certificate))
			{
				if(msg != NULL)
					msg->Set(SSL_Fatal, SSL_Certificate_Revoked);
				return SSL_CertificateVerifier::Verify_Failed;
			}
		}
	}

	return SSL_CertificateVerifier::Verify_TaskCompleted;
}


#endif
