/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */
#ifndef DEVICE_API_MOCKOPSENSOR_H
#define DEVICE_API_MOCKOPSENSOR_H

#ifdef PI_SENSOR
#if defined USE_MOCK_OP_SENSOR_IMPL || defined SELFTEST

#include "modules/pi/device_api/OpSensor.h"
#include "modules/hardcore/timer/optimer.h"

class MockOpSensor : public OpSensor, public OpTimerListener
{
public:
	MockOpSensor(OpSensorType type);
	~MockOpSensor();

	virtual OP_STATUS SetListener(OpSensorListener* listener);
	virtual OpSensorType GetType() const { return m_type; }

	virtual void OnTimeOut(OpTimer* timer);
private:
	friend class OpSensor;
	void GenerateData(OpSensorData &data);
	enum { DEFAULT_POLL_INTERVAL = 100 }; // 100 ms

	OpSensorListener* m_listener;
	OpTimer m_timer;
	double m_poll_interval;
	OpSensorType m_type;
};

#endif // defined USE_MOCK_OP_SENSOR_IMPL || defined SELFTEST
#endif // PI_SENSOR

#endif // DEVICE_API_MOCKOPSENSOR_H
