/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#ifndef _URL_ENUM_H_
#define _URL_ENUM_H_

#include "modules/debug/debug.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif // HAVE_CONFIG_H

#define CHECK_RANGED_ENUM(type, val) OP_ASSERT((static_cast<type>(val) >= type ## _RangeStart) && (static_cast<type>(val) < type ## _RangeEnd))
#define FROM_RANGED_ENUM(type, val) ( static_cast<unsigned int>(val) - static_cast<unsigned int>(type ## _RangeStart) )
#define GET_RANGED_ENUM(type, val) ( static_cast<type>( val + (static_cast<unsigned int>(type ## _RangeStart) ) ) )

// NOTE!! CHECK THAT YOU DO NOT EXCEED VALUE 63 WHEN EXPANDING THIS SET
// IF YOU DO INCREASE THE NUMBER OF BITS USED IN URL_Rep::rep_info_st::content_type
// (Checked by running with -test=url.enum)

enum URLContentType
{
	URLContentType_RangeStart = 1000,
	URL_HTML_CONTENT = URLContentType_RangeStart,
	URL_XML_CONTENT,
	URL_SVG_CONTENT,
	URL_WBXML_CONTENT,
	URL_WML_CONTENT,
	URL_WBMP_CONTENT,
	URL_OPF_CONTENT,
	URL_CSR_CONTENT,
	URL_TEXT_CONTENT,
	URL_GIF_CONTENT,
	URL_JPG_CONTENT,
	URL_TV_CONTENT,
	URL_XBM_CONTENT,
	URL_BMP_CONTENT,
	URL_WEBP_CONTENT,
	URL_CPR_CONTENT,
	URL_WAV_CONTENT,
	URL_MIDI_CONTENT,
	URL_AVI_CONTENT,
	URL_DIR_CONTENT,
	URL_MPG_CONTENT,
	URL_MP4_CONTENT,
	URL_X_MIXED_REPLACE_CONTENT,
	URL_X_X509_USER_CERT,
	URL_X_X509_CA_CERT,
	URL_X_PEM_FILE,
	URL_NEWS_ATTACHMENT,
	URL_MIME_CONTENT,
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	URL_MHTML_ARCHIVE,
#endif
	URL_X_JAVASCRIPT,
	URL_PNG_CONTENT,
	URL_CSS_CONTENT,
	URL_PAC_CONTENT,
	URL_PFX_CONTENT,
	/** The MIME type is unknown (cannot be mapped to the enum) */
	URL_UNKNOWN_CONTENT,
	/** The MIME type is undetermined (empty). ContentDetector class can still try to sniff a MIME type, and map it to this enum. */
	URL_UNDETERMINED_CONTENT,
#ifdef _ISHORTCUT_SUPPORT
	URL_ISHORTCUT_CONTENT,
#endif//_ISHORTCUT_SUPPORT
#ifdef JAD_SUPPORT
	URL_JAD_CONTENT,
#endif // JAD_SUPPORT
#ifdef ICO_SUPPORT
	URL_ICO_CONTENT,
#endif // ICO_SUPPORT
#ifdef NEED_EPOC_CONTENT_TYPE
	URL_EPOC_CONTENT,
#endif //EPOC
#ifdef OPERA_SETUP_DOWNLOAD_APPLY_SUPPORT
	URL_MULTI_CONFIGURATION_CONTENT,
	URL_SKIN_CONFIGURATION_CONTENT,
	URL_MENU_CONFIGURATION_CONTENT,
	URL_TOOLBAR_CONFIGURATION_CONTENT,
	URL_MOUSE_CONFIGURATION_CONTENT,
	URL_KEYBOARD_CONFIGURATION_CONTENT,
#endif //OPERA_SETUP_DOWNLOAD_APPLY_SUPPORT
#ifdef _BITTORRENT_SUPPORT_
	URL_P2P_BITTORRENT,
#endif
#if defined(PREFS_DOWNLOAD)
	URL_PREFS_CONTENT,
#endif
#ifdef WBMULTIPART_MIXED_SUPPORT
	URL_X_MIXED_BIN_REPLACE_CONTENT,
#endif // WBMULTIPART_MIXED_SUPPORT
#ifdef GADGET_SUPPORT
	URL_GADGET_INSTALL_CONTENT,
	URL_UNITESERVICE_INSTALL_CONTENT,
#endif // GADGET_SUPPORT
#ifdef EXTENSION_SUPPORT
	URL_EXTENSION_INSTALL_CONTENT,
#endif // EXTENSION_SUPPORT
#ifdef EXTENDED_MIMETYPES
	URL_FLASH_CONTENT,
#endif
#ifdef APPLICATION_CACHE_SUPPORT
	URL_MANIFEST_CONTENT,
#endif // APPLICATION_CACHE_SUPPORT
#ifdef EVENT_SOURCE_SUPPORT
	URL_EVENTSOURCE_CONTENT,
#endif // EVENT_SOURCE_SUPPORT
	URL_MEDIA_CONTENT,
#ifdef XML_AIT_SUPPORT
	URL_XML_AIT,
#endif // XML_AIT_SUPPORT

