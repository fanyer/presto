/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_URL_URL_MODULE_H
#define MODULES_URL_URL_MODULE_H

#include "modules/hardcore/opera/module.h"
#include "modules/util/objfactory.h"
#include "modules/url/protocols/commelm.h"
#include "modules/url/url_enum.h"
#include "modules/url/url_id.h"
#include "modules/url/tools/arrays_decl.h"
#include "modules/url/url_lop_api.h"
#ifdef PI_INTERNET_CONNECTION
#include "modules/url/internet_connection_wrapper.h"
#endif // PI_INTERNET_CONNECTION

#ifdef PI_NETWORK_INTERFACE_MANAGER
class InternetConnectionStatusChecker;
#endif // PI_NETWORK_INTERFACE_MANAGER

class URL_Manager;
class URL_API;
class UrlDNSManager;
class CommCleaner;
class URL_Rep;
class URL_RelRep;
class URL_Name;
class OpSocketAddress;
class HTTP_Manager;
class FTP_Manager;
class WebSocket_Manager;
struct KeywordIndex;
class ServerName;
struct protocol_selentry;
class OpDLL;
class MountPointProtocolManager;
struct ContentDetectorPatternData;
class UAManager;

#ifdef USE_SPDY
class SpdySettingsManager;
#endif // USE_SPDY

#define IDNA_TempBufSize 256

#if defined EXTENDED_MIMETYPES && defined _BITTORRENT_SUPPORT_
# define NUMBER_OF_MIMETYPES 38
#elif defined EXTENDED_MIMETYPES || defined _BITTORRENT_SUPPORT_
# define NUMBER_OF_MIMETYPES 37
#else
# define NUMBER_OF_MIMETYPES 36
#endif

class UrlModule : public OperaModule
{
public:
	UrlModule();
	virtual ~UrlModule(){};

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	URL_Manager* g_urlManager;
	URL_API *g_url_api_obj;
	CommCleaner *g_comm_cleaner_obj;
	UrlDNSManager *g_url_dns_manager;
	OpObjectFactory<SCommWaitElm>* scomm_wait_elm_factory;
	OpObjectFactory<CommWaitElm>* comm_wait_elm_factory;

	URL_Rep* g_EmptyURL_Rep;
	URL_RelRep* g_EmptyURL_RelRep;
	unsigned char* ds_buf;
	unsigned long ds_lbuf;

    char*	tempurl;
	size_t	tempurl_len;

	const URL_Name*	g_last_user;
	unsigned int	g_last_hide;
	URL_RelRep *	g_last_rel_rep;
	uni_char*		tempuniurl;
	OpString		tempunipath;
	const URL_Name*	g_uni_last_user;
	unsigned int	g_uni_last_hide;
	URL_RelRep *	g_uni_last_rel_rep;

	char *			g_HTTP_TmpBuf;
	unsigned long	g_HTTP_TmpBufSize;

	unsigned long	g_auth_id_counter0 ;
	unsigned long	g_attachment_counter;

	ServerName		*g_CommUseLocalHost;
	BOOL			g_CommUseLocalHostInitialized;

	int				g_comm_count;
	Head			ConnectionWaitList;
	Head			ConnectionTempList;
	/** What is the ID that will be assigned to the next SComm object ?*/
	unsigned int	scomm_id;
	/** List of SComm objects that are waiting to be deleted (because they were blocked at the time the owner tried to delete them) */
	Head			DeletedCommList;
	int				g_comm_list_call_count;

	OpSocketAddress*		local_host_addr;

	HTTP_Manager *			g_http_Manager;
#ifndef NO_FTP_SUPPORT
	FTP_Manager *			g_ftp_Manager;
#endif
#ifdef WEBSOCKETS_SUPPORT
	WebSocket_Manager*		webSocket_Manager;
#endif
	DECLARE_MODULE_CONST_ARRAY(const char*, HTTP_Method_Strings);
	DECLARE_MODULE_CONST_ARRAY(const char*, HTTP_Scheme_Strings);
	DECLARE_MODULE_CONST_ARRAY(const char*, HTTP_Header_List);
	DECLARE_MODULE_CONST_ARRAY(ContentDetectorPatternData, PatternData);
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, Untrusted_headers_HTTP);
#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, Dynattr_Blocked_HTTPHeader);
#endif
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, HTTP_arbitrary_method_list);

