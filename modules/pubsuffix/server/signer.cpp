/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

/* This file is a Python extension */

#include <Python.h>
#include <stdio.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

typedef int BOOL;
#define FALSE 0
#define TRUE 1

static PyObject *signer_error_nofile = NULL;
static PyObject *signer_error_nomemory = NULL;
static PyObject *signer_error_noalg = NULL;
static PyObject *signer_error_generic = NULL;

static unsigned long SignBinaryData(unsigned char **target, const unsigned char *buffer, unsigned int buf_len, EVP_PKEY *key, int sig_alg);

/** def SignPostfix(prefix, body, keyfile, alg)
 *
 *	Signs body using the hash defined by alg and the key in keyfile,
 *	and returns a concatenated string of prefix, base64 encoded signature, crlf, and the body string
 *
 *	prefix: The string to add before the signature
 *	body:	The data to be signed
 *	keyfile:	The name and path of the PEM encoded private key 
 *	alg: string defining the hash algorithm to use, must match the names used by OpenSSL, e.g "sha256"
 *
 *	In case of errors, will throw an exception.
 */
static PyObject *SignPostfix(PyObject *self, PyObject *args)
{
	const char *prefix = NULL;
	const char *body = NULL;
	const char *keyfile = NULL;
	const char *alg = NULL;

	if(!PyArg_ParseTuple(args, "ssss", &prefix, &body, &keyfile, &alg))
		return NULL;

	if(keyfile == NULL || !*keyfile)
	{
		PyErr_SetString(signer_error_nofile,"No keyfile specified");
		return NULL;
	}

	if(prefix == NULL)
		prefix = "";

	if(body == NULL)
		prefix = "";

	if(alg == NULL || !*alg )
		alg = "sha256";

	BOOL ok = FALSE;
	EVP_PKEY *pkey = NULL;
	BIO *source = NULL;
	BIO *output_target = NULL;
	unsigned char *trgt = NULL;
	unsigned long trgt_len = 0;
	unsigned char *temp_buffer=NULL;
	int sig_alg = OBJ_sn2nid(alg);
	if(sig_alg == NID_undef)
		sig_alg = OBJ_ln2nid(alg);

	if(sig_alg == NID_undef)
	{
		PyErr_SetString(signer_error_noalg,"Unknown algorithm");
		return NULL;
	}

	EVP_ENCODE_CTX base_64;

	source = BIO_new_file(keyfile, "r");
	if(!source)
	{
		PyErr_SetString(signer_error_nofile,"Could not open keyfile");
		return NULL;
	}

	pkey = PEM_read_bio_PrivateKey(source, NULL, NULL, NULL);
	BIO_free(source);
	source = NULL;

	if(pkey == NULL)
	{
		PyErr_SetString(signer_error_nofile,"Could not read key");
		return NULL;
	}

	do{
		output_target = BIO_new(BIO_s_mem());
		if(!output_target)
		{
			PyErr_SetString(signer_error_nomemory,"Output buffer allocation failed");
			break;
		}

		if(!BIO_puts(output_target, prefix))
		{
			PyErr_SetString(signer_error_nomemory,"Writing to output buffer failed");
			break;
		}

		// Construct signature
		trgt_len = SignBinaryData(&trgt, (unsigned char *)body, strlen(body), pkey, sig_alg);
		if(trgt == NULL || trgt_len == 0)
		{
			PyErr_SetString(signer_error_nomemory,"Signing failed");
			break;
		}

		int outl =0;
		temp_buffer = new unsigned char[((trgt_len+2)/3)*4+ (trgt_len/48)*2 +10];
		if(temp_buffer == NULL)
		{
			PyErr_SetString(signer_error_nomemory,"Allocation of base64 conversion buffer failed");
			break;
		}
		
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

		if(!BIO_write(output_target, temp_buffer, n))
		{
			PyErr_SetString(signer_error_nomemory,"Writing to output buffer failed");
			break;
		}

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

		if(!BIO_write(output_target, temp_buffer, n))
		{
			PyErr_SetString(signer_error_nomemory,"Writing to output buffer failed");
			break;
		}

		delete [] temp_buffer;
		temp_buffer = NULL;

		if(!BIO_puts(output_target, "\r\n"))
		{
			PyErr_SetString(signer_error_nomemory,"Writing to output buffer failed");
			break;
		}

		if(!BIO_puts(output_target, body))
		{
			PyErr_SetString(signer_error_nomemory,"Writing to output buffer failed");
			break;
		}

		ok = TRUE;
	}while(0);

	EVP_PKEY_free(pkey);
	pkey = NULL;
	delete [] temp_buffer;
	temp_buffer = NULL;
	delete [] trgt;
	trgt = NULL;

	if(!ok)
		return NULL;

	unsigned char *buffer = NULL;
	unsigned long buffer_len =0;

	// Get the memory buffer
	buffer_len = BIO_get_mem_data(output_target, &buffer);
	if(buffer == NULL || buffer_len == 0)
	{
		PyErr_SetString(signer_error_nomemory,"Signing failed");
		BIO_free(output_target);
		output_target = NULL;
		return NULL;
	}

	PyObject *return_val = Py_BuildValue("s#", buffer, buffer_len);

	BIO_free(output_target);
	output_target = NULL;

	return return_val;
}

/** Python method definition */
static PyMethodDef  signerfuncs[] = {
	{"SignPostfix", SignPostfix, METH_VARARGS, "Sign a body using specified key and method"},
	{NULL, NULL, 0, NULL} /* End of list */
};

/** Python initiator */
extern "C" PyMODINIT_FUNC
initsigner(void)
{
	OpenSSL_add_all_algorithms();
	if(ERR_get_error())
		return;
	
	PyObject *module = Py_InitModule("signer", signerfuncs);

	if(!module)
		return;

	signer_error_nofile = PyErr_NewException("signer.NoKeyFile", NULL, NULL);
	Py_INCREF(signer_error_nofile);
	PyModule_AddObject(module, "error", signer_error_nofile);

	signer_error_nomemory = PyErr_NewException("signer.OutOfMemory", NULL, NULL);
	Py_INCREF(signer_error_nomemory);
	PyModule_AddObject(module, "error", signer_error_nomemory);

	signer_error_noalg = PyErr_NewException("signer.NoSignerAlg", NULL, NULL);
	Py_INCREF(signer_error_noalg);
	PyModule_AddObject(module, "error", signer_error_noalg);

	signer_error_generic = PyErr_NewException("signer.AnError", NULL, NULL);
	Py_INCREF(signer_error_generic);
	PyModule_AddObject(module, "error", signer_error_generic);

}

/** Create signature */
static unsigned long SignBinaryData(unsigned char **target, const unsigned char *buffer, unsigned int buf_len, EVP_PKEY *key, int sig_alg)
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

