/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
#include "modules/dom/src/orientation/devicemotionevent.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/doc/frm_doc.h"

/* static */ OP_STATUS
DOM_DeviceMotionEvent::Make(DOM_DeviceMotionEvent*& event, DOM_Runtime* runtime)
{
	event = OP_NEW(DOM_DeviceMotionEvent, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(event, runtime,
			runtime->GetPrototype(DOM_Runtime::DEVICEMOTION_EVENT_PROTOTYPE), "DeviceMotionEvent")
			);

	event->InitEvent(ONDEVICEMOTION, runtime->GetEnvironment()->GetWindow(), NULL);
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_DeviceMotionEvent::Make(DOM_DeviceMotionEvent*& event, DOM_Runtime* runtime
                          , double x, double y, double z
                          , double x_with_gravity, double y_with_gravity, double z_with_gravity
    	                  , double alpha, double beta, double gamma
                          , double interval)
{
	RETURN_IF_ERROR(Make(event, runtime));
	ES_Object* es_acceleration_struct = NULL;
	ES_Object* es_acceleration_with_gravity_struct = NULL;
	ES_Object* es_rotation_struct = NULL;
	ES_Value val_interval;
	if (op_isnan(x) && op_isnan(y)  && op_isnan(z) &&
	    op_isnan(x_with_gravity) && op_isnan(y_with_gravity) && op_isnan(z_with_gravity) &&
	    op_isnan(alpha) && op_isnan(beta) && op_isnan(gamma))
		DOMSetNull(&val_interval);
	else
	{
		RETURN_IF_ERROR(MakeAcceleration(es_acceleration_struct, runtime, x, y, z));
		RETURN_IF_ERROR(MakeAcceleration(es_acceleration_with_gravity_struct,runtime, x_with_gravity, y_with_gravity, z_with_gravity));
		RETURN_IF_ERROR(MakeRotation(es_rotation_struct, runtime, alpha, beta, gamma));
		DOMSetNumber(&val_interval, interval);
	}
	return event->InitValues(es_acceleration_struct, es_acceleration_with_gravity_struct, es_rotation_struct, val_interval);
}

/* static */ void
DOM_DeviceMotionEvent::SetNumberOrNull(ES_Value* val, double number)
{
	if (op_isnan(number))
		DOMSetNull(val);
	else
		DOMSetNumber(val, number);
}

/* static */ void
DOM_DeviceMotionEvent::GetNameOrUndefined(DOM_Runtime* runtime, ES_Object* object, const uni_char* property_name, ES_Value* value)
{
	if (runtime->GetName(object, property_name, value) != OpBoolean::IS_TRUE)
		DOMSetUndefined(value);
}

OP_STATUS
DOM_DeviceMotionEvent::InitValues(ES_Object* acceleration, ES_Object* acceleration_with_gravity, ES_Object* rotation_rate, ES_Value& interval)
{
	ES_Value acceleration_val, acceleration_with_gravity_val, rotation_rate_val;
	DOMSetObject(&acceleration_val, acceleration);
	DOMSetObject(&acceleration_with_gravity_val, acceleration_with_gravity);
	DOMSetObject(&rotation_rate_val, rotation_rate);
	RETURN_IF_ERROR(Put(UNI_L("acceleration"), acceleration_val, TRUE));
	RETURN_IF_ERROR(Put(UNI_L("accelerationIncludingGravity"), acceleration_with_gravity_val, TRUE));
	RETURN_IF_ERROR(Put(UNI_L("rotationRate"), rotation_rate_val, TRUE));
	return Put(UNI_L("interval"), interval, TRUE);
}
/* static */ OP_STATUS
DOM_DeviceMotionEvent::MakeAcceleration(ES_Object*& obj, DOM_Runtime* runtime, double x, double y, double z)
{
	RETURN_IF_ERROR(runtime->CreateNativeObjectObject(&obj));
	ES_Value val_x, val_y, val_z;
	SetNumberOrNull(&val_x, x);
	SetNumberOrNull(&val_y, y);
	SetNumberOrNull(&val_z, z);
	RETURN_IF_ERROR(runtime->PutName(obj, UNI_L("x"), val_x, PROP_READ_ONLY | PROP_DONT_DELETE));
	RETURN_IF_ERROR(runtime->PutName(obj, UNI_L("y"), val_y, PROP_READ_ONLY | PROP_DONT_DELETE));
	return runtime->PutName(obj, UNI_L("z"), val_z, PROP_READ_ONLY | PROP_DONT_DELETE);
}

/* static */ OP_STATUS
DOM_DeviceMotionEvent::MakeRotation(ES_Object*& obj, DOM_Runtime* runtime, double alpha, double beta, double gamma)
{
	RETURN_IF_ERROR(runtime->CreateNativeObjectObject(&obj));
	ES_Value val_alpha, val_beta, val_gamma;
	SetNumberOrNull(&val_alpha, alpha);
	SetNumberOrNull(&val_beta, beta);
	SetNumberOrNull(&val_gamma, gamma);
	RETURN_IF_ERROR(runtime->PutName(obj, UNI_L("alpha"), val_alpha, PROP_READ_ONLY | PROP_DONT_DELETE));
	RETURN_IF_ERROR(runtime->PutName(obj, UNI_L("beta"), val_beta, PROP_READ_ONLY | PROP_DONT_DELETE));
	return runtime->PutName(obj, UNI_L("gamma"), val_gamma, PROP_READ_ONLY | PROP_DONT_DELETE);
}

/* static */ int
DOM_DeviceMotionEvent::initAccelerometerEvent(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(event_obj, DOM_TYPE_DEVICEMOTION_EVENT, DOM_DeviceMotionEvent);
	DOM_CHECK_ARGUMENTS("sbbooon");

	//in DOMString typeArg
	//in boolean canBubbleArg
	//in boolean cancelableArg
	//in number accelerationArg
	//in number accelerationIncludingGravityArg
	//in number rotationRateArg
	//in boolean intervalArg

	int result = initEvent(this_object, argv, argc, return_value, origining_runtime);
	if (result != ES_FAILED)
		return result;

	ES_Object* es_acceleration_struct;
	ES_Object* es_acceleration_with_gravity_struct;
	ES_Object* es_rotation_struct;

	ES_Value val_x, val_y, val_z;
	double x,y,z;
	GetNameOrUndefined(origining_runtime, argv[3].value.object, UNI_L("x"), &val_x);
	GetNameOrUndefined(origining_runtime, argv[3].value.object, UNI_L("y"), &val_y);
	GetNameOrUndefined(origining_runtime, argv[3].value.object, UNI_L("z"), &val_z);
	x = (val_x.type == VALUE_NUMBER) ? val_x.value.number : op_nan(NULL);
	y = (val_y.type == VALUE_NUMBER) ? val_y.value.number : op_nan(NULL);
	z = (val_z.type == VALUE_NUMBER) ? val_z.value.number : op_nan(NULL);
	CALL_FAILED_IF_ERROR(DOM_DeviceMotionEvent::MakeAcceleration(es_acceleration_struct, origining_runtime, x, y, z));

	GetNameOrUndefined(origining_runtime, argv[4].value.object, UNI_L("x"), &val_x);
	GetNameOrUndefined(origining_runtime, argv[4].value.object, UNI_L("y"), &val_y);
	GetNameOrUndefined(origining_runtime, argv[4].value.object, UNI_L("z"), &val_z);
	x = (val_x.type == VALUE_NUMBER) ? val_x.value.number : op_nan(NULL);
	y = (val_y.type == VALUE_NUMBER) ? val_y.value.number : op_nan(NULL);
	z = (val_z.type == VALUE_NUMBER) ? val_z.value.number : op_nan(NULL);
	CALL_FAILED_IF_ERROR(DOM_DeviceMotionEvent::MakeAcceleration(es_acceleration_with_gravity_struct, origining_runtime, x, y, z));

	GetNameOrUndefined(origining_runtime, argv[5].value.object, UNI_L("alpha"), &val_x);
	GetNameOrUndefined(origining_runtime, argv[5].value.object, UNI_L("beta"), &val_y);
	GetNameOrUndefined(origining_runtime, argv[5].value.object, UNI_L("gamma"), &val_z);
	x = (val_x.type == VALUE_NUMBER) ? val_x.value.number : op_nan(NULL);
	y = (val_y.type == VALUE_NUMBER) ? val_y.value.number : op_nan(NULL);
	z = (val_z.type == VALUE_NUMBER) ? val_z.value.number : op_nan(NULL);
	CALL_FAILED_IF_ERROR(DOM_DeviceMotionEvent::MakeRotation(es_rotation_struct, origining_runtime, x, y, z));

	CALL_FAILED_IF_ERROR(event_obj->InitValues(es_acceleration_struct, es_acceleration_with_gravity_struct, es_rotation_struct, argv[6]));
#undef ARGUMENT
	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_DeviceMotionEvent)
	DOM_FUNCTIONS_FUNCTION(DOM_DeviceMotionEvent, DOM_DeviceMotionEvent::initAccelerometerEvent, "initAccelerometerEvent", "sbbooon-")
DOM_FUNCTIONS_END(DOM_DeviceMotionEvent)

#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
