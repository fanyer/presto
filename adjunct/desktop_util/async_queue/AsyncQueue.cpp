/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/async_queue/AsyncQueue.h"
#include "adjunct/desktop_util/async_queue/AsyncCommand.h"

AsyncQueue::AsyncQueue(unsigned long delay)
  : m_message_handler(0, 0)
  , m_delay(delay)
  , m_callback_initialized(FALSE)
{
}


OP_STATUS AsyncQueue::AddCommand(AsyncCommand* command)
{
	if (!command)
		return OpStatus::ERR;

	command->Into(this);

	if (!m_callback_initialized)
		RETURN_IF_ERROR(SetupCallBack());

	if (!PostMessage(MSG_EXECUTE_ASYNC_COMMAND, 0, reinterpret_cast<MH_PARAM_2>(command), m_delay))
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}


OP_STATUS AsyncQueue::SetupCallBack()
{
	OP_STATUS ret = m_message_handler.SetCallBack(this, MSG_EXECUTE_ASYNC_COMMAND, 0);
	m_callback_initialized = OpStatus::IsSuccess(ret);

	return ret;
}


void AsyncQueue::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	AsyncCommand* command = reinterpret_cast<AsyncCommand*>(par2);

	command->Execute();

	OP_DELETE(command);
}
