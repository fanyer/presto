/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _URL_MANAGER_
#define _URL_MANAGER_

#define		UM_DCACHE_DIR_OK			0
#define		UM_DCACHE_DIR_CREATED		1
#define		UM_DCACHE_DIR_CANNOT_CREATE	2
#define		UM_DCACHE_DIR_USED			3
#define		UM_DCACHE_DIR_OOM			4

#include "modules/util/simset.h"

#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/url/prot_sel.h"

#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/hardcore/base/periodic_task.h"
#include "modules/util/opfile/opfile.h"
#include "modules/about/opillegalhostpage.h"

class URL;
class URL_API;
class URL_DataStorage;
class URLLinkHead;
class Sequence_Splitter;
struct URL_Name_Components_st;
typedef Sequence_Splitter ParameterList;
class Cookie_Item_Handler;
class CookieDomain;
class Cookie;
struct protocol_selentry;
class ExternalProtocolHandlers;
class Context_Manager;

#include "modules/cache/cache_man.h"
#include "modules/cookies/cookie_man.h"
#include "modules/auth/auth_man.h"
#include "modules/url/url_dynattr_man.h"

#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
#define ERR_COMM_RELOAD_DIRECT 34000
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE

#ifdef WEB_TURBO_MODE
#define MAX_REENABLE_TURBO_DELAY	1800000 // See SetWebTurboAvailable()
#define FIRST_REENALBE_TURBO_DELAY	60000
#endif // WEB_TURBO_MODE


#ifdef URL_NETWORK_LISTENER_SUPPORT
class URL_Manager_Listener: public Link
{
public:
	virtual void NetworkEvent(URLManagerEvent network_event, URL_ID id) = 0;
	virtual  ~URL_Manager_Listener(){if(InList())Out();}
};
#endif

class URL_Manager
	: public OpPrefsListener,
	  public URL_DynamicAttributeManager,
	  public Cache_Manager,
	  public Cookie_Manager
#ifdef _ENABLE_AUTHENTICATE
	  , public Authentication_Manager
