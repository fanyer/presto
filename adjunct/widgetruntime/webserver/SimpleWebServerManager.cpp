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

#include "SimpleWebServerManager.h"
#include "src/SimpleWebServerManagerImpl.h"


SimpleWebServerManager::SimpleWebServerManager()
	: m_impl(NULL)
{
}

SimpleWebServerManager::~SimpleWebServerManager()
{
	OP_DELETE(m_impl);
}


OP_STATUS SimpleWebServerManager::Init()
{
	OP_DELETE(m_impl);

	m_impl = OP_NEW(SimpleWebServerManagerImpl, ());
	if (NULL == m_impl)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	return m_impl->Init();
}


OP_STATUS SimpleWebServerManager::AddWebServerStateListener(
		WebServerStateListener& listener)
{
	return m_impl->AddWebServerStateListener(listener);
}


OP_STATUS SimpleWebServerManager::StartWebServer()
{
	return m_impl->StartWebServer();
}


OP_STATUS SimpleWebServerManager::ShowWebServerStatus()
{
	return m_impl->ShowWebServerStatus();
}


# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
