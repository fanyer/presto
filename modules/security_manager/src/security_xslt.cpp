/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright 2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef XSLT_SUPPORT

#include "modules/security_manager/include/security_manager.h"
#include "modules/url/url2.h"

OP_STATUS OpSecurityManager_XSLT::CheckXSLTSecurity(const OpSecurityContext &source, const OpSecurityContext &target, BOOL &allowed)
{
	// Default to not allow
	allowed = FALSE;

	// Check context
	const URL &source_url = source.GetURL();
	const URL &target_url = target.GetURL();

	if (source_url.IsValid() && target_url.IsValid())
		if (target_url.Type() == URL_DATA)
			allowed = TRUE;
		else if (g_secman_instance->OriginCheck(source_url, target_url))
			allowed = TRUE;

	return OpStatus::OK;
}

#endif // XSLT_SUPPORT
