/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILCALLRECORD_H
#define DOM_DOMJILCALLRECORD_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpTelephony.h"
class FindCallRecordsCallbackAsyncImpl;
class DOM_JILTelephony;

class DOM_JILCallRecord : public DOM_JILObject
{
	friend class FindCallRecordsCallbackAsyncImpl;
	friend class DOM_JILTelephony;
public:
	static OP_STATUS Make(DOM_JILCallRecord*& new_obj, DOM_Runtime *origining_runtime, const OpTelephony::CallRecord* call_record_contents = NULL);
	virtual void GCTrace();

	OP_STATUS SetCallRecord(const OpTelephony::CallRecord* call_record_contents);
	OP_STATUS CopyTo(DOM_JILCallRecord*& call_record);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_CALL_RECORD || DOM_Object::IsA(type); }
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
private:
	DOM_JILCallRecord() : m_durationSeconds(0), m_startTime(0) {}

	struct Null_or_undef_st {
		unsigned int address:2;
		unsigned int id:2;
		unsigned int name:2;
		unsigned int type:2;
		unsigned int durationSeconds:2;
	} m_undefnull;

	OpString m_address;
	OpString m_id;
	OpString m_name;
	OpString m_type;
	unsigned int m_durationSeconds;
	ES_Object* m_startTime;
};

class DOM_JILCallRecord_Constructor : public DOM_BuiltInConstructor
{
public:
	DOM_JILCallRecord_Constructor() : DOM_BuiltInConstructor(DOM_Runtime::JIL_CALLRECORD_PROTOTYPE) { }

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILCALLRECORD_H

