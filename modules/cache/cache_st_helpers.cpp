/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SELFTEST

#include "cache_int.h"
#include "cache_common.h"
#include "modules/selftest/testutils.h"
#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_cs.h"
#include "modules/dochand/winman.h"
#include "modules/cache/cache_st_helpers.h"
#include "modules/cache/cache_selftest.h"


#ifdef DISK_CACHE_SUPPORT
WaitURLs::WaitURLs(OpFileFolder defSizeFolder, OpFileFolder custSizeFolder, MessageHandler *mHandler)
: urlsRemaining(0)
, defaultSizeFolder(defSizeFolder)
, customSizeFolder(custSizeFolder)
, expectedSizeLow(-1)
, expectedSizeHigh(-1)
, expectedFilesMain(0)
, expectedFilesCustom(0)
, lastSize(-1)
, lastDeleteCount(-1)
, lastFilesMain(-1) 
, lastFilesCustom(-1) 
, timesWithNoChange(0)
, check_main_dir(TRUE)
, msgHandler(mHandler)
, currId(0)
{ 
}

OP_STATUS WaitURLs::AddURL(const URL &test)
{
	URL *urlPtr = OP_NEW(URL, (test));
	if (!urlPtr)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = testURLs.Add(urlPtr);

	if (OpStatus::IsError(status))
		return status;

	return OpStatus::OK;
}

OP_STATUS WaitURLs::AddURL(UINT32 size, URL_CONTEXT_ID context)
{
	OpString urlPath;
	
	urlPath.AppendFormat(UNI_L("http://testsuites.oslo.opera.com/core/networking/http/cache/data/cache.php?size=%d&type=text/javascript&id=001-%d"), size, currId++);
	
	return AddURL(g_url_api->GetURL(urlPath.CStr(), context));
}

void WaitURLs::ClearURLs()
{
	for (UINT32 i = 0; i < testURLs.GetCount(); ++i)
		OP_DELETE(testURLs.Get(i));
	testURLs.Clear();
}
	
void WaitURLs::SetExpectedFiles(int main, int custom, BOOL add_main)
{ 
	expectedFilesMain = main; 
	expectedFilesCustom = custom; 
	expectedSizeLow = -1;
	expectedSizeHigh = -1;
	lastSize = -1;
	lastDeleteCount = -1;
	check_main_dir = add_main;
}

void WaitURLs::SetExpectedSize(int kbLow, int kbHigh, BOOL add_main)
{ 
	expectedSizeLow = kbLow;
	expectedSizeHigh = kbHigh;
	expectedFilesMain = -1; 
	expectedFilesCustom = -1;
	lastSize = -1;
	lastDeleteCount = -1;
	check_main_dir = add_main;
}

void WaitURLs::Reload() 
{ 
	urlsRemaining = testURLs.GetCount(); 

	for (UINT32 i = 0; i < testURLs.GetCount(); ++i)
	{
		if(OpStatus::IsError(testURLs.Get(i)->LoadDocument(msgHandler, emptyURL, policy)))
		  ST_failed("Load Url %d failed!", i);
	}
}
	
