/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"

void SSL_Options::Set_RegisterChanges(BOOL val)
{
	if(register_updates && !val)
	{
		SSL_CertificateItem *current_cert, *to_delete= NULL;
		int i;

		updated_protocols = updated_v2_ciphers = updated_v3_ciphers = updated_password_aging = FALSE;

		for(i = 0; i< 2; i++)
		{
			current_cert = (i == 0 ? System_ClientCerts.First() : System_TrustedCAs.First()) ;
			while(current_cert)
			{
				if(current_cert->cert_status == Cert_Deleted)
				{
					to_delete = current_cert;
				}
				current_cert->cert_status = Cert_Not_Updated;
				current_cert = current_cert->Suc();
				if(to_delete)
				{
					if(i == 0 && to_delete->certificate.GetLength())
					{
						ServerName *server = g_url_api->GetFirstServerName();

						while(server)
						{
							server->InvalidateSessionForCertificate(to_delete->certificate);
							server = g_url_api->GetNextServerName();
						}
					}

					to_delete->Out();
					OP_DELETE(to_delete);
					to_delete = NULL;
				}
			}
		}
	}
	register_updates= val;
}

BOOL SSL_Options::LoadUpdates(SSL_Options *optionsManager)
{
	if(!optionsManager)
		return TRUE;

	if(optionsManager->updated_protocols)
	{
		Enable_SSL_V3_0 = optionsManager->Enable_SSL_V3_0;
		Enable_TLS_V1_0 = optionsManager->Enable_TLS_V1_0;
		Enable_TLS_V1_1 = optionsManager->Enable_TLS_V1_1;
#ifdef _SUPPORT_TLS_1_2
		Enable_TLS_V1_2 = optionsManager->Enable_TLS_V1_2;
#endif
	}

	if(optionsManager->updated_v3_ciphers)
	{
		current_security_profile->original_ciphers = optionsManager->current_security_profile->original_ciphers;
		current_security_profile->ciphers= current_security_profile->original_ciphers;

		if(current_security_profile->original_ciphers.ErrorRaisedFlag ||
			current_security_profile->ciphers.ErrorRaisedFlag)
			return FALSE;

		some_secure_ciphers_are_disabled = FALSE;
		uint32 j;
		for(j=0;j <StrongCipherSuite.Count(); j++)
		{
			if(!current_security_profile->ciphers.Contain(StrongCipherSuite[j]))
			{
				some_secure_ciphers_are_disabled = TRUE;
				break;
			}
		}
	}

	if(!some_secure_ciphers_are_disabled && !Enable_SSL_V3_0 && !Enable_TLS_V1_0
		&& !Enable_TLS_V1_1
#ifdef _SUPPORT_TLS_1_2
		&& !Enable_TLS_V1_2
#endif
		)
		some_secure_ciphers_are_disabled = TRUE;

	if(optionsManager->updated_password)
	{
		optionsManager->CheckPasswordAging();
		if(optionsManager->obfuscated_SystemCompletePassword.GetLength())
		{
			optionsManager->DeObfuscate(SystemCompletePassword);
			Obfuscate();
			SystemPasswordVerifiedLast = optionsManager->SystemPasswordVerifiedLast;
			CheckPasswordAging();
		}
		SystemPartPassword = optionsManager->SystemPartPassword;
		SystemPartPasswordSalt = optionsManager->SystemPartPasswordSalt;
		SystemPasswordVerifiedLast = optionsManager->SystemPasswordVerifiedLast;
		if(obfuscated_SystemCompletePassword.ErrorRaisedFlag ||
			SystemCompletePassword.ErrorRaisedFlag ||
			SystemPartPassword.ErrorRaisedFlag    ||
			SystemPartPasswordSalt.ErrorRaisedFlag)
			return FALSE;
	}

	if(optionsManager->updated_password_aging)
	{
		PasswordAging = optionsManager->PasswordAging;
		CheckPasswordAging();
	}

	if(optionsManager->updated_repository)
	{
		root_repository_list = optionsManager->root_repository_list;
		untrusted_repository_list = optionsManager->untrusted_repository_list;
		crl_override_list.Clear();
		SSL_CRL_Override_Item *crl_overideitem = (SSL_CRL_Override_Item *) optionsManager->crl_override_list.First();
		while(crl_overideitem)
		{
			OpAutoPtr<SSL_CRL_Override_Item> new_item(OP_NEW_L(SSL_CRL_Override_Item, ()));
			new_item->cert_id = crl_overideitem->cert_id;
			if(OpStatus::IsError(new_item->cert_id.GetOPStatus()) ||
				OpStatus::IsError(new_item->crl_url.Set(crl_overideitem->crl_url)))
				return FALSE;
			new_item.release()->Into(&crl_override_list);

			crl_overideitem = (SSL_CRL_Override_Item *) crl_overideitem->Suc();
		}

		unsigned long n = optionsManager->ocsp_override_list.Count();
		if(OpStatus::IsError(ocsp_override_list.Resize(n)))
			return FALSE;

		unsigned long i;
		for(i=0;i<n;i++)
		{
			if(OpStatus::IsError(ocsp_override_list[i].Set(optionsManager->ocsp_override_list[i])))
				return FALSE;
		}
	}

	uint32 type_sel;
	for(type_sel = 1; type_sel < SSL_LOAD_ALL_STORES; type_sel <<= 1)
	{
		BOOL *loaded_flag = NULL;
		BOOL *loaded_flag_other = NULL;
		const uni_char *filename = NULL;
		SSL_CertificateHead *current_head= NULL, *current_head_update=NULL;
		SSL_CertificateItem *current_cert, *current_cert_update;
		SSL_CertificateStore store = SSL_Unknown_Store;
		uint32 recordtag = 0;

		current_head= MapSelectionToStore(type_sel, loaded_flag, filename, store, recordtag);
		current_head_update = optionsManager->MapSelectionToStore(type_sel, loaded_flag_other, filename, store, recordtag);

		if(current_head && loaded_flag && (*loaded_flag) &&
			current_head_update && loaded_flag_other && (*loaded_flag_other))
		{
			current_cert_update = current_head_update->First();

			while(current_cert_update)
			{
				if(current_cert_update->cert_status != Cert_Not_Updated)
				{
					BOOL perform_update = TRUE;
					if(current_cert_update->cert_status == Cert_Updated ||
						current_cert_update->cert_status == Cert_Deleted ||
						current_cert_update->cert_status == Cert_Inserted)
					{
						current_cert = current_head->First();
						while(current_cert)
						{
							if((current_cert->certificate.GetLength() == 0 || current_cert->cert_title.Compare(current_cert_update->cert_title) == 0) &&
								((current_cert->certificate.GetLength() && current_cert->certificate == current_cert_update->certificate) ||
								(current_cert->certificate.GetLength() == 0 && current_cert->public_key_hash == current_cert_update->public_key_hash)))
								break;
							current_cert = current_cert->Suc();
						}

						if(!current_cert)
							perform_update = (current_cert_update->cert_status == Cert_Deleted ? FALSE : TRUE);
						else
						{
							if(store == SSL_ClientStore && current_cert->certificate.GetLength())
							{
								ServerName *server = g_url_api->GetFirstServerName();

								while(server)
								{
									server->InvalidateSessionForCertificate(current_cert->certificate);
									server = g_url_api->GetNextServerName();
								}
							}

							OP_DELETE(current_cert);
							current_cert = NULL;
						}
					}

					if(perform_update &&
						(current_cert_update->cert_status == Cert_Updated ||
						current_cert_update->cert_status == Cert_Inserted))
					{
						current_cert = OP_NEW(SSL_CertificateItem, (*current_cert_update));
						if(current_cert == NULL || current_cert->ErrorRaisedFlag)
						{
							OP_DELETE(current_cert);
							return FALSE;
						}
						current_cert->Into(current_head);
					}
				}

				current_cert_update = current_cert_update->Suc();
			}
		}
	}

	if(optionsManager->updated_external_items)
		updated_external_items = TRUE;

	return TRUE;
}

#endif // relevant support
