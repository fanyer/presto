/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/


#ifndef MODULES_HISTORY_HISTORY_MODULE_H
#define MODULES_HISTORY_HISTORY_MODULE_H

#ifdef HISTORY_SUPPORT

#include "modules/hardcore/opera/module.h"

class OpHistoryModel;
class DirectHistory;

/**
 * @brief Module for url history, both typed and actual
 * @author Patricia Aas
 *
 * The history module stores the urls and access times
 * of pages visited. This information is used for 
 * autocompletion, Top Ten, history panel and history 
 * page.
 */
class HistoryModule : public OperaModule, public OpPrefsListener
{
public:

	HistoryModule();
	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	// == OpPrefsListener ======================

	virtual void PrefChanged(enum OpPrefsCollection::Collections id, int pref,
	                         int newvalue);

	// == Globals ==============================

	OpHistoryModel* m_history_model;
# ifdef DIRECT_HISTORY_SUPPORT
	DirectHistory* m_direct_history;
# endif // DIRECT_HISTORY_SUPPORT
};

# define g_globalHistory (g_opera->history_module.m_history_model)
# define globalHistory g_globalHistory
# ifdef DIRECT_HISTORY_SUPPORT
#  define g_directHistory (g_opera->history_module.m_direct_history)
#  define directHistory g_directHistory
# endif // DIRECT_HISTORY_SUPPORT

#define HISTORY_MODULE_REQUIRED

#endif // HISTORY_SUPPORT
#endif // !MODULES_HISTORY_HISTORY_MODULE_H
