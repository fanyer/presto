/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SELFTEST
#include "modules/cache/cache_selftest.h"
#include "modules/cache/cache_st_helpers.h"
#include "modules/cache/cache_debug.h"
#include "modules/cache/cache_ctxman_disk.h"
#include "modules/cache/url_stor.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"


#ifdef DISK_CACHE_SUPPORT
OP_STATUS CS_MessageObject::CheckContentDuringLoad(URL_Rep *rep)
{
	if(!expected_content || !expected_content->IsOpen())
		return OpStatus::OK;

	OP_ASSERT(rep);

	if(!rep)
	{
		ST_failed("NULL URL during loading verify");

		return OpStatus::ERR_NULL_POINTER;
	}

	URL_DataDescriptor *dd=rep->GetDescriptor(NULL, URL::KNoRedirect, TRUE);
	BOOL more;

	if(!dd)
	{
		ST_failed("No Data descriptor");

		return OpStatus::ERR_NULL_POINTER;
	}

	URL_DataStorage *ds=rep->GetDataStorage();

	if(!ds)
	{
		ST_failed("URL_DataStorage NULL during loading verify");

		OP_DELETE(dd);

		return OpStatus::ERR_NULL_POINTER;
	}

	//OpFileLength start = ds->GetNextRangeStart();
	OP_STATUS ops1;
	OP_STATUS ops2=OpStatus::OK;
	OpFileLength move_pos=(waiting_for==FILE_LENGTH_NONE) ? 0 : waiting_for;

	if( OpStatus::IsError(ops1=expected_content->SetFilePos(move_pos)) ||
		OpStatus::IsError(ops2=dd->SetPosition(move_pos)))
	{
		ST_failed("Error accessing position %d: %d - %d", (int)move_pos, (int) ops1, (int) ops2);

		OP_ASSERT(OpStatus::IsSuccess(ops1));
		OP_ASSERT(OpStatus::IsSuccess(ops2));

		OP_DELETE(dd);

		return OpStatus::IsError(ops1) ? ops1 : ops2;
	}

	unsigned full_length = 0;

	do
	{
		unsigned text_length = dd->RetrieveData(more);
		OpFileLength bytes_read;
		const char *net_buf=dd->GetBuffer();

		full_length += text_length;

		if(!net_buf)
		{
			ST_failed("network buffer NULL during loading verify");

			OP_DELETE(dd);

			return OpStatus::ERR_NULL_POINTER;
		}

		if(!text_length)
		{
			ST_failed("No bytes available! Problem with StopLoading() or Cache_Storage::SetFinished()?");

			OP_DELETE(dd);

			return OpStatus::ERR_NULL_POINTER;
		}

		// Check the content against the file
		char *temp=OP_NEWA(char, text_length);

		if(!temp)
		{
			OP_DELETE(dd);

			ST_failed("Out of memory!");

			return OpStatus::ERR_NO_MEMORY;
		}

		OP_STATUS ops=expected_content->Read(temp, text_length, &bytes_read);

		if(OpStatus::IsSuccess(ops) && text_length!=bytes_read)
			ops=OpStatus::ERR;

		if(OpStatus::IsSuccess(ops))
		{
			for(unsigned i=0; i<text_length; i++)
			{
				if(temp[i]!=net_buf[i])
					ops=OpStatus::ERR_OUT_OF_RANGE;
			}
		}

		dd->ConsumeData(text_length);

		OP_DELETEA(temp);

		if(OpStatus::IsError(ops))
		{
			ST_failed("Content does not match the expected values!");

			OP_DELETE(dd);
			return ops;
		}
	}
	while(more);

	output("%d bytes have been read starting from %d\n", (INT32)full_length, (INT32)move_pos);

	OP_DELETE(dd);
	return OpStatus::OK;
}
#endif // DISK_CACHE_SUPPORT

void WaitSingleURL::OnTimeOut(OpTimer* timer)
{
	if(!activity)
		ST_failed("Inactivity for %d ms - URL: %s", wait_time, str_url_downloaded.CStr());
	else
	{
		timer->Stop();
		timer->Start(wait_time);
		activity=FALSE;
	}
}

void WaitSingleURL::ST_End()
{
	StopTimeout();

#ifdef WEBSERVER_SUPPORT
	if(uct)
		uct->UnsetCallBacks(this);
#endif

	if(auto_delete)
		OP_DELETE(this);
}

WaitSingleURL::~WaitSingleURL()
{
	StopTimeout();
	
#ifdef WEBSERVER_SUPPORT
	if(uct)
		uct->UnsetCallBacks(this);
#endif
}

#ifdef DISK_CACHE_SUPPORT
void CustomLoad::CreateMessageHandler()
{
	OP_ASSERT(!mh); 
	
	if(!mh)
	{
		mh=OP_NEW(CS_MessageHandler, ());

		if(mh)
			mh->SetDefaultMessageObject(wait);
	}
}

CustomLoad::~CustomLoad()
{
	if(mh && wait)
		mh->UnsetCallBacks(wait);

	if(mh)
		OP_DELETE(mh);
}
#endif // DISK_CACHE_SUPPORT

void WaitSingleURL::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	activity=TRUE;

	if(msg==MSG_URL_DATA_LOADED)
	{
		packet_num++;

		URL_Rep *rep=(URL_Rep *)par1;

		if(download_moment==packet_num)
		{
			if(ManageLoadToFile(rep, num_headers_executed-1))
				return;
		}

		if(rep->GetAttribute(URL::KLoadStatus, URL::KNoRedirect)==URL_LOADED)
		{
			StopTimeout();
			str_url_downloaded.Empty();

		#if defined WEBSERVER_SUPPORT && defined DISK_CACHE_SUPPORT
			if(uct)
				uct->PrintTransferredBytes();
			else
				output("No UniteCacheTester on MSG_URL_DATA_LOADED!\n");
		#endif
			
			UINT32 response_code=rep->GetAttribute(URL::KHTTP_Response_Code);
			
			if(response_code==expected_response_code ||
			  (expected_response_code && response_code==200) || // Forgiving support for HTTP ranges
			  (expected_response_code==200 && !response_code))  // Forgiving support for file://
			{
			#ifdef DISK_CACHE_SUPPORT
				if(OpStatus::IsSuccess(CheckContentDuringLoad(rep)))
			#endif // DISK_CACHE_SUPPORT
				{
					if((URLStatus)rep->GetAttribute(URL::KLoadStatus, URL::KFollowRedirect)!=URL_LOADED)
						output("***** Suspicious URL status: %d\n", (UINT32)rep->GetAttribute(URL::KLoadStatus, URL::KFollowRedirect));

					num_load_executed++;

					if(expect_error)
					{
						ST_failed("A network error was expected while accessing %s, but it did not happen [got response code: %d]...", rep->GetAttribute(URL::KName).CStr(), response_code);
						ST_End();
					}
					else
					{
					#if defined WEBSERVER_SUPPORT && defined DISK_CACHE_SUPPORT
						if( (min_exp_trnsf_bytes==-1 || (int)uct->GetLastTransfer()>=min_exp_trnsf_bytes) &&
							(max_exp_trnsf_bytes==-1 || (int)uct->GetLastTransfer()<=max_exp_trnsf_bytes))
						{
							if(num_load_executed==num_expected_load || num_expected_load==-1)
							{
								if(enable_st_passed)
									ST_passed();
								ST_End();
							}
							else if(num_load_executed>num_expected_load)
							{
								ST_failed("Test inconsistency: download %d when only %d where expected", num_load_executed, num_expected_load);
								ST_End();
							}
							else
								output("%s download number %d completed\n", GetDownloadStorage(num_load_executed-1)?"'normal'":"'download storage'", num_load_executed);
						}
						else
						{
							// We can end up here multiple times, if num_expected_load>1
							// For the transfer after the first, we usually expect 0 bytes transfered, but the original expected amount could also be fine
							// For the first transfer, 0 bytes is fine if accept_no_transfer is TRUE

							if((num_load_executed==1 && !accept_no_transfer) || (int)uct->GetLastTransfer()>0)
							{
								ST_failed("Wrong number of bytes transfered on %s download %d: %d is not in %d - %d range", GetDownloadStorage(num_load_executed-1)?"'normal'":"'download storage'", num_load_executed, (int)uct->GetLastTransfer(), min_exp_trnsf_bytes, max_exp_trnsf_bytes);
								ST_End();
							}
							else if(num_expected_load==num_load_executed || num_expected_load==-1) // Last tranfer, or every transfer is a pass
							{
								if(enable_st_passed)
									ST_passed();
								ST_End();
							}

							// Nothing to do on the intermediate transfers
						}
					#else
						{
							if(enable_st_passed)
								ST_passed();
							ST_End();
						}
					#endif// WEBSERVER_SUPPORT
					}
				}
			}
			else
			{
				ST_failed("Wrong response code: %d instead of %d", response_code, expected_response_code);
				ST_End();
			}
		}
#ifdef DISK_CACHE_SUPPORT
		else
		{
			CheckContentDuringLoad(rep);
		}
#endif // DISK_CACHE_SUPPORT
	}
	else if(msg==MSG_URL_LOADING_FAILED)
	{
		StopTimeout();
		str_url_downloaded.Empty();
	#if defined WEBSERVER_SUPPORT && defined DISK_CACHE_SUPPORT
		if(uct)
			uct->PrintTransferredBytes();
		else
			output("No UniteCacheTester on MSG_URL_LOADING_FAILED!\n");
	#endif

		if(expect_error)
		{
			ST_passed();
			ST_End();
		}
		else
		{
		#if defined(WEBSERVER_SUPPORT)
			if(g_webserver)
			{
				URL_Rep *rep=urlManager->LocateURL((URL_ID)par1);
				// NB: par1 is supposed to be the ID. For now the implementation returns the URL_Rep
				OP_ASSERT((URL_Rep *)par1 == rep);
				
				ST_failed("URL failed to load - error: 0x%x - URL: %s\n", (UINT32)par2, (rep)?rep->GetAttribute(URL::KName).CStr(): " UNKNOWN ");
				ST_End();
			}
			else
		#endif
			{
				ST_failed("URL failed to load - error: 0x%x - Unite disabled\n", (UINT32)par2);
				ST_End();
			}
		}
	}
	else if(msg==MSG_HEADER_LOADED)
	{
		packet_num=0;
		num_headers_executed++;
		OP_ASSERT(num_headers_executed>num_load_executed);

		output("%s download number %d: headers arrived\n", GetDownloadStorage(num_headers_executed-1)?"'download storage'":"'normal'", num_headers_executed);

		if(download_moment==0)
		{
			if(ManageLoadToFile((URL_Rep *)par1, num_headers_executed-1))
				return;
		}
	}
	else
	{
		StopTimeout();
		str_url_downloaded.Empty();

		ST_failed("Unhandled message %d", (UINT32)msg);
		ST_End();
	}
}

BOOL WaitSingleURL::ManageLoadToFile(URL_Rep *rep, int pos, BOOL force)
{
	if(GetDownloadStorage(pos) || force)
	{
	#ifdef WEBSERVER_SUPPORT
		if(!rep)
		{
			ST_failed("Error: rep NULL");
			ST_End();

			return TRUE;
		}
		else
		{
			OpString name;

			name.AppendFormat(UNI_L("sesn%coprd%d.tmp"), PATHSEPCHAR, num_downloads++);
			
			///////// Do it during MSG_HEADER_LOADED
			OP_STATUS ops=rep->LoadToFile(name);

			if(OpStatus::IsError(ops))
			{
				ST_failed("Error during Download Storage creation: %d", ops);
				ST_End();

				return TRUE;
			}
		}
	#else
		OP_ASSERT(!"This test requires Unite...");
	#endif

		return FALSE;
	}

	return FALSE;
}

