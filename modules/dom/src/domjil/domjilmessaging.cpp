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

#include "modules/dom/src/domjil/domjilmessaging.h"
#include "modules/dom/src/domjil/domjilattachment.h"
#include "modules/dom/src/domjil/domjilmessage.h"
#include "modules/dom/src/domjil/domjilmessagetypes.h"
#include "modules/dom/src/domjil/domjilmessagequantities.h"
#include "modules/dom/src/domjil/domjilmessagefoldertypes.h"
#include "modules/dom/src/domjil/domjilemailaccount.h"
#include "modules/dom/src/domjil/utils/jilstringarray.h"
#include "modules/dom/src/domsuspendcallback.h"
#include "modules/dom/src/domasynccallback.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/device_api/WildcardCmp.h"
#include "modules/ecmascript_utils/esasyncif.h"

class DOM_JILMessagingSimpleCallback : public DOM_SuspendCallback<OpMessaging::SimpleMessagingCallback>
{
	public:
		virtual void OnFailed(OP_MESSAGINGSTATUS error) { DOM_SuspendCallbackBase::OnFailed(error); }
		virtual void OnFinished() { OnSuccess(); }
};

DOM_JILMessaging::DOM_JILMessaging()
	: m_messaging(NULL)
	, m_onMessageSendingFailureCallback(NULL)
	, m_onMessagesFoundCallback(NULL)
	, m_onMessageArrivedCallback(NULL)
{
}

DOM_JILMessaging::~DOM_JILMessaging()
{
	OP_DELETE(m_messaging);
}

/* virtual */ void
DOM_JILMessaging::GCTrace()
{
	DOM_JILObject::GCTrace();
	GCMark(m_onMessageSendingFailureCallback);
	GCMark(m_onMessagesFoundCallback);
	GCMark(m_onMessageArrivedCallback);
}

int
DOM_JILMessaging::createMessage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_JIL_MESSAGING);
	DOM_CHECK_ARGUMENTS_JIL("s");
	const uni_char* message_type_str = argv[0].value.string;

	OpMessaging::Message::Type message_type = DOM_JILMessageTypes::FromString(message_type_str);
	if (message_type == OpMessaging::Message::TypeUnknown)
		return HandleError(OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE, return_value, origining_runtime);

	DOM_JILMessage* message;
	CALL_FAILED_IF_ERROR(DOM_JILMessage::Make(message, origining_runtime));
	CALL_FAILED_IF_ERROR(message->SetType(message_type));
	DOM_Object::DOMSetObject(return_value, message);
	return ES_VALUE;
}

int
DOM_JILMessaging::sendMessage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("o");
	DOM_ARGUMENT_JIL_OBJECT(to_send, 0, DOM_TYPE_JIL_MESSAGE, DOM_JILMessage);
	OP_ASSERT(self->m_messaging);

	OpMessaging::Message* message;
	OP_STATUS error = to_send->GetMessageCopy(message, return_value, origining_runtime);
	if (OpStatus::IsError(error))
		return HandleError(error, return_value, origining_runtime);
	OpAutoPtr<OpMessaging::Message> ap_message(message);

	message->m_sender_address.Empty(); // sourceAddress and time are special cases, writable, but not passed to platform.
	message->m_sending_time = 0;

	error = DOM_JILMessage::CheckParameters(message, return_value, origining_runtime);
	if (OpStatus::IsError(error))
		return HandleError(error, return_value, origining_runtime);

	error = self->m_messaging->SendMessage(message, self->m_current_email_account.m_id.CStr());
	if (OpStatus::IsError(error))
		return HandleError(error, return_value, origining_runtime);

	ap_message.release();
	return ES_FAILED;
}

/* static */ OP_STATUS
DOM_JILMessaging::Make(DOM_JILMessaging*& new_obj, DOM_Runtime* runtime)
{
	OP_ASSERT(runtime);
	new_obj = OP_NEW(DOM_JILMessaging, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, runtime, runtime->GetPrototype(DOM_Runtime::JIL_MESSAGING_PROTOTYPE), "Messaging"));
	RETURN_IF_ERROR(OpMessaging::Create(&new_obj->m_messaging, new_obj, new_obj->m_current_email_account));

	DOM_JILMessageTypes* message_types;
	RETURN_IF_ERROR(DOM_JILMessageTypes::Make(message_types, runtime));
	ES_Value message_types_val;
	DOMSetObject(&message_types_val, message_types);

	DOM_JILMessageFolderTypes* message_folder_types;
	RETURN_IF_ERROR(DOM_JILMessageFolderTypes::Make(message_folder_types, runtime));
	ES_Value message_folder_types_val;
	DOMSetObject(&message_folder_types_val, message_folder_types);

	RETURN_IF_LEAVE(new_obj->PutFunctionL("Message", OP_NEW(DOM_JILMessage_Constructor, ()), "Message", NULL);
		new_obj->PutFunctionL("Attachment", OP_NEW(DOM_JILAttachment_Constructor, ()), "Attachment", NULL);
		new_obj->PutL("MessageTypes", message_types_val, PROP_READ_ONLY | PROP_DONT_DELETE);
		new_obj->PutL("MessageFolderTypes", message_folder_types_val, PROP_READ_ONLY | PROP_DONT_DELETE);
		new_obj->PutFunctionL("Account", OP_NEW(DOM_JILEmailAccount_Constructor, ()), "Account", NULL));

	return OpStatus::OK;
}

