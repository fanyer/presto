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
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/keyex/certverify.h"
#include "modules/libssl/certs/certhandler.h"
#include "modules/libssl/certs/certinst_base.h"
#include "modules/url/url2.h"
#include "modules/cache/url_stream.h"
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
#include "modules/libssl/data/root_auto_retrieve.h"
#include "modules/libssl/data/untrusted_auto_retrieve.h"
#endif

#ifdef SSL_ALLOW_REDIRECT_OF_CRLS
#define CRL_REDIRECT_ALLOWED	URL::KFollowRedirect
#else
#define CRL_REDIRECT_ALLOWED	URL::KNoRedirect
#endif

BOOL load_PEM_certificates2(SSL_varvector32 &data_source, SSL_varvector32 &pem_content);

void SSL_CertificateVerifier::ProcessIntermediateCACert()
{
	OP_MEMORY_VAR BOOL success = FALSE;

	do{
		if(pending_url == NULL || pending_url->IsEmpty())
			break;

		OpFileLength len = 0;
		pending_url->GetAttribute(URL::KContentLoaded, &len, URL::KFollowRedirect);

		if(pending_url->GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) != URL_LOADED ||
			pending_url->GetAttribute(URL::KHTTP_Response_Code,URL::KFollowRedirect) != HTTP_OK ||
			len == 0)
		{
			g_securityManager->SetHaveCheckedURL(pending_url_ref, 24*60*60);
			break;
		}

		SSL_ASN1Cert retrieved_cert;
		{
			URL_DataStream source_stream(*pending_url, TRUE);
			SSL_varvector32 temp_read_cert, temp_read_decoded;

			TRAPD(op_err, temp_read_cert.AddContentL(&source_stream));
			if(OpStatus::IsError(op_err) || temp_read_cert.Error())
				break;

			// Detect PEM source
			{
				uint32 pos = 0;
				uint32 dlen = temp_read_cert.GetLength();
				byte *data = temp_read_cert.GetDirect();

				while(pos +10 <= dlen)
				{
					if(!op_isspace(data[pos]))
					{
						if(op_strnicmp((char *) data+pos, "-----BEGIN",10) == 0)
						{
							// this is a PEM file
							temp_read_cert.Append("\0", 1);

							if(!load_PEM_certificates2(temp_read_cert, temp_read_decoded))
								break;

							retrieved_cert = temp_read_decoded;
						}
						else
							retrieved_cert = temp_read_cert;

						break;
					}
					pos++;
				}
			}
		}

		if(retrieved_cert.GetLength() == 0)
			break;

		OpAutoPtr<SSL_CertificateHandler> cert(g_ssl_api->CreateCertificateHandler());
		if(!cert.get())
			break;

		cert->LoadCertificate(retrieved_cert);
		if(cert->Error() || cert->CertificateCount() != 1 || cert->SelfSigned(0))
			break; // Not more than one certificate, and not selfsigned.

		cert.reset();

		OpAutoPtr<SSL_DownloadedIntermediateCert> cert_item(OP_NEW(SSL_DownloadedIntermediateCert, ()));
		if(!cert_item.get())
			break;

		cert_item->download_url = *pending_url;
		cert_item->certificate = retrieved_cert;

		if(cert_item->certificate.Error())
			break;

		cert_item->Into(&intermediate_list);
		cert_item.release();

		success = TRUE;
	}while(0);

	if(!success)
		CancelPendingLoad();

	pending_url_ref.UnsetURL();
	OP_DELETE(pending_url);
	pending_url = NULL;

	return;
}

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
OP_STATUS SSL_CertificateVerifier::StartRootRetrieval(MH_PARAM_1 &fin_id)
{
	OpAutoPtr<SSL_Auto_Root_Retriever> temp(OP_NEW(SSL_Auto_Root_Retriever, ()));
	block_retrieve_requests = TRUE;
	fin_id = 0;

	if(!temp.get())
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(temp->Construct(pending_issuer_id, MSG_DO_AUTO_ROOT_REQUEST));

	RETURN_IF_ERROR(auto_fetcher.AddUpdater(temp.release()));

	g_securityManager->SetHaveCheckedIssuerID(pending_issuer_id);
	fin_id = (UINTPTR) this;

	pending_issuer_id.Resize(0);
	block_retrieve_requests = FALSE;
	return OpStatus::OK;
}

