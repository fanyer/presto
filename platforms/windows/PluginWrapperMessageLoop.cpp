/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef PLUGIN_WRAPPER

#include "PluginWrapperMessageLoop.h"
#include "platforms/windows/CustomWindowMessages.h"

PluginWrapperMessageLoop::PluginWrapperMessageLoop()
	: m_last_flash_message(0)
{
}

PluginWrapperMessageLoop::~PluginWrapperMessageLoop()
{
	while (m_delayed_flash_messages.GetCount() > 0)
	{
		MSG* msg = m_delayed_flash_messages.Remove(0);
		DispatchMessage(msg);

		OP_DELETE(msg);
	}

}

OP_STATUS PluginWrapperMessageLoop::Init()
{
	RETURN_IF_ERROR(WindowsOpComponentPlatform::Init());

	return OpStatus::OK;
}

OP_STATUS PluginWrapperMessageLoop::ProcessMessage(MSG &msg)
{
	switch (msg.message)
	{
		case WM_FLASH_MESSAGE:
		{
			double now = g_component_manager->GetRuntimeMS();
			BOOL delay_flash = m_delayed_flash_messages.GetCount() > 0  || (now - m_last_flash_message) < 5;

			m_last_flash_message = now;

			if (delay_flash)
			{
				OpAutoPtr<MSG> saved_message = OP_NEW(MSG, (msg));
				RETURN_OOM_IF_NULL(saved_message.get());
				RETURN_IF_ERROR(m_delayed_flash_messages.Add(saved_message.get()));
				saved_message.release();
				break;
			}

			//fallthrough
		}
		default:
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return OpStatus::OK;
}

OP_STATUS PluginWrapperMessageLoop::DoLoopContent(BOOL ran_core)
{
	if (ran_core && m_delayed_flash_messages.GetCount() > 0)
	{
		MSG* msg = m_delayed_flash_messages.Remove(0);
		DispatchMessage(msg);

		OP_DELETE(msg);

		m_last_flash_message = g_component_manager->GetRuntimeMS();
	}

	if (m_delayed_flash_messages.GetCount() > 0)
	{
		RequestRunSlice(0);
	}

	return OpStatus::OK;
}

#endif // PLUGIN_WRAPPER
