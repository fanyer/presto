/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#ifndef _URL_2_H_
#define _URL_2_H_

class URL_Rep;
class URL_RelRep;
class URL_ProtocolData;
class URL_Manager;
class DataFile_Record;
class OpFile;
class URL_DataDescriptor;
class HTTP_Link_Relations;
class ServerName;
class AuthElm;
struct DebugUrlMemory;
class MemoryManager;
class MessageHandler;
class Upload_Base;
class HeaderList;
class URL_InUse;
class URL_RelRep;
class Window;
class IAmLoadingThisURL;
class AsyncExporterListener;
struct StorageSegment;
class HTMLLoadContext;

#include "modules/url/protocols/http_met.h"

#include "modules/url/url_enum.h"
#include "modules/url/url_policy.h"
#include "modules/url/url_id.h"
#include "modules/url/url_trace.h"

/** The container used to handle a link to a URL resource
 *
 *	!NOTE!NOTE!NOTE! The URL object, and copies of, it may be the ONLY handle a caller have
 *	to the identified resource! Deleting/freeing handle frequently means that the resource cannot
 *	be relocated, and will in those cases also mean that the resource is removed from the cache.
 *
 *	The string representation of the URL (the name) ***MUST*** ****NEVER**** be used in a call to
 *	g_url_api->GetURL() with the expectation of getting a URL referencec to the same resource back.
 *
 *	As mentioned, the URL object is your only reliable reference to the resource. Hold onto it with
 *	all hands as long as you are using the resource. *
 *
 *	If you need to pass around a document, pass around copies of the URL *object*, or references to
 *	structures containg a URL object; ***NEVER*** pass around a string representation of the URL name.
 *
 *	String representations of a URL Name are for presentation, bookmarking, and to initiate the
 *	original retrieval, such as from a bookmark, they should never be used for any other purpose,
 *	as they are not unique identifiers of the document contained in the URL.
 *
 *
 *		The following is an example of how ***NOT*** to use URLs
 *
 *				void InvalidUseOfURLName(URL url)
 *				{
 *					OpString8 name;
 *					url.GetAttribute(URL::KName, name);
 *
 *					URL url2 = g_url_api->GetURL(name);
 *					OP_ASSERT((url == url2) && !"NOT A VALID USE OF NAME and GetURL")
 *				}
 *
 *		For more information see https://wiki.oslo.opera.com/developerwiki/URL_Handover
 *
 *	This container contains a pointer to a URL_Rep and to a URL_RelRep object, both are
 *	referencecounted.
 *
 *	The URL_Rep object pointed to by rep contains all data pertaining to the name of the URL, its storage, etc. It
 *	also contains a list of #fragment identifiers associated with the URL, one of which may be used as
 *	the rel_rep fragment pointer
 *
 *	The rel_rep pointer points to a fragment identifier (the part after a "#" in the URL name). If the
 *	rel_rep value is not the EmptyURL_RelRep pointer (which indicates that there is no fragment identifier),
 *	the URL_Rep object owns the URL_RelRep object.
 *
 *	Primary API function
 *
 *		Load		Starts unconditional load
 *		Reload		Starts conditional load, or an unconditional reload, depending on the flags
 *		ResumeLoad	Resumes loading the resource at the location it previously stopped loading,
 *					provided it is possible or permissable to continue
 *		LoadDocument	Evaluates the best way to load the document, based on cache policies and the
 *					actual circumstances of the load request, and if necessary initiates the Load or Reload operation.
 *		StopLoading	Teminate a specific window's load, or all loads, of this URL
 *		Unload		Reset the URL (must be unreserved)  deleting all loaded information
 *
 *		GetDescriptor	Creates a URL_DataDescriptor object used to retrieve the data stored in the URL
 *		Id			Returns a unique ID for this URL.
 *
 *		GetAttribute/GetAttributeL	Retrive information about the URL on various formats
 *		SetAttribute/SetAttributeL	Set infromation about the URL
 *
 *		IsEmpty()	Is this URL an empty (blank) URL?
 *		IsValid()	Is this URL a valid URL (non-empty)?
 *		Access()	Update visited status and LRU status of the document
 *
 *		WriteDocumentData*	Group of functions used to append binary data to the URL's document.
 *
 *		PrepareForViewing	Used to remove all content encoding and prepare the document for direct
 *							access by the user and other applications.
 *		SaveAsFile			Save the document as the name file
 *		LoadToFile			Save the document as the named file, and point the cache element directly at that file
 *
 *
 *	Messages posted to the documents during loading (msg, par1, par2). par1 is ALWAYS the Id() of the sending URL,
 *	par2 depends on the message (msg) posted. Load activators need to listen for these messages.
 *
 *		MSG_HEADER_LOADED	The URL has received enough information to know what the document is,
 *							the document may now decide what to do with it. par2 is non-zero if the URL
 *							has been marked URL::KIsFollowed.
 *							This is the first message in a successful load.
 *
 *		MSG_URL_DATA_LOADED	This is sent each time more data has been received, as long as the status is URL_LOADING.
 *							The last message is sent as the URL changes status to URL_LOADED
 *							This message is also sent by data descriptors while there are more data available
 *							from the URL and it is no longer loading (The descriptor must be initialized with a message handler).
 *							par2 is non-zero if one of the loading clients specified inline loading.
 *
 *		MSG_MULTIPART_RELOAD	This message is sent to inform the document that a new header and body for the same
 *							URL will be received shortly, and should replace the current display of the document as
 *							soon as it arrives. MSG_INLINE_REPLACE is also sent at the same time.
 *							The next successful loading message will usually be a MSG_HEADER_LOADED.
 *							If the next message is a MSG_URL_DATA_LOADED, then the loading should be finished
 *							(but if it isn't, ignore the message; new bodyparts do not start unless header loaded is received)
 *
 *		MSG_URL_LOADING_FAILED	An error occured during load, par2 indicates the error code.
 *							The error code ERR_SSL_ERROR_HANDLED, means that the error has already
 *							been handled, and that loading should be silently ended.
 *
 *		MSG_URL_MOVED		The URL is being redirected to a new location, which is being loaded. the URL::KMovedToURL attribute
 *							returns the target URL. par2 is the Id of the redirect target.
 */
class URL
#ifdef URL_ENABLE_TRACER
	: private URL_Tracer
