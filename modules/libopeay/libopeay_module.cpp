/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "modules/libopeay/libopeay_module.h"

#if defined(_SSL_USE_OPENSSL_) || defined(LIBOPEAY_ENABLE_PARTLY_OPENSSL_SUPPORT)
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/err.h"
#include "modules/libopeay/openssl/rand.h"
#include "modules/libopeay/openssl/x509v3.h"
#include "modules/libopeay/openssl/pkcs12.h"

#include "modules/libopeay/libopeay_implmodule.h"
#include "modules/libopeay/libopeay_arrays.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/libssl/method_disable.h"

#ifndef OPENSSL_NO_ENGINE
# include "modules/libopeay/openssl/engine.h"
#endif

#ifdef OPERA_SMALL_VERSION

extern "C"{

#ifdef HAS_COMPLEX_GLOBALS
extern X509_CRL_METHOD const OPENSSL_GLOBAL_ITEM_NAME(x_crl__int_crl_meth);
#endif

// This list is sorted by variable name.
#ifndef OPENSSL_NO_ENGINE
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(STACK_OF(ENGINE_CLEANUP_ITEM) *, cleanup_stack);
#endif
#if defined SSL_USE_WEAK_CIPHERS
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(int,                     DES_check_key);
#endif
#ifndef OPENSSL_NO_ENGINE
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(ENGINE *,                engine_list_head);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(ENGINE *,                engine_list_tail);
#endif
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(ERR_STATE,               err__fallback);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(const ERR_FNS *,         err__err_fns);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(LHASH_OF(ERR_STRING_DATA) *, err__int_error_hash);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(LHASH_OF(ERR_STATE) *,   err__int_thread_hash);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(int,                     err__int_thread_hash_references);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(int,                     err__int_err_library_number);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(_LHASH *,                ex_data);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(int,                     free_type);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(int,                     have_perfc);
#if defined(_MSC_VER) && defined(_M_X86)
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(int,                     have_tsc);
#endif
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(Libopeay_ImplementModuleData::hmac__m_t, hmac__m);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(_LHASH *,                int_thread_hash);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(int,                     int_thread_hash_references);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(unsigned int,            md_rand__crypto_lock_rand);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(double,                  md_rand__entropy);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(int,                     md_rand__initialized);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(CRYPTO_THREADID,         md_rand__locking_threadid);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(Libopeay_ImplementModuleData::md_rand__md_t,       md_rand__md);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(Libopeay_ImplementModuleData::md_rand__md_count_t, md_rand__md_count);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(Libopeay_ImplementModuleData::md_rand__state_t,    md_rand__state);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(int,                     md_rand__state_index);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(int,                     md_rand__state_num);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(STACK_OF(NAME_FUNCS) *,  name_funcs_stack);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(_LHASH *,                names_lh);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(LHASH_OF(ADDED_OBJ) *,   obj_dat__added);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(int,                     obj_dat__new_nid);
#ifdef LIBOPEAY_PKCS12_SUPPORT
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(_STACK *,                pbe_algs);
#endif
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(volatile int,            stirred_pool);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(STACK_OF(X509_TRUST) *,  trtable);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(const X509_CRL_METHOD *, x_crl__default_crl_method);
OPENSSL_IMPLEMENT_GLOBAL_OPERA_FUN(STACK_OF(X509_PURPOSE) *, xptable);

// Implementation resides in modules/libopeay/addon/purposes.h.
int AddOperaSpecificPurposes();

} // extern "C"

Libopeay_ImplementModuleData::Libopeay_ImplementModuleData()
{
	// This list is sorted by variable name.
#ifndef OPENSSL_NO_ENGINE
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(cleanup_stack)              = NULL;
#endif
#if defined SSL_USE_WEAK_CIPHERS
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(DES_check_key)              = 0;
#endif
#ifndef OPENSSL_NO_ENGINE
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(engine_list_head) = NULL;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(engine_list_tail) = NULL;
#endif
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(err__err_fns                = NULL);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(err__int_error_hash         = NULL);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(err__int_thread_hash        = NULL);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(err__int_thread_hash_references = 0);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(err__int_err_library_number = ERR_LIB_USER);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(ex_data)                    = NULL;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(free_type)                  = 0;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(have_perfc)                 = 1;
#if defined(_MSC_VER) && defined(_M_X86)
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(have_tsc)                   = 1;
#endif
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(int_thread_hash)            = NULL;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(int_thread_hash_references) = 0;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(md_rand__crypto_lock_rand)  = 0;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(md_rand__entropy)           = 0;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(md_rand__initialized)       = 0;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(md_rand__state_index)       = 0;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(md_rand__state_num)         = 0;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(name_funcs_stack)           = NULL;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(names_lh)                   = NULL;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(obj_dat__added)             = NULL;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(obj_dat__new_nid)           = 0;
#ifdef LIBOPEAY_PKCS12_SUPPORT
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(pbe_algs)                   = NULL;
#endif
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(stirred_pool)               = 0;
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(trtable)                    = NULL;
#if defined(_SSL_USE_OPENSSL_) || defined(EXTERNAL_SSL_OPENSSL_IMPLEMENTATION)
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(x_crl__default_crl_method)  = &OPENSSL_GLOBAL_ITEM_NAME(x_crl__int_crl_meth);
#endif
	OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(xptable)                    = NULL;

#ifdef LIBOPEAY_X509_EX_DATA
	ev_oid_x509_list_index = -1;
#endif

# define MEMSET0(a) op_memset(&OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(a), 0, sizeof(OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(a)));

	// Init arrays and structures by binary nulls.
	MEMSET0(err__fallback);
	MEMSET0(hmac__m);
	MEMSET0(md_rand__locking_threadid);
	MEMSET0(md_rand__md);
	MEMSET0(md_rand__md_count);
	MEMSET0(md_rand__state);

# undef MEMSET0
}

