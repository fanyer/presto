/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/webfeeds_module.h"
#include "modules/webfeeds/src/webfeeds_api_impl.h"
#include "modules/webfeeds/src/webfeedstorage.h"

void WebfeedsModule::InitL(const OperaInitInfo& info)
{
	webfeeds_api = OP_NEW_L(WebFeedsAPI_impl, ());
	LEAVE_IF_ERROR(webfeeds_api->Init());

#ifdef WEBFEEDS_AUTO_UPDATES_SUPPORT
	webfeeds_api->DoAutomaticUpdates(TRUE);
#endif // WEBFEEDS_AUTO_UPDATES_SUPPORT
}

void WebfeedsModule::Destroy()
{
	OP_DELETE(webfeeds_api);
	webfeeds_api = NULL;
}

BOOL WebfeedsModule::FreeCachedData(BOOL toplevel_context)
{
	// TODO try to actually free some memory

	return FALSE;
}

#endif // WEBFEEDS_BACKEND_SUPPORT
