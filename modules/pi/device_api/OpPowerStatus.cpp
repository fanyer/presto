/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include <core/pch.h>

#ifdef PI_POWER_STATUS

#include "modules/pi/device_api/OpPowerStatus.h"

/* static */ OP_STATUS
OpPowerStatusMonitor::Create(OpPowerStatusMonitor** new_power_monitor)
{
	OP_ASSERT(new_power_monitor);
	OpPowerStatusMonitor* jil_monitor = OP_NEW(OpPowerStatusMonitor, ());
	RETURN_OOM_IF_NULL(jil_monitor);
	OP_STATUS error = OpPowerStatus::Create(&jil_monitor->m_power_status, jil_monitor);
	if(OpStatus::IsError(error))
		OP_DELETE(jil_monitor);
	else
		*new_power_monitor = jil_monitor;
	return error;
}

OpPowerStatusMonitor::~OpPowerStatusMonitor()
{
	OP_DELETE(m_power_status);
}

/* virtual */ OP_STATUS
OpPowerStatusMonitor::AddListener(OpPowerStatusListener* listener)
{
	return m_listeners.Add(listener);
}

/* virtual */ OP_STATUS
OpPowerStatusMonitor::RemoveListener(OpPowerStatusListener* listener)
{
	return m_listeners.RemoveByItem(listener);
}

/* virtual */ void
OpPowerStatusMonitor::OnWakeUp(OpPowerStatus* power_status)
{
	for (UINT32 i=0; i<m_listeners.GetCount(); i++)
		m_listeners.Get(i)->OnWakeUp(power_status);
}

/* virtual */ void
OpPowerStatusMonitor::OnChargeLevelChange(OpPowerStatus* power_status, BYTE new_charge_level)
{
	for (UINT32 i=0; i<m_listeners.GetCount(); i++)
		m_listeners.Get(i)->OnChargeLevelChange(power_status, new_charge_level);
}

/* virtual */ void
OpPowerStatusMonitor::OnPowerSourceChange(OpPowerStatus* power_status, PowerSupplyType power_source)
{
	for (UINT32 i=0; i<m_listeners.GetCount(); i++)
		m_listeners.Get(i)->OnPowerSourceChange(power_status, power_source);
}

/* virtual */
void OpPowerStatusMonitor::OnLowPowerStateChange(OpPowerStatus* power_status, BOOL is_low)
{
	for (UINT32 i=0; i<m_listeners.GetCount(); i++)
		m_listeners.Get(i)->OnLowPowerStateChange(power_status, is_low);
}
#endif // PI_POWER_STATUS

