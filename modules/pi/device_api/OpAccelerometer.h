/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifndef PI_DEVICE_API_OPACCELEROMETER_H
#define PI_DEVICE_API_OPACCELEROMETER_H

#ifdef PI_ACCELEROMETER

/** @short Provides information about accelerometer.
 *
 * @deprecated Please use the OpSensor interface instead.
 */
class OpAccelerometer
{
public:

	virtual ~OpAccelerometer() { }

	static OP_STATUS Create(OpAccelerometer** new_accelerometer);

	/** Gets three axes of device acceleration in m/s^2.
	 *
	 * The axes are anchored at the device with the x axis pointing to the
	 * right of the device (the direction in which x values increase on the
	 * screen), y axis pointing upwards and z axis perpendicular to both x and
	 * y axes, with positive values towards the front of the screen.
	 *
	 * All values are in SI units (m/s^2) and measure the acceleration applied
	 * to the device.
	 *
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
	 * value is equal to A - 9.81 m/s^2.
	 *
	 * If any of the axes is not supported by the platform, then the value
	 * corresponding to this axis must be set to NaN.
	 *
	 * @return - any valid OpStatus. Should return ERR_NOT_SUPPORTED if the device
	 * doesn't have an accelerometer.
	 */
	virtual OP_STATUS GetCurrentData(double* x, double* y, double* z) = 0;
};

#endif // PI_ACCELEROMETER

#endif // PI_DEVICE_API_OPACCELEROMETER_H