#endif
	  , public MessageObject
{
private:
	friend class CacheTester;
	AutoDeleteHead unknown_url_schemes;
	URLType last_unknown_scheme;

private:

	ServerName_Store servername_store;
	time_t	last_freed_server_resources;

	/** List of URL_Scheme_Authority_Components for non-servername schemes */
	URL_Scheme_Authority_List	scheme_and_authority_list;

	char*			temp_buffer1; // escaped URL  // NOTE!!! Same location as uni_temp_buffer3 NOTE!!
	char*			temp_buffer2; // final editing / resolved url // NOTE!!! Same location as uni_temp_buffer2 NOTE!!
	//char*			temp_buffer3; // temp for split (relative) url;
	uni_char*		uni_temp_buffer1; // escaped URL
	uni_char*		uni_temp_buffer2; // final editing / resolved url // NOTE!!! Same location as temp_buffer2 NOTE!!
	uni_char*		uni_temp_buffer3; // temp for split (relative) url // NOTE!!! Same location as temp_buffer1 NOTE!!
	unsigned int	temp_buffer_len;
	BOOL			posted_temp_buffer_shrink_msg;
	OpStringS8		tld_whitelist; //list of tlds.

#ifdef _EMBEDDED_BYTEMOBILE
	OpString ebo_server;
	unsigned short ebo_port;
	OpString8 ebo_options;
	OpString8 ebo_bc_req;
	BOOL embedded_bm_disabled;
	BOOL embedded_bm_optimized;
	BOOL embedded_bm_compressed_server;
#endif //_EMBEDDED_BYTEMOBILE
#if defined(_EMBEDDED_BYTEMOBILE) || defined(SCOPE_HTTP_LOGGER) || defined(WEB_TURBO_MODE)
	int request_sequence_number;
#endif  // _EMBEDDED_BYTEMOBILE || SCOPE_HTTP_LOGGER || WEB_TURBO_MODE
#ifdef OPERA_PERFORMANCE
	int connection_sequence_number;
#endif  // OPERA_PERFORMANCE

#ifdef WEB_TURBO_MODE
	BOOL m_web_turbo_available;
	UINT32 m_reenable_turbo_delay;
	BOOL m_turbo_reenable_message_posted;
#endif // WEB_TURBO_MODE

	long		opera_url_number;

	BOOL		startloading_paused;

	class PeriodicCheckTimeouts : public PeriodicTask
	{
		virtual void Run ();
	} checkTimeouts;
	class PeriodicFreeUnusedResources : public PeriodicTask
	{
		virtual void Run ();
	} freeUnusedResources;
	class PeriodicAutoSaveCache : public PeriodicTask
	{
		virtual void Run ();
	} autoSaveCache;

#ifdef URL_DELAY_SAVE_TO_CACHE
	time_t message_last_handled[4];
#endif // URL_DELAY_SAVE_TO_CACHE
#ifdef URL_NETWORK_LISTENER_SUPPORT
	Head		listeners;
#endif

private:
	friend class ExternalProtocolHandlers;

	OP_STATUS		CheckTempbuffers(unsigned checksize);
	enum { TEMPBUFFER_SHRINK_LIMIT = ((URL_INITIAL_TEMPBUFFER_SIZE + 255) & ~255) };
	/** Makes the global shared buffers small if they are big. */
	void			ShrinkTempbuffers();
	URL				LocalGetURL(const URL* prnt_url, const OpStringC &url, const OpStringC &rel, BOOL unique, URL_CONTEXT_ID context_id=0);
	
#ifdef SELFTEST
public:
#endif
	friend class URL_API;
	const protocol_selentry* LookUpUrlScheme(const char* str, BOOL lowercase);
	const protocol_selentry *GetUrlScheme(const OpStringC& str, BOOL create=TRUE
#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
						, BOOL external = FALSE
#endif
#if defined URL_ENABLE_REGISTER_SCHEME || defined EXTERNAL_PROTOCOL_SCHEME_SUPPORT
						, BOOL have_servername=FALSE
#endif
#ifdef URL_ENABLE_REGISTER_SCHEME
						, URLType real_type = URL_NULL_TYPE
#endif
						);
	const protocol_selentry* LookUpUrlScheme(URLType type);

private:
	const protocol_selentry *CheckAbsURL(uni_char* this_name);

private:

	void			InternalInit();
	void			InternalDestruct();

public:

					URL_Manager();	
	void			InitL();
	virtual			~URL_Manager();

#ifdef URL_DELAY_SAVE_TO_CACHE
	BOOL			DelayCacheOperations(OpMessage msg);
#endif // URL_DELAY_SAVE_TO_CACHE
	void			HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void			CleanUp(BOOL ignore_downloads = FALSE);

	const OpStringC8 &GetTLDWhiteList();
	void			CloseAllConnections();

	BOOL			FreeUnusedResources(BOOL all=TRUE);

#ifdef _OPERA_DEBUG_DOC_
	void				GetMemUsed(DebugUrlMemory &); // URL/servername only;
#endif


	ServerName*		GetServerName(const char *name, BOOL create = FALSE, BOOL normalize=TRUE);
	ServerName*		GetServerName(OP_STATUS &op_err, const uni_char *name, unsigned short &port, BOOL create = FALSE, BOOL normalize=TRUE);
	ServerName*		GetFirstServerName(){return servername_store.GetFirstServerName();}
	ServerName*		GetNextServerName(){return servername_store.GetNextServerName();}

#ifdef URL_NETWORK_LISTENER_SUPPORT
	void NetworkEvent(URLManagerEvent network_event, URL_ID id);
	void RegisterListener(URL_Manager_Listener *listener);
	void UnregisterListener(URL_Manager_Listener *listener);
#endif

	URL				GetURL(const OpStringC8 &url, URL_CONTEXT_ID context_id=0);
	URL				GetURL(const URL& prnt_url, const OpStringC8 &url, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0);

#ifdef URL_ACTIVATE_GET_URL_BY_DATAFILE_RECORD
	URL				GetURL(DataFile_Record *rec, FileName_Store &filenames, BOOL unique=FALSE, URL_CONTEXT_ID context_id=0);
#endif

	OP_STATUS		GetResolvedNameRep(URL_Name_Components_st &resolved_name, URL_Rep* prnt_url, const uni_char* t_name);

	URL				GetURL(const OpStringC &url, URL_CONTEXT_ID context_id=0);
	URL				GetURL(const OpStringC8 &url, const OpStringC8 &rel, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0);


	URL				GetURL(const OpStringC &url, const OpStringC &rel, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0)
						{ return LocalGetURL(0, url, rel, unique, context_id); }

	URL				GetURL(const URL& prnt_url, const OpStringC8 &url, const OpStringC8 &rel, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0);
	URL				GetURL(const URL& prnt_url, const OpStringC &url, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0);

	URL				GetURL(const URL& prnt_url, const OpStringC &url, const OpStringC &rel, BOOL unique = FALSE, URL_CONTEXT_ID context_id=0)
						{ return LocalGetURL(&prnt_url, url, rel, unique, context_id); }

	URL_Scheme_Authority_Components *FindSchemeAndAuthority(OP_STATUS &op_err, URL_Name_Components_st *components, BOOL create=FALSE);

	const uni_char *GetProxy(ServerName *servername, URLType type) const;
	BOOL			UseProxy(URLType /* type */) const;
#ifdef _SSL_USE_SMARTCARD_
	void			CleanSmartCardAuthenticatedDocuments(ServerName *server);
	void			InvalidateSmartCardSessions(SSL_varvector24_list &);
#endif

	URL				GetNewOperaURL();

	/**
	 * Returns a URL pointing to (containing) an invalid host error page or an empty url in case of error.
	 *
	 * @param url - a string representing the url
	 * @param ctx_id - context id
	 * @param kind - illegal host page kind.
	 *
	 * @see OpIllegalHostPage::IllegalHostKind
	 */
	URL GenerateInvalidHostPageUrl(const uni_char* url, URL_CONTEXT_ID ctx_id
#ifdef ERROR_PAGE_SUPPORT
		, OpIllegalHostPage::IllegalHostKind kind
#endif // ERROR_PAGE_SUPPORT
		);

	OP_STATUS		WriteFiles();

	void			AutoSaveFilesL();

#ifdef _OPERA_DEBUG_DOC_
	OP_STATUS       GenerateDebugPage(URL& url);
#endif
#ifdef OPERA_PERFORMANCE
	OpProbeTimestamp &GetSessionStart() { return session_start; }
	void OnStartLoading();
	void OnLoadingFinished();
	void OnUserInitiatedLoad();
	void GeneratePerformancePage(URL& url);
	void EnablePerformancePage() { opera_performance_enabled = TRUE; }
	BOOL GetPerformancePageEnabled() { return opera_performance_enabled; }
	void AddToMessageLog(const char *message_type, OpProbeTimestamp &start_processing, double &processing_diff);
	void AddToMessageDelayLog(const char *message_type, const char *old_message_type, OpProbeTimestamp &start_processing, double &processing_diff);
	void ClearLogs();
	Head message_handler_log;
	Head message_delay_log;
	OpProbeTimestamp session_start;
	OpProbeTimestamp session_end;

	OpProbeTimestamp message_start;
	OpProbeTimestamp message_end;
	OpMessage previous_msg;
	double idletime;
	void MessageStarted(OpMessage msg);
	void MessageStopped(OpMessage msg);

	// Track user initiated document loads
	BOOL user_initiated;

	// Track if profiling through OnStartLoading has been called
	BOOL started_profile;

	// Keep OnLoadingFinished from triggering logic prematurely.
	BOOL loading_started_called;

	//Only use per page load profiling after opera:performance has been loaded once
	BOOL opera_performance_enabled;
#endif // OPERA_PERFORMANCE

	URLType			MapUrlType(const uni_char* str);
	URLType			MapUrlType(const char* str, BOOL lowercase = FALSE);
	const char*		MapUrlType(URLType type);

	BOOL			TooManyOpenConnections(ServerName *);
#ifdef NEED_URL_ACTIVE_CONNECTIONS
    BOOL            ActiveConnections();
#endif

	int				ConvertHTTPResponseToErrorCode(int response);

	void			CheckTimeOuts();

#ifdef _NATIVE_SSL_SUPPORT_
	void			ForgetCertificates();
	void			ClearSSLSessions();
#endif // _NATIVE_SSL_SUPPORT_
	void			RemoveSensitiveData();
	void			UpdateUAOverrides();

	void			SetPauseStartLoading(BOOL val);
	BOOL			GetPauseStartLoading() const{return startloading_paused;}

public: // OpPrefsListener interface
	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);
    virtual void PrefChanged(OpPrefsCollection::Collections id, int, const OpStringC &);

	/** Check if proxy should be used on the specified server.
	  *
	  * @param hostname Name of host to lookup.
	  * @param post Port on host.
	  * @return TRUE if a proxy should be used.
	  */
	static BOOL UseProxyOnServer(ServerName *hostname, unsigned short port);

	void PostShrinkTempBufferMessage();

