/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/* addon/elgamal.c */
/* Parts of this code is modelled or copied from OpenSSL code under the following terms */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
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
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
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

#if defined(NEED_LIBOPEAY_ELGAMAL)


#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/crypto.h"
#include "modules/libopeay/openssl/err.h"
#include "modules/libopeay/openssl/buffer.h"
#include "modules/libopeay/openssl/bn.h"
#include "modules/libopeay/openssl/rsa.h"
#include "modules/libopeay/openssl/dh.h"
#include "modules/libopeay/openssl/rand.h"
#include "modules/libopeay/openssl/x509.h"
#include "modules/libopeay/addon/elgamal.h"

#define ElGamal_err(f,r)  ERR_PUT_error(ERR_LIB_USER,(f),(r),__FILE__,__LINE__)

#define ELGAMAL_F_ELGAMAL_ENCRYPT	1
#define ELGAMAL_F_ELGAMAL_SIGN		2
#define ELGAMAL_F_ELGAMAL_DECRYPT	3
#define ELGAMAL_F_ELGAMAL_VERIFY	4

#define ELGAMAL_R_THE_ASN1_OBJECT_IDENTIFIER_IS_NOT_KNOWN_FOR_THIS_MD 1
#define ELGAMAL_R_PADDING_CHECK_FAILED		2
#define ELGAMAL_R_UNKNOWN_ALGORITHM_TYPE	3
#define ELGAMAL_R_DATA_TOO_LARGE_FOR_MODULUS 4
#define ELGAMAL_R_DIGEST_TOO_BIG_FOR_KEY	5
#define ELGAMAL_R_WRONG_SIGNATURE_LENGTH	6

#if 0
int ElGamal_GenerateExchangeKey(DH *dh)
{
	BN_CTX *ctx;
	BIGNUM *p1 ; /* p-1 */
	BIGNUM *r; /* gcd result; */
	int l;
	
	if(dh == NULL)
		return -1;

	BN_free(dh->priv_key);
	BN_free(dh->pub_key);
	dh->pub_key = NULL;
	dh->priv_key = NULL;

	ctx = BN_CTX_new();
	if (ctx == NULL) goto err;

	dh->priv_key=BN_new();
	if (dh->priv_key == NULL) goto err;

	p1 = BN_new();
	r = BN_new();
	if(p1 == NULL || r == NULL) goto err;

	if(!BN_sub(p1, dh->p, BN_value_one())) goto err;
	
	l = dh->length ? dh->length : BN_num_bits(dh->p)-1; /* secret exponent length */
	/* Find a number relatively prime with (p-1) */
	do{
		if (!BN_rand(dh->priv_key, l, 0, 0)) goto err;
		if(!BN_gcd(r, dh->priv_key, p1, ctx)) goto err;
	}while(BN_cmp(r, BN_value_one()) != 0);
	
	if(!BN_mod_exp(r, dh->g, dh->priv_key, dh->p, ctx)) goto err;
	
	dh->pub_key = r;
	r = NULL;

err:;
	if(p1)
		BN_free(p1);
	if(r)
		BN_free(r);
	if(ctx)
		BN_CTX_free(ctx);

	return (dh->pub_key ? 0 : -1);
}
#endif

/* modelled on code from rsa_eay.c */
int ElGamal_encrypt(int flen, const unsigned char *from, unsigned char *to, BIGNUM **a,  DH *dh)
{
	BIGNUM y,f, k, p1, ret;
	int i,j,k1,l,num=0,r= -1;
	unsigned char *buf=NULL;
	BN_CTX *ctx=NULL;

	if(a== NULL)
		return(-1);

	if(*a == NULL)
		*a = BN_new();
	if(*a == NULL)
		return(-1);

	BN_init(&k);
	BN_init(&y);
	BN_init(&p1);
	BN_init(&f);
	BN_init(&ret);
	if ((ctx=BN_CTX_new()) == NULL) goto err;
	num=BN_num_bytes(dh->p);
	if ((buf=(unsigned char *)OPENSSL_malloc(num)) == NULL)
		{
		ElGamal_err(ELGAMAL_F_ELGAMAL_ENCRYPT,ERR_R_MALLOC_FAILURE);
		goto err;
		}

	if(!BN_sub(&p1, dh->p, BN_value_one())) goto err;

	l = dh->length ? dh->length : BN_num_bits(dh->p)-1; /* secret exponent length */
	/* Find a number relatively prime with (p-1) */
	do{
		if (!BN_rand(&k, l, 0, 0)) goto err;
		if(!BN_gcd(&y, &k, &p1, ctx)) goto err;
	}while(BN_cmp(&y, BN_value_one()) != 0);
	
	if(!BN_mod_exp(*a, dh->g, &k, dh->p, ctx)) goto err;

	if(RSA_padding_add_PKCS1_type_2(buf,num,from,flen)<=0) goto err;

	if(BN_bin2bn(buf, num, &f) == NULL)
		goto err;

	if(!BN_mod_exp(&y, dh->pub_key, &k, dh->p, ctx)) goto err;

	if(!BN_mod_mul(&ret, &y, &f, dh->p, ctx)) goto err;

	j=BN_num_bytes(&ret);
	i=BN_bn2bin(&ret,&(to[num-j]));
	for (k1=0; k1<(num-i); k1++)
		to[k1]=0;

	r=num;
err:
	if (ctx != NULL) BN_CTX_free(ctx);
	BN_clear_free(&y);
	BN_clear_free(&f);
	BN_clear_free(&ret);
	if (buf != NULL) 
		{
		op_memset(buf,0,num);
		OPENSSL_free(buf);
		}
	return(r);
}

