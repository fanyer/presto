/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/hardcore/component/OpMessenger.h"
#include "modules/hardcore/component/OpComponentManager.h"
#include "modules/hardcore/logging/OpFileMessageLogger.h"


OpMessenger::OpMessenger(const OpMessageAddress& address /* = OpMessageAddress() */)
	: m_address(address)
	, m_use_count(0)
	, m_delete(false)
{
}

/* virtual */
OpMessenger::~OpMessenger()
{
	OP_ASSERT(!m_use_count || !"Messenger deleted while on the stack.");
}

/* virtual */ OP_STATUS
OpMessenger::SendMessage(OpTypedMessage* message, unsigned int flags /* = SENDMESSAGE_OOM_IF_NULL */)
{
	return g_component_manager->SendMessage(message, flags);
}

OP_STATUS
OpMessenger::ReplyTo(const OpTypedMessage& message, OpTypedMessage* reply, unsigned int flags)
{
	if (!reply)
		return (flags & OpMessenger::SENDMESSAGE_OOM_IF_NULL) ?
			OpStatus::ERR_NO_MEMORY : OpStatus::ERR_NULL_POINTER;

	reply->SetSrc(message.GetDst());
	reply->SetDst(message.GetSrc());
	return SendMessage(reply, flags);
}

/* virtual */ OP_STATUS
OpMessenger::ProcessMessage(const OpTypedMessage* message)
{
	OP_STATUS ret = OpStatus::OK;

	/* The range iterator will increase the reference counts of the head node and the selected node,
	 * ensuring that our traversal will end safely even if this messenger (and consequently the
	 * list) is deleted by a listener. */
	for (OtlList<OpMessageListener*>::Range it = m_listeners.All(); it; ++it)
	{
#ifdef HC_MESSAGE_LOGGING
		if(GetMessageLogger())
			GetMessageLogger()->Log(message, UNI_L("Transferring to listener"));
#endif
		OP_STATUS s = (*it)->ProcessMessage(message);
		if (OpStatus::IsError(s))
			ret = s;
	}

	return ret;
}

/* virtual */ OP_STATUS
OpMessenger::AddMessageListener(OpMessageListener* listener)
{
	OP_ASSERT(listener);
	return m_listeners.Append(listener);
}

/* virtual */ OP_STATUS
OpMessenger::RemoveMessageListener(OpMessageListener* listener)
{
	return m_listeners.RemoveItem(listener) ? OpStatus::OK : OpStatus::ERR;
}

/* virtual */ void
OpMessenger::SafeDelete()
{
	m_delete = true;

	if (m_use_count == 0)
		OP_DELETE(this);
}

/* virtual */ int
OpMessenger::IncUseCount()
{
	return ++m_use_count;
}

/* virtual */ int
OpMessenger::DecUseCount()
{
	int ret = --m_use_count;
	if (ret == 0 && m_delete)
		OP_DELETE(this);

	return ret;
}

#ifdef HC_MESSAGE_LOGGING
/* virtual */ void
OpMessenger::SetMessageLogger(OpSharedPtr<OpMessageLogger> logger)
{
	m_message_logger = logger;
}

/* virtual */ OpMessageLogger*
OpMessenger::GetMessageLogger() const
{
	return m_message_logger.get();
}
#endif
