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

#include "modules/probetools/probepoints.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/util/timecache.h"
#include "modules/url/url2.h"
#include "modules/util/handy.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/libssl/ssl_api.h"
#include "modules/probetools/src/probehelper.h"

#ifdef URL_DELAY_SAVE_TO_CACHE
#include "modules/dochand/winman.h"
#endif // URL_DELAY_SAVE_TO_CACHE

#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_rel.h"
#include "modules/url/url_man.h"
#include "modules/cookies/url_cm.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/protocols/connman.h"
#include "modules/url/protocols/http1.h"
#ifndef NO_FTP_SUPPORT
#include "modules/url/protocols/ftp.h"
#endif // NO_FTP_SUPPORT
#ifdef WEBSOCKETS_SUPPORT
#include "modules/url/protocols/websocket_manager.h"
#endif // WEBSOCKETS_SUPPORT
#ifdef WEB_TURBO_MODE
#include "modules/obml_comm/obml_config.h"
#endif // WEB_TURBO_MODE

#include "modules/util/cleanse.h"

#include "modules/olddebug/timing.h"
#include "modules/olddebug/tstdump.h"
#include "modules/url/url_debugmem.h"
#include "modules/idna/idna.h"

#ifdef WEB_TURBO_MODE
#include "modules/windowcommander/OpWindowCommanderManager.h"
#endif // WEB_TURBO_MODE

#ifdef WEB_TURBO_MODE
#include "modules/content_filter/content_filter.h"
#endif //WEB_TURBO_MODE

#ifdef _DEBUG
//#define _DEBUG_CONN
#endif

#ifdef SCOPE_RESOURCE_MANAGER
#include "modules/scope/scope_network_listener.h"
#endif // SCOPE_RESOURCE_MANAGER

#ifdef APPLICATION_CACHE_SUPPORT
#include "modules/applicationcache/application_cache_manager.h"
#endif // APPLICATION_CACHE_SUPPORT

#include "modules/prefs/prefsmanager/collections/pc_doc.h"

#include "modules/database/opdatabasemanager.h" // because of DATABASE_MODULE_ADD_CONTEXT

#ifdef WEB_HANDLERS_SUPPORT
#include "modules/security_manager/include/security_manager.h"
#endif // WEB_HANDLERS_SUPPORT

// Url_man.cpp

// URL Management

URL_Manager::URL_Manager()
	: servername_store(SERVERNAME_STORE_SIZE)
{
	InternalInit();
}

void URL_Manager::InternalInit()
{
	last_freed_server_resources = 0;
#ifdef EXTERNAL_PROTOCOL_HANDLER_SUPPORT
	last_unknown_scheme = (URLType)(URL_NULL_TYPE + 1000);
#else // EXTERNAL_PROTOCOL_HANDLER_SUPPORT
	last_unknown_scheme = URL_NULL_TYPE;
#endif // EXTERNAL_PROTOCOL_HANDLER_SUPPORT
	temp_buffer1 = NULL;
	temp_buffer2 = NULL;
	//temp_buffer3 = NULL;
	uni_temp_buffer1 = NULL;
	uni_temp_buffer2 = NULL;
	uni_temp_buffer3 = NULL;
	temp_buffer_len = 0;
	posted_temp_buffer_shrink_msg = FALSE;

#ifdef _EMBEDDED_BYTEMOBILE
	ebo_port = 0;
	embedded_bm_optimized = FALSE;
	embedded_bm_disabled = FALSE;
	embedded_bm_compressed_server = FALSE;
#endif //_EMBEDDED_BYTEMOBILE
#if defined(_EMBEDDED_BYTEMOBILE) || defined(SCOPE_RESOURCE_MANAGER) || defined(WEB_TURBO_MODE)
	request_sequence_number = 0;
#endif  // _EMBEDDED_BYTEMOBILE || SCOPE_RESOURCE_MANAGER || WEB_TURBO_MODE
#ifdef OPERA_PERFORMANCE
	connection_sequence_number = 0;
#endif  // OPERA_PERFORMANCE
#ifdef WEB_TURBO_MODE
	m_web_turbo_available = TRUE;
	m_reenable_turbo_delay = FIRST_REENALBE_TURBO_DELAY;
	m_turbo_reenable_message_posted = FALSE;
#endif // WEB_TURBO_MODE

	opera_url_number = 0;
	startloading_paused = FALSE;

	// where are these two declared ?

	http_Manager = NULL;

#ifndef NO_FTP_SUPPORT
	ftp_Manager = NULL;
#endif // NO_FTP_SUPPORT

#ifdef WEBSOCKETS_SUPPORT
	g_webSocket_Manager = NULL;
#endif // WEBSOCKETS_SUPPORT
#ifdef _BYTEMOBILE
	byte_mobile_optimized = FALSE;
#endif
	tld_whitelist.SetUTF8FromUTF16(g_pcnet->GetStringPref(PrefsCollectionNetwork::IdnaWhiteList).CStr());
#ifdef URL_DELAY_SAVE_TO_CACHE
	time_t now = g_timecache->CurrentTime();
	for (int count=0;count<4;count++)
		message_last_handled[count] = now;
#endif //URL_DELAY_SAVE_TO_CACHE

#ifdef OPERA_PERFORMANCE
	started_profile = FALSE;
	user_initiated = FALSE;
	loading_started_called = FALSE;
	opera_performance_enabled = FALSE;
#endif // OPERA_PERFORMANCE
}

#ifdef WEB_TURBO_MODE
void URL_Manager::SetWebTurboAvailable(BOOL is_available, BOOL reenable_attempt)
{
	if( !is_available ) 
	{
		if( !m_turbo_reenable_message_posted 
#ifdef OBML_COMM_FORCE_CONFIG_FILE
			&& !g_obml_config->GetExpired() // Absence of correct and valid config file means no Turbo support
#endif // OBML_COMM_FORCE_CONFIG_FILE
			)
		{
			if( m_reenable_turbo_delay <= MAX_REENABLE_TURBO_DELAY &&
				g_main_message_handler->PostDelayedMessage(MSG_URL_RE_ENABLE_WEB_TURBO_MODE,0,0,m_reenable_turbo_delay) )
			{
				m_turbo_reenable_message_posted = TRUE;
			}
			m_reenable_turbo_delay = m_reenable_turbo_delay*2;
		}

		if( m_web_turbo_available )
		{
			OpWebTurboUsageListener::TurboUsage usage = m_turbo_reenable_message_posted ? OpWebTurboUsageListener::TEMPORARILY_INTERRUPTED : OpWebTurboUsageListener::INTERRUPTED;
			g_windowCommanderManager->GetWebTurboUsageListener()->OnUsageChanged(usage);
		}
	}
	else
	{
		if( !reenable_attempt )
			m_reenable_turbo_delay = FIRST_REENALBE_TURBO_DELAY;
		if( m_turbo_reenable_message_posted )
		{
			g_main_message_handler->RemoveDelayedMessage(MSG_URL_RE_ENABLE_WEB_TURBO_MODE,0,0);
			m_turbo_reenable_message_posted = FALSE;
		}

		if( !m_web_turbo_available )
		{
			g_windowCommanderManager->GetWebTurboUsageListener()->OnUsageChanged(OpWebTurboUsageListener::NORMAL);
		}
	}
	m_web_turbo_available = is_available;
}
#endif // WEB_TURBO_MODE

#ifdef _EMBEDDED_BYTEMOBILE

BOOL URL_Manager::GetEmbeddedBmOpt()
{
	return g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EmbeddedBytemobile) && embedded_bm_optimized && !GetEmbeddedBmDisabled();
}
	
void URL_Manager::SetEBOServer(const OpStringC8 &hostname, unsigned short port)
{
	ebo_port = port;
	ebo_server.Set(hostname);
}

void URL_Manager::SetEBOOptions(const OpStringC8 &option)
{
	ebo_options.Set(option);
	//Check if compression is supported.:
	int setting = op_atoi(option.CStr());
	SetEmbeddedBmCompressed((setting & 0x01) != 0);
}

void URL_Manager::SetEBOBCResp(const OpStringC8 &bc_req)
{
/*	SSL_Hash_Pointer hash("test");
	hash->InitHash();
	hash->CalculateHash(bc_req);
	hash->CalculateHash(":");*/
}

#endif //_EMBEDDED_BYTEMOBILE

const OpStringC8 &URL_Manager::GetTLDWhiteList()
{
	return tld_whitelist;
}

