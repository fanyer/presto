/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 *
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilmessage.h"
#include "modules/dom/src/domjil/domjilmessaging.h"
#include "modules/dom/src/domjil/domjilwidget.h"
#include "modules/dom/src/domsuspendcallback.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domjil/domjilattachment.h"
#include "modules/dom/src/domjil/domjilmessagetypes.h"
#include "modules/dom/src/domjil/utils/jilutils.h"
#include "modules/dom/src/domjil/utils/jilstringarray.h"
#include "modules/device_api/jil/JILFSMgr.h"
#include "modules/forms/webforms2support.h"

/* static */ OP_STATUS
DOM_JILMessage::Make(DOM_JILMessage*& new_obj, DOM_Runtime* origining_runtime, const OpMessaging::Message* message_contents)
{
	new_obj = OP_NEW(DOM_JILMessage, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::JIL_MESSAGE_PROTOTYPE), "Message"));

	new_obj->m_undefnull.body = IS_UNDEF;
	new_obj->m_undefnull.isRead = IS_UNDEF;
	new_obj->m_undefnull.callbackNumber = IS_UNDEF;
	new_obj->m_undefnull.attachments = IS_UNDEF;
	new_obj->m_undefnull.bccAddresses = IS_UNDEF;
	new_obj->m_undefnull.ccAddresses = IS_UNDEF;
	new_obj->m_undefnull.destinationAddresses = IS_UNDEF;
	new_obj->m_undefnull.messageId = IS_UNDEF;
	new_obj->m_undefnull.messagePriority = IS_UNDEF;
	new_obj->m_undefnull.messageType = IS_UNDEF;
	new_obj->m_undefnull.sourceAddress = IS_UNDEF;
	new_obj->m_undefnull.time = IS_UNDEF;
	new_obj->m_undefnull.subject = IS_UNDEF;
	new_obj->m_undefnull.validityPeriodHours = IS_UNDEF;
	if (message_contents)
		RETURN_IF_ERROR(new_obj->SetMessage(origining_runtime, message_contents));
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_JILMessage::Make(DOM_JILMessage*& new_message, DOM_Runtime* runtime, const DOM_JILMessage* to_copy)
{
	RETURN_IF_ERROR(DOM_JILMessage::Make(new_message, runtime));

	new_message->m_undefnull.isRead = to_copy->m_undefnull.isRead;
	new_message->m_undefnull.messagePriority = to_copy->m_undefnull.messagePriority;
	new_message->m_undefnull.attachments = to_copy->m_undefnull.attachments;
	new_message->m_undefnull.bccAddresses = to_copy->m_undefnull.bccAddresses;
	new_message->m_undefnull.ccAddresses = to_copy->m_undefnull.ccAddresses;
	new_message->m_undefnull.destinationAddresses = to_copy->m_undefnull.destinationAddresses;
	new_message->m_undefnull.body = to_copy->m_undefnull.body;
	new_message->m_undefnull.callbackNumber = to_copy->m_undefnull.callbackNumber;
	new_message->m_undefnull.messageId = to_copy->m_undefnull.messageId;
	new_message->m_undefnull.messageType = to_copy->m_undefnull.messageType;
	new_message->m_undefnull.sourceAddress = to_copy->m_undefnull.sourceAddress;
	new_message->m_undefnull.subject = to_copy->m_undefnull.subject;
	new_message->m_undefnull.time = to_copy->m_undefnull.time;
	new_message->m_undefnull.validityPeriodHours = to_copy->m_undefnull.validityPeriodHours;

	new_message->m_isRead = to_copy->m_isRead;
	new_message->m_messagePriority = to_copy->m_messagePriority;
	new_message->m_time = to_copy->m_time;
	new_message->m_validityPeriodHours = to_copy->m_validityPeriodHours;
	if (to_copy->m_undefnull.attachments == IS_VALUE)
		RETURN_IF_ERROR(to_copy->m_attachments->GetCopy(new_message->m_attachments, runtime));

	if (to_copy->m_undefnull.bccAddresses == IS_VALUE)
		RETURN_IF_ERROR(to_copy->m_bccAddresses->GetCopy(new_message->m_bccAddresses, runtime));
	if (to_copy->m_undefnull.ccAddresses == IS_VALUE)
		RETURN_IF_ERROR(to_copy->m_ccAddresses->GetCopy(new_message->m_ccAddresses, runtime));
	if (to_copy->m_undefnull.destinationAddresses == IS_VALUE)
		RETURN_IF_ERROR(to_copy->m_destinationAddresses->GetCopy(new_message->m_destinationAddresses, runtime));
	RETURN_IF_ERROR(new_message->m_body.Set(to_copy->m_body));
	RETURN_IF_ERROR(new_message->m_callbackNumber.Set(to_copy->m_callbackNumber));
	RETURN_IF_ERROR(new_message->m_messageId.Set(to_copy->m_messageId));
	RETURN_IF_ERROR(new_message->m_messageType.Set(to_copy->m_messageType));
	RETURN_IF_ERROR(new_message->m_sourceAddress.Set(to_copy->m_sourceAddress));
	RETURN_IF_ERROR(new_message->m_subject.Set(to_copy->m_subject));

	return OpStatus::OK;
}

DOM_JILMessage::DOM_JILMessage()
	: m_isRead(FALSE)
	, m_messagePriority(OpMessaging::Message::PriorityUnknown)
	, m_attachments(NULL)
	, m_bccAddresses(NULL)
	, m_ccAddresses(NULL)
	, m_destinationAddresses(NULL)
	, m_time(0)
	, m_validityPeriodHours(0)
{
}

DOM_JILMessage::~DOM_JILMessage()
{
}