#ifdef DISK_CACHE_SUPPORT
#ifdef WEBSERVER_SUPPORT
void UniteCacheTester::StartWebServer(WebserverListeningMode m)
{
	OP_MEMORY_VAR WebserverListeningMode mode = m;

	if(!g_webserver && !g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverEnable))
	{
		TRAPD(op_ignore, g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverEnable, 1));

		OperaInitInfo info;

		TRAPD(op_err, g_opera->webserver_module.InitL(info));

		if(OpStatus::IsSuccess(op_err))
		{
			webserver_module_initialized = TRUE;

			OP_ASSERT(g_webserver);

			output("Webserver module reinitialized - Unite temporarily enabled for thes selftests \n");
		}
		else
		{
			TRAPD(op_ignore, g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverEnable, 0));
		}
	}

	lsn_start.SetWaitForProxy(FALSE);

	if(mode & WEBSERVER_LISTEN_PROXY)
	{
		OpString8 user, dev, proxy, pw;

		if(!IsUniteProperlyConfigured(user, dev, proxy, pw))
		{
			output("*** Unite not properly configured (please check user, device, proxy and password). Not waiting for Proxy. ***\n");

			mode ^= WEBSERVER_LISTEN_PROXY;
		}
		else
			lsn_start.SetWaitForProxy(TRUE);
	}

	if(g_webserver && g_webserver->IsRunning())
	{
		output("\nUnite Webserver already running - Listening: %d\n", g_webserver->IsListening());
		
		ST_passed();
	}
	else
	{
		if (!g_webserver)
			ST_failed("Unite Webserver disabled by preferences\n");
		else
		{
			ws_started=TRUE;

			if(mode & WEBSERVER_LISTEN_PROXY)
			{
				g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::UseOperaAccount, TRUE);
				g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverEnable, TRUE);
				g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverListenToAllNetworks, TRUE);
			}
			int port = g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverPort);
			if( OpStatus::IsError(g_webserver->AddWebserverListener(&lsn_start)) || 
				OpStatus::IsError(g_webserver->Start(mode, port
			#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
													 , NULL
			#endif
													 UPNP_PARAM_COMMA(NULL))))
				ST_failed("Error during Unite WebServer initialization!");
			else
				output("Unite Webserver successfully started for this test\n");
		}
	}
}

void UniteCacheTester::StopWebServer()
{
	lsn_start.EnableProxyDisconnect();
	if(g_webserver)
		g_webserver->RemoveWebserverListener(&lsn_start);

	if(new_subserver)
	{
		if(g_webserver)
			g_webserver->RemoveSubServer(new_subserver);
		new_subserver=NULL;
	}
	
	if(ws_started)
	{
		if(g_webserver && OpStatus::IsError(g_webserver->Stop()))
			ST_failed("Error stopping the webserver!");

		ws_started=FALSE;
	}

	if(webserver_module_initialized)
	{
		g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverEnable, 0);
		TRAPD(op_err, g_opera->webserver_module.Destroy());

		webserver_module_initialized=FALSE;
		output("Webserver module destroyed - Unite disabled to restore the initial state \n");
	}
}

void UniteCacheTester::StartService(const uni_char *name, const uni_char *storage_path)
{
	if(g_webserver && g_webserver->IsRunning() && g_webserver->GetLocalListeningPort())
	{
		/* We install a dummy service in this window */
		OpString storagePath;
		
		OP_ASSERT(new_subserver==NULL);
		
		if(new_subserver)
		{
			g_webserver->RemoveSubServer(new_subserver);
			new_subserver=NULL;
		}
		
		if( OpStatus::IsError(g_folder_manager->GetFolderPath(OPFILE_TEMP_FOLDER, storagePath)) ||
			OpStatus::IsError(storagePath.Append(storage_path)) ||
			OpStatus::IsError(service_name.Set(name)) ||
			OpStatus::IsError(WebSubServer::Make(new_subserver, g_webserver, win->Id(), storagePath, name, name, UNI_L("Opera Software ASA"), UNI_L("This is a test service used for SELFTESTS"), UNI_L("download url unknown"), TRUE)) ||
			OpStatus::IsError(g_webserver->AddSubServer(new_subserver)))
			{
				OP_DELETE(new_subserver);
				
				ST_failed("Impossible to install the service");
			}
		
		OpString8 str8;

		str8.Set(name);
		output("WebServer running on port %d with %d services - created %s\n", (UINT32) g_webserver->GetLocalListeningPort(), g_webserver->GetSubServerCount(), str8.CStr());

		ST_passed();
	}
	else
	{
		ST_failed("WebServer not running...\n ");
	}
}

OP_STATUS UniteCacheTester::Share(const uni_char *file_path, const char *str_vpath, OpFileLength *size, URL &dest_url, URL_CONTEXT_ID ctx)
{
	// Create and check the file
	OpFile file;
	BOOL exists=FALSE;
	OpFileLength file_size=0;

	if( OpStatus::IsError(file.Construct(file_path)) ||
		OpStatus::IsError(file.Exists(exists)) ||
		!exists ||
		OpStatus::IsError(file.GetFileLength(file_size)) ||
		file_size<=0)
	{
		OpString8 path8;

		path8.Set(file_path);

		ST_failed("Error on accessing the cache file to share it - %s - exists: %d - size: %d!", path8.CStr(), exists, file_size);
		
		return OpStatus::ERR;
	}
	
	if(size)
		*size=file_size;
		
	OpString vpath;
	
	vpath.Set(str_vpath);

	WebserverResourceDescriptor_Base *m_res_desc = WebserverResourceDescriptor_Static::Make(file_path, vpath);

	if (m_res_desc == NULL || !new_subserver ||
		OpStatus::IsError(new_subserver->AddSubserverResource(m_res_desc)))
	{
		OP_DELETE(m_res_desc);
		ST_failed("Error on sharing files on the Unite server");
		
		return OpStatus::ERR;
	}
	
	dest_url=GetURL(str_vpath, ctx);
	
	return OpStatus::OK;
}

URL UniteCacheTester::GetURL(const char *str_vpath, URL_CONTEXT_ID ctx)
{
	OpString8 url_path8;
	OpString url_path;

	if (g_webserver)
	{
		url_path8.AppendFormat("http://localhost:%d/%s/%s", g_webserver->GetLocalListeningPort(), service_name.CStr(), str_vpath);
		url_path.Set(url_path8.CStr());
	}

	//output("Downloading %s\n", url_path8.CStr());
	
	return g_url_api->GetURL(url_path.CStr(), ctx);
}

#endif // WEBSERVER_SUPPORT

URL CacheTester::GetURLTestServerSize(URL_CONTEXT_ID ctx, UINT32 size, const char* id, const char *mime_type)
{
	OpString url_path;
	OpString8 url_path8;
		
	url_path8.AppendFormat("http://testsuites.oslo.opera.com/core/networking/http/cache/data/cache.php?size=%d&type=%s&id=%s", size, (mime_type)?mime_type:"text/html", (id)?id:"");
	url_path.Set(url_path8.CStr());
	
	//output("Downloading %s\n", url_path8.CStr());

	return g_url_api->GetURL(url_path.CStr(), ctx);
}

URL CacheTester::GetURLExternalTestServerSize(URL_CONTEXT_ID ctx, UINT32 size, const char* id, const char *mime_type)
{
	OpString url_path;
	OpString8 url_path8;

	url_path8.AppendFormat("http://team.opera.com/testbed/core/networking/cache-data.php?size=%d&type=%s&id=%s", size, (mime_type)?mime_type:"text/html", (id)?id:"");
	url_path.Set(url_path8.CStr());

	//output("Downloading %s\n", url_path8.CStr());

	return g_url_api->GetURL(url_path.CStr(), ctx);
}

#ifdef WEBSERVER_SUPPORT

OP_STATUS UniteCacheTester::UnShare(const char *str_vpath)
{
	if(!new_subserver)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	OpString str;
	
	RETURN_IF_ERROR(str.Set(str_vpath));
	
	WebserverResourceDescriptor_Base *res=new_subserver->RemoveSubserverResource(str);
	
	if(!res)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
		
	OP_DELETE(res);
	
	return OpStatus::OK;
}

void UniteCacheTester::PrintTransferredBytes()
{
	if(g_webserver && g_webserver->IsRunning() && new_subserver)
	{
		output(" Transferred bytes: %d (%d new bytes) - %d\n", (UINT32)g_webserver->GetBytesUploaded(), GetNewBytesTransferred(), (UINT32)g_webserver->GetBytesDownloaded());
	}
	else
		output("PrintTransferredBytes not available!\n");
}

UINT32 UniteCacheTester::GetNewBytesTransferred()
{
	if(g_webserver && g_webserver->IsRunning() && new_subserver)
	{
		UINT32 ret=(UINT32)(g_webserver->GetBytesUploaded()-last_bytes);

		last_transfer=ret;
		last_bytes=g_webserver->GetBytesUploaded();

		return ret;
	}

	return (UINT32)-1;
}

UINT32 UniteCacheTester::GetLastTransfer()
{
	return (UINT32) last_transfer;
}

void UniteCacheTester::LoadOK(URL &url, OpFileDescriptor *expected_content, CustomLoad *cl)
{
	if(!CS_MessageHandler::CheckUniteURL(url))
		return;

	int expected=200;

	if(url.GetAttribute(URL::KType)==URL_FILE)
		expected=0;

	WaitSingleURL *mo=NULL;

	if(cl)
		mo=cl->wait;

	if(!mo)
		mo=CreateDefaultMessageObject(TRUE);

	if(cl && cl->mh)
	{
		cl->mh->SetExpectedTransferRange(mh.min_exp_trnsf_bytes, mh.max_exp_trnsf_bytes, FALSE);
		cl->mh->LoadUnconditional(url, expected, mo, expected_content, cl);
	}
	else
	{
		mh.expect_error=FALSE;
		mh.LoadUnconditional(url, expected, mo, expected_content, cl);
	}
}

void UniteCacheTester::LoadNoReloadOK(URL &url, OpFileDescriptor *expected_content, BOOL assume_no_transfer)
{
	if(!CS_MessageHandler::CheckUniteURL(url))
		return;

	URLStatus st=(URLStatus)url.GetAttribute(URL::KLoadStatus);

	if(st!=URL_LOADED)
		ST_failed("URL status wrong");
	else
	{
		int expected=200;

		if(url.GetAttribute(URL::KType)==URL_FILE)
			expected=0;

		if(assume_no_transfer)
			SetExpectedTransferRange(0, 0);

		mh.LoadNoReload(url, expected, CreateDefaultMessageObject(TRUE), expected_content);
	}
}

void UniteCacheTester::LoadNormalOK(URL &url, OpFileDescriptor *expected_content)
{
	if(!CS_MessageHandler::CheckUniteURL(url))
		return;

	int expected=200;

	if(url.GetAttribute(URL::KType)==URL_FILE)
		expected=0;

	mh.expect_error=FALSE;

	mh.LoadNormal(url, expected, CreateDefaultMessageObject(TRUE), expected_content);
}

void UniteCacheTester::LoadStatus(URL &url,  int http_status_code)
{
	SetExpectedTransferRange(0, 2048);				// We expect the 404 page transfer

	mh.LoadUnconditional(url, http_status_code, CreateDefaultMessageObject(TRUE), NULL); // No expected content, because a 404 is the required result
}

