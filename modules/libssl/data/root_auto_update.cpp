/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#if defined LIBSSL_AUTO_UPDATE_ROOTS

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/data/ssl_xml_update.h"
#include "modules/libssl/data/root_auto_retrieve.h"
#include "modules/libssl/data/untrusted_auto_retrieve.h"
#include "modules/libssl/data/root_auto_update.h"
#include "modules/libssl/certs/certinst_base.h"


SSL_Auto_Root_Updater::SSL_Auto_Root_Updater()
{
}

SSL_Auto_Root_Updater::~SSL_Auto_Root_Updater()
{
}

OP_STATUS SSL_Auto_Root_Updater::Construct(OpMessage fin_msg)
{
	URL url;

	url = g_url_api->GetURL(AUTOUPDATE_SCHEME "://" AUTOUPDATE_SERVER "/" AUTOUPDATE_VERSION "/repository.xml");
	g_url_api->MakeUnique(url);

	return SSL_XML_Updater::Construct(url, fin_msg);
}

OP_STATUS SSL_Auto_Root_Updater::ProcessFile()
{
	if(!CheckOptionsManager(0))
		return OpStatus::ERR;

	optionsManager->root_repository_list.Resize(0);
	optionsManager->updated_repository = TRUE;

	if(!parser.EnterElement(UNI_L("repository")))
		return OpStatus::ERR;

	RETURN_IF_ERROR(ProcessRepository());
	parser.LeaveElement();

	return OpStatus::OK;
}