OP_STATUS SSL_CertificateVerifier::StartUntrustedRetrieval(MH_PARAM_1 &fin_id)
{
	OpAutoPtr<SSL_Auto_Untrusted_Retriever> temp(new SSL_Auto_Untrusted_Retriever());
	block_retrieve_requests = TRUE;
	fin_id = 0;

	if(!temp.get())
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(temp->Construct(pending_issuer_id, MSG_DO_AUTO_UNTRUSTED_REQUEST));

	// NOT added to autoupdaters; as keyexhange has ownership and control
	RETURN_IF_ERROR(temp->StartLoading());

	RETURN_IF_ERROR(auto_fetcher.AddUpdater(temp.release()));

	g_securityManager->SetHaveCheckedUntrustedID(pending_issuer_id);
	pending_issuer_id.Resize(0);

	fin_id = (UINTPTR) this;
	block_retrieve_requests = FALSE;
	return OpStatus::OK;
}
#endif // LIBSSL_AUTO_UPDATE_ROOTS

URL SSL_CertificateVerifier::GetPending_URL(MH_PARAM_1 id)
{
	if(pending_url && pending_url->Id() == URL_ID(id))
		return *pending_url;

#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	URLLink *item = (URLLink *) loading_crls.First();
	while(item)
	{
		if(item->GetURL().Id() == URL_ID(id))
			return item->GetURL();
		item = (URLLink *) item->Suc();
	}
#endif // LIBSSL_ENABLE_CRL_SUPPORT

	return URL();
}

BOOL SSL_CertificateVerifier::StartLoadingURL(MessageObject *target, URL &url, BOOL allow_redirects)
{
	if(servername != NULL &&
       url.GetServerName() == (ServerName*) servername &&
       url.GetAttribute(URL::KResolvedPort) == port)
		return FALSE; // Don't retrieve URLs on same server as we are on.

	if(!url.IsEmpty())
	{
		OpRequest *request;
		OpURL requestURL(url);

		if (OpRequest::Make(request, batch_request, requestURL, g_revocation_context) == OpStatus::OK)
		{
			if(!allow_redirects)
				request->SetLimitedRequestProcessing(TRUE);
			request->SetCookiesProcessingDisabled(TRUE);
			request->SetExternallyManagedConnection(TRUE);

			request->SetParallelConnectionsDisabled();
			if(servername != (ServerName *) NULL)
				request->SetNetTypeLimit(servername->GetNetType());
			request->SetMaxRequestTime(60);
			request->SetMaxResponseIdleTime(5);
			request->UseGenericAuthenticationHandling();
			request->GetLoadPolicy().SetReloadPolicy(URL_Load_Normal);

		if (OpStatus::IsSuccess(batch_request->SendRequest(request)) && (g_main_message_handler->HasCallBack(target, MSG_SSL_AUTOFETCH2_FINISHED, (UINTPTR) this) ||
				OpStatus::IsSuccess(g_main_message_handler->SetCallBack(target, MSG_SSL_AUTOFETCH2_FINISHED, (UINTPTR) this))))
			return TRUE;

		}

	}
	OP_ASSERT(url.GetAttribute(URL::KLoadStatus) != URL_LOADING);
	return FALSE;
}

#ifdef LIBSSL_ENABLE_CRL_SUPPORT
BOOL SSL_CertificateVerifier::StartLoadingCrls(MessageObject *target)
{
	if(!loaded_crls)
	{
		url_loading_mode = Loading_CRL_Or_OCSP;
		CRL_item *item = crl_list.First();
		while(item)
		{
			if(!item->retrieved)
			{
				time_t loaded =0;
				item->crl_url.GetAttribute(URL::KVLocalTimeLoaded,&loaded, CRL_REDIRECT_ALLOWED);
				if(item->crl_url.GetAttribute(URL::KLoadStatus, CRL_REDIRECT_ALLOWED) == URL_LOADED &&
					loaded > g_timecache->CurrentTime() - 60)
				{
					// If this was loaded very recently, it is probably a race condition, ignore it
					//item->loaded = TRUE;
					item->retrieved = TRUE;
				}
			}
			if(!item->retrieved)
			{
				URLLink *new_item = OP_NEW(URLLink, (item->crl_url));
				if(!new_item || !StartLoadingURL(target, item->crl_url
#ifdef SSL_ALLOW_REDIRECT_OF_CRLS
						, TRUE
#endif
					))
				{
					item->retrieved =TRUE;
					OP_DELETE(new_item);
					//CancelPendingLoad();
				}
				else
				{
					item->retrieved =TRUE;
					new_item->Into(&loading_crls);
				}
			}
			item = item->Suc();
		}
		loaded_crls = TRUE;
		return loading_crls.Empty() ? FALSE : TRUE;
	}
	return FALSE;
}
#endif // LIBSSL_ENABLE_CRL_SUPPORT

