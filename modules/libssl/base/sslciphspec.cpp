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
#include "modules/libssl/base/sslciphspec.h"
#include "modules/libssl/protocol/sslmac.h"
#include "modules/libssl/methods/sslcipher.h"
#include "modules/libssl/methods/sslpubkey.h"

SSL_CipherSpec::~SSL_CipherSpec()
{
	OP_DELETE(Method);
	OP_DELETE(MAC);
	OP_DELETE(SignCipherMethod);
}
#endif

