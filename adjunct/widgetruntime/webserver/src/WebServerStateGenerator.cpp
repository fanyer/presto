/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT
# ifdef WIDGET_RUNTIME_UNITE_SUPPORT

#include "../WebServerStateListener.h"
#include "WebServerStateGenerator.h"


OP_STATUS WebServerStateGenerator::AddWebServerStateListener(
		WebServerStateListener& listener)
{
	OP_STATUS result = OpStatus::OK;

	// Assumption: it makes no sense to listen to one's own events.  And it may
	// actually lead to stack overflow.
	if (reinterpret_cast<void*>(this) != reinterpret_cast<void*>(&listener))
	{
		result = m_listeners.Add(&listener);
	}

	return result;
}


void WebServerStateGenerator::NotifyLoggedIn()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
	{
		if (OpStatus::IsError(m_listeners.GetNext(it)->OnLoggedIn()))
		{
			NotifyWebServerStartUpError();
			break;
		}
	}
}


void WebServerStateGenerator::NotifyWebServerSetUpCompleted(
		const OpStringC& shared_secret)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
	{
		if (OpStatus::IsError(m_listeners.GetNext(it)
					->OnWebServerSetUpCompleted(shared_secret)))
		{
			NotifyWebServerStartUpError();
			break;
		}
	}
}


void WebServerStateGenerator::NotifyWebServerStarted()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
	{
		if (OpStatus::IsError(m_listeners.GetNext(it)->OnWebServerStarted()))
		{
			NotifyWebServerError();
			break;
		}
	}
}


void WebServerStateGenerator::NotifyWebServerStopped()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
	{
		m_listeners.GetNext(it)->OnWebServerStopped();
	}
}


void WebServerStateGenerator::NotifyWebServerError(
		WebserverStatus web_server_status)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
	{
		m_listeners.GetNext(it)->OnWebServerError(web_server_status);
	}
}


void WebServerStateGenerator::NotifyWebServerStartUpError()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
	{
		m_listeners.GetNext(it)->OnWebServerStartUpError();
	}
}


# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