OP_STATUS
DOM_JILMessage::SetMessage(DOM_Runtime *origining_runtime, const OpMessaging::Message* message_contents)
{
	OP_ASSERT(origining_runtime);
	OP_ASSERT(message_contents);

	RETURN_IF_ERROR(DOM_JILAttachment_Array::Make(m_attachments, origining_runtime, &message_contents->m_attachment));
	m_undefnull.attachments = IS_VALUE;
	RETURN_IF_ERROR(DOM_JILString_Array::Make(m_bccAddresses, origining_runtime, &message_contents->m_bcc_address));
	m_undefnull.bccAddresses = IS_VALUE;
	RETURN_IF_ERROR(DOM_JILString_Array::Make(m_ccAddresses, origining_runtime, &message_contents->m_cc_address));
	m_undefnull.ccAddresses = IS_VALUE;
	RETURN_IF_ERROR(DOM_JILString_Array::Make(m_destinationAddresses, origining_runtime, &message_contents->m_destination_address));
	m_undefnull.destinationAddresses = IS_VALUE;

	ES_Value val;
	RETURN_IF_ERROR(m_body.Set(message_contents->m_body));
	m_undefnull.body = m_body.HasContent() ? IS_VALUE : IS_UNDEF;

	if (message_contents->m_reply_to_address.GetCount())
		RETURN_IF_ERROR(m_callbackNumber.Set(message_contents->m_reply_to_address.Get(0)->CStr()));
	else
		m_callbackNumber.Empty();
	m_undefnull.callbackNumber = m_callbackNumber.HasContent() ? IS_VALUE : IS_UNDEF;

	m_isRead = (message_contents->m_is_read == YES);
	m_undefnull.isRead = (message_contents->m_is_read == MAYBE) ? IS_UNDEF : IS_VALUE;
	RETURN_IF_ERROR(m_messageId.Set(message_contents->m_id));
	m_undefnull.messageId = m_messageId.HasContent() ? IS_VALUE : IS_UNDEF;
	RETURN_IF_ERROR(m_sourceAddress.Set(message_contents->m_sender_address));
	m_undefnull.sourceAddress = m_sourceAddress.HasContent() ? IS_VALUE : IS_UNDEF;
	RETURN_IF_ERROR(m_subject.Set(message_contents->m_subject));
	m_undefnull.subject = m_subject.HasContent() ? IS_VALUE : IS_UNDEF;

	m_messagePriority = message_contents->m_priority;
	m_undefnull.messagePriority = (m_messagePriority == OpMessaging::Message::PriorityUnknown) ? IS_UNDEF : IS_VALUE;

	RETURN_IF_ERROR(SetType(message_contents->m_type));

	if (message_contents->m_sending_time == 0.0)
		m_undefnull.time = IS_UNDEF;
	else
	{
		m_time = message_contents->m_sending_time * 1000.0;
		m_undefnull.time = IS_VALUE;
	}

	if (message_contents->m_sending_timeout == 0.0)
		m_undefnull.validityPeriodHours = IS_UNDEF;
	else
	{
		m_validityPeriodHours = message_contents->m_sending_timeout * 1000.0;
		m_undefnull.validityPeriodHours = IS_VALUE;
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_JILMessage::SetType(OpMessaging::Message::Type type)
{
	if (type == OpMessaging::Message::TypeUnknown)
	{
		m_undefnull.messageType = IS_UNDEF;
		return OpStatus::OK;
	}
	else
	{
		m_undefnull.messageType = IS_VALUE;
		return m_messageType.Set(DOM_JILMessageTypes::ToString(type));
	}
}

OpMessaging::Message::Type DOM_JILMessage::GetType()
{
	return m_undefnull.messageType == IS_VALUE
		? DOM_JILMessageTypes::FromString(m_messageType)
		: OpMessaging::Message::TypeUnknown;
}

OP_STATUS
DOM_JILMessage::GetMessageCopy(OpMessaging::Message*& message, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	message = OP_NEW(OpMessaging::Message, ());
	RETURN_OOM_IF_NULL(message);
	OpAutoPtr<OpMessaging::Message> ap_message(message);
	if (m_undefnull.bccAddresses == IS_VALUE)
		RETURN_IF_ERROR(m_bccAddresses->GetCopy(&message->m_bcc_address, origining_runtime));
	if (m_undefnull.ccAddresses == IS_VALUE)
		RETURN_IF_ERROR(m_ccAddresses->GetCopy(&message->m_cc_address, origining_runtime));
	if (m_undefnull.destinationAddresses == IS_VALUE)
		RETURN_IF_ERROR(m_destinationAddresses->GetCopy(&message->m_destination_address, origining_runtime));
	if (m_undefnull.attachments == IS_VALUE)
	{
		ES_Value* attachment;
		int i = 0;
		while (m_attachments->GetIndex(i++, attachment))
			if (attachment->type == VALUE_OBJECT)
			{
				DOM_HOSTOBJECT_SAFE(dom_attachment, attachment->value.object, DOM_TYPE_JIL_ATTACHMENT, DOM_JILAttachment);
				OP_ASSERT(dom_attachment);
				OpMessaging::Attachment* op_attachment;
				RETURN_IF_ERROR(dom_attachment->GetCopy(op_attachment));
				RETURN_IF_ERROR(message->m_attachment.Add(op_attachment));
			}
	}
	RETURN_IF_ERROR(message->m_body.Set(m_body));
	RETURN_IF_ERROR(message->m_sender_address.Set(m_sourceAddress));
	if (m_undefnull.callbackNumber == IS_VALUE)
	{
		OpString* number = OP_NEW(OpString, ());
		RETURN_OOM_IF_NULL(number);
		OpAutoPtr<OpString> ap_number(number);
		RETURN_IF_ERROR(number->Set(m_callbackNumber));
		RETURN_IF_ERROR(message->m_reply_to_address.Add(number));
		ap_number.release();
	}
	message->m_is_read = (m_undefnull.isRead == IS_VALUE) ? m_isRead ? YES : NO : MAYBE;
	RETURN_IF_ERROR(message->m_id.Set(m_messageId));
	if (m_undefnull.messagePriority == IS_VALUE)
		message->m_priority = m_messagePriority;
	else
		message->m_priority = OpMessaging::Message::PriorityUnknown;
	if (m_undefnull.messageType == IS_VALUE)
		message->m_type = DOM_JILMessageTypes::FromString(m_messageType);
	else
		message->m_type = OpMessaging::Message::TypeUnknown;
	RETURN_IF_ERROR(message->m_sender_address.Set(m_sourceAddress));
	RETURN_IF_ERROR(message->m_subject.Set(m_subject));
	if (m_undefnull.time == IS_VALUE)
		message->m_sending_time = static_cast<time_t>(m_time / 1000.0);
	else
		message->m_sending_time = 0;
	if (m_undefnull.validityPeriodHours == IS_VALUE)
		message->m_sending_timeout = static_cast<time_t>(m_validityPeriodHours / 1000.0);
	else
		message->m_sending_timeout = 0;

	ap_message.release();
	return OpStatus::OK;
}

/* static */ BOOL
DOM_JILMessage::ValidateEmails(OpAutoVector<OpString>* addresses)
{
	for (unsigned int i_address = 0; i_address < addresses->GetCount(); i_address++)
	{
		OpString* address = addresses->Get(i_address);
		if (address->IsEmpty())
			continue;
		if (!FormValidator::IsValidEmailAddress(address->CStr()))
			return FALSE;
	}
	return TRUE;
}

/* static */ BOOL
DOM_JILMessage::ValidatePhoneNumbers(OpAutoVector<OpString>* addresses)
{
	for (unsigned int i_address = 0; i_address < addresses->GetCount(); i_address++)
	{
		OpString* address = addresses->Get(i_address);
		if (address->IsEmpty())
			continue;
		if (!JILUtils::ValidatePhoneNumber(address->CStr()))
			return FALSE;
	}
	return TRUE;
}

/* static */ BOOL
DOM_JILMessage::ValidateMMSAddresses(OpAutoVector<OpString>* addresses)
{
	for (unsigned int i_address = 0; i_address < addresses->GetCount(); i_address++)
	{
		OpString* address = addresses->Get(i_address);
		if (address->IsEmpty())
			continue;
		if (!JILUtils::ValidatePhoneNumber(address->CStr())
			&& !FormValidator::IsValidEmailAddress(address->CStr()))
			return FALSE;
	}
	return TRUE;
}

//TODO: attachments
/* static */ OP_STATUS
DOM_JILMessage::CheckParameters(OpMessaging::Message* message, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	int destination_count = message->m_destination_address.GetCount();
	BOOL has_destination = FALSE;
	for (int i_destination = 0; i_destination < destination_count; i_destination++)
		if (message->m_destination_address.Get(i_destination)->HasContent())
		{
			has_destination = TRUE;
			break;
		}
	if (!has_destination)
		return OpMessaging::Status::ERR_MESSAGE_NO_DESTINATION;

	if (!message->m_body.CStr())
		return OpMessaging::Status::ERR_MESSAGE_PARAMETER;

	// Don't check m_source_address, because we don't pass it to platform.

	switch (message->m_type)
	{
		case OpMessaging::Message::Email:
		{
			if (!ValidateEmails(&message->m_destination_address)
				|| !ValidateEmails(&message->m_bcc_address)
				|| !ValidateEmails(&message->m_cc_address)
				|| !ValidateEmails(&message->m_reply_to_address))
				return OpMessaging::Status::ERR_MESSAGE_ADDRESS;

			if (message->m_sending_timeout != 0) // magic number
				return OpMessaging::Status::ERR_MESSAGE_PARAMETER;
			break;
		}
		case OpMessaging::Message::SMS:
		{
			if (!ValidatePhoneNumbers(&message->m_destination_address)
				|| !ValidatePhoneNumbers(&message->m_bcc_address)
				|| !ValidatePhoneNumbers(&message->m_cc_address)
				|| !ValidatePhoneNumbers(&message->m_reply_to_address))
				return OpMessaging::Status::ERR_MESSAGE_ADDRESS;

			if (message->m_subject.HasContent())
				return OpMessaging::Status::ERR_MESSAGE_PARAMETER_NOT_EMPTY;
			break;
		}
		case OpMessaging::Message::MMS:
		{
			if (!ValidateMMSAddresses(&message->m_destination_address)
				|| !ValidateMMSAddresses(&message->m_bcc_address)
				|| !ValidateMMSAddresses(&message->m_cc_address)
				|| !ValidateMMSAddresses(&message->m_reply_to_address))
				return OpMessaging::Status::ERR_MESSAGE_ADDRESS;
			break;
		}
		case OpMessaging::Message::TypeUnknown:
			return OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE;
	}

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_JILMessage::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_attachments:
		{
			if (value)
			{
				switch (m_undefnull.attachments)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetObject(value, m_attachments); break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_bccAddress:
		{
			if (value)
			{
				switch (m_undefnull.bccAddresses)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetObject(value, m_bccAddresses); break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_body:
		{
			if (value)
			{
				switch (m_undefnull.body)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetString(value, m_body.CStr()); break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_callbackNumber:
		{
			if (value)
			{
				switch (m_undefnull.callbackNumber)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetString(value, m_callbackNumber.CStr()); break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_ccAddress:
		{
			if (value)
			{
				switch (m_undefnull.ccAddresses)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetObject(value, m_ccAddresses); break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_destinationAddress:
		{
			if (value)
			{
				switch (m_undefnull.destinationAddresses)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetObject(value, m_destinationAddresses); break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_isRead:
		{
			if (value)
			{
				switch (m_undefnull.isRead)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetBoolean(value, m_isRead); break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_messageId:
		{
			if (value)
			{
				switch (m_undefnull.messageId)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetString(value, m_messageId.CStr()); break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_messagePriority:
		{
			if (value)
			{
				switch (m_undefnull.messagePriority)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE:
						switch (m_messagePriority)
						{
							case OpMessaging::Message::Highest:
							case OpMessaging::Message::High:
								DOMSetBoolean(value, TRUE);
								break;
							case OpMessaging::Message::Normal:
							case OpMessaging::Message::Low:
							case OpMessaging::Message::Lowest:
								DOMSetBoolean(value, FALSE);
								break;
						}
						break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_messageType:
		{
			if (value)
			{
				switch (m_undefnull.messageType)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetString(value, m_messageType.CStr()); break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_sourceAddress:
		{
			if (value)
			{
				switch (m_undefnull.sourceAddress)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetString(value, m_sourceAddress.CStr()); break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_subject:
		{
			if (value)
			{
				switch (m_undefnull.subject)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetString(value, m_subject.CStr()); break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_time:
		{
			if (value)
			{
				switch (m_undefnull.time)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE:
					{
						ES_Value ms;
						DOMSetNumber(&ms, m_time);
						ES_Object* date;
						GET_FAILED_IF_ERROR(GetRuntime()->CreateNativeObject(ms, ENGINE_DATE_PROTOTYPE, &date));
						DOMSetObject(value, date);
						break;
					}
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_validityPeriodHours:
		{
			if (value)
			{
				switch (m_undefnull.validityPeriodHours)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE:
						time_t now;
						op_time(&now);
						int hours = static_cast<int>(((m_validityPeriodHours / 1000.0) - now) / (60*60));
						DOMSetNumber(value, hours > 0 ? hours : 0);
						break;
				}
			}
			return GET_SUCCESS;
		}
	}
	return GET_FAILED;
}

/* virtual */ void
DOM_JILMessage::GCTrace()
{
	DOM_JILObject::GCTrace();
	if (m_attachments)
		GCMark(m_attachments);
	if (m_bccAddresses)
		GCMark(m_bccAddresses);
	if (m_ccAddresses)
		GCMark(m_ccAddresses);
	if (m_destinationAddresses)
		GCMark(m_destinationAddresses);
}

/* virtual */ ES_PutState
DOM_JILMessage::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_messageId:
			return PUT_SUCCESS; // read only
		case OP_ATOM_sourceAddress:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.sourceAddress = IS_NULL; m_sourceAddress.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.sourceAddress = IS_UNDEF; m_sourceAddress.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_sourceAddress.Set(value->value.string));
					m_undefnull.sourceAddress = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_time:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.time = IS_NULL; break;
				case VALUE_UNDEFINED:
					m_undefnull.time = IS_UNDEF; break;
				case VALUE_OBJECT:
				{
					if (op_strcmp(origining_runtime->GetClass(value->value.object), "Date") != 0)
						return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
					ES_Value date_val;
					PUT_FAILED_IF_ERROR(GetRuntime()->GetNativeValueOf(value->value.object, &date_val));
					m_undefnull.time = IS_VALUE;
					m_time = date_val.value.number;
					break;
				}
				default:
					return PUT_NEEDS_NUMBER;
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_attachments:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					if (m_undefnull.attachments == IS_VALUE)
						m_attachments->Empty();
					m_undefnull.attachments = IS_NULL;
					break;
				case VALUE_UNDEFINED:
					if (m_undefnull.attachments == IS_VALUE)
						m_attachments->Empty();
					m_undefnull.attachments = IS_UNDEF;
					break;
				case VALUE_OBJECT:
				{
					ES_Value array_len_val;
					if (op_strcmp(origining_runtime->GetClass(value->value.object), "Array") == 0
						&& origining_runtime->GetName(value->value.object, UNI_L("length"), &array_len_val) == OpBoolean::IS_TRUE
						&& array_len_val.type == VALUE_NUMBER && op_isfinite(array_len_val.value.number) && array_len_val.value.number >= 0)
					{
						DOM_JILAttachment_Array* new_attachments;
						int len = static_cast<int>(array_len_val.value.number);
						if (len < 0)
							return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
						OP_STATUS error = DOM_JILAttachment_Array::Make(new_attachments, origining_runtime, value->value.object, len);
						if (OpStatus::IsError(error))
							return error == OpStatus::ERR_NO_MEMORY ? PUT_NO_MEMORY : DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
						m_attachments = new_attachments;
						m_undefnull.attachments = IS_VALUE;
					}
					else
					{
						DOM_HOSTOBJECT_SAFE(dom_attachment_array, value->value.object, DOM_TYPE_JIL_ATTACHMENT_ARRAY, DOM_JILAttachment_Array);
						if (!dom_attachment_array)
							return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
						DOM_JILAttachment_Array* new_attachments;
						PUT_FAILED_IF_ERROR(dom_attachment_array->GetCopy(new_attachments, origining_runtime));
						m_attachments = dom_attachment_array;
						m_undefnull.attachments = IS_VALUE;
					}
					break;
				}
				default:
					return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_bccAddress:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					if (m_undefnull.bccAddresses == IS_VALUE)
						m_bccAddresses->Empty();
					m_undefnull.bccAddresses = IS_NULL;
					break;
				case VALUE_UNDEFINED:
					if (m_undefnull.bccAddresses == IS_VALUE)
						m_bccAddresses->Empty();
					m_undefnull.bccAddresses = IS_UNDEF;
					break;
				case VALUE_OBJECT:
				{
					ES_Value array_len_val;
					if (op_strcmp(origining_runtime->GetClass(value->value.object), "Array") == 0
						&& origining_runtime->GetName(value->value.object, UNI_L("length"), &array_len_val) == OpBoolean::IS_TRUE
						&& array_len_val.type == VALUE_NUMBER && op_isfinite(array_len_val.value.number) && array_len_val.value.number >= 0)
					{
						DOM_JILString_Array* new_bccAddresses;
						int len = static_cast<int>(array_len_val.value.number);
						if (len < 0)
							return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
						OP_STATUS error = DOM_JILString_Array::Make(new_bccAddresses, origining_runtime, value->value.object, len);
						if (OpStatus::IsError(error))
							return error == OpStatus::ERR_NO_MEMORY ? PUT_NO_MEMORY : DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
						m_bccAddresses = new_bccAddresses;
						m_undefnull.bccAddresses = IS_VALUE;
					}
					else
					{
						DOM_HOSTOBJECT_SAFE(dom_address_array, value->value.object, DOM_TYPE_JIL_STRING_ARRAY, DOM_JILString_Array);
						if (!dom_address_array)
							return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
						DOM_JILString_Array* new_bccAddresses;
						PUT_FAILED_IF_ERROR(dom_address_array->GetCopy(new_bccAddresses, origining_runtime));
						m_bccAddresses = new_bccAddresses;
						m_undefnull.bccAddresses = IS_VALUE;
					}
					break;
				}
				default:
					return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_ccAddress:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					if (m_undefnull.ccAddresses == IS_VALUE)
						m_ccAddresses->Empty();
					m_undefnull.ccAddresses = IS_NULL;
					break;
				case VALUE_UNDEFINED:
					if (m_undefnull.ccAddresses == IS_VALUE)
						m_ccAddresses->Empty();
					m_undefnull.ccAddresses = IS_UNDEF;
					break;
				case VALUE_OBJECT:
				{
					ES_Value array_len_val;
					if (op_strcmp(origining_runtime->GetClass(value->value.object), "Array") == 0
						&& origining_runtime->GetName(value->value.object, UNI_L("length"), &array_len_val) == OpBoolean::IS_TRUE
						&& array_len_val.type == VALUE_NUMBER && op_isfinite(array_len_val.value.number) && array_len_val.value.number >= 0)
					{
						DOM_JILString_Array* new_ccAddresses;
						int len = static_cast<int>(array_len_val.value.number);
						if (len < 0)
							return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
						OP_STATUS error = DOM_JILString_Array::Make(new_ccAddresses, origining_runtime, value->value.object, len);
						if (OpStatus::IsError(error))
							return error == OpStatus::ERR_NO_MEMORY ? PUT_NO_MEMORY : DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
						m_ccAddresses = new_ccAddresses;
						m_undefnull.ccAddresses = IS_VALUE;
					}
					else
					{
						DOM_HOSTOBJECT_SAFE(dom_address_array, value->value.object, DOM_TYPE_JIL_STRING_ARRAY, DOM_JILString_Array);
						if (!dom_address_array)
							return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
						DOM_JILString_Array* new_ccAddresses;
						PUT_FAILED_IF_ERROR(dom_address_array->GetCopy(new_ccAddresses, origining_runtime));
						m_ccAddresses = new_ccAddresses;
						m_undefnull.ccAddresses = IS_VALUE;
					}
					break;
				}
				default:
					return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_destinationAddress:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					if (m_undefnull.destinationAddresses == IS_VALUE)
						m_destinationAddresses->Empty();
					m_undefnull.destinationAddresses = IS_NULL;
					break;
				case VALUE_UNDEFINED:
					if (m_undefnull.destinationAddresses == IS_VALUE)
						m_destinationAddresses->Empty();
					m_undefnull.destinationAddresses = IS_UNDEF;
					break;
				case VALUE_OBJECT:
				{
					ES_Value array_len_val;
					if (op_strcmp(origining_runtime->GetClass(value->value.object), "Array") == 0
						&& origining_runtime->GetName(value->value.object, UNI_L("length"), &array_len_val) == OpBoolean::IS_TRUE
						&& array_len_val.type == VALUE_NUMBER && op_isfinite(array_len_val.value.number) && array_len_val.value.number >= 0)
					{
						DOM_JILString_Array* new_destinationAddresses;
						int len = static_cast<int>(array_len_val.value.number);
						if (len < 0)
							return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
						OP_STATUS error = DOM_JILString_Array::Make(new_destinationAddresses, origining_runtime, value->value.object, len);
						if (OpStatus::IsError(error))
							return error == OpStatus::ERR_NO_MEMORY ? PUT_NO_MEMORY : DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
						m_destinationAddresses = new_destinationAddresses;
						m_undefnull.destinationAddresses = IS_VALUE;
					}
					else
					{
						DOM_HOSTOBJECT_SAFE(dom_address_array, value->value.object, DOM_TYPE_JIL_STRING_ARRAY, DOM_JILString_Array);
						if (!dom_address_array)
							return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
						DOM_JILString_Array* new_destinationAddresses;
						PUT_FAILED_IF_ERROR(dom_address_array->GetCopy(new_destinationAddresses, origining_runtime));
						m_destinationAddresses = new_destinationAddresses;
						m_undefnull.destinationAddresses = IS_VALUE;
					}
					break;
				}
				default:
					return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_body:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.body = IS_NULL; m_body.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.body = IS_UNDEF; m_body.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_body.Set(value->value.string));
					m_undefnull.body = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_callbackNumber:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.callbackNumber = IS_NULL; m_callbackNumber.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.callbackNumber = IS_UNDEF; m_callbackNumber.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_callbackNumber.Set(value->value.string));
					m_undefnull.callbackNumber = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_isRead:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.isRead = IS_NULL; break;
				case VALUE_UNDEFINED:
					m_undefnull.isRead = IS_UNDEF; break;
				case VALUE_BOOLEAN:
					m_undefnull.isRead = IS_VALUE; m_isRead = value->value.boolean; break;
				default:
					return PUT_NEEDS_BOOLEAN;
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_messagePriority:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.messagePriority = IS_NULL; break;
				case VALUE_UNDEFINED:
					m_undefnull.messagePriority = IS_UNDEF; break;
				case VALUE_BOOLEAN:
					m_undefnull.messagePriority = IS_VALUE;
					m_messagePriority = value->value.boolean ? OpMessaging::Message::Highest : OpMessaging::Message::Normal;
					break;
				default:
					return PUT_NEEDS_BOOLEAN;
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_messageType:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.messageType = IS_NULL; m_messageType.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.messageType = IS_UNDEF; m_messageType.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_messageType.Set(value->value.string));
					m_undefnull.messageType = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_subject:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.subject = IS_NULL; m_subject.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.subject = IS_UNDEF; m_subject.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_subject.Set(value->value.string));
					m_undefnull.subject = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_validityPeriodHours:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.validityPeriodHours = IS_NULL; break;
				case VALUE_UNDEFINED:
					m_undefnull.validityPeriodHours = IS_UNDEF; break;
				case VALUE_NUMBER:
					if (!op_isfinite(value->value.number) || value->value.number < 0)
						return PUT_SUCCESS;
					m_undefnull.validityPeriodHours = IS_VALUE;
					time_t now;
					op_time(&now);
					m_validityPeriodHours = (now + (value->value.number * 60*60)) * 1000.0;
					break;
				default:
					return PUT_NEEDS_NUMBER;
			}
			return PUT_SUCCESS;
		}
	}
	return PUT_FAILED;
}

/* static */ int
DOM_JILMessage::addAddress(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGE, DOM_JILMessage);
	DOM_CHECK_ARGUMENTS_JIL("ss");
	const uni_char* address_type = argv[0].value.string;
	const uni_char* addresses_to_add = argv[1].value.string;

	DOM_JILString_Array* to_array = NULL;
	if (uni_strncmp(address_type, UNI_L("bcc"), 4) == 0)
	{
		if (self->m_undefnull.bccAddresses != IS_VALUE)
		{
			CALL_FAILED_IF_ERROR(DOM_JILString_Array::Make(self->m_bccAddresses, origining_runtime));
			self->m_undefnull.bccAddresses = IS_VALUE;
		}
		to_array = self->m_bccAddresses;
	}
	else if (uni_strncmp(address_type, UNI_L("cc"), 3) == 0)
	{
		if (self->m_undefnull.ccAddresses != IS_VALUE)
		{
			CALL_FAILED_IF_ERROR(DOM_JILString_Array::Make(self->m_ccAddresses, origining_runtime));
			self->m_undefnull.ccAddresses = IS_VALUE;
		}
		to_array = self->m_ccAddresses;
	}
	else if (uni_strncmp(address_type, UNI_L("destination"), 12) == 0)
	{
		if (self->m_undefnull.destinationAddresses != IS_VALUE)
		{
			CALL_FAILED_IF_ERROR(DOM_JILString_Array::Make(self->m_destinationAddresses, origining_runtime));
			self->m_undefnull.destinationAddresses = IS_VALUE;
		}
		to_array = self->m_destinationAddresses;
	}
	else
		return HandleError(OpStatus::ERR_OUT_OF_RANGE, return_value, origining_runtime);
	OP_ASSERT(to_array);

	BOOL strip_numbers = FALSE;
	if (self->m_undefnull.messageType == IS_VALUE && self->m_messageType.HasContent())
		strip_numbers = DOM_JILMessageTypes::FromString(self->m_messageType.CStr()) == OpMessaging::Message::SMS;
	return DOM_JILMessage::AddToArray(*self, *to_array, strip_numbers, addresses_to_add, return_value, origining_runtime);
}

/* static */ int
DOM_JILMessage::addAttachment(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGE, DOM_JILMessage);
	DOM_CHECK_ARGUMENTS_JIL("s");

	if (!g_DOM_jilUtils->RuntimeHasFilesystemAccess(self->GetRuntime()))
		return CallException(DOM_Object::JIL_SECURITY_ERR, return_value, origining_runtime, UNI_L("File access not permitted"));

	OpString system_filename;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DAPI_jil_fs_mgr->JILToSystemPath(argv[0].value.string, system_filename), HandleError);

	DOM_JILAttachment* dom_attachment;
	OP_STATUS error = DOM_JILAttachment::Make(dom_attachment, origining_runtime, 0, system_filename.CStr());
	if (OpStatus::IsError(error))
		return HandleError(error, return_value, origining_runtime);
	ES_Value attachment;
	DOMSetObject(&attachment, dom_attachment);

	if (self->m_undefnull.attachments != IS_VALUE)
	{
		CALL_FAILED_IF_ERROR(DOM_JILAttachment_Array::Make(self->m_attachments, origining_runtime));
		self->m_undefnull.attachments = IS_VALUE;
	}

	int array_len = self->m_attachments->GetLength();
	ES_PutState put_error = self->m_attachments->PutIndex(array_len, &attachment, origining_runtime);
	if (put_error == PUT_NO_MEMORY)
		return HandleError(OpStatus::ERR_NO_MEMORY, return_value, origining_runtime);
	OP_ASSERT(put_error == PUT_SUCCESS);
	return ES_FAILED;
}

/* static */ int
DOM_JILMessage::deleteAddress(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGE, DOM_JILMessage);
	DOM_CHECK_ARGUMENTS_JIL("ss");
	const uni_char* address_type = argv[0].value.string;
	const uni_char* addresses_to_delete = argv[1].value.string;

	DOM_JILString_Array* from_array = NULL;
	if (uni_strncmp(address_type, UNI_L("bcc"), 4) == 0)
	{
		if (self->m_undefnull.bccAddresses != IS_VALUE)
			return DOM_JILMessage::HandleError(OpMessaging::Status::ERR_MESSAGE_ADDRESS, return_value, origining_runtime);
		from_array = self->m_bccAddresses;
	}
	else if (uni_strncmp(address_type, UNI_L("cc"), 3) == 0)
	{
		if (self->m_undefnull.ccAddresses != IS_VALUE)
			return DOM_JILMessage::HandleError(OpMessaging::Status::ERR_MESSAGE_ADDRESS, return_value, origining_runtime);
		from_array = self->m_ccAddresses;
	}
	else if (uni_strncmp(address_type, UNI_L("destination"), 12) == 0)
	{
		if (self->m_undefnull.destinationAddresses != IS_VALUE)
			return DOM_JILMessage::HandleError(OpMessaging::Status::ERR_MESSAGE_ADDRESS, return_value, origining_runtime);
		from_array = self->m_destinationAddresses;
	}
	else
		return HandleError(OpStatus::ERR_OUT_OF_RANGE, return_value, origining_runtime);
	OP_ASSERT(from_array);

	return DOM_JILMessage::RemoveFromArray(*self, *from_array, addresses_to_delete, return_value, origining_runtime);
}

int
DOM_JILMessage::deleteAttachment(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGE, DOM_JILMessage);
	DOM_CHECK_ARGUMENTS_JIL("o");

	DOM_HOSTOBJECT_SAFE(to_remove, argv[0].value.object, DOM_TYPE_JIL_ATTACHMENT, DOM_JILAttachment);
	if (!to_remove)
		return HandleError(OpMessaging::Status::ERR_MESSAGE_ATTACHMENT, return_value, origining_runtime);

	if (self->m_undefnull.attachments != IS_VALUE)
		return DOM_JILMessage::HandleError(OpMessaging::Status::ERR_MESSAGE_ATTACHMENT, return_value, origining_runtime);

	OP_STATUS error = self->m_attachments->Remove(*to_remove);
	if (OpStatus::IsError(error))
		return HandleError(error, return_value, origining_runtime);

	return ES_FAILED;
}

class DOM_JILMessageSaveAttachmentCallback : public DOM_SuspendCallback<OpMessaging::SaveAttachmentCallback>
{
public:
	DOM_JILMessageSaveAttachmentCallback(DOM_Runtime* runtime, DOM_JILAttachment* dom_to_save)
		: m_runtime(runtime)
		, m_dom_to_save(dom_to_save)
		, m_to_save(NULL)
		, m_messaging(NULL)
		, m_constructed(FALSE)
	{
	}
	~DOM_JILMessageSaveAttachmentCallback()
	{
		OP_DELETE(m_to_save);
	}
	virtual OP_STATUS Construct()
	{
		if (m_constructed)
			return OpStatus::OK;
		m_constructed = TRUE;

		RETURN_IF_ERROR(m_dom_to_save->GetCopy(m_to_save));

		ES_Value widget_val;
		ES_Value messaging_val;
		// I'm sure following operations will succeed.
		m_runtime->GetEnvironment()->GetWindow()->GetPrivate(DOM_PRIVATE_jilWidget, &widget_val);
		DOM_HOSTOBJECT_SAFE(dom_widget, widget_val.value.object, DOM_TYPE_JIL_WIDGET, DOM_JILWidget);
		dom_widget->Get(UNI_L("Messaging"), &messaging_val);
		DOM_HOSTOBJECT_SAFE(dom_messaging, messaging_val.value.object, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
		m_messaging = dom_messaging->m_messaging;

		return OpStatus::OK;
	}
	virtual void OnFailed(OP_STATUS error) { DOM_SuspendCallbackBase::OnFailed(error); }
	virtual void OnFinished() { OnSuccess(); }
	OpMessaging* GetMessaging() { return m_messaging; }
	OpMessaging::Attachment* GetAttachment() { return m_to_save; }
private:
	DOM_Runtime* m_runtime;
	DOM_JILAttachment* m_dom_to_save;
	OpMessaging::Attachment* m_to_save;
	OpMessaging* m_messaging;
	BOOL m_constructed;
};

int
DOM_JILMessage::saveAttachment(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGE, DOM_JILMessage);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("so");

	if (!g_DOM_jilUtils->RuntimeHasFilesystemAccess(self->GetRuntime()))
		return CallException(DOM_Object::JIL_SECURITY_ERR, return_value, origining_runtime, UNI_L("File access not permitted"));

	OpString system_path;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(g_DAPI_jil_fs_mgr->JILToSystemPath(argv[0].value.string, system_path), HandleError);

	DOM_HOSTOBJECT_SAFE(dom_to_save, argv[1].value.object, DOM_TYPE_JIL_ATTACHMENT, DOM_JILAttachment);
	if (!dom_to_save)
		return HandleError(OpMessaging::Status::ERR_MESSAGE_ATTACHMENT, return_value, origining_runtime);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessageSaveAttachmentCallback, save_attachment_callback, return_value, call, (origining_runtime, dom_to_save));
	CALL_FAILED_IF_ERROR_WITH_HANDLER(save_attachment_callback->Construct(), HandleError);
	OpMemberFunctionObject3<OpMessaging, const uni_char*, const OpMessaging::Attachment*, OpMessaging::SaveAttachmentCallback*>
		function_save_attachment(save_attachment_callback->GetMessaging(), &OpMessaging::SaveAttachment, system_path.CStr(), save_attachment_callback->GetAttachment(), save_attachment_callback);
	DOM_SUSPENDING_CALL(call, function_save_attachment, DOM_JILMessageSaveAttachmentCallback, save_attachment_callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(save_attachment_callback->GetErrorCode(), HandleError);

	return ES_FAILED;
}


/* static */ int
DOM_JILMessage::AddToArray(ES_Object* self, DOM_JILString_Array& to_array, BOOL strip_numbers, const uni_char* strings_to_add_const, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	OpString strings_to_add;
	CALL_FAILED_IF_ERROR(strings_to_add.Set(strings_to_add_const));
	int array_len =  to_array.GetLength();
	uni_char* single_to_add = uni_strtok(strings_to_add, UNI_L(";"));
	while (single_to_add)
	{
		TempBuffer buffer;
		if (strip_numbers)
		{
			OP_STATUS error = DOM_JILMessage::StripPhoneNumber(single_to_add, &buffer);
			if (OpStatus::IsError(error))
				return HandleError(error, return_value, origining_runtime);
		}
		else
			CALL_FAILED_IF_ERROR(buffer.Append(single_to_add));

		ES_Value to_add;
		DOMSetString(&to_add, &buffer);
		ES_PutState put_error = to_array.PutIndex(array_len++, &to_add, origining_runtime);
		if (put_error == PUT_NO_MEMORY)
			return ES_NO_MEMORY;
		OP_ASSERT(put_error == PUT_SUCCESS);

		single_to_add = uni_strtok(0, UNI_L(";"));
	}
	return ES_FAILED;
}

/* static */ int
DOM_JILMessage::RemoveFromArray(ES_Object* self, DOM_JILString_Array& from_array, const uni_char* strings_to_remove_const, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	OpString strings_to_remove;
	CALL_FAILED_IF_ERROR(strings_to_remove.Set(strings_to_remove_const));
	uni_char* single_string_to_remove = uni_strtok(strings_to_remove.CStr(), UNI_L(";"));
	while (single_string_to_remove)
	{
		OP_STATUS error = from_array.Remove(single_string_to_remove);
		if (OpStatus::IsError(error))
			return HandleError(error, return_value, origining_runtime);
		single_string_to_remove = uni_strtok(0, UNI_L(";"));
	}

	return ES_FAILED;
}


/* static */ int
DOM_JILMessage::HandleError(OP_STATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	switch (error)
	{
	case OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Wrong message type"));
	case OpMessaging::Status::ERR_MESSAGE_PARAMETER:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Invalid message parameter"));
	case OpMessaging::Status::ERR_MESSAGE_NO_DESTINATION:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("No message destination set"));
	case OpMessaging::Status::ERR_MESSAGE_ADDRESS:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Invalid message address"));
	case OpMessaging::Status::ERR_MESSAGE_ATTACHMENT:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Invalid attachment"));
	case OpMessaging::Status::ERR_MESSAGE_PARAMETER_NOT_EMPTY:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Message parameter should be null"));
	case OpStatus::ERR_OUT_OF_RANGE:
	case OpStatus::ERR_NO_SUCH_RESOURCE:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Invalid parameter"));
	case OpStatus::ERR_FILE_NOT_FOUND:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("File not found"));
	default:
		return HandleJILError(error, return_value, origining_runtime);
	}
}

/* static */ OP_STATUS
DOM_JILMessage::StripPhoneNumber(const uni_char* number, TempBuffer* stripped_number)
{
	const uni_char* allowed = UNI_L("0123456789+#*");
	OP_ASSERT(number);

	uni_char* found = uni_strpbrk(number, allowed);
	BOOL has_found = FALSE;
	while (found)
	{
		has_found = TRUE;
		RETURN_IF_ERROR(stripped_number->Append(*found));
		found = uni_strpbrk(found+1, allowed);
	}
	if (!has_found && uni_strlen(number) != 0)
		return OpMessaging::Status::ERR_MESSAGE_ADDRESS;
	return OpStatus::OK;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILMessage)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessage, DOM_JILMessage::addAddress, "addAddress", "ss-", "Message.addAddress")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessage, DOM_JILMessage::addAttachment, "addAttachment", "s-", "Message.addAttachment")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessage, DOM_JILMessage::deleteAddress, "deleteAddress", "ss-", "Message.deleteAddress")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessage, DOM_JILMessage::deleteAttachment, "deleteAttachment", "o-", "Message.deleteAttachment")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessage, DOM_JILMessage::saveAttachment, "saveAttachment", "so-", "Message.saveAttachment")
DOM_FUNCTIONS_END(DOM_JILMessage)

/* virtual */
int DOM_JILMessage_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_JILMessage* dom_message;
	CALL_FAILED_IF_ERROR(DOM_JILMessage::Make(dom_message, static_cast<DOM_Runtime*>(origining_runtime)));
	DOMSetObject(return_value, dom_message);
	return ES_VALUE;
}


#endif // DOM_JIL_API_SUPPORT
