/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ES_UTILS_ECMASCRIPT_UTILS_MODULE_H
#define ES_UTILS_ECMASCRIPT_UTILS_MODULE_H

#include "modules/hardcore/opera/module.h"

#ifdef ECMASCRIPT_DEBUGGER
class ES_EngineDebugBackend;
#endif // ECMASCRIPT_DEBUGGER

#ifdef ECMASCRIPT_REMOTE_DEBUGGER
class ES_Utils;
class ES_UtilsCallback;
#endif // ECMASCRIPT_REMOTE_DEBUGGER

class ES_ThreadSchedulerImpl;
class ES_SyncRunInProgress;

class EcmascriptUtilsModule
	: public OperaModule
{
protected:
	friend class ES_ThreadSchedulerImpl;
	unsigned scheduler_id;

	friend class ES_TimerManager;
	unsigned timer_manager_id;

#ifdef ECMASCRIPT_DEBUGGER
	friend class ES_EngineDebugBackend;
	ES_EngineDebugBackend *engine_debug_backend;
#endif // ECMASCRIPT_DEBUGGER

#ifdef ECMASCRIPT_REMOTE_DEBUGGER
	friend class ES_Utils;
	ES_UtilsCallback *callback;
#endif // ECMASCRIPT_REMOTE_DEBUGGER

#ifdef ESUTILS_SYNCIF_SUPPORT
	friend class ES_SyncRunInProgress;
	ES_SyncRunInProgress *sync_run_in_progress;
#endif // ESUTILS_SYNCIF_SUPPORT

public:
	EcmascriptUtilsModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();
};

#define ECMASCRIPT_UTILS_MODULE_REQUIRED

#endif // ES_UTILS_ECMASCRIPT_UTILS_MODULE_H