BOOL SSL_CertificateVerifier::StartPendingURL(MessageObject *target)
{
	if(pending_url)
	{
		if(!StartLoadingURL(target, *pending_url))
		{
			CancelPendingLoad();
			return FALSE;
		}
		return TRUE;
	}

	return FALSE;
}

void SSL_CertificateVerifier::OnRequestRedirected(OpRequest *req, OpResponse *res, OpURL from, OpURL to)
{
	OpString hostname;
	if(OpStatus::IsError(to.GetHostName(hostname)) || (servername != NULL &&
		hostname == ((ServerName*) servername)->UniName() &&
		to.GetResolvedPort() == port))
	{
		g_main_message_handler->PostMessage(MSG_SSL_CERT_VERIFICATION_CANCEL_PENDING_LOAD, (UINTPTR)this, 0);
		// If looping, abort
		batch_request->ClearRequests();
	}
}

void SSL_CertificateVerifier::OnRequestFailed(OpRequest *req, OpResponse *res, Str::LocaleString error)
{
	if (url_loading_mode == Loading_CRL_Or_OCSP)
	{
		if(security_rating > SECURITY_STATE_HALF)
			security_rating = SECURITY_STATE_HALF;

		if (pending_url)
		{
			OpURL op_pending_url(*pending_url);
			if (req->GetURL() == op_pending_url)
				low_security_reason |= SECURITY_LOW_REASON_UNABLE_TO_OCSP_VALIDATE;
			else
				low_security_reason |= SECURITY_LOW_REASON_UNABLE_TO_CRL_VALIDATE;
		}
		else
			low_security_reason |= SECURITY_LOW_REASON_UNABLE_TO_CRL_VALIDATE;

	}
	g_main_message_handler->PostMessage(MSG_SSL_CERT_VERIFICATION_CANCEL_PENDING_LOAD, (UINTPTR)this, 0);
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::ProcessFinishedLoad(MessageObject *target)
{
	SSL_CertificateVerifier::VerifyStatus ret = SSL_CertificateVerifier::Verify_Started;
	SSL_KeyExLoadingMode org_loading_mode = url_loading_mode;

	switch(org_loading_mode)
	{
	case Loading_Intermediate:
		ProcessIntermediateCACert();
		ret = SSL_CertificateVerifier::Verify_TaskCompleted;
		break;
	case Loading_CRL_Or_OCSP:
		{
			if (pending_url && (URLStatus) pending_url->GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) != URL_LOADING)
			{
				if((URLStatus) pending_url->GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) == URL_LOADED)
					ValidateOCSP(FALSE);
				else
				{
					// OCSP Failure; setting low security flags
					low_security_reason |= SECURITY_LOW_REASON_UNABLE_TO_OCSP_VALIDATE;
					if(security_rating > SECURITY_STATE_HALF)
						security_rating = SECURITY_STATE_HALF;
				}


				pending_url_ref.UnsetURL();
				OP_DELETE(pending_url);
				pending_url = NULL;
				certificate_loading_ocsp = FALSE;
			}
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
			URLLink *next_item = (URLLink *) loading_crls.First();
			while(next_item)
			{
				URLLink *item = next_item;
				next_item = (URLLink *) next_item->Suc();
				if((URLStatus) item->GetURL().GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) != URL_LOADING)
				{
					CRL_item *crl_item = crl_list.First();
					while(crl_item)
					{
						if(crl_item->crl_url == item->GetURL())
						{
							if(OpStatus::IsError(crl_item->DecodeData()))
							{
								low_security_reason |= SECURITY_LOW_REASON_UNABLE_TO_CRL_VALIDATE;
								if(security_rating > SECURITY_STATE_HALF)
								{
									security_rating = SECURITY_STATE_HALF;
								}

								crl_item->crl_url.Unload();

								while(crl_item->PromoteURL())
								{
									URLLink *new_item = OP_NEW(URLLink, (crl_item->crl_url));
									if(!new_item || !StartLoadingURL(target, crl_item->crl_url
#ifdef SSL_ALLOW_REDIRECT_OF_CRLS
											, TRUE
#endif
										))
									{
										OP_DELETE(new_item);
									}
									else
									{
										crl_item->retrieved =TRUE;
										new_item->Into(&loading_crls);
									}
									break;
								}
							}
							break;
						}

						crl_item = crl_item->Suc();
					}

					item->Out();
					OP_DELETE(item);
					//break;
				}
			}
#endif // LIBSSL_ENABLE_CRL_SUPPORT
		}

	}

	pending_url_ref.UnsetURL();
	OP_DELETE(pending_url);
	pending_url = NULL;
	url_loading_mode = Loading_None;

	if(ret != SSL_CertificateVerifier::Verify_Failed)
	{
		switch(org_loading_mode)
		{
		case Loading_CRL_Or_OCSP:
		case Loading_Intermediate:
			ret = SSL_CertificateVerifier::Verify_TaskCompleted;
			break;
		}
	}

	return ret;
}

