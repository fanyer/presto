/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _URL_TOOLS_
#define _URL_TOOLS_

#include "modules/util/opstring.h"
#include "modules/pi/network/OpSocketAddress.h"

#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define URL_ERRSTR(p,x) Str::##p##_##x
#else
#define URL_ERRSTR(p,x) x
#endif



enum OpSocketAddressNetType;

OP_STATUS OP_DeleteFile(const uni_char *filename, const uni_char *directory= NULL);
OP_STATUS OP_RenameFile(const uni_char *oldfilename,const uni_char *newfilename);
OP_STATUS OP_SafeFileReplace(const uni_char *directory, const uni_char *target_filename, const uni_char *replace_file, const uni_char *tempold_file);

void IncFileString(uni_char* file_str, int max, int min, int i);

OP_STATUS ConvertUrlStatusToOpStatus(unsigned long url_status);
#ifdef HC_CAP_ERRENUM_AS_STRINGENUM 
/** Deprecated once HC_CAP_ERRENUM_AS_STRINGENUM is defined, when url_status will correspond to Str::LocaleString values */
Str::LocaleString DEPRECATED(ConvertUrlStatusToLocaleString(unsigned long url_status));
#else
Str::LocaleString ConvertUrlStatusToLocaleString(unsigned long url_status);
#endif

BOOL ResolveUrlNameL(const OpStringC& name_in, OpString& resolved_out, BOOL entered_by_user=FALSE);

void EscapeFileURL(uni_char* esc_url, uni_char* url, BOOL include_specials= FALSE, BOOL escape_8bit= FALSE);

Str::LocaleString GetCrossNetworkError(OpSocketAddressNetType referrer_net, OpSocketAddressNetType host_net);

BOOL IsRedirectResponse(int response);
#endif
