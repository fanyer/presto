/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OpSSLSocket.h
 *
 * SSL-capable socket.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef OP_SSL_SOCKET_H
#define OP_SSL_SOCKET_H

#ifdef _EXTERNAL_SSL_SUPPORT_

#include "modules/pi/network/OpSocket.h"

class OpSSLSocket: public OpSocket
{
public:
	static OP_STATUS Create(OpSocket** socket, OpSocketListener* listener, BOOL secure = FALSE);
};

#endif // _EXTERNAL_SSL_SUPPORT_

#endif // OP_SSL_SOCKET_H
