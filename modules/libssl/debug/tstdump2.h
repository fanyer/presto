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

#ifndef _SSL_TSTDUMP2_H_
#define _SSL_TSTDUMP2_H_

#include "modules/olddebug/tstdump.h"

#if defined  _SSLVAR_H_ && defined _NATIVE_SSL_SUPPORT_
void DumpTofile(const SSL_varvector32 &source,unsigned long len, const OpStringC8 &text, const OpStringC8 &name);
void DumpTofile(const SSL_varvector32 &source,unsigned long len, const OpStringC8 &text, const OpStringC8 &text1, const OpStringC8 &name);
void BinaryDumpTofile(const SSL_varvector32 &source, unsigned long len, const OpStringC8 &name);
void BinaryDumpTofile(const SSL_varvector32 &source, const OpStringC8 &name);
#endif

#endif // _SSL_TSTDUMP2_H_
