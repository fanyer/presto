/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT

#include "modules/url/url_man.h"
#include "modules/util/opstring.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/security_manager/src/security_unite.h"
#include "modules/dochand/docman.h"

OP_STATUS
OpSecurityManager_Unite::CheckUniteSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed)
{
	const URL &url1 = source.GetURL();
	const URL &url2 = target.GetURL();

	OP_ASSERT(url2.GetAttribute(URL::KIsUniteServiceAdminURL));  // don't call this check for non-admin URLs

	if (url1.IsEmpty() || url2.IsEmpty() || !url2.GetAttribute(URL::KIsUniteServiceAdminURL))
	{
		allowed = FALSE;
		return OpStatus::OK;
	}

	allowed = AllowUniteURL(url1, url2, source.IsTopDocument());

	return OpStatus::OK;
}

/* static */ BOOL
OpSecurityManager_Unite::AllowUniteURL(const URL& source, const URL& target, BOOL is_top_document)
{
	OP_ASSERT(target.GetAttribute(URL::KIsUniteServiceAdminURL));

	// comes from warning page which has allowed it
	if (source.Type() == URL_OPERA)
		return TRUE;

	//  Only admin URLs or local services are allowed to load admin URLs without warning
	if (!source.GetAttribute(URL::KIsUniteServiceAdminURL) && !source.GetAttribute(URL::KIsLocalUniteService))
		return FALSE;

	return AllowInterUniteConnection(target, source, is_top_document);
}

/* static */ BOOL
OpSecurityManager_Unite::AllowInterUniteConnection(const URL& source_url, const URL& target_url, BOOL is_top_document)
{
	// Consider the following:
	//
	// service1 = unite://device.username.operaunite.com/service1
	// service2 = unite://device.username.operaunite.com/service2
	// rootservice1 = unite://device.username.operaunite.com/
	// rootservice2 = unite://device.username.operaunite.com/_root
	//
	// where:
	//
	// service1 != service2
	// rootservice1 == rootservice2

	BOOL source_is_admin = !!source_url.GetAttribute(URL::KIsUniteServiceAdminURL);

	if (!source_is_admin && !source_url.GetAttribute(URL::KIsLocalUniteService))
	{
		return FALSE;
	}

	OpStringC8 target_service = target_url.GetAttribute(URL::KPath);
	OpStringC8 source_service = source_url.GetAttribute(URL::KPath);

	const char* target = target_service.CStr();
	const char* source = source_service.CStr();

	OP_ASSERT(target && source);

	// Normalize the URLs
	if (*target == '/')
		target++;

	if (*source == '/')
		source++;

	const char* target_end = op_strchr(target, '/');
	const char* source_end = op_strchr(source, '/');

	int target_len = target_end ? target_end - target : op_strlen(target);
	int source_len = source_end ? source_end - source : op_strlen(source);

	if (op_strncmp("_root", target, target_len) == 0)
	{
		target = "";
		target_len = 0;
	}

	if (op_strncmp("_root", source, source_len) == 0)
	{
		source = "";
		source_len = 0;
	}

	// Is same service, allow
	if ((source_is_admin || is_top_document)
		&& target_len == source_len && op_strncmp(target, source, target_len) == 0)
	{
		return TRUE;
	}

	// only admins loading in top document is allowed otherwise
	if (!is_top_document && !source_is_admin)
		return FALSE;

	// Allow root to access any service
	if (source_len == 0)
		return TRUE;

	// only admins loading in top document is allowed otherwise
	if (!is_top_document || !source_is_admin)
		return FALSE;

	// Allow any service to access root default page
	if (target_len == 0)
		return TRUE;

	return FALSE;
}
#endif // WEBSERVER_SUPPORT
