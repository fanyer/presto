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

#include "core/pch.h"

#ifdef _NATIVE_SSL_SUPPORT_
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/ssl_api.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/libssl/smartcard/smc_man.h"
#include "modules/libssl/smartcard/smckey.h"

#include "modules/hardcore/mh/mh.h"

#include "modules/util/gen_str.h"

extern OP_STATUS ParseCommonName(const OpString_list &info, OpString &title);

SSL_Certificate_And_Key_List::SSL_Certificate_And_Key_List(const OpStringC &lbl, const OpStringC &sn, SSL_ASN1Cert_list &cert, SSL_PublicKeyCipher *pkey)
: certificate(NULL), key(pkey)
{
	OP_STATUS stat;

	der_certificate.ForwardTo(this);
	der_certificate = cert;
	if(key)
		key->ForwardTo(this);
	stat = label.Set(lbl);
	if(OpStatus::IsError(stat))
		RaiseAlert(stat);
	stat = serialnumber.Set(sn);
	if(OpStatus::IsError(stat))
		RaiseAlert(stat);

	if(Certificate())
	{
		OpString_list info;
		certificate->GetSubjectName(0,info);
		ParseCommonName(info, name);
		// Ignore OOM errors
		certificate->ResetError();
	}
}

SSL_CertificateHandler *SSL_Certificate_And_Key_List::Certificate()
{
	if(certificate == NULL)
	{
		certificate = g_ssl_api->CreateCertificateHandler();
		if(certificate == NULL)
		{
			RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
			return NULL;
		}

		certificate->ForwardTo(this);

		certificate->LoadCertificate(der_certificate);

		if(ErrorRaisedFlag)
		{
			OP_DELETE(certificate);
			certificate = NULL;
		}
	}

	return certificate;
}

SSL_Certificate_And_Key_List::~SSL_Certificate_And_Key_List()
{
	OP_DELETE(certificate);
	certificate = NULL;
	OP_DELETE(key);
	key = NULL;
}

SSL_KeyManager::SSL_KeyManager()
{
}

SSL_KeyManager::~SSL_KeyManager()
{
	if(InList())
		Out();
}

OP_STATUS SSL_KeyManager::SetupCertificateEntry(SSL_CertificateHandler_ListHead *cipherlist, const OpStringC &label, const OpStringC &serial, SSL_ASN1Cert_list &certificate, SSL_PublicKeyCipher *pkey)
{
	OpStackAutoPtr<SSL_PublicKeyCipher> key(pkey);
	OpStackAutoPtr<SSL_Certificate_And_Key_List>  item(NULL);
	OpStackAutoPtr<SSL_CertificateHandler_List> candidateitem(NULL);

	if(cipherlist == NULL)
		return OpStatus::ERR_NULL_POINTER;

	item.reset(OP_NEW(SSL_Certificate_And_Key_List, (label, serial, certificate, key.get())));
	
	if(!item.get())
		return OpStatus::ERR_NO_MEMORY;

	key.release();

	candidateitem.reset(OP_NEW(SSL_CertificateHandler_List, ()));
	if(!candidateitem.get())
		return OpStatus::ERR_NO_MEMORY;

	candidateitem->external_key_item = item.release();
	
	candidateitem->Into(cipherlist);
	candidateitem.release();

	return OpStatus::OK;
}

BOOL SSL_KeyManager::AllowedCertificate(SSL_DistinguishedName_list &ca_names, SSL_DistinguishedName &issuer_candidate)
{
	uint16 n = (uint16) ca_names.Count();
	if(n != 0)
	{
		for(uint16 i=0;i<n;i++)
		{
			if(ca_names[i] == issuer_candidate)
				return TRUE;
		}
	}
	else
		return TRUE;
	return FALSE;
}


/* Does not use these presently
BOOL SSL_KeyManager::Idle()
{
	return (Get_Reference_Count() == 0);
}

time_t SSL_KeyManager::LastUsed()
{
	return last_used;
}

void SSL_KeyManager::WasUsedNow()
{
	last_used = prefsManager->CurrentTime();
}
*/


SSL_ExternalKeyManager::SSL_ExternalKeyManager()
{
}

