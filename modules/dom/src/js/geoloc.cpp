/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
*/

#include "core/pch.h"

#if defined(DOM_GEOLOCATION_SUPPORT) && defined(PI_GEOLOCATION)

#include "modules/dom/src/js/geoloc.h"

#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/windowcommander/src/WindowCommander.h"


/* virtual */
DOM_Geolocation::~DOM_Geolocation()
{
	ReleaseAll();
}

void
DOM_Geolocation::ReleaseAll()
{
	while (GeoLocRequest_elm *elm = requests.First())
		elm->Release();
}

/* static */ void
DOM_Geolocation::BeforeUnload(DOM_EnvironmentImpl *env)
{
	DOM_Geolocation *o = GetGeolocationObject(env);
	if (o)
		o->ReleaseAll();
}

/* static */ DOM_Geolocation *
DOM_Geolocation::GetGeolocationObject(DOM_EnvironmentImpl *env)
{
	DOM_Object *window = env->GetWindow();

	ES_Value value;
	if (window->GetPrivate(DOM_PRIVATE_navigator, &value) == OpBoolean::IS_TRUE)
		if (DOM_GetHostObject(value.value.object)->GetPrivate(DOM_PRIVATE_geolocation, &value) == OpBoolean::IS_TRUE)
			return static_cast<DOM_Geolocation*>(DOM_GetHostObject(value.value.object));

	return NULL;
}

