/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/util/htmlify.h"
#include "modules/url/protocols/http1.h"
#include "modules/cache/cache_man.h"

#include "modules/about/operablank.h"
#include "modules/about/operablanker.h"
#include "modules/about/operaabout.h"
#include "modules/about/operacredits.h"
#include "modules/about/operacpu.h"
#ifdef VEGA_OPPAINTER_SUPPORT
# include "modules/about/operagpu.h"
#endif // VEGA_OPPAINTER_SUPPORT
#include "modules/about/operadrives.h"
#include "modules/about/operahistorysearch.h"
#include "modules/about/operahistory.h"
#include "modules/about/operaplugins.h"
#include "modules/about/operacache.h"
#include "modules/about/operaperformance.h"

#ifdef ABOUT_PRIVATE_BROWSING
#include "modules/about/opprivacymode.h"
#include "modules/dochand/win.h"
#endif

#ifdef _OPERA_DEBUG_DOC_
#include "modules/about/operadebugnet.h"
#include "modules/dochand/winman.h"
#endif

#include "modules/about/operafeeds.h"
#ifdef ABOUT_OPERA_DEBUG
#include "modules/about/operadebug.h"
#endif // ABOUT_OPERA_DEBUG
#ifdef OPERACONFIG_URL
# include "modules/prefs/util/operaconfig.h"
#endif

#if defined OPERAWIDGETS_URL || defined OPERAEXTENSIONS_URL
# include "modules/gadgets/operawidgets.h"
#endif
#ifdef OPERAUNITE_URL
# include "modules/gadgets/operaunite.h"
#endif
#ifdef OPERABOOKMARKS_URL
#include "modules/bookmarks/operabookmarks.h"
#endif // OPERABOOKMARKS_URL

#ifdef _OPERA_DEBUG_DOC_
#include "modules/ecmascript_utils/esasyncif.h"
#endif

#if !defined(NO_URL_OPERA) || defined(HAS_OPERABLANK)

#include "modules/viewers/viewers.h"

#include "modules/url/url_ds.h"
#include "modules/url/url_pd.h"
#include "modules/cache/url_stor.h"
#include "modules/cache/url_cs.h"
#include "modules/util/opfile/opfile.h"
#include "modules/formats/uri_escape.h"

#include "modules/hardcore/mem/mem_man.h"

#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#ifdef WEBFEEDS_DISPLAY_SUPPORT
# include "modules/webfeeds/webfeeds_api.h"
#endif // WEBFEEDS_DISPLAY_SUPPORT

#include "modules/url/tools/arrays.h"

#ifdef NEED_LOGO_INC
# include "modules/url/opera_logo.inc"
#endif

#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/url_lop_api.h"

// Url_lop.cpp

// URL Load Opera URL's

#if !defined(NO_URL_OPERA) || defined(HAS_OPERABLANK)
#define CONST_KEYWORD_ARRAY(name) PREFIX_CONST_ARRAY(static, KeywordIndex, name, url)
#define CONST_KEYWORD_ENTRY(x,y) CONST_DOUBLE_ENTRY(keyword, x, index, y)
#define CONST_KEYWORD_END(name) CONST_END(name)

enum opera_url_keyword_enums {
	OP_URL_UNKNOWN,
	OP_URL_HELP,
	OP_URL_ABOUT,
	OP_URL_BLANK,
	OP_URL_BLANKER,
	OP_URL_BOOKMARK,
	OP_URL_CACHE,
	OP_URL_CONFIG,
	OP_URL_DEBUG,
	OP_URL_DEBUG_DOC,
	OP_URL_DRIVES,
	OP_URL_EXTENSIONS,
	OP_URL_FEED_ID,
	OP_URL_FEEDS,
	OP_URL_CPU,
#ifdef VEGA_OPPAINTER_SUPPORT
	OP_URL_GPU,
#endif // VEGA_OPPAINTER_SUPPORT
	OP_URL_HISTORY,
	OP_URL_HISTORYSEARCH,
	OP_URL_LOGO,
	OP_URL_PERFORMANCE,
	OP_URL_PLUGINS,
	OP_URL_WIDGETS,
	OP_URL_UNITE,
	OP_URL_PRIVATE,
#ifdef OPERA_CREDITS_PAGE
	OP_URL_CREDITS,
#endif // OPERA_CREDITS_PAGE
};


