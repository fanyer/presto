/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_RSAPARAM_H
#define LIBSSL_RSAPARAM_H

#if defined _NATIVE_SSL_SUPPORT_

class SSL_ServerRSAParams: public SSL_Handshake_Message_Base
{
public:
    SSL_varvector16 Modulus;
    SSL_varvector16 Exponent;
    
public:
    SSL_ServerRSAParams();
    virtual BOOL Valid(SSL_Alert *msg=NULL) const;
	SSL_ServerRSAParams &operator =(const SSL_ServerRSAParams &other){Modulus = other.Modulus; Exponent = other.Exponent; return *this;}

#ifdef SSL_SERVER_SUPPORT
    SSL_varvector16 &GetModulus(){return rsa_modulus;};
    SSL_varvector16 &GetExponent(){return rsa_exponent;};
#endif

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_ServerRSAParams";}
#endif
};
#endif
#endif // LIBSSL_RSAPARAM_H
