/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 *
 */

#include <core/pch.h>

#ifdef PI_MESSAGING

#include "modules/pi/device_api/OpMessaging.h"

OP_STATUS
OpMessaging::Message::CopyTo(OpMessaging::Message** new_message)
{
	OP_ASSERT(new_message);
	OpMessaging::Message* message = OP_NEW(OpMessaging::Message, ());
	RETURN_OOM_IF_NULL(message);
	OpAutoPtr<OpMessaging::Message> ap_message(message);

	for (unsigned int i_len = 0; i_len < m_attachment.GetCount(); i_len++)
	{
		OpMessaging::Attachment* copy;
		RETURN_IF_ERROR(m_attachment.Get(i_len)->CopyTo(&copy));
		OpAutoPtr<OpMessaging::Attachment> ap_copy(copy);
		RETURN_IF_ERROR(message->m_attachment.Add(copy));
		ap_copy.release();
	}
	for (unsigned int i_len = 0; i_len < m_cc_address.GetCount(); i_len++)
	{
		OpString* copy = OP_NEW(OpString, ());
		RETURN_OOM_IF_NULL(copy);
		OpAutoPtr<OpString> ap_copy(copy);
		RETURN_IF_ERROR(copy->Set(*m_cc_address.Get(i_len)));
		RETURN_IF_ERROR(message->m_cc_address.Add(copy));
		ap_copy.release();
	}
	for (unsigned int i_len = 0; i_len < m_bcc_address.GetCount(); i_len++)
	{
		OpString* copy = OP_NEW(OpString, ());
		RETURN_OOM_IF_NULL(copy);
		OpAutoPtr<OpString> ap_copy(copy);
		RETURN_IF_ERROR(copy->Set(*m_bcc_address.Get(i_len)));
		RETURN_IF_ERROR(message->m_bcc_address.Add(copy));
		ap_copy.release();
	}
	for (unsigned int i_len = 0; i_len < m_destination_address.GetCount(); i_len++)
	{
		OpString* copy = OP_NEW(OpString, ());
		RETURN_OOM_IF_NULL(copy);
		OpAutoPtr<OpString> ap_copy(copy);
		RETURN_IF_ERROR(copy->Set(*m_destination_address.Get(i_len)));
		RETURN_IF_ERROR(message->m_destination_address.Add(copy));
		ap_copy.release();
	}
	for (unsigned int i_len = 0; i_len < m_reply_to_address.GetCount(); i_len++)
	{
		OpString* copy = OP_NEW(OpString, ());
		RETURN_OOM_IF_NULL(copy);
		OpAutoPtr<OpString> ap_copy(copy);
		RETURN_IF_ERROR(copy->Set(*m_reply_to_address.Get(i_len)));
		RETURN_IF_ERROR(message->m_reply_to_address.Add(copy));
		ap_copy.release();
	}
	RETURN_IF_ERROR(message->m_sender_address.Set(m_sender_address));
	RETURN_IF_ERROR(message->m_subject.Set(m_subject));
	RETURN_IF_ERROR(message->m_body.Set(m_body));
	RETURN_IF_ERROR(message->m_id.Set(m_id));
	message->m_is_read = m_is_read;
	message->m_priority = m_priority;
	message->m_sending_timeout = m_sending_timeout;
	message->m_sending_time = m_sending_time;
	message->m_type = m_type;

	ap_message.release();
	*new_message = message;
	return OpStatus::OK;
}

#endif // PI_MESSAGING

