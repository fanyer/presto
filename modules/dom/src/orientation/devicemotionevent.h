 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DEVICEMOTIONEVENT_H
#define DOM_DEVICEMOTIONEVENT_H

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT

#include "modules/dom/src/domevents/domevent.h"

class DOM_DeviceMotionEvent
	: public DOM_Event
{
public:
	static OP_STATUS Make(DOM_DeviceMotionEvent *&event, DOM_Runtime *runtime);
	static OP_STATUS Make(DOM_DeviceMotionEvent*& event, DOM_Runtime* runtime
	                      , double x, double y, double z
	                      , double x_with_grav, double y_with_grav, double z_with_grav
	                      , double alpha, double beta, double gamma
	                      , double interval);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_DEVICEMOTION_EVENT || DOM_Event::IsA(type); }

	static OP_STATUS MakeAcceleration(ES_Object*& obj, DOM_Runtime* runtime, double x, double y, double z);
	static OP_STATUS MakeRotation(ES_Object*& obj, DOM_Runtime* runtime, double alpha, double beta, double gamma);

	OP_STATUS InitValues(ES_Object* acceleration, ES_Object* acceleration_with_gravity, ES_Object* rotation_rate, ES_Value& interval);

	DOM_DECLARE_FUNCTION(initAccelerometerEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };

private:
	static void SetNumberOrNull(ES_Value* val, double number);
	static void GetNameOrUndefined(DOM_Runtime* runtime, ES_Object* object, const uni_char* property_name, ES_Value* value);
};

#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
#endif // DOM_DEVICEMOTIONEVENT_H
