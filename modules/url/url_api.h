/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2004-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _URL_API_H_
#define _URL_API_H_

#ifndef _URL_2_H_
#include "modules/url/url2.h"
#endif

class ServerName;
class Cookie_Item_Handler;
class Cookie;
class OperaURL_Generator;
class URL_DynamicUIntAttributeHandler;
class URL_DynamicStringAttributeHandler;
class URL_DynamicURLAttributeHandler;

#include "modules/url/url_id.h"

#ifdef URL_NETWORK_DATA_COUNTERS
struct NetworkCounters
{
	/* Total bytes transferred through network sockets. */
	UINT64 sent;
	UINT64 received;

#ifdef WEB_TURBO_MODE
	/* Number of bytes saved by Opera Turbo. */
	INT64 turbo_savings;
#endif

	void Reset()
	{
		sent = received = 0;
#ifdef WEB_TURBO_MODE
		turbo_savings = 0;
#endif
	}
};
#endif

/** 
 *	Global API to the URL module, works in association with the URL class
 *	
 *  To create use URL_API::SetupAPIL(),
 *  To shutdown, use URL_API::ShutdownAPI();
 *  Both are static functions
 **/
class URL_API 
{
	friend class UrlModule;
#ifdef SELFTEST
	friend class CacheTester;  // Just used for testing that double loading is not happening
#endif
private:
	/** Constructor */
	URL_API();
	/** Set up the object */
	void ConstructL();

public:
	/** Destructor */
	~URL_API();

#ifdef HISTORY_SUPPORT
	/** Can this URL be stored in the global history? */
	BOOL GlobalHistoryPermitted(URL &url);
#endif

#ifdef URL_SESSION_FILE_PERMIT
	/** Can this URL be stored in a session file? Activate with API_URL_SESSION_FILE_PERMISSION */
	BOOL SessionFilePermitted(URL &url);
#endif

	/** Can this URL be loaded and displayed? */
	BOOL LoadAndDisplayPermitted(URL &url);

#ifdef _MIME_SUPPORT_
	/** Can this URL be loaded and displayed in a mail view? */
	BOOL EmailUrlSuppressed(URLType url);
#endif

#if defined ERROR_PAGE_SUPPORT || defined URL_OFFLINE_LOADABLE_CHECK
	/** Can this URL be loaded and displayed while in offline status? */
	BOOL OfflineLoadable(URLType url_type);
#endif

#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	/** Checks if a particular URL is in an offline environment */
	BOOL IsOffline(URL &url);
#endif

	/** Get the pointer to a ServerName object
	 *
	 *	@param	name	The name of the ServerName object requested
	 *	@param	create	If TRUE create the ServerName object if it does not already exist
	 *
	 *	@return ServerName *	A pointer to the ServerName object, may be NULL if create 
	 *							is FALSE, or an error occured. DO NOT delete this object
	 */
	ServerName*		GetServerName(const OpStringC8 &name, BOOL create = FALSE);

	/** Get the pointer to a ServerName object
	 *
	 *	@param	name	The name of the ServerName object requested
	 *	@param	create	If TRUE create the ServerName object if it does not already exist
	 *
	 *	@return ServerName *	A pointer to the ServerName object, may be NULL if create 
	 *							is FALSE, or an error occured. DO NOT delete this object
	 */
	ServerName*		GetServerName(const OpStringC &name, BOOL create = FALSE);

	/** Get the pointer to a ServerName object, and return the port number(if any) in the  string
	 *
	 *	@param	name	The name of the ServerName object requested
	 *	@param	port	Reference to where the portnumber part of name is to be stored. If zero, no portnumber was present (or it was zero :)
	 *	@param	create	If TRUE create the ServerName object if it does not already exist
	 *
	 *	@return ServerName *	A pointer to the ServerName object, may be NULL if create 
	 *							is FALSE, or an error occured. DO NOT delete this object
	 */
	//ServerName*		GetServerName(const char *name, unsigned short &port, BOOL create = FALSE);

