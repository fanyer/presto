/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Niklas Beischer <no@opera.com>, Erik Moller <emoller@opera.com>
**
*/

#include "core/pch.h"

#ifdef WEBSOCKETS_SUPPORT

#include "modules/url/protocols/websocket.h"
#include "modules/url/protocols/websocket_protocol.h"

#define WS_NEW_DBG OP_NEW_DBG(__FUNCTION__, "url_websocket")

/* static */
OP_STATUS OpWebSocket::Create(OpWebSocket** socket, OpWebSocketListener* listener, MessageHandler* mh)
{
	WS_NEW_DBG;
	return WebSocketProtocol::Create(socket, listener, mh);
}

/* static */
void OpWebSocket::Destroy(OpWebSocket** socket)
{
	WS_NEW_DBG;
	WebSocketProtocol* obj = static_cast<WebSocketProtocol*>(*socket);
	if (obj->m_state != WS_CLOSED)
		obj->Stop();
	*socket = NULL;

	SComm::SafeDestruction(obj);
}

#endif // WEBSOCKETS_SUPPORT
