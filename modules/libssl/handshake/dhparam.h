/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_DHPARAM_H
#define LIBSSL_DHPARAM_H

#if defined _NATIVE_SSL_SUPPORT_ && defined SSL_DH_SUPPORT
class SSL_ServerDHParams : public SSL_Handshake_Message_Base
{
private:
    SSL_varvector16 dh_p, dh_g, dh_Ys;
	
public: 
    SSL_ServerDHParams();

    virtual BOOL Valid(SSL_Alert *msg=NULL) const;

	SSL_ServerDHParams &operator =(const SSL_ServerDHParams &other){dh_p = other.dh_p; dh_g = other.dh_g; dh_Ys = other.dh_Ys; return *this;}
	
    const SSL_varvector16 &GetDH_P() const{return dh_p;};
    const SSL_varvector16 &GetDH_G() const{return dh_g;};
    const SSL_varvector16 &GetDH_Ys() const{return dh_Ys;};

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_ServerDHParams";}
#endif
};
#endif // SSL_DH_SUPPORT


#endif // LIBSSL_DHPARAM_H
