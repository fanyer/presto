/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
*/
/* This file includes code based, in part, on code written by Eric  Young */
/* Copyright (C) 1995-1997 Eric Young (eay@mincom.oz.au)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@mincom.oz.au).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are adheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@mincom.oz.au).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@mincom.oz.au)"
 *    The word 'cryptographic' can be left out if the routines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@mincom.oz.au)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_) && defined(_SSL_USE_OPENSSL_)
#if !defined(ADS12) || defined(ADS12_TEMPLATING) // see end of streamtype.cpp

#include "modules/libssl/sslbase.h"

#include "modules/olddebug/tstdump.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/encodings/utility/opstring-encodings.h"

#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/external/openssl/eayhead.h"
#include "modules/libssl/external/openssl/certhand1.h" 
#include "modules/libssl/external/openssl/cert_store.h"
#include "modules/libssl/external/openssl/opeay_certrepository.h"
#include "modules/formats/uri_escape.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/libssl/methods/sslhash.h"
#include "modules/libssl/methods/sslpubkey.h"

#include "modules/libssl/auto_update_version.h"

#include "modules/libopeay/addon/purposes.h"
#include "modules/libopeay/openssl/rsa.h"
#include "modules/libopeay/openssl/dsa.h"
#include "modules/libopeay/openssl/dh.h"
#include "modules/libopeay/openssl/x509v3.h"
#include "modules/libopeay/openssl/asn1t.h"
#include "modules/libopeay/openssl/pkcs7.h"

extern "C" {
	int ASN1_UTCTIME_unisprint(uni_char *sp, ASN1_UTCTIME *tm_string);
	int CheckX509_EV_dataID();
}

#define X509_O_GET_CERT_BY_SUBJECT 150

#ifndef OPENSSL_NO_MD2
#error "MD2 is no longer supported. re-enable the OPENSSL_NO_MD2"
#endif

// Certificate was retrieved from the repository, not from a server provided certificate chain
#define CH_FROM_REPOSITORY 1
// Warn about certificate?
#define CH_WARN_FLAG 2
// Trust connections to sites with this certificate
#define CH_ALLOW_FLAG 4
// This certificate was preshipped in our repository (or from an external repository)
#define CH_PRESHIPPED 8

int SSL_get_cert_by_subject(X509_LOOKUP *ctx, int type, X509_NAME *name, X509_OBJECT *re);
OP_STATUS Parse_name(X509_NAME *name, OpString_list &target);
OP_STATUS ParseTime(ASN1_UTCTIME *time_val, OpString &target);

SSL_CertificateHandler *SSL_API::CreateCertificateHandler()
{
	return OP_NEW(SSLEAY_CertificateHandler, ());
}

void SSL_Cert_Store::Shutdown()
{
	if(cert_store != NULL)
	{
		X509_STORE_free(cert_store);
		cert_store = NULL;
	}
}

const X509_LOOKUP_METHOD x509_opera_options_lookup =
{
	NULL, // "Load certificates from SSL options"
	NULL,			/* new */
	NULL,			/* free */
	NULL,			/* init */
	NULL,			/* shutdown */
	NULL,			/* ctrl */
	NULL, //SSL_get_cert_by_subject,/* get_by_subject */
	NULL,			/* get_by_issuer_serial */
	NULL,			/* get_by_fingerprint */
	NULL			/* get_by_alias */
};

#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
#ifndef ROOTSTORE_SIGNKEY
# include AUTOUPDATE_PUBKEY_INCLUDE
#endif

int i2d_OBJ_seq(STACK_OF(ASN1_OBJECT) *sk, unsigned char **pp)
{
	return i2d_ASN1_SET_OF_ASN1_OBJECT(sk, pp, i2d_ASN1_OBJECT,
							V_ASN1_SEQUENCE,
						   V_ASN1_UNIVERSAL,
						   IS_SEQUENCE);
}

STACK_OF(ASN1_OBJECT) *d2i_OBJ_seq(STACK_OF(ASN1_OBJECT) **sk, const unsigned char **pp, long len)
{
	return d2i_ASN1_SET_OF_ASN1_OBJECT(sk, pp, len, 
					d2i_ASN1_OBJECT, ASN1_OBJECT_free,
					V_ASN1_SEQUENCE,
					V_ASN1_UNIVERSAL);
}

int OBJ_sort_cmp(const ASN1_OBJECT * const *a, const ASN1_OBJECT * const *b)
{
	if(a == NULL)
	{
		if(b == NULL)
			return 0;
		return -1;
	}
	else if(b== NULL)
		return +1;

	return OBJ_cmp(*a,*b);
}


#define i2d_ASN1_OBJECT_STACK_Vector(source, target) i2d_Vector((int (*)(void *, unsigned char **)) i2d_OBJ_seq, source, target)
#define d2i_ASN1_OBJECT_STACK_Vector(target, source) (STACK_OF(ASN1_OBJECT) *) d2i_Vector((void *(*)(void *, unsigned char **, long)) d2i_OBJ_seq, target, source)

#endif

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
void CalculateIssuerID(X509 *cert, SSL_varvector32 &target, BOOL subject, BOOL use_kid, BOOL always_use_pubkey=FALSE)
{
	target.Resize(0);

	if(cert == NULL)
		return;

	unsigned char *name_buffer = (unsigned char *) g_memory_manager->GetTempBuf2k();
	unsigned char *name_buffer_pos = name_buffer;
	size_t len =0;
	unsigned int md_len = 0;

	OP_ASSERT(g_memory_manager->GetTempBuf2kLen() > EVP_MAX_MD_SIZE + 1 );

	*(name_buffer_pos++) = AUTOUPDATE_VERSION_NUM;
	len ++;

	X509_NAME *dn = (subject ? X509_get_subject_name(cert) : X509_get_issuer_name(cert));
	unsigned char *name=NULL;
	
	int name_len = i2d_X509_NAME(dn, &name);
	if(name == NULL)
		return;
	EVP_MD_CTX hash;
	EVP_DigestInit(&hash, EVP_sha256());
	EVP_DigestUpdate(&hash, name, name_len);
	OPENSSL_free(name);
	if(always_use_pubkey || X509_check_issued(cert, cert)==0)
	{
		ASN1_BIT_STRING *key = X509_get0_pubkey_bitstr(cert);
		if(key)
			EVP_DigestUpdate(&hash, key->data, key->length);
	}
	else if(use_kid && cert->akid && cert->akid->keyid)
	{
		EVP_DigestUpdate(&hash,  cert->akid->keyid->data, cert->akid->keyid->length);
	}

	EVP_DigestFinal(&hash, name_buffer_pos, &md_len);
	len += md_len;

	target.Set(name_buffer, len);
}
#endif

/**
 * Return values are:
 *  1 lookup successful.
 *  0 certificate not found.
 * -1 some other error.
 */
int SSL_get_issuer(X509 **issuer, X509_STORE_CTX *ctx, X509 *x)
{
	if(issuer == NULL || ctx == NULL || x == NULL)
		return -1;
	*issuer = NULL;

	SSLEAY_CertificateHandler *object = (SSLEAY_CertificateHandler *) X509_STORE_CTX_get_app_data(ctx);
#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
	if (!g_ssl_disable_internal_repository || g_ssl_external_repository.Empty())
#endif
	{
		SSL_DistinguishedName name;

		i2d_X509_NAME_Vector(X509_get_issuer_name(x), name);
		if(name.ErrorRaisedFlag)
		{
			X509err(X509_O_GET_CERT_BY_SUBJECT,ERR_R_MALLOC_FAILURE);
			return 0;
		}

		SSL_CertificateItem *cert_item = NULL;
		X509 *cert;

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
		object->pending_issuer_id.Resize(0);
#endif

		SSL_Options *sec_manager = NULL;

		if(object->is_standalone && ctx->check_issued(ctx, x, x))
		{
			return 0;
		}
		if(object->options)
		{
			sec_manager = object->options;
		}
		else

		{
			if(!g_ssl_api->CheckSecurityManager() || OpStatus::IsError(g_securityManager->Init(SSL_LOAD_CA_STORE | SSL_LOAD_INTERMEDIATE_CA_STORE)))
			{
				X509err(X509_O_GET_CERT_BY_SUBJECT,ERR_R_MALLOC_FAILURE);
				return 0;
			}
			sec_manager = g_securityManager;
		}

		while((cert_item = sec_manager->FindTrustedCA(name, cert_item)) != NULL)
		{
			cert = d2i_X509_Vector(NULL, cert_item->certificate);

			if(!cert)
				ERR_clear_error(); // we do not want these errors; assume we are errorless until this

			if(cert)
			{
				if(ctx->check_issued(ctx, x, cert))
				{
					if(g_SSL_X509_cert_flags_index <0)
						g_SSL_X509_cert_flags_index = X509_get_ex_new_index(0, NULL, NULL, NULL, NULL);

					if(g_SSL_X509_cert_flags_index >=0)
					{
						UINTPTR flags = 0;

						flags = CH_FROM_REPOSITORY | 
								(cert_item->WarnIfUsed ? CH_WARN_FLAG : 0) | 
								(cert_item->DenyIfUsed ? 0 : CH_ALLOW_FLAG /* TRUE if allowed */) |
								(cert_item->PreShipped ? CH_PRESHIPPED : 0);

						X509_set_ex_data(cert, g_SSL_X509_cert_flags_index, (void *) flags);
					}

#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
					if(ctx->check_issued(ctx, cert, cert) &&
						cert_item->ev_policies.GetLength() > 0)
					{
						STACK_OF(ASN1_OBJECT) *policies;

						if(cert_item->decoded_ev_policies.GetLength() == 0)
						{
							const unsigned char *data_start = cert_item->ev_policies.GetDirect();
							const unsigned char *data = data_start;
							int len = cert_item->ev_policies.GetLength();

							policies = d2i_OBJ_seq(NULL, &data, len);
							//d2i_ASN1_OBJECT_STACK_Vector(NULL, cert_item->policies);

							if(policies)
							{
								BOOL verify = FALSE;
								OpAutoPtr<SSL_PublicKeyCipher> signature_checker;
								SSL_Hash_Pointer digester(SSL_SHA_256);

								do{
									if(digester.Error())
										break;

									digester->InitHash();

									digester->CalculateHash(data_start, data-data_start);

									if(digester.Error())
										break;

									verify = TRUE;
								}while(FALSE);

								if(verify) do {
									verify = FALSE;

									OP_STATUS op_err = OpStatus::OK;

									signature_checker.reset(g_ssl_api->CreatePublicKeyCipher(SSL_RSA, op_err));

									if(OpStatus::IsError(op_err) || signature_checker.get() == NULL)
										break;

									SSL_varvector32 pubkey_bin_ex;

									pubkey_bin_ex.SetExternal(
#ifdef ROOTSTORE_SIGNKEY
										(unsigned char *) g_rootstore_sign_pubkey
#else
										(unsigned char *) AUTOUPDATE_CERTNAME
#endif
										);
									pubkey_bin_ex.Resize(
#ifdef ROOTSTORE_SIGNKEY
										g_rootstore_sign_pubkey_len
#else
										sizeof(AUTOUPDATE_CERTNAME)
#endif
										);

									signature_checker->LoadAllKeys(pubkey_bin_ex);

									if(signature_checker->Error())
										break;

									verify = TRUE;
								} while(FALSE);

								BOOL correctly_signed =FALSE;

								while(verify && len>0 && !correctly_signed)
								{
									ASN1_BIT_STRING *signature;

									len -= (data-data_start);
									data_start = data;
									signature = NULL;

									signature = d2i_ASN1_BIT_STRING(NULL, &data, len);
									if(!signature)
										break;

									len -= (data-data_start);

									if(signature_checker->VerifyASN1(digester.Ptr(), (const byte *) signature->data, (uint32) signature->length)
										&& !signature_checker->Error() )
										correctly_signed = TRUE;

									ASN1_BIT_STRING_free(signature);

								}
								//#define SSL_EXT_VALIDATION_SIGN_CHECK_NOT_NEEDED
#ifndef SSL_EXT_VALIDATION_SIGN_CHECK_NOT_NEEDED
								if(!correctly_signed) 
								{
									sk_ASN1_OBJECT_pop_free(policies, ASN1_OBJECT_free);
									policies = NULL;
								}
								else
#endif
								{
									i2d_ASN1_OBJECT_STACK_Vector(policies, cert_item->decoded_ev_policies);
								}
							}
						}
						else
						{
							policies = d2i_ASN1_OBJECT_STACK_Vector(NULL, cert_item->decoded_ev_policies);
						}


						if(policies != NULL)
						{
							if(X509_VERIFY_PARAM_set1_policies(ctx->param, policies))
								ctx->param->flags |= X509_V_FLAG_POLICY_CHECK;
							X509_set_ex_data(cert, CheckX509_EV_dataID(), policies);
							//sk_ASN1_OBJECT_pop_free(policies, ASN1_OBJECT_free);
						}
					}
#endif

					*issuer = cert;
						return 1;
				}
				X509_free(cert);
			}
		}

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
		// Can we find the issuer online?
		SSL_varvector24 temp_id;
		BOOL fetch = FALSE;

		CalculateIssuerID(x, temp_id, FALSE, TRUE);
		fetch = sec_manager->GetCanFetchIssuerID(temp_id);
		if(!fetch)
		{
			CalculateIssuerID(x, temp_id, FALSE, FALSE);
			fetch = sec_manager->GetCanFetchIssuerID(temp_id);
		}

		if(fetch)
		{
			object->pending_issuer_id = temp_id;
			object->RaiseAlert(SSL_Warning, SSL_Unknown_CA_RequestingUpdate);
			return -1;
		}
#endif
	}

#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
	SSL_External_CertRepository_Base *base_rep = (SSL_External_CertRepository_Base *) g_ssl_external_repository.First();
	while(base_rep)
	{
		if(base_rep->GetRepositoryType() == SSL_External_CertRepository_Base::OpenSSL)
		{
			OpenSSL_External_Repository *ext_repository = (OpenSSL_External_Repository *) base_rep;

			int ret = ext_repository->LookupCertificate(issuer, ctx, x);
		
			if(ret >0 && *issuer != NULL)
			{
				if(ctx->check_issued(ctx, x, *issuer))
				{
					if(g_SSL_X509_cert_flags_index <0)
						g_SSL_X509_cert_flags_index = X509_get_ex_new_index(0, NULL, NULL, NULL, NULL);

					if(g_SSL_X509_cert_flags_index >=0)
					{
						UINTPTR flags = 0;

						flags = CH_FROM_REPOSITORY |
								CH_ALLOW_FLAG /* Allow */ |
								CH_PRESHIPPED /* preshipped flag */;

						X509_set_ex_data(*issuer, g_SSL_X509_cert_flags_index, (void *) flags);
					}

					return 1;
				}

				OP_ASSERT(0);  // Wrong certificate, should never happen.
				ret = 0;
			}

			X509_free(*issuer);
			*issuer = NULL;

			if(ret <0)
			{
				object->RaiseAlert(SSL_Warning, SSL_Unknown_CA_RequestingExternalUpdate);
				return -1;
			}
		}
		base_rep = (SSL_External_CertRepository_Base *) base_rep->Suc();
	}
#endif // SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY

	return 0;
}

