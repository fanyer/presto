/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
*  Code copied from openssl and modified by haavardm, to make the key generation asynchronous
*
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/* crypto/rsa/rsa_gen.c */
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
#ifdef LIBOPEAY_ASYNCHRONOUS_KEYGENERATION
#include <openssl/cryptlib.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include "modules/libopeay/addon/asynch_rsa_gen.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/crypto.h"
#include "modules/libopeay/openssl/buffer.h"
#include "modules/libopeay/openssl/bn.h"
#include "modules/libopeay/openssl/evp.h"
#include "modules/libopeay/openssl/rsa.h"
#include "modules/libopeay/openssl/objects.h"
#include "modules/libopeay/openssl/x509.h"

#define SSL_SHA_LENGTH 20

static int GenerateKey_update(int a,int b,void* HDlg)
{ 
	return 0;
}



static int rsa_builtin_keygen_asynch(RSA *rsa, int bits, BIGNUM *e_value, BN_GENCB *cb, RSA_KEYGEN_STATE_HANDLE *state_handle);


/*RSA_generate_key_ex_asynch is for testing only*/
int RSA_generate_key_ex_asynch(RSA *rsa, int bits, BIGNUM *e_value, BN_GENCB *cb, RSA_KEYGEN_STATE_HANDLE * state_handle)
	{
	state_handle->rsa = rsa;
	state_handle->e_value = *e_value;
	state_handle->cb = *cb;
	state_handle->bits = bits;
	
	state_handle->error=FALSE;
	
	state_handle->state = RSA_KEYGEN_INITIALISING;
	state_handle->progress = 0;
	if(state_handle->rsa->meth->rsa_keygen)
		{
		int ok = state_handle->rsa->meth->rsa_keygen(state_handle->rsa, state_handle->bits, &state_handle->e_value, &state_handle->cb);
		state_handle->state = RSA_KEYGEN_DONE;		
		return ok;
		}
	return 1;	
	}

RSA_KEYGEN_STATE_HANDLE *RSA_generate_key_ex_asynch_init(int bits)
	{
	RSA_KEYGEN_STATE_HANDLE *state_handle = (RSA_KEYGEN_STATE_HANDLE*) OPENSSL_malloc(sizeof(RSA_KEYGEN_STATE_HANDLE));
	if(state_handle == NULL)
		return NULL;

	BN_init(&state_handle->e_value);
	state_handle->error = FALSE;
		
	if(!BN_set_word(&state_handle->e_value, RSA_F4))
	{
    	OPENSSL_free(state_handle);	
    	return NULL;
	}

	state_handle->rsa = RSA_new();

    if(state_handle->rsa == NULL)
    {
    	OPENSSL_free(state_handle);	
    	return NULL;
    }
		
	BN_GENCB_set_old(&state_handle->cb, GenerateKey_update, 0L);		
		
	state_handle->state = RSA_KEYGEN_INITIALISING;
	state_handle->progress = 0;
	state_handle->bits = bits;

	if(state_handle->rsa->meth->rsa_keygen)
		{
		state_handle->rsa->meth->rsa_keygen(state_handle->rsa, state_handle->bits, &state_handle->e_value, &state_handle->cb);
		state_handle->state = RSA_KEYGEN_DONE;		
		return state_handle;
		}
	return state_handle;
	}

int RSA_generate_key_ex_asynch_runslice(RSA_KEYGEN_STATE_HANDLE * state_handle)
	{
	if 	(state_handle->state == RSA_KEYGEN_DONE)
		{
		return 1;
		}

	int ret = rsa_builtin_keygen_asynch(state_handle->rsa, state_handle->bits, &state_handle->e_value, &state_handle->cb, state_handle);
	state_handle->progress =(int) ((100*(int)state_handle->state )/ (int)RSA_KEYGEN_DONE);
		if (state_handle->progress > 100)
			{
			state_handle->progress = 100;
			}
	return ret;
	}
	
