/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_LIBSSL_LIBSSL_MODULE_H
#define MODULES_LIBSSL_LIBSSL_MODULE_H

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/hardcore/opera/module.h"

#if defined(_SSL_USE_OPENSSL_)

#ifndef CLEAR_PASSWD_FROM_MEMORY
#error "For security reasons FEATURE_CLEAR_PASSWORDS *MUST* be YES when libssl with libopeay is used"
#endif
#endif
#include "modules/url/tools/arrays_decl.h"
#include "modules/libssl/base/sslenum2.h"
#include "modules/url/url_id.h"
#include "modules/url/url2.h"
#include "modules/url/url_dynattr.h"
#include "modules/libssl/keyex/revokelist.h"

class SSL_Options;
class SSL_Null_Hash;
class SSL_ASN1Cert_list;
class SSL_API;
struct SSL_Cipher_and_NID;
struct SSL_Digest_and_NID;
struct Cipher_spec;
class SSL_AutoUpdaters;
class SSL_Cert_Store;
class SSL_ExternalKeyManager;
class SSLFalseStartManager;

class LibsslModule : public OperaModule
{
public:
	SSL_API *m_ssl_api;
#ifdef LIBSSL_ENABLE_SSL_FALSE_START
	SSLFalseStartManager *m_ssl_false_start_manager;
#endif

	SSL_Options *m_securityManager;
	SSL_Null_Hash *m_SSL_Hash_Pointer_NullHash;
	BOOL m_Crypto_Loaded;
	URL_CONTEXT_ID m_revocation_context;
	BOOL m_has_run_intermediate_shutdown;
#if defined LIBSSL_AUTO_UPDATE
	SSL_AutoUpdaters *updaters;
	BOOL tried_to_autoupdate;
#endif
	SSL_ASN1Cert_list* cert_def1;
	BOOL m_browsing_certificates;
	SSL_Revoke_List m_revoked_certificates;
#if defined(_SSL_USE_OPENSSL_)
	SSL_Cert_Store *m_cert_store;
	BOOL  m_SSL_RND_Initialized;
	DWORD *m_SSL_RND_feeder_data;
#ifdef _SSL_SEED_FROMMESSAGE_LOOP_
	unsigned m_SSL_RND_feeder_pos,  m_SSL_RND_feeder_len;
#ifdef _DEBUG
	unsigned int m_idle_feed_count;
	BOOL m_been_fed;
	unsigned int m_feed_count;
#endif
#endif
#endif
	DECLARE_MODULE_CONST_ARRAY(SSL_Cipher_and_NID, SSL_Cipher_map);
	DECLARE_MODULE_CONST_ARRAY(SSL_Digest_and_NID, SSL_Digest_map);
	DECLARE_MODULE_CONST_ARRAY(struct Cipher_spec, Cipher_ciphers);
#if defined EXTERNAL_DIGEST_API
	int m_ext_digest_method_counter;
	SSL_HashAlgorithmType m_ext_digest_algorithm_counter;
#endif
	int m_x509_cert_flags_index;

#if  !(defined LIBSSL_ALWAYS_WARN_MD5  || defined LIBOPEAY_DISABLE_MD5_PARTIAL)
	BOOL warn_md5;
#endif
#if !(defined LIBSSL_ALWAYS_WARN_SHA1 || defined LIBOPEAY_DISABLE_SHA1_PARTIAL)
	BOOL warn_sha1;
#endif
#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
	Head	external_repository; // Head is easier to manage deletions for than a pointer.
	BOOL	disable_internal_repository;
#endif // SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	// Smartcard support
	// Code is located in smc_man.cpp
	SSL_ExternalKeyManager *m_external_clientkey_manager;
#endif // _SSL_USE_EXTERNAL_KEYMANAGERS_

#ifdef SSL_ENABLE_URL_HANDSHAKE_STATUS
private:
	URL_DynamicUIntAttributeHandler flag_handler;

public:
	/** URL Attribute for SSL handshake started */
	URL::URL_Uint32Attribute kSSLHandshakeSent;
	/** URL Attribute for SSL handshake completed */
	URL::URL_Uint32Attribute kSSLHandshakeCompleted;
#endif
	/** What is the ID that will be assigned to the next KeyExchange object ?*/
	unsigned int	keyex_id;


public:
	LibsslModule();
	virtual ~LibsslModule(){};
	virtual void InitL(const OperaInitInfo& info);
	virtual void InterModuleInitL(const OperaInitInfo& info);

	virtual void Destroy();
	virtual void InterModuleShutdown();

#ifdef DYNAMIC_FOLDER_SUPPORT
	void CheckRevocationContext();
#endif
};

