/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef __SSLBASE_H
#define __SSLBASE_H

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/libssl/ssluint.h"
#include "modules/libssl/base/sslenum.h" 
#include "modules/util/simset.h"
#include "modules/datastream/fl_lib.h"

class SSL_Alert;
class SSL_Error_Status;
class SSL_varvector32;
class SSL_varvector16;
class SSL_ConnectionState;
class SSL_Cipher; 
class SSL_Hash;
class LoadAndWritableList;

typedef AutoDeleteHead SSL_Head;
typedef DataStream_UIntBase LoadAndWritableUIntBase;
typedef DataStream_UIntVarLength LoadAndWritableUIntVarLength;
typedef DataStream_ByteArray LoadAndWritableByteArray;
typedef LoadAndWritableList SSL_Handshake_Message_Base;

OP_STATUS SetLangString(Str::LocaleString str, OpString &target);

#include "modules/libssl/base/sslexcept.h"
#include "modules/libssl/base/sslalertbase.h"
#include "modules/libssl/base/sslerr.h"
#include "modules/libssl/base/loadtemp.h"
#include "modules/libssl/base/sslalert.h"


#include "modules/libssl/base/sslvar.h"
#include "modules/libssl/base/sslvarl1.h"

#endif
#endif
