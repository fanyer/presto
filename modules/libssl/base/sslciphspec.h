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

#ifndef __SSLCIPHSPEC_H
#define __SSLCIPHSPEC_H

#ifdef _NATIVE_SSL_SUPPORT_

class SSL_MAC;
class SSL_GeneralCipher;
class SSL_PublicKeyCipher;

class SSL_CipherSpec{
public:
    SSL_GeneralCipher *Method;
    SSL_MAC   *MAC;
    SSL_PublicKeyCipher *SignCipherMethod;
    SSL_CompressionMethod compression; 
    uint64     Sequence_number; 
public:
    SSL_CipherSpec(): Method(NULL),
		MAC(NULL),
		SignCipherMethod(NULL),
		compression(SSL_Null_Compression),
		Sequence_number(0){};
    ~SSL_CipherSpec();
};
#endif
#endif