#endif
{
public:
	friend class URL_Manager;
	friend class Cache_Manager;
	friend class URL_API;
	friend class URL_Rep;
	friend class Viewers;
	friend class URL_HTTP_LoadHandler;
	friend class URL_DataStorage;
	friend class URL_InUse;
	friend class IAmLoadingThisURL;
#ifdef SELFTEST
	friend class CacheTester;
#endif
public:

	/** Redirect policy */
	enum URL_Redirect
	{
		/** Don't follow redirects */
		KNoRedirect,

		/** Follow redirects to the end */
		KFollowRedirect
	};

	/** Integer attributes */
	enum URL_Uint32Attribute
	{
		/** NOP */
		KNoInt=0,
		/* Start range */
		KUInt_Start_URL,
		/** Flag  Set Get  Has this URL been visited */
		KIsFollowed,

		/* Start range */
		KUInt_Start_Rep,
			/** URLContentType    Set Get  Type of content in the URLs*/
			KContentType,
			/** URLContentType    Set      Force the Type of content in the URL, following redirects*/
			KForceContentType,
			/** Flag  Set Get  Is this document protected by a authentication password */
			KHaveAuthentication,
			/** Flag  Set Get  Is this document protected by a form password */
			KHavePassword,
			/** Flag  Set Get  Is this a formsubmission? */
			KHTTPIsFormsRequest,
			/** Flag  Set Get  Is this an image*/
			KIsImage,
			/** URLStatus  Set Get  What is the current load status of this document*/
			KLoadStatus,
			/** URLContentType		Get  Type of content in the URLs, as originally set*/
			KOriginalContentType,
			/** Flag  Set Get  Are we relaoding this URL? */
			KReloading,
			/** Flag  Set Get  Is this URL unique? */
			KUnique,
			/** uint32 (seconds) Set Get	The period between polling for data updates from/to the server. 
			 *	If a poll period passes without any activity the error Str::S_URL_ERROR_IDLE_TIMEOUT 
			 *	is reported (sending the request header itself is not considered such activity, but 
			 *	the upload request body is).
			 *	It is recommended that KTimeoutMinimumBeforeIdleCheck is used in combination with this
			 *	timeout when the poll period is shorter than a reasonable time to establish the connection (usually 10-15 seconds)
			 *	If KTimeoutStartsAtRequestSend is configured the timeout starts running when the request has been sent (connection established)
			 */
			KTimeoutPollIdle,

			/** uint32 (seconds) Set Get the timeout for tcp connect. */
			KTimeoutMaxTCPConnectionEstablished,

			/** Flag  Set Get  Is the content of this document trusted? */
			KUntrustedContent,
#ifdef KYOCERA_GUARD_EXTENSION
			/** URL_CopyGuard     Get  Is this a "copy protected" file? */
			KCopyGuardStatus,
#endif
#ifdef WEBSERVER_SUPPORT
            /** Flag    Get     Is this URL a admin URL for a Unite service? (e.g. admin.host.username.operaunite.com) */
            KIsUniteServiceAdminURL,
			/** Flag    Get     Does this URL have admin privileges for Unite services? */
			KIsUniteServiceAdmin,
			/** Flag    Get     Is this the URL of a unite service running locally? */
			KIsLocalUniteService,
#endif
			/** Flag  Set Get   Should the URL bypass any proxy */
			KBypassProxy,
#ifdef WEB_TURBO_MODE
			/** Flag	Get		Was this URL created for Turbo Mode? */
			KUsesTurbo,
#endif
			/* Start range */
			KUInt_Start_Name,
				/** BOOL	Get  Does this URL have a ServerName? */
				KHaveServerName,
				/** uint32	Get  How long is the Authority component's maximum length */
				KInternal_AuthorityLength,
				/** UA_BaseStringId  Set Get  */
				KOverRideUserAgentId,
				/** URLType   Get  The Real URL type */
				KRealType,
				/** unsigned short     Get  The actual port this URL will access */
				KResolvedPort,
				/** unsigned short  Get  The port designated by the URL, 0 is default port for the protocol */
				KServerPort,
				/** URLType   Get  Scheme type for URL, if known */
				KType,
				KScheme = KType, // Alias	
			/* End range */
			KUInt_End_Name,

			/* Start range */
			KUInt_Start_Storage,
				/** Set Get  Auto detected character set ID (used with CharsetDetector::AutoDetectStringFromId) */
				KAutodetectCharSetID,
				/** Flag      Get  Is this document's cache in use by any datadescriptors? */
				KCacheInUse,
				/** Flag      Get  Is this document's cache entry persistent */
				KCachePersistent,
				/** Set      Compress Content in URL Enabled by API */
				KCompressCacheContent,
				/** Flag  Set Get  Always validate before displaying */
				KCachePolicy_AlwaysVerify,
				/** Flag  Set Get  Do not store in disk cache, use RAM cache only */
				KCachePolicy_NoStore,
				/** URLCacheType          Get  What type of cache is used */
				KCacheType,
				/** Flag  Set      Will always convert the current cache to a streamcache type url (no permanent storage) */
				KCacheTypeStreamCache,
				/** Flag      Get  Is the data present in the cache? */
				KDataPresent,
				/** Flag  Set Get  Disable sending and receipt of cookie */
				KDisableProcessCookies,
				/** Flag      Get  Is it more than 3 minutes since the document was loaded */
				KFermented,
				/** Flag      Get  Was proxy used when the document was loaded */
				KUseProxy,
				/** Flag  Set Get  Force the cachefile to be kept open during operation */
				KForceCacheKeepOpenFile,
				/** Flag  Set Get  Has the header of the document been loaded. This flag is an
				 *	*internal* state flag used by some URL schemes. It may not be set in all cases.
				 *	It MUST NOT be used by non-network code
				 */
				KHeaderLoaded,
				/** URLContentType    Set Get  Type of content in the URLs (used internally, not for external use) */
				K_INTERNAL_ContentType,
				/** Flag  Set		Override the load counter protection against reload during redirection (used internally, not for external use) */
				K_INTERNAL_Override_Reload_Protection,
				/** Flag  Set Get  Are we resuming this download */
				KIsResuming,
				/** Flag  Set Get  Is this a thirdparty (for cookies), relative to the first URL to reference this document */
				KIsThirdParty,
				/** Flag  Set Get  Is this at the top of the domain reach (for cookies) relative to the first URL to reference this document */
				KIsThirdPartyReach,
				/** Flag  Set  Let any authentication handling for the url be handled generically by window commander. */
				KUseGenericAuthenticationHandling,
				/** Set Get  The character set of the document  ID (used with CharsetDetector::AutoDetectStringFromId) */
				KMIME_CharSetId,
				/** Flag  Set Get  Are we reloading and  overwriting the previous download location ?*/
				KReloadSameTarget,
				/** Flag      Set  Reset the cache object */
				KResetCache,
				/** URL_Resumable_Status  Set Get   Do we think/know if this server supports resume?*/
				KResumeSupported,
				/** SecurityLevels      Get  Security level of this document (numerical values 0-9, 10 is "unknown") */
				KSecurityStatus,
#ifdef _MIME_SUPPORT_
				/** Flag  Set      MIME decode presentation uses big attachment icon */
				KBigAttachmentIcons,
				/** Flag  Set     Use the  plain text version of the document if it is available */
				KPreferPlaintext,
				/** Flag  Set     Ignore warnings from the decoding of the document (e-mail specific) */
				KIgnoreWarnings,
#endif
#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
				/** BOOL  Set      If TRUE, pause/block the download, but do not stop the request. If FALSE, resume unblock the connection */
				KPauseDownload,
#endif
#ifdef TRUST_RATING
				KTrustRating,
#endif

				/** OpSocketAddressNetType  Set Get
				 *					If set to other than NETTYPE_UNDETERMINED (the default) this
				 *					limits the network type this URL can load from to the broader
				 *					network category (e.g. localhost can access public, public cannot access local)
				 */
				KLimitNetType,

				/**
				 * Identifies the network type that given resource has been loaded from. It's used for setting KLimitNetType attribute
				 * when referrer URL is loaded from cache.
				 */
				KLoadedFromNetType,
#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
				/** Flag  Set Get	If set this flag notifies the OpSocket implementation that it is used for file download purposes. */
				KUsedForFileDownload,
#endif

#ifdef WEB_TURBO_MODE
				/** Flag	Get		Was this Turbo URL loaded in bypass mode (directly from origin server)? */
				KTurboBypassed,
				/** Flag	Get		Was the resource compressed by the Opera Turbo Proxy? */
				KTurboCompressed,
#endif // WEB_TURBO_MODE
#ifdef USE_SPDY
				/** Flag	Get		Was this URL loaded with SPDY protocol? (extensions might want to know) */
				KLoadedWithSpdy,
#endif // USE_SPDY
				/** Flag	Set	Get	Is this a Multimedia file, used by the Audio/Video tag*/
				KMultimedia,
				/** Flag  Set Get  TRUE if the MIME type was NULL or broken (which should have the same effect), as sniffing can populate the MIME type afterwards. It is used for XHR */
				KMIME_Type_Was_NULL,

				/** Flag	Get		Did the user actually start this loading, and did he/she have the opportunity to inspect the URL?*/
				KIsUserInitiated,

				/** Flag	Get		The unique id of the storage*/
				KStorageId,

				/* Start range */
				KUInt_Start_HTTPdata,
					/** Flag  Set Get  Always validate before displaying, also when moving in history */
					KCachePolicy_MustRevalidate,
					/** Flag  Set Get  Should this URL be checked every time if it is a query URL and there is no explicit expiration date? */
					KCachePolicy_AlwaysCheckNeverExpiredQueryURLs,
					/** uint  Set Get  HTTP age header */
					KHTTP_Age,
					/** Flag  Set Get  What is the HTTP method used for this request.
					 * When setting methods HTTP_METHOD_POST or HTTP_METHOD_PUT the url -must- be created
					 * using unique = TRUE in call to URL_API::GetURL */
					KHTTP_Method,
					/** signed int           Get  How long did the Refresh head say we should wait before fetching the given URL?*/
					KHTTP_Refresh_Interval,
					/** uint  Set Get  The received HTTP response code */
					KHTTP_Response_Code,
					/** Flag  Set Get  Was this retrieved over a HTTP 1.0 connection or better*/
					KHTTP_10_or_more,
					/** Flag  Set Get  Is this a HTTP redirect*/
					KIsHttpRedirect,
					/** uint  Set Get  How many requests redirects were used to get to this URL? */
					KRedirectCount,
					/** Flag  Set Get	Should we send the Accept-Encoding header? (default: yes) */
					KSendAcceptEncoding,
					/** Flag      Get  Are we still using the same User Agent setting that we loaded this document with? */
					KStillSameUserAgent,
					/** UA_BaseStringId  Set Get  Which User Agent ID did we use to load this document? */
					KUsedUserAgentId,
					/** uint  Set Get  Which version indicator did we use for the user agent ID? */
					KUsedUserAgentVersion,

					/** Flag priority of http request.  Higher number means higher priority. */
					KHTTP_Priority,
#ifdef HTTP_CONTENT_USAGE_INDICATION
					/** HTTP_ContentUsageIndication	Get Set	Indication of current usage for this URL */
					KHTTP_ContentUsageIndication,
#endif // HTTP_CONTENT_USAGE_INDICATION
#ifdef CORS_SUPPORT
					/** uint  Set Get	Should this follow CORS rules upon redirects? (default: no) */
					KFollowCORSRedirectRules,
#endif // CORS_SUPPORT
					/* End range */
				KUInt_End_HTTPdata,

				/* Start range */
				KUInt_Start_MIMEdata,
#ifdef NEED_URL_MIME_DECODE_LISTENERS
					/** Flag      Get  Does this URL have attachment listeners? */
					KMIME_HasAttachmentListener,
#endif
				/* End range */
				KUInt_End_MIMEdata,

			/* End range */
			KUInt_End_Storage,
		/* End range */
		KUInt_End_Rep,
		/* End range */
		KUInt_End_URL,

	/** End of fixed value list */
		KMaxUintAttribute,

		// The first value in the dynamic attributes list
		KStartUintDynamicAttributeList,

		// Place all full 32-bit integers with default handling below this enum
		KStartUintFullDynamicIntegers,
			/** BYTE  Set Get  The reason id for a security level of 1 or 2 */
			KSecurityLowStatusReason,
			/** MIME_ScriptEmbedSourcePolicyType  Set Get  Disable Scripting and external Embeds for this URL? */
			KSuppressScriptAndEmbeds,
			/** uint32 (seconds) Set Get	The maximum time the URL is allowed to spend loading
			 *	If this timeout is triggered the reported error is Str::S_URL_ERROR_TIMEOUT
			 *	If KTimeoutStartsAtRequestSend is configured the timeout starts running when the request has been sent (connection established)
			 *	Redirects reset timer.
			 */
			KTimeoutMaximum,
			/** uint32 (seconds) Set Get	The minimum time the URL is allowed to spend loading before starting
			 *	to poll if it is idle. Only active if KTimeoutPollIdle is configured.
			 *	If KTimeoutStartsAtRequestSend is configured the timeout starts running when the request has been sent (connection established)
			 *	Redirects reset timer.
			 */
			KTimeoutMinimumBeforeIdleCheck,
			/** uint32 (seconds) Set Get	If set the timeout checking start running when we send a request. */
			KRequestSendTimeoutValue,
			/** uint32 (seconds) Set Get	If set the timeout checking start running when we receive a response. */
			KResponseReceiveIdleTimeoutValue,
		// Place all full 32-bit integers with default handling above this line
		KEndUintFullDynamicIntegers,

		// Place all integer flags with default handling below this enum
		KStartUintDynamicFlags,
			/** BOOL  Set Get  Is this a generated Directory listing? */
			KIsDirectoryListing,
			/** BOOL  Set Get  If TRUE: Send this request only if it would be sent through a proxy, otherwise post an MSG_NOT_USING_PROXY errorcode */
			KSendOnlyIfProxy,
			/** Flag  Set Get  Is this connection specially managed? If so, move the connection
			*				   for this request (if a new one is generated) outside the normal max connection rules
			*				   NOTE: MUST be used with *extreme* caution
			*				   The flag is reset after loading starts.
			*/
			KHTTP_Managed_Connection,
			/** Flag  Set      Set to force the MIME decoder to only decode, and not produce a presentation */
			KMIME_Decodeonly,
			/** Flag  Set Get  Is the content generated*/
			KIsGenerated,
			/** Flag  Set Get  Is the content generated by Opera*/
			KIsGeneratedByOpera,
			/** Flag  Set Get  Override redirection preference, also obey cache control directives for the override URLs */
			KOverrideRedirectDisabled,
			/** Flag  Set Get  Disable HTTP redirection for this load, also turn off authentication and user interaction */
			KSpecialRedirectRestriction,
			/** Flag  Set Get	If True the timeout checking start running when we send the request, not when we start loading the URL */
			KTimeoutStartsAtRequestSend,
			/** Flag  Set Get	If True the timeout checking start running when we start the connection attempt or join the connection (when request sent), not when we start loading the URL */
			KTimeoutStartsAtConnectionStart,
			/** Flag  Set Get	Used Internally, DO NOT update. Set each time data is received or sent when idle timeouts are configured, reset when idle is checked */
			KTimeoutSeenDataSinceLastPoll_INTERNAL,
			/** Set Get OpIllegalHostPage::IllegalHostKind Type of problem if the url is invalid */
			KInvalidURLKind,
#ifdef _MIME_SUPPORT_
			/** Flag      Get  Is this URL an alias for some other URL? */
			KIsAlias,
#endif
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
			/** Flag  Set Get  Is this a Automatic Proxy Configuration file */
			KIsPACFile,
#endif
#ifdef TRUST_RATING
			KTrustRatingBypassed,
#endif
#ifdef ABSTRACT_CERTIFICATES
			/** Flag  Set Get  Requests the certificate details of the current URL. The URL must of course be a HTTPS url to get a valid Cert. It is only possible to request the certificate before the URL is loaded, therefore this method must be called before Load()*/
			KCertificateRequested,
#endif
#ifdef CONTENT_DISPOSITION_ATTACHMENT_FLAG
			/** Flag  Set Get  Did the server use a "Content-Disposition:attachment" header? */
			KUsedContentDispositionAttachment,
#endif
#ifdef _SSL_USE_SMARTCARD_
			/** Flag  Set Get  Is this document authenticated using a smart card? */
			KHaveSmartCardAuthentication,
#endif
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
			/** Flag  Set Get  Is this an out of coverage file*/
			KIsOutOfCoverageFile,
			/** Flag      Get  Is this a OEM page file that should not be removed from cache? */
			KNeverFlush,
#endif
#ifdef URL_BLOCK_FOR_MULTIPART
			/** BOOL      Get  Are we waiting for a blocking multipart*/
			KWaitingForBlockingMultipart,
#endif
#ifdef URL_SIGNAL_PROTOCOL_TUNNEL
			// This URL is used as a HTTP protocol tunnel for other protocols (e.g. OBML)
			KUsedAsTunnel,
#endif // URL_SIGNAL_PROTOCOL_TUNNEL
#ifdef URL_DISABLE_INTERACTION
			/** Flag  Set Get	If set, this flag prevents dialogs to be opened for this URL. If a dialog
			 *					would have been displayed, a safe option will be selected instead, usually ending the transaction.
			 *					The flag is permanent, until the URL is unloaded.
			 */
			KBlockUserInteraction,
#endif
#ifdef URL_ALLOW_DISABLE_COMPRESS
            /** BOOL  Set if a url should not send http compression accept headers */
	        KDisableCompress,
#endif
#ifdef IMODE_EXTENSIONS
			/** Flag  Set Get  Use IMode Utn? */
			KSetUseUtn,
#endif //IMODE_EXTENSIONS
#ifdef GOGI_URL_FILTER
		/** Flag set when URL has been approved by UI filtering. */
		KAllowed,
#endif //GOGI_URL_FILTER
#ifdef APPLICATION_CACHE_SUPPORT
			/** Flag Set Get Is this url foreign? True if the url is loaded from one cache, and has a manifest attribute that points to another cache. */
			KIsApplicationCacheForeign,

			/** Flag Set Get Is this url handled by offline application cache? **/
			KIsApplicationCacheURL,
#endif // APPLICATION_CACHE_SUPPORT

#if MAX_PLUGIN_CACHE_LEN>0
			/** Instruct Opera to use the stream cache (if possible), as required by the ns4plugins module (CORE-25492).
				Note that the attribute is not saved on disk.
			*/
			KUseStreamCache,
#endif
			/** Flag Get Set If set, the url will not be used as a referrer url */
			KDontUseAsReferrer,

			/** Flag Get Set Allows a document to use scripting regardless of scripting being enabled */
			KForceAllowScripts,

#ifdef URL_FILTER
			/** Flag to tell that this url should not be filtered by the content blocker. */
			KSkipContentBlocker,
#endif // URL_FILTER

#ifdef ERROR_PAGE_SUPPORT
			/** Flag Get only. Tells that this url represents a click-through page like
			 *  opera:crossnetworkwarning, opera:site-warning and opera:clickjackblocking.
			 *  KIsGeneratedByOpera should be set as well, and Type() should be URL_OPERA . */
			KIsClickThroughPage,
#endif // ERROR_PAGE_SUPPORT

			/** This is an url generated to represent a unique origin.
			 *  See DocumentOrigin::MakeUniqueOrigin(). */
			KIsUniqueOrigin,

#ifdef SELFTEST
			/** Flag  Set Get  Is the content generated for a selftest. */
			KIsGeneratedBySelfTests,
#endif // SELFTEST

		// Place all integer flags with default handling above this line
		KEndUintDynamicFlags,

		// Place all 32-bit integers and flags with NON-default handling below this enum
		KStartUintDynamicspecialIntegers,
		// Place all 32-bit integers and flags with NON-default handling above this line
		KEndUintDynamicSpecialIntegers,

		// The first value in the dynamic attribute item list (non-URL module attributes)
		KFirstUintDynamicAttributeItem
	};

	/** 8-bit character string attributes */
	enum URL_StringAttribute
	{
		/** NOP */
		KNoStr=0,
		/* Start range */
		KStr_Start_URL,
			/**		Get The fragment part of the URL */
			KFragment_Name,
		/* Start range */
		KStr_Start_Rep,
			/* Start range */
			KStr_Start_Name,
				/**     Get  The name of the server*/
				KHostName,
				/**     Get  The "name:port" of the server, use only with GetAttributeL */
				KHostNameAndPort_L,
				/**     Get  Get the password part of the URL */
				KPassWord,
				/**     Get  Get the path part of the URL (does not include the query portion)*/
				KPath,
				/**		Get  The path and query component of the URL combined to one string*/
				KPathAndQuery_L,
				/**     Get  The name of the protocol used by the URL */
				KProtocolName,
				/**		Get  The query component of the URL. When a query is present, this string will always include the leading "?" */
				KQuery,
				/**     Get  The username part of the URL */
				KUserName,
			/* End range */
			KStr_End_Name,
			/* Start range */
			KStr_Start_Storage,
				/** Set Get  Auto detected character set*/
				KAutodetectCharSet,
				/** Set      Set a Age HTTP directive */
				KHTTPAgeHeader,
				/** Set      Set a Cache Control directive */
				KHTTPCacheControl,
				/** Set      Set a Cache Control directive, used from HTTP */
				KHTTPCacheControl_HTTP,
				/** Set      Set a Cache expiration directive */
				KHTTPExpires,
				/** Set      Set a Cache Control directive */
				KHTTPPragma,
				/** Set Get  The character set of the document */
				KMIME_CharSet,
				/** Set      Force the MIME-type to this */
				KMIME_ForceContentType,
				/** Set Get  Set/Get the MIME type string*/
				KMIME_Type,
				/**     Get  Get the Original MIME type string, but if the MIME type is undetermined (empty), URL will try to detect a MIME type anyway */
				KOriginalMIME_Type,
				/**	  Get  Get MIME type as returned by server */
				KServerMIME_Type,
				/* Start range */
				KStr_Start_HTTP,
					/** Set Get	 HTTP authentication username */
					KHTTPUsername,
					/** Set Get	 HTTP authentication password */
					KHTTPPassword,
					/** Set Get  HTTP default style string */
					KHTTPDefaultStyle,
					/** Set Get  The Content Language*/
					KHTTPContentLanguage,
					/** Set      The real location of the content */
					KHTTPContentLocation,
					/** Set Get  The MIME type of the POST data for this document*/
					KHTTP_ContentType,
					/** Set Get  The HTTP Date header */
					KHTTP_Date,
					/** Set Get  The content encoding (e.g. gzip) */
					KHTTPEncoding,
					/** Set Get  The HTTP Entity Header*/
					KHTTP_EntityTag,
					/** Set Get  The X-Frame-Options header */
					KHTTPFrameOptions,
					/** Set Get  The received Last modified date*/
					KHTTP_LastModified,
					/** Set Get  The received location header */
					KHTTP_Location,
					/** Set Get  Does not return a value on const string, use set string variants*/
					KHTTP_MovedToURL_Name,
					/** Set Get  The URL part of the Refresh header */
					KHTTPRefreshUrlName,
					/** Set Get  The response explanation part of the HTTP response*/
					KHTTPResponseText,
					/**     Get   Retrieve a string with all preserved headers from the last response */
					KHTTPAllResponseHeadersL,
					/**     Get   Retrieve a string with a specific header from the last response */
					KHTTPSpecificResponseHeaderL,
					/**     Get   Retrieve a concatenated string with all values for a specific header name. */
					KHTTPSpecificResponseHeadersL,
				/* End range */
				KStr_End_HTTP,

				/* Start range */
				KStr_Start_Protocols,
				/* Start range */
				KStr_Start_FTP,
					/** Set Get  The FTP MDTM (modification) date*/
					KFTP_MDTM_Date,
				/* End range */
				KStr_End_FTP,

				/* Start range */
				KStr_Start_Mail,
				/* End range */
				KStr_End_Mail,
				/* End range */
				KStr_End_Protocols,
				/* End range */
			KStr_End_Storage,
		/* End range */
		KStr_End_Rep,
		/* End range */
		KStr_End_URL,

		/** End of fixed enum list */
		KMaxStringAttribute,

		// The first value in the dynamic attributes list
		KStartStrDynamicAttributeList,

		// Place all strings with default handling below this enum
		KStartStrDynamicString,
		/** Set Get  Mail To addresses */
		KMailTo,
		/** Set Get  Mail Cc addresses */
		KMailCC,
		/** Set Get  Mail BCc adresses */
		KMailBCC,
		/** Set Get  Mail From address */
		KMailFrom,
		/** Set Get  Mail subject */
		KMailSubject,
		/** Set Get  Mail content type */
		KMailContentType,
		/** Set Get  Mail Data string */
		KMailDataStr,
		// Place all strings with default handling above this line
		KEndStrDynamicString,

		// Place all strings with NON-default handling below this enum
		KStartStrDynamicSpecialStrings,
		/**
		 *	The applied string will, after preprocessing according to the requirments in step 2 through 4 of
		 *	http://dev.w3.org/2006/webapi/XMLHttpRequest/#the-open-method (sec. 3.6.1) be used as the HTTP method
		 *	sent to the server with the request for this URL.
		 *
		 *	Invalid methods (e.g. non-tokens, non-allowes values, or containing whitespace) will result in OpStatus::ERR_PARSING_FAILED being returned,
		 *	and the method becomes HTTP_METHOD_Invalid, which will result in a Bad request error without network accesss
		 *
		 *	If the string is one of the known methods, that method value will be used, if it is any other valid value
		 *	the string will be used in the request.
		 *
		 *	When a non-HEAD/GET method is used the URL is automatically made Unique.
		 *
		 *	All requests that are not HEAD or GET are sent on a new connection, like POST, and are non-resendable, and sent with
		 *	Content-Length: 0 if no upload  element hasn't been defined. Requests in the queue will be held pending the reply
		 *	to avoid issues with pipelining.
		 */
		KHTTPSpecialMethodStr,
		// Place all strings with NON-default handling above this line
		KEndStrDynamicSpecialStrings,

		// The first value in the dynamic attribute item list (non-URL module attributes)
		KFirstStrDynamicAttributeItem
	};

	/** 8-bit character string attributes */
	enum URL_NameStringAttribute
	{
		KNoNameStr = 0,

		/**     Get  The name of the URL, without username and password,
		*           non-special escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KName = 1,
		/**     Get  The name of the URL, without username and
		*           password, escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KName_Escaped=2,
		/**     Get  The name of the URL, including username but not
		*           password, non-special escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KName_Username=3,
		/**     Get  The name of the URL, including username but not
		*           password, escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KName_Username_Escaped=4,
		/**     Get  The name of the URL, including username and
		*           password, non-special escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*
		*           !!!!NOTE!!!! This result MUST NOT be exported to the UI or
		*           scripts unless there is a very good reason for it!!!
		*/
		KName_Username_Password_NOT_FOR_UI=5,
		/**     Get  The name of the URL, including username and
		*           password, escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*
		*           !!!!NOTE!!!! This result MUST NOT be exported to the UI or
		*           scripts unless there is a very good reason for it!!!
		*/
		KName_Username_Password_Escaped_NOT_FOR_UI=6,
		/**     Get  The name of the URL, including username, but password
		*           is replaced by "*****", non-special escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KName_Username_Password_Hidden=7,
		/**     Get  The name of the URL, including username, but password
		*           is replaced by "*****", escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KName_Username_Password_Hidden_Escaped=8,
		/**     Get  The name of the URL with the fragment identifier, without username and password,
		*           non-special escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KName_With_Fragment=9,
		/**     Get  The name of the URL with the fragment identifier, without username and
		*           password, escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KName_With_Fragment_Escaped=10,
		/**     Get  The name of the URL with the fragment identifier, including username but not
		*           password, non-special escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KName_With_Fragment_Username=11,
		/**     Get  The name of the URL with the fragment identifier, including username but not
		*           password, escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KName_With_Fragment_Username_Escaped=12,
		/**     Get  The name of the URL with the fragment identifier, including username and
		*           password, non-special escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*
		*           !!!!NOTE!!!! This result MUST NOT be exported to the UI or
		*           scripts unless there is a very good reason for it!!!
		*/
		KName_With_Fragment_Username_Password_NOT_FOR_UI=13,
		/**     Get  The name of the URL with the fragment identifier, including username and
		*           password, escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*
		*           !!!!NOTE!!!! This result MUST NOT be exported to the UI or
		*           scripts unless there is a very good reason for it!!!
		*/
		KName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI=14,
		/**     Get  The name of the URL with the fragment identifier, including username, but password
		*           is replaced by "*****", non-special escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KName_With_Fragment_Username_Password_Hidden=15,
		/**     Get  The name of the URL with the fragment identifier, including username, but password
		*           is replaced by "*****", escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KName_With_Fragment_Username_Password_Hidden_Escaped=16
	};

	/** uni_char string attributes */
	enum URL_UniStringAttribute
	{
		/** NOP */
		KNoUniStr=0,
		/* Start range */
		KUStr_Start_URL,
			/**		Get The fragment part of the URL */
			KUniFragment_Name,
		/* Start range */
		KUStr_Start_Rep,
			/**     Get  Suggested Filename, GetAttributeL only */
			KSuggestedFileName_L,
			/**     Get  Suggested Filename extension, GetAttributeL only */
			KSuggestedFileNameExtension_L,

			/* Start range */
			KUStr_Start_Name,
			/**     Get  The hostname*/
			KUniHostName,
			/**     Get  The "name:port" of the server, use only with GetAttributeL */
			KUniHostNameAndPort_L,
			/**     Get  Filename extension, GetAttributeL only */
			KUniNameFileExt_L,
			/* End range */
			KUStr_End_Name,
			/* Start range */
			KUStr_Start_Storage,
#if defined _LOCALHOST_SUPPORT_ || !defined RAMCACHE_ONLY
				/**     Get  The name of the file (in case of cachefile, not including path) */
				KFileName,
				/**     Get  The full path of the Cache file */
				KFilePathName,
				/**     Get  The full path of the Cache file (LEAVE ONLY)*/
				KFilePathName_L,
#endif
				/** Set Get  The Last internal error message*/
				KInternalErrorString,
				/** Set Get  The last security level text*/
				KSecurityText,
				/* Start range */
				KUStr_Start_HTTP,
					/** Set get  Unicode version of suggested filename*/
					KHTTP_SuggestedFileName,
				/* End range */
				KUStr_End_HTTP,
				/* Start range */
				KUStr_Start_MIME,
#ifdef _MIME_SUPPORT_
					/** Set Get  MIME body comment string  */
					KBodyCommentString,
					/** Set Get  MIME body comment string (for set: Append to existing string) */
					KBodyCommentString_Append,
#endif // _MIME_SUPPORT_
				/* End range */
				KUStr_End_MIME,
				/* End range */
			KUStr_End_Storage,
		/* End range */
		KUStr_End_Rep,
		/* End range */
		KUStr_End_URL,
		/** End of list */
		KMaxUniStringAttribute
	};

	/** uni_char name string attributes */
	enum URL_UniNameStringAttribute
	{
		KNoUniNameStr = 0,

		/**     Get  The name of the URL, without username and password,
		*           non-special UTF-8 escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName =1,
		/**     Get  The name of the URL, without username and
		*           password, escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_Escaped=2,
		/**     Get  The name of the URL, including username but not
		*           password, non-special UTF-8 escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_Username=3,
		/**     Get  The name of the URL, including username but not
		*           password, escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_Username_Escaped=4,
		/**     Get  The name of the URL, including username and
		*           password, non-special UTF-8 escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*
		*           !!!!NOTE!!!! This result MUST NOT be exported to the UI or
		*           scripts unless there is a very good reason for it!!!
		*/
		KUniName_Username_Password_NOT_FOR_UI=5,
		/**     Get  The name of the URL, including username and
		*           password, escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*
		*           !!!!NOTE!!!! This result MUST NOT be exported to the UI or
		*           scripts unless there is a very good reason for it!!!
		*/
		KUniName_Username_Password_Escaped_NOT_FOR_UI=6,
		/**     Get  The name of the URL, including username, but password
		*           is replaced by "*****", non-special UTF-8 escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_Username_Password_Hidden=7,
		/**     Get  The name of the URL, including username, but password
		*           is replaced by "*****", escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_Username_Password_Hidden_Escaped=8,
		/**     Get  The name of the URL with the fragment identifier, without username and password,
		*           non-special  UTF-8 escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_With_Fragment=9,
		/**     Get  The name of the URL with the fragment identifier, without username and
		*           password, escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_With_Fragment_Escaped=10,
		/**     Get  The name of the URL with the fragment identifier, including username but not
		*           password, non-special UTF-8 escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_With_Fragment_Username=11,
		/**     Get  The name of the URL with the fragment identifier, including username but not
		*           password, escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_With_Fragment_Username_Escaped=12,
		/**     Get  The name of the URL with the fragment identifier, including username and
		*           password, non-special UTF-8 escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*
		*           !!!!NOTE!!!! This result MUST NOT be exported to the UI or
		*           scripts unless there is a very good reason for it!!!
		*/
		KUniName_With_Fragment_Username_Password_NOT_FOR_UI=13,
		/**     Get  The name of the URL with the fragment identifier, including username and
		*           password, escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*
		*           !!!!NOTE!!!! This result MUST NOT be exported to the UI or
		*           scripts unless there is a very good reason for it!!!
		*/
		KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI=14,
		/**     Get  The name of the URL with the fragment identifier, including username, but password
		*           is replaced by "*****", non-special UTF-8 escaped sequences are deescaped
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_With_Fragment_Username_Password_Hidden=15,
		/**     Get  The name of the URL with the fragment identifier, including username, but password
		*           is replaced by "*****", escaped sequences are preserved
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_With_Fragment_Username_Password_Hidden_Escaped=16,

		/**     Get  The name of the URL to be used for reading data.
		*           It's either a KUniName or KUniName with path replaced with "symbolic link" target.
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_For_Data=17,

		/**     Get  The name of the URL to be used for tel: protocol.
		*           NOTE: GetAttribute returns pointer to a "fixed" workbuffer.
		*           NOTE: The "fixed" work buffer can be deleted by URL_Rep construction
		*           (e.g. by URL_API::GetURL).
		*/
		KUniName_For_Tel=18,

		/**     Get  Path component of URL*/
		KUniPath,
		/**     Get  Path component of URL, with escaped characters*/
		KUniPath_Escaped,
		/**     Get  Path and query component of URL*/
		KUniPathAndQuery,
		/**     Get  Path and query component of URL, with escaped characters*/
		KUniPathAndQuery_Escaped,
#ifdef URL_FILE_SYMLINKS
		/**     Get Set  Target path of a URL that is a "symbolic link"*/
		KUniPath_SymlinkTarget,
		/**     Get  Target path of a URL that is a "symbolic link", with escaped characters*/
		KUniPath_SymlinkTarget_Escaped,
#endif // URL_FILE_SYMLINKS
		/**     Get  Path to use for reading data. It's either KUniPath or KUniPath_SymlinkTarget*/
		KUniPath_FollowSymlink,
		/**     Get  Path to use for reading data, with escaped characters*/
		KUniPath_FollowSymlink_Escaped,
		/**     Get  Query component of URL. When a query is present, this string will always include the leading "?"*/
		KUniQuery_Escaped
	};

	/** void pointer attributes (must be converted to the specified type) */
	enum URL_VoidPAttribute
	{
		/** NOP */
		KNoVoip=0,
		/* Start range */
		KVoip_Start_URL,
			/* Start range */
			KVoip_Start_Rep,
				/** URL_ID *  Get  The ID of this URL, usually the URL_Rep pointer value. Will be stored in the provided param location */
				K_ID,
				/** time_t Set Get  When did we last visit this document (Time is UTC). Will be stored in the provided param location */
				KVLocalTimeVisited,/*time_t */
				/* Start range */
				KVoip_Start_Name,
					/** (const protocol_selentry *)    Get  Pointer to scheme specification structure, defined in prot_sel.h */
					KProtocolScheme,
					/** (ServerName *)      Get  Server Name pointer */
					KServerName,
				/* End range */
				KVoip_End_Name,
				/* Start range */
				KVoip_Start_Storage,
					/** time_t    Get  When was this document last modified, according to the server? Will be stored in the provided param location*/
					KVLastModified,
					/** time_t Set Get  When did we start loading this document (Time is UTC). Will be stored in the provided param location*/
					KVLocalTimeLoaded,/*time_t */
					/** OpFileLength *      Get  Number of bytes loaded from server will be stored in the provided param location */
					KContentLoaded,
					/** OpFileLength *      Get  Number of bytes loaded from server will be stored in the provided param location,
					 *							 this will force an updated value of the size to be retrieved */
					KContentLoaded_Update,
					/** OpFileLength *  Set Get  Number of bytes expected will be/is  stored in the provided param location*/
					KContentSize,
#ifdef WEB_TURBO_MODE
					/** UINT32 Set Get  The number of bytes transferred from the Turbo proxy, including HTTP protocol overhead. NB! Set is only for internal use! */
					KTurboTransferredBytes,
					/** UINT32 Set Get	The number of bytes received by the Turbo proxy when loading this resource, including HTTP protocol overhead.
					 *					NB! This may in special cases return zero even if KTurboTransferredBytes returns non-zero (e.g. host lookup failed). Set is only for internal use! */
					KTurboOriginalTransferredBytes,
#endif // WEB_TURBO_MODE
					/** Flag	Get	the size limit of the streaming multimedia cache */
					KMultimediaCacheSize,
					/* Start range */
					KVoip_Start_HTTP,
						/** Set      Adds a customized header to those that will be sent when this URL is sent. param is (URL_Custom_Header *), defined in tools/ */
						KAddHTTPHeader,
						/**     Get  HTTP link relations structure */
						KHTTPLinkRelations,
#ifdef URL_ENABLE_HTTP_RANGE_SPEC
						/** OpFileLength * Set Get	Position in the request that we are requesting that the server stops the response at (if not specified or 0, at end, and not sent) inclusive. zero-based*/
						KHTTPRangeEnd,
						/** OpFileLength * Set Get	Position in the request that we are requesting that the server starts the response at (if not specified or 0, at beginning, and not sent). zero-based */
						KHTTPRangeStart,
#endif
						/** time_t  Set Get  When does this document's freshness expire. Will be stored in the provided param location */
						KVHTTP_ExpirationDate,

#ifdef ABSTRACT_CERTIFICATES
						/** Set Get  Gets the certificate relating to this URL. The URL must be a HTTPS url and the certificate must have been requested using the RequestCertficate() method before loading began. A response from the web server must be received before this method can be called.(OpCertificate*) */
						KRequestedCertificate,
#endif
						/** Get      Gets the number of bytes the URL intends to or has uploaded. */
						KHTTP_Upload_TotalBytes,
					/* End range */
					KVoip_End_HTTP,
					/* Start range */
					KVoip_Start_Mailto,
#ifdef HAS_SET_HTTP_DATA
						/** (Upload_Base *)   Get  Retrieve the upload element for the mailto URL. Does NOT release the upload element. !!! DO NOT delete !!! */
						KMailTo_UploadData,
#endif
					/* End range */
					KVoip_End_Mailto,
					/* Start range */
					KVoip_Start_MIME,
#ifdef NEED_URL_MIME_DECODE_LISTENERS
						/** MIMEDecodeListener * Set     Add the given attachment listener to list of listeners*/
						KMIME_AddAttachmentListener,
						/** MIMEDecodeListener * Set     Remove the given attachment listener from list of listeners*/
						KMIME_RemoveAttachmentListener,
#endif // M2
					/* End range */
					KVoip_End_MIME,
				/* End range */
				KVoip_End_Storage,
			/* End range */
			KVoip_End_Rep,
		/* End range */
		KVoip_End_URL,


		/** End of list */
		KMaxVoidPAttribute
	};

	/** URL attributes (return URL objects) */
	enum URL_URLAttribute
	{
		/** NOP */
		KNoUrl=0,

		/* Start range */
		KURL_Start_URL,
			/* Start range */
			KURL_Start_Rep,
				/* Start range */
				KURL_Start_Storage,
					/* Start range */
					KURL_Start_HTTP,

						/**     Get  Redirection target */
						KMovedToURL,

						/**     Get  Get the Content-Location URL specified by the server*/
						KHTTPContentLocationURL,

						/**     Get  Get the last URL used as a referrer for this document*/
						KReferrerURL,

#ifdef _SECURE_INFO_SUPPORT
						/** Set Get  Get the document with the security information for this document*/
						KSecurityInformationURL,
#endif
					/* End range */
					KURL_End_HTTP,
					/* Start range */
					KURL_Start_FTP,
					/* End range */
					KURL_End_FTP,
					/* Start range */
					KURL_Start_MIME,
						/** Set  Get  Which URL is this URL an alias for? */
						KAliasURL,
						/** Set  Get  Which URL is this URL a base URL for? */
						KBaseAliasURL,
#ifdef NEED_URL_MIME_DECODE_LISTENERS
						/** Action Set     Signal the attachment listeners that the given attachment has been created*/
						KMIME_SignalAttachmentListeners,
#endif
					/* End range */
					KURL_End_MIME,
				/* End range */
				KURL_End_Storage,
			/* End range */
			KURL_End_Rep,
		/* End range */
		KURL_End_URL,

		/** End of fixed enum list */
		KMaxURLAttribute,

		// The first value in the dynamic attributes list
		KStartURLDynamicAttributeList,

		// Place all URL attributes with default handling below this enum
		KStartURLDynamicURLs,
#ifdef APPLICATION_CACHE_SUPPORT
		/** Set Get If the loading fails, GetDescriptor will return this fallback url instead.
		 * 	Should only be used internally in url */
		KFallbackURL,
#endif // APPLICATION_CACHE_SUPPORT
		// Place all URL attributes with default handling above this line
		KEndURLDynamicURLs,

		// Place all URL attributes with NON-default handling below this enum
		KStartURLDynamicSpecialURLs,
		// Place all URL attributes with NON-default handling above this line
		KEndURLDynamicSpecialURLs,

		// The first value in the dynamic attribute item list (non-URL module attributes)
		KFirstURLDynamicAttributeItem
	};

	enum URL_WriteDocumentConversion {
		/** Normal Raw Data */
		KNormal,
		/** HTMLify data */
		KHTMLify,
		/** HTMLify data, if there are URLs A tag links are created */
		KHTMLify_CreateLinks,
		/** XMLify data */
		KXMLify,
		/** XMLify data, if there are URLs A tag links are created */
		KXMLify_CreateLinks
	};

#ifdef URL_ENABLE_ASSOCIATED_FILES
	/**
	 * bits for types of associated files. If this is updated, the corresponding enum in op_resource.h must also be updated.
	 */
	enum AssociatedFileType {
		Thumbnail = 1 << 0,			// Not allowed in HTTPS pages
		CompiledECMAScript = 1 << 1,
		ConvertedwOFFFont = 1 << 2,
	};
#endif

#ifdef CORS_SUPPORT
	enum {
		/** Perform CORS verification on redirects. */
		CORSRedirectVerify = 1,
		/** Allow same-origin requests to be redirected cross-origin, non-anonymous. */
		CORSRedirectAllowSimpleCrossOrigin = 2,

		/** Allow same-origin requests to be redirected cross-origin, anonymously. */
		CORSRedirectAllowSimpleCrossOriginAnon = 3,

		/** Disallow same-origin requests to be redirected cross-origin. */
		CORSRedirectDenySimpleCrossOrigin = 4,

		CORSRedirectMaxValue = CORSRedirectDenySimpleCrossOrigin
	};
#endif // CORS_SUPPORT

private:

	/** The URL representation that defines this URL */
	URL_Rep*			rep;

	/** The fragment identifier of this URL. Owned by the rep object */
	URL_RelRep*			rel_rep;

public:

	/** Default constructor */
						URL();

	/** Copy Constructor */
						URL(const URL& url);

	/** Constructor
	 *
	 *	Create a URL object copy that have the rel string as the fragment part
	 *
	 *  @param	url		The original URL
	 *	@param	rel		fragment part to be used, may be discarded during OOM situations
	 */
						URL(const URL& url, const uni_char* rel);

	/** Constructor based on URL_Rep object
	 *
	 *	NOTE: SHOULD NOT be used outside the URL module, and sparingly within the URL module
	 *
	 *	Create a URL object base on the given URL_Rep and that have the rel string as the fragment part
	 *
	 *  @param	url		The URL_Rep of the original URL
	 *	@param	rel		fragment part to be used, may be discarded during OOM situations
	 */
						URL(URL_Rep* url_rep, const char* rel);

	/** Assignment operator */
						URL	&operator=(const URL& url);

	/** Destructor */
						~URL();

	/** Is this the same URL as the other URL? Disregards fragment part */
	BOOL				operator==(const URL& url) const { return rep == url.rep; }


#ifndef RAMCACHE_ONLY
	/** Write this URLs cache information to the given DataFile_Record
	 *
	 *	@param	rec		The record target
	 */
	void				WriteCacheDataL(DataFile_Record *rec);
#endif

	/** Does this URL have a valid URL? */
	BOOL				IsEmpty() const;

	/** Is this URL non-empty? */
	BOOL				IsValid() const;

#ifdef URL_ACTIVATE_OLD_NAME_FUNCS
	/**
	 *	@deprecated   Use GetAttribute(URL::KUniNameVariations, charsetID, string_ret, follow_ref) instead;
	 *			GetAttribute(URL::KNameVariations, string_ret, follow_ref, charsetID) can also be used as of url_6
	 *			You need to select the proper value of attr from the list of available list of URL::KUniName* enums
	 *
	 *	Import with API_URL_NAME_WITH_PASSWORD_STATUS
	 *
	 *	Return the unicode version of the URLs name, modified as password_hide specifies
	 *
	 *	@param	password_hide	What to include in the URL string
	 */
	DEPRECATED(const uni_char *UniName(Password_Status password_hide) const);

	/**
	 *	@deprecated   Use GetAttribute(URL::KNameVariations, charsetID, string_ret, follow_ref) instead;
	 *			You need to select the proper value of attr from the list of available list of URL::KName* enums
	 *
	 *	Import with API_URL_NAME_WITH_PASSWORD_STATUS
	 *
	 *	Return the 8-bit version of the URLs name, modified as password_hide specifies
	 *
	 *	@param	password_hide	What to include in the URL string
	 */
	DEPRECATED(const char* Name(Password_Status password_hide) const);

	/**
	 *	@deprecated   Use GetAttribute(URL::KUniNameVariations, charsetID, string_ret, follow_ref) instead;
	 *			GetAttribute(URL::KNameVariations, string_ret, follow_ref, charsetID) can also be used as of url_6
	 *			You need to select the proper value of attr from the list of available list of URL::KUniName* enums
	 *
	 *	Import with API_URL_NAME_WITH_PASSWORD_STATUS
	 *
	 *  Return the unicode version of the URLs name including the fragment identifier, modified as password_hide specifies
	 *
	 *	@param	password_hide	What to include in the URL string
	 */
	DEPRECATED(const uni_char* GetUniNameWithRel(Password_Status password_hide) const);

	/**
	 *	@deprecated   Use GetAttribute(URL::KNameVariations, string_ret, follow_ref) instead;
	 *			You need to select the proper value of attr from the list of available list of URL::KName* enums
	 *
	 *	Import with API_URL_NAME_WITH_PASSWORD_STATUS
	 *
	 *	Return the 8-bit version of the URLs name, modified as password_hide specifies,
	 *	@param	password_hide	What to include in the URL string
	 */
	DEPRECATED(const char* GetName(BOOL follow_ref,Password_Status password_hide));

	/**
	 *	@deprecated   Use GetAttribute(URL::KUniNameVariations, charsetID, string_ret, follow_ref) instead;
	 *			GetAttribute(URL::KNameVariations, string_ret, follow_ref, charsetID) can also be used as of url_6
	 *			You need to select the proper value of attr from the list of available list of URL::KUniName* enums
	 *
	 *	Import with API_URL_NAME_WITH_PASSWORD_STATUS
	 *
	 *	Return the unicode version of the URLs name, modified as password_hide specifies
	 *	If specified, return the name of the URL at the *end* of the redirection chain
	 *
	 *	@param	follow_ref		If TRUE return the name of the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise return the name of this URL.
	 *
	 *	@param	password_hide	What to include in the URL string
	 */
	DEPRECATED(const uni_char* GetUniName(BOOL follow_ref,Password_Status password_hide));
#endif // URL_ACTIVATE_OLD_NAME_FUNCS

	/**
	 *	Start loading this document according to the load information (which specifies inline loading, cache-policy, user-interaction etc.)
	 *	This operation may initiate network action or  automatic generation of a document, depending on the document type, but may also
	 *	decide to use the document as-is without network action.
	 *
	 *	@param	mh				MessageHandler that will be used to post update messages to the caller
	 *	@param	referer_url		The URL (if any) of the document that pointed to this URL
	 *	@param	loadpolicy		Specification of cache policy, inline loading, user interaction, etc. used to determine how to load the document
	 *  @param  html_ctx			Element that triggered the load (NULL means unknown); the caller is responsible for deleting the object
	 *
	 *	@return CommState		The status of the load action. with the following values returned
	 *
	 *					COMM_LOADING	Loading has started, normal messagehandling
	 *					COMM_REQUEST_FAILED		Loading aborted, ususally due to incorrect input data, no messages will be sent.
	 *					COMM_REQUEST_FINISHED	Document already loaded, no messages will be sent.
	 */
	CommState	LoadDocument(MessageHandler* mh, const URL& referer_url, const URL_LoadPolicy &loadpolicy, HTMLLoadContext *html_ctx = NULL);

	/** Start loading this URL
	 *
	 *	@param	mh		MessageHandler that will be used to post update messages to the caller
	 *	@param	referer_url				The URL (if any) of the document that pointed to this URL
	 *	@param	inline_loading			Is this an inline element?
	 *	@param	load_silent				Deprecated
	 *	@param	user_initiated			Did the user actually start this loading, and did he/she have the opportunity to inspect the URL?
	 *	@param	thirdparty_determined	Have we already determined the thirdparty status of this document? If so, no need to do it twice.
	 */
	CommState			Load(MessageHandler* mh, const URL& referer_url, BOOL inline_loading = FALSE, BOOL load_silent = FALSE, BOOL user_initiated=FALSE, BOOL thirdparty_determined=FALSE);

	/** Start reloading this URL
	 *
	 *	@param	mh		MessageHandler that will be used to post update messages to the caller
	 *	@param	referer_url		The URL (if any) of the document that pointed to this URL
	 *	@param	only_if_modified		We only want a new copy of the document if it has been updated
	 *	@param	proxy_no_cache			Bypass proxies
	 *	@param	inline_loading			Is this an inline element?
	 *	@param	load_silent				Deprecated
	 *	@param	check_modified_silent	Deprecated
	 *	@param	user_initiated			Did the user actually start this loading, and did he/she have the opportunity to inspect the URL?
	 *	@param	thirdparty_determined	Have we already determined the thirdparty status of this document? If so, no need to do it twice.
	 *	@param	[out]allow_if_modified	Set to TRUE if only_if_modified was TRUE and the request will go ahead as conditional.
	 */
	CommState			Reload(MessageHandler* mh, const URL& referer_url, BOOL only_if_modified, BOOL proxy_no_cache, BOOL inline_loading = FALSE, BOOL check_modified_silent = FALSE, BOOL load_silent = FALSE, BOOL user_initiated=FALSE, BOOL thirdparty_determined=FALSE, EnteredByUser entered_by_user = NotEnteredByUser, BOOL* allow_if_modified = NULL);

	/** Resume loading of this URL at the location it was interrupted, if possible
	 *
	 *	@param	mh		MessageHandler that will be used to post update messages to the caller
	 *	@param	referer_url				The URL (if any) of the document that pointed to this URL
	 *	@param	inline_loading			Is this an inline element?
	 *	@param	load_silent				Deprecated
	 *	@param	check_modified_silent	Deprecated
	 */
	CommState			ResumeLoad(MessageHandler* mh, const URL& referer_url, BOOL inline_loading = FALSE, BOOL check_modified_silent = FALSE, BOOL load_silent = FALSE);

	/** Quick load of the URL if is it possible. Currently used for file (not directories) and data URLs
	 *
	 *	@param	guess_content_type		Should we try to guess the content type of this document
	 */
	BOOL				QuickLoad(BOOL guess_content_type);

	/** Get a URL_DataDescriptor for this URL, if possible
	 *	If specified get it for the URL at the end of the redirection chain.
	 *
	 *	@param	mh		MessageHandler that will be used to post update messages to the caller
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise return a descriptor of this URL.
	 *	@param	get_raw_data	If TRUE, don't use character set convertion of the data (if relevant)
	 *	@param	get_decoded_data	If TRUE, decode the data using the content-encoding specification (usually decompression)
	 *	@param	window			The Window whose charset preferences is used to guess/force the character set of the document
	 *	@param	override_content_type	What content-type should the descriptor assume the content is. Use this if the server
	 *									content-type is different from the actual type.
	 *	@param	override_charset_id		What charset ID should the descriptor use for the content?
	 *	@param	get_original_content	Should the original content be retrieved? E.g., in case the content has been decoded,
	 *									and the original content is still availablable, such as for MHTML
	 *	@param	parent_charset_id		Character encoding (charset) passed from the parent resource.
	 *									Should either be the actual encoding of the enclosing resource,
	 *									or an encoding recommended by it, for instance as set by a CHARSET
	 *									attribute in a SCRIPT tag. Pass 0 if no such contextual information
	 *									is available.
	 *
	 *	@return URL_DataDescriptor *	An allocated datadescriptor that may be used to retrieve the data contained in the URL.
	 *									NOTE: Caller takes ownership of the descriptor an MUST delete it after use.
	 *									NOTE: The returned value may be NULL if it is no data in the URL at the present time.
	 */
	URL_DataDescriptor*	GetDescriptor(MessageHandler *mh, URL::URL_Redirect follow_ref, BOOL get_raw_data=FALSE, BOOL get_decoded_data=TRUE,
		Window *window = NULL, URLContentType override_content_type = URL_UNDETERMINED_CONTENT,
		unsigned short override_charset_id=0, BOOL get_original_content=FALSE, unsigned short parent_charset_id = 0);
	/** @overload */
	URL_DataDescriptor*	GetDescriptor(MessageHandler *mh, BOOL follow_ref, BOOL get_raw_data=FALSE, BOOL get_decoded_data=TRUE,
		Window *window = NULL, URLContentType override_content_type = URL_UNDETERMINED_CONTENT,
		unsigned short override_charset_id=0, BOOL get_original_content=FALSE, unsigned short parent_charset_id = 0);


	/** Close the document */
	void				WriteDocumentDataFinished();

	/**	Write the specified formatted string and parameters to the document of this URL
	 *
	 *	@param	format		sprintf type format string
	 *	@param	...			Parameters specified by the format string
	 */
	OP_STATUS			WriteDocumentDataUniSprintf(const uni_char *format, ...);

	/** Write a binary file with the given content type into this dodcument
	 *
	 *  NOTE: This function is only compiled in for some platforms
	 *
	 *	@param	ctype	Contenttype to set
	 *	@param	data	pointer to the binary data
	 *	@param	len		length of data string (in characters)
	 *	@return OP_STATUS
	 */
	OP_STATUS			WriteDocumentImage(const char * ctype, const char * data, int size);


	/** Write the specified buffer to the document of this URL
	 *	If specified the source is XML- or HTMLified first
	 *
	 *	@param	conversion	How to preprocess the input (no action/HTMify, XMLify)
	 *	@param	data	pointer to the data NULL terminations are ignored
	 *	@param	len		length of data string (in characters)
	 */
	OP_STATUS			WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC &data, unsigned int len= (unsigned int) KAll);

	/** Write the specified buffer to the document of this URL
	 *	If specified the source is XML- or HTMLified first
	 *
	 *	@param	conversion	How to preprocess the input (no action/HTMify, XMLify)
	 *	@param	data	pointer to the data NULL terminations are ignored
	 *	@param	len		length of data string (in characters)
	 */
	OP_STATUS			WriteDocumentData(URL::URL_WriteDocumentConversion conversion, const OpStringC8 &data, unsigned int len= (unsigned int) KAll);

	/** Write data from the specified datadescriptor to the document of this URL.
	 *	If specified the source is XML- or HTMLified
	 *	For HMTL/XMLifying the content is assumed to the UTF-16 encoded.
	 *
	 *	@param	conversion	How to preprocess the input (no action/HTMify, XMLify)
	 *	@param	src		Datadescriptor source
	 *	@param	len		Number of bytes to read from the source, 0 means read everything that is available.
	 */
	OP_STATUS			WriteDocumentData(URL::URL_WriteDocumentConversion conversion, URL_DataDescriptor *data, unsigned int len= (unsigned int) KAll);

	/** Signal to the document that there are more data ready */
	void				WriteDocumentDataSignalDataReady();


#ifdef URL_ENABLE_ASSOCIATED_FILES
	/**
	 * Create an empty file for writing,
	 * erases contents of the file if it exists,
	 * caller must close and delete the file
	 * @return NULL on error
	 */
	OpFile *CreateAssociatedFile(AssociatedFileType type);

	/**
	 * Open an existing associated file for reading,
	 * caller must close and delete the file,
	 * it is not possible to open the file for reading while it is opened for writing
	 * @return NULL if the file doesn't exist or on error
	 */
	OpFile *OpenAssociatedFile(AssociatedFileType type);
#endif

	/** Get the Unicodeversion of the fragement identifier*/
	const uni_char*		UniRelName() const;

	/** Get the 8-bit string version of the fragement identifier */
	const char*			RelName() const;

	/** Get the ID of this URL (the pointer of the rep object)
	 *
	 *	@return URL_ID a unique identifier for this URL
	 */
	URL_ID					Id(/* follow_ref == FALSE */) const
#if !defined URL_USE_UNIQUE_ID
				{ return ((URL_ID) (UINTPTR) rep); } // This is also done in URL_Rep::GetID()
#endif
				;

	/** This URL was accessed at this time, update the information
	 *
	 *	@param	set_visited	Mark the URL as visited.
	 */
	void				Access(BOOL set_visited);

	/** What is the context ID of the URL */
	URL_CONTEXT_ID GetContextId() const;

	/** Has this URL expired according to the current cache policies?
	 *
	 *	@param inline_load			Is this URL to be used as an inline element?
	 *	@param user_setting_only	Are we going to only use the user settings, and ignore the cache policies set by the server?
	 *
	 *	@return		BOOL	TRUE if the document should be validated with the server before displaying
	 */
	BOOL				Expired(BOOL inline_load, BOOL user_setting_only = FALSE) ;

	/** Has this URL expired according to the current cache policies?
	 *
	 *	@param	inline_load			Is this URL to be used as an inline element?
	 *	@param	user_setting_only	Are we going to only use the user settings, and ignore the cache policies set by the server?
	 *	@param	maxstale			The maximum staleness accepted for the resource, before expiring it
	 *	@param	maxage				The maximum age accepted for the resource, before expiring it
	 *
	 *	@return		BOOL	TRUE if the document should be validated with the server before displaying
	 */
	BOOL				Expired(BOOL inline_load, BOOL user_setting_only, INT32 maxstale, INT32 maxage);

	/** Used by Same Server check. If this is updated, the corresponding enum in op_url.h must also be updated. */
	enum Server_Check {
		/** Don't check port */
		KNoPort,
		/** Check port */
		KCheckPort,
		/** Check resolved port */
		KCheckResolvedPort
	};
	/** Has this URL a Servername and is it the same server
	 *	(and optionally, port or resolved port) as the other URL?
	 *
	 *	@param	other			The other URL
	 *	@param	include_port	Also check if it is the same port?
	 *
	 *	@return		TRUE if there is a match.
	 */
	BOOL				SameServer(const URL &other, Server_Check include_port=KNoPort) const;

protected:
	/** Increment Used count of this URL to block automatic unloading */
	void				IncUsed(int i = 1) ;

	/** Decrement Used count of this URL to unblock automatic unloading */
	void				DecUsed(int i = 1) ;

#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
	/** Increment Loading and Used count of this URL to block (re)loading and unloading
	 *
	 *	@return		OP_STATUS	Will fail in case of OOM, and error is returned
	 */
	OP_STATUS			IncLoading();

	/** Decrement Loading and Used count of this URL to unblock (re)loading and unloading */
	void			DecLoading();
#endif


public:
	/** Set HTTP POST form data (string)
	 *
	 *	@param	data			NULL terminated string
	 *	@param	new_data		Is this new data?
	 *	@param	with_headers	Does this data contain a valid HTTP header?
	 *	@return OP_STATUS
	 */
	OP_STATUS			SetHTTP_Data(const char* data, BOOL new_data, BOOL with_headers=FALSE);

#ifdef HAS_SET_HTTP_DATA
	/** Set HTTP POST form data (upload)
	 *
	 *	@param	data			Upload element, PrepareUploadL must have been called
	 *	@param	new_data		Is this new data?
	 *	@return OP_STATUS
	 */
	void				SetHTTP_Data(Upload_Base* data, BOOL new_data) ;
#endif

#ifdef HAS_SET_HTTP_DATA
	/** Set Mail data (upload)
	 *
	 *	@param	data			Upload element, PrepareUploadL must have been called
	 *	@param	new_data		Is this new data?
	 *	@return OP_STATUS
	 */
	void				SetMailData(Upload_Base* data) ;
#endif

#ifdef _EMBEDDED_BYTEMOBILE
	void				SetPredicted(BOOL val, int depth);
#endif // _EMBEDDED_BYTEMOBILE


	/** Prefetch address from DNS
	 *
	 *	@return		BOOL	TRUE if a resolve request were sent
	 */
	BOOL				PrefetchDNS();

	/** Stop loading this URL for the given message handler
	 *
	 *	NOTE: No messages are posted to the messagehandler(s) being stopped, The documents owning
	 *	them must keep themselves updated separately
	 *
	 *	@param	mh	The message handler that will no longer be updated about the progress
	 *				of this document's loading. If NULL or it is the only messagehandler
	 *				registered with the URL loading will be terminated.
	 */
	void				StopLoading(MessageHandler* mh) ;

#ifdef DYNAMIC_PROXY_UPDATE
	/** Stop loading the document then immediately restart it */
	void				StopAndRestartLoading();
#endif // DYNAMIC_PROXY_UPDATE

	/** Returns the amount of data already downloaded for this URL item */
	OpFileLength		GetContentLoaded() const ;

	/** Write the document to disk, if necessary by force
	 *
	 *	@param force_file	Force the creation of a cache element
	 *	@return		BOOL	TRUE if the operation succeded
	 */
	BOOL				DumpSourceToDisk(BOOL force_file = FALSE) ;

	/** Prepare the document for viewing in an external application, stripping content encodings
	 *	If specified get it for the URL at the end of the redirection chain.
	 *	This operation replaces the current cache file
	 *
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 *	@param	get_raw_data	If TRUE, don't use character set conversion of the data (if relevant)
	 *	@param	get_decoded_data	If TRUE, decode the data using the content-encoding specification (usually decompression)
	 *	@param	force_to_file	The caller requires a file to be created, used to cause a flush to disk if memory only cache is used
	 *
	 *	@return long	An error code, 0 if the operation succeeded
	 */
	unsigned long		PrepareForViewing(URL::URL_Redirect follow_ref,BOOL get_raw_char_data = TRUE,BOOL get_decoded_data = TRUE, BOOL force_to_file=FALSE);
	unsigned long		PrepareForViewing(BOOL follow_ref,BOOL get_raw_char_data = TRUE,BOOL get_decoded_data = TRUE, BOOL force_to_file=FALSE);

	/** Free unused resources */
	void				FreeUnusedResources() ;

	/** Unload the data contained in the URL */
	void				Unload() ;

	/** Get the first messagehandler used to load this document */
	MessageHandler*		GetFirstMessageHandler() ;

	/** Replace the current messagehandler old_mh with new_mh, without interrupting the download
	 *	Used to transfer control of a document between windows
	 *
	 *	@param	old_mh	The message handler currently receiving pregress updates for the document
	 *	@param	new_mh	The message handler that will from now on receive pregress updates for the document
	 */
	void				ChangeMessageHandler(MessageHandler* old_mh, MessageHandler* new_mh) ;

	/** Save the document as the named file, does not destroy the cache element, but it can change behaviour.
	 *	Does not remove encoding. PrepareForViewing() is called internally.
	 *
	 *	@param	file_name	Location to save the document
	 *	@return unsigned long	error code,0 if successful
	 */
	unsigned long 		SaveAsFile(const OpStringC &file_name) ;

	/** Save the document as the named file. Does not remove encoding. The cache object is unchanged (it uses AccesReadOnly()).
	 *  This is the method of choice for Multimedia Cache, but it enable only complete export (so if something is missing, an error is retrieved)
	 *
	 *	@param	file_name	Location to save the document
	 *	@return unsigned long	error code,0 if successful
	 */
	OP_STATUS ExportAsFile(const OpStringC &file_name) ;

	/** Checks if ExportAsFile() is allowed (it means than the file is completely available,
	    and it is particularly important for the Multimedia cache)

		@return TRUE if ExportAsFile() can be called
	*/
	BOOL IsExportAllowed();

#ifdef PI_ASYNC_FILE_OP
	/** Asynchronously save the document as the named file. Does not decode the content. The cache object is unchanged (it uses AccesReadOnly()).
	 *  This is the method of choice for Multimedia Cache, but it enable only complete export (so if something is missing, an error is retrieved).
	 *  Note that in case of Unsupported operation, URL::SaveAsFile() will be attempted, if requested.
	 *
	 *	@param file_name File where to save the document
	 *	@param listener Listener that will be notified of the progress (it cannot be NULL)
	 *	@param param optional user parameter that will be passed to the listener
	 *	@param delete_if_error TRUE to delete the target file in case of error
	 *  @param safe_fall_back If TRUE, in case of problems, URL::SaveAsFile() will be attempted.
	 *                        This should really be TRUE, or containers and embedded files will fail... and encoded files... and more...
	 *	@return In case of OpStatus::ERR_NOT_SUPPORTED, URL::SaveAsFile() can be attempted.
	 */
	OP_STATUS ExportAsFileAsync(const OpStringC &file_name, AsyncExporterListener *listener, UINT32 param=0, BOOL delete_if_error=TRUE, BOOL safe_fall_back=TRUE);
#endif // PI_ASYNC_FILE_OP

	/** Save the document as a download transfer entry in the named file, replaces the current cache element
	 *
	 *
	 *	@param	file_name	Location to save the document
	 *	@return unsigned long	error code,0 if successful
	 */
	OP_STATUS			LoadToFile(const OpStringC &file_name) ;

	/**	Determine this URLs thirdparty status, based on the provided referrer URL
	 *	If the return value is FALSE, then an asynch ServerName::GetDomainTypeASync operation
	 *	has started, and caller have to follow those procedures.
	 */
	BOOL				DetermineThirdParty(URL &referrer);

#ifdef _MIME_SUPPORT_
	/** Retrive the number i attachment URL
	 *
	 *	@param	i	Attachment number to retrieve
	 *	@param  url	Target URL in which to store the URL of the attachment
	 *	@return BOOL	TRUE if we found a URL
	 */
	BOOL			GetAttachment(int i, URL &url);

	/** @return TRUE if the content type is an MHTML archive */
	BOOL			IsMHTML() const;

	/** @return the URL of the root attachment pointed to by the "start" parameter of an MHTML archive */
	URL				GetMHTMLRootPart();
#endif

	/** Copy all the HTTP headers that was received when this URL was retrieved
	 *
	 *	@param	header_list_copy	A copy of the headers is stored in this object
	 */
	void			CopyAllHeadersL(HeaderList& header_list_copy) const;


#ifdef IMODE_EXTENSIONS
	OP_STATUS SetUseUtn(BOOL value){ return SetAttribute(URL::KSetUseUtn, value); }
	BOOL GetUseUtn(){ return !!GetAttribute(URL::KSetUseUtn); }
#endif //IMODE_EXTENSIONS


	/** Returns the type (scheme) of the URL.  Same as (URLType) GetAttribute(URL::KType), but with added type-safety. */
	URLType			Type() const;

	/** Returns the current load status of this URL.  Same as (URLStatus) GetAttribute(URL::KLoadStatus, is_followed), but with added type-safety. */
	URLStatus		Status(BOOL is_followed)const;
	URLStatus		Status(URL::URL_Redirect is_followed)const;

	/** Returns the type of content in the URL.  Same as (URLContentType) GetAttribute(URL::KContentType,TRUE), but with added type-safety. */
	URLContentType	ContentType() const;

	/** Returns the number of bytes (expected to be) loaded.  Same as GetAttribute(URL::KContentSize), but with added type-safety. */
	OpFileLength	GetContentSize() const;

	/** Returns the number of bytes loaded from the server.  Same as GetAttribute(URL::KContentLoaded), but with added type-safety. */
	OpFileLength	ContentLoaded() const;


	/** Get the unsigend integer value of the identified attribute.
	 *	If specified get it for the URL at the end of the redirection chain.
	 *
	 *	NOTE: Some return values must be typecasted to be useful, the enum specification documents this
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 *
	 *	@return uint32		The unsigned integer value of the requested attribute
	 */
	uint32 GetAttribute(URL::URL_Uint32Attribute attr, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	uint32 GetAttribute(URL::URL_Uint32Attribute attr, BOOL follow_ref) const;

	/** Get the string value of the identified attribute.
	 *	If specified get it for the URL at the end of the redirection chain.
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 *
	 *	@return OpStringC8		The string value of the requested attribute
	 */
	const OpStringC8 GetAttribute(URL::URL_StringAttribute attr, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	const OpStringC8 GetAttribute(URL::URL_StringAttribute attr, BOOL follow_ref) const;

	/** Get the string value of the identified attribute.
	 *	If specified get it for the URL at the end of the redirection chain.
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 *
	 *	@return OpStringC		The string value of the requested attribute
	 */
	const OpStringC GetAttribute(URL::URL_UniStringAttribute attr, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	const OpStringC GetAttribute(URL::URL_UniStringAttribute attr, BOOL follow_ref) const;

	/** Get the string value of the identified attribute and store it in the input target string object
	 *	If specified get it for the URL at the end of the redirection chain.
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	val				The resulting string will be stored here
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 */
	OP_STATUS		GetAttribute(URL::URL_StringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	OP_STATUS		GetAttribute(URL::URL_StringAttribute attr, OpString &val, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	void			GetAttributeL(URL::URL_StringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	void			GetAttributeL(URL::URL_StringAttribute attr, OpString8 &val, BOOL follow_ref) const;
	OP_STATUS		GetAttribute(URL::URL_StringAttribute attr, OpString8 &val, BOOL follow_ref) const;

	/** Get the string value of the identified attribute and store it in the input target string object
	 *	If specified get it for the URL at the end of the redirection chain.
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	val				The resulting string will be stored here
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 */
	void			GetAttributeL(URL::URL_UniStringAttribute attr, OpString &val, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	OP_STATUS		GetAttribute(URL::URL_UniStringAttribute attr, OpString &val, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	void			GetAttributeL(URL::URL_UniStringAttribute attr, OpString &val, BOOL follow_ref) const;
	OP_STATUS		GetAttribute(URL::URL_UniStringAttribute attr, OpString &val, BOOL follow_ref) const;

	/**
	 *	@deprecated   Use GetAttribute(attr, string_ret, follow_ref) instead;
	 *
	 *  Get the string value of the identified Name attribute.
	 *	If specified get it for the URL at the end of the redirection chain.
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 *
	 *	@return OpStringC8		The string value of the requested attribute
	 */

	DEPRECATED(const OpStringC8 GetAttribute(URL::URL_NameStringAttribute attr, URL::URL_Redirect follow_ref = URL::KNoRedirect) const);
	DEPRECATED(const OpStringC8 GetAttribute(URL::URL_NameStringAttribute attr, BOOL follow_ref) const);

	/**
	 *	@deprecated   Use GetAttribute(attr, charset, string_ret, follow_ref) instead;
	 *		In url_6 attr may be replaced with the corresponding URL::KName* value and
	 *		GetAttribute(attr, string_ret, follow_ref, charset) used instead
	 *
	 *  Get the string value of the identified name attribute.
	 *	If specified get it for the URL at the end of the redirection chain.
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 *
	 *	@return OpStringC		The string value of the requested attribute
	 */
	DEPRECATED(const OpStringC GetAttribute(URL::URL_UniNameStringAttribute attr, URL::URL_Redirect follow_ref = URL::KNoRedirect) const);
	DEPRECATED(const OpStringC GetAttribute(URL::URL_UniNameStringAttribute attr, BOOL follow_ref) const);

	/** Get the string value of the identified name attribute and store it in the input target string object
	 *	If specified get it for the URL at the end of the redirection chain.
	 *
	 *	BOOL redirect variants are deprecated.
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	val				The resulting string will be stored here
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 */
	void			GetAttributeL(URL::URL_NameStringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	OP_STATUS		GetAttribute(URL::URL_NameStringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	void			GetAttributeL(URL::URL_NameStringAttribute attr, OpString8 &val, BOOL follow_ref) const;
	OP_STATUS		GetAttribute(URL::URL_NameStringAttribute attr, OpString8 &val, BOOL follow_ref) const;
	OP_STATUS		GetAttribute(URL::URL_NameStringAttribute attr, OpString &val,URL::URL_Redirect follow_ref = URL::KNoRedirect,  unsigned short charsetID = 0) const;


	/** Get the string value of the identified name attribute and store it in the input target string object
	 *	If specified get it for the URL at the end of the redirection chain.
	 *
	 *	Non-charset versions are deprecated.
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	val				The resulting string will be stored here
	 *	@param	charsetID		The charset ID of the charset to be used when encoding the _unescaped_ version of the URL. Use 0 if the result is irrelevant, that will use UTF-8
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 */
	void			GetAttributeL(URL::URL_UniNameStringAttribute attr, unsigned short charsetID, OpString &val, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	OP_STATUS		GetAttribute(URL::URL_UniNameStringAttribute attr, unsigned short charsetID, OpString &val, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	void			GetAttributeL(URL::URL_UniNameStringAttribute attr, OpString &val, BOOL follow_ref=FALSE) const;
	OP_STATUS		GetAttribute(URL::URL_UniNameStringAttribute attr, OpString &val, BOOL follow_ref=FALSE) const;

	/** Return a Display version of the URL in val
	 *	@param	val				The resulting string will be stored here
	 *	@param	charsetID		The charset ID of the charset to be used when encoding the _unescaped_ version of the URL. Use 0 if the result is irrelevant, that will use UTF-8
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 *	@return OP_STATUS
	 */
	OP_STATUS		GetURLDisplayName(OpString &val, URL::URL_Redirect follow_ref = URL::KNoRedirect, unsigned short charsetID=0){return GetAttribute( URL::KUniName_With_Fragment_Username, charsetID, val, follow_ref);}
	OP_STATUS		GetURLDisplayName(OpString8 &val, URL::URL_Redirect follow_ref = URL::KNoRedirect){return GetAttribute( URL::KName_With_Fragment_Username, val, follow_ref);}

	/** Get the void pointer value of the identified attribute and store it in the input target string object
	 *	If specified get it for the URL at the end of the redirection chain.
	 *
	 *	NOTE: The returned values must be typecasted to be useful, the enum specification documents which types are proper
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 *	@param	param			Location where some attributes that must be copied into a location can be stored. NOTE: This parameter is only used to retrive selected attributes
	 *	@return void *			Pointer to object, when appropriate, otherwise NULL.
	 */
	const void *GetAttribute(URL::URL_VoidPAttribute  attr, const void *param, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	const void *GetAttribute(URL::URL_VoidPAttribute  attr, const void *param, BOOL follow_ref) const;

	/** Get the URL of the identified attribute
	 *	If specified get it for the URL at the end of the redirection chain.
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	follow_ref		If URL::KFollowRedirect return a descriptor for the URL at the end of the redirection chain, if there is a redirection chain,
	 *							Otherwise perform it on this URL.
	 *	@return URL		The URL stored in the given attribute
	 */
	URL GetAttribute(URL_URLAttribute  attr, URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	URL GetAttribute(URL_URLAttribute  attr, BOOL follow_ref) const;

	/** Set the value of the identified attribute using the provided integer value
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	value			The input value used to update the attribute
	 */
	void SetAttributeL(URL::URL_Uint32Attribute attr, uint32 value);

	/** Set the string value of the identified attribute using the provided string
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	string			The input string used to update the attribute
	 */
	void SetAttributeL(URL::URL_StringAttribute attr, const OpStringC8 &string);

	/** Set the string value of the identified attribute using the provided string
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	string			The input string used to update the attribute
	 */
	void SetAttributeL(URL::URL_UniStringAttribute attr, const OpStringC &string);

	/** Set the value of the identified attribute using the provided void pointer
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	param			The input void pointer used to update the attribute
	 */
	void SetAttributeL(URL::URL_VoidPAttribute  attr, const void *param);

	/** Set the URL value of the identified attribute using the provided URL
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	param			The URL used to update the attribute
	 */
	void SetAttributeL(URL::URL_URLAttribute  attr, const URL &param);

	/** Set the value of the identified attribute using the provided integer value
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	value			The input value used to update the attribute
	 *	@return OP_STATUS
	 */
	OP_STATUS SetAttribute(URL::URL_Uint32Attribute attr, uint32 value);

	/** Set the string value of the identified attribute using the provided string
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	string			The input string used to update the attribute
	 *	@return OP_STATUS
	 */
	OP_STATUS SetAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string);

	/** Set the string value of the identified attribute using the provided string
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	string			The input string used to update the attribute
	 *	@return OP_STATUS
	 */
	OP_STATUS SetAttribute(URL::URL_UniStringAttribute attr, const OpStringC &string);

	/** Set the value of the identified attribute using the provided void pointer
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	param			The input void pointer used to update the attribute
	 *	@return OP_STATUS
	 */
	OP_STATUS SetAttribute(URL::URL_VoidPAttribute  attr, const void *param);

	/** Set the URL value of the identified attribute using the provided URL
	 *
	 *	@param	attr			The identifier for the requested attribute
	 *	@param	param			The URL used to update the attribute
	 *	@return OP_STATUS
	 */
	OP_STATUS SetAttribute(URL::URL_URLAttribute  attr, const URL &param);

	/**	Get a sorted vector of the available byte ranges.
	 *
	 *	@param segments An empty vector that will be populated with
	 *	the available ranges, sorted by the start byte in each range.
	 */
	OP_STATUS GetSortedCoverage(OpAutoVector<StorageSegment> &segments);

	/**	If the requested byte is in the segment, this method says how many bytes are available, else it returns
	 *	the number of bytes NOT available (and that needs to be downloaded before reaching a segment).
	 *	This is bad behaviour has been chosen to avoid traversing the segments chain two times.
     *
	 *	So:
	 *	- available==TRUE  ==> length is the number of bytes available on the current segment
	 *	- available==FALSE ==> length is the number of bytes to skip or download before something is available.
	 *						   if no other segments are available, length will be 0 (this is because the Multimedia
	 *						   cache does not know the length of the file)
	 *
	 *	@param position Start of the range requested
	 *	@param available TRUE if the bytes are available, FALSE if they need to be skipped
	 *	@param length bytes available / to skip
	 *	@param multiple_segments TRUE if it is ok to merge different segments to get a bigger coverage (this operation is relatively slow)
	 */
	void GetPartialCoverage(OpFileLength position, BOOL &available, OpFileLength &length, BOOL multiple_segments);

	/**
	 *	This method is intended to be used with the Multimedia cache, in particular to know when it's safe to save the file for an external application.
	 *	Return the next segment required to completely download the URL.
	 *	This method should be called till when length become 0, which means that the URL has been fully downloaded.
	 *	@param start Will contain the starting byte of the next segment to download
	 *	@param len Length of the segment to download; FILE_LENGTH_NONE means URL fully downloaded
	 */
	OP_STATUS GetNextMissingCoverage(OpFileLength &start, OpFileLength &len);

	/** Check if the streaming multimedia cache is or will be used.
	 *
	 * Note that it is not always possible to know before loading has
	 * begun whether or not the streaming cache will be used, in
	 * particular the result may be different after HTTP Cache-Control
	 * headers have been received.
	 */
	BOOL UseStreamingCache();

public:
	/**
	  * Returns the corresponding URL_Rep structure which
	  * contains the physical document data for this
	  * URL instance.
	  */
	URL_Rep *GetRep() const;

public:

	/** @deprecated
	 *	Use GetAttribute(URL::KIsFollowed) instead */
	BOOL				DEPRECATED(IsFollowed() const);

	/** @deprecated
	 *	Use GetAttribute(URL::KHTTP_Response_Code) instead */
	short				DEPRECATED(GetHttpResponse() const);

	/** @deprecated Use  SetAttributeL(URL::KLoadStatus, status) instead */
	void			ForceStatus(URLStatus status);

	/** @deprecated Use  SetAttributeL(URL::KMIME_ForceContentType, mtype) instead */
	//OP_STATUS		DEPRECATED(MIME_ForceContentType(const OpStringC8 &mtype));
	/** @deprecated  Use (BOOL) GetAttribute(URL::KIsImage) instead */
	BOOL			IsImage() const;
	/** @deprecated  Use SetAttributeL(URL::KHTTP_Method, method)  instead */
	void			SetHTTP_Method(HTTP_Method method);
	/** @deprecated  Use  SetAttributeL_RET(URL::KMailDataStr, str) instead */
	OP_STATUS		SetMailData(const char* str);
	/** @deprecated  Use SetAttributeL_RET(URL::KHTTP_ContentType, ct)  instead */
	OP_STATUS		SetHTTP_ContentType(const char* ct);
	/** @deprecated  No longer supported */
	OP_STATUS		SetMailTo(const char* str); // inlined below
	/** @deprecated  Use  SetAttributeL_RET(URL::KMailSubject, str) instead */
	OP_STATUS		SetMailSubject(const char* str);
	/** @deprecated  Use  (ServerName *) GetAttribute(URL::KServerName) instead */
	ServerName *	GetServerName(URL::URL_Redirect follow_ref = URL::KNoRedirect) const;
	/** @deprecated  Use  GetAttribute(URL::KServerPort) instead */
	unsigned short	GetServerPort() const;
	/** @deprecated  Use URL_ID id=0; GetAttribute(URL::K_ID, follow_ref, &id); instead */
	URL_ID Id(BOOL follow_ref) const;
	URL_ID Id(URL::URL_Redirect follow_ref) const;
};

#include "modules/url/url_rep.h"
#include "modules/url/url_rel.h"

/* gcc 3 but < 3.4 can't handle deprecated inline; so separate DEPRECATED
 * declarations from inline definitions.
 */

#ifndef RAMCACHE_ONLY
inline void			URL::WriteCacheDataL(DataFile_Record *rec) { rep->WriteCacheDataL(rec); }
#endif
inline CommState	URL::Load(MessageHandler* mh, const URL& referer_url, BOOL inline_loading, BOOL load_silent, BOOL user_initiated, BOOL thirdparty_determined)
	{return rep->Load(mh, referer_url, user_initiated, thirdparty_determined, inline_loading);}
inline CommState	URL::Reload(MessageHandler* mh, const URL& referer_url, BOOL only_if_modified, BOOL proxy_no_cache, BOOL inline_loading, BOOL check_modified_silent, BOOL load_silent, BOOL user_initiated, BOOL thirdparty_determined, EnteredByUser entered_by_user, BOOL* allow_if_modified)
	{return rep->Reload(mh, referer_url, only_if_modified, proxy_no_cache, user_initiated, thirdparty_determined, entered_by_user, inline_loading, allow_if_modified);}
inline	CommState	URL::ResumeLoad(MessageHandler* mh, const URL& referer_url, BOOL inline_loading, BOOL check_modified_silent, BOOL load_silent)
	{return rep->ResumeLoad(mh, referer_url);}
inline BOOL URL::QuickLoad(BOOL guess_content_type){return rep->QuickLoad(guess_content_type);}
inline URL_DataDescriptor*	URL::GetDescriptor(MessageHandler *mh, URL::URL_Redirect follow_ref,
									   BOOL get_raw_data, BOOL get_decoded_data,
									   Window *window, URLContentType override_content_type,
									   unsigned short override_charset_id,
									   BOOL get_original_content, unsigned short parent_charset_id)
{
#ifdef APPLICATION_CACHE_SUPPORT

	URL fallback_url = GetAttribute(URL::KFallbackURL, FALSE);

	if (fallback_url.IsValid() && fallback_url.Status(FALSE) == URL_LOADED)
	{
		/* return a fallback url if this was an application cache url with fallback, and the loading from network failed */
		return fallback_url.GetRep()->GetDescriptor(mh,follow_ref,get_raw_data, get_decoded_data, window,
		                        		override_content_type, override_charset_id, get_original_content, parent_charset_id);
	}
#endif // APPLICATION_CACHE_SUPPORT

	return rep->GetDescriptor(mh,follow_ref,get_raw_data, get_decoded_data, window,
					override_content_type, override_charset_id, get_original_content, parent_charset_id);
}
inline URL_DataDescriptor*	URL::GetDescriptor(MessageHandler *mh, BOOL follow_ref,
										BOOL get_raw_data, BOOL get_decoded_data,
										Window *window, URLContentType override_content_type,
										unsigned short override_charset_id,
										BOOL get_original_content,
										unsigned short parent_charset_id)
	{return GetDescriptor(mh,(follow_ref ? URL::KFollowRedirect : URL::KNoRedirect), get_raw_data,get_decoded_data,window,
					override_content_type,override_charset_id,get_original_content,parent_charset_id);}
inline const uni_char* URL::UniRelName() const {return rel_rep->UniName().CStr();}
inline const char*	URL::RelName() const {return rel_rep->name.CStr();}
inline URL_CONTEXT_ID URL::GetContextId() const {return rep->GetContextId();}
inline BOOL			URL::Expired(BOOL inline_load, BOOL user_setting_only) { return rep->Expired(inline_load, user_setting_only); }
#ifdef NEED_URL_EXPIRED_WITH_STALE
inline BOOL			URL::Expired(BOOL inline_load, BOOL user_setting_only, INT32 maxstale, INT32 maxage) { return rep->Expired(inline_load, user_setting_only, maxstale, maxage); }
#endif
inline void			URL::IncUsed(int i) { rep->IncUsed(i); }
inline void			URL::DecUsed(int i) { rep->DecUsed(i); }
#ifdef URL_ACTIVATE_URL_LOAD_RESERVATION_OBJECT
inline OP_STATUS	URL::IncLoading(){return rep->IncLoading(); }
inline void			URL::DecLoading(){rep->DecLoading(); }
#endif
inline OP_STATUS	URL::SetHTTP_Data(const char* data, BOOL new_data, BOOL with_headers) { return rep->SetHTTP_Data(data, new_data, with_headers); }
#ifdef HAS_SET_HTTP_DATA
inline void			URL::SetHTTP_Data(Upload_Base* data, BOOL new_data) { rep->SetHTTP_Data(data, new_data); }
inline void			URL::SetMailData(Upload_Base* data) { rep->SetMailData(data); }
#endif
inline BOOL			URL::PrefetchDNS() { return rep->PrefetchDNS(); }
inline void			URL::StopLoading(MessageHandler* mh) { rep->StopLoading(mh); }
#if defined(DYNAMIC_PROXY_UPDATE)
inline void			URL::StopAndRestartLoading(){rep->StopAndRestartLoading();}
#endif // DYNAMIC_PROXY_UPDATE
inline OpFileLength URL::GetContentLoaded() const {OpFileLength registered_len=0; GetAttribute(URL::KContentLoaded, &registered_len); return registered_len;}
inline BOOL			URL::DumpSourceToDisk(BOOL force_file) {return rep->DumpSourceToDisk(force_file); }
inline unsigned long	URL::PrepareForViewing(URL::URL_Redirect follow_ref,BOOL get_raw_char_data,BOOL get_decoded_data, BOOL force_to_file){return rep->PrepareForViewing(follow_ref, get_raw_char_data,get_decoded_data, force_to_file);}
inline unsigned long	URL::PrepareForViewing(BOOL follow_ref,BOOL get_raw_char_data,BOOL get_decoded_data, BOOL force_to_file)
	{return PrepareForViewing((follow_ref ? URL::KFollowRedirect : URL::KNoRedirect),get_raw_char_data, get_decoded_data, force_to_file);}
inline void				URL::FreeUnusedResources() {rep->FreeUnusedResources();}
inline void				URL::Unload() { rep->Unload(); }
inline MessageHandler*	URL::GetFirstMessageHandler() { return rep->GetFirstMessageHandler(); }
inline void				URL::ChangeMessageHandler(MessageHandler* old_mh, MessageHandler* new_mh) { rep->ChangeMessageHandler(old_mh, new_mh); }
inline unsigned long	URL::SaveAsFile(const OpStringC &file_name) { return rep->SaveAsFile(file_name); }
inline OP_STATUS		URL::ExportAsFile(const OpStringC &file_name) { return rep->ExportAsFile(file_name); }
inline BOOL				URL::IsExportAllowed() { return rep->IsExportAllowed(); }
#ifdef PI_ASYNC_FILE_OP
	inline OP_STATUS		URL::ExportAsFileAsync(const OpStringC &file_name, AsyncExporterListener *listener, UINT32 param, BOOL delete_if_error, BOOL safe_fall_back)
	{ return rep->ExportAsFileAsync(file_name, listener, param, delete_if_error, safe_fall_back); }
#endif // PI_ASYNC_FILE_OP

inline OP_STATUS		URL::LoadToFile(const OpStringC &file_name) { return rep->LoadToFile(file_name); }
inline BOOL				URL::DetermineThirdParty(URL &referrer){ return rep->DetermineThirdParty(referrer);}
#ifdef _MIME_SUPPORT_
inline BOOL				URL::GetAttachment(int i, URL &url){return rep->GetAttachment(i,url);}
inline BOOL				URL::IsMHTML() const {return rep->IsMHTML();}
inline URL				URL::GetMHTMLRootPart(){return rep->GetMHTMLRootPart();}
#endif
inline void				URL::CopyAllHeadersL(HeaderList& header_list_copy) const { rep->CopyAllHeadersL(header_list_copy); }
inline URLType			URL::Type() const { return (URLType) GetAttribute(URL::KType); }
inline URLStatus		URL::Status(BOOL is_followed) const {return (URLStatus) GetAttribute(URL::KLoadStatus, (is_followed ?  URL::KFollowRedirect :  URL::KNoRedirect));}
inline URLStatus		URL::Status(URL::URL_Redirect is_followed) const {return (URLStatus) GetAttribute(URL::KLoadStatus, is_followed);}
inline URLContentType	URL::ContentType() const {return (URLContentType) GetAttribute(URL::KContentType, URL::KFollowRedirect);}
inline OpFileLength		URL::GetContentSize() const { OpFileLength registered_len=0; GetAttribute(URL::KContentSize, &registered_len); return registered_len; }
inline OpFileLength		URL::ContentLoaded() const {  OpFileLength registered_len=0; GetAttribute(URL::KContentLoaded, &registered_len);  return registered_len;}

inline uint32			URL::GetAttribute(URL::URL_Uint32Attribute attr, BOOL follow_ref) const{return GetAttribute(attr, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));}
inline const OpStringC8	URL::GetAttribute(URL::URL_StringAttribute attr, URL::URL_Redirect follow_ref) const{OpStringC8 ret(rep->GetAttribute(attr, follow_ref, rel_rep)); return ret;}
inline const OpStringC8 URL::GetAttribute(URL::URL_StringAttribute attr, BOOL follow_ref) const {
	const OpStringC8 ans(GetAttribute(attr, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect)));
	return ans; // ADS1.2 crashes if you don't go via an intermediary :-(
}
inline const OpStringC	URL::GetAttribute(URL::URL_UniStringAttribute attr, URL::URL_Redirect follow_ref) const{OpStringC ret(rep->GetAttribute(attr, follow_ref, rel_rep)); return ret;}
inline const OpStringC	URL::GetAttribute(URL::URL_UniStringAttribute attr, BOOL follow_ref) const {
	const OpStringC ans(GetAttribute(attr, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect)));
	return ans; // ADS1.2 crashes if you don't go via an intermediary :-(
}
inline OP_STATUS		URL::GetAttribute(URL::URL_StringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref) const{return rep->GetAttribute(attr, val, follow_ref, rel_rep);}
inline OP_STATUS		URL::GetAttribute(URL::URL_StringAttribute attr, OpString &val, URL::URL_Redirect follow_ref) const{return rep->GetAttribute(attr, val, follow_ref, rel_rep);}
inline void				URL::GetAttributeL(URL::URL_StringAttribute attr, OpString8 &val, BOOL follow_ref) const{GetAttributeL(attr, val, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));}
inline OP_STATUS		URL::GetAttribute(URL::URL_StringAttribute attr, OpString8 &val, BOOL follow_ref) const{return GetAttribute(attr, val, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));}
inline OP_STATUS		URL::GetAttribute(URL::URL_UniStringAttribute attr, OpString &val, URL::URL_Redirect follow_ref) const{return rep->GetAttribute(attr, val, follow_ref, rel_rep);}
inline void				URL::GetAttributeL(URL::URL_UniStringAttribute attr, OpString &val, BOOL follow_ref) const{GetAttributeL(attr, val, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));}
inline OP_STATUS		URL::GetAttribute(URL::URL_UniStringAttribute attr, OpString &val, BOOL follow_ref) const{return GetAttribute(attr, val, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));}
inline const OpStringC8	URL::GetAttribute(URL::URL_NameStringAttribute attr, URL::URL_Redirect follow_ref) const{OpStringC8 ret(rep->GetAttribute(attr, follow_ref, rel_rep)); return ret;}
inline const OpStringC8 URL::GetAttribute(URL::URL_NameStringAttribute attr, BOOL follow_ref) const {
	OpStringC8 ans(rep->GetAttribute(attr, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect)));
	return ans; // ADS1.2 crashes if you don't go via an intermediary :-(
}
inline const OpStringC	URL::GetAttribute(URL::URL_UniNameStringAttribute attr, URL::URL_Redirect follow_ref) const { OpStringC ret(rep->GetAttribute(attr, follow_ref, rel_rep)); return ret; }
inline const OpStringC	URL::GetAttribute(URL::URL_UniNameStringAttribute attr, BOOL follow_ref) const {
	OpStringC ans(rep->GetAttribute(attr, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect)));
	return ans; // ADS1.2 crashes if you don't go via an intermediary :-(
}
inline OP_STATUS	URL::GetAttribute(URL::URL_NameStringAttribute attr, OpString8 &val, URL::URL_Redirect follow_ref) const{return rep->GetAttribute(attr, val, follow_ref, rel_rep);}
inline void			URL::GetAttributeL(URL::URL_NameStringAttribute attr, OpString8 &val, BOOL follow_ref) const{GetAttributeL(attr, val, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));}
inline OP_STATUS	URL::GetAttribute(URL::URL_NameStringAttribute attr, OpString8 &val, BOOL follow_ref) const{return GetAttribute(attr, val, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));}
inline OP_STATUS	URL::GetAttribute(URL::URL_NameStringAttribute attr, OpString &val,URL::URL_Redirect follow_ref,  unsigned short charsetID) const{return GetAttribute((URL::URL_UniNameStringAttribute)((int)attr), charsetID, val, follow_ref);}
inline OP_STATUS	URL::GetAttribute(URL::URL_UniNameStringAttribute attr, unsigned short charsetID, OpString &val, URL::URL_Redirect follow_ref) const{return rep->GetAttribute(attr, charsetID, val, follow_ref, rel_rep);}
inline void			URL::GetAttributeL(URL::URL_UniNameStringAttribute attr, OpString &val, BOOL follow_ref) const{GetAttributeL(attr, 0, val, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));}
inline OP_STATUS	URL::GetAttribute(URL::URL_UniNameStringAttribute attr, OpString &val, BOOL follow_ref) const{return GetAttribute(attr, 0, val, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));}
op_force_inline const void *URL::GetAttribute(URL::URL_VoidPAttribute  attr, const void *param, URL::URL_Redirect follow_ref) const{return rep->GetAttribute(attr, param, follow_ref);}
inline const void *	URL::GetAttribute(URL::URL_VoidPAttribute  attr, const void *param, BOOL follow_ref) const{return GetAttribute(attr, param, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));}
inline URL			URL::GetAttribute(URL::URL_URLAttribute  attr, URL::URL_Redirect follow_ref) const{return rep->GetAttribute(attr, follow_ref);}
inline URL			URL::GetAttribute(URL::URL_URLAttribute  attr, BOOL follow_ref) const{return GetAttribute(attr, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect));}
inline OP_STATUS	URL::SetAttribute(URL::URL_Uint32Attribute attr, uint32 value){return rep->SetAttribute(attr,value);}
inline OP_STATUS	URL::SetAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string){return rep->SetAttribute(attr,string);}
inline OP_STATUS	URL::SetAttribute(URL_UniStringAttribute attr, const OpStringC &string){return rep->SetAttribute(attr,string);}
inline OP_STATUS	URL::SetAttribute(URL::URL_VoidPAttribute  attr, const void *param){return rep->SetAttribute(attr,param);}
inline URL_Rep *	URL::GetRep() const { return rep;}
inline BOOL			URL::IsFollowed() const { return !!GetAttribute(URL::KIsFollowed); }
inline short		URL::GetHttpResponse() const { return (short) GetAttribute(URL::KHTTP_Response_Code); }
inline void			URL::ForceStatus(URLStatus status){URL_SET_ATTRIBUTE(URL::KLoadStatus, status);}
inline BOOL			URL::IsImage() const{ return !!GetAttribute(URL::KIsImage);}
inline void			URL::SetHTTP_Method(HTTP_Method method){ URL_SET_ATTRIBUTE(URL::KHTTP_Method, method); }
inline OP_STATUS	URL::SetMailData(const char* str)  { URL_SET_ATTRIBUTE_RET(URL::KMailDataStr, str); }
inline OP_STATUS	URL::SetHTTP_ContentType(const char* ct) { URL_SET_ATTRIBUTE_RET(URL::KHTTP_ContentType, ct); }
inline OP_STATUS	URL::SetMailTo(const char* str) { URL_SET_ATTRIBUTE_RET(URL::KMailTo, str); }
inline OP_STATUS	URL::SetMailSubject(const char* str) { URL_SET_ATTRIBUTE_RET(URL::KMailSubject, str); }
inline ServerName *	URL::GetServerName(URL::URL_Redirect follow_ref) const {return (ServerName *) GetAttribute(URL::KServerName, (void *) NULL, follow_ref);}
inline unsigned short	URL::GetServerPort() const {return (unsigned short) GetAttribute(URL::KServerPort);}
inline URL_ID URL::Id(BOOL follow_ref) const { URL_ID id=0; GetAttribute(URL::K_ID, &id, (follow_ref ? URL::KFollowRedirect : URL::KNoRedirect)); return id;}
inline URL_ID URL::Id(URL::URL_Redirect follow_ref) const { URL_ID id=0; GetAttribute(URL::K_ID, &id, follow_ref); return id;}

