/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/ssl_api.h"

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/protocol/op_ssl.h"
#include "modules/libssl/methods/sslpubkey.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

void SSL_API::RemoveURLReferences(ProtocolComm *comm)
{
	static_cast<SSL *>(comm)->RemoveURLReferences();
}

SSL_Options *SSL_API::CreateSecurityManager(BOOL register_changes, OpFileFolder folder)
{
	SSL_Options *optionsManager = OP_NEW(SSL_Options, (folder));

	if (optionsManager == NULL)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	        return NULL;
	}

	if (OpStatus::IsError(optionsManager->Init()))
	{
		OP_DELETE(optionsManager);
		return NULL;
	}

	optionsManager->Set_RegisterChanges(register_changes);

	return optionsManager;
}

void SSL_API::CommitOptionsManager(SSL_Options *optionsManager)
{
	if(!optionsManager
#ifdef SELFTEST
		|| (lock_security_manager && g_securityManager)
#endif
		)
		return;

	if(g_securityManager)
	{
		SSL_Options *new_Manager = CreateSecurityManager();

		if(!new_Manager)
			return;

		new_Manager->PasswordLockCount = g_securityManager->PasswordLockCount;


		new_Manager->Init(
			(optionsManager->loaded_cacerts || g_securityManager->loaded_cacerts ? SSL_LOAD_CA_STORE : 0) |
			(optionsManager->loaded_intermediate_cacerts || g_securityManager->loaded_intermediate_cacerts ? SSL_LOAD_INTERMEDIATE_CA_STORE : 0) |
			(optionsManager->loaded_usercerts || g_securityManager->loaded_usercerts ? SSL_LOAD_CLIENT_STORE : 0) |
			(optionsManager->loaded_trusted_serves || g_securityManager->loaded_trusted_serves ? SSL_LOAD_TRUSTED_STORE : 0) |
			(optionsManager->loaded_untrusted_certs|| g_securityManager->loaded_untrusted_certs ? SSL_LOAD_UNTRUSTED_STORE : 0)
			);
		if(new_Manager->loaded_primary != optionsManager->loaded_primary ||
			(!new_Manager->loaded_cacerts  && (optionsManager->loaded_cacerts || g_securityManager->loaded_cacerts)) ||
			(!new_Manager->loaded_intermediate_cacerts  && (optionsManager->loaded_intermediate_cacerts || g_securityManager->loaded_intermediate_cacerts)) ||
			(!new_Manager->loaded_usercerts  && (optionsManager->loaded_usercerts || g_securityManager->loaded_usercerts )) ||
			(!new_Manager->loaded_trusted_serves && (optionsManager->loaded_trusted_serves|| g_securityManager->loaded_trusted_serves)) ||
			(!new_Manager->loaded_untrusted_certs && (optionsManager->loaded_untrusted_certs || g_securityManager->loaded_untrusted_certs))
			)
			return;

		if(!new_Manager->LoadUpdates(optionsManager))
			return;

		if(!optionsManager->updated_password &&
			new_Manager->SystemPartPassword == g_securityManager->SystemPartPassword &&
			new_Manager->SystemPartPasswordSalt == g_securityManager->SystemPartPasswordSalt)
		{
			if((g_securityManager->PasswordAging == SSL_ASK_PASSWD_ONCE ||  
				g_securityManager->PasswordAging == SSL_ASK_PASSWD_AFTER_TIME) &&
				(new_Manager->PasswordAging == SSL_ASK_PASSWD_ONCE ||  
				new_Manager->PasswordAging == SSL_ASK_PASSWD_AFTER_TIME))
			{
				g_securityManager->CheckPasswordAging();
				if(g_securityManager->obfuscated_SystemCompletePassword.GetLength())
				{
					g_securityManager->DeObfuscate(new_Manager->SystemCompletePassword);
					new_Manager->Obfuscate();
					new_Manager->SystemPasswordVerifiedLast = g_securityManager->SystemPasswordVerifiedLast;
					new_Manager->CheckPasswordAging();
				}
			}
		}

		new_Manager->ask_security_password = g_securityManager->ask_security_password;
		g_securityManager->ask_security_password = NULL;

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
		new_Manager->AutoRetrieved_certs.Append(&g_securityManager->AutoRetrieved_certs);
		new_Manager->AutoRetrieved_untrustedcerts.Append(&g_securityManager->AutoRetrieved_untrustedcerts);
#endif
		new_Manager->AutoRetrieved_urls.Append(&g_securityManager->AutoRetrieved_urls);

		if(g_securityManager->dec_reference() <= 0)
			OP_DELETE(g_securityManager);

		g_securityManager = new_Manager;
	}
	else
	{
		g_securityManager = optionsManager;
		optionsManager->Set_RegisterChanges(FALSE);
		optionsManager->inc_reference();
	}

	g_securityManager->Save();
}

BOOL SSL_API::CheckSecurityManager()
{
	if(g_securityManager)
		return TRUE;

	g_securityManager = CreateSecurityManager();

	return (g_securityManager != NULL);
}

