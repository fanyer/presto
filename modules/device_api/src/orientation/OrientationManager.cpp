/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DAPI_ORIENTATION_MANAGER_SUPPORT

#include "modules/device_api/OrientationManager.h"

#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/windowcommander/src/WindowCommanderManager.h"

#ifdef SELFTEST
#include "modules/device_api/src/orientation/MockOpSensor.h"
#endif // SELFTEST

/*static*/ OP_STATUS OrientationManager::Make(OrientationManager*& new_manager)
{
	OpAutoPtr<OrientationManager> orientation_manager_ap(OP_NEW(OrientationManager, ()));
	RETURN_OOM_IF_NULL(orientation_manager_ap.get());
	RETURN_IF_ERROR(g_pcjs->RegisterListener(orientation_manager_ap.get()));
	new_manager = orientation_manager_ap.release();
	return OpStatus::OK;
}

void OrientationManager::InitSensorData(OpSensorData& data, OpSensorType type)
{
	switch(type)
	{
	case SENSOR_TYPE_ACCELERATION:
	case SENSOR_TYPE_LINEAR_ACCELERATION:
		data.acceleration.x = op_nan(0);
		data.acceleration.y = op_nan(0);
		data.acceleration.z = op_nan(0);
		break;
	case SENSOR_TYPE_ORIENTATION:
		data.orientation.alpha = op_nan(0);
		data.orientation.beta  = op_nan(0);
		data.orientation.gamma = op_nan(0);
		data.orientation.absolute = FALSE;
		break;
	case SENSOR_TYPE_ROTATION_SPEED:
		data.rotation.alpha_speed = op_nan(0);
		data.rotation.beta_speed  = op_nan(0);
		data.rotation.gamma_speed = op_nan(0);
		break;
	default:
		OP_ASSERT(!"Unknown sensor type");
		break;
	}
	data.timestamp = op_nan(0);
}

OrientationManager::OrientationManager()
	: m_orientation_sensor(NULL)
	, m_acceleration_sensor(NULL)
	, m_acceleration_with_gravity_sensor(NULL)
	, m_rotation_speed_sensor(NULL)
#ifdef SELFTEST
	, m_force_use_mock_sensors(FALSE)
#endif //SELFTEST
{
	m_cleanup_timer.SetTimerListener(this);
	m_empty_orientation_event_timer.SetTimerListener(this);
	m_empty_motion_event_timer.SetTimerListener(this);

	InitSensorData(m_last_acceleration_data, SENSOR_TYPE_ACCELERATION);
	InitSensorData(m_last_acceleration_with_gravity_data, SENSOR_TYPE_LINEAR_ACCELERATION);
	InitSensorData(m_last_orientation_data, SENSOR_TYPE_ORIENTATION);
	InitSensorData(m_last_rotation_speed_data, SENSOR_TYPE_ROTATION_SPEED);
}

OrientationManager::~OrientationManager()
{
	Cleanup();
	g_pcjs->UnregisterListener(this);
	OP_ASSERT(!m_orientation_sensor);
	OP_ASSERT(!m_acceleration_sensor);
	OP_ASSERT(!m_acceleration_with_gravity_sensor);
	OP_ASSERT(!m_rotation_speed_sensor);
}

// Overloaded attach to start sensor
OP_STATUS
OrientationManager::AttachOrientationListener(OpOrientationListener* listener)
{
	OP_ASSERT(listener);
	OP_STATUS error;
	if (!IsOrientationSensorInitialized())
	{
		error = StartSensor(SENSOR_TYPE_ORIENTATION);
		if (OpStatus::IsError(error) && error != OpStatus::ERR_NOT_SUPPORTED)
			return error;
	}

	error = OpListenable<OpOrientationListener>::AttachListener(listener);
	if (!IsOrientationSensorInitialized())
		ScheduleEmptyEvent(TRUE);

	if (OpStatus::IsError(error))
		ScheduleCleanup(); // in case anything fails schedule cleanup
	return error;
}

