/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_COMPASSNEEDSCALIBRATIONEVENT_H
#define DOM_COMPASSNEEDSCALIBRATIONEVENT_H

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT

#include "modules/dom/src/domevents/domevent.h"

class DOM_CompassNeedsCalibrationEvent
	: public DOM_Event
{
public:
	static OP_STATUS Make(DOM_CompassNeedsCalibrationEvent*& result, DOM_Runtime* runtime);

private:
	virtual OP_STATUS DefaultAction(BOOL cancelled);
};

#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
#endif // DOM_COMPASSNEEDSCALIBRATIONEVENT_H
