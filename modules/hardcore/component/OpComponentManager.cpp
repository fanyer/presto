/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/hardcore/component/OpComponentManager.h"
#include "modules/hardcore/opera/components.h"
#include "modules/hardcore/logging/OpMessageLogger.h"

// Brings in all the descriptors objects needed by the protobuf system
#include "modules/protobuf/src/generated/g_protobuf_descriptors.h"

#include "modules/hardcore/src/generated/g_message_hardcore_messages.h"

/* static */ OP_STATUS
OpComponentManager::Create(OpComponentManager** manager, int address_cm /* = 0 */,
						   OpComponentPlatform* platform /* = 0 */)
{
	OpAutoPtr<OpComponentManager> cm(OP_NEW(OpComponentManager, (address_cm, platform)));
	RETURN_OOM_IF_NULL(cm.get());

	RETURN_IF_ERROR(cm->Construct());
	*manager = cm.release();

	return OpStatus::OK;
}

OpComponentManager::OpComponentManager(int address_cm, OpComponentPlatform* platform)
		: OpMessenger(OpMessageAddress(address_cm, 0, 0))
		, m_platform(platform)
		, m_inbox()
		, m_first_delayed_message(NULL)
		, m_first_unblocked_message(NULL)
		, m_next_component_number(1)
		, m_components()
		, m_component_stack(NULL)
		, m_protobuf_descriptors(NULL)
		, m_connected(false)
		, m_cleanup_stack(NULL)
{
}

OpComponentManager::~OpComponentManager()
{
	OpStatus::Ignore(Destroy());

	OP_DELETE(m_op_time_info);
}

OP_STATUS
OpComponentManager::Construct()
{
	OpAutoPtr<OpProtobufDescriptorSet> protobuf_descriptors(OP_NEW(OpProtobufDescriptorSet, ()));
	RETURN_OOM_IF_NULL(protobuf_descriptors.get());

	RETURN_IF_ERROR(protobuf_descriptors->Construct());
	if (!protobuf_descriptors->IsValid())
		return OpStatus::ERR;

	m_protobuf_descriptors = protobuf_descriptors.release();

	return OpTimeInfo::Create(&m_op_time_info);
}

OP_STATUS
OpComponentManager::Destroy()
{
	OP_ASSERT(m_component_stack == NULL);

	// Delete all components.
	OpHashIterator* it = m_components.GetIterator();
	RETURN_OOM_IF_NULL(it);
	while (OpStatus::IsSuccess(it->First()))
		OP_DELETE(static_cast<OpComponent*>(it->GetData()));

	OP_DELETE(it);
	OP_ASSERT(m_components.GetCount() == 0);

	// Burn all undelivered mail.
	while (OpTypedMessage* p = m_inbox.First())
	{
		p->Out();
		OP_DELETE(p);
	}

	// Delete stored values.
	for (ValueMap::Iterator i = m_values.Begin(); i != m_values.End(); i++)
		OP_DELETE(i->second);
	m_values.Clear();

	OP_DELETE(m_protobuf_descriptors);

	return OpStatus::OK;
}

OP_STATUS
OpComponentManager::PushRunningComponent(OpComponent* component)
{
	struct RunningComponentStack* top = OP_NEW(struct RunningComponentStack, ());
	RETURN_OOM_IF_NULL(top);

	top->component = component;
	top->next = m_component_stack;
	m_component_stack = top;

	if (top->component)
		top->component->IncUseCount();

	return OpStatus::OK;
}

void
OpComponentManager::PopRunningComponent()
{
	OP_ASSERT(m_component_stack);

	OpComponent* prev_component = m_component_stack->component;
	struct RunningComponentStack* top = m_component_stack;
	m_component_stack = m_component_stack->next;
	OP_DELETE(top);

	if (prev_component)
		prev_component->DecUseCount();
}

