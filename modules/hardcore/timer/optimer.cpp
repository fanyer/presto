/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/hardcore/timer/optimer.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/pi/OpSystemInfo.h"

OpTimer::OpTimer() :
	m_listener(NULL),
	m_firetime(op_nan(NULL))
{
}

OpTimer::~OpTimer()
{
	if (g_optimer_message_handler && !op_isnan(m_firetime))
		g_optimer_message_handler->UnsetCallBacks(this);
}

void
OpTimer::Start(UINT32 ms)
{
	Stop();

	if (g_optimer_message_handler)
	{
		if (op_isnan(m_firetime))
			g_optimer_message_handler->SetCallBack(this, MSG_OPTIMER, reinterpret_cast<MH_PARAM_1>(this));

		g_optimer_message_handler->PostMessage(MSG_OPTIMER, reinterpret_cast<MH_PARAM_1>(this), 0, ms);
	}

	OP_ASSERT(g_op_time_info);
	m_firetime = g_op_time_info->GetRuntimeMS() + ms;
}

UINT32
OpTimer::Stop()
{
	if (g_optimer_message_handler)
		g_optimer_message_handler->RemoveDelayedMessage(MSG_OPTIMER, reinterpret_cast<MH_PARAM_1>(this), 0);

	if (g_op_time_info)
	{
		INT32 time_left = static_cast<INT32>(m_firetime - g_op_time_info->GetRuntimeMS());
		return MAX(time_left, 0);
	}
	else
		return 0;
}

int
OpTimer::TimeSinceFiring() const
{
	if (op_isnan(m_firetime) || !g_op_time_info)
		return 0;

	return static_cast<int>(g_op_time_info->GetRuntimeMS() - m_firetime);
}

void
OpTimer::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	/* We don't need to call Stop() here; all it would do is try to remove the
	   message we just received, and calculate how much time was left, which we
	   don't care about. */
	m_listener->OnTimeOut(this);
}
