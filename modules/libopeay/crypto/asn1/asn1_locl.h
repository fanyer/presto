/* asn1t.h */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#ifndef HEADER_ASN1_LOCL_H
#define HEADER_ASN1_LOCL_H

/* Internal ASN1 structures and functions: not for application use */

/* ASN1 print context structure */

struct asn1_pctx_st
	{
	unsigned long flags;
	unsigned long nm_flags;
	unsigned long cert_flags;
	unsigned long oid_flags;
	unsigned long str_flags;
	} /* ASN1_PCTX */;

/* ASN1 public key method structure */

struct evp_pkey_asn1_method_st
	{
	int pkey_id;
	int pkey_base_id;
	unsigned long pkey_flags;

	char *pem_str;
	char *info;

	int (*pub_decode)(EVP_PKEY *pk, X509_PUBKEY *pub);
	int (*pub_encode)(X509_PUBKEY *pub, const EVP_PKEY *pk);
	int (*pub_cmp)(const EVP_PKEY *a, const EVP_PKEY *b);
	int (*pub_print)(BIO *out, const EVP_PKEY *pkey, int indent,
							ASN1_PCTX *pctx);

	int (*priv_decode)(EVP_PKEY *pk, PKCS8_PRIV_KEY_INFO *p8inf);
	int (*priv_encode)(PKCS8_PRIV_KEY_INFO *p8, const EVP_PKEY *pk);
	int (*priv_print)(BIO *out, const EVP_PKEY *pkey, int indent,
							ASN1_PCTX *pctx);

	int (*pkey_size)(const EVP_PKEY *pk);
	int (*pkey_bits)(const EVP_PKEY *pk);

	int (*param_decode)(EVP_PKEY *pkey,
				const unsigned char **pder, int derlen);
	int (*param_encode)(const EVP_PKEY *pkey, unsigned char **pder);
	int (*param_missing)(const EVP_PKEY *pk);
	int (*param_copy)(EVP_PKEY *to, const EVP_PKEY *from);
	int (*param_cmp)(const EVP_PKEY *a, const EVP_PKEY *b);
	int (*param_print)(BIO *out, const EVP_PKEY *pkey, int indent,
							ASN1_PCTX *pctx);

	void (*pkey_free)(EVP_PKEY *pkey);
	int (*pkey_ctrl)(EVP_PKEY *pkey, int op, long arg1, void *arg2);

	/* Legacy functions for old PEM */

	int (*old_priv_decode)(EVP_PKEY *pkey,
				const unsigned char **pder, int derlen);
	int (*old_priv_encode)(const EVP_PKEY *pkey, unsigned char **pder);

	} /* EVP_PKEY_ASN1_METHOD */;

#define CONST_EVP_PKEY_ASN1_METHOD_ITEM(cname, \
	a_pkey_id, \
	a_pkey_base_id, \
	a_pkey_flags, \
	a_pem_str, \
	a_info, \
	a_pub_decode, \
	a_pub_encode, \
	a_pub_cmp, \
	a_pub_print, \
	a_priv_decode, \
	a_priv_encode, \
	a_priv_print, \
	a_pkey_size, \
	a_pkey_bits, \
	a_param_decode, \
	a_param_encode, \
	a_param_missing, \
	a_param_copy, \
	a_param_cmp, \
	a_param_print, \
	a_pkey_free, \
	a_pkey_ctrl, \
	a_old_priv_decode, \
	a_old_priv_encode \
) \
	OPENSSL_PREFIX_CONST_ITEM(extern, EVP_PKEY_ASN1_METHOD, cname, libopeay) \
		CONST_ITEM_ENTRY_START( pkey_id,         a_pkey_id) \
		CONST_ITEM_ENTRY_SINGLE(pkey_base_id,    a_pkey_base_id) \
		CONST_ITEM_ENTRY_SINGLE(pkey_flags,      a_pkey_flags) \
		CONST_ITEM_ENTRY_SINGLE(pem_str,         a_pem_str) \
		CONST_ITEM_ENTRY_SINGLE(info,            a_info) \
		CONST_ITEM_ENTRY_SINGLE(pub_decode,      a_pub_decode) \
		CONST_ITEM_ENTRY_SINGLE(pub_encode,      a_pub_encode) \
		CONST_ITEM_ENTRY_SINGLE(pub_cmp,         a_pub_cmp) \
		CONST_ITEM_ENTRY_SINGLE(pub_print,       a_pub_print) \
		CONST_ITEM_ENTRY_SINGLE(priv_decode,     a_priv_decode) \
		CONST_ITEM_ENTRY_SINGLE(priv_encode,     a_priv_encode) \
		CONST_ITEM_ENTRY_SINGLE(priv_print,      a_priv_print) \
		CONST_ITEM_ENTRY_SINGLE(pkey_size,       a_pkey_size) \
		CONST_ITEM_ENTRY_SINGLE(pkey_bits,       a_pkey_bits) \
		CONST_ITEM_ENTRY_SINGLE(param_decode,    a_param_decode) \
		CONST_ITEM_ENTRY_SINGLE(param_encode,    a_param_encode) \
		CONST_ITEM_ENTRY_SINGLE(param_missing,   a_param_missing) \
		CONST_ITEM_ENTRY_SINGLE(param_copy,      a_param_copy) \
		CONST_ITEM_ENTRY_SINGLE(param_cmp,       a_param_cmp) \
		CONST_ITEM_ENTRY_SINGLE(param_print,     a_param_print) \
		CONST_ITEM_ENTRY_SINGLE(pkey_free,       a_pkey_free) \
		CONST_ITEM_ENTRY_SINGLE(pkey_ctrl,       a_pkey_ctrl) \
		CONST_ITEM_ENTRY_SINGLE(old_priv_decode, a_old_priv_decode) \
		CONST_ITEM_ENTRY_SINGLE(old_priv_encode, a_old_priv_encode) \
	CONST_ITEM_ENTRY_END

/* Method to handle CRL access.
 * In general a CRL could be very large (several Mb) and can consume large
 * amounts of resources if stored in memory by multiple processes.
 * This method allows general CRL operations to be redirected to more
 * efficient callbacks: for example a CRL entry database.
 */

#define X509_CRL_METHOD_DYNAMIC		1

struct x509_crl_method_st
	{
	int flags;
	int (*crl_init)(X509_CRL *crl);
	int (*crl_free)(X509_CRL *crl);
	int (*crl_lookup)(X509_CRL *crl, X509_REVOKED **ret,
				ASN1_INTEGER *ser, X509_NAME *issuer);
	int (*crl_verify)(X509_CRL *crl, EVP_PKEY *pk);
	};

#define CONST_X509_CRL_METHOD_ITEM(cname, \
	a_flags, \
	a_crl_init, \
	a_crl_free, \
	a_crl_lookup, \
	a_crl_verify \
) \
	OPENSSL_PREFIX_CONST_ITEM(extern, X509_CRL_METHOD, cname, libopeay) \
		CONST_ITEM_ENTRY_START( flags,         a_flags) \
		CONST_ITEM_ENTRY_SINGLE(crl_init,      a_crl_init) \
		CONST_ITEM_ENTRY_SINGLE(crl_free,      a_crl_free) \
		CONST_ITEM_ENTRY_SINGLE(crl_lookup,    a_crl_lookup) \
		CONST_ITEM_ENTRY_SINGLE(crl_verify,    a_crl_verify) \
	CONST_ITEM_ENTRY_END

#endif // !HEADER_ASN1_LOCL_H