CONST_KEYWORD_ARRAY(operaurl_keyword)
	// This list *must* be in alphabetical order, otherwise the binary search will fail!
	// If you add/remove elements to/from this list, StartLoadingOperaSpecialDocument must be updated as well
CONST_KEYWORD_ENTRY(NULL,          OP_URL_UNKNOWN)
CONST_KEYWORD_ENTRY("/help/",      OP_URL_HELP)// special special case, only test on keyword inside first set of slashes
#if !defined(NO_URL_OPERA)
CONST_KEYWORD_ENTRY("about",       OP_URL_ABOUT)
#endif
#if !defined(NO_URL_OPERA) || defined(HAS_OPERABLANK)
#ifdef HAS_OPERABLANK
CONST_KEYWORD_ENTRY("blank",       OP_URL_BLANK)
#endif
#endif
#if !defined(NO_URL_OPERA)
#ifdef SELFTEST
CONST_KEYWORD_ENTRY("blanker",     OP_URL_BLANKER)
#endif
#ifdef OPERABOOKMARKS_URL
CONST_KEYWORD_ENTRY("bookmarks",   OP_URL_BOOKMARK)
#endif // OPERABOOKMARKS_URL
CONST_KEYWORD_ENTRY("cache",       OP_URL_CACHE)
#ifdef OPERACONFIG_URL
CONST_KEYWORD_ENTRY("config",      OP_URL_CONFIG)
#endif
#ifdef CPUUSAGETRACKING
CONST_KEYWORD_ENTRY("cpu",      OP_URL_CPU)
#endif // CPUUSAGETRACKING
#if defined(OPERA_CREDITS_PAGE)
CONST_KEYWORD_ENTRY("credits", OP_URL_CREDITS)
#endif // OPERA_CREDITS_PAGE
#ifdef ABOUT_OPERA_DEBUG
CONST_KEYWORD_ENTRY("debug",          OP_URL_DEBUG)
#endif // ABOUT_OPERA_DEBUG
#ifdef SYS_CAP_FILESYSTEM_HAS_MULTIPLE_DRIVES
CONST_KEYWORD_ENTRY("drives",         OP_URL_DRIVES)
#endif
#ifdef OPERAEXTENSIONS_URL
CONST_KEYWORD_ENTRY("extensions",     OP_URL_EXTENSIONS)
#endif // OPERAWIDGETS_URL
#ifdef URL_ENABLE_OPERA_FEEDS_URL
CONST_KEYWORD_ENTRY("feed-id",        OP_URL_FEED_ID)
CONST_KEYWORD_ENTRY("feeds",          OP_URL_FEEDS)
#endif // URL_ENABLE_OPERA_FEEDS_URL
#ifdef VEGA_OPPAINTER_SUPPORT
CONST_KEYWORD_ENTRY("gpu",            OP_URL_GPU)
#endif // VEGA_OPPAINTER_SUPPORT
CONST_KEYWORD_ENTRY("help",		      OP_URL_HELP)// special special case, only test on keyword inside first set of slashes
#ifdef HISTORY_SUPPORT
CONST_KEYWORD_ENTRY("history",        OP_URL_HISTORY)
#endif
#ifdef OPERAHISTORYSEARCH_URL
CONST_KEYWORD_ENTRY("historysearch",  OP_URL_HISTORYSEARCH)
#endif
#ifdef NEED_LOGO_INC
CONST_KEYWORD_ENTRY("logo",           OP_URL_LOGO)
#endif
#ifdef _OPERA_DEBUG_DOC_
CONST_KEYWORD_ENTRY("memdebug",          OP_URL_DEBUG_DOC)
#endif // _OPERA_DEBUG_DOC_
#ifdef OPERA_PERFORMANCE
CONST_KEYWORD_ENTRY("performance",          OP_URL_PERFORMANCE)
#endif // OPERA_PERFORMANCE
#ifdef _PLUGIN_SUPPORT_
CONST_KEYWORD_ENTRY("plugins",        OP_URL_PLUGINS)
#endif
#ifdef ABOUT_PRIVATE_BROWSING
CONST_KEYWORD_ENTRY("private",        OP_URL_PRIVATE)
#endif
#ifdef OPERAUNITE_URL
CONST_KEYWORD_ENTRY("unite", OP_URL_UNITE)
#endif // OPERAUNITE_URL
#ifdef OPERAWIDGETS_URL
CONST_KEYWORD_ENTRY("widgets", OP_URL_WIDGETS)
#endif // OPERAWIDGETS_URL
#endif
CONST_END(operaurl_keyword)


