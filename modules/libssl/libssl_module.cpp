/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/libssl/sslbase.h"

OP_STATUS SSL_TidyUp_Random();
void SSL_TidyUp_Options();

void DeactivateCrypto();

#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/libssl_module.h"
#include "modules/url/tools/arrays.h"
#include "modules/libssl/ssl_api.h"
#if defined(_SSL_USE_OPENSSL_)
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/ossl_typ.h"
#include "modules/libssl/external/openssl/cert_store.h"
#endif
#include "modules/libssl/handshake/asn1certlist.h"

#if defined EXTERNAL_DIGEST_API
#include "modules/libssl/external/ext_digest/external_digest_man.h"
#endif

#include "modules/libssl/data/updaters.h"
#include "modules/url/url_man.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/libssl/method_disable.h"
#include "modules/libssl/smartcard/smc_man.h"
#include "modules/libssl/protocol/ssl_false_start_manager.h"
#include "modules/libcrypto/include/CryptoMasterPasswordHandler.h"


LibsslModule::LibsslModule()
	: m_ssl_api(NULL)
#ifdef LIBSSL_ENABLE_SSL_FALSE_START
	, m_ssl_false_start_manager(NULL)
#endif
	, m_securityManager(NULL)
	, m_SSL_Hash_Pointer_NullHash(NULL)
	, m_Crypto_Loaded(FALSE)
	, m_revocation_context(0)
	, m_has_run_intermediate_shutdown(FALSE)
#if defined LIBSSL_AUTO_UPDATE
	, updaters(NULL)
	, tried_to_autoupdate(FALSE)
#endif
	, cert_def1(NULL)
	, m_browsing_certificates(FALSE)
#if defined(_SSL_USE_OPENSSL_)
	, m_cert_store(NULL)
	, m_SSL_RND_Initialized(FALSE)
	, m_SSL_RND_feeder_data(NULL)
#ifdef _SSL_SEED_FROMMESSAGE_LOOP_
	, m_SSL_RND_feeder_pos(0)
	, m_SSL_RND_feeder_len(0)
#ifdef _DEBUG
	, m_idle_feed_count(0)
	, m_been_fed(FALSE)
	, m_feed_count(0)
#endif
#endif
#endif
#if defined EXTERNAL_DIGEST_API
	, m_ext_digest_method_counter(EXTERNAL_DIGEST_ID_START)
	, m_ext_digest_algorithm_counter(SSL_HASH_MAX_NUM)
#endif
	, m_x509_cert_flags_index(-1)
#if  !(defined LIBSSL_ALWAYS_WARN_MD5 || defined LIBOPEAY_DISABLE_MD5_PARTIAL)
	, warn_md5(FALSE)
#endif
#if !(defined LIBSSL_ALWAYS_WARN_SHA1 || defined LIBOPEAY_DISABLE_SHA1_PARTIAL)
	, warn_sha1(FALSE)
#endif
#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
	, disable_internal_repository(FALSE)
#endif // SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	, m_external_clientkey_manager(NULL)
#endif
	, keyex_id(0)
{
}

void LibsslModule::InitL(const OperaInitInfo& info)
{
	CONST_DOUBLE_ARRAY_INIT_L(SSL_Cipher_map);
	CONST_DOUBLE_ARRAY_INIT_L(SSL_Digest_map);
	CONST_DOUBLE_ARRAY_INIT_L(Cipher_ciphers);

	m_ssl_api = OP_NEW_L(SSL_API, ());

#ifdef LIBSSL_ENABLE_SSL_FALSE_START

	if (OpStatus::IsMemoryError(SSLFalseStartManager::Make(m_ssl_false_start_manager)))
		LEAVE(OpStatus::ERR_NO_MEMORY);
#endif

#if defined LIBSSL_AUTO_UPDATE
	updaters = OP_NEW_L(SSL_AutoUpdaters, ());
	updaters->InitL();
#endif
	m_cert_store = OP_NEW_L(SSL_Cert_Store, ());
#if !(defined LIBSSL_ALWAYS_WARN_MD5 || defined LIBOPEAY_DISABLE_MD5_PARTIAL)
	warn_md5 = ((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_WARN_MD5) != 0 ? TRUE : FALSE);
#endif
#if !(defined LIBSSL_ALWAYS_WARN_SHA1 || defined LIBOPEAY_DISABLE_SHA1_PARTIAL)
	warn_sha1 = ((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_WARN_SHA1) != 0 ? TRUE : FALSE);
#endif

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	m_external_clientkey_manager = OP_NEW_L(SSL_ExternalKeyManager, ());
#endif
}

