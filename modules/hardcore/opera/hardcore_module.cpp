/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/hardcore/hardcore_module.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/base/periodic_task.h"
#include "modules/hardcore/hardcore_tweaks.h"
#ifdef CPUUSAGETRACKING
#include "modules/hardcore/cpuusagetracker/cpuusagetracker.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetrackers.h"
#endif // CPUUSAGETRACKING

extern void UnInitializeOutOfMemoryHandling();

HardcoreModule::HardcoreModule() :
	memory_manager(NULL),
	message_handler(NULL),
	optimer_handler(NULL),
	periodic_task_manager(NULL),
	oom_condition(0)
    ,handle_oom_prevtime(0)
	,handle_ood_prevtime(0)
#ifdef OUT_OF_MEMORY_POLLING
    ,flush_oom_prevtime(0)
	,oom_stopping_loading(FALSE)
    ,oom_rainyday_fund(NULL)
    ,oom_rainyday_bytes(0)
	,periodicTask(NULL)
#endif // OUT_OF_MEMORY_POLLING
#ifdef CPUUSAGETRACKING
	, next_cputracker_id(0)
	, default_fallback_cputracker(NULL)
	, active_cputracker(NULL)
	, cputrackers(NULL)
#endif // CPUUSAGETRACKING
{
}

void
HardcoreModule::InitL(const OperaInitInfo& info)
{
	memory_manager =  OP_NEW_L(MemoryManager,());
	LEAVE_IF_ERROR(memory_manager->Init());

#ifdef CPUUSAGETRACKING
	cputrackers = OP_NEW_L(CPUUsageTrackers, ());
	default_fallback_cputracker = OP_NEW_L(CPUUsageTracker, ());
#endif // CPUUSAGETRACKING

	message_handler = OP_NEW_L(MessageHandler,(NULL));
	optimer_handler =  OP_NEW_L(MessageHandler,(NULL));

	periodic_task_manager =  OP_NEW_L(PeriodicTaskManager,());

	op_setlocale(LC_CTYPE, "");

	// Set locale to system default for strcoll and strftime
	// Allows UI to sort elements without resetting locale all the time
	op_setlocale(LC_COLLATE, "");
	op_setlocale(LC_TIME, "");
}

void
HardcoreModule::Destroy()
{
	OP_DELETE(message_handler);
	message_handler = NULL;

	OP_DELETE(optimer_handler);
	optimer_handler = NULL;

	UnInitializeOutOfMemoryHandling();

	OP_DELETE(periodic_task_manager);
	periodic_task_manager = NULL;

#ifdef CPUUSAGETRACKING
	OP_DELETE(default_fallback_cputracker);
	default_fallback_cputracker = NULL;
	OP_DELETE(cputrackers);
	cputrackers = NULL;
#endif // CPUUSAGETRACKING

	OP_DELETE(memory_manager);
	memory_manager = NULL;
}
