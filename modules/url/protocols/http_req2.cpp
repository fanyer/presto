/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/url/uamanager/ua.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/doc/css_mode.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/util/handy.h"

#include "modules/libcrypto/include/CryptoHash.h"

#include "modules/url/url_sn.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/connman.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/http_req2.h"
#include "modules/url/protocols/http_te.h"
#include "modules/upload/upload.h"
#include "modules/url/tools/arrays.h"

#include "modules/olddebug/timing.h"
#include "modules/olddebug/tstdump.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/url/url_man.h"
#include "modules/cache/url_cs.h"

#ifdef IMODE_EXTENSIONS
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/url2.h"
#include "modules/url/url_rep.h"
#endif //IMODE_EXTENSIONS

#ifdef WEB_TURBO_MODE
# include "modules/obml_comm/obml_config.h"
# include "modules/obml_comm/obml_id_manager.h"
# include "modules/prefs/prefsmanager/collections/pc_js.h"
# include "modules/prefs/prefsmanager/collections/pc_display.h"
#endif // WEB_TURBO_MODE

#ifdef SCOPE_RESOURCE_MANAGER
# include "modules/scope/scope_network_listener.h"
#endif // SCOPE_RESOURCE_MANAGER

// http_req.cpp

// HTTP request data

//extern MemoryManager* g_memory_manager;
//extern PrefsManager* prefsManager;
//extern URL_Manager* urlManager;

#if defined _DEBUG || defined _RELEASE_DEBUG_VERSION
#ifdef YNP_WORK
#define DEBUG_HTTP
#define DEBUG_HTTP_MSG
#define DEBUG_HTTP_FILE
#define DEBUG_HTTP_HEXDUMP
#define DEBUG_HTTP_REQDUMP
#define DEBUG_HTTP_REQDUMP1
#define DEBUG_UPLOAD
#endif
#endif

#ifdef _RELEASE_DEBUG_VERSION
#define DEBUG_HTTP
#define DEBUG_HTTP_MSG
#define DEBUG_HTTP_FILE
#define DEBUG_HTTP_HEXDUMP
#endif

#include "modules/url/protocols/accept_hdr.h"
#include "modules/url/tools/url_util.h"

PREFIX_CONST_ARRAY(static, const char*, HTTP_Method_Strings, url)
	CONST_ENTRY("GET")
	CONST_ENTRY("POST")
	CONST_ENTRY("HEAD")
	CONST_ENTRY("CONNECT")
	CONST_ENTRY("PUT")
	CONST_ENTRY("OPTIONS")
	CONST_ENTRY("DELETE")
	CONST_ENTRY("TRACE")
	CONST_ENTRY("SEARCH")
CONST_END(HTTP_Method_Strings)

PREFIX_CONST_ARRAY(static, const char*, HTTP_Scheme_Strings, url)
	CONST_ENTRY("http")
	CONST_ENTRY("https")
CONST_END(HTTP_Scheme_Strings)

PREFIX_CONST_ARRAY(static, const char *, HTTP_Header_List, url)
	CONST_ENTRY("User-Agent")		// sending
	CONST_ENTRY("Host")				// symbian sending
	CONST_ENTRY("Accept")			// sending
	CONST_ENTRY("Accept-Language")	// sending
	CONST_ENTRY("Accept-Charset")	// sending
	CONST_ENTRY("Accept-Encoding")	// implement
	CONST_ENTRY("Expect")			// implement
	CONST_ENTRY("Authorization")			// symbian
	CONST_ENTRY("Proxy-Authorization")	// symbian
#ifdef SUPPORT_RIM_MDS_AUTHENTICATION
	CONST_ENTRY("X-MDS-Authorization")
#endif
	CONST_ENTRY("Referer")			// symbian
	CONST_ENTRY("If-Modified-Since") // sending
#ifdef _HTTP_USE_IFUNMODIFIED
	CONST_ENTRY("If-Unmodified-Since") // implement
#endif
#ifdef _HTTP_USE_IFMATCH
	CONST_ENTRY("If-Match")// implement
#endif
	CONST_ENTRY("If-None-Match")
	CONST_ENTRY("If-Range")
	CONST_ENTRY("Cookie")
	CONST_ENTRY("Cookie2")
	CONST_ENTRY("Pragma")
	CONST_ENTRY("Cache-Control")
	CONST_ENTRY("Proxy-Connection")
	CONST_ENTRY("Connection")
	CONST_ENTRY("TE")
	CONST_ENTRY("Range")
	//"Content-Type",
	CONST_ENTRY("Content-Length")
#ifdef _EMBEDDED_BYTEMOBILE
	CONST_ENTRY("X-EBO-UA")
	CONST_ENTRY("X-EBO-Info")
	CONST_ENTRY("X-EBO-Req")
#endif //_EMBEDDED_BYTEMOBILE
#ifdef WEB_TURBO_MODE
	CONST_ENTRY("X-Opera-Info")
	CONST_ENTRY("X-Opera-ID")
	CONST_ENTRY("X-Opera-Host")
	CONST_ENTRY("X-OA")
	CONST_ENTRY("X-OB")
	CONST_ENTRY("X-OC")
#endif // WEB_TURBO_MODE
#ifdef DRM_INFO
	CONST_ENTRY("DRM-Version")
#endif // DRM_INFO
#ifdef DEVICE_STOCK_UA_SUPPORT
	CONST_ENTRY("Device-Stock-UA")
#endif // DEVICE_STOCK_UA_SUPPORT
	CONST_ENTRY(NULL)
CONST_END(HTTP_Header_List);

#ifdef URL_NEED_HTTP_ACCEPT_STRING
const char * GetHTTP_Accept_str()
{
	return HTTP_Accept_str;
}
#endif

const char *GetHTTPMethodString(HTTP_Method method)
{
	if(method < CONST_ARRAY_SIZE(url, HTTP_Method_Strings))
		return g_HTTP_Method_Strings[method];
	return NULL;
}

BOOL GetMethodHasRequestBody(HTTP_Method meth)
{
	switch(meth)
	{
	case HTTP_METHOD_POST:
	case HTTP_METHOD_PUT:
	case HTTP_METHOD_SEARCH:
	case HTTP_METHOD_OPTIONS:
	case HTTP_METHOD_DELETE:
	case HTTP_METHOD_String:
		return TRUE;
	}
	return FALSE;
}
BOOL GetMethodIsSafe(HTTP_Method meth)
{
	switch(meth)
	{
	case HTTP_METHOD_GET:
	case HTTP_METHOD_HEAD:
		return TRUE;
	}
	return FALSE;
}

BOOL GetMethodIsRestartable(HTTP_Method meth)
{
	switch(meth)
	{
	case HTTP_METHOD_GET:
	case HTTP_METHOD_HEAD:
	case HTTP_METHOD_CONNECT:
		return TRUE;
	}
	return FALSE;
}


HTTP_Request::HTTP_Request(
		MessageHandler* msg_handler,
		HTTP_Method meth,
		HTTP_request_st *req,
		UINT tcp_connection_established_timout,
		BOOL open_extra_idle_connections
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_)  || defined(_CERTICOM_SSL_SUPPORT_)
		, BOOL sec
#endif
		)
	: ProtocolComm(msg_handler,NULL,NULL)
#ifdef SCOPE_RESOURCE_MANAGER
		, has_sent_response_finished(FALSE)
#endif //SCOPE_RESOURCE_MANAGER

{
	InternalInit(meth, req, tcp_connection_established_timout, open_extra_idle_connections
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		, sec
#endif
		);
}

