/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/hardcore/component/OpChannel.h"
#include "modules/hardcore/component/OpComponentManager.h"
#include "modules/hardcore/src/generated/g_message_hardcore_messages.h" // Need OpComponentManagerResponseMessage

OpChannel::OpChannel(const OpMessageAddress& destination, OpComponent* component /* = NULL */)
	: m_component(component ? component : g_component)
	, m_destination(destination)
	, m_state(UNINITIALIZED)
{
	OP_ASSERT(m_component || !"Channels must be created in a running component.");
}

/* virtual */
OpChannel::~OpChannel()
{
	if (IsInitialized())
		m_component->DeregisterChannel(this);
}

OP_STATUS
OpChannel::Construct(bool root /* = false */)
{
	OP_ASSERT(!IsInitialized());
	return m_component->RegisterChannel(this, root);
}

void
OpChannel::Direct(const OpMessageAddress& new_destination)
{
	OP_ASSERT(IsInitialized());

	if (IsDirected() && GetDestination() != new_destination)
		SetState(UNDIRECTED);

	m_destination = new_destination;
	SetState(DIRECTED);
}

OP_STATUS
OpChannel::Connect()
{
	OP_ASSERT(IsDirected() || !"Cannot connect to an undefined remote entity.");
	OP_ASSERT(GetDestination().ch > ROOT || !"Cannot connect to a non-channel.");
	OP_STATUS s = OpStatus::OK;
	if (!IsConnected())
	{
		/* Re-configure the destination address of the remote channel. */
		s = SendMessage(OpPeerConnectedMessage::Create());
		if (OpStatus::IsSuccess(s))
			SetState(CONNECTED);
	}
	return s;
}

OP_STATUS
OpChannel::SendMessage(OpTypedMessage* message, unsigned int flags /* = SENDMESSAGE_OOM_IF_NULL */)
{
	OP_ASSERT(IsInitialized());

	if (!message)
		return (flags & OpMessenger::SENDMESSAGE_OOM_IF_NULL) ?
			OpStatus::ERR_NO_MEMORY : OpStatus::ERR_NULL_POINTER;

	if (!IsDirected())
	{
		OP_DELETE(message);
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	}

	message->SetSrc(GetAddress());
	message->SetDst(GetDestination());

	return m_component->SendMessage(message, flags);
}

OP_STATUS
OpChannel::ProcessMessage(const OpTypedMessage* message)
{
	OP_STATUS ret;

	/* Calling OpMessenger::ProcessMessage() may result in this object's
	 * destruction, so push information we might need onto the stack. */
	bool is_root = IsRoot(), is_directed = IsDirected();

	switch (message->GetType())
	{
	case OpPeerConnectedMessage::Type:
		Direct(message->GetSrc());
		SetState(CONNECTED);
		// Alert listener(s) to our completion
		return OpMessenger::ProcessMessage(message);

	case OpPeerPendingMessage::Type:
		/* Update the destination address with the address of the remote
		 * OpComponentManager which will handle a corresponding
		 * OpComponentRequestMessage. Thus if the destination process dies
		 * (i.e., the remote OpComponentManager whose address we now store here)
		 * before requested component is created, then this channel will receive
		 * an OpPeerDisconnectedMessage.
		 *
		 * Note: It is possible that the requested component is created before
		 * the destination OpComponentManager was able to send this message. In
		 * that case we already received an OpPeerConnectedMessage and the
		 * channel is already connected and we can ignore this message. */
		if (!IsDirected())
			m_destination = message->GetSrc();

		// Listener(s) are not to be told.
		return OpStatus::OK;

	case OpPeerDisconnectedMessage::Type:
		/* Ignore disconnect messages from unknown entities. These will arrive
		 * when new channels are created with this channel as end-point, but are
		 * destroyed before the connection is upgraded on this end. */
		if (!(message->GetSrc() == GetDestination()))
			return OpStatus::OK;

		/* The remote end-point has vanished. We don't need to send an
		 * OpPeerDisconnectedMessage back to the remote point because it told
		 * us that it has vanished. */
		SetState(UNDIRECTED, false);
		// Alert listener(s) to the destination's no-more-ness.
		ret = OpMessenger::ProcessMessage(message);
		if (is_root)
			// The root channel controls the hosting component's lifetime.
			ret = m_component->OnRootChannelDisconnected();
		return ret;

	default:
		/* Forward message to channel listeners. If a listener returns an error,
		 * send this error to the caller, building a bridge for our traditional
		 * error handling. */
		ret = OpMessenger::ProcessMessage(message);
		if (is_directed && OpStatus::IsError(ret))
			return m_component->SendMessage(OpStatusMessage::Create(ret, message->GetDst(), message->GetSrc()));
		return ret;
	}
}

OP_STATUS
OpChannel::AddMessageListener(OpMessageListener* listener, bool silent)
{
	RETURN_IF_ERROR(OpMessenger::AddMessageListener(listener));

	/* If we are already connected to the peer, notify the listener immediately
	 * with an OpPeerConnectedMessage. */
	if (!silent && IsConnected())
	{
		if (OpPeerConnectedMessage *msg = OpPeerConnectedMessage::Create(m_destination, m_address, 0))
		{
			listener->ProcessMessage(msg);
			OP_DELETE(msg);
		}
		else
		{
			OpMessenger::RemoveMessageListener(listener);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

void
OpChannel::SetState(State new_state, bool send_disconnect /* = true */)
{
	OP_ASSERT(new_state == m_state + 1 || new_state <= UNDIRECTED);

	if (new_state <= UNDIRECTED)
	{
		/* Send OpPeerDisconnectedMessage to the remote end-point, if it is a connected channel. */
		if (IsConnected() && send_disconnect)
			OpStatus::Ignore(SendMessage(OpPeerDisconnectedMessage::Create()));

		m_destination = OpMessageAddress();
	}

	m_state = new_state;
}