OP_STATUS
OpComponentManager::Destroy(OpComponent* component)
{
	// First, hunt down and kill the component itself.
	OpMessageAddress address = component->GetAddress();
	OpComponent* dummy;
	OP_ASSERT(address.ch == 0);
	OP_ASSERT(OpStatus::IsSuccess(m_components.GetData(address.co, &dummy)));
	OP_ASSERT(component == dummy);
	RETURN_IF_ERROR(m_components.Remove(address.co, &dummy));

	OP_STATUS ret = OpStatus::OK;

	// Broadcast a disconnect.
	if (IsInitialManager())
		ret = DisconnectMessenger(address);
	else
		ret = SendMessage(OpPeerDisconnectedMessage::Create(address, OpMessageAddress(0, 0, 0)));

	// Let the local platform know that we've destroyed the component.
	if (m_platform)
		m_platform->OnComponentDestroyed(address);

	return ret;
}

void
OpComponentManager::SetPlatform(OpComponentPlatform* platform)
{
	m_platform = platform;
	if (m_platform && m_inbox.First())
		m_platform->RequestRunSlice(NextTimeout());
}

OpComponent*
OpComponentManager::GetRunningComponent() const
{
	return m_component_stack ? m_component_stack->component : NULL;
}

unsigned int
OpComponentManager::RunSlice(unsigned int runtime /* = 100 */)
{
	OP_ASSERT(m_platform);

	if (!IsInitialManager() && !m_connected)
	{
		// We need to connect to the initial component manager in order to receive broadcasts.
		OpStatus::Ignore(SendMessage(OpPeerConnectedMessage::Create(GetAddress(), OpMessageAddress(0, 0, 0))));
		m_connected = true;
	}

	double now = m_op_time_info->GetRuntimeMS();
	double end = now + runtime;

	do {
		OpTypedMessage *message = GetMessage(now);
		if (!message)
			break;

#ifdef HC_MESSAGE_LOGGING
		if(GetMessageLogger())
			GetMessageLogger()->BeforeDispatch(message);
#endif
		DispatchMessage(message);
#ifdef HC_MESSAGE_LOGGING
		if(GetMessageLogger())
			GetMessageLogger()->AfterDispatch(message);
#endif
		OP_DELETE(message);

		now = m_op_time_info->GetRuntimeMS();
	} while (now < end);

	// Calculate delay until next message should be processed.
	return NextTimeout(now);
}

#ifdef MESSAGELOOP_RUNSLICE_SUPPORT
OP_STATUS
OpComponentManager::YieldPlatform(OpComponentPlatform::EventFlags flags /* = PROCESS_ANY */)
{
	OP_ASSERT(GetPlatform());

	/* Allow the platform to handle OS events while we're waiting for the right time to dispatch the
	 * next message. */
	RETURN_IF_ERROR(GetPlatform()->ProcessEvents(NextTimeout(), flags));

	/* Dispatch the next message. */
	RunSlice();

	return OpStatus::OK;
}
#endif // MESSAGELOOP_RUNSLICE_SUPPORT

void
OpComponentManager::DispatchMessage(const OpTypedMessage *message)
{
	const OpMessageAddress& dst = message->GetDst();
	OP_ASSERT(message->IsValid());
	OP_ASSERT(IsLocal(dst));

	if (dst.IsComponentManager()) // Addressed to this component manager
	{
		RETURN_VOID_IF_ERROR(PushRunningComponent(NULL));
		OpStatus::Ignore(ProcessMessage(message)); // FIXME: Handle OOM.
		PopRunningComponent();
		return;
	}

	OpComponent *component = GetComponent(dst);
	if (component)
	{
		RETURN_VOID_IF_ERROR(PushRunningComponent(component));
		OpStatus::Ignore(component->DispatchMessage(message)); // FIXME: Handle OOM.
		PopRunningComponent();
		return;
	}

	// Destination component disappeared between delivery phase and now.
	// Send OpPeerDisconnectedMessage back to sender.
	// Ignore if remote end has also disappeared.
	OpStatus::Ignore(SendMessage(
		OpPeerDisconnectedMessage::Create(dst, message->GetSrc(), 0)));
}