void UniteCacheTester::LoadError(URL &url)
{
	SetExpectedTransferRange(0, 0, TRUE);				// We expect the 404 page transfer
	mh.LoadUnconditional(url, 404, CreateDefaultMessageObject(TRUE), NULL); // No expected content, because a 404 is the required result
}

void UniteCacheTester::LoadNoReload404(URL &url)
{
	SetExpectedTransferRange(0, 2048);  		// We expect the 404 page transfer
	mh.LoadNoReload(url, 404, CreateDefaultMessageObject(TRUE), NULL); 	// No expected content, because a 404 is the required result
}

URL UniteCacheTester::JScreatePage(int file_size, int age, URL_CONTEXT_ID ctx, BOOL start_download, BOOL never_flush)
{
	OpString8 url_str;

	if (g_webserver)
		url_str.AppendFormat("http://localhost:%d/cache_js_selftest/generate_page?size=%d&age=%d&id=%d", g_webserver->GetLocalListeningPort(), (int)file_size, age, op_rand());
	else
	{
		URL dummy = g_url_api->GetURL("http://example.tld/dummy", NULL, TRUE, ctx);

		ST_failed("Unite is disabled!");

		return dummy;
	}
	// output("Loading from %s\r\n", url_str.CStr());

	URL url=g_url_api->GetURL(url_str, ctx);

	if(never_flush)
		url.SetAttribute(URL::KNeverFlush, TRUE);

	if(start_download)
	{
		SetExpectedTransferRange(file_size, file_size+1024+file_size/1024 /* Add some more margin for big files */);
		LoadOK(url, NULL);
	}

	return url;
}

BOOL UniteCacheTester::IsUniteProperlyConfigured(OpString8 &user, OpString8 &dev, OpString8 &proxy, OpString8 &pw)
{
	OpStatus::Ignore(user.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser)));
	OpStatus::Ignore(dev.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice)));
	OpStatus::Ignore(proxy.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverProxyHost)));
	OpStatus::Ignore(pw.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverHashedPassword)));

	if(user.IsEmpty() || dev.IsEmpty() || proxy.IsEmpty() || pw.IsEmpty())
		return FALSE;

	return TRUE;
}

URL UniteCacheTester::JScreateExternalPage(int file_size, int age, URL_CONTEXT_ID ctx, BOOL start_download)
{
	OpString8 user;
	OpString8 dev;
	OpString8 proxy;
	OpString8 pw;

	if(!IsUniteProperlyConfigured(user, dev, proxy, pw))
	{
		output("*** Unite not properly configured (please check user, device, proxy and password). Switching to localhost. ***\n");

		return JScreatePage(file_size, age, ctx, start_download);
	}

	OpString8 url_str;

	if (g_webserver)
		url_str.AppendFormat("http://%s.%s.%s/cache_js_selftest/generate_page?size=%d&age=%d&id=%d", dev.CStr(), user.CStr(), proxy.CStr(), (int)file_size, age, op_rand());
	// output("Loading from %s\r\n", url_str.CStr());

	URL url=g_url_api->GetURL(url_str, ctx);

	if(start_download)
	{
		SetExpectedTransferRange(file_size, file_size+1024);
		LoadOK(url, NULL);
	}

	return url;
}

void UniteCacheTester::JSResetSteps(URL_CONTEXT_ID ctx, int step)
{
	OpString8 url_str;

	if (g_webserver)
		url_str.AppendFormat("http://localhost:%d/cache_js_selftest/restart_steps?step=%d", g_webserver->GetLocalListeningPort(), step);

	URL url=g_url_api->GetURL(url_str, ctx);

	SetExpectedTransferRange(-1, -1);
	LoadOK(url, NULL);
}

void UniteCacheTester::JSAutoStepNormal(URL_CONTEXT_ID ctx, int file_size, Transfer exp_transfer)
{
	OpString8 url_str;

	if (g_webserver)
		url_str.AppendFormat("http://localhost:%d/cache_js_selftest/auto_step?size=%d", g_webserver->GetLocalListeningPort(), file_size);

	URL url=g_url_api->GetURL(url_str, ctx);

	if(exp_transfer==FULL)
		SetExpectedTransferRange(file_size, file_size+1024);
	else if(exp_transfer==NONE)
		SetExpectedTransferRange(0, 0);
	else
		SetExpectedTransferRange(0, 1024);

	output("URL status: %d\n", (UINT32)url.GetAttribute(URL::KLoadStatus, URL::KFollowRedirect));

	LoadNormalOK(url, NULL);
}

void UniteCacheTester::JSAutoStepNoReload(URL_CONTEXT_ID ctx, int file_size, Transfer exp_transfer)
{
	OpString8 url_str;

	if (g_webserver)
		url_str.AppendFormat("http://localhost:%d/cache_js_selftest/auto_step", g_webserver->GetLocalListeningPort());

	URL url=g_url_api->GetURL(url_str, ctx);

	if(exp_transfer==FULL)
		SetExpectedTransferRange(file_size, file_size+1024);
	else if(exp_transfer==NONE)
		SetExpectedTransferRange(0, 0);
	else
		SetExpectedTransferRange(0, 1024);

	output("URL status: %d\n", (UINT32)url.GetAttribute(URL::KLoadStatus, URL::KFollowRedirect));

	LoadNoReloadOK(url, NULL);
}

#endif // WEBSERVER_SUPPORT

void CS_MessageHandler::LoadUnconditional(URL &url, UINT32 expected_response, WaitSingleURL *wait, OpFileDescriptor *expected_content, CustomLoad *cl)
{
	OP_ASSERT(wait || default_wait);

	OpMessage messages[]={MSG_URL_DATA_LOADED, MSG_URL_LOADING_FAILED, MSG_HEADER_LOADED};
	WaitSingleURL *obj=(wait)?wait:default_wait;

	if(obj)
	{
		obj->SetExpectedContent(expected_content, &url);
		obj->SetExpectedResponceCode(expected_response);
		obj->SetURLDownloaded(&url);
	}
	else
	{
		ST_failed("NULL WaitSingleURL");

		return;
	}

	if(expected_content)
	{
		if(expected_content->IsOpen())
			expected_content->SetFilePos(0);
		else
			expected_content->Open(OPFILE_READ);
	}

	SetCallBackList(obj, 0, messages, 3);
	
	url.GetAttribute(URL::KHTTPRangeStart, &obj->waiting_for);
	obj->SetExpectedTransferRange(min_exp_trnsf_bytes, max_exp_trnsf_bytes, expect_error);

	obj->StartTimeout();

	if(CheckUniteURL(url) && OpStatus::IsError(LoadURL(url, unconditional_policy, cl)))
	{
		obj->StopTimeout();
		OP_DELETE(wait);
		ST_failed("Load failed 1!");
	}
}

OP_STATUS CS_MessageHandler::LoadURL(URL &url, URL_LoadPolicy policy, CustomLoad *cl)
{
	if(cl)
	{
		if(cl->mh)
			OP_ASSERT(cl->mh==this);

		if(cl->method == LOAD_LOAD)
			return url.Load(this, empty_url);
		if(cl->method == LOAD_RELOAD)
			return url.Reload(this, empty_url, FALSE, FALSE);
	}
	return url.LoadDocument(this, empty_url, policy);
}

void CS_MessageHandler::Reload(URL &url, UINT32 expected_response, WaitSingleURL *wait, OpFileDescriptor *expected_content)
{
	output("Reload requested...\n");
	OP_ASSERT(wait || default_wait);

	OpMessage messages[]={MSG_URL_DATA_LOADED, MSG_URL_LOADING_FAILED, MSG_HEADER_LOADED};
	WaitSingleURL *obj=(wait)?wait:default_wait;

	if(obj)
	{
		obj->SetExpectedContent(expected_content, &url);
		obj->SetExpectedResponceCode(expected_response);
		obj->SetURLDownloaded(&url);
	}
	else
	{
		ST_failed("NULL WaitSingleURL");

		return;
	}

	if(expected_content)
	{
		if(expected_content->IsOpen())
			expected_content->SetFilePos(0);
		else
			expected_content->Open(OPFILE_READ);
	}

	SetCallBackList(obj, 0, messages, 3);
	
	url.GetAttribute(URL::KHTTPRangeStart, &obj->waiting_for);
	obj->SetExpectedTransferRange(min_exp_trnsf_bytes, max_exp_trnsf_bytes, expect_error);

	obj->StartTimeout();

	if(OpStatus::IsError(url.Reload(this, empty_url, FALSE, FALSE)))
	{
		obj->StopTimeout();
		OP_DELETE(wait);

		ST_failed("ReLoad failed 1!");
	}
	else
		output("Reload accepted...\n");
}

void CS_MessageHandler::LoadNoReload(URL &url, UINT32 expected_response, WaitSingleURL *wait, OpFileDescriptor *expected_content)
{
	OP_ASSERT(wait || default_wait);

	OpMessage messages[]={MSG_URL_DATA_LOADED, MSG_URL_LOADING_FAILED, MSG_HEADER_LOADED};
	WaitSingleURL *obj=(wait)?wait:default_wait;

	if(obj)
	{
		obj->SetExpectedContent(expected_content, &url);
		obj->SetExpectedResponceCode(expected_response);
		obj->SetURLDownloaded(&url);
	}
	else
	{
		ST_failed("NULL WaitSingleURL");

		return;
	}

	if(expected_content)
	{
		if(expected_content->IsOpen())
			expected_content->SetFilePos(0);
		else
			expected_content->Open(OPFILE_READ);
	}


	SetCallBackList(obj, 0, messages, 3);
	
	url.GetAttribute(URL::KHTTPRangeStart, &obj->waiting_for);
	obj->SetExpectedTransferRange(min_exp_trnsf_bytes, max_exp_trnsf_bytes, expect_error);

	CacheTester::DisableAlwaysVerify(url);

	obj->StartTimeout();

	if(CheckUniteURL(url) && COMM_REQUEST_FAILED==LoadURL(url, noreload_policy, NULL))
	{
		obj->StopTimeout();
		OP_DELETE(wait);

		ST_failed("Load failed 2!");
	}
}

BOOL CS_MessageHandler::CheckUniteURL(URL url)
{
#ifdef WEBSERVER_SUPPORT
	if(!g_webserver)
	{
		ServerName *server_name = (ServerName *) url.GetAttribute(URL::KServerName, NULL);

		if(server_name)
		{
			OpStringC8 name = server_name->GetName();
			unsigned short port = (unsigned short) url.GetAttribute(URL::KServerPort);
			unsigned short unite_port = (unsigned short) g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverPort);

			if(!name.CompareI("localhost") && port == unite_port)
			{
				ST_failed("Unite is disabled!");

				return FALSE;
			}
			else
				return TRUE;
		}
		else
		{
			ST_failed("URL error!");

			return FALSE;
		}
	}
#endif

	return TRUE;
}

void CS_MessageHandler::LoadNormal(URL &url, UINT32 expected_response, WaitSingleURL *wait, OpFileDescriptor *expected_content)
{
	OP_ASSERT(wait || default_wait);

	OpMessage messages[]={MSG_URL_DATA_LOADED, MSG_URL_LOADING_FAILED, MSG_HEADER_LOADED};
	WaitSingleURL *obj=(wait)?wait:default_wait;

	if(obj)
	{
		obj->SetExpectedContent(expected_content, &url);
		obj->SetExpectedResponceCode(expected_response);
		obj->SetURLDownloaded(&url);
	}
	else
	{
		ST_failed("NULL CS_MessageObject");

		return;
	}

	if(expected_content)
	{
		if(expected_content->IsOpen())
			expected_content->SetFilePos(0);
		else
			expected_content->Open(OPFILE_READ);
	}


	SetCallBackList(obj, 0, messages, 3);

	url.GetAttribute(URL::KHTTPRangeStart, &obj->waiting_for);
	obj->SetExpectedTransferRange(min_exp_trnsf_bytes, max_exp_trnsf_bytes, expect_error);

	CacheTester::DisableAlwaysVerify(url);

	obj->StartTimeout();

	if(CheckUniteURL(url) && COMM_REQUEST_FAILED==LoadURL(url, normal_policy, NULL))
	{
		obj->StopTimeout();
		OP_DELETE(wait);

		ST_failed("Load failed 3!");
	}
}

