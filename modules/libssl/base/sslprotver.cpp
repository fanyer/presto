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

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/base/sslprotver.h"
#include "modules/libssl/options/sslopt.h"

#ifdef _DEBUG
//#define TST_DUMP
#endif


SSL_ProtocolVersion::SSL_ProtocolVersion() 
: DataStream_SourceBuffer(buf, sizeof(buf))
{
	SSL_Major_Protocol_Version = 3;
	SSL_Minor_Protocol_Version = 1;
}

SSL_ProtocolVersion::SSL_ProtocolVersion(uint8 ma, uint8 mi)
: DataStream_SourceBuffer(buf, sizeof(buf))
{
	SSL_Major_Protocol_Version = ma;
	SSL_Minor_Protocol_Version = mi;
	OP_ASSERT(SSL_Major_Protocol_Version != 0);
}

SSL_ProtocolVersion::SSL_ProtocolVersion(const SSL_ProtocolVersion &other) 
: DataStream_SourceBuffer(buf, sizeof(buf))
{
	SSL_Major_Protocol_Version = other.SSL_Major_Protocol_Version;
	SSL_Minor_Protocol_Version = other.SSL_Minor_Protocol_Version;
	OP_ASSERT(SSL_Major_Protocol_Version != 0);
}


SSL_ProtocolVersion &SSL_ProtocolVersion::operator =(const SSL_ProtocolVersion &other)
{
	SSL_Major_Protocol_Version = other.SSL_Major_Protocol_Version;
	SSL_Minor_Protocol_Version = other.SSL_Minor_Protocol_Version;
	OP_ASSERT(SSL_Major_Protocol_Version != 0);
	
	return *this;
}


OP_STATUS SSL_ProtocolVersion::SetFromFeatureStatus(TLS_Feature_Test_Status protocol)
{
	switch (protocol)
	{
		case TLS_Test_1_2_Extensions:
		case TLS_1_2_and_Extensions:
			if (g_securityManager->Enable_TLS_V1_2)
			{
				Set(3,3);
				break;
			}
			// Fall through
		//case TLS_Test_1_1_Extensions:
		//case TLS_1_1_and_Extensions:
			if (g_securityManager->Enable_TLS_V1_1)
			{
				Set(3,2);
				break;
			}
			// Fall through

		case TLS_1_0_only:
		case TLS_Test_1_0:
		case TLS_Test_1_0_Extensions:
		case TLS_1_0_and_Extensions:
			if (g_securityManager->Enable_TLS_V1_0)
			{
				Set(3,1);
				break;
			}
			// Fall through

		case TLS_Test_SSLv3_only:
		case TLS_SSL_v3_only:
			Set(3,0);
			break;

		default:
			return OpStatus::ERR;
	}
	return OpStatus::OK;
}

int SSL_ProtocolVersion::Compare(uint8 major, uint8 minor) const
{
	if(SSL_Major_Protocol_Version < major)
		return -1;
	if(SSL_Major_Protocol_Version > major)
		return 1;
	if(SSL_Minor_Protocol_Version < minor)
		return -1;
	if(SSL_Minor_Protocol_Version > minor)
		return 1;
	return 0;
}

#endif

