/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef PLUGIN_THREADTOOLS_H
#define PLUGIN_THREADTOOLS_H

#include "modules/pi/OpThreadTools.h"

class PluginComponentPlatform;

class PluginThreadTools : public OpThreadTools
{
public:
	PluginThreadTools(PluginComponentPlatform* platform) : m_platform(platform) {}

	virtual void* Allocate(size_t size);
	virtual void Free(void* memblock);
	virtual OP_STATUS PostMessageToMainThread(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2, unsigned long delay = 0) { return OpStatus::ERR; }
	virtual OP_STATUS SendMessageToMainThread(OpTypedMessage* message);

private:
	PluginComponentPlatform* m_platform;
};

#endif // PLUGIN_THREADTOOLS_H
