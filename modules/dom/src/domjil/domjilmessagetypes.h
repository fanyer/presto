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

#ifndef DOM_DOMJILMESSAGETYPES_H
#define DOM_DOMJILMESSAGETYPES_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpMessaging.h"

#define MESSAGE_TYPE_SMS UNI_L("sms")
#define MESSAGE_TYPE_MMS UNI_L("mms")
#define MESSAGE_TYPE_EMAIL UNI_L("email")
#define MESSAGE_TYPE_UNKNOWN UNI_L("unknown")

class DOM_JILMessageTypes : public DOM_JILObject
{
public:
	DOM_JILMessageTypes() {}
	~DOM_JILMessageTypes() {}
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_MESSAGETYPES || DOM_JILObject::IsA(type); }

	static OP_STATUS Make(DOM_JILMessageTypes*& new_message_types, DOM_Runtime* runtime);

	static BOOL IsSms(const uni_char* type) {return uni_str_eq(MESSAGE_TYPE_SMS, type);}
	static BOOL IsMms(const uni_char* type) {return uni_str_eq(MESSAGE_TYPE_MMS, type);}
	static BOOL IsEmail(const uni_char* type) {return uni_str_eq(MESSAGE_TYPE_EMAIL, type);}
	static const uni_char* Sms() {return MESSAGE_TYPE_SMS;}
	static const uni_char* Mms() {return MESSAGE_TYPE_MMS;}
	static const uni_char* Email() {return MESSAGE_TYPE_EMAIL;}

	static OpMessaging::Message::Type FromString(const uni_char* type);
	static const uni_char* ToString(OpMessaging::Message::Type type);
protected:
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILMESSAGETYPES_H