/* virtual */ void
DOM_JILMessaging::OnMessageSendingError(OP_MESSAGINGSTATUS error, const OpMessaging::Message* message)
{
	if (!m_onMessageSendingFailureCallback)
		return;

	ES_AsyncInterface* async = GetRuntime()->GetEnvironment()->GetAsyncInterface();
	OP_ASSERT(async);
	ES_Value argv[2];
	DOM_JILMessage* jil_message;
	RETURN_VOID_IF_ERROR(DOM_JILMessage::Make(jil_message, GetRuntime(), message));
	DOM_Object::DOMSetObject(&(argv[0]), jil_message);

	// The actual messages are specified by JIL in this exact form
	// (so no need to translate or anything).
	switch (error)
	{
		case OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE:
		case OpMessaging::Status::ERR_MESSAGE_PARAMETER:
		case OpMessaging::Status::ERR_MESSAGE_NO_DESTINATION:
		case OpMessaging::Status::ERR_MESSAGE_ADDRESS:
		case OpMessaging::Status::ERR_MESSAGE_PARAMETER_NOT_EMPTY:
		case OpMessaging::Status::ERR_MMS_TOO_BIG:
			DOM_Object::DOMSetString(&(argv[1]), UNI_L("3:Incompatible payload"));
			break;
		case OpMessaging::Status::ERR_NO_NETWORK_COVERAGE:
			DOM_Object::DOMSetString(&(argv[1]), UNI_L("1:No networking"));
			break;
		default:
			DOM_Object::DOMSetString(&(argv[1]), UNI_L("4:Unknown error"));
	};
	OpStatus::Ignore(async->CallFunction(m_onMessageSendingFailureCallback, GetNativeObject(), ARRAY_SIZE(argv), argv, NULL, NULL));
}

/* virtual */
void
DOM_JILMessaging::OnMessageArrived(const OpMessaging::Message* message)
{
	if (!m_onMessageArrivedCallback)
		return;

	ES_AsyncInterface* async = GetRuntime()->GetEnvironment()->GetAsyncInterface();
	OP_ASSERT(async);
	ES_Value argv[2];
	DOM_JILMessage* jil_message;
	RETURN_VOID_IF_ERROR(DOM_JILMessage::Make(jil_message, GetRuntime(), message));
	DOM_Object::DOMSetObject(&(argv[0]), jil_message);

	OpStatus::Ignore(async->CallFunction(m_onMessageArrivedCallback, GetNativeObject(), ARRAY_SIZE(argv), argv, NULL, NULL));
}

/* virtual */ ES_GetState
DOM_JILMessaging::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_onMessageSendingFailure:
			DOMSetObject(value, m_onMessageSendingFailureCallback);
			return GET_SUCCESS;
		case OP_ATOM_onMessagesFound:
			DOMSetObject(value, m_onMessagesFoundCallback);
			return GET_SUCCESS;
		case OP_ATOM_onMessageArrived:
			DOMSetObject(value, m_onMessageArrivedCallback);
			return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_JILMessaging::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_onMessageSendingFailure:
			switch (value->type)
			{
				case VALUE_NULL:
					m_onMessageSendingFailureCallback = NULL;
					break;
				case VALUE_OBJECT:
					m_onMessageSendingFailureCallback = value->value.object;
					break;
				default:
					return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			}
			return PUT_SUCCESS;
		case OP_ATOM_onMessagesFound:
			switch (value->type)
			{
				case VALUE_NULL:
					m_onMessagesFoundCallback = NULL;
					break;
				case VALUE_OBJECT:
					m_onMessagesFoundCallback = value->value.object;
					break;
				default:
					return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			}
			return PUT_SUCCESS;
		case OP_ATOM_onMessageArrived:
			switch (value->type)
			{
				case VALUE_NULL:
					m_onMessageArrivedCallback = NULL;
					break;
				case VALUE_OBJECT:
					m_onMessageArrivedCallback = value->value.object;
					break;
				default:
					return DOM_PUTNAME_DOMEXCEPTION(TYPE_MISMATCH_ERR);
			}
			return PUT_SUCCESS;
	}
	return PUT_FAILED;
}


int
DOM_JILMessaging::createFolder(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("ss");
	const uni_char* message_type_str = argv[0].value.string;
	const uni_char* folder_name = argv[1].value.string;

	OpMessaging::Folder folder;
	folder.m_type = DOM_JILMessageTypes::FromString(message_type_str);
	if (folder.m_type == OpMessaging::Message::TypeUnknown)
		return HandleError(OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE, return_value, origining_runtime);
	if (folder.m_type == OpMessaging::Message::Email)
	{
		if (self->m_current_email_account.m_id.IsEmpty())
			return HandleError(OpMessaging::Status::ERR_EMAIL_ACCOUNT, return_value, origining_runtime);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(folder.m_email_account_id.Set(self->m_current_email_account.m_id), HandleError);
	}
	CALL_FAILED_IF_ERROR_WITH_HANDLER(folder.m_name.Set(folder_name), HandleError);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessagingSimpleCallback, create_folder_callback, return_value, call, ());
	OpMemberFunctionObject2<OpMessaging, const OpMessaging::Folder*, OpMessaging::SimpleMessagingCallback*>
		function_create_folder(self->m_messaging, &OpMessaging::CreateFolder, &folder, create_folder_callback);
	DOM_SUSPENDING_CALL(call, function_create_folder, DOM_JILMessagingSimpleCallback, create_folder_callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(create_folder_callback->GetErrorCode(), HandleError);

	return ES_FAILED;
}

int
DOM_JILMessaging::deleteFolder(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("ss");
	const uni_char* message_type_str = argv[0].value.string;
	const uni_char* folder_name = argv[1].value.string;

	OpMessaging::Folder folder;
	folder.m_type = DOM_JILMessageTypes::FromString(message_type_str);
	if (folder.m_type == OpMessaging::Message::TypeUnknown)
		return HandleError(OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE, return_value, origining_runtime);
	if (folder.m_type == OpMessaging::Message::Email)
	{
		if (self->m_current_email_account.m_id.IsEmpty())
			return HandleError(OpMessaging::Status::ERR_EMAIL_ACCOUNT, return_value, origining_runtime);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(folder.m_email_account_id.Set(self->m_current_email_account.m_id), HandleError);
	}
	CALL_FAILED_IF_ERROR_WITH_HANDLER(folder.m_name.Set(folder_name), HandleError);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessagingSimpleCallback, delete_folder_callback, return_value, call, ());
	OpMemberFunctionObject3<OpMessaging, const OpMessaging::Folder*, OpMessaging::SimpleMessagingCallback*, BOOL>
		function_delete_folder(self->m_messaging, &OpMessaging::DeleteFolder, &folder, delete_folder_callback, TRUE);
	DOM_SUSPENDING_CALL(call, function_delete_folder, DOM_JILMessagingSimpleCallback, delete_folder_callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(delete_folder_callback->GetErrorCode(), HandleError);

	return ES_FAILED;
}

