/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_ASN1SCERT_H
#define LIBSSL_ASN1SCERT_H

#if defined _NATIVE_SSL_SUPPORT_
class SSL_ASN1Cert_list: public SSL_varvector24_list
{
public:
    SSL_ASN1Cert_list &operator =(SSL_ASN1Cert_list &other){SSL_varvector24_list::operator =(other);return *this;};
    //SSL_ASN1Cert_list &operator =(const SSL_varvector24_list &other){SSL_varvector24_list::operator =(other);return *this;};
    SSL_ASN1Cert_list &operator =(const SSL_varvector32 &other){SSL_varvector24_list::operator =(other);return *this;};
    virtual BOOL Valid(SSL_Alert *msg=NULL) const;

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_ASN1Cert_list";}
#endif
};
#endif

#endif // LIBSSL_ASN1SCERT_H
