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
#include "modules/libssl/handshake/premaster.h"

SSL_PreMasterSecret::SSL_PreMasterSecret()
: random(SSL_PREMASTERRANDOMLENGTH, FALSE)
{
	AddItem(client_version);
	AddItem(random);
}

SSL_PreMasterSecret::~SSL_PreMasterSecret()
{
}

#endif
