/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILCALLRECORDTYPES_H
#define DOM_DOMJILCALLRECORDTYPES_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpTelephony.h"

#define CALL_RECORD_TYPE_MISSED UNI_L("MISSED")
#define CALL_RECORD_TYPE_OUTGOING UNI_L("OUTGOING")
#define CALL_RECORD_TYPE_RECEIVED UNI_L("RECEIVED")
#define CALL_RECORD_TYPE_UNKNOWN UNI_L("unknown")

class DOM_JILCallRecordTypes : public DOM_JILObject
{
public:
	DOM_JILCallRecordTypes() {}
	~DOM_JILCallRecordTypes() {}
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_CALL_RECORD_TYPES || DOM_Object::IsA(type); }
	static OP_STATUS Make(DOM_JILCallRecordTypes*& new_obj, DOM_Runtime *origining_runtime);

	static BOOL IsMissed(const uni_char* type)   {return uni_strcmp(CALL_RECORD_TYPE_MISSED, type)   == 0;}
	static BOOL IsOutgoing(const uni_char* type) {return uni_strcmp(CALL_RECORD_TYPE_OUTGOING, type) == 0;}
	static BOOL IsReceived(const uni_char* type) {return uni_strcmp(CALL_RECORD_TYPE_RECEIVED, type) == 0;}
	static const uni_char* Missed()   {return CALL_RECORD_TYPE_MISSED;}
	static const uni_char* Outgoing() {return CALL_RECORD_TYPE_OUTGOING;}
	static const uni_char* Received() {return CALL_RECORD_TYPE_RECEIVED;}

	static OpTelephony::CallRecord::Type FromString(const uni_char* type);
	static const uni_char* ToString(OpTelephony::CallRecord::Type type);
};

#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILCALLRECORDTYPES_H