void CacheTester::DisableAlwaysVerify(URL url)
{
	url.SetAttribute(URL::KCachePolicy_AlwaysVerify, FALSE);

	URL url_redir=url.GetAttribute(URL::KMovedToURL);

	if(!url_redir.IsEmpty())
	{
		url_redir.SetAttribute(URL::KCachePolicy_AlwaysVerify, FALSE);

		// Support also another level of redirection, just in case...
		URL url_redir2=url_redir.GetAttribute(URL::KMovedToURL);

		if(!url_redir2.IsEmpty())
			url_redir2.SetAttribute(URL::KCachePolicy_AlwaysVerify, FALSE);
	}
}

void CacheTester::FreezeTests()
{
	// Check that the main context cannot be freeze
	Context_Manager *ctxfrz_main=urlManager->FindContextManager(0);

	if(!ctxfrz_main)
	{
		ST_failed("FindContextManager() cannot locate the main context!");

		return;
	}

	ctxfrz_main=urlManager->GetMainContext();

	if(!ctxfrz_main)
	{
		ST_failed("GetMainContext() cannot locate the main context!");

		return;
	}

	if(urlManager->IsContextManagerFrozen(ctxfrz_main))
	{
		ST_failed("Main context already frozen!");

		return;
	}

	urlManager->FreezeContextManager(ctxfrz_main);

	ctxfrz_main=urlManager->FindContextManager(0);

	if(!ctxfrz_main)
	{
		ST_failed("FindContextManager() cannot locate the main context after a freeze!");

		return;
	}

	ctxfrz_main=urlManager->GetMainContext();

	if(!ctxfrz_main)
	{
		ST_failed("GetMainContext() cannot locate the main context after a freeze!");

		return;
	}

	if(urlManager->IsContextManagerFrozen(ctxfrz_main))
	{
		ST_failed("Main context frozen!");

		return;
	}

	// Replica of the attributes required by the macros, using some alias
	Context_Manager *&ctx_main=urlManager->ctx_main;
	List<Context_Manager> &context_list=urlManager->context_list;
	List<Context_Manager> &frozen_contexts=urlManager->frozen_contexts;

	// Check the macro: without frozen. The new manager should be reacheable, but not the main one
	BOOL found_main=FALSE;

	CACHE_CTX_WHILE_BEGIN_REF
		if(manager==ctxfrz_main)
			found_main=TRUE;
	CACHE_CTX_WHILE_END_REF

	if(!found_main)
	{
		ST_failed("ctxfrz_main should be reacheable without checking the freeze list!");

		return;
	}

	// Check the macro: with frozen. Both should be reacheable
	found_main=FALSE;

	CACHE_CTX_WHILE_BEGIN_REF_FROZEN
		if(manager==ctxfrz_main)
			found_main=TRUE;
	CACHE_CTX_WHILE_END_REF_FROZEN

	if(!found_main)
	{
		ST_failed("ctxfrz_main should be reacheable also when checking the freeze list!");

		return;
	}
}

OP_STATUS CacheFileTest::VerifyFileSign(URL &url, const char *sign, OpFileLength &len, BOOL writeCache)
{
	OP_ASSERT(op_strlen(sign)>0 && op_strlen(sign)<256);

	if(writeCache)
		urlManager->WriteCacheIndexesL(TRUE, FALSE);

	URL_Rep *rep=url.GetRep();
	OpFileLength content_len=0;

	if(!rep || !rep->GetDataStorage() || !rep->GetDataStorage()->GetCacheStorage())
	{
		ST_failed("Cache storage jnot available!");
		
		return OpStatus::ERR;
	}

	url.GetAttribute(URL::KContentSize, &content_len);

	Cache_Storage *cs=rep->GetDataStorage()->GetCacheStorage();
	OpFileFolder folder;
	OpStringC name=cs->FileName(folder);
	OpFile file;
	
	output("Content size: %d - content loaded: %d\n", (UINT32)content_len, (UINT32)cs->ContentLoaded());
	
	// Verify that the file is an Opera Multimedia Cache File (KMultimedia attribute has to be set)
	if(OpStatus::IsError(file.Construct(name.CStr(), folder)))
	{
		ST_failed("File construction error!");
		
		return OpStatus::ERR;
	}
	
	if(sign)
	{
		char read_sign[256]; // ARRAY OK 2010-05-19 lucav
		OpFileLength bytes_read;
		
		if( OpStatus::IsError(file.Open(OPFILE_READ)) ||
			OpStatus::IsError(file.Read(read_sign, op_strlen(sign), &bytes_read)) ||
			bytes_read != (OpFileLength)op_strlen(sign))
		{
			ST_failed("Signature reading error!");
			
			return OpStatus::ERR;
		}
		
		if(op_strncmp(read_sign, sign, op_strlen(sign)))
		{
			ST_failed("Wrong signature!");
			
			return OpStatus::ERR;
		}
	}
	
	if(OpStatus::IsError(file.GetFileLength(len)))
	{
		ST_failed("File length error");
		
		return OpStatus::ERR;
	}
	file.Close();
	
	return OpStatus::OK;
}

OP_STATUS CacheFileTest::VerifyFileContent(URL &url, const uni_char *file_path, RedirectPolicy redir_policy, BOOL temporary_url)
{
	URL url_redir=url.GetAttribute(URL::KMovedToURL);

	switch(redir_policy)
	{
		case REDIR_NOT_ALLOWED:
			if(!url_redir.IsEmpty())
			{
				ST_failed("Redirection not allowed! ");

				return OpStatus::ERR_OUT_OF_RANGE;
			}
			break;
		
		case REDIR_ALLOWED_DONT_FOLLOW:
			/** Nothing special to do*/
			break;

		case REDIR_ALLOWED_FOLLOW:
			if(!url_redir.IsEmpty())
				return VerifyFileContent(url_redir, file_path, REDIR_ALLOWED_FOLLOW, temporary_url);
		break;
		
		case REDIR_REQUIRED_DONT_FOLLOW:
			if(url_redir.IsEmpty())
			{
				ST_failed("Redirection required even if not followed! ");

				return OpStatus::ERR_OUT_OF_RANGE;
			}
		break;

		case REDIR_REQUIRED_FOLLOW:
			if(url_redir.IsEmpty())
			{
				ST_failed("Redirection required! ");

				return OpStatus::ERR_OUT_OF_RANGE;
			}

			// Multiple chained redirects are allowed, but we cannot force them to keep redirect, or it would bee
			// a infinite redirect loop... which is usually not very useful
			return VerifyFileContent(url_redir, file_path, REDIR_ALLOWED_FOLLOW, temporary_url);
		break;
	}

	// Create and check the file
	OpFile file;  // Original file
	BOOL exists=FALSE;
	OpFileLength file_size;
	URL_Rep *rep=url.GetRep();
	OP_STATUS ops_open;

	// Open original file
	ops_open=OpenFile(file, file_path, file_size);

	if(OpStatus::IsError(ops_open))
	{
		ST_failed("Could not open the file!");

		return ops_open;
	}
	
	// Check and open cache file
	Cache_Storage *cs=(rep && rep->GetDataStorage() && rep->GetDataStorage()->GetCacheStorage())?rep->GetDataStorage()->GetCacheStorage():NULL;

	if(!cs)
	{
		ST_failed("VerifyFileContent(): No cache storage available!");
		
		return OpStatus::ERR;
	}
	
	if((cs->GetCacheType()!=URL_CACHE_DISK) && !temporary_url)
	{
		const char *descr="";

		if(cs->GetCacheType()==URL_CACHE_MEMORY)
			descr="(URL_CACHE_MEMORY)";
		else if(cs->GetCacheType()==URL_CACHE_TEMP)
			descr="(URL_CACHE_TEMP)";
		else if(cs->GetCacheType()==URL_CACHE_USERFILE)
			descr="(URL_CACHE_USERFILE)";
		else if(cs->GetCacheType()==URL_CACHE_STREAM)
			descr="(URL_CACHE_STREAM)";

		ST_failed("VerifyFileContent(): Cache type should be %s instead of %d %s!", (temporary_url)?"URL_CACHE_TEMP":"URL_CACHE_DISK", cs->GetCacheType(), descr);

		return OpStatus::ERR;
	}
	
	OP_STATUS op_err;
	File_Storage *fs=(File_Storage *)cs;
	OpFileLength file_size2=FILE_LENGTH_NONE;
	OpAutoPtr<OpFileDescriptor> fd_cache(fs->AccessReadOnly(op_err));
	
	if( OpStatus::IsError(op_err) || !fd_cache.get() ||
		OpStatus::IsError(fd_cache->Exists(exists)) ||
		!exists ||
		OpStatus::IsError(fd_cache->GetFileLength(file_size2)) ||
		file_size2<=0)
	{
		// Embedded files don't have a name, but they can produce a reader
		if(cs->IsEmbedded() || !exists)
		{
			SimpleStreamReader *reader=cs->CreateStreamReader();

			OP_ASSERT(reader);

			OP_STATUS ops=VerifyFileContent(reader, file_path);

			OP_DELETE(reader);

			return ops;
		}

		OpString8 filename;
		OpFileLength len=0;
		url.GetAttribute(URL::KContentSize, &len);

		filename.Set(url.GetAttribute(URL::KFileName));
		ST_failed("Error 1 on accessing the cache file of %s - %d (%s) - %d bytes to check the content - err: %d - exists: %d - file size: %d!", url.GetAttribute(URL::KName).CStr(), url.GetContextId(), filename.CStr(), (int)len, op_err, exists, file_size2);
		
		return OpStatus::ERR;
	}

#if CACHE_CONTAINERS_ENTRIES>0
	if(cs->GetContainerID())
	{
		CacheContainer cc;
		SimpleStreamReader *reader=cs->CreateStreamReader();

		OP_ASSERT(reader);

		OP_STATUS ops=VerifyFileContent(reader, file_path);

		OP_DELETE(reader);

		return ops;
	}
#endif // CACHE_CONTAINERS_ENTRIES>0
	
	if(file_size!=file_size2 && !url.GetAttribute(URL::KMultimedia))
	{
		ST_failed("Different sizes: %d vs %d!", (UINT32)file_size, (UINT32)file_size2);
		
		return OpStatus::ERR;
	}
	
	// Get the segments
	OpAutoVector<StorageSegment> sort_seg;
	
	op_err=cs->GetSortedCoverage(sort_seg);
	
	if(OpStatus::IsError(op_err))
	{
		ST_failed("Error during segment check!");
		
		return op_err;
	}
	
	// Check the segments
	unsigned char buf1[1024];	// ARRAY OK 2010-05-19 lucav
	unsigned char buf2[1024];	// ARRAY OK 2010-05-19 lucav
	for(UINT32 s=0; s<sort_seg.GetCount(); s++)
	{
		StorageSegment *seg=sort_seg.Get(s);
		OpFileLength bytes_read;
		
		OP_ASSERT(seg);
		
		if(seg)
		{
			RETURN_IF_ERROR(file.SetFilePos(seg->content_start));
			RETURN_IF_ERROR(fd_cache->SetFilePos(seg->content_start));
			
			while(seg->content_length>0)
			{
				OpFileLength size=(seg->content_length>1024)?1024:seg->content_length;
			
				RETURN_IF_ERROR(file.Read(buf1, size, &bytes_read));
				if(bytes_read!=size)
					return OpStatus::ERR_OUT_OF_RANGE;
					
				RETURN_IF_ERROR(fd_cache->Read(buf2, size, &bytes_read));
				if(bytes_read!=size)
					return OpStatus::ERR_OUT_OF_RANGE;
					
				for(UINT32 i=0; i<size; i++)
					if(buf1[i]!=buf2[i])
						return OpStatus::ERR_OUT_OF_RANGE;
						
				seg->content_length-=size;
			}
		}
	}
	
	file.Close();
	
	return OpStatus::OK;
}

