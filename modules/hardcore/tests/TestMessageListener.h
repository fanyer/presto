/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef TEST_MESSAGE_LISTENER_H
#define TEST_MESSAGE_LISTENER_H

#include "modules/hardcore/component/OpMessenger.h"
#include "modules/hardcore/timer/optimer.h"

/** The TestMessageListener class can be used as a base-class in async tests for
 * the component framework.
 *
 * On creation the instance starts a timer. If the test is not finished within
 * the timer's timeout, the test fails.
 *
 * A test starts usually with requesting a TestComponent. When the TestComponent
 * is connected, OnStartTest() is called. That method needs to be implemented
 * to start the actual test.
 *
 * To run a test, the TestMessageListener implementation should also override
 * OnMessage() to handle all expected messages.
 *
 * Example:
 * @code
 * class MyTestListener : public TestMessageListener
 * {
 * public:
 *     MyTestListener(OpChannel* channel) : TestMessageListener(channel) {}
 *     virtual ~MyTestListener() {}
 *
 *     // Start the test, e.g., by sending some message
 *     virtual OP_STATUS OnStartTest()
 *     {
 *         if (OpStatus::IsError(m_channel->SendMessage(SomeMessage::Create(...))))
 *             Fail("Could not send the message");
 *         return OpStatus::OK;
 *     }
 *
 *     // Handle any expected message, pass unexpected messages to the
 *     // base-class (thus fail the test):
 *     virtual OP_STATUS OnMessage(const OpTypedMessage* message)
 *     {
 *         switch (message->GetType()) {
 *         case MyExpectedMessage::Type:
 *             return OnMyExpectedMessage(MyExpectedMessage::Cast(message));
 *         ...
 *         default:
 *             return TestMessageListener::OnMessage(message);
 *         }
 *     }
 * };
 * @endcode
 */
class TestMessageListener
	: public OpMessageListener
	, public OpTimerListener
{
private:
	bool m_timer_active;
	bool m_started;
	OpTimer m_timer;

	/** @name Implementation of OpTimerListener
	 * @{ */
	virtual void OnTimeOut(OpTimer* timer)
	{
		OP_ASSERT(timer == &m_timer);
		OP_ASSERT(m_timer_active || !"We shouldn't receive an OnTimeOut() if the timer is not active!");
		m_timer_active = false;
		ST_failed("Test timed out");
	}
	/** @} */

	void EndTest()
	{
		m_timer.Stop();
		if (m_channel) m_channel->RemoveMessageListener(this);
		m_channel = NULL;
	}

protected:
	OpChannel* m_channel;

public:
	TestMessageListener(OpChannel* channel, unsigned int timeout_ms = 1000)
		: m_timer_active(true)
		, m_started(false)
		, m_channel(channel)
	{
		m_timer.SetTimerListener(this);
		m_timer.Start(timeout_ms);
	}

	virtual ~TestMessageListener()
	{
		m_timer.SetTimerListener(NULL);
		EndTest();
	}

	void Fail(const char* message)
	{
		EndTest();
		ST_failed(message);
	}

	void Succeed()
	{
		EndTest();
		ST_passed();
	}

	bool VerifyValidMessage(const OpTypedMessage* message)
	{
		if (message->IsValid())
			return true;
		Fail("message is not valid");
		return false;
	}

	/** This is usually the start point of the test.
	 * This method can be called manually on the test, or the listener can wait
	 * for the next OpPeerConnectedMessage.
	 * This method sets the flag m_started and calls OnStartTest(). */

	void StartTest()
	{
		m_started = true;
		if (OpStatus::IsError(OnStartTest()))
			Fail("Could not start test");
	}

	/** @name Implementation of OpMessageListener
	 * @{ */

private:
	/** This method handles incoming messages.
	 *
	 * The first incoming OpPeerConnectedMessage is dispatched to the method
	 * OnPeerConnected(), which calls OnStartTest(). This is the starting point
	 * for a test. An implementation of this class needs to implement
	 * OnStartTest(). Any other message (even a second incoming
	 * OpPeerConnectedMessage) is handed over to OnOtherMessage(). */
	virtual OP_STATUS ProcessMessage(const OpTypedMessage* message)
	{
		if (!m_started && message->GetType() == OpPeerConnectedMessage::Type)
		{
			if (VerifyValidMessage(message))
				StartTest();
			return OpStatus::OK;
		}
		else
			return OnMessage(message);
	}

public:
	/** By default any unhandled message is a test-failure. Test
	 * implementations should override this method to not fail on expected
	 * messages. */
	virtual OP_STATUS OnMessage(const OpTypedMessage* message)
	{
		Fail("Received unexpected message");
		return OpStatus::OK;
	}

	/** Needs to be implemented by the test to actually start the test.
	 * Usually a test is started by sending some handshake-message. */
	virtual OP_STATUS OnStartTest() = 0;

	/** @} */
};

#endif // TEST_MESSAGE_LISTENER_H
