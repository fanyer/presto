/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#if !defined(ADS12) || defined(ADS12_TEMPLATING) // see end of streamtype.cpp

#include "modules/libssl/sslbase.h"
#include "modules/libssl/keyex/sslkeyex.h"
#include "modules/libssl/handshake/certreq.h"

SSL_CertificateRequest_st::SSL_CertificateRequest_st()
{
	AddItem(typelist);
#ifdef _SUPPORT_TLS_1_2
	AddItem(supported_sig_algs);
	supported_sig_algs.SetEnableRecord(FALSE);
#endif
	AddItem(Authorities);
}

#if 0 /* Unref YNP */
SSL_CertificateRequest_st::SSL_CertificateRequest_st(const SSL_CertificateRequest_st &old)
{
	AddItem(typelist);
#ifdef _SUPPORT_TLS_1_2
	AddItem(supported_sig_algs);
	supported_sig_algs.SetEnableRecord(FALSE);
#endif
	AddItem(Authorities);
}
#endif // 0


SSL_CertificateRequest_st::~SSL_CertificateRequest_st()
{
}

void SSL_CertificateRequest_st::SetCommState(SSL_ConnectionState *connstate)
{
#ifdef _SUPPORT_TLS_1_2
	if(connstate->version.Compare(3,3) >= 0)
		supported_sig_algs.SetEnableRecord(TRUE);
#endif
}

SSL_KEA_ACTION SSL_CertificateRequest_st::ProcessMessage(SSL_ConnectionState *pending)
{
	OP_ASSERT(pending && pending->key_exchange);

	if(pending->anonymous_connection)
	{
		RaiseAlert(SSL_Fatal,SSL_Handshake_Failure);
		return SSL_KEA_Handle_Errors;
	}
	/* TODO: Handle feature test */

	return pending->key_exchange->ReceivedCertificateRequest(this);
}

void SSL_CertificateRequest_st::Resize(uint8 typec,uint16 authoc)
{
	typelist.Resize(typec);
	Authorities.Resize(authoc);
}


Loadable_SSL_ClientCertificateType &SSL_CertificateRequest_st::CtypeItem(uint8 item)
{
	return typelist[item];
}


#if 0 /* Unref YNP */
const SSL_ClientCertificateType &SSL_CertificateRequest_st::CtypeItem(uint8 item) const 
{
	if(item >= certificate_types.GetLength())
		item = certificate_types.GetLength()-1;
	return ((const SSL_ClientCertificateType *) certificate_types.GetDirect())[item];
}

SSL_DistinguishedName &SSL_CertificateRequest_st::AuthNameItem(uint16 item)
{
    return authorities[item];
}

const SSL_ClientCertificateType &SSL_CertificateRequest_st::CtypeItem(uint8 item) const
{ 
    static SSL_ClientCertificateType temp;

    if (item < typelist.length )
        return typelist.certificate_types[item];
    else {
        temp = SSL_Illegal_Cert_Type;
        return temp;
    }
}

const SSL_DistinguishedName &SSL_CertificateRequest_st::AuthNameItem(uint16 item) const
{
    return authorities[item];
}

bool Valid(SSL_ClientCertificateType item,SSL_Alert *msg)
{
	switch(item)
	{
    case SSL_rsa_sign :
    case SSL_dss_sign :
    case SSL_rsa_fixed_dh :
    case SSL_dss_fixed_dh :
    case SSL_rsa_ephemeral_dh  :
    case SSL_dss_ephemeral_dh :
		break;

    default:
        if(msg != NULL)
            msg->Set(SSL_Internal, SSL_InternalError);
		return FALSE;
	} 

	return TRUE;
}
#endif

#endif // !ADS12 or ADS12_TEMPLATING
#endif // _NATIVE_SSL_SUPPORT_
