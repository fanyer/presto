/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/pi/OpSystemInfo.h"

#include "modules/libssl/sslbase.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/keyex/sslkeyex.h"
#include "modules/libssl/handshake/keyex_client.h"

#include "modules/util/cleanse.h"

void SSL_Client_Key_Exchange_st::SetUpMessageL(SSL_ConnectionState *pending)
{
	OP_ASSERT(pending && pending->key_exchange);

	Message = pending->key_exchange->Encrypted_PreMasterSecret();
	pending->key_exchange->Encrypted_PreMasterUsed();

	if (pending->version.Major() > 3 || (pending->version.Major() == 3 && pending->version.Minor() > 0))
		Message.FixedLoadLength(FALSE);
}
#endif // relevant support
