/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/network/op_resource.h"
#include "modules/network/src/op_resource_impl.h"

BOOL OpResource::QuickLoad(OpResource *&resource, OpURL url, BOOL guess_content_type)
{
	return OpResourceImpl::QuickLoad(resource, url, guess_content_type);
}