void URL_Manager::InitL()
{
	URL_DynamicAttributeManager::InitL();

#ifdef _DEBUG
	OP_ASSERT(((unsigned int) URL_NUMER_OF_CONTENT_TYPES) - ((unsigned int) URL_HTML_CONTENT)<= (1 << 6)); // Too many contenttypes, cannot start. The size of URL_Rep::rep_info_st::content_type must be adjusted, then update this test
	if(((unsigned int) URL_NUMER_OF_CONTENT_TYPES)  - ((unsigned int) URL_HTML_CONTENT) > (1 << 6))
	{
		OP_ASSERT(0); LEAVE(OpStatus::ERR_OUT_OF_RANGE); // Hey! I really meant it!! I can't start before that size is updated!!
	}
#endif

	OP_STATUS op_err = servername_store.Construct();
	LEAVE_IF_ERROR(op_err);

	LEAVE_IF_ERROR(CheckTempbuffers(URL_INITIAL_TEMPBUFFER_SIZE));

#ifdef NETWORK_STATISTICS_SUPPORT
	network_statistics_manager = OP_NEW_L(Network_Statistics_Manager, ());
#endif // NETWORK_STATISTICS_SUPPORT


	http_Manager = OP_NEW_L(HTTP_Manager, ());
#ifndef NO_FTP_SUPPORT
	ftp_Manager = OP_NEW_L(FTP_Manager, ());
#endif // NO_FTP_SUPPORT
#ifdef WEBSOCKETS_SUPPORT
	g_webSocket_Manager = OP_NEW_L(WebSocket_Manager, ());
#endif // WEBSOCKETS_SUPPORT


	Cookie_Manager::InitL(OPFILE_COOKIE_FOLDER);
	Cache_Manager::InitL();
	Authentication_Manager::InitL();


#ifdef WEB_TURBO_MODE
	g_opera->obml_comm_module.InitConfigL();
#endif // WEB_TURBO_MODE
	
	g_pcnet->RegisterListenerL(this);

	LEAVE_IF_ERROR(g_periodic_task_manager->RegisterTask(&checkTimeouts, 5000));
#ifndef RAMCACHE_ONLY
	LEAVE_IF_ERROR(g_periodic_task_manager->RegisterTask(&freeUnusedResources, 10000));
#endif
	LEAVE_IF_ERROR(g_periodic_task_manager->RegisterTask(&autoSaveCache, 5 * 60 * 1000));

	static const OpMessage url_man_messages[] =
    {
		MSG_FLUSH_CACHE,
		MSG_URLMAN_AUTOSAVE_CACHE,
		MSG_URL_PIPELINE_PROB_DO_RELOAD_INTERNAL,
		MSG_URL_SHRINK_TEMPORARY_BUFFERS,
		MSG_URLMAN_DELETE_SOMEFILES,
#ifdef _ENABLE_AUTHENTICATE
		MSG_URL_NEED_AUTHENTICATION,
#endif
#ifndef RAMCACHE_ONLY
		MSG_URL_FREEUNUSED,
#endif
#ifdef NEED_URL_CONNECTION_CLOSED_DETECTION
		MSG_URL_CLOSE_ALL_CONNECTIONS,
#endif
#ifdef _SSL_USE_SMARTCARD_
		MSG_SMARTCARD_CHECK,
#endif
#ifdef _SUPPORT_WINSOCK2
		MSG_RESOLVER_UPDATE,
#endif
#ifdef SUPPORT_RIM_MDS_CACHE_INFO
		MSG_CACHEMAN_REPORT_SOME_ITEMS,
#endif
#ifdef WEB_TURBO_MODE
		MSG_URL_RE_ENABLE_WEB_TURBO_MODE,
#endif
#ifdef DEBUG_LOAD_STATUS
		MSG_LOAD_DEBUG_STATUS,
#endif
#ifdef HTTP_BENCHMARK
		33334,
		33335,
#endif // HTTP_BENCHMARK
    };

	LEAVE_IF_ERROR(g_main_message_handler->SetCallBackList(this, 0, url_man_messages, ARRAY_SIZE(url_man_messages)));
#ifdef DEBUG_LOAD_STATUS
	g_main_message_handler->PostMessage(MSG_LOAD_DEBUG_STATUS,0,0,0);
#endif
}

#ifdef URL_DELAY_SAVE_TO_CACHE
BOOL URL_Manager::DelayCacheOperations(OpMessage msg)
{
	time_t now = g_timecache->CurrentTime();
	BOOL busy = false;
	time_t last_handled;

	switch (msg)
	{
	case MSG_FLUSH_CACHE:
		last_handled = message_last_handled[0];
		break;
	case MSG_URLMAN_DELETE_SOMEFILES:
		last_handled = message_last_handled[1];
		break;
	case MSG_URLMAN_AUTOSAVE_CACHE:
		last_handled = message_last_handled[2];
		break;
	case MSG_URL_FREEUNUSED:
		last_handled = message_last_handled[3];
		break;
	default:
		OP_ASSERT(!"Unhandled message type");
		last_handled = 0;
		break;
	}

	if (now - last_handled < 10*60)
	{
		Window* window = NULL;
		if (g_windowManager)
		{
			// Cache operations will be delayed for exactly ONE window, the one in front
			for (window = g_windowManager->FirstWindow(); window && !window->IsVisibleOnScreen(); window = window->Suc()) ;
			//OP_ASSERT(window && window->IsVisibleOnScreen());
			if (window) 
			{
				busy = window->IsLoading();
			}
			else
			{
				for (window = g_windowManager->FirstWindow(); window && !busy; window = window->Suc())
				{
					busy = window->IsLoading();
				}
			}
		}
	}

	if (!busy)
	{
		last_handled = g_timecache->CurrentTime();
		switch (msg)
		{
		case MSG_FLUSH_CACHE:
			message_last_handled[0] = last_handled;
			break;
		case MSG_URLMAN_DELETE_SOMEFILES:
			message_last_handled[1] = last_handled;
			break;
		case MSG_URLMAN_AUTOSAVE_CACHE:
			message_last_handled[2] = last_handled;
			break;
		case MSG_URL_FREEUNUSED:
			message_last_handled[3] = last_handled;
			break;
		}
	}

	return busy;
}
#endif // URL_DELAY_SAVE_TO_CACHE

void URL_Manager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
	case MSG_FLUSH_CACHE:
#ifdef URL_DELAY_SAVE_TO_CACHE
		if (DelayCacheOperations(msg))
			g_main_message_handler->PostDelayedMessage(MSG_FLUSH_CACHE,0,0,60*1000); // try again in a minute
		else
#endif // URL_DELAY_SAVE_TO_CACHE
			urlManager->DoCheckCache();
		break;

	case MSG_URL_PIPELINE_PROB_DO_RELOAD_INTERNAL:
		{
			URL_Rep *rep = LocateURL((URL_ID) par1);
			if(rep && rep->GetDataStorage()) // only bother if it is loaded.
			{
				OpStringC8 empty;

				// Disable last modified
				rep->SetAttribute(URL::KHTTP_EntityTag, empty);
				rep->SetAttribute(URL::KHTTP_LastModified, empty);
				rep->SetAttribute(URL::KCachePolicy_AlwaysVerify, TRUE); // Force a refresh using must-revalidate

				g_main_message_handler->PostMessage(MSG_URL_PIPELINE_PROB_DO_RELOAD, par1, par2);
			}
		}
		break;

#ifdef DISK_CACHE_SUPPORT
	case MSG_URLMAN_DELETE_SOMEFILES:
#ifdef URL_DELAY_SAVE_TO_CACHE
		if (DelayCacheOperations(msg))
			g_main_message_handler->PostDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES,0,0,1000); // try again in a sec
		else
#endif // URL_DELAY_SAVE_TO_CACHE
			urlManager->DeleteFilesInDeleteList();
		break;
#endif // DISK_CACHE_SUPPORT

	case MSG_URLMAN_AUTOSAVE_CACHE:
		{
#ifdef URL_DELAY_SAVE_TO_CACHE
			if (!DelayCacheOperations(msg))
#endif // URL_DELAY_SAVE_TO_CACHE
			{
				TRAPD(op_err, urlManager->AutoSaveFilesL());
				if(OpStatus::IsMemoryError(op_err))
					g_memory_manager->RaiseCondition(op_err);
			}
		}
		break;

	case MSG_URL_SHRINK_TEMPORARY_BUFFERS:
		posted_temp_buffer_shrink_msg = FALSE;
		ShrinkTempbuffers();
		URL_Name::ShrinkTempBuffers();
		break;