class DOM_JILMessagingGetFolderNamesCallback : public DOM_SuspendCallback<OpMessaging::GetFolderNamesCallback>
{
	public:
		DOM_JILMessagingGetFolderNamesCallback(DOM_Runtime* runtime) : m_results_array(NULL), m_runtime(runtime) {}
		virtual void OnFailed(OP_MESSAGINGSTATUS error) { DOM_SuspendCallbackBase::OnFailed(error); }
		virtual void OnFinished(const OpAutoVector<OpString>* result)
		{
			OP_STATUS error = m_runtime->CreateNativeArrayObject(&m_results_array, 0);
			if (OpStatus::IsError(error))
			{
				OnFailed(error);
				return;
			}
			for (UINT32 i = 0; i < result->GetCount(); i++)
			{
				ES_Value folder_name_val;
				DOM_Object::DOMSetString(&folder_name_val, result->Get(i)->CStr());
				error = m_runtime->PutIndex(m_results_array, i, folder_name_val);
				if (OpStatus::IsError(error))
				{
					OnFailed(error);
					return;
				}
			}
			OnSuccess();
		}
	ES_Object* m_results_array;
	DOM_Runtime* m_runtime;
};

int
DOM_JILMessaging::getFolderNames(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("s");
	const uni_char* type_str = argv[0].value.string;

	OpMessaging::Message::Type type = DOM_JILMessageTypes::FromString(type_str);
	if (type == OpMessaging::Message::TypeUnknown)
		return HandleError(OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE, return_value, origining_runtime);
	OpString email_account_id;
	if (type == OpMessaging::Message::Email)
	{
		if (self->m_current_email_account.m_id.IsEmpty())
			return HandleError(OpMessaging::Status::ERR_EMAIL_ACCOUNT, return_value, origining_runtime);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(email_account_id.Set(self->m_current_email_account.m_id), HandleError);
	}

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessagingGetFolderNamesCallback, get_folder_names_callback, return_value, call, (origining_runtime));
	OpMemberFunctionObject3<OpMessaging, const OpMessaging::Message::Type, OpMessaging::GetFolderNamesCallback*, const uni_char*>
		function_get_folder_names(self->m_messaging, &OpMessaging::GetFolderNames, type, get_folder_names_callback, email_account_id.CStr());
	DOM_SUSPENDING_CALL(call, function_get_folder_names, DOM_JILMessagingGetFolderNamesCallback, get_folder_names_callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(get_folder_names_callback->GetErrorCode(), HandleError);
	DOM_Object::DOMSetObject(return_value, get_folder_names_callback->m_results_array);

	return ES_VALUE;
}

class DOM_JILMessagingGetEmailAccountsCallback : public DOM_SuspendCallback<OpMessaging::GetEmailAccountsCallback>
{
	public:
		DOM_JILMessagingGetEmailAccountsCallback(DOM_Runtime* runtime) : m_results_array(NULL), m_runtime(runtime) {}
		virtual void OnFailed(OP_MESSAGINGSTATUS error) { DOM_SuspendCallbackBase::OnFailed(error); }
		virtual void OnFinished(const OpAutoVector<OpMessaging::EmailAccount>* result)
		{
			OP_STATUS error = m_runtime->CreateNativeArrayObject(&m_results_array, 0);
			if (OpStatus::IsError(error))
			{
				OnFailed(error);
				return;
			}
			for (UINT32 i = 0; i < result->GetCount(); i++)
			{
				ES_Value account_val;
				DOM_JILEmailAccount* dom_account;
				error = DOM_JILEmailAccount::Make(dom_account, m_runtime);
				if (OpStatus::IsError(error))
				{
					OnFailed(error);
					return;
				}
				error = dom_account->SetAccount(result->Get(i));
				if (OpStatus::IsError(error))
				{
					OnFailed(error);
					return;
				}
				DOM_Object::DOMSetObject(&account_val, dom_account);
				error = m_runtime->PutIndex(m_results_array, i, account_val);
				if (OpStatus::IsError(error))
				{
					OnFailed(error);
					return;
				}
			}
			OnSuccess();
		}
	ES_Object* m_results_array;
	DOM_Runtime* m_runtime;
};

int
DOM_JILMessaging::getEmailAccounts(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("")

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessagingGetEmailAccountsCallback, callback, return_value, call, (origining_runtime));
	OpMemberFunctionObject1<OpMessaging, OpMessaging::GetEmailAccountsCallback*>
		function(self->m_messaging, &OpMessaging::GetEmailAccounts, callback);
	DOM_SUSPENDING_CALL(call, function, DOM_JILMessagingGetEmailAccountsCallback, callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandleError);
	DOM_Object::DOMSetObject(return_value, callback->m_results_array);

	return ES_VALUE;
}

int
DOM_JILMessaging::getCurrentEmailAccount(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);

	DOM_JILEmailAccount* dom_account;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(DOM_JILEmailAccount::Make(dom_account, origining_runtime), HandleError);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(dom_account->SetAccount(&self->m_current_email_account), HandleError);
	DOMSetObject(return_value, dom_account);

	return ES_VALUE;
}

class DOM_JILMessagingSetCurrentEmailCallback : public DOM_SuspendCallback<OpMessaging::GetEmailAccountsCallback>
{
	public:
		DOM_JILMessagingSetCurrentEmailCallback(DOM_Runtime* runtime, DOM_JILMessaging* dom_messaging)
			: m_runtime(runtime), m_dom_messaging(dom_messaging) {}
		virtual void OnFailed(OP_MESSAGINGSTATUS error) { DOM_SuspendCallbackBase::OnFailed(error); }
		virtual void OnFinished(const OpAutoVector<OpMessaging::EmailAccount>* result)
		{
			if (result->GetCount() != 1)
			{
				OP_ASSERT(!"Should be one email account only");
				DOM_SuspendCallbackBase::OnFailed(OpStatus::ERR_OUT_OF_RANGE);
				return;
			}
			OpMessaging::EmailAccount* one_result = result->Get(0);
			OP_STATUS error = m_dom_messaging->m_current_email_account.m_id.Set(one_result->m_id);
			if (OpStatus::IsError(error))
			{
				DOM_SuspendCallbackBase::OnFailed(error);
				return;
			}
			error = m_dom_messaging->m_current_email_account.m_name.Set(one_result->m_name);
			if (OpStatus::IsError(error))
			{
				DOM_SuspendCallbackBase::OnFailed(error);
				return;
			}
			OnSuccess();
		}
	private:
		DOM_Runtime* m_runtime;
		DOM_JILMessaging* m_dom_messaging;
};

