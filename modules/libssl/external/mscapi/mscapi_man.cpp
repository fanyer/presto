/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined _NATIVE_SSL_SUPPORT_ && defined _SSL_USE_MSCAPI_

#if !defined(MSWIN)
#error "FEATURE_SMARTCARD_MSCAPI can only be used in MSWIN builds."
#endif

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"

#include "modules/hardcore/mem/mem_man.h"

#include "modules/pi/opfactory.h"
#include "modules/pi/OpDLL.h"



#include "mscapi_pubk.h"

pfCertOpenSystemStore capi_CertOpenSystemStore = NULL;
pfCertCloseStore capi_CertCloseStore = NULL;
pfCertNameToStr capi_CertNameToStr = NULL;
pfCertFindCertificateInStore capi_CertFindCertificateInStore = NULL;
pfCryptExportPublicKeyInfo capi_CryptExportPublicKeyInfo = NULL;

#define CRYPT32_FUNCTION_NAME(f) f "W"

SmartCard_Master *CreateMSCAPI_ManagerL()
{
	OpStackAutoPtr<OpDLL> crypto_api_dll(NULL);
	OpDLL *temp_dll=NULL;

	LEAVE_IF_ERROR(g_factory->CreateOpDLL(&temp_dll));
	crypto_api_dll.reset(temp_dll);
	temp_dll = NULL;

	LEAVE_IF_ERROR(crypto_api_dll->Load(UNI_L("crypt32.dll")));

	OP_ASSERT(crypto_api_dll->IsLoaded());

	OpStackAutoPtr<MSCAPI_Manager> manager(OP_NEW_L(MSCAPI_Manager, (crypto_api_dll.get())));

	crypto_api_dll.release(); // manager != NULL if we got here

	LEAVE_IF_ERROR(manager->Init(NULL, 0));
	
	return manager.release();
}


MSCAPI_Manager::MSCAPI_Manager(OpDLL *crypto_dll)
: crypto_api_dll(crypto_dll)
{
	hMYSystemStore = NULL;
	hUserDSSystemStore = NULL;
	hCASystemStore = NULL;
	hRootSystemStore = NULL;
}

OP_STATUS MSCAPI_Manager::Init(uni_char **arguments, int max_args)
{
	if(!crypto_api_dll->IsLoaded())
		return OpStatus::ERR_NULL_POINTER;

	capi_CertOpenSystemStore = (pfCertOpenSystemStore) crypto_api_dll->GetSymbolAddress(CRYPT32_FUNCTION_NAME("CertOpenSystemStore"));
	capi_CertCloseStore = (pfCertCloseStore) crypto_api_dll->GetSymbolAddress("CertCloseStore");
	capi_CertNameToStr = (pfCertNameToStr) crypto_api_dll->GetSymbolAddress(CRYPT32_FUNCTION_NAME("CertNameToStr"));
	capi_CertFindCertificateInStore = (pfCertFindCertificateInStore) crypto_api_dll->GetSymbolAddress("CertFindCertificateInStore");
	capi_CryptExportPublicKeyInfo = (pfCryptExportPublicKeyInfo) crypto_api_dll->GetSymbolAddress("CryptExportPublicKeyInfo");


	if(capi_CertOpenSystemStore == NULL ||
		capi_CertCloseStore == NULL ||
		capi_CertNameToStr == NULL ||
		capi_CertFindCertificateInStore == NULL ||
		capi_CryptExportPublicKeyInfo == NULL)
		return OpStatus::ERR_NULL_POINTER;

	DWORD provider_type;
	DWORD name_length;
	uni_char *name = (uni_char *) g_memory_manager->GetTempBuf2k();
	DWORD i;

	for(i = 0;;i++)
	{
		*name = '\0';
		name_length = g_memory_manager->GetTempBuf2kLen();
		provider_type = NULL;
		if(!CryptEnumProviders(i, NULL, 0, &provider_type, name, &name_length))
		{
			int error = GetLastError();
			if(error == ERROR_MORE_DATA)
				continue; //Ignore items with too long name
			if(error != ERROR_NO_MORE_ITEMS)
				return TranslateToOP_STATUS(error);

			break;
		}

		if(provider_type != PROV_RSA_FULL &&
			provider_type != PROV_RSA_SIG)
			continue;

		OpAutoPtr<MSCAPI_Provider> provider(OP_NEW(MSCAPI_Provider, (this)));

		if(provider.get())
		{
			RETURN_IF_ERROR(provider->Construct(name, provider_type));

			provider->Into(&provider_list);
			provider.release();
		}

	}

	hMYSystemStore = CertOpenSystemStore(NULL, UNI_L("MY"));
			
	if(!hMYSystemStore)
	{
		return TranslateToOP_STATUS(GetLastError());
	}
			
	hUserDSSystemStore = CertOpenSystemStore(NULL, UNI_L("UserDS"));
			
	hCASystemStore = CertOpenSystemStore(NULL, UNI_L("CA"));
			
	hRootSystemStore = CertOpenSystemStore(NULL, UNI_L("ROOT"));

	return OpStatus::OK;
}