static int rsa_builtin_keygen_asynch(RSA *rsa, int bits, BIGNUM *e_value, BN_GENCB *cb, RSA_KEYGEN_STATE_HANDLE *state_handle)
	{
	BIGNUM *r0,*r1,*r2,*tmp;
	int bitsp,bitsq,ok=-1,n,degenerate;
	BN_CTX *ctx = NULL;
	
	if (state_handle->state == RSA_KEYGEN_INITIALISING)
		{
		state_handle->ctx = BN_CTX_new();
		if ( state_handle->ctx == NULL) goto err;
		BN_CTX_start(state_handle->ctx);
			
		state_handle->r0 = BN_CTX_get(state_handle->ctx);
		state_handle->r1 = BN_CTX_get(state_handle->ctx);
		state_handle->r2 = BN_CTX_get(state_handle->ctx);
		state_handle->r3 = BN_CTX_get(state_handle->ctx);
		
		if (state_handle->r3 == NULL) goto err;


		state_handle->bitsp = (bits+1)/2;
		state_handle->bitsq = bits - state_handle->bitsp;

		state_handle->ok = -1;
		state_handle->n = 0;

		state_handle->tmp = NULL;	

		state_handle->degenerate = 0;
		}
		
	degenerate = state_handle->degenerate;	
	ctx = state_handle->ctx;
	r0 = state_handle->r0;
	r1 = state_handle->r1;
	r2 = state_handle->r2;


	bitsp = state_handle->bitsp;
	bitsq = state_handle->bitsq;

	ok = state_handle->ok;
	n = state_handle->n;

	tmp = state_handle->tmp;
		
	if (state_handle->state == RSA_KEYGEN_INITIALISING)
		{
		/* We need the RSA components non-NULL */
		if(!rsa->n && ((rsa->n=BN_new()) == NULL)) goto err;
		if(!rsa->d && ((rsa->d=BN_new()) == NULL)) goto err;
		if(!rsa->e && ((rsa->e=BN_new()) == NULL)) goto err;
		if(!rsa->p && ((rsa->p=BN_new()) == NULL)) goto err;
		if(!rsa->q && ((rsa->q=BN_new()) == NULL)) goto err;
		if(!rsa->dmp1 && ((rsa->dmp1=BN_new()) == NULL)) goto err;
		if(!rsa->dmq1 && ((rsa->dmq1=BN_new()) == NULL)) goto err;
		if(!rsa->iqmp && ((rsa->iqmp=BN_new()) == NULL)) goto err;
	
		BN_copy(rsa->e, e_value);
		
		state_handle->state = RSA_KEYGEN_GENERATING_P;
		return 0;
		}
	if (state_handle->state == RSA_KEYGEN_GENERATING_P)
		{
		if(!BN_generate_prime_ex(rsa->p, bitsp, 0, NULL, NULL, cb))
			goto err;
		if (!BN_sub(r2,rsa->p,BN_value_one())) goto err;
		if (!BN_gcd(r1,r2,rsa->e,ctx)) goto err;
		if (BN_is_one(r1)) 
			{
			state_handle->state = RSA_KEYGEN_DONE_GENERATING_P;
			return 0;	
			};
		if(!BN_GENCB_call(cb, 2, n++))
			goto err;
		return 0;
		}

	if (state_handle->state == RSA_KEYGEN_DONE_GENERATING_P)
		{
		if(!BN_GENCB_call(cb, 3, 0))
			goto err;
		state_handle->state = RSA_KEYGEN_GENERATING_Q1;
		state_handle->degenerate = 0;
		return 0;
		}
		/* When generating ridiculously small keys, we can get stuck
		 * continually regenerating the same prime values. Check for
		 * this and bail if it happens 3 times. */

	if (state_handle->state == RSA_KEYGEN_GENERATING_Q1)		 
		{
		if(!BN_generate_prime_ex(rsa->q, bitsq, 0, NULL, NULL, cb))
			goto err;

		if ((BN_cmp(rsa->p, rsa->q) == 0) && (++degenerate < 3))
			{
			state_handle->state = RSA_KEYGEN_GENERATING_Q1;
			}
		else
			{
			state_handle->state = RSA_KEYGEN_GENERATING_Q2;
			}	
		return 0;
		}
					
	if (state_handle->state == RSA_KEYGEN_GENERATING_Q2)		 
		{		
		if(degenerate == 3)
			{
			ok = 0; /* we set our own err */
			RSAerr(RSA_F_RSA_BUILTIN_KEYGEN,RSA_R_KEY_SIZE_TOO_SMALL);
			goto err;
			}
			
		if (!BN_sub(r2,rsa->q,BN_value_one())) goto err;
		if (!BN_gcd(r1,r2,rsa->e,ctx)) goto err;
		if (BN_is_one(r1))
			{
			state_handle->state = RSA_KEYGEN_DONE_GENERATING_Q;
			return 0;	
			}
		if(!BN_GENCB_call(cb, 2, n++))
			goto err;
		state_handle->state = RSA_KEYGEN_GENERATING_Q1;	
		state_handle->degenerate = 0;
		return 0;
		}
		
	if (state_handle->state == RSA_KEYGEN_DONE_GENERATING_Q)
		{
			if(!BN_GENCB_call(cb, 3, 1))
				goto err;
			if (BN_cmp(rsa->p,rsa->q) < 0)
				{
				tmp=rsa->p;
				rsa->p=rsa->q;
				rsa->q=tmp;
				}
		
			/* calculate n */
			if (!BN_mul(rsa->n,rsa->p,rsa->q,ctx)) goto err;
		
			/* calculate d */
			if (!BN_sub(r1,rsa->p,BN_value_one())) goto err;	/* p-1 */
			if (!BN_sub(r2,rsa->q,BN_value_one())) goto err;	/* q-1 */
			if (!BN_mul(r0,r1,r2,ctx)) goto err;	/* (p-1)(q-1) */
			if (!BN_mod_inverse(rsa->d,rsa->e,r0,ctx)) goto err;	/* d */
		
			/* calculate d mod (p-1) */
			if (!BN_mod(rsa->dmp1,rsa->d,r1,ctx)) goto err;
		
			/* calculate d mod (q-1) */
			if (!BN_mod(rsa->dmq1,rsa->d,r2,ctx)) goto err;
		
			/* calculate inverse of q mod p */
			if (!BN_mod_inverse(rsa->iqmp,rsa->q,rsa->p,ctx)) goto err;
		
			ok=1;
			state_handle->state = RSA_KEYGEN_DONE;
		}
err:
	if (ok == -1)
		{
		state_handle->state = RSA_KEYGEN_DONE;
		RSAerr(RSA_F_RSA_BUILTIN_KEYGEN,ERR_LIB_BN);
		ok=0;
		}
	if(ctx)
	{
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
	}

	return ok;
	}