	//ServerName*		GetServerNameNoLocal(char *name,BOOL create = FALSE);

	/** Get the first available servername in the list of servername
	 *	Use GetNextServerName() to get the next ones.
	 *	The list IS NOT sorted
	 */
	ServerName*		GetFirstServerName();

	/** Get the next available servername in the list of servername
	 *	Use GetFirstServerName() to get the first item.
	 *	The list IS NOT sorted
	 */
	ServerName*		GetNextServerName();

	/** Get a URL for the given urlname and fragment
	 *
	 *	!!NOTE!!NOTE!!NOTE!! ***NEVER*** use an absolute URL string in the expectation of 
	 *	having this function return a reference to a specific document or resource, because the 
	 *	name string was extracted from another reference to that resource. URL string names are 
	 *	one-to-many, multiple resources can have the same string representation.
	 *	See https://wiki.oslo.opera.com/developerwiki/URL_Handover for more information 
	 *
	 *	@param	url		The textstring name of the URL, must be an aboslute URL. May include a fragment identifier
	 *	@param	URL		The URL object referencing the identified document. May be an empty 
	 *					URL if there was a problem.
	 */
	URL				GetURL(const OpStringC &url, URL_CONTEXT_ID context_id=0);
	URL				GetURL(const uni_char* url, URL_CONTEXT_ID context_id=0);

	/** Get a URL for the given urlname and fragment, The name may be an absolute URL, or a 
	 *	relative URL resolved relative to the parent URL
	 *
	 *	!!NOTE!!NOTE!!NOTE!! ***NEVER*** use an absolute URL string in the expectation of 
	 *	having this function return a reference to a specific document or resource, because the 
	 *	name string was extracted from another reference to that resource. URL string names are 
	 *	one-to-many, multiple resources can have the same string representation.
	 *	See https://wiki.oslo.opera.com/developerwiki/URL_Handover for more information 
	 *
	 *	@param	prnt_url	The URL used to resolve a relative url parameter
	 *	@param	url		The textstring name of the URL, properly formatted, may be a relative URL.
	 *	@param	rel		The fragment identifier (after the "#" in the full name of the URL)
	 *	@param	unique	Create a new URL object that may match the name of other URLs, but 
	 *					represents a different document. E.g a POST form query.
	 *					These objects can only be accessed through the URL object and copies, 
	 *					they cannot be accessed through this family of calls.
	 *	@param	URL		The URL object referencing the identified document. May be an empty 
	 *					URL if there was a problem.
	 */
	URL				GetURL(const URL& prnt_url, const OpStringC8 &url, const OpStringC8 &rel, BOOL unique = FALSE);
	URL				GetURL(const URL& prnt_url, const char* url, const char* rel, BOOL unique = FALSE);

	/** Get a URL for the given urlname and fragment
	 *
	 *	!!NOTE!!NOTE!!NOTE!! ***NEVER*** use an absolute URL string in the expectation of 
	 *	having this function return a reference to a specific document or resource, because the 
	 *	name string was extracted from another reference to that resource. URL string names are 
	 *	one-to-many, multiple resources can have the same string representation.
	 *	See https://wiki.oslo.opera.com/developerwiki/URL_Handover for more information 
	 *
	 *	@param	url		The textstring name of the URL, must be an aboslute URL
	 *	@param	rel		The fragment identifier (after the "#" in the full name of the URL)
	 *	@param	unique	Create a new URL object that may match the name of other URLs, but 
	 *					represents a different document. E.g a POST form query.
	 *					These objects can only be accessed through the URL object and copies, 
	 *					they cannot be accessed through this family of calls.
	 *	@param	URL		The URL object referencing the identified document. May be an empty 
	 *					URL if there was a problem.
	 */
	URL				GetURL(const OpStringC8 &url, const OpStringC8 &rel, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0);
	URL				GetURL(const char* url, const char* rel, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0);