MSCAPI_Manager::~MSCAPI_Manager()
{
	provider_list.Clear();

	if(hMYSystemStore)
		CertCloseStore(hMYSystemStore, 0);
	if(hUserDSSystemStore)
		CertCloseStore(hUserDSSystemStore, 0);
	if(hCASystemStore)
		CertCloseStore(hCASystemStore, 0);
	if(hRootSystemStore)
		CertCloseStore(hRootSystemStore, 0);

	capi_CertOpenSystemStore = NULL;
	capi_CertCloseStore = NULL;
	capi_CertNameToStr = NULL;
	capi_CertFindCertificateInStore = NULL;
	capi_CryptExportPublicKeyInfo = NULL;

	OP_DELETE(crypto_api_dll);
}


OP_STATUS MSCAPI_Manager::GetAvailableCards(SSL_CertificateHandler_ListHead *cipherlist)
{
	MSCAPI_Provider *item = provider_list.First();
	
	while(item)
	{
		RETURN_IF_ERROR(item->GetAvailableCards(cipherlist));

		item = item->Suc();
	}

	return OpStatus::OK;
}

void MSCAPI_Manager::ConfirmCardsPresent()
{
}

OP_STATUS	MSCAPI_Manager::TranslateToOP_STATUS(int last_error)
{
	return OpStatus::ERR;
}

BOOL MSCAPI_Manager::TranslateToSSL_Alert(int last_error, SSL_Alert &msg) const
{
	return OpStatus::ERR;
}

BOOL MSCAPI_Manager::TranslateToSSL_Alert(int last_error, SSL_Error_Status *msg)
{
	return OpStatus::ERR;
}

BOOL MSCAPI_Manager::BuildCertificateChain(HCRYPTPROV provider, OpString &label, OpString &shortname, SSL_ASN1Cert_list &cert)
{
	CERT_PUBLIC_KEY_INFO *pubkey = (CERT_PUBLIC_KEY_INFO *) g_memory_manager->GetTempBuf2k();
	DWORD len;

	label.Empty();
	shortname.Empty();
	cert.Resize(0);

	if(!hMYSystemStore)
		return FALSE;

	len = g_memory_manager->GetTempBuf2kLen();
	if(!CryptExportPublicKeyInfo(provider,  /*AT_SIGNATURE*/ AT_KEYEXCHANGE, (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING), pubkey, &len))
	{
		int err0 = GetLastError();
		op_memset(&pubkey, 0, sizeof(pubkey));
		len = g_memory_manager->GetTempBufLen();
		if(!CryptExportPublicKeyInfo(provider, /* AT_KEYEXCHANGE */ AT_SIGNATURE, (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING), pubkey, &len))
		{
			int err = GetLastError();
			return FALSE;
		}
	}
		
	PCCERT_CONTEXT cert_item = NULL;
	
	cert_item = CertFindCertificateInStore(hMYSystemStore, (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING), 0, CERT_FIND_PUBLIC_KEY, pubkey, cert_item);
	if(!cert_item && hUserDSSystemStore)
		cert_item = CertFindCertificateInStore(hUserDSSystemStore, (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING), 0, CERT_FIND_PUBLIC_KEY, pubkey, cert_item);

	if(!cert_item)
		return FALSE;

	
	len = CertNameToStr((X509_ASN_ENCODING | PKCS_7_ASN_ENCODING), 
		&cert_item->pCertInfo->Subject, CERT_SIMPLE_NAME_STR, NULL, 0);
	

	if(len)
	{
		if(shortname.Reserve(len+1) == NULL)
			return OpStatus::ERR_NO_MEMORY;
		
		len = CertNameToStr((X509_ASN_ENCODING | PKCS_7_ASN_ENCODING), 
			&cert_item->pCertInfo->Subject, CERT_SIMPLE_NAME_STR, shortname.DataPtr(), shortname.Capacity());
	}
	
	cert.Resize(1);
	if(cert.Error())
		return FALSE();
	
	cert[0].Set(cert_item->pbCertEncoded, cert_item->cbCertEncoded);
	
	if(cert.Error() || cert[0].GetLength() == 0)
		return FALSE;

	return TRUE;
}

