/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
#include "modules/dom/src/orientation/deviceorientationevent.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/doc/frm_doc.h"

/* static */ OP_STATUS
DOM_DeviceOrientationEvent::Make(DOM_DeviceOrientationEvent *&event, DOM_Runtime *runtime)
{
	event = OP_NEW(DOM_DeviceOrientationEvent, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(event, runtime,
			runtime->GetPrototype(DOM_Runtime::DEVICEORIENTATION_EVENT_PROTOTYPE), "DeviceOrientationEvent")
			);

	event->InitEvent(ONDEVICEORIENTATION, runtime->GetEnvironment()->GetWindow(), NULL);
	return OpStatus::OK;
}

/* static */ void
DOM_DeviceOrientationEvent::SetNumberOrNull(ES_Value* val, double number)
{
	if (op_isnan(number))
		DOMSetNull(val);
	else
		DOMSetNumber(val, number);
}

OP_STATUS
DOM_DeviceOrientationEvent::InitValues(double alpha, double beta, double gamma, BOOL absolute)
{
	ES_Value val_alpha, val_beta, val_gamma, val_absolute;
	SetNumberOrNull(&val_alpha, alpha);
	SetNumberOrNull(&val_beta, beta);
	SetNumberOrNull(&val_gamma, gamma);
	if (op_isnan(alpha) && op_isnan(beta) && op_isnan(gamma))
		DOMSetNull(&val_absolute);
	else
		DOMSetBoolean(&val_absolute, absolute);
	return InitValues(val_alpha, val_beta, val_gamma, val_absolute);
}


OP_STATUS
DOM_DeviceOrientationEvent::InitValues(ES_Value& alpha, ES_Value& beta, ES_Value& gamma, ES_Value& absolute)
{
	RETURN_IF_ERROR(Put(UNI_L("alpha"), alpha, TRUE));
	RETURN_IF_ERROR(Put(UNI_L("beta"), beta, TRUE));
	RETURN_IF_ERROR(Put(UNI_L("gamma"), gamma, TRUE));
	return Put(UNI_L("absolute"), absolute, TRUE);
}

/* static */ int
DOM_DeviceOrientationEvent::initDeviceOrientationEvent(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(event_obj, DOM_TYPE_DEVICEORIENTATION_EVENT, DOM_DeviceOrientationEvent);
	DOM_CHECK_ARGUMENTS("sbbnnnb");

	//in DOMString typeArg
	//in boolean canBubbleArg
	//in boolean cancelableArg
	//in number alphaArg
	//in number betaArg
	//in number gammaArg
	//in boolean absoluteArg


	int result = initEvent(this_object, argv, argc, return_value, origining_runtime);

	if (result != ES_FAILED)
		return result;

	CALL_FAILED_IF_ERROR(event_obj->InitValues(argv[3], argv[4], argv[5], argv[6]));

	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_DeviceOrientationEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_DeviceOrientationEvent, DOM_DeviceOrientationEvent::initDeviceOrientationEvent, "initDeviceOrientationEvent", "sbbnnnb-")
DOM_FUNCTIONS_END(DOM_DeviceOrientationEvent)

#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