void HTTP_Request::InternalInit(
		HTTP_Method meth,
		HTTP_request_st *req,
		UINT tcp_connection_established_timout,
		BOOL open_extra_idle_connections
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		, BOOL sec
#endif
)
{
#ifdef DEBUG_HTTP_REQDUMP
	OpString8 textbuf1;

	textbuf1.AppendFormat("htpr%04d.txt",Id());
	PrintfTofile(textbuf1,"Creating Request %d : Server %s:%u : Path %s\n",
		Id(), req->connect_host->Name(), req->connect_port, req->request.CStr());
#endif
#ifdef DEBUG_HTTP_REQDUMP1
	PrintfTofile("httpreq.txt","Creating Request %d : Server %s:%u : Path %s Tick %lu\n",
		Id(), req->connect_host->Name(), req->connect_port, req->request.CStr(), (unsigned long) g_op_time_info->GetWallClockMS());
#endif
	http_conn = NULL;
	request = req;
#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
	secondary_request = NULL;
#endif

	m_tcp_connection_established_timout = tcp_connection_established_timout;

	sendcount = 0;
    SetHeaderLoaded(FALSE);
    SetProxyRequest(request->connect_host != request->origin_host || request->connect_port != request->origin_port ? TRUE : FALSE);
    SetProxyNoCache(FALSE);
    SetMethod(meth, (const char *) NULL);
	SetSentRequest(FALSE);
	SetWaiting(FALSE);
	SetRangeStart(FILE_LENGTH_NONE);
	SetRangeEnd(FILE_LENGTH_NONE);

    header_info = NULL;

	connectcount = 0;
	info.disable_headers_reduction = FALSE;
	info.loading_finished = FALSE;
	info.read_secondary = FALSE;
	//info.external_headerinfo = FALSE;
	info.proxy_connect_open = FALSE;
	info.send_expect_100_continue = FALSE;
	info.send_100c_body = FALSE;
	info.force_waiting = FALSE;
	info.is_formsrequest = FALSE;
	info.data_with_headers = FALSE;
	info.send_close = FALSE;
	info.pipeline_used_by_previous = FALSE;
	info.disable_automatic_refetch_on_error = FALSE;
	info.managed_request = FALSE;
#ifdef HTTP_AGGRESSIVE_PIPELINE_TIMEOUT
	info.sent_pipelined = FALSE;
#endif
#ifdef URL_ALLOW_DISABLE_COMPRESS
	info.disable_compress=FALSE;
#endif
	info.privacy_mode = FALSE;
#ifdef _ENABLE_AUTHENTICATE
	proxy_auth_id = 0;
	auth_id = 0;
#ifdef HTTP_DIGEST_AUTH

	auth_digest.entity_hash=NULL;

	auth_digest.hash_base = CRYPTO_HASH_TYPE_MD5;

	auth_digest.used_A1=NULL;
	auth_digest.used_cnonce=NULL;
	auth_digest.used_nonce=NULL;
	auth_digest.used_noncecount=0;
	proxy_digest.entity_hash=NULL;

	proxy_digest.hash_base = CRYPTO_HASH_TYPE_MD5;

	proxy_digest.used_A1=NULL;
	proxy_digest.used_cnonce=NULL;
	proxy_digest.used_nonce=NULL;
	proxy_digest.used_noncecount=0;
#endif
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	info.do_ntlm_authentication = FALSE;
	proxy_ntlm_auth_element = NULL;
	server_negotiate_auth_element = NULL;
	info.ntlm_updated_proxy = FALSE;
	info.ntlm_updated_server= FALSE;
#endif
#endif

#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
	info.secure = sec;
#endif

	loading_started = 0;

	if(info.proxy_request)
		SetConnectMsgMode(START_CONNECT_PROXY);
	//entity_tag = NULL;

	request = req;

	master = NULL;

#ifdef _URL_EXTERNAL_LOAD_ENABLED_
	info.ex_is_external = FALSE;
	info.upload_buffer_underrun = FALSE;
#endif


	info.retried_loading = FALSE;
	previous_load_length = 0;
	previous_load_difference = 0;

#if defined(_EMBEDDED_BYTEMOBILE) || defined(SCOPE_HTTP_LOGGER) || defined(WEB_TURBO_MODE)
	m_request_number = 0;
#endif  // _EMBEDDED_BYTEMOBILE || SCOPE_HTTP_LOGGER || WEB_TURBO_MODE
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	info.load_direct = FALSE;
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef WEB_TURBO_MODE
	info.bypass_turbo_proxy = FALSE;
	info.first_turbo_request = 0;
	info.turbo_proxy_auth_retry = 0;
#endif // WEB_TURBO_MODE
#ifdef _EMBEDDED_BYTEMOBILE
	info.bypass_ebo_proxy = FALSE;
#endif // _EMBEDDED_BYTEMOBILE
#if defined(WEBSERVER_SUPPORT) && defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
	info.unite_request = FALSE;
	info.unite_admin_request = FALSE;
#endif // WEBSERVER_SUPPORT
	use_nettype = NETTYPE_UNDETERMINED;
#ifdef HTTP_CONTENT_USAGE_INDICATION
	info.usage_indication = HTTP_UsageIndication_MainDocument;
#endif // HTTP_CONTENT_USAGE_INDICATION
	info.open_extra_idle_connections = open_extra_idle_connections;
	msg_id = 0;
#if (defined EXTERNAL_ACCEPTLANGUAGE || defined EXTERNAL_HTTP_HEADERS)
	context_id = 0;
#endif
#ifdef OPERA_PERFORMANCE
	time_request_created.timestamp_now();
#endif // OPERA_PERFORMANCE
	//requeued = 0;
}

/*
OP_STATUS HTTP_Request::Construct()
{
	TRAPD(op_err, ConstructL());

	return op_err;
}
*/