#if !defined(NO_URL_OPERA) || defined(HAS_OPERABLANK)
	DECLARE_MODULE_CONST_ARRAY(KeywordIndex, operaurl_keyword);
#endif
#ifndef NO_FTP_SUPPORT
	DECLARE_MODULE_CONST_ARRAY(const char*, url_month_array);
#endif

	DECLARE_MODULE_CONST_ARRAY(protocol_selentry, URL_typenames);

	DECLARE_MODULE_CONST_ARRAY_PRESIZED(const char*, mime_types, NUMBER_OF_MIMETYPES);

#if defined URL_USE_UNIQUE_ID
	URL_ID					m_url_id_counter;
#endif
	URL_CONTEXT_ID			m_url_context_id_counter;
	UINT32					m_url_storage_id_counter;

	/** List of Opera URL generators */
	List<OperaURL_Generator>	m_opera_url_generators;

#ifdef DOM_GADGET_FILE_API_SUPPORT
	MountPointProtocolManager *mountPointProtocolManager;
#endif // DOM_GADGET_FILE_API_SUPPORT

	unsigned short m_disabled_url_charsetids[4];
	BOOL GetEnabledURLNameCharset(unsigned short id);

#ifdef PI_INTERNET_CONNECTION
	InternetConnectionWrapper m_internet_connection;
#endif // PI_INTERNET_CONNECTION

#ifdef PI_NETWORK_INTERFACE_MANAGER
	InternetConnectionStatusChecker *m_network_connection_status_checker;
#endif // PI_NETWORK_INTERFACE_MANAGER

	UAManager *m_ua_manager;

#ifdef USE_SPDY
	SpdySettingsManager *m_spdy_settings_manager;
#endif // USE_SPDY

#ifdef URL_NETWORK_DATA_COUNTERS
	NetworkCounters m_network_counters;
#endif
};

#define g_url_module g_opera->url_module

#define g_SCommWaitElm_factory g_opera->url_module.scomm_wait_elm_factory
#define g_CommWaitElm_factory g_opera->url_module.comm_wait_elm_factory
#define urlManager g_opera->url_module.g_urlManager
#define g_cookieManager static_cast<Cookie_Manager*>(g_opera->url_module.g_urlManager)
#define g_url_api g_opera->url_module.g_url_api_obj
#define g_comm_cleaner g_opera->url_module.g_comm_cleaner_obj
#define g_url_dns_manager g_opera->url_module.g_url_dns_manager
#define EmptyURL_Rep g_opera->url_module.g_EmptyURL_Rep
#define EmptyURL_RelRep g_opera->url_module.g_EmptyURL_RelRep
#define g_ds_buf g_opera->url_module.ds_buf
#define g_ds_lbuf g_opera->url_module.ds_lbuf
#define g_tempurl     g_opera->url_module.tempurl
#define g_tempurl_len g_opera->url_module.tempurl_len
#define g_tempunipath g_opera->url_module.tempunipath
#define g_tempuniurl  g_opera->url_module.tempuniurl
#define last_user g_opera->url_module.g_last_user
#define last_hide g_opera->url_module.g_last_hide
#define last_rel_rep g_opera->url_module.g_last_rel_rep
#define uni_last_user g_opera->url_module.g_uni_last_user
#define uni_last_hide g_opera->url_module.g_uni_last_hide
#define uni_last_rel_rep g_opera->url_module.g_uni_last_rel_rep
#define HTTP_TmpBuf g_opera->url_module.g_HTTP_TmpBuf
#define HTTP_TmpBufSize g_opera->url_module.g_HTTP_TmpBufSize
#define g_auth_id_counter  g_opera->url_module.g_auth_id_counter0
#define g_mime_attachment_counter g_opera->url_module.g_attachment_counter
#define CommUseLocalHost g_opera->url_module.g_CommUseLocalHost
#define CommUseLocalHostInitialized g_opera->url_module.g_CommUseLocalHostInitialized
#define g_ConnectionWaitList (&(g_opera->url_module.ConnectionWaitList))
#define g_ConnectionTempList (&(g_opera->url_module.ConnectionTempList))
#define g_url_comm_count g_opera->url_module.g_comm_count
#define g_local_host_addr g_opera->url_module.local_host_addr
#define g_scomm_id g_opera->url_module.scomm_id
#define g_DeletedCommList (&(g_opera->url_module.DeletedCommList))
#define comm_list_call_count g_opera->url_module.g_comm_list_call_count
#define http_Manager g_opera->url_module.g_http_Manager
#define ftp_Manager g_opera->url_module.g_ftp_Manager
#define g_webSocket_Manager g_opera->url_module.webSocket_Manager
#define g_OperaUrlGenerators g_opera->url_module.m_opera_url_generators

