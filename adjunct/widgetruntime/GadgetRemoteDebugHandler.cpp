/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick/windows/DesktopGadget.h"
#include "adjunct/widgetruntime/GadgetRemoteDebugHandler.h"

#include "modules/gadgets/OpGadget.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_tools.h"
#include "modules/scope/scope_module.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/scope_connection_manager.h"


namespace
{
	const INT32 CONNECTION_POLLING_INTERVAL = 500; // [ms]
	const INT32 CONNECTION_POLLING_TIMEOUT = 3000; // [ms]

	
}

IScopeManager* GadgetRemoteDebugHandler::l_scope_manager;


GadgetRemoteDebugHandler::GadgetRemoteDebugHandler(BOOL isTestRun, IPrefsCollectionTools* prefsm,IScopeManager* scpm,IMessageHandler* msgh)
	: m_is_test_run(isTestRun)
{
	if (!m_is_test_run)
	{
		l_pctools = (IPrefsCollectionTools *)OP_NEW(RegularPrefsCollectionTools,());
		l_scope_manager = (IScopeManager *) OP_NEW(RegularScopeManager,());
		l_message_handler =  (IMessageHandler*) OP_NEW(RegularMessageHandler,());
	}
	else
	{
		l_pctools = prefsm;
		l_scope_manager = scpm;
		l_message_handler =	msgh;
	}

	l_message_handler->SetCallBack(
		this, MSG_SCOPE_CONNECTION_STATUS, 0);
}


GadgetRemoteDebugHandler::~GadgetRemoteDebugHandler()
{
	
	l_message_handler->UnsetCallBacks(this);
	if (!m_is_test_run)
	{
		OP_DELETE(l_scope_manager);
		OP_DELETE(l_message_handler);
		OP_DELETE(l_pctools);
	}
}


GadgetRemoteDebugHandler::State GadgetRemoteDebugHandler::GetState() const
{
	State state;
	state.m_ip_address.Set(
			l_pctools->GetStringPref(PrefsCollectionTools::ProxyHost));
	state.m_port_no =
			l_pctools->GetIntegerPref(PrefsCollectionTools::ProxyPort);
	state.m_connected = l_scope_manager->IsConnected();
	return state;
}


OP_STATUS GadgetRemoteDebugHandler::AddListener(Listener& listener)
{
	return m_listeners.Add(&listener);
}


OP_STATUS GadgetRemoteDebugHandler::RemoveListener(Listener& listener)
{
	return m_listeners.Remove(&listener);
}


OP_STATUS GadgetRemoteDebugHandler::SetState(const State& rhs)
{
	OP_STATUS status = OpStatus::OK;

	// Refresh the current state.
	const State& state = GetState();

	if (!state.m_connected && !rhs.m_connected)
	{
		status = OpStatus::OK;
	}
	else if (state.m_connected && !rhs.m_connected)
	{
		status = Disconnect();
	}
	else
	{
		// Will have to either connect or reconnect.

		const BOOL state_changed =
				!(state.m_ip_address == rhs.m_ip_address)
				|| state.m_port_no != rhs.m_port_no;

		if (state.m_connected && state_changed)
		{
			status = Reconnect(rhs);
		}
		else
		{
			status = Connect(rhs);
		}
	}

	return status;
}


OP_STATUS GadgetRemoteDebugHandler::Connect(const State& state)
{
	if (!l_scope_manager->IsConnected())
	{
		RETURN_IF_LEAVE(
			l_pctools->WriteStringL(
					PrefsCollectionTools::ProxyHost, state.m_ip_address);
			l_pctools->WriteIntegerL(
					PrefsCollectionTools::ProxyPort, state.m_port_no);
		);

		// now, it is extremely important to pass non-zero value as first
		// parameter here, otherwise gadget won't be able to connect to remote
		// dragonfly server (at least not in current version of scope module)
		// [pobara 2010-02-27]
		RETURN_IF_ERROR(l_message_handler->PostMessage(
					MSG_SCOPE_CREATE_CONNECTION, 1, 0, 0));
	}
	return OpStatus::OK;
}


OP_STATUS GadgetRemoteDebugHandler::Reconnect(const State& state)
{
	m_reconnector.Init(*this, state);
	m_reconnecting_poller.Start(CONNECTION_POLLING_INTERVAL,
			CONNECTION_POLLING_TIMEOUT, m_reconnector);
	RETURN_IF_ERROR(Disconnect());
	return OpStatus::OK;
}