	/* Place new content types above this line */
	URL_NUMER_OF_CONTENT_TYPES,
	URLContentType_RangeEnd
};


enum HTTP_Request_Method
{
	HTTP_Get,
	HTTP_Post,
	HTTP_Head,
	HTTP_Connect,
	HTTP_Put,
	HTTP_Options,
	HTTP_Delete,
	HTTP_Trace,
	HTTP_Search,
	HTTP_String
};

enum URLStatus
{
	URLStatus_RangeStart = 0,
	URL_EMPTY = URLStatus_RangeStart,
	URL_UNLOADED,
	URL_LOADED,
	URL_LOADING,
	URL_LOADING_ABORTED,
	URL_LOADING_FAILURE,
	URL_LOADING_WAITING,
	URL_LOADING_FROM_CACHE,
	URLStatus_RangeEnd
};

#ifdef _DEBUG
Debug& operator<<(Debug& d, enum URLStatus s);
#endif // _DEBUG

enum URLManagerEvent
{
	URL_LOADING_STARTED,
	URL_GADGET_LOADING_STARTED,
	URL_LOADING_REDIRECT_FILTERED
};

enum HTTP_Authinfo_Status {
	AuthInfo_Failed,
	AuthInfo_Succeded,
	Authinfo_Not_Finished
};

enum CommState {
    COMM_REQUEST_FAILED = 0,
    COMM_LOADING,
    COMM_WAITING,
    COMM_REQUEST_FINISHED,
    COMM_REQUEST_WAITING,
    COMM_WAITING_FOR_SYNC_DNS,
    COMM_LISTENING,
    COMM_CHANGED_HOSTNAME
};

enum URLLoadResult
{
	URL_LOAD_FINISHED,
	URL_LOAD_FAILED,
	URL_LOAD_STOPPED
};

enum URLCacheType
{
	URL_CACHE_NO = 3000,    /**< nothing cached yet */
	URL_CACHE_MEMORY,    /**< Cache_Storage and Memory_Only_Storage */
	URL_CACHE_DISK,      /**< regular cache file */
	URL_CACHE_TEMP,      /**< delete the file when possible */
	URL_CACHE_USERFILE,  /**< probably something not related to the cache */
	URL_CACHE_STREAM,    /**< Use StreamCache_Storage and never touch disk */
	URL_CACHE_MHTML      /**< This is a MHTML/MIME decoder element, directly accessible content is a MIME document wrapper */
};

enum URL_CopyGuard
{
	COPYGUARD_NONE = 0,
	COPYGUARD_SAMPLE = 1,
	COPYGUARD_COPY = 2
};

