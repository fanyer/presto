/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef JABBER_SUPPORT

#include "jabber-backend.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"

// ***************************************************************************
//
//	JabberBackend
//
// ***************************************************************************

JabberBackend::JabberBackend()
  : m_client(NULL),
	m_current_chat_status(Account::OFFLINE)
{
}


JabberBackend::~JabberBackend()
{
	OP_DELETE(m_client);
}


/**
 * Step 3 in the backend loading process.
 * All initial memory allocations should occur here. OOM-safety is mandatory.
 * Called from Account::InitBackends().
 */

OP_STATUS JabberBackend::Init(Account* account)
{
	if (!account)
		return OpStatus::ERR_NULL_POINTER;

	m_account = account; // MessageBackend::m_account

	m_client = OP_NEW(JabberClient, (this));
	if (!m_client)
		return OpStatus::ERR_NO_MEMORY;

	return m_client->Init();
}


void JabberBackend::GetPresenceAndStatusMessage(Account::ChatStatus status, OpString8 &presence, OpString8 &status_message)
{
	switch (status)
	{
		case Account::ONLINE:
		{
			presence.Set("online");
			status_message.Set("I'm online");
			break;
		}
		case Account::BE_RIGHT_BACK:
		{
			presence.Set("away");
			status_message.Set("I'll be right back");
			break;
		}
		case Account::OUT_TO_LUNCH:
		{
			presence.Set("xa");
			status_message.Set("I'm out to lunch");
			break;
		}
		case Account::AWAY:
		{
			presence.Set("xa");
			status_message.Set("I'm away");
			break;
		}
		case Account::ON_THE_PHONE:
		{
			presence.Set("dnd");
			status_message.Set("I'm on the phone");
			break;
		}
		case Account::BUSY:
		{
			presence.Set("dnd");
			status_message.Set("I'm busy");
			break;
		}
		case Account::OFFLINE:
		{
			presence.Set("offline");
			status_message.Empty();
			break;
		}
		default:
		{
			presence.Empty();
			status_message.Empty();
			break;
		}
	}
}


OP_STATUS JabberBackend::Connect()
{
	if (!m_client)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_client->Connect());

	m_current_chat_status = Account::ONLINE; // m_client sets the presence to ONLINE by default.
	SignalStatusChanged();

	return OpStatus::OK;
}


OP_STATUS JabberBackend::Disconnect()
{
	if (!m_client)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(m_client->Disconnect());

	m_current_chat_status = Account::OFFLINE;
	SignalStatusChanged();

	return OpStatus::OK;
}


OP_STATUS JabberBackend::SendChatMessage(MessageEngine::ChatMessageType type, const OpStringC& message, const ChatInfo& room, const OpStringC& chatter)
{
	if (!m_client)
		return OpStatus::ERR_NULL_POINTER;

	HotlistModel *model = g_hotlist_manager->GetContactsModel();
	for (int i = 0; i < model->GetItemCount(); i++)
	{
		if (model->GetItemByIndex(i)->GetName() == chatter)
			return m_client->SendMessage(message, chatter, model->GetItemByIndex(i)->GetShortName());
	}

	return OpStatus::ERR; // didn't find the intended recipient.
}


BOOL JabberBackend::IsChatStatusAvailable(Account::ChatStatus chat_status)
{
	BOOL available = FALSE;

	switch (chat_status)
	{
		case Account::ONLINE :
		case Account::BUSY :
		case Account::BE_RIGHT_BACK :
		case Account::AWAY :
		case Account::ON_THE_PHONE :
		case Account::OUT_TO_LUNCH :
		case Account::OFFLINE :
		{
			available = TRUE;
			break;
		}
	}

	return available;
}


OP_STATUS JabberBackend::SetChatStatus(Account::ChatStatus chat_status)
{
	if (m_current_chat_status != chat_status)
	{
		if (m_current_chat_status == Account::OFFLINE)
		{
			RETURN_IF_ERROR(Connect());
			m_current_chat_status = chat_status;
		}
		else
		{
			if (chat_status == Account::OFFLINE)
			{
				m_current_chat_status = chat_status;
				RETURN_IF_ERROR(Disconnect());
			}
			else
			{
				OpString8 presence;
				OpString8 status_message;
				GetPresenceAndStatusMessage(chat_status, presence, status_message);
				m_client->AnnouncePresence(presence, status_message);

				m_current_chat_status = chat_status;
				SignalStatusChanged();
			}
		}
	}

	return OpStatus::OK;
}


OP_STATUS JabberBackend::GetProgress(Account::ProgressInfo& progress) const
{
	if (!m_client)
		return OpStatus::ERR_NULL_POINTER;

	return m_client->GetProgress(progress);
}


OP_STATUS JabberBackend::SubscribeToPresence(const OpStringC& jabber_id, BOOL subscribe)
{
	if (!m_client)
		return OpStatus::ERR_NULL_POINTER;

	return m_client->SubscribeToPresence(jabber_id, subscribe);
}


OP_STATUS JabberBackend::AllowPresenceSubscription(const OpStringC& jabber_id, BOOL allow)
{
	if (!m_client)
		return OpStatus::ERR_NULL_POINTER;

	return m_client->AllowPresenceSubscription(jabber_id, allow);
}

#endif // JABBER_SUPPORT
