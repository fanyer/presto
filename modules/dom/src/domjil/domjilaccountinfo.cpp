/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT
#include "modules/dom/src/domjil/domjilaccountinfo.h"

#include "modules/doc/frm_doc.h"
#include "modules/dom/src/domruntime.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/formats/encoder.h"
#include "modules/libcrypto/include/CryptoHash.h"
#include "modules/url/url_enum.h"	// just for an enum required to base64 encode a piece of data
#include "modules/dom/src/domsuspendcallback.h"
#include "modules/dom/src/domcallstate.h"

#include "modules/pi/device_api/OpSubscriberInfo.h"
#include "modules/device_api/jil/JILNetworkResources.h"


struct GetUserAccountBalanceCallbackSuspendingImpl: public DOM_SuspendCallback<JilNetworkResources::GetUserAccountBalanceCallback>
{
	OpString16 m_currency;
	double m_cash;

	GetUserAccountBalanceCallbackSuspendingImpl(): m_cash(0) {}

	virtual void SetCash(const double &cash)
	{
		m_cash = cash;
	}

	virtual OP_STATUS SetCurrency(const uni_char* currency)
	{
		RETURN_IF_ERROR(m_currency.Set(currency));
		return OpStatus::OK;
	}

	virtual void Finished(OP_STATUS error)
	{
		if (OpStatus::IsError(error))
			OnFailed(error);
		else
			OnSuccess();
	}

	virtual OP_STATUS SetErrorCodeAndDescription(int code, const uni_char* description)
	{
#ifdef OPERA_CONSOLE
		if (g_console->IsLogging())
		{
			OpConsoleEngine::Message message(OpConsoleEngine::Gadget, OpConsoleEngine::Information);
			RETURN_IF_ERROR(message.message.AppendFormat(UNI_L("JIL Network Protocol: GetSelfLocation returned error: %d (%s)"), code, description));
			TRAP_AND_RETURN(res, g_console->PostMessageL(&message));
		}
		return OpStatus::OK;
#endif // OPERA_CONSOLE
	}
};

struct GetUserSubscriptionTypeCallbackSuspendingImpl: public DOM_SuspendCallback<JilNetworkResources::GetUserSubscriptionTypeCallback>
{
	OpString16 m_type;

	virtual OP_STATUS SetType(const uni_char* type)
	{
		return m_type.Set(type);
	}

	virtual void Finished(OP_STATUS error)
	{
		if (OpStatus::IsError(error))
			OnFailed(error);
		else
			OnSuccess();
	}

	virtual OP_STATUS SetErrorCodeAndDescription(int code, const uni_char* description)
	{
#ifdef OPERA_CONSOLE
		if (g_console->IsLogging())
		{
			OpConsoleEngine::Message message(OpConsoleEngine::Gadget, OpConsoleEngine::Information);
			RETURN_IF_ERROR(message.message.AppendFormat(UNI_L("JIL Network Protocol: GetSelfLocation returned error: %d (%s)"), code, description));
			TRAP_AND_RETURN(res, g_console->PostMessageL(&message));
		}
		return OpStatus::OK;
#endif // OPERA_CONSOLE
	}
};

/* static */
OP_STATUS DOM_JILAccountInfo::Make(DOM_JILAccountInfo*& new_account, DOM_Runtime* runtime)
{
	OP_ASSERT(runtime);
	new_account = OP_NEW(DOM_JILAccountInfo, ());
	return DOMSetObjectRuntime(new_account, runtime, runtime->GetPrototype(DOM_Runtime::JIL_ACCOUNTINFO_PROTOTYPE), "AccountInfo");
}

OP_STATUS DOM_JILAccountInfo::ComputeUniqueUserId(OpString& id)
{
	OpString8 msisdn;
	OpString8 imsi;
	RETURN_IF_ERROR(g_op_subscriber_info->GetSubscriberMSISDN(&msisdn));
	RETURN_IF_ERROR(g_op_subscriber_info->GetSubscriberIMSI(&imsi));

	OpAutoPtr<CryptoHash> ap_hash(CryptoHash::CreateSHA256());
	RETURN_OOM_IF_NULL(ap_hash.get());
	RETURN_IF_ERROR(ap_hash->InitHash());
	ap_hash->CalculateHash(imsi.CStr());
	ap_hash->CalculateHash(msisdn.CStr());

	// As we know the hash is SHA256 we know that the size of the hash is 32 bytes
	// - no need to do dynamic allocations
	UINT8 result[32];
	OP_ASSERT(ARRAY_SIZE(result) == ap_hash->Size());
	ap_hash->ExtractHash(result);

	char* encoded = NULL;
	int encoded_length;
	MIME_Encode_Error encode_status =
		MIME_Encode_SetStr(encoded, encoded_length, reinterpret_cast<const char*>(result), ARRAY_SIZE(result), NULL, GEN_BASE64_ONELINE);

	if (encode_status != MIME_NO_ERROR)
		return OpStatus::ERR;

	OP_STATUS status = id.Set(encoded, encoded_length);
	OP_DELETEA(encoded);
	return status;
}