	/** Get a URL for the given urlname and fragment
	 *
	 *	!!NOTE!!NOTE!!NOTE!! ***NEVER*** use an absolute URL string in the expectation of 
	 *	having this function return a reference to a specific document or resource, because the 
	 *	name string was extracted from another reference to that resource. URL string names are 
	 *	one-to-many, multiple resources can have the same string representation.
	 *	See https://wiki.oslo.opera.com/developerwiki/URL_Handover for more information 
	 *
	 *	@param	url		The textstring name of the URL, must be an aboslute URL
	 *	@param	rel		The fragment identifier (after the "#" in the full name of the URL)
	 *	@param	unique	Create a new URL object that may match the name of other URLs, but 
	 *					represents a different document. E.g a POST form query.
	 *					These objects can only be accessed through the URL object and copies, 
	 *					they cannot be accessed through this family of calls.
	 *	@param	URL		The URL object referencing the identified document. May be an empty 
	 *					URL if there was a problem.
	 */
	URL				GetURL(const OpStringC &url, const OpStringC &rel, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0);
	URL				GetURL(const uni_char* url, const uni_char* rel, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0);


	/** Get a URL for the given urlname and fragment, The name may be an absolute URL, or a 
	 *	relative URL resolved relative to the parent URL
	 *
	 *	!!NOTE!!NOTE!!NOTE!! ***NEVER*** use an absolute URL string in the expectation of 
	 *	having this function return a reference to a specific document or resource, because the 
	 *	name string was extracted from another reference to that resource. URL string names are 
	 *	one-to-many, multiple resources can have the same string representation.
	 *	See https://wiki.oslo.opera.com/developerwiki/URL_Handover for more information 
	 *
	 *	@param	prnt_url	The URL used to resolve a relative url parameter
	 *	@param	url		The textstring name of the URL, properly formatted, May be a relative 
	 *					URL, and may include a fragment identifier
	 *	@param	unique	Create a new URL object that may match the name of other URLs, but 
	 *					represents a different document. E.g a POST form query.
	 *					These objects can only be accessed through the URL object and copies, 
	 *					they cannot be accessed through this family of calls.
	 *	@param	URL		The URL object referencing the identified document. May be an empty 
	 *					URL if there was a problem.
	 */

	URL				GetURL(const URL& prnt_url, const OpStringC &url, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0);
	URL				GetURL(const URL& prnt_url, const uni_char* url, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0);

	/** Get a URL for the given urlname and fragment, The name may be an absolute URL, or a 
	 *	relative URL resolved relative to the parent URL
	 *
	 *	!!NOTE!!NOTE!!NOTE!! ***NEVER*** use an absolute URL string in the expectation of 
	 *	having this function return a reference to a specific document or resource, because the 
	 *	name string was extracted from another reference to that resource. URL string names are 
	 *	one-to-many, multiple resources can have the same string representation.
	 *	See https://wiki.oslo.opera.com/developerwiki/URL_Handover for more information 
	 *
	 *	@param	prnt_url	The URL used to resolve a relative url parameter
	 *	@param	url		The textstring name of the URL, properly formatted, may be a relative URL.
	 *	@param	rel		The fragment identifier (after the "#" in the full name of the URL)
	 *	@param	unique	Create a new URL object that may match the name of other URLs, but 
	 *					represents a different document. E.g a POST form query.
	 *					These objects can only be accessed through the URL object and copies, 
	 *					they cannot be accessed through this family of calls.
	 *	@param	URL		The URL object referencing the identified document. May be an empty 
	 *					URL if there was a problem.
	 */
	URL				GetURL(const URL& prnt_url, const OpStringC &url, const OpStringC &rel, BOOL unique = FALSE);
	URL				GetURL(const URL& prnt_url, const uni_char* url, const uni_char* rel, BOOL unique = FALSE);