enum SecurityLevel {
	SECURITY_STATE_NONE = 0,          /**< No security, non-"https" url. */
	SECURITY_STATE_LOW = 1,           /**< Low security: SECURITY_LOW_REASON_CERTIFICATE_WARNING, SECURITY_LOW_REASON_WEAK_KEY, SECURITY_LOW_REASON_WEAK_PROTOCOL (old SSL ver). */
	SECURITY_STATE_HALF = 2,          /**< Medium security: SECURITY_LOW_REASON_UNABLE_TO_CRL_VALIDATE, SECURITY_LOW_REASON_OCSP_FAILED, SECURITY_LOW_REASON_UNABLE_TO_OCSP_VALIDATE, SECURITY_LOW_REASON_SLIGHTLY_WEAK_KEY, SECURITY_LOW_REASON_WEAK_PROTOCOL (reneg). */
	SECURITY_STATE_FULL = 3,          /**< High security. */
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	SECURITY_STATE_SOME_EXTENDED = 4, /**< Main document has SECURITY_STATE_FULL_EXTENDED, inline element has SECURITY_STATE_FULL. */
	SECURITY_STATE_FULL_EXTENDED = 5, /**< SECUREITY_STATE_FULL and server has certificate satisfying Extended Validation (EV) specifications. */
#endif
	SECURITY_STATE_UNKNOWN = 10       /**< Security state has not been determined yet or is temporarily unknown, while the document is being (re-)loaded.
	                                   *   If reloading, don't conclude that security has been lowered, show previous state until the final state is determined. */
};

enum SecurityLowReason {
	SECURITY_REASON_NOT_NEEDED                  = 0x000, /**< (Security is off or not lowered.) */
	SECURITY_LOW_REASON_WEAK_METHOD             = 0x001, /**< The encryption method used by the server is weak or out-dated. */
	SECURITY_LOW_REASON_WEAK_KEY                = 0x002, /**< The server is using a short encryption key, easily crackable (adjusted by pref). */
	SECURITY_LOW_REASON_SLIGHTLY_WEAK_KEY       = 0x004, /**< The server is using a medium length encryption key, soon easily crackable (adjusted by pref). */
	SECURITY_LOW_REASON_WEAK_PROTOCOL           = 0x008, /**< The server is using old SSL version or is vulnerable to TLS renegotiation attack (if pref set). */
	SECURITY_LOW_REASON_CERTIFICATE_WARNING     = 0x010, /**< A certificate warning was given (but the user accepted it). E.g. certificate expired or not yet valid. */
	SECURITY_LOW_REASON_UNABLE_TO_OCSP_VALIDATE = 0x020, /**< We were unable to reach the Online certificate validation (OCSP) server. */
	SECURITY_LOW_REASON_OCSP_FAILED             = 0x040, /**< Online certificate validation (OCSP) failed. */
	SECURITY_LOW_REASON_UNABLE_TO_CRL_VALIDATE  = 0x080, /**< We were unable to reach the Certificate revocation list (CRL) server. */
	SECURITY_LOW_REASON_CRL_FAILED              = 0x100  /**< Certificate was revoked in the Certificate Revocation List (CRL). */
};

enum URL_Priority
{
	/** Lowest priority */
	URL_LOWEST_PRIORITY = 0,

	/** Normal priority */
	URL_NORMAL_PRIORITY = 15,
	URL_DEFAULT_PRIORITY = URL_NORMAL_PRIORITY,

	/** Highest priority */
	URL_HIGHEST_PRIORITY = 31,
};

#define AuthScheme			BYTE

#define AUTH_USER_CANCELLED		0
#define AUTH_NO_METH			1
#define AUTH_NO_PROXY_METH		2
#define AUTH_NO_DOMAIN_MATCH	3
#define AUTH_MEMORY_FAILURE		4
#define AUTH_GENERAL_FAILURE	5

