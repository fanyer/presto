/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/olddebug/olddebug_module.h"

#ifdef OLDDEBUG_MODULE_REQUIRED

void CloseDebugFiles();

OlddebugModule::OlddebugModule()
{
}

void OlddebugModule::InitL(const OperaInitInfo& info)
{
}

void OlddebugModule::Destroy()
{
	CloseDebugFiles();
}

#endif // OLDDEBUG_MODULE_REQUIRED