#define URL_MAN_PERMISSION_FUNCTIONS
	BOOL GlobalHistoryPermitted(URL &url);
	BOOL SessionFilePermitted(URL &url);
	BOOL LoadAndDisplayPermitted(URL &url);
	BOOL EmailUrlSuppressed(URLType url_type);
	BOOL OfflineLoadable(URLType url_type);

	/** Checks if a particular URL is in an offline environment.
	 * Application cache is not included as it has it's own offline handling
	 */
	BOOL IsOffline(URL &url);

#if defined(_EMBEDDED_BYTEMOBILE) || defined(SCOPE_HTTP_LOGGER) || defined(WEB_TURBO_MODE)
	int GetNextRequestSeqNumber() { return ++request_sequence_number; }
#endif  // _EMBEDDED_BYTEMOBILE || SCOPE_HTTP_LOGGER || WEB_TURBO_MODE

#ifdef OPERA_PERFORMANCE
	int GetNextConnectionSeqNumber() { return ++connection_sequence_number; }
#endif  // OPERA_PERFORMANCE

#ifdef WEB_TURBO_MODE
	/** @short Gets Turbo Mode service availability
	 * 
	 * @return FALSE if Turbo Mode services are deemed to be unavailable.
	 */
	BOOL GetWebTurboAvailable() { return m_web_turbo_available; }

	/** @short Used to signal availability of Web Turbo servers
	 * 
	 * If status changes to not available a new attempt to enable usage of
	 * Turbo Mode will be made after FIRST_REENALBE_TURBO_DELAY milliseconds.
	 * If it is disabled again the delay before the next attempt is doubled.
	 * This continues until the delay reaches MAX_REENABLE_TURBO_DELAY.
	 * 
	 * @param is_available FALSE if the Turbo Mode servers are unavailable. Otherwise TRUE.
	 * @param is_attempt TRUE if call is an attempt to re-enable. Default FALSE.
	 */
	void SetWebTurboAvailable(BOOL is_available, BOOL reenable_attempt = FALSE);
