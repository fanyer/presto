/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 *
 */

#ifndef DOM_DOMJILMESSAGE_H
#define DOM_DOMJILMESSAGE_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpMessaging.h"

class DOM_JILString_Array;
class DOM_JILAttachment_Array;
class DOM_JILString_Array;
class FindMessagesCallbackAsyncImpl;

class DOM_JILMessage : public DOM_JILObject
{
	friend class DOM_JILMessage_Constructor;
	friend class FindMessagesCallbackAsyncImpl;
public:
	static OP_STATUS Make(DOM_JILMessage*& new_obj, DOM_Runtime* origining_runtime, const OpMessaging::Message* message_contents = NULL);
	static OP_STATUS Make(DOM_JILMessage*& new_message, DOM_Runtime* runtime, const DOM_JILMessage* to_copy);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_MESSAGE || DOM_JILObject::IsA(type); }

	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(addAddress);
	DOM_DECLARE_FUNCTION(addAttachment);
	DOM_DECLARE_FUNCTION(deleteAddress);
	DOM_DECLARE_FUNCTION(deleteAttachment);
	DOM_DECLARE_FUNCTION(saveAttachment);
	enum { FUNCTIONS_ARRAY_SIZE = 6 };

	OP_STATUS SetType(OpMessaging::Message::Type type);
	OpMessaging::Message::Type GetType();
	OP_STATUS GetMessageCopy(OpMessaging::Message*& message, ES_Value* return_value, DOM_Runtime* origining_runtime);
	OP_STATUS SetMessage(DOM_Runtime *origining_runtime, const OpMessaging::Message* message_contents);
	virtual ~DOM_JILMessage();

	/** Checks:
	 *		callbackNumber - should be proper for message type
	 *		destinationAddress[] - should have at least one address,
	 *		destinationAddress[] - should be proper for message type
	 *		ccAddress[] - should be proper for message type
	 *		bccAddress[] - should be proper for message type
	 *		messageType - should be set to MMS, SMS or Email
	 *		subject - should be empty for SMS
	 *		validityPeriodHours - should be absent in Email
	 *		TODO: attachments - their existence should be checked by platform code?
	 * Doesn't check:
	 *		body - can be empty
	 *		isRead - any value is OK
	 *		messageId - it's read only
	 *		messagePriority - any value is OK
	 *		sourceAddress - it's read only
	 *		time - it's read only
	 */
	static OP_STATUS CheckParameters(OpMessaging::Message* message, ES_Value* return_value, DOM_Runtime* origining_runtime);
private:

	static BOOL ValidateMMSAddresses(OpAutoVector<OpString>* addresses);
	static BOOL ValidatePhoneNumbers(OpAutoVector<OpString>* addresses);
	static BOOL ValidateEmails(OpAutoVector<OpString>* addresses);

	static int AddToArray(ES_Object* self, DOM_JILString_Array& to_array, BOOL strip_numbers, const uni_char* strings_to_add_const, ES_Value* return_value, DOM_Runtime* origining_runtime);
	static int RemoveFromArray(ES_Object* self, DOM_JILString_Array& from_array, const uni_char* strings_to_remove_const, ES_Value* return_value, DOM_Runtime* origining_runtime);

	/** JIL 1.2.1 specification in Message.addAddress states we should remove
	 * all characters which are not 0123456789+*# from phone numbers.
	 */
	static OP_STATUS StripPhoneNumber(const uni_char* number, TempBuffer* stripped_number);
	static int HandleError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime);
	DOM_JILMessage();

	struct Null_or_undef_st {
		unsigned int attachments:2;
		unsigned int body:2;
		unsigned int callbackNumber:2;
		unsigned int isRead:2;
		unsigned int bccAddresses:2;
		unsigned int ccAddresses:2;
		unsigned int destinationAddresses:2;
		unsigned int messageId:2;
		unsigned int messagePriority:2;
		unsigned int messageType:2;
		unsigned int sourceAddress:2;
		unsigned int time:2;
		unsigned int subject:2;
		unsigned int validityPeriodHours:2;
	} m_undefnull;

	BOOL m_isRead;
	OpMessaging::Message::Priority m_messagePriority;
	DOM_JILAttachment_Array* m_attachments;
	DOM_JILString_Array* m_bccAddresses;
	DOM_JILString_Array* m_ccAddresses;
	DOM_JILString_Array* m_destinationAddresses;
	OpString m_body;
	OpString m_callbackNumber;
	OpString m_messageId;
	OpString m_messageType;
	OpString m_sourceAddress;
	OpString m_subject;
	double m_time; // Milliseconds since UTC midnight, 1st January, 1970.
	double m_validityPeriodHours; // Milliseconds since UTC midnight, 1st January, 1970.
};

class DOM_JILMessage_Constructor : public DOM_BuiltInConstructor
{
public:
	DOM_JILMessage_Constructor() : DOM_BuiltInConstructor(DOM_Runtime::JIL_MESSAGE_PROTOTYPE) {}
	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILMESSAGE_H
