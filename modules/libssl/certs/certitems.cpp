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
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/options/certitem.h"
#include "modules/libssl/certs/certhandler.h"
#include "modules/libssl/methods/sslpubkey.h"

#include "modules/libssl/debug/tstdump2.h"

SSL_CertificateItem::SSL_CertificateItem()
{
	certificatetype = SSL_rsa_sign;
	handler = NULL; 
	WarnIfUsed = FALSE;
	DenyIfUsed = FALSE;
	PreShipped = FALSE;

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	IsExternalCert = FALSE;
	external_key = NULL;
#endif

	cert_status = Cert_Not_Updated;
	ForwardToThis(name, certificate);
	ForwardToThis(private_key_salt, private_key);
	public_key_hash.ForwardTo(this);

	trusted_until = 0;
	trusted_for_port = 0;
	accepted_for_kind = CertAccept_Site;
}

SSL_CertificateItem::SSL_CertificateItem(const SSL_CertificateItem &other)
    : SSL_Error_Status(), Link(),
	  name(other.name),
      certificate(other.certificate),
      private_key_salt(other.private_key_salt),
      private_key(other.private_key),
      public_key_hash(other.public_key_hash),
	  trusted_until(other.trusted_until),
	  trusted_for_name(other.trusted_for_name),
	  trusted_for_port(other.trusted_for_port),
	  accepted_for_kind(other.accepted_for_kind)
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	  , external_key(NULL)
#endif
{
	InternalInit(other);
}

void SSL_CertificateItem::InternalInit(const SSL_CertificateItem &other)
{
	OP_STATUS op_err;

	handler = NULL; 
	certificatetype = other.certificatetype;
    WarnIfUsed = other.WarnIfUsed;
    DenyIfUsed = other.DenyIfUsed;
	PreShipped = other.PreShipped;
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	ev_policies = other.ev_policies;
	decoded_ev_policies = other.decoded_ev_policies;
#endif
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	IsExternalCert = other.IsExternalCert;
	if(other.external_key)
	{
		external_key = other.external_key->Fork();
		if(external_key == NULL)
			RaiseAlert(OpStatus::ERR_NO_MEMORY);
	}
#endif
	cert_status = Cert_Not_Updated;

	op_err = cert_title.Set(other.cert_title);
	if(OpStatus::IsError(op_err))
		RaiseAlert(op_err);

	ForwardToThis(name, certificate);
	ForwardToThis(private_key_salt, private_key);
	public_key_hash.ForwardTo(this);
}

SSL_CertificateItem::~SSL_CertificateItem()
{
	if(InList())
		Out();
	OP_DELETE(handler);
	handler = NULL;
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	OP_DELETE(external_key);
	external_key = NULL;
#endif
}


OP_STATUS SSL_CertificateItem::SetCertificate(SSL_ASN1Cert &cert_item, SSL_CertificateHandler *certificates, uint24 item)
{
	OpString_list info;

	certificatetype = certificates->CertificateType(item);
	certificate = cert_item;
	certificates->GetSubjectName(item,name);
	cert_title.Empty();
	return certificates->GetCommonName(item, cert_title);
}

SSL_CertificateHandler *SSL_CertificateItem::GetCertificateHandler(SSL_Alert *msg)
{
	if(handler == NULL && certificate.GetLength() >0)
	{
		handler = g_ssl_api->CreateCertificateHandler();
		if(handler != NULL)
		{
			handler->LoadCertificate(certificate);
			if(handler->Error(msg))
			{
				OP_DELETE(handler);
				handler = NULL;
			}
		}
		else
		{
			if(msg)
				msg->RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
		}
	}

	if(handler)
		handler->ResetError();

	return handler;
}
#endif // relevant support