void LibsslModule::InterModuleInitL(const OperaInitInfo& info)
{
	// Here we put the stuff that have to be initialized after the URL module have been initialized
#ifdef SSL_ENABLE_URL_HANDSHAKE_STATUS
	flag_handler.SetIsFlag(TRUE);
	kSSLHandshakeSent = g_url_api->RegisterAttributeL(&flag_handler);
	kSSLHandshakeCompleted = g_url_api->RegisterAttributeL(&flag_handler);
#endif
#ifdef DYNAMIC_FOLDER_SUPPORT
	CheckRevocationContext();
#endif // DYNAMIC_FOLDER_SUPPORT

	// Create security manager if it is not created yet.
	if (!g_ssl_api->CheckSecurityManager())
		LEAVE(OpStatus::ERR_NO_MEMORY);

	if (!g_securityManager->loaded_usercerts)
		LEAVE_IF_ERROR(g_securityManager->Init(SSL_ClientStore));
}

void LibsslModule::InterModuleShutdown()
{
	if (m_has_run_intermediate_shutdown)
		return;

#if defined LIBSSL_AUTO_UPDATE
	if (updaters)
		updaters->Clear();
#endif

	// Save feature_status
	if(g_securityManager)
	{
		TRAPD(op_err, g_securityManager->SaveL());
		OpStatus::Ignore(op_err);
		g_securityManager->PartialShutdownCertificate();
	}

	m_has_run_intermediate_shutdown = TRUE;
}

void LibsslModule::Destroy()
{
	InterModuleShutdown();

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	OP_DELETE(m_external_clientkey_manager);
	m_external_clientkey_manager = NULL;
#endif

#if defined LIBSSL_AUTO_UPDATE
	OP_DELETE(updaters);
	updaters = NULL;
#endif

	OpStatus::Ignore(SSL_TidyUp_Random());
	SSL_TidyUp_Options();

	// if cleaning up due to startup failure, this might be NULL
	if (g_opera->libssl_module.m_cert_store)
		g_opera->libssl_module.m_cert_store->Shutdown();

	extern void Cleanup_HashPointer();
	Cleanup_HashPointer();

#ifdef EXTERNAL_DIGEST_API
	void ShutdownExternalDigest();

	ShutdownExternalDigest();
#endif

	OP_DELETE(cert_def1);
	cert_def1 = NULL;

	CONST_ARRAY_SHUTDOWN(SSL_Digest_map);
	CONST_ARRAY_SHUTDOWN(SSL_Cipher_map);
	CONST_ARRAY_SHUTDOWN(Cipher_ciphers);
	OP_DELETE(m_ssl_api);
	m_ssl_api = NULL;

#if defined(_SSL_USE_OPENSSL_)
	OP_DELETE(m_cert_store);
	m_cert_store = NULL;
#endif

#ifdef LIBSSL_ENABLE_SSL_FALSE_START
	OP_DELETE(m_ssl_false_start_manager);
	m_ssl_false_start_manager = NULL;
#endif


}

#ifdef DYNAMIC_FOLDER_SUPPORT
void LibsslModule::CheckRevocationContext()
{
	if(m_revocation_context)
		return;

	OpFileFolder revocation_cache =OPFILE_ABSOLUTE_FOLDER;
	
	if(OpStatus::IsError(g_folder_manager->AddFolder(OPFILE_CACHE_FOLDER, UNI_L("revocation"),&revocation_cache)))
		return;

	if(revocation_cache == OPFILE_ABSOLUTE_FOLDER)
		return;

	URL_CONTEXT_ID cand = (URL_CONTEXT_ID) this;
	for(int i=10; i>0; i++, cand++)
		if(!urlManager->ContextExists(cand))
		{
			m_revocation_context = cand;
			break;
		}

	if(!m_revocation_context)
		return;

	TRAPD(op_err, urlManager->AddContextL(m_revocation_context, OPFILE_ABSOLUTE_FOLDER, OPFILE_COOKIE_FOLDER, revocation_cache, revocation_cache,TRUE, (int) PrefsCollectionNetwork::DiskCacheSize));
	if(OpStatus::IsError(op_err))
	{
		m_revocation_context = 0;
		// Ignore error
	}
}

#endif // DYNAMIC_FOLDER_SUPPORT

#endif // _NATIVE_SSL_SUPPORT_
