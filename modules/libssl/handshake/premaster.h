/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_PREMASTER_H
#define LIBSSL_PREMASTER_H

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/base/sslprotver.h"
#include "modules/libssl/handshake/randfield.h"

#define SSL_PREMASTERLENGTH 48
#define SSL_PREMASTERRANDOMLENGTH 46

class SSL_PreMasterSecret: public SSL_Handshake_Message_Base
{
private:
    SSL_ProtocolVersion client_version;
	SSL_Random random;
    
    friend class SSL_EncryptedPreMasterSecret;
    
public:
    SSL_PreMasterSecret();
    virtual ~SSL_PreMasterSecret();

    void Generate(){random.Generate();};
    //const byte *GetPremasterRandom(){return random.GetDirect();};
    void SetVersion(const SSL_ProtocolVersion &ver){client_version = ver;};
	
#ifdef SSL_SERVER_SUPPORT
    SSL_ProtocolVersion GetVersion() const{return client_version;};
    virtual BOOL Valid(SSL_Alert *msg=NULL) const;
#endif

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_PreMasterSecret";}
#endif
};
#endif
#endif // LIBSSL_PREMASTER_H
