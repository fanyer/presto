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
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
#include "modules/libssl/data/root_auto_retrieve.h"
#endif


SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_ExtractNames(SSL_Alert *msg)
{
	uint24 i;
	CertificateNames.Resize(0);
	for(i=0; i< val_certificate_count;i++)
	{
		OpString_list name;

		if(OpStatus::IsError(cert_handler_validated->GetSubjectName(i, name)))
			break;

		if(OpStatus::IsError(CertificateNames.Resize(i+1)))
			break;

		if(OpStatus::IsError(CertificateNames[i].Set(name[1].HasContent() ? name[1] : name[0])))
			break;

		if(i== 0)
		{
			if(OpStatus::IsError(CertificateNames[i].SetConcat((name[1].HasContent() ? name[1] : name[0]), UNI_L(", "), name[4], NULL)))
				break;
		}
		else if(OpStatus::IsError(CertificateNames[i].Set(name[1].HasContent() ? name[1] : name[0])))
			break;
	}

	if(val_certificate_count == 1)
	{
		do{
			OpString_list name;

			if(OpStatus::IsError(cert_handler_validated->GetIssuerName(0, name)))
				break;

			int j = CertificateNames.Count();
			if(OpStatus::IsError(CertificateNames.Resize(j+1)))
				break;
	
			if(OpStatus::IsError(CertificateNames[j].Set(name[1].HasContent() ? name[1] : name[0])))
				break;
		}while(0);
	}

	return SSL_CertificateVerifier::Verify_TaskCompleted;
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_ExtractVerificationStatus(SSL_Alert *msg)
{
	OP_DELETEA(warnstatus);

	warnstatus = cert_handler->ExtractVerificationStatus(warncount);
	if(warnstatus == NULL)
	{
		if(msg != NULL)
			msg->Set(SSL_Internal, SSL_Allocation_Failure);
		return SSL_CertificateVerifier::Verify_Failed;
	}
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	if(disable_EV)
		warnstatus[0].extended_validation = FALSE;
#endif

	return SSL_CertificateVerifier::Verify_TaskCompleted;
}

const SSL_CertificateVerification_Info *SSL_CertificateVerifier::WarnList() const
{
	return warnstatus;
}

#endif