#ifdef LIBSSL_ENABLE_CRL_SUPPORT
int SSL_check_revocation(X509_STORE_CTX *ctx)
{
	SSLEAY_CertificateHandler *object = (SSLEAY_CertificateHandler *) X509_STORE_CTX_get_app_data(ctx);

	OP_ASSERT(object);

	if(object == NULL)
		return 0;

	if(!object->set_crls)
	{
		CRL_item *crl_item = NULL;
		for(int i=0; i< sk_X509_num(ctx->chain);i++)
		{
			X509 *cert = sk_X509_value(ctx->chain, i);
			OP_ASSERT(cert);
			if(!cert)
				continue;

			CRL_DIST_POINTS *crl_list; 

			crl_list = (CRL_DIST_POINTS *) X509_get_ext_d2i(cert,NID_crl_distribution_points, NULL, NULL);
			if(!crl_list)
				continue;

			int k;
			for(k = 0; k < sk_DIST_POINT_num(crl_list); k++) 
			{
				DIST_POINT *info_item = sk_DIST_POINT_value(crl_list,k);
				if(info_item && info_item->distpoint && info_item->distpoint->type == 0)
				{
					int j;
					for(j = 0; j< sk_GENERAL_NAME_num(info_item->distpoint->name.fullname); j++) 
					{
						GENERAL_NAME *name = sk_GENERAL_NAME_value(info_item->distpoint->name.fullname,j);
						if(name && name->type == GEN_URI && 
							name->d.ia5)
						{
							OpString8 crl_uri;
							OP_STATUS op_err = crl_uri.Set((const char *) name->d.ia5->data, name->d.ia5->length);
							if(OpStatus::IsError(op_err))
								continue;

							URL crl_url = g_url_api->GetURL(crl_uri, g_revocation_context);
							if(object->current_crls)
								crl_item = object->current_crls->FindCRLItem(crl_url);

							if(!crl_item && object->current_crls_new)
								crl_item = object->current_crls_new->FindCRLItem(crl_url);

							if(crl_item)
							{
								object->LoadCRL_item(crl_item);
								break;
							}
						}
					}
				}
			}
			CRL_DIST_POINTS_free(crl_list);
		}
		{
			SSL_varvector32 emptyissuer;
			if(object->current_crls)
			{
				crl_item = object->current_crls->First();
				while(crl_item)
				{
					if(crl_item->HaveIssuer(emptyissuer))
						object->LoadCRL_item(crl_item);
					crl_item = crl_item->Suc();
				}
			}

			if(object->current_crls_new)
			{
				crl_item = object->current_crls_new->First();
				while(crl_item)
				{
					if(crl_item->HaveIssuer(emptyissuer))
						object->LoadCRL_item(crl_item);
					crl_item = crl_item->Suc();
				}
			}
		}
		object->set_crls = TRUE;
	}
    OP_ASSERT(object->libopeay_check_revocation);

    // Call libopeay's original algorithm
    return object->libopeay_check_revocation(ctx);
}
#endif

void UpdateX509PointerAndReferences(X509 *&dest, X509 *new_value, BOOL inc)
{
	if(new_value && inc)
		CRYPTO_add(&new_value->references,1,CRYPTO_LOCK_X509);

	if(dest /*&& CRYPTO_add(&dest->references,-1,CRYPTO_LOCK_X509) < 0*/)
		X509_free(dest);

	dest = new_value;
}


SSLEAY_CertificateHandler::SSLEAY_CertificateHandler()
{
	Init();
	CheckError();
}

/* Unref YNP
SSLEAY_CertificateHandler::SSLEAY_CertificateHandler(const SSLEAY_CertificateHandler &old)
{
uint24 i;

  fingerprints.ForwardTo(this);
  certificatecount = old.certificatecount;
  certificate_stack = NULL;
  certificates = NULL;
  endcertificate = NULL;
  topcertificate = NULL;
  certificates = X509_STORE_CTX_new();
  if(certificates == NULL){
  RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
  return;
  }
  if(certificatecount == 0)
  return;
  certificate_stack = new x509_cert_stack_st[certificatecount];
  if(certificate_stack == NULL){
  RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
  return;
  }
  for(i=0;i<certificatecount;i++){
  certificate_stack[i].certificate = duplicate_X509(old.certificate_stack[i].certificate);
  if(certificate_stack[i].certificate == NULL){
  RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
  certificatecount = i;
  return;
  }
  if (i>0)
  X509_add_cert(certificates,certificate_stack[i].certificate);
  }
  fingerprints = old.fingerprints;  
  CheckError();
  }
*/

SSLEAY_CertificateHandler::SSLEAY_CertificateHandler(const SSLEAY_CertificateHandler *old)
{
	Init();
	if (ErrorRaisedFlag)
		return;

	LoadCertificates((STACK_OF(X509) *)old->chain);
	if (!ErrorRaisedFlag)
		certificatecount = old->certificatecount;
	else
		Clear();
}

void SSLEAY_CertificateHandler::Init()
{
	ERR_clear_error();
	if(g_store == NULL)
	{
		g_store = X509_STORE_new();
		if(g_store == NULL)
			RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
		X509_STORE_add_lookup(g_store, (X509_LOOKUP_METHOD*)&x509_opera_options_lookup);
		X509_STORE_set_verify_cb_func(g_store, SSLEAY_CertificateHandler_Verify_callback);
		//X509_STORE_set_flags(g_store, X509_V_FLAG_X509_STRICT);
	}

	ForwardToThis(fingerprints_sha1, fingerprints_sha256);
	ForwardToThis(fingerprintmaker_sha1, fingerprintmaker_sha256);
	fingerprintmaker_sha256.Set(SSL_SHA_256);
	fingerprintmaker_sha1.Set(SSL_SHA);
	certificatecount = 0;
	certificate_stack = NULL;
	endcertificate = NULL;
	topcertificate = NULL;
	firstissuer = NULL;
	sent_ocsp_request = NULL;
	extra_certs_chain = NULL;
	chain = NULL;
	options = NULL;
	is_standalone = FALSE;

#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	set_crls = FALSE;
	current_crls = current_crls_new = NULL;
	libopeay_check_revocation = NULL;
#endif	

	certificates = X509_STORE_CTX_new();
	if(certificates == NULL)
	{
		RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
		return;
	}
	//X509_STORE_CTX_init(certificates, g_store, NULL, NULL);
}

SSLEAY_CertificateHandler::~SSLEAY_CertificateHandler()
{
	Clear();
}

void SSLEAY_CertificateHandler::Clear()
{
	UpdateX509PointerAndReferences(endcertificate, NULL);
	UpdateX509PointerAndReferences(topcertificate, NULL);
	UpdateX509PointerAndReferences(firstissuer, NULL);

	if(certificates != NULL)
	{
	#ifdef LIBSSL_ENABLE_CRL_SUPPORT
		sk_X509_CRL_pop_free(certificates->crls, X509_CRL_free);
		certificates->crls = NULL;
	#endif
		X509_STORE_CTX_free(certificates);
	}
	certificates = NULL;

	sk_X509_pop_free(extra_certs_chain, X509_free);
	extra_certs_chain = NULL;

	if(chain != NULL)
		sk_X509_free(chain);
	chain = NULL;

	if(sent_ocsp_request)
		OCSP_REQUEST_free(sent_ocsp_request);

	OP_DELETEA(certificate_stack);
	certificate_stack = NULL;

	certificatecount = 0;

}

BOOL SSLEAY_CertificateHandler::PrepareCertificateStorage(uint24 count)
{
	Clear();

	certificates = X509_STORE_CTX_new();
	if(certificates == NULL)
	{
		RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
		return FALSE;
	}
	
	certificatecount = count;
	if(certificatecount == 0)
		return TRUE;

	fingerprints_sha1.Resize(certificatecount);
	fingerprints_sha256.Resize(certificatecount);

	if(!ErrorRaisedFlag)
		certificate_stack = OP_NEWA(x509_cert_stack_st, certificatecount+10 /* just to be on the safe side, OpenSSL stops at depth 9*/);               
	if(certificate_stack != NULL)
		chain = sk_X509_new_null();
	else
		certificatecount = 0;
	
	extra_certs_chain = sk_X509_new_null();

	if(ErrorRaisedFlag ||  extra_certs_chain == NULL || certificate_stack == NULL || chain == NULL)
	{
		if(!ErrorRaisedFlag )
		{
			RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
			CheckError();
		}
		Clear();
		return FALSE;
	}

	return TRUE;
}

void SSLEAY_CertificateHandler::FinalizeCertificateStorage()
{
	if(certificatecount > 0)
	{
		endcertificate = certificate_stack[0].certificate;
		CRYPTO_add(&endcertificate->references,1,CRYPTO_LOCK_X509);
	}

	X509_STORE_CTX_init(certificates, g_store, (certificatecount ? certificate_stack[0].certificate : NULL) ,chain);
	certificates->get_issuer = SSL_get_issuer;
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	// Store libopeays original revocation algorithm, to check for unused certificates
	// before calling the original libopeay algorithm.
	libopeay_check_revocation = certificates->check_revocation;

	// Assign our callback to check for unused certificates. SSL_check_revocation will call libopeay_check_revocation when done.
	certificates->check_revocation = SSL_check_revocation;

	certificates->crls = sk_X509_CRL_new_null();
	if(certificates->crls == NULL)
		RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
#endif
}

void SSLEAY_CertificateHandler::LoadCertificate(SSL_ASN1Cert_list &certs)
{
	uint24 i;
	X509 *temp1;

	ERR_clear_error();
	if(!PrepareCertificateStorage(certs.Count()))
		return;

	for(i=0;i<certificatecount;i++)
	{
		temp1 = NULL;
		certificate_stack[i].certificate = NULL;
		fingerprintmaker_sha1->CompleteHash(certs[i],fingerprints_sha1[i]);
		fingerprintmaker_sha256->CompleteHash(certs[i],fingerprints_sha256[i]);
		if(!ErrorRaisedFlag)
			temp1 = d2i_X509_Vector(&certificate_stack[i].certificate, certs[i]);
		if(ErrorRaisedFlag || temp1 == NULL)
		{
			CheckError();
			Clear();
			if(!ErrorRaisedFlag)
				RaiseAlert(SSL_Internal, SSL_InternalError);
			return;
		}
		//CRYPTO_add(&certificate_stack[i].certificate->references,1,CRYPTO_LOCK_X509);
		// Does not have to check return value (result checked later)
		sk_X509_push(chain,certificate_stack[i].certificate);
		//OP_ASSERT(certificate_stack[i].certificate->references >= 2);
	}   
	
	FinalizeCertificateStorage();
	CheckError();
}

