/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/hardcore/component/OpSyncMessenger.h"
#include "modules/hardcore/component/Messages.h"

#ifdef MESSAGELOOP_RUNSLICE_SUPPORT

/**
 * A message filter blocking messages to all channels except the given channel. The
 * filter is limited to the component hosting the given channel.
 */
class NarcissistFilter
	: public OpTypedMessageSelection
{
public:
	NarcissistFilter(const OpMessageAddress& address)
		: m_address(address) {}

	virtual BOOL IsMember(const OpTypedMessage* message) const {
		return message->GetDst().co != m_address.co
			|| message->GetDst().ch == m_address.ch;
	}

protected:
	OpMessageAddress m_address;
};

OpSyncMessenger::OpSyncMessenger(OpChannel* peer)
	: m_peer(peer)
	, m_response(NULL)
	, m_status(OpStatus::OK)
{
}

OpSyncMessenger::~OpSyncMessenger()
{
	OP_DELETE(m_peer);
	ClearResponse();
}

OpChannel*
OpSyncMessenger::TakePeer()
{
	OpChannel* peer = m_peer;
	m_peer = NULL;
	return peer;
}

OP_STATUS
OpSyncMessenger::AcceptResponse(UINT32 type)
{
	return m_acceptable_response_types.Add(static_cast<INT32>(type));
}

/* virtual */ OP_STATUS
OpSyncMessenger::SendMessage(OpTypedMessage* message_, bool allow_nesting /* = true */,
							 unsigned int timeout /* = 0 */)
{
	OpAutoPtr<OpTypedMessage> message(message_);

	/* Convenience path. */
	if (!m_peer || !message.get())
		return OpStatus::ERR_NO_MEMORY;

	/* Block all other channels if nesting is disallowed. */
	NarcissistFilter filter(m_peer->GetAddress());
	if (!allow_nesting)
		RETURN_IF_ERROR(g_component_manager->AddMessageFilter(&filter));

	m_status = ExchangeMessages(message.release(), timeout);

	/* Unblock other channels. */
	if (!allow_nesting)
		OpStatus::Ignore(g_component_manager->RemoveMessageFilter(&filter));

	return m_status;
}

