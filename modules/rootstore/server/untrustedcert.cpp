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

// Write information about an untrusted certificate
BOOL ProduceUntrustedCertificateXML(BIO *target, const DEFCERT_UNTRUSTED_cert_st *entry, EVP_PKEY *key, int sig_alg)
{
	unsigned int i, read_len=0;
	int outl;
	if(target == NULL)
		return FALSE;
	if(entry->dercert_data == NULL)
		return TRUE;

	EVP_ENCODE_CTX base_64;
	unsigned char *buffer = new unsigned char[BUFFER_SIZE+1];
	if(buffer == NULL)
		return FALSE;

	BOOL ret = FALSE;
	X509 *cert=NULL;
	char *name=NULL;

	do{
		if(!BIO_puts(target, "-->\n<!-- Copyright (C) 2008-2009 Opera Software ASA. All Rights Reserved. -->\n\n<untrusted-certificate>\n"))
			break;

		const unsigned char *data = entry->dercert_data;
		cert = d2i_X509(NULL, &data, entry->dercert_len);
		if(cert == NULL)
			break;

		// Cert name in one line
		char *name = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
		if(!name)
			break;
		if(!WriteXML(target, "subject", name, (char *) buffer, BUFFER_SIZE))
			break;

		OPENSSL_free(name);

		// friendly name
		if(entry->nickname)
			if(!WriteXML(target, "shortname", entry->nickname, (char *) buffer, BUFFER_SIZE))
				break;

		// reason it is untrusted
		if(entry->reason)
			if(!WriteXML(target, "untrusted-reason", entry->reason, (char *) buffer, BUFFER_SIZE))
				break;

		if(!BIO_puts(target, "<certificate-data>\n"))
			break;
		EVP_EncodeInit(&base_64);
		read_len=0;
		// Write Base64 encoded certificate
		for(i = 0; i < entry->dercert_len; i+= read_len)
		{
			read_len = (BUFFER_SIZE/4)*3;
			if(i + read_len > entry->dercert_len)
				read_len = entry->dercert_len -i;

			outl =0;
			EVP_EncodeUpdate(&base_64, buffer, &outl, entry->dercert_data+i, read_len);
			if(outl && !BIO_write(target, buffer, outl))
				break;
		}

		outl =0;
		EVP_EncodeFinal(&base_64, buffer, &outl);
		if(outl && !BIO_write(target, buffer, outl))
			break;

		if(!BIO_puts(target, "\n</certificate-data>\n"))
			break;

		if(!BIO_puts(target, "</untrusted-certificate>\n"))
			break;
		ret = TRUE;
	}
	while(0);

	X509_free(cert);
	OPENSSL_free(name);
	delete [] buffer;

	return ret;
}

BOOL ProduceUntrustedCertificateXML_File(const char *version, unsigned int version_num, const DEFCERT_UNTRUSTED_cert_st *item, EVP_PKEY *key, int sig_alg, BOOL include_pubkey)
{
    if(item == NULL || item->dercert_data == NULL)
        return TRUE;

	BIO *target = NULL;
	BIO *target1 = NULL;
	BOOL ret = FALSE;

	do{
		// Create file
		target = GenerateFile(version, version_num, "untrusted/", item->dercert_data, item->dercert_len, include_pubkey, NULL);
		if(!target)
			break;

		target1 = BIO_new(BIO_s_mem());
		if(!target1)
			break;

		// header
		if(!BIO_puts(target, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"))
			break;

		// Write cert
		if(!ProduceUntrustedCertificateXML(target1, item, key, sig_alg))
			break;

		if(!BIO_flush(target1))
			break;

		unsigned char *buffer = NULL;
		unsigned long buffer_len =0;

		buffer_len = BIO_get_mem_data(target1, &buffer);
		if(buffer == NULL || buffer_len == 0)
			break;

		// Sign file
		if(!SignXMLData(target, buffer, buffer_len, key, sig_alg))
			break;

		if(!BIO_flush(target))
			break;
		ret = TRUE;
	}
	while(0);

	BIO_free(target1);
	BIO_free(target);
	target = NULL;
	target1 = NULL;

	return TRUE;
}