void SSLEAY_CertificateHandler::LoadCertificate(SSL_ASN1Cert &cert)
{
	SSL_ASN1Cert_list tempcerts;

	tempcerts = cert;
	LoadCertificate(tempcerts);
}


#ifdef _SUPPORT_SMIME_
void SSLEAY_CertificateHandler::LoadCertificate(X509 *crt)
{
	STACK_OF(X509) temp_certs;

	sk_X509_push(&temp_certs, crt);
	sk_X509_pop(&temp_certs);
}
#endif


void SSLEAY_CertificateHandler::LoadCertificates(STACK_OF(X509) *crts)
{
	ERR_clear_error();
	if(!PrepareCertificateStorage(sk_X509_num(crts)))
		return;

	uint24 i = 0;
	for(uint24 cert_num = 0; cert_num < certificatecount; cert_num ++)
	{
		X509 *crt;
		unsigned int len=0;

		crt = sk_X509_value(crts, cert_num);
		if(crt != NULL)
		{
			fingerprints_sha1[i].Resize(EVP_sha1()->md_size);
			fingerprints_sha256[i].Resize(EVP_sha256()->md_size);
			if(ErrorRaisedFlag)
			{
				Clear();
				return;
			}

			certificate_stack[i].certificate = crt;
			CRYPTO_add(&certificate_stack[i].certificate->references,1,CRYPTO_LOCK_X509);
			X509_digest(crt, EVP_sha1(), fingerprints_sha1[i].GetDirect(),&len);
			len = 0;
			X509_digest(crt, EVP_sha256(), fingerprints_sha256[i].GetDirect(),&len);

			sk_X509_push(chain,crt);

			//CRYPTO_add(&crt->references,1,CRYPTO_LOCK_X509);
			i++;
		}
	}
	certificatecount = i;

	FinalizeCertificateStorage();
	CheckError();
}

void SSLEAY_CertificateHandler::LoadExtraCertificate(SSL_ASN1Cert &crt)
{
	X509 *temp1;
	ERR_clear_error();

	temp1 = d2i_X509_Vector(NULL, crt);
	if(temp1 != NULL)
	{
		// Add the certificate to an extra chain that owns the extra certificate,
		// The certificates will be free'd in Clear();
		if(sk_X509_push(extra_certs_chain,temp1) )
			sk_X509_push(chain, temp1);
		else
			X509_free(temp1); // free on failure
	}

	ERR_clear_error(); // Ignore errors

}

#ifdef _SUPPORT_SMIME_
BOOL SSLEAY_CertificateHandler::SelectCertificate(PKCS7_ISSUER_AND_SERIAL *issuer)
{
	if(issuer == NULL || chain == NULL)
		return FALSE;

	X509 *cert = X509_find_by_issuer_and_serial(chain, issuer->issuer, issuer->serial);

	if(cert)
	{
		UpdateX509PointerAndReferences(endcertificate, cert);

		X509_STORE_CTX_set_cert(certificates, endcertificate);
	}

	return (cert != NULL);
}
#endif

#ifdef LIBSSL_ENABLE_CRL_SUPPORT
BOOL SSLEAY_CertificateHandler::CollectCRLs(X509 *x, CRL_List *precollected_crls, CRL_List *collect_crls, BOOL &CRL_distribution)
{
	// NID_crl_distribution_points
	// NID_netscape_revocation_url
	// NID_netscape_ca_revocation_url

	OP_ASSERT(precollected_crls != NULL);
	OP_ASSERT(collect_crls != NULL);

	if(!precollected_crls || !collect_crls)
		return FALSE;

	SSL_varvector32 issuer;
	i2d_X509_NAME_Vector(X509_get_issuer_name(x), issuer);

	OpAutoPtr<CRL_item> new_item(OP_NEW(CRL_item, ()));
	if(new_item.get() == NULL)
		return FALSE;

	CRL_DIST_POINTS *crl_list; 
	BOOL have_crl = FALSE;

	crl_list = (CRL_DIST_POINTS *) X509_get_ext_d2i(x,NID_crl_distribution_points, NULL, NULL);
	if(crl_list && X509_check_issued(x,x) == 0)
	{
		CRL_DIST_POINTS_free(crl_list);
		return TRUE;
	}

	if(crl_list)
	{
		CRL_distribution = TRUE;

		int i;
		for(i = 0; i < sk_DIST_POINT_num(crl_list); i++) 
		{
			DIST_POINT *info_item = sk_DIST_POINT_value(crl_list,i);
			if(info_item && info_item->distpoint && info_item->distpoint->type == 0)
			{
				int j;
				for(j = 0; j< sk_GENERAL_NAME_num(info_item->distpoint->name.fullname); j++) 
				{
					GENERAL_NAME *name = sk_GENERAL_NAME_value(info_item->distpoint->name.fullname,j);
					if(name && name->type == GEN_URI && 
						name->d.ia5)
					{
						OpString8 crl_uri;
						OP_STATUS op_err = crl_uri.Set((const char *) name->d.ia5->data, name->d.ia5->length);
						if(OpStatus::IsError(op_err))
							continue;

						URL crl_url = g_url_api->GetURL(crl_uri, g_revocation_context);

						if(new_item->AddURL(crl_url, issuer, precollected_crls, collect_crls))
							have_crl = TRUE;
					}
				}
			}
		}
		CRL_DIST_POINTS_free(crl_list);
	}

	unsigned char *name_buffer = (unsigned char *) g_memory_manager->GetTempBuf2k();
	unsigned int md_len = 0;

	OP_ASSERT(g_memory_manager->GetTempBuf2kLen() > EVP_MAX_MD_SIZE + 1 );

	unsigned char *name=NULL;
	
	int name_len = i2d_X509_NAME(X509_get_issuer_name(x), &name);
	if(name != NULL)
	{
		EVP_MD_CTX hash;
		EVP_DigestInit(&hash, EVP_sha256());
		EVP_DigestUpdate(&hash, name, name_len);
		OPENSSL_free(name);
		EVP_DigestFinal(&hash, name_buffer, &md_len);
		SSL_varvector32 empty_issuer;

		SSL_CRL_Override_Item *crl_overideitem = (SSL_CRL_Override_Item *) g_securityManager->crl_override_list.First();
		while(crl_overideitem)
		{
			if(crl_overideitem->cert_id.GetDirect() && crl_overideitem->cert_id.GetLength() == md_len &&
				op_memcmp(crl_overideitem->cert_id.GetDirect(), name_buffer, md_len) == 0)
			{
				URL crl_url = g_url_api->GetURL(crl_overideitem->crl_url, g_revocation_context);

				if(!crl_url.IsEmpty() && !new_item->HaveURL(crl_url))
				{
					OpAutoPtr<CRL_item> new_item2(OP_NEW(CRL_item, ()));

					if(new_item2.get())
					{
						new_item2->AddURL(crl_url, empty_issuer, precollected_crls, collect_crls, TRUE);
						if(!new_item2->crl_url.IsEmpty())
						{
							new_item2.release()->Into(collect_crls);
							have_crl = TRUE;
						}
					}
				}
				break; // only one in a list
			}
			crl_overideitem = (SSL_CRL_Override_Item *) crl_overideitem->Suc();
		}
	}

	if(!new_item->crl_url.IsEmpty())
	{
		new_item.release()->Into(collect_crls);
		return TRUE;
	}

	
	return have_crl;
}

/*
BOOL SSLEAY_CertificateHandler::CollectCRLsOfChain(X509 *x, CRL_List *precollected_crls, CRL_List *collect_crls)
{
	BOOL ret = TRUE;
	if(!CollectCRLs(x, precollected_crls, collect_crls) && X509_check_issued(x,x) != 0)
	{
		ret = FALSE;
	}

	X509 *issuer;

	while(X509_check_issued(x,x) != 0)
	{
		if(SSL_get_issuer(&issuer, certificates, x) > 0)
		{
			if(!CollectCRLs(issuer, precollected_crls, collect_crls))
			{
				ret = FALSE;
			}
		}
		x = issuer;
	}
	return ret;
}
*/


BOOL SSLEAY_CertificateHandler::CollectCRLs(CRL_List *precollected_crls, CRL_List *collect_crls, BOOL &CRL_distribution)
{
	OpString_list ocsp_url_strings;

	CRL_distribution = FALSE;
	if(endcertificate == NULL)
		return TRUE;

	BOOL ret = TRUE;

	uint24 i;
	for(i=1; i<certificatecount; i++)
	{
		// get the rest of the chain, if possible from the store
		if(!CollectCRLs(certificate_stack[i].certificate, precollected_crls, collect_crls, CRL_distribution))
			ret = FALSE;
	}

	if(!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseOCSPValidation) || 
		!OpStatus::IsSuccess(Get_AIA_URL_Information(0, ocsp_url_strings, NID_ad_OCSP)) || 
		ocsp_url_strings.Count() == 0/* || 
		!(precollected_crls->Empty() && collect_crls->Empty())*/)
	{
		// Only look for CRLs on end certficate if there is no OCSP verification check and it is not selfsigned.
		
		if(X509_check_issued(endcertificate,endcertificate) != 0 && !CollectCRLs(endcertificate, precollected_crls, collect_crls, CRL_distribution))
		{
			// TODO : register missing CRL and missing OCSP
		}
		else
			certificate_stack[0].have_crl = TRUE;
	}
	certificate_stack[0].have_ocsp = (ocsp_url_strings.Count() != 0);

	return ret;
}

BOOL SSLEAY_CertificateHandler::LoadCRLs(CRL_List *collect_crls)
{
	if(collect_crls == NULL)
		return TRUE;

	BOOL ret = TRUE;
	CRL_item *item = collect_crls->First();
	while(item)
	{
		if(!LoadCRL_item(item))
			ret = FALSE;

		item = item->Suc();
	}
	ERR_clear_error();

	return ret;
}

BOOL SSLEAY_CertificateHandler::LoadCRL_item(CRL_item *item)
{
	if(item == NULL)
		return TRUE;

	BOOL ret = TRUE;
	if(OpStatus::IsSuccess(item->DecodeData()))
	{
		X509_CRL *crl_cand = d2i_X509_CRL_Vector(NULL, item->crl_data);
		ERR_clear_error(); // Assume that we are error free this far, and ignore CRL decoding related errors; the NULL pointer is sufficient information of failure.

		if(crl_cand)
		{
#if !(defined LIBOPEAY_DISABLE_MD5_PARTIAL && defined LIBOPEAY_DISABLE_SHA1_PARTIAL)
			int sig_alg = OBJ_obj2nid(crl_cand->sig_alg->algorithm);
#ifndef LIBOPEAY_DISABLE_MD5_PARTIAL
			if(g_SSL_warn_md5 && (sig_alg == NID_md2 ||
				sig_alg == NID_md2WithRSAEncryption ||
				sig_alg == NID_md5 ||
				sig_alg == NID_md5WithRSA ||
				sig_alg == NID_md5WithRSAEncryption))
				certificate_stack[0].used_md5 = TRUE;
#endif // LIBOPEAY_DISABLE_MD5_PARTIAL
#if !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
			if(g_SSL_warn_sha1 && (sig_alg == NID_sha1 ||
				sig_alg == NID_sha1WithRSAEncryption ||
				sig_alg == NID_dsaWithSHA1_2 ||
				sig_alg == NID_sha1WithRSA ||
				sig_alg == NID_dsaWithSHA1))
				certificate_stack[0].used_sha1 = TRUE;
#endif // !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
#endif // !(defined LIBOPEAY_DISABLE_MD5_PARTIAL && defined LIBOPEAY_DISABLE_SHA1_PARTIAL)

			time_t loaded_date= 0;
			item->crl_url.GetAttribute(URL::KVLocalTimeLoaded, &loaded_date);
			if((X509_CRL_get_lastUpdate(crl_cand) && X509_cmp_time(X509_CRL_get_lastUpdate(crl_cand), NULL) >= 0)  || // invalid or not yet valid
				(X509_CRL_get_nextUpdate(crl_cand) && X509_cmp_time(X509_CRL_get_nextUpdate(crl_cand), NULL) <= 0) || // invalid or not yet valid
				loaded_date < (time_t) (g_op_time_info->GetTimeUTC()/1000.0-3600*96) // Max 4 days old
				)
			{
				// need to refetch
				X509_CRL_free(crl_cand);
				crl_cand = NULL;
				item->retrieved = FALSE;
			}
			else if(!sk_X509_CRL_push(certificates->crls, crl_cand))
			{
				X509_CRL_free(crl_cand);
				crl_cand = NULL;
				item->retrieved = FALSE;
			}
			else
				item->retrieved = TRUE;
		}
		
		if(crl_cand == NULL)
		{
			item->crl_data.Resize(0);
			item->loaded = FALSE;
		}
		else 
			item->retrieved = TRUE;
	}

	if(!item->loaded && item->crl_url.GetAttribute(URL::KLoadStatus) != URL_LOADING)
	{
		item->crl_url.Unload(); // Unload to avoid some race conditions.
		ret = FALSE; // Need to load some CRLs, at least
	}

	return ret;
}
#endif // LIBSSL_ENABLE_CRL_SUPPORT