/** An object of this class prevents the document contained in the URL
 *	member from being automatically freed during cache cleanups.
 *	This is accomplished using the IncUsed/DecUsed functions of the URL class.
 *
 *  URLs may be assigned/set after construction
 *	Empty URLs are not blocked
 */
class URL_InUse
#ifdef URL_ENABLE_TRACER
	: private URL_InUse_Tracer
#endif
{
private:
	/** The url being reserved */
	URL url;

public:
	/** Default Constructor */
	URL_InUse(){}

	/** Copy Constructor */
#ifdef URL_ENABLE_TRACER
	URL_InUse(URL_InUse &old):URL_InUse_Tracer(old){SetURL(old);}
#else
	URL_InUse(URL_InUse &old){SetURL(old);}
#endif

	/** Reserves the given url
	 *
	 *	@param p_url	URL to reserve
	 */
	URL_InUse(URL &p_url){SetURL(p_url);}
	URL_InUse(const URL &p_url){SetURL(p_url);}

	/** Destructor */
	~URL_InUse();

	/** Assignment operator */
	URL_InUse &operator =(URL_InUse &old){SetURL(old); return *this;}

	/** Assignment operator
	 *
	 *	@param p_url	URL to reserve
	 */
	URL_InUse &operator =(URL &p_url){SetURL(p_url); return *this;}

	/** Reserves the same url as the other object
	 *
	 *	@param p_url	Contains URL to reserve
	 */
	void SetURL(URL_InUse &old){SetURL(old.url);};

	/** Reserves the given url
	 *
	 *	@param p_url	URL to reserve
	 */
	void SetURL(URL &);
	void SetURL(const URL &);

	/** Unreserves the current url */
	void UnsetURL();

	/** Get the current URL */
	URL& GetURL(){return url;}

	/** Access functions in the contained URL */
	URL *operator ->() {return &url;}
	const URL *operator ->() const {return &url;}

	/** Returns a reference to the contained URL. DO NOT USE in assignement operations !! */
    operator URL &(){return url;}

	/** Returns a pointer to the contained URL. DO NOT USE in assignement operations !! */
	operator URL *(){return &url;}
};

