/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilcallrecordtypes.h"

/* static */ OP_STATUS
DOM_JILCallRecordTypes::Make(DOM_JILCallRecordTypes*& new_obj, DOM_Runtime* runtime)
{
	new_obj = OP_NEW(DOM_JILCallRecordTypes, ());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(new_obj, runtime, runtime->GetPrototype(DOM_Runtime::JIL_CALLRECORDTYPES_PROTOTYPE), "CallRecordTypes"));
	RETURN_IF_ERROR(new_obj->PutStringConstant(UNI_L("MISSED"), CALL_RECORD_TYPE_MISSED));
	RETURN_IF_ERROR(new_obj->PutStringConstant(UNI_L("OUTGOING"), CALL_RECORD_TYPE_OUTGOING));
	RETURN_IF_ERROR(new_obj->PutStringConstant(UNI_L("RECEIVED"), CALL_RECORD_TYPE_RECEIVED));
	return OpStatus::OK;
}

/* static */ OpTelephony::CallRecord::Type
DOM_JILCallRecordTypes::FromString(const uni_char* type)
{
	if (!type)
		return OpTelephony::CallRecord::TypeUnknown;
	else if (IsMissed(type))
		return OpTelephony::CallRecord::Missed;
	else if (IsOutgoing(type))
		return OpTelephony::CallRecord::Initiated;
	else if (IsReceived(type))
		return OpTelephony::CallRecord::Received;
	return OpTelephony::CallRecord::TypeUnknown;
}

/* static */ const uni_char*
DOM_JILCallRecordTypes::ToString(OpTelephony::CallRecord::Type type)
{
	switch (type)
	{
		case OpTelephony::CallRecord::Missed: return CALL_RECORD_TYPE_MISSED;
		case OpTelephony::CallRecord::Initiated: return CALL_RECORD_TYPE_OUTGOING;
		case OpTelephony::CallRecord::Received: return CALL_RECORD_TYPE_RECEIVED;
		default: return CALL_RECORD_TYPE_UNKNOWN;
	}
}

#endif // DOM_JIL_API_SUPPORT

