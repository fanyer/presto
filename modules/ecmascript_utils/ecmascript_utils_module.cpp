/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/ecmascript_utils/esutils.h"

EcmascriptUtilsModule::EcmascriptUtilsModule()
	: scheduler_id(0),
	  timer_manager_id(0)
#ifdef ECMASCRIPT_DEBUGGER
	, engine_debug_backend(NULL)
#endif // ECMASCRIPT_DEBUGGER
#ifdef ECMASCRIPT_REMOTE_DEBUGGER
	, callback(NULL)
#else // ECMASCRIPT_REMOTE_DEBUGGER
#endif // ECMASCRIPT_REMOTE_DEBUGGER
#ifdef ESUTILS_SYNCIF_SUPPORT
	, sync_run_in_progress(NULL)
#endif // ESUTILS_SYNCIF_SUPPORT
{
}

void
EcmascriptUtilsModule::InitL(const OperaInitInfo& info)
{
	LEAVE_IF_ERROR(ES_Utils::Initialize());
}

void
EcmascriptUtilsModule::Destroy()
{
	ES_Utils::Destroy();
}

