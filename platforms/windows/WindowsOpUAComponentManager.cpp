/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "platforms/windows_common/pi_impl/windowsuacomponentmanager.h"

OP_STATUS OpUAComponentManager::Create(OpUAComponentManager **target_pp)
{
	OpAutoPtr<WindowsUAComponentManager> manager(OP_NEW(WindowsUAComponentManager, ()));
	RETURN_OOM_IF_NULL(manager.get());
	RETURN_IF_ERROR(manager->Construct());
	*target_pp = manager.release();
	return OpStatus::OK;
}