void RSA_keygen_asynch_clean_up(RSA_KEYGEN_STATE_HANDLE * state_handle)
{
	OPENSSL_free(state_handle);
}

unsigned char *
RSA_keygen_asynch_get_public_key(
		enum SSL_Certificate_Request_Format format,
		const char* challenge,
		unsigned char* spki_string,
		unsigned long* spki_len,
		unsigned long* keylength,
		RSA_KEYGEN_STATE_HANDLE* state_handle)
	{
				
	EVP_PKEY *pkey = NULL;
	EVP_MD_CTX sha;
	unsigned char *temp,*buffer=NULL, *ret=NULL;
	X509_PUBKEY *pk;
	ASN1_OBJECT *o;
	X509_ALGOR *a;
	
	unsigned int len1;
	unsigned long len;

	BN_clear_free(&state_handle->e_value);
	
	pkey = EVP_PKEY_new();
	int err = EVP_PKEY_assign_RSA(pkey, state_handle->rsa);
	if (err != 1)
	{
		RSA_free(state_handle->rsa);
		goto err;
	}

    len = i2d_PrivateKey(pkey,NULL);
	if(len <= 0)
	    goto err;

    *keylength = len + SSL_SHA_LENGTH;
	buffer= (unsigned char *) OPENSSL_malloc((size_t) *keylength);
	if(buffer == NULL)
	    goto err;

    temp = buffer + SSL_SHA_LENGTH;
	len1 =len = i2d_PublicKey(pkey,&temp);
	if(len <= 0)
	    goto err;

    EVP_DigestInit(&sha,EVP_sha1());
	EVP_DigestUpdate(&sha,buffer+ SSL_SHA_LENGTH,len1);
	EVP_DigestFinal(&sha,buffer,&len1);

	if(format == SSL_Netscape_SPKAC)
		{
		NETSCAPE_SPKI *spki= NULL;
	
		spki = NETSCAPE_SPKI_new();
		if(spki == NULL) 
			goto err;
      
		pk = spki->spkac->pubkey;
		a= pk->algor;
    
		if (!ASN1_BIT_STRING_set(pk->public_key,buffer + SSL_SHA_LENGTH,len)) goto err;

		if ((o=OBJ_nid2obj(pkey->type)) == NULL) goto err;
		ASN1_OBJECT_free(a->algorithm);
		a->algorithm=o;

	   /* Set the parameter list */
		if ((a->parameter == NULL) || (a->parameter->type != V_ASN1_NULL))
			{
			ASN1_TYPE_free(a->parameter);
			a->parameter=ASN1_TYPE_new();
			if(a->parameter == NULL)
				goto err;
			a->parameter->type=V_ASN1_NULL;
			}
    
		if(challenge != NULL && op_strlen(challenge) > 0)
			ASN1_STRING_set(spki->spkac->challenge,(unsigned char *) challenge,op_strlen(challenge));

		NETSCAPE_SPKI_sign(spki,pkey,EVP_md5());  
		len = i2d_NETSCAPE_SPKI(spki,NULL);

		if(/*len < 0 ||*/ len > *spki_len)
			goto err;
        
	    temp = spki_string;
	    *spki_len = i2d_NETSCAPE_SPKI(spki,&temp);
		
		NETSCAPE_SPKI_free(spki);
		}
	else if(format == SSL_PKCS10)
		{
		X509_REQ *req = NULL;
		X509_NAME_ENTRY *cn_challenge = NULL;

		req = X509_REQ_new();
		if(req == NULL)
			goto err;

		ASN1_INTEGER_set(req->req_info->version, 0);

		pk = req->req_info->pubkey;
		a = pk->algor;

		if (!ASN1_BIT_STRING_set(pk->public_key,buffer + SSL_SHA_LENGTH,len))
			goto err;

		if ((o=OBJ_nid2obj(pkey->type)) == NULL)
			goto err;
		ASN1_OBJECT_free(a->algorithm);
		a->algorithm=o;

	   /* Set the parameter list */
		if ((a->parameter == NULL) || (a->parameter->type != V_ASN1_NULL))
			{
			ASN1_TYPE_free(a->parameter);
			a->parameter=ASN1_TYPE_new();
			if(a->parameter == NULL)
				goto err;
			a->parameter->type=V_ASN1_NULL;
			}

		if(challenge == NULL || *challenge == '\0')
			challenge = "Certificate request generated by Opera";

		if(challenge != NULL && op_strlen(challenge) > 0)
			{
			cn_challenge= X509_NAME_ENTRY_new();
			if(cn_challenge == NULL)
				{
				X509_REQ_free(req);
				goto err;
				}
			cn_challenge->object = OBJ_nid2obj(NID_commonName);
			ASN1_STRING_free(cn_challenge->value);
			cn_challenge->value = ASN1_T61STRING_new();
			if(cn_challenge->value == NULL)
				{
				X509_NAME_ENTRY_free(cn_challenge);
				X509_REQ_free(req);
				goto err;
				}

			ASN1_STRING_set(cn_challenge->value,(unsigned char *) challenge,op_strlen(challenge));

			X509_NAME_add_entry(req->req_info->subject, cn_challenge,-1,-1);
			}

		X509_REQ_sign(req,pkey,EVP_md5());  

		len = i2d_X509_REQ(req,NULL);
		if(/*len < 0 || */len > *spki_len)
			goto err;
        
        temp = spki_string;
        *spki_len = i2d_X509_REQ(req,&temp);
		
		X509_REQ_free(req);
		}

    temp = buffer + SSL_SHA_LENGTH;
    len = i2d_PrivateKey(pkey,&temp);
    if(len != *keylength - SSL_SHA_LENGTH)
        goto err;
    
    ret = buffer;
    buffer = NULL;
err:;
	if(buffer != NULL)
		{
		OPENSSL_free(buffer);
		ret = NULL;
		state_handle->error = TRUE;	
		}

	EVP_PKEY_free(pkey);   	
   	return ret;
	}

#endif //LIBOPEAY_ASYNCHRONOUS_KEYGENERATION
