/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef URL_CAPABILITIES_H
#define URL_CAPABILITIES_H

// Must-Revalidate Cache-Control directive supported
#define URL_CAP_HTTP_MUST_REVALIDATE

// Ability to update send progress as the data are sent to the socket 
#define URL_CAP_SOCKET_SENDPROGRESS

// This version includes the URL_CDK_KEYBOARD_CONFIGURATION_CONTENT content-type enum
#define URL_CAP_CDK_KEYBOARD_DEF

// This version includes support for the URL_CHAT_TRANSFER type
#define URL_CAP_CHAT_TRANSFER_SUPPORT

// This module uses the new URL attribute API functions
#define URL_CAP_ATTRIBUTE_API

// This module uses the new g_url_api object to replace the urlManager object
#define URL_CAP_G_URL_API

// This module has a v1 of the directory/file restructuring
#define URL_CAP_DIRECTORIES_1

// This module has the URL::LoadDocument function and the associate LoadPolicy classes
#define URL_CAP_LOAD_DOCUMENT_POLICY

// This version have a WriteDocumentData functions with URL::URL_WriteDocumentConversion specification
#define URL_CAP_HAVE_WRITE_DOCUMENT_CONVERSION_API

// This module allow scripts and plugins to add their customized headers
#define URL_CAP_SET_HTTP_HEADER_2

// This version has all previously global objects located in the g_opera object
#define URL_CAP_GLOBAL_OBJECTS_IN_GOPERA

// This version supports the "Windows NT 5.1" user agent string, to identify as Opera Desktop on mobile platforms
#define URL_CAP_UASTRING_OPERADESKTOP

#define URL_CAP_HAS_PREFS_CONTENT	

// The URL::KIsDirectoryListing is defined in this release
#define URL_CAP_DIRECTORYLIST_FLAG

// The URL_Load_Normal and URL_Reload_If_Expired policy enums are defined
#define URL_CAP_IF_EXPIRED_RELOAD_POLICY 

// This version has the IAmLoadingThisURL class API, must be activated by API_URL_LOAD_RESERVATION_OBJECT
#define URL_CAP_I_AM_LOADING_THIS_URL_CLASS

// This module needs the base_url version of GetAttachmentIconURL
#define URL_CAP_USE_ICON_BASE_URL

// This module has the MIME_ScriptEmbedSourcePolicyType enums
#define URL_CAP_SCRIPT_EMBED_RESTRICTION_POLICY

// This module has the GenerateBasicAuthenticationString string
#define URL_CAP_GEN_BASIC_AUTH_STRING

// This module has the entered_by_user flag on URL_API::ResolveUrlNameL()
#define URL_CAP_RESOLVE_ENTERED_BY_USER

// This module includes Empty DCache and free unused resources also in Ramcache mode
#define URL_CAP_EMPTY_RAM_D_CACHE

// This module have the basic window commander listener
#define URL_CAP_BASIC_WINCOM_LISTENER

// This module has support for the SEARCH HTTP method
#define URL_CAP_HTTP_SEARCH_METHOD

// This module no longer have the capability to send a OPTIONS request to check HTTP version capabilities, was not reliable, and never used by default
#define URL_CAP_NO_HTTP_OPTIONS_CHECK

// This module has the stream cache and MHTML cache enums
#define URL_CAP_EXTENDED_CACHE_TYPES_1

// This module has the corrected (pointer, flag) API for the Void Pointer GetAttribute function.
#define URL_CAP_CORRECTED_VOIDP_GETATTRIBUTE

// This module have URL::SameServer, URL::KHaveServerName, and KUniHostNameAndPort_L
#define URL_CAP_CHECK_SERVERNAME 

// The 'secure' parameter that were used in a bunch of OpSocket methods are now only used in Create().
#define URL_CAP_OPSOCKET_SECURE_CREATE_ONLY

// The Prepare for viewing function can force memory-only cache to file
#define URL_CAP_PREPAREVIEW_FORCE_TO_FILE

// This module has the nettype functionaity, and it is enabled (this depends on functionality from the pi-module)
#define URL_CAP_NETTYPE

// This module have the url_lop_api.h file and the OperaURL_Generator class (registered using g_url_api->RegisterOperaURL())
#define URL_CAP_OPERA_URL_GENERATORS

// The mountpoint protocol manager is available in this module
#define URL_CAP_HAS_MOUNTPOINT_PROTOCOL_MANAGER

// This module is capable of disabling the mime enums and using the enums from the MIME module
#define URL_CAP_NO_MIME_ENUMS

// This module knows about the network selftest utility module
#define URL_CAP_KNOW_NETWORK_SELFTEST