enum URLType
{
	URL_HTTP = 2000,
	URL_FTP,
	URL_Gopher,
	URL_WAIS,
	URL_FILE,
	URL_NEWS,
	URL_TELNET,
	URL_MAILTO,
	URL_HTTPS,
	URL_OPERA,
	URL_JAVASCRIPT,
	URL_SNEWS,
	URL_NEWSATTACHMENT,
	URL_SNEWSATTACHMENT,
	URL_ATTACHMENT,
	URL_EMAIL,
	URL_CONTENT_ID,
	URL_TN3270,
	URL_NNTP,				// alternative protocol, translated to URL_NEWS
	URL_NNTPS,				// alternative protocol, translated to URL_SNEWS
	URL_TV,
	URL_TEL,
	URL_MAIL,
	URL_DATA,
	URL_ABOUT,				// alias for URL_OPERA, only used for about:blank
	URL_IRC,
	URL_CHAT_TRANSFER,
	URL_MOUNTPOINT,
	URL_WIDGET,
	URL_UNITE,
	URL_WEBSOCKET,
	URL_WEBSOCKET_SECURE,
	URL_SOCKS,
	URL_ED2K,

	// Append new types here. Do not use ifdefs.
	URL_UNKNOWN,
	URL_NULL_TYPE
};

enum COOKIE_MODES
{
	COOKIE_NONE = 0,
	COOKIE_NO_THIRD_PARTY = 1, // also domain
	COOKIE_ONLY_SERVER_deprecated=2,
	COOKIE_SEND_NOT_ACCEPT_3P=3,
	COOKIE_ONLY_SELECTED_SERVERS_N3P_deprecated =4,// No third party
	COOKIE_ONLY_SELECTED_SERVERS_OS_deprecated =5, // Only cookies for the server
	COOKIE_ONLY_SELECTED_SERVERS_ALL_deprecated=6, // From any server
	COOKIE_WARN_THIRD_PARTY=7,			// All non-third-party, user decides third party
	COOKIE_ONLY_SELECTED_SERVERS_W3P_deprecated=8,   // All non-third-party selected servers,  user decides third party selected server
	// Used by filters
	COOKIE_DEFAULT=9,
	COOKIE_NONE_FROM_SERVER_deprecated=10,
	COOKIE_NO_THIRD_PARTY_FROM_SERVER_deprecated=11,
	COOKIE_ACCEPT_THIRD_PARTY=12, // domain
	COOKIE_THIRD_PARTY_ONLY_SERVER_deprecated=13, // server_only
	// New values below
	COOKIE_ALL = 14,
	COOKIE_SEND_NOT_ACCEPT_3P_old =15,
	// New Values above
	Cookie_max_value
};

enum COOKIE_ILLPATH_MODE
{
	COOKIE_ILLPATH_DEFAULT,
	COOKIE_ILLPATH_REFUSE,
	COOKIE_ILLPATH_ACCEPT
};

enum COOKIE_DELETE_ON_EXIT_MODE
{
	COOKIE_EXIT_DELETE_DEFAULT,
	COOKIE_NO_DELETE_ON_EXIT,
	COOKIE_DELETE_ON_EXIT
};

/** Represents the different states that user name and password
  * criteria information is shown in URL strings as displayed to
  * the application or the user.
  */
enum Password_Status {
	PASSWORD_SHOW,		/**< Include complete username:password */
	PASSWORD_HIDDEN,	/**< Do not display password, but replace it by "*****" */
	PASSWORD_ONLYUSER,	/**< Do not display any password, but show username */
	PASSWORD_NOTHING,	/**< Remove all password information, also username */
	PASSWORD_INTERNAL_LINKID,	/**< ********INTERNAL************ USE ONLY. Same as PASSWORD_SHOW but includes portnumber, uses the default portnumber if it is known **/
	PASSWORD_INTERNAL_LINKID_UNIQUE,	/**< ********INTERNAL************ USE ONLY. Same as PASSWORD_INTERNAL_LINKID, but used for unique URL's (POST forms) and prepends a unique identifier **/
	PASSWORD_SHOW_VERBATIM,		/**< Same as PASSWORD_SHOW, but do not un- or re-escape anything, used for relative URLs **/
	PASSWORD_HIDDEN_VERBATIM,	/**< Same as PASSWORD_HIDDEN, but do not un- or re-escape anything, used for relative URLs **/
	PASSWORD_NOTHING_VERBATIM,	/**< Same as PASSWORD_NOTHING, but do not un- or re-escape anything, used for relative URLs **/
	PASSWORD_ONLYUSER_VERBATIM, /**< Same as PASSWORD_ONLYUSER, but do not un- or re-escape anything, used for relative URLs **/
	PASSWORD_SHOW_UNESCAPE_URL, /**< Same as PASSWORD_SHOW. Also deescape Escaped characters */
	PASSWORD_HIDDEN_UNESCAPE_URL, /**< Do not display password, but replace it by "*****", Also deescape Escaped characters */
	PASSWORD_ONLYUSER_UNESCAPE_URL,	/**< Do not display any password, but show username,  Also deescape Escaped characters   */
	PASSWORD_NOTHING_UNESCAPE_URL /**< Remove all password information, also username, Also deescape Escaped characters */
};


