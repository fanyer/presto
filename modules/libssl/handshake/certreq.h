/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_CERTREQ_H
#define LIBSSL_CERTREQ_H

#if defined _NATIVE_SSL_SUPPORT_
#include "modules/libssl/handshake/tlssighash.h"

class SSL_CertificateRequest_st: public SSL_Handshake_Message_Base
{  
private:
	SSL_ClientCertificateType_list typelist; 

public:
#ifdef _SUPPORT_TLS_1_2
	TLS_SignatureAlgListBase supported_sig_algs;
#endif
	SSL_DistinguishedName_list Authorities;
	
public:
	SSL_CertificateRequest_st();
	SSL_CertificateRequest_st(const SSL_CertificateRequest_st &); // Unref'ed
	virtual ~SSL_CertificateRequest_st();
	
	void Resize(uint8 typec,uint16 authoc);
	
	uint16 GetCtypeCount() const{return (uint16) typelist.Count();}
	
	Loadable_SSL_ClientCertificateType &CtypeItem(uint8);
	const Loadable_SSL_ClientCertificateType &CtypeItem(uint8) const;
	SSL_CertificateRequest_st &operator =(const SSL_CertificateRequest_st&);
	
    void SetCommState(SSL_ConnectionState *item);
	virtual SSL_KEA_ACTION ProcessMessage(SSL_ConnectionState *pending);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_CertificateRequest_st";}
#endif
};
#endif
#endif // LIBSSL_CERTREQ_H
