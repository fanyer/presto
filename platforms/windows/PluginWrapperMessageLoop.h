/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef PLUGIN_WRAPPER_MESSAGE_LOOP_H
#define PLUGIN_WRAPPER_MESSAGE_LOOP_H

#ifdef PLUGIN_WRAPPER

#include "platforms/windows/IPC/WindowsOpComponentPlatform.h"

class PluginWrapperMessageLoop : public WindowsOpComponentPlatform
{
public:

	PluginWrapperMessageLoop();
	~PluginWrapperMessageLoop();

	OP_STATUS			Init();

private:

	OP_STATUS ProcessMessage(MSG &msg);
	OP_STATUS DoLoopContent(BOOL ran_core);

	double m_last_flash_message;
	OpVector<MSG> m_delayed_flash_messages;
};

#endif // PLUGIN_WRAPPER
#endif // PLUGIN_WRAPPER_MESSAGE_LOOP_H
