/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_HARDCORE_MODULE_H
#define MODULES_HARDCORE_HARDCORE_MODULE_H

#include "modules/hardcore/opera/module.h"
#include "modules/hardcore/mem/oom.h"

class MemoryManager;
class MessageHandler;
class OperaMessageHandler;
class DelayedMessageControl;
class GlobalMessageDispatcher;
class PeriodicTaskManager;
#ifdef CPUUSAGETRACKING
class CPUUsageTracker;
class CPUUsageTrackActivator;
class CPUUsageTrackers;
#endif // CPUUSAGETRACKING

class HardcoreModule : public OperaModule
{
public:
	HardcoreModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	MemoryManager* memory_manager;
	MessageHandler* message_handler;
	MessageHandler* optimer_handler;
	PeriodicTaskManager* periodic_task_manager;

	int oom_condition;

	time_t handle_oom_prevtime;
	time_t handle_ood_prevtime;
#ifdef OUT_OF_MEMORY_POLLING
	time_t flush_oom_prevtime;
	BOOL oom_stopping_loading;
	rainyday_t* oom_rainyday_fund;
	size_t oom_rainyday_bytes;
	OOMPeriodicTask* periodicTask;
#endif //OUT_OF_MEMORY_POLLING
#ifdef CPUUSAGETRACKING
	unsigned next_cputracker_id;
	CPUUsageTracker* default_fallback_cputracker;
	CPUUsageTrackActivator* active_cputracker;
	CPUUsageTrackers* cputrackers;
#endif // CPUUSAGETRACKING
};

#define g_memory_manager g_opera->hardcore_module.memory_manager
#define g_main_message_handler g_opera->hardcore_module.message_handler
#define g_optimer_message_handler g_opera->hardcore_module.optimer_handler
#define g_message_dispatcher g_component_manager
#define g_oom_condition g_opera->hardcore_module.oom_condition
#define g_periodic_task_manager g_opera->hardcore_module.periodic_task_manager

#define g_handle_oom_prevtime g_opera->hardcore_module.handle_oom_prevtime
#define g_handle_ood_prevtime g_opera->hardcore_module.handle_ood_prevtime
#ifdef OUT_OF_MEMORY_POLLING
#define g_flush_oom_prevtime g_opera->hardcore_module.flush_oom_prevtime
#define g_oom_stopping_loading g_opera->hardcore_module.oom_stopping_loading
#define g_oom_rainyday_fund g_opera->hardcore_module.oom_rainyday_fund
#define g_oom_rainyday_bytes g_opera->hardcore_module.oom_rainyday_bytes
#define g_PeriodicTask g_opera->hardcore_module.periodicTask
#endif //OUT_OF_MEMORY_POLLING
#ifdef CPUUSAGETRACKING
# define g_next_cputracker_id g_opera->hardcore_module.next_cputracker_id
# define g_fallback_cputracker g_opera->hardcore_module.default_fallback_cputracker
# define g_active_cputracker g_opera->hardcore_module.active_cputracker
# define g_cputrackers g_opera->hardcore_module.cputrackers
#endif // CPUUSAGETRACKING


#define HARDCORE_MODULE_REQUIRED

#endif // !MODULES_HARDCORE_HARDCORE_MODULE_H
