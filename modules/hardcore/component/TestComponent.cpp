/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/hardcore/component/TestComponent.h"
#include "modules/hardcore/component/Messages.h"
#include "modules/opdata/OpData.h"

/** This class is used as a message-listener for the selftest "new channel" in
 * component.ot.
 *
 * The selftest tests to create a new channel A in the selftest, which is
 * directed to an existing TestComponent. The selftest then sends the
 * text-message "test new channel" to the TestComponent. The TestComponent
 * creates as well a new channel B and connects B with A. This class is the
 * message-listener of B. The selftest continues to send a text-message "echo"
 * to B, this listener responds by sending a text-message "mew" back.
 *
 * When the channel A disconnects from B (i.e., this listener receives an
 * OpPeerDisconnectedMessage), then this listener will be deleted.
 */
class TestComponentNewChannel : public OpMessageListener
{
	TestComponent* m_component;
	OpChannel* m_channel;

public:
	TestComponentNewChannel(TestComponent* component, OpChannel* channel);
	virtual ~TestComponentNewChannel();
	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);
private:
	OP_STATUS OnTextMessage(const OpTextMessage* message);
};

TestComponentNewChannel::TestComponentNewChannel(TestComponent* component, OpChannel* channel)
	: m_component(component)
	, m_channel(channel)
{
	OP_ASSERT(m_component);
	OP_ASSERT(m_channel);
}

TestComponentNewChannel::~TestComponentNewChannel()
{
	if (m_channel)
		m_channel->RemoveMessageListener(this);
	m_component->RemoveListener(this);
}

OP_STATUS TestComponentNewChannel::ProcessMessage(const OpTypedMessage* message)
{
	switch (message->GetType()) {
	case OpPeerDisconnectedMessage::Type:
		OP_DELETE(this);
		return OpStatus::OK;

	case OpTextMessage::Type:
		return OnTextMessage(OpTextMessage::Cast(message));
	}

	return OpStatus::ERR;
}

OP_STATUS TestComponentNewChannel::OnTextMessage(const OpTextMessage* message)
{
	if (message->GetText() == UNI_L("echo"))
	{
		UniString text;
		RETURN_IF_ERROR(text.SetConstData(UNI_L("mew")));
		return m_channel->SendMessage(OpTextMessage::Create(text));
	}
	if (message->GetText() == UNI_L("deleteChannel"))
	{
		OP_DELETE(m_channel);
		m_channel = NULL;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}
// ================= TestComponent ===========================================

TestComponent::TestComponent(const OpMessageAddress& address)
	: OpComponent(address, COMPONENT_TEST)
{
}

TestComponent::~TestComponent()
{
	// Delete all existing listener instances.
	OtlList<OpMessageListener*>::Range r(m_listeners.All());
	for (; r; ++r)
		OP_DELETE(*r);
}

OP_STATUS
TestComponent::ProcessMessage(const OpTypedMessage* message)
{
	switch (message->GetType()) {
	case OpTextMessage::Type:
		return OnTextMessage(OpTextMessage::Cast(message));
	}
	return OpComponent::ProcessMessage(message);
}

void TestComponent::RemoveListener(OpMessageListener* listener)
{
	m_listeners.RemoveItem(listener);
}

OP_STATUS
TestComponent::OnTextMessage(const OpTextMessage* message)
{
	const UniString& text = message->GetText();

	if (text == UNI_L("echo"))
		message->Reply(OpTextMessage::Create(text));

	else if (text == UNI_L("error"))
		return OpStatus::ERR;

	else if (text == UNI_L("die"))
		g_component_manager->DestroyComponent(this);

	else if  (text == UNI_L("deleteChannel"))
		message->Reply(OpTextMessage::Create(text));

	else if (text == UNI_L("test new channel"))
	{
		/* Create a new connected channel with an associated listener. The
		 * channel is connected to the sender of the message. The listener
		 * expects to receive a message "echo" which is handled by sending a
		 * reply "mew" on the channel. */
		OpChannel* channel = CreateChannel(message->GetSrc());
		RETURN_OOM_IF_NULL(channel);
		OpAutoPtr<TestComponentNewChannel> listener(OP_NEW(TestComponentNewChannel, (this, channel)));
		RETURN_OOM_IF_NULL(listener.get());
		RETURN_IF_ERROR(channel->AddMessageListener(listener.get()));
		RETURN_IF_ERROR(m_listeners.Append(listener.get()));
		channel->Connect();
		listener.release();
	}
	return OpStatus::OK;
}
