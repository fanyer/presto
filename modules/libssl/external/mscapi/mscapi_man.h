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

#ifndef _MSCAPI_MAN_H_
#define _MSCAPI_MAN_H_

#if defined _NATIVE_SSL_SUPPORT_ && defined _SSL_USE_MSCAPI_

#include "modules/libssl/smartcard/smc_man.h"
#include <wincrypt.h>


class MSCAPI_Provider;
class OpDLL;

class MSCAPI_Provider_Head: public SSL_Head{
public:
    MSCAPI_Provider* First() const{
		return (MSCAPI_Provider *) Head::First();
    };     //pointer
    MSCAPI_Provider* Last() const{
		return (MSCAPI_Provider *) Head::Last();
    };     //pointer
};  

class MSCAPI_Manager : public SmartCard_Master
{
private:
	OpDLL			*crypto_api_dll;
	MSCAPI_Provider_Head provider_list;
	HCERTSTORE		hMYSystemStore;
	HCERTSTORE		hUserDSSystemStore;
	HCERTSTORE		hCASystemStore;
	HCERTSTORE		hRootSystemStore;

public:
	/** Constructor, object takes ownership of crypto_dll */
	MSCAPI_Manager(OpDLL *crypto_dll);

	/** Intialize the Object */
	virtual OP_STATUS	Init(uni_char **arguments, int max_args);

	~MSCAPI_Manager();

	
	/** Retrieve a list of the Public Keys and certificates available from this provider, items are added to cipherlist */
	virtual OP_STATUS	GetAvailableCards(SSL_CertificateHandler_ListHead *cipherlist);

	/** Checks that all cards are present in their readers, and takes necessary actions for those cards that are not present */
	virtual void ConfirmCardsPresent();

	//BOOL MustLoginForCertificate(){return certificate_login;}

	/** Returns TRUE if successful */
	BOOL BuildCertificateChain(HCRYPTPROV provider, OpString &label, OpString &shortname, SSL_ASN1Cert_list &cert);


	/** Translate MSCAPI error code into an OP_STATUS code */
	static OP_STATUS	TranslateToOP_STATUS(int last_error);
	
	/** Translate MSCAPI error code into an SSL_Alert code */
	BOOL		TranslateToSSL_Alert(int last_error, SSL_Alert &msg) const;

	/** Translate MSCAPI error code into an SSL_Alert code and set it*/
	static BOOL		TranslateToSSL_Alert(int last_error, SSL_Error_Status *msg);
};



// MSCAPI_Provider will be active the entire session
class MSCAPI_Provider : public Link, public OpReferenceCounter
{
private:
	OpDLL *crypto_api_dll;
	OpSmartPointerNoDelete<MSCAPI_Manager> master;

	OpString provider_name;
	DWORD provider_type;
	HCRYPTPROV base_provider;

public:

	MSCAPI_Provider(MSCAPI_Manager *mstr);
	virtual ~MSCAPI_Provider();

	OP_STATUS Construct(const OpStringC &prov_name, DWORD prov_typ);

	OP_STATUS	GetAvailableCards(SSL_CertificateHandler_ListHead *cipherlist);
	void		ConfirmCardsPresent();

	MSCAPI_Provider *Suc(){return (MSCAPI_Provider *) Link::Suc();}
	MSCAPI_Provider *Pred(){return (MSCAPI_Provider *) Link::Pred();}
};

// crypt32.DLL
typedef HCERTSTORE (WINAPI *pfCertOpenSystemStoreA)(HCRYPTPROV hProv, LPCSTR szSubsystemProtocol);
typedef HCERTSTORE (WINAPI *pfCertOpenSystemStoreW)(HCRYPTPROV hProv, LPCWSTR szSubsystemProtocol);
#define pfCertOpenSystemStore  pfCertOpenSystemStoreW

typedef BOOL (WINAPI *pfCertCloseStore)(IN HCERTSTORE hCertStore, DWORD dwFlags);
typedef BOOL (WINAPI *pfCryptExportPublicKeyInfo)(IN HCRYPTPROV hCryptProv, IN DWORD dwKeySpec, IN DWORD dwCertEncodingType, OUT PCERT_PUBLIC_KEY_INFO pInfo, IN OUT DWORD *pcbInfo);
typedef PCCERT_CONTEXT (WINAPI *pfCertFindCertificateInStore)(IN HCERTSTORE hCertStore, IN DWORD dwCertEncodingType, IN DWORD dwFindFlags, IN DWORD dwFindType, IN const void *pvFindPara, IN PCCERT_CONTEXT pPrevCertContext);
typedef DWORD (WINAPI *pfCertNameToStrA)(IN DWORD dwCertEncodingType, IN PCERT_NAME_BLOB pName, IN DWORD dwStrType, OUT OPTIONAL LPSTR psz, IN DWORD csz);
typedef DWORD (WINAPI *pfCertNameToStrW)(IN DWORD dwCertEncodingType, IN PCERT_NAME_BLOB pName, IN DWORD dwStrType, OUT OPTIONAL LPWSTR psz, IN DWORD csz);
#define pfCertNameToStr  pfCertNameToStrW