	/** Get a URL for the given urlname and fragment
	 *
	 *	!!NOTE!!NOTE!!NOTE!! ***NEVER*** use an absolute URL string in the expectation of 
	 *	having this function return a reference to a specific document or resource, because the 
	 *	name string was extracted from another reference to that resource. URL string names are 
	 *	one-to-many, multiple resources can have the same string representation.
	 *	See https://wiki.oslo.opera.com/developerwiki/URL_Handover for more information 
	 *
	 *	@param	url		The textstring name of the URL, must be an aboslute URL. May include a fragment identifier
	 *	@param	URL		The URL object referencing the identified document. May be an empty 
	 *					URL if there was a problem.
	 */
	URL				GetURL(const OpStringC8 &url, URL_CONTEXT_ID context_id=0);
	URL				GetURL(const char* url, URL_CONTEXT_ID context_id=0);

	/** Get a URL for the given urlname and fragment, The name may be an absolute URL, or a 
	 *	relative URL resolved relative to the parent URL
	 *
	 *	!!NOTE!!NOTE!!NOTE!! ***NEVER*** use an absolute URL string in the expectation of 
	 *	having this function return a reference to a specific document or resource, because the 
	 *	name string was extracted from another reference to that resource. URL string names are 
	 *	one-to-many, multiple resources can have the same string representation.
	 *	See https://wiki.oslo.opera.com/developerwiki/URL_Handover for more information 
	 *
	 *	@param	prnt_url	The URL used to resolve a relative url parameter
	 *	@param	url		The textstring name of the URL, properly formatted, May be a relative 
	 *					URL, and may include a fragment identifier
	 *	@param	unique	Create a new URL object that may match the name of other URLs, but 
	 *					represents a different document. E.g a POST form query.
	 *					These objects can only be accessed through the URL object and copies, 
	 *					they cannot be accessed through this family of calls.
	 *	@param	URL		The URL object referencing the identified document. May be an empty 
	 *					URL if there was a problem.
	 */
	URL				GetURL(const URL& prnt_url, const OpStringC8 &url, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0);
	URL				GetURL(const URL& prnt_url, const char* url, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0);

	/** Make the URL identified by the url parameter unique */
	void			MakeUnique(URL &url);

	/** Make the a copy of the URL identified by the url parameter, but make it unique */
	URL				MakeUniqueCopy(const URL &url);


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
	BOOL ResolveUrlNameL(const OpStringC& name_in, OpString& resolved_out, BOOL entered_by_user = FALSE);

	/** Get the cookie string for the given URL
	 *
	 *	@param	url		URL for which the cookies are to be selected
	 *	@param	version	The lowest version number for the selected cookies 
	 *	@param	max_version	The highest version number for the selected cookies
	 *	@param	already_have_password	We already know this URL is login form 
	 *					password protocted, tag all selected cookies as being 
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
	 *
	 *	@return const char *	A pointer to the buffer containing the selected cookies. 
	 *					This buffer is common, and the string MUST be copied immediately
	 */
	const char*		GetCookiesL(URL &url,
							int& version,
							int &max_version,
							BOOL already_have_password,
							BOOL already_have_authentication,
							BOOL &have_password,
							BOOL &have_authentication);

	/** Handle a single Set_Cookie header for the given URL
	 *	This can be used to set cookies from HTML Meta tags and Javascripts
	 *
	 *	@param	url		URL from which the cookie is being set.
	 *	@param	cookiearg	Single cookie. Syntax is decided by the version number
	 *	@param	version		The cookie version. If 0 (zero) the cookie is a Netscape cookie, 
	 *					if non-zero it is an RFC 2965 cookie.
	 */
	void			HandleSingleCookieL(URL &url, const char *cookiearg,
							   const char *request, int version,
							   Window *win = NULL
							   );

#ifdef _ASK_COOKIE
#ifdef ENABLE_COOKIE_CREATE_DOMAIN
	/** Create a cookie domain for the given domain name */
	void			CreateCookieDomain(const char *domain_name);

