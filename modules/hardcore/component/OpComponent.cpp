/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/hardcore/component/OpComponent.h"
#include "modules/hardcore/component/Messages.h"

// Include auto-generated OpComponent::Create() method:
#include "modules/hardcore/component/OpComponentCreate.inc"

/* virtual */
OpComponent::~OpComponent()
{
	g_component_manager->Destroy(this);
	RemoveLocalChannels();
}

BOOL
OpComponent::AddressExists(const OpMessageAddress& address)
{
	if (!IsLocal(address))
		return FALSE;

	if (address.IsComponent())
		return TRUE;

	return m_channels.Count(address) > 0;
}

OpChannel*
OpComponent::CreateChannel(const OpMessageAddress& destination, OpMessageListener* listener)
{
	OpAutoPtr<OpChannel> channel(OP_NEW(OpChannel, (destination, this)));
	RETURN_VALUE_IF_NULL(channel.get(), NULL);
	RETURN_VALUE_IF_ERROR(channel->Construct(), NULL);
	if (listener)
		RETURN_VALUE_IF_ERROR(channel->AddMessageListener(listener), NULL);
	return channel.release();
}

OP_STATUS
OpComponent::RegisterChannel(OpChannel* channel, bool root /* = false */)
{
	/* Assign a local address. */
	OpMessageAddress address;

	if (root)
		address = OpMessageAddress(GetAddress().cm, GetAddress().co, OpChannel::ROOT);
	else
		address = AssignNextChannelAddress();

	OP_ASSERT(m_channels.Count(address) == 0);

	RETURN_IF_ERROR(m_channels.Insert(address, channel));
	channel->SetInitialAddress(address);

	/* Advance state. */
	OpMessageAddress destination = channel->GetDestination();
	channel->SetState(OpChannel::UNDIRECTED);

	if (destination.IsValid())
		channel->Direct(destination);

	if (channel->IsRoot())
		channel->SetState(OpChannel::CONNECTED);

	return OpStatus::OK;
}

OP_STATUS
OpComponent::DeregisterChannel(OpChannel* channel)
{
	if (m_channels.Count(channel->GetAddress()) == 0)
		return OpStatus::ERR;

	m_channels.Erase(channel->GetAddress());
	channel->SetState(OpChannel::UNINITIALIZED);

	return OpStatus::OK;
}

OpChannel*
OpComponent::GetChannel(const OpMessageAddress& address) const
{
	ChannelMap::ConstIterator it = m_channels.Find(address);
	if (it == m_channels.End())
		return NULL;
	return it->second;
}

OP_STATUS
OpComponent::RequestComponent(OpChannel** channel, OpComponentType type)
{
	CountMessengerUse in(this);

	*channel = NULL;

	OpAutoPtr<OpChannel> channel_(OP_NEW(OpChannel, (OpMessageAddress(), this)));
	RETURN_OOM_IF_NULL(channel_.get());
	RETURN_IF_ERROR(channel_->Construct());

	// Relay the request to the initial component manager, where
	// it will be transmitted to the initial platform.
	RETURN_IF_ERROR(SendMessage(OpComponentRequestMessage::Create(
									type,
									channel_->GetAddress(),
									OpMessageAddress(0, 0, 0))));

	// The channel is now expecting an OpPeerConnected message
	// from a newly created component of the desired type.
	*channel = channel_.release();
	return OpStatus::OK;
}

OP_STATUS
OpComponent::DispatchMessage(const OpTypedMessage *message)
{
	CountMessengerUse in(this);

	const OpMessageAddress& dst = message->GetDst();
	OpMessenger* destination = NULL;
	OP_ASSERT(message->IsValid());
	OP_ASSERT(IsLocal(dst));

	if (dst.IsComponent()) // Addressed directly to this component
		destination = this;
	else if (dst.IsChannel())
		destination = GetChannel(dst);
	else
		OP_ASSERT(!"Madness!");

	if (destination)
		return destination->ProcessMessage(message);

	// Destination disappeared between delivery phase and now.
	// Send OpPeerDisconnectedMessage back to sender.
	// Ignore if remote end has also disappeared.
	if (message->GetType() != OpPeerDisconnectedMessage::Type)
		OpStatus::Ignore(message->Reply(OpPeerDisconnectedMessage::Create()));

	return OpStatus::OK;
}

/* protected */

OpComponent::OpComponent(const OpMessageAddress& address, OpComponentType type)
	: OpMessenger(address)
	, m_type(type)
	, m_next_channel_number(OpChannel::FIRST)
	, m_channels()
{
}

/* virtual */ void
OpComponent::Destroy()
{
}

OpMessageAddress
OpComponent::AssignNextChannelAddress()
{
	int ch = m_next_channel_number++;
	OP_ASSERT(ch > 0);
	OP_ASSERT(m_next_channel_number > ch); // Detect overflow.
	return OpMessageAddress(GetAddress().cm, GetAddress().co, ch);
}

OP_STATUS
OpComponent::DisconnectMessenger(const OpMessageAddress& address)
{
	CountMessengerUse in(this);

	for (ChannelMap::Iterator i = m_channels.Begin(); i != m_channels.End(); ++i)
		if (i->second->GetDestination().cm == address.cm &&
			(address.co == 0 || i->second->GetDestination().co == address.co))
			SendMessage(OpPeerDisconnectedMessage::Create(i->second->GetDestination(), i->second->GetAddress()));

	return OpStatus::OK;
}

OP_STATUS
OpComponent::OnRootChannelDisconnected()
{
	SafeDelete();
	return OpStatus::OK;
}

void
OpComponent::RemoveLocalChannels()
{
	for (ChannelMap::Iterator it = m_channels.Begin(); it != m_channels.End();)
	{
		OpMessageAddress address = it->first;
		OpChannel* channel = it->second;
		if (IsLocal(address))
		{
			m_channels.Erase(it++);
			OP_DELETE(channel);
		}
		else
			++it;
	}
}

OpComponentActivator::OpComponentActivator()
	: m_active(false)
{
}

OpComponentActivator::~OpComponentActivator()
{
	if (m_active)
		g_component_manager->PopRunningComponent();
}

OP_STATUS
OpComponentActivator::Activate(OpComponent* component)
{
	RETURN_IF_ERROR(g_component_manager->PushRunningComponent(component));
	m_active = true;

	return OpStatus::OK;
}
