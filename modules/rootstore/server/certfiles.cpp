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

/** Write the text in an (optional) tag, while XML exscaping it, using the buffer for temporary storage */
BOOL WriteXML(BIO *target, const char *tagname, const char *text, char *buffer, unsigned int buf_len)
{
	const char *token = text;
	unsigned int outl=0;

	if(tagname)
		if(!BIO_printf(target, "<%s>\n", tagname))
			return FALSE;

	while(*token)
	{
		unsigned char tok = (unsigned char) *(token++);
		const char *tag = NULL;
		switch(tok)
		{
		case '<':
			tag = "&lt;";
			break;
		case '>':
			tag = "&gt;";
			break;
		case '&':
			tag = "&amp;";
			break;
		case '"':
			tag = "&quot;";
			break;
		}
		if(outl && (tag || tok >= 128 || outl >= buf_len))
		{
			if(!BIO_write(target, buffer, outl))
				return FALSE;
			outl = 0;
		}
		if(tag)
		{
			if(!BIO_puts(target, tag))
				return FALSE;
		}
		else if(tok >=128)
		{
			if(!BIO_printf(target, "&#%xH;",(unsigned char) tok))
				return FALSE;
		}
		else
			buffer[outl++] = tok;
	}
	if(outl)
		if(!BIO_write(target, buffer, outl))
			return FALSE;
	if(tagname)
		if(!BIO_printf(target, "\n</%s>\n", tagname))
			return FALSE;

	return TRUE;
}

/** Write a certificate, with signed EV OIDS, as XML data to a file */
BOOL ProduceCertificateXML(BIO *target, const DEFCERT_st *entry, EVP_PKEY *key, int sig_alg, unsigned int version_num)
{
	unsigned int i, read_len=0;
	int outl;

	if(target == NULL)
		return FALSE;
	if(entry->dercert_data == NULL || entry->dercert_len == 0)
		return TRUE;

	EVP_ENCODE_CTX base_64;
	unsigned char *buffer = new unsigned char[BUFFER_SIZE+1];
	if(buffer == NULL)
		return FALSE;

	BOOL ret = FALSE;
	X509 *cert=NULL;
	char *name=NULL;

	do{
		if(!BIO_puts(target, "<certificate"))
			break;

		if((version_num == 1 || version_num >= 3) && entry->dependencies)
		{
			if(entry->dependencies->before && BIO_printf(target, " before=\"%u\"", entry->dependencies->before) <= 0)
				break;
			if(entry->dependencies->after && BIO_printf(target, " after=\"%u\"", entry->dependencies->after) <= 0)
				break;
		}

		if(!BIO_puts(target, ">\n"))
			break;

		const unsigned char *data = entry->dercert_data;
		cert = d2i_X509(NULL, &data, entry->dercert_len);
		if(cert == NULL)
			break;

		// Print the certificate's name as a oneliner
		char *name = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
		if(!name)
			break;
		if(!WriteXML(target, "issuer", name, (char *) buffer, BUFFER_SIZE))
			break;

		OPENSSL_free(name);

		// What is the friendly name for this cert?
		if(entry->nickname)
			if(!WriteXML(target, "shortname", entry->nickname, (char *) buffer, BUFFER_SIZE))
				break;

		// Write certificate as Base64 data
		if(!BIO_puts(target, "<certificate-data>\n"))
			break;
		EVP_EncodeInit(&base_64);
		read_len=0;
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
		// Warn flag
		if(entry->def_warn)
			if(!BIO_puts(target, "<warn/>\n"))
				break;
		
		// Deny flag
		if(entry->def_deny)
			if(!BIO_puts(target, "<deny/>\n"))
				break;

		// Write and sign any EV OIDs 
		if(entry->dependencies && entry->dependencies->EV_OIDs &&  *entry->dependencies->EV_OIDs)
			if(!ProduceEV_OID_list(target, entry, (char *) buffer, BUFFER_SIZE, key,sig_alg))
				break;

		if(!BIO_puts(target, "</certificate>\n"))
			break;
		ret = TRUE;
	}
	while(0);

	X509_free(cert);
	OPENSSL_free(name);
	delete [] buffer;

	return ret;
}

/* Create a list of dependent certificates */
void ProduceCertificate_List(unsigned int i, BOOL *to_visit, unsigned int to_visit_len)
{
	if(i >= to_visit_len || to_visit == NULL)
		return;

	if(to_visit[i])
		return;
	to_visit[i] = TRUE;

	const DEFCERT_st *entry = GetRoot(i);
	if(entry->dependencies && entry->dependencies->dependencies)
	{
		const DEFCERT_depcertificate_item *deps = entry->dependencies->dependencies;

		// Recursively flag all the certificates this certificate depends on
		while(deps->cert != NULL)
		{
			const byte *dep = (deps)->cert;
			deps ++;
			unsigned int j;
			for(j=0; j< GetRootCount(); j++)
			{
				if(GetRoot(j)->dercert_data == dep)
				{
					ProduceCertificate_List(j,to_visit, to_visit_len);
				}
			}
		}
	}
}
