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
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/handshake/hello_done.h"

#include "modules/util/cleanse.h"

SSL_KEA_ACTION SSL_Hello_Done_st::ProcessMessage(SSL_ConnectionState *pending)
{
	OP_ASSERT(pending && pending->version_specific);

	pending->version_specific->SessionUpdate(Session_Server_Done_Received);
	return SSL_KEA_Prepare_Premaster;
}

#endif // relevant support