#ifdef SELFTEST
void SSL_API::UnLoadSecurityManager(BOOL finished)
{
	if(g_securityManager && g_securityManager->dec_reference() <= 0)
		OP_DELETE(g_securityManager);
	g_securityManager = NULL;
	if(finished)
		CheckSecurityManager();
}
#endif


void SSL_API::CheckSecurityTimeouts()
{
	if(g_securityManager)
		g_securityManager->CheckPasswordAging();
}



ProtocolComm *SSL_API::Generate_SSL(MessageHandler* msg_handler, ServerName* hostname,
						   unsigned short portnumber, BOOL V3_handshake, BOOL do_record_splitting)
{
	SSL *ssl = OP_NEW(SSL, (msg_handler,  hostname, portnumber, do_record_splitting));

	if(!ssl)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return NULL;
	}

	if(ssl->ErrorRaisedFlag)
	{
		SSL_Alert msg;

		ssl->Error(&msg);
		if(msg.GetDescription() == SSL_Allocation_Failure)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
		OP_DELETE(ssl);
		return NULL;
	}

	if(V3_handshake)
		ssl->ForceV3Handshake();
	
	ssl->ResetError();
	return ssl;
}

uint32 SSL_API::DetermineSecurityStrength(SSL_PublicKeyCipher *key, SSL_keysizes &lowest_key, int &security_rating, int &low_security_reason)
{
	if(!key)
		return 0;

	uint32 kea_bits = 0;
	BOOL low_changed = FALSE;

	kea_bits = key->KeyBitsLength();

	if(kea_bits && (!lowest_key.RSA_keysize || lowest_key.RSA_keysize > kea_bits))
	{
		low_changed = TRUE;
		lowest_key.RSA_keysize = kea_bits;
	}

	if(low_changed && lowest_key.RSA_keysize && security_rating >= SECURITY_STATE_HALF)
	{

		uint32 rsa_level_2_lower_limit = 900;
		uint32 rsa_level_3_lower_limit = 1000;

		uint32 sec_level_policy = (uint32) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MinimumSecurityLevel);
		switch(sec_level_policy)
		{
		case 0:
			break;
		case 1:
			rsa_level_3_lower_limit = 1020;
			break;
		case 2:
			rsa_level_3_lower_limit = 1100;
			break;
		case 3:
			rsa_level_3_lower_limit = 1200;
			break;
		default:
			rsa_level_2_lower_limit = 900 + (sec_level_policy -3)*100;
			rsa_level_3_lower_limit = 1200 + (sec_level_policy -3)*100;
			break;
		}

		if(lowest_key.RSA_keysize < rsa_level_2_lower_limit)
		{
			low_security_reason |= SECURITY_LOW_REASON_WEAK_KEY;
			security_rating = SECURITY_STATE_LOW;
		}
		else if(lowest_key.RSA_keysize < rsa_level_3_lower_limit ) // lowered to 999 because of the RSA Secure Server CA certificate because it is using a 1000 bit key
		{
			low_security_reason |= SECURITY_LOW_REASON_SLIGHTLY_WEAK_KEY;
			security_rating --;
		}
		else if(security_rating> SECURITY_STATE_FULL)
		{
			security_rating = SECURITY_STATE_FULL;
		}
			
	}

	return kea_bits;
}

#ifdef LIBSSL_ENABLE_KEYGEN

#define SSL_KeygenSize_FIRST 1024
#define SSL_KeygenSize_LAST 4096
#define SSL_KeygenSize_STEP 256
#define SSL_KeygenSize_DEFAULT 1536

unsigned int SSL_API::SSL_GetKeygenSize(SSL_BulkCipherType type, int i)
{
	if(type != SSL_RSA)
		return 0;

	i--;
	if (i< 0 || (unsigned int) i > ((unsigned int)SSL_KeygenSize_LAST - (unsigned int)SSL_KeygenSize_FIRST)/ (unsigned int)SSL_KeygenSize_STEP)
		return 0;
	return ((unsigned int)SSL_KeygenSize_FIRST + (unsigned int)SSL_KeygenSize_STEP*(unsigned int)i);
}

unsigned int SSL_API::SSL_GetDefaultKeyGenSize(SSL_BulkCipherType type)
{
	if(type != SSL_RSA)
		return 0;

	return SSL_KeygenSize_DEFAULT;
}
#endif

#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
#include "modules/libssl/ext_certrepository.h"

void SSL_API::RegisterExternalCertLookupRepository(SSL_External_CertRepository_Base *repository)
{
	if(repository)
		repository->IntoStart(&g_ssl_external_repository);
	else if(g_ssl_external_repository.Empty())
		g_ssl_disable_internal_repository = FALSE;
}

void SSL_API::SetDisableInternalLookupRepository(BOOL disable)
{
	if(g_ssl_external_repository.Empty())
		disable = FALSE;

	g_ssl_disable_internal_repository = disable;
}

#endif // SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
#include "modules/libssl/smartcard/smc_man.h"

void SSL_API::RegisterExternalKeyManager(SSL_KeyManager *provider)
{
	g_external_clientkey_manager->RegisterKeyManager(provider);
}
#endif // _SSL_USE_EXTERNAL_KEYMANAGERS_

#endif