// Overloaded attach to start sensorOrientationManager::G
OP_STATUS
OrientationManager::AttachMotionListener(OpMotionListener* listener)
{
	OP_ASSERT(listener);
	OP_STATUS error = OpStatus::OK;
	while (!IsMotionSensorInitialized())
	{
		error = StartSensor(SENSOR_TYPE_ACCELERATION);
		if (OpStatus::IsError(error) && error != OpStatus::ERR_NOT_SUPPORTED)
			break;

		error = StartSensor(SENSOR_TYPE_LINEAR_ACCELERATION);
		if (OpStatus::IsError(error) && error != OpStatus::ERR_NOT_SUPPORTED)
			break;

		error = StartSensor(SENSOR_TYPE_ROTATION_SPEED);

		break;
	}

	if (OpStatus::IsError(error) && error != OpStatus::ERR_NOT_SUPPORTED)
	{
		ScheduleCleanup(); // in case anything fails schedule cleanup
		return error;
	}

	// If after this motion sensor is still uninitialized then we don't support any motion sensors.
	if (!IsMotionSensorInitialized())
		ScheduleEmptyEvent(FALSE);

	error = OpListenable<OpMotionListener>::AttachListener(listener);
	if (OpStatus::IsError(error))
		ScheduleCleanup(); // in case anything fails schedule cleanup
	return error;
}

OP_STATUS
OrientationManager::DetachOrientationListener(OpOrientationListener* listener)
{
	ScheduleCleanup();
	CompassCalibrationReply(listener, FALSE); // Clean outstanding calibration callback for this listener.
	return OpListenable<OpOrientationListener>::DetachListener(listener);
}

OP_STATUS
OrientationManager::DetachMotionListener(OpMotionListener* listener)
{
	ScheduleCleanup();
	return OpListenable<OpMotionListener>::DetachListener(listener);
}

void
OrientationManager::CompassCalibrationReply(OpOrientationListener* listener, BOOL was_handled)
{
	if (OpStatus::IsSuccess(m_compass_calibration_replies_pending.RemoveByItem(listener)))
	{
		m_compass_calibration_handled = m_compass_calibration_handled || was_handled;
		if (m_compass_calibration_replies_pending.GetCount() == 0 && !m_compass_calibration_handled)
			g_windowCommanderManager->GetSensorCalibrationListener()->OnSensorCalibrationRequest(m_orientation_sensor);
	}
}

OP_STATUS
OrientationManager::StartSensor(OpSensorType type)
{
	if (!IsOrientationEnabled())
		return OpStatus::ERR_NOT_SUPPORTED;

	OpSensor*& sensor = GetSensorForType(type);
	if (sensor)
		return OpStatus::OK;

	OpSensor* tmp_sensor;

#ifdef SELFTEST
	if (m_force_use_mock_sensors)
	{
		tmp_sensor = OP_NEW(MockOpSensor, (type));
		RETURN_OOM_IF_NULL(tmp_sensor);
	}
	else
#endif //SELFTEST
	{
		RETURN_IF_ERROR(OpSensor::Create(&tmp_sensor, type));
	}
	OpAutoPtr<OpSensor> sensor_ap(tmp_sensor);
	RETURN_IF_ERROR(sensor_ap->SetListener(this));
	sensor = sensor_ap.release();
	OpSensorData& data = GetLastSensorDataForType(type);
	InitSensorData(data, type);
	return OpStatus::OK;
}

void
OrientationManager::StopSensor(OpSensorType type)
{
	OpSensor*& sensor = GetSensorForType(type);
	if (!sensor)
		return;
	OP_DELETE(sensor);
	sensor = NULL;
}

OpSensor*&
OrientationManager::GetSensorForType(OpSensorType type)
{
	switch (type)
	{
	case SENSOR_TYPE_LINEAR_ACCELERATION:
		return m_acceleration_sensor;
	case SENSOR_TYPE_ACCELERATION:
		return m_acceleration_with_gravity_sensor;
	case SENSOR_TYPE_ORIENTATION:
		return m_orientation_sensor;
	default:
		OP_ASSERT(!"Unexpected sensor type!");
	case SENSOR_TYPE_ROTATION_SPEED:
		return m_rotation_speed_sensor;
	}
}

OpSensorData&
OrientationManager::GetLastSensorDataForType(OpSensorType type)
{
	switch (type)
	{
	case SENSOR_TYPE_LINEAR_ACCELERATION:
		return m_last_acceleration_data;
	case SENSOR_TYPE_ACCELERATION:
		return m_last_acceleration_with_gravity_data;
	case SENSOR_TYPE_ORIENTATION:
		return m_last_orientation_data;
	default:
		OP_ASSERT(!"Unexpected sensor type!");
	case SENSOR_TYPE_ROTATION_SPEED:
		return m_last_rotation_speed_data;
	}
}

