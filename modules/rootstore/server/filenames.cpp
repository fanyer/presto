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
#include "modules/rootstore/rootstore_base.h"
#include "modules/rootstore/server/api.h"

#define BUFFER_SIZE 16*1024

// Create a filename, based on DEFCERT entry
char *GenerateFileName(unsigned int version_num, const DEFCERT_st *entry, BOOL with_pub_key, const DEFCERT_keyid_item *item)
{
	if(entry->dercert_data == NULL)
		return NULL;
	
	return GenerateFileName(version_num, entry->dercert_data, entry->dercert_len, with_pub_key, item);
}

// Create a filename, based on the actual data for a certificate
char *GenerateFileName(unsigned int version_num, const byte *dercert_data, size_t dercert_len,
						BOOL with_pub_key, const DEFCERT_keyid_item *item)
{
	if(dercert_data == NULL)
		return NULL;

	char *ret = NULL;
	// Work buffer, large enough to hold the largest hash in asciiform in _XXXX form
	char *buffer = new char[((EVP_MAX_MD_SIZE+1)/2)*5+1];
	unsigned char *name = NULL;
	X509 *cert = NULL;

	if(buffer == NULL)
		return NULL;

	do
	{
		// Get the certificate
		const unsigned char *data = dercert_data;
		X509 *cert = d2i_X509(NULL, &data, dercert_len);
		if(cert == NULL)
			break;

		// buffer for name hash
		unsigned char name_hash[EVP_MAX_MD_SIZE+1]; /* ARRAY OK 2009-06-11 yngve */
		EVP_MD_CTX hash;

		// extract name
		int len = i2d_X509_NAME(X509_get_subject_name(cert), &name);
		if(name == NULL)
			break;

		// digest it
		EVP_DigestInit(&hash, EVP_sha256());
		EVP_DigestUpdate(&hash, name, len);
		if(with_pub_key)
		{
			// and if requested the public key
			ASN1_BIT_STRING *key = X509_get0_pubkey_bitstr(cert);
			if(key)
				EVP_DigestUpdate(&hash, key->data, key->length);
		}
		// and a key id
		if(item && item->keyid)
			EVP_DigestUpdate(&hash, item->keyid, item->keyid_len);

		// version number is first part of name
		name_hash[0] = version_num;
		unsigned int md_len = 0;
		// then the digest result
		EVP_DigestFinal(&hash, name_hash+1, &md_len);

		// Hexify the result
		char *id_string = buffer;
		int id_len = md_len+1, i =0;
		
		// first, version number
		if(version_num)
		{
			sprintf(id_string, "%.2X", name_hash[i]);
			id_string+=2;
		}
		// then each pair of bytes
		for(i++ ;i<id_len; i+=2, id_string+=5)
			sprintf(id_string, (i+2 < id_len ? "%.2X%.2X_" : "%.2X%.2X"), name_hash[i], name_hash[i+1]);

		char *short_name = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
		if(short_name)
		{
			printf("%s\n    %s: %s\n         %s\n", short_name, (with_pub_key ? "TRUE" : "FALSE"), 
				(item == NULL ? "No dep" : "dep"), buffer);

			OPENSSL_free(short_name);
		}

		// Return in buffe
		ret = buffer;
	}while(0);

	if(name)
		OPENSSL_free(name);
	name = NULL;
	if(cert)
		X509_free(cert);
	cert = NULL;
	// If no return free buffer
	if(ret == NULL)
		delete [] buffer;
	buffer = NULL;

	return ret;
}

/* Create a filename and open the file, according to the specified cert, key and keyid in a given path*/ 
BIO *GenerateFile(const char *version, unsigned int version_num, const char *path, 
		const byte *dercert_data, size_t dercert_len, 
		BOOL with_pub_key, const DEFCERT_keyid_item *item)
{
	if(dercert_data == NULL)
		return NULL;
	BIO *ret = NULL;
	char *filename = NULL;

	do
	{
		filename = GenerateFileName(version_num, dercert_data, dercert_len, with_pub_key, item);
		if(filename == NULL)
			break;
		ret = GenerateFile(version, path, filename);
	}while(0);

	delete [] filename;

	return ret;
}


/* Create a filename and open the file, according to the specified cert, key and keyid */ 
BIO *GenerateFile(const char *version, unsigned int version_num, const DEFCERT_st *entry, BOOL with_pub_key, const DEFCERT_keyid_item *item)
{
	if(!entry || entry->dercert_data == NULL)
		return NULL;
	
	return GenerateFile(version, version_num, "roots/", entry->dercert_data, entry->dercert_len, with_pub_key, item);
}

