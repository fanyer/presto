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
#include "modules/libssl/certs/certinstaller.h"

class UpdateRootStoreInstaller : public SSL_Certificate_Installer
{
public:
	UpdateRootStoreInstaller(){overwrite_exisiting = TRUE;}
};


SSL_Auto_Root_Retriever::SSL_Auto_Root_Retriever()
{
}

SSL_Auto_Root_Retriever::~SSL_Auto_Root_Retriever()
{
}

OP_STATUS SSL_Auto_Root_Retriever::Construct(SSL_varvector32 &issuer_id, OpMessage fin_msg)
{
	URL url;

	if(issuer_id.GetLength() == 0)
		return OpStatus::ERR_OUT_OF_RANGE; 

	OpString8 issuer_id_string;
	uint32 i, id_len= issuer_id.GetLength();
	unsigned char *id_bytes = issuer_id.GetDirect();

	char *id_string = issuer_id_string.Reserve(((id_len+1)/2)*5+10);
	RETURN_OOM_IF_NULL(id_string);

	i =0;
	op_sprintf(id_string, "%.2X", id_bytes[i]);
	for(i++, id_string+=2;i<id_len; i+=2, id_string+=5)
		op_sprintf(id_string, (i+2 < id_len ? "%.2X%.2X_" : "%.2X%.2X.xml"), id_bytes[i], id_bytes[i+1]);

	OpString8 issuer_query;

	issuer_query.SetConcat(AUTOUPDATE_SCHEME "://" AUTOUPDATE_SERVER "/" AUTOUPDATE_VERSION "/roots/", issuer_id_string);

	url = g_url_api->GetURL(issuer_query);
	g_url_api->MakeUnique(url);
	
	return SSL_XML_Updater::Construct(url, fin_msg);
}

OP_STATUS SSL_Auto_Root_Retriever::ProcessFile()
{
	if(!CheckOptionsManager(SSL_LOAD_CA_STORE | SSL_LOAD_INTERMEDIATE_CA_STORE))
		return OpStatus::ERR;

	if(!parser.EnterElement(UNI_L("certificates")))
		return OpStatus::ERR;

	while(parser.EnterElement(UNI_L("certificate")))
	{
		RETURN_IF_ERROR(ProcessCertificate());
		parser.LeaveElement();
	}
	parser.LeaveElement();
	return OpStatus::OK;
}

OP_STATUS SSL_Auto_Root_Retriever::ProcessCertificate()
{
	SSL_ASN1Cert certificate_data;
#if defined SSL_CHECK_EXT_VALIDATION_POLICY
	SSL_varvector32 ev_oids;
#endif
	BOOL warn = FALSE, deny =FALSE;
	OpString suggested_name;

	const uni_char *before_cond = parser.GetAttribute(UNI_L("before")); // active only before the identified version
	const uni_char *after_cond = parser.GetAttribute(UNI_L("after")); // active only from (inclusive) the identified
	if(before_cond && uni_atoi(before_cond) <= ROOTSTORE_CATEGORY) 
		return OpStatus::OK; // If we are after this number, don't process
	if(after_cond && uni_atoi(after_cond) > ROOTSTORE_CATEGORY) 
		return OpStatus::OK; // If we are before this number, don't process

	while(parser.EnterAnyElement())
	{
		SSL_varvector32 *base_64_target = NULL;

		if(parser.GetElementName() == UNI_L("certificate-data"))
			base_64_target = &certificate_data;
		else if(parser.GetElementName() == UNI_L("warn"))
			warn = TRUE;
		else if(parser.GetElementName() == UNI_L("deny"))
			deny= TRUE;
		else if(parser.GetElementName() == UNI_L("shortname"))
		{
			OpStatus::Ignore(suggested_name.Set(parser.GetText()));
			suggested_name.Strip();
			OP_ASSERT(suggested_name.FindFirstOf(UNI_L("\n\r")) == KNotFound);
		}
#if defined SSL_CHECK_EXT_VALIDATION_POLICY
		else if(parser.GetElementName() == UNI_L("ev-oids"))
			base_64_target = &ev_oids;
#endif
		if(base_64_target != NULL)
		{
			RETURN_IF_ERROR(GetBase64Data(*base_64_target));
			if(base_64_target->GetLength() == 0)
				return OpStatus::ERR;
		}
		parser.LeaveElement();
	}

	OpAutoPtr<UpdateRootStoreInstaller> installer(OP_NEW(UpdateRootStoreInstaller, ()));
	if(installer.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;

	SSL_Certificate_Installer_flags flags(SSL_CA_Store, warn, deny);

	RETURN_IF_ERROR(installer->Construct(certificate_data, flags, optionsManager));

	installer->SetSuggestedName(suggested_name);

	if(installer.get() ==  NULL)
		return OpStatus::ERR_NO_MEMORY;

	installer->Raise_SetPreshippedFlag(TRUE);
	RETURN_IF_ERROR(installer->StartInstallation());

#if defined SSL_CHECK_EXT_VALIDATION_POLICY
	SSL_CertificateItem *cert_item = NULL;
	int n=0;
	
	while((cert_item = optionsManager->Find_Certificate(SSL_CA_Store, n))!= NULL)
	{
		n++;
		if(cert_item->certificate == certificate_data)
		{
			cert_item->ev_policies = ev_oids;
			if(cert_item->cert_status == Cert_Not_Updated)
				cert_item->cert_status = Cert_Updated;
			break;
		}
	}
#endif

	return OpStatus::OK;
}
#endif // LIBSSL_AUTO_UPDATE_ROOTS
#endif