#endif // WEB_TURBO_MODE

#ifdef _EMBEDDED_BYTEMOBILE
	BOOL GetEmbeddedBmOpt();
	void SetEmbeddedBmOpt(BOOL optimize) { embedded_bm_optimized = optimize; }
	BOOL GetEmbeddedBmDisabled()  { return embedded_bm_disabled; }
	void SetEmbeddedBmDisabled(BOOL disabled) { embedded_bm_disabled = disabled; }
	void SetEmbeddedBmCompressed(BOOL compressed) { embedded_bm_compressed_server = compressed; }
	BOOL GetEmbeddedBmCompressed() { return embedded_bm_compressed_server; }
	void SetEBOServer(const OpStringC8 &hostname, unsigned short port);
	OpString &GetEBOServer() { return ebo_server; };
	void SetEBOBCResp(const OpStringC8 &bc_req);
	void SetEBOOptions(const OpStringC8 &option);
#endif //_EMBEDDED_BYTEMOBILE

#ifdef _BYTEMOBILE
	BOOL byte_mobile_optimized;
	BOOL GetByteMobileOpt(){ return byte_mobile_optimized; }
	void SetByteMobileOpt(BOOL optimize){ byte_mobile_optimized = optimize;
	}
#endif

#ifdef NETWORK_STATISTICS_SUPPORT
	class Network_Statistics_Manager *network_statistics_manager;
public:
	class Network_Statistics_Manager *GetNetworkStatisticsManager() { return network_statistics_manager; }
#endif // NETWORK_STATISTICS_SUPPORT

