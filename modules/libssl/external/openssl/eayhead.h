/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _EAY_HEAD_H
#define _EAY_HEAD_H

#if defined _NATIVE_SSL_SUPPORT_ && defined _SSL_USE_OPENSSL_

#define SSL_IN_OPERA

#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/crypto.h"
#include "modules/libopeay/openssl/buffer.h"
#include "modules/libopeay/openssl/bn.h"
#include "modules/libopeay/openssl/evp.h"
#include "modules/libopeay/openssl/asn1.h"
#include "modules/libopeay/openssl/x509.h"
#include "modules/libopeay/openssl/objects.h"
#include "modules/libopeay/openssl/err.h"   
#include "modules/libopeay/openssl/pem.h"
#include "modules/libopeay/openssl/pkcs7.h"
#include "modules/libopeay/openssl/rand.h"
#include "modules/libopeay/openssl/hmac.h"

BOOL i2d_Vector(int (*i2d)(void *, unsigned char **), void *source, SSL_varvector32 &target);
void *d2i_Vector(void *(*d2i)(void *, unsigned char **, long), void *, SSL_varvector32 &source);

#define i2d_X509_Vector(source, target) i2d_Vector((int (*)(void *, unsigned char **)) i2d_X509, source, target)
#define i2d_X509_NAME_Vector(source, target) i2d_Vector((int (*)(void *, unsigned char **))i2d_X509_NAME, source, target)
#define i2d_PrivateKey_Vector(source, target) i2d_Vector((int (*)(void *, unsigned char **))i2d_PrivateKey, source, target)
#define i2d_PublicKey_Vector(source, target) i2d_Vector((int (*)(void *, unsigned char **))i2d_PublicKey, source, target)
#define i2d_ASN1_INTEGER_Vector(source, target) i2d_Vector((int (*)(void *, unsigned char **))i2d_ASN1_INTEGER, source, target)
#define i2d_DSA_SIG_Vector(source, target) i2d_Vector((int (*)(void *, unsigned char **))i2d_DSA_SIG, source, target)
#define i2d_PKCS7_Vector(source, target) i2d_Vector((int (*)(void *, unsigned char **))i2d_PKCS7, source, target)
#define i2d_PKCS7_SIGNED_Vector(source, target) i2d_Vector((int (*)(void *, unsigned char **))i2d_PKCS7_SIGNED, source, target)
#define i2d_PKCS12_Vector(source, target) i2d_Vector((int (*)(void *, unsigned char **))i2d_PKCS12, source, target)
#define i2d_OCSP_REQUEST_Vector(source, target) i2d_Vector((int (*)(void *, unsigned char **))i2d_OCSP_REQUEST, source, target)

#define d2i_X509_Vector(target, source) (X509 *) d2i_Vector((void *(*)(void *, unsigned char **, long)) d2i_X509, target, source)
#define d2i_PrivateKey_Vector(target, source) (EVP_PKEY *) d2i_Vector((void *(*)(void *, unsigned char **, long))d2i_AutoPrivateKey, target, source)
#define d2i_X509_NAME_Vector(target, source) (X509_NAME *) d2i_Vector((void *(*)(void *, unsigned char **, long))d2i_X509_NAME, target, source)
#define d2i_PKCS7_Or_Netscape_Vector(target, source) (PKCS7_Or_Netscape *) d2i_Vector((void *(*)(void *, unsigned char **, long))d2i_PKCS7_Or_Netscape_certs, target, source)
#define d2i_DSA_SIG_Vector(target, source) (DSA_SIG *) d2i_Vector((void *(*)(void *, unsigned char **, long))d2i_DSA_SIG, target, source)
#define d2i_PKCS7_Vector(target, source) (PKCS7 *) d2i_Vector((void *(*)(void *, unsigned char **, long))d2i_PKCS7, target, source)
#define d2i_PKCS7_SIGNED_Vector(target, source) (PKCS7_SIGNED *) d2i_Vector((void *(*)(void *, unsigned char **, long))d2i_PKCS7_SIGNED, target, source);
#define d2i_NETSCAPE_CERT_SEQUENCE_Vector(target, source) (NETSCAPE_CERT_SEQUENCE *) d2i_Vector((void *(*)(void *, unsigned char **, long))d2i_NETSCAPE_CERT_SEQUENCE, target, source);
#define d2i_OCSP_RESPONSE_Vector(target, source) (OCSP_RESPONSE *) d2i_Vector((void *(*)(void *, unsigned char **, long))d2i_OCSP_RESPONSE, target, source)
#define d2i_X509_CRL_Vector(target, source) (X509_CRL *) d2i_Vector((void *(*)(void *, unsigned char **, long)) d2i_X509_CRL, target, source)

#endif
#endif 
