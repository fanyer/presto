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

#include "modules/pi/OpSystemInfo.h"

#include "modules/libssl/sslbase.h"
#include "modules/libssl/handshake/cipherid.h"
#include "modules/libssl/sslrand.h"

#include "modules/util/cleanse.h"

SSL_CipherID::SSL_CipherID() 
: DataStream_SourceBuffer(id, sizeof(id))
{
	id[0]=id[1]=0;
}

SSL_CipherID::SSL_CipherID(uint8 a,uint8 b)
: DataStream_SourceBuffer(id, sizeof(id))
{
	id[0] = a;
	id[1] = b;
}

#ifdef SSL_FULL_CIPHERID_SUPPORT
SSL_CipherID::SSL_CipherID(const SSL_CipherID &other)
: DataStream_SourceBuffer(id, sizeof(id))
{
	id[0] = other.id[0];
	id[1] = other.id[1];
}
#endif

SSL_CipherID &SSL_CipherID::operator =(const SSL_CipherID &other)
{
	id[0] = other.id[0];
	id[1] = other.id[1];
	
	return *this;
} 

int SSL_CipherID::operator ==(const SSL_CipherID &other) const
{
	return (id[0] == other.id[0]  && id[1] == other.id[1]);
}
#endif // relevant support
