/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/encodings/utility/charsetnames.h"

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/url2.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_rel.h"
#include "modules/url/url_man.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/tools/arrays.h"
#include "modules/url/tools/content_detector.h"
#include "modules/url/url_dns_man.h"
#include "modules/url/uamanager/ua.h"

#include "modules/util/cleanse.h"
#include "modules/util/opfile/opfile.h"

#ifdef DOM_GADGET_FILE_API_SUPPORT
# include "modules/url/loadhandler/url_lhsand.h"
#endif // DOM_GADGET_FILE_API_SUPPORT

#ifdef PI_INTERNET_CONNECTION
# include "modules/url/internet_connection_wrapper.h"
#endif

#ifdef PI_NETWORK_INTERFACE_MANAGER
#include "modules/url/internet_connection_status_checker.h"
#endif // PI_NETWORK_INTERFACE_MANAGER

#ifdef USE_SPDY
# include "modules/url/protocols/spdy/spdy_internal/spdy_settingsmanager.h"
#endif // USE_SPDY

UrlModule::UrlModule()
: g_urlManager(NULL),
	g_url_api_obj(NULL),
	g_comm_cleaner_obj(NULL),
	scomm_wait_elm_factory(0),
	comm_wait_elm_factory(0),
	g_EmptyURL_Rep(0),
	g_EmptyURL_RelRep(0),
	ds_buf(NULL),
	ds_lbuf(0),
    tempurl(NULL),
	tempurl_len(0),

	g_last_user(NULL),
	g_last_hide(URL::KNoStr),
	g_last_rel_rep(NULL),
	tempuniurl(NULL),
	g_uni_last_user(NULL),
	g_uni_last_hide(URL::KNoUniStr),
	g_uni_last_rel_rep(NULL),

	g_HTTP_TmpBuf(NULL),
	g_HTTP_TmpBufSize(0),
	g_auth_id_counter0(0),

	g_attachment_counter(0),

	g_CommUseLocalHost(NULL),
	g_CommUseLocalHostInitialized(FALSE),
	g_comm_count(0),
	scomm_id(0),
	g_comm_list_call_count(0),

	local_host_addr(NULL),
	g_http_Manager(NULL)
#ifndef NO_FTP_SUPPORT
	, g_ftp_Manager(NULL)
#endif
#if defined URL_USE_UNIQUE_ID
	, m_url_id_counter(0)
#endif
	, m_url_context_id_counter(0)
	, m_url_storage_id_counter(0)
#ifdef DOM_GADGET_FILE_API_SUPPORT
	, mountPointProtocolManager(NULL)
#endif // DOM_GADGET_FILE_API_SUPPORT
#ifdef PI_NETWORK_INTERFACE_MANAGER
	, m_network_connection_status_checker(NULL)
#endif // PI_NETWORK_INTERFACE_MANAGER
	, m_ua_manager(NULL)
#ifdef USE_SPDY
	, m_spdy_settings_manager(NULL)
#endif // USE_SPDY
{
	size_t i;
	for(i = 0; i< ARRAY_SIZE(m_disabled_url_charsetids); i++)
		m_disabled_url_charsetids[i] = 0;

#ifdef URL_NETWORK_DATA_COUNTERS
	m_network_counters.Reset();
#endif
}

