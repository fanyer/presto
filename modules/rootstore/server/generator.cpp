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
#include <sys/stat.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

/* Main controller of the certificate repository builder */

#include "modules/rootstore/server/types.h"
#include "modules/rootstore/server/api.h"

#if !defined(ENABLE_V1_REPOSITORY) && defined IS_TEST_SERVER
#define ENABLE_V1_REPOSITORY
#endif

#ifdef ENABLE_V1_REPOSITORY
/* Configuration for version and key for v1 (debug) folder */
#define VERSION_1 "01"
#define VERSION_NUM_1 0x01
#define VERSION_KEY_1 "tools/key.pem"
#endif

/* Configuration for version and key for v2 folder */
#define VERSION_2 "02"
#define VERSION_NUM_2 0x02
#define VERSION_KEY_2 "tools/version02.key.pem"

/* Configuration for version and key for v3 (default) folder, uses v2 key*/
#define VERSION_3 "03"
#define VERSION_NUM_3 0x03
#define VERSION_KEY_3 "tools/version02.key.pem"

// Target folder names
#define FOLDER_TREE(VERSION) \
	"output/" VERSION, \
	"output/" VERSION "/roots", \
	"output/" VERSION "/untrusted" 

// Target folders, expanded
const char *folders[] = {
		"output",
#ifdef ENABLE_V1_REPOSITORY
		FOLDER_TREE(VERSION_1),
#endif
		FOLDER_TREE(VERSION_2),
		FOLDER_TREE(VERSION_3),
		NULL
};

// Configuration of repository, version, keys and methods
const struct config_list_st {
	const char *version_text;
	int version_num;
	const char *key_name;
	int sign_nid;
} config_list[] = {
#ifdef ENABLE_V1_REPOSITORY
		{VERSION_1, VERSION_NUM_1, VERSION_KEY_1, NID_sha256WithRSAEncryption},
#endif
		{VERSION_2, VERSION_NUM_2, VERSION_KEY_2, NID_sha256WithRSAEncryption},
		{VERSION_3, VERSION_NUM_3, VERSION_KEY_3, NID_sha256WithRSAEncryption},
		{NULL, 0, NULL, 0}
};


/** Build the repositor */
int main()
{
    OpenSSL_add_all_algorithms();
	if(ERR_get_error())
		return FALSE;

	BOOL ret = FALSE;
	EVP_PKEY *pkey = NULL;
	BIO *source = NULL;

	// Create target folders
	const char **fldrs = folders;
	while(*fldrs)
	{
		if(mkdir(*fldrs, 0755) != 0 && errno != EEXIST)
			return FALSE;
		fldrs++;
	}

	// For each configuration 
	const config_list_st *cfg = config_list;
	while(cfg->key_name != NULL)
	{
		ret = FALSE;
		// Read the key file
		source = BIO_new_file(cfg->key_name, "r");
		if(!source)
			break;;

		pkey = PEM_read_bio_PrivateKey(source, NULL, NULL, NULL);
		BIO_free(source);
		source = NULL;

		if(pkey == NULL)
			break;

		// Create the repository
		if(!CreateRepository(cfg->version_text, cfg->version_num, pkey, cfg->sign_nid))
			break;
		// Produce the EV OID list
		if(!ProduceEVOID_XML_File(cfg->version_text, pkey, cfg->sign_nid))
			break;

		if(ERR_get_error())
			break;
		// Cleanup
		BIO_free(source);
		EVP_PKEY_free(pkey);
		source = NULL;
		pkey = NULL;
		printf ("Finished %s %s\n", cfg->version_text, cfg->key_name);
		ret = TRUE;
		cfg++; 
	}

	// Clean up
	if(source)
		BIO_free(source);
	if(pkey)
		EVP_PKEY_free(pkey);
	EVP_cleanup();
	return (ret ? 0 : 1); // 0 is OK in shell scripts
}

