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

#include "core/pch.h"

#ifdef _NATIVE_SSL_SUPPORT_
#include "modules/libssl/sslbase.h"
#include "modules/libssl/methods/sslnull.h"

void SSL_Null_Hash::InitHash()
{
}

const byte *SSL_Null_Hash::CalculateHash(const byte *source,uint32 len)
{
	return source;
}

byte *SSL_Null_Hash::ExtractHash(byte *target)
{
	return target;
}

BOOL SSL_Null_Hash::Valid(SSL_Alert *msg) const
{
	return !Error(msg);
}

SSL_Hash *SSL_Null_Hash::Fork() const
{
	return OP_NEW(SSL_Null_Hash, ());
}
#endif
