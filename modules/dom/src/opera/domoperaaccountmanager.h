/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#if !defined(DOM_OPERAACCOUNTMANAGER_H) && defined(DOM_ACCOUNTMANAGER_SUPPORT)
#define DOM_OPERAACCOUNTMANAGER_H

#include "modules/dom/src/domobj.h"
#include "modules/dom/domenvironment.h"
#include "modules/opera_auth/opera_account_manager.h"

/**
 * This class handles MyOpera account creation and authentication against the auth server,
 * according to the specification at
 * https://cgit.oslo.osa/cgi-bin/cgit.cgi/core/extapps-docs.git/plain/unite/specs/operaAccountManager/index.html
 */
class DOM_OperaAccountManager
	: public DOM_Object
	, public CoreOperaAccountManager::Listener
{
public:
	static void MakeL(DOM_OperaAccountManager *&new_obj, DOM_Runtime *origining_runtime);
	virtual ~DOM_OperaAccountManager();

	virtual BOOL IsA(int type) { return type == DOM_TYPE_OPERA_ACCOUNT_MANAGER || DOM_Object::IsA(type); }
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual void GCTrace();

	// Functions
	DOM_DECLARE_FUNCTION(createAccount);
	DOM_DECLARE_FUNCTION(login);
	DOM_DECLARE_FUNCTION(logout);
	DOM_DECLARE_FUNCTION(abort);
	enum {
		FUNCTIONS_createAccount = 1,
		FUNCTIONS_login,
		FUNCTIONS_logout,
		FUNCTIONS_abort,
		FUNCTIONS_ARRAY_SIZE
	};

	// CoreOperaAccountManager::Listener callback
	virtual void OnAccountLoginStateChange();

private:
	DOM_OperaAccountManager();
	void ConstructL();

	ES_Object* m_onauthenticationchange;
};

#endif // !defined(DOM_OPERAACCOUNTMANAGER_H) && defined(DOM_ACCOUNTMANAGER_SUPPORT)