int ElGamal_decrypt(int flen, const unsigned char *from, unsigned char *to, BIGNUM *a, DH *dh)
{
	BIGNUM y,y1,f,ret;
	int j,num=0,r= -1;
	unsigned char *buf=NULL;
	BN_CTX *ctx=NULL;

	BN_init(&y);
	BN_init(&y1);
	BN_init(&f);
	BN_init(&ret);
	if(BN_bin2bn(from, flen, &f) != &f) goto err;
	if ((ctx=BN_CTX_new()) == NULL) goto err;


	num=BN_num_bytes(dh->p);
	if ((buf=(unsigned char *)OPENSSL_malloc(num)) == NULL)
		{
		ElGamal_err(ELGAMAL_F_ELGAMAL_ENCRYPT,ERR_R_MALLOC_FAILURE);
		goto err;
		}


	if (BN_ucmp(&f, dh->p) >= 0)
		{
		ElGamal_err(ELGAMAL_F_ELGAMAL_DECRYPT,ELGAMAL_R_DATA_TOO_LARGE_FOR_MODULUS);
		goto err;
		}

	if(!BN_mod_exp(&y1, a, dh->priv_key, dh->p, ctx)) goto err;
	if(!BN_mod_inverse(&y, &y1, dh->p, ctx)) goto err;
	if(!BN_mod_mul(&ret, &y, &f, dh->p, ctx)) goto err;

	j=BN_bn2bin(&ret,buf); /* j is only used with no-padding mode */

	r = RSA_padding_check_PKCS1_type_2(to,num,buf,j,num);
	if (r < 0)
		ElGamal_err(ELGAMAL_F_ELGAMAL_DECRYPT,ELGAMAL_R_PADDING_CHECK_FAILED);

err:
	if (ctx != NULL) BN_CTX_free(ctx);
	BN_clear_free(&y1);
	BN_clear_free(&y);
	BN_clear_free(&f);
	BN_clear_free(&ret);
	if (buf != NULL) 
		{
		op_memset(buf,0,num);
		OPENSSL_free(buf);
		}
	return(r);
}