// This module lets a URL be copied, resulting in a unique URL by using g_url_api->MakeUniqueCopy()
#define URL_CAP_MAKE_UNIQUE_COPY

// URL_API::VisitedPagesSearchPermitted exists (if VPS is enabled)
#define URL_CAP_VISITED_PAGES_SEARCH_PERMITTED

// URL::KUseProxy exists
#define URL_CAP_KUSEPROXY_EXISTS

// Supports priorities on http requests
#define URL_CAP_HTTP_PRIORITY

// ServerName has SetAdvisorInfo and GetAdvisor
#define URL_CAP_SN_ADVISORY

// This module has the debug URL string
#define URL_CAP_HAVE_DEBUG_URL_STRING

// This module have the charset requiring variants of UniName attribute functions.
#define URL_CAP_HAVE_CHARSET_UNINAME

// In This module all GetAttribute APIs have the redirect URL::URL_Redirect enum
#define URL_CAP_HAVE_REDIRECT_ENUM

// In This module all IsNetworkAvailable exists
#define URL_CAP_HAVE_ISNETWORKAVAILABLE

// This module supports resetting the cache element
#define URL_CAP_HAVE_RESETCACHE

#define URL_CAP_AUTHENTICATION_WITHOUT_UI

// This module contains the URL::KIgnoreWarnings attribute
#define URL_CAP_HAVE_IGNORE_WARNINGS

// This module have the COOKIE_SEND_NOT_ACCEPT_3P enum value which have the same value as COOKIE_ALL had earlier.
#define URL_CAP_HAVE_COOKIE_SEND_NOT_ACCEPT_3P

// This module can independently adjust the Cache Size of URL contexts
#define URL_CAP_HAVE_CONTEXT_CACHE_SIZE

// URL can use libcrypto for hashing
#define URL_CAP_USE_LIBCRYPTO_MODULE_HASH

// This module can support the "Generational Cache", above all TWEAK_CACHE_FAST_INDEX and TWEAK_CACHE_CONTAINERS_ENTRIES
#define URL_CAP_GENERATIONAL_CACHE

// The module supports a separate content type for Unite services (URL_UNITESERVICE_INSTALL_CONTENT), rather than using the gadget one
#define URL_CAP_HAS_UNITESERVICE_CONTENT_TYPE

// The module supports a window parameter to HandleSingleCookie
#define URL_CAP_SINGLE_COOKIE_WINDOW

// This module supports the URL::KUsedAsTunnel attribute
#define URL_CAP_HAS_USED_AS_TUNNEL

// This module calls http-logger in scope when the entire response has been receieved, depends on SCOPE_CAP_RESPONSE_RECEIVED
#define URL_CAP_SCOPE_HTTP_LOGGER_RESPONSE_RECEIVED

// This module have separated the Path and Query components of the URL
#define URL_CAP_SEPARATE_QUERY_COMPONENT

// This module have the dynamic attribute API g_url_api->RegisterAttributeL
#define URL_CAP_DYNAMIC_ATTRIBUTE

// This module have the URL::GetURLDisplayName and the GetAttribute(URL::URL_NameStringAttribute, OpString &, ...) functions
#define URL_CAP_DISPLAY_NAME

// This module supports the URL::KHTTPFrameOptions attribute and the associated header
#define URL_CAP_FRAME_OPTION_HEADER

// Imports the ServerNameAuthenticationManager from auth if available
#define URL_CAP_IMPORT_AUTH_SN

// Supports the URL_DocumentLoader class API
#define URL_CAP_DOCLOADER

// This release supports the Timeout attributes that lets loaders specify loading timeout policies for URLs
#define URL_CAP_TIMEOUT_ATTRIBUTES

// This relase supports the URL_API::RegisterUrlScheme function
#define URL_CAP_REGISTER_SCHEME

// This module contain the files modules/url/remap.h , and also indicates that the RemapUrlL() is now OP_STATUS RemapUrl()
#define URL_CAP_REMAP_HEADER

// This module contains the SecurityLevels enum
#define URL_CAP_SECURITY_LEVEL_ENUM

// This release have the send/receive multiheader functionality
#define URL_CAP_DYNATTR_MULTI_HEADER

// This module supports test for KIsUniteServiceAdminURL
#define URL_CAP_UNITE_ADMIN_URL

// URL has KHTTP_ContentUsageIndication attribute
#define URL_CAP_HTTP_CONTENT_USAGE_INDICATION

// URL has KUsesTurbo attribute
#define URL_CAP_WEB_TURBO_ATTRIBUTE

// URL has KTurboTransferredBytes and KTurboOriginalTransferredBytes attributes
#define URL_CAP_TURBO_COMPRESSION_INFO

#endif // URL_CAPABILITIES_H