#ifdef _ENABLE_AUTHENTICATE
	case MSG_URL_NEED_AUTHENTICATION:	//	0x8002
		if(urlManager)
		{
			OP_STATUS s;
			if (OpStatus::IsMemoryError(s = urlManager->StartAuthentication()))
				g_memory_manager->RaiseCondition(s);
		}
		break;
#endif
	case MSG_URL_FREEUNUSED:
		{
#ifdef URL_DELAY_SAVE_TO_CACHE
			if (!DelayCacheOperations(msg))
#endif // URL_DELAY_SAVE_TO_CACHE
			{
				if (urlManager->FreeUnusedResources(FALSE))
				{
					// disable the periodic check until the free
					// unused operation completes to avoid multiple
					// posts
					freeUnusedResources.SetDisabled();
					g_main_message_handler->PostDelayedMessage(MSG_URL_FREEUNUSED,0,0,500);
				}
				else
					// re-enable the periodic check now that the
					// operation is completed
					freeUnusedResources.SetEnabled();
			}
		}
		break;

#ifdef NEED_URL_CONNECTION_CLOSED_DETECTION
	case MSG_URL_CLOSE_ALL_CONNECTIONS:
		{
			StopAllLoadingURLs();
			CloseAllConnections();
		}
#endif
#ifdef _SSL_USE_SMARTCARD_
	case MSG_SMARTCARD_CHECK:
		{
			extern void CheckSmartCardsPresentInReaders();

			CheckSmartCardsPresentInReaders();
		}
		break;
#endif // _SSL_USE_SMARTCARD_
#ifdef SUPPORT_RIM_MDS_CACHE_INFO
	case MSG_CACHEMAN_REPORT_SOME_ITEMS:
		{
			ReportSomeCacheItems(par1);
		}
		break;
#endif // SUPPORT_RIM_MDS_CACHE_INFO
#ifdef HTTP_BENCHMARK
	case 33334:
		extern void PrepareNextBenchMark_Step1(unsigned long);
		PrepareNextBenchMark_Step1(lParam);
		break;

	case 33335:
		extern void StartBenchmark();
		StartBenchmark();
		break;
#endif // HTTP_BENCHMARK

#ifdef DEBUG_LOAD_STATUS
	case MSG_LOAD_DEBUG_STATUS:
		//PrintfTofile("loading.txt", "Load status as of tick %lu\n", (unsigned long) g_op_time_info->GetWallClockMS());
		g_main_message_handler->PostDelayedMessage(MSG_LOAD_DEBUG_STATUS,0,0,10000);
		break;
#endif
#ifdef WEB_TURBO_MODE
	case MSG_URL_RE_ENABLE_WEB_TURBO_MODE:
		m_turbo_reenable_message_posted = FALSE;
		SetWebTurboAvailable(TRUE, TRUE);
		break;
#endif // WEB_TURBO_MODE
	}
}

void URL_Manager::PeriodicCheckTimeouts::Run()
{
	g_url_api->CheckTimeOuts();
}

void URL_Manager::PeriodicFreeUnusedResources::Run()
{
#ifdef URL_DELAY_SAVE_TO_CACHE
	if (!urlManager->DelayCacheOperations(MSG_URL_FREEUNUSED))
#endif // URL_DELAY_SAVE_TO_CACHE
	{
		if (urlManager->FreeUnusedResources(FALSE))
		{
			// disable this periodic check until the free unused
			// operation completes
			SetDisabled();
			g_main_message_handler->PostDelayedMessage(MSG_URL_FREEUNUSED,0,0,500);
		}
	}
}

void URL_Manager::PeriodicAutoSaveCache::Run()
{
#ifdef URL_DELAY_SAVE_TO_CACHE
	if (!urlManager->DelayCacheOperations(MSG_URLMAN_AUTOSAVE_CACHE))
#endif // URL_DELAY_SAVE_TO_CACHE
	{
		TRAPD(op_err, urlManager->AutoSaveFilesL());
		if(OpStatus::IsMemoryError(op_err))
			g_memory_manager->RaiseCondition(op_err);
	}
}

void URL_Manager::CleanUp(BOOL ignore_downloads)
{
	CacheCleanUp(ignore_downloads);
	CloseAllConnections();
	Comm::CommRemoveDeletedComm(); // Remove any Finished, but not deleted Communication objects
	Comm::CleanWaitingComm_List();
#ifdef OPERA_PERFORMANCE
	connection_sequence_number = 0;
#endif  // OPERA_PERFORMANCE
}

void URL_Manager::CloseAllConnections()
{
	if (http_Manager)
		http_Manager->CloseAllConnections(); // ABTODO re-implement this if necessary
#ifndef NO_FTP_SUPPORT
	if (ftp_Manager)
		ftp_Manager->CloseAllConnections();
#endif // NO_FTP_SUPPORT
#ifdef WEBSOCKETS_SUPPORT
	if (g_webSocket_Manager)
		g_webSocket_Manager->CloseAllConnections();
#endif // WEBSOCKETS_SUPPORT
#ifdef HTTP_BENCHMARK
	Comm::CommRemoveDeletedComm(); // Remove any Finished, but not deleted Communication objects
	Comm::CleanWaitingComm_List();
	OP_DELETE(http_Manager);
	http_Manager = OP_NEW(HTTP_Manager, ());
#endif
}

BOOL URL_Manager::FreeUnusedResources(BOOL all)
{
	if(all)
	{
		http_Manager->ClearIdleConnections();
#ifndef NO_FTP_SUPPORT
		ftp_Manager->ClearIdleConnections();
#endif // NO_FTP_SUPPORT
#ifdef WEBSOCKETS_SUPPORT
		g_webSocket_Manager->ClearIdleConnections();
#endif // WEBSOCKETS_SUPPORT
	}

	Cookie_Manager::FreeUnusedResources();
	BOOL ret = Cache_Manager::FreeUnusedResources(all);

	BOOL check_servers = FALSE;
	if(all || last_freed_server_resources < g_timecache->CurrentTime()-5*60) // every 5 minutes, max;
	{
		check_servers = TRUE;
		last_freed_server_resources = g_timecache->CurrentTime();
	}

	ServerName *server = GetFirstServerName();
	while(server)
	{
		if(check_servers)
			server->FreeUnusedResources(all);

		if(server->SafeToDelete())
		{
			servername_store.RemoveServerName(server);
			OP_DELETE(server);
		}

		server = GetNextServerName();
	}

	return ret;
}

OP_STATUS URL_Manager::WriteFiles()
{
	OP_PROBE7(OP_PROBE_URL_MANAGER_WRITEFILES);

#ifdef DISK_COOKIES_SUPPORT
	TRAPD(op_err1, WriteCookiesL());
	RETURN_IF_MEMORY_ERROR(op_err1);
#endif
#ifndef RAMCACHE_ONLY
	TRAP_AND_RETURN(op_err2, WriteCacheIndexesL(TRUE, TRUE));
#endif
	return OpStatus::OK;
}

void URL_Manager::AutoSaveFilesL()
{
	OP_PROBE7_L(OP_PROBE_URL_MANAGER_AUTOSAVECACHE_L);

#ifdef DISK_COOKIES_SUPPORT
	AutoSaveCookies(); // Only leaves in case of OOM
#endif // DISK_COOKIES_SUPPORT
	AutoSaveCacheL();

	ServerName *server = servername_store.GetFirstServerName();
	while(server)
	{
		if(server->SafeToDelete())
		{
			servername_store.RemoveServerName(server);
			OP_DELETE(server);
		}

		server = servername_store.GetNextServerName();
	}
}

URL_Manager::~URL_Manager()
{
	InternalDestruct();
}

void URL_Manager::InternalDestruct()
{
#ifdef _NATIVE_SSL_SUPPORT_
	// Remove all url references from libssl objects.
	// This is to avoid crashes due to dangling pointers after Cache_Manager::PreDestructStep deletes all URL_Reps
	ClearSSLSessions();
	if (http_Manager)
		http_Manager->RemoveURLReferences();
#endif

	Cookie_Manager::PreDestructStep();
	Cache_Manager::PreDestructStep();

	g_main_message_handler->RemoveDelayedMessage(MSG_FLUSH_CACHE, 0, 0);
	g_main_message_handler->RemoveDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES, 0, 0);
	g_main_message_handler->RemoveDelayedMessage(MSG_URL_FREEUNUSED, 0, 0);
