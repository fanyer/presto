/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef NO_CORE_COMPONENTS

#include "platforms/mac/pluginwrapper/pluginwrapper_systeminfo.h"

#include "platforms/mac/util/macutils_mincore.h"

OP_STATUS OpSystemInfo::Create(OpSystemInfo** new_systeminfo)
{
	*new_systeminfo = OP_NEW(PluginSystemInfo, ());
	return *new_systeminfo ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

double PluginSystemInfo::GetRuntimeMS()
{
	return GetMonotonicMS();
}

#endif // NO_CORE_COMPONENTS