/* static */ OP_STATUS
DOM_Geolocation::Make(DOM_Geolocation *&new_obj, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_Geolocation, ()), origining_runtime,
		origining_runtime->GetPrototype(DOM_Runtime::GEOLOCATION_PROTOTYPE), "Geolocation"));

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_Geolocation::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_lastPosition)
	{
		if (value)
		{
			if (positionValid)
			{
				DOM_Position *pos;
				GET_FAILED_IF_ERROR(DOM_Position::Make(pos, lastPosition, GetRuntime()));

				DOMSetObject(value, pos);
				return GET_SUCCESS;
			}
			else
				DOMSetNull(value);
		}
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ void
DOM_Geolocation::GCTrace()
{
	GeoLocRequest_elm *obj = requests.First();
	while (obj)
	{
		GCMark(obj);
		obj = obj->Suc();
	}
}

/* static */ int
DOM_Geolocation::getCurrentPosition(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(geolocation, DOM_TYPE_GEOLOCATION, DOM_Geolocation);
	DOM_CHECK_ARGUMENTS("f|F");

	if (!geolocation->GetFramesDocument())
		return DOM_CALL_INTERNALEXCEPTION(RESOURCE_UNAVAILABLE_ERR);

	ES_Object *position_cb     = (argc > 0 && argv[0].type == VALUE_OBJECT) ? argv[0].value.object : NULL;
	ES_Object *position_err_cb = (argc > 1 && argv[1].type == VALUE_OBJECT) ? argv[1].value.object : NULL;
	ES_Object *position_opt    = (argc > 2 && argv[2].type == VALUE_OBJECT) ? argv[2].value.object : NULL;

	GeoLocRequest_elm *elm;

	OP_STATUS status = GeoLocRequest_elm::Make(elm, geolocation, origining_runtime, position_cb, position_err_cb, position_opt);

	if (OpStatus::IsError(status))
	{
		if (OpStatus::IsMemoryError(status))
			return ES_NO_MEMORY;
		else if (position_err_cb)
			geolocation->TriggerErrorCallback(position_err_cb, OpGeolocation::Error::PROVIDER_ERROR, origining_runtime);

		return ES_FAILED;
	}

	geolocation->AqcuireGeolocElement(elm);
	return elm->getCurrentPosition();
}

/* static */ int
DOM_Geolocation::watchPosition(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(geolocation, DOM_TYPE_GEOLOCATION, DOM_Geolocation);
	DOM_CHECK_ARGUMENTS("f|F");

	if (!geolocation->GetFramesDocument())
		return DOM_CALL_INTERNALEXCEPTION(RESOURCE_UNAVAILABLE_ERR);

	ES_Object *position_cb     = (argc > 0 && argv[0].type == VALUE_OBJECT) ? argv[0].value.object : NULL;
	ES_Object *position_err_cb = (argc > 1 && argv[1].type == VALUE_OBJECT) ? argv[1].value.object : NULL;
	ES_Object *position_opt    = (argc > 2 && argv[2].type == VALUE_OBJECT) ? argv[2].value.object : NULL;

	GeoLocRequest_elm *elm;
	OP_STATUS status = GeoLocRequest_elm::Make(elm, geolocation, origining_runtime, position_cb, position_err_cb, position_opt);

	if (OpStatus::IsError(status))
	{
		if (OpStatus::IsMemoryError(status))
			return ES_NO_MEMORY;
		else if (position_err_cb)
			geolocation->TriggerErrorCallback(position_err_cb, OpGeolocation::Error::PROVIDER_ERROR, origining_runtime);

		DOMSetNumber(return_value, 0.0);
		return ES_VALUE;
	}

	geolocation->AqcuireGeolocElement(elm);
	return elm->watchPosition(return_value);
}

/* static */ int
DOM_Geolocation::clearWatch(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(geolocation, DOM_TYPE_GEOLOCATION, DOM_Geolocation);
	DOM_CHECK_ARGUMENTS("n");

	UINT32 watch_id = static_cast<UINT32>(argv[0].value.number);

	GeoLocRequest_elm *obj = geolocation->requests.First();
	while (obj)
	{
		if (obj->ElmId() == watch_id)
		{
			obj->clearWatch();
			obj->Release();
			break; // Only one object with the same id
		}
		obj = obj->Suc();
	}

	return ES_FAILED;	// no return value
}

OP_STATUS
DOM_Geolocation::TriggerErrorCallback(ES_Object *error_callback, OpGeolocation::Error::ErrorCode error, DOM_Runtime *origining_runtime)
{
	DOM_PositionError *obj;
	RETURN_IF_ERROR(DOM_PositionError::Make(obj, error, origining_runtime));

	ES_Value arguments[1];
	DOM_Object::DOMSetObject(&arguments[0], obj);
	RETURN_IF_ERROR(GetEnvironment()->GetAsyncInterface()->CallFunction(error_callback, *this, 1, arguments));

	return OpStatus::OK;
}

void
DOM_Geolocation::AqcuireGeolocElement(GeoLocRequest_elm *element)
{
	element->Into(&requests);
}

DOM_FUNCTIONS_START(DOM_Geolocation)
	DOM_FUNCTIONS_FUNCTION(DOM_Geolocation, DOM_Geolocation::getCurrentPosition, "getCurrentPosition", NULL)
	DOM_FUNCTIONS_FUNCTION(DOM_Geolocation, DOM_Geolocation::watchPosition, "watchPosition", NULL)
	DOM_FUNCTIONS_FUNCTION(DOM_Geolocation, DOM_Geolocation::clearWatch, "clearWatch", "n-")
DOM_FUNCTIONS_END(DOM_Geolocation)

//////////////////////////////////////////////////////////////////////////

/* static */ OP_STATUS
DOM_Position::Make(DOM_Position *&new_obj, const OpGeolocation::Position &pos, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_Position, ()), origining_runtime,
		origining_runtime->GetPrototype(DOM_Runtime::POSITION_PROTOTYPE), "Position"));

	RETURN_IF_ERROR(new_obj->Initialize(pos));

	return OpStatus::OK;
}

OP_STATUS
DOM_Position::Initialize(const OpGeolocation::Position &pos)
{
	RETURN_IF_ERROR(DOM_Coordinates::Make(m_coordinates, pos, GetRuntime()));

	return OpStatus::OK;
}

/* virtual */ void
DOM_Position::GCTrace()
{
	GCMark(m_coordinates);
}

/* virtual */ ES_GetState
DOM_Position::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_timestamp:
		DOMSetNumber(value, op_floor(m_coordinates->pos.timestamp));
		return GET_SUCCESS;

	case OP_ATOM_coords:
		DOMSetObject(value, m_coordinates);
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

//////////////////////////////////////////////////////////////////////////

/* static */ OP_STATUS
DOM_Coordinates::Make(DOM_Coordinates *&new_obj, const OpGeolocation::Position &pos, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_Coordinates, (pos)), origining_runtime,
		origining_runtime->GetPrototype(DOM_Runtime::COORDINATES_PROTOTYPE), "Coordinates"));

	return OpStatus::OK;
}

DOM_Coordinates::DOM_Coordinates(const OpGeolocation::Position &pos)
{
	this->pos = pos;
	positionValid = TRUE;
}

