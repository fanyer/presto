/** -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
  *
  * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
  *
  * This file is part of the Opera web browser.  It may not be distributed
  * under any circumstances.
  */

#include "core/pch.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"

#if !defined(USE_UTIL_IPADDR)

/**
 * Most existing code expects a global function to get the local IP address
 */
OpString* GetSystemIp(OpString* pIp)
{
	OP_STATUS status = ((DesktopOpSystemInfo*)g_op_system_info)->GetSystemIp(*pIp);
	if (OpStatus::IsMemoryError(status))
		return NULL;
	return pIp;
}

#endif // !USE_UTIL_IPADDR
