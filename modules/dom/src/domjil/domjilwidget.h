/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILWIDGET_H
#define DOM_DOMJILWIDGET_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/database/opstorage.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/dom/src/domenvironmentimpl.h"
#ifdef PI_POWER_STATUS
#include "modules/pi/device_api/OpPowerStatus.h"
#endif // PI_POWER_STATUS

class DOM_Runtime;
class GetPreferenceOperationCallback;
class PreferenceOperationCallback;
class WebStorageValue;

class DOM_JILWidget : public DOM_JILObject
#ifdef PI_POWER_STATUS
	, public OpPowerStatusListener
#endif // PI_POWER_STATUS
{
public:
	static void MakeL(DOM_JILWidget*& new_object, DOM_Runtime* runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_WIDGET || DOM_Object::IsA(type); }
	virtual ~DOM_JILWidget() { }

	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual void GCTrace();
	OP_STATUS HandleEvent(DOM_Environment::GadgetEvent event, DOM_Environment::GadgetEventData *data);

#ifdef PI_POWER_STATUS
	// OpPowerStatusListener
	virtual void OnWakeUp(OpPowerStatus* power_status);
	virtual void OnChargeLevelChange(OpPowerStatus* power_status, BYTE new_charge_level);
	virtual void OnPowerSourceChange(OpPowerStatus* power_status, OpPowerStatusListener::PowerSupplyType power_source);
	virtual void OnLowPowerStateChange(OpPowerStatus* power_status, BOOL is_low);

	void OnBeforeEnvironmentDestroy();
#endif // PI_POWER_STATUS

	DOM_DECLARE_FUNCTION(openURL);
	DOM_DECLARE_FUNCTION(setPreferenceForKey);
	DOM_DECLARE_FUNCTION(preferenceForKey);
	enum { FUNCTIONS_ARRAY_SIZE = 4 };

private:
	DOM_JILWidget() : m_on_restore(NULL), m_on_maximize(NULL), m_on_focus(NULL), m_on_wakeup(NULL), m_lazy_multimedia_init(FALSE) {}

	static void CreateExceptionTypesL(DOM_Object*& new_exception_types_object, DOM_Runtime* runtime);

	ES_Object** GetEventHandler(OpAtom property_atom);

	static int HandlePreferenceError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* runtime);

	ES_Object* m_on_restore;
	ES_Object* m_on_maximize;
	ES_Object* m_on_focus;
	ES_Object* m_on_wakeup;

	void GetStorageItem(const WebStorageValue *key, GetPreferenceOperationCallback* callback);
	void SetStorageItem(const WebStorageValue *key, const WebStorageValue* value, PreferenceOperationCallback* callback);
	OP_STATUS EnsureStorageObject();

	BOOL m_lazy_multimedia_init;
	AutoReleaseOpStoragePtr m_storage;
};

#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILWIDGET.H
