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
#include <time.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#include "modules/rootstore/server/types.h"
#include "modules/rootstore/rootstore_base.h"
#include "modules/rootstore/server/api.h"

#define BUFFER_SIZE 16*1024

/** Create a DER version of a object sequence */
int i2d_OBJ_seq(STACK_OF(ASN1_OBJECT) *sk, unsigned char **pp)
{
	return i2d_ASN1_SET_OF_ASN1_OBJECT(sk, pp, i2d_ASN1_OBJECT,
							V_ASN1_SEQUENCE,
						   V_ASN1_UNIVERSAL,
						   IS_SEQUENCE);
}

/** Write the EV OIDs for this certificate */
BOOL ProduceEV_OID_list(BIO *target, const DEFCERT_st *entry, char *buffer, unsigned int buf_len, EVP_PKEY *key, int sig_alg)
{
	if(!entry)
		return FALSE;

	if(!entry->dependencies || !entry->dependencies->EV_OIDs || !*entry->dependencies->EV_OIDs)
		return TRUE;

	time_t current_time = time(NULL);
	struct tm *time_fields = gmtime(&current_time);

	if(!time_fields)
		return FALSE;

	// Check expiration of EV
	if(entry->dependencies->EV_valid_to_year < (unsigned int) time_fields->tm_year+1900)
		return TRUE; // Ignore

	if(entry->dependencies->EV_valid_to_year == (unsigned int) time_fields->tm_year+1900 && 
			entry->dependencies->EV_valid_to_month < (unsigned int) time_fields->tm_mon+1)
		return TRUE; // Ignore


	unsigned char *oids = NULL;
	unsigned char *sig = NULL;
	ASN1_BIT_STRING *str = NULL;
	STACK_OF(ASN1_OBJECT) *policy_list = sk_ASN1_OBJECT_new_null();
	if(!policy_list)
		return FALSE;

	BOOL ret= FALSE;
	do{
		const char * const *oid_pos = entry->dependencies->EV_OIDs;

		// Write the text form
		if(!BIO_puts(target, "<ev-oid-text>\n"))
			break;
		while(*oid_pos != NULL)
		{
			ret = FALSE;

			// for each entry
			if(!WriteXML(target, "ev-oid-entry", *oid_pos, buffer, buf_len))
				break;
			// Convert text form to OBJect
			ASN1_OBJECT *policy_id = OBJ_txt2obj(*oid_pos, TRUE);
		
			if(!policy_id)
				break;

			// And store it in the list
			if(!sk_ASN1_OBJECT_push(policy_list, policy_id))
				break;

			policy_id=NULL;
			oid_pos++;
			ret = TRUE;
		}

		if(!ret)
			break;
		ret = FALSE;

		if(!BIO_puts(target, "</ev-oid-text>\n"))
			break;

		// Convert the OID list to a DER encoded list
		unsigned long len = i2d_OBJ_seq(policy_list, NULL);
		if(len == 0)
			break;

		oids = new unsigned char[len];
		unsigned char *oids_p = oids;

		if(oids == NULL)
			break;

		len = i2d_OBJ_seq(policy_list, &oids_p);
		if(len == 0)
			break;

#ifdef DEBUG_MODE
		BIO_puts(target, "<!--\n");
	
		for(unsigned long j =0; j< len; j++)
			BIO_printf(target, " %02x", oids[j]);

		BIO_puts(target, "\n-->\n");
#endif

		sk_ASN1_OBJECT_pop_free(policy_list, ASN1_OBJECT_free);
		policy_list = NULL;

		sig = NULL;
		unsigned long sig_len = 0;

		// Sign the data
		sig_len = SignBinaryData(&sig, oids, len, key, sig_alg);
		if(sig == NULL)
			break;

		ASN1_BIT_STRING *str = ASN1_BIT_STRING_new();
		if(str == NULL)
			break;

		// Create a binary string of the result 
		if(!ASN1_BIT_STRING_set(str, sig, sig_len))
			break;

		delete [] sig;
		sig = NULL;

		sig_len = i2d_ASN1_BIT_STRING(str, NULL);
		if(sig_len == 0)
			break;

		sig = new unsigned char[sig_len];
		if(sig == NULL)
			break;
		unsigned char *sig2 = sig;
		sig_len = i2d_ASN1_BIT_STRING(str, &sig2);
		if(sig_len == 0)
			break;

#ifdef DEBUG_MODE
		BIO_puts(target, "<!--\n");

		for(unsigned long j =0; j< sig_len; j++)
			BIO_printf(target, " %02x", sig[j]);

		BIO_puts(target, "\n-->\n");
#endif

		ASN1_BIT_STRING_free(str);
		str = NULL;

		// Write the Base64 encoded signed binary representation to the file
		BIO_puts(target, "<ev-oids>\n");

		unsigned int i, read_len=0;
		int outl;

		EVP_ENCODE_CTX base_64;
		EVP_EncodeInit(&base_64);
		for(i=0; i< len; i+= read_len)
		{
			ret = FALSE;
			read_len = (buf_len/4)*3;
			if(i+read_len > len)
				read_len = len-i;
			outl =0;
			EVP_EncodeUpdate(&base_64, (unsigned char *) buffer, &outl, oids+i, read_len);
			if(outl && !BIO_write(target, buffer, outl))
				break;
			ret = TRUE;
		}
		if(!ret)
			break;
		for(i=0; i< sig_len; i+= read_len)
		{
			ret = FALSE;
			read_len = (buf_len/4)*3;
			if(i+read_len > sig_len)
				read_len = sig_len-i;
			outl =0;
			EVP_EncodeUpdate(&base_64, (unsigned char *) buffer, &outl, sig+i, read_len);
			if(outl && !BIO_write(target, buffer, outl))
				break;
			ret = TRUE;
		}
		if(!ret)
			break;
		ret = FALSE;

		outl =0;
		EVP_EncodeFinal(&base_64, (unsigned char *) buffer, &outl);
		if(outl && !BIO_write(target, buffer, outl))
			break;
		if(!BIO_puts(target, "\n</ev-oids>\n"))
			break;
		ret = TRUE;
	}while(0);

	if(!policy_list)
		sk_ASN1_OBJECT_pop_free(policy_list, ASN1_OBJECT_free);
	if(str)
		ASN1_BIT_STRING_free(str);
	delete [] oids;
	delete [] sig;
	return ret;
}