void WaitURLs::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if(msg == MSG_URL_DATA_LOADED)
	{
		for(UINT32 i = 0; i < testURLs.GetCount(); i++)
		{
			if((UINT32)testURLs.Get(i)->Id() == (UINT32)par1 && testURLs.Get(i)->Status(FALSE) == URL_LOADED)
			{
				urlsRemaining--;
				output("URL %d loaded - %d remaining.\n", i, urlsRemaining);
				
				if(!urlsRemaining)
					ST_passed();
			}
		}
	}
	else if(msg == MSG_URL_LOADING_FAILED)
	{
		BOOL signaled=FALSE;

		for(UINT32 i = 0; i < testURLs.GetCount(); i++)
		{
			if((UINT32)testURLs.Get(i)->Id() == (UINT32)par1 && testURLs.Get(i)->Status(FALSE) == URL_LOADED)
			{
				ST_failed("URL %d failed to load.\n", i);
				signaled=TRUE;
			}
		}

		if(!signaled)
		{
			for(UINT32 i = 0; i < testURLs.GetCount(); i++)
			{
				if((UINT32)testURLs.Get(i)->Id() == (UINT32)par1)
				{
					ST_failed("URL %d failed to load 2.\n", i);
					signaled=TRUE;
				}
			}
		}

		if(!signaled)
			ST_failed("URL wiht ID %d failed to load 3.\n", (UINT32)par1);
	}
	else if(msg == MSG_URLMAN_DELETE_SOMEFILES)
	{
		if (expectedSizeLow != -1)
		{
			OpString customDirFolder;
			OpString8 customDirFolder8;
			OpString defaultDirFolder;
			OpString8 defaultDirFolder8;
			FileName_Store	filesMain(8191),
							filesCustomSize(8191),
							filesDefaultSize(8191);
			filesMain.Construct();
			filesCustomSize.Construct();
			filesDefaultSize.Construct();

			g_folder_manager->GetFolderPath(customSizeFolder, customDirFolder);
			g_folder_manager->GetFolderPath(defaultSizeFolder, defaultDirFolder);
			customDirFolder8.Set(customDirFolder);
			defaultDirFolder8.Set(defaultDirFolder);
	
			Context_Manager::ReadDCacheDir(filesMain, filesMain, OPFILE_CACHE_FOLDER, TRUE, FALSE, NULL);
			Context_Manager::ReadDCacheDir(filesCustomSize, filesCustomSize, customSizeFolder, TRUE, FALSE, NULL);
			Context_Manager::ReadDCacheDir(filesDefaultSize, filesDefaultSize, defaultSizeFolder, TRUE, FALSE, NULL);
			int sizeMain=CacheHelpers::ComputeDiskSize(&filesMain);
			int sizeCustom=CacheHelpers::ComputeDiskSize(&filesCustomSize);
			int sizeDefault=CacheHelpers::ComputeDiskSize(&filesDefaultSize);
			int diskSize=sizeCustom + sizeDefault;
			int global_count=(int) urlManager->GetNumFilesToDelete();

			if(check_main_dir)
				diskSize+=sizeMain;

			if (lastSize == diskSize && lastDeleteCount==global_count)
				timesWithNoChange++;
			else
				timesWithNoChange = 0;

			lastSize = diskSize;
			lastDeleteCount = global_count;

			if(timesWithNoChange)
			{
				if(timesWithNoChange>10)
					ST_failed("Time expired: Directory contains %d kB which isn't within the expected range [%d, %d] kB!", diskSize / 1024, expectedSizeLow ? expectedSizeLow / 1024 : 0, expectedSizeHigh ? expectedSizeHigh / 1024 : 0);
				else if ((diskSize > expectedSizeHigh)) // Waiting for the size to decrease
				{
					output("Deleting: still %d kB in dir (%d - %d - %d )- stalled Attempt %d - Dir: %s and %s - Files in global delete list: %d\n", diskSize / 1024, sizeMain, sizeCustom, sizeDefault, timesWithNoChange, customDirFolder8.CStr(), defaultDirFolder8.CStr(), global_count);
					msgHandler->PostDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES, 0, 0, 1300);  // Wait to delete the files
				}
				else
				{
					if(expectedSizeLow > diskSize || diskSize > expectedSizeHigh)
						ST_failed("Directory contains %d kB which isn't within the expected range [%d, %d] kB!", diskSize ? diskSize / 1024 : 0, expectedSizeLow ? expectedSizeLow / 1024 : 0, expectedSizeHigh ? expectedSizeHigh / 1024 : 0);

					ST_passed();
				}
			}
			else
			{
				if(diskSize>=expectedSizeLow && diskSize<=expectedSizeHigh) // A pass in the first message is fine
					ST_passed();
				else
					msgHandler->PostDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES, 0, 0, 1300);  // Wait for the size to stabilize a bit...
			}
		}
		else
		{
			OpString customDirFolder;
			OpString8 customDirFolder8;
			FileName_Store	filesMain(8191),
							filesCustomSize(8191),
							filesDefaultSize(8191);

			filesMain.Construct();
			filesCustomSize.Construct();
			filesDefaultSize.Construct();

			g_folder_manager->GetFolderPath(customSizeFolder, customDirFolder);
			customDirFolder8.Set(customDirFolder);
			
			Context_Manager::ReadDCacheDir(filesMain, filesMain, OPFILE_CACHE_FOLDER, TRUE, FALSE, NULL);
			Context_Manager::ReadDCacheDir(filesCustomSize, filesCustomSize, customSizeFolder, TRUE, FALSE, NULL);
			Context_Manager::ReadDCacheDir(filesDefaultSize, filesDefaultSize, defaultSizeFolder, TRUE, FALSE, NULL);

			UINT32	mainCount = (UINT32)filesMain.LinkObjectCount(),
					customCount = (UINT32)filesCustomSize.LinkObjectCount() + ((defaultSizeFolder)!=customSizeFolder ? (UINT32)filesDefaultSize.LinkObjectCount() : 0);

			if((UINT32)lastFilesMain == mainCount && (UINT32)lastFilesCustom == customCount)
				timesWithNoChange++;
			else
				timesWithNoChange = 0;
			
			if(	( (expectedFilesMain != -1 && (UINT32)mainCount > (UINT32)expectedFilesMain) || (expectedFilesCustom != -1 && customCount > (UINT32)expectedFilesCustom)) && timesWithNoChange < 10)
			{
				lastFilesMain = mainCount;
				lastFilesCustom = customCount;

				if(expectedFilesCustom != -1)
					output("Deleting: still %d files in main dir and %d in custom dir %s - stalled Attempt %d\n", lastFilesMain, lastFilesCustom, customDirFolder8.CStr(), timesWithNoChange);
				else
					output("Deleting: still %d files in main dir - stalled Attempt %d\n", lastFilesMain, timesWithNoChange);

				msgHandler->PostDelayedMessage(MSG_URLMAN_DELETE_SOMEFILES, 0, 0, 1000);  // Wait to delete the files
			}
			else
			{
				if(expectedFilesMain != -1 && mainCount != (UINT32)expectedFilesMain)
					ST_failed("Main directory contains %d files instead of %d!", mainCount, expectedFilesMain);
				else if(expectedFilesCustom != -1 && customCount != (UINT32)expectedFilesCustom)
					ST_failed("Custom directory %s contains %d files instead of %d!", customDirFolder8.CStr(), customCount, expectedFilesCustom);
				else
					ST_passed();
			}
		}
	}
}
#endif // DISK_CACHE_SUPPORT