int
DOM_JILMessaging::setCurrentEmailAccount(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("s");
	const uni_char* account_id = argv[0].value.string;

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessagingSetCurrentEmailCallback, callback, return_value, call, (origining_runtime, self));
	OpMemberFunctionObject2<OpMessaging, const uni_char*, OpMessaging::GetEmailAccountsCallback*>
		function(self->m_messaging, &OpMessaging::GetSingleEmailAccount, account_id, callback);
	DOM_SUSPENDING_CALL(call, function, DOM_JILMessagingSetCurrentEmailCallback, callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandleError);

	return ES_FAILED;
}

int
DOM_JILMessaging::deleteEmailAccount(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("s");
	const uni_char* account_id = argv[0].value.string;

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessagingSimpleCallback, callback, return_value, call, ());
	OpMemberFunctionObject2<OpMessaging, const uni_char*, OpMessaging::SimpleMessagingCallback*>
		function(self->m_messaging, &OpMessaging::DeleteEmailAccount, account_id, callback);
	DOM_SUSPENDING_CALL(call, function, DOM_JILMessagingSimpleCallback, callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandleError);

	return ES_FAILED;
}

class DOM_JILMessagingGetMessageQuantitiesCallback : public DOM_SuspendCallback<OpMessaging::GetMessageCountCallback>
{
	public:
		DOM_JILMessagingGetMessageQuantitiesCallback(DOM_Runtime* runtime, OpMessaging::Message::Type type, const uni_char* folder_name, const uni_char* email_account_id)
			: m_dom_quantities(NULL), m_runtime(runtime), m_type(type), m_folder_name(folder_name), m_email_account_id(email_account_id) {}
		virtual OP_STATUS Construct()
		{
			m_folder.m_type = m_type;
			if (m_folder.m_type == OpMessaging::Message::Email)
			{
				if (!m_email_account_id || !*m_email_account_id)
					return OpMessaging::Status::ERR_EMAIL_ACCOUNT;
				RETURN_IF_ERROR(m_folder.m_email_account_id.Set(m_email_account_id));
			}
			RETURN_IF_ERROR(m_folder.m_name.Set(m_folder_name));
			return OpStatus::OK;
		}
		virtual void OnFailed(OP_MESSAGINGSTATUS error) { DOM_SuspendCallbackBase::OnFailed(error); }
		virtual void OnFinished(unsigned int total, unsigned int read, unsigned int unread)
		{
			OP_STATUS error = DOM_JILMessageQuantities::Make(m_dom_quantities, m_runtime, total, read, unread);
			if (OpStatus::IsError(error))
			{
				OnFailed(error);
				return;
			}
			OnSuccess();
		}

		DOM_JILMessageQuantities* m_dom_quantities;
		OpMessaging::Folder m_folder;
	private:
		DOM_Runtime* m_runtime;
		OpMessaging::Message::Type m_type;
		const uni_char* m_folder_name;
		const uni_char* m_email_account_id;
};

int
DOM_JILMessaging::getMessageQuantities(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("ss");
	const uni_char* message_type_str = argv[0].value.string;
	const uni_char* folder_name = argv[1].value.string;
	OpMessaging::Message::Type message_type = DOM_JILMessageTypes::FromString(message_type_str);
	if (message_type == OpMessaging::Message::TypeUnknown)
		return HandleError(OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE, return_value, origining_runtime);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessagingGetMessageQuantitiesCallback, callback, return_value, call,
		(origining_runtime, message_type, folder_name, self->m_current_email_account.m_id.CStr()));
	OpMemberFunctionObject2<OpMessaging, const OpMessaging::Folder*, OpMessaging::GetMessageCountCallback*>
		function(self->m_messaging, &OpMessaging::GetMessageCount, &callback->m_folder, callback);
	DOM_SUSPENDING_CALL(call, function, DOM_JILMessagingGetMessageQuantitiesCallback, callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandleError);
	DOM_Object::DOMSetObject(return_value, callback->m_dom_quantities);

	return ES_VALUE;
}

class DOM_JILMessagingGetMessageCallback : public DOM_SuspendCallback<OpMessaging::GetMessageCallback>
{
	public:
		DOM_JILMessagingGetMessageCallback(DOM_Runtime* runtime, OpMessaging::Message::Type type, const uni_char* folder_name, const uni_char* email_account_id)
			: m_dom_message(NULL), m_runtime(runtime), m_type(type), m_folder_name(folder_name), m_email_account_id(email_account_id) {}
		virtual OP_STATUS Construct()
		{
			m_folder.m_type = m_type;
			if (m_folder.m_type == OpMessaging::Message::Email)
			{
				if (!m_email_account_id || !*m_email_account_id)
					return OpMessaging::Status::ERR_EMAIL_ACCOUNT;
				RETURN_IF_ERROR(m_folder.m_email_account_id.Set(m_email_account_id));
			}
			RETURN_IF_ERROR(m_folder.m_name.Set(m_folder_name));
			return OpStatus::OK;
		}
		virtual void OnFailed(OP_MESSAGINGSTATUS error) { DOM_SuspendCallbackBase::OnFailed(error); }
		virtual void OnFinished(const OpMessaging::Message* message)
		{
			OP_ASSERT(message);
			OP_STATUS error = DOM_JILMessage::Make(m_dom_message, m_runtime, message);
			if (OpStatus::IsError(error))
			{
				OnFailed(error);
				return;
			}
			OnSuccess();
		}

		DOM_JILMessage* m_dom_message;
		OpMessaging::Folder m_folder;
	private:
		DOM_Runtime* m_runtime;
		OpMessaging::Message::Type m_type;
		const uni_char* m_folder_name;
		const uni_char* m_email_account_id;
};