BOOL OrientationManager::ShouldDataTriggerEvent(OpSensorType type, BOOL &orientation)
{
	orientation = FALSE;
	switch (type)
	{
		case SENSOR_TYPE_ORIENTATION:
			orientation = TRUE;
			return TRUE;
		case SENSOR_TYPE_LINEAR_ACCELERATION:
			return TRUE;
		case SENSOR_TYPE_ACCELERATION:
			return !m_acceleration_sensor;
		case SENSOR_TYPE_ROTATION_SPEED:
			return !m_acceleration_sensor && !m_acceleration_with_gravity_sensor;
		default:
			OP_ASSERT(!"unknown sensor type");
			return FALSE;
	}

}

/* virtual */ void
OrientationManager::OnNewData(OpSensor* sensor, const OpSensorData* value)
{
	OP_ASSERT(sensor);
	OP_ASSERT(value);

#if DAPI_DEVICE_ORIENTATION_NOTIFICATION_THRESHOLD > 0
	if (sensor->GetType() == SENSOR_TYPE_ORIENTATION && DAPI_DEVICE_ORIENTATION_NOTIFICATION_THRESHOLD > AngleDiffFull(*value))
		return; // Threshold not exceeded.
#endif

	OpSensorData& last_sensor_data = GetLastSensorDataForType(sensor->GetType());
	double interval = 0.0;
	BOOL is_orientation_event;
	BOOL is_triggering_event = ShouldDataTriggerEvent(sensor->GetType(), is_orientation_event);


	if (is_triggering_event)
	{
		if (op_isnan(last_sensor_data.timestamp))
			is_triggering_event = FALSE;
		else
			interval = value->timestamp - last_sensor_data.timestamp;
	}
	last_sensor_data = *value;
	if (is_triggering_event)
	{
		if (is_orientation_event)
		{
			OpOrientationListener::Data data;
			data.alpha = m_last_orientation_data.orientation.alpha;
			data.beta  = m_last_orientation_data.orientation.beta;
			data.gamma = m_last_orientation_data.orientation.gamma;
			data.absolute = m_last_orientation_data.orientation.absolute;
			NotifyOrientationChange(data);
		}
		else
		{
			OpMotionListener::Data data;
			data.x              = m_last_acceleration_data.acceleration.x;
			data.y              = m_last_acceleration_data.acceleration.y;
			data.z              = m_last_acceleration_data.acceleration.z;

			data.x_with_gravity = m_last_acceleration_with_gravity_data.acceleration.x;
			data.y_with_gravity = m_last_acceleration_with_gravity_data.acceleration.y;
			data.z_with_gravity = m_last_acceleration_with_gravity_data.acceleration.z;

			data.rotation_alpha = m_last_rotation_speed_data.rotation.alpha_speed;
			data.rotation_beta  = m_last_rotation_speed_data.rotation.beta_speed;
			data.rotation_gamma = m_last_rotation_speed_data.rotation.gamma_speed;

			data.interval =  interval;
			NotifyAccelerationChange(data);
		}
	}
}

/* virtual */ void
OrientationManager::OnSensorNeedsCalibration(OpSensor* sensor)
{
	if (sensor->GetType() == SENSOR_TYPE_ORIENTATION)
	{
		if (m_compass_calibration_replies_pending.GetCount() != 0)
			return; // Don't notify listeners if we have previous notification outstanding.
		m_compass_calibration_handled = FALSE;
		if (OpStatus::IsSuccess(m_compass_calibration_replies_pending.DuplicateOf(OpListenable<OpOrientationListener>::GetListeners())))
			NotifyCompassNeedsCalibration();
		return;
	}
	// We dont handle calibraing other sensors or error occured - platform should take care of it by itself.
	g_windowCommanderManager->GetSensorCalibrationListener()->OnSensorCalibrationRequest(sensor);
}

/* virtual */ void
OrientationManager::OnSensorDestroyed(const OpSensor* sensor)
{
	OpSensor*& member_sensor = GetSensorForType(sensor->GetType());
	member_sensor = NULL;
}

void
OrientationManager::ScheduleCleanup()
{
	m_cleanup_timer.Start(1000);
}