// Advapi

typedef BOOL (WINAPI *pfCryptGetUserKey)(HCRYPTPROV hProv, DWORD dwKeySpec, HCRYPTKEY *phUserKey);
typedef BOOL (WINAPI *pfCryptGetKeyParam)(HCRYPTKEY hKey, DWORD dwParam, BYTE *pbData, DWORD *pdwDataLen, DWORD dwFlags);
typedef BOOL (WINAPI *pfCryptContextAddRef)(HCRYPTPROV hProv, DWORD *pdwReserved, DWORD dwFlags);
typedef BOOL (WINAPI *pfCryptReleaseContext)(HCRYPTPROV hProv, DWORD dwFlags);
typedef BOOL (WINAPI *pfCryptEncrypt)(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final, DWORD dwFlags, BYTE *pbData, DWORD *pdwDataLen, DWORD dwBufLen);
typedef BOOL (WINAPI *pfCryptDecrypt)(HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final, DWORD dwFlags, BYTE *pbData, DWORD *pdwDataLen);
typedef BOOL (WINAPI *pfCryptCreateHash)(HCRYPTPROV hProv, ALG_ID Algid, HCRYPTKEY hKey, DWORD dwFlags, HCRYPTHASH *phHash); 
typedef BOOL (WINAPI *pfCryptSetHashParam)(HCRYPTHASH hHash, DWORD dwParam, BYTE *pbData, DWORD dwFlags);

typedef BOOL (WINAPI *pfCryptSignHashA)(HCRYPTHASH hHash, DWORD dwKeySpec, LPCSTR sDescription, DWORD dwFlags, BYTE *pbSignature, DWORD *pdwSigLen);
typedef BOOL (WINAPI *pfCryptSignHashW)(HCRYPTHASH hHash, DWORD dwKeySpec, LPCWSTR sDescription, DWORD dwFlags, BYTE *pbSignature, DWORD *pdwSigLen);
#define pfCryptSignHash  pfCryptSignHashW

typedef BOOL (WINAPI *pfCryptDestroyHash)(HCRYPTHASH hHash);
typedef BOOL (WINAPI *pfCryptDestroyKey)(HCRYPTKEY hKey);

typedef BOOL (WINAPI *pfCryptEnumProvidersA)(DWORD dwIndex, DWORD *pdwReserved, DWORD dwFlags, DWORD *pdwProvType, LPSTR pszProvName, DWORD *pcbProvName);
typedef BOOL (WINAPI *pfCryptEnumProvidersW)(DWORD dwIndex, DWORD *pdwReserved, DWORD dwFlags, DWORD *pdwProvType, LPWSTR pszProvName, DWORD *pcbProvName);
#define pfCryptEnumProviders  pfCryptEnumProvidersW

typedef BOOL (WINAPI *pfCryptAcquireContextA)(HCRYPTPROV *phProv, LPCSTR pszContainer, LPCSTR pszProvider, DWORD dwProvType, DWORD dwFlags);
typedef BOOL (WINAPI *pfCryptAcquireContextW)(HCRYPTPROV *phProv, LPCWSTR pszContainer, LPCWSTR pszProvider, DWORD dwProvType, DWORD dwFlags);
#define pfCryptAcquireContext  pfCryptAcquireContextW

typedef BOOL (WINAPI *pfCryptGetProvParam)(HCRYPTPROV hProv, DWORD dwParam, BYTE *pbData, DWORD *pdwDataLen, DWORD dwFlags);


extern pfCertOpenSystemStore capi_CertOpenSystemStore;
extern pfCertCloseStore capi_CertCloseStore;
extern pfCertNameToStr capi_CertNameToStr;
extern pfCertFindCertificateInStore capi_CertFindCertificateInStore;
extern pfCryptExportPublicKeyInfo capi_CryptExportPublicKeyInfo;

#undef CertNameToStr
#undef CertOpenSystemStore
#define CertOpenSystemStore capi_CertOpenSystemStore
#define CertCloseStore		capi_CertCloseStore 
#define CertNameToStr		capi_CertNameToStr 
#define CertFindCertificateInStore	capi_CertFindCertificateInStore 
#define CryptExportPublicKeyInfo	capi_CryptExportPublicKeyInfo 


#endif

#endif
