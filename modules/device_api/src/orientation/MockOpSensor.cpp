/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include <core/pch.h>
#ifdef PI_SENSOR
#if defined USE_MOCK_OP_SENSOR_IMPL || defined SELFTEST

#include "modules/device_api/src/orientation/MockOpSensor.h"
#include "modules/pi/OpSystemInfo.h"

#ifdef OPERA_CONSOLE
# include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE

MockOpSensor::MockOpSensor(OpSensorType type)
	: m_listener(NULL)
	, m_poll_interval(DEFAULT_POLL_INTERVAL)
	, m_type(type)
{
	m_timer.SetTimerListener(this);
}

MockOpSensor::~MockOpSensor()
{
	if (m_listener)
		m_listener->OnSensorDestroyed(this);
	SetListener(NULL);
}

/* virtual */ OP_STATUS
MockOpSensor::SetListener(OpSensorListener* listener)
{
	if (listener && !m_listener)
		m_timer.Start(static_cast<UINT32>(op_floor(m_poll_interval)));
	else if(!listener)
		m_timer.Stop();
	m_listener = listener;
	return OpStatus::OK;
}

/* virtual */ void
MockOpSensor::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(&m_timer == timer);
	OpSensorData data;
	GenerateData(data);
	data.timestamp = g_op_time_info->GetWallClockMS();
	OP_ASSERT(m_listener);
	m_listener->OnNewData(this, &data);
	m_timer.Start(static_cast<UINT32>(op_floor(m_poll_interval)));
}

void
MockOpSensor::GenerateData(OpSensorData &data)
{
	switch (GetType())
	{
	case SENSOR_TYPE_ACCELERATION:
		data.acceleration.x = op_sin(g_op_time_info->GetRuntimeMS()/1000);
		data.acceleration.y = op_sin(g_op_time_info->GetRuntimeMS()/3100);
		data.acceleration.z = op_sin(g_op_time_info->GetRuntimeMS()/2100);
		break;
	case SENSOR_TYPE_LINEAR_ACCELERATION:
		data.acceleration.x = op_sin(g_op_time_info->GetRuntimeMS()/1000);
		data.acceleration.y = op_sin(g_op_time_info->GetRuntimeMS()/3100);
		data.acceleration.z = op_sin(g_op_time_info->GetRuntimeMS()/2100) - 9.81;
		break;
	case SENSOR_TYPE_ORIENTATION:
		data.orientation.alpha = op_fmod(g_op_time_info->GetRuntimeMS() / 100, 360);
		data.orientation.beta  = op_fmod(g_op_time_info->GetRuntimeMS() / 100, 360) - 180;
		data.orientation.gamma = op_fmod(g_op_time_info->GetRuntimeMS() / 100, 180) - 90;
		data.orientation.absolute = TRUE;
		break;
	case SENSOR_TYPE_ROTATION_SPEED:
		data.orientation.alpha = 1;
		data.orientation.beta  = 2;
		data.orientation.gamma = -1;
		break;

	}
}

#endif // defined USE_MOCK_OP_SENSOR_IMPL || defined SELFTEST

#ifdef USE_MOCK_OP_SENSOR_IMPL

OP_STATUS OpSensor::Create(OpSensor** sensor, OpSensorType type)
{
	OP_ASSERT(sensor);
	*sensor = OP_NEW(MockOpSensor, (type));
	RETURN_OOM_IF_NULL(*sensor);
	return OpStatus::OK;
}

#endif // USE_MOCK_OP_SENSOR_IMPL

#endif // PI_SENSOR