#ifdef WEB_TURBO_MODE
	g_main_message_handler->RemoveDelayedMessage(MSG_URL_RE_ENABLE_WEB_TURBO_MODE, 0, 0);
#endif // WEB_TURBO_MODE
	g_main_message_handler->UnsetCallBacks(this);

	g_pcnet->UnregisterListener(this);
	g_periodic_task_manager->UnregisterTask(&checkTimeouts);
#ifndef RAMCACHE_ONLY
	g_periodic_task_manager->UnregisterTask(&freeUnusedResources);
#endif
	g_periodic_task_manager->UnregisterTask(&autoSaveCache);

	ServerName *servername = GetFirstServerName();
	while(servername)
	{
		servername->UnsetParentDomain();
		servername = GetNextServerName();
	}


	OP_DELETE(http_Manager);
	http_Manager = NULL;
	OP_DELETEA(HTTP_TmpBuf);
	HTTP_TmpBuf = NULL;
	HTTP_TmpBufSize = 0;
#ifndef NO_FTP_SUPPORT
	OP_DELETE(ftp_Manager);
	ftp_Manager = NULL;
#endif // NO_FTP_SUPPORT
#ifdef WEBSOCKETS_SUPPORT
	OP_DELETE(g_webSocket_Manager);
	g_webSocket_Manager = NULL;
#endif // WEBSOCKETS_SUPPORT

	Comm::CleanWaitingComm_List();
	//OP_ASSERT(Comm::ConnectionWaitList.Empty());
	//OP_ASSERT(SComm::DeletedCommList.Empty());

#ifdef URL_NETWORK_LISTENER_SUPPORT
	listeners.RemoveAll();
#endif

	//OP_ASSERT(HTTP_1_1::HTTP_count == 0);

	//delete dcache_store;
	//delete [] temp_buffer1;
	//delete [] temp_buffer2;
	//delete [] temp_buffer3;
	OP_DELETEA(uni_temp_buffer1);
	OP_DELETEA(uni_temp_buffer2);
	OP_DELETEA(uni_temp_buffer3);

#ifdef NETWORK_STATISTICS_SUPPORT
	OP_DELETE(network_statistics_manager);
#endif // NETWORK_STATISTICS_SUPPORT

#ifdef OPERA_PERFORMANCE
	ClearLogs();
#endif // OPERA_PERFORMANCE
}

#ifdef NEED_URL_ACTIVE_CONNECTIONS
BOOL URL_Manager::ActiveConnections()
{
	return (g_url_comm_count != 0);
}
#endif

BOOL URL_Manager::TooManyOpenConnections(ServerName *server)
{
#ifdef _DEBUG_CONN
		PrintfTofile("http1.txt","URLman count %s : server %d total %d Waiting %d\n",
			(server ? server->Name() : "---"),
			(server ? server->GetConnectCount() : 0),
			comm_count,
			Comm::CountWaitingComm(COMM_WAIT_STATUS_LOAD));
		PrintfTofile("http.txt","URLman count %s : server %d total %d Waiting %d\n",
			(server ? server->Name() : "---"),
			(server ? server->GetConnectCount() : 0),
			comm_count,
			Comm::CountWaitingComm(COMM_WAIT_STATUS_LOAD));
		PrintfTofile("httpreq.txt","URLman count %s : server %d total %d Waiting %d\n",
			(server ? server->Name() : "---"),
			(server ? server->GetConnectCount() : 0),
			comm_count,
			Comm::CountWaitingComm(COMM_WAIT_STATUS_LOAD));
		PrintfTofile("winsck.txt","URLman count %s : server %d total %d Waiting %d\n",
			(server ? server->Name() : "---"),
			(server ? server->GetConnectCount() : 0),
			comm_count,
			Comm::CountWaitingComm(COMM_WAIT_STATUS_LOAD));
#endif
    if ((!server || server->GetConnectCount() <=
		(unsigned int) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsServer)) &&
		g_url_comm_count <=	g_pcnet->GetIntegerPref(PrefsCollectionNetwork::MaxConnectionsTotal)
		&& Comm::CountWaitingComm(COMM_WAIT_STATUS_LOAD)==0)
			return FALSE;
	return TRUE;
}

const uni_char *URL_Manager::GetProxy(ServerName *servername, URLType type) const
{
	const uni_char *host = (servername ? servername->UniName() : NULL);

	switch (type)
	{
	case URL_HTTP:
		return g_pcnet->GetHTTPProxyServer(host);

	case URL_HTTPS:
		return g_pcnet->GetHTTPSProxyServer(host);

#ifndef NO_FTP_SUPPORT
	case URL_FTP:
		return g_pcnet->GetFTPProxyServer(host);
#endif // NO_FTP_SUPPORT

#ifdef GOPHER_SUPPORT
	case URL_Gopher:
		return g_pcnet->GetGopherProxyServer(host);
#endif // GOPHER_SUPPORT

#ifdef WAIS_SUPPORT
	case URL_WAIS:
		return g_pcnet->GetWAISProxyServer(host);
#endif // WAIS_SUPPORT

#ifdef WEBSOCKETS_SUPPORT
	case URL_WEBSOCKET_SECURE:
	case URL_WEBSOCKET:
		{
			const uni_char *proxy = g_pcnet->GetHTTPSProxyServer(host);
			if (!proxy)
				proxy = g_pcnet->GetHTTPProxyServer(host);
			return proxy;
		}
		break;
#endif //WEBSOCKETS_SUPPORT

#ifdef SOCKS_SUPPORT
	case URL_SOCKS:
		return g_pcnet->GetSOCKSProxyServer(host);
#endif // SOCKS_SUPPORT

	}

	return NULL;
}

BOOL URL_Manager::UseProxy(URLType type) const
{
	return GetProxy(NULL /* FIXME later */, type) != NULL;
}

int URL_Manager::ConvertHTTPResponseToErrorCode(int response)
{
	int err = 0;

	switch (response)
	{
	case HTTP_PAYMENT_REQUIRED:
		err = URL_ERRSTR(SI, ERR_HTTP_PAYMENT_REQUIRED);
		break;
	case HTTP_FORBIDDEN:
		err = URL_ERRSTR(SI, ERR_HTTP_FORBIDDEN);
		break;

	case HTTP_NOT_FOUND:
		err = URL_ERRSTR(SI, ERR_HTTP_NOT_FOUND);
		break;

	case HTTP_METHOD_NOT_ALLOWED:
		err = URL_ERRSTR(SI, ERR_HTTP_METHOD_NOT_ALLOWED);
		break;
	case HTTP_NOT_ACCEPTABLE:
		err = URL_ERRSTR(SI, ERR_HTTP_NOT_ACCEPTABLE);
		break;
	case HTTP_TIMEOUT:
		err = URL_ERRSTR(SI, ERR_HTTP_TIMEOUT);
		break;
	case HTTP_CONFLICT:
		err = URL_ERRSTR(SI, ERR_HTTP_CONFLICT);
		break;
	case HTTP_GONE:
		err = URL_ERRSTR(SI, ERR_HTTP_GONE);
		break;
	case HTTP_LENGTH_REQUIRED:
		err = URL_ERRSTR(SI, ERR_HTTP_LENGTH_REQUIRED);
		break;
	case HTTP_PRECOND_FAILED:
		err = URL_ERRSTR(SI, ERR_HTTP_PRECOND_FAILED);
		break;
	case HTTP_REQUEST_ENT_TOO_LARGE:
		err = URL_ERRSTR(SI, ERR_HTTP_REQUEST_ENT_TOO_LARGE);
		break;
	case HTTP_REQUEST_URI_TOO_LARGE:
		err = URL_ERRSTR(SI, ERR_HTTP_REQUEST_URI_TOO_LARGE);
		break;
	case HTTP_UNSUPPORTED_MEDIA_TYPE:
		err = URL_ERRSTR(SI, ERR_HTTP_UNSUPPORTED_MEDIA_TYPE);
		break;
	case HTTP_RANGE_NOT_SATISFIABLE:
		err = URL_ERRSTR(SI, ERR_HTTP_RANGE_NOT_SATISFIABLE);
		break;
	case HTTP_EXPECTATION_FAILED:
		err = URL_ERRSTR(SI, ERR_HTTP_EXPECTATION_FAILED);
		break;

	case HTTP_NO_CONTENT:
		err = URL_ERRSTR(SI, ERR_HTTP_NO_CONTENT);
		break;

	case HTTP_RESET_CONTENT:
		err = URL_ERRSTR(SI, ERR_HTTP_RESET_CONTENT);
		break;

	case HTTP_INTERNAL_ERROR:
		err = URL_ERRSTR(SI, ERR_HTTP_SERVER_ERROR);
		break;

	case HTTP_NOT_IMPLEMENTED:
		err = URL_ERRSTR(SI, ERR_HTTP_NOT_IMPLEMENTED);
		break;
	case HTTP_BAD_GATEWAY:
		err = URL_ERRSTR(SI, ERR_HTTP_BAD_GATEWAY);
		break;
	case HTTP_SERVICE_UNAVAILABLE :
		err = URL_ERRSTR(SI, ERR_HTTP_SERVICE_UNAVAILABLE);
		break;
	case HTTP_GATEWAY_TIMEOUT:
		err = URL_ERRSTR(SI, ERR_HTTP_GATEWAY_TIMEOUT);
		break;
	case HTTP_VERSION_NOT_SUPPORTED:
		err = URL_ERRSTR(SI, ERR_HTTP_VERSION_NOT_SUPPORTED);
		break;
	case HTTP_BAD_REQUEST:
		err = URL_ERRSTR(SI, ERR_ILLEGAL_URL);
		break;

	case HTTP_UNAUTHORIZED:
		err = URL_ERRSTR(SI, ERR_UNSUPPORTED_SERVER_AUTH);
		break;

	case HTTP_PROXY_UNAUTHORIZED:
		err = URL_ERRSTR(SI, ERR_UNSUPPORTED_PROXY_AUTH);
		break;

	default:
		if (response >= 400 && response < 500)
			err = URL_ERRSTR(SI, ERR_ILLEGAL_URL);
		else if (response >= 500 && response < 600)
			err = URL_ERRSTR(SI, ERR_HTTP_SERVER_ERROR);
		else if (response >= 600)
			err = URL_ERRSTR(SI, ERR_HTTP_SERVER_ERROR);
	}
	return err;
}