#define LIBSSL_MODULE_REQUIRED

#define g_ssl_module				g_opera->libssl_module
#define g_ssl_api					g_opera->libssl_module.m_ssl_api
#define g_ssl_false_start_manager	g_opera->libssl_module.m_ssl_false_start_manager
#define g_securityManager			g_opera->libssl_module.m_securityManager
#define g_ssl_auto_updaters			g_opera->libssl_module.updaters
#define g_ssl_tried_auto_updaters	g_opera->libssl_module.tried_to_autoupdate
#define g_SSL_Hash_Pointer_NullHash	g_opera->libssl_module.m_SSL_Hash_Pointer_NullHash
#define g_cert_def1					g_opera->libssl_module.cert_def1
#define g_browsing_certificates		g_opera->libssl_module.m_browsing_certificates
#define g_store						g_opera->libssl_module.m_cert_store->cert_store
#define g_SSL_RND_Initialized			g_opera->libssl_module.m_SSL_RND_Initialized
#define g_SSL_RND_feeder_data			g_opera->libssl_module.m_SSL_RND_feeder_data
#define g_revocation_context			g_opera->libssl_module.m_revocation_context
#define g_revoked_certificates			g_opera->libssl_module.m_revoked_certificates
#ifdef _SSL_SEED_FROMMESSAGE_LOOP_
#define g_SSL_RND_feeder_pos			g_opera->libssl_module.m_SSL_RND_feeder_pos
#define g_SSL_RND_feeder_len			g_opera->libssl_module.m_SSL_RND_feeder_len
#ifdef _DEBUG
#define g_idle_feed_count			g_opera->libssl_module.m_idle_feed_count
#define g_been_fed			g_opera->libssl_module.m_been_fed
#define g_feed_count			g_opera->libssl_module.m_feed_count
#endif
#ifdef LIBSSL_ALWAYS_WARN_MD5
#define g_SSL_warn_md5			TRUE
#else
#define g_SSL_warn_md5			g_opera->libssl_module.warn_md5
#endif
#ifdef LIBSSL_ALWAYS_WARN_SHA1
#define g_SSL_warn_sha1			TRUE
#else
#define g_SSL_warn_sha1			g_opera->libssl_module.warn_sha1
#endif
#ifdef _SSL_PROVIDE_OP_RAND
#define g_ssl_oprand_buffer g_opera->libssl_module.op_rand_buffer
#define g_ssl_oprand_buffer_pos g_opera->libssl_module.op_rand_buffer_pos
#define g_ssl_oprand_last_refresh g_opera->libssl_module.rand_last_refresh
#endif
#endif
#ifndef HAS_COMPLEX_GLOBALS
# define g_SSL_Cipher_map	CONST_ARRAY_GLOBAL_NAME(libssl, SSL_Cipher_map)
# define g_SSL_Digest_map	CONST_ARRAY_GLOBAL_NAME(libssl, SSL_Digest_map)
#endif
#if defined EXTERNAL_DIGEST_API
# define g_ext_digest_method_counter	g_opera->libssl_module.m_ext_digest_method_counter
# define g_ext_digest_algorithm_counter	g_opera->libssl_module.m_ext_digest_algorithm_counter
#endif

#define securityManager				g_opera->libssl_module.m_securityManager
#define SSL_RND_feeder_data			g_opera->libssl_module.m_SSL_RND_feeder_data
#ifdef _SSL_SEED_FROMMESSAGE_LOOP_
#define SSL_RND_feeder_pos			g_opera->libssl_module.m_SSL_RND_feeder_pos
#define SSL_RND_feeder_len			g_opera->libssl_module.m_SSL_RND_feeder_len
#endif

#define g_SSL_X509_cert_flags_index	g_opera->libssl_module.m_x509_cert_flags_index

#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
#define g_ssl_external_repository g_opera->libssl_module.external_repository
#define g_ssl_disable_internal_repository g_opera->libssl_module.disable_internal_repository
#endif // SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
#define g_external_clientkey_manager g_opera->libssl_module.m_external_clientkey_manager
#endif // _SSL_USE_EXTERNAL_KEYMANAGERS_

#ifdef SSL_ENABLE_URL_HANDSHAKE_STATUS
#define g_KSSLHandshakeSent g_opera->libssl_module.kSSLHandshakeSent
#define g_KSSLHandshakeCompleted g_opera->libssl_module.kSSLHandshakeCompleted
#endif

#define g_keyex_id g_opera->libssl_module.keyex_id

#endif // _NATIVE_SSL_SUPPORT_

#endif // !MODULES_LIBSSL_LIBSSL_MODULE_H
