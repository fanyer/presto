/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_DESKTOP_UTILS_H
#define SCOPE_DESKTOP_UTILS_H

#include "modules/scope/src/scope_service.h"
#include "adjunct/desktop_scope/src/generated/g_scope_desktop_utils_interface.h"

#include "modules/util/opfile/opfolder.h"

class OpScopeDesktopUtils :
	public OpScopeDesktopUtils_SI
{
public:

	OpScopeDesktopUtils();
	virtual ~OpScopeDesktopUtils();

	// Request/Response functions
	OP_STATUS DoGetString(const DesktopStringID &in, DesktopStringText &out);
	OP_STATUS DoGetOperaPath(DesktopPath &out);
	OP_STATUS DoGetLargePreferencesPath(DesktopPath &out);
	OP_STATUS DoGetSmallPreferencesPath(DesktopPath &out);
	OP_STATUS DoGetCachePreferencesPath(DesktopPath &out);
	OP_STATUS DoGetCurrentProcessId(DesktopPid &out);
};

#endif // SCOPE_HELLO_DESKTOP_H
