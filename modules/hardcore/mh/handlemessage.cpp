/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/hardcore/mh/mh.h"
#include "modules/url/url_man.h"
#include "modules/dochand/win.h"
#include "modules/inputmanager/inputmanager.h"

#ifdef DEBUG_LOAD_STATUS
#include "modules/olddebug/tstdump.h"
#include "modules/pi/OpSystemInfo.h"
#endif // DEBUG_LOAD_STATUS

extern void HandleOutOfMemorySituation();	                // in modules/hardcore/mem/oom.cpp
extern void SoftOutOfMemorySituation();                     // ditto
extern void HandleOutOfDiskSituation();                     // ditto
#ifdef OUT_OF_MEMORY_POLLING
extern void RefillOutOfMemoryRainyDayFund();				// ditto
#endif // OUT_OF_MEMORY_POLLING

//////////////////
// int g_oom_condition
//
// bit 7: 1 if we currently are processing an oom-event (to avoid recursion)
// bit 2: 1 if out of disk
// bit 1: 1 if the flag is set;
// bit 0: 1 if a hard error

void
MessageHandler::PostOOMCondition( BOOL must_abort )
{
	g_oom_condition |= (2 | (must_abort ? 1 : 0));
}

void
MessageHandler::PostOODCondition()
{
	g_oom_condition |= 4;
}

void MessageHandler::HandleErrors()
{
	// OOM cleanup has the very highest priority in all message handlers.
	// For now, the condition and cleanup actions are themselves global.
	if ((g_oom_condition != 0) &&
		!(g_oom_condition & 0x80)) // not already processing? (avoid recursion)
	{
		int c = g_oom_condition;
		g_oom_condition = 0x80;
		if (c & 4)
			HandleOutOfDiskSituation();
		if (c & 1)
			HandleOutOfMemorySituation();
		else if (c & 2)
			SoftOutOfMemorySituation();
		g_oom_condition = 0;
	}

}

void MessageHandler::HandleMessage(OpMessage msg, MH_PARAM_1 wParam, MH_PARAM_2 lParam)
{
	HandleErrors();

#ifdef OUT_OF_MEMORY_POLLING
	if (this == g_main_message_handler && msg == MSG_OOM_RAINYDAY_REFILL)
		RefillOutOfMemoryRainyDayFund();
#endif // OUT_OF_MEMORY_POLLING

	OpStatus::Ignore(HandleCallback(msg, wParam, lParam));//Don't call g_memory_manager->RaiseCondition() here. It causes loops if OOM.
}