OP_STATUS SSL_Auto_Root_Updater::ProcessRepository()
{
	optionsManager->Init(SSL_LOAD_INTERMEDIATE_CA_STORE| SSL_LOAD_CA_STORE);
	optionsManager->crl_override_list.Clear();
	optionsManager->ocsp_override_list.Resize(0);
	optionsManager->root_repository_list.Resize(0);
	optionsManager->untrusted_repository_list.Resize(0);
	optionsManager->updated_repository = TRUE;
	while(parser.EnterAnyElement())
	{
		if(parser.GetElementName() == UNI_L("repository-item"))
		{
			SSL_varvector32 item;

			RETURN_IF_ERROR(GetHexData(item));

			if(item.GetLength() != 0)
			{
				uint32 i = optionsManager->root_repository_list.Count();
				optionsManager->root_repository_list.Resize(i+1);
				if(!optionsManager->root_repository_list.Error())
					optionsManager->root_repository_list[i] = item;
				if(optionsManager->root_repository_list.Error())
					return optionsManager->root_repository_list.GetOPStatus();

				optionsManager->updated_repository = TRUE;

				BOOL item_present = FALSE;
				while(parser.EnterAnyElement())
				{
					if(parser.GetElementName() == UNI_L("required-item"))
					{
						do {
							const uni_char *before_cond = parser.GetAttribute(UNI_L("before")); // active only before the identified version
							const uni_char *after_cond = parser.GetAttribute(UNI_L("after")); // active only from (inclusive) the identified
							if(before_cond && uni_atoi(before_cond) <= ROOTSTORE_CATEGORY) 
								break; // If we are after this number, don't process
							if(after_cond && uni_atoi(after_cond) > ROOTSTORE_CATEGORY) 
								break; // If we are before this number, don't process

						SSL_varvector32 req_item;

						RETURN_IF_ERROR(GetHexData(req_item));

						if(req_item.GetLength())
						{
							SSL_CertificateItem *cert_item;
							if(!item_present)
							{
								cert_item = optionsManager->Find_CertificateByID(SSL_CA_Store, item);

								if(!cert_item)
									cert_item = optionsManager->Find_CertificateByID(SSL_IntermediateCAStore, item);

								if(cert_item)
									item_present = TRUE;
							}

							if(item_present)
							{
								cert_item = optionsManager->Find_CertificateByID(SSL_CA_Store, req_item);

								if(!cert_item)
									cert_item = optionsManager->Find_CertificateByID(SSL_IntermediateCAStore, req_item);

								if(!cert_item)
								{
										OpAutoPtr<SSL_Auto_Root_Retriever> temp(OP_NEW(SSL_Auto_Root_Retriever, ()));
									if(temp.get() && 
										OpStatus::IsSuccess(temp->Construct(req_item, MSG_SSL_FINISHED_AUTO_UPDATE_ACTION))) // Ignore OOM
									{
										g_ssl_auto_updaters->AddUpdater(temp.release());
									}
								}
							}
						}
						}while(0);
					}
					else if(parser.GetElementName() == UNI_L("update-item"))
					{
						do{
							const uni_char *before_cond = parser.GetAttribute(UNI_L("before")); // active only before the identified version
							const uni_char *after_cond = parser.GetAttribute(UNI_L("after")); // active only from (inclusive) the identified
							if(before_cond && uni_atoi(before_cond) <= ROOTSTORE_CATEGORY) 
								break; // If we are after this number, don't process
							if(after_cond && uni_atoi(after_cond) > ROOTSTORE_CATEGORY) 
								break; // If we are before this number, don't process

							OpString8 date;

							if(OpStatus::IsSuccess(date.SetUTF8FromUTF16(parser.GetText())))
							{
								SSL_CertificateItem *cert_item;
								cert_item = optionsManager->Find_CertificateByID(SSL_CA_Store, item);

								if(!cert_item)
									cert_item = optionsManager->Find_CertificateByID(SSL_IntermediateCAStore, item);

								if(cert_item && cert_item->GetCertificateHandler())
								{
									if(cert_item->GetCertificateHandler()->CheckIsExpired(0, date) == SSL_Expired)
									{
										OpAutoPtr<SSL_Auto_Root_Retriever> temp(OP_NEW(SSL_Auto_Root_Retriever, ()));
										if(temp.get() && 
											OpStatus::IsSuccess(temp->Construct(item, MSG_SSL_FINISHED_AUTO_UPDATE_ACTION))) // Ignore OOM
										{
											g_ssl_auto_updaters->AddUpdater(temp.release());
										}
									}
								}
							}
						}while(0);
					}

					parser.LeaveElement();
				}
			}
		}
		else if(parser.GetElementName() == UNI_L("delete-list"))
		{
			while(parser.EnterAnyElement())
			{
				if(parser.GetElementName() == UNI_L("delete-item"))
				{
					SSL_varvector32 item;

					RETURN_IF_ERROR(GetHexData(item));

					if(item.GetLength() != 0)
					{
						SSL_CertificateItem *cert_item;
						cert_item = optionsManager->Find_CertificateByID(SSL_CA_Store, item);

						if(!cert_item)
							cert_item = optionsManager->Find_CertificateByID(SSL_IntermediateCAStore, item);

						if(cert_item)
						{
							cert_item->cert_status = Cert_Deleted;
						}
					}
				}
				parser.LeaveElement();
			}
		}
		else if(parser.GetElementName() == UNI_L("untrusted-list"))
		{
			optionsManager->Init(SSL_LOAD_UNTRUSTED_STORE);
			while(parser.EnterAnyElement())
			{
				if(parser.GetElementName() == UNI_L("untrusted-item"))
				{
					do {
						const uni_char *before_cond = parser.GetAttribute(UNI_L("before")); // active only before the identified version
						const uni_char *after_cond = parser.GetAttribute(UNI_L("after")); // active only from (inclusive) the identified
						if(before_cond && uni_atoi(before_cond) <= ROOTSTORE_CATEGORY) 
							break; // If we are after this number, don't process
						if(after_cond && uni_atoi(after_cond) > ROOTSTORE_CATEGORY) 
							break; // If we are before this number, don't process
					SSL_varvector32 item;

					RETURN_IF_ERROR(GetHexData(item));

					if(item.GetLength() != 0)
					{
						SSL_CertificateItem *cert_item = optionsManager->Find_CertificateByID(SSL_UntrustedSites, item);

						if(!cert_item)
						{
							OpAutoPtr<SSL_Auto_Untrusted_Retriever> temp(OP_NEW(SSL_Auto_Untrusted_Retriever, ()));
							if(temp.get() && 
								OpStatus::IsSuccess(temp->Construct(item, MSG_SSL_FINISHED_AUTO_UPDATE_ACTION))) // Ignore OOM
							{
								g_ssl_auto_updaters->AddUpdater(temp.release());
							}
						}
					}
					}while(0);
				}
				else if(parser.GetElementName() == UNI_L("untrusted-repository-item"))
				{
					do {
						const uni_char *before_cond = parser.GetAttribute(UNI_L("before")); // active only before the identified version
						const uni_char *after_cond = parser.GetAttribute(UNI_L("after")); // active only from (inclusive) the identified
						if(before_cond && uni_atoi(before_cond) <= ROOTSTORE_CATEGORY) 
							break; // If we are after this number, don't process
						if(after_cond && uni_atoi(after_cond) > ROOTSTORE_CATEGORY) 
							break; // If we are before this number, don't process
						SSL_varvector24 item;

						RETURN_IF_ERROR(GetHexData(item));

						if(item.GetLength() != 0 && !optionsManager->untrusted_repository_list.Contain(item))
						{
							uint32 i = optionsManager->untrusted_repository_list.Count();
							optionsManager->untrusted_repository_list.Resize(i+1);
							if(!optionsManager->untrusted_repository_list.Error())
								optionsManager->untrusted_repository_list[i] = item;
							if(optionsManager->untrusted_repository_list.Error())
								return optionsManager->untrusted_repository_list.GetOPStatus();

							optionsManager->updated_repository = TRUE;
						}
					}while(0);
				}
				parser.LeaveElement();
			}
		}
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
		else if(parser.GetElementName() == UNI_L("crl-location"))
		{
			do{
				const uni_char *before_cond = parser.GetAttribute(UNI_L("before")); // active only before the identified version
				const uni_char *after_cond = parser.GetAttribute(UNI_L("after")); // active only from (inclusive) the identified
				if(before_cond && uni_atoi(before_cond) <= ROOTSTORE_CRL_LEVEL) 
					break; // If we are after this number, don't process
				if(after_cond && uni_atoi(after_cond) > ROOTSTORE_CRL_LEVEL) 
					break; // If we are before this number, don't process

				SSL_varvector32 issuer_id;
				OpString8 crl_url;

				while(parser.EnterAnyElement())
				{
					if(parser.GetElementName() == UNI_L("issuer-id"))
					{
						RETURN_IF_ERROR(GetHexData(issuer_id));
					}
					else if(parser.GetElementName() == UNI_L("crl-url"))
					{
						RETURN_IF_ERROR(crl_url.SetUTF8FromUTF16(parser.GetText()));
				}
				parser.LeaveElement();
			}
				if(issuer_id.GetLength()> 0 && crl_url.HasContent())
				{
					OpAutoPtr<SSL_CRL_Override_Item> crl_overideitem(OP_NEW(SSL_CRL_Override_Item, ()));
					if(crl_overideitem.get() == NULL)
						return OpStatus::ERR_NO_MEMORY;
					crl_overideitem->cert_id = issuer_id;
					RETURN_IF_ERROR(crl_overideitem->cert_id.GetOPStatus());

					crl_overideitem->crl_url.TakeOver(crl_url);
					crl_overideitem.release()->Into(&optionsManager->crl_override_list);
					optionsManager->updated_repository = TRUE;
				}
			}while(0);
		}
#endif // LIBSSL_ENABLE_CRL_SUPPORT
		else if(parser.GetElementName() == UNI_L("ocsp-override"))
		{
			do
			{
				const uni_char *before_cond = parser.GetAttribute(UNI_L("before")); // active only before the identified version
				const uni_char *after_cond = parser.GetAttribute(UNI_L("after")); // active only from (inclusive) the identified
				if(before_cond && uni_atoi(before_cond) <= ROOTSTORE_CRL_LEVEL) 
					break; // If we are after this number, don't process
				if(after_cond && uni_atoi(after_cond) > ROOTSTORE_CRL_LEVEL) 
					break; // If we are before this number, don't process

				OpString8 ocsp_url;
				RETURN_IF_ERROR(ocsp_url.SetUTF8FromUTF16(parser.GetText()));

				uint32 i = optionsManager->ocsp_override_list.Count();
				RETURN_IF_ERROR(optionsManager->ocsp_override_list.Resize(i+1));
				RETURN_IF_ERROR(optionsManager->ocsp_override_list[i].Set(ocsp_url));
				optionsManager->updated_repository = TRUE;
			}
			while(0);
		}
		parser.LeaveElement();
	}
	return OpStatus::OK;
}

OP_STATUS SSL_Auto_Root_Updater::ProcessCertificate()
{
	return OpStatus::OK;
}
#endif // LIBSSL_AUTO_UPDATE_ROOTS
#endif
