/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include <stdio.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#include "modules/rootstore/server/types.h"
#include "modules/rootstore/server/api.h"

/** Sign the XML data in the buffer, and output the signature and the buffer in the target buffer */ 
BOOL SignXMLData(BIO *target, const unsigned char *buffer, unsigned int buf_len, EVP_PKEY *key, int sig_alg)
{
	if(target == NULL)
		return FALSE;

	if(key == NULL)
		return FALSE;

	if(buffer == NULL || buf_len == 0)
		return FALSE;

	unsigned char *trgt = NULL;
	unsigned long trgt_len = 0;
	unsigned char *temp_buffer=NULL;
	BOOL ret = FALSE;

	EVP_ENCODE_CTX base_64;

	do {
		// Construct signature
		trgt_len = SignBinaryData(&trgt, buffer, buf_len, key, sig_alg);
		if(trgt == NULL || trgt_len == 0)
			break;

		// Output a comment before sig
		if(!BIO_puts(target, "<!-- "))
			break;

		int outl =0;
		temp_buffer = new unsigned char[((trgt_len+2)/3)*4+ (trgt_len/48)*2 +10];
		if(temp_buffer == NULL)
			break;
		
		// Base64 encode the sig
		EVP_EncodeInit(&base_64);
		EVP_EncodeUpdate(&base_64, temp_buffer, &outl, trgt, trgt_len);

        int i=0,n=0;
        for(i=0;i<outl;i++)
        {
        	// remove CRs and  LFs
            if(temp_buffer[i] != '\r' && temp_buffer[i] != '\n')
            {
                if(i!= n)
                    temp_buffer[n] = temp_buffer[i];
                n++;
            }
        }

        // Write sig
		if(n && !BIO_write(target, temp_buffer, n))
			break;

		outl =0;
		
		// Get last of encoded sig
		EVP_EncodeFinal(&base_64, temp_buffer, &outl);
        for(i=n=0;i<outl;i++)
        {
        	// remove CR and LF
            if(temp_buffer[i] != '\r' && temp_buffer[i] != '\n')
            {
                if(i!= n)
                    temp_buffer[n] = temp_buffer[i];
                n++;
            }
        }
        // write rest of sig
		if(n && !BIO_write(target, temp_buffer, n))
			break;
		delete [] temp_buffer;
		temp_buffer = NULL;
		
		// Append a CR
		if(!BIO_puts(target, "\n"))
			break;

		// Write signed data
		if(!BIO_write(target, buffer, buf_len))
			break;

		ret = TRUE;
	}while(0);

	delete [] temp_buffer;
	delete [] trgt;

	return ret;
}

/** Create signature */
unsigned long SignBinaryData(unsigned char **target, const unsigned char *buffer, unsigned int buf_len, EVP_PKEY *key, int sig_alg)
{
	if(target == NULL)
		return 0;
	*target = NULL;

	if(key == NULL)
		return 0;

	if(buffer == NULL || buf_len == 0)
		return 0;

	EVP_MD_CTX ctx;
	EVP_MD_CTX_init(&ctx);
	unsigned long ret=0, target_len=0;
	unsigned char *trgt=NULL;

	target_len = EVP_PKEY_size(key);

	do{
		if(target_len == 0)
			break;

		// Buffer for sig
		trgt = new unsigned char[target_len];
		if(trgt == NULL)
			break;

		// Get Sig digest by the alg
		if( EVP_get_digestbynid(sig_alg) == NULL)
			break;

		// Start signing
		if(!EVP_SignInit(&ctx, EVP_get_digestbynid(sig_alg)))
			break;

		// Do signing
		if(!EVP_SignUpdate(&ctx, buffer, buf_len))
			break;

		unsigned int len_chk = 0;
		
		// Complete signature
		if(!EVP_SignFinal(&ctx, trgt, &len_chk, key))
			break;

		if(len_chk != target_len)
			break;

		*target = trgt;
		trgt = NULL;
		ret = target_len;
	}while(0);


	EVP_MD_CTX_cleanup(&ctx);
	if(trgt)
		delete [] trgt;

	return ret;
}