int
DOM_JILMessaging::getMessage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("ssn");
	const uni_char* message_type_str = argv[0].value.string;
	const uni_char* folder_name = argv[1].value.string;
	int message_index = static_cast<int>(argv[2].value.number);

	if (message_index < 0)
		return HandleError(OpMessaging::Status::ERR_MESSAGE_PARAMETER, return_value, origining_runtime);

	OpMessaging::Message::Type message_type = DOM_JILMessageTypes::FromString(message_type_str);
	if (message_type == OpMessaging::Message::TypeUnknown)
		return HandleError(OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE, return_value, origining_runtime);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessagingGetMessageCallback, callback, return_value, call,
		(origining_runtime, message_type, folder_name, self->m_current_email_account.m_id.CStr()));
	OpMemberFunctionObject3<OpMessaging, const OpMessaging::Folder*, UINT32, OpMessaging::GetMessageCallback*>
		function(self->m_messaging, &OpMessaging::GetMessage, &callback->m_folder, message_index, callback);
	DOM_SUSPENDING_CALL(call, function, DOM_JILMessagingGetMessageCallback, callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), HandleError);
	DOM_Object::DOMSetObject(return_value, callback->m_dom_message);

	return ES_VALUE;
}

class FindMessagesCallbackAsyncImpl : public OpMessaging::MessagesSearchIterationCallback, public DOM_AsyncCallback
{
public:
	struct SearchParams
	{
		DOM_JILMessage* reference_item;
		int start_index;
		int end_index;
		SearchParams() : reference_item(NULL), start_index(0), end_index(-1){}
		~SearchParams() {}
	};

	~FindMessagesCallbackAsyncImpl()
	{
		if (m_search_params && m_search_params->reference_item)
			GetRuntime()->Unprotect(*m_search_params->reference_item);
		OP_DELETE(m_search_params);
	}

	static OP_STATUS Make(FindMessagesCallbackAsyncImpl*& new_obj, DOM_Runtime* runtime, ES_Object* this_object, ES_Object* es_callback, SearchParams* params, const uni_char* folder_name)
	{
		FindMessagesCallbackAsyncImpl* callback = OP_NEW(FindMessagesCallbackAsyncImpl,
			(runtime, this_object, es_callback, params, folder_name));
		if (!callback)
			return OpStatus::ERR_NO_MEMORY;
		OpAutoPtr<FindMessagesCallbackAsyncImpl> ap_callback(callback);
		OP_STATUS error = runtime->Protect(*callback->m_search_params->reference_item);
		if (OpStatus::IsError(error))
		{
			callback->m_search_params->reference_item = NULL;
			return error;
		}
		RETURN_IF_ERROR(callback->Construct());

		ES_Object* results_array;
		RETURN_IF_ERROR(callback->GetRuntime()->CreateNativeArrayObject(&results_array, 0));
		callback->m_results_array_protector.Reset(results_array);
		RETURN_IF_ERROR(callback->m_results_array_protector.Protect());
		new_obj = ap_callback.release();
		return OpStatus::OK;
	}

	virtual BOOL OnItem(OpMessaging::Message* result)
	{
		OP_STATUS error;
		OpAutoPtr<OpMessaging::Message> ap_result(result);
		OP_BOOLEAN matches = IsMatch(result);
		if (matches == OpBoolean::IS_FALSE)
			return TRUE; // not a match, give me next
		else if (matches != OpBoolean::IS_TRUE)
			return FALSE; // ERROR - stop iteration
		if (m_match_count < m_search_params->start_index)
		{
			// m_match_count is lower than specified start_index - skip it
			++m_match_count;
			return TRUE;
		}

		DOM_JILMessage* dom_message;
		error = DOM_JILMessage::Make(dom_message, GetRuntime(), result);
		if (OpStatus::IsError(error))
		{
			OnError(error);
			return FALSE;
		}
		ES_Value val;
		DOM_Object::DOMSetObject(&val, dom_message);
		error = GetRuntime()->PutIndex(m_results_array_protector.Get(), m_current_index, val);
		if (OpStatus::IsError(error))
		{
			OnError(OpStatus::ERR_NO_MEMORY);
			return FALSE;
		}
		++m_current_index;
		++m_match_count;
		if (m_match_count > m_search_params->end_index)
		{
			// max result reached
			OnFinished();
			return FALSE;
		}
		return TRUE;
	}

	virtual void OnFinished()
	{
		ES_Value argv[2];
		DOM_Object::DOMSetObject(&argv[0], m_results_array_protector.Get());
		DOM_Object::DOMSetString(&argv[1], m_folder_name);
		OpStatus::Ignore(CallCallback(argv, ARRAY_SIZE(argv)));
		OP_DELETE(this);
	}

	virtual void OnError(OP_MESSAGINGSTATUS error)
	{
		ES_Value argv[1];
		DOM_Object::DOMSetNull(&(argv[0]));
		OpStatus::Ignore(CallCallback(argv, ARRAY_SIZE(argv)));
		OP_DELETE(this);
	}

	OP_BOOLEAN CompareAttachmentArray(DOM_JILAttachment_Array* array_to_find, const OpAutoVector<OpMessaging::Attachment>* array_find_in)
	{
		if (array_to_find->GetLength() == 0)
			return OpBoolean::IS_TRUE;
		if (array_find_in->GetCount() == 0)
			return OpBoolean::IS_FALSE;

		ES_Value *single_to_find_val;
		for (UINT32 i_to_find = 0; array_to_find->GetIndex(i_to_find, single_to_find_val); i_to_find++)
			if (single_to_find_val->type == VALUE_OBJECT)
			{
				DOM_HOSTOBJECT_SAFE(dom_single_to_find, single_to_find_val->value.object, DOM_TYPE_JIL_ATTACHMENT, DOM_JILAttachment);
				OP_ASSERT(dom_single_to_find);
				OP_BOOLEAN matches = OpBoolean::IS_TRUE;
				for (UINT32 i_find_in = 0; i_find_in < array_find_in->GetCount(); i_find_in++)
				{
					OpMessaging::Attachment* single_to_check = array_find_in->Get(i_find_in);
					matches = OpBoolean::IS_TRUE;
					if (matches == OpBoolean::IS_TRUE && dom_single_to_find->m_undefnull.MIMEType == IS_VALUE)
						matches = JILWildcardCmp(dom_single_to_find->m_MIMEType.CStr(), single_to_check->m_mimetype.CStr());
					if (matches == OpBoolean::IS_TRUE && dom_single_to_find->m_undefnull.size == IS_VALUE)
						matches = dom_single_to_find->m_size == single_to_check->m_size ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
					if (matches == OpBoolean::IS_TRUE && dom_single_to_find->m_undefnull.fileName == IS_VALUE)
						matches = JILWildcardCmp(dom_single_to_find->m_fileName, single_to_check->m_suggested_name);
					if (matches == OpBoolean::IS_FALSE)
						continue;
					else
						break;
				}
				if (matches != OpBoolean::IS_TRUE)
					return matches;
			}
		return OpBoolean::IS_TRUE;
	}

