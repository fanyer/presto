 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DEVICEORIENTATIONEVENT_H
#define DOM_DEVICEORIENTATIONEVENT_H

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT

#include "modules/dom/src/domevents/domevent.h"

class DOM_DeviceOrientationEvent
	: public DOM_Event
{
public:
	static OP_STATUS Make(DOM_DeviceOrientationEvent *&event, DOM_Runtime *runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_DEVICEORIENTATION_EVENT || DOM_Event::IsA(type); }

	OP_STATUS InitValues(double alpha, double beta, double gamma, BOOL absolute);
	OP_STATUS InitValues(ES_Value& alpha, ES_Value& beta, ES_Value& gamma, ES_Value& absolute);

	DOM_DECLARE_FUNCTION(initDeviceOrientationEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };

private:
	static void SetNumberOrNull(ES_Value* val, double number);
};

#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
#endif // DOM_DEVICEORIENTATIONEVENT_H