void URL_DataStorage::StartLoadingOperaSpecialDocument(MessageHandler *msg_handler)
{
	OP_MEMORY_VAR OP_STATUS stat = OpStatus::OK;
	OpStatus::Ignore(stat);

	SetAttribute(URL::KLoadStatus,URL_LOADING);

    URL the_url = URL(url, (char *) NULL);

	const char *path = url->GetAttribute(URL::KPath).CStr();

	int sel=0;
	int keywordlength = 0;
	const char* slash = op_strchr(path, '/');
	if (slash)  // if keyword contains slash only use the part before slash as keyword
	{
		if(path == slash)		//if this is a keyword of type /help check on lenght of this only
		{
			const char* nextslash = op_strchr(path+1, '/');
			if(nextslash)
			{
				keywordlength = nextslash-path;
				sel = CheckKeywordsIndex(path,keywordlength,g_operaurl_keyword, CONST_ARRAY_SIZE(url, operaurl_keyword));
			}
		}
		else
		{
			keywordlength = slash-path;
			sel = CheckKeywordsIndex(path,keywordlength,g_operaurl_keyword, CONST_ARRAY_SIZE(url, operaurl_keyword));
		}
	}

	if(keywordlength == 0)	//the path did not contain a legal slash pair
	{
		sel = CheckKeywordsIndex(path,g_operaurl_keyword, CONST_ARRAY_SIZE(url, operaurl_keyword));
	}

	OpGeneratedDocument *document = NULL;

	switch(sel)
	{
#if !defined(NO_URL_OPERA) 
	case OP_URL_CACHE:
		document = OP_NEW(OperaCache, (the_url));
		break;
#ifdef _PLUGIN_SUPPORT_
	case OP_URL_PLUGINS:
		document = OP_NEW(OperaPlugins, (the_url));
		break;
#endif

	case OP_URL_ABOUT:
		document = OperaAbout::Create(the_url);
		break;

#ifdef OPERA_CREDITS_PAGE
	case OP_URL_CREDITS:
		document = OP_NEW(OperaCredits, (the_url));
		break;
#endif // OPERA_CREDITS_PAGE

#ifdef CPUUSAGETRACKING
	case OP_URL_CPU:
		document = OP_NEW(OperaCPU, (the_url));
		break;
#endif // CPUUSAGETRACKING

#ifdef VEGA_OPPAINTER_SUPPORT
	case OP_URL_GPU:
		document = OP_NEW(OperaGPU, (the_url));
		break;
#endif // VEGA_OPPAINTER_SUPPORT

#ifdef HISTORY_SUPPORT
	case OP_URL_HISTORY:
		document = OP_NEW(OperaHistory, (the_url));
		break;
#endif

#ifdef SYS_CAP_FILESYSTEM_HAS_MULTIPLE_DRIVES
	case OP_URL_DRIVES:
		document = OP_NEW(OperaDrives, (the_url));
		break;
#endif
#ifdef SELFTEST
	case OP_URL_BLANKER: // blanker
	{
		document = OP_NEW(OperaBlanker, (the_url));
		break;
	}
#endif
#endif //!defined(NO_URL_OPERA)

#if defined(HAS_OPERABLANK)
	case OP_URL_BLANK:
		document = OP_NEW(OperaBlank, (the_url));
    	break;
#endif

#ifdef OPERABOOKMARKS_URL
	case OP_URL_BOOKMARK:
		document = OP_NEW(OperaBookmarks, (the_url));
		break;
#endif // OPERABOOKMARKS_URL

#ifdef OPERACONFIG_URL
	case OP_URL_CONFIG:
		document = OP_NEW(OperaConfig, (the_url));
		// Bug#281924: Make sure the contents of opera:config is fresh
		g_url_api->MakeUnique(the_url);
		break;
#endif

#ifdef OPERAWIDGETS_URL
	case OP_URL_WIDGETS:
        document = OP_NEW(OperaWidgets, (the_url, OperaWidgets::Widgets));
        break;
#endif

#ifdef OPERAEXTENSIONS_URL
	case OP_URL_EXTENSIONS:
        document = OP_NEW(OperaWidgets, (the_url, OperaWidgets::Extensions));
        break;
#endif

#ifdef OPERAUNITE_URL
	case OP_URL_UNITE:
        document = OP_NEW(OperaUnite, (the_url));
        break;
#endif

#ifdef NEED_URL_WRITE_DOC_IMAGE
	case OP_URL_LOGO:
		stat = the_url.WriteDocumentImage("image/png", (const char *) &opera_logo_png, sizeof(opera_logo_png));
        break;
#endif // NEED_URL_WRITE_DOC_IMAGE

#ifdef URL_ENABLE_OPERA_FEEDS_URL
	case OP_URL_FEED_ID:  // on format feed-id/xxxxxxxx or feed-id/xxxxxxxx-xxxxxxxx
		switch(g_webfeeds_api->WriteByFeedId(path+keywordlength, the_url))
		{
		case WebFeedsAPI::Written:
			break;  // loaded ok
		case WebFeedsAPI::Queued:
			// this URL will be finished later by WebFeeds
			return;
		case WebFeedsAPI::IllegalId:
		case WebFeedsAPI::UnknownId:
			SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_ILLEGAL_URL));
			break;
		case WebFeedsAPI::WriteError:
			goto failure;
		}
		
		break;

	case OP_URL_FEEDS:
		document = OP_NEW(OperaFeeds, (the_url));
		break;
