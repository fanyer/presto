/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_OPCRYPT_XP_H
#define MODULES_UTIL_OPCRYPT_XP_H

#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opmemfile.h"

enum OPCF_ACTION { OPCF_DECODE=0, OPCF_ENCODE=1 };

BOOL OPC_Crypt(const uni_char *filename_in, const uni_char *filename_out,
			   BYTE *key, int keylen, OPCF_ACTION action, BOOL compress);

#ifndef HAVE_NO_OPMEMFILE

BOOL OPC_DeCrypt(const uni_char *szInFileName, OpMemFile* &resOpFile, BYTE *userKey, int userKeyLen);

#endif // !HAVE_NO_OPMEMFILE

#endif // !MODULES_UTIL_OPCRYPT_XP_H
