/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef VISITED_PAGES_SEARCH

#include "modules/hardcore/opera/module.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/search_engine/VisitedSearch.h"

void SearchEngineModule::InitL(const OperaInitInfo& info)
{
#ifndef HISTORY_SUPPORT
	g_pcnet->RegisterListenerL(this);
#endif
	g_pccore->RegisterListenerL(this);

	LEAVE_IF_ERROR(OpenVisitedSearch());
	
	empty_visited_search_result = OP_NEW_L(VisitedSearch::Result, ());
	((VisitedSearch::Result*)empty_visited_search_result)->url = const_cast<char*>("");
}

void SearchEngineModule::Destroy()
{
#ifndef HISTORY_SUPPORT
	if (g_pcnet)
		g_pcnet->UnregisterListener(this);
#endif
	if (g_pccore)
		g_pccore->UnregisterListener(this);

	CloseVisitedSearch(TRUE);

	OP_DELETE((VisitedSearch::Result*)empty_visited_search_result);
}

void SearchEngineModule::PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (id == OpPrefsCollection::Core && pref == PrefsCollectionCore::VisitedPages)
		OpStatus::Ignore(OpenVisitedSearch());

#ifndef HISTORY_SUPPORT
	if (id == OpPrefsCollection::Network && pref == PrefsCollectionNetwork::VSMaxIndexSize)
		OpStatus::Ignore(OpenVisitedSearch());
#else  // HISTORY_SUPPORT
	if (id == OpPrefsCollection::Core && pref == PrefsCollectionCore::MaxGlobalHistory)
		OpStatus::Ignore(OpenVisitedSearch());
#endif  // HISTORY_SUPPORT
}

OP_STATUS SearchEngineModule::OpenVisitedSearch()
{
	OpString directory;
	int max_size;

#ifdef UTIL_VPS_FOLDER
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_VPS_FOLDER, directory));
#else
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, directory));
	RETURN_IF_ERROR(directory.AppendFormat(UNI_L("%c%s"), PATHSEPCHAR, UNI_L("vps")));
#endif

#ifndef HISTORY_SUPPORT
	max_size = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::VSMaxIndexSize);
#else
	max_size = g_pccore->GetIntegerPref(PrefsCollectionCore::MaxGlobalHistory);
#endif

	// See if we are enabled or disabled
	if (!g_pccore->GetIntegerPref(PrefsCollectionCore::VisitedPages))
		max_size = 0;

	if (visited_search == NULL)
#ifdef VPS_WRAPPER
		visited_search = AsyncVisitedSearch::Create();
#else
		visited_search = OP_NEW(VisitedSearch, ());
#endif

	RETURN_OOM_IF_NULL(visited_search);

	if (max_size == 0)
	{
		if (visited_search->IsOpen())
			RETURN_IF_ERROR(visited_search->Clear(FALSE));
		else
			OpStatus::Ignore(BlockStorage::DeleteFile(directory.CStr()));
	}
	else
	{
		if (!visited_search->IsOpen())
			OpStatus::Ignore(visited_search->Open(directory.CStr()));  // on error, let's rather start Opera anyway without this service running
	}

#ifndef HISTORY_SUPPORT
	visited_search->SetMaxSize(max_size);
#else
	visited_search->SetMaxItems(max_size);
#endif

	return OpStatus::OK;
}

void SearchEngineModule::CloseVisitedSearch(BOOL force_close)
{
	if (visited_search == NULL)
		return;

	if (visited_search->IsOpen() && OpStatus::IsError(visited_search->Close(force_close)) && !force_close)
		return;

	OP_DELETE(visited_search);
	visited_search = NULL;
}


#endif // VISITED_PAGES_SEARCH