	/** Create a cookie domain for the given ServerName */
	void			CreateCookieDomain(ServerName *domain_sn);
#endif

#ifdef URL_ENABLE_EXT_COOKIE_ITEM
	/** Set the cookie contained in the item handler */
	void			SetCookie(Cookie_Item_Handler *cookie_item);

	/** Remove duplicate cookies from the queue, MNust match on name, domain, 
	 *	path, version, secure flag, and port attributes */
	void			RemoveSameCookiesFromQueue(Cookie_Item_Handler *set_item);
#endif
#endif

#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
	/** Are cookies being handled that may affect the given url? */
	int				HandlingCookieForURL(URL &url);
#endif // _ASK_COOKIE

#ifdef QUICK_COOKIE_EDITOR_SUPPORT
	/** Build the cookie editor list */
	void			BuildCookieEditorListL();
#endif

	/** Have the cookie file been read? If not load it */
	void			CheckCookiesReadL();

#ifdef URL_API_CLEAR_COOKIE_API
	/** Clear cookies, if requested delete filters too */
	void	ClearCookiesL(BOOL delete_filters= FALSE);
#endif

#ifdef NEED_URL_COOKIE_ARRAY
	/** Build a Cookie list for the given URL
	 *
	 *	@param	cookieArray	Where to put the pointer to the allcoated array of pointers to the cookies. 
	 *						Caller takes over this array
	 *	@param	size_returned	Where to put the number of items in the array
	 *	@param	url			For which URL should the cookies be picked?
	 *	@return OP_STATUS
	 */
	OP_STATUS       BuildCookieList(
                               Cookie ** cookieArray,
                               int * size_returned,
                               URL &url);

	/** Build a Cookie list for the given domain and path
	 *
	 *	@param	cookieArray	Where to put the pointer to the allcoated array of pointers to the cookies. 
	 *						Caller takes over this array
	 *	@param	size_returned	Where to put the number of items in the array
	 *	@param	domain			For which server/domain should the cookies be picked?
	 *	@param	path			For which path within the domain should the selection be done
	 *	@match_subpaths			Only select cookies with matching path or subpath set in path
	 *	@return OP_STATUS
	 */
	OP_STATUS       BuildCookieList(
                               Cookie ** cookieArray,
                               int * size_returned,
                               uni_char * domain,
                               uni_char * path,
                               BOOL match_subpaths = FALSE);

	/**	Remove either a named cookie or all cookies for the given domain and path
	 *	
	 *	@param	domainstr	The domain for which cookies will be deleted
	 *	@param	pathstr		The path for which cookies will be deleted
	 *	@param	namest		(Optionally) the name of the cookie to be deleted
	 *	@return OP_STATUS
	 */
	OP_STATUS       RemoveCookieList(uni_char * domainstr, uni_char * pathstr, uni_char * namestr);
#endif // NEED_URL_COOKIE_ARRAY

	/** Have we started loading something in this session? */
	void			HasStartedLoading();

#if defined(_ENABLE_AUTHENTICATE) && defined(UNUSED_CODE)
	/** The handling of the current authentication element has ended. */
	void			AuthenticationDialogFinished();
#endif

	/** Forget all accepted certificates and other certificate related data*/
	void			ForgetCertificates();

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
	void			PurgeData(BOOL visited_links, BOOL disk_cache, BOOL sensitive, BOOL session_cookies, BOOL cookies, BOOL certificates, BOOL memory_cache);

#ifdef URL_ENABLE_SAVE_DATA
	/** Save the selected data
	 *
	 *	Action taken if the flag is TRUE
	 *
	 *	@param	visited_links		Save visisted links
	 *	@param	disk_cache			Svae the disk cache index
	 *	@param	cookies				Save persistene cookies
	 */
	void			SaveDataL(BOOL visited_links, BOOL disk_cache,  BOOL cookies);
#endif

	/** Check timeouts (such as deleting information that has expired */
	void			CheckTimeOuts();

