/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/url/url2.h"
#include "modules/network/op_url.h"
#include "modules/network/op_request.h"
#include "modules/network/src/op_request_impl.h"
#include "modules/network/op_batch_request.h"
#include "modules/network/src/op_batch_request_impl.h"
#include "modules/url/tools/url_util.h"
#include "modules/locale/locale-enum.h"
#include "modules/cache/url_dd.h"

OP_STATUS OpBatchRequest::Make(OpBatchRequest *&req, OpBatchRequestListener *listener)
{
	return OpBatchRequestImpl::Make(req, listener);
}