// Write an EV OID entry to the EV OID file
BOOL ProduceEV_XML(BIO *target, const DEFCERT_st *entry, EVP_PKEY *key, int sig_alg)
{
	if(!entry->dependencies || !entry->dependencies->EV_OIDs || !*entry->dependencies->EV_OIDs)
		return TRUE;

	time_t current_time = time(NULL);
	struct tm *time_fields = gmtime(&current_time);

	if(!time_fields)
		return FALSE;

	// Check expiration of EV
	if(entry->dependencies->EV_valid_to_year < (unsigned int) time_fields->tm_year+1900)
		return TRUE; // Ignore

	if(entry->dependencies->EV_valid_to_year == (unsigned int) time_fields->tm_year+1900 &&
			entry->dependencies->EV_valid_to_month < (unsigned int) time_fields->tm_mon+1)
		return TRUE; // Ignore

	BOOL ret = FALSE;
	int outl;
	EVP_ENCODE_CTX base_64;
	X509 *cert=NULL;
	char *name=NULL;
	unsigned char *buffer = new unsigned char[BUFFER_SIZE+1];
	unsigned char *buffer1 = new unsigned char[BUFFER_SIZE+1];
	unsigned char *name_buf = NULL;

	do{
		if(buffer == NULL || buffer1 == NULL)
			break;

		if(!BIO_puts(target, "<issuer>\n"))
			break;

		const unsigned char *data = entry->dercert_data;
		cert = d2i_X509(NULL, &data, entry->dercert_len);
		if(cert == NULL)
			break;

		// Write oneline issuer name
		name = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
		if(!name)
			break;

		if(!WriteXML(target, "issuer-name", name, (char *) buffer, BUFFER_SIZE))
			break;

		OPENSSL_free(name);
		name = NULL;

		if(!BIO_puts(target, "<issuer-id>\n"))
			break;

		// Write the Base64 encoded subject name 
		int name_len= i2d_X509_NAME(X509_get_subject_name(cert), NULL);
		name_buf = new unsigned char[name_len];
		unsigned char *name_buf1 = name_buf;

		if(i2d_X509_NAME(X509_get_subject_name(cert), &name_buf1) != name_len)
			break;

		EVP_EncodeInit(&base_64);
		outl =0;
		EVP_EncodeUpdate(&base_64, buffer, &outl, name_buf, name_len);
		if(outl && !BIO_write(target, buffer, outl))
			break;
		outl =0;
		EVP_EncodeFinal(&base_64, buffer, &outl);
		if(outl && !BIO_write(target, buffer, outl))
			break;
		if(!BIO_puts(target, "\n</issuer-id>\n"))
			break;

		// Write the key associated with the EV OIDs, just in case two certificates have the exact same name
		if(!BIO_puts(target, "<issuer-key>\n"))
			break;

		unsigned int pk_dgst_len =0;
		X509_pubkey_digest(cert,  EVP_sha1(), buffer1, &pk_dgst_len);
		EVP_EncodeInit(&base_64);
		outl =0;
		EVP_EncodeUpdate(&base_64, buffer, &outl, buffer1, pk_dgst_len );
		if(outl && !BIO_write(target, buffer, outl))
			break;
		outl =0;
		EVP_EncodeFinal(&base_64, buffer, &outl);
		if(outl && !BIO_write(target, buffer, outl))
			break;
		if(!BIO_puts(target, "\n</issuer-key>\n"))
			break;

		// Write OID list
		if(!ProduceEV_OID_list(target, entry, (char *) buffer, BUFFER_SIZE,key,sig_alg))
			break;

		if(!BIO_puts(target, "</issuer>\n"))
			break;
		ret = TRUE;
	}while(0);

	if(cert)
		X509_free(cert);
	if(name)
		OPENSSL_free(name);
	delete [] buffer;
	delete [] buffer1;
	delete [] name_buf;

	return TRUE;
}

