/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/libopeay/core_includes.h"

#if defined(_SSL_USE_OPENSSL_) || defined(EXTERNAL_SSL_OPENSSL_IMPLEMENTATION)
extern "C" {
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/crypto/des/ecb3_enc.c"
}
#endif 
