/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#ifdef SEARCH_ENGINES

#include "modules/searchmanager/searchmanager_module.h"
#include "modules/searchmanager/searchmanager.h"

SearchmanagerModule::SearchmanagerModule() :
	m_search_manager(NULL)
{
}

void SearchmanagerModule::InitL(const OperaInitInfo& info)
{
	m_search_manager = OP_NEW_L(SearchManager, ());
#ifndef PLATFORM_LOADS_SEARCHES
	m_search_manager->LoadL();
#endif // PLATFORM_LOADS_SEARCHES
}

void SearchmanagerModule::Destroy()
{
	OP_DELETE(m_search_manager);
	m_search_manager = NULL;
}

#endif // SEARCH_ENGINES
