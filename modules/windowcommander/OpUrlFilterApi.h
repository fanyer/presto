/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OPURLFILTERAPI_H
#define OPURLFILTERAPI_H

#ifdef URL_FILTER

class URLFilterListener;
class AsyncURLFilterListener;

/**
 * Interface for setting the URL filter listeners
 */
class OpUrlFilterApi
{
public:
	virtual ~OpUrlFilterApi() {}

	/** Set listener for the URL filter*/
	virtual void SetURLFilterListener(URLFilterListener* listener) = 0;
#ifdef GOGI_URL_FILTER
	/** Set listener for the asynchronous URL filter*/
	virtual void SetAsyncURLFilterListener(AsyncURLFilterListener* listener) = 0;
#endif // GOGI_URL_FILTER
};
#endif // URL_FILTER

#endif // OPURLFILTERAPI_H
