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

#ifndef _ROOTSTORE_SERVER_API_H_
#define _ROOTSTORE_SERVER_API_H_

struct DEFCERT_st;
struct DEFCERT_keyid_item;
struct DEFCERT_DELETE_cert_st;
struct DEFCERT_UNTRUSTED_cert_st;
struct DEFCERT_crl_overrides;

unsigned int GetRootCount();
const DEFCERT_st *GetRoot(unsigned int i);
unsigned int GetDeleteCertCount();
const DEFCERT_DELETE_cert_st *GetDeleteCertSpec(unsigned int i);
unsigned int GetUntrustedCertCount();
const DEFCERT_UNTRUSTED_cert_st *GetUntrustedCertSpec(unsigned int i);
unsigned int GetUntrustedRepositoryCertCount();
const DEFCERT_UNTRUSTED_cert_st *GetUntrustedRepositoryCertSpec(unsigned int i);
const DEFCERT_crl_overrides *GetCRLOverride();
const char *const *GetOCSPOverride();
BOOL WriteXML(BIO *target, const char *tagname, const char *text, char *buffer, unsigned int buf_len);
BOOL ProduceEVOID_XML_File(const char *version, EVP_PKEY *key, int sig_alg);
BOOL ProduceEV_XML(BIO *target, const DEFCERT_st *entry, EVP_PKEY *key, int sig_alg);
BOOL ProduceCertificateXML(BIO *target, const DEFCERT_st *entry, EVP_PKEY *key, int sig_alg, unsigned int version_num);
void ProduceCertificate_List(unsigned int i, BOOL *to_visit, unsigned int to_visit_len);
BOOL ProduceEV_OID_list(BIO *target, const DEFCERT_st *entry, char *buffer, unsigned int buf_len, EVP_PKEY *key, int sig_alg);
char *GenerateFileName(unsigned int version_num, const DEFCERT_st *entry, BOOL with_pub_key, const DEFCERT_keyid_item *item);
char *GenerateFileName(unsigned int version_num, const byte *dercert_data, size_t dercert_len,
						BOOL with_pub_key, const DEFCERT_keyid_item *item);
BIO *GenerateFile(const char *version, unsigned int version_num, const char *path,  
		const byte *dercert_data, size_t dercert_len, 
		BOOL with_pub_key, const DEFCERT_keyid_item *item);
BIO *GenerateFile(const char *version, unsigned int version_num, const DEFCERT_st *entry, BOOL with_pub_key, const DEFCERT_keyid_item *item);
BOOL CreateRepository(const char *version, unsigned int version_num, EVP_PKEY *key, int sig_al);
BOOL ProduceUntrustedCertificateXML(BIO *target, const DEFCERT_UNTRUSTED_cert_st *entry, EVP_PKEY *key, int sig_alg);
BOOL ProduceUntrustedCertificateXML_File(const char *version, unsigned int version_num, const DEFCERT_UNTRUSTED_cert_st *item, EVP_PKEY *key, int sig_alg, BOOL include_pubkey=TRUE);

BOOL SignXMLData(BIO *target, const unsigned char *buffer, unsigned int buf_len, EVP_PKEY *key, int sig_alg);
unsigned long SignBinaryData(unsigned char **target, const unsigned char *buffer, unsigned int buf_len, EVP_PKEY *key, int sig_alg);

BIO *GenerateFile(const char *version, const char *path, const char *name);


#endif // _ROOTSTORE_SERVER_API_H_
