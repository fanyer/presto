/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILTELEPHONY_H
#define DOM_DOMJILTELEPHONY_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpTelephony.h"

class DOM_JILTelephony : public DOM_JILObject, public OpTelephonyListener
{
public:
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_TELEPHONY || DOM_Object::IsA(type); }
	static OP_STATUS Make(DOM_JILTelephony*& new_telephony, DOM_Runtime* runtime);
	virtual void GCTrace();

	// OpTelephonyListener implementation.
	virtual void OnCall(OpTelephony::CallRecord::Type type, const OpString& phone_number);

	DOM_DECLARE_FUNCTION(deleteAllCallRecords);
	DOM_DECLARE_FUNCTION(deleteCallRecord);
	DOM_DECLARE_FUNCTION(findCallRecords);
	DOM_DECLARE_FUNCTION(getCallRecord);
	DOM_DECLARE_FUNCTION(getCallRecordCnt);
	DOM_DECLARE_FUNCTION(initiateVoiceCall);

	enum { FUNCTIONS_ARRAY_SIZE = 7};
protected:
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
private:
	static int HandleError(OP_TELEPHONYSTATUS error, ES_Value* return_value, DOM_Runtime* runtime);
	DOM_JILTelephony() : m_onCallEventCallback(NULL), m_onCallRecordsFoundCallback(NULL), m_telephony(NULL) {}
	~DOM_JILTelephony() { OP_DELETE(m_telephony); }
	ES_Object* m_onCallEventCallback;
	ES_Object* m_onCallRecordsFoundCallback;
	OpTelephony* m_telephony;
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILTELEPHONY_H