void
UrlModule::InitL(const OperaInitInfo& info)
{
	CONST_ARRAY_INIT_L(URL_typenames);
	CONST_ARRAY_INIT_L(HTTP_Method_Strings);
	CONST_ARRAY_INIT_L(HTTP_Scheme_Strings);
	CONST_ARRAY_INIT_L(HTTP_Header_List);
	CONST_ARRAY_INIT_L(PatternData);
	CONST_ARRAY_INIT_L(Untrusted_headers_HTTP);
#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
	CONST_ARRAY_INIT_L(Dynattr_Blocked_HTTPHeader);
#endif
	CONST_ARRAY_INIT_L(HTTP_arbitrary_method_list);

#if !defined(NO_URL_OPERA) || defined(HAS_OPERABLANK)
	CONST_ARRAY_INIT_L(operaurl_keyword);
#endif
#ifndef NO_FTP_SUPPORT
	CONST_ARRAY_INIT_L(url_month_array);
#endif
	CONST_ARRAY_INIT_L(mime_types);

	m_ua_manager = OP_NEW_L(UAManager, ());
	m_ua_manager->ConstructL();

	g_EmptyURL_Rep = OP_NEW_L(URL_Rep, ());
	g_EmptyURL_RelRep = OP_NEW_L(URL_RelRep, ());

	g_url_dns_manager = UrlDNSManager::CreateL();

	g_url_api_obj = OP_NEW_L(URL_API, ());
	g_url_api_obj->ConstructL();

	scomm_wait_elm_factory = OP_NEW_L(OpObjectFactory<SCommWaitElm>, ());
	scomm_wait_elm_factory->ConstructL(100); //FIXME:OOM
		// This cache is created handle OOM while safely deleting SComm objects
		// FIXME: Tune cache size

	comm_wait_elm_factory = OP_NEW_L(OpObjectFactory<CommWaitElm>, ());
	comm_wait_elm_factory->ConstructL(100); //FIXME:OOM
		// This cache is created handle OOM while safely deleting Comm objects
		// FIXME: Tune cache size

#ifdef DISK_CACHE_SUPPORT
	// Starts the cleaning of unsused cachefiles
	g_main_message_handler->PostDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES, 0, 0, 1000); // every second
#endif

#ifdef DEBUG_LOAD_STATUS
	g_main_message_handler->PostDelayedMessage(MSG_LOAD_DEBUG_STATUS,0,0,10000);
#endif

#ifdef DOM_GADGET_FILE_API_SUPPORT
	mountPointProtocolManager = OP_NEW_L(MountPointProtocolManager, ());
#endif // DOM_GADGET_FILE_API_SUPPORT

#ifdef PI_NETWORK_INTERFACE_MANAGER
	LEAVE_IF_ERROR(InternetConnectionStatusChecker::Make(m_network_connection_status_checker));
#endif // PI_NETWORK_INTERFACE_MANAGER

	size_t i=0;
	OP_ASSERT(i< ARRAY_SIZE(m_disabled_url_charsetids));
	m_disabled_url_charsetids[i++] = g_charsetManager->GetCharsetIDL("utf-16");
	OP_ASSERT(i< ARRAY_SIZE(m_disabled_url_charsetids));
	m_disabled_url_charsetids[i++] = g_charsetManager->GetCharsetIDL("utf-8");
	OP_ASSERT(i< ARRAY_SIZE(m_disabled_url_charsetids));
	m_disabled_url_charsetids[i++] = g_charsetManager->GetCharsetIDL("utf-16be");
	OP_ASSERT(i< ARRAY_SIZE(m_disabled_url_charsetids));
	m_disabled_url_charsetids[i++] = g_charsetManager->GetCharsetIDL("utf-16le");

#ifdef PI_INTERNET_CONNECTION
	LEAVE_IF_ERROR(m_internet_connection.Init());
#endif

#ifdef _NATIVE_SSL_SUPPORT_
	g_ssl_module.InterModuleInitL(info);
#endif
#ifdef USE_SPDY
	m_spdy_settings_manager = OP_NEW_L(SpdySettingsManager, ());
#endif // USE_SPDY
}