/* virtual */ ES_GetState
DOM_Coordinates::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (!positionValid)
		return GET_FAILED;

	switch (property_name)
	{
	case OP_ATOM_latitude:
		DOMSetNumber(value, pos.latitude);
		return GET_SUCCESS;

	case OP_ATOM_longitude:
		DOMSetNumber(value, pos.longitude);
		return GET_SUCCESS;

	case OP_ATOM_altitude:
		pos.capabilities & OpGeolocation::Position::HAS_ALTITUDE ? DOMSetNumber(value, pos.altitude) : DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_accuracy:
		DOMSetNumber(value, pos.horizontal_accuracy);
		return GET_SUCCESS;

	case OP_ATOM_altitudeAccuracy:
		pos.capabilities & OpGeolocation::Position::HAS_ALTITUDEACCURACY ? DOMSetNumber(value, pos.vertical_accuracy) : DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_heading:
		pos.capabilities & OpGeolocation::Position::HAS_HEADING ? DOMSetNumber(value, pos.heading) : DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_speed:
		pos.capabilities & OpGeolocation::Position::HAS_VELOCITY ? DOMSetNumber(value, pos.velocity) : DOMSetNull(value);
		return GET_SUCCESS;
	}

	return GET_FAILED;
}


//////////////////////////////////////////////////////////////////////////

/* static */ OP_STATUS
DOM_PositionError::Make(DOM_PositionError *&new_obj, OpGeolocation::Error::ErrorCode error, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_PositionError, (error)), origining_runtime,
		origining_runtime->GetPrototype(DOM_Runtime::POSITIONERROR_PROTOTYPE), "PositionError"));

	RETURN_IF_ERROR(new_obj->Initialize());

	return OpStatus::OK;
}

DOM_PositionError::DOM_PositionError(OpGeolocation::Error::ErrorCode error)
{
	switch (error)
	{
	case OpGeolocation::Error::PERMISSION_ERROR:
		code = PERMISSION_DENIED;
		break;
	case OpGeolocation::Error::PROVIDER_ERROR:
	case OpGeolocation::Error::POSITION_NOT_FOUND:
		code = POSITION_UNAVAILABLE;
		break;
	case OpGeolocation::Error::TIMEOUT_ERROR:
		code = TIMEOUT;
		break;
	default:
		code = UNKNOWN_ERROR;
		break;
	}
}

OP_STATUS
DOM_PositionError::Initialize()
{
	DOM_Runtime *runtime = GetRuntime();
	ES_Object *object = GetNativeObject();

	TRAPD(status,
		PutNumericConstantL(object, "UNKNOWN_ERROR", UNKNOWN_ERROR, runtime);
		PutNumericConstantL(object, "PERMISSION_DENIED", PERMISSION_DENIED, runtime);
		PutNumericConstantL(object, "POSITION_UNAVAILABLE", POSITION_UNAVAILABLE, runtime);
		PutNumericConstantL(object, "TIMEOUT", TIMEOUT, runtime);
		message.SetL("no message available");
	);

	return status;
}

/* virtual */ ES_GetState
DOM_PositionError::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_code:
		DOMSetNumber(value, static_cast<double>(code));
		return GET_SUCCESS;

	case OP_ATOM_message:
		DOMSetString(value, message.CStr());
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

//////////////////////////////////////////////////////////////////////////

/* static */ OP_STATUS
GeoLocRequest_elm::Make(GeoLocRequest_elm *&new_obj, DOM_Geolocation *owner, DOM_Runtime* origining_runtime, ES_Object *position_callback, ES_Object *position_error_callback, ES_Object *position_options)
{
	return DOMSetObjectRuntime(new_obj = OP_NEW(GeoLocRequest_elm, (owner, position_callback, position_error_callback, position_options)), origining_runtime);
}

GeoLocRequest_elm::GeoLocRequest_elm(DOM_Geolocation *owner, ES_Object *position_callback, ES_Object *position_error_callback, ES_Object *position_options)
  : owner(owner)
  , position_callback(position_callback)
  , position_error_callback(position_error_callback)
  , position_options(position_options)
  , state(AS_GET_NONE)
  , watch(FALSE)
  , elm_id(owner->GetNextElmId())
  , cancel(NULL)
  , is_started(FALSE)
{
	options.timeout = LONG_MAX;
	options.maximum_age = 0;
	options.high_accuracy = FALSE;
}

