/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
#include "modules/dom/src/orientation/compassneedscalibrationevent.h"
#include "modules/device_api/OrientationManager.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/js/window.h"

/* static */ OP_STATUS
DOM_CompassNeedsCalibrationEvent::Make(DOM_CompassNeedsCalibrationEvent*& result, DOM_Runtime* runtime)
{
	result = OP_NEW(DOM_CompassNeedsCalibrationEvent, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(result, runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));
	result->InitEvent(ONCOMPASSNEEDSCALIBRATION, runtime->GetEnvironment()->GetWindow());
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_CompassNeedsCalibrationEvent::DefaultAction(BOOL /* unused_cancelled */)
{
	OP_ASSERT(GetTarget()->IsA(DOM_TYPE_WINDOW));
	g_DAPI_orientation_manager->CompassCalibrationReply(static_cast<JS_Window*>(GetTarget()), prevent_default);
	return OpStatus::OK;
}


#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