/* virtual */ void
OrientationManager::OnTimeOut(OpTimer* timer)
{
	if (timer == &m_cleanup_timer)
		Cleanup();
	if (timer == &m_empty_orientation_event_timer)
	{
		OpOrientationListener::Data data;
		data.alpha = op_nan(0);
		data.beta  = op_nan(0);
		data.gamma = op_nan(0);
		data.absolute = FALSE;
		NotifyOrientationChange(data);
	}
	if (timer == &m_empty_motion_event_timer)
	{
		OpMotionListener::Data data;
		data.x              = op_nan(0);
		data.y              = op_nan(0);
		data.z              = op_nan(0);

		data.x_with_gravity = op_nan(0);
		data.y_with_gravity = op_nan(0);
		data.z_with_gravity = op_nan(0);

		data.rotation_alpha = op_nan(0);
		data.rotation_beta  = op_nan(0);
		data.rotation_gamma = op_nan(0);

		data.interval =  op_nan(0);
		NotifyAccelerationChange(data);
	}
}

void
OrientationManager::Cleanup()
{
	if (OpListenable<OpMotionListener>::GetListeners().GetCount() == 0)
	{
		StopSensor(SENSOR_TYPE_ACCELERATION);
		StopSensor(SENSOR_TYPE_LINEAR_ACCELERATION);
		StopSensor(SENSOR_TYPE_ROTATION_SPEED);
	}

	if (OpListenable<OpOrientationListener>::GetListeners().GetCount() == 0)
	{
		StopSensor(SENSOR_TYPE_ORIENTATION);
	}
}

#if DAPI_DEVICE_ORIENTATION_NOTIFICATION_THRESHOLD > 0
double
OrientationManager::AngleDiff(double last, double first, double period)
{
	double diff = op_fabs(last - first);
	if (diff > period /2)
		diff = period - diff;
	return diff;
}

double
OrientationManager::AngleDiffFull(const OpSensorData& cur_data)
{
	double delta_alpha = AngleDiff(cur_data.orientation.alpha, m_last_orientation_data.orientation.alpha, 360.0);
	double delta_beta  = AngleDiff(cur_data.orientation.beta, m_last_orientation_data.orientation.beta, 360.0);
	double delta_gamma = AngleDiff(cur_data.orientation.gamma, m_last_orientation_data.orientation.gamma, 180.0);
	if (op_fabs(cur_data.orientation.gamma - m_last_orientation_data.orientation.gamma) > 90) // correction for the situation when roll 'overflows'.
	{
		delta_alpha = op_fmod(delta_alpha + 180.0, 360.0);
		delta_beta = op_fmod(delta_beta + 180.0, 360.0);
	}
	return delta_alpha + delta_beta + delta_gamma;
}
#endif // DAPI_DEVICE_ORIENTATION_NOTIFICATION_THRESHOLD > 0

void OrientationManager::ScheduleEmptyEvent(BOOL orientation)
{
	if (orientation)
		m_empty_orientation_event_timer.Start(1);
	else
		m_empty_motion_event_timer.Start(1);
}

/* virtual */ void
OrientationManager::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	OP_ASSERT(id == OpPrefsCollection::JS);
	if (pref == PrefsCollectionJS::EnableOrientation)
	{
		if (newvalue)
		{
			if (OpListenable<OpMotionListener>::GetListeners().GetCount() > 0)
			{
				StartSensor(SENSOR_TYPE_ACCELERATION);
				StartSensor(SENSOR_TYPE_LINEAR_ACCELERATION);
				StartSensor(SENSOR_TYPE_ROTATION_SPEED);
			}

			if (OpListenable<OpOrientationListener>::GetListeners().GetCount() > 0)
			{
				StartSensor(SENSOR_TYPE_ORIENTATION);
			}
		}
		else
		{
			if (IsMotionSensorInitialized())
			{
				StopSensor(SENSOR_TYPE_ACCELERATION);
				StopSensor(SENSOR_TYPE_LINEAR_ACCELERATION);
				StopSensor(SENSOR_TYPE_ROTATION_SPEED);
				ScheduleEmptyEvent(FALSE);
			}
			if (IsOrientationSensorInitialized())
			{
				StopSensor(SENSOR_TYPE_ORIENTATION);
				ScheduleEmptyEvent(TRUE);
			}
		}
	}
}


BOOL
OrientationManager::IsOrientationEnabled()
{
	return g_pcjs->GetIntegerPref(PrefsCollectionJS::EnableOrientation);
}

#ifdef SELFTEST
void
OrientationManager::TriggerCompassNeedsCalibration()
{
	if (m_orientation_sensor)
		OnSensorNeedsCalibration(m_orientation_sensor);
}
#endif // SELFTEST

#endif // DAPI_ORIENTATION_MANAGER_SUPPORT