OP_STATUS
OpComponentManager::HandlePeerRequest(const OpMessageAddress& requester, OpComponentType type)
{
	if (!requester.IsValid())
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	OpComponent* component;
	RETURN_IF_ERROR(CreateComponent(component, type));

	OP_ASSERT(requester.IsChannel() || !"Peer requests are assumed to originate from channels.");

	// Construct a local channel end-point.
	OpChannel* channel = OP_NEW(OpChannel, (requester, component));
	if (!channel || OpStatus::IsError(channel->Construct(true)))
	{
		OP_DELETE(channel);
		OpStatus::Ignore(DestroyComponent(component));
		return OpStatus::ERR_NO_MEMORY;
	}

	// Add the component as a listener.
	OP_STATUS s = channel->AddMessageListener(component, true);
	if (OpStatus::IsError(s))
	{
		OpStatus::Ignore(DestroyComponent(component));
		return s;
	}

	// Let the requester know the component is ready for duty.
	s = channel->SendMessage(OpPeerConnectedMessage::Create(component->GetAddress(), requester));
	if (OpStatus::IsError(s))
		OpStatus::Ignore(DestroyComponent(component));

	if (!IsInitialManager() && m_platform)
		m_platform->OnComponentCreated(component->GetAddress());

	return s;
}

OP_STATUS
OpComponentManager::PeerGone(int component_manager)
{
	OP_ASSERT(GetAddress().cm == 0 || component_manager == 0); // Should only be called on the initial component manager
	if (component_manager == GetAddress().cm)
		return OpStatus::ERR;

	return SendMessage(OpPeerDisconnectedMessage::Create(
		OpMessageAddress(component_manager, 0, 0), OpMessageAddress(0, 0, 0)));
}

OpComponent*
OpComponentManager::GetComponent(int address)
{
	OpComponent* ret;
	if (OpStatus::IsSuccess(m_components.GetData(address, &ret)))
		return ret;

	return NULL;
}

BOOL
OpComponentManager::AddressExists(const OpMessageAddress& address)
{
	if (!IsLocal(address))
		return FALSE;

	if (address.IsComponentManager())
		return TRUE;

	// Forward to component, if it exists.
	OpComponent* component = NULL;
	OpStatus::Ignore(m_components.GetData(address.co, &component));
	return component && component->AddressExists(address);
}

OpMessageAddress
OpComponentManager::AssignNextComponentAddress()
{
	int co = m_next_component_number++;
	OP_ASSERT(co > 0);
	OP_ASSERT(m_next_component_number > co); // Detect overflow.
	return OpMessageAddress(m_address.cm, co, 0);
}

