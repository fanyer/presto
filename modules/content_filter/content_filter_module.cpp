/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef URL_FILTER
#include "modules/content_filter/content_filter.h"

ContentFilterModule::ContentFilterModule()
:	url_filter(NULL)
{
}

void
ContentFilterModule::InitL(const OperaInitInfo& info)
{
	url_filter = OP_NEW_L(URLFilter, ());
	if(url_filter)
	{
		url_filter->InitL();
	}
}

void
ContentFilterModule::Destroy()
{
	OP_DELETE(url_filter);
	url_filter = NULL;
}

#endif // URL_FILTER

