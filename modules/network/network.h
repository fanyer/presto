/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2004-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef NETWORK_H
#define NETWORK_H

#include "modules/network/op_url.h"
#include "modules/network/op_server_name.h"

class ServerNameCallback;

/** 
 *	Global API to the Network module, works in association with the OpURL class
 **/
class Network
{
public:
	/** Set URL to visited. */
	static BOOL SetVisited(OpURL url, URL_CONTEXT_ID context_id);

	/** Has this URL been visited. */
	static BOOL IsVisited(OpURL url, URL_CONTEXT_ID context_id);

	/** Get the pointer to a ServerName object
	 *
	 *	@param	name	The name of the ServerName object requested
	 *	@param	callback	The callback used to notify of when the OpServerName object is ready.
	 *	@param	create	If TRUE create the ServerName object if it does not already exist
	 */
	static void GetServerName(const OpStringC8 &name, ServerNameCallback *callback, BOOL create = FALSE);

	/** Get the pointer to a ServerName object through an async callback.
	 *
	 *	@param	name		The name of the ServerName object requested
	 *	@param	callback	The callback used to notify of when the OpServerName object is ready.
	 *	@param	create		If TRUE create the ServerName object if it does not already exist
	 */
	static void GetServerName(const OpStringC &name, ServerNameCallback *callback, BOOL create = FALSE);

	/** Complete a partially completed URL. 
	 *	
	 *	"www.opera.com" -> "http://www.opera.com/"
	 *	"ftp.opera.com" -> "ftp://ftp.opera.com/
	 *	"c:\mypath\myfile" -> file://localhost/c:/mypath/myfile"
	 *
	 * @param name_in The URL string to complete.
	 * @param resolved_out (output) The resolved string.
	 * @param entered_by_user
	 *   Flag indicating that the URL was entered by the user. URLs entered
	 *   by the user may get special treatment for non-ASCII characters,
	 *   depending on the configuration for using UTF-8 in URLs.
	 * @return TRUE unless the output URL is empty.
	 */
	static BOOL ResolveUrlNameL(const OpStringC &name_in, OpString &resolved_out, BOOL entered_by_user = FALSE);

	/** Get the cookie string for the given OpURL
	 *
	 *	@param	url		URL for which the cookies are to be selected
	 *	@param	cookies	Returned cookies
	 *	@param	already_have_password	We already know this URL is login form 
	 *					password protected, tag all selected cookies as being
	 *					associated with a password protected area.
	 *	@param	already_have_authentication		We already know this URL is HTTP 
	 *					Authentication password protected, tag all 
	 *					selected cookies as being associated with 
	 *					a password protected area.
	 *	@param	have_password	returns TRUE if any of the selected cookies are 
	 *					associated with a login form password protected area
	 *					This may be used to tag the url parameter as password protected
	 *	@param	have_authentication		returns TRUE if any of the selected cookies are 
	 *					associated with a HTTP Authentication password protected area
	 *					This may be used to tag the url parameter as password protected
	 *  @return OK or ERR_NO_MEMORY
	 */
	static OP_STATUS GetCookiesL(OpURL url, OpString8 &cookies
#ifdef URL_DETECT_PASSWORD_COOKIES
							, BOOL already_have_password,
							BOOL already_have_authentication,
							BOOL &have_password,
							BOOL &have_authentication
#endif
		);

	/** Handle a single Set_Cookie header for the given OpURL
	 *	This can be used to set cookies from HTML Meta tags and Javascripts
	 *
	 *	@param	url		URL from which the cookie is being set.
	 *	@param	cookiearg	Single cookie.
	 */
	static void HandleSingleCookieL(OpURL url, const char *cookiearg, const char *request);

#ifdef _ASK_COOKIE
#ifdef ENABLE_COOKIE_CREATE_DOMAIN
	/** Create a cookie domain for the given domain name */
	static void CreateCookieDomain(const char *domain_name);
#endif

#ifdef URL_ENABLE_EXT_COOKIE_ITEM
	/** Set the cookie contained in the item handler */
	static void SetCookie(Cookie_Item_Handler *cookie_item);
#endif
#endif

#ifdef QUICK_COOKIE_EDITOR_SUPPORT
	/** Build the cookie editor list */
	static void			BuildCookieEditorListL();
#endif

