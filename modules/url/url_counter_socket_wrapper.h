/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _URL_COUNTER_SOCKET_WRAPPER_H_
#define _URL_COUNTER_SOCKET_WRAPPER_H_

#include "modules/url/url_socket_wrapper.h"

class CounterSocketWrapper : public SocketWrapper
{
public:
	CounterSocketWrapper(OpSocket *socket) : SocketWrapper(socket) { }

	OP_STATUS Send(const void *data, UINT length);
	OP_STATUS Recv(void *buffer, UINT length, UINT *bytes_received);
};

#endif // _URL_COUNTER_SOCKET_WRAPPER_H_
