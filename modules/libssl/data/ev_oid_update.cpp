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

#if defined _NATIVE_SSL_SUPPORT_ && defined SSL_CHECK_EXT_VALIDATION_POLICY

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/data/ev_oid_update.h"

SSL_EV_OID_updater::SSL_EV_OID_updater()
{
}

SSL_EV_OID_updater::~SSL_EV_OID_updater()
{
}

OP_STATUS SSL_EV_OID_updater::Construct(OpMessage fin_msg)
{
	URL url;

	url = g_url_api->GetURL(AUTOUPDATE_SCHEME "://" AUTOUPDATE_SERVER "/" AUTOUPDATE_VERSION "/ev-oids.xml");
	g_url_api->MakeUnique(url);

	return SSL_XML_Updater::Construct(url, fin_msg);
}

OP_STATUS SSL_EV_OID_updater::ProcessFile()
{
	if(!CheckOptionsManager(SSL_LOAD_CA_STORE))
		return OpStatus::ERR;

	int i=0; 
	SSL_CertificateItem *cert_item = NULL;
	
	while((cert_item = optionsManager->Find_Certificate(SSL_CA_Store, i++)) != NULL)
	{
		if(cert_item->ev_policies.GetLength()>0)
		{
			cert_item->ev_policies.Resize(0);
			cert_item->decoded_ev_policies.Resize(0);
			cert_item->cert_status = Cert_Updated;
		}
	}

	if(!parser.EnterElement(UNI_L("ev-oid-list")))
		return OpStatus::ERR;

	while(parser.EnterElement(UNI_L("issuer")))
	{
		RETURN_IF_ERROR(ProcessCertificate());
		parser.LeaveElement();
	}
	parser.LeaveElement();

	return OpStatus::OK;
}

OP_STATUS SSL_EV_OID_updater::ProcessCertificate()
{
	SSL_DistinguishedName issuer_name;
	SSL_varvector32 issuer_keyid;
	SSL_varvector32 ev_oids;

	while(parser.EnterAnyElement())
	{
		SSL_varvector32 *base_64_target = NULL;
		/*
		if(parser.GetElementName() == UNI_L("issuer-name") ||
			parser.GetElementName() == UNI_L("ev-oid-text"))
		{
			// Ignore
		}
		else
		*/
		if(parser.GetElementName() == UNI_L("issuer-id"))
		{
			base_64_target = &issuer_name;
		}
		else  if(parser.GetElementName() == UNI_L("issuer-key"))
		{
			base_64_target = &issuer_keyid; // SHA-1 hash
		}
		else  if(parser.GetElementName() == UNI_L("ev-oids"))
			base_64_target = &ev_oids;

		if(base_64_target != NULL)
		{
			RETURN_IF_ERROR(GetBase64Data(*base_64_target));
			if(base_64_target->GetLength() == 0)
				return OpStatus::ERR;
		}
		parser.LeaveElement();
	}

	SSL_CertificateItem *cert_item = optionsManager->Find_Certificate(SSL_CA_Store, issuer_name);
	while(cert_item)
	{
		SSL_CertificateHandler *cert = cert_item->GetCertificateHandler();
		if(cert)
		{
			SSL_varvector16 keyhash;
			cert->GetPublicKeyHash(0, keyhash);

			if(keyhash == issuer_keyid)
			{
				cert_item->ev_policies = ev_oids;
				cert_item->cert_status = Cert_Updated;
				break;
			}
		}
		cert_item = optionsManager->Find_Certificate(SSL_CA_Store, issuer_name, cert_item);
	}

	return OpStatus::OK;
}

#endif
