/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#if !defined OLDDEBUG_TSTDUMP_H && defined OLDDEBUG_ENABLED
#define OLDDEBUG_TSTDUMP_H

#include "modules/pi/OpSystemInfo.h"

#ifdef _SSLVAR_H_
void DumpTofile(const SSL_varvector32 &source,unsigned long len, const OpStringC8 &text, const OpStringC8 &name);
void DumpTofile(const SSL_varvector32 &source,unsigned long len, const OpStringC8 &text, const OpStringC8 &text1, const OpStringC8 &name);
#endif

void CloseDebugFile(const OpStringC8 &name);
void DumpTofile(const unsigned char *source,unsigned long len, const OpStringC8 &text, const OpStringC8 &text1, const OpStringC8 &name);
void DumpTofile(const unsigned char *source,unsigned long len, const OpStringC8 &text, const OpStringC8 &name);
void DumpTofile(const unsigned char *source,unsigned long len, const OpStringC8 &text, const OpStringC8 &name, int indent_level);
void BinaryDumpTofile(const unsigned char *source,unsigned long len, const OpStringC8 &name);
void PrintfTofile(const OpStringC8 &name,const char *fmt, ...);

void TruncateDebugFiles();
void TruncateDebugFile(const OpStringC8 &name);
void CloseDebugFiles();

#ifdef _VA_LIST_DEFINED
void PrintvfTofile(const OpStringC8 &name,const OpStringC8 &fmt, va_list args);
#endif

#endif
