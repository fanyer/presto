/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef SSLSECPROFILE_H
#define SSLSECPROFILE_H

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/libssl/handshake/hand_types.h"
#include "modules/libssl/handshake/cipherid.h"

class SSL_Security_ProfileList : public Link, public OpReferenceCounter
{
public:
    SSL_CipherSuites original_ciphers;
    SSL_CipherSuites ciphers;
    SSL_varvectorCompress compressions; 
	
	virtual ~SSL_Security_ProfileList(){};
	SSL_Security_ProfileList(){};

private:
    SSL_Security_ProfileList(const SSL_Security_ProfileList &);
    SSL_Security_ProfileList(const SSL_Security_ProfileList *);
};

typedef OpSmartPointerWithDelete<SSL_Security_ProfileList> SSL_Security_ProfileList_Pointer;

#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSLSECPROFILE_H */
