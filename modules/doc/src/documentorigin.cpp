/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/doc/documentorigin.h"
#include "modules/url/url2.h"
#include "modules/util/opstring.h"
#include "modules/url/url_api.h"

/* static */ DocumentOrigin*
DocumentOrigin::Make(URL url)
{
	DocumentOrigin* o = OP_NEW(DocumentOrigin, (url));
	if (o)
	{
		OP_STATUS status = url.GetAttribute(URL::KUniHostName, o->effective_domain);
		if (OpStatus::IsSuccess(status) && !o->effective_domain.CStr())
			status = o->effective_domain.Set("");

		if (OpStatus::IsError(status))
		{
			OP_DELETE(o);
			o = NULL;
		}
	}
	return o;
}

/* static */ DocumentOrigin*
DocumentOrigin::MakeUniqueOrigin(URL_CONTEXT_ID context, BOOL in_sandbox)
{
	char url_string[100]; // ARRAY OK 2011-11-25 bratell
	op_snprintf(url_string, 100, "http://%d.%d.unique./",
		        g_opera->doc_module.unique_origin_counter_high,
		        g_opera->doc_module.unique_origin_counter_low);
	if (++g_opera->doc_module.unique_origin_counter_low == 0)
		++g_opera->doc_module.unique_origin_counter_high;
	URL url = g_url_api->GetURL(url_string, context);
	if (url.IsEmpty())
		return NULL;
	g_url_api->MakeUnique(url);
	/** If this is a "unique origin" as per HTML */
	RETURN_VALUE_IF_ERROR(url.SetAttribute(URL::KIsUniqueOrigin, TRUE), NULL);
	DocumentOrigin* origin = Make(url);
	if (origin)
		origin->in_sandbox = in_sandbox;
	return origin;
}
