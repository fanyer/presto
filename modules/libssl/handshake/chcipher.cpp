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

#include "modules/util/cleanse.h"
#include "modules/libssl/handshake/chcipher.h"


SSL_ChangeCipherSpec_st::SSL_ChangeCipherSpec_st()
{
	SetTag(1);
	spec.enable_tag = TRUE;
	spec.enable_length = FALSE;
}

void SSL_ChangeCipherSpec_st::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	LoadAndWritableList::PerformActionL(action, arg1, arg2);
	if(action == DataStream::KReadAction && 
		arg2 == DataStream_BaseRecord::RECORD_TAG && GetTag() != 1)
	{
		RaiseAlert(SSL_Fatal, SSL_Illegal_Parameter);
	}
}
#endif // relevant support
