/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/store/mboxnone.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/store/storemessage.h"


/***********************************************************************************
 ** Constructor
 **
 ** MboxNone::MboxNone
 ***********************************************************************************/
MboxNone::MboxNone()
  : m_next(0)
{
}


/***********************************************************************************
 ** Add a new message to the store
 **
 ** MboxNone::AddMessage
 ***********************************************************************************/
OP_STATUS MboxNone::AddMessage(StoreMessage& message, INT64& mbx_data)
{
	unsigned index = m_next;

	// Check if we already have this message in cache
	for (unsigned i = 0; i < MaxCachedMessages; i++)
	{
		if (m_cached_messages[i].m2_id == message.GetId())
			index = i;
	}

	message_gid_t old_id = m_cached_messages[index].m2_id;

	// Reset cached message, we'll change it now
	m_cached_messages[index].m2_id = 0;

	// Get new cached message
	RETURN_IF_ERROR(message.GetRawMessage(m_cached_messages[index].message));
	m_cached_messages[index].m2_id = message.GetId();

	if (index == m_next)
		m_next = (m_next + 1) % MaxCachedMessages;

	// Let M2 know that previous message might have changed
	if (old_id != message.GetId() && MessageEngine::GetInstance())
		MessageEngine::GetInstance()->OnMessageChanged(old_id);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Get a message from the store
 **
 ** MboxNone::GetMessage
 ***********************************************************************************/
OP_STATUS MboxNone::GetMessage(INT64 mbx_data, StoreMessage& message, BOOL override)
{
	for (unsigned i = 0; i < MaxCachedMessages; i++)
	{
		if (m_cached_messages[i].m2_id == message.GetId())
			return message.SetRawMessage(m_cached_messages[i].message.CStr());
	}

	return OpStatus::ERR_FILE_NOT_FOUND;
}


/***********************************************************************************
 ** Whether a message exists in this store
 **
 ** MboxNone::IsCached
 ***********************************************************************************/
BOOL MboxNone::IsCached(message_gid_t id)
{
	for (unsigned i = 0; i < MaxCachedMessages; i++)
	{
		if (m_cached_messages[i].m2_id == id)
			return TRUE;
	}

	return FALSE;
}


/***********************************************************************************
 ** Remove a message from the store
 **
 ** MboxNone::RemoveMessage
 ***********************************************************************************/
OP_STATUS MboxNone::RemoveMessage(INT64 mbx_data, message_gid_t id, UINT16 account_id, time_t sent_date, BOOL draft)
{
	for (unsigned i = 0; i < MaxCachedMessages; i++)
	{
		if (m_cached_messages[i].m2_id == id)
		{
			m_cached_messages[i].message.Empty();
			m_cached_messages[i].m2_id = 0;
			break;
		}
	}

	return OpStatus::OK;
}


#endif // M2_SUPPORT