/** Write the EV OID file */
BOOL ProduceEVOID_XML_File(const char *version, EVP_PKEY *key, int sig_alg)
{
	unsigned int i;

	if(key == NULL)
		return FALSE;

	// Create file
	BIO *target1 = NULL;
	BIO *target = GenerateFile(version, "", "ev-oids");
	if(!target)
		return FALSE;

	BOOL ret = FALSE;
	do {
		BIO *target1 = BIO_new(BIO_s_mem());
		if(!target1)
			break;

		// Header, not signed
		if(!BIO_puts(target, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"))
			break;
		
		// start of signed part
		if(!BIO_puts(target1, "-->\n<!-- Copyright (C) 2008-2009 Opera Software ASA. All Rights Reserved. -->\n\n<ev-oid-list>\n"))
			break;
		
		// For each root write the identifying info and the EV OIDs, if any. 
		for(i=0; i< GetRootCount(); i++)
		{
			ret = FALSE;
			if(!ProduceEV_XML(target1, GetRoot(i),key,sig_alg))
				break;
			ret = TRUE;
		}
		if(!ret)
			break;
		ret = FALSE;

		if(!BIO_puts(target1, "</ev-oid-list>\n"))
			break;
		if(!BIO_flush(target1))
			break;

		unsigned char *buffer = NULL;
		unsigned long buffer_len =0;

		// Extract and sign
		buffer_len = BIO_get_mem_data(target1, &buffer);
		if(buffer == NULL || buffer_len == 0)
			break;

		if(!SignXMLData(target, buffer, buffer_len, key, sig_alg))
			break;

		if(!BIO_flush(target))
			break;
		
		ret = TRUE;
	}
	while(0);

	if(target1)
		BIO_free(target1);
	BIO_free(target);
	target = NULL;
	target1 = NULL;

	return ret;
}
