/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SECURITY_UNITE_H
#define SECURITY_UNITE_H

#ifdef WEBSERVER_SUPPORT

class OpSecurityManager_Unite
{
public:

	OP_STATUS CheckUniteSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed);

protected:
	static BOOL AllowUniteURL(const URL& target, const URL& source, BOOL is_top_document=FALSE);
		/**< Unite URLs are URLs with administration rights for Unite service.
		 *   Hence opening of them is severly restricted based upon who is trying
		 *   to open it.
		 *
		 *   The specification for what is allowed to open unite URLS is specified
		 *   in the wiki at
		 *   https://wiki.oslo.opera.com/developerwiki/Alien/User_Administration
		 *
		 *   See also task CORE-19590
		 *
		 *   \param target   The URL to the unite service we're trying to open.  MUST be
		 *                   of type URL_UNITE
		 *   \param source   The URL this service is being opened from.  This will be
		 *                   the referer in DocumentManager::OpenURL.
		 */

	static BOOL AllowInterUniteConnection(const URL& target, const URL& source, BOOL is_top_document=FALSE);
		/**< Unite services are only allowed to access Unite URLs from the same service.
	     *
		 *   Exception if loding URL is top_document.  Then the root service is
		 *   allowed to access the default page of all Unite services, and all
		 *   Unite services are allowed to access the default page of the root URL.
		 *
		 *   \param target   The URL to the unite service we're trying to open.  MUST be
		 *                   of type URL_UNITE
		 *   \param source   The URL this service is being opened from.  This will be
		 *                   the referer in DocumentManager::OpenURL.
		 *   \param is_top_document  TRUE if the source URL is the top document trying
		 *                   to load the URL (i.e. target will be the new top document)
		 */
};

#endif // WEBSERVER_SUPPORT
#endif // SECURITY_UNITE_H