public:
	/** Create a new multi cache/cookie context with the given ID
	 *
	 *	IDs MUST be unique within a given session. Use ContextExists() to check
	 *
	 *	Folder locations MUST be different from those used by any other context.
	 *
	 *	vlink_loc should be the same as cookie_loc, while cache_loc can be different
	 *
	 *	OPFILE_ABSOLUTE_FOLDER indicates a temporary context, and in such cases all 
	 *	the folder parameters should be the same.
	 *
	 *	If either cache_loc or vlink_loc is OPFILE_ABSOLUTE_FOLDER both cache and visited links are disabled
	 *
	 *	@param		id				ID of the context. This id must be used 
	 *	@param		root_loc		Folder location of the context's root folder, where all the profile data usually is.
	 *	@param		cookie_loc		Folder location of the cookies, should be the same as vlink_loc
	 *	@param		vlink_loc		Folder location of the visited link file, should be the same as cookie_loc
	 *	@param		cache_loc		Folder location of the disk cache, should be the same as cookie_loc
	 *  @param		parentURL		Sometimes we need to associate the context with some URL; in other cases empty URL can be passed
	 *	@param		share_cookies_with_main_context		TRUE if this context should use the same cookies as the main browser windows
	 *	@param		cache_size_pref Preference that contains the number of KB assigned to the cache; -1 means default
	 *
	 *	LEAVES in case of OOM and other errors,
	 */
	void			AddContextL(URL_CONTEXT_ID id,
		OpFileFolder root_loc, OpFileFolder cookie_loc, OpFileFolder vlink_loc, OpFileFolder cache_loc,
		URL &parentURL,
		BOOL share_cookies_with_main_context = FALSE,
		int cache_size_pref = -1
		);

	/**	Same as above but without parentURL parameter (for backward compatibility) */
	void			AddContextL(URL_CONTEXT_ID id,
		OpFileFolder root_loc, OpFileFolder cookie_loc, OpFileFolder vlink_loc, OpFileFolder cache_loc,
		BOOL share_cookies_with_main_context = FALSE,
		int cache_size_pref = -1
		);

	/** Create a new multi cache/cookie context with the given ID, supplying directly the Context Manager. This method is intended for "special cases".
		 *
		 *	IDs MUST be unique within a given session. Use ContextExists() to check
		 *
		 *	Folder locations MUST be different from those used by any other context.
		 *
		 *	vlink_loc (used to create the Context Manager) should be the same as cookie_loc, while cache_loc (used to create the Context Manager) can be different
		 *
		 *	OPFILE_ABSOLUTE_FOLDER indicates a temporary context, and in such cases all
		 *	the folder parameters should be the same.
		 *
		 *	If either cache_loc (used to create the Context Manager) or vlink_loc is OPFILE_ABSOLUTE_FOLDER both cache and visited links are disabled
		 *
		 *	@param		id				ID of the context. This id must be used
		 *	@param		manager			Context Manager, already initialized calling ConstructPrefL() or ConstructSizeL() with load_index==FALSE
		 *	@param		cookie_loc		Folder location of the cookies, should be the same as vlink_loc
		 *	@param		share_cookies_with_main_context		TRUE if this context should use the same cookies as the main browser windows
		 *	@param		load_index		TRUE to load the index (ususally TRUE, because it should not have been loaded during the construction)
		 *
		 *	LEAVES in case of OOM and other errors,
		 *
		 *	*** NOTE *** Context_Manager::ConstructPrefL() or Context_Manager::ConstructSizeL() must already have been called
		 */
		void			AddContextL(URL_CONTEXT_ID id,
			Context_Manager *manager,
			OpFileFolder cookie_loc,
			BOOL share_cookies_with_main_context = FALSE,
			BOOL load_index = TRUE
			);

	/** Remove the identified multicache context
		@param id Cache Context ID
		@param empty_context TRUE to delete all the cache files ("disposable" cache context), FALSE to keep them (if it can be reused in the future)
	*/
	void			RemoveContext(URL_CONTEXT_ID id, BOOL empty_context);

	/** Does the identified multicache context exist? */
	BOOL			ContextExists(URL_CONTEXT_ID id);

	void			IncrementContextReference(URL_CONTEXT_ID id);
	void			DecrementContextReference(URL_CONTEXT_ID id);

	/**	Return a new context ID that does not exist already. Zero(0) means failure; */
	URL_CONTEXT_ID	GetNewContextID();

	/** Attempt to "redirect" (this is done changing the URL in the address bar) the URL if there has been a reset at TCP level, via RST
	    The method tries to isolate the specific situation pinpointed in CORE-45283, to avoid side effects.
		@param url_source Initial URL that failed
		@param url_dest URL to redirect to (only meaningful if execute_redirect is TRUE)
		@param error Error that blocked the loading
		@param execute_redirect TRUE if Opera should try to redirect to url_dest
	*/
	static OP_STATUS ManageTCPReset(const URL &url_source, URL &url_dest, MH_PARAM_2 error, BOOL &execute_redirect);

#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT

public:

	/** Is this URL type registed for an external handler? */
	BOOL IsAnExternalProtocolHandler(URLType type);
	/** Register a URI scheme as having an external protocol handler 
	 *  NOTE: uses g_memory_manager->GetTempBufUni()
	 *  Returns the new URLType enum registered for the scheme if successful. URL_NULL_TYPE if operation failed,
	 */
	URLType AddProtocolHandler(const char* name, BOOL allow_as_security_context=FALSE);
#endif // EXTERNAL_PROTOCOL_SCHEME_SUPPORT
};

#include "modules/url/url_tools.h"

#endif
