/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#ifdef URL_FILTER

#include "modules/windowcommander/src/UrlFilterApi.h"

void UrlFilterApi::SetURLFilterListener(URLFilterListener* listener)
{
	g_urlfilter->SetListener(listener);
}
#ifdef GOGI_URL_FILTER
void UrlFilterApi::SetAsyncURLFilterListener(AsyncURLFilterListener* listener)
{
	g_urlfilter->SetAsyncListener(listener);
}
#endif // GOGI_URL_FILTER
#endif // URL_FILTER

