/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Peter Krefting
 */
#include "core/pch.h"
#include "modules/prefs/init/delayed_init.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/hardcore/mh/mh.h"

#ifdef PREFS_DELAYED_INIT_NEEDED

PrefsDelayedInit::PrefsDelayedInit()
{
}

PrefsDelayedInit::~PrefsDelayedInit()
{
	g_main_message_handler->UnsetCallBacks(this);
}

void PrefsDelayedInit::StartL()
{
	LEAVE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_PREFS_RECONFIGURE, 0));
	g_main_message_handler->PostMessage(MSG_PREFS_RECONFIGURE, 0, 0);
}

void PrefsDelayedInit::HandleCallback(OpMessage msg, MH_PARAM_1, MH_PARAM_2)
{
	// Opera is now initialized, and we can do whatever delayed configuration
	// we needed to make.

	if (msg == MSG_PREFS_RECONFIGURE)
	{
#ifdef PREFS_VALIDATE
		TRAPD(rc2, g_pccore->DelayedInitL());
		RAISE_IF_ERROR(rc2);
#endif // PREFS_VALIDATE

		// This object is not needed any longer.
		OP_DELETE(this);
		g_opera->prefs_module.m_delayed_init = NULL;
	}
}

#endif // PREFS_DELAYED_INIT_NEEDED