/* virtual */
ES_GetState DOM_JILAccountInfo::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	DOM_CHECK_OR_RESTORE_PERFORMED; // hack so that 
	switch (property_atom)
	{
		case OP_ATOM_phoneUserUniqueId:
		{
			if (value)
			{
				OpString subscriber_id;
				GET_FAILED_IF_ERROR(ComputeUniqueUserId(subscriber_id));
				TempBuffer* buffer = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(buffer->Append(subscriber_id.CStr()));
				DOMSetString(value, buffer);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_phoneOperatorName:
		{
			if (value)
			{
				OpString operator_name;
				GET_FAILED_IF_ERROR(g_op_subscriber_info->GetOperatorName(&operator_name));
				TempBuffer* buffer = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(buffer->Append(operator_name.CStr()));
				DOMSetString(value, buffer);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_phoneMSISDN:
		{
			if (value)
			{
				OpString8 msisdn;
				GET_FAILED_IF_ERROR(g_op_subscriber_info->GetSubscriberMSISDN(&msisdn));
				TempBuffer* buffer = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(buffer->Append(msisdn.CStr()));
				DOMSetString(value, buffer);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_userAccountBalance:
		{
			if (value)
			{
				DOM_SuspendingCall call(this, NULL, 0, value, restart_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
				NEW_SUSPENDING_CALLBACK(GetUserAccountBalanceCallbackSuspendingImpl, callback, restart_value, call, ());
				OpMemberFunctionObject1<JilNetworkResources, JilNetworkResources::GetUserAccountBalanceCallback*>
					function(g_DAPI_network_resources, &JilNetworkResources::GetUserAccountBalance, callback);

				DOM_SUSPENDING_CALL(call, function, GetUserAccountBalanceCallbackSuspendingImpl, callback);
				OP_ASSERT(callback);
				if (callback->HasFailed())
					DOMSetUndefined(value);
				else
					DOMSetNumber(value, callback->m_cash);
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_userSubscriptionType:
		{
			if (value)
			{
				DOM_SuspendingCall call(this, NULL, 0, value, restart_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
				NEW_SUSPENDING_CALLBACK(GetUserSubscriptionTypeCallbackSuspendingImpl, callback, restart_value, call, ());
				OpMemberFunctionObject1<JilNetworkResources, JilNetworkResources::GetUserSubscriptionTypeCallback*>
					function(g_DAPI_network_resources, &JilNetworkResources::GetUserSubscriptionType, callback);

				DOM_SUSPENDING_CALL(call, function, GetUserSubscriptionTypeCallbackSuspendingImpl, callback);
				OP_ASSERT(callback);
				if (callback->HasFailed())
					DOMSetUndefined(value);
				else
				{
					TempBuffer* buffer = GetEmptyTempBuf();
					GET_FAILED_IF_ERROR(buffer->Append(callback->m_type));
					DOMSetString(value, buffer);
				}
			}
			return GET_SUCCESS;
		}
	}
	return GET_FAILED;
}

/* virtual */
ES_PutState DOM_JILAccountInfo::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_phoneUserUniqueId:
		case OP_ATOM_phoneOperatorName:
		case OP_ATOM_phoneMSISDN:
		case OP_ATOM_userAccountBalance:
		case OP_ATOM_userSubscriptionType:
			return PUT_SUCCESS;
	}
	return PUT_FAILED;
}

/* static */
int DOM_JILAccountInfo::HandleAccountError(OP_STATUS error_code, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	switch (error_code)
	{
	case OpStatus::ERR_NO_SUCH_RESOURCE:
		return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("No SIM card"));
	case OpStatus::ERR_NOT_SUPPORTED:
		return CallException(DOM_Object::JIL_UNSUPPORTED_ERR, return_value, origining_runtime);
	default:
		return HandleJILError(error_code, return_value, origining_runtime);
	}
}

#endif // DOM_JIL_API_SUPPORT