#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
#define g_externalProtocolHandlers urlManager
#endif

#ifndef HAS_COMPLEX_GLOBALS
#define g_HTTP_Method_Strings	CONST_ARRAY_GLOBAL_NAME(url, HTTP_Method_Strings)
#define g_HTTP_Scheme_Strings	CONST_ARRAY_GLOBAL_NAME(url, HTTP_Scheme_Strings)
#define g_HTTP_Header_List	CONST_ARRAY_GLOBAL_NAME(url, HTTP_Header_List)
#define g_Untrusted_headers_HTTP	CONST_ARRAY_GLOBAL_NAME(url, Untrusted_headers_HTTP)
#define g_PatternData	CONST_ARRAY_GLOBAL_NAME(url, PatternData)
#define g_Dynattr_Blocked_HTTPHeader	CONST_ARRAY_GLOBAL_NAME(url, Dynattr_Blocked_HTTPHeader)
#define g_HTTP_arbitrary_method_list	CONST_ARRAY_GLOBAL_NAME(url, HTTP_arbitrary_method_list)

#if !defined(NO_URL_OPERA) || defined(HAS_OPERABLANK)
#define g_operaurl_keyword	CONST_ARRAY_GLOBAL_NAME(url, operaurl_keyword)
#endif
#ifndef NO_FTP_SUPPORT
#define g_url_month_array	CONST_ARRAY_GLOBAL_NAME(url, url_month_array)
#endif
#define g_mime_types	CONST_ARRAY_GLOBAL_NAME(url, mime_types)
#endif // HAS_COMPLEX_GLOBALS

#define g_URL_typenames	CONST_ARRAY_GLOBAL_NAME(url, URL_typenames)

#if defined URL_USE_UNIQUE_ID
#define  g_url_id_counter		g_opera->url_module.m_url_id_counter
#endif
#define  g_url_context_id_counter		g_opera->url_module.m_url_context_id_counter
#define  g_url_storage_id_counter		g_opera->url_module.m_url_storage_id_counter

#ifdef DOM_GADGET_FILE_API_SUPPORT
#define g_mountPointProtocolManager	g_opera->url_module.mountPointProtocolManager
#endif // DOM_GADGET_FILE_API_SUPPORT

#ifdef PI_INTERNET_CONNECTION
#define g_internet_connection g_opera->url_module.m_internet_connection
#endif // PI_INTERNET_CONNECTION

#ifdef PI_NETWORK_INTERFACE_MANAGER
#define g_network_connection_status_checker g_opera->url_module.m_network_connection_status_checker
#endif // PI_NETWORK_INTERFACE_MANAGER

#ifdef URL_NETWORK_DATA_COUNTERS
#define g_network_counters g_opera->url_module.m_network_counters
#endif

#ifdef USE_SPDY
#define g_spdy_settings_manager g_opera->url_module.m_spdy_settings_manager
#endif // USE_SPDY

#define URL_MODULE_REQUIRED

#endif // !MODULES_URL_URL_MODULE_H
