/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DEVICE_API_ORIENTATION_MANAGER_H
#define DEVICE_API_ORIENTATION_MANAGER_H

#ifdef DAPI_ORIENTATION_MANAGER_SUPPORT

#include "modules/device_api/OpListenable.h"
#include "modules/pi/device_api/OpSensor.h"

#include "modules/hardcore/timer/optimer.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"

class ST_device_j7uhsb_ionmanager;

/** Listener interface for orientation events */
class OpOrientationListener
{
public:
	/** @see OpOrientationData. */
	struct Data{
		double alpha;
		double beta;
		double gamma;
		bool absolute;
	};
	/** Called when there is a significant change in devices physical orientation.
	 */
	virtual void OnOrientationChange(const Data& data) = 0;

	/** Called whenever the sensor notifies about need of calibration.
	 *  The Listener MUST respond by calling OrientationManager::CompassCalibrationReply().
	 *  was_handled parameter must be set to TRUE if the listener handles calibration
	 *  and doesn't want platform to provide the default calibration method. Else
	 *  if the listeners doesn't want to prevent this default behaviour it should
	 *  call OrientationManager::CompassCalibrationReply() with was_handled == FALSE.
	 */
	virtual void OnCompassNeedsCalibration() = 0;
	virtual ~OpOrientationListener() {}
};

/** Listener interface for motion events */
class OpMotionListener
{
public:
	struct Data{
		double x;
		double y;
		double z;
		double x_with_gravity;
		double y_with_gravity;
		double z_with_gravity;
		double rotation_alpha;
		double rotation_beta;
		double rotation_gamma;
		double interval;
	};
	virtual void OnAccelerationChange(const Data& data) = 0;
	virtual ~OpMotionListener() {}
};


/**
 * Utility class responsible for Handling the lifetime of platrorm
 * sensors and forwarding the orientation/motion data to listeners.
 */
