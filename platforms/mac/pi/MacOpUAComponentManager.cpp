/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/pi/OpUAComponentManager.h"
#include "platforms/mac/pi/MacOpSystemInfo.h"

class MacOpUAComponentManager : public OpUAComponentManager
{
public:
	const char * GetOSString(Args &args)
	{
		return ((MacOpSystemInfo*)g_op_system_info)->GetOSStr(args.ua);
	}
};

OP_STATUS OpUAComponentManager::Create(OpUAComponentManager **target_pp)
{
	RETURN_OOM_IF_NULL(*target_pp = OP_NEW(MacOpUAComponentManager, ()));
	return OpStatus::OK;
}
