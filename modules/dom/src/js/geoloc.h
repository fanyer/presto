/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
*/

#ifndef DOM_GEOLOC_H
#define DOM_GEOLOC_H
#if defined(DOM_GEOLOCATION_SUPPORT) && defined(PI_GEOLOCATION)

#include "modules/dom/src/domobj.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/geolocation/geolocation.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/windowcommander/OpWindowCommander.h"

class DOM_Coordinates;
class GeoLocRequest_elm;

class DOM_Geolocation
	: public DOM_Object
{
public:
	virtual ~DOM_Geolocation();

	static OP_STATUS Make(DOM_Geolocation *&new_obj, DOM_Runtime *origining_runtime);
	static void BeforeUnload(DOM_EnvironmentImpl *env);
	static DOM_Geolocation *GetGeolocationObject(DOM_EnvironmentImpl *env);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_GEOLOCATION || DOM_Object::IsA(type); }
	virtual void GCTrace();

	OP_STATUS TriggerErrorCallback(ES_Object *error_callback, OpGeolocation::Error::ErrorCode error, DOM_Runtime *origining_runtime);

	// Functions
	DOM_DECLARE_FUNCTION(getCurrentPosition);
	DOM_DECLARE_FUNCTION(watchPosition);
	DOM_DECLARE_FUNCTION(clearWatch);
	enum
	{
		FUNCTIONS_getCurrentPosition = 1,
		FUNCTIONS_watchPosition,
		FUNCTIONS_clearWatch,
		FUNCTIONS_ARRAY_SIZE
	};

private:
	friend class GeoLocRequest_elm;

	DOM_Geolocation() : positionValid(FALSE), cur_elm_id(0)
	{
		lastPosition.altitude = 0;
		lastPosition.capabilities = OpGeolocation::Position::STANDARD;
		lastPosition.heading = 0;
		lastPosition.horizontal_accuracy = 0;
		lastPosition.vertical_accuracy = 0;
		lastPosition.latitude = 0;
		lastPosition.longitude = 0;
		lastPosition.timestamp = 0;
		lastPosition.type = OpGeolocation::ANY;
		lastPosition.type_info.radio.cell_id = -1;
		lastPosition.velocity = 0;
	}

	UINT32 GetNextElmId() { return ++cur_elm_id; }
	void ReleaseAll();

	void AqcuireGeolocElement(GeoLocRequest_elm *element);

	BOOL positionValid;
	OpGeolocation::Position lastPosition;

	List<GeoLocRequest_elm> requests;

	UINT32 cur_elm_id;
};

//////////////////////////////////////////////////////////////////////////

class DOM_Position
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_Position *&new_obj, const OpGeolocation::Position &pos, DOM_Runtime *origining_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_POSITION || DOM_Object::IsA(type); }
	virtual void GCTrace();

private:
	DOM_Position() : m_coordinates(NULL) {}
	OP_STATUS Initialize(const OpGeolocation::Position &pos);

	DOM_Coordinates *m_coordinates;
};

//////////////////////////////////////////////////////////////////////////

class DOM_Coordinates
	: public DOM_Object
{
public:
	static OP_STATUS Make(DOM_Coordinates *&new_obj, const OpGeolocation::Position &pos, DOM_Runtime *origining_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_COORDINATES || DOM_Object::IsA(type); }

private:
	friend class DOM_Position;

	DOM_Coordinates(const OpGeolocation::Position &pos);

	BOOL positionValid;
	OpGeolocation::Position pos;
};

//////////////////////////////////////////////////////////////////////////

class DOM_PositionError
	: public DOM_Object
{
public:
	enum PositionError
	{
		UNKNOWN_ERROR			= 0,
		PERMISSION_DENIED		= 1,
		POSITION_UNAVAILABLE	= 2,
		TIMEOUT					= 3
	};

	static OP_STATUS Make(DOM_PositionError *&new_obj, OpGeolocation::Error::ErrorCode error, DOM_Runtime *origining_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_POSITIONERROR || DOM_Object::IsA(type); }

private:
	DOM_PositionError(OpGeolocation::Error::ErrorCode error);
	OP_STATUS Initialize();

	PositionError code;
	OpString message;
};

//////////////////////////////////////////////////////////////////////////

class GeoLocRequest_elm
	: public ListElement<GeoLocRequest_elm>
	, public OpGeolocationListener
	, public ES_AsyncCallback
	, public DOM_Object
	, public OpSecurityCheckCallback
	, public UserConsentListener
{
public:
	static OP_STATUS Make(GeoLocRequest_elm *&new_obj, DOM_Geolocation *owner, DOM_Runtime* runtime, ES_Object *position_callback, ES_Object *position_error_callback, ES_Object *position_options);
	void Release();
	void ReleaseIfFinished();

	void GCTrace();

	int getCurrentPosition() { return watchPosition(NULL); }
	int watchPosition(ES_Value *return_value);
	void clearWatch();

	UINT32 ElmId() { return elm_id; }

	// From UserConsentListener
	virtual void OnUserConsentRevoked();
	virtual void OnBeforeRuntimeDestroyed(DOM_Runtime *runtime) { };

protected:
	// from OpGeolocationListener
	void OnGeolocationUpdated(const OpGeolocation::Position &position);
	void OnGeolocationError(const OpGeolocation::Error &error);

	// from ES_AsyncCallback
	OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);

	// from OpSecurityCheckCallback
	void OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType type);
	void OnSecurityCheckError(OP_STATUS error);

private:
	friend class DOM_Geolocation;

	OP_STATUS EvalNextOption(BOOL restart = FALSE);

	GeoLocRequest_elm(DOM_Geolocation *owner, ES_Object *position_callback, ES_Object *position_error_callback, ES_Object *position_options);

	enum AsyncState
	{
		AS_GET_NONE,
		AS_GET_ENABLEHIGHACCURACY,
		AS_GET_TIMEOUT,
		AS_GET_MAXIMUM_AGE,
		AS_GET_FINISHED
	};

	DOM_Geolocation *owner;
	ES_Object *position_callback;
	ES_Object *position_error_callback;
	ES_Object *position_options;

	OpGeolocation::Options options;
	int state;
	BOOL watch;
	BOOL allowed;
	OpGeolocation *gloc;
	UINT32 elm_id;
	OpSecurityCheckCancel *cancel;
	BOOL is_started;
};

#endif // DOM_GEOLOCATION_SUPPORT && PI_GEOLOCATION
#endif // DOM_GEOLOC_H
