/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)

#include "platforms/x11api/plugins/toolkits/toolkit_loop.h"

#include "platforms/x11api/plugins/toolkits/gtk2_toolkit_loop.h"

ToolkitLoop* ToolkitLoop::Create(Toolkit toolkit, int read_fd, int write_fd)
{
	OpAutoPtr<ToolkitLoop> new_loop;
	switch (toolkit)
	{
		case GTK2:
			new_loop.reset(OP_NEW(Gtk2ToolkitLoop, ()));
			break;
	}

	if (!new_loop.get())
		return NULL;

	RETURN_VALUE_IF_ERROR(new_loop->Init(read_fd, write_fd), NULL);
	return new_loop.release();
}

#endif // X11API && NS4P_COMPONENT_PLUGINS