#endif // URL_ENABLE_OPERA_FEEDS_URL

#ifdef _OPERA_DEBUG_DOC_
	case OP_URL_DEBUG_DOC:
		document = OP_NEW(OperaDebugNet, (the_url));
		break;
#endif

#ifdef OPERA_PERFORMANCE
	case OP_URL_PERFORMANCE:
		document = OP_NEW(OperaPerformance, (the_url));
		break;
#endif // OPERA_PERFORMANCE

#ifdef OPERAHISTORYSEARCH_URL
	case OP_URL_HISTORYSEARCH:
		document = OperaHistorySearch::Create(the_url);
		break;
#endif

#if !defined (_NO_HELP_)
	case OP_URL_HELP:
		{
			OpString fname;
			if (keywordlength!=0)
				stat = fname.Set(path+keywordlength+1);	//different levels of errors, but anyway
			if(OpStatus::IsSuccess(stat) && fname.IsEmpty())
				stat = fname.Set(UNI_L("index.html"));
			if(OpStatus::IsError(stat))
				goto failure;

			if(fname.Find("%25") != KNotFound) // %XX encoded "%" not allowed
				goto failure;

			UriUnescape::ReplaceChars(fname.DataPtr(), UriUnescape::NonCtrlAndEsc);

			if(fname.Find("..") != KNotFound || fname.Find("./") != KNotFound || fname.Find(".\\") != KNotFound) // path navigation is not allowed
				goto failure;

			urlManager->MakeUnique(url);

			SetAttribute(URL::KLoadStatus,URL_LOADED);

			OpString path;
			TRAPD(rc, g_pcnet->GetStringPrefL(PrefsCollectionNetwork::HelpUrl, path));
			if (OpStatus::IsError(rc))
			{
				goto failure;
			}
			//unescape the url

			OpString full;

			if(OpStatus::IsError(full.SetConcat(path, fname)))
			{
				stat = OpStatus::ERR_NO_MEMORY;
				goto failure;//FIXME:OOM7 - Seems to be in between two different ways of handling errors, not sure if this is correct...
			}

			OpString  full2;

			TRAPD(op_err, ResolveUrlNameL(full, full2));

			URL temp_url = urlManager->GetURL(full2);
			TRAP(op_err, SetAttributeL(URL::KMovedToURL, temp_url));
			if(OpStatus::IsSuccess(op_err) )
			{
				url->SetAttribute(URL::KIsGeneratedByOpera, TRUE);
				ExecuteRedirect_Stage2();
			}
			else
			{
				HandleFailed(URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN));
			}

			return;

		}
		break;
