/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_LIBOPEAY_LIBOPEAY_IMPLMODULE_H
#define MODULES_LIBOPEAY_LIBOPEAY_IMPLMODULE_H

#if defined(_SSL_USE_OPENSSL_) || defined(LIBOPEAY_ENABLE_PARTLY_OPENSSL_SUPPORT)

#ifdef OPENSSL_SYS_WIN32
/* Under Win32 thes are defined in wincrypt.h */
#undef PKCS7_ISSUER_AND_SERIAL
#undef PKCS7_SIGNER_INFO
#undef X509_NAME
#undef X509_CERT_PAIR
#endif

#include <openssl/ossl_typ.h>
#include <openssl/safestack.h>
#include <openssl/lhash.h>
#include <openssl/err.h>
#include "modules/libopeay/crypto/rand/rand_lcl.h"

/* MUST match corresponding defintion STATE_SIZE in crypto/rand/md_rand.c */
#define LIBOPEAY_RAND_STATE_SIZE	1023

#ifdef OPERA_SMALL_VERSION

class Libopeay_ImplementModuleData
{
public:
	/** @name Types of some variables below.
	  *
	  * This list is sorted by type name.
	  *
	  * Name format should be the same as for variables below.
	  *
	  * @{
	  */
	typedef unsigned char hmac__m_t[EVP_MAX_MD_SIZE];
	typedef long          md_rand__md_count_t[2];
	typedef unsigned char md_rand__md_t[MD_DIGEST_LENGTH];
	typedef unsigned char md_rand__state_t[LIBOPEAY_RAND_STATE_SIZE+MD_DIGEST_LENGTH];
	/** @} */

	/** @name List of OpenSSL global and static variables.
	  *
	  * This list is sorted by variable name.
	  *
	  * Name format should be: FILENAME__VARNAME, where:
	  *     FILENAME - filename wher the variable is originally defined, without extension.
	  *                Example: obj_dat for obj_dat.c file.
	  *     VARNAME  - variable name as it is originally defined.
	  *                Example: new_nid.
	  * Example of the result: obj_dat__new_nid.
	  *
	  * NB: old variables do not follow this naming rule.
	  * Nevertheless, new variables should follow the rule.
	  *
	  * @{
	  */
#ifndef OPENSSL_NO_ENGINE
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(STACK_OF(ENGINE_CLEANUP_ITEM) *, cleanup_stack);
#endif // OPENSSL_NO_ENGINE
#if defined SSL_USE_WEAK_CIPHERS
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(int,                     DES_check_key);
#endif
#ifndef OPENSSL_NO_ENGINE
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(ENGINE *,                engine_list_head);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(ENGINE *,                engine_list_tail);
#endif // OPENSSL_NO_ENGINE
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(ERR_STATE,               err__fallback);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(const ERR_FNS *,         err__err_fns);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(LHASH_OF(ERR_STRING_DATA) *, err__int_error_hash);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(LHASH_OF(ERR_STATE) *,   err__int_thread_hash);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(int,                     err__int_thread_hash_references);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(int,                     err__int_err_library_number);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(_LHASH *,                ex_data);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(int,                     free_type);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(int,                     have_perfc);
#if defined(_MSC_VER) && defined(_M_X86)
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(int,                     have_tsc);
#endif
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(hmac__m_t,               hmac__m);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(_LHASH *,                int_thread_hash);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(int,                     int_thread_hash_references);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(unsigned int,            md_rand__crypto_lock_rand);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(double,                  md_rand__entropy);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(int,                     md_rand__initialized);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(CRYPTO_THREADID,         md_rand__locking_threadid);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(md_rand__md_t,           md_rand__md);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(md_rand__md_count_t,     md_rand__md_count);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(md_rand__state_t,        md_rand__state);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(int,                     md_rand__state_index);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(int,                     md_rand__state_num);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(STACK_OF(NAME_FUNCS) *,  name_funcs_stack);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(_LHASH *,                names_lh);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(LHASH_OF(ADDED_OBJ) *,   obj_dat__added);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(int,                     obj_dat__new_nid);
#ifdef LIBOPEAY_PKCS12_SUPPORT
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(_STACK *,                pbe_algs);
#endif // LIBOPEAY_PKCS12_SUPPORT
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(volatile int,            stirred_pool);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(STACK_OF(X509_TRUST) *,  trtable);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(const X509_CRL_METHOD *, x_crl__default_crl_method);
	OPENSSL_IMPLEMENT_GLOBAL_OPERA(STACK_OF(X509_PURPOSE)*, xptable);
	/** @} */

#ifdef LIBOPEAY_X509_EX_DATA
	int ev_oid_x509_list_index; // Actually used by libssl, but fits better here
#	define g_ev_oid_x509_list_index			g_opera->libopeay_module.m_data->ev_oid_x509_list_index
#endif

public:
	Libopeay_ImplementModuleData();
	~Libopeay_ImplementModuleData(); 	// Variables are destroyed by internal cleanup calls
};

#endif /* OPERA_SMALL_VERSION */

#endif /* _SSL_USE_OPENSSL_ */

#endif /* !MODULES_LIBOPEAY_LIBOPEAY_MODULE_H */