#ifdef DISK_CACHE_SUPPORT
int CacheHelpers::CheckIndex(OpFileFolder folder, BOOL accept_construction_failure)
{
	int n=0;
	
	#if defined DISK_CACHE_SUPPORT && (defined(CACHE_FAST_INDEX) || CACHE_CONTAINERS_ENTRIES>0)
		DiskCacheEntry entry;
		UINT32 record_tag;
		int record_len;
		OpFile f;
		OpConfigFileReader scache;

		BOOL exists=FALSE;
		
		if(OpStatus::IsError(f.Construct(DiskCacheFile, folder)))
		{
			if(!accept_construction_failure)
				ST_failed("Impossible to create dcache4.url file object!");

			return -1;
		}
		
		if(OpStatus::IsError(f.Open(OPFILE_READ)))
		{
			if(!accept_construction_failure)
				ST_failed("Impossible to open the cache index!");

			return -1;
		}
		
		if(OpStatus::IsError(f.Exists(exists)) || !exists)
		{
			if(!accept_construction_failure)
				ST_failed("Index file does not exists!");

			return -1;
		}

		if(OpStatus::IsError(scache.Construct(&f, FALSE)))
		{
			if(!accept_construction_failure)
				ST_failed("Impossible to construct the cache reader");

			return -1;
		}
		
		#if CACHE_SMALL_FILES_SIZE>0
			OpStatus::Ignore(entry.ReserveSpaceForEmbeddedContent(CACHE_SMALL_FILES_SIZE));
		#endif
		
		while(OpStatus::IsSuccess(scache.ReadNextRecord(record_tag, record_len)) && record_len>0)
		{
			if(record_tag==1)
				n++;
		}
		
		scache.Close();
	#endif // defined DISK_CACHE_SUPPORT && (defined(CACHE_FAST_INDEX) || CACHE_CONTAINERS_ENTRIES>0)
	
	return n;
}