#endif // ! _NO_HELP_

#ifdef ABOUT_OPERA_DEBUG
	case OP_URL_DEBUG:
		document = OP_NEW(OperaDebug, (the_url));
	break;
#endif // ABOUT_OPERA_DEBUG

#ifdef ABOUT_PRIVATE_BROWSING
	case OP_URL_PRIVATE:
		document = OP_NEW(OpPrivacyModePage, (the_url, msg_handler->GetWindow() && msg_handler->GetWindow()->GetPrivacyMode()));
	break;
#endif // ABOUT_PRIVATE_BROWSING

	default:
		{
			OP_ASSERT(path == url->GetAttribute(URL::KPath).CStr());
			OperaURL_Generator *generator = g_OperaUrlGenerators.First();
			while(generator)
			{
				if(generator->MatchesPath(path))
				{
					URL this_url(url, (char *) NULL);
					OP_STATUS op_err = OpStatus::OK;

					switch(generator->GetMode())
					{
					case OperaURL_Generator::KFileContent:
						{
							OpFileFolder folder = OPFILE_ABSOLUTE_FOLDER;
							OpStringC name = generator->GetFileLocation(this_url, folder, op_err);

							if(name.IsEmpty())
								break; // No file for this

							if(OpStatus::IsError(op_err))
								break;

							Cache_Storage *new_cs = Local_File_Storage::Create(url, name,folder);
							if(new_cs == NULL)
								break; // Maybe OOM, but let's continue

							storage = new_cs;
							SetAttribute(URL::KLoadStatus,URL_LOADED);
							goto end;
						}
					case OperaURL_Generator::KQuickGenerate:
						{
							OpWindowCommander* commander = NULL;
							if (msg_handler->GetWindow())
								commander = (OpWindowCommander*)msg_handler->GetWindow()->GetWindowCommander();

							op_err = generator->QuickGenerate(this_url, commander);

							if(OpStatus::IsError(op_err))
								break;

							goto end;
						}
					case OperaURL_Generator::KGenerateLoadHandler:
						{
							URL_LoadHandler *new_lh = NULL;

							op_err = generator->RetrieveLoadHandler(this_url,new_lh);

							if(OpStatus::IsError(op_err))
							{
								OP_DELETE(new_lh);
								break;
							}

							loading = new_lh;
							return;
						}
					}
					if(op_err == OpStatus::ERR_NOT_SUPPORTED)
					{
						OP_DELETE(storage); // Make sure the content has been destroyed
						storage = NULL;
						break; // no content for this, try the next one.
					}
					else if(OpStatus::IsError(op_err))
					{
						if(OpStatus::IsMemoryError(op_err))
							g_memory_manager->RaiseCondition(op_err);
						goto failure;
					}
				}
				
				generator = generator->Suc();
			}

			URLContentType ctype =(URLContentType) GetAttribute(URL::KContentType);
			if ((ctype != URL_HTML_CONTENT) &&
				(ctype != URL_XML_CONTENT)
				)
			{
				SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, URL_ERRSTR(SI, ERR_ILLEGAL_URL));
			}
		}
	}

	if (document)
	{
		OP_STATUS rc = document->GenerateData();
		OP_DELETE(document);
		document = NULL;
		if (OpStatus::IsError(rc))
		{
			goto failure;
		}
	}

	SetAttribute(URL::KLoadStatus,URL_LOADED);
	goto end;
failure:
	if (document)
		OP_DELETE(document);
	if(storage)
		storage->SetFinished();
	if(OpStatus::IsMemoryError(stat))//FIXME:OOM7 - Can't return the OOM error.
		g_memory_manager->RaiseCondition(stat);
	SetAttribute(URL::KLoadStatus,URL_LOADING_FAILURE);
	BroadcastMessage(MSG_URL_LOADING_FAILED, url->GetID(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR), MH_LIST_ALL);
	UnsetListCallbacks();
	mh_list->Clean();
	url->Access(FALSE);
	return;