#define MIME_NOT_CLEANSPACE 1
#define MIME_IN_COMMENT 2
#define MIME_8BIT_RESTRICTION 4

enum Mime_EncodeTypes {
#ifdef URL_UPLOAD_BASE64_SUPPORT
	GEN_BASE64,
	GEN_BASE64_ONELINE,
	GEN_BASE64_ONLY_LF,
	MAIL_BASE64,
	UUENCODE,
#endif
#ifdef URL_UPLOAD_QP_SUPPORT
	MAIL_QP,
	MAIL_QP_E,
#endif
#ifdef URL_UPLOAD_MAILENCODING_SUPPORT
	MAIL_7BIT,
	MAIL_8BIT,
#endif
	MAIL_BINARY,
#ifdef URL_UPLOAD_HTTP_SUPPORT
	HTTP_ENCODING,
	HTTP_ENCODING_BINARY,
#endif
	BINHEX,
	NO_ENCODING
};

enum MIME_Encode_Error {	// Change to extend OpStatus... (no)
	MIME_NO_ERROR,
	MIME_LINE_TOO_LONG,
	MIME_7BIT_FAIL,
	MIME_8BIT_FAIL,
	MIME_FAILURE
};

/// User-Agent type
enum UA_BaseStringId
{
	UA_PolicyUnknown =-1,
	UA_NotOverridden = 0,
	UA_Opera,
	UA_Mozilla,
	/*UA_Mozilla_Compatible,*/
	UA_MSIE,
	UA_MozillaOnly,
	UA_MSIE_Only,
	UA_BEAR,
	UA_OperaDesktop,
	UA_Mozilla_478,
	// Append New At bottom, Used in Preference files
	UA_IMODE,
	UA_TVStore
};

inline BOOL IsOperaUA(enum UA_BaseStringId ua)
{
	return ua == UA_Opera || ua == UA_OperaDesktop
#ifdef URL_TVSTORE_UA
				|| ua == UA_TVStore
#endif
				;
}

enum EnteredByUser {
	NotEnteredByUser,
	WasEnteredByUser
};

enum URL_Resumable_Status {
	URL_Resumable_Status_RangeStart = 4000,
	Not_Resumable = URL_Resumable_Status_RangeStart,
	Resumable_Unknown,
	Probably_Resumable,
	URL_Resumable_Status_RangeEnd
};

enum TrustedNeverFlushLevel {
	NeverFlush_Undecided = 0,
	NeverFlush_Untrusted = 1,
	NeverFlush_Trusted = 2
};

#ifdef TRUST_RATING
enum TrustRating {
	Untrusted_Ask_Advisory = -1, // Site is untrusted but advisory must be asked to detrmine the trust rate.
	Not_Set = 0,                 // Trust rating has not been retrieved from server yet.
	Domain_Trusted,              // Domain is trusted
	Unknown_Trust,               // Unknown level
	Phishing,                    // Site is phishing
	Malware                      // Site offers malware
};
#endif

/**
 * IDN security level required to display this server name. See Unicode
 * Technical Report #36 for details.
 *
 * Please note that UTR36 has renamed the security levels since the draft
 * that was used to implement this. The new names are listed in
 * parentheses.
 */
