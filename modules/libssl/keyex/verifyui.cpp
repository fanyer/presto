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
#include "modules/libssl/keyex/certverify.h"


SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_CheckInteractionNeeded(SSL_Alert *msg)
{
	uint24 i;

	SSL_Alert alert;
	int certificate_dialog = dialog_type;

	if(UserConfirmed == PERMANENTLY_CONFIRMED)
		return SSL_CertificateVerifier::Verify_TaskCompleted;

	if(certificate_purpose == SSL_Purpose_Object_Signing)
	{
		if(UserConfirmed == APPLET_SESSION_CONFIRMED)
			return SSL_CertificateVerifier::Verify_TaskCompleted;
	}
	else if(UserConfirmed == SESSION_CONFIRMED)
		return SSL_CertificateVerifier::Verify_TaskCompleted;

	if(certificate_dialog)
	{
		// Dialog already decided
	}
	else if ((warn_mask & SSL_CERT_ANONYMOUS_CONNECTION) != 0)
	{
		certificate_dialog = IDM_SSL_ANONYMOUS_CONNECTION;
		alert.Set(SSL_Fatal, SSL_Insufficient_Security);
	}
	else if(!accept_unknown_ca && ((warn_mask & SSL_CERT_NO_ISSUER) != 0 || (warn_mask & SSL_CERT_UNKNOWN_ROOT) != 0 || (warn_mask & SSL_CERT_INCORRECT_ISSUER) != 0))
	{
		certificate_dialog = IDM_NO_CHAIN_CERTIFICATE;
		alert.Set(SSL_Fatal, SSL_Unknown_CA);
	}
	else if(!accept_unknown_ca && ((warn_mask & SSL_CERT_EXPIRED) != 0 || (warn_mask & SSL_CERT_NOT_YET_VALID) != 0))
	{
		certificate_dialog = IDM_EXPIRED_SERVER_CERTIFICATE;
		alert.Set(SSL_Fatal, SSL_Certificate_Expired);
	}
	else if(warn_mask & SSL_CERT_WARN)
	{
		certificate_dialog = IDM_WARN_CERTIFICATE;
		alert.Set(SSL_Fatal, SSL_Access_Denied);
	}
	else if ((warn_mask & SSL_CERT_AUTHENTICATION_ONLY) != 0)
	{
		certificate_dialog = IDM_SSL_AUTHENTICATED_ONLY;
		alert.Set(SSL_Fatal, SSL_Authentication_Only_Warning);
	}
	else if ((warn_mask & (SSL_CERT_LOW_ENCRYPTION | SSL_CERT_DIS_CIPH_LOW_ENCRYPTION)) != 0)
	{
		certificate_dialog = ((warn_mask & SSL_CERT_DIS_CIPH_LOW_ENCRYPTION) != 0 ? IDM_SSL_DIS_CIPHER_LOW_ENCRYPTION_LEVEL : IDM_SSL_LOW_ENCRYPTION_LEVEL);
		alert.Set(SSL_Fatal, SSL_Insufficient_Security);
	}
	else if(warn_mask & SSL_CERT_NAME_MISMATCH)
	{
		certificate_dialog = IDM_WRONG_CERTIFICATE_NAME;
		alert.Set(SSL_Fatal, SSL_Access_Denied);
	}
	else if ((warn_mask & SSL_NO_RENEG_EXTSUPPORT) != 0)
	{
		certificate_dialog = IDM_SSL_LOW_ENCRYPTION_LEVEL;
		alert.Set(SSL_Fatal, SSL_NoRenegExtSupport);
	}

	certificate_status = warn_mask & (~SSL_CERT_INFO_FLAGS);

	if(certificate_dialog == 0)
		return SSL_CertificateVerifier::Verify_TaskCompleted;

	if (
			certificate_dialog != 0 && (!show_dialog
#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
			// Do not allow user interaction for strict transport security connections.
			// This is to avoid click-through-security.
			|| servername->SupportsStrictTransportSecurity()
#endif // LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	))
	{
		certificate_dialog = 0;
		if(msg)
			*msg = alert;
		return SSL_CertificateVerifier::Verify_Failed;
	}

	certificate_ctx.reset(OP_NEW(SSL_Certificate_DisplayContext, (certificate_dialog)));
	if(certificate_ctx.get() == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
		return SSL_CertificateVerifier::Verify_Failed;
	}

	certificate_ctx->SetCertificateList(Certificate);

	certificate_ctx->SetCertificateChain(cert_handler->Fork(), FALSE);

	certificate_ctx->SetURL(DisplayURL);
	certificate_ctx->SetServerInformation(servername, port);
	if(cipherdescription != (SSL_CipherDescriptions *) NULL)
		certificate_ctx->SetCipherName(cipherdescription->fullname);
	certificate_ctx->SetCipherVersion(used_version);
	certificate_ctx->SetAlertMessage(alert);
	certificate_ctx->SetCompleteMessage(MSG_SSL_WARNING_HANDLED, (UINTPTR) this);
	certificate_ctx->SetRemberAcceptAndRefuse(TRUE);
	certificate_ctx->SetCertificateIsExpired((warn_mask & SSL_CERT_EXPIRED) != 0 || (warn_mask & SSL_CERT_NOT_YET_VALID) != 0);

	if((warn_mask & SSL_CERT_NAME_MISMATCH) != 0)
	{
		OpString actual_name;
		OpString name_list;
		OpStatus::Ignore(actual_name.Set(servername->GetName())); // Ignore, not critical
		OpStatus::Ignore(name_list.Set(certificate_name_list)); // Ignore, not critical

		if(actual_name.Compare(servername->GetUniName()) != 0)
		{
			actual_name.Append(UNI_L("("));
			actual_name.Append(servername->GetUniName());
			actual_name.Append(UNI_L(")"));
		}

		certificate_ctx->AddCertificateComment(Str::S_CERT_MISMATCH_SERVERNAME, actual_name, name_list);
	}

	if((warn_mask & SSL_CERT_LOW_ENCRYPTION) != 0 && certificate_dialog != IDM_SSL_LOW_ENCRYPTION_LEVEL)
	{
		certificate_ctx->AddCertificateComment(Str::S_SSL_LOW_ENCRYPTION_WARNING, NULL, NULL);
	}

	if((warn_mask & SSL_CERT_DIS_CIPH_LOW_ENCRYPTION) != 0 && certificate_dialog != IDM_SSL_DIS_CIPHER_LOW_ENCRYPTION_LEVEL)
	{
		certificate_ctx->AddCertificateComment(Str::S_SSL_LOW_ENCRYPTION_DIS_CIPHER_WARNING, NULL, NULL);
	}

	if((warn_mask & SSL_CERT_ANONYMOUS_CONNECTION) != 0 && certificate_dialog != IDM_SSL_ANONYMOUS_CONNECTION)
	{
		certificate_ctx->AddCertificateComment(Str::S_SSL_ANONYMOUS_WARNING, NULL, NULL);
	}
	if((warn_mask & SSL_CERT_AUTHENTICATION_ONLY) != 0 && certificate_dialog != IDM_SSL_AUTHENTICATED_ONLY)
	{
		certificate_ctx->AddCertificateComment(Str::S_MSG_HTTP_SSL_AUTHENTICATION_ONLY, NULL, NULL);
	}
			if ((warn_mask & SSL_NO_RENEG_EXTSUPPORT) != 0)
			{
				certificate_ctx->AddCertificateComment(Str::S_MSG_NO_RENEG_SUPPORT_WARN, NULL, NULL);
			}

	if((warn_mask & SSL_CERT_LOW_SEC_REASON_MASK) != 0)
	{
		BYTE reasons = (BYTE) ((warn_mask >> SSL_CERT_LOW_SEC_REASON_SHIFT) & SECURITY_LOW_REASON_MASK);

		if((reasons & SECURITY_LOW_REASON_WEAK_METHOD) != 0)
		{
			certificate_ctx->AddCertificateComment(Str::S_WEAK_ENCRYPTION_METHOD, NULL, NULL);
		}
		if((reasons & SECURITY_LOW_REASON_WEAK_KEY) != 0)
		{
			certificate_ctx->AddCertificateComment(Str::S_WEAK_CERTIFICATE_KEY, NULL, NULL);
		}
		if((reasons & SECURITY_LOW_REASON_SLIGHTLY_WEAK_KEY) != 0)
		{
			certificate_ctx->AddCertificateComment(Str::S_SLIGHTLY_WEAK_CERTIFICATE_KEY, NULL, NULL);
		}
		if((reasons & SECURITY_LOW_REASON_WEAK_PROTOCOL) != 0)
		{
			certificate_ctx->AddCertificateComment(Str::S_WEAK_PROTOCOL_VERSION, NULL, NULL);
		}
	}

	if(warnstatus) for(i = 0; i< warncount; i++)
	{
		if((warnstatus[i].status & SSL_CERT_UNUSED) != 0)
			continue;

		if((warnstatus[i].status & SSL_CERT_UNKNOWN_ROOT) != 0)
		{
			certificate_ctx->AddCertificateComment(Str::S_CERT_UNKNOWN_ROOTCA, warnstatus[i].subject_name, warnstatus[i].issuer_name);
		}
		else if((warnstatus[i].status & SSL_CERT_NO_ISSUER) != 0) // can't be both
		{
			certificate_ctx->AddCertificateComment(Str::S_CERT_UNKNOWN_CA, warnstatus[i].subject_name, warnstatus[i].issuer_name);
		}
		if((warnstatus[i].status & SSL_CERT_NOT_YET_VALID) != 0)
		{
			certificate_ctx->AddCertificateComment(Str::S_CERT_NOT_YET_VALID, warnstatus[i].subject_name, warnstatus[i].valid_from_date);
		}
		else if((warnstatus[i].status & SSL_CERT_EXPIRED) != 0)
		{
			certificate_ctx->AddCertificateComment(Str::S_CERTIFICATE_EXPIRED, warnstatus[i].subject_name, warnstatus[i].expiration_date);
		}
		if((warnstatus[i].status & SSL_CERT_WARN_MASK) == SSL_CERT_WARN)
		{
			certificate_ctx->AddCertificateComment(Str::S_CERT_WARN_ABOUT, warnstatus[i].subject_name, NULL );
		}
		if((warnstatus[i].status & SSL_CERT_INCORRECT_ISSUER) != 0)
		{
			certificate_ctx->AddCertificateComment(Str::S_BAD_CA_CERT_FLAGS, warnstatus[i].subject_name, NULL );
		}
	}

	certificate_ctx->SetWindow(window);

	if(OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_SSL_WARNING_HANDLED, (UINTPTR) this)) ||
		!InitSecurityCertBrowsing(NULL, certificate_ctx.get()))
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		certificate_ctx.reset();
		return SSL_CertificateVerifier::Verify_Failed;
	}

	return SSL_CertificateVerifier::Verify_Started;
}


SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_CheckInteractionCompleted(SSL_Alert *msg)
{
	if(!certificate_ctx.get())
		return SSL_CertificateVerifier::Verify_TaskCompleted;

	SSL_CertificateVerifier::VerifyStatus ret;
	BOOL result = (certificate_ctx->GetFinalAction() == SSL_CERT_ACCEPT ? TRUE : FALSE);
	if(result)
	{
		if(server_info != NULL)
		{
			SSL_AcceptCert_Item *accept_item = OP_NEW(SSL_AcceptCert_Item, ());

			if(accept_item)
			{
				accept_item->certificate = Certificate[0];
				accept_item->AcceptedAs.Set(Matched_name);
				accept_item->confirm_mode = (accept_search_mode != SSL_CONFIRMED ? accept_search_mode : (certificate_ctx->GetRemberAcceptAndRefuse() ? PERMANENTLY_CONFIRMED : SESSION_CONFIRMED));
				unsigned long i, count = CertificateNames.Count();
				if(OpStatus::IsSuccess(accept_item->Certificate_names.Resize(count)))
				{
					for(i = 0; i < count; i++)
						accept_item->Certificate_names[i].Set(CertificateNames[i]);
				}
				accept_item->certificate_status = certificate_status;

				server_info->AddAcceptedCertificate(accept_item);
			}
		}
		UserConfirmed = (certificate_ctx->GetRemberAcceptAndRefuse() ? PERMANENTLY_CONFIRMED : SESSION_CONFIRMED);
		security_rating = SECURITY_STATE_LOW;
		low_security_reason |= SECURITY_LOW_REASON_CERTIFICATE_WARNING;

		ret = SSL_CertificateVerifier::Verify_TaskCompleted;
	}
	else
	{
		UserConfirmed = USER_REJECTED;
		if(msg)
			*msg = certificate_ctx->GetAlertMessage();
		ret = SSL_CertificateVerifier::Verify_Failed;
	}

	certificate_ctx.reset();

	return ret;
}

#endif
