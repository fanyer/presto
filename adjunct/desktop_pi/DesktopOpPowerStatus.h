/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Petter Nilsen (pettern)
 */

#ifndef DESKTOP_OP_POWERSTATUS_H
#define DESKTOP_OP_POWERSTATUS_H

#ifdef PI_POWER_STATUS
#include "modules/pi/device_api/OpPowerStatus.h"

/** @brief OpPowerStatus functions needed in all Desktop implementations. It's a dummy implementation neede by geolocation
  */
class DesktopOpPowerStatus : public OpPowerStatus
{
public:
	virtual OP_STATUS IsExternalPowerConnected(BOOL* is_connected) { *is_connected = TRUE; return OpStatus::OK; }
	virtual OP_STATUS GetBatteryCharge(BYTE* charge) { *charge = 255; return OpStatus::OK; }
	virtual OP_STATUS IsLowPowerState(BOOL* is_in_low_power_state) { *is_in_low_power_state = FALSE; return OpStatus::OK; }
};

#endif // PI_POWER_STATUS
#endif // DESKTOP_OP_POWERSTATUS_H
