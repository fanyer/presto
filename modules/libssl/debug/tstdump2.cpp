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
#include "modules/libssl/debug/tstdump2.h"

#if defined(_DEBUG) || defined(_DEBUG_) ||  defined(_RELEASE_DEBUG_VERSION)

void DumpTofile(const SSL_varvector32 &source,unsigned long len, const OpStringC8 &text, const OpStringC8 &name)
{
	DumpTofile(source.GetDirect(),len,text,name);
}


void DumpTofile(const SSL_varvector32 &source,unsigned long len, const OpStringC8 &text,const OpStringC8 &text1, const OpStringC8 &name)
{
	DumpTofile(source.GetDirect(),len,text,text1,name);
}

void BinaryDumpTofile(const SSL_varvector32 &source,unsigned long len, const OpStringC8 &name)
{
	BinaryDumpTofile(source.GetDirect(), len, name);
}

void BinaryDumpTofile(const SSL_varvector32 &source, const OpStringC8 &name)
{
	BinaryDumpTofile(source.GetDirect(), source.GetLength(), name);
}

#endif
#endif        
