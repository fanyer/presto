/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPPLATFORMVIEWERS_H
#define OPPLATFORMVIEWERS_H

#ifdef USE_PLATFORM_VIEWERS

/** @short Provide actions for Core to be able to implement viewers betters
 */
class OpPlatformViewers
{
public:
	/** Create an OpPlatformViewers object. */
	static OP_STATUS Create(OpPlatformViewers** new_platformviewers);

	virtual ~OpPlatformViewers() {}

#ifdef USE_EXTERNAL_BROWSER
	/** Open a URL in the system default browser.
	 *
	 * Disable FEATURE_EXTERNAL_BROWSER if no such browser is available.
	 *
	 * Example use case: This is used by widgets when opening links with
	 * widget.openURL() to make it feel more like a native widget.
	 *
	 * @param url The name of the URL to open. The URL has passed relevant
	 *            security checks.
	 * @returns OpStatus::OK if handling was deemed satisfactory by the
	 *          platform. In case of an error, core might try to open the
	 *          URL in a new tab.
	 */
	virtual OP_STATUS OpenInDefaultBrowser(const uni_char* url) = 0;
#endif // USE_EXTERNAL_BROWSER
};

#endif // USE_PLATFORM_VIEWERS

#endif // OPPLATFORMVIEWERS_H