	OP_BOOLEAN CompareStringArray(DOM_JILString_Array* array_to_find, const OpAutoVector<OpString>* array_find_in)
	{
		if (array_to_find->GetLength() == 0)
			return OpBoolean::IS_TRUE;
		if (array_find_in->GetCount() == 0)
			return OpBoolean::IS_FALSE;

		ES_Value *single_to_find_val;
		for (UINT32 i_to_find = 0; array_to_find->GetIndex(i_to_find, single_to_find_val); i_to_find++)
			if (single_to_find_val->type == VALUE_STRING)
			{
				const uni_char* single_to_find = single_to_find_val->value.string;
				OP_BOOLEAN matches = OpBoolean::IS_TRUE;
				for (UINT32 i_find_in = 0; i_find_in < array_find_in->GetCount(); i_find_in++)
				{
					const uni_char* single_to_check = array_find_in->Get(i_find_in)->CStr();
					matches = JILWildcardCmp(single_to_find, single_to_check);
					if (matches == OpBoolean::IS_FALSE)
						continue;
					else
						break;
				}
				if (matches != OpBoolean::IS_TRUE)
					return matches;
			}
		return OpBoolean::IS_TRUE;
	}

	OP_BOOLEAN IsMatch(OpMessaging::Message* item)
	{
		OP_ASSERT(item);
		OP_BOOLEAN matches = OpBoolean::IS_TRUE;

		// messageId, sourceAddress and time are read-only, so skip.
		// There's also no need to check type - it's already there in Iterate.
		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_undefnull.body == IS_VALUE)
			matches = JILWildcardCmp(m_search_params->reference_item->m_body.CStr(), item->m_body);
		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_undefnull.callbackNumber == IS_VALUE)
		{
			if (item->m_reply_to_address.GetCount())
				matches = JILWildcardCmp(m_search_params->reference_item->m_callbackNumber.CStr(), item->m_reply_to_address.Get(0)->CStr());
			else if (uni_str_eq(m_search_params->reference_item->m_callbackNumber.CStr(), UNI_L("*")))
				matches = OpBoolean::IS_TRUE;
			else
				matches = OpBoolean::IS_FALSE;
		}
		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_undefnull.subject == IS_VALUE)
			matches = JILWildcardCmp(m_search_params->reference_item->m_subject.CStr(), item->m_subject);

		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_undefnull.messagePriority == IS_VALUE)
		{
			// Priority is collapsed to "high or normal".
			if (m_search_params->reference_item->m_messagePriority == OpMessaging::Message::Highest
				&& (item->m_priority == OpMessaging::Message::Highest || item->m_priority == OpMessaging::Message::High))
					matches = OpBoolean::IS_TRUE;
			else if (m_search_params->reference_item->m_messagePriority == OpMessaging::Message::Normal
				&& (item->m_priority == OpMessaging::Message::Normal || item->m_priority == OpMessaging::Message::Low || item->m_priority == OpMessaging::Message::Lowest))
					matches = OpBoolean::IS_TRUE;
			else
				matches = OpBoolean::IS_FALSE;
		}
		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_undefnull.isRead == IS_VALUE)
		{
			if ((m_search_params->reference_item->m_isRead == TRUE && item->m_is_read == YES)
				|| m_search_params->reference_item->m_isRead == FALSE && item->m_is_read == NO)
				matches = OpBoolean::IS_TRUE;
			else
				matches = OpBoolean::IS_FALSE;
		}

		// Compare hours, so ignore those dangling milliseconds, seconds, and minutes.
		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_undefnull.validityPeriodHours == IS_VALUE)
			matches = (m_search_params->reference_item->m_validityPeriodHours / (1000 * 60 * 60) == item->m_sending_timeout / (60 * 60))
				? OpBoolean::IS_TRUE
				: OpBoolean::IS_FALSE;

		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_undefnull.attachments == IS_VALUE)
			matches = CompareAttachmentArray(m_search_params->reference_item->m_attachments, &item->m_attachment);

		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_undefnull.bccAddresses == IS_VALUE)
			matches = CompareStringArray(m_search_params->reference_item->m_bccAddresses, &item->m_bcc_address);
		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_undefnull.ccAddresses == IS_VALUE)
			matches = CompareStringArray(m_search_params->reference_item->m_ccAddresses, &item->m_cc_address);
		if (matches == OpBoolean::IS_TRUE && m_search_params->reference_item->m_undefnull.destinationAddresses == IS_VALUE)
			matches = CompareStringArray(m_search_params->reference_item->m_destinationAddresses, &item->m_destination_address);

		return matches;
	}
private:
	FindMessagesCallbackAsyncImpl(DOM_Runtime* runtime, ES_Object* this_object, ES_Object* es_callback, SearchParams* params, const uni_char* folder_name)
		: DOM_AsyncCallback(runtime, es_callback, this_object)
		, m_results_array_protector(NULL, runtime)
		, m_current_index(0)
		, m_search_params(params)
		, m_match_count(0)
		, m_folder_name(folder_name)
	{}

	DOM_AutoProtectPtr m_results_array_protector;
	int m_current_index;
	SearchParams* m_search_params;
	int m_match_count;
	const uni_char* m_folder_name;
};