void HTTP_Request::ConstructL(
#ifdef IMODE_EXTENSIONS
								BOOL use_ua_utn
#endif
							  )
{
	const char **header_list = (const char **) g_HTTP_Header_List;
	Upload_EncapsulatedElement::InitL(NULL, FALSE, FALSE, FALSE, FALSE, NULL, ENCODING_NONE, header_list);
	Headers.SetSeparator(SEPARATOR_COMMA);
	header_info = OP_NEW_L(HeaderInfo, (method));

	LEAVE_IF_ERROR(SetUpMaster());

	char *UserAgentStr = (char *) g_memory_manager->GetTempBuf2k();
#ifdef IMODE_EXTENSIONS
	int ua_str_len = g_uaManager->GetUserAgentStr(UserAgentStr, g_memory_manager->GetTempBuf2kLen()-1, request->connect_host->UniName(), NULL,
												request->origin_host->GetOverRideUserAgent(), use_ua_utn);
#else //!IMODE_EXTENSIONS
	int ua_str_len = g_uaManager->GetUserAgentStr(UserAgentStr, g_memory_manager->GetTempBuf2kLen()-1, request->connect_host->UniName(), NULL,
												request->origin_host->GetOverRideUserAgent());
#endif //!IMODE_EXTENSIONS
	if (!ua_str_len)
#if defined(UNIX)
		op_strcpy(UserAgentStr, "Mozilla/4.0 (Linux; US) Opera " VER_NUM_STR " [en]");
#elif defined(_MACINTOSH_)
		op_strcpy(UserAgentStr, "Mozilla/4.0 (Macintosh; US) Opera " VER_NUM_STR " [en]");
#else
		op_strcpy(UserAgentStr, "Mozilla/4.0 (Windows 98; US) Opera " VER_NUM_STR " [en]");
#endif

	//op_strcpy(UserAgentStr, "Opera/" VER_NUM_STR " (X11; SUSE 10.2; U; en)");
	//op_strcpy(UserAgentStr, "UA=Opera/8.01 (J2ME/MIDP; Opera Mini; en; U; ssr)");
	//op_strcpy(UserAgentStr, "Mozilla/4.73 [en] (WinNT; U) Opera 4.02");
	//op_strcpy(UserAgentStr, "Mozilla/4.72 [en] (WinNT; U) Opera 4.01");
	//op_strcpy(UserAgentStr, "Mozilla/4.0 (Windows 98; US) Opera 3.62 [en]");
	//op_strcpy(UserAgentStr, "Mozilla/4.0 (Windows 98; US) Opera 3.62 [en]");
	//op_strcpy(UserAgentStr, "Mozilla/4.0 (Windows 98; US) Opera 5.12 [en]");
	//op_strcpy(UserAgentStr, "Mozilla/4.0 (compatible; MSIE 5.01; EPOC)");
	//op_strcpy(UserAgentStr, "Mozilla/4.0 (compatible; MSIE 5.01; Windows 2000)");
	//op_strcpy(UserAgentStr, "Mozilla/4.0 (compatible; MSIE 6.0; Windows 2000)");
	//op_strcpy(UserAgentStr, "Mozilla/4.0 (compatible; MSIE 5.0; Windows 2000) Opera 7.0  [en]");
	//op_strcpy(UserAgentStr, "Mozilla/4.0 (compatible; MSIE 6.0; MSIE 5.5; Windows 2000) Opera 7.0  [en]");
	//op_strcpy(UserAgentStr, "Mozilla/4.51 [en] (WinNT; I)");
	//op_strcpy(UserAgentStr, "Mozilla/4.71 [en] (WinNT; I)");
	//op_strcpy(UserAgentStr, "Mozilla/4.71 [en] (WinNT; U)");
	//op_strcpy(UserAgentStr, "Mozilla/4.0 (compatible; MSIE 5.01; Linux) Opera 6.0 [en]");
	//op_strcpy(UserAgentStr, "Mozilla/4.0 (Linux; US) Opera 5.50 [en]");
	//op_strcpy(UserAgentStr, "Mozilla/5.0 (Windows 98; U; en; rv:1.7.5) Gecko/20041110");
	//op_strcpy(UserAgentStr, "Opera/6.05 (Windows 98; U) [en]");
	//op_strcpy(UserAgentStr, "Opra/6.02 (Windows 98; U) [en]");
	//op_strcpy(UserAgentStr, "JusticeDepartment/3.0 (Windows 98; U) [en]");
	//op_strcpy(UserAgentStr, "FunnyBrowser/3.0 (Windows 98; U) [en]");
	//op_strcpy(UserAgentStr, "FancyBrowser/3.0 (Windows 98; U) [en]");
	//op_strcpy(UserAgentStr, "Mozilla/4.0 (compatible; Opera 6.02; Windows 98)");
	//op_strcpy(UserAgentStr, "Mozilla/4.0 (faircompetition; Opera 6.02; Windows 98)");
	//op_strcpy(UserAgentStr, "Opera/6.02 (Windows 98; U) [en] Mozilla/4.0");

	Headers.AddParameterL("User-Agent", UserAgentStr);

#ifdef WEB_TURBO_MODE
	OpStringC8 hoststring1(request->use_proxy_passthrough ? g_obml_config->GetTurboProxyName8() : Origin_HostName());
	unsigned short use_port, fake_port;
#ifdef _SSL_SUPPORT_
	fake_port = (info.secure ? 443 : 80);
#else
	fake_port = 80;
#endif // _SSL_SUPPORT_
	use_port = (request->use_proxy_passthrough ? fake_port : Origin_HostPort()); // the proxy string already has port specified
#else
	OpStringC8 hoststring1(Origin_HostName());
	unsigned short use_port = Origin_HostPort();
#endif // WEB_TURBO_MODE
	const char *hoststring = hoststring1.CStr();

#ifdef _SSL_SUPPORT_
	if (hoststring1.HasContent() && use_port !=(info.secure ? 443 : 80))
#else
	if (hoststring1.HasContent() && use_port != 80)
#endif
	{
		if ((size_t) hoststring1.Length() + 50 > g_memory_manager->GetTempBufLen())
			LEAVE(OpStatus::ERR);

		char *hoststring2 = (char *)g_memory_manager->GetTempBuf();
		op_snprintf(hoststring2, g_memory_manager->GetTempBufLen(), "%s:%d", Origin_HostName(),Origin_HostPort());
		hoststring = hoststring2;
	}
	Headers.AddParameterL("Host", hoststring);

	if(GetMethod() == HTTP_METHOD_CONNECT)
	{
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
		if(!proxy_ntlm_auth_element)
#endif
		{
			Headers.AddParameterL("Proxy-Connection", "Keep-Alive");
#ifdef SUPPORT_RIM_MDS_AUTHENTICATION
			if( g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RIMMDSBrowserMode ) )
				Headers.ClearAndAddParameterL("X-MDS-Authorization",g_op_system_info->GetAppId());
#endif // SUPPORT_RIM_MDS_AUTHENTICATION
		}
	}
	else
	{
		const char *accept_str = g_pcnet->GetAcceptTypes();
		if(accept_str == NULL || *accept_str == '\0')
			accept_str = HTTP_Accept_str;

		Headers.AddParameterL("Accept", accept_str);

#ifndef DISABLE_WAP_ACCEPT_PARAMETERS
		if(request->origin_host->GetIsWAPServer())
		{
			Headers.AddParameterL("Accept", "text/vnd.wap.wml, image/vnd.wap.wbmp; level=0");
		}
#endif

#ifdef _HTTP_COMPRESS
#ifdef URL_ALLOW_DISABLE_COMPRESS
		if (!info.disable_compress)
#endif
			Headers.AddParameterL("Accept-Encoding", "gzip, deflate");
#endif

#if defined EXTERNAL_ACCEPTLANGUAGE
		extern const char* GetAcceptLanguage(URL_CONTEXT_ID);
		const char* lang = GetAcceptLanguage(context_id);
#else // EXTERNAL_ACCEPTLANGUAGE
		const char *lang = g_pcnet->GetAcceptLanguage();
#endif // EXTERNAL_ACCEPTLANGUAGE
		Headers.AddParameterL("Accept-Language", lang);

		BOOL global_DNT = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DoNotTrack);
		BOOL site_specific_DNT = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DoNotTrack, request->connect_host);
		if (global_DNT || site_specific_DNT)
		{
			// According to the specification, if Do-Not_track is "on",
			// but overridden to off for specific site, send "0"
			if (!site_specific_DNT)
				Headers.AddParameterL("DNT", "0");
			else
				Headers.AddParameterL("DNT", "1");
		}

#ifdef DRM_INFO
# ifdef PLATFORM_DRM_INFO
		const char* ver = g_op_system_info->GetDRMVersion();
		if( ver )
			Headers.ClearAndAddParameterL("DRM-Version", ver);
# else
		Headers.ClearAndAddParameterL("DRM-Version", DRM_VERSION_SUPPORTED);
# endif // PLATFORM_DRM_INFO
#endif // DRM_INFO
	}

#ifdef SUPPORT_RIM_MDS_AUTHENTICATION
		if( g_pcnet->GetIntegerPref(PrefsCollectionNetwork::RIMMDSBrowserMode ) )
			Headers.ClearAndAddParameterL("X-MDS-Authorization",g_op_system_info->GetAppId());
#endif // SUPPORT_RIM_MDS_AUTHENTICATION

#ifdef EXTERNAL_HTTP_HEADERS
	extern void SetExternalHttpHeadersL(
			URL_CONTEXT_ID,
			Header_List*, const uni_char* domain, BOOL connect, BOOL secure
# ifdef SET_EXT_HTTP_HEADERS_PATH_PORT
			, const char *path, unsigned short port
# endif // SET_EXT_HTTP_HEADERS_PATH_PORT
			);
	SetExternalHttpHeadersL(
			context_id, 
			&Headers, request->origin_host->UniName(), GetMethod() == HTTP_METHOD_CONNECT, info.secure
# ifdef SET_EXT_HTTP_HEADERS_PATH_PORT
			, request->path, request->origin_port
# endif // SET_EXT_HTTP_HEADERS_PATH_PORT
			);
#endif // EXTERNAL_HTTP_HEADERS
}

OP_STATUS HTTP_Request::SetUpMaster()
{
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
	BOOL sec_proxy = (info.secure && (request->connect_host != request->origin_host ||
		request->connect_port != request->origin_port));
	ServerName *host = (sec_proxy ? request->origin_host : request->connect_host);
	unsigned short port = (sec_proxy ? request->origin_port : request->connect_port);
	master = http_Manager->FindServer(host, port, info.secure, TRUE);
#else
	master = http_Manager->FindServer(request->connect_host, request->connect_port, TRUE);
#endif
	if(master == NULL)
		return OpStatus::ERR_NO_MEMORY;

#ifdef WEB_TURBO_MODE
	if( request->use_turbo )
		master->SetTurboEnabled();
#endif // WEB_TURBO_MODE

	return OpStatus::OK;
}

//***************************************************************************

HTTP_Request::~HTTP_Request()
{
	InternalDestruct();
}

