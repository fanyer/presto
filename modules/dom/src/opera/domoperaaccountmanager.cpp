/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_ACCOUNTMANAGER_SUPPORT

#include "modules/dom/src/opera/domoperaaccountmanager.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/opera_auth/opera_auth_module.h"
#include "modules/opera_auth/opera_account_manager.h"

/* static */ void
DOM_OperaAccountManager::MakeL(DOM_OperaAccountManager *&new_obj, DOM_Runtime *origining_runtime)
{
	new_obj = OP_NEW_L(DOM_OperaAccountManager, ());
	LEAVE_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::OPERAACCOUNTMANAGER_PROTOTYPE), "OperaAccountManager"));
	new_obj->ConstructL();
}

DOM_OperaAccountManager::DOM_OperaAccountManager()
	: m_onauthenticationchange(NULL)
{
}

#define PUT_ERROR_CODE_L(x) PutNumericConstantL(object, #x, CoreOperaAccountManager::OPERA_ACCOUNT_##x, runtime)

void
DOM_OperaAccountManager::ConstructL()
{
	ES_Object *object = GetNativeObject();
	DOM_Runtime *runtime = GetRuntime();

	PutNumericConstantL(object, "LOGGED_OUT", 0, runtime);
	PutNumericConstantL(object, "LOGIN_ATTEMPT", 1, runtime);
	PutNumericConstantL(object, "LOGGED_IN", 2, runtime);

	PUT_ERROR_CODE_L(SUCCESS);
	PUT_ERROR_CODE_L(ALREADY_LOGGED_IN);
	PUT_ERROR_CODE_L(INVALID_USERNAME_PASSWORD);
	PUT_ERROR_CODE_L(USER_BANNED);
	PUT_ERROR_CODE_L(USER_ALREADY_EXISTS);
	PUT_ERROR_CODE_L(INVALID_ENCRYPTION_KEY_USED);
	PUT_ERROR_CODE_L(REQUEST_ABORTED);
	PUT_ERROR_CODE_L(REQUEST_ERROR);
	PUT_ERROR_CODE_L(REQUEST_TIMEOUT);
	PUT_ERROR_CODE_L(REQUEST_ALREADY_IN_PROGRESS);
	PUT_ERROR_CODE_L(INTERNAL_SERVER_ERROR);
	PUT_ERROR_CODE_L(INTERNAL_CLIENT_ERROR);
	PUT_ERROR_CODE_L(BAD_REQUEST);
	PUT_ERROR_CODE_L(OUT_OF_MEMORY);
	PUT_ERROR_CODE_L(PARSING_ERROR);
	PUT_ERROR_CODE_L(USERNAME_TOO_SHORT);
	PUT_ERROR_CODE_L(USERNAME_TOO_LONG);
	PUT_ERROR_CODE_L(USERNAME_CONTAINS_INVALID_CHARACTERS);
	PUT_ERROR_CODE_L(PASSWORD_TOO_SHORT);
	PUT_ERROR_CODE_L(PASSWORD_TOO_LONG);
	PUT_ERROR_CODE_L(EMAIL_ADDRESS_INVALID);
	PUT_ERROR_CODE_L(EMAIL_ADDRESS_ALREADY_IN_USE);

	LEAVE_IF_ERROR(g_opera_account_manager->AddListener(this));
}

#undef PUT_ERROR_CODE_L

/* virtual */
DOM_OperaAccountManager::~DOM_OperaAccountManager()
{
	if (g_opera_account_manager) // This may happen after OperaAuthModule::Destroy().
		g_opera_account_manager->RemoveListener(this);
}

