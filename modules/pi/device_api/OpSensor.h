/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifndef PI_DEVICE_API_OPSENSOR_H
#define PI_DEVICE_API_OPSENSOR_H

#ifdef PI_SENSOR

class OpSensor;

/** Enumeraterated types of sensors
 */
enum OpSensorType
{
	/** Sensor for raw acceleration(gravity + device acceleration).
	 *  data in OpSensorData::acceleration.
	 */
	SENSOR_TYPE_ACCELERATION,

	/** Sensor for linear acceleration(without gravity vector).
	 *  data in OpSensorData::acceleration.
	 */
	SENSOR_TYPE_LINEAR_ACCELERATION,

	/** Sensor for device orientation.
	 *  data in OpSensorData::acceleration.
	 */
	SENSOR_TYPE_ORIENTATION,

	/** Sensor for device angular speed(gyroscope).
	 *  data in OpSensorData::rotation.
	 */
	SENSOR_TYPE_ROTATION_SPEED,

	/// ...
	/// possible other types like BATTERY, PROXIMITY, LUMINANCE, TEMPERATURE
	/// not needed now but might be needed if we want to implement:
	/// http://dev.w3.org/2009/dap/system-info/
};

/** Structure containing data returned by
 *  the acceleration sensor.
 *  Sensor types which use this struct:
 *   - SENSOR_TYPE_ACCELERATION
 *   - SENSOR_TYPE_LINEAR_ACCELERATION
 *
 * The axes are anchored at the device with the x axis pointing to the
 * right of the device (the direction in which x values increase on the
 * screen), y axis pointing upwards and z axis perpendicular to both x and
 * y axes, with positive values towards the front of the screen.
 * See http://dev.w3.org/geo/api/spec-source-orientation.html for illustration.
 *
 * All values are in SI units (m/s^2) and measure the acceleration applied
 * to the device.
 * Examples:
 * When the device lies flat on a table, screen up, the gravitational
 * acceleration causes the accelerometer to report value -9.81 m/s^2
 * along the z axis.
 *
 * When the device lies flat on a table and is pushed on its left side
 * towards the right, the x acceleration value is negative.
 *
 * When the device lies flat on a table and is pushed on its bottom side
 * towards the top, the y acceleration value is negative.
 *
 * When the device lies flat on a table and is pushed toward the sky
 * with an acceleration of A m/s^2 (which is negative), the reported
 * value is equal to:
 *  - for SENSOR_TYPE_ACCELERATION: A - 9.81 m/s^2
 *  - for SENSOR_TYPE_LINEAR_ACCELERATION: A m/s^2
 *
 * If any of the axes is not supported by the platform, then the value
 * corresponding to this axis must be set to NaN.
 */
struct OpAccelerationData
{
	double x; //< x axis value.
	double y; //< y axis value.
	double z; //< z axis value.
};

/** Structure containing data returned by
 *  the orientation sensor.
 *  Sensor types which use this struct:
 *   - SENSOR_TYPE_ORIENTATION
 *
 *  x,y,z axes referred are the same as described in OpAccelerometerData.
 *  All the values are in degrees.
 *  If any of the values  is not supported by the platform, then the value
 *  corresponding to this value must be set to NaN.
 */
struct OpOrientationData
{
	/** Angle between North (or some other reference direction when absolute is false)
	 *  and the y-axis [0 to 360). 0=North, 90=West, 180=South, 270=East when absolute is true.
	 *
	 *  When absolute is true, the reference direction is North; ideally true North
	 *  should be used but an implementation with a reasonable approximation (e.g.
	 *  magnetic North) may use that instead.  Otherwise, e.g. when using gyroscope
	 *  and accelerometer, an arbitrary fixed reference direction may be used and
	 *  absolute should be set false.
	 */
	double alpha;
	/** Rotation around x-axis [-180 to 180), with positive values when the y-axis moves toward the z-axis. */
	double beta;
	/** Rotation around y-axis (-90 to 90), with positive values when the z-axis moves toward the x-axis. */
	double gamma;
	/** If absolute is false then the underlying sensor can't obtain absolute values of alpha, beta and gamma,
	 *  but can instead provide those values in relation to some other (unknown) frame of reference. */
	bool absolute;
};

/** Structure containing data returned by
 *  the angular speed sensor(gyroscope).
 *  Sensor types which use this struct:
 *   - SENSOR_TYPE_ROTATION_SPEED
 *
 *  the angles of the rotationas are the same as in OpOrientationData.
 *  All the values are in degrees/s.
 *  If any of the values  is not supported by the platform, then the value
 *  corresponding to this value must be set to NaN.
 */
struct OpRotationData
{
	double alpha_speed; //< rotation speed around axis perpendicular to north.
	double beta_speed;  //< rotation around x-axis
	double gamma_speed; //< rotation around y-axis
};

/** Structure containing data
 *  returned by the sensor.
 */
struct OpSensorData
{
	union
	{
		OpAccelerationData acceleration;
		OpRotationData rotation;
		OpOrientationData orientation;
	};
	double timestamp; //< utc millisecond timestamp. Same format as OpSystemInfo::GetWallClockMS.
};


/** Interface for receiving notifications about sensor data.
 */
class OpSensorListener
{
public:
	virtual ~OpSensorListener() {};
	/** Callback called by the sensor when updated data becomes available.
	 *  @param sensor sensor which sent this event.
	 *  @param value last value of the sensor.
	 */
	virtual void OnNewData(OpSensor* sensor, const OpSensorData* value) = 0;

	/** Callback called by the sensor when it requires calibration.
	 *  Should only be called if the underlying implementation determines that
	 *  calibration could improve accuracy of the sensor readings.
	 *  The listener can respond either by calibrating the sensor on its own or by
	 *  delegating to OpSensorCalibrationListener from WindowCommanderManager.
	 */
	virtual void OnSensorNeedsCalibration(OpSensor* sensor) = 0;

	/** Callback called by the sensor when it is being destroyed.
	 *  The listener should not perform any calls on this sensor after
	 *  receiving this callback.
	 *  @param sensor sensor which sent this event.
	 */
	virtual void OnSensorDestroyed(const OpSensor* sensor) = 0;
};

/** @short Generic interface for physical data sensor.
 */
class OpSensor
{
public:
	virtual ~OpSensor() { }
	/** Constructs a new sensor of a given type.
	 *  @param new_sensor set to a newly constructed sensor object.
	 *  @param type a type of a sensor to construct.
	 *  @return OK for success or any relevant error code if failed. Particularly:
	 *  - ERR_NO_MEMORY - in case of OOM
	 *  - ERR_NOT_SUPPORTED - in case the platform or device doesn't suport this sensor.
	 *  - ERR_NO_ACCESS - access to the sensor has been denied by platform
	 */
	static OP_STATUS Create(OpSensor** new_sensor, OpSensorType type);

	/** Sets listener to be notified of sensor events.
	 *
	 *  In order to preserve resources, implementations are advised to
	 *  enable the physical sensor only when a listener for it is set and
	 *  to disable it when the listener is removed (by passing NULL as an
	 *  argument).
	 *
	 *  When a listener is already set and the method is called again, the
	 *  new listener is set (it overrides the old one).
	 *
	 *  @param listener Listener for the sensor. Pass NULL to remove the listener.
	 *  @return OK for success ERR_NO_MEMORY on OOM or ERR in case of any other error.
	 */
	virtual OP_STATUS SetListener(OpSensorListener* listener) = 0;

	/** Gets The type of the sensor. */
	virtual OpSensorType GetType() const = 0;
};

#endif // PI_SENSOR

#endif // PI_DEVICE_API_OPSENSOR_H