BOOL SSLEAY_CertificateHandler::VerifySignatures(SSL_CertificatePurpose purpose, SSL_Alert *msg,
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
												CRL_List *crl_list_enabled,
#endif // LIBSSL_ENABLE_CRL_SUPPORT
												SSL_Options *opt, // Non-null means retrieve certs from here
												BOOL standalone // TRUE means that selfsigned need not be from reposiroty to be OK
												 )
{
	uint24 i;

	validated_certificates.Resize(0);
	
	if(endcertificate == NULL){
		RaiseAlert(SSL_Internal, SSL_No_Certificate,msg);
		return FALSE;
		//return !Error(msg);
	}
	
	ERR_clear_error();
	if(certificatecount == 0 || endcertificate == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_No_Certificate,msg);
		return FALSE;
	}

	UpdateX509PointerAndReferences(firstissuer, NULL);
	UpdateX509PointerAndReferences(topcertificate, NULL);
	for(i=0;i<certificatecount+10;i++)
	{
		certificate_stack[i].ResetEntry();
		if(i>=certificatecount)
			UpdateX509PointerAndReferences(certificate_stack[i].certificate, NULL);
	}
	options = opt;
	is_standalone = standalone;
	/* disable this check becuse some sites uses illegal certificates
	uint16 i;
	for(i=0;i<certificatecount-1;i++)
	{
		if(X509_name_cmp(X509_get_issuer_name(certificate_stack[i].certificate),
			X509_get_subject_name(certificate_stack[i+1].certificate)) != 0){
			certificate_stack[i].invalid = TRUE;
			RaiseAlert(SSL_Internal, SSL_Invalid_Certificate_Chain,msg);
			return FALSE;
			//return !Error(msg);
		}
	} 
	*/

	X509_STORE_CTX_set_app_data(certificates, (void *) this);
	int purpose1 = X509_PURPOSE_ANY;
	switch(purpose)
	{
	case SSL_Purpose_Client_Certificate:
		purpose1 = X509_PURPOSE_SSL_CLIENT;
		break;
	case SSL_Purpose_Server_Certificate:
		purpose1 = X509_PURPOSE_SSL_SERVER;
		break;
	case SSL_Purpose_Object_Signing:
		purpose1 = OPERA_X509_PURPOSE_CODE_SIGN;
		break;
	}
	X509_STORE_CTX_set_purpose(certificates, purpose1);

#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	CRL_List new_crl_list;

	if(crl_list_enabled != NULL)
	{
		BOOL CRL_distribution = FALSE;
		if(!CollectCRLs(crl_list_enabled, &new_crl_list, CRL_distribution))
		{
			CheckError();
			if(Error(msg))
				return FALSE;
		}

		if(CRL_distribution || !crl_list_enabled->Empty() || !new_crl_list.Empty())
			certificates->param->flags |= (X509_V_FLAG_CRL_CHECK_ALL | X509_V_FLAG_CRL_CHECK);
	}
	set_crls = FALSE;
	current_crls = crl_list_enabled;
	current_crls_new = &new_crl_list;
	/* Reset CRL list so that only relevant CRLs are used */
	sk_X509_CRL_pop_free(certificates->crls, X509_CRL_free);
	certificates->crls = sk_X509_CRL_new_null();
#endif // LIBSSL_ENABLE_CRL_SUPPORT

	sk_X509_pop_free(certificates->chain,X509_free);
	certificates->chain = NULL;

	int verify_status = X509_verify_cert(certificates);

#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	set_crls = FALSE;
	current_crls = current_crls_new = NULL;
#endif // LIBSSL_ENABLE_CRL_SUPPORT

	if(verify_status <= 0)
	{
		CheckError();
		return !Error(msg);
	}

#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	if(crl_list_enabled != NULL)
	{
		BOOL found_no_crl = !(crl_list_enabled->Empty() && new_crl_list.Empty()) || (certificates->param->flags & X509_V_FLAG_CRL_CHECK_ALL) == 0;
		if(!found_no_crl)
		{
			for(i=certificatecount; i< certificatecount+10; i++)
			{
				if(certificate_stack[i].visited && certificate_stack[i].no_crl)
				{
					found_no_crl = TRUE;
					break;
				}
			}
		}
		if(found_no_crl)
		{
			SSL_Alert msg2;
			Error(&msg2);
			ResetError();

			
			for(i=certificatecount; i< certificatecount+10; i++)
			{
				if(certificate_stack[i].visited)
				{
					BOOL CRL_distribution = FALSE;
					if(!CollectCRLs(certificate_stack[i].certificate, crl_list_enabled, &new_crl_list, CRL_distribution))
					{
						CheckError();
						if(Error(msg))
							return FALSE;
					}
					LoadCRLs(crl_list_enabled);
					LoadCRLs(&new_crl_list);
				}
			}

			if(!new_crl_list.Empty())
			{
				crl_list_enabled->Append(&new_crl_list);
				RaiseAlert(SSL_Warning, SSL_Unloaded_CRLs_Or_OCSP);
				CheckError();
				Error(msg);
				return FALSE;
			}

			if(msg2.GetLevel() != SSL_NoError)
				RaiseAlert(msg2);
		}
	}
#endif // LIBSSL_ENABLE_CRL_SUPPORT

#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	X509_POLICY_TREE  *policy_tree = X509_STORE_CTX_get0_policy_tree(certificates);

	BOOL need_CRL = TRUE;
	if(policy_tree && certificate_stack[0].have_ocsp)
	{
		for (i = 0; i <certificatecount+10; i++)
		{
			if(!certificate_stack[i].visited)
				continue;

			if(certificate_stack[i].actual_index == 1 &&
				X509_check_issued(certificate_stack[i].certificate,certificate_stack[i].certificate) == 0)
			{
				need_CRL = FALSE;
				break;
			}
		}
	}

	if(policy_tree && need_CRL && (!crl_list_enabled || (crl_list_enabled->Empty() && new_crl_list.Empty()) ||
		(!certificate_stack[0].have_ocsp && !certificate_stack[0].have_crl)))
		policy_tree = NULL; // Do not check EV-oids for certificates that have no OCSP or CRL validation

	if(policy_tree)
	{
		for (i = 0; i <certificatecount+10; i++)
		{
			if(!certificate_stack[i].visited)
				continue;

			if(certificate_stack[i].invalid_crl || certificate_stack[i].no_crl)
				policy_tree = NULL; // Do not check EV-oids for certificates that failed CRL validation
		}
	}

	if(policy_tree && topcertificate)
	{
		int levels = X509_policy_tree_level_count(policy_tree);
		X509_POLICY_LEVEL *level = X509_policy_tree_get0_level(policy_tree, levels-1);
		STACK_OF(ASN1_OBJECT) *policies = (STACK_OF(ASN1_OBJECT) *)X509_get_ex_data(topcertificate, CheckX509_EV_dataID());

		if(level && policies)
		{
			sk_ASN1_OBJECT_set_cmp_func(policies,  OBJ_sort_cmp);

			int count = X509_policy_level_node_count(level);
			//ASN1_OBJECT *policy_id = OBJ_txt2obj("2.16.840.1.113733.1.7.23.6", TRUE);

			for(int i = 0; i< count; i++)
			{
				X509_POLICY_NODE *node = X509_policy_level_get0_node(level, i);
				if(node == NULL)
					continue;

				const ASN1_OBJECT *policy = X509_policy_node_get0_policy(node);
			
				//if(policy && OBJ_cmp(policy_id, policy) == 0)
				if(policy && sk_ASN1_OBJECT_find(policies, policy) >= 0)
				{
					certificate_stack[0].extended_validation = TRUE;
										
					//OP_ASSERT(0);
				}
			}

			//ASN1_OBJECT_free(policy_id);

			//OP_ASSERT(0);
		}
	}
#endif // SSL_CHECK_EXT_VALIDATION_POLICY

	CheckError();
	Error(msg);
	return TRUE;
}

int SSLEAY_CertificateHandler_Verify_callback(int OK_status,X509_STORE_CTX *ctx)
{
	return SSLEAY_CertificateHandler::Verify_callback(OK_status,ctx);
}