/* virtual */ OP_STATUS
OpSyncMessenger::SendMessage(OpTypedMessage* message_, OpTypedMessageSelection* filter,
							 unsigned int timeout /* = 0 */)
{
	OpAutoPtr<OpTypedMessage> message(message_);

	/* Convenience path. */
	if (!m_peer || !message.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(g_component_manager->AddMessageFilter(filter));
	m_status = ExchangeMessages(message.release(), timeout);
	OpStatus::Ignore(g_component_manager->RemoveMessageFilter(filter));

	return m_status;
}

OP_STATUS
OpSyncMessenger::ExchangeMessages(OpTypedMessage* message, unsigned int timeout,
								  OpComponentPlatform::EventFlags flags /* = PROCESS_IPC_MESSAGES */)
{
	m_status = OpStatus::OK;
	m_message = message;
	ClearResponse();

	/* Send early if possible. */
	if (m_peer->IsDirected())
		RETURN_IF_ERROR(Send());

	/* Install timeout if requested. */
	if (timeout > 0)
		SetTimeout(timeout);

	RETURN_IF_ERROR(m_peer->AddMessageListener(this));
	BOOL activated = OpStatus::IsSuccess(PushActivationRecord());

	/* Wait for a response. */
	while (!m_response && !OpStatus::IsError(m_status))
	{
		OP_ASSERT(g_component_manager);
		OP_STATUS status = g_component_manager->YieldPlatform(flags);
		if (OpStatus::IsSuccess(m_status))
			m_status = status;
	}

	if (activated)
		PopActivationRecord();

	/* m_peer might be NULL after ->YieldPlatform() if remote end-point terminates the connection */
	if (m_peer != NULL)
		m_peer->RemoveMessageListener(this);

	/* Inform the platform of the next time we need attention. Normally this is transmitted through
	 * the return value of the outermost RunSlice(). However, in some cases (notably when plug-ins
	 * make requests directly from the OS message loop) this method may be invoked outside of a
	 * RunSlice() and then the Windows implementation seems to skip a beat. FIXME: Workaround. */
	g_component_manager->GetPlatform()->RequestRunSlice(g_component_manager->NextTimeout());

	return m_status;
}

void
OpSyncMessenger::ClearResponse()
{
	if (m_response)
	{
		OP_DELETE(m_response);
		m_response = NULL;
	}
}


OpTypedMessage*
OpSyncMessenger::GetResponse()
{
	return m_response;
}

OpTypedMessage*
OpSyncMessenger::TakeResponse()
{
	OpTypedMessage* response = m_response;
	m_response = NULL;
	return response;
}

/* virtual */ OP_STATUS
OpSyncMessenger::ProcessMessage(const OpTypedMessage* message)
{
	switch (message->GetType())
	{
		case OpPeerConnectedMessage::Type:
			if (m_message)
				m_status = Send();
			break;

		case OpStatusMessage::Type:
			m_status = OpStatusMessage::Cast(message)->GetStatus();
			if (m_status == OpStatus::ERR_YIELD)
				RETURN_IF_ERROR(PropagateTimeout());
			break;

		case OpPeerDisconnectedMessage::Type:
			OP_DELETE(m_peer);
			m_peer = NULL;
			m_status = OpStatus::ERR_NO_SUCH_RESOURCE;
			break;

		default:
			if (HasAcceptableType(message))
				m_status = CloneMessage(&m_response, message);
			break;
	};

	return OpStatus::OK;
}

OP_STATUS
OpSyncMessenger::Send()
{
	OP_ASSERT(m_message);
	OpTypedMessage* message = m_message;
	m_message = NULL;
	return m_peer->SendMessage(message);
}

OP_STATUS
OpSyncMessenger::CloneMessage(OpTypedMessage** out_clone, const OpTypedMessage* message)
{
	OpSerializedMessage* sm = OpTypedMessage::Serialize(message);
	RETURN_OOM_IF_NULL(sm);

	*out_clone = OpSerializedMessage::Deserialize(sm);
	OP_DELETE(sm);
	RETURN_OOM_IF_NULL(*out_clone);

	return OpStatus::OK;
}

bool
OpSyncMessenger::HasAcceptableType(const OpTypedMessage* message)
{
	if (m_acceptable_response_types.GetCount() == 0)
		return true;

	for (unsigned int i = 0; i < m_acceptable_response_types.GetCount(); i++)
		if (static_cast<INT32>(message->GetType()) == m_acceptable_response_types.Get(i))
			return true;

	return false;
}

/**
 * Helper class maintaining a stack of active sync messengers in the current component manager
 * (i.e. thread.)
 */
class ActivationRecords
	: public OpComponentManager::Value
{
	OtlList<OpSyncMessenger*> m_messengers;

public:
	/**
	 * Push a sync messenger onto the activation stack.
	 *
	 * @param messenger Messenger activated.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS Push(OpSyncMessenger* messenger);

	/**
	 * Pop a sync messenger from the activation stack.
	 *
	 * @param messenger Active messenger to be deactivated.
	 */
	void Pop(OpSyncMessenger* messenger);

	/**
	 * Transmit an immediate timeout message to all messengers enclosed by the argument messenger.
	 *
	 * I.e. if the call stack is M_A -> YP -> ... -> M_B -> YP -> ... -> M_C -> ...,
	 * where M_k is messenger k, and YP is YieldPlatform(), then PropagateTimeout(M_A) will
	 * send immediate timeout messages to M_B, M_C, and so on.
	 *
	 * @return OpStatus::OK on success, or OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS PropagateTimeout(OpSyncMessenger* messenger);

	/** Key used to store the activation records in the component manager. */
	static const char* GetKey() { return "hardcore-opsyncmessenger-activation-records"; }
};

OP_STATUS
ActivationRecords::Push(OpSyncMessenger* messenger)
{
	return m_messengers.Append(messenger);
}

void
ActivationRecords::Pop(OpSyncMessenger* messenger)
{
	OP_ASSERT(messenger == m_messengers.Last());
	m_messengers.PopLast();
}

OP_STATUS
ActivationRecords::PropagateTimeout(OpSyncMessenger* messenger)
{
	bool time_out = false;
	for (OtlList<OpSyncMessenger*>::ConstIterator e = m_messengers.Begin(); e != m_messengers.End(); e++)
	{
		if (time_out)
			/* Force a timeout of the nested messenger, breaking its loop. This will trigger
			 * RequestRunSlice(0) and terminate the innermost ProcessEvents(), if necessary. */
			RETURN_IF_MEMORY_ERROR((*e)->SetTimeout(0));

		else if (*e == messenger)
			time_out = true;
	}

	return OpStatus::OK;
}

OP_STATUS
OpSyncMessenger::PushActivationRecord()
{
	OpComponentManager::Value* value = NULL;
	ActivationRecords* records = NULL;
	if (g_component_manager->LoadValue(ActivationRecords::GetKey(), value) == OpStatus::ERR_NO_SUCH_RESOURCE)
	{
		OpAutoPtr<ActivationRecords> records_(OP_NEW(ActivationRecords, ()));
		RETURN_OOM_IF_NULL(records_.get());
		RETURN_IF_ERROR(g_component_manager->StoreValue(ActivationRecords::GetKey(), records_.get()));
		records = records_.release();
	}
	else
		records = static_cast<ActivationRecords*>(value);

	return records->Push(this);
}

void
OpSyncMessenger::PopActivationRecord()
{
	OpComponentManager::Value* value = NULL;
	if (OpStatus::IsSuccess(g_component_manager->LoadValue(ActivationRecords::GetKey(), value)))
		static_cast<ActivationRecords*>(value)->Pop(this);
}

OP_STATUS
OpSyncMessenger::PropagateTimeout()
{
	OpComponentManager::Value* value = NULL;
	if (OpStatus::IsSuccess(g_component_manager->LoadValue(ActivationRecords::GetKey(), value)))
		return static_cast<ActivationRecords*>(value)->PropagateTimeout(this);

	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

OP_STATUS
OpSyncMessenger::SetTimeout(unsigned int timeout)
{
	RETURN_OOM_IF_NULL(m_peer);
	return OpMessenger::SendMessage(OpStatusMessage::Create(OpStatus::ERR_YIELD,
															m_peer->GetAddress(),
															m_peer->GetAddress(),
															timeout));
}

#endif // MESSAGELOOP_RUNSLICE_SUPPORT