enum URL_IDN_SecurityLevel {
	IDN_Undecided = 0,	/**< Only used internally in ServerName */
	IDN_No_IDN = 1,		/**< Domain name contains no non-ASCII characters ("ASCII-Only") */
	IDN_Minimal = 2,	/**< Domain name conforms to minimal security level ("Highly Restrictive") */
	IDN_Moderate = 3,	/**< Domain name conforms to moderate security level ("Moderately Restrictive") */
//	IDN_Expanded = 4,	/**< Domain name conforms to expanded security level ("Minimally Restrictive") */
	IDN_Unrestricted = 5,/**< Domain name conforms to unrestricted security level ("Unrestricted") */
	IDN_Invalid			/**< Domain name does not conform to any IDN security levels */
};

#include "modules/mime/mime_enum.h"

enum MIME_ScriptEmbedSourcePolicyType
{
	MIME_No_ScriptEmbed_Restrictions = 0,
	MIME_EMail_ScriptEmbed_Restrictions = 1,
	MIME_Local_ScriptEmbed_Restrictions,
	MIME_Remote_ScriptEmbed_Restrictions,
	MIME_Remote_Script_Disable,
	// Add new values above this. Values must be limited to 3 bits
	MIME_Size_ScriptEmbed_Restrictions
};

#define SECURITY_STATE_MASK 0x0f
#define SECURITY_STATE_MASK_BITS 4

#define SECURITY_LOW_REASON_MASK	0x1FF
#define SECURITY_LOW_REASON_BITS	9


#define COOKIE_ILLEGAL_PATH_SHOW_WARNING 0
#define COOKIE_ILLEGAL_PATH_REFUSE_ALL 1
#define COOKIE_ILLEGAL_PATH_ACCEPT_ALL 2

#define COOKIE_NOIPADDR_WARN 0
#define COOKIE_NOIPADDR_NO_ACCEPT 1
#define COOKIE_NOIPADDR_SERVER_ONLY 2


#define MAX_URL_LEN			4096

#define MAKE_INLINE_PARM_2(stat, extra) ((WORD)(((BYTE)(extra))|(((WORD)((BYTE)(stat)))<<8)))
#define URL_INLINE_NEED_PLUGIN	0x01

#define URL_STAT_PARM(parm) HIBYTE(parm)
#define URL_EXTRA_PARM(parm) LOBYTE(parm)

#define DEFAULT_FILE_LOAD_BUF_SIZE	(4096*4)

#define URL_SET_ATTRIBUTE(attr, val) \
	{OpStatus::Ignore(SetAttribute(attr,val));}

#define URL_SET_ATTRIBUTE_RET(attr, val) \
	{return SetAttribute(attr,val);}

#define URL_SET_ATTRIBUTE_RETCOND(attr, val) \
	RETURN_IF_ERROR(SetAttribute(attr,val));

#ifdef WEB_TURBO_MODE
# define WEB_TURBO_FEATURE_REQUEST_HEADER_REDUCTION		0x01
# define WEB_TURBO_FEATURE_ECMASCRIPT					0x02
# define WEB_TURBO_FEATURE_PLUGINS						0x04
# define WEB_TURBO_FEATURE_RESPONSE_HEADER_REDUCTION	0x08

# define WEB_TURBO_BYPASS_RESPONSE			1
# define WEB_TURBO_HOSTNAME_LOOKUP_FAULURE	2
#endif // WEB_TURBO_MODE

#ifdef HTTP_CONTENT_USAGE_INDICATION
enum HTTP_ContentUsageIndication
{
	HTTP_UsageIndication_MainDocument,
	HTTP_UsageIndication_Script,
	HTTP_UsageIndication_Style,
	HTTP_UsageIndication_BGImage,
	HTTP_UsageIndication_OtherInline,
	HTTP_UsageIndication_max_value // Need to increase storage if this gets a bigger value than 7
};
#endif // HTTP_CONTENT_USAGE_INDICATION


#endif // _URL_ENUM_H_
