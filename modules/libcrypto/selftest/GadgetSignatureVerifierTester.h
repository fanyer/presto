/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GADGET_SIGNATURE_VERIFIER_TESTER
#define GADGET_SIGNATURE_VERIFIER_TESTER

#if defined(SELFTEST) && defined(SIGNED_GADGET_SUPPORT)

#include "modules/libcrypto/include/GadgetSignatureStorage.h"
#include "modules/libcrypto/include/GadgetSignatureVerifier.h"
#include "modules/security_manager/include/security_manager.h"


class GadgetSignatureVerifierTester : public MessageObject
{
public:
	GadgetSignatureVerifierTester()
	{
		OP_ASSERT(g_libcrypto_cert_storage);
	}

	virtual ~GadgetSignatureVerifierTester()
	{
		g_main_message_handler->UnsetCallBacks(this);
	}

public:
	OpString m_gadget_filename;
	OpString m_expected_signature_filename;
	CryptoXmlSignature::VerifyError m_expected_verify_error;

public:
	GadgetSignatureStorage          m_signature_storage;
	GadgetSignatureVerifier         m_verifier;

public:
	void PrepareL(const char* filename,
				  const char* expected_signature_filename,
				  CryptoXmlSignature::VerifyError expected_verify_error)
	{
		m_gadget_filename.SetL(filename);
		m_expected_signature_filename.SetL(expected_signature_filename);
		m_expected_verify_error = expected_verify_error;
	}

	void ProcessL()
	{
		if (!g_libcrypto_cert_storage)
			LEAVE(OpStatus::ERR_OUT_OF_RANGE);

		m_verifier.SetGadgetFilenameL(m_gadget_filename);
		m_verifier.SetCAStorage(g_libcrypto_cert_storage);
		m_verifier.SetGadgetSignatureStorageContainer(&m_signature_storage);
		g_main_message_handler->SetCallBack(
			this, MSG_GADGET_SIGNATURE_VERIFICATION_FINISHED, m_verifier.Id());
		m_verifier.ProcessL();
	}

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		if (msg != MSG_GADGET_SIGNATURE_VERIFICATION_FINISHED )
		{
			ST_failed("Expected another message in callback.");
			return;
		}

		if (par1 != m_verifier.Id())
		{
			ST_failed("Expected another verifier.");
			return;
		}

		CryptoXmlSignature::VerifyError verify_error = m_signature_storage.GetCommonVerifyError();
		if (verify_error != m_expected_verify_error)
		{
			ST_failed("Unexpected result from verifier: %s instead of %s.",
				CryptoXmlSignature::VerifyErrorName(verify_error),
				CryptoXmlSignature::VerifyErrorName(m_expected_verify_error));
			return;
		}

		if (verify_error == CryptoXmlSignature::OK_CHECKED_LOCALLY ||
		    verify_error == CryptoXmlSignature::OK_CHECKED_WITH_OCSP)
		{
			const GadgetSignature* sig = m_signature_storage.GetBestSignature();
			if (!sig)
			{
				ST_failed("NULL signature.");
				return;
			}

			const OpStringC& sig_filename = sig->GetSignatureFilename();
			if (m_expected_signature_filename.HasContent() &&
			    sig_filename != m_expected_signature_filename)
			{
				char* sig_utf8 = NULL;
				sig_filename.UTF8Alloc(&sig_utf8);
				char* expected_utf8 = NULL;
				m_expected_signature_filename.UTF8Alloc(&expected_utf8);

				ST_failed("Unexpected signature filename: %s instead of %s.",
					sig_utf8 ? sig_utf8 : "NULL",
					expected_utf8 ? expected_utf8 : "NULL");

				OP_DELETEA(expected_utf8);
				OP_DELETEA(sig_utf8);
				return;
			}
		}

		OnGadgetSignatureVerificationFinished();
	}

	virtual void OnGadgetSignatureVerificationFinished() { ST_passed(); }

};

inline BOOL StartGadgetSigningTest(
	GadgetSignatureVerifierTester* tester,
	const char* filename,
	const char* expected_signature_filename,
	CryptoXmlSignature::VerifyError expected_verify_error)
{
	if (!tester)
		return FALSE;

	TRAPD(err,
		  tester->PrepareL(filename, expected_signature_filename, expected_verify_error);
		  tester->ProcessL());
	return OpStatus::IsSuccess(err);
}

#endif // SELFTEST && SIGNED_GADGET_SUPPORT
#endif // GADGET_SIGNATURE_VERIFIER_TESTER
