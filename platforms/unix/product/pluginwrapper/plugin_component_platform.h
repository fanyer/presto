/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef PLUGIN_COMPONENT_PLATFORM
#define PLUGIN_COMPONENT_PLATFORM

#include "platforms/posix_ipc/posix_ipc_component_platform.h"
#include "platforms/x11api/plugins/toolkits/toolkit_loop.h"

class PluginComponentPlatform : public PosixIpcComponentPlatform, public DescriptorListener
{
public:
	PluginComponentPlatform() : m_loop(NULL) {}

	OP_STATUS RunGTK2Loop();

	ToolkitLoop* GetLoop() { return m_loop; }

	// From PosixIpcComponentPlatform
	virtual int Run() { return RunGTK2Loop() == OpStatus::OK ? 0 : -1; }
	virtual void Exit();
	virtual OP_STATUS SendMessage(OpTypedMessage* message);

	// From DescriptorListener
	virtual OP_STATUS OnReadReady(bool& keep_listening);
	virtual OP_STATUS OnWriteReady(bool& keep_listening);

private:
	ToolkitLoop* m_loop;
};

#endif // PLUGIN_COMPONENT_PLATFORM