OpFileLength CacheFileTest::GetCacheFileSize(URL_Rep *rep, BOOL follow_redirect)
{
	if(follow_redirect)
	{
		URL url_redir=rep->GetAttribute(URL::KMovedToURL);
		
		if(!url_redir.IsEmpty())
			return GetCacheFileSize(url_redir.GetRep(), TRUE);
	}

	// Create and check the file
	BOOL exists=FALSE;

	// Check and open cache file
	Cache_Storage *cs=(rep && rep->GetDataStorage() && rep->GetDataStorage()->GetCacheStorage())?rep->GetDataStorage()->GetCacheStorage():NULL;

	if(!cs)
	{
		ST_failed("GetCacheFileSize(): No cache storage available!");
		
		return FILE_LENGTH_NONE;
	}
	
	if(cs->GetCacheType()!=URL_CACHE_DISK)
	{
		ST_failed("GetCacheFileSize(): Cache type should be URL_CACHE_DISK!");
		
		return FILE_LENGTH_NONE;
	}
	
	OP_STATUS op_err;
	File_Storage *fs=(File_Storage *)cs;
	OpFileLength file_size=0;
	OpAutoPtr<OpFileDescriptor> fd_cache(fs->AccessReadOnly(op_err));
	
	if( OpStatus::IsError(op_err) || !fd_cache.get() ||
		OpStatus::IsError(fd_cache->Exists(exists)) ||
		!exists ||
		OpStatus::IsError(fd_cache->GetFileLength(file_size)) ||
		file_size<=0)
	{
		OpString8 file_name;
		OpFileFolder folder;
		OpString8 dir_name;
		OpString dir_name16;

		file_name.Set(fs->GetFileName());
		folder=fs->GetFolder();
		g_folder_manager->GetFolderPath(folder, dir_name16);

		dir_name.Set(dir_name16);

		ST_failed("Error 2 on accessing the cache file '%s' [%d] (emb: %d, container: %d, context: %d) of %s to check the size - err: %d - exists: %d - size: %d!\n Base cache path: %s\n", file_name.CStr(), folder, cs->IsEmbedded(), cs->GetContainerID(), rep->GetContextId(), rep->GetAttribute(URL::KName).CStr(), op_err, exists, file_size, dir_name.CStr());
		
		return FILE_LENGTH_NONE;
	}

	return file_size;
}

#endif // DISK_CACHE_SUPPORT

#ifdef DIRECTORY_SEARCH_SUPPORT
OP_STATUS CacheFileTest::OpenFile(OpFile &file, const uni_char *file_path, OpFileLength &file_size)
{
	BOOL exists=FALSE;
	
	if( OpStatus::IsError(file.Construct(file_path)) ||
		OpStatus::IsError(file.Exists(exists)) ||
		!exists ||
		OpStatus::IsError(file.GetFileLength(file_size)) ||
		file_size<=0)
	{
		ST_failed("Error on accessing the cache file to open it - exists: %d - size: %d!", exists, file_size);
		
		return OpStatus::ERR;
	}
	
	if( OpStatus::IsError(file.Open(OPFILE_READ)))
	{
		ST_failed("Error opening the file!");
		
		return OpStatus::ERR;
	}
	
	return OpStatus::OK;
}

OP_STATUS CacheFileTest::DeleteCacheDir(OpFileFolder folder)
{
	OpFile f;
	BOOL exists;

	OpString path;
	
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(folder, path));

	if (!path.IsEmpty())
	{
		RETURN_IF_ERROR(f.Construct(path.CStr()));
		RETURN_IF_ERROR(f.Exists(exists));

		if(exists)
			RETURN_IF_ERROR(f.Delete(TRUE));
	}

	return OpStatus::OK;
}

OP_STATUS CacheFileTest::CreateEmptyCacheDir(const uni_char *name, OpFileFolder &folder)
{
	RETURN_IF_ERROR(g_folder_manager->AddFolder(OPFILE_CACHE_FOLDER, name, &folder));

	RETURN_IF_ERROR(DeleteCacheDir(folder));

	OpFile f;
	OpFile f2;
	BOOL exists;

	RETURN_IF_ERROR(f.Construct(UNI_L("test.fil"), folder));
	RETURN_IF_ERROR(f2.Construct(UNI_L("test.fil"), folder));

	RETURN_IF_ERROR(f.Open(OPFILE_WRITE));
	RETURN_IF_ERROR(f.Close());

	RETURN_IF_ERROR(f2.Exists(exists));

	if(!exists)
		return OpStatus::ERR;

	RETURN_IF_ERROR(f.Delete(FALSE));

	RETURN_IF_ERROR(f2.Exists(exists));

	if(exists)
		return OpStatus::ERR;

	OpString8 path;
	OpString path16;

	g_folder_manager->GetFolderPath(folder, path16);

	path.Set(path16);

	output("Cache directory %s created successfully\n", path.CStr());

	return OpStatus::OK;
}

#ifdef CACHE_FAST_INDEX
OP_STATUS CacheFileTest::VerifyFileContent(const uni_char *file_path, const OpString8 &expected)
{
	SimpleBufferReader sbr(expected.CStr(), expected.Length());

	RETURN_IF_ERROR(sbr.Construct());

	return VerifyFileContent(&sbr, file_path);
}

OP_STATUS CacheFileTest::VerifyFileContent(SimpleStreamReader *reader, const uni_char *file_path)
{
	// Create and check the file
	OpFile file;  // Original file
	OpFileLength file_size;
	
	if(!reader)
	{
		ST_failed("NULL Reader!");
		
		return OpStatus::ERR_NULL_POINTER;
	}

	// Open original file
	RETURN_IF_ERROR(OpenFile(file, file_path, file_size));
	
	// Compare the contents
	unsigned char buf1[1024];	// ARRAY OK 2010-05-19 lucav
	unsigned char buf2[1024];	// ARRAY OK 2010-05-19 lucav
	
	while(file_size)
	{
		OpFileLength size=(file_size>1024)?1024:file_size;
		OpFileLength bytes_read=0;
	
		RETURN_IF_ERROR(file.Read(buf1, size, &bytes_read));
		if(bytes_read!=size)
			return OpStatus::ERR_OUT_OF_RANGE;
			
		RETURN_IF_ERROR(reader->ReadBuf(buf2, (UINT32)size));
			
		for(UINT32 i=0; i<size; i++)
			if(buf1[i]!=buf2[i])
			{
				ST_failed("Content error after %d bytes: %d != %d", i, buf1[i], buf2[i]);

				return OpStatus::ERR_OUT_OF_RANGE;
			}
				
		file_size-=size;
	}
	
	file.Close();
	
	return OpStatus::OK;
}

#endif // CACHE_FAST_INDEX



#define TMP_BIN_BUF_MASK 0x1FF
#define TMP_BIN_BUF_SIZE (TMP_BIN_BUF_MASK+1)

OP_STATUS CacheFileTest::CreateTempFile(UINT32 size, BOOL random_content, OpString &file_path, OpFile &file, UINT32 seq)
{
	OpString path;
	
	// Look for a temporary name not used
	RETURN_IF_ERROR(path.AppendFormat(UNI_L("cache_tmp_%d_%d_%d"), g_op_time_info->GetRuntimeMS(), (UINT32)op_rand(), seq));
	RETURN_IF_ERROR(file.Construct(path.CStr(), OPFILE_TEMP_FOLDER));
	
	BOOL b;
	
	RETURN_IF_ERROR(file.Exists(b));
	
	if(b)
		return CreateTempFile(size, random_content, path, file, seq+1);
		
	// Create the file
	RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
	
	UINT8 buf[TMP_BIN_BUF_SIZE];
	
	// Write the content
	for(UINT32 i=0; i<size; i++)
	{
		UINT8 val=(random_content) ? ((UINT8)op_rand()) : (UINT8)i;
		
		buf[i & TMP_BIN_BUF_MASK]=val;
		
		if( (i+1) % TMP_BIN_BUF_SIZE == 0)  // Flush
			RETURN_IF_ERROR(file.Write(buf, TMP_BIN_BUF_SIZE));
	}
	
	if(size % TMP_BIN_BUF_SIZE)
		RETURN_IF_ERROR(file.Write(buf, size % TMP_BIN_BUF_SIZE));
		
	file.Close();
	
	OpFileLength true_size;
	
	file.GetFileLength(true_size);
	
	OP_ASSERT(true_size==size);
	
	return file_path.Set(file.GetFullPath());
}
#endif // DIRECTORY_SEARCH_SUPPORT

void CacheTester::TestURLs(Context_Manager *ctx, URL_DataStorage **list, URL url1, URL url2, URL url3, URL url4, URL url5)
{
	OP_ASSERT(ctx && list);

	URL_DataStorage* orig=*list;
	URL_DataStorage* first=orig?orig:url1.GetRep()->GetDataStorage();
	URL_DataStorage* second=orig?orig:url5.GetRep()->GetDataStorage();

	// Test LRU insertion
	ctx->SetLRU_Item(url1.GetRep()->GetDataStorage());
	if(*list!=first)
		ST_failed("LRU 1 not properly set!");

	ctx->SetLRU_Item(url2.GetRep()->GetDataStorage());
	if(*list!=first)
		ST_failed("LRU 2 not properly set!");

	ctx->SetLRU_Item(url3.GetRep()->GetDataStorage());
	if(*list!=first)
		ST_failed("LRU 3 not properly set!");

	ctx->SetLRU_Item(url4.GetRep()->GetDataStorage());
	if(*list!=first)
		ST_failed("LRU 4 not properly set!");

	// Test LRU removal
	ctx->RemoveLRU_Item(url4.GetRep()->GetDataStorage());
	if(*list!=first)
		ST_failed("LRU 1 not properly kept (1)!");

	ctx->RemoveLRU_Item(url3.GetRep()->GetDataStorage());
	if(*list!=first)
		ST_failed("LRU 1 not properly kept (2)!");

	ctx->RemoveLRU_Item(url1.GetRep()->GetDataStorage());
	if(*list!=url2.GetRep()->GetDataStorage() && *list!=orig)
		ST_failed("second or original LRU not properly re-set!");

	ctx->RemoveLRU_Item(url2.GetRep()->GetDataStorage());
	if(*list!=orig)
		ST_failed("Original LRU not properly re-set!");

	// Insert again
	ctx->SetLRU_Item(url5.GetRep()->GetDataStorage());
	if(*list!=second)
		ST_failed("LRU 5 not properly set!");

	// Remove again
	ctx->RemoveLRU_Item(url5.GetRep()->GetDataStorage());
	if(*list!=orig)
		ST_failed("Original LRU not properly re-set (2)!");
}

