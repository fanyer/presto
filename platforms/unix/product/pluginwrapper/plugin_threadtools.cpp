/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef NO_CORE_COMPONENTS

#include "platforms/unix/product/pluginwrapper/plugin_threadtools.h"

#include "platforms/unix/product/pluginwrapper/plugin_component_platform.h"
#include "platforms/x11api/plugins/toolkits/toolkit_loop.h"

OP_STATUS OpThreadTools::Create(OpThreadTools** new_thread_tools)
{
	OP_ASSERT(g_component_manager);
	PluginComponentPlatform* platform = static_cast<PluginComponentPlatform*>(g_component_manager->GetPlatform());
	*new_thread_tools = OP_NEW(PluginThreadTools, (platform));
	RETURN_OOM_IF_NULL(*new_thread_tools);
	return OpStatus::OK;
}

void* PluginThreadTools::Allocate(size_t size)
{
	return malloc(size);
}

void PluginThreadTools::Free(void* memblock)
{
	free(memblock);
}

OP_STATUS PluginThreadTools::SendMessageToMainThread(OpTypedMessage* message)
{
	OP_ASSERT(m_platform->GetLoop());
	return m_platform->GetLoop()->SendMessageOnMainThread(message);
}

#endif // NO_CORE_COMPONENTS
