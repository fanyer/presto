/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 *
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilmessagetypes.h"

/* static */ OP_STATUS
DOM_JILMessageTypes::Make(DOM_JILMessageTypes*& new_message_types, DOM_Runtime* runtime)
{
	new_message_types = OP_NEW(DOM_JILMessageTypes, ());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(new_message_types, runtime, runtime->GetPrototype(DOM_Runtime::JIL_MESSAGETYPES_PROTOTYPE), "MessageTypes"));
	RETURN_IF_ERROR(new_message_types->PutStringConstant(UNI_L("SMSMessage"), MESSAGE_TYPE_SMS));
	RETURN_IF_ERROR(new_message_types->PutStringConstant(UNI_L("EmailMessage"), MESSAGE_TYPE_EMAIL));
	RETURN_IF_ERROR(new_message_types->PutStringConstant(UNI_L("MMSMessage"), MESSAGE_TYPE_MMS));
	return OpStatus::OK;
}

/* static */ OpMessaging::Message::Type
DOM_JILMessageTypes::FromString(const uni_char* type)
{
	if (IsSms(type))
		return OpMessaging::Message::SMS;
	else if (IsMms(type))
		return OpMessaging::Message::MMS;
	else if (IsEmail(type))
		return OpMessaging::Message::Email;
	return OpMessaging::Message::TypeUnknown;
}

/* static */ const uni_char*
DOM_JILMessageTypes::ToString(OpMessaging::Message::Type type)
{
	switch (type)
	{
		case OpMessaging::Message::SMS: return MESSAGE_TYPE_SMS;
		case OpMessaging::Message::MMS: return MESSAGE_TYPE_MMS;
		case OpMessaging::Message::Email: return MESSAGE_TYPE_EMAIL;
		default: return MESSAGE_TYPE_UNKNOWN;
	}
}

#endif // DOM_JIL_API_SUPPORT