void SSL_CertificateVerifier::CancelPendingLoad()
{
	if (batch_request)
	{
		batch_request->ClearRequests();
	}
	switch(url_loading_mode)
	{
	case Loading_Intermediate:
		block_ica_requests = TRUE;
		if(pending_url && !pending_url->IsEmpty())
			g_securityManager->SetHaveCheckedURL(pending_url_ref, 24*60*60);
		break;

	case Loading_CRL_Or_OCSP:
		{
			if(pending_url && !pending_url->IsEmpty())
				pending_url->Unload();

#ifdef LIBSSL_ENABLE_CRL_SUPPORT
			URLLink *item = (URLLink *) loading_crls.First();
			while(item)
			{
				URL url = item->GetURL();
				if(url.IsValid())
					url.Unload();
				item = (URLLink *) item->Suc();
			}
#endif // LIBSSL_ENABLE_CRL_SUPPORT
		}
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
		loading_crls.Clear();
#endif // LIBSSL_ENABLE_CRL_SUPPORT
		certificate_loading_ocsp = FALSE;
		break;

	}

	if(pending_url && !pending_url->IsEmpty())
	{
		pending_url_ref.UnsetURL();
	}
	OP_DELETE(pending_url);
	pending_url = NULL;
}

void SSL_CertificateVerifier::OnBatchResponsesFinished()
{
	if(verify_state == SSL_CertificateVerifier::Verify_Aborted)
		return;

	SSL_CertificateVerifier::VerifyStatus ret = ProcessFinishedLoad(this);

	switch(ret)
	{
	case SSL_CertificateVerifier::Verify_Failed:
		{
			SSL_Alert msg;
			Error(&msg);
			VerifyFailed(msg);
			break;
		}

	case SSL_CertificateVerifier::Verify_Started:
		break;

	case SSL_CertificateVerifier::Verify_TaskCompleted:
		switch(verify_state)
		{
		case SSL_CertificateVerifier::Verify_LoadingAIACert:
			verify_state = SSL_CertificateVerifier::Verify_LoadingAIACertCompleted;
			break;
		case SSL_CertificateVerifier::Verify_LoadingCRLOrOCSP:
			verify_state = SSL_CertificateVerifier::Verify_LoadingCRLCompleted;
			break;
		case SSL_CertificateVerifier::Verify_Aborted:
			return;
		default:
			OP_ASSERT(!"Invalid verification state");
			{
				SSL_Alert msg(SSL_Internal, SSL_InternalError);
				VerifyFailed(msg);
			}
			break;
		}
		ProgressVerifySiteCertificate();
	}
	//mainMessageHandler->PostMessage(MSG_SSL_AUTOFETCH2_FINISHED, (UINTPTR) this, 0);
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_HandleDownloadedIntermediates(SSL_Alert *msg)
{
#ifdef USE_SSL_CERTINSTALLER
	if(msg && !msg->Error() && cert_handler_validated->SelfSigned(val_certificate_count-1))
	{
		uint24 count = Validated_Certificate.Count();
		OP_MEMORY_VAR uint24 i;
		SSL_Certificate_Installer_flags install_flags(SSL_IntermediateCAStore, FALSE,FALSE);
		SSL_Options * OP_MEMORY_VAR optionsManager = NULL;
		OP_STATUS op_err = OpStatus::OK;

		for(i= 0; i < count && OpStatus::IsSuccess(op_err); i++)
		{
			SSL_DownloadedIntermediateCert *item = (SSL_DownloadedIntermediateCert *) intermediate_list.First();
			while(item)
			{
				if(item->certificate == Validated_Certificate[i])
				{
					SSL_Certificate_Installer_Base *installer= NULL;

					if(!optionsManager)
					{
						optionsManager = g_ssl_api->CreateSecurityManager(TRUE);
						if(!optionsManager)
							continue;
						optionsManager->Init(SSL_LOAD_CA_STORE | SSL_LOAD_INTERMEDIATE_CA_STORE);
					}

					TRAPD(op_err, installer = g_ssl_api->CreateCertificateInstallerL(item->certificate, install_flags, NULL, optionsManager));
					if(OpStatus::IsError(op_err) || !installer)
						break;

					op_err =installer->StartInstallation();

					if(op_err != InstallerStatus::INSTALL_FINISHED)
						op_err = OpStatus::ERR;

					OP_DELETE(installer);

					break;
				}
				item = (SSL_DownloadedIntermediateCert *) item->Suc();
			}
		}
		if(optionsManager)
		{
			g_ssl_api->CommitOptionsManager(optionsManager);
			if(optionsManager->dec_reference() <= 0)
				OP_DELETE(optionsManager);
		}
	}
#endif // USE_SSL_CERTINSTALLER

	return SSL_CertificateVerifier::Verify_TaskCompleted;
}

void SSL_CertificateVerifier::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	case MSG_SSL_FINISHED_AUTOUPDATE_BATCH:
		g_main_message_handler->UnsetCallBack(this, MSG_SSL_FINISHED_AUTOUPDATE_BATCH);
		if(verify_state == SSL_CertificateVerifier::Verify_Aborted)
			return;

		verify_state = SSL_CertificateVerifier::Verify_WaitForRepositoryCompleted;
		ProgressVerifySiteCertificate();
		break;
#endif
	case MSG_SSL_AUTOFETCH2_FINISHED:
	case MSG_SSL_AUTOFETCH_FINISHED:
		g_main_message_handler->UnsetCallBack(this, msg);
		if(verify_state == SSL_CertificateVerifier::Verify_Aborted)
			return;

		switch(verify_state)
		{
		case SSL_CertificateVerifier::Verify_LoadingRepositoryCert:
			verify_state = SSL_CertificateVerifier::Verify_LoadingRepositoryCertCompleted;
			break;
		case SSL_CertificateVerifier::Verify_LoadingUntrustedCert:
			verify_state = SSL_CertificateVerifier::Verify_LoadingUntrustedCertCompleted;
		}
		ProgressVerifySiteCertificate();
		break;
	case MSG_SSL_WARNING_HANDLED:
		g_main_message_handler->UnsetCallBack(this, MSG_SSL_WARNING_HANDLED);
		if(verify_state == SSL_CertificateVerifier::Verify_Aborted)
			return;
		OP_ASSERT(verify_state == SSL_CertificateVerifier::Verify_AskingUser);

		if(verify_state == SSL_CertificateVerifier::Verify_AskingUser)
			verify_state = SSL_CertificateVerifier::Verify_AskingUserCompleted;
		else // verify_state == SSL_CertificateVerifier::Verify_AskingUserServerKeys
			verify_state = SSL_CertificateVerifier::Verify_AskingUserServerKeyCompleted;

		ProgressVerifySiteCertificate();
		break;

#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
	case MSG_SSL_EXTERNAL_REPOSITORY_LOADED:
		g_main_message_handler->UnsetCallBack(this, MSG_SSL_EXTERNAL_REPOSITORY_LOADED);
		if(verify_state == SSL_CertificateVerifier::Verify_Aborted)
			return;

		verify_state = SSL_CertificateVerifier::Verify_LoadingExternalRepositoryCertCompleted;
		ProgressVerifySiteCertificate();
		break;
#endif
	case MSG_SSL_CERT_VERIFICATION_CANCEL_PENDING_LOAD:
		CancelPendingLoad();
		break;

	default:
		break;//URL_DocumentLoader::HandleCallback(msg, par1, par2);
	}
}

#endif // _NATIVE_SSL_SUPPORT_