void HTTP_Request::InternalDestruct()
{
#ifdef DEBUG_HTTP_FILE
	//PrintfTofile("winsck.txt","[%d] HTTP_req:~HTTP_req() - call_count=%d\n", id, call_count);
#endif
#ifdef DEBUG_HTTP_REQDUMP1
	PrintfTofile("httpreq.txt","Destroying Request %d Tick %lu\n",
		Id(), (unsigned long) g_op_time_info->GetWallClockMS());
#endif
	Stop();
	if((HTTP_Server_Manager*)master)
		master->RemoveRequest(this);

	OP_ASSERT(!GetSink());
	OP_ASSERT(!http_conn);
		//http_conn->ChangeParent(NULL);
	http_conn = NULL;
	mh->UnsetCallBacks(this);

	if(InList())
		Out();

#ifdef _ENABLE_AUTHENTICATE
#ifdef HTTP_DIGEST_AUTH
	ClearAuthentication();
#endif
#endif

		OP_DELETE(request);
#ifdef DEBUG_HTTP_REQDUMP
		OpString8 textbuf1;

		textbuf1.AppendFormat("htpr%04d.txt",Id());
		PrintfTofile(textbuf1,"Destroying Request %d Tick %lu\n",Id(), (unsigned long) g_op_time_info->GetWallClockMS());
		CloseDebugFile(textbuf1);
#endif
}

CommState HTTP_Request::Load()
{
	CommState comm_state = COMM_REQUEST_FAILED;
	if((HTTP_Server_Manager*)master)
		comm_state =  master->AddRequest(this);

	if (comm_state != COMM_REQUEST_FAILED && request)
		ProtocolComm::SetMaxTCPConnectionEstablishedTimeout(m_tcp_connection_established_timout);

	return comm_state;
}

void HTTP_Request::Clear()
{
	//http_conn = NULL;
    SetHeaderLoaded(FALSE);
	SetSentRequest(FALSE);
	SetSendingRequest(FALSE);
	SetWaiting(FALSE);
	info.proxy_connect_open = FALSE;
	info.read_secondary = FALSE;
#ifdef HTTP_AGGRESSIVE_PIPELINE_TIMEOUT
	info.sent_pipelined = FALSE;
#endif
	//previous_load_difference = 0;
	//previous_load_length = 0;
	//sendcount = 0;
#ifdef WEB_TURBO_MODE
	info.first_turbo_request = 0;
	info.turbo_proxy_auth_retry = 0;
#endif // WEB_TURBO_MODE

	header_info->Clear();
}

HeaderInfo* HTTP_Request::GetHeader()
{
#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
	return (secondary_request && !info.sent_request ? secondary_request->header_info : header_info);
#else
	return header_info;
#endif
}

OP_STATUS HTTP_Request::SetExternalHeaderInfo(HeaderInfo *hi)
{
	header_info = hi;

	if(!hi)
	{
		header_info = OP_NEW(HeaderInfo, (method));
		if(header_info == NULL)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

//***************************************************************************
#ifdef _ENABLE_AUTHENTICATE

void HTTP_Request::SetAuthorization(const OpStringC8 &auth)
{
	Headers.ClearAndAddParameterL("Authorization", auth);

#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	if(proxy_ntlm_auth_element)
		Headers.ClearHeader("Proxy-Authorization");
#endif
}

//***************************************************************************

void HTTP_Request::SetProxyAuthorization(const OpStringC8 &auth)
{
#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
	if(secondary_request)
	{
		secondary_request->SetProxyAuthorization(auth);
		return;
	}
#endif

	Headers.ClearAndAddParameterL("Proxy-Authorization", auth);
}

unsigned long HTTP_Request::GetProxyAuthorizationId()
{
#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
	if(secondary_request)
		return secondary_request->GetProxyAuthorizationId();
	else
#endif
		return proxy_auth_id;
}
void HTTP_Request::SetProxyAuthorizationId(unsigned long aid)
{
#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
	if(secondary_request)
		secondary_request->SetProxyAuthorizationId(aid);
	else
#endif
	{
		proxy_auth_id=aid;
	}
}

/****************/
#endif

//***************************************************************************

void HTTP_Request::SetCookieL(OpStringC8 str,
			int version, int max_version
		)
{
	int cookie_version = (max_version > version	|| max_version == 0 ? max_version : version);

	Headers.ClearHeader("Cookie");
	Headers.ClearHeader("Cookie2");

	if(str.Length() == 0)
		return;

	if (max_version > 0)
	{
		OpString8 temp_var;
		temp_var.AppendFormat("%d", max_version);
		Headers.SetSeparatorL("Cookie", SEPARATOR_SEMICOLON);
		Headers.AddParameterL("Cookie", "$Version", temp_var);
	}

	if(cookie_version != 1 && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableCookiesV2))
		Headers.AddParameterL("Cookie2", "$Version", "1");

	Headers.AddParameterL("Cookie", str);
}


#ifdef _URL_EXTERNAL_LOAD_ENABLED_
OP_STATUS HTTP_Request::AddCookies(OpStringC8 str)
{
	return ex_cookies.Set(str);
}

OP_STATUS HTTP_Request::AddConnectionParameter(OpStringC8 str)
{
	if(ex_connection_header.HasContent())
		RETURN_IF_ERROR(ex_connection_header.Append(", "));
	return ex_connection_header.Append(str);
}

OP_STATUS HTTP_Request::AppendUploadData(const void *buf, int bufsize)
{
	OP_ASSERT(upload_data && info.ex_is_external);
	if (!upload_data || !info.ex_is_external)
		return OpStatus::ERR;;

	// Not very happy about this type cast:
	RETURN_IF_ERROR(((Upload_AsyncBuffer *)upload_data)->AppendData(buf, bufsize));
	if (info.upload_buffer_underrun)
	{
		info.upload_buffer_underrun = FALSE;
		OP_ASSERT(http_conn);
		if (http_conn)
			http_conn->RequestMoreData();
	}
	return OpStatus::OK;
}
#endif // _URL_EXTERNAL_LOAD_ENABLED_

void HTTP_Request::SetExternalHeaders(class Header_List *headers)
{
	if (headers)
	{
		headers->SetSeparator(SEPARATOR_COMMA);

		Headers.InsertHeaders(*headers, HEADER_INSERT_LAST, NULL, TRUE);
		OP_DELETE(headers);
	}
}

OP_STATUS HTTP_Request::CopyExternalHeadersToUrl(URL &url)
{
	Header_Item *header = Headers.First();

#ifdef CORS_SUPPORT
	UINT32 cors_redirect = url.GetAttribute(URL::KFollowCORSRedirectRules);
#endif // CORS_SUPPORT

	while (header)
	{
		if (header->IsExternal())
		{
			URL_Custom_Header new_header;
			RETURN_IF_ERROR(new_header.name.Set(header->GetName().CStr()));

			if (new_header.value.Reserve(header->CalculateLength() + 1) == NULL)
				return OpStatus::ERR_NO_MEMORY;
			header->GetValue(new_header.value.CStr());

			char *crlf = op_strstr(new_header.value.CStr(), "\r\n");
			if (crlf)
			{
				*crlf = '\0';
			}

#ifdef CORS_SUPPORT
			// Do not copy the old origin header if it is a cors
			// redirect since this header is set by
			// OpCrossOrigin_Request::PrepareRequestURL.
			if (cors_redirect != URL::CORSRedirectVerify || op_stricmp("Origin", new_header.name.CStr()))
#endif // CORS_SUPPORT
				url.SetAttribute(URL::KAddHTTPHeader, &new_header);
		}

		header = header->Suc();
	}

	return OpStatus::OK;
}

void HTTP_Request::SetSendAcceptEncodingL(BOOL flag)
{
	Headers.HeaderSetEnabledL("Accept-Encoding", flag);
}

//***************************************************************************

void HTTP_Request::SetRefererL(OpStringC8 ref)
{
	Headers.ClearHeader("Referer");
	if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ReferrerEnabled, request->origin_host))
	{
		Headers.ClearAndAddParameterL("Referer", ref);
	}
}

//***************************************************************************

void HTTP_Request::SetDataL(OpStringC8 dat, BOOL with_headers)
{
	OpStackAutoPtr<Upload_BinaryBuffer> buffer(OP_NEW_L(Upload_BinaryBuffer, ()));

	buffer->InitL((unsigned char *) dat.DataPtr(), dat.Length(), (with_headers ? UPLOAD_COPY_EXTRACT_HEADERS : UPLOAD_COPY_BUFFER));
	buffer->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION);
	SetElement(buffer.release(),TRUE, FALSE, TRUE);
}