void URL_Manager::UpdateUAOverrides()
{
	ServerName* serv_name = servername_store.GetFirstServerName();
	while(serv_name)
	{
		serv_name->UpdateUAOverrides();
		serv_name = servername_store.GetNextServerName();
	}
}

void URL_Manager::CheckTimeOuts()
{
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_) && !defined(_CERTICOM_SSL_SUPPORT_)
	time_t cur_time = g_timecache->CurrentTime();

	ServerName* serv_name = servername_store.GetFirstServerName();
	while(serv_name)
	{
		serv_name->CheckTimeOuts(cur_time);
		serv_name = servername_store.GetNextServerName();
	}

	g_ssl_api->CheckSecurityTimeouts();
#endif
}

#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_) && !defined(_CERTICOM_SSL_SUPPORT_)
void URL_Manager::ForgetCertificates()
{
	ServerName* serv_name = servername_store.GetFirstServerName();
	while(serv_name)
	{
		serv_name->ForgetCertificates();
		serv_name = servername_store.GetNextServerName();
	}
}
#endif

#ifdef _NATIVE_SSL_SUPPORT_
void URL_Manager::ClearSSLSessions()
{
	// Close all idle connections
	CloseAllConnections();

	ServerName* serv_name = servername_store.GetFirstServerName();
	while(serv_name)
	{
		serv_name->InvalidateSessionCache(TRUE);
		serv_name = servername_store.GetNextServerName();
	}
}
#endif // _NATIVE_SSL_SUPPORT_

void URL_Manager::RemoveSensitiveData()
{
	CloseAllConnections();

	ServerName* serv_name = servername_store.GetFirstServerName();
	while(serv_name)
	{
		serv_name->RemoveSensitiveData();
		serv_name = servername_store.GetNextServerName();
	}

	RemoveSensitiveCacheData();
	Authentication_Manager::Clear_Authentication_List();
}

extern void LogBOOL(BOOL);

void URL_Manager::SetPauseStartLoading(BOOL val)
{
	if(startloading_paused && val == FALSE)
	{
		startloading_paused = FALSE;
		Comm::CommRemoveDeletedComm(); // Remove any Finished, but not deleted Communication objects
		Comm::TryLoadBlkWaitingComm(); // Remove any Finished, but not deleted Communication objects
		Comm::TryLoadWaitingComm(NULL);
        //Log("Yngve : UnPausing2"); LogBOOL(startloading_paused); LogBOOL(val);LogNl();
	}
    else
    {
		startloading_paused = val;
        //Log("Yngve : Pausing 2"); LogBOOL(startloading_paused); LogBOOL(val);LogNl();
    }
}

void URL_Manager::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	OP_ASSERT(OpPrefsCollection::Network == id);

	switch ((PrefsCollectionNetwork::integerpref) pref)
	{
#ifdef WEB_TURBO_MODE
	case PrefsCollectionNetwork::UseWebTurbo:
		SetWebTurboAvailable(TRUE);
		// Fall through to generic proxy pref handling
#endif
	case PrefsCollectionNetwork::UseHTTPProxy:
	case PrefsCollectionNetwork::UseHTTPSProxy:
#ifndef NO_FTP_SUPPORT
	case PrefsCollectionNetwork::UseFTPProxy:
#endif
#ifdef GOPHER_SUPPORT
	case PrefsCollectionNetwork::UseGopherProxy:
#endif
#ifdef WAIS_SUPPORT
	case PrefsCollectionNetwork::UseWAISProxy:
#endif
#ifdef SOCKS_SUPPORT
	case PrefsCollectionNetwork::UseSOCKSProxy:
#endif

#ifdef NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS
		//RestartAllConnections();
		http_Manager->RestartAllConnections();
#else
		CloseAllConnections();
#endif
		break;

	default:
		break;
	}

	Cache_Manager::PrefChanged(pref, newvalue);
}

void URL_Manager::PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC &newStr)
{
	OP_ASSERT(OpPrefsCollection::Network == id);

	switch ((PrefsCollectionNetwork::stringpref) pref)
	{
	case PrefsCollectionNetwork::HTTPProxy:
	case PrefsCollectionNetwork::HTTPSProxy:
#ifndef NO_FTP_SUPPORT
	case PrefsCollectionNetwork::FTPProxy:
#endif
#ifdef GOPHER_SUPPORT
	case PrefsCollectionNetwork::GopherProxy:
#endif
#ifdef WAIS_SUPPORT
	case PrefsCollectionNetwork::WAISProxy:
#endif
#ifdef SOCKS_SUPPORT
	case PrefsCollectionNetwork::SOCKSProxy:
#endif
		CloseAllConnections();
		break;
#if defined(WEB_TURBO_MODE) && defined(URL_FILTER)
	case PrefsCollectionNetwork::WebTurboBypassURLs:
		{
			// Since there's no opstatus to return here, just trap and ignore.
			OP_STATUS ret = OpStatus::OK;
			TRAP(ret, g_urlfilter->SetWebTurboBypassURLsL(newStr));
			OpStatus::Ignore(ret);
		}
		break;
#endif //defined(WEB_TURBO_MODE) && defined(URL_FILTER)
	}
}

