/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_CONTENT_FILTER_MODULE_H
#define MODULES_CONTENT_FILTER_MODULE_H

#ifdef URL_FILTER
class URLFilter;

class ContentFilterModule  : public OperaModule
{
public:
	ContentFilterModule();
	virtual ~ContentFilterModule(){};

	void InitL(const OperaInitInfo& info);
	void Destroy();

	URLFilter* url_filter;
};

#define g_urlfilter g_opera->content_filter_module.url_filter

#define CONTENT_FILTER_MODULE_REQUIRED

#endif // URL_FILTER

#endif // !MODULES_CONTENT_FILTER_MODULE_H