//***************************************************************************

#ifdef HAS_SET_HTTP_DATA
void HTTP_Request::SetData(Upload_Base* dat)
{
	SetElement(dat,FALSE, FALSE, TRUE);
}
#endif

//***************************************************************************
void HTTP_Request::SetRequestContentTypeL(OpStringC8 ct)
{
	OpStringC8 def_form_type("application/x-www-form-urlencoded");

	Headers.ClearAndAddParameterL("Content-Type", (ct.IsEmpty() || ct.FindFirstOf('/') != KNotFound ? ct : def_form_type));

	/* FIXME: Temporarily disabling below block of code in favor of line above to fix CORE-34390 regression. */
	/*
	if (GetElement())
		GetElement()->ClearAndSetContentTypeL(ct.IsEmpty() || ct.FindFirstOf('/') != KNotFound ? ct : def_form_type);
	*/
}

//***************************************************************************

/*
const char* HTTP_Request::GetDate() const
{
  return header_info->date;
}
*/

//***************************************************************************

void HTTP_Request::SetIfModifiedSinceL(OpStringC8 mod)
{
	Headers.ClearAndAddParameterL("If-Modified-Since", mod);
}

void HTTP_Request::SetIfRangeL(OpStringC8 ent)
{
	Headers.ClearAndAddParameterL("If-Range", ent);

}

#ifdef _HTTP_USE_IFMATCH
void HTTP_Request::SetIfMatchL(OpStringC8 &ent)
{
	Headers.ClearAndAddParameterL("If-Match", ent);

}
#endif

void HTTP_Request::SetIfNoneMatchL(OpStringC8 ent)
{
	Headers.ClearAndAddParameterL("If-None-Match", ent);
}
//***************************************************************************
const char *HTTP_Request::Origin_HostName() const
{
	return request->origin_host->Name();
}

unsigned short HTTP_Request::Origin_HostPort() const
{
	return request->origin_port;
}

//***************************************************************************

unsigned HTTP_Request::ReadData(char *buf, unsigned blen)
{
	unsigned ret=0;
#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
	/*
	if(info.read_secondary)
		ret = secondary_request ? secondary_request->ReadData(buf,blen) : 0;
	else
	*/
#endif
#ifdef _HTTP_COMPRESS
	if(!TE_decoding.Empty())
	{
		HTTP_Transfer_Decoding *decoder = (HTTP_Transfer_Decoding *) TE_decoding.First();

		BOOL readconn = FALSE;
		ret = decoder->ReadData(buf, blen, http_conn, readconn);

		if(decoder->Error())
		{
			EndLoading();
			mh->PostMessage(MSG_COMM_LOADING_FAILED,Id(),URL_ERRSTR(SI, ERR_HTTP_DECODING_FAILURE));
			return 0;
		}

		if((!http_conn && !decoder->Finished()) || (http_conn && !readconn))
		{
#ifdef DEBUG_HTTP_FILE
			//PrintfTofile("http.txt","Sending MSG_COMM_DATA_READY [%d] #4\n", Id());
#endif
			// Delay if there is a connection, and the decoder is finished, to let the connection handle this as normal
			mh->PostDelayedMessage(MSG_COMM_DATA_READY, Id(), 0, (http_conn && decoder->Finished() ? 200 : 0 ));
		}
		else if(!http_conn && decoder->Finished())
			mh->PostMessage(MSG_COMM_LOADING_FINISHED,Id(),0);
	}
	else
#endif
	if(http_conn)
		ret = http_conn->ReadData(buf,blen);
	else
		ret = ProtocolComm::ReadData(buf, blen);

#ifdef HTTP_DIGEST_AUTH
	ProcessEntityBody((const unsigned char *) buf,ret);;
#endif

	return ret;
}

void HTTP_Request::Stop()
{
	if(http_conn)
		http_conn->RemoveRequest(this);
	if(InList())
		master->RemoveRequest(this);
	OP_ASSERT(!GetSink());
	http_conn = NULL;
}

void HTTP_Request::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	NormalCallCount blocker(this);
									__DO_START_TIMING(TIMING_COMM_LOAD_HT);

	switch (msg)
	{
		/*
		case MSG_HTTP_HEADER_LOADED :
			SetProgressInformation(HEADER_LOADED);
			break;
		*/
		case MSG_COMM_LOAD_OPTIONS_FINISHED:
			mh->UnsetCallBack(this, MSG_COMM_LOAD_OPTIONS_FINISHED);
			Load();
			break;

		case MSG_COMM_DATA_READY :
#ifdef DEBUG_HTTP_FILE
			//PrintfTofile("http.txt","handling MSG_COMM_DATA_READY [%d] #4\n", Id());
#endif
			ProcessReceivedData();
			//mh->PostMessage(msg,Id(),par2);
			break;

		case MSG_COMM_LOADING_FINISHED :
			LoadingFinished();
			break;

		case MSG_COMM_LOADING_FAILED :
			if(http_conn)
			{
				http_conn->RemoveRequest(this);
				http_conn = NULL;
			}
			else if (master)
				master->RemoveRequest(this);
#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
			secondary_request = NULL;
#endif
			mh->PostMessage(msg, Id(), par2);
			break;
	}
									__DO_STOP_TIMING(TIMING_COMM_LOAD_HT);
}


void HTTP_Request::LoadingFinished(BOOL send_msg)
{
#ifdef DEBUG_HTTP_REQDUMP1
	PrintfTofile("httpreq.txt","Finished Loading Request %d\n",
		Id());
#endif

	if(http_conn)
		http_conn->OnLoadingFinished();

	http_conn = NULL;
#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
	if(secondary_request)
		secondary_request = NULL;
#endif
	if(send_msg)
	{
#ifdef _HTTP_COMPRESS
		HTTP_Transfer_Decoding *decoder = (HTTP_Transfer_Decoding *) TE_decoding.First();
		if(decoder && !decoder->Finished())
		{
#ifdef DEBUG_HTTP_FILE
			//PrintfTofile("http.txt","Sending MSG_COMM_DATA_READY [%d] #5:\n", Id());
#endif
			mh->PostMessage(MSG_COMM_DATA_READY, Id(), 0);
		}
		else
#endif
		{
			mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
			info.loading_finished = 1;
		}

	}
	mh->UnsetCallBacks(this);
}

void HTTP_Request::EndLoading()
{
#ifdef SCOPE_RESOURCE_MANAGER
	if(!has_sent_response_finished)
	{
		SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_RESPONSE_FINISHED, this);
		has_sent_response_finished = TRUE;
	}
#endif // SCOPE_RESOURCE_MANAGER

	master->RemoveRequest(this);
	//if(http_conn)
	//	http_conn->EndLoading();
}

void HTTP_Request::RequestMoreData()
{
	if(method == HTTP_METHOD_CONNECT)
		ProtocolComm::RequestMoreData();
	//if(http_conn)
	//	http_conn->RequestMoreData();
}

void HTTP_Request::SendData(char *buf, unsigned blen)
{
	if(http_conn)
		http_conn->SendData(buf, blen);
	else
		if( buf )
			OP_DELETEA(buf);
}


CommState HTTP_Request::ConnectionEstablished()
{
	return ProtocolComm::ConnectionEstablished();
}


// *************************************************************

const char*	HTTP_Request::GetMethodString() const
{
	return GetMethod()<= HTTP_METHOD_MAX ? g_HTTP_Method_Strings[method] : method_string;
}

const char*	HTTP_Request::GetSchemeString() const
{
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
	return g_HTTP_Scheme_Strings[info.secure ? 1 : 0];
#else
	return g_HTTP_Scheme_Strings[0];
#endif // defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
}