SSL_ExternalKeyManager::~SSL_ExternalKeyManager()
{
	key_masters.Clear();
#ifdef _SSL_USE_SMARTCARD_
	g_periodic_task_manager->UnregisterTask(this);
#endif
}

OP_STATUS SSL_ExternalKeyManager::InitL()
{
#ifdef _SSL_USE_SMARTCARD_
	RETURN_IF_ERROR(g_periodic_task_manager->RegisterTask(this, 5000));
#endif

	return OpStatus::OK;
}

#ifdef _SSL_USE_SMARTCARD_
void SSL_ExternalKeyManager::Run()
{
	ConfirmKeysPresent();
}

void SSL_ExternalKeyManager::ConfirmKeysPresent()
{
	SSL_KeyManager *current_driver;

	current_driver = key_masters.First();
	while(current_driver)
	{
		current_driver->ConfirmKeysPresent();
		current_driver = current_driver->Suc();
	}
}
#endif

OP_STATUS SSL_ExternalKeyManager::GetAvailableKeys(SSL_CertificateHandler_ListHead *cipherlist, SSL_DistinguishedName_list &ca_names, ServerName *sn, uint16 port)
{
	SSL_KeyManager *current_driver;

	current_driver = key_masters.First();
	while(current_driver)
	{
		RETURN_IF_ERROR(current_driver->GetAvailableKeys(cipherlist, ca_names, sn, port));
		current_driver = current_driver->Suc();
	}
	return OpStatus::OK;
}

OP_STATUS SSL_ExternalKeyManager::GetAvailableKeysAndCertificates(SSL_CertificateHandler_ListHead *cipherlist, SSL_DistinguishedName_list &ca_names, ServerName *sn, uint16 port)
{
	OP_STATUS op_err;

	if(cipherlist == NULL)
		return OpStatus::ERR_NULL_POINTER;

	op_err = GetAvailableKeys(cipherlist,ca_names, sn,port);

#ifdef LIBSSL_ASK_ABOUT_NO_EXTERNAL_KEY
#error "Does not work, need re-engineering"
	if(OpStatus::IsSuccess(op_err) && cipherlist->Empty() && g_external_clientkey_manager->ActiveKeys() &&
		prefsManager->GetIntegerPrefDirect(PrefsManager::AskUserWhenNoSmartCardPresent))
	{
		mess->Message(NULL, (Str::LocaleString) 0, (Str::LocaleString) 0, MB_OK | MB_ICONEXCLAMATION, UNI_L("Smartcard Missing"), UNI_L("There is no appropriate smartcard in the reader.\r\n")
			UNI_L("If you forgot to insert the card, or inserted the wrong card, \r\nplease insert the correct card now."));
		op_err = GetAvailableKeys(cipherlist);
	}
#endif

	if(OpStatus::IsSuccess(op_err))
	{
		SSL_CertificateHandler_List *item = cipherlist->First();

		// Remove duplicate certificates.
		while(item)
		{
			if(item->external_key_item && item->external_key_item->DERCertificate().Count())
			{
				SSL_CertificateHandler_List *item1, *item2 = item->Suc();

				while(item2)
				{
					item1 = item2;
					item2 = item2->Suc();

					if(item1->external_key_item && 
						item1->external_key_item->DERCertificate().Count() && 
						item1->external_key_item->DERCertificate()[0] == item->external_key_item->DERCertificate()[0])
					{
						item1->Out();
						OP_DELETE(item1);
					}
				}
			}

			item = item->Suc();
		}

		item = cipherlist->First();
		while(item)
		{
			if(item->external_key_item != NULL)
			{
				SSL_CertificateHandler *cert = item->external_key_item->Certificate();

				if(cert != NULL)
				{
					if(!cert->UsagePermitted(SSL_Purpose_Client_Certificate))
						cert = NULL; // item will be deleted
				}
				if(cert == NULL)
				{
					SSL_CertificateHandler_List *temp = item;
					item = temp->Suc();
					temp->Out();
					OP_DELETE(temp);
					continue;
				}
			}
			item = item->Suc();
		}

	}

	return op_err;
}
#endif
#endif