	/** Stop all loading documents, close all connections. If specified, ignore transferwindow downloads */
	void			CleanUp(BOOL ignore_downloads = FALSE);

#ifdef NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS
	/** Stop all loading documens, then restart all of them again
	 *	Used to change network/proxy settings on the fly
	 */
	void			RestartAllConnections();

	/** Stop all loading documents */
	void			StopAllLoadingURLs();
#endif // NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS

	/** Close all open idle connections, or tell the active ones to shut down as 
	 *	soon as they are finished doing what they are doing 
	 */
	void			CloseAllConnections();

	/** Check if a network connection is available. Returns FALSE if in offline mode.
	 */
#ifdef PI_NETWORK_INTERFACE_MANAGER
	BOOL			IsNetworkAvailable();
#endif

#ifdef NEED_CACHE_EMPTY_INFO
	/**	Is the cache empty? */
	BOOL			IsCacheEmpty();
#endif

	/** Create a numbered opera:1234 URL*/
	URL				GetNewOperaURL();

	/** Block immediate loads of requeued requests when stopping requests, or restart connections after a block */
	void			SetPauseStartLoading(BOOL val);


#ifdef ERROR_PAGE_SUPPORT
	/** Write an error page tailored for the error code into the given URL, provided it does not already contain content.
	 *
	 *	@param	url		URL where the error page must be written
	 *	@param	errorcode	error code (usually a language code)
	 *	@param	ref_url		The referrer URL of the loaded page
	 *	@param	show_search_field If TRUE then we'll give a search field with a suggested search
	 *		to allow the user to find what couldn't be found in the first attempt. This is suitable
	 *		to use if the user was responsible for the broken input. It's not suitable if someone
	 *		else was responsible for it since that would allow an evil site to inject data into an
	 *		opera generated page.
	 *	@param	user_text	The exact text, the user typed in the address bar, if any 
	 */
	OP_STATUS		GenerateErrorPage(URL& url, long errorcode, URL *ref_url, BOOL show_search_field, const OpStringC &user_text = OpStringC());
#endif //ERROR_PAGE_SUPPORT

	/** Register a generator for a Opera: URL 
	 *	
	 *	- The registered URLs are matched in the sequence they are registered
	 *	- The caller retains ownership of the generator object, and must delete it when approriate
	 *	- The generator will unregister automatically when destroyed
	 *	- The generator can be unregistered by performing Out() or delete on the object.
	 *
	 *	@param	generator	Generator implementation.
	 */
	void			RegisterOperaURL(OperaURL_Generator *generator);

	/** Register a dynamic unsigned integer attribute handler for URLs.
	 *	Returns the assigned attribute enum
	 *	LEAVEs in case of errors
	 *	For use : #include "modules/url/url_dynattr.h"
	 */
	URL::URL_Uint32Attribute RegisterAttributeL(URL_DynamicUIntAttributeHandler *handler);

	/** Register a dynamic unsigned integer attribute handler for URLs.
	 *	Returns the assigned attribute enum
	 *	LEAVEs in case of errors
	 *	For use : #include "modules/url/url_dynattr.h"
	 */
	URL::URL_StringAttribute RegisterAttributeL(URL_DynamicStringAttributeHandler *handler);
	
	/** Register a dynamic unsigned integer attribute handler for URLs.
	 *	Returns the assigned attribute enum
	 *	LEAVEs in case of errors
	 *	For use : #include "modules/url/url_dynattr.h"
	 */
	URL::URL_URLAttribute RegisterAttributeL(URL_DynamicURLAttributeHandler *handler);

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
	URLType RegisterUrlScheme(const OpStringC &scheme_name, BOOL have_servername=FALSE, URLType real_type = URL_NULL_TYPE, BOOL *actual_servername_flag=NULL);
#endif
};

#ifndef URL_MODULE_REQUIRED
/** The global access point to the URL module API */
extern URL_API *g_url_api;
#endif

#endif  // _URL_API_H_