BOOL URL_Manager::UseProxyOnServer(ServerName *hostname, unsigned short port)
{
	if (!hostname || hostname->GetName().IsEmpty())
        return TRUE;

	OpStringC8 temp_host = hostname->GetName();

	// Check for local name
	if (!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::HasUseProxyLocalNames, hostname) &&
		hostname->GetNameComponentCount() == 1)
		return FALSE;

	if (temp_host.Compare("127.0.0.1")==0 || temp_host.Compare("localhost") ==0)
		return FALSE;

	OpStringC no_proxy_servers_tmp = g_pcnet->GetStringPref(PrefsCollectionNetwork::NoProxyServers, hostname);
    if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::HasNoProxyServers, hostname) && no_proxy_servers_tmp.HasContent())
    {
	    OpString no_proxy_servers; ANCHOR(OpString, no_proxy_servers);
	    if (OpStatus::IsError(no_proxy_servers.Set(no_proxy_servers_tmp)))
		    return TRUE;
	    uni_strlwr(no_proxy_servers.CStr());

	    uni_char* no_proxy;
        const uni_char * const separator = UNI_L(" ;,\r\n\t");
        for (no_proxy = uni_strtok(no_proxy_servers.CStr(), separator); no_proxy;
	         no_proxy = uni_strtok(NULL, separator))
	    {
		    uni_char *port_str = uni_strchr(no_proxy, ':');
		    if (port_str)
			    *(port_str++) = '\0';

		    if (MatchExpr(hostname->UniName(), no_proxy,TRUE))
		    {
			    // *** if no portexpression do not use proxy ***
			    if (!port_str || !*port_str)
				    return FALSE;
			    while(port_str && *port_str)
			    {
				    uni_char *next_expr = uni_strchr(port_str,'|');
				    if (next_expr)
					    *(next_expr++) = '\0';
				    if (*port_str)
				    {
					    uni_char *range = uni_strchr(port_str,'-');
					    if (range)
					    {
						    *(range++) = '\0';
						    while(*range && *range=='-')
							    range++;

						    if (*port_str || *range)
						    {
							    if ((!*port_str || uni_strtol(port_str, NULL, 10) <= port) && (!*range ||  port <= uni_strtol(range, NULL, 10)))
								    return FALSE;
						    }
					    }
					    else
						    if (uni_strtol(port_str, NULL, 10) == port)
							    return FALSE;
						    //if (MatchExpr(port_number, port_str,TRUE))
						    //	return FALSE;
				    }
				    port_str = next_expr;
			    }
		    }
	    }
    }

#ifdef URL_PER_SITE_PROXY_POLICY
    return g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableProxy, hostname);
#else
    return TRUE;
#endif
}


#ifdef HISTORY_SUPPORT
BOOL URL_Manager::GlobalHistoryPermitted(URL &url)
{
	if(url.GetAttribute(URL::KUnique)
#ifdef _SSL_USE_SMARTCARD_
				|| url.GetAttribute(URL::KHaveSmartCardAuthentication)
#endif
		)
		return FALSE;

	switch((URLType) url.GetAttribute(URL::KType))
	{
		// These URL types can be stored in the global history
	case URL_HTTP:
	case URL_HTTPS:
	case URL_FTP:
	case URL_Gopher:
	case URL_WAIS:
	case URL_FILE:
	case URL_NEWS:
	case URL_SNEWS:
	case URL_NNTP:
	case URL_NNTPS:
	case URL_TELNET:
	case URL_TN3270:
		return TRUE;
	}

	// no other url types can be stored in the global history
	return FALSE;
}
#endif

BOOL URL_Manager::LoadAndDisplayPermitted(URL &url)
{
	URLType url_type = (URLType) url.GetAttribute(URL::KType);

	switch(url_type)
	{
		// These URL types can be loaded and viewed
	case URL_HTTP:
	case URL_HTTPS:
#ifdef FTP_SUPPORT
	case URL_FTP:
#endif
#ifdef _LOCALHOST_SUPPORT_
	case URL_FILE:
#endif
	case URL_MOUNTPOINT:
	case URL_CONTENT_ID:
	case URL_ATTACHMENT:
	case URL_EMAIL:
#if defined(GADGET_SUPPORT)
	case URL_WIDGET:
#endif /* GADGET_SUPPORT*/
#ifdef SUPPORT_DATA_URL
	case URL_DATA:
#endif
#ifdef HAS_ATVEF_SUPPORT
	case URL_TV:
#endif
#if defined(WEBSERVER_SUPPORT)
	case URL_UNITE:
#endif
#if !defined(NO_URL_OPERA) || defined(HAS_OPERABLANK)
	case URL_OPERA:
#endif // !NO_URL_OPERA
		return TRUE;
	case URL_JAVASCRIPT:
		return FALSE;
	}

	// Proxyloaded URLs can also be loaded
	if(urlManager->UseProxy(url_type))
		return TRUE;

#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
	// and protocols with an external handler
	if(IsAnExternalProtocolHandler(url_type))
		return TRUE;
#endif // EXTERNAL_PROTOCOL_SCHEME_SUPPORT

	OpString protocol;
	RETURN_VALUE_IF_ERROR(protocol.SetFromUTF8(url.GetAttribute(URL::KProtocolName).CStr()), FALSE);

	TrustedProtocolData data;
	int index = g_pcdoc->GetTrustedProtocolInfo(protocol, data);
	if (index >= 0)
	{
		if (data.viewer_mode == UseWebService)
		{
#ifdef WEB_HANDLERS_SUPPORT
			OpSecurityContext source(url);
			OpSecurityContext target(url);
			target.AddText(protocol.CStr());
			BOOL allowed;
			OP_STATUS status = g_secman_instance->CheckSecurity(OpSecurityManager::WEB_HANDLER_REGISTRATION, source, target, allowed);
			return OpStatus::IsSuccess(status) && allowed;
#else // WEB_HANDLERS_SUPPORT
			return TRUE;
#endif // WEB_HANDLERS_SUPPORT
		}
		else if (data.viewer_mode == UseInternalApplication)
			return protocol.CompareI("mailto") != 0; // Mailto opened in opera is not handled by core
	}

	// no other url types can be loaded and displayed
	return FALSE;
}

#ifdef _MIME_SUPPORT_
BOOL URL_Manager::EmailUrlSuppressed(URLType url_type)
{
	switch(url_type)
	{
		// Loading of these URL types are NOT suppressed when they are loaded from email in the M2 windows
	case URL_NEWSATTACHMENT:
	case URL_SNEWSATTACHMENT:
	case URL_ATTACHMENT:
	case URL_CONTENT_ID:
	case URL_NULL_TYPE:
#ifdef NEED_URL_FILE_NOT_EMAIL_SUPPRESSED
	case URL_FILE: // EPOC uses file URLs for mail attachments
#endif
		return FALSE;
	}

	// no other url types can be loaded and displayed from M2, unless external embeds are enabled (checked elsewhere)
	return TRUE;
}
#endif

#ifdef URL_SESSION_FILE_PERMIT
BOOL URL_Manager::SessionFilePermitted(URL &url)
{
	// Empty URLs and alias URLs are not written to the session files
	// (Alias URLs are created by content-loaction heades in email-attachments and reference a given content-ID in that email)
	if(url.IsEmpty()  /* || !url.UniName(PASSWORD_SHOW) // non-Empty URLs do not return NULL pointers */
#ifdef _MIME_SUPPORT_
		|| url.GetAttribute(URL::KIsAlias)
#endif
		)
		return FALSE;

	URLType url_type = (URLType) url.GetAttribute(URL::KType);

	switch(url_type)
	{
		// Nor are mail and news related URLs
	case URL_NEWSATTACHMENT:
	case URL_SNEWSATTACHMENT:
	case URL_ATTACHMENT:
	case URL_CONTENT_ID:
	case URL_EMAIL:
		return FALSE;
	}

	// all other URLs are stored in the session file
	return TRUE;
}
#endif

BOOL URL_Manager::OfflineLoadable(URLType url_type)
{

	// Only the following URL types are stored, or generated locally, and can therefore be loaded in an offline situation
	switch(url_type)
	{
	case URL_FILE:
#if !defined(NO_URL_OPERA) || defined(HAS_OPERABLANK)
	case URL_OPERA:
#endif // !NO_URL_OPERA
	case URL_EMAIL:
	case URL_ATTACHMENT:
	case URL_NEWSATTACHMENT:
	case URL_SNEWSATTACHMENT:
	case URL_JAVASCRIPT:
#ifdef SUPPORT_DATA_URL
	case URL_DATA:
#endif
#if defined(GADGET_SUPPORT)
	case URL_WIDGET:
#endif
		return TRUE;
	}

	return FALSE;
}

BOOL URL_Manager::IsOffline(URL &url)
{
#ifdef APPLICATION_CACHE_SUPPORT
	// Application cache has it's own offline handling, thus
	// the offline mode should not be on for application cache contexts.
	// Offline mode is an older feature that "competes" with application cache.
	if (g_application_cache_manager && g_application_cache_manager->GetCacheFromContextId(url.GetContextId()))
		return FALSE;
#endif // APPLICATION_CACHE_SUPPORT

	BOOL offline = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode);
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
	offline = offline || urlManager->GetContextIsOffline(url.GetContextId());
#endif
	return offline;
}

