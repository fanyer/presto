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

#include "platforms/unix/product/pluginwrapper/plugin_systeminfo.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"

OP_STATUS OpSystemInfo::Create(OpSystemInfo** new_systeminfo)
{
	*new_systeminfo = OP_NEW(PluginSystemInfo, ());
	return *new_systeminfo ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

double PluginSystemInfo::GetRuntimeMS()
{
#undef MONOTONIC_TIMER
#ifdef CLOCK_MONOTONIC_FAST
# define MONOTONIC_TIMER CLOCK_MONOTONIC_FAST
#elif defined(CLOCK_MONOTONIC)
# define MONOTONIC_TIMER CLOCK_MONOTONIC
#endif
	struct timespec now;
	if (clock_gettime(MONOTONIC_TIMER, &now) != 0)
		return 0;

	return now.tv_sec * 1e3 + now.tv_nsec * 1e-6;
}

#endif // NO_CORE_COMPONENTS
