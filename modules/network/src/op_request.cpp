/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/network/op_request.h"
#include "modules/network/src/op_request_impl.h"

OP_STATUS OpRequest::Make(OpRequest *&req, OpRequestListener *requestListener, OpURL url, URL_CONTEXT_ID context_id, OpURL referrer)
{
	return OpRequestImpl::Make(req, requestListener, url, context_id, referrer);
}