void
GeoLocRequest_elm::Release()
{
	// Expose ourselves to the wrath of the garbage collector (unless we're currently protected)
	// and remove ourself from the list of active listeners, before
	// updating the windowcommander state.
	Out();

	if (allowed)
		g_secman_instance->UnregisterUserConsentListener(OpSecurityManager::PERMISSION_TYPE_GEOLOCATION, GetRuntime(), this);

	owner = NULL;
	position_callback = NULL;
	position_error_callback = NULL;
	position_options = NULL;

	if (is_started)
	{
		OP_ASSERT(GetFramesDocument());
		GetFramesDocument()->DecGeolocationUseCount();
		is_started = FALSE;
	}
	if (g_geolocation && g_geolocation->IsListenerInUse(this))
	{
		OP_ASSERT(GetFramesDocument());
		g_geolocation->StopReception(this);
	}
	if (cancel)
	{
		cancel->CancelSecurityCheck();
		cancel = NULL;
	}
}

void
GeoLocRequest_elm::ReleaseIfFinished()
{
	// We assume this is being called after one of the callbacks has been fired
	if (options.one_shot)
		Release();
}

void
GeoLocRequest_elm::GCTrace()
{
	GCMark(position_callback);
	GCMark(position_error_callback);
	GCMark(position_options);
}

/* virtual */ void
GeoLocRequest_elm::OnUserConsentRevoked()
{
	OnGeolocationError(OpGeolocation::Error(OpGeolocation::Error::PERMISSION_ERROR, OpGeolocation::ANY));
	Release();
	allowed = FALSE;
}

void
GeoLocRequest_elm::OnGeolocationUpdated(const OpGeolocation::Position &position)
{
	if (owner)
	{
		owner->lastPosition = position;
		owner->positionValid = TRUE;
	}

	if (position_callback)
	{
		DOM_Position *obj;
		RETURN_VOID_IF_ERROR(DOM_Position::Make(obj, position, GetRuntime()));

		ES_Value arguments[1];
		DOM_Object::DOMSetObject(&arguments[0], obj);
		RETURN_VOID_IF_ERROR(GetEnvironment()->GetAsyncInterface()->CallFunction(position_callback, NULL, 1, arguments));
	}

	ReleaseIfFinished();
}

void
GeoLocRequest_elm::OnGeolocationError(const OpGeolocation::Error &error)
{
	if (position_error_callback)
	{
		DOM_PositionError *obj;
		RETURN_VOID_IF_ERROR(DOM_PositionError::Make(obj, error.error, GetRuntime()));

		ES_Value arguments[1];
		DOM_Object::DOMSetObject(&arguments[0], obj);
		RETURN_VOID_IF_ERROR(GetEnvironment()->GetAsyncInterface()->CallFunction(position_error_callback, NULL, 1, arguments));
	}
}

int
GeoLocRequest_elm::watchPosition(ES_Value *return_value)
{
	if (return_value)
	{
		watch = TRUE;
		DOM_Object::DOMSetNumber(return_value, ElmId());
	}

	if (position_options)
	{
		CALL_FAILED_IF_ERROR(EvalNextOption(TRUE));
		return ES_SUSPEND | ES_VALUE;
	}

	// After we've suspended and resumed, return_value == NULL and position_options == NULL, we end up here;
	// and all options has been evaluated at this point

	int result;

	if (watch)
	{
		result = ES_VALUE;				// int watchCurrentPosition(...)
		options.one_shot = FALSE;		// continuous
	}
	else
	{
		result = ES_FAILED;				// void getPosition(...)
		options.one_shot = TRUE;		// one-shot
	}

	if (!g_geolocation->IsReady())
	{
		// The request will fail, so don't ask for permission, just return error
		OnGeolocationError(OpGeolocation::Error(OpGeolocation::Error::PROVIDER_ERROR, OpGeolocation::ANY));
		Release();
		return result;
	}

	// If the timeout is 0 and we have no valid cached position, trigger a timeout immediately.
	// Fixes CT-163
	double now = g_op_time_info->GetTimeUTC();
	if (owner && options.timeout == 0 &&
		((owner->positionValid && (now - owner->lastPosition.timestamp > options.maximum_age)) ||
		!owner->positionValid)
	   )
	{
		OnGeolocationError(OpGeolocation::Error(OpGeolocation::Error::TIMEOUT_ERROR, OpGeolocation::ANY));
		ReleaseIfFinished();
		return result;
	}

	if (GetFramesDocument())
		CALL_FAILED_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::GEOLOCATION, OpSecurityContext(GetFramesDocument()), OpSecurityContext(), this, &cancel));

	return result;
}