UINT32 CacheTester::CheckURLsInStore(URL_CONTEXT_ID ctx_id)
{
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
	{
		ST_failed("Context %d not found!", ctx_id);
		return (UINT32)-1;
	}

	URL_Rep* url_rep;
	UINT32 num_items = 0;

	url_rep = ctx->url_store->GetFirstURL_Rep();
	while ( url_rep != NULL )
	{
		num_items++;
		url_rep = ctx->url_store->GetNextURL_Rep();
	}

	return num_items;
}

void CacheTester::TestURLsRAM(URL_CONTEXT_ID ctx_id)
{
	OpAutoVector<URL> testURLs;
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
		ST_failed("Context %d not found!", ctx_id);

	CacheHelpers::CacheBogusURLs(ctx_id, 1, 1, 5, 0, &testURLs, 0, TRUE, FALSE);

	if(testURLs.GetCount()!=5)
		ST_failed("%d URLs created instead of 5", testURLs.GetCount());

	TestURLs(urlManager->GetMainContext(), &urlManager->GetMainContext()->LRU_ram, *testURLs.Get(0), *testURLs.Get(1), *testURLs.Get(2), *testURLs.Get(3), *testURLs.Get(4));
}

#ifdef DISK_CACHE_SUPPORT
void CacheTester::TestURLsTemp(URL_CONTEXT_ID ctx_id)
{
	OpAutoVector<URL> testURLs;
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
		ST_failed("Context %d not found!", ctx_id);

	CacheHelpers::CacheBogusURLs(ctx_id, 1, 1, 5, 0, &testURLs, 0, TRUE, FALSE);

	if(testURLs.GetCount()!=5)
		ST_failed("%d URLs created instead of 5", testURLs.GetCount());

	testURLs.Get(0)->DumpSourceToDisk(TRUE);
	testURLs.Get(1)->DumpSourceToDisk(TRUE);
	testURLs.Get(2)->DumpSourceToDisk(TRUE);
	testURLs.Get(3)->DumpSourceToDisk(TRUE);
	testURLs.Get(4)->DumpSourceToDisk(TRUE);

	testURLs.Get(0)->SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
	testURLs.Get(1)->SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
	testURLs.Get(2)->SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
	testURLs.Get(3)->SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
	testURLs.Get(4)->SetAttribute(URL::KCacheType, URL_CACHE_TEMP);

	TestURLs(urlManager->GetMainContext(), &urlManager->GetMainContext()->LRU_temp, *testURLs.Get(0), *testURLs.Get(1), *testURLs.Get(2), *testURLs.Get(3), *testURLs.Get(4));
}

void CacheTester::TestURLsDisk(URL_CONTEXT_ID ctx_id)
{
	OpAutoVector<URL> testURLs;
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
		ST_failed("Context %d not found!", ctx_id);

	CacheHelpers::CacheBogusURLs(ctx_id, 1, 1, 5, 0, &testURLs, 0, TRUE, FALSE);

	if(testURLs.GetCount()!=5)
		ST_failed("%d URLs created instead of 5", testURLs.GetCount());

	testURLs.Get(0)->DumpSourceToDisk(TRUE);
	testURLs.Get(1)->DumpSourceToDisk(TRUE);
	testURLs.Get(2)->DumpSourceToDisk(TRUE);
	testURLs.Get(3)->DumpSourceToDisk(TRUE);
	testURLs.Get(4)->DumpSourceToDisk(TRUE);

	testURLs.Get(0)->SetAttribute(URL::KCacheType, URL_CACHE_DISK);
	testURLs.Get(1)->SetAttribute(URL::KCacheType, URL_CACHE_DISK);
	testURLs.Get(2)->SetAttribute(URL::KCacheType, URL_CACHE_DISK);
	testURLs.Get(3)->SetAttribute(URL::KCacheType, URL_CACHE_DISK);
	testURLs.Get(4)->SetAttribute(URL::KCacheType, URL_CACHE_DISK);

	TestURLs(urlManager->GetMainContext(), &urlManager->GetMainContext()->LRU_disk, *testURLs.Get(0), *testURLs.Get(1), *testURLs.Get(2), *testURLs.Get(3), *testURLs.Get(4));
}
#endif // DISK_CACHE_SUPPORT

void CacheTester::MoveCreationInThePast(URL_DataStorage *ds)
{
	if(ds) ds->local_time_loaded=1;
}

void CacheTester::MoveLocalTimeVisitedInThePast(URL_Rep *rep)
{
	if(rep)
		rep->last_visited=1;
}


void CacheTester::BlockPeriodicFreeUnusedResources()
{
#ifndef RAMCACHE_ONLY
	g_periodic_task_manager->UnregisterTask(&(urlManager->freeUnusedResources));
#endif
}

void CacheTester::RestorePeriodicFreeUnusedResources()
{
#ifndef RAMCACHE_ONLY
	g_periodic_task_manager->RegisterTask(&(urlManager->freeUnusedResources), 10000);
#endif
}

void CacheTester::TestURLsSkip(URL_CONTEXT_ID ctx_id, UINT32 num)
{
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
	{
		ST_failed("Context %d not found!", ctx_id);

		return;
	}

	URL_Rep **ar=OP_NEWA(URL_Rep *, num);
	LinkObjectBookmark *bookmarks=OP_NEWA(LinkObjectBookmark, num);
	int num_null=0;

	if(!ar || !bookmarks)
	{
		OP_DELETEA(ar);
		OP_DELETEA(bookmarks);

		ST_failed("Out of memory!");

		return;
	}

	// Normal Get with bookmark
	URL_Rep* cur_rep = ctx->url_store->GetFirstURL_Rep();
	LinkObjectBookmark bm;

	for(UINT32 i=0; i<num; i++)
	{
		bookmarks[i] = bm; // Copy constructor

		ar[i] = cur_rep;

		if(!cur_rep)
			num_null++;
		else
			cur_rep = ctx->url_store->GetNextURL_Rep(bm);
	}

	// Check against skip
	for(UINT32 i = 1; i < num; i++)
	{
		//ctx->url_store->GetFirstURL_Rep();
		//cur_rep = ctx->url_store->SkipNextURL_Reps(i);
		cur_rep = ctx->url_store->JumpToURL_RepBookmark(bookmarks[i]);
		
		if(ar[i] != cur_rep)
		{
			ST_failed("URL %d is different!", i);

			OP_DELETEA(ar);
			OP_DELETEA(bookmarks);

			return;
		}

		if(i < num-1 && ar[i+1] != ctx->url_store->GetNextURL_Rep())
		{
			ST_failed("Next URL of %d is different!", i);

			OP_DELETEA(ar);
			OP_DELETEA(bookmarks);

			return;
		}

	}

	OP_ASSERT(1000 * num >= ctx->url_store->LinkObjectCount());

	// Count URLs
	UINT32 num_urls=0;
	double start = g_op_time_info->GetRuntimeMS();
	URL_Rep * last_rep=NULL;
	LinkObjectBookmark bm_last;

	for(UINT32 j = 0; j < 20; j++)
	{
		num_urls = 0;
		cur_rep = ctx->url_store->GetFirstURL_Rep();

		bm.Reset();

		while (cur_rep)
		{
			num_urls++;
			bm_last = bm; // Copy constructor
			last_rep = cur_rep;
			cur_rep = ctx->url_store->GetNextURL_Rep(bm);
		}
	}

	OP_ASSERT(num_urls == ctx->url_store->LinkObjectCount());

	output("%d URLs counted 20 times in %d ms\n", num_urls, (UINT32) (g_op_time_info->GetRuntimeMS()-start));

	start = g_op_time_info->GetRuntimeMS();

	// Skip URLs
	for(UINT32 k = 0; k < 20; k++)
	{
		//cur_rep = ctx->url_store->GetFirstURL_Rep();

		//URL_Rep *last=ctx->url_store->SkipNextURL_Reps(num_urls-2);
		URL_Rep *last=ctx->url_store->JumpToURL_RepBookmark(bm_last);

		if(!last || last != last_rep)
		{
			ST_failed("Error during skip!");

			OP_DELETEA(ar);
			OP_DELETEA(bookmarks);

			return;
		}
	}

	OP_DELETEA(ar);
	OP_DELETEA(bookmarks);

	output("%d URLs skipped 20 times in %d ms\n", num_urls, (UINT32) (g_op_time_info->GetRuntimeMS()-start));
}

#ifdef DISK_CACHE_SUPPORT
/// Return the size of the cache file
OpFileLength CacheTester::GetCacheFileLen(URL url)
{
	OP_STATUS ops;

	if(!url.GetRep() || !url.GetRep()->GetDataStorage() || !url.GetRep()->GetDataStorage()->GetCacheStorage())
		return 0;

	OpFileDescriptor *fd= ((File_Storage *)(url.GetRep()->GetDataStorage()->GetCacheStorage()))->AccessReadOnly(ops);

	if(!fd)
		return 0;

	OpFileLength len;

	fd->GetFileLength(len);

	OP_DELETE(fd);

	return len;
}

BOOL CacheTester::IsCacheToSync(URL_CONTEXT_ID ctx_id)
{
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
	{
		ST_failed("Context %d not found!", ctx_id);

		return FALSE;
	}

	return IsCacheToSync(ctx->GetCacheLocationForFilesCorrected());
}

BOOL CacheTester::IsCacheToSync(OpFileFolder folder)
{
	// Check if the sync phase is required. No flag file, no sync required
	OpFile flag_file;
	BOOL flag_exists=TRUE;

	if (OpStatus::IsSuccess(flag_file.Construct(CACHE_ACTIVITY_FILE, folder)) &&
		OpStatus::IsSuccess(flag_file.Exists(flag_exists)))
	{
		return flag_exists;
	}
	else
	{
		ST_failed("Error during sync file check of folder %d!", folder);

		return FALSE;
	}
}

OP_STATUS CacheTester::ForceCacheSync(OpFileFolder folder)
{
	// Check if the sync phase is required. No flag file, no sync required
	OpFile flag_file;

	RETURN_IF_ERROR(flag_file.Construct(CACHE_ACTIVITY_FILE, folder));

	return flag_file.Delete(FALSE);
}

OP_STATUS CacheTester::ForceCacheSync(URL_CONTEXT_ID ctx_id)
{
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	return ForceCacheSync(ctx->GetCacheLocationForFilesCorrected());
}
#endif // DISK_CACHE_SUPPORT

OP_STATUS CacheTester::TouchFiles(OpFileFolder folder, int num_files, BOOL simulate_crash, const uni_char *sub_folder)
{
	uni_char *full_path=NULL;
	OpString full_path_str;

	// Switch to absolute path, as Linux seems to behave differently that Windows, not accepting wilcards with paths...
	if(sub_folder)
	{
		OpString tmp_storage;

		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(folder, tmp_storage));

		full_path_str.AppendFormat(UNI_L("%s%s%c"), tmp_storage.CStr(), sub_folder, PATHSEPCHAR);
		full_path=full_path_str.CStr();
		folder=OPFILE_ABSOLUTE_FOLDER;
	}

	for(int i=0; i<num_files; i++)
	{
		OpFile file;
		OpString str;

		str.AppendFormat(UNI_L("%sopr_tmp_touch_%d_%d"), full_path?full_path:UNI_L(""), i, ((UINT32)op_rand()) & 0xFFFFFF);

		RETURN_IF_ERROR(file.Construct(str.CStr(), folder));
		RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
		RETURN_IF_ERROR(file.Close());
	}

	if(simulate_crash)
	{
		OpFile file;

		RETURN_IF_ERROR(file.Construct(CACHE_ACTIVITY_FILE, folder));
		RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
		RETURN_IF_ERROR(file.Close());
	}

	return OpStatus::OK;
}

