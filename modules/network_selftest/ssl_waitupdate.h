/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */
#ifndef _SSL_WAIT_UPDATE_H
#define _SSL_WAIT_UPDATE_H

#if defined(SELFTEST) && defined LIBSSL_AUTO_UPDATE

#include "modules/libssl/updaters.h" 

class SSL_WaitForAutoUpdate : public MessageObject
{
private:
	BOOL active;

public:
	SSL_WaitForAutoUpdate()
	{
		active = FALSE;
	}

	void Activate()
	{
		active = TRUE;
		g_main_message_handler->PostMessage(MSG_SSL_START_AUTO_UPDATE, 0, 0);
		g_main_message_handler->PostDelayedMessage(MSG_SSL_FINISHED_AUTOUPDATE_BATCH, 0, 1000, 60*1000);
		if(OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_SSL_FINISHED_AUTOUPDATE_BATCH, 0)))
		{
			ST_failed("Wait for update init failed");
			active = FALSE;
		}
	}

	~SSL_WaitForAutoUpdate()
	{
		g_main_message_handler->RemoveDelayedMessage(MSG_SSL_FINISHED_AUTOUPDATE_BATCH,0,1000);
		g_main_message_handler->UnsetCallBacks(this);
	}

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		if(active && msg == MSG_SSL_FINISHED_AUTOUPDATE_BATCH )
		{
			if(par2 == 1000)
				ST_failed(" Timed out ");
			else
				ST_passed();
			active = FALSE;
		}
	}
};

class SSL_WaitForPendingAutoUpdate : public MessageObject
{
private:
	BOOL active;

public:
	SSL_WaitForPendingAutoUpdate()
	{
		active = FALSE;
	}

	void Activate()
	{
		if(!g_ssl_auto_updaters->Active())
		{
			ST_passed();
			return;
		}

		active = TRUE;
		g_main_message_handler->PostDelayedMessage(MSG_SSL_FINISHED_AUTOUPDATE_BATCH, 0, 1000, 60*1000);
		if(OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_SSL_FINISHED_AUTOUPDATE_BATCH, 0)))
		{
			ST_failed("Wait for update init failed");
			active = FALSE;
		}
	}

	~SSL_WaitForPendingAutoUpdate()
	{
		g_main_message_handler->RemoveDelayedMessage(MSG_SSL_FINISHED_AUTOUPDATE_BATCH,0,1000);
		g_main_message_handler->UnsetCallBacks(this);
	}

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		if(active && msg == MSG_SSL_FINISHED_AUTOUPDATE_BATCH )
		{
			if(par2 == 1000)
				ST_failed(" Timed out ");
			else
				ST_passed();
			active = FALSE;
		}
	}
};

#endif // SELFTEST && LIBSSL_AUTO_UPDATE
#endif // _SSL_WAIT_UPDATE_H