int SSLEAY_CertificateHandler::Verify_callback(int OK_status_arg, X509_STORE_CTX *ctx)
{
	int OP_MEMORY_VAR OK_status = OK_status_arg;
	OP_MEMORY_VAR uint24 depth;
	int reason;
	SSLEAY_CertificateHandler *object;
	X509 *current_cert;

	depth = X509_STORE_CTX_get_error_depth(ctx);
	object = (SSLEAY_CertificateHandler *) X509_STORE_CTX_get_app_data(ctx);
    reason = X509_STORE_CTX_get_error(ctx);
	current_cert = X509_STORE_CTX_get_current_cert(ctx);
                                      
	if(object == NULL)
		return OK_status;

	if(depth+1 >= (uint24) sk_X509_num(ctx->chain))
		UpdateX509PointerAndReferences(object->topcertificate, current_cert, TRUE);

	if(depth >= object->certificatecount+10)
		depth = object->certificatecount +9;

	OP_MEMORY_VAR uint24 actual_depth;
	
	for(actual_depth = 0; actual_depth< object->certificatecount; actual_depth ++)
	{
		if(X509_cmp(object->certificate_stack[actual_depth].certificate, current_cert) == 0)
			break;
	}

	if(actual_depth >= object->certificatecount)
	{
		for(; actual_depth< object->certificatecount+10 && object->certificate_stack[actual_depth].visited; actual_depth ++)
		{
			if(X509_cmp(object->certificate_stack[actual_depth].certificate, current_cert) == 0)
				break;
		}

	}
	
	if(depth == 1 && object->firstissuer != current_cert)
	{
		UpdateX509PointerAndReferences(object->firstissuer, current_cert, TRUE);
	}

	if(actual_depth< object->certificatecount+10 && !object->certificate_stack[actual_depth].visited)
	{
		OP_STATUS op_err;
		OpString_list temp_list;
		int i;

		uint24 previous_size = object->validated_certificates.Count();
		uint24 item;

		if(previous_size < depth +1)
			object->validated_certificates.Resize(depth+1);
		i2d_X509_Vector(current_cert, object->validated_certificates[depth]);

		for(item = previous_size; item < depth;item++)
		{
			X509 *certitem = sk_X509_value(ctx->chain, item);
			if(certitem)
				i2d_X509_Vector(certitem, object->validated_certificates[item]);
		}

		object->certificate_stack[actual_depth].visited = TRUE;
		object->certificate_stack[actual_depth].actual_index = depth;
		if(!object->certificate_stack[actual_depth].certificate)
		{
			object->certificate_stack[actual_depth].certificate = current_cert;
			CRYPTO_add(&current_cert->references,1,CRYPTO_LOCK_X509);
		}

		op_err = Parse_name(X509_get_issuer_name(current_cert),temp_list);
		if(OpStatus::IsError(op_err))
			object->RaiseAlert(op_err);
		i=0;
		if(temp_list[0].IsEmpty())
		{
			i = (temp_list[1].IsEmpty() ? 2 : 1);

		}
		op_err = object->certificate_stack[actual_depth].issuer_name.Set(temp_list[i]);
		if(OpStatus::IsError(op_err))
			object->RaiseAlert(op_err);
		op_err = Parse_name(X509_get_subject_name(current_cert),temp_list);
		if(OpStatus::IsError(op_err))
			object->RaiseAlert(op_err);
		i=0;
		if(temp_list[0].IsEmpty())
		{
			i = (temp_list[1].IsEmpty() ? 2 : 1);

		}
		op_err = object->certificate_stack[actual_depth].subject_name.Set(temp_list[i]);
		if(OpStatus::IsError(op_err))
			object->RaiseAlert(op_err);
		op_err = ParseTime(current_cert->cert_info->validity->notBefore, object->certificate_stack[actual_depth].valid_from_date);
		if(OpStatus::IsError(op_err))
			object->RaiseAlert(op_err);
		op_err = ParseTime(current_cert->cert_info->validity->notAfter, object->certificate_stack[actual_depth].expiration_date);
		if(OpStatus::IsError(op_err))
			object->RaiseAlert(op_err);

#if !(defined LIBOPEAY_DISABLE_MD5_PARTIAL && defined LIBOPEAY_DISABLE_SHA1_PARTIAL)
		int sig_alg = OBJ_obj2nid(current_cert->sig_alg->algorithm);
#ifndef LIBOPEAY_DISABLE_MD5_COMPLETELY
		if(g_SSL_warn_md5 && (sig_alg == NID_md5 ||
			sig_alg == NID_md5WithRSA ||
			sig_alg == NID_md5WithRSAEncryption))
			object->certificate_stack[actual_depth].used_md5 = TRUE;
#endif // LIBOPEAY_DISABLE_MD5_COMPLETELY
#if !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
		if(g_SSL_warn_sha1 && (sig_alg == NID_sha1 ||
			sig_alg == NID_sha1WithRSAEncryption ||
			sig_alg == NID_dsaWithSHA1_2 ||
			sig_alg == NID_sha1WithRSA ||
			sig_alg == NID_dsaWithSHA1))
			object->certificate_stack[actual_depth].used_sha1 = TRUE;
#endif // !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
#endif // !(defined LIBOPEAY_DISABLE_MD5_PARTIAL && defined LIBOPEAY_DISABLE_SHA1_PARTIAL)

		if(g_SSL_X509_cert_flags_index >=0)
		{
			UINTPTR flags = (UINTPTR) X509_get_ex_data(current_cert, g_SSL_X509_cert_flags_index);

			if((flags & CH_FROM_REPOSITORY) != 0)
			{
				object->certificate_stack[actual_depth].from_store = TRUE;
				object->certificate_stack[actual_depth].warn_about_cert = ((flags & CH_WARN_FLAG) != 0) ? TRUE : FALSE;
				object->certificate_stack[actual_depth].allow_access = ((flags & CH_ALLOW_FLAG) != 0) ? TRUE : FALSE;
				object->certificate_stack[actual_depth].preshipped_cert = ((flags & CH_PRESHIPPED) != 0) ? TRUE : FALSE;
			}
		}
	}

	switch(reason){
	case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_OK : 
		return OK_status;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT :
	case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
		if(object->is_standalone)
			return 1;
    //case X509_V_ROOT_OK :
		//return object->HandleTopCertificateAuthority(X509_STORE_CTX_get_current_cert(ctx));
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT :
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
	case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
	case X509_V_ERR_CERT_CHAIN_TOO_LONG:
	case X509_V_ERR_PATH_LENGTH_EXCEEDED:
		object->certificate_stack[actual_depth].no_issuer = TRUE;
		return TRUE;
    case X509_V_ERR_UNABLE_TO_DECRYPT_CERT_SIGNATURE :
    case X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY :
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD  :
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD  :
		object->certificate_stack[actual_depth].invalid = TRUE;
		object->RaiseAlert(SSL_Fatal,SSL_Bad_Certificate);
		return 0;
	case X509_V_ERR_INVALID_CA:
		object->certificate_stack[actual_depth].incorrect_caflags = TRUE;
		return TRUE;
    case X509_V_ERR_CERT_SIGNATURE_FAILURE :
		object->certificate_stack[actual_depth].invalid = TRUE;
		object->RaiseAlert(SSL_Fatal,SSL_Certificate_Verify_Error);
		return 0;
    case X509_V_ERR_CERT_NOT_YET_VALID :
		object->certificate_stack[actual_depth].expired = TRUE;
		object->certificate_stack[actual_depth].not_yet_valid = TRUE;
		object->RaiseAlert(SSL_Warning,SSL_Certificate_Not_Yet_Valid);
		return 1;
	case X509_V_ERR_INVALID_PURPOSE:
		object->certificate_stack[actual_depth].invalid = TRUE;
		object->RaiseAlert(SSL_Fatal,SSL_Certificate_Purpose_Invalid);
		return 0;
	case X509_V_ERR_CERT_REJECTED:
		object->certificate_stack[actual_depth].invalid = TRUE;
		object->RaiseAlert(SSL_Fatal,SSL_Certificate_Verify_Error);
		return 0;
	case X509_V_ERR_CERT_HAS_EXPIRED :
		object->certificate_stack[actual_depth].expired = TRUE;
		object->RaiseAlert(SSL_Fatal,SSL_Certificate_Expired);
		return 1;
		/*  case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT :
		object->RaiseAlert(SSL_Fatal,SSL_Certificate_Unknown);
		return 0;        */
    case X509_V_ERR_OUT_OF_MEM :
		object->RaiseAlert(SSL_Internal,SSL_Allocation_Failure);
		return 0;
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	case X509_V_ERR_CRL_NOT_YET_VALID:
	case X509_V_ERR_CRL_HAS_EXPIRED:
		object->certificate_stack[actual_depth].invalid_crl = TRUE;
		return 1;
	case X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD:
	case X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD:
	case X509_V_ERR_KEYUSAGE_NO_CRL_SIGN:
	case X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION:
	case X509_V_ERR_CRL_SIGNATURE_FAILURE:
		object->certificate_stack[actual_depth].invalid_crl = TRUE;
		object->RaiseAlert(SSL_Fatal,SSL_Certificate_CRL_Verify_failed);
		return 0;
	case X509_V_ERR_UNABLE_TO_GET_CRL:
		if((actual_depth != 0 || !object->certificate_stack[0].have_ocsp) && X509_check_issued(current_cert, current_cert) != 0)
			object->certificate_stack[actual_depth].no_crl = TRUE;
		else
			ctx->error = X509_V_OK;
		return 1;
	case X509_V_ERR_CERT_REVOKED:
		object->RaiseAlert(SSL_Fatal,SSL_Certificate_Revoked);
		{
			SSL_varvector16 fingerprint;
			object->fingerprintmaker_sha256->CompleteHash(object->validated_certificates[depth],fingerprint);
			OpStatus::Ignore(g_revoked_certificates.AddRevoked(fingerprint, object->validated_certificates[depth]));
		}

		return 0;
#endif // LIBSSL_ENABLE_CRL_SUPPORT
	}
	object->RaiseAlert(SSL_Internal,SSL_Illegal_Parameter);
	return 0;
}

/*
BOOL SSLEAY_CertificateHandler::CheckIssuedBy(uint24 item, SSL_ASN1Cert &trustedtopCA,SSL_Alert *msg)
{
	if(!certificate_stack || item >= certificatecount)
		return SSL_NotExpired;
	X509 *cert = certificate_stack[item].certificate;
	if(!cert)
		return FALSE;

	X509 *trusted = d2i_X509_Vector(NULL, trustedtopCA);
	if(!trusted)
	{
		RaiseAlert(SSL_Internal,SSL_InternalError,msg);
		CheckError();
		return FALSE;
	}

	BOOL ret = FALSE;

	if(X509_check_issued(topcertificate, trusted) == X509_V_OK)
	{
		ret = TRUE;
	}

	X509_free(trusted);
	
	CheckError();
	return ret;
}
*/

SSL_ExpirationType SSLEAY_CertificateHandler::CheckIsExpired(uint24 item, time_t spec_date)
{
	if(!certificate_stack || item >= certificatecount)
		return SSL_NotExpired;
	X509 *cert = certificate_stack[item].certificate;
	if(!cert)
		return SSL_NotExpired;

	time_t *spec_time = (spec_date ? &spec_date : NULL);

	if(X509_cmp_time(X509_get_notBefore(cert), spec_time)> 0)
		return SSL_NotYetValid;
	if(X509_cmp_time(X509_get_notAfter(cert), spec_time)< 0)
		return SSL_Expired;

	return SSL_NotExpired;
}

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
/** This implementation ignores timezone, Zulu timezone is assumed */
int cmp_time_string(ASN1_TIME *ctm, const char *cmp_time)
{
	const char *str;
	ASN1_TIME atm;
	char buff1[24],buff2[24],*p; /* ARRAY OK 2008-02-16 yngve */
	int i,j;

	if(cmp_time == NULL || !*cmp_time || ctm->data == NULL|| !*ctm->data)
		return 0;
	atm.data = (unsigned char *) cmp_time;
	atm.length = op_strlen(cmp_time);
	atm.flags = 0;
	atm.type = V_ASN1_GENERALIZEDTIME;

	p=buff2;
	str = (const char *) atm.data;
	i = atm.length;
	if(ASN1_GENERALIZEDTIME_check(&atm))
	{
		if (i < 13) return 0;
		//General time format
		if (ctm->type == V_ASN1_UTCTIME)
		{
			// Different types
			if(str[0] >= '2')
			{
				if(ctm->data[0] >='5' || str[1]>'0')
					return -1; //Either ctm is year 1950-1999 and cmptime >=2000, or cmptime >= year 2100
				if(str[2] >='5')
					return -1; // cmptime is at least year 2050, ctm cannot go past 2049
			}
			else
			{
				if(ctm->data[0]<'5')
					return 1; // cmptime is year 19XX, ctm is 20XX
				if(str[2] < '5')
					return 0; // Illegal year
			}
			str+=2; // Skip century
			op_memcpy(p,str,10);
			p+=10;
			str+=10;
		}
		else
		{
			op_memcpy(p,str,12);
			p+=12;
			str+=12;
		}
	}
	else
	{
		if ((i < 11) || (i > 17)) return 0;
		// old UTC time format
		if(ctm->type == V_ASN1_GENERALIZEDTIME)
		{
			// Conflicting time formats
			if(ctm->data[0] >= '2')
			{
				if(str[0] >='5' || ctm->data[1]>'0')
					return 1; //Either cmptime is year 1950-1999 and ctm >=2000, or ctm >= year 2100
				if(ctm->data[2] >='5')
					return 1; // ctm is at least year 2050, cmptime cannot go past 2049
			}
			else
			{
				if(str[0]<'5')
					return -1; // cmptime is year 20XX, ctm is 19XX
				if(ctm->data[2] < '5')
					return 0; // Illegal year
			}
			if(str[0] >= '5')
			{
				*(p++) = '1';
				*(p++) = '9';
			}
			else
			{
				*(p++) = '2';
				*(p++) = '0';
			}
		}

		op_memcpy(p,str,10);
		p+=10;
		str+=10;
	}
	*(p++)='\0';
	// We ignore timezone.

	p=buff1;
	i=ctm->length;
	str=(char *)ctm->data;
	if (ctm->type == V_ASN1_UTCTIME)
		{
		if ((i < 11) || (i > 17)) return 0;
		op_memcpy(p,str,10);
		p+=10;
		str+=10;
		}
	else
		{
		if (i < 13) return 0;
		op_memcpy(p,str,12);
		p+=12;
		str+=12;
		}
	// We ignore timezone
	*(p++)='\0';

	if (ctm->type == V_ASN1_UTCTIME)
		{
		i=(buff1[0]-'0')*10+(buff1[1]-'0');
		if (i < 50) i+=100; /* cf. RFC 2459 */
		j=(buff2[0]-'0')*10+(buff2[1]-'0');
		if (j < 50) j+=100;

		if (i < j) return -1;
		if (i > j) return 1;
		}
	return op_strcmp(buff1,buff2);
}

SSL_ExpirationType SSLEAY_CertificateHandler::CheckIsExpired(uint24 item, OpString8 &spec_date)
{
	if(!certificate_stack || item >= certificatecount)
		return SSL_NotExpired;
	X509 *cert = certificate_stack[item].certificate;
	if(!cert || spec_date.IsEmpty())
		return SSL_NotExpired;

	if(cmp_time_string(X509_get_notBefore(cert), spec_date.CStr())> 0)
		return SSL_NotYetValid;
	if(cmp_time_string(X509_get_notAfter(cert), spec_date.CStr())< 0)
		return SSL_Expired;

	return SSL_NotExpired;
}
#endif


SSL_CertificateVerification_Info *SSLEAY_CertificateHandler::ExtractVerificationStatus(uint24 &number)
{
	number = 0;

	SSL_CertificateVerification_Info *info = OP_NEWA(SSL_CertificateVerification_Info, certificatecount+10);
	if(info == NULL)
		return NULL;

	uint24 n = 0;
	uint24 i, j;

	for (i = 0; i <certificatecount+10; i++)
	{
		if(!certificate_stack[i].visited)
			continue;

		j = certificate_stack[i].actual_index;

		info[j].status = 0;

		if(certificate_stack[i].invalid)
			info[j].status |= SSL_CERT_INVALID;

		if(certificate_stack[i].incorrect_caflags)
			info[j].status |= SSL_CERT_INCORRECT_ISSUER;

		if(certificate_stack[i].no_issuer)
			info[j].status |= SSL_CERT_NO_ISSUER;

		if(certificate_stack[i].no_crl)
			info[j].status |= SSL_NO_CRL;

		if(certificate_stack[i].invalid_crl)
			info[j].status |= SSL_INVALID_CRL;

		if(!certificate_stack[i].allow_access)
			info[j].status |= SSL_CERT_DENY;
		else if(certificate_stack[i].warn_about_cert)
			info[j].status |= SSL_CERT_WARN;

		if(certificate_stack[i].from_store)
			info[j].status |= SSL_FROM_STORE;

		if(certificate_stack[i].preshipped_cert)
			info[j].status |= SSL_PRESHIPPED;

#if !defined LIBOPEAY_DISABLE_MD5_PARTIAL
		if(certificate_stack[i].used_md5)
			info[j].status |= SSL_USED_MD5;
#endif

#if !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
		if(certificate_stack[i].used_sha1)
			info[j].status |= SSL_USED_SHA1;
#endif

		if(i< certificatecount)
		{
			switch(CheckIsExpired(i))
			{
			case SSL_NotYetValid:
				info[j].status |= SSL_CERT_NOT_YET_VALID | SSL_CERT_EXPIRED;
				break;
			case SSL_Expired:
				info[j].status |= SSL_CERT_EXPIRED;
				break;
			}
		}
		else
		{
			if(certificate_stack[i].expired)
				info[j].status |= SSL_CERT_EXPIRED;

			if(certificate_stack[i].not_yet_valid)
				info[j].status |= SSL_CERT_NOT_YET_VALID;
			
		}

		OpStatus::Ignore(info[j].issuer_name.Set(certificate_stack[i].issuer_name));
		OpStatus::Ignore(info[j].subject_name.Set(certificate_stack[i].subject_name));
		OpStatus::Ignore(info[j].expiration_date.Set(certificate_stack[i].expiration_date));
		OpStatus::Ignore(info[j].valid_from_date.Set(certificate_stack[i].valid_from_date));
	
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
		if((info[j].status & (~SSL_CERT_INFO_FLAGS))!= 0)
			certificate_stack[0].extended_validation = FALSE;
#endif
		if(j>=n)
			n=j+1;
	}

#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	info[0].extended_validation = certificate_stack[0].extended_validation;
#endif

	number = n;
	return info;
}

