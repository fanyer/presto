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
#include "modules/libssl/sslrand.h"
#include "modules/libssl/handshake/randfield.h"

#include "modules/util/cleanse.h"


SSL_Random::SSL_Random(int size, BOOL use_time)//: LoadAndWritable(32)
{
	FixedLoadLength(TRUE);
	Resize(size);
	include_time = use_time;
}

#ifdef SSL_FULL_RANDOM_SUPPORT
SSL_Random::SSL_Random(const SSL_Random &other)
{ 
	FixedLoadLength(TRUE);
	operator =(other);
}
#endif

#ifdef SSL_FULL_RANDOM_SUPPORT
SSL_Random &SSL_Random::operator =(const SSL_Random &other)
{
	include_time = other.include_time;
	SSL_secure_varvector16::operator =(other);
	return *this;
}
#endif

void SSL_Random::Generate()
{
	double cl1;
	
	cl1 = g_op_time_info->GetRuntimeMS();
	SSL_SEED_RND((unsigned char *) &cl1,sizeof(cl1));
	
	SSL_RND(*this,GetLength());
	if(include_time && GetDirect() && GetLength() >= SSL_SIZE_UINT_32)
	{
		uint32 gmt_unix_time;

		gmt_unix_time = (uint32) (g_op_time_info->GetTimeUTC()/1000.0);
		byte *target = GetDirect()+3;

		*(target--) = (byte) (gmt_unix_time & 0xFF);
		gmt_unix_time >>= 8;
		*(target--) = (byte) (gmt_unix_time & 0xFF);
		gmt_unix_time >>= 8;
		*(target--) = (byte) (gmt_unix_time & 0xFF);
		gmt_unix_time >>= 8;
		*target = (byte) (gmt_unix_time & 0xFF);
	}
}

#endif // relevant support
