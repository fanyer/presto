/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
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
#include "modules/libssl/method_disable.h"
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
#include "modules/libssl/data/root_auto_retrieve.h"
#endif

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_CheckCert(SSL_Alert *msg)
{
	g_securityManager->Init(SSL_LOAD_UNTRUSTED_STORE);

	// Check site certificate, to see if it is known to have been revoked; other certs checked after validated certs have been retrieved.
	if (g_revoked_certificates.CheckForRevokedCert(0, cert_handler,  Certificate))
	{
		if(msg != NULL)
			msg->Set(SSL_Fatal, SSL_Certificate_Revoked);
		return SSL_CertificateVerifier::Verify_Failed;
	}

	// Check if already untrusted
	SSL_CertificateItem *item = g_securityManager->FindUnTrustedCert(
		Certificate[0]);	

	if(item)
	{
		if (msg)
			msg->Set(SSL_Fatal, SSL_Access_Denied);
		return SSL_CertificateVerifier::Verify_Failed;
	}

	// Check if already accepted
	SSL_AcceptCert_Item *accept_item =
        (server_info != NULL ? server_info->LookForAcceptedCertificate(Certificate[0], accept_search_mode) : NULL);

	if(accept_item)
	{
		Matched_name.Set(accept_item->AcceptedAs);
		unsigned long i, count = accept_item->Certificate_names.Count();
		if(OpStatus::IsSuccess(CertificateNames.Resize(count)))
		{
			for(i = 0; i < count; i++)
				CertificateNames[i].Set(accept_item->Certificate_names[i]);
		}
		certificate_status = accept_item->certificate_status;

		UserConfirmed = accept_item->confirm_mode;
		security_rating = SECURITY_STATE_LOW;
		low_security_reason |= SECURITY_LOW_REASON_CERTIFICATE_WARNING;

		OpAutoPtr<SSL_PublicKeyCipher> current_key;

		for(i = 0; i < cert_handler->CertificateCount(); i++)
		{
			current_key.reset(cert_handler->GetCipher(i));
			g_ssl_api->DetermineSecurityStrength(current_key.get(), lowkeys, security_rating, low_security_reason);
		}

		return SSL_CertificateVerifier::Verify_Completed;
	}

	return SSL_CertificateVerifier::Verify_TaskCompleted;
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_CheckUntrusted(SSL_Alert *msg)
{
	uint24 i;

	for(i=0; i < val_certificate_count; i++)
	{
		SSL_CertificateItem *item = g_securityManager->FindUnTrustedCert(
			Validated_Certificate[i]);

		if(item)
		{
			if (msg)
				msg->Set(SSL_Fatal, SSL_Access_Denied);
			return SSL_CertificateVerifier::Verify_Failed;
		}
	}

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	// Check for item in conditional 
	for(i=0; i < val_certificate_count; i++) // site cert already checked
	{
		SSL_varvector24 id;
		cert_handler_validated->GetSubjectID(i,id, TRUE);

		BOOL in_list =FALSE;

		if(g_securityManager->GetCanFetchUntrustedID(id, in_list))
		{
			pending_issuer_id = id;
			msg->Set(SSL_Warning, SSL_Untrusted_Cert_RequestingUpdate);
			return SSL_CertificateVerifier::Verify_Failed;
		}

		if(in_list) // if the id is in the list, but we cannot fetch it, assume it is blocked.
		{
			msg->Set(SSL_Fatal, SSL_Access_Denied);
			msg->SetReason(UNI_L("Certificate ID is in list of untrusted certificates")); // Hardcoded string, fallback just in case.
			return SSL_CertificateVerifier::Verify_Failed;
		}
	}
#endif

	return SSL_CertificateVerifier::Verify_TaskCompleted;
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_CheckKeySize(SSL_Alert *msg)
{
	uint24 i;

#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	disable_EV = !check_extended_validation;
#endif

	do{
		uint32 keysize, min_keysize =0;
		uint24 min_keyid = val_certificate_count;
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
		uint32 last_keysize=0;
#endif
		OpAutoPtr<SSL_PublicKeyCipher> current_key;

		for(i = 0; i < val_certificate_count; i++)
		{
			current_key.reset(cert_handler_validated->GetCipher(i));
			keysize = g_ssl_api->DetermineSecurityStrength(current_key.get(), lowkeys, security_rating, low_security_reason);

#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
			if(security_rating < SECURITY_STATE_FULL)
				disable_EV = TRUE;
			last_keysize= keysize;
#endif
			if(min_keyid > i || keysize < min_keysize)
			{
				min_keyid = i;
				min_keysize = keysize;
			}
		}

#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
		if(security_rating >= SECURITY_STATE_FULL && min_keysize < 2048 )
		{
			disable_EV = TRUE;
		}
#endif
	}while(0);

	return SSL_CertificateVerifier::Verify_TaskCompleted;
}


SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_CheckMissingCerts(SSL_Alert *msg)
{
	uint24 i;
	BOOL missing_cert = FALSE;
	for(i=0;i<warncount;i++)
	{
		if((warnstatus[i].status & (SSL_CERT_UNKNOWN_ROOT | SSL_CERT_NO_ISSUER)) != 0)
		{
			missing_cert =TRUE;
			break;
		}
	}
	if(i>=warncount)
	{
		i = val_certificate_count-1;
		if((warnstatus[i].status & SSL_FROM_STORE) == 0)
		{
			missing_cert =TRUE;
			warnstatus[i].status |= (cert_handler_validated->SelfSigned(i) ? SSL_CERT_UNKNOWN_ROOT : SSL_CERT_NO_ISSUER);
		}
	}

	if(missing_cert)
	{
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
		warnstatus[0].extended_validation = FALSE;
#endif
		if(!cert_handler_validated->SelfSigned(val_certificate_count-1) && !block_ica_requests)
		{
			// Can we retrieve the intermediates?

			OpString_list ica_urls;

			if(OpStatus::IsSuccess(cert_handler_validated->GetIntermediateCA_Requests(val_certificate_count-1, ica_urls)) &&
				ica_urls.Count() > 0)
			{
				for(i = 0; i < ica_urls.Count(); i++)
				{
					URL ica_url_cand = g_url_api->GetURL(ica_urls[i], g_revocation_context);

					if(!ica_url_cand.IsEmpty() &&
						ica_url_cand.GetAttribute(URL::KHostName).FindFirstOf('.') != KNotFound
						&& g_url_api->LoadAndDisplayPermitted(ica_url_cand) )
					{
						SSL_DownloadedIntermediateCert *item = (SSL_DownloadedIntermediateCert *) intermediate_list.First();
						OpString8 cand1;
						ica_url_cand.GetAttribute(URL::KName_Escaped, cand1);
						while(item)
						{
							// May miss some special cases, by not using the but that is OK
							OpString8 temp_name;
							OpStatus::Ignore(item->download_url.GetAttribute(URL::KName_Escaped, temp_name));
							if(cand1.Compare(temp_name)==0)
							{
								block_ica_requests = TRUE;
								break;
							}
							item = (SSL_DownloadedIntermediateCert *) item->Suc();
						}
						if(!item) // Hasn't tried this URL before
						{
							pending_url = OP_NEW(URL, (ica_url_cand));
							if(pending_url)
							{
								url_loading_mode = Loading_Intermediate;
								pending_url_ref.SetURL(*pending_url);
								msg->Set(SSL_Warning, SSL_Unknown_CA_WithAIA_URL);
								return SSL_CertificateVerifier::Verify_Failed;
							}
						}
					}
				}
			}
		}

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
		if(!block_retrieve_requests)
		{
			// Check with Opera repository if it have a copy

			cert_handler_validated->GetIssuerID(val_certificate_count-1, pending_issuer_id);

			if(g_securityManager->GetCanFetchIssuerID(pending_issuer_id))
			{
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
				crl_list.Clear();
#endif
				msg->Set(SSL_Warning, SSL_Unknown_CA_RequestingUpdate);
				return SSL_CertificateVerifier::Verify_Failed;
			}
		}
#endif
	}

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	if(!block_retrieve_requests)
	{
		// Check with Opera repository if it have a copy
		i = val_certificate_count-1;
		do{
			if((warnstatus[i].status & SSL_FROM_STORE) != 0)
				break;

			if(!ignore_repository && g_ssl_auto_updaters->Active() && 
				// Don't wait for repository if we are connecting to the repository server.
				servername !=  NULL && servername->GetName().Compare(AUTOUPDATE_SERVER) != 0)
			{
				msg->Set(SSL_Warning, SSL_WaitForRepository);
				return SSL_CertificateVerifier::Verify_Failed;
			}

			cert_handler_validated->GetIssuerID(i-1, pending_issuer_id); // Want the issuer information in the preceeding certificate

			if(g_securityManager->GetCanFetchIssuerID(pending_issuer_id))
			{
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
				crl_list.Clear();
#endif
				msg->Set(SSL_Warning, SSL_Unknown_CA_RequestingUpdate);
				return SSL_CertificateVerifier::Verify_Failed;
			}
		}while(0);
	}
#endif

	return SSL_CertificateVerifier::Verify_TaskCompleted;
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_CheckWarnings(SSL_Alert *msg)
{
	uint24 i;

	for(i=0;i<warncount;i++)
	{
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
		if((warnstatus[i].status & (~SSL_CERT_INFO_FLAGS)) != 0)
			warnstatus[0].extended_validation = FALSE;
#endif
		if((warnstatus[i].status & SSL_CERT_WARN_MASK) == SSL_CERT_DENY)
		{
			if (msg)
				msg->Set(SSL_Fatal, SSL_Access_Denied);
			return SSL_CertificateVerifier::Verify_Failed;
		}
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
		if((warnstatus[i].status & (SSL_NO_CRL | SSL_INVALID_CRL)) != 0 && 
			(low_security_reason & SECURITY_LOW_REASON_UNABLE_TO_CRL_VALIDATE) == 0
			)
		{
			low_security_reason |= SECURITY_LOW_REASON_UNABLE_TO_CRL_VALIDATE;
			if(security_rating > SECURITY_STATE_HALF)
			{
				security_rating = SECURITY_STATE_HALF;
			}
		}
#endif
#if !defined LIBOPEAY_DISABLE_MD5_PARTIAL
		if((warnstatus[i].status & SSL_USED_MD5) != 0)
		{
			security_rating = SECURITY_STATE_LOW;
			low_security_reason |= SECURITY_LOW_REASON_WEAK_KEY;
		}
#endif
#if !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
		if((warnstatus[i].status & SSL_USED_SHA1) != 0)
		{
			security_rating = SECURITY_STATE_LOW;
			low_security_reason |= SECURITY_LOW_REASON_WEAK_KEY;
		}
#endif
	}

	warn_mask  = 0;
	if(cipherdescription != NULL)
	{
		if(cipherdescription->KeySize == 0)
			warn_mask |= SSL_CERT_AUTHENTICATION_ONLY;
		else if(cipherdescription->kea_alg == SSL_Anonymous_Diffie_Hellman_KEA)
			warn_mask |= SSL_CERT_ANONYMOUS_CONNECTION;
		else if(security_rating < SECURITY_STATE_HALF)
		{
			warn_mask |= ((cipherdescription->security_rating< (SSL_SecurityRating) SECURITY_STATE_FULL && g_securityManager->some_secure_ciphers_are_disabled) ?
SSL_CERT_DIS_CIPH_LOW_ENCRYPTION : SSL_CERT_LOW_ENCRYPTION);
			warn_mask |= ((low_security_reason & SECURITY_LOW_REASON_MASK) << SSL_CERT_LOW_SEC_REASON_SHIFT);
		}
	}

	warnstatus[0].status |= VerifyCheckExtraWarnings(low_security_reason);

	for(i=0;i<warncount;i++)
	{
		if((warnstatus[i].status & SSL_CERT_UNUSED) != 0)
			continue;
		warn_mask |= warnstatus[i].status;
	}

#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	extended_validation = (security_rating >= SECURITY_STATE_FULL && warnstatus[0].extended_validation);
#endif

	return SSL_CertificateVerifier::Verify_TaskCompleted;
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_CheckName(SSL_Alert *msg)
{
	BOOL name_check_failed = FALSE;
	if(check_name)
	{
		SSL_ServerName_List names;
		SSL_DistinguishedName name;

		name.ForwardTo(this);

		cert_handler->GetServerName(0,names);

		OpStatus::Ignore(certificate_name_list.Set(names.Namelist)); // Ignore, not critical

		name_check_failed = TRUE;
		if(names.MatchName(servername))
		{
			Matched_name.Set(names.Matched_name);
			name_check_failed = FALSE;
		}
	}

	if(name_check_failed)
	{
		warn_mask |= SSL_CERT_NAME_MISMATCH;
	}

	return SSL_CertificateVerifier::Verify_TaskCompleted;
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_CheckTrusted(SSL_Alert *msg)
{
	if(server_info == NULL)
		return SSL_CertificateVerifier::Verify_TaskCompleted;

	g_securityManager->Init(SSL_LOAD_TRUSTED_STORE);
	SSL_CertificateItem *item = g_securityManager->FindTrustedServer(
		Certificate[0],	servername,	port,
		(accept_search_mode != APPLET_SESSION_CONFIRMED ? CertAccept_Site : CertAccept_Applet));

	if(item)
	{
		if((item->trusted_until && item->trusted_until > g_timecache->CurrentTime()) ||
			// either accepted for a period
			(!item->trusted_until && ((warn_mask & SSL_CERT_EXPIRED) == 0 && (warn_mask & SSL_CERT_NOT_YET_VALID) == 0))
			// or until it has expired
			)
		{
			SSL_AcceptCert_Item *accept_item = OP_NEW(SSL_AcceptCert_Item, ());

			if(accept_item)
			{
				accept_item->certificate = Certificate[0];
				accept_item->AcceptedAs.Set(Matched_name);
				accept_item->confirm_mode = PERMANENTLY_CONFIRMED;
				unsigned long i, count = CertificateNames.Count();
				if(OpStatus::IsSuccess(accept_item->Certificate_names.Resize(count)))
				{
					for(i = 0; i < count; i++)
						accept_item->Certificate_names[i].Set(CertificateNames[i]);
				}
				accept_item->certificate_status = warn_mask & (~SSL_CERT_INFO_FLAGS);

				server_info->AddAcceptedCertificate(accept_item);
			}

			certificate_status = warn_mask & (~SSL_CERT_INFO_FLAGS);
			UserConfirmed = PERMANENTLY_CONFIRMED;
			security_rating = SECURITY_STATE_LOW;
			low_security_reason |= SECURITY_LOW_REASON_CERTIFICATE_WARNING;

			return SSL_CertificateVerifier::Verify_TaskCompleted;
		}
		// otherwise we ask if necessary
	}
	return SSL_CertificateVerifier::Verify_TaskCompleted;
}
#endif