static int ElGamal_perform_sign(int flen, const unsigned char *from, unsigned char *to, BIGNUM **a, DH *dh)
{
	BIGNUM y, y1,f, k, p1,ret;
	int i,j,k1,l,num=0,r= -1;
	unsigned char *buf=NULL;
	BN_CTX *ctx=NULL;

	if(a== NULL)
		return(-1);

	if(*a == NULL)
		*a = BN_new();
	if(*a == NULL)
		return(-1);

	BN_init(&k);
	BN_init(&p1);
	BN_init(&y);
	BN_init(&y1);
	BN_init(&f);
	BN_init(&ret);
	if ((ctx=BN_CTX_new()) == NULL) goto err;
	num=BN_num_bytes(dh->p);
	if ((buf=(unsigned char *)OPENSSL_malloc(num)) == NULL)
		{
		ElGamal_err(ELGAMAL_F_ELGAMAL_ENCRYPT,ERR_R_MALLOC_FAILURE);
		goto err;
		}

	if(!BN_sub(&p1, dh->p, BN_value_one())) goto err;

	l = dh->length ? dh->length : BN_num_bits(dh->p)-1; /* secret exponent length */
	/* Find a number relatively prime with (p-1) */
	do{
		if (!BN_rand(&k, l, 0, 0)) goto err;
		if(!BN_gcd(&y, &k, &p1, ctx)) goto err;
	}while(BN_cmp(&y, BN_value_one()) != 0);
	
	if(!BN_mod_exp(*a, dh->g, &k, dh->p, ctx)) goto err;

	if(RSA_padding_add_PKCS1_type_2(buf,num,from,flen)<=0) goto err;

	if(BN_bin2bn(buf, num, &f) == NULL)
		goto err;

	if(!BN_mod_mul(&y, dh->priv_key, *a, &p1, ctx)) goto err;

#ifdef __USE_OPENSSL_097__
	if(!BN_mod_sub(&y1, &f, &y, &p1, ctx)) goto err;
#else
	if(!BN_sub(&ret, &f, &y)) goto err;
	if(!BN_mod(&y1, &ret, &p1, ctx)) goto err;
#endif

	if(!BN_mod_inverse(&y, &k, &p1, ctx)) goto err;

	if(!BN_mod_mul(&ret, &y1, &y, &p1, ctx)) goto err;

	j=BN_num_bytes(&ret);
	i=BN_bn2bin(&ret,&(to[num-j]));
	for (k1=0; k1<(num-i); k1++)
		to[k1]=0;

	r=num;
err:
	if (ctx != NULL) BN_CTX_free(ctx);
	BN_clear_free(&k);
	BN_clear_free(&y1);
	BN_clear_free(&y);
	BN_clear_free(&f);
	BN_clear_free(&ret);
	if (buf != NULL) 
	{
		op_memset(buf,0,num);
		OPENSSL_free(buf);
	}

	return r;
}

int ElGamal_sign(int type, const unsigned char *m, unsigned int m_len,
	     unsigned char *sigret, unsigned int *siglen, BIGNUM **a, DH *dh)
	{
	X509_SIG sig;
	ASN1_TYPE parameter;
	int i,j,ret=1;
	unsigned char *p, *tmps = NULL;
	const unsigned char *s = NULL;
	X509_ALGOR algor;
	ASN1_OCTET_STRING digest;

	if(a == NULL)
		return 0;

	{
		sig.algor= &algor;
		sig.algor->algorithm=OBJ_nid2obj(type);
		if (sig.algor->algorithm == NULL)
			{
			ElGamal_err(ELGAMAL_F_ELGAMAL_SIGN,ELGAMAL_R_UNKNOWN_ALGORITHM_TYPE);
			return(0);
			}
		if (sig.algor->algorithm->length == 0)
			{
			ElGamal_err(ELGAMAL_F_ELGAMAL_SIGN,ELGAMAL_R_THE_ASN1_OBJECT_IDENTIFIER_IS_NOT_KNOWN_FOR_THIS_MD);
			return(0);
			}
		parameter.type=V_ASN1_NULL;
		parameter.value.ptr=NULL;
		sig.algor->parameter= &parameter;

		sig.digest= &digest;
		sig.digest->data=(unsigned char *)m; /* TMP UGLY CAST */
		sig.digest->length=m_len;

		i=i2d_X509_SIG(&sig,NULL);
	}
	j=DH_size(dh);
	if ((i-RSA_PKCS1_PADDING) > j)
		{
		ElGamal_err(ELGAMAL_F_ELGAMAL_SIGN,ELGAMAL_R_DIGEST_TOO_BIG_FOR_KEY);
		return(0);
		}
	tmps=(unsigned char *)OPENSSL_malloc((unsigned int)j+1);
	if (tmps == NULL)
		{
		ElGamal_err(ELGAMAL_F_ELGAMAL_SIGN,ERR_R_MALLOC_FAILURE);
		return(0);
		}
	p=tmps;
	i2d_X509_SIG(&sig,&p);
	s=tmps;


	i = ElGamal_perform_sign(i, s, sigret, a, dh);

	if (i <= 0)
		ret=0;
	else
		*siglen=i;

	op_memset(tmps,0,(unsigned int)j+1);
	OPENSSL_free(tmps);

	return(ret);
}

