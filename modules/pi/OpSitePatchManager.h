/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPSITEPATCHMANAGER_H
#define OPSITEPATCHMANAGER_H

#ifdef PI_SITEPATCH_MANAGER

/**
 * API for getting the sitepatch URLs (spoof, browserjs, and update)
 * at runtime.
 *
 * If sitepatching is enabled to overcome issues on badly written or
 * non-conformant websites and automatic updates are enabled for such a
 * feature (see FEATURE_BROWSER_JAVASCRIPT and TWEAK_GOGI_SITEPATCH_UPDATE),
 * the browser will query a predefined server for information on keeping
 * sitepatching information current.  Further information on sitepatching
 * can be found (at the time of writing) at
 * http://www.opera.com/docs/browserjs/ .
 *
 * The URLs requested from said server are usually hardcoded and set at
 * compile time via tweaks (see
 * TWEAK_GOGI_SITEPATCH_{UPDATE,BROWSERJS,SPOOF}_URL).  However, in very
 * specific cases a finer-grained level of control over those three URLs
 * is needed.  This API caters for that need, by allowing code to query for
 * those URLs at runtime rather than relying upon hardcoded strings.
 *
 * This API is not meant for general usage, as relying on the hardcoded
 * URLs caters for the vast majority of use cases.
 *
 * Given that concrete implementations of this class are allowed to exist
 * if and only if both API_PI_SITEPATCH_MANAGER and
 * TWEAK_GOGI_SITEPATCH_UPDATE are imported/enabled, and given that
 * TWEAK_GOGI_SITEPATCH_UPDATE requires for all the three URLs involved
 * (TWEAK_GOGI_SITEPATCH_{UPDATE,BROWSERJS,SPOOF}_URL) to not be NULL,
 * the concrete implementations for the methods in this interface CAN NOT
 * return NULL.
 */
class OpSitePatchManager
{
public:

	/**
	 * Creates the OpSitePatchManager object.
	 *
	 * Classes implementing the OpSitePatchManager interface are
	 * supposed to compute the strings to return in each of the
	 * methods to implement when the object gets created.  The
	 * returned URLs are not meant to change for the entire lifetime
	 * of the object itself.
	 *
	 * For that very reason, all methods in the interface return a
	 * const uni_char* rather than an OP_STATUS value or leave.
	 * OOM is not supposed to occur except at object creation time,
	 * and Core already handles that case.
	 *
	 * @param[in,out] manager the address to the pointer that will
	 *                        contain the newly created
	 *                        OpSitePatchManager object.
	 * @return OpStatus::OK if the operation completed successfully,
	 *         OpStatus::ERR_NO_MEMORY if OOM is triggered,
	 *         or another appropriate OpStatus enumeration item.
	 */
	static OP_STATUS Create(OpSitePatchManager** manager);

	/**
	 * Returns the URL from which to retrieve override_downloaded.ini
	 * files to masquerade Opera as another browser for certain
	 * websites.
	 *
	 * Concrete implementations of this method CAN NOT return NULL
	 * as the URL from which to retrieve override_downloaded.ini.
	 *
	 * @return the URL for downloading override_downloaded.ini
	 *         files, as a string.
	 */
	virtual const uni_char* SpoofURL() const = 0;

	/**
	 * Returns the URL from which to retrieve browser.js files
	 * to fix specific website bugs/incompatibilities.
	 *
	 * Concrete implementations of this method CAN NOT return NULL
	 * as the URL from which to retrieve browser.js.
	 *
	 * @return the URL for downloading browser.js files, as a
	 *         string.
	 */
	virtual const uni_char* BrowserJSURL() const = 0;

	/**
	 * Returns the URL to query for checking whether updates to
	 * override_downloaded.ini and browser.js are available.
	 *
	 * Concrete implementations of this method CAN NOT return NULL
	 * as the URL from which to retrieve update information for
	 * sitepatching files.
	 *
	 * @return the URL for update queries, as a string.
	 */
	virtual const uni_char* UpdateURL() const = 0;

	/**
	 * Destructor.
	 */
	virtual ~OpSitePatchManager()
	{
	}
};

#endif // PI_SITEPATCH_MANAGER

#endif // OPSITEPATCHMANAGER_H
