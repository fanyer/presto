/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Petter Nilsen (pettern)
 */

#include "core/pch.h"

#ifdef PI_POWER_STATUS

#include "adjunct/desktop_pi/DesktopOpPowerStatus.h"

/* static */
OP_STATUS OpPowerStatus::Create(OpPowerStatus** new_power_status, OpPowerStatusMonitor* monitor)
{
	*new_power_status = OP_NEW(DesktopOpPowerStatus, ());
	if(!*new_power_status)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

#endif // PI_POWER_STATUS