OP_STATUS
OpComponentManager::CreateComponent(OpComponent*& component, OpComponentType type)
{
	OpMessageAddress address = AssignNextComponentAddress();
	RETURN_IF_ERROR(OpComponent::Create(&component, type, &address));
	OP_ASSERT(component);
	OP_ASSERT(!m_components.Contains(address.co));
	RETURN_IF_ERROR(m_components.Add(address.co, component));

	// Send OpPeerConnectedMessage to the initial component manager.
	if (OpStatus::IsError(SendMessage(OpPeerConnectedMessage::Create(address, OpMessageAddress(0, 0, 0)))))
	{
		OpStatus::Ignore(DestroyComponent(component));
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS
OpComponentManager::DestroyComponent(OpComponent* component)
{
	component->SafeDelete();
	return OpStatus::OK;
}

bool
OpComponentManager::MessageIsBlocked(const OpTypedMessage* message) const
{
	OtlList<OpTypedMessageSelection*>::ConstIterator filter = m_message_filters.Begin();
	for (; filter != m_message_filters.End(); ++filter)
		if (!(*filter)->IsMember(message))
			return true;

	return false;
}

OpTypedMessage*
OpComponentManager::GetMessage(double now)
{
	OpTypedMessage* message = m_first_unblocked_message;

	while (message && MessageIsBlocked(message))
		message = message->Suc();

	m_first_unblocked_message = message;

	if (!message || static_cast<unsigned int>(message->GetDelay(now)) != 0)
		return NULL;

	if (message == m_first_delayed_message)
		m_first_delayed_message = message->Suc();

	m_first_unblocked_message = message->Suc();

	message->Out();

	return message;
}

OpComponent*
OpComponentManager::GetComponent(const OpMessageAddress& address) const
{
	if (!IsLocal(address))
		return NULL;
	OpComponent* component;
	RETURN_VALUE_IF_ERROR(m_components.GetData(address.co, &component), NULL);
	return component;
}

OP_STATUS
OpComponentManager::SendMessage(OpTypedMessage* message, unsigned int flags /* = SENDMESSAGE_OOM_IF_NULL */)
{
	if (!message)
		return (flags & OpMessenger::SENDMESSAGE_OOM_IF_NULL) ?
			OpStatus::ERR_NO_MEMORY : OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(message->IsValid());
	OP_STATUS s = OpStatus::ERR;
	if (IsLocal(message->GetDst()))
	{
#ifdef HC_MESSAGE_LOGGING
		if(GetMessageLogger())
			GetMessageLogger()->Log(message, UNI_L("Sending locally"));
#endif
		s = ReceiveMessage(message);
	}
	else if (m_platform)
	{
#ifdef HC_MESSAGE_LOGGING
		if(GetMessageLogger())
			GetMessageLogger()->Log(message, UNI_L("Sending through platform"));
#endif
		s = m_platform->SendMessage(message);
	}
	else // We've been given no platform to work with, our hands are tied.
	{
#ifdef HC_MESSAGE_LOGGING
		if(GetMessageLogger())
			GetMessageLogger()->Log(message, UNI_L("Aborting send due to missing platform"));
#endif
		OP_DELETE(message);
	}

	return s;
}

OP_STATUS
OpComponentManager::AddMessageFilter(OpTypedMessageSelection* filter)
{
	return m_message_filters.Append(filter);
}

bool
OpComponentManager::HasMessageFilter(OpTypedMessageSelection* filter) const
{
	OtlList<OpTypedMessageSelection*>::ConstIterator element = m_message_filters.Begin();
	for (; element != m_message_filters.End(); ++element)
		if (*element == filter)
			return true;

	return false;
}

OP_STATUS
OpComponentManager::RemoveMessageFilter(OpTypedMessageSelection* filter)
{
	if (m_message_filters.RemoveItem(filter))
	{
		m_first_unblocked_message = m_inbox.First();
		if (m_platform)
			m_platform->RequestRunSlice(NextTimeout());

		return OpStatus::OK;
	}

	return  OpStatus::ERR;
}

void OpComponentManager::AddToInbox(OpTypedMessage* message)
{
	OpTypedMessage* previous_first_unblocked_message = m_first_unblocked_message;

	// Move m_first_delayed_message to appropriate place in m_inbox.
	double now = m_op_time_info->GetRuntimeMS();

	while (m_first_delayed_message && m_first_delayed_message->GetDueTime() <= now)
		m_first_delayed_message = m_first_delayed_message->Suc();

	// Find appropriate spot in m_inbox for the given message.
	OpTypedMessage* p = m_first_delayed_message;
	while (p && p->GetDueTime() <= message->GetDueTime())
		p = p->Suc();
	if (p) // insert before p
	{
		message->Precede(p);

		if (p == m_first_unblocked_message)
			m_first_unblocked_message = message;
	}
	else // insert last
		message->Into(&m_inbox);

	// Update m_first_delayed_message, if necessary
	if (message->GetDueTime() > now) // this is a delayed message
	{
		if (!m_first_delayed_message) // there is no first delayed msg
			m_first_delayed_message = message;
		else if (message->GetDueTime() < m_first_delayed_message->GetDueTime())
		{
			// this message will trigger before the current first delayed msg
			OP_ASSERT(message->Suc() == m_first_delayed_message);
			m_first_delayed_message = message;
		}
	}

	if (!MessageIsBlocked(message))
	{
		if (!m_first_unblocked_message)
			m_first_unblocked_message = message;
		else
			m_first_unblocked_message = m_inbox.First();
	}

	if (m_platform && previous_first_unblocked_message != m_first_unblocked_message)
			m_platform->RequestRunSlice(NextTimeout());
}

OP_STATUS
OpComponentManager::ReceiveMessage(OpTypedMessage* message)
{
	OP_ASSERT(message);
	if (message->GetDueTime() == 0)
		message->SetDueTime(g_component_manager->GetRuntimeMS());

	OP_ASSERT(message->IsValid());
#ifdef HC_MESSAGE_LOGGING
	if(GetMessageLogger())
		GetMessageLogger()->Log(message, UNI_L("Adding to inbox"));
#endif
	AddToInbox(message);

	return OpStatus::OK;
}

OP_STATUS
OpComponentManager::ProcessMessage(const OpTypedMessage* message)
{
	switch (message->GetType())
	{
		case OpComponentRequestMessage::Type:
			return HandleComponentRequest(OpComponentRequestMessage::Cast(message));

		case OpPeerConnectedMessage::Type:
			return HandlePeerConnected(OpPeerConnectedMessage::Cast(message));

		case OpPeerDisconnectedMessage::Type:
			return HandlePeerDisconnected(OpPeerDisconnectedMessage::Cast(message));

		default:
			return OpMessenger::ProcessMessage(message);
	}
}

unsigned int
OpComponentManager::RemoveMessages(OpTypedMessageSelection* selection)
{
	unsigned int messages_removed = 0;
	OpTypedMessage* message = m_inbox.First();
	while (message)
	{
		OpTypedMessage* next = message->Suc();
		if (selection->IsMember(message))
		{
			if (message == m_first_delayed_message)
				m_first_delayed_message = next;

			if (message == m_first_unblocked_message)
				m_first_unblocked_message = next;

			message->Out();
			OP_DELETE(message);
			messages_removed++;
		}
		message = next;
	}

	return messages_removed;
}

unsigned int
OpComponentManager::RemoveFirstMessage(OpTypedMessageSelection* selection)
{
	OpTypedMessage* message = m_inbox.First();
	while (message)
	{
		OpTypedMessage* next = message->Suc();
		if (selection->IsMember(message))
		{
			if (message == m_first_delayed_message)
				m_first_delayed_message = next;

			if (message == m_first_unblocked_message)
				m_first_unblocked_message = next;

			message->Out();
			OP_DELETE(message);
			return 1;
		}
		message = next;
	}

	return 0;
}

#ifdef AVERAGE_MESSAGE_LAG

double
OpComponentManager::GetAverageLag() const
{
	double now = m_op_time_info->GetRuntimeMS();
	double lag = 0;
	size_t num_lagged = 0;

	for (OpTypedMessage* p = m_inbox.First(); p && p->GetDueTime() < now; p = p->Suc(), num_lagged++)
		lag += now - p->GetDueTime();
	return num_lagged ? lag / num_lagged : 0;
}

#endif // AVERAGE_MESSAGE_LAG

OP_STATUS
OpComponentManager::DisconnectMessenger(const OpMessageAddress& address)
{
	if (IsManager(address))
	{
		m_peers.RemoveByItem(address.cm);
		OP_ASSERT(m_peers.Find(address.cm) == -1);
	}

	// Alert components to disconnect all channels to the address space.
	OpHashIterator* it = m_components.GetIterator();
	RETURN_OOM_IF_NULL(it);
	OP_STATUS ret = OpStatus::OK;

	if (OpStatus::IsSuccess(it->First()))
		do { ret = static_cast<OpComponent*>(it->GetData())->DisconnectMessenger(address); }
		while (OpStatus::IsSuccess(ret) && OpStatus::IsSuccess(it->Next()));

	OP_DELETE(it);

	// Broadcast the disconnect.
	if (IsInitialManager() && IsManager(address))
		for (unsigned int i = 0; i < m_peers.GetCount(); i++)
			RETURN_IF_ERROR(SendMessage(OpPeerDisconnectedMessage::Create(
											address,
											OpMessageAddress(m_peers.Get(i), 0, 0))));

	return ret;
}

OP_STATUS
OpComponentManager::HandleComponentRequest(OpComponentRequestMessage* message)
{
	/* Relay the request to the platform, which will transmit the details
	 * to its chosen component manager. See OpComponentPlatform::RequestPeer(). */
	int peer = GetAddress().cm;
	OP_STATUS s = m_platform->RequestPeer(peer, message->GetSrc(), message->GetComponentType());
	if (OpStatus::IsError(s))
		/* Creation failed. Let the caller know. */
		return SendMessage(OpStatusMessage::Create(s, OpMessageAddress(0, 0, 0), message->GetSrc()));

	/* Inform the requester that its request is pending completion by the returned
	 * peer. This is necessary in order for the request to be cancelled if the peer
	 * terminates before completing the task. If there was no request-peer link, the
	 * request would stall indefinitely. */
	return SendMessage(OpPeerPendingMessage::Create(OpMessageAddress(peer, 0, 0), message->GetSrc()));
}

OP_STATUS
OpComponentManager::HandlePeerConnected(OpPeerConnectedMessage* message)
{
	OP_ASSERT(IsInitialManager());

	if (IsManager(message->GetSrc()))
	{
		int manager = message->GetSrc().cm;
		OP_ASSERT((m_peers.Find(manager) == -1) || !"I already know this manager. Was I not informed of its death?");
		m_peers.Add(manager);
	}
	else
		m_platform->OnComponentCreated(message->GetSrc());

	return OpStatus::OK;
}

OP_STATUS
OpComponentManager::HandlePeerDisconnected(OpPeerDisconnectedMessage* message)
{
	DisconnectMessenger(message->GetSrc());

	if (IsInitialManager() && !IsManager(message->GetSrc()))
		m_platform->OnComponentDestroyed(message->GetSrc());

	return OpStatus::OK;
}

unsigned int OpComponentManager::NextTimeout(double now /* = 0 */) const
{
	return m_first_unblocked_message ?
		static_cast<unsigned int>(m_first_unblocked_message->GetDelay(now)) : UINT_MAX;
}

OP_STATUS
OpComponentManager::LoadValue(const char* key, Value*& value)
{
	ValueMap::Iterator found = m_values.Find(key);
	if (found != m_values.End())
	{
		value = found->second;
		return OpStatus::OK;
	}

	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

OP_STATUS
OpComponentManager::StoreValue(const char* key, Value* value)
{
	ValueMap::Iterator found = m_values.Find(key);
	if (found != m_values.End())
	{
		OP_DELETE(found->second);
	}

	return m_values.Insert(key, value);
}

#ifdef _DEBUG
void
OpComponentManager::ValidateInbox()
{
	double now = m_op_time_info->GetRuntimeMS();
	bool unblocked_ok = false;

	/* Verify that undelayed messages aren't. */
	OpTypedMessage* p = m_inbox.First();
	while (p && p != m_first_delayed_message)
	{
		if (p == m_first_unblocked_message)
			unblocked_ok = true;

		OP_ASSERT(unblocked_ok || MessageIsBlocked(p));
		OP_ASSERT(p->GetDueTime() <= now);
		p = p->Suc();
	}

	/* Verify that delayed messages are correctly ordered. */
	while (p)
	{
		if (p == m_first_unblocked_message)
			unblocked_ok = true;

		OP_ASSERT(unblocked_ok || MessageIsBlocked(p));
		if (p->Suc())
			OP_ASSERT(p->GetDueTime() <= p->Suc()->GetDueTime());
		p = p->Suc();
	}

	/* Verify that first unblocked message is set. */
	OP_ASSERT(unblocked_ok || !m_first_unblocked_message);
	OP_ASSERT(m_message_filters.Count() > 0 || m_first_unblocked_message == m_inbox.First());
}
#endif // _DEBUG