MSCAPI_Provider::MSCAPI_Provider(MSCAPI_Manager *mstr)
: master(mstr)
{
	base_provider = NULL;
}

MSCAPI_Provider::~MSCAPI_Provider()
{
	if(base_provider != NULL)
		CryptReleaseContext(base_provider , 0);
	base_provider  = NULL;
}

OP_STATUS MSCAPI_Provider::Construct(const OpStringC &prov_name, DWORD prov_typ)
{

	RETURN_IF_ERROR(provider_name.Set(prov_name));
	provider_type = prov_typ;

	if(!CryptAcquireContext(&base_provider, NULL, provider_name.CStr(), provider_type, CRYPT_VERIFYCONTEXT))
	{
		return master->TranslateToOP_STATUS(GetLastError());
	}

	return (base_provider != NULL? OpStatus::OK : OpStatus::ERR);
}

OP_STATUS MSCAPI_Provider::GetAvailableCards(SSL_CertificateHandler_ListHead *cipherlist)
{
	BOOL first= TRUE;

	do{
		char *name_2 = (char *) g_memory_manager->GetTempBuf();
		DWORD name_2_len = g_memory_manager->GetTempBufLen();
		HCRYPTPROV provider2 = NULL;
		CERT_PUBLIC_KEY_INFO *pubkey = (CERT_PUBLIC_KEY_INFO *) g_memory_manager->GetTempBuf2k();
		
		*name_2 = '\0';
		if(!CryptGetProvParam(base_provider, PP_ENUMCONTAINERS, (BYTE *) name_2, &name_2_len, (first ? CRYPT_FIRST : 0)))
			break;
		
		first = FALSE;
		
		if(!CryptAcquireContext(&provider2,make_doublebyte_in_tempbuffer(name_2, op_strlen(name_2)), provider_name.CStr(), provider_type, 0))
		{
			continue;
			//return TranslateToOP_STATUS(GetLastError());
		}
		
		{
			OpString lbl;
			OpString sn;
			SSL_ASN1Cert_list cert;
			OpStackAutoPtr<SSL_PublicKeyCipher> pkey(NULL);

			if(master->BuildCertificateChain(provider2, lbl, sn, cert))
			{				
				pkey.reset(OP_NEW(MSCAPI_PublicKeyCipher, (provider2)));
				
				if(pkey.get() == NULL)
				{
					continue;
				}
				
				{
					OpStackAutoPtr<SSL_CertificateHandler_List> item(OP_NEW(SSL_CertificateHandler_List, ()));
					
					if(!item.get())
						return OpStatus::ERR_NO_MEMORY;
					
					item->external_key_item = OP_NEW(SSL_Certificate_And_Key_List, (lbl, sn, cert, pkey.get()));
					if(!item->external_key_item)
						return OpStatus::ERR_NO_MEMORY;
					
					pkey.release();
					
					item->Into(cipherlist);
					
					item.release();
					
				}
				
			}
		}
		
		CryptReleaseContext(provider2, 0);

	}while(1);

	return OpStatus::OK;
}

void MSCAPI_Provider::ConfirmCardsPresent()
{
}

#ifdef _SSL_USE_MSCAPI_
	{
		extern SmartCard_Master *CreateMSCAPI_ManagerL();
		
		SmartCard_Master *manager = NULL;
		
		TRAP_AND_RETURN(op_err, manager = CreateMSCAPI_ManagerL());
		
		manager->Into(&key_masters);
	}
#endif


#endif
