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
#include "modules/libssl/protocol/sslbaserec.h"

#ifdef _DEBUG
//#define TST_DUMP
#endif

SSL_Record_Base::SSL_Record_Base()
{
	InternalInit();
}

void SSL_Record_Base::InternalInit()
{
	handled = FALSE;
	IV_field = FALSE;
}

SSL_Record_Base::~SSL_Record_Base()
{
}


void SSL_Record_Base::SetType(SSL_ContentType item)
{
	SetTag(item);
}

void SSL_Record_Base::SetVersion(const SSL_ProtocolVersion &ver)
{
	version = ver;
}

SSL_ContentType SSL_Record_Base::GetType() const
{
	return (SSL_ContentType) GetTag();
}

const SSL_ProtocolVersion &SSL_Record_Base::GetVersion() const
{
	return version;
}
#endif