/* virtual */ ES_GetState
DOM_OperaAccountManager::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_username:
		DOMSetString(value, g_opera_account_manager->GetUsername().CStr());
		return GET_SUCCESS;
	case OP_ATOM_rememberMe:
		DOMSetBoolean(value, g_opera_account_manager->GetSavePasswordSetting());
		return GET_SUCCESS;
	case OP_ATOM_authState:
		DOMSetNumber(value, g_opera_account_manager->IsLoggedIn() ? 2 : g_opera_account_manager->IsRequestInProgress() ? 1 : 0);
		return GET_SUCCESS;
	case OP_ATOM_authStatus:
		DOMSetNumber(value, (int)g_opera_account_manager->LastError());
		return GET_SUCCESS;
	case OP_ATOM_onauthenticationchange:
		DOMSetObject(value, m_onauthenticationchange);
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_OperaAccountManager::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_username:
	case OP_ATOM_rememberMe:
	case OP_ATOM_authState:
	case OP_ATOM_authStatus:
		return PUT_READ_ONLY;

	case OP_ATOM_onauthenticationchange:
		if (value->type != VALUE_OBJECT)
			return PUT_SUCCESS;
		m_onauthenticationchange = value->value.object;
		return PUT_SUCCESS;
	}

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */ void
DOM_OperaAccountManager::GCTrace()
{
	GCMark(m_onauthenticationchange);
}

/* static */ int
DOM_OperaAccountManager::createAccount(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA_ACCOUNT_MANAGER);
	DOM_CHECK_ARGUMENTS("sss|b");

	OpString username, password, email;
	username.Set(argv[0].value.string);
	password.Set(argv[1].value.string);
	email.Set(argv[2].value.string);

	BOOL rememberMe = FALSE;
	if (argc > 3)
		rememberMe = argv[3].value.boolean;

	g_opera_account_manager->CreateAccount(username, password, email, rememberMe);
	return ES_FAILED; // No return value.
}

/* static */ int
DOM_OperaAccountManager::login(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA_ACCOUNT_MANAGER);
	DOM_CHECK_ARGUMENTS("ss|b");

	OpString username, password;
	username.Set(argv[0].value.string);
	password.Set(argv[1].value.string);

	BOOL rememberMe = FALSE;
	if (argc > 2)
		rememberMe = argv[2].value.boolean;

	g_opera_account_manager->Login(username, password, rememberMe);
	return ES_FAILED; // No return value.
}

/* static */ int
DOM_OperaAccountManager::logout(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA_ACCOUNT_MANAGER);

	g_opera_account_manager->Logout();
	return ES_FAILED; // No return value.
}

/* static */ int
DOM_OperaAccountManager::abort(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA_ACCOUNT_MANAGER);

	g_opera_account_manager->Abort();
	return ES_FAILED; // No return value.
}

DOM_FUNCTIONS_START(DOM_OperaAccountManager)
	DOM_FUNCTIONS_FUNCTION(DOM_OperaAccountManager, DOM_OperaAccountManager::createAccount, "createAccount", "sssb-")
	DOM_FUNCTIONS_FUNCTION(DOM_OperaAccountManager, DOM_OperaAccountManager::login, "login", "ssb-")
	DOM_FUNCTIONS_FUNCTION(DOM_OperaAccountManager, DOM_OperaAccountManager::logout, "logout", "")
	DOM_FUNCTIONS_FUNCTION(DOM_OperaAccountManager, DOM_OperaAccountManager::abort, "abort", "")
DOM_FUNCTIONS_END(DOM_OperaAccountManager)

/* virtual */ void
DOM_OperaAccountManager::OnAccountLoginStateChange()
{
	DOM_Runtime *runtime = GetRuntime();
	ES_AsyncInterface *asyncif = runtime->GetEnvironment()->GetAsyncInterface();

	if (!m_onauthenticationchange)
		return;

	if (op_strcmp(ES_Runtime::GetClass(m_onauthenticationchange), "Function") == 0)
	{
		asyncif->CallFunction(m_onauthenticationchange, NULL, 0, NULL, NULL);
		return;
	}

	// Make a dummy Event argument.
	ES_Value arguments[1];
	DOM_Event *event = OP_NEW(DOM_Event, ());
	RETURN_VOID_IF_ERROR(DOMSetObjectRuntime(event, runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "AuthenticationChangeEvent"));
	event->InitEvent(DOM_EVENT_CUSTOM, NULL);
	RETURN_VOID_IF_ERROR(event->SetType(UNI_L("AuthenticationChangeEvent")));
	DOM_Object::DOMSetObject(&arguments[0], event);

	asyncif->CallMethod(m_onauthenticationchange, UNI_L("handleEvent"), 1, arguments, NULL);
}

#endif // DOM_ACCOUNTMANAGER_SUPPORT
