/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef OPERA_AUTH_SUPPORT

#include "modules/util/str.h"
#include "modules/util/opstring.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/opera_auth/opera_auth_base.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/locale/oplanguagemanager.h"

OperaAuthentication::OperaAuthentication()
	: OpAuthTimeout(MSG_OPERA_AUTH_TIMEOUT)
	, m_listener(NULL)
{

}

OperaAuthentication::~OperaAuthentication()
{

}

OpAuthTimeout::~OpAuthTimeout()
{
	g_main_message_handler->UnsetCallBacks(this);
}

// uses a custom interval
OP_STATUS OpAuthTimeout::StartTimeout(UINT32 timeout, BOOL short_interval)
{
	if (!g_main_message_handler->HasCallBack(this, m_message, (MH_PARAM_1)short_interval))
	{
		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, m_message, (MH_PARAM_1)short_interval));
	}
	g_main_message_handler->RemoveDelayedMessage(m_message, (MH_PARAM_1)short_interval, 0);
	g_main_message_handler->PostDelayedMessage(m_message, (MH_PARAM_1)short_interval, 0, timeout*1000);

	return OpStatus::OK;
}

// uses the set time interval
OP_STATUS OpAuthTimeout::RestartTimeout()
{
	if (!g_main_message_handler->HasCallBack(this, m_message, 0))
	{
		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, m_message, 0));
	}
	g_main_message_handler->RemoveDelayedMessage(m_message, 0, 0);
	g_main_message_handler->PostDelayedMessage(m_message, 0, 0, m_timeout*1000);

	return OpStatus::OK;
}

void OpAuthTimeout::StopTimeout()
{
	if (g_main_message_handler->HasCallBack(this, m_message, 0))
	{
		g_main_message_handler->RemoveDelayedMessage(m_message, 0, 0);
		g_main_message_handler->UnsetCallBack(this, m_message);
	}
}

#endif // OPERA_AUTH_SUPPORT
