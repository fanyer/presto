/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifdef SEARCH_ENGINES

#ifndef MODULES_SEARCHMANAGER_SEARCHMANAGER_MODULE_H
#define MODULES_SEARCHMANAGER_SEARCHMANAGER_MODULE_H

#include "modules/hardcore/opera/module.h"

class SearchManager;

class SearchmanagerModule : public OperaModule
{
public:
	SearchmanagerModule();

	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();
	SearchManager *m_search_manager;
};

#define SEARCHMANAGER_MODULE_REQUIRED

#define g_search_manager g_opera->searchmanager_module.m_search_manager

#endif // MODULES_SEARCHMANAGER_SEARCHMANAGER_MOULE_H
#endif // SEARCH_ENGINES