OP_STATUS GadgetRemoteDebugHandler::Disconnect()
{
	if (l_scope_manager->IsConnected())
	{
		RETURN_IF_ERROR(l_message_handler->PostMessage(
					MSG_SCOPE_CLOSE_CONNECTION, 0, 0));
	}
	return OpStatus::OK;
}


void GadgetRemoteDebugHandler::HandleCallback(
		OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{

	if (msg == MSG_SCOPE_CONNECTION_STATUS)
	{
		OpString notification_text;
		const OpScopeConnectionManager::ConnectionStatus status =
				static_cast<enum OpScopeConnectionManager::ConnectionStatus>(par2);

		switch (status)
		{
			case OpScopeConnectionManager::CONNECTION_OK:
				for (OpListenersIterator it(m_listeners);
						m_listeners.HasNext(it); )
				{
					m_listeners.GetNext(it)->OnConnectionSuccess();
				}
				break;

			case OpScopeConnectionManager::CONNECTION_FAILED:
			case OpScopeConnectionManager::CONNECTION_TIMEDOUT:
				for (OpListenersIterator it(m_listeners);
						m_listeners.HasNext(it); )
				{
					m_listeners.GetNext(it)->OnConnectionFailure();
				}
				break;
		}
	}
}


//=============================================================================
// ConnectionPoller
//

GadgetRemoteDebugHandler::ConnectionPoller::ConnectionPoller()
	: m_interval(-1), m_max_poll_count(-1), m_poll_count(-1), m_listener(NULL)
{
	m_timer.SetTimerListener(this);
}


BOOL GadgetRemoteDebugHandler::ConnectionPoller::IsPolling() const
{
	return NULL != m_listener;
}


void GadgetRemoteDebugHandler::ConnectionPoller::Start(INT32 interval,
		INT32 timeout, Listener& listener)
{
	m_timer.Stop();

	m_interval = MAX(1, interval);
	m_poll_count = 0;
	m_max_poll_count = MAX(1, timeout / m_interval);
	m_listener = &listener;

	m_timer.Start(m_interval);
}


void GadgetRemoteDebugHandler::ConnectionPoller::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(&m_timer == timer);
	OP_ASSERT(NULL != m_listener);

	if (GadgetRemoteDebugHandler::l_scope_manager->IsConnected())
	{
		if (++m_poll_count < m_max_poll_count)
		{
			m_timer.Start(m_interval);
		}
		else
		{
			m_listener->OnMaxPollCount();
			m_listener = NULL;
		}
	}
	else
	{
		m_listener->OnConnectionClosed();
		m_listener = NULL;
	}
}



//=============================================================================
// Reconnector
//

GadgetRemoteDebugHandler::Reconnector::Reconnector()
	: m_handler(NULL)
{
}


void GadgetRemoteDebugHandler::Reconnector::Init(
		GadgetRemoteDebugHandler& handler, const State& state)
{
	m_handler = &handler;
	m_state = state;
}


void GadgetRemoteDebugHandler::Reconnector::OnConnectionClosed()
{
	Connect();
}


void GadgetRemoteDebugHandler::Reconnector::OnMaxPollCount()
{
	Connect();
}


void GadgetRemoteDebugHandler::Reconnector::Connect()
{
	OP_ASSERT(NULL != m_handler);
	OpStatus::Ignore(m_handler->Connect(m_state));
}



//=============================================================================
// State
//

GadgetRemoteDebugHandler::State::State()
	: m_port_no(-1), m_connected(FALSE)
{
}


GadgetRemoteDebugHandler::State::State(const State& rhs)
	: m_port_no(rhs.m_port_no), m_connected(rhs.m_connected)
{
	m_ip_address.Set(rhs.m_ip_address);
}


GadgetRemoteDebugHandler::State& GadgetRemoteDebugHandler::State::operator=(
		const State& rhs)
{
	if (this != &rhs)
	{
		m_ip_address.Set(rhs.m_ip_address);
		m_port_no = rhs.m_port_no;
		m_connected = rhs.m_connected;
	}
	return *this;
}


 


#endif // WIDGET_RUNTIME_SUPPORT
