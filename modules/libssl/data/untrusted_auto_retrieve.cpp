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
#include "modules/libssl/data/untrusted_auto_retrieve.h"
#include "modules/libssl/certs/certinst_base.h"


SSL_Auto_Untrusted_Retriever::SSL_Auto_Untrusted_Retriever()
{
}

SSL_Auto_Untrusted_Retriever::~SSL_Auto_Untrusted_Retriever()
{
}

OP_STATUS SSL_Auto_Untrusted_Retriever::Construct(SSL_varvector32 &issuer_id, OpMessage fin_msg)
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

	issuer_query.SetConcat(AUTOUPDATE_SCHEME "://" AUTOUPDATE_SERVER "/" AUTOUPDATE_VERSION "/untrusted/", issuer_id_string);

	url = g_url_api->GetURL(issuer_query.CStr());
	g_url_api->MakeUnique(url);
	
	return SSL_XML_Updater::Construct(url, fin_msg);
}

OP_STATUS SSL_Auto_Untrusted_Retriever::ProcessFile()
{
	if(!CheckOptionsManager(SSL_LOAD_CA_STORE | SSL_LOAD_INTERMEDIATE_CA_STORE | SSL_LOAD_UNTRUSTED_STORE))
		return OpStatus::ERR;

	if(!parser.EnterElement(UNI_L("untrusted-certificate")))
		return OpStatus::ERR;

	RETURN_IF_ERROR(ProcessCertificate());

	parser.LeaveElement();
	return OpStatus::OK;
}

OP_STATUS SSL_Auto_Untrusted_Retriever::ProcessCertificate()
{
	SSL_ASN1Cert certificate_data;
	OpString shortname;

	while(parser.EnterAnyElement())
	{
		SSL_varvector32 *base_64_target = NULL;

		if(parser.GetElementName() == UNI_L("certificate-data"))
			base_64_target = &certificate_data;
		else if(parser.GetElementName() == UNI_L("shortname"))
			OpStatus::Ignore(shortname.Set(parser.GetText()));


		if(base_64_target != NULL)
		{
			RETURN_IF_ERROR(GetBase64Data(*base_64_target));
			if(base_64_target->GetLength() == 0)
				return OpStatus::ERR;
		}
		parser.LeaveElement();
	}

	SSL_CertificateItem *cert_item = optionsManager->AddUnTrustedCert(certificate_data);
	if(cert_item && shortname.HasContent())
		OpStatus::Ignore(cert_item->cert_title.Set(shortname));

	return OpStatus::OK;
}
#endif // LIBSSL_AUTO_UPDATE_ROOTS
#endif
