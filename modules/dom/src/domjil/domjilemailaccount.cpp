/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Jamroszczak tjamroszczak@opera.com
 *
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilemailaccount.h"
#include "modules/dom/src/domjil/utils/jilutils.h"

/* virtual */
int DOM_JILEmailAccount_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	DOM_JILEmailAccount* dom_account;
	CALL_FAILED_IF_ERROR(DOM_JILEmailAccount::Make(dom_account, static_cast<DOM_Runtime*>(origining_runtime)));
	DOMSetObject(return_value, dom_account);
	return ES_VALUE;
}

/* static */ OP_STATUS
DOM_JILEmailAccount::Make(DOM_JILEmailAccount*& new_obj, DOM_Runtime* runtime)
{
	new_obj = OP_NEW(DOM_JILEmailAccount, ());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(new_obj, runtime, runtime->GetPrototype(DOM_Runtime::JIL_EMAILACCOUNT_PROTOTYPE), "Account"));
	new_obj->m_undefnull.id = IS_UNDEF;
	new_obj->m_undefnull.name = IS_UNDEF;
	return OpStatus::OK;
}

OP_STATUS
DOM_JILEmailAccount::SetAccount(const OpMessaging::EmailAccount* account)
{
	RETURN_IF_ERROR(m_id.Set(account->m_id));
	m_undefnull.id = IS_VALUE;
	RETURN_IF_ERROR(m_name.Set(account->m_name));
	m_undefnull.name = IS_VALUE;
	return OpStatus::OK;
}

/* virtual */ ES_PutState
DOM_JILEmailAccount::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_accountId:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.id = IS_NULL; m_id.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.id = IS_UNDEF; m_id.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_id.Set(value->value.string));
					m_undefnull.id = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		}
		case OP_ATOM_accountName:
		{
			switch (value->type)
			{
				case VALUE_NULL:
					m_undefnull.name = IS_NULL; m_name.Empty(); break;
				case VALUE_UNDEFINED:
					m_undefnull.name = IS_UNDEF; m_name.Empty(); break;
				case VALUE_STRING:
					PUT_FAILED_IF_ERROR(m_name.Set(value->value.string));
					m_undefnull.name = IS_VALUE;
					break;
				default:
					return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		}
	}
	return PUT_FAILED;
}

/* virtual */ ES_GetState
DOM_JILEmailAccount::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_accountId:
		{
			if (value)
			{
				switch (m_undefnull.id)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetString(value, m_id.CStr()); break;
				}
			}
			return GET_SUCCESS;
		}
		case OP_ATOM_accountName:
		{
			if (value)
			{
				switch (m_undefnull.name)
				{
					case IS_UNDEF: DOMSetUndefined(value); break;
					case IS_NULL: DOMSetNull(value); break;
					case IS_VALUE: DOMSetString(value, m_name.CStr()); break;
				}
			}
			return GET_SUCCESS;
		}
	}
	return GET_FAILED;
}

#endif // DOM_JIL_API_SUPPORT