void HTTP_Request::PrepareHeadersL()
{
	if(use_nettype != NETTYPE_UNDETERMINED &&
		((!info.proxy_request && use_nettype != master->HostName()->GetNetType())|| // if not proxied the nettypes must match, or we do not send referrer
		(info.proxy_request && use_nettype <= NETTYPE_PRIVATE))) // Don't send referrer through proxy for private and localhost servers.
	{
		// If we know, or suspect, that a host is on a different net than the referrer, then we do not send the referrer header to prevent information leaks
		Headers.ClearHeader("Referer");
	}

	loading_started = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);

	info.send_expect_100_continue = FALSE;
	if (GetMethod() != HTTP_METHOD_CONNECT &&
		(GetMethodHasRequestBody(GetMethod()) || !GetMethodIsSafe(GetMethod())))
	{
		OpFileLength blen = PayloadLength();
		OpString8 temp_var;
		ANCHOR(OpString8, temp_var);

		LEAVE_IF_ERROR(g_op_system_info->OpFileLengthToString(blen, &temp_var));
		Headers.AddParameterL("Content-Length", temp_var);
		info.send_100c_body = FALSE;
	}

	if(GetProxyNoCache())
	{
		if(http_conn && (http_conn->info.host_is_1_0 || !http_conn->info.http_protocol_determined))
			Headers.ClearAndAddParameterL("Pragma", "no-cache");
		Headers.ClearAndAddParameterL("Cache-Control", "no-cache");
	}

	if(GetMethod() != HTTP_METHOD_CONNECT)
	{
		if(info.proxy_request &&
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
			!info.secure &&
#endif
			(http_conn && (http_conn->info.host_is_1_0 || !http_conn->info.http_protocol_determined)))
		{
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
			if(proxy_ntlm_auth_element) // To be on the safe side
				info.send_close = FALSE;
#endif
			Headers.ClearAndAddParameterL("Proxy-Connection", (info.send_close ? "close" : "Keep-Alive"));
			Headers.ClearHeader("Connection");
		}
		else
		{
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
			if(proxy_ntlm_auth_element || server_negotiate_auth_element) // To be on the safe side
				info.send_close = FALSE;
#endif
			Headers.ClearAndAddParameterL("Connection", (info.send_close ? "close" : "Keep-Alive"));

//do not announce TE support, see CORE-29211
#ifdef UNUSED_CODE
			if(info.proxy_request || http_conn->info.http_protocol_determined || http_conn->info.host_is_1_0)
			{
				Headers.AddParameterL("Connection", "TE");

				Headers.AddParameterL("TE",
#ifdef _HTTP_COMPRESS
									"deflate, gzip, "	// compiletime concatenation
#endif
									"chunked, identity, trailers" ); // compiletime concatenation
			}
#endif //  UNUSED_CODE
		}

#ifdef _URL_EXTERNAL_LOAD_ENABLED_
		Headers.AddParameterL("Connection", ex_connection_header);
#endif
	}

	if(info.use_range_start || info.use_range_end)
	{
		Headers.ClearHeader("Range");

		OpString8 temp_var;
		ANCHOR(OpString8, temp_var);

		OpString8 temp_start;
		ANCHOR(OpString8, temp_start);
		OpString8 temp_end;
		ANCHOR(OpString8, temp_end);

		if(range_start!=FILE_LENGTH_NONE)
			LEAVE_IF_ERROR(g_op_system_info->OpFileLengthToString(range_start, &temp_start));
		if(info.use_range_end)
			LEAVE_IF_ERROR(g_op_system_info->OpFileLengthToString(range_end, &temp_end));

		LEAVE_IF_ERROR(temp_var.SetConcat(temp_start, "-", temp_end));
		Headers.AddParameterL("Range", "bytes", temp_var);
	}

#ifdef _EMBEDDED_BYTEMOBILE
	if (http_conn)
	{
		Headers.ClearHeader("X-EBO-UA");
		Headers.ClearHeader("X-EBO-Info");
		Headers.ClearHeader("X-EBO-Req");
		//X-EBO-UA header
		//Used to discover EBO proxies
		char *eboTempHeader = (char *) g_memory_manager->GetTempBuf2k();
		if (!GetLoadDirect() && !urlManager->GetEmbeddedBmOpt() && http_conn->info.first_request && !urlManager->GetEmbeddedBmDisabled() && GetMethod() != HTTP_METHOD_CONNECT)
		{
			op_snprintf(eboTempHeader,  g_memory_manager->GetTempBuf2kLen(),
				"BID=" BM_BID ", BCReq=%s",
				http_conn->GetBMInformationProvider().generateBCReqL()
				);

			Headers.AddParameterL("X-EBO-UA", eboTempHeader);
		}

		//X-EBO-Info header
		OpString8 ebo_guid;
		ANCHOR(OpString8, ebo_guid);
		http_conn->GetBMInformationProvider().getOperaGUIDL(ebo_guid);
		LayoutMode mode = LAYOUT_NORMAL;
		if (http_conn->GetEboOptimized())
		{
			Window *window = 0;
			SetProgressInformation(GET_ORIGINATING_CORE_WINDOW, 0, &window);
			if (window)
			{
				mode = window->GetLayoutMode();
			}
		}

		if (!GetLoadDirect() &&  http_conn->GetEboOptimized() && GetMethod() != HTTP_METHOD_CONNECT && !http_conn->GetEBOOCReq().Length())
		{
			//We need to know what screen we are using to get screen size, not so trivial on mutiscreen devices
			Window *window = NULL;
			SetProgressInformation(GET_ORIGINATING_CORE_WINDOW, 0, &window);
			OpScreenProperties sp;
			g_op_screen_info->GetProperties(&sp, (window ? window->GetOpWindow() : NULL));

			op_snprintf(eboTempHeader,  g_memory_manager->GetTempBuf2kLen(),
				"BID=" BM_BID ", UID=%s, Settings=%X, Screen=%ux%u, BCReq=%s, BSettings=%u",
				ebo_guid.CStr(),
				http_conn->GetBMInformationProvider().getSettings(),
				sp.width, sp.height,
				http_conn->GetBMInformationProvider().generateBCReqL(),
				(unsigned)mode
				);

			Headers.AddParameterL("X-EBO-Info", eboTempHeader);
			if (http_conn && http_conn->GetSink() && urlManager->GetEmbeddedBmCompressed() && http_conn->info.first_request && !GetLoadDirect())
				LEAVE_IF_ERROR(http_conn->GetSink()->SetZLibTransceive());
		}
		else if (!http_conn->info.ebo_authenticated && http_conn->GetEBOOCReq().Length() && !urlManager->GetEmbeddedBmDisabled())
		{
			//We need to know what screen we are using to get screen size, not so trivial on mutiscreen devices
			Window *window = NULL;
			SetProgressInformation(GET_ORIGINATING_CORE_WINDOW, 0, &window);
			OpScreenProperties sp;
			g_op_screen_info->GetProperties(&sp, (window ? window->GetOpWindow() : NULL));

			OpString8 eboServ;
			ANCHOR(OpString8, eboServ);
			eboServ.SetUTF8FromUTF16(urlManager->GetEBOServer());

			op_snprintf(eboTempHeader, g_memory_manager->GetTempBuf2kLen(),
				"BID=" BM_BID ", UID=%s, Settings=%X, Screen=%ux%u, OCResp=%s, BSettings=%u",
				ebo_guid.CStr(),
				http_conn->GetBMInformationProvider().getSettings(),
				sp.width, sp.height,
				http_conn->GetBMInformationProvider().generateOCRespL(eboServ.CStr(), http_conn->GetEBOOCReq().CStr()),
				(unsigned)mode

			);
			http_conn->info.ebo_authenticated = TRUE;

			Headers.AddParameterL("X-EBO-Info", eboTempHeader);
		}

		//X-EBO-Req header
		if (!GetLoadDirect() && http_conn->GetEboOptimized() &&  GetMethod() != HTTP_METHOD_CONNECT)
		{
			SetRequestNumber(urlManager->GetNextRequestSeqNumber());
			if (!request->predicted)
				op_snprintf(eboTempHeader, g_memory_manager->GetTempBuf2kLen(), "Key=%d", GetRequestNumber());
			else
				op_snprintf(eboTempHeader, g_memory_manager->GetTempBuf2kLen(), "Key=%d, Flags=2", GetRequestNumber());

			Headers.AddParameterL("X-EBO-Req", eboTempHeader);
		}
#endif // _EMBEDDED_BYTEMOBILE
#if defined(SCOPE_HTTP_LOGGER) || defined(WEB_TURBO_MODE)
#ifdef _EMBEDDED_BYTEMOBILE
		else
#endif // _EMBEDDED_BYTEMOBILE
			SetRequestNumber(urlManager->GetNextRequestSeqNumber());
#endif // (SCOPE_HTTP_LOGGER) || (WEB_TURBO_MODE)
#ifdef _EMBEDDED_BYTEMOBILE
	}
