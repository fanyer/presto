/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef URLFILTERAPI_H
#define URLFILTERAPI_H

#ifdef URL_FILTER

#include "modules/windowcommander/OpUrlFilterApi.h"
#include "modules/content_filter/content_filter.h"

class UrlFilterApi : public OpUrlFilterApi
{
public:
	void SetURLFilterListener(URLFilterListener* listener);
#ifdef GOGI_URL_FILTER
	void SetAsyncURLFilterListener(AsyncURLFilterListener* listener);
#endif // GOGI_URL_FILTER
};

#endif // URL_FILTER

#endif // URLFILTERAPI_H