void URL_Manager::AddContextL(URL_CONTEXT_ID id,
		OpFileFolder root_loc, OpFileFolder cookie_loc, OpFileFolder vlink_loc, OpFileFolder cache_loc,
		BOOL share_cookies_with_main_context,
		int cache_size_pref
		)
{
	URL emptyURL;
	AddContextL(id, root_loc, cookie_loc, vlink_loc, cache_loc, emptyURL, share_cookies_with_main_context, cache_size_pref);
}

void URL_Manager::AddContextL(URL_CONTEXT_ID id,
		OpFileFolder root_loc, OpFileFolder cookie_loc, OpFileFolder vlink_loc, OpFileFolder cache_loc,
		URL &parentURL,
		BOOL share_cookies_with_main_context,
		int cache_size_pref
		)
{
	if (!id) // urlManager->GetNewContextID() may have returned 0, and there are no further checks, so bail out now
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);

	/*
	 * 	Cookie_Manager must be created before Context_Manager to avoid unbalanced
	 * 	context references from URL_Rep objects created when Cache_Manager reads
	 * 	cache index from disk.
	 */
	TRAPD(op_err, Cookie_Manager::AddContextL(id, cookie_loc, share_cookies_with_main_context));

	if(OpStatus::IsSuccess(op_err))
		TRAP(op_err, Cache_Manager::AddContextL(id, vlink_loc, cache_loc, cache_size_pref, parentURL));

	if(OpStatus::IsSuccess(op_err))
		op_err = DATABASE_MODULE_ADD_CONTEXT(id, root_loc);

	if(OpStatus::IsError(op_err))
	{
		RemoveContext(id, TRUE);
		LEAVE(op_err);
	}
}

void URL_Manager::AddContextL(URL_CONTEXT_ID id,
			Context_Manager *manager,
			OpFileFolder cookie_loc,
			BOOL share_cookies_with_main_context,
			BOOL load_index
			)
{
	if (!id) // urlManager->GetNewContextID() may have returned 0, and there are no further checks, so bail out now
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);

	if (!manager) // urlManager->GetNewContextID() may have returned 0, and there are no further checks, so bail out now
		LEAVE(OpStatus::ERR_NULL_POINTER);

	/*
	 * 	Cookie_Manager must be created before the Context_Manager load the index to avoid unbalanced
	 * 	context references from URL_Rep objects created when Cache_Manager reads
	 * 	cache index from disk.
	 */
	OP_STATUS ops=AddContextManager(manager);

	if(OpStatus::IsError(ops))
		LEAVE(ops);

#ifdef DISK_CACHE_SUPPORT
#ifndef SEARCH_ENGINE_CACHE
	if(load_index)
	{
		manager->ReadDCacheFileL();
		manager->ReadVisitedFileL();
	}
#endif
#endif
}


void URL_Manager::RemoveContext(URL_CONTEXT_ID id, BOOL empty_context)
{
	Cache_Manager::RemoveContext(id, empty_context);
	Cookie_Manager::RemoveContext(id);
	DATABASE_MODULE_REMOVE_CONTEXT(id);
}

BOOL URL_Manager::ContextExists(URL_CONTEXT_ID id)
{
	return (Cookie_Manager::ContextExists(id) && Cache_Manager::ContextExists(id) ? TRUE : FALSE);
}

void URL_Manager::IncrementContextReference(URL_CONTEXT_ID context_id)
{
	Cache_Manager::IncrementContextReference(context_id);
	Cookie_Manager::IncrementContextReference(context_id);
}

void URL_Manager::DecrementContextReference(URL_CONTEXT_ID context_id)
{
	Cache_Manager::DecrementContextReference(context_id);
	Cookie_Manager::DecrementContextReference(context_id);
}

URL_CONTEXT_ID	URL_Manager::GetNewContextID()
{
	URL_CONTEXT_ID start_id = g_url_context_id_counter;
	URL_CONTEXT_ID new_id;
	unsigned int tries = 0;
	do {
		new_id = ++g_url_context_id_counter;
		if(new_id == 0)
		{
			OP_ASSERT(0); // should never happen
			if(start_id == 0)
				return 0;
			new_id = ++g_url_context_id_counter;
		}
		if(new_id == start_id)
		{
			OP_ASSERT(0); // should never happen
			return 0;
		}
		tries ++;
		if(tries > 100000)
		{
			OP_ASSERT(0); // should never happen
			return 0;
		}
	}while(ContextExists(new_id));

	return new_id;
}

#ifdef _OPERA_DEBUG_DOC_
void URL_Manager::GetMemUsed(DebugUrlMemory &debug)
{
	debug.memory_buffers += temp_buffer_len * 3* sizeof(char);
	debug.memory_buffers += temp_buffer_len * 3* sizeof(uni_char);

	//debug.memory_buffers += URL_Name::g_tempurl_len * sizeof(char);
	//debug.memory_buffers += URL_Name::g_tempurl_len * 2 * sizeof(uni_char);

//	GetCacheMemUsed(DebugUrlMemory &debug);

	ServerName *sn = servername_store.GetFirstServerName();

	while(sn)
	{
		sn->GetMemUsed(debug);
		sn = servername_store.GetNextServerName();
	}

//	GetCookieMemUsed();
}
#endif

#ifdef OPERA_PERFORMANCE

void URL_Manager::MessageStarted(OpMessage msg)
{
	if (started_profile)
	{
		message_end.timestamp_now();
		OpProbeTimestamp ts = message_end - message_start;
		double load_time = ts.get_time();
		idletime += load_time;
		if (load_time > 30)
		{
			extern const char* GetStringFromMessage(OpMessage);
			urlManager->AddToMessageDelayLog(GetStringFromMessage(msg), GetStringFromMessage(previous_msg), message_start, load_time);
		}
	}
}

void URL_Manager::MessageStopped(OpMessage msg)
{
	previous_msg = msg;
	message_start.timestamp_now();
}

void URL_Manager::OnStartLoading()
{
	if (opera_performance_enabled && !started_profile && user_initiated)
	{
		idletime = 0;
#ifdef SUPPORT_PROBETOOLS
		g_probetools_helper->OnStartLoading();
#endif // SUPPORT_PROBETOOLS
		started_profile = TRUE;
		loading_started_called = TRUE;
		session_start.timestamp_now();
#ifdef NETWORK_STATISTICS_SUPPORT
		urlManager->GetNetworkStatisticsManager()->Reset();
#endif // NETWORK_STATISTICS_SUPPORT
		http_Manager->Reset();
		ClearLogs();
	}
}

void URL_Manager::OnLoadingFinished()
{
	if (opera_performance_enabled && user_initiated && loading_started_called)
	{
#ifdef NETWORK_STATISTICS_SUPPORT
		urlManager->GetNetworkStatisticsManager()->Freeze();
#endif // NETWORK_STATISTICS_SUPPORT
		http_Manager->Freeze();
		session_end.timestamp_now();
#ifdef SUPPORT_PROBETOOLS
		g_probetools_helper->OnLoadingFinished();
#endif // SUPPORT_PROBETOOLS
		user_initiated = FALSE;
		started_profile = FALSE;
		loading_started_called = FALSE;
	}
}

void URL_Manager::OnUserInitiatedLoad()
{
	if (opera_performance_enabled)
		user_initiated = TRUE;
}

void URL_Manager::AddToMessageLog(const char *message_type, OpProbeTimestamp &start_processing, double &processing_diff)
{
	if (opera_performance_enabled && started_profile)
	{
		Message_Processing_Log_Point *temp = OP_NEW(Message_Processing_Log_Point,(message_type, start_processing, processing_diff));
		if (temp)
			temp->Into(&message_handler_log);
	}
}

void URL_Manager::AddToMessageDelayLog(const char *message_type, const char *old_message_type, OpProbeTimestamp &start_processing, double &processing_diff)
{
	if (opera_performance_enabled && started_profile)
	{
		Message_Delay_Log_Point *temp = OP_NEW(Message_Delay_Log_Point,(message_type, old_message_type, start_processing, processing_diff));
		if (temp)
			temp->Into(&message_delay_log);
	}
}

void URL_Manager::ClearLogs()
{
	while (!message_handler_log.Empty())
	{
		Message_Processing_Log_Point *temp = (Message_Processing_Log_Point*)message_handler_log.First();
		temp->Out();
		OP_DELETE(temp);
	}
	while (!message_delay_log.Empty())
	{
		Message_Delay_Log_Point *temp = (Message_Delay_Log_Point*)message_delay_log.First();
		temp->Out();
		OP_DELETE(temp);
	}
}