static int ElGamal_perform_verify(const unsigned char *from, int flen, unsigned char *ref, unsigned int ref_len, BIGNUM *a, DH *dh)
{
	BIGNUM s1, s2, y, y1,f;
	int num=0,r= 0;
	unsigned char *buf=NULL;
	BN_CTX *ctx=NULL;

	if(a== NULL)
		return(0);

	BN_init(&s1);
	BN_init(&s2);
	BN_init(&y);
	BN_init(&y1);
	BN_init(&f);
	if ((ctx=BN_CTX_new()) == NULL) goto err;
	num=BN_num_bytes(dh->p);
	if ((buf=(unsigned char *)OPENSSL_malloc(num)) == NULL)
		{
		ElGamal_err(ELGAMAL_F_ELGAMAL_VERIFY,ERR_R_MALLOC_FAILURE);
		goto err;
		}


	if(RSA_padding_add_PKCS1_type_2(buf,num,from,flen)<=0) goto err;

	if(BN_bin2bn(buf, num, &f) == NULL)
		goto err;

	/* g^M mod p */
	if(!BN_mod_exp(&s1, dh->g, &f, dh->p, ctx)) goto err;

	/* y^a mod p */
	if(!BN_mod_exp(&y, dh->pub_key, a, dh->p, ctx)) goto err;

	if(BN_bin2bn(ref, ref_len, &f) == NULL)
		goto err;

	/* a^b mod p */
	if(!BN_mod_mul(&y1, a, &f , dh->p, ctx)) goto err;

	/* (y^a)*(a^b) mod p */
	if(!BN_mod_mul(&s2, &y, &y1, dh->p, ctx)) goto err;

	if(BN_cmp(&s1, &s2) == 0)
		r = 1;

err:
	if (ctx != NULL) BN_CTX_free(ctx);
	BN_clear_free(&s1);
	BN_clear_free(&s2);
	BN_clear_free(&y);
	BN_clear_free(&y1);
	BN_clear_free(&f);
	if (buf != NULL) 
	{
		op_memset(buf,0,num);
		OPENSSL_free(buf);
	}

	return r;
}

int ElGamal_verify(int dtype, const unsigned char *m, unsigned int m_len,
	     unsigned char *sigbuf, unsigned int siglen, BIGNUM *a, DH *dh)
	{
	X509_SIG sig;
	ASN1_TYPE parameter;
	int i,j,ret=1;
	unsigned char *p, *tmps = NULL;
	const unsigned char *s = NULL;
	X509_ALGOR algor;
	ASN1_OCTET_STRING digest;

	if(a == NULL)
		return 0;

	if (siglen > (unsigned int)DH_size(dh))
		{
		ElGamal_err(ELGAMAL_F_ELGAMAL_VERIFY,ELGAMAL_R_WRONG_SIGNATURE_LENGTH);
		return(0);
		}

	{
		sig.algor= &algor;
		sig.algor->algorithm=OBJ_nid2obj(dtype);
		if (sig.algor->algorithm == NULL)
			{
			ElGamal_err(ELGAMAL_F_ELGAMAL_VERIFY,ELGAMAL_R_UNKNOWN_ALGORITHM_TYPE);
			return(0);
			}
		if (sig.algor->algorithm->length == 0)
			{
			ElGamal_err(ELGAMAL_F_ELGAMAL_VERIFY,ELGAMAL_R_THE_ASN1_OBJECT_IDENTIFIER_IS_NOT_KNOWN_FOR_THIS_MD);
			return(0);
			}
		parameter.type=V_ASN1_NULL;
		parameter.value.ptr=NULL;
		sig.algor->parameter= &parameter;

		sig.digest= &digest;
		sig.digest->data=(unsigned char *)m; /* TMP UGLY CAST */
		sig.digest->length=m_len;

		i=i2d_X509_SIG(&sig,NULL);
	}
	j=DH_size(dh);
	if ((i-RSA_PKCS1_PADDING) > j)
		{
		ElGamal_err(ELGAMAL_F_ELGAMAL_VERIFY,ELGAMAL_R_DIGEST_TOO_BIG_FOR_KEY);
		return(0);
		}
	tmps=(unsigned char *)OPENSSL_malloc((unsigned int)j+1);
	if (tmps == NULL)
		{
		ElGamal_err(ELGAMAL_F_ELGAMAL_VERIFY,ERR_R_MALLOC_FAILURE);
		return(0);
		}
	p=tmps;
	i2d_X509_SIG(&sig,&p);
	s=tmps;

	ret = ElGamal_perform_verify(s, i, sigbuf, siglen, a, dh);

	op_memset(tmps,0,(unsigned int)j+1);
	OPENSSL_free(tmps);

	return(ret);
}


#endif