#include "modules/util/simset.h"

/** Linked List implementation for lists of URL objects
 */
class URLLink : public Link
{
public:
	/** The contained URL object, can be referenced directly */
	URL url;

public:
	/** Constructor
	 *
	 *	@param p_url	Contains URL to use
	 */
	URLLink(const URL &p_url): url(p_url) {}

	/** Copy Constructor
	 *
	 *	@param p_url_link	Pointer to the URLLink object to use
	 */
	URLLink(const URLLink *p_url_link): url(p_url_link ? p_url_link->url : URL()) {}

	/** Destructor */
	virtual ~URLLink();

	/** Updates the contained URL to point at the URL contained by p_url_link
	 *
	 *	@param p_url_link	Pointer to the URLLink object to use
	 */
	void SetURL(URLLink *p_url_link){url = (p_url_link ? p_url_link->url : URL());}

	/** Updates the contained URL to point at the URL contained by p_url_link
	 *
	 *	@param p_url	Contains URL to use
	 */
	void SetURL(URL &old){url = old;}

	/** Unreserves the current url */
	void UnsetURL(){url = URL();}

	/** Get the current URL */
	URL GetURL(){return url;}
};


#include "modules/url/url_api.h"

#include "modules/cache/url_dd.h"

#endif // _URL_2_H_