void URL_Manager::GeneratePerformancePage(URL& url)
{
	EnablePerformancePage();
#ifdef SUPPORT_PROBETOOLS
	g_probetools_helper->GeneratePerformancePage(url);
#endif // SUPPORT_PROBETOOLS

	OpProbeTimestamp ts = session_end - session_start;
	double load_time = ts.get_time();
	url.WriteDocumentDataUniSprintf(UNI_L("<h2>Page load time: %.0f milliseconds</h2>"), load_time);

#ifdef NETWORK_STATISTICS_SUPPORT
	GetNetworkStatisticsManager()->WriteReport(url);
#endif // NETWORK_STATISTICS_SUPPORT

	http_Manager->WriteReport(url, session_start);

	url.WriteDocumentDataUniSprintf(UNI_L("<h2>message processing taking longer than 100 ms</h2><table><tr><th>Processing start time</th><th>processing time</th><th>Message</th></tr>"));
	Message_Processing_Log_Point *temp = (Message_Processing_Log_Point *)message_handler_log.First();
	while (temp)
	{
		OpString temp_string;
		temp_string.Set(temp->message_string);
		OpProbeTimestamp ts = temp->time_start_processing - session_start;
		double pa = ts.get_time();
		url.WriteDocumentDataUniSprintf(UNI_L("<tr><td>%.0f</td><td>%.0f</td><td>%s</td></tr>"), pa, temp->time, temp_string.CStr());
		temp = (Message_Processing_Log_Point *)temp->Suc();
	}
	url.WriteDocumentDataUniSprintf(UNI_L("</table>"));

	url.WriteDocumentDataUniSprintf(UNI_L("<h2>Delays between message processing (often due to OS/network activity or screen updates) taking longer than 30 ms</h2><table><tr><th>Delay start time</th><th>Delay time</th><th>Previous Message</th><th>Current Message</th></tr>"));
	Message_Delay_Log_Point *temp2 = (Message_Delay_Log_Point *)message_delay_log.First();
	while (temp2)
	{
		OpString temp_string;
		OpString old_temp_string;
		temp_string.Set(temp2->message_string);
		old_temp_string.Set(temp2->old_message_string);
		OpProbeTimestamp ts = temp2->time_start_processing - session_start;
		double pa = ts.get_time();
		url.WriteDocumentDataUniSprintf(UNI_L("<tr><td>%.0f</td><td>%.0f</td><td>%s</td><td>%s</td></tr>"), pa, temp2->time, old_temp_string.CStr(), temp_string.CStr());
		temp2 = (Message_Delay_Log_Point *)temp2->Suc();
	}
	url.WriteDocumentDataUniSprintf(UNI_L("</table>"));
	ClearLogs();
}
#endif // OPERA_PERFORMANCE

#ifdef URL_NETWORK_LISTENER_SUPPORT
void URL_Manager::RegisterListener(URL_Manager_Listener *listener)
{
	if(!listener)
		return;
	listener->IntoStart(&listeners);
}

void URL_Manager::UnregisterListener(URL_Manager_Listener *listener)
{
   if(listener->InList())
	listener->Out();
}

void URL_Manager::NetworkEvent(URLManagerEvent network_event, URL_ID id)
{
	URL_Manager_Listener *p = static_cast<URL_Manager_Listener *>(listeners.First());
	while (p)
	{
		p->NetworkEvent(network_event, id);
		p = static_cast<URL_Manager_Listener *>(p->Suc());
	}
}
#endif

URL URL_Manager::GenerateInvalidHostPageUrl(const uni_char* url, URL_CONTEXT_ID ctx_id
#ifdef ERROR_PAGE_SUPPORT
											, OpIllegalHostPage::IllegalHostKind kind
#endif // ERROR_PAGE_SUPPORT
											)
{
	OpString temporary_url;
	temporary_url.AppendFormat(UNI_L("opera:illegal-url-%d"), opera_url_number++);

	if(temporary_url.HasContent())
	{
		URL error_url = GetURL(temporary_url.CStr(), ctx_id);

		if(error_url.IsValid() && OpStatus::IsSuccess(error_url.GetRep()->name.illegal_original_url.Set(url)))
		{
#ifdef ERROR_PAGE_SUPPORT
			error_url.SetAttribute(URL::KInvalidURLKind, (int) kind);
#endif // ERROR_PAGE_SUPPORT
			return error_url;
		}
	}

	return URL();
}

ServerName*	URL_Manager::GetServerName(const char *name, BOOL create /*= FALSE*/, BOOL normalize /*= TRUE*/)
{
	// Assumes name is UTF-8 (%xx is also assumed to be UTF-8)
	if(name == NULL)
		return NULL;

	size_t len = op_strlen(name);
	size_t i = len;
	const char *test = name;
	while(i>0)
	{
		unsigned char test_char = (unsigned char) *(test++);
		if(test_char > 127 || (test_char >= 'A' && test_char <= 'Z')) // Uppercase letters are sent through IDNA prep
			break;
		i--;
	}

	if(i==0 && (!normalize || op_strspn(name, "0123456789ABCDEFabcdefXx.") != len))
	{
		ServerName *name_cand = servername_store.GetServerName(name);
		if(name_cand)
			return name_cand;
	}

	OP_STATUS op_err = OpStatus::OK;
	RETURN_VALUE_IF_ERROR(CheckTempbuffers(5*len+256), NULL);
	make_doublebyte_in_buffer(name, len, uni_temp_buffer3, IDNA_TempBufSize);
	unsigned short dummy_port=0;
	return IDNA::GetServerName(servername_store, op_err, uni_temp_buffer3, dummy_port, create, normalize);
}

ServerName*	URL_Manager::GetServerName(OP_STATUS &op_err, const uni_char *name, unsigned short &port, BOOL create /*= FALSE*/, BOOL normalize /*= TRUE*/)
{
	return IDNA::GetServerName(servername_store, op_err, name, port, create, normalize);
}

OP_STATUS URL_Manager::ManageTCPReset(const URL &url_source, URL &url_dest, MH_PARAM_2 error, BOOL &execute_redirect)
{
	BOOL no_content = FALSE;
	HTTP_Method method = (HTTP_Method) url_source.GetAttribute(URL::KHTTP_Method);

	execute_redirect = FALSE;

	if (!url_source.GetRep()->GetDataStorage() || !url_source.GetRep()->GetDataStorage()->GetCacheStorage())
		no_content = TRUE;

	if (no_content && method == HTTP_METHOD_GET && error == ERR_COMM_CONNECTION_CLOSED && (url_source.Type() == URL_HTTP || url_source.Type() == URL_HTTPS))
	{
		// If we receive a RST on a URL without any kind of third level domain, we add "www."  to see if it works
		// See CORE-45283 for details
		BOOL is_third_level_domain_missing = url_source.GetServerName()->GetDomainTypeASync() == ServerName::DOMAIN_NORMAL && url_source.GetServerName()->GetParentDomain()->GetDomainTypeASync()==ServerName::DOMAIN_TLD;

		if (is_third_level_domain_missing)
		{
			OpString8 newName;
			BOOL add_port = FALSE;

			// Build the new URL, adding "www." and replicating port and path
			if ( (url_source.Type() == URL_HTTP && url_source.GetServerPort() != 80 && url_source.GetServerPort() != 0) ||
					(url_source.Type() == URL_HTTPS && url_source.GetServerPort() != 443 && url_source.GetServerPort() != 0))
					add_port = TRUE;

			OP_STATUS ops = newName.AppendFormat("http%s://www.%s", (url_source.Type() == URL_HTTPS) ? "s" : "", url_source.GetServerName()->GetName().CStr());

			if (OpStatus::IsSuccess(ops) && add_port)
				ops = newName.AppendFormat(":%d", url_source.GetServerPort());

			if (OpStatus::IsSuccess(ops))
			{
				OpString8 url_data;
					
				TRAP(ops, url_source.GetAttributeL(URL::KPathAndQuery_L,url_data));
					
				if(OpStatus::IsSuccess(ops))
					ops = newName.AppendFormat("%s", url_data.CStr());
			}

			// Build the new URL and notify the caller
			if (OpStatus::IsSuccess(ops))
			{
				url_dest = urlManager->GetURL(newName, url_source.GetContextId());

				execute_redirect = TRUE;

				return OpStatus::OK;
			}

			return ops;
		}
	}

	return OpStatus::OK;
}