end:
	if(!storage)
	{
		storage = old_storage;
	}
	else
	{
		OP_DELETE(old_storage);
	}
	old_storage = NULL;
	BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
	BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
	UnsetListCallbacks();
	mh_list->Clean();
	url->Access(FALSE);
}
#endif // !defined(NO_URL_OPERA) || defined(HAS_OPERABLANK)

#if !defined(NO_URL_OPERA) 
/*
OOM-Fixed this for unicode on windows. Have no idea if it will work for other
platforms or configurations. - tord
*/

#ifdef _OPERA_DEBUG_DOC_
OP_STATUS URL_Manager::GenerateDebugPage(URL& url)
{
// ----------------------------------------------------------------------------------------
// Memory cache information

	url.WriteDocumentDataUniSprintf(
		UNI_L("<h1>Main Memory Consumers in Opera</h1>\r\n<table>\r\n<tr>    <td></td>            <td>used</td>      <td>max</td>       <td>percent</td>    </tr>\r\n <tr>    <td>Documents</td>   <td>%ld kB</td>    <td>%ld kB</td>    <td>%d</td></tr>\r\n"),
		g_memory_manager->DocMemoryUsed() / 1024,
		g_memory_manager->MaxDocMemory() / 1024,
		g_memory_manager->MaxDocMemory() ? (int) (100.0 * g_memory_manager->DocMemoryUsed() / g_memory_manager->MaxDocMemory()) : 0);

	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr style='font-size:smaller;font-style:italic'>    <td align=right>EcmaScript</td>   <td>%ld kB</td>    <td></td>    <td></td>         </tr>\r\n"),
		g_ecmaManager->GetCurrHeapSize() / 1024);

	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr style='font-size:smaller;font-style:italic'>    <td align=right>Active documents</td>   <td>%d</td>    <td></td>    <td></td>         </tr>\r\n"),
		g_windowManager->GetActiveDocumentCount());

	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr style='font-size:smaller;font-style:italic'>    <td align=right>Cached documents</td>   <td>%d</td>    <td></td>    <td></td>         </tr>\r\n"),
		g_memory_manager->GetCachedDocumentCount());

	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr style='font-size:smaller;font-style:italic'>    <td align=right>Archived documents</td>   <td>%d</td>    <td></td>    <td></td>         </tr>\r\n"),
		g_windowManager->GetDocumentCount() - g_memory_manager->GetCachedDocumentCount() - g_windowManager->GetActiveDocumentCount());

	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr>    <td>Images</td>      <td>%ld kB</td>    <td>%ld kB</td>    <td>%d</td>         </tr>\r\n"),
		imgManager->GetUsedCacheMem() / 1024,
		imgManager->GetCacheSize() / 1024,
		imgManager->GetCacheSize() ? (int) (100.0 * imgManager->GetUsedCacheMem() / imgManager->GetCacheSize()) : 0);

#ifdef IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr>    <td>Image animations</td>      <td>%ld kB</td>    <td>%ld kB</td>    <td>%d</td>         </tr>\r\n"),
		imgManager->GetUsedAnimCacheMem() / 1024,
		imgManager->GetAnimCacheSize() / 1024,
		imgManager->GetAnimCacheSize() ? (int) (100.0 * imgManager->GetUsedAnimCacheMem() / imgManager->GetAnimCacheSize()) : 0);
#endif // IMG_CACHE_MULTIPLE_ANIMATION_FRAMES

	Cache_Manager::WriteDebugInfo(url);


	url.WriteDocumentData(URL::KNormal, 
		UNI_L("<table>\r\n<tr>    <td>URL</td>   <td>Memory used</td>  <td>Reference count</td>  <td>Lock</td>  <td>Showed</td>  </tr>\r\n"));


	size_t total_mem = 0;