int
DOM_JILMessaging::findMessages(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("osnn");
	DOM_ARGUMENT_JIL_OBJECT(to_compare, 0, DOM_TYPE_JIL_MESSAGE, DOM_JILMessage);
	const uni_char* folder_name = argv[1].value.string;
	double start_index = argv[2].value.number;
	double end_index = argv[3].value.number;

	OpAutoPtr<FindMessagesCallbackAsyncImpl::SearchParams> search_params(OP_NEW(FindMessagesCallbackAsyncImpl::SearchParams, ()));
	if (!search_params.get())
		return ES_NO_MEMORY;
	search_params->start_index = static_cast<int>(start_index);
	search_params->end_index = static_cast<int>(end_index);
	if (search_params->start_index < 0 || search_params->end_index < 0)
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Negative index value"));
	if (search_params->start_index > search_params->end_index)
	{
		if (self->m_onMessagesFoundCallback)
		{
			ES_Value arguments[2];
			ES_Object* empty_array;
			CALL_FAILED_IF_ERROR(origining_runtime->CreateNativeArrayObject(&empty_array));
			DOM_Object::DOMSetObject(&(arguments[0]), empty_array);
			DOM_Object::DOMSetString(&(arguments[1]), folder_name);
			ES_AsyncInterface* async_iface = origining_runtime->GetEnvironment()->GetAsyncInterface();
			CALL_FAILED_IF_ERROR(async_iface->CallFunction(self->m_onMessagesFoundCallback, *self, ARRAY_SIZE(arguments), arguments, NULL, NULL));
		}
		return ES_FAILED;
	}

	CALL_FAILED_IF_ERROR_WITH_HANDLER(DOM_JILMessage::Make(search_params->reference_item, origining_runtime, to_compare), HandleError);
	OpMessaging::Message::Type type = to_compare->GetType();

	FindMessagesCallbackAsyncImpl* callback;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(FindMessagesCallbackAsyncImpl::Make(callback, origining_runtime, *this_object, self->m_onMessagesFoundCallback, search_params.get(),
		uni_str_eq(folder_name, UNI_L("all")) ? NULL : folder_name), HandleError);
	search_params.release();

	self->m_messaging->Iterate(uni_str_eq(folder_name, UNI_L("all")) ? NULL : folder_name, type, callback, self->m_current_email_account.m_id.CStr());

	return ES_FAILED;
}

int
DOM_JILMessaging::deleteAllMessages(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("ss");
	const uni_char* msg_type_str = argv[0].value.string;
	const uni_char* folder_name_str = argv[1].value.string;

	OpMessaging::Message::Type message_type = DOM_JILMessageTypes::FromString(msg_type_str);
	if (message_type == OpMessaging::Message::TypeUnknown)
		return HandleError(OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE, return_value, origining_runtime);

	OpMessaging::Folder folder;
	folder.m_type = message_type;
	if (folder.m_type == OpMessaging::Message::Email)
	{
		if (self->m_current_email_account.m_id.IsEmpty())
			return HandleError(OpMessaging::Status::ERR_EMAIL_ACCOUNT, return_value, origining_runtime);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(folder.m_email_account_id.Set(self->m_current_email_account.m_id), HandleError);
	}
	CALL_FAILED_IF_ERROR_WITH_HANDLER(folder.m_name.Set(folder_name_str), HandleError);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessagingSimpleCallback, delete_message_callback, return_value, call, ());
	OpMemberFunctionObject3<OpMessaging, const OpMessaging::Folder*, const uni_char*,OpMessaging::SimpleMessagingCallback*>
		function_delete_message(self->m_messaging, &OpMessaging::DeleteMessage, &folder, NULL, delete_message_callback);
	DOM_SUSPENDING_CALL(call, function_delete_message, DOM_JILMessagingSimpleCallback, delete_message_callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(delete_message_callback->GetErrorCode(), HandleError);
	return ES_FAILED;
}

int
DOM_JILMessaging::deleteMessage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("sss");
	const uni_char* msg_type_str = argv[0].value.string;
	const uni_char* folder_name_str = argv[1].value.string;
	const uni_char* msg_id_str = argv[2].value.string;

	OpMessaging::Message::Type message_type = DOM_JILMessageTypes::FromString(msg_type_str);
	if (message_type == OpMessaging::Message::TypeUnknown)
		return HandleError(OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE, return_value, origining_runtime);

	OpMessaging::Folder folder;
	folder.m_type = message_type;
	if (folder.m_type == OpMessaging::Message::Email)
	{
		if (self->m_current_email_account.m_id.IsEmpty())
			return HandleError(OpMessaging::Status::ERR_EMAIL_ACCOUNT, return_value, origining_runtime);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(folder.m_email_account_id.Set(self->m_current_email_account.m_id), HandleError);
	}
	CALL_FAILED_IF_ERROR_WITH_HANDLER(folder.m_name.Set(folder_name_str), HandleError);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessagingSimpleCallback, delete_message_callback, return_value, call, ());
	OpMemberFunctionObject3<OpMessaging, const OpMessaging::Folder*, const uni_char*,OpMessaging::SimpleMessagingCallback*>
		function_delete_message(self->m_messaging, &OpMessaging::DeleteMessage, &folder, msg_id_str, delete_message_callback);
	DOM_SUSPENDING_CALL(call, function_delete_message, DOM_JILMessagingSimpleCallback, delete_message_callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(delete_message_callback->GetErrorCode(), HandleError);

	return ES_FAILED;
}

int
DOM_JILMessaging::copyMessageToFolder(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("os");
	DOM_ARGUMENT_OBJECT(dom_message, 0, DOM_TYPE_JIL_MESSAGE, DOM_JILMessage);
	const uni_char* folder_name_str = argv[1].value.string;

	if (dom_message->GetType() == OpMessaging::Message::TypeUnknown)
		return HandleError(OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE, return_value, origining_runtime);

	OpMessaging::Folder folder;
	folder.m_type = dom_message->GetType();
	if (folder.m_type == OpMessaging::Message::Email)
	{
		if (self->m_current_email_account.m_id.IsEmpty())
			return HandleError(OpMessaging::Status::ERR_EMAIL_ACCOUNT, return_value, origining_runtime);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(folder.m_email_account_id.Set(self->m_current_email_account.m_id), HandleError);
	}
	CALL_FAILED_IF_ERROR_WITH_HANDLER(folder.m_name.Set(folder_name_str), HandleError);

	OpMessaging::Message* op_message = NULL;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(dom_message->GetMessageCopy(op_message, return_value, origining_runtime), HandleError);
	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessagingSimpleCallback, put_message_callback, return_value, call, ());
	OpMemberFunctionObject4<OpMessaging, OpMessaging::Message *, const OpMessaging::Folder*, BOOL,OpMessaging::SimpleMessagingCallback*>
		function_put_message(self->m_messaging, &OpMessaging::PutMessage, op_message, &folder, FALSE, put_message_callback);
	DOM_SUSPENDING_CALL(call, function_put_message, DOM_JILMessagingSimpleCallback, put_message_callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(put_message_callback->GetErrorCode(), HandleError);

	return ES_FAILED;
}

