/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef SEARCH_ENGINE_MODULE_H
#define SEARCH_ENGINE_MODULE_H

#ifdef VISITED_PAGES_SEARCH

#include "modules/hardcore/opera/module.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"

#ifdef VPS_WRAPPER
class AsyncVisitedSearch;
#else
class VisitedSearch;
#endif

class SearchEngineModule : public OperaModule, public OpPrefsListener
{
public:
	SearchEngineModule()
	{
		visited_search = NULL;
		empty_visited_search_result = NULL;
	}

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	virtual void PrefChanged(enum OpPrefsCollection::Collections id, int pref,
	                         int newvalue);

#ifdef VPS_WRAPPER
	AsyncVisitedSearch *visited_search;
#else
	VisitedSearch *visited_search;
#endif
	void* empty_visited_search_result;

protected:
	OP_STATUS OpenVisitedSearch();
	void CloseVisitedSearch(BOOL force_close);
};

#define g_visited_search g_opera->search_engine_module.visited_search

#define SEARCH_ENGINE_MODULE_REQUIRED

#endif // VISITED_PAGES_SEARCH


#define SEARCH_ENGINE_LOG_BLOCKSTORAGE   1
#define SEARCH_ENGINE_LOG_BTREE          2
#define SEARCH_ENGINE_LOG_ACT            4
#define SEARCH_ENGINE_LOG_STRINGTABLE    8
#define SEARCH_ENGINE_LOG_VISITEDSEARCH 16

//#define SEARCH_ENGINE_LOG SEARCH_ENGINE_LOG_STRINGTABLE


#endif // SEARCH_ENGINE_MODULE_H