#endif // _EMBEDDED_BYTEMOBILE
#ifdef DEVICE_STOCK_UA_SUPPORT
	// Spec says this should only be sent for the main document, not inlines
	if (info.usage_indication == HTTP_UsageIndication_MainDocument)
	{
		UA_BaseStringId effective_ua = request->origin_host->GetOverRideUserAgent();
		if (effective_ua == UA_NotOverridden)
			effective_ua = g_uaManager->GetUABaseId();

		// Spec says this should only be sent when we're not spoofing the UA.
		if (effective_ua == UA_Opera)
		{
			OpString8 stock_ua;
			if (OpStatus::IsSuccess(g_op_system_info->GetDeviceStockUA(&stock_ua)))
				Headers.AddParameterL("Device-Stock-UA", stock_ua);
		}
	}
#endif // DEVICE_STOCK_UA_SUPPORT

#ifdef WEB_TURBO_MODE
	if( request->use_turbo && urlManager->GetWebTurboAvailable() && !GetLoadDirect() )
	{
		Headers.ClearHeader("X-Opera-Info");
		Headers.ClearHeader("X-Opera-Host");
		Headers.ClearHeader("X-Opera-ID");
		Headers.ClearHeader("X-OA");
		Headers.ClearHeader("X-OB");

# ifdef URL_TURBO_MODE_HEADER_REDUCTION
		BOOL reduce_headers =
# ifdef _EMBEDDED_BYTEMOBILE
			!urlManager->GetEmbeddedBmOpt() &&
# endif // _EMBEDDED_BYTEMOBILE
			!request->use_proxy_passthrough &&
			request->IsTurboProxy() &&
			!info.disable_headers_reduction;

		if( reduce_headers )
			http_conn->ReduceHeadersL(Headers);
# else
		BOOL reduce_headers = FALSE;
# endif // URL_TURBO_MODE_HEADER_REDUCTION

		// Set request number for OOO request handling in HTTP pipelines
		if( GetRequestNumber() == 0 )
			SetRequestNumber(urlManager->GetNextRequestSeqNumber());

		// Set up feature indicator
		UINT32 features = 0;
		if( reduce_headers )
			features |= WEB_TURBO_FEATURE_REQUEST_HEADER_REDUCTION | WEB_TURBO_FEATURE_RESPONSE_HEADER_REDUCTION;
		if( g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled) )
			features |= WEB_TURBO_FEATURE_ECMASCRIPT;
# ifdef _PLUGIN_SUPPORT_
		if( g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled) )
			features |= WEB_TURBO_FEATURE_PLUGINS;
# endif
		
		// Set request ID and device info for Turbo Mode proxy
#ifdef HTTP_CONTENT_USAGE_INDICATION
		HTTP_ContentUsageIndication usage = GetUsageIndication();
#else
		UINT32 usage = 0;
#endif // HTTP_CONTENT_USAGE_INDICATION

		char *tmpBuf = (char *) g_memory_manager->GetTempBuf2k();

		//We need to know what screen we are using to get screen size, not so trivial on mutiscreen devices
		Window *window = NULL;
		SetProgressInformation(GET_ORIGINATING_CORE_WINDOW, 0, &window);
		OpScreenProperties sp;
		g_op_screen_info->GetProperties(&sp, (window ? window->GetOpWindow() : NULL));

		op_snprintf(tmpBuf, g_memory_manager->GetTempBuf2kLen(), "ID=%d, p=%d, f=%d, sw=%d, sh=%d",
			GetRequestNumber(), usage, features, sp.width, sp.height);
		Headers.AddParameterL("X-Opera-Info", tmpBuf);

		if(http_conn && (http_conn->info.first_request && !info.sent_pipelined || http_conn->info.turbo_proxy_auth_retry) || info.first_turbo_request || info.turbo_proxy_auth_retry)
		{
			// Send Turbo ID and OBML brand headers only with first request on connection
			if(http_conn && !http_conn->info.turbo_proxy_auth_retry || !http_conn && !info.turbo_proxy_auth_retry)
			{
				const char* id = g_obml_id_manager->GetTurboClientId();
				if( id )
					Headers.AddParameterL("X-Opera-ID",id);

				const char* brand = g_obml_config->GetBranding(); 
				if( brand )
					Headers.AddParameterL("X-OB",brand);
			}

			// Authenticate first request
			OpString8 auth;
			if( OpStatus::IsSuccess(g_obml_id_manager->CreateTurboProxyAuth(auth, http_conn ? http_conn->Id() : turbo_auth_id)) )
				Headers.AddParameterL("X-OA",auth.CStr());

			// Reset retry indicator
			if (http_conn)
				http_conn->info.turbo_proxy_auth_retry = FALSE;
			else
				info.turbo_proxy_auth_retry = FALSE;
		}

		// Send Turbo Host header if available
		const char* turbo_host = master->GetTurboHost();
		if( turbo_host )
			Headers.AddParameterL("X-Opera-Host",turbo_host);
	
	}
#endif // WEB_TURBO_MODE
}

char* HTTP_Request::ComposeRequestL(unsigned& len)
{
	OpStackAutoPtr<Header_Item> request_line(OP_NEW_L(Header_Item, ()));

#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	if(info.do_ntlm_authentication && method != HTTP_METHOD_CONNECT)
	{
		SetProgressInformation(RESTART_LOADING, 0, 0);
	}
#endif

	request_line->InitL(NULL);
	request_line->SetSeparator(SEPARATOR_SPACE);

	if (GetMethod()<= HTTP_METHOD_MAX)
		request_line->AddParameterL(g_HTTP_Method_Strings[GetMethod()]);
	else if(GetMethod() == HTTP_METHOD_String)
		request_line->AddParameterL(method_string);
	else
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);
#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
	if(GetMethod() == HTTP_METHOD_CONNECT)
	{
		OpString8 temp_var;
		temp_var.AppendFormat("%s:%d", request->origin_host->Name(), request->origin_port);
		request_line->AddParameterL(temp_var);
	}
	else
#endif
#ifdef _EMBEDDED_BYTEMOBILE
	if (!urlManager->GetProxy(request->origin_host, URL_HTTP) && urlManager->GetEmbeddedBmOpt())
	{
		request_line->AddParameterL(request->request.CStr() && *request->path ? request->path : "/");
	}
	else
#endif
	{
		request_line->AddParameterL(request->request);
	}

	const char *http_ver = HTTPVersion_1_1;

#ifdef _BYTEMOBILE
	if (info.proxy_request &&
		g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableHTTP11ForProxy))
		master->SetHTTP_ProtocolDetermined(TRUE);
	else
#endif
	if(info.proxy_request &&
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		!info.secure &&
#endif
		(!master->GetHTTP_ProtocolDetermined() || !master->GetHTTP_1_1())
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
		&& !proxy_ntlm_auth_element // NTLM auth assumes that the proxy is HTTP 1.1 and supports persistent connection
		&& !server_negotiate_auth_element // NTLM auth assumes that the proxy is HTTP 1.1 and supports persistent connection
#endif
		)
	{
		if( !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableHTTP11ForProxy)
#ifdef _EMBEDDED_BYTEMOBILE
			 && !urlManager->GetEmbeddedBmOpt()
#endif
#ifdef WEB_TURBO_MODE
			&& !(request->use_turbo && !request->use_proxy_passthrough)
#endif
			)
		{
			// Force Opera to use HTTP 1.0 against the proxy
			http_conn->info.http_protocol_determined = TRUE;
			http_conn->info.host_is_1_0 = TRUE;
			master->SetHTTP_ProtocolDetermined(TRUE);
			master->SetHTTP_1_1(FALSE);
		}

		/*if(!master->GetHTTP_ProtocolDetermined())
		{
			info.send_close = TRUE;
			http_conn->info.disable_more_requests = TRUE;
		}*/
		if((!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableStartWithHTTP11ForProxy) || !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableHTTP11ForProxy))