	/** Prefetch address from DNS asynchronously. This will look up the URL host name in the DNS system and cache the corresponding IP address.
	 * If the request is sent at a later time this will speed up the response time. Helps response time for an eventual request if prefetching
	 * is done while typing in a url.
	 */
	static void PrefetchDNS(OpURL url);

	/** Purge the selected data
	 *
	 *	Action taken if the flag is TRUE
	 *
	 *	@param	visited_links		Clear all visisted links
	 *	@param	disk_cache			Clear the disk cache
	 *	@param	sensitive			Delete sensitive data, such as passwords and password protected documents
	 *	@param	session_cookies		Delete only the session cookies (those thatwould be deleted on exit)
	 *	@param	cookies				Delete all cookies
	 *	@param	certificates		(Not yet active) Delete certificates
	 *	@param	memory_cache		Clear the memory cache
	 */
	static void PurgeData(BOOL visited_links, BOOL disk_cache, BOOL sensitive, BOOL session_cookies, BOOL cookies, BOOL certificates, BOOL memory_cache);

#ifdef URL_ENABLE_SAVE_DATA
	/** Save the selected data
	 *
	 *	Action taken if the flag is TRUE
	 *
	 *	@param	visited_links		Save visisted links
	 *	@param	disk_cache			Svae the disk cache index
	 *	@param	cookies				Save persistene cookies
	 */
	static void SaveDataL(BOOL visited_links, BOOL disk_cache, BOOL cookies);
#endif

	/** Check timeouts (such as deleting information that has expired */
	static void CheckTimeOuts();

	/** Stop all loading documents, close all connections. If specified, ignore transferwindow downloads */
	static void CleanUp(BOOL ignore_downloads = FALSE);

#ifdef NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS
	/** Stop all loading documens, then restart all of them again
	 *	Used to change network/proxy settings on the fly
	 */
	static void RestartAllConnections();

	/** Stop all loading documents */
	static void StopAllLoadingURLs();
#endif // NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS

	/** Close all open idle connections, or tell the active ones to shut down as 
	 *	soon as they are finished doing what they are doing 
	 */
	static void CloseAllConnections();

	/** Check if a network connection is available. Returns FALSE if in offline mode.
	 */
#ifdef PI_NETWORK_INTERFACE_MANAGER
	static BOOL IsNetworkAvailable();
#endif

	/** Block immediate loads of requeued requests when stopping requests, or restart connections after a block */
	static void SetPauseStartLoading(BOOL val);

#ifdef URL_ENABLE_REGISTER_SCHEME
	/** Register a new URL scheme with the specific attribute If-And-Only-If the scheme does not
	 *	already exist. (existing schemes are not updated)
	 *	This call must be made *before* the first time a URL with the scheme is encountered, otherwise
	 *	a default scheme will be registered
	 *	Returns the URLType of the existing scheme, or of the new one if the scheme was successfully 
	 *	created, and URL_NULL_TYPE if creation failed.
	 *
	 *	These schemes will NOT be loadable
	 *
	 *	@param	scheme_name			Name of the scheme, must only contain alphanumerical and "-" characters
	 *	@param	have_servername		Does this scheme always have a hostname/authority component
	 *								This should only be set if there is a need for directly checking the hostname.
	 *	@param	real_type			What is the real type of this scheme? Note: Use with caution, this 
	 *								can affect caching and authentication. Default is the same as the returned scheme
	 *	@param	actual_servername_flag	Optional pointer to BOOL where the scheme's actual have_servername flag can be stored.
	 *
	 *	@return	URLType	of the new or exisiting scheme, URL_NULL_TYPE if creation failed and it did not exist
	 */
	static URLType RegisterUrlScheme(const OpStringC &scheme_name, BOOL have_servername = FALSE, URLType real_type = URL_NULL_TYPE, BOOL *actual_servername_flag = NULL);
#endif
};

#endif // NETWORK_H
