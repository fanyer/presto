/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef GADGETURLHANDLER_H
#define	GADGETURLHANDLER_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "modules/content_filter/content_filter.h"

/**
 * A gadget-specific URL handler.  In general, gadgets require special handling
 * of URLs.
 *
 * Regular widgets may contain links and things are straightforward:  Upon
 * clicking, a link should load in a regular browser window.
 *
 * For "Save As Widget" widgets, we distinguish two kinds of links that we dub
 * internal and external.  Internal links are links used for navigation within
 * a web application.  External links link a web application to resources
 * external to that application.  Naturally, internal links should load within
 * the widget itself, while external links should load in separate, regular
 * browser windows.
 *
 * We use URLFilter from the @c content_filter module as our link classifier.
 * GadgetUrlHandler contains a private instance of URLFilter.  This instance is
 * completely separate and independent from the instance initialized within the
 * @c content_filter module, which is used for Content Blocking.  Our URLFilter
 * is initialized with its own INI file, whose contents are assumed to define a
 * filter suitable for a given web application.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetUrlHandler
{
public:
	/**
	 * Initializes the handler.
	 *
	 * @return status
	 */
	OP_STATUS Init();

	/**
	 * Determines if a URL is "external" to the gadget.  For webapps, this
	 * means that, when clicked, the URL should be opened in a regular browser
	 * window.
	 *
	 * @param url the URL to check
	 * @return @c TRUE iff @a url is external to the gadget
	 */
	BOOL IsExternalUrl(const uni_char* url);

private:
	URLFilter url_filter;
};


#endif  // WIDGET_RUNTIME_SUPPORT

#endif	// GADGETURLHANDLER_H