UINT32 CacheHelpers::ComputeDiskSize(FileName_Store *filenames)
{
	UINT32 size=0;
	
	#ifdef DISK_CACHE_SUPPORT
		FileNameElement* name;
		OpFileLength len;
		
		name = filenames->GetFirstFileName();
		
		while(name)
		{
			OpFile f;
			
			f.Construct(name->FileName().CStr(), name->Folder());
			len=0;
			f.GetFileLength(len);
			size+=(UINT32)len;
			
			name = filenames->GetNextFileName();
		}
	#endif
	
	return size;
}
#endif // DISK_CACHE_SUPPORT

#if ( defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE) ) && !defined(SEARCH_ENGINE_CACHE)
int CacheHelpers::CheckDirectory(OpFileFolder folder, BOOL add_assoc_files)
{
	FileName_Store *files;
	int ret=-1;

	files=OP_NEW(FileName_Store, (8191));

	if(files && OpStatus::IsSuccess(files->Construct()))
	{
		Context_Manager::ReadDCacheDir(*files, *files, folder, TRUE, add_assoc_files, NULL, -1, TRUE, TRUE);

		ret=(int)files->LinkObjectCount();
	}

	OP_DELETE(files);

	return ret;
}
#endif // ( defined(DISK_CACHE_SUPPORT) || defined(CACHE_PURGE) ) && !defined(SEARCH_ENGINE_CACHE)

/// Create ONE fake URL and put it into the cache; the URL can be retrieved.
URL CacheHelpers::CacheBogusURLRetrieve(int context, unsigned int size, BOOL load_in_the_past, BOOL set_inuse, BOOL unique, URLType type)
{
	OpAutoVector<URL> testURLs;

	CacheBogusURLs(context, size, size, 1, 0, &testURLs, id++, FALSE, load_in_the_past, set_inuse, unique, type);

	if(testURLs.Get(0))
	{
		OP_ASSERT(unique || testURLs.Get(0)->GetRep()->GetRefCount()==2);
		OP_ASSERT(unique || testURLs.Get(0)->GetRep()->GetUsedCount()==1 || !set_inuse);
		OP_ASSERT(testURLs.Get(0)->Type()==type);

		return *testURLs.Get(0);
	}

	return URL();
}

void CacheHelpers::CloseUnusedWindows()
{
	Window * win=g_windowManager->FirstWindow();
	
	while(win)
	{
		Window * temp_win=win;
		URL url=temp_win->GetCurrentURL();
		
		win=win->Suc();

	#ifdef WEBSERVER_SUPPORT
		if(!g_webserver || NULL==g_webserver->WindowHasWebserverAssociation(temp_win->Id()))
		{
			OpString name;
			url.GetURLDisplayName(name);
			if(KNotFound==name.Find("elftest"))  // Covers both "selftest" and "Selftest"
				temp_win->Close();
		}
	#endif
	}
}