/*
void ConvertName(const X509_NAME *xname, SSL_DistinguishedName &name)
{ 
	if(xname == NULL)
	{
		name.Resize(0);
		return;
	}
	
	if(!i2d_X509_NAME_Vector((X509_NAME *) xname, name) || !name.GetLength())
	{
		name.Resize(0);
	}
}
*/
void SSLEAY_CertificateHandler::GetX509NameAttributeString(uint24 item, 
			SSL_DistinguishedName &name, X509_NAME *(*func)(X509 *), const X509 *cert) const
{
	const X509_NAME *xname;
	const X509 *cert1 = cert;

	if(!cert1 && item < certificatecount)
		cert1 = certificate_stack[item].certificate;
	
	xname = (cert1 && func ? func((X509 *) cert1) : NULL);

	if(xname == NULL || !i2d_X509_NAME_Vector((X509_NAME *) xname, name))
	{
		name.Resize(0);
	}

	ERR_clear_error();

}

void Parse_stringL(const unsigned char *source, int num, OpString &target);

void ConvertStringL(ASN1_STRING *string, OpString &output)
{
	if(!string)
		return;
	
	if(string->type == V_ASN1_BMPSTRING)
	{
		ANCHORD(OpString, temp);
		SetFromEncodingL(&temp, "utf-16be", (uni_char *) string->data, string->length);
		if(UNICODE_SIZE(temp.Length()) != string->length)
		{
			// Incorrect length; NUL in string
			temp.Empty();
			return;
		}
		LEAVE_IF_ERROR(UriEscape::AppendEscaped(output, temp.CStr(),
			UriEscape::PrefixBackslashX | UriEscape::Ctrl | UriEscape::Range_80_9f));
	}
	else if(string->type == V_ASN1_UTF8STRING)
	{
		ANCHORD(OpString8,temp);
		temp.SetL((char *) string->data, string->length);
		if(temp.Length() != string->length)
		{
			// Incorrect length;
			output.Empty();
			return;
		}

		ANCHORD(OpString,temp1);
		temp1.SetFromUTF8L(temp);
		LEAVE_IF_ERROR(UriEscape::AppendEscaped(output, temp1.CStr(),
			UriEscape::PrefixBackslashX | UriEscape::Ctrl | UriEscape::Range_80_9f));
	}
	else
		Parse_stringL(string->data,string->length,output);
}

OP_STATUS ConvertString(ASN1_STRING *string, OpString &output)
{
	TRAPD(op_err, ConvertStringL(string, output));
	return op_err;
}


void SSLEAY_CertificateHandler::GetServerName(uint24 item,SSL_ServerName_List &name_list) const
{
	X509_EXTENSION *ext; 
	STACK_OF(X509_EXTENSION) *ext_stack;
	X509_NAME_ENTRY *ne;
	X509_NAME *certname;
	long field;
	
	name_list.Reset();
	if(item >= certificatecount || certificate_stack[item].certificate == NULL)
		return;
	
	ext_stack = certificate_stack[item].certificate->cert_info->extensions; 
	if(ext_stack != NULL){
		field = X509v3_get_ext_by_NID(ext_stack,NID_subject_alt_name,-1);

		if(field >=0)
		{
			STACK_OF(GENERAL_NAME) *names = NULL;

			ext= X509v3_get_ext(ext_stack,field);

			if(ext && (names = (STACK_OF(GENERAL_NAME) *) X509V3_EXT_d2i(ext)) != NULL)
			{
				GENERAL_NAME *name;
				int i;

				for(i = 0; i< sk_GENERAL_NAME_num(names); i++)
				{
					name = sk_GENERAL_NAME_value(names, i);
					switch(name->type)
					{
					case GEN_DNS:
						{
							OP_STATUS op_err = name_list.DNSNames.AddString((const char *)name->d.ia5->data, name->d.ia5->length);
							if(OpStatus::IsError(op_err))
							{
								name_list.RaiseAlert(op_err);
								break;
							}

							OpString8 temp;
							temp.Set((const char *)name->d.ia5->data, name->d.ia5->length);
							if(temp.Length() == name->d.ia5->length)
							{
								if(name_list.Namelist.HasContent())
									name_list.Namelist.Append(";");

								name_list.Namelist.Append(temp);
							}
						}
						break;
					case GEN_IPADD:
						{
							uint16 n = name_list.IPadr_List.Count();
							name_list.IPadr_List.Resize(n+1);
							if(name_list.Error())
								break;

							name_list.IPadr_List[n].Set(name->d.ip->data, name->d.ip->length);
							if(name_list.Namelist.HasContent())
								name_list.Namelist.Append(";");
							if (name->d.ip->length == 4)						
								name_list.Namelist.AppendFormat("%u.%u.%u.%u", name->d.ip->data[0], name->d.ip->data[1], name->d.ip->data[2], name->d.ip->data[3]);
							else if (name->d.ip->length == 6)
								name_list.Namelist.AppendFormat("%u.%u.%u.%u.%u.%u", name->d.ip->data[0], name->d.ip->data[1], name->d.ip->data[2], name->d.ip->data[3], name->d.ip->data[4], name->d.ip->data[5]);
							
						}
						break;
					}
				}
			}
			sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
		}
	}
	
	certname = X509_get_subject_name(certificate_stack[item].certificate);
	if(certname != NULL)
	{
		field = -1;

		do{
			field = X509_NAME_get_index_by_NID(certname, NID_commonName,field);

			if(field>=0)
			{
				ne=(X509_NAME_ENTRY *)X509_NAME_get_entry(certname,field);

				if(field>=0)
				{
					OpString temp_common;
					OpAutoPtr<OpString8> temp_common2(OP_NEW(OpString8, ()));
					if(temp_common2.get() == NULL)
					{
						name_list.RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
						break;
					}
					
					OP_STATUS op_err = ConvertString(ne->value, temp_common);
					if(temp_common.HasContent())
					{
						if(OpStatus::IsSuccess(op_err))
							op_err = temp_common2->SetUTF8FromUTF16(temp_common);
						if(temp_common2->HasContent())
						{
							if(name_list.Namelist.HasContent())
								name_list.Namelist.Append(";");
							name_list.Namelist.Append(*temp_common2.get());
						}
						if(OpStatus::IsSuccess(op_err) && temp_common2->HasContent())
							name_list.CommonNames.Add(temp_common2.release());
						if(OpStatus::IsError(op_err))
						{
							name_list.RaiseAlert(op_err);
							break;
						}
					}
				}
			}
		}while(field >= 0);
	}
	ERR_clear_error();
}

void SSLEAY_CertificateHandler::GetSubjectName(uint24 item,SSL_DistinguishedName &name) const
{ 
	GetX509NameAttributeString(item, name, X509_get_subject_name);
}

void SSLEAY_CertificateHandler::GetIssuerName(uint24 item,SSL_DistinguishedName &name) const
{ 
	GetX509NameAttributeString(item, name, X509_get_issuer_name);
}

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
void SSLEAY_CertificateHandler::GetIssuerID(uint24 item,SSL_varvector32 &target) const
{
	if(item >= certificatecount)
	{
		target.Resize(0);
		return;
	}

	CalculateIssuerID(certificate_stack[item].certificate, target, FALSE, TRUE);
}

void SSLEAY_CertificateHandler::GetSubjectID(uint24 item,SSL_varvector32 &target, BOOL always_use_pubkey) const
{
	if(item >= certificatecount)
	{
		target.Resize(0);
		return;
	}

	CalculateIssuerID(certificate_stack[item].certificate, target, TRUE, FALSE, TRUE);
}
#endif // LIBSSL_AUTO_UPDATE_ROOTS


#if 0
void SSLEAY_CertificateHandler::GetSerialNumber(uint24 item,SSL_varvector32 &number) const
{ 
	if(item >= certificatecount || certificate_stack[item].certificate == NULL)
	{
		number.Resize(0);
		return;
	}

	ASN1_INTEGER *snum = X509_get_serialNumber(certificate_stack[item].certificate);
	
	if(snum == NULL)
	{
		number.Resize(0);
		return;
	}

	i2d_ASN1_INTEGER_Vector(snum, number);

	ERR_clear_error();
}
#endif

uint24 SSLEAY_CertificateHandler::CertificateCount() const
{
	return certificatecount;
}


void SSLEAY_CertificateHandler::GetValidatedCertificateChain(SSL_ASN1Cert_list &certs)
{
	certs = validated_certificates;
}

void Parse_stringL(const unsigned char *source, int num, OpString &target)
{
	LEAVE_IF_ERROR(UriEscape::AppendEscaped(target, (const char*)source, num,
		UriEscape::PrefixBackslashX | UriEscape::Ctrl | UriEscape::Range_80_9f));
}

OP_STATUS ParseObject(ASN1_OBJECT *obj, Str::LocaleString unknown, OpString &target, int &id)
{
	int nid;
	
	nid = OBJ_obj2nid(obj);
	target.Empty();  
	if(nid == NID_undef && unknown != Str::NOT_A_STRING)
	{
		RETURN_IF_ERROR(SetLangString(unknown, target));
		OP_ASSERT(target.HasContent()); // Language string missing - check LNG file
	}
	{
		char * temp = (char *) g_memory_manager->GetTempBuf2();
		int temp_len = g_memory_manager->GetTempBuf2Len();

		if(OBJ_obj2txt(temp, temp_len,obj,0)>0)
			RETURN_IF_ERROR(target.Append(temp)); 
	}
	id = nid;
	return OpStatus::OK;
}


OP_STATUS Parse_name(X509_NAME *name, OpString_list &target)
{
	X509_NAME_ENTRY *ne;
	unsigned int i,othercount,lineno;
	int n, fields, append, use_fieldname;
	
	if (name == NULL) 
		return OpStatus::ERR_NULL_POINTER;
	
	OpString tempstring;
	
	fields = X509_NAME_entry_count(name);
	OpStatus::Ignore(target.Resize(0)); // Safely ignore OP_STATUS when resizing to 0
	RETURN_IF_ERROR(target.Resize(fields+5));
	
	othercount = 0;   
	for (i=0; (int)i<fields; i++)
	{
		ne=X509_NAME_get_entry(name,i);
		RETURN_IF_ERROR(ParseObject(ne->object, Str::S_LIBSSL_UNKOWN_NAME_FIELD, tempstring, n));
		
		append = 0;
		use_fieldname = 0;
		switch(n)
		{
		case NID_commonName :  
			lineno = 0;
			append = 2; // Postcede
			break;
		case NID_organizationName : 
			lineno = 1;
			break;
		case NID_organizationalUnitName : 
			lineno = 2;
			append = 2;   // Postcede
			break;
		case NID_localityName : 
			lineno = 3;
			break;
		case NID_stateOrProvinceName : 
			lineno = 4;
			append = 1;  // Precede
			break;
		case NID_countryName :  
			lineno = 4;
			append = 2;  // Postcede
			break;
		default : 
			lineno = 5+othercount;
			othercount++;
			use_fieldname = 1;
		}

		if (use_fieldname)
			RETURN_IF_ERROR(tempstring.Append(": "));
		else 
			tempstring.Empty();

		ConvertString(ne->value, tempstring);

		{
			uint32 stlen = target[lineno].Length();
			
			if(stlen >0 &&append == 2)
			{
				RETURN_IF_ERROR(target[lineno].Append(", "));
				RETURN_IF_ERROR(target[lineno].Append(tempstring));
			}
			else
			{
				if(stlen > 0 && append == 1)
				{
					RETURN_IF_ERROR(tempstring.Append(", "));
					RETURN_IF_ERROR(tempstring.Append(target[lineno]));
				}
				RETURN_IF_ERROR(target[lineno].Set( tempstring )); 
			}
		}
	}

	return OpStatus::OK;
}