void
GeoLocRequest_elm::clearWatch()
{
	g_geolocation->StopReception(this);
}

OP_STATUS
GeoLocRequest_elm::EvalNextOption(BOOL restart)
{
	OP_STATUS status = OpStatus::OK;

	if (restart)
		state = AS_GET_NONE;

	// We might have died/clearWatch'ed while we where working on this
	if (!position_options)
	{
		state = AS_GET_FINISHED;
		return status;
	}

	state++;


	if (FramesDocument *frames_doc = GetFramesDocument())
	{
		const uni_char* next_property_name = NULL;
		switch (state)
		{
		case AS_GET_ENABLEHIGHACCURACY:
			next_property_name = UNI_L("enableHighAccuracy");
			break;
		case AS_GET_TIMEOUT:
			next_property_name = UNI_L("timeout");
			break;
		case AS_GET_MAXIMUM_AGE:
			next_property_name = UNI_L("maximumAge");
			break;
		default:
			OP_ASSERT(!"Should not happen! Unhandled property.");
			// Fall through.
		case AS_GET_FINISHED:
			break;
		}

		if (next_property_name)
		{
			// Keep this object as a GC root until callback attempt has completed.
			// See HandleCallback() and HandleError()
			if (!GetRuntime()->Protect(*this))
				return OpStatus::ERR_NO_MEMORY;

			ES_AsyncInterface *asyncif = frames_doc->GetESAsyncInterface();
			status = asyncif->GetSlot(position_options, next_property_name, this);
			if (OpStatus::IsError(status))
				GetRuntime()->Unprotect(*this);
		}
	}

	return status;
}

OP_STATUS
GeoLocRequest_elm::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
	GetRuntime()->Unprotect(*this);
	if (status == ES_ASYNC_CANCELLED || owner == NULL)
		return OpStatus::OK;	// Just ignore, as we're probably dead already...

	if (status == ES_ASYNC_SUCCESS)
	{
		switch (state)
		{
		case AS_GET_ENABLEHIGHACCURACY:
			if (result.type == VALUE_BOOLEAN)
				options.high_accuracy = result.value.boolean;
			else
				options.high_accuracy = FALSE;
			break;
		case AS_GET_TIMEOUT:
			if (result.type == VALUE_NUMBER)
			{
				if (op_isinf(result.value.number))
					options.timeout = LONG_MAX;
				else if (result.value.number < 0.)
					options.timeout = 0;
				else
					options.timeout = static_cast<long>(result.value.number);
			}
			else
				options.timeout = LONG_MAX;
			break;
		case AS_GET_MAXIMUM_AGE:
			if (result.type == VALUE_NUMBER)
			{
				if (op_isinf(result.value.number))
					options.maximum_age = LONG_MAX;
				else if (result.value.number >= 0.)
					options.maximum_age = static_cast<long>(result.value.number);
				else
					options.maximum_age = 0;
			}
			break;
		}
	}

	RETURN_IF_ERROR(EvalNextOption());

	if (state == AS_GET_FINISHED)
	{
		// We're finished, execute the rest of the code in watchCurrentPosition
		position_options = NULL;
		watchPosition(NULL);
	}

	return OpStatus::OK;
}

void
GeoLocRequest_elm::OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType type)
{
	cancel = NULL;

	if (!InList()) // We're not expecting any callbacks as we're already dead
		return;

	this->allowed = allowed;

	if (allowed)
	{
		g_secman_instance->RegisterUserConsentListener(OpSecurityManager::PERMISSION_TYPE_GEOLOCATION, GetRuntime(), this);
		// Do not call StartReception if we're already in motion
		if (!g_geolocation->IsListenerInUse(this))
		{
			OP_ASSERT(GetFramesDocument());
			GetFramesDocument()->IncGeolocationUseCount();
			is_started = TRUE;
			OpStatus::Ignore(g_geolocation->StartReception(options, this));
		}
	}
	else
	{
		if (!GetFramesDocument()->GetWindow()->IsThumbnailWindow())
			// We don't want to take screenshots with the page showing an error.
			OnGeolocationError(OpGeolocation::Error(OpGeolocation::Error::PERMISSION_ERROR, OpGeolocation::ANY));
		Release();
	}
}

void GeoLocRequest_elm::OnSecurityCheckError(OP_STATUS error)
{
	OnSecurityCheckSuccess(FALSE, PERSISTENCE_NONE);
}
#endif // DOM_GEOLOCATION_SUPPORT && PI_GEOLOCATION