void CacheHelpers::CacheBogusURLs(int context, unsigned int minSize, unsigned int maxSize, int num, int totalBytes, OpAutoVector<URL> *testURLs, int id, BOOL plain, BOOL load_in_the_past, BOOL set_inuse, BOOL unique, URLType type)
{
	// Sanity check sizes
	if (minSize > maxSize)
		maxSize = minSize;

	char *data = OP_NEWA(char, maxSize);

	if(!data)
		return;

	// Fill the entire buffer with an easy to check sequence.
	char *curr = data,
		 *end = curr + maxSize;
	unsigned char c = 0;
	for (; curr != end; ++curr, ++c)
		*curr = c;

	//char url[12 + 27]; /* ARRAY OK 2009-10-29 emoller */
	char *url=(char *)g_memory_manager->GetTempBuf();

	// Loop until both conditions are satisfied. Total bytes and number of files.
	int bytes = 0;
	int i = 0;
	while (i < num || bytes < totalBytes)
	{
		// Give each one a unique url.
		if(type==URL_HTTP)
			op_snprintf(url, 100, "http://example.tld/foobar%d.html", id + i);
		else if(type==URL_HTTPS)
			op_snprintf(url, 100, "https://example.tld/foobar%d.html", id + i);
		else if(type==URL_FILE)
			op_snprintf(url, 100, "file://example_%d.html", id + i);

		// Randomize between min and max size.
		int var = minSize == maxSize ? 0 : op_rand() % (maxSize - minSize);

		// Create the URL and add the generated data to it.
		URL test = g_url_api->GetURL(url, NULL, unique, context);

		op_snprintf(data, maxSize, "bogus %010d bytes png, id=%010d", minSize + var, id + i);
		test.WriteDocumentImage("image/png", data, minSize + var);
		test.WriteDocumentDataFinished();

		URL_Rep *test_rep = test.GetRep();
		if (test_rep)
		{
			URL_DataStorage *url_storage = test_rep->GetDataStorage();
			
			if (url_storage)
			{
				if (plain)
				{
					
					#if (CACHE_SMALL_FILES_SIZE>0 || CACHE_CONTAINERS_ENTRIES>0)
						Cache_Storage *cache_storage = url_storage->GetCacheStorage();

						if (cache_storage)
							cache_storage->SetPlainFile(TRUE);
					#endif
				}
				
				if(load_in_the_past)
					CacheTester::MoveCreationInThePast(url_storage);
			}
			
			if(load_in_the_past)
				CacheTester::MoveLocalTimeVisitedInThePast(test_rep);
		}

		// Set some response header values.
		//char entityTag[27]; /* ARRAY OK 2009-10-29 emoller */
		char *entityTag=(char *)g_memory_manager->GetTempBuf2();
		op_snprintf(entityTag, 27, "4d3c050-113-474%010d", id + 1);
		test.SetAttribute(URL::KHTTP_Response_Code, 200);
		test.SetAttribute(URL::KUsedUserAgentId, 1);
		test.SetAttribute(URL::KHTTP_Date, "Web, 28 Oct 2009 12:15:50 GMT");
		test.SetAttribute(URL::KHTTP_LastModified, "Web, 28 Oct 2009 12:15:50 GMT");
		test.SetAttribute(URL::KHTTP_EntityTag, entityTag);

		if(set_inuse)
			test.GetRep()->IncUsed(1);

		if (testURLs)
		{
			URL *newURL = OP_NEW(URL, (test));
			testURLs->Add(newURL);
		}

		bytes += minSize + var;
		++i;
	}

	OP_DELETEA(data);
}

URL CacheHelpers::CacheCustomURLBinary(int context, const void *content, int size, const char *target_url, const char *mime_type, BOOL plain, BOOL set_inuse, BOOL unique)
{
	const char *url_to_get=target_url;

	if(!url_to_get)
	{
		char *tmp=(char *)g_memory_manager->GetTempBuf();

		// Give each one a (usually...) unique url.
		op_snprintf(tmp, 100, "http://example.tld/custom%d_%d", op_rand(), (int)(g_op_time_info->GetTimeUTC()));

		url_to_get=tmp;
	}

	// Create the URL and add the generated data to it.
	URL test = g_url_api->GetURL(url_to_get, NULL, unique, context);

	test.WriteDocumentImage(mime_type, (const char *)content, size);
	test.WriteDocumentDataFinished();

	URL_Rep *test_rep = test.GetRep();
	if (test_rep)
	{
		#if (CACHE_SMALL_FILES_SIZE>0 || CACHE_CONTAINERS_ENTRIES>0)
			if (plain)
			{
				if (test_rep->GetDataStorage() && test_rep->GetDataStorage()->GetCacheStorage())
					test_rep->GetDataStorage()->GetCacheStorage()->SetPlainFile(TRUE);
			}
		#endif
	}

	// Set some response header values.
	//char entityTag[27]; /* ARRAY OK 2009-10-29 emoller */
	char *entityTag=(char *)g_memory_manager->GetTempBuf2();
	op_snprintf(entityTag, 27, "4d3c050-113-474%010d", 0x1234);
	test.SetAttribute(URL::KHTTP_Response_Code, 200);
	test.SetAttribute(URL::KUsedUserAgentId, 1);
	test.SetAttribute(URL::KHTTP_Date, "Web, 28 Oct 2009 12:15:50 GMT");
	test.SetAttribute(URL::KHTTP_LastModified, "Web, 28 Oct 2009 12:15:50 GMT");
	test.SetAttribute(URL::KHTTP_EntityTag, entityTag);

	if(set_inuse)
		test.GetRep()->IncUsed(1);

	// Load it and optionally keep a reference to it.
	test.QuickLoad(TRUE);

	return test;
}


#endif //SELFTEST