// Copied from err.c
#define err_clear_data(p,i) \
	do { \
	if (((p)->err_data[i] != NULL) && \
		(p)->err_data_flags[i] & ERR_TXT_MALLOCED) \
		{  \
		OPENSSL_free((p)->err_data[i]); \
		(p)->err_data[i]=NULL; \
		} \
	(p)->err_data_flags[i]=0; \
	} while(0)

#define err_clear(p,i) \
	do { \
	(p)->err_flags[i]=0; \
	(p)->err_buffer[i]=0; \
	err_clear_data(p,i); \
	} while(0)

Libopeay_ImplementModuleData::~Libopeay_ImplementModuleData()
{
	if (g_opera->libopeay_module.m_data)
	{
#ifdef LIBOPEAY_PKCS12_SUPPORT
		EVP_PBE_cleanup();
#endif
#ifndef OPENSSL_NO_ENGINE 
		ENGINE_cleanup();
#endif
#if defined(_SSL_USE_OPENSSL_) || defined(USE_OPENSSL_CERTIFICATE_VERIFICATION)
		CRYPTO_cleanup_all_ex_data();
#endif

		ERR_remove_thread_state(NULL);
	}	

	ERR_STATE *err_fallback = &OPENSSL_IMPLEMENT_GLOBAL_OPERA_NAME(err__fallback);
	for (int i=0; i<ERR_NUM_ERRORS; i++)
	{
		err_clear(err_fallback, i);
	}
	err_fallback->top=err_fallback->bottom=0;
}

LibopeayModule::LibopeayModule() : 
#ifndef HAS_COMPLEX_GLOBALS
	m_consts(NULL),
#endif
	m_data(NULL)
#if !defined LIBOPEAY_DISABLE_MD5_PARTIAL
	, turn_off_md5(FALSE)
#endif
#if !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
	, turn_off_sha1(FALSE)
#endif
{
}

void LibopeayModule::InitL(const OperaInitInfo& info)
{
#ifndef HAS_COMPLEX_GLOBALS
#if defined(_SSL_USE_OPENSSL_) || defined(USE_OPENSSL_CERTIFICATE_VERIFICATION)
	InitGlobalsL();
#endif
#endif

	m_data = OP_NEW_L(Libopeay_ImplementModuleData, ());

#ifndef LIBOPEAY_DISABLE_MD5_PARTIAL
	turn_off_md5  = ((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_DISABLE_MD5) != 0 ? TRUE : FALSE);
#endif
#ifndef LIBOPEAY_DISABLE_SHA1_PARTIAL
	turn_off_sha1 = ((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_DISABLE_SHA1) != 0 ? TRUE : FALSE);
#endif

#if defined(_SSL_USE_OPENSSL_) || defined(USE_OPENSSL_CERTIFICATE_VERIFICATION)

#ifndef OPENSSL_NO_ENGINE
	ENGINE_load_openssl();
#endif

	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();

	int purposes_err = AddOperaSpecificPurposes();
	int other_err    = ERR_peek_error();
	ERR_clear_error();

	if(purposes_err != 1 || other_err != 0)
		LEAVE(OpStatus::ERR);

#endif // defined(_SSL_USE_OPENSSL_) || defined(USE_OPENSSL_CERTIFICATE_VERIFICATION)
}

void LibopeayModule::Destroy()
{
}

LibopeayModule::~LibopeayModule()
{
	// if cleaning up due to startup failure, libopeay might not have been initialised
	if (m_data)
	{
#if defined(_SSL_USE_OPENSSL_) || defined(USE_OPENSSL_CERTIFICATE_VERIFICATION)

#ifdef LIBOPEAY_PKCS12_SUPPORT
		EVP_PBE_cleanup();
#endif
		EVP_cleanup();
		X509_PURPOSE_cleanup();
		//X509_TRUST_cleanup();
#ifndef OPENSSL_NO_ENGINE
		ENGINE_cleanup();
#endif
		CRYPTO_cleanup_all_ex_data();

		ERR_remove_thread_state(NULL);
#endif // _SSL_USE_OPENSSL_

		OP_DELETE(m_data);
		m_data = NULL;

#endif // defined(_SSL_USE_OPENSSL_) || defined(USE_OPENSSL_CERTIFICATE_VERIFICATION)
	}

#ifndef HAS_COMPLEX_GLOBALS
#if defined(_SSL_USE_OPENSSL_) || defined(USE_OPENSSL_CERTIFICATE_VERIFICATION)
	DestroyGlobals();
#endif
#endif
}

#endif // OPERA_SMALL_VERSION