#if defined DISK_CACHE_SUPPORT && CACHE_CONTAINERS_ENTRIES>0
int CacheTester::CheckMarked(URL_CONTEXT_ID ctx_id)
{
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
	{
		ST_failed("Context %d not found!", ctx_id);
		return -1;
	}

	return (int)((Context_Manager_Disk *)ctx)->cnt_marked.GetCount();
}

OP_STATUS CacheTester::SaveFiles(URL_CONTEXT_ID ctx_id, BOOL flush_containers, BOOL disable_always_verify)
{
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
	{
		ST_failed("Context %d not found!", ctx_id);

		return OpStatus::ERR_NO_SUCH_RESOURCE;
	}

	if(disable_always_verify)
		DisableAlwaysVerifyForAllURLs(ctx_id);

	URL_Rep* url_rep;

	// Save non unique URLs
	url_rep = ctx->url_store->GetFirstURL_Rep();
	while ( url_rep != NULL )
	{
		url_rep->DumpSourceToDisk();

		url_rep = ctx->url_store->GetNextURL_Rep();
	}

	// Save also Unique URLs (many of the URLs have already been saved)
	URL_DataStorage *url_ds=(URL_DataStorage *)(ctx->LRU_list.First());

	while(url_ds != NULL)
	{
		url_ds->url->DumpSourceToDisk();
		url_ds = (URL_DataStorage *)url_ds->Suc();
	}

	// Save containers
	if(flush_containers)
		FlushContainers(ctx_id);

	return OpStatus::OK;
}

void CacheTester::DisableAlwaysVerifyForAllURLs(URL_CONTEXT_ID ctx_id)
{
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
	{
		ST_failed("Context %d not found!", ctx_id);

		return;
	}

	URL_Rep* url_rep;

	// Manage non unique URLs
	url_rep = ctx->url_store->GetFirstURL_Rep();
	while ( url_rep != NULL )
	{
		url_rep->SetAttribute(URL::KCachePolicy_AlwaysVerify, FALSE);

		url_rep = ctx->url_store->GetNextURL_Rep();
	}

	// Manage  also Unique URLs
	URL_DataStorage *url_ds=(URL_DataStorage *)(ctx->LRU_list.First());

	while(url_ds != NULL)
	{
		url_ds->url->SetAttribute(URL::KCachePolicy_AlwaysVerify, FALSE);
		url_ds = (URL_DataStorage *)url_ds->Suc();
	}
}


void CacheTester::FlushContainers(URL_CONTEXT_ID ctx_id, BOOL reset)
{
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
	{
		ST_failed("Context %d not found!", ctx_id);
		return;
	}

	((Context_Manager_Disk *)ctx)->FlushContainers(reset);
}

#endif // defined DISK_CACHE_SUPPORT && CACHE_CONTAINERS_ENTRIES>0

void CacheTester::BrutalDelete(URL *url, BOOL delete_url, Window *window)
{
	OP_ASSERT(url && url->GetRep());

	if(url && url->GetRep())
	{
		if(url->GetRep()->GetUsedCount())
			url->GetRep()->DecUsed(url->GetRep()->GetUsedCount());

		if((URLCacheType)url->GetAttribute(URL::KCacheType)!=URL_CACHE_USERFILE)
			url->SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
		urlManager->MakeUnique(url->GetRep());

		Context_Manager *mgr=urlManager->FindContextManager(url->GetContextId());

		//if(url->GetRep()->GetRefCount() == 1)
		{
			url->GetRep()->Unload();
			OP_DELETE(url->GetRep());
			url->rep=NULL;
		}

		OP_ASSERT(mgr);

		// Invariants check disabled as the URL has been deleted without checks, so we could just get
		// meaningless asserts
		if(mgr)
			mgr->SetInvariantsCheck(FALSE);
	}

	if(delete_url)
		OP_DELETE(url);
}

void CacheTester::SetDeepDebug(URL_CONTEXT_ID ctx_id, BOOL deep)
{
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
	{
		ST_failed("Context %d not found!", ctx_id);
		return;
	}

	ctx->debug_deep=deep;
}

void CacheTester::SetCacheSize(URL_CONTEXT_ID ctx_id, OpFileLength disk_size, OpFileLength ram_size)
{
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
	{
		ST_failed("Context %d not found!", ctx_id);
		return;
	}

	ctx->size_disk.SetSize(disk_size);
	ctx->size_ram.SetSize(ram_size);
}

URL CacheTester::GetPageSizeURL(UINT32 size, char *id, URL_CONTEXT_ID context_id, BOOL unique)
{
	OpString url_path;
	OpString8 url_path8;
		
	url_path.AppendFormat(UNI_L("http://testsuites.oslo.opera.com/core/networking/http/cache/data/cache.php?size=%d&type=text/javascript&id=%s"), size, (id==NULL)?"empty":id);
	url_path8.Set(url_path.CStr());
	
	return g_url_api->GetURL(url_path.CStr(), NULL, unique, context_id);
}

void CacheTester::ScheduleForDelete(URL url)
{
	CacheTester::MoveCreationInThePast(url.GetRep()->GetDataStorage());
	CacheTester::MoveLocalTimeVisitedInThePast(url.GetRep());
	url.GetRep()->SetAttribute(URL::KCacheType, URL_CACHE_TEMP);
}

void CacheTester::DeleteEverythingPossible(URL_CONTEXT_ID ctx_id, BOOL write_index)
{
	Context_Manager *ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
	{
		ST_failed("Context %d not found!", ctx_id);

		return;
	}

	int cache_size_pref=ctx->cache_size_pref;
	OpFileLength cache_size=ctx->size_disk.GetSize();

	ctx->cache_size_pref=-1;
	ctx->SetCacheSize(0);
	ctx->FreeUnusedResources(TRUE);
#ifdef DISK_CACHE_SUPPORT
	if(write_index)
		ctx->WriteCacheIndexesL(TRUE, FALSE);
#endif
	ctx->cache_size_pref=cache_size_pref;
	ctx->SetCacheSize(cache_size);
}

#ifdef DISK_CACHE_SUPPORT
OP_STATUS CacheTester::WriteIndex(URL_CONTEXT_ID ctx_id)
{
    Context_Manager * OP_MEMORY_VAR ctx=urlManager->Cache_Manager::FindContextManager(ctx_id);

	if(!ctx)
	{
		ST_failed("Context %d not found!", ctx_id);

		return OpStatus::ERR_NO_SUCH_RESOURCE;
	}

	RETURN_IF_LEAVE(ctx->WriteCacheIndexesL(FALSE, FALSE));

	return OpStatus::OK;
}
#endif // DISK_CACHE_SUPPORT
#ifdef DISK_CACHE_SUPPORT
OP_STATUS MultiContext::CreateNewContext(URL_CONTEXT_ID &ctx, const uni_char* folder_name, BOOL delete_directory, OpFileFolder *folder_dest, int *index)
{
	OpFileFolder folder;

	// Create the directory
	RETURN_IF_ERROR(g_folder_manager->AddFolder(OPFILE_CACHE_FOLDER, folder_name, &folder));
	
	if(folder_dest)
		*folder_dest=folder;

	return CreateNewContext(ctx, folder, delete_directory, index);
}

OP_STATUS MultiContext::CreateNewContext(URL_CONTEXT_ID &ctx, OpFileFolder folder, BOOL delete_directory, int *index)
{
	OP_ASSERT(ctxs.GetCount() == folders.GetCount());

	if(index)
		*index=-1;

	if(ctxs.GetCount() != folders.GetCount())
		return OpStatus::ERR_NOT_SUPPORTED;

	if(delete_directory)
		RETURN_IF_ERROR(CacheFileTest::DeleteCacheDir(folder));

	// Create directory and context
	ctx=urlManager->GetNewContextID();

	RETURN_IF_LEAVE(urlManager->AddContextL(ctx, folder, folder, folder, folder, FALSE));

	// Create the "waiter" and the Message handler, in case they are used later
	// NOTE: Acceptable small problems: in OOM, we can leak memory...
	OpMessage messages[]={MSG_URL_DATA_LOADED, MSG_URL_LOADING_FAILED, MSG_URLMAN_DELETE_SOMEFILES, MSG_HEADER_LOADED};
	MessageHandler *mh=OP_NEW(MessageHandler, (NULL));

	RETURN_OOM_IF_NULL(mh);

	WaitURLs *wait=OP_NEW(WaitURLs, (folder, folder, mh));

	RETURN_OOM_IF_NULL(wait);

	mh->SetCallBackList(wait, 0, messages, 4);

	// Fill the vectors
	// NOTE: Acceptable small problem: in OOM, we can end-up with vectors of different size...
	RETURN_IF_ERROR(folders.Add((INT32)folder));
	RETURN_IF_ERROR(ctxs.Add((INT32)ctx));
	RETURN_IF_ERROR(waits.Add(wait));
	RETURN_IF_ERROR(handlers.Add(mh));

	if(index)
		*index=ctxs.GetCount()-1;

	return OpStatus::OK;
}

OP_STATUS MultiContext::AddContext(Context_Manager *man, int *index, Window *window)
{
	OP_ASSERT(ctxs.GetCount() == folders.GetCount());
	OP_ASSERT(man);

	if(index)
		*index=-1;

	if(ctxs.GetCount() != folders.GetCount())
		return OpStatus::ERR_NOT_SUPPORTED;

	if(!man)
		return OpStatus::ERR_NULL_POINTER;

	// Create the "waiter" and the Message handler, in case they are used later
	// NOTE: Acceptable small problems: in OOM, we can leak memory...
	OpMessage messages[]={MSG_URL_DATA_LOADED, MSG_URL_LOADING_FAILED, MSG_URLMAN_DELETE_SOMEFILES, MSG_HEADER_LOADED};
	MessageHandler *mh=OP_NEW(MessageHandler, (window));

	RETURN_OOM_IF_NULL(mh);

	WaitURLs *wait=OP_NEW(WaitURLs, (man->cache_loc, man->cache_loc, mh));

	RETURN_OOM_IF_NULL(wait);

	mh->SetCallBackList(wait, 0, messages, 4);

	// Fill the vectors
	// NOTE: Acceptable small problem: in OOM, we can end-up with vectors of different size...
	RETURN_IF_ERROR(folders.Add((INT32)man->cache_loc));
	RETURN_IF_ERROR(ctxs.Add((INT32)man->context_id));
	RETURN_IF_ERROR(waits.Add(wait));
	RETURN_IF_ERROR(handlers.Add(mh));
	OpString storagePath;

	g_folder_manager->GetFolderPath(man->cache_loc, storagePath);

	OP_NEW_DBG("MultiContext::AddContext()", "cache.ctx");
	OP_DBG((UNI_L("*** Context %d mapped to folder %d: %s"), (int)man->context_id, (int)man->cache_loc, storagePath.CStr()));

	if(index)
		*index=ctxs.GetCount()-1;

	return OpStatus::OK;
}