void
UrlModule::Destroy()
{
#if defined(_SSL_USE_OPENSSL_) && !defined(_EXTERNAL_SSL_SUPPORT_) && !defined(_CERTICOM_SSL_SUPPORT_)
	// Some parts of libssl needs to be shut down before url internals
	// shut down, in particular before the server name database is
	// shut down.
	//
	// This is a temporary workaround pending some sort of redesign
	// that eliminates the problem properly.  It was implemented here
	// by and because of decisions made by the architecture group.
	g_opera->libssl_module.InterModuleShutdown();
#endif

	if(g_CommUseLocalHost)
	{
		g_CommUseLocalHost->Decrement_Reference();
		g_CommUseLocalHost = NULL;
	}

	if (g_url_dns_manager)
		g_url_dns_manager->PreDestructStep();

	if (g_url_api_obj)
	{
		g_url_api_obj->CleanUp();
		OP_DELETE(g_url_api_obj);

		g_url_api_obj = NULL;
	}

	OP_DELETE(g_EmptyURL_Rep);
	g_EmptyURL_Rep = NULL;
	OP_DELETE(g_EmptyURL_RelRep);
	g_EmptyURL_RelRep = NULL;

#ifndef YNP_DEBUG_NAME
	OP_DELETEA(tempurl);
	tempurl = NULL;
	tempurl_len = 0;
	tempunipath.Empty();
	OP_DELETEA(tempuniurl);
	tempuniurl  = NULL;
#endif

	if (g_ds_buf != NULL)
	{
		OPERA_cleanse_heap(g_ds_buf, g_ds_lbuf);
		OP_DELETEA(g_ds_buf);
		g_ds_buf = NULL;
		g_ds_lbuf = 0;
	}

	if(g_HTTP_TmpBuf)
	{
		OPERA_cleanse_heap(g_HTTP_TmpBuf, g_HTTP_TmpBufSize);
		OP_DELETEA(g_HTTP_TmpBuf);
		g_HTTP_TmpBuf = NULL;
		g_HTTP_TmpBufSize=0;
	}

	ConnectionWaitList.Clear();
	DeletedCommList.Clear();

	OP_DELETE(g_local_host_addr);

	OP_DELETE(comm_wait_elm_factory);
	comm_wait_elm_factory = NULL;
	OP_DELETE(scomm_wait_elm_factory);
	scomm_wait_elm_factory = NULL;

	m_opera_url_generators.RemoveAll(); //Unlinks, not owner, therefore no deletion
	OP_DELETE(g_url_dns_manager);

#ifdef PI_NETWORK_INTERFACE_MANAGER
	OP_DELETE(m_network_connection_status_checker);
	m_network_connection_status_checker = NULL;
#endif // PI_NETWORK_INTERFACE_MANAGER

#ifdef DOM_GADGET_FILE_API_SUPPORT
	OP_DELETE(mountPointProtocolManager);
	mountPointProtocolManager = NULL;
#endif // DOM_GADGET_FILE_API_SUPPORT

	//CONST_ARRAY_SHUTDOWN(Keyword_Index_List); // expected to be shut down by formats.
	// Add all array shutdowns below
	CONST_ARRAY_SHUTDOWN(HTTP_arbitrary_method_list);
	CONST_ARRAY_SHUTDOWN(HTTP_Method_Strings);
	CONST_ARRAY_SHUTDOWN(HTTP_Scheme_Strings);
	CONST_ARRAY_SHUTDOWN(HTTP_Header_List);
	CONST_ARRAY_SHUTDOWN(PatternData);
	CONST_ARRAY_SHUTDOWN(Untrusted_headers_HTTP);
#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
	CONST_ARRAY_SHUTDOWN(Dynattr_Blocked_HTTPHeader);
#endif

#if !defined(NO_URL_OPERA) || defined(HAS_OPERABLANK)
	CONST_ARRAY_SHUTDOWN(operaurl_keyword);
#endif
#ifndef NO_FTP_SUPPORT
	CONST_ARRAY_SHUTDOWN(url_month_array);
#endif
	CONST_ARRAY_SHUTDOWN(URL_typenames);

	CONST_ARRAY_SHUTDOWN(mime_types);

	OP_DELETE(m_ua_manager);
	m_ua_manager = NULL;

#ifdef USE_SPDY
	OP_DELETE(m_spdy_settings_manager);
	m_spdy_settings_manager = NULL;
#endif // USE_SPDY
}

BOOL UrlModule::GetEnabledURLNameCharset(unsigned short id)
{
	if(id == 0)
		return FALSE;

	size_t i;
	for(i = 0; i< ARRAY_SIZE(m_disabled_url_charsetids); i++)
		if(m_disabled_url_charsetids[i] == id)
			return FALSE;

	return TRUE;
}