int
DOM_JILMessaging::moveMessageToFolder(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(self, DOM_TYPE_JIL_MESSAGING, DOM_JILMessaging);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("os");
	DOM_ARGUMENT_OBJECT(dom_message, 0, DOM_TYPE_JIL_MESSAGE, DOM_JILMessage);
	const uni_char* folder_name_str = argv[1].value.string;

	if (dom_message->GetType() == OpMessaging::Message::TypeUnknown)
		return HandleError(OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE, return_value, origining_runtime);

	OpMessaging::Folder folder;
	folder.m_type = dom_message->GetType();
	if (folder.m_type == OpMessaging::Message::Email)
	{
		if (self->m_current_email_account.m_id.IsEmpty())
			return HandleError(OpMessaging::Status::ERR_EMAIL_ACCOUNT, return_value, origining_runtime);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(folder.m_email_account_id.Set(self->m_current_email_account.m_id), HandleError);
	}
	CALL_FAILED_IF_ERROR_WITH_HANDLER(folder.m_name.Set(folder_name_str), HandleError);

	OpMessaging::Message* op_message = NULL;
	CALL_FAILED_IF_ERROR_WITH_HANDLER(dom_message->GetMessageCopy(op_message, return_value, origining_runtime), HandleError);
	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILMessagingSimpleCallback, put_message_callback, return_value, call, ());
	OpMemberFunctionObject4<OpMessaging, OpMessaging::Message *, const OpMessaging::Folder*, BOOL,OpMessaging::SimpleMessagingCallback*>
		function_put_message(self->m_messaging, &OpMessaging::PutMessage, op_message, &folder, TRUE, put_message_callback);
	DOM_SUSPENDING_CALL(call, function_put_message, DOM_JILMessagingSimpleCallback, put_message_callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(put_message_callback->GetErrorCode(), HandleError);

	return ES_FAILED;
}

/* static */ int
DOM_JILMessaging::HandleError(OP_MESSAGINGSTATUS error, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	switch (error)
	{
	case OpMessaging::Status::ERR_WRONG_MESSAGE_TYPE:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Wrong message type"));
	case OpMessaging::Status::ERR_MESSAGE_PARAMETER:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Wrong message parameter"));
	case OpMessaging::Status::ERR_MESSAGE_NO_DESTINATION:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("No message destination address"));
	case OpMessaging::Status::ERR_MESSAGE_ADDRESS:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Wrong message address"));
	case OpMessaging::Status::ERR_MESSAGE_PARAMETER_NOT_EMPTY:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Non-empty parameter"));
	case OpMessaging::Status::ERR_NO_NETWORK_COVERAGE:
		return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("No network coverage"));
	case OpMessaging::Status::ERR_MMS_TOO_BIG:
		return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("MMS exceeded size limit"));
	case OpMessaging::Status::ERR_EMAIL_ACCOUNT:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Wrong email account"));
	case OpMessaging::Status::ERR_EMAIL_ACCUNT_IN_USE:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Email account currently in use"));
	case OpMessaging::Status::ERR_FOLDER_DOESNT_EXIST:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Folder doesn't exist"));
	case OpMessaging::Status::ERR_FOLDER_ALREADY_EXISTS:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Folder already exists"));
	case OpMessaging::Status::ERR_FOLDER_INVALID_NAME:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Folder name is invalid"));
	case OpMessaging::Status::ERR_FOLDER_NOT_EMPTY:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("Folder is not empty"));
	case OpMessaging::Status::ERR_NO_MESSAGE:
		return CallException(DOM_Object::JIL_INVALID_PARAMETER_ERR, return_value, origining_runtime, UNI_L("No such message"));
	default:
		return HandleJILError(error, return_value, origining_runtime);
	};
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILMessaging)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessaging, DOM_JILMessaging::createMessage, "createMessage", "s-", "Messaging.createMessage")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessaging, DOM_JILMessaging::sendMessage, "sendMessage", "o-", "Messaging.sendMessage")
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILMessaging, DOM_JILMessaging::createFolder, "createFolder", "ss-", "Messaging.createFolder", 1)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessaging, DOM_JILMessaging::getCurrentEmailAccount, "getCurrentEmailAccount", "-", "Messaging.getCurrentEmailAccount")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessaging, DOM_JILMessaging::getFolderNames, "getFolderNames", "s-", "Messaging.getFolderNames")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessaging, DOM_JILMessaging::setCurrentEmailAccount, "setCurrentEmailAccount", "s-", "Messaging.setCurrentEmailAccount")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessaging, DOM_JILMessaging::getEmailAccounts, "getEmailAccounts", "-", "Messaging.getEmailAccounts")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessaging, DOM_JILMessaging::deleteEmailAccount, "deleteEmailAccount", "s-", "Messaging.deleteEmailAccount")
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILMessaging, DOM_JILMessaging::deleteFolder, "deleteFolder", "ss-", "Messaging.deleteFolder", 1)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessaging, DOM_JILMessaging::getMessageQuantities, "getMessageQuantities", "ss-", "Messaging.getMessageQuantities")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessaging, DOM_JILMessaging::getMessage, "getMessage", "ssn-", "Messaging.getMessage")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessaging, DOM_JILMessaging::findMessages, "findMessages", "osnn-", "Messaging.findMessages")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessaging, DOM_JILMessaging::deleteAllMessages, "deleteAllMessages", "ss-", "Messaging.deleteAllMessages")
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILMessaging, DOM_JILMessaging::deleteMessage, "deleteMessage", "sss-", "Messaging.deleteMessage")
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILMessaging, DOM_JILMessaging::copyMessageToFolder, "copyMessageToFolder", "os-", "Messaging.copyMessageToFolder", 1)
	DOM_FUNCTIONS_WITH_SECURITY_RULE_A1(DOM_JILMessaging, DOM_JILMessaging::moveMessageToFolder, "moveMessageToFolder", "os-", "Messaging.moveMessageToFolder", 1)
DOM_FUNCTIONS_END(DOM_JILMessaging)

#endif // DOM_JIL_API_SUPPORT