class OrientationManager :
	  public OpListenable<OpOrientationListener>
	, public OpListenable<OpMotionListener>
	, public OpSensorListener
	, public OpTimerListener
	, public OpPrefsListener
{
public:
	static OP_STATUS Make(OrientationManager*& new_manager);
	virtual ~OrientationManager();

	/** Attaches the listener for Orientation events.
	 *  Attaching the first listener will try to enable
	 *  platform sensor implementation.
	 */
	OP_STATUS AttachOrientationListener(OpOrientationListener* listener);

	/** Attaches the listener for Motion events.
	 *  Attaching the first listener will try to enable
	 *  platform sensor implementation.
	 */
	OP_STATUS AttachMotionListener(OpMotionListener* listener);

	/** Detaches the listener for Orientation events.
	 *  Detaching the last listener will schedule disabling
	 *  the platform sensors asynchronously.
	 */
	OP_STATUS DetachOrientationListener(OpOrientationListener* listener);

	/** Detaches the listener for Motion events.
	 *  Detaching the last listener will schedule disabling
	 *  the platform sensors asynchronously.
	 */
	OP_STATUS DetachMotionListener(OpMotionListener* listener);

	/** Called by OpOrientationListener as a reply to OnCompassNeedsCalibration.
	 *
	 *  @note If this function is called with listener parameter which is not
	 *        a listener expected to call this then it will be ignored. For
	 *        example it is always safe to call:
	 *        g_DAPI_orientationManager->CompassCalibrationReply(listener, FALSE);
	 *        when unataching/destroying listener to make sure there is no
	 *        outstanding request.
	 *  @param listener listener which responds to OnCompassNeedsCalibration.
	 *  @param was_handled - if TRUE then the listener has successfully handled
	 *         calibration and OpSensorCalibrationListener::OnSensorCalibrationRequest()
	 *         should not be called to perform calibration.
	 */
	void CompassCalibrationReply(OpOrientationListener* listener, BOOL was_handled);

#ifdef SELFTEST
	/** Simulates compass needs calibration.
	  * For debugging purposes only.
	  */
	void TriggerCompassNeedsCalibration();
#endif // SELFTEST

private:
	// from OpSensorListener
	virtual void OnNewData(OpSensor* sensor, const OpSensorData* value);
	virtual void OnSensorNeedsCalibration(OpSensor* sensor);
	virtual void OnSensorDestroyed(const OpSensor* sensor);

	// from OpTimerListener
	virtual void OnTimeOut(OpTimer* timer);

	// from OpPrefsListener
	virtual void PrefChanged(OpPrefsCollection::Collections /* id */, int /* pref */, int /* newvalue */);

	OrientationManager();

	DECLARE_LISTENABLE_EVENT1(OpOrientationListener, OrientationChange, const OpOrientationListener::Data&);
	DECLARE_LISTENABLE_EVENT0(OpOrientationListener, CompassNeedsCalibration);
	DECLARE_LISTENABLE_EVENT1(OpMotionListener, AccelerationChange, const OpMotionListener::Data);

	BOOL IsOrientationSensorInitialized() { return m_orientation_sensor ? TRUE : FALSE; }
	BOOL IsMotionSensorInitialized() { return m_acceleration_sensor || m_acceleration_with_gravity_sensor || m_rotation_speed_sensor; }

	BOOL IsOrientationEnabled();

	/** Checks whether platform sensor event should trigger
	 *  OrientationManager event. The algorithm for this is:
	 *  1) if this is orientation sensor event then return TRUE.
	 *  2) else if it is most important of available motion sensors return TRUE.
	 *   (the precedence of sensors is : LINEAR_ACCELERATION > ACCELERATION(with gravity) > ROTATION)
	 *  3) else return FALSE.
	 *  @param type - type of a sensor event.
	 *  @param[out] orientation - set to TRUE if this is orientation event.
	 */
	BOOL ShouldDataTriggerEvent(OpSensorType type, BOOL &orientation);

	/** Calculates the difference between two angles(or any ther periodic value).
	 *  This includes periodicity so for example the diff between
	 *  350 and 10 deg is correctly 20 deg.
	 * @param last last angle
	 * @param first first angle
	 * @param period - the period above which the values overflow.
	 */
	static double AngleDiff(double last, double first, double period);
	double AngleDiffFull(const OpSensorData& cur_data);

	OP_STATUS StartSensor(OpSensorType type);
	void StopSensor(OpSensorType type);
	/// Gets the reference to a sensor for a given type
	OpSensor*& GetSensorForType(OpSensorType type);
	/// Gets the reference to the last cached sensor data for a given type
	OpSensorData& GetLastSensorDataForType(OpSensorType type);

	OpSensor* m_orientation_sensor;
	OpSensor* m_acceleration_sensor;
	OpSensor* m_acceleration_with_gravity_sensor;
	OpSensor* m_rotation_speed_sensor;
	OpSensorData m_last_acceleration_data;
	OpSensorData m_last_acceleration_with_gravity_data;
	OpSensorData m_last_orientation_data;
	OpSensorData m_last_rotation_speed_data;

	void InitSensorData(OpSensorData& data, OpSensorType type);

	void ScheduleEmptyEvent(BOOL orientation);
	void ScheduleCleanup();
	void Cleanup();

	OpTimer m_cleanup_timer;
	OpTimer m_empty_orientation_event_timer;
	OpTimer m_empty_motion_event_timer;
#ifdef SELFTEST
	friend class ST_device_j7uhsb_ionmanager;
	/// If this flag is set the orientation manager will always use Mock PI instead of normal ones
	BOOL m_force_use_mock_sensors;
#endif //SELFTEST

	OpVector<void> m_compass_calibration_replies_pending;
	BOOL m_compass_calibration_handled;
};

#endif // DAPI_ORIENTATION_MANAGER_SUPPORT

#endif //DEVICE_API_ORIENTATION_MANAGER_H