/*
void ExtractBIO_string(BIO *bio, SSL_varvector32 &target)
{
uint32 len;

  len = BIO_number_written(bio);
  target.Resize(len);
  if(target.ErrorRaisedFlag){
  target.Resize(0);
  return;
  }
  
	BIO_read(bio, (char *) target.GetDirect(), (int) len);
} */


OP_STATUS SSLEAY_CertificateHandler::GetName(uint24 item,OpString_list &name, X509_NAME *(*func)(X509 *)) const
{ 
	const X509 *cert1 = NULL;
	const X509_NAME * OP_MEMORY_VAR xname;

	if(item < certificatecount)
		cert1 = certificate_stack[item].certificate ;

	xname = (cert1 && func ? func((X509 *) cert1) : NULL);

	name.Resize(0);
	if(xname)
		RETURN_IF_ERROR(Parse_name((X509_NAME *) xname, name));  

	ERR_clear_error();
	return OpStatus::OK;
}


OP_STATUS SSLEAY_CertificateHandler::GetSubjectName(uint24 item,OpString_list &name) const
{ 
	return GetName(item, name, X509_get_subject_name);
}

OP_STATUS SSLEAY_CertificateHandler::GetIssuerName(uint24 item,OpString_list &name) const
{ 
	return GetName(item, name, X509_get_issuer_name);
}


OP_STATUS ParseTime(ASN1_UTCTIME *time_val, OpString &target)
{
	if(target.Reserve(100) == NULL)
		return OpStatus::ERR_NO_MEMORY;
	*target.DataPtr() = '\0';
	if(!ASN1_UTCTIME_unisprint(target.DataPtr(), time_val))
	{
		target.Empty();
		return OpStatus::ERR_PARSING_FAILED;
    }

	return OpStatus::OK;
}


SSL_PublicKeyCipher *GetPublicKeyCipher(X509 *);

SSL_PublicKeyCipher *SSLEAY_CertificateHandler::GetCipher(uint24 item) const
{
	if(item >= certificatecount)
		return NULL;

	X509 *cert = (item == 0 ? endcertificate : certificate_stack[item].certificate);

	return GetPublicKeyCipher(cert);
}

uint32 SSLEAY_CertificateHandler::KeyBitsLength(uint24 item) const
{
	uint32 len = 0;
	SSL_PublicKeyCipher *cipher = GetCipher(item);

	if(cipher)
	{
		len = cipher->KeyBitsLength();
		OP_DELETE(cipher);
	}

	return len;
}

/*
BOOL SSLEAY_CertificateHandler::SignOnly(uint24) const
{
return FALSE;
}

  BOOL SSLEAY_CertificateHandler::EncodeOnly(uint24) const
  {
  return FALSE;
  }
  
	BOOL SSLEAY_CertificateHandler::EncodeAndSign(uint24) const
	{
	return TRUE;
} */

SSL_CertificateHandler *SSLEAY_CertificateHandler::Fork() const
{
	return OP_NEW(SSLEAY_CertificateHandler, (this));
}

BOOL SSLEAY_CertificateHandler::SelfSigned(uint24 item) const
{
	//X509_CINF *ci;
	BOOL flag;
	
	if(item >= certificatecount)
		return FALSE;
	
	flag = (X509_check_issued(certificate_stack[item].certificate, certificate_stack[item].certificate) == X509_V_OK);
	ERR_clear_error();
	return flag;
}

SSL_SignatureAlgorithm SSLEAY_CertificateHandler::CertificateSignatureAlg(uint24 item) const
{
	X509 *cert;
	int nid;
	SSL_SignatureAlgorithm type;
	
	type = SSL_Illegal_sign;
	if(item >= certificatecount)
		return type;
    
	cert =  certificate_stack[item].certificate;
	
	nid = OBJ_obj2nid(cert->sig_alg->algorithm);
	switch(nid)
	{
	case  NID_md5WithRSAEncryption : 
		type = SSL_RSA_MD5;
		break;

	case  NID_shaWithRSAEncryption :
	case  NID_sha1WithRSAEncryption :
		type = SSL_RSA_SHA;
		break;

	case  NID_sha256WithRSAEncryption :
		type = SSL_RSA_SHA_256;
		break;

	case  NID_sha224WithRSAEncryption :
		type = SSL_RSA_SHA_224;
		break;

	case  NID_sha512WithRSAEncryption :
		type = SSL_RSA_SHA_512;
		break;

	case  NID_sha384WithRSAEncryption :
		type = SSL_RSA_SHA_384;
		break;

	case  NID_dsaWithSHA :
	case  NID_dsaWithSHA1 :
		type = SSL_DSA_sign;
		break;
	}
	ERR_clear_error();
	return type;
}

SSL_SignatureAlgorithm SSLEAY_CertificateHandler::CertificateSigningKeyAlg(uint24 item) const
{
	X509 *cert;
	int nid;
	SSL_SignatureAlgorithm type;
	
	type = SSL_Anonymous_sign;
	if(item >= certificatecount)
		return type;
    
	cert =  certificate_stack[item].certificate;
	
	nid = OBJ_obj2nid(cert->cert_info->key->algor->algorithm);
	switch(nid)
	{
    case NID_rsa :
    case NID_rsaEncryption :
		type = SSL_RSA_sign;
		break;

	case NID_dsa:
		type = SSL_DSA_sign;
		break;
	}
	ERR_clear_error();
	return type;
}

SSL_ClientCertificateType SSLEAY_CertificateHandler::CertificateType(uint24 item) const
{
	X509 *cert;
	int nid;
	SSL_ClientCertificateType type;
	
	type = SSL_Illegal_Cert_Type;
	if(item >= certificatecount)
		return type;
    
	cert =  certificate_stack[item].certificate;
	
	nid = OBJ_obj2nid(cert->cert_info->key->algor->algorithm);
	switch(nid)
	{
    case NID_rsa :
    case NID_rsaEncryption : type =  SSL_rsa_sign;
		break;
		
    case NID_dsa          : type =  SSL_dss_sign;
		break;
		
    case NID_dhKeyAgreement :
		nid = OBJ_obj2nid(cert->sig_alg->algorithm);
		switch(nid)
		{
		case  NID_md2WithRSAEncryption :
		case  NID_md5WithRSAEncryption :
		case  NID_shaWithRSAEncryption :
		case  NID_sha1WithRSAEncryption :   type =  SSL_rsa_fixed_dh;
			break;
		case  NID_dsaWithSHA :
		case  NID_dsaWithSHA1 :     type =  SSL_dss_fixed_dh;
			break;
		}
		break;
	}
	ERR_clear_error();
	return type;
}

void SSLEAY_CertificateHandler::GetPublicKeyHash(uint24 item, SSL_varvector16 &keyhash) const
{
	//  SSL_varvector16 pub_key;
	SSL_Hash_Pointer sha1(SSL_SHA);
	ASN1_BIT_STRING *key;
	
	if(sha1.ErrorRaisedFlag || item >= certificatecount)
	{
		keyhash.Resize(0);
		return;
	}
	
	key = certificate_stack[item].certificate->cert_info->key->public_key;
	sha1->CompleteHash(key->data, key->length, keyhash);
}

void SSLEAY_CertificateHandler::GetFingerPrint(uint24 item, SSL_varvector16 &fingerprint) const
{
	if(item >= certificatecount)
	{
		fingerprint.Resize(0);
		return;
	}
	
	fingerprint = fingerprints_sha256[item];
}

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
BOOL SSLEAY_CertificateHandler::UsagePermitted(SSL_CertificatePurpose usage_type)
{
	BOOL ret = FALSE;
	X509 *x = certificate_stack ? certificate_stack[0].certificate : NULL;
	ASN1_BIT_STRING *usage;
	STACK_OF(ASN1_OBJECT) *extusage;
	int i;
	BOOL found_extension = FALSE;

	if(x == NULL)
		return FALSE;

	/* Handle key usage */
	if((usage= (ASN1_BIT_STRING *) X509_get_ext_d2i(x, NID_key_usage, NULL, NULL)) != NULL) 
	{
		if(usage->length > 0) 
		{
			switch(usage_type)
			{
			case SSL_Purpose_Client_Certificate:
				if(usage->data[0] & 0x80)
					ret = TRUE;
				break;
			}
		}
		ASN1_BIT_STRING_free(usage);

		if(ret)
			return ret;
	
		found_extension = TRUE;
	}

	if((extusage= (STACK_OF(ASN1_OBJECT) *)X509_get_ext_d2i(x, NID_ext_key_usage, NULL, NULL))) 
	{
		for(i = 0; i < sk_ASN1_OBJECT_num(extusage); i++) 
		{
			int nid = OBJ_obj2nid(sk_ASN1_OBJECT_value(extusage,i));
			switch(usage_type)
			{
			case SSL_Purpose_Client_Certificate:
				if(nid == NID_client_auth)
				{
					ret = TRUE;
				}
				break;
			}
		}
		sk_ASN1_OBJECT_pop_free(extusage, ASN1_OBJECT_free);

		if(ret)
			return ret;
	
		found_extension = TRUE;
	}

	return (found_extension ? FALSE : TRUE);
}
#endif

OP_STATUS SSLEAY_CertificateHandler::Get_AIA_URL_Information(uint24 index, OpString_list &aia_url_strings, int nid)
{
	ERR_clear_error();
	aia_url_strings.Resize(0);

	X509 *cert = certificate_stack[index].certificate;
	if(cert == NULL)
		return OpStatus::OK;

	AUTHORITY_INFO_ACCESS *info_access;
	OP_STATUS op_err = OpStatus::OK;

	if((info_access= (AUTHORITY_INFO_ACCESS *)X509_get_ext_d2i(cert, NID_info_access, NULL, NULL)) != 0) 
	{

		int i;
		for(i = 0; i < sk_ACCESS_DESCRIPTION_num(info_access); i++) 
		{
			ACCESS_DESCRIPTION *info_item = sk_ACCESS_DESCRIPTION_value(info_access,i);
			if(info_item && 
				OBJ_obj2nid(info_item->method) == nid && 
				info_item->location->type == GEN_URI && 
				info_item->location->d.ia5)
			{
				int n = aia_url_strings.Count();
				op_err = aia_url_strings.Resize(n+1);
				if(OpStatus::IsSuccess(op_err))
					op_err = aia_url_strings[n].Set((const char *) info_item->location->d.ia5->data, info_item->location->d.ia5->length);
				if(OpStatus::IsError(op_err))
					break;
			}
		}
		sk_ACCESS_DESCRIPTION_pop_free(info_access, ACCESS_DESCRIPTION_free);
	}

	CheckError();

	if(Error())
	{
		return (op_err != OpStatus::OK ? op_err : GetOPStatus());
	}

	return op_err;
}


OP_STATUS SSLEAY_CertificateHandler::GetIntermediateCA_Requests(uint24 index, OpString_list &ica_url_strings)
{
	ica_url_strings.Resize(0);

	return Get_AIA_URL_Information(index, ica_url_strings, NID_ad_ca_issuers);
}


