/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * Windowless plugin instance representation.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#ifndef WINDOWLESS_INSTANCE_H
#define WINDOWLESS_INSTANCE_H

#include "common.h"


/**
 * Windowless plugin instance.
 */
class WindowlessInstance : public PluginInstance
{
public:
	WindowlessInstance(NPP instance, const char* bgcolor = 0);
	virtual int16_t HandleEvent(void* event);
	virtual bool IsWindowless() { return true; }

	/* Setup instance. Called during NPP_New. */
	virtual bool Initialize();

protected:
	virtual bool GetOriginRelativeToWindow(int& x, int& y);
};

#endif // WINDOWLESS_INSTANCE_H