/*	for (ImageSource* candidate = (ImageSource*)image_list->First();
		 candidate != NULL;
		 candidate = candidate->Suc())
	{
		url.WriteDocumentDataUniSprintf(
			UNI_L("<tr>    <td>%s</td>      <td>%d kB</td>    <td>%d</td>    <td>%s</td>    <td>%d</td>    </tr>\r\n"),
			candidate->GetUrl()->UniName(PASSWORD_SHOW),
			candidate->GetMemUsed() / 1024,
			candidate->GetRefCount(),
			candidate->IsLocked() ? UNI_L("yes") : UNI_L("no"),
			candidate->GetShowedCount());

		total_mem += candidate->GetMemUsed();
	}
*/

	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr>    <td></td>    <td>%d kB</td>    <td></td>  <td></td>   <td></td>    </tr>\r\n"),
		total_mem);



	url.WriteDocumentData(URL::KNormal, 
		UNI_L("</table>\r\n"));

	DebugUrlMemory debugurl;

	urlManager->GetMemUsed(debugurl);

	url.WriteDocumentDataUniSprintf(
		UNI_L("<h1>URL Manager info</h1>\r\n<table><tr><td>Category</td><td>Number of entries</td><td>Memory Consumed (bytes)<td></tr>\r\n")
		);
	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr>    <td>Common buffers</td>    <td></td>    <td>%lu</td>    </tr>\r\n"),
			debugurl.memory_buffers);
	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr>    <td>Server Names</td>    <td>%lu</td>    <td>%lu</td>    </tr>\r\n"),
			debugurl.number_servernames, debugurl.memory_servernames);
	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr>    <td>Unloaded URLs</td>    <td>%lu</td>    <td>%lu</td>    </tr>\r\n"),
			debugurl.number_visited, debugurl.memory_visited);
	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr>    <td>Loaded URLs</td>    <td>%lu</td>    <td>%lu</td>    </tr>\r\n"),
			debugurl.number_loaded, debugurl.memory_loaded);
	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr>    <td>Loaded URLs in Ramcache</td>    <td>%lu</td>    <td>%lu</td>    </tr>\r\n"),
			debugurl.number_ramcache, debugurl.memory_ramcache);
	if(debugurl.number_waiting_cookies)
		url.WriteDocumentDataUniSprintf(
			UNI_L("<tr>    <td>Cookies</td>    <td>%lu (+%lu waiting)</td>    <td>%lu</td>    </tr>\r\n"),
				debugurl.number_cookies, debugurl.number_waiting_cookies, debugurl.memory_cookies);
	else
		url.WriteDocumentDataUniSprintf(
			UNI_L("<tr>    <td>Cookies</td>    <td>%lu</td>    <td>%lu</td>    </tr>\r\n"),
				debugurl.number_cookies, debugurl.memory_cookies);

	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr>    <td>Total Memory used</td>    <td></td>    <td>%u</td></tr>\r\n"),
		debugurl.memory_buffers + debugurl.memory_servernames + debugurl.memory_visited+debugurl.memory_loaded+ debugurl.memory_ramcache+ debugurl.memory_cookies);


	// socket information
	url.WriteDocumentDataUniSprintf(UNI_L("<h1>URL information</h1>\r\n"));


	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr>    <td>Registered Ramcache</td>    <td></td>    <td>%lu</td>    </tr>\r\n"),
			debugurl.registered_ramcache);

	url.WriteDocumentDataUniSprintf(
		UNI_L("<tr>    <td>Registered Diskcache</td>    <td></td>    <td>%lu</td>    </tr>\r\n"),
			debugurl.registered_diskcache);
	url.WriteDocumentData(URL::KNormal,
		UNI_L("</table>\r\n"));
#ifdef _BYTEMOBILE
	url.WriteDocumentDataUniSprintf(UNI_L("<h9>Bytemobile optimized: %d</h9>\r\n"), urlManager->GetByteMobileOpt());
#endif

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
//    url.WriteDocumentDataUniSprintf(UNI_L("<h9>Disk cache size: %d</h9>\r\n"), urlManager->GetOCacheSize());
//    url.WriteDocumentDataUniSprintf(UNI_L("<h9>Disk cache used: %d</h9>\r\n"), urlManager->GetOCacheUsed());
#endif

	if (http_Manager)
		http_Manager->generateDebugDocListing(&url);



	return OpStatus::OK;
}
#endif // _OPERA_DEBUG_DOC_
#endif
#endif // !NO_URL_OPERA || HAS_OPERABLANK || EPOC
