/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_COMPONENT_TESTCOMPONENT
#define MODULES_HARDCORE_COMPONENT_TESTCOMPONENT

#include "modules/hardcore/component/OpComponent.h"
#include "modules/hardcore/src/generated/g_message_hardcore_messages.h"

/** The TestComponent is used by the selftest system to run some component
 * framework selftests.
 *
 * To start a new TestComponent, a selftest can request a new component of that
 * type and add its message-listener to the returned channel to receive messages
 * from that component:
 *
 * @code
 * test("some test") async;
 * {
 *     OpChannel* channel;
 *     if (OpStatus::IsError(g_opera->RequestComponent(&channel, COMPONENT_TEST)))
 *         ST_failed("Failed to request a COMPONENT_TEST");
 *     // deleting the channel will disconnect and destroy the test-component
 *     ST_delete_after_group(channel);
 *     OpMessageListener* listener = OP_NEW(MyTestListener, (channel, ...));
 *     ST_delete_after_group(listener);
 *     if (OpStatus::IsError(channel->AddMessageListener(&listener)))
 *         ST_failed("Failed to add a message listener to the channel");
 *     ....
 * }
 * @endcode
 * This example selftest requests a new TestComponent and assigns some
 * OpMessageListener instance to the local channel. Thus the OpMessageListener
 * instance will receive messages from the TestComponent.
 *
 * To actually run some tests, the selftest needs to send some messages to the
 * TestComponent. This can be done once the TestComponent is connected.
 * The class TestMessageListener helps to implement such tests.
 *
 * The TestComponent can answer the following messages:
 * - Sending a text-message with the text "echo" makes the TestComponent send
 *   a text-message with the text "echo" as a reply to the sender.
 * - Sending a text-message with the text "error" makes the
 *   TestComponent::ProcessMessage() return OpStatus::ERR. That causes the
 *   component-framework to send an OpStatusMessage with status OpStatus::ERR to
 *   the sender.
 * - Sending a text-message with the text "die" terminates the TestComponent,
 *   i.e., the TestComponent will destroy itself and thus all connected channels
 *   (and their listeners) are expected to receive an OpPeerDisconnectedMessage.
 */
class TestComponent
	: public OpComponent
{
public:
	TestComponent(const OpMessageAddress& address);
	virtual ~TestComponent();
	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message);

	void RemoveListener(OpMessageListener* listener);
private:
	/** Handles text-messages.
	 * - On receiving "echo", a text-message "echo" is sent to the sender.
	 * - On receiving "error", this method returns with OpStatus::ERR. Thus the
	 *   sender is expected to receive an OpStatusMessage.
	 * - On receiving "die", this method destroys this component.
	 */
	OP_STATUS OnTextMessage(const OpTextMessage* message);

	OtlList<OpMessageListener*> m_listeners;
};

#endif /* MODULES_HARDCORE_COMPONENT_TESTCOMPONENT */