OP_STATUS SSLEAY_CertificateHandler::GetOCSP_Request(OpString_list &ocsp_url_strings, SSL_varvector32 &binary_request
#ifndef TLS_NO_CERTSTATUS_EXTENSION
													 , SSL_varvector32 &premade_ocsp_extensions
#endif
													 )
{
	ERR_clear_error();
	ocsp_url_strings.Resize(0);
	binary_request.Resize(0);

	if(endcertificate == NULL || firstissuer == NULL || endcertificate == firstissuer)
		return OpStatus::OK;

	RETURN_IF_ERROR(Get_AIA_URL_Information(0, ocsp_url_strings, NID_ad_OCSP));
	
	OCSP_REQUEST *request = OCSP_REQUEST_new();
	STACK_OF(OCSP_CERTID) *ids = sk_OCSP_CERTID_new_null();
	OP_STATUS op_err = OpStatus::ERR_NO_MEMORY;
	if(request && ids)
	{
		op_err = OpStatus::OK;
		do{
			OCSP_CERTID *id = OCSP_cert_to_id(NULL, endcertificate, firstissuer);
			if(id == NULL || !sk_OCSP_CERTID_push(ids, id) || !OCSP_request_add0_id(request, id))
			{
				op_err = OpStatus::ERR_NO_MEMORY;
				break;
			}

			/*
			id = OCSP_cert_id_new(EVP_sha1(), X509_get_subject_name(firstissuer), 
				X509_get0_pubkey_bitstr(firstissuer), X509_get_serialNumber(endcertificate));
			if(id == NULL || !sk_OCSP_CERTID_push(ids, id) || !OCSP_request_add0_id(request, id))
			{
				op_err = OpStatus::ERR_NO_MEMORY;
				break;
			}
			*/
		} while(0);

#ifndef TLS_NO_CERTSTATUS_EXTENSION
		if(premade_ocsp_extensions.GetLength() == 0)
#endif
		{
#ifdef TLS_ADD_OCSP_NONCE
			OCSP_request_add1_nonce(request, NULL, -1);
#endif
		}
#ifndef TLS_NO_CERTSTATUS_EXTENSION
		else
		{
			const byte *buf = premade_ocsp_extensions.GetDirect();

			if(!d2i_ASN1_SET_OF_X509_EXTENSION(&request->tbsRequest->requestExtensions, 
				&buf,  premade_ocsp_extensions.GetLength(),
				d2i_X509_EXTENSION, X509_EXTENSION_free,
				2, ASN1_TFLG_CONTEXT /* |		ASN1_TFLG_SEQUENCE_OF|ASN1_TFLG_OPTIONAL*/))
					op_err = OpStatus::ERR;
		}
#endif

		i2d_OCSP_REQUEST_Vector(request, binary_request);
		if(!binary_request.GetLength())
			op_err = OpStatus::ERR;
		else
		{
			OP_ASSERT(sent_ocsp_request == NULL);
			OCSP_REQUEST_free(sent_ocsp_request);
			sent_ocsp_request = request;
			request = NULL;
		}
		RaiseAlert(binary_request);

	}

	if(request)
	{
		OCSP_REQUEST_free(request);
	}
	if(ids)
		sk_OCSP_CERTID_free(ids);

	CheckError();

	if(Error())
	{
		return (op_err != OpStatus::OK ? op_err : OpStatus::ERR);
	}

	return op_err;
}

OP_STATUS SSLEAY_CertificateHandler::VerifyOCSP_Response(SSL_varvector32 &binary_response)
{
	ERR_clear_error();
	if(!sent_ocsp_request)
	{
		RaiseAlert(SSL_Fatal, SSL_Certificate_OCSP_Verify_failed);
		SetReason(Str::S_OCSP_VERIFY_FAILED);
		return OpStatus::ERR;
	}

	OCSP_RESPONSE *response = d2i_OCSP_RESPONSE_Vector(NULL, binary_response);
	if(!response)
	{
		OCSP_REQUEST_free(sent_ocsp_request);
		sent_ocsp_request = NULL;
		RaiseAlert(SSL_Fatal, SSL_Certificate_OCSP_Verify_failed);
		SetReason(Str::S_OCSP_VERIFY_FAILED);
		ERR_clear_error();
		return OpStatus::ERR;
	}

	OCSP_BASICRESP *basic_response = NULL;

	OP_STATUS op_err = OpStatus::ERR_NO_MEMORY;
	do{
		op_err = OpStatus::OK;

		int code = OCSP_response_status(response);

		if(code != OCSP_RESPONSE_STATUS_SUCCESSFUL)
		{
			//OP_ASSERT(0);
			SSL_AlertDescription al= SSL_Certificate_OCSP_error;
			Str::LocaleString reason = Str::S_OCSP_UNKNOWN_REASON;
			switch(code)
			{
			case OCSP_RESPONSE_STATUS_MALFORMEDREQUEST:
				reason = Str::S_OCSP_MALFORMED_REQUEST;
				break;
			case OCSP_RESPONSE_STATUS_INTERNALERROR:
				reason = Str::S_OCSP_INTERNALERROR;
				break;
			case OCSP_RESPONSE_STATUS_TRYLATER:
				reason = Str::S_OCSP_TRYLATER;
				break;
			case OCSP_RESPONSE_STATUS_SIGREQUIRED:
				reason = Str::S_OCSP_SIGREQUIRED;
				break;
			case OCSP_RESPONSE_STATUS_UNAUTHORIZED:
				al= SSL_Fraudulent_Certificate;
				reason = Str::NOT_A_STRING;
				break;
			}

			RaiseAlert(SSL_Fatal, al);
			SetReason(reason);
			op_err = OpStatus::ERR;
			break;
		}

		basic_response = OCSP_response_get1_basic(response);
		if(!basic_response)
		{
			op_err = OpStatus::ERR_NO_MEMORY;
			break;
		}

		int test;
#ifdef TLS_ADD_OCSP_NONCE
		test = OCSP_check_nonce(sent_ocsp_request, basic_response);
		if(test != -1 /*responder did not send a nonce, accepted*/ && 
			test != 1 /* nonces were correct, accepted */)
		{
			// Everything else is an error
			RaiseAlert(SSL_Fatal, SSL_Certificate_OCSP_error);
			SetReason(Str::S_OCSP_RESPONSE_ERROR);
			op_err = OpStatus::ERR;
			break;
		}
#endif

#if !(defined LIBOPEAY_DISABLE_MD5_PARTIAL && defined LIBOPEAY_DISABLE_SHA1_PARTIAL)
		int sig_alg = OBJ_obj2nid(basic_response->signatureAlgorithm->algorithm);
#ifndef LIBOPEAY_DISABLE_MD5_PARTIAL
		if(g_SSL_warn_md5 && (sig_alg == NID_md5 ||
			sig_alg == NID_md5WithRSA ||
			sig_alg == NID_md5WithRSAEncryption))
			certificate_stack[0].used_md5 = TRUE;
#endif
#if !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
		if(g_SSL_warn_sha1 && (sig_alg == NID_sha1 ||
			sig_alg == NID_sha1WithRSAEncryption ||
			sig_alg == NID_dsaWithSHA1_2 ||
			sig_alg == NID_sha1WithRSA ||
			sig_alg == NID_dsaWithSHA1))
			certificate_stack[0].used_sha1 = TRUE;
#endif
#endif
        test = OCSP_basic_verify(basic_response, NULL, g_store, 0);

		if(test < 0)
		{
			RaiseAlert(SSL_Fatal, SSL_Certificate_OCSP_Verify_failed);
			SetReason(Str::S_OCSP_VERIFY_FAILED);
			op_err = OpStatus::ERR;
			break;
		}
		
		OCSP_ONEREQ *cert_one;
		OCSP_CERTID *certid;

		cert_one= sk_OCSP_ONEREQ_value(sent_ocsp_request->tbsRequest->requestList, 0);
		if(!cert_one || (certid = cert_one->reqCert) == NULL)
		{
			RaiseAlert(SSL_Fatal, SSL_Certificate_OCSP_error);
			SetReason(Str::S_OCSP_RESPONSE_ERROR);
			op_err = OpStatus::ERR;
			break;
		}

		int revocation_code = -1, response_code= -1;
		ASN1_GENERALIZEDTIME *thisupd=NULL, *nextupd=NULL;
		test = OCSP_resp_find_status(basic_response, certid, &response_code, &revocation_code, NULL, &thisupd, &nextupd);

		if (!test || response_code <0 || !OCSP_check_validity(thisupd,nextupd, 3600, 100*24*3600))
		{
			RaiseAlert(SSL_Fatal, SSL_Certificate_OCSP_error);
			SetReason(Str::S_OCSP_TOO_OLD);
			op_err =  OpStatus::ERR;
			break;
		}

		switch(response_code)
		{
		case V_OCSP_CERTSTATUS_GOOD:
			// certificate is OK (or the status is unknown), and we accept the certificate
			ERR_clear_error();
			OCSP_BASICRESP_free(basic_response);
			OCSP_RESPONSE_free(response);
			return OpStatus::OK;
		case V_OCSP_CERTSTATUS_REVOKED:
			{

				Str::LocaleString reason = Str::S_NOT_A_STRING;
				switch(revocation_code)
				{
				case OCSP_REVOKED_STATUS_REMOVEFROMCRL:
				case OCSP_REVOKED_STATUS_CERTIFICATEHOLD:
				case OCSP_REVOKED_STATUS_UNSPECIFIED:
					// default
					break;
				case OCSP_REVOKED_STATUS_KEYCOMPROMISE:
					reason = Str::S_OCSP_REVOKED_KEYCOMPROMISE;
					break;
				case OCSP_REVOKED_STATUS_CACOMPROMISE:
					reason = Str::S_OCSP_REVOKED_CACOMPROMISE;
					break;
				case OCSP_REVOKED_STATUS_AFFILIATIONCHANGED:
					reason = Str::S_OCSP_REVOKED_AFFILIATIONCHANGED;
					break;
				case OCSP_REVOKED_STATUS_SUPERSEDED:
					reason = Str::S_OCSP_REVOKED_SUPERSEDED;
					break;
				case OCSP_REVOKED_STATUS_CESSATIONOFOPERATION:
					reason = Str::S_OCSP_REVOKED_CESSATIONOFOPERATION;
					break;
				}

				RaiseAlert(SSL_Fatal, SSL_Certificate_Revoked);
				SetReason(reason);
				op_err =  OpStatus::ERR;
				break;
			}
		case V_OCSP_CERTSTATUS_UNKNOWN:
		default:
				RaiseAlert(SSL_Fatal, SSL_Fraudulent_Certificate);
				op_err =  OpStatus::ERR;
				break;
		}
		
	} while(0);

	if(basic_response)
		OCSP_BASICRESP_free(basic_response);

	OCSP_RESPONSE_free(response);

	OCSP_REQUEST_free(sent_ocsp_request);
	sent_ocsp_request = NULL;

	CheckError();
		
	if(!ErrorRaisedFlag && Error()) // if we already have an error use that one
	{
		return (op_err != OpStatus::OK ? op_err : OpStatus::ERR);
	}
	ERR_clear_error();

	return op_err;
}

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
void SSLEAY_CertificateHandler::GetPendingIssuerId(SSL_varvector32 &id)
{
	id = pending_issuer_id;
}
#endif


void SSLEAY_CheckError(SSL_Error_Status *target);

OP_STATUS CreateOCSP_Extensions(SSL_varvector16 &extenions)
{
	extenions.Resize(0);
	
	OCSP_REQUEST *request = OCSP_REQUEST_new();
	OP_STATUS op_err = OpStatus::ERR_NO_MEMORY;
	if(request)
	{
		op_err = OpStatus::OK;

#ifdef TLS_ADD_OCSP_NONCE
		OCSP_request_add1_nonce(request, NULL, -1);
#endif

		uint32 len = i2d_ASN1_SET_OF_X509_EXTENSION(request->tbsRequest->requestExtensions, NULL, i2d_X509_EXTENSION, 2, ASN1_TFLG_EXPLICIT /*| ASN1_TFLG_SEQUENCE_OF|ASN1_TFLG_OPTIONAL*/, 0);
		extenions.Resize(len);
		if(!extenions.Error())
		{
			byte *buf = extenions.GetDirect();
			len = i2d_ASN1_SET_OF_X509_EXTENSION(request->tbsRequest->requestExtensions, &buf, i2d_X509_EXTENSION, 2, (ASN1_TFLG_EXPLICIT /*| ASN1_TFLG_SEQUENCE_OF|ASN1_TFLG_OPTIONAL*/), 0);
			OP_ASSERT(len == extenions.GetLength());
		}

		if(!extenions.GetLength())
			op_err = OpStatus::ERR;
	}

	if(request)
	{
		OCSP_REQUEST_free(request);
	}

	SSLEAY_CheckError(&extenions);
	if(extenions.Error())
	{
		return (op_err != OpStatus::OK ? op_err : OpStatus::ERR);
	}

	return op_err;

}

#include "modules/libopeay/openssl/err.h"

void SSLEAY_CheckError(SSL_Error_Status *target)
{
	unsigned long  err;
	BOOL decode,fail;
	
	fail = decode = FALSE;
	while((err = ERR_get_error()) != 0)
	{
		int lib = ERR_GET_LIB(err);
		int reason = ERR_GET_REASON(err);
		if(lib == ERR_LIB_X509 && reason == X509_R_UNKNOWN_PURPOSE_ID)
			continue;
		if(lib == ERR_LIB_ASN1 && (
			reason == ASN1_R_UNKNOWN_MESSAGE_DIGEST_ALGORITHM || 
			reason == ASN1_R_UNKNOWN_PUBLIC_KEY_TYPE ||
			reason == ASN1_R_UNSUPPORTED_PUBLIC_KEY_TYPE) )
			continue;

		fail = TRUE;
		if( ERR_GET_LIB(err) == ERR_LIB_ASN1 || ERR_GET_LIB(err) == ERR_LIB_X509 )
			decode = TRUE;
	}
	
	if(fail && !target->Error())
		target->RaiseAlert((decode ? SSL_Fatal : SSL_Internal),(decode ? SSL_Decode_Error : SSL_InternalError));
}

void SSLEAY_CertificateHandler::CheckError()
{
	SSLEAY_CheckError(this);
}

#endif // !ADS12 or ADS12_TEMPLATING
#endif // relevant support