void MultiContext::RemoveFoldersAndContexts()
{
	OP_ASSERT(ctxs.GetCount() == folders.GetCount());
	OP_ASSERT(handlers.GetCount() == waits.GetCount());

	while(ctxs.GetCount())
	{
		URL_CONTEXT_ID ctx=(URL_CONTEXT_ID)ctxs.Get(ctxs.GetCount()-1);

		if(ctx!=g_windowManager->GetTurboModeContextId())
			urlManager->RemoveContext(ctx, TRUE);

		ctxs.Remove(ctxs.GetCount()-1);
	}

	while(folders.GetCount())
	{
		OpFileFolder folder=(OpFileFolder)folders.Get(folders.GetCount()-1);

		OpStatus::Ignore(CacheFileTest::DeleteCacheDir(folder));
		folders.Remove(folders.GetCount()-1);
	}

	while(handlers.GetCount())
	{
		MessageHandler *mh=handlers.Get(handlers.GetCount()-1);
		WaitURLs *wait=waits.Get(handlers.GetCount()-1);

		handlers.Remove(handlers.GetCount()-1);
		waits.Remove(waits.GetCount()-1);

		mh->UnsetCallBacks(wait);
		OP_DELETE(mh);
		OP_DELETE(wait);
	}

	mh_main.UnsetCallBacks(&wait_main);
}

MultiContext::MultiContext(BOOL cache_get_queries): mh_main(NULL), wait_main(OPFILE_CACHE_FOLDER, OPFILE_CACHE_FOLDER, &mh_main)
{
	pref_doc_mod = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckDocModification);
	pref_doc_expiry = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DocExpiry);
	pref_other_mod = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckOtherModification);
	pref_other_expiry = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OtherExpiry);
	pref_cache_https = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheHTTPS);
#ifdef WEBSERVER_SUPPORT
	pref_opera_account = g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount);
	pref_webserver_enabled = g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverEnable);
	pref_listen_all = g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverListenToAllNetworks);
#endif // WEBSERVER_SUPPORT
	pref_web_turbo  = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseWebTurbo);
#ifdef DELAYED_SCRIPT_EXECUTION
	prefs_delayed_scripts = g_pcjs->GetIntegerPref(PrefsCollectionJS::DelayedScriptExecution);
#endif // DELAYED_SCRIPT_EXECUTION

	pref_never_expire = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AlwaysCheckNeverExpireGetQueries);
	pref_empty_on_exit = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EmptyCacheOnExit);

	pref_ignore_popups = g_pcjs->GetIntegerPref(PrefsCollectionJS::IgnoreUnrequestedPopups);
	pref_cache_redir = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AlwaysCheckRedirectChanged);
	pref_cache_redir_images = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AlwaysCheckRedirectChanged);
	pref_enable_disk_cache = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CacheToDisk);

	if(cache_get_queries)
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::AlwaysCheckNeverExpireGetQueries, 0);

	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::AlwaysCheckRedirectChanged, 0);
	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::AlwaysCheckRedirectChangedImages, 0);

	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::DocExpiry, 500);
	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CacheToDisk, 1);

	OpMessage messages[]={MSG_URL_DATA_LOADED, MSG_URL_LOADING_FAILED, MSG_URLMAN_DELETE_SOMEFILES, MSG_HEADER_LOADED};
	mh_main.SetCallBackList(&wait_main, 0, messages, 4);
}

MultiContext::~MultiContext()
{
	RemoveFoldersAndContexts();

	OP_ASSERT(!ctxs.GetCount());
	OP_ASSERT(!folders.GetCount());

	// Restore preferences
	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::AlwaysCheckNeverExpireGetQueries, pref_never_expire);
	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::EmptyCacheOnExit, pref_empty_on_exit);
	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CheckDocModification, pref_doc_mod);
	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::DocExpiry, pref_doc_expiry);
	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CacheHTTPS, pref_cache_https);
#ifdef WEBSERVER_SUPPORT
	g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::UseOperaAccount, pref_opera_account);
	g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverEnable, pref_webserver_enabled);
	g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverListenToAllNetworks, pref_listen_all);
#endif // WEBSERVER_SUPPORT
	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseWebTurbo, pref_web_turbo);
#ifdef DELAYED_SCRIPT_EXECUTION
	g_pcjs->WriteIntegerL(PrefsCollectionJS::DelayedScriptExecution, prefs_delayed_scripts);
#endif // DELAYED_SCRIPT_EXECUTION

	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CheckOtherModification, pref_other_mod);
	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::OtherExpiry, pref_other_expiry);
	g_pcjs->WriteIntegerL(PrefsCollectionJS::IgnoreUnrequestedPopups, pref_ignore_popups);
	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::AlwaysCheckRedirectChanged, pref_cache_redir);
	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::AlwaysCheckRedirectChangedImages, pref_cache_redir_images);
	g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CacheToDisk, pref_enable_disk_cache);
}

void MultiContext::WaitForFiles(URL_CONTEXT_ID ctx, int num_files, BOOL check_immediately)
{
	int idx=GetIndex(ctx);

	if(idx<0)
	{
		ST_failed("Context %d not existing!", (int)ctx);

		return;
	}

	if(check_immediately && CacheHelpers::CheckDirectory((OpFileFolder)folders.Get(idx)) == num_files)
	{
		ST_passed();

		return;
	}

	waits.Get(idx)->SetExpectedFiles(-1, num_files);
	handlers.Get(idx)->PostDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES, 0, 0, 1000);  // Wait to delete the files
}

void MultiContext::EmptyDCache(URL_CONTEXT_ID ctx, BOOL delete_app_cache)
{
	// If the directory does not exists, then it does not wait for the files to be deleted.
	// EmptyDCache() is still called to cover corner cases, like index not yet saved, and only embedded files
	Context_Manager *man=urlManager->FindContextManager(ctx);

	if(!man)
	{
		ST_failed("Context %d not existing!", (int)ctx);

		return;
	}

	OpFileFolder folder=man->GetCacheLocationForFiles();

	if(folder!=OPFILE_ABSOLUTE_FOLDER)
	{
		OpFile dir;
		BOOL exist;
		OpString dir_name;

		if(	OpStatus::IsSuccess(g_folder_manager->GetFolderPath(folder, dir_name)) &&
			OpStatus::IsSuccess(dir.Construct(dir_name.CStr())) &&
			OpStatus::IsSuccess(dir.Exists(exist)) && !exist )
		{
			// Folder not present
			man->EmptyDCache(delete_app_cache);
			ST_passed();

			return;
		}
	}


	WaitURLs *w;
	MessageHandler *m;

	if(ctx==0)
	{
		w=&wait_main;
		m=&mh_main;
	}
	else
	{
		int idx=GetIndex(ctx);

		if(idx<0)
		{
			ST_failed("Context %d not found in MultiContext!", (int)ctx);

			return;
		}

		w=waits.Get(idx);
		m=handlers.Get(idx);
	}

	w->SetExpectedSize(0, 0, FALSE);
	man->EmptyDCache(delete_app_cache);
	m->RemoveDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES, 0, 0); // Make sure there is only one message
	m->PostDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES, 0, 0, 1000);  // Wait to delete the files	
}

int MultiContext::CheckIndex(URL_CONTEXT_ID ctx, BOOL save_index)
{
	int idx=GetIndex(ctx);

	if(idx<0)
	{
		ST_failed("Context %d not existing!", (int)ctx);

		return -1;
	}

	if(save_index)
	{
		RETURN_IF_ERROR(CacheTester::WriteIndex(ctx));
	}

	return CacheHelpers::CheckIndex((OpFileFolder)folders.Get(idx), TRUE);
}


void MultiContext::PrefIgnorePopups(BOOL ignore)
{
	g_pcjs->WriteIntegerL(PrefsCollectionJS::IgnoreUnrequestedPopups, ignore);
}

BOOL MultiContext::ForgetContext(URL_CONTEXT_ID ctx_to_forget, BOOL delete_dir, BOOL clean_handlers)
{
	OP_ASSERT(ctxs.GetCount() == folders.GetCount());
	OP_ASSERT(handlers.GetCount() == waits.GetCount());
	int idx=ctxs.GetCount()-1;

	while(idx>=0)
	{
		URL_CONTEXT_ID ctx=(URL_CONTEXT_ID)ctxs.Get(idx);

		if(ctx==ctx_to_forget)
		{
			ctxs.Remove(idx);
			folders.Remove(idx);

			OpFileFolder folder=(OpFileFolder)folders.Get(idx);

			if(delete_dir)
				OpStatus::Ignore(CacheFileTest::DeleteCacheDir(folder));

			MessageHandler *mh=handlers.Get(idx);
			WaitURLs *wait=waits.Get(idx);

			handlers.Remove(idx);
			waits.Remove(idx);

			if(clean_handlers)
			{
				if(mh)
					mh->UnsetCallBacks(wait);
				OP_DELETE(mh);
				OP_DELETE(wait);
			}

			return TRUE;
		}

		idx--;
	}

	return FALSE;
}

void MMCacheTest::VerifyPartialCoverage(MultimediaCacheFile *cf, OpFileLength position, BOOL expected_available, OpFileLength expected_length)
{
	if(!cf)
		ST_failed("MultimediaCacheFile NULL!");
	else
	{
		BOOL available=FALSE;
		OpFileLength length;

		cf->GetPartialCoverage(position, available, length, TRUE);

		if(expected_available!=available)
			ST_failed("Availability mismatch!");
		else if(expected_length!=length)
			ST_failed("Length mismatch: %d != %d !", (int)expected_length, (int)length);
	}
}

void MMCacheTest::ReadAndVerifyContent(MultimediaCacheFile *cf, OpFileLength position, OpFileLength requested_length, OpFileLength expected_length, UINT8 *read_buf, UINT8 *expected_content)
{
	if(!cf)
		ST_failed("MultimediaCacheFile NULL!");
	else
	{
		UINT32 read = 0;

		// Read
		if(OpStatus::IsError(cf->ReadContent(position, read_buf, (UINT32)requested_length, read)))
		{
			if(expected_length==FILE_LENGTH_NONE)
				return;  // Error expected

			ST_failed("Error on reading content!");
		}
		else if (read!=expected_length)
			ST_failed("Read %d bytes instead of %d", read, (int) expected_length);
		else
		{
			BOOL ok=TRUE;
			UINT32 error_byte=0;

			// Check
			for(UINT32 i=0; i<read; i++)
			{
				if(read_buf[i]!=expected_content[i] && ok)
				{
					ok=FALSE;
					error_byte=i;
				}
				read_buf[i]=0; // reset
			}

			if(!ok)
				ST_failed("Error on verify - First difference after %d bytes", error_byte);
		}
	}
}

void MMCacheTest::ReadFromFileAndVerifyContent(MultimediaCacheFileDescriptor *cfd, OpFileLength position, OpFileLength requested_length, OpFileLength expected_length, UINT8 *read_buf, UINT8 *expected_content)
{
	if(!cfd)
		ST_failed("MultimediaCacheFileDescriptor NULL!");
	else
	{
		OpFileLength read = 0;

		// Read
		cfd->SetReadPosition(position);
		
		if(OpStatus::IsError(cfd->Read(read_buf, requested_length, &read)))
		{
			if(expected_length==FILE_LENGTH_NONE)
				return;  // Error expected

			ST_failed("Error on reading content!");
		}
		else if (read!=expected_length)
			ST_failed("Read %d bytes instead of %d", read, (int) expected_length);
		else
		{
			BOOL ok=TRUE;
			UINT32 error_byte=0;

			// Check
			for(UINT32 i=0; i<read; i++)
			{
				if(read_buf[i]!=expected_content[i] && ok)
				{
					ok=FALSE;
					error_byte=i;
				}
				read_buf[i]=0; // reset
			}

			if(!ok)
				ST_failed("Error on verify - First difference after %d bytes", error_byte);
		}
	}
}

#endif // DISK_CACHE_SUPPORT
#endif // SELFTEST
