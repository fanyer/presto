/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/methods/sslcipher.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"


#define DECLARE_PASSWORD_SPEC DataRecord_Spec password_spec(TRUE, 1, TRUE, FALSE, TRUE, 4, TRUE, TRUE);

void SSL_TidyUp_Options()
{
	if(g_securityManager != NULL)
	{
		if(g_securityManager->dec_reference() == 0)
			OP_DELETE(g_securityManager);
		g_securityManager= NULL;
	}
}

SSL_Security_ProfileList *SSL_Options::FindSecurityProfile(ServerName *server, unsigned short port)
{
	return (current_security_profile != NULL ? current_security_profile : default_security_profile);
}

SSL_Options::SSL_Options(OpFileFolder folder)
{
	InternalInit(folder);
}

void SSL_Options::InternalInit(OpFileFolder folder)
{
	storage_folder = folder;
	initializing = FALSE;
	register_updates = FALSE;
	security_profile = NULL;
	current_security_profile = default_security_profile = security_profile = NULL	;

	loaded_primary= loaded_cacerts=
		loaded_intermediate_cacerts = loaded_usercerts  =
		loaded_trusted_serves = loaded_untrusted_certs =FALSE;

    Enable_SSL_V3_0 = TRUE;
	Enable_TLS_V1_0 = TRUE;
	Enable_TLS_V1_1 = TRUE;
#ifdef _SUPPORT_TLS_1_2
	Enable_TLS_V1_2 = TRUE;
#endif

    SecurityEnabled= FALSE;
    ComponentsLacking = TRUE;

	updated_protocols = updated_v2_ciphers = updated_v3_ciphers =
		updated_password_aging = updated_password = updated_external_items =
		updated_repository = FALSE;

	referencecount = 1;
	usercert_base_version = keybase_version = 0;

	obfuscation_cipher = NULL;

	PasswordLockCount = 0;

	DECLARE_PASSWORD_SPEC;

	SystemPartPassword.SetRecordSpec(password_spec);
	SystemPartPassword.SetTag(TAG_SSL_PASSWD_MAINBLOCK);
	SystemPartPasswordSalt.SetRecordSpec(password_spec);
	SystemPartPasswordSalt.SetTag(TAG_SSL_PASSWD_MAINSALT);
	ask_security_password = NULL;

	if (g_pcnet)
	{
		g_pcnet->RegisterListenerL(this);
	}

}

SSL_Options::~SSL_Options()
{
	InternalDestruct();
}

void SSL_Options::InternalDestruct()
{
	security_profile = NULL;

	current_security_profile = NULL;
	default_security_profile = NULL;

	OP_DELETE(obfuscation_cipher);
	obfuscation_cipher  = NULL;

	OP_DELETE(ask_security_password);
	ask_security_password = NULL;
	if (g_pcnet)
	{
		g_pcnet->UnregisterListener(this);
	}

}

/* virtual */void SSL_Options::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	BOOL new_boolean = newvalue ? TRUE : FALSE;
	switch (id)
	{
	case OpPrefsCollection::Network:
		switch ((enum PrefsCollectionNetwork::integerpref) pref)
		{
		case PrefsCollectionNetwork::EnableSSL3_0:

			Enable_SSL_V3_0 = new_boolean;
			break;

		case PrefsCollectionNetwork::EnableSSL3_1:
			Enable_TLS_V1_0 = new_boolean;
			break;

		case PrefsCollectionNetwork::EnableTLS1_1:
			Enable_TLS_V1_1 = new_boolean;
			break;

#ifdef _SUPPORT_TLS_1_2
		case PrefsCollectionNetwork::EnableTLS1_2:
			Enable_TLS_V1_2 = new_boolean;
			break;
#endif // _SUPPORT_TLS_1_2
		}

		SecurityEnabled =
			Enable_SSL_V3_0 ||
			Enable_TLS_V1_0 ||
			Enable_TLS_V1_1
#ifdef _SUPPORT_TLS_1_2
			|| Enable_TLS_V1_2
#endif // _SUPPORT_TLS_1_2
			;
	}
}

BOOL SSL_Options::IsTestFeatureAvailable(TLS_Feature_Test_Status protocol)
{
	switch (protocol)
	{
		case TLS_Test_1_0:
		case TLS_Test_1_0_Extensions:
			return Enable_TLS_V1_0;
		//case TLS_Test_1_1_Extensions:
		//	return Enable_TLS_V1_1;
		case TLS_Test_1_2_Extensions:
			return Enable_TLS_V1_2;
		case TLS_Test_SSLv3_only:
			return Enable_SSL_V3_0;

		// Final versions are always enabled, they will use highes enabled version anyway.
		case TLS_SSL_v3_only:
		case TLS_1_0_only:
		case TLS_1_0_and_Extensions:
		//case TLS_1_1_and_Extensions:
		case TLS_1_2_and_Extensions:
			return TRUE;
	}
	return FALSE;
}

BOOL SSL_Options::IsEnabled(UINT8 major, UINT8 minor)
{
	SSL_ProtocolVersion version(major, minor);
	if (version.Compare(3, 1) == 0)
		return Enable_TLS_V1_0;
	else if (version.Compare(3, 2) == 0)
		return Enable_TLS_V1_1;
	else if (version.Compare(3, 3) == 0)
		return Enable_TLS_V1_2;
	else if (version.Compare(3, 0) == 0)
		return Enable_SSL_V3_0;
	else
		return FALSE;
}

void SSL_Options::PartialShutdownCertificate()
{
	System_ClientCerts.Clear();
	System_UnTrustedSites.Clear();
	System_TrustedSites.Clear();
	System_IntermediateCAs.Clear();
	System_TrustedCAs.Clear();

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	AutoRetrieved_untrustedcerts.Clear();
	AutoRetrieved_certs.Clear();
#endif
	AutoRetrieved_urls.Clear();
}

void SSL_Options::RemoveSensitiveData()
{
	if(PasswordLockCount == 0)
	{
		SystemCompletePassword.Resize(0);
		ClearObfuscated();
	}
	SystemPasswordVerifiedLast = 0;

	System_TrustedSites.Clear();
	loaded_trusted_serves = FALSE;
	System_UnTrustedSites.Clear();
	loaded_untrusted_certs = FALSE;

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	AutoRetrieved_certs.Clear();
	AutoRetrieved_untrustedcerts.Clear();
#endif
	AutoRetrieved_urls.Clear();
	
    OpStackAutoPtr<OpFile> input(OP_NEW(OpFile, ()));
	if(input.get() && storage_folder != OPFILE_ABSOLUTE_FOLDER)
	{
		if (OpStatus::IsSuccess(input->Construct(UNI_L("opssl6.dat"), storage_folder)))
		{
			OpStatus::Ignore(input->Delete());
		}
	}
}

#endif // relevant support
