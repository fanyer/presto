/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_CERTMESSAGE_H
#define LIBSSL_CERTMESSAGE_H

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/handshake/asn1certlist.h"

class SSL_Certificate_st : public SSL_ASN1Cert_list
{
public:
    SSL_Certificate_st &operator =(SSL_ASN1Cert_list &other){SSL_ASN1Cert_list::operator =(other);return *this;};
    //SSL_Certificate_st &operator =(const SSL_varvector24_list &other){SSL_ASN1Cert_list::operator =(other);return *this;};
    SSL_Certificate_st &operator =(const SSL_varvector32 &other){SSL_ASN1Cert_list::operator =(other);return *this;};

	virtual SSL_KEA_ACTION ProcessMessage(SSL_ConnectionState *pending);
	virtual void SetUpMessageL(SSL_ConnectionState *pending);

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_Certificate_st";}
#endif
};
#endif
#endif // LIBSSL_CERTMESSAGE_H