#ifdef _EMBEDDED_BYTEMOBILE
			&& !urlManager->GetEmbeddedBmOpt()
#endif //_EMBEDDED_BYTEMOBILE
#ifdef WEB_TURBO_MODE
			&& !(request->use_turbo && !request->use_proxy_passthrough)
#endif
			)
			http_ver = HTTPVersion_1_0;

#ifdef _EMBEDDED_BYTEMOBILE
		if (GetLoadDirect() || !urlManager->GetEmbeddedBmOpt())
#endif //_EMBEDDED_BYTEMOBILE
		{
			info.send_expect_100_continue = FALSE;
		}
	}

	request_line->AddParameterL(http_ver);

	if(Headers.First() && Headers.First()->GetName().IsEmpty())
		OP_DELETE(Headers.First());

	request_line->IntoStart(&Headers);
	
	Headers.ClearHeader("Connection");
	Headers.ClearHeader("TE");
	Headers.ClearHeader("Content-Length");
	Headers.ClearHeader("Expect");

	PrepareHeadersL();

#ifdef SCOPE_RESOURCE_MANAGER
	int orig_request_number = GetRequestNumber();
	HTTP_Request::LoadStatusData_Compose load_status_param(this, orig_request_number);
	SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_COMPOSE_REQUEST, &load_status_param);
#endif // SCOPE_RESOURCE_MANAGER

	uint32 req_buf_size = PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION) +256;
	if (GetMethodHasRequestBody(GetMethod()))
	{
		uint32 net_buf_size = (uint32)g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize)*1024;
		if (req_buf_size < net_buf_size)
			req_buf_size = net_buf_size;
	}
	uint32 remaining_len = req_buf_size;
	BOOL done = FALSE;

	unsigned char *req = OP_NEWA_L(unsigned char, req_buf_size);

	unsigned char *target = OutputHeaders(req, remaining_len, done);

	OP_ASSERT(done);
	if(!done)
		LEAVE(OpStatus::ERR);

	if (GetMethodHasRequestBody(GetMethod()))
	{
		// Request contains body that we can add right away to be sent in the first package
#ifdef DEBUG_ENABLE_OPASSERT
		target =
#endif
			OutputContent(target, remaining_len, done);
	}

	len = req_buf_size - remaining_len;

	OP_ASSERT((uint32) (target - req) == len);

	// *** 13/10/97 YNP ***
#ifdef DEBUG_HTTP_HEXDUMP
#ifndef DEBUG_HTTP_HEXDUMP_ONLY_INDATA
	if(req)
	{
		OpString8 textbuf;

		textbuf.AppendFormat("ComposeRequest Sending data from %d (request ID %d) Tick %lu", http_conn->Id(), Id(), (unsigned long) g_op_time_info->GetWallClockMS());
		DumpTofile((unsigned char *) req,(unsigned long) len,textbuf,"http.txt");
#ifdef DEBUG_HTTP_REQDUMP
		OpString8 textbuf1;

		textbuf1.AppendFormat("http%04d.txt",http_conn->Id());
		DumpTofile((unsigned char *) req,(unsigned long) len,textbuf,textbuf1);
		OpString8 textbuf2;

		textbuf2.AppendFormat("htpr%04d.txt",Id());
		DumpTofile((unsigned char *) req,(unsigned long) len,textbuf,textbuf2);
#endif
	}
	req[len] = '\0';
	PrintfTofile("http.txt", "ComposeRequest Sending data from %d (request ID %d)\n-----------------------\n%s\n-----------------------\n",http_conn->Id(), Id(),req);
	PrintfTofile("headers.txt", "ComposeRequest Sending data from %d (request ID %d)\n-----------------------\n%s\n-----------------------\n",http_conn->Id(), Id(),req);
#endif
#ifdef DEBUG_HTTP_REQDUMP1
	PrintfTofile("httpreq.txt","ComposeRequest Sending Request %d on connection %d Tick %lu\n",
		Id(), http_conn->Id(), (unsigned long) g_op_time_info->GetWallClockMS());
#endif
#endif
	// ********************

	SetProgressInformation(START_REQUEST,0, master->HostName()->UniName());
	SetProgressInformation(ACTIVITY_CONNECTION , 0, NULL);
	return (char *) req;
}

OP_STATUS HTTP_Request::SetCallbacks(MessageObject* master, MessageObject* parent)
{
	static const OpMessage messages[] =
    {
		MSG_COMM_DATA_READY,
        MSG_COMM_LOADING_FINISHED,
        MSG_COMM_LOADING_FAILED
    };

	if (parent && parent != master)
        RETURN_IF_ERROR(mh->SetCallBackList(parent, Id(), messages, ARRAY_SIZE(messages)));

	if(method != HTTP_METHOD_CONNECT)
		RETURN_IF_ERROR(mh->SetCallBackList(master, Id(), messages, ARRAY_SIZE(messages)));

	return ProtocolComm::SetCallbacks(master, this);
}

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
BOOL HTTP_Request::MoveToNextProxyCandidate()
{
#ifdef _SSL_SUPPORT_
	//delete secondary_request;
	if(secondary_request)
		secondary_request = NULL;
#endif // _SSL_SUPPORT_

	if(!request->current_connect_host)
		return FALSE;

	request->connect_host->SetFailedProxy(request->connect_port);
	request->current_connect_host = (ProxyListItem *) request->current_connect_host->Suc();
	if(request->current_connect_host)
	{
		request->connect_host = request->current_connect_host->proxy_candidate;
		request->connect_port = request->current_connect_host->port;
		request->proxy = request->current_connect_host->type;
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		// Note that this is somewhat more complicated in Construct, and maybe should be so here as well.
		master = http_Manager->FindServer(request->connect_host, request->connect_port, info.secure, TRUE);
#else
		master = http_Manager->FindServer(request->connect_host, request->connect_port, TRUE);
#endif
		if(master == NULL)
			return FALSE;
		return TRUE;
	}
	return FALSE;
}
#endif

void HTTP_Request::SetProgressInformation(ProgressState progress_level,
											 unsigned long progress_info1,
											 const void *progress_info2)
{
	switch(progress_level)
	{
	case REQUEST_CONNECTION_ESTABLISHED_TIMOUT:
	{
		if (progress_info1 < m_tcp_connection_established_timout)
		{
			m_tcp_connection_established_timout -= progress_info1;
		}
		else
			m_tcp_connection_established_timout = 0;
		break;
	}
	case REQUEST_QUEUED:
	case WAITING_FOR_CONNECTION:
	case START_CONNECT:
	case START_REQUEST:
	case WAITING_FOR_COOKIES:
	case WAITING_FOR_COOKIES_DNS:
	case UPLOADING_PROGRESS:
	case LOAD_PROGRESS:
		if(info.proxy_request)
		{
			OpString temp2;
			temp2.AppendFormat(UNI_L("%s (%s)"), (request->origin_host != NULL ? request->origin_host->UniName() : UNI_L("")),
						(request->connect_host != NULL ? request->connect_host->UniName() : UNI_L("")));
			if (temp2.HasContent())
			{
				progress_info2 =  temp2.CStr();
			}
			ProtocolComm::SetProgressInformation(progress_level,progress_info1, progress_info2);
		}
		else
			ProtocolComm::SetProgressInformation(progress_level,progress_info1, progress_info2);
		break;
	case HEADER_LOADED :
		if(method== HTTP_METHOD_OPTIONS)
		{
			mh->PostMessage(MSG_COMM_LOAD_OPTIONS_FINISHED, Id(), 0);
			//break;
		}
	default:
		ProtocolComm::SetProgressInformation(progress_level,progress_info1, progress_info2);
	}
}

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
void HTTP_Request::PauseLoading()
{
	if (http_conn)
		http_conn->PauseLoading();
	else
		master->RemoveRequest(this);
}

OP_STATUS HTTP_Request::ContinueLoading()
{
	if (http_conn)
		return http_conn->ContinueLoading();
	else if (!GetSink() && !info.loading_finished)
	{
		SetProgressInformation(RESTART_LOADING, 0, NULL);
		if (master->AddRequest(this) == COMM_REQUEST_FAILED)
		{
			EndLoading();
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		}
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION
