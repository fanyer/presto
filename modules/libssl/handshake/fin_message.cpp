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
#include "modules/libssl/handshake/fin_message.h"

#include "modules/util/cleanse.h"

void SSL_Finished_st::SetUpMessageL(SSL_ConnectionState *pending)
{
	pending->version_specific->GetFinishedMessage(/*pending->client*/ TRUE, Message);
	// TODO: Make conditional on serverfinished received
	//pending->version_specific->GetFinishedMessage(!pending->client, pending->prepared_server_finished);
	pending->last_client_finished = Message;
}

SSL_KEA_ACTION SSL_Finished_st::ProcessMessage(SSL_ConnectionState *pending)
{
	OP_ASSERT(pending && pending->version_specific);

	if(Message !=  pending->prepared_server_finished)
	{
		RaiseAlert(SSL_Fatal,SSL_Handshake_Failure);
		return SSL_KEA_Handle_Errors;
	}

	pending->last_server_finished = Message;
	pending->version_specific->SessionUpdate(Session_Finished_Confirmed);
	return SSL_KEA_No_Action;
}
#endif // relevant support
